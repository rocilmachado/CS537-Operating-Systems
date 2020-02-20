#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<string.h>
#include<fcntl.h>


#define SIMPLE_COMMAND 1
#define BUILTIN_COMMAND 2
#define REDIRECTION_COMMAND 3
#define PARALLEL_COMMANDS 4
#define MULTIPLE_COMMANDS 5

#define EMPTY_LINE 6
#define MAX_PARALLEL_COMMANDS 10000
#define MAX_NUM_PATHS 1000


void printErrorMessage(void);
void parseSpace(char*, char**);
void trimleading(char*);
char* parseRedirection(char* ,char**);
void addPath(char**, char*);
void removePath(char**, char*);
void clearPath(char**);


int builtinCmdHandler(char** parsed, char** path)
{

    int NoOfOwnCmds = 3, i, switchOwnArg = 0;
    char* ListOfOwnCmds[NoOfOwnCmds];

    ListOfOwnCmds[0] = "exit";
    ListOfOwnCmds[1] = "cd";
    ListOfOwnCmds[2] = "path";

    for (i = 0; i < NoOfOwnCmds; i++) {
        if (strcmp(parsed[0], ListOfOwnCmds[i]) == 0) {
            switchOwnArg = i + 1;
            break;
        }
    }

    switch (switchOwnArg) {
    case 1:
	if(parsed[1] != NULL){
		printErrorMessage();
		return 1;
	}
	else
	{
		printf("\n");
		exit(0);
	}
    case 2:
	if(parsed[1] == NULL || parsed[2] != NULL)
	{
		printErrorMessage();
		return 1;
	}else{
        int rc =  chdir(parsed[1]);
	if(rc != 0)
	printErrorMessage();
        return 1;
	}
    case 3:
	if(strcmp(parsed[1],"add") == 0)
		addPath(path,parsed[2]);
	else if(strcmp(parsed[1],"remove") == 0)
		removePath(path,parsed[2]);
	else if(strcmp(parsed[1],"clear") == 0)
		clearPath(path);
	else
		printErrorMessage();
        
        return 1;
    default:
        break;
    }

    return 0;
}

void addPath(char** path, char* newpath)
{
	int numpaths = 0;
	int i;

	for(i = 0 ; i < MAX_NUM_PATHS ; i++)
	{
		if(path[i] == NULL)
			break;

	}

	numpaths = i;

	for(int j = numpaths; j >0; j--)
	{
		path[j] = (char*)malloc(strlen(path[j-1]) * sizeof(char));
		strcpy(path[j],path[j-1]);

	}
	path[0] = (char*)malloc(strlen(newpath)*sizeof(char));
	strcpy(path[0], newpath);

	return;
}

void removePath(char** path, char* rempath)
{
	int numpaths = 0;
	int i,j,k;
	int pathFound = 0;

	for(i = 0; i < MAX_NUM_PATHS; i++)
	{
		if(path[i] == NULL)
			break;
	}

	numpaths = i;

	for(j = 0 ; j < numpaths ; j++)
	{
		if(strcmp(path[j], rempath) ==0)
		{ 
			pathFound = 1;
			break;
		}

	}

	if(pathFound == 0)
		printErrorMessage();

	for(k = j ; k <  numpaths-1; k++)
	{
		path[k] = (char*)malloc(strlen(path[k+1])* sizeof(char) *2);
		strcpy(path[k], path[k+1]);
	}

	path[numpaths-1] = NULL;

	return;

}

void clearPath(char** path)
{
	int i;

	for(i = 0; i < MAX_NUM_PATHS; i++)
	{
		path[i] = NULL;
	}

	return;
}



void init_path(char** path)
{
	path[0] = "/bin";
	path[1] = NULL;
}

int  prompt(char* readbuf)
{
	char* buf = NULL;
	size_t bufsize = 0;
	int bytesread = getline(&buf, &bufsize, stdin);
	if(bytesread == -1) 
	{
		printf("\n");
		exit(0);
	}
	if(strlen(buf)!=0){
	strcpy(readbuf,buf);
	if(strlen(readbuf) == 1 && readbuf[0] == '\n') 
		return 1;
	return 0;
	}
	else
		return 1;
}



void executeSimpleCommandRedirection(char** parsedArgs, char* filename, char** path){
	
	int pathFound = 0;
	char* command;
	pid_t pid = fork();
	
	if(pid < 0){
		printf("\nFailed forking child..");
		return;
	}else if(pid == 0){

		for(int i = 0; i < MAX_NUM_PATHS;i++)
		{
			if(path[i] == NULL)
				break;
			command = (char*)malloc((strlen(path[i])+ 1 + strlen(parsedArgs[0]) +1)* sizeof(char));
			strcpy(command,path[i]);
			strcat(command, "/");
			strcat(command,parsedArgs[0]);
			int rc = access(command, X_OK);
			if(rc == 0){
					pathFound = 1;
					break;
                                }
			free(command);
                }
                if(pathFound == 0){
                        printErrorMessage();
			return;
		}

		if(filename != NULL)
                {      
                        close(STDOUT_FILENO);
                        close(STDERR_FILENO);
                        fopen(filename, "w");
                        fopen(filename,"w");

                }
				
		int exec_rc = execv(command,parsedArgs);
	       	if(exec_rc < 0){
			printErrorMessage();
		}

	}else{
		wait(NULL);
		return;
	}
}

void executeSimpleCommand(char** parsedArgs, char** path){
        int pathFound = 0;
	char* command;

	for(int i = 0; i < MAX_NUM_PATHS;i++)
                {
                        if(path[i] == NULL)
                                break;
			command = (char*)malloc((strlen(path[i])+ 1 + strlen(parsedArgs[0]) +1)*sizeof(char));
                        strcpy(command,path[i]);
                        strcat(command, "/");
                        strcat(command,parsedArgs[0]);
			int rc = access(command, X_OK);
                        if(rc == 0){
                                        pathFound = 1;
                                        break;
                        }
			free(command);
                }
                if(pathFound == 0){
                        printErrorMessage();
			return;
		}


	pid_t pid = fork();
        if(pid < 0){
                printf("\nFailed forking child..");
                return;
        }else if(pid == 0)
	{

                int exec_rc = execv(command,parsedArgs);
                if(exec_rc < 0){
                        printErrorMessage();
                }
        }else{
                wait(NULL);
                return;
        }
}
void executeParallelCommands(char **singlecmds, char** path)
{
	char* parsedArgs[MAX_PARALLEL_COMMANDS];
	int pathFound = 0;
	char* command;
	pid_t pid, wpid;
	int status = 0;
	int i;

	for(i = 0 ; i  < MAX_PARALLEL_COMMANDS ; i++)
	{
		char* rdrfile;

		if(singlecmds[i] == NULL)
		{
			break;
		}

		if(strchr(singlecmds[i], '>') != NULL)
		{
                   rdrfile = parseRedirection(singlecmds[i],parsedArgs);

                }

		else

		{parseSpace(singlecmds[i], parsedArgs);}

		for(int i = 0; i < MAX_NUM_PATHS;i++)
                {
                        if(path[i] == NULL)
                                break;
                        command = (char*)malloc(strlen(path[i])+ 1 + strlen(parsedArgs[0]) +1);
                        strcpy(command,path[i]);
                        strcat(command, "/");
                        strcat(command,parsedArgs[0]);
                        int rc = access(command, X_OK);
                        if(rc == 0){
                                        pathFound = 1;
                                        break;
                        }
                        free(command);
                }
                if(pathFound == 0){
                        printErrorMessage();
                        return;
                }


		pid = fork();

		if(pid < 0){
			printf("\nFailed forking child.....");
			return;
		}
	       	if(pid == 0){
			if(rdrfile != NULL)
                	{       
                        	close(STDOUT_FILENO);
                        	close(STDERR_FILENO);
                        	fopen(rdrfile, "w");
                        	fopen(rdrfile,"w");

                	}


			int exec_rc = execv(command,parsedArgs);
			if(exec_rc < 0){
				printErrorMessage();
			}

	}
	}
	while ((wpid = wait(&status)) > 0);
	return;
}

void parseSpace(char* str, char** parsed)
{
    int i= 0;
    char* str_copy = strdup(str);
    char* p = strsep(&str_copy, "\t\n ");

    while(p != NULL)  {

        if (strlen(p) > 0)
	{ 
		parsed[i] = p;
	       	i++;
	}
	p = strsep(&str_copy, "\t\n ");
    }
    parsed[i] = NULL;
    return;
  }

int parseMultipleCommands(char* cmdline, char** cmds) {
        int num_cmds = 0;
        char* token = strtok(cmdline, ";\n");
        while(token!=NULL) {
                cmds[num_cmds++] = token;
                token = strtok(NULL, ";\n");
        }
        return num_cmds;
}

int parseParallelCommands(char* cmdline, char** singlecmds)
{
	int num_cmds = 0;
	char* token = strtok(cmdline, "&\n");
	while(token != NULL){
		if(token[0] != '\n')
		singlecmds[num_cmds++] = token;
		token = strtok(NULL, "&\n");
		
	}
	return num_cmds;
}

char* parseRedirection(char* toBeParsed,char** parsedArgs)
{
	int num_cmds = 0;
	char* parsedArgOpts;
	char* singlecmds[1000];
	char* token = strtok(toBeParsed, ">");
	char* filename[2];
	while(token != NULL){
                if(token[0] != '\n')
                singlecmds[num_cmds++] = token;
                token = strtok(NULL, ">");
        }
	if(num_cmds > 2 )
		printErrorMessage();
	parsedArgOpts = singlecmds[0];
	parseSpace(parsedArgOpts, parsedArgs);
	parseSpace(singlecmds[1], filename);
	int len = strlen(filename[0]);
	printf("strlen is %d\n",len);
        if(filename[0][len-1] == '\n') filename[0][len-1] = '\0';
	return filename[0];	
}

void printErrorMessage(){
	char error_message[30] = "An error has occurred\n";
	write(STDERR_FILENO, error_message, strlen(error_message));
}

int main(int argc, char** argv){

	char* cmds[512];
	int numcmds  = 0;
	char* path[MAX_NUM_PATHS];
	char* parsedArgs[100];
	char* singlecmds[100];
	size_t bufsize = 0;
	char* inputline =(char*) malloc(bufsize * sizeof(char));
	int status= 1;
	int rederr = 0;
	
	init_path(path);

	if(argv[1])
	{
		FILE* f = fopen(argv[1], "r");
		if(f == NULL)
		{
			printErrorMessage();
			exit(1);
		}

		while(getline(&inputline,&bufsize,f) != -1)
		{
			if(strlen(inputline) == 1 && inputline[0] == '\n')
				continue;

			if(strchr(inputline, ';') != NULL)
				{//There are multiple commands separated by ;
					numcmds = parseMultipleCommands(inputline, cmds);


					for(int  curcmd = 0; curcmd < numcmds; curcmd++)
					{		
						if(strchr(cmds[curcmd],'&') != NULL)
							{// Parallel commands
								parseParallelCommands(cmds[curcmd], singlecmds);
								executeParallelCommands(singlecmds, path);

							}
						else
							{
								char* rdrfile = NULL;
								if(strchr(cmds[curcmd], '>') != NULL)
								{
									rdrfile = parseRedirection(cmds[curcmd], parsedArgs);
									if(rdrfile == NULL)
									{
										rederr = 1;
										printErrorMessage();
										break;
									}
								}
								else
								{
									parseSpace(cmds[curcmd], parsedArgs);
								}

								if(parsedArgs[0][0] == '\0') //Empty line
								{
			
									continue;
								}
								
								if(!builtinCmdHandler(parsedArgs, path))
								{

									if(rdrfile != NULL)
										executeSimpleCommandRedirection(parsedArgs, rdrfile, path);
									else
										executeSimpleCommand(parsedArgs, path);
								}	

							}

					}
				if(rederr == 1)
				{
					rederr = 0;
					continue;
				}	
				}
			else{
				if(strchr(inputline, '&') != NULL)
				{
					//Parallel commands
	
					parseParallelCommands(inputline, singlecmds);
					executeParallelCommands(singlecmds,path);

				}

				else
                         	{
					char* rdrfile = NULL;
					if(strchr(inputline, '>') != NULL)
					{
						rdrfile = parseRedirection(inputline,parsedArgs);
						if(rdrfile == NULL)
						{
							printErrorMessage();
							continue;
						}
					}
					else
					{
						parseSpace(inputline,parsedArgs);
					}


                               
                               	 	if(parsedArgs[0][0] == '\0')//Empty line i.e. whitespaces followed by a newline
                                	{      
                                        	continue;  //Go to prompt
                                	}
                                	if(!builtinCmdHandler(parsedArgs,path))
                                	{
						if(rdrfile != NULL)
							executeSimpleCommandRedirection(parsedArgs, rdrfile, path);
						else
							executeSimpleCommand(parsedArgs,path);
                                	}
				}	
			}

		}
	}

	else{

	do{
		////////////////////////////////////////////PROMPT////////////////////////////////////////////////////////////////////////////////////////////
		printf("smash> ");
		fflush(stdout);
		if(prompt(inputline)){
			//printf("%s\n",inputline);
			continue;
		}	

		if(strchr(inputline, ';') != NULL)
		
		{//There are multiple commands separated by ;
			numcmds = parseMultipleCommands(inputline, cmds);
			//printf("The number of commands is %d : ", numcmds);

			for(int  curcmd = 0; curcmd < numcmds; curcmd++)
			{
				if(strchr(cmds[curcmd],'&') != NULL)
						{// Parallel commands
							parseParallelCommands(cmds[curcmd], singlecmds);
							executeParallelCommands(singlecmds, path);

						}
				else
				{
					char* rdrfile = NULL;
					if(strchr(cmds[curcmd], '>') != NULL)
					{
						rdrfile = parseRedirection(cmds[curcmd], parsedArgs);
					}
					else
					{
						//printf("Correct Path\n");
						parseSpace(cmds[curcmd], parsedArgs);
					}

					if(parsedArgs[0][0] == '\0') //Empty line
					{
						//emptyline = 1;
						continue;
					}
					if(!builtinCmdHandler(parsedArgs, path))
					{
						//printf("Executing command !!!!!\n");
						if(rdrfile != NULL)
							executeSimpleCommandRedirection(parsedArgs, rdrfile, path);
						else
							executeSimpleCommand(parsedArgs, path);
					}

				}

			}
		}
		else{
			if(strchr(inputline, '&') != NULL)
			{
				//Parallel commands
				parseParallelCommands(inputline, singlecmds);
				executeParallelCommands(singlecmds, path);

			}

			else
                         {
				char* rdrfile = NULL;
				if(strchr(inputline, '>') != NULL)
				{
					rdrfile = parseRedirection(inputline,parsedArgs);
				}
				else
				{
					parseSpace(inputline,parsedArgs);
				}


                                if(parsedArgs[0][0] == '\0')//Empty line i.e. whitespaces followed by a newline
                                { 
                                        continue;  //Go to prompt
                                }
                                if(!builtinCmdHandler(parsedArgs, path))
                                {
					if(rdrfile != NULL)
						executeSimpleCommandRedirection(parsedArgs, rdrfile, path);
					else
						executeSimpleCommand(parsedArgs,path);
                                }
			}
		}
		}while(status);
	}

	printf("\n");
	return 0;
}
