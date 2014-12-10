/*	$OpenBSD: machdep.c,v 1.150 2014/12/10 15:29:53 mikeb Exp $ */

/*
 * Copyright (c) 2003-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/msgbuf.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/exec_elf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif

#include <net/if.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>

#include <mips64/cache.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/frame.h>
#include <machine/autoconf.h>
#include <machine/memconf.h>
#include <machine/regnum.h>
#ifdef TGT_ORIGIN
#include <machine/mnode.h>
#endif
#if defined(TGT_INDY) || defined(TGT_INDIGO2)
CACHE_PROTOS(ip22)
#endif
#ifdef TGT_INDIGO2
CACHE_PROTOS(tcc)
#endif

#ifdef CPU_RM7000
#include <mips64/rm7000.h>
#endif

#include <dev/cons.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>
#include <machine/bus.h>

extern char kernel_text[];
extern bus_addr_t comconsaddr;

/* The following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* Machine "architecture" */
char	cpu_model[30];

/* will be updated in ipXX_machdep.c whenever necessary */
struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = {
	&dma_constraint,
	NULL
};

vm_map_t exec_map;
vm_map_t phys_map;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

caddr_t	msgbufbase;

int	physmem;		/* Max supported memory, changes to actual. */
int	rsvdmem;		/* Reserved memory not usable. */
int	ncpu = 1;		/* At least one CPU in the system. */
struct	user *proc0paddr;
int	console_ok;		/* Set when console initialized. */
int16_t	masternasid;

int32_t *environment;
struct sys_rec sys_config;
struct cpu_hwinfo bootcpu_hwinfo;

/* Pointers to the start and end of the symbol table. */
caddr_t	ssym;
caddr_t	esym;
caddr_t	ekern;

struct phys_mem_desc mem_layout[MAXMEMSEGS];

caddr_t	mips_init(int, void *, caddr_t);
void	dumpsys(void);
void	dumpconf(void);

static void dobootopts(int, void *);

boolean_t is_memory_range(paddr_t, psize_t, psize_t);

void	(*md_halt)(int) = arcbios_halt;

int sgi_cpuspeed(int *);

/*
 * Do all the stuff that locore normally does before calling main().
 * Reset mapping and set up mapping to hardware and init "wired" reg.
 */

caddr_t
mips_init(int argc, void *argv, caddr_t boot_esym)
{
	char *cp;
	int i, guessed;
	u_int cpufamily;
	struct cpu_info *ci;
	extern char start[], edata[], end[];
	extern char *hw_vendor;
#ifndef CPU_R8000
	extern char cache_err[], exception[], e_exception[];
	vaddr_t xtlb_handler;
#endif

#ifdef MULTIPROCESSOR
	/*
	 * Set curcpu address on primary processor.
	 */
	setcurcpu(&cpu_info_primary);
#endif

	ci = curcpu();

	/*
	 * Make sure we can access the extended address space.
	 * Note that r10k and later do not allow XUSEG accesses
	 * from kernel mode unless SR_UX is set.
	 */
	setsr(getsr() | SR_KX | SR_UX);

	/*
	 * Clear the compiled BSS segment in OpenBSD code.
	 */
	bzero(edata, end - edata);

	/*
	 * Reserve space for the symbol table, if it exists.
	 */
	ssym = (char *)*(u_int64_t *)end;

	/* Attempt to locate ELF header and symbol table after kernel. */
	if (end[0] == ELFMAG0 && end[1] == ELFMAG1 &&
	    end[2] == ELFMAG2 && end[3] == ELFMAG3 ) {

		/* ELF header exists directly after kernel. */
		ssym = end;
		esym = boot_esym;
		ekern = esym;

	} else if (((long)ssym - (long)end) >= 0 &&
	    ((long)ssym - (long)end) <= 0x1000 &&
	    ssym[0] == ELFMAG0 && ssym[1] == ELFMAG1 &&
	    ssym[2] == ELFMAG2 && ssym[3] == ELFMAG3 ) {

		/* Pointers exist directly after kernel. */
		esym = (char *)*((u_int64_t *)end + 1);
		ekern = esym;

	} else {

		/* Pointers aren't setup either... */
		ssym = NULL;
		esym = NULL;
		ekern = end;
	}

	/*
	 * Initialize the system type and set up memory layout.
	 * Note that some systems have a more complex memory setup.
	 */
	bios_ident();

	/*
	 * Read and store ARCBios variables for future reference.
	 */
	cp = Bios_GetEnvironmentVariable("ConsoleOut");
	if (cp != NULL && *cp != '\0')
		strlcpy(bios_console, cp, sizeof(bios_console));
	cp = Bios_GetEnvironmentVariable("gfx");
	if (cp != NULL && *cp != '\0')
		strlcpy(bios_graphics, cp, sizeof(bios_graphics));
	cp = Bios_GetEnvironmentVariable("keybd");
	if (cp != NULL && *cp != '\0')
		strlcpy(bios_keyboard, cp, sizeof(bios_keyboard));
	/* the following variables may not exist on all systems */
	cp = Bios_GetEnvironmentVariable("eaddr");
	if (cp != NULL && strlen(cp) > 0)
		strlcpy(bios_enaddr, cp, sizeof bios_enaddr);
	bios_consrate = bios_getenvint("dbaud");
	if (bios_consrate < 50 || bios_consrate > 115200)
		bios_consrate = 9600;	/* sane default */
	cp = Bios_GetEnvironmentVariable("OSLoadOptions");
	if (cp != NULL && strlen(cp) > 0)
		strlcpy(osloadoptions, cp, sizeof osloadoptions);

	/*
	 * Determine system type and set up configuration record data.
	 */
	hw_vendor = "SGI";
	switch (sys_config.system_type) {
#ifdef TGT_INDIGO
	case SGI_IP20:
		bios_printf("Found SGI-IP20, setting up.\n");
		/* IP22 is intentional, we use the same kernel */
		strlcpy(cpu_model, "IP22", sizeof(cpu_model));
		ip22_setup();
		break;
#endif
#if defined(TGT_INDY) || defined(TGT_INDIGO2)
	case SGI_IP22:
		bios_printf("Found SGI-IP22, setting up.\n");
		strlcpy(cpu_model, "IP22", sizeof(cpu_model));
		ip22_setup();
		break;
#endif
#ifdef TGT_INDIGO2
	case SGI_IP26:
		bios_printf("Found SGI-IP26, setting up.\n");
		strlcpy(cpu_model, "IP26", sizeof(cpu_model));
		ip22_setup();
		break;
	case SGI_IP28:
		bios_printf("Found SGI-IP28, setting up.\n");
		strlcpy(cpu_model, "IP28", sizeof(cpu_model));
		ip22_setup();
		break;
#endif
#ifdef TGT_O2
	case SGI_O2:
		bios_printf("Found SGI-IP32, setting up.\n");
		strlcpy(cpu_model, "IP32", sizeof(cpu_model));
		ip32_setup();
		break;
#endif
#ifdef TGT_ORIGIN
	case SGI_IP27:
		bios_printf("Found SGI-IP27, setting up.\n");
		strlcpy(cpu_model, "IP27", sizeof(cpu_model));
		ip27_setup();

		break;

	case SGI_IP35:
		bios_printf("Found SGI-IP35, setting up.\n");
		/* IP27 is intentional, we use the same kernel */
		strlcpy(cpu_model, "IP27", sizeof(cpu_model));
		ip27_setup();

		break;
#endif
#ifdef TGT_OCTANE
	case SGI_OCTANE:
		bios_printf("Found SGI-IP30, setting up.\n");
		strlcpy(cpu_model, "IP30", sizeof(cpu_model));
		ip30_setup();
		break;
#endif
	default:
		bios_printf("There is no support for this system type "
		    "(%02x) in this kernel.\n"
		    "Are you sure you have booted the right kernel "
		    "for this machine?\n",
		    sys_config.system_type);
		bios_printf("Halting system.\n");
		Bios_Halt();
		for (;;) ;
	}

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_AUTOBOOT;
	dobootopts(argc, argv);

	/*
	 * Figure out where we supposedly booted from.
	 */
	cp = Bios_GetEnvironmentVariable("OSLoadPartition");
	if (cp == NULL)
		cp = "unknown";
	if (strlcpy(osloadpartition, cp, sizeof osloadpartition) >=
	    sizeof osloadpartition)
		bios_printf("Value of `OSLoadPartition' is too large.\n"
		 "The kernel might not be able to find out its root device.\n");

	/*
	 * Set pagesize to enable use of page macros and functions.
	 * Commit available memory to UVM system.
	 */
	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();

	for (i = 0; i < MAXMEMSEGS && mem_layout[i].mem_last_page != 0; i++) {
		uint64_t fp, lp;
		uint64_t firstkernpage, lastkernpage;
		paddr_t firstkernpa, lastkernpa;

		if (IS_XKPHYS((vaddr_t)start))
			firstkernpa = XKPHYS_TO_PHYS((vaddr_t)start);
		else
			firstkernpa = CKSEG0_TO_PHYS((vaddr_t)start);
		if (IS_XKPHYS((vaddr_t)ekern))
			lastkernpa = XKPHYS_TO_PHYS((vaddr_t)ekern);
		else
			lastkernpa = CKSEG0_TO_PHYS((vaddr_t)ekern);

		firstkernpage = atop(trunc_page(firstkernpa));
		lastkernpage = atop(round_page(lastkernpa));

		fp = mem_layout[i].mem_first_page;
		lp = mem_layout[i].mem_last_page;

		/* Account for kernel and kernel symbol table. */
		if (fp >= firstkernpage && lp < lastkernpage)
			continue;	/* In kernel. */

		if (lp < firstkernpage || fp > lastkernpage) {
			uvm_page_physload(fp, lp, fp, lp, 0);
			continue;	/* Outside kernel. */
		}

		if (fp >= firstkernpage)
			fp = lastkernpage;
		else if (lp < lastkernpage)
			lp = firstkernpage;
		else { /* Need to split! */
			uint64_t xp = firstkernpage;
			uvm_page_physload(fp, xp, fp, xp, 0);
			fp = lastkernpage;
		}
		if (lp > fp) {
			uvm_page_physload(fp, lp, fp, lp, 0);
		}
	}

	/*
	 * Configure cache.
	 */
	guessed = 0;
	switch (bootcpu_hwinfo.type) {
#ifdef CPU_R4000
	case MIPS_R4000:
		cpufamily = MIPS_R4000;
		break;
#endif
#ifdef CPU_R4600
	case MIPS_R4600:
		cpufamily = MIPS_R5000;
		break;
#endif
#ifdef CPU_R5000
	case MIPS_R5000:
	case MIPS_RM52X0:
		cpufamily = MIPS_R5000;
		break;
#endif
#ifdef CPU_RM7000
	case MIPS_RM7000:
	case MIPS_RM9000:
		cpufamily = MIPS_R5000;
		break;
#endif
#ifdef CPU_R8000
	case MIPS_R8000:
		cpufamily = MIPS_R8000;
		break;
#endif
#ifdef CPU_R10000
	case MIPS_R10000:
	case MIPS_R12000:
	case MIPS_R14000:
		cpufamily = MIPS_R10000;
		break;
#endif
	default:
		/*
		 * If we can't identify the cpu type, it must be
		 * r10k-compatible on Octane and Origin families, and
		 * it is likely to be r5k-compatible on O2 and
		 * r4k-compatible on Ind{igo*,y}.
		 */
		guessed = 1;
		switch (sys_config.system_type) {
		case SGI_IP20:
		case SGI_IP22:
			bios_printf("Unrecognized processor type %02x, assuming"
			    " R4000 compatible\n", bootcpu_hwinfo.type);
			cpufamily = MIPS_R4000;
			break;
		case SGI_O2:
			bios_printf("Unrecognized processor type %02x, assuming"
			    " R5000 compatible\n", bootcpu_hwinfo.type);
			cpufamily = MIPS_R5000;
			break;
		case SGI_IP26:
			bios_printf("Unrecognized processor type %02x, assuming"
			    " R8000 compatible\n", bootcpu_hwinfo.type);
			cpufamily = MIPS_R8000;
			break;
		default:
		case SGI_IP27:
		case SGI_IP28:
		case SGI_OCTANE:
		case SGI_IP35:
			bios_printf("Unrecognized processor type %02x, assuming"
			    " R10000 compatible\n", bootcpu_hwinfo.type);
			cpufamily = MIPS_R10000;
			break;
		}
		break;
	}
	switch (cpufamily) {
#ifdef CPU_R4000
	case MIPS_R4000:
		Mips4k_ConfigCache(ci);
		break;
#endif
#if defined(CPU_R4600) || defined(CPU_R5000) || defined(CPU_RM7000)
	case MIPS_R5000:
#if defined(TGT_INDY) || defined(TGT_INDIGO2)
		if (sys_config.system_type == SGI_IP22)
			ip22_ConfigCache(ci);
		else
#endif
			Mips5k_ConfigCache(ci);
		break;
#endif
#ifdef CPU_R8000
	case MIPS_R8000:
#ifdef TGT_INDIGO2
		if (sys_config.system_type == SGI_IP26)
			tcc_ConfigCache(ci);
		else
#endif
			tfp_ConfigCache(ci);
		break;
#endif
#ifdef CPU_R10000
	case MIPS_R10000:
		Mips10k_ConfigCache(ci);
		break;
#endif
	default:
		if (guessed) {
			bios_printf("There is no support for this processor "
			    "family in this kernel.\n"
			    "Are you sure you have booted the right kernel "
			    "for this machine?\n");
		} else {
			bios_printf("There is no support for this processor "
			    "family (%02x) in this kernel.\n",
			    bootcpu_hwinfo.type);
		}
		bios_printf("Halting system.\n");
		Bios_Halt();
		for (;;) ;
		break;
	}

	/*
	 * Last chance to call the BIOS. Wiping the TLB means the BIOS' data
	 * areas are demapped on most systems.
	 */
	delay(20*1000);		/* Let any UART FIFO drain... */

	tlb_init(bootcpu_hwinfo.tlbsize);

#ifdef CPU_R8000	/* { */
	/*
	 * Set up TrapBase to point to our own trap vector area.
	 */
	{
		extern char tfp_trapbase[];
		cp0_set_trapbase((vaddr_t)tfp_trapbase);
	}
#else	/* } { */
	/*
	 * Copy down exception vector code.
	 */
	bcopy(exception, (char *)CACHE_ERR_EXC_VEC, e_exception - cache_err);
	bcopy(exception, (char *)GEN_EXC_VEC, e_exception - exception);

	/*
	 * Build proper TLB refill handler trampolines.
	 */
	switch (cpufamily) {
#ifdef CPU_R4000
	case MIPS_R4000:
	    {
		extern void xtlb_miss_err_r4k;
		extern void xtlb_miss_err_r4000SC;

		if (ci->ci_l2.size == 0 ||
		    ((cp0_get_prid() >> 4) & 0x0f) >= 4) /* R4400 */
			xtlb_handler = (vaddr_t)&xtlb_miss_err_r4k;
		else {
			xtlb_handler = (vaddr_t)&xtlb_miss_err_r4000SC;
			xtlb_handler |= CKSEG1_BASE;
		}
	    }
		break;
#endif
#if defined(CPU_R5000) || defined(CPU_RM7000)
	case MIPS_R5000:
	    {
		/*
		 * R5000 processors need a specific chip bug workaround
		 * in their tlb handlers.  Theoretically only revision 1
		 * of the processor need it, but there is evidence
		 * later versions also need it.
		 *
		 * This is also necessary on RM52x0 and most RM7k/RM9k,
		 * and is a documented errata for these chips.
		 */
		extern void xtlb_miss_err_r5k;
		xtlb_handler = (vaddr_t)&xtlb_miss_err_r5k;
	    }
		break;
#endif
	default:
	    {
		extern void xtlb_miss;
		xtlb_handler = (vaddr_t)&xtlb_miss;
	    }
		break;
	}

	build_trampoline(TLB_MISS_EXC_VEC, xtlb_handler);
	build_trampoline(XTLB_MISS_EXC_VEC, xtlb_handler);
#endif	/* } */

#ifdef CPU_R4000
	/*
	 * Enable R4000 EOP errata workaround code if necessary.
	 */
	if (cpufamily == MIPS_R4000 && ((cp0_get_prid() >> 4) & 0x0f) < 3)
		r4000_errata = 1;
#endif

	/*
	 * Allocate U page(s) for proc[0], pm_tlbpid 1.
	 */
	ci->ci_curproc = &proc0;
	proc0.p_cpu = ci;
	proc0.p_addr = proc0paddr = ci->ci_curprocpaddr =
	    (struct user *)pmap_steal_memory(USPACE, NULL, NULL);
	proc0.p_md.md_regs = (struct trap_frame *)&proc0paddr->u_pcb.pcb_regs;
	tlb_set_pid(MIN_USER_ASID);

	/*
	 * Get a console, very early but after initial mapping setup
	 * and exception handler setup - console probe code might need
	 * to invoke guarded_read(), and this needs our handlers to be
	 * available.
	 */
	consinit();
	printf("Initial setup done, switching console.\n");

#ifdef DDB
	{
		/*
		 * Early initialize cpu0 so that commands such as `mach tlb'
		 * can work from ddb if ddb is entered before cpu0 attaches.
		 */
		extern struct cpu_info cpu_info_primary;
		bcopy(&bootcpu_hwinfo, &cpu_info_primary.ci_hw,
		    sizeof(struct cpu_hwinfo));
	}
#endif

	/*
	 * Init message buffer.
	 */
	msgbufbase = (caddr_t)pmap_steal_memory(MSGBUFSIZE, NULL, NULL);
	initmsgbuf(msgbufbase, MSGBUFSIZE);

	/*
	 * Bootstrap VM system.
	 */
	pmap_bootstrap();

#ifdef CPU_R8000
	/*
	 * Turn on precise FPU exceptions. This also causes the FS bit in
	 * the FPU status register to be honoured, instead of being forced
	 * to one.
	 */
	setsr(getsr() | SR_SERIALIZE_FPU);

	/*
	 * Turn on sequential memory model. This makes sure that there are
	 * no risks of hitting virtual coherency exceptions, which are not
	 * recoverable on R8000.
	 */
	cp0_set_config((cp0_get_config() & ~CFGR_ICE) | CFGR_SMM);
#else
	/*
	 * Turn off bootstrap exception vectors.
	 */
	setsr(getsr() & ~SR_BOOT_EXC_VEC);
#endif

	proc0.p_md.md_regs->sr = getsr();

	/*
	 * Clear out the I and D caches.
	 */
	Mips_SyncCache(ci);

#ifdef DDB
	db_machine_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif

	/*
	 * Return new stack pointer.
	 */
	return ((caddr_t)proc0paddr + USPACE - 64);
}

/*
 * Decode boot options.
 */
static void
dobootopts(int argc, void *argv)
{
	char *cp;
	int i;

	/* XXX Should this be done differently, eg env vs. args? */
	for (i = 1; i < argc; i++) {
		if (bios_is_32bit)
			cp = (char *)(long)((int32_t *)argv)[i];
		else
			cp = ((char **)argv)[i];
		if (cp == NULL)
			continue;

		/*
		 * Parse PROM options.
		 */
		if (strncmp(cp, "OSLoadOptions=", 14) == 0) {
			if (strcmp(&cp[14], "auto") == 0)
				boothowto &= ~(RB_SINGLE|RB_ASKNAME);
			else if (strcmp(&cp[14], "single") == 0)
				boothowto |= RB_SINGLE;
			else if (strcmp(&cp[14], "debug") == 0)
				boothowto |= RB_KDB;
			continue;
		}

		/*
		 * Parse kernel options.
		 */
		if (*cp == '-') {
			while (*++cp != '\0')
				switch (*cp) {
				case 'a':
					boothowto |= RB_ASKNAME;
					break;
				case 'c':
					boothowto |= RB_CONFIG;
					break;
				case 'd':
					boothowto |= RB_KDB;
					break;
				case 's':
					boothowto |= RB_SINGLE;
					break;
				}
		}
	}
}


/*
 * Console initialization: called early on from mips_init(), before vm init
 * is completed.
 * Do enough configuration to choose and initialize a console.
 */
void
consinit()
{
	if (console_ok)
		return;
	cninit();
	console_ok = 1;
}

/*
 * cpu_startup: allocate memory for variable-sized tables, initialize CPU, and 
 * do auto-configuration.
 */
void
cpu_startup()
{
	vaddr_t minaddr, maxaddr;
#ifdef PMAPDEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;	/* Shut up pmap debug during bootstrap. */
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %lu (%luMB)\n", ptoa((psize_t)physmem),
	    ptoa((psize_t)physmem)/1024/1024);
	printf("rsvd mem = %lu (%luMB)\n", ptoa((psize_t)rsvdmem),
	    (ptoa((psize_t)rsvdmem) + 1023 * 1024)/1024/1024);

	/*
	 * Allocate a submap for exec arguments. This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);
	/* Allocate a submap for physio. */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

#ifdef PMAPDEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %lu (%luMB)\n", ptoa((psize_t)uvmexp.free),
	    ptoa((psize_t)uvmexp.free)/1024/1024);

	cpu_cpuspeed = sgi_cpuspeed;

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
}

/*
 * Machine dependent system variables.
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
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;		/* Overloaded */

	switch (name[0]) {
	default:
		return EOPNOTSUPP;
	}
}

int
sgi_cpuspeed(int *freq)
{
	/*
	 * XXX assumes all CPU have the same frequency
	 */
	*freq = curcpu()->ci_hw.clock / 1000000;
	return (0);
}

int	waittime = -1;

__dead void
boot(int howto)
{
	if (curproc)
		savectx(curproc->p_addr, 0);

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

	uvm_shutdown();
	splhigh();
	cold = 1;

	if ((howto & RB_DUMP) != 0)
		dumpsys();

haltsys:
	config_suspend_all(DVACT_POWERDOWN);

	if ((howto & RB_HALT) != 0) {
		if ((howto & RB_POWERDOWN) != 0)
			printf("System Power Down.\n");
		else
			printf("System Halt.\n");
	} else
		printf("System restart.\n");

	delay(1000000);
	md_halt(howto);

	printf("Failed!!! Please reset manually.\n");
	for (;;) ;
	/* NOTREACHED */
}

void
arcbios_halt(int howto)
{
	register_t sr;

	sr = disableintr();

#if 0
	/* restore ARCBios page size... */
	tlb_set_page_mask(PG_SIZE_4K);
#endif

	if (howto & RB_HALT) {
#ifdef TGT_INDIGO
		/* Indigo does not support powerdown */
		if (sys_config.system_type == SGI_IP20)
			howto &= ~RB_POWERDOWN;
#endif
		if (howto & RB_POWERDOWN) {
#ifdef TGT_INDY
			/*
			 * ARCBios needs to use the FPU on Indy during
			 * shutdown.
			 */
			if (sys_config.system_type == SGI_IP22)
				setsr(getsr() | SR_COP_1_BIT);
#endif
			Bios_PowerDown();
		} else
			Bios_EnterInteractiveMode();
	} else
		Bios_Reboot();

	setsr(sr);
}

u_long	dumpmag = 0x8fca0101;	/* Magic number for savecore. */
int	dumpsize = 0;			/* Also for savecore. */
long	dumplo = 0;

void
dumpconf(void)
{
	int nblks;

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	dumpsize = ptoa(physmem);
	if (dumpsize > atop(round_page(dbtob(nblks - dumplo))))
		dumpsize = atop(round_page(dbtob(nblks - dumplo)));
	else if (dumplo == 0)
		dumplo = nblks - btodb(ptoa(physmem));

	/*
	 * Don't dump on the first page in case the dump device includes a 
	 * disk label.
	 */
	if (dumplo < btodb(PAGE_SIZE))
		dumplo = btodb(PAGE_SIZE);
}

/*
 * Doadump comes here after turning off memory management and getting on the
 * dump stack, either when called above, or by the auto-restart code.
 */
void
dumpsys()
{
	extern int msgbufmapped;

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	/*
	 * For dumps during auto-configuration, if dump device has already
	 * configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);
	printf("dump not yet implemented\n");
#if 0 /* XXX HAVE TO FIX XXX */
	switch (error = (*bdevsw[major(dumpdev)].d_dump)(dumpdev, dumplo,)) {

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
		printf("error %d\n", error);
		break;

	case 0:
		printf("succeeded\n");
	}
#endif
}

boolean_t
is_memory_range(paddr_t pa, psize_t len, psize_t limit)
{
	struct phys_mem_desc *seg;
	uint64_t fp, lp;
	int i;

	fp = atop(pa);
	lp = atop(round_page(pa + len));

	if (limit != 0 && lp > atop(limit))
		return FALSE;

	for (i = 0, seg = mem_layout; i < MAXMEMSEGS; i++, seg++)
		if (fp >= seg->mem_first_page && lp <= seg->mem_last_page)
			return TRUE;

	return FALSE;
}

#ifdef CPU_RM7000
#ifdef	RM7K_PERFCNTR
/*
 * RM7000 Performance counter support.
 */
int
rm7k_perfcntr(cmd, arg1, arg2, arg3)
	int cmd;
	long  arg1, arg2, arg3;
{
	int result;
	quad_t cntval;
	struct proc *p = curproc;


	switch(cmd) {
	case PCNT_FNC_SELECT:
		if ((arg1 & 0xff) > PCNT_SRC_MAX ||
		   (arg1 & ~(PCNT_CE|PCNT_UM|PCNT_KM|0xff)) != 0) {
			result = EINVAL;
			break;
		}
#ifdef DEBUG
printf("perfcnt select %x, proc %p\n", arg1, p);
#endif
		p->p_md.md_pc_count = 0;
		p->p_md.md_pc_spill = 0;
		p->p_md.md_pc_ctrl = arg1;
		result = 0;
		break;

	case PCNT_FNC_READ:
		cntval = p->p_md.md_pc_count;
		cntval += (quad_t)p->p_md.md_pc_spill << 31;
		result = copyout(&cntval, (void *)arg1, sizeof(cntval));
		break;

	default:
#ifdef DEBUG
printf("perfcnt error %d\n", cmd);
#endif
		result = -1;
		break;
	}
	return(result);
}

/*
 * Called when the performance counter d31 gets set.
 * Increase spill value and reset d31.
 */
void
rm7k_perfintr(trapframe)
	struct trap_frame *trapframe;
{
	struct proc *p = curproc;

#ifdef DEBUG
	printf("perfintr proc %p!\n", p);
#endif
	cp0_setperfcount(cp0_getperfcount() & 0x7fffffff);
	if (p != NULL) {
		p->p_md.md_pc_spill++;
	}
}

int
rm7k_watchintr(trapframe)
	struct trap_frame *trapframe;
{
	return(0);
}

#endif	/* RM7K_PERFCNTR */
#endif	/* CPU_RM7000 */
