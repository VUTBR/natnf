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
#include <stdlib.h>
#include <unistd.h>
#include "export.h"

#define FILENAME "config.conf"
#define MAXBUF 1024
#define DELIM "="
#define REMARK "#"

#define COLLECTOR_IP "COLLECTOR_IP"
#define COLLECTOR_PORT "COLLECTOR_PORT"
#define TEMPLATE_TIMEOUT "TEMPLATE_TIMEOUT"

extern struct export_settings exs;

char * trimwhitespace(char *);
int isValidIpAddress(char *);
void checkAndSetCollectorIpAddress(char *);
void checkAndSetCollectorPort(int);
void checkAndSetTemplateTimeout(int);
void printHelpMessage();
void load_config(int, char **);
void load_config_file(char *);

#endif /* CONFIG_H */
