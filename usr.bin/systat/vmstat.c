/*	$OpenBSD: vmstat.c,v 1.72 2009/10/27 23:59:44 deraadt Exp $	*/
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
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "systat.h"

static struct Info {
	long	time[CPUSTATES];
	struct	uvmexp uvmexp;
	struct	vmtotal Total;
	struct	nchstats nchstats;
	long	nchcount;
	u_quad_t *intrcnt;
} s, s1, s2, s3, z;

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
static void getinfo(struct Info *);
void putint(int, int, int, int);
void putintmk(int, int, int, int);
void putuint64(u_int64_t, int, int, int);
void putfloat(double, int, int, int, int, int);
int ucount(void);

void print_vm(void);
int read_vm(void);
int select_vm(void);
int vm_keyboard_callback(int);

static	time_t t;
static	double etime;
static	float hertz;
static	int nintr;
static	long *intrloc;
static	char **intrname;
static	int nextintsrow;

WINDOW *
openkre(void)
{
	return (subwin(stdscr, LINES-1-1, 0, 1, 0));
}

void
closekre(WINDOW *w)
{

	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

/*
 * These constants define where the major pieces are laid out
 */
#define STATROW		 0	/* uses 1 row and 68 cols */
#define STATCOL		 2
#define MEMROW		 2	/* uses 4 rows and 34 cols */
#define MEMCOL		 0
#define PAGEROW		 2	/* uses 4 rows and 26 cols */
#define PAGECOL		37
#define INTSROW		 2	/* uses all rows to bottom and 17 cols */
#define INTSCOL		63
#define PROCSROW	 7	/* uses 2 rows and 20 cols */
#define PROCSCOL	 0
#define GENSTATROW	 7	/* uses 2 rows and 35 cols */
#define GENSTATCOL	16
#define VMSTATROW	 7	/* uses 18 rows and 12 cols */
#define VMSTATCOL	48
#define GRAPHROW	10	/* uses 3 rows and 51 cols */
#define GRAPHCOL	 0
#define NAMEIROW	14	/* uses 3 rows and 49 cols */
#define NAMEICOL	 0
#define DISKROW		18	/* uses 5 rows and 50 cols (for 9 drives) */
#define DISKCOL		 0

#define	DRIVESPACE	45	/* max space for drives */


field_def *view_vm_0[] = {
	NULL
};

/* Define view managers */
struct view_manager vmstat_mgr = {
	"VMstat", select_vm, read_vm, NULL, print_header,
	print_vm, vm_keyboard_callback, NULL, NULL
};

field_view views_vm[] = {
	{view_vm_0, "vmstat", '7', &vmstat_mgr},
	{NULL, NULL, 0, NULL}
};

int ncpu = 1;

int
initvmstat(void)
{
	field_view *v;
	int mib[4], i;
	size_t size;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	size = sizeof(ncpu);
	if (sysctl(mib, 2, &ncpu, &size, NULL, 0) < 0)
		return (-1);

	hertz = stathz ? stathz : hz;
	if (!dkinit(1))
		return(0);

	mib[0] = CTL_KERN;
	mib[1] = KERN_INTRCNT;
	mib[2] = KERN_INTRCNT_NUM;
	size = sizeof(nintr);
	if (sysctl(mib, 3, &nintr, &size, NULL, 0) < 0)
		return (-1);

	intrloc = calloc(nintr, sizeof(long));
	intrname = calloc(nintr, sizeof(char *));

	for (i = 0; i < nintr; i++) {
		char name[128];

		mib[0] = CTL_KERN;
		mib[1] = KERN_INTRCNT;
		mib[2] = KERN_INTRCNT_NAME;
		mib[3] = i;
		size = sizeof(name);
		if (sysctl(mib, 4, name, &size, NULL, 0) < 0)
			return (-1);

		intrname[i] = strdup(name);
		if (intrname[i] == NULL)
			return (-1);
	}

	nextintsrow = INTSROW + 2;
	allocinfo(&s);
	allocinfo(&s1);
	allocinfo(&s2);
	allocinfo(&s3);
	allocinfo(&z);

	getinfo(&s2);
	copyinfo(&z, &s1);

	for (v = views_vm; v->name != NULL; v++)
		add_view(v);

	return(1);
}

void
fetchkre(void)
{
	getinfo(&s3);
}

void
labelkre(void)
{
	int i, j, l;

	mvprintw(MEMROW, MEMCOL,     "            memory totals (in KB)");
	mvprintw(MEMROW + 1, MEMCOL, "           real   virtual     free");
	mvprintw(MEMROW + 2, MEMCOL, "Active");
	mvprintw(MEMROW + 3, MEMCOL, "All");

	mvprintw(PAGEROW, PAGECOL, "        PAGING   SWAPPING ");
	mvprintw(PAGEROW + 1, PAGECOL, "        in  out   in  out ");
	mvprintw(PAGEROW + 2, PAGECOL, "ops");
	mvprintw(PAGEROW + 3, PAGECOL, "pages");

	mvprintw(INTSROW, INTSCOL + 3, " Interrupts");
	mvprintw(INTSROW + 1, INTSCOL + 9, "total");

	mvprintw(LINES - 3, INTSCOL + 9, "IPKTS");
	mvprintw(LINES - 2, INTSCOL + 9, "OPKTS");

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
	if (LINES - 1 > VMSTATROW + 18)
		mvprintw(VMSTATROW + 18, VMSTATCOL + 10, "kmapent");

	mvprintw(GENSTATROW, GENSTATCOL, "   Csw   Trp   Sys   Int   Sof  Flt");

	mvprintw(GRAPHROW, GRAPHCOL,
	    "    . %%Int    . %%Sys    . %%Usr    . %%Nic    . %%Idle");
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
	mvprintw(DISKROW + 3, DISKCOL, "speed");
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

#define X(fld)	{s.fld[i]; s.fld[i]-=s1.fld[i];}
#define Y(fld)	{s.fld; s.fld -= s1.fld;}
#define Z(fld)	{s.nchstats.fld; s.nchstats.fld -= s1.nchstats.fld;}
#define PUTRATE(fld, l, c, w) \
	do { \
		Y(fld); \
		putint((int)((float)s.fld/etime + 0.5), l, c, w); \
	} while (0)
#define MAXFAIL 5

static	char cpuchar[CPUSTATES] = { '|', '=', '>', '-', ' ' };
static	char cpuorder[CPUSTATES] = { CP_INTR, CP_SYS, CP_USER, CP_NICE, CP_IDLE };

void
showkre(void)
{
	float f1, f2;
	int psiz;
	u_int64_t inttotal, intcnt;
	int i, l, c;
	static int failcnt = 0, first_run = 0;

	if (state == TIME) {
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
			error("The alternate system clock has died!");
			failcnt = 0;
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
		t = intcnt = s.intrcnt[i];
		s.intrcnt[i] -= s1.intrcnt[i];
		intcnt = (u_int64_t)((float)s.intrcnt[i]/etime + 0.5);
		inttotal += intcnt;
		putuint64(intcnt, intrloc[i], INTSCOL, 8);
	}
	putuint64(inttotal, INTSROW + 1, INTSCOL, 8);
	Z(ncs_goodhits); Z(ncs_badhits); Z(ncs_miss);
	Z(ncs_long); Z(ncs_pass2); Z(ncs_2passes);
	s.nchcount = nchtotal.ncs_goodhits + nchtotal.ncs_badhits +
	    nchtotal.ncs_miss + nchtotal.ncs_long;

	putint(sum.ifc_ip, LINES - 3, INTSCOL, 8);
	putint(sum.ifc_op, LINES - 2, INTSCOL, 8);

	psiz = 0;
	f2 = 0.0;

	for (c = 0; c < CPUSTATES; c++) {
		i = cpuorder[c];
		f1 = cputime(i);
		f2 += f1;
		l = (int) ((f2 + 1.0) / 2.0) - psiz;
		putfloat(f1, GRAPHROW, GRAPHCOL + 1 + (10 * c), 5, 1, 0);
		move(GRAPHROW + 2, psiz);
		psiz += l;
		while (l-- > 0)
			addch(cpuchar[c]);
	}

#define pgtokb(pg)	((pg) * (s.uvmexp.pagesize / 1024))

	putint(pgtokb(s.uvmexp.active), MEMROW + 2, MEMCOL + 7, 8);
	putint(pgtokb(s.uvmexp.active + s.uvmexp.swpginuse),    /* XXX */
	    MEMROW + 2, MEMCOL + 17, 8);
	putint(pgtokb(s.uvmexp.npages - s.uvmexp.free), MEMROW + 3, MEMCOL + 7, 8);
	putint(pgtokb(s.uvmexp.npages - s.uvmexp.free + s.uvmexp.swpginuse),
	    MEMROW + 3, MEMCOL + 17, 8);
	putint(pgtokb(s.uvmexp.free), MEMROW + 2, MEMCOL + 26, 8);
	putint(pgtokb(s.uvmexp.free + s.uvmexp.swpages - s.uvmexp.swpginuse),
	    MEMROW + 3, MEMCOL + 26, 8);
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
	if (LINES - 1 > VMSTATROW + 18)
		putint(s.uvmexp.kmapent, VMSTATROW + 18, VMSTATCOL, 9);

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
	putint(nchtotal.ncs_miss + nchtotal.ncs_long - nchtotal.ncs_pass2,
	   NAMEIROW + 2, NAMEICOL + 38, 7);
	putfloat((nchtotal.ncs_miss + nchtotal.ncs_long - nchtotal.ncs_pass2) *
	    100.0 / nz(s.nchcount), NAMEIROW + 2, NAMEICOL + 45, 4, 0, 1);
#undef nz

}

int
vm_keyboard_callback(int ch)
{
	switch(ch) {
	case 'r':
		copyinfo(&s2, &s1);
		state = RUN;
		break;
	case 'b':
		state = BOOT;
		copyinfo(&z, &s1);
		break;
	case 't':
		state = TIME;
		break;
	case 'z':
		if (state == RUN)
			getinfo(&s1);
		break;
	}
	return (keyboard_callback(ch));
}


static float
cputime(int indx)
{
	double tm;
	int i;

	tm = 0;
	for (i = 0; i < CPUSTATES; i++)
		tm += s.time[i];
	if (tm == 0.0)
		tm = 1.0;
	return (s.time[indx] * 100.0 / tm);
}

void
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

void
putintmk(int n, int l, int c, int w)
{
	char b[128];

	move(l, c);
	if (n == 0) {
		while (w-- > 0)
			addch(' ');
		return;
	}
	if (n > 9999 * 1024)
		snprintf(b, sizeof b, "%*dG", w - 1, n / 1024 / 1024);
	else if (n > 9999)
		snprintf(b, sizeof b, "%*dM", w - 1, n / 1024);
	else
		snprintf(b, sizeof b, "%*dK", w - 1, n);
	if (strlen(b) > w) {
		while (w-- > 0)
			addch('*');
		return;
	}
	addstr(b);
}

void
putuint64(u_int64_t n, int l, int c, int w)
{
	char b[128];

	move(l, c);
	if (n == 0) {
		while (w-- > 0)
			addch(' ');
		return;
	}
	snprintf(b, sizeof b, "%*llu", w, n);
	if (strlen(b) > w) {
		while (w-- > 0)
			addch('*');
		return;
	}
	addstr(b);
}

void
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
getinfo(struct Info *si)
{
	static int cp_time_mib[] = { CTL_KERN, KERN_CPTIME };
	static int nchstats_mib[2] = { CTL_KERN, KERN_NCHSTATS };
	static int uvmexp_mib[2] = { CTL_VM, VM_UVMEXP };
	static int vmtotal_mib[2] = { CTL_VM, VM_METER };
	int mib[4], i;
	size_t size;

	dkreadstats();

	for (i = 0; i < nintr; i++) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_INTRCNT;
		mib[2] = KERN_INTRCNT_CNT;
		mib[3] = i;
		size = sizeof(si->intrcnt[i]);
		if (sysctl(mib, 4, &si->intrcnt[i], &size, NULL, 0) < 0) {
			si->intrcnt[i] = 0;
		}
	}

	size = sizeof(si->time);
	if (sysctl(cp_time_mib, 2, &si->time, &size, NULL, 0) < 0) {
		error("Can't get KERN_CPTIME: %s\n", strerror(errno));
		bzero(&si->time, sizeof(si->time));
	}

	size = sizeof(si->nchstats);
	if (sysctl(nchstats_mib, 2, &si->nchstats, &size, NULL, 0) < 0) {
		error("Can't get KERN_NCHSTATS: %s\n", strerror(errno));
		bzero(&si->nchstats, sizeof(si->nchstats));
	}

	size = sizeof(si->uvmexp);
	if (sysctl(uvmexp_mib, 2, &si->uvmexp, &size, NULL, 0) < 0) {
		error("Can't get VM_UVMEXP: %s\n", strerror(errno));
		bzero(&si->uvmexp, sizeof(si->uvmexp));
	}

	size = sizeof(si->Total);
	if (sysctl(vmtotal_mib, 2, &si->Total, &size, NULL, 0) < 0) {
		error("Can't get VM_METER: %s\n", strerror(errno));
		bzero(&si->Total, sizeof(si->Total));
	}
}

static void
allocinfo(struct Info *si)
{
	memset(si, 0, sizeof(*si));
	si->intrcnt = (u_quad_t *) calloc(nintr, sizeof(u_quad_t));
	if (si->intrcnt == NULL)
		errx(2, "out of memory");
}

static void
copyinfo(struct Info *from, struct Info *to)
{
	u_quad_t *intrcnt;

	intrcnt = to->intrcnt;
	*to = *from;
	bcopy(from->intrcnt, to->intrcnt = intrcnt, nintr * sizeof (u_quad_t));
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
	putintmk((int)(words/etime + 0.5), DISKROW + 3, c, 5);
	putfloat(atime/etime, DISKROW + 4, c, 5, 1, 1);
}



int
select_vm(void)
{
	num_disp = 0;
	return (0);
}

int
read_vm(void)
{
	if (state == TIME)
		copyinfo(&s3, &s1);
	fetchkre();
	fetchifstat();
	if (state == TIME)
		dkswap();
	num_disp = 0;
	return 0;
}


void
print_vm(void)
{
	copyinfo(&s3, &s);
	labelkre();
	showkre();
}
