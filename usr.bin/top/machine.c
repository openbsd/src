/*	$OpenBSD: machine.c,v 1.26 2001/12/05 02:29:19 art Exp $	*/

/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  For an OpenBSD system
 *
 * DESCRIPTION:
 * This is the machine-dependent module for OpenBSD
 * Tested on:
 *	i386
 *
 * TERMCAP: -ltermlib
 *
 * CFLAGS: -DHAVE_GETOPT -DORDER
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

#define DOSWAP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <err.h>
#include <math.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/dir.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef DOSWAP
#include <sys/swap.h>
#include <err.h>
#endif

static int swapmode __P((int *, int *));

#include "top.h"
#include "display.h"
#include "machine.h"
#include "utils.h"

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle {
	struct kinfo_proc **next_proc;	/* points to next valid proc pointer */
	int     remaining;	/* number of pointers remaining */
};

/* declarations for load_avg */
#include "loadavg.h"

#define PP(pp, field) ((pp)->kp_proc . field)
#define EP(pp, field) ((pp)->kp_eproc . field)
#define VP(pp, field) ((pp)->kp_eproc.e_vm . field)

/* what we consider to be process size: */
#define PROCSIZE(pp) (VP((pp), vm_tsize) + VP((pp), vm_dsize) + VP((pp), vm_ssize))

/*
 *  These definitions control the format of the per-process area
 */
static char header[] =
"  PID X        PRI NICE  SIZE   RES STATE WAIT     TIME    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6

#define Proc_format \
	"%5d %-8.8s %3d %4d %5s %5s %-5s %-6.6s %6s %5.2f%% %.14s"


/* process state names for the "STATE" column of the display */
/* the extra nulls in the string "run" are for adding a slash and
   the processor number when needed */

char *state_abbrev[] = {
	"", "start", "run\0\0\0", "sleep", "stop", "zomb",
};


static int stathz;

/* these are for calculating cpu state percentages */
static long cp_time[CPUSTATES];
static long cp_old[CPUSTATES];
static long cp_diff[CPUSTATES];

/* these are for detailing the process states */
int     process_states[7];
char   *procstatenames[] = {
	"", " starting, ", " running, ", " idle, ", " stopped, ", " zombie, ",
	NULL
};

/* these are for detailing the cpu states */
int     cpu_states[CPUSTATES];
char   *cpustatenames[] = {
	"user", "nice", "system", "interrupt", "idle", NULL
};

/* these are for detailing the memory statistics */
int     memory_stats[8];
char   *memorynames[] = {
	"Real: ", "K/", "K act/tot  ", "Free: ", "K  ",
#ifdef DOSWAP
	"Swap: ", "K/", "K used/tot",
#endif
	NULL
};

#ifdef ORDER
/* these are names given to allowed sorting orders -- first is default */
char   *ordernames[] = {"cpu", "size", "res", "time", "pri", NULL};
#endif

/* these are for keeping track of the proc array */
static int nproc;
static int onproc = -1;
static int pref_len;
static struct kinfo_proc *pbase;
static struct kinfo_proc **pref;

/* these are for getting the memory statistics */
static int pageshift;		/* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */
#define pagetok(size) ((size) << pageshift)

int maxslp;

int
getstathz()
{
	struct clockinfo cinf;
	size_t  size = sizeof(cinf);
	int     mib[2];

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	if (sysctl(mib, 2, &cinf, &size, NULL, 0) == -1)
		return (-1);
	return (cinf.stathz);
}

int
machine_init(statics)
	struct statics *statics;
{
	int pagesize;

	stathz = getstathz();
	if (stathz == -1)
		return (-1);

	pbase = NULL;
	pref = NULL;
	onproc = -1;
	nproc = 0;

	/* get the page size with "getpagesize" and calculate pageshift from
	 * it */
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
#ifdef ORDER
	statics->order_names = ordernames;
#endif
	return (0);
}

char *
format_header(uname_field)
	char   *uname_field;
{
	char *ptr;

	ptr = header + UNAME_START;
	while (*uname_field != '\0') {
		*ptr++ = *uname_field++;
	}
	return (header);
}

void
get_system_info(si)
	struct system_info *si;
{
	static int sysload_mib[] = {CTL_VM, VM_LOADAVG};
	static int vmtotal_mib[] = {CTL_VM, VM_METER};
	static int cp_time_mib[] = { CTL_KERN, KERN_CPTIME };
	struct loadavg sysload;
	struct vmtotal vmtotal;
	double *infoloadp;
	int total, i;
	size_t  size;
	
	size = sizeof(cp_time);
	if (sysctl(cp_time_mib, 2, &cp_time, &size, NULL, 0) < 0) {
		warn("sysctl kern.cp_time failed");
		total = 0;
	}

	size = sizeof(sysload);
	if (sysctl(sysload_mib, 2, &sysload, &size, NULL, 0) < 0) {
		warn("sysctl failed");
		total = 0;
	}
	infoloadp = si->load_avg;
	for (i = 0; i < 3; i++)
		*infoloadp++ = ((double) sysload.ldavg[i]) / sysload.fscale;

	/* convert cp_time counts to percentages */
	total = percentages(CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

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
#ifdef DOSWAP
	if (!swapmode(&memory_stats[6], &memory_stats[7])) {
		memory_stats[6] = 0;
		memory_stats[7] = 0;
	}
#endif

	/* set arrays and strings */
	si->cpustates = cpu_states;
	si->memory = memory_stats;
	si->last_pid = -1;
}

static struct handle handle;

struct kinfo_proc *
getprocs(op, arg, cnt)
	int op, arg;
	int *cnt;
{
	size_t size = sizeof(int);
	int mib[4] = {CTL_KERN, KERN_PROC, op, arg};
	int smib[2] = {CTL_KERN, KERN_NPROCS};
	static int maxslp_mib[] = {CTL_VM, VM_MAXSLP};
	static struct kinfo_proc *procbase;
	int st;

	size = sizeof(maxslp);
	if (sysctl(maxslp_mib, 2, &maxslp, &size, NULL, 0) < 0) {
		warn("sysctl vm.maxslp failed");
		return (0);
	}

	st = sysctl(smib, 2, cnt, &size, NULL, 0);
	if (st == -1) {
		/* _kvm_syserr(kd, kd->program, "kvm_getprocs"); */
		return (0);
	}
	if (procbase)
		free(procbase);
	size = (6 * (*cnt) * sizeof(struct kinfo_proc)) / 5;
	procbase = (struct kinfo_proc *)malloc(size);
	if (procbase == NULL)
		return (0);
	st = sysctl(mib, 4, procbase, &size, NULL, 0);
	if (st == -1) {
		/* _kvm_syserr(kd, kd->program, "kvm_getprocs"); */
		return (0);
	}
	if (size % sizeof(struct kinfo_proc) != 0) {
		/* _kvm_err(kd, kd->program,
		    "proc size mismatch (%d total, %d chunks)",
		    size, sizeof(struct kinfo_proc)); */
		return (0);
	}
	return (procbase);
}

caddr_t 
get_process_info(si, sel, compare)
	struct system_info *si;
	struct process_select *sel;
	int (*compare) __P((const void *, const void *));

{
	int show_idle, show_system, show_uid, show_command;
	int total_procs, active_procs, i;
	struct kinfo_proc **prefp, *pp;

	if ((pbase = getprocs(KERN_PROC_KTHREAD, 0, &nproc)) == NULL) {
		/* warnx("%s", kvm_geterr(kd)); */
		quit(23);
	}
	if (nproc > onproc)
		pref = (struct kinfo_proc **) realloc(pref, sizeof(struct kinfo_proc *)
		    * (onproc = nproc));
	if (pref == NULL) {
		warnx("Out of memory.");
		quit(23);
	}
	/* get a pointer to the states summary array */
	si->procstates = process_states;

	/* set up flags which define what we are going to select */
	show_idle = sel->idle;
	show_system = sel->system;
	show_uid = sel->uid != -1;
	show_command = sel->command != NULL;

	/* count up process states and get pointers to interesting procs */
	total_procs = 0;
	active_procs = 0;
	memset((char *) process_states, 0, sizeof(process_states));
	prefp = pref;
	for (pp = pbase, i = 0; i < nproc; pp++, i++) {
		/*
		 *  Place pointers to each valid proc structure in pref[].
		 *  Process slots that are actually in use have a non-zero
		 *  status field.  Processes with SSYS set are system
		 *  processes---these get ignored unless show_sysprocs is set.
		 */
		if (PP(pp, p_stat) != 0 &&
		    (show_system || ((PP(pp, p_flag) & P_SYSTEM) == 0))) {
			total_procs++;
			process_states[(unsigned char) PP(pp, p_stat)]++;
			if ((PP(pp, p_stat) != SZOMB) &&
			    (show_idle || (PP(pp, p_pctcpu) != 0) ||
				(PP(pp, p_stat) == SRUN)) &&
			    (!show_uid || EP(pp, e_pcred.p_ruid) == (uid_t) sel->uid)) {
				*prefp++ = pp;
				active_procs++;
			}
		}
	}

	/* if requested, sort the "interesting" processes */
	if (compare != NULL) {
		qsort((char *) pref, active_procs, sizeof(struct kinfo_proc *), compare);
	}
	/* remember active and total counts */
	si->p_total = total_procs;
	si->p_active = pref_len = active_procs;

	/* pass back a handle */
	handle.next_proc = pref;
	handle.remaining = active_procs;
	return ((caddr_t) & handle);
}

char    fmt[MAX_COLS];		/* static area where result is built */

char *
format_next_process(handle, get_userid)
	caddr_t handle;
	char *(*get_userid)();

{
	char waddr[sizeof(void *) * 2 + 3];	/* Hexify void pointer */
	struct kinfo_proc *pp;
	struct handle *hp;
	char *p_wait;
	int cputime;
	double pct;

	/* find and remember the next proc structure */
	hp = (struct handle *) handle;
	pp = *(hp->next_proc++);
	hp->remaining--;

	/* get the process's user struct and set cputime */
	if ((PP(pp, p_flag) & P_INMEM) == 0) {
		/*
		 * Print swapped processes as <pname>
		 */
		char   *comm = PP(pp, p_comm);
#define COMSIZ sizeof(PP(pp, p_comm))
		char    buf[COMSIZ];
		(void) strncpy(buf, comm, COMSIZ);
		comm[0] = '<';
		(void) strncpy(&comm[1], buf, COMSIZ - 2);
		comm[COMSIZ - 2] = '\0';
		(void) strncat(comm, ">", COMSIZ - 1);
		comm[COMSIZ - 1] = '\0';
	}
	cputime = (PP(pp, p_uticks) + PP(pp, p_sticks) + PP(pp, p_iticks)) / stathz;

	/* calculate the base for cpu percentages */
	pct = pctdouble(PP(pp, p_pctcpu));

	if (PP(pp, p_wchan))
		if (PP(pp, p_wmesg))
			p_wait = EP(pp, e_wmesg);
		else {
			snprintf(waddr, sizeof(waddr), "%lx",
			    (unsigned long) (PP(pp, p_wchan)) & ~KERNBASE);
			p_wait = waddr;
		}
	else
		p_wait = "-";

	/* format this entry */
	snprintf(fmt, MAX_COLS,
	    Proc_format,
	    PP(pp, p_pid),
	    (*get_userid) (EP(pp, e_pcred.p_ruid)),
	    PP(pp, p_priority) - PZERO,
	    PP(pp, p_nice) - NZERO,
	    format_k(pagetok(PROCSIZE(pp))),
	    format_k(pagetok(VP(pp, vm_rssize))),
	    (PP(pp, p_stat) == SSLEEP && PP(pp, p_slptime) > maxslp)
	    ? "idle" : state_abbrev[(unsigned char) PP(pp, p_stat)],
	    p_wait,
	    format_time(cputime),
	    100.0 * pct,
	    printable(PP(pp, p_comm)));

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
#ifdef ORDER

/*
 *  proc_compares - comparison functions for "qsort"
 */

/*
 * First, the possible comparison keys.  These are defined in such a way
 * that they can be merely listed in the source code to define the actual
 * desired ordering.
 */


#define ORDERKEY_PCTCPU \
	if (lresult = (pctcpu)PP(p2, p_pctcpu) - (pctcpu)PP(p1, p_pctcpu), \
	    (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)
#define ORDERKEY_CPUTIME \
	if ((result = PP(p2, p_rtime.tv_sec) - PP(p1, p_rtime.tv_sec)) == 0) \
		if ((result = PP(p2, p_rtime.tv_usec) - \
		     PP(p1, p_rtime.tv_usec)) == 0)
#define ORDERKEY_STATE \
	if ((result = sorted_state[(unsigned char) PP(p2, p_stat)] - \
	    sorted_state[(unsigned char) PP(p1, p_stat)])  == 0)
#define ORDERKEY_PRIO \
	if ((result = PP(p2, p_priority) - PP(p1, p_priority)) == 0)
#define ORDERKEY_RSSIZE \
	if ((result = VP(p2, vm_rssize) - VP(p1, vm_rssize)) == 0)
#define ORDERKEY_MEM \
	if ((result = PROCSIZE(p2) - PROCSIZE(p1)) == 0)


/* compare_cpu - the comparison function for sorting by cpu percentage */
int
compare_cpu(v1, v2)
	const void *v1, *v2;
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc *p1;
	struct kinfo_proc *p2;
	int result;
	pctcpu lresult;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc **) pp1;
	p2 = *(struct kinfo_proc **) pp2;

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
int
compare_size(v1, v2)
	const void *v1, *v2;
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc *p1;
	struct kinfo_proc *p2;
	int result;
	pctcpu lresult;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc **) pp1;
	p2 = *(struct kinfo_proc **) pp2;

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
int
compare_res(v1, v2)
	const void *v1, *v2;
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc *p1;
	struct kinfo_proc *p2;
	int result;
	pctcpu lresult;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc **) pp1;
	p2 = *(struct kinfo_proc **) pp2;

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
int
compare_time(v1, v2)
	const void *v1, *v2;
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc *p1;
	struct kinfo_proc *p2;
	int result;
	pctcpu lresult;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc **) pp1;
	p2 = *(struct kinfo_proc **) pp2;

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
int
compare_prio(v1, v2)
	const void *v1, *v2;
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc *p1;
	struct kinfo_proc *p2;
	int result;
	pctcpu lresult;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc **) pp1;
	p2 = *(struct kinfo_proc **) pp2;

	ORDERKEY_PRIO
	    ORDERKEY_PCTCPU
	    ORDERKEY_CPUTIME
	    ORDERKEY_STATE
	    ORDERKEY_RSSIZE
	    ORDERKEY_MEM
	    ;
	return (result);
}

int     (*proc_compares[]) () = {
	compare_cpu,
	compare_size,
	compare_res,
	compare_time,
	compare_prio,
	NULL
};
#else
/*
 *  proc_compare - comparison function for "qsort"
 *	Compares the resource consumption of two processes using five
 *  	distinct keys.  The keys (in descending order of importance) are:
 *  	percent cpu, cpu ticks, state, resident set size, total virtual
 *  	memory usage.  The process states are ordered as follows (from least
 *  	to most important):  zombie, sleep, stop, start, run.  The array
 *  	declaration below maps a process state index into a number that
 *  	reflects this ordering.
 */
int
proc_compare(v1, v2)
	const void *v1, *v2;
{
	struct proc **pp1 = (struct proc **) v1;
	struct proc **pp2 = (struct proc **) v2;
	struct kinfo_proc *p1;
	struct kinfo_proc *p2;
	int result;
	pctcpu lresult;

	/* remove one level of indirection */
	p1 = *(struct kinfo_proc **) pp1;
	p2 = *(struct kinfo_proc **) pp2;

	/* compare percent cpu (pctcpu) */
	if ((lresult = PP(p2, p_pctcpu) - PP(p1, p_pctcpu)) == 0) {
		/* use CPU usage to break the tie */
		if ((result = PP(p2, p_rtime).tv_sec - PP(p1, p_rtime).tv_sec) == 0) {
			/* use process state to break the tie */
			if ((result = sorted_state[(unsigned char) PP(p2, p_stat)] -
				sorted_state[(unsigned char) PP(p1, p_stat)]) == 0) {
				/* use priority to break the tie */
				if ((result = PP(p2, p_priority) -
				    PP(p1, p_priority)) == 0) {
					/* use resident set size (rssize) to
					 * break the tie */
					if ((result = VP(p2, vm_rssize) -
					    VP(p1, vm_rssize)) == 0) {
						/* use total memory to break
						 * the tie */
						result = PROCSIZE(p2) - PROCSIZE(p1);
					}
				}
			}
		}
	} else {
		result = lresult < 0 ? -1 : 1;
	}
	return (result);
}
#endif

/*
 * proc_owner(pid) - returns the uid that owns process "pid", or -1 if
 *		the process does not exist.
 *		It is EXTREMLY IMPORTANT that this function work correctly.
 *		If top runs setuid root (as in SVR4), then this function
 *		is the only thing that stands in the way of a serious
 *		security problem.  It validates requests for the "kill"
 *		and "renice" commands.
 */
int 
proc_owner(pid)
	pid_t   pid;
{
	struct kinfo_proc **prefp, *pp;
	int cnt;

	prefp = pref;
	cnt = pref_len;
	while (--cnt >= 0) {
		pp = *prefp++;
		if (PP(pp, p_pid) == pid) {
			return ((int) EP(pp, e_pcred.p_ruid));
		}
	}
	return (-1);
}
#ifdef DOSWAP
/*
 * swapmode is rewritten by Tobias Weingartner <weingart@openbsd.org>
 * to be based on the new swapctl(2) system call.
 */
static int
swapmode(used, total)
	int    *used;
	int    *total;
{
	int     nswap, rnswap, i;
	struct swapent *swdev;

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
#endif
