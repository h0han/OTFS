#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
//#include "miniz.h"
#include "zlib.h"
#include "time.h"
#include <string.h>
#define _CRT_SECURE_NO_WARNINGS

int get_file_size(int fd){
        int fsize;
        fsize = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        return fsize;
}

int main(int argc, char** argv){
        clock_t start = clock();
        char* buf = NULL;
        int fsize;
        unsigned long INBUFSIZE;
        unsigned long OUTBUFSIZE;
        int fd = open(argv[1], O_RDONLY, 0666);
        if (fd==-1){
                printf("Open error\n");
        }
        fsize = get_file_size(fd);
        INBUFSIZE = fsize+1;
        OUTBUFSIZE = (unsigned long)1.001*(INBUFSIZE+12)+1;
        printf("%s\n", argv[1]);

        char af[200] = "comp_";
        strcat(af, argv[1]);
        printf("%s\n", af);

        int cfd = open(af, O_RDWR|O_CREAT, 0666);
        if (cfd==-1){
                printf("Open error\n");
        }

        unsigned char *buff = malloc(INBUFSIZE);
        unsigned char *cbuff = malloc(OUTBUFSIZE);
        //uLongf size= 11;
        read(fd, buff, fsize);
        //miniz_comp(buff);

        int err = compress2(cbuff, &OUTBUFSIZE, buff, INBUFSIZE, 9);
                if(err != Z_OK){
                printf("something is wrong\n");
        }
        write(cfd, cbuff, OUTBUFSIZE);
        printf("%d\n", get_file_size(cfd));
        close(fd);
        close(cfd);
        printf("Compression is done\n");
        clock_t end = clock();
        printf("time : %lf\n", (double)(end - start)/CLOCKS_PER_SEC);
        free(buff);
        free(cbuff);
        return 0;
}

