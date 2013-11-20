/* $OpenBSD: am335x.c,v 1.6 2013/11/20 13:32:40 rapha Exp $ */

/*
 * Copyright (c) 2011 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2013 Raphael Graf <r@undefined.ch>
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

#include <armv7/armv7/armv7var.h>

#define PRCM_SIZE	0x2000
#define PRCM_ADDR	0x44E00000

#define SCM_SIZE	0x2000
#define SCM_ADDR	0x44E10000

#define INTC_SIZE	0x300
#define INTC_ADDR	0x48200000

#define DMTIMERx_SIZE	0x80
#define DMTIMER0_ADDR	0x44E05000
#define DMTIMER1_ADDR	0x44E31000	/* 1MS */
#define DMTIMER2_ADDR	0x48040000
#define DMTIMER3_ADDR	0x48042000
#define DMTIMER4_ADDR	0x48044000
#define DMTIMER5_ADDR	0x48046000
#define DMTIMER6_ADDR	0x48048000
#define DMTIMER7_ADDR	0x4804A000
#define DMTIMER0_IRQ	66
#define DMTIMER1_IRQ	67
#define DMTIMER2_IRQ	68
#define DMTIMER3_IRQ	69
#define DMTIMER4_IRQ	92
#define DMTIMER5_IRQ	93
#define DMTIMER6_IRQ	94
#define DMTIMER7_IRQ	95

#define WD_SIZE		0x80
#define WD_ADDR		0x44E35000
#define WD_IRQ		91

#define GPIOx_SIZE	0x200
#define GPIO0_ADDR	0x44E07000
#define GPIO1_ADDR	0x4804C000
#define GPIO2_ADDR	0x481AC000
#define GPIO3_ADDR	0x481AE000
#define GPIO0_IRQ	96
#define GPIO1_IRQ	98
#define GPIO2_IRQ	32
#define GPIO3_IRQ	62

#define TPCC_SIZE	0x100000
#define TPCC_ADDR	0x49000000
#define TPTC0_ADDR	0x49800000
#define TPTC1_ADDR	0x49900000
#define TPTC2_ADDR	0x49a00000
#define EDMACOMP_IRQ	12
#define EDMAMPERR_IRQ	13
#define EDMAERR_IRQ	14

#define UARTx_SIZE	0x90
#define UART0_ADDR	0x44E09000
#define UART1_ADDR	0x48022000
#define UART2_ADDR	0x48024000
#define UART3_ADDR	0x481A6000
#define UART4_ADDR	0x481A8000
#define UART5_ADDR	0x481AA000
#define UART0_IRQ	72
#define UART1_IRQ	73
#define UART2_IRQ	74
#define UART3_IRQ	44
#define UART4_IRQ	45
#define UART5_IRQ	46

#define HSMMCx_SIZE	0x200
#define HSMMC0_ADDR	0x48060100
#define HSMMC1_ADDR	0x481d8100
#define HSMMC0_IRQ	64
#define HSMMC1_IRQ	28

#define CPSW_SIZE	0x4000
#define CPSW_ADDR	0x4A100000
#define CPSW_IRQ	40

struct armv7_dev am335x_devs[] = {

	/*
	 * Power, Reset and Clock Manager
	 */

	{ .name = "prcm",
	  .unit = 0,
	  .mem = { { PRCM_ADDR, PRCM_SIZE } },
	},

	/*
	 * System Control Module
	 */

	{ .name = "sitaracm",
	  .unit = 0,
	  .mem = { { SCM_ADDR, SCM_SIZE } },
	},

	/*
	 * Interrupt Controller
	 */

	{ .name = "intc",
	  .unit = 0,
	  .mem = { { INTC_ADDR, INTC_SIZE } },
	},

	/*
	 * EDMA Controller
	 */
	{ .name = "edma",
	  .unit = 0,
	  .mem = { { TPCC_ADDR, TPCC_SIZE } },
	  .irq = { EDMACOMP_IRQ }
	},

	/*
	 * General Purpose Timers
	 */

	{ .name = "dmtimer",
	  .unit = 0,
	  .mem = { { DMTIMER2_ADDR, DMTIMERx_SIZE } },
	  .irq = { DMTIMER2_IRQ }
	},

	{ .name = "dmtimer",
	  .unit = 1,
	  .mem = { { DMTIMER3_ADDR, DMTIMERx_SIZE } },
	  .irq = { DMTIMER3_IRQ }
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
	  .unit = 0,
	  .mem = { { UART0_ADDR, UARTx_SIZE } },
	  .irq = { UART0_IRQ }
	},

	/*
	 * GPIO 
	 */

	{ .name = "omgpio",
	  .unit = 0,
	  .mem = { { GPIO0_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO0_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 1,
	  .mem = { { GPIO1_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO1_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 2,
	  .mem = { { GPIO2_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO2_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 3,
	  .mem = { { GPIO3_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO3_IRQ }
	},

	/*
	 * MMC
	 */

	{ .name = "ommmc",
	  .unit = 0,
	  .mem = { { HSMMC0_ADDR, HSMMCx_SIZE } },
	  .irq = { HSMMC0_IRQ }
	},

	{ .name = "ommmc",
	  .unit = 1,
	  .mem = { { HSMMC1_ADDR, HSMMCx_SIZE } },
	  .irq = { HSMMC1_IRQ }
	},

	/* cpsw Ethernet */
	{ .name = "cpsw",
	  .unit = 0,
	  .mem = { { CPSW_ADDR, CPSW_SIZE } },
	  .irq = { CPSW_IRQ }
	},

	/* Terminator */
	{ .name = NULL,
	  .unit = 0
	}
};

void
am335x_init(void)
{
	armv7_set_devs(am335x_devs);
}
