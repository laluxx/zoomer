#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
    XImage* image;
} Screenshot;

Screenshot create_screenshot(Display* display, Window window);
void destroy_screenshot(Screenshot* screenshot, Display* display);
void refresh_screenshot(Screenshot* screenshot, Display* display, Window window);
