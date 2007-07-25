/*	$OpenBSD: main.c,v 1.68 2007/07/25 11:50:47 claudio Exp $	*/
/*	$NetBSD: main.c,v 1.9 1996/05/07 02:55:02 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	Regents of the University of California.  All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)main.c	8.4 (Berkeley) 3/1/94";
#else
static char *rcsid = "$OpenBSD: main.c,v 1.68 2007/07/25 11:50:47 claudio Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/route.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

struct nlist nl[] = {
#define N_MBSTAT	0
	{ "_mbstat" },
#define N_IPSTAT	1
	{ "_ipstat" },
#define N_TCBTABLE	2
	{ "_tcbtable" },
#define N_TCPSTAT	3
	{ "_tcpstat" },
#define N_UDBTABLE	4
	{ "_udbtable" },
#define N_UDPSTAT	5
	{ "_udpstat" },
#define N_IFNET		6
	{ "_ifnet" },
#define N_ICMPSTAT	7
	{ "_icmpstat" },
#define N_RTSTAT	8
	{ "_rtstat" },
#define N_UNIXSW	9
	{ "_unixsw" },
#define N_RTREE		10
	{ "_rt_tables"},
#define N_FILE		11
	{ "_file" },
#define N_IGMPSTAT	12
	{ "_igmpstat" },
#define N_MRTPROTO	13
	{ "_ip_mrtproto" },
#define N_MRTSTAT	14
	{ "_mrtstat" },
#define N_MFCHASHTBL	15
	{ "_mfchashtbl" },
#define N_MFCHASH	16
	{ "_mfchash" },
#define N_VIFTABLE	17
	{ "_viftable" },
#define N_AHSTAT	18
	{ "_ahstat"},
#define N_ESPSTAT	19
	{ "_espstat"},
#define N_IP4STAT	20
	{ "_ipipstat"},
#define N_DDPSTAT	21
	{ "_ddpstat"},
#define N_DDPCB		22
	{ "_ddpcb"},
#define N_ETHERIPSTAT	23
	{ "_etheripstat"},
#define N_IP6STAT	24
	{ "_ip6stat" },
#define N_ICMP6STAT	25
	{ "_icmp6stat" },
#define N_PIM6STAT	26
	{ "_pim6stat" },
#define N_MRT6PROTO	27
	{ "_ip6_mrtproto" },
#define N_MRT6STAT	28
	{ "_mrt6stat" },
#define N_MF6CTABLE	29
	{ "_mf6ctable" },
#define N_MIF6TABLE	30
	{ "_mif6table" },
#define N_MBPOOL	31
	{ "_mbpool" },
#define N_MCLPOOL	32
	{ "_mclpool" },
#define N_IPCOMPSTAT	33
	{ "_ipcompstat" },
#define N_RIP6STAT	34
	{ "_rip6stat" },
#define N_CARPSTAT	35
	{ "_carpstats" },
#define N_RAWIPTABLE	36
	{ "_rawcbtable" },
#define N_RAWIP6TABLE	37
	{ "_rawin6pcbtable" },
#define N_PFSYNCSTAT	38
	{ "_pfsyncstats" },
#define N_PIMSTAT	39
	{ "_pimstat" },
#define N_AF2RTAFIDX	40
	{ "_af2rtafidx" },
#define N_RTBLIDMAX	41
	{ "_rtbl_id_max" },
#define N_RTMASK	42
	{ "_mask_rnhead" },
	{ ""}
};

struct protox {
	u_char	pr_index;			/* index into nlist of cb head */
	u_char	pr_sindex;			/* index into nlist of stat block */
	u_char	pr_wanted;			/* 1 if wanted, 0 otherwise */
	void	(*pr_cblocks)(u_long, char *);	/* control blocks printing routine */
	void	(*pr_stats)(u_long, char *);	/* statistics printing routine */
	void	(*pr_dump)(u_long);		/* pcb printing routine */
	char	*pr_name;			/* well-known name */
} protox[] = {
	{ N_TCBTABLE,	N_TCPSTAT,	1,	protopr,
	  tcp_stats,	tcp_dump,	"tcp" },
	{ N_UDBTABLE,	N_UDPSTAT,	1,	protopr,
	  udp_stats,	0,		"udp" },
	{ N_RAWIPTABLE,	N_IPSTAT,	1,	protopr,
	  ip_stats,	0,		"ip" },
	{ -1,		N_ICMPSTAT,	1,	0,
	  icmp_stats,	0,		"icmp" },
	{ -1,		N_IGMPSTAT,	1,	0,
	  igmp_stats,	0,		"igmp" },
	{ -1,		N_AHSTAT,	1,	0,
	  ah_stats,	0,		"ah" },
	{ -1,		N_ESPSTAT,	1,	0,
	  esp_stats,	0,		"esp" },
	{ -1,		N_IP4STAT,	1,	0,
	  ipip_stats,	0,		"ipencap" },
	{ -1,		N_ETHERIPSTAT,	1,	0,
	  etherip_stats,0,		"etherip" },
	{ -1,		N_IPCOMPSTAT,	1,	0,
	  ipcomp_stats,	0,		"ipcomp" },
	{ -1,		N_CARPSTAT,	1,	0,
	  carp_stats,	0,		"carp" },
	{ -1,		N_PFSYNCSTAT,	1,	0,
	  pfsync_stats,	0,		"pfsync" },
	{ -1,		N_PIMSTAT,	1,	0,
	  pim_stats,	0,		"pim" },
	{ -1,		-1,		0,	0,
	  0,		0,		0 }
};

#ifdef INET6
struct protox ip6protox[] = {
	{ N_TCBTABLE,	N_TCPSTAT,	1,	ip6protopr,
	  0,		tcp_dump,	"tcp" },
	{ N_UDBTABLE,	N_UDPSTAT,	1,	ip6protopr,
	  0,		0,		"udp" },
	{ N_RAWIP6TABLE,N_IP6STAT,	1,	ip6protopr,
	  ip6_stats,	0,		"ip6" },
	{ -1,		N_ICMP6STAT,	1,	0,
	  icmp6_stats,	0,		"icmp6" },
	{ -1,		N_PIM6STAT,	1,	0,
	  pim6_stats,	0,		"pim6" },
	{ -1,		N_RIP6STAT,	1,	0,
	  rip6_stats,	0,		"rip6" },
	{ -1,		-1,		0,	0,
	  0,		0,		0 }
};
#endif

struct protox atalkprotox[] = {
	{ N_DDPCB,	N_DDPSTAT,	1,	atalkprotopr,
	  ddp_stats,	0,		"ddp" },
	{ -1,		-1,		0,	0,
	  0,		0,		0 }
};

#ifndef INET6
struct protox *protoprotox[] = {
	protox, atalkprotox, NULL
};
#else
struct protox *protoprotox[] = {
	protox, ip6protox, atalkprotox, NULL
};
#endif

static void printproto(struct protox *, char *);
static void usage(void);
static struct protox *name2protox(char *);
static struct protox *knownname(char *);

kvm_t *kvmd;

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	const char *errstr;
	struct protoent *p;
	struct protox *tp = NULL; /* for printing cblocks & stats */
	int ch;
	char *nlistf = NULL, *memf = NULL, *ep;
	char buf[_POSIX2_LINE_MAX];
	gid_t gid;
	u_long pcbaddr = 0;
	u_int tableid = 0;

	af = AF_UNSPEC;

	while ((ch = getopt(argc, argv, "AabdFf:gI:ilM:mN:np:P:qrsT:tuvW:w:")) != -1)
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'F':
			Fflag = 1;
			break;
		case 'f':
			if (strcmp(optarg, "inet") == 0)
				af = AF_INET;
			else if (strcmp(optarg, "inet6") == 0)
				af = AF_INET6;
			else if (strcmp(optarg, "local") == 0)
				af = AF_LOCAL;
			else if (strcmp(optarg, "unix") == 0)
				af = AF_UNIX;
			else if (strcmp(optarg, "encap") == 0)
				af = PF_KEY;
			else if (strcmp(optarg, "atalk") == 0)
				af = AF_APPLETALK;
			else if (strcmp(optarg, "mask") == 0)
				af = 0xff;
			else {
				(void)fprintf(stderr,
				    "%s: %s: unknown address family\n",
				    __progname, optarg);
				exit(1);
			}
			break;
		case 'g':
			gflag = 1;
			break;
		case 'I':
			iflag = 1;
			interface = optarg;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			if ((tp = name2protox(optarg)) == NULL) {
				(void)fprintf(stderr,
				    "%s: %s: unknown protocol\n",
				    __progname, optarg);
				exit(1);
			}
			pflag = 1;
			break;
		case 'P':
			errno = 0;
			pcbaddr = strtoul(optarg, &ep, 16);
			if (optarg[0] == '\0' || *ep != '\0' ||
			    errno == ERANGE) {
				(void)fprintf(stderr,
				    "%s: %s: invalid PCB address\n",
				    __progname, optarg);
				exit(1);
			}
			Pflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			++sflag;
			break;
		case 'T':
			tableid = strtonum(optarg, 0, RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "invalid table id: %s", errstr);
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			af = AF_UNIX;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'W':
			Wflag = 1;
			interface = optarg;
			break;
		case 'w':
			interval = atoi(optarg);
			iflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	/*
	 * Show per-interface statistics which don't need access to
	 * kernel memory (they're using IOCTLs)
	 */
	if (Wflag) {
		if (interface == NULL)
			usage();
		net80211_ifstats(interface);
		exit(0);
	}

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 * Dumping PCB info is also restricted.
	 */
	gid = getgid();
	if (nlistf != NULL || memf != NULL || Pflag)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");
	if (nlistf == NULL && memf == NULL && rflag && !Aflag) {
		/* printing the routing table no longer needs kvm */
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");
		if (sflag)
			rt_stats(1, 0);
		else
			p_rttables(af, tableid);
		exit(0);
	}
	if ((kvmd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
	    buf)) == NULL) {
		fprintf(stderr, "%s: kvm_open: %s\n", __progname, buf);
		exit(1);
	}

	if (nlistf == NULL && memf == NULL && !Pflag)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		if (isdigit(**argv)) {
			interval = atoi(*argv);
			if (interval <= 0)
				usage();
			++argv;
			iflag = 1;
		}
		if (*argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif

	if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0) {
		if (nlistf)
			fprintf(stderr, "%s: %s: no namelist\n", __progname,
			    nlistf);
		else
			fprintf(stderr, "%s: no namelist\n", __progname);
		exit(1);
	}
	if (mflag) {
		mbpr(nl[N_MBSTAT].n_value, nl[N_MBPOOL].n_value,
		    nl[N_MCLPOOL].n_value);
		exit(0);
	}
	if (pflag) {
		printproto(tp, tp->pr_name);
		exit(0);
	}
	if (Pflag) {
		if (tp == NULL && (tp = name2protox("tcp")) == NULL) {
			(void)fprintf(stderr,
			    "%s: %s: unknown protocol\n",
			    __progname, "tcp");
			exit(1);
		}
		if (tp->pr_dump)
			(tp->pr_dump)(pcbaddr);
		exit(0);
	}
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
	
	if (iflag) {
		intpr(interval, nl[N_IFNET].n_value);
		exit(0);
	}
	if (rflag) {
		if (sflag)
			rt_stats(0, nl[N_RTSTAT].n_value);
		else
			routepr(nl[N_RTREE].n_value, nl[N_RTMASK].n_value,
			    nl[N_AF2RTAFIDX].n_value, nl[N_RTBLIDMAX].n_value);
		exit(0);
	}
	if (gflag) {
		if (sflag) {
			if (af == AF_INET || af == AF_UNSPEC)
				mrt_stats(nl[N_MRTPROTO].n_value,
				    nl[N_MRTSTAT].n_value);
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mrt6_stats(nl[N_MRT6PROTO].n_value,
				    nl[N_MRT6STAT].n_value);
#endif
		}
		else {
			if (af == AF_INET || af == AF_UNSPEC)
				mroutepr(nl[N_MRTPROTO].n_value,
				    nl[N_MFCHASHTBL].n_value,
				    nl[N_MFCHASH].n_value,
				    nl[N_VIFTABLE].n_value);
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mroute6pr(nl[N_MRT6PROTO].n_value,
				    nl[N_MF6CTABLE].n_value,
				    nl[N_MIF6TABLE].n_value);
#endif
		}
		exit(0);
	}
	if (af == AF_INET || af == AF_UNSPEC) {
		setprotoent(1);
		setservent(1);
		/* ugh, this is O(MN) ... why do we do this? */
		while ((p = getprotoent())) {
			for (tp = protox; tp->pr_name; tp++)
				if (strcmp(tp->pr_name, p->p_name) == 0)
					break;
			if (tp->pr_name == 0 || tp->pr_wanted == 0)
				continue;
			printproto(tp, p->p_name);
		}
		endprotoent();
	}
#ifdef INET6
	if (af == AF_INET6 || af == AF_UNSPEC)
		for (tp = ip6protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
#endif
	if ((af == AF_UNIX || af == AF_UNSPEC) && !sflag)
		unixpr(nl[N_UNIXSW].n_value);
	if (af == AF_APPLETALK || af == AF_UNSPEC)
		for (tp = atalkprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	exit(0);
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * If the interface was not specifically requested, and the symbol
 * is not in the namelist, ignore this one.
 */
static void
printproto(struct protox *tp, char *name)
{
	void (*pr)(u_long, char *);
	u_char i;

	if (sflag) {
		pr = tp->pr_stats;
		i = tp->pr_sindex;
	} else {
		pr = tp->pr_cblocks;
		i = tp->pr_index;
	}
	if (pr != NULL && i < sizeof(nl) / sizeof(nl[0]) &&
	    (nl[i].n_value || af != AF_UNSPEC))
		(*pr)(nl[i].n_value, name);
}

/*
 * Read kernel memory, return 0 on success.
 */
int
kread(u_long addr, void *buf, int size)
{

	if (kvm_read(kvmd, addr, buf, size) != size) {
		(void)fprintf(stderr, "%s: %s\n", __progname,
		    kvm_geterr(kvmd));
		return (-1);
	}
	return (0);
}

char *
plural(int n)
{
	return (n != 1 ? "s" : "");
}

char *
plurales(int n)
{
	return (n != 1 ? "es" : "");
}

/*
 * Find the protox for the given "well-known" name.
 */
static struct protox *
knownname(char *name)
{
	struct protox **tpp, *tp;

	for (tpp = protoprotox; *tpp; tpp++)
		for (tp = *tpp; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, name) == 0)
				return (tp);
	return (NULL);
}

/*
 * Find the protox corresponding to name.
 */
static struct protox *
name2protox(char *name)
{
	struct protox *tp;
	char **alias;			/* alias from p->aliases */
	struct protoent *p;

	/*
	 * Try to find the name in the list of "well-known" names. If that
	 * fails, check if name is an alias for an Internet protocol.
	 */
	if ((tp = knownname(name)))
		return (tp);

	setprotoent(1);			/* make protocol lookup cheaper */
	while ((p = getprotoent())) {
		/* assert: name not same as p->name */
		for (alias = p->p_aliases; *alias; alias++)
			if (strcmp(name, *alias) == 0) {
				endprotoent();
				return (knownname(p->p_name));
			}
	}
	endprotoent();
	return (NULL);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-Aan] [-f address_family] [-M core] [-N system]\n"
	    "       %s [-bdFgilmnqrstu] [-f address_family] [-M core] [-N system] [-T tableid]\n"
	    "       %s [-bdn] [-I interface] [-M core] [-N system] [-w wait]\n"
	    "       %s [-M core] [-N system] -P pcbaddr\n"
	    "       %s [-s] [-M core] [-N system] [-p protocol]\n"
	    "       %s [-a] [-f address_family] [-i | -I interface]\n"
	    "       %s [-W interface]\n",
	    __progname, __progname, __progname, __progname,
	    __progname, __progname, __progname);
	exit(1);
}
