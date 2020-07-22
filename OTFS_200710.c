#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct File_Block{
} File_Block;

typedef struct Dir_Block{
	char name_list[128][28]; // In map, there are 128 files;
	int inode_num[128];
} Dir_Block; //sizeof(Dir_Block) == 4KB

typedef struct Super_block { // size : 256 * 4 = 1024
// block count?
	//void *sb_start;
	//void *ibitmap_start;
	//void *dbitmap_start;
	//void *itable_start;
	//void *hash_start;
	//void *darea_start;
	
	int sb_size; // 1024 byte
	int ibitmap_size; // 1024 byte
	int dbitmap_size; // 128 * 1024 byte
	int itable_size; // 4 * 1024 * 1024 byte
	long datastart;


	int last_ibit;
	long last_dbit;

	int num_file;
	int allocated_block;
	
	int ig[247]; // For 1024 bytes
} superblock;

typedef struct Inode_bitmap { // size : 1024
	unsigned char ibit[1024];
} ibitmap;

typedef struct Data_bitmap { // size : 1024
	unsigned char dbit[1024];
} dbitmap;

typedef struct Inode { // size : 128 * 4 = 512b 
	char* filename; // does it need malloc?
	long size;
	int inode_num;
	int file_or_dir; // file == 0, dir == 1;
	File_Block* FB[12];
	Dir_Block* DB[12]; // DB needs to have 12 Dir_Block so it needs malloc(12 * 4kb)
} inode;

int otfind(const char* path) { // return -1 means error
	// get file	
	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}
	int size = 1024;

	superblock* super_block;
	super_block = malloc(sizeof(super_block));
	inode* inode_table;
	inode_table = malloc(8 * 1024 * sizeof(inode));

	lseek(fd, 0, SEEK_SET);
	read(fd, super_block, 1024); // superblock load

	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(fd, itable_location, SEEK_SET);
	read(fd, inode_table, 8 * 1024 * sizeof(inode)); // itable load
	
	int inode_num; // this is return value
	int root_check = 0;
	char temp[100]; // used strtok
	strcpy(temp, path); 
	char *ptr; // used strtok
	int i; // used for loop
	int j; // used for loop 
	int num_DB;

	inode fileinode;
	char name[28];
	// find dir with path	
	do {
		ptr = strtok(temp, "/");
		if (root_check == 0) {
			//access to root
			fileinode = inode_table[0];
		}
		else {
			fileinode = inode_table[inode_num];
		}
		for (i = 0; i <= (fileinode.size / 4096); i++) { 
			for (j = 0; j < 128; j++) {
				if ((strcmp((fileinode.DB[i]->name_list[j]), ptr)) == 0) {
					inode_num = (fileinode.DB[i])->inode_num[j];
				}
			}
		}
		root_check++;
		ptr = strtok(NULL, "/");
	} while(ptr != NULL);

	close(fd);
	free(super_block);
	free(inode_table);		
	return inode_num;
}

int init()
{
	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}
	int size = 1024;
	
	superblock* super_block;
	ibitmap* inode_bitmap;
	dbitmap* data_bitmap;
	inode* inode_table;
	
	super_block = malloc(sizeof(superblock));
	super_block->sb_size = size;
	super_block->ibitmap_size = size;
	super_block->dbitmap_size = 128 * size;
	super_block->itable_size = 4 * size * size;
	write(fd, super_block, super_block->sb_size);
	
	inode_bitmap = malloc(sizeof(ibitmap));
	write(fd, inode_bitmap, super_block->ibitmap_size);

	data_bitmap = malloc(128 * sizeof(dbitmap));	
	write(fd, data_bitmap, super_block->dbitmap_size);

	inode_table = malloc(8 * 1024 * sizeof(inode));
	write(fd, inode_table, super_block->itable_size);

	inode root;
	root.filename = malloc(28);
	root.size = 4096; // at first, root's size = 4kb (1 Datablock)
	strcpy(root.filename, "/");
	root.inode_num = 0;
	root.file_or_dir = 1;
	inode_table[0] = root;
	// root information;
	// root inode num = 2;
	close(fd);
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);
	printf("Region file is created\n");
	return 0;
}

int getattr(const char *path, struct stat *buf) {
	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}
	int size = 1024;
	int res = 0;

	superblock* super_block;
	super_block = malloc(sizeof(super_block));
	ibitmap* inode_bitmap;
	inode_bitmap = malloc(sizeof(ibitmap));
	dbitmap* data_bitmap;
	data_bitmap = malloc(128 * sizeof(dbitmap));
	inode* inode_table;
	inode_table = malloc(8 * 1024 * sizeof(inode));

	lseek(fd, 0, SEEK_SET);
	read(fd, super_block, 1024); // superblock load

	lseek(fd, 1024, SEEK_SET);
	read(fd, inode_bitmap, 1024); // ibitmap load

	lseek(fd, 2048, SEEK_SET);
	read(fd, data_bitmap, 128 * sizeof(dbitmap)); // dbitmap load

	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(fd, itable_location, SEEK_SET);
	read(fd, inode_table, 8 * 1024 * sizeof(inode)); // itable load

	char *temp;
	strcpy(temp, path);

	if (strcmp(path, "/") == 0) {
		inode root = inode_table[0];
		buf->st_ino = root.inode_num;
		buf->st_mode = S_IFDIR |0755;
		buf->st_nlink = 1;
		//buf->st_uid;
		//buf->st_gid;
		buf->st_size = root.size;
		//buf->st_blocks = ;
		buf->st_atime = time(NULL);
		buf->st_ctime = time(NULL);
		buf->st_mtime = time(NULL);;
	}
	else { 
		int temp_inodenum;
		if ((temp_inodenum = otfind(path)) == -1) { // get inode from path
			printf("There is no file\n");
		}
		else {
			inode temp = inode_table[temp_inodenum];
			buf->st_ino = temp.inode_num;
			buf->st_mode = S_IFDIR |0755;
			buf->st_nlink = 1;
			//buf->st_uid;
			//buf->st_gid;
			buf->st_size = temp.size;
			// buf->st_blocks = ;
			buf->st_atime = time(NULL);
			buf->st_ctime = time(NULL);
			buf->st_mtime = time(NULL);
		}
	}
	close(fd);
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);		
}

int main(){
	init();
}