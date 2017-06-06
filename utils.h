/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                utils.h                      */
/***********************************************/

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>

extern const int is_debug;
#define DEBUG(msg) \
    if (is_debug) fprintf(stderr, "[%s] %s\n", __func__, msg);

uint32_t get_uptime_ms(void);
uint32_t get_timestamp_s(void);
uint64_t get_timestamp_ms(void);
int is_protocol_portless(uint8_t proto);

#endif /* UTILS_H */
