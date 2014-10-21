/*	$OpenBSD: tftp.c,v 1.24 2014/10/21 06:15:16 dlg Exp $	*/
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

/*
 * TFTP User Program -- Protocol Machines
 *
 * This version includes many modifications by Jim Guyton <guyton@rand-unix>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/tftp.h>

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "extern.h"
#include "tftpsubs.h"

static int	cmpport(struct sockaddr *, struct sockaddr *);
static int	makerequest(int, const char *, struct tftphdr *, const char *);
static void	nak(int, struct sockaddr *);
static void 	tpacket(const char *, struct tftphdr *, int);
static void	startclock(void);
static void	stopclock(void);
static void	printstats(const char *, unsigned long);
static void	printtimeout(void);
static void	oack(struct tftphdr *, int, int);
static int	oack_set(const char *, const char *);

extern struct sockaddr_storage	 peeraddr;	/* filled in by main */
extern int			 f;		/* the opened socket */
extern int			 trace;
extern int			 verbose;
extern int			 rexmtval;
extern int			 maxtimeout;
extern FILE			*file;
extern volatile sig_atomic_t	 intrflag;
extern char			*ackbuf;
extern int			 has_options;
extern int			 opt_tsize;
extern int			 opt_tout;
extern int			 opt_blksize;

struct timeval	tstart;
struct timeval	tstop;
unsigned int	segment_size = SEGSIZE;
unsigned int	packet_size = SEGSIZE + 4;

struct errmsg {
	int	 e_code;
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
	{ EOPTNEG,	"Option negotiation failed" },
	{ -1,		NULL }
};

struct options {
	const char      *o_type;
} options[] = {
	{ "tsize" },
	{ "timeout" },
	{ "blksize" },
	{ NULL }
};

enum opt_enum {
	OPT_TSIZE = 0,
	OPT_TIMEOUT,
	OPT_BLKSIZE
};

/*
 * Send the requested file.
 */
void
sendfile(int fd, char *name, char *mode)
{
	struct tftphdr		*dp, *ap; /* data and ack packets */
	struct sockaddr_storage	 from, peer;
	struct sockaddr_storage	 serv; /* valid server port number */
	struct pollfd		 pfd[1];
	unsigned long		 amount;
	socklen_t		 fromlen;
	int			 convert; /* true if converting crlf -> lf */
	int			 n, nfds, error, timeouts, size;
	uint16_t		 block = 0;
	int			 firsttrip = 1;

	startclock();		/* start stat's clock */
	dp = r_init();		/* reset fillbuf/read-ahead code */
	ap = (struct tftphdr *)ackbuf;
	file = fdopen(fd, "r");
	convert = !strcmp(mode, "netascii");
	amount = 0;
	memcpy(&peer, &peeraddr, peeraddr.ss_len);
	memset(&serv, 0, sizeof(serv));

	do {
		/* read data from file */
		if (firsttrip)
			size = makerequest(WRQ, name, dp, mode) - 4;
		else {
			size = readit(file, &dp, convert, segment_size);
			if (size < 0) {
				nak(errno + 100, (struct sockaddr *)&peer);
				break;
			}
			dp->th_opcode = htons((u_short)DATA);
			dp->th_block = htons(block);
		}

		/* send data to server and wait for server ACK */
		for (timeouts = 0, error = 0; !intrflag;) {
			if (timeouts >= maxtimeout) {
				printtimeout();
				goto abort;
			}

			if (!error) {
				if (trace)
					tpacket("sent", dp, size + 4);
				if (sendto(f, dp, size + 4, 0,
				    (struct sockaddr *)&peer,
				    peer.ss_len) != size + 4) {
					warn("sendto");
					goto abort;
				}
				if (!firsttrip)
					read_ahead(file, convert, segment_size);
			}
			error = 0;

			pfd[0].fd = f;
			pfd[0].events = POLLIN;
			nfds = poll(pfd, 1, rexmtval * 1000);
			if (nfds == 0) {
				timeouts += rexmtval;
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
			n = recvfrom(f, ackbuf, packet_size, 0,
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
			if (!serv.ss_family)
				serv = from;
			else if (!cmpport((struct sockaddr *)&serv,
			    (struct sockaddr *)&from)) {
				warn("server port mismatch");
				goto abort;
			}
			peer = from;
			if (trace)
				tpacket("received", ap, n);

			ap->th_opcode = ntohs(ap->th_opcode);

			if (ap->th_opcode == OACK) {
				oack(ap, n, 0);
				break;
			}

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

		if (firsttrip) {
			size = segment_size;
			firsttrip = 0;
		} else
			amount += size;
		block++;
	} while ((size == segment_size) && !intrflag);

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
	struct tftphdr		*dp, *ap; /* data and ack packets */
	struct sockaddr_storage	 from, peer;
	struct sockaddr_storage	 serv; /* valid server port number */
	struct pollfd		 pfd[1];
	unsigned long		 amount;
	socklen_t		 fromlen;
	int			 convert; /* true if converting crlf -> lf */
	int			 n, nfds, error, timeouts, size;
	int			 firsttrip;
	uint16_t		 block;

	startclock();		/* start stat's clock */
	dp = w_init();		/* reset fillbuf/read-ahead code */
	ap = (struct tftphdr *)ackbuf;
	file = fdopen(fd, "w");
	convert = !strcmp(mode, "netascii");
	n = 0;
	block = 1;
	amount = 0;
	firsttrip = 1;
	memcpy(&peer, &peeraddr, peeraddr.ss_len);
	memset(&serv, 0, sizeof(serv));

options:
	do {
		/* create new ACK packet */
		if (firsttrip) {
			size = makerequest(RRQ, name, ap, mode);
			firsttrip = 0;
		} else {
			ap->th_opcode = htons((u_short)ACK);
			ap->th_block = htons(block);
			size = 4;
			block++;
		}

		/* send ACK to server and wait for server data */
		for (timeouts = 0, error = 0; !intrflag;) {
			if (timeouts >= maxtimeout) {
				printtimeout();
				goto abort;
			}

			if (!error) {
				if (trace)
					tpacket("sent", ap, size);
				if (sendto(f, ackbuf, size, 0,
				    (struct sockaddr *)&peer,
				    peer.ss_len) != size) {
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
				timeouts += rexmtval;
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
			n = recvfrom(f, dp, packet_size, 0,
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
			if (!serv.ss_family)
				serv = from;
			else if (!cmpport((struct sockaddr *)&serv,
			    (struct sockaddr *)&from)) {
				warn("server port mismatch");
				goto abort;
			}
			peer = from;
			if (trace)
				tpacket("received", dp, n);

			dp->th_opcode = ntohs(dp->th_opcode);

			if (dp->th_opcode == OACK) {
				oack(dp, n, 0);
				block = 0;
				goto options;
			}

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
			nak(errno + 100, (struct sockaddr *)&peer);
			break;
		}
		amount += size;
	} while (size == segment_size && !intrflag);

abort:
	/* ok to ack, since user has seen err msg */
	ap->th_opcode = htons((u_short)ACK);
	ap->th_block = htons(block);
	(void)sendto(f, ackbuf, 4, 0, (struct sockaddr *)&peer,
	    peer.ss_len);
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
cmpport(struct sockaddr *sa, struct sockaddr *sb)
{
	char a[NI_MAXSERV], b[NI_MAXSERV];
	if (getnameinfo(sa, sa->sa_len, NULL, 0, a, sizeof(a), NI_NUMERICSERV))
		return (0);
	if (getnameinfo(sb, sb->sa_len, NULL, 0, b, sizeof(b), NI_NUMERICSERV))
		return (0);
	if (strcmp(a, b) != 0)
		return (0);

	return (1);
}

static int
makerequest(int request, const char *name, struct tftphdr *tp,
    const char *mode)
{
	char		*cp;
	int		 len, pktlen;
	off_t		 fsize = 0;
	struct stat	 st;

	tp->th_opcode = htons((u_short)request);
	cp = tp->th_stuff;
	pktlen = packet_size - offsetof(struct tftphdr, th_stuff);
	len = strlen(name) + 1;
	strlcpy(cp, name, pktlen);
	strlcpy(cp + len, mode, pktlen - len);
	len += strlen(mode) + 1;

	if (opt_tsize) {
		if (request == WRQ) {
			stat(name, &st);
			fsize = st.st_size;
		}
		len += snprintf(cp + len, pktlen - len, "%s%c%lld%c",
		    options[OPT_TSIZE].o_type, 0, fsize, 0);
	}
	if (opt_tout)
		len += snprintf(cp + len, pktlen - len, "%s%c%d%c",
		    options[OPT_TIMEOUT].o_type, 0, rexmtval, 0);
	if (opt_blksize)
		len += snprintf(cp + len, pktlen - len, "%s%c%d%c",
		    options[OPT_BLKSIZE].o_type, 0, opt_blksize, 0);

	return (cp + len - (char *)tp);
}

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
static void
nak(int error, struct sockaddr *peer)
{
	struct errmsg	*pe;
	struct tftphdr	*tp;
	int		 length;

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
	length = strlcpy(tp->th_msg, pe->e_msg, packet_size) + 5;
	if (length > packet_size)
		length = packet_size;
	if (trace)
		tpacket("sent", tp, length);
	if (sendto(f, ackbuf, length, 0, peer,
	    peer->sa_len) != length)
		warn("nak");
}

static void
tpacket(const char *s, struct tftphdr *tp, int n)
{
	char		*cp, *file;
	static char	*opcodes[] =
	    { "#0", "RRQ", "WRQ", "DATA", "ACK", "ERROR", "OACK" };

	u_short op = ntohs(tp->th_opcode);

	if (op < RRQ || op > OACK)
		printf("%s opcode=%x ", s, op);
	else
		printf("%s %s ", s, opcodes[op]);

	switch (op) {
	case RRQ:
	case WRQ:
		n -= 2;
		file = cp = tp->th_stuff;
		cp = strchr(cp, '\0');
		printf("<file=%s, mode=%s", file, cp + 1);
		if (has_options)
			oack(tp, n, 1);
		printf(">\n");
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
	case OACK:
		printf("<");
		oack(tp, n, 1);
		printf(">\n");
		break;
	}
}

static void
startclock(void)
{
	(void)gettimeofday(&tstart, NULL);
}

static void
stopclock(void)
{
	(void)gettimeofday(&tstop, NULL);
}

static void
printstats(const char *direction, unsigned long amount)
{
	double	delta;

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

static void
oack(struct tftphdr *tp, int size, int trace)
{
	int	 i, len, off;
	char	*opt, *val;

	u_short op = ntohs(tp->th_opcode);

	opt = tp->th_u.tu_stuff;
	val = tp->th_u.tu_stuff;

	if (op == RRQ || op == WRQ) {
		len = strlen(opt) + 1;
		opt = strchr(opt, '\0');
		opt++;
		len += strlen(opt) + 1;
		opt = strchr(opt, '\0');
		opt++;
		val = opt;
		off = len;
		if (trace)
			printf(", ");
	} else
		off = 2;

	for (i = off, len = 0; i < size - 1; i++) {
		if (*val != '\0') {
			val++;
			continue;
		}
		/* got option and value */
		val++;
		if (trace)
			printf("%s=%s", opt, val);
		else
			if (oack_set(opt, val) == -1)
				break;
		len = strlen(val) + 1;
		val += len;
		opt = val;
		i += len;
		if (trace && i < size - 1)
			printf(", ");
	}
}

int
oack_set(const char *option, const char *value)
{
	int		 i, n;
	const char	*errstr;
	struct sockaddr_storage peer;
	memcpy(&peer, &peeraddr, peeraddr.ss_len);

	for (i = 0; options[i].o_type != NULL; i++) {
		if (!strcasecmp(options[i].o_type, option)) {
			if (i == OPT_TSIZE) {
				/* XXX verify OACK response */
			}
			if (i == OPT_TIMEOUT) {
				/* verify OACK response */
				n = strtonum(value, TIMEOUT_MIN, TIMEOUT_MAX,
				    &errstr);
				if (errstr || rexmtval != n ||
				    opt_tout == 0) {
					nak(EOPTNEG, (struct sockaddr *)&peer);
					intrflag = 1;
					return (-1);
				}
				/* OK */
			}
			if (i == OPT_BLKSIZE) {
				/* verify OACK response */
				n = strtonum(value, SEGSIZE_MIN, SEGSIZE_MAX,
				    &errstr);
				if (errstr || opt_blksize != n ||
				    opt_blksize == 0) {
					nak(EOPTNEG, (struct sockaddr *)&peer);
					intrflag = 1;
					return (-1);
				}
				/* OK, set option */
				segment_size = n;
				packet_size = segment_size + 4;
			}
		}
	}

	return (1);
}
