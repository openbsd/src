/*	$OpenBSD: main.c,v 1.113 2018/08/13 14:36:54 mpi Exp $	*/
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

#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
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
#define N_RTREE		0
	{ "_rt_tables"},
#define N_RTMASK	1
	{ "_mask_rnhead" },
#define N_AF2RTAFIDX	2
	{ "_af2rtafidx" },
#define N_RTBLIDMAX	3
	{ "_rtbl_id_max" },

	{ "" }
};

struct protox {
	void	(*pr_stats)(char *);	/* statistics printing routine */
	char	*pr_name;		/* well-known name */
	int	pr_proto;		/* protocol number */
} protox[] = {
	{ ip_stats,	"ip",	IPPROTO_IPV4 },
	{ icmp_stats,	"icmp", 0 },
	{ igmp_stats,	"igmp", 0 },
	{ ipip_stats,	"ipencap", 0 },
	{ tcp_stats,	"tcp",	IPPROTO_TCP },
	{ udp_stats,	"udp",	IPPROTO_UDP },
	{ ipsec_stats,	"ipsec", 0 },
	{ esp_stats,	"esp", 0 },
	{ ah_stats,	"ah", 0 },
	{ etherip_stats,"etherip", 0 },
	{ ipcomp_stats,	"ipcomp", 0 },
	{ carp_stats,	"carp", 0 },
	{ pfsync_stats,	"pfsync", 0 },
	{ div_stats,	"divert", IPPROTO_DIVERT },
	{ pflow_stats,	"pflow", 0 },
	{ NULL,		NULL, 0 }
};

struct protox ip6protox[] = {
	{ ip6_stats,	"ip6", IPPROTO_IPV6 },
	{ div6_stats,	"divert6", IPPROTO_DIVERT },
	{ icmp6_stats,	"icmp6", 0 },
	{ rip6_stats,	"rip6", 0 },
	{ NULL,		NULL, 0 }
};

struct protox *protoprotox[] = {
	protox, ip6protox, NULL
};

static void usage(void);
static struct protox *name2protox(char *);
static struct protox *knownname(char *);
u_int gettable(const char *);

kvm_t *kvmd;

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	const char *errstr;
	struct protox *tp = NULL; /* for printing cblocks & stats */
	int ch;
	char *nlistf = NULL, *memf = NULL, *ep;
	char buf[_POSIX2_LINE_MAX];
	u_long pcbaddr = 0;
	u_int tableid;
	int Tflag = 0;
	int repeatcount = 0;
	int proto = 0;
	int need_nlist, kvm_flags = O_RDONLY;

	af = AF_UNSPEC;
	tableid = getrtable();

	while ((ch = getopt(argc, argv,
	    "AaBbc:dFf:ghI:iLlM:mN:np:P:qrsT:tuvW:w:")) != -1)
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'B':
			Bflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'c':
			repeatcount = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "count is %s", errstr);
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
			else if (strcmp(optarg, "mpls") == 0)
				af = AF_MPLS;
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
		case 'h':
			hflag = 1;
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
			Tflag = 1;
			tableid = gettable(optarg);
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
			interval = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "interval is %s", errstr);
			iflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc) {
		interval = strtonum(*argv, 1, INT_MAX, &errstr);
		if (errstr)
			errx(1, "interval is %s", errstr);
		++argv;
		--argc;
		iflag = 1;
	}
	if (argc)
		usage();

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

	if (mflag) {
		mbpr();
		exit(0);
	}
	if (iflag) {
		intpr(interval, repeatcount);
		exit(0);
	}
	if (sflag) {
		if (rflag) {
			rt_stats();
		} else if (gflag) {
			if (af == AF_INET || af == AF_UNSPEC)
				mrt_stats();
			if (af == AF_INET6 || af == AF_UNSPEC)
				mrt6_stats();
		} else if (pflag && tp->pr_name) {
			(*tp->pr_stats)(tp->pr_name);
		} else {
			if (af == AF_INET || af == AF_UNSPEC)
				for (tp = protox; tp->pr_name; tp++)
					(*tp->pr_stats)(tp->pr_name);
			if (af == AF_INET6 || af == AF_UNSPEC)
				for (tp = ip6protox; tp->pr_name; tp++)
					(*tp->pr_stats)(tp->pr_name);
		}
		exit(0);
	}
	if (gflag) {
		if (af == AF_INET || af == AF_UNSPEC)
			mroutepr();
		if (af == AF_INET6 || af == AF_UNSPEC)
			mroute6pr();
		exit(0);
	}

	/*
	 * The remaining code may need kvm so lets try to open it.
	 * -r and -P are the only bits left that actually can use this.
	 */
	need_nlist = (nlistf != NULL) || (memf != NULL) || (Aflag && rflag);
	if (!need_nlist && !Pflag)
		kvm_flags |= KVM_NO_FILES;

	if ((kvmd = kvm_openfiles(nlistf, memf, NULL, kvm_flags, buf)) == NULL)
		errx(1, "kvm_openfiles: %s", buf);

	if (need_nlist && (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0)) {
		if (nlistf)
			errx(1, "%s: no namelist", nlistf);
		else
			errx(1, "no namelist");
	}

	if (rflag) {
		if (Aflag || nlistf != NULL || memf != NULL)
			routepr(nl[N_RTREE].n_value, nl[N_RTMASK].n_value,
			    nl[N_AF2RTAFIDX].n_value, nl[N_RTBLIDMAX].n_value,
			    tableid);
		else
			p_rttables(af, tableid);
		exit(0);
	}

	if (pflag) {
		if (tp->pr_proto == 0)
			errx(1, "no protocol handler for protocol %s",
			    tp->pr_name);
		else
			proto = tp->pr_proto;
	}

	protopr(kvmd, pcbaddr, tableid, proto);
	exit(0);
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

char *
pluralys(u_int64_t n)
{
	return (n != 1 ? "ies" : "y");
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
	    "usage: %s [-AaBln] [-f address_family] [-M core] [-N system]\n"
	    "       %s [-bdFgilmnqrstu] [-f address_family] [-M core] [-N system]\n"
	    "               [-T tableid]\n"
	    "       %s [-bdhn] [-c count] [-I interface] [-M core] [-N system] [-w wait]\n"
	    "       %s [-v] [-M core] [-N system] -P pcbaddr\n"
	    "       %s [-s] [-M core] [-N system] [-p protocol]\n"
	    "       %s [-a] [-f address_family] [-i | -I interface]\n"
	    "       %s [-W interface]\n",
	    __progname, __progname, __progname, __progname,
	    __progname, __progname, __progname);
	exit(1);
}

u_int
gettable(const char *s)
{
	const char *errstr;
	struct rt_tableinfo info;
	int mib[6];
	size_t len;
	u_int tableid;

	tableid = strtonum(s, 0, RT_TABLEID_MAX, &errstr);
	if (errstr)
		errx(1, "invalid table id: %s", errstr);

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_TABLE;
	mib[5] = tableid;

	len = sizeof(info);
	if (sysctl(mib, 6, &info, &len, NULL, 0) == -1)
		err(1, "routing table %d", tableid);

	return (tableid);
}
