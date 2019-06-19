/*	$OpenBSD: ip32_machdep.c,v 1.23 2018/12/13 16:35:07 visa Exp $ */

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
#include <mips64/mips_cpu.h>
#include <machine/memconf.h>

#include <mips64/arcbios.h>

#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebus.h>
#include <sgi/localbus/macebusvar.h>

#include <dev/ic/comvar.h>

extern char *hw_prod;
extern int tlb_set_wired_get_random(int);	/* tlbhandler.S */

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

		memrange_register(first_page, last_page, 0);
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

	/*
	 * IP32 PROM version 4.18:
	 *	PROM Monitor (BE)
	 *	Tue Oct 22 10:58:00 PDT 2002
	 *	VERSION 4.18
	 *	O2 R5K/R7K/R10K/R12K
	 *	IRIX 6.5.x IP32prom IP32PROM-v4
	 * has incorrect function pointers for all ARCBios calls running in
	 * CKSEG1 (all the reboot/restart routines).
	 *
	 * We attempt to detect this and fix the function pointers here.
	 *
	 * XXX 4.16 and 4.17 might need similar fixes. 4.15 is sane.
	 */

	if ((vaddr_t)bios_halt == RESET_EXC_VEC + 0x161c &&
	    (vaddr_t)bios_powerdown == RESET_EXC_VEC + 0x1648 &&
	    (vaddr_t)bios_restart == RESET_EXC_VEC + 0x1674 &&
	    (vaddr_t)bios_reboot == RESET_EXC_VEC + 0x16a0 &&
	    (vaddr_t)bios_eim == RESET_EXC_VEC + 0x15f0 &&
	    *(uint32_t *)bios_halt == 0xafbf0014 &&
	    *(uint32_t *)bios_powerdown == 0xafbf0014 &&
	    *(uint32_t *)bios_restart == 0xafb1001c &&
	    *(uint32_t *)bios_eim == 0xafbf0014) {
		bios_halt = (void (*)(void))(RESET_EXC_VEC + 0x15bc);
		bios_powerdown = (void (*)(void))(RESET_EXC_VEC + 0x15e8);
		bios_restart = (void (*)(void))(RESET_EXC_VEC + 0x1614);
		bios_reboot = (void (*)(void))(RESET_EXC_VEC + 0x1640);
		bios_eim = (void (*)(void))(RESET_EXC_VEC + 0x1590);
	}

	crime_configure_memory();

	/* R12K O2s must run with DSD on. */
	switch ((cp0_get_prid() >> 8) & 0xff) {
	case MIPS_R12000:
		setsr(getsr() | SR_DSD);
		protosr |= SR_DSD;
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
		 * checked by writing to the wired register, which
		 * sets the random register to the number of available
		 * TLB entries.
		 * As its value decreases with every instruction
		 * executed, we use a combined write-then-read routine
		 * which will return a number close enough to the
		 * number of entries, so that any value larger than 48
		 * means that there are 64 entries available
		 * (in the current state of that code, the value will
		 * be the number of entries, minus 2).
		 */
		bootcpu_hwinfo.tlbsize = 48;
		if ((bootcpu_hwinfo.c0prid & 0xf0) >= 0x20) {
			/*
			 * The whole 64 entries exist, although the last
			 * 16 may not be used by the random placement
			 * operations, as we are about to check; but we
			 * need to make them invalid anyway.
			 */
			tlb_set_wired(48);
			tlb_flush(64);
			if (tlb_set_wired_get_random(0) > 48)
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

	/* Setup serial console if ARCS is telling us not to use video. */
	if (strncmp(bios_console, "video", 5) != 0) {
		comconsaddr = MACE_ISA_SER1_OFFS;
		comconsfreq = 1843200;
		comconsiot = &macebus_tag;
		comconsrate = bios_consrate;
	}

	/* not sure if there is a way to tell O2 and O2+ apart */
	hw_prod = "O2";

	_device_register = arcs_device_register;
}
