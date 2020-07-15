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
	long long max = 4000000;
	int fsize, ret;
	gzFile gzfp;
	char buf[max];
	char copy_file[128] = "copy_"; 
	char file[123];
	int n = strlen(argv[1]);
	strncpy(file, argv[1]+5, n-5); 
	strcat(copy_file, file);	
	copy_file[n] = 0;


	/* check arguments */
	if (argc != 2) {
		printf ("argument Error\n");
		exit (-1);
	}

	if ((gzfp = gzopen(argv[1], "rb")) == NULL) {
		printf("Error_gz\n");
		return 0;
	}
	fsize = gzread(gzfp, buf, max);
		

	int fd;
	fd = open(copy_file, O_RDWR|O_CREAT, 0666);
	write(fd, buf, fsize);

	close(fd);
	gzclose(gzfp);
	printf("%s is decompressed to %s (%d bytes)\n", argv[1], copy_file, fsize);
	clock_t end = clock();
	printf("Time : %lf\n", (double)(end - start)/CLOCKS_PER_SEC);
	return 0;
}

