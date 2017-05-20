#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "error.h"
#include "export.h"
#include "utils.h"

int socket_out;
struct sockaddr_in dest;

struct nat_record *buf_records[RECORDS_MAX] = { NULL };
int buf_begin = 0;
int buf_end = 0;

sem_t cnt_buf_empty;
sem_t cnt_buf_taken;

/** A buffer with pre-calculated data fields for flow export.
 */
static char flowbuf[1500];

/** A buffer with pre-calculated data fields for template export.
 */
static char templbuf[1500];

void export_init(void)
{
    int ret;

    socket_out = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_out == -1)
    {
        error("Socket not created");
    }

    bzero(dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(COLLECTOR_PORT);
    ret = inet_aton(COLLECTOR_IP_STR, &dest.sin_addr);
    if (ret == 0)
    {
        error("inet_aton");
    }

    export_init_flow();
    export_init_template();

    sem_init(&cnt_buf_empty, 0, RECORDS_MAX);
    sem_init(&cnt_buf_taken, 0, 0);
}

/** Initialize the flow buffer.
 * TODO
 */
void export_init_flow(void)
{

}

/** Initialize the template buffer.
 * TODO
 */
void export_init_template(void)
{

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
    DEBUG("sem_wait() call.");
    sem_wait(&cnt_buf_empty);
    DEBUG("Enter critical section.");
    buf_records[buf_end] = natr;
    buf_end++;
    sem_post(&cnt_buf_taken);
    DEBUG("Leave critical section.");
}

void export_send_record(struct nat_record *natr)
{
    size_t len;
    int flags;
    socklen_t addrlen;

    //sendto(socket_out, flowbuf, len, flags, (struct sockaddr *)&dest, addrlen);
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

/** Duplicate an existing nat record.
 */
struct nat_record *nat_record_dup(struct nat_record *old)
{
    if (old == NULL)
        return NULL;
    struct nat_record *new = nat_record_new();
    memcpy(new, old, sizeof(struct nat_record));
    return new;
}
