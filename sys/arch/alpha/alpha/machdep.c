/*	$OpenBSD: machdep.c,v 1.32 2000/03/23 09:59:52 art Exp $	*/
/*	$NetBSD: machdep.c,v 1.61 1996/12/07 01:54:49 cgd Exp $	*/

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
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#ifdef REAL_CLISTS
#include <sys/clist.h>
#endif
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>
#include <sys/sysctl.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <machine/kcore.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <vm/pmap.h>
#include <vm/vm_kern.h>

#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/cpuconf.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#include <net/netisr.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
# ifndef INET
#  include <netinet/in.h>
# endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include "ppp.h"
#include "bridge.h"

#include "le_ioasic.h"			/* for le_iomem creation */

vm_map_t buffer_map;

int	cpu_dump __P((void));
int	cpu_dumpsize __P((void));
void	do_sir __P((void));
void	dumpsys __P((void));
void	identifycpu __P((void));
void	netintr __P((void));
void	regdump __P((struct trapframe *framep));
void	printregs __P((struct reg *));

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int	maxmem;			/* max memory per process */

int	totalphysmem;		/* total amount of physical memory in system */
int	physmem;		/* physical mem used by OpenBSD + some rsvd */
int	firstusablepage;	/* first usable memory page */
int	lastusablepage;		/* last usable memory page */
int	resvmem;		/* amount of memory reserved for PROM */
int	unusedmem;		/* amount of memory for OS that we don't use */
int	unknownmem;		/* amount of memory with an unknown use */

int	cputype;		/* system type, from the RPB */

/*
 * XXX We need an address to which we can assign things so that they
 * won't be optimized away because we didn't use the value.
 */
u_int32_t no_optimize;

/* the following is used externally (sysctl_hw) */
char	machine[] = "alpha";
char	cpu_model[128];
const struct cpusw *cpu_fn_switch;		/* function switch */

struct	user *proc0paddr;

/* Number of machine cycles per microsecond */
u_int64_t	cycles_per_usec;

/* some memory areas for device DMA.  "ick." */
caddr_t		le_iomem;		/* XXX iomem for LANCE DMA */

/* number of cpus in the box.  really! */
int		ncpus;

char boot_flags[64];
char booted_kernel[64];

/* for cpu_sysctl() */
char	root_device[17];
int	alpha_unaligned_print = 1;	/* warn about unaligned accesses */
int	alpha_unaligned_fix = 1;	/* fix up unaligned accesses */
int	alpha_unaligned_sigbus = 0;	/* don't SIGBUS on fixed-up accesses */

void
alpha_init(pfn, ptb, symend)
	u_long pfn;		/* first free PFN number */
	u_long ptb;		/* PFN of current level 1 page table */
	char *symend;		/* end of the symbol table */
{
	extern char _end[];
	extern char *esym;
	caddr_t start, v;
	struct mddt *mddtp;
	int i, mddtweird;
	char *p;

	/* Save the symbol table end */
	esym = symend;

	/*
	 * Turn off interrupts and floating point.
	 * Make sure the instruction and data streams are consistent.
	 */
	(void)splhigh();
	alpha_pal_wrfen(0);
	ALPHA_TBIA();
	alpha_pal_imb();

	/*
	 * get address of the restart block, while the bootstrap
	 * mapping is still around.
	 */
	hwrpb = (struct rpb *)ALPHA_PHYS_TO_K0SEG(
	    (vm_offset_t)(*(struct rpb **)HWRPB_ADDR));

	/*
	 * Remember how many cycles there are per microsecond, 
	 * so that we can use delay().  Round up, for safety.
	 */
	cycles_per_usec = (hwrpb->rpb_cc_freq + 999999) / 1000000;

	/*
	 * Init the PROM interface, so we can use printf
	 * until PROM mappings go away in consinit.
	 */
	init_prom_interface();

	/*
	 * Point interrupt/exception vectors to our own.
	 */
	alpha_pal_wrent(XentInt, ALPHA_KENTRY_INT);
	alpha_pal_wrent(XentArith, ALPHA_KENTRY_ARITH);
	alpha_pal_wrent(XentMM, ALPHA_KENTRY_MM);
	alpha_pal_wrent(XentIF, ALPHA_KENTRY_IF);
	alpha_pal_wrent(XentUna, ALPHA_KENTRY_UNA);
	alpha_pal_wrent(XentSys, ALPHA_KENTRY_SYS);

	/*
	 * Disable System and Processor Correctable Error reporting.
	 * Clear pending machine checks and error reports, etc.
	 */
	alpha_pal_wrmces(alpha_pal_rdmces() | ALPHA_MCES_DSC | ALPHA_MCES_DPC);

	/*
	 * Find out how much memory is available, by looking at
	 * the memory cluster descriptors.  This also tries to do
	 * its best to detect things things that have never been seen
	 * before...
	 *
	 * XXX Assumes that the first "system" cluster is the
	 * only one we can use. Is the second (etc.) system cluster
	 * (if one happens to exist) guaranteed to be contiguous?  or...?
	 */
	mddtp = (struct mddt *)(((caddr_t)hwrpb) + hwrpb->rpb_memdat_off);

	/*
	 * BEGIN MDDT WEIRDNESS CHECKING
	 */
	mddtweird = 0;

#define cnt	 mddtp->mddt_cluster_cnt
#define	usage(n) mddtp->mddt_clusters[(n)].mddt_usage
	if (cnt != 2 && cnt != 3) {
		printf("WARNING: weird number (%ld) of mem clusters\n", cnt);
		mddtweird = 1;
	} else if (usage(0) != MDDT_PALCODE ||
		   usage(1) != MDDT_SYSTEM ||
	           (cnt == 3 && usage(2) != MDDT_PALCODE)) {
		mddtweird = 1;
		printf("WARNING: %ld mem clusters, but weird config\n", cnt);
	}

	for (i = 0; i < cnt; i++) {
		if ((usage(i) & MDDT_mbz) != 0) {
			printf("WARNING: mem cluster %d has weird usage %lx\n",
			    i, usage(i));
			mddtweird = 1;
		}
		if (mddtp->mddt_clusters[i].mddt_pg_cnt == 0) {
			printf("WARNING: mem cluster %d has pg cnt == 0\n", i);
			mddtweird = 1;
		}
		/* XXX other things to check? */
	}
#undef cnt
#undef usage

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
	/*
	 * END MDDT WEIRDNESS CHECKING
	 */

	for (i = 0; i < mddtp->mddt_cluster_cnt; i++) {
		totalphysmem += mddtp->mddt_clusters[i].mddt_pg_cnt;
#define	usage(n) mddtp->mddt_clusters[(n)].mddt_usage
#define	pgcnt(n) mddtp->mddt_clusters[(n)].mddt_pg_cnt
		if ((usage(i) & MDDT_mbz) != 0)
			unknownmem += pgcnt(i);
		else if ((usage(i) & ~MDDT_mbz) == MDDT_PALCODE)
			resvmem += pgcnt(i);
		else if ((usage(i) & ~MDDT_mbz) == MDDT_SYSTEM) {
			/*
			 * assumes that the system cluster listed is
			 * one we're in...
			 */
			if (physmem != resvmem) {
				physmem += pgcnt(i);
				firstusablepage =
				    mddtp->mddt_clusters[i].mddt_pfn;
				lastusablepage =
				    firstusablepage + pgcnt(i) - 1;
			} else
				unusedmem += pgcnt(i);
		}
#undef usage
#undef pgcnt
	}
	if (totalphysmem == 0)
		panic("can't happen: system seems to have no memory!");
	maxmem = physmem;

#if 0
	printf("totalphysmem = %d\n", totalphysmem);
	printf("physmem = %d\n", physmem);
	printf("firstusablepage = %d\n", firstusablepage);
	printf("lastusablepage = %d\n", lastusablepage);
	printf("resvmem = %d\n", resvmem);
	printf("unusedmem = %d\n", unusedmem);
	printf("unknownmem = %d\n", unknownmem);
#endif

	/*
	 * find out this CPU's page size
	 */
	PAGE_SIZE = hwrpb->rpb_page_size;
	if (PAGE_SIZE != 8192)
		panic("page size %d != 8192?!", PAGE_SIZE);

	/*
	 * Init PAGE_SIZE dependent variables in the MI VM system
	 */
	vm_set_page_size();

	v = (caddr_t)alpha_round_page(symend ? symend : _end);
	/*
	 * Init mapping for u page(s) for proc 0
	 */
	start = v;
	curproc->p_addr = proc0paddr = (struct user *)v;
	v += UPAGES * NBPG;

	/*
	 * Find out what hardware we're on, and remember its type name.
	 */
	cputype = hwrpb->rpb_type;
	if (cputype < 0 || cputype > ncpusw) {
unknown_cputype:
		printf("\n");
		printf("Unknown system type %d.\n", cputype);
		printf("\n");
		panic("unknown system type");
	}
	cpu_fn_switch = &cpusw[cputype];
	if (cpu_fn_switch->family == NULL)
		goto unknown_cputype;
	if (cpu_fn_switch->option == NULL) {
		printf("\n");
		printf("OpenBSD does not currently support system type %d\n",
		    cputype);
		printf("(%s family).\n", cpu_fn_switch->family);
		printf("\n");
		panic("unsupported system type");
	}
	if (!cpu_fn_switch->present) {
		printf("\n");
		printf("Support for system type %d (%s family) is\n", cputype,
		    cpu_fn_switch->family);
		printf("not present in this kernel.  Build a kernel with "
		    "\"options %s\"\n", cpu_fn_switch->option);
		printf("to include support for this system type.\n");
		printf("\n");
		panic("support for system not present");
	}

	if ((*cpu_fn_switch->model_name)() != NULL)
		strncpy(cpu_model, (*cpu_fn_switch->model_name)(),
		    sizeof cpu_model - 1);
	else {
		strncpy(cpu_model, cpu_fn_switch->family,
		    sizeof cpu_model - 1);
		strcat(cpu_model, " family");		/* XXX */
	}
	cpu_model[sizeof cpu_model - 1] = '\0';

#if NLE_IOASIC > 0
	/*
	 * Grab 128K at the top of physical memory for the lance chip
	 * on machines where it does dma through the I/O ASIC.
	 * It must be physically contiguous and aligned on a 128K boundary.
	 *
	 * Note that since this is conditional on the presence of
	 * IOASIC-attached 'le' units in the kernel config, the
	 * message buffer may move on these systems.  This shouldn't
	 * be a problem, because once people have a kernel config that
	 * they use, they're going to stick with it.
	 */
	if (cputype == ST_DEC_3000_500 ||
	    cputype == ST_DEC_3000_300) {	/* XXX possibly others? */
		lastusablepage -= btoc(128 * 1024);
		le_iomem =
		    (caddr_t)ALPHA_PHYS_TO_K0SEG(ctob(lastusablepage + 1));
	}
#endif /* NLE_IOASIC */

	/*
	 * Initialize error message buffer (at end of core).
	 */
	lastusablepage -= btoc(MSGBUFSIZE);
	printf("%lx %d\n", (caddr_t)ALPHA_PHYS_TO_K0SEG(ctob(lastusablepage + 1)),
	    MSGBUFSIZE);
	initmsgbuf((caddr_t)ALPHA_PHYS_TO_K0SEG(ctob(lastusablepage + 1)),
	    MSGBUFSIZE);
	printf("%lx %d\n", (caddr_t)ALPHA_PHYS_TO_K0SEG(ctob(lastusablepage + 1)),
	    MSGBUFSIZE);

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
#define valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)ALIGN((lim) = ((name)+(num)))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(timeouts, struct timeout, ntimeout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate.
	 * We allocate 10% of memory for buffer space.  Insure a
	 * minimum of 16 buffers.  We allocate 1/2 as many swap buffer
	 * headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = (physmem * 10) / (CLSIZE * 100);
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

	/*
	 * Clear allocated memory.
	 */
	bzero(start, v - start);

	/*
	 * Initialize the virtual memory system, and set the
	 * page table base register in proc 0's PCB.
	 */
#ifndef NEW_PMAP
	pmap_bootstrap((vm_offset_t)v, ALPHA_PHYS_TO_K0SEG(ptb << PGSHIFT));
#else
	pmap_bootstrap((vm_offset_t)v, ALPHA_PHYS_TO_K0SEG(ptb << PGSHIFT),
	    hwrpb->rpb_max_asn);
#endif

	/*
	 * Initialize the rest of proc 0's PCB, and cache its physical
	 * address.
	 */
	proc0.p_md.md_pcbpaddr =
	    (struct pcb *)ALPHA_K0SEG_TO_PHYS((vm_offset_t)&proc0paddr->u_pcb);

	/*
	 * Set the kernel sp, reserving space for an (empty) trapframe,
	 * and make proc0's trapframe pointer point to it for sanity.
	 */
	proc0paddr->u_pcb.pcb_hw.apcb_ksp =
	    (u_int64_t)proc0paddr + USPACE - sizeof(struct trapframe);
	proc0.p_md.md_tf =
	    (struct trapframe *)proc0paddr->u_pcb.pcb_hw.apcb_ksp;

#ifdef NEW_PMAP
	pmap_activate(kernel_pmap, &proc0paddr->u_pcb.pcb_hw, 0);
#endif

	/*
	 * Look at arguments passed to us and compute boothowto.
	 * Also, get kernel name so it can be used in user-land.
	 */
	prom_getenv(PROM_E_BOOTED_OSFLAGS, boot_flags, sizeof(boot_flags));
#if 0
	printf("boot flags = \"%s\"\n", boot_flags);
#endif
	prom_getenv(PROM_E_BOOTED_FILE, booted_kernel,
	    sizeof(booted_kernel));
#if 0
	printf("booted kernel = \"%s\"\n", booted_kernel);
#endif

	boothowto = RB_SINGLE;
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	for (p = boot_flags; p && *p != '\0'; p++) {
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
		}
	}

	/*
	 * Figure out the number of cpus in the box, from RPB fields.
	 * Really.  We mean it.
	 */
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		struct pcs *pcsp;

		pcsp = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off +
		    (i * hwrpb->rpb_pcs_size));
		if ((pcsp->pcs_flags & PCS_PP) != 0)
			ncpus++;
	}
}

void
consinit()
{
	(*cpu_fn_switch->cons_init)();
	pmap_unmap_prom();
#ifdef DDB
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

void
cpu_startup()
{
	register unsigned i;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
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
	printf("real mem = %d (%d reserved for PROM, %d used by OpenBSD)\n",
	    ctob(totalphysmem), ctob(resvmem), ctob(physmem));
	if (unusedmem)
		printf("WARNING: unused memory = %d bytes\n", ctob(unusedmem));
	if (unknownmem)
		printf("WARNING: %d bytes of memory with unknown purpose\n",
		    ctob(unknownmem));

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
	    &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16 * NCARGS, TRUE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
	    M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
	    VM_MBUF_SIZE, FALSE);
	/*
	 * Initialize timeouts
	 */
	timeout_init();

#if defined(DEBUG)
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %ld\n", (long)ptoa(cnt.v_free_count));
	printf("using %ld buffers containing %ld bytes of memory\n",
		(long)nbuf, (long)(bufpages * CLBYTES));

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
	configure();

	/*
	 * Note that bootstrapping is finished, and set the HWRPB up
	 * to do restarts.
	 */
	hwrpb_restart_setup();
}

void
identifycpu()
{

	/*
	 * print out CPU identification information.
	 */
	printf("%s, %ldMHz\n", cpu_model,
	    hwrpb->rpb_cc_freq / 1000000);	/* XXX true for 21164? */
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
boot(howto /* , bootstr */)
	int howto;
	/* char *bootstr; */
{
	extern int cold;

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
		extern struct proc proc0;

		/* protect against curproc->p_stats.foo refs in sync   XXX */
		if (curproc == NULL)
			curproc = &proc0;

		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested do it. */
#if 0
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
#else
	if (howto & RB_DUMP)
#endif
		dumpsys();

haltsys:

	/* run any shutdown hooks */
	doshutdownhooks();

#ifdef BOOTKEY
	printf("hit any key to %s...\n", howto & RB_HALT ? "halt" : "reboot");
	cngetc();
	printf("\n");
#endif

	/* Finally, halt/reboot the system. */
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

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return -1;

	return (1);
}

/*
 * cpu_dump: dump machine-dependent kernel core dump headers.
 */
int
cpu_dump()
{
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	long buf[dbtob(1) / sizeof (long)];
	kcore_seg_t	*segp;
	cpu_kcore_hdr_t	*cpuhdrp;

        dump = bdevsw[major(dumpdev)].d_dump;

	segp = (kcore_seg_t *)buf;
	cpuhdrp =
	    (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(*segp)) / sizeof (long)];

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	/*
	 * Add the machine-dependent header info
	 */
	cpuhdrp->lev1map_pa = ALPHA_K0SEG_TO_PHYS((vm_offset_t)Lev1map);
	cpuhdrp->page_size = PAGE_SIZE;
	cpuhdrp->core_seg.start = ctob(firstusablepage);
	cpuhdrp->core_seg.size = ctob(physmem);

	return (dump(dumpdev, dumplo, (caddr_t)buf, dbtob(1)));
}

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
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
	dumpblks += ctod(physmem);

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1)))
		goto bad;

	/* Put dump at end of partition */
	dumplo = nblks - dumpblks;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = physmem;
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
	unsigned bytes, i, n;
	int maddr, psize;
	daddr_t blkno;
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
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
		printf("\ndump to dev %x not possible\n", dumpdev);
		return;
	}
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	/* XXX should purge all outstanding keystrokes. */

	if ((error = cpu_dump()) != 0)
		goto err;

	bytes = ctob(physmem);
	maddr = ctob(firstusablepage);
	blkno = dumplo + cpu_dumpsize();
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;
	for (i = 0; i < bytes; i += n) {

		/* Print out how many MBs we to go. */
		n = bytes - i;
		if (n && (n % (1024*1024)) == 0)
			printf("%d ", n / (1024 * 1024));

		/* Limit size for next transfer. */
		if (n > BYTES_PER_DUMP)
			n =  BYTES_PER_DUMP;

		error = (*dump)(dumpdev, blkno,
		    (caddr_t)ALPHA_PHYS_TO_K0SEG(maddr), n);
		if (error)
			break;
		maddr += n;
		blkno += btodb(n);			/* XXX? */

		/* XXX should look for keystrokes, to cancel. */
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
	extern char sigcode[], esigcode[];
	extern struct proc *fpcurproc;
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
	 * call to grow() is a nop, and the useracc() check
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
		(void)grow(p, (u_long)scp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %p usp %p\n", p->p_pid,
		    sig, &oonstack, scp);
#endif
	if (useracc((caddr_t)scp, fsize, B_WRITE) == 0) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig(%d): useracc failed on sig %d\n",
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
	if (p == fpcurproc) {
		alpha_pal_wrfen(1);
		savefpstate(&p->p_addr->u_pcb.pcb_fp);
		alpha_pal_wrfen(0);
		fpcurproc = NULL;
	}
	ksc.sc_ownedfp = p->p_md.md_flags & MDP_FPUSED;
	bcopy(&p->p_addr->u_pcb.pcb_fp, (struct fpreg *)ksc.sc_fpregs,
	    sizeof(struct fpreg));
	ksc.sc_fp_control = 0;					/* XXX ? */
	bzero(ksc.sc_reserved, sizeof ksc.sc_reserved);		/* XXX */
	bzero(ksc.sc_xxx, sizeof ksc.sc_xxx);			/* XXX */


#ifdef COMPAT_OSF1
	/*
	 * XXX Create an OSF/1-style sigcontext and associated goo.
	 */
#endif

	if (psp->ps_siginfo & sigmask(sig)) {
		initsiginfo(&ksi, sig, code, type, val);
		sip = (void *)scp + kscsize;
		(void) copyout((caddr_t)&ksi, (caddr_t)sip, fsize - kscsize);
	}

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
	frame->tf_regs[FRAME_PC] =
	    (u_int64_t)PS_STRINGS - (esigcode - sigcode);
	frame->tf_regs[FRAME_A0] = sig;
	frame->tf_regs[FRAME_A1] = (psp->ps_siginfo & sigmask(sig)) ?
	    (u_int64_t)sip : NULL;
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
 * psl to gain improper priviledges or to cause
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
	struct sigcontext *scp, ksc;
	extern struct proc *fpcurproc;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
	    printf("sigreturn: pid %d, scp %p\n", p->p_pid, scp);
#endif

	if (ALIGN(scp) != (u_int64_t)scp)
		return (EINVAL);

	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if (useracc((caddr_t)scp, sizeof (*scp), B_WRITE) == 0 ||
	    copyin((caddr_t)scp, (caddr_t)&ksc, sizeof ksc))
		return (EINVAL);

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
	if (p == fpcurproc)
		fpcurproc = NULL;
	bcopy((struct fpreg *)ksc.sc_fpregs, &p->p_addr->u_pcb.pcb_fp,
	    sizeof(struct fpreg));
	/* XXX ksc.sc_fp_control ? */

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

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
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
		return (sysctl_rdstring(oldp, oldlenp, newp, root_device));

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
		return (sysctl_rdstring(oldp, oldlenp, newp, booted_kernel));

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
	extern struct proc *fpcurproc;
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
#define FP_RN 2 /* XXX */
	p->p_addr->u_pcb.pcb_fp.fpr_cr = (long)FP_RN << 58;
	alpha_pal_wrusp(stack);
	tfp->tf_regs[FRAME_PS] = ALPHA_PSL_USERSET;
	tfp->tf_regs[FRAME_PC] = pack->ep_entry & ~3;

	tfp->tf_regs[FRAME_A0] = stack;
	/* a1 and a2 already zeroed */
	tfp->tf_regs[FRAME_T12] = tfp->tf_regs[FRAME_PC];	/* a.k.a. PV */

	p->p_md.md_flags &= ~MDP_FPUSED;
	if (fpcurproc == p)
		fpcurproc = NULL;

	retval[0] = retval[1] = 0;
}

void
netintr()
{
	int n, s;

	s = splhigh();
	n = netisr;
	netisr = 0;
	splx(s);

#define	DONETISR(bit, fn)						\
	do {								\
		if (n & (1 << (bit)))					\
			fn;						\
	} while (0)

#ifdef INET
	DONETISR(NETISR_ARP, arpintr());
	DONETISR(NETISR_IP, ipintr());
#endif
#ifdef INET6
	DONETISR(NETISR_IPV6, ip6intr());
#endif
#ifdef NETATALK
	DONETISR(NETISR_ATALK, atintr());
#endif
#ifdef NS
	DONETISR(NETISR_NS, nsintr());
#endif
#ifdef ISO
	DONETISR(NETISR_ISO, clnlintr());
#endif
#ifdef CCITT
	DONETISR(NETISR_CCITT, ccittintr());
#endif
#ifdef NATM
	DONETISR(NETISR_NATM, natmintr());
#endif
#if NPPP > 0
	DONETISR(NETISR_PPP, pppintr());
#endif
#if NBRIDGE > 0
	DONETISR(NETISR_BRIDGE, bridgeintr());
#endif

#undef DONETISR
}

void
do_sir()
{
	u_int64_t n;

	do {
		(void)splhigh();
		n = ssir;
		ssir = 0;
		splsoft();		/* don't recurse through spl0() */
	
#define	DO_SIR(bit, fn)							\
		do {							\
			if (n & (bit)) {				\
				cnt.v_soft++;				\
				fn;					\
			}						\
		} while (0)

		DO_SIR(SIR_NET, netintr());
		DO_SIR(SIR_CLOCK, softclock());

#undef DO_SIR
	} while (ssir != 0);
}

int
spl0()
{

	if (ssir)
		do_sir();		/* it lowers the IPL itself */

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

void
setrunqueue(p)
	struct proc *p;
{
	int bit;

	/* firewall: p->p_back must be NULL */
	if (p->p_back != NULL)
		panic("setrunqueue");

	bit = p->p_priority >> 2;
	whichqs |= (1 << bit);
	p->p_forw = (struct proc *)&qs[bit];
	p->p_back = qs[bit].ph_rlink;
	p->p_back->p_forw = p;
	qs[bit].ph_rlink = p;
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
	if ((whichqs & (1 << bit)) == 0)
		panic("remrunqueue");

	p->p_back->p_forw = p->p_forw;
	p->p_forw->p_back = p->p_back;
	p->p_back = NULL;	/* for firewall checking. */

	if ((struct proc *)&qs[bit] == qs[bit].ph_link)
		whichqs &= ~(1 << bit);
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
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
#endif
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
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

	while (N > 0)				/* XXX */
		N -= 3;				/* XXX */
}

#if defined(COMPAT_OSF1) || 1		/* XXX */
void	cpu_exec_ecoff_setregs __P((struct proc *, struct exec_package *,
	    u_long, register_t *));

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
#ifdef COMPAT_OSF1
	extern struct emul emul_osf1;
#endif

	switch (execp->f.f_magic) {
#ifdef COMPAT_OSF1
	case ECOFF_MAGIC_ALPHA:
		epp->ep_emul = &emul_osf1;
		break;
#endif

	case ECOFF_MAGIC_NATIVE_ALPHA:
		epp->ep_emul = &emul_native;
		break;

	default:
		return ENOEXEC;
	}
	return 0;
}
#endif

/* XXX XXX BEGIN XXX XXX */
vm_offset_t alpha_XXX_dmamap_or;				/* XXX */
								/* XXX */
vm_offset_t							/* XXX */
alpha_XXX_dmamap(v)						/* XXX */
	vm_offset_t v;						/* XXX */
{								/* XXX */
								/* XXX */
	return (vtophys(v) | alpha_XXX_dmamap_or);		/* XXX */
}								/* XXX */
/* XXX XXX END XXX XXX */
