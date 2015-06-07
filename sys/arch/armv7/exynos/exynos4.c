/*	$OpenBSD: exynos4.c,v 1.1 2015/06/07 16:54:16 jsg Exp $	*/
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

#define MCT_ADDR	0x10050000
#define MCT_SIZE	0x800

#define WD_ADDR		0x10060000
#define WD_SIZE		0x400
#define WD_IRQ		43

#define CLOCK_ADDR	0x10030000
#define CLOCK_SIZE	0x20000

#define POWER_ADDR	0x10020000
#define POWER_SIZE	0x4000

#define SYSREG_ADDR	0x10010000
#define SYSREG_SIZE	0x400

#define UARTx_SIZE	0x100
#define UART1_ADDR	0x13800000
#define UART2_ADDR	0x13810000
#define UART3_ADDR	0x13820000
#define UART4_ADDR	0x13830000
#define UART5_ADDR	0x13840000

#define UART1_IRQ	52
#define UART2_IRQ	53
#define UART3_IRQ	54
#define UART4_IRQ	55
#define UART5_IRQ	56

#define USB_EHCI_ADDR	0x12580000
#define USB_OHCI_ADDR	0x12590000
#define USB_PHY_ADDR	0x125b0000
#define USBx_SIZE	0x1000

#define USB_IRQ		70

#define GPIO1_ADDR	0x11400000
#define GPIO1_SIZE	0x280
#define GPIO2_ADDR	0x11400C00
#define GPIO2_SIZE	0x80
#define GPIO3_ADDR	0x13400000
#define GPIO3_SIZE	0x120
#define GPIO4_ADDR	0x10D10000
#define GPIO4_SIZE	0x80
#define GPIO5_ADDR	0x10D100C0
#define GPIO5_SIZE	0x20
#define GPIO6_ADDR	0x03860000
#define GPIO6_SIZE	0x20

#define I2Cx_SIZE	0x100
#define I2C1_ADDR	0x13860000
#define I2C2_ADDR	0x13870000
#define I2C3_ADDR	0x13880000
#define I2C4_ADDR	0x13890000
#define I2C5_ADDR	0x138a0000
#define I2C6_ADDR	0x138b0000
#define I2C7_ADDR	0x138c0000
#define I2C8_ADDR	0x138d0000

#define I2C1_IRQ	58
#define I2C2_IRQ	59
#define I2C3_IRQ	60
#define I2C4_IRQ	61
#define I2C5_IRQ	62
#define I2C6_IRQ	63
#define I2C7_IRQ	64
#define I2C8_IRQ	65

#define ESDHCx_SIZE	0x1000
#define ESDHC1_ADDR	0x12510000
#define ESDHC2_ADDR	0x12520000
#define ESDHC3_ADDR	0x12530000
#define ESDHC4_ADDR	0x12540000

#define ESDHC1_IRQ	73
#define ESDHC2_IRQ	74
#define ESDHC3_IRQ	75
#define ESDHC4_IRQ	76
#define SDMMC_IRQ	77

struct armv7_dev exynos4_devs[] = {

	/*
	 * Multi-Core Timer
	 */
	{ .name = "exmct",
	  .unit = 0,
	  .mem = {
	    { MCT_ADDR, MCT_SIZE },
	  },
	},

	/*
	 * Watchdog Timer
	 */
	{ .name = "exdog",
	  .unit = 0,
	  .mem = {
	    { WD_ADDR, WD_SIZE },
	  },
	},

	/*
	 * Clock
	 */
	{ .name = "exclock",
	  .unit = 0,
	  .mem = {
	    { CLOCK_ADDR, CLOCK_SIZE },
	  },
	},

	/*
	 * Power
	 */
	{ .name = "expower",
	  .unit = 0,
	  .mem = {
	    { POWER_ADDR, POWER_SIZE },
	  },
	},

	/*
	 * Sysreg
	 */
	{ .name = "exsysreg",
	  .unit = 0,
	  .mem = {
	    { SYSREG_ADDR, SYSREG_SIZE },
	  },
	},

	/*
	 * UART
	 */
	{ .name = "exuart",
	  .unit = 0,
	  .mem = { { UART1_ADDR, UARTx_SIZE } },
	  .irq = { UART1_IRQ }
	},
	{ .name = "exuart",
	  .unit = 1,
	  .mem = { { UART2_ADDR, UARTx_SIZE } },
	  .irq = { UART2_IRQ }
	},
	{ .name = "exuart",
	  .unit = 2,
	  .mem = { { UART3_ADDR, UARTx_SIZE } },
	  .irq = { UART3_IRQ }
	},
	{ .name = "exuart",
	  .unit = 3,
	  .mem = { { UART4_ADDR, UARTx_SIZE } },
	  .irq = { UART4_IRQ }
	},
	{ .name = "exuart",
	  .unit = 4,
	  .mem = { { UART5_ADDR, UARTx_SIZE } },
	  .irq = { UART5_IRQ }
	},

	/*
	 * GPIO
	 */
	{ .name = "exgpio",
	  .unit = 0,
	  .mem = { { GPIO1_ADDR, GPIO1_SIZE } },
	},

	{ .name = "exgpio",
	  .unit = 1,
	  .mem = { { GPIO2_ADDR, GPIO2_SIZE } },
	},

	{ .name = "exgpio",
	  .unit = 2,
	  .mem = { { GPIO3_ADDR, GPIO3_SIZE } },
	},

	{ .name = "exgpio",
	  .unit = 3,
	  .mem = { { GPIO4_ADDR, GPIO4_SIZE } },
	},

	{ .name = "exgpio",
	  .unit = 4,
	  .mem = { { GPIO5_ADDR, GPIO5_SIZE } },
	},

	{ .name = "exgpio",
	  .unit = 5,
	  .mem = { { GPIO6_ADDR, GPIO6_SIZE } },
	},

	/*
	 * I2C
	 */
	{ .name = "exiic",
	  .unit = 0,
	  .mem = { { I2C1_ADDR, I2Cx_SIZE } },
	  .irq = { I2C1_IRQ },
	},

	{ .name = "exiic",
	  .unit = 1,
	  .mem = { { I2C2_ADDR, I2Cx_SIZE } },
	  .irq = { I2C2_IRQ },
	},

	{ .name = "exiic",
	  .unit = 2,
	  .mem = { { I2C3_ADDR, I2Cx_SIZE } },
	  .irq = { I2C3_IRQ },
	},

	{ .name = "exiic",
	  .unit = 3,
	  .mem = { { I2C4_ADDR, I2Cx_SIZE } },
	  .irq = { I2C4_IRQ },
	},

	{ .name = "exiic",
	  .unit = 4,
	  .mem = { { I2C5_ADDR, I2Cx_SIZE } },
	  .irq = { I2C5_IRQ },
	},

	{ .name = "exiic",
	  .unit = 5,
	  .mem = { { I2C6_ADDR, I2Cx_SIZE } },
	  .irq = { I2C6_IRQ },
	},

	{ .name = "exiic",
	  .unit = 6,
	  .mem = { { I2C7_ADDR, I2Cx_SIZE } },
	  .irq = { I2C7_IRQ },
	},

	{ .name = "exiic",
	  .unit = 7,
	  .mem = { { I2C8_ADDR, I2Cx_SIZE } },
	  .irq = { I2C8_IRQ },
	},

	/*
	 * ESDHC
	 */
	{ .name = "exesdhc",
	  .unit = 0,
	  .mem = { { ESDHC1_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC1_IRQ },
	},

	{ .name = "exesdhc",
	  .unit = 1,
	  .mem = { { ESDHC2_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC2_IRQ },
	},

	{ .name = "exesdhc",
	  .unit = 2,
	  .mem = { { ESDHC3_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC3_IRQ },
	},

	{ .name = "exesdhc",
	  .unit = 3,
	  .mem = { { ESDHC4_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC4_IRQ },
	},

	/*
	 * USB
	 */
	{ .name = "exehci",
	  .unit = 0,
	  .mem = {
	    { USB_EHCI_ADDR, USBx_SIZE },
	    { USB_PHY_ADDR, USBx_SIZE },
	  },
	  .irq = { USB_IRQ }
	},

	/* Terminator */
	{ .name = NULL,
	  .unit = 0
	}
};

void
exynos4_init(void)
{
	armv7_set_devs(exynos4_devs);
}
