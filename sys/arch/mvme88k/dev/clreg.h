/*	$OpenBSD: clreg.h,v 1.6 2004/04/24 19:51:47 miod Exp $ */

/* Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1995 Dale Rahn. All rights reserved.
 *
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

#define	CL_COR7		0x0007
#define	CL_LIVR		0x0009
#define	CL_COR1		0x0010
#define	CL_IER		0x0011
#define	CL_STCR		0x0012
#define	CL_CCR		0x0013
#define	CL_COR5		0x0014
#define	CL_COR4		0x0015
#define	CL_COR3		0x0016
#define	CL_COR2		0x0017
#define	CL_COR6		0x0018
#define	CL_DMABSTS	0x0019
#define	CL_CSR		0x001a
#define	CL_CMR		0x001b
#define	CL_SCHR4	0x001c
#define	CL_SCHR3	0x001d
#define	CL_SCHR2	0x001e
#define	CL_SCHR1	0x001f
#define	CL_SCRH		0x0022
#define	CL_SCRL		0x0023

#define	CL_RTPR		0x0024
#define	CL_RTPRH	0x0024
#define	CL_RTPRL	0x0025

#define	CL_LICR		0x0026
#define	CL_LNXT		0x002e
#define	CL_RFOC		0x0030

#define	CL_TCBADRU	0x0038
#define	CL_TCBADRL	0x003a
#define	CL_RCBADRU	0x003c
#define	CL_RCBADRL	0x003e
#define	CL_ARBADRU	0x0040
#define	CL_ARBARDL	0x0042
#define	CL_BRBADRU	0x0044
#define	CL_BRBADRL	0x0046
#define	CL_BRBCNT	0x0048
#define	CL_ARBCNT	0x004a

#define	CL_BRBSTS	0x004e
#define	CL_ARBSTS	0x004f

#define	CL_ATBADR	0x0050
#define	CL_ATBADRU	0x0050
#define	CL_ATBADRL	0x0052
#define	CL_BTBADR	0x0054
#define	CL_BTBADRU	0x0054
#define	CL_BTBADRL	0x0056

#define	CL_BTBCNT	0x0058
#define	CL_ATBCNT	0x005a

#define	CL_BTBSTS	0x005e
#define	CL_ATBSTS	0x005f

#define	CL_TFTC		0x0080
#define	CL_GFRCR	0x0081
#define	CL_REOIR	0x0084
#define	CL_TEOIR	0x0085
#define	CL_MEOIR	0x0086

#define	CL_RISR		0x0088
#define	CL_RISRH	0x0088
#define	CL_RISRL	0x0089

#define	CL_TISR		0x008a
#define	CL_MISR		0x008b
#define	CL_BERCNT	0x008e
#define	CL_TCOR		0x00c0
#define	CL_TBPR		0x00c3
#define	CL_RCOR		0x00c8
#define	CL_RBPR		0x00cb
#define	CL_CPSR		0x00d6
#define	CL_TPR		0x00da
#define	CL_MSVR_RTS	0x00de
#define	CL_MSVR_DTR	0x00df
#define	CL_TPILR	0x00e0
#define	CL_RPILR	0x00e1
#define	CL_STK		0x00e2
#define	CL_MPILR	0x00e3
#define	CL_TIR		0x00ec
#define	CL_RIR		0x00ed
#define	CL_CAR		0x00ee
#define	CL_MIR		0x00ef
#define	CL_DMR		0x00f6
#define	CL_RDR		0x00f8
#define	CL_TDR		0x00f8

#define CD2400_SIZE		0x200

/*
 * Cirrus chip base address on the mvme1x7 boards.
 */
#define CD2400_BASE_ADDR	0xfff45000
#define CD2400_SECONDARY_ADDR	0xfff45200
