/*	$NetBSD: machdep.c,v 1.183 1996/01/04 22:22:01 jtc Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
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

#include <dev/cons.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i8042reg.h>
#include <dev/ic/mc146818reg.h>
#include <i386/isa/isa_machdep.h>
#include <i386/isa/nvram.h>

#include "isa.h"
#include "npx.h"
#if NNPX > 0
extern struct proc *npxproc;
#endif

/* the following is used externally (sysctl_hw) */
char machine[] = "i386";		/* cpu "architecture" */

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

int	physmem;
int	dumpmem_low;
int	dumpmem_high;
int	boothowto;
int	cpu_class;

struct	msgbuf *msgbufp;
int	msgbufmapped;

vm_map_t buffer_map;

extern	vm_offset_t avail_start, avail_end;
static	vm_offset_t hole_start, hole_end;
static	vm_offset_t avail_next;

caddr_t allocsys __P((caddr_t));
void dumpsys __P((void));
void cpu_reset __P((void));

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	unsigned i;
	caddr_t v;
	int sz;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
	struct pcb *pcb;
	int x;

	/*
	 * Initialize error message buffer (at end of core).
	 */
	/* avail_end was pre-decremented in pmap_bootstrap to compensate */
	for (i = 0; i < btoc(sizeof(struct msgbuf)); i++)
		pmap_enter(pmap_kernel(),
		    (vm_offset_t)((caddr_t)msgbufp + i * NBPG),
		    avail_end + i * NBPG, VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
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
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
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

	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	configure();

	/*
	 * Set up proc0's TSS and LDT.
	 */
	curpcb = pcb = &proc0.p_addr->u_pcb;
	pcb->pcb_flags = 0;
	pcb->pcb_tss.tss_ioopt =
	    ((caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss) << 16;
	for (x = 0; x < sizeof(pcb->pcb_iomap) / 4; x++)
		pcb->pcb_iomap[x] = 0xffffffff;

	pcb->pcb_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
	pcb->pcb_cr0 = rcr0();
	pcb->pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	pcb->pcb_tss.tss_esp0 = (int)proc0.p_addr + USPACE - 16;
	tss_alloc(pcb);

	ltr(pcb->pcb_tss_sel);
	lldt(pcb->pcb_ldt_sel);

	proc0.p_md.md_regs = (struct trapframe *)pcb->pcb_tss.tss_esp0 - 1;

}

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))
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
	 * Determine how many buffers to allocate.  We use 10% of the
	 * first 2MB of memory, and 5% of the rest, with a minimum of 16
	 * buffers.  We allocate 1/2 as many swap buffer headers as file
	 * i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / (10 * CLSIZE);
		else
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
	return v;
}

/*  
 * Info for CTL_HW
 */
char	cpu_model[120];
extern	char version[];

struct cpu_nameclass i386_cpus[] = {
	{ "i386SX",	CPUCLASS_386 },	/* CPU_386SX */
	{ "i386DX",	CPUCLASS_386 },	/* CPU_386   */
	{ "i486SX",	CPUCLASS_486 },	/* CPU_486SX */
	{ "i486DX",	CPUCLASS_486 },	/* CPU_486   */
	{ "Pentium",	CPUCLASS_586 },	/* CPU_586   */
	{ "Cx486DLC",	CPUCLASS_486 },	/* CPU_486DLC (Cyrix) */
};

identifycpu()
{
	int len;
	extern char cpu_vendor[];

	printf("CPU: ");
#ifdef DIAGNOSTIC
	if (cpu < 0 || cpu >= (sizeof i386_cpus/sizeof(struct cpu_nameclass)))
		panic("unknown cpu type %d\n", cpu);
#endif
	sprintf(cpu_model, "%s (", i386_cpus[cpu].cpu_name);
	if (cpu_vendor[0] != '\0') {
		strcat(cpu_model, cpu_vendor);
		strcat(cpu_model, " ");
	}

	cpu_class = i386_cpus[cpu].cpu_class;
	switch(cpu_class) {
	case CPUCLASS_386:
		strcat(cpu_model, "386");
		break;
	case CPUCLASS_486:
		strcat(cpu_model, "486");
		break;
	case CPUCLASS_586:
		strcat(cpu_model, "586");
		break;
	default:
		strcat(cpu_model, "unknown");	/* will panic below... */
		break;
	}
	strcat(cpu_model, "-class CPU)");
	printf("%s\n", cpu_model);	/* cpu speed would be nice, but how? */

	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
#if !defined(I386_CPU) && !defined(I486_CPU) && !defined(I586_CPU)
#error No CPU classes configured.
#endif
#ifndef I586_CPU
	case CPUCLASS_586:
#ifdef I486_CPU
		printf("NOTICE: lowering CPU class to i486\n");
		cpu_class = CPUCLASS_486;
		break;
#endif
#endif
#ifndef I486_CPU
	case CPUCLASS_486:
#ifdef I386_CPU
		printf("NOTICE: lowering CPU class to i386\n");
		cpu_class = CPUCLASS_386;
		break;
#endif
#endif
#ifndef I386_CPU
	case CPUCLASS_386:
		panic("CPU class not configured");
#endif
	default:
		break;
	}

	if (cpu == CPU_486DLC) {
#ifndef CYRIX_CACHE_WORKS
		printf("WARNING: CYRIX 486DLC CACHE UNCHANGED.\n");
#else
#ifndef CYRIX_CACHE_REALLY_WORKS
		printf("WARNING: CYRIX 486DLC CACHE ENABLED IN HOLD-FLUSH MODE.\n");
#else
		printf("WARNING: CYRIX 486DLC CACHE ENABLED.\n");
#endif
#endif
	}

#if defined(I486_CPU) || defined(I586_CPU)
	/*
	 * On a 486 or above, enable ring 0 write protection and outer ring
	 * alignment checking.
	 */
	if (cpu_class >= CPUCLASS_486)
		lcr0(rcr0() | CR0_WP | CR0_AM);
#endif
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

#ifdef COMPAT_IBCS2
void
ibcs2_sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	extern int bsd_to_ibcs2_sig[];

	sendsig(catcher, bsd_to_ibcs2_sig[sig], mask, code);
}
#endif

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;
	extern char sigcode[], esigcode[];

	/* 
	 * Build the argument list for the signal handler.
	 */
	frame.sf_signum = sig;

	tf = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct sigframe *)tf->tf_esp - 1;
	}

	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.sc_onstack = oonstack;
	frame.sf_sc.sc_mask = mask;
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		frame.sf_sc.sc_gs = tf->tf_vm86_gs;
		frame.sf_sc.sc_fs = tf->tf_vm86_fs;
		frame.sf_sc.sc_es = tf->tf_vm86_es;
		frame.sf_sc.sc_ds = tf->tf_vm86_ds;
	} else
#endif
	{
		__asm("movl %%gs,%w0" : "=r" (frame.sf_sc.sc_gs));
		__asm("movl %%fs,%w0" : "=r" (frame.sf_sc.sc_fs));
		frame.sf_sc.sc_es = tf->tf_es;
		frame.sf_sc.sc_ds = tf->tf_ds;
	}
	frame.sf_sc.sc_edi    = tf->tf_edi;
	frame.sf_sc.sc_esi    = tf->tf_esi;
	frame.sf_sc.sc_ebp    = tf->tf_ebp;
	frame.sf_sc.sc_ebx    = tf->tf_ebx;
	frame.sf_sc.sc_edx    = tf->tf_edx;
	frame.sf_sc.sc_ecx    = tf->tf_ecx;
	frame.sf_sc.sc_eax    = tf->tf_eax;
	frame.sf_sc.sc_eip    = tf->tf_eip;
	frame.sf_sc.sc_cs     = tf->tf_cs;
	frame.sf_sc.sc_eflags = tf->tf_eflags;
	frame.sf_sc.sc_esp    = tf->tf_esp;
	frame.sf_sc.sc_ss     = tf->tf_ss;

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_esp = (int)fp;
	tf->tf_eip = (int)(((char *)PS_STRINGS) - (esigcode - sigcode));
#ifdef VM86
	tf->tf_eflags &= ~PSL_VM;
#endif
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);
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
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, context;
	register struct trapframe *tf;

	tf = p->p_md.md_regs;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, sigcntxp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Check for security violations.  If we're returning to protected
	 * mode, the CPU will validate the segment registers automatically
	 * and generate a trap on violations.  We handle the trap, rather
	 * than doing all of the checking here.
	 */
	if (((context.sc_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
	    !USERMODE(context.sc_cs, context.sc_eflags))
		return (EINVAL);

	if (context.sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = context.sc_mask & ~sigcantmask;

	/*
	 * Restore signal context.
	 */
#ifdef VM86
	if (context.sc_eflags & PSL_VM) {
		tf->tf_vm86_gs = context.sc_gs;
		tf->tf_vm86_fs = context.sc_fs;
		tf->tf_vm86_es = context.sc_es;
		tf->tf_vm86_ds = context.sc_ds;
	} else
#endif
	{
		/* %fs and %gs were restored by the trampoline. */
		tf->tf_es = context.sc_es;
		tf->tf_ds = context.sc_ds;
	}
	tf->tf_edi    = context.sc_edi;
	tf->tf_esi    = context.sc_esi;
	tf->tf_ebp    = context.sc_ebp;
	tf->tf_ebx    = context.sc_ebx;
	tf->tf_edx    = context.sc_edx;
	tf->tf_ecx    = context.sc_ecx;
	tf->tf_eax    = context.sc_eax;
	tf->tf_eip    = context.sc_eip;
	tf->tf_cs     = context.sc_cs;
	tf->tf_eflags = context.sc_eflags;
	tf->tf_esp    = context.sc_esp;
	tf->tf_ss     = context.sc_ss;

	return (EJUSTRETURN);
}

int	waittime = -1;
struct pcb dumppcb;

void
boot(howto)
	register int howto;
{
	extern int cold;

	if (cold) {
		printf("hit reset please");
		for(;;);
	}
	boothowto = howto;
	if ((howto&RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}
	splhigh();
	if (howto & RB_HALT) {
		doshutdownhooks();
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	} else {
		if (howto & RB_DUMP) {
			savectx(&dumppcb, 0);
			dumppcb.pcb_cr3 = rcr3();
			dumpsys();
		}
		doshutdownhooks();
	}
	printf("rebooting...\n");
	cpu_reset();
	for(;;) ;
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

	dumpsize = btoc(IOM_END + ctob(dumpmem_high));

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
#define BYTES_PER_DUMP  NBPG	/* must be a multiple of pagesize XXX small */
static vm_offset_t dumpspace;

vm_offset_t
reserve_dumppages(p)
	vm_offset_t p;
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

void
dumpsys()
{
	unsigned bytes, i, n;
	int maddr, psize;
	daddr_t blkno;
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int error = 0;
	int c;

	msgbufmapped = 0;	/* don't record dump msgs in msgbuf */
	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

#if 0	/* XXX this doesn't work.  grr. */
        /* toss any characters present prior to dump */
	while (sget() != NULL); /*syscons and pccons differ */
#endif

	bytes = ctob(dumpmem_high) + IOM_END;
	maddr = 0;
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;
	for (i = 0; i < bytes; i += n) {
		/*
		 * Avoid dumping the ISA memory hole, and areas that
		 * BIOS claims aren't in low memory.
		 */
		if (i >= ctob(dumpmem_low) && i < IOM_END) {
			n = IOM_END - i;
			maddr += n;
			blkno += btodb(n);
			continue;
		}

		/* Print out how many MBs we to go. */
		n = bytes - i;
		if (n && (n % (1024*1024)) == 0)
			printf("%d ", n / (1024 * 1024));

		/* Limit size for next transfer. */
		if (n > BYTES_PER_DUMP)
			n =  BYTES_PER_DUMP;

		(void) pmap_map(dumpspace, maddr, maddr + n, VM_PROT_READ);
		error = (*dump)(dumpdev, blkno, (caddr_t)dumpspace, n);
		if (error)
			break;
		maddr += n;
		blkno += btodb(n);			/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
		/* operator aborting dump? */
		if (sget() != NULL) {
			error = EINTR;
			break;
		}
#endif
	}

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
	delay(5000000);		/* 5 seconds */
}

#ifdef HZ
/*
 * If HZ is defined we use this code, otherwise the code in
 * /sys/i386/i386/microtime.s is used.  The other code only works
 * for HZ=100.
 */
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();

	*tvp = time;
	tvp->tv_usec += tick;
	splx(s);
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
}
#endif /* HZ */

/*
 * Clear registers on exec
 */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	register struct pcb *pcb = &p->p_addr->u_pcb;
	register struct trapframe *tf;

#if NNPX > 0
	/* If we were using the FPU, forget about it. */
	if (npxproc == p)
		npxdrop();
#endif

#ifdef USER_LDT
	if (pcb->pcb_flags & PCB_USER_LDT)
		i386_user_cleanup(pcb);
#endif

	p->p_md.md_flags &= ~MDP_USEDFPU;
	pcb->pcb_flags = 0;

	tf = p->p_md.md_regs;
	__asm("movl %w0,%%gs" : : "r" (LSEL(LUDATA_SEL, SEL_UPL)));
	__asm("movl %w0,%%fs" : : "r" (LSEL(LUDATA_SEL, SEL_UPL)));
	tf->tf_es = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_ds = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_ebp = 0;
	tf->tf_ebx = (int)PS_STRINGS;
	tf->tf_eip = pack->ep_entry;
	tf->tf_cs = LSEL(LUCODE_SEL, SEL_UPL);
	tf->tf_eflags = PSL_USERSET;
	tf->tf_esp = stack;
	tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);

	retval[1] = 0;
}

/*
 * Initialize segments and descriptor tables
 */

union descriptor gdt[NGDT];
union descriptor ldt[NLDT];
struct gate_descriptor idt[NIDT];

extern  struct user *proc0paddr;

void
setgate(gd, func, args, type, dpl)
	struct gate_descriptor *gd;
	void *func;
	int args, type, dpl;
{

	gd->gd_looffset = (int)func;
	gd->gd_selector = GSEL(GCODE_SEL, SEL_KPL);
	gd->gd_stkcpy = args;
	gd->gd_xx = 0;
	gd->gd_type = type;
	gd->gd_dpl = dpl;
	gd->gd_p = 1;
	gd->gd_hioffset = (int)func >> 16;
}

void
setregion(rd, base, limit)
	struct region_descriptor *rd;
	void *base;
	size_t limit;
{

	rd->rd_limit = (int)limit;
	rd->rd_base = (int)base;
}

void
setsegment(sd, base, limit, type, dpl, def32, gran)
	struct segment_descriptor *sd;
	void *base;
	size_t limit;
	int type, dpl, def32, gran;
{

	sd->sd_lolimit = (int)limit;
	sd->sd_lobase = (int)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (int)limit >> 16;
	sd->sd_xx = 0;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (int)base >> 24;
}

#define	IDTVEC(name)	__CONCAT(X, name)
extern	IDTVEC(div),     IDTVEC(dbg),     IDTVEC(nmi),     IDTVEC(bpt),
	IDTVEC(ofl),     IDTVEC(bnd),     IDTVEC(ill),     IDTVEC(dna),
	IDTVEC(dble),    IDTVEC(fpusegm), IDTVEC(tss),     IDTVEC(missing),
	IDTVEC(stk),     IDTVEC(prot),    IDTVEC(page),    IDTVEC(rsvd),
	IDTVEC(fpu),     IDTVEC(align),
	IDTVEC(syscall), IDTVEC(osyscall);

void
init386(first_avail)
	vm_offset_t first_avail;
{
	int x;
	unsigned biosbasemem, biosextmem;
	struct region_descriptor region;
	extern char etext[], sigcode[], esigcode[];
	extern void consinit __P((void));
	extern void lgdt();

	proc0.p_addr = proc0paddr;

	consinit();	/* XXX SHOULD NOT BE DONE HERE */

	/* make gdt gates and memory segments */
	setsegment(&gdt[GCODE_SEL].sd, 0, 0xfffff, SDT_MEMERA, SEL_KPL, 1, 1);
	setsegment(&gdt[GDATA_SEL].sd, 0, 0xfffff, SDT_MEMRWA, SEL_KPL, 1, 1);
	setsegment(&gdt[GLDT_SEL].sd, ldt, sizeof(ldt) - 1, SDT_SYSLDT, SEL_KPL,
	    0, 0);
	setsegment(&gdt[GUCODE_SEL].sd, 0, i386_btop(VM_MAXUSER_ADDRESS) - 1,
	    SDT_MEMERA, SEL_UPL, 1, 1);
	setsegment(&gdt[GUDATA_SEL].sd, 0, i386_btop(VM_MAXUSER_ADDRESS) - 1,
	    SDT_MEMRWA, SEL_UPL, 1, 1);

	/* make ldt gates and memory segments */
	setgate(&ldt[LSYS5CALLS_SEL].gd, &IDTVEC(osyscall), 1, SDT_SYS386CGT,
	    SEL_UPL);
	ldt[LUCODE_SEL] = gdt[GUCODE_SEL];
	ldt[LUDATA_SEL] = gdt[GUDATA_SEL];
	ldt[LBSDICALLS_SEL] = ldt[LSYS5CALLS_SEL];

	/* exceptions */
	for (x = 0; x < NIDT; x++)
		setgate(&idt[x], &IDTVEC(rsvd), 0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  0], &IDTVEC(div),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  1], &IDTVEC(dbg),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  2], &IDTVEC(nmi),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  3], &IDTVEC(bpt),     0, SDT_SYS386TGT, SEL_UPL);
	setgate(&idt[  4], &IDTVEC(ofl),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  5], &IDTVEC(bnd),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  6], &IDTVEC(ill),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  7], &IDTVEC(dna),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  8], &IDTVEC(dble),    0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[  9], &IDTVEC(fpusegm), 0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[ 10], &IDTVEC(tss),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[ 11], &IDTVEC(missing), 0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[ 12], &IDTVEC(stk),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[ 13], &IDTVEC(prot),    0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[ 14], &IDTVEC(page),    0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[ 16], &IDTVEC(fpu),     0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[ 17], &IDTVEC(align),   0, SDT_SYS386TGT, SEL_KPL);
	setgate(&idt[128], &IDTVEC(syscall), 0, SDT_SYS386TGT, SEL_UPL);

	setregion(&region, gdt, sizeof(gdt) - 1);
	lgdt(&region);
	setregion(&region, idt, sizeof(idt) - 1);
	lidt(&region);

#if NISA > 0
	isa_defaultirq();
#endif

	splhigh();
	enable_intr();

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
	 * Use BIOS values stored in RTC CMOS RAM, since probing
	 * breaks certain 386 AT relics.
	 */
	biosbasemem = (mc146818_read(NULL, NVRAM_BASEHI) << 8) |
	    mc146818_read(NULL, NVRAM_BASELO);
	biosextmem = (mc146818_read(NULL, NVRAM_EXTHI) << 8) |
	    mc146818_read(NULL, NVRAM_EXTLO);

	/* Round down to whole pages. */
	biosbasemem &= -(NBPG / 1024);
	biosextmem &= -(NBPG / 1024);

	avail_start = NBPG;	/* BIOS leaves data in low memory */
				/* and VM system doesn't work with phys 0 */
	avail_end = biosextmem ? IOM_END + biosextmem * 1024
	    : biosbasemem * 1024;

	/* number of pages of physmem addr space */
	physmem = btoc((biosbasemem + biosextmem) * 1024);
	dumpmem_low = btoc(biosbasemem * 1024);
	dumpmem_high = btoc(biosextmem * 1024);

	/*
	 * Initialize for pmap_free_pages and pmap_next_page.
	 * These guys should be page-aligned.
	 */
	hole_start = biosbasemem * 1024;
	/* we load right after the I/O hole; adjust hole_end to compensate */
	hole_end = round_page(first_avail);
	avail_next = avail_start;

	if (physmem < btoc(2 * 1024 * 1024)) {
		printf("warning: too little memory available; running in degraded mode\n"
		    "press a key to confirm\n\n");
		cngetc();
	}

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap((vm_offset_t)atdevbase + IOM_SIZE);
}

struct queue {
	struct queue *q_next, *q_prev;
};

/*
 * insert an element into a queue
 */
void
_insque(elem, head)
	register struct queue *elem, *head;
{
	register struct queue *next;

	next = head->q_next;
	elem->q_next = next;
	head->q_next = elem;
	elem->q_prev = head;
	next->q_prev = elem;
}

/*
 * remove an element from a queue
 */
void
_remque(elem)
	register struct queue *elem;
{
	register struct queue *next, *prev;

	next = elem->q_next;
	prev = elem->q_prev;
	next->q_prev = prev;
	prev->q_next = next;
	elem->q_prev = 0;
}


#ifdef COMPAT_NOMID
static int
exec_nomid(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error;
	u_long midmag, magic;
	u_short mid;
	struct exec *execp = epp->ep_hdr;

	/* check on validity of epp->ep_hdr performed by exec_out_makecmds */

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0xffff;
	magic = midmag & 0xffff;

	if (magic == 0) {
		magic = (execp->a_midmag & 0xffff);
		mid = MID_ZERO;
	}

	midmag = mid << 16 | magic;

	switch (midmag) {
	case (MID_ZERO << 16) | ZMAGIC:
		/*
		 * 386BSD's ZMAGIC format:
		 */
		error = cpu_exec_aout_prep_oldzmagic(p, epp);
		break;

	case (MID_ZERO << 16) | QMAGIC:
		/*
		 * BSDI's QMAGIC format:
		 * same as new ZMAGIC format, but with different magic number
		 */
		error = exec_aout_prep_zmagic(p, epp);
		break;

	default:
		error = ENOEXEC;
	}

	return error;
}
#endif

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 *
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 *
 * On the i386, old (386bsd) ZMAGIC binaries and BSDI QMAGIC binaries
 * if COMPAT_NOMID is given as a kernel option.
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;

#ifdef COMPAT_NOMID
	if ((error = exec_nomid(p, epp)) == 0)
		return error;
#endif /* ! COMPAT_NOMID */

	return error;
}

#if defined(COMPAT_NOMID) || defined(COMPAT_FREEBSD)
/*
 * cpu_exec_aout_prep_oldzmagic():
 *	Prepare the vmcmds to build a vmspace for an old (386BSD) ZMAGIC
 *	binary.
 *
 * Cloned from exec_aout_prep_zmagic() in kern/exec_aout.c; a more verbose
 * description of operation is there.
 */
int
cpu_exec_aout_prep_oldzmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;
	struct exec_vmcmd *ccmdp;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = epp->ep_taddr + execp->a_text;
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((execp->a_text != 0 || execp->a_data != 0) &&
	    epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, NBPG, /* XXX should NBPG be CLBYTES? */
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp,
	    execp->a_text + NBPG, /* XXX should NBPG be CLBYTES? */
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, execp->a_bss,
	    epp->ep_daddr + execp->a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return exec_aout_setup_stack(p, epp);
}
#endif /* COMPAT_NOMID || COMPAT_FREEBSD */

u_int
pmap_free_pages()
{

	if (avail_next <= hole_start)
		return ((hole_start - avail_next) / NBPG +
			(avail_end - hole_end) / NBPG);
	else
		return ((avail_end - avail_next) / NBPG);
}

int
pmap_next_page(addrp)
	vm_offset_t *addrp;
{

	if (avail_next + NBPG > avail_end)
		return FALSE;

	if (avail_next + NBPG > hole_start && avail_next < hole_end)
		avail_next = hole_end;

	*addrp = avail_next;
	avail_next += NBPG;
	return TRUE;
}

u_int
pmap_page_index(pa)
	vm_offset_t pa;
{

	if (pa >= avail_start && pa < hole_start)
		return i386_btop(pa - avail_start);
	if (pa >= hole_end && pa < avail_end)
		return i386_btop(pa - hole_end + hole_start - avail_start);
	return -1;
}

/*
 * consinit:
 * initialize the system console.
 * XXX - shouldn't deal with this initted thing, but then,
 * it shouldn't be called from init386 either.
 */
void
consinit()
{
	static int initted;

	if (initted)
		return;
	initted = 1;
	cninit();
}

void
cpu_reset()
{
	struct region_descriptor region;

	/* Toggle the hardware reset line on the keyboard controller. */
	outb(KBCMDP, KBC_PULSE0);
	delay(20000);
	outb(KBCMDP, KBC_PULSE0);
	delay(20000);

	/*
	 * Try to cause a triple fault and watchdog reset by setting the
	 * IDT to point to nothing.
	 */
	setregion(&region, 0, 0);
	lidt(&region);

	/*
	 * Try to cause a triple fault and watchdog reset by unmapping the
	 * entire address space.
	 */
	bzero((caddr_t)PTD, NBPG);
	pmap_update(); 

	for (;;);
}
