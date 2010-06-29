/*	$OpenBSD: dns.c,v 1.22 2010/06/29 03:47:24 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <sys/queue.h>
#include <sys/tree.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <event.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

struct resdata {
	struct imsgev	 iev;
	struct imsgev	*asker;
};

struct mx {
	char	host[MAXHOSTNAMELEN];
	double	prio;
};

void		 parent_dispatch_dns(int, short, void *);

int		 dns(void);
void		 dns_dispatch_parent(int, short, void *);
void		 lookup_a(struct imsgev *, struct dns *, int, int);
void		 lookup_mx(struct imsgev *, struct dns *);
int		 get_mxlist(char *, char *, struct dns **);
void		 free_mxlist(struct dns *);
int		 mxcmp(const void *, const void *);
void		 lookup_ptr(struct imsgev *, struct dns *);

/*
 * User interface.
 */

void
dns_query_a(struct smtpd *env, char *host, int port, u_int64_t id)
{
	struct dns	 query;

	bzero(&query, sizeof(query));
	strlcpy(query.host, host, sizeof(query.host));
	query.port = port;
	query.id = id;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_DNS_A, 0, 0, -1, &query,
	    sizeof(query));
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

/*
 * Parent resolver process interface.
 */

void
dns_async(struct smtpd *env, struct imsgev *asker, int type, struct dns *query)
{
	struct resdata	*rd;
	int		 fd;

	if ((rd = calloc(1, sizeof(*rd))) == NULL)
		fatal(NULL);

	rd->asker = asker;
	query->env = env;

	fd = dns();
	imsg_init(&rd->iev.ibuf, fd);
	rd->iev.handler = parent_dispatch_dns;
	rd->iev.events = EV_READ;
	rd->iev.data = rd;
	event_set(&rd->iev.ev, rd->iev.ibuf.fd, rd->iev.events, rd->iev.handler,
	    rd->iev.data);
	event_add(&rd->iev.ev, NULL);

	imsg_compose_event(&rd->iev, type, 0, 0, -1, query, sizeof(*query));
}

void
parent_dispatch_dns(int sig, short event, void *p)
{
	struct resdata		*rd = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = &rd->iev;
	ibuf = &rd->iev.ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0)
			fatal("parent_dispatch_dns: pipe closed");
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("parent_dispatch_dns: msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_dns: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DNS_A:
			imsg_compose_event(rd->asker, IMSG_DNS_A, 0, 0, -1, imsg.data,
			    sizeof(struct dns));
			break;

		case IMSG_DNS_A_END:
		case IMSG_DNS_PTR:
			imsg_compose_event(rd->asker, imsg.hdr.type, 0, 0, -1,
			    imsg.data, sizeof(struct dns));
			close(ibuf->fd);
			event_del(&iev->ev);
			free(rd);
			imsg_free(&imsg);
			return;

		default:
			log_warnx("parent_dispatch_dns: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_dns: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

/*
 * Helper resolver process.
 */

int
dns(void)
{
	int		 fd[2];
	pid_t		 pid;
	struct imsgev	*iev;

	if (socketpair(AF_UNIX, SOCK_STREAM, AF_UNSPEC, fd) == -1)
		fatal("socketpair");

	session_socket_blockmode(fd[0], BM_NONBLOCK);
	session_socket_blockmode(fd[1], BM_NONBLOCK);

	if ((pid = fork()) == -1)
		fatal("dns: fork");
	if (pid > 0) {
		close(fd[1]);
		return (fd[0]);
	}
	close(fd[0]);

	event_base_free(NULL);
	event_init();

	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	if ((iev = calloc(1, sizeof(*iev))) == NULL)
		fatal(NULL);
	imsg_init(&iev->ibuf, fd[1]);
	iev->handler = dns_dispatch_parent;
	iev->events = EV_READ;
	iev->data = iev;
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev->data);
	event_add(&iev->ev, NULL);

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	_exit(0);
}

void
dns_dispatch_parent(int sig, short event, void *p)
{
	struct imsgev		*iev = p;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("dns_dispatch_parent: msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("dns_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DNS_A:
			lookup_a(iev, imsg.data, 0, 1);
			break;

		case IMSG_DNS_MX:
			lookup_mx(iev, imsg.data);
			break;

		case IMSG_DNS_PTR:
			lookup_ptr(iev, imsg.data);
			break;

		default:
			log_warnx("dns_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("dns_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
lookup_a(struct imsgev *iev, struct dns *query, int numeric, int finalize)
{
	struct addrinfo	*res0, *res, hints;
	char		*port = NULL;

	log_debug("lookup_a %s:%d%s", query->host, query->port,
	    numeric ? " (numeric)" : "");

	if (query->port && asprintf(&port, "%u", query->port) == -1)
		fatal(NULL);
	
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (numeric)
		hints.ai_flags = AI_NUMERICHOST;

	query->error = getaddrinfo(query->host, port, &hints, &res0);
	if (query->error)
		goto end;

	for (res = res0; res; res = res->ai_next) {
		memcpy(&query->ss, res->ai_addr, res->ai_addr->sa_len);
		imsg_compose_event(iev, IMSG_DNS_A, 0, 0, -1, query, sizeof(*query));
	}
	freeaddrinfo(res0);
end:
	free(port);
	log_debug("lookup_a %s", query->error ? "failed" : "success");
	if (finalize)
		imsg_compose_event(iev, IMSG_DNS_A_END, 0, 0, -1, query,
		    sizeof(*query));
}

void
lookup_mx(struct imsgev *iev, struct dns *query)
{
	struct dns	*mx0, *mx;
	int		 success = 0;

	log_debug("lookup_mx %s", query->host);

	/* if ip address, skip MX lookup */
	/* XXX: maybe do it just once in parse.y? */
	lookup_a(iev, query, 1, 0);
	if (!query->error)
		goto end;

	query->error = get_mxlist(query->host, query->env->sc_hostname, &mx0);
	if (query->error)
		goto end;

	if (mx0 == NULL) {
		log_debug("implicit mx");
		if ((mx0 = calloc(1, sizeof(*mx0))) == NULL)
			fatal(NULL);
		strlcpy(mx0->host, query->host, sizeof(mx0->host));
	}

	for (mx = mx0; mx; mx = mx->next) {
		mx->port = query->port;
		mx->id = query->id;
		lookup_a(iev, mx, 0, 0);
		if (!mx->error)
			success++;
	}
	free_mxlist(mx0);

	if (success == 0)
		query->error = EAI_NODATA;

end:
	log_debug("lookup_mx %s", query->error ? "failed" : "success");
	imsg_compose_event(iev, IMSG_DNS_A_END, 0, 0, -1, query, sizeof(*query));
}

int
get_mxlist(char *host, char *self, struct dns **res)
{
	struct mx	 tab[MAX_MX_COUNT];
	unsigned char	 *p, *endp;
	int		 ntab, i, ret, type, n, maxprio, cname_ok = 3;
	int		 qdcount, ancount;
	union {
		HEADER	 hdr;
		char	 buf[PACKETSZ];
	} answer;
again:
	ntab = 0;
	maxprio = 16384;
	ret = res_query(host, C_IN, T_MX, answer.buf, sizeof(answer.buf));
	if (ret < 0) {
		switch (h_errno) {
		case TRY_AGAIN:
			return (EAI_AGAIN);
		case HOST_NOT_FOUND:
			return (EAI_NONAME);
		case NO_RECOVERY:
			return (EAI_FAIL);
		case NO_DATA:
			*res = NULL;
			return (0);
		}
		fatal("get_mxlist: res_query");
	}

	p = answer.buf + HFIXEDSZ;
	endp = answer.buf + ret;
	qdcount = ntohs(((HEADER *)answer.buf)->qdcount);
	ancount = ntohs(((HEADER *)answer.buf)->ancount);

	if (qdcount < 1)
		return (EAI_FAIL);
	for (i = 0; i < qdcount; i++) {
		ret = dn_skipname(p, endp);
		if (ret < 0)
			return (EAI_FAIL);
		p += ret + QFIXEDSZ;
	}

	while (p < endp && ntab < ancount && ntab < MAX_MX_COUNT) {
		ret = dn_skipname(p, endp);
		if (ret < 0)
			return (EAI_FAIL);
		p += ret;

		GETSHORT(type, p);
		p += sizeof(u_int16_t) + sizeof(u_int32_t);
		GETSHORT(n, p);

		if (type == T_CNAME) {
			if (cname_ok-- == 0)
				return (EAI_FAIL);
			ret = dn_expand(answer.buf, endp, p, tab[0].host,
			    sizeof(tab[0].host));
			if (ret < 0)
				return (EAI_FAIL);
			host = tab[0].host;
			goto again;
		}

		if (type != T_MX) {
			log_warnx("get_mxlist: %s: bad rr type %d", host, type);
			p += n;
			continue;
		}

		GETSHORT(tab[ntab].prio, p);

		ret = dn_expand(answer.buf, endp, p, tab[ntab].host,
		    sizeof(tab[ntab].host));
		if (ret < 0)
			return (EAI_FAIL);
		p += ret;

		/*
		 * In case our name is listed as MX, prevent loops by excluding
		 * all hosts of our or greater preference number.
		 */
		if (strcmp(self, tab[ntab].host) == 0)
			maxprio = tab[ntab].prio;

		ntab++;
	}

	/*
	 * Randomize equal preference hosts using the fractional part.
	 */
	for (i = 0; i < ntab; i++)
		tab[i].prio += (double)arc4random_uniform(ntab) / ntab;

	qsort(tab, ntab, sizeof(struct mx), mxcmp);

	for (i = 0; i < ntab; i++) {
		log_debug("mx %s prio %f", tab[i].host, tab[i].prio);
		if (tab[i].prio >= maxprio)
			break;
		if ((*res = calloc(1, sizeof(struct dns))) == NULL)
			fatal(NULL);
		strlcpy((*res)->host, tab[i].host, sizeof((*res)->host));
		res = &(*res)->next;
	}

	if (i == 0)
		return (EAI_FAIL);

	return (0);
}

void
free_mxlist(struct dns *first)
{
	struct dns	*mx, *next;

	for (mx = first; mx; mx = next) {
		next = mx->next;
		free(mx);
	}
}

int
mxcmp(const void *va, const void *vb)
{
	const struct mx	*a = va;
	const struct mx	*b = vb;

	if (a->prio > b->prio)
		return (1);
	else if (a->prio < b->prio)
		return (-1);
	else
		return (0);
}

void
lookup_ptr(struct imsgev *iev, struct dns *query)
{
	struct addrinfo	*res, hints;

	log_debug("lookup_ptr %s", ss_to_text(&query->ss));

	query->error = getnameinfo((struct sockaddr *)&query->ss,
	    query->ss.ss_len, query->host, sizeof(query->host), NULL, 0,
	    NI_NAMEREQD);
	if (query->error)
		goto end;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(query->host, NULL, &hints, &res) == 0) {
		query->error = EAI_NODATA;
		freeaddrinfo(res);
	}
end:
	log_debug("lookup_ptr %s", query->error ? "failed" : "success");
	imsg_compose_event(iev, IMSG_DNS_PTR, 0, 0, -1, query, sizeof(*query));
}
