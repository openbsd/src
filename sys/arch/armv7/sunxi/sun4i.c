/*	$OpenBSD: sun4i.c,v 1.2 2013/10/23 18:01:52 jasper Exp $	*/
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

#include <sys/types.h>
#include <sys/param.h>

#include <machine/bus.h>

#include <armv7/sunxi/sunxivar.h>
#include <armv7/sunxi/sunxireg.h>

struct sxi_dev sxia1x_devs[] = {

	/* 'Port IO' */
	{ .name = "sxipio",
	  .unit = 0,
	  .mem = { { PIO_ADDR, PIOx_SIZE } },
	  .irq = { PIO_IRQ }
	},

	/* Clock Control Module/Unit */
	{ .name = "sxiccmu",
	  .unit = 0,
	  .mem = { { CCMU_ADDR, CCMU_SIZE } },
	},

	/* Interrupt Controller */
	{ .name = "a1xintc",
	  .unit = 0,
	  .mem = { { INTC_ADDR, INTC_SIZE } },
	},

	/* Timers/Counters, resources mapped on first unit */
	{ .name = "sxitimer",
	  .unit = 0,
	  .mem = {	{ TIMER_ADDR, TIMERx_SIZE },
			{ CPUCNTRS_ADDR, CPUCNTRS_ADDR } }
	},
	{ .name = "sxitimer",
	  .unit = 1,
	},
	{ .name = "sxitimer",
	  .unit = 2,
	},

	/* Watchdog Timer */
	{ .name = "sxidog",
	  .unit = 0,
	  .mem = { { WDOG_ADDR, WDOG_SIZE } }
	},

	/* Real Time Clock */
	{ .name = "sxirtc",
	  .unit = 0,
	  .mem = { { RTC_ADDR, RTC_SIZE } }
	},

	/* DMA Controller */
	{ .name = "sxidmac",
	  .unit = 0,
	  .mem = { { DMAC_ADDR, DMAC_SIZE } },
	  .irq = { DMAC_IRQ }
	},

	/* UART */
	{ .name = "sxiuart",
	  .unit = 0,
	  .mem = { { UART0_ADDR, UARTx_SIZE } },
	  .irq = { UART0_IRQ }
	},
	{ .name = "sxiuart",
	  .unit = 1,
	  .mem = { { UART1_ADDR, UARTx_SIZE } },
	  .irq = { UART1_IRQ }
	},
	{ .name = "sxiuart",
	  .unit = 2,
	  .mem = { { UART2_ADDR, UARTx_SIZE } },
	  .irq = { UART2_IRQ }
	},
	{ .name = "sxiuart",
	  .unit = 3,
	  .mem = { { UART3_ADDR, UARTx_SIZE } },
	  .irq = { UART3_IRQ }
	},
	{ .name = "sxiuart",
	  .unit = 4,
	  .mem = { { UART4_ADDR, UARTx_SIZE } },
	  .irq = { UART4_IRQ }
	},
	{ .name = "sxiuart",
	  .unit = 5,
	  .mem = { { UART5_ADDR, UARTx_SIZE } },
	  .irq = { UART5_IRQ }
	},
	{ .name = "sxiuart",
	  .unit = 6,
	  .mem = { { UART6_ADDR, UARTx_SIZE } },
	  .irq = { UART6_IRQ }
	},
	{ .name = "sxiuart",
	  .unit = 7,
	  .mem = { { UART7_ADDR, UARTx_SIZE } },
	  .irq = { UART7_IRQ }
	},

	/* EMAC */
	{ .name = "sxie",
	  .unit = 0,
	  .mem = {	{ EMAC_ADDR, EMAC_SIZE },
			{ SXIESRAM_ADDR, SXIESRAM_SIZE } },
	  .irq = { EMAC_IRQ}
	},

	/* SATA/AHCI */
	{ .name = "ahci",
	  .unit = 0,
	  .mem = { { SATA_ADDR, SATA_SIZE } },
	  .irq = { SATA_IRQ }
	},

	/* USB */
	{ .name = "ehci",
	  .unit = 0,
	  .mem = { { USB1_ADDR, USBx_SIZE } },
	  .irq = { USB1_IRQ }
	},
	{ .name = "ehci",
	  .unit = 1,
	  .mem = { { USB2_ADDR, USBx_SIZE } },
	  .irq = { USB2_IRQ }
	},
	{ .name = "ohci",
	  .unit = 0,
	  .mem = { { USB1_ADDR, USBx_SIZE } },
	  .irq = { USB0_IRQ }
	},
	{ .name = "ohci",
	  .unit = 1,
	  .mem = { { USB2_ADDR, USBx_SIZE } },
	  .irq = { USB1_IRQ }
	},

	/* Terminator */
	{ .name = NULL,
	  .unit = 0,
	}
};

void sxia1x_init(void);
void
sxia1x_init(void)
{
	sxi_set_devs(sxia1x_devs);
}
