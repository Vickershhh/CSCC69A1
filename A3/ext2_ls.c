#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include "helper.h"

int inode = 2;
unsigned char *disk;
struct ext2_group_desc *gd;
struct ext2_dir_entry_2 *dir;
struct ext2_inode * inode_table;


char** split_str(char* query, char symbol) {
    int count = 0;
    char* tmp = query;
    char* comma = 0;
    while (*tmp) {
        if (symbol == *tmp){
            count++;
            comma = tmp;
        }
        tmp++;
    }
    /* Add space for trailing token. */
    count += (comma < (query + strlen(query) - 1) + 1);

    char** result = malloc(sizeof(char*) * count);
    if (result) {
        char delim[2];
        delim[0] = symbol;
        delim[1] = 0;
        int idx  = 0;
        char* tok = strtok(query, delim);
        while (tok) {
            assert(idx < count);
            *(result + idx++) = strdup(tok);
            tok = strtok(0, delim);
        }
        *(result + idx) = 0;
    }
    return result;
}


//search the input url, to check whether it exists or not
int search(char* path){
	int count = 0;
	//char **paths = str_split(path,'/');
	
	while(count < 1024){
        char type;
        if (dir->file_type == 2){
            type ='d';
        }else if(dir->file_type == 1){
            type = 'f';
        }
		if(strcmp(dir->name,path)== 0 && type == 'd'){	
			//printf("yangShu%d%s%s\n",strcmp(dir->name,path),path,dir->name);
			return dir->inode;	
        } 
		count += dir->rec_len;
		dir = (struct ext2_dir_entry_2 *)(disk + ((&inode_table[inode-1])->i_block[0] * 1024+count));
    }
    return 0;
}
//search the input url, to check whether it exists or not
int search_2(char* query){
	int len = 1;
	for(int i = 0;i < strlen(query);i++){
		if (query[i] == '/'){
			len += 1;
		}
    }
    if (query[strlen(query)-1] == '/'){
		len -= 1;
	}
	char **paths = split_str(query,'/');;
    int index = 0;
	while (index < len){
		char* path = paths[index];
		int result = search(path);
		if (result!=0){
			inode = result;
			dir = (struct ext2_dir_entry_2 *)(disk + ((&inode_table[inode-1])->i_block[0] * 1024));
		}else{
			return 0;
		}
		index += 1;
	}
	return 1;
}

int main(int argc, char **argv) {
	
		if(argc != 3) {
			fprintf(stderr, "Usage: readimg <image file name>\n");
			exit(1);
		}
		char* query = argv[2];

		char new_query[strlen(query) +2];

		if (query[0] == '/'){
			new_query[0] = '.';
			new_query[1] = '.';
			strcat(new_query,query);
			strcpy(query,new_query);
		}

		int fd = open(argv[1], O_RDWR);
		disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if(disk == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
		
		gd = (struct ext2_group_desc *)(disk + 1024 + 1024);
		inode_table =(struct ext2_inode  *) (gd->bg_inode_table * 1024 +disk);
		dir = (struct ext2_dir_entry_2 *)(disk + (&inode_table[1])->i_block[0] * 1024);
		
		if(search_2(query) == 0){
            perror("No such file or diretory");
            exit(1);		
				
		}else{
			int count = 0;
            while (count<1024){
                for(int len = 0; len < dir->name_len; len++){
                    printf("%c", dir->name[len]);
                }
                printf("\n");
                count += dir->rec_len;
                dir = (struct ext2_dir_entry_2 *)(disk + ((&inode_table[inode-1])->i_block[0] * 1024) + count);
            }
		} 
    return 0;
}
