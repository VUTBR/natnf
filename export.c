#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/sysinfo.h>
#include <sys/time.h>

#include "error.h"
#include "export.h"
#include "utils.h"

int template_full_fields[][2] =
{
    {TL_SRC_IP, 4},
    {TL_DST_IP, 4},
    {TL_SRC_PORT, 2},
    {TL_DST_PORT, 2},
    {TL_POST_NAT_SRC_IP, 4},
    {TL_POST_NAT_DST_IP, 4},
    {TL_POST_NAT_SRC_PORT, 2},
    {TL_POST_NAT_DST_PORT, 2},
    {TL_NAT_EVENT, 1},
    {TL_OBSERVATION_TIME_MS, 8} /* 8 because of double */
};
int template_no_ports_fields[][2] =
{
    {TL_SRC_IP, 4},
    {TL_DST_IP, 4},
    {TL_POST_NAT_SRC_IP, 4},
    {TL_POST_NAT_DST_IP, 4},
    {TL_NAT_EVENT, 1},
    {TL_OBSERVATION_TIME_MS, 8}
};

/** Template structures.
 * For the detailed explanation of the fields, see:
 * http://www.cisco.com/en/US/technologies/tk648/tk362/technologies_white_paper09186a00800a3db9.html
 */
/* A full template, including port numbers. */
struct template_full
{
    uint16_t template_id;
    uint16_t field_count;
    struct type_length tl[N_FIELDS_FULL];
};
/* A template without port numbers, in case they can not be extracted. */
struct template_no_ports
{
    uint16_t template_id;
    uint16_t field_count;
    struct type_length tl[N_FIELDS_NO_PORTS];
};
/* The complete template packet. */
struct template_packet
{
    uint16_t version;
    uint16_t count;
    uint32_t sys_uptime;
    uint32_t timestamp;
    uint32_t seq_number;
    uint32_t source_id;
    /* Flow set: */
    uint16_t flowset_id;
    uint16_t flowset_len;
    /* Templates: */
    struct template_full t1;
    struct template_no_ports t2;
};

struct export_settings exs;

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
static struct template_packet template;

/** Initialize the settings structure.
 * TODO: This is the best place for reading settings from file.
 */
void export_init_settings(void)
{
    int ret;

    exs.ip_str = COLLECTOR_IP_STR;
    exs.port = COLLECTOR_PORT;
    exs.template_timeout = _TEMPLATE_TIMEOUT;
    exs.socket_out = socket(AF_INET, SOCK_DGRAM, 0);
    if (exs.socket_out == -1)
    {
        error("Socket not created");
    }
    bzero(exs.dest, sizeof(exs.dest));
    exs.dest.sin_family = AF_INET;
    exs.dest.sin_port = htons(exs.port);
    ret = inet_aton(exs.ip_str, &exs.dest.sin_addr);
    if (ret == 0)
    {
        error("inet_aton");
    }
}

void export_init(void)
{
    export_init_settings();
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
    struct sysinfo info;
    struct timeval tv;
    int ret, i;

    ret = sysinfo(&info);
    if (ret == -1)
        perror("sysinfo");
    ret = gettimeofday(&tv, NULL);
    if (ret == -1)
        perror("gettimeofday");
    bzero(template, sizeof(template));

    template.version = 9;
    template.count = 2; /* 2 templates in a flow. */
    template.sys_uptime = info.uptime;
    template.timestamp = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
    template.seq_number = 0;
    template.source_id = 0x000000aa;

    template.flowset_id = 0;
    template.flowset_len = sizeof(template.flowset_id) + sizeof(template.flowset_len) +
        sizeof(template.t1) + sizeof(template.t2);

    template.t1.template_id = TEMPLATE_ID_FULL;
    template.t1.field_count = N_FIELDS_FULL;
    for (i = 0; i < N_FIELDS_FULL; i++)
    {
        template.t1.tl[i].type = template_full_fields[i][0];
        template.t1.tl[i].len = template_full_fields[i][1];
    }

    template.t2.template_id = TEMPLATE_ID_NO_PORTS;
    template.t2.field_count = N_FIELDS_NO_PORTS;
    for (i = 0; i < N_FIELDS_NO_PORTS; i++)
    {
        template.t2.tl[i].type = template_no_ports_fields[i][0];
        template.t2.tl[i].len = template_no_ports_fields[i][1];
    }
}

void export_finish(void)
{
    close(exs.socket_out);
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

    //sendto(exs.socket_out, flowbuf, len, flags, (struct sockaddr *)&exs.dest, addrlen);
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
