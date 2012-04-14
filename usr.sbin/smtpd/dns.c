/*	$OpenBSD: dns.c,v 1.48 2012/04/14 13:31:46 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2011 Eric Faurot <eric@faurot.net>
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
#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asr.h"
#include "dnsdefs.h"
#include "dnsutil.h"
#include "smtpd.h"
#include "log.h"


struct mx {
	char	host[MAXHOSTNAMELEN];
	int	prio;
};

struct dnssession {
	SPLAY_ENTRY(dnssession)		 nodes;
	u_int64_t			 id;
	struct dns			 query;
	struct event			 ev;
	struct asr_query		*aq;
	struct mx			 mxarray[MAX_MX_COUNT];
	size_t				 mxarraysz;
	size_t				 mxcurrent;
	size_t				 mxfound;
};

static int  dnssession_cmp(struct dnssession *, struct dnssession *);

SPLAY_HEAD(dnstree, dnssession) dns_sessions = SPLAY_INITIALIZER(&dns_sessions);

SPLAY_PROTOTYPE(dnstree, dnssession, nodes, dnssession_cmp);


static struct dnssession *dnssession_init(struct dns *);
static void dnssession_destroy(struct dnssession *);
static void dnssession_mx_insert(struct dnssession *, const char *, int);
static void dns_asr_event_set(struct dnssession *, struct asr_result *);
static void dns_asr_handler(int, short, void *);
static int  dns_asr_error(int);
static void dns_asr_dispatch_host(struct dnssession *);
static void dns_asr_dispatch_mx(struct dnssession *);
static void dns_asr_dispatch_cname(struct dnssession *);
static void dns_reply(struct dns *, int);

struct asr *asr = NULL;

/*
 * User interface.
 */

void
dns_query_host(char *host, int port, u_int64_t id)
{
	struct dns	 query;

	bzero(&query, sizeof(query));
	strlcpy(query.host, host, sizeof(query.host));
	query.port = port;
	query.id = id;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_DNS_HOST, 0, 0, -1,
	    &query, sizeof(query));
}

void
dns_query_mx(char *host, int port, u_int64_t id)
{
	struct dns	 query;

	bzero(&query, sizeof(query));
	strlcpy(query.host, host, sizeof(query.host));
	query.port = port;
	query.id = id;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_DNS_MX, 0, 0, -1, &query,
	    sizeof(query));
}

void
dns_query_ptr(struct sockaddr_storage *ss, u_int64_t id)
{
	struct dns	 query;

	bzero(&query, sizeof(query));
	query.ss = *ss;
	query.id = id;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_DNS_PTR, 0, 0, -1, &query,
	    sizeof(query));
}

/* LKA interface */
void
dns_async(struct imsgev *asker, int type, struct dns *query)
{
	struct dnssession *s;

	if (asr == NULL && (asr = asr_resolver(NULL)) == NULL) {
		log_warnx("dns_async: cannot create resolver");
		goto noasr;
	}

	query->type  = type;
	query->asker = asker;
	s = dnssession_init(query);

	switch (type) {
	case IMSG_DNS_HOST:
		log_debug("dns: lookup host \"%s\"", query->host);
		if (sockaddr_from_str((struct sockaddr*)&query->ss, PF_UNSPEC,
		    query->host) == 0) {
			log_debug("dns:  \"%s\" is an IP address", query->host);
			query->error = DNS_OK;
			dns_reply(query, IMSG_DNS_HOST);
			dns_reply(query, IMSG_DNS_HOST_END);
			dnssession_destroy(s);
			return;
		}
		dnssession_mx_insert(s, query->host, 0);
		stat_increment(STATS_LKA_SESSION_HOST);
		dns_asr_dispatch_host(s);
		return;
	case IMSG_DNS_PTR:
		s->aq = asr_query_cname(asr, (struct sockaddr*)&query->ss,
		    query->ss.ss_len);
		stat_increment(STATS_LKA_SESSION_CNAME);
		if (s->aq == NULL) {
			log_debug("dns_async: asr_query_cname error");
			break;
		}
		dns_asr_dispatch_cname(s);
		return;
	case IMSG_DNS_MX:
		log_debug("dns: lookup mx \"%s\"", query->host);
		s->aq = asr_query_dns(asr, T_MX, C_IN, query->host, 0);
		stat_increment(STATS_LKA_SESSION_MX);
		if (s->aq == NULL) {
			log_debug("dns_async: asr_query_dns error");
			break;
		}
		dns_asr_dispatch_mx(s);
		return;
	default:
		log_debug("dns_async: bad request");
		break;
	}

	stat_increment(STATS_LKA_FAILURE);
	dnssession_destroy(s);
noasr:
	query->error = DNS_RETRY;
	dns_reply(query, type != IMSG_DNS_PTR ? IMSG_DNS_HOST_END : type);
}

static void
dns_reply(struct dns *query, int type)
{
	imsg_compose_event(query->asker, type, 0, 0, -1, query, sizeof(*query));
}

static void
dns_asr_event_set(struct dnssession *s, struct asr_result *ar)
{
	struct timeval tv = { 0, 0 };
	
	tv.tv_usec = ar->ar_timeout * 1000;
	event_set(&s->ev, ar->ar_fd,
	    ar->ar_cond == ASR_READ ? EV_READ : EV_WRITE, dns_asr_handler, s);
	event_add(&s->ev, &tv);
}

static void
dns_asr_handler(int fd, short event, void *arg)
{
	struct dnssession *s = arg;

	switch(s->query.type) {
	case IMSG_DNS_HOST:
		dns_asr_dispatch_host(s);
		break;
	case IMSG_DNS_PTR:
		dns_asr_dispatch_cname(s);
		break;
	case IMSG_DNS_MX:
		dns_asr_dispatch_mx(s);
		break;
	default:
		fatalx("bad query type");
	}
}

static int
dns_asr_error(int ar_err)
{
	switch (ar_err) {
	case ASR_OK:
		return DNS_OK;
	case EASR_FAMILY:
	case EASR_NAME:
		stat_increment(STATS_LKA_FAILURE);
		return DNS_EINVAL;
	default:
		return DNS_RETRY;
	}
}

static void
dns_asr_dispatch_mx(struct dnssession *s)
{
	struct dns		*query = &s->query;
	struct asr_result	 ar;
	struct packed		 pack;
	struct header		 h;
	struct query		 q;
	struct rr		 rr;
	char			 buf[512];

	if (asr_run(s->aq, &ar) == ASR_COND) {
		dns_asr_event_set(s, &ar);
		return;
	}

	if (ar.ar_err) {
		query->error = dns_asr_error(ar.ar_err);
		dns_reply(query, IMSG_DNS_HOST_END);
		dnssession_destroy(s);
		return;
	}

	packed_init(&pack, ar.ar_data, ar.ar_datalen);
	unpack_header(&pack, &h);
	unpack_query(&pack, &q);

	/* check if the domain name exists */
	/* XXX what about other DNS error codes? */
	if (RCODE(h.flags) == ERR_NAME) {
		query->error = DNS_ENONAME;
		dns_reply(query, IMSG_DNS_HOST_END);
		dnssession_destroy(s);
		return;
	}

	if (h.ancount == 0)
		/* fallback to host if no MX is found. */
		dnssession_mx_insert(s, query->host, 0);

	for (; h.ancount; h.ancount--) {
		unpack_rr(&pack, &rr);
		print_dname(rr.rr.mx.exchange, buf, sizeof(buf));
		buf[strlen(buf) - 1] = '\0';
		dnssession_mx_insert(s, buf, rr.rr.mx.preference);
	}

	free(ar.ar_data);
	ar.ar_data = NULL;

	/* Now we have a sorted list of MX to resolve. Simply "turn" this
	 * MX session into a regular host session.
	 */
	s->aq = NULL;
	s->query.type = IMSG_DNS_HOST;
	dns_asr_dispatch_host(s);
}

static void
dns_asr_dispatch_host(struct dnssession *s)
{
	struct dns		*query = &s->query;
	struct mx		*mx;
	struct asr_result	 ar;
	int			 ret;

	/* default to notfound, override with retry or ok later */
	if (s->mxcurrent == 0)
		query->error = DNS_ENOTFOUND;

next:
	/* query all listed hosts in turn */
	while (s->aq == NULL) {
		if (s->mxcurrent == s->mxarraysz) {
			if (s->mxfound)
				query->error = DNS_OK;
			dns_reply(query, IMSG_DNS_HOST_END);
			dnssession_destroy(s);
			return;
		}
		mx = s->mxarray + s->mxcurrent++;
		s->aq = asr_query_host(asr, mx->host, AF_UNSPEC);
	}

	while ((ret = asr_run(s->aq, &ar)) == ASR_YIELD) {
		free(ar.ar_cname);
		memcpy(&query->ss, &ar.ar_sa.sa, ar.ar_sa.sa.sa_len);
		dns_reply(query, IMSG_DNS_HOST);
		s->mxfound++;
	}

	if (ret == ASR_COND) {
		dns_asr_event_set(s, &ar);
		return;
	}

	if (dns_asr_error(ar.ar_err) == DNS_RETRY)
		query->error = DNS_RETRY;

	s->aq  = NULL;
	goto next;
}

static void
dns_asr_dispatch_cname(struct dnssession *s)
{
	struct dns		*query = &s->query;
	struct asr_result	 ar;

	switch (asr_run(s->aq, &ar)) {
	case ASR_COND:
		dns_asr_event_set(s, &ar);
		return;
	case ASR_YIELD:
		/* Only return the first answer */
		query->error = DNS_OK;
		strlcpy(query->host, ar.ar_cname, sizeof (query->host));
		asr_abort(s->aq);
		free(ar.ar_cname);
		break;
	case ASR_DONE:
		query->error = dns_asr_error(ar.ar_err);
		break;
	}
	dns_reply(query, IMSG_DNS_PTR);
	dnssession_destroy(s);
}

static struct dnssession *
dnssession_init(struct dns *query)
{
	struct dnssession *s;

	s = calloc(1, sizeof(struct dnssession));
	if (s == NULL)
		fatal("dnssession_init: calloc");

	stat_increment(STATS_LKA_SESSION);

	s->id = query->id;
	s->query = *query;
	SPLAY_INSERT(dnstree, &dns_sessions, s);
	return (s);
}

static void
dnssession_destroy(struct dnssession *s)
{
	stat_decrement(STATS_LKA_SESSION);
	SPLAY_REMOVE(dnstree, &dns_sessions, s);
	event_del(&s->ev);
	free(s);
}

static void
dnssession_mx_insert(struct dnssession *s, const char *host, int prio)
{
	size_t i, j;

	for (i = 0; i < s->mxarraysz; i++)
		if (prio < s->mxarray[i].prio)
			break;

	if (i == MAX_MX_COUNT)
		return;

	if (s->mxarraysz < MAX_MX_COUNT)
		s->mxarraysz++;

	for (j = s->mxarraysz - 1; j > i; j--)
		s->mxarray[j] = s->mxarray[j - 1];

        s->mxarray[i].prio = prio;
	strlcpy(s->mxarray[i].host, host,
	    sizeof (s->mxarray[i].host));
}

static int
dnssession_cmp(struct dnssession *s1, struct dnssession *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->id < s2->id)
		return (-1);

	if (s1->id > s2->id)
		return (1);

	return (0);
}

SPLAY_GENERATE(dnstree, dnssession, nodes, dnssession_cmp);
