/*	$OpenBSD: sunxireg.h,v 1.1 2013/10/23 17:08:48 jasper Exp $	*/
/*
 * Copyright (c) 2013 Artturi Alm
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

#define	TIMER0_FREQUENCY	(32768)
#define	TIMER1_FREQUENCY	(32768)
#define	TIMER2_FREQUENCY	(32768)
#define	COUNTER_FREQUENCY	(24000000)

/* SRAM Controller / System Control */
#define	SYSCTRL_ADDR		0x01c00000
#define	SYSCTRL_SIZE		0x1000

#define	DMAC_ADDR		0x01c02000
#define	DMAC_SIZE		0x1000
#define	DMAC_IRQ		27

#define	SDMMC0_ADDR		0x01c0f000
#define	SDMMCx_SIZE		0x1000
#define	SDMMC0_IRQ		32

#define	SATA_ADDR		0x01c18000
#define	SATA_SIZE		0x1000
#define	SATA_IRQ		56

#define	TIMER_ADDR		0x01c20c00
#define	TIMERx_SIZE		0x200
#define	TIMER0_IRQ		22
#define	TIMER1_IRQ		23
#define	TIMER2_IRQ		24
#define	STATTIMER_IRQ		TIMER1_IRQ /* XXX */

#define	WDOG_ADDR		0x90
#define	WDOG_SIZE		0x08
#define	WDOG_IRQ		24

#define	RTC_ADDR		0x0104
#define	RTC_SIZE		0x08

/* Clock Control Module/Unit */
#define	CCMU_ADDR		0x01c20000
#define	CCMU_SIZE		0x400

#define	PIO_ADDR		0x01c20800
#define	PIOx_SIZE		0x400
#define	PIO_IRQ			28

#define	UARTx_SIZE		0x400
#define	UART0_ADDR		0x01c28000 
#define	UART1_ADDR		0x01c28400 
#define	UART2_ADDR		0x01c28800 
#define	UART3_ADDR		0x01c28c00 
#define	UART4_ADDR		0x01c29000 
#define	UART5_ADDR		0x01c29400 
#define	UART6_ADDR		0x01c29800 
#define	UART7_ADDR		0x01c29c00 
#define	UART0_IRQ		1
#define	UART1_IRQ		2
#define	UART2_IRQ		3
#define	UART3_IRQ		4
#define	UART4_IRQ		17
#define	UART5_IRQ		18
#define	UART6_IRQ		19
#define	UART7_IRQ		20

#define	USB0_ADDR		0x01c13000 /* usb otg */
#define	USB1_ADDR		0x01c14000 /* first port up from pcb */
#define	USB2_ADDR		0x01c1c000 /* 'top port' == above USB1 */
#define	USBx_SIZE		0x1000
#define	USB0_IRQ		38
#define	USB1_IRQ		39
#define	USB2_IRQ		40

/* Ethernet MAC Controller */
#define	EMAC_ADDR		0x01c0b000
#define	EMAC_SIZE		0x1000
#define	EMAC_IRQ		55
#define	AWESRAM_ADDR		0x00008000 /* combined area for EMAC fifos */
#define	AWESRAM_SIZE		0x4000

/* Security System */
#define	SS_ADDR			0x01c15000 /* not in use */
#define	SS_SIZE			0x1000
#define	SS_IRQ			54

/* A1x / Cortex-A8 */
#define	INTC_ADDR		0x01c20400
#define	INTC_SIZE		0x400

/* A20 / Cortex-A7 */
#define	GIC_ADDR		0x01c80000 /* = periphbase */
#define	GIC_SIZE		0x8000
#define	CPUCONFG_ADDR		0x01c25c00 /* not in use */
#define	CPUCONFG_SIZE		0x200
#define	CPUCNTRS_ADDR		0x01c25200 /* used by sxitimer */
#define	CPUCNTRS_SIZE		0x200
