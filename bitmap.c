#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int checkb(int fd, int iorb){
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

void chanb(int fd, int num, int iorb) {
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
    printf("com: %d\n", com);
    com = 128 >> com;
    printf("change com: %d\n", com);
    printf("buf: %d\n", buf[0]);
    unsigned char new = buf[0] ^ com;
    printf("after caclue: %d\n", new);
    buf[0] = new;
    write(fd, buf, 1);
    free(buf);
}

int main(){
	int fd = open("text.txt", O_RDWR|O_CREAT, 0666);
	if(fd < 0){
		printf("Opening Error");
	}
	//unsigned char* buff = calloc(0, 2048);
	//write(fd, buff, 2048);
	int num = checkb(fd, 0);
	printf("first: %d\n", num);
	chanb(fd, num, 0);
	num = checkb(fd, 0);
	printf("second %d\n", num);
	close(fd);
}
