/* $OpenBSD: machdep.c,v 1.79 2002/12/17 23:11:31 millert Exp $ */
/* $NetBSD: machdep.c,v 1.210 2000/06/01 17:12:38 thorpej Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <machine/kcore.h>
#ifndef NO_IEEE
#include <machine/fpu.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/cpuconf.h>
#ifndef NO_IEEE
#include <machine/ieeefp.h>
#endif

#include <dev/pci/pcivar.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

int	cpu_dump(void);
int	cpu_dumpsize(void);
u_long	cpu_dump_mempagecnt(void);
void	dumpsys(void);
caddr_t allocsys(caddr_t);
void	identifycpu(void);
void	regdump(struct trapframe *framep);
void	printregs(struct reg *);

/*
 * Declare these as initialized data so we can patch them.
 */
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 10
#endif

#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int	bufcachepercent = BUFCACHEPERCENT;

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

#ifdef APERTURE
#ifdef INSECURE
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

int	maxmem;			/* max memory per process */

int	totalphysmem;		/* total amount of physical memory in system */
int	physmem;		/* physical mem used by OpenBSD + some rsvd */
int	resvmem;		/* amount of memory reserved for PROM */
int	unusedmem;		/* amount of memory for OS that we don't use */
int	unknownmem;		/* amount of memory with an unknown use */

int	cputype;		/* system type, from the RPB */

int	bootdev_debug = 0;	/* patchable, or from DDB */

/*
 * XXX We need an address to which we can assign things so that they
 * won't be optimized away because we didn't use the value.
 */
u_int32_t no_optimize;

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* from <machine/param.h> */
char	cpu_model[128];
char	root_device[17];

struct	user *proc0paddr;

/* Number of machine cycles per microsecond */
u_int64_t	cycles_per_usec;

/* number of cpus in the box.  really! */
int		ncpus;

struct bootinfo_kernel bootinfo;

/* For built-in TCDS */
#if defined(DEC_3000_300) || defined(DEC_3000_500)
u_int8_t	dec_3000_scsiid[2], dec_3000_scsifast[2];
#endif

struct platform platform;

/* for cpu_sysctl() */
int	alpha_unaligned_print = 1;	/* warn about unaligned accesses */
int	alpha_unaligned_fix = 1;	/* fix up unaligned accesses */
int	alpha_unaligned_sigbus = 1;	/* SIGBUS on fixed-up accesses */
#ifndef NO_IEEE
int	alpha_fp_sync_complete = 0;	/* fp fixup if sync even without /s */
#endif

/*
 * XXX This should be dynamically sized, but we have the chicken-egg problem!
 * XXX it should also be larger than it is, because not all of the mddt
 * XXX clusters end up being used for VM.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];	/* low size bits overloaded */
int	mem_cluster_cnt;

void
alpha_init(pfn, ptb, bim, bip, biv)
	u_long pfn;		/* first free PFN number */
	u_long ptb;		/* PFN of current level 1 page table */
	u_long bim;		/* bootinfo magic */
	u_long bip;		/* bootinfo pointer */
	u_long biv;		/* bootinfo version */
{
	extern char kernel_text[], _end[];
	struct mddt *mddtp;
	struct mddt_cluster *memc;
	int i, mddtweird;
	struct vm_physseg *vps;
	vaddr_t kernstart, kernend;
	paddr_t kernstartpfn, kernendpfn, pfn0, pfn1;
	vsize_t size;
	char *p;
	caddr_t v;
	const char *bootinfo_msg;
	const struct cpuinit *c;
	extern caddr_t esym;
	struct cpu_info *ci;
	cpuid_t cpu_id;

	/* NO OUTPUT ALLOWED UNTIL FURTHER NOTICE */

	/*
	 * Turn off interrupts (not mchecks) and floating point.
	 * Make sure the instruction and data streams are consistent.
	 */
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
	alpha_pal_wrfen(0);
	ALPHA_TBIA();
	alpha_pal_imb();

	cpu_id = cpu_number();

#if defined(MULTIPROCESSOR)
	/*
	 * Set our SysValue to the address of our cpu_info structure.
	 * Secondary processors do this in their spinup trampoline.
	 */
	alpha_pal_wrval((u_long)&cpu_info[cpu_id]);
#endif

	ci = curcpu();
	ci->ci_cpuid = cpu_id;

	/*
	 * Get critical system information (if possible, from the
	 * information provided by the boot program).
	 */
	bootinfo_msg = NULL;
	if (bim == BOOTINFO_MAGIC) {
		if (biv == 0) {		/* backward compat */
			biv = *(u_long *)bip;
			bip += 8;
		}
		switch (biv) {
		case 1: {
			struct bootinfo_v1 *v1p = (struct bootinfo_v1 *)bip;

			bootinfo.ssym = v1p->ssym;
			bootinfo.esym = v1p->esym;
			/* hwrpb may not be provided by boot block in v1 */
			if (v1p->hwrpb != NULL) {
				bootinfo.hwrpb_phys =
				    ((struct rpb *)v1p->hwrpb)->rpb_phys;
				bootinfo.hwrpb_size = v1p->hwrpbsize;
			} else {
				bootinfo.hwrpb_phys =
				    ((struct rpb *)HWRPB_ADDR)->rpb_phys;
				bootinfo.hwrpb_size =
				    ((struct rpb *)HWRPB_ADDR)->rpb_size;
			}
			bcopy(v1p->boot_flags, bootinfo.boot_flags,
			    min(sizeof v1p->boot_flags,
			      sizeof bootinfo.boot_flags));
			bcopy(v1p->booted_kernel, bootinfo.booted_kernel,
			    min(sizeof v1p->booted_kernel,
			      sizeof bootinfo.booted_kernel));
			/* booted dev not provided in bootinfo */
			init_prom_interface((struct rpb *)
			    ALPHA_PHYS_TO_K0SEG(bootinfo.hwrpb_phys));
                	prom_getenv(PROM_E_BOOTED_DEV, bootinfo.booted_dev,
			    sizeof bootinfo.booted_dev);
			break;
		}
		default:
			bootinfo_msg = "unknown bootinfo version";
			goto nobootinfo;
		}
	} else {
		bootinfo_msg = "boot program did not pass bootinfo";
nobootinfo:
		bootinfo.ssym = (u_long)_end;
		bootinfo.esym = (u_long)_end;
		bootinfo.hwrpb_phys = ((struct rpb *)HWRPB_ADDR)->rpb_phys;
		bootinfo.hwrpb_size = ((struct rpb *)HWRPB_ADDR)->rpb_size;
		init_prom_interface((struct rpb *)HWRPB_ADDR);
		prom_getenv(PROM_E_BOOTED_OSFLAGS, bootinfo.boot_flags,
		    sizeof bootinfo.boot_flags);
		prom_getenv(PROM_E_BOOTED_FILE, bootinfo.booted_kernel,
		    sizeof bootinfo.booted_kernel);
		prom_getenv(PROM_E_BOOTED_DEV, bootinfo.booted_dev,
		    sizeof bootinfo.booted_dev);
	}

	esym = (caddr_t)bootinfo.esym;
	/*
	 * Initialize the kernel's mapping of the RPB.  It's needed for
	 * lots of things.
	 */
	hwrpb = (struct rpb *)ALPHA_PHYS_TO_K0SEG(bootinfo.hwrpb_phys);

#if defined(DEC_3000_300) || defined(DEC_3000_500)
	if (hwrpb->rpb_type == ST_DEC_3000_300 ||
	    hwrpb->rpb_type == ST_DEC_3000_500) {
		prom_getenv(PROM_E_SCSIID, dec_3000_scsiid,
		    sizeof(dec_3000_scsiid));
		prom_getenv(PROM_E_SCSIFAST, dec_3000_scsifast,
		    sizeof(dec_3000_scsifast));
	}
#endif

	/*
	 * Remember how many cycles there are per microsecond, 
	 * so that we can use delay().  Round up, for safety.
	 */
	cycles_per_usec = (hwrpb->rpb_cc_freq + 999999) / 1000000;

	/*
	 * Initialize the (temporary) bootstrap console interface, so
	 * we can use printf until the VM system starts being setup.
	 * The real console is initialized before then.
	 */
	init_bootstrap_console();

	/* OUTPUT NOW ALLOWED */

	/* delayed from above */
	if (bootinfo_msg)
		printf("WARNING: %s (0x%lx, 0x%lx, 0x%lx)\n",
		    bootinfo_msg, bim, bip, biv);

	/* Initialize the trap vectors on the primary processor. */
	trap_init();

	/*
	 * Find out what hardware we're on, and do basic initialization.
	 */
	cputype = hwrpb->rpb_type;
	if (cputype < 0) {
		/*
		 * At least some white-box systems have SRM which
		 * reports a systype that's the negative of their
		 * blue-box counterpart.
		 */
		cputype = -cputype;
	}
	c = platform_lookup(cputype);
	if (c == NULL) {
		platform_not_supported();
		/* NOTREACHED */
	}
	(*c->init)();
	strcpy(cpu_model, platform.model);

	/*
	 * Initialize the real console, so that the bootstrap console is
	 * no longer necessary.
	 */
	(*platform.cons_init)();

#ifdef DIAGNOSTIC
	/* Paranoid sanity checking */

	/* We should always be running on the primary. */
	assert(hwrpb->rpb_primary_cpu_id == alpha_pal_whami());

	/*
	 * On single-CPU systypes, the primary should always be CPU 0,
	 * except on Alpha 8200 systems where the CPU id is related
	 * to the VID, which is related to the Turbo Laser node id.
	 */
	if (cputype != ST_DEC_21000)
		assert(hwrpb->rpb_primary_cpu_id == 0);
#endif

	/* NO MORE FIRMWARE ACCESS ALLOWED */
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
	/*
	 * XXX (unless _PMAP_MAY_USE_PROM_CONSOLE is defined and
	 * XXX pmap_uses_prom_console() evaluates to non-zero.)
	 */
#endif

	/*
	 * find out this system's page size
	 */
	if ((uvmexp.pagesize = hwrpb->rpb_page_size) != 8192)
		panic("page size %d != 8192?!", uvmexp.pagesize);

	uvm_setpagesize();

	/*
	 * Find the beginning and end of the kernel (and leave a
	 * bit of space before the beginning for the bootstrap
	 * stack).
	 */
	kernstart = trunc_page((vaddr_t)kernel_text) - 2 * PAGE_SIZE;
	kernend = (vaddr_t)round_page((vaddr_t)bootinfo.esym);

	kernstartpfn = atop(ALPHA_K0SEG_TO_PHYS(kernstart));
	kernendpfn = atop(ALPHA_K0SEG_TO_PHYS(kernend));

	/*
	 * Find out how much memory is available, by looking at
	 * the memory cluster descriptors.  This also tries to do
	 * its best to detect things things that have never been seen
	 * before...
	 */
	mddtp = (struct mddt *)(((caddr_t)hwrpb) + hwrpb->rpb_memdat_off);

	/* MDDT SANITY CHECKING */
	mddtweird = 0;
	if (mddtp->mddt_cluster_cnt < 2) {
		mddtweird = 1;
		printf("WARNING: weird number of mem clusters: %lu\n",
		    mddtp->mddt_cluster_cnt);
	}

#if 0
	printf("Memory cluster count: %d\n", mddtp->mddt_cluster_cnt);
#endif

	for (i = 0; i < mddtp->mddt_cluster_cnt; i++) {
		memc = &mddtp->mddt_clusters[i];
#if 0
		printf("MEMC %d: pfn 0x%lx cnt 0x%lx usage 0x%lx\n", i,
		    memc->mddt_pfn, memc->mddt_pg_cnt, memc->mddt_usage);
#endif
		totalphysmem += memc->mddt_pg_cnt;
		if (mem_cluster_cnt < VM_PHYSSEG_MAX) {	/* XXX */
			mem_clusters[mem_cluster_cnt].start =
			    ptoa(memc->mddt_pfn);
			mem_clusters[mem_cluster_cnt].size =
			    ptoa(memc->mddt_pg_cnt);
			if (memc->mddt_usage & MDDT_mbz ||
			    memc->mddt_usage & MDDT_NONVOLATILE || /* XXX */
			    memc->mddt_usage & MDDT_PALCODE)
				mem_clusters[mem_cluster_cnt].size |=
				    VM_PROT_READ;
			else
				mem_clusters[mem_cluster_cnt].size |=
				    VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
			mem_cluster_cnt++;
		}

		if (memc->mddt_usage & MDDT_mbz) {
			mddtweird = 1;
			printf("WARNING: mem cluster %d has weird "
			    "usage 0x%lx\n", i, memc->mddt_usage);
			unknownmem += memc->mddt_pg_cnt;
			continue;
		}
		if (memc->mddt_usage & MDDT_NONVOLATILE) {
			/* XXX should handle these... */
			printf("WARNING: skipping non-volatile mem "
			    "cluster %d\n", i);
			unusedmem += memc->mddt_pg_cnt;
			continue;
		}
		if (memc->mddt_usage & MDDT_PALCODE) {
			resvmem += memc->mddt_pg_cnt;
			continue;
		}

		/*
		 * We have a memory cluster available for system
		 * software use.  We must determine if this cluster
		 * holds the kernel.
		 */
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
		/*
		 * XXX If the kernel uses the PROM console, we only use the
		 * XXX memory after the kernel in the first system segment,
		 * XXX to avoid clobbering prom mapping, data, etc.
		 */
	    if (!pmap_uses_prom_console() || physmem == 0) {
#endif /* _PMAP_MAY_USE_PROM_CONSOLE */
		physmem += memc->mddt_pg_cnt;
		pfn0 = memc->mddt_pfn;
		pfn1 = memc->mddt_pfn + memc->mddt_pg_cnt;
		if (pfn0 <= kernstartpfn && kernendpfn <= pfn1) {
			/*
			 * Must compute the location of the kernel
			 * within the segment.
			 */
#if 0
			printf("Cluster %d contains kernel\n", i);
#endif
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
		    if (!pmap_uses_prom_console()) {
#endif /* _PMAP_MAY_USE_PROM_CONSOLE */
			if (pfn0 < kernstartpfn) {
				/*
				 * There is a chunk before the kernel.
				 */
#if 0
				printf("Loading chunk before kernel: "
				    "0x%lx / 0x%lx\n", pfn0, kernstartpfn);
#endif
				uvm_page_physload(pfn0, kernstartpfn,
				    pfn0, kernstartpfn, VM_FREELIST_DEFAULT);
			}
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
		    }
#endif /* _PMAP_MAY_USE_PROM_CONSOLE */
			if (kernendpfn < pfn1) {
				/*
				 * There is a chunk after the kernel.
				 */
#if 0
				printf("Loading chunk after kernel: "
				    "0x%lx / 0x%lx\n", kernendpfn, pfn1);
#endif
				uvm_page_physload(kernendpfn, pfn1,
				    kernendpfn, pfn1, VM_FREELIST_DEFAULT);
			}
		} else {
			/*
			 * Just load this cluster as one chunk.
			 */
#if 0
			printf("Loading cluster %d: 0x%lx / 0x%lx\n", i,
			    pfn0, pfn1);
#endif
			uvm_page_physload(pfn0, pfn1, pfn0, pfn1,
			    VM_FREELIST_DEFAULT);
		}
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
	    }
#endif /* _PMAP_MAY_USE_PROM_CONSOLE */
	}

	/*
	 * Dump out the MDDT if it looks odd...
	 */
	if (mddtweird) {
		printf("\n");
		printf("complete memory cluster information:\n");
		for (i = 0; i < mddtp->mddt_cluster_cnt; i++) {
			printf("mddt %d:\n", i);
			printf("\tpfn %lx\n",
			    mddtp->mddt_clusters[i].mddt_pfn);
			printf("\tcnt %lx\n",
			    mddtp->mddt_clusters[i].mddt_pg_cnt);
			printf("\ttest %lx\n",
			    mddtp->mddt_clusters[i].mddt_pg_test);
			printf("\tbva %lx\n",
			    mddtp->mddt_clusters[i].mddt_v_bitaddr);
			printf("\tbpa %lx\n",
			    mddtp->mddt_clusters[i].mddt_p_bitaddr);
			printf("\tbcksum %lx\n",
			    mddtp->mddt_clusters[i].mddt_bit_cksum);
			printf("\tusage %lx\n",
			    mddtp->mddt_clusters[i].mddt_usage);
		}
		printf("\n");
	}

	if (totalphysmem == 0)
		panic("can't happen: system seems to have no memory!");
	maxmem = physmem;
#if 0
	printf("totalphysmem = %d\n", totalphysmem);
	printf("physmem = %d\n", physmem);
	printf("resvmem = %d\n", resvmem);
	printf("unusedmem = %d\n", unusedmem);
	printf("unknownmem = %d\n", unknownmem);
#endif

	/*
	 * Initialize error message buffer (at end of core).
	 */
	{
		vsize_t sz = (vsize_t)round_page(MSGBUFSIZE);
		vsize_t reqsz = sz;

		vps = &vm_physmem[vm_nphysseg - 1];

		/* shrink so that it'll fit in the last segment */
		if ((vps->avail_end - vps->avail_start) < atop(sz))
			sz = ptoa(vps->avail_end - vps->avail_start);

		vps->end -= atop(sz);
		vps->avail_end -= atop(sz);
		initmsgbuf((caddr_t) ALPHA_PHYS_TO_K0SEG(ptoa(vps->end)), sz);

		/* Remove the last segment if it now has no pages. */
		if (vps->start == vps->end)
			vm_nphysseg--;

		/* warn if the message buffer had to be shrunk */
		if (sz != reqsz)
			printf("WARNING: %ld bytes not available for msgbuf "
			    "in last cluster (%ld used)\n", reqsz, sz);

	}

	/*
	 * Init mapping for u page(s) for proc 0
	 */
	proc0.p_addr = proc0paddr =
	    (struct user *)pmap_steal_memory(UPAGES * PAGE_SIZE, NULL, NULL);

	/*
	 * Allocate space for system data structures.  These data structures
	 * are allocated here instead of cpu_startup() because physical
	 * memory is directly addressable.  We don't have to map these into
	 * virtual address space.
	 */
	size = (vsize_t)allocsys(NULL);
	v = (caddr_t)pmap_steal_memory(size, NULL, NULL);
	if ((allocsys(v) - v) != size)
		panic("alpha_init: table size inconsistency");

	/*
	 * Clear allocated memory.
	 */
	bzero(v, size);

	/*
	 * Initialize the virtual memory system, and set the
	 * page table base register in proc 0's PCB.
	 */
	pmap_bootstrap(ALPHA_PHYS_TO_K0SEG(ptb << PGSHIFT),
	    hwrpb->rpb_max_asn, hwrpb->rpb_pcs_cnt);

	/*
	 * Initialize the rest of proc 0's PCB, and cache its physical
	 * address.
	 */
	proc0.p_md.md_pcbpaddr =
	    (struct pcb *)ALPHA_K0SEG_TO_PHYS((vaddr_t)&proc0paddr->u_pcb);

	/*
	 * Set the kernel sp, reserving space for an (empty) trapframe,
	 * and make proc0's trapframe pointer point to it for sanity.
	 */
	proc0paddr->u_pcb.pcb_hw.apcb_ksp =
	    (u_int64_t)proc0paddr + USPACE - sizeof(struct trapframe);
	proc0.p_md.md_tf =
	    (struct trapframe *)proc0paddr->u_pcb.pcb_hw.apcb_ksp;

	/*
	 * Initialize the primary CPU's idle PCB to proc0's.  In a
	 * MULTIPROCESSOR configuration, each CPU will later get
	 * its own idle PCB when autoconfiguration runs.
	 */
	ci->ci_idle_pcb = &proc0paddr->u_pcb;
	ci->ci_idle_pcb_paddr = (u_long)proc0.p_md.md_pcbpaddr;

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */

	boothowto = RB_SINGLE;
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	for (p = bootinfo.boot_flags; p && *p != '\0'; p++) {
		/*
		 * Note that we'd really like to differentiate case here,
		 * but the Alpha AXP Architecture Reference Manual
		 * says that we shouldn't.
		 */
		switch (*p) {
		case 'a': /* autoboot */
		case 'A':
			boothowto &= ~RB_SINGLE;
			break;

		case 'b': /* Enter DDB as soon as the console is initialised */
		case 'B':
			boothowto |= RB_KDB;
			break;

		case 'c': /* enter user kernel configuration */
		case 'C':
			boothowto |= RB_CONFIG;
			break;

#ifdef DEBUG
		case 'd': /* crash dump immediately after autoconfig */
		case 'D':
			boothowto |= RB_DUMP;
			break;
#endif

		case 'h': /* always halt, never reboot */
		case 'H':
			boothowto |= RB_HALT;
			break;

#if 0
		case 'm': /* mini root present in memory */
		case 'M':
			boothowto |= RB_MINIROOT;
			break;
#endif

		case 'n': /* askname */
		case 'N':
			boothowto |= RB_ASKNAME;
			break;

		case 's': /* single-user (default, supported for sanity) */
		case 'S':
			boothowto |= RB_SINGLE;
			break;

		case '-':
			/*
			 * Just ignore this.  It's not required, but it's
			 * common for it to be passed regardless.
			 */
			break;

		default:
			printf("Unrecognized boot flag '%c'.\n", *p);
			break;
		}
	}


	/*
	 * Figure out the number of cpus in the box, from RPB fields.
	 * Really.  We mean it.
	 */
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		struct pcs *pcsp;

		pcsp = LOCATE_PCS(hwrpb, i);
		if ((pcsp->pcs_flags & PCS_PP) != 0)
			ncpus++;
	}

	/*
	 * Initialize debuggers, and break into them if appropriate.
	 */
#ifdef DDB
	ddb_init();

	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef KGDB
	if (boothowto & RB_KDB)
		kgdb_connect(0);
#endif
	/*
	 * Figure out our clock frequency, from RPB fields.
	 */
	hz = hwrpb->rpb_intr_freq >> 12;
	if (!(60 <= hz && hz <= 10240)) {
		hz = 1024;
#ifdef DIAGNOSTIC
		printf("WARNING: unbelievable rpb_intr_freq: %ld (%d hz)\n",
			hwrpb->rpb_intr_freq, hz);
#endif
	}
}

caddr_t
allocsys(v)
	caddr_t v;
{
	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 *
	 * These data structures are allocated here instead of cpu_startup()
	 * because physical memory is directly addressable. We don't have
	 * to map these into virtual address space.
	 */
#define valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)ALIGN((name)+(num))

#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate.
	 * We allocate 10% of memory for buffer space.  Insure a
	 * minimum of 16 buffers.
	 */
	if (bufpages == 0)
		bufpages = (physmem / (100/bufcachepercent));
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	valloc(buf, struct buf, nbuf);

#undef valloc

	return v;
}

void
consinit()
{

	/*
	 * Everything related to console initialization is done
	 * in alpha_init().
	 */
#if defined(DIAGNOSTIC) && defined(_PMAP_MAY_USE_PROM_CONSOLE)
	printf("consinit: %susing prom console\n",
	    pmap_uses_prom_console() ? "" : "not ");
#endif
}

#include "pckbc.h"
#include "pckbd.h"
#if (NPCKBC > 0) && (NPCKBD == 0)

#include <dev/ic/pckbcvar.h>

/*
 * This is called by the pckbc driver if no pckbd is configured.
 * On the i386, it is used to glue in the old, deprecated console
 * code.  On the Alpha, it does nothing.
 */
int
pckbc_machdep_cnattach(kbctag, kbcslot)
	pckbc_tag_t kbctag;
	pckbc_slot_t kbcslot;
{
	return (ENXIO);
}
#endif /* NPCKBC > 0 && NPCKBD == 0 */

void
cpu_startup()
{
	register unsigned i;
	int base, residual;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
#if defined(DEBUG)
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("total memory = %ld (%ldK)\n", (long)ptoa(totalphysmem),
	    (long)ptoa(totalphysmem) / 1024);
	printf("(%ld reserved for PROM, ", (long)ptoa(resvmem));
	printf("%ld used by OpenBSD)\n", (long)ptoa(physmem));
	if (unusedmem) {
		printf("WARNING: unused memory = %ld (%ldK)\n",
		    (long)ptoa(unusedmem), (long)ptoa(unusedmem) / 1024);
	}
	if (unknownmem) {
		printf("WARNING: %ld (%ldK) of memory with unknown purpose\n",
		    (long)ptoa(unknownmem), (long)ptoa(unknownmem) / 1024);
	}

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)))
		panic("startup: cannot allocate VM for buffers");
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.  Of
		 * that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t) buffers + (i * MAXBSIZE);
		curbufsize = NBPG * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for "
				    "buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
					VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
		pmap_update(pmap_kernel());
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

#if defined(DEBUG)
	pmapdebug = opmapdebug;
#endif
	printf("avail memory = %ld (%ldK)\n", (long)ptoa(uvmexp.free),
	    (long)ptoa(uvmexp.free) / 1024);
#if 0
	{
		extern u_long pmap_pages_stolen;

		printf("stolen memory for VM structures = %d\n", pmap_pages_stolen * PAGE_SIZE);
	}
#endif
	printf("using %ld buffers containing %ld bytes (%ldK) of memory\n",
	    (long)nbuf, (long)bufpages * NBPG, (long)bufpages * (NBPG / 1024));

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}

	/*
	 * Set up the HWPCB so that it's safe to configure secondary
	 * CPUs.
	 */
	hwrpb_primary_init();
}

/*
 * Retrieve the platform name from the DSR.
 */
const char *
alpha_dsr_sysname()
{
	struct dsrdb *dsr;
	const char *sysname;

	/*
	 * DSR does not exist on early HWRPB versions.
	 */
	if (hwrpb->rpb_version < HWRPB_DSRDB_MINVERS)
		return (NULL);

	dsr = (struct dsrdb *)(((caddr_t)hwrpb) + hwrpb->rpb_dsrdb_off);
	sysname = (const char *)((caddr_t)dsr + (dsr->dsr_sysname_off +
	    sizeof(u_int64_t)));
	return (sysname);
}

/*
 * Lookup the system specified system variation in the provided table,
 * returning the model string on match.
 */
const char *
alpha_variation_name(variation, avtp)
	u_int64_t variation;
	const struct alpha_variation_table *avtp;
{
	int i;

	for (i = 0; avtp[i].avt_model != NULL; i++)
		if (avtp[i].avt_variation == variation)
			return (avtp[i].avt_model);
	return (NULL);
}

/*
 * Generate a default platform name based for unknown system variations.
 */
const char *
alpha_unknown_sysname()
{
	static char s[128];		/* safe size */

	sprintf(s, "%s family, unknown model variation 0x%lx",
	    platform.family, hwrpb->rpb_variation & SV_ST_MASK);
	return ((const char *)s);
}

void
identifycpu()
{
	char *s;

	/*
	 * print out CPU identification information.
	 */
	printf("%s", cpu_model);
	for(s = cpu_model; *s; ++s)
		if(strncasecmp(s, "MHz", 3) == 0)
			goto skipMHz;
	printf(", %ldMHz", hwrpb->rpb_cc_freq / 1000000);
skipMHz:
	printf("\n");
	printf("%ld byte page size, %d processor%s.\n",
	    hwrpb->rpb_page_size, ncpus, ncpus == 1 ? "" : "s");
#if 0
	/* this isn't defined for any systems that we run on? */
	printf("serial number 0x%lx 0x%lx\n",
	    ((long *)hwrpb->rpb_ssn)[0], ((long *)hwrpb->rpb_ssn)[1]);

	/* and these aren't particularly useful! */
	printf("variation: 0x%lx, revision 0x%lx\n",
	    hwrpb->rpb_variation, *(long *)hwrpb->rpb_revision);
#endif
}

int	waittime = -1;
struct pcb dumppcb;

void
boot(howto)
	int howto;
{
#if defined(MULTIPROCESSOR)
#if 0 /* XXX See below. */
	u_long cpu_id;
#endif
#endif

#if defined(MULTIPROCESSOR)
	/* We must be running on the primary CPU. */
	if (alpha_pal_whami() != hwrpb->rpb_primary_cpu_id)
		panic("cpu_reboot: not on primary CPU!");
#endif

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	/* If "always halt" was specified as a boot flag, obey. */
	if ((boothowto & RB_HALT) != 0)
		howto |= RB_HALT;

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:

	/* run any shutdown hooks */
	doshutdownhooks();

#if defined(MULTIPROCESSOR)
#if 0 /* XXX doesn't work when called from here?! */
	/* Kill off any secondary CPUs. */
	for (cpu_id = 0; cpu_id < hwrpb->rpb_pcs_cnt; cpu_id++) {
		if (cpu_id == hwrpb->rpb_primary_cpu_id ||
		    cpu_info[cpu_id].ci_softc == NULL)
			continue;
		cpu_halt_secondary(cpu_id);
	}
#endif
#endif

#ifdef BOOTKEY
	printf("hit any key to %s...\n", howto & RB_HALT ? "halt" : "reboot");
	cnpollc(1);	/* for proper keyboard command handling */
	cngetc();
	cnpollc(0);
	printf("\n");
#endif

	/* Finally, powerdown/halt/reboot the system. */
	if ((howto & RB_POWERDOWN) == RB_POWERDOWN &&
	    platform.powerdown != NULL) {
		(*platform.powerdown)();
		printf("WARNING: powerdown failed!\n");
	}
	printf("%s\n\n", howto & RB_HALT ? "halted." : "rebooting...");
	prom_halt(howto & RB_HALT);
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers.
 */
int
cpu_dumpsize()
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t)) +
	    ALIGN(mem_cluster_cnt * sizeof(phys_ram_seg_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return -1;

	return (1);
}

/*
 * cpu_dump_mempagecnt: calculate size of RAM (in pages) to be dumped.
 */
u_long
cpu_dump_mempagecnt()
{
	u_long i, n;

	n = 0;
	for (i = 0; i < mem_cluster_cnt; i++)
		n += atop(mem_clusters[i].size);
	return (n);
}

/*
 * cpu_dump: dump machine-dependent kernel core dump headers.
 */
int
cpu_dump()
{
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	char buf[dbtob(1)];
	kcore_seg_t *segp;
	cpu_kcore_hdr_t *cpuhdrp;
	phys_ram_seg_t *memsegp;
	int i;

	dump = bdevsw[major(dumpdev)].d_dump;

	bzero(buf, sizeof buf);
	segp = (kcore_seg_t *)buf;
	cpuhdrp = (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(*segp))];
	memsegp = (phys_ram_seg_t *)&buf[ALIGN(sizeof(*segp)) +
	    ALIGN(sizeof(*cpuhdrp))];

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	/*
	 * Add the machine-dependent header info.
	 */
	cpuhdrp->lev1map_pa = ALPHA_K0SEG_TO_PHYS((vaddr_t)kernel_lev1map);
	cpuhdrp->page_size = PAGE_SIZE;
	cpuhdrp->nmemsegs = mem_cluster_cnt;

	/*
	 * Fill in the memory segment descriptors.
	 */
	for (i = 0; i < mem_cluster_cnt; i++) {
		memsegp[i].start = mem_clusters[i].start;
		memsegp[i].size = mem_clusters[i].size & ~PAGE_MASK;
	}

	return (dump(dumpdev, dumplo, (caddr_t)buf, dbtob(1)));
}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first NBPG of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf()
{
	int nblks, dumpblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		goto bad;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		goto bad;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		goto bad;

	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		goto bad;
	dumpblks += ctod(cpu_dump_mempagecnt());

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1)))
		goto bad;

	/* Put dump at end of partition */
	dumplo = nblks - dumpblks;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = cpu_dump_mempagecnt();
	return;

bad:
	dumpsize = 0;
	return;
}

/*
 * Dump the kernel's image to the swap partition.
 */
#define	BYTES_PER_DUMP	NBPG

void
dumpsys()
{
	u_long totalbytesleft, bytes, i, n, memcl;
	u_long maddr;
	int psize;
	daddr_t blkno;
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	int error;
	extern int msgbufmapped;

	/* Save registers. */
	savectx(&dumppcb);

	msgbufmapped = 0;	/* don't record dump msgs in msgbuf */
	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	/* XXX should purge all outstanding keystrokes. */

	if ((error = cpu_dump()) != 0)
		goto err;

	totalbytesleft = ptoa(cpu_dump_mempagecnt());
	blkno = dumplo + cpu_dumpsize();
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;

	for (memcl = 0; memcl < mem_cluster_cnt; memcl++) {
		maddr = mem_clusters[memcl].start;
		bytes = mem_clusters[memcl].size & ~PAGE_MASK;

		for (i = 0; i < bytes; i += n, totalbytesleft -= n) {

			/* Print out how many MBs we to go. */
			if ((totalbytesleft % (1024*1024)) == 0)
				printf("%ld ", totalbytesleft / (1024 * 1024));

			/* Limit size for next transfer. */
			n = bytes - i;
			if (n > BYTES_PER_DUMP)
				n =  BYTES_PER_DUMP;
	
			error = (*dump)(dumpdev, blkno,
			    (caddr_t)ALPHA_PHYS_TO_K0SEG(maddr), n);
			if (error)
				goto err;
			maddr += n;
			blkno += btodb(n);			/* XXX? */

			/* XXX should look for keystrokes, to cancel. */
		}
	}

err:
	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
	printf("\n\n");
	delay(1000);
}

void
frametoreg(framep, regp)
	struct trapframe *framep;
	struct reg *regp;
{

	regp->r_regs[R_V0] = framep->tf_regs[FRAME_V0];
	regp->r_regs[R_T0] = framep->tf_regs[FRAME_T0];
	regp->r_regs[R_T1] = framep->tf_regs[FRAME_T1];
	regp->r_regs[R_T2] = framep->tf_regs[FRAME_T2];
	regp->r_regs[R_T3] = framep->tf_regs[FRAME_T3];
	regp->r_regs[R_T4] = framep->tf_regs[FRAME_T4];
	regp->r_regs[R_T5] = framep->tf_regs[FRAME_T5];
	regp->r_regs[R_T6] = framep->tf_regs[FRAME_T6];
	regp->r_regs[R_T7] = framep->tf_regs[FRAME_T7];
	regp->r_regs[R_S0] = framep->tf_regs[FRAME_S0];
	regp->r_regs[R_S1] = framep->tf_regs[FRAME_S1];
	regp->r_regs[R_S2] = framep->tf_regs[FRAME_S2];
	regp->r_regs[R_S3] = framep->tf_regs[FRAME_S3];
	regp->r_regs[R_S4] = framep->tf_regs[FRAME_S4];
	regp->r_regs[R_S5] = framep->tf_regs[FRAME_S5];
	regp->r_regs[R_S6] = framep->tf_regs[FRAME_S6];
	regp->r_regs[R_A0] = framep->tf_regs[FRAME_A0];
	regp->r_regs[R_A1] = framep->tf_regs[FRAME_A1];
	regp->r_regs[R_A2] = framep->tf_regs[FRAME_A2];
	regp->r_regs[R_A3] = framep->tf_regs[FRAME_A3];
	regp->r_regs[R_A4] = framep->tf_regs[FRAME_A4];
	regp->r_regs[R_A5] = framep->tf_regs[FRAME_A5];
	regp->r_regs[R_T8] = framep->tf_regs[FRAME_T8];
	regp->r_regs[R_T9] = framep->tf_regs[FRAME_T9];
	regp->r_regs[R_T10] = framep->tf_regs[FRAME_T10];
	regp->r_regs[R_T11] = framep->tf_regs[FRAME_T11];
	regp->r_regs[R_RA] = framep->tf_regs[FRAME_RA];
	regp->r_regs[R_T12] = framep->tf_regs[FRAME_T12];
	regp->r_regs[R_AT] = framep->tf_regs[FRAME_AT];
	regp->r_regs[R_GP] = framep->tf_regs[FRAME_GP];
	/* regp->r_regs[R_SP] = framep->tf_regs[FRAME_SP]; XXX */
	regp->r_regs[R_ZERO] = 0;
}

void
regtoframe(regp, framep)
	struct reg *regp;
	struct trapframe *framep;
{

	framep->tf_regs[FRAME_V0] = regp->r_regs[R_V0];
	framep->tf_regs[FRAME_T0] = regp->r_regs[R_T0];
	framep->tf_regs[FRAME_T1] = regp->r_regs[R_T1];
	framep->tf_regs[FRAME_T2] = regp->r_regs[R_T2];
	framep->tf_regs[FRAME_T3] = regp->r_regs[R_T3];
	framep->tf_regs[FRAME_T4] = regp->r_regs[R_T4];
	framep->tf_regs[FRAME_T5] = regp->r_regs[R_T5];
	framep->tf_regs[FRAME_T6] = regp->r_regs[R_T6];
	framep->tf_regs[FRAME_T7] = regp->r_regs[R_T7];
	framep->tf_regs[FRAME_S0] = regp->r_regs[R_S0];
	framep->tf_regs[FRAME_S1] = regp->r_regs[R_S1];
	framep->tf_regs[FRAME_S2] = regp->r_regs[R_S2];
	framep->tf_regs[FRAME_S3] = regp->r_regs[R_S3];
	framep->tf_regs[FRAME_S4] = regp->r_regs[R_S4];
	framep->tf_regs[FRAME_S5] = regp->r_regs[R_S5];
	framep->tf_regs[FRAME_S6] = regp->r_regs[R_S6];
	framep->tf_regs[FRAME_A0] = regp->r_regs[R_A0];
	framep->tf_regs[FRAME_A1] = regp->r_regs[R_A1];
	framep->tf_regs[FRAME_A2] = regp->r_regs[R_A2];
	framep->tf_regs[FRAME_A3] = regp->r_regs[R_A3];
	framep->tf_regs[FRAME_A4] = regp->r_regs[R_A4];
	framep->tf_regs[FRAME_A5] = regp->r_regs[R_A5];
	framep->tf_regs[FRAME_T8] = regp->r_regs[R_T8];
	framep->tf_regs[FRAME_T9] = regp->r_regs[R_T9];
	framep->tf_regs[FRAME_T10] = regp->r_regs[R_T10];
	framep->tf_regs[FRAME_T11] = regp->r_regs[R_T11];
	framep->tf_regs[FRAME_RA] = regp->r_regs[R_RA];
	framep->tf_regs[FRAME_T12] = regp->r_regs[R_T12];
	framep->tf_regs[FRAME_AT] = regp->r_regs[R_AT];
	framep->tf_regs[FRAME_GP] = regp->r_regs[R_GP];
	/* framep->tf_regs[FRAME_SP] = regp->r_regs[R_SP]; XXX */
	/* ??? = regp->r_regs[R_ZERO]; */
}

void
printregs(regp)
	struct reg *regp;
{
	int i;

	for (i = 0; i < 32; i++)
		printf("R%d:\t0x%016lx%s", i, regp->r_regs[i],
		   i & 1 ? "\n" : "\t");
}

void
regdump(framep)
	struct trapframe *framep;
{
	struct reg reg;

	frametoreg(framep, &reg);
	reg.r_regs[R_SP] = alpha_pal_rdusp();

	printf("REGISTERS:\n");
	printregs(&reg);
}

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define	SDB_FOLLOW	0x01
#define	SDB_KSTACK	0x02
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	struct proc *p = curproc;
	struct sigcontext *scp, ksc;
	struct trapframe *frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack, fsize, rndfsize, kscsize;
	siginfo_t *sip, ksi;

	frame = p->p_md.md_tf;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	fsize = sizeof ksc;
	rndfsize = ((fsize + 15) / 16) * 16;
	kscsize = rndfsize;
	if (psp->ps_siginfo & sigmask(sig)) {
		fsize += sizeof ksi;
		rndfsize = ((fsize + 15) / 16) * 16;
	}

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to uvm_grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		scp = (struct sigcontext *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - rndfsize);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		scp = (struct sigcontext *)(alpha_pal_rdusp() - rndfsize);
	if ((u_long)scp <= USRSTACK - ctob(p->p_vmspace->vm_ssize))
		(void)uvm_grow(p, (u_long)scp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %p usp %p\n", p->p_pid,
		    sig, &oonstack, scp);
#endif
	if (uvm_useracc((caddr_t)scp, fsize, B_WRITE) == 0) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig(%d): uvm_useracc failed on sig %d\n",
			    p->p_pid, sig);
#endif
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	ksc.sc_onstack = oonstack;
	ksc.sc_mask = mask;
	ksc.sc_pc = frame->tf_regs[FRAME_PC];
	ksc.sc_ps = frame->tf_regs[FRAME_PS];

	/* copy the registers. */
	frametoreg(frame, (struct reg *)ksc.sc_regs);
	ksc.sc_regs[R_ZERO] = 0xACEDBADE;		/* magic number */
	ksc.sc_regs[R_SP] = alpha_pal_rdusp();

	/* save the floating-point state, if necessary, then copy it. */
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 1);
	ksc.sc_ownedfp = p->p_md.md_flags & MDP_FPUSED;
	memcpy((struct fpreg *)ksc.sc_fpregs, &p->p_addr->u_pcb.pcb_fp,
	    sizeof(struct fpreg));
#ifndef NO_IEEE
	ksc.sc_fp_control = alpha_read_fp_c(p);
#else
	ksc.sc_fp_control = 0;
#endif
	memset(ksc.sc_reserved, 0, sizeof ksc.sc_reserved);	/* XXX */
	memset(ksc.sc_xxx, 0, sizeof ksc.sc_xxx);		/* XXX */

#ifdef COMPAT_OSF1
	/*
	 * XXX Create an OSF/1-style sigcontext and associated goo.
	 */
#endif

	if (psp->ps_siginfo & sigmask(sig)) {
		initsiginfo(&ksi, sig, code, type, val);
		sip = (void *)scp + kscsize;
		(void) copyout((caddr_t)&ksi, (caddr_t)sip, fsize - kscsize);
	} else
		sip = NULL;

	/*
	 * copy the frame out to userland.
	 */
	(void) copyout((caddr_t)&ksc, (caddr_t)scp, kscsize);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): sig %d scp %p code %lx\n", p->p_pid, sig,
		    scp, code);
#endif

	/*
	 * Set up the registers to return to sigcode.
	 */
	frame->tf_regs[FRAME_PC] = p->p_sigcode;
	frame->tf_regs[FRAME_A0] = sig;
	frame->tf_regs[FRAME_A1] = (u_int64_t)sip;
	frame->tf_regs[FRAME_A2] = (u_int64_t)scp;
	frame->tf_regs[FRAME_T12] = (u_int64_t)catcher;		/* t12 is pv */
	alpha_pal_wrusp((unsigned long)scp);

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): pc %lx, catcher %lx\n", p->p_pid,
		    frame->tf_regs[FRAME_PC], frame->tf_regs[FRAME_A3]);
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d returns\n",
		    p->p_pid, sig);
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext ksc;
	int error;

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
	    printf("sigreturn: pid %d, scp %p\n", p->p_pid, scp);
#endif

	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if ((error = copyin(SCARG(uap, sigcntxp), &ksc, sizeof(ksc))) != 0)
		return (error);

	if (ksc.sc_regs[R_ZERO] != 0xACEDBADE)		/* magic number */
		return (EINVAL);
	/*
	 * Restore the user-supplied information
	 */
	if (ksc.sc_onstack)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = ksc.sc_mask &~ sigcantmask;

	p->p_md.md_tf->tf_regs[FRAME_PC] = ksc.sc_pc;
	p->p_md.md_tf->tf_regs[FRAME_PS] =
	    (ksc.sc_ps | ALPHA_PSL_USERSET) & ~ALPHA_PSL_USERCLR;

	regtoframe((struct reg *)ksc.sc_regs, p->p_md.md_tf);
	alpha_pal_wrusp(ksc.sc_regs[R_SP]);

	/* XXX ksc.sc_ownedfp ? */
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 0);
	memcpy(&p->p_addr->u_pcb.pcb_fp, (struct fpreg *)ksc.sc_fpregs,
	    sizeof(struct fpreg));
#ifndef NO_IEEE
	p->p_addr->u_pcb.pcb_fp.fpr_cr = ksc.sc_fpcr;
	p->p_md.md_flags = ksc.sc_fp_control & MDP_FP_C;
#endif

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	dev_t consdev;

	if (name[0] != CPU_CHIPSET && namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
			sizeof consdev));

	case CPU_ROOT_DEVICE:
		return (sysctl_rdstring(oldp, oldlenp, newp,
		    root_device));

	case CPU_UNALIGNED_PRINT:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &alpha_unaligned_print));

	case CPU_UNALIGNED_FIX:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &alpha_unaligned_fix));

	case CPU_UNALIGNED_SIGBUS:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &alpha_unaligned_sigbus));

	case CPU_BOOTED_KERNEL:
		return (sysctl_rdstring(oldp, oldlenp, newp,
		    bootinfo.booted_kernel));
	
	case CPU_CHIPSET:
		return (alpha_sysctl_chipset(name + 1, namelen - 1, oldp,
		    oldlenp));

#ifndef NO_IEEE
	case CPU_FP_SYNC_COMPLETE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &alpha_fp_sync_complete));
#endif
	case CPU_ALLOWAPERTURE:
#ifdef APERTURE
		if (securelevel > 0)
                        return (sysctl_rdint(oldp, oldlenp, newp,
				 allowaperture));
                else
                        return (sysctl_int(oldp, oldlenp, newp, newlen,
                            &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Set registers on exec.
 */
void
setregs(p, pack, stack, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct trapframe *tfp = p->p_md.md_tf;
#ifdef DEBUG
	int i;
#endif

#ifdef DEBUG
	/*
	 * Crash and dump, if the user requested it.
	 */
	if (boothowto & RB_DUMP)
		panic("crash requested by boot flags");
#endif

#ifdef DEBUG
	for (i = 0; i < FRAME_SIZE; i++)
		tfp->tf_regs[i] = 0xbabefacedeadbeef;
#else
	bzero(tfp->tf_regs, FRAME_SIZE * sizeof tfp->tf_regs[0]);
#endif
	bzero(&p->p_addr->u_pcb.pcb_fp, sizeof p->p_addr->u_pcb.pcb_fp);
	alpha_pal_wrusp(stack);
	tfp->tf_regs[FRAME_PS] = ALPHA_PSL_USERSET;
	tfp->tf_regs[FRAME_PC] = pack->ep_entry & ~3;

	tfp->tf_regs[FRAME_A0] = stack;
	/* a1 and a2 already zeroed */
	tfp->tf_regs[FRAME_T12] = tfp->tf_regs[FRAME_PC];	/* a.k.a. PV */

	p->p_md.md_flags &= ~MDP_FPUSED;
#ifndef NO_IEEE
	if (__predict_true((p->p_md.md_flags & IEEE_INHERIT) == 0)) {
		p->p_md.md_flags &= ~MDP_FP_C;
		p->p_addr->u_pcb.pcb_fp.fpr_cr = FPCR_DYN(FP_RN);
	}
#endif
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 0);
}

/*
 * Release the FPU.
 */
void
fpusave_cpu(struct cpu_info *ci, int save)
{
	struct proc *p;
#if defined(MULTIPROCESSOR)
	int s;
#endif

	KDASSERT(ci == curcpu());

#if defined(MULTIPROCESSOR)
	atomic_setbits_ulong(&ci->ci_flags, CPUF_FPUSAVE);
#endif

	p = ci->ci_fpcurproc;
	if (p == NULL)
		goto out;

	if (save) {
		alpha_pal_wrfen(1);
		savefpstate(&p->p_addr->u_pcb.pcb_fp);
	}

	alpha_pal_wrfen(0);

	p->p_addr->u_pcb.pcb_fpcpu = NULL;
	ci->ci_fpcurproc = NULL;

out:
#if defined(MULTIPROCESSOR)
	atomic_clearbits_ulong(&ci->ci_flags, CPUF_FPUSAVE);
#endif
	return;
}

/*
 * Synchronize FP state for this process.
 */
void
fpusave_proc(struct proc *p, int save)
{
	struct cpu_info *ci = curcpu();
	struct cpu_info *oci;
#if defined(MULTIPROCESSOR)
	u_long ipi = save ? ALPHA_IPI_SYNCH_FPU : ALPHA_IPI_DISCARD_FPU;
	int s, spincount;
#endif

	KDASSERT(p->p_addr != NULL);
	KDASSERT(p->p_flag & P_INMEM);

	oci = p->p_addr->u_pcb.pcb_fpcpu;
	if (oci == NULL) {
		return;
	}

#if defined(MULTIPROCESSOR)
	if (oci == ci) {
		KASSERT(ci->ci_fpcurproc == p);
		fpusave_cpu(ci, save);
		return;
	}

	KASSERT(oci->ci_fpcurproc == p);
	alpha_send_ipi(oci->ci_cpuid, ipi);

	spincount = 0;
	while (p->p_addr->u_pcb.pcb_fpcpu != NULL) {
		spincount++;
		delay(1000);    /* XXX */
		if (spincount > 10000)
			panic("fpsave ipi didn't");
	}
#else
	KASSERT(ci->ci_fpcurproc == p);
	fpusave_cpu(ci, save);
#endif /* MULTIPROCESSOR */
}

int
spl0()
{

	if (ssir) {
		(void) alpha_pal_swpipl(ALPHA_PSL_IPL_SOFT);
		softintr_dispatch();
	}

	return (alpha_pal_swpipl(ALPHA_PSL_IPL_0));
}

/*
 * The following primitives manipulate the run queues.  _whichqs tells which
 * of the 32 queues _qs have processes in them.  Setrunqueue puts processes
 * into queues, Remrunqueue removes them from queues.  The running process is
 * on no queue, other processes are on a queue related to p->p_priority,
 * divided by 4 actually to shrink the 0-127 range of priorities into the 32
 * available queues.
 */
/*
 * setrunqueue(p)
 *	proc *p;
 *
 * Call should be made at splclock(), and p->p_stat should be SRUN.
 */

/* XXXART - grmble */
#define sched_qs qs
#define sched_whichqs whichqs

void
setrunqueue(p)
	struct proc *p;
{
	int bit;

	/* firewall: p->p_back must be NULL */
	if (p->p_back != NULL)
		panic("setrunqueue");

	bit = p->p_priority >> 2;
	sched_whichqs |= (1 << bit);
	p->p_forw = (struct proc *)&sched_qs[bit];
	p->p_back = sched_qs[bit].ph_rlink;
	p->p_back->p_forw = p;
	sched_qs[bit].ph_rlink = p;
}

/*
 * remrunqueue(p)
 *
 * Call should be made at splclock().
 */
void
remrunqueue(p)
	struct proc *p;
{
	int bit;

	bit = p->p_priority >> 2;
	if ((sched_whichqs & (1 << bit)) == 0)
		panic("remrunqueue");

	p->p_back->p_forw = p->p_forw;
	p->p_forw->p_back = p->p_back;
	p->p_back = NULL;	/* for firewall checking. */

	if ((struct proc *)&sched_qs[bit] == sched_qs[bit].ph_link)
		sched_whichqs &= ~(1 << bit);
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  Unfortunately, we can't read the hardware registers.
 * We guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splclock();
	static struct timeval lasttime;

	*tvp = time;
#ifdef notdef
	tvp->tv_usec += clkread();
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
#endif
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

/*
 * Wait "n" microseconds.
 */
void
delay(n)
	unsigned long n;
{
	long N = cycles_per_usec * (n);

	/*
	 * XXX Should be written to use RPCC?
	 */

	__asm __volatile(
		"# The 2 corresponds to the insn count\n"
		"1:	subq	%2, %1, %0	\n"
		"	bgt	%0, 1b"
		: "=r" (N)
		: "i" (2), "0" (N));
}

#if defined(COMPAT_OSF1) || 1		/* XXX */
void	cpu_exec_ecoff_setregs(struct proc *, struct exec_package *,
	    u_long, register_t *);

void
cpu_exec_ecoff_setregs(p, epp, stack, retval)
	struct proc *p;
	struct exec_package *epp;
	u_long stack;
	register_t *retval;
{
	struct ecoff_exechdr *execp = (struct ecoff_exechdr *)epp->ep_hdr;

	setregs(p, epp, stack, retval);
	p->p_md.md_tf->tf_regs[FRAME_GP] = execp->a.gp_value;
}

/*
 * cpu_exec_ecoff_hook():
 *	cpu-dependent ECOFF format hook for execve().
 * 
 * Do any machine-dependent diddling of the exec package when doing ECOFF.
 *
 */
int
cpu_exec_ecoff_hook(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct ecoff_exechdr *execp = (struct ecoff_exechdr *)epp->ep_hdr;
	extern struct emul emul_native;
	int error;
	extern int osf1_exec_ecoff_hook(struct proc *, struct exec_package *);

	switch (execp->f.f_magic) {
#ifdef COMPAT_OSF1
	case ECOFF_MAGIC_ALPHA:
		error = osf1_exec_ecoff_hook(p, epp);
		break;
#endif

	case ECOFF_MAGIC_NATIVE_ALPHA:
		epp->ep_emul = &emul_native;
		error = 0;
		break;

	default:
		error = ENOEXEC;
	}
	return (error);
}
#endif

int
alpha_pa_access(pa)
	u_long pa;
{
	int i;

	for (i = 0; i < mem_cluster_cnt; i++) {
		if (pa < mem_clusters[i].start)
			continue;
		if ((pa - mem_clusters[i].start) >=
		    (mem_clusters[i].size & ~PAGE_MASK))
			continue;
		return (mem_clusters[i].size & PAGE_MASK);	/* prot */
	}

	/*
	 * Address is not a memory address.  If we're secure, disallow
	 * access.  Otherwise, grant read/write.
	 */
	if (securelevel > 0)
		return (VM_PROT_NONE);
	else
		return (VM_PROT_READ | VM_PROT_WRITE);
}

/* XXX XXX BEGIN XXX XXX */
paddr_t alpha_XXX_dmamap_or;					/* XXX */
								/* XXX */
paddr_t								/* XXX */
alpha_XXX_dmamap(v)						/* XXX */
	vaddr_t v;						/* XXX */
{								/* XXX */
								/* XXX */
	return (vtophys(v) | alpha_XXX_dmamap_or);		/* XXX */
}								/* XXX */
/* XXX XXX END XXX XXX */

char *
dot_conv(x)
	unsigned long x;
{
	int i;
	char *xc;
	static int next;
	static char space[2][20];

	xc = space[next ^= 1] + sizeof space[0];
	*--xc = '\0';
	for (i = 0;; ++i) {
		if (i && (i & 3) == 0)
			*--xc = '.';
		*--xc = "0123456789abcdef"[x & 0xf];
		x >>= 4;
		if (x == 0)
			break;
	}
	return xc;
}
