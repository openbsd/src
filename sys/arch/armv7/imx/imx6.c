/* $OpenBSD: imx6.c,v 1.3 2015/05/30 08:09:19 jsg Exp $ */
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

#define ANALOG_ADDR	0x020c8000
#define ANALOG_SIZE	0x1000

#define IOMUXC_ADDR	0x020e0000
#define IOMUXC_SIZE	0x4000

#define WD1_ADDR	0x020bc000
#define WD1_SIZE	0x400
#define WD2_ADDR	0x020c0000
#define WD2_SIZE	0x400

#define OCOTP_ADDR	0x021bc000
#define OCOTP_SIZE	0x4000

#define UARTx_SIZE	0x4000
#define UART1_ADDR	0x02020000
#define UART2_ADDR	0x021e8000
#define UART3_ADDR	0x021ec000
#define UART4_ADDR	0x021f0000
#define UART5_ADDR	0x021f4000

#define UART1_IRQ	26
#define UART2_IRQ	27
#define UART3_IRQ	28
#define UART4_IRQ	29
#define UART5_IRQ	30

#define USBPHYx_SIZE		0x1000
#define USBPHY1_ADDR		0x020c9000
#define USBPHY2_ADDR		0x020ca000
#define USBOTG_ADDR		0x02184000
#define USBOTG_EHCI_ADDR	0x02184100
#define USBUH1_ADDR		0x02184200
#define USBUH1_EHCI_ADDR	0x02184300
#define USBUH2_ADDR		0x02184400
#define USBUH2_EHCI_ADDR	0x02184500
#define USBUH3_ADDR		0x02184600
#define USBUH3_EHCI_ADDR	0x02184700
#define USBNC_ADDR		0x02184800
#define USBx_SIZE		0x100

#define USBH1_IRQ	40
#define USBH2_IRQ	41
#define USBH3_IRQ	42
#define USBOTG_IRQ	43
#define USBPHY0_IRQ	44
#define USBPHY1_IRQ	45

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

#define I2Cx_SIZE	0x4000
#define I2C1_ADDR	0x021a0000
#define I2C2_ADDR	0x021a4000
#define I2C3_ADDR	0x021a8000

#define I2C1_IRQ	36
#define I2C2_IRQ	37
#define I2C3_IRQ	38

#define ESDHCx_SIZE	0x4000
#define ESDHC1_ADDR	0x02190000
#define ESDHC2_ADDR	0x02194000
#define ESDHC3_ADDR	0x02198000
#define ESDHC4_ADDR	0x0219c000

#define ESDHC1_IRQ	22
#define ESDHC2_IRQ	23
#define ESDHC3_IRQ	24
#define ESDHC4_IRQ	25

#define ENET_ADDR	0x02188000
#define ENET_SIZE	0x4000

#define ENET_IRQ0	118
#define ENET_IRQ1	119

#define SATA_ADDR	0x02200000
#define SATA_SIZE	0x4000

#define SATA_IRQ	39

#define PCIE_REG_ADDR	0x01ffc000
#define PCIE_REG_SIZE	0x4000
#define PCIE_MAP_ADDR	0x01000000
#define PCIE_MAP_SIZE	0xffc000

#define PCIE_IRQ0	120
#define PCIE_IRQ1	121
#define PCIE_IRQ2	122
#define PCIE_IRQ3	123

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
	 * Watchdog Timer
	 */
	{ .name = "imxdog",
	  .unit = 0,
	  .mem = {
	    { WD1_ADDR, WD1_SIZE },
	    { WD2_ADDR, WD2_SIZE },
	  },
	},

	/*
	 * On-Chip OTP Controller
	 */
	{ .name = "imxocotp",
	  .unit = 0,
	  .mem = { { OCOTP_ADDR, OCOTP_SIZE } },
	},

	/*
	 * UART
	 */
	{ .name = "imxuart",
	  .unit = 0,
	  .mem = { { UART1_ADDR, UARTx_SIZE } },
	  .irq = { UART1_IRQ }
	},
	{ .name = "imxuart",
	  .unit = 1,
	  .mem = { { UART2_ADDR, UARTx_SIZE } },
	  .irq = { UART2_IRQ }
	},
	{ .name = "imxuart",
	  .unit = 2,
	  .mem = { { UART3_ADDR, UARTx_SIZE } },
	  .irq = { UART3_IRQ }
	},
	{ .name = "imxuart",
	  .unit = 3,
	  .mem = { { UART4_ADDR, UARTx_SIZE } },
	  .irq = { UART4_IRQ }
	},
	{ .name = "imxuart",
	  .unit = 4,
	  .mem = { { UART5_ADDR, UARTx_SIZE } },
	  .irq = { UART5_IRQ }
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

	/*
	 * I2C
	 */
	{ .name = "imxiic",
	  .unit = 0,
	  .mem = { { I2C1_ADDR, I2Cx_SIZE } },
	  .irq = { I2C1_IRQ },
	},

	{ .name = "imxiic",
	  .unit = 1,
	  .mem = { { I2C2_ADDR, I2Cx_SIZE } },
	  .irq = { I2C2_IRQ },
	},

	{ .name = "imxiic",
	  .unit = 2,
	  .mem = { { I2C3_ADDR, I2Cx_SIZE } },
	  .irq = { I2C3_IRQ },
	},

	/*
	 * ESDHC
	 */
	{ .name = "imxesdhc",
	  .unit = 0,
	  .mem = { { ESDHC1_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC1_IRQ },
	},

	{ .name = "imxesdhc",
	  .unit = 1,
	  .mem = { { ESDHC2_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC2_IRQ },
	},

	{ .name = "imxesdhc",
	  .unit = 2,
	  .mem = { { ESDHC3_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC3_IRQ },
	},

	{ .name = "imxesdhc",
	  .unit = 3,
	  .mem = { { ESDHC4_ADDR, ESDHCx_SIZE } },
	  .irq = { ESDHC4_IRQ },
	},

	/*
	 * USB
	 */
	{ .name = "ehci",
	  .unit = 0,
	  .mem = {
		  { USBUH1_EHCI_ADDR, USBx_SIZE },
		  { USBUH1_ADDR, USBx_SIZE },
		  { USBPHY2_ADDR, USBPHYx_SIZE },
		  { USBNC_ADDR, USBx_SIZE },
	  },
	  .irq = { USBH1_IRQ }
	},

	{ .name = "ehci",
	  .unit = 1,
	  .mem = {
		  { USBOTG_EHCI_ADDR, USBx_SIZE },
		  { USBOTG_ADDR, USBx_SIZE },
		  { USBPHY1_ADDR, USBPHYx_SIZE },
		  { USBNC_ADDR, USBx_SIZE },
	  },
	  .irq = { USBOTG_IRQ }
	},

	/*
	 * Ethernet
	 */
	{ .name = "imxenet",
	  .unit = 0,
	  .mem = { { ENET_ADDR, ENET_SIZE } },
	  .irq = { ENET_IRQ0, ENET_IRQ1 }
	},

	/*
	 * AHCI compatible SATA controller
	 */
	{ .name = "ahci",
	  .unit = 0,
	  .mem = { { SATA_ADDR, SATA_SIZE } },
	  .irq = { SATA_IRQ }
	},
};

void
imx6_init(void)
{
	armv7_set_devs(imx6_devs);
}
