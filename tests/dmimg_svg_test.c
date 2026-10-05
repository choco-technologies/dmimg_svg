#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmimg.h"
#include <errno.h>
#include <string.h>

/*
 * The SVG files in fixtures/ decoded through dmimg - which loads dmimg_svg
 * by the extension.
 */

#ifndef DMIMG_SVG_FIXTURES_DIR
#define DMIMG_SVG_FIXTURES_DIR "fixtures"
#endif
#define FIXTURE(name)   DMIMG_SVG_FIXTURES_DIR "/" name

#define MAX_PIXELS      (160u * 200u)

#define RED             0xFFFF0000u
#define BLUE            0xFF0000FFu
#define GREEN           0xFF008000u

static uint32_t g_pixels[MAX_PIXELS];
static uint32_t g_width;
static uint32_t g_height;
static uint32_t g_blocks;
static uint32_t g_rows;                 /* Rows output, of all blocks */

static int collect(void* ctx, const dmimg_block_t* b)
{
    (void)ctx;
    g_blocks++;
    g_rows += b->height;
    if (b->x + b->width > g_width || b->y + b->height > g_height)
        return -100;
    for (uint32_t y = 0; y < b->height; y++)
        for (uint32_t x = 0; x < b->width; x++)
            g_pixels[(b->y + y) * g_width + b->x + x] = b->pixels[y * b->stride + x];
    return 0;
}

static uint32_t at(uint32_t x, uint32_t y)
{
    return g_pixels[y * g_width + x];
}

/* Decode a file at a scale into g_pixels; 0, the status of opening or decoding, -1 when it is too big */
static int decode(const char* path, uint8_t scale, dmimg_info_t* info)
{
    int status = 0;
    dmimg_t image = dmimg_open_file(path, info, &status);
    if (image == NULL)
        return (status != 0) ? status : -1;
    g_width = DMIMG_SCALED(info->width, scale);
    g_height = DMIMG_SCALED(info->height, scale);
    g_blocks = 0;
    g_rows = 0;
    memset(g_pixels, 0, sizeof(g_pixels));
    int ret = (g_width * g_height <= MAX_PIXELS) ? dmimg_decode(image, scale, collect, NULL) : -1;
    dmimg_close(image);
    if (ret != 0)
        Dmod_Printf("    %s: %d\n", path, ret);
    return ret;
}

/* The number of pixels of the halves (red | blue) of a w x h image that are not */
static int wrong_halves(uint32_t w, uint32_t h)
{
    int wrong = 0;
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
            wrong += at(x, y) != ((x < w / 2u) ? RED : BLUE);
    if (wrong != 0)
        Dmod_Printf("    (0, 0) is 0x%08X, (%u, 0) is 0x%08X\n", (unsigned)at(0, 0), (unsigned)(w - 1u), (unsigned)at(w - 1u, 0));
    return wrong;
}

DMOD_TEST_STEP(dmimg_svg_rasterizes_shapes)
{
    dmimg_info_t info;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("halves.svg"), 0, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.width, 8u);
    DMOD_TEST_EXPECT_EQ(info.height, 6u);
    DMOD_TEST_EXPECT_TRUE(info.alpha);
    DMOD_TEST_EXPECT_EQ(info.scales, (uint8_t)(DMIMG_SCALE(0) | DMIMG_SCALE(1) | DMIMG_SCALE(2) | DMIMG_SCALE(3)));
    DMOD_TEST_EXPECT_EQ(wrong_halves(8, 6), 0);
}

DMOD_TEST_STEP(dmimg_svg_scales_the_view_box)
{
    dmimg_info_t info;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("viewbox.svg"), 0, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.width, 16u);
    DMOD_TEST_EXPECT_EQ(info.height, 12u);
    DMOD_TEST_EXPECT_EQ(wrong_halves(16, 12), 0);

    /* Half and a quarter of it */
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("viewbox.svg"), 1, &info), 0);
    DMOD_TEST_EXPECT_EQ(wrong_halves(8, 6), 0);
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("viewbox.svg"), 2, &info), 0);
    DMOD_TEST_EXPECT_EQ(wrong_halves(4, 3), 0);
}

DMOD_TEST_STEP(dmimg_svg_parses_colors)
{
    dmimg_info_t info;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("colors.svg"), 0, &info), 0);
    DMOD_TEST_EXPECT_EQ(at(3, 0), 0xFFAABBCCu);         /* #abc */
    DMOD_TEST_EXPECT_EQ(at(3, 3), 0xFF0A141Eu);         /* rgb(10, 20, 30) in a style */
    DMOD_TEST_EXPECT_EQ(at(3, 5), 0xFFFAFAD2u);         /* lightgoldenrodyellow */

    /* Half opaque, not premultiplied */
    uint32_t half = at(3, 7);
    DMOD_TEST_EXPECT_EQ(half & 0x00FFFFFFu, 0x00FF0000u);
    DMOD_TEST_EXPECT_TRUE((half >> 24) >= 126u && (half >> 24) <= 129u);
}

DMOD_TEST_STEP(dmimg_svg_draws_gradients_in_the_shapes_box)
{
    dmimg_info_t info;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("gradient.svg"), 0, &info), 0);
    int wrong = 0;
    for (uint32_t x = 0; x < 20u; x++)
        wrong += at(x, 0) != at(x, 3);                  /* x2="1" is x2="100%" */
    DMOD_TEST_EXPECT_EQ(wrong, 0);
    DMOD_TEST_EXPECT_TRUE(((at(0, 0) >> 16) & 0xFFu) > 0xE0u && (at(0, 0) & 0xFFu) < 0x20u);    /* Red ... */
    DMOD_TEST_EXPECT_TRUE(((at(19, 0) >> 16) & 0xFFu) < 0x20u && (at(19, 0) & 0xFFu) > 0xE0u);  /* ... to blue */
}

DMOD_TEST_STEP(dmimg_svg_outputs_strips)
{
    dmimg_info_t info;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("tall.svg"), 0, &info), 0);
    DMOD_TEST_EXPECT_TRUE(g_blocks > 1u);               /* 100 x 200 is more than a strip */
    DMOD_TEST_EXPECT_EQ(g_rows, 200u);                  /* Every row once */
    int wrong = 0;
    for (uint32_t y = 0; y < 200u; y++)
        for (uint32_t x = 0; x < 100u; x++)
            wrong += (y < 100u) ? at(x, y) != GREEN : (at(x, y) >> 24) != 0u;
    DMOD_TEST_EXPECT_EQ(wrong, 0);
}

DMOD_TEST_STEP(dmimg_svg_draws_dashed_rotated_strokes)
{
    /* The gauge: a ring of 270 degrees from the bottom left to the bottom right */
    dmimg_info_t info;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("gauge.svg"), 0, &info), 0);
    DMOD_TEST_EXPECT_EQ(at(80, 12), 0xFF1E293Bu);       /* The top of the ring */
    DMOD_TEST_EXPECT_EQ(at(12, 80), 0xFF1E293Bu);       /* Its left */
    DMOD_TEST_EXPECT_EQ(at(148, 80), 0xFF1E293Bu);      /* Its right */
    DMOD_TEST_EXPECT_EQ(at(80, 148) >> 24, 0u);         /* The gap at the bottom */
    DMOD_TEST_EXPECT_EQ(at(80, 80) >> 24, 0u);          /* Nothing inside */
    DMOD_TEST_EXPECT_EQ(at(2, 2) >> 24, 0u);
}

DMOD_TEST_STEP(dmimg_svg_rejects_what_is_not_an_image)
{
    dmimg_info_t info;
    int status = 0;
    DMOD_TEST_EXPECT_TRUE(dmimg_open_file(FIXTURE("not_svg.svg"), &info, &status) == NULL);
    DMOD_TEST_EXPECT_EQ(status, -EBADMSG);
    DMOD_TEST_EXPECT_TRUE(dmimg_open_file(FIXTURE("no_size.svg"), &info, &status) == NULL);
    DMOD_TEST_EXPECT_EQ(status, -EBADMSG);
}
