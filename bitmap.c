#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int checkb(int iorb){
	int fd = open("region.c", O_RDWR|O_CREAT, 0666);
	if (iorb == 0) { //for inode bitmap
        	lseek(fd, 1024, SEEK_SET);
        	unsigned char* buf = malloc(1024);
        	read(fd, buf, 1024);
        	for (int byte = 0; byte < 1024; byte++) {
	            	unsigned char comp = 128;
        	    	int count = 0;
        		if (buf[byte] != 255) {
                		while ((buf[byte] & comp)!=0) {
	                	    comp = comp >> 1;
	        	            count++;
        	        	}
	        	        return byte * 8 + count;
        	    	}
        	}
    	}	
	else if (iorb == 1){ //for data bitmap
                lseek(fd,2048, SEEK_SET);
                unsigned char* buf = malloc(1024*128);
                read(fd, buf, 1024*128);
                for(int byte = 0; byte<1024*128;byte++){
                        unsigned char comp;
                        int count = 0;
                        comp = 128;
                        while((buf[byte]&comp)!=0){
                        	comp = comp>>1;
                               	count++;
                        }
		free(buf);
                return byte*8+count;
		}	
	}
	else{
		printf("Parameter should be 0 or 1!");
	}

}

void chanb(int num, int iorb) {
	int fd = open("region.c", O_RDWR|O_CREAT, 0666);
	unsigned char* buf = malloc(1);
	if (iorb == 0) {
		lseek(fd, 1024 + (num/8) * 512, SEEK_SET);
	}
	else {
		lseek(fd, 2048 + (num/8) * 4096, SEEK_SET);
	}
	read(fd, buf, 1);
	lseek(fd, -1, SEEK_CUR);
	unsigned char com = num % 8;
	com = 128 >> com;
	unsigned char new = buf[0] ^ com;
	buf[0] = new;
	write(fd, buf, 1);
	free(buf);
}


