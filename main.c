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

#define MAX_SHADER_SIZE 4096

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
} Flashlight;

#define INITIAL_FL_DELTA_RADIUS 250.0f
#define FL_DELTA_RADIUS_DECELERATION 10.0f

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

static bool load_shader(Shader* shader, const char* path) {
    strncpy(shader->path, path, sizeof(shader->path) - 1);
    char* content = read_file(path);
    if (!content) return false;
    
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

static void update_flashlight(Flashlight* fl, float dt, Vec2f window_size) {
    // Handle manual radius adjustment (only when enabled and not in toggle animation)
    if (fl->is_enabled && !fl->animating && fabsf(fl->delta_radius) > 1.0f) {
        fl->target_radius = fmaxf(50.0f, fl->target_radius + fl->delta_radius * dt);
        fl->delta_radius -= fl->delta_radius * FL_DELTA_RADIUS_DECELERATION * dt;
    }
    
    // Lerp radius towards target
    fl->radius += (fl->target_radius - fl->radius) * config.flashlight_lerp_speed * dt;
    
    // Stop animating when close enough
    if (fl->animating && fabsf(fl->target_radius - fl->radius) < 1.0f) {
        fl->radius = fl->target_radius;
        fl->animating = false;
    }
    
    // Update shadow with same lerp speed as radius
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
    glUniform2f(glGetUniformLocation(shader, "cursorPos"), mouse->curr.x, mouse->curr.y);
    glUniform1f(glGetUniformLocation(shader, "flShadow"), flashlight->shadow);
    glUniform1f(glGetUniformLocation(shader, "flRadius"), flashlight->radius);
    
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
    
    // Default config path
    const char* home = getenv("HOME");
    if (home) {
        snprintf(config_file, sizeof(config_file), "%s/.config/zoomer/config", home);
    }
    
    // Parse arguments
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
    
    // Get screen refresh rate - try multiple methods
    XRRScreenConfiguration* screen_config = XRRGetScreenInfo(display, DefaultRootWindow(display));
    short rate = XRRConfigCurrentRate(screen_config);
    XRRFreeScreenConfigInfo(screen_config);
    
    // Fallback to 60Hz if rate seems wrong
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
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(glew_err));
        return 1;
    }
    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    
    // Load shaders
    Shader vertex_shader, fragment_shader;
    if (!load_shader(&vertex_shader, "vert.glsl") || 
        !load_shader(&fragment_shader, "frag.glsl")) {
        fprintf(stderr, "Failed to load shaders\n");
        return 1;
    }
    
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
    
    // Get initial window size for flashlight
    XWindowAttributes initial_wa;
    XGetWindowAttributes(display, win, &initial_wa);
    float max_screen_dimension = fmaxf(initial_wa.width, initial_wa.height);
    
    Flashlight flashlight = {
        .radius = 200.0f,
        .target_radius = 200.0f,
        .animating = false
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
                        // Coming in: start from full screen, shrink to target
                        XWindowAttributes current_wa;
                        XGetWindowAttributes(display, win, &current_wa);
                        max_screen_dimension = fmaxf(current_wa.width, current_wa.height) * 1.5f;
                        flashlight.radius = max_screen_dimension;
                        flashlight.target_radius = 200.0f;
                    } else {
                        // Going out
                        flashlight.target_radius = flashlight.target_radius * config.flashlight_disable_radius_multiplier;
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
                    // Scroll up
                    if ((event.xbutton.state & ControlMask) && flashlight.is_enabled) {
                        flashlight.delta_radius += INITIAL_FL_DELTA_RADIUS;
                        flashlight.animating = false; // Stop animation when manually adjusting
                    } else {
                        camera.delta_scale += config.scroll_speed;
                        camera.scale_pivot = mouse.curr;
                    }
                } else if (event.xbutton.button == Button5) {
                    // Scroll down
                    if ((event.xbutton.state & ControlMask) && flashlight.is_enabled) {
                        flashlight.delta_radius -= INITIAL_FL_DELTA_RADIUS;
                        flashlight.animating = false; // Stop animation when manually adjusting
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
    
        update_camera(&camera, dt, &mouse, screenshot.image,
                      (Vec2f){(float)wa.width, (float)wa.height});
        update_flashlight(&flashlight, dt, (Vec2f){(float)wa.width, (float)wa.height});
    
        draw_scene(&screenshot, &camera, shader_program, vao,
                   (Vec2f){(float)wa.width, (float)wa.height}, &mouse, &flashlight);
    
        glXSwapBuffers(display, win);
        glFinish();
    }

    destroy_screenshot(&screenshot, display);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shader_program);

    glXDestroyContext(display, glc);
    XDestroyWindow(display, win);
    XCloseDisplay(display);

    return 0;
}
