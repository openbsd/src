/* $OpenBSD: mtd803reg.h,v 1.2 2003/08/19 04:03:53 mickey Exp $ */
/* $NetBSD: mtd803reg.h,v 1.1 2002/11/07 21:56:59 martin Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Peter Bex <Peter.Bex@student.kun.nl>.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
#ifndef __DEV_IC_MTD803REG_H__
#define __DEV_IC_MTD803REG_H__

#define MTD_PCI_LOIO		0x10
#define MTD_PCI_LOMEM		0x14

/* Command and Status Register */
#define MTD_PAR0		0x00		/* Physical address 0-3 */
#define MTD_PAR1		0x04		/* Physical address 4-5 */
#define MTD_MAR0		0x08		/* Multicast address 0-3 */
#define MTD_MAR1		0x0c		/* Multicast address 4-7 */
#define MTD_FAR0		0x10		/* Flowctrl address 0-3 */
#define MTD_FAR1		0x14		/* Flowctrl address 4-5 */
#define MTD_RXTXR		0x18		/* Receive-transmit config */
#define MTD_BCR			0x1c		/* Bus command */
#define MTD_TXPDR		0x20		/* Transmit polling demand */
#define MTD_RXPDR		0x24		/* Receive polling demand */
#define MTD_RCWP		0x28		/* Receive word pointer */
#define MTD_TXLBA		0x2c		/* Transmit list base addr */
#define MTD_RXLBA		0x30		/* Receive list base addr */
#define MTD_ISR			0x34		/* Interrupt Status Register */
#define MTD_IMR			0x38		/* Interrupt Mask Register */
#define MTD_FHLT		0x3c		/* Flow ctrl high/low thresh */
#define MTD_MIIMGT		0x40		/* ROM and MII management */
#define MTD_TALLY		0x44		/* Tally ctr for CRC & MPA */
#define MTD_TSR			0x48		/* Tally ctr for TSR */
#define MTD_PHYBASE		0x4c		/* PHY status & control */
#define MTD_OUI			0x50		/* OUI register */
#define MTD_LPAR		0x54		/* Link Partner, Advertisment */
#define MTD_WUECSR		0x5c		/* Wake-up Events CSR */

#define MTD_ALL_ADDR		0xffffffff	/* Mask all addresses */
#define MTD_TXPDR_DEMAND	0xffffffff	/* Demand transmit polling */
#define MTD_RXPDR_DEMAND	0xffffffff	/* Demand receive polling */

/* PHY registers */
/* Basic mode control register */
#define MTD_PHY_BMCR		0x00

/* Bus Command Register */
#define MTD_BCR_RSRVD1		0xfffffc00	/* Bits [31:10] are reserved */
#define MTD_BCR_PROG		0x00000200	/* Programming */
#define MTD_BCR_RLE		0x00000100	/* Read Line command Enable */
#define MTD_BCR_RME		0x00000080	/* Read Multiple cmd Enable */
#define MTD_BCR_WIE		0x00000040	/* Write and Inval. cmd Enab. */
#define MTD_BCR_BLEN1		0x00000000	/* 1 dword burst length */
#define MTD_BCR_BLEN4		0x00000008	/* 4 dwords burst length */
#define MTD_BCR_BLEN8		0x00000010	/* 8 dwords burst length */
#define MTD_BCR_BLEN16		0x00000018	/* 16 dwords burst length */
#define MTD_BCR_BLEN32		0x00000020	/* 32 dwords burst length */
#define MTD_BCR_BLEN64		0x00000028	/* 64 dwords burst length */
#define MTD_BCR_BLEN128		0x00000030	/* 128 dwords burst length */
#define MTD_BCR_BLEN512		0x00000038	/* 512 dwords burst length */
#define MTD_BCR_RSVRD0		0x00000006	/* Bits [2:1] are reserved */
#define MTD_BCR_RESET		0x00000001	/* Software reset */

#define MTD_TIMEOUT		1000		/* Timeout when resetting */

/* Transmit configuration register */
#define MTD_TX_RUN		0x80000000	/* Transmit running status */
#define MTD_TX_RSRVD1		0x60000000	/* Bits [14:13] are reserved */
#define MTD_TX_BACKOPT		0x10000000	/* Optional backoff */
#define MTD_TX_FASTBACK		0x08000000	/* Fast back-off */
#define MTD_TX_RSRVD0		0x04000000	/* Bit 10 is reserved */
#define MTD_TX_ENH		0x02000000	/* Enhanced mode */
#define MTD_TX_FCTL		0x01000000	/* Transmit fctl packet enable*/
#define MTD_TX_64		0x00000000	/* 64 bytes */
#define MTD_TX_32		0x00200000	/* 32 bytes */
#define MTD_TX_128		0x00400000	/* 128 bytes */
#define MTD_TX_256		0x00600000	/* 256 bytes */
#define MTD_TX_512		0x00800000	/* 512 bytes */
#define MTD_TX_768		0x00a00000	/* 768 bytes */
#define MTD_TX_1024		0x00c00000	/* 1024 bytes */
#define MTD_TX_STFWD		0x00e00000	/* Store and forward */
#define MTD_TX_FDPLX		0x00100000	/* Full duplex mode */
#define MTD_TX_SPD10		0x00080000	/* Port speed is 10M */
#define MTD_TX_ENABLE		0x00040000	/* Transmit enable */
#define MTD_TX_LPBACK		0x00020000	/* Loopback mode bit 1 */
#define MTD_TX_LPBACKZERO	0x00010000	/* Loopback mode bit 0 */

/* Receive configuration register */
#define MTD_RX_RUN		0x00008000	/* Receive running status */
#define MTD_RX_EARLY		0x00004000	/* Early interrupt enable */
#define MTD_RX_FCTL		0x00002000	/* Receive fctl packet enable */
#define MTD_RX_FANA		0x00001000	/* Fctl address undefined(n/a)*/
#define MTD_RX_BLEN		0x00000800	/* Receive burst len enable */
#define MTD_RX_512		0x00000700	/* 512 words */
#define MTD_RX_128		0x00000600	/* 128 words */
#define MTD_RX_64		0x00000500	/* 64 words */
#define MTD_RX_32		0x00000400	/* 32 words */
#define MTD_RX_16		0x00000300	/* 16 words */
#define MTD_RX_8		0x00000200	/* 8 words */
#define MTD_RX_4		0x00000100	/* 4 words */
#define MTD_RX_1		0x00000000	/* 1 word */
#define MTD_RX_PROM		0x00000080	/* Promiscuous mode */
#define MTD_RX_ABROAD		0x00000040	/* Accept broadcast */
#define MTD_RX_AMULTI		0x00000020	/* Accept multicast */
#define MTD_RX_ARP		0x00000008	/* Receive runt packet */
#define MTD_RX_ALP		0x00000004	/* Receive long packet */
#define MTD_RX_ERRP		0x00000002	/* Receive error packet */
#define MTD_RX_ENABLE		0x00000001	/* Receive enable */

/* Interrupt Status Register */
#define MTD_ISR_RSRVD1		0xfff80000	/* Bits [31:19] are reserved */
#define MTD_ISR_PDF		0x00040000	/* Parallel Detection Fault */
#define MTD_ISR_RFCON		0x00020000	/* Receive FCtl xON packet */
#define MRD_ISR_RFCOFF		0x00010000	/* Receive FCtl xOFF packet */
#define MTD_ISR_LSC		0x00008000	/* Link Status Change */
#define MTD_ISR_ANC		0x00004000	/* Autonegotiation complete */
#define MTD_ISR_FBUSERR		0x00002000	/* Fatal bus error */
#define MTD_ISR_PARERR		0x00000000	/* Parity error */
#define MTD_ISR_MASTERR		0x00000800	/* Master error */
#define MTD_ISR_TARERR		0x00001000	/* Target error */
#define MTD_ISR_TXUNDER		0x00000400	/* Transmit underflow */
#define MTD_ISR_RXOVER		0x00000200	/* Receive overflow */
#define MTD_ISR_TXEARLY		0x00000100	/* Transmit early int */
#define MTD_ISR_RXEARLY		0x00000080	/* Receive early int */
#define MTD_ISR_CTROVER		0x00000040	/* Counter overflow */
#define MTD_ISR_RXBUN		0x00000020	/* Receive buffer n/a */
#define MTD_ISR_TXBUN		0x00000010	/* Transmit buffer n/a */
#define MTD_ISR_TXIRQ		0x00000008	/* Transmit interrupt */
#define MTD_ISR_RXIRQ		0x00000004	/* Receive interrupt */
#define MTD_ISR_RXERR		0x00000002	/* Receive error */
#define MTD_ISR_RSRVD0		0x00000001	/* Bit 1 is reserved */

#define MTD_ISR_MASK		MTD_ISR_TXIRQ | MTD_ISR_RXIRQ | MTD_ISR_RXBUN \
				| MTD_ISR_RXERR | MTD_ISR_PDF \
				| MTD_ISR_FBUSERR | MTD_ISR_TXUNDER \
				| MTD_ISR_RXOVER | MTD_ISR_PARERR \
				| MTD_ISR_MASTERR | MTD_ISR_TARERR

#define MTD_ISR_ENABLE		0xffffffff	/* Enable interrupts */

/* Interrupt Mask Register. Essentially the same as ISR */
#define MTD_IMR_RSRVD2		0xfff80000	/* Bits [31:19] are reserved */
#define MTD_IMR_PDF		0x00040000	/* Parallel Detection Fault */
#define MTD_IMR_RFCON		0x00020000	/* Receive FCtl xON packet */
#define MRD_IMR_RFCOFF		0x00010000	/* Receive FCtl xOFF packet */
#define MTD_IMR_LSC		0x00008000	/* Link Status Change */
#define MTD_IMR_ANC		0x00004000	/* Autonegotiation complete */
#define MTD_IMR_FBUSERR		0x00002000	/* Fatal bus error */
#define MTD_IMR_RSRVD1		0x00001800	/* Bits [12:11] are reserved */
#define MTD_IMR_TXUNDER		0x00000400	/* Transmit underflow */
#define MTD_IMR_RXOVER		0x00000200	/* Receive overflow */
#define MTD_IMR_TXEARLY		0x00000100	/* Transmit early int */
#define MTD_IMR_RXEARLY		0x00000080	/* Receive early int */
#define MTD_IMR_CTROVER		0x00000040	/* Counter overflow */
#define MTD_IMR_RXBUN		0x00000020	/* Receive buffer n/a */
#define MTD_IMR_TXBUN		0x00000010	/* Transmit buffer n/a */
#define MTD_IMR_TXIRQ		0x00000008	/* Transmit interrupt */
#define MTD_IMR_RXIRQ		0x00000004	/* Receive interrupt */
#define MTD_IMR_RXERR		0x00000002	/* Receive error */
#define MTD_IMR_RSRVD0		0x00000001	/* Bit 1 is reserved */

#define MTD_IMR_MASK		MTD_IMR_TXIRQ | MTD_IMR_RXIRQ | MTD_IMR_RXBUN \
				| MTD_IMR_RXERR | MTD_IMR_PDF \
				| MTD_IMR_FBUSERR | MTD_IMR_TXUNDER \
				| MTD_IMR_RXOVER \

/* Tally counters for CRC and MPA */
#define MTD_TALLY_CRCOVER	0x80000000	/* CRC tally ctr overflow */
#define MTD_TALLY_NCRCERR	0x7fff0000	/* Number of CRC errors */
#define MTD_TALLY_MPAOVER	0x00008000	/* MPA tally ctr overflow */
#define MTD_TALLY_NMPAERR	0x00007fff	/* Number of MPA errors */

/* Tally counters for Transmit Status Report */
#define MTD_TSR_NABORT		0xff000000	/* Number of aborted packets */
#define MTD_TSR_NLCOL		0x00ff0000	/* Number of late collisions */
#define MTD_TSR_NRETRY		0x0000ffff	/* Number of transm. retries */

/* Wake-Up Events Control and Status Register */
#define MTD_WUECSR_RSRVD1	0xfffff000	/* Bits [31:12] are reserved */
#define MTD_WUECSR_FRCWKUP	0x00000800	/* Force Wake Up LAN mode */
#define MTD_WUECSR_STATCHG	0x00000400	/* Status Change enable */
#define MTD_WUECSR_AGU		0x00000200	/* Accept Global Unicast */
#define MTD_WUECSR_WUPOP	0x00000100	/* Wake Up Pin Output Pattern */
#define MTD_WUECSR_WUPPROP	0x00000080	/* Wake Up Pin Property */
#define MTD_WUECSR_LCD		0x00000040	/* Link Change Detected */
#define MTD_WUECSR_MPR		0x00000020	/* Magic Packet Received */
#define MTD_WUECSR_WUFR		0x00000010	/* Wake Up Frame Received */
#define MTD_WUECSR_RSRVD0	0x00000008	/* Unspecified! */
#define MTD_WUECSR_LCE		0x00000004	/* Link Change Enable */
#define MTD_WUECSR_MPE		0x00000002	/* Magic Packet Enable */
#define MTD_WUECSR_WUFE		0x00000001	/* Wake Up Frame Enable */


/*
 * Note: We should probably move the following info to a new PHY driver.
 * Or maybe remove them anyway, but we might need them someday so leave them
 *  here for now.
 */
/* PHY Control and Status Register */
#define MTD_PHY_T4		0x80000000	/* T4 operation capability */
#define MTD_PHY_TXFD		0x40000000	/* 100-TX Full Duplex cap. */
#define MTD_PHY_TXHD		0x20000000	/* 100-TX Half Duplex cap. */
#define MTD_PHY_TPFD		0x10000000	/* 10-TP Full Duplex cap. */
#define MTD_PHY_TPHD		0x08000000	/* 10-TP Half Duplex cap. */
#define MTD_PHY_RSRVD2		0x07c00000	/* Bits [16:22] are reserved */
#define MTD_PHY_ANC		0x00200000	/* Autonegotiation complete */
#define MTD_PHY_RMTFAULT	0x00100000	/* Remote fault */
#define MTD_PHY_AUTONEG		0x00080000	/* Autonegotiation */
#define MTD_PHY_LINK		0x00040000	/* Link status */
#define MTD_PHY_JABBER		0x00020000	/* Jabber detected */
#define MTD_PHY_EXTREG		0x00010000	/* Extended register exists */
#define MTD_PHY_RESET		0x00008000	/* Reset PHY registers */
#define MTD_PHY_LPBACK		0x00004000	/* Loopback select */
#define MTD_PHY_SPEED		0x00002000	/* Speed select */
#define MTD_PHY_ANEN		0x00001000	/* Autoneg enable */
#define MTD_PHY_POWDWN		0x00000800	/* Power-down */
#define MTD_PHY_RSRVD1		0x00000400	/* Bit 10 is reserved */
#define MTD_PHY_RESTAN		0x00000200	/* Restart Autoneg */
#define MTD_PHY_DUPLEX		0x00000100	/* Duplex select */
#define MTD_PHY_COLTST		0x00000080	/* Collision test enable */
#define MTD_PHY_RSRVD0		0x0000007f	/* Bits [6:0] are reserved */

/* OUI register */
#define MTD_OUI_HIGH		0xfc000000	/* OUI High register (0x34) */
#define MTD_OUI_PARTNO		0x02f00000	/* Part number (0x0) */
#define MTD_OUI_REVISION	0x000f0000	/* Revision number (0x0) */
#define MTD_OUI_LOW		0x0000ffff	/* OUI Low register (0x0302) */

/* Link Partner Ability Register and Advertisment Register */
#define MTD_LPAR_LP_NEXTPAGE	0x80000000	/* Next page */
#define MTD_LPAR_LP_ACK		0x40000000	/* Acknowledge */
#define MTD_LPAR_LP_RMTFAULT	0x20000000	/* Remote fault detected */
#define MTD_LPAR_RSRVD1		0x1c000000	/* Bits [28:26] are reserved */
#define MTD_LPAR_LP_T4		0x02000000	/* Capable of T4 operation */
#define MTD_LPAR_LP_TXFD	0x01000000	/* Cap. of 100-TX Full Duplex */
#define MTD_LPAR_LP_TXHD	0x00800000	/* Cap. of 100-TX Half Duplex */
#define MTD_LPAR_LP_TPFD	0x00400000	/* Cap. of 10-TP Full Duplex */
#define MTD_LPAR_LP_TPHD	0x00200000	/* Cap. of 10-TP Half Duplex */
#define MTD_LPAR_SELECTOR1	0x001f0000	/* Selector field 1 */
#define MTD_LPAR_AD_NEXTPAGE	0x00008000	/* Next page */
#define MTD_LPAR_AD_ACK		0x00004000	/* Acknowledge */
#define MTD_LPAR_AD_RMTFAULT	0x00002000	/* Remote fault detected */
#define MTD_LPAR_RSRVD0		0x00001c00	/* Bits [12:10] are reserved */
#define MTD_LPAR_AD_T4		0x00000200	/* Capable of T4 operation */
#define MTD_LPAR_AD_TXFD	0x00000100	/* Cap. of 100-TX Full Duplex */
#define MTD_LPAR_AD_TXHD	0x00000080	/* Cap. of 100-TX Half Duplex */
#define MTD_LPAR_AD_TPFD	0x00000040	/* Cap. of 10-TP Full Duplex */
#define MTD_LPAR_AD_TPHD	0x00000020	/* Cap. of 10-TP Half Duplex */
#define MTD_LPAR_SELECTOR0	0x0000001f	/* Selector field 0 */

#endif	/* __DEV_IC_MTD803REG_H__ */
