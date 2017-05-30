/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                config.c                     */
/***********************************************/

#include "config.h"

char * trimwhitespace(char *str)
{
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end+1) = 0;

    return str;
}

int isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

void checkAndSetCollectorIpAddress(char *address)
{
    if ( isValidIpAddress(address) )
    {
        //reallocate ip_str to appropriate size
        exs.ip_str = (char *)malloc(sizeof(char) * strlen(address));
        strncpy(exs.ip_str, address, strlen(address));
    }
    else
    {
        fprintf(stderr,"Config - Collector IP address invalid\n");
    }
}

void checkAndSetCollectorPort(int port)
{
    if ( port < 65536 && port > 0 )
    {
        exs.port = port;
    }
    else
    {
        fprintf(stderr,"Config - Collector port invalid\n");
    }
}

void checkAndSetSyslogIpAddress(char *address)
{
    if ( isValidIpAddress(address) )
    {
        //reallocate ip_str to appropriate size
        exs.syslog_ip_str = (char *)malloc(sizeof(char) * strlen(address));
        strncpy(exs.syslog_ip_str, address, strlen(address));
    }
    else
    {
        fprintf(stderr,"Config - Syslog IP address invalid\n");
    }
}

void checkAndSetSyslogPort(int port)
{
    if ( port < 65536 && port > 0 )
    {
        exs.syslog_port = port;
    }
    else
    {
        fprintf(stderr,"Config - Syslog port invalid\n");
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

void printHelpMessage()
{
    printf("\
######################################\n\
# Help for natnf project\n\
# Authors\n\
#     Jakub Mackovič - xmacko00\n\
#     Jakub Pastuszek - xpastu00\n\
# VUT FIT Brno\n\
# Export informací o překladu adres\n\
######################################\n\
Usage: natnf [ -h ] [ -f [<config-file-name>] ] [ -c <collector-ip-address> ]\
 [ -p <collector-port> ] [ -s <syslog-ip-address> ] [ -r <syslog-port> ]\
 [ -l <syslog-level> ] [ -t <template-timeout> ] [ -F ]\n\
\t-h\thelp\n\
\t-f\tconfig file name (default: config.conf)\n\
\t\t(if -f config is loaded from file)\n\
\t-c\tcollector IP address\n\
\t-p\tcollector port\n\
\t-s\tsyslog IP address\n\
\t-r\tsyslog port\n\
\t-l\tsyslog level\n\
\t-t\ttemplate timeout\n\
\t-F\tdaemonize\n\
######################################\n\
");
}

void load_config(int argc, char **argv)
{
    int opt, i;
    int isFileConfig = 0;

    extern char *optarg;

    while ((opt = getopt(argc, argv, "hf::c:p:s:r:l:t:F")) != -1)
    {
        switch (opt)
        {
        case 'h':       //display help
            printHelpMessage();
            exit(0);
            break;
        case 'f':       //load config from file
            if (!optarg) {load_config_file(NULL);break;}
            else {load_config_file(optarg);}
            isFileConfig = 1;
            break;
        case 'c':       //set collector IP address
            if (!optarg) {break;}
            if (!isFileConfig) {checkAndSetCollectorIpAddress(optarg);}
            break;
        case 'p':       //set collector port
            if (!optarg) {break;}
            i = atoi(optarg);
            if (!isFileConfig) {checkAndSetCollectorPort(i);}
            break;
        case 's':       //set syslog IP address
            if (!optarg) {break;}
            if (!isFileConfig) {checkAndSetSyslogIpAddress(optarg);}
            break;
        case 'r':       //set syslog port
            if (!optarg) {break;}
            i = atoi(optarg);
            if (!isFileConfig) {checkAndSetSyslogPort(i);}
            break;
        case 'l':       //set syslog level
            if (!optarg) {break;}
            i = atoi(optarg);
            if (!isFileConfig) {checkAndSetSyslogLevel(i);}
            break;
        case 't':       //set template timeout
            if (!optarg) {break;}
            i = atoi(optarg);
            if (!isFileConfig) {checkAndSetTemplateTimeout(i);}
            break;
        case 'F':       //daemonize
            exs.daemonize = 1;
            break;
        default:        //any other param is wrong
            fprintf(stderr, "Wrong parameter - use: [ -h ] [ -f [<config-file-name>] ]\
 [ -c <collector-ip-address> ] [ -p <collector-port> ] [ -s <syslog-ip-address> ] [ -r <syslog-port> ]\
 [ -l <syslog-level> ] [ -t <template-timeout> ] [ -F ]\n");
            exit(1);
        }
    }
}

void load_config_file(char *filename)
{
    FILE *file;

    if ( NULL == filename)
    {
        file = fopen (FILENAME, "r");
    }
    else
    {
        file = fopen (filename, "r");
    }

    if (file != NULL)
    {
        char line[MAXBUF];

        while(fgets(line, sizeof(line), file) != NULL)
        {
            char *cfline;
            char *value;
            char var[MAXBUF];
            int i;

            i = strcspn(line,REMARK);

            //Skip remark and empty lines
            if ( i <= 1 )
            {
                continue;
            }
            
            i = strcspn((char *)line,DELIM);

            //get part after DELIM (with DELLIM)
            cfline = strstr((char *)line,DELIM);
            cfline = cfline + strlen(DELIM);

            memcpy( var, line, i );
            var[i] = '\0';

            trimwhitespace(var);
            cfline = trimwhitespace(cfline);

            value = strtok(cfline,REMARK);
            if ( NULL != value )
            {
                cfline = value;
            }

            if ( !strcmp(var, COLLECTOR_IP) )
            {
                checkAndSetCollectorIpAddress(cfline);
            }
            else if ( !strcmp(var, COLLECTOR_PORT) )
            {
                i = atoi(cfline);
                checkAndSetCollectorPort(i);
            }
            else if ( !strcmp(var, SYSLOG_IP) )
            {
                checkAndSetSyslogIpAddress(cfline);
            }
            else if ( !strcmp(var, SYSLOG_PORT) )
            {
                i = atoi(cfline);
                checkAndSetSyslogPort(i);
            }
            else if ( !strcmp(var, SYSLOG_LEVEL) )
            {
                i = atoi(cfline);
                checkAndSetSyslogLevel(i);
            }
            else if ( !strcmp(var, TEMPLATE_TIMEOUT) )
            {
                i = atoi(cfline);
                checkAndSetTemplateTimeout(i);
            }
        }

        fclose(file);
    }
    else
    {
        fprintf(stderr, "Config - File does not exist");
    }
}

int daemonize(void)
{
    /* Our process ID and Session ID */
    pid_t pid, sid;
    
    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
            return 0;
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) { // Child can continue to run even after the parent has finished executing
            exit(0);
    }

    /* Change the file mode mask */
    umask(0);
            
    /* Open any logs here */        
            
    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
            /* Log the failure */
            return 0;
    }
           
    /* Change the current working directory */
//        if ((chdir("/")) < 0) {
            /* Log the failure */
//               return 0;
//      }
    
    /* Close out the standard file descriptors */
    //Because daemons generally dont interact directly with user so there is no need of keeping these open
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    /* Daemon-specific initialization goes here */
   
    return 1; 
    /* An infinite loop */
}
