/*	$OpenBSD: machdep.c,v 1.2 2004/01/29 13:21:10 mickey Exp $	*/
/*	$NetBSD: machdep.c,v 1.3 2003/05/07 22:58:18 fvdl Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/extent.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/syscallargs.h>

#ifdef SYSVMSG
#include <sys/msg.h>
#endif

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/cons.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_page.h>

#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/bootinfo.h>
#include <machine/fpu.h>
#include <machine/mtrr.h>
#include <machine/mpbiosvar.h>
#include <machine/reg.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>
#include <dev/ic/i8042reg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#include "isa.h"
#include "isadma.h"
#include "ksyms.h"

/* the following is used externally (sysctl_hw) */
char machine[] = "amd64";		/* cpu "architecture" */
char machine_arch[] = "x86_64";		/* machine == machine_arch */

char bootinfo[BOOTINFO_MAXSIZE];

struct bi_devmatch *x86_64_alldisks = NULL;
int x86_64_ndisks = 0;

#ifdef CPURESET_DELAY
int	cpureset_delay = CPURESET_DELAY;
#else
int     cpureset_delay = 2000; /* default to 2s */
#endif

int	physmem;
u_int64_t	dumpmem_low;
u_int64_t	dumpmem_high;
extern int	boothowto;
int	cpu_class;

char	*ssym = NULL;

#define	CPUID2MODEL(cpuid)	(((cpuid) >> 4) & 15)

vaddr_t	msgbuf_vaddr;
paddr_t msgbuf_paddr;

vaddr_t	idt_vaddr;
paddr_t	idt_paddr;

vaddr_t lo32_vaddr;
paddr_t lo32_paddr;

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

#ifdef NBUF
int	nbuf = NUF;
#else
int	nbuf = 0;
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int bufcachepercent = BUFCACHEPERCENT;

#ifdef DEBUG
int sigdebug = 0;
pid_t sigpid = 0;
#define SDB_FOLLOW      0x01
#endif

extern	paddr_t avail_start, avail_end;

void (*delay_func)(int) = i8254_delay;
void (*microtime_func)(struct timeval *) = i8254_microtime;
void (*initclock_func)(void) = i8254_initclocks;

struct mtrr_funcs *mtrr_funcs;

/*
 * Size of memory segments, before any memory is stolen.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int	mem_cluster_cnt;

vaddr_t	allocsys(vaddr_t);
void	setup_buffers(vaddr_t *);
int	cpu_dump(void);
int	cpu_dumpsize(void);
u_long	cpu_dump_mempagecnt(void);
void	dumpsys(void);
void	init_x86_64(paddr_t);
void	syscall_intern(struct proc *p);

/*
 * Machine-dependent startup code
 */
void
cpu_startup(void)
{
	vaddr_t v;
	vsize_t sz;
	int x;
	vaddr_t minaddr, maxaddr;

	/*
	 * Initialize error message buffer (et end of core).
	 */
	msgbuf_vaddr = uvm_km_valloc(kernel_map, x86_round_page(MSGBUFSIZE));
	if (msgbuf_vaddr == 0)
		panic("failed to valloc msgbuf_vaddr");

	/* msgbuf_paddr was init'd in pmap */
	for (x = 0; x < btoc(MSGBUFSIZE); x++)
		pmap_kenter_pa((vaddr_t)msgbuf_vaddr + x * PAGE_SIZE,
		    msgbuf_paddr + x * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kenel());

	initmsgbuf((caddr_t)msgbuf_vaddr, round_page(MSGBUFSIZE));

	printf("%s", version);

	printf("real mem = %u (%uK)\n", ctob(physmem), ctob(physmem)/1024);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = allocsys(0);
	if ((v = uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper. They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	setup_buffers(&maxaddr);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	minaddr = vm_map_min(kernel_map);
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

	printf("avail mem = %lu (%luK)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024);
	printf("using %u buffers containing %u bytes (%uK) of memory\n",
	    nbuf, bufpages * PAGE_SIZE, bufpages * PAGE_SIZE / 1024);

	bufinit();

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support - c; continuing..\n");
#endif
	}

	/* Safe for i/o port / memory space allocation to use malloc now. */
	x86_bus_space_mallocok();
}


/*
 * The following defines are for the code in setup_buffers that tries to
 * ensure that enough ISA DMAable memory is still left after the buffercache
 * has been allocated.
 */
#define CHUNKSZ		(3 * 1024 * 1024)
#define ISADMA_LIMIT	(16 * 1024 * 1024)	/* XXX wrong place */
#define ALLOC_PGS(sz, limit, pgs) \
    uvm_pglistalloc((sz), 0, (limit), PAGE_SIZE, 0, &(pgs), 1, 0)
#define FREE_PGS(pgs) uvm_pglistfree(&(pgs))

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
vaddr_t
allocsys(vaddr_t v)
{

#define	valloc(name, type, num) \
	    v = (vaddr_t)(((name) = (type *)v) + (num))

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
	if (bufpages == 0) {
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / 10;
		else
			bufpages = (btoc(2 * 1024 * 1024) + physmem) *
			    bufcachepercent / 100;
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	/* Restrict to at most 35% filled kvm */
	/* XXX - This needs UBC... */
	if (nbuf >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 35 / 100) 
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 35 / 100;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);
	return v;
}

void
setup_buffers(vaddr_t *maxaddr)
{
	vsize_t size;
	vaddr_t addr;
	int base, residual, left, chunk, i;
	struct pglist pgs, saved_pgs;
	struct vm_page *pg;
	int rv;

	size = MAXBSIZE * nbuf;
	addr = vm_map_min(kernel_map);
	if ((rv = uvm_map(kernel_map, &addr, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0))))
		panic("cpu_startup: cannot allocate VM for buffers %d", rv);
	buffers = (char *)addr;

	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE / PAGE_SIZE) {
		/* don't want to alloc more physical mem than needed */
		base = MAXBSIZE / PAGE_SIZE;
		residual = 0;
	}

	/*
	 * In case we might need DMA bouncing we have to make sure there
	 * is some memory below 16MB available.  On machines with many
	 * pages reserved for the buffer cache we risk filling all of that
	 * area with buffer pages.  We still want much of the buffers
	 * reside there as that lowers the probability of them needing to
	 * bounce, but we have to set aside some space for DMA buffers too.
	 *
	 * The current strategy is to grab hold of one 3MB chunk below 16MB
	 * first, which we are saving for DMA buffers, then try to get
	 * one chunk at a time for fs buffers, until that is not possible
	 * anymore, at which point we get the rest wherever we may find it.
	 * After that we give our saved area back. That will guarantee at
	 * least 3MB below 16MB left for drivers' attach routines, among
	 * them isadma.  However we still have a potential problem of PCI
	 * devices attached earlier snatching that memory.  This can be
	 * solved by making the PCI DMA memory allocation routines go for
	 * memory above 16MB first.
	 */

	left = bufpages;

	/*
	 * First, save ISA DMA bounce buffer area so we won't lose that
	 * capability.
	 */
	TAILQ_INIT(&saved_pgs);
	TAILQ_INIT(&pgs);
	if (!ALLOC_PGS(CHUNKSZ, ISADMA_LIMIT, saved_pgs)) {
		/*
		 * Then, grab as much ISA DMAable memory as possible
		 * for the buffer cache as it is nice to not need to
		 * bounce all buffer I/O.
		 */
		for (left = bufpages; left > 0; left -= chunk) {
			chunk = min(left, CHUNKSZ / PAGE_SIZE);
			if (ALLOC_PGS(chunk * PAGE_SIZE, ISADMA_LIMIT, pgs))
				break;
		}
	}

	/*
	 * If we need more pages for the buffer cache, get them from anywhere.
	 */
	if (left > 0 && ALLOC_PGS(left * PAGE_SIZE, avail_end, pgs))
		panic("cannot get physical memory for buffer cache");

	/*
	 * Finally, give back the ISA DMA bounce buffer area, so it can be
	 * allocated by the isadma driver later.
	 */
	if (!TAILQ_EMPTY(&saved_pgs))
		FREE_PGS(saved_pgs);

	pg = TAILQ_FIRST(&pgs);
	for (i = 0; i < nbuf; i++) {
		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		addr = (vaddr_t)buffers + i * MAXBSIZE;
		for (size = PAGE_SIZE * (i < residual ? base + 1 : base);
		    size > 0; size -= PAGE_SIZE, addr += PAGE_SIZE) {
			pmap_kenter_pa(addr, VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ|VM_PROT_WRITE);
			pg = TAILQ_NEXT(pg, pageq);
		}
	}
	pmap_update(pmap_kernel());
}

/*
 * Set up proc0's TSS and LDT.
 */
void
x86_64_proc0_tss_ldt_init(void)
{
	struct pcb *pcb;
	int x;

	gdt_init();

	cpu_info_primary.ci_curpcb = pcb = &proc0.p_addr->u_pcb;

	pcb->pcb_flags = 0;
	pcb->pcb_tss.tss_iobase =
	    (u_int16_t)((caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss);
	for (x = 0; x < sizeof(pcb->pcb_iomap) / 4; x++)
		pcb->pcb_iomap[x] = 0xffffffff;

	pcb->pcb_ldt_sel = pmap_kernel()->pm_ldt_sel =
	    GSYSSEL(GLDT_SEL, SEL_KPL);
	pcb->pcb_cr0 = rcr0();
	pcb->pcb_tss.tss_rsp0 = (u_int64_t)proc0.p_addr + USPACE - 16;
	pcb->pcb_tss.tss_ist[0] = (u_int64_t)proc0.p_addr + PAGE_SIZE;
	proc0.p_md.md_regs = (struct trapframe *)pcb->pcb_tss.tss_rsp0 - 1;
	proc0.p_md.md_tss_sel = tss_alloc(pcb);

	ltr(proc0.p_md.md_tss_sel);
	lldt(pcb->pcb_ldt_sel);
}

/*       
 * Set up TSS and LDT for a new PCB.
 */         
         
void    
x86_64_init_pcb_tss_ldt(ci)   
	struct cpu_info *ci;
{        
	int x;      
	struct pcb *pcb = ci->ci_idle_pcb;
 
	pcb->pcb_tss.tss_iobase =
	    (u_int16_t)((caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss);
	for (x = 0; x < sizeof(pcb->pcb_iomap) / 4; x++)
		pcb->pcb_iomap[x] = 0xffffffff;

	/* XXXfvdl pmap_kernel not needed */ 
	pcb->pcb_ldt_sel = pmap_kernel()->pm_ldt_sel =
	    GSYSSEL(GLDT_SEL, SEL_KPL);
	pcb->pcb_cr0 = rcr0();
        
        ci->ci_idle_tss_sel = tss_alloc(pcb);
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
	struct btinfo_bootpath *bibp;

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

	case CPU_BOOTED_KERNEL:
	        bibp = lookup_bootinfo(BTINFO_BOOTPATH);
	        if(!bibp)
			return(ENOENT); /* ??? */
		return (sysctl_rdstring(oldp, oldlenp, newp, bibp->bootpath));
	case CPU_DISKINFO:
		if (x86_64_alldisks == NULL)
			return (ENOENT);
		return (sysctl_rdstruct(oldp, oldlenp, newp, x86_64_alldisks,
		    sizeof (struct disklist) +
			(x86_64_ndisks - 1) * sizeof (struct nativedisk_info)));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

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
sendsig(sig_t catcher, int sig, int mask, u_long code, int type,
    union sigval val)
{
	struct proc *p = curproc;
	struct trapframe *tf = p->p_md.md_regs;
	struct sigacts * psp = p->p_sigacts;
	struct sigcontext ksc;
	siginfo_t ksi;
	register_t sp, scp, sip;
	u_long sss;

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig: %s[%d] sig %d catcher %p\n",
		    p->p_comm, p->p_pid, sig, catcher);
#endif

	if (p->p_md.md_flags & MDP_USEDFPU)
		fpusave_proc(p, 1);

	bcopy(tf, &ksc, sizeof(*tf));
	ksc.sc_onstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	ksc.sc_mask = mask;

	/* Allocate space for the signal handler context. */
	if ((psp->ps_flags & SAS_ALTSTACK) && !ksc.sc_onstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		sp = (register_t)psp->ps_sigstk.ss_sp + psp->ps_sigstk.ss_size;
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		sp = tf->tf_rsp;

	sp &= ~15ULL;	/* just in case */
	sss = (sizeof(ksc) + 15) & ~15;
	sip = 0;
	if (psp->ps_siginfo & sigmask(sig)) {
		sip = sp - ((sizeof(ksi) + 15) & ~15);
		sss += (sizeof(ksi) + 15) & ~15;

		initsiginfo(&ksi, sig, code, type, val);
		if (copyout(&ksi, (void *)sip, sizeof(ksi)))
			sigexit(p, SIGILL);
	}
	scp = sp - sss;

	if (copyout(&ksc, (void *)scp, sizeof(ksc)))
		sigexit(p, SIGILL);

	/*
	 * Build context to run handler in.
	 */
	tf->tf_ds = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_es = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_fs = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_gs = LSEL(LUDATA_SEL, SEL_UPL);

	tf->tf_rax = (u_int64_t)catcher;
	tf->tf_rdi = sig;
	tf->tf_rsi = sip;
	tf->tf_rdx = scp;

	tf->tf_rip = (u_int64_t)p->p_sigcode;
	tf->tf_cs = LSEL(LUCODE_SEL, SEL_UPL);
	tf->tf_rflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_rsp = sp - sss;
	tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig(%d): pc 0x%x, catcher 0x%x\n", p->p_pid,
		    tf->tf_rip, tf->tf_rax);
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
int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, ksc;
	struct trapframe *tf = p->p_md.md_regs;
	int error;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sigreturn: pid %d, scp %p\n", p->p_pid, scp);
#endif

	if (copyin((caddr_t)scp, &ksc, sizeof ksc))
		return (error);

	ksc.sc_trapno = tf->tf_trapno;
	ksc.sc_err = tf->tf_err;
	bcopy(&ksc, tf, sizeof(*tf));

	/* Restore signal stack. */
	if (ksc.sc_onstack)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = ksc.sc_mask & ~sigcantmask;

	return (EJUSTRETURN);
}

int	waittime = -1;
struct pcb dumppcb;

void
boot(int howto)
{

	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;

		if (curproc == NULL)
			curproc = &proc0;	/* XXX */
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}

	/* Disable interrupts. */
	splhigh();

	/* Do a dump if requested. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	doshutdownhooks();

#ifdef MULTIPROCESSOR
	x86_broadcast_ipi(X86_IPI_HALT);
#endif

	if (howto & RB_HALT) {
	        if (howto & RB_POWERDOWN) {
#if NACPI > 0
			delay(500000);
			acpi_enter_sleep_state(acpi_softc, ACPI_STATE_S5);
			printf("WARNING: powerdown failed!\n");
#endif
		}

		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cnpollc(1);	/* for proper keyboard command handling */
		cngetc();
		cnpollc(0);
	}

	printf("rebooting...\n");
	if (cpureset_delay > 0)
		delay(cpureset_delay * 1000);
	cpu_reset();
	for(;;) ;
	/*NOTREACHED*/
}

/*
 * XXXfvdl share dumpcode.
 */

/*
 * These variables are needed by /sbin/savecore
 */
u_int32_t	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * cpu_dump: dump the machine-dependent kernel core dump headers.
 */
int
cpu_dump()
{
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	char buf[dbtob(1)];
	kcore_seg_t *segp;

	dump = bdevsw[major(dumpdev)].d_dump;

	memset(buf, 0, sizeof buf);
	segp = (kcore_seg_t *)buf;

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	return (dump(dumpdev, dumplo, (caddr_t)buf, dbtob(1)));
}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf()
{
	const struct bdevsw *bdev;
	int nblks, dumpblks;	/* size of dump area */

	if (dumpdev == NODEV)
		goto bad;
	bdev = &bdevsw[major(dumpdev)];

	if (bdev == NULL)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdev->d_psize == NULL)
		goto bad;
	nblks = (*bdev->d_psize)(dumpdev);
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
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP  PAGE_SIZE /* must be a multiple of pagesize XXX small */
static vaddr_t dumpspace;

vaddr_t
reserve_dumppages(vaddr_t p)
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

void
dumpsys()
{
	u_long totalbytesleft, bytes, i, n, memseg;
	u_long maddr;
	int psize;
	daddr_t blkno;
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	int error;

	/* Save registers. */
	savectx(&dumppcb);

	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo <= 0 || dumpsize == 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	error = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	if ((error = cpu_dump()) != 0)
		goto err;

	totalbytesleft = ptoa(cpu_dump_mempagecnt());
	blkno = dumplo + cpu_dumpsize();
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;

	for (memseg = 0; memseg < mem_cluster_cnt; memseg++) {
		maddr = mem_clusters[memseg].start;
		bytes = mem_clusters[memseg].size;

		for (i = 0; i < bytes; i += n, totalbytesleft -= n) {
			/* Print out how many MBs we have left to go. */
			if ((totalbytesleft % (1024*1024)) == 0)
				printf("%ld ", totalbytesleft / (1024 * 1024));

			/* Limit size for next transfer. */
			n = bytes - i;
			if (n > BYTES_PER_DUMP)
				n = BYTES_PER_DUMP;

			(void) pmap_map(dumpspace, maddr, maddr + n,
			    VM_PROT_READ);

			error = (*dump)(dumpdev, blkno, (caddr_t)dumpspace, n);
			if (error)
				goto err;
			maddr += n;
			blkno += btodb(n);		/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
			/* operator aborting dump? */
			if (sget() != NULL) {
				error = EINTR;
				break;
			}
#endif
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
	delay(5000000);		/* 5 seconds */
}

/*
 * Clear registers on exec
 */
void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct trapframe *tf;

	/* If we were using the FPU, forget about it. */
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 0);

#ifdef USER_LDT
	pmap_ldt_cleanup(p);
#endif

	syscall_intern(p);

	p->p_md.md_flags &= ~MDP_USEDFPU;
	pcb->pcb_flags = 0;
	pcb->pcb_savefpu.fp_fxsave.fx_fcw = __NetBSD_NPXCW__;
	pcb->pcb_savefpu.fp_fxsave.fx_mxcsr = __INITIAL_MXCSR__;
	pcb->pcb_savefpu.fp_fxsave.fx_mxcsr_mask = __INITIAL_MXCSR_MASK__;

	tf = p->p_md.md_regs;
	tf->tf_ds = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_es = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_fs = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_gs = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_rdi = 0;
	tf->tf_rsi = 0;
	tf->tf_rbp = 0;
	tf->tf_rbx = 0;
	tf->tf_rdx = 0;
	tf->tf_rcx = 0;
	tf->tf_rax = 0;
	tf->tf_rip = pack->ep_entry;
	tf->tf_cs = LSEL(LUCODE_SEL, SEL_UPL);
	tf->tf_rflags = PSL_USERSET;
	tf->tf_rsp = stack;
	tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);

	retval[1] = 0;
}

/*
 * Initialize segments and descriptor tables
 */

struct gate_descriptor *idt;
char idt_allocmap[NIDT];
struct simplelock idt_lock;
char *ldtstore;
char *gdtstore;
extern  struct user *proc0paddr;

void
setgate(gd, func, ist, type, dpl, sel)
	struct gate_descriptor *gd;
	void *func;
	int ist, type, dpl, sel;
{
	gd->gd_looffset = (u_int64_t)func & 0xffff;
	gd->gd_selector = sel;
	gd->gd_ist = ist;
	gd->gd_type = type;
	gd->gd_dpl = dpl;
	gd->gd_p = 1;
	gd->gd_hioffset = (u_int64_t)func >> 16;
	gd->gd_zero = 0;
	gd->gd_xx1 = 0;
	gd->gd_xx2 = 0;
	gd->gd_xx3 = 0;
}

void
unsetgate(gd)
	struct gate_descriptor *gd;
{
	memset(gd, 0, sizeof (*gd));
}

void
setregion(rd, base, limit)
	struct region_descriptor *rd;
	void *base;
	u_int16_t limit;
{
	rd->rd_limit = limit;
	rd->rd_base = (u_int64_t)base;
}

/*
 * Note that the base and limit fields are ignored in long mode.
 */
void
set_mem_segment(sd, base, limit, type, dpl, gran, def32, is64)
	struct mem_segment_descriptor *sd;
	void *base;
	size_t limit;
	int type, dpl, gran, is64;
{
	sd->sd_lolimit = (unsigned)limit;
	sd->sd_lobase = (unsigned long)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (unsigned)limit >> 16;
	sd->sd_avl = 0;
	sd->sd_long = is64;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (unsigned long)base >> 24;
}

void
set_sys_segment(sd, base, limit, type, dpl, gran)
	struct sys_segment_descriptor *sd;
	void *base;
	size_t limit;
	int type, dpl, gran;
{
	memset(sd, 0, sizeof *sd);
	sd->sd_lolimit = (unsigned)limit;
	sd->sd_lobase = (u_int64_t)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (unsigned)limit >> 16;
	sd->sd_gran = gran;
	sd->sd_hibase = (u_int64_t)base >> 24;
}

void cpu_init_idt()
{
	struct region_descriptor region;

	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
	lidt(&region); 
}


#define	IDTVEC(name)	__CONCAT(X, name)
typedef void (vector)(void);
extern vector IDTVEC(syscall);
extern vector IDTVEC(syscall32);
extern vector IDTVEC(osyscall);
extern vector IDTVEC(oosyscall);
extern vector *IDTVEC(exceptions)[];

#define	KBTOB(x)	((size_t)(x) * 1024UL)

#if 0
void
beepme(int pitch, int duration)
{
#define IO_TIMER1       0x040
#define IO_PPI          0x061
#define              TIMER_SEL2      0x80
#define              TIMER_16BIT     0x30 
#define              TIMER_SQWAVE    0x06 
#define TIMER_CNTR2     (IO_TIMER1 + 2)
#define TIMER_MODE      (IO_TIMER1 + 3)
#define TIMER_FREQ      1193182
#define TIMER_DIV(x) ((TIMER_FREQ+(x)/2)/(x))

#define PIT_ENABLETMR2  0x01
#define PIT_SPKRDATA    0x02
#define PIT_SPKR        (PIT_ENABLETMR2|PIT_SPKRDATA)

#define PITCH 440

	outb(IO_TIMER1 + TIMER_MODE, TIMER_SEL2 | TIMER_16BIT | TIMER_SQWAVE);
	outb(IO_TIMER1 + TIMER_CNTR2, TIMER_DIV(pitch) % 256);
	outb(IO_TIMER1 + TIMER_CNTR2, TIMER_DIV(pitch) / 256);
	outb(IO_PPI, inb(IO_PPI) | PIT_SPKR);
	delay(duration / 2);
	outb(IO_PPI, inb(IO_PPI) & ~PIT_SPKR);
	delay(duration / 2);
}
#endif

void
init_x86_64(first_avail)
	paddr_t first_avail;
{
	extern void consinit(void);
	extern struct extent *iomem_ex;
	struct region_descriptor region;
	struct mem_segment_descriptor *ldt_segp;
	int x, first16q, ist;
	u_int64_t seg_start, seg_end;
	u_int64_t seg_start1, seg_end1;
#if !defined(REALEXTMEM) && !defined(REALBASEMEM)
	struct btinfo_memmap *bim;
	u_int64_t addr, size, io_end;
#endif

	cpu_init_msrs(&cpu_info_primary);

	proc0.p_addr = proc0paddr;
	cpu_info_primary.ci_curpcb = &proc0.p_addr->u_pcb;

	x86_bus_space_init();

	consinit();	/* XXX SHOULD NOT BE DONE HERE */

	/*
	 * Initailize PAGE_SIZE-dependent variables.
	 */
	uvm_setpagesize();

#if 0
	uvmexp.ncolors = 2;
#endif

	avail_start = PAGE_SIZE; /* BIOS leaves data in low memory */
				 /* and VM system doesn't work with phys 0 */
#ifdef MULTIPROCESSOR
	if (avail_start < MP_TRAMPOLINE + PAGE_SIZE)
		avail_start = MP_TRAMPOLINE + PAGE_SIZE;
#endif

	/*
	 * Call pmap initialization to make new kernel address space.
	 * We must do this before loading pages into the VM system.
	 */
	pmap_bootstrap(VM_MIN_KERNEL_ADDRESS);

	if (avail_start != PAGE_SIZE)
		pmap_prealloc_lowmem_ptps();

#if !defined(REALBASEMEM) && !defined(REALEXTMEM)

	/*
	 * Check to see if we have a memory map from the BIOS (passed
	 * to us by the boot program.
	 */
	bim = lookup_bootinfo(BTINFO_MEMMAP);
	if (bim != NULL && bim->num > 0) {
#if DEBUG_MEMLOAD
		printf("BIOS MEMORY MAP (%d ENTRIES):\n", bim->num);
#endif
		for (x = 0; x < bim->num; x++) {
			addr = bim->entry[x].addr;
			size = bim->entry[x].size;
#if DEBUG_MEMLOAD
			printf("    addr 0x%lx  size 0x%lx  type 0x%x\n",
			    addr, size, bim->entry[x].type);
#endif

			/*
			 * If the segment is not memory, skip it.
			 */
			switch (bim->entry[x].type) {
			case BIM_Memory:
			case BIM_ACPI:
			case BIM_NVS:
				break;
			default:
				continue;
			}

			seg_start = addr;
			seg_end = addr + size;

			if (seg_end > 0x100000000000ULL) {
				printf("WARNING: skipping large "
				    "memory map entry: "
				    "0x%lx/0x%lx/0x%x\n",
				    addr, size,
				    bim->entry[x].type);
				continue;
			}

			/*
			 * Allocate the physical addresses used by RAM
			 * from the iomem extent map.  This is done before
			 * the addresses are page rounded just to make
			 * sure we get them all.
			 */
			if (seg_start < 0x100000000UL) {
				if (seg_end > 0x100000000UL)
					io_end = 0x100000000UL;
				else
					io_end = seg_end;
				if (extent_alloc_region(iomem_ex, seg_start,
				    io_end - seg_start, EX_NOWAIT)) {
					/* XXX What should we do? */
					printf("WARNING: CAN'T ALLOCATE "
					    "MEMORY SEGMENT %d "
					    "(0x%lx/0x%lx/0l%x) FROM "
					    "IOMEM EXTENT MAP!\n",
					    x, seg_start, io_end - seg_start,
					    bim->entry[x].type);
				}
			}

			/*
			 * If it's not free memory, skip it.
			 */
			if (bim->entry[x].type != BIM_Memory)
				continue;

			/* XXX XXX XXX */
			if (mem_cluster_cnt >= VM_PHYSSEG_MAX)
				panic("init386: too many memory segments");

			seg_start = round_page(seg_start);
			seg_end = trunc_page(seg_end);

			if (seg_start == seg_end)
				continue;

			mem_clusters[mem_cluster_cnt].start = seg_start;
			mem_clusters[mem_cluster_cnt].size =
			    seg_end - seg_start;

			if (avail_end < seg_end)
				avail_end = seg_end;
			physmem += atop(mem_clusters[mem_cluster_cnt].size);
			mem_cluster_cnt++;
		}
	}
#endif	/* ! REALBASEMEM && ! REALEXTMEM */

	/*
	 * If the loop above didn't find any valid segment, fall back to
	 * former code.
	 */
	if (mem_cluster_cnt == 0) {
		/*
		 * Allocate the physical addresses used by RAM from the iomem
		 * extent map.  This is done before the addresses are
		 * page rounded just to make sure we get them all.
		 */
		if (extent_alloc_region(iomem_ex, 0, KBTOB(biosbasemem),
		    EX_NOWAIT)) {
			/* XXX What should we do? */
			printf("WARNING: CAN'T ALLOCATE BASE MEMORY FROM "
			    "IOMEM EXTENT MAP!\n");
		}
		mem_clusters[0].start = 0;
		mem_clusters[0].size = trunc_page(KBTOB(biosbasemem));
		physmem += atop(mem_clusters[0].size);
		if (extent_alloc_region(iomem_ex, IOM_END, KBTOB(biosextmem),
		    EX_NOWAIT)) {
			/* XXX What should we do? */
			printf("WARNING: CAN'T ALLOCATE EXTENDED MEMORY FROM "
			    "IOMEM EXTENT MAP!\n");
		}
#if NISADMA > 0
		/*
		 * Some motherboards/BIOSes remap the 384K of RAM that would
		 * normally be covered by the ISA hole to the end of memory
		 * so that it can be used.  However, on a 16M system, this
		 * would cause bounce buffers to be allocated and used.
		 * This is not desirable behaviour, as more than 384K of
		 * bounce buffers might be allocated.  As a work-around,
		 * we round memory down to the nearest 1M boundary if
		 * we're using any isadma devices and the remapped memory
		 * is what puts us over 16M.
		 */
		if (biosextmem > (15*1024) && biosextmem < (16*1024)) {
			char pbuf[9];

			format_bytes(pbuf, sizeof(pbuf),
			    biosextmem - (15*1024));
			printf("Warning: ignoring %s of remapped memory\n",
			    pbuf);
			biosextmem = (15*1024);
		}
#endif
		mem_clusters[1].start = IOM_END;
		mem_clusters[1].size = trunc_page(KBTOB(biosextmem));
		physmem += atop(mem_clusters[1].size);

		mem_cluster_cnt = 2;

		avail_end = IOM_END + trunc_page(KBTOB(biosextmem));
	}

	/*
	 * If we have 16M of RAM or less, just put it all on
	 * the default free list.  Otherwise, put the first
	 * 16M of RAM on a lower priority free list (so that
	 * all of the ISA DMA'able memory won't be eaten up
	 * first-off).
	 */
	if (avail_end <= (16 * 1024 * 1024))
		first16q = VM_FREELIST_DEFAULT;
	else
		first16q = VM_FREELIST_FIRST16;

	/* Make sure the end of the space used by the kernel is rounded. */
	first_avail = round_page(first_avail);

	/*
	 * Now, load the memory clusters (which have already been
	 * rounded and truncated) into the VM system.
	 *
	 * NOTE: WE ASSUME THAT MEMORY STARTS AT 0 AND THAT THE KERNEL
	 * IS LOADED AT IOM_END (1M).
	 */
	for (x = 0; x < mem_cluster_cnt; x++) {
		seg_start = mem_clusters[x].start;
		seg_end = mem_clusters[x].start + mem_clusters[x].size;
		seg_start1 = 0;
		seg_end1 = 0;

		if (seg_start > 0xffffffffULL) {
			printf("skipping %lld bytes of memory above 4GB\n",
			    seg_end - seg_start);
			continue;
		}
		if (seg_end > 0x100000000ULL) {
			printf("skipping %lld bytes of memory above 4GB\n",
			    seg_end - 0x100000000ULL);
			seg_end = 0x100000000ULL;
		}

		/*
		 * Skip memory before our available starting point.
		 */
		if (seg_end <= avail_start)
			continue;

		if (avail_start >= seg_start && avail_start < seg_end) {
			if (seg_start != 0)
				panic("init_x86_64: memory doesn't start at 0");
			seg_start = avail_start;
			if (seg_start == seg_end)
				continue;
		}

		/*
		 * If this segment contains the kernel, split it
		 * in two, around the kernel.
		 */
		if (seg_start <= IOM_END && first_avail <= seg_end) {
			seg_start1 = first_avail;
			seg_end1 = seg_end;
			seg_end = IOM_END;
		}

		/* First hunk */
		if (seg_start != seg_end) {
			if (seg_start <= (16 * 1024 * 1024) &&
			    first16q != VM_FREELIST_DEFAULT) {
				u_int64_t tmp;

				if (seg_end > (16 * 1024 * 1024))
					tmp = (16 * 1024 * 1024);
				else
					tmp = seg_end;
#if DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    (unsigned long long)seg_start,
				    (unsigned long long)tmp,
				    atop(seg_start), atop(tmp));
#endif
				uvm_page_physload(atop(seg_start),
				    atop(tmp), atop(seg_start),
				    atop(tmp), first16q);
				seg_start = tmp;
			}

			if (seg_start != seg_end) {
#if DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    (unsigned long long)seg_start,
				    (unsigned long long)seg_end,
				    atop(seg_start), atop(seg_end));
#endif
				uvm_page_physload(atop(seg_start),
				    atop(seg_end), atop(seg_start),
				    atop(seg_end), VM_FREELIST_DEFAULT);
			}
		}

		/* Second hunk */
		if (seg_start1 != seg_end1) {
			if (seg_start1 <= (16 * 1024 * 1024) &&
			    first16q != VM_FREELIST_DEFAULT) {
				u_int64_t tmp;

				if (seg_end1 > (16 * 1024 * 1024))
					tmp = (16 * 1024 * 1024);
				else
					tmp = seg_end1;
#if DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    (unsigned long long)seg_start1,
				    (unsigned long long)tmp,
				    atop(seg_start1), atop(tmp));
#endif
				uvm_page_physload(atop(seg_start1),
				    atop(tmp), atop(seg_start1),
				    atop(tmp), first16q);
				seg_start1 = tmp;
			}

			if (seg_start1 != seg_end1) {
#if DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    (unsigned long long)seg_start1,
				    (unsigned long long)seg_end1,
				    atop(seg_start1), atop(seg_end1));
#endif
				uvm_page_physload(atop(seg_start1),
				    atop(seg_end1), atop(seg_start1),
				    atop(seg_end1), VM_FREELIST_DEFAULT);
			}
		}
	}

	/*
	 * Steal memory for the message buffer (at end of core).
	 */
	{
		struct vm_physseg *vps = NULL;
		psize_t sz = round_page(MSGBUFSIZE);
		psize_t reqsz = sz;

		for (x = 0; x < vm_nphysseg; x++) {
			vps = &vm_physmem[x];
			if (ptoa(vps->avail_end) == avail_end)
				break;
		}
		if (x == vm_nphysseg)
			panic("init_x86_64: can't find end of memory");

		/* Shrink so it'll fit in the last segment. */
		if ((vps->avail_end - vps->avail_start) < atop(sz))
			sz = ptoa(vps->avail_end - vps->avail_start);

		vps->avail_end -= atop(sz);
		vps->end -= atop(sz);
		msgbuf_paddr = ptoa(vps->avail_end);

		/* Remove the last segment if it now has no pages. */
		if (vps->start == vps->end) {
			for (vm_nphysseg--; x < vm_nphysseg; x++)
				vm_physmem[x] = vm_physmem[x + 1];
		}

		/* Now find where the new avail_end is. */
		for (avail_end = 0, x = 0; x < vm_nphysseg; x++)
			if (vm_physmem[x].avail_end > avail_end)
				avail_end = vm_physmem[x].avail_end;
		avail_end = ptoa(avail_end);

		/* Warn if the message buffer had to be shrunk. */
		if (sz != reqsz)
			printf("WARNING: %ld bytes not available for msgbuf "
			    "in last cluster (%ld used)\n", reqsz, sz);
	}

	/*
	 * XXXfvdl todo: acpi wakeup code.
	 */

	pmap_growkernel(VM_MIN_KERNEL_ADDRESS + 32 * 1024 * 1024);

	pmap_kenter_pa(idt_vaddr, idt_paddr, VM_PROT_READ|VM_PROT_WRITE);
	pmap_kenter_pa(idt_vaddr + PAGE_SIZE, idt_paddr + PAGE_SIZE,
	    VM_PROT_READ|VM_PROT_WRITE);

	pmap_kenter_pa(lo32_vaddr, lo32_paddr, VM_PROT_READ|VM_PROT_WRITE);

	idt = (struct gate_descriptor *)idt_vaddr;
	gdtstore = (char *)(idt + NIDT);
	ldtstore = gdtstore + DYNSEL_START;

	/* make gdt gates and memory segments */
	set_mem_segment(GDT_ADDR_MEM(gdtstore, GCODE_SEL), 0, 0xfffff, SDT_MEMERA,
	    SEL_KPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GDATA_SEL), 0, 0xfffff, SDT_MEMRWA,
	    SEL_KPL, 1, 0, 1);

	set_sys_segment(GDT_ADDR_SYS(gdtstore, GLDT_SEL), ldtstore, LDT_SIZE - 1,
	    SDT_SYSLDT, SEL_KPL, 0);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUCODE_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMERA, SEL_UPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUDATA_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMRWA, SEL_UPL, 1, 0, 1);

	/* make ldt gates and memory segments */
	setgate((struct gate_descriptor *)(ldtstore + LSYS5CALLS_SEL),
	    &IDTVEC(oosyscall), 0, SDT_SYS386CGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	*(struct mem_segment_descriptor *)(ldtstore + LUCODE_SEL) =
	    *GDT_ADDR_MEM(gdtstore, GUCODE_SEL);
	*(struct mem_segment_descriptor *)(ldtstore + LUDATA_SEL) =
	    *GDT_ADDR_MEM(gdtstore, GUDATA_SEL);

	/*
	 * 32 bit GDT entries.
	 */

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUCODE32_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMERA, SEL_UPL, 1, 1, 0);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUDATA32_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMRWA, SEL_UPL, 1, 1, 0);

	/*
	 * 32 bit LDT entries.
	 */
	ldt_segp = (struct mem_segment_descriptor *)(ldtstore + LUCODE32_SEL);
	set_mem_segment(ldt_segp, 0, x86_btop(VM_MAXUSER_ADDRESS32) - 1,
	    SDT_MEMERA, SEL_UPL, 1, 1, 0);
	ldt_segp = (struct mem_segment_descriptor *)(ldtstore + LUDATA32_SEL);
	set_mem_segment(ldt_segp, 0, x86_btop(VM_MAXUSER_ADDRESS32) - 1,
	    SDT_MEMRWA, SEL_UPL, 1, 1, 0);

	/*
	 * Other entries.
	 */
	memcpy((struct gate_descriptor *)(ldtstore + LSOL26CALLS_SEL),
	    (struct gate_descriptor *)(ldtstore + LSYS5CALLS_SEL),
	    sizeof (struct gate_descriptor));
	memcpy((struct gate_descriptor *)(ldtstore + LBSDICALLS_SEL),
	    (struct gate_descriptor *)(ldtstore + LSYS5CALLS_SEL),
	    sizeof (struct gate_descriptor));

	/* exceptions */
	for (x = 0; x < 32; x++) {
		ist = (x == 8 || x == 3) ? 1 : 0;
		setgate(&idt[x], IDTVEC(exceptions)[x], ist, SDT_SYS386IGT,
		    (x == 3 || x == 4) ? SEL_UPL : SEL_KPL,
		    GSEL(GCODE_SEL, SEL_KPL));
		idt_allocmap[x] = 1;
	}

	/* new-style interrupt gate for syscalls */
	setgate(&idt[128], &IDTVEC(osyscall), 0, SDT_SYS386IGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	idt_allocmap[128] = 1;

	setregion(&region, gdtstore, DYNSEL_START - 1);
	lgdt(&region);

	cpu_init_idt();

#ifdef DDB
	{
		extern caddr_t esym;
		struct btinfo_symtab *symtab;

		symtab = lookup_bootinfo(BTINFO_SYMTAB);
		if (symtab) {
			ssym = (char *)((vaddr_t)symtab->ssym + KERNBASE);
			esym = (caddr_t)((vaddr_t)symtab->esym + KERNBASE);
		}

		db_machine_init();
		ddb_init();
	}

	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef KGDB
	kgdb_port_init();
	if (boothowto & RB_KDB) {
		kgdb_debug_init = 1;
		kgdb_connect(1);
	}
#endif

	intr_default_setup();

	softintr_init();
	splraise(IPL_IPI);
	enable_intr();

        /* Make sure maxproc is sane */ 
        if (maxproc > cpu_maxproc())
                maxproc = cpu_maxproc();
}

void *
lookup_bootinfo(type)
	int type;
{
	struct btinfo_common *help;
	int n = *(int*)bootinfo;
	help = (struct btinfo_common *)(bootinfo + sizeof(int));
	while(n--) {
		if(help->type == type) {
#if 0
        		if (type == BTINFO_CONSOLE) {
				struct btinfo_console *consinfo = (struct btinfo_console *)help;
				snprintf(consinfo->devname, 16, "com");
			        consinfo->speed = 9600;
			        consinfo->addr = 0x3f8; 
			}
#endif
			return(help);
		}
		help = (struct btinfo_common *)((char*)help + help->len);
	}
	return(0);
}

void
cpu_reset()
{

	disable_intr();

	/*
	 * The keyboard controller has 4 random output pins, one of which is
	 * connected to the RESET pin on the CPU in many PCs.  We tell the
	 * keyboard controller to pulse this line a couple of times.
	 */
	outb(IO_KBD + KBCMDP, KBC_PULSE0);
	delay(100000);
	outb(IO_KBD + KBCMDP, KBC_PULSE0);
	delay(100000);

	/*
	 * Try to cause a triple fault and watchdog reset by making the IDT
	 * invalid and causing a fault.
	 */
	memset((caddr_t)idt, 0, NIDT * sizeof(idt[0]));
	__asm __volatile("divl %0,%1" : : "q" (0), "a" (0)); 

#if 0
	/*
	 * Try to cause a triple fault and watchdog reset by unmapping the
	 * entire address space and doing a TLB flush.
	 */
	memset((caddr_t)PTD, 0, PAGE_SIZE);
	tlbflush(); 
#endif

	for (;;);
}

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers.
 */
int
cpu_dumpsize()
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) +
	    ALIGN(mem_cluster_cnt * sizeof(phys_ram_seg_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return (-1);

	return (1);
}

/*
 * cpu_dump_mempagecnt: calculate the size of RAM (in pages) to be dumped.
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

#if 0
extern void i8254_microtime(struct timeval *tv);

/*
 * XXXXXXX
 * the simulator's 8254 seems to travel backward in time sometimes?
 * work around this with this hideous code. Unacceptable for
 * real hardware, but this is just a patch to stop the weird
 * effects. SMP unsafe, etc.
 */
void
microtime(struct timeval *tv)
{
	static struct timeval mtv;

	i8254_microtime(tv);
	if (tv->tv_sec <= mtv.tv_sec && tv->tv_usec < mtv.tv_usec) {
		mtv.tv_usec++;
		if (mtv.tv_usec > 1000000) {
			mtv.tv_sec++;
			mtv.tv_usec = 0;
		}
		*tv = mtv;
	} else
		mtv = *tv;
}
#endif

void
cpu_initclocks()
{
	(*initclock_func)();
}

#ifdef MULTIPROCESSOR
void
need_resched(struct cpu_info *ci)
{
	ci->ci_want_resched = 1;
	if ((ci)->ci_curproc != NULL)
		aston((ci)->ci_curproc);
}
#endif

/*
 * Allocate an IDT vector slot within the given range.
 * XXX needs locking to avoid MP allocation races.
 * XXXfvdl share idt code
 */

int
idt_vec_alloc(low, high)
	int low;
	int high;
{
	int vec;

	simple_lock(&idt_lock);
	for (vec = low; vec <= high; vec++) {
		if (idt_allocmap[vec] == 0) {
			idt_allocmap[vec] = 1;
			simple_unlock(&idt_lock);
			return vec;
		}
	}
	simple_unlock(&idt_lock);
	return 0;
}

void
idt_vec_set(vec, function)
	int vec;
	void (*function)(void);
{
	/*
	 * Vector should be allocated, so no locking needed.
	 */
	KASSERT(idt_allocmap[vec] == 1);
	setgate(&idt[vec], function, 0, SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
}

void
idt_vec_free(vec)
	int vec;
{
	simple_lock(&idt_lock);
	unsetgate(&idt[vec]);
	idt_allocmap[vec] = 0;
	simple_unlock(&idt_lock);
}

/*
 * Number of processes is limited by number of available GDT slots.
 */
int
cpu_maxproc(void)
{
#ifdef USER_LDT
	return ((MAXGDTSIZ - DYNSEL_START) / 32);
#else
	return (MAXGDTSIZ - DYNSEL_START) / 16;
#endif
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int cpl = curcpu()->ci_ilevel;

	if (cpl < wantipl) {
		splassert_fail(wantipl, cpl, func);
	}
}
#endif

int
check_context(const struct reg *regs, struct trapframe *tf)
{
	uint16_t sel;

	if (((regs->r_rflags ^ tf->tf_rflags) & PSL_USERSTATIC) != 0)
		return EINVAL;

	sel = regs->r_es & 0xffff;
	if (sel != 0 && !VALID_USER_DSEL(sel))
		return EINVAL;

	sel = regs->r_fs & 0xffff;
	if (sel != 0 && !VALID_USER_DSEL(sel))
		return EINVAL;

	sel = regs->r_gs & 0xffff;
	if (sel != 0 && !VALID_USER_DSEL(sel))
		return EINVAL;

	sel = regs->r_ds & 0xffff;
	if (!VALID_USER_DSEL(sel))
		return EINVAL;

	sel = regs->r_ss & 0xffff;
	if (!VALID_USER_DSEL(sel)) 
		return EINVAL;

	sel = regs->r_cs & 0xffff;
	if (!VALID_USER_CSEL(sel))
		return EINVAL;

	if (regs->r_rip >= VM_MAXUSER_ADDRESS)
		return EINVAL;

	return 0;
}
