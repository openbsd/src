/*	$OpenBSD: print.c,v 1.29 2003/01/05 01:39:24 deraadt Exp $	*/
/*	$NetBSD: print.c,v 1.27 1995/09/29 21:58:12 cgd Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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
#if 0
static char sccsid[] = "@(#)print.c	8.6 (Berkeley) 4/16/94";
#else
static char rcsid[] = "$OpenBSD: print.c,v 1.29 2003/01/05 01:39:24 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <sys/ucred.h>
#include <sys/sysctl.h>
#include <uvm/uvm_extern.h>

#include <err.h>
#include <grp.h>
#include <kvm.h>
#include <math.h>
#include <nlist.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>
#include <unistd.h>
#include <pwd.h>

#include "ps.h"

extern kvm_t *kd;
extern int needenv, needcomm, commandonly;

static char *cmdpart(char *);

#define	min(a,b)	((a) < (b) ? (a) : (b))

static char *
cmdpart(arg0)
	char *arg0;
{
	char *cp;

	return ((cp = strrchr(arg0, '/')) != NULL ? cp + 1 : arg0);
}

void
printheader()
{
	VAR *v;
	struct varent *vent;

	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		if (v->flag & LJUST) {
			if (vent->next == NULL)	/* last one */
				(void)printf("%s", v->header);
			else
				(void)printf("%-*s", v->width, v->header);
		} else
			(void)printf("%*s", v->width, v->header);
		if (vent->next != NULL)
			(void)putchar(' ');
	}
	(void)putchar('\n');
}

void
command(ki, ve)
	KINFO *ki;
	VARENT *ve;
{
	VAR *v;
	int left;
	char **argv, **p;

	v = ve->var;
	if (ve->next != NULL || termwidth != UNLIMITED) {
		if (ve->next == NULL) {
			left = termwidth - (totwidth - v->width);
			if (left < 1) /* already wrapped, just use std width */
				left = v->width;
		} else
			left = v->width;
	} else
		left = -1;
	if (needenv && kd != NULL) {
		argv = kvm_getenvv(kd, ki->ki_p, termwidth);
		if ((p = argv) != NULL) {
			while (*p) {
				fmt_puts(*p, &left);
				p++;
				fmt_putc(' ', &left);
			}
		}
	} else
		argv = NULL;
	if (needcomm) {
		if (!commandonly) {
			if (kd != NULL) {
				argv = kvm_getargv(kd, ki->ki_p, termwidth);
				if ((p = argv) != NULL) {
					while (*p) {
						fmt_puts(*p, &left);
						p++;
						fmt_putc(' ', &left);
					}
				}
			}
			if (argv == NULL || argv[0] == '\0' ||
			    strcmp(cmdpart(argv[0]), KI_PROC(ki)->p_comm)) {
				fmt_putc('(', &left);
				fmt_puts(KI_PROC(ki)->p_comm, &left);
				fmt_putc(')', &left);
			}
		} else {
			fmt_puts(KI_PROC(ki)->p_comm, &left);
		}
	}
	if (ve->next && left > 0)
		printf("%*s", left, "");
}

void
ucomm(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s", v->width, KI_PROC(k)->p_comm);
}

void
logname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if (KI_EPROC(k)->e_login[0]) {
		int n = min(v->width, MAXLOGNAME);
		(void)printf("%-*.*s", n, n, KI_EPROC(k)->e_login);
		if (v->width > n)
			(void)printf("%*s", v->width - n, "");
	} else
		(void)printf("%-*s", v->width, "-");
}

#define pgtok(a)	(((a)*getpagesize())/1024)

void
state(k, ve)
	KINFO *k;
	VARENT *ve;
{
	struct proc *p;
	int flag;
	char *cp;
	VAR *v;
	char buf[16];

	v = ve->var;
	p = KI_PROC(k);
	flag = p->p_flag;
	cp = buf;

	switch (p->p_stat) {

	case SSTOP:
		*cp = 'T';
		break;

	case SSLEEP:
		if (flag & P_SINTR)	/* interruptible (long) */
			*cp = p->p_slptime >= maxslp ? 'I' : 'S';
		else
			*cp = 'D';
		break;

	case SRUN:
	case SIDL:
		*cp = 'R';
		break;

	case SZOMB:
		*cp = 'Z';
		break;

	default:
		*cp = '?';
	}
	cp++;
	if (flag & P_INMEM) {
	} else
		*cp++ = 'W';
	if (p->p_nice < NZERO)
		*cp++ = '<';
	else if (p->p_nice > NZERO)
		*cp++ = 'N';
	if (flag & P_TRACED)
		*cp++ = 'X';
	if (flag & P_SYSTRACE)
		*cp++ = 'x';
	if (flag & P_WEXIT && p->p_stat != SZOMB)
		*cp++ = 'E';
	if (flag & P_PPWAIT)
		*cp++ = 'V';
	if (flag & P_SYSTEM)
		*cp++ = 'K';
	/* XXX Since P_SYSTEM now shows a K, should L just be for holdcnt? */
	if ((flag & P_SYSTEM) || p->p_holdcnt)
		*cp++ = 'L';
	if ((flag & P_SYSTEM) == 0 &&
	    KI_EPROC(k)->e_maxrss / 1024 < pgtok(KI_EPROC(k)->e_vm.vm_rssize))
		*cp++ = '>';
	if (KI_EPROC(k)->e_flag & EPROC_SLEADER)
		*cp++ = 's';
	if ((flag & P_CONTROLT) && KI_EPROC(k)->e_pgid == KI_EPROC(k)->e_tpgid)
		*cp++ = '+';
	*cp = '\0';
	(void)printf("%-*s", v->width, buf);
}

void
pri(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width, KI_PROC(k)->p_priority - PZERO);
}

void
uname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, user_from_uid(KI_EPROC(k)->e_ucred.cr_uid, 0));
}

void
runame(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, user_from_uid(KI_EPROC(k)->e_pcred.p_ruid, 0));
}

void
gname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, group_from_gid(KI_EPROC(k)->e_ucred.cr_gid, 0));
}

void
rgname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, group_from_gid(KI_EPROC(k)->e_pcred.p_rgid, 0));
}

void
tdev(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	dev_t dev;
	char buff[16];

	v = ve->var;
	dev = KI_EPROC(k)->e_tdev;
	if (dev == NODEV)
		(void)printf("%*s", v->width, "??");
	else {
		(void)snprintf(buff, sizeof(buff),
		    "%d/%d", major(dev), minor(dev));
		(void)printf("%*s", v->width, buff);
	}
}

void
tname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;
	dev = KI_EPROC(k)->e_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%-*s", v->width, "??");
	else {
		if (strncmp(ttname, "tty", 3) == 0)
			ttname += 3;
		(void)printf("%*.*s%c", v->width-1, v->width-1, ttname,
			KI_EPROC(k)->e_flag & EPROC_CTTY ? ' ' : '-');
	}
}

void
longtname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;
	dev = KI_EPROC(k)->e_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%-*s", v->width, "??");
	else
		(void)printf("%-*s", v->width, ttname);
}

void
started(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	static time_t now;
	time_t startt;
	struct tm *tp;
	char buf[100];

	v = ve->var;
	if (!k->ki_u.u_valid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}

	startt = k->ki_u.u_start.tv_sec;
	tp = localtime(&startt);
	if (!now)
		(void)time(&now);
	if (now - k->ki_u.u_start.tv_sec < 24 * SECSPERHOUR) {
		(void)strftime(buf, sizeof(buf) - 1, "%l:%M%p", tp);
	} else if (now - k->ki_u.u_start.tv_sec < 7 * SECSPERDAY) {
		(void)strftime(buf, sizeof(buf) - 1, "%a%I%p", tp);
	} else
		(void)strftime(buf, sizeof(buf) - 1, "%e%b%y", tp);
	(void)printf("%-*s", v->width, buf);
}

void
lstarted(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	time_t startt;
	char buf[100];

	v = ve->var;
	if (!k->ki_u.u_valid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}
	startt = k->ki_u.u_start.tv_sec;
	(void)strftime(buf, sizeof(buf) -1, "%c",
	    localtime(&startt));
	(void)printf("%-*s", v->width, buf);
}

void
wchan(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if (KI_PROC(k)->p_wchan) {
		int n;

		if (KI_PROC(k)->p_wmesg) {
			n = min(v->width, WMESGLEN);
			(void)printf("%-*.*s", n, n, KI_EPROC(k)->e_wmesg);
			if (v->width > n)
				(void)printf("%*s", v->width - n, "");
		} else
			(void)printf("%-*lx", v->width,
			    (long)KI_PROC(k)->p_wchan &~ KERNBASE);
	} else
		(void)printf("%-*s", v->width, "-");
}

void
vsize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width,
	    pgtok(KI_EPROC(k)->e_vm.vm_dsize + KI_EPROC(k)->e_vm.vm_ssize +
		KI_EPROC(k)->e_vm.vm_tsize));
}

void
rssize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	/* XXX don't have info about shared */
	(void)printf("%*d", v->width, (KI_PROC(k)->p_flag & P_SYSTEM) ? 0 :
	    pgtok(KI_EPROC(k)->e_vm.vm_rssize));
}

void
p_rssize(k, ve)		/* doesn't account for text */
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width, (KI_PROC(k)->p_flag & P_SYSTEM) ? 0 :
	    pgtok(KI_EPROC(k)->e_vm.vm_rssize));
}

void
cputime(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	long secs;
	long psecs;	/* "parts" of a second. first micro, then centi */
	char obuff[128];

	v = ve->var;
	if (KI_PROC(k)->p_stat == SZOMB || !k->ki_u.u_valid) {
		secs = 0;
		psecs = 0;
	} else {
		/*
		 * This counts time spent handling interrupts.  We could
		 * fix this, but it is not 100% trivial (and interrupt
		 * time fractions only work on the sparc anyway).	XXX
		 */
		secs = KI_PROC(k)->p_rtime.tv_sec;
		psecs = KI_PROC(k)->p_rtime.tv_usec;
		if (sumrusage) {
			secs += k->ki_u.u_cru.ru_utime.tv_sec +
				k->ki_u.u_cru.ru_stime.tv_sec;
			psecs += k->ki_u.u_cru.ru_utime.tv_usec +
				k->ki_u.u_cru.ru_stime.tv_usec;
		}
		/*
		 * round and scale to 100's
		 */
		psecs = (psecs + 5000) / 10000;
		secs += psecs / 100;
		psecs = psecs % 100;
	}
	(void)snprintf(obuff, sizeof(obuff),
	    "%3ld:%02ld.%02ld", secs/60, secs%60, psecs);
	(void)printf("%*s", v->width, obuff);
}

double
getpcpu(k)
	KINFO *k;
{
	struct proc *p;
	static int failure;
	double d;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);

	p = KI_PROC(k);
#define	fxtofl(fixpt)	((double)(fixpt) / fscale)

	/* XXX - I don't like this */
	if (p->p_swtime == 0 || (p->p_flag & P_INMEM) == 0)
		return (0.0);
	if (rawcpu)
		return (100.0 * fxtofl(p->p_pctcpu));

	d = p->p_swtime * log(fxtofl(ccpu));
	if (d < -700.0)
		d = 0.0;		/* avoid IEEE underflow */
	else
		d = exp(d);
	if (d == 1.0)
		return (0.0);
	return (100.0 * fxtofl(p->p_pctcpu) /
		(1.0 - d));
}

void
pcpu(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*.1f", v->width, getpcpu(k));
}

double
getpmem(k)
	KINFO *k;
{
	static int failure;
	struct proc *p;
	struct eproc *e;
	double fracmem;
	int szptudot;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);

	p = KI_PROC(k);
	e = KI_EPROC(k);
	if ((p->p_flag & P_INMEM) == 0 || (p->p_flag & P_SYSTEM))
		return (0.0);
	/* XXX want pmap ptpages, segtab, etc. (per architecture) */
	szptudot = USPACE/getpagesize();
	/* XXX don't have info about shared */
	fracmem = ((float)e->e_vm.vm_rssize + szptudot)/mempages;
	return (100.0 * fracmem);
}

void
pmem(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*.1f", v->width, getpmem(k));
}

void
pagein(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*ld", v->width,
	    k->ki_u.u_valid ? k->ki_u.u_ru.ru_majflt : 0);
}

void
maxrss(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*lld", v->width, KI_EPROC(k)->e_maxrss / 1024);
}

void
tsize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width, pgtok(KI_EPROC(k)->e_vm.vm_tsize));
}

void
dsize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width, pgtok(KI_EPROC(k)->e_vm.vm_dsize));
}

void
ssize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width, pgtok(KI_EPROC(k)->e_vm.vm_ssize));
}

/*
 * Generic output routines.  Print fields from various prototype
 * structures.
 */
static void
printval(bp, v)
	char *bp;
	VAR *v;
{
	static char ofmt[32] = "%";
	char *fcp, *cp;
	enum type type;

	cp = ofmt + 1;
	fcp = v->fmt;
	if (v->flag & LJUST)
		*cp++ = '-';
	*cp++ = '*';
	while ((*cp++ = *fcp++));

	/*
	 * Note that the "INF127" check is nonsensical for types
	 * that are or can be signed.
	 */
#define	GET(type)		(*(type *)bp)
#define	CHK_INF127(n)		(((n) > 127) && (v->flag & INF127) ? 127 : (n))

	switch (v->type) {
	case INT32:
		if (sizeof(int32_t) == sizeof(int))
			type = INT;
		else if (sizeof(int32_t) == sizeof(long))
			type = LONG;
		else
			errx(1, "unknown conversion for type %d", v->type);
		break;
	case UINT32:
		if (sizeof(u_int32_t) == sizeof(u_int))
			type = UINT;
		else if (sizeof(u_int32_t) == sizeof(u_long))
			type = ULONG;
		else
			errx(1, "unknown conversion for type %d", v->type);
		break;
	default:
		type = v->type;
		break;
	}

	switch (type) {
	case CHAR:
		(void)printf(ofmt, v->width, GET(char));
		break;
	case UCHAR:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_char)));
		break;
	case SHORT:
		(void)printf(ofmt, v->width, GET(short));
		break;
	case USHORT:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_short)));
		break;
	case INT:
		(void)printf(ofmt, v->width, GET(int));
		break;
	case UINT:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_int)));
		break;
	case LONG:
		(void)printf(ofmt, v->width, GET(long));
		break;
	case ULONG:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_long)));
		break;
	case KPTR:
		(void)printf(ofmt, v->width, GET(u_long) &~ KERNBASE);
		break;
	default:
		errx(1, "unknown type %d", v->type);
	}
#undef GET
#undef CHK_INF127
}

void
pvar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	printval((char *)((char *)KI_PROC(k) + v->off), v);
}

void
evar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	printval((char *)((char *)KI_EPROC(k) + v->off), v);
}

void
uvar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if (k->ki_u.u_valid)
		printval((char *)((char *)&k->ki_u + v->off), v);
	else
		(void)printf("%*s", v->width, "-");
}

void
rvar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if (k->ki_u.u_valid)
		printval((char *)((char *)(&k->ki_u.u_ru) + v->off), v);
	else
		(void)printf("%*s", v->width, "-");
}

void
emulname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;

	(void)printf("%-*s",
	    (int)v->width, KI_EPROC(k)->e_emul);
}
