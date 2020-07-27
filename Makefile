all : OTFS

OTFS : OTFS_200727.o bitmap.o
	gcc -D_FILE_OFFSET_BITS=64 -g -o OTFS OTFS_200727.o bitmap.o

OTFS_200727.o : OTFS_200727.c
	gcc -D_FILE_OFFSET_BITS=64 -c OTFS_200727.c

bitmap.o : bitmap.c
	gcc -D_FILE_OFFSET_BITS=64 -c bitmap.c

clean : 
	rm OTFS
