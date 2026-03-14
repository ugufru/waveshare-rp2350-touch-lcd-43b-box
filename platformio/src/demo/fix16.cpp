// fix16.cpp — Sin/cos LUT (single shared instance across all translation units)

#include "fix16.h"

fix16_t sin_lut[256];

void build_sin_lut(void) {
    for (int i = 0; i < 256; i++) {
        sin_lut[i] = (fix16_t)(sinf((float)i * 6.2831853f / 256.0f) * 65536.0f);
    }
}
