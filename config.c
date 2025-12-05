#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static Config default_config(void) {
    return (Config){
        .min_scale = 0.01f,
        .scroll_speed = 1.5f,
        .drag_friction = 6.0f,
        .scale_friction = 4.0f
    };
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
    fprintf(f, "min_scale = %f\n", config.min_scale);
    fprintf(f, "scroll_speed = %f\n", config.scroll_speed);
    fprintf(f, "drag_friction = %f\n", config.drag_friction);
    fprintf(f, "scale_friction = %f\n", config.scale_friction);
    
    fclose(f);
}
