/* $OpenBSD: machdep.c,v 1.61 2002/09/21 13:42:43 hugh Exp $ */
/* $NetBSD: machdep.c,v 1.108 2000/09/13 15:00:23 thorpej Exp $	 */

/*
 * Copyright (c) 2002, Hugh Graham.
 * Copyright (c) 2002, Miodrag Vallat.
 * Copyright (c) 1994, 1996, 1998 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 * 
 * Changed for the VAX port (and for readability) /IC
 * 
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
 *
 * This code is derived from software contributed to Berkeley by the Systems
 * Programming Group of the University of Utah Computer Science Department.
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
 * from: Utah Hdr: machdep.c 1.63 91/04/24
 * 
 * @(#)machdep.c	7.16 (Berkeley) 6/3/91
 */

#include <sys/signal.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/msgbuf.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/timeout.h>
#include <sys/device.h>
#include <sys/exec.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <net/netisr.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/ip_var.h>
#endif
#ifdef NETATALK
#include <netatalk/at_extern.h>
#endif
#ifdef NS
#include <netns/ns_var.h>
#endif
#include "ppp.h"	/* For NPPP */
#include "bridge.h"	/* For NBRIDGE */
#if NPPP > 0
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#endif

#include <machine/sid.h>
#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/cpu.h>
#include <machine/macros.h>
#include <machine/nexus.h>
#include <machine/trap.h>
#include <machine/reg.h>
#include <machine/db_machdep.h>
#include <vax/vax/gencons.h>

#ifdef DDB
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif
#include <vax/vax/db_disasm.h>

#include "smg.h"

caddr_t allocsys(caddr_t);

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef	NBUF
int nbuf = NBUF;
#else
int nbuf = 0;
#endif
#ifdef	BUFPAGES
int bufpages = BUFPAGES;
#else
int bufpages = 0;
#endif
int bufcachepercent = BUFCACHEPERCENT;

extern int *chrtoblktbl;
extern int virtual_avail, virtual_end;
/*
 * We do these external declarations here, maybe they should be done
 * somewhere else...
 */
int		want_resched;
char		machine[] = MACHINE;		/* from <machine/param.h> */
char		machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */
char		cpu_model[100];
int		physmem;
int		dumpsize = 0;
int		cold = 1; /* coldstart */
struct cpmbx	*cpmbx;

/*
 * XXX some storage space must be allocated statically because of
 * early console init
 */
#define	IOMAPSZ	100
char extiospace[EXTENT_FIXED_STORAGE_SIZE(IOMAPSZ)];

struct extent *extio;
extern vaddr_t iospace;

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

#ifdef DEBUG
int iospace_inited = 0;
#endif

void
cpu_startup()
{
	caddr_t		v;
	int		base, residual, i, sz;
	vm_offset_t	minaddr, maxaddr;
	vm_size_t	size;
	extern unsigned int avail_end;

	/*
	 * Initialize error message buffer.
	 */
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 * Also call CPU init on systems that need that.
	 */
	printf("%s%s [%08X %08X]\n", version, cpu_model, vax_cpudata, vax_siedata);
        if (dep_call->cpu_conf)
                (*dep_call->cpu_conf)();

	printf("total memory = %d\n", avail_end);
	physmem = btoc(avail_end);
	panicstr = NULL;
	mtpr(AST_NO, PR_ASTLVL);
	spl0();

	dumpsize = physmem + 1;

	/*
	 * Find out how much space we need, allocate it, and then give
	 * everything true virtual addresses.
	 */

	sz = (int) allocsys((caddr_t)0);
	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (((unsigned long)allocsys(v) - (unsigned long)v) != sz)
		panic("startup: table size inconsistency");
	/*
	 * Now allocate buffers proper.	 They are different than the above in
	 * that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;		/* # bytes for buffers */

	/* allocate VM for buffers... area is not managed by VM system */
	if (uvm_map(kernel_map, (vm_offset_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
			UVM_ADV_NORMAL, 0)))
		panic("cpu_startup: cannot allocate VM for buffers");

	minaddr = (vm_offset_t) buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	/* now allocate RAM for buffers */
	for (i = 0; i < nbuf; i++) {
		vm_offset_t curbuf;
		vm_size_t curbufsize;
		struct vm_page *pg;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.	The rest get (base) physical pages.
		 * 
		 * The rest of each buffer occupies virtual space, but has no
		 * physical memory allocated for it.
		 */
		curbuf = (vm_offset_t) buffers + i * MAXBSIZE;
		curbufsize = PAGE_SIZE * (i < residual ? base + 1 : base);
		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: "
				    "not enough RAM for buffer cache");
			pmap_enter(kernel_map->pmap, curbuf,
			    VM_PAGE_TO_PHYS(pg), VM_PROT_READ|VM_PROT_WRITE,
			    VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(kernel_map->pmap);

	/*
	 * Allocate a submap for exec arguments.  This map effectively limits
	 * the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				 16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio.  This map effectively limits the
	 * number of processes doing physio at any one time.
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

	printf("avail memory = %ld\n", ptoa(uvmexp.free));
	printf("using %d buffers containing %d bytes of memory\n", nbuf, bufpages * PAGE_SIZE);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */

	bufinit();
#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif

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
}

long	dumplo = 0;
long	dumpmag = 0x8fca0101;

void
cpu_dumpconf()
{
	int		nblks;

	/*
	 * XXX include the final RAM page which is not included in physmem.
	 */
	dumpsize = physmem + 1;
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize) (dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(dumpsize));
	}
	/*
	 * Don't dump on the first block in case the dump
	 * device includes a disk label.
	 */
	if (dumplo < btodb(PAGE_SIZE))
		dumplo = btodb(PAGE_SIZE);
}

int
cpu_sysctl(a, b, c, d, e, f, g)
	int	*a;
	u_int	b;
	void	*c, *e;
	size_t	*d, f;
	struct	proc *g;
{
	return (EOPNOTSUPP);
}

void
setstatclockrate(hzrate)
	int hzrate;
{
	panic("setstatclockrate");
}

void
consinit()
{
	/*
	 * Init I/O memory resource map. Must be done before cninit()
	 * is called; we may want to use iospace in the console routines.
	 *
	 * XXX console code uses the first page at iospace, so do not make
	 * the extent start at iospace.
	 */
	extio = extent_create("extio",
	    (u_long)iospace + VAX_NBPG, (u_long)iospace + IOSPSZ * VAX_NBPG,
	    M_DEVBUF, extiospace, sizeof(extiospace), EX_NOWAIT);
#ifdef DEBUG
	iospace_inited = 1;
#endif
	cninit();
#ifdef DDB
	ddb_init();
#ifdef DEBUG
	if (sizeof(struct user) > REDZONEADDR)
		panic("struct user inside red zone");
#endif
#endif
}

int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args { 
		syscallarg(struct sigcontext *) sigcntxp;
	} *uap = v;
	struct trapframe *scf;
	struct sigcontext *cntx;

	scf = p->p_addr->u_pcb.framep;
	cntx = SCARG(uap, sigcntxp);

	if (uvm_useracc((caddr_t)cntx, sizeof (*cntx), B_READ) == 0)
		return EINVAL;
	/* Compatibility mode? */
	if ((cntx->sc_ps & (PSL_IPL | PSL_IS)) ||
	    ((cntx->sc_ps & (PSL_U | PSL_PREVU)) != (PSL_U | PSL_PREVU)) ||
	    (cntx->sc_ps & PSL_CM)) {
		return (EINVAL);
	}
	if (cntx->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	/* Restore signal mask. */
    p->p_sigmask = cntx->sc_mask & ~sigcantmask;

	scf->fp = cntx->sc_fp;
	scf->ap = cntx->sc_ap;
	scf->pc = cntx->sc_pc;
	scf->sp = cntx->sc_sp;
	scf->psl = cntx->sc_ps;
	return (EJUSTRETURN);
}

struct trampframe {
	unsigned	sig;	/* Signal number */
	unsigned	code;	/* Info code */
	unsigned	scp;	/* Pointer to struct sigcontext */
	unsigned	r0, r1, r2, r3, r4, r5; /* Registers saved when
						 * interrupt */
	unsigned	pc;	/* Address of signal handler */
	unsigned	arg;	/* Pointer to first (and only) sigreturn
				 * argument */
};

/*
 * XXX no siginfo implementation!!!!
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t		catcher;
	int		sig, mask;
	u_long		code;
	int 	type;
	union sigval 	val;
{
	struct	proc	*p = curproc;
	struct	sigacts *psp = p->p_sigacts;
	struct	trapframe *syscf;
	struct	sigcontext *sigctx, gsigctx;
	struct	trampframe *trampf, gtrampf;
	unsigned	cursp;
	int	onstack;

#if 0
printf("sendsig: signal %x  catcher %x\n", sig, catcher);
#endif
	syscf = p->p_addr->u_pcb.framep;

	onstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/* Allocate space for the signal handler context. */
	if (onstack)
		cursp = ((int)psp->ps_sigstk.ss_sp + psp->ps_sigstk.ss_size);
	else
		cursp = syscf->sp;

	/* Set up positions for structs on stack */
	sigctx = (struct sigcontext *) (cursp - sizeof(struct sigcontext));
	trampf = (struct trampframe *) ((unsigned)sigctx -
	    sizeof(struct trampframe));

	/*
	 * Place sp at the beginning of trampf; this ensures that possible
	 * further calls to sendsig won't overwrite this struct
	 * trampframe/struct sigcontext pair with their own.
	 */
	cursp = (unsigned) trampf;

	gtrampf.arg = (int) sigctx;
	gtrampf.pc = (unsigned) catcher;
	gtrampf.scp = (int) sigctx;
	gtrampf.code = code;
	gtrampf.sig = sig;

	gsigctx.sc_pc = syscf->pc;
	gsigctx.sc_ps = syscf->psl;
	gsigctx.sc_ap = syscf->ap;
	gsigctx.sc_fp = syscf->fp; 
	gsigctx.sc_sp = syscf->sp; 
	gsigctx.sc_onstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	gsigctx.sc_mask = mask;

#if defined(COMPAT_13) || defined(COMPAT_ULTRIX)
	native_sigset_to_sigset13(mask, &gsigctx.__sc_mask13);
#endif

	if (copyout(&gtrampf, trampf, sizeof(gtrampf)) ||
	    copyout(&gsigctx, sigctx, sizeof(gsigctx)))
		sigexit(p, SIGILL);

	syscf->pc = p->p_sigcode;
	syscf->psl = PSL_U | PSL_PREVU;
	syscf->ap = (unsigned) sigctx-8;
	syscf->sp = cursp;

	if (onstack)
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
}

int	waittime = -1;
static	volatile int showto; /* Must be volatile to survive MM on -> MM off */

void
boot(howto)
	register int howto;
{
	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr will be out of
		 * synch; adjust it now.
		 */
		resettodr();
	}
	splhigh();		/* extreme priority */

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP)
		dumpsys();

	/* Run any shutdown hooks. */
	doshutdownhooks();

haltsys:
	if (howto & RB_HALT) {
		if (dep_call->cpu_halt)
			(*dep_call->cpu_halt) ();
		printf("halting (in tight loop); hit\n\t^P\n\tHALT\n\n");
		for (;;) ;
	} else {
		showto = howto;
#ifdef notyet
		/*
		 * If we are provided with a bootstring, parse it and send
		 * it to the boot program.
		 */
		if (b)
			while (*b) {
				showto |= (*b == 'a' ? RB_ASKBOOT : (*b == 'd' ?
				    RB_DEBUG : (*b == 's' ? RB_SINGLE : 0)));
				b++;
			}
#endif
		/*
		 * Now it's time to:
		 *  0. Save some registers that are needed in new world.
		 *  1. Change stack to somewhere that will survive MM off.
		 * (RPB page is good page to save things in).
		 *  2. Actually turn MM off.
		 *  3. Dump away memory to disk, if asked.
		 *  4. Reboot as asked.
		 * The RPB page is _always_ first page in memory, we can
		 * rely on that.
		 */
#ifdef notyet
		asm("	movl	sp, (0x80000200)
			movl	0x80000200, sp
			mfpr	$0x10, -(sp)	# PR_PCBB
			mfpr	$0x11, -(sp)	# PR_SCBB
			mfpr	$0xc, -(sp)	# PR_SBR
			mfpr	$0xd, -(sp)	# PR_SLR
			mtpr	$0, $0x38	# PR_MAPEN
		");
#endif

		if (dep_call->cpu_reboot)
			(*dep_call->cpu_reboot)(showto);

		/* cpus that don't handle reboots get the standard reboot. */
		while ((mfpr(PR_TXCS) & GC_RDY) == 0)
			;

		mtpr(GC_CONS|GC_BTFL, PR_TXDB);
	}
	asm("movl %0, r5":: "g" (showto)); /* How to boot */
	asm("movl %0, r11":: "r"(showto)); /* ??? */
	asm("halt");
	for (;;) ;
	/* NOTREACHED */
}

void
dumpsys()
{
	extern int msgbufmapped;

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	/*
	 * For dumps during autoconfiguration, if dump device has already
	 * configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);
	printf("dump ");
	switch ((*bdevsw[major(dumpdev)].d_dump) (dumpdev, 0, 0, 0)) {

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

	default:
		printf("succeeded\n");
		break;
	}
}

int
process_read_regs(p, regs)
	struct proc    *p;
	struct reg     *regs;
{
	struct trapframe *tf = p->p_addr->u_pcb.framep;

	bcopy(&tf->r0, &regs->r0, 12 * sizeof(int));
	regs->ap = tf->ap;
	regs->fp = tf->fp;
	regs->sp = tf->sp;
	regs->pc = tf->pc;
	regs->psl = tf->psl;
	return 0;
}

#ifdef PTRACE

int
process_write_regs(p, regs)
	struct proc    *p;
	struct reg     *regs;
{
	struct trapframe *tf = p->p_addr->u_pcb.framep;

	bcopy(&regs->r0, &tf->r0, 12 * sizeof(int));
	tf->ap = regs->ap;
	tf->fp = regs->fp;
	tf->sp = regs->sp;
	tf->pc = regs->pc;
	tf->psl = (regs->psl|PSL_U|PSL_PREVU) &
		~(PSL_MBZ|PSL_IS|PSL_IPL1F|PSL_CM);
	return 0;
}

int
process_set_pc(p, addr)
	struct	proc *p;
	caddr_t addr;
{
	struct	trapframe *tf;
	void	*ptr;

	if ((p->p_flag & P_INMEM) == 0)
		return (EIO);

	ptr = (char *) p->p_addr->u_pcb.framep;
	tf = ptr;

	tf->pc = (unsigned) addr;

	return (0);
}

int
process_sstep(p, sstep)
	struct proc    *p;
{
	void	       *ptr;
	struct trapframe *tf;

	if ((p->p_flag & P_INMEM) == 0)
		return (EIO);

	ptr = p->p_addr->u_pcb.framep;
	tf = ptr;

	if (sstep)
		tf->psl |= PSL_T;
	else
		tf->psl &= ~PSL_T;

	return (0);
}

#endif	/* PTRACE */

#undef PHYSMEMDEBUG
/*
 * Allocates a virtual range suitable for mapping in physical memory.
 * This differs from the bus_space routines in that it allocates on
 * physical page sizes instead of logical sizes. This implementation
 * uses resource maps when allocating space, which is allocated from 
 * the IOMAP submap. The implementation is similar to the uba resource
 * map handling. Size is given in pages.
 * If the page requested is bigger than a logical page, space is
 * allocated from the kernel map instead.
 *
 * It is known that the first page in the iospace area is unused; it may
 * be use by console device drivers (before the map system is inied).
 */
vaddr_t
vax_map_physmem(phys, size)
	paddr_t phys;
	int size;
{
	vaddr_t addr;
	int error;
	static int warned = 0;

#ifdef DEBUG
	if (!iospace_inited)
		panic("vax_map_physmem: called before rminit()?!?");
#endif
	if (size >= LTOHPN) {
		addr = uvm_km_valloc(kernel_map, size * VAX_NBPG);
		if (addr == 0)
			panic("vax_map_physmem: kernel map full");
	} else {
		error = extent_alloc(extio, size * VAX_NBPG, VAX_NBPG, 0,
		    EX_NOBOUNDARY, EX_NOWAIT | EX_MALLOCOK, (u_long *)&addr);
		if (error != 0) {
			if (warned++ == 0) /* Warn only once */
				printf("vax_map_physmem: iomap too small");
			return 0;
		}
	}
	ioaccess(addr, phys, size);
#ifdef PHYSMEMDEBUG
	printf("vax_map_physmem: alloc'ed %d pages for paddr %lx, at %lx\n",
	    size, phys, addr);
#endif
	return addr | (phys & VAX_PGOFSET);
}

/*
 * Unmaps the previous mapped (addr, size) pair.
 */
void
vax_unmap_physmem(addr, size)
	vaddr_t addr;
	int size;
{
#ifdef PHYSMEMDEBUG
	printf("vax_unmap_physmem: unmapping %d pages at addr %lx\n", 
	    size, addr);
#endif
	iounaccess(addr, size);
	if (size >= LTOHPN)
		uvm_km_free(kernel_map, addr, size * VAX_NBPG);
	else
		extent_free(extio, (u_long)addr & ~VAX_PGOFSET,
		    size * VAX_NBPG, EX_NOWAIT);
}

/*
 * Allocate space for system data structures.  We are given a starting
 * virtual address and we return a final virtual address; along the way we
 * set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want, allocate that
 * much and fill it with zeroes, and then call allocsys() again with the
 * correct base virtual address.
 */
#define VALLOC(name, type, num) v = (caddr_t)(((name) = (type *)v) + (num))

caddr_t
allocsys(v)
    register caddr_t v;
{

#ifdef SYSVSHM
    shminfo.shmmax = shmmaxpgs;
    shminfo.shmall = shmmaxpgs;
    shminfo.shmseg = shmseg;
    VALLOC(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
    VALLOC(sema, struct semid_ds, seminfo.semmni);
    VALLOC(sem, struct sem, seminfo.semmns);

    /* This is pretty disgusting! */
    VALLOC(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
    VALLOC(msgpool, char, msginfo.msgmax);
    VALLOC(msgmaps, struct msgmap, msginfo.msgseg);
    VALLOC(msghdrs, struct msg, msginfo.msgtql);
    VALLOC(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate.  We make sure we allocate
	 * at least 16 buffers.  	 
	 */
	if (bufpages == 0) {
	    if (physmem < btoc(2 * 1024 * 1024))
	        bufpages = physmem / 10;
	    else
		bufpages = (btoc(2 * 1024 * 1024) + physmem) *
		    bufcachepercent / 100;
	}
    if (nbuf == 0) 
        nbuf = bufpages < 16 ? 16 : bufpages;

    /* Restrict to at most 70% filled kvm */
    if (nbuf * MAXBSIZE >
        (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) * 7 / 10)
        nbuf = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
            MAXBSIZE * 7 / 10;

    /* More buffer pages than fits into the buffers is senseless.  */
    if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
        bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

    VALLOC(buf, struct buf, nbuf);
    return (v);
}

/*
 * The following is a very stripped-down db_disasm.c, with only the logic
 * to skip instructions.
 */

long skip_operand(long ib, int size);

static __inline__ u_int8_t
get_byte(ib)
	long    ib;
{
	return *((u_int8_t *)ib);
}

long
skip_opcode(ib)
	long    ib;
{
	u_int opc;
	int size;
	char *argp;	/* pointer into argument-list */

	opc = get_byte(ib++);
	if (opc >= 0xfd) {
		/* two byte op-code */
		opc = opc << 8;
		opc += get_byte(ib++);
		argp = vax_inst2[INDEX_OPCODE(opc)].argdesc;
	} else
		argp = vax_inst[opc].argdesc;

	if (argp == NULL)
		return ib;

	while (*argp) {
		switch (*argp) {

		case 'b':	/* branch displacement */
			switch (*(++argp)) {
			case 'b':
				ib++;
				break;
			case 'w':
				ib += 2;
				break;
			case 'l':
				ib += 4;
				break;
			}
			break;

		case 'a':	/* absolute adressing mode */
			/* FALLTHROUGH */
		default:
			switch (*(++argp)) {
			case 'b':	/* Byte */
				size = 1;
				break;
			case 'w':	/* Word */
				size = 2;
				break;
			case 'l':	/* Long-Word */
			case 'f':	/* F_Floating */
				size = 4;
				break;
			case 'q':	/* Quad-Word */
			case 'd':	/* D_Floating */
			case 'g':	/* G_Floating */
				size = 8;
				break;
			case 'o':	/* Octa-Word */
			case 'h':	/* H_Floating */
				size = 16;
				break;
			default:
				size = 0;
			}
			ib = skip_operand(ib, size);
		}

		if (!*argp || !*++argp)
			break;
		if (*argp++ != ',')
			break;
	}

	return ib;
}

long
skip_operand(ib, size)
	long    ib;
	int	size;
{
	int c = get_byte(ib++);

	switch (c >> 4) { /* mode */
	case 4:		/* indexed */
		ib = skip_operand(ib, 0);
		break;

	case 9:		/* autoincrement deferred */
		if (c == 0x9f) {	/* pc: immediate deferred */
			/*
			 * addresses are always longwords!
			 */
			ib += 4;
		}
		break;
	case 8:		/* autoincrement */
		if (c == 0x8f) {	/* pc: immediate ==> special syntax */
			ib += size;
		}
		break;

	case 11:	/* byte displacement deferred/ relative deferred  */
	case 10:	/* byte displacement / relative mode */
		ib++;
		break;

	case 13:		/* word displacement deferred */
	case 12:		/* word displacement */
		ib += 2;
		break;

	case 15:		/* long displacement referred */
	case 14:		/* long displacement */
		ib += 4;
		break;
	}

	return ib;
}

void
generic_halt()
{
	if (cpmbx->user_halt != UHALT_DEFAULT) {
		if (cpmbx->mbox_halt != 0)
			cpmbx->mbox_halt = 0;	/* let console override */
	} else if (cpmbx->mbox_halt != MHALT_HALT)
		cpmbx->mbox_halt = MHALT_HALT;	/* the os decides */

	asm("halt");
}

void
generic_reboot(int arg)
{
	if (cpmbx->user_halt != UHALT_DEFAULT) {
		if (cpmbx->mbox_halt != 0)
			cpmbx->mbox_halt = 0;
	} else if (cpmbx->mbox_halt != MHALT_REBOOT)
		cpmbx->mbox_halt = MHALT_REBOOT;

	asm("halt");
}
