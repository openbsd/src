/*	$OpenBSD: pppstats.c,v 1.5 1998/05/08 04:52:37 millert Exp $	*/

/*
 * print PPP statistics:
 * 	pppstats [-a|-d] [-v|-r|-z] [-c count] [-w wait] [interface]
 *
 *   -a Show absolute values rather than deltas
 *   -d Show data rate (KB/s) rather than bytes
 *   -v Show more stats for VJ TCP header compression
 *   -r Show compression ratio
 *   -z Show compression statistics instead of default display
 *
 * History:
 *      perkins@cps.msu.edu: Added compression statistics and alternate
 *                display. 11/94
 *      Brad Parker (brad@cayman.com) 6/92
 *
 * from the original "slstats" by Van Jacobson
 *
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
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
 */

#ifndef lint
#if 0
static char rcsid[] = "Id: pppstats.c,v 1.22 1998/03/31 23:48:03 paulus Exp $";
#else
static char rcsid[] = "$OpenBSD: pppstats.c,v 1.5 1998/05/08 04:52:37 millert Exp $";
#endif
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/ppp_defs.h>
#include <net/if.h>
#include <net/if_ppp.h>

int	vflag, rflag, zflag;	/* select type of display */
int	aflag;			/* print absolute values, not deltas */
int	dflag;			/* print data rates, not bytes */
int	interval, count;
int	infinite;
int	unit;
int	s;			/* socket file descriptor */
int	signalled;		/* set if alarm goes off "early" */
char	interface[IFNAMSIZ];

static void usage __P((void));
static void catchalarm __P((int));
static void get_ppp_stats __P((struct ppp_stats *));
static void get_ppp_cstats __P((struct ppp_comp_stats *));
static void intpr __P((void));

int main __P((int, char *argv[]));

static void
usage()
{
	extern char *__progname;

	fprintf(stderr,
	    "Usage: %s [-a|-d] [-v|-r|-z] [-c count] [-w wait] [interface]\n",
	    __progname);
	exit(1);
}

/*
 * Called if an interval expires before intpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
static void
catchalarm(arg)
	int arg;
{

	signalled = 1;
}

static void
get_ppp_stats(curp)
	struct ppp_stats *curp;
{
	struct ifpppstatsreq req;

	memset(&req, 0, sizeof(req));
	(void)strncpy(req.ifr_name, interface, sizeof(req.ifr_name) - 1);
	req.ifr_name[sizeof(req.ifr_name) - 1] = '\0';

	if (ioctl(s, SIOCGPPPSTATS, &req) < 0) {
		if (errno == ENOTTY)
			errx(1, "kernel support missing");
		else
			err(1, "couldn't get PPP statistics");
	}
	*curp = req.stats;
}

static void
get_ppp_cstats(csp)
	struct ppp_comp_stats *csp;
{
	struct ifpppcstatsreq creq;

	memset(&creq, 0, sizeof(creq));
	(void)strncpy(creq.ifr_name, interface, sizeof(creq.ifr_name) - 1);
	creq.ifr_name[sizeof(creq.ifr_name) - 1] = '\0';

	if (ioctl(s, SIOCGPPPCSTATS, &creq) < 0) {
		if (errno == ENOTTY) {
			warnx("no kernel compression support");
			if (zflag)
				exit(1);
			rflag = 0;
		} else
			err(1, "couldn't get PPP compression stats");
	}
	*csp = creq.stats;
}

#define MAX0(a)		((int)(a) > 0? (a): 0)
#define V(offset)	MAX0(cur.offset - old.offset)
#define W(offset)	MAX0(ccs.offset - ocs.offset)

#define RATIO(c, i, u)	((c) == 0? 1.0: (u) / ((double)(c) + (i)))
#define CRATE(x)	RATIO(W(x.comp_bytes), W(x.inc_bytes), W(x.unc_bytes))

#define KBPS(n)		((n) / (interval * 1000.0))

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed is cumulative.
 */
static void
intpr()
{
	register int line = 0;
	sigset_t oldmask, mask;
	char *bunit;
	int ratef = 0;
	struct ppp_stats cur, old;
	struct ppp_comp_stats ccs, ocs;

	memset(&old, 0, sizeof(old));
	memset(&ocs, 0, sizeof(ocs));

	for (;;) {
		get_ppp_stats(&cur);
		if (zflag || rflag)
			get_ppp_cstats(&ccs);

		(void)signal(SIGALRM, catchalarm);
		signalled = 0;
		(void)alarm(interval);

		if ((line % 20) == 0) {
			if (zflag) {
				printf("IN:  COMPRESSED  INCOMPRESSIBLE   COMP | ");
				printf("OUT: COMPRESSED  INCOMPRESSIBLE   COMP\n");
				bunit = dflag? "KB/S": "BYTE";
				printf("    %s   PACK     %s   PACK  RATIO | ",
				    bunit, bunit); 
				printf("    %s   PACK     %s   PACK  RATIO",
				    bunit, bunit);    
			} else {
				printf("%8.8s %6.6s %6.6s",
					"IN", "PACK", "VJCOMP");

				if (!rflag)
					printf(" %6.6s %6.6s", "VJUNC", "VJERR");
				if (vflag)
					printf(" %6.6s %6.6s", "VJTOSS", "NON-VJ");
				if (rflag)
					printf(" %6.6s %6.6s", "RATIO", "UBYTE");
				printf("  | %8.8s %6.6s %6.6s",
					"OUT", "PACK", "VJCOMP");

				if (!rflag)
					printf(" %6.6s %6.6s", "VJUNC", "NON-VJ");
				if (vflag)
					printf(" %6.6s %6.6s", "VJSRCH", "VJMISS");
				if (rflag)
					printf(" %6.6s %6.6s", "RATIO", "UBYTE");
			}
			putchar('\n');
		}

		if (zflag) {
			if (ratef) {
				printf("%8.3f %6u %8.3f %6u %6.2f",
				    KBPS(W(d.comp_bytes)), W(d.comp_packets),
				    KBPS(W(d.inc_bytes)), W(d.inc_packets),
				    ccs.d.ratio * 256.0);

				printf(" | %8.3f %6u %8.3f %6u %6.2f",
				    KBPS(W(c.comp_bytes)), W(c.comp_packets),
				    KBPS(W(c.inc_bytes)), W(c.inc_packets),
				    ccs.c.ratio * 256.0);
			} else {
				printf("%8u %6u %8u %6u %6.2f",
				   W(d.comp_bytes), W(d.comp_packets),
				   W(d.inc_bytes), W(d.inc_packets),
				   ccs.d.ratio * 256.0);

				printf(" | %8u %6u %8u %6u %6.2f",
				   W(c.comp_bytes), W(c.comp_packets),
				   W(c.inc_bytes), W(c.inc_packets),
				   ccs.c.ratio * 256.0);
			}
		} else {
			if (ratef)
				printf("%8.3f", KBPS(V(p.ppp_ibytes)));
			else
				printf("%8u", V(p.ppp_ibytes));
			printf(" %6u %6u", V(p.ppp_ipackets),
			    V(vj.vjs_compressedin));
			if (!rflag)
				printf(" %6u %6u", V(vj.vjs_uncompressedin),
				   V(vj.vjs_errorin));
			if (vflag)
				printf(" %6u %6u", V(vj.vjs_tossed),
				   V(p.ppp_ipackets) -
				   V(vj.vjs_compressedin) -
				   V(vj.vjs_uncompressedin) -
				   V(vj.vjs_errorin));
			if (rflag) {
				printf(" %6.2f ", CRATE(d));
				if (ratef)
					printf("%6.2f", KBPS(W(d.unc_bytes)));
				else
					printf("%6u", W(d.unc_bytes));
			}
			if (ratef)
				printf("  | %8.3f", KBPS(V(p.ppp_obytes)));
			else
				printf("  | %8u", V(p.ppp_obytes));

			printf(" %6u %6u", V(p.ppp_opackets),
			    V(vj.vjs_compressed));
			if (!rflag)
				printf(" %6u %6u",
				   V(vj.vjs_packets) - V(vj.vjs_compressed),
				   V(p.ppp_opackets) - V(vj.vjs_packets));
			if (vflag)
				printf(" %6u %6u", V(vj.vjs_searches),
				   V(vj.vjs_misses));
			if (rflag) {
				printf(" %6.2f ", CRATE(c));
				if (ratef)
					printf("%6.2f", KBPS(W(c.unc_bytes)));
				else
					printf("%6u", W(c.unc_bytes));
			}
		}

		putchar('\n');
		fflush(stdout);
		line++;

		count--;
		if (!infinite && !count)
			break;

		sigemptyset(&mask);
		sigaddset(&mask, SIGALRM);
		sigprocmask(SIG_BLOCK, &mask, &oldmask);
		if (signalled == 0) {
			sigemptyset(&mask);
			sigsuspend(&mask);
		}
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		signalled = 0;
		(void)alarm(interval);
		if (!aflag) {
			old = cur;
			ocs = ccs;
			ratef = dflag;
		}
	}
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	struct ifreq ifr;

	(void)strcpy(interface, "ppp0");

	while ((c = getopt(argc, argv, "advrzc:w:")) != -1) {
		switch (c) {
		case 'a':
			++aflag;
			break;
		case 'd':
			++dflag;
			break;
		case 'v':
			++vflag;
			break;
		case 'r':
			++rflag;
			break;
		case 'z':
			++zflag;
			break;
		case 'c':
			count = atoi(optarg);
			if (count <= 0)
				usage();
			break;
		case 'w':
			interval = atoi(optarg);
			if (interval <= 0)
				usage();
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!interval && count)
		interval = 5;
	if (interval && !count)
		infinite = 1;
	if (!interval && !count)
		count = 1;
	if (aflag)
		dflag = 0;

	if (argc > 1)
		usage();
	if (argc > 0) {
		(void)strncpy(interface, argv[0], sizeof(interface) - 1);
		interface[sizeof(interface) - 1] = '\0';
	}
	if (sscanf(interface, "ppp%d", &unit) != 1)
		errx(1, "invalid interface '%s' specified", interface);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "couldn't create IP socket");
	(void)strcpy(ifr.ifr_name, interface);
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		errx(1, "nonexistent interface '%s' specified", interface);

	intpr();
	exit(0);
}
