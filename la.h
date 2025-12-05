#pragma once

#include <math.h>

typedef struct {
    float x, y;
} Vec2f;

static inline Vec2f vec2_mul(Vec2f a, float s) {
    return (Vec2f){a.x * s, a.y * s};
}

static inline Vec2f vec2_div(Vec2f a, float s) {
    return (Vec2f){a.x / s, a.y / s};
}

static inline Vec2f vec2_mul_v(Vec2f a, Vec2f b) {
    return (Vec2f){a.x * b.x, a.y * b.y};
}

static inline Vec2f vec2_div_v(Vec2f a, Vec2f b) {
    return (Vec2f){a.x / b.x, a.y / b.y};
}

static inline Vec2f vec2_sub(Vec2f a, Vec2f b) {
    return (Vec2f){a.x - b.x, a.y - b.y};
}

static inline Vec2f vec2_add(Vec2f a, Vec2f b) {
    return (Vec2f){a.x + b.x, a.y + b.y};
}

static inline float vec2_length(Vec2f a) {
    return sqrtf(a.x * a.x + a.y * a.y);
}

static inline Vec2f vec2_normalize(Vec2f a) {
    float len = vec2_length(a);
    if (len == 0.0f) {
        return (Vec2f){0.0f, 0.0f};
    }
    return vec2_div(a, len);
}

