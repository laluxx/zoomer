#pragma once

typedef struct {
    float min_scale;
    float scroll_speed;
    float drag_friction;
    float scale_friction;
    float camera_pan_amount;
    float camera_position_lerp_speed;
    float flashlight_lerp_speed;
    float flashlight_disable_radius_multiplier;
} Config;

extern Config config;

Config load_config(const char* filepath);
void generate_default_config(const char* filepath);

