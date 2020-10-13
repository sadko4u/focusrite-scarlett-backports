#include <stdio.h>
#include <stdint.h>

int main(int argc, const char *argv)
{
    union
    {
        float f32;
        uint32_t u32;
    } v;

    for (ssize_t i=0; i<173; ++i)
    {
        v.f32 = (i == 0) ? -128.0f : (i - 160) * 0.5f;
        printf("0x%04x, ", (v.u32 >> 16));
        if ((i & 0x07) == 0x07)
            printf("\n");
    }
}


