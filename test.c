#include <stdio.h>
#include "myfs.h"
int main()
{
	mount_filesystem();

	int fd1 = my_open("file1.txt", READ_WRITE);
	int fd2 = my_open("file2.txt", READ_WRITE);
	int fd3 = my_open("file3.txt", READ_WRITE);
	int fd4 = my_open("file4.txt", READ_WRITE);

	char *buffer = "CS570"; 
	my_write(fd1, buffer, strlen(buffer));

	char *buffer1 = "Advance OS";
	my_write(fd1, buffer1, strlen(buffer1));

	char *buffer3 = "#include <iostream>";
	my_write(fd3, buffer3, strlen(buffer3));

	char *buffer4 = "How to implement filesystem";
	my_write(fd4, buffer4, strlen(buffer4));

	my_mkdir("dir");
	my_mkdir("dir/level1");
	my_mkdir("dir/level1/level2");

	my_open("dir/level1/level2/lev2.txt", READ_WRITE);
	my_open("dir/level1/lev1.txt", READ_WRITE);

	print_fd_table();

	char *read_buffer = (char *)malloc(strlen(buffer));
	my_read(fd1, read_buffer, strlen(buffer));
	printf("Data read: %s\n", read_buffer);
	
	print_data_bitmap();
	print_inode_bitmap();
	print_all_files();


	my_rmdir("dir");
	print_data_bitmap();
	print_inode_bitmap();
	print_all_files();

	unmount_disk();

    return 0;
}