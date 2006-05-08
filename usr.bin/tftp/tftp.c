/*	$OpenBSD: tftp.c,v 1.16 2006/05/08 13:02:51 claudio Exp $	*/
/*	$NetBSD: tftp.c,v 1.5 1995/04/29 05:55:25 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
#if 0
static char sccsid[] = "@(#)tftp.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] = "$OpenBSD: tftp.c,v 1.16 2006/05/08 13:02:51 claudio Exp $";
#endif /* not lint */

/* Many bug fixes are from Jim Guyton <guyton@rand-unix> */

/*
 * TFTP User Program -- Protocol Machines
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/tftp.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "extern.h"
#include "tftpsubs.h"

#define	PKTSIZE	SEGSIZE + 4

extern struct sockaddr_in	peeraddr;	/* filled in by main */
extern int			f;		/* the opened socket */
extern int			trace;
extern int			verbose;
extern int			rexmtval;
extern int			maxtimeout;
extern FILE			*file;
extern volatile sig_atomic_t	intrflag;

char	ackbuf[PKTSIZE];

struct timeval	tstart;
struct timeval	tstop;

struct errmsg {
	int	e_code;
	char	*e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ -1,		NULL }
};

static int	makerequest(int, const char *, struct tftphdr *, const char *);
static void	nak(int);
static void 	tpacket(const char *, struct tftphdr *, int);
static void	startclock(void);
static void	stopclock(void);
static void	printstats(const char *, unsigned long);
static void	printtimeout(void);

/*
 * Send the requested file.
 */
void
sendfile(int fd, char *name, char *mode)
{
	struct tftphdr *dp, *ap;	/* data and ack packets */
	struct sockaddr_in from;
	struct pollfd pfd[1];
	unsigned long amount;
	int convert;			/* true if converting crlf -> lf */
	int n, nfds, error, fromlen, timeouts, block, size;

	startclock();		/* start stat's clock */
	dp = r_init();		/* reset fillbuf/read-ahead code */
	ap = (struct tftphdr *)ackbuf;
	file = fdopen(fd, "r");
	convert = !strcmp(mode, "netascii");
	block = 0;
	amount = 0;

	do {
		/* read data from file */
		if (!block)
			size = makerequest(WRQ, name, dp, mode) - 4;
		else {
			size = readit(file, &dp, convert);
			if (size < 0) {
				nak(errno + 100);
				break;
			}
			dp->th_opcode = htons((u_short)DATA);
			dp->th_block = htons((u_short)block);
		}

		/* send data to server and wait for server ACK */
		for (timeouts = 0, error = 0; !intrflag;) {
			if (timeouts == maxtimeout) {
				printtimeout();
				goto abort;
			}

			if (!error) {
				if (trace)
					tpacket("sent", dp, size + 4);
				if (sendto(f, dp, size + 4, 0,
		    		    (struct sockaddr *)&peeraddr,
				    sizeof(peeraddr)) != size + 4) {
					warn("sendto");
					goto abort;
				}
				read_ahead(file, convert);
			}
			error = 0;

			pfd[0].fd = f;
			pfd[0].events = POLLIN;
			nfds = poll(pfd, 1, rexmtval * 1000);
			if (nfds == 0) {
				timeouts++;
				continue;
			}
			if (nfds == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				warn("poll");
				goto abort;
			}
			fromlen = sizeof(from);
			n = recvfrom(f, ackbuf, sizeof(ackbuf), 0,
			    (struct sockaddr *)&from, &fromlen);
			if (n == 0) {
				warn("recvfrom");
				goto abort;
			}
			if (n == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				warn("recvfrom");
				goto abort;
			}
			peeraddr.sin_port = from.sin_port;	/* added */
			if (trace)
				tpacket("received", ap, n);
			ap->th_opcode = ntohs(ap->th_opcode);
			ap->th_block = ntohs(ap->th_block);

			if (ap->th_opcode == ERROR) {
				printf("Error code %d: %s\n",
				    ap->th_code, ap->th_msg);
				goto abort;
			}
			if (ap->th_opcode == ACK) {
				int j;
				if (ap->th_block == block)
					break;
				/* re-synchronize with other side */
				j = synchnet(f);
				if (j && trace)
					printf("discarded %d packets\n", j);
				if (ap->th_block == (block - 1))
					continue;
			}
			error = 1;	/* received packet does not match */
		}

		if (block > 0)
			amount += size;
		block++;
	} while ((size == SEGSIZE || block == 1) && !intrflag);

abort:
	fclose(file);
	stopclock();
	if (amount > 0) {
		if (intrflag)
			putchar('\n');
		printstats("Sent", amount);
	}
}

/*
 * Receive a file.
 */
void
recvfile(int fd, char *name, char *mode)
{
	struct tftphdr *dp, *ap;	/* data and ack packets */
	struct sockaddr_in from;
	struct pollfd pfd[1];
	unsigned long amount;
	int convert;			/* true if converting crlf -> lf */
	int n, nfds, error, fromlen, timeouts, block, size, firsttrip;

	startclock();		/* start stat's clock */
	dp = w_init();		/* reset fillbuf/read-ahead code */
	ap = (struct tftphdr *)ackbuf;
	file = fdopen(fd, "w");
	convert = !strcmp(mode, "netascii");
	n = 0;
	block = 1;
	amount = 0;
	firsttrip = 1;

	do {
		/* create new ACK packet */
		if (firsttrip) {
			size = makerequest(RRQ, name, ap, mode);
			firsttrip = 0;
		} else {
			ap->th_opcode = htons((u_short)ACK);
			ap->th_block = htons((u_short)(block));
			size = 4;
			block++;
		}

		/* send ACK to server and wait for server data */
		for (timeouts = 0, error = 0; !intrflag;) {
			if (timeouts == maxtimeout) {
				printtimeout();
				goto abort;
			}

			if (!error) {
				if (trace)
					tpacket("sent", ap, size);
				if (sendto(f, ackbuf, size, 0,
			    	    (struct sockaddr *)&peeraddr,
				    sizeof(peeraddr)) != size) {
					warn("sendto");
					goto abort;
				}
				write_behind(file, convert);
			}
			error = 0;

			pfd[0].fd = f;
			pfd[0].events = POLLIN;
			nfds = poll(pfd, 1, rexmtval * 1000);
			if (nfds == 0) {
				timeouts++;
				continue;
			}
			if (nfds == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				warn("poll");
				goto abort;
			}
			fromlen = sizeof(from);
			n = recvfrom(f, dp, PKTSIZE, 0,
			    (struct sockaddr *)&from, &fromlen);
			if (n == 0) {
				warn("recvfrom");
				goto abort;
			}
			if (n == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				warn("recvfrom");
				goto abort;
			}
			peeraddr.sin_port = from.sin_port;	/* added */
			if (trace)
				tpacket("received", dp, n);
			dp->th_opcode = ntohs(dp->th_opcode);
			dp->th_block = ntohs(dp->th_block);

			if (dp->th_opcode == ERROR) {
				printf("Error code %d: %s\n",
				    dp->th_code, dp->th_msg);
				goto abort;
			}
			if (dp->th_opcode == DATA) {
				int j;
				if (dp->th_block == block)
					break;
				/* re-synchronize with other side */
				j = synchnet(f);
				if (j && trace)
					printf("discarded %d packets\n", j);
				if (dp->th_block == (block - 1))
					continue;
			}
			error = 1;	/* received packet does not match */
		}

		/* write data to file */
		size = writeit(file, &dp, n - 4, convert);
		if (size < 0) {
			nak(errno + 100);
			break;
		}
		amount += size;
	} while (size == SEGSIZE && !intrflag);

abort:
	/* ok to ack, since user has seen err msg */
	ap->th_opcode = htons((u_short)ACK);
	ap->th_block = htons((u_short)block);
	(void) sendto(f, ackbuf, 4, 0, (struct sockaddr *)&peeraddr,
	    sizeof(peeraddr));
	write_behind(file, convert);	/* flush last buffer */

	fclose(file);
	stopclock();
	if (amount > 0) {
		if (intrflag)
			putchar('\n');
		printstats("Received", amount);
	}
}

static int
makerequest(int request, const char *name, struct tftphdr *tp,
    const char *mode)
{
	char *cp;
	int len, pktlen;

	tp->th_opcode = htons((u_short)request);
	cp = tp->th_stuff;
	pktlen = PKTSIZE - offsetof(struct tftphdr, th_stuff);
	len = strlen(name) + 1;
	strlcpy(cp, name, pktlen);
	strlcpy(cp + len, mode, pktlen - len);
	len += strlen(mode) + 1;
	return (cp + len - (char *)tp);
}

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
static void
nak(int error)
{
	struct errmsg *pe;
	struct tftphdr *tp;
	int length;

	tp = (struct tftphdr *)ackbuf;
	tp->th_opcode = htons((u_short)ERROR);
	tp->th_code = htons((u_short)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = strerror(error - 100);
		tp->th_code = EUNDEF;
	}
	length = strlcpy(tp->th_msg, pe->e_msg, sizeof(ackbuf)) + 5;
	if (length > sizeof(ackbuf))
		length = sizeof(ackbuf);
	if (trace)
		tpacket("sent", tp, length);
	if (sendto(f, ackbuf, length, 0, (struct sockaddr *)&peeraddr,
	    sizeof(peeraddr)) != length)
		warn("nak");
}

static void
tpacket(const char *s, struct tftphdr *tp, int n)
{
	static char *opcodes[] =
	    { "#0", "RRQ", "WRQ", "DATA", "ACK", "ERROR" };
	char *cp, *file;
	u_short op = ntohs(tp->th_opcode);

	if (op < RRQ || op > ERROR)
		printf("%s opcode=%x ", s, op);
	else
		printf("%s %s ", s, opcodes[op]);

	switch (op) {
	case RRQ:
	case WRQ:
		n -= 2;
		file = cp = tp->th_stuff;
		cp = strchr(cp, '\0');
		printf("<file=%s, mode=%s>\n", file, cp + 1);
		break;
	case DATA:
		printf("<block=%d, %d bytes>\n", ntohs(tp->th_block), n - 4);
		break;
	case ACK:
		printf("<block=%d>\n", ntohs(tp->th_block));
		break;
	case ERROR:
		printf("<code=%d, msg=%s>\n", ntohs(tp->th_code), tp->th_msg);
		break;
	}
}

static void
startclock(void)
{
	(void) gettimeofday(&tstart, NULL);
}

static void
stopclock(void)
{
	(void) gettimeofday(&tstop, NULL);
}

static void
printstats(const char *direction, unsigned long amount)
{
	double delta;

	/* compute delta in 1/10's second units */
	delta = ((tstop.tv_sec * 10.) + (tstop.tv_usec / 100000)) -
	    ((tstart.tv_sec * 10.) + (tstart.tv_usec / 100000));
	delta = delta / 10.;	/* back to seconds */
	printf("%s %lu bytes in %.1f seconds", direction, amount, delta);
	if (verbose)
		printf(" [%.0f bits/sec]", (amount * 8.) / delta);
	putchar('\n');
}

static void
printtimeout(void)
{
	printf("Transfer timed out.\n");
}
