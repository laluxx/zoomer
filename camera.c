#include "camera.h"
#include <math.h>

Vec2f world(const Camera* camera, Vec2f v) {
    return vec2_div(v, camera->scale);
}


void update_camera(Camera *camera, float dt, const Mouse *mouse,
                   XImage* image, Vec2f window_size) {
    if (fabsf(camera->delta_scale) > 0.5f) {
        Vec2f half_window = vec2_mul(window_size, 0.5f);
        Vec2f p0 = vec2_div(vec2_sub(camera->scale_pivot, half_window), camera->scale);
        
        camera->scale = fmaxf(camera->scale + camera->delta_scale * dt, config.min_scale);
        
        Vec2f p1 = vec2_div(vec2_sub(camera->scale_pivot, half_window), camera->scale);
        camera->position = vec2_add(camera->position, vec2_sub(p0, p1));
        camera->target_position = camera->position;  // Sync target when zooming
        camera->delta_scale -= camera->delta_scale * dt * config.scale_friction;
    }
    
    if (!mouse->drag && vec2_length(camera->velocity) > VELOCITY_THRESHOLD) {
        camera->position = vec2_add(camera->position, vec2_mul(camera->velocity, dt));
        camera->target_position = camera->position;  // Sync target when coasting
        camera->velocity = vec2_sub(camera->velocity, 
                                   vec2_mul(camera->velocity, dt * config.drag_friction));
    }
    
    // Lerp position towards target (only active during keyboard navigation)
    camera->position.x += (camera->target_position.x - camera->position.x) *
                          config.camera_position_lerp_speed * dt;
    camera->position.y += (camera->target_position.y - camera->position.y) *
                          config.camera_position_lerp_speed * dt;
}
