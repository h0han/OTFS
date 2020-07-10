#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


void rmdir(int fd, const char* path){
	int inum = find_dir(path);
	lseek(fd, 2048+128*1024, SEEK_SET);
	lseek(fd, inum, SEEK_CUR);

}
