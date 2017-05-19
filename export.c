#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "error.h"
#include "export.h"

void export_init(void)
{
    int ret;

    socket_out = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_out == -1)
    {
        error("Socket not created");
    }

    bzero(sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(COLLECTOR_PORT);
    ret = inet_aton(COLLECTOR_IP_STR, &sin.sin_addr);
    if (ret == 0)
    {
        error("inet_aton");
    }
}

void export_finish(void)
{
    close(socket_out);
}

/** Add a new nat record to the buffer.
 */
void export_append(struct nat_record *natr)
{
    /* TODO exclusive access. */
    buf_records[buf_end] = natr;
    buf_end++;
}
