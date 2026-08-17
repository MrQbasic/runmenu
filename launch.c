#include "launch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <spawn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#define BASH "/bin/bash"

void launchFromPath(char* path){
    //get file extension
    char* extension = path;
    while(1){
        char c = extension[0];
        if(c == '.') break;
        extension++;
    }
    extension++;
    //do the fork
    pid_t pid = fork();
    if (pid < 0) {
        printf("FORK failed!\n");
        exit(1);
    }

    if (pid == 0) {
        //Detach file
        setsid();
        //redirect stdio
        int devnull = open("/dev/null", O_RDWR);
        if (devnull < 0) return;
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
        //launch depending file extension
        if(strcmp(extension, "sh") == 0){
            char* argv[] = {BASH, path, NULL}; 
            execvp(argv[0], argv);
        }else{
            char* argv[] = {path, NULL}; 
            execvp(path, argv);
        }
        //unexpected only on error
        fprintf(stderr, "EXEC failed: %s\n", strerror(errno));
        exit(127);
    }

    exit(0);
}