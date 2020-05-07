#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myfs.h"

//assuming that dir can store 10 files maximum
int size_of_dir = 10;

//for file descriptor table 
fd_entry *fd_list_head=NULL, *fd_list_tail=NULL;
int cur_fd_num = 0;

char *MAGIC_STRING = "882244";

FILE *disk;
super_block *sup_block;

//Due to shortage of time, I din't write the whole logic myself istead borrowed the function
// and used it for my purpoes accordingly (6th May, 2020)
// ref: https://stackoverflow.com/questions/27261558/splitting-up-a-path-string-in-c
char **split_pathname(char *path, int *size)
{
    char *tmp;
    char **splitted = NULL;
    int i, length;

    if (!path){
        goto Exit;
    }

    tmp = strdup(path);
    length = strlen(tmp);

    *size = 1;
    for (i = 0; i < length; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            (*size)++;
        }
    }

    splitted = (char **)malloc(*size * sizeof(*splitted));
    if (!splitted) {
        free(tmp);
        goto Exit;
    }

    for (i = 0; i < *size; i++) {
        splitted[i] = strdup(tmp);
        tmp += strlen(splitted[i]) + 1;
    }
    return splitted;

Exit:
    *size = 0;
    return NULL;
}

int my_open(const char *pathname, int mode)
{
	//read root inode and data 
	inode *root_inode = (inode *)read_inode(sup_block->root_inode_num);
	char *block[sup_block->block_size];
	read_data_block(sup_block->data_start_block + root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	//STAR COM1: traverse the pathname to the file and parent directory
	dir* parent_dir;
	inode* parent_dir_inode;
	int parent_dir_inode_num;

	char *dup_pathname = strdup(pathname);

	int size;
	char **splited_pathname;
	char filename[10];
	
	splited_pathname = split_pathname(dup_pathname, &size);

	if(size==1)
	{
		strcpy(filename, pathname);
		parent_dir = root_dir;
		parent_dir_inode = root_inode;
		parent_dir_inode_num = sup_block->root_inode_num;
	}
	else
	{
		dir* cur_dir=root_dir;
		inode* cur_dir_inode=root_inode;
		int cur_dir_inode_num=sup_block->root_inode_num;

		for(int i=0; i<size; i++)
		{
			if(i==size-1)
			{
				strcpy(filename, splited_pathname[i]);
				parent_dir = cur_dir;
				parent_dir_inode = cur_dir_inode;
				parent_dir_inode_num = cur_dir_inode_num;
			}
			else
			{
				int result_found = 0;
				for(int k=0; k<size_of_dir; k++)
				{
					if(cur_dir->dir_bitmap[k]==1 && !strcmp(cur_dir->filenames[k], splited_pathname[i]))
					{
						inode* in = (inode *) read_inode(cur_dir->fileinodes[k]);
						if(in->isDir)
						{
							cur_dir_inode_num = cur_dir->fileinodes[k];
							cur_dir_inode = in;
							read_data_block(sup_block->data_start_block+in->data_blocks[0], block);
							cur_dir = (dir *)block;
							result_found = 1;
							break;
						}
						else
						{
							continue;
						}
					}
				}

				if(!result_found)
				{
					printf("my_open(): Invalid pathanme '%s'. \n", pathname);
					return -1;
				}
			}
		}
	}
	//END COM1: traverse the pathname to the file and parent directory
	
	//search to see if file exists
	short is_new_file=1; //bool to check if file exist or not
	int j;
	for(j=0; j<size_of_dir; j++)
	{
		if(parent_dir->dir_bitmap[j] == 1 && !strcmp(parent_dir->filenames[j], filename))
		{
			inode* inode_ptr = (inode *)read_inode(parent_dir->fileinodes[j]);
			if(!inode_ptr->isDir)
			{	is_new_file = 0;
				break;
			}
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
		int free_dir_index = get_dir_entry(parent_dir);

		//update root dir and inode
		parent_dir->dir_bitmap[free_dir_index] = 1;
		strcpy(parent_dir->filenames[free_dir_index], filename);
		printf("my_open(): filename: %s\n", parent_dir->filenames[free_dir_index]);
		parent_dir->fileinodes[free_dir_index] = new_inode_num;
		// root_dir->filesizes[root_dir->num_of_files] = new_inode->size;
		parent_dir->num_of_files++;
		parent_dir_inode->size += new_inode->size;
		write_data_block(sup_block->data_start_block+parent_dir_inode->data_blocks[0], (char *) parent_dir);

		write_inode(parent_dir_inode, parent_dir_inode_num);

		//persist data and inode bitmap
		write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
		write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);
		
		//select read and write offset depending upon the mode of the file
		//because new file read and write offset are at the start
		int read_offset=0;
		int write_offset=0;
		
		fd_entry *new_fd = create_fd_entry(new_inode_num, read_offset, write_offset, mode);

		// free(filename);

		if(new_fd!=NULL)
			return new_fd->fd;
		else
			return -1;
		
	}
	else //if file with the name found in the directory
	{
		int file_inode_num = parent_dir->fileinodes[j];

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
				write_offset = 0;
			}
			else if(mode == APPEND)
			{
				read_offset=0;
				write_offset=file_inode->size;
			}
			else if(mode == WRITE)
			{
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

	// printf("Writing %d bytes to inode %d, fd: %d\n", count, fd_ent->inode_num, fd_ent->fd);

	int offset = sup_block->data_start_addresss + cur_file_inode->data_blocks[0]*sup_block->block_size + fd_ent->write_offset;

	offset = fseek(disk, offset, SEEK_SET);
	if(offset != 0) 
		return -1;

	int rt = fwrite(buffer, 1, count, disk);
	fd_ent->write_offset += count; //update the write_offset
	cur_file_inode->size += count; //increase the size of the file
	write_inode(cur_file_inode, fd_ent->inode_num);
	free(cur_file_inode);
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
	//read root inode and root directory
	inode *root_inode = (inode *)read_inode(sup_block->root_inode_num);
	char block[sup_block->block_size];
	read_data_block(sup_block->data_start_block+root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	//START COM1: traverse the pathname to find the file and parent directory
	dir* parent_dir;
	inode* parent_dir_inode;
	int parent_dir_inode_num;

	char *dup_pathname = strdup(pathname);

	int size;
	char **splited_pathname;
	char filename[10];
	
	splited_pathname = split_pathname(dup_pathname, &size);

	if(size==1)
	{
		strcpy(filename, pathname);
		parent_dir = root_dir;
		parent_dir_inode = root_inode;
		parent_dir_inode_num = sup_block->root_inode_num;
	}
	else
	{
		dir* cur_dir=root_dir;
		inode* cur_dir_inode=root_inode;
		int cur_dir_inode_num=sup_block->root_inode_num;

		for(int i=0; i<size; i++)
		{
			if(i==size-1)
			{
				strcpy(filename, splited_pathname[i]);
				parent_dir = cur_dir;
				parent_dir_inode = cur_dir_inode;
				parent_dir_inode_num = cur_dir_inode_num;
			}
			else
			{
				int result_found = 0;
				for(int k=0; k<size_of_dir; k++)
				{
					if(cur_dir->dir_bitmap[k]==1 && !strcmp(cur_dir->filenames[k], splited_pathname[i]))
					{
						inode* in = (inode *) read_inode(cur_dir->fileinodes[k]);
						if(in->isDir)
						{
							cur_dir_inode_num = cur_dir->fileinodes[k];
							cur_dir_inode = in;
							read_data_block(sup_block->data_start_block+in->data_blocks[0], block);
							cur_dir = (dir *)block;
							result_found = 1;
							break;
						}
						else
						{
							continue;
						}
					}
				}

				if(!result_found)
				{
					printf("my_unlink(): Invalid pathanme '%s'. \n", pathname);
					return -1;
				}
			}
		}
	}
	//END COM1: traverse the pathname to find the file and parent directory

	// now find the file in the parent directory
	short file_found = 0;
	int index;
	for(index=0; index<size_of_dir; index++)
	{
		if(!strcmp(parent_dir->filenames[index], filename) && parent_dir->dir_bitmap[index]==1)
		{
			file_found = 1;
			break;
		}
	}

	if(file_found)
	{
		int file_inode_num = parent_dir->fileinodes[index];
		inode* file_inode = (inode *)read_inode(file_inode_num);

		// free datablock and inode for reuse
		read_data_block(sup_block->data_bitmap_block, DATA_BITMAP);
		read_data_block(sup_block->inode_bitmap_block ,INODE_BITMAP);
		DATA_BITMAP[file_inode->data_blocks[0]] = '0';
		INODE_BITMAP[file_inode_num] = '0';
		write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);
		write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);

		//remove entry from the directory
		parent_dir->dir_bitmap[index] = 0;
		parent_dir->num_of_files--;
		parent_dir_inode->size -= file_inode->size;

		//presist root inode data structure
		write_data_block(sup_block->data_start_block+parent_dir_inode->data_blocks[0], (char *) parent_dir);

		printf("my_unlink(): file %s unlinked successfully.\n", parent_dir->filenames[index]);

		return 0;
	}
	else
	{
		printf("my_unlink(): file not found.\n");
		return -1;
	}
}

int my_format(int blocksize)
{
	//read root inode
	inode *root_inode = (inode *)read_inode(sup_block->root_inode_num);
	
	read_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
	read_data_block(sup_block->data_bitmap_block, DATA_BITMAP);

	//reset inode bitmap and data bitmap
	for(int i=0; i<sup_block->num_data_blocks; i++)
		DATA_BITMAP[i] = '0';
	for(int i=0; i<sup_block->total_num_inodes; i++)
		INODE_BITMAP[i] = '0';

	//for root directory
	INODE_BITMAP[sup_block->root_inode_num] = '1';
	DATA_BITMAP[root_inode->data_blocks[0]] = '1';

	//persist data and inode bitmap
	write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
	write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);

	//read root directory
	char block[sup_block->block_size];
	read_data_block(root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	//empty the root directory
	root_inode->size = 0;
	root_dir->num_of_files = 0;
	for(int i=0; i<size_of_dir; i++)
	{
		root_dir->dir_bitmap[i] = 0;
	}

	//presist root inode and root directory
	write_inode(root_inode, sup_block->root_inode_num);
	write_data_block(sup_block->data_start_block+root_inode->data_blocks[0], root_dir);

	//remove any open files from fd_table
	while(fd_list_head != NULL)
	{
		remove_fd_entry(fd_list_head->fd);
	}
}

int my_mkdir(const char *pathname)
{
	//read root inode and root directory
	inode* root_inode = (inode *)read_inode(sup_block->root_inode_num);
	char block[sup_block->block_size];
	read_data_block(sup_block->data_start_block+root_inode->data_blocks[0], block);
	dir* root_dir = (dir *)block;

	//START COM1: traverse the pathanme and find the file and parent directory
	dir* parent_dir;
	inode* parent_dir_inode;
	int parent_dir_inode_num;

	char *dup_pathname = strdup(pathname);

	int size;
	char **splited_pathname;
	char *filename;
	
	splited_pathname = split_pathname(dup_pathname, &size);

	if(size==1)
	{
		printf("successful\n");
		filename = strdup(pathname);
		parent_dir = root_dir;
		parent_dir_inode = root_inode;
		parent_dir_inode_num = sup_block->root_inode_num;
	}
	else
	{
		dir* cur_dir=root_dir;
		inode* cur_dir_inode=root_inode;
		int cur_dir_inode_num=sup_block->root_inode_num;

		for(int i=0; i<size; i++)
		{
			if(i==size-1)
			{
				filename = splited_pathname[i];
				parent_dir = cur_dir;
				parent_dir_inode = cur_dir_inode;
				parent_dir_inode_num = cur_dir_inode_num;
			}
			else
			{
				int result_found = 0;
				for(int k=0; k<size_of_dir; k++)
				{
					if(cur_dir->dir_bitmap[k]==1 && !strcmp(cur_dir->filenames[k], splited_pathname[i]))
					{
						inode* in = (inode *) read_inode(cur_dir->fileinodes[k]);
						if(in->isDir)
						{
							cur_dir_inode_num = cur_dir->fileinodes[k];
							cur_dir_inode = in;
							read_data_block(sup_block->data_start_block+in->data_blocks[0], block);
							cur_dir = (dir *)block;
							result_found = 1;
							break;
						}
						else
						{
							continue;
						}
					}
				}

				if(!result_found)
				{
					printf("my_mkdir(): Invalid pathanme '%s'. \n", pathname);
					return -1;
				}
			}
		}
	}
	//END COM1: traverse the pathanme and find the file and parent directory

	//get avaialble entry in direcotry
	int index = get_dir_entry(parent_dir);
	parent_dir->dir_bitmap[index] = 1;

	//check to see if the dir with that name alreasy exits or not
	short is_dir_already_present=0; 
	int j;
	for(j=0; j<size_of_dir; j++)
	{
		if(parent_dir->dir_bitmap[j] == 1 && !strcmp(parent_dir->filenames[j], filename))
		{
			inode* inode_ptr = (inode *)read_inode(parent_dir->fileinodes[j]);
			if(inode_ptr->isDir)
			{	is_dir_already_present = 1;
				break;
			}
		}
	}

	if(is_dir_already_present)
	{
		printf("my_mkdir(): dir with name '%s' already present.\n", pathname);
		return -1;
	}

	read_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
	read_data_block(sup_block->data_bitmap_block, DATA_BITMAP);

	//make inode and direcotry for new direcory
	int new_dir_inode_num = get_inode_num();
	inode* new_dir_inode = create_inode(1, 0);
	int block_num = get_free_datablock(new_dir_inode_num);
	if(block_num == -1)
	{
		printf("my_mkdir(): not enough space.\n");
		return -1;
	}
	new_dir_inode->data_blocks[0]=block_num;

	write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);
	write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);

	strcpy(parent_dir->filenames[index], filename);
	parent_dir->fileinodes[index] = new_dir_inode_num;

	dir* new_dir = (dir *)malloc(sizeof(dir));
	new_dir->num_of_files = 0;
	for(int i=0; i<size_of_dir; i++) //resetting all the entries in the dir
		new_dir->dir_bitmap[i]=0;

	write_data_block(sup_block->data_start_block+parent_dir_inode->data_blocks[0], parent_dir);
	write_data_block(sup_block->data_start_block+new_dir_inode->data_blocks[0], new_dir);
	write_inode(new_dir_inode, new_dir_inode_num);
	
	printf("my_mkdir(): \"%s\" dir successfully created.\n", pathname);
	return 0;
}

int my_rmdir(const char *pathname)
{
	//read root inode and root directory
	inode* root_inode = (inode *)read_inode(sup_block->root_inode_num);
	char block[sup_block->block_size];
	read_data_block(sup_block->data_start_block+root_inode->data_blocks[0], block);
	dir* root_dir = (dir *)block;

	//START COM1: traverse pathanme and find the file and parent directory
	dir* parent_dir;
	inode* parent_dir_inode;
	int parent_dir_inode_num;

	char *dup_pathname = strdup(pathname);

	int size;
	char **splited_pathname;
	char *dir_to_remove;
	
	splited_pathname = split_pathname(dup_pathname, &size);

	if(size==1)
	{
		dir_to_remove = strdup(pathname);
		parent_dir = root_dir;
		parent_dir_inode = root_inode;
		parent_dir_inode_num = sup_block->root_inode_num;
	}
	else
	{
		dir* cur_dir=root_dir;
		inode* cur_dir_inode=root_inode;
		int cur_dir_inode_num=sup_block->root_inode_num;

		for(int i=0; i<size; i++)
		{
			if(i==size-1)
			{
				dir_to_remove = splited_pathname[i];
				parent_dir = cur_dir;
				parent_dir_inode = cur_dir_inode;
				parent_dir_inode_num = cur_dir_inode_num;
			}
			else
			{
				int result_found = 0;
				for(int k=0; k<size_of_dir; k++)
				{
					if(cur_dir->dir_bitmap[k]==1 && !strcmp(cur_dir->filenames[k], splited_pathname[i]))
					{
						inode* in = (inode *) read_inode(cur_dir->fileinodes[k]);
						if(in->isDir)
						{
							cur_dir_inode_num = cur_dir->fileinodes[k];
							cur_dir_inode = in;
							read_data_block(sup_block->data_start_block+in->data_blocks[0], block);
							cur_dir = (dir *)block;
							result_found = 1;
							break;
						}
						else
						{
							continue;
						}
					}
				}

				if(!result_found)
				{
					printf("rm_mkdir(): Invalid pathanme '%s'. \n", pathname);
					return -1;
				}
				else
				{
					parent_dir = cur_dir;
					parent_dir_inode = cur_dir_inode;
					parent_dir_inode_num = cur_dir_inode_num;	
				}	
			}
		}
	}

	//remove the directory and direcotries inside recursively
	for(int i=0; i<size_of_dir; i++)
	{
		if(parent_dir->dir_bitmap[i]==1 && !strcmp(parent_dir->filenames[i], dir_to_remove))
		{
			inode* in = (inode *) read_inode(parent_dir->fileinodes[i]);
			if(in->isDir)
			{
				char block[sup_block->block_size];
				read_data_block(sup_block->data_start_block+in->data_blocks[0], block);
				dir* _dir = (dir *)block;

				rm_dir_recursively(_dir);

				DATA_BITMAP[in->data_blocks[0]] = '0';
				INODE_BITMAP[parent_dir->fileinodes[i]] = '0';
				parent_dir->dir_bitmap[i] = 0;
				parent_dir_inode->size -= in->size;
				break;
			}
		}
	}

	//presist freed data blocks and inodes
	write_inode(parent_dir_inode, parent_dir_inode_num);
	write_data_block(sup_block->data_start_block+parent_dir_inode->data_blocks[0], parent_dir);
	write_data_block(sup_block->data_bitmap_block, DATA_BITMAP);
	write_data_block(sup_block->inode_bitmap_block, INODE_BITMAP);

}

void rm_dir_recursively(dir* dir_to_remove)
{
	for(int i=0; i<size_of_dir; i++)
	{
		if(dir_to_remove->dir_bitmap[i] == 1)
		{
			inode* inode_ptr = (inode *)read_inode(dir_to_remove->fileinodes[i]);

			//if file is a directory and recursively remove it
			if(inode_ptr->isDir)
			{	
				char block[sup_block->block_size];
				read_data_block(sup_block->data_start_block+inode_ptr->data_blocks[0], block);
				dir* _dir = (dir *)block;
				rm_dir_recursively(_dir);
				DATA_BITMAP[inode_ptr->data_blocks[0]] = '0';
				INODE_BITMAP[dir_to_remove->fileinodes[i]] = '0';
			}
			else // if file, then free inode and datablocks
			{
				DATA_BITMAP[inode_ptr->data_blocks[0]] = '0';
				INODE_BITMAP[dir_to_remove->fileinodes[i]] = '0';
			}	
		}
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
	root->isDir = 1;
 
	int block_num = get_free_datablock(sup_block->root_inode_num);

	if(block_num == -1)
	{
		printf("No disk space. Unable to mount the disk.\n");
		return;
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
	char *temp = (char*)ptr;
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
	new->isDir = isDir;

	return new;
}

/* FILE DESCRIPTOR LIST*/

fd_entry *create_fd_entry(int inode_num, int read_offset, int write_offset, enum mode mode)
{
	//create a new fd_entry
	fd_entry *new_entry = (fd_entry*)malloc(sizeof(fd_entry));
	new_entry->fd = ++cur_fd_num;
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
		if(cur->fd == fd_or_inode_num || cur->inode_num == fd_or_inode_num)
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

void print_fd_table()
{
	printf("*********** File Discriptor Table  *****************\n");
	fd_entry *cur=fd_list_head, *prev=fd_list_head;
	while(cur != NULL) //traverse the list to find the entry
	{
		printf("fd: %d, inode_num: %d, read_off: %d, write_off: %d\n", cur->fd, cur->inode_num, cur->read_offset, cur->write_offset);
		prev = cur;
		cur = cur->next;
	}
	printf("****************************************************\n");
}

//recursively print the direcories and files inside them
void print_dir(dir* root_dir, int* space)
{
	(*space)++;
	for(int i=0; i<size_of_dir; i++)
	{
		if(root_dir->dir_bitmap[i] == 1)
		{
			inode* inode_ptr = (inode *)read_inode(root_dir->fileinodes[i]);
			for(int i=0; i<(*space); i++) printf("--");
			printf("%d Name: %s, Size: %d, Inode: %d, D-Block: %d\n", inode_ptr->isDir, root_dir->filenames[i], inode_ptr->size, 
						root_dir->fileinodes[i], inode_ptr->data_blocks[0]);
			if(inode_ptr->isDir)
			{	
				char block[sup_block->block_size];
				read_data_block(sup_block->data_start_block+inode_ptr->data_blocks[0], block);
				dir* _dir = (dir *)block;
				print_dir(_dir, space);
		}	}
	}
	(*space)--;
}

void print_all_files()
{
	inode *root_inode = (inode *)read_inode(sup_block->root_inode_num);

	char *block[sup_block->block_size];
	read_data_block(sup_block->data_start_block + root_inode->data_blocks[0], block);
	dir *root_dir = (dir *)block;

	int space = -1;

	printf("****************************************************\n");
	print_dir(root_dir, &space);
	// printf("*********** Root Dir(number_of_files: %d) ***********\n", root_dir->num_of_files);
	// for(int i=0; i<size_of_dir; i++)
	// {
	// 	if(root_dir->dir_bitmap[i] == 1)
	// 	{
	// 		inode* inode_ptr = (inode *)read_inode(root_dir->fileinodes[i]);
	// 		printf("%d Name: %s, Size: %d, Inode: %d, D-Block: %d\n", inode_ptr->isDir, root_dir->filenames[i], inode_ptr->size, root_dir->fileinodes[i], inode_ptr->data_blocks[0]);
	// 	}
	// }
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
