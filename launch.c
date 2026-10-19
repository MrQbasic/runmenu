#include "launch.h"

#include <stdlib.h>
#include <stdio.h>

void launchFromPath(char* path){
    char* cmd;
    int length = snprintf(NULL, 0, "fish -c '%s' &", path);
    cmd = (char*) malloc(length+1);
    snprintf(cmd, length+1, "fish -c '%s' &", path);

    printf("CMD: %s\n", cmd);

    system(cmd);

    exit(0);
}