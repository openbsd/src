/*	$OpenBSD: mount_nfs.c,v 1.38 2004/05/18 11:07:53 otto Exp $	*/
/*	$NetBSD: mount_nfs.c,v 1.12.4.1 1996/05/25 22:48:05 fvdl Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
static char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_nfs.c	8.11 (Berkeley) 5/4/95";
#else
static char rcsid[] = "$NetBSD: mount_nfs.c,v 1.12.4.1 1996/05/25 22:48:05 fvdl Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <syslog.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#ifdef ISO
#include <netiso/iso.h>
#endif

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#define _KERNEL
#include <nfs/nfs.h>
#undef _KERNEL

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"

#define	ALTF_BG		0x1
#define ALTF_NOCONN	0x2
#define ALTF_DUMBTIMR	0x4
#define ALTF_INTR	0x8
#define ALTF_NFSV3	0x20
#define ALTF_RDIRPLUS	0x40
#define	ALTF_MNTUDP	0x80
#define ALTF_RESVPORT	0x100
#define ALTF_SEQPACKET	0x200
#define ALTF_SOFT	0x800
#define ALTF_TCP	0x1000
#define ALTF_PORT	0x2000
#define ALTF_NFSV2	0x4000
#define ALTF_NOAC       0x8000
#define ALTF_ACREGMIN	0x10000
#define ALTF_ACREGMAX	0x20000
#define ALTF_ACDIRMIN	0x40000
#define ALTF_ACDIRMAX	0x80000

const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_UPDATE,
	{ "bg", 0, ALTF_BG, 1 },
	{ "conn", 1, ALTF_NOCONN, 1 },
	{ "dumbtimer", 0, ALTF_DUMBTIMR, 1 },
	{ "intr", 0, ALTF_INTR, 1 },
	{ "nfsv3", 0, ALTF_NFSV3, 1 },
	{ "rdirplus", 0, ALTF_RDIRPLUS, 1 },
	{ "mntudp", 0, ALTF_MNTUDP, 1 },
	{ "resvport", 0, ALTF_RESVPORT, 1 },
#ifdef ISO
	{ "seqpacket", 0, ALTF_SEQPACKET, 1 },
#endif
	{ "soft", 0, ALTF_SOFT, 1 },
	{ "tcp", 0, ALTF_TCP, 1 },
	{ "port", 0, ALTF_PORT, 1 },
	{ "nfsv2", 0, ALTF_NFSV2, 1 },
	{ "ac", 1, ALTF_NOAC, 1 },
	{ "acregmin", 0, ALTF_ACREGMIN, 1},
	{ "acregmax", 0, ALTF_ACREGMAX, 1},
	{ "acdirmin", 0, ALTF_ACDIRMIN, 1},
	{ "acdirmax", 0, ALTF_ACDIRMAX, 1},
	{ NULL }
};

struct nfs_args nfsdefargs = {
	NFS_ARGSVERSION,
	NULL,
	sizeof (struct sockaddr_in),
	SOCK_DGRAM,
	0,
	NULL,
	0,
	NFSMNT_NFSV3,
	NFS_WSIZE,
	NFS_RSIZE,
	NFS_READDIRSIZE,
	10,
	NFS_RETRANS,
	NFS_MAXGRPS,
	NFS_DEFRAHEAD,
	0,
	0,
	NULL,
	0,
	0,
	0,
	0
};

struct nfhret {
	u_long		stat;
	long		vers;
	long		auth;
	long		fhsize;
	u_char		nfh[NFSX_V3FHMAX];
};
#define	DEF_RETRY	10000
#define	BGRND	1
#define	ISBGRND	2
int retrycnt;
int opflags = 0;
int nfsproto = IPPROTO_UDP;
int mnttcp_ok = 1;
u_short port_no = 0;
int force2 = 0;
int force3 = 0;

int	getnfsargs(char *, struct nfs_args *);
#ifdef ISO
struct	iso_addr *iso_addr(const char *);
#endif
void	set_rpc_maxgrouplist(int);
__dead	void usage(void);
int	xdr_dir(XDR *, char *);
int	xdr_fh(XDR *, struct nfhret *);

int
main(int argc, char *argv[])
{
	int c;
	struct nfs_args *nfsargsp;
	struct nfs_args nfsargs;
	int mntflags, altflags, num;
	char name[MAXPATHLEN], *p, *spec;

	retrycnt = DEF_RETRY;

	mntflags = 0;
	altflags = 0;
	nfsargs = nfsdefargs;
	nfsargsp = &nfsargs;
	while ((c = getopt(argc, argv,
	    "23a:bcdD:g:I:iL:lo:PpqR:r:sTt:w:x:U")) != -1)
		switch (c) {
		case '3':
			if (force2)
				errx(1, "-2 and -3 are mutually exclusive");
			force3 = 1;
			break;
		case '2':
			if (force3)
				errx(1, "-2 and -3 are mutually exclusive");
			force2 = 1;
			nfsargsp->flags &= ~NFSMNT_NFSV3;
			break;
		case 'a':
			num = strtol(optarg, &p, 10);
			if (*p || num < 0)
				errx(1, "illegal -a value -- %s", optarg);
			nfsargsp->readahead = num;
			nfsargsp->flags |= NFSMNT_READAHEAD;
			break;
		case 'b':
			opflags |= BGRND;
			break;
		case 'c':
			nfsargsp->flags |= NFSMNT_NOCONN;
			break;
		case 'D':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -D value -- %s", optarg);
			nfsargsp->deadthresh = num;
			nfsargsp->flags |= NFSMNT_DEADTHRESH;
			break;
		case 'd':
			nfsargsp->flags |= NFSMNT_DUMBTIMR;
			break;
#if 0 /* XXXX */
		case 'g':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -g value -- %s", optarg);
			set_rpc_maxgrouplist(num);
			nfsargsp->maxgrouplist = num;
			nfsargsp->flags |= NFSMNT_MAXGRPS;
			break;
#endif
		case 'I':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -I value -- %s", optarg);
			nfsargsp->readdirsize = num;
			nfsargsp->flags |= NFSMNT_READDIRSIZE;
			break;
		case 'i':
			nfsargsp->flags |= NFSMNT_INT;
			break;
		case 'L':
			num = strtol(optarg, &p, 10);
			if (*p || num < 2)
				errx(1, "illegal -L value -- %s", optarg);
			nfsargsp->leaseterm = num;
			nfsargsp->flags |= NFSMNT_LEASETERM;
			break;
		case 'l':
			nfsargsp->flags |= NFSMNT_RDIRPLUS;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &altflags);
			if (altflags & ALTF_BG)
				opflags |= BGRND;
			if (altflags & ALTF_NOCONN)
				nfsargsp->flags |= NFSMNT_NOCONN;
			if (altflags & ALTF_DUMBTIMR)
				nfsargsp->flags |= NFSMNT_DUMBTIMR;
			if (altflags & ALTF_INTR)
				nfsargsp->flags |= NFSMNT_INT;
			if (altflags & ALTF_NFSV3) {
				if (force2)
					errx(1,"conflicting version options");
				force3 = 1;
			}
			if (altflags & ALTF_NFSV2) {
				if (force3)
					errx(1,"conflicting version options");
				force2 = 1;
				nfsargsp->flags &= ~NFSMNT_NFSV3;
			}
			if (altflags & ALTF_RDIRPLUS)
				nfsargsp->flags |= NFSMNT_RDIRPLUS;
			if (altflags & ALTF_MNTUDP)
				mnttcp_ok = 0;
			if (altflags & ALTF_RESVPORT)
				nfsargsp->flags |= NFSMNT_RESVPORT;
#ifdef ISO
			if (altflags & ALTF_SEQPACKET)
				nfsargsp->sotype = SOCK_SEQPACKET;
#endif
			if (altflags & ALTF_SOFT)
				nfsargsp->flags |= NFSMNT_SOFT;
			if (altflags & ALTF_TCP) {
				nfsargsp->sotype = SOCK_STREAM;
				nfsproto = IPPROTO_TCP;
			}
			if (altflags & ALTF_PORT)
				port_no = atoi(strstr(optarg, "port=") + 5);
			if (altflags & ALTF_NOAC) {
				nfsargsp->flags
				    |= (NFSMNT_ACREGMIN | NFSMNT_ACREGMAX |
					NFSMNT_ACDIRMIN | NFSMNT_ACDIRMAX);
				nfsargsp->acregmin = 0;
				nfsargsp->acregmax = 0;
				nfsargsp->acdirmin = 0;
				nfsargsp->acdirmax = 0;
			}
			if (altflags & ALTF_ACREGMIN) {
				nfsargsp->flags |= NFSMNT_ACREGMIN;
				nfsargsp->acregmin =
				    atoi(strstr(optarg, "acregmin=") + 9);
			}
			if (altflags & ALTF_ACREGMAX) {
				nfsargsp->flags |= NFSMNT_ACREGMAX;
				nfsargsp->acregmax =
				    atoi(strstr(optarg, "acregmax=") + 9);
			}
			if (altflags & ALTF_ACDIRMIN) {
				nfsargsp->flags |= NFSMNT_ACDIRMIN;
				nfsargsp->acdirmin =
				    atoi(strstr(optarg, "acdirmin=") + 9);
			}
			if (altflags & ALTF_ACDIRMAX) {
				nfsargsp->flags |= NFSMNT_ACDIRMAX;
				nfsargsp->acdirmax =
				    atoi(strstr(optarg, "acdirmax=") + 9);
			}
			altflags = 0;
			break;
		case 'P':
			nfsargsp->flags |= NFSMNT_RESVPORT;
			break;
#ifdef ISO
		case 'p':
			nfsargsp->sotype = SOCK_SEQPACKET;
			break;
#endif
		case 'R':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -R value -- %s", optarg);
			retrycnt = num;
			break;
		case 'r':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -r value -- %s", optarg);
			nfsargsp->rsize = num;
			nfsargsp->flags |= NFSMNT_RSIZE;
			break;
		case 's':
			nfsargsp->flags |= NFSMNT_SOFT;
			break;
		case 'T':
			nfsargsp->sotype = SOCK_STREAM;
			nfsproto = IPPROTO_TCP;
			break;
		case 't':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -t value -- %s", optarg);
			nfsargsp->timeo = num;
			nfsargsp->flags |= NFSMNT_TIMEO;
			break;
		case 'w':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -w value -- %s", optarg);
			nfsargsp->wsize = num;
			nfsargsp->flags |= NFSMNT_WSIZE;
			break;
		case 'x':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -x value -- %s", optarg);
			nfsargsp->retrans = num;
			nfsargsp->flags |= NFSMNT_RETRANS;
			break;
		case 'U':
			mnttcp_ok = 0;
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	spec = *argv++;
	if (realpath(*argv, name) == NULL)
		err(1, "realpath %s", name);

	if (!getnfsargs(spec, nfsargsp))
		exit(1);
	if (mount(MOUNT_NFS, name, mntflags, nfsargsp)) {
		if (errno == EOPNOTSUPP)
			errx(1, "%s: Filesystem not supported by kernel", name);
		else
			err(1, "%s", name);
	}
	exit(0);
}

int
getnfsargs(char *spec, struct nfs_args *nfsargsp)
{
	CLIENT *clp;
	struct hostent *hp;
	static struct sockaddr_in saddr;
#ifdef ISO
	static struct sockaddr_iso isoaddr;
	struct iso_addr *isop;
	int isoflag = 0;
#endif
	struct timeval pertry, try;
	enum clnt_stat clnt_stat;
	int so = RPC_ANYSOCK, i, nfsvers, mntvers, orgcnt;
	char *hostp, *delimp;
	u_short tport;
	static struct nfhret nfhret;
	static char nam[MNAMELEN + 1];

	if (strlcpy(nam, spec, sizeof(nam)) >= sizeof(nam)) {
		errx(1, "hostname too long");
	}

	if ((delimp = strchr(spec, '@')) != NULL) {
		hostp = delimp + 1;
	} else if ((delimp = strchr(spec, ':')) != NULL) {
		hostp = spec;
		spec = delimp + 1;
	} else {
		warnx("no <host>:<dirpath> or <dirpath>@<host> spec");
		return (0);
	}
	*delimp = '\0';
	/*
	 * DUMB!! Until the mount protocol works on iso transport, we must
	 * supply both an iso and an inet address for the host.
	 */
#ifdef ISO
	if (!strncmp(hostp, "iso=", 4)) {
		u_short isoport;

		hostp += 4;
		isoflag++;
		if ((delimp = strchr(hostp, '+')) == NULL) {
			warnx("no iso+inet address");
			return (0);
		}
		*delimp = '\0';
		if ((isop = iso_addr(hostp)) == NULL) {
			warnx("bad ISO address");
			return (0);
		}
		memset(&isoaddr, 0, sizeof (isoaddr));
		memcpy(&isoaddr.siso_addr, isop, sizeof (struct iso_addr));
		isoaddr.siso_len = sizeof (isoaddr);
		isoaddr.siso_family = AF_ISO;
		isoaddr.siso_tlen = 2;
		isoport = htons(NFS_PORT);
		memcpy(TSEL(&isoaddr), &isoport, isoaddr.siso_tlen);
		hostp = delimp + 1;
	}
#endif /* ISO */

	/*
	 * Handle an internet host address
	 */
	if (inet_aton(hostp, &saddr.sin_addr) == 0) {
		hp = gethostbyname(hostp);
		if (hp == NULL) {
			warnx("can't resolve address for host %s", hostp);
			return (0);
		}
		memcpy(&saddr.sin_addr, hp->h_addr, hp->h_length);
	}

	if (force2) {
		nfsvers = NFS_VER2;
		mntvers = RPCMNT_VER1;
	} else {
		nfsvers = NFS_VER3;
		mntvers = RPCMNT_VER3;
	}
	orgcnt = retrycnt;
tryagain:
	nfhret.stat = EACCES;	/* Mark not yet successful */
	while (retrycnt > 0) {
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(PMAPPORT);
		if ((tport = port_no ? port_no : pmap_getport(&saddr,
		    RPCPROG_NFS, nfsvers, nfsargsp->sotype == SOCK_STREAM ?
		    IPPROTO_TCP : IPPROTO_UDP)) == 0) {
			if ((opflags & ISBGRND) == 0)
				clnt_pcreateerror("NFS Portmap");
		} else {
			saddr.sin_port = 0;
			pertry.tv_sec = 10;
			pertry.tv_usec = 0;
			if (mnttcp_ok && nfsargsp->sotype == SOCK_STREAM)
			    clp = clnttcp_create(&saddr, RPCPROG_MNT, mntvers,
				&so, 0, 0);
			else
			    clp = clntudp_create(&saddr, RPCPROG_MNT, mntvers,
				pertry, &so);
			if (clp == NULL) {
				if ((opflags & ISBGRND) == 0)
					clnt_pcreateerror("Cannot MNT PRC");
			} else {
				clp->cl_auth = authunix_create_default();
				try.tv_sec = 10;
				try.tv_usec = 0;
				nfhret.auth = RPCAUTH_UNIX;
				nfhret.vers = mntvers;
				clnt_stat = clnt_call(clp, RPCMNT_MOUNT,
				    xdr_dir, spec, xdr_fh, &nfhret, try);
				if (clnt_stat != RPC_SUCCESS) {
					if (clnt_stat == RPC_PROGVERSMISMATCH) {
						if (nfsvers == NFS_VER3 &&
						    !force3) {
							retrycnt = orgcnt;
							nfsvers = NFS_VER2;
							mntvers = RPCMNT_VER1;
							nfsargsp->flags &=
								~NFSMNT_NFSV3;
							goto tryagain;
						} else {
							fprintf(stderr, "%s",
							    clnt_sperror(clp,
								"MNT RPC"));
						}
					}
					if ((opflags & ISBGRND) == 0)
						warnx("%s", clnt_sperror(clp,
						    "bad MNT RPC"));
				} else {
					auth_destroy(clp->cl_auth);
					clnt_destroy(clp);
					retrycnt = 0;
				}
			}
		}
		if (--retrycnt > 0) {
			if (opflags & BGRND) {
				opflags &= ~BGRND;
				if ((i = fork())) {
					if (i == -1)
						err(1, "nqnfs 2");
					exit(0);
				}
				(void) setsid();
				(void) close(STDIN_FILENO);
				(void) close(STDOUT_FILENO);
				(void) close(STDERR_FILENO);
				(void) chdir("/");
				opflags |= ISBGRND;
			}
			sleep(60);
		}
	}
	if (nfhret.stat) {
		if (opflags & ISBGRND)
			exit(1);
		errno = nfhret.stat;
		warnx("can't access %s: %s", spec, strerror(nfhret.stat));
		return (0);
	}
	saddr.sin_port = htons(tport);
#ifdef ISO
	if (isoflag) {
		nfsargsp->addr = (struct sockaddr *) &isoaddr;
		nfsargsp->addrlen = sizeof (isoaddr);
	} else
#endif /* ISO */
	{
		nfsargsp->addr = (struct sockaddr *) &saddr;
		nfsargsp->addrlen = sizeof (saddr);
	}
	nfsargsp->fh = nfhret.nfh;
	nfsargsp->fhsize = nfhret.fhsize;
	nfsargsp->hostname = nam;
	return (1);
}

/*
 * xdr routines for mount rpc's
 */
int
xdr_dir(XDR *xdrsp, char *dirp)
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

int
xdr_fh(XDR *xdrsp, struct nfhret *np)
{
	int i;
	long auth, authcnt, authfnd = 0;

	if (!xdr_u_long(xdrsp, &np->stat))
		return (0);
	if (np->stat)
		return (1);
	switch (np->vers) {
	case 1:
		np->fhsize = NFSX_V2FH;
		return (xdr_opaque(xdrsp, (caddr_t)np->nfh, NFSX_V2FH));
	case 3:
		if (!xdr_long(xdrsp, &np->fhsize))
			return (0);
		if (np->fhsize <= 0 || np->fhsize > NFSX_V3FHMAX)
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)np->nfh, np->fhsize))
			return (0);
		if (!xdr_long(xdrsp, &authcnt))
			return (0);
		for (i = 0; i < authcnt; i++) {
			if (!xdr_long(xdrsp, &auth))
				return (0);
			if (auth == np->auth)
				authfnd++;
		}
		/*
		 * Some servers, such as DEC's OSF/1 return a nil authenticator
		 * list to indicate RPCAUTH_UNIX.
		 */
		if (!authfnd && (authcnt > 0 || np->auth != RPCAUTH_UNIX))
			np->stat = EAUTH;
		return (1);
	};
	return (0);
}

__dead void
usage(void)
{
	(void)fprintf(stderr, "usage: mount_nfs %s\n%s\n%s\n%s\n",
	    "[-23PTUbcdilqs] [-a maxreadahead] [-D deadthresh]",
	    "\t[-I readdirsize] [-g maxgroups] [-L leaseterm] [-o options]",
	    "\t[-R retrycnt] [-r readsize] [-t timeout] [-w writesize] [-x retrans]",
	    "\trhost:path node");
	exit(1);
}
