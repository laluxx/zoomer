#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <strings.h>

Config config;

static Config default_config(void) {
    return (Config){
        .min_scale = 0.01f,
        .scroll_speed = 1.5f,
        .drag_friction = 6.0f,
        .scale_friction = 4.0f,
        .camera_pan_amount = 200.0f,
        .camera_position_lerp_speed = 3.0f,
        .flashlight_lerp_speed = 8.0f,
        .flashlight_disable_radius_multiplier = 2.0f,
        .lerp_camera_recenter = true,
        .camera_recenter_lerp_speed = 3.0f,
        .scale_lerp_speed = 3.0f,
        .blur_background = true,
        .background_blur_radius = 10.0f,
        .blur_outside_flashlight = true,
        .outside_flashlight_blur_radius = 10.0f,
        .hide_cursor_on_flashlight = true,
    };
}

static bool parse_bool(const char* value) {
    return (strcasecmp(value, "true") == 0 ||
            strcasecmp(value, "yes") == 0 ||
            strcasecmp(value, "on") == 0 ||
            strcmp(value, "1") == 0);
}

Config load_config(const char* filepath) {
    Config config = default_config();
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Config file not found: %s. Using defaults.\n", filepath);
        return config;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Strip whitespace and comments
        char* hash = strchr(line, '#');
        if (hash) *hash = '\0';
        
        char key[64], value[64];
        if (sscanf(line, " %63[^=] = %63s", key, value) == 2) {
            // Trim whitespace
            char* k = key;
            while (*k == ' ' || *k == '\t') k++;
            char* k_end = k + strlen(k) - 1;
            while (k_end > k && (*k_end == ' ' || *k_end == '\t' || *k_end == '\n')) k_end--;
            *(k_end + 1) = '\0';
            
            if (strcmp(k, "min_scale") == 0) {
                config.min_scale = atof(value);
            } else if (strcmp(k, "scroll_speed") == 0) {
                config.scroll_speed = atof(value);
            } else if (strcmp(k, "drag_friction") == 0) {
                config.drag_friction = atof(value);
            } else if (strcmp(k, "scale_friction") == 0) {
                config.scale_friction = atof(value);
            } else if (strcmp(k, "scale_lerp_speed") == 0) {
                config.scale_lerp_speed = atof(value);
            } else if (strcmp(k, "camera_pan_amount") == 0) {
                config.camera_pan_amount = atof(value);
            } else if (strcmp(k, "camera_position_lerp_speed") == 0) {
                config.camera_position_lerp_speed = atof(value);
            } else if (strcmp(k, "flashlight_lerp_speed") == 0) {
                config.flashlight_lerp_speed = atof(value);
            } else if (strcmp(k, "flashlight_disable_radius_multiplier") == 0) {
                config.flashlight_disable_radius_multiplier = atof(value);
            } else if (strcmp(k, "lerp_camera_recenter") == 0) {
                config.lerp_camera_recenter = parse_bool(value);
            } else if (strcmp(k, "camera_recenter_lerp_speed") == 0) {
                config.camera_recenter_lerp_speed = atof(value);
            } else if (strcmp(k, "blur_background") == 0) {
                config.blur_background = parse_bool(value);
            } else if (strcmp(k, "background_blur_radius") == 0) {
                config.background_blur_radius = atof(value);
            } else if (strcmp(k, "blur_outside_flashlight") == 0) {
                config.blur_outside_flashlight = parse_bool(value);
            } else if (strcmp(k, "outside_flashlight_blur_radius") == 0) {
                config.outside_flashlight_blur_radius = atof(value);
            } else if (strcmp(k, "hide_cursor_on_flashlight") == 0) {
                config.hide_cursor_on_flashlight = parse_bool(value);
            }
        }
    }
    
    fclose(f);
    return config;
}

void generate_default_config(const char* filepath) {
    // Create directory if needed
    char dir[512];
    strncpy(dir, filepath, sizeof(dir) - 1);
    char* last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);
    }
    
    FILE* f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "Failed to create config file: %s\n", filepath);
        return;
    }
    
    Config config = default_config();
    fprintf(f, "# Camera settings\n");
    fprintf(f, "min_scale = %f\n",                                 config.min_scale);
    fprintf(f, "scroll_speed = %f\n",                              config.scroll_speed);
    fprintf(f, "drag_friction = %f\n",                             config.drag_friction);
    fprintf(f, "scale_friction = %f\n",                            config.scale_friction);
    fprintf(f, "scale_lerp_speed = %f\n",                          config.scale_lerp_speed);
    fprintf(f, "camera_pan_amount = %f\n",                         config.camera_pan_amount);
    fprintf(f, "camera_position_lerp_speed = %f\n",                config.camera_position_lerp_speed);
    fprintf(f, "\n# Recenter settings (0 or middle mouse button)\n");
    fprintf(f, "lerp_camera_recenter = %s  # true/false, yes/no, on/off, 1/0\n", 
            config.lerp_camera_recenter ? "true" : "false");
    fprintf(f, "camera_recenter_lerp_speed = %f\n",                config.camera_recenter_lerp_speed);
    fprintf(f, "\n# Flashlight settings\n");
    fprintf(f, "flashlight_lerp_speed = %f\n",                     config.flashlight_lerp_speed);
    fprintf(f, "flashlight_disable_radius_multiplier = %f\n",      config.flashlight_disable_radius_multiplier);
    fprintf(f, "blur_background = %s  # true/false, yes/no, on/off, 1/0\n", 
            config.blur_background ? "true" : "false");
    fprintf(f, "background_blur_radius = %f\n",      config.background_blur_radius);
    fprintf(f, "blur_outside_flashlight = %s  # true/false, yes/no, on/off, 1/0\n", 
            config.blur_outside_flashlight ? "true" : "false");
    fprintf(f, "outside_flashlight_blur_radius = %f\n",      config.outside_flashlight_blur_radius);
    fprintf(f, "hide_cursor_on_flashlight = %s  # true/false, yes/no, on/off, 1/0\n", 
            config.hide_cursor_on_flashlight ? "true" : "false");

    
    fclose(f);
}
