/*	$OpenBSD: machdep.c,v 1.201 2014/12/10 15:29:52 mikeb Exp $	*/
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
 * 3. Neither the name of the University nor the names of its contributors
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

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/cons.h>
#include <stand/boot/bootarg.h>

#include <net/if.h>
#include <uvm/uvm.h>

#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/fpu.h>
#include <machine/biosvar.h>
#include <machine/mpbiosvar.h>
#include <machine/kcore.h>
#include <machine/tss.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>
#include <dev/ic/i8042reg.h>
#include <amd64/isa/nvram.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
extern int db_console;
#endif

#include "isa.h"
#include "isadma.h"
#include "ksyms.h"

#include "acpi.h"
#if NACPI > 0
#include <dev/acpi/acpivar.h>
#endif

#include "com.h"
#if NCOM > 0
#include <sys/tty.h>
#include <dev/ic/comvar.h>
#include <dev/ic/comreg.h>
#endif

#include "softraid.h"
#if NSOFTRAID > 0
#include <dev/softraidvar.h>
#endif

#ifdef HIBERNATE
#include <machine/hibernate_var.h>
#endif /* HIBERNATE */

/* the following is used externally (sysctl_hw) */
char machine[] = MACHINE;

/*
 * switchto vectors
 */
void (*cpu_idle_leave_fcn)(void) = NULL;
void (*cpu_idle_cycle_fcn)(void) = NULL;
void (*cpu_idle_enter_fcn)(void) = NULL;
void (*cpu_busy_cycle_fcn)(void) = NULL;

/* the following is used externally for concurrent handlers */
int setperf_prio = 0;

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

paddr_t	dumpmem_paddr;
vaddr_t	dumpmem_vaddr;
psize_t	dumpmem_sz;


char	*ssym = NULL;
vaddr_t kern_end;

vaddr_t	msgbuf_vaddr;
paddr_t msgbuf_paddr;

vaddr_t	idt_vaddr;
paddr_t	idt_paddr;

vaddr_t lo32_vaddr;
paddr_t lo32_paddr;
paddr_t tramp_pdirpa;

int kbd_reset;
int lid_suspend;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = 0;

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

/* UVM constraint ranges. */
struct uvm_constraint_range  isa_constraint = { 0x0, 0x00ffffffUL };
struct uvm_constraint_range  dma_constraint = { 0x0, 0xffffffffUL };
struct uvm_constraint_range *uvm_md_constraints[] = {
    &isa_constraint,
    &dma_constraint,
    NULL,
};

#ifdef DEBUG
int sigdebug = 0;
pid_t sigpid = 0;
#define SDB_FOLLOW      0x01
#endif

paddr_t avail_start;
paddr_t avail_end;

void (*delay_func)(int) = i8254_delay;
void (*initclock_func)(void) = i8254_initclocks;

/*
 * Format of boot information passed to us by 32-bit /boot
 */
typedef struct _boot_args32 {
	int	ba_type;
	int	ba_size;
	int	ba_nextX;	/* a ptr in 32-bit world, but not here */
	char	ba_arg[1];
} bootarg32_t;

#define BOOTARGC_MAX	NBPG	/* one page */

bios_bootmac_t *bios_bootmac;

/* locore copies the arguments from /boot to here for us */
char bootinfo[BOOTARGC_MAX];
int bootinfo_size = BOOTARGC_MAX;

void getbootinfo(char *, int);

/* Data passed to us by /boot, filled in by getbootinfo() */
bios_diskinfo_t	*bios_diskinfo;
bios_memmap_t	*bios_memmap;
u_int32_t	bios_cksumlen;

/*
 * Size of memory segments, before any memory is stolen.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int	mem_cluster_cnt;

int	cpu_dump(void);
int	cpu_dumpsize(void);
u_long	cpu_dump_mempagecnt(void);
void	dumpsys(void);
void	cpu_init_extents(void);
void	map_tramps(void);
void	init_x86_64(paddr_t);
void	(*cpuresetfn)(void);

#ifdef KGDB
#ifndef KGDB_DEVNAME
#define KGDB_DEVNAME	"com"
#endif /* KGDB_DEVNAME */
char kgdb_devname[] = KGDB_DEVNAME;
#if NCOM > 0
#ifndef KGDBADDR
#define KGDBADDR	0x3f8
#endif /* KGDBADDR */
int comkgdbaddr = KGDBADDR;
#ifndef KGDBRATE
#define KGDBRATE	TTYDEF_SPEED
#endif /* KGDBRATE */
int comkgdbrate = KGDBRATE;
#ifndef KGDBMODE
#define KGDBMODE	((TTYDEF_CFLAG & ~(CSIZE | CSTOPB | PARENB)) | CS8)
#endif /* KGDBMODE */
int comkgdbmode = KGDBMODE;
#endif /* NCOM */
void	kgdb_port_init(void);
#endif /* KGDB */

#ifdef APERTURE
#ifdef INSECURE
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

/*
 * Machine-dependent startup code
 */
void
cpu_startup(void)
{
	vaddr_t minaddr, maxaddr;

	msgbuf_vaddr = PMAP_DIRECT_MAP(msgbuf_paddr);
	initmsgbuf((caddr_t)msgbuf_vaddr, round_page(MSGBUFSIZE));

	printf("%s", version);
	startclocks();

	printf("real mem = %lu (%luMB)\n", ptoa((psize_t)physmem),
	    ptoa((psize_t)physmem)/1024/1024);

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

	printf("avail mem = %lu (%luMB)\n", ptoa((psize_t)uvmexp.free),
	    ptoa((psize_t)uvmexp.free)/1024/1024);

	bufinit();

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}

	/* Safe for i/o port / memory space allocation to use malloc now. */
	x86_bus_space_mallocok();
}

/*
 * Set up proc0's PCB and the cpu's TSS.
 */
void
x86_64_proc0_tss_ldt_init(void)
{
	struct pcb *pcb;

	cpu_info_primary.ci_curpcb = pcb = &proc0.p_addr->u_pcb;
	pcb->pcb_cr0 = rcr0();
	pcb->pcb_fsbase = 0;
	pcb->pcb_kstack = (u_int64_t)proc0.p_addr + USPACE - 16;
	proc0.p_md.md_regs = (struct trapframe *)pcb->pcb_kstack - 1;

	/* an empty iomap, by setting its offset to the TSS limit */
	cpu_info_primary.ci_tss->tss_iobase = sizeof(struct x86_64_tss);
	cpu_info_primary.ci_tss->tss_rsp0 = pcb->pcb_kstack;
	cpu_info_primary.ci_tss->tss_ist[0] =
	    (u_int64_t)proc0.p_addr + PAGE_SIZE - 16;

	ltr(GSYSSEL(GPROC0_SEL, SEL_KPL));
	lldt(0);
}

/*       
 * Set up TSS for a new PCB.
 */         
         
#ifdef MULTIPROCESSOR
void    
x86_64_init_pcb_tss_ldt(struct cpu_info *ci)   
{        
	struct pcb *pcb = ci->ci_idle_pcb;
 
	ci->ci_tss->tss_iobase = sizeof(*ci->ci_tss);
	ci->ci_tss->tss_rsp0 = pcb->pcb_kstack;
	ci->ci_tss->tss_ist[0] = pcb->pcb_kstack - USPACE + PAGE_SIZE;

	pcb->pcb_cr0 = rcr0();
}       
#endif	/* MULTIPROCESSOR */

bios_diskinfo_t *
bios_getdiskinfo(dev_t dev)
{
	bios_diskinfo_t *pdi;

	if (bios_diskinfo == NULL)
		return NULL;

	for (pdi = bios_diskinfo; pdi->bios_number != -1; pdi++) {
		if ((dev & B_MAGICMASK) == B_DEVMAGIC) { /* search by bootdev */
			if (pdi->bsd_dev == dev)
				break;
		} else {
			if (pdi->bios_number == dev)
				break;
		}
	}

	if (pdi->bios_number == -1)
		return NULL;
	else
		return pdi;
}

int
bios_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	bios_diskinfo_t *pdi;
	extern dev_t bootdev;
	int biosdev;

	/* all sysctl names at this level except diskinfo are terminal */
	if (namelen != 1 && name[0] != BIOS_DISKINFO)
		return (ENOTDIR);	       /* overloaded */

	if (!(bootapiver & BAPIV_VECTOR))
		return EOPNOTSUPP;

	switch (name[0]) {
	case BIOS_DEV:
		if ((pdi = bios_getdiskinfo(bootdev)) == NULL)
			return ENXIO;
		biosdev = pdi->bios_number;
		return sysctl_rdint(oldp, oldlenp, newp, biosdev);
	case BIOS_DISKINFO:
		if (namelen != 2)
			return ENOTDIR;
		if ((pdi = bios_getdiskinfo(name[1])) == NULL)
			return ENXIO;
		return sysctl_rdstruct(oldp, oldlenp, newp, pdi, sizeof(*pdi));
	case BIOS_CKSUMLEN:
		return sysctl_rdint(oldp, oldlenp, newp, bios_cksumlen);
	default:
		return EOPNOTSUPP;
	}
	/* NOTREACHED */
}

/*  
 * machine dependent system variables.
 */ 
int
cpu_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	extern int amd64_has_xcrypt;
	dev_t consdev;
	dev_t dev;

	switch (name[0]) {
	case CPU_CONSDEV:
		if (namelen != 1)
			return (ENOTDIR);		/* overloaded */
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));
	case CPU_CHR2BLK:
		if (namelen != 2)
			return (ENOTDIR);		/* overloaded */
		dev = chrtoblk((dev_t)name[1]);
		return sysctl_rdstruct(oldp, oldlenp, newp, &dev, sizeof(dev));
	case CPU_BIOS:
		return bios_sysctl(name + 1, namelen - 1, oldp, oldlenp,
		    newp, newlen, p);
	case CPU_CPUVENDOR:
		return (sysctl_rdstring(oldp, oldlenp, newp, cpu_vendor));
	case CPU_CPUID:
		return (sysctl_rdint(oldp, oldlenp, newp, cpu_id));
	case CPU_CPUFEATURE:
		return (sysctl_rdint(oldp, oldlenp, newp, cpu_feature));
	case CPU_KBDRESET:
		if (securelevel > 0)
			return (sysctl_rdint(oldp, oldlenp, newp,
			    kbd_reset));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &kbd_reset));
	case CPU_ALLOWAPERTURE:
		if (namelen != 1)
			return (ENOTDIR);		/* overloaded */
#ifdef APERTURE
		if (securelevel > 0)
			return (sysctl_int_lower(oldp, oldlenp, newp, newlen,
			    &allowaperture));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	case CPU_XCRYPT:
		return (sysctl_rdint(oldp, oldlenp, newp, amd64_has_xcrypt));
	case CPU_LIDSUSPEND:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &lid_suspend));
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
	struct sigacts *psp = p->p_p->ps_sigacts;
	struct sigcontext ksc;
	siginfo_t ksi;
	register_t sp, scp, sip;
	u_long sss;

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig: %s[%d] sig %d catcher %p\n",
		    p->p_comm, p->p_pid, sig, catcher);
#endif

	bcopy(tf, &ksc, sizeof(*tf));
	bzero((char *)&ksc + sizeof(*tf), sizeof(ksc) - sizeof(*tf));
	ksc.sc_mask = mask;

	/* Allocate space for the signal handler context. */
	if ((p->p_sigstk.ss_flags & SS_DISABLE) == 0 &&
	    !sigonstack(tf->tf_rsp) && (psp->ps_sigonstack & sigmask(sig)))
		sp = (register_t)p->p_sigstk.ss_sp + p->p_sigstk.ss_size;
	else
		sp = tf->tf_rsp - 128;

	sp &= ~15ULL;	/* just in case */
	sss = (sizeof(ksc) + 15) & ~15;

	if (p->p_md.md_flags & MDP_USEDFPU) {
		fpusave_proc(p, 1);
		sp -= sizeof(struct fxsave64);
		ksc.sc_fpstate = (struct fxsave64 *)sp;
		if (copyout(&p->p_addr->u_pcb.pcb_savefpu.fp_fxsave,
		    (void *)sp, sizeof(struct fxsave64)))
			sigexit(p, SIGILL);

		/* Signal handlers get a completely clean FP state */
		p->p_md.md_flags &= ~MDP_USEDFPU;
	}

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
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_gs = GSEL(GUDATA_SEL, SEL_UPL);

	tf->tf_rax = (u_int64_t)catcher;
	tf->tf_rdi = sig;
	tf->tf_rsi = sip;
	tf->tf_rdx = scp;

	tf->tf_rip = (u_int64_t)p->p_p->ps_sigcode;
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_rflags &= ~(PSL_T|PSL_D|PSL_VM|PSL_AC);
	tf->tf_rsp = scp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig(%d): pc 0x%llx, catcher 0x%llx\n", p->p_pid,
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
	if ((error = copyin((caddr_t)scp, &ksc, sizeof ksc)))
		return (error);

	if (((ksc.sc_rflags ^ tf->tf_rflags) & PSL_USERSTATIC) != 0 ||
	    !USERMODE(ksc.sc_cs, ksc.sc_eflags))
		return (EINVAL);

	if (p->p_md.md_flags & MDP_USEDFPU)
		fpusave_proc(p, 0);

	if (ksc.sc_fpstate) {
		struct fxsave64 *fx = &p->p_addr->u_pcb.pcb_savefpu.fp_fxsave;

		if ((error = copyin(ksc.sc_fpstate, fx, sizeof(*fx))))
			return (error);
		fx->fx_mxcsr &= fpu_mxcsr_mask;
		p->p_md.md_flags |= MDP_USEDFPU;
	}

	ksc.sc_trapno = tf->tf_trapno;
	ksc.sc_err = tf->tf_err;
	bcopy(&ksc, tf, sizeof(*tf));

	/* Restore signal mask. */
	p->p_sigmask = ksc.sc_mask & ~sigcantmask;

	/*
	 * sigreturn() needs to return to userspace via the 'iretq'
	 * method, so that if the process was interrupted (by tick,
	 * an IPI, whatever) as opposed to already being in the kernel
	 * when a signal was being delivered, the process will be
	 * completely restored, including the userland %rcx and %r11
	 * registers which the 'sysretq' instruction cannot restore.
	 */
	p->p_md.md_flags |= MDP_IRET;

	return (EJUSTRETURN);
}

#ifdef MULTIPROCESSOR
/* force a CPU into the kernel, whether or not it's idle */
void
cpu_kick(struct cpu_info *ci)
{
	/* only need to kick other CPUs */
	if (ci != curcpu()) {
		if (ci->ci_mwait != NULL) {
			/*
			 * If not idling, then send an IPI, else
			 * just clear the "keep idling" bit.
			 */
			if ((ci->ci_mwait[0] & MWAIT_IN_IDLE) == 0)
				x86_send_ipi(ci, X86_IPI_NOP);
			else
				atomic_clearbits_int(&ci->ci_mwait[0],
				    MWAIT_KEEP_IDLING);
		} else {
			/* no mwait, so need an IPI */
			x86_send_ipi(ci, X86_IPI_NOP);
		}
	}
}
#endif

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
void
signotify(struct proc *p)
{
	aston(p);
	cpu_kick(p->p_cpu);
}

#ifdef MULTIPROCESSOR
void
cpu_unidle(struct cpu_info *ci)
{
	if (ci->ci_mwait != NULL) {
		/*
		 * Just clear the "keep idling" bit; if it wasn't
		 * idling then we didn't need to do anything anyway.
		 */
		atomic_clearbits_int(&ci->ci_mwait[0], MWAIT_KEEP_IDLING);
		return;
	}

	if (ci != curcpu())
		x86_send_ipi(ci, X86_IPI_NOP);
}
#endif

int	waittime = -1;
struct pcb dumppcb;

__dead void
boot(int howto)
{
	if ((howto & RB_POWERDOWN) != 0)
		lid_suspend = 0;

	if (cold) {
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();

		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}
	if_downall();

	delay(4*1000000);	/* XXX */

	uvm_shutdown();
	splhigh();
	cold = 1;

	if ((howto & RB_DUMP) != 0)
		dumpsys();

haltsys:
	config_suspend_all(DVACT_POWERDOWN);

#ifdef MULTIPROCESSOR
	x86_broadcast_ipi(X86_IPI_HALT);
#endif

	if ((howto & RB_HALT) != 0) {
#if NACPI > 0 && !defined(SMALL_KERNEL)
		extern int acpi_enabled;

		if (acpi_enabled) {
			delay(500000);
			if ((howto & RB_POWERDOWN) != 0)
				acpi_powerdown();
		}
#endif
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
	for (;;) ;
	/* NOTREACHED */
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * cpu_dump: dump the machine-dependent kernel core dump headers.
 */
int
cpu_dump(void)
{
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	char buf[dbtob(1)];
	kcore_seg_t *segp;
	cpu_kcore_hdr_t *cpuhdrp;
	phys_ram_seg_t *memsegp;
	caddr_t va;
	int i;

	dump = bdevsw[major(dumpdev)].d_dump;

	memset(buf, 0, sizeof buf);
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
	cpuhdrp->ptdpaddr = PTDpaddr;
	cpuhdrp->nmemsegs = mem_cluster_cnt;

	/*
	 * Fill in the memory segment descriptors.
	 */
	for (i = 0; i < mem_cluster_cnt; i++) {
		memsegp[i].start = mem_clusters[i].start;
		memsegp[i].size = mem_clusters[i].size & ~PAGE_MASK;
	}

	/*
	 * If we have dump memory then assume the kernel stack is in high
	 * memory and bounce
	 */
	if (dumpmem_vaddr != 0) {
		bcopy(buf, (char *)dumpmem_vaddr, sizeof(buf));
		va = (caddr_t)dumpmem_vaddr;
	} else {
		va = (caddr_t)buf;
	}
	return (dump(dumpdev, dumplo, va, dbtob(1)));
}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf(void)
{
	int nblks, dumpblks;	/* size of dump area */

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		return;
	dumpblks += ctod(cpu_dump_mempagecnt());

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1)))
		return;

	/* Put dump at end of partition */
	dumplo = nblks - dumpblks;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = cpu_dump_mempagecnt();
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP  MAXPHYS /* must be a multiple of pagesize */

void
dumpsys(void)
{
	u_long totalbytesleft, bytes, i, n, memseg;
	u_long maddr;
	daddr_t blkno;
	void *va;
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

#ifdef UVM_SWAP_ENCRYPT
	uvm_swap_finicrypt_all();
#endif

	error = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (error == -1) {
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
			if ((totalbytesleft % (1024*1024)) < BYTES_PER_DUMP)
				printf("%ld ", totalbytesleft / (1024 * 1024));

			/* Limit size for next transfer. */
			n = bytes - i;
			if (n > BYTES_PER_DUMP)
				n = BYTES_PER_DUMP;
			if (maddr > 0xffffffff) {
				va = (void *)dumpmem_vaddr;
				if (n > dumpmem_sz)
					n = dumpmem_sz;
				bcopy((void *)PMAP_DIRECT_MAP(maddr), va, n);
			} else {
				va = (void *)PMAP_DIRECT_MAP(maddr);
			}

			error = (*dump)(dumpdev, blkno, va, n);
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
	struct trapframe *tf;

	/* If we were using the FPU, forget about it. */
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 0);
	p->p_md.md_flags &= ~MDP_USEDFPU;
	p->p_addr->u_pcb.pcb_fsbase = 0;

	tf = p->p_md.md_regs;
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_gs = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_rdi = 0;
	tf->tf_rsi = 0;
	tf->tf_rbp = 0;
	tf->tf_rbx = 0;
	tf->tf_rdx = 0;
	tf->tf_rcx = 0;
	tf->tf_rax = 0;
	tf->tf_r8 = 0;
	tf->tf_r9 = 0;
	tf->tf_r10 = 0;
	tf->tf_r11 = 0;
	tf->tf_r12 = 0;
	tf->tf_r13 = 0;
	tf->tf_r14 = 0;
	tf->tf_r15 = 0;
	tf->tf_rip = pack->ep_entry;
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_rflags = PSL_USERSET;
	tf->tf_rsp = stack;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);

	retval[1] = 0;
}

/*
 * Initialize segments and descriptor tables
 */

struct gate_descriptor *idt;
char idt_allocmap[NIDT];
char *gdtstore;
extern  struct user *proc0paddr;

void
setgate(struct gate_descriptor *gd, void *func, int ist, int type, int dpl,
    int sel)
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
unsetgate(struct gate_descriptor *gd)
{
	memset(gd, 0, sizeof (*gd));
}

void
setregion(struct region_descriptor *rd, void *base, u_int16_t limit)
{
	rd->rd_limit = limit;
	rd->rd_base = (u_int64_t)base;
}

/*
 * Note that the base and limit fields are ignored in long mode.
 */
void
set_mem_segment(struct mem_segment_descriptor *sd, void *base, size_t limit,
    int type, int dpl, int gran, int def32, int is64)
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
set_sys_segment(struct sys_segment_descriptor *sd, void *base, size_t limit,
    int type, int dpl, int gran)
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

void cpu_init_idt(void)
{
	struct region_descriptor region;

	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
	lidt(&region); 
}

void
cpu_init_extents(void)
{
	extern struct extent *iomem_ex;
	static int already_done;
	int i;

	/* We get called for each CPU, only first should do this */
	if (already_done)
		return;

	/*
	 * Allocate the physical addresses used by RAM from the iomem
	 * extent map.
	 */
	for (i = 0; i < mem_cluster_cnt; i++) {
		if (extent_alloc_region(iomem_ex, mem_clusters[i].start,
		    mem_clusters[i].size, EX_NOWAIT)) {
			/* XXX What should we do? */
			printf("WARNING: CAN'T ALLOCATE RAM (%llx-%llx)"
			    " FROM IOMEM EXTENT MAP!\n", mem_clusters[i].start,
			    mem_clusters[i].start + mem_clusters[i].size - 1);
		}
	}

	already_done = 1;
}

#if defined(MULTIPROCESSOR) || \
    (NACPI > 0 && !defined(SMALL_KERNEL))
void
map_tramps(void)
{
	struct pmap *kmp = pmap_kernel();

	pmap_kenter_pa(lo32_vaddr, lo32_paddr, PROT_READ | PROT_WRITE);

	/*
	 * The initial PML4 pointer must be below 4G, so if the
	 * current one isn't, use a "bounce buffer" and save it
	 * for tramps to use.
	 */
	if (kmp->pm_pdirpa > 0xffffffff) {
		memcpy((void *)lo32_vaddr, kmp->pm_pdir, PAGE_SIZE);
		tramp_pdirpa = lo32_paddr;
	} else
		tramp_pdirpa = kmp->pm_pdirpa;

	pmap_kremove(lo32_vaddr, PAGE_SIZE);

#ifdef MULTIPROCESSOR
	/* Map trampoline code page RW (to copy code) */
	pmap_kenter_pa((vaddr_t)MP_TRAMPOLINE,	/* virtual */
	    (paddr_t)MP_TRAMPOLINE,	/* physical */
	    PROT_READ | PROT_WRITE);	/* protection */
#endif /* MULTIPROCESSOR */

	/* Map trampoline code page RW (to copy code) */
	pmap_kenter_pa((vaddr_t)ACPI_TRAMPOLINE, /* virtual */
	    (paddr_t)ACPI_TRAMPOLINE,	/* physical */
	    PROT_READ | PROT_WRITE);	/* protection */
}
#endif

#define	IDTVEC(name)	__CONCAT(X, name)
typedef void (vector)(void);
extern vector *IDTVEC(exceptions)[];

void
init_x86_64(paddr_t first_avail)
{
	struct region_descriptor region;
	bios_memmap_t *bmp;
	int x, ist;

	cpu_init_msrs(&cpu_info_primary);

	proc0.p_addr = proc0paddr;
	cpu_info_primary.ci_curpcb = &proc0.p_addr->u_pcb;

	x86_bus_space_init();

	/*
	 * Attach the glass console early in case we need to display a panic.
	 */
	cninit();

	/*
	 * Initialize PAGE_SIZE-dependent variables.
	 */
	uvm_setpagesize();

	/*
	 * Boot arguments are in a single page specified by /boot.
	 *
	 * We require the "new" vector form, as well as memory ranges
	 * to be given in bytes rather than KB.
	 *
	 * locore copies the data into bootinfo[] for us.
	 */
	if ((bootapiver & (BAPIV_VECTOR | BAPIV_BMEMMAP)) ==
	    (BAPIV_VECTOR | BAPIV_BMEMMAP)) {
		if (bootinfo_size >= sizeof(bootinfo))
			panic("boot args too big");

		getbootinfo(bootinfo, bootinfo_size);
	} else
		panic("invalid /boot");

/*
 * Memory on the AMD64 port is described by three different things.
 *
 * 1. biosbasemem, biosextmem - These are outdated, and should really
 *    only be used to santize the other values.  They are the things
 *    we get back from the BIOS using the legacy routines, usually
 *    only describing the lower 4GB of memory.
 *
 * 2. bios_memmap[] - This is the memory map as the bios has returned
 *    it to us.  It includes memory the kernel occupies, etc.
 *
 * 3. mem_cluster[] - This is the massaged free memory segments after
 *    taking into account the contents of bios_memmap, biosbasemem,
 *    biosextmem, and locore/machdep/pmap kernel allocations of physical
 *    pages.
 *
 * The other thing is that the physical page *RANGE* is described by
 * three more variables:
 *
 * avail_start - This is a physical address of the start of available
 *               pages, until IOM_BEGIN.  This is basically the start
 *               of the UVM managed range of memory, with some holes...
 *
 * avail_end - This is the end of physical pages.  All physical pages
 *             that UVM manages are between avail_start and avail_end.
 *             There are holes...
 *
 * first_avail - This is the first available physical page after the
 *               kernel, page tables, etc.
 *
 * We skip the first few pages for trampolines, hibernate, and to avoid
 * buggy SMI implementations that could corrupt the first 64KB.
 */
	avail_start = 16*PAGE_SIZE;

#ifdef MULTIPROCESSOR
	if (avail_start < MP_TRAMPOLINE + PAGE_SIZE)
		avail_start = MP_TRAMPOLINE + PAGE_SIZE;
	if (avail_start < MP_TRAMP_DATA + PAGE_SIZE)
		avail_start = MP_TRAMP_DATA + PAGE_SIZE;
#endif

#if (NACPI > 0 && !defined(SMALL_KERNEL))
	if (avail_start < ACPI_TRAMPOLINE + PAGE_SIZE)
		avail_start = ACPI_TRAMPOLINE + PAGE_SIZE;
	if (avail_start < ACPI_TRAMP_DATA + PAGE_SIZE)
		avail_start = ACPI_TRAMP_DATA + PAGE_SIZE;
#endif

#ifdef HIBERNATE
	if (avail_start < HIBERNATE_HIBALLOC_PAGE + PAGE_SIZE)
		avail_start = HIBERNATE_HIBALLOC_PAGE + PAGE_SIZE;
#endif /* HIBERNATE */

	/*
	 * We need to go through the BIOS memory map given, and
	 * fill out mem_clusters and mem_cluster_cnt stuff, taking
	 * into account all the points listed above.
	 */ 
	avail_end = mem_cluster_cnt = 0;
	for (bmp = bios_memmap; bmp->type != BIOS_MAP_END; bmp++) {
		paddr_t s1, s2, e1, e2;

		/* Ignore non-free memory */
		if (bmp->type != BIOS_MAP_FREE)
			continue;
		if (bmp->size < PAGE_SIZE)
			continue;

		/* Init our segment(s), round/trunc to pages */
		s1 = round_page(bmp->addr);
		e1 = trunc_page(bmp->addr + bmp->size);
		s2 = e2 = 0;

		/*
		 * XXX Some buggy ACPI BIOSes use memory that they
		 * declare as free.  Typically the affected memory
		 * areas are small blocks between areas reserved for
		 * ACPI and other BIOS goo.  So skip areas smaller
		 * than 1 MB above the 16 MB boundary (to avoid
		 * affecting legacy stuff).
		 */
		if (s1 > 16*1024*1024 && (e1 - s1) < 1*1024*1024)
			continue;

		/* Check and adjust our segment(s) */
		/* Nuke low pages */
		if (s1 < avail_start) {
			s1 = avail_start;
			if (s1 > e1)
				continue;
		}

		/* Crop stuff into "640K hole" */
		if (s1 < IOM_BEGIN && e1 > IOM_BEGIN)
			e1 = IOM_BEGIN;
		if (s1 < biosbasemem && e1 > biosbasemem)
			e1 = biosbasemem;

		/* Split any segments straddling the 16MB boundary */
		if (s1 < 16*1024*1024 && e1 > 16*1024*1024) {
			e2 = e1;
			s2 = e1 = 16*1024*1024;
		}

		/* Store segment(s) */
		if (e1 - s1 >= PAGE_SIZE) {
			mem_clusters[mem_cluster_cnt].start = s1;
			mem_clusters[mem_cluster_cnt].size = e1 - s1;
			mem_cluster_cnt++;
		}
		if (e2 - s2 >= PAGE_SIZE) {
			mem_clusters[mem_cluster_cnt].start = s2;
			mem_clusters[mem_cluster_cnt].size = e2 - s2;
			mem_cluster_cnt++;
		}
		if (avail_end < e1) avail_end = e1;
		if (avail_end < e2) avail_end = e2;
	}

	/*
	 * Call pmap initialization to make new kernel address space.
	 * We must do this before loading pages into the VM system.
	 */
	first_avail = pmap_bootstrap(first_avail, trunc_page(avail_end));

	/* Allocate these out of the 640KB base memory */
	if (avail_start != PAGE_SIZE)
		avail_start = pmap_prealloc_lowmem_ptps(avail_start);

	cpu_init_extents();

	/* Make sure the end of the space used by the kernel is rounded. */
	first_avail = round_page(first_avail);
	kern_end = KERNBASE + first_avail;

	/*
	 * Now, load the memory clusters (which have already been
	 * flensed) into the VM system.
	 */
	for (x = 0; x < mem_cluster_cnt; x++) {
		paddr_t seg_start = mem_clusters[x].start;
		paddr_t seg_end = seg_start + mem_clusters[x].size;

		if (seg_start < first_avail) seg_start = first_avail;
		if (seg_start > seg_end) continue;
		if (seg_end - seg_start < PAGE_SIZE) continue;

		physmem += atop(mem_clusters[x].size);

#if DEBUG_MEMLOAD
		printf("loading 0x%lx-0x%lx (0x%lx-0x%lx)\n",
		    seg_start, seg_end, atop(seg_start), atop(seg_end));
#endif
		uvm_page_physload(atop(seg_start), atop(seg_end),
		    atop(seg_start), atop(seg_end), 0);
	}

	/*
         * Now, load the memory between the end of I/O memory "hole"
         * and the kernel.
	 */
	{
		paddr_t seg_start = round_page(IOM_END);
		paddr_t seg_end = trunc_page(KERNTEXTOFF - KERNBASE);

		if (seg_start < seg_end) {
#if DEBUG_MEMLOAD
			printf("loading 0x%lx-0x%lx\n", seg_start, seg_end);
#endif
			uvm_page_physload(atop(seg_start), atop(seg_end),
			    atop(seg_start), atop(seg_end), 0);
		}
	}

#if DEBUG_MEMLOAD
	printf("avail_start = 0x%lx\n", avail_start);
	printf("avail_end = 0x%lx\n", avail_end);
	printf("first_avail = 0x%lx\n", first_avail);
#endif

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
	 * Steal some memory for a dump bouncebuffer if we have memory over
	 * the 32-bit barrier.
	 */
	if (avail_end > 0xffffffff) {
		struct vm_physseg *vps = NULL;
		psize_t sz = round_page(MAX(BYTES_PER_DUMP, dbtob(1)));

		/* XXX assumes segments are ordered */
		for (x = 0; x < vm_nphysseg; x++) {
			vps = &vm_physmem[x];
			/* Find something between 16meg and 4gig */
			if (ptoa(vps->avail_end) <= 0xffffffff &&
			    ptoa(vps->avail_start) >= 0xffffff)
				break;
		}
		if (x == vm_nphysseg)
			panic("init_x86_64: no memory between "
			    "0xffffff-0xffffffff");

		/* Shrink so it'll fit in the segment. */
		if ((vps->avail_end - vps->avail_start) < atop(sz))
			sz = ptoa(vps->avail_end - vps->avail_start);

		vps->avail_end -= atop(sz);
		vps->end -= atop(sz);
		dumpmem_paddr = ptoa(vps->avail_end);
		dumpmem_vaddr = PMAP_DIRECT_MAP(dumpmem_paddr);
		dumpmem_sz = sz;

		/* Remove the last segment if it now has no pages. */
		if (vps->start == vps->end) {
			for (vm_nphysseg--; x < vm_nphysseg; x++)
				vm_physmem[x] = vm_physmem[x + 1];
		}
	}

	pmap_growkernel(VM_MIN_KERNEL_ADDRESS + 32 * 1024 * 1024);

	pmap_kenter_pa(idt_vaddr, idt_paddr, PROT_READ | PROT_WRITE);
	pmap_kenter_pa(idt_vaddr + PAGE_SIZE, idt_paddr + PAGE_SIZE,
	    PROT_READ | PROT_WRITE);

#if defined(MULTIPROCESSOR) || \
    (NACPI > 0 && !defined(SMALL_KERNEL))
	map_tramps();
#endif

	idt = (struct gate_descriptor *)idt_vaddr;
	cpu_info_primary.ci_tss = (void *)(idt + NIDT);
	gdtstore = (void *)(cpu_info_primary.ci_tss + 1);

	/* make gdt gates and memory segments */
	set_mem_segment(GDT_ADDR_MEM(gdtstore, GCODE_SEL), 0,
	    0xfffff, SDT_MEMERA, SEL_KPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GDATA_SEL), 0,
	    0xfffff, SDT_MEMRWA, SEL_KPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUCODE32_SEL), 0,
	    atop(VM_MAXUSER_ADDRESS32) - 1, SDT_MEMERA, SEL_UPL, 1, 1, 0);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUDATA_SEL), 0,
	    atop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMRWA, SEL_UPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUCODE_SEL), 0,
	    atop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMERA, SEL_UPL, 1, 0, 1);

	set_sys_segment(GDT_ADDR_SYS(gdtstore, GPROC0_SEL),
	    cpu_info_primary.ci_tss, sizeof (struct x86_64_tss)-1,
	    SDT_SYS386TSS, SEL_KPL, 0);

	/* exceptions */
	for (x = 0; x < 32; x++) {
		ist = (x == 8) ? 1 : 0;
		setgate(&idt[x], IDTVEC(exceptions)[x], ist, SDT_SYS386IGT,
		    (x == 3 || x == 4) ? SEL_UPL : SEL_KPL,
		    GSEL(GCODE_SEL, SEL_KPL));
		idt_allocmap[x] = 1;
	}

	setregion(&region, gdtstore, GDT_SIZE - 1);
	lgdt(&region);

	cpu_init_idt();

	intr_default_setup();
	fpuinit(&cpu_info_primary);

	softintr_init();
	splraise(IPL_IPI);
	enable_intr();

#ifdef DDB
	db_machine_init();
	ddb_init();
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
}

#ifdef KGDB
void
kgdb_port_init(void)
{
#if NCOM > 0
	if (!strcmp(kgdb_devname, "com")) {
		bus_space_tag_t tag = X86_BUS_SPACE_IO;
		com_kgdb_attach(tag, comkgdbaddr, comkgdbrate, COM_FREQ,
		    comkgdbmode);
	}
#endif
} 
#endif /* KGDB */

void
cpu_reset(void)
{

	disable_intr();

	if (cpuresetfn)
		(*cpuresetfn)();

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
	__asm volatile("divl %0,%1" : : "q" (0), "a" (0)); 

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
cpu_dumpsize(void)
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
cpu_dump_mempagecnt(void)
{
	u_long i, n;

	n = 0;
	for (i = 0; i < mem_cluster_cnt; i++)
		n += atop(mem_clusters[i].size);
	return (n);
}

/*
 * Figure out which portions of memory are used by the kernel/system.
 */
int
amd64_pa_used(paddr_t addr)
{
	struct vm_page	*pg;

	/* Kernel manages these */
	if ((pg = PHYS_TO_VM_PAGE(addr)) && (pg->pg_flags & PG_DEV) == 0)
		return 1;

	/* Kernel is loaded here */
	if (addr > IOM_END && addr < (kern_end - KERNBASE))
		return 1;

	/* Low memory used for various bootstrap things */
	if (addr < avail_start)
		return 1;

	/*
	 * The only regions I can think of that are left are the things
	 * we steal away from UVM.  The message buffer?
	 * XXX - ignore these for now.
	 */

	return 0;
}

void
cpu_initclocks(void)
{
	(*initclock_func)();
}

void
need_resched(struct cpu_info *ci)
{
	ci->ci_want_resched = 1;

	/* There's a risk we'll be called before the idle threads start */
	if (ci->ci_curproc) {
		aston(ci->ci_curproc);
		cpu_kick(ci);
	}
}

/*
 * Allocate an IDT vector slot within the given range.
 * XXX needs locking to avoid MP allocation races.
 */

int
idt_vec_alloc(int low, int high)
{
	int vec;

	for (vec = low; vec <= high; vec++) {
		if (idt_allocmap[vec] == 0) {
			idt_allocmap[vec] = 1;
			return vec;
		}
	}
	return 0;
}

void
idt_vec_set(int vec, void (*function)(void))
{
	/*
	 * Vector should be allocated, so no locking needed.
	 */
	KASSERT(idt_allocmap[vec] == 1);
	setgate(&idt[vec], function, 0, SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
}

void
idt_vec_free(int vec)
{
	unsetgate(&idt[vec]);
	idt_allocmap[vec] = 0;
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int cpl = curcpu()->ci_ilevel;

	if (cpl < wantipl) {
		splassert_fail(wantipl, cpl, func);
	}

	if (wantipl == IPL_NONE && curcpu()->ci_idepth != 0) {
		splassert_fail(-1, curcpu()->ci_idepth, func);
	}
}
#endif

void
getbootinfo(char *bootinfo, int bootinfo_size)
{
	bootarg32_t *q;
	bios_ddb_t *bios_ddb;
	bios_bootduid_t *bios_bootduid;
	bios_bootsr_t *bios_bootsr;

#undef BOOTINFO_DEBUG
#ifdef BOOTINFO_DEBUG
	printf("bootargv:");
#endif

	for (q = (bootarg32_t *)bootinfo;
	    (q->ba_type != BOOTARG_END) &&
	    ((((char *)q) - bootinfo) < bootinfo_size);
	    q = (bootarg32_t *)(((char *)q) + q->ba_size)) {

		switch (q->ba_type) {
		case BOOTARG_MEMMAP:
			bios_memmap = (bios_memmap_t *)q->ba_arg;
#ifdef BOOTINFO_DEBUG
			printf(" memmap %p", bios_memmap);
#endif
			break;
		case BOOTARG_DISKINFO:
			bios_diskinfo = (bios_diskinfo_t *)q->ba_arg;
#ifdef BOOTINFO_DEBUG
			printf(" diskinfo %p", bios_diskinfo);
#endif
			break;
		case BOOTARG_APMINFO:
			/* generated by i386 boot loader */
			break;
		case BOOTARG_CKSUMLEN:
			bios_cksumlen = *(u_int32_t *)q->ba_arg;
#ifdef BOOTINFO_DEBUG
			printf(" cksumlen %d", bios_cksumlen);
#endif
			break;
		case BOOTARG_PCIINFO:
			/* generated by i386 boot loader */
			break;
		case BOOTARG_CONSDEV:
			if (q->ba_size >= sizeof(bios_consdev_t) +
			    offsetof(struct _boot_args32, ba_arg)) {
				bios_consdev_t *cdp =
				    (bios_consdev_t*)q->ba_arg;
#if NCOM > 0
				static const int ports[] =
				    { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
				int unit = minor(cdp->consdev);
				int consaddr = cdp->consaddr;
				if (consaddr == -1 && unit >= 0 &&
				    unit < nitems(ports))
					consaddr = ports[unit];
				if (major(cdp->consdev) == 8 &&
				    consaddr != -1) {
					comconsunit = unit;
					comconsaddr = consaddr;
					comconsrate = cdp->conspeed;
					comconsiot = X86_BUS_SPACE_IO;

					/* Probe the serial port this time. */
					cninit();
				}
#endif
#ifdef BOOTINFO_DEBUG
				printf(" console 0x%x:%d",
				    cdp->consdev, cdp->conspeed);
#endif
			}
			break;
		case BOOTARG_BOOTMAC:
			bios_bootmac = (bios_bootmac_t *)q->ba_arg;
			break;

		case BOOTARG_DDB:
			bios_ddb = (bios_ddb_t *)q->ba_arg;
#ifdef DDB
			db_console = bios_ddb->db_console;
#endif
			break;

		case BOOTARG_BOOTDUID:
			bios_bootduid = (bios_bootduid_t *)q->ba_arg;
			bcopy(bios_bootduid, bootduid, sizeof(bootduid));
			break;

		case BOOTARG_BOOTSR:
			bios_bootsr = (bios_bootsr_t *)q->ba_arg;
#if NSOFTRAID > 0
			bcopy(&bios_bootsr->uuid, &sr_bootuuid,
			    sizeof(sr_bootuuid));
			bcopy(&bios_bootsr->maskkey, &sr_bootkey,
			    sizeof(sr_bootkey));
#endif
			explicit_bzero(bios_bootsr, sizeof(bios_bootsr_t));
			break;

		default:
#ifdef BOOTINFO_DEBUG
			printf(" unsupported arg (%d) %p", q->ba_type,
			    q->ba_arg);
#endif
			break;
		}
	}
#ifdef BOOTINFO_DEBUG
	printf("\n");
#endif
}

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
