/*	$NetBSD: machdep.c,v 1.43 1996/01/15 05:30:47 phil Exp $	*/

/*-
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

/*
 * Modified for the pc532 by Phil Nelson.  2/3/93
 */

static char rcsid[] = "/b/source/CVS/src/sys/arch/pc532/pc532/machdep.c,v 1.2 1993/09/13 07:26:49 phil Exp";

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
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <dev/cons.h>

#include <net/netisr.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/icu.h>

extern vm_offset_t avail_end;
extern struct user *proc0paddr;

vm_map_t buffer_map;

/* A local function... */
void reboot_cpu();
void dumpsys __P((void));


/* the following is used externally (sysctl_hw) */
char machine[] = "pc532";
char cpu_model[] = "ns32532";

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
int	msgbufmapped = 0;		/* set when safe to use msgbuf */


/*  Real low level initialization.  This is called in unmapped mode and
    sets up the inital page directory and page tables for the kernel.
    This routine is the first to be called by locore.s to get the
    kernel running at the correct place in memory.
 */

extern char end[], _edata[];
extern vm_offset_t   avail_start;
extern vm_offset_t   avail_end;

int physmem = 0;
int maxmem = 0;

vm_offset_t KPTphys;

int IdlePTD;
int start_page;
int _istack;

int low_mem_map;

/* Support for VERY low debugging ... in case we get NO output.
   (e.g. in case pmap does not work and can't do regular mapped
   output. */
#if VERYLOWDEBUG
#include <pc532/umprintf.c> 
#endif

void
_low_level_init ()
{
  int ix, ix1, ix2;
  int  p0, p1, p2;
  extern int _mapped;

#if VERYLOWDEBUG
  umprintf ("starting low level init\n");
#endif

  mem_size = ram_size(end);
  physmem = btoc(mem_size);
  start_page = (((int)&end + NS532_PAGE_SIZE) & ~(NS532_PAGE_SIZE-1))
    & 0xffffff;
  avail_start = start_page; 
  avail_end   = mem_size - NS532_PAGE_SIZE;
  
#if VERYLOWDEBUG
  umprintf ("mem_size = 0x%x\nphysmem=%x\nstart_page=0x%x\navail_end=0x%x\n",
	    mem_size, physmem, start_page, avail_end);
#endif


  /* Initialize the mmu with a simple memory map. */

  /* A new interrupt stack, i.e. not the rom monitor's. */
  _istack = avail_start;
  avail_start += NS532_PAGE_SIZE;

  /* The page directory that starts the entire mapping. */
  p0 = (int) avail_start;
  IdlePTD = p0;
  KPTphys = p0;
  avail_start += NS532_PAGE_SIZE;

  /* First clear out the page table directory. */
  bzero((char *)p0, NS532_PAGE_SIZE);

  /* Now for the memory mapped I/O, the ICU and the eprom. */
  p1 = (int) avail_start;
  avail_start += NS532_PAGE_SIZE;
  bzero ((char *)p1, NS532_PAGE_SIZE);

  /* Addresses here start at FFC00000... */

  /* Map the interrupt stack to FFC00000 - FFC00FFF */
  WR_ADR(int, p1, _istack+3);

  /* All futhur entries are cache inhibited.  => 0x4? in low bits. */

  /* The Duarts and Parity.  Addresses FFC80000 */
  WR_ADR(int, p1+4*0x80, 0x28000043);

  /* SCSI Polled  (Reduced space.)  Addresses FFD00000 - FFDFFFFF */
  for (ix = 0x100; ix < 0x200; ix++)
    WR_ADR(int, p1 + ix*4, 0x30000043 + ((ix - 0x100)<<12));

  /* SCSI "DMA"  (Reduced space.)  Addresses FFE00000 - FFEEFFFF */
  for (ix = 0x200; ix < 0x2ff; ix++)
    WR_ADR(int, p1 + ix*4, 0x38000043 + ((ix - 0x200)<<12));

  /* SCSI "DMA" With A22 (EOP)  Addresses FFEFF000 - FFEFFFFF */
  WR_ADR(int, p1 + 0x2ff*4, 0x38400043);

  /* The e-prom  Addresses FFF00000 - FFF3FFFF */
  for (ix = 0x300; ix < 0x340; ix++)
    WR_ADR(int, p1 + ix*4, 0x10000043 + ((ix - 0x300)<<12));

  /* Finally the ICU!  Addresses FFFFF000 - FFFFFFFF */
  WR_ADR(int, p1+4*0x3ff, 0xFFFFF043);

  /* Add the memory mapped I/O entry in the directory. */
  WR_ADR(int, p0+4*1023, p1 + 0x43);

  /* Map the kernel pages starting at FE00000 and at 0.
	It also maps any pages past the end of the kernel,
	up to the value of avail_start at this point.
	These pages currently are:
	1 - interrupt stack
	2 - Top level page table
	3 - 2nd level page table for I/O
	4 - 2nd level page table for the kernel & low memory
	5-7 will be allocated as 2nd level page tables by pmap_bootstrap.
   */

  low_mem_map = p2 = (int) avail_start;
  avail_start += NS532_PAGE_SIZE;
  bzero ((char *)p2, NS532_PAGE_SIZE);
  WR_ADR(int,p0+4*pdei(KERNBASE), p2 + 3);
  WR_ADR(int,p0, p2+3);

  for (ix = 0; ix < (avail_start)/NS532_PAGE_SIZE; ix++) {
    WR_ADR(int, p2 + ix*4, NS532_PAGE_SIZE * ix + 3);
  }

  /* Load the ptb0 register and start mapping. */

  _mapped = 1;
  _load_ptb0 (p0);
  asm(" lmr mcr, 3");		/* Start the machine mapping, 1 vm space. */

}

extern void icu_init();

/* init532 is the first procedure called in mapped mode by locore.s
 */  

init532()
{
  int free_pages;
  void (**int_tab)();
  extern int _save_sp;

/*#include "ddb.h" */
#if NDDB > 0
	kdb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif

  /* Initialize the pmap stuff.... */
  pmap_bootstrap (avail_start, 0);
  /* now running on new page tables, configured, and u/iom is accessible */

  /* Set up the proc0paddr struct. */
  proc0paddr->u_pcb.pcb_flags = 0;
  proc0paddr->u_pcb.pcb_pl = 0xffffffff;
  proc0paddr->u_pcb.pcb_ptb = IdlePTD;
  proc0paddr->u_pcb.pcb_onstack = 
    	(struct on_stack *) proc0paddr + UPAGES*NBPG 
	- sizeof (struct on_stack);

  /* Set up the ICU. */
  icu_init();
  intr_init();
} 


/*
 * Machine-dependent startup code
 */
int boothowto = 0, Maxmem = 0;
long dumplo;
int physmem, maxmem;

extern int bootdev;
extern cyloffset;

/* pmap_enter prototype */
void pmap_enter __P((register pmap_t, vm_offset_t, register vm_offset_t,
	vm_prot_t, boolean_t));

void
cpu_startup(void)
{
	register int unixsize;
	register unsigned i;
	register struct pte *pte;
	int mapaddr, j;
	register caddr_t v;
	int maxbufs, base, residual;
	extern long Usrptsize;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
	int firstaddr;

	/*
	 * Initialize error message buffer (at end of core).
	 */

	/* avail_end was pre-decremented in pmap_bootstrap to compensate */
	for (i = 0; i < btoc(sizeof (struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t) msgbufp,
			   avail_end + i * NBPG, VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

#ifdef KDB
	kdb_init();			/* startup kernel debugger */
#endif
	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("\nreal mem  = 0x%x\n", ctob(physmem));

	/*
	 * Allocate space for system data structures.
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
	valloc(timeouts, struct timeout, ntimeout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
	/*
	 * Determine how many buffers to allocate.
	 * Use 10% of memory for the first 2 Meg, 5% of the remaining
	 * memory. Insure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < (2 * 1024 * 1024))
			bufpages = physmem / 10 / CLSIZE;
		else
			bufpages = ((2 * 1024 * 1024 + physmem) / 20) / CLSIZE;

	bufpages = min(NKMEMCLUSTERS*2/5, bufpages);  /* XXX ? - cgd */

	if (nbuf == 0) {
		nbuf = bufpages / 2;
		if (nbuf < 16) {
			nbuf = 16;
			/* XXX (cgd) -- broken vfs_bio currently demands this */
			bufpages = 32;
		}
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
		firstaddr = (int)kmem_alloc(kernel_map, round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
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
	if (base >= MAXBSIZE) {
		/* don't want to alloc more physical mem than needed */
		base = MAXBSIZE;
		residual = 0;
	}
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
	 * Initialize timeouts
	 */
	timeout_init();

	printf("avail mem = 0x%x\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

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

#ifdef PGINPROF
/*
 * Return the difference (in microseconds)
 * between the  current time and a previous
 * time as represented  by the arguments.
 * If there is a pending clock interrupt
 * which has not been serviced due to high
 * ipl, return error code.
 */
/*ARGSUSED*/
vmtime(otime, olbolt, oicr)
	register int otime, olbolt, oicr;
{

	return (((time.tv_sec-otime)*60 + lbolt-olbolt)*16667);
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
	register int *regs;
	register struct sigframe *fp;
	struct sigacts *ps = p->p_sigacts;
	int oonstack;
	extern char sigcode[], esigcode[];

	regs = p->p_md.md_regs;
	oonstack = ps->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((ps->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (ps->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(ps->ps_sigstk.ss_sp +
		    ps->ps_sigstk.ss_size - sizeof(struct sigframe));
		ps->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct sigframe *)(regs[REG_SP]
				- sizeof(struct sigframe));
	}

	if ((unsigned)fp <= (unsigned)p->p_vmspace->vm_maxsaddr + MAXSSIZ - ctob(p->p_vmspace->vm_ssize)) 
		(void)grow(p, (unsigned)fp);

	if (useracc((caddr_t)fp, sizeof (struct sigframe), B_WRITE) == 0) {
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
	 * Build the argument list for the signal handler.
	 */
	fp->sf_signum = sig;
	fp->sf_code = code;
	fp->sf_scp = &fp->sf_sc;
	fp->sf_handler = catcher;

	/* save registers */
	bcopy (regs, fp->sf_scp->sc_reg, 8*sizeof(int));

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	fp->sf_sc.sc_onstack = oonstack;
	fp->sf_sc.sc_mask = mask;
	fp->sf_sc.sc_sp = regs[REG_SP];
	fp->sf_sc.sc_fp = regs[REG_FP];
	fp->sf_sc.sc_pc = regs[REG_PC];
	fp->sf_sc.sc_ps = regs[REG_PSR];
	fp->sf_sc.sc_sb = regs[REG_SB];
	regs[REG_SP] = (int)fp;
	regs[REG_PC] = (int)(((char *)PS_STRINGS) - (esigcode - sigcode));
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
int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register struct sigframe *fp;
	register int *regs = p->p_md.md_regs;
	fp = (struct sigframe *) regs[REG_SP] ;

	if (useracc((caddr_t)fp, sizeof (*fp), 0) == 0)
		return(EINVAL);

	/* restore registers */
	bcopy (fp->sf_scp->sc_reg, regs, 8*sizeof(int));

	scp = fp->sf_scp;
	if (useracc((caddr_t)scp, sizeof (*scp), 0) == 0)
		return(EINVAL);
#ifdef notyet
	if ((scp->sc_ps & PSL_MBZ) != 0 || (scp->sc_ps & PSL_MBO) != PSL_MBO) {
		return(EINVAL);
	}
#endif
	if (scp->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask &~
	    (sigmask(SIGKILL)|sigmask(SIGCONT)|sigmask(SIGSTOP));
	regs[REG_FP] = scp->sc_fp;
	regs[REG_SP] = scp->sc_sp;
	regs[REG_PC] = scp->sc_pc;
	regs[REG_PSR] = scp->sc_ps;
	regs[REG_SB] = scp->sc_sb;
	return(EJUSTRETURN);
}

int waittime = -1;
struct pcb dumppcb;

void
boot(howto)
	int howto;
{
	register int devtype;		/* r10 == major of root dev */
	extern const char *panicstr;
	extern int cold;
	int nomsg = 1;

	if(cold) {
		printf("cold boot: hit reset please");
		for(;;);
	}
	boothowto = howto;
	if ((howto&RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* defeat against panic on sync   XXX */
		if (curproc == NULL)
			curproc = proc0;

		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now. (non panic!)
		 */
		if (panicstr == 0)
			resettodr();

		DELAY(10000);		/* wait for printf to finish */
	}
	splhigh();
	devtype = major(rootdev);

	if (howto&RB_HALT) {
		printf ("\nThe operating system has halted.\n\n");
		cpu_reset();
		for(;;) ;
		/*NOTREACHED*/
	} else {
		if (howto & RB_DUMP) {
#if STACK_DUMP
		  	/* dump the stack! */
		        { int *fp = (int *)_get_fp();
		            int i=0;
		            while ((u_int)fp < (u_int)UPT_MIN_ADDRESS-40) {
		              printf ("0x%x (@0x%x), ", fp[1], fp);
		              fp = (int *)fp[0];
		              if (++i == 3) { printf ("\n"); i=0; }
		            }
		        }
#endif
			savectx(&dumppcb, 0);
			dumppcb.pcb_ptb = _get_ptb0();
			dumpsys();
		}
		doshutdownhooks();
	}

	printf("rebooting ...");
	reboot_cpu();
	for(;;) ;
	/*NOTREACHED*/
}

void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();

	*tvp = time;
	tvp->tv_usec += tick;
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	splx(s);
}

/*
 * Strange exec values!  (Do we want to support a minix a.out header?)
 */
int
cpu_exec_aout_makecmds() 
{
  return ENOEXEC;
};

/*
 * Clear registers on exec
 */
void
setregs(p, entry, stack, retval)
	struct proc *p;
	struct exec_package *entry;
	u_long stack;
	register_t *retval;
{
	struct on_stack *r = (struct on_stack *)p->p_md.md_regs;
	int i;

/* printf ("Setregs: entry = %x, stack = %x, (usp = %x)\n", entry, stack,
		r->pcb_usp);  */

	/* Start fp at stack also! */
	r->pcb_usp = stack;
	r->pcb_fp = stack;
	r->pcb_pc = entry->ep_entry;
	r->pcb_psr = PSL_USERSET;
	r->pcb_reg[0] = (int)PS_STRINGS;
	for (i=1; i<8; i++) r->pcb_reg[i] = 0;

	p->p_addr->u_pcb.pcb_flags = 0;
}


extern struct pte	*CMAP1, *CMAP2;
extern caddr_t		CADDR1, CADDR2;
/*
 * zero out physical memory
 * specified in relocation units (NBPG bytes)
 */
clearseg(n)
{
	/* map page n in to virtual address CADDR2 */
	*(int *)CMAP2 = PG_V | PG_KW | ctob(n);
	tlbflush();
	bzero(CADDR2,NBPG);
	*(int *) CADDR2 = 0;
}

/*
 * copy a page of physical memory
 * specified in relocation units (NBPG bytes)
 */
copyseg(frm, n)
{
	/* map page n in to virtual address CADDR2 */
	*(int *)CMAP2 = PG_V | PG_KW | ctob(n);
	tlbflush();
	bcopy((void *)frm, (void *)CADDR2, NBPG);
}

/*
 * copy a page of physical memory
 * specified in relocation units (NBPG bytes)
 */
physcopyseg(frm, to)
{
	/* map page frm in to virtual address CADDR1 */
	*(int *)CMAP1 = PG_V | PG_KW | ctob(frm);
	/* map page to in to virtual address CADDR2 */
	*(int *)CMAP2 = PG_V | PG_KW | ctob(to);
	tlbflush();
	bcopy(CADDR1, CADDR2, NBPG);
}

/*
 * insert an element into a queue 
 */
#undef insque
_insque(element, head)
	register struct prochd *element, *head;
{
	element->ph_link = head->ph_link;
	head->ph_link = (struct proc *)element;
	element->ph_rlink = (struct proc *)head;
	((struct prochd *)(element->ph_link))->ph_rlink=(struct proc *)element;
}

/*
 * remove an element from a queue
 */
#undef remque
_remque(element)
	register struct prochd *element;
{
	((struct prochd *)(element->ph_link))->ph_rlink = element->ph_rlink;
	((struct prochd *)(element->ph_rlink))->ph_link = element->ph_link;
	element->ph_rlink = (struct proc *)0;
}

vmunaccess() {printf ("vmunaccess!\n");}

/*
 * Below written in C to allow access to debugging code
 */
copyinstr(fromaddr, toaddr, maxlength, lencopied) size_t *lencopied, maxlength;
	void *toaddr, *fromaddr; {
	int c,tally;

	tally = 0;
	while (maxlength--) {
		c = fubyte(fromaddr++);
		if (c == -1) {
			if(lencopied) *lencopied = tally;
			return(EFAULT);
		}
		tally++;
		*(char *)toaddr++ = (char) c;
		if (c == 0){
			if(lencopied) *lencopied = (u_int)tally;
			return(0);
		}
	}
	if(lencopied) *lencopied = (u_int)tally;
	return(ENAMETOOLONG);
}

copyoutstr(fromaddr, toaddr, maxlength, lencopied) size_t *lencopied, maxlength;
	void *fromaddr, *toaddr; {
	int c;
	int tally;

	tally = 0;
	while (maxlength--) {
		c = subyte(toaddr++, *(char *)fromaddr);
		if (c == -1) return(EFAULT);
		tally++;
		if (*(char *)fromaddr++ == 0){
			if(lencopied) *lencopied = tally;
			return(0);
		}
	}
	if(lencopied) *lencopied = tally;
	return(ENAMETOOLONG);
}

copystr(fromaddr, toaddr, maxlength, lencopied) size_t *lencopied, maxlength;
	void *fromaddr, *toaddr; {
	u_int tally;

	tally = 0;
	while (maxlength--) {
		*(u_char *)toaddr = *(u_char *)fromaddr++;
		tally++;
		if (*(u_char *)toaddr++ == 0) {
			if(lencopied) *lencopied = tally;
			return(0);
		}
	}
	if(lencopied) *lencopied = tally;
	return(ENAMETOOLONG);
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

	bytes = mem_size;
	maddr = 0;
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;
	for (i = 0; i < bytes; i += n) {
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

/* Stub function for reboot_cpu. */

void reboot_cpu()
{
  extern void low_level_reboot();
  
  /* Point Low MEMORY to Kernel Memory! */
  *((int *)PTD) =  low_mem_map+3; /* PTD[pdei(KERNBASE)]; */
  low_level_reboot();

}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sysarch_args /* {
		syscallarg(int) op;
		syscallarg(char *) parms;
	} */ *uap = v;

	return ENOSYS;
}

/*
 * consinit:
 * initialize the system console.
 * XXX - shouldn't deal with this cons_initted thing, but then,
 * it shouldn't be called from init386 either.
 */
static int cons_initted;

void
consinit()
{
	if (!cons_initted) {
		cninit();
		cons_initted = 1;
	}
}

/* DEBUG routine */

void dump_qs()
{  int ix;
   struct proc *ptr;

   for (ix=0; ix<NQS; ix++) 
     if (qs[ix].ph_link != qs[ix].ph_rlink)
       {
         ptr = qs[ix].ph_link;
	 do {
	   printf ("qs[%d]: 0x%x 0x%x\n", ix, ptr->p_forw, ptr->p_back);
	   ptr = ptr->p_forw;
	 } while (ptr != (struct proc *)0 && ptr != qs[ix].ph_link);
       }
   panic("nil P_BACK");
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
