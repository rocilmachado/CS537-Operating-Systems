#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>

int main(int argc, char** argv){


if(argc < 3)
{
	printf("wis-tar: tar-file file […]\n");
	exit(1);
}

char* tarfile = argv[1];
char* filename;
FILE* curfile;
char dest[100];
long  filesize;

FILE* f = fopen(tarfile, "w");

for(int i = 2; i < argc ; i++){
	
	filename = argv[i];

	//Open the file that should be a part of the tar file
	curfile = fopen(filename, "r");

	if(curfile == NULL){ // file doesn't exist
		printf("wis-tar: cannot open file\n");
		exit(1);
	}

	struct stat info;
   	int err = stat(filename, &info);
    	// Do error checking here!
	if(err != 0)
	{
		printf("stat command failed\n");
		exit(1);
	}
	
	filesize = info.st_size;

	//Print the filename in ASCII
	strncpy(dest, filename, 100);
	
	fwrite(dest, sizeof(dest), 1, f);

	//Print filename in binary
	fwrite(&filesize, 8, 1, f);

	fseek(curfile, 0, SEEK_SET);

	char* rdbuf = (char*)malloc(filesize * sizeof(char));


	//Read the contents from the current constituent file and write them to the tarfile
	fread(rdbuf, filesize, 1, curfile);
	fwrite(rdbuf, filesize, 1, f);

	fclose(curfile);
	free(rdbuf);
	
}

fclose(f);

return 0;

}
