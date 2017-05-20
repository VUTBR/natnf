#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "error.h"
#include "export.h"
#include "utils.h"

int socket_out;
struct sockaddr_in sin;

struct nat_record *buf_records[RECORDS_MAX] = { NULL };
int buf_begin = 0;
int buf_end = 0;

sem_t cnt_buf_empty;
sem_t cnt_buf_taken;

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

    sem_init(&cnt_buf_empty, 0, RECORDS_MAX);
    sem_init(&cnt_buf_taken, 0, 0);
}

void export_finish(void)
{
    close(socket_out);
    sem_destroy(&cnt_buf_empty);
    sem_destroy(&cnt_buf_taken);
}

/** Add a new nat record to the buffer.
 */
void export_append(struct nat_record *natr)
{
    DEBUG("sem_wait() call.\n");
    sem_wait(&cnt_buf_empty);
    DEBUG("Enter critical section.\n");
    buf_records[buf_end] = natr;
    buf_end++;
    sem_post(&cnt_buf_taken);
    DEBUG("Leave critical section.\n");
}

struct nat_record *nat_record_new(void)
{
    struct nat_record *new;
    new = malloc(sizeof(struct nat_record));
    if (new == NULL)
    {
        perror("malloc returned NULL");
        /* Continue even if null. */
    }
    return new;
}
