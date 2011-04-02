/*	$OpenBSD: dns.c,v 1.39 2011/04/02 16:40:19 eric Exp $	*/

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

struct dnssession *dnssession_init(struct smtpd *, struct dns *);
void	dnssession_destroy(struct smtpd *, struct dnssession *);
void	dnssession_mx_insert(struct dnssession *, const char *, int);
void	dns_asr_event_set(struct dnssession *, struct asr_result *);
void	dns_asr_handler(int, short, void *);
void	dns_asr_dispatch_host(struct dnssession *);
void	dns_asr_dispatch_mx(struct dnssession *);
void	dns_asr_dispatch_cname(struct dnssession *);

struct asr *asr = NULL;

/*
 * User interface.
 */

void
dns_query_host(struct smtpd *env, char *host, int port, u_int64_t id)
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
dns_query_mx(struct smtpd *env, char *host, int port, u_int64_t id)
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
dns_query_ptr(struct smtpd *env, struct sockaddr_storage *ss, u_int64_t id)
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
dns_async(struct smtpd *env, struct imsgev *asker, int type, struct dns *query)
{
	struct dnssession *dnssession;

	if (asr == NULL && (asr = asr_resolver(NULL)) == NULL) {
		log_warnx("dns_async: cannot create resolver");
		goto noasr;
	}

	query->env   = env;
	query->type  = type;
	query->asker = asker;
	dnssession = dnssession_init(env, query);

	switch (type) {
	case IMSG_DNS_HOST:
		dnssession_mx_insert(dnssession, query->host, 0);
		env->stats->lka.queries_host++;
		dns_asr_dispatch_host(dnssession);
		return;
	case IMSG_DNS_PTR:
		dnssession->aq = asr_query_cname(asr,
		    (struct sockaddr*)&query->ss, query->ss.ss_len);
		env->stats->lka.queries_cname++;
		if (dnssession->aq == NULL) {
			log_debug("dns_async: asr_query_cname error");
			break;
		}
		dns_asr_dispatch_cname(dnssession);
		return;
	case IMSG_DNS_MX:
		dnssession->aq = asr_query_dns(asr, T_MX, C_IN, query->host, 0);
		env->stats->lka.queries_mx++;
		if (dnssession->aq == NULL) {
			log_debug("dns_async: asr_query_dns error");
			break;
		}
		dns_asr_dispatch_mx(dnssession);
		return;
	default:
		log_debug("dns_async: bad request");
		break;
	}

	env->stats->lka.queries_failure++;
	dnssession_destroy(env, dnssession);
noasr:
	query->error = EAI_AGAIN;
	if (type != IMSG_DNS_PTR)
		type = IMSG_DNS_HOST_END;
	imsg_compose_event(asker, type, 0, 0, -1, query, sizeof(*query));
}

void
dns_asr_event_set(struct dnssession *dnssession, struct asr_result *ar)
{
	struct timeval tv = { 0, 0 };
	
	tv.tv_usec = ar->ar_timeout * 1000;
	event_set(&dnssession->ev, ar->ar_fd,
	    ar->ar_cond == ASR_READ ? EV_READ : EV_WRITE,
	    dns_asr_handler, dnssession);
	event_add(&dnssession->ev, &tv);
}

void
dns_asr_handler(int fd, short event, void *arg)
{
	struct dnssession *dnssession = arg;

	switch(dnssession->query.type) {
	case IMSG_DNS_HOST:
		dns_asr_dispatch_host(dnssession);
		break;
	case IMSG_DNS_PTR:
		dns_asr_dispatch_cname(dnssession);
		break;
	case IMSG_DNS_MX:
		dns_asr_dispatch_mx(dnssession);
		break;
	default:
		fatalx("bad query type");
	}
}

void
dns_asr_dispatch_mx(struct dnssession *dnssession)
{
	struct dns		*query = &dnssession->query;
	struct asr_result	 ar;
	struct packed		 pack;
	struct header		 h;
	struct query		 q;
	struct rr		 rr;
	char			 buf[512];

	if (asr_run(dnssession->aq, &ar) == ASR_COND) {
		dns_asr_event_set(dnssession, &ar);
		return;
	}

	if (ar.ar_err)
		goto hosts;  /* empty list */

	packed_init(&pack, ar.ar_data, ar.ar_datalen);
	unpack_header(&pack, &h);
	unpack_query(&pack, &q);

	if (h.ancount == 0)
		/* fallback to host if no MX is found. */
		dnssession_mx_insert(dnssession, query->host, 0);

	for (; h.ancount; h.ancount--) {
		unpack_rr(&pack, &rr);
		print_dname(rr.rr.mx.exchange, buf, sizeof(buf));
		buf[strlen(buf) - 1] = '\0';
		dnssession_mx_insert(dnssession, buf, rr.rr.mx.preference);
	}

	free(ar.ar_data);
	ar.ar_data = NULL;

hosts:
	/* Now we have a sorted list of MX to resolve. Simply "turn" this
	 * MX session into a regular host session.
	 */
	dnssession->aq = NULL;
	dnssession->query.type = IMSG_DNS_HOST;
	dns_asr_dispatch_host(dnssession);
}

void
dns_asr_dispatch_host(struct dnssession *dnssession)
{
	struct dns		*query = &dnssession->query;
	struct mx		*mx;
	struct asr_result	 ar;
	int			 ret;

next:
	/* query all listed hosts in turn */
	while (dnssession->aq == NULL) {
		if (dnssession->mxcurrent == dnssession->mxarraysz) {
			query->error = (dnssession->mxfound) ? 0 : EAI_NONAME;
			if (query->error)
				query->env->stats->lka.queries_failure++;
			imsg_compose_event(query->asker, IMSG_DNS_HOST_END, 0,
			    0, -1, query, sizeof(*query));
			dnssession_destroy(query->env, dnssession);
			return;
		}
		mx = dnssession->mxarray + dnssession->mxcurrent++;
		dnssession->aq = asr_query_host(asr, mx->host, AF_UNSPEC);
	}

	while ((ret = asr_run(dnssession->aq, &ar)) == ASR_YIELD) {
		free(ar.ar_cname);
		memcpy(&query->ss, &ar.ar_sa.sa, ar.ar_sa.sa.sa_len);
		query->error = 0;
		imsg_compose_event(query->asker, IMSG_DNS_HOST, 0, 0, -1, query,
		    sizeof(*query));
		dnssession->mxfound++;
	}

	if (ret == ASR_COND) {
		dns_asr_event_set(dnssession, &ar);
		return;
	}

	dnssession->aq  = NULL;
	goto next;
}

void
dns_asr_dispatch_cname(struct dnssession *dnssession)
{
	struct dns		*query = &dnssession->query;
	struct asr_result	 ar;

	switch (asr_run(dnssession->aq, &ar)) {
	case ASR_COND:
		dns_asr_event_set(dnssession, &ar);
		return;
	case ASR_YIELD:
		/* Only return the first answer */
		query->error = 0;
		strlcpy(query->host, ar.ar_cname, sizeof (query->host));
		asr_abort(dnssession->aq);
		free(ar.ar_cname);
		break;
	case ASR_DONE:
		/* This is necessarily an error */
		query->env->stats->lka.queries_failure++;
		query->error = ar.ar_err;
		break;
	}
	imsg_compose_event(query->asker, IMSG_DNS_PTR, 0, 0, -1, query,
	    sizeof(*query));
	dnssession_destroy(query->env, dnssession);
}

struct dnssession *
dnssession_init(struct smtpd *env, struct dns *query)
{
	struct dnssession *dnssession;

	dnssession = calloc(1, sizeof(struct dnssession));
	if (dnssession == NULL)
		fatal("dnssession_init: calloc");

	env->stats->lka.queries++;
	env->stats->lka.queries_active++;
	if (env->stats->lka.queries_active > env->stats->lka.queries_maxactive)
		env->stats->lka.queries_maxactive = \
		    env->stats->lka.queries_active;

	dnssession->id = query->id;
	dnssession->query = *query;
	SPLAY_INSERT(dnstree, &env->dns_sessions, dnssession);
	return dnssession;
}

void
dnssession_destroy(struct smtpd *env, struct dnssession *dnssession)
{
	env->stats->lka.queries_active--;
	SPLAY_REMOVE(dnstree, &env->dns_sessions, dnssession);
	event_del(&dnssession->ev);
	free(dnssession);
}

void
dnssession_mx_insert(struct dnssession *dnssession, const char *host, int prio)
{
	size_t i, j;

	for (i = 0; i < dnssession->mxarraysz; i++)
		if (prio < dnssession->mxarray[i].prio)
			break;

	if (i == MAX_MX_COUNT)
		return;

	if (dnssession->mxarraysz < MAX_MX_COUNT)
		dnssession->mxarraysz++;

	for (j = dnssession->mxarraysz - 1; j > i; j--)
		dnssession->mxarray[j] = dnssession->mxarray[j - 1];

        dnssession->mxarray[i].prio = prio;
	strlcpy(dnssession->mxarray[i].host, host,
	    sizeof (dnssession->mxarray[i].host));
}

int
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
