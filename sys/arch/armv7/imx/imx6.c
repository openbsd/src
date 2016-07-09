/* $OpenBSD: imx6.c,v 1.5 2016/07/09 18:14:18 kettenis Exp $ */
/*
 * Copyright (c) 2011 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2012 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>
#include <machine/bus.h>
#include <arch/arm/armv7/armv7var.h>

#include <armv7/armv7/armv7var.h>

/* IRQs are defined without the 32 cpu IRQs */

#define CCM_ADDR	0x020c4000
#define CCM_SIZE	0x5000

#define CCM_IRQ1	87
#define CCM_IRQ2	88

#define IOMUXC_ADDR	0x020e0000
#define IOMUXC_SIZE	0x4000

#define OCOTP_ADDR	0x021bc000
#define OCOTP_SIZE	0x4000

#define GPIOx_SIZE	0x4000
#define GPIO1_ADDR	0x0209c000
#define GPIO2_ADDR	0x020a0000
#define GPIO3_ADDR	0x020a4000
#define GPIO4_ADDR	0x020a8000
#define GPIO5_ADDR	0x020ac000
#define GPIO6_ADDR	0x020b0000
#define GPIO7_ADDR	0x020b4000

#define GPIO1_IRQ7	58
#define GPIO1_IRQ6	59
#define GPIO1_IRQ5	60
#define GPIO1_IRQ4	61
#define GPIO1_IRQ3	62
#define GPIO1_IRQ2	63
#define GPIO1_IRQ1	64
#define GPIO1_IRQ0	65
#define GPIO1_IRQ16	66
#define GPIO1_IRQ32	67
#define GPIO2_IRQ16	68
#define GPIO2_IRQ32	69
#define GPIO3_IRQ16	70
#define GPIO3_IRQ32	71
#define GPIO4_IRQ16	72
#define GPIO4_IRQ32	73
#define GPIO5_IRQ16	74
#define GPIO5_IRQ32	75
#define GPIO6_IRQ16	76
#define GPIO6_IRQ32	77
#define GPIO7_IRQ16	78
#define GPIO7_IRQ32	79

struct armv7_dev imx6_devs[] = {

	/*
	 * Clock Control Module
	 */
	{ .name = "imxccm",
	  .unit = 0,
	  .mem = { { CCM_ADDR, CCM_SIZE } },
	},

	/*
	 * IOMUX Controller
	 */
	{ .name = "imxiomuxc",
	  .unit = 0,
	  .mem = { { IOMUXC_ADDR, IOMUXC_SIZE } },
	},

	/*
	 * On-Chip OTP Controller
	 */
	{ .name = "imxocotp",
	  .unit = 0,
	  .mem = { { OCOTP_ADDR, OCOTP_SIZE } },
	},

	/*
	 * GPIO
	 */
	{ .name = "imxgpio",
	  .unit = 0,
	  .mem = { { GPIO1_ADDR, GPIOx_SIZE } },
	},

	{ .name = "imxgpio",
	  .unit = 1,
	  .mem = { { GPIO2_ADDR, GPIOx_SIZE } },
	},

	{ .name = "imxgpio",
	  .unit = 2,
	  .mem = { { GPIO3_ADDR, GPIOx_SIZE } },
	},

	{ .name = "imxgpio",
	  .unit = 3,
	  .mem = { { GPIO4_ADDR, GPIOx_SIZE } },
	},

	{ .name = "imxgpio",
	  .unit = 4,
	  .mem = { { GPIO5_ADDR, GPIOx_SIZE } },
	},

	{ .name = "imxgpio",
	  .unit = 5,
	  .mem = { { GPIO6_ADDR, GPIOx_SIZE } },
	},

	{ .name = "imxgpio",
	  .unit = 6,
	  .mem = { { GPIO7_ADDR, GPIOx_SIZE } },
	},
};

void
imx6_init(void)
{
	armv7_set_devs(imx6_devs);
}
