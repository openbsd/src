/*	$OpenBSD: sunxi_machdep.c,v 1.8 2015/05/19 03:30:54 jsg Exp $	*/
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
extern struct board_dev *sunxi_board_devs(void);
extern void sunxi_board_init(void);
extern int comcnspeed;
extern int comcnmode;

void
sunxi_platform_smc_write(bus_space_tag_t iot, bus_space_handle_t ioh,
    bus_size_t off, uint32_t op, uint32_t val)
{

}

void
sunxi_platform_init_cons(void)
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
sunxi_platform_watchdog_reset(void)
{
	sxidog_reset();
}

void
sunxi_platform_powerdown(void)
{

}

const char *
sunxi_platform_board_name(void)
{
	return (sunxi_board_name());
}

void
sunxi_platform_disable_l2_if_needed(void)
{

}

void
sunxi_platform_board_init(void)
{
	sunxi_board_init();
}

struct armv7_platform sunxi_platform = {
	.boot_name = "OpenBSD/sunxi",
	.board_name = sunxi_platform_board_name,
	.board_init = sunxi_platform_board_init,
	.smc_write = sunxi_platform_smc_write,
	.init_cons = sunxi_platform_init_cons,
	.watchdog_reset = sunxi_platform_watchdog_reset,
	.powerdown = sunxi_platform_powerdown,
	.disable_l2_if_needed = sunxi_platform_disable_l2_if_needed,
};

struct armv7_platform *
sunxi_platform_match(void)
{
	struct board_dev *devs;

	devs = sunxi_board_devs();
	if (devs == NULL)
		return (NULL);

	sunxi_platform.devs = devs;
	return (&sunxi_platform);
}
