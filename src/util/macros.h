#ifndef UTIL_MACROS_H
#define UTIL_MACROS_H

#ifndef MIN
#  define MIN(x, y) ((x) < (y) ? (x) : (y))
#  define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define countof(ar) (sizeof(ar) / sizeof((ar)[0]))

#endif
