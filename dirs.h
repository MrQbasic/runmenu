#include <stdbool.h>
#include <stddef.h>
#include <dirent.h>

typedef struct DirEntry{
    bool hasSelected;   //a child is also selected
    int selectedChild;
    bool isSelected;    //this one is selected
    bool isDirectoy;
    bool isLink;        //symlink in file system
    char* name;
    char* path;
    int entries;
    struct DirEntry** entryList;
    struct DirEntry* parent;
} DirEntry;

int getSubTypeCount(DIR* dir, unsigned char type);

void getDirList(DIR* dir, DirEntry* root);

DirEntry* handleDirList(DirEntry* dir, DirEntry** hoverdEntry, int* cursor, bool* select, bool* back);

void createDir(DirEntry* parent, char* dirName, int inputLength);

void createLink(DirEntry* dir, char* pathToOrigin);

void getFilesInPath();

char** getSuggestions(int best_n, char* input);

void removeFile(DirEntry* entry);

bool renameEntry(DirEntry* entry, char* newName);