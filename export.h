#include <netinet/ip.h>
#include <stdlib.h>

/* TODO Replace the following with variable user input. */
#define COLLECTOR_IP_STR "127.0.0.1"
#define COLLECTOR_PORT 3001

#define RECORDS_MAX 65536

/* Outgoing connection identification */
int socket_out;
struct sockaddr_in sin;

/* TODO Add fields that identify a NAT translation. */
struct nat_record
{
    int todo;
};

/** Circular buffer of nat record pointers.
 * TODO Access to the following variables should be exclusive (threads).
 */
struct nat_record *buf_records[RECORDS_MAX] = { NULL };
int buf_begin = 0;
int buf_end = 0;

void export_init(void);
void export_finish(void);
void export_append(struct nat_record *natr);
