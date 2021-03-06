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

    /* We're interested in source and destination NAT. */
    is_nat = nfct_getobjopt(ct, NFCT_GOPT_IS_SNAT) ||
             nfct_getobjopt(ct, NFCT_GOPT_IS_SPAT) ||
             nfct_getobjopt(ct, NFCT_GOPT_IS_DNAT) ||
             nfct_getobjopt(ct, NFCT_GOPT_IS_DPAT);

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

/** Statistic variables
 */
unsigned long long int sent_packets = 0;
uint32_t last_sent_record = 0;
unsigned long long int sent_records = 0;

extern struct log_settings logs;

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
         * the code. 
         * Src port should be set to 0 */
        natr->pre_nat_src_port = 0;
        natr->pre_nat_dst_port = (natr->icmp_type << 8) | (natr->icmp_code);
        natr->post_nat_src_port = 0;
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

	/* Correct ports byte order */
    natr->pre_nat_src_port = ntohs(natr->pre_nat_src_port);
    natr->pre_nat_dst_port = ntohs(natr->pre_nat_dst_port);
    natr->post_nat_src_port = ntohs(natr->post_nat_src_port);
    natr->post_nat_dst_port = ntohs(natr->post_nat_dst_port);
 
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

    if (exs.syslog_enable)
    {
        bzero(&buf, sizeof(buf));

        cx = snprintf(buf, MAX_STRING - 1,"NAT_EVENT=%s, SRC_IP=%s", 
                        type[natr->nat_event], 
                        inet_ntoa(natr->pre_nat_src_ip));
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", SRC_PORT=%d", natr->pre_nat_src_port);
        cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", POST_NAT_SRC_IP=%s", inet_ntoa(natr->post_nat_src_ip));
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", POST_NAT_SRC_PORT=%d", natr->post_nat_src_port);

        cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", DST_IP=%s", inet_ntoa(natr->pre_nat_dst_ip));
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", DST_PORT=%d", natr->pre_nat_dst_port);
        cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", POST_NAT_DST_IP=%s", inet_ntoa(natr->post_nat_dst_ip));
        if (is_full)
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", POST_NAT_DST_PORT=%d", natr->post_nat_dst_port);
        cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", PROTOCOL=%d", natr->protocol);
        if (natr->protocol == IPPROTO_ICMP)
        {
            cx += snprintf(buf+cx, MAX_STRING - 1 - cx,", ICMP_TYPE=%d, ICMP_CODE=%d",
                        natr->icmp_type,
                        natr->icmp_code);
        }

        msg(buf);
    }

    /* Print some debugging info about the nat record. */
    if (exs.verbose)
    {
        bzero(&buf, sizeof(buf));

        /* Time stamp - start */
        char s[1000];
		time_t t = time(NULL);
		struct tm * p = localtime(&t);
		strftime(s, 1000, "%F %H:%M:%S", p);
		cx = snprintf(buf, MAX_STRING - 1,"%s ", s);
		/* Time stamp - end */

        cx += snprintf(buf+cx, MAX_STRING - 1 - cx,"[%s] %s", type[natr->nat_event], inet_ntoa(natr->pre_nat_src_ip));
        
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

    /* The following is a blocking call. */
	ret = nfct_catch(h);

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
    int i, j, ret, more_records, first_run;

    int exported_time = get_timestamp_s();

    while (1)
    {
        DEBUG("sem_wait() call.");
        sem_wait(&cnt_buf_taken);

        first_run = 1;
        i = 0;

        while (1)
        {
            /* Gather as many records as possible. */
            if (first_run)
            {
	            for (j = 0; j < MAX_FLOWS; j++)
	                natr_buffer[j] = NULL;
	            first_run = 0;
			}

			more_records = 1;

			usleep(10);		//not active waiting

            while ( i < MAX_FLOWS )
            {
				//check after first flow gathered
				if (i)
				{
	                /* See if there are more records to gather. */
	                ret = sem_trywait(&cnt_buf_taken);
	                if (ret == -1 && errno == EAGAIN)
	                {
	                    more_records = 0;
	                    break;
	                }
	            }

				//DEBUG("Enter critical section.");
	            pthread_mutex_lock(&mutex_records);

				natr = nat_record_dup(buf_records[buf_begin]);
                natr_buffer[i] = natr;
                free(buf_records[buf_begin]);
                buf_begin++;
                if(RECORDS_MAX==buf_begin){buf_begin=0;}	//modulo RECORDS_MAX
                sem_post(&cnt_buf_empty);

                //DEBUG("Leave critical section.");
                pthread_mutex_unlock(&mutex_records);

                i++;
            }

            /* See if there are more records to gather. */
            ret = sem_trywait(&cnt_buf_taken);
            if (ret == -1 && errno == EAGAIN)
            {
                more_records = 0;
            }

            //full packet OR exceeded export timeout
            if (	((NULL != natr_buffer[MAX_FLOWS-1]) ||
						((exported_time + exs.export_timeout) < get_timestamp_s())
					) &&
					i
			)
            {
				//printf("%d\t%d\t%d\t%d\n",i, more_records,(NULL != natr_buffer[MAX_FLOWS-1]),((exported_time + exs.export_timeout) < get_timestamp_s()));
				export_send_records();

				exported_time = get_timestamp_s();
				
				sent_records=+i;
				++sent_packets;
				last_sent_record = exported_time;

				first_run = 1;
				i = 0;

	            if (!more_records)
	                break;
            }
        }
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

void thread_responder(void *arg)
{
	//Open file to log statistics
	logs.fd_l = fopen(logs.log_path, "a");

	if ( logs.fd_l < 0 )
	{
		printf("Log file for statistics cannot be opened");
		pthread_exit((void *)(1));
	}	

    while (1)
    {
        DEBUG("Saving statistics");
		fprintf(logs.fd_l, "%lld:%lld,%d,%lld\n", get_timestamp_s(), sent_packets, last_sent_record, sent_records);
		fflush(logs.fd_l);
		sleep(10);
    }
}

int main(int argc, char **argv)
{
	int ret;
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
	
	/* User responder thread */
    ret = pthread_create(&pt[3], &attr, thread_responder, NULL);
    if (ret != 0)
    {
        error("Thread not created");
    }

    for (int i = 0; i <= 3; i++)
    {
        ret = pthread_join(pt[i], &statp); 
        if (ret == -1) {
            error("Pthread join fail");
        }
    }

    export_finish();
	
	fclose(logs.fd_l);

	ret == -1 ? exit(EXIT_FAILURE) : exit(EXIT_SUCCESS);
}
