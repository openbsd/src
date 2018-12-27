/* $OpenBSD: rebound.c,v 1.107 2018/12/27 18:00:15 tedu Exp $ */
/*
 * Copyright (c) 2015 Ted Unangst <tedu@openbsd.org>
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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>

#define MINIMUM(a,b) (((a)<(b))?(a):(b))

/*
 * TTL for permanently cached records.
 * They don't expire, but need something to put in the response.
 */
#define CACHETTL 10

uint16_t randomid(void);

int https_init(void);
int https_connect(const char *ip, const char *name);
int https_query(uint8_t *query, size_t qlen, uint8_t *resp, size_t *resplen);

static int https;
static char https_ip[256];
static char https_name[256];

union sockun {
	struct sockaddr a;
	struct sockaddr_storage s;
	struct sockaddr_in i;
	struct sockaddr_in6 i6;
};

static struct timespec now;
static int debug;
static int daemonized;

struct dnspacket {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
	char qname[];
	/* ... */
};
#define NAMELEN 256

/*
 * requests will point to cache entries until a response is received.
 * until then, the request owns the entry and must free it.
 * after the response is set, the request must not free it.
 */
struct dnscache {
	RB_ENTRY(dnscache) expnode;
	RB_ENTRY(dnscache) cachenode;
	struct dnspacket *req;
	size_t reqlen;
	struct dnspacket *resp;
	size_t resplen;
	struct timespec ts;
	struct timespec basetime;
	int permanent;
};
static RB_HEAD(cacheexp, dnscache) cacheexp;
RB_PROTOTYPE_STATIC(cacheexp, dnscache, expnode, expirycmp)
static RB_HEAD(cachetree, dnscache) cachetree;
RB_PROTOTYPE_STATIC(cachetree, dnscache, cachenode, cachecmp)

static int cachecount;
static int cachemax;
static uint64_t cachehits;

/*
 * requests are kept on a fifo list, but only after socket s is set.
 */
struct request {
	int s;
	int client;
	int tcp;
	union sockun from;
	socklen_t fromlen;
	struct timespec ts;
	TAILQ_ENTRY(request) fifo;
	uint16_t clientid;
	uint16_t reqid;
	struct dnscache *cacheent;
	char origname[NAMELEN];
	char newname[NAMELEN];
};
static TAILQ_HEAD(, request) reqfifo;

static int conncount;
static int connmax;
static uint64_t conntotal;
static int stopaccepting;

static void sendreply(struct request *req, uint8_t *buf, size_t r);

void
logmsg(int prio, const char *msg, ...)
{
	va_list ap;

	if (debug || !daemonized) {
		va_start(ap, msg);
		vfprintf(stdout, msg, ap);
		fprintf(stdout, "\n");
		va_end(ap);
	}
	if (!debug) {
		va_start(ap, msg);
		vsyslog(LOG_DAEMON | prio, msg, ap);
		va_end(ap);
	}
}

void __dead
logerr(const char *msg, ...)
{
	va_list ap;

	if (debug || !daemonized) {
		va_start(ap, msg);
		fprintf(stderr, "rebound: ");
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}
	if (!debug) {
		va_start(ap, msg);
		vsyslog(LOG_DAEMON | LOG_ERR, msg, ap);
		va_end(ap);
	}
	exit(1);
}

static int
expirycmp(struct dnscache *c1, struct dnscache *c2)
{
	if (timespeccmp(&c1->ts, &c2->ts, <))
		return -1;
	if (timespeccmp(&c1->ts, &c2->ts, >))
		return 1;
	return c1 < c2 ? -1 : 1;
}
RB_GENERATE_STATIC(cacheexp, dnscache, expnode, expirycmp)

static int
cachecmp(struct dnscache *c1, struct dnscache *c2)
{
	if (c1->reqlen == c2->reqlen)
		return memcmp(c1->req, c2->req, c1->reqlen);
	return c1->reqlen < c2->reqlen ? -1 : 1;
}
RB_GENERATE_STATIC(cachetree, dnscache, cachenode, cachecmp)

static void
lowercase(unsigned char *s, size_t len)
{
	while (len--) {
		*s = tolower(*s);
		s++;
	}
}

static void
randomcase(unsigned char *s, size_t len)
{
	unsigned char bits[NAMELEN / 8], *b;
	u_int i = 0;

	arc4random_buf(bits, (len + 7) / 8);
	b = bits;
	while (len--) {
		*s = (*b & (1 << i)) ? toupper(*s) : tolower(*s);
		s++;
		i++;
		if (i == 8) {
			b++;
			i = 0;
		}
	}
}

static void
freecacheent(struct dnscache *ent)
{
	cachecount -= 1;
	RB_REMOVE(cachetree, &cachetree, ent);
	RB_REMOVE(cacheexp, &cacheexp, ent);
	free(ent->req);
	free(ent->resp);
	free(ent);
}

/*
 * names end with either a nul byte or a two byte 0xc0 pointer
 */
static size_t
dnamelen(const unsigned char *p, size_t len)
{
	size_t n = 0;

	for (n = 0; n < len; n++) {
		if (p[n] == 0)
			return n + 1;
		if ((p[n] & 0xc0) == 0xc0)
			return n + 2;
	}
	return len + 1;
}

static int
adjustttl(struct dnscache *ent)
{
	struct dnspacket *resp = ent->resp;
	char *p = (char *)resp;
	u_int rlen = ent->resplen;
	u_int used = 0;
	uint32_t ttl, cnt, i;
	uint16_t len;
	time_t diff;

	if (ent->permanent)
		return 0;

	diff = now.tv_sec - ent->basetime.tv_sec;
	if (diff <= 0)
		return 0;

	/* checks are redundant; checked when cacheent is created */
	/* skip past packet header */
	used += sizeof(struct dnspacket);
	if (used >= rlen)
		return -1;
	if (ntohs(resp->qdcount) != 1)
		return -1;
	/* skip past query name, type, and class */
	used += dnamelen(p + used, rlen - used);
	used += 2;
	used += 2;
	cnt = ntohs(resp->ancount);
	for (i = 0; i < cnt; i++) {
		if (used >= rlen)
			return -1;
		/* skip past answer name, type, and class */
		used += dnamelen(p + used, rlen - used);
		used += 2;
		used += 2;
		if (used + 4 >= rlen)
			return -1;
		memcpy(&ttl, p + used, 4);
		ttl = ntohl(ttl);
		/* expired */
		if (diff >= ttl)
			return -1;
		ttl -= diff;
		ttl = ntohl(ttl);
		memcpy(p + used, &ttl, 4);
		used += 4;
		if (used + 2 >= rlen)
			return -1;
		memcpy(&len, p + used, 2);
		used += 2;
		used += ntohs(len);
	}
	ent->basetime.tv_sec += diff;
	return 0;
}

static struct dnscache *
cachelookup(struct dnspacket *dnsreq, size_t reqlen, size_t namelen)
{
	struct dnscache *hit, key;
	unsigned char origname[NAMELEN];
	uint16_t origid;

	memcpy(origname, dnsreq->qname, namelen);
	lowercase(dnsreq->qname, namelen);

	origid = dnsreq->id;
	dnsreq->id = 0;

	key.reqlen = reqlen;
	key.req = dnsreq;
	hit = RB_FIND(cachetree, &cachetree, &key);
	if (hit) {
		if (adjustttl(hit) != 0) {
			freecacheent(hit);
			hit = NULL;
		} else
			cachehits += 1;
	}

	memcpy(dnsreq->qname, origname, namelen);
	dnsreq->id = origid;
	return hit;
}

static void
freerequest(struct request *req)
{
	struct dnscache *ent;

	if (req->tcp)
		conncount -= 2;
	else
		conncount -= 1;
	if (req->s != -1) {
		TAILQ_REMOVE(&reqfifo, req, fifo);
		close(req->s);
	}
	if (req->tcp && req->client != -1)
		close(req->client);
	if ((ent = req->cacheent) && !ent->resp) {
		free(ent->req);
		free(ent);
	}
	free(req);
}

static void
servfail(int ud, uint16_t id, struct sockaddr *fromaddr, socklen_t fromlen)
{
	struct dnspacket pkt;

	memset(&pkt, 0, sizeof(pkt));
	pkt.id = id;
	pkt.flags = htons(1 << 15 | 0x2);
	sendto(ud, &pkt, sizeof(pkt), 0, fromaddr, fromlen);
}

static struct request *
newrequest(int ud, struct sockaddr *remoteaddr)
{
	union sockun from;
	socklen_t fromlen;
	struct request *req;
	uint8_t buf[65536];
	struct dnspacket *dnsreq;
	struct dnscache *hit = NULL;
	size_t r;
	size_t namelen = 0;

	dnsreq = (struct dnspacket *)buf;

	fromlen = sizeof(from);
	r = recvfrom(ud, buf, sizeof(buf), 0, &from.a, &fromlen);
	if (r == 0 || r == -1 || r < sizeof(struct dnspacket))
		return NULL;
	if (ntohs(dnsreq->qdcount) == 1) {
		/* some more checking */
		namelen = dnamelen(dnsreq->qname, r - sizeof(struct dnspacket));
		if (namelen > r - sizeof(struct dnspacket))
			return NULL;
		if (namelen > NAMELEN)
			return NULL;
		hit = cachelookup(dnsreq, r, namelen);
	}

	conntotal += 1;
	if (hit) {
		hit->resp->id = dnsreq->id;
		memcpy(hit->resp->qname, dnsreq->qname, namelen);
		sendto(ud, hit->resp, hit->resplen, 0, &from.a, fromlen);
		return NULL;
	}

	if (!(req = calloc(1, sizeof(*req))))
		return NULL;

	conncount += 1;
	req->ts = now;
	req->ts.tv_sec += 30;
	req->s = -1;

	req->client = ud;
	memcpy(&req->from, &from, fromlen);
	req->fromlen = fromlen;

	req->clientid = dnsreq->id;
	req->reqid = randomid();
	dnsreq->id = req->reqid;
	if (ntohs(dnsreq->qdcount) == 1) {
		memcpy(req->origname, dnsreq->qname, namelen);
		randomcase(dnsreq->qname, namelen);
		memcpy(req->newname, dnsreq->qname, namelen);

		hit = calloc(1, sizeof(*hit));
		if (hit) {
			hit->req = malloc(r);
			if (hit->req) {
				memcpy(hit->req, dnsreq, r);
				hit->reqlen = r;
				hit->req->id = 0;
				lowercase(hit->req->qname, namelen);
			} else {
				free(hit);
				hit = NULL;

			}
		}
		req->cacheent = hit;
	}

	if (https) {
		int rv;
		char resp[65536];
		size_t resplen;

		rv = https_query(buf, r, resp, &resplen);
		if (rv != 0) {
			rv = https_connect(https_ip, https_name);
			if (rv == 0)
				rv = https_query(buf, r, buf, &resplen);
		}
		if (rv != 0) {
			logmsg(LOG_NOTICE, "failed to make https query");
			goto fail;
		}
		sendreply(req, resp, resplen);
		freerequest(req);
		return NULL;
	}

	req->s = socket(remoteaddr->sa_family, SOCK_DGRAM, 0);
	if (req->s == -1)
		goto fail;

	TAILQ_INSERT_TAIL(&reqfifo, req, fifo);

	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1) {
		logmsg(LOG_NOTICE, "failed to connect (%d)", errno);
		if (errno == EADDRNOTAVAIL)
			servfail(ud, req->clientid, &from.a, fromlen);
		goto fail;
	}
	if (send(req->s, buf, r, 0) != r) {
		logmsg(LOG_NOTICE, "failed to send (%d)", errno);
		goto fail;
	}

	return req;

fail:
	freerequest(req);
	return NULL;
}

static uint32_t
minttl(struct dnspacket *resp, u_int rlen)
{
	uint32_t minttl = -1, ttl, cnt, i;
	uint16_t len;
	char *p = (char *)resp;
	u_int used = 0;

	/* skip past packet header */
	used += sizeof(struct dnspacket);
	if (used >= rlen)
		return -1;
	if (ntohs(resp->qdcount) != 1)
		return -1;
	/* skip past query name, type, and class */
	used += dnamelen(p + used, rlen - used);
	used += 2;
	used += 2;
	cnt = ntohs(resp->ancount);
	for (i = 0; i < cnt; i++) {
		if (used >= rlen)
			return -1;
		/* skip past answer name, type, and class */
		used += dnamelen(p + used, rlen - used);
		used += 2;
		used += 2;
		if (used + 4 >= rlen)
			return -1;
		memcpy(&ttl, p + used, 4);
		used += 4;
		if (used + 2 >= rlen)
			return -1;
		ttl = ntohl(ttl);
		if (ttl < minttl)
			minttl = ttl;
		memcpy(&len, p + used, 2);
		used += 2;
		used += ntohs(len);
	}
	return minttl;
}

static void
sendreply(struct request *req, uint8_t *buf, size_t r)
{
	struct dnspacket *resp;
	struct dnscache *ent;
	uint32_t ttl;

	resp = (struct dnspacket *)buf;
	if (r == 0 || r == -1 || r < sizeof(struct dnspacket))
		return;
	if (resp->id != req->reqid)
		return;
	resp->id = req->clientid;
	if (ntohs(resp->qdcount) == 1) {
		/* some more checking */
		size_t namelen = dnamelen(resp->qname,
		    r - sizeof(struct dnspacket));
		if (namelen > r - sizeof(struct dnspacket))
			return;
		if (namelen > NAMELEN)
			return;
		if (memcmp(resp->qname, req->newname, namelen) != 0)
			return;
		memcpy(resp->qname, req->origname, namelen);
	}
	sendto(req->client, buf, r, 0, &req->from.a, req->fromlen);
	if ((ent = req->cacheent)) {
		/* check that the response is worth caching */
		ttl = minttl(resp, r);
		if (ttl == -1 || ttl == 0)
			return;
		/*
		 * we do this next, because there's a potential race against
		 * other requests made at the same time. if we lose, abort.
		 * if anything else goes wrong, though, we need to reverse.
		 */
		if (RB_INSERT(cachetree, &cachetree, ent))
			return;
		ent->ts = now;
		ent->ts.tv_sec += MINIMUM(ttl, 300);
		ent->basetime = now;
		ent->resp = malloc(r);
		if (!ent->resp) {
			RB_REMOVE(cachetree, &cachetree, ent);
			return;
		}
		memcpy(ent->resp, buf, r);
		ent->resplen = r;
		if (RB_INSERT(cacheexp, &cacheexp, ent)) {
			free(ent->resp);
			ent->resp = NULL;
			RB_REMOVE(cachetree, &cachetree, ent);
			return;
		}
		cachecount += 1;
	}
}

static void
handlereply(struct request *req)
{
	uint8_t buf[65536];
	size_t r;

	r = recv(req->s, buf, sizeof(buf), 0);
	sendreply(req, buf, r);
}

static struct request *
tcpphasetwo(struct request *req)
{
	int error;
	socklen_t len = sizeof(error);

	req->tcp = 2;

	if (getsockopt(req->s, SOL_SOCKET, SO_ERROR, &error, &len) == -1 ||
	    error != 0)
		goto fail;
	if (setsockopt(req->client, SOL_SOCKET, SO_SPLICE, &req->s,
	    sizeof(req->s)) == -1)
		goto fail;
	if (setsockopt(req->s, SOL_SOCKET, SO_SPLICE, &req->client,
	    sizeof(req->client)) == -1)
		goto fail;

	return req;

fail:
	freerequest(req);
	return NULL;
}

static struct request *
newtcprequest(int ld, struct sockaddr *remoteaddr)
{
	struct request *req;
	int client;

	client = accept(ld, NULL, 0);
	if (client == -1) {
		if (errno == ENFILE || errno == EMFILE)
			stopaccepting = 1;
		return NULL;
	}

	if (!(req = calloc(1, sizeof(*req)))) {
		close(client);
		return NULL;
	}

	conntotal += 1;
	conncount += 2;
	req->ts = now;
	req->ts.tv_sec += 30;
	req->tcp = 1;
	req->client = client;

	req->s = socket(remoteaddr->sa_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (req->s == -1)
		goto fail;

	TAILQ_INSERT_TAIL(&reqfifo, req, fifo);

	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1) {
		if (errno != EINPROGRESS)
			goto fail;
	} else {
		return tcpphasetwo(req);
	}

	return req;

fail:
	freerequest(req);
	return NULL;
}

static size_t
encodename(const char *name, unsigned char *buf)
{
	const char *s = name;
	const char *dot;
	unsigned char len;
	size_t totlen = 0;

	while (*s) {
		dot = strchr(s, '.');
		if (!dot)
			break;
		len = dot - s;
		*buf++ = len;
		memcpy(buf, s, len);
		buf += len;
		totlen += len + 1;
		if (*dot == 0)
			break;
		s = dot + 1;
	}
	*buf = 0;
	totlen += 1;
	return totlen;
}

static int
preloadcache(const char *name, uint16_t type, void *rdata, uint16_t rdatalen)
{
	struct dnspacket *req = NULL, *resp = NULL;
	size_t reqlen, resplen;
	struct dnscache *ent = NULL;
	unsigned char *p;
	uint16_t len, class;
	uint32_t ttl;

	/* header + len + name + type + class */
	reqlen = sizeof(*req) + 1 + strlen(name) + 2 + 2;
	req = malloc(reqlen);
	if (req == NULL)
		goto fail;

	req->id = 0;
	req->flags = htons(0x100);
	req->qdcount = htons(1);
	req->ancount = 0;
	req->nscount = 0;
	req->arcount = 0;
	p = (char *)req + sizeof(*req);
	len = encodename(name, p);
	p += len;
	memcpy(p, &type, 2);
	p += 2;
	class = htons(1); /* IN */
	memcpy(p, &class, 2);

	/* req + name (compressed) + type + class + ttl + len + data */
	resplen = reqlen + 2 + 2 + 2 + 4 + 2 + rdatalen;
	resp = malloc(resplen);
	if (resp == NULL)
		goto fail;
	memcpy(resp, req, reqlen);
	resp->flags = htons(0x100 | 0x8000);	/* response */
	resp->ancount = htons(1);
	p = (char *)resp + reqlen;
	len = htons(sizeof(*req));
	memcpy(p, &len, 2);
	*p |= 0xc0;
	p += 2;
	memcpy(p, &type, 2);
	p += 2;
	memcpy(p, &class, 2);
	p += 2;
	ttl = htonl(CACHETTL);
	memcpy(p, &ttl, 4);
	p += 4;
	len = htons(rdatalen);
	memcpy(p, &len, 2);
	p += 2;
	memcpy(p, rdata, rdatalen);

	ent = calloc(1, sizeof(*ent));
	if (ent == NULL)
		goto fail;
	ent->req = req;
	ent->reqlen = reqlen;
	ent->resp = resp;
	ent->resplen = resplen;
	ent->permanent = 1;

	/* not added to the cacheexp tree */
	RB_INSERT(cachetree, &cachetree, ent);
	return 0;

fail:
	free(req);
	free(resp);
	free(ent);
	return -1;
}

static void
preloadA(const char *name, const char *ip)
{
	struct in_addr in;

	inet_aton(ip, &in);

	if (preloadcache(name, htons(1), &in, 4) == -1)
		logerr("failed to add cache entry for %s", name);
}

static void
preloadPTR(const char *ip, const char *name)
{
	char ipbuf[256];
	char namebuf[256];
	struct in_addr in;

	inet_aton(ip, &in);
	in.s_addr = swap32(in.s_addr);
	snprintf(ipbuf, sizeof(ipbuf), "%s.in-addr.arpa.", inet_ntoa(in));

	encodename(name, namebuf);

	if (preloadcache(ipbuf, htons(12), namebuf, 1 + strlen(namebuf)) == -1)
		logerr("failed to add cache entry for %s", ip);
}

static int
readconfig(int conffd, union sockun *remoteaddr)
{
	const char ns[] = "nameserver";
	const char rc[] = "record";
	const char doh[] = "https";
	char buf[1024];
	char *p;
	struct sockaddr_in *sin = &remoteaddr->i;
	struct sockaddr_in6 *sin6 = &remoteaddr->i6;
	FILE *conf;
	int rv = -1;

	conf = fdopen(conffd, "r");

	while (fgets(buf, sizeof(buf), conf) != NULL) {
		buf[strcspn(buf, "\n")] = '\0';

		if (strncmp(buf, ns, strlen(ns)) == 0) {
			p = buf + strlen(ns) + 1;
			while (isspace((unsigned char)*p))
				p++;

			/* this will not end well */
			if (strcmp(p, "127.0.0.1") == 0)
				continue;

			memset(remoteaddr, 0, sizeof(*remoteaddr));
			if (inet_pton(AF_INET, p, &sin->sin_addr) == 1) {
				sin->sin_len = sizeof(*sin);
				sin->sin_family = AF_INET;
				sin->sin_port = htons(53);
				rv = AF_INET;
			} else if (inet_pton(AF_INET6, p, &sin6->sin6_addr) == 1) {
				sin6->sin6_len = sizeof(*sin6);
				sin6->sin6_family = AF_INET6;
				sin6->sin6_port = htons(53);
				rv = AF_INET6;
			}
		} else if (strncmp(buf, rc, strlen(rc)) == 0) {
			char rectype[16], name[256], value[256];
			p = buf + strlen(rc) + 1;

			memset(rectype, 0, sizeof(rectype));
			sscanf(p, "%15s %255s %255s", rectype, name, value);
			if (strcmp(rectype, "A") == 0) {
				if (strlen(name) < 2 ||
				    name[strlen(name) - 1] != '.') {
					logerr("do not like name %s", name);
					continue;
				}
				preloadA(name, value);
				preloadPTR(value, name);
			}
		} else if (strncmp(buf, doh, strlen(doh)) == 0) {
			p = buf + strlen(doh) + 1;
			if (sscanf(p, "%255s %255s", https_ip, https_name) != 2)
				logerr("do not like https line");
			https = 1;
			if (rv == -1)
				rv = 0;
		}
	}
	fclose(conf);
	return rv;
}

static void
workerinit(void)
{
	struct rlimit rlim;
	struct passwd *pwd;

	if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
		logerr("getrlimit: %s", strerror(errno));
	rlim.rlim_cur = rlim.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rlim) == -1)
		logerr("setrlimit: %s", strerror(errno));
	connmax = rlim.rlim_cur - 10;
	if (connmax > 512)
		connmax = 512;

	cachemax = 10000; /* something big, but not huge */

	TAILQ_INIT(&reqfifo);
	RB_INIT(&cacheexp);
	RB_INIT(&cachetree);

	https_init();

	if (!(pwd = getpwnam("_rebound")))
		logerr("getpwnam failed");

	if (chroot(pwd->pw_dir) == -1)
		logerr("chroot: %s", strerror(errno));
	if (chdir("/") == -1)
		logerr("chdir: %s", strerror(errno));

	setproctitle("worker");
	if (setgroups(1, &pwd->pw_gid) ||
	    setresgid(pwd->pw_gid, pwd->pw_gid, pwd->pw_gid) ||
	    setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid))
		logerr("failed to privdrop");

	if (pledge("stdio inet", NULL) == -1)
		logerr("pledge failed");
}

static void
readevent(int kq, struct kevent *ke, struct sockaddr *remoteaddr,
    int *udpfds, int numudp, int *tcpfds, int numtcp)
{
	struct kevent ch[2];
	struct request *req;
	int i;

	req = ke->udata;
	if (req != NULL) {
		if (req->tcp == 0)
			handlereply(req);
		freerequest(req);
		return;
	}
	for (i = 0; i < numudp; i++) {
		if (ke->ident == udpfds[i]) {
			if ((req = newrequest(ke->ident, remoteaddr))) {
				EV_SET(&ch[0], req->s, EVFILT_READ,
				    EV_ADD, 0, 0, req);
				kevent(kq, ch, 1, NULL, 0, NULL);
			}
			return;
		}
	}
	for (i = 0; i < numtcp; i++) {
		if (ke->ident == tcpfds[i]) {
			if ((req = newtcprequest(ke->ident, remoteaddr))) {
				EV_SET(&ch[0], req->s,
				    req->tcp == 1 ? EVFILT_WRITE :
				    EVFILT_READ, EV_ADD, 0, 0, req);
				kevent(kq, ch, 1, NULL, 0, NULL);
			}
			return;
		}
	}
	logerr("read event on unknown fd");
}

static int
workerloop(int conffd, int udpfds[], int numudp, int tcpfds[], int numtcp)
{
	union sockun remoteaddr;
	struct kevent ch[2];
	struct timespec ts, *timeout = NULL;
	struct request *req;
	struct dnscache *ent;
	int i, r, af, kq;

	kq = kqueue();

	if (!debug) {
		pid_t parent = getppid();
		/* would need pledge(proc) to do this below */
		EV_SET(&ch[0], parent, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
		if (kevent(kq, ch, 1, NULL, 0, NULL) == -1)
			logerr("kevent1: %d", errno);
	}

	workerinit();

	af = readconfig(conffd, &remoteaddr);
	if (af == -1)
		logerr("parse error in config file");

	for (i = 0; i < numudp; i++) {
		EV_SET(&ch[0], udpfds[i], EVFILT_READ, EV_ADD, 0, 0, NULL);
		if (kevent(kq, ch, 1, NULL, 0, NULL) == -1)
			logerr("udp kevent: %d", errno);
	}
	for (i = 0; i < numtcp; i++) {
		EV_SET(&ch[0], tcpfds[i], EVFILT_READ, EV_ADD, 0, 0, NULL);
		if (kevent(kq, ch, 1, NULL, 0, NULL) == -1)
			logerr("tcp kevent: %d", errno);
	}
	EV_SET(&ch[0], SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	EV_SET(&ch[1], SIGUSR1, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, ch, 2, NULL, 0, NULL) == -1)
		logerr("sig kevent: %d", errno);
	logmsg(LOG_INFO, "worker process going to work");
	while (1) {
		struct kevent kev[4];
		r = kevent(kq, NULL, 0, kev, 4, timeout);
		if (r == -1)
			logerr("kevent failed (%d)", errno);

		clock_gettime(CLOCK_MONOTONIC, &now);

		if (stopaccepting) {
			for (i = 0; i < numtcp; i++) {
				EV_SET(&ch[0], tcpfds[i], EVFILT_READ,
				    EV_ADD, 0, 0, NULL);
				kevent(kq, ch, 1, NULL, 0, NULL);
			}
			stopaccepting = 0;
		}

		for (i = 0; i < r; i++) {
			struct kevent *ke = &kev[i];
			switch (ke->filter) {
			case EVFILT_SIGNAL:
				if (ke->ident == SIGHUP) {
					logmsg(LOG_INFO, "hupped, exiting");
					exit(0);
				} else {
					logmsg(LOG_INFO, "connection stats: "
					    "%d active, %llu total",
					    conncount, conntotal);
					logmsg(LOG_INFO, "cache stats: "
					    "%d active, %llu hits",
					    cachecount, cachehits);
				}
				break;
			case EVFILT_PROC:
				logmsg(LOG_INFO, "parent died");
				exit(0);
				break;
			case EVFILT_WRITE:
				req = ke->udata;
				req = tcpphasetwo(req);
				if (req) {
					EV_SET(&ch[0], req->s, EVFILT_WRITE,
					    EV_DELETE, 0, 0, NULL);
					EV_SET(&ch[1], req->s, EVFILT_READ,
					    EV_ADD, 0, 0, req);
					kevent(kq, ch, 2, NULL, 0, NULL);
				}
				break;
			case EVFILT_READ:
				readevent(kq, ke, &remoteaddr.a, udpfds,
				    numudp, tcpfds, numtcp);
				break;
			default:
				logerr("don't know what happened");
				break;
			}
		}

		timeout = NULL;

		if (stopaccepting) {
			for (i = 0; i < numtcp; i++) {
				EV_SET(&ch[0], tcpfds[i], EVFILT_READ,
				    EV_DELETE, 0, 0, NULL);
				kevent(kq, ch, 1, NULL, 0, NULL);
			}
			memset(&ts, 0, sizeof(ts));
			/* one second added below */
			timeout = &ts;
		}

		while (conncount > connmax)
			freerequest(TAILQ_FIRST(&reqfifo));
		while (cachecount > cachemax)
			freecacheent(RB_MIN(cacheexp, &cacheexp));

		/* burn old cache entries */
		while ((ent = RB_MIN(cacheexp, &cacheexp))) {
			if (timespeccmp(&ent->ts, &now, <=))
				freecacheent(ent);
			else
				break;
		}
		if (ent) {
			timespecsub(&ent->ts, &now, &ts);
			timeout = &ts;
		}

		/* burn stalled requests */
		while ((req = TAILQ_FIRST(&reqfifo))) {
			if (timespeccmp(&req->ts, &now, <=))
				freerequest(req);
			else
				break;
		}
		if (req && (!ent || timespeccmp(&req->ts, &ent->ts, <=))) {
			timespecsub(&req->ts, &now, &ts);
			timeout = &ts;
		}
		/* one second grace to avoid spinning */
		if (timeout)
			timeout->tv_sec += 1;

	}
	/* not reached */
	exit(1);
}

static int
openconfig(const char *confname, int kq)
{
	struct kevent kev;
	int conffd;

	conffd = open(confname, O_RDONLY);
	if (conffd == -1)
		logerr("failed to open config %s", confname);
	if (kq != -1) {
		EV_SET(&kev, conffd, EVFILT_VNODE, EV_ADD,
		    NOTE_DELETE | NOTE_ATTRIB, 0, NULL);
		kevent(kq, &kev, 1, NULL, 0, NULL);
	}
	return conffd;
}

static pid_t
reexec(int conffd, int *udpfds, int numudp, int *tcpfds, int numtcp)
{
	pid_t child;
	char **argv;
	int argc;
	int i;

	if ((child = fork()) == -1)
		logerr("failed to fork");
	if (child != 0)
		return child;

	/*   = rebound -W -- -c conffd [-u udpfd]    [-t tcpfd]    NULL */
	argc = 1       +1 +1 +1 +1     +(2 * numudp) +(2 * numtcp) +1;
	argv = reallocarray(NULL, argc, sizeof(char *));
	if (!argv)
		logerr("out of memory building argv");
	argc = 0;
	argv[argc++] = "rebound";
	argv[argc++] = "-W";
	argv[argc++] = "--";
	argv[argc++] = "-c";
	if (asprintf(&argv[argc++], "%d", conffd) == -1)
		logerr("out of memory building argv");
	for (i = 0; i < numudp; i++) {
		argv[argc++] = "-u";
		if (asprintf(&argv[argc++], "%d", udpfds[i]) == -1)
			logerr("out of memory building argv");
	}
	for (i = 0; i < numtcp; i++) {
		argv[argc++] = "-t";
		if (asprintf(&argv[argc++], "%d", tcpfds[i]) == -1)
			logerr("out of memory building argv");
	}
	argv[argc++] = NULL;

	execv("/usr/sbin/rebound", argv);
	logerr("re-exec failed");
}

static int
monitorloop(int *udpfds, int numudp, int *tcpfds, int numtcp,
    const char *confname)
{
	pid_t child;
	struct kevent kev;
	int r, kq;
	int conffd = -1;
	struct timespec ts, *timeout = NULL;

	if (unveil(confname, "r") == -1)
		err(1, "unveil");

	if (unveil("/usr/sbin/rebound", "x") == -1)
		err(1, "unveil");

	if (pledge("stdio rpath proc exec", NULL) == -1)
		err(1, "pledge");

	kq = kqueue();

	/* catch these signals with kevent */
	signal(SIGHUP, SIG_IGN);
	EV_SET(&kev, SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	kevent(kq, &kev, 1, NULL, 0, NULL);
	signal(SIGTERM, SIG_IGN);
	EV_SET(&kev, SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	kevent(kq, &kev, 1, NULL, 0, NULL);
	while (1) {
		int hupped = 0;
		int childdead = 0;

		if (conffd == -1)
			conffd = openconfig(confname, kq);

		child = reexec(conffd, udpfds, numudp, tcpfds, numtcp);

		/* monitor child */
		EV_SET(&kev, child, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
		kevent(kq, &kev, 1, NULL, 0, NULL);

		/* wait for something to happen: HUP or child exiting */
		timeout = NULL;
		while (1) {
			r = kevent(kq, NULL, 0, &kev, 1, timeout);
			if (r == -1)
				logerr("kevent failed (%d)", errno);
			if (r == 0) {
				/* timeout expired */
				logerr("child died without HUP");
			}
			switch (kev.filter) {
			case EVFILT_VNODE:
				/* config file changed */
				logmsg(LOG_INFO, "config changed, reloading");
				sleep(1);
				raise(SIGHUP);
				break;
			case EVFILT_SIGNAL:
				if (kev.ident == SIGHUP) {
					/* signaled. kill child. */
					logmsg(LOG_INFO,
					    "received HUP, restarting");
					close(conffd);
					conffd = -1;
					hupped = 1;
					if (childdead)
						goto doublebreak;
					kill(child, SIGHUP);
				} else if (kev.ident == SIGTERM) {
					/* good bye */
					logmsg(LOG_INFO,
					    "received TERM, quitting");
					kill(child, SIGTERM);
					exit(0);
				}
				break;
			case EVFILT_PROC:
				/* child died. wait for our own HUP. */
				logmsg(LOG_INFO, "observed child exit");
				childdead = 1;
				if (hupped)
					goto doublebreak;
				memset(&ts, 0, sizeof(ts));
				ts.tv_sec = 1;
				timeout = &ts;
				break;
			default:
				logerr("don't know what happened");
				break;
			}
		}
doublebreak:
		while (waitpid(child, NULL, 0) == -1) {
			if (errno != EINTR)
				break;
		}
	}
	return 1;
}

static void
addfd(int fd, int **fdp, int *numfds)
{
	int *fds = *fdp;

	fds = reallocarray(fds, *numfds + 1, sizeof(int));
	if (fds == NULL)
		logerr("failed to allocate port array");
	fds[*numfds] = fd;
	*numfds += 1;
	*fdp = fds;
}

static int
argtofd(const char *arg)
{
	const char *errstr;
	int n;

	n = strtonum(arg, 0, 512, &errstr);
	if (errstr)
		logerr("invalid fd in argv");
	return n;
}


static void __dead
usage(void)
{
	fprintf(stderr, "usage: rebound [-d] [-c config] [-l address]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	union sockun bindaddr;
	int *udpfds = NULL;
	int numudp = 0;
	int *tcpfds = NULL;
	int numtcp = 0;
	int ch, fd;
	int one = 1;
	int worker = 0;
	const char *confname = "/etc/resolv.conf";
	const char *bindname = "127.0.0.1";

	tzset();
	openlog("rebound", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	while ((ch = getopt(argc, argv, "c:dl:W")) != -1) {
		switch (ch) {
		case 'c':
			confname = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		case 'l':
			bindname = optarg;
			break;
		case 'W':
			daemonized = 1;
			worker = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (worker) {
		int conffd;

		/* rewind "--" argument */
		argv--;
		argc++;

		optreset = optind = 1;
		while ((ch = getopt(argc, argv, "c:t:u:")) != -1) {
			switch (ch) {
			case 'c':
				conffd = argtofd(optarg);
				break;
			case 't':
				fd = argtofd(optarg);
				addfd(fd, &tcpfds, &numtcp);
				break;
			case 'u':
				fd = argtofd(optarg);
				addfd(fd, &udpfds, &numudp);
				break;
			default:
				usage();
				break;
			}
		}
		argv += optind;
		argc -= optind;
		if (argc)
			logerr("extraneous arguments for worker");

		return workerloop(conffd, udpfds, numudp, tcpfds, numtcp);
	}

	if (argc)
		usage();

	closefrom(3);

	memset(&bindaddr, 0, sizeof(bindaddr));
	bindaddr.i.sin_len = sizeof(bindaddr.i);
	bindaddr.i.sin_family = AF_INET;
	bindaddr.i.sin_port = htons(53);
	inet_aton(bindname, &bindaddr.i.sin_addr);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		logerr("socket: %s", strerror(errno));
	if (bind(fd, &bindaddr.a, bindaddr.a.sa_len) == -1)
		logerr("bind: %s", strerror(errno));
	addfd(fd, &udpfds, &numudp);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		logerr("socket: %s", strerror(errno));
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (bind(fd, &bindaddr.a, bindaddr.a.sa_len) == -1)
		logerr("bind: %s", strerror(errno));
	if (listen(fd, 10) == -1)
		logerr("listen: %s", strerror(errno));
	addfd(fd, &tcpfds, &numtcp);

	memset(&bindaddr, 0, sizeof(bindaddr));
	bindaddr.i6.sin6_len = sizeof(bindaddr.i6);
	bindaddr.i6.sin6_family = AF_INET6;
	bindaddr.i6.sin6_port = htons(53);
	bindaddr.i6.sin6_addr = in6addr_loopback;

	fd = socket(AF_INET6, SOCK_DGRAM, 0);
	if (fd == -1)
		logerr("socket: %s", strerror(errno));
	if (bind(fd, &bindaddr.a, bindaddr.a.sa_len) == -1)
		logerr("bind: %s", strerror(errno));
	addfd(fd, &udpfds, &numudp);

	fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd == -1)
		logerr("socket: %s", strerror(errno));
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (bind(fd, &bindaddr.a, bindaddr.a.sa_len) == -1)
		logerr("bind: %s", strerror(errno));
	if (listen(fd, 10) == -1)
		logerr("listen: %s", strerror(errno));
	addfd(fd, &tcpfds, &numtcp);

	if (debug) {
		int conffd = openconfig(confname, -1);
		return workerloop(conffd, udpfds, numudp, tcpfds, numtcp);
	}

	if (daemon(0, 0) == -1)
		logerr("daemon: %s", strerror(errno));
	daemonized = 1;

	return monitorloop(udpfds, numudp, tcpfds, numtcp, confname);
}
