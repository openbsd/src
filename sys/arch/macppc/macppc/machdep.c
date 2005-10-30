/*	$OpenBSD: machdep.c,v 1.81 2005/10/30 22:29:02 kettenis Exp $	*/
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
#include <sys/conf.h>
#include <sys/core.h>
#include <sys/kcore.h>

#include <uvm/uvm_extern.h>

#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#include <net/netisr.h>

#include <dev/cons.h>

#include <machine/bat.h>
#include <machine/pmap.h>
#include <machine/powerpc.h>
#include <machine/trap.h>
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/pio.h>
#include <machine/intr.h>

#include <dev/pci/pcivar.h>

#include <arch/macppc/macppc/ofw_machdep.h>
#include <dev/ofw/openfirm.h>

#include "adb.h"
#if NADB > 0
#include <arch/macppc/dev/adbvar.h>
#endif

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

/*
 * Global variables used here and there
 */
struct pcb *curpcb;
struct pmap *curpm;

extern struct user *proc0paddr;
struct pool ppc_vecpl;

/*
 * Declare these as initialized data so we can patch them.
 */
#ifdef NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif

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

int ppc_malloc_ok = 0;

#ifndef SYS_TYPE
/* XXX Hardwire it for now */
#define SYS_TYPE POWER4e
#endif

int system_type = SYS_TYPE;	/* XXX Hardwire it for now */

char ofw_eth_addr[6];		/* Save address of first network ifc found */
char *bootpath;
char bootpathbuf[512];

struct firmware *fw = NULL;

#ifdef DDB
void * startsym, *endsym;
#endif

#ifdef APERTURE
#ifdef INSECURE
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

void ofw_dbg(char *str);

caddr_t allocsys(caddr_t);
void dumpsys(void);
void systype(char *name);
int lcsplx(int ipl);	/* called from LCore */
int power4e_get_eth_addr(void);
void ppc_intr_setup(intr_establish_t *establish,
    intr_disestablish_t *disestablish);
void *ppc_intr_establish(void *lcv, pci_intr_handle_t ih, int type,
    int level, int (*func)(void *), void *arg, char *name);
int bus_mem_add_mapping(bus_addr_t bpa, bus_size_t size, int cacheable,
    bus_space_handle_t *bshp);
bus_addr_t bus_space_unmap_p(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t size);
void bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t size);


/*
 * Extent maps to manage I/O. Allocate storage for 8 regions in each,
 * initially. Later devio_malloc_safe will indicate that it's save to
 * use malloc() to dynamically allocate region descriptors.
 */
static long devio_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];
struct extent *devio_ex;
static int devio_malloc_safe = 0;

extern int OF_stdout;

/* XXX, called from asm */
void initppc(u_int startkernel, u_int endkernel, char *args);

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
	extern void callback(void *);
	extern void *msgbuf_addr;
	int exc, scratch;

	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);

	curpcb = &proc0paddr->u_pcb;

	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = pmap_kernel();

	ppc_check_procid();

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
	 * Set up initial BAT table to only map the lowest 256 MB area
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


	uvmexp.pagesize = 4096;
	uvm_setpagesize();

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);

	/* now that we know physmem size, map physical memory with BATs */
	if (physmem > btoc(0x10000000)) {
		battable[0x1].batl = BATL(0x10000000, BAT_M);
		battable[0x1].batu = BATU(0x10000000);
	}
	if (physmem > btoc(0x20000000)) {
		battable[0x2].batl = BATL(0x20000000, BAT_M);
		battable[0x2].batu = BATU(0x20000000);
	}
	if (physmem > btoc(0x30000000)) {
		battable[0x3].batl = BATL(0x30000000, BAT_M);
		battable[0x3].batu = BATU(0x30000000);
	}
	if (physmem > btoc(0x40000000)) {
		battable[0x4].batl = BATL(0x40000000, BAT_M);
		battable[0x4].batu = BATU(0x40000000);
	}
	if (physmem > btoc(0x50000000)) {
		battable[0x5].batl = BATL(0x50000000, BAT_M);
		battable[0x5].batu = BATU(0x50000000);
	}
	if (physmem > btoc(0x60000000)) {
		battable[0x6].batl = BATL(0x60000000, BAT_M);
		battable[0x6].batu = BATU(0x60000000);
	}
	if (physmem > btoc(0x70000000)) {
		battable[0x7].batl = BATL(0x70000000, BAT_M);
		battable[0x7].batu = BATU(0x70000000);
	}

	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 */
	(fw->vmon)();

	__asm__ volatile ("eieio; mfmsr %0; ori %0,%0,%1; mtmsr %0; sync;isync"
		      : "=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI));

	/*
	 * use the memory provided by pmap_bootstrap for message buffer
	 */
	initmsgbuf(msgbuf_addr, MSGBUFSIZE);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 * Default to SINGLE and ASKNAME if no args or
	 * SINGLE and DFLTROOT if this is a ramdisk kernel.
	 */
#ifdef RAMDISK_HOOKS
	boothowto = RB_SINGLE | RB_DFLTROOT;
#else
	boothowto = RB_AUTOBOOT;
#endif /* RAMDISK_HOOKS */

	/*
	 * Parse arg string.
	 */

	/* make a copy of the args! */
	strncpy(bootpathbuf, args, 512);
	bootpath= &bootpathbuf[0];
	while ( *++bootpath && *bootpath != ' ');
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
	bootpath= &bootpathbuf[0];

#ifdef DDB
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
	ofwconsinit();
	/* while using openfirmware, run userconfig */
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
	/*
	 * Replace with real console.
	 */
	ofwconprobe();
	consinit();

#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif

	/*
	 * Figure out ethernet address.
	 */
	(void)power4e_get_eth_addr();

        pool_init(&ppc_vecpl, sizeof(struct vreg), 16, 0, 0, "ppcvec", NULL);

}
void ofw_dbg(char *str)
{
	int i = strlen (str);
	OF_write(OF_stdout, str, i);
}


void
install_extint(void (*handler)(void))
{
	void extint(void);
	void extsize(void);
	extern u_long extint_call;
	u_long offset = (u_long)handler - (u_long)&extint_call;
	int omsr, msr;

#ifdef	DIAGNOSTIC
	if (offset > 0x1ffffff)
		panic("install_extint: too far away");
#endif
	omsr = ppc_mfmsr();
	msr = omsr & ~PSL_EE;
	ppc_mtmsr(msr);
	extint_call = (extint_call & 0xfc000003) | offset;
	bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
	syncicache((void *)&extint_call, sizeof extint_call);
	syncicache((void *)EXC_EXI, (int)&extsize);
	ppc_mtmsr(omsr);
}

/*
 * Machine dependent startup code.
 */
void
cpu_startup()
{
	int sz, i;
	caddr_t v;
	vm_offset_t minaddr, maxaddr;
	int base, residual;
	v = (caddr_t)proc0paddr + USPACE;

	proc0.p_addr = proc0paddr;

	printf("%s", version);

	printf("real mem = %d (%dK)\n", ctob(physmem), ctob(physmem)/1024);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);
	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	sz = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(sz),
	    NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE,
	    UVM_PROT_NONE, UVM_INH_NONE, UVM_ADV_NORMAL, 0)))
		panic("cpu_startup: cannot allocate VM for buffers");
	/*
	addr = (vaddr_t)buffers;
	*/
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE) {
		/* Don't want to alloc more physical mem than ever needed */
		base = MAXBSIZE;
		residual = 0;
	}
	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;
		struct vm_page *pg;

		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = PAGE_SIZE * (i < residual ? base + 1 : base);
		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for"
					" buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
					VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

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

	printf("avail mem = %ld (%ldK)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free) / 1024);
	printf("using %d buffers containing %d bytes of memory\n", nbuf,
	    bufpages * PAGE_SIZE);

	/*
	 * Set up the buffers.
	 */
	bufinit();

	devio_malloc_safe = 1;
}

/*
 * Allocate space for system data structures.
 */
caddr_t
allocsys(caddr_t v)
{
#define	valloc(name, type, num) \
	v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef	SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Decide on buffer space to use.
	 */
	if (bufpages == 0)
		bufpages = physmem * bufcachepercent / 100;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	/* Restrict to at most 35% filled kvm */
	if (nbuf >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 35 / 100)
		nbuf = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 35 / 100;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);

	return v;
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
sendsig(sig_t catcher, int sig, int mask, u_long code, int type,
    union sigval val)
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
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext sc;
	struct trapframe *tf;
	int error;

	if ((error = copyin(SCARG(uap, sigcntxp), &sc, sizeof sc)))
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
			return (sysctl_rdint(oldp, oldlenp, newp,
			    allowaperture));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	case CPU_ALTIVEC:
		return (sysctl_rdint(oldp, oldlenp, newp, ppc_altivec));
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
dumpconf()
{
	int nblks;	/* size of dump area */
	int maj;
	int i;


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
vaddr_t dumpspace;

int
reserve_dumppages(caddr_t p)
{
	dumpspace = (vaddr_t)p;
	return BYTES_PER_DUMP;
}

/*
 * cpu_dump: dump machine-dependent kernel core dump headers.
 */
int cpu_dump(void);
int
cpu_dump()
{
	int (*dump) (dev_t, daddr_t, caddr_t, size_t);
	long buf[dbtob(1) / sizeof (long)];
	kcore_seg_t	*segp;

	dump = bdevsw[major(dumpdev)].d_dump;

	segp = (kcore_seg_t *)buf;

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	return (dump(dumpdev, dumplo, (caddr_t)buf, dbtob(1)));
}

void
dumpsys()
{
#if 0
	u_int npg;
	u_int i, j;
	daddr_t blkno;
	int (*dump) (dev_t, daddr_t, caddr_t, size_t);
	char *str;
	int maddr;
	extern int msgbufmapped;
	int error;

	/* save registers */

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
	printf("dumping to dev %x, offset %ld\n", dumpdev, dumplo);

	error = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	if (error == -1) {
		printf("area unavailable\n");
		delay (10000000);
		return;
	}

	dump = bdevsw[major(dumpdev)].d_dump;
	error = cpu_dump();
	for (i = 0; !error && i < ndumpmem; i++) {
		npg = dumpmem[i].end - dumpmem[i].start;
		maddr = ctob(dumpmem[i].start);
		blkno = dumplo + btodb(maddr) + 1;

		for (j = npg; j;
			j--, maddr += PAGE_SIZE, blkno+= btodb(PAGE_SIZE))
		{
			/* Print out how many MBs we have to go. */
                        if (dbtob(blkno - dumplo) % (1024 * 1024) < NBPG)
                                printf("%d ",
                                    (ctob(dumpsize) - maddr) / (1024 * 1024));

			pmap_enter(pmap_kernel(), dumpspace, maddr,
				VM_PROT_READ, PMAP_WIRED);
			if ((error = (*dump)(dumpdev, blkno,
			    (caddr_t)dumpspace, PAGE_SIZE)) != 0)
				break;
		}
	}

	switch (error) {

	case 0:         str = "succeeded\n\n";                  break;
	case ENXIO:     str = "device bad\n\n";                 break;
	case EFAULT:    str = "device not ready\n\n";           break;
	case EINVAL:    str = "area improper\n\n";              break;
	case EIO:       str = "i/o error\n\n";                  break;
	case EINTR:     str = "aborted from console\n\n";       break;
	default:        str = "error %d\n\n";                   break;
	}
	printf(str, error);

#else
	printf("dumpsys() - no yet supported\n");
	
#endif
	delay(5000000);         /* 5 seconds */

}

volatile int cpl, ipending, astpending;
int imask[IPL_NUM];

/*
 * this is a hack interface to allow zs to work better until
 * a true soft interrupt mechanism is created.
 */
#include "zstty.h"
#if NZSTTY > 0
	extern void zssoft(void *);
#endif
void
softtty()
{
#if NZSTTY > 0
	zssoft(0);
#endif
}

int netisr;

/*
 * Soft networking interrupts.
 */
void
softnet(int isr)
{
#define DONETISR(flag, func) \
	if (isr & (1 << flag))\
		func();

#include <net/netisr_dispatch.h>
}

int
lcsplx(int ipl)
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
boot(int howto)
{
	static int syncing;

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
	splhigh();
	if (howto & RB_HALT) {
		doshutdownhooks();
		if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NADB > 0
			delay(1000000);
			adb_poweroff();
			printf("WARNING: adb powerdown failed!\n");
#endif
			OF_interpret("shut-down", 0);
		}

		printf("halted\n\n");
		(fw->exit)();
	}
	if (!cold && (howto & RB_DUMP))
		dumpsys();
	doshutdownhooks();
	printf("rebooting\n\n");

#if NADB > 0
	adb_restart();  /* not return */
#endif

	OF_interpret("reset-all", 0);
	OF_exit();
	printf("boot failed, spinning\n");
	while(1) /* forever */;
}

/*
 *  Get Ethernet address for the onboard ethernet chip.
 */
int
power4e_get_eth_addr()
{
	int qhandle, phandle;
	char name[32];

	for (qhandle = OF_peer(0); qhandle; qhandle = phandle) {
		if (OF_getprop(qhandle, "device_type", name, sizeof name) >= 0
		    && !strcmp(name, "network")
		    && OF_getprop(qhandle, "local-mac-address",
				  &ofw_eth_addr, sizeof ofw_eth_addr) >= 0) {
			return(0);
		}
		if ((phandle = OF_child(qhandle)))
			continue;
		while (qhandle) {
			if ((phandle = OF_peer(qhandle)))
				break;
			qhandle = OF_parent(qhandle);
		}
	}
	return(-1);
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
 * set system type from string
 */
void
systype(char *name)
{
	/* this table may be order specific if substrings match several
	 * computers but a longer string matches a specific
	 */
	int i;
	struct systyp {
		char *name;
		char *systypename;
		int type;
	} systypes[] = {
		{ "MOT",	"(PWRSTK) MCG powerstack family", PWRSTK },
		{ "V-I Power",	"(POWER4e) V-I ppc vme boards ",  POWER4e},
		{ "iMac",	"(APPL) Apple iMac ",  APPL},
		{ "PowerMac",	"(APPL) Apple PowerMac ",  APPL},
		{ "PowerBook",	"(APPL) Apple Powerbook ",  APPL},
		{ NULL,"",0}
	};
	for (i = 0; systypes[i].name != NULL; i++) {
		if (strncmp( name , systypes[i].name,
			strlen (systypes[i].name)) == 0)
		{
			system_type = systypes[i].type;
			printf("recognized system type of %s as %s\n",
				name, systypes[i].systypename);
			break;
		}
	}
	if (system_type == OFWMACH) {
		printf("System type %snot recognized, good luck\n",
			name);
	}
}

/*
 * one attempt at interrupt stuff..
 *
 */
#include <dev/pci/pcivar.h>

int ppc_configed_intr_cnt = 0;
struct intrhand ppc_configed_intr[MAX_PRECONF_INTR];

void *
ppc_intr_establish(void *lcv, pci_intr_handle_t ih, int type, int level,
    int (*func)(void *), void *arg, char *name)
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
			" configured: driver %s too many interrupts", name);
	}
	/* disestablish is going to be tricky to supported for these :-) */
	return (void *)ppc_configed_intr_cnt;
}

intr_establish_t *intr_establish_func = (intr_establish_t *)ppc_intr_establish;
intr_disestablish_t *intr_disestablish_func;

void
ppc_intr_setup(intr_establish_t *establish, intr_disestablish_t *disestablish)
{
	intr_establish_func = establish;
	intr_disestablish_func = disestablish;
}

/* BUS functions */
int
bus_space_map(bus_space_tag_t t, bus_addr_t bpa, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	int error;

	if  (POWERPC_BUS_TAG_BASE(t) == 0) {
		/* if bus has base of 0 fail. */
		return 1;
	}
	bpa |= POWERPC_BUS_TAG_BASE(t);
	if ((error = extent_alloc_region(devio_ex, bpa, size, EX_NOWAIT |
	    (ppc_malloc_ok ? EX_MALLOCOK : 0))))
		return error;

	if ((error  = bus_mem_add_mapping(bpa, size, cacheable, bshp))) {
		if (extent_free(devio_ex, bpa, size, EX_NOWAIT |
			(ppc_malloc_ok ? EX_MALLOCOK : 0)))
		{
			printf("bus_space_map: pa 0x%lx, size 0x%x\n",
				bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}
	return 0;
}
bus_addr_t
bus_space_unmap_p(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
	bus_addr_t paddr;

	pmap_extract(pmap_kernel(), bsh, &paddr);
	bus_space_unmap((t), (bsh), (size));
	return paddr ;
}
void
bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
	bus_addr_t sva;
	bus_size_t off, len;
	bus_addr_t bpa;

	/* should this verify that the proper size is freed? */
	sva = trunc_page(bsh);
	off = bsh - sva;
	len = size+off;

	if (pmap_extract(pmap_kernel(), sva, &bpa) == TRUE) {
		if (extent_free(devio_ex, bpa | (bsh & PAGE_MASK), size, EX_NOWAIT |
			(ppc_malloc_ok ? EX_MALLOCOK : 0)))
		{
			printf("bus_space_map: pa 0x%lx, size 0x%x\n",
				bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}
	/* do not free memory which was stolen from the vm system */
	if (ppc_malloc_ok &&
	    ((sva >= VM_MIN_KERNEL_ADDRESS) && (sva < VM_MAX_KERNEL_ADDRESS)))
		uvm_km_free(phys_map, sva, len);
	else {
		pmap_remove(vm_map_pmap(phys_map), sva, sva+len);
		pmap_update(pmap_kernel());
	}
}

vm_offset_t ppc_kvm_stolen = VM_KERN_ADDRESS_SIZE;

int
bus_mem_add_mapping(bus_addr_t bpa, bus_size_t size, int cacheable,
    bus_space_handle_t *bshp)
{
	bus_addr_t vaddr;
	bus_addr_t spa, epa;
	bus_size_t off;
	int len;

	spa = trunc_page(bpa);
	epa = bpa + size;
	off = bpa - spa;
	len = size+off;

#if 0
	if (epa <= spa) {
		panic("bus_mem_add_mapping: overflow");
	}
#endif
	if (ppc_malloc_ok == 0) {
		bus_size_t alloc_size;

		/* need to steal vm space before kernel vm is initialized */
		alloc_size = round_page(len);

		vaddr = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
		ppc_kvm_stolen += alloc_size;
		if (ppc_kvm_stolen > PPC_SEGMENT_LENGTH) {
			panic("ppc_kvm_stolen, out of space");
		}
	} else {
		vaddr = uvm_km_kmemalloc(phys_map, NULL, len,
		    UVM_KMF_NOWAIT|UVM_KMF_VALLOC);
		if (vaddr == 0)
			panic("bus_mem_add_mapping: kvm alloc of 0x%x failed",
			    len);
	}
	*bshp = vaddr + off;
#ifdef DEBUG_BUS_MEM_ADD_MAPPING
	printf("mapping %x size %x to %x vbase %x\n",
		bpa, size, *bshp, spa);
#endif
	for (; len > 0; len -= PAGE_SIZE) {
		pmap_kenter_cache(vaddr, spa,
			VM_PROT_READ | VM_PROT_WRITE,
			cacheable ? PMAP_CACHE_WT : PMAP_CACHE_CI);
		spa += PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}
	return 0;
}

int
bus_space_alloc(bus_space_tag_t tag, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int cacheable,
    bus_addr_t *addrp, bus_space_handle_t *handlep)
{

	panic("bus_space_alloc: unimplemented");
}

void
bus_space_free(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{

	panic("bus_space_free: unimplemented");
}

void *
mapiodev(paddr_t pa, psize_t len)
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
			panic("ppc_kvm_stolen, out of space");
		}
	} else {
		va = uvm_km_kmemalloc(phys_map, NULL, size,
		    UVM_KMF_NOWAIT|UVM_KMF_VALLOC);
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
unmapiodev(void *kva, psize_t p_size)
{
	vaddr_t vaddr;
	int size;

	size = p_size;

	vaddr = trunc_page((vaddr_t)kva);

	uvm_km_free_wakeup(phys_map, vaddr, size);

	for (; size > 0; size -= PAGE_SIZE) {
#if 0
		pmap_remove(vm_map_pmap(phys_map), vaddr, vaddr+PAGE_SIZE-1);
#else
		pmap_remove(pmap_kernel(), vaddr,  vaddr+PAGE_SIZE-1);
#endif
		vaddr += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}



/*
 * probably should be ppc_space_copy
 */

#define _CONCAT(A,B) A ## B
#define __C(A,B)	_CONCAT(A,B)

#define BUS_SPACE_COPY_N(BYTES,TYPE)					\
void									\
__C(bus_space_copy_,BYTES)(void *v, bus_space_handle_t h1,		\
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2,		\
    bus_size_t c)							\
{									\
	TYPE *src, *dst;						\
	int i;								\
									\
	src = (TYPE *) (h1+o1);						\
	dst = (TYPE *) (h2+o2);						\
									\
	if (h1 == h2 && o2 > o1)					\
		for (i = c-1; i >= 0; i--)				\
			dst[i] = src[i];				\
	else								\
		for (i = 0; i < c; i++)					\
			dst[i] = src[i];				\
}
BUS_SPACE_COPY_N(1,u_int8_t)
BUS_SPACE_COPY_N(2,u_int16_t)
BUS_SPACE_COPY_N(4,u_int32_t)

void
bus_space_set_region_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int8_t val, bus_size_t c)
{
	u_int8_t *dst;
	int i;

	dst = (u_int8_t *) (h+o);

	for (i = 0; i < c; i++)
		dst[i] = val;
}

void
bus_space_set_region_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int16_t val, bus_size_t c)
{
	u_int16_t *dst;
	int i;

	dst = (u_int16_t *) (h+o);
	val = swap16(val);

	for (i = 0; i < c; i++)
		dst[i] = val;
}
void
bus_space_set_region_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int32_t val, bus_size_t c)
{
	u_int32_t *dst;
	int i;

	dst = (u_int32_t *) (h+o);
	val = swap32(val);

	for (i = 0; i < c; i++)
		dst[i] = val;
}

#define BUS_SPACE_READ_RAW_MULTI_N(BYTES,SHIFT,TYPE)			\
void									\
__C(bus_space_read_raw_multi_,BYTES)(bus_space_tag_t bst,		\
    bus_space_handle_t h, bus_addr_t o, u_int8_t *dst, bus_size_t size)	\
{									\
	TYPE *src;							\
	TYPE *rdst = (TYPE *)dst;					\
	int i;								\
	int count = size >> SHIFT;					\
									\
	src = (TYPE *)(h+o);						\
	for (i = 0; i < count; i++) {					\
		rdst[i] = *src;						\
		__asm__("eieio");					\
	}								\
}
BUS_SPACE_READ_RAW_MULTI_N(2,1,u_int16_t)
BUS_SPACE_READ_RAW_MULTI_N(4,2,u_int32_t)

#define BUS_SPACE_WRITE_RAW_MULTI_N(BYTES,SHIFT,TYPE)			\
void									\
__C(bus_space_write_raw_multi_,BYTES)( bus_space_tag_t bst,		\
    bus_space_handle_t h, bus_addr_t o, const u_int8_t *src,		\
    bus_size_t size)							\
{									\
	int i;								\
	TYPE *dst;							\
	TYPE *rsrc = (TYPE *)src;					\
	int count = size >> SHIFT;					\
									\
	dst = (TYPE *)(h+o);						\
	for (i = 0; i < count; i++) {					\
		*dst = rsrc[i];						\
		__asm__("eieio");					\
	}								\
}

BUS_SPACE_WRITE_RAW_MULTI_N(2,1,u_int16_t)
BUS_SPACE_WRITE_RAW_MULTI_N(4,2,u_int32_t)

int
bus_space_subregion(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

int
ppc_open_pci_bridge()
{
	char *
	pci_bridges[] = {
		"/pci",
		NULL
	};
	int handle;
	int i;

	for (i = 0; pci_bridges[i] != NULL; i++) {
		handle = OF_open(pci_bridges[i]);
		if ( handle != -1) {
			return handle;
		}
	}
	return 0;
}
void
ppc_close_pci_bridge(int handle)
{
	OF_close(handle);
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
