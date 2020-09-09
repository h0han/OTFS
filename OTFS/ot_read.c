static int ot_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int inode_num = fi->fh;

	superblock* super_block;
	inode* inode_table;
	super_block = malloc(1024);
	inode_table = malloc(8 * 1024 * 512);
	inode fileinode;
	
	time_t atime;
	time_t mtime;
	time_t ctime;

	fileinode = inode_table[inode_num];

	int sig = pread(_g_fd, buf, size, offset);
	if (sig == -1) {
		printf("Error");
		return -1;
	
	fileinode.atime = time[NULL];
	return 0;
}
