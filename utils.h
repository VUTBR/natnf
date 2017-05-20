#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

extern const int is_debug;
#define DEBUG(msg) \
    if (is_debug) fprintf(stderr, "[%s] %s\n", __func__, msg);

#endif /* UTILS_H */
