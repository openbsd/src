/*	$OpenBSD: ip32_machdep.c,v 1.7 2009/10/14 20:21:16 miod Exp $ */

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

#include <dev/ic/comvar.h>

extern char *hw_prod;

void crime_configure_memory(void);

void
crime_configure_memory(void)
{
	struct phys_mem_desc *m;
	volatile u_int64_t *bank_ctrl;
	paddr_t addr;
	psize_t size;
	u_int64_t ctrl0, ctrl;
	u_int32_t first_page, last_page;
	int bank, i;

	bank_ctrl = (void *)PHYS_TO_KSEG1(CRIMEBUS_BASE + CRIME_MEM_BANK0_CONTROL);
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

		/*
		 * Try to coalesce with other memory segments if banks are 
		 * contiguous.
		 */
		m = NULL;
		for (i = 0; i < MAXMEMSEGS; i++) {
			if (mem_layout[i].mem_last_page == 0) {
				if (m == NULL)
					m = &mem_layout[i];
			} else if (last_page == mem_layout[i].mem_first_page) {
				m = &mem_layout[i];
				m->mem_first_page = first_page;
			} else if (mem_layout[i].mem_last_page == first_page) {
				m = &mem_layout[i];
				m->mem_last_page = last_page;
			}
		}
		if (m != NULL) {
			if (m->mem_last_page == 0) {
				m->mem_first_page = first_page;
				m->mem_last_page = last_page;
			}
			m->mem_freelist = VM_FREELIST_DEFAULT;
			physmem += atop(size);
		}
	}

#ifdef DEBUG
	for (i = 0; i < MAXMEMSEGS; i++)
		if (mem_layout[i].mem_first_page)
			bios_printf("MEM %d, 0x%x to  0x%x\n",i,
				ptoa(mem_layout[i].mem_first_page),
				ptoa(mem_layout[i].mem_last_page));
#endif
}

void
ip32_setup()
{
	uncached_base = PHYS_TO_XKPHYS(0, CCA_NC);

	crime_configure_memory();

	/* R12K O2s must run with DSD on. */
	switch ((cp0_get_prid() >> 8) & 0xff) {
	case MIPS_R12000:
		setsr(getsr() | SR_DSD);
		break;
	}

	comconsaddr = MACE_ISA_SER1_OFFS;
	comconsfreq = 1843200;
	comconsiot = &macebus_tag;

	/* not sure if there is a way to tell O2 and O2+ apart */
	hw_prod = "O2";
}
