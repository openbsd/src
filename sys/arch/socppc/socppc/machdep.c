/*	$OpenBSD: machdep.c,v 1.76 2018/07/16 08:53:44 jsg Exp $	*/
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
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/user.h>

#include <net/if.h>
#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/pio.h>
#include <powerpc/powerpc.h>
#include <machine/trap.h>

#include <dev/cons.h>

#include <dev/ic/comvar.h>

#include <dev/ofw/fdt.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

/*
 * Global variables used here and there
 */
extern struct user *proc0paddr;

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

int ppc_malloc_ok = 0;

char *bootpath;
char bootpathbuf[512];

struct bd_info {
	unsigned long	bi_memstart;
	unsigned long	bi_memsize;
	unsigned long	bi_flashstart;
	unsigned long	bi_flashsize;
	unsigned long	bi_flashoffset;
	unsigned long	bi_sramstart;
	unsigned long	bi_sramsize;
	unsigned long	bi_immr_base;
	unsigned long	bi_bootflags;
	unsigned long	bi_ip_addr;
	unsigned char	bi_enetaddr[6];
	unsigned long	bi_ethspeed;
} bootinfo;

extern struct bd_info **fwargsave;
extern struct fdt_head *fwfdtsave;

#ifdef APERTURE
int allowaperture = 0;
#endif

void dumpsys(void);
int lcsplx(int ipl);
void myetheraddr(u_char *);

/*
 * Extent maps to manage I/O. Allocate storage for 8 regions in each,
 * initially. Later devio_malloc_safe will indicate that it's safe to
 * use malloc() to dynamically allocate region descriptors.
 */
static long devio_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];
struct extent *devio_ex;
static int devio_malloc_safe = 0;

void initppc(u_int, u_int, char *);

void prom_printf(const char *, ...);

extern struct ppc_bus_space mainbus_bus_space;

void
initppc(u_int startkernel, u_int endkernel, char *args)
{
	extern void *trapcode; extern int trapsize;
	extern void *extint; extern int extsize;
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
	extern void *msgbuf_addr;
	int exc;
	void *node;

	extern char __bss_start[], __end[];
	bzero(__bss_start, __end - __bss_start);

	/* Make a copy of the args! */
	strlcpy(bootpathbuf, args ? args : "wd0a", sizeof bootpathbuf);

	if (fwfdtsave && fwfdtsave->fh_magic == FDT_MAGIC) {
		/* 
		 * Save the FDT firmware blob passed by the bootloader
		 * before we zero all memory.
		 * 
		 */
		void *fdt = (void *)endkernel;
		memcpy(fdt, fwfdtsave, fwfdtsave->fh_size);
		endkernel += fwfdtsave->fh_size;

		fdt_init(fdt);

		/*
		 * XXX Create a fake bootinfo structure if we were
		 * loaded by RouterBOOT.
		 */
		node = fdt_find_node("/memory");
		if (node) {
			char *reg;

			if (fdt_node_property(node, "reg", &reg)) {
				bootinfo.bi_memstart = *(u_int32_t *)reg;
				bootinfo.bi_memsize = *((u_int32_t *)reg + 1);
			}
		}
		node = fdt_find_node("/soc8343");
		if (node) {
			char *reg;

			if (fdt_node_property(node, "reg", &reg))
				bootinfo.bi_immr_base = *(u_int32_t *)reg;
		}
		node = fdt_find_node("/soc8343/ethernet");
		if (node) {
			char *addr;

			if (fdt_node_property(node, "mac-address", &addr))
				memcpy(bootinfo.bi_enetaddr, addr, 6);
		}
	} else {
		/*
		 * We were loaded by an old U-Boot that didn't provide
		 * a flattened device tree.  It should have provided a
		 * valid bootinfo structure which we'll use to build
		 * such a device tree ourselves.
		 *
		 * XXX We don't build a flattened device tree yet.
		 */
		memcpy(&bootinfo, *fwargsave, sizeof bootinfo);

		extern uint8_t dt_blob_start[];
		fdt_init(&dt_blob_start);
	}

	proc0.p_cpu = &cpu_info[0];
	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);

	curpcb = &proc0paddr->u_pcb;

	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = pmap_kernel();

	cpu_bootstrap();

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);

	/*
	 * Set up trap vectors
	 */
	for (exc = EXC_RSVD; exc <= EXC_LAST; exc += 0x100) {
		switch (exc) {
		default:
			bcopy(&trapcode, (void *)exc, (size_t)&trapsize);
			break;
		case EXC_EXI:
			bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
			break;
		case EXC_DSI:
			bcopy(&dsitrap, (void *)EXC_DSI, (size_t)&dsisize);
			break;
		case EXC_ISI:
			bcopy(&isitrap, (void *)EXC_ISI, (size_t)&isisize);
			break;
		case EXC_ALI:
			bcopy(&alitrap, (void *)EXC_ALI, (size_t)&alisize);
			break;
		case EXC_DECR:
			bcopy(&decrint, (void *)EXC_DECR, (size_t)&decrsize);
			break;
		case EXC_IMISS:
			bcopy(&tlbimiss, (void *)EXC_IMISS, (size_t)&tlbimsize);
			break;
		case EXC_DLMISS:
			bcopy(&tlbdlmiss, (void *)EXC_DLMISS, (size_t)&tlbdlmsize);
			break;
		case EXC_DSMISS:
			bcopy(&tlbdsmiss, (void *)EXC_DSMISS, (size_t)&tlbdsmsize);
			break;
		case EXC_PGM:
		case EXC_TRC:
		case EXC_BPT:
#if defined(DDB)
			bcopy(&ddblow, (void *)exc, (size_t)&ddbsize);
#endif
			break;
		}
	}

	syncicache((void *)EXC_RST, EXC_LAST - EXC_RST + 0x100);


	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 */
	pmap_enable_mmu();

	/*
	 * use the memory provided by pmap_bootstrap for message buffer
	 */
	initmsgbuf(msgbuf_addr, MSGBUFSIZE);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_AUTOBOOT;

	/*
	 * Parse arg string.
	 */
	bootpath = &bootpathbuf[0];
	while (*++bootpath && *bootpath != ' ');
	if (*bootpath) {
		*bootpath++ = 0;
		while (*bootpath) {
			switch (*bootpath++) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			case 'c':
				boothowto |= RB_CONFIG;
				break;
			default:
				break;
			}
		}
	}
	bootpath = &bootpathbuf[0];

#ifdef DDB
	ddb_init();
#endif

	/*
	 * Adjust base of internal memory mapped registers.
	 */
	mainbus_bus_space.bus_base = bootinfo.bi_immr_base;

	devio_ex = extent_create("devio", 0x80000000, 0xffffffff, M_DEVBUF,
		(caddr_t)devio_ex_storage, sizeof(devio_ex_storage),
		EX_NOCOALESCE|EX_NOWAIT);

	/*
	 * Initialize console.
	 */
	extern int comconsrate;
	comconsfreq = 266666666;
	comconsrate = 115200;
	comconsaddr = 0x00004500;
	comconsiot = &mainbus_bus_space;

	node = fdt_find_node("/chosen");
	if (node) {
		char *console;

		fdt_node_property(node, "linux,stdout-path", &console);
		node = fdt_find_node(console);
		if (node) {
			char *freq;
			char *reg;

			if (fdt_node_property(node, "clock-frequency", &freq))
				comconsfreq = *(u_int32_t *)freq;
			if (fdt_node_property(node, "reg", &reg))
				comconsaddr = *(u_int32_t *)reg;
		}
	}

	consinit();

#ifdef DDB
	if (boothowto & RB_KDB)
		db_enter();
#endif

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
}

void
dumpsys(void)
{
}

int cpu_imask[IPL_NUM];

int
lcsplx(int ipl)
{
	return spllower(ipl);
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

	printf("real mem = %lu (%luMB)\n", ptoa(physmem),
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

	devio_malloc_safe = 1;
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
	cninit();
	cons_initted = 1;
}

/*
 * Clear registers on exec
 */
void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	u_int32_t newstack;
	u_int32_t pargs;
	u_int32_t args[4];

	struct trapframe *tf = trapframe(p);
	pargs = -roundup(-stack + 8, 16);
	newstack = (u_int32_t)(pargs - 32);

	copyin ((void *)p->p_p->ps_strings, &args, 0x10);

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
sendsig(sig_t catcher, int sig, sigset_t mask, const siginfo_t *ksip)
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_p->ps_sigacts;

	bzero(&frame, sizeof(frame));
	frame.sf_signum = sig;

	tf = trapframe(p);

	/*
	 * Allocate stack space for signal handler.
	 */
	if ((p->p_sigstk.ss_flags & SS_DISABLE) == 0 &&
	    !sigonstack(tf->fixreg[1]) &&
	    (psp->ps_sigonstack & sigmask(sig)))
		fp = (struct sigframe *)
		    trunc_page((vaddr_t)p->p_sigstk.ss_sp + p->p_sigstk.ss_size);
	else
		fp = (struct sigframe *)tf->fixreg[1];

	fp = (struct sigframe *)((int)(fp - 1) & ~0xf);

	/*
	 * Generate signal context for SYS_sigreturn.
	 */
	frame.sf_sc.sc_mask = mask;
	frame.sf_sip = NULL;
	bcopy(tf, &frame.sf_sc.sc_frame, sizeof *tf);
	if (psp->ps_siginfo & sigmask(sig)) {
		frame.sf_sip = &fp->sf_si;
		frame.sf_si = *ksip;
	}
	frame.sf_sc.sc_cookie = (long)&fp->sf_sc ^ p->p_p->ps_sigcookie;
	if (copyout(&frame, fp, sizeof frame) != 0)
		sigexit(p, SIGILL);

	tf->fixreg[1] = (int)fp;
	tf->lr = (int)catcher;
	tf->fixreg[3] = (int)sig;
	tf->fixreg[4] = (psp->ps_siginfo & sigmask(sig)) ? (int)&fp->sf_si : 0;
	tf->fixreg[5] = (int)&fp->sf_sc;
	tf->srr0 = p->p_p->ps_sigcode;

#if WHEN_WE_ONLY_FLUSH_DATA_WHEN_DOING_PMAP_ENTER
	pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),tf->srr0, &pa);
	syncicache(pa, (p->p_p->ps_emul->e_esigcode -
	    p->p_p->ps_emul->e_sigcode));
#endif
}

/*
 * System call to cleanup state after a signal handler returns.
 */
int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext ksc, *scp = SCARG(uap, sigcntxp);
	struct trapframe *tf;
	int error;

	if (PROC_PC(p) != p->p_p->ps_sigcoderet) {
		sigexit(p, SIGILL);
		return (EPERM);
	}

	if ((error = copyin(scp, &ksc, sizeof ksc)))
		return error;

	if (ksc.sc_cookie != ((long)scp ^ p->p_p->ps_sigcookie)) {
		sigexit(p, SIGILL);
		return (EFAULT);
	}

	/* Prevent reuse of the sigcontext cookie */
	ksc.sc_cookie = 0;
	(void)copyout(&ksc.sc_cookie, (caddr_t)scp +
	    offsetof(struct sigcontext, sc_cookie), sizeof (ksc.sc_cookie));

	tf = trapframe(p);
	if ((ksc.sc_frame.srr1 & PSL_USERSTATIC) != (tf->srr1 & PSL_USERSTATIC))
		return EINVAL;
	bcopy(&ksc.sc_frame, tf, sizeof *tf);
	p->p_sigmask = ksc.sc_mask & ~sigcantmask;
	return EJUSTRETURN;
}

/*
 * Machine dependent system variables.
 * None for now.
 */
int
cpu_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return ENOTDIR;
	switch (name[0]) {
	case CPU_ALLOWAPERTURE:
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
	case CPU_ALTIVEC:
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
	default:
		return EOPNOTSUPP;
	}
}

u_long dumpmag = 0x04959fca;			/* magic number */
int dumpsize = 0;			/* size of dump in pages */
long dumplo = -1;			/* blocks */

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void dumpconf(void);

void
dumpconf(void)
{
	int nblks;	/* size of dump area */
	int i;

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	/* Always skip the first block, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

        for (i = 0; i < ndumpmem; i++)
		dumpsize = max(dumpsize, dumpmem[i].end);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo - 1))
		dumpsize = dtoc(nblks - dumplo - 1);
	if (dumplo < nblks - ctod(dumpsize) - 1)
		dumplo = nblks - ctod(dumpsize) - 1;

}

#define BYTES_PER_DUMP  (PAGE_SIZE)  /* must be a multiple of pagesize */
static vaddr_t dumpspace;

int
reserve_dumppages(caddr_t p)
{
	dumpspace = (vaddr_t)p;
	return BYTES_PER_DUMP;
}

/*
 * Halt or reboot the machine after syncing/dumping according to howto.
 */
__dead void
boot(int howto)
{
	static int syncing;

	if (cold) {
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && !syncing) {
		syncing = 1;
		vfs_shutdown(curproc);

		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}
	if_downall();

	uvm_shutdown();
	splhigh();
	cold = 1;

	if ((howto & RB_DUMP) != 0)
		dumpsys();

haltsys:
	config_suspend_all(DVACT_POWERDOWN);

	if ((howto & RB_HALT) != 0) {
		if ((howto & RB_POWERDOWN) != 0) {
			;
		}

		printf("halted\n\n");
		for (;;)
			continue;
		/* NOTREACHED */
	}
	printf("rebooting\n\n");

	{
		volatile int32_t *reset;
		int32_t v;

		reset = mapiodev(0xe0000900, 0x100);
		reset[6] = 0x52535445;
		v = reset[6];
		reset[7] = 0x00000002;
	}

	printf("boot failed, spinning\n");
	for (;;)
		continue;
	/* NOTREACHED */
}

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
void
signotify(struct proc *p)
{
	aston(p);
}

/* bcopy(), error on fault */
int
kcopy(const void *from, void *to, size_t size)
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

struct mem_region uboot_mem[2], uboot_avail[4];

void
ppc_mem_regions(struct mem_region **memp, struct mem_region **availp)
{
	uboot_mem[0].start = bootinfo.bi_memstart;
	uboot_mem[0].size = bootinfo.bi_memsize;

	/* Reserve memory used for exception vectors. */
	uboot_avail[0] = uboot_mem[0];
	if (uboot_mem[0].start < EXC_LAST + 0x100) {
		uboot_avail[0].size -= (EXC_LAST + 0x100 - uboot_mem[0].start);
		uboot_avail[0].start = EXC_LAST + 0x100;
	}

	*memp = uboot_mem;
	*availp = uboot_avail;
}

void
myetheraddr(u_char *cp)
{
	bcopy(bootinfo.bi_enetaddr, cp, sizeof bootinfo.bi_enetaddr);
	bootinfo.bi_enetaddr[5]++;
}

/* prototype for locore function */
void cpu_switchto_asm(struct proc *oldproc, struct proc *newproc);

void cpu_switchto(struct proc *oldproc, struct proc *newproc)
{
	cpu_switchto_asm(oldproc, newproc);
}
