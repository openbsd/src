/*	$OpenBSD: snmpe.c,v 1.58 2019/05/13 07:24:50 martijn Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2017 Marco Pfatschbacher <mpf@openbsd.org>
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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "snmpd.h"
#include "mib.h"

void	 snmpe_init(struct privsep *, struct privsep_proc *, void *);
int	 snmpe_parse(struct snmp_message *);
void	 snmpe_tryparse(int, struct snmp_message *);
int	 snmpe_parsevarbinds(struct snmp_message *);
void	 snmpe_response(struct snmp_message *);
void	 snmpe_sig_handler(int sig, short, void *);
int	 snmpe_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 snmpe_bind(struct address *);
void	 snmpe_recvmsg(int fd, short, void *);
void	 snmpe_readcb(int fd, short, void *);
void	 snmpe_writecb(int fd, short, void *);
void	 snmpe_acceptcb(int fd, short, void *);
void	 snmpe_prepare_read(struct snmp_message *, int);
int	 snmpe_encode(struct snmp_message *);
void	 snmp_msgfree(struct snmp_message *);

struct imsgev	*iev_parent;
static const struct timeval	snmpe_tcp_timeout = { 10, 0 }; /* 10s */

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	snmpe_dispatch_parent }
};

void
snmpe(struct privsep *ps, struct privsep_proc *p)
{
	struct snmpd		*env = ps->ps_env;
	struct address		*h;
	struct listen_sock	*so;
#ifdef DEBUG
	char		 buf[BUFSIZ];
	struct oid	*oid;
#endif

#ifdef DEBUG
	for (oid = NULL; (oid = smi_foreach(oid, 0)) != NULL;) {
		smi_oid2string(&oid->o_id, buf, sizeof(buf), 0);
		log_debug("oid %s", buf);
	}
#endif

	/* bind SNMP UDP/TCP sockets */
	TAILQ_FOREACH(h, &env->sc_addresses, entry) {
		if ((so = calloc(1, sizeof(*so))) == NULL)
			fatal("snmpe: %s", __func__);
		if ((so->s_fd = snmpe_bind(h)) == -1)
			fatal("snmpe: failed to bind SNMP socket");
		so->s_ipproto = h->ipproto;
		TAILQ_INSERT_TAIL(&env->sc_sockets, so, entry);
	}

	proc_run(ps, p, procs, nitems(procs), snmpe_init, NULL);
}

/* ARGSUSED */
void
snmpe_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct snmpd		*env = ps->ps_env;
	struct listen_sock	*so;

	kr_init();
	trap_init();
	timer_init();
	usm_generate_keys();

	/* listen for incoming SNMP UDP/TCP messages */
	TAILQ_FOREACH(so, &env->sc_sockets, entry) {
		if (so->s_ipproto == IPPROTO_TCP) {
			if (listen(so->s_fd, 5) < 0)
				fatalx("snmpe: failed to listen on socket");
			event_set(&so->s_ev, so->s_fd, EV_READ, snmpe_acceptcb, so);
			evtimer_set(&so->s_evt, snmpe_acceptcb, so);
		} else {
			event_set(&so->s_ev, so->s_fd, EV_READ|EV_PERSIST,
			    snmpe_recvmsg, env);
		}
		event_add(&so->s_ev, NULL);
	}

	if (unveil("/", "") == -1)
		fatal("unveil");
	if (unveil(NULL, NULL) == -1)
		fatal("unveil");
}

void
snmpe_shutdown(void)
{
	struct listen_sock *so;

	TAILQ_FOREACH(so, &snmpd_env->sc_sockets, entry) {
		event_del(&so->s_ev);
		if (so->s_ipproto == IPPROTO_TCP)
			event_del(&so->s_evt);
		close(so->s_fd);
	}
	kr_shutdown();
}

int
snmpe_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	default:
		break;
	}

	return (-1);
}

int
snmpe_bind(struct address *addr)
{
	char	 buf[512];
	int	 val, s;

	if ((s = snmpd_socket_af(&addr->ss, htons(addr->port),
	    addr->ipproto)) == -1)
		return (-1);

	/*
	 * Socket options
	 */
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;

	if (addr->ipproto == IPPROTO_TCP) {
		val = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
		    &val, sizeof(val)) == -1)
			fatal("setsockopt SO_REUSEADDR");
	} else { /* UDP */
		switch (addr->ss.ss_family) {
		case AF_INET:
			val = 1;
			if (setsockopt(s, IPPROTO_IP, IP_RECVDSTADDR,
			    &val, sizeof(int)) == -1) {
				log_warn("%s: failed to set IPv4 packet info",
				    __func__);
				goto bad;
			}
			break;
		case AF_INET6:
			val = 1;
			if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO,
			    &val, sizeof(int)) == -1) {
				log_warn("%s: failed to set IPv6 packet info",
				    __func__);
				goto bad;
			}
		}
	}

	if (bind(s, (struct sockaddr *)&addr->ss, addr->ss.ss_len) == -1)
		goto bad;

	if (print_host(&addr->ss, buf, sizeof(buf)) == NULL)
		goto bad;

	log_info("snmpe: listening on %s %s:%d",
	    (addr->ipproto == IPPROTO_TCP) ? "tcp" : "udp", buf, addr->port);

	return (s);

 bad:
	close(s);
	return (-1);
}

int
snmpe_parse(struct snmp_message *msg)
{
	struct snmpd		*env = snmpd_env;
	struct snmp_stats	*stats = &env->sc_stats;
	struct ber_element	*a;
	long long		 ver, req;
	long long		 errval, erridx;
	unsigned int		 type;
	u_int			 class;
	char			*comn;
	char			*flagstr, *ctxname;
	size_t			 len;
	struct sockaddr_storage *ss = &msg->sm_ss;
	struct ber_element	*root = msg->sm_req;

	msg->sm_errstr = "invalid message";

	if (ber_scanf_elements(root, "{ie", &ver, &a) != 0)
		goto parsefail;

	/* SNMP version and community */
	msg->sm_version = ver;
	switch (msg->sm_version) {
	case SNMP_V1:
	case SNMP_V2:
		if (env->sc_min_seclevel != 0)
			goto badversion;
		if (ber_scanf_elements(a, "se", &comn, &msg->sm_pdu) != 0)
			goto parsefail;
		if (strlcpy(msg->sm_community, comn,
		    sizeof(msg->sm_community)) >= sizeof(msg->sm_community)) {
			stats->snmp_inbadcommunitynames++;
			msg->sm_errstr = "community name too long";
			goto fail;
		}
		break;
	case SNMP_V3:
		if (ber_scanf_elements(a, "{iisi}e",
		    &msg->sm_msgid, &msg->sm_max_msg_size, &flagstr,
		    &msg->sm_secmodel, &a) != 0)
			goto parsefail;

		msg->sm_flags = *flagstr;
		if ((a = usm_decode(msg, a, &msg->sm_errstr)) == NULL)
			goto parsefail;

		if (MSG_SECLEVEL(msg) < env->sc_min_seclevel ||
		    msg->sm_secmodel != SNMP_SEC_USM) {
			/* XXX currently only USM supported */
			msg->sm_errstr = "unsupported security model";
			stats->snmp_usmbadseclevel++;
			msg->sm_usmerr = OIDVAL_usmErrSecLevel;
			goto parsefail;
		}

		if (ber_scanf_elements(a, "{xxe",
		    &msg->sm_ctxengineid, &msg->sm_ctxengineid_len,
		    &ctxname, &len, &msg->sm_pdu) != 0)
			goto parsefail;
		if (len > SNMPD_MAXCONTEXNAMELEN)
			goto parsefail;
		memcpy(msg->sm_ctxname, ctxname, len);
		msg->sm_ctxname[len] = '\0';
		break;
	default:
	badversion:
		stats->snmp_inbadversions++;
		msg->sm_errstr = "bad snmp version";
		goto fail;
	}

	if (ber_scanf_elements(msg->sm_pdu, "t{e", &class, &type, &a) != 0)
		goto parsefail;

	/* SNMP PDU context */
	if (class != BER_CLASS_CONTEXT)
		goto parsefail;

	switch (type) {
	case SNMP_C_GETBULKREQ:
		if (msg->sm_version == SNMP_V1) {
			stats->snmp_inbadversions++;
			msg->sm_errstr =
			    "invalid request for protocol version 1";
			goto fail;
		}
		/* FALLTHROUGH */

	case SNMP_C_GETREQ:
		stats->snmp_ingetrequests++;
		/* FALLTHROUGH */

	case SNMP_C_GETNEXTREQ:
		if (type == SNMP_C_GETNEXTREQ)
			stats->snmp_ingetnexts++;
		if (msg->sm_version != SNMP_V3 &&
		    strcmp(env->sc_rdcommunity, msg->sm_community) != 0 &&
		    strcmp(env->sc_rwcommunity, msg->sm_community) != 0) {
			stats->snmp_inbadcommunitynames++;
			msg->sm_errstr = "wrong read community";
			goto fail;
		}
		msg->sm_context = type;
		break;

	case SNMP_C_SETREQ:
		stats->snmp_insetrequests++;
		if (msg->sm_version != SNMP_V3 &&
		    strcmp(env->sc_rwcommunity, msg->sm_community) != 0) {
			if (strcmp(env->sc_rdcommunity, msg->sm_community) != 0)
				stats->snmp_inbadcommunitynames++;
			else
				stats->snmp_inbadcommunityuses++;
			msg->sm_errstr = "wrong write community";
			goto fail;
		}
		msg->sm_context = type;
		break;

	case SNMP_C_GETRESP:
		stats->snmp_ingetresponses++;
		msg->sm_errstr = "response without request";
		goto parsefail;

	case SNMP_C_TRAP:
	case SNMP_C_TRAPV2:
		if (msg->sm_version != SNMP_V3 &&
		    strcmp(env->sc_trcommunity, msg->sm_community) != 0) {
			stats->snmp_inbadcommunitynames++;
			msg->sm_errstr = "wrong trap community";
			goto fail;
		}
		stats->snmp_intraps++;
		msg->sm_errstr = "received trap";
		goto fail;

	default:
		msg->sm_errstr = "invalid context";
		goto parsefail;
	}

	/* SNMP PDU */
	if (ber_scanf_elements(a, "iiie{et",
	    &req, &errval, &erridx, &msg->sm_pduend,
	    &msg->sm_varbind, &class, &type) != 0) {
		stats->snmp_silentdrops++;
		msg->sm_errstr = "invalid PDU";
		goto fail;
	}
	if (class != BER_CLASS_UNIVERSAL || type != BER_TYPE_SEQUENCE) {
		stats->snmp_silentdrops++;
		msg->sm_errstr = "invalid varbind";
		goto fail;
	}

	msg->sm_request = req;
	msg->sm_error = errval;
	msg->sm_errorindex = erridx;

	print_host(ss, msg->sm_host, sizeof(msg->sm_host));
	if (msg->sm_version == SNMP_V3)
		log_debug("%s: %s: SNMPv3 context %d, flags %#x, "
		    "secmodel %lld, user '%s', ctx-engine %s, ctx-name '%s', "
		    "request %lld", __func__, msg->sm_host, msg->sm_context,
		    msg->sm_flags, msg->sm_secmodel, msg->sm_username,
		    tohexstr(msg->sm_ctxengineid, msg->sm_ctxengineid_len),
		    msg->sm_ctxname, msg->sm_request);
	else
		log_debug("%s: %s: SNMPv%d '%s' context %d request %lld",
		    __func__, msg->sm_host, msg->sm_version + 1,
		    msg->sm_community, msg->sm_context, msg->sm_request);

	return (0);

 parsefail:
	stats->snmp_inasnparseerrs++;
 fail:
	print_host(ss, msg->sm_host, sizeof(msg->sm_host));
	log_debug("%s: %s: %s", __func__, msg->sm_host, msg->sm_errstr);
	return (-1);
}

int
snmpe_parsevarbinds(struct snmp_message *msg)
{
	struct snmp_stats	*stats = &snmpd_env->sc_stats;
	char			 buf[BUFSIZ];
	struct ber_oid		 o;
	int			 ret = 0;

	msg->sm_errstr = "invalid varbind element";

	if (msg->sm_i == 0) {
		msg->sm_i = 1;
		msg->sm_a = msg->sm_varbind;
		msg->sm_last = NULL;
	}

	 for (; msg->sm_a != NULL && msg->sm_i < SNMPD_MAXVARBIND;
	    msg->sm_a = msg->sm_next, msg->sm_i++) {
		msg->sm_next = msg->sm_a->be_next;

		if (msg->sm_a->be_class != BER_CLASS_UNIVERSAL ||
		    msg->sm_a->be_type != BER_TYPE_SEQUENCE)
			continue;
		if ((msg->sm_b = msg->sm_a->be_sub) == NULL)
			continue;

		for (msg->sm_state = 0; msg->sm_state < 2 && msg->sm_b != NULL;
		    msg->sm_b = msg->sm_b->be_next) {
			switch (msg->sm_state++) {
			case 0:
				if (ber_get_oid(msg->sm_b, &o) != 0)
					goto varfail;
				if (o.bo_n < BER_MIN_OID_LEN ||
				    o.bo_n > BER_MAX_OID_LEN)
					goto varfail;
				if (msg->sm_context == SNMP_C_SETREQ)
					stats->snmp_intotalsetvars++;
				else
					stats->snmp_intotalreqvars++;
				log_debug("%s: %s: oid %s",
				    __func__, msg->sm_host,
				    smi_oid2string(&o, buf, sizeof(buf), 0));
				break;
			case 1:
				msg->sm_c = NULL;
				msg->sm_end = NULL;

				switch (msg->sm_context) {

				case SNMP_C_GETNEXTREQ:
					msg->sm_c = ber_add_sequence(NULL);
					ret = mps_getnextreq(msg, msg->sm_c,
					    &o);
					if (ret == 0 || ret == 1)
						break;
					ber_free_elements(msg->sm_c);
					msg->sm_error = SNMP_ERROR_NOSUCHNAME;
					goto varfail;

				case SNMP_C_GETREQ:
					msg->sm_c = ber_add_sequence(NULL);
					ret = mps_getreq(msg, msg->sm_c, &o,
					    msg->sm_version);
					if (ret == 0 || ret == 1)
						break;
					msg->sm_error = SNMP_ERROR_NOSUCHNAME;
					ber_free_elements(msg->sm_c);
					goto varfail;

				case SNMP_C_SETREQ:
					if (snmpd_env->sc_readonly == 0) {
						ret = mps_setreq(msg,
						    msg->sm_b, &o);
						if (ret == 0)
							break;
					}
					msg->sm_error = SNMP_ERROR_READONLY;
					goto varfail;

				case SNMP_C_GETBULKREQ:
					ret = mps_getbulkreq(msg, &msg->sm_c,
					    &msg->sm_end, &o,
					    (msg->sm_i <= msg->sm_nonrepeaters)
					    ? 1 : msg->sm_maxrepetitions);
					if (ret == 0 || ret == 1)
						break;
					msg->sm_error = SNMP_ERROR_NOSUCHNAME;
					goto varfail;

				default:
					goto varfail;
				}
				if (msg->sm_c == NULL)
					break;
				if (msg->sm_end == NULL)
					msg->sm_end = msg->sm_c;
				if (msg->sm_last == NULL)
					msg->sm_varbindresp = msg->sm_c;
				else
					ber_link_elements(msg->sm_last, msg->sm_c);
				msg->sm_last = msg->sm_end;
				break;
			}
		}
		if (msg->sm_state < 2)  {
			log_debug("%s: state %d", __func__, msg->sm_state);
			goto varfail;
		}
	}

	msg->sm_errstr = "none";
	msg->sm_error = 0;
	msg->sm_errorindex = 0;

	return (ret);
 varfail:
	log_debug("%s: %s: %s, error index %d", __func__,
	    msg->sm_host, msg->sm_errstr, msg->sm_i);
	if (msg->sm_error == 0)
		msg->sm_error = SNMP_ERROR_GENERR;
	msg->sm_errorindex = msg->sm_i;
	return (-1);
}

void
snmpe_acceptcb(int fd, short type, void *arg)
{
	struct listen_sock	*so = arg;
	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	struct snmp_message	*msg;
	int afd;

	event_add(&so->s_ev, NULL);
	if ((type & EV_TIMEOUT))
		return;

	if ((afd = accept4(fd, (struct sockaddr *)&ss, &len,
	    SOCK_NONBLOCK|SOCK_CLOEXEC)) < 0) {
		/* Pause accept if we are out of file descriptors  */
		if (errno == ENFILE || errno == EMFILE) {
			struct timeval evtpause = { 1, 0 };

			event_del(&so->s_ev);
			evtimer_add(&so->s_evt, &evtpause);
		} else if (errno != EAGAIN && errno != EINTR)
			log_debug("%s: accept4", __func__);
		return;
	}
	if ((msg = calloc(1, sizeof(*msg))) == NULL)
		goto fail;

	snmpe_prepare_read(msg, afd);
	return;
fail:
	free(msg);
	close(afd);
	return;
}

void
snmpe_prepare_read(struct snmp_message *msg, int fd)
{
	msg->sm_sock = fd;
	msg->sm_sock_tcp = 1;
	event_del(&msg->sm_sockev);
	event_set(&msg->sm_sockev, fd, EV_READ,
	    snmpe_readcb, msg);
	event_add(&msg->sm_sockev, &snmpe_tcp_timeout);
}

void
snmpe_tryparse(int fd, struct snmp_message *msg)
{
	struct snmp_stats	*stats = &snmpd_env->sc_stats;

	ber_set_application(&msg->sm_ber, smi_application);
	ber_set_readbuf(&msg->sm_ber, msg->sm_data, msg->sm_datalen);
	msg->sm_req = ber_read_elements(&msg->sm_ber, NULL);
	if (msg->sm_req == NULL) {
		if (errno == ECANCELED) {
			/* short read; try again */
			snmpe_prepare_read(msg, fd);
			return;
		}
		goto fail;
	}

	if (snmpe_parse(msg) == -1) {
		if (msg->sm_usmerr && MSG_REPORT(msg)) {
			usm_make_report(msg);
			snmpe_response(msg);
			return;
		} else
			goto fail;
	}
	stats->snmp_inpkts++;

	snmpe_dispatchmsg(msg);
	return;
 fail:
	snmp_msgfree(msg);
	close(fd);
}

void
snmpe_readcb(int fd, short type, void *arg)
{
	struct snmp_message *msg = arg;
	ssize_t len;

	if (type == EV_TIMEOUT || msg->sm_datalen >= sizeof(msg->sm_data))
		goto fail;

	len = read(fd, msg->sm_data + msg->sm_datalen,
	    sizeof(msg->sm_data) - msg->sm_datalen);
	if (len <= 0) {
		if (errno != EAGAIN && errno != EINTR)
			goto fail;
		snmpe_prepare_read(msg, fd);
		return;
	}

	msg->sm_datalen += (size_t)len;
	snmpe_tryparse(fd, msg);
	return;

 fail:
	snmp_msgfree(msg);
	close(fd);
}

void
snmpe_writecb(int fd, short type, void *arg)
{
	struct snmp_stats	*stats = &snmpd_env->sc_stats;
	struct snmp_message	*msg = arg;
	struct snmp_message	*nmsg;
	ssize_t			 len;
	size_t			 reqlen;
	struct ber		*ber = &msg->sm_ber;

	if (type == EV_TIMEOUT)
		goto fail;

	len = ber->br_wend - ber->br_wbuf;
	ber->br_wptr = ber->br_wbuf;

	log_debug("%s: write fd %d len %zd", __func__, fd, len);

	len = write(fd, ber->br_wptr, len);
	if (len == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		else
			goto fail;
	}

	ber->br_wptr += len;

	if (ber->br_wptr < ber->br_wend) {
		event_del(&msg->sm_sockev);
		event_set(&msg->sm_sockev, msg->sm_sock, EV_WRITE,
		    snmpe_writecb, msg);
		event_add(&msg->sm_sockev, &snmpe_tcp_timeout);
		return;
	}

	stats->snmp_outpkts++;

	if ((nmsg = calloc(1, sizeof(*nmsg))) == NULL)
		goto fail;

	/*
	 * Reuse the connection.
	 * In case we already read data of the next message, copy it over.
	 */
	reqlen = ber_calc_len(msg->sm_req);
	if (msg->sm_datalen > reqlen) {
		memcpy(nmsg->sm_data, msg->sm_data + reqlen,
		    msg->sm_datalen - reqlen);
		nmsg->sm_datalen = msg->sm_datalen - reqlen;
		snmp_msgfree(msg);
		snmpe_prepare_read(nmsg, fd);
		snmpe_tryparse(fd, nmsg);
	} else {
		snmp_msgfree(msg);
		snmpe_prepare_read(nmsg, fd);
	}
	return;

 fail:
	close(fd);
	snmp_msgfree(msg);
}

void
snmpe_recvmsg(int fd, short sig, void *arg)
{
	struct snmpd		*env = arg;
	struct snmp_stats	*stats = &env->sc_stats;
	ssize_t			 len;
	struct snmp_message	*msg;

	if ((msg = calloc(1, sizeof(*msg))) == NULL)
		return;

	msg->sm_sock = fd;
	msg->sm_slen = sizeof(msg->sm_ss);
	if ((len = recvfromto(fd, msg->sm_data, sizeof(msg->sm_data), 0,
	    (struct sockaddr *)&msg->sm_ss, &msg->sm_slen,
	    (struct sockaddr *)&msg->sm_local_ss, &msg->sm_local_slen)) < 1) {
		free(msg);
		return;
	}

	stats->snmp_inpkts++;
	msg->sm_datalen = (size_t)len;

	bzero(&msg->sm_ber, sizeof(msg->sm_ber));
	ber_set_application(&msg->sm_ber, smi_application);
	ber_set_readbuf(&msg->sm_ber, msg->sm_data, msg->sm_datalen);

	msg->sm_req = ber_read_elements(&msg->sm_ber, NULL);
	if (msg->sm_req == NULL) {
		stats->snmp_inasnparseerrs++;
		snmp_msgfree(msg);
		return;
	}

#ifdef DEBUG
	fprintf(stderr, "recv msg:\n");
	smi_debug_elements(msg->sm_req);
#endif

	if (snmpe_parse(msg) == -1) {
		if (msg->sm_usmerr != 0 && MSG_REPORT(msg)) {
			usm_make_report(msg);
			snmpe_response(msg);
			return;
		} else {
			snmp_msgfree(msg);
			return;
		}
	}

	snmpe_dispatchmsg(msg);
}

void
snmpe_dispatchmsg(struct snmp_message *msg)
{
	/* dispatched to subagent */
	if (snmpe_parsevarbinds(msg) == 1)
		return;

	/* respond directly */
	msg->sm_context = SNMP_C_GETRESP;
	snmpe_response(msg);
}

void
snmpe_response(struct snmp_message *msg)
{
	struct snmp_stats	*stats = &snmpd_env->sc_stats;
	u_int8_t		*ptr = NULL;
	ssize_t			 len;

	if (msg->sm_varbindresp == NULL && msg->sm_pduend != NULL)
		msg->sm_varbindresp = ber_unlink_elements(msg->sm_pduend);

	switch (msg->sm_error) {
	case SNMP_ERROR_TOOBIG:
		stats->snmp_intoobigs++;
		break;
	case SNMP_ERROR_NOSUCHNAME:
		stats->snmp_innosuchnames++;
		break;
	case SNMP_ERROR_BADVALUE:
		stats->snmp_inbadvalues++;
		break;
	case SNMP_ERROR_READONLY:
		stats->snmp_inreadonlys++;
		break;
	case SNMP_ERROR_GENERR:
	default:
		stats->snmp_ingenerrs++;
		break;
	}

	/* Create new SNMP packet */
	if (snmpe_encode(msg) < 0)
		goto done;

	len = ber_write_elements(&msg->sm_ber, msg->sm_resp);
	if (ber_get_writebuf(&msg->sm_ber, (void *)&ptr) == -1)
		goto done;

	usm_finalize_digest(msg, ptr, len);
	if (msg->sm_sock_tcp) {
		event_del(&msg->sm_sockev);
		event_set(&msg->sm_sockev, msg->sm_sock, EV_WRITE,
		    snmpe_writecb, msg);
		event_add(&msg->sm_sockev, &snmpe_tcp_timeout);
		return;
	} else {
		len = sendtofrom(msg->sm_sock, ptr, len, 0,
		    (struct sockaddr *)&msg->sm_ss, msg->sm_slen,
		    (struct sockaddr *)&msg->sm_local_ss, msg->sm_local_slen);
		if (len != -1)
			stats->snmp_outpkts++;
	}

 done:
	snmp_msgfree(msg);
}

void
snmp_msgfree(struct snmp_message *msg)
{
	event_del(&msg->sm_sockev);
	ber_free(&msg->sm_ber);
	if (msg->sm_req != NULL)
		ber_free_elements(msg->sm_req);
	if (msg->sm_resp != NULL)
		ber_free_elements(msg->sm_resp);
	free(msg);
}

int
snmpe_encode(struct snmp_message *msg)
{
	struct ber_element	*ehdr;
	struct ber_element	*pdu, *epdu;

	msg->sm_resp = ber_add_sequence(NULL);
	if ((ehdr = ber_add_integer(msg->sm_resp, msg->sm_version)) == NULL)
		return -1;
	if (msg->sm_version == SNMP_V3) {
		char	f = MSG_SECLEVEL(msg);

		if ((ehdr = ber_printf_elements(ehdr, "{iixi}", msg->sm_msgid,
		    msg->sm_max_msg_size, &f, sizeof(f),
		    msg->sm_secmodel)) == NULL)
			return -1;

		/* XXX currently only USM supported */
		if ((ehdr = usm_encode(msg, ehdr)) == NULL)
			return -1;
	} else {
		if ((ehdr = ber_add_string(ehdr, msg->sm_community)) == NULL)
			return -1;
	}

	pdu = epdu = ber_add_sequence(NULL);
	if (msg->sm_version == SNMP_V3) {
		if ((epdu = ber_printf_elements(epdu, "xs{",
		    snmpd_env->sc_engineid, snmpd_env->sc_engineid_len,
		    msg->sm_ctxname)) == NULL) {
			ber_free_elements(pdu);
			return -1;
		}
	}

	if (!ber_printf_elements(epdu, "tiii{e}", BER_CLASS_CONTEXT,
	    msg->sm_context, msg->sm_request,
	    msg->sm_error, msg->sm_errorindex,
	    msg->sm_varbindresp)) {
		ber_free_elements(pdu);
		return -1;
	}

	if (MSG_HAS_PRIV(msg))
		pdu = usm_encrypt(msg, pdu);
	ber_link_elements(ehdr, pdu);

#ifdef DEBUG
	fprintf(stderr, "resp msg:\n");
	smi_debug_elements(msg->sm_resp);
#endif
	return 0;
}
