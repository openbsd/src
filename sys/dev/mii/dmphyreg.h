/*	$NetBSD: dmphyreg.h,v 1.1 2000/02/02 04:29:49 thorpej Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#ifndef _DEV_MII_DMPHYREG_H_
#define	_DEV_MII_DMPHYREG_H_

/*
 * Davicom DM9101 registers.
 */

#define	MII_DMPHY_DSCR		0x10	/* DAVICOM Specified Config Reg */
#define	DSCR_BP_4B5B		0x8000	/* bypass 4b5b encoding/decoding */
#define	DSCR_SCR		0x4000	/* bypass scrambler/descrambler */
#define	DSCR_ALIGN		0x2000	/* bypass symbol alignment */
#define	DSCR_REPEATER		0x0800	/* repeater mode */
#define	DSCR_TX			0x0400	/* 1 == 100baseTX, 0 == 100baseFX */
#define	DSCR_UTP		0x0200	/* 1 == UTP, 0 == STP */
#define	DSCR_CLK25MDIS		0x0100	/* CLK25M disable */
#define	DSCR_F_LINK_100		0x0080	/* force good link in 100Mb/s mode */
#define	DSCR_LINKLED_CTL	0x0020	/* 1 == link only, 0 == link+traffic */
#define	DSCR_FDXLEN_MODE	0x0010	/* 1 == 10baseT polarity, 0 == FDX */
#define	DSCR_SMRST		0x0008	/* reset state machine */
#define	DSCR_MFPSC		0x0004	/* MF preamble suppression */
#define	DSCR_SLEEP		0x0002	/* sleep mode */
#define	DSCR_RLOUT		0x0001	/* remote loop-out control */

#define	MII_DMPHY_DSCSR		0x11	/* DAVICOM Spec'd Conf/Stat Reg */
#define	DSCSR_100FDX		0x8000	/* 100Mb/s FDX */
#define	DSCSR_100HDX		0x4000	/* 100Mb/s HDX */
#define	DSCSR_10FDX		0x2000	/* 10Mb/s FDX */
#define	DSCSR_10HDX		0x1000	/* 10Mb/s HDX */
#define	DSCSR_PHYAD		0x01f0	/* PHY address */
#define	DSCSR_ANMB		0x000f	/* Autonegotiation monitor */

#define	ANMB_IDLE		0x0000	/* idle */
#define	ANMB_AB_MATCH		0x0001	/* ability match */
#define	ANMB_ACK_MATCH		0x0002	/* acknowledge match */
#define	ANMB_ACK_MATCH_FAIL	0x0003	/* acknowledge match fail */
#define	ANMB_CON_MATCH		0x0004	/* consistency match */
#define	ANMB_CON_MATCH_FAIL	0x0005	/* consistency match fail */
#define	ANMB_PAR_LINK		0x0006	/* par detect signal link ready */
#define	ANMB_PAR_LINK_FAIL	0x0007	/* par detect signal link ready fail */
#define	ANMB_ANEG_OK		0x0008	/* autonegotiation completed ok */

#define	MII_DMPHY_BTCSR		0x12	/* 10baseT Configuration/Status */
#define	BTCSR_LP_EN		0x4000	/* link pulse enable */
#define	BTCSR_HBE		0x2000	/* heartbeat enable */
#define	BTCSR_JABEN		0x0800	/* jabber enable */
#define	BTCSR_10BT_SER		0x0400	/* 10baseT serial mode */
#define	BTCSR_POLR		0x0001	/* polarity reversed */

#endif /* _DEV_MII_DMPHYREG_H_ */
