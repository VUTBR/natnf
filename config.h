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

#define MAXBUF 1024

extern struct export_settings exs;
extern struct log_settings logs;

int isValidIpAddress(char *);
void checkAndSetCollectorIpAddress(char *);
void checkAndSetCollectorPort(char *);
void checkAndSetSyslogLevel(int);
void checkAndSetTemplateTimeout(int);
void checkAndSetExportTimeout(int);
void printHelpMessage();
void load_config(int, char **);

#endif /* CONFIG_H */
