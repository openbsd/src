/*	$OpenBSD: txphyreg.h,v 1.3 2003/10/22 09:39:29 jmc Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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

#ifndef _DEV_MII_TXPHYREG_H_
#define	_DEV_MII_TXPHYREG_H_

/*
 * Texas Instruments TNETE2101 registers.
 */
#define	MII_TXPHY_ID		0x10	/* PHY identifier */

#define	MII_TXPHY_CTL		0x11	/* PHY control */
#define	TXCTL_IGLINK		0x8000	/* ignore link */
#define	TXCTL_SWAPOL		0x4000	/* swap polarity */
#define	TXCTL_MANCONF		0x2000	/* manual configuration */
#define	TXCTL_SQEEN		0x1000	/* SQE enable */
#define	TXCTL_MTEST		0x0800	/* manufacturing test */
#define	TXCTL_FIBER		0x0400	/* 100BaseFX mode */
#define	TXCTL_FEFEN		0x0200	/* far-end fault indication enable */
#define	TXCTL_NOENDEC		0x0100	/* no encode/decode */
#define	TXCTL_NOALIGN		0x0080	/* no symbol alignment */
#define	TXCTL_DUPONLY		0x0040	/* duplex LED only */
#define	TXCTL_REPEATER		0x0020	/* repeater mode enable */
#define	TXCTL_RXRESET		0x0010	/* 100baseTX rx reset */
#define	TXCTL_NOLINKP		0x0008	/* disable link pulse tx */
#define	TXCTL_NFEW		0x0004	/* no far end wrap */
#define	TXCTL_INTEN		0x0002	/* interrupt enable */
#define	TXCTL_TINT		0x0001	/* test interrupt */

#define	MII_TXPHY_STS		0x12	/* PHY status */
#define	TXSTS_MINT		0x8000	/* mii interrupt */
#define	TXSTS_PHOK		0x4000	/* power high ok */
#define	TXSTS_PLOK		0x2000	/* polarity ok */
#define	TXSTS_TPENERGY		0x1000	/* TP energy detect */
#define	TXSTS_SYNCLOSS		0x0800	/* 100btx rx descrambler sync loss */
#define	TXSTS_FEFI		0x0400	/* Far-end fault indication */

#endif /* _DEV_MII_MTDPHYREG_H_ */
