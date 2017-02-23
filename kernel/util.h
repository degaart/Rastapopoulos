#pragma once

#include <stdint.h>

#define LOBYTE(i) ((i) & 0xFF)
#define HIBYTE(i) (((i) & 0xFF00) >> 8)

#define LOWORD(i)   ((i) & 0xFFFF)
#define HIWORD(i)   (((i) & 0xFFFF0000) >> 16)

#define LODWORD(i)  ((uint32_t) ((i) & 0xFFFFFFFF))
#define HIDWORD(i)  ((uint32_t) (((i) & 0xFFFFFFFF00000000) >> 32))

#define MAKE_UINT64(lo, hi)  ( ((uint64_t)(lo)) | ((uint64_t)(hi) << 32) )

#define countof(a) sizeof(a) / sizeof(a[0])

#define ALIGN(val, align) \
    (((val) + ((align) - 1)) & ~((align) - 1))

#define TRUNCATE(val, align) \
    (((val) / (align)) * (align))

#define IS_ALIGNED(val, align) \
    (((val) % (align)) == 0)



