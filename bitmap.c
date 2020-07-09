#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int checkb(int fd, int iorb){
	if (iorb == 0){ //for inode bitmap
		lseek(fd,1024, SEEK_SET);
		unsigned char buf[1024];
		read(fd, &buf, 1024);
		for(int byte = 0; byte<1024;byte++){
			unsigned char comp;
			int count;
			//if(byte!=0){
				count = 0;
				comp = 128;
				while(buf[byte]&&comp){
					comp = comp>>1;
					count++;
				}
				return byte*8+count;
			/*}
			else{
				count = 2;
				comp = 32;
				while(buf[byte]&&comp){
					comp = comp>>1;
					count++;
				}
				return count;
			}*/
		}

	}
	else if (iorb == 1){ //for data bitmap
                lseek(fd,2048, SEEK_SET);
                unsigned char buf[1024*128];
                read(fd, &buf, 1024*128);
                for(int byte = 0; byte<1024*128;byte++){
                        unsigned char comp;
                        int count = 0;
                        if(byte!=0){
                                comp = 128;
                                while(buf[byte]&&comp){
                                        comp = comp>>1;
                                        count++;
                                }
                                return byte*8+count;
                        }
                        else{
                                comp = 32;
                                while(buf[byte]&&comp){
                                        comp = comp>>1;
                                        count++;
                                }
                                return count;
                        }
                }
	}
	else{
		printf("Parameter should be 0 or 1!");
	}

}
void chanb(int fd, int num, int iorb){
	unsigned char buf[1];
	if(iorb ==0){
		lseek(fd, 1024+num*512, SEEK_SET);
	}
	else{
		lseek(fd, 2048+num*4096, SEEK_SET);
	}
	read(fd, &buf, 1);
	lseek(fd, -1, SEEK_CUR);
	unsigned char new = ~buf[0];
	buf[0] = new;
	write(fd, &buf, 1);
	
}
int main(){
	int fd = open("text.c", O_RDWR|O_CREAT, 0666);
	int num = checkb(fd, 0);
	printf("%d\n", num);
	chanb(fd, num, 0);
	printf("%d\n", num);
	close(fd);
}
