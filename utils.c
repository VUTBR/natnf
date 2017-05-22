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

uint32_t get_timestamp_s(void)
{
    struct timeval tv;
    int ret;

    ret = gettimeofday(&tv, NULL);
    if (ret == -1)
        perror("gettimeofday");
    return tv.tv_sec;
}
