/*	$OpenBSD: trace.c,v 1.2 1996/06/23 14:32:34 deraadt Exp $	*/
/*	$NetBSD: trace.c,v 1.13 1995/06/20 22:28:03 christos Exp $	*/

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
#if 0
static char sccsid[] = "@(#)trace.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$OpenBSD: trace.c,v 1.2 1996/06/23 14:32:34 deraadt Exp $";
#endif
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#define	RIPCMDS
#include "defs.h"
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include "pathnames.h"

#define	NRECORDS	50		/* size of circular trace buffer */
#ifdef DEBUG
FILE	*ftrace = stdout;
int	traceactions = 0;
#endif
static	struct timeval lastlog;
static	char *savetracename;

static int iftraceinit __P((struct interface *, struct ifdebug *));
void dumpif __P((FILE *, struct interface *));
void dumptrace __P((FILE *, char *, struct ifdebug *));

void
traceinit(ifp)
	register struct interface *ifp;
{
	if (iftraceinit(ifp, &ifp->int_input) &&
	    iftraceinit(ifp, &ifp->int_output))
		return;
	tracehistory = 0;
	fprintf(stderr, "traceinit: can't init %s\n", ifp->int_name);
}

static int
iftraceinit(ifp, ifd)
	struct interface *ifp;
	register struct ifdebug *ifd;
{
	register struct iftrace *t;

	ifd->ifd_records =
	  (struct iftrace *)malloc(NRECORDS * sizeof (struct iftrace));
	if (ifd->ifd_records == 0)
		return (0);
	ifd->ifd_front = ifd->ifd_records;
	ifd->ifd_count = 0;
	for (t = ifd->ifd_records; t < ifd->ifd_records + NRECORDS; t++) {
		t->ift_size = 0;
		t->ift_packet = 0;
	}
	ifd->ifd_if = ifp;
	return (1);
}

void
traceon(file)
	char *file;
{
	struct stat stbuf;

	if (ftrace != NULL)
		return;
	if (stat(file, &stbuf) >= 0 && !S_ISREG(stbuf.st_mode))
		return;
	savetracename = file;
	(void) gettimeofday(&now, NULL);
	ftrace = fopen(file, "a");
	if (ftrace == NULL)
		return;
	dup2(fileno(ftrace), 1);
	dup2(fileno(ftrace), 2);
	traceactions = 1;
	fprintf(ftrace, "Tracing enabled %s\n", ctime((time_t *)&now.tv_sec));
}

void
traceoff()
{
	if (!traceactions)
		return;
	if (ftrace != NULL) {
		int fd = open(_PATH_DEVNULL, O_RDWR);

		fprintf(ftrace, "Tracing disabled %s\n",
		    ctime((time_t *)&now.tv_sec));
		fflush(ftrace);
		(void) dup2(fd, 1);
		(void) dup2(fd, 2);
		(void) close(fd);
		fclose(ftrace);
		ftrace = NULL;
	}
	traceactions = 0;
	tracehistory = 0;
	tracepackets = 0;
	tracecontents = 0;
}

void
sigtrace(s)
	int s;
{

	if (s == SIGUSR2)
		traceoff();
	else if (ftrace == NULL && savetracename)
		traceon(savetracename);
	else
		bumploglevel();
}

/*
 * Move to next higher level of tracing when -t option processed or
 * SIGUSR1 is received.  Successive levels are:
 *	traceactions
 *	traceactions + tracepackets
 *	traceactions + tracehistory (packets and contents after change)
 *	traceactions + tracepackets + tracecontents
 */
void
bumploglevel()
{

	(void) gettimeofday(&now, NULL);
	if (traceactions == 0) {
		traceactions++;
		if (ftrace)
			fprintf(ftrace, "Tracing actions started %s\n",
			    ctime((time_t *)&now.tv_sec));
	} else if (tracepackets == 0) {
		tracepackets++;
		tracehistory = 0;
		tracecontents = 0;
		if (ftrace)
			fprintf(ftrace, "Tracing packets started %s\n",
			    ctime((time_t *)&now.tv_sec));
	} else if (tracehistory == 0) {
		tracehistory++;
		if (ftrace)
			fprintf(ftrace, "Tracing history started %s\n",
			    ctime((time_t *)&now.tv_sec));
	} else {
		tracepackets++;
		tracecontents++;
		tracehistory = 0;
		if (ftrace)
			fprintf(ftrace, "Tracing packet contents started %s\n",
			    ctime((time_t *)&now.tv_sec));
	}
	if (ftrace)
		fflush(ftrace);
}

void
trace(ifd, who, p, len, m)
	register struct ifdebug *ifd;
	struct sockaddr *who;
	char *p;
	int len, m;
{
	register struct iftrace *t;

	if (ifd->ifd_records == 0)
		return;
	t = ifd->ifd_front++;
	if (ifd->ifd_front >= ifd->ifd_records + NRECORDS)
		ifd->ifd_front = ifd->ifd_records;
	if (ifd->ifd_count < NRECORDS)
		ifd->ifd_count++;
	if (t->ift_size > 0 && t->ift_size < len && t->ift_packet) {
		free(t->ift_packet);
		t->ift_packet = 0;
	}
	t->ift_stamp = now;
	t->ift_who = *who;
	if (len > 0 && t->ift_packet == 0) {
		t->ift_packet = malloc(len);
		if (t->ift_packet == 0)
			len = 0;
	}
	if (len > 0)
		memcpy(t->ift_packet, p, len);
	t->ift_size = len;
	t->ift_metric = m;
}

void
traceaction(fd, action, rt)
	FILE *fd;
	char *action;
	struct rt_entry *rt;
{
	struct sockaddr_in *dst, *gate, *netmask;
	static struct bits {
		int	t_bits;
		char	*t_name;
	} flagbits[] = {
		{ RTF_UP,	"UP" },
		{ RTF_GATEWAY,	"GATEWAY" },
		{ RTF_HOST,	"HOST" },
		{ 0 }
	}, statebits[] = {
		{ RTS_PASSIVE,	"PASSIVE" },
		{ RTS_REMOTE,	"REMOTE" },
		{ RTS_INTERFACE,"INTERFACE" },
		{ RTS_CHANGED,	"CHANGED" },
		{ RTS_INTERNAL,	"INTERNAL" },
		{ RTS_EXTERNAL,	"EXTERNAL" },
		{ RTS_SUBNET,	"SUBNET" },
		{ 0 }
	};
	register struct bits *p;
	register int first;
	char *cp;

	if (fd == NULL)
		return;
	if (lastlog.tv_sec != now.tv_sec || lastlog.tv_usec != now.tv_usec) {
		fprintf(fd, "\n%.19s:\n", ctime((time_t *)&now.tv_sec));
		lastlog = now;
	}
	fprintf(fd, "%s ", action);
	dst = (struct sockaddr_in *)&rt->rt_dst;
	gate = (struct sockaddr_in *)&rt->rt_router;
	netmask = (struct sockaddr_in *)&rt->rt_netmask;
	fprintf(fd, "dst %s, ", inet_ntoa(dst->sin_addr));
	fprintf(fd, "router %s, ", inet_ntoa(gate->sin_addr));
	fprintf(fd, " netmask %s, metric %d, flags",
	        inet_ntoa(netmask->sin_addr), rt->rt_metric);
	cp = " %s";
	for (first = 1, p = flagbits; p->t_bits > 0; p++) {
		if ((rt->rt_flags & p->t_bits) == 0)
			continue;
		fprintf(fd, cp, p->t_name);
		if (first) {
			cp = "|%s";
			first = 0;
		}
	}
	fprintf(fd, " state");
	cp = " %s";
	for (first = 1, p = statebits; p->t_bits > 0; p++) {
		if ((rt->rt_state & p->t_bits) == 0)
			continue;
		fprintf(fd, cp, p->t_name);
		if (first) {
			cp = "|%s";
			first = 0;
		}
	}
	fprintf(fd, " timer %d\n", rt->rt_timer);
	if (tracehistory && !tracepackets &&
	    (rt->rt_state & RTS_PASSIVE) == 0 && rt->rt_ifp)
		dumpif(fd, rt->rt_ifp);
	fflush(fd);
	if (ferror(fd))
		traceoff();
}

void
tracenewmetric(fd, rt, newmetric)
	FILE *fd;
	struct rt_entry *rt;
	int newmetric;
{
	struct sockaddr_in *dst, *gate;

	if (fd == NULL)
		return;
	if (lastlog.tv_sec != now.tv_sec || lastlog.tv_usec != now.tv_usec) {
		fprintf(fd, "\n%.19s:\n", ctime((time_t *)&now.tv_sec));
		lastlog = now;
	}
	dst = (struct sockaddr_in *)&rt->rt_dst;
	gate = (struct sockaddr_in *)&rt->rt_router;
	fprintf(fd, "CHANGE metric dst %s, ", inet_ntoa(dst->sin_addr));
	fprintf(fd, "router %s, from %d to %d\n",
	     inet_ntoa(gate->sin_addr), rt->rt_metric, newmetric);
	fflush(fd);
	if (ferror(fd))
		traceoff();
}

void
dumpif(fd, ifp)
	FILE *fd;
	register struct interface *ifp;
{
	if (ifp->int_input.ifd_count || ifp->int_output.ifd_count) {
		fprintf(fd, "*** Packet history for interface %s ***\n",
			ifp->int_name);
#ifdef notneeded
		dumptrace(fd, "to", &ifp->int_output);
#endif
		dumptrace(fd, "from", &ifp->int_input);
		fprintf(fd, "*** end packet history ***\n");
	}
}

void
dumptrace(fd, dir, ifd)
	FILE *fd;
	char *dir;
	register struct ifdebug *ifd;
{
	register struct iftrace *t;
	char *cp = !strcmp(dir, "to") ? "Output" : "Input";

	if (ifd->ifd_front == ifd->ifd_records &&
	    ifd->ifd_front->ift_size == 0) {
		fprintf(fd, "%s: no packets.\n", cp);
		fflush(fd);
		return;
	}
	fprintf(fd, "%s trace:\n", cp);
	t = ifd->ifd_front - ifd->ifd_count;
	if (t < ifd->ifd_records)
		t += NRECORDS;
	for ( ; ifd->ifd_count; ifd->ifd_count--, t++) {
		if (t >= ifd->ifd_records + NRECORDS)
			t = ifd->ifd_records;
		if (t->ift_size == 0)
			continue;
		dumppacket(fd, dir, (struct sockaddr_in *)&t->ift_who,
		    t->ift_packet, t->ift_size, &t->ift_stamp);
	}
}

void
dumppacket(fd, dir, who, cp, size, stamp)
	FILE *fd;
	struct sockaddr_in *who;		/* should be sockaddr */
	char *dir, *cp;
	register int size;
	struct timeval *stamp;
{
	register struct rip *msg = (struct rip *)cp;
	register struct netinfo *n;

	if (fd == NULL)
		return;
	if (msg->rip_cmd && msg->rip_cmd < RIPCMD_MAX)
		fprintf(fd, "%s %s %s.%d %.19s:\n", ripcmds[msg->rip_cmd],
		    dir, inet_ntoa(who->sin_addr), ntohs(who->sin_port),
		    ctime((time_t *)&stamp->tv_sec));
	else {
		fprintf(fd, "Bad cmd 0x%x %s %s.%d %.19s\n", msg->rip_cmd,
		    dir, inet_ntoa(who->sin_addr), ntohs(who->sin_port),
		    ctime((time_t *)&stamp->tv_sec));
		fprintf(fd, "size=%d cp=%lx packet=%lx\n", size,
			(u_long) cp, (u_long) packet);
		fflush(fd);
		return;
	}
	if (tracepackets && tracecontents == 0) {
		fflush(fd);
		return;
	}
	switch (msg->rip_cmd) {

	case RIPCMD_REQUEST:
	case RIPCMD_RESPONSE:
		size -= 4 * sizeof (char);
		n = msg->rip_nets;
		for (; size > 0; n++, size -= sizeof (struct netinfo)) {
			if (size < sizeof (struct netinfo)) {
				fprintf(fd, "(truncated record, len %d)\n",
				    size);
				break;
			}
			switch (n->rip_family) {

			case AF_INET:
				{
					struct sockaddr_in sa;
					sa.sin_addr.s_addr = n->rip_dst;
					fprintf(fd, "\tdst %s",
						inet_ntoa(sa.sin_addr));
					if (msg->rip_vers > RIP_VERSION_1) {
						fprintf(fd, ", mask 0x%.8x",
							n->rip_netmask);
						sa.sin_addr.s_addr =
							n->rip_router;
						fprintf(fd, ", router %s",
							inet_ntoa(sa.sin_addr));
					}
				}
				break;

			default:
				fprintf(fd, "\taf %d?", n->rip_family);
				break;
			}
			fprintf(fd, ", metric %d\n", ntohl(n->rip_metric));
		}
		break;

	case RIPCMD_TRACEON:
		fprintf(fd, "\tfile=%*s\n", size, msg->rip_tracefile);
		break;

	case RIPCMD_TRACEOFF:
		break;
	}
	fflush(fd);
	if (ferror(fd))
		traceoff();
}
