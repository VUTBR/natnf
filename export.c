#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <pthread.h>

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
struct packet_header
{
    uint16_t version;
    uint16_t count;
    uint32_t sys_uptime;
    uint32_t timestamp;
    uint32_t seq_number;
    uint32_t source_id;
};
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

struct flow_packet_full
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

    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t post_nat_src_ip;
    uint32_t post_nat_dst_ip;
    uint16_t post_nat_src_port;
    uint16_t post_nat_dst_port;
    uint8_t nat_event;
    double observation_time_ms;
};

struct flow_packet_no_ports
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

    uint32_t src_ip;
    uint32_t dst_ip;
    uint32_t post_nat_src_ip;
    uint32_t post_nat_dst_ip;
    uint8_t nat_event;
    double observation_time_ms;
};

struct export_settings exs;

struct nat_record *buf_records[RECORDS_MAX] = { NULL };
int buf_begin = 0;
int buf_end = 0;

sem_t cnt_buf_empty;
sem_t cnt_buf_taken;

pthread_mutex_t mutex_socket = PTHREAD_MUTEX_INITIALIZER;

int flow_sequence = 0;

/** A buffer with pre-calculated data fields for flow export.
 */
static struct flow_packet_full flow_full;
static struct flow_packet_no_ports flow_no_ports;

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
    bzero(&exs.dest, sizeof(exs.dest));
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

void export_init_header(void *packet)
{
    struct packet_header *hdr;
    hdr = (struct packet_header *) packet;

    hdr->version = htons(9);
    hdr->count = htons(1); /* A default value, the caller should ideally set it manually. */
    /* Uptime and timestamp will be updated on each template sending, initialize them
     * anyway: */
    hdr->sys_uptime = htonl(get_uptime_sec());
    hdr->timestamp = htonl(get_timestamp_ms());
    hdr->seq_number = htonl(flow_sequence);
    hdr->source_id = htonl(0x000000aa);
}

/** Initialize the flow buffer.
 * TODO
 */
void export_init_flow(void)
{
    bzero(&flow_full, sizeof(flow_full));
    bzero(&flow_no_ports, sizeof(flow_no_ports));

    export_init_header(&flow_full);
    export_init_header(&flow_no_ports);
}

/** Initialize the template buffer.
 */
void export_init_template(void)
{
    int ret, i;

    bzero(&template, sizeof(template));

    export_init_header(&template);
    template.count = htons(2); /* 2 templates in a flow. */

    template.flowset_id = htons(0);
    template.flowset_len = htons(sizeof(template.flowset_id) + sizeof(template.flowset_len) +
            sizeof(template.t1) + sizeof(template.t2));

    template.t1.template_id = htons(TEMPLATE_ID_FULL);
    template.t1.field_count = htons(N_FIELDS_FULL);
    for (i = 0; i < N_FIELDS_FULL; i++)
    {
        template.t1.tl[i].type = htons(template_full_fields[i][0]);
        template.t1.tl[i].len = htons(template_full_fields[i][1]);
    }

    template.t2.template_id = htons(TEMPLATE_ID_NO_PORTS);
    template.t2.field_count = htons(N_FIELDS_NO_PORTS);
    for (i = 0; i < N_FIELDS_NO_PORTS; i++)
    {
        template.t2.tl[i].type = htons(template_no_ports_fields[i][0]);
        template.t2.tl[i].len = htons(template_no_ports_fields[i][1]);
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

void export_send_template(void)
{
    size_t len;
    int flags;
    socklen_t addrlen;

    len = sizeof(template);
    flags = 0;
    addrlen = sizeof(struct sockaddr);

    pthread_mutex_lock(&mutex_socket);
    template.sys_uptime = htonl(get_uptime_sec());
    template.timestamp = htonl(get_timestamp_ms());
    template.seq_number = htonl(flow_sequence);
    flow_sequence++;
    sendto(exs.socket_out, &template, len, flags, (struct sockaddr *)&exs.dest, addrlen);
    pthread_mutex_unlock(&mutex_socket);
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
