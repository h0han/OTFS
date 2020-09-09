char* ot_paren(const char* path, char *par_path) {
        int num_file = count(path, '/');
        char* ptr = strtok(path, "/");
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
                printf("dirinode.data_num[%d]: %d\n", i, dirinode.data_num[i]);
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
        char par_path[28] = "";
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
        for (int i = 0;i<12;i++){
                int pdnum = pino.data_num[i];
                if(pdnum ==0){
                        printf("for %dst parent inode's date_num: %d\n", i, pdnum);
                        break;
                }
                pread(_g_fd, (char*) Dir, 4096, 2048+1024*128+512*8*1024 + pdnum * 4096);
                for(int k =0; k<128;k++){
                        printf("parent's Dir->inode_num[]: %d\n", Dir->inode_num[k]);
                        if(Dir->inode_num[k] == inum){
                                Dir->inode_num[k] = 0;
                                strcpy(Dir->name_list[k],"");
                        }
                }
                pwrite(_g_fd, (char*) Dir, 4096, 2048+1024*128+512*8*1024 + pdnum * 4096);
        }
        free(Dir);
        printf("###ot_rmdir end###\n");
        return 0;

}
