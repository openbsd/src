/*	$OpenBSD: snmp.c,v 1.14 2014/04/14 12:58:04 blambert Exp $	*/

/*
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <event.h>

#include <openssl/ssl.h>

#include "relayd.h"
#include "snmp.h"

#define RELAYD_MIB	"1.3.6.1.4.1.30155.3"
#define SNMP_ELEMENT(x...)	do {				\
	if (snmp_element(RELAYD_MIB x) == -1)			\
		goto done;					\
} while (0)

static struct snmp_oid	hosttrapoid = {
	{ 1, 3, 6, 1, 4, 1, 30155, 3, 1, 0 },
	10
};

static struct agentx_handle	*snmp_agentx = NULL;
enum privsep_procid		 snmp_procid;

void	 snmp_sock(int, short, void *);
int	 snmp_element(const char *, enum snmp_type, void *, int64_t,
	    struct agentx_pdu *);
int	 snmp_string2oid(const char *, struct snmp_oid *);
void	 snmp_event_add(struct relayd *, int);
void	 snmp_agentx_process(struct agentx_handle *, struct agentx_pdu *,
	    void *);

void
snmp_init(struct relayd *env, enum privsep_procid id)
{
	if (event_initialized(&env->sc_snmpev))
		event_del(&env->sc_snmpev);
	if (event_initialized(&env->sc_snmpto))
		event_del(&env->sc_snmpto);
	if (env->sc_snmp != -1) {
		if (snmp_agentx) {
			snmp_agentx_close(snmp_agentx, AGENTX_CLOSE_OTHER);
			snmp_agentx = NULL;
		}
		close(env->sc_snmp);
		env->sc_snmp = -1;
	}

	if ((env->sc_flags & F_SNMP) == 0)
		return;

	snmp_procid = id;

	proc_compose_imsg(env->sc_ps, snmp_procid, -1,
	    IMSG_SNMPSOCK, -1, NULL, 0);
}

void
snmp_setsock(struct relayd *env, enum privsep_procid id)
{
	struct sockaddr_un	 sun;
	int			 s = -1;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		goto done;

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, env->sc_snmp_path, sizeof(sun.sun_path));

	socket_set_blockmode(s, BM_NONBLOCK);

	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		close(s);
		s = -1;
	}
 done:
	proc_compose_imsg(env->sc_ps, id, -1, IMSG_SNMPSOCK, s, NULL, 0);
}

int
snmp_getsock(struct relayd *env, struct imsg *imsg)
{
	struct timeval		 tv = SNMP_RECONNECT_TIMEOUT;
	struct agentx_pdu	*pdu;

	if (imsg->fd == -1)
		goto retry;

	env->sc_snmp = imsg->fd;

	log_debug("%s: got new snmp socket %d", __func__, imsg->fd);

	if ((snmp_agentx = snmp_agentx_alloc(env->sc_snmp)) == NULL)
		fatal("snmp_getsock: agentx alloc");
	if ((pdu = snmp_agentx_open_pdu(snmp_agentx, "relayd", NULL)) == NULL)
		fatal("snmp_getsock: agentx pdu");
	(void)snmp_agentx_send(snmp_agentx, pdu);

	snmp_agentx_set_callback(snmp_agentx, snmp_agentx_process, env);
	snmp_event_add(env, EV_WRITE);

	return (0);
 retry:
	evtimer_set(&env->sc_snmpto, snmp_sock, env);
	evtimer_add(&env->sc_snmpto, &tv);
	return (0);
}

void
snmp_event_add(struct relayd *env, int wflag)
{
	event_del(&env->sc_snmpev);
	event_set(&env->sc_snmpev, env->sc_snmp, EV_READ|wflag, snmp_sock, env);
	event_add(&env->sc_snmpev, NULL);
}

void
snmp_sock(int fd, short event, void *arg)
{
	struct relayd	*env = arg;
	int		 evflags = 0;

	if (event & EV_TIMEOUT) {
		goto reopen;
	}
	if (event & EV_WRITE) {
		if (snmp_agentx_send(snmp_agentx, NULL) == -1) {
			if (errno != EAGAIN)
				goto close;

			/* short write */
			evflags |= EV_WRITE;
		}
	}
	if (event & EV_READ) {
		if (snmp_agentx_recv(snmp_agentx) == NULL) {
			if (snmp_agentx->error) {
				log_warnx("agentx protocol error '%i'",
				    snmp_agentx->error);
				goto close;
			}
			if (errno != EAGAIN) {
				log_warn("agentx socket error");
				goto close;
			}

			/* short read */
		}

		/* PDU handled in the registered callback */
	}

	snmp_event_add(env, evflags);
	return;

 close:
	log_debug("%s: snmp socket closed %d", __func__, env->sc_snmp);
	snmp_agentx_free(snmp_agentx);
	env->sc_snmp = -1;
	snmp_agentx = NULL;
 reopen:
	proc_compose_imsg(env->sc_ps, snmp_procid, -1,
	    IMSG_SNMPSOCK, -1, NULL, 0);
	return;
}

void
snmp_agentx_process(struct agentx_handle *h, struct agentx_pdu *pdu, void *arg)
{
	struct agentx_close_request_data	 close_hdr;
	struct relayd				*env = arg;

	switch (pdu->request->hdr->type) {

	case AGENTX_NOTIFY:
		if (snmp_agentx_response(h, pdu) == -1)
			break;
		break;

	case AGENTX_OPEN:
		if (snmp_agentx_open_response(h, pdu) == -1)
			break;
		break;

	case AGENTX_CLOSE:
		snmp_agentx_read_raw(pdu, &close_hdr, sizeof(close_hdr));
		log_info("snmp: agentx master has closed connection (%i)",
		    close_hdr.reason);

		snmp_agentx_free(snmp_agentx);
		env->sc_snmp = -1;
		snmp_agentx = NULL;
		proc_compose_imsg(env->sc_ps, snmp_procid, -1,
		    IMSG_SNMPSOCK, -1, NULL, 0);
		break;

	default:
		if (snmp_agentx_response(h, pdu) == -1)
			break;
		break;
	}

	snmp_agentx_pdu_free(pdu);
	return;
}


int
snmp_element(const char *oidstr, enum snmp_type type, void *buf, int64_t val,
    struct agentx_pdu *pdu)
{
	u_int32_t		 d;
	u_int64_t		 l;
	struct snmp_oid		 oid;

	DPRINTF("%s: oid %s type %d buf %p val %lld", __func__,
	    oid, type, buf, val);

	if (snmp_string2oid(oidstr, &oid) == -1)
		return (-1);

	switch (type) {
	case SNMP_GAUGE32:
	case SNMP_NSAPADDR:
	case SNMP_INTEGER32:
	case SNMP_UINTEGER32:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_INTEGER,
		    &d, sizeof(d)) == -1)
			return (-1);
		break;

	case SNMP_COUNTER32:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_COUNTER32,
		    &d, sizeof(d)) == -1)
			return (-1);
		break;

	case SNMP_TIMETICKS:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_TIME_TICKS,
		    &d, sizeof(d)) == -1)
			return (-1);
		break;

	case SNMP_COUNTER64:
		l = (u_int64_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_COUNTER64,
		    &l, sizeof(l)) == -1)
			return (-1);
		break;

	case SNMP_IPADDR:
	case SNMP_OPAQUE:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_OPAQUE,
		    buf, strlen(buf)) == -1)
			return (-1);
		break;

	case SNMP_OBJECT: {
		struct snmp_oid		oid1;

		if (snmp_string2oid(buf, &oid1) == -1)
			return (-1);
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_OBJECT_IDENTIFIER,
		    &oid1, sizeof(oid1)) == -1)
			return (-1);
	}

	case SNMP_BITSTRING:
	case SNMP_OCTETSTRING:
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_OCTET_STRING,
		    buf, strlen(buf)) == -1)
			return (-1);
		break;

	case SNMP_NULL:
		/* no data beyond the OID itself */
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_NULL,
		    NULL, 0) == -1)
			return (-1);
	}

	return (0);
}

/*
 * SNMP traps for relayd
 */

void
snmp_hosttrap(struct relayd *env, struct table *table, struct host *host)
{
	struct agentx_pdu *pdu;

	if (snmp_agentx == NULL || env->sc_snmp == -1)
		return;

	/*
	 * OPENBSD-RELAYD-MIB host status trap
	 * XXX The trap format needs some tweaks and other OIDs
	 */

	if ((pdu = snmp_agentx_notify_pdu(&hosttrapoid)) == NULL)
		return;

	SNMP_ELEMENT(".1.0", SNMP_NULL, NULL, 0, pdu);
	SNMP_ELEMENT(".1.1.0", SNMP_OCTETSTRING, host->conf.name, 0, pdu);
	SNMP_ELEMENT(".1.2.0", SNMP_INTEGER32, NULL, host->up, pdu);
	SNMP_ELEMENT(".1.3.0", SNMP_INTEGER32, NULL, host->last_up, pdu);
	SNMP_ELEMENT(".1.4.0", SNMP_INTEGER32, NULL, host->up_cnt, pdu);
	SNMP_ELEMENT(".1.5.0", SNMP_INTEGER32, NULL, host->check_cnt, pdu);
	SNMP_ELEMENT(".1.6.0", SNMP_OCTETSTRING, table->conf.name, 0, pdu);
	SNMP_ELEMENT(".1.7.0", SNMP_INTEGER32, NULL, table->up, pdu);
	if (!host->conf.retry)
		goto done;
	SNMP_ELEMENT(".1.8.0", SNMP_INTEGER32, NULL, host->conf.retry, pdu);
	SNMP_ELEMENT(".1.9.0", SNMP_INTEGER32, NULL, host->retry_cnt, pdu);

 done:
	snmp_agentx_send(snmp_agentx, pdu);
	snmp_event_add(env, EV_WRITE);
}

int
snmp_string2oid(const char *oidstr, struct snmp_oid *o)
{
	char			*sp, *p, str[BUFSIZ];
	const char		*errstr;

	if (strlcpy(str, oidstr, sizeof(str)) >= sizeof(str))
		return (-1);
	bzero(o, sizeof(*o));

	for (p = sp = str; p != NULL; sp = p) {
		if ((p = strpbrk(p, ".-")) != NULL)
			*p++ = '\0';
		o->o_id[o->o_n++] = strtonum(sp, 0, UINT_MAX, &errstr);
		if (errstr || o->o_n > SNMP_MAX_OID_LEN)
			return (-1);
	}

	return (0);
}
