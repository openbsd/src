/*	$NetBSD: vmstat.c,v 1.29.4.1 1996/06/05 00:21:05 cgd Exp $	*/
/*	$OpenBSD: vmstat.c,v 1.93 2004/11/28 14:05:24 miod Exp $	*/

/*
 * Copyright (c) 1980, 1986, 1991, 1993
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
"@(#) Copyright (c) 1980, 1986, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)vmstat.c	8.1 (Berkeley) 6/6/93";
#else
static const char rcsid[] = "$OpenBSD: vmstat.c,v 1.93 2004/11/28 14:05:24 miod Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <limits.h>
#include "dkstats.h"

#include <uvm/uvm_object.h>
#include <uvm/uvm_extern.h>

struct nlist namelist[] = {
#define X_UVMEXP	0		/* sysctl */
	{ "_uvmexp" },
#define	X_BOOTTIME	1		/* sysctl */
	{ "_boottime" },
#define X_NCHSTATS	2		/* sysctl */
	{ "_nchstats" },
#define	X_KMEMSTAT	3		/* sysctl */
	{ "_kmemstats" },
#define	X_KMEMBUCKETS	4		/* sysctl */
	{ "_bucket" },
#define	X_FORKSTAT	5		/* sysctl */
	{ "_forkstat" },
#define X_NSELCOLL	6		/* sysctl */
	{ "_nselcoll" },
#define X_POOLHEAD	7		/* sysctl */
	{ "_pool_head" },
#define X_ALLEVENTS	8		/* no sysctl */
	{ "_allevents" },
#define X_END		9		/* no sysctl */
	{ "" },
};

/* Objects defined in dkstats.c */
extern struct _disk	cur, last;
extern char	**dr_name;
extern int	*dk_select, dk_ndrive;

struct	uvmexp uvmexp, ouvmexp;
int		ndrives;

int	winlines = 20;

kvm_t *kd;

#define	FORKSTAT	0x01
#define	INTRSTAT	0x02
#define	MEMSTAT		0x04
#define	SUMSTAT		0x08
#define	TIMESTAT	0x10
#define	VMSTAT		0x20

void	cpustats(void);
void	dkstats(void);
void	dointr(void);
void	domem(void);
void	dopool(void);
void	dosum(void);
void	dovmstat(u_int, int);
void	kread(int, void *, size_t);
void	usage(void);
void	dotimes(void);
void	doforkst(void);
void	printhdr(void);

char	**choosedrives(char **);

/* Namelist and memory file names. */
char	*nlistf, *memf;

extern char *__progname;

int verbose = 0;
int zflag = 0;

int ncpu;

int
main(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;
	int mib[2];
	size_t size;
	int c, todo;
	u_int interval;
	int reps;
	char errbuf[_POSIX2_LINE_MAX];

	interval = reps = todo = 0;
	while ((c = getopt(argc, argv, "c:fiM:mN:stw:vz")) != -1) {
		switch (c) {
		case 'c':
			reps = atoi(optarg);
			break;
		case 'f':
			todo |= FORKSTAT;
			break;
		case 'i':
			todo |= INTRSTAT;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			todo |= MEMSTAT;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 's':
			todo |= SUMSTAT;
			break;
		case 't':
			todo |= TIMESTAT;
			break;
		case 'w':
			interval = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'z':
			zflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (todo == 0)
		todo = VMSTAT;

	if (nlistf != NULL || memf != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
#if notyet
	if (nlistf != NULL || memf != NULL) {
#endif
		kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
		if (kd == 0)
			errx(1, "kvm_openfiles: %s", errbuf);

		if ((c = kvm_nlist(kd, namelist)) != 0) {
			setgid(getgid());
			setegid(getegid());

			if (c > 0) {
				(void)fprintf(stderr,
				    "%s: undefined symbols:", __progname);
				for (c = 0;
				    c < sizeof(namelist)/sizeof(namelist[0]);
				    c++)
					if (namelist[c].n_type == 0)
						fprintf(stderr, " %s",
						    namelist[c].n_name);
				(void)fputc('\n', stderr);
				exit(1);
			} else
				errx(1, "kvm_nlist: %s", kvm_geterr(kd));
		}
#ifdef notyet
	}
#endif /* notyet */

	setegid(getegid());
	setgid(getgid());

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	size = sizeof(ncpu);
	(void) sysctl(mib, 2, &ncpu, &size, NULL, 0);

	if (todo & VMSTAT) {
		struct winsize winsize;

		dkinit(0);	/* Initialize disk stats, no disks selected. */
		argv = choosedrives(argv);	/* Select disks. */
		winsize.ws_row = 0;
		(void) ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&winsize);
		if (winsize.ws_row > 0)
			winlines = winsize.ws_row;

	}

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv)
			reps = atoi(*argv);
	}
#endif

	if (interval) {
		if (!reps)
			reps = -1;
	} else if (reps)
		interval = 1;

	if (todo & FORKSTAT)
		doforkst();
	if (todo & MEMSTAT) {
		domem();
		dopool();
	}
	if (todo & SUMSTAT)
		dosum();
	if (todo & TIMESTAT)
		dotimes();
	if (todo & INTRSTAT)
		dointr();
	if (todo & VMSTAT)
		dovmstat(interval, reps);
	exit(0);
}

char **
choosedrives(char **argv)
{
	int i;

	/*
	 * Choose drives to be displayed.  Priority goes to (in order) drives
	 * supplied as arguments, default drives.  If everything isn't filled
	 * in and there are drives not taken care of, display the first few
	 * that fit.
	 */
#define BACKWARD_COMPATIBILITY
	for (ndrives = 0; *argv; ++argv) {
#ifdef	BACKWARD_COMPATIBILITY
		if (isdigit(**argv))
			break;
#endif
		for (i = 0; i < dk_ndrive; i++) {
			if (strcmp(dr_name[i], *argv))
				continue;
			dk_select[i] = 1;
			++ndrives;
			break;
		}
	}
	for (i = 0; i < dk_ndrive && ndrives < 2; i++) {
		if (dk_select[i])
			continue;
		dk_select[i] = 1;
		++ndrives;
	}
	return(argv);
}

time_t
getuptime(void)
{
	static time_t now;
	static struct timeval boottime;
	time_t uptime;
	int mib[2];
	size_t size;

	if (boottime.tv_sec == 0) {
		if (nlistf == NULL && memf == NULL) {
			size = sizeof(boottime);
			mib[0] = CTL_KERN;
			mib[1] = KERN_BOOTTIME;
			if (sysctl(mib, 2, &boottime, &size, NULL, 0) < 0) {
				warn("could not get kern.boottime");
				bzero(&boottime, sizeof(boottime));
			}
		} else {
			kread(X_BOOTTIME, &boottime, sizeof(boottime));
		}
	}
	(void)time(&now);
	uptime = now - boottime.tv_sec;
	if (uptime <= 0 || uptime > 60*60*24*365*10)
		errx(1, "time makes no sense; namelist must be wrong");

	return(uptime);
}

int	hz, hdrcnt;

void
dovmstat(u_int interval, int reps)
{
	struct vmtotal total;
	time_t uptime, halfuptime;
	void needhdr(int);
	int mib[2];
	struct clockinfo clkinfo;
	size_t size;

	uptime = getuptime();
	halfuptime = uptime / 2;
	(void)signal(SIGCONT, needhdr);

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	size = sizeof(clkinfo);
	if (sysctl(mib, 2, &clkinfo, &size, NULL, 0) < 0) {
		warn("could not read kern.clockrate");
		return;
	}
	hz = clkinfo.stathz;

	for (hdrcnt = 1;;) {
		/* Read new disk statistics */
		dkreadstats();
		if (!--hdrcnt || last.dk_ndrive != cur.dk_ndrive)
			printhdr();
		if (nlistf == NULL && memf == NULL) {
			size = sizeof(struct uvmexp);
			mib[0] = CTL_VM;
			mib[1] = VM_UVMEXP;
			if (sysctl(mib, 2, &uvmexp, &size, NULL, 0) < 0) {
				warn("could not get vm.uvmexp");
				bzero(&uvmexp, sizeof(struct uvmexp));
			}
		} else {
			kread(X_UVMEXP, &uvmexp, sizeof(struct uvmexp));
		}
		size = sizeof(total);
		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		if (sysctl(mib, 2, &total, &size, NULL, 0) < 0) {
			warn("could not read vm.vmmeter");
			bzero(&total, sizeof(total));
		}
		(void)printf("%2u%2u%2u",
		    total.t_rq - 1, total.t_dw + total.t_pw, total.t_sw);
#define	rate(x)	(((x) + halfuptime) / uptime)	/* round */
#define pgtok(a) ((a) * ((int)uvmexp.pagesize >> 10))
		(void)printf("%7u%7u ",
		    pgtok(total.t_avm), pgtok(total.t_free));
		(void)printf("%5u ", rate(uvmexp.faults - ouvmexp.faults));
		(void)printf("%3u ", rate(uvmexp.pdreact - ouvmexp.pdreact));
		(void)printf("%3u ", rate(uvmexp.pageins - ouvmexp.pageins));
		(void)printf("%3u %3u ",
		    rate(uvmexp.pdpageouts - ouvmexp.pdpageouts), 0);
		(void)printf("%3u ", rate(uvmexp.pdscans - ouvmexp.pdscans));
		dkstats();
		(void)printf("%4u %5u %4u ",
		    rate(uvmexp.intrs - ouvmexp.intrs),
		    rate(uvmexp.syscalls - ouvmexp.syscalls),
		    rate(uvmexp.swtch - ouvmexp.swtch));
		cpustats();
		(void)printf("\n");
		(void)fflush(stdout);
		if (reps >= 0 && --reps <= 0)
			break;
		ouvmexp = uvmexp;
		uptime = interval;
		/*
		 * We round upward to avoid losing low-frequency events
		 * (i.e., >= 1 per interval but < 1 per second).
		 */
		halfuptime = uptime == 1 ? 0 : (uptime + 1) / 2;
		(void)sleep(interval);
	}
}

void
printhdr(void)
{
	int i;

	(void)printf(" procs   memory        page%*s", 20, "");
	if (ndrives > 0)
		(void)printf("%s %*straps         cpu\n",
		   ((ndrives > 1) ? "disks" : "disk"),
		   ((ndrives > 1) ? ndrives * 4 - 4 : 0), "");
	else
		(void)printf("%*s  traps          cpu\n",
		   ndrives * 3, "");

	(void)printf(" r b w    avm    fre   flt  re  pi  po  fr  sr ");
	for (i = 0; i < dk_ndrive; i++)
		if (dk_select[i])
			(void)printf("%c%c%c ", dr_name[i][0],
			    dr_name[i][1],
			    dr_name[i][strlen(dr_name[i]) - 1]);
	(void)printf(" int   sys   cs us sy id\n");
	hdrcnt = winlines - 2;
}

/*
 * Force a header to be prepended to the next output.
 */
/* ARGSUSED */
void
needhdr(int signo)
{

	hdrcnt = 1;
}

void
dotimes(void)
{
	u_int pgintime, rectime;
	int mib[2];
	size_t size;

	/* XXX Why are these set to 0 ? This doesn't look right. */
	pgintime = 0;
	rectime = 0;

	if (nlistf == NULL && memf == NULL) {
		size = sizeof(struct uvmexp);
		mib[0] = CTL_VM;
		mib[1] = VM_UVMEXP;
		if (sysctl(mib, 2, &uvmexp, &size, NULL, 0) < 0) {
			warn("could not read vm.uvmexp");
			bzero(&uvmexp, sizeof(struct uvmexp));
		}
	} else {
		kread(X_UVMEXP, &uvmexp, sizeof(struct uvmexp));
	}

	(void)printf("%u reactivates, %u total time (usec)\n",
	    uvmexp.pdreact, rectime);
	if (uvmexp.pdreact != 0)
		(void)printf("average: %u usec / reclaim\n",
		    rectime / uvmexp.pdreact);
	(void)printf("\n");
	(void)printf("%u page ins, %u total time (msec)\n",
	    uvmexp.pageins, pgintime / 10);
	if (uvmexp.pageins != 0)
		(void)printf("average: %8.1f msec / page in\n",
	    	    pgintime / (uvmexp.pageins * 10.0));
}

int
pct(long top, long bot)
{
	long ans;

	if (bot == 0)
		return(0);
	ans = (quad_t)top * 100 / bot;
	return (ans);
}

#define	PCT(top, bot) pct((long)(top), (long)(bot))

void
dosum(void)
{
	struct nchstats nchstats;
	long nchtotal;
	size_t size;
	int mib[2], nselcoll;

	if (nlistf == NULL && memf == NULL) {
		size = sizeof(struct uvmexp);
		mib[0] = CTL_VM;
		mib[1] = VM_UVMEXP;
		if (sysctl(mib, 2, &uvmexp, &size, NULL, 0) < 0) {
			warn("could not read vm.uvmexp");
			bzero(&uvmexp, sizeof(struct uvmexp));
		}
	} else {
		kread(X_UVMEXP, &uvmexp, sizeof(struct uvmexp));
	}

	/* vm_page constants */
	(void)printf("%11u bytes per page\n", uvmexp.pagesize);

	/* vm_page counters */
	(void)printf("%11u pages managed\n", uvmexp.npages);
	(void)printf("%11u pages free\n", uvmexp.free);
	(void)printf("%11u pages active\n", uvmexp.active);
	(void)printf("%11u pages inactive\n", uvmexp.inactive);
	(void)printf("%11u pages being paged out\n", uvmexp.paging);
	(void)printf("%11u pages wired\n", uvmexp.wired);
	(void)printf("%11u pages zeroed\n", uvmexp.zeropages);
	(void)printf("%11u pages reserved for pagedaemon\n",
		     uvmexp.reserve_pagedaemon);
	(void)printf("%11u pages reserved for kernel\n",
		     uvmexp.reserve_kernel);

	/* swap */
	(void)printf("%11u swap pages\n", uvmexp.swpages);
	(void)printf("%11u swap pages in use\n", uvmexp.swpginuse);
	(void)printf("%11u total anon's in system\n", uvmexp.nanon);
	(void)printf("%11u free anon's\n", uvmexp.nfreeanon);

	/* stat counters */
	(void)printf("%11u page faults\n", uvmexp.faults);
	(void)printf("%11u traps\n", uvmexp.traps);
	(void)printf("%11u interrupts\n", uvmexp.intrs);
	(void)printf("%11u cpu context switches\n", uvmexp.swtch);
	(void)printf("%11u software interrupts\n", uvmexp.softs);
	(void)printf("%11u syscalls\n", uvmexp.syscalls);
	(void)printf("%11u pagein operations\n", uvmexp.pageins);
	(void)printf("%11u swap ins\n", uvmexp.swapins);
	(void)printf("%11u swap outs\n", uvmexp.swapouts);
	(void)printf("%11u forks\n", uvmexp.forks);
	(void)printf("%11u forks where vmspace is shared\n",
		     uvmexp.forks_sharevm);

	/* daemon counters */
	(void)printf("%11u number of times the pagedaemon woke up\n",
		     uvmexp.pdwoke);
	(void)printf("%11u revolutions of the clock hand\n", uvmexp.pdrevs);
	(void)printf("%11u pages freed by pagedaemon\n", uvmexp.pdfreed);
	(void)printf("%11u pages scanned by pagedaemon\n", uvmexp.pdscans);
	(void)printf("%11u pages reactivated by pagedaemon\n", uvmexp.pdreact);
	(void)printf("%11u busy pages found by pagedaemon\n", uvmexp.pdbusy);

	if (nlistf == NULL && memf == NULL) {
		size = sizeof(nchstats);
		mib[0] = CTL_KERN;
		mib[1] = KERN_NCHSTATS;
		if (sysctl(mib, 2, &nchstats, &size, NULL, 0) < 0) {
			warn("could not read kern.nchstats");
			bzero(&nchstats, sizeof(nchstats));
		}
	} else {
		kread(X_NCHSTATS, &nchstats, sizeof(nchstats));
	}

	nchtotal = nchstats.ncs_goodhits + nchstats.ncs_neghits +
	    nchstats.ncs_badhits + nchstats.ncs_falsehits +
	    nchstats.ncs_miss + nchstats.ncs_long;
	(void)printf("%11ld total name lookups\n", nchtotal);
	(void)printf("%11s cache hits (%d%% pos + %d%% neg) system %d%% "
	    "per-directory\n",
	    "", PCT(nchstats.ncs_goodhits, nchtotal),
	    PCT(nchstats.ncs_neghits, nchtotal),
	    PCT(nchstats.ncs_pass2, nchtotal));
	(void)printf("%11s deletions %d%%, falsehits %d%%, toolong %d%%\n", "",
	    PCT(nchstats.ncs_badhits, nchtotal),
	    PCT(nchstats.ncs_falsehits, nchtotal),
	    PCT(nchstats.ncs_long, nchtotal));

	if (nlistf == NULL && memf == NULL) {
		size = sizeof(nselcoll);
		mib[0] = CTL_KERN;
		mib[1] = KERN_NSELCOLL;
		if (sysctl(mib, 2, &nselcoll, &size, NULL, 0) < 0) {
			warn("could not read kern.nselcoll");
			nselcoll = 0;
		}
	} else {
		kread(X_NSELCOLL, &nselcoll, sizeof(nselcoll));
	}
	(void)printf("%11d select collisions\n", nselcoll);
}

void
doforkst(void)
{
	struct forkstat fks;
	size_t size;
	int mib[2];

	if (nlistf == NULL && memf == NULL) {
		size = sizeof(struct forkstat);
		mib[0] = CTL_KERN;
		mib[1] = KERN_FORKSTAT;
		if (sysctl(mib, 2, &fks, &size, NULL, 0) < 0) {
			warn("could not read kern.forkstat");
			bzero(&fks, sizeof(struct forkstat));
		}
	} else {
		kread(X_FORKSTAT, &fks, sizeof(struct forkstat));
	}

	(void)printf("%d forks, %d pages, average %.2f\n",
	    fks.cntfork, fks.sizfork, (double)fks.sizfork / fks.cntfork);
	(void)printf("%d vforks, %d pages, average %.2f\n",
	    fks.cntvfork, fks.sizvfork,
	    (double)fks.sizvfork / (fks.cntvfork ? fks.cntvfork : 1));
	(void)printf("%d rforks, %d pages, average %.2f\n",
	    fks.cntrfork, fks.sizrfork,
	    (double)fks.sizrfork / (fks.cntrfork ? fks.cntrfork : 1));
	(void)printf("%d kthread creations, %d pages, average %.2f\n",
	    fks.cntkthread, fks.sizkthread,
	    (double)fks.sizkthread / (fks.cntkthread ? fks.cntkthread : 1));
}

void
dkstats(void)
{
	int dn, state;
	double etime;

	/* Calculate disk stat deltas. */
	dkswap();
	etime = 0;
	for (state = 0; state < CPUSTATES; ++state) {
		etime += cur.cp_time[state];
	}
	if (etime == 0)
		etime = 1;
	etime /= hz;
	etime /= ncpu;
	for (dn = 0; dn < dk_ndrive; ++dn) {
		if (!dk_select[dn])
			continue;
		(void)printf("%3.0f ",
		    (cur.dk_rxfer[dn] + cur.dk_rxfer[dn]) / etime);
	}
}

void
cpustats(void)
{
	int state;
	double pct, total;

	total = 0;
	for (state = 0; state < CPUSTATES; ++state)
		total += cur.cp_time[state];
	if (total)
		pct = 100 / total;
	else
		pct = 0;
	(void)printf("%2.0f ", (cur.cp_time[CP_USER] + cur.cp_time[CP_NICE]) * pct);
	(void)printf("%2.0f ", (cur.cp_time[CP_SYS] + cur.cp_time[CP_INTR]) * pct);
	(void)printf("%2.0f", cur.cp_time[CP_IDLE] * pct);
}

void
dointr(void)
{
	struct evcntlist allevents;
	struct evcnt evcnt, *evptr;
	struct device dev;

	time_t uptime;
	u_int64_t inttotal;
	int nintr;
	char intrname[128];
	int mib[4];
	size_t siz;
	int i;

	if (nlistf != NULL || memf != NULL) {
		errx(1,
		    "interrupt statistics are only available on live kernels");
	}

	uptime = getuptime();

	mib[0] = CTL_KERN;
	mib[1] = KERN_INTRCNT;
	mib[2] = KERN_INTRCNT_NUM;
	siz = sizeof(nintr);
	if (sysctl(mib, 3, &nintr, &siz, NULL, 0) < 0) {
		warnx("could not read kern.intrcnt.nintrcnt");
		return;
	}

	(void)printf("%-16s %20s %8s\n", "interrupt", "total", "rate");

	inttotal = 0;
	for (i = 0; i < nintr; i++) {
		char name[128];
		u_quad_t cnt;
		int vector;

		mib[0] = CTL_KERN;
		mib[1] = KERN_INTRCNT;
		mib[2] = KERN_INTRCNT_NAME;
		mib[3] = i;
		siz = sizeof(name);
		if (sysctl(mib, 4, name, &siz, NULL, 0) < 0) {
			warnx("could not read kern.intrcnt.name.%d", i);
			return;
		}

		mib[0] = CTL_KERN;
		mib[1] = KERN_INTRCNT;
		mib[2] = KERN_INTRCNT_VECTOR;
		mib[3] = i;
		siz = sizeof(vector);
		if (sysctl(mib, 4, &vector, &siz, NULL, 0) < 0) {
			strlcpy(intrname, name, sizeof(intrname));
		} else {
			snprintf(intrname, sizeof(intrname), "irq%d/%s",
			    vector, name);
		}

		mib[0] = CTL_KERN;
		mib[1] = KERN_INTRCNT;
		mib[2] = KERN_INTRCNT_CNT;
		mib[3] = i;
		siz = sizeof(cnt);
		if (sysctl(mib, 4, &cnt, &siz, NULL, 0) < 0) {
			warnx("could not read kern.intrcnt.cnt.%d", i);
			return;
		}

		if (cnt || zflag)
			(void)printf("%-16.16s %20llu %8llu\n", intrname,
			    cnt, cnt / uptime);
		inttotal += cnt;
	}

	kread(X_ALLEVENTS, &allevents, sizeof allevents);
	evptr = allevents.tqh_first;
	while (evptr) {
		if (kvm_read(kd, (long)evptr, (void *)&evcnt,
		    sizeof evcnt) != sizeof evcnt)
			errx(1, "event chain trashed: %s", kvm_geterr(kd));
		if (strcmp(evcnt.ev_name, "intr") == 0) {
			if (kvm_read(kd, (long)evcnt.ev_dev, (void *)&dev,
			    sizeof dev) != sizeof dev)
				errx(1, "event chain trashed: %s", kvm_geterr(kd));
			if (evcnt.ev_count)
				(void)printf("%-16.16s %20llu %8llu\n",
				    dev.dv_xname,
				    evcnt.ev_count, evcnt.ev_count / uptime);
			inttotal += evcnt.ev_count;
		}
		evptr = evcnt.ev_list.tqe_next;
	}
	(void)printf("%-16s %20llu %8llu\n", "Total", inttotal,
	    inttotal / uptime);
}

/*
 * These names are defined in <sys/malloc.h>.
 */
char *kmemnames[] = INITKMEMNAMES;

void
domem(void)
{
	struct kmembuckets *kp;
	struct kmemstats *ks;
	int i, j;
	int len, size, first;
	u_long totuse = 0, totfree = 0;
	quad_t totreq = 0;
	char *name;
	struct kmemstats kmemstats[M_LAST];
	struct kmembuckets buckets[MINBUCKET + 16];
	int mib[4];
	size_t siz;
	char buf[BUFSIZ], *bufp, *ap;

	if (memf == NULL && nlistf == NULL) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_MALLOCSTATS;
		mib[2] = KERN_MALLOC_BUCKETS;
		siz = sizeof(buf);
		if (sysctl(mib, 3, buf, &siz, NULL, 0) < 0) {
			warnx("could not read kern.malloc.buckets");
			return;
		}

		bufp = buf;
		mib[2] = KERN_MALLOC_BUCKET;
		siz = sizeof(struct kmembuckets);
		i = 0;
		while ((ap = strsep(&bufp, ",")) != NULL) {
			mib[3] = atoi(ap);

			if (sysctl(mib, 4, &buckets[MINBUCKET + i], &siz,
			    NULL, 0) < 0) {
				warn("could not read kern.malloc.bucket.%d", mib[3]);
				return;
			}
			i++;
		}
	} else {
		kread(X_KMEMBUCKETS, buckets, sizeof(buckets));
	}

	for (first = 1, i = MINBUCKET, kp = &buckets[i]; i < MINBUCKET + 16;
	     i++, kp++) {
		if (kp->kb_calls == 0 && !verbose)
			continue;
		if (first) {
			(void)printf("Memory statistics by bucket size\n");
			(void)printf(
		"    Size   In Use   Free           Requests  HighWater  Couldfree\n");
			first = 0;
		}
		size = 1 << i;
		(void)printf("%8d %8llu %6llu %18llu %7llu %10llu\n", size,
			(unsigned long long)(kp->kb_total - kp->kb_totalfree),
			(unsigned long long)kp->kb_totalfree,
			(unsigned long long)kp->kb_calls,
			(unsigned long long)kp->kb_highwat,
			(unsigned long long)kp->kb_couldfree);
		totfree += size * kp->kb_totalfree;
	}

	/*
	 * If kmem statistics are not being gathered by the kernel,
	 * first will still be 1.
	 */
	if (first) {
		printf(
		    "Kmem statistics are not being gathered by the kernel.\n");
		return;
	}

	if (memf == NULL && nlistf == NULL) {
		bzero(kmemstats, sizeof(kmemstats));
		for (i = 0; i < M_LAST; i++) {
			mib[0] = CTL_KERN;
			mib[1] = KERN_MALLOCSTATS;
			mib[2] = KERN_MALLOC_KMEMSTATS;
			mib[3] = i;
			siz = sizeof(struct kmemstats);

			/* 
			 * Skip errors -- these are presumed to be unallocated
			 * entries.
			 */
			if (sysctl(mib, 4, &kmemstats[i], &siz, NULL, 0) < 0)
				continue;
		}
	} else {
		kread(X_KMEMSTAT, kmemstats, sizeof(kmemstats));
	}

	(void)printf("\nMemory usage type by bucket size\n");
	(void)printf("    Size  Type(s)\n");
	kp = &buckets[MINBUCKET];
	for (j =  1 << MINBUCKET; j < 1 << (MINBUCKET + 16); j <<= 1, kp++) {
		if (kp->kb_calls == 0)
			continue;
		first = 1;
		len = 8;
		for (i = 0, ks = &kmemstats[0]; i < M_LAST; i++, ks++) {
			if (ks->ks_calls == 0)
				continue;
			if ((ks->ks_size & j) == 0)
				continue;
			name = kmemnames[i] ? kmemnames[i] : "undefined";
			len += 2 + strlen(name);
			if (first)
				printf("%8d  %s", j, name);
			else
				printf(",");
			if (len >= 80) {
				printf("\n\t ");
				len = 10 + strlen(name);
			}
			if (!first)
				printf(" %s", name);
			first = 0;
		}
		printf("\n");
	}

	(void)printf(
	   "\nMemory statistics by type                           Type  Kern\n");
	(void)printf(
"          Type InUse MemUse HighUse  Limit Requests Limit Limit Size(s)\n");
	for (i = 0, ks = &kmemstats[0]; i < M_LAST; i++, ks++) {
		if (ks->ks_calls == 0)
			continue;
		(void)printf("%14s%6ld%6ldK%7ldK%6ldK%9ld%5u%6u",
		    kmemnames[i] ? kmemnames[i] : "undefined",
		    ks->ks_inuse, (ks->ks_memuse + 1023) / 1024,
		    (ks->ks_maxused + 1023) / 1024,
		    (ks->ks_limit + 1023) / 1024, ks->ks_calls,
		    ks->ks_limblocks, ks->ks_mapblocks);
		first = 1;
		for (j =  1 << MINBUCKET; j < 1 << (MINBUCKET + 16); j <<= 1) {
			if ((ks->ks_size & j) == 0)
				continue;
			if (first)
				printf("  %d", j);
			else
				printf(",%d", j);
			first = 0;
		}
		printf("\n");
		totuse += ks->ks_memuse;
		totreq += ks->ks_calls;
	}
	(void)printf("\nMemory Totals:  In Use    Free    Requests\n");
	(void)printf("              %7luK %6luK    %8qu\n",
	     (totuse + 1023) / 1024, (totfree + 1023) / 1024, totreq);
}

static void
print_pool(struct pool *pp, char *name)
{
	static int first = 1;
	int ovflw;
	char maxp[32];

	if (first) {
		(void)printf("Memory resource pool statistics\n");
		(void)printf(
		    "%-11s%5s%9s%5s%9s%6s%6s%6s%6s%6s%6s%5s\n",
		    "Name",
		    "Size",
		    "Requests",
		    "Fail",
		    "Releases",
		    "Pgreq",
		    "Pgrel",
		    "Npage",
		    "Hiwat",
		    "Minpg",
		    "Maxpg",
		    "Idle");
		first = 0;
	}

	/* Skip unused pools unless verbose output. */
	if (pp->pr_nget == 0 && !verbose)
		return;

	if (pp->pr_maxpages == UINT_MAX)
		snprintf(maxp, sizeof maxp, "inf");
	else
		snprintf(maxp, sizeof maxp, "%u", pp->pr_maxpages);
/*
 * Print single word.  `ovflow' is number of characters didn't fit
 * on the last word.  `fmt' is a format string to print this word.
 * It must contain asterisk for field width.  `width' is a width
 * occupied by this word.  `fixed' is a number of constant chars in
 * `fmt'.  `val' is a value to be printed using format string `fmt'.
 */
#define	PRWORD(ovflw, fmt, width, fixed, val) do {	\
	(ovflw) += printf((fmt),			\
	    (width) - (fixed) - (ovflw) > 0 ?		\
	    (width) - (fixed) - (ovflw) : 0,		\
	    (val)) - (width);				\
	if ((ovflw) < 0)				\
		(ovflw) = 0;				\
} while (/* CONSTCOND */0)

	ovflw = 0;
	PRWORD(ovflw, "%-*s", 11, 0, name);
	PRWORD(ovflw, " %*u", 5, 1, pp->pr_size);
	PRWORD(ovflw, " %*lu", 9, 1, pp->pr_nget);
	PRWORD(ovflw, " %*lu", 5, 1, pp->pr_nfail);
	PRWORD(ovflw, " %*lu", 9, 1, pp->pr_nput);
	PRWORD(ovflw, " %*lu", 6, 1, pp->pr_npagealloc);
	PRWORD(ovflw, " %*lu", 6, 1, pp->pr_npagefree);
	PRWORD(ovflw, " %*d", 6, 1, pp->pr_npages);
	PRWORD(ovflw, " %*d", 6, 1, pp->pr_hiwat);
	PRWORD(ovflw, " %*d", 6, 1, pp->pr_minpages);
	PRWORD(ovflw, " %*s", 6, 1, maxp);
	PRWORD(ovflw, " %*lu\n", 5, 1, pp->pr_nidle);	
}

static void dopool_kvm(void);
static void dopool_sysctl(void);

void
dopool(void)
{
	if (nlistf == NULL && memf == NULL)
		dopool_sysctl();
	else
		dopool_kvm();
}

void
dopool_sysctl(void)
{
	long total = 0, inuse = 0;
	struct pool pool;
	size_t size;
	int mib[4];
	int npools, i;

	mib[0] = CTL_KERN;
	mib[1] = KERN_POOL;
	mib[2] = KERN_POOL_NPOOLS;
	size = sizeof(npools);
	if (sysctl(mib, 3, &npools, &size, NULL, 0) < 0) {
		warn("can't figure out number of pools in kernel");
		return;
	}

	for (i = 1; npools; i++) {
		char name[32];

		mib[0] = CTL_KERN;
		mib[1] = KERN_POOL;
		mib[2] = KERN_POOL_POOL;
		mib[3] = i;
		size = sizeof(struct pool);
		if (sysctl(mib, 4, &pool, &size, NULL, 0) < 0) {
			if (errno == ENOENT)
				continue;
			warn("error getting pool");
			return;
		}
		npools--;
		mib[2] = KERN_POOL_NAME;
		size = sizeof(name);
		if (sysctl(mib, 4, &name, &size, NULL, 0) < 0) {
			warn("error getting pool name");
			return;
		}
		print_pool(&pool, name);

		inuse += (pool.pr_nget - pool.pr_nput) * pool.pr_size;
		total += pool.pr_npages * getpagesize();	/* XXX */
	}

	inuse /= 1024;
	total /= 1024;
	printf("\nIn use %ldK, total allocated %ldK; utilization %.1f%%\n",
	    inuse, total, (double)(100 * inuse) / total);
}

void
dopool_kvm(void)
{
	long addr;
	long total = 0, inuse = 0;
	TAILQ_HEAD(,pool) pool_head;
	struct pool pool, *pp = &pool;

	kread(X_POOLHEAD, &pool_head, sizeof(pool_head));
	addr = (long)TAILQ_FIRST(&pool_head);

	while (addr != 0) {
		char name[32];

		if (kvm_read(kd, addr, (void *)pp, sizeof *pp) != sizeof *pp) {
			(void)fprintf(stderr,
			    "vmstat: pool chain trashed: %s\n",
			    kvm_geterr(kd));
			exit(1);
		}
		if (kvm_read(kd, (long)pp->pr_wchan, name, sizeof name) < 0) {
			(void)fprintf(stderr,
			    "vmstat: pool name trashed: %s\n",
			    kvm_geterr(kd));
			exit(1);
		}

		name[31] = '\0';

		print_pool(pp, name);

		inuse += (pp->pr_nget - pp->pr_nput) * pp->pr_size;
		total += pp->pr_npages * getpagesize();	/* XXX */

		addr = (long)TAILQ_NEXT(pp, pr_poollist);
	}

	inuse /= 1024;
	total /= 1024;
	printf("\nIn use %ldK, total allocated %ldK; utilization %.1f%%\n",
	    inuse, total, (double)(100 * inuse) / total);
}

/*
 * kread reads something from the kernel, given its nlist index.
 */
void
kread(int nlx, void *addr, size_t size)
{
	char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "symbol %s not defined", sym);
	}
	if (kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "%s: %s", sym, kvm_geterr(kd));
	}
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-fimstvz] [-c count] [-M core] "
	    "[-N system] [-w wait] [disks]\n", __progname);
	exit(1);
}
