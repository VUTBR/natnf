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
    printf("Usage: natnf -c <collector-address> [-phsltef]\n" \
           "\t-c\tcollector IPv4/IPv6 address or DNS name\n " \
           "\t-p\tcollector port (default value: %s)\n" \
           "\t-h\thelp\n" \
           "\t-s\tenable syslog logging\n" \
           "\t-l\tsyslog level\n" \
           "\t-t\ttemplate timeout [seconds] (default value: %d)\n" \
           "\t-e\texport timeout [seconds] (default value: %d)\n" \
           "\t-d\tlog path (default value: %s)\n" \
           "\t-f\tdaemonize\n", COLLECTOR_PORT, TIMEOUT_TEMPLATE_DEFAULT, TIMEOUT_EXPORT_DEFAULT, LOG_PATH_DEFAULT);
}

void load_config(int argc, char **argv)
{
    int opt, i;

    extern char *optarg;
	
	char *pch;
	char command[512];

	if (argc < 2) {
		fprintf(stderr, "Please specify at least collector IP address. See -h or manpage for help\n");
        exit(0);
	}
	
	exs.ip_str = NULL;

    while ((opt = getopt(argc, argv, "hc:p:sl:t:e:d:fv")) != -1)
    {
        switch (opt)
        {
        case 'h':       //display help
            printHelpMessage();
            exit(0);
            break;
        case 'c':       //set collector IP address, IP address check is performed in getaddrinfo() function
        	exs.ip_str = optarg;
        	//exs.ip_str = (char *)malloc(sizeof(char) * strlen(optarg));
        	//strncpy(exs.ip_str, optarg, strlen(optarg));
            break;
        case 'p':       //set collector port
            checkAndSetCollectorPort(optarg);
            break;
        case 's':       //enable syslog logging
            exs.syslog_enable = 1;
            break;
        case 'l':       //set syslog level
            i = atoi(optarg);
            checkAndSetSyslogLevel(i);
            break;
        case 't':       //set template timeout
            i = atoi(optarg);
            checkAndSetTemplateTimeout(i);
            break;
        case 'e':       //set export timeout
            i = atoi(optarg);
            checkAndSetExportTimeout(i);
            break;
        case 'd':       //path to logfile
            pch = strrchr(optarg,'/');
            strcpy( command, "mkdir -p " );
            strncat( command, optarg, pch-optarg );
            system(command);
            logs.log_path = optarg;
            break;
        case 'f':       //daemonize
            exs.daemonize = 1;
            break;
        case 'v':       //verbose mode
            exs.verbose = 1;
            break;
        default:        //any other param is wrong
            error("Wrong parameter. See -h for help");
        }
    }

    if (exs.ip_str == NULL) {
        fprintf(stderr, "Please specify at least collector IP address. See -h or manpage for help\n");
        exit(0);
    }

    if (exs.verbose) { 
    	fprintf(stderr, "Collector: %s:%s\n" \
                        "Syslog is %s, syslog level: %d\n"
                        "Template timeout: %d\n"
                        "Export timeout: %d\n"
                        "Logpath: %s\n\n", 
                        exs.ip_str,exs.port, 
                        exs.syslog_enable ? "ENABLED" : "DISABLED", exs.syslog_level, 
                        exs.template_timeout, 
                        exs.export_timeout,
                        logs.log_path);
	}
}
