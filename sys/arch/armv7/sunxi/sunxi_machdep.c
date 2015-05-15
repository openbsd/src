/*	$OpenBSD: sunxi_machdep.c,v 1.6 2015/05/15 15:35:43 jsg Exp $	*/
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

extern int sxiuartcnattach(bus_space_tag_t, bus_addr_t, int, long, tcflag_t);
extern void sxidog_reset(void);
extern char *sunxi_board_name(void);
extern int comcnspeed;
extern int comcnmode;

const char *platform_boot_name = "OpenBSD/sunxi";

void
platform_smc_write(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    uint32_t op, uint32_t val)
{

}

void
platform_init_cons(void)
{
	paddr_t paddr;

	switch (board_id) {
	case BOARD_ID_SUN4I_A10:
	case BOARD_ID_SUN7I_A20:
		paddr = 0x01c28000;	/* UART0 */
		break;
	default:
		sxiuartcnattach(&armv7_a4x_bs_tag, paddr, comcnspeed,
		    24000000, comcnmode);
		panic("board type %x unknown", board_id);
	}

	sxiuartcnattach(&armv7_a4x_bs_tag, paddr, comcnspeed, 24000000,
	    comcnmode);
}

void
platform_watchdog_reset(void)
{
	sxidog_reset();
}

void
platform_powerdown(void)
{

}

const char *
platform_board_name(void)
{
	return (sunxi_board_name());
}

void
platform_bootconfig_dram(BootConfig *bootconfig, psize_t *memstart, psize_t *memsize)
{
	int loop;

	if (bootconfig->dramblocks == 0) {
		*memstart = SDRAM_START;
		*memsize = 0x10000000; /* 256 MB */
		/* Fake bootconfig structure for the benefit of pmap.c */
		/* XXX must make the memory description h/w independant */
		bootconfig->dram[0].address = *memstart;
		bootconfig->dram[0].pages = *memsize / PAGE_SIZE;
		bootconfig->dramblocks = 1;
	} else {
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

}

