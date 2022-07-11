/*	$OpenBSD: rtr_proto.c,v 1.7 2022/07/11 16:47:27 claudio Exp $ */

/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/tree.h>
#include <errno.h>
#include <stdint.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"
#include "log.h"

struct rtr_header {
	uint8_t		version;
	uint8_t		type;
	uint16_t	session_id; /* or error code */
	uint32_t	length;
};

#define RTR_MAX_LEN		2048
#define RTR_DEFAULT_REFRESH	3600
#define RTR_DEFAULT_RETRY	600
#define RTR_DEFAULT_EXPIRE	7200

enum rtr_pdu_type {
	SERIAL_NOTIFY = 0,
	SERIAL_QUERY,
	RESET_QUERY,
	CACHE_RESPONSE,
	IPV4_PREFIX,
	IPV6_PREFIX = 6,
	END_OF_DATA = 7,
	CACHE_RESET = 8,
	ROUTER_KEY = 9,
	ERROR_REPORT = 10,
};

#define FLAG_ANNOUNCE	0x1
#define FLAG_MASK	FLAG_ANNOUNCE
struct rtr_ipv4 {
	uint8_t		flags;
	uint8_t		prefixlen;
	uint8_t		maxlen;
	uint8_t		zero;
	uint32_t	prefix;
	uint32_t	asnum;
};

struct rtr_ipv6 {
	uint8_t		flags;
	uint8_t		prefixlen;
	uint8_t		maxlen;
	uint8_t		zero;
	uint32_t	prefix[4];
	uint32_t	asnum;
};

struct rtr_endofdata {
	uint32_t	serial;
	uint32_t	refresh;
	uint32_t	retry;
	uint32_t	expire;
};

enum rtr_event {
	RTR_EVNT_START,
	RTR_EVNT_CON_OPEN,
	RTR_EVNT_CON_CLOSED,
	RTR_EVNT_TIMER_REFRESH,
	RTR_EVNT_TIMER_RETRY,
	RTR_EVNT_TIMER_EXPIRE,
	RTR_EVNT_SEND_ERROR,
	RTR_EVNT_SERIAL_NOTIFY,
	RTR_EVNT_CACHE_RESPONSE,
	RTR_EVNT_END_OF_DATA,
	RTR_EVNT_CACHE_RESET,
	RTR_EVNT_NO_DATA,
};

static const char *rtr_eventnames[] = {
	"start",
	"connection open",
	"connection closed",
	"refresh timer expired",
	"retry timer expired",
	"expire timer expired",
	"sent error",
	"serial notify received",
	"cache response received",
	"end of data received",
	"cache reset received",
	"no data"
};

enum rtr_state {
	RTR_STATE_CLOSED,
	RTR_STATE_ERROR,
	RTR_STATE_IDLE,
	RTR_STATE_ACTIVE,
};

static const char *rtr_statenames[] = {
	"closed",
	"error",
	"idle",
	"active"
};

struct rtr_session {
	TAILQ_ENTRY(rtr_session)	entry;
	char				descr[PEER_DESCR_LEN];
	struct roa_tree			roa_set;
	struct ibuf_read		r;
	struct msgbuf			w;
	struct timer_head		timers;
	uint32_t			id;		/* rtr_config id */
	uint32_t			serial;
	uint32_t			refresh;
	uint32_t			retry;
	uint32_t			expire;
	int				session_id;
	int				fd;
	enum rtr_state			state;
	enum reconf_action		reconf_action;
	enum rtr_error			last_sent_error;
	enum rtr_error			last_recv_error;
	char				last_sent_msg[REASON_LEN];
	char				last_recv_msg[REASON_LEN];
};

TAILQ_HEAD(, rtr_session) rtrs = TAILQ_HEAD_INITIALIZER(rtrs);

static void	rtr_fsm(struct rtr_session *, enum rtr_event);

static const char *
log_rtr(struct rtr_session *rs)
{
	return rs->descr;
}

static const char *
log_rtr_type(enum rtr_pdu_type type)
{
	static char buf[20];

	switch (type) {
	case SERIAL_NOTIFY:
		return "serial notify";
	case SERIAL_QUERY:
		return "serial query";
	case RESET_QUERY:
		return "reset query";
	case CACHE_RESPONSE:
		return "cache response";
	case IPV4_PREFIX:
		return "IPv4 prefix";
	case IPV6_PREFIX:
		return "IPv6 prefix";
	case END_OF_DATA:
		return "end of data";
	case CACHE_RESET:
		return "cache reset";
	case ROUTER_KEY:
		return "router key";
	case ERROR_REPORT:
		return "error report";
	default:
		snprintf(buf, sizeof(buf), "unknown %u", type);
		return buf;
	}
};

static struct ibuf *
rtr_newmsg(enum rtr_pdu_type type, uint32_t len, uint16_t session_id)
{
	struct ibuf *buf;
	struct rtr_header rh;

	if (len > RTR_MAX_LEN) {
		errno = ERANGE;
		return NULL;
	}
	len += sizeof(rh);
	if ((buf = ibuf_open(len)) == NULL)
		return NULL;

	memset(&rh, 0, sizeof(rh));
	rh.version = 1;
	rh.type = type;
	rh.session_id = htons(session_id);
	rh.length = htonl(len);

	/* cannot fail with fixed buffers */
	ibuf_add(buf, &rh, sizeof(rh));
	return buf;
}

/*
 * Try to send an error PDU to cache, put connection into error
 * state.
 */
static void
rtr_send_error(struct rtr_session *rs, enum rtr_error err, char *msg,
    void *pdu, size_t len)
{
	struct ibuf *buf;
	size_t mlen = 0;
	uint32_t hdrlen;

	rs->last_sent_error = err;
	if (msg) {
		mlen = strlen(msg);
		strlcpy(rs->last_sent_msg, msg, sizeof(rs->last_sent_msg));
	} else
		memset(rs->last_sent_msg, 0, sizeof(rs->last_sent_msg));

	rtr_fsm(rs, RTR_EVNT_SEND_ERROR);

	buf = rtr_newmsg(ERROR_REPORT, 2 * sizeof(hdrlen) + len + mlen, err);
	if (buf == NULL) {
		log_warn("rtr %s: send error report", log_rtr(rs));
		return;
	}

	/* cannot fail with fixed buffers */
	hdrlen = ntohl(len);
	ibuf_add(buf, &hdrlen, sizeof(hdrlen));
	ibuf_add(buf, pdu, len);
	hdrlen = ntohl(mlen);
	ibuf_add(buf, &hdrlen, sizeof(hdrlen));
	ibuf_add(buf, msg, mlen);
	ibuf_close(&rs->w, buf);

	log_warnx("%s: sending error report[%u] %s", log_rtr(rs), err,
	    msg ? msg : "");
}

static void
rtr_reset_query(struct rtr_session *rs)
{
	struct ibuf *buf;

	buf = rtr_newmsg(RESET_QUERY, 0, 0);
	if (buf == NULL) {
		log_warn("rtr %s: send reset query", log_rtr(rs));
		rtr_send_error(rs, INTERNAL_ERROR, "out of memory", NULL, 0);
		return;
	}
	ibuf_close(&rs->w, buf);
}

static void
rtr_serial_query(struct rtr_session *rs)
{
	struct ibuf *buf;
	uint32_t s;

	buf = rtr_newmsg(SERIAL_QUERY, sizeof(s), rs->session_id);
	if (buf == NULL) {
		log_warn("rtr %s: send serial query", log_rtr(rs));
		rtr_send_error(rs, INTERNAL_ERROR, "out of memory", NULL, 0);
		return;
	}

	/* cannot fail with fixed buffers */
	s = htonl(rs->serial);
	ibuf_add(buf, &s, sizeof(s));
	ibuf_close(&rs->w, buf);
}

/*
 * Validate the common rtr header (first 8 bytes) including the
 * included length field.
 * Returns -1 on failure. On success msgtype and msglen are set
 * and the function return 0.
 */
static int
rtr_parse_header(struct rtr_session *rs, void *buf,
    size_t *msglen, enum rtr_pdu_type *msgtype)
{
	struct rtr_header rh;
	uint32_t len = 16;	/* default for ERROR_REPORT */
	int session_id;

	memcpy(&rh, buf, sizeof(rh));

	if (rh.version != 1) {
		log_warnx("rtr %s: received message with unsupported version",
		    log_rtr(rs));
		rtr_send_error(rs, UNEXP_PROTOCOL_VERS, NULL, &rh, sizeof(rh));
		return -1;
	}

	*msgtype = rh.type;
	*msglen = ntohl(rh.length);

	switch (*msgtype) {
	case SERIAL_NOTIFY:
		session_id = rs->session_id;
		len = 12;
		break;
	case CACHE_RESPONSE:
		/* set session_id if not yet happened */
		if (rs->session_id == -1)
			rs->session_id = ntohs(rh.session_id);
		session_id = rs->session_id;
		len = 8;
		break;
	case IPV4_PREFIX:
		session_id = 0;
		len = 20;
		break;
	case IPV6_PREFIX:
		session_id = 0;
		len = 32;
		break;
	case END_OF_DATA:
		session_id = rs->session_id;
		len = 24;
		break;
	case CACHE_RESET:
		session_id = 0;
		len = 8;
		break;
	case ROUTER_KEY:
		len = 36;	/* XXX probably too small, but we ignore it */
		/* FALLTHROUGH */
	case ERROR_REPORT:
		if (*msglen > RTR_MAX_LEN) {
			log_warnx("rtr %s: received %s: msg too big: %zu byte",
			    log_rtr(rs), log_rtr_type(*msgtype), *msglen);
			rtr_send_error(rs, CORRUPT_DATA, "too big",
			    &rh, sizeof(rh));
			return -1;
		}
		if (*msglen < len) {
			log_warnx("rtr %s: received %s: msg too small: "
			    "%zu byte", log_rtr(rs), log_rtr_type(*msgtype),
			    *msglen);
			rtr_send_error(rs, CORRUPT_DATA, "too small",
			    &rh, sizeof(rh));
			return -1;
		}
		/*
		 * session_id check ommitted since ROUTER_KEY and ERROR_REPORT
		 * use the field for different things.
		 */
		return 0;
	default:
		log_warnx("rtr %s: received unknown message: type %u",
		    log_rtr(rs), *msgtype);
		rtr_send_error(rs, UNSUPP_PDU_TYPE, NULL, &rh, sizeof(rh));
		return -1;
	}

	if (len != *msglen) {
		log_warnx("rtr %s: received %s: illegal len: %zu byte not %u",
		    log_rtr(rs), log_rtr_type(*msgtype), *msglen, len);
		rtr_send_error(rs, CORRUPT_DATA, "bad length",
		    &rh, sizeof(rh));
		return -1;
	}

	if (session_id != ntohs(rh.session_id)) {
		/* ignore SERIAL_NOTIFY during startup */
		if (rs->session_id == -1 && *msgtype == SERIAL_NOTIFY)
			return 0;

		log_warnx("rtr %s: received %s: bad session_id: %d != %d",
		    log_rtr(rs), log_rtr_type(*msgtype), ntohs(rh.session_id),
		    session_id);
		rtr_send_error(rs, CORRUPT_DATA, "bad session_id",
		    &rh, sizeof(rh));
		return -1;
	}

	return 0;
}

static int
rtr_parse_notify(struct rtr_session *rs, uint8_t *buf, size_t len)
{
	if (rs->state == RTR_STATE_ACTIVE) {
		log_warnx("rtr %s: received %s: while active (ignored)",
		    log_rtr(rs), log_rtr_type(SERIAL_NOTIFY));
		return 0;
	}

	rtr_fsm(rs, RTR_EVNT_SERIAL_NOTIFY);
	return 0;
}

static int
rtr_parse_cache_response(struct rtr_session *rs, uint8_t *buf, size_t len)
{
	if (rs->state != RTR_STATE_IDLE) {
		log_warnx("rtr %s: received %s: out of context",
		    log_rtr(rs), log_rtr_type(CACHE_RESPONSE));
		return -1;
	}

	rtr_fsm(rs, RTR_EVNT_CACHE_RESPONSE);
	return 0;
}

static int
rtr_parse_ipv4_prefix(struct rtr_session *rs, uint8_t *buf, size_t len)
{
	struct rtr_ipv4 ip4;
	struct roa *roa;

	if (len != sizeof(struct rtr_header) + sizeof(ip4)) {
		log_warnx("rtr %s: received %s: bad pdu len",
		    log_rtr(rs), log_rtr_type(IPV4_PREFIX));
		rtr_send_error(rs, CORRUPT_DATA, "bad len", buf, len);
		return -1;
	}

	if (rs->state != RTR_STATE_ACTIVE) {
		log_warnx("rtr %s: received %s: out of context",
		    log_rtr(rs), log_rtr_type(IPV4_PREFIX));
		rtr_send_error(rs, CORRUPT_DATA, NULL, buf, len);
		return -1;
	}

	memcpy(&ip4, buf + sizeof(struct rtr_header), sizeof(ip4));
	if (ip4.prefixlen > 32 || ip4.maxlen > 32 ||
	    ip4.prefixlen > ip4.maxlen) {
		log_warnx("rtr: %s: received %s: bad prefixlen / maxlen",
		    log_rtr(rs), log_rtr_type(IPV4_PREFIX));
		rtr_send_error(rs, CORRUPT_DATA, "bad prefixlen / maxlen",
		    buf, len);
		return -1;
	}

	if ((roa = calloc(1, sizeof(*roa))) == NULL) {
		log_warn("rtr %s: received %s",
		    log_rtr(rs), log_rtr_type(IPV4_PREFIX));
		rtr_send_error(rs, INTERNAL_ERROR, "out of memory", NULL, 0);
		return -1;
	}
	roa->aid = AID_INET;
	roa->prefixlen = ip4.prefixlen;
	roa->maxlen = ip4.maxlen;
	roa->asnum = ntohl(ip4.asnum);
	roa->prefix.inet.s_addr = ip4.prefix;

	if (ip4.flags & FLAG_ANNOUNCE) {
		if (RB_INSERT(roa_tree, &rs->roa_set, roa) != NULL) {
			log_warnx("rtr %s: received %s: duplicate announcement",
			    log_rtr(rs), log_rtr_type(IPV4_PREFIX));
			rtr_send_error(rs, DUP_REC_RECV, NULL, buf, len);
			free(roa);
			return -1;
		}
	} else {
		struct roa *r;

		r = RB_FIND(roa_tree, &rs->roa_set, roa);
		if (r == NULL) {
			log_warnx("rtr %s: received %s: unknown withdrawl",
			    log_rtr(rs), log_rtr_type(IPV4_PREFIX));
			rtr_send_error(rs, UNK_REC_WDRAWL, NULL, buf, len);
			free(roa);
			return -1;
		}
		RB_REMOVE(roa_tree, &rs->roa_set, r);
		free(r);
		free(roa);
	}

	return 0;
}

static int
rtr_parse_ipv6_prefix(struct rtr_session *rs, uint8_t *buf, size_t len)
{
	struct rtr_ipv6 ip6;
	struct roa *roa;

	if (len != sizeof(struct rtr_header) + sizeof(ip6)) {
		log_warnx("rtr %s: received %s: bad pdu len",
		    log_rtr(rs), log_rtr_type(IPV6_PREFIX));
		rtr_send_error(rs, CORRUPT_DATA, "bad len", buf, len);
		return -1;
	}

	if (rs->state != RTR_STATE_ACTIVE) {
		log_warnx("rtr %s: received %s: out of context",
		    log_rtr(rs), log_rtr_type(IPV6_PREFIX));
		rtr_send_error(rs, CORRUPT_DATA, NULL, buf, len);
		return -1;
	}

	memcpy(&ip6, buf + sizeof(struct rtr_header), sizeof(ip6));
	if (ip6.prefixlen > 128 || ip6.maxlen > 128 ||
	    ip6.prefixlen > ip6.maxlen) {
		log_warnx("rtr: %s: received %s: bad prefixlen / maxlen",
		    log_rtr(rs), log_rtr_type(IPV6_PREFIX));
		rtr_send_error(rs, CORRUPT_DATA, "bad prefixlen / maxlen",
		    buf, len);
		return -1;
	}

	if ((roa = calloc(1, sizeof(*roa))) == NULL) {
		log_warn("rtr %s: received %s",
		    log_rtr(rs), log_rtr_type(IPV6_PREFIX));
		rtr_send_error(rs, INTERNAL_ERROR, "out of memory", NULL, 0);
		return -1;
	}
	roa->aid = AID_INET6;
	roa->prefixlen = ip6.prefixlen;
	roa->maxlen = ip6.maxlen;
	roa->asnum = ntohl(ip6.asnum);
	memcpy(&roa->prefix.inet6, ip6.prefix, sizeof(roa->prefix.inet6));

	if (ip6.flags & FLAG_ANNOUNCE) {
		if (RB_INSERT(roa_tree, &rs->roa_set, roa) != NULL) {
			log_warnx("rtr %s: received %s: duplicate announcement",
			    log_rtr(rs), log_rtr_type(IPV6_PREFIX));
			rtr_send_error(rs, DUP_REC_RECV, NULL, buf, len);
			free(roa);
			return -1;
		}
	} else {
		struct roa *r;

		r = RB_FIND(roa_tree, &rs->roa_set, roa);
		if (r == NULL) {
			log_warnx("rtr %s: received %s: unknown withdrawl",
			    log_rtr(rs), log_rtr_type(IPV6_PREFIX));
			rtr_send_error(rs, UNK_REC_WDRAWL, NULL, buf, len);
			free(roa);
			return -1;
		}
		RB_REMOVE(roa_tree, &rs->roa_set, r);
		free(r);
		free(roa);
	}
	return 0;
}

static int
rtr_parse_end_of_data(struct rtr_session *rs, uint8_t *buf, size_t len)
{
	struct rtr_endofdata eod;
	uint32_t t;

	buf += sizeof(struct rtr_header);
	len -= sizeof(struct rtr_header);

	if (len != sizeof(eod)) {
		log_warnx("rtr %s: received %s: bad pdu len",
		    log_rtr(rs), log_rtr_type(END_OF_DATA));
		return -1;
	}

	memcpy(&eod, buf, sizeof(eod));

	if (rs->state != RTR_STATE_ACTIVE) {
		log_warnx("rtr %s: received %s: out of context",
		    log_rtr(rs), log_rtr_type(END_OF_DATA));
		return -1;
	}

	rs->serial = ntohl(eod.serial);
	/* validate timer values to be in the right range */
	t = ntohl(eod.refresh);
	if (t < 1 || t > 86400)
		goto bad;
	rs->refresh = t;
	t = ntohl(eod.retry);
	if (t < 1 || t > 7200)
		goto bad;
	rs->retry = t;
	t = ntohl(eod.expire);
	if (t < 600 || t > 172800)
		goto bad;
	if (t <= rs->retry || t <= rs->refresh)
		goto bad;
	rs->expire = t;

	rtr_fsm(rs, RTR_EVNT_END_OF_DATA);
	return 0;

bad:
	log_warnx("rtr %s: received %s: bad timeout values",
	    log_rtr(rs), log_rtr_type(END_OF_DATA));
	return -1;
}

static int
rtr_parse_cache_reset(struct rtr_session *rs, uint8_t *buf, size_t len)
{
	if (rs->state != RTR_STATE_IDLE) {
		log_warnx("rtr %s: received %s: out of context",
		    log_rtr(rs), log_rtr_type(CACHE_RESET));
		return -1;
	}

	rtr_fsm(rs, RTR_EVNT_CACHE_RESET);
	return 0;
}

/*
 * Parse an Error Response message. This function behaves a bit different
 * from other parse functions since on error the connection needs to be
 * dropped without sending an error response back.
 */
static int
rtr_parse_error(struct rtr_session *rs, uint8_t *buf, size_t len)
{
	struct rtr_header rh;
	uint32_t pdu_len, msg_len;
	uint8_t *msg;
	char *str = NULL;
	uint16_t errcode;

	memcpy(&rh, buf, sizeof(rh));
	buf += sizeof(struct rtr_header);
	len -= sizeof(struct rtr_header);
	errcode = ntohs(rh.session_id);

	memcpy(&pdu_len, buf, sizeof(pdu_len));
	pdu_len = ntohs(pdu_len);

	if (len < pdu_len + sizeof(pdu_len)) {
		log_warnx("rtr %s: received %s: bad pdu len: %u byte",
		    log_rtr(rs), log_rtr_type(ERROR_REPORT), pdu_len);
		rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
		return -1;
	}

	/* for now just ignore the embedded pdu */
	buf += pdu_len + sizeof(pdu_len);
	len -= pdu_len + sizeof(pdu_len);

	memcpy(&msg_len, buf, sizeof(msg_len));
	msg_len = ntohs(msg_len);

	if (len < msg_len + sizeof(msg_len)) {
		log_warnx("rtr %s: received %s: bad msg len: %u byte",
		    log_rtr(rs), log_rtr_type(ERROR_REPORT), msg_len);
		rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
		return -1;
	}

	msg = buf + sizeof(msg_len);
	if (msg_len != 0)
		/* optional error msg, no need to check for failure */
		str = strndup(msg, msg_len);

	log_warnx("rtr %s: received error: %s%s%s", log_rtr(rs),
	    log_rtr_error(errcode), str ? ": " : "", str ? str : "");

	if (errcode == NO_DATA_AVAILABLE) {
		rtr_fsm(rs, RTR_EVNT_NO_DATA);
	} else {
		rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
		rs->last_recv_error = errcode;
		if (str)
			strlcpy(rs->last_recv_msg, str,
			    sizeof(rs->last_recv_msg));
		else
			memset(rs->last_recv_msg, 0,
			    sizeof(rs->last_recv_msg));

		free(str);
		return -1;
	}
	free(str);

	return 0;
}

/*
 * Try to process received rtr message, it is possible that not a full
 * message is in the buffer. In that case stop, once new data is available
 * a retry will be done.
 */
static void
rtr_process_msg(struct rtr_session *rs)
{
	size_t rpos, av, left;
	void *rptr;
	size_t msglen;
	enum rtr_pdu_type msgtype;

	rpos = 0;
	av = rs->r.wpos;

	for (;;) {
		if (rpos + sizeof(struct rtr_header) > av)
			break;
		rptr = rs->r.buf + rpos;
		if (rtr_parse_header(rs, rptr, &msglen, &msgtype) == -1)
			return;

		/* missing data */
		if (rpos + msglen > av)
			break;

		switch (msgtype) {
		case SERIAL_NOTIFY:
			if (rtr_parse_notify(rs, rptr, msglen) == -1) {
				rtr_send_error(rs, CORRUPT_DATA, NULL,
				    rptr, msglen);
				return;
			}
			break;
		case CACHE_RESPONSE:
			if (rtr_parse_cache_response(rs, rptr, msglen) == -1) {
				rtr_send_error(rs, CORRUPT_DATA, NULL,
				    rptr, msglen);
				return;
			}
			break;
		case IPV4_PREFIX:
			if (rtr_parse_ipv4_prefix(rs, rptr, msglen) == -1) {
				return;
			}
			break;
		case IPV6_PREFIX:
			if (rtr_parse_ipv6_prefix(rs, rptr, msglen) == -1) {
				return;
			}
			break;
		case END_OF_DATA:
			if (rtr_parse_end_of_data(rs, rptr, msglen) == -1) {
				rtr_send_error(rs, CORRUPT_DATA, NULL,
				    rptr, msglen);
				return;
			}
			break;
		case CACHE_RESET:
			if (rtr_parse_cache_reset(rs, rptr, msglen) == -1) {
				rtr_send_error(rs, CORRUPT_DATA, NULL,
				    rptr, msglen);
				return;
			}
			break;
		case ROUTER_KEY:
			/* silently ignore router key */
			break;
		case ERROR_REPORT:
			if (rtr_parse_error(rs, rptr, msglen) == -1)
				/* no need to send back an error */
				return;
			break;
		default:
			log_warnx("rtr %s: received %s: unexpected pdu type",
			    log_rtr(rs), log_rtr_type(msgtype));
			rtr_send_error(rs, INVALID_REQUEST, NULL, rptr, msglen);
			return;
		}
		rpos += msglen;
	}

	left = av - rpos;
	memmove(&rs->r.buf, rs->r.buf + rpos, left);
	rs->r.wpos = left;
}

/*
 * Simple FSM for RTR sessions
 */
static void
rtr_fsm(struct rtr_session *rs, enum rtr_event event)
{
	enum rtr_state prev_state = rs->state;

	switch (event) {
	case RTR_EVNT_CON_CLOSED:
		if (rs->state != RTR_STATE_CLOSED) {
			/* flush buffers */
			msgbuf_clear(&rs->w);
			rs->r.wpos = 0;
			close(rs->fd);
			rs->fd = -1;
		}
		rs->state = RTR_STATE_CLOSED;
		/* try to reopen session */
		timer_set(&rs->timers, Timer_Rtr_Retry,
		    arc4random_uniform(10));
		break;
	case RTR_EVNT_START:
	case RTR_EVNT_TIMER_RETRY:
		switch (rs->state) {
		case RTR_STATE_ERROR:
			rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
			return;
		case RTR_STATE_CLOSED:
			timer_set(&rs->timers, Timer_Rtr_Retry, rs->retry);
			rtr_imsg_compose(IMSG_SOCKET_CONN, rs->id, 0, NULL, 0);
			return;
		default:
			break;
		}
		/* FALLTHROUGH */
	case RTR_EVNT_CON_OPEN:
		timer_stop(&rs->timers, Timer_Rtr_Retry);
		if (rs->session_id == -1)
			rtr_reset_query(rs);
		else
			rtr_serial_query(rs);
		break;
	case RTR_EVNT_SERIAL_NOTIFY:
		/* schedule a refresh after a quick wait */
		timer_set(&rs->timers, Timer_Rtr_Refresh,
		    arc4random_uniform(10));
		break;
	case RTR_EVNT_TIMER_REFRESH:
		/* send serial query */
		rtr_serial_query(rs);
		break;
	case RTR_EVNT_TIMER_EXPIRE:
		free_roatree(&rs->roa_set);
		rtr_recalc();
		break;
	case RTR_EVNT_CACHE_RESPONSE:
		rs->state = RTR_STATE_ACTIVE;
		timer_stop(&rs->timers, Timer_Rtr_Refresh);
		timer_stop(&rs->timers, Timer_Rtr_Retry);
		break;
	case RTR_EVNT_END_OF_DATA:
		/* start refresh and expire timers */
		timer_set(&rs->timers, Timer_Rtr_Refresh, rs->refresh);
		timer_set(&rs->timers, Timer_Rtr_Expire, rs->expire);
		rs->state = RTR_STATE_IDLE;
		rtr_recalc();
		break;
	case RTR_EVNT_CACHE_RESET:
		/* reset session and retry after a quick wait */
		rs->session_id = -1;
		free_roatree(&rs->roa_set);
		timer_set(&rs->timers, Timer_Rtr_Retry,
		    arc4random_uniform(10));
		break;
	case RTR_EVNT_NO_DATA:
		/* start retry timer */
		timer_set(&rs->timers, Timer_Rtr_Retry, rs->retry);
		/* stop refresh timer just to be sure */
		timer_stop(&rs->timers, Timer_Rtr_Refresh);
		break;
	case RTR_EVNT_SEND_ERROR:
		rs->state = RTR_STATE_ERROR;
		/* flush receive buffer */
		rs->r.wpos = 0;
		break;
	}

	log_info("rtr %s: state change %s -> %s, reason: %s",
	    log_rtr(rs), rtr_statenames[prev_state], rtr_statenames[rs->state],
	    rtr_eventnames[event]);
}

/*
 * IO handler for RTR sessions
 */
static void
rtr_dispatch_msg(struct pollfd *pfd, struct rtr_session *rs)
{
	ssize_t n;
	int error;

	if (pfd->revents & POLLHUP) {
		log_warnx("rtr %s: Connection closed, hangup", log_rtr(rs));
		rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
		return;
	}
	if (pfd->revents & (POLLERR|POLLNVAL)) {
		log_warnx("rtr %s: Connection closed, error", log_rtr(rs));
		rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
		return;
	}
	if (pfd->revents & POLLOUT && rs->w.queued) {
		if ((error = ibuf_write(&rs->w)) <= 0 && errno != EAGAIN) {
			if (error == 0)
				log_warnx("rtr %s: Connection closed",
				    log_rtr(rs));
			else if (error == -1)
				log_warn("rtr %s: write error", log_rtr(rs));
			rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
			return;
		}
		if (rs->w.queued == 0 && rs->state == RTR_STATE_ERROR)
			rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
	}
	if (pfd->revents & POLLIN) {
		if ((n = read(rs->fd, rs->r.buf + rs->r.wpos,
		    sizeof(rs->r.buf) - rs->r.wpos)) == -1) {
			if (errno != EINTR && errno != EAGAIN) {
				log_warn("rtr %s: read error", log_rtr(rs));
				rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
			}
			return;
		}
		if (n == 0) {
			log_warnx("rtr %s: Connection closed", log_rtr(rs));
			rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
			return;
		}
		rs->r.wpos += n;

		/* new data arrived, try to process it */
		rtr_process_msg(rs);
	}

}

void
rtr_check_events(struct pollfd *pfds, size_t npfds)
{
	struct rtr_session *rs;
	struct timer *t;
	time_t now;
	size_t i = 0;

	for (i = 0; i < npfds; i++) {
		if (pfds[i].revents == 0)
			continue;
		TAILQ_FOREACH(rs, &rtrs, entry)
			if (rs->fd == pfds[i].fd) {
				rtr_dispatch_msg(&pfds[i], rs);
				break;
			}
		if (rs == NULL)
			log_warnx("%s: unknown fd in pollfds", __func__);
	}

	/* run all timers */
	now = getmonotime();
	TAILQ_FOREACH(rs, &rtrs, entry)
		if ((t = timer_nextisdue(&rs->timers, now)) != NULL) {
			log_debug("rtr %s: %s triggered", log_rtr(rs),
			    timernames[t->type]);
			/* stop timer so it does not trigger again */
			timer_stop(&rs->timers, t->type);
			switch (t->type) {
			case Timer_Rtr_Refresh:
				rtr_fsm(rs, RTR_EVNT_TIMER_REFRESH);
				break;
			case Timer_Rtr_Retry:
				rtr_fsm(rs, RTR_EVNT_TIMER_RETRY);
				break;
			case Timer_Rtr_Expire:
				rtr_fsm(rs, RTR_EVNT_TIMER_EXPIRE);
				break;
			default:
				fatalx("King Bula lost in time");
			}
		}
}

size_t
rtr_count(void)
{
	struct rtr_session *rs;
	size_t count = 0;

	TAILQ_FOREACH(rs, &rtrs, entry)
		count++;
	return count;
}

size_t
rtr_poll_events(struct pollfd *pfds, size_t npfds, time_t *timeout)
{
	struct rtr_session *rs;
	time_t now = getmonotime();
	size_t i = 0;

	TAILQ_FOREACH(rs, &rtrs, entry) {
		time_t nextaction;
		struct pollfd *pfd = pfds + i++;

		if (i > npfds)
			fatalx("%s: too many sessions for pollfd", __func__);

		if ((nextaction = timer_nextduein(&rs->timers, now)) != -1 &&
		    nextaction < *timeout)
			*timeout = nextaction;

		if (rs->state == RTR_STATE_CLOSED) {
			pfd->fd = -1;
			continue;
		}

		pfd->fd = rs->fd;
		pfd->events = 0;

		if (rs->w.queued)
			pfd->events |= POLLOUT;
		if (rs->state >= RTR_STATE_IDLE)
			pfd->events |= POLLIN;
	}

	return i;
}

struct rtr_session *
rtr_new(uint32_t id, char *descr)
{
	struct rtr_session *rs;

	if ((rs = calloc(1, sizeof(*rs))) == NULL)
		fatal("RTR session %s", descr);

	RB_INIT(&rs->roa_set);
	TAILQ_INIT(&rs->timers);
	msgbuf_init(&rs->w);

	strlcpy(rs->descr, descr, sizeof(rs->descr));
	rs->id = id;
	rs->session_id = -1;
	rs->refresh = RTR_DEFAULT_REFRESH;
	rs->retry = RTR_DEFAULT_RETRY;
	rs->expire = RTR_DEFAULT_EXPIRE;
	rs->state = RTR_STATE_CLOSED;
	rs->reconf_action = RECONF_REINIT;
	rs->last_recv_error = NO_ERROR;
	rs->last_sent_error = NO_ERROR;

	/* make sure that some timer is running to abort bad sessions */
	timer_set(&rs->timers, Timer_Rtr_Expire, rs->expire);

	log_debug("rtr %s: new session, start", log_rtr(rs));
	TAILQ_INSERT_TAIL(&rtrs, rs, entry);
	rtr_fsm(rs, RTR_EVNT_START);

	return rs;
}

struct rtr_session *
rtr_get(uint32_t id)
{
	struct rtr_session *rs;

	TAILQ_FOREACH(rs, &rtrs, entry)
		if (rs->id == id)
			return rs;
	return NULL;
}

void
rtr_free(struct rtr_session *rs)
{
	if (rs == NULL)
		return;

	rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
	timer_remove_all(&rs->timers);
	free_roatree(&rs->roa_set);
	free(rs);
}

void
rtr_open(struct rtr_session *rs, int fd)
{
	if (rs->state != RTR_STATE_CLOSED) {
		log_warnx("rtr %s: bad session state", log_rtr(rs));
		rtr_fsm(rs, RTR_EVNT_CON_CLOSED);
	}

	log_debug("rtr %s: connection opened", log_rtr(rs));

	rs->fd = rs->w.fd = fd;
	rs->state = RTR_STATE_IDLE;
	rtr_fsm(rs, RTR_EVNT_CON_OPEN);
}

void
rtr_config_prep(void)
{
	struct rtr_session *rs;

	TAILQ_FOREACH(rs, &rtrs, entry)
		rs->reconf_action = RECONF_DELETE;
}

void
rtr_config_merge(void)
{
	struct rtr_session *rs, *nrs;

	TAILQ_FOREACH_SAFE(rs, &rtrs, entry, nrs)
		if (rs->reconf_action == RECONF_DELETE) {
			TAILQ_REMOVE(&rtrs, rs, entry);
			rtr_free(rs);
		}
}

void
rtr_config_keep(struct rtr_session *rs)
{
	rs->reconf_action = RECONF_KEEP;
}

void
rtr_roa_merge(struct roa_tree *rt)
{
	struct rtr_session *rs;
	struct roa *roa;

	TAILQ_FOREACH(rs, &rtrs, entry) {
		RB_FOREACH(roa, roa_tree, &rs->roa_set)
			roa_insert(rt, roa);
	}
}

void
rtr_shutdown(void)
{
	struct rtr_session *rs, *nrs;

	TAILQ_FOREACH_SAFE(rs, &rtrs, entry, nrs)
		rtr_free(rs);
}

void
rtr_show(struct rtr_session *rs, pid_t pid)
{
	struct ctl_show_rtr msg;
	struct ctl_timer ct;
	u_int i;
	time_t d;

	memset(&msg, 0, sizeof(msg));

	/* descr, remote_addr, local_addr and remote_port set by parent */
	msg.serial = rs->serial;
	msg.refresh = rs->refresh;
	msg.retry = rs->retry;
	msg.expire = rs->expire;
	msg.session_id = rs->session_id;
	msg.last_sent_error = rs->last_sent_error;
	msg.last_recv_error = rs->last_recv_error;
	strlcpy(msg.last_sent_msg, rs->last_sent_msg,
	    sizeof(msg.last_sent_msg));
	strlcpy(msg.last_recv_msg, rs->last_recv_msg,
	    sizeof(msg.last_recv_msg));

	/* send back imsg */
	rtr_imsg_compose(IMSG_CTL_SHOW_RTR, rs->id, pid, &msg, sizeof(msg));

	/* send back timer imsgs */
	for (i = 1; i < Timer_Max; i++) {
		if (!timer_running(&rs->timers, i, &d))
			continue;
		ct.type = i;
		ct.val = d;
		rtr_imsg_compose(IMSG_CTL_SHOW_TIMER, rs->id, pid,
		    &ct, sizeof(ct));
	}
}
