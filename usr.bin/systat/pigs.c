/*	$OpenBSD: pigs.c,v 1.31 2018/09/13 15:23:32 millert Exp $	*/
/*	$NetBSD: pigs.c,v 1.3 1995/04/29 05:54:50 cgd Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
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
 * Pigs display from Bill Reeves at Lucasfilm
 */

#include <sys/param.h>	/* MAXCOMLEN */
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <curses.h>
#include <math.h>
#include <pwd.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "systat.h"

int compar(const void *, const void *);
void print_pg(void);
int read_pg(void);
int select_pg(void);
void showpigs(int k);

static struct kinfo_proc *procbase = NULL;
static int nproc, pigs_cnt, *pb_indices = NULL;
static int onproc = -1;

static long stime[CPUSTATES];
static double  lccpu;
struct loadavg sysload;



field_def fields_pg[] = {
	{"USER", 6, 16, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"NAME", 10, 24, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"PID", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"CPU", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 30, 60, 1, FLD_ALIGN_BAR, -1, 0, 0, 100},
};

#define FLD_PG_USER	FIELD_ADDR(fields_pg,0)
#define FLD_PG_NAME	FIELD_ADDR(fields_pg,1)
#define FLD_PG_PID	FIELD_ADDR(fields_pg,2)
#define FLD_PG_VALUE	FIELD_ADDR(fields_pg,3)
#define FLD_PG_BAR	FIELD_ADDR(fields_pg,4)

/* Define views */
field_def *view_pg_0[] = {
	FLD_PG_PID, FLD_PG_USER, FLD_PG_NAME, FLD_PG_VALUE, FLD_PG_BAR, NULL
};


/* Define view managers */
struct view_manager pigs_mgr = {
	"Pigs", select_pg, read_pg, NULL, print_header,
	print_pg, keyboard_callback, NULL, NULL
};

field_view views_pg[] = {
	{view_pg_0, "pigs", '5', &pigs_mgr},
	{NULL, NULL, 0, NULL}
};

int	fscale;

#define pctdouble(p) ((double)(p) / fscale)

typedef long pctcpu;

int
select_pg(void)
{
	int mib[] = { CTL_KERN, KERN_FSCALE };
	size_t size = sizeof(fscale);

        if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
            &fscale, &size, NULL, 0) < 0)
                return (-1);
	num_disp = pigs_cnt;
	return (0);
}


int
getprocs(void)
{
	size_t size;
	int mib[6] = {CTL_KERN, KERN_PROC, KERN_PROC_KTHREAD, 0, sizeof(struct kinfo_proc), 0};
	
	int st;

	free(procbase);
	procbase = NULL;

	st = sysctl(mib, 6, NULL, &size, NULL, 0);
	if (st == -1)
		return (1);

	size = 5 * size / 4;		/* extra slop */
	if ((procbase = malloc(size + 1)) == NULL)
		return (1);

	mib[5] = (int)(size / sizeof(struct kinfo_proc));
	st = sysctl(mib, 6, procbase, &size, NULL, 0);
	if (st == -1)
		return (1);

	nproc = (int)(size / sizeof(struct kinfo_proc));
	return (0);
}


int
read_pg(void)
{
	static int cp_time_mib[] = { CTL_KERN, KERN_CPTIME };
	long ctimes[CPUSTATES];
	double t;
	int i, k;
	size_t size;

	num_disp = pigs_cnt = 0;

	if (getprocs()) {
		error("Failed to read process info!");
		return 1;
	}

	if (nproc > onproc) {
		int *p;
		p = reallocarray(pb_indices, nproc + 1, sizeof(int));
		if (p == NULL) {
			error("Out of Memory!");
			return 1;
		}
		pb_indices = p;
		onproc = nproc;
	}

	memset(&procbase[nproc], 0, sizeof(*procbase));

	for (i = 0; i <= nproc; i++)
		pb_indices[i] = i;

	/*
	 * and for the imaginary "idle" process
	 */
	size = sizeof(ctimes);
	sysctl(cp_time_mib, 2, &ctimes, &size, NULL, 0);

	t = 0;
	for (i = 0; i < CPUSTATES; i++)
		t += ctimes[i] - stime[i];
	if (t == 0.0)
		t = 1.0;

	procbase[nproc].p_pctcpu = (ctimes[CP_IDLE] - stime[CP_IDLE]) / t / pctdouble(1);
	for (i = 0; i < CPUSTATES; i++)
		stime[i] = ctimes[i];

	qsort(pb_indices, nproc + 1, sizeof (int), compar);

	pigs_cnt = 0;
	for (k = 0; k < nproc + 1; k++) {
		int j = pb_indices[k];
		if (pctdouble(procbase[j].p_pctcpu) < 0.01)
			break;
		pigs_cnt++;
	}

	num_disp = pigs_cnt;
	return 0;
}


void
print_pg(void)
{
	int n, count = 0;

	for (n = dispstart; n < num_disp; n++) {
		showpigs(pb_indices[n]);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}
}

int
initpigs(void)
{
	static int sysload_mib[] = {CTL_VM, VM_LOADAVG};
	static int cp_time_mib[] = { CTL_KERN, KERN_CPTIME };
	static int ccpu_mib[] = { CTL_KERN, KERN_CCPU };
	field_view *v;
	size_t size;
	fixpt_t ccpu;

	size = sizeof(stime);
	sysctl(cp_time_mib, 2, &stime, &size, NULL, 0);

	size = sizeof(sysload);
	sysctl(sysload_mib, 2, &sysload, &size, NULL, 0);

	size = sizeof(ccpu);
	sysctl(ccpu_mib, 2, &ccpu, &size, NULL, 0);

	lccpu = log((double) ccpu / sysload.fscale);

	for (v = views_pg; v->name != NULL; v++)
		add_view(v);

	return(1);
}

void
showpigs(int k)
{
	struct kinfo_proc *kp;
	double value;
	const char *uname, *pname;

	if (procbase == NULL)
		return;

	value = pctdouble(procbase[k].p_pctcpu) * 100;

	kp = &procbase[k];
	if (kp->p_comm[0] == '\0') {
		uname = "";
		pname = "<idle>";
	} else {
		uname = user_from_uid(kp->p_uid, 0);
		pname = kp->p_comm;
		print_fld_uint(FLD_PG_PID, kp->p_pid);
	}

	tb_start();
	tbprintf("%.2f", value);
	print_fld_tb(FLD_PG_VALUE);

	print_fld_str(FLD_PG_NAME, pname);
	print_fld_str(FLD_PG_USER, uname);
	print_fld_bar(FLD_PG_BAR, value);

	end_line();
}


int
compar(const void *a, const void *b)
{
	int i1 = *((int *)a);
	int i2 = *((int *)b);

	return procbase[i1].p_pctcpu > 
		procbase[i2].p_pctcpu ? -1 : 1;
}

