/*	$NetBSD: if_gmreg.h,v 1.1 2000/02/27 18:00:55 tsubai Exp $	*/

/*-
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct gmac_dma {
	u_int32_t cmd;
	u_int32_t cmd_hi;
	u_int32_t address;
	u_int32_t address_hi;
};

#define GMAC_OWN	0x80000000
#define GMAC_SOP	0x40000000	/* start of packet? */
#define GMAC_LEN_MASK	0x00003fff

#define GMAC_INT_TXDONE	0x04
#define GMAC_INT_RXDONE	0x10

#define GMAC_RXMAC_PR	0x08

/*
 * register offset
 */
#define GMAC_STATUS		0x000c
#define GMAC_INTMASK		0x0010
#define GMAC_SOFTWARERESET	0x1010

#define GMAC_TXDMAKICK		0x2000
#define GMAC_TXDMACONFIG	0x2004
#define GMAC_TXDMADESCBASELO	0x2008
#define GMAC_TXDMADESCBASEHI	0x200c
#define GMAC_TXDMACOMPLETE	0x2100

#define GMAC_RXDMACONFIG	0x4000
#define GMAC_RXDMADESCBASELO	0x4004
#define GMAC_RXDMADESCBASEHI	0x4008
#define GMAC_RXDMAKICK		0x4100

#define GMAC_MACPAUSE		0x6008
#define GMAC_MACPAUSE		0x6008
#define GMAC_TXMACSTATUS	0x6010
#define GMAC_TXMACCONFIG	0x6030
#define GMAC_RXMACCONFIG	0x6034
#define GMAC_MACCTRLCONFIG	0x6038
#define GMAC_XIFCONFIG		0x603c
#define GMAC_INTERPACKETGAP0	0x6040
#define GMAC_INTERPACKETGAP1	0x6044
#define GMAC_INTERPACKETGAP2	0x6048
#define GMAC_SLOTTIME		0x604c
#define GMAC_MINFRAMESIZE	0x6050
#define GMAC_MAXFRAMESIZE	0x6054
#define GMAC_PASIZE		0x6058
#define GMAC_JAMSIZE		0x605c
#define GMAC_ATTEMPTLIMIT	0x6060		/* atemptlimit */
#define GMAC_MACCNTLTYPE	0x6064
#define GMAC_MACADDRESS0	0x6080
#define GMAC_MACADDRESS1	0x6084
#define GMAC_MACADDRESS2	0x6088
#define GMAC_MACADDRESS3	0x608c
#define GMAC_MACADDRESS4	0x6090
#define GMAC_MACADDRESS5	0x6094
#define GMAC_MACADDRESS6	0x6098
#define GMAC_MACADDRESS7	0x609c
#define GMAC_MACADDRESS8	0x60a0
#define GMAC_MACADDRFILT0	0x60a4
#define GMAC_MACADDRFILT1	0x60a8
#define GMAC_MACADDRFILT2	0x60ac
#define GMAC_MACADDRFILT2_1MASK	0x60b0		/* macaddressfilter2&1mask */
#define GMAC_MACADDRFILT0MASK	0x60b4		/* macaddressfilter0mask */
#define GMAC_HASHTABLE0		0x60c0

#define GMAC_RANDOMSEED		0x6130
#define GMAC_MIFFRAMEOUTPUT	0x620c
#define GMAC_DATAPATHMODE	0x9050

#ifndef ETHER_MAX_LEN
#define ETHER_MAX_LEN           1518
#endif
#ifndef ETHER_MIN_LEN
#define ETHER_MIN_LEN           64
#endif

