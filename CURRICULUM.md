# PixelGPU Curriculum

A staged curriculum for building a **virtual graphics card, driver stack, and compatibility platform** from first principles.

The sequence intentionally begins with a **single pixel in a fake framebuffer** and evolves toward a **virtual fixed-function 3D accelerator with legacy API compatibility and optional host GPU acceleration**.

Each phase preserves prior abstractions while deepening the realism of the underlying “hardware.”

---

## Phase 1 — Virtual Display Hardware

**Objective:** establish the illusion of graphics hardware.

Build a desktop application using SDL (or similar) that creates a window and treats a block of memory as **VRAM**.

### Topics
- framebuffer memory layout
- width, height, stride, and pixel formats
- scanout and presentation
- refresh timing
~~- double buffering~~ (deferred until later phases)
- mode setting

### Deliverables
- windowed display output
- linear framebuffer byte array
- present-once-per-frame loop
- basic mode descriptor

### Learning Outcomes
Students understand how display memory becomes visible pixels.

---

## Phase 2 — Dumb 2D Framebuffer Driver

**Objective:** implement classic software rendering primitives.

All drawing is performed in software through a driver library targeting the virtual framebuffer. The features implemented will mimic a subset of a basic 2D hardware-accelerated graphics co-processor similar to the Amiga Agnus ( https://en.wikipedia.org/wiki/MOS_Technology_Agnus ) chip, capable of rectangular memory transfer operations and line drawing without CPU attention.

### Topics
- pixel plot and readback
- Bresenham lines
- rectangle outline and fill with solid colors
- basic pixel, rectangle and line clipping
- blits and copies
- transparent blits (possible stretch goal: multichannel blit opertors a la Amiga Agnus Blitter)
- bitmap text
- color conversion

### Sub-phases
- Phase 2a: Program-space implementation of functionality
- Phase 2b: Abstraction of program-space implementation into a semi-opaque link library behind a defined API (still presenting as CPU-driven)

### Deliverables
- reusable 2D driver library (Phase 2b)
- primitive demo scenes
- clipping and overlap tests

### Learning Outcomes
Students experience rasterization edge cases and correctness constraints.

---

## Phase 3 — Driver and Hardware Split

**Objective:** separate API from device implementation.

The virtual hardware becomes a device-like abstraction with command submission instead of direct drawing.

### Topics
- command packets
- MMIO-style registers
- command FIFO / ring buffer
- status flags
- fences and events
- hardware reset paths

### Deliverables
- command queue protocol
- fake hardware registers
- command processor loop
- interrupt/event simulation

### Learning Outcomes
Students learn why real drivers submit commands instead of touching hardware state directly.

---

## Phase 4 — Userspace and Kernel Boundary

**Objective:** model the operating-system driver boundary.

Refactor the project so command submission, memory mapping, and synchronization resemble a real graphics stack.

### Topics
- memory-mapped VRAM
- user/kernel separation
- synchronization primitives
- device files or service layer
- fault isolation
- privilege boundaries

### Deliverables
- userspace driver API
- optional Linux kernel module or character device
- mmap-style framebuffer access

### Learning Outcomes
Students understand the systems architecture around graphics devices.

---

## Phase 5 — Software 3D Pipeline

**Objective:** implement fixed-function style 3D in software.

The virtual hardware is extended into a Voodoo-like pipeline.

### Topics
- transforms and matrices
- primitive assembly
- triangle rasterization
- barycentric interpolation
- z-buffering
- perspective correction
- texture sampling
- culling and clipping

### Deliverables
- indexed triangle renderer
- textured cube demo
- depth-tested scene rendering

### Learning Outcomes
Students build a real triangle pipeline and understand why GPUs are structured this way.

---

## Phase 6 — Compatibility Layer

**Objective:** make real software target PixelGPU.

Expose the fake hardware through an API layer that external applications can use.

### Topics
- API surface design
- MiniGL-style wrapper
- Glide-inspired subset
- raylib backend
- legacy software integration

### Deliverables
- one compatibility backend
- one real demo or retro application target
- sample application SDK

### Learning Outcomes
Students learn API design, compatibility tradeoffs, and subset engineering.

---

## Phase 7 — Hidden Hardware Acceleration

**Objective:** preserve the virtual device while swapping in faster internals.

The guest-visible hardware contract stays unchanged while the implementation underneath becomes accelerated.

### Topics
- host OpenGL backend
- shader-based fixed-function emulation
- backend abstraction layers
- software vs hardware parity
- backend feature negotiation

### Deliverables
- interchangeable backend system
- CPU backend
- accelerated GPU-backed backend

### Learning Outcomes
Students understand how virtualization and translation layers preserve compatibility.

---

## Phase 8 — Optimization and Profiling

**Objective:** transform correctness into performance.

Students profile the entire stack and iteratively improve throughput and latency.

### Topics
- dirty rectangles
- tiled rendering
- SIMD
- batching
- cache locality
- write combining
- multithreaded command generation
- frame pacing

### Deliverables
- benchmark suite
- profiler traces
- before/after performance reports

### Learning Outcomes
Students learn how architectural bottlenecks shape real graphics systems.

---

## Phase 9 — Real-World Architecture Comparisons

**Objective:** connect the toy design to historical and modern hardware.

The final phase maps PixelGPU concepts to real systems.

### Topics
- VGA and SVGA framebuffers
- 2D accelerators
- Voodoo and fixed-function pipelines
- software rasterizers
- virtual GPUs
- translation layers
- modern explicit APIs

### Deliverables
- comparative architecture writeup
- historical hardware case studies
- reflection on simplifications vs realism

### Learning Outcomes
Students understand where the toy architecture faithfully mirrors real-world systems.

---

## Final Outcome

By the end of the curriculum, students will have built:

- a virtual graphics card
- a 2D and 3D driver stack
- a command submission model
- a compatibility layer for real applications
- interchangeable software and hardware-accelerated backends
- a performance-tested graphics subsystem

The curriculum is designed to teach **graphics, systems architecture, virtualization, and performance engineering as one continuous engineering story**.

