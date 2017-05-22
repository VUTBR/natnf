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

void load_config_file()
{
    FILE *file = fopen (FILENAME, "r");

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
                if ( isValidIpAddress(cfline) )
                {
                    //reallocate ip_str to appropriate size
                    exs.ip_str = (char *)malloc(sizeof(char) * strlen(cfline));
                    strncpy(exs.ip_str, cfline, strlen(cfline));
                }
                else
                {
                    fprintf(stderr,"Config - IP address invalid\n");
                }
                
            }
            else if ( !strcmp(var, COLLECTOR_PORT) )
            {
                i = atoi(cfline);
                if ( i < 65536 && i > 0 )
                {
                    exs.port = i;
                }
                else
                {
                    fprintf(stderr,"Config - Port invalid\n");
                }
                
            }
            else if ( !strcmp(var, TEMPLATE_TIMEOUT) )
            {
                i = atoi(cfline);
                if ( i > 0 )
                {
                    exs.template_timeout = i;
                }
                else
                {
                    fprintf(stderr,"Config - Timeout invalid\n");
                }
            }
        }

        fclose(file);
    }
}