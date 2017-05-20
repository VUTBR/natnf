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
#define TIMEOUT_TEMPLATE 60

/* See https://www.iana.org/assignments/ipfix/ipfix.xml#ipfix-nat-event-type
 * for the meaning of these values.*/
#define NAT_CREATE 1
#define NAT_DELETE 2
#define NAT_OTHER 0

/* Outgoing connection identification */
extern int socket_out;
extern struct sockaddr_in sin;

/* TODO Add fields that identify a NAT translation. */
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
    uint8_t nat_event;
};

/** Circular buffer of nat record pointers.
 */
extern struct nat_record *buf_records[RECORDS_MAX];
extern int buf_begin;
extern int buf_end;

/* Exclusive access to the records buffer. */
extern sem_t cnt_buf_empty;
extern sem_t cnt_buf_taken;

void export_init(void);
void export_init_flow(void);
void export_init_template(void);
void export_finish(void);
void export_append(struct nat_record *natr);
void export_send_record(struct nat_record *natr);

struct nat_record *nat_record_new(void);
struct nat_record *nat_record_dup(struct nat_record *natr);

#endif /* EXPORT_H */
