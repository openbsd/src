/*
 * print PPP statistics:
 * 	pppstats [-v] [-r] [-z] [-c count] [-w wait] [interface]
 *
 *   -v Verbose mode for default display
 *   -r Show compression ratio in default display
 *   -z Show compression statistics instead of default display
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
 *
 *	Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */

#ifndef lint
static char rcsid[] = "$Id: pppstats.c,v 1.1.1.1 1995/10/18 08:48:01 deraadt Exp $";
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/ppp_defs.h>
#include <net/if.h>
#include <net/if_ppp.h>

int	vflag, rflag, zflag;	/* select type of display */
int	interval, count;
int	infinite;
int	unit;
int	s;			/* socket file descriptor */
int	signalled;		/* set if alarm goes off "early" */
char	*progname;
char	interface[IFNAMSIZ];

void
usage()
{
	fprintf(stderr, "Usage: %s [-v|-r|-z] [-c count] [-w wait] [interface]\n",
		progname);
	exit(1);
}

/*
 * Called if an interval expires before intpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
void
catchalarm(arg)
    int arg;
{
	signalled = 1;
}

void
get_ppp_stats(curp)
    struct ppp_stats *curp;
{
	struct ifpppstatsreq req;

	strncpy(req.ifr_name, interface, sizeof(req.ifr_name));
	if (ioctl(s, SIOCGPPPSTATS, &req) == 0) {
		*curp = req.stats;
		return;
	}
	fprintf(stderr, "%s: ", progname);
	if (errno == ENOTTY)
		errx(1, "kernel support missing");
	else
		err(1, "couldn't get PPP statistics");
}

get_ppp_cstats(csp)
    struct ppp_comp_stats *csp;
{
	struct ifpppcstatsreq creq;

	strncpy(creq.ifr_name, interface, sizeof(creq.ifr_name));
	if (ioctl(s, SIOCGPPPCSTATS, &creq) == 0) {
		*csp = creq.stats;
		return;
	}
	if (errno == ENOTTY) {
		if (zflag)
			errx(1, "no kernel compression support\n");
		warnx("no kernel compression support\n");
		rflag = 0;
	} else
		err(1, "couldn't get PPP compression stats");
}


#define V(offset)	(cur.offset - old.offset)
#define W(offset)	(ccs.offset - ocs.offset)

#define CRATE(x)	((x).unc_bytes == 0? 0.0: \
			 1.0 - ((double)((x).comp_bytes + (x).inc_bytes) \
				/ (x).unc_bytes))

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed is cumulative.
 */
void
intpr()
{
	register int line = 0;
	sigset_t oldmask, mask;
	struct ppp_stats cur, old;
	struct ppp_comp_stats ccs, ocs;

	memset(&old, 0, sizeof(old));
	memset(&ocs, 0, sizeof(ocs));

	while (1) {
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
				printf("    BYTE   PACK     BYTE   PACK  RATIO | ");
				printf("    BYTE   PACK     BYTE   PACK  RATIO");
			} else {
				printf("%8.8s %6.6s %6.6s %6.6s %6.6s",
					"IN", "PACK", "VJCOMP", "VJUNC", "VJERR");
				if (vflag)
					printf(" %6.6s %6.6s", "VJTOSS", "NON-VJ");
				if (rflag)
					printf(" %6.6s %6.6s", "RATIO", "UBYTE");
				printf("  | %8.8s %6.6s %6.6s %6.6s %6.6s",
					"OUT", "PACK", "VJCOMP", "VJUNC", "NON-VJ");
				if (vflag)
					printf(" %6.6s %6.6s", "VJSRCH", "VJMISS");
				if(rflag)
					printf(" %6.6s %6.6s", "RATIO", "UBYTE");
			}
			putchar('\n');
		}
	
		if (zflag) {
			printf("%8u %6u %8u %6u %6.2f",
			       W(d.comp_bytes),
			       W(d.comp_packets),
			       W(d.inc_bytes),
			       W(d.inc_packets),
			       ccs.d.ratio == 0? 0.0:
			        1 - 1.0 / ccs.d.ratio * 256.0);

			printf(" | %8u %6u %8u %6u %6.2f",
			       W(c.comp_bytes),
			       W(c.comp_packets),
			       W(c.inc_bytes),
			       W(c.inc_packets),
			       ccs.c.ratio == 0? 0.0:
			        1 - 1.0 / ccs.c.ratio * 256.0);
	
		} else {

			printf("%8u %6u %6u %6u %6u",
			       V(p.ppp_ibytes),
			       V(p.ppp_ipackets),
			       V(vj.vjs_compressedin),
			       V(vj.vjs_uncompressedin),
			       V(vj.vjs_errorin));
			if (vflag)
				printf(" %6u %6u",
				       V(vj.vjs_tossed),
				       V(p.ppp_ipackets) -
				       V(vj.vjs_compressedin) -
				       V(vj.vjs_uncompressedin) -
				       V(vj.vjs_errorin));
			if (rflag)
				printf(" %6.2f %6u",
				       CRATE(ccs.d),
				       W(d.unc_bytes));
			printf("  | %8u %6u %6u %6u %6u",
			       V(p.ppp_obytes),
			       V(p.ppp_opackets),
			       V(vj.vjs_compressed),
			       V(vj.vjs_packets) - V(vj.vjs_compressed),
			       V(p.ppp_opackets) - V(vj.vjs_packets));
			if (vflag)
				printf(" %6u %6u",
				       V(vj.vjs_searches),
				       V(vj.vjs_misses));

			if (rflag)
				printf(" %6.2f %6u",
				       CRATE(ccs.c),
				       W(c.unc_bytes));
	    
		}

		putchar('\n');
		fflush(stdout);
		line++;

		count--;
		if (!infinite && !count)
			break;

		oldmask = sigblock(sigmask(SIGALRM));
		if (signalled == 0)
			sigpause(0);
		sigsetmask(oldmask);
		signalled = 0;
		(void)alarm(interval);
		old = cur;
		ocs = ccs;
	}
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	struct ifreq ifr;

	strcpy(interface, "ppp0");
	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		++progname;

	while ((c = getopt(argc, argv, "vrzc:w:")) != -1) {
		switch (c) {
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

	if (argc > 1)
		usage();
	if (argc > 0)
		strncpy(interface, argv[0], sizeof(interface));

	if (sscanf(interface, "ppp%d", &unit) != 1)
		errx(1, "invalid interface '%s' specified\n", interface);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "couldn't create IP socket");
	strcpy(ifr.ifr_name, interface);
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		errx(1, "nonexistent interface '%s' specified\n", interface);

	intpr();
	exit(0);
}
