#include "ext2.h"

#define INODES_COUNT 32
#define BLOCKS_COUNT 128



int find_next(char* query, char* buf);
int find_dir(char* query, void* inodes);
int* find_free_blocks(int* block_bitmap, int num);
int total_free(void* start, int Count);
void set_bitmap(void* start, int inode_idx, int bitval);
int* get_bitmap(void* start,int Count);


void set_bitmap_to_0(unsigned int * bitmap, int index);
int find_inode(unsigned char *disk, char *path);