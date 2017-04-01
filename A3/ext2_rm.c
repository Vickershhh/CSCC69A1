#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "helper.h"

unsigned char *disk;

int main(int argc, char **argv) {

    if (argc != 3) {
      fprintf(stderr, "Usage: readimg <image file name>\n");
      exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    char* query = argv[2];

    int number = find_inode(disk, query); 
    int i;
    if (number == -1) {
      printf("ENOEN\n");
      return 2;
    }
    char *loc = malloc(256);
    char *dir_name = malloc(256);
    int len = strlen(query)-1;
    while (query[len] != '/'){
      len--;
    }
    int x=0;
    for (i=len+1;i<strlen(query) && query[i] != '/';i++)
        dir_name[x++] = query[i];
    dir_name[x] = 0;
    strncpy(loc, query, len);
    loc[len] = 0;
    if (loc[0] == 0) {
        loc[0] = '/',
        loc[1] = 0;
    }
    int inode_parent = find_inode(disk, loc);
    if (inode_parent == -1) {
        printf("ENOENT\n");
        return 2;
    }
    struct ext2_group_desc * gd = (struct ext2_group_desc *)(disk + 2 * 1024);
    unsigned int inode_bitmap = gd->bg_inode_bitmap;
    unsigned int block_bitmap = gd->bg_block_bitmap;
    char * start = (char *)(disk + 1024 * gd->bg_inode_table);
    struct ext2_inode *inode;
    inode = (struct ext2_inode *)(start + (sizeof (struct ext2_inode) * (inode_parent - 1)));
    struct ext2_inode * del = (struct ext2_inode *)(start + (sizeof (struct ext2_inode) * (number - 1)));
    unsigned int * curr_block_pt = inode->i_block;
    unsigned int curr_block;
    int count = 0;
    int j = 0;
    while (j < 12 && inode->i_block[j]) {
      curr_block = inode->i_block[j];
      struct ext2_dir_entry_2 * de = (struct ext2_dir_entry_2 *)(disk + 1024 * curr_block);
      struct ext2_dir_entry_2 * prev;
      int p = 0;
      while (p < 1024) {
        p += de->rec_len;
        if (de->inode == number) {
          if ((unsigned int) de->file_type == 1) {
            del->i_links_count--;
            if (del->i_links_count != 0) {
              set_bitmap_to_0((unsigned int *) (disk + 1024 * inode_bitmap), number -1);
              set_bitmap_to_0((unsigned int *) (disk + 1024 * block_bitmap), del->i_block[j] - 1);
              x = 0;
              while (x < 12 && del->i_block[x]) {
                del->i_block[x] = 0;
                count++;
                x++;
              }
              del->i_blocks = 0;
              gd->bg_free_blocks_count += count;
              gd->bg_free_inodes_count++;
              del->i_size = 0;
            }
            prev->rec_len = de->rec_len + prev->rec_len;
            return 0;
          } else {
            printf("EISDIR\n");
            return 2;
          }
        }
        prev = de;
        de = (void *) de + de->rec_len;
      }
      curr_block_pt++;
      j++;
    }
    return 0;
}
