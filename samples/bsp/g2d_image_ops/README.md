# g2d_image_ops

Single-image regression sample for `IGraphics2D`.

```bash
# RK3588 (RGA)
g2d_image_ops \
  --image /path/to/input.png \
  --output-dir /tmp/g2d_image_ops \
  --g2d rkrga

# Jetson Orin NX (VIC)
g2d_image_ops \
  --image /path/to/input.png \
  --output-dir /tmp/g2d_image_ops \
  --g2d nvvic
```

`--g2d` defaults to `nvvic` on Jetson builds and `rkrga` otherwise.

The sample normalizes the input to `RGBA8888`, runs the public G2D APIs, writes
PNG outputs, and compares supported operations with OpenCV reference images.
Unsupported capabilities are reported as `SKIP`.

Covered operations:

- copy
- resize
- crop
- resize-to-rect via `imageBlit`
- fill
- rectangle
- alpha blend
- rotate 90/180/270
- flip H/V/HV
- color conversion
- async job
- batched job

Notes:

- RK RGA supports fixed rotations and flips, but not perspective or affine warp.
- RGA has alignment and minimum-size requirements. The sample pads small or
  unaligned inputs before running hardware operations.
- On RK3588, fill/rectangle run on the RGA2 core, which can only access memory
  below 4 GB. The sample therefore allocates its buffers from a DMA32 heap.
- On Jetson, the transform operations (copy, resize, crop, blit, rotate, flip,
  color conversion) run on the VIC. VIC has no fill/draw/blend primitive, so
  `imageFill`, `imageDrawRectangle` and `imageBlend` run on the CPU against the
  mapped surface (RGBA8888/BGRA8888 only).
- VIC has no job list, so `createJob()` is unsupported: `async_job` and
  `batch_job` are reported as `SKIP` on Jetson.
- VIC rotation/flip direction follows NVIDIA's `07_video_convert` sample, whose
  documentation contradicts the `nvbufsurftransform.h` comments. If `rotate90`/
  `rotate270` or `flip_h`/`flip_v` fail on a device, swap the mapping in
  `toVicTransform()` in `NvVicGraphics2D.cpp`.
- Rotate 90/270 combined with a single-axis mirror is not supported on VIC.
