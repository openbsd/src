/* $OpenBSD: rebound.c,v 1.5 2015/10/15 20:58:14 tedu Exp $ */
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

#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <assert.h>
#include <pwd.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>

uint16_t randomid(void);

struct timespec now;
int debug;

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

struct dnscache {
	TAILQ_ENTRY(dnscache) cache;
	struct dnspacket *req;
	size_t reqlen;
	struct dnspacket *resp;
	size_t resplen;
	struct timespec ts;
};
TAILQ_HEAD(, dnscache) cache;

struct request {
	int s;
	int client;
	struct sockaddr from;
	socklen_t fromlen;
	struct timespec ts;
	TAILQ_ENTRY(request) fifo;
	uint16_t clientid;
	uint16_t reqid;
	struct dnscache *cacheent;
};
TAILQ_HEAD(, request) reqfifo;


void
logmsg(int prio, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (debug) {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	} else {
		vsyslog(prio, msg, ap);
	}
	va_end(ap);
}

struct dnscache *
cachelookup(struct dnspacket *dnsreq, size_t reqlen)
{
	struct dnscache *hit;
	uint16_t origid;

	origid = dnsreq->id;
	dnsreq->id = 0;
	TAILQ_FOREACH(hit, &cache, cache) {
		if (memcmp(hit->req, dnsreq, reqlen) == 0)
			break;
	}
	dnsreq->id = origid;
	return hit;
}

struct request *
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
			hit->req->id = 0;
		} else {
			free(hit);
			hit = NULL;

		}
	}
	req->cacheent = hit;

	req->s = socket(AF_INET, SOCK_DGRAM, 0);
	if (req->s == -1)
		goto fail;
	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1) {
		logmsg(0, "failed to connect");
		goto fail;
	}
	if (send(req->s, buf, r, 0) != r)
		goto fail;
	req->ts = now;
	req->ts.tv_sec += 30;

	return req;
fail:
	close(req->s);
	free(req);
	return NULL;
}

void
sendreply(int ud, struct request *req)
{
	uint8_t buf[65536];
	struct dnspacket *resp;
	size_t r;

	resp = (struct dnspacket *)buf;

	r = recv(req->s, buf, sizeof(buf), 0);
	if (r == 0 || r == -1 || r < sizeof(struct dnspacket))
		return;
	if (resp->id != req->reqid)
		return;
	resp->id = req->clientid;
	sendto(ud, buf, r, 0, &req->from, req->fromlen);
	if (req->cacheent) {
		req->cacheent->ts = now;
		req->cacheent->ts.tv_sec += 10;
		TAILQ_INSERT_TAIL(&cache, req->cacheent, cache);
		req->cacheent->resp = malloc(r);
		if (!req->cacheent->resp)
			return;
		memcpy(req->cacheent->resp, buf, r);
		req->cacheent->resplen = r;
	}
}

void
freerequest(struct request *req)
{
	TAILQ_REMOVE(&reqfifo, req, fifo);
	close(req->client);
	close(req->s);
	free(req);
}

void
freecacheent(struct dnscache *ent)
{
	TAILQ_REMOVE(&cache, ent, cache);
	free(ent->req);
	free(ent->resp);
	free(ent);
}

struct request *
newtcprequest(int ld, struct sockaddr *remoteaddr)
{
	struct request *req;

	if (!(req = malloc(sizeof(*req))))
		return NULL;

	req->s = -1;
	req->fromlen = sizeof(req->from);
	req->client = accept(ld, &req->from, &req->fromlen);
	if (req->client == -1)
		goto fail;

	req->s = socket(AF_INET, SOCK_STREAM, 0);
	if (req->s == -1)
		goto fail;
	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1)
		goto fail;
	if (setsockopt(req->client, SOL_SOCKET, SO_SPLICE, &req->s,
	    sizeof(req->s)) == -1)
		goto fail;
	if (setsockopt(req->s, SOL_SOCKET, SO_SPLICE, &req->client,
	    sizeof(req->client)) == -1)
		goto fail;
	req->ts = now;
	req->ts.tv_sec += 30;

	return req;
fail:
	close(req->s);
	close(req->client);
	free(req);
	return NULL;
}

int
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

int
launch(const char *confname, int ud, int ld, int kq)
{
	struct sockaddr_storage remoteaddr;
	struct kevent chlist[1], kev[4];
	struct timespec ts, *timeout = NULL;
	struct request *req;
	struct dnscache *ent;
	struct passwd *pwd;
	FILE *conf;
	int i, r, af;
	pid_t child;

	conf = fopen(confname, "r");
	if (!conf) {
		logmsg(LOG_DAEMON | LOG_ERR, "failed to open config %s", confname);
		return -1;
	}

	if (!debug) {
		if ((child = fork()))
			return child;
	}

	pwd = getpwnam("nobody");

	if (chroot("/var/empty") || chdir("/")) {
		logmsg(LOG_DAEMON | LOG_ERR, "chroot failed (%d)", errno);
		exit(1);
	}

	setproctitle("worker");
	setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid);

	close(kq);

	if (pledge("stdio inet", NULL) == -1) {
		logmsg(LOG_DAEMON | LOG_ERR, "pledge failed");
		exit(1);
	}

	af = readconfig(conf, &remoteaddr);
	fclose(conf);
	if (af == -1) {
		logmsg(LOG_DAEMON | LOG_ERR, "failed to read config %s", confname);
		exit(1);
	}

	kq = kqueue();

	EV_SET(&kev[0], ud, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[1], ld, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[2], SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	kevent(kq, kev, 3, NULL, 0, NULL);
	signal(SIGHUP, SIG_IGN);
	while (1) {
		r = kevent(kq, NULL, 0, kev, 4, timeout);
		if (r == -1) {
			logmsg(LOG_DAEMON | LOG_ERR, "kevent failed (%d)", errno);
			exit(1);
		}
		clock_gettime(CLOCK_MONOTONIC, &now);

		for (i = 0; i < r; i++) {
			if (kev[i].filter == EVFILT_SIGNAL) {
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
					EV_SET(&chlist[0], req->s, EVFILT_READ,
					    EV_ADD, 0, 0, NULL);
					kevent(kq, chlist, 1, NULL, 0, NULL);
					TAILQ_INSERT_TAIL(&reqfifo, req, fifo);
				}
			} else {
				/* use a tree here? */
				req = TAILQ_FIRST(&reqfifo);
				while (req) {
					if (req->s == kev[i].ident)
						break;
					req = TAILQ_NEXT(req, fifo);
				}
				assert(req);
				if (req->client == -1)
					sendreply(ud, req);
				freerequest(req);
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
	exit(0);
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
		if (child == -1) {
			logmsg(LOG_DAEMON | LOG_ERR, "failed to launch");
			return 1;
		}
		/* monitor child */
		EV_SET(&kev, child, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
		kevent(kq, &kev, 1, NULL, 0, NULL);

		/* wait for something to happen: HUP or child exiting */
		while (1) {
			r = kevent(kq, NULL, 0, &kev, 1, timeout);
			if (r == 0) {
				logmsg(LOG_DAEMON | LOG_ERR,
				    "child died without HUP");
				return 1;
			} else if (kev.filter == EVFILT_SIGNAL) {
				/* signaled. kill child. */
				logmsg(LOG_DAEMON | LOG_INFO,
				    "received HUP, restarting");
				hupped = 1;
				if (childdead)
					break;
				kill(child, SIGHUP);
			} else if (kev.filter == EVFILT_PROC) {
				/* child died. wait for our own HUP. */
				logmsg(LOG_DAEMON | LOG_INFO,
				    "observed child exit");
				childdead = 1;
				if (hupped)
					break;
				memset(&ts, 0, sizeof(ts));
				ts.tv_sec = 1;
				timeout = &ts;
			} else {
				logmsg(LOG_DAEMON | LOG_ERR,
				    "don't know what happened");
			}
		}
		wait(NULL);
	}
	return 1;
}
