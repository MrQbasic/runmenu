#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include <spawn.h>

#include "dirs.h"
#include "draw.h"

#define BASH "/bin/bash"

#define USERINPUT_LENGTH 128


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
            execvp(path, NULL);
        }
        //unexpected only on error
        fprintf(stderr, "EXEC failed: %s\n", strerror(errno));
        exit(127);
    }

    exit(0);
}


int getSubTypeCount(DIR* dir, unsigned char type){
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') continue;
        if(entry->d_type == type) count++;
    }
    rewinddir(dir);
    return count;
}


//return is count of dirs
void getDirList(DIR* dir, DirEntry* root){

    //alloc space for all the sub ptrs
    root->entries  = getSubTypeCount(dir, DT_DIR);
    root->entries += getSubTypeCount(dir, DT_REG);
    root->entries += getSubTypeCount(dir, DT_LNK);

    printf("%s: files: %d\n", root->path, root->entries);

    root->nextEntry = (DirEntry**) malloc(sizeof(DirEntry*) * (root->entries));

    //scan the dir
    struct dirent *entry;
    int file_index = 0;

    while ((entry = readdir(dir)) != NULL) {
        //filter out all hidden folders
        if(entry->d_name[0] == '.') continue;

        switch (entry->d_type){
            case DT_DIR:
                //check if the array is full
                if(file_index == root->entries){
                    printf("ERROR: Unexpected\n");
                    exit(1);
                }
                //create an entry for it 
                DirEntry* e1 =(DirEntry*) malloc(sizeof(DirEntry));
                //type
                e1->isDirectoy = true;
                //name
                e1->name = (char*) malloc(strlen(entry->d_name) + 1);
                strcpy(e1->name, entry->d_name);
                //substructs
                e1->nextEntry = NULL;
                e1->parent = root;
                e1->isSelected = false;
                //append to the list
                root->nextEntry[file_index] = e1;
                
                //concat rootpath and name to get the new path
                int newpath_len = strlen(root->path) + strlen(entry->d_name)+2;
                e1->path = (char*) malloc(sizeof(char) * newpath_len); 
                snprintf(e1->path, newpath_len, "%s/%s", root->path, entry->d_name);

                //recursivly fill the dirs
                DIR *dir = opendir(e1->path);
                if(dir != NULL){
                    getDirList(dir, e1);
                }
                
                file_index ++;
                break;

            case DT_LNK:
            case DT_REG:
                //check if the array is full
                if(file_index == root->entries){
                    printf("ERROR: Unexpected\n");
                    exit(1);
                }
                //create an entry for it 
                DirEntry* e2 =(DirEntry*) malloc(sizeof(DirEntry));
                //type
                e2->isDirectoy = false;
                e2->isSelected = false;
                //name
                e2->name = (char*) malloc(strlen(entry->d_name) + 1);
                strcpy(e2->name, entry->d_name);
                //substructs
                e2->nextEntry = NULL;
                e2->parent = root;
                e2->entries = 0;
                //concat rootpath and name to get the new path
                newpath_len = strlen(root->path) + strlen(entry->d_name)+2;
                e2->path = (char*) malloc(sizeof(char) * newpath_len); 
                snprintf(e2->path, newpath_len, "%s/%s", root->path, entry->d_name);
                //check if its a link
                char* newPath = (char*) malloc(sizeof(char) * 1024);
                ssize_t linkPathLength = readlink(e2->path, newPath, sizeof(char) * 1023);
                if(linkPathLength != -1){
                    newPath[linkPathLength] = '\0';
                    free(e2->path);
                    e2->path = newPath;
                }
                //append to the list
                root->nextEntry[file_index] = e2;
                file_index ++;
                break;

            default:
                printf("WARN: Unhandled file type detected: %s\n", entry->d_name);
                break;
        }
    }
}


DirEntry* lastDir;

//returns the current open entry (NULL if nothing changed)
DirEntry* handleDirList(DirEntry* dir, int* cursor, bool* select, bool* back){
    //speed up
    if(*select == false && *back == false) return NULL;
    //unecpected case due to nature of event handeling
    if(*select && *back){
        *select = false;
        *back = false;
    }

    //go through all entried
    for(int i=0; i<dir->entries; i++){
        //get the entry
        DirEntry* entry = dir->nextEntry[i];
        //check if its a selected dir
        if(entry->isSelected){
            return handleDirList(entry, cursor, select, back);
        }
        //check if we are at the end of the branch
        if(dir->hasSelected) continue;
        //check if we are on the selected entry
        if(i != *cursor) continue;
        //check if we have to select
        if(*select){
            if(entry->isDirectoy){
                entry->isSelected = true;
                entry->hasSelected = false;
                dir->hasSelected = true;
                dir->selectedChild = i;
                *select = false;
                return entry;

            }else{
                launchFromPath(entry->path);
                return NULL;
            }
        }
        //check if we go back
        if(*back){
            dir->isSelected = false;
            *back = false;
            if(dir->parent != NULL){
                dir->parent->hasSelected = false;
                *cursor = dir->parent->selectedChild;
                return dir->parent;
            }
            return dir;
        }
    }
    //only runs when the head folder has no entries
    *back = false;
    *select = false;
    dir->parent->hasSelected = false;
    dir->isSelected = false; 
    return dir->parent;
}






void createDir(DirEntry* parent, char* dirName, int inputLength){
    //check if we have a name
    if(inputLength == 0){
        setMessage(MSG_WARN, "Please input a valid name into the main bar!");
        return;
    }
    //construct a new entry
    DirEntry* entry = malloc(sizeof(DirEntry));
    entry->name = (char*) malloc(strlen(dirName));
    strcpy(entry->name, dirName);
    entry->parent = parent;
    entry->isDirectoy = true;
    entry->hasSelected = false;
    entry->isSelected = true;
    entry->nextEntry = NULL;
    entry->selectedChild = 0;
    entry->entries = 0;
    int pathLength = strlen(dirName) + 2 + strlen(parent->path);
    entry->path = (char*) malloc(sizeof(char) * pathLength);
    snprintf(entry->path, sizeof(char) * pathLength, "%s/%s", parent->path, dirName);
    //add to parent
    DirEntry** newList = (DirEntry**) malloc(sizeof(DirEntry*) * (parent->entries + 1));
    memcpy(newList, parent->nextEntry, parent->entries * sizeof(DirEntry*));
    free(parent->nextEntry);
    parent->nextEntry = newList;
    parent->entries++; 
    parent->nextEntry[parent->entries-1] = entry;
    parent->hasSelected = true;
    parent->selectedChild = parent->entries - 1;
    //add to file system
    char buf[1024];
    messageType type;
    if(mkdir(entry->path, 0755) != 0){
        snprintf(buf, sizeof(buf), "Could not create dir: %s", entry->path);
        type = MSG_ERROR;
    }else{
        snprintf(buf, sizeof(buf), "New dir: %s", entry->path);
        type = MSG_INFO;
    } 
    setMessage(type, buf);
}

//void createLink(void path)


int main(void) {
    setupWindow();

    //----------------------------------------------------------------------
    // scanning for apps and shortcuts in cfg dir
    const char *path_home = getenv("HOME");
    if (path_home == NULL) {
        printf("ERROR: end var HOME not set!\n");
        return 1;
    }

    //concat the home path and the folder path
    char path[1024];
    int path_len = snprintf(path, sizeof(path), "%s/.config/runmenu", path_home); //TODO handle the overflow
    //int path_len =snprintf(path, sizeof(path), "%s", path_home); //TODO handle the overflow

    //create the cfg dir
    DIR *dir = opendir(path);
    if(dir == NULL){
        printf("WARNING: config dir not found!: %s\n", path);
        if(mkdir(path, 0755) == 0){
            dir = opendir(path);
            if(dir == NULL){
                printf("ERROR: Unexpected ERROR :(\n");
                return 1;
            }else{
                printf("Created new dir.\n");
            }
        }else{
            if(errno == EEXIST) {
                printf("ERROR: Unexpected ERROR  \n");
            }else{
                printf("ERROR: Failed to create dir %s\n", path);
                printf("ERROR: %d\n", errno);
                return 1;
            }
        }
    }

    DirEntry rootDir;
    rootDir.path = (char*) malloc(sizeof(char) * path_len);
    strcpy(rootDir.path, path);
    rootDir.nextEntry = NULL;
    rootDir.isDirectoy = true;
    rootDir.hasSelected = false;
    rootDir.isSelected = false;
    rootDir.parent = NULL;
    rootDir.entries = 0;
    getDirList(dir, &rootDir);


    //main loop
    XEvent event;

    char userinput[USERINPUT_LENGTH] = "";
    int userinput_cursor = 0;

    int line_cursor = 0;
    
    DirEntry* currentEntry = &rootDir;

    while (1) {
        XNextEvent(display, &event);

        bool select = false;
        bool back = false;

        switch (event.type) {
            case KeyPress:
                //handle typing
                KeySym keysym;
                char buf[32];
                int len = XLookupString(&event.xkey, buf, sizeof(buf) - 1, &keysym, NULL);

                //check if its a command or not
                if(event.xkey.state & ControlMask){
                    switch(keysym){
                        case XK_N:
                        case XK_n:
                            createDir(currentEntry, userinput, userinput_cursor);
                            break;

                        default:
                            //unexpected input
                            break;
                    }
                }else{
                    //check for special keys
                    switch (keysym){
                        case XK_Escape:
                            //close the window
                            XCloseDisplay(display);
                            return 0;
                            break;

                        case XK_BackSpace:
                            //erase the last char if there is one
                            if(userinput_cursor > 0){
                                userinput_cursor--;
                                userinput[userinput_cursor] = '\0';
                            }
                            break;

                        case XK_Left:
                            back = true;
                            break;

                        case XK_Right:
                            select = true;
                            break;

                        case XK_Return:
                            launchFromPath(userinput);
                            break;

                        case XK_Down:
                            line_cursor++;
                            break;

                        case XK_Up:
                            line_cursor--;
                            if(line_cursor < 0) line_cursor = 0;
                            break;

                        default:
                            if(userinput_cursor+len > USERINPUT_LENGTH) len = 0;
                            strncpy(userinput+userinput_cursor, buf, len);
                            userinput_cursor += len;
                            break;
                    }
                }
                //NO BREAK WE NEED TO RERENDER AFTER THIS EVENT!
                [[fallthrough]];
            case Expose:
                //handle window rendering
                drawBegin();

                drawUserinput(userinput);
                
                //print the dir list
                DirEntry* tmpEntry = handleDirList(&rootDir, &line_cursor, &select, &back);
                if(tmpEntry != NULL) currentEntry = tmpEntry;

                int pageCnt = drawDirList(&rootDir, &line_cursor, 0);

                drawBotomBar(line_cursor/LINES_IN_PAGE, pageCnt);

                drawMessage();

                XFlush(display);
                break;

            default:
                //all other events there should be none ? 
                break;
        }
    }

    XCloseDisplay(display);
    return 0;
}