#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myfs.h"


typedef struct inode_t{
	int isDir;
    int size;
	int num_of_blocks;
	char *data_blocks[4];
	char *sing_block;
	char *double_block;
} inode_t;

typedef struct dir{
	char *filenames[10];
	int filesizes[10];
	int fileinodes[10];
	int num_of_files;
} dir;