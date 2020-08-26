#include<stdio.h>
#include<stdlib.h>
#include<string.h>

int main(int argc, char** argv){
	if(argc < 2)
	{
		printf("wis-untar: tar-file\n");
		exit(1);
	}

	FILE* tarfile = fopen(argv[1], "r");
	char namebuf[100];
	long filesize;
	
	if(tarfile == NULL){ //No tarfile name input given by the user 
		printf("wis-untar: cannot open file\n");
		exit(1);
	}
	while(1)
	{
		//Extract the name of the constituent file
		size_t numbytes = fread(namebuf, 100, 1, tarfile);
		if(numbytes == 0)
		{
			break;
		}

		
		//Extract the size of the constituent file
		fread(&filesize, 8, 1, tarfile);
				
		//Extract the contents of the constituent file
		char* content = (char*)malloc(filesize * sizeof(char));
		fread(content, filesize, 1, tarfile);
		
		//Create a new file to write
		FILE* f = fopen(namebuf, "w");
		if(f == NULL)
		{
			printf("Couldn't open the file for writing\n");
			exit(1);
		}

		//Write the contents into the new file with the name in namebuf
		fwrite(content,filesize, 1,f);

		fclose(f);

		free(content);
	}

	fclose(tarfile);
	
	return 0;
}








