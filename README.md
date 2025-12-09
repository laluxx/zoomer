# Zoomer - Boomer C Port

A C port of [tsoding's boomer](https://github.com/tsoding/boomer) screen magnification tool.

## Dependencies

- X11
- OpenGL
- GLEW (OpenGL Extension Wrangler)
- Xrandr

On Debian/Ubuntu:
```bash
sudo apt install libx11-dev libgl1-mesa-dev libglew-dev libxrandr-dev
```

On Arch:
```bash
sudo pacman -S libx11 mesa glew libxrandr
```

## Building

```bash
make
```

## Installation

```bash
sudo make install
```

Or run from the current directory (make sure shader files are present):
```bash
./zoomer
```

## Usage

```
Usage: zoomer [OPTIONS]
  -d, --delay <seconds>     delay execution by <seconds>
  -h, --help                show this help
  -c, --config <filepath>   use config at <filepath>
  -w, --windowed            windowed mode
  --new-config [filepath]   generate default config
```

## Controls

| Control                                                                         | Description                                                   |
|---------------------------------------------------------------------------------|---------------------------------------------------------------|
| **Middle mouse click** or <kbd>0</kbd>                                          | Reset the application state (position, scale, velocity, etc). |
| <kbd>q</kbd> or <kbd>ESC</kbd>                                                  | Quit the application.                                         |
| <kbd>f</kbd>                                                                    | Toggle flashlight effect.                                     |
| **Drag** with left mouse button                                                 | Move the image around.                                        |
| **Scroll wheel** or <kbd>+</kbd>/<kbd>-</kbd>                                   | Zoom in/out.                                                  |
| <kbd>Ctrl</kbd> + **Scroll wheel** or <kbd>Ctrl</kbd>+<kbd>+</kbd>/<kbd>-</kbd> | Change the radius of the flashlight (when enabled).           |
| <kbd>h</kbd> or <kbd>←</kbd> (Left arrow)                                       | Pan camera left.                                              |
| <kbd>j</kbd> or <kbd>↓</kbd> (Down arrow)                                       | Pan camera down.                                              |
| <kbd>k</kbd> or <kbd>↑</kbd> (Up arrow)                                         | Pan camera up.                                                |
| <kbd>l</kbd> or <kbd>→</kbd> (Right arrow)                                      | Pan camera right.                                             |

## Configuration

Configuration file is located at `$HOME/.config/zoomer/config` and has roughly the following format:

```
<param-1> = <value-1>
<param-2> = <value-2>
# comment
<param-3> = <value-3>
```

You can generate a new config at `$HOME/.config/zoomer/config` with `$ zoomer --new-config`.

Supported parameters:

| Name                                 | Description                                                       |
|--------------------------------------|-------------------------------------------------------------------|
| min_scale                            | The smallest it can get when zooming out                          |
| scroll_speed                         | How quickly you can zoom in/out by scrolling                      |
| drag_friction                        | How quickly the movement slows down after dragging                |
| scale_friction                       | How quickly the zoom slows down after scrolling                   |
| scale_lerp_speed                     | Speed of zoom animation when recentering                          |
| camera_pan_amount                    | Distance to move when using arrows/hjkl keys for panning          |
| camera_position_lerp_speed           | Speed of panning animation when using keyboard navigation         |
| flashlight_lerp_speed                | Speed of flashlight radius and fade animations                    |
| flashlight_disable_radius_multiplier | Flashlight radius multiplier when disabling                       |
| lerp_camera_recenter                 | Enable/disable smooth camera recenter animation (true/false)      |
| camera_rfecenter_lerp_speed          | Speed of camera recenter animation (if lerp_camera_recenter=true) |
| blur_outside_flashlight              | Whether to blur outside the flashlight when active                |
| outside_flashlight_blur_radius       | The radius of the blur outside the flashlight                     |
| vertex_shader_path                   | Path for the vertex shader                                        |
| fragment_shader_path                 | Path for the fragment shader                                      |

## Experimental Features Compilation Flags

Experimental or unstable features can be enabled by passing the following flags to `nimble build` command:

| Flag       | Description                                                                                                                    |
|------------|--------------------------------------------------------------------------------------------------------------------------------|
| `-DLIVE`   | Live image update. See issue [#26].                                                                                            |
| `-DMITSHM` | Enables faster Live image update using MIT-SHM X11 extension. Should be used along with `-DLIVE` to have an effect             |
| `-DSELECT` | Application lets the user to click on te window to "track" and it will track that specific window instead of the whole screen. |


## TODO
Color for the background                              -- COLOR CONFIG
Blurred screenshot for the background                 -- BOOL CONFIG
Guassian blur radius bot the blurred background       -- FLOAT CONFIG
Option to not use liquid glass on the flashlight      -- BOOL CONFIG
Region color                                          -- COLOR CONFIG
morph the flashlight circle like a bubble             -- BOOL CONFIG


drag with right mouse button to select a region of pixels
it will show the width and height of the region in pixels

ALT + mouse whell could enable the flashlight if it’s off
and change both the camera scale and the radius
of the flashlight at the same time

new keybind ’s’ to take a screenshot at runtime
of the visible part of the screenshot
and flags -s --screen-shot to take a screenshot of
the entire screen copy it to te clipboard and exit

## FIXME
After disabling toggling the flashlight it should remember the size 


## License

Same as original boomer project (MIT).
