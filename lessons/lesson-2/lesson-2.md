# Lesson 2: Your First 2D Graphics Library

## What You Will Build

By the end of this lesson you will have a working 2D graphics library, capable of some typical operations like rectangular pixel manipulation (fill and copy) and line drawing (Bresenham). These capabilities roughly mimic the hardware assisted abilities of an 80s-vintage hardware graphics processing unit like the Amiga's Agnus chip, which could be commanded by the CPU (via register writes) to perform unattended pixel-memory operations and trigger notification (via interrupts) when completed. This graphics co-processor ("Copper" then, but now known as a GPU) used first-tier memory access (DMA) to read and write primary system RAM that was shared with the CPU, in a split-bandwidth configuration. Other system RAM was inaccessible to the GPU, and the CPU had preferential full-bandwidth access to it. This is a very simple analog of modern GPUs and displays, where memory ownership, capabilities and IO can be restrictive.

The application starts once again as a black window. Each challenge below asks you to add a new ability in software, mimicking how the co-processor/GPU implements graphics tasks. Initially, the challenge will have you write these as normal user-space drawing functions, just like lesson 1's demos. We will utilize some pre-provided pixel color calculation and other helper functions that are an outgrowth of lesson 1. When you have implemented all the capabilities, we will then wrap them up in an opaque library with an API that hides whether they are implemented by CPU-software or a theoretical GPU hardware behind the API.

## Prerequisites

Before starting, make sure you complete at least some of the challenges of Lesson 1. Especially you should understand:

- How to draw a pixel (and make sure it's safely within the screen bounds).
- How to draw rectangles (ditto).
- How to compute the memory address of a pixel from its x and y coordinates.
- How to compute a 32-bit pixel-memory XBGR data value from input R, G, and B components.

You will use all these prior concepts immediately.


## Tooling You Need

Platform / Tooling has not changed since lesson 1.

## Understanding the Starting Code

The starting code is exactly the same as lesson 1.

Open `apps/gpu_app_mock_dumbfb/src/gpu_app_mock_dumbfb.cpp`. The skeleton version contains:

1. A call to `pgpu_display_create()` that opens a 640×480 window at 60 Hz and returns a handle.
2. A call to `pgpu_display_get_framebuffer()` that returns a `void*` to the pixel memory.
3. A render loop that calls `pgpu_wait_vsync()` then `pgpu_display_present_now()` once per frame.
4. A shutdown path that calls `pgpu_display_destroy()` when the window is closed.

The result is a window that shows whatever is in the framebuffer. Because the framebuffer starts zeroed, the window is black.

### The Pixel XBGR Format Helper Function

The function

```c
static inline uint32_t xbgr(uint8_t r, uint8_t g, uint8_t b) noexcept
```

is pre-provided as a helper for encoding R, G, B triplet components into the 32-bit XBGR encoding.


### Pixel Address Write Helper Function

The function

```c
static inline void plot(uint32_t* fb, uint32_t stride_px,
                        uint32_t x, uint32_t y, uint32_t color) noexcept
```

is provided to make it easy to write a single pixel of a computed/encoded RGB value into a particular X/Y pixel coordinate.

No clipping or other memory safety is provided. The code therein may be useful to copy and implement in other functions you write, possibly with your own enhancements. You may also wish to decompose this into an address helper function, and create a corresponding pixel-read function using the address helper.

### Implementing drawing library functions

It is recommended that you write utility graphics functions imitating the template shown for plot(), wherein you pass fb and stride as the first arguments, always. Following that will normally be coordinate data (usually in pairs, in X, Y order) and then non-coordinate data like colors, etc. Colors should be in the form of uint32_t color. Copying and extending the plot() function is probably a good strategy for any new drawing-library function.

### Ascending and Descending copies

A typical problem in onscreen blitting (**BL**ock**T**ransfer = BLT = BLiT) is overlapping source and destination rectangles. Generally, overlap can always be resolved (in the absence of scaling) by iterating the correct direction in memory, either ascending or descending. Initially you may want to implement a block-copy algorithm that only handles non-overlapping copies, and then adapt it to have either incrementing or decrementing iteration, and an initialization stage that determines which is suited for each invocation. A no-error-checking variant that bypasses bounds checking and overlap planning could be called from a safer error-preventing wrapper. Performance-intensive code that is known to not trigger potential error conditions could directly call the no-guardrails variant.

### Bresenham's Line Drawing Algorithm

Wikipedia provides an excellent summary of the Bresenham Line Drawing algorithm simplified for integer coordinates
https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm#Algorithm_for_integer_arithmetic

This is the algorithm most used in 80s-era hardware line drawing (eg Amiga Agnus), especially where limited color palettes prevented antialiasing and made fractional/sub-pixel coordinates unhelpful.

Wikipedia provides excellent pseudocode that you should be able to implement.

## Graphics Library Challenges

Work through these in order. Each one roughly builds on the skills from the previous. The framebuffer is 640×480 pixels (unless you change the resolution preset).

For all challenges, implement your drawing primitives (rectangle, line, copy) as a library function. Then add your drawing testing code inside the render loop, between the opening brace and the `pgpu_wait_vsync` call.

### Challenge 1 — Filled Rectangle

You basically did this already as challenge 8 of lesson 1. Now you simply refactor this into a concise and opaque library call so you can utilize it many times. Inputs should include XA,YA and XB, YB corner coordinates (consider whether you care if one is above/below the other in memory or not) and a color parameter.

**What you should see:** A colored rectangle on the black background, clearly visible with sharp edges.

**Hint:** Two nested loops: the outer loop runs from `y = rect_top` to `y < rect_top + rect_height`, the inner from `x = rect_left` to `x < rect_left + rect_width`. Plot one pixel per iteration.

**Optimization consideration:** Because you are writing 32-bit quantities, there probably aren't many opportunities to execute a larger-memory-sized subset (eg, 64-bit writes of all the pixel pairs that are on a 64-bit-aligned memory address) and then using 32-bit writes to finish the left and right edges that might be unaligned. In past architectures, memory alignment and pixel alignment sometimes didn't match well, so exploiting big writes to do most of the work and small writes to finish the unaligned edges sometimes was a big gain.

**Safety consideration:** What about clipping the edges to the known display edges, to prevent accidental corruption from memory over/underwrites?

### Challenge 2 — Unfilled (stroked) Rectangle

This is a degenerate case of the above. We don't want the interior filled, just the edges drawn.

**What you should see:** A colored rectangle outlined on the black background, clearly visible with sharp edges and black (unfilled) interior.

**Hint:** There are four edges. You can probably implement this as two loops, one drawing both the top and bottom edges (same loop X, different Y) and left and right edges (same loop Y, different X). You can even plan to undershoot by a pixel on each end of one of the axes, so you don't needlessly draw the corner pixels multiple times.

**Safety consideration:** Unlike clipping filled rectangles, where you just limit the coordinates on clipped edges, you now have to plan to actually omit drawing of edges that are beyond the display boundary. Otherwise the clipped edge will become unintentionally visible, just smashed against the edge of the display. You may want a case for all-edges-visible-unclipped, and fall back to simply line-drawing each of the four edges otherwise.

### Challenge 3 — Block Copy

This is an extended case of Lesson 2 Challenge 1. Instead of having a *fixed* source color written to every pixel, you need to have a source read location that tracks XY relative to the destination write location. Read each pixel from the source, then write it to the destination.

For purposes of testing, you might want to pre-fill the screen with a gradient of some sort (see Lesson 1, Challenge 4, "Two-axis gradient" for a good one). This will make it clearly visually apparent when you have or have not successfully copied the source to the destination.

**What you should see:** A rectangular region should be replicated into the destination from the source area.

**Hint:** See "Ascending and Descending copies" above for some notes on overlapping read/write regions.

**Safety consideration:** Here, you need to decide what exactly happens if the caller tries to perform an out of bounds read or write. An illegal read can probably be satisfied with a black pixel being pseudo-read. An illegal write should be safely rejected.

**Stretch Goal:** Define a "transparent color" which will be ignored when found in the source region, causing writes taken from that transparent area to be skipped.


### Challenge 4 — Scaling Block Copy

Extend the previous challenge to permit a larger or smaller (in either axis, and not necessarily uniform or matched in direction!) source area than the destination. Generally scaling blitters of the era (see https://en.wikipedia.org/wiki/Assault_(1988_video_game) ) did nearest-neighbor scaling without any antialiasing, so you only need one read per pixel written, but you will need non-integer increments of the source X and Y.

**What you should see:** A scaled/distorted copy of the source region.

**Stretch Goal:** Implement a bilinear or other smooth scaling sampling method. This reads 1-4 source samples. Round the floating-point source XY coordinates down and up, sample all contacted pixels, and blend according to the fractional proportion on each axis. Learn about Barycentric sampling for added education.


### Challenge 5 — Line Drawing

Implement the Bresenham's line drawing algorithm as cited above, for drawing a solid-colored single-pixel wide line.

**What you should see:** A single pixel line from start to finish. At various slopes, it may appear jagged and stair-stepped.

**Safety consideration:** It's very easy to draw a line outside the display memory buffer, and moderately challenging to efficiently clip the line geometrically (see Cohen-Sutherland https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm ). To start, simply check each pixel written and reject those outside the display bounds.

**Stretch Goal:** Implement Cohen-Sutherland geometric clipping.

**Stretch Goal:** Implement multi-pixel wide line drawing by offsetting the source and dest XY pairs perpendicular to the line slope. The slope of the perpendicular line is the inverse rise/run of the original line. Drawing even line widths is challenging without antialiasing, so focus just on widths of 1, 3, 5 pixels, etc.

**Stretch Goal:** Implement bitmap-patterned dotted line patterning. Take an 8, 16, 32 or even 64-bit unsigned quantity as an argument, and interpret it as a bitfield. For each pixel drawn, compare the corresponding bit in the bitfield, and inhibit pixel writes when the bitfield has a 0. This allows for complex dotted line patterns that repeat on the bitfield width. Use a bit shift with a bitwise AND ("&" not "&&") test to move through the bitfield, resetting when the number of pixels drawn hits the number of bits wide your bitfield is.



### Challenge 6 — Anti-Aliased Line Drawing

Use the Xiaolin Wu line drawing algorithm ( https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm ) to draw an anti-aliased line.

**What you should see:** Smooth-edged lines. All angles should look much smoother than Bresenham.

**Hint:** Because we don't have real alpha channel support, as you calculate the contribution of each line pixel, you will need to read the existing destination pixel, blend the new pixel contribution into it, and rewrite it.

**Safety consideration:** You can use Cohen-Sutherland to clip the end pixels, just be aware that Wu has a few special cases to watch.



### Challenge 7 — Circle Drawing

Use the Bresenham-midpoint ( https://en.wikipedia.org/wiki/Midpoint_circle_algorithm ) and/or Wu ( https://cgg.mff.cuni.cz/~pepca/ref/WU.pdf ) circle drawing algorithms to draw single pixel or multi-pixel wide circles with or without anti-aliasing.

**What you should see:** Simple hard-edged single-pixel-wide circles all the way to antialiased thick-stroke circles.

**Hint:** Nothing here is new or shocking, it's just an extension of what you did with lines, above.

**Safety consideration:** It's harder to geometrically pre-clip a circle the way you can with a line. Potentially have a safe-circle (with per-pixel write safety testing) and an unsafe-circle implementation. Have a safe front-end that checks your parameters (centerpoint XY, radius) to see if safety is required. If not, you can use the unsafe variant. If so, call the safe variant and incur the safety test on every pixel drawn.

### Challenge 8 — Color Gradient Rectangle

Extend Challenge 1 to specify four colors, representing the four corners of the rectangle. Use contrasting colors to make sure it's working the way you anticipate.

**What you should see:** A color gradient rectangle on the black background.

**Hint:** In your outer Y loop, for each scan line drawn, compute the start and end color for the line, and interpolate the color across the line in your inner X loop.

### Challenge 9 — Unfilled/Unclosed Arbitrary Polygon

Taking a variable-length list of vertices (generally an array with a length/count specifier, possibly with a start-offset as well), iterate the vertex pairs, drawing lines between them. Optionally add a "force close"parameter to loop the final vertex back to the start to ensure a closed shape.

**What you should see:** An outline of a polygon.

**Hint:** You can reuse most of your existing line code for this, including pattern, width and anti-aliasing options if you wrote them. Clipping is performed at the line level, so no special magic is needed.


### Challenge 10 — Screen Hacks and Eurodemos

Now, put a number of these techniques together to replicate a mid-80s Amiga screen hack/demo. Draw tons of lines of random colors, overlapping boxes, gradients, copy random box regions elsewhere on the screen (maybe with scaling). Draw polygons made of randomly-moving vertices. These techniques, in 1985, were mind-blowing, and you will have replicated them yourself, from first-principles, on a dumb framebuffer.


### Optional Challenge 11 — Boss Level: Closed Filled Triangle

**Skip and go on to Challenge 12 if you don't feel up for this level of challenge.**

This challenge gets into the underpinnings that later allow for construction of a 3D graphics library. For this challenge, you must take three XY vertices and draw a polygon onscreen, filling it with pixels of the same unified color. Different outline and fill color is not supported, if you want this, draw the fill and then outline it a la Challenge 9.

**What you should see:** A filled, closed three-point polygon.

**Hint:** Clipping is different here. Line clipping can be done on the edges, but will discard a polygon that completely fills the screen. The solution is to run an axis-aligned-bounding-box (AABB) intersection test between the triangle's bounding rectangle and the bounds of the screen. If it's anything other than a total miss, then sort the vertices based on Y. Now iterate the vertical range of the triangle that overlaps the screen Y range, constructing X spans going from the triangle left edge to right, and clip these spans against the screen X limits. For those that intersect, iterate the X coordinate, filling as you go. Below is a pseudocode outline.

**Triangle Scanline Rasterizer Outline+Pseudocode**

#### Setup: Sort Vertices by Y

```
sort A, B, C so that Ay <= By <= Cy
```

This gives us a consistent top-to-bottom traversal and splits the triangle into at most two "trapezoid" sections at the middle vertex B.



#### The Two-Section Structure

A triangle has three edges. After Y-sorting, one edge runs the **full height** (A→C), and the other two cover the **upper half** (A→B) and **lower half** (B→C) respectively. B's Y coordinate is where we switch from the upper to the lower short edge.

```
long edge:        A → C       (spans all scanlines from Ay to Cy)
upper short edge: A → B       (spans Ay to By)
lower short edge: B → C       (spans By to Cy)
```

For each scanline Y, we interpolate X along whichever edges bracket that row, giving us the left and right X of the span.



#### Edge X Interpolation

For any edge from vertex P to vertex Q (where Py < Qy), the X position at scanline Y is:

```
t = (Y - Py) / (Qy - Py)        -- how far down the edge we are, 0..1
X = Px + t * (Qx - Px)          -- lerp X across the edge
```

To avoid recomputing this per scanline, accumulate with a step:

```
dx_per_row = (Qx - Px) / (Qy - Py)
x = Px  (at Y = Py, as a float/fixed-point value)
each row: x += dx_per_row
```



#### Assembling Spans: The Main Loop

```
function fill_triangle(A, B, C, color):

    -- Step 1: sort vertices by Y
    sort {A, B, C} by Y ascending

    -- Step 2: degenerate check
    if Ay == Cy: return              -- zero-height triangle, nothing to draw

    -- Step 3: precompute the long edge slope (A to C)
    long_dx = (Cx - Ax) / (Cy - Ay)
    long_x  = Ax                    -- current X on long edge, starts at A

    -- ---- Upper section: scanlines from A down to B ----

    if Ay < By:                     -- skip if flat top (A and B share a row)
        short_dx = (Bx - Ax) / (By - Ay)
        short_x  = Ax               -- current X on short edge, starts at A

        for Y from ceil(Ay) to ceil(By) - 1:

            draw_span(Y, long_x, short_x, color)

            long_x  += long_dx
            short_x += short_dx

    -- ---- Lower section: scanlines from B down to C ----

    if By < Cy:                     -- skip if flat bottom (B and C share a row)
        short_dx = (Cx - Bx) / (Cy - By)
        short_x  = Bx               -- current X on short edge, restarts at B

        for Y from ceil(By) to ceil(Cy) - 1:

            draw_span(Y, long_x, short_x, color)

            long_x  += long_dx
            short_x += short_dx
```

The long edge accumulator runs continuously through both sections without reset. Only the short edge is reinitialized at B.



#### Drawing Each Span (with Screen Clipping)

```
function draw_span(Y, x0, x1, color):

    -- Skip rows outside vertical screen bounds
    if Y < 0 or Y >= screen_height: return

    -- Determine left and right (interpolated edges can be either side)
    left  = min(x0, x1)
    right = max(x0, x1)

    -- Clip horizontally to screen bounds
    left  = max(left,  0)
    right = min(right, screen_width - 1)

    -- Skip if nothing remains after clipping
    if left > right: return

    -- Fill the span
    for X from ceil(left) to floor(right):
        plot(X, Y, color)
```



#### Notes on Edge Cases

**Flat top or flat bottom** triangles (where two vertices share a Y) are handled naturally — the relevant section's `if` guard skips the zero-height loop, and the other section runs normally.

**Degenerate (collinear) triangles** are caught by the early return when `Ay == Cy`.

**Left/right edge assignment** doesn't need to be determined in advance — `draw_span` resolves it with `min`/`max` each row. You can instead precompute which side the long edge falls on (compare Bx against the long edge X at By) and avoid the per-span branch if performance matters.

**Sub-pixel accuracy** is why we use `ceil` on the loop bounds and keep X as a float or fixed-point accumulator — this ensures we hit exactly the pixels whose centres lie inside the triangle, which is the correct fill convention and avoids both gaps and double-drawing at shared edges between adjacent triangles.



### Challenge 12 — Library API

Now, take ALL of the functions you implemented above, and put them into a single link library. An empty library skeleton is provided for you. Once you move all of your functions into it, you can simply link to it and call them from your test application. You should have a library prefix like pgpu_gl_ in front of all of your library functions, for safety.







