#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <pthread.h>
#include <unistd.h>

#include "export.h"
#include "utils.h"

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

    natr->pre_nat_src_ip.s_addr = nfct_get_attr_u32(ct, ATTR_ORIG_IPV4_SRC);
    natr->pre_nat_dst_ip.s_addr = nfct_get_attr_u32(ct, ATTR_ORIG_IPV4_DST);
    /* The following two are correctly reversed (dst and src)! */
    natr->post_nat_src_ip.s_addr = nfct_get_attr_u32(ct, ATTR_REPL_IPV4_DST);
    natr->post_nat_dst_ip.s_addr = nfct_get_attr_u32(ct, ATTR_REPL_IPV4_SRC);
    natr->pre_nat_src_port = nfct_get_attr_u16(ct, ATTR_ORIG_PORT_SRC);
    natr->pre_nat_dst_port = nfct_get_attr_u16(ct, ATTR_ORIG_PORT_DST);
    /* The following two are correctly reversed (dst and src)! */
    natr->post_nat_src_port = nfct_get_attr_u16(ct, ATTR_REPL_PORT_DST);
    natr->post_nat_dst_port = nfct_get_attr_u16(ct, ATTR_REPL_PORT_SRC);
    if (type == NFCT_T_NEW)
        natr->nat_event = NAT_CREATE;
    else if (type == NFCT_T_DESTROY)
        natr->nat_event = NAT_DELETE;
    else
        natr->nat_event = NAT_OTHER;
}

void print_nat_header(struct nat_record *natr)
{
    char *type[3] =
    {
        [0] = "UNKNOWN",
        [1] = "CREATE",
        [2] = "DELETE"
    };

    printf("[%s]", type[natr->nat_event]);
    printf(" %s", inet_ntoa(natr->pre_nat_src_ip));
    if (natr->pre_nat_src_port != 0)
        printf(":%d", natr->pre_nat_src_port);
    printf(" %s", inet_ntoa(natr->pre_nat_dst_ip));
    if (natr->pre_nat_dst_port != 0)
        printf(":%d", natr->pre_nat_dst_port);

    printf(" <--> ");

    printf("%s", inet_ntoa(natr->post_nat_src_ip));
    if (natr->post_nat_src_port != 0)
        printf(":%d", natr->post_nat_src_port);
    printf(" %s", inet_ntoa(natr->post_nat_dst_ip));
    if (natr->post_nat_dst_port != 0)
        printf(":%d", natr->post_nat_dst_port);

    printf("\n");
    fflush(stdout);
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
    while (1)
    {
        DEBUG("sem_wait() call.");
        sem_wait(&cnt_buf_taken);
        DEBUG("Enter critical section.");
        natr = nat_record_dup(buf_records[buf_begin]);
        free(buf_records[buf_begin]);
        buf_begin++;
        sem_post(&cnt_buf_empty);
        DEBUG("Leave critical section.");
        if (natr == NULL)
            continue;
        export_send_record(natr);
    }
}

/** Callback for a thread that shall send template packets to the
 * collector periodically.
 */
void thread_template(void *arg)
{
    while (1)
    {
        sleep(exs.timeout_template);

        DEBUG("Sending template");

        /* TODO: Send a template. */
    }
}

int main(void)
{
	int ret;
    unsigned short value;
    pthread_t pt[3];
    pthread_attr_t attr;
    void *statp;

    export_init();


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
