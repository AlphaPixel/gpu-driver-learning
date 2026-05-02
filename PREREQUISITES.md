# PixelGPU Prerequisites

This document covers the minimum pre-project study material students should review before starting PixelGPU.

It only includes early phase prerequisite sections to be less daunting:

1. Framebuffers, pixel memory, and raster basics
2. The classic graphics pipeline in one page

The goal is not mastery. The goal is to give students just enough conceptual footing to start building a virtual framebuffer and understand how pixels are generated.

---

## 1. Framebuffers, Pixel Memory, and Raster Basics

Estimated time: 60–90 minutes

This is the most important starting point.

Students should understand:

- image width and height
- bytes per pixel
- pixel formats such as RGB, BGR, RGBA, and BGRA
- row pitch / stride
- top-down vs bottom-up layout
- clipping to image bounds
- scanout as “copy memory to the display”
- why a framebuffer is just structured memory

### Recommended reading

#### Peter Occil — *Graphics and Music Challenges for Classic-Style Computer Applications*
<https://peteroupc.github.io/graphics.html#Graphics_Challenge_for_Classic_Style_Games>

#### Scratchapixel — *An Overview of the Rasterization Algorithm*
This is the best text-first foundation for the project. It explains what rasterization is, why it exists, and how it maps geometry onto a pixel grid. Read this first.  
<https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/overview-rasterization-algorithm.html>

#### Scratchapixel — *The Rasterization Stage*
This follows naturally from the overview and explains what rasterization is actually trying to solve: determining which pixels are covered and what values should be written there.  
<https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/rasterization-stage.html>

### Recommended focused reading

#### Scratchapixel — *Rasterization: a Practical Implementation*
Useful as a second pass once the overview makes sense. This is more implementation-oriented and starts to bridge theory to code.  
<https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/rasterization-practical-implementation.html>

#### Scratchapixel — *Barycentric Coordinates*
Students do not need full 3D math yet, but they should at least understand that triangle coverage and interpolation often rely on barycentric coordinates. This becomes important later for triangle rasterization.  
<https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/barycentric-coordinates.html>

### Optional supplement

#### GeeksforGeeks — *Bresenham’s Line Generation Algorithm*
This is a simpler, shorter read for one of the classic primitive raster algorithms students may implement early. Use it as a quick reference, not as the primary conceptual source.  
<https://www.geeksforgeeks.org/bresenhams-line-generation-algorithm/>

### What students should be able to explain afterward

- what a framebuffer is
- why stride may differ from width × bytes-per-pixel
- how a pixel address is computed from x/y coordinates
- what clipping means in software rendering
- the difference between a geometric primitive and the pixels it covers

---

## 2. The Classic Graphics Pipeline in One Page

Estimated time: 30–45 minutes

Students do not need a full OpenGL or Direct3D course before starting. They only need one clean mental model of how graphics work from input geometry to final pixels.

They should understand the broad sequence:

- vertex input
- primitive assembly
- rasterization
- fragment generation
- depth / visibility tests
- framebuffer writeout

This is enough context to make later PixelGPU phases intelligible.

### Required reading

#### LearnOpenGL — *Hello Triangle*
This is one of the clearest compact explanations of the graphics pipeline available in text form. Even though PixelGPU does not begin as an OpenGL project, this page explains the major pipeline stages clearly enough to give students the right mental model. Focus on the pipeline overview, not on the API details.  
<https://learnopengl.com/Getting-started/Hello-Triangle>

### Recommended focused reading

#### LearnOpenGL — *OpenGL*
This page is useful for understanding what a graphics API is, what a specification is, and how the API sits between application code and an implementation. Read it for architecture, not for OpenGL specifics.  
<https://learnopengl.com/Getting-started/OpenGL>

#### LearnOpenGL — *Review*
This is a compact glossary-style refresher that is useful after reading the previous page. It helps students consolidate terms such as viewport, graphics pipeline, shader, and vertex data without needing a long lecture.  
<https://learnopengl.com/Getting-started/Review>

### Optional deeper reading

#### Scratchapixel — *Projection Stage*
This starts to connect higher-level pipeline ideas to the actual transformation from vertices to raster space. It is more relevant once students begin thinking about triangles rather than only pixels and rectangles.  
<https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/projection-stage.html>

#### Scratchapixel — *Visibility Problem, Depth Buffer, and Depth Interpolation*
This is optional before the project starts, but it gives a preview of how visibility is resolved and why later 3D stages need a z-buffer.  
<https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/visibility-problem-depth-buffer-depth-interpolation.html>

### What students should be able to explain afterward

- the difference between vertices, primitives, fragments, and pixels
- what rasterization does in the pipeline
- why visibility/depth testing happens before final framebuffer writes
- why the project can begin with pixels and still eventually grow into a 3D pipeline

---

## Guidance for Students

Do not over-study before starting the code.

After reviewing the material above, students should move directly into an implementation milestone such as:

- opening a window with SDL
- allocating a framebuffer in memory
- plotting pixels
- drawing lines and rectangles
- saving or comparing image output in tests

That is enough foundation to begin PixelGPU.

