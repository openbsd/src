/* $OpenBSD: omap4.c,v 1.3 2013/11/06 19:03:07 syl Exp $ */

/*
 * Copyright (c) 2011 Uwe Stuehler <uwe@openbsd.org>
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

#include <armv7/armv7/armv7var.h>

#define OMAPID_ADDR	0x4a002000
#define OMAPID_SIZE	0x1000

#define WD_ADDR		0x4a314000
#define WD_SIZE		0x80

#define GPIOx_SIZE	0x1000
#define GPIO1_ADDR	0x4a310000
#define GPIO2_ADDR	0x48055000
#define GPIO3_ADDR	0x48057000
#define GPIO4_ADDR	0x48059000
#define GPIO5_ADDR	0x4805b000
#define GPIO6_ADDR	0x4805d000

#define GPIO1_IRQ	29
#define GPIO2_IRQ	30
#define GPIO3_IRQ	31
#define GPIO4_IRQ	32
#define GPIO5_IRQ	33
#define GPIO6_IRQ	34

#define UARTx_SIZE	0x400
#define UART1_ADDR	0x4806A000
#define UART2_ADDR	0x4806C000
#define UART3_ADDR	0x48020000
#define UART4_ADDR	0x4806E000

#define UART1_IRQ	72
#define UART2_IRQ	73
#define UART3_IRQ	74
#define UART4_IRQ	70

#define HSMMCx_SIZE	0x200
#define HSMMC1_ADDR	0x4809c100
#define HSMMC1_IRQ	83

#define PRM_ADDR	0x4a306000
#define PRM_SIZE	0x2000
#define CM1_ADDR	0x4a004000
#define CM1_SIZE	0x1000
#define CM2_ADDR	0x4a008000
#define CM2_SIZE	0x2000
#define SCRM_ADDR	0x4a30a000
#define SCRM_SIZE	0x1000
#define PCNF1_ADDR	0x4a100000
#define PCNF1_SIZE	0x1000
#define PCNF2_ADDR	0x4a31e000
#define PCNF2_SIZE	0x1000

#define HSUSBHOST_ADDR	0x4a064000
#define HSUSBHOST_SIZE	0x800
#define USBEHCI_ADDR	0x4a064c00
#define USBEHCI_SIZE	0x400
#define USBOHCI_ADDR	0x4a064800
#define USBOHCI_SIZE	0x400
#define USBEHCI_IRQ	77

struct armv7_dev omap4_devs[] = {

	/*
	 * Power, Reset and Clock Manager
	 */

	{ .name = "prcm",
	  .unit = 0,
	  .mem = {
	    { PRM_ADDR, PRM_SIZE },
	    { CM1_ADDR, CM1_SIZE },
	    { CM2_ADDR, CM2_SIZE },
	  },
	},

	/*
	 * OMAP identification registers/fuses
	 */

	{ .name = "omapid",
	  .unit = 0,
	  .mem = { { OMAPID_ADDR, OMAPID_SIZE } },
	},

	/*
	 * GPIO
	 */

	{ .name = "omgpio",
	  .unit = 0,
	  .mem = { { GPIO1_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO1_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 1,
	  .mem = { { GPIO2_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO2_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 2,
	  .mem = { { GPIO3_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO3_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 3,
	  .mem = { { GPIO4_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO4_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 4,
	  .mem = { { GPIO5_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO5_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 5,
	  .mem = { { GPIO6_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO6_IRQ }
	},

	/*
	 * Watchdog Timer
	 */

	{ .name = "omdog",
	  .unit = 0,
	  .mem = { { WD_ADDR, WD_SIZE } }
	},

	/*
	 * UART
	 */

	{ .name = "com",
	  .unit = 2,
	  .mem = { { UART3_ADDR, UARTx_SIZE } },
	  .irq = { UART3_IRQ }
	},

	/*
	 * MMC
	 */

	{ .name = "ommmc",
	  .unit = 0,
	  .mem = { { HSMMC1_ADDR, HSMMCx_SIZE } },
	  .irq = { HSMMC1_IRQ }
	},

	/*
	 * USB
	 */

	{ .name = "ehci",
	  .unit = 0,
	  .mem = {
		  { USBEHCI_ADDR, USBEHCI_SIZE },
		  { HSUSBHOST_ADDR, HSUSBHOST_SIZE },
	  },
	  .irq = { USBEHCI_IRQ }
	},

	/* Terminator */
	{ .name = NULL,
	  .unit = 0,
	}
};

void
omap4_init(void)
{
	armv7_set_devs(omap4_devs);
}
