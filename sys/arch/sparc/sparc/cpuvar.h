/*	$OpenBSD: cpuvar.h,v 1.19 2010/09/28 20:27:55 miod Exp $	*/
/*	$NetBSD: cpuvar.h,v 1.4 1997/07/06 21:14:25 pk Exp $ */

/*
 *  Copyright (c) 1996 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Paul Kranenburg.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SPARC_CPUVAR_H
#define _SPARC_CPUVAR_H

#include <sys/device.h>
#include <sys/sched.h>

#include <sparc/sparc/cache.h>	/* for cacheinfo */

/*
 * CPU/MMU module information.
 * There is one of these for each "mainline" CPU module we support.
 * The information contained in the structure is used only during
 * auto-configuration of the CPUs; some fields are copied into the
 * per-cpu data structure (cpu_softc) for easy access during normal
 * operation.
 */
struct cpu_softc;
struct module_info {
	int  cpu_type;
	enum vactype vactype;
	void (*cpu_match)(struct cpu_softc *, struct module_info *, int);
	void (*getcacheinfo)(struct cpu_softc *sc, int node);
	void (*hotfix)(struct cpu_softc *);
	void (*mmu_enable)(void);
	void (*cache_enable)(void);
	int  ncontext;			/* max. # of contexts (we use) */

	void (*get_syncflt)(void);
	int  (*get_asyncflt)(u_int *, u_int *);
	void (*cache_flush)(caddr_t, u_int);
	void (*vcache_flush_page)(int);
	void (*vcache_flush_segment)(int, int);
	void (*vcache_flush_region)(int);
	void (*vcache_flush_context)(void);
	void (*pcache_flush_line)(int, int);
	void (*pure_vcache_flush)(void);
	void (*cache_flush_all)(void);
	void (*memerr)(unsigned, u_int, u_int, struct trapframe *);
};


struct cpu_softc;
struct cpu_info {
	struct cpu_softc *ci_softc;

	struct proc *ci_curproc;
	struct cpu_info *ci_next;

	struct schedstate_percpu ci_schedstate;
	u_int32_t 		ci_randseed;

#ifdef DIAGNOSTIC
	int	ci_mutex_level;
#endif
};

#define curcpu() (&cpuinfo.ci)
#define cpu_number() (cpuinfo.mid)
#define CPU_IS_PRIMARY(ci)	((ci)->ci_softc->master)
#define CPU_INFO_ITERATOR	int
#define CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = curcpu(); ci != NULL; ci = ci->ci_next)
#define CPU_INFO_UNIT(ci) ((ci)->ci_softc ? (ci)->ci_softc->dv.dv_unit : 0)
#define MAXCPUS	1
#define cpu_unidle(ci)

/*
 * The cpu_softc structure. This structure maintains information about one
 * currently installed CPU (there may be several of these if the machine
 * supports multiple CPUs, as on some Sun4m architectures). The information
 * in this structure supersedes the old "cpumod", "mmumod", and similar
 * fields.
 */

struct cpu_softc {
	struct device	dv;		/* generic device info */

	struct cpu_info ci;

	int		node;		/* PROM node for this CPU */

	/* CPU information */
	char		*cpu_name;	/* CPU model */
	int		cpu_impl;	/* CPU implementation code */
	int		cpu_vers;	/* CPU version code */
	int		mmu_impl;	/* MMU implementation code */
	int		mmu_vers;	/* MMU version code */
	int		master;		/* 1 if this is bootup CPU */

	int		mid;		/* Module ID for MP systems */
	int		mbus;		/* 1 if CPU is on MBus */
	int		mxcc;		/* 1 if a MBus-level MXCC is present */

	caddr_t		mailbox;	/* VA of CPU's mailbox */


	int		mmu_ncontext;	/* Number of contexts supported */
	int		mmu_nregion; 	/* Number of regions supported */
	int		mmu_nsegment;	/* [4/4c] Segments */
	int		mmu_npmeg;	/* [4/4c] Pmegs */
	int		sun4_mmu3l;	/* [4]: 3-level MMU present */
#if defined(SUN4_MMU3L)
#define HASSUN4_MMU3L	(cpuinfo.sun4_mmu3l)
#else
#define HASSUN4_MMU3L	(0)
#endif

	/* Context administration */
	int		*ctx_tbl;	/* [4m] SRMMU-edible context table */
	union ctxinfo	*ctxinfo;
	union ctxinfo	*ctx_freelist;  /* context free list */
	int		ctx_kick;	/* allocation rover when none free */
	int		ctx_kickdir;	/* ctx_kick roves both directions */

	/* MMU tables that map `cpuinfo'' on each CPU */
	int		*L1_ptps;	/* XXX */

/* XXX - of these, we currently use only cpu_type */
	int		arch;		/* Architecture: CPU_SUN4x */
	int		class;		/* Class: SuperSPARC, microSPARC... */
	int		classlvl;	/* Iteration in class: 1, 2, etc. */
	int		classsublvl;	/* stepping in class (version) */
	int		cpu_type;	/* Type: see CPUTYP_xxx below */

	int		hz;		/* Clock speed */

	/* Cache information */
	struct cacheinfo	cacheinfo;	/* see cache.h */

	/* FPU information */
	int		fpupresent;	/* true if FPU is present */
	int		fpuvers;	/* FPU revision */

	/* various flags to workaround anomalies in chips */
	int		flags;		/* see CPUFLG_xxx, below */

	/*
	 * The following pointers point to processes that are somehow
	 * associated with this CPU--running on it, using its FPU,
	 * etc.
	 *
	 * XXXMP: much more needs to go here
	 */
	struct	proc 	*fpproc;		/* FPU owner */

	/*
	 * The following are function pointers to do interesting CPU-dependent
	 * things without having to do type-tests all the time
	 */

	/* bootup things: access to physical memory */
	u_int	(*read_physmem)(u_int addr, int space);
	void	(*write_physmem)(u_int addr, u_int data);
	void	(*cache_tablewalks)(void);
	void	(*mmu_enable)(void);
	void	(*hotfix)(struct cpu_softc *);

	/* locore defined: */
	void	(*get_syncflt)(void);		/* Not C-callable */
	int	(*get_asyncflt)(u_int *, u_int *);

       	/* Synchronous Fault Status; temporary storage */
       	struct {
		int     sfsr;
		int     sfva;
	} syncfltdump;

	/* Cache handling functions */
	void	(*cache_enable)(void);
	void	(*cache_flush)(caddr_t, u_int);
	void	(*vcache_flush_page)(int);
	void	(*vcache_flush_segment)(int, int);
	void	(*vcache_flush_region)(int);
	void	(*vcache_flush_context)(void);
	void	(*pcache_flush_line)(int, int);
	void	(*pure_vcache_flush)(void);
	void	(*cache_flush_all)(void);

#ifdef SUN4M
	/* hardware-assisted block operation routines */
	void		(*hwbcopy)(const void *from, void *to, size_t len);
	void		(*hwbzero)(void *buf, size_t len);

	/* routine to clear mbus-sbus buffers */
	void		(*mbusflush)(void);
#endif

	/*
	 * Memory error handler; parity errors, unhandled NMIs and other
	 * unrecoverable faults end up here.
	 */
	void    (*memerr)(unsigned, u_int, u_int, struct trapframe *);
	/* XXX: Add more here! */
};

/*
 * CPU types. When nonzero, these enable system-specific behaviour.
 * The general form is
 * 	CPUTYP_proctype_bustype_cachetype_etc_etc
 */
#define CPUTYP_UNKNOWN		0

/* sun4 models */
#define CPUTYP_4_100		1 	/* Sun4/100 */
#define CPUTYP_4_200		2	/* Sun4/200 */
#define CPUTYP_4_300		3	/* Sun4/300 */
#define CPUTYP_4_400		4	/* Sun4/400 */

/* rough sun4m families; not really used */
#define CPUTYP_SS1_MBUS_MXCC	21	/* SuperSPARC-I, MBus, MXCC (SS10) */
#define CPUTYP_SS1_MBUS_NOMXCC	23	/* SuperSPARC-I, on MBus w/o MXCC */
#define CPUTYP_MS2		24	/* MicroSPARC-2 */
#define CPUTYP_MS1		25 	/* MicroSPARC-1 */
#define CPUTYP_HS_MBUS		26	/* MBus-based HyperSPARC */
#define CPUTYP_CYPRESS		27	/* MBus-based Cypress */

/*
 * CPU flags
 */
#define CPUFLG_CACHEPAGETABLES	0x1	/* caching pagetables OK on Sun4m */
#define CPUFLG_CACHEIOMMUTABLES	0x2	/* caching IOMMU translations OK */
#define CPUFLG_CACHEDVMA	0x4	/* DVMA goes through cache */
#define CPUFLG_SUN4CACHEBUG	0x8	/* trap page can't be cached */
#define CPUFLG_CACHE_MANDATORY	0x10	/* if cache is on, don't use
					   uncached access */

/*
 * Related function prototypes
 */
void getcpuinfo(struct cpu_softc *sc, int node);
void mmu_install_tables(struct cpu_softc *);
void pmap_alloc_cpu(struct cpu_softc *);

#define cpuinfo	(*(struct cpu_softc *)CPUINFO_VA)
#endif	/* _SPARC_CPUVAR_H */
