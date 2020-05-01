#include <stdio.h>
#include "myfs.h"
int main()
{
	// open_disk_file();
	MountFS();
	print_inode_bitmap();
	print_data_bitmap();

	// print_root_dir();

	int fd = my_open("test.txt", 1);

	print_root_dir();

	printf("main: fd: %d\n", fd);

	print_inode_bitmap();
	print_data_bitmap();

	char data[5] = "SHAH1";
	my_write(fd, data, 5);

	char read[5];
	my_read(fd, read, 5);
	printf("Data read: %s\n", read);

	print_root_dir();

	printf("HELLLO WORLD\n");
    return 0;
}