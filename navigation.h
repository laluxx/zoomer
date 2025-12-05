#pragma once

#include <stdbool.h>
#include <X11/Xlib.h>
#include "la.h"
#include "config.h"

#define VELOCITY_THRESHOLD 15.0f

typedef struct {
    Vec2f curr;
    Vec2f prev;
    bool drag;
} Mouse;

typedef struct {
    Vec2f position;
    Vec2f velocity;
    float scale;
    float delta_scale;
    Vec2f scale_pivot;
} Camera;

Vec2f world(const Camera* camera, Vec2f v);
void update_camera(Camera* camera, const Config* config, float dt,
                   const Mouse* mouse, XImage* image, Vec2f window_size);
