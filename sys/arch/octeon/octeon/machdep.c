/*	$OpenBSD: machdep.c,v 1.108 2018/12/18 14:24:02 visa Exp $ */

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
#include <sys/msgbuf.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/exec_elf.h>
#include <sys/timetc.h>
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
#include <dev/ofw/fdt.h>

#include <octeon/dev/cn30xxcorereg.h>
#include <octeon/dev/cn30xxipdreg.h>
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

extern uint8_t dt_blob_start[];

struct boot_desc *octeon_boot_desc;
struct boot_info *octeon_boot_info;

void		*octeon_fdt;
unsigned int	 octeon_ver;

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
vaddr_t		mips_init(register_t, register_t, register_t, register_t);
boolean_t 	is_memory_range(paddr_t, psize_t, psize_t);
void		octeon_memory_init(struct boot_info *);
int		octeon_cpuspeed(int *);
void		octeon_tlb_init(void);
static void	process_bootargs(void);
static uint64_t	get_ncpusfound(void);

extern void 	parse_uboot_root(void);

cons_decl(cn30xxuart);
struct consdev uartcons = cons_init(cn30xxuart);

u_int		ioclock_get_timecount(struct timecounter *);

struct timecounter ioclock_timecounter = {
	.tc_get_timecount = ioclock_get_timecount,
	.tc_poll_pps = NULL,
	.tc_counter_mask = 0xffffffff,	/* truncated to 32 bits */
	.tc_frequency = 0,		/* determined at runtime */
	.tc_name = "ioclock",
	.tc_quality = 0,		/* ioclock can be overridden
					 * by cp0 counter */
	.tc_priv = 0			/* clock register,
					 * determined at runtime */
};

void
octeon_memory_init(struct boot_info *boot_info)
{
	struct octeon_bootmem_block *block;
	struct octeon_bootmem_desc *memdesc;
	paddr_t blockaddr;
	uint64_t fp, lp;
	int i;

	physmem = atop((uint64_t)boot_info->dram_size << 20);

	if (boot_info->phys_mem_desc_addr == 0)
		panic("bootmem desc is missing");
	memdesc = (struct octeon_bootmem_desc *)PHYS_TO_XKPHYS(
	    boot_info->phys_mem_desc_addr, CCA_CACHED);
	printf("bootmem desc 0x%x version %d.%d\n",
	    boot_info->phys_mem_desc_addr, memdesc->major_version,
	    memdesc->minor_version);
	if (memdesc->major_version > 3)
		panic("unhandled bootmem desc version %d.%d",
		    memdesc->major_version, memdesc->minor_version);

	blockaddr = memdesc->head_addr;
	if (blockaddr == 0)
		panic("bootmem list is empty");
	for (i = 0; i < MAXMEMSEGS && blockaddr != 0; blockaddr = block->next) {
		block = (struct octeon_bootmem_block *)PHYS_TO_XKPHYS(
		    blockaddr, CCA_CACHED);
		printf("avail phys mem 0x%016lx - 0x%016lx\n", blockaddr,
		    (paddr_t)(blockaddr + block->size));

		fp = atop(round_page(blockaddr));
		lp = atop(trunc_page(blockaddr + block->size));

		/* Clamp to the range of the pmap. */
		if (fp > atop(pfn_to_pad(PG_FRAME)))
			continue;
		if (lp > atop(pfn_to_pad(PG_FRAME)) + 1)
			lp = atop(pfn_to_pad(PG_FRAME)) + 1;
		if (fp >= lp)
			continue;

		/* Skip small fragments. */
		if (lp - fp < atop(1u << 20))
			continue;

		mem_layout[i].mem_first_page = fp;
		mem_layout[i].mem_last_page = lp;
		i++;
	}

	printf("Total DRAM Size 0x%016llX\n",
	    (uint64_t)boot_info->dram_size << 20);

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
mips_init(register_t a0, register_t a1, register_t a2, register_t a3)
{
	uint prid;
	vaddr_t xtlb_handler;
	int i;
	struct boot_desc *boot_desc;
	struct boot_info *boot_info;
	uint32_t config4;

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

	switch (octeon_model_family(prid)) {
	default:
		octeon_ver = OCTEON_1;
		break;
	case OCTEON_MODEL_FAMILY_CN50XX:
		octeon_ver = OCTEON_PLUS;
		break;
	case OCTEON_MODEL_FAMILY_CN61XX:
	case OCTEON_MODEL_FAMILY_CN63XX:
	case OCTEON_MODEL_FAMILY_CN66XX:
	case OCTEON_MODEL_FAMILY_CN68XX:
		octeon_ver = OCTEON_2;
		break;
	case OCTEON_MODEL_FAMILY_CN71XX:
	case OCTEON_MODEL_FAMILY_CN73XX:
	case OCTEON_MODEL_FAMILY_CN78XX:
		octeon_ver = OCTEON_3;
		break;
	}

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
	if (cp0_get_config_1() & CONFIG1_FP)
		bootcpu_hwinfo.c1prid = cp1_get_prid();
	else
		bootcpu_hwinfo.c1prid = 0;

	bootcpu_hwinfo.tlbsize = 1 + ((cp0_get_config_1() & CONFIG1_MMUSize1)
	    >> CONFIG1_MMUSize1_SHIFT);
	if (cp0_get_config_3() & CONFIG3_M) {
		config4 = cp0_get_config_4();
		if (((config4 & CONFIG4_MMUExtDef) >>
		    CONFIG4_MMUExtDef_SHIFT) == 1)
			bootcpu_hwinfo.tlbsize +=
			    (config4 & CONFIG4_MMUSizeExt) << 6;
	}

	bcopy(&bootcpu_hwinfo, &curcpu()->ci_hw, sizeof(struct cpu_hwinfo));

	/*
	 * Configure cache.
	 */

	Octeon_ConfigCache(curcpu());
	Octeon_SyncCache(curcpu());

	octeon_tlb_init();

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
	 * Save the FDT and let the system use it.
	 */
	if (octeon_boot_info->ver_minor >= 3 &&
	    octeon_boot_info->fdt_addr != 0) {
		void *fdt;
		size_t fdt_size;

		fdt = (void *)PHYS_TO_XKPHYS(octeon_boot_info->fdt_addr,
		    CCA_CACHED);
		if (fdt_init(fdt) != 0 && (fdt_size = fdt_get_size(fdt)) != 0) {
			octeon_fdt = (void *)pmap_steal_memory(fdt_size, NULL,
			    NULL);
			memcpy(octeon_fdt, fdt, fdt_size);
			fdt_init(octeon_fdt);
		}
	} else
		fdt_init(dt_blob_start);

	/*
	 * Get a console, very early but after initial mapping setup.
	 */

	consinit();
	printf("Initial setup done, switching console.\n");

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
	if (octeon_boot_info->ver_minor >= 3)
		DUMP_BOOT_INFO(fdt_addr, %#llx);

	/*
	 * It is possible to launch the kernel from the bootloader without
	 * physical CPU 0. That does not really work, however, because of the
	 * way how the kernel assigns and uses cpuids. Moreover, cnmac(4) is
	 * hard coded to use CPU 0 for packet reception.
	 */
	if (!(octeon_boot_info->core_mask & 1))
		panic("cannot run without physical CPU 0");

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
	proc0.p_md.md_regs = (struct trapframe *)&proc0paddr->u_pcb.pcb_regs;
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
		db_enter();
#endif

	switch (octeon_model_family(prid)) {
	case OCTEON_MODEL_FAMILY_CN73XX:
		ioclock_timecounter.tc_priv = (void *)FPA3_CLK_COUNT;
		break;
	default:
		ioclock_timecounter.tc_priv = (void *)IPD_CLK_COUNT;
		break;
	}
	ioclock_timecounter.tc_frequency = octeon_ioclock_speed();
	tc_init(&ioclock_timecounter);

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
		com_fdt_init_cons();
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
	printf("%s", version);
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
	u_int64_t mio_rst_boot, rst_boot;

	switch (octeon_ver) {
	case OCTEON_2:
		mio_rst_boot = octeon_xkphys_read_8(MIO_RST_BOOT);
		return OCTEON_IO_REF_CLOCK * ((mio_rst_boot >>
		    MIO_RST_BOOT_PNR_MUL_SHIFT) & MIO_RST_BOOT_PNR_MUL_MASK);
	case OCTEON_3:
		rst_boot = octeon_xkphys_read_8(RST_BOOT);
		return OCTEON_IO_REF_CLOCK * ((rst_boot >>
		    RST_BOOT_PNR_MUL_SHIFT) & RST_BOOT_PNR_MUL_MASK);
	default:
		return octeon_boot_info->eclock;
	}
}

void
octeon_tlb_init(void)
{
	uint64_t cvmmemctl;
	uint32_t hwrena = 0;
	uint32_t pgrain = 0;
	int chipid;

	chipid = octeon_get_chipid();
	switch (octeon_model_family(chipid)) {
	case OCTEON_MODEL_FAMILY_CN73XX:
		/* Enable LMTDMA/LMTST transactions. */
		cvmmemctl = octeon_get_cvmmemctl();
		cvmmemctl |= COP_0_CVMMEMCTL_LMTENA;
		cvmmemctl &= ~COP_0_CVMMEMCTL_LMTLINE_M;
		cvmmemctl |= 2ull << COP_0_CVMMEMCTL_LMTLINE_S;
		octeon_set_cvmmemctl(cvmmemctl);
		break;
	}

	/*
	 * Make sure Coprocessor 2 is disabled.
	 */
	setsr(getsr() & ~SR_COP_2_BIT);

	/*
	 * If the UserLocal register is available, let userspace
	 * access it using the RDHWR instruction.
	 */
	if (cp0_get_config_3() & CONFIG3_ULRI) {
		cp0_set_userlocal(NULL);
		hwrena |= HWRENA_ULR;
		cpu_has_userlocal = 1;
	}
	cp0_set_hwrena(hwrena);

#ifdef MIPS_PTE64
	pgrain |= PGRAIN_ELPA;
#endif
	if (cp0_get_config_3() & CONFIG3_RXI)
		pgrain |= PGRAIN_XIE;
	cp0_set_pagegrain(pgrain);

	tlb_init(bootcpu_hwinfo.tlbsize);
}

static u_int64_t
get_ncpusfound(void)
{
	uint64_t core_mask;
	uint64_t i, ncpus = 0;
	int chipid;

	chipid = octeon_get_chipid();
	switch (octeon_model_family(chipid)) {
	case OCTEON_MODEL_FAMILY_CN73XX:
	case OCTEON_MODEL_FAMILY_CN78XX:
		core_mask = octeon_xkphys_read_8(OCTEON_CIU3_BASE + CIU3_FUSE);
		break;
	default:
		core_mask = octeon_xkphys_read_8(OCTEON_CIU_BASE + CIU_FUSE);
		break;
	}

	/* There has to be 1-to-1 mapping between cpuids and coreids. */
	for (i = 0; i < OCTEON_MAXCPUS && (core_mask & (1ul << i)) != 0; i++)
		ncpus++;

	return ncpus;
}

static void
process_bootargs(void)
{
	int i;
	extern struct boot_desc *octeon_boot_desc;

	/*
	 * U-Boot doesn't pass us anything by default, we need to explicitly
	 * pass the rootdevice.
	 */
	for (i = 0; i < octeon_boot_desc->argc; i++ ) {
		const char *arg = (const char*)
		    PHYS_TO_XKPHYS(octeon_boot_desc->argv[i], CCA_CACHED);

		if (octeon_boot_desc->argv[i] == 0)
			continue;

#ifdef DEBUG
		printf("boot_desc->argv[%d] = %s\n", i, arg);
#endif

		/*
		 * XXX: We currently only expect one other argument,
		 * rootdev=ROOTDEV.
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
cpu_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
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

		if (octeon_ver == OCTEON_3)
			octeon_xkphys_write_8(RST_SOFT_RST, 1);
		else
			octeon_xkphys_write_8(OCTEON_CIU_BASE +
			    CIU_SOFT_RST, 1);
	}

	for (;;)
		continue;
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

u_int
ioclock_get_timecount(struct timecounter *tc)
{
	uint64_t reg = (uint64_t)tc->tc_priv;

	return octeon_xkphys_read_8(reg);
}

#ifdef MULTIPROCESSOR
uint32_t cpu_spinup_mask = 0;
uint64_t cpu_spinup_a0, cpu_spinup_sp;

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
	mips_sync();

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

	octeon_tlb_init();
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

	(*md_startclock)(ci);
	ncpus++;
	cpuset_add(&cpus_running, ci);
	octeon_intr_init();
	mips64_ipi_init();
	spl0();
	(void)updateimask(0);

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}
#endif /* MULTIPROCESSOR */
