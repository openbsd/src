/*	$OpenBSD: nfsstat.c,v 1.23 2004/05/09 03:28:11 deraadt Exp $	*/
/*	$NetBSD: nfsstat.c,v 1.7 1996/03/03 17:21:30 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1989, 1993
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
"@(#) Copyright (c) 1983, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)nfsstat.c	8.1 (Berkeley) 6/6/93";
static char *rcsid = "$NetBSD: nfsstat.c,v 1.7 1996/03/03 17:21:30 thorpej Exp $";
#else
static char *rcsid = "$OpenBSD: nfsstat.c,v 1.23 2004/05/09 03:28:11 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <kvm.h>
#include <nlist.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <paths.h>
#include <err.h>

#define SHOW_SERVER 0x01
#define SHOW_CLIENT 0x02
#define SHOW_ALL (SHOW_SERVER | SHOW_CLIENT)

struct nlist nl[] = {
#define	N_NFSSTAT	0
	{ "_nfsstats" },
	{ "" },
};
kvm_t *kd;
volatile sig_atomic_t signalled;	/* set if alarm goes off "early" */
int nfs_id;

void getnfsstats(struct nfsstats *);
void printhdr(void);
void intpr(u_int);
void sidewaysintpr(u_int, u_int);
void usage(void);
void catchalarm(int);

int
main(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;
	char *p;
	u_int interval;
	u_int display = SHOW_ALL;
	char *memf, *nlistf;
	int ch;

	interval = 0;
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "cM:N:sw:")) != -1)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'w':
			interval = (u_int)strtol(optarg, &p, 0);
			if (*optarg == '\0' || *p != '\0')
				errx(1, "invalid interval");
			break;
		case 's':
			display = SHOW_SERVER;
			break;
		case 'c':
			display = SHOW_CLIENT;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif
	if (nlistf || memf) {
		char errbuf[_POSIX2_LINE_MAX];

		if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf)) == 0)
			errx(1, "nfsstat: %s", errbuf);
		if (kvm_nlist(kd, nl) != 0)
			errx(1, "kvm_nlist: can't get names");
	} else {
		int mib[4];
		size_t len;

		mib[0] = CTL_VFS;
		mib[1] = VFS_GENERIC;
		mib[2] = VFS_MAXTYPENUM;
		len = sizeof(nfs_id);
		if (sysctl(mib, 3, &nfs_id, &len, NULL, 0))
			err(1, "sysctl: VFS_MAXTYPENUM");

		for (; nfs_id; nfs_id--) {
			struct vfsconf vfsc;

			mib[0] = CTL_VFS;
			mib[1] = VFS_GENERIC;
			mib[2] = VFS_CONF;
			mib[3] = nfs_id;

			len = sizeof(vfsc);
			if (sysctl(mib, 4, &vfsc, &len, NULL, 0))
				continue;

			if (!strcmp(vfsc.vfc_name, MOUNT_NFS))
				break;
		}
		if (nfs_id == 0)
			errx(1, "cannot find nfs filesystem id");
	}

	if (interval)
		sidewaysintpr(interval, display);
	else
		intpr(display);

	return 0;
}

void
getnfsstats(struct nfsstats *p)
{
	if (kd) {
		if (kvm_read(kd, nl[N_NFSSTAT].n_value, p, sizeof(*p)) != sizeof(*p))
			errx(1, "kvm_read failed");
	} else {
		int mib[3];
		size_t len = sizeof(*p);

		mib[0] = CTL_VFS;
		mib[1] = nfs_id; /* 2 */
		mib[2] = NFS_NFSSTATS;

		if (sysctl(mib, 3, p, &len, NULL, 0))
			err(1, "sysctl");
	}
}

/*
 * Print a description of the nfs stats.
 */
void
intpr(u_int display)
{
	struct nfsstats nfsstats;

	getnfsstats(&nfsstats);

	if (display & SHOW_CLIENT) {
		printf("Client Info:\n");
		printf("Rpc Counts:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		    "Getattr", "Setattr", "Lookup", "Readlink", "Read",
		    "Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    nfsstats.rpccnt[NFSPROC_GETATTR],
		    nfsstats.rpccnt[NFSPROC_SETATTR],
		    nfsstats.rpccnt[NFSPROC_LOOKUP],
		    nfsstats.rpccnt[NFSPROC_READLINK],
		    nfsstats.rpccnt[NFSPROC_READ],
		    nfsstats.rpccnt[NFSPROC_WRITE],
		    nfsstats.rpccnt[NFSPROC_CREATE],
		    nfsstats.rpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		    "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
		    "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    nfsstats.rpccnt[NFSPROC_RENAME],
		    nfsstats.rpccnt[NFSPROC_LINK],
		    nfsstats.rpccnt[NFSPROC_SYMLINK],
		    nfsstats.rpccnt[NFSPROC_MKDIR],
		    nfsstats.rpccnt[NFSPROC_RMDIR],
		    nfsstats.rpccnt[NFSPROC_READDIR],
		    nfsstats.rpccnt[NFSPROC_READDIRPLUS],
		    nfsstats.rpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
		    "Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
		    nfsstats.rpccnt[NFSPROC_MKNOD],
		    nfsstats.rpccnt[NFSPROC_FSSTAT],
		    nfsstats.rpccnt[NFSPROC_FSINFO],
		    nfsstats.rpccnt[NFSPROC_PATHCONF],
		    nfsstats.rpccnt[NFSPROC_COMMIT]);
		printf("Rpc Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
		    "TimedOut", "Invalid", "X Replies", "Retries", "Requests");
		printf("%9d %9d %9d %9d %9d\n",
		    nfsstats.rpctimeouts,
		    nfsstats.rpcinvalid,
		    nfsstats.rpcunexpected,
		    nfsstats.rpcretries,
		    nfsstats.rpcrequests);
		printf("Cache Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s",
		    "Attr Hits", "Misses", "Lkup Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n",
		    "BioR Hits", "Misses", "BioW Hits", "Misses");
		printf("%9d %9d %9d %9d",
		    nfsstats.attrcache_hits, nfsstats.attrcache_misses,
		    nfsstats.lookupcache_hits, nfsstats.lookupcache_misses);
		printf(" %9d %9d %9d %9d\n",
		    nfsstats.biocache_reads-nfsstats.read_bios,
		    nfsstats.read_bios,
		    nfsstats.biocache_writes-nfsstats.write_bios,
		    nfsstats.write_bios);
		printf("%9.9s %9.9s %9.9s %9.9s",
		    "BioRLHits", "Misses", "BioD Hits", "Misses");
		printf(" %9.9s %9.9s\n", "DirE Hits", "Misses");
		printf("%9d %9d %9d %9d",
		    nfsstats.biocache_readlinks-nfsstats.readlink_bios,
		    nfsstats.readlink_bios,
		    nfsstats.biocache_readdirs-nfsstats.readdir_bios,
		    nfsstats.readdir_bios);
		printf(" %9d %9d\n",
		    nfsstats.direofcache_hits, nfsstats.direofcache_misses);
	}
	if (display & SHOW_SERVER) {
		printf("\nServer Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		    "Getattr", "Setattr", "Lookup", "Readlink", "Read",
		    "Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    nfsstats.srvrpccnt[NFSPROC_GETATTR],
		    nfsstats.srvrpccnt[NFSPROC_SETATTR],
		    nfsstats.srvrpccnt[NFSPROC_LOOKUP],
		    nfsstats.srvrpccnt[NFSPROC_READLINK],
		    nfsstats.srvrpccnt[NFSPROC_READ],
		    nfsstats.srvrpccnt[NFSPROC_WRITE],
		    nfsstats.srvrpccnt[NFSPROC_CREATE],
		    nfsstats.srvrpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		    "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
		    "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    nfsstats.srvrpccnt[NFSPROC_RENAME],
		    nfsstats.srvrpccnt[NFSPROC_LINK],
		    nfsstats.srvrpccnt[NFSPROC_SYMLINK],
		    nfsstats.srvrpccnt[NFSPROC_MKDIR],
		    nfsstats.srvrpccnt[NFSPROC_RMDIR],
		    nfsstats.srvrpccnt[NFSPROC_READDIR],
		    nfsstats.srvrpccnt[NFSPROC_READDIRPLUS],
		    nfsstats.srvrpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
		    "Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
		    nfsstats.srvrpccnt[NFSPROC_MKNOD],
		    nfsstats.srvrpccnt[NFSPROC_FSSTAT],
		    nfsstats.srvrpccnt[NFSPROC_FSINFO],
		    nfsstats.srvrpccnt[NFSPROC_PATHCONF],
		    nfsstats.srvrpccnt[NFSPROC_COMMIT]);
		printf("Server Ret-Failed\n");
		printf("%17d\n", nfsstats.srvrpc_errs);
		printf("Server Faults\n");
		printf("%13d\n", nfsstats.srv_errs);
		printf("Server Cache Stats:\n");
		printf("%9.9s %9.9s %9.9s %9.9s\n",
		    "Inprog", "Idem", "Non-idem", "Misses");
		printf("%9d %9d %9d %9d\n",
		    nfsstats.srvcache_inproghits,
		    nfsstats.srvcache_idemdonehits,
		    nfsstats.srvcache_nonidemdonehits,
		    nfsstats.srvcache_misses);
		printf("Server Write Gathering:\n");
		printf("%9.9s %9.9s %9.9s\n",
		    "WriteOps", "WriteRPC", "Opsaved");
		printf("%9d %9d %9d\n",
		    nfsstats.srvvop_writes,
		    nfsstats.srvrpccnt[NFSPROC_WRITE],
		    nfsstats.srvrpccnt[NFSPROC_WRITE] - nfsstats.srvvop_writes);
	}
}

/*
 * Print a running summary of nfs statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
sidewaysintpr(u_int interval, u_int display)
{
	struct nfsstats nfsstats, lastst;
	int hdrcnt;
	sigset_t emptyset;

	(void)signal(SIGALRM, catchalarm);
	signalled = 0;
	(void)alarm(interval);
	bzero((caddr_t)&lastst, sizeof(lastst));

	for (hdrcnt = 1;;) {
		if (!--hdrcnt) {
			printhdr();
			hdrcnt = 20;
		}

		getnfsstats(&nfsstats);

		if (display & SHOW_CLIENT)
		  printf("Client: %8d %8d %8d %8d %8d %8d %8d %8d\n",
		    nfsstats.rpccnt[NFSPROC_GETATTR] -
		    lastst.rpccnt[NFSPROC_GETATTR],
		    nfsstats.rpccnt[NFSPROC_LOOKUP] -
		    lastst.rpccnt[NFSPROC_LOOKUP],
		    nfsstats.rpccnt[NFSPROC_READLINK] -
		    lastst.rpccnt[NFSPROC_READLINK],
		    nfsstats.rpccnt[NFSPROC_READ] -
		    lastst.rpccnt[NFSPROC_READ],
		    nfsstats.rpccnt[NFSPROC_WRITE] -
		    lastst.rpccnt[NFSPROC_WRITE],
		    nfsstats.rpccnt[NFSPROC_RENAME] -
		    lastst.rpccnt[NFSPROC_RENAME],
		    nfsstats.rpccnt[NFSPROC_ACCESS] -
		    lastst.rpccnt[NFSPROC_ACCESS],
		    (nfsstats.rpccnt[NFSPROC_READDIR] -
		    lastst.rpccnt[NFSPROC_READDIR]) +
		    (nfsstats.rpccnt[NFSPROC_READDIRPLUS] -
		    lastst.rpccnt[NFSPROC_READDIRPLUS]));
		if (display & SHOW_SERVER)
		  printf("Server: %8d %8d %8d %8d %8d %8d %8d %8d\n",
		    nfsstats.srvrpccnt[NFSPROC_GETATTR] -
		    lastst.srvrpccnt[NFSPROC_GETATTR],
		    nfsstats.srvrpccnt[NFSPROC_LOOKUP] -
		    lastst.srvrpccnt[NFSPROC_LOOKUP],
		    nfsstats.srvrpccnt[NFSPROC_READLINK] -
		    lastst.srvrpccnt[NFSPROC_READLINK],
		    nfsstats.srvrpccnt[NFSPROC_READ] -
		    lastst.srvrpccnt[NFSPROC_READ],
		    nfsstats.srvrpccnt[NFSPROC_WRITE] -
		    lastst.srvrpccnt[NFSPROC_WRITE],
		    nfsstats.srvrpccnt[NFSPROC_RENAME] -
		    lastst.srvrpccnt[NFSPROC_RENAME],
		    nfsstats.srvrpccnt[NFSPROC_ACCESS] -
		    lastst.srvrpccnt[NFSPROC_ACCESS],
		    (nfsstats.srvrpccnt[NFSPROC_READDIR] -
		    lastst.srvrpccnt[NFSPROC_READDIR]) +
		    (nfsstats.srvrpccnt[NFSPROC_READDIRPLUS] -
		    lastst.srvrpccnt[NFSPROC_READDIRPLUS]));
		lastst = nfsstats;
		fflush(stdout);
		sigemptyset(&emptyset);
		if (!signalled)
			sigsuspend(&emptyset);
		signalled = 0;
		(void)alarm(interval);
	}
	/*NOTREACHED*/
}

void
printhdr(void)
{
	printf("        %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s\n",
	    "Getattr", "Lookup", "Readlink", "Read", "Write", "Rename",
	    "Access", "Readdir");
	fflush(stdout);
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
void
catchalarm(int signo)
{
	signalled = 1;
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-cs] [-M core] [-N system] [-w interval]\n",
	    __progname);
	exit(1);
}
