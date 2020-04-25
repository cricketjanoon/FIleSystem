#ifndef __MYFS_H
#define __MYFS_H


int my_open(const char *pathname, int mode);

int my_close(int fd);

int my_read(int fd, void *buffer, int count);

int my_write(int fd, void *buffer, int count);

int my_mkdir(const char *pathname);

int my_format(int blocksize);

int my_unlink(const char *pathname);

int my_rmdir(const char *pathname);


#endif