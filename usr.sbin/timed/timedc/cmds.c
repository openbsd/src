/*	$OpenBSD: cmds.c,v 1.15 2002/09/06 19:28:01 deraadt Exp $	*/

/*-
 * Copyright (c) 1985, 1993 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)cmds.c	5.1 (Berkeley) 5/11/93";
#endif /* not lint */

#ifdef sgi
#ident "$Revision: 1.15 $"
#endif

#include "timedc.h"
#include <sys/file.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TSPTYPES
#include <protocols/timed.h>

#ifdef sgi
#include <bstring.h>
#include <sys/clock.h>
#else
#define	SECHR	(60*60)
#define	SECDAY	(24*SECHR)
#endif /* sgi */

# define DATE_PROTO "udp"
# define DATE_PORT "time"


int sock;
int sock_raw;
char myname[MAXHOSTNAMELEN];
struct hostent *hp;
struct sockaddr_in server;
struct sockaddr_in dayaddr;
extern int measure_delta;

void bytenetorder(struct tsp *);
void bytehostorder(struct tsp *);


#define BU ((unsigned long)2208988800U)	/* seconds before UNIX epoch */


/* compute the difference between our date and another machine
 */
static int				/* difference in days from our time */
daydiff(char *hostname)
{
	struct sockaddr_in from;
	struct timeval now;
	struct pollfd pfd;
	unsigned long sec;
	int i, trials;
	socklen_t fromlen;

	for (trials = 0; trials < 10; trials++) {
		/* ask for the time */
		sec = 0;

		siginterrupt(SIGINT, 1);
		if (sendto(sock, &sec, sizeof(sec), 0,
		    (struct sockaddr *)&dayaddr, sizeof(dayaddr)) < 0) {
			if (errno == EINTR && gotintr)
				goto bail;
			siginterrupt(SIGINT, 0);
			perror("sendto(sock)");
			goto bail;
		}
		siginterrupt(SIGINT, 0);

		for (;;) {
			pfd.fd = sock;
			pfd.events = POLLIN;
			i = poll(&pfd, 1, 2 * 1000);
			if (i < 0) {
				if (errno == EINTR) {
					if (gotintr)
						goto bail;
					continue;
				}
				perror("poll(date read)");
				goto bail;
			}
			if (i == 0)
				break;

			fromlen = sizeof(from);
			siginterrupt(SIGINT, 1);
			if (recvfrom(sock, &sec, sizeof(sec), 0,
			    (struct sockaddr *)&from, &fromlen) < 0) {
				if (errno == EINTR && gotintr)
					goto bail;
				siginterrupt(SIGINT, 0);
				perror("recvfrom(date read)");
				goto bail;
			}
			siginterrupt(SIGINT, 0);

			sec = ntohl(sec);
			if (sec < BU) {
				fprintf(stderr, "%s says it is before 1970: %lu",
				    hostname, sec);
				goto bail;
			}
			sec -= BU;

			(void)gettimeofday(&now, (struct timezone *)NULL);
			return (sec - now.tv_sec);
		}
	}

	/* if we get here, we tried too many times */
	fprintf(stderr,"%s will not tell us the date\n", hostname);

bail:
	siginterrupt(SIGINT, 0);	
	return (0);
}


/*
 * Clockdiff computes the difference between the time of the machine on
 * which it is called and the time of the machines given as argument.
 * The time differences measured by clockdiff are obtained using a sequence
 * of ICMP TSTAMP messages which are returned to the sender by the IP module
 * in the remote machine.
 * In order to compare clocks of machines in different time zones, the time
 * is transmitted (as a 32-bit value) in milliseconds since midnight UT.
 * If a hosts uses a different time format, it should set the high order
 * bit of the 32-bit quantity it transmits.
 * However, VMS apparently transmits the time in milliseconds since midnight
 * local time (rather than GMT) without setting the high order bit.
 * Furthermore, it does not understand daylight-saving time.  This makes
 * clockdiff behaving inconsistently with hosts running VMS.
 *
 * In order to reduce the sensitivity to the variance of message transmission
 * time, clockdiff sends a sequence of messages.  Yet, measures between
 * two `distant' hosts can be affected by a small error. The error can,
 * however, be reduced by increasing the number of messages sent in each
 * measurement.
 */
void
clockdiff(argc, argv)
	int argc;
	char *argv[];
{
	struct servent *sp;
	long avg;
	int avg_cnt;
	extern int measure(u_long, u_long, char *, struct sockaddr_in *, int);
	int measure_status;

	if (argc < 2)  {
		printf("Usage: clockdiff host ... \n");
		return;
	}

	(void)gethostname(myname,sizeof(myname));

	/* get the address for the date ready */
	sp = getservbyname(DATE_PORT, DATE_PROTO);
	if (!sp) {
		(void)fprintf(stderr, "%s/%s is an unknown service\n",
		    DATE_PORT, DATE_PROTO);
		dayaddr.sin_port = 0;
	} else
		dayaddr.sin_port = sp->s_port;

	measure_status = 0;
	while (argc > 1) {
		argc--;
		argv++;

		siginterrupt(SIGINT, 1);
		hp = gethostbyname(*argv);
		if (hp == NULL) {
			if (errno == EINTR && gotintr) {
				siginterrupt(SIGINT, 0);
				return;
			}
			siginterrupt(SIGINT, 0);
			fprintf(stderr, "timedc: %s: ", *argv);
			herror(0);
			continue;
		}
		siginterrupt(SIGINT, 0);

		server.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, &server.sin_addr.s_addr, hp->h_length);
		for (avg_cnt = 0, avg = 0; avg_cnt < 16; avg_cnt++) {
			measure_status = measure(10000, 100, *argv, &server, 1);
			if (measure_status != GOOD)
				break;
			avg += measure_delta;
		}
		if (measure_status == GOOD)
			measure_delta = avg/avg_cnt;

		switch (measure_status) {
		case HOSTDOWN:
			printf("%s is down\n", hp->h_name);
			continue;
		case NONSTDTIME:
			printf("%s transmitts a non-standard time format\n",
			    hp->h_name);
			continue;
		case UNREACHABLE:
			printf("%s is unreachable\n", hp->h_name);
			continue;
		}

		/*
		 * Try to get the date only after using ICMP timestamps to
		 * get the time.  This is because the date protocol
		 * is optional.
		 */
		if (dayaddr.sin_port != 0) {
			dayaddr.sin_family = hp->h_addrtype;
			bcopy(hp->h_addr, &dayaddr.sin_addr.s_addr,
			    hp->h_length);
			avg = daydiff(*argv);
			if (avg > SECDAY) {
				printf("time on %s is %ld days ahead %s\n",
				    hp->h_name, avg/SECDAY, myname);
				continue;
			} else if (avg < -SECDAY) {
				printf("time on %s is %ld days behind %s\n",
				    hp->h_name, -avg/SECDAY, myname);
				continue;
			}
		}

		if (measure_delta > 0) {
			printf("time on %s is %d ms. ahead of time on %s\n",
			    hp->h_name, measure_delta, myname);
		} else if (measure_delta == 0) {
			printf("%s and %s have the same time\n",
			    hp->h_name, myname);
		} else {
			printf("time on %s is %d ms. behind time on %s\n",
			    hp->h_name, -measure_delta, myname);
		}
	}
}


/*
 * finds location of master timedaemon
 */
void
msite(int argc, char *argv[])
{
	struct sockaddr_in dest, from;
	struct servent *srvp;
	int i, cc;
	socklen_t length;
	struct pollfd pfd;
	struct tsp msg;
	char *tgtname;

	if (argc < 1) {
		printf("Usage: msite [hostname]\n");
		return;
	}

	srvp = getservbyname("timed", "udp");
	if (srvp == 0) {
		fprintf(stderr, "udp/timed: unknown service\n");
		return;
	}
	memset(&dest, 0, sizeof dest);
	dest.sin_port = srvp->s_port;
	dest.sin_family = AF_INET;

	(void)gethostname(myname, sizeof(myname));
	i = 1;

	do {
		tgtname = (i >= argc) ? myname : argv[i];
		siginterrupt(SIGINT, 1);
		hp = gethostbyname(tgtname);
		if (hp == 0) {
			if (errno == EINTR && gotintr)
				goto bail;
			siginterrupt(SIGINT, 0);
			fprintf(stderr, "timedc: %s: ", tgtname);
			herror(0);
			continue;
		}

		bcopy(hp->h_addr, &dest.sin_addr.s_addr, hp->h_length);
		(void)strlcpy(msg.tsp_name, myname, sizeof msg.tsp_name);
		msg.tsp_type = TSP_MSITE;
		msg.tsp_vers = TSPVERSION;
		bytenetorder(&msg);

		if (sendto(sock, &msg, sizeof(struct tsp), 0,
		    (struct sockaddr *)&dest, sizeof(dest)) < 0) {
			if (errno == EINTR && gotintr)
				goto bail;
			siginterrupt(SIGINT, 0);
			perror("sendto");
			continue;
		}

		pfd.fd = sock;
		pfd.events = POLLIN;
		switch (poll(&pfd, 1, 15 * 1000)) {
		case -1:
			if (errno == EINTR && gotintr)
				goto bail;
			siginterrupt(SIGINT, 0);
			continue;
		case 1:
			length = sizeof(from);
			cc = recvfrom(sock, &msg, sizeof(struct tsp), 0,
			    (struct sockaddr *)&from, &length);
			if (cc < 0) {
				if (errno == EINTR && gotintr)
					goto bail;
				siginterrupt(SIGINT, 0);
				perror("recvfrom");
				continue;
			}
			siginterrupt(SIGINT, 0);

			if (cc < sizeof(struct tsp)) {
				fprintf(stderr,
				    "short packet (%u/%u bytes) from %s\n",
				    cc, sizeof(struct tsp),
				    inet_ntoa(from.sin_addr));
				continue;
			}
			bytehostorder(&msg);
			if (msg.tsp_type == TSP_ACK) {
				printf("master timedaemon at %s is %s\n",
				    tgtname, msg.tsp_name);
			} else {
				if (msg.tsp_type >= TSPTYPENUMBER)
					printf("received unknown ack: %u\n",
					    msg.tsp_type);
				else
					printf("received wrong ack: %s\n",
					    tsptype[msg.tsp_type]);
			}
			break;
		case 0:
			siginterrupt(SIGINT, 0);
			printf("communication error with %s\n", tgtname);
			break;
		}
	} while (++i < argc);

bail:
	siginterrupt(SIGINT, 0);
}

/*
 * quits timedc
 */
void
quit()
{
	exit(0);
}


/*
 * Causes the election timer to expire on the selected hosts
 * It sends just one udp message per machine, relying on
 * reliability of communication channel.
 */
void
testing(int argc, char *argv[])
{
	struct sockaddr_in sin;
	struct tsp msg;
	struct servent *srvp;

	if (argc < 2)  {
		printf("Usage: election host1 [host2 ...]\n");
		return;
	}

	srvp = getservbyname("timed", "udp");
	if (srvp == 0) {
		fprintf(stderr, "udp/timed: unknown service\n");
		return;
	}

	while (argc > 1) {
		argc--;
		argv++;

		siginterrupt(SIGINT, 1);
		hp = gethostbyname(*argv);
		if (hp == NULL) {
			if (errno == EINTR && gotintr)
				goto bail;
			siginterrupt(SIGINT, 0);
			fprintf(stderr, "timedc: %s: ", *argv);
			herror(0);
			argc--;
			argv++;
			continue;
		}

		memset(&sin, 0, sizeof sin);
		sin.sin_port = srvp->s_port;
		sin.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, &sin.sin_addr.s_addr, hp->h_length);

		msg.tsp_type = TSP_TEST;
		msg.tsp_vers = TSPVERSION;
		(void)gethostname(myname, sizeof(myname));
		(void)strncpy(msg.tsp_name, myname, sizeof(msg.tsp_name));
		bytenetorder(&msg);

		if (sendto(sock, &msg, sizeof(struct tsp), 0,
		    (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			if (errno == EINTR && gotintr)
				goto bail;
			siginterrupt(SIGINT, 0);
			perror("sendto");
		}
	}
bail:
	siginterrupt(SIGINT, 0);
}


/*
 * Enables or disables tracing on local timedaemon
 */
void
tracing(int argc, char *argv[])
{
	struct sockaddr_in dest;
	struct sockaddr_in from;
	struct tsp msg;
	struct servent *srvp;
	struct pollfd pfd;
	int cc, onflag;
	socklen_t length;

	if (argc != 2) {
		printf("Usage: tracing { on | off }\n");
		return;
	}

	srvp = getservbyname("timed", "udp");
	if (srvp == 0) {
		fprintf(stderr, "udp/timed: unknown service\n");
		return;
	}

	memset(&dest, 0, sizeof dest);
	dest.sin_port = srvp->s_port;
	dest.sin_family = AF_INET;

	(void)gethostname(myname,sizeof(myname));
	siginterrupt(SIGINT, 1);
	hp = gethostbyname(myname);
	if (hp == NULL && errno == EINTR && gotintr)
		goto bail;

	bcopy(hp->h_addr, &dest.sin_addr.s_addr, hp->h_length);

	if (strcmp(argv[1], "on") == 0) {
		msg.tsp_type = TSP_TRACEON;
		onflag = ON;
	} else {
		msg.tsp_type = TSP_TRACEOFF;
		onflag = OFF;
	}

	(void)strlcpy(msg.tsp_name, myname, sizeof msg.tsp_name);
	msg.tsp_vers = TSPVERSION;
	bytenetorder(&msg);
	if (sendto(sock, &msg, sizeof(struct tsp), 0,
	    (struct sockaddr *)&dest, sizeof(dest)) < 0) {
		if (errno == EINTR && gotintr)
			goto bail;
		siginterrupt(SIGINT, 0);
		perror("sendto");
		return;
	}

	pfd.fd = sock;
	pfd.events = POLLIN;
	switch (poll(&pfd, 1, 5 * 1000)) {
	case 1:
		length = sizeof(from);
		cc = recvfrom(sock, &msg, sizeof(struct tsp), 0,
		    (struct sockaddr *)&from, &length);
		if (cc < 0) {
			if (errno == EINTR && gotintr)
				goto bail;
			siginterrupt(SIGINT, 0);
			perror("recvfrom");
			return;
		}
		siginterrupt(SIGINT, 0);
		if (cc < sizeof(struct tsp)) {
			fprintf(stderr, "short packet (%u/%u bytes) from %s\n",
			    cc, sizeof(struct tsp), inet_ntoa(from.sin_addr));
			goto bail;
		}
		bytehostorder(&msg);
		if (msg.tsp_type == TSP_ACK) {
			if (onflag)
				printf("timed tracing enabled\n");
			else
				printf("timed tracing disabled\n");
		} else  {
			if (msg.tsp_type >= TSPTYPENUMBER)
				printf("unknown ack received: %u\n",
				    msg.tsp_type);
			else
				printf("wrong ack received: %s\n",
				    tsptype[msg.tsp_type]);
		}
		break;
	case 0:
		siginterrupt(SIGINT, 0);
		printf("communication error\n");
		break;
	}
bail:
	siginterrupt(SIGINT, 0);
}
