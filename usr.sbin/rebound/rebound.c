/* $OpenBSD: rebound.c,v 1.22 2015/10/16 18:38:53 tedu Exp $ */
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
#include <sys/event.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>

uint16_t randomid(void);

static struct timespec now;
static int debug;

struct dnspacket {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
	/* ... */
};

struct dnsrr {
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdatalen;
	/* ... */
};

/*
 * requests will point to cache entries until a response is received.
 * until then, the request owns the entry and must free it.
 * after it's on the list, the request must not free it.
 */

struct dnscache {
	TAILQ_ENTRY(dnscache) cache;
	struct dnspacket *req;
	size_t reqlen;
	struct dnspacket *resp;
	size_t resplen;
	struct timespec ts;
};
static TAILQ_HEAD(, dnscache) cache;

struct request {
	int s;
	int client;
	int phase;
	struct sockaddr from;
	socklen_t fromlen;
	struct timespec ts;
	TAILQ_ENTRY(request) fifo;
	uint16_t clientid;
	uint16_t reqid;
	struct dnscache *cacheent;
};
static TAILQ_HEAD(, request) reqfifo;


static void
logmsg(int prio, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (debug) {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	} else {
		vsyslog(LOG_DAEMON | prio, msg, ap);
	}
	va_end(ap);
}

static void __dead
logerr(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (debug) {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	} else {
		vsyslog(LOG_DAEMON | LOG_ERR, msg, ap);
	}
	va_end(ap);
	exit(1);
}

static struct dnscache *
cachelookup(struct dnspacket *dnsreq, size_t reqlen)
{
	struct dnscache *hit;
	uint16_t origid;

	origid = dnsreq->id;
	dnsreq->id = 0;
	TAILQ_FOREACH(hit, &cache, cache) {
		if (hit->reqlen == reqlen &&
		    memcmp(hit->req, dnsreq, reqlen) == 0)
			break;
	}
	dnsreq->id = origid;
	return hit;
}

static struct request *
newrequest(int ud, struct sockaddr *remoteaddr)
{
	struct sockaddr from;
	socklen_t fromlen;
	struct request *req;
	uint8_t buf[65536];
	struct dnspacket *dnsreq;
	struct dnscache *hit;
	size_t r;

	dnsreq = (struct dnspacket *)buf;

	fromlen = sizeof(from);
	r = recvfrom(ud, buf, sizeof(buf), 0, &from, &fromlen);
	if (r == 0 || r == -1 || r < sizeof(struct dnspacket))
		return NULL;

	if ((hit = cachelookup(dnsreq, r))) {
		hit->resp->id = dnsreq->id;
		sendto(ud, hit->resp, hit->resplen, 0, &from, fromlen);
		return NULL;
	}

	if (!(req = calloc(1, sizeof(*req))))
		return NULL;

	req->ts = now;
	req->ts.tv_sec += 30;

	req->client = -1;
	memcpy(&req->from, &from, fromlen);
	req->fromlen = fromlen;

	req->clientid = dnsreq->id;
	req->reqid = randomid();
	dnsreq->id = req->reqid;

	hit = calloc(1, sizeof(*hit));
	if (hit) {
		hit->req = malloc(r);
		if (hit->req) {
			memcpy(hit->req, dnsreq, r);
			hit->reqlen = r;
			hit->req->id = 0;
		} else {
			free(hit);
			hit = NULL;

		}
	}
	req->cacheent = hit;

	req->s = socket(remoteaddr->sa_family, SOCK_DGRAM, 0);
	if (req->s == -1)
		goto fail;
	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1) {
		logmsg(LOG_NOTICE, "failed to connect");
		goto fail;
	}
	if (send(req->s, buf, r, 0) != r)
		goto fail;

	return req;
fail:
	free(hit);
	if (req->s != -1)
		close(req->s);
	free(req);
	return NULL;
}

static void
sendreply(int ud, struct request *req)
{
	uint8_t buf[65536];
	struct dnspacket *resp;
	struct dnscache *ent;
	size_t r;

	resp = (struct dnspacket *)buf;

	r = recv(req->s, buf, sizeof(buf), 0);
	if (r == 0 || r == -1 || r < sizeof(struct dnspacket))
		return;
	if (resp->id != req->reqid)
		return;
	resp->id = req->clientid;
	sendto(ud, buf, r, 0, &req->from, req->fromlen);
	if ((ent = req->cacheent)) {
		ent->ts = now;
		ent->ts.tv_sec += 10;
		ent->resp = malloc(r);
		if (!ent->resp)
			return;
		memcpy(ent->resp, buf, r);
		ent->resplen = r;
		TAILQ_INSERT_TAIL(&cache, ent, cache);
	}
}

static void
freerequest(struct request *req)
{
	struct dnscache *ent;

	TAILQ_REMOVE(&reqfifo, req, fifo);
	if (req->client != -1)
		close(req->client);
	if (req->s != -1)
		close(req->s);
	if ((ent = req->cacheent) && !ent->resp) {
		free(ent->req);
		free(ent);
	}
	free(req);
}

static void
freecacheent(struct dnscache *ent)
{
	TAILQ_REMOVE(&cache, ent, cache);
	free(ent->req);
	free(ent->resp);
	free(ent);
}

static struct request *
tcpphasetwo(struct request *req)
{
	int error;
	socklen_t len = sizeof(error);

	req->phase = 2;
	
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

	if (!(req = calloc(1, sizeof(*req))))
		return NULL;

	req->ts = now;
	req->ts.tv_sec += 30;

	req->s = -1;
	req->fromlen = sizeof(req->from);
	req->client = accept(ld, &req->from, &req->fromlen);
	if (req->client == -1)
		goto fail;

	req->s = socket(remoteaddr->sa_family, SOCK_STREAM, 0);
	if (req->s == -1)
		goto fail;
	if (fcntl(req->s, F_SETFL, O_NONBLOCK) == -1)
		goto fail;
	req->phase = 1;
	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1) {
		if (errno != EINPROGRESS)
			goto fail;
	} else {
		TAILQ_INSERT_TAIL(&reqfifo, req, fifo);
		return tcpphasetwo(req);
	}

	TAILQ_INSERT_TAIL(&reqfifo, req, fifo);
	return req;

fail:
	if (req->s != -1)
		close(req->s);
	if (req->client != -1)
		close(req->client);
	free(req);
	return NULL;
}

static int
readconfig(FILE *conf, struct sockaddr_storage *remoteaddr)
{
	char buf[1024];
	struct sockaddr_in *sin = (struct sockaddr_in *)remoteaddr;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)remoteaddr;

	if (fgets(buf, sizeof(buf), conf) == NULL)
		return -1;
	buf[strcspn(buf, "\n")] = '\0';

	memset(remoteaddr, 0, sizeof(*remoteaddr));
	if (inet_pton(AF_INET, buf, &sin->sin_addr) == 1) {
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_port = htons(53);
		return AF_INET;
	} else if (inet_pton(AF_INET6, buf, &sin6->sin6_addr) == 1) {
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = htons(53);
		return AF_INET6;
	} else {
		return -1;
	}
}

static int
launch(const char *confname, int ud, int ld, int kq)
{
	struct sockaddr_storage remoteaddr;
	struct kevent chlist[2], kev[4];
	struct timespec ts, *timeout = NULL;
	struct request *req;
	struct dnscache *ent;
	struct passwd *pwd;
	FILE *conf;
	int i, r, af;
	pid_t child;

	conf = fopen(confname, "r");
	if (!conf) {
		logmsg(LOG_ERR, "failed to open config %s", confname);
		return -1;
	}

	if (!debug) {
		if ((child = fork()))
			return child;
	}

	if (!(pwd = getpwnam("_rebound")))
		logerr("getpwnam failed");

	if (chroot("/var/empty") || chdir("/"))
		logerr("chroot failed (%d)", errno);

	setproctitle("worker");
	if (setgroups(1, &pwd->pw_gid) ||
	    setresgid(pwd->pw_gid, pwd->pw_gid, pwd->pw_gid) ||
	    setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid))
		logerr("failed to privdrop");

	close(kq);

	if (pledge("stdio inet", NULL) == -1)
		logerr("pledge failed");

	af = readconfig(conf, &remoteaddr);
	fclose(conf);
	if (af == -1)
		logerr("failed to read config %s", confname);

	kq = kqueue();

	EV_SET(&kev[0], ud, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[1], ld, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[2], SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	kevent(kq, kev, 3, NULL, 0, NULL);
	signal(SIGHUP, SIG_IGN);
	logmsg(LOG_INFO, "worker process going to work");
	while (1) {
		r = kevent(kq, NULL, 0, kev, 4, timeout);
		if (r == -1)
			logerr("kevent failed (%d)", errno);

		clock_gettime(CLOCK_MONOTONIC, &now);

		for (i = 0; i < r; i++) {
			if (kev[i].filter == EVFILT_SIGNAL) {
				logmsg(LOG_INFO, "hupped, exiting");
				exit(0);
			} else if (kev[i].ident == ud) {
				req = newrequest(ud,
				    (struct sockaddr *)&remoteaddr);
				if (req) {
					EV_SET(&chlist[0], req->s, EVFILT_READ,
					    EV_ADD, 0, 0, NULL);
					kevent(kq, chlist, 1, NULL, 0, NULL);
					TAILQ_INSERT_TAIL(&reqfifo, req, fifo);
				}
			} else if (kev[i].ident == ld) {
				req = newtcprequest(ld,
				    (struct sockaddr *)&remoteaddr);
				if (req) {
					EV_SET(&chlist[0], req->s,
					    req->phase == 1 ? EVFILT_WRITE :
					    EVFILT_READ, EV_ADD, 0, 0, NULL);
					kevent(kq, chlist, 1, NULL, 0, NULL);
				}
			} else if (kev[i].filter == EVFILT_WRITE) {
				req = TAILQ_FIRST(&reqfifo);
				while (req) {
					if (req->s == kev[i].ident)
						break;
					req = TAILQ_NEXT(req, fifo);
				}
				if (!req)
					logerr("lost request");
				req = tcpphasetwo(req);
				if (req) {
					EV_SET(&chlist[0], req->s, EVFILT_WRITE,
					    EV_DELETE, 0, 0, NULL);
					EV_SET(&chlist[1], req->s, EVFILT_READ,
					    EV_ADD, 0, 0, NULL);
					kevent(kq, chlist, 2, NULL, 0, NULL);
				}
			} else if (kev[i].filter == EVFILT_READ) {
				/* use a tree here? */
				req = TAILQ_FIRST(&reqfifo);
				while (req) {
					if (req->s == kev[i].ident)
						break;
					req = TAILQ_NEXT(req, fifo);
				}
				if (!req)
					logerr("lost request");
				if (req->client == -1)
					sendreply(ud, req);
				freerequest(req);
			} else {
				logerr("don't know what happened");
			}
		}

		timeout = NULL;
		/* burn old cache entries */
		while ((ent = TAILQ_FIRST(&cache))) {
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

	}
	/* not reached */
	exit(1);
}

static void __dead
usage(void)
{
	fprintf(stderr, "usage: rebound [-c config]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	struct sockaddr_in bindaddr;
	int r, kq, ld, ud, ch;
	int one;
	int childdead, hupped;
	pid_t child;
	struct kevent kev;
	struct timespec ts, *timeout = NULL;
	const char *conffile = "/etc/rebound.conf";

	while ((ch = getopt(argc, argv, "c:d")) != -1) {
		switch (ch) {
		case 'c':
			conffile = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (argc)
		usage();

	openlog("rebound", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	if (!debug)
		daemon(0, 0);

	TAILQ_INIT(&reqfifo);
	TAILQ_INIT(&cache);

	memset(&bindaddr, 0, sizeof(bindaddr));
	bindaddr.sin_len = sizeof(bindaddr);
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_port = htons(53);
	inet_aton("127.0.0.1", &bindaddr.sin_addr);

	ud = socket(AF_INET, SOCK_DGRAM, 0);
	if (ud == -1)
		err(1, "socket");
	if (bind(ud, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1)
		err(1, "bind");

	ld = socket(AF_INET, SOCK_STREAM, 0);
	if (ld == -1)
		err(1, "socket");
	one = 1;
	setsockopt(ld, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (bind(ld, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1)
		err(1, "bind");
	if (listen(ld, 10) == -1)
		err(1, "listen");

	if (debug) {
		launch(conffile, ud, ld, -1);
		return 1;
	}

	kq = kqueue();

	EV_SET(&kev, SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	kevent(kq, &kev, 1, NULL, 0, NULL);
	signal(SIGHUP, SIG_IGN);
	while (1) {
		hupped = 0;
		childdead = 0;
		child = launch(conffile, ud, ld, kq);
		if (child == -1)
			logerr("failed to launch");

		/* monitor child */
		EV_SET(&kev, child, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
		kevent(kq, &kev, 1, NULL, 0, NULL);

		/* wait for something to happen: HUP or child exiting */
		while (1) {
			r = kevent(kq, NULL, 0, &kev, 1, timeout);
			if (r == -1)
				logerr("kevent failed (%d)", errno);

			if (r == 0) {
				logerr("child died without HUP");
			} else if (kev.filter == EVFILT_SIGNAL) {
				/* signaled. kill child. */
				logmsg(LOG_INFO,
				    "received HUP, restarting");
				hupped = 1;
				if (childdead)
					break;
				kill(child, SIGHUP);
			} else if (kev.filter == EVFILT_PROC) {
				/* child died. wait for our own HUP. */
				logmsg(LOG_INFO,
				    "observed child exit");
				childdead = 1;
				if (hupped)
					break;
				memset(&ts, 0, sizeof(ts));
				ts.tv_sec = 1;
				timeout = &ts;
			} else {
				logerr("don't know what happened");
			}
		}
		wait(NULL);
	}
	return 1;
}
