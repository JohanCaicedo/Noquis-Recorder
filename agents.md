# AGENTS.md

This file provides system instructions and domain context to AI coding assistants working on the AI-Link Capture (Capturadora) project.

## build restrictions
- Never use cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=g:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
- Never use cmake --build build --config Release

## Architectural Rules (What to do and What NOT to do)
- **Zero-Buffer Logic:** Do NOT introduce frame queues, disk writes, or buffering delays in the video or audio pipeline. The exact goal is ultra-low latency, so frames must flow directly from ingestion -> VRAM -> display.
- **GPU Format Constraints:** 
  - `GPUUpscaler` works over **RGBA** (`NVCV_RGBA`, 4 channels) buffers.
  - `GPUDenoiser` works over **BGR** (`NVCV_BGR`, 3 channels) buffers. 
  Do NOT mix these formats; check `CV_8UC3` conversions and `NVWrapperForCVMat` mappings.
- **NVIDIA SDK Dependencies:** Do NOT remove the dynamic `PATH` system environment manipulation found in `GPUUpscaler.cpp` or `GPUDenoiser.cpp`. Functions like `NvVFX_CreateEffect` strictly rely on the process `PATH` to locate the dynamically linked `.dll` files within the `NVIDIA Video Effects\bin` directory.
- **Memory Management:** For every `NvCVImage_Alloc` or `cudaMalloc` allocation, there MUST be a corresponding `NvCVImage_Dealloc` or `cudaFree` call in the `release()` method of the respective class to prevent GPU VRAM leaks.
- **Audio Bridge:** The app uses `miniaudio` to provide a direct-to-headset link. Do NOT add explicit audio-video synchronization logic since caching audio packets destroys the < 10ms latency requirement.

## Feature Modifications
- **UI / Zen Mode:** The user interface uses native windowing mechanisms to render transparently or via OpenCV `imshow`. Zen Mode is designed to be invisible for gaming (fullscreen borderless, letterboxed mapping 16:9 to 16:10). Do NOT add bulky GUI toolkits (Qt, ImGui, etc.) unless explicitly instructed by the user.
- **OpenCV Backend:** USB device capture relies on forcing the `CAP_MSMF` backend into the MJPEG format. This is vital to reach 60FPS on a USB 2.0 bandwidth limit. Do NOT switch this out for conventional `CAP_DSHOW` or raw format captures.

## PR, Commits, and Debugging instructions
- Title format: `[Capturadora] <Title>`
- If modifying NVIDIA VFX logic, pay close attention to the SDK `STATE_SIZE` query lifecycle and memory allocations occurring *after* `NvVFX_Load()`.
- Always verify model pathing during testing; missing models will throw `The file could not be found` errors.
## NVIDIA VIDEO EFFECTS FOLDER STRUCTURE
Example VFX SDK features (non-exhaustive, case-insensitive):
  * nvvfxvideosuperres      - Video super resolution
  * nvvfxdenoising          - Video denoising
  * nvvfxupscale            - Video upscaling
  * nvvfxgreenscreen        - AI green screen
  * nvvfxbackgroundblur     - Background blur
  * nvvfxrelighting         - Relighting
  * nvvfxaigsrelighting     - AI green screen relighting