# dmimg_svg

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmimg_svg/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmimg_svg/actions/workflows/ci.yml)

The SVG decoder of [dmimg](https://github.com/choco-technologies/dmimg) -
a rasterizer.

## Description

A dmimg decoder plugin: once it is enabled - or when
`dmimg_open_file()` meets an `.svg` file and loads it by the name - every
program that reads images through dmimg reads SVG files, as pixels.

- **What is drawn**: paths and the basic shapes (rect with rounded corners,
  circle, ellipse, line, polyline, polygon), fills (non-zero, even-odd),
  strokes (joins, caps, miter limit, dashes), linear and radial gradients,
  opacity, transforms, groups, presentation attributes, `style` and simple
  class rules of `<style>`; anti-aliased.
- **What is not**: text, `<image>`, `<use>`, clip paths, masks, patterns,
  filters, markers - they are left out, the rest is drawn.
- **Size**: the SVG's `width` and `height` (px, pt, pc, mm, cm, in at 96
  dpi); a percentage or none: the `viewBox`'s; no viewBox either: the
  shapes'. Rounded up, at most 8192 x 8192. An SVG is drawn at any size as
  cheaply, so every scale (1, 1/2, 1/4, 1/8) is offered - to draw it at
  another size, give it that `width` and `height`.
- **Memory**: the document is read whole (at most 4 MiB) and parsed into
  its shapes; the text is then released. The pixels are rasterized a strip
  at a time - 8192 of them (32 KiB) - never the whole image.
- `info.alpha` is always set: what the SVG does not cover is transparent.

Built on [nanosvg](https://github.com/memononen/nanosvg) (zlib), slightly
altered - see [third_party/nanosvg](third_party/nanosvg).

## Usage

```c
#include "dmimg.h"

dmimg_info_t info;
dmimg_t image = dmimg_open_file("/flash/icon.svg", &info, NULL);     /* loads dmimg_svg */
if (image != NULL)
{
    dmimg_decode(image, 0, put_block, ctx);                          /* strips of rows */
    dmimg_close(image);
}
```

## Building

### Using CMake

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

### Using Make

```bash
make DMOD_MODE=DMOD_MODULE DMOD_DIR=/path/to/dmod
```

## Testing

The tests rasterize the SVG files in [tests/fixtures](tests/fixtures)
through dmimg - shapes, viewBox and scales, colors, gradients, strips, a dashed and
rotated stroke, files that are not images:

```bash
cd build
ctest --output-on-failure
```

## License

MIT - see [LICENSE](LICENSE); nanosvg: [third_party/nanosvg/LICENSE](third_party/nanosvg/LICENSE).
