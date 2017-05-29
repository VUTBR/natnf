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
# Help for natnf project\n\
# Authors\n\
#     Jakub Mackovič - xmacko00\n\
#     Jakub Pastuszek - xpastu00\n\
# VUT FIT Brno\n\
# Export informací o překladu adres\n\
###################################\n\
");
}

void load_config(int argc, char **argv)
{
    int opt, i;
    int isFileConfig = 0;

    extern char *optarg;

    while ((opt = getopt(argc, argv, "hf::a:p:t:")) != -1)
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
        case 'a':       //set collector IP address
            if (!optarg) {break;}
            if (!isFileConfig) {checkAndSetCollectorIpAddress(optarg);}
            break;
        case 'p':       //set collector port
            if (!optarg) {break;}
            i = atoi(optarg);
            if (!isFileConfig) {checkAndSetCollectorPort(i);}
            break;
        case 't':       //set template timeout
            if (!optarg) {break;}
            i = atoi(optarg);
            if (!isFileConfig) {checkAndSetTemplateTimeout(i);}
            break;
        default:        //any other param is wrong
            fprintf(stderr, "Wrong parameter - use: [-h] [-f [config-file-name]] [-a collector-ip-address]\
 [-p collector-port] [-t template-timeout]\n");
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
                
            }
            else if ( !strcmp(var, TEMPLATE_TIMEOUT) )
            {
                i = atoi(cfline);

            }
        }

        fclose(file);
    }
}