/*	$OpenBSD: creatorreg.h,v 1.5 2002/07/29 06:21:45 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/* Number of register sets */
#define	FFB_NREGS		24

/* Register set numbers */
#define	FFB_REG_PROM		0
#define	FFB_REG_DAC		1
#define	FFB_REG_FBC		2
#define	FFB_REG_DFB8R		3
#define	FFB_REG_DFB8G		4
#define	FFB_REG_DFB8B		5
#define	FFB_REG_DFB8X		6
#define	FFB_REG_DFB24		7
#define	FFB_REG_DFB32		8
#define	FFB_REG_SFB8R		9
#define	FFB_REG_SFB8G		10
#define	FFB_REG_SFB8B		11
#define	FFB_REG_SFB8X		12
#define	FFB_REG_SFB32		13
#define	FFB_REG_SFB64		14
#define	FFB_REG_DFB422A		15

#define	FFB_FBC_ALPHA		0x00c
#define	FFB_FBC_RED		0x010
#define	FFB_FBC_GREEN		0x014
#define	FFB_FBC_BLUE		0x018
#define	FFB_FBC_DEPTH		0x01c
#define	FFB_FBC_Y		0x020
#define	FFB_FBC_X		0x024
#define	FFB_FBC_RYF		0x030
#define	FFB_FBC_RXF		0x034
#define	FFB_FBC_DMYF		0x040
#define	FFB_FBC_DMXF		0x044
#define	FFB_FBC_EBYI		0x050
#define	FFB_FBC_EBXI		0x054
#define	FFB_FBC_BY		0x060
#define	FFB_FBC_BX		0x064
#define	FFB_FBC_DY		0x068
#define	FFB_FBC_DX		0x06c
#define	FFB_FBC_BH		0x070
#define	FFB_FBC_BW		0x074
#define	FFB_FBC_SUVTX		0x100
#define	FFB_FBC_PPC		0x200	/* pixel processor control */
#define	FFB_FBC_WID		0x204
#define	FFB_FBC_FG		0x208
#define	FFB_FBC_BG		0x20c
#define	FFB_FBC_CONSTY		0x210
#define	FFB_FBC_CONSTZ		0x214
#define	FFB_FBC_XCLIP		0x218
#define	FFB_FBC_DCSS		0x21c
#define	FFB_FBC_VCLIPMIN	0x220
#define	FFB_FBC_VCLIPMAX	0x224
#define	FFB_FBC_VCLIPZMIN	0x228
#define	FFB_FBC_VCLIPZMAX	0x22c
#define	FFB_FBC_DCSF		0x230
#define	FFB_FBC_DCSB		0x234
#define	FFB_FBC_DCZF		0x238
#define	FFB_FBC_DCZB		0x23c
#define	FFB_FBC_BLENDC		0x244
#define	FFB_FBC_BLENDC1		0x248
#define	FFB_FBC_BLENDC2		0x24c
#define	FFB_FBC_FBRAMITC	0x250
#define	FFB_FBC_FBC		0x254	/* Frame Buffer Control	*/
#define	FFB_FBC_ROP		0x258	/* Raster OPeration */
#define	FFB_FBC_CMP		0x25c	/* Frame Buffer Compare */
#define	FFB_FBC_MATCHAB		0x260	/* Buffer AB Match Mask	*/
#define	FFB_FBC_MATCHC		0x264
#define	FFB_FBC_MAGNAB		0x268	/* Buffer AB Magnitude Mask */
#define	FFB_FBC_MAGNC		0x26c
#define	FFB_FBC_FBCFG0		0x270
#define	FFB_FBC_FBCFG1		0x274
#define	FFB_FBC_FBCFG2		0x278
#define	FFB_FBC_FBCFG3		0x27c
#define	FFB_FBC_PPCFG		0x280
#define	FFB_FBC_PICK		0x284
#define	FFB_FBC_FILLMODE	0x288
#define	FFB_FBC_FBRAMWAC	0x28c
#define	FFB_FBC_PMASK		0x290	/* RGB Plane Mask */
#define	FFB_FBC_XPMASK		0x294	/* X PlaneMask */
#define	FFB_FBC_YPMASK		0x298
#define	FFB_FBC_ZPMASK		0x29c
#define	FFB_FBC_CLIP0MIN	0x2a0
#define	FFB_FBC_CLIP0MAX	0x2a4
#define	FFB_FBC_CLIP1MIN	0x2a8
#define	FFB_FBC_CLIP1MAX	0x2ac
#define	FFB_FBC_CLIP2MIN	0x2b0
#define	FFB_FBC_CLIP2MAX	0x2b4
#define	FFB_FBC_CLIP3MIN	0x2b8
#define	FFB_FBC_CLIP3MAX	0x2bc
#define	FFB_FBC_RAWBLEND2	0x2c0
#define	FFB_FBC_RAWPREBLEND	0x2c4
#define	FFB_FBC_RAWSTENCIL	0x2c8
#define	FFB_FBC_RAWSTENCILCTL	0x2cc
#define	FFB_FBC_THREEDRAM1	0x2d0
#define	FFB_FBC_THREEDRAM2	0x2d4
#define	FFB_FBC_PASSIN		0x2d8
#define	FFB_FBC_RAWCLRDEPTH	0x2dc
#define	FFB_FBC_RAWPMASK	0x2e0
#define	FFB_FBC_RAWCSRC		0x2e4
#define	FFB_FBC_RAWMATCH	0x2e8
#define	FFB_FBC_RAWMAGN		0x2ec
#define	FFB_FBC_RAWROPBLEND	0x2f0
#define	FFB_FBC_RAWCMP		0x2f4
#define	FFB_FBC_RAWWAC		0x2f8
#define	FFB_FBC_FBRAMID		0x2fc
#define	FFB_FBC_DRAWOP		0x300	/* Draw OPeration */
#define	FFB_FBC_FONTLPAT	0x30c
#define	FFB_FBC_FONTXY		0x314
#define	FFB_FBC_FONTW		0x318	/* Font Width */
#define	FFB_FBC_FONTINC		0x31c	/* Font Increment */
#define	FFB_FBC_FONT		0x320
#define	FFB_FBC_BLEND2		0x330
#define	FFB_FBC_PREBLEND	0x334
#define	FFB_FBC_STENCIL		0x338
#define	FFB_FBC_STENCILCTL	0x33c
#define	FFB_FBC_DCSS1		0x350
#define	FFB_FBC_DCSS2		0x354
#define	FFB_FBC_DCSS3		0x358
#define	FFB_FBC_WIDPMASK	0x35c
#define	FFB_FBC_DCS2		0x360
#define	FFB_FBC_DCS3		0x364
#define	FFB_FBC_DCS4		0x368
#define	FFB_FBC_DCD2		0x370
#define	FFB_FBC_DCD3		0x374
#define	FFB_FBC_DCD4		0x378
#define	FFB_FBC_PATTERN		0x380
#define	FFB_FBC_DEVID		0x800
#define	FFB_FBC_UCSR		0x900	/* User Control & Status */
#define	FFB_FBC_MER		0x980

#define	FFB_FBC_WB_A		0x20000000
#define	FFB_FBC_WM_COMBINED	0x00080000
#define	FFB_FBC_RB_A		0x00004000
#define	FFB_FBC_SB_BOTH		0x00003000
#define	FFB_FBC_ZE_OFF		0x00000400
#define	FFB_FBC_YE_OFF		0x00000100
#define	FFB_FBC_XE_ON		0x00000080
#define	FFB_FBC_XE_OFF		0x00000040
#define	FFB_FBC_RGBE_ON		0x0000002a
#define	FFB_FBC_RGBE_MASK	0x0000003f

#define	FBC_PPC_FW_DIS		0x00800000	/* force wid disable */
#define	FBC_PPC_FW_ENA		0x00c00000	/* force wid enable */
#define	FBC_PPC_ACE_DIS		0x00040000	/* aux clip disable */
#define	FBC_PPC_ACE_AUXSUB	0x00080000	/* aux clip add */
#define	FBC_PPC_ACE_AUXADD	0x000c0000	/* aux clip subtract */
#define	FBC_PPC_DCE_DIS		0x00020000	/* depth cue disable */
#define	FBC_PPC_DCE_ENA		0x00020000	/* depth cue enable */
#define	FBC_PPC_ABE_DIS		0x00008000	/* alpha blend disable */
#define	FBC_PPC_ABE_ENA		0x0000c000	/* alpha blend enable */
#define	FBC_PPC_VCE_DIS		0x00001000	/* view clip disable */
#define	FBC_PPC_VCE_2D		0x00002000	/* view clip 2d */
#define	FBC_PPC_VCE_3D		0x00003000	/* view clip 3d */
#define	FBC_PPC_APE_DIS		0x00000800	/* area pattern disable */
#define	FBC_PPC_APE_ENA		0x00000c00	/* area pattern enable */
#define	FBC_PPC_TBE_OPAQUE	0x00000200	/* opaque background */
#define	FBC_PPC_TBE_TRANSPAR	0x00000300	/* transparent background */
#define	FBC_PPC_ZS_VAR		0x00000080	/* z source ??? */
#define	FBC_PPC_ZS_CONST	0x000000c0	/* z source ??? */
#define	FBC_PPC_YS_VAR		0x00000020	/* y source ??? */
#define	FBC_PPC_YS_CONST	0x00000030	/* y source ??? */
#define	FBC_PPC_XS_WID		0x00000004	/* x source ??? */
#define	FBC_PPC_XS_VAR		0x00000008	/* x source ??? */
#define	FBC_PPC_XS_CONST	0x0000000c	/* x source ??? */
#define	FBC_PPC_CS_VAR		0x00000002	/* color source ??? */
#define	FBC_PPC_CS_CONST	0x00000003	/* color source ??? */

#define	FBC_ROP_NEW		0x83
#define	FBC_ROP_OLD		0x85

#define	FBC_UCSR_FIFO_MASK	0x00000fff
#define	FBC_UCSR_FB_BUSY	0x01000000
#define	FBC_UCSR_RP_BUSY	0x02000000
#define	FBC_UCSR_READ_ERR	0x40000000
#define	FBC_UCSR_FIFO_OVFL	0x80000000

#define	FBC_DRAWOP_DOT		0x00
#define	FBC_DRAWOP_AADOT	0x01
#define	FBC_DRAWOP_BRLINECAP	0x02
#define	FBC_DRAWOP_BRLINEOPEN	0x03
#define	FBC_DRAWOP_DDLINE	0x04
#define	FBC_DRAWOP_AALINE	0x05
#define	FBC_DRAWOP_TRIANGLE	0x06
#define	FBC_DRAWOP_POLYGON	0x07
#define	FBC_DRAWOP_RECTANGLE	0x08
#define	FBC_DRAWOP_FASTFILL	0x09
#define	FBC_DRAWOP_BCOPY	0x0a	/* block copy: not implemented */
#define	FBC_DRAWOP_VSCROLL	0x0b	/* vertical scroll */
