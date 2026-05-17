# gpu_app_mock_dumbfb

Phase 1 demo application for the PixelGPU project. Opens a desktop window and allows graphics to be drawn directly into a framebuffer in plain heap memory, then exits automatically after 600 frames.

## Purpose

The application demonstrates the complete Phase 1 workflow:

- Allocating a framebuffer as a plain heap buffer
- Opening a display window through the C API (`pgpu_display_create`)
- Writing pixels with raw pointer arithmetic (no drawing library)
- Synchronising to the display timer with `pgpu_wait_vsync`
- Receiving a vsync callback from the backend timer thread
- Graceful shutdown on window close, Esc key, or Ctrl-C

## Building

From the repository root, using any of the available presets:

```
cmake --workflow --preset windows-msvc-debug
```

The executable is placed in `build/<preset>/apps/gpu_app_mock_dumbfb/`.

## Running

```
./build/windows-msvc-debug/apps/gpu_app_mock_dumbfb/gpu_app_mock_dumbfb.exe
```

The application opens a 640×480 window (VGA resolution) at 60 Hz and runs a draw loop for 600 frames or until exited. No arguments are required.

## Controls

| Input | Action |
|---|---|
| Esc | Close window and exit |
| Window close button | Exit |
| Ctrl-C | Interrupt and exit |

## Console Output

On startup the app prints the resolution, framebuffer size in bytes, and which backends are registered. On exit it prints a summary.

```
PixelGPU Demo
  Resolution : 640 x 480
  Frame size : 1228800 bytes
  Backends   : sdl3(100), glfw(100)

Press Esc, close the window, or Ctrl-C to exit early.

Backend in use : sdl3
```

## Test Time

The draw loop runs for 600 frames (~10 seconds at 60 Hz). The application exits afterward.

## Framebuffer Format

All pixels are written in **XBGR8888** format: 32 bits per pixel, little-endian byte order.

```
Byte 0 = Blue
Byte 1 = Green
Byte 2 = Red
Byte 3 = X (unused, set to 0xFF)
```

The commented-out `xbgr(r, g, b)` helper function in the source packs values in this order. No drawing library is used; all patterns write directly to the `uint32_t*` framebuffer pointer returned by `pgpu_display_get_framebuffer()`.
