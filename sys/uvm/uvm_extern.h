/*	$OpenBSD: uvm_extern.h,v 1.95 2011/04/18 19:23:46 art Exp $	*/
/*	$NetBSD: uvm_extern.h,v 1.57 2001/03/09 01:02:12 chs Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: Id: uvm_extern.h,v 1.1.2.21 1998/02/07 01:16:53 chs Exp
 */

/*-
 * Copyright (c) 1991, 1992, 1993
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
 *
 *	@(#)vm_extern.h	8.5 (Berkeley) 5/3/95
 */

#ifndef _UVM_UVM_EXTERN_H_
#define _UVM_UVM_EXTERN_H_

/*
 * uvm_extern.h: this file defines the external interface to the VM system.
 *
 * this should be the only file included by non-VM parts of the kernel
 * which need access to VM services.   if you want to know the interface
 * to the MI VM layer without knowing the details, this is the file to
 * learn.
 *
 * NOTE: vm system calls are prototyped in syscallargs.h
 */

/*
 * typedefs, necessary for standard UVM headers.
 */

typedef unsigned int  uvm_flag_t;
typedef int vm_fault_t;

typedef int vm_inherit_t;	/* XXX: inheritance codes */
typedef off_t voff_t;		/* XXX: offset within a uvm_object */

union vm_map_object;
typedef union vm_map_object vm_map_object_t;

struct vm_map_entry;
typedef struct vm_map_entry *vm_map_entry_t;

struct vm_map;
typedef struct vm_map *vm_map_t;

struct vm_page;
typedef struct vm_page  *vm_page_t;

/*
 * defines
 */

/*
 * the following defines are for uvm_map and functions which call it.
 */

/* protections bits */
#define UVM_PROT_MASK	0x07	/* protection mask */
#define UVM_PROT_NONE	0x00	/* protection none */
#define UVM_PROT_ALL	0x07	/* everything */
#define UVM_PROT_READ	0x01	/* read */
#define UVM_PROT_WRITE  0x02	/* write */
#define UVM_PROT_EXEC	0x04	/* exec */

/* protection short codes */
#define UVM_PROT_R	0x01	/* read */
#define UVM_PROT_W	0x02	/* write */
#define UVM_PROT_RW	0x03    /* read-write */
#define UVM_PROT_X	0x04	/* exec */
#define UVM_PROT_RX	0x05	/* read-exec */
#define UVM_PROT_WX	0x06	/* write-exec */
#define UVM_PROT_RWX	0x07	/* read-write-exec */

/* 0x08: not used */

/* inherit codes */
#define UVM_INH_MASK	0x30	/* inherit mask */
#define UVM_INH_SHARE	0x00	/* "share" */
#define UVM_INH_COPY	0x10	/* "copy" */
#define UVM_INH_NONE	0x20	/* "none" */
#define UVM_INH_DONATE	0x30	/* "donate" << not used */

/* 0x40, 0x80: not used */

/* bits 0x700: max protection, 0x800: not used */

/* bits 0x7000: advice, 0x8000: not used */

typedef int		vm_prot_t;

/*
 *	Protection values, defined as bits within the vm_prot_t type
 *
 *   These are funky definitions from old CMU VM and are kept
 *   for compatibility reasons, one day they are going to die,
 *   just like everybody else.
 */

#define	VM_PROT_NONE	((vm_prot_t) 0x00)

#define VM_PROT_READ	((vm_prot_t) 0x01)	/* read permission */
#define VM_PROT_WRITE	((vm_prot_t) 0x02)	/* write permission */
#define VM_PROT_EXECUTE	((vm_prot_t) 0x04)	/* execute permission */

/*
 *	The default protection for newly-created virtual memory
 */

#define VM_PROT_DEFAULT	(VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)

/*
 *	The maximum privileges possible, for parameter checking.
 */

#define VM_PROT_ALL	(VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)

/* advice: matches MADV_* from sys/mman.h */
#define UVM_ADV_NORMAL	0x0	/* 'normal' */
#define UVM_ADV_RANDOM	0x1	/* 'random' */
#define UVM_ADV_SEQUENTIAL 0x2	/* 'sequential' */
/* 0x3: will need, 0x4: dontneed */
#define UVM_ADV_MASK	0x7	/* mask */

/* mapping flags */
#define UVM_FLAG_FIXED   0x010000 /* find space */
#define UVM_FLAG_OVERLAY 0x020000 /* establish overlay */
#define UVM_FLAG_NOMERGE 0x040000 /* don't merge map entries */
#define UVM_FLAG_COPYONW 0x080000 /* set copy_on_write flag */
#define UVM_FLAG_AMAPPAD 0x100000 /* for bss: pad amap to reduce malloc() */
#define UVM_FLAG_TRYLOCK 0x200000 /* fail if we can not lock map */
#define	UVM_FLAG_HOLE    0x400000 /* no backend */

/* macros to extract info */
#define UVM_PROTECTION(X)	((X) & UVM_PROT_MASK)
#define UVM_INHERIT(X)		(((X) & UVM_INH_MASK) >> 4)
#define UVM_MAXPROTECTION(X)	(((X) >> 8) & UVM_PROT_MASK)
#define UVM_ADVICE(X)		(((X) >> 12) & UVM_ADV_MASK)

#define UVM_MAPFLAG(PROT,MAXPROT,INH,ADVICE,FLAGS) \
	((MAXPROT << 8)|(PROT)|(INH)|((ADVICE) << 12)|(FLAGS))

/* magic offset value */
#define UVM_UNKNOWN_OFFSET ((voff_t) -1)
				/* offset not known(obj) or don't care(!obj) */

/*
 * the following defines are for uvm_km_kmemalloc's flags
 */
#define UVM_KMF_NOWAIT	0x1			/* matches M_NOWAIT */
#define UVM_KMF_VALLOC	0x2			/* allocate VA only */
#define UVM_KMF_CANFAIL	0x4			/* caller handles failure */
#define UVM_KMF_ZERO	0x08			/* zero pages */
#define UVM_KMF_TRYLOCK	UVM_FLAG_TRYLOCK	/* try locking only */

/*
 * the following defines the strategies for uvm_pagealloc()
 */
#define	UVM_PGA_STRAT_NORMAL	0	/* high -> low free list walk */
#define	UVM_PGA_STRAT_ONLY	1	/* only specified free list */
#define	UVM_PGA_STRAT_FALLBACK	2	/* ONLY falls back on NORMAL */

/*
 * flags for uvm_pagealloc()
 */
#define UVM_PGA_USERESERVE	0x0001	/* ok to use reserve pages */
#define	UVM_PGA_ZERO		0x0002	/* returned page must be zeroed */

/*
 * flags for uvm_pglistalloc()
 */
#define UVM_PLA_WAITOK		0x0001	/* may sleep */
#define UVM_PLA_NOWAIT		0x0002	/* can't sleep (need one of the two) */
#define UVM_PLA_ZERO		0x0004	/* zero all pages before returning */
#define UVM_PLA_TRYCONTIG	0x0008	/* try to allocate contig physmem */

/*
 * lockflags that control the locking behavior of various functions.
 */
#define	UVM_LK_ENTER	0x00000001	/* map locked on entry */
#define	UVM_LK_EXIT	0x00000002	/* leave map locked on exit */

/*
 * flags to uvm_physload.
 */
#define	PHYSLOAD_DEVICE	0x01	/* don't add to the page queue */

/*
 * structures
 */

struct buf;
struct core;
struct mount;
struct pglist;
struct proc;
struct ucred;
struct uio;
struct uvm_object;
struct vm_anon;
struct vm_aref;
struct vm_map;
struct vmspace;
struct pmap;
struct vnode;
struct pool;
struct simplelock;

extern struct pool *uvm_aiobuf_pool;

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
	int anonpages;		/* number of pages used by anon pagers */
	int vnodepages;		/* number of pages used by vnode page cache */
	int vtextpages;		/* number of pages used by vtext vnodes */

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
	int nanon;	/* number total of anon's in system */
	int nanonneeded;/* number of anons currently needed */
	int nfreeanon;	/* number of free anon's */

	/* stat counters */
	int faults;		/* page fault count */
	int traps;		/* trap count */
	int intrs;		/* interrupt count */
	int swtch;		/* context switch count */
	int softs;		/* software interrupt count */
	int syscalls;		/* system calls */
	int pageins;		/* pagein operation count */
				/* pageouts are in pdpageouts below */
	int swapins;		/* swapins */
	int swapouts;		/* swapouts */
	int pgswapin;		/* pages swapped in */
	int pgswapout;		/* pages swapped out */
	int forks;  		/* forks */
	int forks_ppwait;	/* forks where parent waits */
	int forks_sharevm;	/* forks where vmspace is shared */
	int pga_zerohit;	/* pagealloc where zero wanted and zero
				   was available */
	int pga_zeromiss;	/* pagealloc where zero wanted and zero
				   not available */
	int zeroaborts;		/* number of times page zeroing was
				   aborted */

	/* fault subcounters */
	int fltnoram;	/* number of times fault was out of ram */
	int fltnoanon;	/* number of times fault was out of anons */
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
	int pdreanon;	/* anon pages reactivated due to min threshold */
	int pdrevnode;	/* vnode pages reactivated due to min threshold */
	int pdrevtext;	/* vtext pages reactivated due to min threshold */

	int fpswtch;	/* FPU context switches */
	int kmapent;	/* number of kernel map entries */
};

#ifdef _KERNEL
extern struct uvmexp uvmexp;
#endif

/*
 * Finally, bring in standard UVM headers.
 */
#include <sys/vmmeter.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <uvm/uvm_param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <uvm/uvm_page.h>
#include <uvm/uvm_pmap.h>
#include <uvm/uvm_map.h>
#include <uvm/uvm_fault.h>
#include <uvm/uvm_pager.h>

/*
 * Shareable process virtual address space.
 * May eventually be merged with vm_map.
 * Several fields are temporary (text, data stuff).
 */
struct vmspace {
	struct	vm_map vm_map;	/* VM address map */
	int	vm_refcnt;	/* number of references */
	caddr_t	vm_shm;		/* SYS5 shared memory private data XXX */
/* we copy from vm_startcopy to the end of the structure on fork */
#define vm_startcopy vm_rssize
	segsz_t vm_rssize; 	/* current resident set size in pages */
	segsz_t vm_swrss;	/* resident set size before last swap */
	segsz_t vm_tsize;	/* text size (pages) XXX */
	segsz_t vm_dsize;	/* data size (pages) XXX */
	segsz_t vm_dused;	/* data segment length (pages) XXX */
	segsz_t vm_ssize;	/* stack size (pages) */
	caddr_t	vm_taddr;	/* user virtual address of text XXX */
	caddr_t	vm_daddr;	/* user virtual address of data XXX */
	caddr_t vm_maxsaddr;	/* user VA at max stack growth */
	caddr_t vm_minsaddr;	/* user VA at top of stack */
};

#ifdef _KERNEL

/*
 * used to keep state while iterating over the map for a core dump.
 */
struct uvm_coredump_state {
	void *cookie;		/* opaque for the caller */
	vaddr_t start;		/* start of region */
	vaddr_t realend;	/* real end of region */
	vaddr_t end;		/* virtual end of region */
	vm_prot_t prot;		/* protection of region */
	int flags;		/* flags; see below */
};

#define	UVM_COREDUMP_STACK	0x01	/* region is user stack */

/*
 * the various kernel maps, owned by MD code
 */
extern struct vm_map *exec_map;
extern struct vm_map *kernel_map;
extern struct vm_map *kmem_map;
extern struct vm_map *phys_map;


/*
 * macros
 */

/* zalloc zeros memory, alloc does not */
#define uvm_km_zalloc(MAP,SIZE) uvm_km_alloc1(MAP,SIZE,0,TRUE)
#define uvm_km_alloc(MAP,SIZE)  uvm_km_alloc1(MAP,SIZE,0,FALSE)

#endif /* _KERNEL */

#ifdef	pmap_resident_count
#define vm_resident_count(vm) (pmap_resident_count((vm)->vm_map.pmap))
#else
#define vm_resident_count(vm) ((vm)->vm_rssize)
#endif

/* XXX clean up later */
struct buf;
struct loadavg;
struct proc;
struct pmap;
struct vmspace;
struct vmtotal;
struct mount;
struct vnode;
struct core;

#ifdef _KERNEL

/* vm_machdep.c */
void		vmapbuf(struct buf *, vsize_t);
void		vunmapbuf(struct buf *, vsize_t);
void		cpu_fork(struct proc *, struct proc *, void *, size_t,
		    void (*)(void *), void *);

/* uvm_aobj.c */
struct uvm_object	*uao_create(vsize_t, int);
void			uao_detach(struct uvm_object *);
void			uao_detach_locked(struct uvm_object *);
void			uao_reference(struct uvm_object *);
void			uao_reference_locked(struct uvm_object *);

/* uvm_fault.c */
int			uvm_fault(vm_map_t, vaddr_t, 
				vm_fault_t, vm_prot_t);
				/* handle a page fault */

/* uvm_glue.c */
#if defined(KGDB)
void			uvm_chgkprot(caddr_t, size_t, int);
#endif
void			uvm_fork(struct proc *, struct proc *, boolean_t,
			    void *, size_t, void (*)(void *), void *);
void			uvm_exit(struct proc *);
void			uvm_init_limits(struct proc *);
boolean_t		uvm_kernacc(caddr_t, size_t, int);

int			uvm_vslock(struct proc *, caddr_t, size_t,
			    vm_prot_t);
void			uvm_vsunlock(struct proc *, caddr_t, size_t);

int			uvm_vslock_device(struct proc *, void *, size_t,
			    vm_prot_t, void **);
void			uvm_vsunlock_device(struct proc *, void *, size_t,
			    void *);


/* uvm_init.c */
void			uvm_init(void);	
				/* init the uvm system */

/* uvm_io.c */
int			uvm_io(vm_map_t, struct uio *, int);

#define	UVM_IO_FIXPROT	0x01

/* uvm_km.c */
vaddr_t			uvm_km_alloc1(vm_map_t, vsize_t, vsize_t, boolean_t);
void			uvm_km_free(vm_map_t, vaddr_t, vsize_t);
void			uvm_km_free_wakeup(vm_map_t, vaddr_t, vsize_t);
vaddr_t			uvm_km_kmemalloc_pla(struct vm_map *,
			    struct uvm_object *, vsize_t, vsize_t, int,
			    paddr_t, paddr_t, paddr_t, paddr_t, int);
#define uvm_km_kmemalloc(map, obj, sz, flags)				\
	uvm_km_kmemalloc_pla(map, obj, sz, 0, flags, 0, (paddr_t)-1, 0, 0, 0)
vaddr_t			uvm_km_valloc(vm_map_t, vsize_t);
vaddr_t			uvm_km_valloc_try(vm_map_t, vsize_t);
vaddr_t			uvm_km_valloc_wait(vm_map_t, vsize_t);
vaddr_t			uvm_km_valloc_align(struct vm_map *, vsize_t, vsize_t, int);
vaddr_t			uvm_km_valloc_prefer_wait(vm_map_t, vsize_t,
					voff_t);

struct vm_map		*uvm_km_suballoc(vm_map_t, vaddr_t *,
				vaddr_t *, vsize_t, int,
				boolean_t, vm_map_t);

/*
 * Allocation mode for virtual space.
 *
 *  kv_map - pointer to the pointer to the map we're allocating from.
 *  kv_align - alignment.
 *  kv_wait - wait for free space in the map if it's full. The default
 *   allocators don't wait since running out of space in kernel_map and
 *   kmem_map is usually fatal. Special maps like exec_map are specifically
 *   limited, so waiting for space in them is necessary.
 *  kv_singlepage - use the single page allocator.
 *  kv_executable - map the physical pages with PROT_EXEC.
 */
struct kmem_va_mode {
	struct vm_map **kv_map;
	vsize_t kv_align;
	char kv_wait;
	char kv_singlepage;
	char kv_executable;
};

/*
 * Allocation mode for physical pages.
 *
 *  kp_constraint - allocation constraint for physical pages.
 *  kp_object - if the pages should be allocated from an object.
 *  kp_align - physical alignment of the first page in the allocation.
 *  kp_boundary - boundary that the physical addresses can't cross if
 *   the allocation is contiguous.
 *  kp_nomem - don't allocate any backing pages.
 *  kp_maxseg - maximal amount of contiguous segments.
 *  kp_zero - zero the returned memory.
 *  kp_pageable - allocate pageable memory.
 */
struct kmem_pa_mode {
	struct uvm_constraint_range *kp_constraint;
	struct uvm_object **kp_object;
	paddr_t kp_align;
	paddr_t kp_boundary;
	int kp_maxseg;
	char kp_nomem;
	char kp_zero;
	char kp_pageable;
};

/*
 * Dynamic allocation parameters. Stuff that changes too often or too much
 * to create separate va and pa modes for.
 *
 * kd_waitok - is it ok to sleep?
 * kd_trylock - don't sleep on map locks.
 * kd_prefer - offset to feed to PMAP_PREFER
 * kd_slowdown - special parameter for the singlepage va allocator
 *  that tells the caller to sleep if possible to let the singlepage
 *  allocator catch up.
 */
struct kmem_dyn_mode {
	voff_t kd_prefer;
	int *kd_slowdown;
	char kd_waitok;
	char kd_trylock;
};

#define KMEM_DYN_INITIALIZER { UVM_UNKNOWN_OFFSET, NULL, 0, 0 }

/*
 * Notice that for kv_ waiting has a different meaning. It's only supposed
 * to be used for very space constrained maps where waiting is a way
 * to throttle some other operation.
 * The exception is kv_page which needs to wait relatively often.
 * All kv_ except kv_intrsafe will potentially sleep.
 */
extern const struct kmem_va_mode kv_any;
extern const struct kmem_va_mode kv_intrsafe;
extern const struct kmem_va_mode kv_page;

extern const struct kmem_pa_mode kp_dirty;
extern const struct kmem_pa_mode kp_zero;
extern const struct kmem_pa_mode kp_dma;
extern const struct kmem_pa_mode kp_dma_zero;
extern const struct kmem_pa_mode kp_pageable;
extern const struct kmem_pa_mode kp_none;

extern const struct kmem_dyn_mode kd_waitok;
extern const struct kmem_dyn_mode kd_nowait;
extern const struct kmem_dyn_mode kd_trylock;


void *km_alloc(size_t, const struct kmem_va_mode *, const struct kmem_pa_mode *,
    const struct kmem_dyn_mode *);
void km_free(void *, size_t, const struct kmem_va_mode *,
    const struct kmem_pa_mode *);

/* uvm_map.c */
#define	uvm_map(_m, _a, _sz, _u, _f, _al, _fl) uvm_map_p(_m, _a, _sz, _u, _f, _al, _fl, 0)
int			uvm_map_p(vm_map_t, vaddr_t *, vsize_t,
				struct uvm_object *, voff_t, vsize_t,
				uvm_flag_t, struct proc *);
int			uvm_map_pageable(vm_map_t, vaddr_t, 
				vaddr_t, boolean_t, int);
int			uvm_map_pageable_all(vm_map_t, int, vsize_t);
boolean_t		uvm_map_checkprot(vm_map_t, vaddr_t,
				vaddr_t, vm_prot_t);
int			uvm_map_protect(vm_map_t, vaddr_t, 
				vaddr_t, vm_prot_t, boolean_t);
struct vmspace		*uvmspace_alloc(vaddr_t, vaddr_t,
				boolean_t, boolean_t);
void			uvmspace_init(struct vmspace *, struct pmap *,
				vaddr_t, vaddr_t, boolean_t, boolean_t);
void			uvmspace_exec(struct proc *, vaddr_t, vaddr_t);
struct vmspace		*uvmspace_fork(struct vmspace *);
void			uvmspace_free(struct vmspace *);
void			uvmspace_share(struct proc *, struct proc *);


/* uvm_meter.c */
void			uvm_meter(void);
int			uvm_sysctl(int *, u_int, void *, size_t *, 
				void *, size_t, struct proc *);
void			uvm_total(struct vmtotal *);

/* uvm_mmap.c */
int			uvm_mmap(vm_map_t, vaddr_t *, vsize_t,
				vm_prot_t, vm_prot_t, int, 
				caddr_t, voff_t, vsize_t, struct proc *);

/* uvm_page.c */
struct vm_page		*uvm_pagealloc(struct uvm_object *,
				voff_t, struct vm_anon *, int);
vaddr_t			uvm_pagealloc_contig(vaddr_t, vaddr_t,
				vaddr_t, vaddr_t);
void			uvm_pagealloc_multi(struct uvm_object *, voff_t,
    			    vsize_t, int);
void			uvm_pagerealloc(struct vm_page *, 
					     struct uvm_object *, voff_t);
/* Actually, uvm_page_physload takes PF#s which need their own type */
void			uvm_page_physload_flags(paddr_t, paddr_t, paddr_t,
			    paddr_t, int, int);
#define uvm_page_physload(s, e, as, ae, fl)	\
	uvm_page_physload_flags(s, e, as, ae, fl, 0)
void			uvm_setpagesize(void);
void			uvm_shutdown(void);

/* uvm_pager.c */
void			uvm_aio_biodone1(struct buf *);
void			uvm_aio_biodone(struct buf *);
void			uvm_aio_aiodone(struct buf *);

/* uvm_pdaemon.c */
void			uvm_pageout(void *);
void			uvm_aiodone_daemon(void *);
void			uvm_wait(const char *);

/* uvm_pglist.c */
int			uvm_pglistalloc(psize_t, paddr_t,
				paddr_t, paddr_t, paddr_t,
				struct pglist *, int, int); 
void			uvm_pglistfree(struct pglist *);

/* uvm_pmemrange.c */
void			uvm_pmr_use_inc(paddr_t, paddr_t);

/* uvm_swap.c */
void			uvm_swap_init(void);

/* uvm_unix.c */
int			uvm_coredump(struct proc *, struct vnode *, 
				struct ucred *, struct core *);
int			uvm_coredump_walkmap(struct proc *,
			    void *,
			    int (*)(struct proc *, void *,
				    struct uvm_coredump_state *), void *);
void			uvm_grow(struct proc *, vaddr_t);

/* uvm_user.c */
void			uvm_deallocate(vm_map_t, vaddr_t, vsize_t);

/* uvm_vnode.c */
void			uvm_vnp_setsize(struct vnode *, voff_t);
void			uvm_vnp_sync(struct mount *);
void 			uvm_vnp_terminate(struct vnode *);
				/* terminate a uvm/uvn object */
boolean_t		uvm_vnp_uncache(struct vnode *);
struct uvm_object	*uvn_attach(void *, vm_prot_t);

/* kern_malloc.c */
void			kmeminit_nkmempages(void);
void			kmeminit(void);
extern u_int		nkmempages;

#endif /* _KERNEL */

#endif /* _UVM_UVM_EXTERN_H_ */
