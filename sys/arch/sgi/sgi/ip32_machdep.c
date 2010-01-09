/*	$OpenBSD: ip32_machdep.c,v 1.13 2010/01/09 20:33:16 miod Exp $ */

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
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/tty.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/memconf.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebus.h>
#include <sgi/localbus/macebusvar.h>

#include <dev/ic/comvar.h>

extern char *hw_prod;

void crime_configure_memory(void);

void
crime_configure_memory(void)
{
	volatile u_int64_t *bank_ctrl;
	paddr_t addr;
	psize_t size;
	u_int64_t ctrl0, ctrl;
	u_int64_t first_page, last_page;
	int bank;
#ifdef DEBUG
	int i;
#endif

	bank_ctrl =
	    (void *)PHYS_TO_CKSEG1(CRIMEBUS_BASE + CRIME_MEM_BANK0_CONTROL);
	for (bank = 0; bank < CRIME_MAX_BANKS; bank++) {
		ctrl = bank_ctrl[bank];
		addr = (ctrl & CRIME_MEM_BANK_ADDR) << 25;
		size = (ctrl & CRIME_MEM_BANK_128MB) ? 128 : 32;

		/*
		 * Empty banks are reported as duplicates of bank #0.
		 */
		if (bank == 0)
			ctrl0 = ctrl;
		else if (ctrl == ctrl0)
			continue;

#ifdef DEBUG
		bios_printf("crime: bank %d contains %ld MB at %p\n",
		    bank, size, addr);
#endif

		/*
		 * Do not report memory regions below 256MB, since ARCBIOS
		 * will do.
		 */
		if (addr < 256 * 1024 * 1024)
			continue;

		addr += CRIME_MEMORY_OFFSET;
		size *= 1024 * 1024;
		first_page = atop(addr);
		last_page = atop(addr + size);

		memrange_register(first_page, last_page, 0,
		    VM_FREELIST_DEFAULT);
	}

#ifdef DEBUG
	for (i = 0; i < MAXMEMSEGS; i++)
		if (mem_layout[i].mem_last_page != 0)
			bios_printf("MEM %d, %p to %p\n", i,
			    ptoa(mem_layout[i].mem_first_page),
			    ptoa(mem_layout[i].mem_last_page));
#endif
}

void
ip32_setup()
{
	u_long cpuspeed;

	uncached_base = PHYS_TO_XKPHYS(0, CCA_NC);

	crime_configure_memory();

	/* R12K O2s must run with DSD on. */
	switch ((cp0_get_prid() >> 8) & 0xff) {
	case MIPS_R12000:
		setsr(getsr() | SR_DSD);
		break;
	}

	bootcpu_hwinfo.c0prid = cp0_get_prid();
	bootcpu_hwinfo.c1prid = cp1_get_prid();
	cpuspeed = bios_getenvint("cpufreq");
	if (cpuspeed < 100)
		cpuspeed = 180;		/* reasonable default */
	bootcpu_hwinfo.clock = cpuspeed * 1000000;
	bootcpu_hwinfo.type = (bootcpu_hwinfo.c0prid >> 8) & 0xff;

	/*
	 * Figure out how many TLB are available.
	 */
	switch (bootcpu_hwinfo.type) {
#ifdef CPU_RM7000
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
		bootcpu_hwinfo.tlbsize = 48;
		if (((bootcpu_hwinfo.c0prid >> 4) & 0x0f) >= 2) {
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
				bootcpu_hwinfo.tlbsize = 64;
		}
		break;
#endif
#ifdef CPU_R10000
	case MIPS_R10000:
	case MIPS_R12000:
	case MIPS_R14000:
		bootcpu_hwinfo.tlbsize = 64;
		break;
#endif
	default:	/* R5000, RM52xx */
		bootcpu_hwinfo.tlbsize = 48;
		break;
	}

	comconsaddr = MACE_ISA_SER1_OFFS;
	comconsfreq = 1843200;
	comconsiot = &macebus_tag;
	comconsrate = bios_getenvint("dbaud");
	if (comconsrate < 50 || comconsrate > 115200)
		comconsrate = 9600;

	/* not sure if there is a way to tell O2 and O2+ apart */
	hw_prod = "O2";
}
