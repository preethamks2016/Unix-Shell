#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

size_t bufsize = 100;
char** searchPath;
int count;
int redirect = 0;
int pathCount = 1;
char error_message[30] = "An error has occurred\n";
char* outputFile;


void init() {
    searchPath = (char**) malloc(100*sizeof(char*));
    searchPath[0] = strdup("/bin/");
    return;
}

char* getTrimmedFileName(char* str) {
    const char sep[6] = " \t\n";
    char** arr = NULL;
    char* fileName = strtok(str, sep);
    // printf("output fileName: %s\n", fileName);
    if(strtok(NULL, sep)!=NULL) { // more than 1 file
        return NULL;
    }
    return fileName;
}

bool checkForIf(char* str) {
    const char sep2[6] = " \t";
    char* firstWord = strtok(strdup(str), sep2);

    // printf("firstWord: %s\n", firstWord);
    if ((strcmp(firstWord, "if") == 0)) return true;
    return false;
}

char* splitRedirection(char* str, int* status) {
    bool ifExists = checkForIf(str);
    if (ifExists == true) {
        redirect = 0;
        return str;
    }

    const char sep[2] = ">";
    int splits=0;
    char** arr = NULL;
    char* token = strtok(str, sep);

    while(token != NULL) {
        arr = realloc (arr, sizeof (char*) * ++splits);
        if (splits>2) break;
        if (arr == NULL) exit (-1); // memory allocation failed 
        arr[splits-1] = token;
        token = strtok(NULL, sep);
    }
    // printf("splits: %d\n", splits);
    if(splits==1) 
    {
        redirect = 0; 
        return str;
    }
    else if(splits==2) // return the part after '>'
    {   
        redirect = 1;
        outputFile = getTrimmedFileName(arr[1]);
        // printf("arr[1]: %s\n", arr[1]);
        if (outputFile == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            *status = 1;
        }
        return arr[0];
    } 
    else // error
    {
       write(STDERR_FILENO, error_message, strlen(error_message)); 
       *status = 1;
    }

    return str;
}

char** processString(char* str1, int* status) {
    char ** argv = NULL; // char array input for exec command
    // printf("You typed: '%s'\n", command);

    // check for redirection
    int splitStatus = 0;
    char* str = splitRedirection(str1, &splitStatus);
    if(splitStatus != 0) {
        *status = 1;
        return argv;
    }

    // split string and append tokens to 'argv'
    count = 0;
    char *token;
    const char sep[6] = " \t\n";
    //get the tokens
    token = strtok(str, sep);
    while(token != NULL) {
        argv = realloc (argv, sizeof (char*) * ++count);
        if (argv == NULL) exit (-1); // memory allocation failed 
        // printf( "%s\n", token);
        argv[count-1] = token;
        token = strtok(NULL, sep);
    }
    // realloc one extra element for the last NULL
    argv = realloc(argv, sizeof (char*) * (count+1));
    argv[count] = NULL;

    // printf("count: %d\n", count);
    // printf("printing args - \n");
    // for(int i=0; i<count; i++) {
    //     printf("%s\n", argv[i]);
    // }
    return argv;
}

char* getPath(char** argv) {
    if (access(argv[0], X_OK)==0) return argv[0];
    char* newPath;
    for(int i=0; i<pathCount; i++) {
        //concatenate with path
        newPath = strdup(searchPath[i]);
        strcat(newPath, argv[0]);
        // printf("newPath %d: %s\n", i, newPath);
        if (access(newPath, X_OK)==0) {
            // printf("matched!\n");
            return newPath;
        }
    }
    return argv[0];
}

int executeCommand(char** argv, char* redirectPath) {
    // not built in
    char* path = getPath(argv); // search in paths and modify accordingly

    pid_t pid = fork();
    if (pid == 0) {
        // the child process will execute this
        // check for redirection
        if (redirectPath != NULL) { // change file descriptor
            int fd = open(redirectPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, fileno(stdout));  
        }
        execv(path, argv); 
        // if succeeds, execve should never return. If it returns, it must fail (e.g. cannot find the executable file)
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1); 
    } else {
        // do parent's work
        int status;
        waitpid(pid, &status, 0); // wait for the child to finish its work before keep going
        return WEXITSTATUS(status);
    }
}

void processIfElse(char** argv, int start, int end) {
    // check syntax
    if ((strcmp(argv[start], "if") != 0) || (strcmp(argv[end], "fi") != 0)) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }

    // build if command
    int i=start+1;
    char** cmd=NULL;

    while(i<=end && (strcmp(argv[i], "==") != 0) && (strcmp(argv[i], "!=") != 0)) {
        cmd = realloc (cmd, sizeof(char*) * (i-start));
        cmd[i-(start+1)] = argv[i];
        i++;
    }

    // printf("i: %d. count: %d\n", i, count);
    // if i reaches end -> error
    if(i > end) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }

    cmd[i-(start+1)] = NULL;
    //execute if command
    int out = executeCommand(cmd, NULL);
    // printf("out: %d\n", out);

    if (((strcmp(argv[i], "==") == 0) && out == atoi(argv[i+1])) || ((strcmp(argv[i], "!=") == 0) && out != atoi(argv[i+1]))) {
        if ((strcmp(argv[i+2], "then") == 0)) {
            // execute then
            int j=i+3; // after then
            // printf("j: %d\n", j);

            // recursion
            if (strcmp(argv[j], "if") == 0) {
                // recursive call
                processIfElse(argv, j, end-1);
                return;
            }

            char** cmd1=NULL;
            while((strcmp(argv[j], "fi") != 0)) {
                cmd1 = realloc (cmd1, sizeof (char*) * j);
                cmd1[j-(i+3)] = argv[j];
                j++;
            }
            // if no command after then
            if(j-(i+3)==0) {
                // do nothing
                return;
            }

            cmd1[j-(i+3)] = NULL;

            //built in command
            if (strcmp(cmd1[0], "cd") == 0) {
                if(j-(i+3)==2) {
                    chdir(cmd1[1]);
                } else {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    return;
                }
            } 
            //not built in 
            else {                
                // check for redirection
                char* redirectPath = NULL;
                for(int k=0;k<j-(i+3);k++){
                    if((strcmp(cmd1[k], ">") == 0)) {
                        cmd1[k]=NULL;
                        redirectPath = strdup(cmd1[k+1]);
                    }
                }
                // printf("redirectPath: %s", redirectPath);
                executeCommand(cmd1, redirectPath);
                // executeCommand(cmd1);
            }
        } else {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
    }
    return;
}

int process(char** argv) {
    char* command = argv[0]; // get first word
    // todo: switch case
    //1) exit
    if(strcmp(command, "exit") == 0) {
        // printf("exiting, count:%d\n", count);
        if(count==1) exit(0); // exit the shell
        else {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 0;
        }
    } 
    //2) path
    else if (strcmp(command, "path") == 0) {
        // printf("size: %d\n", args-1); // number of paths
        // update global variable 'searchPath'
        // printf("adding paths..\n");
        for(int i=1; i<count; i++) {
            searchPath[i-1]=strdup(argv[i]); 
            strcat(searchPath[i-1], "/"); //add trailing /
            // printf("%s\n", searchPath[i-1]);
        }
        pathCount = count-1;
        // printf("pathCount: %d\n", pathCount);
    } 
    //3) cd
    else if (strcmp(command, "cd") == 0) {
        if(count==2) {
            chdir(argv[1]);
        } else {
            // printf("144\n");
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 0;
        }
    } 
    // 4) if else
    else if (strcmp(command, "if") == 0) {
        processIfElse(argv, 0, count-1);
    }
    // not a built-in command
    else {
        char* path = getPath(argv); // search in paths and modify accordingly
        pid_t pid = fork();
        if (pid == 0) {
            // the child process will execute this

            // check for redirection
            if (redirect==1) { // change file descriptor
                int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, fileno(stdout));  
            }
            int ret = execv(path, argv); 
            // if succeeds, execve should never return. If it returns, it must fail (e.g. cannot find the executable file)
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1); 
        } else {
            // do parent's work
            int status;
            waitpid(pid, &status, 0); // wait for the child to finish its work before keep going
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    init();

    char* inputFile;
    bool batchMode;
    if (argc==1) {
        batchMode = false;
    } else if (argc==2) {
        // printf("%s\n", argv[1]);
        inputFile = argv[1];
        batchMode=true;
        int fd = open(inputFile, O_RDONLY); //todo: if open fails/ corrupt file. file does not exist
        if (fd==-1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        dup2(fd, fileno(stdin));  
    } else { // more than 1 input file
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    char *string;
    size_t characters;
    while((!batchMode) || (batchMode && ((characters = getline(&string, &bufsize, stdin)) != -1))) {
        if (!batchMode) {
            printf("wish>");
            characters = getline(&string, &bufsize, stdin);
        }
        // printf("command: %s", string);
        char* str = strdup(string); // copy of the command
        // convert string into command
        int status = 0;
        char ** argv = processString(str, &status);
        if (count == 0) continue; // empty line
        if (status != 0) continue; //error in parsing
        // printf("status: %d\n", status);
        // process the command
        int ret = process(argv);
        // printf("processing complete\n");
        //free the memory
        free(argv); 
        free(str);
    }
    free(string);
    return(0);
}

