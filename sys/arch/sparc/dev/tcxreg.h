/*	$OpenBSD: tcxreg.h,v 1.3 2002/10/01 20:55:14 miod Exp $	*/
/*	$NetBSD: tcxreg.h,v 1.1 1996/06/19 13:17:35 pk Exp $ */

/*
 *  Copyright (c) 1996 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Paul Kranenburg.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. All advertising materials mentioning features or use of this software
 *     must display the following acknowledgement:
 *         This product includes software developed by the NetBSD
 *         Foundation, Inc. and its contributors.
 *  4. Neither the name of The NetBSD Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * User flags for tcx.
 */
#define	TCX_INTR		0x00000002	/* use retrace interrupt */

/*
 * A TCX is composed of numerous groups of control registers, all with TLAs:
 *	DHC - ???
 *	TEC - transform engine control?
 *	THC - TEC Hardware Configuration
 *	ROM - a 128Kbyte ROM with who knows what in it.
 *	STIP - ???
 *	RSTIP - Raw ???
 *	BLIT - ???
 *	RBLIT - Raw ???
 *	ALT - ???
 *	colormap - see below
 *	frame buffer memory (video RAM)
 *	possible other stuff
 *
 */
#define TCX_REG_DFB8	0
#define TCX_REG_DFB24	1
#define TCX_REG_STIP	2
#define TCX_REG_BLIT	3
#define TCX_REG_RDFB32	4
#define TCX_REG_RSTIP	5
#define TCX_REG_RBLIT	6
#define TCX_REG_TEC	7
#define TCX_REG_CMAP	8
#define TCX_REG_THC	9
#define TCX_REG_ROM	10
#define TCX_REG_DHC	11
#define TCX_REG_ALT	12

#define TCX_NREG	13


/*
 * The layout of the THC.
 */
struct tcx_thc {
	u_int32_t	thc_config;
	u_int32_t	thc_xxx1[31];
	u_int32_t	thc_sensebus;
	u_int32_t	thc_xxx2[3];
	u_int32_t	thc_delay;
	u_int32_t	thc_strapping;
	u_int32_t	thc_xxx3[1];
	u_int32_t	thc_linecount;
	u_int32_t	thc_xxx4[478];
	u_int32_t	thc_hcmisc;
	u_int32_t	thc_xxx5[56];
	u_int32_t	thc_cursoraddr;
	u_int32_t	thc_cursorAdata[32];
	u_int32_t	thc_cursorBdata[32];

};

/* cursor x/y position for 'off' */
#define	THC_CURSOFF		((65536-32) | ((65536-32) << 16))

/* bits in thc_config ??? */
#define THC_CFG_FBID		0xf0000000	/* id mask */
#define THC_CFG_FBID_SHIFT	28
#define THC_CFG_SENSE		0x07000000	/* sense mask */
#define THC_CFG_SENSE_SHIFT	24
#define THC_CFG_REV		0x00f00000	/* revision mask */
#define THC_CFG_REV_SHIFT	20
#define THC_CFG_RST		0x00008000	/* reset */

/* bits in thc_hcmisc */
#define	THC_MISC_OPENFLG	0x80000000	/* open flag (what's that?) */
#define	THC_MISC_SWERR_EN	0x20000000	/* enable SW error interrupt */
#define	THC_MISC_VSYNC_LEVEL	0x08000000	/* vsync level when disabled */
#define	THC_MISC_HSYNC_LEVEL	0x04000000	/* hsync level when disabled */
#define	THC_MISC_VSYNC_DISABLE	0x02000000	/* vsync disable */
#define	THC_MISC_HSYNC_DISABLE	0x01000000	/* hsync disable */
#define	THC_MISC_RESET		0x00001000	/* reset */
#define	THC_MISC_VIDEN		0x00000400	/* video enable */
#define	THC_MISC_SYNC		0x00000200	/* not sure what ... */
#define	THC_MISC_VSYNC		0x00000100	/* ... these really are */
#define	THC_MISC_SYNCEN		0x00000080	/* sync enable */
#define	THC_MISC_CURSRES	0x00000040	/* cursor resolution */
#define	THC_MISC_INTEN		0x00000020	/* v.retrace intr enable */
#define	THC_MISC_INTR		0x00000010	/* intr pending / ack */
#define	THC_MISC_DACWAIT	0x0000000f	/* cycles before transfer */

/*
 * Partial description of TEC.
 */
struct tcx_tec {
	u_int32_t	tec_config;	/* what's in it? */
	u_int32_t	tec_xxx0[35];
	u_int32_t	tec_delay;	/* */
#define TEC_DELAY_SYNC		0x00000f00
#define TEC_DELAY_WR_F		0x000000c0	/* wr falling */
#define TEC_DELAY_WR_R		0x00000030	/* wr rising */
#define TEC_DELAY_SOE_F		0x0000000c	/* soe falling */
#define TEC_DELAY_SOE_S		0x00000003	/* soe sclk */
	u_int32_t	tec_strapping;	/* */
#define TEC_STRAP_FIFO_LIMIT	0x00f00000
#define TEC_STRAP_CACHE_EN	0x00010000
#define TEC_STRAP_ZERO_OFFSET	0x00008000
#define TEC_STRAP_REFRSH_DIS	0x00004000
#define TEC_STRAP_REF_LOAD	0x00001000
#define TEC_STRAP_REFRSH_PERIOD	0x000003ff
	u_int32_t	tec_hcmisc;	/* */
	u_int32_t	tec_linecount;	/* */
	u_int32_t	tec_hss;	/* */
	u_int32_t	tec_hse;	/* */
	u_int32_t	tec_hds;	/* */
	u_int32_t	tec_hsedvs;	/* */
	u_int32_t	tec_hde;	/* */
	u_int32_t	tec_vss;	/* */
	u_int32_t	tec_vse;	/* */
	u_int32_t	tec_vds;	/* */
	u_int32_t	tec_vde;	/* */
};
