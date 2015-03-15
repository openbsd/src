/*	$OpenBSD: print.c,v 1.60 2015/03/15 00:41:27 millert Exp $	*/
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

#include <sys/param.h>	/* MAXCOMLEN PZERO NODEV */
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <sys/sysctl.h>

#include <err.h>
#include <grp.h>
#include <kvm.h>
#include <math.h>
#include <nlist.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>

#include "ps.h"

extern kvm_t *kd;
extern int needenv, needcomm, neednlist, commandonly;

static char *cmdpart(char *);

#define	min(a,b)	((a) < (b) ? (a) : (b))

static char *
cmdpart(char *arg0)
{
	char *cp;

	return ((cp = strrchr(arg0, '/')) != NULL ? cp + 1 : arg0);
}

void
printheader(void)
{
	VAR *v;
	struct varent *vent;

	if (!needheader)
		return;
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
command(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	int left, wantspace = 0;
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
		argv = kvm_getenvv(kd, kp, termwidth);
		if ((p = argv) != NULL) {
			while (*p) {
				fmt_puts(*p, &left);
				p++;
				if (*p)
					fmt_putc(' ', &left);
				else
					wantspace = 1;
			}
		}
	} else
		argv = NULL;
	if (needcomm) {
		if (!commandonly) {
			if (kd != NULL) {
				argv = kvm_getargv(kd, kp, termwidth);
				if ((p = argv) != NULL) {
					if (wantspace) {
						fmt_putc(' ', &left);
						wantspace = 0;
					}
					while (*p) {
						fmt_puts(*p, &left);
						p++;
						if (*p)
							fmt_putc(' ', &left);
						else
							wantspace = 1;
					}
				}
			}
			if (argv == NULL || argv[0] == '\0' ||
			    strcmp(cmdpart(argv[0]), kp->p_comm)) {
				if (wantspace) {
					fmt_putc(' ', &left);
					wantspace = 0;
				}
				fmt_putc('(', &left);
				fmt_puts(kp->p_comm, &left);
				fmt_putc(')', &left);
			}
		} else {
			if (wantspace) {
				fmt_putc(' ', &left);
				wantspace = 0;
			}
			fmt_puts(kp->p_comm, &left);
		}
	}
	if (ve->next && left > 0) {
		if (wantspace) {
			fmt_putc(' ', &left);
			wantspace = 0;
		}
		printf("%*s", left, "");
	}
}

void
ucomm(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s", v->width, kp->p_comm);
}

void
curwd(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	int name[] = { CTL_KERN, KERN_PROC_CWD, kp->p_pid };
	char path[PATH_MAX];
	size_t pathlen = sizeof path;

	if (!kvm_sysctl_only || sysctl(name, 3, path, &pathlen, NULL, 0) != 0)
		*path = '\0';

	v = ve->var;
	(void)printf("%-*s", v->width, path);
}

void
logname(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	if (kp->p_login[0]) {
		int n = min(v->width, LOGIN_NAME_MAX);
		(void)printf("%-*.*s", n, n, kp->p_login);
		if (v->width > n)
			(void)printf("%*s", v->width - n, "");
	} else
		(void)printf("%-*s", v->width, "-");
}

#define pgtok(a)	(((unsigned long long)(a)*getpagesize())/1024)

void
state(const struct kinfo_proc *kp, VARENT *ve)
{
	int flag;
	char *cp, state = '\0';
	VAR *v;
	char buf[16];

	v = ve->var;
	flag = kp->p_flag;
	cp = buf;

	switch (kp->p_stat) {

	case SSTOP:
		*cp = 'T';
		break;

	case SSLEEP:
		if (flag & P_SINTR)	/* interruptible (long) */
			*cp = kp->p_slptime >= maxslp ? 'I' : 'S';
		else
			*cp = 'D';
		break;

	case SRUN:
	case SIDL:
	case SONPROC:
		state = *cp = 'R';
		break;

	case SDEAD:
		*cp = 'Z';
		break;

	default:
		*cp = '?';
	}
	cp++;

	if (kp->p_nice < NZERO)
		*cp++ = '<';
	else if (kp->p_nice > NZERO)
		*cp++ = 'N';
	if (kp->p_psflags & PS_TRACED)
		*cp++ = 'X';
	if (flag & P_SYSTRACE)
		*cp++ = 'x';
	if ((kp->p_psflags & (PS_EXITING | PS_ZOMBIE)) == PS_EXITING)
		*cp++ = 'E';
	if (kp->p_psflags & PS_ISPWAIT)
		*cp++ = 'V';
	if (flag & P_SYSTEM)
		*cp++ = 'K';
	if ((flag & P_SYSTEM) == 0 &&
	    kp->p_rlim_rss_cur / 1024 < pgtok(kp->p_vm_rssize))
		*cp++ = '>';
	if (kp->p_eflag & EPROC_SLEADER)
		*cp++ = 's';
	if ((kp->p_psflags & PS_CONTROLT) && kp->p__pgid == kp->p_tpgid)
		*cp++ = '+';
	*cp = '\0';

	if (state == 'R' && kp->p_cpuid != KI_NOCPU) {
		char pbuf[16];

		snprintf(pbuf, sizeof pbuf, "/%llu", kp->p_cpuid);
		*++cp = '\0';
		strlcat(buf, pbuf, sizeof buf);
		cp = buf + strlen(buf);
	}

	(void)printf("%-*s", v->width, buf);
}

void
pri(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width, kp->p_priority - PZERO);
}

void
pnice(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	v = ve->var;
	(void)printf("%*d", v->width, kp->p_nice - NZERO);
}

void
euname(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, user_from_uid(kp->p_uid, 0));
}

void
runame(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, user_from_uid(kp->p_ruid, 0));
}

void
gname(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, group_from_gid(kp->p_gid, 0));
}

void
rgname(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, group_from_gid(kp->p_rgid, 0));
}

void
tdev(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	dev_t dev;
	char buff[16];

	v = ve->var;
	dev = kp->p_tdev;
	if (dev == NODEV)
		(void)printf("%*s", v->width, "??");
	else {
		(void)snprintf(buff, sizeof(buff),
		    "%d/%d", major(dev), minor(dev));
		(void)printf("%*s", v->width, buff);
	}
}

void
tname(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;
	dev = kp->p_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%-*s", v->width, "??");
	else {
		if (strncmp(ttname, "tty", 3) == 0)
			ttname += 3;
		(void)printf("%*.*s%c", v->width-1, v->width-1, ttname,
			kp->p_eflag & EPROC_CTTY ? ' ' : '-');
	}
}

void
longtname(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;
	dev = kp->p_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%-*s", v->width, "??");
	else
		(void)printf("%-*s", v->width, ttname);
}

void
started(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	static time_t now;
	time_t startt;
	struct tm *tp;
	char buf[100];

	v = ve->var;
	if (!kp->p_uvalid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}

#define SECSPERHOUR	(60 * 60)
#define SECSPERDAY	(24 * 60 * 60)

	startt = kp->p_ustart_sec;
	tp = localtime(&startt);
	if (!now)
		(void)time(&now);
	if (now - kp->p_ustart_sec < 12 * SECSPERHOUR) {
		(void)strftime(buf, sizeof(buf) - 1, "%l:%M%p", tp);
	} else if (now - kp->p_ustart_sec < 7 * SECSPERDAY) {
		(void)strftime(buf, sizeof(buf) - 1, "%a%I%p", tp);
	} else
		(void)strftime(buf, sizeof(buf) - 1, "%e%b%y", tp);
	(void)printf("%-*s", v->width, buf);
}

void
lstarted(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	time_t startt;
	char buf[100];

	v = ve->var;
	if (!kp->p_uvalid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}
	startt = kp->p_ustart_sec;
	(void)strftime(buf, sizeof(buf) -1, "%c",
	    localtime(&startt));
	(void)printf("%-*s", v->width, buf);
}

void
wchan(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	if (kp->p_wmesg[0]) {
		(void)printf("%-*s", (int)v->width, kp->p_wmesg);
	} else
		(void)printf("%-*s", v->width, "-");
}

void
vsize(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*llu", v->width,
	    pgtok(kp->p_vm_dsize + kp->p_vm_ssize + kp->p_vm_tsize));
}

void
rssize(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	/* XXX don't have info about shared */
	(void)printf("%*llu", v->width, (kp->p_flag & P_SYSTEM) ? 0 :
	    pgtok(kp->p_vm_rssize));
}

void
p_rssize(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*llu", v->width, (kp->p_flag & P_SYSTEM) ? 0 :
	    pgtok(kp->p_vm_rssize));
}

void
cputime(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;
	long secs;
	long psecs;	/* "parts" of a second. first micro, then centi */
	char obuff[128];

	v = ve->var;
	if (kp->p_stat == SDEAD || !kp->p_uvalid) {
		secs = 0;
		psecs = 0;
	} else {
		/*
		 * This counts time spent handling interrupts.  We could
		 * fix this, but it is not 100% trivial (and interrupt
		 * time fractions only work on the sparc anyway).	XXX
		 */
		secs = kp->p_rtime_sec;
		psecs = kp->p_rtime_usec;
		if (sumrusage) {
			secs += kp->p_uctime_sec;
			psecs += kp->p_uctime_usec;
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
getpcpu(const struct kinfo_proc *kp)
{
	double d;

	if (fscale == 0)
		return (0.0);

#define	fxtofl(fixpt)	((double)(fixpt) / fscale)

	/* XXX - I don't like this */
	if (kp->p_swtime == 0)
		return (0.0);
	if (rawcpu)
		return (100.0 * fxtofl(kp->p_pctcpu));

	d = kp->p_swtime * log(fxtofl(ccpu));
	if (d < -700.0)
		d = 0.0;		/* avoid IEEE underflow */
	else
		d = exp(d);
	if (d == 1.0)
		return (0.0);
	return (100.0 * fxtofl(kp->p_pctcpu) /
		(1.0 - d));
}

void
pcpu(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*.1f", v->width, getpcpu(kp));
}

double
getpmem(const struct kinfo_proc *kp)
{
	double fracmem;

	if (mempages == 0)
		return (0.0);

	if (kp->p_flag & P_SYSTEM)
		return (0.0);
	/* XXX don't have info about shared */
	fracmem = ((float)kp->p_vm_rssize)/mempages;
	return (100.0 * fracmem);
}

void
pmem(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*.1f", v->width, getpmem(kp));
}

void
pagein(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*llu", v->width,
	    kp->p_uvalid ? kp->p_uru_majflt : 0);
}

void
maxrss(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*llu", v->width, kp->p_rlim_rss_cur / 1024);
}

void
tsize(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*llu", v->width, pgtok(kp->p_vm_tsize));
}

void
dsize(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*llu", v->width, pgtok(kp->p_vm_dsize));
}

void
ssize(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*llu", v->width, pgtok(kp->p_vm_ssize));
}

/*
 * Generic output routines.  Print fields from various prototype
 * structures.
 */
static void
printval(char *bp, VAR *v)
{
	char ofmt[32];

	snprintf(ofmt, sizeof(ofmt), "%%%s*%s", (v->flag & LJUST) ? "-" : "",
	    v->fmt);

	/*
	 * Note that the "INF127" check is nonsensical for types
	 * that are or can be signed.
	 */
#define	GET(type)		(*(type *)bp)
#define	CHK_INF127(n)		(((n) > 127) && (v->flag & INF127) ? 127 : (n))

	switch (v->type) {
	case INT8:
		(void)printf(ofmt, v->width, GET(int8_t));
		break;
	case UINT8:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_int8_t)));
		break;
	case INT16:
		(void)printf(ofmt, v->width, GET(int16_t));
		break;
	case UINT16:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_int16_t)));
		break;
	case INT32:
		(void)printf(ofmt, v->width, GET(int32_t));
		break;
	case UINT32:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_int32_t)));
		break;
	case INT64:
		(void)printf(ofmt, v->width, GET(int64_t));
		break;
	case UINT64:
		(void)printf(ofmt, v->width, CHK_INF127(GET(u_int64_t)));
		break;
	default:
		errx(1, "unknown type %d", v->type);
	}
#undef GET
#undef CHK_INF127
}

void
pvar(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	if ((v->flag & USER) && !kp->p_uvalid)
		(void)printf("%*s", v->width, "-");
	else
		printval((char *)kp + v->off, v);
}

void
emulname(const struct kinfo_proc *kp, VARENT *ve)
{
	VAR *v;

	v = ve->var;

	(void)printf("%-*s", (int)v->width, kp->p_emul);
}
