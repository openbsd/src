/*	$OpenBSD: sgecreg.h,v 1.4 2011/09/26 21:44:04 miod Exp $	*/
/*	$NetBSD: sgecreg.h,v 1.1 1999/08/08 11:41:29 ragge Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corp.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


/* Driver for SGEC (second generation Ethernet controller) chip, type DC-541,
   found on the KA670 (and probably other) CPU.

   17 May 1998...Jay Maynard, jmaynard@phoenix.net
*/

/* SGEC CSRs */
struct zedevice {
	u_long ze_nicsr0;	/* vector address, IPL, sync mode */
	u_long ze_nicsr1;	/* TX poll demand */
	u_long ze_nicsr2;	/* RX poll demand */
	struct ze_rdes *ze_nicsr3;	/* RX descriptor list address */
	struct ze_tdes *ze_nicsr4;	/* TX descriptor list address */
	u_long ze_nicsr5;	/* SGEC status */
	u_long ze_nicsr6;	/* SGEC command/mode */
	u_long ze_nicsr7;	/* system page table base address */
	u_long ze_nivcsr8;	/* reserved virtual CSR */
	u_long ze_nivcsr9;	/* watchdog timers (virtual) */
	u_long ze_nivcsr10;	/* revision, missed frame count (v) */
	u_long ze_nivcsr11;	/* boot message verification (low) (v) */
	u_long ze_nivcsr12;	/* boot message verification (high) (v) */
	u_long ze_nivcsr13;	/* boot message processor (v) */
	u_long ze_nivcsr14;	/* diagnostic breakpoint (v) */
	u_long ze_nicsr15;	/* monitor command */
};

/*
 * Register offsets.
 */
#define	ZE_CSR0		0
#define	ZE_CSR1		4
#define	ZE_CSR2		8
#define	ZE_CSR3		12
#define	ZE_CSR4		16
#define	ZE_CSR5		20
#define	ZE_CSR6		24
#define	ZE_CSR7		28
#define	ZE_CSR8		32
#define	ZE_CSR9		36
#define	ZE_CSR10	40
#define	ZE_CSR11	44
#define	ZE_CSR12	48
#define	ZE_CSR13	52
#define	ZE_CSR14	56
#define	ZE_CSR15	60

/* SGEC bit definitions */
/* NICSR0: */
#define ZE_NICSR0_IPL 0xc0000000	/* interrupt priority level: */
#define ZE_NICSR0_IPL14 0x00000000	/* 0x14 */
#define ZE_NICSR0_IPL15 0x40000000	/* 0x15 */
#define ZE_NICSR0_IPL16 0x80000000	/* 0x16 */
#define ZE_NICSR0_IPL17 0xc0000000	/* 0x17 */
#define ZE_NICSR0_SA 0x20000000		/* sync(1)/async mode */
#define ZE_NICSR0_MBO 0x1fff0003	/* must be set to one on write */
#define ZE_NICSR0_IV_MASK 0x0000fffc	/* bits for the interrupt vector */

/* NICSR1: */
#define ZE_NICSR1_TXPD 0xffffffff	/* transmit polling demand */

/* NICSR2: */
#define ZE_NICSR2_RXPD 0xffffffff	/* receive polling demand */

/* NICSR3 and NICSR4 are pure addresses */
/* NICSR5: */
#define ZE_NICSR5_ID 0x80000000		/* init done */
#define ZE_NICSR5_SF 0x40000000		/* self-test failed */
#define ZE_NICSR5_SS 0x3c000000		/* self-test status field */
#define ZE_NICSR5_TS 0x03000000		/* transmission state: */
#define ZE_NICSR5_TS_STOP 0x00000000	/* stopped */
#define ZE_NICSR5_TS_RUN 0x01000000	/* running */
#define ZE_NICSR5_TS_SUSP 0x02000000	/* suspended */
#define ZE_NICSR5_RS 0x00c00000		/* reception state: */
#define ZE_NICSR5_RS_STOP 0x00000000	/* stopped */
#define ZE_NICSR5_RS_RUN 0x00400000	/* running */
#define ZE_NICSR5_RS_SUSP 0x00800000	/* suspended */
#define ZE_NICSR5_OM 0x00060000		/* operating mode: */
#define ZE_NICSR5_OM_NORM 0x00000000	/* normal */
#define ZE_NICSR5_OM_ILBK 0x00020000	/* internal loopback */
#define ZE_NICSR5_OM_ELBK 0x00040000	/* external loopback */
#define ZE_NICSR5_OM_DIAG 0x00060000	/* reserved for diags */
#define ZE_NICSR5_DN 0x00010000		/* virtual CSR access done */
#define ZE_NICSR5_MBO 0x0038ff00	/* must be one */
#define ZE_NICSR5_BO 0x00000080		/* boot message received */
#define ZE_NICSR5_TW 0x00000040		/* transmit watchdog timeout */
#define ZE_NICSR5_RW 0x00000020		/* receive watchdog timeout */
#define ZE_NICSR5_ME 0x00000010		/* memory error */
#define ZE_NICSR5_RU 0x00000008		/* receive buffer unavailable */
#define ZE_NICSR5_RI 0x00000004		/* receiver interrupt */
#define ZE_NICSR5_TI 0x00000002		/* transmitter interrupt */
#define ZE_NICSR5_IS 0x00000001		/* interrupt summary */
/* whew! */

/* NICSR6: */
#define ZE_NICSR6_RE 0x80000000		/* reset */
#define ZE_NICSR6_IE 0x40000000		/* interrupt enable */
#define ZE_NICSR6_MBO 0x01e7f000	/* must be one */
#define ZE_NICSR6_BL 0x1e000000		/* burst limit mask */
#define ZE_NICSR6_BL_8 0x10000000	/* 8 longwords */
#define ZE_NICSR6_BL_4 0x08000000	/* 4 longwords */
#define ZE_NICSR6_BL_2 0x04000000	/* 2 longwords */
#define ZE_NICSR6_BL_1 0x02000000	/* 1 longword */
#define ZE_NICSR6_BE 0x00100000		/* boot message enable */
#define ZE_NICSR6_SE 0x00080000		/* single cycle enable */
#define ZE_NICSR6_ST 0x00000800		/* start(1)/stop(0) transmission */
#define ZE_NICSR6_SR 0x00000400		/* start(1)/stop(0) reception */
#define ZE_NICSR6_OM 0x00000300		/* operating mode: */
#define ZE_NICSR6_OM_NORM 0x00000000	/* normal */
#define ZE_NICSR6_OM_ILBK 0x00000100	/* internal loopback */
#define ZE_NICSR6_OM_ELBK 0x00000200	/* external loopback */
#define ZE_NICSR6_OM_DIAG 0x00000300	/* reserved for diags */
#define ZE_NICSR6_DC 0x00000080		/* disable data chaining */
#define ZE_NICSR6_FC 0x00000040		/* force collision mode */
#define ZE_NICSR6_PB 0x00000008		/* pass bad frames */
#define ZE_NICSR6_AF 0x00000006		/* address filtering mode: */
#define ZE_NICSR6_AF_NORM 0x00000000	/* normal filtering */
#define ZE_NICSR6_AF_PROM 0x00000002	/* promiscuous mode */
#define ZE_NICSR6_AF_ALLM 0x00000004	/* all multicasts */

/* NICSR7 is an address, NICSR8 is reserved */
/* NICSR9: */
#define ZE_VNICSR9_RT 0xffff0000	/* receiver timeout, *1.6 us */
#define ZE_VNICSR9_TT 0x0000ffff	/* transmitter timeout */

/* NICSR10: */
#define ZE_VNICSR10_RN 0x001f0000	/* SGEC version */
#define ZE_VNICSR10_MFC 0x0000ffff	/* missed frame counter */

/* if you want to know what's in NICSRs 11-15, define them yourself! */

/* Descriptors: */
/* Receive descriptor */
struct ze_rdes {
	u_short ze_rdes0;		/* descriptor word 0 flags */
	u_short ze_framelen;		/* received frame length */
	u_char ze_rsvd1[3];		/* unused bytes */
	u_char ze_rdes1;		/* descriptor word 1 flags */
	short ze_pageoffset;		/* offset of buffer in page */
	short ze_bufsize;		/* length of data buffer */
	u_char *ze_bufaddr;		/* address of data buffer */
};

/* Receive descriptor bits */
#define ZE_FRAMELEN_OW 0x8000		/* SGEC owns this descriptor */
#define ZE_RDES0_ES 0x8000		/* an error has occurred */
#define ZE_RDES0_LE 0x4000		/* length error */
#define ZE_RDES0_DT 0x3000		/* data type: */
#define ZE_RDES0_DT_NORM 0x0000		/* normal frame */
#define ZE_RDES0_DT_ILBK 0x1000		/* internally looped back frame */
#define ZE_RDES0_DT_ELBK 0x2000		/* externally looped back frame */
#define ZE_RDES0_RF 0x0800		/* runt frame */
#define ZE_RDES0_BO 0x0400		/* buffer overflow */
#define ZE_RDES0_FS 0x0200		/* first segment */
#define ZE_RDES0_LS 0x0100		/* last segment */
#define ZE_RDES0_TL 0x0080		/* frame too long */
#define ZE_RDES0_CS 0x0040		/* collision seen */
#define ZE_RDES0_FT 0x0020		/* Ethernet frame type */
#define ZE_RDES0_TN 0x0008		/* address translation not valid */
#define ZE_RDES0_DB 0x0004		/* dribbling bits seen */
#define ZE_RDES0_CE 0x0002		/* CRC error */
#define ZE_RDES0_OF 0x0001		/* internal FIFO overflow */
#define ZE_RDES1_CA 0x80		/* chain address */
#define ZE_RDES1_VA 0x40		/* virtual address */
#define ZE_RDES1_VT 0x20		/* virtual(1)/phys PTE address */

/* Transmit descriptor */
struct ze_tdes {
	u_short ze_tdes0;		/* descriptor word 0 flags */
	u_short ze_tdr;			/* TDR count of cable fault */
	u_char ze_rsvd1[2];		/* unused bytes */
	u_short ze_tdes1;		/* descriptor word 1 flags */
	short ze_pageoffset;		/* offset of buffer in page */
	short ze_bufsize;		/* length of data buffer */
	u_char *ze_bufaddr;		/* address of data buffer */
};

/* Transmit descriptor bits */
#define ZE_TDR_OW 0x8000		/* SGEC owns this descriptor */
#define ZE_TDES0_ES 0x8000		/* an error has occurred */
#define ZE_TDES0_TO 0x4000		/* transmit watchdog timeout */
#define ZE_TDES0_LE 0x1000		/* length error */
#define ZE_TDES0_LO 0x0800		/* loss of carrier */
#define ZE_TDES0_NC 0x0400		/* no carrier */
#define ZE_TDES0_LC 0x0200		/* late collision */
#define ZE_TDES0_EC 0x0100		/* excessive collisions */
#define ZE_TDES0_HF 0x0080		/* heartbeat fail */
#define ZE_TDES0_CC 0x0078		/* collision count mask */
#define ZE_TDES0_TN 0x0004		/* address translation invalid */
#define ZE_TDES0_UF 0x0002		/* underflow */
#define ZE_TDES0_DE 0x0001		/* transmission deferred */
#define ZE_TDES1_CA 0x8000		/* chain address */
#define ZE_TDES1_VA 0x4000		/* virtual address */
#define ZE_TDES1_DT 0x3000		/* data type: */
#define ZE_TDES1_DT_NORM 0x0000		/* normal transmit frame */
#define ZE_TDES1_DT_SETUP 0x2000	/* setup frame */
#define ZE_TDES1_DT_DIAG 0x3000		/* diagnostic frame */
#define ZE_TDES1_AC 0x0800		/* CRC disable */
#define ZE_TDES1_FS 0x0400		/* first segment */
#define ZE_TDES1_LS 0x0200		/* last segment */
#define ZE_TDES1_IC 0x0100		/* interrupt on completion */
#define ZE_TDES1_VT 0x0080		/* virtual(1)/phys PTE address */

