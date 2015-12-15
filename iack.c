#include <errno.h>
#include <string.h>

#include <netlink/genl/genl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

static int set_iack(struct nl80211_state *state,
                    struct nl_cb *cb,
                    struct nl_msg *msg,
                    int argc, char **argv,
                    enum id_input id)
{
	enum nl80211_iack_state iack_state;

	if (argc != 1) {
		printf("Invalid parameters!\n");
		return 2;
	}

	if (strcmp(argv[0], "on") == 0)
		iack_state = NL80211_IACK_ENABLED;
	else if (strcmp(argv[0], "off") == 0)
		iack_state = NL80211_IACK_DISABLED;
	else {
		printf("Invalid parameter: %s\n", argv[0]);
		return 2;
	}

	NLA_PUT_U32(msg, NL80211_ATTR_IACK_STATE, iack_state);

	return 0;

 nla_put_failure:
	return -ENOBUFS;
}

COMMAND(set, iack, "<on|off>",
        NL80211_CMD_SET_IACK, 0, CIB_NETDEV, set_iack,
        "Set implicit ack state to on or off.");

static int print_iack_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	const char *s;

	nla_parse(attrs, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
	          genlmsg_attrlen(gnlh, 0), NULL);

	if (!attrs[NL80211_ATTR_IACK_STATE])
		return NL_SKIP;

	switch (nla_get_u32(attrs[NL80211_ATTR_IACK_STATE])) {
	case NL80211_IACK_ENABLED:
		s = "on";
		break;
	case NL80211_IACK_DISABLED:
	default:
		s = "off";
		break;
	}

	printf("Implicit ACK: %s\n", s);

	return NL_SKIP;
}

static int get_iack(struct nl80211_state *state,
                    struct nl_cb *cb,
                    struct nl_msg *msg,
                    int argc, char **argv,
                    enum id_input id)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
	          print_iack_handler, NULL);
	return 0;
}

COMMAND(get, iack, "<param>",
        NL80211_CMD_GET_IACK, 0, CIB_NETDEV, get_iack,
        "Retrieve implicit ack state.");
