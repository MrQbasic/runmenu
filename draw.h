#pragma once
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <stdbool.h>

#include "dirs.h"

#define PIXEL_OFFSET_LEFT 4
#define PIXEL_LINESPACE   2

#define LINES_IN_PAGE 10

#define MSG_TEXT_PADDING_X 10
#define MSG_TEXT_PADDING_Y 10

extern Display* display;
extern GC gc;
extern XFontStruct* font;
extern Window window;

typedef enum messageType{
    MSG_INFO,
    MSG_WARN,
    MSG_ERROR
}messageType;

void setMessage(messageType type, char* msg);
void drawMessage();

void setupWindow();

void drawBegin();

void drawBotomBar(int pagePos, int pageCount);

void drawUserinput(char* input);

int drawDirList(DirEntry* dir, int* cursor, int x_pos);

XRRMonitorInfo pick_monitor_under_point(XRRMonitorInfo *monitors, int num, int x, int y);