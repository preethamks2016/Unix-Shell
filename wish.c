#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

size_t bufsize = 100;
char** searchPath;
int count;
int pathCount = 1;
char error_message[30] = "An error has occurred\n";


void init() {
    searchPath = (char**) malloc(100*sizeof(char*));
    searchPath[0] = strdup("/bin");
    return;
}

char** processString(char* str) {
    char ** argv = NULL; // char array input for exec command
    // printf("You typed: '%s'\n", command);

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

    printf("count: %d\n", count);
    printf("printing args - \n");
    for(int i=0; i<count; i++) {
        printf("%s\n", argv[i]);
    }
    return argv;
}

void modifyPath(char** argv) {
    if (access(argv[0], X_OK)==0) return;
    char* newPath;
    for(int i=0; i<pathCount; i++) {
        //concatenate with path
        newPath = strdup(searchPath[i]);
        strcat(newPath, argv[0]);
        printf("newPath %d: %s\n", i, newPath);
        if (access(newPath, X_OK)==0) {
            printf("matched!\n");
            argv[0] = newPath;
            return;
        }
    }
    return;
}

void process(char** argv) {
    char* command = argv[0]; // get first word
    // todo: switch case
    //1) exit
    if(strcmp(command, "exit") == 0) {
        exit(0); // exit the shell
    } 
    //2) path
    else if (strcmp(command, "path") == 0) {
        // printf("size: %d\n", args-1); // number of paths
        // update global variable 'searchPath'
        printf("adding paths..\n");
        for(int i=1; i<count; i++) {
            searchPath[i-1]=strdup(argv[i]); //todo: add trailing /
            printf("%s\n", searchPath[i-1]);
        }
        pathCount = count-1;
        printf("pathCount: %d\n", pathCount);
    } 
    //3) cd
    else if (strcmp(command, "cd") == 0) {
        if(count==2) {
            chdir(argv[1]);
        } else {
            printf("%s", error_message);
        }
    } else {
        // not a built-in command
        modifyPath(argv); // search in paths and modify accordingly
        pid_t pid = fork();
        if (pid == 0) {
            // the child process will execute this
            int ret = execv(argv[0], argv); 
            // if succeeds, execve should never return. If it returns, it must fail (e.g. cannot find the executable file)
            printf("Failed to execute %s\n", argv[0]);
            exit(1); 
        } else {
            // do parent's work
            int status;
            waitpid(pid, &status, 0); // wait for the child to finish its work before keep going
        }
    }
}

int main()
{
    char *string;
    init();
    int n=3;
    while(1) {
        printf("wish>");
        size_t characters = getline(&string, &bufsize, stdin);
        printf("%s", string);
        char* str = strdup(string); // copy of the command
        // convert string into command
        char ** argv = processString(str);
        // process the command
        process(argv);
        //free the memory
        free(argv); 
        free(str);
    }
    free(string);
    return(0);
}

