/*	$OpenBSD: uvmexp.h,v 1.4 2018/11/06 07:49:38 otto Exp $	*/

#ifndef	_UVM_UVMEXP_
#define	_UVM_UVMEXP_

/*
 * CTL_VM identifiers
 */
#define	VM_METER	1		/* struct vmmeter */
#define	VM_LOADAVG	2		/* struct loadavg */
#define	VM_PSSTRINGS	3		/* PSSTRINGS */
#define VM_UVMEXP	4		/* struct uvmexp */
#define VM_SWAPENCRYPT	5		/* int */
#define VM_NKMEMPAGES	6		/* int - # kmem_map pages */
#define	VM_ANONMIN	7
#define	VM_VTEXTMIN	8
#define	VM_VNODEMIN	9
#define	VM_MAXSLP	10
#define	VM_USPACE	11
#define	VM_MALLOC_CONF	12		/* config for userland malloc */
#define	VM_MAXID	13		/* number of valid vm ids */

#define	CTL_VM_NAMES { \
	{ 0, 0 }, \
	{ "vmmeter", CTLTYPE_STRUCT }, \
	{ "loadavg", CTLTYPE_STRUCT }, \
	{ "psstrings", CTLTYPE_STRUCT }, \
	{ "uvmexp", CTLTYPE_STRUCT }, \
	{ "swapencrypt", CTLTYPE_NODE }, \
	{ "nkmempages", CTLTYPE_INT }, \
	{ "anonmin", CTLTYPE_INT }, \
	{ "vtextmin", CTLTYPE_INT }, \
	{ "vnodemin", CTLTYPE_INT }, \
	{ "maxslp", CTLTYPE_INT }, \
	{ "uspace", CTLTYPE_INT }, \
	{ "malloc_conf", CTLTYPE_STRING }, \
}

/*
 * uvmexp: global data structures that are exported to parts of the kernel
 * other than the vm system.
 */
struct uvmexp {
	/* vm_page constants */
	int pagesize;   /* size of a page (PAGE_SIZE): must be power of 2 */
	int pagemask;   /* page mask */
	int pageshift;  /* page shift */

	/* vm_page counters */
	int npages;     /* number of pages we manage */
	int free;       /* number of free pages */
	int active;     /* number of active pages */
	int inactive;   /* number of pages that we free'd but may want back */
	int paging;	/* number of pages in the process of being paged out */
	int wired;      /* number of wired pages */

	int zeropages;		/* number of zero'd pages */
	int reserve_pagedaemon; /* number of pages reserved for pagedaemon */
	int reserve_kernel;	/* number of pages reserved for kernel */
	int unused01;		/* formerly anonpages */
	int vnodepages;		/* XXX # of pages used by vnode page cache */
	int vtextpages;		/* XXX # of pages used by vtext vnodes */

	/* pageout params */
	int freemin;    /* min number of free pages */
	int freetarg;   /* target number of free pages */
	int inactarg;   /* target number of inactive pages */
	int wiredmax;   /* max number of wired pages */
	int anonmin;	/* min threshold for anon pages */
	int vtextmin;	/* min threshold for vtext pages */
	int vnodemin;	/* min threshold for vnode pages */
	int anonminpct;	/* min percent anon pages */
	int vtextminpct;/* min percent vtext pages */
	int vnodeminpct;/* min percent vnode pages */

	/* swap */
	int nswapdev;	/* number of configured swap devices in system */
	int swpages;	/* number of PAGE_SIZE'ed swap pages */
	int swpginuse;	/* number of swap pages in use */
	int swpgonly;	/* number of swap pages in use, not also in RAM */
	int nswget;	/* number of times fault calls uvm_swap_get() */
	int nanon;	/* XXX number total of anon's in system */
	int unused05;	/* formerly nanonneeded */
	int unused06;	/* formerly nfreeanon */

	/* stat counters */
	int faults;		/* page fault count */
	int traps;		/* trap count */
	int intrs;		/* interrupt count */
	int swtch;		/* context switch count */
	int softs;		/* software interrupt count */
	int syscalls;		/* system calls */
	int pageins;		/* pagein operation count */
				/* pageouts are in pdpageouts below */
	int unused07;		/* formerly obsolete_swapins */
	int unused08;		/* formerly obsolete_swapouts */
	int pgswapin;		/* pages swapped in */
	int pgswapout;		/* pages swapped out */
	int forks;  		/* forks */
	int forks_ppwait;	/* forks where parent waits */
	int forks_sharevm;	/* forks where vmspace is shared */
	int pga_zerohit;	/* pagealloc where zero wanted and zero
				   was available */
	int pga_zeromiss;	/* pagealloc where zero wanted and zero
				   not available */
	int unused09;		/* formerly zeroaborts */

	/* fault subcounters */
	int fltnoram;	/* number of times fault was out of ram */
	int fltnoanon;	/* number of times fault was out of anons */
	int fltnoamap;	/* number of times fault was out of amap chunks */
	int fltpgwait;	/* number of times fault had to wait on a page */
	int fltpgrele;	/* number of times fault found a released page */
	int fltrelck;	/* number of times fault relock called */
	int fltrelckok;	/* number of times fault relock is a success */
	int fltanget;	/* number of times fault gets anon page */
	int fltanretry;	/* number of times fault retrys an anon get */
	int fltamcopy;	/* number of times fault clears "needs copy" */
	int fltnamap;	/* number of times fault maps a neighbor anon page */
	int fltnomap;	/* number of times fault maps a neighbor obj page */
	int fltlget;	/* number of times fault does a locked pgo_get */
	int fltget;	/* number of times fault does an unlocked get */
	int flt_anon;	/* number of times fault anon (case 1a) */
	int flt_acow;	/* number of times fault anon cow (case 1b) */
	int flt_obj;	/* number of times fault is on object page (2a) */
	int flt_prcopy;	/* number of times fault promotes with copy (2b) */
	int flt_przero;	/* number of times fault promotes with zerofill (2b) */

	/* daemon counters */
	int pdwoke;	/* number of times daemon woke up */
	int pdrevs;	/* number of times daemon rev'd clock hand */
	int pdswout;	/* number of times daemon called for swapout */
	int pdfreed;	/* number of pages daemon freed since boot */
	int pdscans;	/* number of pages daemon scanned since boot */
	int pdanscan;	/* number of anonymous pages scanned by daemon */
	int pdobscan;	/* number of object pages scanned by daemon */
	int pdreact;	/* number of pages daemon reactivated since boot */
	int pdbusy;	/* number of times daemon found a busy page */
	int pdpageouts;	/* number of times daemon started a pageout */
	int pdpending;	/* number of times daemon got a pending pagout */
	int pddeact;	/* number of pages daemon deactivates */
	int unused11;	/* formerly pdreanon */
	int unused12;	/* formerly pdrevnode */
	int unused13;	/* formerly pdrevtext */

	int fpswtch;	/* FPU context switches */
	int kmapent;	/* number of kernel map entries */
};

struct _ps_strings {
	void	*val;
};

#endif /*_UVM_UVMEXP_ */
