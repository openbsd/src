/*	$NetBSD: machdep.c,v 1.14 1996/01/04 22:21:33 jtc Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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
#include <sys/conf.h>
#include <sys/file.h>
#ifdef REAL_CLISTS
#include <sys/clist.h>
#endif
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>
#include <sys/sysctl.h>
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

#include <vm/vm_kern.h>

#include <dev/cons.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/rpb.h>
#include <machine/prom.h>

#ifdef DEC_3000_500
#include <alpha/alpha/dec_3000_500.h>
#endif
#ifdef DEC_3000_300
#include <alpha/alpha/dec_3000_300.h>
#endif
#ifdef DEC_2100_A50
#include <alpha/alpha/dec_2100_a50.h>
#endif
#ifdef DEC_KN20AA
#include <alpha/alpha/dec_kn20aa.h>
#endif
#ifdef DEC_AXPPCI_33
#include <alpha/alpha/dec_axppci_33.h>
#endif
#ifdef DEC_21000
#include <alpha/alpha/dec_21000.h>
#endif

#include <net/netisr.h>
#include "ether.h"

#include "le.h"			/* XXX for le_iomem creation */

vm_map_t buffer_map;

void dumpsys __P((void));

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
int	msgbufmapped = 0;	/* set when safe to use msgbuf */
int	maxmem;			/* max memory per process */

int	totalphysmem;		/* total amount of physical memory in system */
int	physmem;		/* physical memory used by NetBSD + some rsvd */
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
char	*cpu_model;
char	*model_names[] = {
	"UNKNOWN (0)",
	"Alpha Demonstration Unit",
	"DEC 4000 (\"Cobra\")",
	"DEC 7000 (\"Ruby\")",
	"DEC 3000/500 (\"Flamingo\") family",
	"UNKNOWN (5)",
	"DEC 2000/300 (\"Jensen\")",
	"DEC 3000/300 (\"Pelican\")",
	"UNKNOWN (8)",
	"DEC 2100/A500 (\"Sable\")",
	"AXPvme 64",
	"AXPpci 33 (\"NoName\")",
	"DEC 21000 (\"TurboLaser\")",
	"DEC 2100/A50 (\"Avanti\") family",
	"Mustang",
	"DEC KN20AA",
	"UNKNOWN (16)",
	"DEC 1000 (\"Mikasa\")",
};
int	nmodel_names = sizeof model_names/sizeof model_names[0];

struct	user *proc0paddr;

/* Number of machine cycles per microsecond */
u_int64_t	cycles_per_usec;

/* some memory areas for device DMA.  "ick." */
caddr_t		le_iomem;		/* XXX iomem for LANCE DMA */

/* Interrupt vectors (in locore) */
extern int XentInt(), XentArith(), XentMM(), XentIF(), XentUna(), XentSys();

/* number of cpus in the box.  really! */
int		ncpus;

/* various CPU-specific functions. */
char		*(*cpu_modelname) __P((void));
void		(*cpu_consinit) __P((char *));
dev_t		(*cpu_bootdev) __P((char *));
char		*cpu_iobus;

char *boot_file, *boot_flags, *boot_console, *boot_dev;

int
alpha_init(pfn, ptb, argc, argv, envp)
	u_long pfn;		/* first free PFN number */
	u_long ptb;		/* PFN of current level 1 page table */
	u_long argc;
	char *argv[], *envp[];
{
	extern char _end[];
	caddr_t start, v;
	struct mddt *mddtp;
	int i, mddtweird;
	char *p;

	/*
	 * Turn off interrupts and floating point.
	 * Make sure the instruction and data streams are consistent.
	 */
	(void)splhigh();
	pal_wrfen(0);
	TBIA();
	IMB();

	/*
	 * get address of the restart block, while we the bootstrap
	 * mapping is still around.
	 */
	hwrpb = (struct rpb *) phystok0seg(*(struct rpb **)HWRPB_ADDR);

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
	pal_wrent(XentInt, 0);
	pal_wrent(XentArith, 1);
	pal_wrent(XentMM, 2);
	pal_wrent(XentIF, 3);
	pal_wrent(XentUna, 4);
	pal_wrent(XentSys, 5);

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
		printf("WARNING: weird number (%d) of mem clusters\n", cnt);
		mddtweird = 1;
	} else if (usage(0) != MDDT_PALCODE ||
		   usage(1) != MDDT_SYSTEM ||
	           (cnt == 3 && usage(2) != MDDT_PALCODE)) {
		mddtweird = 1;
		printf("WARNING: %d mem clusters, but weird config\n", cnt);
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
				lastusablepage = firstusablepage + pgcnt(i) - 1;
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

	v = (caddr_t)alpha_round_page(_end);
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
	switch (cputype) {
#ifdef DEC_3000_500				/* and 400, [6-9]00 */
	case ST_DEC_3000_500:
		cpu_modelname = dec_3000_500_modelname;
		cpu_consinit = dec_3000_500_consinit;
		cpu_bootdev = dec_3000_500_bootdev;
		cpu_iobus = "tcasic";
		break;
#endif

#ifdef DEC_3000_300
	case ST_DEC_3000_300:
		cpu_modelname = dec_3000_300_modelname;
		cpu_consinit = dec_3000_300_consinit;
		cpu_bootdev = dec_3000_300_bootdev;
		cpu_iobus = "tcasic";
		break;
#endif

#ifdef DEC_2100_A50
	case ST_DEC_2100_A50:
		cpu_modelname = dec_2100_a50_modelname;
		cpu_consinit = dec_2100_a50_consinit;
		cpu_bootdev = dec_2100_a50_bootdev;
		cpu_iobus = "apecs";
		break;
#endif

#ifdef DEC_KN20AA
	case ST_DEC_KN20AA:
		cpu_modelname = dec_kn20aa_modelname;
		cpu_consinit = dec_kn20aa_consinit;
		cpu_bootdev = dec_kn20aa_bootdev;
		cpu_iobus = "cia";
		break;
#endif

#ifdef DEC_AXPPCI_33
	case ST_DEC_AXPPCI_33:
		cpu_modelname = dec_axppci_33_modelname;
		cpu_consinit = dec_axppci_33_consinit;
		cpu_bootdev = dec_axppci_33_bootdev;
		cpu_iobus = "lca";
		break;
#endif

#ifdef DEC_2000_300
	case ST_DEC_2000_300:
		cpu_modelname = dec_2000_300_modelname;
		cpu_consinit = dec_2000_300_consinit;
		cpu_bootdev = dec_2000_300_bootdev;
		cpu_iobus = "ibus";
	XXX DEC 2000/300 NOT SUPPORTED
		break;
#endif

#ifdef DEC_21000
	case ST_DEC_21000:
		cpu_modelname = dec_21000_modelname;
		cpu_consinit = dec_21000_consinit;
		cpu_bootdev = dec_21000_bootdev;
		cpu_iobus = "tlsb";
		break;
#endif

	default:
		if (cputype > nmodel_names)
			panic("Unknown system type %d", cputype);
		else
			panic("Support for %s system type not in kernel.",
			    model_names[cputype]);
	}

	cpu_model = (*cpu_modelname)();
	if (cpu_model == NULL)
		cpu_model = model_names[cputype];

#if NLE > 0
	/*
	 * Grab 128K at the top of physical memory for the lance chip
	 * on machines where it does dma through the I/O ASIC.
	 * It must be physically contiguous and aligned on a 128K boundary.
	 */
	if (cputype == ST_DEC_3000_500 ||
	    cputype == ST_DEC_3000_300) {	/* XXX possibly others? */
		lastusablepage -= btoc(128 * 1024);
		le_iomem = (caddr_t)phystok0seg(ctob(lastusablepage + 1));
	}
#endif /* NLE */

	/*
	 * Initialize error message buffer (at end of core).
	 */
	lastusablepage -= btoc(sizeof (struct msgbuf));
	msgbufp = (struct msgbuf *)phystok0seg(ctob(lastusablepage + 1));
	msgbufmapped = 1;

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
	valloc(callout, struct callout, ncallout);
	valloc(swapmap, struct map, nswapmap = maxproc * 2);
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
	 * We allocate the BSD standard of 10% of memory for the first
	 * 2 Meg, and 5% of remaining memory for buffer space.  Insure a
	 * minimum of 16 buffers.  We allocate 1/2 as many swap buffer
	 * headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = (btoc(2 * 1024 * 1024) + physmem) /
		    (20 * CLSIZE);
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
	pmap_bootstrap((vm_offset_t)v, phystok0seg(ptb << PGSHIFT));

	/*
	 * Initialize the rest of proc 0's PCB, and cache its physical
	 * address.
	 */
	proc0.p_md.md_pcbpaddr =
	    (struct pcb *)k0segtophys(&proc0paddr->u_pcb);

	/*
	 * Set the kernel sp, reserving space for an (empty) trapframe,
	 * and make proc0's trapframe pointer point to it for sanity.
	 */
	proc0paddr->u_pcb.pcb_ksp =
	    (u_int64_t)proc0paddr + USPACE - sizeof(struct trapframe);
	proc0.p_md.md_tf = (struct trapframe *)proc0paddr->u_pcb.pcb_ksp;

	/*
	 * figure out what arguments we have
	 */
	switch (argc) {
	default:
		printf("weird number of arguments from boot: %d\n", argc);
		if (argc < 1)
			break;
		/* FALLTHRU */
	case 4:
		boot_dev = argv[3];
		/* FALLTHRU */
	case 3:
		boot_console = argv[2];
		/* FALLTHRU */
	case 2:
		boot_flags = argv[1];
		/* FALLTHRU */
	case 1:
		boot_file = argv[0];
		/* FALLTHRU */
	}

	/*
	 * Look at arguments and compute bootdev.
	 * XXX NOT HERE.
	 */
#if 0
	{							/* XXX */
		extern dev_t bootdev;				/* XXX */
		bootdev = (*cpu_bootdev)(boot_dev);
	}							/* XXX */
#endif

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_SINGLE;
#ifdef GENERIC
	boothowto |= RB_ASKNAME;
#endif
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	for (p = boot_flags; p && *p != '\0'; p++) {
		switch (*p) {
		case 'a': /* autoboot */
		case 'A': /* DEC's notion of autoboot */
			boothowto &= ~RB_SINGLE;
			break;

		case 'd': /* use compiled in default root */
			boothowto |= RB_DFLTROOT;
			break;

		case 'm': /* mini root present in memory */
			boothowto |= RB_MINIROOT;
			break;

		case 'n': /* ask for names */
			boothowto |= RB_ASKNAME;
			break;

		case 'N': /* don't ask for names */
			boothowto &= ~RB_ASKNAME;
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

	return (0);
}

consinit()
{

	(*cpu_consinit)(boot_console);
	pmap_unmap_prom();
}

cpu_startup()
{
	register unsigned i;
	register caddr_t v;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem = %d (%d reserved for PROM, %d used by NetBSD)\n",
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
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
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
	configure();
}

identifycpu()
{

	/*
	 * print out CPU identification information.
	 */
	printf("%s, %dMHz\n", cpu_model,
	    hwrpb->rpb_cc_freq / 1000000);	/* XXX true for 21164? */
	printf("%d byte page size, %d processor%s.\n",
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

boot(howto)
	int howto;
{
	extern int cold;

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

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
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP) {
		savectx(&dumppcb, 0);
		dumpsys();
	}

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
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf()
{
	int nblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	/* XXX XXX XXX STARTING MEMORY LOCATION */
	dumpsize = physmem;

	/* Always skip the first CLBYTES, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	if (dumpsize == 0) {
		dumpconf();
		if (dumpsize == 0)
			return;
	}
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);

	printf("dump ");
	switch ((*bdevsw[major(dumpdev)].d_dump)(dumpdev)) {

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

	default:
		printf("succeeded\n");
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
	regp->r_regs[R_A0] = framep->tf_a0;
	regp->r_regs[R_A1] = framep->tf_a1;
	regp->r_regs[R_A2] = framep->tf_a2;
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
	regp->r_regs[R_GP] = framep->tf_gp;
	regp->r_regs[R_SP] = framep->tf_regs[FRAME_SP];
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
	framep->tf_a0 = regp->r_regs[R_A0];
	framep->tf_a1 = regp->r_regs[R_A1];
	framep->tf_a2 = regp->r_regs[R_A2];
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
	framep->tf_gp = regp->r_regs[R_GP];
	framep->tf_regs[FRAME_SP] = regp->r_regs[R_SP];
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
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	struct proc *p = curproc;
	struct sigcontext *scp, ksc;
	struct trapframe *frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack, fsize, rndfsize;
	extern char sigcode[], esigcode[];
	extern struct proc *fpcurproc;

	frame = p->p_md.md_tf;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	fsize = sizeof ksc;
	rndfsize = ((fsize + 15) / 16) * 16;
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
		scp = (struct sigcontext *)(frame->tf_regs[FRAME_SP] -
		    rndfsize);
	if ((u_long)scp <= USRSTACK - ctob(p->p_vmspace->vm_ssize))
		(void)grow(p, (u_long)scp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %lx usp %lx\n", p->p_pid,
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
	ksc.sc_pc = frame->tf_pc;
	ksc.sc_ps = frame->tf_ps;

	/* copy the registers. */
	frametoreg(frame, (struct reg *)ksc.sc_regs);
	ksc.sc_regs[R_ZERO] = 0xACEDBADE;		/* magic number */

	/* save the floating-point state, if necessary, then copy it. */
	if (p == fpcurproc) {
		pal_wrfen(1);
		savefpstate(&p->p_addr->u_pcb.pcb_fp);
		pal_wrfen(0);
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

	/*
	 * copy the frame out to userland.
	 */
	(void) copyout((caddr_t)&ksc, (caddr_t)scp, fsize);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): sig %d scp %lx code %lx\n", p->p_pid, sig,
		    scp, code);
#endif

	/*
	 * Set up the registers to return to sigcode.
	 */
	frame->tf_pc = (u_int64_t)PS_STRINGS - (esigcode - sigcode);
	frame->tf_regs[FRAME_SP] = (u_int64_t)scp;
	frame->tf_a0 = sig;
	frame->tf_a1 = code;
	frame->tf_a2 = (u_int64_t)scp;
	frame->tf_regs[FRAME_T12] = (u_int64_t)catcher;		/* t12 is pv */

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): pc %lx, catcher %lx\n", p->p_pid,
		    frame->tf_pc, frame->tf_regs[FRAME_A3]);
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
	    printf("sigreturn: pid %d, scp %lx\n", p->p_pid, scp);
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

	p->p_md.md_tf->tf_pc = ksc.sc_pc;
	p->p_md.md_tf->tf_ps = (ksc.sc_ps | PSL_USERSET) & ~PSL_USERCLR;

	regtoframe((struct reg *)ksc.sc_regs, p->p_md.md_tf);

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
	int i;
	extern struct proc *fpcurproc;

#ifdef DEBUG
	for (i = 0; i < FRAME_NSAVEREGS; i++)
		tfp->tf_regs[i] = 0xbabefacedeadbeef;
	tfp->tf_gp = 0xbabefacedeadbeef;
	tfp->tf_a0 = 0xbabefacedeadbeef;
	tfp->tf_a1 = 0xbabefacedeadbeef;
	tfp->tf_a2 = 0xbabefacedeadbeef;
#else
	bzero(tfp->tf_regs, FRAME_NSAVEREGS * sizeof tfp->tf_regs[0]);
	tfp->tf_gp = 0;
	tfp->tf_a0 = 0;
	tfp->tf_a1 = 0;
	tfp->tf_a2 = 0;
#endif
	bzero(&p->p_addr->u_pcb.pcb_fp, sizeof p->p_addr->u_pcb.pcb_fp);
#define FP_RN 2 /* XXX */
	p->p_addr->u_pcb.pcb_fp.fpr_cr = (long)FP_RN << 58;
	tfp->tf_regs[FRAME_SP] = stack;	/* restored to usp in trap return */
	tfp->tf_ps = PSL_USERSET;
	tfp->tf_pc = pack->ep_entry & ~3;

	p->p_md.md_flags & ~MDP_FPUSED;
	if (fpcurproc == p)
		fpcurproc = NULL;

	retval[0] = retval[1] = 0;
}

void
netintr()
{
#ifdef INET
#if NETHER > 0
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
#endif
	if (netisr & (1 << NETISR_IP)) {
		netisr &= ~(1 << NETISR_IP);
		ipintr();
	}
#endif
#ifdef NS
	if (netisr & (1 << NETISR_NS)) {
		netisr &= ~(1 << NETISR_NS);
		nsintr();
	}
#endif
#ifdef ISO
	if (netisr & (1 << NETISR_ISO)) {
		netisr &= ~(1 << NETISR_ISO);
		clnlintr();
	}
#endif
#ifdef CCITT
	if (netisr & (1 << NETISR_CCITT)) {
		netisr &= ~(1 << NETISR_CCITT);
		ccittintr();
	}
#endif
#ifdef PPP
	if (netisr & (1 << NETISR_PPP)) {
		netisr &= ~(1 << NETISR_CCITT);
		pppintr();
	}
#endif
}

void
do_sir()
{

	if (ssir & SIR_NET) {
		siroff(SIR_NET);
		cnt.v_soft++;
		netintr();
	}
	if (ssir & SIR_CLOCK) {
		siroff(SIR_CLOCK);
		cnt.v_soft++;
		softclock();
	}
}

int
spl0()
{

	if (ssir) {
		splsoft();
		do_sir();
	}

	return (pal_swpipl(PSL_IPL_0));
}

/*
 * The following primitives manipulate the run queues.  _whichqs tells which
 * of the 32 queues _qs have processes in them.  Setrunqueue puts processes
 * into queues, Remrq removes them from queues.  The running process is on
 * no queue, other processes are on a queue related to p->p_priority, divided
 * by 4 actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
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
 * Remrq(p)
 *
 * Call should be made at splclock().
 */
void
remrq(p)
	struct proc *p;
{
	int bit;

	bit = p->p_priority >> 2;
	if ((whichqs & (1 << bit)) == 0)
		panic("remrq");

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

#if defined(COMPAT_OSF1) || 1		/* XXX */
void
cpu_exec_ecoff_setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct ecoff_aouthdr *eap;

	setregs(p, pack, stack, retval);

	eap = (struct ecoff_aouthdr *)
	    ((caddr_t)pack->ep_hdr + sizeof(struct ecoff_filehdr));
	p->p_md.md_tf->tf_gp = eap->ea_gp_value;
}

/*
 * cpu_exec_ecoff_hook():
 *	cpu-dependent ECOFF format hook for execve().
 * 
 * Do any machine-dependent diddling of the exec package when doing ECOFF.
 *
 */
int
cpu_exec_ecoff_hook(p, epp, eap)
	struct proc *p;
	struct exec_package *epp;
	struct ecoff_aouthdr *eap;
{
	struct ecoff_filehdr *efp = epp->ep_hdr;
	extern struct emul emul_netbsd;
#ifdef COMPAT_OSF1
	extern struct emul emul_osf1;
#endif

	switch (efp->ef_magic) {
#ifdef COMPAT_OSF1
	case ECOFF_MAGIC_ALPHA:
		epp->ep_emul = &emul_osf1;
		break;
#endif

	case ECOFF_MAGIC_NETBSD_ALPHA:
		epp->ep_emul = &emul_netbsd;
		break;

	default:
		return ENOEXEC;
	}
	return 0;
}
#endif

vm_offset_t
vtophys(vaddr)
	vm_offset_t vaddr;
{
	vm_offset_t paddr;

	if (vaddr < K0SEG_BEGIN) {
		printf("vtophys: invalid vaddr 0x%lx", vaddr);
		paddr = vaddr;
	} else if (vaddr < K0SEG_END)
		paddr = k0segtophys(vaddr);
	else
		paddr = vatopa(vaddr);

#if 0
	printf("vtophys(0x%lx) -> %lx\n", vaddr, paddr);
#endif

	return (paddr);
}
