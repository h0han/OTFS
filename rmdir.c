char* paren(const char* path){
        char pfp[28];
        strcpy(pfp, path);
        char* ptr;
        char* old[30];
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
        char* new;
        strcat(new, "/");
        while((strcmp(old[j], last))){
                strcat(new, old[j]);
                strcat(new, "/");
                j++;
        }
        return new;
}

static int ot_rmdir(const char* path){

        int inum = otfind(path);
        inode ino;

        int fd = open("region", O_RDWR|O_CREAT, 0666);
        chanb(fd, inum, 0);
        lseek(fd, 2048+1024*128, SEEK_SET);
        lseek(fd, inum, SEEK_CUR);
        read(fd, &ino, 512);
        //path is directory

        for(int i = 0; i<12;i++){
                if(ino.DB[i] != NULL){
                return -1; // error: there are files in dirctory
                }
        }
        for(int j = 0; j<12;j++){
                chanb(fd, ino.data_num[j], 1);
        }

        lseek(fd, 512, SEEK_CUR);
        write(fd, &ino, 512);

        //touch parents's data
        inode* pino;
        int pinum = otfind(paren(path));
        lseek(fd, 2048+1028*128, SEEK_SET);
        lseek(fd, pinum, SEEK_CUR);
        read(fd, pino, 512);
        for (int i = 0; i <= (pino->size / 4096); i++) {
                for (int j = 0; j < 128; j++) {
                        if (((pino->DB[i])->inode_num[j])== inum) {
                                strcpy((pino->DB[i])->name_list[j],"");
                                (pino->DB[i])->inode_num[j] = 0;
                        }
                }
        }
        close(fd);
}

