#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int checkb(int fd, int iorb);
void chanb(int fd, int num, int iorb);
int onoffcheck(int fd, int ino, int iorb);
