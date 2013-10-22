/* $OpenBSD: awa20.c,v 1.1 2013/10/22 13:22:19 jasper Exp $ */

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

#include <armv7/allwinner/allwinnervar.h>
#include <armv7/allwinner/allwinnerreg.h>

struct aw_dev awa20_devs[] = {

	/* 'Port IO' */
	{ .name = "awpio",
	  .unit = 0,
	  .mem = { { PIO_ADDR, PIOx_SIZE } },
	  .irq = { PIO_IRQ }
	},

	/* Clock Control Module/Unit */
	{ .name = "awccmu",
	  .unit = 0,
	  .mem = { { CCMU_ADDR, CCMU_SIZE } },
	},

	/* Timers/Counters, resources mapped on first unit */
	{ .name = "awtimer",
	  .unit = 0,
	  .mem = {	{ TIMER_ADDR, TIMERx_SIZE },
			{ CPUCNTRS_ADDR, CPUCNTRS_ADDR } }
	},
	{ .name = "awtimer",
	  .unit = 1,
	},
	{ .name = "awtimer",
	  .unit = 2,
	},

	/* Watchdog Timer */
	{ .name = "awdog",
	  .unit = 0,
	  .mem = { { WDOG_ADDR, WDOG_SIZE } }
	},

	/* Real Time Clock */
	{ .name = "awrtc",
	  .unit = 0,
	  .mem = { { RTC_ADDR, RTC_SIZE } }
	},

	/* DMA Controller */
	{ .name = "awdmac",
	  .unit = 0,
	  .mem = { { DMAC_ADDR, DMAC_SIZE } },
	  .irq = { DMAC_IRQ }
	},

	/* UART */
	{ .name = "awuart",
	  .unit = 0,
	  .mem = { { UART0_ADDR, UARTx_SIZE } },
	  .irq = { UART0_IRQ }
	},
	{ .name = "awuart",
	  .unit = 1,
	  .mem = { { UART1_ADDR, UARTx_SIZE } },
	  .irq = { UART1_IRQ }
	},
	{ .name = "awuart",
	  .unit = 2,
	  .mem = { { UART2_ADDR, UARTx_SIZE } },
	  .irq = { UART2_IRQ }
	},
	{ .name = "awuart",
	  .unit = 3,
	  .mem = { { UART3_ADDR, UARTx_SIZE } },
	  .irq = { UART3_IRQ }
	},
	{ .name = "awuart",
	  .unit = 4,
	  .mem = { { UART4_ADDR, UARTx_SIZE } },
	  .irq = { UART4_IRQ }
	},
	{ .name = "awuart",
	  .unit = 5,
	  .mem = { { UART5_ADDR, UARTx_SIZE } },
	  .irq = { UART5_IRQ }
	},
	{ .name = "awuart",
	  .unit = 6,
	  .mem = { { UART6_ADDR, UARTx_SIZE } },
	  .irq = { UART6_IRQ }
	},
	{ .name = "awuart",
	  .unit = 7,
	  .mem = { { UART7_ADDR, UARTx_SIZE } },
	  .irq = { UART7_IRQ }
	},

	/* EMAC */
	{ .name = "awe",
	  .unit = 0,
	  .mem = {	{ EMAC_ADDR, EMAC_SIZE },
			{ AWESRAM_ADDR, AWESRAM_SIZE } },
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

void awa20_init(void);
void
awa20_init(void)
{
	aw_set_devs(awa20_devs);
}
