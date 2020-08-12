12345
/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * minimal example filesystem using high-level API
 *
 * Compile with:
 *
 *     gcc -Wall hello.c bitmap.c `pkg-config fuse3 --cflags --libs` -o hello -g
 *
 * ## Source code ##
 * \include hello.c
 */

// 0804 : readdir, getattr, init is okay, we can do cd otfs, ls
// mkdir says "There is no file" and makes Input/Output Error

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <sys/stat.h>
#include "bitmap.h"

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
	const char *filename;
	const char *contents;
	int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--name=%s", filename),
	OPTION("--contents=%s", contents),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};


int _g_fd;

typedef struct Dir_Block{
	char name_list[128][28]; // In map, there are 128 files;
	int inode_num[128];
} Dir_Block; //sizeof(Dir_Block) == 4KB

typedef struct Super_block { // size : 256 * 4 = 1024
// block count?
	int sb_size; // 1024 byte
	int ibitmap_size; // 1024 byte
	int dbitmap_size; // 128 * 1024 byte
	int itable_size; // 512 * 8 * 1024 byte
	long long dtable_size; // 4kb * 128 * 8 * 1024
	
	int ig[250]; // For 1024 bytes
} superblock;
/*
typedef struct Inode_bitmap { // size : 1024
	unsigned char ibit[1024];
} ibitmap;

typedef struct Data_bitmap { // size : 128 * 1024
	unsigned char dbit[131072]; // 131072 = 128 * 1024
} dbitmap;
*/
typedef struct Inode { // size : 128 * 4 = 512b 
	char* filename; // does it need malloc?
	long size;
	int inode_num;
	int data_num[12];
	int file_or_dir; // file == 0, dir == 1;
	Dir_Block* DB[12];
	time_t atime; //access time
	time_t ctime; //change time, time for changing about file metadata
	time_t mtime; //modify time, time for changing about file data such as contents of file
	mode_t mode;	
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
	if ((strcmp(path, "/")) == 0) {
		return 0;
	}
	printf("@@@@@@@ otfind start with path : %s\n", path);
	//int size = 1024;

	superblock* super_block;
	inode* inode_table;
	super_block = malloc(1024); //1024
	inode_table = malloc(8 * 1024 * 512); // 512

	lseek(_g_fd, 0, SEEK_SET);
	read(_g_fd, super_block, 1024); // superblock load

	long itable_location = 1024 + 1024 + 128 * 1024; //super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(_g_fd, itable_location, SEEK_SET);
	read(_g_fd, inode_table, 8 * 1024 * 512); // itable load
	
	long dtable_location = itable_location + 8 * 1024 * 512;

	int inode_num; // this is return value
	int dbit_num;
	int root_check = 0;
	int exist_check = 0;
	char temp[100]; // used strtok
	strcpy(temp, path); 
	char *ptr; // used strtok
	int i; // used for loop
	int j; // used for loop 
	int data_loop_num;
	bool loop_escape;
	Dir_Block* Dir = (Dir_Block*)malloc(4096);
	
	inode fileinode;
	//char name[28];
	// find dir with path	
	ptr = strtok(temp, "/"); // if path is /a/b/, ptr is a

	do {
		if (root_check == 0) {
			//access to root
			inode_num = 0;
			fileinode = inode_table[0];
		}
		else {
			fileinode = inode_table[inode_num];
		}	
		if ((fileinode.size % 4096) == 0) {
			data_loop_num = fileinode.size / 4096;
		}
		else {
			data_loop_num = (fileinode.size / 4096) + 1;
		}
		for (i = 0; i < data_loop_num; i++) { 
			loop_escape = true;
			dbit_num = fileinode.data_num[i];
			pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // root_dbitnum is offset
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
		printf("ptr : %s\n", ptr);
		ptr = strtok(NULL, "/");
	} while(ptr != NULL);
	if (exist_check != root_check) { // If there is not file
		printf("@@@@@@@ otfind complete, there is no file@@@@@@\n");
		return -1;
	}
	free(super_block);
	free(inode_table);
	printf("@@@@@@@ otfind complete@@@@@@\n");
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
        char* new = (char*)malloc(100);
        strcat(new, "/");
        while((strcmp(old[j], last))){
                strcat(new, old[j]);
                strcat(new, "/");
                j++;
        }
        return new;
}


static void *ot_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	int size = 1024; 
	
	// make a new file to store Filesystem data
	char region[10] = "region";
	if ((_g_fd = open(region, O_RDWR|O_CREAT, 0666)) < 0){
		printf("Error\n");
		return 0;
	}

	// set and malloc structure
	superblock* super_block;
	unsigned char* inode_bitmap;
	unsigned char* data_bitmap;
	inode* inode_table;
	
	super_block = malloc(1024);
	super_block->sb_size = 1024;
	super_block->ibitmap_size = 1024;
	super_block->dbitmap_size = 128 * 1024;
	super_block->itable_size = 8 * 1024 * 512;
	super_block->dtable_size = 4 * size* 128 * 8 * size;   
	
	inode_bitmap = malloc(1024);
	data_bitmap = malloc(128 * 1024);
	inode_table = malloc(8 * 1024 * 512);

	lseek(_g_fd, 0, SEEK_SET);
	write(_g_fd, super_block, 1024);
	write(_g_fd, inode_bitmap, 1024);
	write(_g_fd, data_bitmap, 128 * 1024);
	// add root directory
	inode root;
    
	// need inode bitmap alloc
	int root_ibitnum = checkb(_g_fd, 0); // maybe root_ibitnum = 0
	chanb(_g_fd, root_ibitnum, 0);
	printf("root_ibitnum : %d\n", root_ibitnum);
	printf("newcheckb : %d\n", checkb(_g_fd, 0));
	// need data bitmap alloc
	int root_dbitnum = checkb(_g_fd, 1);
	chanb(_g_fd, root_dbitnum, 1);
	printf("root_dbitnum : %d\n", root_dbitnum);
	printf("newcheckb : %d\n", checkb(_g_fd, 1));

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

	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(_g_fd, itable_location, SEEK_SET);
	write(_g_fd, inode_table, 8 * 1024 * 512);
	long dtable_location = itable_location + super_block->itable_size;
	pwrite(_g_fd, (char*) root_dir, 4096, dtable_location + root_dbitnum * 4096); // root_dbitnum is offset
    

	// close and free
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);
	free(root_dir);
	printf("@@@@@@@INIT complete @@@@@@@\n");
	printf("@@@@@@@Region file is created@@@@@@@\n");
	return 0;


/*
	(void) conn;
	cfg->kernel_cache = 1;
	return NULL;
*/
}


static int ot_mkdir(const char *path, mode_t mode) {
	// Check if there is same name directory -> return -EEXIST error

	printf("@@@@@@@mkdir start\n");
	if (otfind(path) != -1) { // there is same file in path
		printf("@@@@@mkdir EEXIST error\n");
		fflush(stdout);
		return -EEXIST; // Actually, return value is -EEXIST
	}
	//inode bitmap, data bitmap add

	time_t cur = time(NULL);

	superblock* super_block;
	unsigned char* inode_bitmap;
	unsigned char* data_bitmap;
	inode* inode_table;
	
	super_block = malloc(1024);
	inode_bitmap = malloc(1024);
	data_bitmap = malloc(1024 * 128);	
	inode_table = malloc(8 * 1024 * 512);
	
	lseek(_g_fd, 0, SEEK_SET);
	read(_g_fd, super_block, 1024); // superblock load
	//printf("super_block size : %d\nibitmap : %d\ndbit : %d\nitable : %d\n", sizeof(superblock), super_block->ibitmap_size, super_block->dbitmap_size, super_block->itable_size);
	read(_g_fd, inode_bitmap, 1024);
	read(_g_fd, data_bitmap, 128 * 1024);
	read(_g_fd, inode_table, 8 * 1024 * 512);
	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	long dtable_location = itable_location + super_block->itable_size;
	
	int num_file = count(path, '/'); // the last char of path must '/' such as "/a/b/c".
	// "/a/b/c" -> num_file = 3
	printf("numfile : %d\n", num_file);

	inode root = inode_table[0];
	printf("mkdir test, root : %s\n", root.filename);


	// set new inode
	inode new;
    
	// need inode bitmap alloc
	int new_ibitnum = checkb(_g_fd, 0); // maybe root_ibitnum = 0
	chanb(_g_fd, new_ibitnum, 0);
	printf("new_ibitnum : %d(it will be 1)\n", new_ibitnum);
	printf("onoffcheck : %d\n", onoffcheck(_g_fd, new_ibitnum,0)); 

	// need data bitmap alloc
	int new_dbitnum = checkb(_g_fd, 1);
	chanb(_g_fd, new_dbitnum, 1);
	printf("new_ibitnum : %d(it will be 1)\n", new_dbitnum);
	printf("onoffcheck : %d\n", onoffcheck(_g_fd, new_dbitnum,0)); 
	
	// set new inode
	new.filename = malloc(28);	
	new.size = 4096; // at first, root's size = 4kb (1 Datablock)
	new.inode_num = new_ibitnum;
	for (int a = 0; a < 12; a++) { 
		new.data_num[a] = 0;
	}
	new.data_num[0] = new_dbitnum;
	new.file_or_dir = 1;
	new.ctime = cur;
	new.mtime = cur;
	new.atime = cur;
	new.mode = mode;

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
	int data_loop_num; // real num is data_loop_num + 1
	bool loop_escape;
	Dir_Block* Dir = (Dir_Block*) malloc (4096); //parent dir
	
	inode fileinode;
	// find dir with path	
	printf("temp : %s\n", temp);
	ptr = strtok(temp, "/");
	for (h = 0; h < num_file; h++){
		printf("ptr : %s\n", ptr);	
		// fileinode is parent inode
		if (root_check == 0) {
			//access to root
			inode_num = 0;
			fileinode = inode_table[0];
		}
		else {
			fileinode = inode_table[inode_num];
		}
		//we need to modify below loop, because < or <=
		if ((fileinode.size % 4096) == 0) {
			data_loop_num = fileinode.size / 4096;
		}
		else {
			data_loop_num = (fileinode.size / 4096) + 1;
		}
		for (i = 0; i < data_loop_num; i++) { // Set Dir directroy
			loop_escape = true;
			dbit_num = fileinode.data_num[i];
			pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
			if (h == num_file - 1) {
				strcpy(new.filename, ptr); 
				strcpy(new_dir->name_list[1], "..");
				new_dir->inode_num[1] = inode_num;
				// we need to change parent inode's data
				for (k = 0; k < 128; k++) { // finding last empty number in inode_num array of Dir
					
					if ((strcmp(Dir->name_list[k], ".")) == 0) {
					       continue;
					}
					if ((strcmp(Dir->name_list[k], "..")) == 0) {
						continue;
					}
					if (Dir->inode_num[k] == 0) {
					        input_num = k;
						break;
					}
				} // if k > 128, we need to add new Data block?
			        strcpy(Dir->name_list[input_num], new.filename);	
				Dir->inode_num[input_num] = new.inode_num;
				pwrite(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); //write parent Dir
				// we need to add new inode in data_table
				pwrite(_g_fd, (char*) new_dir, 4096, dtable_location + new_dbitnum * 4096);
			    	fileinode.ctime = cur;
				fileinode.mtime = cur;				
				fileinode.atime = cur; 
				inode_table[inode_num] = fileinode;
				loop_escape = false;
				break;
			}
			for (j = 0; j < 128; j++) {  // find ptr directory in Dir
				if ((strcmp((Dir->name_list[j]), ptr)) == 0) {
					inode_num = Dir->inode_num[j];
					fileinode.atime = cur; 
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
	inode_table[new_ibitnum] = new;
       	lseek(_g_fd, 0, SEEK_SET);
        write(_g_fd, super_block, 1024);
	lseek(_g_fd, itable_location, SEEK_SET);
        write(_g_fd, inode_table, 8 * 1024 * 512);

	
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);
	free(new_dir);	
	free(Dir);
	printf("@@@@@@@mkdir complete @@@@@@@\n");
	return 0;
}
static int ot_rmdir(const char* path){

        int inum = otfind(path);
        inode ino;

        chanb(_g_fd, inum, 0);
        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        lseek(_g_fd, inum, SEEK_CUR);
        read(_g_fd, &ino, 512);
        //path is directory

        for(int i = 0; i<12;i++){
                if(ino.DB[i] != NULL){
                return -1; // error: there are files in dirctory
                }
        }
        for(int j = 0; j<12;j++){
                chanb(_g_fd, ino.data_num[j], 1);
        }

        lseek(_g_fd, 512, SEEK_CUR);
        write(_g_fd, &ino, 512);

        //touch parents's data
        inode pino;
        int pinum = otfind(paren(path));
        lseek(_g_fd, 2048+1028*128, SEEK_SET);
        lseek(_g_fd, pinum, SEEK_CUR);
        read(_g_fd, &pino, 512);
        for (int i = 0; i <= (pino.size / 4096); i++) {
                for (int j = 0; j < 128; j++) {
                        if (((pino.DB[i])->inode_num[j])== inum) {
                                strcpy((pino.DB[i])->name_list[j],"");
                                (pino.DB[i])->inode_num[j] = 0;
                        }
                }
        }
        return 0;
}
static int ot_getattr(const char *path, struct stat *buf,
			 struct fuse_file_info *fi)
{
	printf("@@@@@@@ getattr start with path : %s\n", path);	
	int nofile_check = 0;

	superblock* super_block;
	super_block = malloc(1024);
	
	inode* inode_table;
	inode_table = malloc(8 * 1024 * 512);
	
	lseek(_g_fd, 0, SEEK_SET);
	read(_g_fd, super_block, 1024); // superblock load

	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(_g_fd, itable_location, SEEK_SET);
	read(_g_fd, inode_table, 8 * 1024 * 512); // itable load

	if (strcmp(path, "/") == 0) {
		inode root = inode_table[0];
		printf("getattr test, root : %s\n", root.filename);
		buf->st_ino = root.inode_num;
		buf->st_mode = S_IFDIR |0755;
		buf->st_nlink = 1;
		//buf->st_uid;
		//buf->st_gid;
		buf->st_size = root.size;
		//buf->st_blocks = ;
		buf->st_atime = root.atime;
		buf->st_ctime = root.ctime;
		buf->st_mtime = root.mtime;
	}
	else { 
		int temp_inodenum;
		if ((temp_inodenum = otfind(path)) == -1) { // get inode from path
			printf("There is no file\n");
			nofile_check = 1;
			//		buf->st_mode = S_IFDIR |0755;
	//		buf->st_nlink = 1;
			//buf->st_uid;
			//buf->st_gid;
	//		buf->st_size = 4096;
			// buf->st_blocks = ;
		}
		else {
			inode temp = inode_table[temp_inodenum];
			buf->st_ino = temp.inode_num;
			
			if (temp.file_or_dir == 1) { 
				buf->st_mode = S_IFDIR | 0755;
			}
			else {
				buf->st_mode = S_IFREG | 0444;
			}
			
			//buf->st_mode = temp.mode;
			buf->st_nlink = 1;
			//buf->st_uid;
			//buf->st_gid;
			buf->st_size = temp.size;
			// buf->st_blocks = ;
			buf->st_atime = temp.atime;
			buf->st_ctime = temp.ctime;
			buf->st_mtime = temp.mtime;
		}
	}
	free(super_block);
	free(inode_table);		
	printf("@@@@@@@getattr complete @@@@@@@\n");
	if (nofile_check == 1) return -ENOENT;
	return 0;
}

static int ot_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	/*
    	(void) offset;
	(void) fi;
	(void) flags;
*/
	printf("@@@@@@@readdir start with path : %s\n", path);
/*
	fflush(stdout);
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	*/

	int dir_inodenum;    
	if ((dir_inodenum = otfind(path)) == -1) { // there is same file in path
		return -1; // Actually, there is no file with name path
	}
	//if(strcmp(path,"/") != 0 ) {
	//	return -1;
	//}
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	superblock* super_block;
	inode* inode_table;	
	
	super_block = malloc(1024);
//	inode_bitmap = malloc(sizeof(ibitmap));
	inode_table = malloc(8 * 1024 * 512);
	
	lseek(_g_fd, 0, SEEK_SET);
	read(_g_fd, super_block, 1024); // superblock load
//	read(fd, inode_bitmap, 1024); // ibitmap load
	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(_g_fd, itable_location, SEEK_SET);
	read(_g_fd, inode_table, 8 * 1024 * 512);
	long dtable_location = itable_location + super_block->itable_size;

	inode dir_inode = inode_table[dir_inodenum];
	inode temp;
	int dbit_num;
	int k;
	Dir_Block* Dir = (Dir_Block*) malloc (4096);
	int data_loop_num;
	if ((dir_inode.size % 4096) == 0) {
		data_loop_num = dir_inode.size / 4096;
	}
    	else {
		data_loop_num = (dir_inode.size / 4096) + 1;
	}
	for (int i = 0; i < data_loop_num; i++) {
		dbit_num = dir_inode.data_num[i];
		pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
		printf("Dir->name_list[0]: %s\n", Dir->name_list[0]);
		printf("Dir->inode_num[0] : %d\n", Dir->inode_num[0]);
		printf("Dir->name_list[1]: %s\n", Dir->name_list[1]);
		printf("Dir->inode_num[1] : %d\n", Dir->inode_num[1]);
		printf("Dir->name_list[2]: %s\n", Dir->name_list[2]);
		printf("Dir->inode_num[2] : %d\n", Dir->inode_num[2]);
		for (k = 0; k < 128; k++) {  // k = 0 -> ".", k = 1 -> ".."
			if ((strcmp(Dir->name_list[k], ".")) == 0) {
				continue;
			}
			if ((strcmp(Dir->name_list[k], "..")) == 0) {
				continue;
			}
		        if (Dir->inode_num[k] == 0) { // we need to think about root_num
				continue;
			}
			temp = inode_table[Dir->inode_num[k]];
		//	printf("k : %d\n", k);
		//	printf("temp.inode_num : %d\n", temp.inode_num);
		//	printf("temp.filename : %s\n", temp.filename);
			filler(buf, temp.filename, NULL, 0, 0);
		}
	}
	free(super_block);
	//free(inode_bitmap);
	free(inode_table);
	free(Dir);
	printf("@@@@@@@readdir complete @@@@@@@\n");
	return 0;
	
}

static int ot_open(const char *path, struct fuse_file_info *fi)
{	
	printf("@@@@@@@ open start @@@@@@@\n");
/*
	if (strcmp(path+1, options.filename) != 0)
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;
*/
        int inode_num;
        if ((inode_num = otfind(path)) == -1) { //it means there is no file on the path
                return -1; 
        }
	fi->fh = inode_num;
/*
        superblock* super_block;
        inode* inode_table;

        super_block = malloc(1024);
        inode_table = malloc(8 * 1024 * 512);

        lseek(_g_fd, 0, SEEK_SET);
        read(_g_fd, super_block, 1024); // superblock load

        long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;

        lseek(_g_fd, itable_location, SEEK_SET);
        read(_g_fd, inode_table, 8 * 1024 * 512); // itable load

        inode temp = inode_table[inode_num];
        temp.atime = time(NULL); // reinitializing atime
 
// we need to write new inode
	lseek(_g_fd, itable_location, SEEK_SET);
	write(_g_fd, inode_table, 8 * 1024 * 512);
*/
//      free(super_block);
//      free(inode_table);
	printf("@@@@@@@ open complete @@@@@@@\n");
	return 0;
}

static int ot_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("@@@@@@@ create start @@@@@@@\n");
        
	if (otfind(path) != -1) { // there is same file in path
                printf("@@@@@ create EEXIST error\n");
                return -EEXIST; // Actually, return value is -EEXIST
        }

	time_t cur = time(NULL);

	superblock* super_block;
	unsigned char* inode_bitmap;
	unsigned char* data_bitmap;
	inode* inode_table;

	super_block = malloc(1024);
	inode_bitmap = malloc(1024);
	data_bitmap = malloc(1024 * 128);
	inode_table = malloc(8 * 1024 * 512);

	lseek(_g_fd, 0, SEEK_SET);
	read(_g_fd, super_block, 1024);
	read(_g_fd, inode_bitmap, 1024);
	read(_g_fd, data_bitmap, 128 * 1024);
	read(_g_fd, inode_table, 8 * 1024 * 512);
	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;
	long dtable_location = itable_location + super_block->itable_size;

	int num_file = count(path, '/');
	// set new inode
	inode new;

	// allocation inode bitmap
	int new_ibitnum = checkb(_g_fd, 0);
	chanb(_g_fd, new_ibitnum, 0);
	
	// allocation data bitmap
	int new_dbitnum = checkb(_g_fd, 1);
	chanb(_g_fd, new_dbitnum, 1);

	// set new inode
	new.filename = malloc(28);
	new.size = 0;
	new.inode_num = new_ibitnum;
        for (int a = 0; a < 12; a++) { 
                new.data_num[a] = 0;
        }
	new.data_num[0] = new_dbitnum;
	new.file_or_dir = 0;
	new.ctime = cur;
	new.mtime = cur;
	new.atime = cur;
	new.mode = mode;
		
	// set new file Block
	char* new_file = (char*) calloc(1, 4096);
	// write in region file
	int inode_num;
	int dbit_num;
	int input_num;
	int root_check = 0;
	char temp[100]; // used strtok
	strcpy(temp, path);
	char *ptr; // used strtok
	int h;
	int i;
	int j;
	int k;
	int data_loop_num;
	bool loop_escape;
	Dir_Block* Dir = (Dir_Block*) malloc (4096); //parent dir
	
	inode fileinode; // parent inode
	// find dir with path	
	ptr = strtok(temp, "/"); // current inode
	for (h = 0; h < num_file; h++){
		// fileinode is parent inode
		if (root_check == 0) {
			//access to root
			inode_num = 0;
			fileinode = inode_table[0];
		}
		else {
			fileinode = inode_table[inode_num];
		}
		//we need to modify below loop, because < or <=
		if ((fileinode.size % 4096) == 0) {
			data_loop_num = fileinode.size / 4096;
		}
		else {
			data_loop_num = (fileinode.size / 4096) + 1;
		}
		for (i = 0; i < data_loop_num; i++) { // Set Dir directroy
			loop_escape = true;
			dbit_num = fileinode.data_num[i];
			pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
			if (h == num_file - 1) {
				strcpy(new.filename, ptr); 
				// we need to change parent inode's data
				for (k = 0; k < 128; k++) { // finding last empty number in inode_num array of Dir
					if ((strcmp(Dir->name_list[k], ".")) == 0) {
					       continue;
					}
					if ((strcmp(Dir->name_list[k], "..")) == 0) {
						continue;
					}
					if (Dir->inode_num[k] == 0) {
					        input_num = k;
						break;
					}
				} // if k > 128, we need to add new Data block?
			        strcpy(Dir->name_list[input_num], new.filename);	
				Dir->inode_num[input_num] = new.inode_num;
				pwrite(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); //write parent Dir
				// we need to add new inode in data_table
				pwrite(_g_fd, (char*) new_file, 4096, dtable_location + new_dbitnum * 4096);
			    	printf("dbitnum in create : %d\n", new_dbitnum);
				printf("The contents of new_file : \n%s\n", new_file);
				fileinode.ctime = cur;
				fileinode.mtime = cur;				
				fileinode.atime = cur; 
				inode_table[inode_num] = fileinode;
				loop_escape = false;
				break;
			}
			for (j = 0; j < 128; j++) {  // find ptr directory in Dir
				if ((strcmp((Dir->name_list[j]), ptr)) == 0) {
					inode_num = Dir->inode_num[j];
					fileinode.atime = cur; 
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
	inode_table[new_ibitnum] = new;
       	lseek(_g_fd, 0, SEEK_SET);
        write(_g_fd, super_block, 1024);
	lseek(_g_fd, itable_location, SEEK_SET);
        write(_g_fd, inode_table, 8 * 1024 * 512);

	
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);
	free(new_file);	
	free(Dir);
	printf("@@@@@@@ create complete @@@@@@@\n");
	return 0;
}

static int ot_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("@@@@@@@ read start @@@@@@@\n");
	/* this is hello_read
        size_t len;
        (void) fi;
        if(strcmp(path+1, options.filename) != 0)
                return -ENOENT;

        len = strlen(options.contents);
        if (offset < len) {
                if (offset + size > len)
                        size = len - offset;
                memcpy(buf, options.contents + offset, size);
        } else
                size = 0;

        return size;
        */

	int inode_num = fi->fh;

	superblock* super_block;
	inode* inode_table;
	super_block = malloc(1024);
	inode_table = malloc(8 * 1024 * 512);
	inode fileinode;

	lseek(_g_fd, 0, SEEK_SET);
	read(_g_fd, super_block, 1024); // superblock load
	//printf("super_block size : %d\nibitmap : %d\ndbit : %d\nitable : %d\n", sizeof(superblock), super_block->ibitmap_size, super_block->dbitmap_size, super_block->itable_size);
	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;	
	lseek(_g_fd, itable_location, SEEK_SET);
	read(_g_fd, inode_table, 8 * 1024 * 512);
	long dtable_location = itable_location + super_block->itable_size;

	fileinode = inode_table[inode_num];
	// we need to think about size related with hello_read
	int dbitnum = fileinode.data_num[0];
	printf("dbitnum : %d\n", dbitnum);
	printf("offset : %d\n", offset);
	int sig = pread(_g_fd, buf, size, dtable_location + dbitnum * 4096 + offset);
	if (sig == -1) {
		printf("READ ERROR\n");
		return -1;
	}
	printf("data buf : %s\n", buf);
	free(super_block);
	free(inode_table);
	printf("@@@@@@@ read complete @@@@@@@\n");
	return size;
}

static int ot_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("@@@@@@@ write start @@@@@@@\n");
        int inode_num = fi->fh;
        //if (inode_num = (otfind(path)) == -1) {
        //        return -1; // There is no file on the path
        // }

        superblock* super_block;
        inode* inode_table;
        super_block = malloc(1024);
        inode_table = malloc(8 * 1024 * 512);
        inode fileinode;

        lseek(_g_fd, 0, SEEK_SET);
        read(_g_fd, super_block, 1024); // superblock load
        //printf("super_block size : %d\nibitmap : %d\ndbit : %d\nitable : %d\n", sizeof(superblock), super_block->ibitmap_size, super_block->dbitmap_size, super_block->itable_size);
        long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;
        lseek(_g_fd, itable_location, SEEK_SET);
        read(_g_fd, inode_table, 8 * 1024 * 512);
        long dtable_location = itable_location + super_block->itable_size;

        fileinode = inode_table[inode_num];
	int dbitnum = fileinode.data_num[0];
	printf("dbitnum : %d\n", dbitnum);
	printf("offset : %d\n", offset);
	printf("data buf : %s\n", buf);
	int sig = pwrite(_g_fd, buf, size, dtable_location + dbitnum * 4096 + offset);
	if (sig  == -1) { // -1 means it does not write buf on the file
		printf("WRITE ERROR\n");
		return -1;
	}
	if ((fileinode.size - offset) < size) {  
		inode_table[inode_num].size = offset + size;
	}
	printf("size : %d\n", inode_table[inode_num].size);	
	lseek(_g_fd, itable_location, SEEK_SET);
        write(_g_fd, inode_table, 8 * 1024 * 512);
	free(super_block);
	free(inode_table);
	printf("@@@@@@@ write complete @@@@@@@\n");
	return size;
}
static int ot_utimens(const char *path, const struct timespec ts[2]) {
        int inode_num;
        if ((inode_num = otfind(path)) == -1) { //it means there is no file on the path
                return -1; 
        }

        superblock* super_block;
        inode* inode_table;
        super_block = malloc(1024);
        inode_table = malloc(8 * 1024 * 512);

        lseek(_g_fd, 0, SEEK_SET);
        read(_g_fd, super_block, 1024); // superblock load
        long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;
        lseek(_g_fd, itable_location, SEEK_SET);
        read(_g_fd, inode_table, 8 * 1024 * 512); // itable load

        inode fileinode = inode_table[inode_num];
	time_t cur = time(NULL);
	if (ts[0].tv_nsec == UTIME_NOW) {
		fileinode.atime = cur;
	} else if (ts[0].tv_nsec != UTIME_OMIT) {
		fileinode.atime = ts[0].tv_sec;
	}
	if (ts[1].tv_nsec == UTIME_NOW) {
		fileinode.mtime = cur;
	} else if (ts[1].tv_nsec != UTIME_OMIT) {
		fileinode.mtime = ts[1].tv_sec;
	}
	//temp.atime = (time_t) ts[0];
	//temp.mtime = (time_t) ts[1];

// we need to write new inode
        lseek(_g_fd, itable_location, SEEK_SET);
        write(_g_fd, inode_table, 8 * 1024 * 512);
        
        free(super_block);
        free(inode_table);
	printf("@@@@@@@ot_utimens complete @@@@@@@\n");
        return 0;
}

static int ot_release (const char *path, struct fuse_file_info *fi) {
	printf("@@@@@@@ot_release complete @@@@@@@\n");
        return 0;
}

static int ot_rename (const char *from, const char *to) {
	// we need to change inode.filename and parent's name_list
        int from_ino;
        if ((from_ino = otfind(from)) == -1) { //it means there is no file on the path
                printf("There is no filename with from, -ENOENT error!\n");
		return -ENOENT; 
        }
        int to_ino;
        if ((to_ino = otfind(to)) != -1) { //it means there is file on the path
                printf("There is filename with to, -EEXIST error!\n");
                return -EEXIST; 
        }

        int fd; 
        char region[10] = "region";
        if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0) {
                printf("Error\n");
                return 0;
        }

        superblock* super_block;
        inode* inode_table;

        super_block = malloc(1024);
        inode_table = malloc(8 * 1024 * 512);

        lseek(_g_fd, 0, SEEK_SET);
        read(_g_fd, super_block, 1024); // superblock load

        long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;

        lseek(_g_fd, itable_location, SEEK_SET);
        read(_g_fd, inode_table, 8 * 1024 * 512); // itable load

        inode to_inode = inode_table[to_ino];
	if (to_inode.file_or_dir != 1) {
		free(super_block);
		free(inode_table);
	    	printf("@@@@@@@-ENOTDIR error @@@@@@@\n");
		return -ENOTDIR;
	}
// we need to write new inode
        lseek(_g_fd, itable_location, SEEK_SET);
        write(_g_fd, inode_table, 8 * 1024 * 512);

        free(super_block);
        free(inode_table);
	printf("@@@@@@@ot_rename complete @@@@@@@\n");
        return 0;	
}

void ot_destroy (void *private_data) {
	close(_g_fd);
	printf("@@@@@@@ot_destroy complete @@@@@@@\n");
}

static const struct fuse_operations ot_oper = {
	.init           = ot_init,
	.mkdir		= ot_mkdir,
	.rmdir		= ot_rmdir,
	.getattr	= ot_getattr,
	.readdir	= ot_readdir,
	.open		= ot_open,
        .create		= ot_create,
	.read		= ot_read,
    	.write		= ot_write,
//      .unlink		= ot_unlink,
//	.utimens	= ot_utimens,
	.release	= ot_release,
	.rename		= ot_rename,	
	.destroy	= ot_destroy,

};

static void show_help(const char *progname)
{
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    --name=<s>          Name of the \"hello\" file\n"
	       "                        (default: \"hello\")\n"
	       "    --contents=<s>      Contents \"hello\" file\n"
	       "                        (default \"Hello, World!\\n\")\n"
	       "\n");
}

int main(int argc, char *argv[])
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	/* Set defaults -- we have to use strdup so that
	   fuse_opt_parse can free the defaults if other
	   values are specified */
	options.filename = strdup("hello");
	options.contents = strdup("Hello World!\n");

	/* Parse options */
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	/* When --help is specified, first print our own file-system
	   specific help text, then signal fuse_main to show
	   additional help (by adding `--help` to the options again)
	   without usage: line (by setting argv[0] to the empty
	   string) */
	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	ret = fuse_main(args.argc, args.argv, &ot_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}

