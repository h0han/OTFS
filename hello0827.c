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
	char name_list[16][252]; // In map, there are 128 files;
	int inode_num[16];
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
typedef struct Indirect_block {
	int indirect_data_num[1024];
} indirect_block;

typedef struct Inode { // size : 128 * 4 = 512b 
	char* filename; // does it need malloc?
	long size;
	int inode_num;
	int data_num[12];
	int s_indirect;
	int d_indirect;
	int file_or_dir; // file == 0, dir == 1;
	time_t atime; //access time
	time_t ctime; //change time, time for changing about file metadata
	time_t mtime; //modify time, time for changing about file data such as contents of file
	mode_t mode;	
	int ig[100]; // For 1024 bytes
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

char* ot_paren(const char* path, char *par_path) {
        int num_file = count(path, '/');
	char temp[252]; // used strtok
	strcpy(temp, path);
        char* ptr = strtok(temp, "/");
        printf("%d\n", num_file);
        for (int i = 0; i < (num_file -1); i++) {
                strcat(par_path, "/");
                strcat(par_path, ptr);
                printf("%s\n", par_path);
                printf("%s\n", ptr);
                ptr = strtok(NULL, "/");
        }
        if(strcmp(par_path, "") ==0){
                strcat(par_path, "/");
        }

        return par_path;
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
	char temp[252]; // used strtok
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
			printf("dbitnum : %d\n", dbit_num);
			printf("filename : %s\n", fileinode.filename);
			pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // root_dbitnum is offset
			for (j = 0; j < 16; j++) {
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


static void *ot_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	int size = 1024; 
	printf("@@@@@@@INIT start @@@@@@@\n");
	printf("@@@@@@@superblock size : %ld @@@@@@@\n", sizeof(superblock));
	printf("@@@@@@@inode size : %ld @@@@@@@\n", sizeof(inode));
	printf("@@@@@@@Dir block size : %ld @@@@@@@\n", sizeof(Dir_Block));
	
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
	root.filename = malloc(252);	
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
	printf("@@@@@@@mkdir mode : %d start\n", mode);
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
	//printf("new_ibitnum : %d(it will be 1)\n", new_ibitnum);
	//printf("onoffcheck : %d\n", onoffcheck(_g_fd, new_ibitnum,0)); 

	// need data bitmap alloc
	int new_dbitnum = checkb(_g_fd, 1);
	chanb(_g_fd, new_dbitnum, 1);
	//printf("new_ibitnum : %d(it will be 1)\n", new_dbitnum);
	//printf("onoffcheck : %d\n", onoffcheck(_g_fd, new_dbitnum,0)); 
	
	// set new inode
	new.filename = malloc(252);	
	new.size = 4096; // at first, root's size = 4kb (1 Datablock)
	new.inode_num = new_ibitnum;
	for (int set = 0; set < 12; set++) { 
		new.data_num[set] = 0;
	}
	new.data_num[0] = new_dbitnum;
	new.file_or_dir = 1;
	new.ctime = cur;
	new.mtime = cur;
	new.atime = cur;
	new.mode = S_IFDIR | mode;

	// set new directory Block	
	Dir_Block* new_dir = malloc(4096);
	strcpy(new_dir->name_list[0], ".");
	new_dir->inode_num[0] = new_ibitnum;
	
        char par_path[252] = "";
        printf("paren(path): %s\n", ot_paren(path, par_path));

	int parent_ino = otfind(par_path);
	inode parent = inode_table[parent_ino];
	Dir_Block* parent_dir = (Dir_Block*) malloc (4096); //parent dir
        indirect_block indirect_b = {0,};

	if (strcmp(par_path, "/") == 0) {
		strcpy(new.filename, path + strlen(par_path));
	}
	else {
		strcpy(new.filename, path + strlen(par_path) + 1); 
	}
	strcpy(new_dir->name_list[1], "..");
	new_dir->inode_num[1] = parent_ino;

	int indirect_flag = 0;
	int dbit_num;
	int input_num;
	bool loop_escape;
	int i;
	int k;
	int data_loop_num = parent.size / 4096;
	loop_escape = false;
	for (i = 0; i < data_loop_num; i++) {
		dbit_num = parent.data_num[i];
		pread(_g_fd, parent_dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
		printf("dbit_num : %d\n", dbit_num);
		printf("filename : %s\n", parent.filename);
		printf(". : %s\n", parent_dir->name_list[0]);
		for (k = 0; k < 16; k++) {
			if ((strcmp(parent_dir->name_list[k], ".")) == 0) {
				continue;
		    	}
			if ((strcmp(parent_dir->name_list[k], "..")) == 0) {
				continue;
			}
			if (parent_dir->inode_num[k] == 0) {
				input_num = k;
				loop_escape = true;
	    			break;
			}
		}
		printf("i : %d\n", i);
		printf("k : %d\n", k);
		if (loop_escape == true) {
			break;
		}
		if (k == 16) {
			printf("i : %d\n", i);
			printf("dataloopnum : %d\n", data_loop_num);
			if (i != data_loop_num - 1) { 
				continue;
			}
			else if (i == data_loop_num -1 && i != 11) {
				// new directory block
				printf("@@@@@@ we need to add new datablock\n");
				printf("dirinode name : %s\n", parent.filename);
				printf("dirinode size : %ld\n", parent.size);
				parent.size += 4096;
				dbit_num = checkb(_g_fd, 1); // This datablock is for parent's new datablock
				chanb(_g_fd, dbit_num, 1);
				parent.data_num[i+1] = dbit_num;
				Dir_Block* new_dir = calloc(1, 4096);
				pwrite(_g_fd, new_dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
				free(new_dir);
				pread(_g_fd, parent_dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
				input_num = 0;
			}	
			else { // i == data_loop_num -1 && i == 11
				// indirect block
				indirect_flag = 1;
			}
		}
	}
	if (indirect_flag == 1) {
		printf("########we have to use indirect block\n");
                new.s_indirect = checkb(_g_fd, 1);
                chanb(_g_fd, new.s_indirect, 1);
                //indirect_b.indirect_data_num = dirinode.s_indirect
                pread(_g_fd, &indirect_b,4096, dtable_location+new.s_indirect*4096);
                for(int m=0; m<1024;m++){
                        if (indirect_b.indirect_data_num[m] == 0){
                                int ind_ele = checkb(_g_fd,1);
                                indirect_b.indirect_data_num[m] = ind_ele;
                                parent.data_num[i+1] = dbit_num;
                                chanb(_g_fd, ind_ele, 1);
                                pread(_g_fd, parent_dir, 4096, dtable_location + ind_ele*4096);
                                dbit_num = ind_ele;
                                //Dir = indirect_b;
                                break;
			}// see indirect block	
		}
	}
	printf("input_num : %d\n", input_num);
	printf("dbit_num : %d\n", dbit_num);
	printf("new.filename : %s\n", new.filename);

	strcpy(parent_dir->name_list[input_num], new.filename);	
	parent_dir->inode_num[input_num] = new.inode_num;
	printf(". : %s\n", parent_dir->name_list[0]);
	pwrite(_g_fd, (char*) parent_dir, 4096, dtable_location + dbit_num * 4096); //write parent Dir
	// we need to add new inode in data_table
	pwrite(_g_fd, (char*) new_dir, 4096, dtable_location + new_dbitnum * 4096);
	parent.ctime = cur;
	parent.mtime = cur;				
	parent.atime = cur; 
	inode_table[parent_ino] = parent;	
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
	free(parent_dir);
	printf("@@@@@@@mkdir complete @@@@@@@\n");
	return 0;
	/*	
	// write in region file	
	int inode_num; 
	int dbit_num;
	int input_num;
	int root_check = 0;
	char temp[252]; // used strtok
	strcpy(temp, path); 
	char *ptr; // used strtok
	int h;	
	int i; // used for loop
	int j; // used for loop 
	int k;
	int data_loop_num; // real num is data_loop_num + 1
	bool loop_escape;
	Dir_Block* Dir = (Dir_Block*) malloc (4096); //parent dir
        indirect_block indirect_b = {0,};

	inode dirinode;
	// find dir with path	
	printf("temp : %s\n", temp);
	ptr = strtok(temp, "/");
	for (h = 0; h < num_file; h++){
		printf("ptr : %s\n", ptr);	
		// fileinode is parent inode
		if (root_check == 0) {
			//access to root
			inode_num = 0;
			dirinode = inode_table[0];
		}
		else {
			dirinode = inode_table[inode_num];
		}
		//we need to modify below loop, because < or <=
		if ((dirinode.size % 4096) == 0) {
			data_loop_num = dirinode.size / 4096;
		}
		else {
			data_loop_num = (dirinode.size / 4096) + 1;
		}
		for (i = 0; i < data_loop_num; i++) { // Set Dir directroy
			loop_escape = true;
			dbit_num = dirinode.data_num[i];
			pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
			if (h == num_file - 1) {
				strcpy(new.filename, ptr); 
				strcpy(new_dir->name_list[1], "..");
				new_dir->inode_num[1] = inode_num;
				// we need to change parent inode's data
				for (k = 0; k < 16; k++) { // finding last empty number in inode_num array of Dir
					
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
			        
				if (k == 16) { // There are full files in directory
					if (i == 11) { // we need to use indirect pointer
						// indirect pointer
						// check first: is there indirect pointer already
						//if(dirinode.s_indirect == 0){}
						printf("########we have to use indirect block\n");
                                                dirinode.s_indirect = checkb(_g_fd, 1);
                                                chanb(_g_fd, dirinode.s_indirect, 1);
                                                //indirect_b.indirect_data_num = dirinode.s_indirect;
						pread(_g_fd, &indirect_b,4096, dtable_location+dirinode.s_indirect*4096);
						//indirect_b = Dir;
						for(int m=0; m<1024;m++){
							if (indirect_b.indirect_data_num[m] == 0){
								int ind_ele = checkb(_g_fd,1);
								indirect_b.indirect_data_num[m] = ind_ele;
					parent.data_num[i+1] = dbit_num;			chanb(_g_fd, ind_ele, 1);
								pread(_g_fd, (char*) Dir, 4096, dtable_location + ind_ele*4096);
								dbit_num = ind_ele;
								//Dir = indirect_b;
								break;
							}
						}
					}
					else {
						printf("@@@@@@ we need to add new datablock\n");
						printf("dirinode name : %s\n", dirinode.filename);
						printf("dirinode size : %ld\n", dirinode.size);
						dirinode.size += 4096;
						dbit_num = checkb(_g_fd, 1); // This datablock is for parent's new datablock
						chanb(_g_fd, dbit_num, 1);
						dirinode.data_num[i+1] = dbit_num;
						pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
						input_num = 0;
					}
				}

				strcpy(Dir->name_list[input_num], new.filename);	
				Dir->inode_num[input_num] = new.inode_num;
				pwrite(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); //write parent Dir
				// we need to add new inode in data_table
				pwrite(_g_fd, (char*) new_dir, 4096, dtable_location + new_dbitnum * 4096);
			    	dirinode.ctime = cur;
				dirinode.mtime = cur;				
				dirinode.atime = cur; 
				inode_table[inode_num] = dirinode;
				loop_escape = false;
				break;
			}
			for (j = 0; j < 16; j++) {  // find ptr directory in Dir
				if ((strcmp((Dir->name_list[j]), ptr)) == 0) {
					inode_num = Dir->inode_num[j];
					dirinode.atime = cur; 
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
	*/
}
static int ot_unlink(const char* path){
        printf("###ot_unlink start###\n");
        int inum = otfind(path);
        printf("%s's inode num: %d\n", path, inum);
        inode* inode_table = malloc(8*512*1024);
        inode dirinode;

        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        read(_g_fd, inode_table, 1024*8*512);
        dirinode = inode_table[inum];

        for(int i = 0; i<12;i++){
                if(dirinode.data_num[i]!=0){
                chanb(_g_fd, dirinode.data_num[i], 1);
               }
        }
        chanb(_g_fd, inum, 0);

        inode_table[inum] = dirinode;
        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        write(_g_fd, inode_table, 1024*8*512);

        printf("touch parents's data\n");
        inode pino;
        printf("path: %s\n", path);
        char par_path[252] = "";
        printf("paren(path): %s\n", ot_paren(path, par_path));
        int pinum = otfind(ot_paren(path, par_path));
        printf("parent inode number: %d\n", pinum);
        if (pinum == -1){
                return -EEXIST;
        }
        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        read(_g_fd, inode_table, 1024*8*512);
        pino = inode_table[pinum];
        Dir_Block* Dir = (Dir_Block*) malloc (4096);
        for (int i = 0;i<12;i++){
                int pdnum = pino.data_num[i];
                if(pdnum ==0){
                        printf("for %dst parent inode's date_num: %d\n", i, pdnum);
                        //break;
                }
                pread(_g_fd, (char*) Dir, 4096, 2048+1024*128+512*8*1024 + pdnum * 4096);
                for(int k =0; k<16;k++){
                        printf("parent's Dir->inode_num[]: %d\n", Dir->inode_num[k]);
                        if(Dir->inode_num[k] == inum){
                                Dir->inode_num[k] = 0;
                                strcpy(Dir->name_list[k],"");
                        }
                }
                pwrite(_g_fd, (char*) Dir, 4096, 2048+1024*128+512*8*1024 + pdnum * 4096);
        }
        free(Dir);
	free(inode_table);
        printf("###ot_unlink end###\n");
        return 0;
}

static int ot_rmdir(const char* path){
        printf("###ot_rmdir start###\n");
        int inum = otfind(path);
        printf("%s's inode num: %d\n", path, inum);
        inode* inode_table = malloc(8*512*1024);
        inode dirinode;

        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        read(_g_fd, inode_table, 1024*8*512);
        dirinode = inode_table[inum];

        for(int i = 0; i<12;i++){
                //printf("dirinode.data_num[%d]: %d\n", i, dirinode.data_num[i]);
        //}
        //for(int i = 1;i<12;i++){ //there is a data about dirctory(itself) in data_num[0]
                if((i!=0)&&(dirinode.data_num[i]!=0)){
                        return -ENOTEMPTY; // error: there are files in dirctory
                }
        //}
        //for(int j = 0; j<12;j++){
                if(dirinode.data_num[i]!=0){
                chanb(_g_fd, dirinode.data_num[i], 1);
               }
        }
        chanb(_g_fd, inum, 0);

        inode_table[inum] = dirinode;
        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        write(_g_fd, inode_table, 1024*8*512);

        printf("touch parents's data\n");
        inode pino;
        printf("path: %s\n", path);
        char par_path[252] = "";
        printf("paren(path): %s\n", ot_paren(path, par_path));
        int pinum = otfind(ot_paren(path, par_path));
        printf("parent inode number: %d\n", pinum);
        if (pinum == -1){
                return -EEXIST;
        }
        //clear!!!
        //
        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        read(_g_fd, inode_table, 1024*8*512);
        pino = inode_table[pinum];
        Dir_Block* Dir = (Dir_Block*) malloc (4096);
	indirect_block indirect_b = {0,};

        for (int i = 0;i<12;i++){
                int pdnum = pino.data_num[i];
                if(pdnum ==0){
                        printf("for %dst parent inode's date_num: %d\n", i, pdnum);
                //        break;
                }
                pread(_g_fd, (char*) Dir, 4096, 2048+1024*128+512*8*1024 + pdnum * 4096);
                for(int k =0; k<16;k++){
                        printf("parent's Dir->inode_num[]: %d\n", Dir->inode_num[k]);
                        if(Dir->inode_num[k] == inum){
                                Dir->inode_num[k] = 0;
                                strcpy(Dir->name_list[k],"");
                        }
                }
		if(pino.s_indirect != 0){
			pread(_g_fd, &indirect_b, 4096, 2048+1024*128+512*8*1024 + pino.s_indirect * 4096);
			for(int m=0;m<1024;m++){
				if(indirect_b.indirect_data_num[m]!=0){
					pread(_g_fd, (char*) Dir, 4096, 2048+1024*128+512*8*1024 + indirect_b.indirect_data_num[m] * 4096);
					for(int k =0; k<16;k++){
	        	        	        printf("parent's Dir->inode_num[]: %d\n", Dir->inode_num[k]);
        	        	        	if(Dir->inode_num[k] == inum){
                                			Dir->inode_num[k] = 0;
                                			strcpy(Dir->name_list[k],"");
						}
					}		
				}
			}
		}
                pwrite(_g_fd, (char*) Dir, 4096, 2048+1024*128+512*8*1024 + pdnum * 4096);
        }
        free(Dir);
        printf("###ot_rmdir end###\n");
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
		/*	
			if (temp.file_or_dir == 1) { 
				buf->st_mode = S_IFDIR | 0755;
			}
			else {
				buf->st_mode = S_IFREG | 0444;
			}
		*/	
			buf->st_mode = temp.mode;
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
		for (k = 0; k < 16; k++) {  // k = 0 -> ".", k = 1 -> ".."
			if ((strcmp(Dir->name_list[k], ".")) == 0) {
				continue;
			}
			if ((strcmp(Dir->name_list[k], "..")) == 0) {
				continue;
			}
		        if (Dir->inode_num[k] == 0) { // we need to think about root_num
				continue;
			}
			printf("Dir->name_list[%d]: %s\n",k, Dir->name_list[k]);
			printf("Dir->inode_num[%d]: %d\n",k, Dir->inode_num[k]);
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
	printf("@@@@@@@ create mode : %d @@@@@@@\n", mode);
	
        
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

	// set new inode
	inode new;

	// allocation inode bitmap
	int new_ibitnum = checkb(_g_fd, 0);
	chanb(_g_fd, new_ibitnum, 0);
	
	// allocation data bitmap
	int new_dbitnum = checkb(_g_fd, 1);
	chanb(_g_fd, new_dbitnum, 1);
	printf("dbitnum : %d\n", new_dbitnum);

	// set new inode
	new.filename = malloc(252);
	new.size = 0;
	new.inode_num = new_ibitnum;
        for (int set = 0; set < 12; set++) { 
                new.data_num[set] = 0;
        }
	new.data_num[0] = new_dbitnum;
	new.file_or_dir = 0;
	new.ctime = cur;
	new.mtime = cur;
	new.atime = cur;
	new.mode = S_IFREG | mode;
		
	// set new file Block
	char* new_file = (char*) calloc(1, 4096);
	// write in region file
	
        char par_path[252] = "";
        printf("paren(path): %s\n", ot_paren(path, par_path));

	int parent_ino = otfind(par_path);
	inode parent = inode_table[parent_ino];
	Dir_Block* parent_dir = (Dir_Block*) malloc (4096); //parent dir
        indirect_block indirect_b = {0,};

	if (strcmp(par_path, "/") == 0) {
		strcpy(new.filename, path + strlen(par_path));
	}
	else {
		strcpy(new.filename, path + strlen(par_path) + 1); 
	}

	int indirect_flag = 0;
	int dbit_num;
	int input_num;
	bool loop_escape;
	int i;
	int k;
	int data_loop_num = parent.size / 4096;
	loop_escape = false;
	for (i = 0; i < data_loop_num; i++) {
		dbit_num = parent.data_num[i];
		pread(_g_fd, parent_dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
		printf("dbit_num : %d\n", dbit_num);
		printf("filename : %s\n", parent.filename);
		printf(". : %s\n", parent_dir->name_list[0]);
		for (k = 0; k < 16; k++) {
			if ((strcmp(parent_dir->name_list[k], ".")) == 0) {
				continue;
		    	}
			if ((strcmp(parent_dir->name_list[k], "..")) == 0) {
				continue;
			}
			if (parent_dir->inode_num[k] == 0) {
				input_num = k;
				loop_escape = true;
	    			break;
			}
		}
		printf("i : %d\n", i);
		printf("k : %d\n", k);
		if (loop_escape == true) {
			break;
		}
		if (k == 16) {
			printf("i : %d\n", i);
			printf("dataloopnum : %d\n", data_loop_num);
			if (i != data_loop_num - 1) { 
				continue;
			}
			else if (i == data_loop_num -1 && i != 11) {
				// new directory block
				printf("@@@@@@ we need to add new datablock\n");
				printf("dirinode name : %s\n", parent.filename);
				printf("dirinode size : %ld\n", parent.size);
				parent.size += 4096;
				dbit_num = checkb(_g_fd, 1); // This datablock is for parent's new datablock
				chanb(_g_fd, dbit_num, 1);
				parent.data_num[i+1] = dbit_num;
				Dir_Block* new_dir = calloc(1, 4096);
				pwrite(_g_fd, new_dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
				free(new_dir);
				pread(_g_fd, parent_dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
				input_num = 0;
			}	
			else { // i == data_loop_num -1 && i == 11
				// indirect block
				indirect_flag = 1;
			}
		}
	}
	if (indirect_flag == 1) {
		// see indirect block
	        printf("########we have to use indirect block\n");
                new.s_indirect = checkb(_g_fd, 1);
                chanb(_g_fd, new.s_indirect, 1);
                //indirect_b.indirect_data_num = dirinode.s_indirect
                pread(_g_fd, &indirect_b,4096, dtable_location+new.s_indirect*4096);
                for(int m=0; m<1024;m++){
                        if (indirect_b.indirect_data_num[m] == 0){
                                int ind_ele = checkb(_g_fd,1);
                                indirect_b.indirect_data_num[m] = ind_ele;
                                parent.data_num[i+1] = dbit_num;
                                chanb(_g_fd, ind_ele, 1);
                                pread(_g_fd, parent_dir, 4096, dtable_location + ind_ele*4096);
                                dbit_num = ind_ele;
                                //Dir = indirect_b;
                                break;
                        }
		}
	}
	printf("input_num : %d\n", input_num);
	printf("dbit_num : %d\n", dbit_num);
	printf("new.filename : %s\n", new.filename);

	strcpy(parent_dir->name_list[input_num], new.filename);	
	parent_dir->inode_num[input_num] = new.inode_num;

	//printf("3 : %s\n", parent_dir->name_list[3]);
	//printf("4 : %s\n", parent_dir->name_list[4]);
	pwrite(_g_fd, (char*) parent_dir, 4096, dtable_location + dbit_num * 4096); //write parent Dir
	
		// we need to add new inode in data_table
	pwrite(_g_fd, (char*) new_file, 4096, dtable_location + new_dbitnum * 4096);
	parent.ctime = cur;
	parent.mtime = cur;				
	parent.atime = cur; 
	inode_table[parent_ino] = parent;	
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
	free(parent_dir);
	/*
	int inode_num;
	int dbit_num;
	int input_num;
	int root_check = 0;
	char temp[252]; // used strtok
	strcpy(temp, path);
	char *ptr; // used strtok
	int h;
	int i;
	int j;
	int k;
	int data_loop_num;
	bool loop_escape;
	Dir_Block* Dir = (Dir_Block*) malloc (4096); //parent dir
	indirect_block indirect_b = {0,};	
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
				for (k = 0; k < 16; k++) { // finding last empty number in inode_num array of Dir
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
                                if (k == 16) { // There are full files in directory
                                        if (i == 11) { // we need to use indirect pointer
                                                // indirect pointer
						printf("########we have to use indirect block\n");
                                                fileinode.s_indirect = checkb(_g_fd, 1);
                                                chanb(_g_fd, fileinode.s_indirect, 1);
                                                //indirect_b.indirect_data_num = dirinode.s_indirect;
						pread(_g_fd, &indirect_b,4096, dtable_location+fileinode.s_indirect*4096);
						//indirect_b = Dir;
						for(int m=0; m<1024;m++){
							if (indirect_b.indirect_data_num[m] == 0){
								int ind_ele = checkb(_g_fd,1);
								indirect_b.indirect_data_num[m] = ind_ele;
								chanb(_g_fd, ind_ele, 1);
								pread(_g_fd, (char*) Dir, 4096, dtable_location + ind_ele*4096);
								dbit_num = ind_ele;
								//Dir = indirect_b;
								pwrite(_g_fd, &indirect_b, 4096, dtable_location+fileinode.s_indirect*4096);
								break;
							}
						}
                                        }
                                        else {
                                                printf("@@@@@@ we need to add new datablock\n");
                                                printf("dirinode name : %s\n", fileinode.filename);
                                                printf("dirinode size : %ld\n", fileinode.size);
                                                fileinode.size += 4096;
                                                dbit_num = checkb(_g_fd, 1); // This datablock is for parent's new datablock
                                                chanb(_g_fd, dbit_num, 1);
                                                fileinode.data_num[i+1] = dbit_num;
                                                pread(_g_fd, (char*) Dir, 4096, dtable_location + dbit_num * 4096); // dbit_num is offset
                                                input_num = 0;
                                        }
                                }
				
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
			for (j = 0; j < 16; j++) {  // find ptr directory in Dir
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
*/
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

	printf("offset : %d\n", offset);
	printf("size : %d\n", size);
	int inode_num = otfind(path);

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
	if (fileinode.file_or_dir == 1) {
		free(super_block);
		free(inode_table);
		return -EISDIR;
	}
	if (offset > fileinode.size) {
		free(super_block);
		free(inode_table);
		printf("Offset is bigger than fileinode.size\n");	
		return -1;
	}
	
	char* data = calloc(1, fileinode.size); // filesize
	printf("fileinode.size : %ld\n", fileinode.size);
	char* temp = calloc(1, 4096);
	indirect_block* new_indirect = calloc(1, 4096);
	int remain_size;
	int dbitnum;
	int i;

	int data_loop_num; // we need to see data block with data_loop_num number.
	if ((fileinode.size % 4096) == 0) {
		data_loop_num = fileinode.size / 4096;
	}
    	else {
		data_loop_num = (fileinode.size / 4096) + 1;
	}
	printf("data_loop : %d\n", data_loop_num);
	int check = 0;
	if (data_loop_num > 12) { 
		// we need to use indirect pointer
		printf("go indirect\n");
		for (i = 0; i < 12; i++) {
			/*
                        if (i == new_data_loop_num - 1) { // this is end data block
                                printf("i : %d\n", i);
                                remain_size = fileinode.size - ((new_data_loop_num - 1) * 4096);
                                printf("remain_size : %d\n", remain_size);
                                dbitnum = fileinode.data_num[i];
                                printf("dbitnum : %d\n", dbitnum);
                                memcpy(temp, new_data + i * 4096, remain_size);
                                pwrite(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
                        }
			*/
			dbitnum = fileinode.data_num[i];
			printf("dbitnum : %d\n", dbitnum);
			pread(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
			memcpy(data + i * 4096, temp, 4096);
			check += 4096;
		}
		pread(_g_fd, new_indirect, 4096, dtable_location + fileinode.s_indirect * 4096);
		for (i = 0; i < data_loop_num - 12; i++) {
                        if (i == data_loop_num - 13) {
                                remain_size = fileinode.size - ((data_loop_num - 1) * 4096);
				printf("remian_size : %d\n", remain_size);
				dbitnum = new_indirect->indirect_data_num[i];
				printf("dbitnum : %d\n", dbitnum);
				pread(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
				memcpy(data + (i+12) * 4096, temp, remain_size);
				check += remain_size;
			}
			else { 
				dbitnum = new_indirect->indirect_data_num[i];
				printf("dbitnum : %d\n", dbitnum);
				pread(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
				memcpy(data + (i+12) * 4096, temp, 4096);
				check += 4096;
			}
		}
	}
	else {
		// we get data
		for (i = 0; i < data_loop_num; i++){
			if (i == data_loop_num - 1) { // we need to think about remain data
				remain_size = fileinode.size - ((data_loop_num - 1) * 4096);
				printf("remian_size : %d\n", remain_size);
				dbitnum = fileinode.data_num[i];
				pread(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
				memcpy(data + i * 4096, temp, remain_size);
			}
			else {
				dbitnum = fileinode.data_num[i];
				pread(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
				memcpy(data + i * 4096, temp, 4096);
			}
		}	
	}
	printf("check : %d\n", check);	
	// copy data to buf
	memcpy(buf, data + offset, size);
	
	free(super_block);
	free(inode_table);
	free(temp);
	free(new_indirect);
	free(data);
	printf("@@@@@@@ read complete @@@@@@@\n");
	return size;
}

static int ot_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	// Honestly, this is inefficient. If there is small change, we need to do many things.
	// we don't need to think about \0, because we have offset
	printf("@@@@@@@ write start @@@@@@@\n");
        int inode_num = otfind(path);
        //if (inode_num = (otfind(path)) == -1) {
        //        return -1; // There is no file on the path
        // }
	printf("size : %ld\n", size);
	printf("offset : %ld\n", offset);

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
	if (fileinode.file_or_dir == 1) {
		free(super_block);
		free(inode_table);
		return -EISDIR;
	}
	if (offset > fileinode.size) {
		free(super_block);
		free(inode_table);
		printf("Offset is bigger than fileinode.size\n");	
		return -1;
	}
	/*
	off_t offset_copy = offset;
	size_t size_remain = size;
	int sig;

	int start_block = (int) offset_copy / 4096;	
	int offset_remain = offset_copy - (start_block * 4096);
*/
	char* data = calloc(1, fileinode.size); // filesize
	//char* temp;
	char* temp = calloc(1, 4096);
	int remain_size;
	int dbitnum;
	int i;
	// set fileinode.size
	int oldfilesize = fileinode.size;
	printf("oldfilesize : %d\n", oldfilesize);
	int isbig = 0; // isbig = 1, (fileinode.size - offset) < size is true
	if ((fileinode.size - offset) < size) {  
		fileinode.size = offset + size;
		isbig = 1;
	}
	printf("fileinode.size : %ld\n", fileinode.size);
	char* new_data = calloc(1, fileinode.size);

	int data_loop_num; // we need to see data block with data_loop_num number.
	if ((oldfilesize % 4096) == 0) {
		data_loop_num = oldfilesize / 4096;
	}
    	else {
		data_loop_num = (oldfilesize / 4096) + 1;
	}
	
	if (oldfilesize == 0) {
		data_loop_num = 1;
	}

	// we need to allocate new data block
	int new_dbitnum;
	int new_data_loop_num;
	int number_single_in; 
	indirect_block* new_indirect = calloc(1, 4096);
	printf("fileinode.s_ind : %d\n", fileinode.s_indirect);
	//pread(_g_fd, new_indirect, 4096, dtable_location + fileinode.s_indirect * 4096);
	printf("new_indirect->indirect_data_num[1] : %d\n", new_indirect->indirect_data_num[1]);
	if (isbig == 1) { 
		if ((fileinode.size % 4096) == 0) {
			new_data_loop_num = fileinode.size / 4096;
		}
		else {
			new_data_loop_num = (fileinode.size / 4096) + 1;
		}
		if (new_data_loop_num <= 12) {
			for (i = data_loop_num; i < new_data_loop_num; i++) {
				new_dbitnum = checkb(_g_fd, 1);
				chanb(_g_fd, new_dbitnum, 1);
				fileinode.data_num[i] = new_dbitnum;
			}
		}
		else if (new_data_loop_num <= 1036) { // 1024 + 12
			if (data_loop_num <= 12) { // Fileinode does not allocate indirect block
				for (i = data_loop_num; i < 12; i++) {
					new_dbitnum = checkb(_g_fd, 1);
					chanb(_g_fd, new_dbitnum, 1);
					fileinode.data_num[i] = new_dbitnum;
				}
				number_single_in = new_data_loop_num - 12;
				fileinode.s_indirect = checkb(_g_fd, 1);
				chanb(_g_fd, fileinode.s_indirect, 1);
				for (i = 0; i < number_single_in; i++) {
					new_dbitnum = checkb(_g_fd, 1);
					chanb(_g_fd, new_dbitnum, 1);
					new_indirect->indirect_data_num[i] = new_dbitnum;
				}	
			}
			else { // we don't need to allocate new indirect block
				number_single_in = new_data_loop_num - data_loop_num;
				new_indirect = calloc(1, 4096);
				pread(_g_fd, new_indirect, 4096, dtable_location + fileinode.s_indirect * 4096);
				for (i = 0; i < number_single_in; i++) {
					new_dbitnum = checkb(_g_fd, 1);
					chanb(_g_fd, new_dbitnum, 1);
					new_indirect->indirect_data_num[i + data_loop_num -12]=new_dbitnum;
				}
			}
		}	

		else {
			printf("Size is too big\n");
		}
		pwrite(_g_fd, new_indirect, 4096, dtable_location + fileinode.s_indirect * 4096);
	}
	else {
		new_data_loop_num = data_loop_num;
	}

	printf("data_loop_num : %d\n", data_loop_num);
	printf("new_data_loop_num : %d\n", new_data_loop_num);
	printf("fileinode.s_ind : %d\n", fileinode.s_indirect);
	pread(_g_fd, new_indirect, 4096, dtable_location + fileinode.s_indirect * 4096);
	printf("new_indirect->indirect_data_num[1] : %d\n", new_indirect->indirect_data_num[260]);

	if (oldfilesize > 4096 * 12) { 
		// we need to use indirect pointer
		// we need to get all data with indirect pointer
                printf("read indirect in write function\n");
                printf("old filesize : %ld\n", oldfilesize);
                for (i = 0; i < 12; i++) {
                        /*
                        if (i == new_data_loop_num - 1) { // this is end data block
                                printf("i : %d\n", i);
                                remain_size = fileinode.size - ((new_data_loop_num - 1) * 4096);
                                printf("remain_size : %d\n", remain_size);
                                dbitnum = fileinode.data_num[i];
                                printf("dbitnum : %d\n", dbitnum);
                                memcpy(temp, new_data + i * 4096, remain_size);
                                pwrite(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
                        }
                        */
                        dbitnum = fileinode.data_num[i];
                        printf("dbitnum : %d\n", dbitnum);
                        pread(_g_fd, data, 4096, dtable_location + dbitnum * 4096);
                        memcpy(data + i * 4096, temp, 4096);
                }
                pread(_g_fd, new_indirect, 4096, dtable_location + fileinode.s_indirect * 4096);
                for (i = 0; i < data_loop_num - 12; i++) {
                        if (i == data_loop_num - 13) {
                                remain_size = oldfilesize - ((data_loop_num - 1) * 4096);
                                printf("remian_size : %d\n", remain_size);
                                dbitnum = new_indirect->indirect_data_num[i];
                                pread(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
                                memcpy(data + (i+12) * 4096, temp, remain_size);
                        }
                        else {
                                dbitnum = new_indirect->indirect_data_num[i];
                                pread(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
                                memcpy(data + (i+12) * 4096, temp, 4096);
                        }
                }
		printf("read end\n");
	}
	else {
		// we get data
		for (i = 0; i < data_loop_num; i++){
			if (i == data_loop_num - 1) { // we need to think about remain data
				remain_size = oldfilesize - ((data_loop_num - 1) * 4096);
				printf("remain size : %d\n", remain_size);
				dbitnum = fileinode.data_num[i];
				printf("dbitnum : %d\n", dbitnum);
				pread(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
				memcpy(data + i * 4096, temp, remain_size);
			}
			else {
				temp = calloc(1, 4096);
				dbitnum = fileinode.data_num[i];
				pread(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
				memcpy(data + i * 4096, temp, 4096);
			}
		}	
	}
	printf("data : %s\n", data);
	// copy data to buf
	if (offset != 0) {
		memcpy(new_data, data, offset); // until offset, there is the number of offset char;
	}
	printf("size : %ld\n", size);
	memcpy(new_data + offset, buf, size);
	if (isbig == 0) {
		remain_size = oldfilesize - offset - size;
		memcpy(new_data + offset + size, data + offset + size, remain_size);
	}
	int check = 0;
	if (fileinode.size > 4096 * 12) { 
		// we need to use indirect pointer
		// we need to write new data with indirect pointer
		printf("new_data_loop_num : %d\n", new_data_loop_num);
		printf("use indirect\n");
		printf("12 direct block\n");
		for (i = 0; i < 12; i++) {
			/*
                        if (i == new_data_loop_num - 1) { // this is end data block
                                printf("i : %d\n", i);
                                remain_size = fileinode.size - ((new_data_loop_num - 1) * 4096);
                                printf("remain_size : %d\n", remain_size);
                                dbitnum = fileinode.data_num[i];
                                printf("dbitnum : %d\n", dbitnum);
                                memcpy(temp, new_data + i * 4096, remain_size);
                                pwrite(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
                        }
			*/
                        printf("i : %d\n", i);
                        dbitnum = fileinode.data_num[i];
                        printf("dbitnum : %d\n", dbitnum);
                        memcpy(temp, new_data + i * 4096, 4096);
                        pwrite(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
			check += 4096;
		}
		printf("go indirect block\n");
		pread(_g_fd, new_indirect, 4096, dtable_location + fileinode.s_indirect * 4096);
		printf("fileinode.s_indirect : %d\n", fileinode.s_indirect);
		for (i = 0; i < new_data_loop_num - 12; i++) {
                        if (i == new_data_loop_num - 13) {
                                remain_size = fileinode.size - ((new_data_loop_num - 1) * 4096);
				printf("remain_size : %d\n", remain_size);
				printf("i : %d\n", i);
				dbitnum = new_indirect->indirect_data_num[i];
				printf("dbitnum : %d\n", dbitnum);
                                memcpy(temp, new_data + (i+12) * 4096, remain_size);
                                pwrite(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
				check += remain_size;
				printf("check : %d\n", check);
			}
			else { 
				printf("i : %d\n", i);
				dbitnum = new_indirect->indirect_data_num[i];
				printf("dbitnum : %d\n", dbitnum);
				memcpy(temp, new_data + (i+12) * 4096, 4096);
		                pwrite(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
				check += 4096;
			}
		}
	}
	else {
		for (i = 0; i < new_data_loop_num; i++) {
			if (i == new_data_loop_num - 1) { // this is end data block
				printf("i : %d\n", i);
				remain_size = fileinode.size - ((new_data_loop_num - 1) * 4096);
				printf("remain_size : %d\n", remain_size);
				dbitnum = fileinode.data_num[i];
				printf("dbitnum : %d\n", dbitnum);
				memcpy(temp, new_data + i * 4096, remain_size);	
				pwrite(_g_fd, temp, remain_size, dtable_location + dbitnum * 4096);
			}
			else {
				printf("i : %d\n", i);
				dbitnum = fileinode.data_num[i];
				printf("dbitnum : %d\n", dbitnum);
				memcpy(temp, new_data + i * 4096, 4096);
				pwrite(_g_fd, temp, 4096, dtable_location + dbitnum * 4096);
			}
		}
	}
    	
	/*
	else {
		if ((4096-offset_remain) >= size_remain) { // we don't need to see many blocks
			sig = pwrite(_g_fd, buf, size, dtable_location + fileinode.data_num[start_block] * 4096 + offset_remain);
		}
		else { // we need to see many blocks
			for (int set = start_block; set < 12; set++) {
				dbitnum[set] = fileinode.data_num[set];
			}
			memcpy(&destbuf, &startbuf, size);
		}
	}
	int sig = pwrite(_g_fd, buf, size, dtable_location + dbitnum * 4096 + offset);
	if (sig  == -1) { // -1 means it does not write buf on the file
		free(super_block);
		free(inode_table);
		printf("WRITE ERROR\n");
		return -1;
	}*/

	inode_table[inode_num] = fileinode;
	lseek(_g_fd, itable_location, SEEK_SET);
        write(_g_fd, inode_table, 8 * 1024 * 512);
	
	free(super_block);
	free(inode_table);
	free(data);
	free(temp);
	free(new_data);
	free(new_indirect);
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

	time_t cur = time(NULL);
	if (ts[0].tv_nsec == UTIME_NOW) {
		(inode_table[inode_num]).atime = cur;
	} else if (ts[0].tv_nsec != UTIME_OMIT) {
		(inode_table[inode_num]).atime = ts[0].tv_sec;
	}
	if (ts[1].tv_nsec == UTIME_NOW) {
		(inode_table[inode_num]).mtime = cur;
	} else if (ts[1].tv_nsec != UTIME_OMIT) {
		(inode_table[inode_num]).mtime = ts[1].tv_sec;
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
        .unlink		= ot_unlink,
        .utimens        = ot_utimens,
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

