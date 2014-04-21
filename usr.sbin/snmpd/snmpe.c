/*	$OpenBSD: snmpe.c,v 1.38 2014/04/21 19:47:27 reyk Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/param.h>
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
int	 snmpe_parse(struct sockaddr_storage *,
	    struct ber_element *, struct snmp_message *);
unsigned long
	 snmpe_application(struct ber_element *);
void	 snmpe_sig_handler(int sig, short, void *);
int	 snmpe_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 snmpe_bind(struct address *);
void	 snmpe_recvmsg(int fd, short, void *);
int	 snmpe_encode(struct snmp_message *);

struct snmpd	*env = NULL;

struct imsgev	*iev_parent;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	snmpe_dispatch_parent }
};

pid_t
snmpe(struct privsep *ps, struct privsep_proc *p)
{
#ifdef DEBUG
	char		 buf[BUFSIZ];
	struct oid	*oid;
#endif

	env = ps->ps_env;

#ifdef DEBUG
	for (oid = NULL; (oid = smi_foreach(oid, 0)) != NULL;) {
		smi_oid2string(&oid->o_id, buf, sizeof(buf), 0);
		log_debug("oid %s", buf);
	}
#endif

	/* bind SNMP UDP socket */
	if ((env->sc_sock = snmpe_bind(&env->sc_address)) == -1)
		fatalx("snmpe: failed to bind SNMP UDP socket");

	return (proc_run(ps, p, procs, nitems(procs), snmpe_init, NULL));
}

/* ARGSUSED */
void
snmpe_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	kr_init();
	trap_init();
	timer_init();
	usm_generate_keys();

	/* listen for incoming SNMP UDP messages */
	event_set(&env->sc_ev, env->sc_sock, EV_READ|EV_PERSIST,
	    snmpe_recvmsg, env);
	event_add(&env->sc_ev, NULL);
}

void
snmpe_shutdown(void)
{
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
	int	 s;

	if ((s = snmpd_socket_af(&addr->ss, htons(addr->port))) == -1)
		return (-1);

	/*
	 * Socket options
	 */
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;

	if (bind(s, (struct sockaddr *)&addr->ss, addr->ss.ss_len) == -1)
		goto bad;

	if (print_host(&addr->ss, buf, sizeof(buf)) == NULL)
		goto bad;

	log_info("snmpe_bind: binding to address %s:%d", buf, addr->port);

	return (s);

 bad:
	close(s);
	return (-1);
}

int
snmpe_parse(struct sockaddr_storage *ss,
    struct ber_element *root, struct snmp_message *msg)
{
	struct snmp_stats	*stats = &env->sc_stats;
	struct ber_element	*a, *b, *c, *d, *e, *f, *next, *last;
	const char		*errstr = "invalid message";
	long long		 ver, req;
	long long		 errval, erridx;
	unsigned long		 type;
	u_int			 class, state, i = 0, j = 0;
	char			*comn, buf[BUFSIZ], host[MAXHOSTNAMELEN];
	char			*flagstr, *ctxname;
	struct ber_oid		 o;
	size_t			 len;

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
			errstr = "community name too long";
			goto fail;
		}
		break;
	case SNMP_V3:
		if (ber_scanf_elements(a, "{iisi}e",
		    &msg->sm_msgid, &msg->sm_max_msg_size, &flagstr,
		    &msg->sm_secmodel, &a) != 0)
			goto parsefail;

		msg->sm_flags = *flagstr;
		if (MSG_SECLEVEL(msg) < env->sc_min_seclevel ||
		    msg->sm_secmodel != SNMP_SEC_USM) {
			/* XXX currently only USM supported */
			errstr = "unsupported security model";
			stats->snmp_usmbadseclevel++;
			msg->sm_usmerr = OIDVAL_usmErrSecLevel;
			goto parsefail;
		}

		if ((a = usm_decode(msg, a, &errstr)) == NULL)
			goto parsefail;

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
		errstr = "bad snmp version";
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
			errstr = "invalid request for protocol version 1";
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
			errstr = "wrong read community";
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
			errstr = "wrong write community";
			goto fail;
		}
		msg->sm_context = type;
		break;
	case SNMP_C_GETRESP:
		stats->snmp_ingetresponses++;
		errstr = "response without request";
		goto parsefail;
	case SNMP_C_TRAP:
	case SNMP_C_TRAPV2:
		if (msg->sm_version != SNMP_V3 &&
		    strcmp(env->sc_trcommunity, msg->sm_community) != 0) {
			stats->snmp_inbadcommunitynames++;
			errstr = "wrong trap community";
			goto fail;
		}
		stats->snmp_intraps++;
		errstr = "received trap";
		goto fail;
	default:
		errstr = "invalid context";
		goto parsefail;
	}

	/* SNMP PDU */
	if (ber_scanf_elements(a, "iiie{et",
	    &req, &errval, &erridx, &msg->sm_pduend,
	    &msg->sm_varbind, &class, &type) != 0) {
		stats->snmp_silentdrops++;
		errstr = "invalid PDU";
		goto fail;
	}
	if (class != BER_CLASS_UNIVERSAL || type != BER_TYPE_SEQUENCE) {
		stats->snmp_silentdrops++;
		errstr = "invalid varbind";
		goto fail;
	}

	msg->sm_request = req;
	msg->sm_error = errval;
	msg->sm_errorindex = erridx;

	print_host(ss, host, sizeof(host));
	if (msg->sm_version == SNMP_V3)
		log_debug("snmpe_parse: %s: SNMPv3 context %d, flags %#x, "
		    "secmodel %lld, user '%s', ctx-engine %s, ctx-name '%s', "
		    "request %lld", host, msg->sm_context, msg->sm_flags,
		    msg->sm_secmodel, msg->sm_username,
		    tohexstr(msg->sm_ctxengineid, msg->sm_ctxengineid_len),
		    msg->sm_ctxname, msg->sm_request);
	else
		log_debug("snmpe_parse: %s: SNMPv%d '%s' context %d "
		    "request %lld", host, msg->sm_version + 1,
		    msg->sm_community, msg->sm_context, msg->sm_request);

	errstr = "invalid varbind element";
	for (i = 1, a = msg->sm_varbind, last = NULL;
	    a != NULL && i < SNMPD_MAXVARBIND; a = next, i++) {
		next = a->be_next;

		if (a->be_class != BER_CLASS_UNIVERSAL ||
		    a->be_type != BER_TYPE_SEQUENCE)
			continue;
		if ((b = a->be_sub) == NULL)
			continue;
		for (state = 0; state < 2 && b != NULL; b = b->be_next) {
			switch (state++) {
			case 0:
				if (ber_get_oid(b, &o) != 0)
					goto varfail;
				if (o.bo_n < BER_MIN_OID_LEN ||
				    o.bo_n > BER_MAX_OID_LEN)
					goto varfail;
				if (msg->sm_context == SNMP_C_SETREQ)
					stats->snmp_intotalsetvars++;
				else
					stats->snmp_intotalreqvars++;
				log_debug("snmpe_parse: %s: oid %s", host,
				    smi_oid2string(&o, buf, sizeof(buf), 0));
				break;
			case 1:
				c = d = NULL;
				switch (msg->sm_context) {
				case SNMP_C_GETNEXTREQ:
					c = ber_add_sequence(NULL);
					if ((d = mps_getnextreq(c, &o)) != NULL)
						break;
					ber_free_elements(c);
					c = NULL;
					msg->sm_error = SNMP_ERROR_NOSUCHNAME;
					msg->sm_errorindex = i;
					break;	/* ignore error */
				case SNMP_C_GETREQ:
					c = ber_add_sequence(NULL);
					if ((d = mps_getreq(c, &o,
					    msg->sm_version)) != NULL)
						break;
					msg->sm_error = SNMP_ERROR_NOSUCHNAME;
					ber_free_elements(c);
					goto varfail;
				case SNMP_C_SETREQ:
					if (env->sc_readonly == 0
					    && mps_setreq(b, &o) == 0)
						break;
					msg->sm_error = SNMP_ERROR_READONLY;
					goto varfail;
				case SNMP_C_GETBULKREQ:
					j = msg->sm_maxrepetitions;
					msg->sm_errorindex = 0;
					msg->sm_error = SNMP_ERROR_NOSUCHNAME;
					for (d = NULL, len = 0; j > 0; j--) {
						e = ber_add_sequence(NULL);
						if (c == NULL)
							c = e;
						f = mps_getnextreq(e, &o);
						if (f == NULL) {
							ber_free_elements(e);
							if (d == NULL)
								goto varfail;
							break;
						}
						len += ber_calc_len(e);
						if (len > SNMPD_MAXVARBINDLEN) {
							ber_free_elements(e);
							break;
						}
						if (d != NULL)
							ber_link_elements(d, e);
						d = e;
					}
					msg->sm_error = 0;
					break;
				default:
					goto varfail;
				}
				if (c == NULL)
					break;
				if (last == NULL)
					msg->sm_varbindresp = c;
				else
					ber_link_elements(last, c);
				last = c;
				break;
			}
		}
		if (state < 2)  {
			log_debug("snmpe_parse: state %d", state);
			goto varfail;
		}
	}

	return (0);
 varfail:
	log_debug("snmpe_parse: %s: %s, error index %d", host, errstr, i);
	if (msg->sm_error == 0)
		msg->sm_error = SNMP_ERROR_GENERR;
	msg->sm_errorindex = i;
	return (0);
 parsefail:
	stats->snmp_inasnparseerrs++;
 fail:
	print_host(ss, host, sizeof(host));
	log_debug("snmpe_parse: %s: %s", host, errstr);
	return (-1);
}

void
snmpe_recvmsg(int fd, short sig, void *arg)
{
	struct snmp_stats	*stats = &env->sc_stats;
	struct sockaddr_storage	 ss;
	u_int8_t		*ptr = NULL;
	socklen_t		 slen;
	ssize_t			 len;
	struct ber		 ber;
	struct ber_element	*req = NULL;
	struct snmp_message	 msg;

	bzero(&msg, sizeof(msg));
	slen = sizeof(ss);
	if ((len = recvfrom(fd, msg.sm_data, sizeof(msg.sm_data), 0,
	    (struct sockaddr *)&ss, &slen)) < 1)
		return;

	stats->snmp_inpkts++;
	msg.sm_datalen = (size_t)len;

	bzero(&ber, sizeof(ber));
	ber.fd = -1;
	ber_set_application(&ber, smi_application);
	ber_set_readbuf(&ber, msg.sm_data, msg.sm_datalen);

	req = ber_read_elements(&ber, NULL);
	if (req == NULL) {
		stats->snmp_inasnparseerrs++;
		goto done;
	}

#ifdef DEBUG
	fprintf(stderr, "recv msg:\n");
	smi_debug_elements(req);
#endif

	if (snmpe_parse(&ss, req, &msg) == -1) {
		if (msg.sm_usmerr != 0 && MSG_REPORT(&msg))
			usm_make_report(&msg);
		else
			goto done;
	} else
		msg.sm_context = SNMP_C_GETRESP;

	if (msg.sm_varbindresp == NULL && msg.sm_pduend != NULL)
		msg.sm_varbindresp = ber_unlink_elements(msg.sm_pduend);

	switch (msg.sm_error) {
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
	if (snmpe_encode(&msg) < 0)
		goto done;

	len = ber_write_elements(&ber, msg.sm_resp);
	if (ber_get_writebuf(&ber, (void *)&ptr) == -1)
		goto done;

	usm_finalize_digest(&msg, ptr, len);
	len = sendto(fd, ptr, len, 0, (struct sockaddr *)&ss, slen);
	if (len != -1)
		stats->snmp_outpkts++;

 done:
	ber_free(&ber);
	if (req != NULL)
		ber_free_elements(req);
	if (msg.sm_resp != NULL)
		ber_free_elements(msg.sm_resp);
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
		if ((epdu = ber_printf_elements(epdu, "xs{", env->sc_engineid,
		    env->sc_engineid_len, msg->sm_ctxname)) == NULL) {
			ber_free_elements(pdu);
			return -1;
		}
	}

	if (!ber_printf_elements(epdu, "tiii{e}.", BER_CLASS_CONTEXT,
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
