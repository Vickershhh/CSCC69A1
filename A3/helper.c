#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "helper.h"

extern unsigned char* disk;


int find_next(char* query, char* buf) {
  if (query[0] != '/') {
    return -1;
  }
  int i, j;
  for (i = 1, j = 0; query[i] != '/'; i++, j++) {
    if (query[i] == '\0') {
      break;
    }
    buf[j] = query[i];
  }
  buf[j] = '\0';
  return i;
}

int find_dir(char* query, void* inodes) {

  int inode_num = EXT2_ROOT_INO; 
  struct ext2_inode* inode;
  struct ext2_dir_entry_2* dir;
  char buf[512] = "undefined"; 

  while (1) {

    inode = (struct ext2_inode*) (inodes + (inode_num - 1) * sizeof(struct ext2_inode));
    int total_size = inode->i_size;

    if (strcmp(query, "/") == 0) {
      return inode_num - 1;
    }

    int n = find_next(query, buf);
    query += n;

    int curr = 0;
    int new_inode_num = -1;
    int block_num_idx = 0;
    int block_num;
    block_num = inode->i_block[block_num_idx];
    dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
    while (curr < total_size) {
      curr += dir->rec_len;
      if (dir->file_type == EXT2_FT_DIR) {
        if (strlen(buf) == dir->name_len &&
            strncmp(buf, dir->name, dir->name_len) == 0) {
          new_inode_num = dir->inode;
          break;
        }
      }
      dir = (void*)dir + dir->rec_len;
      if (curr % EXT2_BLOCK_SIZE == 0) { 
        block_num_idx++;
        block_num = inode->i_block[block_num_idx];
        dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
      }
    }

    if (new_inode_num == -1) {
      return -1;
    } else {
      inode_num = new_inode_num;
    }

  }
}

int* get_bitmap(void* start,int COUNT) {
  int* block_bitmap = malloc(sizeof(int) * COUNT);
  int i, byte_int, bit;
  for (i = 0; i < COUNT; i++) {
    byte_int = i / 8;
    bit = i % 8;
    char* byte = start + byte_int;
    block_bitmap[i] = (*byte >> bit) & 1;
  }
  return block_bitmap;
}
void set_bitmap(void* start, int inode_idx, int num) {
  int byte_int = inode_idx / 8;
  int bit = inode_idx % 8;
  char* byte = start + byte_int;
  *byte = (*byte & ~(1 << bit)) | (num << bit);
}

int* find_free_blocks(int* block_bm, int num) {

  int i = 0, j = 0;
  int* blocks = malloc(sizeof(int) * num);
  while (i < num) {
    while (block_bm[j] == 1) {
      j++;
      if (j == BLOCKS_COUNT) {
        return NULL;
      }
    }
    blocks[i] = j;
    i++;
    j++;
  }
  return blocks;
}

int total_free(void* start, int Count) {
  int i, byte_off, bit_off;
  char* byte;
  int count = 0;
  for (i = 0; i < Count; i++) {
    byte_off = i / 8;
    bit_off = i % 8;
    byte = start + byte_off;
    count += (*byte >> bit_off) & 1;
  }
  return Count - count;
}
void set_bitmap_to_0(unsigned int * bitmap, int index) {
  bitmap += (index / 32);
  unsigned int f = ~(1 << (index%32));
  *bitmap &= f;
}

int find_inode(unsigned char *x, char *query){
  int result = -1;
  if (strlen(query) == 1 && query[0] == '/')
    return 2;
  if (query[0] != '/'){
    return -1;
  }

  struct ext2_group_desc *gd = (struct ext2_group_desc *)(x + 2048);
  int inode_id = gd->bg_inode_table;
  char *inode_addr = x + (1024 * inode_id);
  struct ext2_inode *cur_inode = (struct ext2_inode *)(inode_addr + 128);
  char *folder = malloc(256);
  int cursor = 1;

  while (cursor < strlen(query)){ 

    int k = 0;

    while (query[cursor] != '/' && cursor < strlen(query)) {
      folder[k++] = query[cursor++];
    }
    folder[k] = 0;
    cursor++;

    int found = 0;
    for (int i=0;i<12 && cur_inode->i_block[i] && !found;i++){ // look at all blocks cur_inode has

      int block_no = cur_inode->i_block[i];
      struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)( x + (EXT2_BLOCK_SIZE * block_no) );

        int j = 0;
        while (j< 1024) { // go through "dir_entry"s in this block
            j += dir->rec_len;

            char *copy = malloc(255); // copy of directory name
            strncpy(copy, dir->name, dir->name_len);
            copy[dir->name_len] = 0;

            if (strlen(copy) == strlen(folder)){
              if (strncmp(copy, folder, strlen(copy)) == 0){
                if (dir->file_type == 1){
                    if (cursor >= strlen(query)){
                        return dir->inode;
                    }
                    return -1;
                }else if (dir->file_type == 2){

                  cur_inode =(struct ext2_inode *)(inode_addr + (128 * (dir->inode -1)));
                  result = dir->inode;
                  found = 1;
                  break;
                }
              }
            }

            // iterate to the next directory
            char *ptr = (char *)dir;
            ptr += dir->rec_len;
            dir = (struct ext2_dir_entry_2 *) ptr;
        }

    }
    if (!found) {
      return -1;
    }
  }
  return result;
}