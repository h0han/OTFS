static int ot_open(const char *path, struct fuse_file_info *fi)
{
	int inode_num;
	if ((inode_num = otfind(path)) == -1) {	//it means there is no file on the path
		return -1;
	}
	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0) {
		printf("Error\n");
		return 0;
	}

	superblock* super_block;
	inode* inode_table;
	ibitmap* inode_bitmap;
	dbitmap* data_bitmap;

	super_block = malloc(1024);
	inode_table = malloc(8 * 1024 * 512);

	lseek(fd, 0, SEEK_SET);
	read(fd, super_block, 1024); // superblock load

	long itable_location = super_block->sb_size + super_block->ibitmap_size + super_block->dbitmap_size;

	lseek(fd, itable_location, SEEK_SET);
	read(fd, inode_table, 8 * 1024 * 512); // itable load

	inode temp;
	inode temp = inode_table[inode_num];

	Inode.atime = time(NULL); // reinitializing atime

	close(fd);
	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	free(inode_table);

	return Inode.FB[data_num[inode temp]]
}
