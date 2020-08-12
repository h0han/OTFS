char* paren(const char* path){
        char pfp[28];
        strcpy(pfp, path);
        char* ptr;
        char* old[28];
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
        char* new = malloc(28);
        while((strcmp(old[j], last))){
                strcat(new, "/");
                strcat(new, old[j]);
                j++;
        }
        if ((strcmp(new,"")==0)){
                strcpy(new, "/");
        }
        return new;
}



static int ot_rmdir(const char* path){
        printf("###ot_rmdir start###\n");
        int inum = otfind(path);
        printf("%s's inode num: %d\n", path, inum);
        inode* inode_table = malloc(8*512*1024);
        inode dirinode;

        //chanb(_g_fd, inum, 0);
        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        read(_g_fd, inode_table, 1024*8*512);
        dirinode = inode_table[inum];

        //path is directory

        for(int i = 0; i<12;i++){
                printf("dirinode.data_num[%d]: %d\n", i, dirinode.data_num[i]);
        //}
        //for(int i = 1;i<12;i++){ //there is a data about dirctory(itself) in data_num[0] 
                if((i!=0)&&(dirinode.data_num[i]!=0)){
                        return -ENOTEMPTY; // error: there are files in dirctory
                }
        //}
        //for(int j = 0; j<12;j++){
                chanb(_g_fd, dirinode.data_num[i], 1);
        }
        inode_table[inum] = dirinode;
        lseek(_g_fd, 2048+1024*128, SEEK_SET);
        write(_g_fd, inode_table, 1024*8*512);
        /*
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
        printf("###ot_rmdir end###\n");
        return 0;
        */
}
