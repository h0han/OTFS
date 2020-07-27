#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "bitmap.h"

typedef struct File_Block{
	char data[4096];
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
	int itable_size; // 512 * 8 * 1024 byte
	long dtable_size; // 4kb * 128 * 8 * 1024
	long datastart;


	int last_ibit;
	long last_dbit;

	int num_file;
	int allocated_block;
	
	int ig[246]; // For 1024 bytes
} superblock;

typedef struct Inode_bitmap { // size : 1024
	unsigned char ibit[1024];
} ibitmap;

typedef struct Data_bitmap { // size : 128 * 1024
	unsigned char dbit[131072]; // 131072 = 128 * 1024
} dbitmap;

typedef struct Inode { // size : 128 * 4 = 512b 
	char* filename; // does it need malloc?
	long size;
	int inode_num;
	int data_num[12];
	int file_or_dir; // file == 0, dir == 1;
	File_Block* FB[12];
	Dir_Block* DB[12]; // DB needs to have 12 Dir_Block so it needs malloc(12 * 4kb)
} inode;

int count(const char *c, char x) {
        int i, count;
	count=0;
	for(i=0;i < strlen(c)+1;i++){
  	        if(c[i]==x) count++;
	        else continue;
	}
	return count;
}

int otfind(const char* path) { // return -1 means "There is no file"
	// get file	
	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}
	int size = 1024;

	superblock* super_block;
	inode* inode_table;
	super_block = malloc(sizeof(super_block));
	inode_table = malloc(8 * 1024 * sizeof(inode));

	lseek(fd, 0, SEEK_SET);
	read(fd, super_block, 1024); // superblock load

	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(fd, itable_location, SEEK_SET);
	read(fd, inode_table, super_block->itable_size); // itable load
	
	long dtable_location = itable_location + super_block->itable_size;

	int inode_num; // this is return value
	int dbit_num;
	int root_check = 0;
	int exist_check = 0;
	char temp[100]; // used strtok
	strcpy(temp, path); 
	char *ptr; // used strtok
	int i; // used for loop
	int j; // used for loop 
	bool loop_escape;
	Dir_Block* Dir;
	
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
			loop_escape = true;
			dbit_num = fileinode.data_num[i];
			pread(fd, (char*) Dir, 4096, dtable_location + dbit_num); // root_dbitnum is offset
			for (j = 0; j < 128; j++) {
				if ((strcmp((Dir->name_list[j]), ptr)) == 0) {
					inode_num = Dir->inode_num[j];
					exist_check++;
					loop_escape = false;
					break;
				}
			}
			if (loop_escape == false) {
			       break;
			}
		}
		root_check++;
		ptr = strtok(NULL, "/");
	} while(ptr != NULL);
	if (exist_check != root_check) { // If there is not file
		return -1;
	}
	close(fd);
	free(super_block);
	free(inode_table);
	return inode_num;
}


char* paren(const char* path){
        char pfp[28];
        strcpy(pfp, path);
        char* ptr;
        char* old[30];
        ptr = strtok(pfp, "/");
        int i = 0;
        char* last;
        while(ptr!=NULL){
                old[i] = ptr;
                i++; 
                last = ptr;
                ptr= strtok(NULL, "/");
        }
        int j = 0;
        char* new;
        strcat(new, "/");
        while((strcmp(old[j], last))){
                strcat(new, old[j]);
                strcat(new, "/");
                j++;
        }
        return new;
}

void *ot_init()
{
	int size = 1024; 
	
	// make a new file to store Filesystem data
	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}

	// set and malloc structure
	superblock* super_block;
	ibitmap* inode_bitmap;
	dbitmap* data_bitmap;
	inode* inode_table;
	
	super_block = malloc(sizeof(superblock));
	super_block->sb_size = size;
	super_block->ibitmap_size = size;
	super_block->dbitmap_size = 128 * size;
	super_block->itable_size = 4 * size * size;
	super_block->dtable_size = 4 * size * 128 * 8 * size;   
	
	inode_bitmap = malloc(super_block->ibitmap_size);
	data_bitmap = malloc(super_block->dbitmap_size);	
	inode_table = malloc(super_block->itable_size);

	// add root directory
	inode root;
    
	// need inode bitmap alloc
	int root_ibitnum = checkb(fd, 0); // maybe root_ibitnum = 0
	printf("%d\n", root_ibitnum);
	chanb(fd, root_ibitnum, 0);
	printf("%d\n", checkb(fd, 0));
	printf("%d\n", checkb(fd, 0));

	// need data bitmap alloc
	int root_dbitnum = checkb(fd, 1);
	chanb(fd, root_dbitnum, 1);

	// set root inode
	root.filename = malloc(28);	
	strcpy(root.filename, "/");
	root.size = 4096; // at first, root's size = 4kb (1 Datablock)
	root.inode_num = root_ibitnum;
	root.data_num[0] = root_dbitnum;
	root.file_or_dir = 1;
	inode_table[root_ibitnum] = root;
	
	// set root directory Block	
	Dir_Block* root_dir = malloc(4096);
	strcpy(root_dir->name_list[0], ".");
	root_dir->inode_num[0] = root_ibitnum;

	// write in region file	

	lseek(fd, 0, SEEK_SET);
	write(fd, super_block, super_block->sb_size);
	write(fd, inode_bitmap, super_block->ibitmap_size);
	write(fd, data_bitmap, super_block->dbitmap_size);
	write(fd, inode_table, super_block->itable_size);
	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	long dtable_location = itable_location + super_block->itable_size;
	pwrite(fd, (char*) root_dir, 4096, dtable_location + root_dbitnum); // root_dbitnum is offset

	// close and free
	close(fd);
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);
	free(root_dir);
	printf("Region file is created\n");
	return 0;
}

static int ot_mkdir(const char *path, mode_t mode) {
	// Check if there is same name directory -> return -EEXIST error
	
	if (otfind(path) != -1) { // there is same file in path
		return -1; // Actually, return value is -EEXIST
	}
	//inode bitmap, data bitmap add
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
	inode_bitmap = malloc(super_block->ibitmap_size);
	data_bitmap = malloc(super_block->dbitmap_size);	
	inode_table = malloc(super_block->itable_size);
	
	lseek(fd, 0, SEEK_SET);
	read(fd, super_block, 1024); // superblock load
	read(fd, inode_bitmap, super_block->ibitmap_size);
	read(fd, data_bitmap, super_block->dbitmap_size);
	read(fd, inode_table, super_block->itable_size);
	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	long dtable_location = itable_location + super_block->itable_size;
	
	int num_file = count(path, '/'); // the last char of path must '/' such as "/a/b/c/".

	// set new inode
	inode new;
    
	// need inode bitmap alloc
	int new_ibitnum = checkb(fd, 0); // maybe root_ibitnum = 0
	chanb(fd, new_ibitnum, 0);

	// need data bitmap alloc
	int new_dbitnum = checkb(fd, 1);
	chanb(fd, new_dbitnum, 1);

	// set new inode
	new.filename = malloc(28);	
	new.size = 4096; // at first, root's size = 4kb (1 Datablock)
	new.inode_num = new_ibitnum;
	new.data_num[0] = new_dbitnum;
	new.file_or_dir = 1;
	inode_table[new_ibitnum] = new;
	
	// set new directory Block	
	Dir_Block* new_dir = malloc(4096);
	strcpy(new_dir->name_list[0], ".");
	new_dir->inode_num[0] = new_ibitnum;
		
	// write in region file	
	int inode_num; 
	int dbit_num;
	int input_num;
	int root_check = 0;
	char temp[100]; // used strtok
	strcpy(temp, path); 
	char *ptr; // used strtok
	int h;	
	int i; // used for loop
	int j; // used for loop 
	int k;
	bool loop_escape;
	Dir_Block* Dir = (Dir_Block*) malloc (4096);
	
	inode fileinode;
	char name[28];
	// find dir with path	
	for (h = 0; h < num_file; h++){
		ptr = strtok(temp, "/");
		if (root_check == 0) {
			//access to root
			inode_num = 0;
			fileinode = inode_table[0];
		}
		else {
			fileinode = inode_table[inode_num];
		}
		for (i = 0; i <= (fileinode.size / 4096); i++) { // Set Dir directroy
			loop_escape = true;
			dbit_num = fileinode.data_num[i];
			pread(fd, (char*) Dir, 4096, dtable_location + dbit_num); // dbit_num is offset
			if (h == num_file - 1) {
				strcpy(new.filename, ptr); 
				strcpy(new_dir->name_list[1], "..");
				new_dir->inode_num[1] = inode_num;
				// we need to change parent inode's data
				for (k = 0; k < 128; k++) { // finding last empty number in inode_num array of Dir
					if (root_check == 0 && k == 0 && i == 0) {
						continue;
					}
					if (Dir->inode_num[k] == 0) {
					        input_num = k;
						break;
					}
				} // if k > 128, we need to add new Data block?
			        strcpy(Dir->name_list[input_num], ptr);	
				Dir->inode_num[input_num] = new.inode_num;
				lseek(fd, 0, SEEK_SET);
				pwrite(fd, (char*) Dir, 4096, dtable_location + dbit_num); 

				// we need to add new inode in data_table
				lseek(fd, 0, SEEK_SET);
				pwrite(fd, (char*) new_dir, 4096, dtable_location + new_dbitnum);
			    	// fileinode.ctime = time(NULL);
				// fileinode.mtime = time(NULL);				
				// fileinode.atime = time(NULL); 
				loop_escape = false;
				break;
			}
			for (j = 0; j < 128; j++) {  // find ptr directory in Dir
				if ((strcmp((Dir->name_list[j]), ptr)) == 0) {
					inode_num = Dir->inode_num[j];
					// fileinode.ctime = time(NULL);
					// fileinode.mtime = time(NULL);			
					// fileinode.atime = time(NULL); 
					loop_escape = false;
					break;
				}
			}
			if (loop_escape == false) {
			       break;
			}
		}
		root_check++;
		ptr = strtok(NULL, "/");
	}
	

	close(fd);
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);
	free(new_dir);	
	free(Dir);	
}

static int ot_getattr(const char *path, struct stat *buf) {
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
/*
static int ot_rmdir(const char* path){

        int inum = otfind(path);
        inode ino;
        chanb(inum,0);
        int fd = open("region.c", O_RDWR|O_CREAT, 0666);
        lseek(fd, 2048+1024*128, SEEK_SET);
        lseek(fd, inum, SEEK_CUR);
        read(fd, &ino, 512);
        //path is directory

        for(int i = 0; i<12;i){
                if(ino.DB[i] != NULL){
                return -1; // error: there are files in dirctory
                }
        }
        for(int j = 0; j<12;j+++){
               chanb(ino.data_num[i], 1);
        }

        lseek(fd, 512, SEEK_CUR);
        write(fd, &ino, 512);

        //touch parents's data
        inode* pino;
        int pinum = otfind(paren(path));
        lseek(fd, 2048+1028*128, SEEK_SET);
        lseek(fd, pinum, SEEK_CUR);
        read(fd, pino, 512);
        for (int i = 0; i <= (pino->size / 4096); i++) {
                for (int j = 0; j < 128; j++) {
                        if (((pino->DB[i])->inode_num[j])== inum) {
                                strcpy((pino->DB[i])->name_list[j],"");
                                (pino->DB[i])->inode_num[j] = NULL;
                        }
                }
        }
        close(fd);
}
*/
static int ot_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	int dir_inodenum;    
	if ((dir_inodenum = otfind(path)) == -1) { // there is same file in path
		return -1; // Actually, there is no file with name path
	}
	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}
	int size = 1024;	
	//if(strcmp(path,"/") != 0 ) {
	//	return -1;
	//}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	superblock* super_block;
	inode* inode_table;	
	ibitmap* inode_bitmap;
	
	super_block = malloc(sizeof(superblock));
//	inode_bitmap = malloc(sizeof(ibitmap));
	inode_table = malloc(super_block->itable_size);
	
	lseek(fd, 0, SEEK_SET);
	read(fd, super_block, 1024); // superblock load
//	read(fd, inode_bitmap, 1024); // ibitmap load
	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(fd, itable_location, SEEK_SET);
	read(fd, inode_table, super_block->itable_size);
	long dtable_location = itable_location + super_block->itable_size;

	inode dir = inode_table[dir_inodenum];
	inode temp;
	int dbit_num;
	int k;
	Dir_Block* Dir = (Dir_Block*) malloc (4096);
	for (int i = 0; i <= (dir.size / 4096); i++) {
		dbit_num = dir.data_num[i];
		pread(fd, (char*) Dir, 4096, dtable_location + dbit_num); // dbit_num is offset
		if (i == 0) {
			for (k = 2; k < 128; k++) {  // k = 0 -> ".", k = 1 -> ".."
			        if (Dir->inode_num[k] == 0) { // we need to think about root_num
					break;
				}
				temp = inode_table[Dir->inode_num[k]];
				filler(buf, temp.filename, NULL, 0);
			}
		}
		else {
			for (k = 0; k < 128; k++) { 
			        if (Dir->inode_num[k] == 0) { // we need to think about root_num
					break;
				}
				temp = inode_table[Dir->inode_num[k]];
				filler(buf, temp.filename, NULL, 0);
			}
		}
		lseek(fd, 0, SEEK_SET);
		
	}

	close(fd);
	free(super_block);
	//free(inode_bitmap);
	free(inode_table);
	free(Dir);	
}

static struct fuse_operations ot_oper = {
	//.init = ot_init,
	.getattr = ot_getattr,
	//.access = ot_access,
	//.readdir = ot_readdir,
	.mkdir = ot_mkdir,
	//.unlink = ot_unlink,
	//.rmdir = ot_rmdir,
	//.rename = ot_rename,
	//.truncate = ot_truncate, 	
	//.open = ot_open,
	//.read = ot_read,
	//.write = ot_write,
	//.release = ot_release,
	//.releasedir = ot_releasedir,
	//.destory = ot_destory,
	//.opendir = ot_opendir,
	//.create = ot_create,
	//.utimens = ot_utimens,
};

int main(){
	ot_init();

}
