#include "screenshot.h"
#include <stdlib.h>

Screenshot create_screenshot(Display* display, Window window) {
    Screenshot screenshot;
    
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);
    
    screenshot.image = XGetImage(
        display, window,
        0, 0,
        attributes.width,
        attributes.height,
        AllPlanes,
        ZPixmap
    );
    
    return screenshot;
}

void destroy_screenshot(Screenshot* screenshot) {
    if (screenshot->image) {
        XDestroyImage(screenshot->image);
        screenshot->image = NULL;
    }
}

void refresh_screenshot(Screenshot* screenshot, Display* display, Window window) {
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);
    
    XImage* refreshed = XGetSubImage(
        display, window,
        0, 0,
        screenshot->image->width,
        screenshot->image->height,
        AllPlanes,
        ZPixmap,
        screenshot->image,
        0, 0
    );
    
    if (!refreshed || 
        refreshed->width != attributes.width || 
        refreshed->height != attributes.height) {
        
        XImage* new_image = XGetImage(
            display, window,
            0, 0,
            attributes.width,
            attributes.height,
            AllPlanes,
            ZPixmap
        );
        
        if (new_image) {
            XDestroyImage(screenshot->image);
            screenshot->image = new_image;
        }
    } else {
        screenshot->image = refreshed;
    }
}
