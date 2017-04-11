#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

static int event_cb(enum nf_conntrack_msg_type type,
		    struct nf_conntrack *ct,
		    void *data)
{
	static int n = 0;
	char buf[1024];

	nfct_snprintf(buf, sizeof(buf), ct, type, NFCT_T_UNKNOWN, NFCT_OF_TIME | NFCT_OF_TIMESTAMP);
	printf("%s\n", buf);

	if (++n == 10)
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

    /* TODO What should be the value of value??? */
    value = 0;
    /* TODO Choose between:
     * ATTR_SNAT_PORT,
     * ATTR_DNAT_PORT,
     * ATTR_SNAT_IPV4,
     * ATTR_DNAT_IPV4,
     * */
    nfct_set_attr_u16(ct, ATTR_SNAT_PORT, value);

	nfct_callback_register(h, NFCT_T_NEW, event_cb, NULL);

	printf("TEST: waiting for 10 events...\n");

	ret = nfct_catch(h);

	printf("TEST: conntrack events ");
	if (ret == -1)
		printf("(%d)(%s)\n", ret, strerror(errno));
	else
		printf("(OK)\n");

	nfct_close(h);

	ret == -1 ? exit(EXIT_FAILURE) : exit(EXIT_SUCCESS);
}
