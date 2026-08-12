#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>

#include "dirs.h"
#include "draw.h"
#include "launch.h"

#define USERINPUT_LENGTH 128

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