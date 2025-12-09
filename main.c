#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>

#include "config.h"
#include "screenshot.h"
#include "camera.h"
#include "la.h"

#define MAX_SHADER_SIZE 8192

typedef struct {
    char path[256];
    char content[MAX_SHADER_SIZE];
} Shader;

typedef struct {
    bool is_enabled;
    float shadow;
    float radius;
    float delta_radius;
    float target_radius;
    bool animating;
    
    // Physics properties for bubble
    Vec2f position;      // Current position
    Vec2f velocity;      // Current velocity
    Vec2f target_pos;    // Target position (cursor)
    Vec2f acceleration;  // Current acceleration (for deformation)
    float mass;          // Mass of the bubble
    float spring_k;      // Spring constant
    float damping;       // Damping coefficient
    
    // Deformation properties
    Vec2f stretch;       // Stretch amount in x,y directions
    float squeeze;       // Perpendicular squeeze factor
} Flashlight;

#define INITIAL_FL_DELTA_RADIUS 250.0f
#define FL_DELTA_RADIUS_DECELERATION 10.0f

// Physics constants for bubble movement
#define BUBBLE_MASS 1.0f
#define BUBBLE_SPRING_K 80.0f
#define BUBBLE_DAMPING 8.0f
#define BUBBLE_STRETCH_FACTOR 0.0001f  // How much velocity causes stretch
#define BUBBLE_SQUEEZE_FACTOR 0.5f     // How much perpendicular squeeze
#define BUBBLE_DEFORM_SMOOTHING 8.0f   // Smoothing for deformation recovery

static char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    return content;
}

static bool load_shader(Shader* shader, const char* config_path, const char* fallback_path) {
    if (config_path && config_path[0] != '\0') {
        char* content = read_file(config_path);
        if (content) {
            strncpy(shader->path, config_path, sizeof(shader->path) - 1);
            strncpy(shader->content, content, sizeof(shader->content) - 1);
            free(content);
            return true;
        } else {
            fprintf(stderr, "Warning: Could not load shader from config path: %s\n", config_path);
        }
    }
    
    char* content = read_file(fallback_path);
    if (!content) {
        fprintf(stderr, "Error: Could not load shader from fallback path: %s\n", fallback_path);
        return false;
    }
    
    strncpy(shader->path, fallback_path, sizeof(shader->path) - 1);
    strncpy(shader->content, content, sizeof(shader->content) - 1);
    free(content);
    return true;
}

static GLuint compile_shader(const Shader* shader, GLenum type) {
    GLuint id = glCreateShader(type);
    const char* src = shader->content;
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);
    
    GLint success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(id, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compilation error (%s):\n%s\n", shader->path, log);
    }
    
    return id;
}

static GLuint create_shader_program(const Shader* vertex, const Shader* fragment) {
    GLuint vs = compile_shader(vertex, GL_VERTEX_SHADER);
    GLuint fs = compile_shader(fragment, GL_FRAGMENT_SHADER);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "Shader linking error:\n%s\n", log);
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(program);
    
    return program;
}

static void update_flashlight(Flashlight* fl, float dt, Vec2f cursor_pos) {
    fl->target_pos = cursor_pos;
    
    // Handle manual radius adjustment
    if (fl->is_enabled && !fl->animating && fabsf(fl->delta_radius) > 1.0f) {
        fl->target_radius = fmaxf(50.0f, fl->target_radius + fl->delta_radius * dt);
        fl->delta_radius -= fl->delta_radius * FL_DELTA_RADIUS_DECELERATION * dt;
    }
    
    // Physics-based position update
    if (fl->is_enabled) {
        // Calculate spring force: F = -k * (x - target)
        Vec2f displacement = vec2_sub(fl->position, fl->target_pos);
        Vec2f spring_force = vec2_mul(displacement, -fl->spring_k);
        
        // Calculate damping force: F = -c * v
        Vec2f damping_force = vec2_mul(fl->velocity, -fl->damping);
        
        // Total force
        Vec2f total_force = vec2_add(spring_force, damping_force);
        
        // Acceleration: a = F / m
        fl->acceleration = vec2_mul(total_force, 1.0f / fl->mass);
        
        // Update velocity: v = v + a * dt
        fl->velocity = vec2_add(fl->velocity, vec2_mul(fl->acceleration, dt));
        
        // Update position: p = p + v * dt
        fl->position = vec2_add(fl->position, vec2_mul(fl->velocity, dt));
        
        // Calculate deformation based on velocity and acceleration
        float vel_mag = vec2_length(fl->velocity);
        
        if (vel_mag > 0.1f) {
            // Normalize velocity to get direction
            Vec2f vel_norm = vec2_mul(fl->velocity, 1.0f / vel_mag);
            
            // Stretch in direction of movement
            float stretch_amount = vel_mag * BUBBLE_STRETCH_FACTOR;
            Vec2f target_stretch = vec2_mul(vel_norm, stretch_amount);
            
            // Smooth interpolation towards target stretch
            fl->stretch.x += (target_stretch.x - fl->stretch.x) * BUBBLE_DEFORM_SMOOTHING * dt;
            fl->stretch.y += (target_stretch.y - fl->stretch.y) * BUBBLE_DEFORM_SMOOTHING * dt;
            
            // Squeeze perpendicular to movement (volume conservation)
            float target_squeeze = stretch_amount * BUBBLE_SQUEEZE_FACTOR;
            fl->squeeze += (target_squeeze - fl->squeeze) * BUBBLE_DEFORM_SMOOTHING * dt;
        } else {
            // Recover to circular shape when not moving
            fl->stretch.x += (0.0f - fl->stretch.x) * BUBBLE_DEFORM_SMOOTHING * dt;
            fl->stretch.y += (0.0f - fl->stretch.y) * BUBBLE_DEFORM_SMOOTHING * dt;
            fl->squeeze += (0.0f - fl->squeeze) * BUBBLE_DEFORM_SMOOTHING * dt;
        }
    } else {
        // When disabled, snap to cursor position
        fl->position = cursor_pos;
        fl->velocity = (Vec2f){0, 0};
        fl->acceleration = (Vec2f){0, 0};
        fl->stretch = (Vec2f){0, 0};
        fl->squeeze = 0.0f;
    }
    
    // Lerp radius towards target
    fl->radius += (fl->target_radius - fl->radius) * config.flashlight_lerp_speed * dt;
    
    // Stop animating when close enough
    if (fl->animating && fabsf(fl->target_radius - fl->radius) < 1.0f) {
        fl->radius = fl->target_radius;
        fl->animating = false;
    }
    
    // Update shadow
    float target_shadow = fl->is_enabled ? 0.8f : 0.0f;
    fl->shadow += (target_shadow - fl->shadow) * config.flashlight_lerp_speed * dt;
}

static void draw_scene(Screenshot* screenshot, Camera* camera, GLuint shader, GLuint vao,
                      Vec2f window_size, Mouse* mouse, Flashlight* flashlight) {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(shader);

    glUniform2f(glGetUniformLocation(shader, "cameraPos"), camera->position.x, camera->position.y);
    glUniform1f(glGetUniformLocation(shader, "cameraScale"), camera->scale);
    glUniform2f(glGetUniformLocation(shader, "screenshotSize"),
                (float)screenshot->image->width, (float)screenshot->image->height);
    glUniform2f(glGetUniformLocation(shader, "windowSize"), window_size.x, window_size.y);
    
    // Pass flashlight bubble properties
    glUniform2f(glGetUniformLocation(shader, "cursorPos"), flashlight->position.x, flashlight->position.y);
    glUniform2f(glGetUniformLocation(shader, "bubbleStretch"), flashlight->stretch.x, flashlight->stretch.y);
    glUniform1f(glGetUniformLocation(shader, "bubbleSqueeze"), flashlight->squeeze);
    
    glUniform1f(glGetUniformLocation(shader, "flShadow"), flashlight->shadow);
    glUniform1f(glGetUniformLocation(shader, "flRadius"), flashlight->radius);
    glUniform1f(glGetUniformLocation(shader, "flEnabled"), flashlight->is_enabled ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(shader, "blur_background"), config.blur_background ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(shader, "background_blur_radius"), config.background_blur_radius);
    glUniform1f(glGetUniformLocation(shader, "blur_outside_flashlight"), config.blur_outside_flashlight ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(shader, "outside_flashlight_blur_radius"), config.outside_flashlight_blur_radius);
    
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
}

static Vec2f get_cursor_position(Display* display) {
    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    
    XQueryPointer(display, DefaultRootWindow(display),
                  &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
    
    return (Vec2f){(float)root_x, (float)root_y};
}

static void print_usage(void) {
    printf("Usage: zoomer [OPTIONS]\n");
    printf("  -d, --delay <seconds>     delay execution by <seconds>\n");
    printf("  -h, --help                show this help\n");
    printf("  -c, --config <filepath>   use config at <filepath>\n");
    printf("  -w, --windowed            windowed mode\n");
    printf("  --new-config [filepath]   generate default config\n");
}

int main(int argc, char** argv) {
    bool windowed = false;
    float delay_sec = 0.0f;
    char config_file[512] = {0};
    
    const char* home = getenv("HOME");
    if (home) {
        snprintf(config_file, sizeof(config_file), "%s/.config/zoomer/config", home);
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--delay") == 0) {
            if (i + 1 < argc) {
                delay_sec = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--windowed") == 0) {
            windowed = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                strncpy(config_file, argv[++i], sizeof(config_file) - 1);
            }
        } else if (strcmp(argv[i], "--new-config") == 0) {
            const char* path = (i + 1 < argc) ? argv[i + 1] : config_file;
            generate_default_config(path);
            printf("Generated config at %s\n", path);
            return 0;
        }
    }
    
    if (delay_sec > 0.0f) {
        struct timespec ts;
        ts.tv_sec = (time_t)delay_sec;
        ts.tv_nsec = (long)((delay_sec - ts.tv_sec) * 1000000000);
        nanosleep(&ts, NULL);
    }
    
    config = load_config(config_file);
    
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open display\n");
        return 1;
    }
    
    Window tracking_window = DefaultRootWindow(display);
    
    XRRScreenConfiguration* screen_config = XRRGetScreenInfo(display, DefaultRootWindow(display));
    short rate = XRRConfigCurrentRate(screen_config);
    XRRFreeScreenConfigInfo(screen_config);
    
    if (rate < 30 || rate > 500) {
        rate = 60;
        printf("Screen rate detection failed, using default: 60 Hz\n");
    } else {
        printf("Screen rate: %d Hz\n", rate);
    }
    
    int screen = DefaultScreen(display);
    
    GLint glx_attrs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(display, screen, glx_attrs);
    if (!vi) {
        fprintf(stderr, "No appropriate visual found\n");
        return 1;
    }
    
    XSetWindowAttributes swa = {0};
    swa.colormap = XCreateColormap(display, DefaultRootWindow(display), vi->visual, AllocNone);
    swa.event_mask = ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
        PointerMotionMask | ExposureMask | StructureNotifyMask;
    if (!windowed) {
        swa.override_redirect = True;
        swa.save_under = True;
    }
    
    XWindowAttributes root_attrs;
    XGetWindowAttributes(display, DefaultRootWindow(display), &root_attrs);
    
    Window win = XCreateWindow(display, DefaultRootWindow(display),
                               0, 0, root_attrs.width, root_attrs.height, 0,
                               vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask | CWOverrideRedirect | CWSaveUnder,
                               &swa);
    
    XStoreName(display, win, "zoomer");
    XMapWindow(display, win);
    
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, win, &wm_delete, 1);
    
    GLXContext glc = glXCreateContext(display, vi, NULL, GL_TRUE);
    glXMakeCurrent(display, win, glc);
    
    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(glew_err));
        return 1;
    }
    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    
    Shader vertex_shader, fragment_shader;
    if (!load_shader(&vertex_shader, config.vertex_shader_path, "/etc/zoomer/vert.glsl") || 
        !load_shader(&fragment_shader, config.fragment_shader_path, "/etc/zoomer/frag.glsl")) {
        fprintf(stderr, "Failed to load shaders\n");
        return 1;
    }
    
    printf("Loaded vertex shader:   %s\n", vertex_shader.path);
    printf("Loaded fragment shader: %s\n", fragment_shader.path);
    
    GLuint shader_program = create_shader_program(&vertex_shader, &fragment_shader);
    
    Screenshot screenshot = create_screenshot(display, tracking_window);
    
    float w = (float)screenshot.image->width;
    float h = (float)screenshot.image->height;
    
    GLfloat vertices[] = {
        w, 0, 0.0f, 1.0f, 1.0f,
        w, h, 0.0f, 1.0f, 0.0f,
        0, h, 0.0f, 0.0f, 0.0f,
        0, 0, 0.0f, 0.0f, 1.0f
    };
    
    GLuint indices[] = {0, 1, 3, 1, 2, 3};
    
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenshot.image->width, screenshot.image->height,
                 0, GL_BGRA, GL_UNSIGNED_BYTE, screenshot.image->data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glUniform1i(glGetUniformLocation(shader_program, "tex"), 0);
    glEnable(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    Camera camera = {.scale = 1.0f, .target_scale = 1.0f,};
    Vec2f cursor_pos = get_cursor_position(display);
    Mouse mouse = {.curr = cursor_pos, .prev = cursor_pos};
    
    XWindowAttributes initial_wa;
    XGetWindowAttributes(display, win, &initial_wa);
    float max_screen_dimension = fmaxf(initial_wa.width, initial_wa.height);
    
    // Initialize flashlight with physics and deformation properties
    Flashlight flashlight = {
        .radius = 200.0f,
        .target_radius = 200.0f,
        .animating = false,
        .position = cursor_pos,
        .velocity = {0, 0},
        .acceleration = {0, 0},
        .target_pos = cursor_pos,
        .mass = BUBBLE_MASS,
        .spring_k = BUBBLE_SPRING_K,
        .damping = BUBBLE_DAMPING,
        .stretch = {0, 0},
        .squeeze = 0.0f
    };
    
    float dt = 1.0f / (float)rate;
    bool running = true;
    
    while (running) {
        if (!windowed) {
            XSetInputFocus(display, win, RevertToParent, CurrentTime);
        }
        
        XWindowAttributes wa;
        XGetWindowAttributes(display, win, &wa);
        glViewport(0, 0, wa.width, wa.height);
        
        XEvent event;
        while (XPending(display)) {
            XNextEvent(display, &event);
            
            switch (event.type) {
            case MotionNotify:
                mouse.curr = (Vec2f){(float)event.xmotion.x, (float)event.xmotion.y};
                if (mouse.drag) {
                    Vec2f delta = vec2_sub(world(&camera, mouse.prev), world(&camera, mouse.curr));
                    camera.position = vec2_add(camera.position, delta);
                    camera.target_position = camera.position;                    
                    camera.velocity = vec2_mul(delta, (float)rate);
                }
                mouse.prev = mouse.curr;
                break;
                    
            case KeyPress: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_q || key == XK_Escape) {
                    running = false;
                } else if (key == XK_0) {
                    if (config.lerp_camera_recenter) {
                        camera.target_position = (Vec2f){0, 0};
                        camera.target_scale = 1.0f;
                        camera.velocity = (Vec2f){0, 0};
                        camera.delta_scale = 0.0f;
                    } else {
                        camera.scale = 1.0f;
                        camera.target_scale = 1.0f;
                        camera.delta_scale = 0.0f;
                        camera.position = (Vec2f){0, 0};
                        camera.target_position = (Vec2f){0, 0};
                        camera.velocity = (Vec2f){0, 0};
                    }
                } else if (key == XK_f) {
                    flashlight.is_enabled = !flashlight.is_enabled;
                    flashlight.animating = true;
        
                    if (flashlight.is_enabled) {
                        XWindowAttributes current_wa;
                        XGetWindowAttributes(display, win, &current_wa);
                        max_screen_dimension = fmaxf(current_wa.width, current_wa.height) * 1.5f;
                        flashlight.radius = max_screen_dimension;
                        flashlight.target_radius = 200.0f;
        
                        if (config.hide_cursor_on_flashlight) {
                            char data[] = {0};
                            Pixmap blank = XCreateBitmapFromData(display, win, data, 1, 1);
                            XColor dummy;
                            Cursor cursor = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
                            XDefineCursor(display, win, cursor);
                            XFreePixmap(display, blank);
                        }
                    } else {
                        flashlight.target_radius = flashlight.target_radius * config.flashlight_disable_radius_multiplier;
        
                        if (config.hide_cursor_on_flashlight) {
                            XUndefineCursor(display, win);
                        }
                    }
                } else if (key == XK_equal) {
                    if ((event.xkey.state & ControlMask) && flashlight.is_enabled) {
                        flashlight.delta_radius += INITIAL_FL_DELTA_RADIUS;
                    } else {
                        camera.delta_scale += config.scroll_speed;
                        camera.scale_pivot = mouse.curr;
                    }
                } else if (key == XK_minus) {
                    if ((event.xkey.state & ControlMask) && flashlight.is_enabled) {
                        flashlight.delta_radius -= INITIAL_FL_DELTA_RADIUS;
                    } else {
                        camera.delta_scale -= config.scroll_speed;
                        camera.scale_pivot = mouse.curr;
                    }
                } else if (key == XK_h || key == XK_Left) {
                    camera.target_position.x -= config.camera_pan_amount;
                } else if (key == XK_j || key == XK_Down) {
                    camera.target_position.y += config.camera_pan_amount;
                } else if (key == XK_k || key == XK_Up) {
                    camera.target_position.y -= config.camera_pan_amount;
                } else if (key == XK_l || key == XK_Right) {
                    camera.target_position.x += config.camera_pan_amount;
                }
                break;
            }
            
            case ButtonPress:
                if (event.xbutton.button == Button1) {
                    mouse.prev = mouse.curr;
                    mouse.drag = true;
                    camera.velocity = (Vec2f){0, 0};
                } else if (event.xbutton.button == Button2) {
                    if (config.lerp_camera_recenter) {
                        camera.target_position = (Vec2f){0, 0};
                        camera.target_scale = 1.0f;
                        camera.velocity = (Vec2f){0, 0};
                        camera.delta_scale = 0.0f;
                    } else {
                        camera.scale = 1.0f;
                        camera.target_scale = 1.0f;
                        camera.delta_scale = 0.0f;
                        camera.position = (Vec2f){0, 0};
                        camera.target_position = (Vec2f){0, 0};
                        camera.velocity = (Vec2f){0, 0};
                    }
                } else if (event.xbutton.button == Button4) {
                    if ((event.xbutton.state & ControlMask) && flashlight.is_enabled) {
                        flashlight.delta_radius += INITIAL_FL_DELTA_RADIUS;
                        flashlight.animating = false;
                    } else {
                        camera.delta_scale += config.scroll_speed;
                        camera.scale_pivot = mouse.curr;
                    }
                } else if (event.xbutton.button == Button5) {
                    if ((event.xbutton.state & ControlMask) && flashlight.is_enabled) {
                        flashlight.delta_radius -= INITIAL_FL_DELTA_RADIUS;
                        flashlight.animating = false;
                    } else {
                        camera.delta_scale -= config.scroll_speed;
                        camera.scale_pivot = mouse.curr;
                    }
                }
                break;
            
            case ButtonRelease:
                if (event.xbutton.button == Button1) {
                    mouse.drag = false;
                }
                break;
            
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == wm_delete) {
                    running = false;
                }
                break;
            }
        }
    
        update_camera(&camera, dt, &mouse, (Vec2f){(float)wa.width, (float)wa.height});
        update_flashlight(&flashlight, dt, mouse.curr);
    
        draw_scene(&screenshot, &camera, shader_program, vao,
                   (Vec2f){(float)wa.width, (float)wa.height}, &mouse, &flashlight);
    
        glXSwapBuffers(display, win);
        glFinish();
    }

    destroy_screenshot(&screenshot);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shader_program);

    glXDestroyContext(display, glc);
    XDestroyWindow(display, win);
    XCloseDisplay(display);

    return 0;
}    
