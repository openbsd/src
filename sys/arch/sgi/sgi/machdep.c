/*	$OpenBSD: machdep.c,v 1.92 2009/12/07 18:51:26 miod Exp $ */

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

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/autoconf.h>
#include <machine/memconf.h>
#include <machine/regnum.h>
#ifdef TGT_ORIGIN
#include <machine/mnode.h>
#endif

#include <mips64/rm7000.h>

#include <dev/cons.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>
#include <machine/bus.h>

extern char kernel_text[];
extern bus_addr_t comconsaddr;

#ifdef DEBUG
void dump_tlb(void);
#endif

/* The following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* Machine "architecture" */
char	cpu_model[30];

/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef	BUFCACHEPERCENT
#define	BUFCACHEPERCENT	5	/* Can be changed in config. */
#endif
#ifndef	BUFPAGES
#define BUFPAGES 0		/* Can be changed in config. */
#endif

int	bufpages = BUFPAGES;
int	bufcachepercent = BUFCACHEPERCENT;

vm_map_t exec_map;
vm_map_t phys_map;

caddr_t	msgbufbase;
vaddr_t	uncached_base;

int	physmem;		/* Max supported memory, changes to actual. */
int	rsvdmem;		/* Reserved memory not usable. */
int	ncpu = 1;		/* At least one CPU in the system. */
struct	user *proc0paddr;
int	console_ok;		/* Set when console initialized. */
int	kbd_reset;
int16_t	masternasid;

int32_t *environment;
struct sys_rec sys_config;

/* Pointers to the start and end of the symbol table. */
caddr_t	ssym;
caddr_t	esym;
caddr_t	ekern;

struct phys_mem_desc mem_layout[MAXMEMSEGS];

caddr_t	mips_init(int, void *, caddr_t);
void	initcpu(void);
void	dumpsys(void);
void	dumpconf(void);

static void dobootopts(int, void *);

void	arcbios_halt(int);
void	build_trampoline(vaddr_t, vaddr_t);

void	(*md_halt)(int) = arcbios_halt;

/*
 * Do all the stuff that locore normally does before calling main().
 * Reset mapping and set up mapping to hardware and init "wired" reg.
 */

caddr_t
mips_init(int argc, void *argv, caddr_t boot_esym)
{
	char *cp;
	int i;
	u_int cputype;
	vaddr_t tlb_handler, xtlb_handler;
	extern char start[], edata[], end[];
	extern char exception[], e_exception[];
	extern char *hw_vendor;
	extern void tlb_miss;
	extern void tlb_miss_err_r5k;
	extern void xtlb_miss;
	extern void xtlb_miss_err_r5k;

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
	 * Determine system type and set up configuration record data.
	 */
	hw_vendor = "SGI";
	switch (sys_config.system_type) {
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
		bios_printf("Kernel doesn't support this system type!\n");
		bios_printf("Halting system.\n");
		Bios_Halt();
		while(1);
	}

	/*
	 * Read and store console type.
	 */
	cp = Bios_GetEnvironmentVariable("ConsoleOut");
	if (cp != NULL && *cp != '\0')
		strlcpy(bios_console, cp, sizeof bios_console);

	/* Disable serial console if ARCS is telling us to use video. */
	if (strncmp(bios_console, "video", 5) == 0)
		comconsaddr = 0;

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
	 * Read platform-specific environment variables.
	 */
	switch (sys_config.system_type) {
#ifdef TGT_O2
	case SGI_O2:
		/* Get Ethernet address from ARCBIOS. */
		cp = Bios_GetEnvironmentVariable("eaddr");
		if (cp != NULL && strlen(cp) > 0)
			strlcpy(bios_enaddr, cp, sizeof bios_enaddr);
		break;
#endif
	default:
		break;
	}

	/*
	 * Set pagesize to enable use of page macros and functions.
	 * Commit available memory to UVM system.
	 */
	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();

	for (i = 0; i < MAXMEMSEGS && mem_layout[i].mem_last_page != 0; i++) {
		uint64_t fp, lp;
		uint64_t firstkernpage, lastkernpage;
		unsigned int freelist;
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
		freelist = mem_layout[i].mem_freelist;

		/* Account for kernel and kernel symbol table. */
		if (fp >= firstkernpage && lp < lastkernpage)
			continue;	/* In kernel. */

		if (lp < firstkernpage || fp > lastkernpage) {
			uvm_page_physload(fp, lp, fp, lp, freelist);
			continue;	/* Outside kernel. */
		}

		if (fp >= firstkernpage)
			fp = lastkernpage;
		else if (lp < lastkernpage)
			lp = firstkernpage;
		else { /* Need to split! */
			uint64_t xp = firstkernpage;
			uvm_page_physload(fp, xp, fp, xp, freelist);
			fp = lastkernpage;
		}
		if (lp > fp) {
			uvm_page_physload(fp, lp, fp, lp, freelist);
		}
	}

	switch (sys_config.system_type) {
#if defined(TGT_O2) || defined(TGT_OCTANE)
	case SGI_O2:
	case SGI_OCTANE:
		sys_config.cpu[0].type = (cp0_get_prid() >> 8) & 0xff;
		sys_config.cpu[0].vers_maj = (cp0_get_prid() >> 4) & 0x0f;
		sys_config.cpu[0].vers_min = cp0_get_prid() & 0x0f;
		sys_config.cpu[0].fptype = (cp1_get_prid() >> 8) & 0xff;
		sys_config.cpu[0].fpvers_maj = (cp1_get_prid() >> 4) & 0x0f;
		sys_config.cpu[0].fpvers_min = cp1_get_prid() & 0x0f;

		/*
		 * Configure TLB.
		 */
		switch(sys_config.cpu[0].type) {
		case MIPS_RM7000:
			/*
			 * Rev A (version >= 2) CPU's have 64 TLB entries.
			 *
			 * However, the last 16 are only enabled if one
			 * particular configuration bit (mode bit #24)
			 * is set on cpu reset, so check whether the
			 * extra TLB are really usable.
			 *
			 * If they are disabled, they are nevertheless
			 * writable, but random TLB insert operations
			 * will never use any of them. This can be
			 * checked by inserting dummy entries and check
			 * if any of the last 16 entries have been used.
			 *
			 * Of course, due to the way the random replacement
			 * works (hashing various parts of the TLB data,
			 * such as address bits and ASID), not all the
			 * available TLB will be used; we simply check
			 * the highest valid TLB entry we can find and
			 * see if it is in the upper 16 entries or not.
			 */
			sys_config.cpu[0].tlbsize = 48;
			if (sys_config.cpu[0].vers_maj >= 2) {
				struct tlb_entry te;
				int e, lastvalid;

				tlb_set_wired(0);
				tlb_flush(64);
				for (e = 0; e < 64 * 8; e++)
					tlb_update(XKSSEG_BASE + ptoa(2 * e),
					    pfn_to_pad(0) | PG_ROPAGE);
				lastvalid = 0;
				for (e = 0; e < 64; e++) {
					tlb_read(e, &te);
					if ((te.tlb_lo0 & PG_V) != 0)
						lastvalid = e;
				}
				tlb_flush(64);
				if (lastvalid >= 48)
					sys_config.cpu[0].tlbsize = 64;
			}
			break;

		case MIPS_R10000:
		case MIPS_R12000:
		case MIPS_R14000:
			sys_config.cpu[0].tlbsize = 64;
			break;

		default:
			sys_config.cpu[0].tlbsize = 48;
			break;
		}
		break;
#endif
	default:
		break;
	}

	/*
	 * Configure cache.
	 */
	switch(sys_config.cpu[0].type) {
	case MIPS_R10000:
	case MIPS_R12000:
	case MIPS_R14000:
		cputype = MIPS_R10000;
		break;
	case MIPS_R5000:
	case MIPS_RM7000:
	case MIPS_RM52X0:
	case MIPS_RM9000:
		cputype = MIPS_R5000;
		break;
	default:
		/*
		 * If we can't identify the cpu type, it must be
		 * r10k-compatible on Octane and Origin families, and
		 * it is likely to be r5k-compatible on O2.
		 */
		switch (sys_config.system_type) {
		case SGI_O2:
			cputype = MIPS_R5000;
			break;
		default:
		case SGI_OCTANE:
		case SGI_IP27:
		case SGI_IP35:
			cputype = MIPS_R10000;
			break;
		}
		break;
	}
	switch (cputype) {
	case MIPS_R10000:
		Mips10k_ConfigCache();
		sys_config._SyncCache = Mips10k_SyncCache;
		sys_config._InvalidateICache = Mips10k_InvalidateICache;
		sys_config._SyncDCachePage = Mips10k_SyncDCachePage;
		sys_config._HitSyncDCache = Mips10k_HitSyncDCache;
		sys_config._IOSyncDCache = Mips10k_IOSyncDCache;
		sys_config._HitInvalidateDCache = Mips10k_HitInvalidateDCache;
		break;
	default:
	case MIPS_R5000:
		Mips5k_ConfigCache();
		sys_config._SyncCache = Mips5k_SyncCache;
		sys_config._InvalidateICache = Mips5k_InvalidateICache;
		sys_config._SyncDCachePage = Mips5k_SyncDCachePage;
		sys_config._HitSyncDCache = Mips5k_HitSyncDCache;
		sys_config._IOSyncDCache = Mips5k_IOSyncDCache;
		sys_config._HitInvalidateDCache = Mips5k_HitInvalidateDCache;
		break;
	}

	/*
	 * Last chance to call the BIOS. Wiping the TLB means the BIOS' data
	 * areas are demapped on most systems.
	 */
	delay(20*1000);		/* Let any UART FIFO drain... */

	sys_config.cpu[0].tlbwired = UPAGES / 2;
	tlb_set_wired(0);
	tlb_flush(sys_config.cpu[0].tlbsize);
	tlb_set_wired(sys_config.cpu[0].tlbwired);

	/*
	 * Get a console, very early but after initial mapping setup.
	 */
	consinit();
	printf("Initial setup done, switching console.\n");

	/*
	 * Init message buffer.
	 */
	msgbufbase = (caddr_t)pmap_steal_memory(MSGBUFSIZE, NULL,NULL);
	initmsgbuf(msgbufbase, MSGBUFSIZE);

	/*
	 * Allocate U page(s) for proc[0], pm_tlbpid 1.
	 */
	proc0.p_addr = proc0paddr = curcpu()->ci_curprocpaddr =
	    (struct user *)pmap_steal_memory(USPACE, NULL, NULL);
	proc0.p_md.md_regs = (struct trap_frame *)&proc0paddr->u_pcb.pcb_regs;
	tlb_set_pid(1);

	/*
	 * Bootstrap VM system.
	 */
	pmap_bootstrap();

	/*
	 * Copy down exception vector code.
	 */
	bcopy(exception, (char *)CACHE_ERR_EXC_VEC, e_exception - exception);
	bcopy(exception, (char *)GEN_EXC_VEC, e_exception - exception);

	/*
	 * Build proper TLB refill handler trampolines.
	 */
	switch (cputype) {
	case MIPS_R5000:
		/*
		 * R5000 processors need a specific chip bug workaround
		 * in their tlb handlers.  Theoretically only revision 1
		 * of the processor need it, but there is evidence
		 * later versions also need it.
		 *
		 * This is also necessary on RM52x0; we test on the `rounded'
		 * cputype value instead of sys_config.cpu[0].type; this
		 * causes RM7k and RM9k to be included, just to be on the
		 * safe side.
		 */
		tlb_handler = (vaddr_t)&tlb_miss_err_r5k;
		xtlb_handler = (vaddr_t)&xtlb_miss_err_r5k;
		break;
	default:
		tlb_handler = (vaddr_t)&tlb_miss;
		xtlb_handler = (vaddr_t)&xtlb_miss;
		break;
	}

	build_trampoline(TLB_MISS_EXC_VEC, tlb_handler);
	build_trampoline(XTLB_MISS_EXC_VEC, xtlb_handler);

	/*
	 * Turn off bootstrap exception vectors.
	 */
	setsr(getsr() & ~SR_BOOT_EXC_VEC);
	proc0.p_md.md_regs->sr = getsr();

	/*
	 * Clear out the I and D caches.
	 */
	Mips_SyncCache();

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
 * Build a tlb trampoline
 */
void
build_trampoline(vaddr_t addr, vaddr_t dest)
{
	const uint32_t insns[] = {
		0x3c1a0000,	/* lui k0, imm16 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x001ad438,	/* dsll k0, k0, 0x10 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x001ad438,	/* dsll k0, k0, 0x10 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x03400008,	/* jr k0 */
		0x00000000	/* nop */
	};
	uint32_t *dst = (uint32_t *)addr;
	const uint32_t *src = insns;
	uint32_t a, b, c, d;

	/*
	 * Decompose the handler address in the four components which,
	 * added with sign extension, will produce the correct address.
	 */
	d = dest & 0xffff;
	dest >>= 16;
	if (d & 0x8000)
		dest++;
	c = dest & 0xffff;
	dest >>= 16;
	if (c & 0x8000)
		dest++;
	b = dest & 0xffff;
	dest >>= 16;
	if (b & 0x8000)
		dest++;
	a = dest & 0xffff;

	/*
	 * Build the trampoline, skipping noop computations.
	 */
	*dst++ = *src++ | a;
	if (b != 0)
		*dst++ = *src++ | b;
	else
		src++;
	*dst++ = *src++;
	if (c != 0)
		*dst++ = *src++ | c;
	else
		src++;
	*dst++ = *src++;
	if (d != 0)
		*dst++ = *src++ | d;
	else
		src++;
	*dst++ = *src++;
	*dst++ = *src++;

	/*
	 * Note that we keep the delay slot instruction a nop, instead
	 * of branching to the second instruction of the handler and
	 * having its first instruction in the delay slot, so that the
	 * tlb handler is free to use k0 immediately.
	 */
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
 * Console initialization: called early on from main, before vm init or startup.
 * Do enough configuration to choose and initialize a console.
 */
void
consinit()
{
	if (console_ok) {
		return;
	}
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
	printf("real mem = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem)/1024/1024);
	printf("rsvd mem = %u (%uMB)\n", ptoa(rsvdmem),
	    ptoa(rsvdmem)/1024/1024);

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
	printf("avail mem = %u (%uMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024/1024);

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

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
	case CPU_KBDRESET:
		if (securelevel > 0)
			return (sysctl_rdint(oldp, oldlenp, newp, kbd_reset));
		return (sysctl_int(oldp, oldlenp, newp, newlen, &kbd_reset));
	default:
		return EOPNOTSUPP;
	}
}

/*
 * Set registers on exec for native exec format. For o64/64.
 */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	extern struct proc *machFPCurProcPtr;

	bzero((caddr_t)p->p_md.md_regs, sizeof(struct trap_frame));
	p->p_md.md_regs->sp = stack;
	p->p_md.md_regs->pc = pack->ep_entry & ~3;
	p->p_md.md_regs->t9 = pack->ep_entry & ~3; /* abicall req */
	p->p_md.md_regs->sr = SR_FR_32 | SR_XX | SR_KSU_USER | SR_KX | SR_UX |
	    SR_EXL | SR_INT_ENAB;
#if !defined(TGT_COHERENT)
	if (sys_config.cpu[0].type == MIPS_R12000)
		p->p_md.md_regs->sr |= SR_DSD;
#endif
	p->p_md.md_regs->sr |= idle_mask & SR_INT_MASK;
	p->p_md.md_regs->ic = (idle_mask << 8) & IC_INT_MASK;
	p->p_md.md_flags &= ~MDP_FPUSED;
	if (machFPCurProcPtr == p)
		machFPCurProcPtr = (struct proc *)0;
	p->p_md.md_ss_addr = 0;
	p->p_md.md_pc_ctrl = 0;
	p->p_md.md_watch_1 = 0;
	p->p_md.md_watch_2 = 0;

	retval[1] = 0;
}


int	waittime = -1;

void
boot(int howto)
{

	/* Take a snapshot before clobbering any registers. */
	if (curproc)
		savectx(curproc->p_addr, 0);

	if (cold) {
		/*
		 * If the system is cold, just halt, unless the user
		 * explicitly asked for reboot.
		 */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* fill curproc with live object */
		if (curproc == NULL)
			curproc = &proc0;
		/*
		 * Synchronize the disks...
		 */
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr will be out of
		 * sync; adjust it now.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}

	uvm_shutdown();
	(void) splhigh();		/* Extreme priority. */

	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	doshutdownhooks();

	if (howto & RB_HALT) {
		if (howto & RB_POWERDOWN)
			printf("System Power Down.\n");
		else
			printf("System Halt.\n");
	} else
		printf("System restart.\n");

	delay(1000000);
	md_halt(howto);

	printf("Failed!!! Please reset manually.\n");
	for (;;) ;
	/*NOTREACHED*/
}

void
arcbios_halt(int howto)
{
	uint32_t sr;

	sr = disableintr();

#if 0
	/* restore ARCBios page size... */
	tlb_set_page_mask(PG_SIZE_4K);
#endif

	if (howto & RB_HALT) {
		if (howto & RB_POWERDOWN)
			Bios_PowerDown();
		else
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
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);
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

void
initcpu()
{
}

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

#ifdef DEBUG
/*
 * Dump TLB contents.
 */
void
dump_tlb()
{
char *attr[] = {
	"CWTNA", "CWTA ", "UCBL ", "CWB  ", "RES  ", "RES  ", "UCNB ", "BPASS"
};

	int tlbno, last;
	struct tlb_entry tlb;

	last = 64;

	for (tlbno = 0; tlbno < last; tlbno++) {
		tlb_read(tlbno, &tlb);

		if (tlb.tlb_lo0 & PG_V || tlb.tlb_lo1 & PG_V) {
			bios_printf("%2d v=%p", tlbno, tlb.tlb_hi & 0xffffffffffffff00);
			bios_printf("/%02x ", tlb.tlb_hi & 0xff);

			if (tlb.tlb_lo0 & PG_V) {
				bios_printf("0x%09x ", pfn_to_pad(tlb.tlb_lo0));
				bios_printf("%c", tlb.tlb_lo0 & PG_M ? 'M' : ' ');
				bios_printf("%c", tlb.tlb_lo0 & PG_G ? 'G' : ' ');
				bios_printf(" %s ", attr[(tlb.tlb_lo0 >> 3) & 7]);
			} else {
				bios_printf("invalid             ");
			}

			if (tlb.tlb_lo1 & PG_V) {
				bios_printf("0x%08x ", pfn_to_pad(tlb.tlb_lo1));
				bios_printf("%c", tlb.tlb_lo1 & PG_M ? 'M' : ' ');
				bios_printf("%c", tlb.tlb_lo1 & PG_G ? 'G' : ' ');
				bios_printf(" %s ", attr[(tlb.tlb_lo1 >> 3) & 7]);
			} else {
				bios_printf("invalid             ");
			}
			bios_printf(" sz=%x", tlb.tlb_mask);
		}
		else {
			bios_printf("%2d v=invalid    ", tlbno);
		}
		bios_printf("\n");
	}
}
#endif
