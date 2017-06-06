/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                 msgs.h                      */
/***********************************************/

#ifndef MSGS_H
#define MSGS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#define MSG_ERROR 1
#define MSG_WARNING 2
#define MSG_DEBUG 3
#define MSG_INFO 4

#define MAX_STRING 1024
#define LOG_NAME "natnf"

void msg_init(int debug);
void msg(char* msg, ...);
void msg_term(void);

#endif /* MSGS_H */
