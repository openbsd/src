/*	$NetBSD$ */

/*
 * Copyright (c) 1995 Dale Rahn. All rights reserved.
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
 *   This product includes software developed by Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
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

/* 
 * Memory map for CL-CD2400 (CD2401)
 *	NOTE: intel addresses are different than motorola addresss
 *	 do we want both here (really is based on endian)
 *	 or do we want to put these in a carefully packed structure?
 */

/* these are mot addresses */
/* global registers */
#define CD2400_GFRCR		0x81
#define CD2400_CAR		0xee

/* option registers */
#define CD2400_CMR		0x1b
#define CD2400_COR1		0x10
#define CD2400_COR2		0x17
#define CD2400_COR3		0x16
#define CD2400_COR4		0x15
#define CD2400_COR5		0x14
#define CD2400_COR6		0x18
#define CD2400_COR7		0x07
#define CD2400_SCHR1		0x1f /* async */
#define CD2400_SCHR2		0x1e /* async */
#define CD2400_SCHR3		0x1d /* async */
#define CD2400_SCHR4		0x1c /* async */
#define CD2400_SCRl		0x23 /* async */
#define CD2400_SCRh		0x22 /* async */
#define CD2400_LNXT		0x2e
#define CD2400_RFAR1		0x1f /* sync */
#define CD2400_RFAR2		0x1e /* sync */
#define CD2400_RFAR3		0x1d /* sync */
#define CD2400_RFAR4		0x1c /* sync */

#define CD2400_CPSR		0xd6

/* bit rate and clock option registers */
#define CD2400_RBPR		0xcb
#define CD2400_RCOR		0xc8
#define CD2400_TBPR		0xc3
#define CD2400_TCOR		0xc0

/* channel command and status registers */
#define CD2400_CCR		0x13
#define CD2400_STCR		0x12 /* sync */
#define CD2400_CSR		0x1a
#define CD2400_MSVR_RTS		0xde
#define CD2400_MSVR_DTR		0xdf

/* interrupt registers */
#define CD2400_LIVR		0x09
#define CD2400_IER		0x11
#define CD2400_LICR		0x26
#define CD2400_STK		0xe2

/* receive interrupt registers */
#define CD2400_RPILR		0xe1
#define CD2400_RIR		0xeD
#define CD2400_RISR		0x88
#define CD2400_RISRl		0x89
#define CD2400_RISRh		0x88
#define CD2400_RFOC		0x30
#define CD2400_RDR		0xf8
#define CD2400_REOIR		0x84

/* transmit interrupt registers */
#define CD2400_TPILR		0xe0
#define CD2400_TIR		0xec
#define CD2400_TISR		0x8a
#define CD2400_TFTC		0x80
#define CD2400_TDR		0xf8
#define CD2400_TEOIR		0x85

/* modem interrrupt registers */
#define CD2400_MPILR		0xe3
#define CD2400_MIR		0xef
#define CD2400_MISR		0x8B
#define CD2400_MEOIR		0x86

/* dma registers */
#define CD2400_DMR		0xf6
#define CD2400_BERCNT		0x8e
#define CD2400_DMABSTS		0x19

/* dma receive registers - leave these long names, as in manual */
#define CD2400_ARBADRL		0x42
#define CD2400_ARBADRU		0x40
#define CD2400_BRBADRL		0x46
#define CD2400_BRBADRU		0x44
#define CD2400_ARBCNT		0x4a
#define CD2400_BRBCNT		0x48
#define CD2400_ARBSTS		0x4f
#define CD2400_BRBSTS		0x4e
#define CD2400_RCBADRL		0x3e
#define CD2400_RCBADRU		0x3c

/* dma transmit registers */
#define CD2400_ATBADRL		0x52
#define CD2400_ATBADRU		0x50
#define CD2400_BTBADRL		0x56
#define CD2400_BTBADRU		0x54
#define CD2400_ATBCNT		0x5a
#define CD2400_BTBCNT		0x58
#define CD2400_ATBSTS		0x5f
#define CD2400_BTBSTS		0x5e
#define CD2400_RTBADRL		0x3a
#define CD2400_RTBADRU		0x38

/* timer registers */
#define CD2400_TPR		0xda
#define CD2400_RTPR		0x24 /* async */
#define CD2400_RTPRl		0x25 /* async */
#define CD2400_RTPRh		0x24 /* async */
#define CD2400_GT1		0x2a /* sync */
#define CD2400_GT1l		0x2b /* sync */
#define CD2400_GT1h		0x2a /* sync */
#define CD2400_GT2		0x29 /* sync */
#define CD2400_TTR		0x29 /* async */


#define CD2400_SIZE		0x200
