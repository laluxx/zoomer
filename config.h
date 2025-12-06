#pragma once

#include <stdbool.h>

typedef struct {
    float min_scale;
    float scroll_speed;
    float drag_friction;
    float scale_friction;
    float scale_lerp_speed;
    float camera_pan_amount;
    float camera_position_lerp_speed;
    float flashlight_lerp_speed;
    float flashlight_disable_radius_multiplier;
    bool  lerp_camera_recenter;
    float camera_recenter_lerp_speed;
    bool  blur_background;
    float background_blur_radius;
    bool  blur_outside_flashlight;
    float outside_flashlight_blur_radius;
    bool  hide_cursor_on_flashlight;
    // Shader paths
    char vertex_shader_path[512];
    char fragment_shader_path[512];
} Config;

extern Config config;

Config load_config(const char* filepath);
void generate_default_config(const char* filepath);

