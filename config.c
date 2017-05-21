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

void get_config(struct export_settings *exs)
{
    FILE *file = fopen (FILENAME, "r");

    if (file != NULL)
    {
        char line[MAXBUF];

        while(fgets(line, sizeof(line), file) != NULL)
        {
            char *cfline;
            char var[MAXBUF];
            int i=12;

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

            if ( !strcmp(var, COLLECTOR_IP) )
            {
                //reallocate ip_str to appropriate size
                exs->ip_str = (char *)malloc(sizeof(char) * strlen(cfline));
                strncpy(exs->ip_str, cfline, strlen(cfline));
            }
            else if ( !strcmp(var, COLLECTOR_PORT) )
            {
                exs->port = atoi(cfline);
            }
            else if ( !strcmp(var, TEMPLATE_TIMEOUT) )
            {
                exs->template_timeout = atoi(cfline);
            }
        }

        fclose(file);
    }
}