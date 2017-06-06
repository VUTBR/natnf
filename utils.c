/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                utils.c                      */
/***********************************************/

#include <sys/sysinfo.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
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

uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    uint64_t ms;

    clock_gettime(0, &ts);
    ms = ts.tv_sec * 1000 + ts.tv_nsec / 1.0e6;
    return ms;
}
