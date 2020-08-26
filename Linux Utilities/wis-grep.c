#include<stdio.h>
#include<stdlib.h>
#include<string.h>



int main(int argc, char** argv)
{

if(argc == 1)
{
	printf("wis-grep: searchterm [file …]\n");
	exit(1);
}

char* searchterm = argv[1];

char* rdbuf = NULL;
size_t bufsize = 0;
size_t numchars;

rdbuf = (char*)malloc(bufsize * sizeof(char));

//If buffer is not allocated succesfully, exit with status 1
if(rdbuf == NULL)
{
        printf("Buffer allocation failed, not enough system memory");
        exit(1);
}

if(argc == 2)
{
	//Read file from standard input, use getline() with stdin
	while ((numchars = getline(&rdbuf, &bufsize, stdin)) != -1) {

               if(strstr(rdbuf,searchterm) != NULL)
               {
                       printf("%s", rdbuf);
               }
           }

}
for (int i = 2 ; i < argc ; i++)
{

	char* filename = argv[i];
	//Open the file and read lines from the file

	FILE* f = fopen(filename, "r");

	if(f == NULL)
	{
		printf("wis-grep: cannot open file\n");
		exit(1);	
	}

  	// Loop through until we are done with the file
  	while ((numchars = getline(&rdbuf, &bufsize, f)) != -1) {
   
	       if(strstr(rdbuf,searchterm) != NULL)
	       {
		       printf("%s", rdbuf);
	       }
           }

	fclose(f);
}
free(rdbuf);
return 0;
}


