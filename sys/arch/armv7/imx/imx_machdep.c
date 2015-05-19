/*	$OpenBSD: imx_machdep.c,v 1.15 2015/05/19 03:30:54 jsg Exp $	*/
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
#include <armv7/imx/imxuartvar.h>
#include <armv7/armv7/armv7_machdep.h>

extern void imxdog_reset(void);
extern char *imx_board_name(void);
extern struct board_dev *imx_board_devs(void);
extern void imx_board_init(void);
extern int comcnspeed;
extern int comcnmode;

void
imx_platform_smc_write(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    uint32_t op, uint32_t val)
{
	bus_space_write_4(iot, ioh, off, val);
}

void
imx_platform_init_cons(void)
{
	paddr_t paddr;

	switch (board_id) {
	/* UART1 */
	case BOARD_ID_IMX6_CUBOXI:
	case BOARD_ID_IMX6_HUMMINGBOARD:
	case BOARD_ID_IMX6_SABRESD:
	case BOARD_ID_IMX6_WANDBOARD:
		paddr = 0x02020000;
		break;
	/* UART2 */
	case BOARD_ID_IMX6_SABRELITE:
	case BOARD_ID_IMX6_UDOO:
	case BOARD_ID_IMX6_NOVENA:
		paddr = 0x021e8000;
		break;
	/* UART4 */
	case BOARD_ID_IMX6_PHYFLEX:
	case BOARD_ID_IMX6_UTILITE:
		paddr = 0x021f0000;
		break;
	default:
		printf("board type %x unknown", board_id);
		return;
		/* XXX - HELP */
	}
	imxuartcnattach(&armv7_bs_tag, paddr, comcnspeed, comcnmode);
}

void
imx_platform_watchdog_reset(void)
{
	imxdog_reset();
}

void
imx_platform_powerdown(void)
{

}

const char *
imx_platform_board_name(void)
{
	return (imx_board_name());
}

void
imx_platform_disable_l2_if_needed(void)
{

}

void
imx_platform_board_init(void)
{
	imx_board_init();
}

struct armv7_platform imx_platform = {
	.boot_name = "OpenBSD/imx",
	.board_name = imx_platform_board_name,
	.board_init = imx_platform_board_init,
	.smc_write = imx_platform_smc_write,
	.init_cons = imx_platform_init_cons,
	.watchdog_reset = imx_platform_watchdog_reset,
	.powerdown = imx_platform_powerdown,
	.disable_l2_if_needed = imx_platform_disable_l2_if_needed,
};

struct armv7_platform *
imx_platform_match(void)
{
	struct board_dev *devs;

	devs = imx_board_devs();
	if (devs == NULL)
		return (NULL);

	imx_platform.devs = devs;
	return (&imx_platform);
}
