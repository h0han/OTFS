#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>

char* ot_paren(const char* path) {
	char par_path[100] = "";
	char* ptr = strtok(path, '/');
	int num_file = count(path, '/');
	for (int i = 0; i < num_file; i++) {
		strcat(par_path, '/');
		strcat(par_path, ptr);
		ptr = strtok(NULL, '/');
	}

	return par_path;
}


