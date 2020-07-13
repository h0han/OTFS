#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

void ot_rmdir( const char* path){
        int inum = otfind(path);
        inode* ino;
        changb(inum,0);
        int fd = open("region.c", O_RDWR|O_CREAT, 0666);
        lseek(fd, 2048+1024*128, SEEK_SET);
        lseek(fd, inum, SEEK_CUR);
        read(fd, ino, 512);
        if (ino->file_or_dir == 0){ //path is file
                for(int i = 0;i<12;i++){
                        if(ino->FB[i] == NULL){
                                break;
                        }
                        changb(ino.FB[i], 1);
                        ino->FB[i] = NULL;
                }
        }
        else{ //path is directory
                for(int i = 0; i<12;i){
                        if(ino->DB[i] == NULL){
                                break;
                        }
                        changb(ino.DB[i], 1);
                        ino->DB[i] = NULL;
                }
        }
        lseek(fd, 512, SEEK_CUR);
        write(fd, ino, 512);
}
