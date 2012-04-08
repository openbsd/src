/*	$OpenBSD: iocreg.h,v 1.2 2012/04/08 22:08:25 miod Exp $	*/
/* $NetBSD: iocreg.h,v 1.2 2005/12/11 12:18:53 christos Exp $	 */

/*
 * Copyright (c) 2003 Christopher Sekiya
 * Copyright (c) 2001 Rafal K. Boni
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IOC1/2 memory map.
 *
 * The IOC1/2 is connected to the HPC#0, PBus channel 6, so these registers
 * are based from the external register window for PBus channel 6 on HPC#0.
 *
 */

#define	IOC_BASE			HPC3_PBUS_CH6_DEVREGS

#define IOC_PLP_REGS			0x00	/* Parallel port registers */
#define IOC_PLP_REGS_SIZE		0x2c

#define IOC_PLP_DATA			0x00	/* Data register */
#define IOC_PLP_CTL			0x04	/* Control register */
#define IOC_PLP_STAT			0x08	/* Status register */
#define IOC_PLP_DMACTL			0x0c	/* DMA control register */
#define IOC_PLP_INTSTAT			0x10	/* Interrupt status register */
#define IOC_PLP_INTMASK			0x14	/* Interrupt mask register */
#define IOC_PLP_TIMER1			0x18	/* Timer 1 register */
#define IOC_PLP_TIMER2			0x1c	/* Timer 2 register */
#define IOC_PLP_TIMER3			0x20	/* Timer 3 register */
#define IOC_PLP_TIMER4			0x24	/* Timer 4 register */

#define IOC_SERIAL_REGS			0x30	/* Serial port registers */
#define IOC_SERIAL_REGS_SIZE		0x0c

#define IOC_SERIAL_PORT1_CMD		0x00	/* Port 1 command transfer */
#define IOC_SERIAL_PORT1_DATA		0x04	/* Port 1 data transfer */
#define IOC_SERIAL_PORT2_CMD		0x08	/* Port 2 command transfer */
#define IOC_SERIAL_PORT2_DATA		0x0c	/* Port 2 data transfer */

#define IOC_KB_REGS			0x40	/* Keyboard/mouse registers */
#define IOC_KB_REGS_SIZE		0x08

/* Miscellaneous registers */

#define IOC_MISC_REGS			0x48	/* Misc. IOC regs */
#define IOC_MISC_REGS_SIZE		0x34

#define IOC_GCSEL			0x48	/* General select register */

#define IOC_GCREG			0x4c	/* General control register */
#define	IOC_GCREG_GIO_33MHZ		0x08

#define IOC_PANEL			0x50	/* Front Panel register */
#define IOC_PANEL_POWER_STATE		0x01
#define IOC_PANEL_POWER_IRQ		0x02
#define IOC_PANEL_VDOWN_IRQ		0x10
#define IOC_PANEL_VDOWN_HOLD		0x20
#define IOC_PANEL_VUP_IRQ		0x40
#define IOC_PANEL_VUP_HOLD		0x80

#define IOC_SYSID			0x58	/* System ID register */
#define IOC_SYSID_SYSTYPE		0x01	/* 0: Sapphire, 1: Full House */
#define IOC_SYSID_BOARDREV		0x1e
#define IOC_SYSID_BOARDREV_SHIFT	1
#define IOC_SYSID_CHIPREV		0xe0
#define IOC_SYSID_CHIPREV_SHIFT		5

#define IOC_READ			0x60	/* Read register */
#define IOC_READ_SCSI0_POWER		0x10
#define IOC_READ_SCSI1_POWER		0x20
#define IOC_READ_ENET_POWER		0x40
#define IOC_READ_ENET_LINK		0x80

#define IOC_DMASEL			0x68	/* DMA select register */
#define IOC_DMASEL_ISDN_B		0x01
#define IOC_DMASEL_ISDN_A		0x02
#define IOC_DMASEL_PARALLEL		0x04
#define IOC_DMASEL_SERIAL_10MHZ		0x00
#define IOC_DMASEL_SERIAL_6MHZ		0x10
#define IOC_DMASEL_SERIAL_EXTERNAL	0x20

#define IOC_RESET			0x70	/* Reset (IP24) / Write 1 (IP22)
						   register */
#define IOC_RESET_PARALLEL		0x01
#define IOC_RESET_PCKBC			0x02
#define IOC_RESET_EISA			0x04
#define IOC_RESET_ISDN			0x08
#define IOC_RESET_LED_GREEN		0x10
#define IOC_RESET_LED_RED		0x20
#define IOC_RESET_LED_ORANGE		0x40

#define IOC_WRITE			0x78	/* Write (IP24) / Write 2 (IP22)
						   register */
#define IOC_WRITE_ENET_NTH		0x01
#define IOC_WRITE_ENET_UTP		0x02
#define IOC_WRITE_ENET_AUI		0x04
#define IOC_WRITE_ENET_AUTO		0x08
#define IOC_WRITE_PC_UART2		0x10
#define IOC_WRITE_PC_UART1		0x20
#define IOC_WRITE_MARGIN_LOW		0x40
#define IOC_WRITE_MARGIN_HIGH		0x80
