/*	$OpenBSD: machdep.c,v 1.24 1999/01/07 23:16:01 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: machdep.c 1.74 92/12/20$
 *
 *	@(#)machdep.c	8.10 (Berkeley) 4/20/94
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
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/mount.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/prom.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <dev/cons.h>
#include <net/netisr.h>

#define	MAXMEM	64*1024*CLSIZE	/* XXX - from cmap.h */
#include <vm/vm_kern.h>

/* the following is used externally (sysctl_hw) */
char machine[] = "mvme68k";		/* cpu "architecture" */

vm_map_t buffer_map;
extern vm_offset_t avail_end;

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
int	msgbufmapped;		/* set when safe to use msgbuf */
int	maxmem;			/* max memory per process */
int	physmem = MAXMEM;	/* max supported memory, changes to actual */
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

extern	u_int lowram;
extern	short exframesize[];

#ifdef COMPAT_HPUX
extern struct emul emul_hpux;
#endif
#ifdef COMPAT_SUNOS
extern struct emul emul_sunos;
#endif

void
mvme68k_init()
{
#if defined(MACHINE_NEW_NONCONTIG)
	extern vm_offset_t avail_start, avail_end;
        
	/*
 	 * Tell the VM system about available physical memory.  The
 	 * hp300 only has one segment.
 	 */
#if defined(UVM)
	uvm_page_physload(atop(avail_start), atop(avail_end),
    		atop(avail_start), atop(avail_end));
#else
	vm_page_physload(atop(avail_start), atop(avail_end),
    		atop(avail_start), atop(avail_end));
#endif /* UVM */
#endif /* MACHINE_NEW_NONCONTIG */
}

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

#ifdef DDB
	db_machine_init();
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	register unsigned i;
	register caddr_t v, firstaddr;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
#ifdef BUFFERS_UNMANAGED
	vm_offset_t bufmemp;
	caddr_t buffermem;
	int ix;
#endif
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in pmap_bootstrap to compensate.
	 */
	for (i = 0; i < btoc(sizeof (struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t)msgbufp,
		    avail_end + i * NBPG, VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Allocate space for system data structures.
	 * The first available real memory address is in "firstaddr".
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */
	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	firstaddr = 0;
again:
	v = (caddr_t)firstaddr;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
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
	 * We just allocate a flat 5%.  Insure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = physmem / 20 / CLSIZE;
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
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vm_size_t)(v - firstaddr);
		firstaddr = (caddr_t) kmem_alloc(kernel_map, round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
#ifdef BUFFERS_UNMANAGED
		buffermem = (caddr_t) kmem_alloc(kernel_map, bufpages*CLBYTES);
		if (buffermem == 0)
			panic("startup: no room for buffers");
#endif
		goto again;
	}
	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vm_size_t)(v - firstaddr) != size)
		panic("startup: table size inconsistency");
	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
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
				 16*NCARGS, TRUE);
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
	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);
#ifdef MFS
	/*
	 * Check to see if a mini-root was loaded into memory. It resides
	 * at the start of the next page just after the end of BSS.
	 */
	{
		extern void *smini;

		if (smini && (boothowto & RB_MINIROOT)) {
			boothowto |= RB_DFLTROOT;
			mfs_initminiroot(smini);
		}
	}
#endif

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

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
}

/*
 * Set registers on exec.
 * XXX Should clear registers except sp, pc,
 * but would break init; should be fixed soon.
 */
void
setregs(p, pack, stack, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct frame *frame = (struct frame *)p->p_md.md_regs;

	frame->f_pc = pack->ep_entry & ~1;
	frame->f_regs[SP] = stack;
	frame->f_regs[A2] = (int)PS_STRINGS;

	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;
	if (fputype)
		m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);

#ifdef COMPAT_SUNOS
	/*
	 * SunOS' ld.so does self-modifying code without knowing
	 * about the 040's cache purging needs.  So we need to uncache
	 * writeable executable pages.
	 */
	if (p->p_emul == &emul_sunos)
		p->p_md.md_flags |= MDP_UNCACHE_WX;
	else
		p->p_md.md_flags &= ~MDP_UNCACHE_WX;
#endif
}

/*
 * Info for CTL_HW
 */
char	cpu_model[120];
extern	char version[];

int	cputyp;
int	cpuspeed;

struct	mvmeprom_brdid brdid;

identifycpu()
{
	char *t, *mc;
	char speed[6];
	char suffix[30];
	extern u_long fpvect_tab, fpvect_end, fpsp_tab;
	int len;

	bzero(suffix, sizeof suffix);

	mc = (mmutype == MMU_68040 ? "40" :
	    (mmutype == MMU_68030 ? "30" : "20"));
	switch (cputyp) {
#ifdef MVME147
	case CPU_147:
		bcopy(&brdid.suffix, suffix, sizeof brdid.suffix);
		sprintf(suffix, "MVME%x", brdid.model, suffix);
		cpuspeed = pccspeed((struct pccreg *)IIOV(0xfffe1000));
		sprintf(speed, "%02d", cpuspeed);
		break;
#endif
#if defined(MVME162) || defined(MVME167) || defined(MVME177)
	case CPU_162:
	case CPU_167:
	case CPU_177:
		bzero(speed, sizeof speed);
		speed[0] = brdid.speed[0];
		speed[1] = brdid.speed[1];
		if (brdid.speed[2] != '0' &&
		    brdid.speed[3] != '0') {
			speed[2] = '.';
			speed[3] = brdid.speed[2];
			speed[4] = brdid.speed[3];
		}
		cpuspeed = (speed[0] - '0') * 10 + (speed[1] - '0');
		bcopy(brdid.longname, suffix, sizeof(brdid.longname));
		for (len = strlen(suffix)-1; len; len--) {
			if (suffix[len] == ' ')
				suffix[len] = '\0';
			else
				break;
		}
		break;
#endif
	}
	sprintf(cpu_model, "Motorola %s: %sMHz MC680%s CPU",
	    suffix, speed, mc);
	switch (mmutype) {
	case MMU_68040:
#ifdef FPSP
		bcopy(&fpsp_tab, &fpvect_tab,
			(&fpvect_end - &fpvect_tab) * sizeof (fpvect_tab));

#endif
		strcat(cpu_model, "+MMU");
		break;
	case MMU_68030:
		strcat(cpu_model, "+MMU");
		break;
	case MMU_68851:
		strcat(cpu_model, ", MC68851 MMU");
		break;
	default:
		printf("%s\nunknown MMU type %d\n", cpu_model, mmutype);
		panic("startup");
	}
	len = strlen(cpu_model);
	if (mmutype == MMU_68040)
		len += sprintf(cpu_model + len,
		    "+FPU, 4k on-chip physical I/D caches");
#if defined(M68030) || defined(M68020)
	else {
		int fpu = fpu_gettype();

		switch (fpu) {
		case 0:
			break;
		case 1:
		case 2:
			len += sprintf(cpu_model + len, ", MC6888%d FPU", fpu);
			break;
		case 3:
			len += sprintf(cpu_model + len, ", unknown FPU", speed);
			break;
		}
	}
#endif
	printf("%s\n", cpu_model);
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

int	waittime = -1;

static struct haltvec *halts;

/* XXX insert by priority */
void
halt_establish(fn, pri)
	void (*fn) __P((void));
	int pri;
{
	struct haltvec *hv, *h;

	hv = (struct haltvec *)malloc(sizeof(*hv), M_TEMP, M_NOWAIT);
	if (hv == NULL)
		return;
	hv->hv_fn = fn;
	hv->hv_pri = pri;
	hv->hv_next = NULL;

	/* put higher priorities earlier in the list */
	h = halts;
	if (h == NULL) {
		halts = hv;
		return;
	}

	if (h->hv_pri < pri) {
		/* higher than first element */
		hv->hv_next = halts;
		halts = hv;
		return;
	}

	while (h) {
		if (h->hv_next == NULL) {
			/* no more elements, must be the lowest priority */
			h->hv_next = hv;
			return;
		}

		if (h->hv_next->hv_pri < pri) {
			/* next element is lower */
			hv->hv_next = h->hv_next;
			h->hv_next = hv;
			return;
		}
		h = h->hv_next;
	}
}

void
boot(howto)
	register int howto;
{

	/* take a snap shot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(curproc->p_addr);

	boothowto = howto;
	if ((howto&RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* do that another panic fly away */
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
	splhigh();			/* extreme priority */
	if (howto&RB_HALT) {
		printf("halted\n\n");
	} else {
		struct haltvec *hv;

		if (howto & RB_DUMP)
			dumpsys();
		for (hv = halts; hv; hv = hv->hv_next)
			(*hv->hv_fn)();
		doboot();
	}
	while (1)
		;
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */

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

	/*
	 * XXX include the final RAM page which is not included in physmem.
	 */
	dumpsize = physmem + 1;

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
}

initcpu()
{
}

straytrap(pc, evec)
	int pc;
	u_short evec;
{
	printf("unexpected trap (vector %d) from %x\n",
	    (evec & 0xFFF) >> 2, pc);
}

int	*nofault;

int
badpaddr(addr, size)
	register void *addr;
	int size;
{
	int off = (int)addr & PGOFSET;
	caddr_t v, p = (void *)((int)addr & ~PGOFSET);
	int x;

	v = mapiodev(p, NBPG);
	if (v == NULL)
		return (1);
	v += off;
	x = badvaddr(v + off, size);
	unmapiodev(v, NBPG);
	return (x);
}

int
badvaddr(addr, size)
	register caddr_t addr;
	int size;
{
	register int i;
	label_t	faultbuf;

#ifdef lint
	i = *addr; if (i) return(0);
#endif
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *)0;
		return (1);
	}
	switch (size) {
	case 1:
		i = *(volatile char *)addr;
		break;
	case 2:
		i = *(volatile short *)addr;
		break;
	case 4:
		i = *(volatile long *)addr;
		break;
	}
	nofault = (int *)0;
	return (0);
}

netintr()
{
#ifdef INET
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
	if (netisr & (1 << NETISR_IP)) {
		netisr &= ~(1 << NETISR_IP);
		ipintr();
	}
#endif
#ifdef INET6
	if (netisr & (1 << NETISR_IPV6)) {
		netisr &= ~(1 << NETISR_IPV6);
		ipv6intr();
	}
#endif
#ifdef NETATALK
	if (netisr & (1 << NETISR_ATALK)) {
		netisr &= ~(1 << NETISR_ATALK);
		atintr();
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
#include "ppp.h"
#if NPPP > 0
	if (netisr & (1 << NETISR_PPP)) {
		netisr &= ~(1 << NETISR_PPP);
		pppintr();
	}
#endif
}

/*
 * Level 7 interrupts are normally caused by the ABORT switch,
 * drop into ddb.
 */
nmihand(frame)
	struct frame *frame;
{
#ifdef DDB
	printf("NMI ... going to debugger\n");
	Debugger();
#else
	/* panic?? */
	printf("unexpected level 7 interrupt ignored\n");
#endif
}

regdump(fp, sbytes)
	struct frame *fp; /* must not be register */
	int sbytes;
{
	static int doingdump = 0;
	register int i;
	int s;
	extern char *hexstr();

	if (doingdump)
		return;
	s = splhigh();
	doingdump = 1;
	printf("pid = %d, pc = %s, ",
	    curproc ? curproc->p_pid : -1, hexstr(fp->f_pc, 8));
	printf("ps = %s, ", hexstr(fp->f_sr, 4));
	printf("sfc = %s, ", hexstr(getsfc(), 4));
	printf("dfc = %s\n", hexstr(getdfc(), 4));
	printf("Registers:\n     ");
	for (i = 0; i < 8; i++)
		printf("        %d", i);
	printf("\ndreg:");
	for (i = 0; i < 8; i++)
		printf(" %s", hexstr(fp->f_regs[i], 8));
	printf("\nareg:");
	for (i = 0; i < 8; i++)
		printf(" %s", hexstr(fp->f_regs[i+8], 8));
	if (sbytes > 0) {
		if (fp->f_sr & PSL_S) {
			printf("\n\nKernel stack (%s):",
			    hexstr((int)(((int *)&fp)-1), 8));
			dumpmem(((int *)&fp)-1, sbytes, 0);
		} else {
			printf("\n\nUser stack (%s):", hexstr(fp->f_regs[SP], 8));
			dumpmem((int *)fp->f_regs[SP], sbytes, 1);
		}
	}
	doingdump = 0;
	splx(s);
}

#define KSADDR	((int *)((u_int)curproc->p_addr + USPACE - NBPG))

dumpmem(ptr, sz, ustack)
	register int *ptr;
	int sz, ustack;
{
	register int i, val;
	extern char *hexstr();

	for (i = 0; i < sz; i++) {
		if ((i & 7) == 0)
			printf("\n%s: ", hexstr((int)ptr, 6));
		else
			printf(" ");
		if (ustack == 1) {
			if ((val = fuword(ptr++)) == -1)
				break;
		} else {
			if (ustack == 0 &&
			    (ptr < KSADDR || ptr > KSADDR+(NBPG/4-1)))
				break;
			val = *ptr++;
		}
		printf("%s", hexstr(val, 8));
	}
	printf("\n");
}

char *
hexstr(val, len)
	register int val;
	int len;
{
	static char nbuf[9];
	register int x, i;

	if (len > 8)
		return("");
	nbuf[len] = '\0';
	for (i = len-1; i >= 0; --i) {
		x = val & 0xF;
		if (x > 9)
			nbuf[i] = x - 10 + 'A';
		else
			nbuf[i] = x + '0';
		val >>= 4;
	}
	return(nbuf);
}

#ifdef STACKCHECK
char oflowmsg[] = "k-stack overflow";
char uflowmsg[] = "k-stack underflow";

badkstack(oflow, fr)
	int oflow;
	struct frame fr;
{
	extern char kstackatbase[];

	printf("%s: sp should be %x\n", 
	    oflow ? oflowmsg : uflowmsg,
	    kstackatbase - (exframesize[fr.f_format] + 8));
	regdump(&fr, 0);
	panic(oflow ? oflowmsg : uflowmsg);
}
#endif

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 * 
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 */
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;
	struct exec *execp = epp->ep_hdr;

#ifdef COMPAT_SUNOS
	{
		extern sunos_exec_aout_makecmds
		    __P((struct proc *, struct exec_package *));
		if ((error = sunos_exec_aout_makecmds(p, epp)) == 0)
			return(0);
	}
#endif
	return (error);
}

u_char	myea[6] = { 0x08, 0x00, 0x3e, 0xff, 0xff, 0xff };

void
myetheraddr(ether)
	u_char *ether;
{
	bcopy(myea, ether, sizeof myea);
}

#if defined(M68030) || defined(M68020)
int
fpu_gettype()
{
	/*
	 * A 68881 idle frame is 28 bytes and a 68882's is 60 bytes.
	 * We, of course, need to have enough room for either.
	 */
	int	fpframe[60 / sizeof(int)];
	label_t	faultbuf;
	u_char	b;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *)0;
		return (0);		/* no FPU */
	}

	/*
	 * Synchronize FPU or cause a fault.
	 * This should leave the 881/882 in the IDLE state,
	 * state, so we can determine which we have by
	 * examining the size of the FP state frame
	 */
	asm("fnop");

	nofault = (int *)0;

	/*
	 * Presumably, this will not cause a fault--the fnop should
	 * have if this will.  We save the state in order to get the
	 * size of the frame.
	 */
	asm("movl %0, a0; fsave a0@" : : "a" (fpframe) : "a0" );
	b = *((u_char *) fpframe + 1);

	/*
	 * Now, restore a NULL state to reset the FPU.
	 */
	fpframe[0] = fpframe[1] = 0;
	m68881_restore(fpframe);

	if (b == 0x18)
		return (1);	/* The size of a 68881 IDLE frame is 0x18 */
	if (b == 0x38)
		return (2);	/* 68882 frame is 0x38 bytes long */
	return (3);		/* unknown FPU type */
}
#endif


#ifdef MVME162
#include <mvme68k/dev/mcreg.h>
/*
 * XXX
 * used by locore.s to figure out how much memory is on the machine.
 * At this stage we only know that our machine is a 162. It is very
 * unfortunate that the MCchip's address must be encoded here.
 */
int
memsize162()
{
	struct mcreg *mc = (struct mcreg *)0xfff42000;

	switch (mc->mc_memoptions & MC_MEMOPTIONS_DRAMMASK) {
	case MC_MEMOPTIONS_DRAM1M:
		return (1*1024*1024);
	case MC_MEMOPTIONS_DRAM2M:
		return (2*1024*1024);
	case MC_MEMOPTIONS_DRAM4M:
		return (4*1024*1024);
	case MC_MEMOPTIONS_DRAM4M2:
		return (4*1024*1024);
	case MC_MEMOPTIONS_DRAM8M:
		return (8*1024*1024);
	case MC_MEMOPTIONS_DRAM16M:
		return (16*1024*1024);
	default:
		/*
		 * XXX if the machine has no MC-controlled memory,
		 * perhaps it has a MCECC or MEMC040 controller?
		 */
		return (memsize1x7());
	}
}
#endif

#if defined(MVME162) || defined(MVME167) || defined(MVME177)
#include <mvme68k/dev/memcreg.h>
/*
 * XXX
 * finish writing this
 * 1) it is ugly
 * 2) it only looks at the first MEMC040/MCECC controller
 */
int
memsize1x7()
{
	struct memcreg *memc = (struct memcreg *)0xfff43000;
	u_long	x;

	x = MEMC_MEMCONF_RTOB(memc->memc_memconf);
	return (x);
}
#endif
