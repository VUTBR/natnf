#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

#define EVENTS_MAX 10

#ifdef EVENTS_UNLIMITED
# define EVENTS_DONE(cnt) (cnt == EVENTS_MAX)
#else
# define EVENTS_DONE(cnt) (0)
#endif

static int event_cb(enum nf_conntrack_msg_type type,
		    struct nf_conntrack *ct,
		    void *data)
{
	static int counter = 0;
    int is_nat;
	char buf[1024];

    /* We're only interested in source NAT. */
    is_nat = nfct_getobjopt(ct, NFCT_GOPT_IS_SNAT) ||
             nfct_getobjopt(ct, NFCT_GOPT_IS_SPAT);

    if (!is_nat)
    {
        return NFCT_CB_CONTINUE;
    }

	nfct_snprintf(buf, sizeof(buf), ct, type, NFCT_O_DEFAULT, NFCT_OF_TIME | NFCT_OF_TIMESTAMP);
	printf("%s\n", buf);

    counter++;
	if (EVENTS_DONE(counter))
		return NFCT_CB_STOP;

	return NFCT_CB_CONTINUE;
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
