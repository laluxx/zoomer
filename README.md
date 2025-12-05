# Zoomer - Boomer C Port

A C port of [tsoding's zoomer](https://github.com/tsoding/boomer) screen magnification tool.

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

| Control                                   | Description                                                   |
|-------------------------------------------|---------------------------------------------------------------|
| Middle mouse click or <kbd>0</kbd>        | Reset the application state (position, scale, velocity, etc). |
| <kbd>q</kbd> or <kbd>ESC</kbd>            | Quit the application.                                         |
| <kbd>r</kbd>                              | Reload configuration.                                         |
| <kbd>Ctrl</kbd> + <kbd>r</kbd>            | Reload the shaders (only for Developer mode)                  |
| <kbd>f</kbd>                              | Toggle flashlight effect.                                     |
| Drag with left mouse button               | Move the image around.                                        |
| Scroll wheel or <kbd>=</kbd>/<kbd>-</kbd> | Zoom in/out.                                                  |
| <kbd>Ctrl</kbd> + Scroll wheel            | Change the radious of the flaslight.                          |

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

| Name           | Description                                        |
|----------------|----------------------------------------------------|
| min_scale      | The smallest it can get when zooming out           |
| scroll_speed   | How quickly you can zoom in/out by scrolling       |
| drag_friction  | How quickly the movement slows down after dragging |
| scale_friction | How quickly the zoom slows down after scrolling    |

## Experimental Features Compilation Flags

Experimental or unstable features can be enabled by passing the following flags to `nimble build` command:

| Flag       | Description                                                                                                                    |
|------------|--------------------------------------------------------------------------------------------------------------------------------|
| `-DLIVE`   | Live image update. See issue [#26].                                                                                            |
| `-DMITSHM` | Enables faster Live image update using MIT-SHM X11 extension. Should be used along with `-DLIVE` to have an effect             |
| `-DSELECT` | Application lets the user to click on te window to "track" and it will track that specific window instead of the whole screen. |

## License

Same as original boomer project (MIT).
