#ifndef __MYFS_H
#define __MYFS_H

#include <stdlib.h>
#include <string.h>

typedef struct inode{
	int isDir;
    int size;
	int num_of_blocks;
	int data_blocks[4];
	char *sing_block;
	char *double_block;
} inode;

typedef struct dir{
	char *filenames[10];
	int filesizes[10];
	int fileinodes[10];
	int num_of_files;
} dir;


typedef struct fd_entry{
	int fd;
	int inode_num;
	int offset;
}fd_entry;

fd_entry fd_table[10];
int cur_fd_entry=0;



int my_open(const char *pathname, int mode);

int my_close(int fd);

int my_read(int fd, void *buffer, int count);

int my_write(int fd, void *buffer, int count);

int my_mkdir(const char *pathname);

int my_format(int blocksize);

int my_unlink(const char *pathname);

int my_rmdir(const char *pathname);




int write_inode(inode *ptr, int inumber);
int write_data_block(int blockNum, void *block);
int get_free_datablock(int inumber);
int read_data_block(int blockNum,void *block);
void print_inode_bitmap();
void print_data_bitmap();
void MountFS();
int get_inode_num();
char* read_inode(int inum);
inode* create_inode(int isDir, int size);


const int block_size = 4096; // 4KB block size
const int num_data_blocks = 237;
const int total_num_inodes = 1024;

const int data_start_block = 19; //following zero-indexing
const int inode_start_block = 3; //following zero-indexing

const int inode_start_address = 4096*3; //inodes start from 3rd block
const int data_start_addresss = 4096*16; //data blocks start at 16th block

const int inode_bitmap_block = 1;
const int data_bitmap_block = 2;

int root_inode_num = 0;

FILE *disk;

char INODE_BITMAP[4096];			
char DATA_BITMAP[4096];


void print_root_dir()
{
	printf("************ Root Dir ***************\n");
	inode *root_inode = (inode *)read_inode(root_inode_num);


	// printf("Root data block: %d\n", root_inode->data_blocks[0]);

	char block[block_size];
	read_data_block(data_start_block + root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	// printf("Root data block address: %p\n", root_dir);
	// printf("Root dir: num of iles: %d\n", root_dir->num_of_files);
	// printf("Name: %s\n", root_dir->filenames[0]);
	// printf("size: %d\n", root_dir->filesizes[0]);
	// printf("inode: %d\n", root_dir->fileinodes[0]);
	for(int i=0; i<root_dir->num_of_files; i++)
	{
		printf("Filename: %s, Filsizes: %d, inode: %d\n", root_dir->filenames[i], root_dir->filesizes[i], root_dir->fileinodes[i]);
	}

	printf("*************************************\n");
}


int my_open(const char *pathname, int mode)
{
	//read root inode and data 
	inode *root_inode = (inode *)read_inode(root_inode_num);
	char block[block_size];
	read_data_block(data_start_block + root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	printf("root data block address: %p\n", root_dir);

	//search to see if file exists
	short is_new_file=1; //bool to check if file exist or not
	char **filenames = root_dir->filenames;
	for(int j=0; j<root_dir->num_of_files; j++)
	{
		if(strcmp(filenames[j], pathname))
		{
			is_new_file = 0;
			printf("new_file\n");
			break;
		}
	}

	printf("checkpoint 1\n");

	read_data_block(inode_bitmap_block, INODE_BITMAP);
	read_data_block(data_bitmap_block, DATA_BITMAP);

	if(is_new_file)
	{
		int new_inode_num = get_inode_num(); //TODO: error check
		inode* new_inode = create_inode(0, block_size); //TODO: change it to the size required

		printf("new file inode num: %d\n", new_inode_num);


		int block_num = get_free_datablock(root_inode_num);
		printf("Free data block for new file: %d\n", block_num);

		if(block_num == -1)
		{
			printf("No disk space. Unable to mount the disk.");
		}
		new_inode->data_blocks[0] = block_num;

		write_inode(new_inode, new_inode_num);

		//update root dir and inode
		root_dir->filenames[root_dir->num_of_files] = pathname;
		root_dir->fileinodes[root_dir->num_of_files] = new_inode_num;
		root_dir->filesizes[root_dir->num_of_files] = new_inode->size;
		root_dir->num_of_files++;
		root_inode->size += new_inode->size;


		write_data_block(data_start_block+root_inode->data_blocks[0], (char *) root_dir);

		write_inode(root_inode, root_inode_num);
		write_data_block(inode_bitmap_block, INODE_BITMAP);
		write_data_block(data_bitmap_block, DATA_BITMAP);
		
		//add file to the fd table
		int fd = 11; //TODO: Need to generate random fd
		fd_entry new_fd = {fd, new_inode_num, 0}; 
		fd_table[cur_fd_entry] = new_fd;
		cur_fd_entry++;

		return fd;
	}
	else
	{
		printf("file alread present");
		return -1;
		//TODO: When file already exists
	}
}

int my_write(int fd, void *buffer, int count)
{
	fd_entry file;
	//find find in the file direction
	//TODO: handle case if file is not found
	for(int i=0; i<cur_fd_entry; i++)
	{
		if(fd==fd_table[i].fd)
		{
			file = fd_table[i];
			break;
		}
	}

	printf("fd: %d\n", file.fd);
	printf("inode: %d\n", file.inode_num);
	printf("offset: %d\n", file.offset);

	printf("checkpoin0\n");

	inode *cur_file_inode = (inode *)read_inode(file.inode_num);

	printf("checkpoin1\n");

	printf("cur_file_inode->data_blocks[0]: %d\n", cur_file_inode->data_blocks[0]);
	int offset = data_start_addresss + cur_file_inode->data_blocks[0]*block_size + file.offset;
	printf("offset: %d\n", offset);
	printf("inode: %d\n", cur_file_inode->data_blocks[0]);


	offset = fseek(disk, offset, SEEK_SET);
	if(offset != 0) 
		return 1;

	printf("before write\n");

	fwrite(buffer, count, 1, disk);
	return 0; //TODO: handle error conditions

}

int my_read(int fd, void *buffer, int count)
{
	fd_entry file;
	//find find in the file direction
	//TODO: handle case if file is not found
	for(int i=0; i<cur_fd_entry; i++)
	{
		if(fd==fd_table[i].fd)
		{
			file = fd_table[i];
			break;
		}
	}

	inode *cur_file_inode = (inode *)read_inode(file.inode_num);

	//setting the dsik pointer to from where to read
	int offset = data_start_addresss + cur_file_inode->data_blocks[0]*block_size + file.offset;
	offset = fseek(disk, offset, SEEK_SET);

	fread(buffer, 1, count, disk);
	return 0; //TODO: check error conditions
}


void open_disk_file()
{
	//TODO: Add check for error checking
	//open the disk file for both reading and writting
	disk = fopen("disk.bin","r+"); 
}

void MountFS()
{
    //set data and i-note bitmap to zero
	for (int i = 0; i < num_data_blocks; ++i)
	{
		DATA_BITMAP[i]='0';
	}
	for (int i = 0; i < total_num_inodes; ++i)
	{
		INODE_BITMAP[i]='0';
	}

    //set inode bitmap equal to 1
	INODE_BITMAP[root_inode_num] = '1';

	//creating a directory struct
	dir *root_dir = (dir *)malloc(sizeof(dir));
	root_dir->num_of_files = 0;

	//creating an inode struct
	inode *root = (inode*)malloc(sizeof(inode));
	int size = sizeof(root_dir);
	root->size = size;
	root->num_of_blocks = size/block_size + ((size%block_size)!=0);
	root->isDir = 1;
 
	int block_num = get_free_datablock(root_inode_num);

	printf("Free data block for root inode: %d\n", block_num);

	if(block_num == -1)
	{
		printf("No disk space. Unable to mount the disk.");
	}
	root->data_blocks[0] = block_num;

	write_data_block(data_start_block + root->data_blocks[0], (char*)root_dir);
	write_inode(root, root_inode_num);

	write_data_block(inode_bitmap_block, INODE_BITMAP);
	write_data_block(data_bitmap_block, DATA_BITMAP);

}

int get_free_datablock(int inumber)
{
	int i=0;
	//check if any of the datablock is free and return its number
	while(i < num_data_blocks){
		if(DATA_BITMAP[i]=='0')
		{
			DATA_BITMAP[i]='1';
			return i;
		}
		i++;
	}
	//incase of not finding free-block return -1
	INODE_BITMAP[inumber] = '0';
	return -1;
}

int get_inode_num()
{
	//printf("give_inum called\n");
	//readData(inode_bitmap_block,INODE_BITMAP);
	for (int i = 3; i < total_num_inodes; ++i){
		if(INODE_BITMAP[i]=='0'){
			INODE_BITMAP[i]='1';
			return i;
		}
	}
	return -1;
}


int write_data_block(int blockNum, void *block)
{
	int t = fseek(disk, block_size*blockNum, SEEK_SET);
	int wr_size = fwrite(block, 1, block_size, disk);
    
    //check if all the bytes are written successfully
    if(wr_size != block_size)
        return -1;
	
    return 0;
}

int write_inode(inode *ptr, int inumber)
{
    //TODO: try to write better code here
	char *temp = (char*)malloc(sizeof(inode));
	temp = (char*)ptr;
	int t = inode_start_address + (inumber*sizeof(inode));
	t = fseek(disk,t,SEEK_SET);
	if(t!=0) 
		return 1;
	
	fwrite(temp, 1, sizeof(inode), disk);
	return 0;
}

void print_inode_bitmap()
{
    printf("Inode Bitmap\n");
	int x=0;
	char imap[4096];
	read_data_block(inode_bitmap_block, imap);
	for(x=0; x<total_num_inodes; x++)
	{
		printf("%c ", imap[x] );
	}
	printf("\n");
}

void print_data_bitmap()
{
    printf("Data Bitmap\n");
	char dmap[4096];
	read_data_block(data_bitmap_block, dmap);
	int x=0;
	for(x=0; x<num_data_blocks; x++)
	{
		printf("%c ", dmap[x] );
	}
	printf("\n");
}

char* read_inode(int inum)
{
	//printf("read_inode called at address %d\n",inode_start_address + (inum*inode_size));
	int t=inode_start_address+(inum*sizeof(inode));
	t=fseek(disk, t, SEEK_SET);
	if(t!=0) return NULL;
	char *temp = (char*)malloc(sizeof(char)*sizeof(inode));
	fread(temp, 1, sizeof(inode), disk);
	return temp;
}

int read_data_block(int blockNum, void *block)
{
//	printf("readData called at address %d\n",block_size*blockNum);
	fseek(disk, block_size*blockNum, SEEK_SET);
	// int i=0;
	fread(block, 1, block_size, disk);
    return 0;
}


int writeFile(char *filename, void *block)
{
	//printf("writeFile called\n");	
	inode *i = (inode*)read_inode(root_inode_num);
	char data[block_size];
	read_data_block(data_start_block+i->data_blocks[0], data);
	dir* root_dir = (dir*)data;
	char **this = root_dir->filenames;
	int j;
	for(j=0; j<root_dir->num_of_files; j++)
	{
		if(!strcmp(this[j],filename)){
			printf("\"%s\" file already exists!! Please choose a different name.\n", this[j] );
			return 1;
		}
	}

	read_data_block(inode_bitmap_block,INODE_BITMAP);
	read_data_block(data_bitmap_block,DATA_BITMAP);

	char *arr = (char*) block;
	int id = get_inode_num();
	if(id==-1)
	{
		printf("Cannot write more files! Out of inodes\n");
		return 1;
	}
	inode *ptr = create_inode(0, strlen(arr));
	//printf("File size: %d\n",ptr->size );


	//ptr->block_pointers = (int*)malloc(sizeof(int)*ptr->num_of_blocks);
	ptr->data_blocks[0]= get_free_datablock(id);//,ptr->size,ptr->block_pointers);
	if(ptr->data_blocks[0]==-1)
	{
		printf("Cannot write file! Insufficient disk Space\n");
		return 1;
	}

	//printf("write inode: %d\n",id );
	//printf("write block: %d\n",data_start_block+ptr->block_pointer);
	write_data_block(data_start_block+ptr->data_blocks[0], block);//!=0);// return 1;
	write_inode(ptr, id);

	inode *root = (inode*)read_inode(root_inode_num);
	char rootbuff[block_size];
	read_data_block(data_start_block+root->data_blocks[0], rootbuff);
	root_dir =(dir*)rootbuff;
	root_dir->filenames[root_dir->num_of_files] = filename;
	root_dir->filesizes[root_dir->num_of_files] = ptr->size;
	root_dir->fileinodes[root_dir->num_of_files] = id;
	root_dir->num_of_files++;
	root->size += ptr->size;
	write_data_block(data_start_block+root->data_blocks[0], (char*)root_dir);
	write_inode(root, root_inode_num);
	write_data_block(inode_bitmap_block, INODE_BITMAP);
	write_data_block(data_bitmap_block, DATA_BITMAP);
	return 0;
}


inode* create_inode(int isDir, int size)
{
//	printf("create_inode called\n");
	inode *new = (inode*)malloc(sizeof(inode));
	new->size=size;
	new->num_of_blocks = size/block_size + ((size%block_size)!=0);

	new->isDir = isDir;
	return new;
}

#endif