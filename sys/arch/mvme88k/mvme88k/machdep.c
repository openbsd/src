/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
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
/*
 * HISTORY
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
#include <sys/errno.h>
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
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/locore.h>
#include <machine/board.h>
#include <machine/trap.h>
#include <machine/bug.h>

#include <dev/cons.h>

#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#define __IS_MACHDEP_C__
#include <assym.s>			  /* EF_EPSR, etc. */
#include <machine/m88100.h>  			/* DMT_VALID        */
#include <machine/m882xx.h>  			/* CMMU stuff       */
#if DDB
#  include <machine/db_machdep.h>
#endif /* DDB */

#if 0
#include <machine/m88100.h>  			/* DMT_VALID        */
#include <machine/m882xx.h>  			/* CMMU stuff       */
#include <vm/vm.h>
#include <vm/vm_kern.h>			  /* kernel_map       */
#include <sys/param.h>
#include <sys/msgbuf.h>
#include <sys/buf.h>
#include <machine/locore.h>		  /* USERMODE         */
/*
#include <machine/nvram.h>
*/
#include <sys/types.h>
#endif /* 0 */

static int waittime = -1;

static void level0_intr(int, unsigned *);
static void level1_intr(int, unsigned *);
static void level2_intr(int, unsigned *);
static void level3_intr(int, unsigned *);
static void level4_intr(int, unsigned *);
static void level5_intr(int, unsigned *);
static void level6_intr(int, unsigned *);
static void level7_intr(int, unsigned *);

unsigned char *ivec[] = {
	(unsigned char *)0xFFFE007,
	(unsigned char *)0xFFFE00B,
	(unsigned char *)0xFFFE00F,
	(unsigned char *)0xFFFE013,
	(unsigned char *)0xFFFE017,
	(unsigned char *)0xFFFE01B,
	(unsigned char *)0xFFFE01F,
};

static void (*int_handler[8])() =
{
    level0_intr,
    level1_intr,
    level2_intr,
    level3_intr,
    level4_intr,
    level5_intr,
    level6_intr,
    level7_intr,
};

unsigned char *int_mask_level = (unsigned char *)INT_MASK_LEVEL;
unsigned char *int_pri_level = (unsigned char *)INT_PRI_LEVEL;
unsigned char *iackaddr;

int physmem;		/* available physical memory, in pages */
int cold;
vm_offset_t avail_end, avail_start, avail_next;
int msgbufmapped = 0;
int foodebug = 0;
int longformat = 0;

extern char kstack[];	/* kernel stack - actually this is == UADDR */
extern char *cpu_string;
extern short exframesize[];

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
int *nofault;

caddr_t allocsys __P((caddr_t));
  
/*
 * Info for CTL_HW
 */
char	machine[] = "MVME187";		/* cpu "architecture" */
char	cpu_model[120];
extern	char version[];

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

#if defined (DDB)
        kdb_init();
        if (boothowto & RB_KDB)
                Debugger();
#endif
}

/*
 * Figure out how much real memory is available.
 * Start looking from the megabyte after the end of the kernel data,
 * until we find non-memory.
 */
vm_offset_t
size_memory(void)
{
    volatile unsigned int *look;
    unsigned int *max;
    extern char end[];
    #define PATTERN   0x5a5a5a5a
    #define STRIDE    (4*1024) 	/* 4k at a time */
    #define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))

    /*
     * count it up.
     */
    max = (void*)MAXPHYSMEM;
    for (look = (void*)Roundup(end, STRIDE); look < max;
			look = (int*)((unsigned)look + STRIDE)) {
	unsigned save;

	/* if can't access, we've reached the end */
	if (foodebug)
	printf("%x\n", look);
	if (badwordaddr((vm_offset_t)look)) {
		printf("%x\n", look);
		look = (int *)((int)look - STRIDE);
		break;
	}

#if 1
	/*
	 * If we write a value, we expect to read the same value back.
	 * We'll do this twice, the 2nd time with the opposite bit
	 * pattern from the first, to make sure we check all bits.
	 */
	save = *look;
	if (*look = PATTERN, *look != PATTERN)
		break;
	if (*look = ~PATTERN, *look != ~PATTERN)
		break;
	*look = save;
#endif
    }

    physmem = btoc(trunc_page((unsigned)look));	/* in pages */
    return(trunc_page((unsigned)look));
}

void
identifycpu()
{
	/* XXX -take this one out. It can be done in m187_bootstrap() */
	strcpy(cpu_model, "Motorola M88K");
	printf("Model: %s\n", cpu_model);
}

/* The following two functions assume UPAGES == 3 */
#if	UPAGES != 3
#error "UPAGES changed?"
#endif

void
save_u_area(struct proc *p, vm_offset_t va)
{
    p->p_md.md_upte[0] = kvtopte(va)->bits;
    p->p_md.md_upte[1] = kvtopte(va + NBPG)->bits;
    p->p_md.md_upte[2] = kvtopte(va + NBPG + NBPG)->bits;
}

void
load_u_area(struct proc *p)
{
    pte_template_t *t;

    t = kvtopte(UADDR);
    t->bits = p->p_md.md_upte[0];
    t = kvtopte(UADDR + NBPG);
    t->bits = p->p_md.md_upte[1];
    t = kvtopte(UADDR + NBPG + NBPG);
    t->bits = p->p_md.md_upte[2];
    cmmu_flush_tlb(1, UADDR, 3 * NBPG);
}


void
cpu_startup()
{
    caddr_t v;
    int sz, i;
    vm_size_t size;    
    int base, residual;
    vm_offset_t minaddr, maxaddr, uarea_pages;
    extern vm_offset_t miniroot;

    /*
     * Initialize error message buffer (at end of core).
     * avail_end was pre-decremented in m1x7_init.
     */
     for (i = 0; i < btoc(sizeof(struct msgbuf)); i++)
         pmap_enter(kernel_pmap, (vm_offset_t)msgbufp,
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
     * Grab UADDR virtual address
     */
	
    uarea_pages = UADDR;

    vm_map_find(kernel_map, vm_object_allocate(PAGE_SIZE * UPAGES), 0,
	(vm_offset_t *)&uarea_pages, PAGE_SIZE * UPAGES, TRUE);

    if (uarea_pages != UADDR) {
	printf("uarea_pages %x: UADDR not free\n", uarea_pages);
        panic("bad UADDR");
    }
    /*
     * Now allocate buffers proper.  They are different than the above
     * in that they usually occupy more virtual memory than physical.
     */
    size = MAXBSIZE * nbuf;
    buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
			       &maxaddr, size, TRUE);
    minaddr = (vm_offset_t)buffers;
    if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
		    (vm_offset_t *)&minaddr, size, FALSE) != KERN_SUCCESS)
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
     * Allocate a map for IO.
     */
    phys_map = vm_map_create(kernel_pmap, IO_SPACE_START,
			     IO_SPACE_END, TRUE);
    if (phys_map == NULL)
	panic("cpu_startup: unable to create physmap");

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

    printf("avail mem = %d\n", ptoa(cnt.v_free_count));
    printf("using %d buffers containing %d bytes of memory\n",
	   nbuf, bufpages * CLBYTES);

    mfs_initminiroot(miniroot);
    /*
     * Set up buffers, so they can be used to read disk labels.
     */
    bufinit();

    /*
     * Configure the system.
     */
    nofault = NULL;
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
    configure();

    dumpconf();
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
	 * Determine how many buffers to allocate (enough to
	 * hold 5% of total physical memory, but at least 16).
	 * Allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
	    if (physmem < btoc(2 * 1024 * 1024))
		bufpages = (physmem / 10) / CLSIZE;
	    else
		bufpages = (physmem / 20) / CLSIZE;
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
 * Set registers on exec.
 * Clear all except sp and pc.
 */
/* ARGSUSED */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	int retval[2];
{
	register struct trapframe *tf = p->p_md.md_tf;
	register int psr;

	/*
	 * The syscall will ``return'' to snip; set it.
	 * Set the rest of the registers to 0 except for r31 (stack pointer,
	 * built in exec()) and psr (supervisor bit).
	 */
	psr = tf->epsr & PSR_SUPERVISOR_MODE_BIT;
#if 0
	/*
	I don't think I need to mess with fpstate on 88k because
	we make sure the floating point pipeline is drained in
	locore.s. Should check on this later. Nivas.
	*/

	if ((fs = p->p_md.md_fpstate) != NULL) {
		/*
		 * We hold an FPU state.  If we own *the* FPU chip state
		 * we must get rid of it, and the only way to do that is
		 * to save it.  In any case, get rid of our FPU state.
		 */
		if (p == fpproc) {
			savefpstate(fs);
			fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC);
		p->p_md.md_fpstate = NULL;
	}
#endif /* 0 */
	bzero((caddr_t)tf, sizeof *tf);
	tf->epsr = psr;
	tf->snip = pack->ep_entry & ~3;
	tf->sfip = tf->snip + 4;
	tf->r[31] = stack;
	retval[1] = 0;
}

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signum
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int	sf_signo;		/* signo for handler */
	int	sf_code;		/* additional info for handler */
	struct	sigcontext *sf_scp;	/* context ptr for handler */
	sig_t	sf_handler;		/* handler addr for u_sigc */
	struct	sigcontext sf_sc;	/* actual context */
};

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	unsigned long code;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	register struct sigacts *psp = p->p_sigacts;
	struct sigframe *fp;
	int oonstack, fsize;
	struct sigframe sf;
	int addr;
	extern char sigcode[], esigcode[];

#define szsigcode (esigcode - sigcode)

	tf = p->p_md.md_tf;
	oonstack = psp->ps_sigstk.ss_flags & SA_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in data space, the
	 * call to grow() is a nop, and the copyout()
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct sigframe);
	if ((psp->ps_flags & SAS_ALTSTACK) &&
	    (psp->ps_sigstk.ss_flags & SA_ONSTACK) == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size - fsize);
		psp->ps_sigstk.ss_flags |= SA_ONSTACK;
	} else
		fp = (struct sigframe *)(tf->r[31] - fsize);
	if ((unsigned)fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize)) 
		(void)grow(p, (unsigned)fp);
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %x usp %x scp %x\n",
		       p->p_pid, sig, &oonstack, fp, &fp->sf_sc);
#endif
	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_signo = sig;
	sf.sf_code = code;
	sf.sf_scp = &fp->sf_sc;
	sf.sf_sc.sc_onstack = oonstack;
	sf.sf_sc.sc_mask = mask;
	/*
	 * Copy the whole user context into signal context that we
	 * are building.
	 */

	bcopy((caddr_t)tf->r, (caddr_t)sf.sf_sc.sc_regs,
		 sizeof(sf.sf_sc.sc_regs));
	sf.sf_sc.sc_xip = tf->sxip;
	sf.sf_sc.sc_nip = tf->snip;
	sf.sf_sc.sc_fip = tf->sfip;
	sf.sf_sc.sc_ps = tf->epsr;
	sf.sf_sc.sc_sp  = tf->r[31];
	sf.sf_sc.sc_fpsr = tf->fpsr;
	sf.sf_sc.sc_fpcr = tf->fpcr;
	sf.sf_sc.sc_ssbr = tf->ssbr;
	sf.sf_sc.sc_dmt0 = tf->dmt0;
	sf.sf_sc.sc_dmd0 = tf->dmd0;
	sf.sf_sc.sc_dma0 = tf->dma0;
	sf.sf_sc.sc_dmt1 = tf->dmt1;
	sf.sf_sc.sc_dmd1 = tf->dmd1;
	sf.sf_sc.sc_dma1 = tf->dma1;
	sf.sf_sc.sc_dmt2 = tf->dmt2;
	sf.sf_sc.sc_dmd2 = tf->dmd2;
	sf.sf_sc.sc_dma2 = tf->dma2;
	sf.sf_sc.sc_fpecr = tf->fpecr;
	sf.sf_sc.sc_fphs1 = tf->fphs1;
	sf.sf_sc.sc_fpls1 = tf->fpls1;
	sf.sf_sc.sc_fphs2 = tf->fphs2;
	sf.sf_sc.sc_fpls2 = tf->fpls2;
	sf.sf_sc.sc_fppt = tf->fppt;
	sf.sf_sc.sc_fprh = tf->fprh;
	sf.sf_sc.sc_fprl = tf->fprl;
	sf.sf_sc.sc_fpit = tf->fpit;
	if (copyout((caddr_t)&sf, (caddr_t)&fp, sizeof sf)) {
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
	 * Signal trampoline code is at base of user stack.
	 */
	addr = (int)PS_STRINGS - szsigcode;
	tf->snip = addr & ~3;
	tf->sfip = tf->snip + 4;
	tf->r[31] = (unsigned)fp;
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
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
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	int *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register struct trapframe *tf;
	struct sigcontext ksc;
	int error;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if ((int)scp & 3 || useracc((caddr_t)scp, sizeof *scp, B_WRITE) == 0)
		return (EINVAL);
	tf = p->p_md.md_tf;
	/*
	 * xip, nip and fip must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((scp->sc_xip | scp->sc_nip | scp->sc_fip) & 3) != 0)
		return (EINVAL);
	bcopy((caddr_t)scp->sc_regs, (caddr_t)tf->r,
		 sizeof(scp->sc_regs));
	tf->sxip = scp->sc_xip;
	tf->snip = scp->sc_nip;
	tf->sfip = scp->sc_fip;
	tf->epsr = scp->sc_ps;
	tf->r[31] = scp->sc_sp;
	tf->fpsr = scp->sc_fpsr;
	tf->fpcr = scp->sc_fpcr;
	tf->ssbr = scp->sc_ssbr;
	tf->dmt0 = scp->sc_dmt0;
	tf->dmd0 = scp->sc_dmd0;
	tf->dma0 = scp->sc_dma0;
	tf->dmt1 = scp->sc_dmt1;
	tf->dmd1 = scp->sc_dmd1;
	tf->dma1 = scp->sc_dma1;
	tf->dmt2 = scp->sc_dmt2;
	tf->dmd2 = scp->sc_dmd2;
	tf->dma2 = scp->sc_dma2;
	tf->fpecr = scp->sc_fpecr;
	tf->fphs1 = scp->sc_fphs1;
	tf->fpls1 = scp->sc_fpls1;
	tf->fphs2 = scp->sc_fphs2;
	tf->fpls2 = scp->sc_fpls2;
	tf->fppt = scp->sc_fppt;
	tf->fprh = scp->sc_fprh;
	tf->fprl = scp->sc_fprl;
	tf->fpit = scp->sc_fpit;

	tf->epsr = scp->sc_ps;

	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SA_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;
	p->p_sigmask = scp->sc_mask &~ sigcantmask;
	return (EJUSTRETURN);
}

void
bootsync(void)
{
	if (waittime < 0) {
		register struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		(void) spl0();
		printf("syncing disks... ");
		/*
		 * Release vnodes held by texts before sync.
		 */
		if (panicstr == 0)
			vnode_pager_umount(NULL);
		sync(&proc0, (void *)NULL, (int *)NULL);

		for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if ((bp->b_flags & (B_BUSY|B_INVAL)) == B_BUSY)
					nbusy++;
			if (nbusy == 0)
				break;
			printf("%d ", nbusy);
			delay(40000 * iter);
		}
		if (nbusy)
			printf("giving up\n");
		else
			printf("done\n");
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
}

doboot()
{
	bugreturn();
}

void
boot(howto)
	register int howto;
{
	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx(curproc->p_addr, 0);

	boothowto = howto;
	if ((howto&RB_NOSYNC) == 0)
		bootsync();
	splhigh();			/* extreme priority */
	if (howto&RB_HALT) {
		printf("halted\n\n");
		bugreturn();
	} else {
		if (howto & RB_DUMP)
			dumpsys();
		doboot();
		/*NOTREACHED*/
	}
	/*NOTREACHED*/
}

unsigned	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

dumpconf()
{
	int nblks;

	dumpsize = physmem;
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(physmem));
	}
	/*
	 * Don't dump on the first CLBYTES (why CLBYTES?)
	 * in case the dump device includes a disk label.
	 */
	if (dumplo < btodb(CLBYTES))
		dumplo = btodb(CLBYTES);
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
	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo < 0)
		return;
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

	default:
		printf("succeeded\n");
		break;
	}
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by returning the current time
 * plus the amount of time since the last clock interrupt (clock.c:clkread).
 *
 * Check that this time is no less than any previously-reported time,
 * which could happen around the time of a clock adjustment.  Just for fun,
 * we guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();
	static struct timeval lasttime;

	*tvp = time;
	tvp->tv_usec += clkread();
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
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
unsigned vmtime(int otime, int olbolt, int oicr)
{
    return ((time.tv_sec-otime)*60 + lbolt-olbolt)*16667;
}
#endif /* PGINPROF */

badwordaddr(void *addr)
{
	return badaddr((vm_offset_t)addr, 4);
}

/* returns positive if memory is not there; */
unsigned check_memory(void *addr, unsigned flag)
{
  return badaddr((vm_offset_t)addr, 1);
}

void start_clock(void)
{
	printf("Start clock\n");
}

static void
level0_intr(int level, unsigned *frame)
{
	printf("Spurious interrupt\n");
}

static void
level1_intr(int level, unsigned *frame)
{
	register char vec;
	iackaddr = ivec[level];

	/* generate IACK and get the vector */
	asm volatile ("ld.b %0,%1" : "=r" (vec) : "" (iackaddr));
}
#if 0
static void
level1_intr(int level, unsigned *frame)
{
	register char vec;
	iackaddr = ivec[level];

	/* generate IACK and get the vector */
	asm volatile ("ld.b %0,%1" : "=r" (vec) : "" (iackaddr));
}
#endif

static void
level2_intr(int level, unsigned *frame)
{
	iackaddr = ivec[level];
}

static void
level3_intr(int level, unsigned *frame)
{
	iackaddr = ivec[level];
}

static void
level4_intr(int level, unsigned *frame)
{
	iackaddr = ivec[level];
}

static void
level5_intr(int level, unsigned *frame)
{
	iackaddr = ivec[level];
}

static void
level6_intr(int level, unsigned *frame)
{
	register char vec;
	struct clockframe clkframe;
	iackaddr = ivec[level];

	/* generate IACK and get the vector */
	asm volatile("ld.b %0,%1" : "=r" (vec) : "" (iackaddr));
	switch (vec){
	case TIMER1IRQ:
		break;
	case TIMER2IRQ:
		/*
		 * build clockframe and pass to the clock
		 * interrupt handler
		 */
		clkframe.pc  = frame[EF_SXIP] & ~3;
		clkframe.sr  = frame[EF_EPSR];
		clkframe.ipl = frame[EF_MASK];
		clockintr(&clkframe);
		break;
	}
}

static void
level7_intr(int level, unsigned *frame)
{
	iackaddr = ivec[level];
}

/*
 *	Device interrupt handler
 *
 *      when we enter, interrupts are disabled;
 *      when we leave, they should be disabled,
 *      but they need not be enabled throughout
 *      the routine.
 */

void
ext_int(unsigned vec, unsigned *eframe)
{
    register unsigned char mask, level;
    register int s;		/* XXX */

    asm volatile ("ld.b	%0,%1" : "=r" (mask) : "" (int_mask_level));
    asm volatile ("ld.b	%0,%1" : "=r" (level) : "" (int_pri_level));

    /* get the mask and stash it away in the trap frame */
    eframe[EF_MASK] = mask;
    /* and block ints level or lower */
    spln((char)mask);
    enable_interrupt();
    (*int_handler[level])(level,eframe);
    /*
     * process any remaining data access exceptions before
     * returning to assembler
     */
    disable_interrupt();
    if (eframe[EF_DMT0] && DMT_VALID)
    {
	trap(T_DATAFLT, eframe);
	data_access_emulation(eframe);
    }
    mask = eframe[EF_MASK];
    asm volatile ("st.b	%0,%1" : "=r" (mask) : "" (int_mask_level));
}

/*
 * check a word wide address.
 * write < 0 -> check for write access.
 * otherwise read.
 */
int wprobe(void *addr, unsigned int write)
{
    /* XXX only checking reads */
    return badaddr((vm_offset_t)addr, sizeof(int));
}

cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;

#ifdef COMPAT_SUNOS
	extern sun_exec_aout_makecmds __P((struct proc *, struct exec_package *));
	if ((error = sun_exec_aout_makecmds(p, epp)) == 0)
		return 0;
#endif
	return error;
}

#if NOTYET
/*
 * nvram_read(BUF, ADDRESS, SIZE)
 * nvram_write(BUF, ADDRESS, SIZE)
 *
 * Read and write non-volatile RAM.
 * Only one byte from each word in the NVRAM area is accessable.
 * ADDRESS points to the virtual starting address, which is some address
 * after the nvram start (NVRAM_ADDR). SIZE refers to virtual size.
 */
void nvram_read(char *buf, vm_offset_t address, unsigned size)
{
    unsigned index = (unsigned)address - NVRAM_ADDR;
    unsigned char *source = (char*)(NVRAM_ADDR + index * 4);

    while (size-- > 0)
    {
        *buf++ = *source;
        source += 4; /* bump up to point to next readable byte */
    }
}

void nvram_write(char *buf, vm_offset_t address, unsigned size)
{
    unsigned index = (unsigned)address - NVRAM_ADDR;
    unsigned char *source = (char*)(NVRAM_ADDR + index * 4);

    while (size-- > 0)
    {
        *source = *buf++;
        source += 4; /* bump up to point to next readable byte */
    }
}
#endif /* NOTYET */

struct sysarch_args {
	int op;
	char *parms;
};

sysarch(p, uap, retval)
	struct proc *p;
	register struct sysarch_args *uap;
	int *retval;
{
	int error = 0;

	switch(uap->op) {
	default:
		error = EINVAL;
		break;
	}
	return(error);
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

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	default:
		return (EOPNOTSUPP);
	}
	/*NOTREACHED*/
}

/*
 * insert an element into a queue 
 */
#undef _insque
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
#undef _remque
_remque(element)
	register struct prochd *element;
{
	((struct prochd *)(element->ph_link))->ph_rlink = element->ph_rlink;
	((struct prochd *)(element->ph_rlink))->ph_link = element->ph_link;
	element->ph_rlink = (struct proc *)0;
}

#if 0
/*
 * Below written in C to allow access to debugging code
 */
copyinstr(fromaddr, toaddr, maxlength, lencopied) u_int *lencopied, maxlength;
	void *toaddr, *fromaddr;
{
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

copyoutstr(fromaddr, toaddr, maxlength, lencopied) u_int *lencopied, maxlength;
	void *fromaddr, *toaddr;
{
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

#endif /* 0 */

copystr(fromaddr, toaddr, maxlength, lencopied)
	u_int *lencopied, maxlength;
	void *fromaddr, *toaddr;
{
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

void
putchar(char c)
{
	bugoutchr(c);
}
/* dummys for now */

bugsyscall()
{
}

mmrw()
{
}

netintr()
{
}

MY_info(f, p, flags, s)
struct trapframe 	*f;
caddr_t 		p;
int 			flags;
char			*s;
{
	regdump(f);
	printf("proc %x flags %x type %s\n", p, flags, s);
}	

MY_info_done(f, flags)
struct trapframe	*f;
int			flags;
{
	regdump(f);
}	

regdump(struct trapframe *f)
{
#define R(i) f->r[i]
    printf("R00-05: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(0),R(1),R(2),R(3),R(4),R(5));
    printf("R06-11: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(6),R(7),R(8),R(9),R(10),R(11));
    printf("R12-17: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(12),R(13),R(14),R(15),R(16),R(17));
    printf("R18-23: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(18),R(19),R(20),R(21),R(22),R(23));
    printf("R24-29: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(24),R(25),R(26),R(27),R(28),R(29));
    printf("R30-31: 0x%08x  0x%08x\n",R(30),R(31));
    printf("sxip %x snip %x sfip %x\n", f->sxip, f->snip, f->sfip);
    if (f->vector == 0x3) { /* print dmt stuff for data access fault */
	printf("dmt0 %x dmd0 %x dma0 %x\n", f->dmt0, f->dmd0, f->dma0);
	printf("dmt1 %x dmd1 %x dma1 %x\n", f->dmt1, f->dmd1, f->dma1);
	printf("dmt2 %x dmd2 %x dma2 %x\n", f->dmt2, f->dmd2, f->dma2);
    }
	if (longformat) {
		printf("fpsr %x", f->fpsr);
		printf("fpcr %x", f->fpcr);
		printf("epsr %x", f->epsr);
		printf("ssbr %x\n", f->ssbr);
		printf("dmt0 %x", f->dmt0);
		printf("dmd0 %x", f->dmd0);
		printf("dma0 %x", f->dma0);
		printf("dmt1 %x", f->dmt1);
		printf("dmd1 %x", f->dmd1);
		printf("dma1 %x", f->dma1);
		printf("dmt2 %x", f->dmt2);
		printf("dmd2 %x", f->dmd2);
		printf("dma2 %x\n", f->dma2);
		printf("fpecr %x", f->fpecr);
		printf("fphs1 %x", f->fphs1);
		printf("fpls1 %x", f->fpls1);
		printf("fphs2 %x", f->fphs2);
		printf("fpls2 %x", f->fpls2);
		printf("fppt %x", f->fppt);
		printf("fprh %x", f->fprh);
		printf("fprl %x", f->fprl);
		printf("fpit %x\n", f->fpit);
		printf("vector %x", f->vector);
		printf("mask %x", f->mask);
		printf("mode %x", f->mode);
		printf("scratch1 %x", f->scratch1);
		printf("pad %x\n", f->pad);
	}
}

#if DDB
inline int
db_splhigh(void)
{
	return (db_spln(6));
}

inline int
db_splx(int s)
{
	return (db_spln(s));
}
#endif /* DDB */	
