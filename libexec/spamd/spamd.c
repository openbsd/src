/*	$OpenBSD: spamd.c,v 1.80 2005/11/12 02:20:37 deraadt Exp $	*/

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
#include <sys/resource.h>

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

#include "sdl.h"
#include "grey.h"

struct con {
	int fd;
	int state;
	int laststate;
	int af;
	struct sockaddr_storage ss;
	void *ia;
	char addr[32];
	char mail[MAX_MAIL], rcpt[MAX_MAIL];
	struct sdlist **blacklists;

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

	char *obuf;
	char *lists;
	size_t osize;
	char *op;
	int ol;
	int data_lines;
	int data_body;
	int stutter;
	int sr;
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
void     initcon(struct con *, int, struct sockaddr *);
void     closecon(struct con *);
int      match(const char *, const char *);
void     nextstate(struct con *);
void     handler(struct con *);
void     handlew(struct con *, int one);

char hostname[MAXHOSTNAMELEN];
struct syslog_data sdata = SYSLOG_DATA_INIT;
char *reply = NULL;
char *nreply = "450";
char *spamd = "spamd IP-based SPAM blocker";
int greypipe[2];
int trappipe[2];
FILE *grey;
FILE *trapcfg;
time_t passtime = PASSTIME;
time_t greyexp = GREYEXP;
time_t whiteexp = WHITEEXP;
time_t trapexp = TRAPEXP;
struct passwd *pw;
pid_t jail_pid = -1;
u_short cfg_port;

extern struct sdlist *blacklists;

int conffd = -1;
int trapfd = -1;
char *cb;
size_t cbs, cbu;

time_t t;

#define MAXCON 800
int maxcon = MAXCON;
int maxblack = MAXCON;
int blackcount;
int clients;
int debug;
int greylist;
int grey_stutter = 10;
int verbose;
int stutter = 1;
int window;
#define MAXTIME 400

void
usage(void)
{
	fprintf(stderr,
	    "usage: spamd [-45dgv] [-B maxblack] [-b address] [-c maxcon]\n");
	fprintf(stderr,
	    "             [-G mins:hours:hours] [-n name] [-p port]\n");
	fprintf(stderr,
	    "             [-r reply] [-S secs] [-s secs] [-w window]\n");
	exit(1);
}

char *
grow_obuf(struct con *cp, int off)
{
	char *tmp;

	tmp = realloc(cp->obuf, cp->osize + 8192);
	if (tmp == NULL) {
		free(cp->obuf);
		cp->obuf = NULL;
		cp->osize = 0;
		return (NULL);
	} else {
		cp->osize += 8192;
		cp->obuf = tmp;
		return (cp->obuf + off);
	}
}

int
parse_configline(char *line)
{
	char *cp, prev, *name, *msg;
	static char **av = NULL;
	static size_t ac = 0;
	size_t au = 0;
	int mdone = 0;

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
			if (tmp == NULL) {
				free(av);
				av = NULL;
				ac = 0;
				return (-1);
			}
			av = tmp;
			ac += 2048;
		}
	} while ((av[au++] = strsep(&cp, ";")) != NULL);

	/* toss empty last entry to allow for trailing ; */
	while (au > 0 && (av[au - 1] == NULL || av[au - 1][0] == '\0'))
		au--;

	if (au < 1)
		goto parse_error;
	else
		sdl_add(name, msg, av, au);
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
read_configline(FILE *config)
{
	char *buf;
	size_t len;

	if ((buf = fgetln(config, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else
			return(-1);	/* all valid lines end in \n */
		parse_configline(buf);
	} else {
		syslog_r(LOG_DEBUG, &sdata, "read_configline: fgetln (%m)");
		return(-1);
	}
	return(0);
}

int
append_error_string(struct con *cp, size_t off, char *fmt, int af, void *ia)
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
	snprintf(c, len, "%s ", nreply);
	i += strlen(c);
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
				return (-1);
			len = cp->osize - (off + i);
		}

		if (c[i-1] == '\n') {
			if (lastcont != 0)
				cp->obuf[lastcont] = '-';
			snprintf(c + i, len, "%s ", nreply);
			i += strlen(c);
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
}

char *
loglists(struct con *cp)
{
	static char matchlists[80];
	struct sdlist **matches;
	int s = sizeof(matchlists) - 4;

	matchlists[0] = '\0';
	matches = cp->blacklists;
	if (matches == NULL)
		return(NULL);
	for (; *matches; matches++) {

		/* don't report an insane amount of lists in the logs.
		 * just truncate and indicate with ...
		 */
		if (strlen(matchlists) + strlen(matches[0]->tag) + 1
		    >= s)
			strlcat(matchlists, " ...", sizeof(matchlists));
		else {
			strlcat(matchlists, " ", s);
			strlcat(matchlists, matches[0]->tag, s);
		}
	}
	return matchlists;
}

void
build_reply(struct con *cp)
{
	struct sdlist **matches;
	int off = 0;

	matches = cp->blacklists;
	if (matches == NULL)
		goto nomatch;
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
				if (c == NULL)
					goto bad;
			}
			cp->obuf[off++] = '\n';
			cp->obuf[off] = '\0';
		}
	}
	return;
nomatch:
	/* No match. give generic reply */
	free(cp->obuf);
	cp->obuf = NULL;
	cp->osize = 0;
	if (cp->blacklists != NULL)
		asprintf(&cp->obuf,
		    "%s-Sorry %s\n"
		    "%s-You are trying to send mail from an address "
		    "listed by one\n"
		    "%s or more IP-based registries as being a SPAM source.\n",
		    nreply, cp->addr, nreply, nreply);
	else
		asprintf(&cp->obuf,
		    "451 Temporary failure, please try again later.\r\n");
	if (cp->obuf != NULL)
		cp->osize = strlen(cp->obuf) + 1;
	else
		cp->osize = 0;
	return;
bad:
	if (cp->obuf != NULL) {
		free(cp->obuf);
		cp->obuf = NULL;
		cp->osize = 0;
	}
}

void
doreply(struct con *cp)
{
	if (reply)
		snprintf(cp->obuf, cp->osize, "%s %s\n", nreply, reply);
	build_reply(cp);
}

void
setlog(char *p, size_t len, char *f)
{
	char *s;

	s = strsep(&f, ":");
	if (!f)
		return;
	while (*f == ' ' || *f == '\t')
		f++;
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
initcon(struct con *cp, int fd, struct sockaddr *sa)
{
	time_t t;
	char *tmp;
	int error;

	time(&t);
	free(cp->obuf);
	cp->obuf = NULL;
	cp->osize = 0;
	free(cp->blacklists);
	cp->blacklists = NULL;
	free(cp->lists);
	cp->lists = NULL;
	bzero(cp, sizeof(struct con));
	if (grow_obuf(cp, 0) == NULL)
		err(1, "malloc");
	cp->fd = fd;
	if (sa->sa_len > sizeof(cp->ss))
		errx(1, "sockaddr size");
	if (sa->sa_family != AF_INET)
		errx(1, "not supported yet");
	memcpy(&cp->ss, sa, sa->sa_len);
	cp->af = sa->sa_family;
	cp->ia = &((struct sockaddr_in *)sa)->sin_addr;
	cp->blacklists = sdl_lookup(blacklists, cp->af, cp->ia);
	cp->stutter = (greylist && !grey_stutter && cp->blacklists == NULL) ?
	    0 : stutter;
	error = getnameinfo(sa, sa->sa_len, cp->addr, sizeof(cp->addr), NULL, 0,
	    NI_NUMERICHOST);
	if (error)
		errx(1, "%s", gai_strerror(error));
	tmp = strdup(ctime(&t));
	if (tmp == NULL)
		err(1, "malloc");
	tmp[strlen(tmp) - 1] = '\0'; /* nuke newline */
	snprintf(cp->obuf, cp->osize, "220 %s ESMTP %s; %s\r\n",
	    hostname, spamd, tmp);
	free(tmp);
	cp->op = cp->obuf;
	cp->ol = strlen(cp->op);
	cp->w = t + cp->stutter;
	cp->s = t;
	strlcpy(cp->rend, "\n", sizeof cp->rend);
	clients++;
	if (cp->blacklists != NULL) {
		blackcount++;
		if (greylist && blackcount > maxblack) {
			closecon(cp); /* close and free */
			return;
		}
		cp->lists = strdup(loglists(cp));
	}
	else
		cp->lists = NULL;
}

void
closecon(struct con *cp)
{
	time_t t;

	time(&t);
	syslog_r(LOG_INFO, &sdata, "%s: disconnected after %ld seconds.%s%s",
	    cp->addr, (long)(t - cp->s),
	    ((cp->lists == NULL) ? "" : " lists:"),
	    ((cp->lists == NULL) ? "": cp->lists));
	if (debug > 0)
		printf("%s connected for %ld seconds.\n", cp->addr,
		    (long)(t - cp->s));
	if (cp->lists != NULL) {
		free(cp->lists);
		cp->lists = NULL;
	}
	if (cp->blacklists != NULL) {
		blackcount--;
		free(cp->blacklists);
		cp->blacklists = NULL;
	}
	if (cp->obuf != NULL) {
		free(cp->obuf);
		cp->obuf = NULL;
		cp->osize = 0;
	}
	close(cp->fd);
	clients--;
	cp->fd = -1;
}

int
match(const char *s1, const char *s2)
{
	return (strncasecmp(s1, s2, strlen(s2)) == 0);
}

void
nextstate(struct con *cp)
{
	if (match(cp->ibuf, "QUIT") && cp->state < 99) {
		snprintf(cp->obuf, cp->osize, "221 %s\r\n", hostname);
		cp->op = cp->obuf;
		cp->ol = strlen(cp->op);
		cp->w = t + cp->stutter;
		cp->laststate = cp->state;
		cp->state = 99;
		return;
	}

	if (match(cp->ibuf, "RSET") && cp->state > 2 && cp->state < 50) {
		snprintf(cp->obuf, cp->osize,
		    "250 Ok to start over.\r\n");
		cp->op = cp->obuf;
		cp->ol = strlen(cp->op);
		cp->w = t + cp->stutter;
		cp->laststate = cp->state;
		cp->state = 2;
		return;
	}
	switch (cp->state) {
	case 0:
		/* banner sent; wait for input */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->laststate = cp->state;
		cp->state = 1;
		cp->r = t;
		break;
	case 1:
		/* received input: parse, and select next state */
		if (match(cp->ibuf, "HELO") ||
		    match(cp->ibuf, "EHLO")) {
			snprintf(cp->obuf, cp->osize,
			    "250 Hello, spam sender. "
			    "Pleased to be wasting your time.\r\n");
			cp->op = cp->obuf;
			cp->ol = strlen(cp->op);
			cp->laststate = cp->state;
			cp->state = 2;
			cp->w = t + cp->stutter;
			break;
		}
		goto mail;
	case 2:
		/* sent 250 Hello, wait for input */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->laststate = cp->state;
		cp->state = 3;
		cp->r = t;
		break;
	mail:
	case 3:
		if (match(cp->ibuf, "MAIL")) {
			setlog(cp->mail, sizeof cp->mail, cp->ibuf);
			snprintf(cp->obuf, cp->osize,
			    "250 You are about to try to deliver spam. "
			    "Your time will be spent, for nothing.\r\n");
			cp->op = cp->obuf;
			cp->ol = strlen(cp->op);
			cp->laststate = cp->state;
			cp->state = 4;
			cp->w = t + cp->stutter;
			break;
		}
		goto rcpt;
	case 4:
		/* sent 250 Sender ok */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->laststate = cp->state;
		cp->state = 5;
		cp->r = t;
		break;
	rcpt:
	case 5:
		if (match(cp->ibuf, "RCPT")) {
			setlog(cp->rcpt, sizeof(cp->rcpt), cp->ibuf);
			snprintf(cp->obuf, cp->osize,
			    "250 This is hurting you more than it is "
			    "hurting me.\r\n");
			cp->op = cp->obuf;
			cp->ol = strlen(cp->op);
			cp->laststate = cp->state;
			cp->state = 6;
			cp->w = t + cp->stutter;
			if (cp->mail[0] && cp->rcpt[0]) {
				if (verbose)
					syslog_r(LOG_INFO, &sdata,
					    "(%s) %s: %s -> %s",
					    cp->blacklists ? "BLACK" : "GREY",
					    cp->addr, cp->mail,
					    cp->rcpt);
				if (debug)
					fprintf(stderr, "(%s) %s: %s -> %s\n",
					    cp->blacklists ? "BLACK" : "GREY",
					    cp->addr, cp->mail, cp->rcpt);
				if (greylist && cp->blacklists == NULL) {
					/* send this info to the greylister */
					fprintf(grey, "IP:%s\nFR:%s\nTO:%s\n",
					    cp->addr, cp->mail, cp->rcpt);
					fflush(grey);
					cp->laststate = cp->state;
					cp->state = 98;
					goto done;
				}
			}
			break;
		}
		goto spam;
	case 6:
		/* sent 250 blah */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->laststate = cp->state;
		cp->state = 5;
		cp->r = t;
		break;

	spam:
	case 50:
		if (match(cp->ibuf, "DATA")) {
			snprintf(cp->obuf, cp->osize,
			    "354 Enter spam, end with \".\" on a line by "
			    "itself\r\n");
			cp->state = 60;
			if (window && setsockopt(cp->fd, SOL_SOCKET, SO_RCVBUF,
			    &window, sizeof(window)) == -1) {
				syslog_r(LOG_DEBUG, &sdata,"setsockopt: %m");
				/* don't fail if this doesn't work. */
			}
		} else {
			snprintf(cp->obuf, cp->osize,
			    "500 5.5.1 Command unrecognized\r\n");
			cp->state = cp->laststate;
		}
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->op = cp->obuf;
		cp->ol = strlen(cp->op);
		cp->w = t + cp->stutter;
		break;
	case 60:
		/* sent 354 blah */
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->laststate = cp->state;
		cp->state = 70;
		cp->r = t;
		break;
	case 70: {
		char *p, *q;

		for (p = q = cp->ibuf; q <= cp->ip; ++q)
			if (*q == '\n' || q == cp->ip) {
				*q = 0;
				if (q > p && q[-1] == '\r')
					q[-1] = 0;
				if (!strcmp(p, ".") ||
				    (cp->data_body && ++cp->data_lines >= 10)) {
					cp->laststate = cp->state;
					cp->state = 98;
					goto done;
				}
				if (!cp->data_body && !*p)
					cp->data_body = 1;
				if (verbose && cp->data_body && *p)
					syslog_r(LOG_DEBUG, &sdata, "%s: "
					    "Body: %s", cp->addr, p);
				else if (verbose && (match(p, "FROM:") ||
				    match(p, "TO:") || match(p, "SUBJECT:")))
					syslog_r(LOG_INFO, &sdata, "%s: %s",
					    cp->addr, p);
				p = ++q;
			}
		cp->ip = cp->ibuf;
		cp->il = sizeof(cp->ibuf) - 1;
		cp->r = t;
		break;
	}
	done:
	case 98:
		doreply(cp);
		cp->op = cp->obuf;
		cp->ol = strlen(cp->op);
		cp->w = t + cp->stutter;
		cp->laststate = cp->state;
		cp->state = 99;
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
	int n;

	if (cp->r) {
		n = read(cp->fd, cp->ip, cp->il);
		if (n == 0)
			closecon(cp);
		else if (n == -1) {
			if (debug > 0)
				perror("read()");
			closecon(cp);
		} else {
			cp->ip[n] = '\0';
			if (cp->rend[0])
				if (strpbrk(cp->ip, cp->rend))
					end = 1;
			cp->ip += n;
			cp->il -= n;
		}
	}
	if (end || cp->il == 0) {
		while (cp->ip > cp->ibuf &&
		    (cp->ip[-1] == '\r' || cp->ip[-1] == '\n'))
			cp->ip--;
		*cp->ip = '\0';
		cp->r = 0;
		nextstate(cp);
	}
}

void
handlew(struct con *cp, int one)
{
	int n;

	/* kill stutter on greylisted connections after initial delay */
	if (cp->stutter && greylist && cp->blacklists == NULL &&
	    (t - cp->s) > grey_stutter)
		cp->stutter=0;

	if (cp->w) {
		if (*cp->op == '\n' && !cp->sr) {
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
		if (*cp->op == '\r')
			cp->sr = 1;
		else
			cp->sr = 0;
		n = write(cp->fd, cp->op, (one && cp->stutter) ? 1 : cp->ol);
		if (n == 0)
			closecon(cp);
		else if (n == -1) {
			if (debug > 0 && errno != EPIPE)
				perror("write()");
			closecon(cp);
		} else {
			cp->op += n;
			cp->ol -= n;
		}
	}
handled:
	cp->w = t + cp->stutter;
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
	int ch, s, s2, conflisten = 0, i, omax = 0, one = 1;
	socklen_t sinlen;
	u_short port;
	struct servent *ent;
	struct rlimit rlp;
	char *bind_address = NULL;
	const char *errstr;

	tzset();
	openlog_r("spamd", LOG_PID | LOG_NDELAY, LOG_DAEMON, &sdata);

	if ((ent = getservbyname("spamd", "tcp")) == NULL)
		errx(1, "Can't find service \"spamd\" in /etc/services");
	port = ntohs(ent->s_port);
	if ((ent = getservbyname("spamd-cfg", "tcp")) == NULL)
		errx(1, "Can't find service \"spamd-cfg\" in /etc/services");
	cfg_port = ntohs(ent->s_port);

	if (gethostname(hostname, sizeof hostname) == -1)
		err(1, "gethostname");

	while ((ch = getopt(argc, argv, "45b:c:B:p:dgG:r:s:S:n:vw:")) != -1) {
		switch (ch) {
		case '4':
			nreply = "450";
			break;
		case '5':
			nreply = "550";
			break;
		case 'b':
			bind_address = optarg;
			break;
		case 'B':
			i = atoi(optarg);
			maxblack = i;
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
		case 'g':
			greylist = 1;
			break;
		case 'G':
			if (sscanf(optarg, "%d:%d:%d", &passtime, &greyexp,
			    &whiteexp) != 3)
				usage();
			/* convert to seconds from minutes */
			passtime *= 60;
			/* convert to seconds from hours */
			whiteexp *= (60 * 60);
			/* convert to seconds from hours */
			greyexp *= (60 * 60);
			break;
		case 'r':
			reply = optarg;
			break;
		case 's':
			i = atoi(optarg);
			if (i < 0 || i > 10)
				usage();
			stutter = i;
			break;
		case 'S':
			i = strtonum(optarg, 0, 90, &errstr);
			if (errstr)
				usage();
			grey_stutter = i;
			break;
		case 'n':
			spamd = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			window = atoi(optarg);
			if (window <= 0)
				usage();
			break;
		default:
			usage();
			break;
		}
	}

	if (!greylist)
		maxblack = maxcon;
	else if (maxblack > maxcon)
		usage();

	rlp.rlim_cur = rlp.rlim_max = maxcon + 15;
	if (setrlimit(RLIMIT_NOFILE, &rlp) == -1)
		err(1, "setrlimit");

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
	if (bind_address) {
		if (inet_pton(AF_INET, bind_address, &sin.sin_addr) != 1)
			err(1, "inet_pton");
	} else
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

	if (debug == 0) {
		if (daemon(1, 1) == -1)
			err(1, "daemon");
	}

	if (greylist) {
		maxblack = (maxblack >= maxcon) ? maxcon - 100 : maxblack;
		if (maxblack < 0)
			maxblack = 0;

		/* open pipe to talk to greylister */
		if (pipe(greypipe) == -1) {
			syslog(LOG_ERR, "pipe (%m)");
			exit(1);
		}
		/* open pipe to recieve spamtrap configs */
		if (pipe(trappipe) == -1) {
			syslog(LOG_ERR, "pipe (%m)");
			exit(1);
		}
		jail_pid = fork();
		switch(jail_pid) {
		case -1:
			syslog(LOG_ERR, "fork (%m)");
			exit(1);
		case 0:
			/* child - continue */
			signal(SIGPIPE, SIG_IGN);
			grey = fdopen(greypipe[1], "w");
			if (grey == NULL) {
				syslog(LOG_ERR, "fdopen (%m)");
				_exit(1);
			}
			close(greypipe[0]);
			trapfd = trappipe[0];
			trapcfg = fdopen(trappipe[0], "r");
			if (trapcfg == NULL) {
				syslog(LOG_ERR, "fdopen (%m)");
				_exit(1);
			}
			close(trappipe[1]);
			goto jail;
		}
		/* parent - run greylister */
		grey = fdopen(greypipe[0], "r");
		if (grey == NULL) {
			syslog(LOG_ERR, "fdopen (%m)");
			exit(1);
		}
		close(greypipe[1]);
		trapcfg = fdopen(trappipe[1], "w");
		if (trapcfg == NULL) {
			syslog(LOG_ERR, "fdopen (%m)");
			exit(1);
		}
		close(trappipe[0]);
		return(greywatcher());
		/* NOTREACHED */
	}

jail:
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

	if (debug != 0)
		printf("listening for incoming connections.\n");
	syslog_r(LOG_WARNING, &sdata, "listening for incoming connections.");

	while (1) {
		struct timeval tv, *tvp;
		int max, i, n;
		int writers;

		max = MAX(s, conflisten);
		max = MAX(max, conffd);
		max = MAX(max, trapfd);

		time(&t);
		for (i = 0; i < maxcon; i++)
			if (con[i].fd != -1)
				max = MAX(max, con[i].fd);

		if (max > omax) {
			free(fdsr);
			fdsr = NULL;
			free(fdsw);
			fdsw = NULL;
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
		if (trapfd != -1)
			FD_SET(trapfd, fdsr);

		if (writers == 0) {
			tvp = NULL;
		} else {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			tvp = &tv;
		}

		n = select(max+1, fdsr, fdsw, NULL, tvp);
		if (n == -1) {
			if (errno != EINTR)
				err(1, "select");
			continue;
		}
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
			if (s2 == -1)
				/* accept failed, they may try again */
				continue;
			for (i = 0; i < maxcon; i++)
				if (con[i].fd == -1)
					break;
			if (i == maxcon)
				close(s2);
			else {
				initcon(&con[i], s2, (struct sockaddr *)&sin);
				syslog_r(LOG_INFO, &sdata,
				    "%s: connected (%d/%d)%s%s",
				    con[i].addr, clients, blackcount,
				    ((con[i].lists == NULL) ? "" :
				    ", lists:"),
				    ((con[i].lists == NULL) ? "":
				    con[i].lists));
			}
		}
		if (FD_ISSET(conflisten, fdsr)) {
			sinlen = sizeof(lin);
			conffd = accept(conflisten, (struct sockaddr *)&lin,
			    &sinlen);
			if (conffd == -1)
				/* accept failed, they may try again */
				continue;
			else if (ntohs(lin.sin_port) >= IPPORT_RESERVED) {
				close(conffd);
				conffd = -1;
			}
		}
		if (conffd != -1 && FD_ISSET(conffd, fdsr))
			do_config();
		if (trapfd != -1 && FD_ISSET(trapfd, fdsr))
			read_configline(trapcfg);
	}
	exit(1);
}
