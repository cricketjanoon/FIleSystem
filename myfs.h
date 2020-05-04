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
int size_of_dir = 10;
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

int get_dir_entry(dir* _dir);
int get_inode_num();
int write_inode(inode *ptr, int inumber);
int write_data_block(int blockNum, void *block);
int get_free_datablock(int inumber);
int read_data_block(int blockNum,void *block);
char* read_inode(int inum);
inode* create_inode(int isDir, int size);

void print_inode_bitmap();
void print_data_bitmap();
void print_root_dir();

/* file descriptor */
fd_entry *fd_list_head=NULL, *fd_list_tail=NULL;
int cur_fd_num = 0;

fd_entry *find_fd_entry(int fd_or_inode_num);
int remove_fd_entry(int fd);
fd_entry *create_fd_entry(int inode_num, int read_offset, int write_offset, enum mode mode);


/* Super Block Data */
char *MAGIC_STRING = "882244";
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

FILE *disk;
super_block *sup_block;


int my_open(const char *pathname, int mode)
{
	//read root inode and data 
	inode *root_inode = (inode *)read_inode(sup_block->root_inode_num);
	char *block[sup_block->block_size];
	read_data_block(sup_block->data_start_block + root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	//search to see if file exists
	short is_new_file=1; //bool to check if file exist or not
	int j;
	for(j=0; j<root_dir->num_of_files; j++)
	{
		if(!strcmp(root_dir->filenames[j], pathname))
		{
			is_new_file = 0;
			break;
		}
	}

	if(is_new_file==1 && mode == READ)
	{
		printf("my_open(): File(READ) already opened.\n");
		return -1;
	}

	read_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
	read_data_block(sup_block->data_bitmap_block, DATA_BITMAP);

	if(is_new_file)
	{
		int new_inode_num = get_inode_num(); //TODO: error check
		inode* new_inode = create_inode(0, 0); //creating a inode for file size 0 

		int block_num = get_free_datablock(new_inode_num);
		if(block_num == -1)
		{
			printf("my_open(): No disk space. Unable to create the file.");
			return -1;
		}
		new_inode->data_blocks[0] = block_num;

		//now that file is created persist the inode in case of crash
		write_inode(new_inode, new_inode_num);


		//get index of free entry in root dir
		int free_dir_index = get_dir_entry(root_dir);

		//update root dir and inode
		root_dir->dir_bitmap[free_dir_index] = 1;
		strcpy(root_dir->filenames[free_dir_index], pathname);
		root_dir->fileinodes[free_dir_index] = new_inode_num;
		// root_dir->filesizes[root_dir->num_of_files] = new_inode->size;
		root_dir->num_of_files++;
		root_inode->size += new_inode->size;
		write_data_block(sup_block->data_start_block+root_inode->data_blocks[0], (char *) root_dir);

		write_inode(root_inode, sup_block->root_inode_num);

		//persist data and inode bitmap
		write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
		write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);
		
		//select read and write offset depending upon the mode of the file
		//because new file read and write offset are at the start
		int read_offset=0;
		int write_offset=0;
		
		fd_entry *new_fd = create_fd_entry(new_inode_num, read_offset, write_offset, mode);

		if(new_fd!=NULL)
			return new_fd->fd;
		else
			return -1;
		
	}
	else //if file with the name found in the directory
	{
		int file_inode_num = root_dir->fileinodes[j];

		//check to see if file is already opened
		fd_entry *file_fd = find_fd_entry(file_inode_num);

		if(file_fd != NULL) //if file is already opened
		{
			printf("my_open(): File already opened.\n");
			return -1;
		}
		else //file not opened
		{
			inode *file_inode = (inode *)read_inode(file_inode_num);

			int read_offset=0, write_offset=0;
			if(mode==READ)
			{
				read_offset = 0;
				write_offset = -1; //not that it matters
			}
			else if(mode == APPEND)
			{
				read_offset=0;
				write_offset=file_inode->size;
			}
			else if(mode == WRITE)
			{
				//TODO: come up with a better solution for replacing with empty file
				read_offset=0;
				write_offset=0;
				file_inode->size=0;
			}
			else if(mode = READ_WRITE)
			{
				read_offset=0;
				write_offset=file_inode->size;
			}

			fd_entry *new_fd = create_fd_entry(file_inode_num, read_offset, write_offset, mode);

			if(new_fd != NULL)
				return new_fd->fd;
			else
				return -1;			
		}
	}
}

int my_write(int fd, void *buffer, int count)
{
	//find find in the file description list
	fd_entry *fd_ent = find_fd_entry(fd);
	if(fd_ent==NULL)
	{
		printf("my_write(): Invalid file descriptor.\n");
		return -1;
	}

	//incase file is found but not opened in write supported mode
	if(fd_ent->mode == READ)
	{
		printf("my_write(): File not opened in write supported mode.\n");
		return -1;
	}

	inode *cur_file_inode = (inode *)read_inode(fd_ent->inode_num);

	//TODO: handle increase in size of the file
	int offset = sup_block->data_start_addresss + cur_file_inode->data_blocks[0]*sup_block->block_size + fd_ent->write_offset;

	offset = fseek(disk, offset, SEEK_SET);
	if(offset != 0) 
		return -1;

	int rt = fwrite(buffer, 1, count, disk);
	fd_ent->write_offset += count; //update the write_offset
	cur_file_inode->size += count; //increase the size of the file
	write_inode(cur_file_inode, fd_ent->inode_num);

	printf("my_write(): %d bytes written succesfully.\n", rt);
	return rt;

}

int my_read(int fd, void *buffer, int count)
{
	//find file in the file descriptor table
	fd_entry *fd_ent = find_fd_entry(fd);

	if(fd_ent == NULL)
	{
		printf("my_read(): Invalid file descriptor.\n");
		return -1;
	}

	//check if file open in read supported mode
	if(fd_ent->mode == READ || fd_ent->mode == READ_WRITE)
	{
		inode *cur_file_inode = (inode *)read_inode(fd_ent->inode_num);

		//setting the dsik pointer to from where to read
		int offset = sup_block->data_start_addresss + cur_file_inode->data_blocks[0]*sup_block->block_size + fd_ent->read_offset;
		// printf("read offset: %d\n", offset);
		offset = fseek(disk, offset, SEEK_SET);
		if(offset != 0) 
			return -1;

		int rt = fread(buffer, 1, count, disk);
		fd_ent->read_offset += count; //update the read_offset
		printf("my_read(): %d bytes read succesfully.\n", rt);
		return rt; //TODO: check error conditions
	}
	else
	{
		printf("my_read(): File is not opened in read supported mode.\n");
		return -1;
	}
	
}

int my_close(int fd)
{
	fd_entry *fd_ent = find_fd_entry(fd);

	if(fd_ent != NULL)
	{
		int rt = remove_fd_entry(fd);
		if(rt != 1)
		{
			printf("my_close(): Error closing the file.");
			return -1;
		}
		else
		{
			printf("my_close(): Successfully close the file with fd: %d\n", fd);
			return 0;
		}
	}
	else
	{
		printf("my_close(): No file found with the above fd number.\n");
		return -1;
	}
}

int my_unlink(const char *pathname)
{
	inode *root_inode = (inode *)read_inode(sup_block->root_inode_num);
	char block[sup_block->block_size];
	read_data_block(sup_block->data_start_block+root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;
	
	short file_found = 0;
	int index;
	for(index=0; index<size_of_dir; index++)
	{
		if(!strcmp(root_dir->filenames[index], pathname) && root_dir->dir_bitmap[index]==1)
		{
			file_found = 1;
			break;
		}
	}

	if(file_found)
	{
		int file_inode_num = root_dir->fileinodes[index];
		inode* file_inode = (inode *)read_inode(file_inode_num);

		// free datablock and inode for reuse
		read_data_block(sup_block->data_bitmap_block, DATA_BITMAP);
		read_data_block(sup_block->inode_bitmap_block ,INODE_BITMAP);
		DATA_BITMAP[file_inode->data_blocks[0]] = 0;
		INODE_BITMAP[file_inode_num] = 0;
		write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);
		write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);

		//remove entry from the directory
		root_dir->dir_bitmap[index] = 0;
		root_dir->num_of_files--;
		root_inode->size -= file_inode->size;

		//presist root inode data structure
		write_data_block(sup_block->data_start_block+root_inode->data_blocks[0], (char *) root_dir);

		printf("my_unlink(): file %s unlinked successfully.\n", root_dir->filenames[index]);

		return 0;
	}
	else
	{
		printf("my_unlink(): file not found.\n");
		return -1;
	}
}

void mount_filesystem()
{
	//open the disk file for both reading and writting
	disk = fopen("disk.bin","r+"); 
	if(disk == NULL)
	{
		printf("mount_filesystem(): Disk file not found.\n");
		return;
	}

	sup_block = (super_block *)malloc(sizeof(super_block));

	//if disk already mounted then return
	fseek(disk, 0, SEEK_SET);
	char *magic = (char *)malloc(sizeof(MAGIC_STRING));
	fread(magic, 1, sizeof(MAGIC_STRING), disk);
	if(magic!=NULL && !strcmp(magic, MAGIC_STRING))
	{
		printf("mount_filesystem(): Disk already mounted.\n");

		fseek(disk, strlen(MAGIC_STRING), SEEK_SET);
		fread(sup_block, 1, sizeof(super_block), disk);

		// printf("sup_ptr->block_size: %d\n", sup_block->block_size);
		// printf("sup_ptr->inode_bitmap_block: %d\n", sup_block->inode_bitmap_block);
		// printf("sup_ptr->inode_start_address: %d\n", sup_block->inode_start_address);
		return;
	}

	//initialize and save super block at the start of the disk for the first time
	sup_block->block_size = BLOCK_SIZE;
	sup_block->num_data_blocks = NUM_DATA_BLOCKS;
	sup_block->total_num_inodes = TOTAL_NUM_INODES;
	sup_block->data_start_block = DATA_START_BLOCK;
	sup_block->inode_start_block = INODE_START_BLOCK;
	sup_block->inode_start_address = INODE_START_ADDRESS;
	sup_block->data_start_addresss = DATA_START_ADDRESS;
	sup_block->root_inode_num = ROOT_INODE_NUM;
	sup_block->inode_bitmap_block = INODE_BITMAP_BLOCK;
	sup_block->data_bitmap_block = DATA_BITMAP_BLOCK;

	fseek(disk, strlen(MAGIC_STRING), SEEK_SET);
	fwrite(sup_block, 1, sizeof(super_block), disk);

	// printf("sup_ptr->block_size: %d\n", sup_block->block_size);
	// printf("sup_ptr->inode_bitmap_block: %d\n", sup_block->inode_bitmap_block);
	// printf("sup_ptr->inode_start_address: %d\n", sup_block->inode_start_address);

    //set data and i-note bitmap to zero
	for (int i = 0; i < sup_block->num_data_blocks; ++i)
	{
		DATA_BITMAP[i]='0';
	}
	for (int i = 0; i < sup_block->total_num_inodes; ++i)
	{
		INODE_BITMAP[i]='0';
	}

    //set inode bitmap equal to 1
	INODE_BITMAP[sup_block->root_inode_num] = '1';

	//creating a directory struct
	dir *root_dir = (dir *)malloc(sizeof(dir));
	root_dir->num_of_files = 0;
	for(int i=0; i<size_of_dir; i++) //resetting all the entries in the dir
		root_dir->dir_bitmap[i]=0;

	//creating an inode struct
	inode *root = (inode*)malloc(sizeof(inode));
	int size = sizeof(root_dir);
	root->size = size;
	// root->num_of_blocks = size/block_size + ((size%block_size)!=0);
	root->isDir = 1;
 
	int block_num = get_free_datablock(sup_block->root_inode_num);

	if(block_num == -1)
	{
		printf("No disk space. Unable to mount the disk.\n");
	}
	root->data_blocks[0] = block_num;

	write_data_block(sup_block->data_start_block + root->data_blocks[0], (char*)root_dir);
	write_inode(root, sup_block->root_inode_num);

	write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
	write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);

	//save magic string at the start of the disk(in super block) so that we know for future use that disk is mounted
	fseek(disk, 0, SEEK_SET);
	fwrite(MAGIC_STRING, 1, strlen(MAGIC_STRING), disk);

	printf("mount_filesystem(): Sucessfully mounted disk.\n");
}

/* HELPER FUNCTIONS */

int get_dir_entry(dir* _dir)
{
	for(int i=0; i<size_of_dir; i++)
	{
		if(_dir->dir_bitmap[i]==0)
		{
			return i;
		}
	}

	//if no free entry is found, return -1
	return -1;
}

int get_free_datablock(int inumber)
{
	int i=0;
	//check if any of the datablock is free and return its number
	while(i < sup_block->num_data_blocks)
	{
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
	//find available inode number
	for (int i = sup_block->root_inode_num; i < sup_block->total_num_inodes; ++i)
	{
		if(INODE_BITMAP[i]=='0')
		{
			INODE_BITMAP[i]='1';
			return i;
		}
	}
	//incase no free inode is available, return -1
	return -1;
}

int write_data_block(int blockNum, void *block)
{
	fseek(disk, sup_block->block_size*blockNum, SEEK_SET);
	int wr_size = fwrite(block, 1, sup_block->block_size, disk);
    
    //check if all the bytes are written successfully
    if(wr_size != sup_block->block_size)
        return -1;
	
    return 0;
}

int write_inode(inode *ptr, int inumber)
{
    //TODO: try to write better code here
	char *temp = (char*)malloc(sizeof(inode));
	temp = (char*)ptr;
	int t = sup_block->inode_start_address + (inumber*sizeof(inode));
	t = fseek(disk, t, SEEK_SET);
	if(t!=0) 
		return 1;
	
	fwrite(temp, 1, sizeof(inode), disk);
	return 0;
}

char* read_inode(int inum)
{
	int t = sup_block->inode_start_address+(inum*sizeof(inode));
	t = fseek(disk, t, SEEK_SET);
	if(t != 0) 
		return NULL;
	
	char *temp = (char*)malloc(sizeof(char)*sizeof(inode));
	fread(temp, 1, sizeof(inode), disk);

	return temp;
}

int read_data_block(int blockNum, void *block)
{
	fseek(disk, sup_block->block_size*blockNum, SEEK_SET);
	fread(block, 1, sup_block->block_size, disk);

    return 0;
}

inode* create_inode(int isDir, int size)
{
	inode *new = (inode*)malloc(sizeof(inode));
	new->size=size;
	// new->num_of_blocks = size/block_size + ((size%block_size)!=0);
	new->isDir = isDir;

	return new;
}

/* FILE DESCRIPTOR LIST*/

fd_entry *create_fd_entry(int inode_num, int read_offset, int write_offset, enum mode mode)
{
	//create a new fd_entry
	fd_entry *new_entry = (fd_entry*)malloc(sizeof(fd_entry));
	new_entry->fd = cur_fd_num++;
	new_entry->inode_num = inode_num;
	new_entry->read_offset = read_offset;
	new_entry->write_offset = write_offset;
	new_entry->mode = mode;

	//add it to the list
	if(fd_list_head == NULL && fd_list_tail == NULL) //base case: if the list is empty
	{
		fd_list_head = new_entry;
		fd_list_tail = new_entry;
		new_entry->next = NULL;
	}
	else //if list has atleast one element
	{
		fd_list_tail->next = new_entry;
		fd_list_tail = new_entry;
		new_entry->next = NULL;
	}
	
	return new_entry;
}

int remove_fd_entry(int fd)
{
	fd_entry *cur=fd_list_head, *prev=fd_list_head;
	while(cur != NULL) //traverse the list to find the entry
	{
		if(cur !=NULL && cur->fd == fd)
		{
			break;
		}
		else
		{
			prev = cur;
			cur = cur->next;
		}
	}

	if(cur !=NULL) //if found remove from the list
	{
		if(cur == prev) //first element of the list
		{
			if(cur==fd_list_tail) //means there was only one element
			{
				fd_list_head=NULL;
				fd_list_tail=NULL;
				free(cur);
				return 1;
			}
			else
			{
				fd_list_head=cur->next;
				free(cur);
				return 1;
			}
			
			//consider about lists head an tail while deleting node
		}
		else //anywhere from 2nd to last in the list
		{
			if(cur==fd_list_tail)//last element of the list
			{
				fd_list_tail = prev;
				fd_list_tail->next = NULL;
				free(cur);
				return 1;
			}
			else
			{
				prev->next = cur->next;
				free(cur);
				return 1;		
			}
		}
	}
	else
	{
		printf("Invalid fd to remove.\n");
		return -1;
	}
}

fd_entry *find_fd_entry(int fd_or_inode_num)
{
	fd_entry *cur=fd_list_head, *prev=fd_list_head;
	while(cur != NULL) //traverse the list to find the entry
	{
		if(cur !=NULL && (cur->fd == fd_or_inode_num || cur->inode_num == fd_or_inode_num))
		{
			break;
		}
		else
		{
			prev = cur;
			cur = cur->next;
		}
	}
	return cur;
}

/* LOG PRINTING FUNCTIONS  */

void unmount_disk()
{
	fclose(disk);
}

void print_root_dir()
{
	inode *root_inode = (inode *)read_inode(sup_block->root_inode_num);

	char *block[sup_block->block_size];
	read_data_block(sup_block->data_start_block + root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	printf("*********** Root Dir(number_of_files: %d) ***********\n", root_dir->num_of_files);
	for(int i=0; i<size_of_dir; i++)
	{
		if(root_dir->dir_bitmap[i] == 1)
		{
			inode* inode_ptr = (inode *)read_inode(root_dir->fileinodes[i]);
			printf("Name: %s, Size: %d, Inode: %d, D-Block: %d\n", root_dir->filenames[i], inode_ptr->size, root_dir->fileinodes[i], inode_ptr->data_blocks[0]);
		}
	}
	printf("****************************************************\n");
}

void print_inode_bitmap()
{
    printf("Inode Bitmap: ");
	char inode_bitmap[sup_block->block_size];
	read_data_block(sup_block->inode_bitmap_block, inode_bitmap);
	for(int i=0; i<sup_block->total_num_inodes; i++)
	{
		printf("%c ", inode_bitmap[i]);
	}
	printf("\n");
}

void print_data_bitmap()
{
    printf("Data Bitmap: ");
	char data_bitmap[sup_block->block_size];
	read_data_block(sup_block->data_bitmap_block, data_bitmap);
	for(int i=0; i<sup_block->num_data_blocks; i++)
	{
		printf("%c ", data_bitmap[i]);
	}
	printf("\n");
}

#endif