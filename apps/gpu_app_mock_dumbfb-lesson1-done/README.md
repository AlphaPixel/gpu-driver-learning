# gpu_app_mock_dumbfb-lesson1-done

Lesson 1 reference implementation for the PixelGPU project. Demonstrates thirteen (skipping Challenge 14) drawing challenges from `lessons/lesson-1/lesson-1.md` as selectable, runnable demos. Each challenge is invoked by passing its number as a command-line argument.

## Purpose

This application is the "done" companion to the student skeleton (`gpu_app_mock_dumbfb`). Where the skeleton contains only the window setup and an empty render loop, this version contains a complete, working implementation of Lesson 1 challenges. Students can run any challenge to see the expected output before or after attempting it themselves.

All drawing uses raw pointer arithmetic into the framebuffer — no drawing library. This is intentional.

## Building

From the repository root, using any of the available presets:

```
cmake --workflow --preset windows-msvc-debug
```

The executable is placed in `build/<preset>/apps/gpu_app_mock_dumbfb-lesson1-done/`.

## Running

```
gpu_app_mock_dumbfb-lesson1-done.exe <challenge-number>
```

The challenge number must be an integer from 1 to 13. The application opens a 640×480 window (VGA resolution) at 60 Hz and runs until the window is closed, Esc is pressed, or Ctrl-C is sent.

### Examples

```
gpu_app_mock_dumbfb-lesson1-done.exe 4     # two-axis gradient
gpu_app_mock_dumbfb-lesson1-done.exe 11    # bouncing rectangle
```

## Challenges

| # | Name | Animated |
|---|---|---|
| 1 | Single pixel in the centre | No |
| 2 | Flood fill | No |
| 3 | Single-axis gradient | No |
| 4 | Two-axis gradient | No |
| 5 | Checkerboard | No |
| 6 | Horizontal colour stripes | No |
| 7 | Random scatter | Yes — dots accumulate each frame |
| 8 | Solid filled rectangle | No |
| 9 | Random rectangles | Yes — new layout every frame |
| 10 | Animated gradient scroll | Yes — scrolls one pixel/frame |
| 11 | Bouncing rectangle | Yes — moves 1 px/frame, bounces off walls |
| 12 | Line between two points | No |
| 13 | Filled circle | No |

Challenge 14 (change resolution) is not included here because it requires changes to the window-creation arguments, which are outside the per-challenge draw functions.

## Controls

| Input | Action |
|---|---|
| Esc | Close window and exit |
| Window close button | Exit |
| Ctrl-C | Interrupt and exit |

## Console Output

On startup the app prints the selected challenge name, the resolution, and the backend in use. On exit it prints total frames rendered and vsync callbacks received.

```
PixelGPU — Lesson 1 Reference
  Challenge 4  — Two-axis gradient
  Resolution : 640 x 480
  Backends   : sdl3(100), glfw(100)

Press Esc, close the window, or Ctrl-C to exit.

Backend in use : sdl3
```

If no argument or an out-of-range argument is given, a usage message listing all challenges is printed and the application exits without opening a window.

## Differences from gpu_app_mock_dumbfb

| | gpu_app_mock_dumbfb | gpu_app_mock_dumbfb-lesson1-done |
|---|---|---|
| Purpose | Student starting skeleton | Instructor reference / answer key |
| Render content | Empty (black window) | One of 13 lesson challenges |
| Challenge selection | N/A | Command-line argument 1–13 |
| Run duration | 600 frames then exits | Runs until closed |
| Drawing code | Absent (student fills in) | Complete implementations |

## Framebuffer Format

All pixels are written in **XBGR8888** format: 32 bits per pixel, little-endian byte order.

```
Byte 0 = Blue
Byte 1 = Green
Byte 2 = Red
Byte 3 = X (unused, set to 0xFF)
```

The `xbgr(r, g, b)` helper in the source packs values in this order. All challenge functions write directly to the `uint32_t*` framebuffer pointer returned by `pgpu_display_get_framebuffer()`.
