/*	$OpenBSD: spamd.c,v 1.1 2002/12/21 01:41:54 deraadt Exp $	*/

/*
 * Copyright (c) 2002 Theo de Raadt.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>

char hostname[MAXHOSTNAMELEN];
struct syslog_data sdata = SYSLOG_DATA_INIT;
char *reply = NULL;
char *nreply = "450";
char *spamd = "spamd IP-based SPAM blocker";

time_t t;

#define MAXCON 200
int maxcon = MAXCON;
int clients;

struct con {
	int fd;
	int state;
	char addr[32];
	char mail[64], rcpt[64];

	/*
	 * we will do stuttering by changing these to time_t's of
	 * now + n, and only advancing when the time is in the past/now
	 */
	int r;
	time_t w;

	char ibuf[8192];
	char *ip;
	int il;
	char rend[5];	/* any chars in here causes input termination */

	char obuf[8192];
	char *op;
	int ol;
} con[MAXCON];

void
usage(void)
{
	fprintf(stderr,
	    "usage: spamd [-45d] [-r reply] [-c maxcon] [-p port] [-n name]\n");
	exit(1);
}

void
doreply(struct con *cp)
{
	if (reply) {
		snprintf(cp->obuf, sizeof cp->obuf,
		    "%s %s\n", nreply, reply);
		return;
	}

	snprintf(cp->obuf, sizeof cp->obuf,
	    "%s-SPAM. www.spews.org/ask.cgi?x=%s\n"
	    "%s-You are trying to send mail from an address listed by one or\n"
	    "%s-more IP-based registries as being in a SPAM-generating netblock.\n"
	    "%s SPAM. www.spews.org/ask.cgi?x=%s\n",
	    nreply, cp->addr, nreply, nreply, nreply, cp->addr);
}

void
setlog(char *p, int l, char *f)
{
	char *s;

	s = strsep(&f, ":");
	if (!s)
		return;
	s = strsep(&f, " \t");
	if (s == NULL)
		return;
	strlcpy(p, s, l);
	s = strsep(&p, " \t\n\r");
	if (s == NULL)
		return;
	s = strsep(&p, " \t\n\r");
	if (s)
		*s = '\0';
}

void
initcon(struct con *cp, int fd, struct sockaddr_in *sin)
{
	time_t t;

	time(&t);
	bzero(cp, sizeof(struct con));
	cp->fd = fd;
	strlcpy(cp->addr, inet_ntoa(sin->sin_addr), sizeof(cp->addr));
	snprintf(cp->obuf, sizeof(cp->obuf),
	    "220 %s ESMTP %s; %s",
	    hostname, spamd, ctime(&t));
	cp->op = cp->obuf;
	cp->ol = strlen(cp->op);
	cp->w = t + 1;
	strlcpy(cp->rend, "\n\r", sizeof cp->rend);
	clients++;
}

int
match(char *s1, char *s2)
{
	return !strncasecmp(s1, s2, strlen(s2));
}

void
nextstate(struct con *cp)
{
	switch (cp->state) {
	case 0:
		/* banner sent; wait for input */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->state = 1;
		cp->r = 1;
		break;
	case 1:
		/* received input: parse, and select next state */
		if (match(cp->ibuf, "HELO") ||
		    match(cp->ibuf, "EHLO")) {
			snprintf(cp->obuf, sizeof cp->obuf,
			    "250 Hello, spam sender. "
			    "Pleased to be wasting your time.\n");
			cp->op = cp->obuf;
			cp->ol = strlen(cp->op);
			cp->state = 2;
			cp->w = t + 1;
			break;
		}
		goto mail;
	case 2:
		/* sent 250 Hello, wait for input */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->state = 3;
		cp->r = 1;
		break;
	mail:
	case 3:
		if (match(cp->ibuf, "MAIL")) {
			setlog(cp->mail, sizeof cp->mail, cp->ibuf);
			snprintf(cp->obuf, sizeof cp->obuf,
			    "250 You are about to try to deliver spam. "
			    "Your time will be spent, amounting to nothing.\n");
			cp->op = cp->obuf;
			cp->ol = strlen(cp->op);
			cp->state = 4;
			cp->w = t + 1;
			break;
		}
		goto rcpt;
	case 4:
		/* sent 250 Sender ok */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->state = 5;
		cp->r = 1;
		break;
	rcpt:
	case 5:
		if (match(cp->ibuf, "RCPT")) {
			setlog(cp->rcpt, sizeof(cp->rcpt), cp->ibuf);
			snprintf(cp->obuf, sizeof cp->obuf,
			    "250 This is hurting you more than it is "
			    "hurting me.\n");
			cp->op = cp->obuf;
			cp->ol = strlen(cp->op);
			cp->state = 6;
			cp->w = t + 1;
			break;
		}
		goto spam;
	case 6:
		/* sent 250 blah */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->state = 50;
		cp->r = 1;
		break;

	spam:
	case 50:
		syslog_r(LOG_INFO, &sdata, "%s: %s -> %s",
		    cp->addr, cp->mail, cp->rcpt);
		doreply(cp);
		cp->op = cp->obuf;
		cp->ol = strlen(cp->op);
		cp->state = 99;
		cp->w = t + 1;
		break;
	case 99:
		close(cp->fd);
		clients--;
		cp->fd = -1;
		break;
	default:
		errx(1, "illegal state %d", cp->state);
		break;
	}
}

void
handler(struct con *cp)
{
	int end = 0;
	int i, n;

	if (cp->r) {
		n = read(cp->fd, cp->ip, cp->il);
		if (n == 0) {
			close(cp->fd);
			clients--;
			cp->fd = -1;
		} else if (n == -1) {
			/* XXX */
		} else {
			if (cp->rend[0])
				for (i = 0; i < n; i++)
					if (strchr(cp->rend, cp->op[i]))
						end = 1;
			cp->ip += n;
			cp->il -= n;
		}
	}
	if (end || cp->il == 0) {
		*cp->ip = '\0';
		cp->r = 0;
		nextstate(cp);
	}
}

void
handlew(struct con *cp, int one)
{
	int n;

	if (cp->w) {
		n = write(cp->fd, cp->op, one ? 1 : cp->ol);
		if (n == 0) {
			close(cp->fd);
			clients--;
			cp->fd = -1;
		} else if (n == -1) {
			/* XXX */
		} else {
			cp->op += n;
			cp->ol -= n;
		}
	}
	cp->w = t + 1;
	if (cp->ol == 0) {
		cp->w = 0;
		nextstate(cp);
	}
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in sin;
	struct passwd *pw;
	int ch, s, s2, i;
	int sinlen, one = 1;
	u_short port = 8025;
	int debug = 0;

	tzset();
	openlog_r("spamd", LOG_PID | LOG_NDELAY, LOG_DAEMON, &sdata);

	pw = getpwnam("_spamd");
	if (!pw)
		pw = getpwnam("nobody");
	if (chroot("/var/empty") == -1) {
		syslog(LOG_ERR, "cannot chdir to /var/empty.");
		exit(1);
	}
	chdir("/");

	if (pw) {
		setgroups(1, &pw->pw_gid);
		setegid(pw->pw_gid);
		setgid(pw->pw_gid);
		seteuid(pw->pw_uid);
		setuid(pw->pw_uid);
	}

	gethostname(hostname, sizeof hostname);

	for (i = 0; i < MAXCON; i++)
		con[i].fd = -1;

	while ((ch = getopt(argc, argv, "45c:p:dr:n:")) != -1) {
		switch (ch) {
		case '4':
			nreply = "450";
			break;
		case '5':
			nreply = "550";
			break;
		case 'c':
			i = atoi(optarg);
			if (i > MAXCON)
				usage();
			maxcon = i;
			break;
		case 'p':
			i = atoi(optarg);
			port = i;
			break;
		case 'd':
			debug = 1;
			break;
		case 'r':
			reply = optarg;
			break;
		case 'n':
			spamd = optarg;
			break;
		default:
			usage();
			break;
		}
	}

	signal(SIGPIPE, SIG_IGN);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1)
		errx(1, "socket");

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one,
	    sizeof(one))  == -1)
		return(-1);

	memset(&sin, 0, sizeof sin);
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if (bind(s, (struct sockaddr *)&sin, sizeof sin) == -1)
		errx(1, "bind");

	listen(s, 10);

	if (debug == 0)
		daemon(1, 1);

	while (1) {
		fd_set *fdsr = NULL, *fdsw = NULL;
		struct timeval tv, *tvp;
		int max = s, i, n, omax = 0;
		int writers;

		time(&t);
		for (i = 0; i < maxcon; i++)
			if (con[i].fd != -1)
				max = MAX(max, con[i].fd);

		if (max > omax) {
			if (fdsr)
				free(fdsr);
			if (fdsw)
				free(fdsw);
			fdsr = (fd_set *)calloc(howmany(max+1, NFDBITS),
			    sizeof(fd_mask));
			if (fdsr == NULL)
				errx(1, "calloc");
			fdsw = (fd_set *)calloc(howmany(max+1, NFDBITS),
			    sizeof(fd_mask));
			if (fdsw == NULL)
				errx(1, "calloc");
			omax = max;
		} else {
			memset(fdsr, howmany(max+1, NFDBITS),
			    sizeof(fd_mask));
			memset(fdsw, howmany(max+1, NFDBITS),
			    sizeof(fd_mask));
		}

		writers = 0;
		for (i = 0; i < maxcon; i++) {
			if (con[i].fd != -1 && con[i].r)
				FD_SET(con[i].fd, fdsr);
			if (con[i].fd != -1 && con[i].w) {
				if (con[i].w <= t)
					FD_SET(con[i].fd, fdsw);
				writers = 1;
			}
		}
		FD_SET(s, fdsr);

		if (writers == 0) {
			tvp = NULL;
		} else {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			tvp = &tv;
		}

		n = select(max+1, fdsr, fdsw, NULL, tvp);
		if (n == -1 && errno == EINTR)
			errx(1, "select");
		if (n == 0)
			continue;

		for (i = 0; i < maxcon; i++) {
			if (con[i].fd != -1 && FD_ISSET(con[i].fd, fdsr))
				handler(&con[i]);
			if (con[i].fd != -1 && FD_ISSET(con[i].fd, fdsw))
				handlew(&con[i], clients + 5 < maxcon);
		}
		if (FD_ISSET(s, fdsr)) {
			sinlen = sizeof(sin);
			s2 = accept(s, (struct sockaddr *)&sin, &sinlen);
			if (s2 == -1) {
				if (errno == EINTR)
					continue;
				errx(1, "accept");
			}
			for (i = 0; i < maxcon; i++)
				if (con[i].fd == -1)
					break;
			if (i == maxcon)
				close(s2);
			else
				initcon(&con[i], s2, &sin);
		}
	}
	exit(1);
}
