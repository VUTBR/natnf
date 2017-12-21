/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                export.c                     */
/***********************************************/

#include "export.h"
#include "error.h"
#include "utils.h"
#include "msgs.h"
#include "daemonize.h"

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
    {TL_PROTO, 1},
    {TL_NAT_EVENT, 1},
    {TL_OBSERVATION_TIME_MS, 8} /* 8 because of time_t's size on 64-bit systems. */
};
int template_no_ports_fields[][2] =
{
    {TL_SRC_IP, 4},
    {TL_DST_IP, 4},
    {TL_POST_NAT_SRC_IP, 4},
    {TL_POST_NAT_DST_IP, 4},
    {TL_ICMP_TYPE, 1},
    {TL_ICMP_CODE, 1},
    {TL_PROTO, 1},
    {TL_NAT_EVENT, 1},
    {TL_OBSERVATION_TIME_MS, 8}
};

/* A buffer of NAT record pointers. Used by the exporter thread. */
struct nat_record *natr_buffer[64] = { NULL };

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

struct flow_no_ports
{
    uint64_t observation_time_ms;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint32_t post_nat_src_ip;
    uint32_t post_nat_dst_ip;
    uint16_t icmp_type;
    uint16_t icmp_code;
    uint8_t protocol;
    uint8_t nat_event;
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

    struct flow_no_ports flow;
    char padding[32];
};

struct send_buffer
{
    void *data;
    int next;
    size_t size;
};

struct export_settings exs;
struct log_settings logs;

struct nat_record *buf_records[RECORDS_MAX] = { NULL };
int buf_begin = 0;
int buf_end = 0;

sem_t cnt_buf_empty;
sem_t cnt_buf_taken;

pthread_mutex_t mutex_socket = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_records = PTHREAD_MUTEX_INITIALIZER;

int flow_sequence = 0;

/** A buffer with pre-calculated data fields for flow export.
 */
static struct flow_packet_full flow_full;
static struct flow_packet_no_ports flow_no_ports;

/** A buffer with pre-calculated data fields for template export.
 */
static struct template_packet template;

static struct send_buffer sendbuf;

/** Initialize the settings structure.
 */
void export_init_settings(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo *collector_info;
    int rv;

    exs.ip_str = COLLECTOR_IP_STR;
    exs.port = COLLECTOR_PORT;
    exs.verbose = 0;
    exs.template_timeout = TIMEOUT_TEMPLATE_DEFAULT;
    exs.export_timeout = TIMEOUT_EXPORT_DEFAULT;
    exs.syslog_enable = 0;
    exs.syslog_level = 4;
    exs.daemonize = 0;
	
	logs.log_path = LOG_PATH_DEFAULT;
	
    load_config(argc, argv);

    if (exs.syslog_enable)
    {
        msg_init(0);    // 1 for debug
    }

    if (exs.daemonize)
    {
        if (!daemonize())
        {
            error("Can not daemonize process");
        }
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(exs.ip_str, exs.port, &hints, &collector_info)) != 0) {
		/* getaddrinfo doesn't set errno, thus the error directly here */
        fprintf(stderr, "%s\n", gai_strerror(rv));
 		exit(1);
    }

    // loop through all the results and connect to the first we can
    for (exs.dest = collector_info; exs.dest != NULL; exs.dest = exs.dest->ai_next) {
        if ((exs.socket_out = socket(exs.dest->ai_family, exs.dest->ai_socktype,
                exs.dest->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        break;
    }

    if (exs.dest == NULL) {
        error("client: failed to connect");
    }

}

void export_init_sendbuf(void)
{
    sendbuf.data = malloc(INIT_SENDBUF_SIZE);
    sendbuf.size = INIT_SENDBUF_SIZE;
    sendbuf.next = 0;
}

void export_free_sendbuf(void)
{
    free(sendbuf.data);
}

void export_init(int argc, char **argv)
{
    export_init_settings(argc, argv);
    export_init_flow();
    export_init_template();

    sem_init(&cnt_buf_empty, 0, RECORDS_MAX);
    sem_init(&cnt_buf_taken, 0, 0);
}

void export_init_header(void *packet)
{
    struct packet_header *hdr;
    hdr = (struct packet_header *) packet;

    hdr->version = 9;
    hdr->count = 1; /* A default value, the caller should ideally set it manually. */
    /* Uptime and timestamp will be updated on each template sending, initialize them
     * anyway: */
    hdr->sys_uptime = get_uptime_ms();
    hdr->timestamp = get_timestamp_s();
    hdr->seq_number = flow_sequence;
    hdr->source_id = 0x000000aa;
}

/** Initialize the flow buffer.
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

    template.flowset_id = htons(0);
    template.flowset_len = htons(sizeof(template.flowset_id) + sizeof(template.flowset_len) +
            sizeof(template.t1));

    template.t1.template_id = htons(TEMPLATE_ID_FULL);
    template.t1.field_count = htons(N_FIELDS_FULL);
    for (i = 0; i < N_FIELDS_FULL; i++)
    {
        template.t1.tl[i].type = htons(template_full_fields[i][0]);
        template.t1.tl[i].len = htons(template_full_fields[i][1]);
    }

    /* Template for flows without ports */
    // template.t2.template_id = htons(TEMPLATE_ID_NO_PORTS);
    // template.t2.field_count = htons(N_FIELDS_NO_PORTS);
    // for (i = 0; i < N_FIELDS_NO_PORTS; i++)
    // {
    //     template.t2.tl[i].type = htons(template_no_ports_fields[i][0]);
    //     template.t2.tl[i].len = htons(template_no_ports_fields[i][1]);
    // }
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
    pthread_mutex_lock(&mutex_records);
    DEBUG("Enter critical section.");
    buf_records[buf_end] = natr;
    buf_end++;
    if(RECORDS_MAX==buf_end){buf_end=0;}    //modulo RECORDS_MAX
    pthread_mutex_unlock(&mutex_records);
    sem_post(&cnt_buf_taken);
    DEBUG("Leave critical section.");
}

void export_send_record(struct nat_record *natr)
{
    size_t len;
    int flags, is_full, ret;
    socklen_t addrlen;
    struct packet_header *hdr;

    addrlen = sizeof(struct sockaddr);
    flags = 0;
    /* XXX */
    is_full = 1;
    /* XXX */
    export_init_sendbuf();

    /* Fill in header values. */
    hdr = is_full ? (struct packet_header *) &flow_full
        : (struct packet_header *) &flow_no_ports;
    hdr->count = 1;
    hdr->sys_uptime = get_uptime_ms();
    hdr->timestamp = get_timestamp_s();

    DEBUG("Sending data flow packet...");
    pthread_mutex_lock(&mutex_socket);
    hdr->seq_number = flow_sequence;
    flow_sequence++;

    /* Fill in nat record data. */
    if (is_full)
    {
        flow_full.flowset_id = TEMPLATE_ID_FULL;
        flow_full.flow.src_ip = natr->pre_nat_src_ip.s_addr;
        flow_full.flow.dst_ip = natr->pre_nat_dst_ip.s_addr;
        flow_full.flow.src_port = natr->pre_nat_src_port;
        flow_full.flow.dst_port = natr->pre_nat_dst_port;
        flow_full.flow.post_nat_src_ip = natr->post_nat_src_ip.s_addr;
        flow_full.flow.post_nat_dst_ip = natr->post_nat_dst_ip.s_addr;
        flow_full.flow.post_nat_src_port = natr->post_nat_src_port;
        flow_full.flow.post_nat_dst_port = natr->post_nat_dst_port;
        flow_full.flow.protocol = natr->protocol;
        flow_full.flow.nat_event = natr->nat_event;
        flow_full.flow.observation_time_ms = natr->timestamp_ms;

        serialize_flow_full();
    }
    else
    {
        flow_no_ports.flowset_id = TEMPLATE_ID_NO_PORTS;
        flow_no_ports.flowset_len += (4 - (flow_no_ports.flowset_len % 4)) % 4;
        flow_no_ports.flow.src_ip = natr->pre_nat_src_ip.s_addr;
        flow_no_ports.flow.dst_ip = natr->pre_nat_dst_ip.s_addr;
        flow_no_ports.flow.post_nat_src_ip = natr->post_nat_src_ip.s_addr;
        flow_no_ports.flow.post_nat_dst_ip = natr->post_nat_dst_ip.s_addr;
        flow_no_ports.flow.icmp_type = natr->icmp_type;
        flow_no_ports.flow.icmp_code = natr->icmp_code;
        flow_no_ports.flow.protocol = natr->protocol;
        flow_no_ports.flow.nat_event = natr->nat_event;
        flow_no_ports.flow.observation_time_ms = natr->timestamp_ms;

        serialize_flow_no_ports();
    }

    ret = sendto(exs.socket_out, sendbuf.data, sendbuf.next, flags, exs.dest->ai_addr, exs.dest->ai_addrlen);
    /*
    if (is_full)
        ret = sendto(exs.socket_out, &flow_full, len, flags, (struct sockaddr *)&exs.dest, addrlen);
    else
        ret = sendto(exs.socket_out, &flow_no_ports, len, flags, (struct sockaddr *)&exs.dest, addrlen);
        */
    if (ret == -1)
        perror("sendto");
    pthread_mutex_unlock(&mutex_socket);
    export_free_sendbuf();
    DEBUG("Data flow packet sent.");
}

/** Send all of the records stored in the natr_buffer.
 */
void export_send_records(void)
{
    size_t len;
    int flags, ret, count, i, to_pad;
    int offset_start, flowset_len_offset;
    socklen_t addrlen;
    struct packet_header *hdr;
    struct nat_record *natr;

    addrlen = sizeof(struct sockaddr);
    flags = 0;
    export_init_sendbuf();

    /* count shall hold the number of records to send. */
    for (count = 0; natr_buffer[count] != NULL; count++);

    serialize_u16(9, &sendbuf, 1); /* version */
    serialize_u16(count, &sendbuf, 1);
    serialize_u32(get_uptime_ms(), &sendbuf, 1);
    serialize_u32(get_timestamp_s(), &sendbuf, 1);
    serialize_u32(flow_sequence, &sendbuf, 1);
    flow_sequence++;
    serialize_u32(0x000000aa, &sendbuf, 1); /* Source ID */
    offset_start = sendbuf.next; /* To know where the padded area starts. */
    serialize_u16(TEMPLATE_ID_FULL, &sendbuf, 1); /* Flowset ID. */
    flowset_len_offset = sendbuf.next; /* To know the flowset_len's offset. */
    serialize_u16(0, &sendbuf, 1); /* Dummy value for flowset length. */

    i = 0;
    for (natr = natr_buffer[i]; natr != NULL; natr = natr_buffer[++i])
    {
        serialize_u32(natr->pre_nat_src_ip.s_addr, &sendbuf, 0);
        serialize_u32(natr->pre_nat_dst_ip.s_addr, &sendbuf, 0);
        serialize_u16(natr->pre_nat_src_port, &sendbuf, 1);
        serialize_u16(natr->pre_nat_dst_port, &sendbuf, 1);
        serialize_u32(natr->post_nat_src_ip.s_addr, &sendbuf, 0);
        serialize_u32(natr->post_nat_dst_ip.s_addr, &sendbuf, 0);
        serialize_u16(natr->post_nat_src_port, &sendbuf, 1);
        serialize_u16(natr->post_nat_dst_port, &sendbuf, 1);
        serialize_u8(natr->protocol, &sendbuf, 1);
        serialize_u8(natr->nat_event, &sendbuf, 1);
        serialize_u64(natr->timestamp_ms, &sendbuf, 1);
        free(natr);
    }

    /* Zero padding to a 32-bit boundary.
     * Pads 0 to 3 bytes. */
    to_pad = (4 - (sendbuf.next % 4)) % 4;
    for (i = 0; i < to_pad; i++)
    {
        serialize_u8(flow_full.padding[i], &sendbuf, 1);
    }
    /* Set the length of the flowset to (next - offset_start) retrospectively. */
    sendbuf_set_u16(&sendbuf, sendbuf.next - offset_start, flowset_len_offset, 1);

    DEBUG("Sending data flow packet...");
    pthread_mutex_lock(&mutex_socket);

    ret = sendto(exs.socket_out, sendbuf.data, sendbuf.next, flags, exs.dest->ai_addr, exs.dest->ai_addrlen);
    if (ret == -1)
        perror("sendto");
    pthread_mutex_unlock(&mutex_socket);
    export_free_sendbuf();
    DEBUG("Data flow packet sent.");

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
    template.version = htons(9);
    template.count = htons(1);
    template.source_id = htonl(0x000000aa);
    template.sys_uptime = htonl(get_uptime_ms());
    template.timestamp = htonl(get_timestamp_s());
    template.seq_number = htonl(flow_sequence);
    flow_sequence++;
    sendto(exs.socket_out, &template, len, flags, exs.dest->ai_addr, exs.dest->ai_addrlen);
    pthread_mutex_unlock(&mutex_socket);
}

void reserve_space(struct send_buffer *b, size_t bytes)
{
    if (b->next + bytes > b->size)
    {
        b->data = realloc(b->data, b->size * 2);
        b->size *= 2;
    }
}

void serialize_u8(uint8_t x, struct send_buffer *b, int is_order)
{
    int size = sizeof(uint8_t);
    if (is_order)
        ; /* nop */
    reserve_space(b, size);
    memcpy(((char *)b->data) + b->next, &x, size);
    b->next += size;
}

void serialize_u16(uint16_t x, struct send_buffer *b, int is_order)
{
    int size = sizeof(uint16_t);
    if (is_order)
        x = htons(x);
    reserve_space(b, size);
    memcpy(((char *)b->data) + b->next, &x, size);
    b->next += size;
}

void serialize_u32(uint32_t x, struct send_buffer *b, int is_order)
{
    int size = sizeof(uint32_t);
    if (is_order)
        x = htonl(x);
    reserve_space(b, size);
    memcpy(((char *)b->data) + b->next, &x, size);
    b->next += size;
}

void serialize_u64(uint64_t x, struct send_buffer *b, int is_order)
{
    int size = sizeof(uint64_t);
    if (is_order)
        x = bswap_64(x);
    reserve_space(b, size);
    memcpy(((char *)b->data) + b->next, &x, size);
    b->next += size;
}

void serialize_flow_full(void)
{
    int to_pad;
    int flowset_len_offset, offset_start;

    serialize_u16(flow_full.version, &sendbuf, 1);
    serialize_u16(flow_full.count, &sendbuf, 1);
    serialize_u32(flow_full.sys_uptime, &sendbuf, 1);
    serialize_u32(flow_full.timestamp, &sendbuf, 1);
    serialize_u32(flow_full.seq_number, &sendbuf, 1);
    serialize_u32(flow_full.source_id, &sendbuf, 1);
    offset_start = sendbuf.next;
    serialize_u16(flow_full.flowset_id, &sendbuf, 1);
    flowset_len_offset = sendbuf.next;
    serialize_u16(flow_full.flowset_len, &sendbuf, 1);
    serialize_u32(flow_full.flow.src_ip, &sendbuf, 0);
    serialize_u32(flow_full.flow.dst_ip, &sendbuf, 0);
    serialize_u16(flow_full.flow.src_port, &sendbuf, 1);
    serialize_u16(flow_full.flow.dst_port, &sendbuf, 1);
    serialize_u32(flow_full.flow.post_nat_src_ip, &sendbuf, 0);
    serialize_u32(flow_full.flow.post_nat_dst_ip, &sendbuf, 0);
    serialize_u16(flow_full.flow.post_nat_src_port, &sendbuf, 1);
    serialize_u16(flow_full.flow.post_nat_dst_port, &sendbuf, 1);
    serialize_u8(flow_full.flow.protocol, &sendbuf, 1);
    serialize_u8(flow_full.flow.nat_event, &sendbuf, 1);
    serialize_u64(flow_full.flow.observation_time_ms, &sendbuf, 1);

    /* Zero padding to a 32-bit boundary.
     * Pads 0 to 3 bytes. */
    to_pad = (4 - (sendbuf.next % 4)) % 4;
    for (int i = 0; i < to_pad; i++)
    {
        serialize_u8(flow_full.padding[i], &sendbuf, 1);
    }
    //printf("sendbuf.next=%d, offset_start=%d, flowset_len_offset=%d\n",
    //        sendbuf.next, offset_start, flowset_len_offset);
    //printf("len=%d, offset=%d\n", sendbuf.next - offset_start, flowset_len_offset);

    /* Set the length of the flowset to (next - offset_start) retrospectively. */
    sendbuf_set_u16(&sendbuf, sendbuf.next - offset_start, flowset_len_offset, 1);
}

void serialize_flow_no_ports(void)
{
    int to_pad;
    int flowset_len_offset, offset_start;

    serialize_u16(flow_no_ports.version, &sendbuf, 1);
    serialize_u16(flow_no_ports.count, &sendbuf, 1);
    serialize_u32(flow_no_ports.sys_uptime, &sendbuf, 1);
    serialize_u32(flow_no_ports.timestamp, &sendbuf, 1);
    serialize_u32(flow_no_ports.seq_number, &sendbuf, 1);
    serialize_u32(flow_no_ports.source_id, &sendbuf, 1);
    offset_start = sendbuf.next;
    serialize_u16(flow_no_ports.flowset_id, &sendbuf, 1);
    flowset_len_offset = sendbuf.next;
    serialize_u16(flow_no_ports.flowset_len, &sendbuf, 1);
    serialize_u32(flow_no_ports.flow.src_ip, &sendbuf, 0);
    serialize_u32(flow_no_ports.flow.dst_ip, &sendbuf, 0);
    serialize_u32(flow_no_ports.flow.post_nat_src_ip, &sendbuf, 0);
    serialize_u32(flow_no_ports.flow.post_nat_dst_ip, &sendbuf, 0);
    serialize_u8(flow_no_ports.flow.icmp_type, &sendbuf, 1);
    serialize_u8(flow_no_ports.flow.icmp_code, &sendbuf, 1);
    serialize_u8(flow_no_ports.flow.protocol, &sendbuf, 1);
    serialize_u8(flow_no_ports.flow.nat_event, &sendbuf, 1);
    serialize_u64(flow_no_ports.flow.observation_time_ms, &sendbuf, 1);

    to_pad = (4 - (sendbuf.next % 4)) % 4;
    for (int i = 0; i < to_pad; i++)
    {
        serialize_u8(flow_full.padding[i], &sendbuf, 1);
    }
    printf("sendbuf.next=%d, offset_start=%d, flowset_len_offset=%d\n",
            sendbuf.next, offset_start, flowset_len_offset);
    printf("len=%d, offset=%d\n", sendbuf.next - offset_start, flowset_len_offset);

    /* Set the length of the flowset to (next - offset_start) retrospectively. */
    sendbuf_set_u16(&sendbuf, sendbuf.next - offset_start, flowset_len_offset, 1);
}

/** Set value of buffer at offset to val.
 */
void sendbuf_set_u8(struct send_buffer *b, uint8_t val, int offset, int is_order)
{
    if (is_order)
        ;
    memcpy(((char *)b->data) + offset, &val, sizeof(val));
}

void sendbuf_set_u16(struct send_buffer *b, uint16_t val, int offset, int is_order)
{
    if (is_order)
        val = htons(val);
    memcpy(((char *)b->data) + offset, &val, sizeof(val));
}

void sendbuf_set_u32(struct send_buffer *b, uint32_t val, int offset, int is_order)
{
    if (is_order)
        val = htonl(val);
    memcpy(((char *)b->data) + offset, &val, sizeof(val));
}

void sendbuf_set_u64(struct send_buffer *b, uint64_t val, int offset, int is_order)
{
    if (is_order)
        val = bswap_64(val);
    memcpy(((char *)b->data) + offset, &val, sizeof(val));
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
