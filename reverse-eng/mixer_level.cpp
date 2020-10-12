#include <stdio.h>
#include <stdint.h>
#include <math.h>

typedef int32_t s32;
typedef uint32_t u32;

/* Decode floating-point value into valid scarlett gain
 * The input floating-point value may be any (including NaNs)
 * The output integer value is in range of -160 to 12 (dB with 0.5 step)
 */
static int scarlett2_float_to_mixer_level(u32 v)
{
    u32 exp, frac;
    int sign, res;

    exp  = (v >> 23) & 0xff;
    if (exp < 0x7e) /* abs(v) < 0.5f */
        return 0;

    sign = (v >> 31);
    if (exp > 0x85) /* abs(v) > 80.0f ? */
        return (sign) ? -160 : 12;

    /* Compute the fraction part */
    frac = (v & 0x007fffffu) | 0x00800000u; /* 24 bits normalized */
    frac = frac >> (0x95 - exp); /* 0x7f - exp + 22 */
    res = (sign) ? -frac : frac;

    /* Limit the value and return */
    if (res < -160)
        return -160;

    return (res < 12) ? res : 12;
}

static const float special[] =
{
    0.5f,
    -0.5f,
    3.0f,
    -3.0f,
    6.0f,
    -6.0f,
    -80.0f,
    10000.0f,
    -10000.0f,
    1e-37f,
    NAN,
    -NAN,
    INFINITY,
    -INFINITY
};

int main(int argc, const char **argv)
{
    union
    {
        float f32;
        uint32_t u32;
    } v;

    for (size_t i=0; i<sizeof(special)/sizeof(float); ++i)
    {
        v.f32 = special[i];
        printf ("%.2f -> %d\n", v.f32, scarlett2_float_to_mixer_level(v.u32));
    }

    for (int i=-128*4; i<=24*4; ++i)
    {
        v.f32 = i * 0.25f;
        printf ("%.2f -> %d\n", v.f32, scarlett2_float_to_mixer_level(v.u32));
    }
}
