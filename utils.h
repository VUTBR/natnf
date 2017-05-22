#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>

extern const int is_debug;
#define DEBUG(msg) \
    if (is_debug) fprintf(stderr, "[%s] %s\n", __func__, msg);

uint32_t get_uptime_ms(void);
uint32_t get_timestamp_ms(void);

#endif /* UTILS_H */
