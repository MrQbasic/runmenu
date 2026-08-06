#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
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

#define USERINPUT_LENGTH 128

#define PIXEL_OFFSET_LEFT 4
#define PIXEL_LINESPACE   2

#define LINES_IN_PAGE 10

#define MSG_TEXT_PADDING_X 10
#define MSG_TEXT_PADDING_Y 10


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
            printf("this should run a bash script: %s\n",path);
        }else{
            printf("launch this: %s\n", path);
        }
        execvp(path, NULL);

        //unexpected only on error
        fprintf(stderr, "EXEC failed: %s\n", strerror(errno));
        exit(127);
    }

    exit(0);
}

Display* display;
GC gc;
XFontStruct* font;
int monitor_height, monitor_width, monitor_start_x, monitor_start_y;
Window window;

XRRMonitorInfo pick_monitor_under_point(XRRMonitorInfo *monitors, int num, int x, int y) {
    for (int i = 0; i < num; i++) {
        XRRMonitorInfo m = monitors[i];
        if (x >= m.x && x < m.x + m.width &&
            y >= m.y && y < m.y + m.height) {
            return m;
        }
    }
    return monitors[0];
}

void getMonitorSizeAndPos(Display *display, int screen, int *width, int *height, int *start_x, int *start_y){
    Window root_window = RootWindow(display, screen);
    //Get position of mouse globaly
    Window ret_root, ret_child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    XQueryPointer(display, root_window, &ret_root, &ret_child, &root_x, &root_y, &win_x, &win_y, &mask);
    //Get all monitors
    int num_monitors;
    XRRMonitorInfo *monitors = XRRGetMonitors(display, root_window, True, &num_monitors);
    if (num_monitors == 0) { fprintf(stderr, "No monitors found\n"); exit(1); }
    //find which one the mouse is hovering over
    XRRMonitorInfo mon = pick_monitor_under_point(monitors, num_monitors, root_x, root_y);
    XRRFreeMonitors(monitors);
    //copy the positions
    *width = mon.width;
    *height = mon.height;
    *start_x = mon.x;
    *start_y = mon.y;
}

unsigned long rgb_to_pixel(unsigned char r, unsigned char g, unsigned char b) {
    return (r << 16) | (g << 8) | b;
}

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

int lineToPixelY(int line){
    return (font->ascent + font->descent + PIXEL_LINESPACE) * line;
}

int subEntityMaxLenghtPixel(DirEntry* root, XFontStruct* font){
    int max = 0;
    //go through dir names
    for(int i=0; i<root->entries; i++){
        DirEntry *entry = root->nextEntry[i];
        int len = strlen(entry->name);
        int pixcnt = XTextWidth(font, entry->name, len);
        if(pixcnt > max) max = pixcnt;
    }
    return max;
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

int drawDirList(Display* display, Drawable window, GC gc, XFontStruct* font, DirEntry* dir, int* cursor, int x_pos){
    if(dir->entries <= 0) return 0;

    //save old color config
    XGCValues old_values;
    XGetGCValues(display, gc, GCForeground, &old_values);

    int totalCount = 0;     //global count of the current dir
    int displayCount = 0;   //local count. max is LINES_IN_PAGES and min 0  
    int skipp;              //this is used to skipp the first n entried in the list to enable a paged view

    int numberOfPages = 0;

    //render the cursor
    int linePos, pagePos;
    
    //check if we are not the final selected entity
    if(dir->hasSelected ){
        linePos = (dir->selectedChild % LINES_IN_PAGE) + 1;
        pagePos = (dir->selectedChild / LINES_IN_PAGE);
        skipp = (dir->selectedChild /LINES_IN_PAGE) * LINES_IN_PAGE;
    }else{
        //clamp cursor
        if(*cursor >= (dir->entries -1)) *cursor = (dir->entries -1);

        //calc number of pages
        numberOfPages = (dir->entries) / LINES_IN_PAGE;

        linePos = (*cursor % LINES_IN_PAGE) + 1;
        pagePos = (*cursor / LINES_IN_PAGE);
        skipp = (*cursor/LINES_IN_PAGE) * LINES_IN_PAGE;
    }

    lastDir = dir; 

    //this is used to get the correct color instantly as we 
    if(dir->hasSelected ){
        XSetForeground(display, gc, rgb_to_pixel(90, 110, 190));
    }else{
        XSetForeground(display, gc, rgb_to_pixel(70, 80, 140));
    }
    
    int cursorWidth = subEntityMaxLenghtPixel(dir, font) + PIXEL_OFFSET_LEFT;
    XFillRectangle(display, window, gc, x_pos, lineToPixelY(linePos)+PIXEL_LINESPACE, cursorWidth, lineToPixelY(1)+PIXEL_LINESPACE);
    

    //go through all the entries 
    for(int i=0; i<dir->entries; i++){
        if(totalCount >= skipp){
            //get the entry
            if(displayCount >= LINES_IN_PAGE) break;
            DirEntry *entry = dir->nextEntry[i];

            //check the entry type
            if(entry->isDirectoy){
                XSetForeground(display, gc, rgb_to_pixel(255, 128, 64));
            }else{
                XSetForeground(display, gc, rgb_to_pixel(64, 200, 64));
            }
            
            //render the name
            int len = strlen(entry->name);
            XDrawString(display, window, gc , PIXEL_OFFSET_LEFT + x_pos, lineToPixelY(displayCount+2) , entry->name, len);
            displayCount++;
            
            //recursivly call the drawDir
            if(entry->isSelected && entry->isDirectoy){
                int offset_x = subEntityMaxLenghtPixel(dir, font) + PIXEL_OFFSET_LEFT;
                numberOfPages = drawDirList(display, window, gc, font, entry, cursor, x_pos+offset_x);
            }
        }
        totalCount++;
    }

    //TODO move file and fodlers into the same array for the parent and use the flag isDirectory to distinguish the two. Sorting needs to still takes place in the read of the files though....


    XSetForeground(display, gc, old_values.foreground);

    return numberOfPages;
}

typedef enum messageType{
    MSG_INFO,
    MSG_WARN,
    MSG_ERROR
}messageType;

messageType currentMsgType;
char* currentMsgText;
bool shouldDrawMsg = false;

void setMessage(messageType type, char* msg){
    currentMsgType = type;
    currentMsgText = msg;
    shouldDrawMsg = true;
}

void drawMessage(){
    if(!shouldDrawMsg) return;
    shouldDrawMsg = false;
    //save old color config
    XGCValues old_values;
    XGetGCValues(display, gc, GCForeground, &old_values);
    //setup type specific things
    const char* prefixString;
    unsigned long newTextColor;
    switch(currentMsgType){
        case MSG_INFO:
            newTextColor = rgb_to_pixel(255, 255, 255);
            prefixString = "INFO: ";
            break;
        case MSG_WARN:
            newTextColor = rgb_to_pixel(255, 255, 32);
            prefixString = "WARN: ";
            break;
        case MSG_ERROR:
            newTextColor = rgb_to_pixel(255, 32, 32);
            prefixString = "ERROR: ";
            break;
    }
    //append the prefix
    size_t length = strlen(currentMsgText) + 8;
    char* buf = (char*) malloc(sizeof(char) * length);
    int buf_len = snprintf(buf, length, "%s%s", prefixString, currentMsgText); 
    //calc some dimensions
    int textWidth = XTextWidth(font, buf, buf_len);
    int textStart_x = monitor_start_x + (monitor_width / 2) - (textWidth / 2);
    int textStart_y = monitor_start_y + (lineToPixelY(LINES_IN_PAGE+2) / 2 );
    //clear the area under the bar
    XSetForeground(display, gc, rgb_to_pixel(0, 0, 0));
    XFillRectangle(display, window, gc, textStart_x - MSG_TEXT_PADDING_X, textStart_y - MSG_TEXT_PADDING_Y - lineToPixelY(1) + PIXEL_LINESPACE, textWidth + MSG_TEXT_PADDING_X*2, lineToPixelY(1) + MSG_TEXT_PADDING_Y*2);
    //draw the background box
    XSetForeground(display, gc, newTextColor);
    XDrawRectangle(display, window, gc, textStart_x - MSG_TEXT_PADDING_X, textStart_y - MSG_TEXT_PADDING_Y - lineToPixelY(1) + PIXEL_LINESPACE, textWidth + MSG_TEXT_PADDING_X*2, lineToPixelY(1) + MSG_TEXT_PADDING_Y*2);
    //draw the text inside
    XDrawString(display, window, gc, textStart_x, textStart_y, buf, buf_len);
    //reset colors and buffer
    free(buf);
    XSetForeground(display, gc, old_values.foreground);
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


int main(void) {
    //Connec to the Server
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);

    //get all the needed dimensions for the window
    getMonitorSizeAndPos(display, screen, &monitor_width, &monitor_height, &monitor_start_x, &monitor_start_y);

    //set the window Color
    XSetWindowAttributes attrs = {0};
    attrs.override_redirect = True;
    attrs.background_pixel = rgb_to_pixel(16, 16, 16);

    //get font
    font = XLoadQueryFont(display, "fixed");
    
    //calc window dimensions
    int window_height = lineToPixelY(LINES_IN_PAGE+2) + PIXEL_LINESPACE;

    //Create the window    
    window = XCreateWindow(
        display, RootWindow(display, screen),
        monitor_start_x, monitor_start_y,
        monitor_width, window_height,
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWOverrideRedirect | CWBackPixel,
        &attrs
    );

    //enable events
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    
    //display Window
    XMapWindow(display, window);

    //get Keyboard focus
    XFlush(display);
    XSetInputFocus(display, window, RevertToParent, CurrentTime);

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
    gc = XCreateGC(display, window, 0, NULL);
    XEvent event;

    XSetForeground(display, gc, rgb_to_pixel(255,255,255));

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
                            if(userinput_cursor > 0) userinput_cursor--;
                            break;

                        case XK_Left:
                            back = true;
                            break;

                        case XK_Right:
                        case XK_Return:
                            select = true;
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
            case Expose:
                //handle window rendering
                XClearWindow(display, window);

                //draw the userinput
                XDrawString(display, window, gc, PIXEL_OFFSET_LEFT, lineToPixelY(1), userinput, userinput_cursor);
                
                //draw cursor
                int width = XTextWidth(font, buf, userinput_cursor);
                XDrawLine(display, window, gc, PIXEL_OFFSET_LEFT + width, PIXEL_LINESPACE*2, PIXEL_OFFSET_LEFT + width, lineToPixelY(1));

                //print the dir list
                DirEntry* tmpEntry = handleDirList(&rootDir, &line_cursor, &select, &back);
                if(tmpEntry != NULL){
                    currentEntry = tmpEntry;
                    printf("Current dir: %s\n", currentEntry->path);
                };

                int pageCnt = drawDirList(display, window, gc, font, &rootDir, &line_cursor, 0);
                
                //Print page index
                int pagePos = (line_cursor / LINES_IN_PAGE);
                char pageStringBuf[256];
                int pageStringLen = snprintf(pageStringBuf, sizeof(pageStringBuf), "%d / %d     Strg + N -> new dir", pagePos+1, pageCnt+1);

                XDrawString(display, window, gc, PIXEL_OFFSET_LEFT, lineToPixelY(2+LINES_IN_PAGE), pageStringBuf, pageStringLen);
                
                drawMessage();

                break;

            default:
                //all other events there should be none ? 
                break;
        }
    }

    XCloseDisplay(display);
    return 0;
}