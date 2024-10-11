// simple debug macros
#pragma once

#define DEBUG 0

#if DEBUG
#define debug(...) \
    do { \
        __VA_ARGS__; \
    } while (0)
#else
#define debug(...) \
    do { \
    } while (0)
#endif