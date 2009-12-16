/*	$OpenBSD: snmpe.c,v 1.25 2009/12/16 22:17:53 deraadt Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
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

int	 snmpe_parse(struct sockaddr_storage *,
	    struct ber_element *, struct snmp_message *);
unsigned long
	 snmpe_application(struct ber_element *);
void	 snmpe_sig_handler(int sig, short, void *);
void	 snmpe_shutdown(void);
void	 snmpe_dispatch_parent(int, short, void *);
int	 snmpe_bind(struct address *);
void	 snmpe_recvmsg(int fd, short, void *);

struct snmpd	*env = NULL;

struct imsgev	*iev_parent;

void
snmpe_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		snmpe_shutdown();
		break;
	default:
		fatalx("snmpe_sig_handler: unexpected signal");
	}
}

pid_t
snmpe(struct snmpd *x_env, int pipe_parent2snmpe[2])
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
#ifdef DEBUG
	struct oid	*oid;
#endif

	switch (pid = fork()) {
	case -1:
		fatal("snmpe: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	env = x_env;

	if (control_init(&env->sc_csock) == -1)
		fatalx("snmpe: control socket setup failed");
	if (control_init(&env->sc_rcsock) == -1)
		fatalx("snmpe: restricted control socket setup failed");

	if ((env->sc_sock = snmpe_bind(&env->sc_address)) == -1)
		fatalx("snmpe: failed to bind SNMP UDP socket");

	if ((pw = getpwnam(SNMPD_USER)) == NULL)
		fatal("snmpe: getpwnam");

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("snmpe: chroot");
	if (chdir("/") == -1)
		fatal("snmpe: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG mode
#endif

	setproctitle("snmp engine");
	snmpd_process = PROC_SNMPE;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("snmpe: cannot drop privileges");
#endif

#ifdef DEBUG
	for (oid = NULL; (oid = smi_foreach(oid, 0)) != NULL;) {
		char	 buf[BUFSIZ];
		smi_oidstring(&oid->o_id, buf, sizeof(buf));
		log_debug("oid %s", buf);
	}
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, snmpe_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, snmpe_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	close(pipe_parent2snmpe[0]);

	if ((iev_parent = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal("snmpe");

	imsg_init(&iev_parent->ibuf, pipe_parent2snmpe[1]);
	iev_parent->handler = snmpe_dispatch_parent;

	iev_parent->events = EV_READ;
	event_set(&iev_parent->ev, iev_parent->ibuf.fd, iev_parent->events,
	    iev_parent->handler, iev_parent);
	event_add(&iev_parent->ev, NULL);

	TAILQ_INIT(&ctl_conns);

	if (control_listen(&env->sc_csock) == -1)
		fatalx("snmpe: control socket listen failed");
	if (control_listen(&env->sc_rcsock) == -1)
		fatalx("snmpe: restricted control socket listen failed");

	event_set(&env->sc_ev, env->sc_sock, EV_READ|EV_PERSIST,
	    snmpe_recvmsg, env);
	event_add(&env->sc_ev, NULL);

	kr_init();
	trap_init();
	timer_init();

	event_dispatch();

	snmpe_shutdown();
	kr_shutdown();

	return (0);
}

void
snmpe_shutdown(void)
{
	log_info("snmp engine exiting");
	_exit(0);
}

void
snmpe_dispatch_parent(int fd, short event, void * ptr)
{
	struct imsgev	*iev;
	struct imsgbuf	*ibuf;
	struct imsg	 imsg;
	ssize_t		 n;

	iev = ptr;
	ibuf = &iev->ibuf;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(iev);
		return;
	default:
		fatalx("snmpe_dispatch_parent: unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("snmpe_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("snmpe_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
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

#ifdef DEBUG
void
snmpe_debug_elements(struct ber_element *root)
{
	static int	 indent = 0;
	long long	 v;
	int		 d;
	char		*buf;
	size_t		 len;
	u_int		 i;
	int		 constructed;
	struct ber_oid	 o;
	char		 str[BUFSIZ];

	/* calculate lengths */
	ber_calc_len(root);

	switch (root->be_encoding) {
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
		constructed = root->be_encoding;
		break;
	default:
		constructed = 0;
		break;
	}

	fprintf(stderr, "%*slen %lu ", indent, "", root->be_len);
	switch (root->be_class) {
	case BER_CLASS_UNIVERSAL:
		fprintf(stderr, "class: universal(%u) type: ", root->be_class);
		switch (root->be_type) {
		case BER_TYPE_EOC:
			fprintf(stderr, "end-of-content");
			break;
		case BER_TYPE_BOOLEAN:
			fprintf(stderr, "boolean");
			break;
		case BER_TYPE_INTEGER:
			fprintf(stderr, "integer");
			break;
		case BER_TYPE_BITSTRING:
			fprintf(stderr, "bit-string");
			break;
		case BER_TYPE_OCTETSTRING:
			fprintf(stderr, "octet-string");
			break;
		case BER_TYPE_NULL:
			fprintf(stderr, "null");
			break;
		case BER_TYPE_OBJECT:
			fprintf(stderr, "object");
			break;
		case BER_TYPE_ENUMERATED:
			fprintf(stderr, "enumerated");
			break;
		case BER_TYPE_SEQUENCE:
			fprintf(stderr, "sequence");
			break;
		case BER_TYPE_SET:
			fprintf(stderr, "set");
			break;
		}
		break;
	case BER_CLASS_APPLICATION:
		fprintf(stderr, "class: application(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case SNMP_T_IPADDR:
			fprintf(stderr, "ipaddr");
			break;
		case SNMP_T_COUNTER32:
			fprintf(stderr, "counter32");
			break;
		case SNMP_T_GAUGE32:
			fprintf(stderr, "gauge32");
			break;
		case SNMP_T_TIMETICKS:
			fprintf(stderr, "timeticks");
			break;
		case SNMP_T_OPAQUE:
			fprintf(stderr, "opaque");
			break;
		case SNMP_T_COUNTER64:
			fprintf(stderr, "counter64");
			break;
		}
		break;
	case BER_CLASS_CONTEXT:
		fprintf(stderr, "class: context(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case SNMP_C_GETREQ:
			fprintf(stderr, "getreq");
			break;
		case SNMP_C_GETNEXTREQ:
			fprintf(stderr, "nextreq");
			break;
		case SNMP_C_GETRESP:
			fprintf(stderr, "getresp");
			break;
		case SNMP_C_SETREQ:
			fprintf(stderr, "setreq");
			break;
		case SNMP_C_TRAP:
			fprintf(stderr, "trap");
			break;
		case SNMP_C_GETBULKREQ:
			fprintf(stderr, "getbulkreq");
			break;
		case SNMP_C_INFORMREQ:
			fprintf(stderr, "informreq");
			break;
		case SNMP_C_TRAPV2:
			fprintf(stderr, "trapv2");
			break;
		case SNMP_C_REPORT:
			fprintf(stderr, "report");
			break;
		}
		break;
	case BER_CLASS_PRIVATE:
		fprintf(stderr, "class: private(%u) type: ", root->be_class);
		break;
	default:
		fprintf(stderr, "class: <INVALID>(%u) type: ", root->be_class);
		break;
	}
	fprintf(stderr, "(%lu) encoding %lu ",
	    root->be_type, root->be_encoding);

	if (constructed)
		root->be_encoding = constructed;

	switch (root->be_encoding) {
	case BER_TYPE_BOOLEAN:
		if (ber_get_boolean(root, &d) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "%s(%d)\n", d ? "true" : "false", d);
		break;
	case BER_TYPE_INTEGER:
	case BER_TYPE_ENUMERATED:
		if (ber_get_integer(root, &v) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "value %lld\n", v);
		break;
	case BER_TYPE_BITSTRING:
		if (ber_get_bitstring(root, (void *)&buf, &len) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "hexdump ");
		for (i = 0; i < len; i++)
			fprintf(stderr, "%02x", buf[i]);
		fprintf(stderr, "\n");
		break;
	case BER_TYPE_OBJECT:
		if (ber_get_oid(root, &o) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "oid %s",
		    smi_oidstring(&o, str, sizeof(str)));
		fprintf(stderr, "\n");
		break;
	case BER_TYPE_OCTETSTRING:
		if (ber_get_string(root, &buf) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		if (root->be_class == BER_CLASS_APPLICATION &&
		    root->be_type == SNMP_T_IPADDR) {
			fprintf(stderr, "addr %s\n",
			    inet_ntoa(*(struct in_addr *)buf));
		} else
			fprintf(stderr, "string \"%s\"\n",
			    root->be_len ? buf : "");
		break;
	case BER_TYPE_NULL:	/* no payload */
	case BER_TYPE_EOC:
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
	default:
		fprintf(stderr, "\n");
		break;
	}

	if (constructed && root->be_sub) {
		indent += 2;
		snmpe_debug_elements(root->be_sub);
		indent -= 2;
	}
	if (root->be_next)
		snmpe_debug_elements(root->be_next);
}
#endif

unsigned long
snmpe_application(struct ber_element *elm)
{
	if (elm->be_class != BER_CLASS_APPLICATION)
		return (BER_TYPE_OCTETSTRING);

	switch (elm->be_type) {
	case SNMP_T_IPADDR:
		return (BER_TYPE_OCTETSTRING);
	case SNMP_T_COUNTER32:
	case SNMP_T_GAUGE32:
	case SNMP_T_TIMETICKS:
	case SNMP_T_OPAQUE:
	case SNMP_T_COUNTER64:
		return (BER_TYPE_INTEGER);
	default:
		break;
	}
	return (BER_TYPE_OCTETSTRING);
}

int
snmpe_parse(struct sockaddr_storage *ss,
    struct ber_element *root, struct snmp_message *msg)
{
	struct snmp_stats	*stats = &env->sc_stats;
	struct ber_element	*a, *b, *c, *d, *e, *f, *next, *last;
	const char		*errstr = "invalid message";
	long long		 ver, req;
	unsigned long		 type, errval, erridx;
	u_int			 class, state, i = 0, j = 0;
	char			*comn, buf[BUFSIZ], host[MAXHOSTNAMELEN];
	struct ber_oid		 o;
	size_t			 len;

	bzero(msg, sizeof(*msg));

	if (ber_scanf_elements(root, "e{ieset{e",
	    &msg->sm_header, &ver, &msg->sm_headerend, &comn,
	    &msg->sm_pdu, &class, &type, &a) != 0)
		goto parsefail;

	/* SNMP version and community */
	switch (ver) {
	case SNMP_V1:
	case SNMP_V2:
		msg->sm_version = ver;
		break;
	case SNMP_V3:
	default:
		stats->snmp_inbadversions++;
		errstr = "bad snmp version";
		goto fail;
	}

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
		if (strcmp(env->sc_rdcommunity, comn) != 0 &&
		    strcmp(env->sc_rwcommunity, comn) != 0) {
			stats->snmp_inbadcommunitynames++;
			errstr = "wrong read community";
			goto fail;
		}
		msg->sm_context = type;
		break;
	case SNMP_C_SETREQ:
		stats->snmp_insetrequests++;
		if (strcmp(env->sc_rwcommunity, comn) != 0) {
			if (strcmp(env->sc_rdcommunity, comn) != 0)
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
		if (strcmp(env->sc_trcommunity, comn) != 0) {
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

	if (strlcpy(msg->sm_community, comn, sizeof(msg->sm_community)) >=
	    sizeof(msg->sm_community)) {
		stats->snmp_inbadcommunitynames++;
		errstr = "community name too long";
		goto fail;
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
	log_debug("snmpe_parse: %s: SNMPv%d '%s' context %d request %lld",
	    host, msg->sm_version + 1, msg->sm_community, msg->sm_context,
	    msg->sm_request);

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
				    smi_oidstring(&o, buf, sizeof(buf)));
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
					if ((d = mps_getreq(c, &o)) != NULL)
						break;
					msg->sm_error = SNMP_ERROR_NOSUCHNAME;
					ber_free_elements(c);
					goto varfail;
				case SNMP_C_SETREQ:
					if (mps_setreq(b, &o) == 0)
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
	u_int8_t		 buf[READ_BUF_SIZE], *ptr = NULL;
	socklen_t		 slen;
	ssize_t			 len;
	struct ber		 ber;
	struct ber_element	*req = NULL, *resp = NULL;
	struct snmp_message	 msg;

	slen = sizeof(ss);
	if ((len = recvfrom(fd, buf, sizeof(buf), 0,
	    (struct sockaddr *)&ss, &slen)) < 1)
		return;

	stats->snmp_inpkts++;

	bzero(&ber, sizeof(ber));
	ber.fd = -1;
	ber_set_application(&ber, snmpe_application);
	ber_set_readbuf(&ber, buf, len);

	req = ber_read_elements(&ber, NULL);

	if (req == NULL) {
		stats->snmp_inasnparseerrs++;
		goto done;
	}

#ifdef DEBUG
	snmpe_debug_elements(req);
#endif

	if (snmpe_parse(&ss, req, &msg) == -1)
		goto done;

	if (msg.sm_varbindresp == NULL)
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
	resp = ber_add_sequence(NULL);
	ber_printf_elements(resp, "ds{tiii{e}}.",
	    msg.sm_version, msg.sm_community,
	    BER_CLASS_CONTEXT, SNMP_C_GETRESP,
	    msg.sm_request, msg.sm_error, msg.sm_errorindex,
	    msg.sm_varbindresp);

#ifdef DEBUG
	snmpe_debug_elements(resp);
#endif

	len = ber_write_elements(&ber, resp);
	if (ber_get_writebuf(&ber, (void *)&ptr) == -1)
		goto done;

	len = sendto(fd, ptr, len, 0, (struct sockaddr *)&ss, slen);
	if (len != -1)
		stats->snmp_outpkts++;

 done:
	ber_free(&ber);
	if (req != NULL)
		ber_free_elements(req);
	if (resp != NULL)
		ber_free_elements(resp);
}
