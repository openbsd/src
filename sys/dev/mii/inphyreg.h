/*	$OpenBSD: inphyreg.h,v 1.2 1999/06/22 16:12:05 jason Exp $	*/
/*	$NetBSD: inphyreg.h,v 1.1 1998/08/11 00:00:28 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#ifndef _DEV_MII_INPHYREG_H_
#define	_DEV_MII_INPHYREG_H_

/*
 * Intel 82555 registers.
 */

#define	MII_INPHY_SCR		0x10	/* Status and Control */
#define	SCR_FLOWCTL		0x8000	/* PHY Base flow control enabled */
#define	SCR_CSDC		0x2000	/* Carrier sense disconnect control */
#define	SCR_TFCD		0x1000	/* Transmit flow control disable */
#define	SCR_RDSI		0x0800	/* Receive deserializer in-sync */
#define	SCR_100TXPD		0x0400	/* 100baseTX is powered down */
#define	SCR_10TPD		0x0200	/* 10baseT is powered down */
#define	SCR_POLARITY		0x0100	/* reverse 10baseT polarity */
#define	SCR_T4			0x0004	/* autoneg resulted in 100baseT4 */
#define	SCR_S100		0x0002	/* autoneg resulted in 100baseTX */
#define	SCR_FDX			0x0001	/* autoneg resulted in full-duplex */

#define	MII_INPHY_SCTRL		0x11	/* Special Control Bit */
#define	SCTRL_SCRBYPASS		0x8000	/* scrambler bypass */
#define	SCTRL_4B5BNYPASS	0x4000	/* 4bit to 5bit bypass */
#define	SCTRL_FTHP		0x2000	/* force transmit H-pattern */
#define	SCTRL_F34TP		0x1000	/* force 34 transmit patter */
#define	SCTRL_GOODLINK		0x0800	/* 100baseTX link good */
#define	SCTRL_TCSD		0x0200	/* transmit carrier sense disable */
#define	SCTRL_DDPD		0x0100	/* disable dynamic power-down */
#define	SCTRL_ANEGLOOP		0x0080	/* autonegotiaion loopback */
#define	SCTRL_MDITRISTATE	0x0040	/* MDI Tri-state */
#define	SCTRL_FILTERBYPASS	0x0020	/* Filter bypass */
#define	SCTRL_AUTOPOLDIS	0x0010	/* auto-polarity disable */
#define	SCTRL_SQUELCHDIS	0x0008	/* squlch test disable */
#define	SCTRL_EXTSQUELCH	0x0004	/* extended sequelch enable */
#define	SCTRL_LINKINTDIS	0x0002	/* link integrity disable */
#define	SCTRL_JABBERDIS		0x0001	/* jabber disabled */

#define	MII_INPHY_100TXRDC	0x14	/* 100baseTX Receive Disconnect Cntr */

#define	MII_INPHY_100TXREFC	0x15	/* 100baseTX Receive Error Frame Ctr */

#define	MII_INPHY_RSEC		0x16	/* Receive Symbol Error Counter */

#define	MII_INPHY_100TXRPEOFC	0x17	/* 100baseTX Rcv Premature EOF Ctr */

#define	MII_INPHY_10TREOFC	0x18	/* 10baseT Rcv EOF Ctr */

#define	MII_INPHY_10TTJDC	0x19	/* 10baseT Tx Jabber Detect Ctr */

#define	MII_INPHY_SCTRL2	0x1b	/* 82555 Special Control */
#define	SCTRL2_LEDMASK		0x0007	/* mask of LEDs control: see below */

#define	LEDMASK_ACTLINK		0x0000	/* A = Activity, L = Link */
#define	LEDMASK_SPDCOLL		0x0001	/* A = Speed, L = Collision */
#define	LEDMASK_SPDLINK		0x0002	/* A = Speed, L = Link */
#define	LEDMASK_ACTCOLL		0x0003	/* A = Activity, L = Collision */
#define	LEDMASK_OFFOFF		0x0004	/* A = off, L = off */
#define	LEDMASK_OFFON		0x0005	/* A = off, L = on */
#define	LEDMASK_ONOFF		0x0006	/* A = on, L = off */
#define	LESMASK_ONON		0x0007	/* A = on, L = on */

#endif /* _DEV_MII_INPHYREG_H_ */
