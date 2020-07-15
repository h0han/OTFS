#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <zlib.h>
#include <time.h>

int get_file_size(int fd){
	int fsize;
	fsize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	return fsize;
}


//argv[1] = source;

int main (int argc, char** argv)
{
	clock_t start = clock();
	char* buf = NULL;
	int fsize;
	gzFile gzfp;	
	char comp_file[128] = "comp_";
	strcat(comp_file, argv[1]);

	/* check arguments */
	if (argc != 2) {
		printf ("argument Error\n");
		exit (-1);
	}

	int fd;
	if ((fd = open(argv[1], O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}
	fsize = get_file_size(fd);
	buf = malloc(fsize);
	read(fd, buf, fsize);
	close(fd);	

	if ((gzfp = gzopen(comp_file, "wb")) == NULL) {
		printf("Error_gz\n");
		return 0;
	}
	gzwrite(gzfp, buf, fsize);
	gzclose(gzfp);
	//fd = open(argv[2], O_RDWR|O_CREAT, 0666);
	//write(fd, buf, fsize);
	//close(fd);
		
	free(buf);
	printf("%s is compressed to %s (%d bytes)\n", argv[1], comp_file, fsize);
	clock_t end = clock();
	printf("Time : %lf\n", (double)(end - start)/CLOCKS_PER_SEC);
	return 0;
}

