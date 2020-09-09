static int ot_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int inode_num = fi->fh;
	if ((inode_num = otfind(path)) != -1) { // There is same file in the path
		return -EEXIST;
	}

	//int fd;
	//char region[10] = "region";
	//if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0) {
	//	printf("Error\n");
	//	return 0;
	//}

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
	int new_dbitnum = checkb(fd, 1);
	chanb(fd, new_dbitnum, 1);

	// set new inode
	new.filename = malloc(28);
	new.size = 4096;
	new.inode_num = new_ibitnum;
	new.data_num = new_dbitnum;
	new.file_or_dir = 1;

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
	bool loop_escape;
	Dir_Block* Dir = (Dir_Block*) malloc (4096);

	inode fileinode;
	// find dir with path
	for (h = 0; h < num_file; h++) {
		ptr = strtok(temp, "/");
		if (root_check == 0) {
			// access to root
			inode_num = 0;
			fileinode = inode_table[0];
		}
		else {
			fileinode = inode_table[inode_num];
		}
		for (i = 0; i <= (fileinode.size / 4096); i++) { // Set Dir directory
			loop_escape = true;
			dbit_num = fileinode.data_num[i];
			pread(fd, (char*) Dir, 4096, dtable_location + dbit_num); // dbit_num is offset
			for (j = 0; j < 128; j++) {  // find ptr directory in Dir
				if ((strcmp((Dir->name_list[j]), ptr)) == 0) {
					inode_num = Dir->inode_num[j];
					fileinode.ctime = time(NULL);
					fileinode.mtime = time(NULL);
					fileinode.atime = time(NULL);
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

	//close(fd);
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(new_dir);
	free(Dir);
	return 0;

}
