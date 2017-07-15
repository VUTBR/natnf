/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                config.c                     */
/***********************************************/

#include "config.h"
#include "utils.h"

void checkAndSetCollectorPort(char *port)
{
    int i = atoi(port);
    if ( i < 65536 && i > 0 ) {
        exs.port = (char *)malloc(sizeof(char) * strlen(port));
        strncpy(exs.port, port, strlen(port));
    }
    else {
        fprintf(stderr, "config - Collector port is invalid\n");
		exit(1);
    }
}

void checkAndSetSyslogLevel(int level)
{
    if ( level < 5 && level > 0 )   //could it be checked to msgs.h defines
    {
        exs.syslog_level = level;
    }
    else
    {
        fprintf(stderr,"Config - Syslog level invalid\n");
    }
}

void checkAndSetTemplateTimeout(int timeout)
{
    if ( timeout > 0 )
    {
        exs.template_timeout = timeout;
    }
    else
    {
        fprintf(stderr,"Config - Template timeout invalid\n");
    }
}

void checkAndSetExportTimeout(int timeout)
{
    if ( timeout > 0 )
    {
        exs.export_timeout = timeout;
    }
    else
    {
        fprintf(stderr,"Config - Export timeout invalid\n");
    }
}

void printHelpMessage()
{
    printf("Usage: natnf -c <collector-ip-address>\
 [ -p <collector-port> ] [ -h ] [ -s ] [ -l <syslog-level> ]\
 [ -t <template-timeout> ] [ -e <export-timeout> ] [ -F ]\n\
\t-c\tcollector IPv4/IPv6 address or DNS name\n\
\t-p\tcollector port (default value: %s)\n\
\t-h\thelp\n\
\t-s\tenable syslog logging\n\
\t-l\tsyslog level\n\
\t-t\ttemplate timeout [seconds] (default value: %d)\n\
\t-e\texport timeout [seconds] (default value: %d)\n\
\t-F\tdaemonize\n\
", COLLECTOR_PORT, TIMEOUT_TEMPLATE_DEFAULT, TIMEOUT_EXPORT_DEFAULT);
}

void load_config(int argc, char **argv)
{
    int opt, i;

    extern char *optarg;

	if (argc < 2) {
		fprintf(stderr, "Please specify collector IP address and port. See -h or manpage for help\n");
        exit(0);
	}

    while ((opt = getopt(argc, argv, "hc:p:sl:t:e:F")) != -1)
    {
        switch (opt)
        {
        case 'h':       //display help
            printHelpMessage();
            exit(0);
            break;
        case 'c':       //set collector IP address, IP address check is performed in getaddrinfo() function
            if (!optarg) {break;}
        	exs.ip_str = (char *)malloc(sizeof(char) * strlen(optarg));
        	strncpy(exs.ip_str, optarg, strlen(optarg));
            break;
        case 'p':       //set collector port
            if (!optarg) {break;}
            checkAndSetCollectorPort(optarg);
            break;
        case 's':       //enable syslog logging
            exs.syslog_enable = 1;
            break;
        case 'l':       //set syslog level
            if (!optarg) {break;}
            i = atoi(optarg);
            checkAndSetSyslogLevel(i);
            break;
        case 't':       //set template timeout
            if (!optarg) {break;}
            i = atoi(optarg);
            checkAndSetTemplateTimeout(i);
            break;
        case 'e':       //set export timeout
            if (!optarg) {break;}
            i = atoi(optarg);
            checkAndSetExportTimeout(i);
            break;
        case 'F':       //daemonize
            exs.daemonize = 1;
            break;
        default:        //any other param is wrong
            error("Wrong parameter - use: [ -h ]\
 [ -c <collector-ip-address> ] [ -p <collector-port> ] [ -s ]\
 [ -l <syslog-level> ] [ -t <template-timeout> ] [ -e <export-timeout> ] [ -F ]");
        }
    }

    char tmp[80];
    
    sprintf(tmp,"Collector:\t%s:%s",exs.ip_str,exs.port);
    DEBUG(tmp);
    if ( exs.syslog_enable )
    {
        sprintf(tmp,"Syslog level:\t%d",exs.syslog_level);
        DEBUG(tmp);
    }
    sprintf(tmp,"Template timeout:\t%d",exs.template_timeout);
    DEBUG(tmp);
    sprintf(tmp,"Export timeout:\t%d",exs.export_timeout);
    DEBUG(tmp);
}
