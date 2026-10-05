# nanosvg

[nanosvg](https://github.com/memononen/nanosvg) by Mikko Mononen - zlib,
see [LICENSE](LICENSE). The SVG parser (`nanosvg.h`) and rasterizer
(`nanosvgrast.h`) of commit `239e102ec2c691f2902e20ace2ed36ee4a35cfe6`.

**Altered** (marked `dmimg_svg:` in `nanosvg.h`):

- `NSVGNamedColor.name` is `char[21]`, not `const char*`: a dmod module's
  data is not relocated when it is loaded (only its GOT is), so the table of
  color names cannot hold pointers;
- in `nsvg__createGradient()`, a number without a unit in a gradient of
  `gradientUnits="objectBoundingBox"` (the default) is a fraction of the
  shape's box - `x2="1"` is its right edge, as `x2="100%"` is. nanosvg took
  it for user units (pixels).

dmimg_svg builds both headers in `src/nanosvg.c`, with
`NANOSVG_ALL_COLOR_KEYWORDS`, on the C library of `src/svg_libc.c`
(allocations through dmod, the string functions dmod lacks, float math -
see `src/svg_libc.h`).
