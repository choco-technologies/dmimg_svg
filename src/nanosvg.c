/*
 * nanosvg's parser and rasterizer (third_party/nanosvg), on the C library
 * of svg_libc.c.
 */

#include "svg_libc.h"

#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
