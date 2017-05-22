/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                config.h                     */
/***********************************************/

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <string.h>
#include "export.h"

#define FILENAME "config.conf"
#define MAXBUF 1024
#define DELIM "="
#define REMARK "#"

#define COLLECTOR_IP "COLLECTOR_IP"
#define COLLECTOR_PORT "COLLECTOR_PORT"
#define TEMPLATE_TIMEOUT "TEMPLATE_TIMEOUT"

char * trimwhitespace(char *);
int isValidIpAddress(char *);
void load_config_file();

#endif /* CONFIG_H */
