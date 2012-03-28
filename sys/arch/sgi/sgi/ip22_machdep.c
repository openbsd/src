/*	$OpenBSD: ip22_machdep.c,v 1.1 2012/03/28 20:44:23 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/memconf.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <sgi/sgi/ip22.h>
#include <sgi/localbus/imcreg.h>
#include <sgi/localbus/imcvar.h>
#include <sgi/hpc/hpcreg.h>
#include <sgi/hpc/iocreg.h>

extern char *hw_prod;

void ip22_arcbios_walk(void);
int ip22_arcbios_walk_component(arc_config_t *);
void ip22_memory_setup(void);

/*
 * Walk the ARCBios component tree to get L2 cache information.
 * This is the only way we can get the L2 cache size.
 */

int
ip22_arcbios_walk_component(arc_config_t *cf)
{
	struct cpu_info *ci = curcpu();
	arc_config_t *child;

	/*
	 * Split secondary caches are not supported.
	 * No IP22 processor module uses them anyway.
	 */
	if (cf->class == arc_CacheClass && cf->type == arc_SecondaryCache) {
		/*
		 * Secondary cache information is encoded as WWLLSSSS, where
		 * WW is the number of ways (should be 01)
		 * LL is Log2(line size) (should be 04 or 05)
		 * SS is Log2(cache size in 4KB units) (should be 0007)
		 */
		ci->ci_l2size = (1 << 12) << (cf->key & 0x0000ffff);
		/* L2 line size */
		ci->ci_cacheconfiguration = 1 << ((cf->key >> 16) & 0xff);
		return 0;	/* abort walk */
	}

	/*
	 * It is safe to assume we have a 32-bit ARCBios, until
	 * IP26 and IP28 support is added, hence unconditional
	 * use of arc_config_t.
	 */
	for (child = (arc_config_t *)Bios_GetChild(cf); child != NULL;
	    child = (arc_config_t *)Bios_GetPeer(child)) {
		if (ip22_arcbios_walk_component(child) == 0)
			return 0;
	}

	return 1;	/* continue walk */
}

void
ip22_arcbios_walk()
{
	(void)ip22_arcbios_walk_component((arc_config_t *)Bios_GetChild(NULL));
}

#define	IMC_NREGION	3

void
ip22_memory_setup()
{
	uint i, bank, shift;
	uint32_t memc0, memc1;
	uint32_t memc;
	paddr_t base[IMC_NREGION], size[IMC_NREGION], limit;
	paddr_t start0, end0, start1, end1;
	struct phys_mem_desc *mem;

	/*
	 * Figure out the top of memory, as reported by ARCBios.
	 */

	limit = 0;
	for (i = 0, mem = mem_layout; i < MAXMEMSEGS; i++, mem++) {
		if (mem->mem_last_page > limit)
			limit = mem->mem_last_page;
	}
	limit = ptoa(limit);

	/*
	 * Figure out where the memory controller has put memory.
	 */

	memc0 = imc_read(IMC_MEMCFG0);
	memc1 = imc_read(IMC_MEMCFG1);

	shift = IMC_MEMC_LSHIFT;
	/* Revision D onwards uses larger units, to allow for more memory */
        if ((imc_read(IMC_SYSID) & IMC_SYSID_REVMASK) >= 5)
		shift = IMC_MEMC_LSHIFT_HUGE;

	for (bank = 0; bank < IMC_NREGION; bank++) {
		memc = (bank & 2) ? memc1 : memc0;
		if ((bank & 1) == 0)
			memc >>= IMC_MEMC_BANK_SHIFT;
		memc &= IMC_MEMC_BANK_MASK;

		if ((memc & IMC_MEMC_VALID) == 0) {
			base[bank] = size[bank] = 0;
			continue;
		}

		base[bank] = (memc & IMC_MEMC_ADDR_MASK) >> IMC_MEMC_ADDR_SHIFT;
		base[bank] <<= shift;

		size[bank] = (memc & IMC_MEMC_SIZE_MASK) >> IMC_MEMC_SIZE_SHIFT;
		size[bank]++;
		size[bank] <<= shift;
	}

	/*
	 * Perform sanity checks on the above data..
	 */

	/* memory should not start below 128MB */
	for (bank = 0; bank < IMC_NREGION; bank++)
		if (size[bank] != 0 && base[bank] < (1ULL << 27))
			goto dopanic;

	/* banks should not overlap */
	for (bank = 1; bank < IMC_NREGION; bank++) {
		if (size[bank] == 0)
			continue;
		start0 = base[bank];
		end0 = base[bank] + size[bank];
		for (i = 0; i < bank; i++) {
			if (size[i] == 0)
				continue;
			start1 = base[i];
			end1 = base[i] + size[i];
			if (end0 > start1 && start0 < end1)
				goto dopanic;
		}
	}

	/*
	 * Now register all the memory beyond what ARCBios stopped at.
	 */

	for (bank = 0; bank < IMC_NREGION; bank++) {
		if (size[bank] == 0)
			continue;

		start0 = base[bank];
		end0 = base[bank] + size[bank];
		if (end0 <= limit)
			continue;

		if (start0 < limit)
			start0 = limit;

		memrange_register(atop(start0), atop(end0), 0);
	}

	return;

dopanic:
	bios_printf("** UNEXPECTED MEMORY CONFIGURATION **\n");
	bios_printf("MEMC0 %08x MEMC1 %08x\n", memc0, memc1);
	bios_printf("Please contact <sgi@openbsd.org>\n"
	    "Halting system.\n");
	Bios_Halt();
	for (;;) ;
}

void
ip22_setup()
{
	u_long cpuspeed;
	volatile uint32_t *sysid;

	/*
	 * Get CPU information.
	 */
	bootcpu_hwinfo.c0prid = cp0_get_prid();
	bootcpu_hwinfo.c1prid = cp1_get_prid();
	cpuspeed = bios_getenvint("cpufreq");
	if (sys_config.system_type == SGI_IP20)
		cpuspeed <<= 1;
	if (cpuspeed < 100)
		cpuspeed = 100;		/* reasonable default */
	bootcpu_hwinfo.clock = cpuspeed * 1000000;
	bootcpu_hwinfo.type = (bootcpu_hwinfo.c0prid >> 8) & 0xff;

	/*
	 * Figure out what critter we are running on.
	 */
	switch (sys_config.system_type) {
	case SGI_IP20:
		hw_prod = "Indigo";
		break;
	case SGI_IP22:
		sysid = (volatile uint32_t *)
		    PHYS_TO_XKPHYS(HPC_BASE_ADDRESS_0 + IOC_BASE + IOC_SYSID,
		      CCA_NC);
		if (*sysid & 0x01) {
			sys_config.system_subtype = IP22_INDIGO2;
			hw_prod = "Indigo2";
		} else {
			sys_config.system_subtype = IP22_INDY;
			hw_prod = "Indy";
		}
		break;
	case SGI_IP26:
		sys_config.system_subtype = IP22_INDIGO2;
		hw_prod = "POWER Indigo2 R8000";
		break;
	case SGI_IP28:
		sys_config.system_subtype = IP22_INDIGO2;
		hw_prod = "POWER Indigo2 R10000";
		break;
	}

	/*
	 * Figure out how many TLB entries are available.
	 */
	switch (bootcpu_hwinfo.type) {
#ifdef CPU_R10000
	case MIPS_R10000:
		bootcpu_hwinfo.tlbsize = 64;
		break;
#endif
	default:	/* R4x00, R5000 */
		bootcpu_hwinfo.tlbsize = 48;
		break;
	}

	/*
	 * Compute memory layout. ARCBios may not report all memory (on
	 * Indigo, it seems to only report up to 128MB, and on Indigo2,
	 * up to 256MB).
	 */
	ip22_memory_setup();

	/*
	 * Scan ARCBios component list for L2 cache information.
	 */
	ip22_arcbios_walk();

	_device_register = arcs_device_register;
}
