#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "helper.h"

#define multiple(x) ((x - 1) / 4 + 1) * 4

unsigned char *disk;
struct ext2_group_desc *gd;

int main(int argc, char **argv) {

  if (argc != 3) {
    fprintf(stderr, "Usage: readimg <image file name>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR);

  char* query = argv[2];

  char new_query[strlen(query) +2];
  if (query[0] != '/') {
    fprintf(stderr, "please use abs path!\n");
    exit(1);
  }

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  gd = (struct ext2_group_desc *)(disk + 1024 + 1024);
  void* inodes = disk + 1024 * gd->bg_inode_table;
  void* inode_bm_loc = disk + 1024 * gd->bg_inode_bitmap;
  void* block_bm_loc = disk + 1024 * gd->bg_block_bitmap;
  int* inode_bm = get_bitmap(inode_bm_loc,INODES_COUNT);
  int* block_bm = get_bitmap(block_bm_loc,BLOCKS_COUNT);
  
  if (find_dir(query, inodes) != -1) {
    exit(EEXIST);
  }    

  // separate query
  int i;
  char new_dir[512];
  int len = strlen(query);
  i = len - 1;
  while (query[i] != '/') {
    i--;
  }
  int name_len = len - i - 1;
  strncpy(new_dir, query + len - name_len, name_len + 1);
  if (i == 0) { 
    query[i + 1] = '\0';
  } else {
    query[i] = '\0';
  }


  name_len = strlen(new_dir);

  int p_inode_idx = find_dir(query, inodes);
  if (p_inode_idx < 0) {

    errno = ENOENT;
    perror(query);
    exit(errno);
  }

  struct ext2_inode* p_inode = inodes + sizeof(struct ext2_inode) * p_inode_idx;
  int dir_num_blocks = p_inode->i_size / 1024;
  int block_num;
  struct ext2_dir_entry_2* dir;
  i = 0;
  while (i < dir_num_blocks) {
    block_num = p_inode->i_block[i];
    dir = (void*)disk + 1024 * block_num;
    int curr_size = 0;
    while (curr_size < 1024) {
      curr_size += dir->rec_len;
      if (dir->file_type == EXT2_FT_REG_FILE &&
          name_len == dir->name_len &&
          strncmp(new_dir, dir->name, name_len) == 0) {

        errno = EEXIST;
        perror(new_dir);
        exit(errno);
      }
      dir = (void*)dir + dir->rec_len;
    }
    i++;
  }

  int inode_idx = -1;
  for (i = 0; i < INODES_COUNT; i++) {
    if (inode_bm[i] == 0) {
      inode_idx = i;
      break;
    }
  }
  int* free_blocks = find_free_blocks(block_bm, 1);
  if (free_blocks == NULL) {
    errno = ENOSPC;
    perror("There not enough free block");
    exit(errno);
  }
  int block_idx = *free_blocks;

  // default dir entry for new dir
  struct ext2_dir_entry_2* new_dirctory = (void*)disk + 1024 * (block_idx + 1);
  // .
  new_dirctory->inode = inode_idx + 1;
  new_dirctory->rec_len = 12;
  new_dirctory->name[0] = '.';
  new_dirctory = (void*)new_dirctory + 12;
  new_dirctory->name_len = 1;
  new_dirctory->file_type = EXT2_FT_DIR;
  new_dirctory->name[0] = '.';
  new_dirctory->name[1] = '.';
  new_dirctory->inode = p_inode_idx + 1;
  new_dirctory->rec_len = 1012;
  new_dirctory->name_len = 2;
  new_dirctory->file_type = EXT2_FT_DIR;

  set_bitmap(block_bm_loc, block_idx, 1);


  struct ext2_inode* new_inode = inodes + sizeof(struct ext2_inode) * inode_idx;
  new_inode->i_mode = EXT2_S_IFDIR;
  new_inode->i_blocks = 2;
  new_inode->i_block[0] = block_idx + 1;
  new_inode->i_size = 1024;
  new_inode->i_links_count = 2;

  set_bitmap(inode_bm_loc, inode_idx, 1);


  p_inode->i_links_count++;


  bool found = false;
  i = 0;
  while (i < dir_num_blocks){
    block_num = p_inode->i_block[i];
    dir = (void*)disk + 1024 * block_num;
    int curr_size = 0;
    while (curr_size < 1024) {
      curr_size += dir->rec_len;
      if (curr_size == 1024 && dir->rec_len >= multiple(name_len) + 8 + 8 + multiple(dir->name_len)) {
        found = true;
        break;
      }
      dir = (void*)dir + dir->rec_len;
    }
    if (found) {
      break;
    }
    i++;
  }
  gd->bg_free_blocks_count = total_free(block_bm_loc,BLOCKS_COUNT);
  gd->bg_free_inodes_count = total_free(inode_bm_loc,INODES_COUNT);
  gd->bg_used_dirs_count++;

  if (found == true) {
    int prev = dir->rec_len;
    int adj = multiple(dir->name_len) + 8;
    dir->rec_len = adj;
    dir = (void*)dir + adj;
    dir->inode = inode_idx + 1;
    dir->rec_len = prev - adj;
    dir->name_len = name_len;
    dir->file_type = EXT2_FT_DIR;
    strncpy((void*)dir + 8, new_dir, name_len);
  } else {
    block_bm = get_bitmap(block_bm_loc,BLOCKS_COUNT);
    int* free = find_free_blocks(block_bm, 1);
    int new_block = *free;
    dir = (void*)disk + 1024 * (new_block + 1);
    dir->inode = inode_idx + 1;
    dir->rec_len = 1024;
    dir->name_len = name_len;
    dir->file_type = EXT2_FT_DIR;
    strncpy((void*)dir + 8, new_dir, name_len);

    set_bitmap(block_bm_loc, new_block, 1);

    p_inode->i_size += 1024;
    p_inode->i_blocks += 2;
    p_inode->i_block[dir_num_blocks] = new_block + 1;
  }
  
  return 0;
}
