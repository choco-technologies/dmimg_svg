# dmimg_svg API Reference

dmimg_svg has no API of its own: it implements the dmimg DIF, and programs
use it through dmimg (`dmimg_open()`, `dmimg_open_file()` - see dmimg's
api-reference.md).

| DIF function | Behavior |
|--------------|----------|
| `_probe` | After a UTF-8 byte order mark and white space: `<svg`, `<?xml`, `<!--` or `<!DOCTYPE svg` |
| `_open` | Reads the whole document (at most 4 MiB, `-ENOTSUP` beyond) and parses it: width, height (see below), `alpha` = true, `scales` = `DMIMG_SCALE(0 ... 3)`. `-EBADMSG` for a document without `<svg` or without a size, `-ENOTSUP` for one larger than 8192 x 8192, `-EIO`, `-ENOMEM` |
| `_decode` | Any scale 0 ... 3 (`-EINVAL` otherwise): the image drawn at 1 / 2^scale. One block per strip of rows, in order, each as wide as the image - as many rows as make 8192 pixels (at least one). `-ENOMEM` |
| `_close` | Releases the decoder and the shapes |

## Size

The SVG's `width` and `height` in px, pt, pc, mm, cm or in (at 96 dpi). A
percentage or no size: the `viewBox`'s. No viewBox either: the bounds of
the shapes. Rounded up to whole pixels.

## Memory

- the document while it is parsed (its size + 1 byte), then the shapes:
  their points and paints;
- while decoding: the rasterizer (its edges and points, growing with the
  most complex shape, and one scanline) and one strip of 8192 0xAARRGGBB
  pixels (32 KiB);
- about 3 KiB of stack on a 32-bit target (parsing a `style`; rasterizing: 1.5 KiB).

Nothing depends on the image's height.
