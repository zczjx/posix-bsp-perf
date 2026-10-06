# g2d_image_ops

Single-image regression sample for `IGraphics2D`.

```bash
g2d_image_ops \
  --image /path/to/input.png \
  --output-dir /tmp/g2d_image_ops \
  --g2d rkrga
```

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
- On Jetson, newly added RGA-specific APIs may report `SKIP` until VIC support is
  implemented.
