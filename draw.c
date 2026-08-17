#include "draw.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Display* display;
GC gc;
XFontStruct* font;
int monitor_height, monitor_width, monitor_start_x, monitor_start_y;
Window window;

unsigned long rgb_to_pixel(unsigned char r, unsigned char g, unsigned char b) {
    return (r << 16) | (g << 8) | b;
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

int drawDirList(DirEntry* dir, int* cursor, int x_pos){
    if(dir->entries <= 0) return 0;

    //save old color config
    XGCValues old_values;
    XGetGCValues(display, gc, GCForeground, &old_values);

    int totalCount = 0;     //global count of the current dir
    int displayCount = 0;   //local count. max is LINES_IN_PAGES and min 0  
    int skipp;              //this is used to skipp the first n entried in the list to enable a paged view

    int numberOfPages = 0;

    //render the cursor
    int linePos;
    
    //check if we are not the final selected entity
    if(dir->hasSelected ){
        linePos = (dir->selectedChild % LINES_IN_PAGE) + 1;
        skipp = (dir->selectedChild /LINES_IN_PAGE) * LINES_IN_PAGE;
    }else{
        //clamp cursor
        if(*cursor >= (dir->entries -1)) *cursor = (dir->entries -1);

        //calc number of pages
        numberOfPages = (dir->entries) / LINES_IN_PAGE;

        linePos = (*cursor % LINES_IN_PAGE) + 1;
        skipp = (*cursor/LINES_IN_PAGE) * LINES_IN_PAGE;
    }

    //this is used to get the correct color instantly as we 
    if(dir->hasSelected ){
        XSetForeground(display, gc, rgb_to_pixel(90, 110, 190));
    }else{
        XSetForeground(display, gc, rgb_to_pixel(70, 80, 140));
    }
    
    int cursorWidth = subEntityMaxLenghtPixel(dir, font) + PIXEL_OFFSET_LEFT * 2;
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
                numberOfPages = drawDirList(entry, cursor, x_pos+offset_x);
            }
        }
        totalCount++;
    }

    //TODO move file and fodlers into the same array for the parent and use the flag isDirectory to distinguish the two. Sorting needs to still takes place in the read of the files though....


    XSetForeground(display, gc, old_values.foreground);

    return numberOfPages;
}


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

void getMonitorSizeAndPos(int screen){
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
    monitor_width = mon.width;
    monitor_height = mon.height;
    monitor_start_x = mon.x;
    monitor_start_y = mon.y;
}

void setupWindow(){
    //Connec to the Server
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);

    //get all the needed dimensions for the window
    getMonitorSizeAndPos(screen);

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
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask | FocusChangeMask);
    
    //display Window
    XMapWindow(display, window);

    //get Keyboard focus
    XFlush(display);
    XSetInputFocus(display, window, RevertToParent, CurrentTime);

    //create context
    gc = XCreateGC(display, window, 0, NULL);

    XSetForeground(display, gc, rgb_to_pixel(255,255,255));
}

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
    int textStart_x = (monitor_width / 2) - (textWidth / 2);
    int textStart_y = (lineToPixelY(LINES_IN_PAGE+2) / 2 );
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


void drawBegin(){
    XClearWindow(display, window);
}

int cursor_pos_X = 0;

void drawUserinput(char* input){
    //draw the userinput
    XDrawString(display, window, gc, PIXEL_OFFSET_LEFT, lineToPixelY(1), input, strlen(input));
    
    //draw cursor
    cursor_pos_X = XTextWidth(font, input, strlen(input)) + PIXEL_OFFSET_LEFT;
    XDrawLine(display, window, gc, cursor_pos_X, PIXEL_LINESPACE*2, cursor_pos_X, lineToPixelY(1));
}

void drawUserinputPlaceholder(){
    //keep old colors
    XGCValues old_values;
    XGetGCValues(display, gc, GCForeground, &old_values);
    //set new colors
    XSetForeground(display, gc, rgb_to_pixel(100, 100, 100));
    //draw the string
    const char* text = "Type something!";
    XDrawString(display, window, gc, PIXEL_OFFSET_LEFT, lineToPixelY(1), text, strlen(text));
    //reset old color
    XSetForeground(display, gc, old_values.foreground);
}

void drawBotomBar(int pagePos, int pageCount){
    //Print page index
    char pageStringBuf[256];
    int pageStringLen = snprintf(pageStringBuf, sizeof(pageStringBuf), "%d / %d     Strg + N -> new dir", pagePos+1, pageCount+1);
    XDrawString(display, window, gc, PIXEL_OFFSET_LEFT, lineToPixelY(2+LINES_IN_PAGE), pageStringBuf, pageStringLen);
}

bool isWindowFocused(){
    Window focusedWindow;
    int revert_to;
    XGetInputFocus(display, &focusedWindow, &revert_to);
    return window == focusedWindow;
}

//Todo.. maybe add dynamic spacing so it always perfectly fits
//returns actually rendered names
int drawSuggestions(char** names, int count, int cursor){
    int current_X = cursor_pos_X + PIXEL_OFFSET_LEFT*5;
    //go through all filenames
    for(int i=0; i<count; i++){
        //check if the string is valid
        if(names[i] == NULL) continue;
        //get width of the string
        int string_width = XTextWidth(font, names[i], strlen(names[i]));
        //check if it fits on the window
        if(current_X+string_width >= monitor_width){
            return i+1;
        }
        //draw cursor if needed
        if(i == cursor){
            XSetForeground(display, gc, rgb_to_pixel(40, 60, 70));
            XFillRectangle(display, window, gc, current_X-PIXEL_OFFSET_LEFT, lineToPixelY(0)+PIXEL_LINESPACE, string_width+PIXEL_OFFSET_LEFT*2, lineToPixelY(1)+PIXEL_LINESPACE);
            XSetForeground(display, gc, rgb_to_pixel(255, 255, 255));
        }
        //draw the string
        XDrawString(display, window, gc, current_X , lineToPixelY(1), names[i], strlen(names[i]));
        //go to the next string
        current_X += string_width + PIXEL_OFFSET_LEFT;
    }
    return count;
}
