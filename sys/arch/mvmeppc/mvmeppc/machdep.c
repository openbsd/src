/*	$OpenBSD: machdep.c,v 1.63 2009/08/22 02:54:50 mk Exp $	*/
/*	$NetBSD: machdep.c,v 1.4 1996/10/16 19:33:11 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/timeout.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <sys/syslog.h>
#include <sys/extent.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <net/netisr.h>

#include <machine/bat.h>
#include <machine/bugio.h>
#include <machine/pmap.h>
#include <machine/powerpc.h>
#include <machine/trap.h>
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/conf.h>
#include <machine/pio.h>
#include <machine/prom.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

void initppc(u_int, u_int, char *);
void dumpsys(void);
int lcsplx(int);
void myetheraddr(u_char *);

/*
 * Global variables used here and there
 */
struct pcb *curpcb;
struct pmap *curpm;
struct proc *fpuproc;

extern struct user *proc0paddr;

/* 
 * This is to fake out the console routines, while booting.
 */
cons_decl(boot);
#define bootcnpollc nullcnpollc

static struct consdev bootcons = {
	NULL, 
	NULL, 
	bootcngetc, 
	bootcnputc,
	bootcnpollc,
	NULL,
	makedev(14, 0), 
	CN_LOWPRI,
};

/* 
 * Declare these as initialized data so we can patch them.
 */
#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef BUFPAGES
int bufpages = BUFPAGES;
#else
int bufpages = 0;
#endif
int bufcachepercent = BUFCACHEPERCENT;

struct bat battable[16];

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

int ppc_malloc_ok;

#ifndef SYS_TYPE
/* XXX Hardwire it for now */
#define SYS_TYPE MVME
#endif

int system_type = SYS_TYPE;	/* XXX Hardwire it for now */

struct firmware *fw = NULL;
extern struct firmware ppc1_firmware;

/*
 * Extent maps to manage I/O. Allocate storage for 8 regions in each,
 * initially. Later devio_malloc_safe will indicate that it's safe to
 * use malloc() to dynamically allocate region descriptors.
 */
static long devio_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];
struct extent *devio_ex;
static int devio_malloc_safe = 0;

void
initppc(startkernel, endkernel, args)
	u_int startkernel, endkernel;
	char *args;
{
	extern void *trapcode; extern int trapsize;
	extern void *dsitrap; extern int dsisize;
	extern void *isitrap; extern int isisize;
	extern void *alitrap; extern int alisize;
	extern void *decrint; extern int decrsize;
	extern void *tlbimiss; extern int tlbimsize;
	extern void *tlbdlmiss; extern int tlbdlmsize;
	extern void *tlbdsmiss; extern int tlbdsmsize;
#ifdef DDB
	extern void *ddblow; extern int ddbsize;
#endif 
	extern void consinit(void);
	extern void *msgbuf_addr;
	int exc, scratch;

	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);
		
	fw = &ppc1_firmware; /*  Just PPC1-Bug for now... */
	buginit();

	curpcb = &proc0paddr->u_pcb;
	
	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = pmap_kernel();

	/* startup fake console driver.  It will be replaced by consinit() */
	cn_tab = &bootcons;

	/*
	 * Initialize BAT registers to unmapped to not generate
	 * overlapping mappings below.
	 */
	ppc_mtibat0u(0);
	ppc_mtibat1u(0);
	ppc_mtibat2u(0);
	ppc_mtibat3u(0);
	ppc_mtdbat0u(0);
	ppc_mtdbat1u(0);
	ppc_mtdbat2u(0);
	ppc_mtdbat3u(0);
	
	/*
	 * Set up initial BAT table
	 */
	battable[0].batl = BATL(0x00000000, BAT_M);
	battable[0].batu = BATU(0x00000000);
	
	/*
	 * Now setup fixed bat registers
	 *
	 * Note that we still run in real mode, and the BAT
	 * registers were cleared above.
	 */
	/* IBAT0 used for initial 256 MB segment */
	ppc_mtibat0l(battable[0].batl);
	ppc_mtibat0u(battable[0].batu);

	/* DBAT0 used similar */
	ppc_mtdbat0l(battable[0].batl);
	ppc_mtdbat0u(battable[0].batu);

	/*
	 * Set up trap vectors
	 */
	for (exc = EXC_RSVD; exc <= EXC_LAST; exc += 0x100) {
		switch (exc) {
		default:
			bcopy(&trapcode, (void *)exc, (size_t)&trapsize);
			break;
		case EXC_EXI:
			/*
			 * This one is (potentially) installed during autoconf
			 */
			break;

		case EXC_DSI:
			bcopy(&dsitrap, (void *)exc, (size_t)&dsisize);
			break;
		case EXC_ISI:
			bcopy(&isitrap, (void *)exc, (size_t)&isisize);
			break;
		case EXC_ALI:
			bcopy(&alitrap, (void *)exc, (size_t)&alisize);
			break;
		case EXC_DECR:
			bcopy(&decrint, (void *)exc, (size_t)&decrsize);
			break;
		case EXC_IMISS:
			bcopy(&tlbimiss, (void *)exc, (size_t)&tlbimsize);
			break;
		case EXC_DLMISS:
			bcopy(&tlbdlmiss, (void *)exc, (size_t)&tlbdlmsize);
			break;
		case EXC_DSMISS:
			bcopy(&tlbdsmiss, (void *)exc, (size_t)&tlbdsmsize);
			break;
#ifdef DDB
		case EXC_PGM:
		case EXC_TRC:
		case EXC_BPT:
			bcopy(&ddblow, (void *)exc, (size_t)&ddbsize);
			break;
#endif
		}
	}

	/* Grr, ALTIVEC_UNAVAIL is a vector not ~0xff aligned: 0x0f20 */
	bcopy(&trapcode, (void *)0xf20, (size_t)&trapsize);

	/*
	 * since trapsize is > 0x20, we just overwrote the EXC_PERF handler
	 * since we do not use it, we will "share" it with the EXC_VEC,
	 * we dont support EXC_VEC either.
	 * should be a 'ba 0xf20 written' at address 0xf00, but we
	 * do not generate EXC_PERF exceptions...
	 */

	syncicache((void *)EXC_RST, EXC_LAST - EXC_RST + 0x100);
	
	/*
	 * Initialize pmap module.
	 */
	uvmexp.pagesize = 4096;
	uvm_setpagesize();
	pmap_bootstrap(startkernel, endkernel);

#if 1
	/* MVME2[67]00 max out at 256MB, and we need BAT2 for now. */
#else
	/* use BATs to map 1GB memory, no pageable BATs now */
	if (physmem > atop(0x10000000)) {
		ppc_mtdbat1l(BATL(0x10000000, BAT_M));
		ppc_mtdbat1u(BATU(0x10000000));
	}
	if (physmem > atop(0x20000000)) {
		ppc_mtdbat2l(BATL(0x20000000, BAT_M));
		ppc_mtdbat2u(BATU(0x20000000));
	}
	if (physmem > atop(0x30000000)) {
		ppc_mtdbat3l(BATL(0x30000000, BAT_M));
		ppc_mtdbat3u(BATU(0x30000000));
	}
#endif

	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 * This will also start using the exception vector prefix of 0x000.
	 */
	(fw->vmon)();

	__asm__ volatile ("eieio; mfmsr %0; ori %0,%0,%1; mtmsr %0; sync;isync"
		      : "=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI));

	/*
	 * use the memory provided by pmap_bootstrap for message buffer
	 */
	initmsgbuf(msgbuf_addr, MSGBUFSIZE);

#ifdef DDB
#ifdef notyet
	db_machine_init();
#endif
	ddb_init();
#endif
	
	/*
	 * Set up extents for pci mappings
	 * Is this too late?
	 * 
	 * what are good start and end values here??
	 * 0x0 - 0x80000000 mcu bus
	 * MAP A				MAP B
	 * 0x80000000 - 0xbfffffff io		0x80000000 - 0xefffffff mem
	 * 0xc0000000 - 0xffffffff mem		0xf0000000 - 0xffffffff io
	 * 
	 * of course bsd uses 0xe and 0xf
	 * So the BSD PPC memory map will look like this
	 * 0x0 - 0x80000000 memory (whatever is filled)
	 * 0x80000000 - 0xdfffffff (pci space, memory or io)
	 * 0xe0000000 - kernel vm segment
	 * 0xf0000000 - kernel map segment (user space mapped here)
	 */

	devio_ex = extent_create("devio", 0x80000000, 0xffffffff, M_DEVBUF,
		(caddr_t)devio_ex_storage, sizeof(devio_ex_storage),
		EX_NOCOALESCE|EX_NOWAIT);

	/*
	 * Now we can set up the console as mapping is enabled.
	 */
	consinit();
	
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}

#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

void
install_extint(handler)
	void (*handler)(void);
{
	extern caddr_t extint, extsize;
	extern u_long extint_call;
	u_long offset = (u_long)handler - (u_long)&extint_call;
	int msr;
	
#ifdef	DIAGNOSTIC
	if (offset > 0x1ffffff)
		panic("install_extint: too far away");
#endif
	msr = ppc_intr_disable();
	extint_call = (extint_call & 0xfc000003) | offset;
	bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
	syncicache((void *)&extint_call, sizeof extint_call);
	syncicache((void *)EXC_EXI, (int)&extsize);
	ppc_intr_enable(msr);
}

/*
 * Machine dependent startup code.
 */
void
cpu_startup()
{
	vaddr_t minaddr, maxaddr;

	proc0.p_addr = proc0paddr;

	printf("%s", version);
	
	printf("real mem = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem)/1024/1024);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr, 16 * NCARGS,
	    VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);
	ppc_malloc_ok = 1;
	
	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free) / 1024 / 1024);

	/*
	 * Set up the buffers.
	 */
	bufinit();

	/*
	 * Set up early mappings
	 */
	devio_malloc_safe = 1;
	nvram_map();
	prep_bus_space_init();	
}

/*
 * consinit
 * Initialize system console.
 */
void
consinit()
{
	static int cons_initted = 0;

	if (cons_initted)
		return;
	cn_tab = NULL;
	cninit();
	cons_initted = 1;
}

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
	u_int32_t newstack;
	u_int32_t pargs;
	u_int32_t args[4];

	struct trapframe *tf = trapframe(p);
	pargs = -roundup(-stack + 8, 16);
	newstack = (u_int32_t)(pargs - 32);

	copyin ((void *)(VM_MAX_ADDRESS-0x10), &args, 0x10);
	
	bzero(tf, sizeof *tf);
	tf->fixreg[1] = newstack;
	tf->fixreg[3] = retval[0] = args[1];	/* XXX */
	tf->fixreg[4] = retval[1] = args[0];	/* XXX */
	tf->fixreg[5] = args[2];		/* XXX */
	tf->fixreg[6] = args[3];		/* XXX */
	tf->srr0 = pack->ep_entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	p->p_addr->u_pcb.pcb_flags = 0;
}

/*
 * Send a signal to process.
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
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oldonstack;
	
	frame.sf_signum = sig;
	
	tf = trapframe(p);
	oldonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	
	/*
	 * Allocate stack space for signal handler.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK)
	    && !oldonstack
	    && (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp
					 + psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)tf->fixreg[1];

	fp = (struct sigframe *)((int)(fp - 1) & ~0xf);
	
	/*
	 * Generate signal context for SYS_sigreturn.
	 */
	frame.sf_sc.sc_onstack = oldonstack;
	frame.sf_sc.sc_mask = mask;
	frame.sf_sip = NULL;
	bcopy(tf, &frame.sf_sc.sc_frame, sizeof *tf);
	if (psp->ps_siginfo & sigmask(sig)) {
		frame.sf_sip = &fp->sf_si;
		initsiginfo(&frame.sf_si, sig, code, type, val);
	}
	if (copyout(&frame, fp, sizeof frame) != 0)
		sigexit(p, SIGILL);
	

	tf->fixreg[1] = (int)fp;
	tf->lr = (int)catcher;
	tf->fixreg[3] = (int)sig;
	tf->fixreg[4] = (psp->ps_siginfo & sigmask(sig)) ? (int)&fp->sf_si : 0;
	tf->fixreg[5] = (int)&fp->sf_sc;
	tf->srr0 = p->p_sigcode;

#if WHEN_WE_ONLY_FLUSH_DATA_WHEN_DOING_PMAP_ENTER
	pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),tf->srr0, &pa);
	syncicache(pa, (p->p_emul->e_esigcode - p->p_emul->e_sigcode));
#endif
}

/*
 * System call to cleanup state after a signal handler returns.
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
	struct sigcontext sc;
	struct trapframe *tf;
	int error;
	
	if ((error = copyin(SCARG(uap, sigcntxp), &sc, sizeof sc)) != 0)
		return error;
	tf = trapframe(p);
	if ((sc.sc_frame.srr1 & PSL_USERSTATIC) != (tf->srr1 & PSL_USERSTATIC))
		return EINVAL;
	bcopy(&sc.sc_frame, tf, sizeof *tf);
	if (sc.sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = sc.sc_mask & ~sigcantmask;
	return EJUSTRETURN;
}

/*
 * Machine dependent system variables.
 * None for now.
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
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return ENOTDIR;
	switch (name[0]) {
	default:
		return EOPNOTSUPP;
	}
}

void
dumpsys()
{
	printf("dumpsys: TBD\n");
}

volatile int cpl, ipending, astpending;
int imask[IPL_NUM];
int netisr;

/*
 * Soft networking interrupts.
 */
void
softnet(isr)
	int isr;
{
#define	DONETISR(flag, func) \
	if (isr & (1 << (flag))) \
		(func)();

#include <net/netisr_dispatch.h>
#undef	DONETISR
}

int
lcsplx(ipl)
	int ipl;
{
	int oldcpl;

	oldcpl = cpl;
	splx(ipl);
	return oldcpl;
}

/*
 * Halt or reboot the machine after syncing/dumping according to howto.
 */
void
boot(howto)
	int howto;
{
	static int syncing;
	static char str[256];

	boothowto = howto;
	if (!cold && !(howto & RB_NOSYNC) && !syncing) {
		syncing = 1;
		vfs_shutdown();		/* sync */
		
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

	uvm_shutdown();
	splhigh();
	if (howto & RB_HALT) {
		doshutdownhooks();
		printf("halted\n\n");
		(fw->exit)();
	}
	if (!cold && (howto & RB_DUMP))
		dumpsys();
	doshutdownhooks();
	printf("rebooting\n\n");

	(fw->boot)(str);
	for (;;) ;	/* spinning */
}

/*
 *  Get Ethernet address for the onboard ethernet chip.
 */
void
myetheraddr(cp)
	u_char *cp;
{
	struct mvmeprom_brdid brdid;

	mvmeprom_brdid(&brdid);
	bcopy(&brdid.etheraddr, cp, 6);
}

typedef void  (void_f) (void);
void_f *pending_int_f = NULL;

/* call the bus/interrupt controller specific pending interrupt handler
 * would be nice if the offlevel interrupt code was handled here
 * instead of being in each of the specific handler code
 */
void
do_pending_int()
{
	if (pending_int_f != NULL) {
		(*pending_int_f)();
	}
}

/* 
 * one attempt at interrupt stuff..
 *
 */
#include <dev/pci/pcivar.h>
typedef void     *(intr_establish_t)(void *, pci_intr_handle_t,
            int, int, int (*func)(void *), void *, char *);
typedef void     (intr_disestablish_t)(void *, void *);

int ppc_configed_intr_cnt = 0;
struct intrhand ppc_configed_intr[MAX_PRECONF_INTR];

void *ppc_intr_establish(void *, pci_intr_handle_t, int, int, int (*)(void *),
    void *, const char *);
void ppc_intr_setup(intr_establish_t *, intr_disestablish_t *);
void ppc_intr_enable(int);
int ppc_intr_disable(void);

void *
ppc_intr_establish(lcv, ih, type, level, func, arg, name)
	void *lcv;
	pci_intr_handle_t ih;
	int type;
	int level;
	int (*func)(void *);
	void *arg;
	const char *name;
{
	if (ppc_configed_intr_cnt < MAX_PRECONF_INTR) {
		ppc_configed_intr[ppc_configed_intr_cnt].ih_fun = func;
		ppc_configed_intr[ppc_configed_intr_cnt].ih_arg = arg;
		ppc_configed_intr[ppc_configed_intr_cnt].ih_level = level;
		ppc_configed_intr[ppc_configed_intr_cnt].ih_irq = ih;
		ppc_configed_intr[ppc_configed_intr_cnt].ih_what = name;
		ppc_configed_intr_cnt++;
	} else {
		panic("ppc_intr_establish called before interrupt controller"
			" configured: driver %s has too many interrupts", name);
	}
	/* disestablish is going to be tricky to supported for these :-) */
	return (void *)ppc_configed_intr_cnt;
}

intr_establish_t *intr_establish_func = ppc_intr_establish;
intr_disestablish_t *intr_disestablish_func;

void
ppc_intr_setup(intr_establish_t *establish, intr_disestablish_t *disestablish)
{
	intr_establish_func = establish;
	intr_disestablish_func = disestablish;
}

vaddr_t ppc_kvm_stolen = VM_KERN_ADDRESS_SIZE;

void *
mapiodev(pa, len)
	paddr_t pa;
	psize_t len;
{
	paddr_t spa;
	vaddr_t vaddr, va;
	int off;
	int size;

	spa = trunc_page(pa);
	off = pa - spa;
	size = round_page(off+len);

	if (ppc_malloc_ok == 0) {
		/* need to steal vm space before kernel vm is initialized */
		va = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
		ppc_kvm_stolen += size;
		if (ppc_kvm_stolen > PPC_SEGMENT_LENGTH) {
			panic("ppc_kvm_stolen: out of space");
		}
	} else {
		va = uvm_km_valloc_wait(phys_map, size);
	}

	if (va == 0) 
		return NULL;

	for (vaddr = va; size > 0; size -= PAGE_SIZE) {
		pmap_kenter_cache(vaddr, spa,
			VM_PROT_READ | VM_PROT_WRITE, PMAP_CACHE_DEFAULT);
		spa += PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}
	return (void *) (va+off);
}
void 
unmapiodev(kva, p_size)
	void *kva;
	psize_t p_size;
{
	vaddr_t vaddr;
	int size;

	size = p_size;

	vaddr = trunc_page((vaddr_t)kva);

	uvm_km_free_wakeup(phys_map, vaddr, size);

	for (; size > 0; size -= PAGE_SIZE) {
		pmap_remove(pmap_kernel(), vaddr,  vaddr+PAGE_SIZE-1);
		vaddr += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

/* bcopy(), error on fault */
int
kcopy(from, to, size)
	const void *from;
	void *to;
	size_t size;
{
	faultbuf env;
	void *oldh = curproc->p_addr->u_pcb.pcb_onfault;

	if (setfault(&env)) {
		curproc->p_addr->u_pcb.pcb_onfault = oldh;
		return EFAULT;
	}
	bcopy(from, to, size);
	curproc->p_addr->u_pcb.pcb_onfault = oldh;

	return 0;
}
