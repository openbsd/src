/*	$OpenBSD: machine.c,v 1.10 1998/07/08 22:14:16 deraadt Exp $	*/

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
 * LIBS: -lkvm
 *
 * TERMCAP: -ltermlib
 *
 * CFLAGS: -DHAVE_GETOPT
 *
 * AUTHOR:  Thorsten Lockert <tholo@sigmasoft.com>
 *          Adapted from BSD4.4 by Christos Zoulas <christos@ee.cornell.edu>
 *          Patch for process wait display by Jarl F. Greipsland <jarle@idt.unit.no>
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
#include <nlist.h>
#include <math.h>
#include <kvm.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/dir.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef DOSWAP
#include <err.h>
#include <sys/map.h>
#include <sys/conf.h>
#endif

static int check_nlist __P((struct nlist *));
static int getkval __P((unsigned long, int *, int, char *));
static int swapmode __P((int *, int *));

#include "top.h"
#include "display.h"
#include "machine.h"
#include "utils.h"

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle
{
    struct kinfo_proc **next_proc;	/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
};

/* declarations for load_avg */
#include "loadavg.h"

#define PP(pp, field) ((pp)->kp_proc . field)
#define EP(pp, field) ((pp)->kp_eproc . field)
#define VP(pp, field) ((pp)->kp_eproc.e_vm . field)

/* what we consider to be process size: */
#define PROCSIZE(pp) (VP((pp), vm_tsize) + VP((pp), vm_dsize) + VP((pp), vm_ssize))

/* definitions for indices in the nlist array */
#define X_CP_TIME	0
#define X_HZ		1

#ifdef DOSWAP
#define	VM_SWAPMAP	2
#define	VM_NSWAPMAP	3
#define	VM_SWDEVT	4
#define	VM_NSWAP	5
#define	VM_NSWDEV	6
#define	VM_DMMAX	7
#define	VM_NISWAP	8
#define	VM_NISWDEV	9
#endif

static struct nlist nlst[] = {
    { "_cp_time" },		/* 0 */
    { "_hz" },			/* 1 */
#ifdef DOSWAP
    { "_swapmap" },		/* 2 */
    { "_nswapmap" },		/* 3 */
    { "_swdevt" },		/* 4 */
    { "_nswap" },		/* 5 */
    { "_nswdev" },		/* 6 */
    { "_dmmax" },		/* 7 */
    { "_niswap" },		/* 8 */
    { "_niswdev" },		/* 9 */
#endif
    { 0 }
};

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

char *state_abbrev[] =
{
    "", "start", "run\0\0\0", "sleep", "stop", "zomb",
};


static kvm_t *kd;

/* these are retrieved from the kernel in _init */

static          int hz;

/* these are offsets obtained via nlist and used in the get_ functions */

static unsigned long cp_time_offset;

/* these are for calculating cpu state percentages */
static int cp_time[CPUSTATES];
static int cp_old[CPUSTATES];
static int cp_diff[CPUSTATES];

/* these are for detailing the process states */

int process_states[7];
char *procstatenames[] = {
    "", " starting, ", " running, ", " idle, ", " stopped, ", " zombie, ",
    NULL
};

/* these are for detailing the cpu states */

int cpu_states[CPUSTATES];
char *cpustatenames[] = {
    "user", "nice", "system", "interrupt", "idle", NULL
};

/* these are for detailing the memory statistics */

int memory_stats[8];
char *memorynames[] = {
    "Real: ", "K/", "K act/tot  ", "Free: ", "K  ",
#ifdef DOSWAP
    "Swap: ", "K/", "K used/tot",
#endif
    NULL
};

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

int
machine_init(statics)

struct statics *statics;

{
    register int i = 0;
    register int pagesize;
    char errbuf[_POSIX2_LINE_MAX];

    if ((kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) == NULL) {
	warnx("%s", errbuf);
	return(-1);
    }

    setegid(getgid());
    setgid(getgid());

    /* get the list of symbols we want to access in the kernel */
    if (kvm_nlist(kd, nlst) <= 0) {
	warnx("nlist failed");
	return(-1);
    }

    /* make sure they were all found */
    if (i > 0 && check_nlist(nlst) > 0)
	return(-1);

    /* get the symbol values out of kmem */
    (void) getkval(nlst[X_HZ].n_value,     (int *)(&hz),	sizeof(hz),
	    nlst[X_HZ].n_name);

    /* stash away certain offsets for later use */
    cp_time_offset = nlst[X_CP_TIME].n_value;

    pbase = NULL;
    pref = NULL;
    onproc = -1;
    nproc = 0;

    /* get the page size with "getpagesize" and calculate pageshift from it */
    pagesize = getpagesize();
    pageshift = 0;
    while (pagesize > 1)
    {
	pageshift++;
	pagesize >>= 1;
    }

    /* we only need the amount of log(2)1024 for our conversion */
    pageshift -= LOG1024;

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;

    /* all done! */
    return(0);
}

char *format_header(uname_field)

register char *uname_field;

{
    register char *ptr;

    ptr = header + UNAME_START;
    while (*uname_field != '\0')
    {
	*ptr++ = *uname_field++;
    }

    return(header);
}

void
get_system_info(si)

struct system_info *si;

{
    int total;

    /* get the cp_time array */
    (void) getkval(cp_time_offset, (int *)cp_time, sizeof(cp_time),
		   "_cp_time");

    /* convert load averages to doubles */
    {
	register int i;
	register double *infoloadp;
	struct loadavg sysload;
	size_t size = sizeof(sysload);
	static int mib[] = { CTL_VM, VM_LOADAVG };

	if (sysctl(mib, 2, &sysload, &size, NULL, 0) < 0) {
	    warn("sysctl failed");
	    bzero(&total, sizeof(total));
	}

	infoloadp = si->load_avg;
	for (i = 0; i < 3; i++)
	    *infoloadp++ = ((double) sysload.ldavg[i]) / sysload.fscale;
    }

    /* convert cp_time counts to percentages */
    total = percentages(CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

    /* sum memory statistics */
    {
	struct vmtotal total;
	size_t size = sizeof(total);
	static int mib[] = { CTL_VM, VM_METER };

	/* get total -- systemwide main memory usage structure */
	if (sysctl(mib, 2, &total, &size, NULL, 0) < 0) {
	    warn("sysctl failed");
	    bzero(&total, sizeof(total));
	}
	/* convert memory stats to Kbytes */
	memory_stats[0] = -1;
	memory_stats[1] = pagetok(total.t_arm);
	memory_stats[2] = pagetok(total.t_rm);
	memory_stats[3] = -1;
	memory_stats[4] = pagetok(total.t_free);
	memory_stats[5] = -1;
#ifdef DOSWAP
	if (!swapmode(&memory_stats[6], &memory_stats[7])) {
	    memory_stats[6] = 0;
	    memory_stats[7] = 0;
	}
#endif
    }

    /* set arrays and strings */
    si->cpustates = cpu_states;
    si->memory = memory_stats;
    si->last_pid = -1;
}

static struct handle handle;

caddr_t get_process_info(si, sel, compare)

struct system_info *si;
struct process_select *sel;
int (*compare) __P((const void *, const void *));

{
    register int i;
    register int total_procs;
    register int active_procs;
    register struct kinfo_proc **prefp;
    register struct kinfo_proc *pp;

    /* these are copied out of sel for speed */
    int show_idle;
    int show_system;
    int show_uid;
    int show_command;

    
    if ((pbase = kvm_getprocs(kd, KERN_PROC_ALL, 0, &nproc)) == NULL) {
	warnx("%s", kvm_geterr(kd));
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
    memset((char *)process_states, 0, sizeof(process_states));
    prefp = pref;
    for (pp = pbase, i = 0; i < nproc; pp++, i++)
    {
	/*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with SSYS set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
	if (PP(pp, p_stat) != 0 &&
	    (show_system || ((PP(pp, p_flag) & P_SYSTEM) == 0)))
	{
	    total_procs++;
	    process_states[(unsigned char) PP(pp, p_stat)]++;
	    if ((PP(pp, p_stat) != SZOMB) &&
		(show_idle || (PP(pp, p_pctcpu) != 0) || 
		 (PP(pp, p_stat) == SRUN)) &&
		(!show_uid || EP(pp, e_pcred.p_ruid) == (uid_t)sel->uid))
	    {
		*prefp++ = pp;
		active_procs++;
	    }
	}
    }

    /* if requested, sort the "interesting" processes */
    if (compare != NULL)
    {
	qsort((char *)pref, active_procs, sizeof(struct kinfo_proc *), compare);
    }

    /* remember active and total counts */
    si->p_total = total_procs;
    si->p_active = pref_len = active_procs;

    /* pass back a handle */
    handle.next_proc = pref;
    handle.remaining = active_procs;
    return((caddr_t)&handle);
}

char fmt[MAX_COLS];		/* static area where result is built */

char *format_next_process(handle, get_userid)

caddr_t handle;
char *(*get_userid)();

{
    register struct kinfo_proc *pp;
    register int cputime;
    register double pct;
    struct handle *hp;
    char waddr[sizeof(void *) * 2 + 3];	/* Hexify void pointer */
    char *p_wait;

    /* find and remember the next proc structure */
    hp = (struct handle *)handle;
    pp = *(hp->next_proc++);
    hp->remaining--;
    

    /* get the process's user struct and set cputime */
    if ((PP(pp, p_flag) & P_INMEM) == 0) {
	/*
	 * Print swapped processes as <pname>
	 */
	char *comm = PP(pp, p_comm);
#define COMSIZ sizeof(PP(pp, p_comm))
	char buf[COMSIZ];
	(void) strncpy(buf, comm, COMSIZ);
	comm[0] = '<';
	(void) strncpy(&comm[1], buf, COMSIZ - 2);
	comm[COMSIZ - 2] = '\0';
	(void) strncat(comm, ">", COMSIZ - 1);
	comm[COMSIZ - 1] = '\0';
    }

    cputime = (PP(pp, p_uticks) + PP(pp, p_sticks) + PP(pp, p_iticks)) / hz;

    /* calculate the base for cpu percentages */
    pct = pctdouble(PP(pp, p_pctcpu));

    if (PP(pp, p_wchan))
        if (PP(pp, p_wmesg))
	    p_wait = EP(pp, e_wmesg);
	else {
	    snprintf(waddr, sizeof(waddr), "%lx", 
		(unsigned long)(PP(pp, p_wchan)) & ~KERNBASE);
	    p_wait = waddr;
        }
    else
	p_wait = "-";
	    
    /* format this entry */
    snprintf(fmt, MAX_COLS,
	    Proc_format,
	    PP(pp, p_pid),
	    (*get_userid)(EP(pp, e_pcred.p_ruid)),
	    PP(pp, p_priority) - PZERO,
	    PP(pp, p_nice) - NZERO,
	    format_k(pagetok(PROCSIZE(pp))),
	    format_k(pagetok(VP(pp, vm_rssize))),
	    (PP(pp, p_stat) == SSLEEP && PP(pp, p_slptime) > MAXSLP)
	     ? "idle" : state_abbrev[(unsigned char) PP(pp, p_stat)],
	    p_wait,
	    format_time(cputime),
	    100.0 * pct,
	    printable(PP(pp, p_comm)));

    /* return the result */
    return(fmt);
}


/*
 * check_nlist(nlst) - checks the nlist to see if any symbols were not
 *		found.  For every symbol that was not found, a one-line
 *		message is printed to stderr.  The routine returns the
 *		number of symbols NOT found.
 */

static int check_nlist(nlst)

register struct nlist *nlst;

{
    register int i;

    /* check to see if we got ALL the symbols we requested */
    /* this will write one line to stderr for every symbol not found */

    i = 0;
    while (nlst->n_name != NULL)
    {
	if (nlst->n_type == 0)
	{
	    /* this one wasn't found */
	    (void) fprintf(stderr, "kernel: no symbol named `%s'\n",
			   nlst->n_name);
	    i = 1;
	}
	nlst++;
    }

    return(i);
}


/*
 *  getkval(offset, ptr, size, refstr) - get a value out of the kernel.
 *	"offset" is the byte offset into the kernel for the desired value,
 *  	"ptr" points to a buffer into which the value is retrieved,
 *  	"size" is the size of the buffer (and the object to retrieve),
 *  	"refstr" is a reference string used when printing error meessages,
 *	    if "refstr" starts with a '!', then a failure on read will not
 *  	    be fatal (this may seem like a silly way to do things, but I
 *  	    really didn't want the overhead of another argument).
 *  	
 */

static int getkval(offset, ptr, size, refstr)

unsigned long offset;
int *ptr;
int size;
char *refstr;

{
    if (kvm_read(kd, offset, (char *) ptr, size) != size)
    {
	if (*refstr == '!')
	{
	    return(0);
	}
	else
	{
	    warn("kvm_read for %s", refstr);
	    quit(23);
	}
    }
    return(1);
}
    
/* comparison routine for qsort */

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

static unsigned char sorted_state[] =
{
    0,	/* not used		*/
    4,	/* start		*/
    5,	/* run			*/
    2,	/* sleep		*/
    3,	/* stop			*/
    1	/* zombie		*/
};
 
int
proc_compare(v1, v2)

const void *v1, *v2;

{
    register struct proc **pp1 = (struct proc **)v1;
    register struct proc **pp2 = (struct proc **)v2;
    register struct kinfo_proc *p1;
    register struct kinfo_proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    /* compare percent cpu (pctcpu) */
    if ((lresult = PP(p2, p_pctcpu) - PP(p1, p_pctcpu)) == 0)
    {
	/* use CPU usage to break the tie */
	if ((result = PP(p2, p_rtime).tv_sec - PP(p1, p_rtime).tv_sec) == 0)
	{
	    /* use process state to break the tie */
	    if ((result = sorted_state[(unsigned char) PP(p2, p_stat)] -
			  sorted_state[(unsigned char) PP(p1, p_stat)])  == 0)
	    {
		/* use priority to break the tie */
		if ((result = PP(p2, p_priority) - PP(p1, p_priority)) == 0)
		{
		    /* use resident set size (rssize) to break the tie */
		    if ((result = VP(p2, vm_rssize) - VP(p1, vm_rssize)) == 0)
		    {
			/* use total memory to break the tie */
			result = PROCSIZE(p2) - PROCSIZE(p1);
		    }
		}
	    }
	}
    }
    else
    {
	result = lresult < 0 ? -1 : 1;
    }

    return(result);
}


/*
 * proc_owner(pid) - returns the uid that owns process "pid", or -1 if
 *		the process does not exist.
 *		It is EXTREMLY IMPORTANT that this function work correctly.
 *		If top runs setuid root (as in SVR4), then this function
 *		is the only thing that stands in the way of a serious
 *		security problem.  It validates requests for the "kill"
 *		and "renice" commands.
 */

int proc_owner(pid)

pid_t pid;

{
    register int cnt;
    register struct kinfo_proc **prefp;
    register struct kinfo_proc *pp;

    prefp = pref;
    cnt = pref_len;
    while (--cnt >= 0)
    {
	pp = *prefp++;	
	if (PP(pp, p_pid) == pid)
	{
	    return((int)EP(pp, e_pcred.p_ruid));
	}
    }
    return(-1);
}

#ifdef DOSWAP
/*
 * swapmode is based on a program called swapinfo written
 * by Kevin Lahey <kml@rokkaku.atl.ga.us>.
 */

#define	SVAR(var) __STRING(var)	/* to force expansion */
#define	KGET(idx, var)							\
	KGET1(idx, &var, sizeof(var), SVAR(var))
#define	KGET1(idx, p, s, msg)						\
	KGET2(nlst[idx].n_value, p, s, msg)
#define	KGET2(addr, p, s, msg)						\
	if (kvm_read(kd, (u_long)(addr), p, s) != s)			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd))

static int
swapmode(used, total)
int *used;
int *total;
{
	int nswap, nswdev, dmmax, nswapmap, niswap, niswdev;
	int s, e, i, l, nfree;
	struct swdevt *sw;
	long *perdev;
	struct map *swapmap, *kswapmap;
	struct mapent *mp, *freemp;

	KGET(VM_NSWAP, nswap);
	KGET(VM_NSWDEV, nswdev);
	KGET(VM_DMMAX, dmmax);
	KGET(VM_NSWAPMAP, nswapmap);
	KGET(VM_SWAPMAP, kswapmap);	/* kernel `swapmap' is a pointer */
	if (nswap == 0) {
		*used = 0;
		*total = 0;
		return (1);
	}
	if ((sw = malloc(nswdev * sizeof(*sw))) == NULL ||
	    (perdev = malloc(nswdev * sizeof(*perdev))) == NULL ||
	    (freemp = mp = malloc(nswapmap * sizeof(*mp))) == NULL)
		err(1, "malloc");
	KGET1(VM_SWDEVT, sw, nswdev * sizeof(*sw), "swdevt");
	KGET2((long)kswapmap, mp, nswapmap * sizeof(*mp), "swapmap");

	/* Supports sequential swap */
	if (nlst[VM_NISWAP].n_value != 0) {
		KGET(VM_NISWAP, niswap);
		KGET(VM_NISWDEV, niswdev);
	} else {
		niswap = nswap;
		niswdev = nswdev;
	}

	/* First entry in map is `struct map'; rest are mapent's. */
	swapmap = (struct map *)mp;
	if (nswapmap != swapmap->m_limit - (struct mapent *)kswapmap)
		errx(1, "panic: nswapmap goof");

	/* Count up swap space. */
	nfree = 0;
	memset(perdev, 0, nswdev * sizeof(*perdev));
	for (mp++; mp->m_addr != 0; mp++) {
		s = mp->m_addr;			/* start of swap region */
		e = mp->m_addr + mp->m_size;	/* end of region */
		nfree += mp->m_size;

		/*
		 * Swap space is split up among the configured disks.
		 *
		 * For interleaved swap devices, the first dmmax blocks
		 * of swap space some from the first disk, the next dmmax
		 * blocks from the next, and so on up to niswap blocks.
		 *
		 * Sequential swap devices follow the interleaved devices
		 * (i.e. blocks starting at niswap) in the order in which
		 * they appear in the swdev table.  The size of each device
		 * will be a multiple of dmmax.
		 *
		 * The list of free space joins adjacent free blocks,
		 * ignoring device boundries.  If we want to keep track
		 * of this information per device, we'll just have to
		 * extract it ourselves.  We know that dmmax-sized chunks
		 * cannot span device boundaries (interleaved or sequential)
		 * so we loop over such chunks assigning them to devices.
		 */
		i = -1;
		while (s < e) {		/* XXX this is inefficient */
			int bound = roundup(s+1, dmmax);

			if (bound > e)
				bound = e;
			if (bound <= niswap) {
				/* Interleaved swap chunk. */
				if (i == -1)
					i = (s / dmmax) % niswdev;
				perdev[i] += bound - s;
				if (++i >= niswdev)
					i = 0;
			} else {
				/* Sequential swap chunk. */
				if (i < niswdev) {
					i = niswdev;
					l = niswap + sw[i].sw_nblks;
				}
				while (s >= l) {
					/* XXX don't die on bogus blocks */
					if (i == nswdev-1)
						break;
					l += sw[++i].sw_nblks;
				}
				perdev[i] += bound - s;
			}
			s = bound;
		}
	}

	*total = 0;
	for (i = 0; i < nswdev; i++) {
		int xsize, xfree;

		xsize = sw[i].sw_nblks;
		xfree = perdev[i];
		*total += xsize;
	}

	/* 
	 * If only one partition has been set up via swapon(8), we don't
	 * need to bother with totals.
	 */
#if DEV_BSHIFT < 10
	*used = (*total - nfree) >> (10 - DEV_BSHIFT);
	*total >>= 10 - DEV_BSHIFT;
#elif DEV_BSHIFT > 10
	*used = (*total - nfree) >> (DEV_BSHIFT - 10);
	*total >>= DEV_BSHIFT - 10;
#endif
	free (sw); free (freemp); free (perdev);
	return 1;
}
#endif
