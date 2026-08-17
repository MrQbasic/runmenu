#include <stdbool.h>
#include <stddef.h>
#include <dirent.h>

typedef struct DirEntry{
    bool hasSelected;   //a child is also selected
    int selectedChild;
    bool isSelected;    //this one is selected
    bool isDirectoy;
    char* name;
    char* path;
    int entries;
    struct DirEntry** nextEntry;
    struct DirEntry* parent;
} DirEntry;

int getSubTypeCount(DIR* dir, unsigned char type);

void getDirList(DIR* dir, DirEntry* root);

DirEntry* handleDirList(DirEntry* dir, int* cursor, bool* select, bool* back);

void createDir(DirEntry* parent, char* dirName, int inputLength);

void getFilesInPath();

char** getSuggestions(int best_n, char* input);