/*	$OpenBSD: spamd.c,v 1.23 2003/03/09 19:22:25 beck Exp $	*/

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

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/types.h>
#include <machine/endian.h>

#include "sdl.h"

struct con {
	int fd;
	int state;
	int af;
	struct sockaddr_in sin;
	void *ia;
	char addr[32];
	char mail[64], rcpt[64];

	/*
	 * we will do stuttering by changing these to time_t's of
	 * now + n, and only advancing when the time is in the past/now
	 */
	time_t r;
	time_t w;
	time_t s;

	char ibuf[8192];
	char *ip;
	int il;
	char rend[5];	/* any chars in here causes input termination */

	int obufalloc;
	char *obuf;
	size_t osize;
	char *op;
	int ol;
} *con;

void     usage(void);
char    *grow_obuf(struct con *, int);
int      parse_configline(char *);
void     parse_configs(void);
void     do_config(void);
int      append_error_string (struct con *, size_t, char *, int, void *);
void     build_reply(struct  con *);
void     doreply(struct con *);
void     setlog(char *, size_t, char *);
void     initcon(struct con *, int, struct sockaddr_in *);
void     closecon(struct con *);
int      match(char *, char *);
void     nextstate(struct con *);
void     handler(struct con *);
void     handlew(struct con *, int one);

char hostname[MAXHOSTNAMELEN];
struct syslog_data sdata = SYSLOG_DATA_INIT;
char *reply = NULL;
char *nreply = "450";
char *spamd = "spamd IP-based SPAM blocker";

extern struct sdlist *blacklists;

int conffd = -1;
char *cb;
size_t cbs, cbu;

time_t t;

#define MAXCON 200
int maxcon = MAXCON;
int clients;
int debug;
#define MAXTIME 400


void
usage(void)
{
	fprintf(stderr,
	    "usage: spamd [-45d] [-r reply] [-c maxcon] [-p port] [-n name]\n");
	exit(1);
}

char *
grow_obuf(struct con *cp, int off)
{
	char *tmp;

	if (!cp->obufalloc)
		cp->obuf = NULL;
	tmp = realloc(cp->obuf, cp->osize + 8192);
	if (tmp != NULL) {
		cp->osize += 8192;
		cp->obuf = tmp;
		cp->obufalloc = 1;
		return (cp->obuf + off);
	}
	return (NULL);
}


int
parse_configline(char *line)
{
	char *cp, prev, *name, *msg;
	static char **av = NULL;
	static size_t ac = 0;
	size_t au = 0;
	int mdone = 0;

	if (debug > 0)
		printf("read config line %40s ...\n", line);

	name = line;

	for (cp = name; *cp && *cp != ';'; cp++)
		;
	if (*cp != ';')
		goto parse_error;
	*cp++ = '\0';
	msg = cp;
	if (*cp++ != '"')
		goto parse_error;
	prev = '\0';
	for (; !mdone; cp++) {
		switch (*cp) {
		case '\\':
			if (!prev)
				prev = *cp;
			else
				prev = '\0';
			break;
		case '"':
			if (prev != '\\') {
				cp++;
				if (*cp == ';') {
					mdone = 1;
					*cp = '\0';
				} else
					goto parse_error;
			}
			break;
		case '\0':
			goto parse_error;
		default:
			prev = '\0';
			break;
		}
	}

	do {
		if (ac == au) {
			char **tmp;

			tmp = realloc(av, (ac + 2048) * sizeof(char *));
			if (tmp == NULL)
				return (-1);
			av = tmp;
			ac += 2048;
		}
	} while ((av[au++] = strsep(&cp, ";")) != NULL);

	if (au < 2)
		goto parse_error;
	else
		sdl_add(name, msg, av, au - 1);
	return (0);

parse_error:
	if (debug > 0)
		printf("bogus config line - need 'tag;message;a/m;a/m;a/m...'\n");
	return (-1);
}

void
parse_configs(void)
{
	char *start, *end;
	int i;

	if (cbu == cbs) {
		char *tmp;

		tmp = realloc(cb, cbs + 8192);
		if (tmp == NULL) {
			if (debug > 0)
				perror("malloc()");
			free(cb);
			cb = NULL;
			cbs = cbu = 0;
			return;
		}
		cbs += 8192;
		cb = tmp;
	}
	cb[cbu++] = '\0';

	start = cb;
	end = start;
	for (i = 0; i < cbu; i++) {
		if (*end == '\n') {
			*end = '\0';
			if (end > start + 1)
				parse_configline(start);
			start = ++end;
		} else
			++end;
	}
	if (end > start + 1)
		parse_configline(start);
}

void
do_config(void)
{
	int n;

	if (debug > 0)
		printf("got configuration connection\n");

	if (cbu == cbs) {
		char *tmp;

		tmp = realloc(cb, cbs + 8192);
		if (tmp == NULL) {
			if (debug > 0)
				perror("malloc()");
			free(cb);
			cb = NULL;
			cbs = 0;
			goto configdone;
		}
		cbs += 8192;
		cb = tmp;
	}

	n = read(conffd, cb + cbu, cbs - cbu);
	if (debug > 0)
		printf("read %d config bytes\n", n);
	if (n == 0) {
		parse_configs();
		goto configdone;
	} else if (n == -1) {
		if (debug > 0)
			perror("read()");
		goto configdone;
	} else
		cbu += n;
	return;

configdone:
	cbu = 0;
	close(conffd);
	conffd = -1;
}


int
append_error_string (struct con *cp, size_t off, char *fmt, int af, void *ia)
{
	char sav = '\0';
	static int lastcont = 0;
	char *c = cp->obuf + off;
	char *s = fmt;
	size_t len = cp->osize - off;
	int i = 0;

	if (off == 0)
		lastcont = 0;

	if (lastcont != 0)
		cp->obuf[lastcont] = '-';
	i += snprintf(c, len, "%s ", nreply);
	lastcont = off + i - 1;
	if (*s == '"')
		s++;
	while (*s) {
		/*
		 * Make sure we at minimum, have room to add a
		 * format code (4 bytes), and a v6 address(39 bytes)
		 * and a byte saved in sav.
		 */
		if (i >= len - 46) {
			c = grow_obuf(cp, off);
			if (c == NULL)
				goto no_mem;
			len = cp->osize - (off + i);
		}

		if (c[i-1] == '\n') {
			if (lastcont != 0)
				cp->obuf[lastcont] = '-';
			i += snprintf(c + i, len, "%s ", nreply);
			lastcont = off + i - 1;
		}

		switch (*s) {
		case '\\':
		case '%':
			if (!sav)
				sav = *s;
			else {
				c[i++] = sav;
				sav = '\0';
				c[i] = '\0';
			}
			break;
		case '"':
		case 'A':
		case 'n':
			if (*(s+1) == '\0') {
				break;
			}
			if (sav == '\\' && *s == 'n') {
				c[i++] = '\n';
				sav = '\0';
				c[i] = '\0';
				break;
			} else if (sav == '\\' && *s == '"') {
				c[i++] = '"';
				sav = '\0';
				c[i] = '\0';
				break;
			} else if (sav == '%' && *s == 'A') {
				inet_ntop(af, ia, c + i, (len - i));
				i += strlen(c + i);
				sav = '\0';
				break;
			}
			/* fallthrough */
		default:
			if (sav)
			c[i++] = sav;
			c[i++] = *s;
			sav = '\0';
			c[i] = '\0';
			break;
		}
		s++;
	}
	return (i);
no_mem:
	/* Out of memory, free obuf and bail, caller must deal */
	if (cp->osize)
		free(cp->obuf);
	cp->obuf = NULL;
	cp->osize = 0;
	return (-1);
}


void
build_reply(struct con *cp)
{
	struct sdlist **matches;
	int off = 0;

	matches = sdl_lookup(blacklists, cp->af, cp->ia);
	if (matches == NULL) {
		if (cp->osize)
			free(cp->obuf);
		cp->obuf = NULL;
		cp->osize = 0;
		goto bad;
	}
	for (; *matches; matches++) {
		int used = 0;
		char *c = cp->obuf + off;
		int left = cp->osize - off;

		used = append_error_string(cp, off, matches[0]->string,
		    cp->af, cp->ia);
		if (used == -1)
			goto bad;
		off += used;
		left -= used;
		if (cp->obuf[off - 1] != '\n') {
			if (left < 1) {
				c = grow_obuf(cp, off);
				if (c == NULL) {
					if (cp->osize)
						free(cp->obuf);
					cp->obuf = NULL;
					cp->osize = 0;
					goto bad;
				}
			}
			cp->obuf[off++] = '\n';
			cp->obuf[off] = '\0';
		}
	}
	return;
bad:
	/* Out of memory, or no match. give generic reply */
	asprintf(&cp->obuf,
	    "%s-Sorry %s\n"
	    "%s-You are trying to send mail from an address listed by one\n"
	    "%s or more IP-based registries as being a SPAM source.\n",
	    nreply, cp->addr, nreply, nreply);
	if (cp->obuf == NULL) {
		/* we're having a really bad day.. */
		cp->obufalloc = 0; /* know not to free or mangle */
		cp->obuf = "450 Try again\n";
	} else
		cp->osize = strlen(cp->obuf) + 1;
}

void
doreply(struct con *cp)
{
	if (reply) {
		if (!cp->obufalloc)
			errx(1, "shouldn't happen");
		snprintf(cp->obuf, cp->osize, "%s %s\n", nreply, reply);
		return;
	}
	build_reply(cp);
}

void
setlog(char *p, size_t len, char *f)
{
	char *s;

	s = strsep(&f, ":");
	if (!s)
		return;
	s = strsep(&f, " \t");
	if (s == NULL)
		return;
	strlcpy(p, s, len);
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
	if (cp->obufalloc) {
		free(cp->obuf);
		cp->obuf = NULL;
	}
	bzero(cp, sizeof(struct con));
	if (grow_obuf(cp, 0) == NULL)
		err(1, "malloc");
	cp->fd = fd;
	memcpy(&cp->sin, sin, sizeof(struct sockaddr_in));
	cp->af = sin->sin_family;
	cp->ia = (void *) &cp->sin.sin_addr;
	strlcpy(cp->addr, inet_ntoa(sin->sin_addr), sizeof(cp->addr));
	snprintf(cp->obuf, cp->osize,
	    "220 %s ESMTP %s; %s",
	    hostname, spamd, ctime(&t));
	cp->op = cp->obuf;
	cp->ol = strlen(cp->op);
	cp->w = t + 1;
	cp->s = t;
	strlcpy(cp->rend, "\n\r", sizeof cp->rend);
	clients++;
}

void
closecon(struct con *cp)
{
	if (debug > 0) {
		time_t t;

		time(&t);
		printf("%s connected for %d seconds.\n", cp->addr, t - cp->s);
	}
	if (cp->osize > 0 && cp->obufalloc) {
		free(cp->obuf);
		cp->obuf = NULL;
		cp->osize = 0;
	}
	close(cp->fd);
	clients--;
	cp->fd = -1;
}

int
match(char *s1, char *s2)
{
	return (strncasecmp(s1, s2, strlen(s2)) == 0);
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
		cp->r = t;
		break;
	case 1:
		/* received input: parse, and select next state */
		if (match(cp->ibuf, "HELO") ||
		    match(cp->ibuf, "EHLO")) {
			snprintf(cp->obuf, cp->osize,
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
		cp->r = t;
		break;
	mail:
	case 3:
		if (match(cp->ibuf, "MAIL")) {
			setlog(cp->mail, sizeof cp->mail, cp->ibuf);
			snprintf(cp->obuf, cp->osize,
			    "250 You are about to try to deliver spam. "
			    "Your time will be spent, for nothing.\n");
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
		cp->r = t;
		break;
	rcpt:
	case 5:
		if (match(cp->ibuf, "RCPT")) {
			setlog(cp->rcpt, sizeof(cp->rcpt), cp->ibuf);
			snprintf(cp->obuf, cp->osize,
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
		cp->r = t;
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
		closecon(cp);
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
			closecon(cp);
		} else if (n == -1) {
			if (debug > 0)
				perror("read()");
			closecon(cp);
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
		if (*cp->op == '\n') {
			/* insert \r before \n */
			n = write(cp->fd, "\r", 1);
			if (n == 0) {
				closecon(cp);
				goto handled;
			} else if (n == -1) {
				if (debug > 0 && errno != EPIPE)
					perror("write()");
				closecon(cp);
				goto handled;
			}
		}
		n = write(cp->fd, cp->op, one ? 1 : cp->ol);
		if (n == 0) {
			closecon(cp);
		} else if (n == -1) {
			if (debug > 0 && errno != EPIPE)
				perror("write()");
			closecon(cp);
		} else {
			cp->op += n;
			cp->ol -= n;
		}
	}
handled:
	cp->w = t + 1;
	if (cp->ol == 0) {
		cp->w = 0;
		nextstate(cp);
	}
}

int
main(int argc, char *argv[])
{
	fd_set *fdsr = NULL, *fdsw = NULL;
	struct sockaddr_in sin;
	struct sockaddr_in lin;
	struct passwd *pw;
	int ch, s, s2, conflisten = 0, i, omax = 0;
	int sinlen, one = 1;
	u_short port, cfg_port;
	struct servent *ent;

	tzset();
	openlog_r("spamd", LOG_PID | LOG_NDELAY, LOG_DAEMON, &sdata);

	if ((ent = getservbyname("spamd", "tcp")) == NULL)
		errx(1, "Can't find service \"spamd\" in /etc/services");
	port = ntohs(ent->s_port);
	if ((ent = getservbyname("spamd-cfg", "tcp")) == NULL)
		errx(1, "Can't find service \"spamd-cff\" in /etc/services");
	cfg_port = ntohs(ent->s_port);

	if (gethostname(hostname, sizeof hostname) == -1)
		err(1, "gethostname");

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

	con = calloc(maxcon, sizeof(*con));
	if (con == NULL)
		err(1, "calloc");

	con->obuf = malloc(8192);

	if (con->obuf == NULL)
		err(1, "malloc");
	con->osize = 8192;

	for (i = 0; i < maxcon; i++)
		con[i].fd = -1;

	signal(SIGPIPE, SIG_IGN);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1)
		err(1, "socket");

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one,
	    sizeof(one)) == -1)
		return (-1);

	conflisten = socket(AF_INET, SOCK_STREAM, 0);
	if (conflisten == -1)
		err(1, "socket");

	if (setsockopt(conflisten, SOL_SOCKET, SO_REUSEADDR, &one,
	    sizeof(one)) == -1)
		return (-1);

	memset(&sin, 0, sizeof sin);
	sin.sin_len = sizeof(sin);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if (bind(s, (struct sockaddr *)&sin, sizeof sin) == -1)
		err(1, "bind");

	memset(&lin, 0, sizeof sin);
	lin.sin_len = sizeof(sin);
	lin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	lin.sin_family = AF_INET;
	lin.sin_port = htons(cfg_port);

	if (bind(conflisten, (struct sockaddr *)&lin, sizeof lin) == -1)
		err(1, "bind local");

	pw = getpwnam("_spamd");
	if (!pw)
		pw = getpwnam("nobody");

	if (chroot("/var/empty") == -1 || chdir("/") == -1) {
		syslog(LOG_ERR, "cannot chdir to /var/empty.");
		exit(1);
	}

	if (pw) {
		setgroups(1, &pw->pw_gid);
		setegid(pw->pw_gid);
		setgid(pw->pw_gid);
		seteuid(pw->pw_uid);
		setuid(pw->pw_uid);
	}

	if (listen(s, 10) == -1)
		err(1, "listen");

	if (listen(conflisten, 10) == -1)
		err(1, "listen");

	if (debug == 0) {
		if (daemon(1, 1) == -1)
			err(1, "fork");
	} else
		printf("listening for incoming connections.\n");

	while (1) {
		struct timeval tv, *tvp;
		int max, i, n;
		int writers;

		max = MAX(s, conflisten);
		max = MAX(max, conffd);

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
				err(1, "calloc");
			fdsw = (fd_set *)calloc(howmany(max+1, NFDBITS),
			    sizeof(fd_mask));
			if (fdsw == NULL)
				err(1, "calloc");
			omax = max;
		} else {
			memset(fdsr, 0, howmany(max+1, NFDBITS) *
			    sizeof(fd_mask));
			memset(fdsw, 0, howmany(max+1, NFDBITS) *
			    sizeof(fd_mask));
		}

		writers = 0;
		for (i = 0; i < maxcon; i++) {
			if (con[i].fd != -1 && con[i].r) {
				if (con[i].r + MAXTIME <= t) {
					closecon(&con[i]);
					continue;
				}
				FD_SET(con[i].fd, fdsr);
			}
			if (con[i].fd != -1 && con[i].w) {
				if (con[i].w + MAXTIME <= t) {
					closecon(&con[i]);
					continue;
				}
				if (con[i].w <= t)
					FD_SET(con[i].fd, fdsw);
				writers = 1;
			}
		}
		FD_SET(s, fdsr);

		/* only one active config conn at a time */
		if (conffd == -1)
			FD_SET(conflisten, fdsr);
		else
			FD_SET(conffd, fdsr);

		if (writers == 0) {
			tvp = NULL;
		} else {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			tvp = &tv;
		}

		n = select(max+1, fdsr, fdsw, NULL, tvp);
		if (n == -1 && errno == EINTR)
			err(1, "select");
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
				err(1, "accept");
			}
			for (i = 0; i < maxcon; i++)
				if (con[i].fd == -1)
					break;
			if (i == maxcon)
				close(s2);
			else
				initcon(&con[i], s2, &sin);
		}
		if (FD_ISSET(conflisten, fdsr)) {
			sinlen = sizeof(lin);
			conffd = accept(conflisten, (struct sockaddr *)&lin,
			    &sinlen);
			if (conffd == -1) {
				if (errno == EINTR)
					continue;
				err(1, "accept");
			}
		}
		if (conffd != -1 && FD_ISSET(conffd, fdsr)) {
			do_config();
		}

	}
	exit(1);
}
