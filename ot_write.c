#include <unistd.h>

static int ot_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
        int inode_num = fi->fh;
        //if (inode_num = (otfind(path)) == -1) {
        //        return -1; // There is no file on the path
        // }

        superblock* super_block;
        inode* inode_table;
        super_block = malloc(1024);
        inode_table = malloc(8 * 1024 * 512);
        inode fileinode;
	time_t atime;
	time_t ctime;
	time_t mtime;

        // adtl_size = malloc(sizeof(size));
        fileinode = inode_table[inode_num];

	int sig = pwrite(_g_fd, buf, size, offset);
	if (sig  == -1) { // -1 means it does not write buf on the file
		printf("WRITE ERROR");
		return -1;
	}

	fileinode.atime = time(NULL);
	fileinode.mtime = time(NULL);
	fileinode.ctime = time(NULL);
	
	return 0;	
}

