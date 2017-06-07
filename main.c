/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                main.c                       */
/***********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>

#include "export.h"
#include "utils.h"
#include "config.h"
#include "msgs.h"

#define EVENTS_UNLIMITED
#define EVENTS_MAX 10

#ifndef EVENTS_UNLIMITED
# define EVENTS_DONE(cnt) (cnt >= EVENTS_MAX)
#else
# define EVENTS_DONE(cnt) (0)
#endif

static int event_cb(enum nf_conntrack_msg_type type,
                    struct nf_conntrack *ct,
                    void *data)
{
	static int counter = 0;
	int is_nat;
    struct nat_record *natr;

	if (type == NFCT_T_UPDATE)
	{
		return NFCT_CB_CONTINUE;
	}

    /* We're only interested in source NAT. */
    is_nat = nfct_getobjopt(ct, NFCT_GOPT_IS_SNAT) ||
             nfct_getobjopt(ct, NFCT_GOPT_IS_SPAT);

    if (!is_nat)
    {
        return NFCT_CB_CONTINUE;
    }

    natr = nat_record_new();

	parse_nat_header(natr, type, ct);
	print_nat_header(natr);
    export_append(natr);

    counter++;
	if (EVENTS_DONE(counter))
		return NFCT_CB_STOP;

	return NFCT_CB_CONTINUE;
}

void parse_nat_header(struct nat_record *natr,
                      enum nf_conntrack_msg_type type,
                      struct nf_conntrack *ct)
{
    struct in_addr ip;

    if (natr == NULL || ct == NULL)
    {
        return;
    }

    natr->timestamp_ms = get_timestamp_ms();
    natr->pre_nat_src_ip.s_addr = nfct_get_attr_u32(ct, ATTR_ORIG_IPV4_SRC);
    natr->pre_nat_dst_ip.s_addr = nfct_get_attr_u32(ct, ATTR_ORIG_IPV4_DST);
    /* The following two are correctly reversed (dst and src)! */
    natr->post_nat_src_ip.s_addr = nfct_get_attr_u32(ct, ATTR_REPL_IPV4_DST);
    natr->post_nat_dst_ip.s_addr = nfct_get_attr_u32(ct, ATTR_REPL_IPV4_SRC);
    natr->protocol = nfct_get_attr_u8(ct, ATTR_L4PROTO);
    if (natr->protocol == IPPROTO_ICMP)
    {
        natr->icmp_type = nfct_get_attr_u8(ct, ATTR_ICMP_TYPE);
        natr->icmp_code = nfct_get_attr_u8(ct, ATTR_ICMP_CODE);
        /* Insert the ICMP type and code into port values.
         * The upper 8 bits out of 16 contain the type and the lower 8 contain
         * the code. */
        natr->pre_nat_src_port = (natr->icmp_type << 8) | (natr->icmp_code);
        natr->pre_nat_dst_port = (natr->icmp_type << 8) | (natr->icmp_code);
        natr->post_nat_src_port = (natr->icmp_type << 8) | (natr->icmp_code);
        natr->post_nat_dst_port = (natr->icmp_type << 8) | (natr->icmp_code);
    }
    else
    {
        /* Not ICMP */
        natr->pre_nat_src_port = nfct_get_attr_u16(ct, ATTR_ORIG_PORT_SRC);
        natr->pre_nat_dst_port = nfct_get_attr_u16(ct, ATTR_ORIG_PORT_DST);
        /* The following two are correctly reversed (dst and src)! */
        natr->post_nat_src_port = nfct_get_attr_u16(ct, ATTR_REPL_PORT_DST);
        natr->post_nat_dst_port = nfct_get_attr_u16(ct, ATTR_REPL_PORT_SRC);
    }
    printf("timestamp_ms=%llx\n", natr->timestamp_ms);
    if (type == NFCT_T_NEW)
        natr->nat_event = NAT_CREATE;
    else if (type == NFCT_T_DESTROY)
        natr->nat_event = NAT_DELETE;
    else
        natr->nat_event = NAT_OTHER;
}

void print_nat_header(struct nat_record *natr)
{
    char buf[MAX_STRING];
    int cx;

    char *type[3] =
    {
        [0] = "UNKNOWN",
        [1] = "CREATE",
        [2] = "DELETE"
    };

    int is_full = !is_protocol_portless(natr->protocol);

    if (is_full)
    {
        msg("NAT_EVENT=%s, SRC_IP=%s, SRC_PORT=%d, POST_NAT_SRC_IP=%s, POST_NAT_SRC_PORT=%d\
 DST_IP=%s, DST_PORT=%d, POST_NAT_DST_IP=%s, POST_NAT_DST_PORT=%d, PROTOCOL=%d",
                type[natr->nat_event],
                inet_ntoa(natr->pre_nat_src_ip),
                natr->pre_nat_src_port,
                inet_ntoa(natr->post_nat_src_ip),
                natr->post_nat_src_port,
                inet_ntoa(natr->pre_nat_dst_ip),
                natr->pre_nat_dst_port,
                inet_ntoa(natr->post_nat_dst_ip),
                natr->post_nat_dst_port,
                natr->protocol
            );
    }
    else
    {
        /* TODO Why are the ip addresses the same? */
        msg("NAT_EVENT=%s, SRC_IP=%s, POST_NAT_SRC_IP=%s, DST_IP=%s, POST_NAT_DST_IP=%s,\
 PROTOCOL=%d, ICMP_TYPE=%d, ICMP_CODE=%d",
                type[natr->nat_event],
                inet_ntoa(natr->pre_nat_src_ip),
                inet_ntoa(natr->post_nat_src_ip),
                inet_ntoa(natr->pre_nat_dst_ip),
                inet_ntoa(natr->post_nat_dst_ip),
                natr->protocol,
                natr->icmp_type,
                natr->icmp_code
            );
    }

    /* Print some debugging info about the nat record. */
    if (is_debug)
    {
        cx = snprintf(buf, MAX_STRING - 1,"[%s] %s", type[natr->nat_event], inet_ntoa(natr->pre_nat_src_ip));
        
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,":%d", natr->pre_nat_src_port);
        cx += snprintf(buf+cx, MAX_STRING - 1 - cx," %s", inet_ntoa(natr->pre_nat_dst_ip));
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,":%d", natr->pre_nat_dst_port);

        cx += snprintf(buf+cx, MAX_STRING - 1 - cx," <--> ");

        cx += snprintf(buf+cx, MAX_STRING - 1 - cx,"%s", inet_ntoa(natr->post_nat_src_ip));
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,":%d", natr->post_nat_src_port);
        cx += snprintf(buf+cx, MAX_STRING - 1 - cx," %s", inet_ntoa(natr->post_nat_dst_ip));
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,":%d", natr->post_nat_dst_port);

        if (natr->protocol == IPPROTO_ICMP)
        {
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx," [%d] - (%d|%d)",natr->protocol, natr->icmp_type, natr->icmp_code);
        }
    
        DEBUG(buf);
    }
}

/** Callback for the thread which catches NAT events and creates
 * data for the exporter.
 * This is a producer.
 */
void thread_catcher(void *arg)
{
	struct nfct_handle *h;
	struct nf_conntrack *ct;
    int ret;

	h = nfct_open(CONNTRACK, NF_NETLINK_CONNTRACK_NEW | NF_NETLINK_CONNTRACK_DESTROY);
	if (!h) {
		error("nfct_open");
	}

    ct = nfct_new();
	if (!ct) {
		error("nfct_new");
	}

	nfct_callback_register(h, NFCT_T_ALL, event_cb, ct);

	printf("TEST: waiting for events...\n");

    /* The following is a blocking call. */
	ret = nfct_catch(h);

	printf("TEST: conntrack events ");
	if (ret == -1)
		printf("(%d)(%s)\n", ret, strerror(errno));
	else
		printf("(OK)\n");

	nfct_close(h);
}

/** Callback for the exporting thread.
 * It consumes data from the caught NAT events and sends them to the
 * collector.
 */
void thread_exporter(void *arg)
{
    struct nat_record *natr;
    int i, ret, more_records;
    while (1)
    {
        DEBUG("Sleeping...");
        printf("exs.export_timeout=%d\n", exs.export_timeout);
        sleep(exs.export_timeout);
        DEBUG("Sleep ended.");

        DEBUG("sem_wait() call.");
        sem_wait(&cnt_buf_taken);
        pthread_mutex_lock(&mutex_records);
        DEBUG("Enter critical section.");

        more_records = 1;
        while (1)
        {
            /* Gather as many records as possible. */
            for (i = 0; i < MAX_FLOWS; i++)
                natr_buffer[i] = NULL;
            for (i = 0; i < MAX_FLOWS; i++)
            {
                natr = nat_record_dup(buf_records[buf_begin]);
                natr_buffer[i] = natr;
                free(buf_records[buf_begin]);
                buf_begin++;
                sem_post(&cnt_buf_empty);

                /* See if there are more records to gather. */
                ret = sem_trywait(&cnt_buf_taken);
                if (ret == -1 && errno == EAGAIN)
                {
                    more_records = 0;
                    break;
                }
            }

            export_send_records();

            if (!more_records)
                break;
        }

        pthread_mutex_unlock(&mutex_records);
        DEBUG("Leave critical section.");
    }
}

/** Callback for a thread that shall send template packets to the
 * collector periodically.
 */
void thread_template(void *arg)
{
    while (1)
    {
        DEBUG("Sending template");
        export_send_template();
        DEBUG("Template sent.");
        sleep(exs.template_timeout);
    }
}

int main(int argc, char **argv)
{
	int ret;
    unsigned short value;
    pthread_t pt[3];
    pthread_attr_t attr;
    void *statp;

    export_init(argc, argv);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); 

    /* Catcher thread */
    ret = pthread_create(&pt[0], &attr, thread_catcher, NULL);
    if (ret != 0)
    {
        error("Thread not created");
    }

    /* Exporter thread */
    ret = pthread_create(&pt[1], &attr, thread_exporter, NULL);
    if (ret != 0)
    {
        error("Thread not created");
    }

    /* Template sender thread */
    ret = pthread_create(&pt[2], &attr, thread_template, NULL);
    if (ret != 0)
    {
        error("Thread not created");
    }

    for (int i = 0; i < 3; i++)
    {
        ret = pthread_join(pt[i], &statp); 
        if (ret == -1) {
            error("Pthread join fail");
        }
    }

    export_finish();

	ret == -1 ? exit(EXIT_FAILURE) : exit(EXIT_SUCCESS);
}
