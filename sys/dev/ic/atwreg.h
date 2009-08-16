/*	$OpenBSD: atwreg.h,v 1.6 2009/08/16 17:24:04 jsg Exp $	*/
/*	$NetBSD: atwreg.h,v 1.10 2004/07/23 05:01:29 dyoung Exp $	*/

/*
 * Copyright (c) 2003 The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Young.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL David Young
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* glossary */

/* DTIM   Delivery Traffic Indication Map, sent by AP
 * ATIM   Ad Hoc Traffic Indication Map
 * TU     1024 microseconds
 * TSF    time synchronization function
 * TBTT   target beacon transmission time
 * DIFS   distributed inter-frame space
 * SIFS   short inter-frame space
 * EIFS   extended inter-frame space
 */

/* Macros for bit twiddling. */

#ifndef _BIT_TWIDDLE
#define _BIT_TWIDDLE
/* nth bit, BIT(0) == 0x1. */
#define BIT(n) (((n) == 32) ? 0 : ((u_int32_t) 1 << (n)))

/* bits m through n, m < n. */
#define BITS(m, n) ((BIT(MAX((m), (n)) + 1) - 1) ^ (BIT(MIN((m), (n))) - 1))

/* find least significant bit that is set */
#define LOWEST_SET_BIT(x) ((((x) - 1) & (x)) ^ (x))

/* for x a power of two and p a non-negative integer, is x a greater power than 2**p? */
#define GTEQ_POWER(x, p) (((u_long)(x) >> (p)) != 0)

#define MASK_TO_SHIFT2(m) (GTEQ_POWER(LOWEST_SET_BIT((m)), 1) ? 1 : 0)

#define MASK_TO_SHIFT4(m) \
	(GTEQ_POWER(LOWEST_SET_BIT((m)), 2) \
	    ? 2 + MASK_TO_SHIFT2((m) >> 2) \
	    : MASK_TO_SHIFT2((m)))

#define MASK_TO_SHIFT8(m) \
	(GTEQ_POWER(LOWEST_SET_BIT((m)), 4) \
	    ? 4 + MASK_TO_SHIFT4((m) >> 4) \
	    : MASK_TO_SHIFT4((m)))

#define MASK_TO_SHIFT16(m) \
	(GTEQ_POWER(LOWEST_SET_BIT((m)), 8) \
	    ? 8 + MASK_TO_SHIFT8((m) >> 8) \
	    : MASK_TO_SHIFT8((m)))

#define MASK_TO_SHIFT(m) \
	(GTEQ_POWER(LOWEST_SET_BIT((m)), 16) \
	    ? 16 + MASK_TO_SHIFT16((m) >> 16) \
	    : MASK_TO_SHIFT16((m)))

#define MASK_AND_RSHIFT(x, mask) (((x) & (mask)) >> MASK_TO_SHIFT(mask))
#define LSHIFT(x, mask) ((x) << MASK_TO_SHIFT(mask))
#define MASK_AND_REPLACE(reg, val, mask) ((reg & ~mask) | LSHIFT(val, mask))
#define PRESHIFT(m) MASK_AND_RSHIFT((m), (m))

#endif /* _BIT_TWIDDLE */

/* ADM8211 Host Control and Status Registers */

#define ATW_PAR		0x00	/* PCI access */
#define ATW_FRCTL	0x04	/* Frame control */
#define ATW_TDR		0x08	/* Transmit demand */
#define ATW_WTDP	0x0C	/* Current transmit descriptor pointer */
#define ATW_RDR		0x10	/* Receive demand */
#define ATW_WRDP	0x14	/* Current receive descriptor pointer */
#define ATW_RDB		0x18	/* Receive descriptor base address */
#define ATW_CSR3A	0x1C	/* Unused (on ADM8211A) */
#define ATW_C_TDBH	0x1C	/* Transmit descriptor base address,
				 * high-priority packet
				 */
#define ATW_TDBD	0x20	/* Transmit descriptor base address, DCF */
#define ATW_TDBP	0x24	/* Transmit descriptor base address, PCF */
#define ATW_STSR	0x28	/* Status */
#define ATW_CSR5A	0x2C	/* Unused */
#define ATW_C_TDBB	0x2C	/* Transmit descriptor base address, buffered
				 * broadcast/multicast packet
				 */
#define ATW_NAR		0x30	/* Network access */
#define ATW_CSR6A	0x34	/* Unused */
#define ATW_IER		0x38	/* Interrupt enable */
#define ATW_CSR7A	0x3C
#define ATW_LPC		0x40	/* Lost packet counter */
#define ATW_TEST1	0x44	/* Test register 1 */
#define ATW_SPR		0x48	/* Serial port */
#define ATW_TEST0	0x4C	/* Test register 0 */
#define ATW_WCSR	0x50	/* Wake-up control/status */
#define ATW_WPDR	0x54	/* Wake-up pattern data */
#define ATW_GPTMR	0x58	/* General purpose timer */
#define ATW_GPIO	0x5C	/* GPIO[5:0] configuration and control */
#define ATW_BBPCTL	0x60	/* BBP control port */
#define ATW_SYNCTL	0x64	/* synthesizer control port */
#define ATW_PLCPHD	0x68	/* PLCP header setting */
#define ATW_MMIWADDR	0x6C	/* MMI write address */
#define ATW_MMIRADDR1	0x70	/* MMI read address 1 */
#define ATW_MMIRADDR2	0x74	/* MMI read address 2 */
#define ATW_TXBR	0x78	/* Transmit burst counter */
#define ATW_CSR15A	0x7C	/* Unused */
#define ATW_ALCSTAT	0x80	/* ALC statistics */
#define ATW_TOFS2	0x84	/* Timing offset parameter 2, 16b */
#define ATW_CMDR	0x88	/* Command */
#define ATW_PCIC	0x8C	/* PCI bus performance counter */
#define ATW_PMCSR	0x90	/* Power management command and status */
#define ATW_PAR0	0x94	/* Local MAC address register 0, 32b */
#define ATW_PAR1	0x98	/* Local MAC address register 1, 16b */
#define ATW_MAR0	0x9C	/* Multicast address hash table register 0 */
#define ATW_MAR1	0xA0	/* Multicast address hash table register 1 */
#define ATW_ATIMDA0	0xA4	/* Ad Hoc Traffic Indication Map (ATIM)
				 * frame DA, byte[3:0]
				 */
#define ATW_ABDA1	0xA8	/* BSSID address byte[5:4];
				 * ATIM frame DA byte[5:4]
				 */
#define ATW_BSSID0	0xAC	/* BSSID  address byte[3:0] */
#define ATW_TXLMT	0xB0	/* WLAN retry limit, 8b;
				 * Max TX MSDU lifetime, 16b
				 */
#define ATW_MIBCNT	0xB4	/* RTS/ACK/FCS MIB count, 32b */
#define ATW_BCNT	0xB8	/* Beacon transmission time, 32b */
#define ATW_TSFTH	0xBC	/* TSFT[63:32], 32b */
#define ATW_TSC		0xC0	/* TSFT[39:32] down count value */
#define ATW_SYNRF	0xC4	/* SYN RF IF direct control */
#define ATW_BPLI	0xC8	/* Beacon interval, 16b.
				 * STA listen interval, 16b.
				 */
#define ATW_CAP0	0xCC	/* Current channel, 4b. RCVDTIM, 1b. */
#define ATW_CAP1	0xD0	/* Capability information, 16b.
				 * ATIM window, 1b.
				 */
#define ATW_RMD		0xD4	/* RX max reception duration, 16b */
#define ATW_CFPP	0xD8	/* CFP parameter, 32b */
#define ATW_TOFS0	0xDC	/* Timing offset parameter 0, 28b */
#define ATW_TOFS1	0xE0	/* Timing offset parameter 1, 24b */
#define ATW_IFST	0xE4	/* IFS timing parameter 1, 32b */
#define ATW_RSPT	0xE8	/* Response time, 24b */
#define ATW_TSFTL	0xEC	/* TSFT[31:0], 32b */
#define ATW_WEPCTL	0xF0	/* WEP control */
#define ATW_WESK	0xF4	/* Write entry for shared/individual key */
#define ATW_WEPCNT	0xF8	/* WEP count */
#define ATW_MACTEST	0xFC

#define ATW_FER		0x100	/* Function event */
#define ATW_FEMR	0x104	/* Function event mask */
#define ATW_FPSR	0x108	/* Function present state */
#define ATW_FFER	0x10C	/* Function force event */


#define ATW_PAR_MWIE		BIT(24)		/* memory write and invalidate
						 * enable
						 */
#define ATW_PAR_MRLE		BIT(23)		/* memory read line enable */
#define ATW_PAR_MRME		BIT(21)		/* memory read multiple
						 * enable
						 */
#define ATW_PAR_RAP_MASK	0x60000	/* receive auto-polling in
						 * receive suspended state
						 */
#define ATW_PAR_CAL_MASK	0xc000	/* cache alignment */
#define		ATW_PAR_CAL_PBL		0x0
						/* min(8 DW, PBL) */
#define		ATW_PAR_CAL_8DW		LSHIFT(0x1, ATW_PAR_CAL_MASK)
						/* min(16 DW, PBL) */
#define		ATW_PAR_CAL_16DW	LSHIFT(0x2, ATW_PAR_CAL_MASK)
						/* min(32 DW, PBL) */
#define		ATW_PAR_CAL_32DW	LSHIFT(0x3, ATW_PAR_CAL_MASK)
#define ATW_PAR_PBL_MASK	0x3f00	/* programmable burst length */
#define		ATW_PAR_PBL_UNLIMITED	0x0
#define		ATW_PAR_PBL_1DW		LSHIFT(0x1, ATW_PAR_PBL_MASK)
#define		ATW_PAR_PBL_2DW		LSHIFT(0x2, ATW_PAR_PBL_MASK)
#define		ATW_PAR_PBL_4DW		LSHIFT(0x4, ATW_PAR_PBL_MASK)
#define		ATW_PAR_PBL_8DW		LSHIFT(0x8, ATW_PAR_PBL_MASK)
#define		ATW_PAR_PBL_16DW	LSHIFT(0x16, ATW_PAR_PBL_MASK)
#define		ATW_PAR_PBL_32DW	LSHIFT(0x32, ATW_PAR_PBL_MASK)
#define ATW_PAR_BLE		BIT(7)		/* big/little endian selection */
#define ATW_PAR_DSL_MASK	0x7c	/* descriptor skip length */
#define ATW_PAR_BAR		BIT(1)		/* bus arbitration */
#define ATW_PAR_SWR		BIT(0)		/* software reset */

#define ATW_FRCTL_PWRMGMT	BIT(31)		/* power management */
#define ATW_FRCTL_VER_MASK	0x60000000	/* protocol version */
#define ATW_FRCTL_ORDER		BIT(28)		/* order bit */
#define ATW_FRCTL_MAXPSP	BIT(27)		/* maximum power saving */
#define ATW_C_FRCTL_PRSP	BIT(26)		/* 1: driver sends probe
						 *    response
						 * 0: ASIC sends prresp
						 */
#define ATW_C_FRCTL_DRVBCON	BIT(25)		/* 1: driver sends beacons
						 * 0: ASIC sends beacons
						 */
#define ATW_C_FRCTL_DRVLINKCTRL	BIT(24)		/* 1: driver controls link LED
						 * 0: ASIC controls link LED
						 */
#define ATW_C_FRCTL_DRVLINKON	BIT(23)		/* 1: turn on link LED
						 * 0: turn off link LED
						 */
#define ATW_C_FRCTL_CTX_DATA	BIT(22)		/* 0: set by CSR28
						 * 1: random
						 */
#define ATW_C_FRCTL_RSVFRM	BIT(21)		/* 1: receive "reserved" 
						 * frames, 0: ignore
						 * reserved frames
						 */
#define ATW_C_FRCTL_CFEND	BIT(19)		/* write to send CF_END,
						 * ADM8211C/CR clears
						 */
#define ATW_FRCTL_DOZEFRM	BIT(18)		/* select pre-sleep frame */
#define ATW_FRCTL_PSAWAKE	BIT(17)		/* MAC is awake (?) */
#define ATW_FRCTL_PSMODE	BIT(16)		/* MAC is power-saving (?) */
#define ATW_FRCTL_AID_MASK	0xffff	/* STA Association ID */

#define ATW_INTR_PCF		BIT(31)		/* started/ended CFP */
#define ATW_INTR_BCNTC		BIT(30)		/* transmitted IBSS beacon */
#define ATW_INTR_GPINT		BIT(29)		/* GPIO interrupt */
#define ATW_INTR_LINKOFF	BIT(28)		/* lost ATW_WCSR_BLN beacons */
#define ATW_INTR_ATIMTC		BIT(27)		/* transmitted ATIM */
#define ATW_INTR_TSFTF		BIT(26)		/* TSFT out of range */
#define ATW_INTR_TSCZ		BIT(25)		/* TSC countdown expired */
#define ATW_INTR_LINKON		BIT(24)		/* matched SSID, BSSID */
#define ATW_INTR_SQL		BIT(23)		/* Marvel signal quality */
#define ATW_INTR_WEPTD		BIT(22)		/* switched WEP table */
#define ATW_INTR_ATIME		BIT(21)		/* ended ATIM window */
#define ATW_INTR_TBTT		BIT(20)		/* (TBTT) Target Beacon TX Time
						 * passed
						 */
#define ATW_INTR_NISS		BIT(16)		/* normal interrupt status
						 * summary: any of 31, 30, 27,
						 * 24, 14, 12, 6, 2, 0.
						 */
#define ATW_INTR_AISS		BIT(15)		/* abnormal interrupt status
						 * summary: any of 29, 28, 26,
						 * 25, 23, 22, 13, 11, 8, 7, 5,
						 * 4, 3, 1.
						 */
#define ATW_INTR_TEIS		BIT(14)		/* transmit early interrupt
						 * status: moved TX packet to
						 * FIFO
						 */
#define ATW_INTR_FBE		BIT(13)		/* fatal bus error */
#define ATW_INTR_REIS		BIT(12)		/* receive early interrupt
						 * status: RX packet filled
						 * its first descriptor
						 */
#define ATW_INTR_GPTT		BIT(11)		/* general purpose timer expired */
#define ATW_INTR_RPS		BIT(8)		/* stopped receive process */
#define ATW_INTR_RDU		BIT(7)		/* receive descriptor
						 * unavailable
						 */
#define ATW_INTR_RCI		BIT(6)		/* completed packet reception */
#define ATW_INTR_TUF		BIT(5)		/* transmit underflow */
#define ATW_INTR_TRT		BIT(4)		/* transmit retry count
						 * expired
						 */
#define ATW_INTR_TLT		BIT(3)		/* transmit lifetime exceeded */
#define ATW_INTR_TDU		BIT(2)		/* transmit descriptor
						 * unavailable
						 */
#define ATW_INTR_TPS		BIT(1)		/* stopped transmit process */
#define ATW_INTR_TCI		BIT(0)		/* completed transmit */
#define ATW_NAR_TXCF		BIT(31)		/* stop process on TX failure */
#define ATW_NAR_HF		BIT(30)		/* flush TX FIFO to host (?) */
#define ATW_NAR_UTR		BIT(29)		/* select retry count source */
#define ATW_NAR_PCF		BIT(28)		/* use one/both transmit
						 * descriptor base addresses
						 */
#define ATW_NAR_CFP		BIT(27)		/* indicate more TX data to
						 * point coordinator
						 */
#define ATW_C_NAR_APSTA		BIT(26)		/* 0: STA mode
						 * 1: AP mode
						 */
#define ATW_C_NAR_TDBBE		BIT(25)		/* 0: disable TDBB
						 * 1: enable TDBB
						 */
#define ATW_C_NAR_TDBHE		BIT(24)		/* 0: disable TDBH
						 * 1: enable TDBH
						 */
#define ATW_C_NAR_TDBHT		BIT(23)		/* write 1 to make ASIC
						 * poll TDBH once; ASIC clears
						 */
#define ATW_NAR_SF		BIT(21)		/* store and forward: ignore
						 * TX threshold
						 */
#define ATW_NAR_TR_MASK		0xc000	/* TX threshold */
#define		ATW_NAR_TR_L64		LSHIFT(0x0, ATW_NAR_TR_MASK)
#define		ATW_NAR_TR_L160		LSHIFT(0x2, ATW_NAR_TR_MASK)
#define		ATW_NAR_TR_L192		LSHIFT(0x3, ATW_NAR_TR_MASK)
#define		ATW_NAR_TR_H96		LSHIFT(0x0, ATW_NAR_TR_MASK)
#define		ATW_NAR_TR_H288		LSHIFT(0x2, ATW_NAR_TR_MASK)
#define		ATW_NAR_TR_H544		LSHIFT(0x3, ATW_NAR_TR_MASK)
#define ATW_NAR_ST		BIT(13)		/* start/stop transmit */
#define ATW_NAR_OM_MASK		0xc00	/* operating mode */
#define		ATW_NAR_OM_NORMAL	0x0
#define		ATW_NAR_OM_LOOPBACK	LSHIFT(0x1, ATW_NAR_OM_MASK)
#define ATW_NAR_MM		BIT(7)		/* RX any multicast */
#define ATW_NAR_PR		BIT(6)		/* promiscuous mode */
#define ATW_NAR_EA		BIT(5)		/* match ad hoc packets (?) */
#define ATW_NAR_DISPCF		BIT(4)		/* 1: PCF *not* supported
						 * 0: PCF supported
						 */
#define ATW_NAR_PB		BIT(3)		/* pass bad packets */
#define ATW_NAR_STPDMA		BIT(2)		/* stop DMA, abort packet */
#define ATW_NAR_SR		BIT(1)		/* start/stop receive */
#define ATW_NAR_CTX		BIT(0)		/* continuous TX mode */

/* IER bits are identical to STSR bits. Use ATW_INTR_*. */
#if 0
#define ATW_IER_NIE		BIT(16)		/* normal interrupt enable */
#define ATW_IER_AIE		BIT(15)		/* abnormal interrupt enable */
/* normal interrupts: combine with ATW_IER_NIE */
#define ATW_IER_PCFIE		BIT(31)		/* STA entered CFP */
#define ATW_IER_BCNTCIE		BIT(30)		/* STA TX'd beacon */
#define ATW_IER_ATIMTCIE	BIT(27)		/* transmitted ATIM */
#define ATW_IER_LINKONIE	BIT(24)		/* matched beacon */
#define ATW_IER_ATIMIE		BIT(21)		/* ended ATIM window */
#define ATW_IER_TBTTIE		BIT(20)		/* TBTT */
#define ATW_IER_TEIE		BIT(14)		/* moved TX packet to FIFO */
#define ATW_IER_REIE		BIT(12)		/* RX packet filled its first
						 * descriptor
						 */
#define ATW_IER_RCIE		BIT(6)		/* completed RX */ 
#define ATW_IER_TDUIE		BIT(2)		/* transmit descriptor
						 * unavailable
						 */
#define ATW_IER_TCIE		BIT(0)		/* completed TX */
/* abnormal interrupts: combine with ATW_IER_AIE */
#define ATW_IER_GPIE		BIT(29)		/* GPIO interrupt */
#define ATW_IER_LINKOFFIE	BIT(28)		/* lost beacon */
#define ATW_IER_TSFTFIE		BIT(26)		/* TSFT out of range */
#define ATW_IER_TSCIE		BIT(25)		/* TSC countdown expired */
#define ATW_IER_SQLIE		BIT(23)		/* signal quality */
#define ATW_IER_WEPIE		BIT(22)		/* finished WEP table switch */
#define ATW_IER_FBEIE		BIT(13)		/* fatal bus error */
#define ATW_IER_GPTIE		BIT(11)		/* general purpose timer expired */
#define ATW_IER_RPSIE		BIT(8)		/* stopped receive process */
#define ATW_IER_RUIE		BIT(7)		/* receive descriptor unavailable */
#define ATW_IER_TUIE		BIT(5)		/* transmit underflow */
#define ATW_IER_TRTIE		BIT(4)		/* exceeded transmit retry count */
#define ATW_IER_TLTTIE		BIT(3)		/* transmit lifetime exceeded */
#define ATW_IER_TPSIE		BIT(1)		/* stopped transmit process */
#endif

#define ATW_LPC_LPCO		BIT(16)		/* lost packet counter overflow */
#define ATW_LPC_LPC_MASK	0xffff	/* lost packet counter */

#define	ATW_TEST1_CONTROL	BIT(31)		/* "0: read from dxfer_control,
						 * 1: read from dxfer_state"
						 */
#define	ATW_TEST1_DBGREAD_MASK	0x70000000	/* "control of read data,
						 * debug only"
						 */
#define	ATW_TEST1_TXWP_MASK	0xe000000	/* select ATW_WTDP content? */
#define	ATW_TEST1_TXWP_TDBD	LSHIFT(0x0, ATW_TEST1_TXWP_MASK)
#define	ATW_TEST1_TXWP_TDBH	LSHIFT(0x1, ATW_TEST1_TXWP_MASK)
#define	ATW_TEST1_TXWP_TDBB	LSHIFT(0x2, ATW_TEST1_TXWP_MASK)
#define	ATW_TEST1_TXWP_TDBP	LSHIFT(0x3, ATW_TEST1_TXWP_MASK)
#define	ATW_TEST1_RSVD0_MASK	0x1ffffc0	/* reserved */
#define	ATW_TEST1_TESTMODE_MASK	0x30
/* normal operation */
#define	ATW_TEST1_TESTMODE_NORMAL	LSHIFT(0x0, ATW_TEST1_TESTMODE_MASK)
/* MAC-only mode */
#define	ATW_TEST1_TESTMODE_MACONLY	LSHIFT(0x1, ATW_TEST1_TESTMODE_MASK)
/* normal operation */
#define	ATW_TEST1_TESTMODE_NORMAL2	LSHIFT(0x2, ATW_TEST1_TESTMODE_MASK)
/* monitor mode */
#define	ATW_TEST1_TESTMODE_MONITOR	LSHIFT(0x3, ATW_TEST1_TESTMODE_MASK)

#define	ATW_TEST1_DUMP_MASK	0xf		/* select dump signal
							 * from dxfer (huh?)
							 */

#define ATW_SPR_SRS		BIT(11)		/* activate SEEPROM access */
#define ATW_SPR_SDO		BIT(3)		/* data out of SEEPROM */
#define ATW_SPR_SDI		BIT(2)		/* data into SEEPROM */
#define ATW_SPR_SCLK		BIT(1)		/* SEEPROM clock */
#define ATW_SPR_SCS		BIT(0)		/* SEEPROM chip select */

#define ATW_TEST0_BE_MASK	0xe0000000	/* Bus error state */
#define ATW_TEST0_TS_MASK	0x1c000000	/* Transmit process state */

/* Stopped */
#define ATW_TEST0_TS_STOPPED		LSHIFT(0, ATW_TEST0_TS_MASK)
/* Running - fetch transmit descriptor */
#define ATW_TEST0_TS_FETCH		LSHIFT(1, ATW_TEST0_TS_MASK)
/* Running - wait for end of transmission */
#define ATW_TEST0_TS_WAIT		LSHIFT(2, ATW_TEST0_TS_MASK)
/* Running - read buffer from memory and queue into FIFO */
#define ATW_TEST0_TS_READING		LSHIFT(3, ATW_TEST0_TS_MASK)
#define ATW_TEST0_TS_RESERVED1		LSHIFT(4, ATW_TEST0_TS_MASK)
#define ATW_TEST0_TS_RESERVED2		LSHIFT(5, ATW_TEST0_TS_MASK)
/* Suspended */
#define ATW_TEST0_TS_SUSPENDED		LSHIFT(6, ATW_TEST0_TS_MASK)
/* Running - close transmit descriptor */
#define ATW_TEST0_TS_CLOSE		LSHIFT(7, ATW_TEST0_TS_MASK)

/* ADM8211C/CR registers */ 
/* Suspended */
#define ATW_C_TEST0_TS_SUSPENDED	LSHIFT(4, ATW_TEST0_TS_MASK)
/* Descriptor write */
#define ATW_C_TEST0_TS_CLOSE		LSHIFT(5, ATW_TEST0_TS_MASK)
/* Last descriptor write */
#define ATW_C_TEST0_TS_CLOSELAST	LSHIFT(6, ATW_TEST0_TS_MASK)
/* FIFO full */
#define ATW_C_TEST0_TS_FIFOFULL		LSHIFT(7, ATW_TEST0_TS_MASK)

#define ATW_TEST0_RS_MASK	0x3800000	/* Receive process state */

/* Stopped */
#define	ATW_TEST0_RS_STOPPED		LSHIFT(0, ATW_TEST0_RS_MASK)
/* Running - fetch receive descriptor */
#define	ATW_TEST0_RS_FETCH		LSHIFT(1, ATW_TEST0_RS_MASK)
/* Running - check for end of receive */
#define	ATW_TEST0_RS_CHECK		LSHIFT(2, ATW_TEST0_RS_MASK)
/* Running - wait for packet */
#define	ATW_TEST0_RS_WAIT		LSHIFT(3, ATW_TEST0_RS_MASK)
/* Suspended */
#define	ATW_TEST0_RS_SUSPENDED		LSHIFT(4, ATW_TEST0_RS_MASK)
/* Running - close receive descriptor */
#define	ATW_TEST0_RS_CLOSE		LSHIFT(5, ATW_TEST0_RS_MASK)
/* Running - flush current frame from FIFO */
#define	ATW_TEST0_RS_FLUSH		LSHIFT(6, ATW_TEST0_RS_MASK)
/* Running - queue current frame from FIFO into buffer */
#define	ATW_TEST0_RS_QUEUE		LSHIFT(7, ATW_TEST0_RS_MASK)

#define ATW_TEST0_EPNE		BIT(18)		/* SEEPROM not detected */
#define ATW_TEST0_EPSNM		BIT(17)		/* SEEPROM bad signature */
#define ATW_TEST0_EPTYP_MASK	BIT(16)		/* SEEPROM type
						 * 1: 93c66,
						 * 0: 93c46
						 */
#define	ATW_TEST0_EPTYP_93c66		ATW_TEST0_EPTYP_MASK
#define	ATW_TEST0_EPTYP_93c46		0
#define ATW_TEST0_EPRLD		BIT(15)		/* recall SEEPROM (write 1) */

#define ATW_WCSR_CRCT		BIT(30)		/* CRC-16 type */
#define ATW_WCSR_WP1E		BIT(29)		/* match wake-up pattern 1 */
#define ATW_WCSR_WP2E		BIT(28)		/* match wake-up pattern 2 */
#define ATW_WCSR_WP3E		BIT(27)		/* match wake-up pattern 3 */
#define ATW_WCSR_WP4E		BIT(26)		/* match wake-up pattern 4 */
#define ATW_WCSR_WP5E		BIT(25)		/* match wake-up pattern 5 */
#define ATW_WCSR_BLN_MASK	0xe00000	/* lose link after BLN lost
						 * beacons
						 */
#define ATW_WCSR_TSFTWE		BIT(20)		/* wake up on TSFT out of
						 * range
						 */
#define ATW_WCSR_TIMWE		BIT(19)		/* wake up on TIM */
#define ATW_WCSR_ATIMWE		BIT(18)		/* wake up on ATIM */
#define ATW_WCSR_KEYWE		BIT(17)		/* wake up on key update */
#define ATW_WCSR_WFRE		BIT(10)		/* wake up on wake-up frame */
#define ATW_WCSR_MPRE		BIT(9)		/* wake up on magic packet */
#define ATW_WCSR_LSOE		BIT(8)		/* wake up on link loss */
/* wake-up reasons correspond to enable bits */
#define ATW_WCSR_KEYUP		BIT(6)		/* */
#define ATW_WCSR_TSFTW		BIT(5)		/* */
#define ATW_WCSR_TIMW		BIT(4)		/* */
#define ATW_WCSR_ATIMW		BIT(3)		/* */
#define ATW_WCSR_WFR		BIT(2)		/* */
#define ATW_WCSR_MPR		BIT(1)		/* */
#define ATW_WCSR_LSO		BIT(0)		/* */

#define ATW_GPTMR_COM_MASK	BIT(16)		/* continuous operation mode */
#define ATW_GPTMR_GTV_MASK	0xffff	/* set countdown in 204us ticks */

#define ATW_GPIO_EC1_MASK	0x3000000	/* GPIO1 event configuration */
#define ATW_GPIO_LAT_MASK	0x300000	/* input latch */
#define ATW_GPIO_INTEN_MASK	0xc0000	/* interrupt enable */
#define ATW_GPIO_EN_MASK	0x3f000	/* output enable */
#define ATW_GPIO_O_MASK		0xfc0	/* output value */
#define ATW_GPIO_I_MASK		0x3f	/* pin static input */

#define ATW_BBPCTL_TWI			BIT(31)	/* Intersil 3-wire interface */
#define ATW_BBPCTL_RF3KADDR_MASK	0x7f000000	/* Address for RF3000 */
#define ATW_BBPCTL_RF3KADDR_ADDR LSHIFT(0x20, ATW_BBPCTL_RF3KADDR_MASK)
#define ATW_BBPCTL_NEGEDGE_DO		BIT(23)	/* data-out on negative edge */
#define ATW_BBPCTL_NEGEDGE_DI		BIT(22)	/* data-in on negative edge */
#define ATW_BBPCTL_CCA_ACTLO		BIT(21)	/* CCA low when busy */
#define ATW_BBPCTL_TYPE_MASK		0x1c0000	/* BBP type */
#define ATW_BBPCTL_WR			BIT(17)	/* start write; reset on
						 * completion
						 */
#define ATW_BBPCTL_RD		BIT(16)		/* start read; reset on
						 * completion
						 */
#define ATW_BBPCTL_ADDR_MASK	0xff00	/* BBP address */
#define ATW_BBPCTL_DATA_MASK	0xff	/* BBP data */

#define ATW_SYNCTL_WR		BIT(31)		/* start write; reset on
						 * completion
						 */
#define ATW_SYNCTL_RD		BIT(30)		/* start read; reset on
						 * completion
						 */
#define ATW_SYNCTL_CS0		BIT(29)		/* chip select */
#define ATW_SYNCTL_CS1		BIT(28)
#define ATW_SYNCTL_CAL		BIT(27)		/* generate RF CAL pulse after
						 * Rx
						 */
#define ATW_SYNCTL_SELCAL	BIT(26)		/* RF CAL source, 0: CAL bit,
						 * 1: MAC; needed by Intersil
						 * BBP
						 */
#define	ATW_C_SYNCTL_MMICE	BIT(25)		/* ADM8211C/CR define this
						 * bit. 0: latch data on
						 * negative edge, 1: positive
						 * edge.
						 */
#define ATW_SYNCTL_RFTYPE_MASK	0x1c00000	/* RF type */
#define ATW_SYNCTL_DATA_MASK	0x3fffff	/* synthesizer setting */

#define ATW_PLCPHD_SIGNAL_MASK	0xff000000	/* signal field in PLCP header,
						 * only for beacon, ATIM, and
						 * RTS.
						 */
#define ATW_PLCPHD_SERVICE_MASK	0xff0000	/* service field in PLCP
						 * header; with RFMD BBP,
						 * sets Tx power for beacon,
						 * RTS, ATIM.
						 */
#define ATW_PLCPHD_PMBL		BIT(15)		/* 0: long preamble, 1: short */

#define	ATW_MMIWADDR_LENLO_MASK		0xff000000	/* tx: written 4th */
#define	ATW_MMIWADDR_LENHI_MASK		0xff0000	/* tx: written 3rd */
#define	ATW_MMIWADDR_GAIN_MASK		0xff00	/* tx: written 2nd */
#define	ATW_MMIWADDR_RATE_MASK		0xff	/* tx: written 1st */

/* was magic 0x100E0C0A */
#define ATW_MMIWADDR_INTERSIL			  \
	(LSHIFT(0x0c, ATW_MMIWADDR_GAIN_MASK)	| \
	 LSHIFT(0x0a, ATW_MMIWADDR_RATE_MASK)	| \
	 LSHIFT(0x0e, ATW_MMIWADDR_LENHI_MASK)	| \
	 LSHIFT(0x10, ATW_MMIWADDR_LENLO_MASK))

/* was magic 0x00009101
 *
 * ADMtek sets the AI bit on the ATW_MMIWADDR_GAIN_MASK address to
 * put the RF3000 into auto-increment mode so that it can write Tx gain,
 * Tx length (high) and Tx length (low) registers back-to-back.
 */
#define ATW_MMIWADDR_RFMD						\
	(LSHIFT(RF3000_TWI_AI|RF3000_GAINCTL, ATW_MMIWADDR_GAIN_MASK) | \
	 LSHIFT(RF3000_CTL, ATW_MMIWADDR_RATE_MASK))

#define	ATW_MMIRADDR1_RSVD_MASK		0xff000000
#define	ATW_MMIRADDR1_PWRLVL_MASK	0xff0000
#define	ATW_MMIRADDR1_RSSI_MASK		0xff00
#define	ATW_MMIRADDR1_RXSTAT_MASK	0xff

/* was magic 0x00007c7e
 *
 * TBD document registers for Intersil 3861 baseband
 */
#define ATW_MMIRADDR1_INTERSIL	\
	(LSHIFT(0x7c, ATW_MMIRADDR1_RSSI_MASK) | \
	 LSHIFT(0x7e, ATW_MMIRADDR1_RXSTAT_MASK))

/* was magic 0x00000301 */
#define ATW_MMIRADDR1_RFMD	\
	(LSHIFT(RF3000_RSSI, ATW_MMIRADDR1_RSSI_MASK) | \
	 LSHIFT(RF3000_RXSTAT, ATW_MMIRADDR1_RXSTAT_MASK))

/* was magic 0x00100000 */
#define ATW_MMIRADDR2_INTERSIL	\
	(LSHIFT(0x0, ATW_MMIRADDR2_ID_MASK) | \
	 LSHIFT(0x10, ATW_MMIRADDR2_RXPECNT_MASK))

/* was magic 0x7e100000 */
#define ATW_MMIRADDR2_RFMD	\
	(LSHIFT(0x7e, ATW_MMIRADDR2_ID_MASK) | \
	 LSHIFT(0x10, ATW_MMIRADDR2_RXPECNT_MASK))

#define	ATW_MMIRADDR2_ID_MASK	0xff000000	/* 1st element ID in WEP table
						 * for Probe Response (huh?)
						 */
/* RXPE is re-asserted after RXPECNT * 22MHz. */
#define	ATW_MMIRADDR2_RXPECNT_MASK	0xff0000
#define	ATW_MMIRADDR2_PROREXT		BIT(15)		/* Probe Response
							 * 11Mb/s length
							 * extension.
							 */
#define	ATW_MMIRADDR2_PRORLEN_MASK	0x7fff	/* Probe Response
							 * microsecond length
							 */

#define ATW_TXBR_ALCUPDATE_MASK	BIT(31)		/* auto-update BBP with ALCSET */
#define ATW_TXBR_TBCNT_MASK	0x1f0000	/* transmit burst count */
#define ATW_TXBR_ALCSET_MASK	0xff00	/* TX power level set point */
#define ATW_TXBR_ALCREF_MASK	0xff	/* TX power level reference point */

#define ATW_ALCSTAT_MCOV_MASK	BIT(27)		/* MPDU count overflow */
#define ATW_ALCSTAT_ESOV_MASK	BIT(26)		/* error sum overflow */
#define ATW_ALCSTAT_MCNT_MASK	0x3ff0000	/* MPDU count, unsigned integer */
#define ATW_ALCSTAT_ERSUM_MASK	0xffff	/* power error sum,
						 * 2's complement signed integer
						 */

#define ATW_TOFS2_PWR1UP_MASK	0xf0000000	/* delay of Tx/Rx from PE1,
						 * Radio, PHYRST change after
						 * power-up, in 2ms units
						 */
#define ATW_TOFS2_PWR0PAPE_MASK	0xf000000	/* delay of PAPE going low
						 * after internal data
						 * transmit end, in us
						 */
#define ATW_TOFS2_PWR1PAPE_MASK	0xf00000	/* delay of PAPE going high
						 * after TXPE asserted, in us
						 */
#define ATW_TOFS2_PWR0TRSW_MASK	0xf0000	/* delay of TRSW going low
						 * after internal data transmit
						 * end, in us
						 */
#define ATW_TOFS2_PWR1TRSW_MASK	0xf000	/* delay of TRSW going high
						 * after TXPE asserted, in us
						 */
#define ATW_TOFS2_PWR0PE2_MASK	0xf00	/* delay of PE2 going low
						 * after internal data transmit
						 * end, in us
						 */
#define ATW_TOFS2_PWR1PE2_MASK	0xf0	/* delay of PE2 going high
						 * after TXPE asserted, in us
						 */
#define ATW_TOFS2_PWR0TXPE_MASK	0xf	/* delay of TXPE going low
						 * after internal data transmit
						 * end, in us
						 */

#define ATW_CMDR_PM		BIT(19)		/* enables power mgmt
						 * capabilities.
						 */
#define ATW_CMDR_APM		BIT(18)		/* APM mode, effective when
						 * PM = 1.
						 */
#define ATW_CMDR_RTE		BIT(4)		/* enable Rx FIFO threshold */
#define ATW_CMDR_DRT_MASK	0xc	/* drain Rx FIFO threshold */
/* 32 bytes */
#define ATW_CMDR_DRT_8DW	LSHIFT(0x0, ATW_CMDR_DRT_MASK)
/* 64 bytes */
#define ATW_CMDR_DRT_16DW	LSHIFT(0x1, ATW_CMDR_DRT_MASK)
/* Store & Forward */
#define ATW_CMDR_DRT_SF		LSHIFT(0x2, ATW_CMDR_DRT_MASK)
/* Reserved */
#define ATW_CMDR_DRT_RSVD	LSHIFT(0x3, ATW_CMDR_DRT_MASK)
#define ATW_CMDR_SINT_MASK	BIT(1)		/* software interrupt---huh? */

/* TBD PCIC */

/* TBD PMCSR */


#define ATW_PAR0_PAB0_MASK	0xff	/* MAC address byte 0 */
#define ATW_PAR0_PAB1_MASK	0xff00	/* MAC address byte 1 */
#define ATW_PAR0_PAB2_MASK	0xff0000	/* MAC address byte 2 */
#define ATW_PAR0_PAB3_MASK	0xff000000	/* MAC address byte 3 */

#define	ATW_C_PAR1_CTD		0xffff0000	/* Continuous Tx pattern */ 
#define ATW_PAR1_PAB5_MASK	0xff00	/* MAC address byte 5 */
#define ATW_PAR1_PAB4_MASK	0xff	/* MAC address byte 4 */

#define ATW_MAR0_MAB3_MASK	0xff000000	/* multicast table bits 31:24 */
#define ATW_MAR0_MAB2_MASK	0xff0000	/* multicast table bits 23:16 */
#define ATW_MAR0_MAB1_MASK	0xff00	/* multicast table bits 15:8 */
#define ATW_MAR0_MAB0_MASK	0xff	/* multicast table bits 7:0 */

#define ATW_MAR1_MAB7_MASK	0xff000000	/* multicast table bits 63:56 */
#define ATW_MAR1_MAB6_MASK	0xff0000	/* multicast table bits 55:48 */
#define ATW_MAR1_MAB5_MASK	0xff00	/* multicast table bits 47:40 */
#define ATW_MAR1_MAB4_MASK	0xff	/* multicast table bits 39:32 */

/* ATIM destination address */
#define ATW_ATIMDA0_ATIMB3_MASK	0xff000000
#define ATW_ATIMDA0_ATIMB2_MASK	0xff0000
#define ATW_ATIMDA0_ATIMB1_MASK	0xff00
#define ATW_ATIMDA0_ATIMB0_MASK	0xff

/* ATIM destination address, BSSID */
#define ATW_ABDA1_BSSIDB5_MASK	0xff000000
#define ATW_ABDA1_BSSIDB4_MASK	0xff0000
#define ATW_ABDA1_ATIMB5_MASK	0xff00
#define ATW_ABDA1_ATIMB4_MASK	0xff

/* BSSID */
#define ATW_BSSID0_BSSIDB3_MASK	0xff000000
#define ATW_BSSID0_BSSIDB2_MASK	0xff0000
#define ATW_BSSID0_BSSIDB1_MASK	0xff00
#define ATW_BSSID0_BSSIDB0_MASK	0xff

#define ATW_TXLMT_MTMLT_MASK	0xffff0000	/* max TX MSDU lifetime in TU */
#define ATW_TXLMT_SRTYLIM_MASK	0xff	/* short retry limit */

#define ATW_MIBCNT_FFCNT_MASK	0xff000000	/* FCS failure count */
#define ATW_MIBCNT_AFCNT_MASK	0xff0000	/* ACK failure count */
#define ATW_MIBCNT_RSCNT_MASK	0xff00	/* RTS success count */
#define ATW_MIBCNT_RFCNT_MASK	0xff	/* RTS failure count */

#define ATW_BCNT_PLCPH_MASK	0xff0000	/* 11M PLCP length (us) */
#define ATW_BCNT_PLCPL_MASK	0xff00	/* 5.5M PLCP length (us) */
#define ATW_BCNT_BCNT_MASK	0xff	/* byte count of beacon frame */

/* For ADM8211C/CR */
/* ATW_C_TSC_TIMTABSEL = 1 */
#define ATW_C_BCNT_EXTEN1	BIT(31)		/* 11M beacon len. extension */
#define ATW_C_BCNT_BEANLEN1	0x7fff0000	/* beacon length in us */
/* ATW_C_TSC_TIMTABSEL = 0 */
#define ATW_C_BCNT_EXTEN0	BIT(15)		/* 11M beacon len. extension */
#define ATW_C_BCNT_BEANLEN0	BIT(14,0)	/* beacon length in us */

#define ATW_C_TSC_TIMOFS	0xff000000	/* I think this is the
						 * SRAM offset for the TIM
						 */
#define ATW_C_TSC_TIMLEN	0x3ff000	/* length of TIM */
#define ATW_C_TSC_TIMTABSEL	BIT(4)		/* select TIM table 0 or 1 */
#define ATW_TSC_TSC_MASK	0xf	/* TSFT countdown value, 0
						 * disables
						 */

#define ATW_SYNRF_SELSYN	BIT(31)	/* 0: MAC controls SYN IF pins,
					 * 1: ATW_SYNRF controls SYN IF pins.
					 */
#define ATW_SYNRF_SELRF		BIT(30)	/* 0: MAC controls RF IF pins,
					 * 1: ATW_SYNRF controls RF IF pins.
					 */
#define ATW_SYNRF_LERF		BIT(29)	/* if SELSYN = 1, direct control of
					 * LERF# pin
					 */
#define ATW_SYNRF_LEIF		BIT(28)	/* if SELSYN = 1, direct control of
					 * LEIF# pin
					 */
#define ATW_SYNRF_SYNCLK	BIT(27)	/* if SELSYN = 1, direct control of
					 * SYNCLK pin
					 */
#define ATW_SYNRF_SYNDATA	BIT(26)	/* if SELSYN = 1, direct control of
					 * SYNDATA pin
					 */
#define ATW_SYNRF_PE1		BIT(25)	/* if SELRF = 1, direct control of
					 * PE1 pin
					 */
#define ATW_SYNRF_PE2		BIT(24)	/* if SELRF = 1, direct control of
					 * PE2 pin
					 */
#define ATW_SYNRF_PAPE		BIT(23)	/* if SELRF = 1, direct control of
					 * PAPE pin
					 */
#define ATW_C_SYNRF_TRSW	BIT(22)	/* if SELRF = 1, direct control of
					 * TRSW pin
					 */
#define ATW_C_SYNRF_TRSWN	BIT(21)	/* if SELRF = 1, direct control of
					 * TRSWn pin
					 */
#define ATW_SYNRF_INTERSIL_EN	BIT(20)	/* if SELRF = 1, enables
					 * some signal used by the
					 * Intersil RF front-end?
					 * Undocumented.
					 */
#define ATW_SYNRF_PHYRST	BIT(18)	/* if SELRF = 1, direct control of
					 * PHYRST# pin
					 */
/* 1: force TXPE = RXPE = 1 if ATW_CMDR[27] = 0. */
#define ATW_C_SYNRF_RF2958PD	ATW_SYNRF_PHYRST

#define ATW_BPLI_BP_MASK	0xffff0000	/* beacon interval in TU */
#define ATW_BPLI_LI_MASK	0xffff	/* STA listen interval in
						 * beacon intervals
						 */

#define ATW_C_CAP0_TIMLEN1	0xff000000	/* TIM table 1 len in bytes
						 * including TIM ID (XXX huh?)
						 */
#define ATW_C_CAP0_TIMLEN0	0xff0000	/* TIM table 0 len in bytes,
						 * including TIM ID (XXX huh?)
						 */
#define	ATW_C_CAP0_CWMAX	0xf00	/* 1 <= CWMAX <= 5 fixes CW?
						 * 5 < CWMAX <= 9 sets max?
						 * 10?
						 * default 0
						 */
#define ATW_CAP0_RCVDTIM	BIT(4)		/* receive every DTIM */
#define ATW_CAP0_CHN_MASK	0xf	/* current DSSS channel */

#define ATW_CAP1_CAPI_MASK	0xffff0000	/* capability information */
#define ATW_CAP1_ATIMW_MASK	0xffff	/* ATIM window in TU */

#define ATW_RMD_ATIMST		BIT(31)		/* ATIM frame TX status */
#define ATW_RMD_CFP		BIT(30)		/* CFP indicator */
#define ATW_RMD_PCNT		0xfff0000	/* idle time between
						 * awake/ps mode, in seconds
						 */
#define ATW_RMD_RMRD_MASK	0xffff	/* max RX reception duration
						 * in us
						 */

#define ATW_CFPP_CFPP		0xff000000	/* CFP unit DTIM */
#define ATW_CFPP_CFPMD		0xffff00	/* CFP max duration in TU */
#define ATW_CFPP_DTIMP		0xff	/* DTIM period in beacon
						 * intervals
						 */
#define ATW_TOFS0_USCNT_MASK	0x3f000000	/* number of system clocks
						 * in 1 microsecond.
						 * Depends PCI bus speed?
						 */
#define ATW_C_TOFS0_TUCNT_MASK	0x7c00	/* PIFS (microseconds) */
#define ATW_TOFS0_TUCNT_MASK	0x3ff	/* TU counter in microseconds */

/* TBD TOFS1 */
#define ATW_TOFS1_TSFTOFSR_MASK	0xff000000	/* RX TSFT offset in
						 * microseconds: RF+BBP
						 * latency
						 */
#define ATW_TOFS1_TBTTPRE_MASK	0xffff00	/* prediction time, (next
						 * Nth TBTT - TBTTOFS) in
						 * microseconds (huh?). To
						 * match TSFT[25:10] (huh?).
						 */
#define	ATW_TBTTPRE_MASK	0x3fffc00
#define ATW_TOFS1_TBTTOFS_MASK	0xff	/* wake-up time offset before
						 * TBTT in TU
						 */
#define ATW_IFST_SLOT_MASK	0xf800000	/* SLOT time in us */
#define ATW_IFST_SIFS_MASK	0x7f8000	/* SIFS time in us */
#define ATW_IFST_DIFS_MASK	0x7e00	/* DIFS time in us */
#define ATW_IFST_EIFS_MASK	0x1ff	/* EIFS time in us */

#define ATW_RSPT_MART_MASK	0xffff0000	/* max response time in us */
#define ATW_RSPT_MIRT_MASK	0xff00	/* min response time in us */
#define ATW_RSPT_TSFTOFST_MASK	0xff	/* TX TSFT offset in us */

#define ATW_WEPCTL_WEPENABLE	BIT(31)		/* enable WEP engine */
#define ATW_WEPCTL_AUTOSWITCH	BIT(30)		/* auto-switch enable (huh?) */
#define ATW_WEPCTL_CURTBL	BIT(29)		/* current table in use */
#define ATW_WEPCTL_WR		BIT(28)		/* */
#define ATW_WEPCTL_RD		BIT(27)		/* */
#define ATW_WEPCTL_WEPRXBYP	BIT(25)		/* bypass WEP on RX */
#define ATW_WEPCTL_SHKEY	BIT(24)		/* 1: pass to host if tbl
						 * lookup fails, 0: use
						 * shared-key
						 */
#define ATW_WEPCTL_UNKNOWN0	BIT(23)		/* has something to do with
						 * revision 0x20. Possibly
						 * selects a different WEP
						 * table.
						 */
#define ATW_WEPCTL_TBLADD_MASK	0x1ff	/* add to table */

/* set these bits in the second byte of a SRAM shared key record to affect
 * the use and interpretation of the key in the record.
 */
#define ATW_WEP_ENABLED	BIT(7)
#define ATW_WEP_104BIT	BIT(6)

#define ATW_WESK_DATA_MASK	0xffff	/* data */
#define ATW_WEPCNT_WIEC_MASK	0xffff	/* WEP ICV error count */

#define ATW_MACTEST_FORCE_IV		BIT(23)
#define ATW_MACTEST_FORCE_KEYID		BIT(22)
#define ATW_MACTEST_KEYID_MASK		0x300000
#define ATW_MACTEST_MMI_USETXCLK	BIT(11)

/* Function Event/Status registers */

#define ATW_FER_INTR		BIT(15)	/* interrupt: set regardless of mask */ 
#define ATW_FER_GWAKE		BIT(4)	/* general wake-up: set regardless of mask */ 

#define ATW_FEMR_INTR_EN	BIT(15)	/* enable INTA# */
#define ATW_FEMR_WAKEUP_EN	BIT(14)	/* enable wake-up */
#define ATW_FEMR_GWAKE_EN	BIT(4)	/* enable general wake-up */

#define ATW_FPSR_INTR_STATUS	BIT(15)	/* interrupt status */
#define ATW_FPSR_WAKEUP_STATUS	BIT(4)	/* CSTSCHG state */
#define ATW_FFER_INTA_FORCE	BIT(15)	/* activate INTA (if not masked) */
#define ATW_FFER_GWAKE_FORCE	BIT(4)	/* activate CSTSCHG (if not masked) */

/* Serial EEPROM offsets */
#define ATW_SR_CLASS_CODE	(0x00/2)
#define ATW_SR_FORMAT_VERSION	(0x02/2)
#define		ATW_SR_MAJOR_MASK	0xff
#define		ATW_SR_MINOR_MASK	0xff00
#define ATW_SR_MAC00		(0x08/2)	/* CSR21 */
#define ATW_SR_MAC01		(0x0A/2)	/* CSR21/22 */
#define ATW_SR_MAC10		(0x0C/2)	/* CSR22 */
#define ATW_SR_CSR20		(0x16/2)
#define		ATW_SR_ANT_MASK		0x1c00
#define		ATW_SR_PWRSCALE_MASK	0x300
#define		ATW_SR_CLKSAVE_MASK	0xc0
#define		ATW_SR_RFTYPE_MASK	0x38
#define		ATW_SR_BBPTYPE_MASK	0x7
#define ATW_SR_CR28_CR03	(0x18/2)
#define		ATW_SR_CR28_MASK	0xff00
#define		ATW_SR_CR03_MASK	0xff
#define ATW_SR_CTRY_CR29	(0x1A/2)
#define		ATW_SR_CTRY_MASK	0xff00	/* country code */
#define			COUNTRY_FCC	0
#define			COUNTRY_IC	1
#define			COUNTRY_ETSI	2
#define			COUNTRY_SPAIN	3
#define			COUNTRY_FRANCE	4
#define			COUNTRY_MMK	5
#define			COUNTRY_MMK2	6
#define		ATW_SR_CR29_MASK	0xff
#define ATW_SR_PCI_DEVICE	(0x20/2)	/* CR0 */
#define ATW_SR_PCI_VENDOR	(0x22/2)	/* CR0 */
#define ATW_SR_SUB_DEVICE	(0x24/2)	/* CR11 */
#define ATW_SR_SUB_VENDOR	(0x26/2)	/* CR11 */
#define ATW_SR_CR15		(0x28/2)
#define ATW_SR_LOCISPTR		(0x2A/2)	/* CR10 */
#define ATW_SR_HICISPTR		(0x2C/2)	/* CR10 */
#define ATW_SR_CSR18		(0x2E/2)
#define ATW_SR_D0_D1_PWR	(0x40/2)	/* CR49 */
#define ATW_SR_D2_D3_PWR	(0x42/2)	/* CR49 */
#define ATW_SR_CIS_WORDS	(0x52/2)
/* CR17 of RFMD RF3000 BBP: returns TWO channels */
#define ATW_SR_TXPOWER(chnl)		(0x54/2 + ((chnl) - 1)/2)
/* CR20 of RFMD RF3000 BBP: returns TWO channels */
#define ATW_SR_LPF_CUTOFF(chnl)		(0x62/2 + ((chnl) - 1)/2)
/* CR21 of RFMD RF3000 BBP: returns TWO channels */
#define ATW_SR_LNA_GS_THRESH(chnl)	(0x70/2 + ((chnl) - 1)/2)
#define ATW_SR_CHECKSUM		(0x7e/2)	/* for data 0x00-0x7d */
#define ATW_SR_CIS		(0x80/2)	/* Cardbus CIS */

/* Tx descriptor */ 
struct atw_txdesc {
	u_int32_t	at_ctl;
#define at_stat at_ctl
	u_int32_t	at_flags;
	u_int32_t	at_buf1;
	u_int32_t	at_buf2;
};

#define ATW_TXCTL_OWN		BIT(31)		/* 1: ready to transmit */
#define ATW_TXCTL_DONE		BIT(30)		/* 0: not processed */
#define ATW_TXCTL_TXDR_MASK	0xff00000	/* TX data rate (?) */
#define ATW_TXCTL_TL_MASK	0xfffff	/* retry limit, 0 - 255 */

#define ATW_TXSTAT_OWN		ATW_TXCTL_OWN	/* 0: not for transmission */
#define ATW_TXSTAT_DONE		ATW_TXCTL_DONE	/* 1: been processed */
#define ATW_TXSTAT_ES		BIT(29)		/* 0: TX successful */
#define ATW_TXSTAT_TLT		BIT(28)		/* TX lifetime expired */
#define ATW_TXSTAT_TRT		BIT(27)		/* TX retry limit expired */
#define ATW_TXSTAT_TUF		BIT(26)		/* TX under-run error */
#define ATW_TXSTAT_TRO		BIT(25)		/* TX over-run error */
#define ATW_TXSTAT_SOFBR	BIT(24)		/* packet size != buffer size
						 * (?)
						 */
#define ATW_TXSTAT_ARC_MASK	0xfff	/* accumulated retry count */

#define ATW_TXFLAG_IC		BIT(31)		/* interrupt on completion */
#define ATW_TXFLAG_LS		BIT(30)		/* packet's last descriptor */
#define ATW_TXFLAG_FS		BIT(29)		/* packet's first descriptor */
#define ATW_TXFLAG_TER		BIT(25)		/* end of ring */
#define ATW_TXFLAG_TCH		BIT(24)		/* at_buf2 is 2nd chain */
#define ATW_TXFLAG_TBS2_MASK	0xfff000	/* at_buf2 byte count */
#define ATW_TXFLAG_TBS1_MASK	0xfff	/* at_buf1 byte count */

/* Rx descriptor */ 
struct atw_rxdesc {
    u_int32_t	ar_stat;
    u_int32_t	ar_ctl;
    u_int32_t	ar_buf1;
    u_int32_t	ar_buf2;
};

#define	ar_rssi	ar_ctl

#define ATW_RXCTL_RER		BIT(25)		/* end of ring */
#define ATW_RXCTL_RCH		BIT(24)		/* ar_buf2 is 2nd chain */
#define ATW_RXCTL_RBS2_MASK	0xfff000	/* ar_buf2 byte count */
#define ATW_RXCTL_RBS1_MASK	0xfff	/* ar_buf1 byte count */

#define ATW_RXSTAT_OWN		BIT(31)		/* 1: NIC may fill descriptor */
#define ATW_RXSTAT_ES		BIT(30)		/* error summary, 0 on 
						 * success
						 */
#define ATW_RXSTAT_SQL		BIT(29)		/* has signal quality (?) */
#define ATW_RXSTAT_DE		BIT(28)		/* descriptor error---packet is
						 * truncated. last descriptor
						 * only
						 */
#define ATW_RXSTAT_FS		BIT(27)		/* packet's first descriptor */
#define ATW_RXSTAT_LS		BIT(26)		/* packet's last descriptor */
#define ATW_RXSTAT_PCF		BIT(25)		/* received during CFP */
#define ATW_RXSTAT_SFDE		BIT(24)		/* PLCP SFD error */
#define ATW_RXSTAT_SIGE		BIT(23)		/* PLCP signal error */
#define ATW_RXSTAT_CRC16E	BIT(22)		/* PLCP CRC16 error */
#define ATW_RXSTAT_RXTOE	BIT(21)		/* RX time-out, last descriptor
						 * only.
						 */
#define ATW_RXSTAT_CRC32E	BIT(20)		/* CRC32 error */
#define ATW_RXSTAT_ICVE		BIT(19)		/* WEP ICV error */
#define ATW_RXSTAT_DA1		BIT(17)		/* DA bit 1, admin'd address */
#define ATW_RXSTAT_DA0		BIT(16)		/* DA bit 0, group address */
#define ATW_RXSTAT_RXDR_MASK	0xf000	/* RX data rate */
#define ATW_RXSTAT_FL_MASK	0xfff	/* RX frame length, last
						 * descriptor only
						 */

/* Static RAM (contains WEP keys, beacon content). Addresses and size
 * are in 16-bit words.
 */
#define ATW_SRAM_ADDR_INDIVL_KEY	0x0
#define ATW_SRAM_ADDR_SHARED_KEY	(0x160 * 2)
#define ATW_SRAM_ADDR_SSID	(0x180 * 2)
#define ATW_SRAM_ADDR_SUPRATES	(0x191 * 2)
#define ATW_SRAM_MAXSIZE	(0x200 * 2)
#define ATW_SRAM_A_SIZE		ATW_SRAM_MAXSIZE
#define ATW_SRAM_B_SIZE		(0x1c0 * 2)

