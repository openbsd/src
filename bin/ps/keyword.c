/*	$OpenBSD: keyword.c,v 1.15 2002/07/19 14:20:44 drahn Exp $	*/
/*	$NetBSD: keyword.c,v 1.12.6.1 1996/05/30 21:25:13 cgd Exp $	*/

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
static char sccsid[] = "@(#)keyword.c	8.5 (Berkeley) 4/2/94";
#else
static char rcsid[] = "$OpenBSD: keyword.c,v 1.15 2002/07/19 14:20:44 drahn Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>

#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ps.h"

#include <sys/ucred.h>
#include <sys/sysctl.h>

static VAR *findvar(char *);
static int  vcmp(const void *, const void *);

#ifdef NOTINUSE
int	utime(), stime(), ixrss(), idrss(), isrss();
	{{"utime"}, "UTIME", USER, utime, 4},
	{{"stime"}, "STIME", USER, stime, 4},
	{{"ixrss"}, "IXRSS", USER, ixrss, 4},
	{{"idrss"}, "IDRSS", USER, idrss, 4},
	{{"isrss"}, "ISRSS", USER, isrss, 4},
#endif

/* Compute offset in common structures. */
#define	POFF(x)	offsetof(struct proc, x)
#define	EOFF(x)	offsetof(struct eproc, x)
#define	UOFF(x)	offsetof(struct usave, x)
#define	ROFF(x)	offsetof(struct rusage, x)

#define	UIDFMT	"u"
#define	UIDLEN	5
#define	UID(n1, n2, fn, off) \
	{ n1, n2, NULL, 0, fn, UIDLEN, 0, off, UINT32, UIDFMT }
#define	GID(n1, n2, fn, off)	UID(n1, n2, fn, off)

#define	PIDFMT	"d"
#define	PIDLEN	5
#define	PID(n1, n2, fn, off) \
	{ n1, n2, NULL, 0, fn, PIDLEN, 0, off, INT32, PIDFMT }

#define	USERLEN	8

VAR var[] = {
	{"%cpu", "%CPU", NULL, 0, pcpu, 4},
	{"%mem", "%MEM", NULL, 0, pmem, 4},
	{"acflag", "ACFLG", NULL, 0, pvar, 3, 0, POFF(p_acflag), USHORT, "x"},
	{"acflg", "", "acflag"},
	{"args", "", "command"},
	{"blocked", "", "sigmask"},
	{"caught", "", "sigcatch"},
	{"comm", "", "ucomm"},
	{"command", "COMMAND", NULL, COMM|LJUST|USER, command, 16},
	{"cpu", "CPU", NULL, 0, pvar, 3, 0, POFF(p_estcpu), UINT, "d"},
	{"cputime", "", "time"},
	{"emul", "EMUL", NULL, LJUST, emulname, EMULNAMELEN},
	{"etime", "", "start"},
	{"f", "F", NULL, 0, pvar, 7, 0, POFF(p_flag), INT, "x"},
	{"flags", "", "f"},
	GID("gid", "GID", evar, EOFF(e_ucred.cr_gid)),
	{"group", "GROUP", NULL, LJUST, gname, USERLEN},
	{"holdcnt", "HOLDCNT", NULL, 0, pvar, 8, 0, POFF(p_holdcnt), INT, "d"},
	{"ignored", "", "sigignore"},
	{"inblk", "INBLK", NULL, USER, rvar, 4, 0, ROFF(ru_inblock), LONG, "d"},
	{"inblock", "", "inblk"},
	{"jobc", "JOBC", NULL, 0, evar, 4, 0, EOFF(e_jobc), SHORT, "d"},
	{"ktrace", "KTRACE", NULL, 0, pvar, 8, 0, POFF(p_traceflag), INT, "x"},
	/* XXX */
	{"ktracep", "KTRACEP", NULL, 0, pvar, 8, 0, POFF(p_tracep), KPTR, "x"},
	{"lim", "LIM", NULL, 0, maxrss, 5},
	{"login", "LOGIN", NULL, LJUST, logname, MAXLOGNAME},
	{"logname", "", "login"},
	{"lstart", "STARTED", NULL, LJUST|USER, lstarted, 28},
	{"majflt", "MAJFLT", NULL, USER, rvar, 4, 0, ROFF(ru_majflt), LONG, "d"},
	{"minflt", "MINFLT", NULL, USER, rvar, 4, 0, ROFF(ru_minflt), LONG, "d"},
	{"msgrcv", "MSGRCV", NULL, USER, rvar, 4, 0, ROFF(ru_msgrcv), LONG, "d"},
	{"msgsnd", "MSGSND", NULL, USER, rvar, 4, 0, ROFF(ru_msgsnd), LONG, "d"},
	{"ni", "", "nice"},
	{"nice", "NI", NULL, 0, pvar, 2, 0, POFF(p_nice), CHAR, "d"},
	{"nivcsw", "NIVCSW", NULL, USER, rvar, 5, 0, ROFF(ru_nivcsw), LONG, "d"},
	{"nsignals", "", "nsigs"},
	{"nsigs", "NSIGS", NULL, USER, rvar, 4, 0, ROFF(ru_nsignals), LONG, "d"},
	{"nswap", "NSWAP", NULL, USER, rvar, 4, 0, ROFF(ru_nswap), LONG, "d"},
	{"nvcsw", "NVCSW", NULL, USER, rvar, 5, 0, ROFF(ru_nvcsw), LONG, "d"},
	/* XXX */
	{"nwchan", "WCHAN", NULL, 0, pvar, 6, 0, POFF(p_wchan), KPTR, "x"},
	{"oublk", "OUBLK", NULL, USER, rvar, 4, 0, ROFF(ru_oublock), LONG, "d"},
	{"oublock", "", "oublk"},
	/* XXX */
	{"p_ru", "P_RU", NULL, 0, pvar, 6, 0, POFF(p_ru), KPTR, "x"},
	/* XXX */
	{"paddr", "PADDR", NULL, 0, evar, 6, 0, EOFF(e_paddr), KPTR, "x"},
	{"pagein", "PAGEIN", NULL, USER, pagein, 6},
	{"pcpu", "", "%cpu"},
	{"pending", "", "sig"},
	PID("pgid", "PGID", evar, EOFF(e_pgid)),
	PID("pid", "PID", pvar, POFF(p_pid)),
	{"pmem", "", "%mem"},
	PID("ppid", "PPID", evar, EOFF(e_ppid)),
	{"pri", "PRI", NULL, 0, pri, 3},
	{"re", "RE", NULL, INF127, pvar, 3, 0, POFF(p_swtime), UINT, "d"},
	GID("rgid", "RGID", evar, EOFF(e_pcred.p_rgid)),
	/* XXX */
	{"rgroup", "RGROUP", NULL, LJUST, rgname, USERLEN},
	{"rlink", "RLINK", NULL, 0, pvar, 8, 0, POFF(p_back), KPTR, "x"},
	{"rss", "RSS", NULL, 0, p_rssize, 5},
	{"rssize", "", "rsz"},
	{"rsz", "RSZ", NULL, 0, rssize, 4},
	UID("ruid", "RUID", evar, EOFF(e_pcred.p_ruid)),
	{"ruser", "RUSER", NULL, LJUST, runame, USERLEN},
	{"sess", "SESS", NULL, 0, evar, 6, 0, EOFF(e_sess), KPTR, "x"},
	{"sig", "PENDING", NULL, 0, pvar, 8, 0, POFF(p_siglist), INT, "x"},
	{"sigcatch", "CAUGHT", NULL, 0, pvar, 8, 0, POFF(p_sigcatch), UINT, "x"},
	{"sigignore", "IGNORED",
		NULL, 0, pvar, 8, 0, POFF(p_sigignore), UINT, "x"},
	{"sigmask", "BLOCKED", NULL, 0, pvar, 8, 0, POFF(p_sigmask), UINT, "x"},
	{"sl", "SL", NULL, INF127, pvar, 3, 0, POFF(p_slptime), UINT, "d"},
	{"start", "STARTED", NULL, LJUST|USER, started, 8},
	{"stat", "", "state"},
	{"state", "STAT", NULL, 0, state, 5},
	GID("svgid", "SVGID", evar, EOFF(e_pcred.p_svgid)),
	UID("svuid", "SVUID", evar, EOFF(e_pcred.p_svuid)),
	{"tdev", "TDEV", NULL, 0, tdev, 4},
	{"time", "TIME", NULL, USER, cputime, 9},
	PID("tpgid", "TGPID", evar, EOFF(e_tpgid)),
	{"tsess", "TSESS", NULL, 0, evar, 6, 0, EOFF(e_tsess), KPTR, "x"},
	{"tsiz", "TSIZ", NULL, 0, tsize, 4},
	{"tt", "TT", NULL, LJUST, tname, 3},
	{"tty", "TTY", NULL, LJUST, longtname, 8},
	{"ucomm", "UCOMM", NULL, LJUST, ucomm, MAXCOMLEN},
	UID("uid", "UID", evar, EOFF(e_ucred.cr_uid)),
	{"upr", "UPR", NULL, 0, pvar, 3, 0, POFF(p_usrpri), UCHAR, "d"},
	{"user", "USER", NULL, LJUST, uname, USERLEN},
	{"usrpri", "", "upr"},
	{"vsize", "", "vsz"},
	{"vsz", "VSZ", NULL, 0, vsize, 5},
	{"wchan", "WCHAN", NULL, LJUST, wchan, 6},
	{"xstat", "XSTAT", NULL, 0, pvar, 4, 0, POFF(p_xstat), USHORT, "x"},
	{""},
};

void
showkey()
{
	VAR *v;
	int i;
	char *p, *sep;

	i = 0;
	sep = "";
	for (v = var; *(p = v->name); ++v) {
		int len = strlen(p);
		if (termwidth && (i += len + 1) > termwidth) {
			i = len;
			sep = "\n";
		}
		(void) printf("%s%s", sep, p);
		sep = " ";
	}
	(void) printf("\n");
}

void
parsefmt(p)
	char *p;
{
	static struct varent *vtail;

#define	FMTSEP	" \t,\n"
	while (p && *p) {
		char *cp;
		VAR *v;
		struct varent *vent;

		while ((cp = strsep(&p, FMTSEP)) != NULL && *cp == '\0')
			/* void */;
		if (!cp)
			break;
		if (!(v = findvar(cp)) || v->parsed == 1)
			continue;
		v->parsed = 1;
		if ((vent = malloc(sizeof(struct varent))) == NULL)
			err(1, NULL);
		vent->var = v;
		vent->next = NULL;
		if (vhead == NULL)
			vhead = vtail = vent;
		else {
			vtail->next = vent;
			vtail = vent;
		}
	}
	if (!vhead)
		errx(1, "no valid keywords");
}

static VAR *
findvar(p)
	char *p;
{
	VAR *v, key;
	char *hp;
	int vcmp();

	key.name = p;

	hp = strchr(p, '=');
	if (hp)
		*hp++ = '\0';

	key.name = p;
	v = bsearch(&key, var, sizeof(var)/sizeof(VAR) - 1, sizeof(VAR), vcmp);

	if (v && v->alias) {
		if (hp) {
			warnx("%s: illegal keyword specification", p);
			eval = 1;
		}
		parsefmt(v->alias);
		return (NULL);
	}
	if (!v) {
		warnx("%s: keyword not found", p);
		eval = 1;
		return (NULL);
	}
	if (hp)
		v->header = hp;
	return (v);
}

static int
vcmp(a, b)
	const void *a, *b;
{
	return (strcmp(((VAR *)a)->name, ((VAR *)b)->name));
}
