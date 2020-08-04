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

	super_block = malloc(1024);
	inode_table = malloc(8 * 1024 * sizeof(inode);
	ibitmap = malloc(sizeof(ibitmap));

	lseek(fd, 0, SEEK_SET);
	read(fd, super_block, 1024);

	lseek(fd, 1024, SEEK_SET);
	read(fd, inode_bitmap, 1024);

	long itable_location = super_block-sb_size + super_block->ibitmap_size + super_block->dbitmap_size;

lseek
