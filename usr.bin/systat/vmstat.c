/*	$OpenBSD: vmstat.c,v 1.50 2004/06/11 06:31:32 deraadt Exp $	*/
/*	$NetBSD: vmstat.c,v 1.5 1996/05/10 23:16:40 thorpej Exp $	*/

/*-
 * Copyright (c) 1983, 1989, 1992, 1993
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
#if 0
static char sccsid[] = "@(#)vmstat.c	8.2 (Berkeley) 1/12/94";
#endif
static char rcsid[] = "$OpenBSD: vmstat.c,v 1.50 2004/06/11 06:31:32 deraadt Exp $";
#endif /* not lint */

/*
 * Cursed vmstat -- from Robert Elz.
 */

#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/sysctl.h>

#include <uvm/uvm_extern.h>

#include <ctype.h>
#include <err.h>
#include <nlist.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <utmp.h>
#include <unistd.h>

#if defined(__i386__)
#define	_KERNEL
#include <machine/psl.h>
#undef _KERNEL
#endif

#include "systat.h"
#include "extern.h"

static struct Info {
	long	time[CPUSTATES];
	struct	uvmexp uvmexp;
	struct	vmtotal Total;
	struct	nchstats nchstats;
	long	nchcount;
	int	*intrcnt;
} s, s1, s2, z;

#include "dkstats.h"
extern struct _disk	cur;

#define	cnt s.Cnt
#define oldcnt s1.Cnt
#define	total s.Total
#define	nchtotal s.nchstats
#define	oldnchtotal s1.nchstats

static	enum state { BOOT, TIME, RUN } state = TIME;

static void allocinfo(struct Info *);
static void copyinfo(struct Info *, struct Info *);
static float cputime(int);
static void dinfo(int, int);
static void getinfo(struct Info *, enum state);
static void putint(int, int, int, int);
static void putfloat(double, int, int, int, int, int);
static int ucount(void);

static	int ut;
static	char buf[26];
static	time_t t;
static	double etime;
static	float hertz;
static	int nintr;
static	long *intrloc;
static	char **intrname;
static	int nextintsrow;

struct	utmp utmp;

WINDOW *
openkre(void)
{

	ut = open(_PATH_UTMP, O_RDONLY);
	if (ut < 0)
		error("No utmp");
	return (stdscr);
}

void
closekre(WINDOW *w)
{

	(void) close(ut);
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
}


static struct nlist namelist[] = {
#define	X_INTRNAMES	0		/* no sysctl */
	{ "_intrnames" },
#define	X_EINTRNAMES	1		/* no sysctl */
	{ "_eintrnames" },
#define	X_INTRCNT	2		/* no sysctl */
	{ "_intrcnt" },
#define	X_EINTRCNT	3		/* no sysctl */
	{ "_eintrcnt" },
#if defined(__i386__)
#define	X_INTRHAND	4		/* no sysctl */
	{ "_intrhand" },
#define	X_APICINTRHAND	5		/* no sysctl */
	{ "_apic_intrhand" },
#endif
	{ "" },
};

/*
 * These constants define where the major pieces are laid out
 */
#define STATROW		 0	/* uses 1 row and 68 cols */
#define STATCOL		 2
#define MEMROW		 2	/* uses 4 rows and 31 cols */
#define MEMCOL		 0
#define PAGEROW		 2	/* uses 4 rows and 26 cols */
#define PAGECOL		37
#define INTSROW		 2	/* uses all rows to bottom and 17 cols */
#define INTSCOL		63
#define PROCSROW	 7	/* uses 2 rows and 20 cols */
#define PROCSCOL	 0
#define GENSTATROW	 7	/* uses 2 rows and 35 cols */
#define GENSTATCOL	16
#define VMSTATROW	 7	/* uses 17 rows and 12 cols */
#define VMSTATCOL	48
#define GRAPHROW	10	/* uses 3 rows and 51 cols */
#define GRAPHCOL	 0
#define NAMEIROW	14	/* uses 3 rows and 49 cols */
#define NAMEICOL	 0
#define DISKROW		18	/* uses 5 rows and 50 cols (for 9 drives) */
#define DISKCOL		 0

#define	DRIVESPACE	45	/* max space for drives */

int ncpu = 1;

int
initkre(void)
{
	char *intrnamebuf, *cp;
	size_t size = sizeof(ncpu);
	int mib[2], i, ret;

	if (namelist[0].n_type == 0) {
		if ((ret = kvm_nlist(kd, namelist)) == -1)
			errx(1, "%s", kvm_geterr(kd));
#if defined(__i386__)
		else if (ret == 1 && namelist[X_APICINTRHAND].n_value != 0)
			nlisterr(namelist);
#endif
		else if (ret > 1)
			nlisterr(namelist);
		if (namelist[0].n_type == 0) {
			error("No namelist");
			return(0);
		}
	}

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if (sysctl(mib, 2, &ncpu, &size, NULL, 0) == -1)
                return (-1);

	hertz = stathz ? stathz : hz;
	if (!dkinit(1))
		return(0);
	if (nintr == 0) {
#if defined(__i386__)
		struct intrhand *intrhand[16], *ihp, ih;
		struct intrhand *apicintrhand[256];
		char iname[16];
		int namelen, n;

		NREAD(X_INTRHAND, intrhand, sizeof(intrhand));
		for (namelen = 0, i = 0; i < 16; i++) {
			ihp = intrhand[i];
			while (ihp) {
				nintr++;
				KREAD(ihp, &ih, sizeof(ih));
				KREAD(ih.ih_what, iname, 16);
				namelen += strlen(iname) + 1;
				ihp = ih.ih_next;
			}
		}
		if (namelist[X_APICINTRHAND].n_value) {
			NREAD(X_APICINTRHAND, apicintrhand, sizeof(apicintrhand));
			for (namelen = 0, i = 0; i < 256; i++) {
				ihp = apicintrhand[i];
				while (ihp) {
					nintr++;
					KREAD(ihp, &ih, sizeof(ih));
					KREAD(ih.ih_what, iname, 16);
					namelen += strlen(iname) + 1;
					printf("apic handler %x %s\n", i, iname);
					ihp = ih.ih_next;
				}
			}
		}		
		intrloc = calloc(nintr, sizeof (long));
		intrname = calloc(nintr, sizeof (char *));
		cp = intrnamebuf = malloc(namelen);
		for (i = 0, n = 0; i < 16; i++) {
			ihp = intrhand[i];
			while (ihp) {
				KREAD(ihp, &ih, sizeof(ih));
				KREAD(ih.ih_what, iname, 16);
				intrname[n++] = cp;
				strlcpy(cp, iname, intrnamebuf + namelen - cp);
				cp += strlen(iname) + 1;
				ihp = ih.ih_next;
			}
		}
		if (namelist[X_APICINTRHAND].n_value) {
			for (i = 0, n = 0; i < 256; i++) {
				ihp = apicintrhand[i];
				while (ihp) {
					KREAD(ihp, &ih, sizeof(ih));
					KREAD(ih.ih_what, iname, 16);
					intrname[n++] = cp;
					strlcpy(cp, iname, intrnamebuf + namelen - cp);
					cp += strlen(iname) + 1;
					ihp = ih.ih_next;
				}
			}
		}
#else
		nintr = (namelist[X_EINTRCNT].n_value -
		    namelist[X_INTRCNT].n_value) / sizeof (int);
		intrloc = calloc(nintr, sizeof (long));
		intrname = calloc(nintr, sizeof (long));
		intrnamebuf = malloc(namelist[X_EINTRNAMES].n_value -
		    namelist[X_INTRNAMES].n_value);
		if (intrnamebuf == 0 || intrname == 0 || intrloc == 0) {
			error("Out of memory\n");
			if (intrnamebuf)
				free(intrnamebuf);
			if (intrname)
				free(intrname);
			if (intrloc)
				free(intrloc);
			nintr = 0;
			return(0);
		}
		NREAD(X_INTRNAMES, intrnamebuf, NVAL(X_EINTRNAMES) -
		    NVAL(X_INTRNAMES));
		for (cp = intrnamebuf, i = 0; i < nintr; i++) {
			intrname[i] = cp;
			cp += strlen(cp) + 1;
		}
#endif
		nextintsrow = INTSROW + 2;
		allocinfo(&s);
		allocinfo(&s1);
		allocinfo(&s2);
		allocinfo(&z);
	}
	getinfo(&s2, RUN);
	copyinfo(&s2, &s1);
	return(1);
}

void
fetchkre(void)
{
	time_t now;

	time(&now);
	strlcpy(buf, ctime(&now), sizeof buf);
	getinfo(&s, state);
}

void
labelkre(void)
{
	int i, j, l;

	clear();
	mvprintw(STATROW, STATCOL + 4, "users    Load");
	mvprintw(MEMROW, MEMCOL,     "          memory totals (in KB)");
	mvprintw(MEMROW + 1, MEMCOL, "         real   virtual    free");
	mvprintw(MEMROW + 2, MEMCOL, "Active");
	mvprintw(MEMROW + 3, MEMCOL, "All");

	mvprintw(PAGEROW, PAGECOL, "        PAGING   SWAPPING ");
	mvprintw(PAGEROW + 1, PAGECOL, "        in  out   in  out ");
	mvprintw(PAGEROW + 2, PAGECOL, "ops");
	mvprintw(PAGEROW + 3, PAGECOL, "pages");

	mvprintw(INTSROW, INTSCOL + 3, " Interrupts");
	mvprintw(INTSROW + 1, INTSCOL + 9, "total");

	mvprintw(VMSTATROW + 0, VMSTATCOL + 10, "forks");
	mvprintw(VMSTATROW + 1, VMSTATCOL + 10, "fkppw");
	mvprintw(VMSTATROW + 2, VMSTATCOL + 10, "fksvm");
	mvprintw(VMSTATROW + 3, VMSTATCOL + 10, "pwait");
	mvprintw(VMSTATROW + 4, VMSTATCOL + 10, "relck");
	mvprintw(VMSTATROW + 5, VMSTATCOL + 10, "rlkok");
	mvprintw(VMSTATROW + 6, VMSTATCOL + 10, "noram");
	mvprintw(VMSTATROW + 7, VMSTATCOL + 10, "ndcpy");
	mvprintw(VMSTATROW + 8, VMSTATCOL + 10, "fltcp");
	mvprintw(VMSTATROW + 9, VMSTATCOL + 10, "zfod");
	mvprintw(VMSTATROW + 10, VMSTATCOL + 10, "cow");
	mvprintw(VMSTATROW + 11, VMSTATCOL + 10, "fmin");
	mvprintw(VMSTATROW + 12, VMSTATCOL + 10, "ftarg");
	mvprintw(VMSTATROW + 13, VMSTATCOL + 10, "itarg");
	mvprintw(VMSTATROW + 14, VMSTATCOL + 10, "wired");
	mvprintw(VMSTATROW + 15, VMSTATCOL + 10, "pdfre");
	if (LINES - 1 > VMSTATROW + 16)
		mvprintw(VMSTATROW + 16, VMSTATCOL + 10, "pdscn");
	if (LINES - 1 > VMSTATROW + 17)
		mvprintw(VMSTATROW + 17, VMSTATCOL + 10, "pzidle");

	mvprintw(GENSTATROW, GENSTATCOL, "   Csw   Trp   Sys   Int   Sof  Flt");

	mvprintw(GRAPHROW, GRAPHCOL,
	    "    . %% Sys    . %% User    . %% Nice    . %% Idle");
	mvprintw(PROCSROW, PROCSCOL, "Proc:r  d  s  w");
	mvprintw(GRAPHROW + 1, GRAPHCOL,
	    "|    |    |    |    |    |    |    |    |    |    |");

	mvprintw(NAMEIROW, NAMEICOL,
	    "Namei         Sys-cache    Proc-cache    No-cache");
	mvprintw(NAMEIROW + 1, NAMEICOL,
	    "    Calls     hits    %%    hits     %%    miss   %%");
	mvprintw(DISKROW, DISKCOL, "Disks");
	mvprintw(DISKROW + 1, DISKCOL, "seeks");
	mvprintw(DISKROW + 2, DISKCOL, "xfers");
	mvprintw(DISKROW + 3, DISKCOL, "Kbyte");
	mvprintw(DISKROW + 4, DISKCOL, "  sec");
	for (i = 0, j = 0; i < cur.dk_ndrive && j < DRIVESPACE; i++)
		if (cur.dk_select[i] && (j + strlen(dr_name[i])) < DRIVESPACE) {
			l = MAX(5, strlen(dr_name[i]));
			mvprintw(DISKROW, DISKCOL + 5 + j,
			    " %*s", l, dr_name[i]);
			j += 1 + l;
		}
	for (i = 0; i < nintr; i++) {
		if (intrloc[i] == 0)
			continue;
		mvprintw(intrloc[i], INTSCOL + 9, "%-8.8s", intrname[i]);
	}
}

#define X(fld)	{t=s.fld[i]; s.fld[i]-=s1.fld[i]; if (state==TIME) s1.fld[i]=t;}
#define Y(fld)	{t = s.fld; s.fld -= s1.fld; if (state == TIME) s1.fld = t;}
#define Z(fld)	{t = s.nchstats.fld; s.nchstats.fld -= s1.nchstats.fld; \
	if (state == TIME) s1.nchstats.fld = t;}
#define PUTRATE(fld, l, c, w) \
	Y(fld); \
	putint((int)((float)s.fld/etime + 0.5), l, c, w)
#define MAXFAIL 5

static	char cpuchar[CPUSTATES] = { '=' , '>', '-', ' ' };
static	char cpuorder[CPUSTATES] = { CP_SYS, CP_USER, CP_NICE, CP_IDLE };

void
showkre(void)
{
	float f1, f2;
	int psiz, inttotal;
	int i, l, c;
	static int failcnt = 0, first_run = 0;

	if (state == TIME) {
		dkswap();
		if (!first_run) {
			first_run = 1;
			return;
		}
	}
	etime = 0;
	for (i = 0; i < CPUSTATES; i++) {
		X(time);
		etime += s.time[i];
	}
	if (etime < 5.0) {	/* < 5 ticks - ignore this trash */
		if (failcnt++ >= MAXFAIL) {
			clear();
			mvprintw(2, 10, "The alternate system clock has died!");
			mvprintw(3, 10, "Reverting to ``pigs'' display.");
			move(CMDLINE, 0);
			refresh();
			failcnt = 0;
			sleep(5);
			command("pigs");
		}
		return;
	}
	failcnt = 0;
	etime /= hertz;
	etime /= ncpu;
	inttotal = 0;
	for (i = 0; i < nintr; i++) {
		if (s.intrcnt[i] == 0)
			continue;
		if (intrloc[i] == 0) {
			if (nextintsrow == LINES)
				continue;
			intrloc[i] = nextintsrow++;
			mvprintw(intrloc[i], INTSCOL + 9, "%-8.8s",
			    intrname[i]);
		}
		X(intrcnt);
		l = (int)((float)s.intrcnt[i]/etime + 0.5);
		inttotal += l;
		putint(l, intrloc[i], INTSCOL, 8);
	}
	putint(inttotal, INTSROW + 1, INTSCOL, 8);
	Z(ncs_goodhits); Z(ncs_badhits); Z(ncs_miss);
	Z(ncs_long); Z(ncs_pass2); Z(ncs_2passes);
	s.nchcount = nchtotal.ncs_goodhits + nchtotal.ncs_badhits +
	    nchtotal.ncs_miss + nchtotal.ncs_long;
	if (state == TIME)
		s1.nchcount = s.nchcount;

	psiz = 0;
	f2 = 0.0;

	/*
	 * Last CPU state not calculated yet.
	 */
	for (c = 0; c < CPUSTATES - 1; c++) {
		i = cpuorder[c];
		f1 = cputime(i);
		f2 += f1;
		l = (int) ((f2 + 1.0) / 2.0) - psiz;
		if (c == 0)
			putfloat(f1, GRAPHROW, GRAPHCOL + 1, 5, 1, 0);
		else
			putfloat(f1, GRAPHROW, GRAPHCOL + 12 * c,
			    5, 1, 0);
		move(GRAPHROW + 2, psiz);
		psiz += l;
		while (l-- > 0)
			addch(cpuchar[c]);
	}

	/*
	 * The above code does not account for time in the CP_INTR state.
	 * Thus the total may be less than 100%.  If the total is less than
	 * the previous total old data may be left on the graph.  The graph
	 * assumes one character position for every 2 percentage points for
	 * a total of 50 positions.  Ensure all positions have been filled.
	 */
	while ( psiz++ <= 50 )
		addch(' ');

	putint(ucount(), STATROW, STATCOL, 3);
	putfloat(avenrun[0], STATROW, STATCOL + 17, 6, 2, 0);
	putfloat(avenrun[1], STATROW, STATCOL + 23, 6, 2, 0);
	putfloat(avenrun[2], STATROW, STATCOL + 29, 6, 2, 0);
	mvaddstr(STATROW, STATCOL + 53, buf);
#define pgtokb(pg)	((pg) * (s.uvmexp.pagesize / 1024))

	putint(pgtokb(s.uvmexp.active), MEMROW + 2, MEMCOL + 6, 7);
	putint(pgtokb(s.uvmexp.active + s.uvmexp.swpginuse),    /* XXX */
	    MEMROW + 2, MEMCOL + 16, 7);
	putint(pgtokb(s.uvmexp.npages - s.uvmexp.free), MEMROW + 3, MEMCOL + 6, 7);
	putint(pgtokb(s.uvmexp.npages - s.uvmexp.free + s.uvmexp.swpginuse),
	    MEMROW + 3, MEMCOL + 16, 7);
	putint(pgtokb(s.uvmexp.free), MEMROW + 2, MEMCOL + 24, 7);
	putint(pgtokb(s.uvmexp.free + s.uvmexp.swpages - s.uvmexp.swpginuse),
	    MEMROW + 3, MEMCOL + 24, 7);
	putint(total.t_rq - 1, PROCSROW + 1, PROCSCOL + 3, 3);

	putint(total.t_dw, PROCSROW + 1, PROCSCOL + 6, 3);
	putint(total.t_sl, PROCSROW + 1, PROCSCOL + 9, 3);
	putint(total.t_sw, PROCSROW + 1, PROCSCOL + 12, 3);
	PUTRATE(uvmexp.forks, VMSTATROW + 0, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.forks_ppwait, VMSTATROW + 1, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.forks_sharevm, VMSTATROW + 2, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.fltpgwait, VMSTATROW + 3, VMSTATCOL + 4, 5);
	PUTRATE(uvmexp.fltrelck, VMSTATROW + 4, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.fltrelckok, VMSTATROW + 5, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.fltnoram, VMSTATROW + 6, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.fltamcopy, VMSTATROW + 7, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.flt_prcopy, VMSTATROW + 8, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.flt_przero, VMSTATROW + 9, VMSTATCOL + 3, 6);
	PUTRATE(uvmexp.flt_acow, VMSTATROW + 10, VMSTATCOL, 9);
	putint(s.uvmexp.freemin, VMSTATROW + 11, VMSTATCOL, 9);
	putint(s.uvmexp.freetarg, VMSTATROW + 12, VMSTATCOL, 9);
	putint(s.uvmexp.inactarg, VMSTATROW + 13, VMSTATCOL, 9);
	putint(s.uvmexp.wired, VMSTATROW + 14, VMSTATCOL, 9);
	PUTRATE(uvmexp.pdfreed, VMSTATROW + 15, VMSTATCOL, 9);
	if (LINES - 1 > VMSTATROW + 16)
		PUTRATE(uvmexp.pdscans, VMSTATROW + 16, VMSTATCOL, 9);
	if (LINES - 1 > VMSTATROW + 17)
		PUTRATE(uvmexp.zeropages, VMSTATROW + 17, VMSTATCOL, 9);

	PUTRATE(uvmexp.pageins, PAGEROW + 2, PAGECOL + 5, 5);
	PUTRATE(uvmexp.pdpageouts, PAGEROW + 2, PAGECOL + 10, 5);
	PUTRATE(uvmexp.swapins, PAGEROW + 2, PAGECOL + 15, 5);
	PUTRATE(uvmexp.swapouts, PAGEROW + 2, PAGECOL + 20, 5);
	PUTRATE(uvmexp.pgswapin, PAGEROW + 3, PAGECOL + 5, 5);
	PUTRATE(uvmexp.pgswapout, PAGEROW + 3, PAGECOL + 10, 5);

	PUTRATE(uvmexp.swtch, GENSTATROW + 1, GENSTATCOL, 6);
	PUTRATE(uvmexp.traps, GENSTATROW + 1, GENSTATCOL + 6, 6);
	PUTRATE(uvmexp.syscalls, GENSTATROW + 1, GENSTATCOL + 12, 6);
	PUTRATE(uvmexp.intrs, GENSTATROW + 1, GENSTATCOL + 18, 6);
	PUTRATE(uvmexp.softs, GENSTATROW + 1, GENSTATCOL + 24, 6);
	PUTRATE(uvmexp.faults, GENSTATROW + 1, GENSTATCOL + 30, 5);
	mvprintw(DISKROW, DISKCOL + 5, "                              ");
	for (i = 0, c = 0; i < cur.dk_ndrive && c < DRIVESPACE; i++)
		if (cur.dk_select[i] && (c + strlen(dr_name[i])) < DRIVESPACE) {
			l = MAX(5, strlen(dr_name[i]));
			mvprintw(DISKROW, DISKCOL + 5 + c,
			    " %*s", l, dr_name[i]);
			c += 1 + l;
			dinfo(i, c);
		}
	/* and pad the DRIVESPACE */
	l = DRIVESPACE - c;
	for (i = 0; i < 5; i++)
		mvprintw(DISKROW + i, DISKCOL + 5 + c, "%*s", l, "");

	putint(s.nchcount, NAMEIROW + 2, NAMEICOL, 9);
	putint(nchtotal.ncs_goodhits, NAMEIROW + 2, NAMEICOL + 10, 8);
#define nz(x)	((x) ? (x) : 1)
	putfloat(nchtotal.ncs_goodhits * 100.0 / nz(s.nchcount),
	    NAMEIROW + 2, NAMEICOL + 19, 4, 0, 1);
	putint(nchtotal.ncs_pass2, NAMEIROW + 2, NAMEICOL + 24, 7);
	putfloat(nchtotal.ncs_pass2 * 100.0 / nz(s.nchcount),
	    NAMEIROW + 2, NAMEICOL + 33, 4, 0, 1);
	putint(nchtotal.ncs_miss - nchtotal.ncs_pass2,
	   NAMEIROW + 2, NAMEICOL + 38, 7);
	putfloat((nchtotal.ncs_miss - nchtotal.ncs_pass2) *
	    100.0 / nz(s.nchcount), NAMEIROW + 2, NAMEICOL + 45, 4, 0, 1);
#undef nz
}

int
cmdkre(char *cmd, char *args)
{

	if (prefix(cmd, "run")) {
		copyinfo(&s2, &s1);
		state = RUN;
		return (1);
	}
	if (prefix(cmd, "boot")) {
		state = BOOT;
		copyinfo(&z, &s1);
		return (1);
	}
	if (prefix(cmd, "time")) {
		state = TIME;
		return (1);
	}
	if (prefix(cmd, "zero")) {
		if (state == RUN)
			getinfo(&s1, RUN);
		return (1);
	}
	return (dkcmd(cmd, args));
}

/* calculate number of users on the system */
static int
ucount(void)
{
	int nusers = 0;

	if (ut < 0)
		return (0);
	while (read(ut, &utmp, sizeof(utmp)))
		if (utmp.ut_name[0] != '\0')
			nusers++;

	lseek(ut, 0, SEEK_SET);
	return (nusers);
}

static float
cputime(int indx)
{
	double t;
	int i;

	t = 0;
	for (i = 0; i < CPUSTATES; i++)
		t += s.time[i];
	if (t == 0.0)
		t = 1.0;
	return (s.time[indx] * 100.0 / t);
}

static void
putint(int n, int l, int c, int w)
{
	char b[128];

	move(l, c);
	if (n == 0) {
		while (w-- > 0)
			addch(' ');
		return;
	}
	snprintf(b, sizeof b, "%*d", w, n);
	if (strlen(b) > w) {
		while (w-- > 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
putfloat(double f, int l, int c, int w, int d, int nz)
{
	char b[128];

	move(l, c);
	if (nz && f == 0.0) {
		while (--w >= 0)
			addch(' ');
		return;
	}
	snprintf(b, sizeof b, "%*.*f", w, d, f);
	if (strlen(b) > w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
getinfo(struct Info *s, enum state st)
{
	static int cp_time_mib[] = { CTL_KERN, KERN_CPTIME };
	static int nchstats_mib[2] = { CTL_KERN, KERN_NCHSTATS };
	static int uvmexp_mib[2] = { CTL_VM, VM_UVMEXP };
	static int vmtotal_mib[2] = { CTL_VM, VM_METER };
	size_t size;
#if defined(__i386__)
	struct intrhand *intrhand[16], *ihp, ih;
	struct intrhand *apicintrhand[256];
	int i, n;
#endif

	dkreadstats();
#if defined(__i386__)
	NREAD(X_INTRHAND, intrhand, sizeof(intrhand));
	for (i = 0, n = 0; i < 16; i++) {
		ihp = intrhand[i];
		while (ihp) {
			KREAD(ihp, &ih, sizeof(ih));
			s->intrcnt[n++] = ih.ih_count;
			ihp = ih.ih_next;
		}
	}
	if (namelist[X_APICINTRHAND].n_value) {
		NREAD(X_APICINTRHAND, apicintrhand, sizeof(apicintrhand));
		for (i = 0, n = 0; i < 256; i++) {
			ihp = apicintrhand[i];
			while (ihp) {
				KREAD(ihp, &ih, sizeof(ih));
				s->intrcnt[n++] = ih.ih_count;
				ihp = ih.ih_next;
			}
		}
	}
#else
	NREAD(X_INTRCNT, s->intrcnt, nintr * sizeof(int));
#endif
	size = sizeof(s->time);
	if (sysctl(cp_time_mib, 2, &s->time, &size, NULL, 0) < 0) {
		error("Can't get KERN_CPTIME: %s\n", strerror(errno));
		bzero(&s->time, sizeof(s->time));
	}

	size = sizeof(s->nchstats);
	if (sysctl(nchstats_mib, 2, &s->nchstats, &size, NULL, 0) < 0) {
		error("Can't get KERN_NCHSTATS: %s\n", strerror(errno));
		bzero(&s->nchstats, sizeof(s->nchstats));
	}

	size = sizeof(s->uvmexp);
	if (sysctl(uvmexp_mib, 2, &s->uvmexp, &size, NULL, 0) < 0) {
		error("Can't get VM_UVMEXP: %s\n", strerror(errno));
		bzero(&s->uvmexp, sizeof(s->uvmexp));
	}

	size = sizeof(s->Total);
	if (sysctl(vmtotal_mib, 2, &s->Total, &size, NULL, 0) < 0) {
		error("Can't get VM_METER: %s\n", strerror(errno));
		bzero(&s->Total, sizeof(s->Total));
	}
}

static void
allocinfo(struct Info *s)
{

	s->intrcnt = (int *) malloc(nintr * sizeof(int));
	if (s->intrcnt == NULL)
		errx(2, "out of memory");
}

static void
copyinfo(struct Info *from, struct Info *to)
{
	int *intrcnt;

	intrcnt = to->intrcnt;
	*to = *from;
	bcopy(from->intrcnt, to->intrcnt = intrcnt, nintr * sizeof (int));
}

static void
dinfo(int dn, int c)
{
	double words, atime;

	c += DISKCOL;

	/* time busy in disk activity */
	atime = (double)cur.dk_time[dn].tv_sec +
	    ((double)cur.dk_time[dn].tv_usec / (double)1000000);

	/* # of K transferred */
	words = (cur.dk_rbytes[dn] + cur.dk_wbytes[dn]) / 1024.0;

	putint((int)((float)cur.dk_seek[dn]/etime+0.5), DISKROW + 1, c, 5);
	putint((int)((float)(cur.dk_rxfer[dn] + cur.dk_wxfer[dn])/etime+0.5),
	    DISKROW + 2, c, 5);
	putint((int)(words/etime + 0.5), DISKROW + 3, c, 5);
	putfloat(atime/etime, DISKROW + 4, c, 5, 1, 1);
}
