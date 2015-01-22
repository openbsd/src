/*	$OpenBSD: omap_machdep.c,v 1.3 2015/01/22 14:33:01 krw Exp $	*/
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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
#include <sys/types.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/termios.h>

#include <machine/bus.h>
#include <machine/bootconfig.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <arm/cortex/smc.h>
#include <arm/armv7/armv7var.h>
#include <armv7/armv7/armv7var.h>
#include <armv7/armv7/armv7_machdep.h>

extern void omap4_smc_call(uint32_t, uint32_t);
extern void omdog_reset(void);
extern int comcnspeed;
extern int comcnmode;

const char *platform_boot_name = "OpenBSD/omap";

void
platform_smc_write(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    uint32_t op, uint32_t val)
{
	switch (op) {
	case 0x100:	/* PL310 DEBUG */
	case 0x102:	/* PL310 CTL */
		break;
	default:
		panic("platform_smc_write: invalid operation %d", op);
	}

	omap4_smc_call(op, val);
}

void
platform_init_cons(void)
{
	paddr_t paddr;

	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
	case BOARD_ID_OMAP3_OVERO:
		paddr = 0x49020000;
		break;
	case BOARD_ID_AM335X_BEAGLEBONE:
		paddr = 0x44e09000;
		break;
	case BOARD_ID_OMAP4_PANDA:
		paddr = 0x48020000;
		break;
	}

	comcnattach(&armv7_a4x_bs_tag, paddr, comcnspeed, 48000000, comcnmode);
	comdefaultrate = comcnspeed;
}

void
platform_watchdog_reset(void)
{
	omdog_reset();
}

void
platform_powerdown(void)
{

}

void
platform_print_board_type(void)
{
	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
		printf("board type: beagle\n");
		break;
	case BOARD_ID_AM335X_BEAGLEBONE:
		printf("board type: beaglebone\n");
		break;
	case BOARD_ID_OMAP3_OVERO:
		printf("board type: overo\n");
		break;
	case BOARD_ID_OMAP4_PANDA:
		printf("board type: panda\n");
		break;
	default:
		printf("board type %x unknown", board_id);
	}
}

void
platform_bootconfig_dram(BootConfig *bootconfig, psize_t *memstart, psize_t *memsize)
{
	uint32_t sdrc_mcfg_0, sdrc_mcfg_1, memsize0, memsize1;
	int loop;

	if (bootconfig->dramblocks == 0) {
		sdrc_mcfg_0 = *(uint32_t *)0x6d000080;
		sdrc_mcfg_1 = *(uint32_t *)0x6d0000b0;
		memsize0 = (((sdrc_mcfg_0 >> 8))&0x3ff) * (2 * 1024 * 1024);
		memsize1 = (((sdrc_mcfg_1 >> 8))&0x3ff) * (2 * 1024 * 1024);
		*memsize = memsize0 + memsize1;

		*memstart = SDRAM_START;
		*memsize =  0x02000000; /* 32MB */
		/* Fake bootconfig structure for the benefit of pmap.c */
		/* XXX must make the memory description h/w independant */
		bootconfig->dram[0].address = *memstart;
		bootconfig->dram[0].pages = memsize0 / PAGE_SIZE;
		bootconfig->dramblocks = 1;
		if (memsize1 != 0) {
			bootconfig->dram[1].address = bootconfig->dram[0].address
			    + memsize0; /* XXX */
			bootconfig->dram[1].pages = memsize1 / PAGE_SIZE;
			bootconfig->dramblocks++; /* both banks populated */
		}
	} else {
		/* doesn't deal with multiple segments, hopefully u-boot collaped them into one */
		*memstart = bootconfig->dram[0].address;
		*memsize = bootconfig->dram[0].pages * PAGE_SIZE;
		printf("memory size derived from u-boot\n");
		for (loop = 0; loop < bootconfig->dramblocks; loop++) {
			printf("bootconf.mem[%d].address = %08x pages %d/0x%08x\n",
			    loop, bootconfig->dram[0].address, bootconfig->dram[0].pages,
			        bootconfig->dram[0].pages * PAGE_SIZE);
		}
	}

}

void
platform_disable_l2_if_needed(void)
{
	switch (board_id) {
	case BOARD_ID_OMAP4_PANDA:
		/* disable external L2 cache */
		omap4_smc_call(0x102, 0);
		break;
	}
}
