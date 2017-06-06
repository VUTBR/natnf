/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                 msgs.c                      */
/***********************************************/

#include "msgs.h"
#include "export.h"

int log_debug = 0;

void msg_init(int debug)
{
	log_debug = debug;

	if (exs.daemonize)
	{
		openlog(LOG_NAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
	}
	else
	{
		openlog(LOG_NAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
	}
}


void msg(char* msg, ...)
{
	va_list arg;
	int level;
	char buf[MAX_STRING];

	switch (exs.syslog_level)
	{
		case MSG_ERROR: 	level = LOG_ERR; break;
		case MSG_WARNING: 	level = LOG_WARNING; break;
		case MSG_DEBUG:		level = LOG_DEBUG; break; 
		default:			level = LOG_INFO; break;
	}

	va_start(arg, msg);
	vsnprintf(buf, MAX_STRING - 1, msg, arg);
	va_end(arg);

	syslog(level, "%s", buf);

	if (log_debug)
	{
		printf("%s\n", buf);
	}
}

void msg_term(void)
{
	closelog();
}
