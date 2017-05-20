#ifndef UTILS_H
#define UTILS_H

extern const int is_debug;
#define DEBUG(msg) \
    if (is_debug) printf("[%s] %s", __func__, msg);

#endif /* UTILS_H */
