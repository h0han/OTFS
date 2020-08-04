static int ot_open(const char *path, struct fuse_file_info *fi)
{
	if (otfind(path) == -1) {	//it means there is no file on the path
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

	super_block = malloc(sizeof(superblock));
	inode_table = malloc(super_block->itable_size);
