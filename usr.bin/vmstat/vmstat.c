/*	$NetBSD: vmstat.c,v 1.29.4.1 1996/06/05 00:21:05 cgd Exp $	*/
/*	$OpenBSD: vmstat.c,v 1.24 1998/05/19 17:38:20 mickey Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char rcsid[] = "$NetBSD: vmstat.c,v 1.29.4.1 1996/06/05 00:21:05 cgd Exp $";
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
#include <vm/vm.h>
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

struct nlist namelist[] = {
#define	X_CPTIME	0
	{ "_cp_time" },
#define X_SUM		1
	{ "_cnt" },
#define	X_BOOTTIME	2
	{ "_boottime" },
#define X_HZ		3
	{ "_hz" },
#define X_STATHZ	4
	{ "_stathz" },
#define X_NCHSTATS	5
	{ "_nchstats" },
#define	X_INTRNAMES	6
	{ "_intrnames" },
#define	X_EINTRNAMES	7
	{ "_eintrnames" },
#define	X_INTRCNT	8
	{ "_intrcnt" },
#define	X_EINTRCNT	9
	{ "_eintrcnt" },
#define	X_KMEMSTAT	10
	{ "_kmemstats" },
#define	X_KMEMBUCKETS	11
	{ "_bucket" },
#define X_ALLEVENTS	12
	{ "_allevents" },
#define	X_FORKSTAT	13
	{ "_forkstat" },
#ifdef notdef
#define	X_DEFICIT	14
	{ "_deficit" },
#define X_REC		15
	{ "_rectime" },
#define X_PGIN		16
	{ "_pgintime" },
#define	X_XSTATS	17
	{ "_xstats" },
#define X_END		28
#else
#define X_END		14
#endif
#ifdef tahoe
#define	X_VBDINIT	(X_END)
	{ "_vbdinit" },
#define	X_CKEYSTATS	(X_END+1)
	{ "_ckeystats" },
#define	X_DKEYSTATS	(X_END+2)
	{ "_dkeystats" },
#endif
#if defined(pc532)
#define	X_IVT		(X_END)
	{ "_ivt" },
#endif
#if defined(i386)
#define	X_INTRHAND	(X_END)
	{ "_intrhand" },
#define	X_INTRSTRAY	(X_END+1)
	{ "_intrstray" },
#endif
	{ "" },
};

/* Objects defined in dkstats.c */
extern struct _disk	cur;
extern char	**dr_name;
extern int	*dk_select, dk_ndrive;

struct	vmmeter sum, osum;
int		ndrives;

int	winlines = 20;

kvm_t *kd;

#define	FORKSTAT	0x01
#define	INTRSTAT	0x02
#define	MEMSTAT		0x04
#define	SUMSTAT		0x08
#define	TIMESTAT	0x10
#define	VMSTAT		0x20

void	cpustats __P((void));
void	dkstats __P((void));
void	dointr __P((void));
void	domem __P((void));
void	dosum __P((void));
void	dovmstat __P((u_int, int));
void	kread __P((int, void *, size_t));
void	usage __P((void));
void	dotimes __P((void));
void	doforkst __P((void));
void	printhdr __P((void));

char	**choosedrives __P((char **));

/* Namelist and memory file names. */
char	*nlistf, *memf;

int
main(argc, argv)
	register int argc;
	register char **argv;
{
	extern int optind;
	extern char *optarg;
	register int c, todo;
	u_int interval;
	int reps;
        char errbuf[_POSIX2_LINE_MAX];

	memf = nlistf = NULL;
	interval = reps = todo = 0;
	while ((c = getopt(argc, argv, "c:fiM:mN:stw:")) != -1) {
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
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (todo == 0)
		todo = VMSTAT;

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

        kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
	if (kd == 0) {
		(void)fprintf(stderr,
		    "vmstat: kvm_openfiles: %s\n", errbuf);
		exit(1);
	}

	if ((c = kvm_nlist(kd, namelist)) != 0) {
		if (c > 0) {
			(void)fprintf(stderr,
			    "vmstat: undefined symbols:");
			for (c = 0;
			    c < sizeof(namelist)/sizeof(namelist[0]); c++)
				if (namelist[c].n_type == 0)
					fprintf(stderr, " %s",
					    namelist[c].n_name);
			(void)fputc('\n', stderr);
		} else
			(void)fprintf(stderr, "vmstat: kvm_nlist: %s\n",
			    kvm_geterr(kd));
		exit(1);
	}

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
	if (todo & MEMSTAT)
		domem();
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
choosedrives(argv)
	char **argv;
{
	register int i;

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
	for (i = 0; i < dk_ndrive && ndrives < 4; i++) {
		if (dk_select[i])
			continue;
		dk_select[i] = 1;
		++ndrives;
	}
	return(argv);
}

time_t
getuptime()
{
	static time_t now;
	static struct timeval boottime;
	time_t uptime;

	if (boottime.tv_sec == 0)
		kread(X_BOOTTIME, &boottime, sizeof(boottime));
	(void)time(&now);
	uptime = now - boottime.tv_sec;
	if (uptime <= 0 || uptime > 60*60*24*365*10) {
		(void)fprintf(stderr,
		    "vmstat: time makes no sense; namelist must be wrong.\n");
		exit(1);
	}
	return(uptime);
}

int	hz, hdrcnt;

void
dovmstat(interval, reps)
	u_int interval;
	int reps;
{
	struct vmtotal total;
	time_t uptime, halfuptime;
	void needhdr();
	int mib[2];
	size_t size;

	uptime = getuptime();
	halfuptime = uptime / 2;
	(void)signal(SIGCONT, needhdr);

	if (namelist[X_STATHZ].n_type != 0 && namelist[X_STATHZ].n_value != 0)
		kread(X_STATHZ, &hz, sizeof(hz));
	if (!hz)
		kread(X_HZ, &hz, sizeof(hz));

	for (hdrcnt = 1;;) {
		if (!--hdrcnt)
			printhdr();
		/* Read new disk statistics */
		dkreadstats();
		kread(X_SUM, &sum, sizeof(sum));
		size = sizeof(total);
		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		if (sysctl(mib, 2, &total, &size, NULL, 0) < 0) {
			printf("Can't get kerninfo: %s\n", strerror(errno));
			bzero(&total, sizeof(total));
		}
		(void)printf("%2u%2u%2u",
		    total.t_rq - 1, total.t_dw + total.t_pw, total.t_sw);
#define pgtok(a) ((a) * ((int)sum.v_page_size >> 10))
#define	rate(x)	(((x) + halfuptime) / uptime)	/* round */
		(void)printf("%6u%6u ",
		    pgtok(total.t_avm), pgtok(total.t_free));
		(void)printf("%4u ", rate(sum.v_faults - osum.v_faults));
		(void)printf("%3u ",
		    rate(sum.v_reactivated - osum.v_reactivated));
		(void)printf("%3u ", rate(sum.v_pageins - osum.v_pageins));
		(void)printf("%3u %3u ",
		    rate(sum.v_pageouts - osum.v_pageouts), 0);
		(void)printf("%3u ", rate(sum.v_scan - osum.v_scan));
		dkstats();
		(void)printf("%4u %4u %3u ",
		    rate(sum.v_intr - osum.v_intr),
		    rate(sum.v_syscall - osum.v_syscall),
		    rate(sum.v_swtch - osum.v_swtch));
		cpustats();
		(void)printf("\n");
		(void)fflush(stdout);
		if (reps >= 0 && --reps <= 0)
			break;
		osum = sum;
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
printhdr()
{
	register int i;

	(void)printf(" procs   memory     page%*s", 20, "");
	if (ndrives > 0)
		(void)printf("%s %*sfaults   cpu\n",
		   ((ndrives > 1) ? "disks" : "disk"),
		   ((ndrives > 1) ? ndrives * 3 - 4 : 0), "");
	else
		(void)printf("%*s  faults   cpu\n",
		   ndrives * 3, "");

	(void)printf(" r b w   avm   fre  flt  re  pi  po  fr  sr ");
	for (i = 0; i < dk_ndrive; i++)
		if (dk_select[i])
			(void)printf("%c%c ", dr_name[i][0],
			    dr_name[i][strlen(dr_name[i]) - 1]);
	(void)printf("  in   sy  cs us sy id\n");
	hdrcnt = winlines - 2;
}

/*
 * Force a header to be prepended to the next output.
 */
void
needhdr()
{

	hdrcnt = 1;
}

void
dotimes()
{
	u_int pgintime, rectime;

	pgintime = 0;
	rectime = 0;
	kread(X_SUM, &sum, sizeof(sum));
	(void)printf("%u reactivates, %u total time (usec)\n",
	    sum.v_reactivated, rectime);
	(void)printf("average: %u usec / reclaim\n", rectime / sum.v_reactivated);
	(void)printf("\n");
	(void)printf("%u page ins, %u total time (msec)\n",
	    sum.v_pageins, pgintime / 10);
	(void)printf("average: %8.1f msec / page in\n",
	    pgintime / (sum.v_pageins * 10.0));
}

int
pct(top, bot)
	long top, bot;
{
	long ans;

	if (bot == 0)
		return(0);
	ans = (quad_t)top * 100 / bot;
	return (ans);
}

#define	PCT(top, bot) pct((long)(top), (long)(bot))

#if defined(tahoe)
#include <machine/cpu.h>
#endif

void
dosum()
{
	struct nchstats nchstats;
	long nchtotal;
#if defined(tahoe)
	struct keystats keystats;
#endif

	kread(X_SUM, &sum, sizeof(sum));
	(void)printf("%9u cpu context switches\n", sum.v_swtch);
	(void)printf("%9u device interrupts\n", sum.v_intr);
	(void)printf("%9u software interrupts\n", sum.v_soft);
	(void)printf("%9u traps\n", sum.v_trap);
	(void)printf("%9u system calls\n", sum.v_syscall);
	(void)printf("%9u total faults taken\n", sum.v_faults);
	(void)printf("%9u swap ins\n", sum.v_swpin);
	(void)printf("%9u swap outs\n", sum.v_swpout);
	(void)printf("%9u pages swapped in\n", sum.v_pswpin / CLSIZE);
	(void)printf("%9u pages swapped out\n", sum.v_pswpout / CLSIZE);
	(void)printf("%9u page ins\n", sum.v_pageins);
	(void)printf("%9u page outs\n", sum.v_pageouts);
	(void)printf("%9u pages paged in\n", sum.v_pgpgin);
	(void)printf("%9u pages paged out\n", sum.v_pgpgout);
	(void)printf("%9u pages reactivated\n", sum.v_reactivated);
	(void)printf("%9u intransit blocking page faults\n", sum.v_intrans);
	(void)printf("%9u zero fill pages created\n", sum.v_nzfod / CLSIZE);
	(void)printf("%9u zero fill page faults\n", sum.v_zfod / CLSIZE);
	(void)printf("%9u pages examined by the clock daemon\n", sum.v_scan);
	(void)printf("%9u revolutions of the clock hand\n", sum.v_rev);
	(void)printf("%9u VM object cache lookups\n", sum.v_lookups);
	(void)printf("%9u VM object hits\n", sum.v_hits);
	(void)printf("%9u total VM faults taken\n", sum.v_vm_faults);
	(void)printf("%9u copy-on-write faults\n", sum.v_cow_faults);
	(void)printf("%9u pages freed by daemon\n", sum.v_dfree);
	(void)printf("%9u pages freed by exiting processes\n", sum.v_pfree);
	(void)printf("%9u pages free\n", sum.v_free_count);
	(void)printf("%9u pages wired down\n", sum.v_wire_count);
	(void)printf("%9u pages active\n", sum.v_active_count);
	(void)printf("%9u pages inactive\n", sum.v_inactive_count);
	(void)printf("%9u bytes per page\n", sum.v_page_size);
	kread(X_NCHSTATS, &nchstats, sizeof(nchstats));
	nchtotal = nchstats.ncs_goodhits + nchstats.ncs_neghits +
	    nchstats.ncs_badhits + nchstats.ncs_falsehits +
	    nchstats.ncs_miss + nchstats.ncs_long;
	(void)printf("%9ld total name lookups\n", nchtotal);
	(void)printf(
	    "%9s cache hits (%d%% pos + %d%% neg) system %d%% per-directory\n",
	    "", PCT(nchstats.ncs_goodhits, nchtotal),
	    PCT(nchstats.ncs_neghits, nchtotal),
	    PCT(nchstats.ncs_pass2, nchtotal));
	(void)printf("%9s deletions %d%%, falsehits %d%%, toolong %d%%\n", "",
	    PCT(nchstats.ncs_badhits, nchtotal),
	    PCT(nchstats.ncs_falsehits, nchtotal),
	    PCT(nchstats.ncs_long, nchtotal));
#if defined(tahoe)
	kread(X_CKEYSTATS, &keystats, sizeof(keystats));
	(void)printf("%9d %s (free %d%% norefs %d%% taken %d%% shared %d%%)\n",
	    keystats.ks_allocs, "code cache keys allocated",
	    PCT(keystats.ks_allocfree, keystats.ks_allocs),
	    PCT(keystats.ks_norefs, keystats.ks_allocs),
	    PCT(keystats.ks_taken, keystats.ks_allocs),
	    PCT(keystats.ks_shared, keystats.ks_allocs));
	kread(X_DKEYSTATS, &keystats, sizeof(keystats));
	(void)printf("%9d %s (free %d%% norefs %d%% taken %d%% shared %d%%)\n",
	    keystats.ks_allocs, "data cache keys allocated",
	    PCT(keystats.ks_allocfree, keystats.ks_allocs),
	    PCT(keystats.ks_norefs, keystats.ks_allocs),
	    PCT(keystats.ks_taken, keystats.ks_allocs),
	    PCT(keystats.ks_shared, keystats.ks_allocs));
#endif
}

void
doforkst()
{
	struct forkstat fks;

	kread(X_FORKSTAT, &fks, sizeof(struct forkstat));
	(void)printf("%d forks, %d pages, average %.2f\n",
	    fks.cntfork, fks.sizfork, (double)fks.sizfork / fks.cntfork);
	(void)printf("%d vforks, %d pages, average %.2f\n",
	    fks.cntvfork, fks.sizvfork, (double)fks.sizvfork / (fks.cntvfork ? fks.cntvfork : 1));
	(void)printf("%d rforks, %d pages, average %.2f\n",
	    fks.cntrfork, fks.sizrfork, (double)fks.sizrfork / (fks.cntrfork ? fks.cntrfork : 1));
}

void
dkstats()
{
	register int dn, state;
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
	for (dn = 0; dn < dk_ndrive; ++dn) {
		if (!dk_select[dn])
			continue;
		(void)printf("%2.0f ", cur.dk_xfer[dn] / etime);
	}
}

void
cpustats()
{
	register int state;
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

#if defined(pc532)
/* To get struct iv ...*/
#define _KERNEL
#include <machine/psl.h>
#undef _KERNEL
void
dointr()
{
	register long i, j, inttotal;
	time_t uptime;
	static char iname[64];
	struct iv ivt[32], *ivp = ivt;

	iname[63] = '\0';
	uptime = getuptime();
	kread(X_IVT, ivp, sizeof(ivt));

	for (i = 0; i < 2; i++) {
		(void)printf("%sware interrupts:\n", i ? "\nsoft" : "hard");
		(void)printf("interrupt       total     rate\n");
		inttotal = 0;
		for (j = 0; j < 16; j++, ivp++) {
			if (ivp->iv_vec && ivp->iv_use && ivp->iv_cnt) {
				if (kvm_read(kd, (u_long)ivp->iv_use, iname, 63) != 63) {
					(void)fprintf(stderr, "vmstat: iv_use: %s\n",
					    kvm_geterr(kd));
					exit(1);
				}
				(void)printf("%-12s %8ld %8ld\n", iname,
				    ivp->iv_cnt, ivp->iv_cnt / uptime);
				inttotal += ivp->iv_cnt;
			}
		}
		(void)printf("Total        %8ld %8ld\n",
		    inttotal, inttotal / uptime);
	}
}
#elif defined(i386)
/* To get struct intrhand */
#define _KERNEL
#include <machine/psl.h>
#undef _KERNEL
void
dointr()
{
	struct intrhand *intrhand[16], *ihp, ih;
	long inttotal;
	time_t uptime;
	int intrstray[16];
	char iname[17];
	int i;

	iname[16] = '\0';
	uptime = getuptime();
	kread(X_INTRHAND, intrhand, sizeof(intrhand));
	kread(X_INTRSTRAY, intrstray, sizeof(intrstray));

	(void)printf("interrupt           total     rate\n");
	inttotal = 0;
	for (i = 0; i < 16; i++) {
		ihp = intrhand[i];
		while (ihp) {
			if (kvm_read(kd, (u_long)ihp, &ih, sizeof(ih)) != sizeof(ih))
				errx(1, "vmstat: ih: %s", kvm_geterr(kd));
			if (kvm_read(kd, (u_long)ih.ih_what, iname, 16) != 16)
				errx(1, "vmstat: ih_what: %s", kvm_geterr(kd));
			printf("%-16.16s %8ld %8ld\n", iname, ih.ih_count, ih.ih_count / uptime);
			inttotal += ih.ih_count;
			ihp = ih.ih_next;
		}
	}
	for (i = 0; i < 16; i++)
		if (intrstray[i]) {
			printf("Stray irq %-2d     %8d %8d\n",
			       i, intrstray[i], intrstray[i] / uptime);
			inttotal += intrstray[i];
		}
	printf("Total            %8ld %8ld\n", inttotal, inttotal / uptime);
}
#else
void
dointr()
{
	register long *intrcnt, inttotal;
	time_t uptime;
	register int nintr, inamlen;
	register char *intrname;
	struct evcntlist allevents;
	struct evcnt evcnt, *evptr;
	struct device dev;

	uptime = getuptime();
	nintr = namelist[X_EINTRCNT].n_value - namelist[X_INTRCNT].n_value;
	inamlen =
	    namelist[X_EINTRNAMES].n_value - namelist[X_INTRNAMES].n_value;
	intrcnt = malloc((size_t)nintr);
	intrname = malloc((size_t)inamlen);
	if (intrcnt == NULL || intrname == NULL) {
		(void)fprintf(stderr, "vmstat: %s.\n", strerror(errno));
		exit(1);
	}
	kread(X_INTRCNT, intrcnt, (size_t)nintr);
	kread(X_INTRNAMES, intrname, (size_t)inamlen);
	(void)printf("interrupt         total     rate\n");
	inttotal = 0;
	nintr /= sizeof(long);
	while (--nintr >= 0) {
		if (*intrcnt)
			(void)printf("%-14s %8ld %8ld\n", intrname,
			    *intrcnt, *intrcnt / uptime);
		intrname += strlen(intrname) + 1;
		inttotal += *intrcnt++;
	}
	kread(X_ALLEVENTS, &allevents, sizeof allevents);
	evptr = allevents.tqh_first;
	while (evptr) {
		if (kvm_read(kd, (long)evptr, (void *)&evcnt,
		    sizeof evcnt) != sizeof evcnt) {
			(void)fprintf(stderr, "vmstat: event chain trashed\n",
			    kvm_geterr(kd));
			exit(1);
		}
		if (strcmp(evcnt.ev_name, "intr") == 0) {
			if (kvm_read(kd, (long)evcnt.ev_dev, (void *)&dev,
			    sizeof dev) != sizeof dev) {
				(void)fprintf(stderr, "vmstat: event chain trashed\n",
				    kvm_geterr(kd));
				exit(1);
			}
			if (evcnt.ev_count)
				(void)printf("%-14s %8ld %8ld\n", dev.dv_xname,
				    evcnt.ev_count, evcnt.ev_count / uptime);
			inttotal += evcnt.ev_count++;
		}
		evptr = evcnt.ev_list.tqe_next;
	}
	(void)printf("Total          %8ld %8ld\n", inttotal, inttotal / uptime);
}
#endif

/*
 * These names are defined in <sys/malloc.h>.
 */
char *kmemnames[] = INITKMEMNAMES;

void
domem()
{
	register struct kmembuckets *kp;
	register struct kmemstats *ks;
	register int i, j;
	int len, size, first;
	long totuse = 0, totfree = 0, totreq = 0;
	char *name;
	struct kmemstats kmemstats[M_LAST];
	struct kmembuckets buckets[MINBUCKET + 16];

	kread(X_KMEMBUCKETS, buckets, sizeof(buckets));
	for (first = 1, i = MINBUCKET, kp = &buckets[i]; i < MINBUCKET + 16;
	     i++, kp++) {
		if (kp->kb_calls == 0)
			continue;
		if (first) {
			(void)printf("Memory statistics by bucket size\n");
			(void)printf(
	    	"    Size   In Use   Free   Requests  HighWater  Couldfree\n");
			first = 0;
		}
		size = 1 << i;
		(void)printf("%8d %8ld %6ld %10ld %7ld %10ld\n", size, 
			kp->kb_total - kp->kb_totalfree,
			kp->kb_totalfree, kp->kb_calls,
			kp->kb_highwat, kp->kb_couldfree);
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

	kread(X_KMEMSTAT, kmemstats, sizeof(kmemstats));
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
	   "\nMemory statistics by type                          Type  Kern\n");
	(void)printf(
"         Type InUse MemUse HighUse  Limit Requests Limit Limit Size(s)\n");
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
	(void)printf("              %7ldK %6ldK    %8ld\n",
	     (totuse + 1023) / 1024, (totfree + 1023) / 1024, totreq);
}

/*
 * kread reads something from the kernel, given its nlist index.
 */
void
kread(nlx, addr, size)
	int nlx;
	void *addr;
	size_t size;
{
	char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr,
		    "vmstat: symbol %s not defined\n", sym);
		exit(1);
	}
	if (kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr, "vmstat: %s: %s\n", sym, kvm_geterr(kd));
		exit(1);
	}
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: vmstat [-fimst] [-c count] [-M core] \
[-N system] [-w wait] [disks]\n");
	exit(1);
}

