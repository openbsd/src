/*	$OpenBSD: main.c,v 1.83 2010/06/29 03:09:29 blambert Exp $	*/
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
#define N_TCBTABLE	0
	{ "_tcbtable" },
#define N_UDBTABLE	1
	{ "_udbtable" },
#define N_DDPCB		2
	{ "_ddpcb"},
#define N_UNIXSW	3
	{ "_unixsw" },

#define N_MFCHASHTBL	4
	{ "_mfchashtbl" },
#define N_MFCHASH	5
	{ "_mfchash" },
#define N_VIFTABLE	6
	{ "_viftable" },

#define N_MF6CTABLE	7
	{ "_mf6ctable" },
#define N_MIF6TABLE	8
	{ "_mif6table" },

#define N_RTREE		9
	{ "_rt_tables"},
#define N_RTMASK	10
	{ "_mask_rnhead" },
#define N_AF2RTAFIDX	11
	{ "_af2rtafidx" },
#define N_RTBLIDMAX	12
	{ "_rtbl_id_max" },

#define N_RAWIPTABLE	13
	{ "_rawcbtable" },
#define N_RAWIP6TABLE	14
	{ "_rawin6pcbtable" },
#define N_DIVBTABLE	15
	{ "_divbtable" },
#define N_DIVB6TABLE	16
	{ "_divb6table" },

	{ ""}
};

struct protox {
	u_char	pr_index;			/* index into nlist of cb head */
	void	(*pr_cblocks)(u_long, char *, int); /* control blocks printing routine */
	void	(*pr_stats)(char *);		/* statistics printing routine */
	void	(*pr_dump)(u_long);		/* pcb printing routine */
	char	*pr_name;			/* well-known name */
} protox[] = {
	{ N_TCBTABLE,	protopr,	tcp_stats,	tcp_dump,	"tcp" },
	{ N_UDBTABLE,	protopr,	udp_stats,	NULL,		"udp" },
	{ N_RAWIPTABLE,	protopr,	ip_stats,	NULL,		"ip" },
	{ N_DIVBTABLE,	protopr,	div_stats,	NULL,		"divert" },
	{ -1,		NULL,		icmp_stats,	NULL,		"icmp" },
	{ -1,		NULL,		igmp_stats,	NULL,		"igmp" },
	{ -1,		NULL,		ah_stats,	NULL,		"ah" },
	{ -1,		NULL,		esp_stats,	NULL,		"esp" },
	{ -1,		NULL,		ipip_stats,	NULL,		"ipencap" },
	{ -1,		NULL,		etherip_stats,	NULL,		"etherip" },
	{ -1,		NULL,		ipcomp_stats,	NULL,		"ipcomp" },
	{ -1,		NULL,		carp_stats,	NULL,		"carp" },
	{ -1,		NULL,		pfsync_stats,	NULL,		"pfsync" },
	{ -1,		NULL,		pim_stats,	NULL,		"pim" },
	{ -1,		NULL,		pflow_stats,	NULL,		"pflow" },
	{ -1,		NULL,		NULL,		NULL,		NULL }
};

struct protox ip6protox[] = {
	{ N_TCBTABLE,	protopr,	NULL,		tcp_dump,	"tcp" },
	{ N_UDBTABLE,	protopr,	NULL,		NULL,		"udp" },
	{ N_RAWIP6TABLE,protopr,	ip6_stats,	NULL,		"ip6" },
	{ N_DIVB6TABLE,	protopr,	div6_stats,	NULL,		"divert6" },
	{ -1,		NULL,		icmp6_stats,	NULL,		"icmp6" },
	{ -1,		NULL,		pim6_stats,	NULL,		"pim6" },
	{ -1,		NULL,		rip6_stats,	NULL,		"rip6" },
	{ -1,		NULL,		NULL,		NULL,		NULL }
};

struct protox atalkprotox[] = {
	{ N_DDPCB,	atalkprotopr,	ddp_stats,	NULL,		"ddp" },
	{ -1,		NULL,		NULL,		NULL,		NULL }
};

struct protox *protoprotox[] = {
	protox, ip6protox, atalkprotox, NULL
};

static void printproto(struct protox *, char *, int);
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
	int repeatcount = 0;

	af = AF_UNSPEC;

	while ((ch = getopt(argc, argv, "Aabc:dFf:gI:ilM:mN:np:P:qrsT:tuvW:w:")) != -1)
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
		case 'c':
			repeatcount = strtonum(optarg, 1, INT_MAX, &errstr);
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
			else if (strcmp(optarg, "mpls") == 0)
				af = AF_MPLS;
			else if (strcmp(optarg, "pflow") == 0)
				af = PF_PFLOW;
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

	if ((kvmd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
	    buf)) == NULL) {
		fprintf(stderr, "%s: kvm_openfiles: %s\n", __progname, buf);
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
		mbpr();
		exit(0);
	}
	if (pflag) {
		printproto(tp, tp->pr_name, af);
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
		intpr(interval, repeatcount);
		exit(0);
	}
	if (rflag) {
		if (sflag)
			rt_stats();
		else if (Aflag || nlistf != NULL || memf != NULL)
			routepr(nl[N_RTREE].n_value, nl[N_RTMASK].n_value,
			    nl[N_AF2RTAFIDX].n_value, nl[N_RTBLIDMAX].n_value,
			    tableid);
		else
			p_rttables(af, tableid);
		exit(0);
	}
	if (gflag) {
		if (sflag) {
			if (af == AF_INET || af == AF_UNSPEC)
				mrt_stats();
			if (af == AF_INET6 || af == AF_UNSPEC)
				mrt6_stats();
		} else {
			if (af == AF_INET || af == AF_UNSPEC)
				mroutepr(nl[N_MFCHASHTBL].n_value,
				    nl[N_MFCHASH].n_value,
				    nl[N_VIFTABLE].n_value);
			if (af == AF_INET6 || af == AF_UNSPEC)
				mroute6pr(nl[N_MF6CTABLE].n_value,
				    nl[N_MIF6TABLE].n_value);
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
			if (tp->pr_name == 0)
				continue;
			printproto(tp, p->p_name, af);
		}
		endprotoent();
	}
	if (af == PF_PFLOW || af == AF_UNSPEC) {
		tp = name2protox("pflow");
		printproto(tp, tp->pr_name, af);
	}
	if (af == AF_INET6 || af == AF_UNSPEC)
		for (tp = ip6protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name, af);
	if ((af == AF_UNIX || af == AF_UNSPEC) && !sflag)
		unixpr(nl[N_UNIXSW].n_value);
	if (af == AF_APPLETALK || af == AF_UNSPEC)
		for (tp = atalkprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name, af);
	exit(0);
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * If the interface was not specifically requested, and the symbol
 * is not in the namelist, ignore this one.
 */
static void
printproto(struct protox *tp, char *name, int af)
{
	if (sflag) {
		if (tp->pr_stats != NULL)
			(*tp->pr_stats)(name);
	} else {
		u_char i = tp->pr_index;
		if (tp->pr_cblocks != NULL &&
		    i < sizeof(nl) / sizeof(nl[0]) &&
		    (nl[i].n_value || af != AF_UNSPEC))
			(*tp->pr_cblocks)(nl[i].n_value, name, af);
	}
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
plural(u_int64_t n)
{
	return (n != 1 ? "s" : "");
}

char *
plurales(u_int64_t n)
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
	    "       %s [-bdFgilmnqrstu] [-f address_family] [-M core] [-N system]\n"
	    "               [-T tableid]\n"
	    "       %s [-bdn] [-c count] [-I interface] [-M core] [-N system] [-w wait]\n"
	    "       %s [-M core] [-N system] -P pcbaddr\n"
	    "       %s [-s] [-M core] [-N system] [-p protocol]\n"
	    "       %s [-a] [-f address_family] [-i | -I interface]\n"
	    "       %s [-W interface]\n",
	    __progname, __progname, __progname, __progname,
	    __progname, __progname, __progname);
	exit(1);
}
