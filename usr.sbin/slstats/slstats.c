/*	$OpenBSD: slstats.c,v 1.19 2004/09/21 21:17:02 jaredy Exp $	*/
/*	$NetBSD: slstats.c,v 1.6.6.1 1996/06/07 01:42:30 thorpej Exp $	*/

/*
 * print serial line IP statistics:
 *	slstats [-i interval] [-v] [interface]
 *
 * Contributed by Van Jacobson (van@ee.lbl.gov), Dec 31, 1989.
 *
 * Copyright (c) 1989, 1990, 1991, 1992 Regents of the University of
 * California. All rights reserved.
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
static char rcsid[] = "$OpenBSD: slstats.c,v 1.19 2004/09/21 21:17:02 jaredy Exp $";
#endif

#define INET

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/slcompress.h>
#include <net/if_slvar.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <paths.h>
#include <nlist.h>
#include <string.h>
#include <unistd.h>

extern	char *__progname;	/* from crt0.o */

int	vflag;
u_int	interval = 5;
int	s;
char    interface[IFNAMSIZ];

void	catchalarm(int);
void	intpr(void);
void	usage(void);

int
main(int argc, char *argv[])
{
	struct ifreq ifr;
	int ch, unit;

	(void)strlcpy(interface, "sl0", sizeof(interface));

	while ((ch = getopt(argc, argv, "i:v")) != -1) {
		switch (ch) {
		case 'i':
			interval = atoi(optarg);
			if (interval <= 0)
				usage();
			break;
		case 'v':
			++vflag;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	if (argc > 0)
		(void)strlcpy(interface, argv[0], sizeof(interface));

	if (sscanf(interface, "sl%d", &unit) != 1 || unit < 0)
		errx(1, "invalid interface '%s' specified", interface);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "couldn't create IP socket");
	(void)strlcpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
		errx(1, "nonexistent interface '%s' specified", interface);

	intpr();
	exit(0);
}

#define V(offset) ((line % 20)? cur.offset - old.offset : cur.offset)

void
usage(void)
{

	fprintf(stderr, "usage: %s [-v] [-i interval] [interface]\n",
	    __progname);
	exit(1);
}

volatile sig_atomic_t	signalled; 	/* set if alarm goes off "early" */

static void
get_sl_stats(struct sl_stats *curp)
{
	struct ifslstatsreq req;

	memset(&req, 0, sizeof(req));
	(void)strlcpy(req.ifr_name, interface, sizeof(req.ifr_name));

	if (ioctl(s, SIOCGSLSTATS, &req) < 0) {
		if (errno == ENOTTY)
			errx(1, "kernel support missing");
		else
			err(1, "couldn't get slip statistics");
	}
	*curp = req.stats;
}

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
intpr(void)
{
	struct sl_stats cur, old;
	sigset_t mask, oldmask;
	int line = 0;

	bzero(&old, sizeof(old));
	while (1) {
		get_sl_stats(&cur);

		(void)signal(SIGALRM, (void (*)(int))catchalarm);
		signalled = 0;
		(void)alarm(interval);

		if ((line % 20) == 0) {
			printf("%8.8s %6.6s %6.6s %6.6s %6.6s",
			    "IN", "PACK", "COMP", "UNCOMP", "ERR");
			if (vflag)
				printf(" %6.6s %6.6s", "TOSS", "IP");
			printf(" | %8.8s %6.6s %6.6s %6.6s %6.6s",
			    "OUT", "PACK", "COMP", "UNCOMP", "IP");
			if (vflag)
				printf(" %6.6s %6.6s", "SEARCH", "MISS");
			putchar('\n');
		}
		printf("%8u %6d %6u %6u %6u", V(sl.sl_ibytes),
		    V(sl.sl_ipackets), V(vj.vjs_compressedin),
		    V(vj.vjs_uncompressedin), V(vj.vjs_errorin));
		if (vflag)
			printf(" %6u %6u", V(vj.vjs_tossed),
			    V(sl.sl_ipackets) - V(vj.vjs_compressedin) -
			    V(vj.vjs_uncompressedin) - V(vj.vjs_errorin));
		printf(" | %8u %6d %6u %6u %6u", V(sl.sl_obytes),
		    V(sl.sl_opackets), V(vj.vjs_compressed),
		    V(vj.vjs_packets) - V(vj.vjs_compressed),
		    V(sl.sl_opackets) - V(vj.vjs_packets));
		if (vflag)
			printf(" %6u %6u", V(vj.vjs_searches),
			    V(vj.vjs_misses));

		putchar('\n');
		fflush(stdout);
		line++;

		sigemptyset(&mask);
		sigaddset(&mask, SIGALRM);
		sigprocmask(SIG_BLOCK, &mask, &oldmask);
		if (!signalled) {
			sigemptyset(&mask);
			sigsuspend(&mask);
		}
		sigprocmask(SIG_BLOCK, &oldmask, NULL);
		signalled = 0;
		(void)alarm(interval);
		old = cur;
	}
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
/* ARGSUSED */
void
catchalarm(int signo)
{
	signalled = 1;
}
