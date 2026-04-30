# Lesson 1: Your First (dumb) Framebuffer

## What You Will Build

By the end of this lesson you will have a working desktop window that displays whatever you write into a block of memory. The library opens the window and handles timing for you; your job is to write code that puts coloured pixels into that memory.

The application starts as a black window. Each challenge below asks you to make something visible; a single pixel, a gradient, a bouncing rectangle; by computing colours and writing them into the framebuffer one pixel at a time. No drawing library. No GPU. Just you, a pointer, and arithmetic.


## Prerequisites

Before starting, read the material in `PREREQUISITES.md` at the root of the repository. In particular, make sure you can explain (to yourself at least):

- What a framebuffer is and why it is just a block of memory
- What a pixel format is and how bytes map to red, green, and blue channels
- How to compute the memory address of a pixel from its x and y coordinates

You will use all three concepts immediately.


## Tooling You Need

Regardless of platform, you need the following installed before the build will work.

| Tool | Minimum version | Notes |
|---|---|---|
| C++ compiler | GCC 12, Clang 16, or MSVC 19.38 (VS 2022 17.8) | C++20 required |
| CMake | 3.25 | The preset system requires this version |
| Ninja | any recent | Used by all presets as the build driver |
| Git | any recent | vcpkg downloads itself via git |

The build system uses **vcpkg in manifest mode**. This means all C++ dependencies (SDL3, GLFW, Catch2, fmt) are fetched and compiled automatically during the first configure step. You do not need to install them separately. The first build will take several minutes while vcpkg compiles the dependencies; subsequent builds are fast.


## Platform Setup

### Linux

**Install compiler, build tools, and X11 development headers.**

On Ubuntu 22.04 or later:

```bash
sudo apt update
sudo apt install build-essential git cmake ninja-build \
    libx11-dev libxrandr-dev libxi-dev libxinerama-dev \
    libxcursor-dev libgl-dev libgles-dev libwayland-dev \
    libxkbcommon-dev
```

On Fedora / RHEL derivatives:

```bash
sudo dnf install gcc-c++ git cmake ninja-build \
    libX11-devel libXrandr-devel libXi-devel \
    libXinerama-devel libXcursor-devel mesa-libGL-devel
```

> The X11 and GL headers are needed even though SDL3 and GLFW are built from source by vcpkg, because those libraries link against the system X11 and OpenGL at runtime.

**Clone and build:**

```bash
git clone https://github.com/AlphaPixel/gpu-driver-learning.git
cd gpu-driver-learning
cmake --workflow --preset gcc-debug
```

The first run downloads and compiles vcpkg dependencies. When it finishes, run the demo:

```bash
./build/gcc-debug/apps/gpu_app_mock_dumbfb/gpu_app_mock_dumbfb
```

A 640×480 black window should appear. Close it with Esc or the window close button.

**Run tests only:**

```bash
ctest --preset gcc-debug
```

**VS Code on Linux:**

Install the [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) (includes CMake Tools). Open the repository root as a folder. CMake Tools will detect `CMakePresets.json` and prompt you to select a configure preset — choose `gcc Debug`. Use the status bar buttons to configure, build, and launch.


### macOS

**Install Xcode Command Line Tools:**

```bash
xcode-select --install
```

**Install CMake and Ninja via Homebrew:**

```bash
brew install cmake ninja
```

> Homebrew is available at [brew.sh](https://brew.sh) if you do not have it.

**Clone and build:**

```bash
git clone https://github.com/AlphaPixel/gpu-driver-learning.git
cd gpu-driver-learning
cmake --workflow --preset clang-debug
```

Run the demo:

```bash
./build/clang-debug/apps/gpu_app_mock_dumbfb/gpu_app_mock_dumbfb
```

> **macOS window note:** On macOS, the display window must be created from the main thread. The demo already handles this correctly. If you see a spinning beachball or an unresponsive window, check that you have not moved the `pgpu_display_create` call off the main thread.

**CLion on macOS:**

1. Open CLion and choose **Open** → select the repository root.
2. CLion detects `CMakePresets.json` automatically. When prompted, select the `clang Debug` profile.
3. CMake runs configure in the background; watch the **CMake** tool window for vcpkg output during the first configure.
4. Once the build finishes, select `gpu_app_mock_dumbfb` from the run configuration dropdown in the toolbar and click **Run**.

To run tests: open the **Run** menu → **Run 'All Tests'**, or right-click the `test` target in the CMake tool window.

**VS Code on macOS:**

Install the [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack). Open the folder, select the `clang Debug` configure preset when prompted, then build and run from the CMake Tools sidebar.


### Windows

#### Option A — Visual Studio 2022 (recommended)

**Install Visual Studio 2022 Community** from [visualstudio.microsoft.com](https://visualstudio.microsoft.com/). During installation, select the **Desktop development with C++** workload. This includes MSVC, CMake, and Ninja.

1. Open Visual Studio.
2. Choose **Open a local folder** and select the repository root.
3. Visual Studio detects `CMakePresets.json`. In the toolbar configuration dropdown, select **msvc Debug**.
4. Visual Studio runs CMake configure automatically. The first configure downloads and builds vcpkg dependencies — watch the **Output** window for progress.
5. Once configure completes, go to **Build → Build All** (or press `Ctrl+Shift+B`).
6. To run: **Debug → Start Without Debugging** (`Ctrl+F5`). Select `gpu_app_mock_dumbfb.exe` as the startup item if asked.

To run tests: **Test → Run All Tests**.

#### Option B — VS Code with MSVC

1. Install Visual Studio 2022 (or the standalone **Build Tools for Visual Studio 2022**) for the compiler. The **Desktop development with C++** workload is required.
2. Install **VS Code** with the [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack).
3. Open the repository root in VS Code.
4. When CMake Tools prompts for a kit, select the **Visual Studio Community 2022 Release - amd64** kit (or equivalent).
5. When prompted for a configure preset, select **msvc Debug**.
6. Build with `Ctrl+Shift+P` → **CMake: Build**.
7. Run with `Ctrl+Shift+P` → **CMake: Run Without Debugging** → select `gpu_app_mock_dumbfb`.

> **Important:** On Windows, always build from a Developer Command Prompt or from VS Code/Visual Studio, which sets up the MSVC compiler environment automatically. Running `cmake` from a plain Command Prompt or PowerShell without the MSVC environment will fail with errors about missing headers.

**Running tests on Windows:**

```
ctest --preset windows-msvc-debug
```

Or use Test Explorer in either IDE.


## Preset Reference

| Preset name | Platform | Compiler | Type |
|---|---|---|---|
| `gcc-debug` | Linux / macOS | GCC | Debug + tests |
| `gcc-release` | Linux / macOS | GCC | Release |
| `clang-debug` | Linux / macOS | Clang | Debug + tests |
| `clang-release` | Linux / macOS | Clang | Release |
| `windows-msvc-debug` | Windows | MSVC | Debug + tests |
| `windows-msvc-release` | Windows | MSVC | Release |
| `windows-clang-debug` | Windows | Clang-cl | Debug + tests |
| `windows-clang-release` | Windows | Clang-cl | Release |

The `--workflow` flag runs configure, build, and test in one step. You can also run each phase separately:

```bash
cmake --preset gcc-debug          # configure only
cmake --build --preset gcc-debug  # build only
ctest --preset gcc-debug          # test only
```


## Understanding the Starting Code

Open `apps/gpu_app_mock_dumbfb/src/gpu_app_mock_dumbfb.cpp`. The skeleton version contains:

1. A call to `pgpu_display_create()` that opens a 640×480 window at 60 Hz and returns a handle.
2. A call to `pgpu_display_get_framebuffer()` that returns a `void*` to the pixel memory.
3. A render loop that calls `pgpu_wait_vsync()` then `pgpu_display_present_now()` once per frame.
4. A shutdown path that calls `pgpu_display_destroy()` when the window is closed.

The result is a window that shows whatever is in the framebuffer. Because the framebuffer starts zeroed, the window is black.

Note: The code will auto-select the "backend" implementation it uses to create a window, display your framebuffer data like a monitor, and monitor input.
Two back-ends are currently provided for portability: SDL3 and GLFW. Either one is adequate for all demos, and they are provided simply for variety and
portability. More backends with more advanced features may be implemented in the future. You can request a specific back-end by its name, using the fifth
argument to `pgpu_display_create`, named `backend_priority`. Passing 'glfw' or 'sdl3' will demand that backend by name, and fail if it is not available.

### The pixel format

Every pixel is 4 bytes in **XBGR8888** format. The reason for the extra X byte is to make every pixel be a 32-bit long word,
aligned with a 32-bit address. Frequently the extra byte is used for things like an Alpha (A) channel, but our dumb-framebuffer
backend doesn't do anything at all with it right now. The byte packing order is:

```
Byte 0 = Blue   (0 = no blue,  255 = full blue)
Byte 1 = Green  (0 = no green, 255 = full green)
Byte 2 = Red    (0 = no red,   255 = full red)
Byte 3 = X      (unused — the library sets this to 0xFF)
```

When you write pixels as `uint32_t` values on a little-endian system, blue occupies the lowest byte (so, kinda BGRX order). The easiest way to pack a pixel is:

```c
uint32_t pixel = ((uint32_t)0xFF << 24)   // X byte
               | ((uint32_t)r    << 16)   // Red
               | ((uint32_t)g    <<  8)   // Green
               | ((uint32_t)b);           // Blue
```

Cast the framebuffer pointer to `uint32_t*` to write one pixel per array element.

### Computing a pixel address

The framebuffer is a flat array laid out row by row, left to right, top to bottom:

```c
uint32_t* fb = (uint32_t*)pgpu_display_get_framebuffer(display);
uint32_t  w  = res.width;   // 640

// Write pixel at (x, y):
fb[y * w + x] = pixel;
```

`y * w` steps to the start of row `y`. Adding `x` steps to the column.

In this simple framebuffer emulator, there are no unusual per-row stride adjustments, each row begins on the long word immediately
following the end of the row before. Some graphics hardware allows you to have more columns in memory than can actually be displayed,
and shift the screen-display rectangle around with row start modifiers, allowing for efficient display pan/scrolling without memory
copying for every pixel scrolled.

### The render loop

```c
while (pgpu_display_is_alive(display)) {
    // --- draw into fb here 
    pgpu_wait_vsync(display, 100);     // wait for the next 60 Hz tick
    pgpu_display_present_now(display); // copy fb to the window
}
```

Everything you draw happens between the top of the loop and `pgpu_wait_vsync`. The present call uploads the framebuffer to the screen once per frame.


## Drawing Challenges

Work through these in order. Each one builds on the skills from the previous. The framebuffer is 640×480 pixels unless you change the resolution preset.

For all challenges, add your drawing code inside the render loop, between the opening brace and the `pgpu_wait_vsync` call.


### Challenge 1 — Single pixel in the centre

Write a single white pixel at position (320, 240) — the exact centre of the 640×480 framebuffer.

**What you should see:** A single white dot on a black background. It will be hard to spot at first; look carefully at the centre of the window.

**Hint:** White is R=255, G=255, B=255. Compute the pixel's index as `y * width + x`. You only need one array assignment.


### Challenge 2 — Flood fill

Fill every pixel in the framebuffer with the same solid colour. Try pure green (R=0, G=255, B=0) first, then experiment with other colours.

**What you should see:** The entire window switches from black to the colour you chose.

**Hint:** Loop over all `width * height` pixels and assign the same value to each. You can use a single `for` loop from index `0` to `width * height - 1`, or two nested loops over `y` and `x` — both work.


### Challenge 3 — Single-axis gradient

Make red increase from 0 at the left edge to 255 at the right edge. Green and blue stay at 0. The result is a gradient from black on the left to pure red on the right.

**What you should see:** A smooth horizontal sweep from black at the left to bright red at the right.

**Hint:** For each pixel at column `x`, the red value is `(x * 255) / (width - 1)`. Use integer arithmetic — no floating point needed. Cast carefully to avoid overflow: compute `x * 255u` as unsigned before dividing.


### Challenge 4 — Two-axis gradient

Extend the previous challenge: red increases left to right as before, and blue increases top to bottom. Green stays at 0.

**What you should see:**
- Top-left corner: black (R=0, G=0, B=0)
- Top-right corner: red (R=255, G=0, B=0)
- Bottom-left corner: blue (R=0, G=0, B=255)
- Bottom-right corner: magenta (R=255, G=0, B=255)

The interior blends smoothly between all four corners.

**Hint:** Each pixel at `(x, y)` gets red from `x` and blue from `y`, using the same formula as Challenge 3 applied separately to each axis.


### Challenge 5 — Checkerboard

Divide the framebuffer into an 8×8 grid of tiles, each 80×60 pixels. Colour each tile either white or black based on whether `(tile_col + tile_row)` is even or odd.

**What you should see:** A classic black-and-white checkerboard filling the whole window.

**Hint:** For a pixel at `(x, y)`, the tile column is `x / 80` and the tile row is `y / 60`. If their sum is even, write white; if odd, write black. The `%` operator extracts the parity: `(col + row) % 2`.


### Challenge 6 — Horizontal colour stripes

Divide the framebuffer into 8 equal horizontal bands. Colour each band a different solid colour. Choose your own palette, or use: red, orange, yellow, green, cyan, blue, violet, white.

**What you should see:** Eight horizontal stripes stacked from top to bottom, each a different colour.

**Hint:** The band index for a pixel at row `y` is `y / (height / 8)`. Index into an array of 8 pre-computed pixel values. Watch the last band: if `height` is not divisible by 8, use `height` as the upper bound for the last stripe rather than `(band + 1) * band_height`.


### Challenge 7 — Random scatter

Each frame, plot 10,000 pixels at random positions with random colours. Do not clear the screen first — let the dots accumulate.

**What you should see:** A growing field of random-coloured dots that eventually covers most of the black background. The window will look like colour static.

**Hint:** Use `rand()` from `<cstdlib>` (or `<stdlib.h>` in C). Constrain x with `rand() % width` and y with `rand() % height`. Generate R, G, and B independently with `rand() % 256`. The accumulation effect is free — just do not clear the framebuffer at the top of the loop.


### Challenge 8 — Solid filled rectangle

Draw a solid coloured rectangle at a fixed position. Pick a position yourself — for example, a 200×100 blue rectangle with its top-left corner at (220, 190), roughly centred.

**What you should see:** A blue rectangle on the black background, clearly visible with sharp edges.

**Hint:** Two nested loops: the outer loop runs from `y = rect_top` to `y < rect_top + rect_height`, the inner from `x = rect_left` to `x < rect_left + rect_width`. Plot one pixel per iteration.


### Challenge 9 — Random rectangles

Each frame, draw 50 rectangles at random positions with random colours and random dimensions. Clear the framebuffer to black at the start of each frame before drawing.

**What you should see:** A flickering storm of coloured rectangles, different every frame.

**Hint:** Put a `fill()` call at the top of your loop to clear to black, then loop 50 times and draw each rectangle with random values. Clamp rectangle bounds to the framebuffer edges to avoid writing out of bounds: `x1 = min(x0 + w, width - 1)`.


### Challenge 10 — Animated gradient scroll

Draw a horizontal red gradient as in Challenge 3, but shift the gradient left by one pixel each frame. When it scrolls fully off the left edge it wraps around and reappears on the right.

**What you should see:** A smooth red gradient that appears to continuously scroll leftward across the window.

**Hint:** Add a `frame` counter that increments each loop iteration. For pixel at column `x`, compute the red value from `(x + frame) % width` instead of `x` alone. No framebuffer clear is needed — the gradient overwrites every pixel each frame.


### Challenge 11 — Bouncing rectangle

Draw a white 60×40 rectangle on a black background. Give it a velocity: start it moving at 1 pixel per frame horizontally and 1 pixel per frame vertically. When any edge of the rectangle reaches the edge of the framebuffer, reverse that component of velocity.

**What you should see:** A white rectangle gliding smoothly around the window, bouncing off all four walls without any tearing at its edges.

**Hint:** Store four variables outside the loop: `rx`, `ry` (position of the top-left corner) and `vx`, `vy` (velocity). Each frame: update `rx += vx` and `ry += vy`. If `rx + rect_width >= screen_width`, set `vx = -vx` and clamp `rx`; same for the vertical axis. Clear to black first, then draw the rectangle.


### Challenge 12 — Line between two points

Draw a straight line between two arbitrary points using integer arithmetic only. Choose two points far apart and in different corners.

**What you should see:** A clean diagonal line connecting the two points, made of individual pixels with no gaps.

**Hint:** Look up **Bresenham's line algorithm**. It uses only integer addition and computes which pixels to light along the line without any floating-point math. The key variable is the error accumulator that decides when to step in the minor axis.


### Challenge 13 — Filled circle

Draw a solid coloured filled circle centred in the window. Try a radius of 100 pixels.

**What you should see:** A smooth circular disc, solid-coloured, centred in the window.

**Hint:** Loop over every pixel in the bounding box of the circle. For each pixel at `(x, y)`, compute `dx = x - cx` and `dy = y - cy`. If `dx*dx + dy*dy <= radius*radius`, write the pixel colour. This uses the equation of a circle and involves only integer multiplication and comparison — no `sqrt` needed.


### Challenge 14 — Change Resolution

Try adjusting the display you create to use PGPU_RES_SXGA instead of PGPU_RES_VGA. Or use PGPU_RES_GAMEBOY_ADVANCE.

**What you should see:**
- SXGA: 1280x1024 window
- GAMEBOY_ADVANCE: 240x160 window (sorry, no auto-upscaling, it'll be tiny on modern displays)

**Hint:** Make sure you're using the returned width and height values dynamically so your pixel plotting and filling don't overrun the framebuffer memory (crash!). Also, you might consider pre-scaling dimensions of stripes and such as a factor of the screen dimensions.


## Checking Your Work

After each challenge, ask yourself:

- **Are the colours right?** Compare to the expected description. If red looks blue or blue looks red, your byte packing may have the R and B channels swapped — double-check the XBGR layout.
- **Are the edges sharp?** Blurry edges on rectangles usually mean an off-by-one in a loop bound.
- **Is the motion smooth?** Jerky animation on the bouncing rectangle usually means your vsync call is not being reached each frame, or your velocity arithmetic has an integer truncation error.
- **Is there any flicker?** If you see the background flash before the foreground is drawn, move your clear step to the very beginning of the loop, before any drawing.


