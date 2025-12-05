#pragma once

typedef struct {
    float min_scale;
    float scroll_speed;
    float drag_friction;
    float scale_friction;
} Config;

Config load_config(const char* filepath);
void generate_default_config(const char* filepath);

