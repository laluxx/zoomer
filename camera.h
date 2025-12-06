#pragma once

#include <stdbool.h>
#include <X11/Xlib.h>
#include "la.h"

#define VELOCITY_THRESHOLD 15.0f

typedef struct {
    Vec2f curr;
    Vec2f prev;
    bool drag;
} Mouse;

typedef struct {
    Vec2f position;
    Vec2f target_position;  
    Vec2f velocity;
    float scale;
    float target_scale;
    float delta_scale;
    Vec2f scale_pivot;
} Camera;

Vec2f world(const Camera* camera, Vec2f v);
void update_camera(Camera *camera, float dt, const Mouse *mouse, Vec2f window_size);
