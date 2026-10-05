#include "svg_libc.h"
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>

/*
 * The C library nanosvg takes (see svg_libc.h): allocations through dmod,
 * the few string functions dmod lacks, and the float math nanosvg uses -
 * accurate to about a unit in the last place of a float, which is far
 * finer than the pixels it is rasterized into.
 */

#define PI          3.14159265358979f
#define HALF_PI     1.57079632679490f
#define TWO_PI_HI   6.28125f                    /* 2 pi in two parts: exact multiples of the first */
#define TWO_PI_LO   0.00193530717958647f
#define TAN_PI_12   0.26794919243112f           /* 2 - sqrt(3) */
#define SQRT_3      1.73205080756888f

/* ---- Memory ---- */

void* dmimg_svg_malloc(size_t size)
{
    return Dmod_Malloc((size != 0) ? size : 1u);
}

void* dmimg_svg_realloc(void* p, size_t size)
{
    if (p == NULL)
        return dmimg_svg_malloc(size);
    if (size == 0)
    {
        Dmod_Free(p);
        return NULL;
    }
    return Dmod_Realloc(p, size);
}

void dmimg_svg_free(void* p)
{
    if (p != NULL)
        Dmod_Free(p);
}

/* ---- Strings ---- */

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/* The value of a digit in bases up to 36, -1 for anything else */
static int digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return -1;
}

long long dmimg_svg_strtoll(const char* s, char** end, int base)
{
    const char* p = s;
    while (is_space(*p))
        p++;
    bool negative = *p == '-';
    if (*p == '-' || *p == '+')
        p++;
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && digit(p[2]) >= 0 && digit(p[2]) < 16)
    {
        p += 2;
        base = 16;
    }
    else if (base == 0)
        base = (p[0] == '0') ? 8 : 10;

    unsigned long long limit = negative ? (unsigned long long)LLONG_MAX + 1u : (unsigned long long)LLONG_MAX;
    unsigned long long value = 0;
    bool any = false, overflow = false;
    for (int d; (d = digit(*p)) >= 0 && d < base; p++)
    {
        any = true;
        if (value > (limit - (unsigned long long)d) / (unsigned long long)base)
            overflow = true;
        else
            value = value * (unsigned long long)base + (unsigned long long)d;
    }
    if (end != NULL)
        *end = (char*)(any ? p : s);
    if (overflow)
        return negative ? LLONG_MIN : LLONG_MAX;
    if (negative)
        return (value == (unsigned long long)LLONG_MAX + 1u) ? LLONG_MIN : -(long long)value;
    return (long long)value;
}

long dmimg_svg_strtol(const char* s, char** end, int base)
{
    long long v = dmimg_svg_strtoll(s, end, base);
    if (v > LONG_MAX)
        return LONG_MAX;
    if (v < LONG_MIN)
        return LONG_MIN;
    return (long)v;
}

char* dmimg_svg_strstr(const char* s, const char* what)
{
    size_t n = strlen(what);
    for (; *s != '\0' || n == 0; s++)
    {
        if (strncmp(s, what, n) == 0)
            return (char*)s;
    }
    return NULL;
}

/*
 * sscanf() of what nanosvg scans: text, white space (any amount of it,
 * none too) and the conversions %u, %d and %x with an optional width.
 */
int dmimg_svg_sscanf(const char* s, const char* format, ...)
{
    va_list args;
    int assigned = 0;
    va_start(args, format);
    while (*format != '\0')
    {
        if (is_space(*format))
        {
            while (is_space(*s))
                s++;
            format++;
            continue;
        }
        if (*format != '%' || format[1] == '%')
        {
            if (*format == '%')
                format++;
            if (*s != *format)
                break;
            s++;
            format++;
            continue;
        }
        format++;
        unsigned width = 0;
        while (*format >= '0' && *format <= '9')
            width = width * 10u + (unsigned)(*format++ - '0');
        char conversion = *format++;
        int base = (conversion == 'x' || conversion == 'X') ? 16 : (conversion == 'u' || conversion == 'd') ? 10 : 0;
        if (base == 0)
            break;
        while (is_space(*s))
            s++;
        bool negative = false;
        unsigned used = 0;
        if (conversion == 'd' && (*s == '-' || *s == '+'))
        {
            negative = *s++ == '-';
            used++;
        }
        unsigned value = 0, digits = 0;
        for (int d; (width == 0 || used < width) && (d = digit(*s)) >= 0 && d < base; s++, used++, digits++)
            value = value * (unsigned)base + (unsigned)d;
        if (digits == 0)
            break;
        if (conversion == 'd')
            *va_arg(args, int*) = negative ? -(int)value : (int)value;
        else
            *va_arg(args, unsigned*) = value;
        assigned++;
    }
    va_end(args);
    return assigned;
}

/* ---- Sorting ---- */

static void swap(uint8_t* a, uint8_t* b, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        uint8_t t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

/* Shell sort, on Ciura's gaps (then x 2.25) */
void dmimg_svg_qsort(void* base, size_t count, size_t size, int (*compare)(const void*, const void*))
{
    static const uint32_t gaps[] = { 1u, 4u, 10u, 23u, 57u, 132u, 301u, 701u, 1577u, 3548u, 7983u, 17961u,
                                     40412u, 90927u, 204585u, 460316u, 1035711u };
    uint8_t* items = base;
    int g = (int)(sizeof(gaps) / sizeof(gaps[0])) - 1;
    while (g > 0 && gaps[g] >= count)
        g--;
    for (; g >= 0; g--)
    {
        size_t gap = gaps[g];
        for (size_t i = gap; i < count; i++)
            for (size_t j = i; j >= gap && compare(items + (j - gap) * size, items + j * size) > 0; j -= gap)
                swap(items + (j - gap) * size, items + j * size, size);
    }
}

/* ---- Math ---- */

float dmimg_svg_fabsf(float x)
{
    return (x < 0.0f) ? -x : x;
}

double dmimg_svg_fabs(double x)
{
    return (x < 0.0) ? -x : x;
}

float dmimg_svg_floorf(float x)
{
    if (!(dmimg_svg_fabsf(x) < 8388608.0f))     /* 2^23: an integer already (or inf, NaN) */
        return x;
    float f = (float)(int32_t)x;
    return (f > x) ? f - 1.0f : f;
}

float dmimg_svg_ceilf(float x)
{
    if (!(dmimg_svg_fabsf(x) < 8388608.0f))
        return x;
    float f = (float)(int32_t)x;
    return (f < x) ? f + 1.0f : f;
}

float dmimg_svg_roundf(float x)
{
    return (x < 0.0f) ? -dmimg_svg_floorf(0.5f - x) : dmimg_svg_floorf(x + 0.5f);
}

float dmimg_svg_fmodf(float x, float y)
{
    if (y == 0.0f || !(dmimg_svg_fabsf(x) < 1e18f))
        return 0.0f;
    float q = x / y;
    if (!(dmimg_svg_fabsf(q) < 1e18f))
        return 0.0f;
    return x - (float)(int64_t)q * y;
}

float dmimg_svg_sqrtf(float x)
{
    if (!(x > 0.0f) || x > 3.0e38f)
        return (x > 0.0f) ? x : 0.0f;           /* 0, inf - a negative number has none: 0 */
    union { float f; uint32_t i; } u = { x };
    u.i = 0x1FBD1DF5u + (u.i >> 1);             /* About sqrt(x), to a few % */
    float y = u.f;
    for (int i = 0; i < 3; i++)
        y = 0.5f * (y + x / y);
    return y;
}

double dmimg_svg_sqrt(double x)
{
    if (!(x > 0.0) || x > 1.0e308)
        return (x > 0.0) ? x : 0.0;
    union { double f; uint64_t i; } u = { x };
    u.i = 0x1FF7A3BEA91D9B1Bull + (u.i >> 1);
    double y = u.f;
    for (int i = 0; i < 4; i++)
        y = 0.5 * (y + x / y);
    return y;
}

/* sin(x) for |x| <= pi / 2 */
static float sin_quadrant(float x)
{
    float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f +
           x2 * (1.0f / 362880.0f + x2 * (-1.0f / 39916800.0f))))));
}

float dmimg_svg_sinf(float x)
{
    if (!(dmimg_svg_fabsf(x) < 1e7f))
        return 0.0f;                            /* Beyond what a float tells apart (or inf, NaN) */
    float k = dmimg_svg_roundf(x * (1.0f / (2.0f * PI)));
    x = (x - k * TWO_PI_HI) - k * TWO_PI_LO;    /* -pi ... pi */
    if (x > HALF_PI)
        x = PI - x;
    else if (x < -HALF_PI)
        x = -PI - x;
    return sin_quadrant(x);
}

float dmimg_svg_cosf(float x)
{
    return dmimg_svg_sinf(dmimg_svg_fabsf(x) + HALF_PI);
}

float dmimg_svg_tanf(float x)
{
    float c = dmimg_svg_cosf(x);
    return (c != 0.0f) ? dmimg_svg_sinf(x) / c : 0.0f;
}

/* atan(t) for 0 <= t <= 1: past tan(pi / 12) by atan(t) = pi / 6 + atan((t sqrt(3) - 1) / (t + sqrt(3))) */
static float atan_unit(float t)
{
    bool shifted = t > TAN_PI_12;
    if (shifted)
        t = (t * SQRT_3 - 1.0f) / (t + SQRT_3);
    float t2 = t * t;
    float a = t * (1.0f + t2 * (-1.0f / 3.0f + t2 * (1.0f / 5.0f + t2 * (-1.0f / 7.0f + t2 * (1.0f / 9.0f + t2 * (-1.0f / 11.0f))))));
    return shifted ? a + PI / 6.0f : a;
}

float dmimg_svg_atan2f(float y, float x)
{
    float ax = dmimg_svg_fabsf(x), ay = dmimg_svg_fabsf(y);
    if (ax == 0.0f && ay == 0.0f)
        return (x < 0.0f) ? ((y < 0.0f) ? -PI : PI) : 0.0f;
    float a = (ax >= ay) ? atan_unit(ay / ax) : HALF_PI - atan_unit(ax / ay);
    if (x < 0.0f)
        a = PI - a;
    return (y < 0.0f) ? -a : a;
}

float dmimg_svg_acosf(float x)
{
    if (x > 1.0f)
        x = 1.0f;
    if (x < -1.0f)
        x = -1.0f;
    return dmimg_svg_atan2f(dmimg_svg_sqrtf((1.0f - x) * (1.0f + x)), x);
}

/* x^y for an integer y - nanosvg raises 10 to the digits of a number only; y is rounded */
double dmimg_svg_pow(double x, double y)
{
    bool inverse = y < 0.0;
    if (inverse)
        y = -y;
    if (y > 2048.0)
        y = 2048.0;
    uint32_t n = (uint32_t)(y + 0.5);
    double result = 1.0;
    for (; n != 0; n >>= 1, x *= x)
    {
        if ((n & 1u) != 0)
            result *= x;
    }
    return inverse ? 1.0 / result : result;
}
