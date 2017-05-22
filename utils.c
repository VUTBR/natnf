#include <sys/sysinfo.h>
#include <sys/time.h>
#include <stdint.h>
#include "utils.h"

const int is_debug = 1;

uint32_t get_uptime_ms(void)
{
    struct sysinfo info;
    int ret;

    ret = sysinfo(&info);
    if (ret == -1)
        perror("sysinfo");
    return info.uptime * 1000;
}

uint32_t get_timestamp_ms(void)
{
    struct timeval tv;
    int ret;

    ret = gettimeofday(&tv, NULL);
    if (ret == -1)
        perror("gettimeofday");
    return tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}
