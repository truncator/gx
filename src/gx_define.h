#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h> // raise()
#include <float.h> // FLT_*

#define PI_OVER_TWO 1.57079632679489661923f
#define PI          3.14159265358979323846f
#define TWO_PI      6.28318530717958647692f

#define FLOAT_EPSILON FLT_EPSILON
#define FLOAT_MAX     FLT_MAX

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define ASSERT(condition) \
    if (!(condition)) \
    { \
        fprintf(stderr, "\n\n[ASSERT] Assertion failed:\n   >>   %s   <<\n    %s, line %u\n\n", #condition, __FILE__, __LINE__); \
        raise(SIGTRAP); \
    }
#define ASSERT_NULL(pointer)     ASSERT(pointer == NULL)
#define ASSERT_NOT_NULL(pointer) ASSERT(pointer != NULL)

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define KILOBYTES(n) ((n) * 1024)
#define MEGABYTES(n) (KILOBYTES(n) * 1024)
