/*	$OpenBSD: mtdphyreg.h,v 1.3 2003/06/02 19:08:58 jason Exp $	*/

/*
 * Copyright (c) 1998 Jason L. Wright (jason@thought.net)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_MII_MTDPHYREG_H_
#define	_DEV_MII_MTDPHYREG_H_

/*
 * Myson MTD972 registers.
 */

#define	MII_MTDPHY_ANNPTR	0x07	/* Auto-Neg Next Page Tx Register */
#define	ANNPTR_NEXT		0x8000	/* Next Page */
#define	ANNPTR_ACK		0x4000	/* Acknowledge */
#define	ANNPTR_MPG		0x2000	/* Message Page */
#define	ANNPTR_ACK2		0x1000	/* Acknowledge 2 */
#define	ANNPTR_TOGGLE		0x0800	/* Toggle */
#define	ANNPTR_DAT		0x0400	/* Data */

#define	MII_MTDPHY_DCR		0x12	/* Disconnect Register */
	/* count of the number of partitions */

#define	MII_MTDPHY_FCSCR	0x13	/* False Carrier Sense Counter Reg */
	/* count of the number of false carrier senses */

#define	MII_MTDPHY_RECR		0x15	/* Receive Error Condition Register */
	/* count of the number of receive errors */

#define	MII_MTDPHY_RR		0x16	/* Revision Register */
	/* revision of the MTD972 */

#define	MII_MTDPHY_PCR		0x17	/* PHY Configuration Register */
#define	PCR_NRZIEN		0x8000	/* NRZI Encode/Decode Enable */
#define	PCR_TOCEL		0x4000	/* Descrambler Time Out Select */
#define	PCR_TODIS		0x2000	/* Descrambler Time Out Disable */
#define	PCR_RPTR		0x1000	/* Enable Repeater mode */
#define	PCR_ENCSEL		0x0800	/* PMD ENCSEL pin control */
#define	PCR_20MENB		0x0100	/* 20MHz output enable */
#define	PCR_25MDIS		0x0080	/* 25MHz output disable */
#define	PCR_FGLNKTX		0x0040	/* Force link status good */
#define	PCR_FCONNT		0x0020	/* Bypass disconnect function */
#define	PCR_TXOFF		0x0010	/* Turn off TX */
#define	PCR_LEDTSL		0x0002	/* LED T display select */
#define	PCR_LEDPSL		0x0001	/* LED P display select */

#define	MII_MTDPHY_LBCR		0x18	/* Loopback and Bypass Control Reg */
#define	LBCR_BP4B5B		0x4000	/* Bypass */
#define	LBCR_BPSCRM		0x2000	/* Bypass */
#define	LBCR_BPALGN		0x1000	/* Bypass symbol alignment */
#define	LBCR_LBK10		0x0800	/* Loopback control for 10BT */
#define	LBCR_LBK1		0x0200	/* Loopback control 1 for PMD */
#define	LBCR_LBK0		0x0100	/* Loopback control 0 for PMD */
#define	LBCR_FDCRS		0x0040	/* Full duplex CRS function */
#define	LBCR_CERR		0x0010	/* Code Error */
#define	LBCR_PERR		0x0008	/* Premature Error */
#define	LBCR_LERR		0x0004	/* Link Error */
#define	LBCR_FERR		0x0002	/* Frame Error */

#define	MII_MTDPHY_PAR		0x19	/* PHY Address Register */
#define	PAR_ANS			0x0400	/* State of autonegotiation */
#define	PAR_FEFE		0x0100	/* Far End Fault Enable */
#define	PAR_DPLX		0x0080	/* Duplex status */
#define	PAR_SPD			0x0040	/* Speed status */
#define	PAR_CONN		0x0020	/* Connection status */
#define	PAR_PHYADDR_MASK	0x001f	/* PHY address mask */

#define	MII_MTDPHY_10SR		0x1b	/* 10baseT Status Register */
#define	TENSR_10BTSER		0x0200	/* Serial mode for 10BaseT interface */
#define	TENSR_POLST		0x0001	/* Polarity state */

#define	MII_MTDPHY_10CR		0x1c	/* 10baseT Control Register */
#define	TENCR_LOE		0x0020	/* Link Pulse Ouput Enable */
#define	TENCR_HBE		0x0010	/* HeartBeat Enable */
#define	TENCR_UTPV		0x0008	/* TPOV=1, TPOI=0 */
#define	TENCR_LSS		0x0004	/* Low Squelch Select */
#define	TENCR_PENB		0x0002	/* Parity Enable */
#define	TENCR_JEN		0x0001	/* Jabber Enable */

#endif /* _DEV_MII_MTDPHYREG_H_ */
