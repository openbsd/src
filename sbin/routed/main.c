/*	$OpenBSD: main.c,v 1.2 1996/06/23 14:32:29 deraadt Exp $	*/
/*	$NetBSD: main.c,v 1.13 1995/06/20 22:27:53 christos Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$OpenBSD: main.c,v 1.2 1996/06/23 14:32:29 deraadt Exp $";
#endif
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/ioctl.h>
#include <sys/file.h>

#include <net/if.h>

#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include "pathnames.h"

int	supplier = -1;		/* process should supply updates */
int	gateway = 0;		/* 1 if we are a gateway to parts beyond */
int	debug = 0;
int	bufspace = 127*1024;	/* max. input buffer size to request */
struct	rip *msg = (struct rip *)packet;

int getsocket __P((int, int, struct sockaddr_in *));
void process __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int n, nfd, tflags = 0;
	struct timeval *tvp, waittime;
	struct itimerval itval;
	register struct rip *query = msg;
	fd_set ibits;
	sigset_t sigset, osigset;
	
	argv0 = argv;
#if BSD >= 43
	openlog("routed", LOG_PID | LOG_ODELAY, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOG_WARNING));
#else
	openlog("routed", LOG_PID);
#define LOG_UPTO(x) (x)
#define setlogmask(x) (x)
#endif
	sp = getservbyname("router", "udp");
	if (sp == NULL) {
		fprintf(stderr, "routed: router/udp: unknown service\n");
		exit(1);
	}
	addr.sin_family = AF_INET;
	addr.sin_port = sp->s_port;
	r = socket(AF_ROUTE, SOCK_RAW, 0);
	/* later, get smart about lookingforinterfaces */
	if (r)
		shutdown(r, 0); /* for now, don't want reponses */
	else {
		fprintf(stderr, "routed: no routing socket\n");
		exit(1);
	}
	s = getsocket(AF_INET, SOCK_DGRAM, &addr);
	if (s < 0)
		exit(1);
	argv++, argc--;
	while (argc > 0 && **argv == '-') {
		if (strcmp(*argv, "-s") == 0) {
			supplier = 1;
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-q") == 0) {
			supplier = 0;
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-t") == 0) {
			tflags++;
			setlogmask(LOG_UPTO(LOG_DEBUG));
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-d") == 0) {
			debug++;
			setlogmask(LOG_UPTO(LOG_DEBUG));
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-g") == 0) {
			gateway = 1;
			argv++, argc--;
			continue;
		}
		fprintf(stderr,
			"usage: routed [ -s ] [ -q ] [ -t ] [ -g ]\n");
		exit(1);
	}

	if (debug == 0 && tflags == 0)
		daemon(0, 0);
	/*
	 * Any extra argument is considered
	 * a tracing log file.
	 */
	if (argc > 0)
		traceon(*argv);
	while (tflags-- > 0)
		bumploglevel();

	(void) gettimeofday(&now, NULL);
	/*
	 * Collect an initial view of the world by
	 * checking the interface configuration and the gateway kludge
	 * file.  Then, send a request packet on all
	 * directly connected networks to find out what
	 * everyone else thinks.
	 */
	rtinit();
	ifinit();
	gwkludge();
	if (gateway > 0)
		rtdefault();
	if (supplier < 0)
		supplier = 0;
	query->rip_cmd = RIPCMD_REQUEST;
	query->rip_vers = RIP_VERSION_1;
	if (sizeof(query->rip_nets[0].rip_family) > 1)	/* XXX */
		query->rip_nets[0].rip_family = htons((u_short)AF_UNSPEC);
	else
		query->rip_nets[0].rip_family = AF_UNSPEC;
	query->rip_nets[0].rip_metric = htonl((u_long)HOPCNT_INFINITY);
	toall(sndmsg, 0, NULL);
	signal(SIGALRM, timer);
	signal(SIGHUP, hup);
	signal(SIGTERM, hup);
	signal(SIGINT, rtdeleteall);
	signal(SIGUSR1, sigtrace);
	signal(SIGUSR2, sigtrace);
	itval.it_interval.tv_sec = TIMER_RATE;
	itval.it_value.tv_sec = TIMER_RATE;
	itval.it_interval.tv_usec = 0;
	itval.it_value.tv_usec = 0;
	srandom(getpid());
	if (setitimer(ITIMER_REAL, &itval, NULL) < 0)
		syslog(LOG_ERR, "setitimer: %m\n");

	FD_ZERO(&ibits);
	nfd = s + 1;			/* 1 + max(fd's) */
	for (;;) {
		FD_SET(s, &ibits);
		/*
		 * If we need a dynamic update that was held off,
		 * needupdate will be set, and nextbcast is the time
		 * by which we want select to return.  Compute time
		 * until dynamic update should be sent, and select only
		 * until then.  If we have already passed nextbcast,
		 * just poll.
		 */
		if (needupdate) {
			timersub(&nextbcast, &now, &waittime);
			if (waittime.tv_sec < 0) {
				waittime.tv_sec = 0;
				waittime.tv_usec = 0;
			}
			if (traceactions)
				fprintf(ftrace,
				 "select until dynamic update %d/%d sec/usec\n",
				    waittime.tv_sec, waittime.tv_usec);
			tvp = &waittime;
		} else
			tvp = NULL;
		n = select(nfd, &ibits, 0, 0, tvp);
		if (n <= 0) {
			/*
			 * Need delayed dynamic update if select returned
			 * nothing and we timed out.  Otherwise, ignore
			 * errors (e.g. EINTR).
			 */
			if (n < 0) {
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "select: %m");
			}
			sigemptyset(&sigset);
			sigaddset(&sigset, SIGALRM);
			sigprocmask(SIG_BLOCK, &sigset, &osigset);
			if (n == 0 && needupdate) {
				if (traceactions)
					fprintf(ftrace,
					    "send delayed dynamic update\n");
				(void) gettimeofday(&now, NULL);
				toall(supply, RTS_CHANGED, NULL);
				lastbcast = now;
				needupdate = 0;
				nextbcast.tv_sec = 0;
			}
			sigprocmask(SIG_SETMASK, &osigset, NULL);
			continue;
		}
		(void) gettimeofday(&now, NULL);
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		sigprocmask(SIG_BLOCK, &sigset, &osigset);
#ifdef doesntwork
/*
printf("s %d, ibits %x index %d, mod %d, sh %x, or %x &ibits %x\n",
	s,
	ibits.fds_bits[0],
	(s)/(sizeof(fd_mask) * 8),
	((s) % (sizeof(fd_mask) * 8)),
	(1 << ((s) % (sizeof(fd_mask) * 8))),
	ibits.fds_bits[(s)/(sizeof(fd_mask) * 8)] & (1 << ((s) % (sizeof(fd_mask) * 8))),
	&ibits
	);
*/
		if (FD_ISSET(s, &ibits))
#else
		if (ibits.fds_bits[s/32] & (1 << s))
#endif
			process(s);
		/* handle ICMP redirects */
		sigprocmask(SIG_SETMASK, &osigset, NULL);
	}
}

void
process(fd)
	int fd;
{
	struct sockaddr from;
	int fromlen, cc;
	union {
		char	buf[MAXPACKETSIZE+1];
		struct	rip rip;
	} inbuf;

	for (;;) {
		fromlen = sizeof (from);
		cc = recvfrom(fd, &inbuf, sizeof (inbuf), 0, &from, &fromlen);
		if (cc <= 0) {
			if (cc < 0 && errno != EWOULDBLOCK)
				perror("recvfrom");
			break;
		}
		if (fromlen != sizeof (struct sockaddr_in))
			break;
		rip_input(&from, &inbuf.rip, cc);
	}
}

int
getsocket(domain, type, sin)
	int domain, type;
	struct sockaddr_in *sin;
{
	int sock, on = 1;

	if ((sock = socket(domain, type, 0)) < 0) {
		perror("socket");
		syslog(LOG_ERR, "socket: %m");
		return (-1);
	}
#ifdef SO_BROADCAST
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) < 0) {
		syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
		close(sock);
		return (-1);
	}
#endif
#ifdef SO_RCVBUF
	for (on = bufspace; ; on -= 1024) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		    &on, sizeof (on)) == 0)
			break;
		if (on <= 8*1024) {
			syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
			break;
		}
	}
	if (traceactions)
		fprintf(ftrace, "recv buf %d\n", on);
#endif
	if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) {
		perror("bind");
		syslog(LOG_ERR, "bind: %m");
		close(sock);
		return (-1);
	}
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
		syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");
	return (sock);
}
