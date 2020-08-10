static int ot_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int inode_num;
	if (inode_num = (otfind(path)) == -1) {
		return -1; // There is no file on the path
	}

	int fd;
	char region[10] = "region";
	if ((fd = open(region, O_RDWR|O_CREAT, 0666)) < 0) {
		printf("Error\n");
		return -2;
	}

	superblock* super_block;
	inode* inode_table;
	super_block = malloc(1024);
	inode_table = malloc(8 * 1024 * 512);
	inode fileinode;

	size = malloc(sizeof(size))
	fileinode = inode_table[inode_num]	
}
