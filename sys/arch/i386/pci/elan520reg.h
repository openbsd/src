/*	$OpenBSD: elan520reg.h,v 1.2 2004/06/05 15:06:22 grange Exp $	*/
/*	$NetBSD: elan520reg.h,v 1.1 2002/08/12 01:03:14 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Register definitions for the AMD Elan SC520 System Controller.
 */

#ifndef _I386_PCI_ELAN520REG_H_
#define	_I386_PCI_ELAN520REG_H_

#define	MMCR_BASE_ADDR		0xfffef000

/*
 * Am5x86 CPU Registers.
 */
#define	MMCR_REVID		0x0000
#define	MMCR_CPUCTL		0x0002

#define	REVID_PRODID		0xff00	/* product ID */
#define	REVID_PRODID_SHIFT	8
#define	REVID_MAJSTEP		0x00f0	/* stepping major */
#define	REVID_MAJSTEP_SHIFT	4
#define	REVID_MINSTEP		0x000f	/* stepping minor */

#define	PRODID_ELAN_SC520	0x00	/* Elan SC520 */

#define	CPUCTL_CPU_CLK_SPD_MASK	0x03	/* CPU clock speed */
#define	CPUCTL_CACHE_WR_MODE	0x10	/* cache mode (0 = wb, 1 = wt) */

/*
 * General Purpose Bus Registers
 */
#define	MMCR_GPECHO		0x0c00	/* GP echo mode */
#define	MMCR_GPCSDW		0x0c01	/* GP chip sel data width */
#define	MMCR_CPCSQUAL		0x0c02	/* GP chip sel qualification */
#define	MMCR_GPCSRT		0x0c08	/* GP chip sel recovery time */
#define	MMCR_GPCSPW		0x0c09	/* GP chip sel pulse width */
#define	MMCR_GPCSOFF		0x0c0a	/* GP chip sel offset */
#define	MMCR_GPRDW		0x0c0b	/* GP read pulse width */
#define	MMCR_GPRDOFF		0x0c0c	/* GP read offset */
#define	MMCR_GPWRW		0x0c0d	/* GP write pulse width */
#define	MMCR_GPWROFF		0x0c0e	/* GP write offset */
#define	MMCR_GPALEW		0x0c0f	/* GPALE pulse width */
#define	MMCR_GPALEOFF		0x0c10	/* GPALE offset */

#define	GPECHO_GP_ECHO_ENB	0x01	/* GP bus echo mode enable */

/*
 * Programmable Input/Output Registers
 */
#define	MMCR_PIOPFS15_0		0x0c20	/* PIO15-PIO0 pin func sel */
#define	MMCR_PIOPFS31_16	0x0c22	/* PIO31-PIO16 pin func sel */
#define	MMCR_CSPFS		0x0c24	/* chip sel pin func sel */
#define	MMCR_CLKSEL		0x0c26	/* clock select */
#define	MMCR_DSCTL		0x0c28	/* drive strength control */
#define	MMCR_PIODIR15_0		0x0c2a	/* PIO15-PIO0 direction */
#define	MMCR_PIODIR31_16	0x0c2c	/* PIO31-PIO16 direction */
#define	MMCR_PIODATA15_0	0x0c30	/* PIO15-PIO0 data */
#define	MMCR_PIODATA31_16	0x0c32	/* PIO31-PIO16 data */
#define	MMCR_PIOSET15_0		0x0c34	/* PIO15-PIO0 set */
#define	MMCR_PIOSET31_16	0x0c36	/* PIO31-PIO16 set */
#define	MMCR_PIOCLR15_0		0x0c38	/* PIO15-PIO0 clear */
#define	MMCR_PIOCLR31_16	0x0c3a	/* PIO31-PIO16 clear */

#define	ELANSC_PIO_NPINS	32	/* total number of PIO pins */

/*
 * Watchdog Timer Registers.
 */
#define	MMCR_WDTMRCTL		0x0cb0	/* watchdog timer control */
#define	MMCR_WDTMRCNTL		0x0cb2	/* watchdog timer count low */
#define	MMCR_WDTMRCNTH		0x0cb4	/* watchdog timer count high */

#define	WDTMRCTL_EXP_SEL_MASK	0x00ff	/* exponent select */
#define	WDTMRCTL_EXP_SEL14	0x0001	/*	496us/492us */
#define	WDTMRCTL_EXP_SEL24	0x0002	/*	508ms/503ms */
#define	WDTMRCTL_EXP_SEL25	0x0004	/*	1.02s/1.01s */
#define	WDTMRCTL_EXP_SEL26	0x0008	/*	2.03s/2.01s */
#define	WDTMRCTL_EXP_SEL27	0x0010	/*	4.07s/4.03s */
#define	WDTMRCTL_EXP_SEL28	0x0020	/*	8.13s/8.05s */
#define	WDTMRCTL_EXP_SEL29	0x0040	/*	16.27s/16.11s */
#define	WDTMRCTL_EXP_SEL30	0x0080	/*	32.54s/32.21s */
#define	WDTMRCTL_IRQ_FLG	0x1000	/* interrupt request */
#define	WDTMRCTL_WRST_ENB	0x4000	/* watchdog timer reset enable */
#define	WDTMRCTL_ENB		0x8000	/* watchdog timer enable */

#define	WDTMRCTL_UNLOCK1	0x3333
#define	WDTMRCTL_UNLOCK2	0xcccc

#define	WDTMRCTL_RESET1		0xaaaa
#define	WDTMRCTL_RESET2		0x5555

/*
 * Reset Generation Registers.
 */
#define	MMCR_SYSINFO		0x0d70	/* system board information */
#define	MMCR_RESCFG		0x0d72	/* reset configuration */
#define	MMCR_RESSTA		0x0d74	/* reset status */

#define	RESCFG_SYS_RST		0x01	/* software system reset */
#define	RESCFG_GP_RST		0x02	/* assert GP bus reset */
#define	RESCFG_PRG_RST_ENB	0x04	/* programmable reset enable */
#define	RESCFG_ICE_ON_RST	0x08	/* enter AMDebug(tm) on reset */

#define	RESSTA_PWRGOOD_DET	0x01	/* POWERGOOD reset detect */
#define	RESSTA_PRGRST_DET	0x02	/* programmable reset detect */
#define	RESSTA_SD_RST_DET	0x04	/* CPU shutdown reset detect */
#define	RESSTA_WDT_RST_DET	0x08	/* watchdog timer reset detect */
#define	RESSTA_ICE_SRST_DET	0x10	/* AMDebug(tm) soft reset detect */
#define	RESSTA_ICE_HRST_DET	0x20	/* AMDebug(tm) soft reset detect */
#define	RESSTA_SCP_RST		0x40	/* SCP reset detect */

#endif /* _I386_PCI_ELAN520REG_H_ */
