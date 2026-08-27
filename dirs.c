#define _DEFAULT_SOURCE
#include "dirs.h"
#include "draw.h"
#include "launch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

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

int pathFilesSize = 0;
char** pathFiles = NULL;
int fileCount = 0;

void expandList(){
    //create a new list with bigger size
    pathFilesSize += 100;
    char** newList = (char**) malloc(sizeof(char*) * pathFilesSize);
    //check if there was a list before
    if(pathFiles != NULL){
        //copy old one over
        memcpy(newList, pathFiles, sizeof(char*) * (pathFilesSize-100));
        free(pathFiles);
    }
    //swap prts
    pathFiles = newList;
}

void getFilesInPath(){
    //get env var
    const char* path_var = getenv("PATH");
    if(!path_var){
        setMessage(MSG_ERROR, "could not get PATH env var");
        return;
    }
    char* path_string = strdup(path_var);

    //change the ":" to the string terminator \0
    char* c = path_string;
    int substringCount = 0;
    while(*c != '\0'){
        if(*c == ':'){
            *c = '\0';
            substringCount++;
        }
        c++;
    }

    //init the list
    expandList();

    //go through all substrings
    char* substring = path_string;
    int fileIndex = 0;
    for(int i=0; i<substringCount; i++){
        //open the dir
        DIR *dir = opendir(substring);
        //go through all the files in the dir
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            //ignore .files and .. and .
            if(entry->d_name[0] == '.') continue;
            //check if we need to resize the list
            if(fileIndex >= pathFilesSize) expandList();
            //copy the name
            char* name = strdup(entry->d_name);
            //add the name to the list
            pathFiles[fileIndex] = name;
            fileIndex++;
        }
        //go to the next substring
        substring += strlen(substring) + 1;
    }
    fileCount = fileIndex;
}

#define SCORE_MATCH_CHAR        1.0f
#define SCORE_LENGTH_PERCHAR   -0.1f

float compareString(char* filename, char* userinput){
    int flen = (int) strlen(filename);
    int ulen = (int) strlen(userinput);
    int minLen = flen < ulen ? flen : ulen; 

    float currentScore = 0.0f;
    //check for perfect matches
    for(int i = 0; i < minLen; i++){
        if(filename[i] == userinput[i]){
            currentScore += SCORE_MATCH_CHAR * (minLen - i);
        }else{
            //currentScore -= SCORE_MATCH_CHAR * (minLen - i);
        }    
    }

    //shorter results first
    currentScore += abs(flen-ulen) * SCORE_LENGTH_PERCHAR;

    return currentScore;
}

//returns a list of strings which are a match
char** getSuggestions(int best_n, char* input){
    //aloc the scoreboard
    char** list = (char**) malloc(sizeof(char*) * best_n);
    float scores[best_n];
    //init scoreboard
    for(int i=0; i<best_n; i++){
        list[i] = NULL;
        scores[i] = -100000.0f;
    }
    //go through all the files in the PATH
    for(int i=0; i<fileCount; i++){
        //eval the filename
        float score = compareString(pathFiles[i], input);

        //check if there is a high score
        for(int j=0; j<best_n; j++){
            if(scores[j] < score){
                for(int h= best_n-1; h > j; h--){
                    scores[h] = scores[h-1];
                    list[h] = list[h-1];
                }
                scores[j] = score;
                list[j] = pathFiles[i];
                break;
            }
        }
    }
    return list;
}