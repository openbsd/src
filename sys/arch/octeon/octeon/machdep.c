/*	$OpenBSD: machdep.c,v 1.64 2015/06/25 10:56:00 jmatthew Exp $ */

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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
#include <sys/tty.h>
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

#include <machine/autoconf.h>
#include <mips64/cache.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/memconf.h>

#include <dev/cons.h>

#include <octeon/dev/iobusvar.h>
#include <machine/octeonreg.h>
#include <machine/octeonvar.h>
#include <machine/octeon_model.h>

/* The following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* Machine "architecture" */
char	cpu_model[64];

struct uvm_constraint_range  dma_constraint = { 0x0, 0xffffffffUL };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

vm_map_t exec_map;
vm_map_t phys_map;

struct boot_desc *octeon_boot_desc;
struct boot_info *octeon_boot_info;

char uboot_rootdev[OCTEON_ARGV_MAX];

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

caddr_t	msgbufbase;

int	physmem;		/* Max supported memory, changes to actual. */
int	ncpu = 1;		/* At least one CPU in the system. */
struct	user *proc0paddr;

struct cpu_hwinfo bootcpu_hwinfo;

/* Pointers to the start and end of the symbol table. */
caddr_t	ssym;
caddr_t	esym;
caddr_t	ekern;

struct phys_mem_desc mem_layout[MAXMEMSEGS];

void		dumpsys(void);
void		dumpconf(void);
vaddr_t		mips_init(__register_t, __register_t, __register_t, __register_t);
boolean_t 	is_memory_range(paddr_t, psize_t, psize_t);
void		octeon_memory_init(struct boot_info *);
int		octeon_cpuspeed(int *);
static void	process_bootargs(void);
static uint64_t	get_ncpusfound(void);

extern void 	parse_uboot_root(void);

cons_decl(cn30xxuart);
struct consdev uartcons = cons_init(cn30xxuart);

#define btoc(x) (((x)+PAGE_MASK)>>PAGE_SHIFT)

#define OCTEON_DRAM_FIRST_256_END	0xfffffffull

void
octeon_memory_init(struct boot_info *boot_info)
{
	uint64_t phys_avail[10 + 2] = {0,};
	uint64_t startpfn, endpfn;
	uint32_t realmem;
	extern char end[];
	int i;
	uint32_t realmem_bytes;

	startpfn = atop(CKSEG0_TO_PHYS((vaddr_t)&end) + PAGE_SIZE);
	endpfn = atop(96 << 20);
	mem_layout[0].mem_first_page = startpfn;
	mem_layout[0].mem_last_page = endpfn;

	physmem = endpfn - startpfn;

	/* Simulator we limit to 96 meg */
	if (boot_info->board_type == BOARD_TYPE_SIM) {
		realmem_bytes = (96 << 20);
	} else {
		realmem_bytes = ((boot_info->dram_size << 20) - PAGE_SIZE);
		realmem_bytes &= ~(PAGE_SIZE - 1);
	}
	/* phys_avail regions are in bytes */
	phys_avail[0] =
	    (CKSEG0_TO_PHYS((uint64_t)&end) + PAGE_SIZE) & ~(PAGE_SIZE - 1);

	/* Simulator gets 96Meg period. */
	if (boot_info->board_type == BOARD_TYPE_SIM) {
		phys_avail[1] = (96 << 20);
	} else {
		if (realmem_bytes > OCTEON_DRAM_FIRST_256_END) {
			phys_avail[1] = OCTEON_DRAM_FIRST_256_END;
			realmem_bytes -= OCTEON_DRAM_FIRST_256_END;
			realmem_bytes &= ~(PAGE_SIZE - 1);
		} else
			phys_avail[1] = realmem_bytes;

		mem_layout[0].mem_last_page = atop(phys_avail[1]);
	}

	/*-
	 * Octeon Memory looks as follows:
         *   PA
	 * First 256 MB DR0
	 * 0000 0000 0000 0000     to  0000 0000 0FFF FFFF
	 * Second 256 MB DR1 
	 * 0000 0004 1000 0000     to  0000 0004 1FFF FFFF
	 * Over 512MB Memory DR2  15.5GB
	 * 0000 0000 2000 0000     to  0000 0003 FFFF FFFF
	 */
	physmem = atop(phys_avail[1] - phys_avail[0]);

	if (boot_info->board_type != BOARD_TYPE_SIM) {
		if (realmem_bytes > OCTEON_DRAM_FIRST_256_END) {
			/* take out the upper non-cached 1/2 */
			phys_avail[2] = 0x410000000ULL;
			phys_avail[3] =
			    (0x410000000ULL + OCTEON_DRAM_FIRST_256_END);
			physmem += btoc(phys_avail[3] - phys_avail[2]);
			mem_layout[2].mem_first_page = atop(phys_avail[2]);
			mem_layout[2].mem_last_page = atop(phys_avail[3] - 1);
			realmem_bytes -= OCTEON_DRAM_FIRST_256_END;

			/* Now map the rest of the memory */
			phys_avail[4] = 0x20000000ULL;
			phys_avail[5] = (0x20000000ULL + realmem_bytes);
			physmem += btoc(phys_avail[5] - phys_avail[4]);
			mem_layout[1].mem_first_page = atop(phys_avail[4]);
			mem_layout[1].mem_last_page = atop(phys_avail[5] - 1);
			realmem_bytes = 0;
		} else {
			/* Now map the rest of the memory */
			phys_avail[2] = 0x410000000ULL;
			phys_avail[3] = (0x410000000ULL + realmem_bytes);
			physmem += btoc(phys_avail[3] - phys_avail[2]);
			mem_layout[1].mem_first_page = atop(phys_avail[2]);
			mem_layout[1].mem_last_page = atop(phys_avail[3] - 1);
			realmem_bytes = 0;
		}
 	}

 	realmem = physmem;

	printf("Total DRAM Size 0x%016X\n",
	    (uint32_t)(boot_info->dram_size << 20));

	for (i = 0; phys_avail[i]; i += 2) {
		printf("Bank %d = 0x%016lX   ->  0x%016lX\n", i >> 1,
		    (long)phys_avail[i], (long)phys_avail[i + 1]);
	}
	for (i = 0; mem_layout[i].mem_last_page; i++) {
		printf("mem_layout[%d] page 0x%016llX -> 0x%016llX\n", i,
		    mem_layout[i].mem_first_page, mem_layout[i].mem_last_page);
	}
}

/*
 * Do all the stuff that locore normally does before calling main().
 * Reset mapping and set up mapping to hardware and init "wired" reg.
 */
vaddr_t
mips_init(__register_t a0, __register_t a1, __register_t a2 __unused,
	__register_t a3)
{
	uint prid;
	vaddr_t xtlb_handler;
	int i;
	struct boot_desc *boot_desc;
	struct boot_info *boot_info;

	extern char start[], edata[], end[];
	extern char exception[], e_exception[];
	extern void xtlb_miss;

	boot_desc = (struct boot_desc *)a3;
	boot_info = (struct boot_info *)
	    PHYS_TO_XKPHYS(boot_desc->boot_info_addr, CCA_CACHED);

#ifdef MULTIPROCESSOR
	/*
	 * Set curcpu address on primary processor.
	 */
	setcurcpu(&cpu_info_primary);
#endif
	/*
	 * Clear the compiled BSS segment in OpenBSD code.
	 */

	bzero(edata, end - edata);

	/*
	 * Set up early console output.
	 */
	cn_tab = &uartcons;

	/*
	 * Reserve space for the symbol table, if it exists.
	 */
	ssym = (char *)(vaddr_t)*(int32_t *)end;
	if (((long)ssym - (long)end) >= 0 &&
	    ((long)ssym - (long)end) <= 0x1000 &&
	    ssym[0] == ELFMAG0 && ssym[1] == ELFMAG1 &&
	    ssym[2] == ELFMAG2 && ssym[3] == ELFMAG3) {
		/* Pointers exist directly after kernel. */
		esym = (char *)(vaddr_t)*((int32_t *)end + 1);
		ekern = esym;
	} else {
		/* Pointers aren't setup either... */
		ssym = NULL;
		esym = NULL;
		ekern = end;
	}

	prid = cp0_get_prid();

	bootcpu_hwinfo.clock = boot_desc->eclock;

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_AUTOBOOT;

	octeon_memory_init(boot_info);

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

		/* kernel is linked in CKSEG0 */
		firstkernpa = CKSEG0_TO_PHYS((vaddr_t)start);
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

	bootcpu_hwinfo.c0prid = prid;
	bootcpu_hwinfo.type = (prid >> 8) & 0xff;
	bootcpu_hwinfo.c1prid = 0;	/* No FPU */
	bootcpu_hwinfo.tlbsize = 1 + ((cp0_get_config_1() >> 25) & 0x3f);
	bcopy(&bootcpu_hwinfo, &curcpu()->ci_hw, sizeof(struct cpu_hwinfo));

	/*
	 * Configure cache.
	 */

	Octeon_ConfigCache(curcpu());
	Octeon_SyncCache(curcpu());

	tlb_init(bootcpu_hwinfo.tlbsize);

	/*
	 * Save the the boot information for future reference since we can't
	 * retrieve it anymore after we've fully bootstrapped the kernel.
	 */

	bcopy(&boot_info, &octeon_boot_info, sizeof(octeon_boot_info));
	bcopy(&boot_desc, &octeon_boot_desc, sizeof(octeon_boot_desc));

	snprintf(cpu_model, sizeof(cpu_model), "Cavium OCTEON (rev %d.%d) @ %d MHz",
		 (bootcpu_hwinfo.c0prid >> 4) & 0x0f,
		 bootcpu_hwinfo.c0prid & 0x0f,
		 bootcpu_hwinfo.clock / 1000000);

	cpu_cpuspeed = octeon_cpuspeed;

	ncpusfound = get_ncpusfound();

	process_bootargs();

	/*
	 * Get a console, very early but after initial mapping setup.
	 */

	consinit();
	printf("Initial setup done, switching console.\n");

#define DEBUG
#ifdef DEBUG
#define DUMP_BOOT_DESC(field, format) \
	printf("boot_desc->" #field ":" #format "\n", boot_desc->field)
#define DUMP_BOOT_INFO(field, format) \
	printf("boot_info->" #field ":" #format "\n", boot_info->field)

	DUMP_BOOT_DESC(desc_ver, %d);
	DUMP_BOOT_DESC(desc_size, %d);
	DUMP_BOOT_DESC(stack_top, %llx);
	DUMP_BOOT_DESC(heap_start, %llx);
	DUMP_BOOT_DESC(heap_end, %llx);
	DUMP_BOOT_DESC(argc, %d);
	DUMP_BOOT_DESC(flags, %#x);
	DUMP_BOOT_DESC(core_mask, %#x);
	DUMP_BOOT_DESC(dram_size, %d);
	DUMP_BOOT_DESC(phy_mem_desc_addr, %#x);
	DUMP_BOOT_DESC(debugger_flag_addr, %#x);
	DUMP_BOOT_DESC(eclock, %d);
	DUMP_BOOT_DESC(boot_info_addr, %#llx);

	DUMP_BOOT_INFO(ver_major, %d);
	DUMP_BOOT_INFO(ver_minor, %d);
	DUMP_BOOT_INFO(stack_top, %llx);
	DUMP_BOOT_INFO(heap_start, %llx);
	DUMP_BOOT_INFO(heap_end, %llx);
	DUMP_BOOT_INFO(boot_desc_addr, %#llx);
	DUMP_BOOT_INFO(exception_base_addr, %#x);
	DUMP_BOOT_INFO(stack_size, %d);
	DUMP_BOOT_INFO(flags, %#x);
	DUMP_BOOT_INFO(core_mask, %#x);
	DUMP_BOOT_INFO(dram_size, %d);
	DUMP_BOOT_INFO(phys_mem_desc_addr, %#x);
	DUMP_BOOT_INFO(debugger_flags_addr, %#x);
	DUMP_BOOT_INFO(eclock, %d);
	DUMP_BOOT_INFO(dclock, %d);
	DUMP_BOOT_INFO(board_type, %d);
	DUMP_BOOT_INFO(board_rev_major, %d);
	DUMP_BOOT_INFO(board_rev_minor, %d);
	DUMP_BOOT_INFO(mac_addr_count, %d);
	DUMP_BOOT_INFO(cf_common_addr, %#llx);
	DUMP_BOOT_INFO(cf_attr_addr, %#llx);
	DUMP_BOOT_INFO(led_display_addr, %#llx);
	DUMP_BOOT_INFO(dfaclock, %d);
	DUMP_BOOT_INFO(config_flags, %#x);
#endif

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
	tlb_set_pid(MIN_USER_ASID);

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

	xtlb_handler = (vaddr_t)&xtlb_miss;
	build_trampoline(TLB_MISS_EXC_VEC, xtlb_handler);
	build_trampoline(XTLB_MISS_EXC_VEC, xtlb_handler);

	/*
	 * Turn off bootstrap exception vectors.
	 * (this is done by PMON already, but it doesn't hurt to be safe)
	 */

	setsr(getsr() & ~SR_BOOT_EXC_VEC);
	proc0.p_md.md_regs->sr = getsr();

#ifdef DDB
	db_machine_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif

	/*
	 * Return the new kernel stack pointer.
	 */
	return ((vaddr_t)proc0paddr + USPACE - 64);
}

/*
 * Console initialization: called early on from main, before vm init or startup.
 * Do enough configuration to choose and initialize a console.
 */
void
consinit()
{
	static int console_ok = 0;

	if (console_ok == 0) {
		cninit();
		console_ok = 1;
	}
}

/*
 * cpu_startup: allocate memory for variable-sized tables, initialize CPU, and
 * do auto-configuration.
 */
void
cpu_startup()
{
	vaddr_t minaddr, maxaddr;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %lu (%luMB)\n", ptoa((psize_t)physmem),
	    ptoa((psize_t)physmem)/1024/1024);

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

	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024/1024);

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

int
octeon_cpuspeed(int *freq)
{
	extern struct boot_info *octeon_boot_info;
	*freq = octeon_boot_info->eclock / 1000000;
	return (0);
}

int
octeon_ioclock_speed(void)
{
	extern struct boot_info *octeon_boot_info;
	int chipid;
	u_int64_t mio_rst_boot;

	chipid = octeon_get_chipid();
	switch (octeon_model_family(chipid)) {
	case OCTEON_MODEL_FAMILY_CN61XX:
		mio_rst_boot = octeon_xkphys_read_8(MIO_RST_BOOT);
		return OCTEON_IO_REF_CLOCK * ((mio_rst_boot >>
		    MIO_RST_BOOT_PNR_MUL_SHIFT) & MIO_RST_BOOT_PNR_MUL_MASK);
		break;
	default:
		return octeon_boot_info->eclock;
	}
}

static u_int64_t
get_ncpusfound(void)
{
	extern struct boot_desc *octeon_boot_desc;
	uint64_t core_mask = octeon_boot_desc->core_mask;
	uint64_t i, m, ncpus = 0;

	for (i = 0, m = 1 ; i < OCTEON_MAXCPUS; i++, m <<= 1)
		if (core_mask & m)
			ncpus++;

	return ncpus;
}

static void
process_bootargs(void)
{
	int i;
	extern struct boot_desc *octeon_boot_desc;

	/*
	 * The kernel is booted via a bootoctlinux command. Thus we need to skip
	 * argv[0] when we start to decode the boot arguments (${bootargs}).
	 * Note that U-Boot doesn't pass us anything by default, we need to
	 * explicitly pass the rootdevice.
	 */
	for (i = 1; i < octeon_boot_desc->argc; i++ ) {
		const char *arg = (const char*)
		    PHYS_TO_XKPHYS(octeon_boot_desc->argv[i], CCA_CACHED);

		if (octeon_boot_desc->argv[i] == 0)
			continue;

#ifdef DEBUG
		printf("boot_desc->argv[%d] = %s\n", i, arg);
#endif

		/*
		 * XXX: We currently only expect one other argument,
		 * argv[1], rootdev=ROOTDEV.
		 */
		if (strncmp(arg, "rootdev=", 8) == 0) {
			if (*uboot_rootdev == '\0') {
				strlcpy(uboot_rootdev, arg,
					sizeof(uboot_rootdev));
				parse_uboot_root();
                        }
		}
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
			printf("System Power Down not supported,"
			" halting system.\n");
		else
			printf("System Halt.\n");
	} else {
		printf("System restart.\n");
		(void)disableintr();
		tlb_set_wired(0);
		tlb_flush(bootcpu_hwinfo.tlbsize);
		octeon_xkphys_write_8(OCTEON_CIU_BASE + CIU_SOFT_RST, 1);
	}

	for (;;) ;
	/* NOTREACHED */
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

void
dumpsys()
{
	/* XXX TBD */
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

#ifdef MULTIPROCESSOR
uint32_t cpu_spinup_mask = 0;
uint64_t cpu_spinup_a0, cpu_spinup_sp;
static int (*ipi_handler)(void *);

uint32_t ipi_intr(uint32_t, struct trap_frame *);

extern bus_space_t iobus_tag;
extern bus_space_handle_t iobus_h;

void
hw_cpu_boot_secondary(struct cpu_info *ci)
{
	vaddr_t kstack;

	kstack = alloc_contiguous_pages(USPACE);
	if (kstack == 0)
		panic("unable to allocate idle stack\n");
	ci->ci_curprocpaddr = (void *)kstack;
	cpu_spinup_a0 = (uint64_t)ci;
	cpu_spinup_sp = (uint64_t)(kstack + USPACE);
	cpu_spinup_mask = (uint32_t)ci->ci_cpuid;

	while (!cpuset_isset(&cpus_running, ci))
		;
}

void
hw_cpu_hatch(struct cpu_info *ci)
{
	int s;

	/*
	 * Set curcpu address on this processor.
	 */
	setcurcpu(ci);

	/*
	 * Make sure we can access the extended address space.
	 * Note that r10k and later do not allow XUSEG accesses
	 * from kernel mode unless SR_UX is set.
	 */
	setsr(getsr() | SR_KX | SR_UX);

	tlb_init(64);
	tlb_set_pid(0);

	/*
	 * Turn off bootstrap exception vectors.
	 */
	setsr(getsr() & ~SR_BOOT_EXC_VEC);

	/*
	 * Clear out the I and D caches.
	 */
	Octeon_ConfigCache(ci);
	Mips_SyncCache(ci);

	printf("cpu%lu launched\n", cpu_number());

	(*md_startclock)(ci);
	ncpus++;
	cpuset_add(&cpus_running, ci);
	octeon_intr_init();
	mips64_ipi_init();
	octeon_setintrmask(0);
	spl0();
	(void)updateimask(0);

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

/*
 * IPI dispatcher.
 */
uint32_t
ipi_intr(uint32_t hwpend, struct trap_frame *frame)
{
	u_long cpuid = cpu_number();

	/*
	 * Mask all pending interrupts.
	 */
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP3_EN0(cpuid), 0);

	ipi_handler((void *)cpuid);

	/*
	 * Reenable interrupts which have been serviced.
	 */
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP3_EN0(cpuid),
		(1ULL << CIU_INT_MBOX0)|(1ULL << CIU_INT_MBOX1));
	return hwpend;
}

int
hw_ipi_intr_establish(int (*func)(void *), u_long cpuid)
{
	if (cpuid == 0)
		ipi_handler = func;

	bus_space_write_8(&iobus_tag, iobus_h, CIU_MBOX_CLR(cpuid),
		0xffffffff);
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP3_EN0(cpuid),
		(1ULL << CIU_INT_MBOX0)|(1ULL << CIU_INT_MBOX1));
	set_intr(INTPRI_IPI, CR_INT_1, ipi_intr);

	return 0;
};

void
hw_ipi_intr_set(u_long cpuid)
{
	bus_space_write_8(&iobus_tag, iobus_h, CIU_MBOX_SET(cpuid), 1);
}

void
hw_ipi_intr_clear(u_long cpuid)
{
	uint64_t clr =
		bus_space_read_8(&iobus_tag, iobus_h, CIU_MBOX_CLR(cpuid));
	bus_space_write_8(&iobus_tag, iobus_h, CIU_MBOX_CLR(cpuid), clr);
}
#endif /* MULTIPROCESSOR */
