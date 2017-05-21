#ifndef EXPORT_H
#define EXPORT_H

#include <netinet/ip.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <semaphore.h>

/* TODO Replace the following with variable user input. */
#define COLLECTOR_IP_STR "127.0.0.1"
#define COLLECTOR_PORT 3001

#define RECORDS_MAX 65536
#define _TEMPLATE_TIMEOUT 60

#define N_FIELDS_FULL (sizeof(template_full_fields) / sizeof(int[2]))
#define N_FIELDS_NO_PORTS (sizeof(template_no_ports_fields) / sizeof(int[2]))

#define TEMPLATE_ID_FULL 256
#define TEMPLATE_ID_NO_PORTS 257

/* See https://www.iana.org/assignments/ipfix/ipfix.xml#ipfix-nat-event-type
 * for the meaning of these values.*/
#define NAT_CREATE 1
#define NAT_DELETE 2
#define NAT_OTHER 0

/* Some template fields definitions.
 * See https://www.iana.org/assignments/ipfix/ipfix.xhtml for their meanings.
 */
#define TL_SRC_IP 8
#define TL_DST_IP 12
#define TL_POST_NAT_SRC_IP 225
#define TL_POST_NAT_DST_IP 226
#define TL_SRC_PORT 7
#define TL_DST_PORT 11
#define TL_POST_NAT_SRC_PORT 227
#define TL_POST_NAT_DST_PORT 228
#define TL_NAT_EVENT 230
#define TL_OBSERVATION_TIME_MS 323

/* Fields that identify a NAT translation. */
struct nat_record
{
    struct in_addr pre_nat_src_ip;
    struct in_addr pre_nat_dst_ip;
    struct in_addr post_nat_src_ip;
    struct in_addr post_nat_dst_ip;
    uint16_t pre_nat_src_port;
    uint16_t pre_nat_dst_port;
    uint16_t post_nat_src_port;
    uint16_t post_nat_dst_port;
    double timestamp_ms;
    uint8_t nat_event;
};

struct export_settings
{
    char *ip_str;
    int port;
    int template_timeout;
    /* Outgoing connection identification: */
    int socket_out;
    struct sockaddr_in dest;
};

extern int template_full_fields[][2];
extern int template_no_ports_fields[][2];

struct template_full;
struct template_no_ports;

struct type_length
{
    uint16_t type;
    uint16_t len;
};

/* A full template, including port numbers. */
extern struct template_full;

/* A template without port numbers, in case they can not be extracted. */
extern struct template_no_ports;

/* The complete template packet. */
extern struct template_packet;

extern struct export_settings exs;

/** Circular buffer of nat record pointers.
 */
extern struct nat_record *buf_records[RECORDS_MAX];
extern int buf_begin;
extern int buf_end;

/* Exclusive access to the records buffer. */
extern sem_t cnt_buf_empty;
extern sem_t cnt_buf_taken;

void export_init_settings(void);
void export_init(void);
void export_init_flow(void);
void export_init_template(void);
void export_finish(void);
void export_append(struct nat_record *natr);
void export_send_record(struct nat_record *natr);

struct nat_record *nat_record_new(void);
struct nat_record *nat_record_dup(struct nat_record *natr);

#endif /* EXPORT_H */
