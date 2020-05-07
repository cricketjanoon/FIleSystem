#ifndef __MYFS_H
#define __MYFS_H

#include <stdlib.h>
#include <string.h>

enum mode { READ, WRITE, APPEND, READ_WRITE }; 

typedef struct inode{
	int isDir;
    int size;
	// int num_of_blocks;
	int data_blocks[4];
	char *sing_block;
	char *double_block;
} inode;

//for simplicity assuming that a dir can store maximum of 10 files
// int size_of_dir = 10;
typedef struct dir{
	char filenames[10][10];
	int fileinodes[10];
	int dir_bitmap[10]; //hack to check which of the entry in dir is free
	// int filesizes[10];
	int num_of_files;
} dir;

typedef struct fd_entry{
	int fd;
	int inode_num;
	int read_offset;
	int write_offset;
	enum mode mode;
	void *next;
}fd_entry;


int my_open(const char *pathname, int mode);
int my_close(int fd);
int my_read(int fd, void *buffer, int count);
int my_write(int fd, void *buffer, int count);
int my_mkdir(const char *pathname);
int my_format(int blocksize);
int my_unlink(const char *pathname);
int my_rmdir(const char *pathname);

void mount_filesystem();
void unmount_disk();

int get_dir_entry(dir* _dir);
int get_inode_num();
int write_inode(inode *ptr, int inumber);
int write_data_block(int blockNum, void *block);
int get_free_datablock(int inumber);
int read_data_block(int blockNum,void *block);
char* read_inode(int inum);
inode* create_inode(int isDir, int size);

void rm_dir_recursively(dir* dir_to_remove);

void print_inode_bitmap();
void print_data_bitmap();
void print_all_files();
void print_dir(dir* root_dir, int* space);
/* file descriptor */
// fd_entry *fd_list_head=NULL, *fd_list_tail=NULL;
// int cur_fd_num = 0;

fd_entry *find_fd_entry(int fd_or_inode_num);
int remove_fd_entry(int fd);
fd_entry *create_fd_entry(int inode_num, int read_offset, int write_offset, enum mode mode);
void print_dir(dir* root_dir, int* space);
void print_fd_table();

/* Super Block Data */
// char *MAGIC_STRING = "882244";
#define BLOCK_SIZE 4096
#define NUM_DATA_BLOCKS 237
#define TOTAL_NUM_INODES 1024
#define INODE_START_BLOCK 3
#define DATA_START_BLOCK 19
#define INODE_START_ADDRESS BLOCK_SIZE*INODE_START_BLOCK //inodes start from 3rd block
#define DATA_START_ADDRESS BLOCK_SIZE*DATA_START_BLOCK //inodes start from 3rd block
#define ROOT_INODE_NUM 0
#define INODE_BITMAP_BLOCK 1
#define DATA_BITMAP_BLOCK 2

typedef struct super_block{
	int block_size;
	int num_data_blocks;
	int total_num_inodes;

	int data_start_block;
	int inode_start_block;

	int inode_start_address;
	int data_start_addresss;

	int root_inode_num;
	int inode_bitmap_block;
	int data_bitmap_block;

}super_block;

char INODE_BITMAP[4096];			
char DATA_BITMAP[4096];
#endif