/*	$OpenBSD: slstats.c,v 1.13 2001/11/17 19:49:41 deraadt Exp $	*/
/*	$NetBSD: slstats.c,v 1.6.6.1 1996/06/07 01:42:30 thorpej Exp $	*/

/*
 * print serial line IP statistics:
 *	slstats [-i interval] [-v] [interface] [system [core]]
 *
 * Copyright (c) 1989, 1990, 1991, 1992 Regents of the University of
 * California. All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Van Jacobson (van@ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: slstats.c,v 1.13 2001/11/17 19:49:41 deraadt Exp $";
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
int	unit;
int	s;
char    interface[IFNAMSIZ];

void	catchalarm __P((void));
void	intpr __P((void));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct ifreq ifr;
	int ch;

	(void)strlcpy(interface, "sl0", sizeof(interface));

	while ((ch = getopt(argc, argv, "i:M:N:v")) != -1) {
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
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		errx(1, "nonexistent interface '%s' specified", interface);

	intpr();
	exit(0);
}

#define V(offset) ((line % 20)? cur.offset - old.offset : cur.offset)

void
usage()
{

	fprintf(stderr, "usage: %s [-i interval] %s",
	    __progname, "[-v] [unit]\n");
	exit(1);
}

volatile sig_atomic_t	signalled; 	/* set if alarm goes off "early" */

static void
get_sl_stats(curp)
	struct sl_stats *curp;
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
intpr()
{
	struct sl_stats cur, old;
	sigset_t mask, oldmask;
	int line = 0;

	bzero(&old, sizeof(old));
	while (1) {
		get_sl_stats(&cur);

		(void)signal(SIGALRM, (void (*)())catchalarm);
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
void
catchalarm()
{
	signalled = 1;
}
