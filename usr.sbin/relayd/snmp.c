/*	$OpenBSD: snmp.c,v 1.20 2014/11/25 09:17:00 blambert Exp $	*/

/*
 * Copyright (c) 2008 - 2014 Reyk Floeter <reyk@openbsd.org>
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

#define	HOST_MAX_SUBIDX		10
#define	TABLE_MAX_SUBIDX	3
#define	ROUTER_MAX_SUBIDX	6
#define	NETROUTE_MAX_SUBIDX	5
#define	RELAY_MAX_SUBIDX	10
#define	SESSION_MAX_SUBIDX	12
#define	RDR_MAX_SUBIDX		10

#define	OIDIDX_relaydInfo	9

#define RELAYD_MIB	"1.3.6.1.4.1.30155.3"
#define SNMP_ELEMENT(x...)	do {				\
	if (snmp_element(RELAYD_MIB x) == -1)			\
		goto done;					\
} while (0)

static struct snmp_oid	hosttrapoid = {
	{ 1, 3, 6, 1, 4, 1, 30155, 3, 1, 0 },
	10
};

static struct snmp_oid	relaydinfooid = {
	{ 1, 3, 6, 1, 4, 1, 30155, 3, 2 },
	9
};

static struct agentx_handle	*snmp_agentx = NULL;
enum privsep_procid		 snmp_procid;

void	 snmp_sock(int, short, void *);
int	 snmp_element(const char *, enum snmp_type, void *, int64_t,
	    struct agentx_pdu *);
int	 snmp_string2oid(const char *, struct snmp_oid *);
char	*snmp_oid2string(struct snmp_oid *, char *, size_t);
void	 snmp_event_add(struct relayd *, int);
void	 snmp_agentx_process(struct agentx_handle *, struct agentx_pdu *,
	    void *);
int	 snmp_register(struct relayd *);
int	 snmp_unregister(struct relayd *);
void	 snmp_event_add(struct relayd *, int);
void	 snmp_agentx_process(struct agentx_handle *, struct agentx_pdu *, void *);

void	*sstodata(struct sockaddr_storage *);
size_t	 sstolen(struct sockaddr_storage *);

struct host *
	 snmp_host_byidx(struct relayd *, u_int *, u_int *, u_int, u_int);
struct table *
	 snmp_table_byidx(struct relayd *, u_int *, u_int *, u_int, u_int);
struct router *
	 snmp_router_byidx(struct relayd *, u_int *, u_int *, u_int, u_int);
struct relay *
	 snmp_relay_byidx(struct relayd *, u_int *, u_int *, u_int, u_int);
struct rsession *
	 snmp_session_byidx(struct relayd *, u_int *, u_int *, u_int, u_int);
struct rdr *
	 snmp_rdr_byidx(struct relayd *, u_int *, u_int *, u_int, u_int);
struct netroute *
	 snmp_netroute_byidx(struct relayd *, u_int *, u_int *, u_int, u_int);

int	 snmp_redirect(struct relayd *, struct snmp_oid *, struct agentx_pdu *,
	    int, uint32_t, uint32_t, u_int);
int	 snmp_relay(struct relayd *, struct snmp_oid *, struct agentx_pdu *,
	    int, uint32_t, uint32_t, u_int);
int	 snmp_router(struct relayd *, struct snmp_oid *, struct agentx_pdu *,
	    int, uint32_t, uint32_t, u_int);
int	 snmp_netroute(struct relayd *, struct snmp_oid *, struct agentx_pdu *,
	    int, uint32_t, uint32_t, u_int);
int	 snmp_host(struct relayd *, struct snmp_oid *, struct agentx_pdu *,
	    int, uint32_t, uint32_t, u_int);
int	 snmp_session(struct relayd *, struct snmp_oid *, struct agentx_pdu *,
	    int, uint32_t, uint32_t, u_int);
int	 snmp_table(struct relayd *, struct snmp_oid *, struct agentx_pdu *,
	    int, uint32_t, uint32_t, u_int);

void
snmp_init(struct relayd *env, enum privsep_procid id)
{
	if (event_initialized(&env->sc_snmpev))
		event_del(&env->sc_snmpev);
	if (event_initialized(&env->sc_snmpto))
		event_del(&env->sc_snmpto);
	if (env->sc_snmp != -1) {
		if (snmp_agentx) {
			snmp_unregister(env);
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
	if (strlcpy(sun.sun_path, env->sc_snmp_path,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path))
		fatalx("invalid socket path");

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
	struct relayd		*env = arg;
	struct agentx_pdu	*pdu;
	int			 evflags = 0;

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
		if ((pdu = snmp_agentx_recv(snmp_agentx)) == NULL) {
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

		snmp_agentx_process(snmp_agentx, pdu, env);
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
	struct agentx_getbulk_repeaters		 repeaters;
	struct agentx_search_range		 sr;
	struct snmp_oid				 oid;
	struct agentx_close_request_data	 close_hdr;
	struct relayd				*env = arg;
	struct agentx_pdu			*resp;
	u_int					 erridx = 0;
	int					 getnext = 0, repetitions;
	int					 nonrepeaters, maxrepetitions;

	nonrepeaters = maxrepetitions = -1;

	switch (pdu->hdr->type) {
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

	case AGENTX_GET_BULK:
		if (snmp_agentx_read_raw(pdu,
		    &repeaters, sizeof(repeaters)) == -1)
			break;

		nonrepeaters = repeaters.nonrepeaters;
		maxrepetitions = repeaters.maxrepetitions;

		/* FALLTHROUGH */
	case AGENTX_GET:
	case AGENTX_GET_NEXT:
		if ((resp = snmp_agentx_response_pdu(0, AGENTX_ERR_NONE, 0)) == NULL) {
			log_warn("%s unable to allocate response pdu", __func__);
			break;
		}
		repetitions = 0;
 repeat:
		while (pdu->datalen > sizeof(struct agentx_hdr)) {
			uint32_t infoendidx, infoentryendidx, infoentryidxendidx;

			erridx++;

			/* AgentX GETs are the OID followed by the null OID */
			if (snmp_agentx_read_searchrange(pdu, &sr) == -1) {
				snmp_agentx_pdu_free(resp);
				resp = NULL;
				break;
			}

			if (sr.end.o_n >= OIDIDX_relaydInfo + 1)
				infoendidx = sr.end.o_id[OIDIDX_relaydInfo];
			else
				infoendidx = UINT32_MAX;
			if (sr.end.o_n >= OIDIDX_relaydInfo + 1 + 1)
				infoentryendidx = sr.end.o_id[OIDIDX_relaydInfo + 1];
			else
				infoentryendidx = UINT32_MAX;
			if (sr.end.o_n >= OIDIDX_relaydInfo + 2 + 1)
				infoentryidxendidx = sr.end.o_id[OIDIDX_relaydInfo + 2];
			else
				infoentryidxendidx = UINT32_MAX;

			bcopy(&sr.start, &oid, sizeof(oid));

			/*
			 * If the requested OID is not part of the registered MIB,
			 * return "no such object", per RFC
			 */
			if (snmp_oid_cmp(&relaydinfooid, &oid) == -1) {
				if (snmp_agentx_varbind(resp, &sr.start,
				    AGENTX_NO_SUCH_OBJECT, NULL, 0) == -1) {
					log_warn("%s: unable to generate response",
					    __func__);
					snmp_agentx_pdu_free(resp);
					resp = NULL;
				}
				goto reply;
			}

			if (oid.o_n != OIDIDX_relaydInfo + 2 + 1) {
				/* GET requests require the exact OID to exist */
				if (pdu->hdr->type == AGENTX_GET)
					goto nosuchinstance;

				if (oid.o_n == OIDIDX_relaydInfo + 1) {
					oid.o_id[OIDIDX_relaydInfo + 1] = 0;
					oid.o_n = OIDIDX_relaydInfo + 1 + 1;
				}
				if (oid.o_n == OIDIDX_relaydInfo + 1 + 1) {
					oid.o_id[OIDIDX_relaydInfo + 2] = 0;
					oid.o_n = OIDIDX_relaydInfo + 2 + 1;
				}
				if (oid.o_n > OIDIDX_relaydInfo + 2 + 1)
					oid.o_n = OIDIDX_relaydInfo + 2 + 1;
			}

			/*
			 * If not including the starting OID, increment
			 * here to go to the 'next' OID to allow the lookups
			 * to work correctly, as they do 'include' matching
			 */
			if (pdu->hdr->type == AGENTX_GET_NEXT)
				getnext = 1;

			switch (oid.o_id[OIDIDX_relaydInfo]) {
			case 1:
				log_warnx("%s: redirects", __func__);
				if (infoendidx < 1)
					break;
				if (snmp_redirect(env, &oid, resp, getnext,
				    infoentryendidx, infoentryidxendidx, sr.include) == 0)
					break;
				if (!getnext)
					goto nosuchinstance;

				oid.o_id[OIDIDX_relaydInfo] = 2;
				oid.o_id[OIDIDX_relaydInfo + 1] = 0;
				oid.o_id[OIDIDX_relaydInfo + 2] = 0;
				/* FALLTHROUGH */
			case 2:
				log_warnx("%s: relays", __func__);
				if (infoendidx < 2)
					break;
				if (snmp_relay(env, &oid, resp, getnext,
				    infoentryendidx, infoentryidxendidx, sr.include) == 0)
					break;
				if (!getnext)
					goto nosuchinstance;

				oid.o_id[OIDIDX_relaydInfo] = 3;
				oid.o_id[OIDIDX_relaydInfo + 1] = 0;
				oid.o_id[OIDIDX_relaydInfo + 2] = 0;
				/* FALLTHROUGH */
			case 3:
				log_warnx("%s: routers", __func__);
				if (infoendidx < 3)
					break;
				if (snmp_router(env, &oid, resp, getnext,
				    infoentryendidx, infoentryidxendidx, sr.include) == 0)
					break;
				if (!getnext)
					goto nosuchinstance;

				oid.o_id[OIDIDX_relaydInfo] = 4;
				oid.o_id[OIDIDX_relaydInfo + 1] = 0;
				oid.o_id[OIDIDX_relaydInfo + 2] = 0;
				/* FALLTHROUGH */
			case 4:
				log_warnx("%s: relaydNetRoutes", __func__);
				if (infoendidx < 4)
					break;
				if (snmp_netroute(env, &oid, resp, getnext,
				    infoentryendidx, infoentryidxendidx, sr.include) == 0)
					break;
				if (!getnext)
					goto nosuchinstance;

				oid.o_id[OIDIDX_relaydInfo] = 5;
				oid.o_id[OIDIDX_relaydInfo + 1] = 0;
				oid.o_id[OIDIDX_relaydInfo + 2] = 0;
				/* FALLTHROUGH */
			case 5:
				log_warnx("%s: hosts", __func__);
				if (infoendidx < 5)
					break;
				if (snmp_host(env, &oid, resp, getnext,
				    infoentryendidx, infoentryidxendidx, sr.include) == 0)
					break;
				if (!getnext)
					goto nosuchinstance;

				oid.o_id[OIDIDX_relaydInfo] = 6;
				oid.o_id[OIDIDX_relaydInfo + 1] = 0;
				oid.o_id[OIDIDX_relaydInfo + 2] = 0;
				/* FALLTHROUGH */
			case 6:
				log_warnx("%s: sessions", __func__);
				if (infoendidx < 6)
					break;
				if (snmp_session(env, &oid, resp, getnext,
				    infoentryendidx, infoentryidxendidx, sr.include) == 0)
					break;
				if (!getnext)
					goto nosuchinstance;

				oid.o_id[OIDIDX_relaydInfo] = 7;
				oid.o_id[OIDIDX_relaydInfo + 1] = 0;
				oid.o_id[OIDIDX_relaydInfo + 2] = 0;
				/* FALLTHROUGH */
			case 7:
				log_warnx("%s: tables", __func__);
				if (infoendidx < 7)
					break;
				if (snmp_table(env, &oid, resp, getnext,
				    infoentryendidx, infoentryidxendidx, sr.include) == 0)
					break;
				if (!getnext)
					goto nosuchinstance;

				if (snmp_agentx_varbind(resp, &oid,
				    AGENTX_END_OF_MIB_VIEW, NULL, 0) == -1) {
					log_warn("%s: unable to generate response",
					    __func__);
					snmp_agentx_pdu_free(resp);
					resp = NULL;
				}
				goto reply;
			default:
 nosuchinstance:
				log_warnx("unknown index %i", oid.o_id[OIDIDX_relaydInfo]);
				if (snmp_agentx_varbind(resp, &sr.start,
				    AGENTX_NO_SUCH_INSTANCE, NULL, 0) == -1) {
					log_warn("%s: unable to generate response",
					    __func__);
					snmp_agentx_pdu_free(resp);
					resp = NULL;
				}
				goto reply;
			}
		}

		if (pdu->hdr->type == AGENTX_GET_BULK) {
			if (nonrepeaters >= 0)
				nonrepeaters--;
			else if (repetitions < maxrepetitions) {
				repetitions++;
				goto repeat;
			}
		}
 reply:
		if (resp) {
			snmp_agentx_send(snmp_agentx, resp);
			snmp_event_add(env, EV_WRITE);
		}

		break;

	case AGENTX_TEST_SET:
	case AGENTX_COMMIT_SET:
	case AGENTX_UNDO_SET:
	case AGENTX_CLEANUP_SET:
		log_warnx("unimplemented request type '%s'",
		    snmp_agentx_type2name(pdu->hdr->type));
		break;

	case AGENTX_RESPONSE:
		switch (pdu->request->hdr->type) {
		case AGENTX_NOTIFY:
			if (snmp_agentx_response(h, pdu) == -1)
				break;
		break;

		case AGENTX_OPEN:
			if (snmp_agentx_open_response(h, pdu) == -1)
				break;
			/* Open AgentX socket; register MIB if not trap-only */
			if (!(env->sc_snmp_flags & FSNMP_TRAPONLY))
				if (snmp_register(env) == -1) {
					log_warn("failed to register MIB");
					break;
				}
			break;

		case AGENTX_CLOSE:
			if (snmp_agentx_response(h, pdu) == -1)
				break;
			break;

		case AGENTX_REGISTER:
			if (snmp_agentx_response(h, pdu) == -1)
				break;
			break;

		case AGENTX_UNREGISTER:
			if (snmp_agentx_response(h, pdu) == -1)
				break;
			break;

		default:
			if (snmp_agentx_response(h, pdu) == -1)
				break;
			break;
		}
		break;

	/* these are nonsensical for subagents to receive */
	case AGENTX_OPEN:
	case AGENTX_REGISTER:
	case AGENTX_UNREGISTER:
	case AGENTX_NOTIFY:
	case AGENTX_PING:
	case AGENTX_INDEX_ALLOCATE:
	case AGENTX_INDEX_DEALLOCATE:
	case AGENTX_ADD_AGENT_CAPS:
	case AGENTX_REMOVE_AGENT_CAPS:
		log_warnx("ignoring request type '%s'",
		    snmp_agentx_type2name(pdu->hdr->type));
		break;

	default:
		log_warnx("unknown request type '%i'", pdu->hdr->type);
		if (snmp_agentx_response(h, pdu) == -1)
			break;
		break;
	}

	snmp_agentx_pdu_free(pdu);
	return;
}

int
snmp_register(struct relayd *env)
{
	struct agentx_pdu	*pdu;

	if ((pdu = snmp_agentx_register_pdu(&relaydinfooid, 3, 0, 0)) == NULL)
		return (-1);

	if (snmp_agentx_send(snmp_agentx, pdu) == -1)
		return (-1);

	snmp_event_add(env, EV_WRITE);
	return (0);
}

int
snmp_unregister(struct relayd *env)
{
	struct agentx_pdu	*pdu;

	if ((pdu = snmp_agentx_unregister_pdu(&relaydinfooid, 0, 0)) == NULL)
		return (-1);

	if (snmp_agentx_send(snmp_agentx, pdu) == -1)
		return (-1);

	snmp_event_add(env, EV_WRITE);
	return (0);
}

int
snmp_element(const char *oidstr, enum snmp_type type, void *buf, int64_t val,
    struct agentx_pdu *pdu)
{
	u_int32_t		 d;
	u_int64_t		 l;
	struct snmp_oid		 oid;

	DPRINTF("%s: oid %s type %d buf %p val %lld", __func__,
	    oidstr, type, buf, val);

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

void *
sstodata(struct sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return (&((struct sockaddr_in *)ss)->sin_addr);
	if (ss->ss_family == AF_INET6)
		return (&((struct sockaddr_in6 *)ss)->sin6_addr);
	return (NULL);
}

size_t
sstolen(struct sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return (((struct sockaddr_in *)ss)->sin_len);
	if (ss->ss_family == AF_INET6)
		return (((struct sockaddr_in6 *)ss)->sin6_len);
	return (0);
}

struct rdr *
snmp_rdr_byidx(struct relayd *env, u_int *instanceidx, u_int *objectidx,
    u_int getnext, u_int include)
{
	struct rdr	*rdr;

	if (*objectidx > RDR_MAX_SUBIDX)
		return (NULL);
	if (*objectidx == 0) {
		if (!getnext)
			return (NULL);
		*objectidx = 1;
	}

 restart:
	TAILQ_FOREACH(rdr, env->sc_rdrs, entry) {
		if (rdr->conf.id >= *instanceidx) {
			if (getnext) {
				/*  Lexographical ordering */

				/* 1) try the next instance index */
				if (rdr->conf.id == *instanceidx && !include)
					rdr = TAILQ_NEXT(rdr, entry);
				if (rdr) {
					*instanceidx = rdr->conf.id;
					return (rdr);
				}

				/* 2) try the next object index */
				if (*objectidx < RDR_MAX_SUBIDX) {
					*objectidx += 1;
					*instanceidx = 1;
					include = 1;
					goto restart;
				}

				/* 3) no further OIDs under this prefix */
				return (NULL);
			}

			if (rdr->conf.id == *instanceidx)
				return (rdr);

			return (NULL);
		}
	}

	return (NULL);
}

struct relay *
snmp_relay_byidx(struct relayd *env, u_int *instanceidx, u_int *objectidx,
    u_int getnext, u_int include)
{
	struct relay	*rly;

	if (*objectidx > RELAY_MAX_SUBIDX)
		return (NULL);
	if (*objectidx == 0) {
		if (!getnext)
			return (NULL);
		*objectidx = 1;
	}

 restart:
	TAILQ_FOREACH(rly, env->sc_relays, rl_entry) {
		if (rly->rl_conf.id >= *instanceidx) {
			if (getnext) {
				/*  Lexographical ordering */

				/* 1) try the next instance index */
				if (rly->rl_conf.id == *instanceidx && !include)
					rly = TAILQ_NEXT(rly, rl_entry);
				if (rly) {
					*instanceidx = rly->rl_conf.id;
					return (rly);
				}

				/* 2) try the next object index */
				if (*objectidx < RELAY_MAX_SUBIDX) {
					*objectidx += 1;
					*instanceidx = 1;
					include = 1;
					goto restart;
				}

				/* 3) no further OIDs under this prefix */
				return (rly);
			}

			if (rly->rl_conf.id == *instanceidx)
				return (rly);

			return (NULL);
		}
	}

	return (NULL);
}

struct router *
snmp_router_byidx(struct relayd *env, u_int *instanceidx, u_int *objectidx,
    u_int getnext, u_int include)
{
	struct router	*router;

	if (*objectidx > ROUTER_MAX_SUBIDX)
		return (NULL);
	if (*objectidx == 0) {
		if (!getnext)
			return (NULL);
		*objectidx = 1;
	}

 restart:
	TAILQ_FOREACH(router, env->sc_rts, rt_entry) {
		if (router->rt_conf.id >= *instanceidx) {
			if (getnext) {
				/*  Lexographical ordering */

				/* 1) try the next instance index */
				if (router->rt_conf.id == *instanceidx && !include)
					router = TAILQ_NEXT(router, rt_entry);
				if (router) {
					*instanceidx = router->rt_conf.id;
					return (router);
				}

				/* 2) try the next object index */
				if (*objectidx < ROUTER_MAX_SUBIDX) {
					*objectidx += 1;
					*instanceidx = 1;
					include = 1;
					goto restart;
				}

				/* 3) no further OIDs under this prefix */
				return (NULL);
			}

			if (router->rt_conf.id == *instanceidx)
				return (router);

			return (NULL);
		}
	}

	return (NULL);
}

struct netroute *
snmp_netroute_byidx(struct relayd *env, u_int *instanceidx, u_int *objectidx,
    u_int getnext, u_int include)
{
	struct netroute		*nr;

	if (*objectidx > NETROUTE_MAX_SUBIDX)
		return (NULL);
	if (*objectidx == 0) {
		if (!getnext)
			return (NULL);
		*objectidx = 1;
	}

 restart:
	TAILQ_FOREACH(nr, env->sc_routes, nr_route) {
		if (nr->nr_conf.id >= *instanceidx) {
			if (getnext) {
				/*  Lexographical ordering */

				/* 1) try the next instance index */
				if (nr->nr_conf.id == *instanceidx && !include)
					nr = TAILQ_NEXT(nr, nr_route);
				if (nr) {
					*instanceidx = nr->nr_conf.id;
					return (nr);
				}

				/* 2) try the next object index */
				if (*objectidx < NETROUTE_MAX_SUBIDX) {
					*objectidx += 1;
					*instanceidx = 1;
					include = 1;
					goto restart;
				}

				/* 3) no further OIDs under this prefix */
				return (NULL);
			}

			if (nr->nr_conf.id == *instanceidx)
				return (nr);

			return (NULL);
		}
	}

	return (NULL);
}

struct host *
snmp_host_byidx(struct relayd *env, u_int *instanceidx, u_int *objectidx,
    u_int getnext, u_int include)
{
	struct host	*host;

	if (*objectidx > HOST_MAX_SUBIDX)
		return (NULL);
	if (*objectidx == 0) {
		if (!getnext)
			return (NULL);
		*objectidx = 1;
	}

 restart:
	TAILQ_FOREACH(host, &env->sc_hosts, globalentry) {
		if (host->conf.id >= *instanceidx) {
			if (getnext) {
				/*  Lexographical ordering */

				/* 1) try the next instance index */
				if (host->conf.id == *instanceidx && !include)
					host = TAILQ_NEXT(host, globalentry);
				if (host) {
					*instanceidx = host->conf.id;
					return (host);
				}

				/* 2) try the next object index */
				if (*objectidx < HOST_MAX_SUBIDX) {
					*objectidx += 1;
					*instanceidx = 1;
					include = 1;
					goto restart;
				}

				/* 3) no further OIDs under this prefix */
				return (NULL);
			}

			if (host->conf.id == *instanceidx)
				return (host);

			return (NULL);
		}
	}

	return (NULL);
}

struct rsession *
snmp_session_byidx(struct relayd *env, u_int *instanceidx, u_int *objectidx,
    u_int getnext, u_int include)
{
	struct rsession		*session;

	if (*objectidx > SESSION_MAX_SUBIDX)
		return (NULL);
	if (*objectidx == 0) {
		if (!getnext)
			return (NULL);
		*objectidx = 1;
	}

 restart:
	TAILQ_FOREACH(session, &env->sc_sessions, se_entry) {
		if (session->se_id >= *instanceidx) {
			if (getnext) {
				/*  Lexographical ordering */

				/* 1) try the next instance index */
				if (session->se_id == *instanceidx && !include)
					session = TAILQ_NEXT(session, se_entry);
				if (session) {
					*instanceidx = session->se_id;
					return (session);
				}

				/* 2) try the next object index */
				if (*objectidx < SESSION_MAX_SUBIDX) {
					*objectidx += 1;
					*instanceidx = 1;
					include = 1;
					goto restart;
				}

				/* 3) no further OIDs under this prefix */
				return (NULL);
			}

			if (session->se_id == *instanceidx)
				return (session);

			return (NULL);
		}
	}

	return (NULL);
}

struct table *
snmp_table_byidx(struct relayd *env, u_int *instanceidx, u_int *objectidx,
    u_int getnext, u_int include)
{
	struct table		*table;

	if (*objectidx > TABLE_MAX_SUBIDX)
		return (NULL);
	if (*objectidx == 0) {
		if (!getnext)
			return (NULL);
		*objectidx = 1;
	}

 restart:
	TAILQ_FOREACH(table, env->sc_tables, entry) {
		if (table->conf.id >= *instanceidx) {
			if (getnext) {
				/*  Lexographical ordering */

				/* 1) try the next instance index */
				if (table->conf.id == *instanceidx && !include)
					table = TAILQ_NEXT(table, entry);
				if (table) {
					*instanceidx = table->conf.id;
					return (table);
				}

				/* 2) try the next object index */
				if (*objectidx < TABLE_MAX_SUBIDX) {
					*objectidx += 1;
					*instanceidx = 1;
					include = 1;
					goto restart;
				}

				/* 3) no further OIDs under this prefix */
				return (NULL);
			}

			if (table->conf.id == *instanceidx)
				return (table);

			return (NULL);
		}
	}

	return (NULL);
}

int
snmp_redirect(struct relayd *env, struct snmp_oid *oid,
    struct agentx_pdu *resp, int getnext, uint32_t einstanceidx,
    uint32_t eobjectidx, u_int include)
{
	struct rdr	*rdr;
	u_int		 instanceidx, objectidx;
	u_int32_t	 status;

	instanceidx = oid->o_id[OIDIDX_relaydInfo + 2];
	objectidx = oid->o_id[OIDIDX_relaydInfo + 1];
	rdr = snmp_rdr_byidx(env, &instanceidx, &objectidx, getnext, include);
	if (rdr == NULL)
		return (-1);

	if (instanceidx >= einstanceidx || objectidx >= eobjectidx)
		return (0);

	oid->o_id[OIDIDX_relaydInfo + 1] = objectidx;
	oid->o_id[OIDIDX_relaydInfo + 2] = instanceidx;

	switch (objectidx) {
	case 1:		/* index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rdr->conf.id,
		    sizeof(rdr->conf.id)) == -1)
			return (-1);
		break;
	case 2:		/* status */
		if (rdr->conf.flags & F_DISABLE)
			status = 1;
		else if (rdr->conf.flags & F_DOWN)
			status = 2;
		else if (rdr->conf.flags & F_BACKUP)
			status = 3;
		else
			status = 0;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &status,
		    sizeof(status)) == -1)
			return (-1);
		break;
	case 3:		/* name */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, rdr->conf.name,
		    strlen(rdr->conf.name)) == -1)
			return (-1);
		break;
	case 4:		/* count */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_COUNTER64, &rdr->stats.cnt,
		    sizeof(rdr->stats.cnt)) == -1)
			return (-1);
		break;
	case 5:		/* average */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rdr->stats.avg,
		    sizeof(rdr->stats.avg)) == -1)
			return (-1);
		break;
	case 6:		/* last */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rdr->stats.last,
		    sizeof(rdr->stats.last)) == -1)
			return (-1);
		break;
	case 7:		/* average hour */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rdr->stats.avg_hour,
		    sizeof(rdr->stats.avg_hour)) == -1)
			return (-1);
		break;
	case 8:		/* last hour */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rdr->stats.last_hour,
		    sizeof(rdr->stats.last_hour)) == -1)
			return (-1);
		break;
	case 9:		/* average day */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rdr->stats.avg_day,
		    sizeof(rdr->stats.avg_day)) == -1)
			return (-1);
		break;
	case 10:	/* last day */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rdr->stats.last_day,
		    sizeof(rdr->stats.last_day)) == -1)
			return (-1);
		break;
	default:
		fatalx("unhandled host element id");
	}

	return (0);
}

int
snmp_relay(struct relayd *env, struct snmp_oid *oid, struct agentx_pdu *resp,
    int getnext, uint32_t einstanceidx, uint32_t eobjectidx, u_int include)
{
	struct relay	*rly;
	u_int		 instanceidx, objectidx;
	u_int32_t	 status, value = 0;
	u_int64_t	 value64 = 0;
	int		 i;

	instanceidx = oid->o_id[OIDIDX_relaydInfo + 2];
	objectidx = oid->o_id[OIDIDX_relaydInfo + 1];
	rly = snmp_relay_byidx(env, &instanceidx, &objectidx, getnext,
	    include);
	if (rly == NULL)
		return (-1);

	if (instanceidx >= einstanceidx || objectidx >= eobjectidx)
		return (0);

	oid->o_id[OIDIDX_relaydInfo + 1] = objectidx;
	oid->o_id[OIDIDX_relaydInfo + 2] = instanceidx;

	switch (objectidx) {
	case 1:		/* index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &rly->rl_conf.id,
		    sizeof(rly->rl_conf.id)) == -1)
			return (-1);
		break;
	case 2:		/* status */
		if (rly->rl_up == HOST_UP)
			status = 0;		/* active */
		else
			status = 1;		/* disabled */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &status,
		    sizeof(status)) == -1)
			return (-1);
		break;
	case 3:		/* name */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, &rly->rl_conf.name,
		    strlen(rly->rl_conf.name)) == -1)
			return (-1);
		break;
	case 4:		/* count */
		for (i = 0; i < env->sc_prefork_relay; i++)
			value64 += rly->rl_stats[i].cnt;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_COUNTER64, &value64,
		    sizeof(value64)) == -1)
			return (-1);
		break;
	case 5:		/* average */
		for (i = 0; i < env->sc_prefork_relay; i++)
			value += rly->rl_stats[i].avg;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &value,
		    sizeof(value)) == -1)
			return (-1);
		break;
	case 6:		/* last */
		for (i = 0; i < env->sc_prefork_relay; i++)
			value += rly->rl_stats[i].last;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &value,
		    sizeof(value)) == -1)
			return (-1);
		break;
	case 7:		/* average hour */
		for (i = 0; i < env->sc_prefork_relay; i++)
			value += rly->rl_stats[i].avg_hour;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &value,
		    sizeof(value)) == -1)
			return (-1);
		break;
	case 8:		/* last hour */
		for (i = 0; i < env->sc_prefork_relay; i++)
			value += rly->rl_stats[i].last_hour;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &value,
		    sizeof(value)) == -1)
			return (-1);
		break;
	case 9:		/* average day */
		for (i = 0; i < env->sc_prefork_relay; i++)
			value += rly->rl_stats[i].avg_day;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &value,
		    sizeof(value)) == -1)
			return (-1);
		break;
	case 10:	/* last day */
		for (i = 0; i < env->sc_prefork_relay; i++)
			value += rly->rl_stats[i].last_day;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &value,
		    sizeof(value)) == -1)
			return (-1);
		break;
	default:
		fatalx("unhandled host element id");
	}

	return (0);
}

int
snmp_router(struct relayd *env, struct snmp_oid *oid, struct agentx_pdu *resp,
    int getnext, uint32_t einstanceidx, uint32_t eobjectidx, u_int include)
{
	struct router	*router;
	u_int		 instanceidx, objectidx;
	u_int32_t	 status;

	instanceidx = oid->o_id[OIDIDX_relaydInfo + 2];
	objectidx = oid->o_id[OIDIDX_relaydInfo + 1];
	router = snmp_router_byidx(env, &instanceidx, &objectidx, getnext,
	    include);
	if (router == NULL)
		return (-1);

	if (instanceidx >= einstanceidx || objectidx >= eobjectidx)
		return (0);

	oid->o_id[OIDIDX_relaydInfo + 1] = objectidx;
	oid->o_id[OIDIDX_relaydInfo + 2] = instanceidx;

	switch (objectidx) {
	case 1:		/* index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &router->rt_conf.id,
		    sizeof(router->rt_conf.id)) == -1)
			return (-1);
		break;
	case 2:		/* table index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &router->rt_conf.gwtable,
		    sizeof(router->rt_conf.gwtable)) == -1)
			return (-1);
		break;
	case 3:		/* status */
		if (router->rt_conf.flags & F_DISABLE)
			status = 1;		/* disabled */
		else
			status = 0;		/* active */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &status,
		    sizeof(status)) == -1)
			return (-1);
		break;
	case 4:		/* name */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, router->rt_conf.name,
		    strlen(router->rt_conf.name)) == -1)
			return (-1);
		break;
	case 5:		/* pf label */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, router->rt_conf.label,
		    strlen(router->rt_conf.label)) == -1)
			return (-1);
		break;
	case 6:		/* rtable */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &router->rt_conf.rtable,
		    sizeof(router->rt_conf.rtable)) == -1)
			return (-1);
		break;
	default:
		fatalx("unhandled host element id");
	}

	return (0);
}

int
snmp_netroute(struct relayd *env, struct snmp_oid *oid,
    struct agentx_pdu *resp, int getnext, uint32_t einstanceidx,
    uint32_t eobjectidx, u_int include)
{
	struct netroute			*nr;
	u_int32_t			 addrtype;
	u_int		 		 instanceidx, objectidx;

	instanceidx = oid->o_id[OIDIDX_relaydInfo + 2];
	objectidx = oid->o_id[OIDIDX_relaydInfo + 1];
	nr = snmp_netroute_byidx(env, &instanceidx, &objectidx, getnext,
	    include);
	if (nr == NULL)
		return (-1);

	if (instanceidx >= einstanceidx || objectidx >= eobjectidx)
		return (0);

	oid->o_id[OIDIDX_relaydInfo + 1] = objectidx;
	oid->o_id[OIDIDX_relaydInfo + 2] = instanceidx;

	switch (objectidx) {
	case 1:		/* index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &nr->nr_conf.id,
		    sizeof(nr->nr_conf.id)) == -1)
			return (-1);
		break;
	case 2:		/* address */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, sstodata(&nr->nr_conf.ss),
		    sstolen(&nr->nr_conf.ss)) == -1)
			return (-1);
		break;
	case 3:		/* address type */
		if (nr->nr_conf.ss.ss_family == AF_INET)
			addrtype = 1;
		else if (nr->nr_conf.ss.ss_family == AF_INET6)
			addrtype = 2;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &addrtype,
		    sizeof(addrtype)) == -1)
			return (-1);
		break;
	case 4:		/* prefix length */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &nr->nr_conf.prefixlen,
		    sizeof(nr->nr_conf.prefixlen)) == -1)
			return (-1);
		break;
	case 5:		/* router index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &nr->nr_conf.routerid,
		    sizeof(nr->nr_conf.routerid)) == -1)
			return (-1);
		break;
	default:
		fatalx("unhandled host element id");
	}

	return (0);
}

int
snmp_host(struct relayd *env, struct snmp_oid *oid, struct agentx_pdu *resp,
    int getnext, uint32_t einstanceidx, uint32_t eobjectidx, u_int include)
{
	struct host			*host;
	u_int32_t			 addrtype, count, error, status;
	u_int				 instanceidx, objectidx;

	instanceidx = oid->o_id[OIDIDX_relaydInfo + 2];
	objectidx = oid->o_id[OIDIDX_relaydInfo + 1];
	host = snmp_host_byidx(env, &instanceidx, &objectidx, getnext,
	    include);
	if (host == NULL)
		return (-1);

	if (instanceidx >= einstanceidx || objectidx >= eobjectidx)
		return (0);

	oid->o_id[OIDIDX_relaydInfo + 1] = objectidx;
	oid->o_id[OIDIDX_relaydInfo + 2] = instanceidx;

	switch (objectidx) {
	case 1:		/* index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &host->conf.id,
		    sizeof(host->conf.id)) == -1)
			return (-1);
		break;
	case 2:		/* parent index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &host->conf.parentid,
		    sizeof(host->conf.parentid)) == -1)
			return (-1);
		break;
	case 3:		/* table index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &host->conf.tableid,
		    sizeof(host->conf.tableid)) == -1)
			return (-1);
		break;
	case 4:		/* name */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, host->conf.name,
		    strlen(host->conf.name)) == -1)
			return (-1);
		break;
	case 5:		/* address */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, sstodata(&host->conf.ss),
		    sstolen(&host->conf.ss)) == -1)
			return (-1);
		break;
	case 6:		/* address type */
		if (host->conf.ss.ss_family == AF_INET)
			addrtype = 1;
		else if (host->conf.ss.ss_family == AF_INET6)
			addrtype = 2;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &addrtype,
		    sizeof(addrtype)) == -1)
			return (-1);
		break;
	case 7:		/* status */
		if (host->flags & F_DISABLE)
			status = 1;
		else if (host->up == HOST_UP)
			status = 0;
		else if (host->up == HOST_DOWN)
			status = 2;
		else
			status = 3;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &status,
		    sizeof(status)) == -1)
			return (-1);
		break;
	case 8:		/* check count */
		count = host->check_cnt;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &count,
		    sizeof(count)) == -1)
			return (-1);
		break;
	case 9:		/* up count */
		count = host->up_cnt;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &count,
		    sizeof(count)) == -1)
			return (-1);
		break;
	case 10:	/* errno */
		error = host->he;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &error,
		    sizeof(errno)) == -1)
			return (-1);
		break;
	default:
		fatalx("unhandled host element id");
	}

	return (0);
}

int
snmp_session(struct relayd *env, struct snmp_oid *oid, struct agentx_pdu *resp,
    int getnext, uint32_t einstanceidx, uint32_t eobjectidx, u_int include)
{
	struct timeval		 tv, now;
	time_t			 ticks;
	struct rsession		*session;
	u_int	 		 instanceidx, objectidx;
	u_int32_t		 status, pid, port, addrtype;

	instanceidx = oid->o_id[OIDIDX_relaydInfo + 2];
	objectidx = oid->o_id[OIDIDX_relaydInfo + 1];
	session = snmp_session_byidx(env, &instanceidx, &objectidx, getnext,
	    include);
	if (session == NULL)
		return (-1);

	if (instanceidx >= einstanceidx || objectidx >= eobjectidx)
		return (0);

	oid->o_id[OIDIDX_relaydInfo + 1] = objectidx;
	oid->o_id[OIDIDX_relaydInfo + 2] = instanceidx;

	switch (objectidx) {
	case 1:		/* index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &session->se_id,
		    sizeof(session->se_id)) == -1)
			return (-1);
		break;
	case 2:		/* relay index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &session->se_relayid,
		    sizeof(session->se_relayid)) == -1)
			return (-1);
		break;
	case 3:		/* in address */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, sstodata(&session->se_in.ss),
		    sstolen(&session->se_in.ss)) == -1)
			return (-1);
		break;
	case 4:		/* in address type */
		if (session->se_in.ss.ss_family == AF_INET)
			addrtype = 1;
		else if (session->se_in.ss.ss_family == AF_INET6)
			addrtype = 2;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &addrtype,
		    sizeof(addrtype)) == -1)
			return (-1);
		break;
	case 5:		/* out address */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, sstodata(&session->se_out.ss),
		    sstolen(&session->se_out.ss)) == -1)
			return (-1);
		break;
	case 6:		/* out address type */
		if (session->se_out.ss.ss_family == AF_INET)
			addrtype = 1;
		else if (session->se_out.ss.ss_family == AF_INET6)
			addrtype = 2;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &addrtype,
		    sizeof(addrtype)) == -1)
			return (-1);
		break;
	case 7:		/* port out */
		port = session->se_out.port;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &port,
		    sizeof(port)) == -1)
			return (-1);
		break;
	case 8:		/* port in */
		port = session->se_in.port;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &port,
		    sizeof(port)) == -1)
			return (-1);
		break;
	case 9:		/* age */
		getmonotime(&now);
		timerclear(&tv);
		timersub(&now, &session->se_tv_start, &tv);
		ticks = tv.tv_sec * 100 + tv.tv_usec / 10000;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &ticks,
		    sizeof(ticks)) == -1)
			return (-1);
		break;
	case 10:	/* idle time */
		getmonotime(&now);
		timerclear(&tv);
		timersub(&now, &session->se_tv_last, &tv);
		ticks = tv.tv_sec * 100 + tv.tv_usec / 10000;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &ticks,
		    sizeof(ticks)) == -1)
			return (-1);
		break;
	case 11:	/* status */
		if (session->se_done)
			status = 1;
		else
			status = 0;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &status,
		    sizeof(status)) == -1)
			return (-1);
		break;
	case 12:	/* session pid */
		pid = (u_int32_t)session->se_pid;
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &pid,
		    sizeof(pid)) == -1)
			return (-1);
		break;
	default:
		fatalx("unhandled table element id");
	}

	return (0);
}

int
snmp_table(struct relayd *env, struct snmp_oid *oid, struct agentx_pdu *resp,
    int getnext, uint32_t einstanceidx, uint32_t eobjectidx, u_int include)
{
	struct table		*table;
	u_int	 		 instanceidx, objectidx;
	u_int32_t		 status;

	instanceidx = oid->o_id[OIDIDX_relaydInfo + 2];
	objectidx = oid->o_id[OIDIDX_relaydInfo + 1];
	table = snmp_table_byidx(env, &instanceidx, &objectidx, getnext,
	    include);
	if (table == NULL)
		return (-1);

	if (instanceidx >= einstanceidx || objectidx >= eobjectidx)
		return (0);

	oid->o_id[OIDIDX_relaydInfo + 1] = objectidx;
	oid->o_id[OIDIDX_relaydInfo + 2] = instanceidx;

	switch (objectidx) {
	case 1:		/* index */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &table->conf.id,
		    sizeof(table->conf.id)) == -1)
			return (-1);
		break;
	case 2:		/* name */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_OCTET_STRING, &table->conf.name,
		    strlen(table->conf.name)) == -1)
			return (-1);
		break;
	case 3:		/* status */
		if (TAILQ_EMPTY(&table->hosts))
			status = 1;		/* empty */
		else if (table->conf.flags & F_DISABLE)
			status = 2;		/* disabled */
		else
			status = 0;		/* active */
		if (snmp_agentx_varbind(resp, oid,
		    AGENTX_INTEGER, &status,
		    sizeof(status)) == -1)
			return (-1);
		break;
	default:
		fatalx("unhandled table element id");
	}

	return (0);
}
