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
void get_config(struct export_settings *);

#endif /* CONFIG_H */
