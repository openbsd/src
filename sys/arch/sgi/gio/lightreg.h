/*	$OpenBSD: lightreg.h,v 1.2 2014/03/18 23:23:09 miod Exp $	*/
/*	$NetBSD: lightreg.h,v 1.3 2006/12/29 00:31:48 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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

#define LIGHT_ADDR_0		0x1f3f0000
#define LIGHT_ADDR_1		0x1f3f8000	/* dual head */
#define	LIGHT_SIZE		0x00008000

#define REX_PAGE0_SET		0x00000000	/* REX registers */
#define REX_PAGE0_GO		0x00000800
#define REX_PAGE1_SET		0x00004790	/* configuration registers */
#define REX_PAGE1_GO		0x00004F90

/* REX register offsets (from REX_PAGE0_{SET,GO}) */
#define REX_P0REG_COMMAND	0x00000000
#define REX_P0REG_XSTARTI	0x0000000C
#define REX_P0REG_YSTARTI	0x0000001C
#define REX_P0REG_XYMOVE	0x00000034
#define REX_P0REG_COLORREDI	0x00000038
#define REX_P0REG_COLORGREENI	0x00000040
#define REX_P0REG_COLORBLUEI	0x00000048
#define REX_P0REG_COLORBACK	0x0000005C
#define REX_P0REG_ZPATTERN	0x00000060
#define REX_P0REG_XENDI		0x00000084
#define REX_P0REG_YENDI		0x00000088

/* configuration register offsets (from REX_PAGE1_{SET,GO}) */
#define REX_P1REG_WCLOCKREV	0x00000054	/* nsclock / revision */
#define REX_P1REG_DAC_ADDRDATA	0x00000058	/* DAC r/w addr and data 8bit */
#define REX_P1REG_CFGSEL	0x0000005c	/* function selector */
#define REX_P1REG_VC1_ADDRDATA	0x00000060	/* vc1 r/w addr and data 8bit */
#define REX_P1REG_CFGMODE	0x00000068	/* REX system config */
#define REX_P1REG_XYOFFSET	0x0000006c	/* x, y start of screen */

/* REX opcodes */
#define REX_OP_NOP		0x00000000
#define REX_OP_DRAW		0x00000001

/* REX command flags */
#define REX_OP_FLG_BLOCK	0x00000008
#define REX_OP_FLG_LENGTH32	0x00000010
#define REX_OP_FLG_QUADMODE	0x00000020
#define REX_OP_FLG_XYCONTINUE	0x00000080
#define REX_OP_FLG_STOPONX	0x00000100
#define REX_OP_FLG_STOPONY	0x00000200
#define REX_OP_FLG_ENZPATTERN	0x00000400
#define REX_OP_FLG_LOGICSRC	0x00080000
#define REX_OP_FLG_ZOPAQUE	0x00800000
#define REX_OP_FLG_ZCONTINUE	0x01000000

/* REX logicops */
#define REX_LOGICOP_SHIFT	28

/* configmode bits */
#define REX_CFGMODE_BUSY	0x00000001

/* configsel bits */
#define REX_CFGSEL_VC1_LADDR	0x00000004	/* low address bits */
#define REX_CFGSEL_VC1_HADDR	0x00000005	/* high address bits */
#define REX_CFGSEL_VC1_SYSCTL	0x00000006
#define REX_CFGSEL_DAC_WADDR	0x00000000	/* write address */
#define REX_CFGSEL_DAC_CMAP	0x00000001	/* colormap data */
#define REX_CFGSEL_DAC_PMASK	0x00000002	/* pixel read mask */
#define REX_CFGSEL_DAC_RADDR	0x00000003	/* read address */
#define REX_CFGSEL_DAC_OVWADDR	0x00000004	/* overlay write address */
#define REX_CFGSEL_DAC_OV	0x00000005	/* overlay registers */
#define REX_CFGSEL_DAC_CTL	0x00000006	/* control registers */
#define REX_CFGSEL_DAC_OVRADDR	0x00000007	/* overlay read address */

/* vc1 sysctl bits (byte) */
#define VC1_SYSCTL_VIDEO_ON	0x04
#define VC1_SYSCTL_CURSOR	0x10
#define VC1_SYSCTL_CURSOR_ON	0x20
