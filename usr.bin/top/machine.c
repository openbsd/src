/* $OpenBSD: machine.c,v 1.51 2005/12/21 01:40:24 millert Exp $	 */

/*-
 * Copyright (c) 1994 Thorsten Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * AUTHOR:  Thorsten Lockert <tholo@sigmasoft.com>
 *          Adapted from BSD4.4 by Christos Zoulas <christos@ee.cornell.edu>
 *          Patch for process wait display by Jarl F. Greipsland <jarle@idt.unit.no>
 *	    Patch for -DORDER by Kenneth Stailey <kstailey@disclosure.com>
 *	    Patch for new swapctl(2) by Tobias Weingartner <weingart@openbsd.org>
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <err.h>
#include <math.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <sys/dir.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/swap.h>
#include <err.h>
#include <errno.h>

#include "top.h"
#include "display.h"
#include "machine.h"
#include "utils.h"
#include "loadavg.h"

static int	swapmode(int *, int *);

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle {
	struct kinfo_proc2 **next_proc;	/* points to next valid proc pointer */
	int		remaining;	/* number of pointers remaining */
};

/* what we consider to be process size: */
#define PROCSIZE(pp) ((pp)->p_vm_tsize + (pp)->p_vm_dsize + (pp)->p_vm_ssize)

/*
 *  These definitions control the format of the per-process area
 */
static char header[] =
	"  PID X        PRI NICE  SIZE   RES STATE    WAIT     TIME    CPU COMMAND";

/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6

#define Proc_format \
	"%5d %-8.8s %3d %4d %5s %5s %-8s %-6.6s %6s %5.2f%% %.51s"

/* process state names for the "STATE" column of the display */
/*
 * the extra nulls in the string "run" are for adding a slash and the
 * processor number when needed
 */

char	*state_abbrev[] = {
	"", "start", "run", "sleep", "stop", "zomb", "dead", "onproc"
};

static int      stathz;

/* these are for calculating cpu state percentages */
static int64_t     **cp_time;
static int64_t     **cp_old;
static int64_t     **cp_diff;

/* these are for detailing the process states */
int process_states[8];
char *procstatenames[] = {
	"", " starting, ", " running, ", " idle, ",
	" stopped, ", " zombie, ", " dead, ", " on processor, ",
	NULL
};

/* these are for detailing the cpu states */
int64_t *cpu_states;
char *cpustatenames[] = {
	"user", "nice", "system", "interrupt", "idle", NULL
};

/* these are for detailing the memory statistics */
int memory_stats[8];
char *memorynames[] = {
	"Real: ", "K/", "K act/tot  ", "Free: ", "K  ",
	"Swap: ", "K/", "K used/tot",
	NULL
};

/* these are names given to allowed sorting orders -- first is default */
char	*ordernames[] = {
	"cpu", "size", "res", "time", "pri", NULL
};

/* these are for keeping track of the proc array */
static int      nproc;
static int      onproc = -1;
static int      pref_len;
static struct kinfo_proc2 *pbase;
static struct kinfo_proc2 **pref;

/* these are for getting the memory statistics */
static int      pageshift;	/* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */
#define pagetok(size) ((size) << pageshift)

int		ncpu;

unsigned int	maxslp;

static int
getstathz(void)
{
	struct clockinfo cinf;
	size_t size = sizeof(cinf);
	int mib[2];

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	if (sysctl(mib, 2, &cinf, &size, NULL, 0) == -1)
		return (-1);
	return (cinf.stathz);
}

int
machine_init(struct statics *statics)
{
	size_t size = sizeof(ncpu);
	int mib[2], pagesize, cpu;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if (sysctl(mib, 2, &ncpu, &size, NULL, 0) == -1)
		return (-1);
	cpu_states = malloc(ncpu * CPUSTATES * sizeof(int64_t));
	if (cpu_states == NULL)
		err(1, NULL);
	cp_time = malloc(ncpu * sizeof(int64_t *));
	cp_old  = malloc(ncpu * sizeof(int64_t *));
	cp_diff = malloc(ncpu * sizeof(int64_t *));
	if (cp_time == NULL || cp_old == NULL || cp_diff == NULL)
		err(1, NULL);
	for (cpu = 0; cpu < ncpu; cpu++) {
		cp_time[cpu] = malloc(CPUSTATES * sizeof(int64_t));
		cp_old[cpu] = malloc(CPUSTATES * sizeof(int64_t));
		cp_diff[cpu] = malloc(CPUSTATES * sizeof(int64_t));
		if (cp_time[cpu] == NULL || cp_old[cpu] == NULL ||
		    cp_diff[cpu] == NULL)
			err(1, NULL);
	}

	stathz = getstathz();
	if (stathz == -1)
		return (-1);

	pbase = NULL;
	pref = NULL;
	onproc = -1;
	nproc = 0;

	/*
	 * get the page size with "getpagesize" and calculate pageshift from
	 * it
	 */
	pagesize = getpagesize();
	pageshift = 0;
	while (pagesize > 1) {
		pageshift++;
		pagesize >>= 1;
	}

	/* we only need the amount of log(2)1024 for our conversion */
	pageshift -= LOG1024;

	/* fill in the statics information */
	statics->procstate_names = procstatenames;
	statics->cpustate_names = cpustatenames;
	statics->memory_names = memorynames;
	statics->order_names = ordernames;
	return (0);
}

char *
format_header(char *uname_field)
{
	char *ptr;

	ptr = header + UNAME_START;
	while (*uname_field != '\0')
		*ptr++ = *uname_field++;
	return (header);
}

void
get_system_info(struct system_info *si)
{
	static int sysload_mib[] = {CTL_VM, VM_LOADAVG};
	static int vmtotal_mib[] = {CTL_VM, VM_METER};
	struct loadavg sysload;
	struct vmtotal vmtotal;
	double *infoloadp;
	size_t size;
	int i;
	int64_t *tmpstate;

	if (ncpu > 1) {
		size = CPUSTATES * sizeof(int64_t);
		for (i = 0; i < ncpu; i++) {
			int cp_time_mib[] = {CTL_KERN, KERN_CPTIME2, i};
			tmpstate = cpu_states + (CPUSTATES * i);
			if (sysctl(cp_time_mib, 3, cp_time[i], &size, NULL, 0) < 0)
				warn("sysctl kern.cp_time2 failed");
			/* convert cp_time2 counts to percentages */
			(void) percentages(CPUSTATES, tmpstate, cp_time[i],
			    cp_old[i], cp_diff[i]);
		}
	} else {
		int cp_time_mib[] = {CTL_KERN, KERN_CPTIME};
		long cp_time_tmp[CPUSTATES];

		size = sizeof(cp_time_tmp);
		if (sysctl(cp_time_mib, 2, cp_time_tmp, &size, NULL, 0) < 0)
			warn("sysctl kern.cp_time failed");
		for (i = 0; i < CPUSTATES; i++)
			cp_time[0][i] = cp_time_tmp[i];
		/* convert cp_time counts to percentages */
		(void) percentages(CPUSTATES, cpu_states, cp_time[0],
		    cp_old[0], cp_diff[0]);
	}

	size = sizeof(sysload);
	if (sysctl(sysload_mib, 2, &sysload, &size, NULL, 0) < 0)
		warn("sysctl failed");
	infoloadp = si->load_avg;
	for (i = 0; i < 3; i++)
		*infoloadp++ = ((double) sysload.ldavg[i]) / sysload.fscale;


	/* get total -- systemwide main memory usage structure */
	size = sizeof(vmtotal);
	if (sysctl(vmtotal_mib, 2, &vmtotal, &size, NULL, 0) < 0) {
		warn("sysctl failed");
		bzero(&vmtotal, sizeof(vmtotal));
	}
	/* convert memory stats to Kbytes */
	memory_stats[0] = -1;
	memory_stats[1] = pagetok(vmtotal.t_arm);
	memory_stats[2] = pagetok(vmtotal.t_rm);
	memory_stats[3] = -1;
	memory_stats[4] = pagetok(vmtotal.t_free);
	memory_stats[5] = -1;

	if (!swapmode(&memory_stats[6], &memory_stats[7])) {
		memory_stats[6] = 0;
		memory_stats[7] = 0;
	}

	/* set arrays and strings */
	si->cpustates = cpu_states;
	si->memory = memory_stats;
	si->last_pid = -1;
}

static struct handle handle;

struct kinfo_proc2 *
getprocs(int op, int arg, int *cnt)
{
	size_t size;
	int mib[6] = {CTL_KERN, KERN_PROC2, 0, 0, sizeof(struct kinfo_proc2), 0};
	static int maxslp_mib[] = {CTL_VM, VM_MAXSLP};
	static struct kinfo_proc2 *procbase;
	int st;

	mib[2] = op;
	mib[3] = arg;

	size = sizeof(maxslp);
	if (sysctl(maxslp_mib, 2, &maxslp, &size, NULL, 0) < 0) {
		warn("sysctl vm.maxslp failed");
		return (0);
	}
    retry:
	free(procbase);
	st = sysctl(mib, 6, NULL, &size, NULL, 0);
	if (st == -1) {
		/* _kvm_syserr(kd, kd->program, "kvm_getproc2"); */
		return (0);
	}
	size = 5 * size / 4;			/* extra slop */
	if ((procbase = malloc(size)) == NULL)
		return (0);
	mib[5] = (int)(size / sizeof(struct kinfo_proc2));
	st = sysctl(mib, 6, procbase, &size, NULL, 0);
	if (st == -1) {
		if (errno == ENOMEM)
			goto retry;
		/* _kvm_syserr(kd, kd->program, "kvm_getproc2"); */
		return (0);
	}
	*cnt = (int)(size / sizeof(struct kinfo_proc2));
	return (procbase);
}

caddr_t
get_process_info(struct system_info *si, struct process_select *sel,
    int (*compare) (const void *, const void *))
{
	int show_idle, show_system, show_threads, show_uid, show_pid;
	int total_procs, active_procs;
	struct kinfo_proc2 **prefp, *pp;

	if ((pbase = getprocs(KERN_PROC_KTHREAD, 0, &nproc)) == NULL) {
		/* warnx("%s", kvm_geterr(kd)); */
		quit(23);
	}
	if (nproc > onproc)
		pref = (struct kinfo_proc2 **)realloc(pref,
		    sizeof(struct kinfo_proc2 *) * (onproc = nproc));
	if (pref == NULL) {
		warnx("Out of memory.");
		quit(23);
	}
	/* get a pointer to the states summary array */
	si->procstates = process_states;

	/* set up flags which define what we are going to select */
	show_idle = sel->idle;
	show_system = sel->system;
	show_threads = sel->threads;
	show_uid = sel->uid != (uid_t)-1;
	show_pid = sel->pid != (pid_t)-1;

	/* count up process states and get pointers to interesting procs */
	total_procs = 0;
	active_procs = 0;
	memset((char *) process_states, 0, sizeof(process_states));
	prefp = pref;
	for (pp = pbase; pp < &pbase[nproc]; pp++) {
		/*
		 *  Place pointers to each valid proc structure in pref[].
		 *  Process slots that are actually in use have a non-zero
		 *  status field.  Processes with SSYS set are system
		 *  processes---these get ignored unless show_sysprocs is set.
		 */
		if (pp->p_stat != 0 &&
		    (show_system || (pp->p_flag & P_SYSTEM) == 0) &&
		    (show_threads || (pp->p_flag & P_THREAD) == 0)) {
			total_procs++;
			process_states[(unsigned char) pp->p_stat]++;
			if (pp->p_stat != SZOMB &&
			    (show_idle || pp->p_pctcpu != 0 ||
			    pp->p_stat == SRUN) &&
			    (!show_uid || pp->p_ruid == sel->uid) &&
			    (!show_pid || pp->p_pid == sel->pid)) {
				*prefp++ = pp;
				active_procs++;
			}
		}
	}

	/* if requested, sort the "interesting" processes */
	if (compare != NULL)
		qsort((char *) pref, active_procs,
		    sizeof(struct kinfo_proc2 *), compare);
	/* remember active and total counts */
	si->p_total = total_procs;
	si->p_active = pref_len = active_procs;

	/* pass back a handle */
	handle.next_proc = pref;
	handle.remaining = active_procs;
	return ((caddr_t) & handle);
}

char fmt[MAX_COLS];	/* static area where result is built */

char *
state_abbr(struct kinfo_proc2 *pp)
{
	static char buf[10];

	if (ncpu > 1 && pp->p_cpuid != KI_NOCPU)
		snprintf(buf, sizeof buf, "%s/%llu",
		    state_abbrev[(unsigned char)pp->p_stat], pp->p_cpuid);
	else
		snprintf(buf, sizeof buf, "%s",
		    state_abbrev[(unsigned char)pp->p_stat]);
	return buf;
}

char *
format_comm(struct kinfo_proc2 *kp)
{
#define ARG_SIZE 60
	static char **s, buf[ARG_SIZE];
	size_t siz = 100;
	char **p;
	int mib[4];
	extern int show_args;

	if (!show_args)
		return (kp->p_comm);

	for (;; siz *= 2) {
		if ((s = realloc(s, siz)) == NULL)
			err(1, NULL);
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC_ARGS;
		mib[2] = kp->p_pid;
		mib[3] = KERN_PROC_ARGV;
		if (sysctl(mib, 4, s, &siz, NULL, 0) == 0)
			break;
		if (errno != ENOMEM)
			return (kp->p_comm);
	}
	buf[0] = '\0';
	for (p = s; *p != NULL; p++) {
		if (p != s)
			strlcat(buf, " ", sizeof(buf));
		strlcat(buf, *p, sizeof(buf));
	}
	if (buf[0] == '\0')
		return (kp->p_comm);
	return (buf);
}

char *
format_next_process(caddr_t handle, char *(*get_userid)(uid_t))
{
	char *p_wait, waddr[sizeof(void *) * 2 + 3];	/* Hexify void pointer */
	struct kinfo_proc2 *pp;
	struct handle *hp;
	int cputime;
	double pct;

	/* find and remember the next proc structure */
	hp = (struct handle *) handle;
	pp = *(hp->next_proc++);
	hp->remaining--;

	if ((pp->p_flag & P_INMEM) == 0) {
		/*
		 * Print swapped processes as <pname>
		 */
		char buf[sizeof(pp->p_comm)];

		(void) strlcpy(buf, pp->p_comm, sizeof(buf));
		(void) snprintf(pp->p_comm, sizeof(pp->p_comm), "<%s>", buf);
	}
	cputime = (pp->p_uticks + pp->p_sticks + pp->p_iticks) / stathz;

	/* calculate the base for cpu percentages */
	pct = pctdouble(pp->p_pctcpu);

	if (pp->p_wchan) {
		if (pp->p_wmesg)
			p_wait = pp->p_wmesg;
		else {
			snprintf(waddr, sizeof(waddr), "%llx",
			    pp->p_wchan & ~KERNBASE);
			p_wait = waddr;
		}
	} else
		p_wait = "-";

	/* format this entry */
	snprintf(fmt, sizeof fmt, Proc_format,
	    pp->p_pid, (*get_userid)(pp->p_ruid),
	    pp->p_priority - PZERO, pp->p_nice - NZERO,
	    format_k(pagetok(PROCSIZE(pp))),
	    format_k(pagetok(pp->p_vm_rssize)),
	    (pp->p_stat == SSLEEP && pp->p_slptime > maxslp) ?
	    "idle" : state_abbr(pp),
	    p_wait, format_time(cputime), 100.0 * pct,
	    printable(format_comm(pp)));

	/* return the result */
	return (fmt);
}

/* comparison routine for qsort */
static unsigned char sorted_state[] =
{
	0,			/* not used		 */
	4,			/* start		 */
	5,			/* run			 */
	2,			/* sleep		 */
	3,			/* stop			 */
	1			/* zombie		 */
};

/*
 *  proc_compares - comparison functions for "qsort"
 */

/*
 * First, the possible comparison keys.  These are defined in such a way
 * that they can be merely listed in the source code to define the actual
 * desired ordering.
 */

#define ORDERKEY_PCTCPU \
	if (lresult = (pctcpu)p2->p_pctcpu - (pctcpu)p1->p_pctcpu, \
	    (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)
#define ORDERKEY_CPUTIME \
	if ((result = p2->p_rtime_sec - p1->p_rtime_sec) == 0) \
		if ((result = p2->p_rtime_usec - p1->p_rtime_usec) == 0)
#define ORDERKEY_STATE \
	if ((result = sorted_state[(unsigned char)p2->p_stat] - \
	    sorted_state[(unsigned char)p1->p_stat])  == 0)
#define ORDERKEY_PRIO \
	if ((result = p2->p_priority - p1->p_priority) == 0)
#define ORDERKEY_RSSIZE \
	if ((result = p2->p_vm_rssize - p1->p_vm_rssize) == 0)
#define ORDERKEY_MEM \
	if ((result = PROCSIZE(p2) - PROCSIZE(p1)) == 0)

/* compare_cpu - the comparison function for sorting by cpu percentage */
static int
compare_cpu(const void *v1, const void *v2)
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc2 *p1, *p2;
	pctcpu lresult;
	int result;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc2 **) pp1;
	p2 = *(struct kinfo_proc2 **) pp2;

	ORDERKEY_PCTCPU
	ORDERKEY_CPUTIME
	ORDERKEY_STATE
	ORDERKEY_PRIO
	ORDERKEY_RSSIZE
	ORDERKEY_MEM
		;
	return (result);
}

/* compare_size - the comparison function for sorting by total memory usage */
static int
compare_size(const void *v1, const void *v2)
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc2 *p1, *p2;
	pctcpu lresult;
	int result;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc2 **) pp1;
	p2 = *(struct kinfo_proc2 **) pp2;

	ORDERKEY_MEM
	ORDERKEY_RSSIZE
	ORDERKEY_PCTCPU
	ORDERKEY_CPUTIME
	ORDERKEY_STATE
	ORDERKEY_PRIO
		;
	return (result);
}

/* compare_res - the comparison function for sorting by resident set size */
static int
compare_res(const void *v1, const void *v2)
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc2 *p1, *p2;
	pctcpu lresult;
	int result;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc2 **) pp1;
	p2 = *(struct kinfo_proc2 **) pp2;

	ORDERKEY_RSSIZE
	ORDERKEY_MEM
	ORDERKEY_PCTCPU
	ORDERKEY_CPUTIME
	ORDERKEY_STATE
	ORDERKEY_PRIO
		;
	return (result);
}

/* compare_time - the comparison function for sorting by CPU time */
static int
compare_time(const void *v1, const void *v2)
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc2 *p1, *p2;
	pctcpu lresult;
	int result;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc2 **) pp1;
	p2 = *(struct kinfo_proc2 **) pp2;

	ORDERKEY_CPUTIME
	ORDERKEY_PCTCPU
	ORDERKEY_STATE
	ORDERKEY_PRIO
	ORDERKEY_MEM
	ORDERKEY_RSSIZE
		;
	return (result);
}

/* compare_prio - the comparison function for sorting by CPU time */
static int
compare_prio(const void *v1, const void *v2)
{
	struct proc   **pp1 = (struct proc **) v1;
	struct proc   **pp2 = (struct proc **) v2;
	struct kinfo_proc2 *p1, *p2;
	pctcpu lresult;
	int result;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc2 **) pp1;
	p2 = *(struct kinfo_proc2 **) pp2;

	ORDERKEY_PRIO
	ORDERKEY_PCTCPU
	ORDERKEY_CPUTIME
	ORDERKEY_STATE
	ORDERKEY_RSSIZE
	ORDERKEY_MEM
		;
	return (result);
}

int (*proc_compares[])(const void *, const void *) = {
	compare_cpu,
	compare_size,
	compare_res,
	compare_time,
	compare_prio,
	NULL
};

/*
 * proc_owner(pid) - returns the uid that owns process "pid", or -1 if
 *		the process does not exist.
 *		It is EXTREMELY IMPORTANT that this function work correctly.
 *		If top runs setuid root (as in SVR4), then this function
 *		is the only thing that stands in the way of a serious
 *		security problem.  It validates requests for the "kill"
 *		and "renice" commands.
 */
uid_t
proc_owner(pid_t pid)
{
	struct kinfo_proc2 **prefp, *pp;
	int cnt;

	prefp = pref;
	cnt = pref_len;
	while (--cnt >= 0) {
		pp = *prefp++;
		if (pp->p_pid == pid)
			return ((uid_t)pp->p_ruid);
	}
	return (uid_t)(-1);
}

/*
 * swapmode is rewritten by Tobias Weingartner <weingart@openbsd.org>
 * to be based on the new swapctl(2) system call.
 */
static int
swapmode(int *used, int *total)
{
	struct swapent *swdev;
	int nswap, rnswap, i;

	nswap = swapctl(SWAP_NSWAP, 0, 0);
	if (nswap == 0)
		return 0;

	swdev = malloc(nswap * sizeof(*swdev));
	if (swdev == NULL)
		return 0;

	rnswap = swapctl(SWAP_STATS, swdev, nswap);
	if (rnswap == -1)
		return 0;

	/* if rnswap != nswap, then what? */

	/* Total things up */
	*total = *used = 0;
	for (i = 0; i < nswap; i++) {
		if (swdev[i].se_flags & SWF_ENABLE) {
			*used += (swdev[i].se_inuse / (1024 / DEV_BSIZE));
			*total += (swdev[i].se_nblks / (1024 / DEV_BSIZE));
		}
	}
	free(swdev);
	return 1;
}
