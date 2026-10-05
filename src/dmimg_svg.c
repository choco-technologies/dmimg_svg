#define DMOD_ENABLE_REGISTRATION    ON
#include "dmimg.h"
#include "svg_libc.h"
#include "nanosvg.h"
#include "nanosvgrast.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

/*
 * dmimg_svg - the SVG decoder of dmimg, on nanosvg (third_party/nanosvg,
 * zlib): the document is parsed into its shapes - paths, with their fills,
 * strokes and gradients - and rasterized, with anti-aliasing, a strip of
 * rows at a time. It keeps the shapes and one strip of 0xAARRGGBB pixels
 * (STRIP_PIXELS), never the image.
 *
 * The size of the image is the SVG's width and height (px, pt, mm, ... at
 * 96 dpi; a percentage or none: the viewBox's). An SVG is drawn smaller as
 * cheaply as whole, so every scale is offered.
 */

#define READ_CHUNK      1024u
#define MAX_DOCUMENT    (4u * 1024u * 1024u)    /* Bytes of an SVG file at most */
#define MAX_SIZE        8192u                   /* Width or height at most */
#define STRIP_PIXELS    8192u                   /* Pixels rasterized at a time (32 KiB) */
#define DPI             96.0f

struct dmimg_decoder
{
    NSVGimage*  image;
    uint32_t    width;
    uint32_t    height;
};

static bool is_space(uint8_t c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool starts(const uint8_t* p, const uint8_t* end, const char* text)
{
    size_t n = strlen(text);
    return (size_t)(end - p) >= n && memcmp(p, text, n) == 0;
}

/* The whole document, NUL-terminated (nanosvg parses it in place); NULL with *status set */
static char* read_document(const dmimg_input_t* input, int* status)
{
    size_t capacity = (input->size != 0 && input->size < MAX_DOCUMENT) ? input->size + 1u : READ_CHUNK;
    size_t length = 0;
    char* text = Dmod_Malloc(capacity);
    for (;;)
    {
        if (text == NULL)
        {
            *status = -ENOMEM;
            return NULL;
        }
        if (length + 1u == capacity)
        {
            if (capacity > MAX_DOCUMENT)
            {
                Dmod_Free(text);
                *status = -ENOTSUP;
                return NULL;
            }
            char* bigger = Dmod_Realloc(text, capacity * 2u);
            if (bigger == NULL)
                Dmod_Free(text);
            text = bigger;
            capacity *= 2u;
            continue;
        }
        int32_t n = input->read(input->ctx, text + length, capacity - 1u - length);
        if (n < 0)
        {
            Dmod_Free(text);
            *status = -EIO;
            return NULL;
        }
        if (n == 0)
            break;
        length += (size_t)n;
    }
    text[length] = '\0';
    return text;
}

/* ---- DIF ---- */

/* An XML declaration, a comment, a doctype or <svg> - after a byte order mark and white space */
dmod_dmimg_dif_api_declaration(1.0, dmimg_svg, bool, _probe, ( const uint8_t* head, size_t size ))
{
    const uint8_t* p = head;
    const uint8_t* end = head + size;
    if (starts(p, end, "\xEF\xBB\xBF"))
        p += 3;
    while (p < end && is_space(*p))
        p++;
    return starts(p, end, "<svg") || starts(p, end, "<?xml") || starts(p, end, "<!--") || starts(p, end, "<!DOCTYPE svg");
}

dmod_dmimg_dif_api_declaration(1.0, dmimg_svg, dmimg_decoder_t, _open,
                               ( const dmimg_input_t* input, dmimg_info_t* info, int* status ))
{
    char* text = read_document(input, status);
    if (text == NULL)
        return NULL;
    bool svg = dmimg_svg_strstr(text, "<svg") != NULL;
    NSVGimage* image = svg ? nsvgParse(text, "px", DPI) : NULL;
    Dmod_Free(text);
    if (image == NULL)
    {
        *status = svg ? -ENOMEM : -EBADMSG;
        return NULL;
    }

    /* Its size, rounded up (nanosvg: the width / height, or the viewBox's, or the shapes') */
    float w = image->width, h = image->height;
    if (!(w >= 0.5f && h >= 0.5f && w <= (float)MAX_SIZE && h <= (float)MAX_SIZE))
    {
        nsvgDelete(image);
        *status = (w > (float)MAX_SIZE || h > (float)MAX_SIZE) ? -ENOTSUP : -EBADMSG;
        return NULL;
    }
    struct dmimg_decoder* d = Dmod_Malloc(sizeof(*d));
    if (d == NULL)
    {
        nsvgDelete(image);
        *status = -ENOMEM;
        return NULL;
    }
    d->image = image;
    d->width = (uint32_t)(w + 0.999f);
    d->height = (uint32_t)(h + 0.999f);

    info->width = d->width;
    info->height = d->height;
    info->alpha = true;
    info->scales = DMIMG_SCALE(0) | DMIMG_SCALE(1) | DMIMG_SCALE(2) | DMIMG_SCALE(3);
    return d;
}

dmod_dmimg_dif_api_declaration(1.0, dmimg_svg, int, _decode,
                               ( dmimg_decoder_t d, uint8_t scale, dmimg_output_fn output, void* ctx ))
{
    if (scale > DMIMG_MAX_SCALE || output == NULL)
        return -EINVAL;
    uint32_t w = DMIMG_SCALED(d->width, scale);
    uint32_t h = DMIMG_SCALED(d->height, scale);
    uint32_t rows = STRIP_PIXELS / w;
    if (rows == 0)
        rows = 1;
    if (rows > h)
        rows = h;

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    uint32_t* strip = Dmod_Malloc((size_t)w * rows * sizeof(uint32_t));
    if (rast == NULL || strip == NULL)
    {
        if (rast != NULL)
            nsvgDeleteRasterizer(rast);
        if (strip != NULL)
            Dmod_Free(strip);
        return -ENOMEM;
    }

    int ret = 0;
    float factor = 1.0f / (float)(1u << scale);
    for (uint32_t y = 0; y < h && ret == 0; y += rows)
    {
        uint32_t n = (h - y < rows) ? h - y : rows;
        uint8_t* rgba = (uint8_t*)strip;
        nsvgRasterize(rast, d->image, 0.0f, -(float)y, factor, rgba, (int)w, (int)n, (int)(w * 4u));

        /* RGBA bytes (not premultiplied) into 0xAARRGGBB, in place */
        for (uint32_t i = 0; i < w * n; i++)
        {
            const uint8_t* p = rgba + i * 4u;
            strip[i] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
        }
        dmimg_block_t block = { 0, y, w, n, w, strip };
        ret = output(ctx, &block);
    }

    Dmod_Free(strip);
    nsvgDeleteRasterizer(rast);
    return ret;
}

dmod_dmimg_dif_api_declaration(1.0, dmimg_svg, void, _close, ( dmimg_decoder_t d ))
{
    nsvgDelete(d->image);
    Dmod_Free(d);
}

/* ---- Module ---- */

int dmod_init(const Dmod_Config_t* Config)
{
    (void)Config;
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
