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