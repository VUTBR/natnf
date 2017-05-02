#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

#define EVENTS_UNLIMITED
#define EVENTS_MAX 10

#ifndef EVENTS_UNLIMITED
# define EVENTS_DONE(cnt) (cnt >= EVENTS_MAX)
#else
# define EVENTS_DONE(cnt) (0)
#endif

const char S_SRC_IP[]	= "src=";
const char S_DST_IP[]	= "dst=";
const char S_SRC_PORT[]	= "sport=";
const char S_DST_PORT[]	= "dport=";

enum direction {
	LOCAL_OUT,
	LOCAL_IN
};

struct nat_ip_port {
	char src_ip[16];
	char src_port[6];
    char dst_ip[16];
    char dst_port[6];
};

static int event_cb(enum nf_conntrack_msg_type type,
					struct nf_conntrack *ct,
					void *data)
{
	static int counter = 0;
	int is_nat;
	char buf[1024];
	struct nf_conntrack *ct_orig, *ct_repl;
	struct nat_ip_port *nat;

	ct_orig = nfct_new();
	if (!ct_orig) {
		perror("nfct_new");
		return 0;
	}

	ct_repl = nfct_new();
	if (!ct_repl) {
		perror("nfct_new");
		return 0;
	}

	nat = (struct nat_ip_port*)malloc(sizeof(struct nat_ip_port));

	if ( NFCT_T_UPDATE == type )
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

    nfct_copy(ct_orig, ct, NFCT_CP_ORIG);
    nfct_copy(ct_repl, ct, NFCT_CP_REPL);

	nfct_snprintf(buf, sizeof(buf), ct, type, NFCT_O_DEFAULT, NFCT_OF_TIME | NFCT_OF_TIMESTAMP);
	printf("%s\n", buf);

	parse_nat_header(nat, ct_orig, type, LOCAL_OUT);
	print_nat_header(nat, LOCAL_OUT);

	parse_nat_header(nat, ct_repl, type, LOCAL_IN);
	print_nat_header(nat, LOCAL_IN);

    counter++;
	if (EVENTS_DONE(counter))
		return NFCT_CB_STOP;

	return NFCT_CB_CONTINUE;
}

void parse_nat_header (	struct nat_ip_port *nat, 
						struct nf_conntrack *ct,
						enum nf_conntrack_msg_type type,
						enum direction dir)
{
	char buf[1024];

	char *start, *end;

	nfct_snprintf(buf, sizeof(buf), ct, type, NFCT_O_DEFAULT, NFCT_OF_TIME | NFCT_OF_TIMESTAMP);

	//ensure NULL string if parse fail
	nat->src_ip[0]	= 0;
	nat->src_port[0]= 0;
	nat->dst_ip[0]	= 0;
	nat->dst_port[0]= 0;

	start = strstr(buf, S_SRC_IP);
	if ( NULL != start )
	{
		start += sizeof S_SRC_IP - 1;	// length of "src="
		end = strchr(start,' ');

		if ( LOCAL_OUT == dir )
		{
			strncpy(nat->src_ip, start, end-start);
			nat->src_ip[end-start+1] = 0;
		}
		else
		{
			//printf("\nDST IP: %.*s\n", end-start, start);
			strncpy(nat->dst_ip, start, end-start);
			nat->dst_ip[end-start+1] = 0;
		}
	}

	start = strstr(buf, S_SRC_PORT);
	if ( NULL != start )
	{
		start += sizeof S_SRC_PORT - 1;	// length of "sport="
		end = strchr(start,' ');

		if ( LOCAL_OUT == dir )
		{
			strncpy(nat->src_port, start, end-start);
			nat->src_port[end-start+1] = 0;
		}
		else
		{
			strncpy(nat->dst_port, start, end-start);
			nat->dst_port[end-start+1] = 0;
		}
	}

	start = strstr(buf, S_DST_IP);
	if ( NULL != start )
	{
		start += sizeof S_DST_IP - 1;	// length of "dst="
		end = strchr(start,' ');

		if ( LOCAL_OUT == dir )
		{
			strncpy(nat->dst_ip, start, end-start);
			nat->dst_ip[end-start+1] = 0;
		}
		else
		{
			strncpy(nat->src_ip, start, end-start);
			nat->src_ip[end-start+1] = 0;
		}
	}

	start = strstr(buf, S_DST_PORT);
	if ( NULL != start )
	{
		start += sizeof S_DST_PORT - 1;	// length of "dport="
		end = strchr(start,' ');

		if ( LOCAL_OUT == dir )
		{
			strncpy(nat->dst_port, start, 5);	//till end of string
			nat->dst_port[6] = 0;
		}
		else
		{
			strncpy(nat->src_port, start, 5);	//till end of string
			nat->src_port[6] = 0;
		}
	}

	return;
}

void print_nat_header (	struct nat_ip_port *nat,
						enum direction dir)
{
	if ( LOCAL_OUT == dir )
	{
		printf("%s:%s %s:%s\t", nat->src_ip, nat->src_port, nat->dst_ip, nat->dst_port);
	}
	else
	{
		printf("-->\t%s:%s %s:%s\n", nat->src_ip, nat->src_port, nat->dst_ip, nat->dst_port);
	}
}

int main(void)
{
	int ret;
	struct nfct_handle *h;
	struct nf_conntrack *ct;
    unsigned short value;

	h = nfct_open(CONNTRACK, NF_NETLINK_CONNTRACK_NEW);
	if (!h) {
		perror("nfct_open");
		return 0;
	}

    ct = nfct_new();
	if (!ct) {
		perror("nfct_new");
		return 0;
	}

	nfct_callback_register(h, NFCT_T_NEW, event_cb, ct);

	printf("TEST: waiting for events...\n");

	ret = nfct_catch(h);

	printf("TEST: conntrack events ");
	if (ret == -1)
		printf("(%d)(%s)\n", ret, strerror(errno));
	else
		printf("(OK)\n");

	nfct_close(h);

	ret == -1 ? exit(EXIT_FAILURE) : exit(EXIT_SUCCESS);
}
