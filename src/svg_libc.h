#ifndef SVG_LIBC_H
#define SVG_LIBC_H

/*
 * The C library nanosvg takes (third_party/nanosvg), for a dmod module: it
 * has only dmod's string functions (memcpy, strlen, strcmp, strchr, ...),
 * no libm, no stdio. Included before nanosvg.h and nanosvgrast.h (see
 * src/nanosvg.c): the system headers first - nanosvg's own includes of them
 * are then empty - and the functions it calls renamed to the ones in
 * svg_libc.c.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "dmod.h"

void*       dmimg_svg_malloc(size_t size);
void*       dmimg_svg_realloc(void* p, size_t size);
void        dmimg_svg_free(void* p);
long        dmimg_svg_strtol(const char* s, char** end, int base);
long long   dmimg_svg_strtoll(const char* s, char** end, int base);
char*       dmimg_svg_strstr(const char* s, const char* what);
int         dmimg_svg_sscanf(const char* s, const char* format, ...);
void        dmimg_svg_qsort(void* base, size_t count, size_t size, int (*compare)(const void*, const void*));
float       dmimg_svg_sqrtf(float x);
double      dmimg_svg_sqrt(double x);
float       dmimg_svg_fabsf(float x);
double      dmimg_svg_fabs(double x);
float       dmimg_svg_floorf(float x);
float       dmimg_svg_ceilf(float x);
float       dmimg_svg_roundf(float x);
float       dmimg_svg_fmodf(float x, float y);
float       dmimg_svg_sinf(float x);
float       dmimg_svg_cosf(float x);
float       dmimg_svg_tanf(float x);
float       dmimg_svg_atan2f(float y, float x);
float       dmimg_svg_acosf(float x);
double      dmimg_svg_pow(double x, double y);

#define malloc      dmimg_svg_malloc
#define realloc     dmimg_svg_realloc
#define free        dmimg_svg_free
#define strtol      dmimg_svg_strtol
#define strtoll     dmimg_svg_strtoll
#define strstr      dmimg_svg_strstr
#define sscanf      dmimg_svg_sscanf
#define qsort       dmimg_svg_qsort
#define sqrtf       dmimg_svg_sqrtf
#define sqrt        dmimg_svg_sqrt
#define fabsf       dmimg_svg_fabsf
#define fabs        dmimg_svg_fabs
#define floorf      dmimg_svg_floorf
#define ceilf       dmimg_svg_ceilf
#define roundf      dmimg_svg_roundf
#define fmodf       dmimg_svg_fmodf
#define sinf        dmimg_svg_sinf
#define cosf        dmimg_svg_cosf
#define tanf        dmimg_svg_tanf
#define atan2f      dmimg_svg_atan2f
#define acosf       dmimg_svg_acosf
#define pow         dmimg_svg_pow

/* nsvgParseFromFile(), which dmimg_svg does not use, through dmod's files */
#define fopen       Dmod_FileOpen
#define fseek       Dmod_FileSeek
#define ftell       Dmod_FileTell
#define fread       Dmod_FileRead
#define fclose      Dmod_FileClose

#endif /* SVG_LIBC_H */
