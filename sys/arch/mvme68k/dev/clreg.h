/*	$Id: clreg.h,v 1.2 1995/11/07 08:48:54 deraadt Exp $ */

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
 *	This product includes software developed by Dale Rahn.
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

struct clreg {
	u_char	anon1[0x7];
	u_char	cl_cor7;			/* 0x07 */
	u_char	anon2[0x1];
	u_char	cl_livr;			/* 0x09 */
	u_char  anon3[0x6];
	u_char	cl_cor1;			/* 0x10 */
	u_char	cl_ier;				/* 0x11 */
	u_char  cl_stcr;			/* 0x12 */
	u_char	cl_ccr;				/* 0x13 */
	u_char	cl_cor5;			/* 0x14 */
	u_char	cl_cor4;			/* 0x15 */
	u_char	cl_cor3;			/* 0x16 */
	u_char	cl_cor2;			/* 0x17 */
	u_char	cl_cor6;			/* 0x18 */
	u_char	cl_dmabsts;			/* 0x19 */
	u_char	cl_csr;				/* 0x1a */
	u_char	cl_cmr;				/* 0x1b */
	u_char  cl_schr4;			/* 0x1c */
	u_char  cl_schr3;			/* 0x1d */
	u_char  cl_schr2;			/* 0x1e */
	u_char  cl_schr1;			/* 0x1f */
	u_char	anon5[0x2];
	u_char  cl_scrh;			/* 0x22 */
	u_char  cl_scrl;			/* 0x23 */
#define cl_rtpr rtpr.rtpr_rtpr
#define cl_rtprh rtpr.hl.rtpr_rtprh
#define cl_rtprl rtpr.hl.rtpr_rtprl
	union {
		u_short	rtpr_rtpr;		/* 0x24 */
		struct {
			u_char	rtpr_rtprh;	/* 0x24 */
			u_char	rtpr_rtprl;	/* 0x25 */
		} hl;
	} rtpr;
	u_char	cl_licr;			/* 0x26 */
	u_char	anon6[0x7];
	u_char	cl_lnxt;			/* 0x2e */
	u_char	anon7[0x1];
	u_char	cl_rfoc;			/* 0x30 */
	u_char	anon8[0x7];
	u_char	cl_rtbadru;			/* 0x38 */
	u_char	anon9[0x1];
	u_char	cl_rtbadrl;			/* 0x3a */
	u_char	anona[0x1];
	u_char	cl_rcbadru;			/* 0x3c */
	u_char	anonb[0x1];
	u_char	cl_rcbadrl;			/* 0x3e */
	u_char	anonc[0x1];
	u_char	cl_arbadru;			/* 0x40 */
	u_char	anond[0x1];
	u_char	cl_arbadrl;			/* 0x42 */
	u_char	anone[0x1];
	u_char	cl_brbadru;			/* 0x44 */
	u_char	anonf[0x1];
	u_char	cl_brbadrl;			/* 0x46 */
	u_char	anong[0x1];
	u_char	cl_brbcnt;			/* 0x48 */
	u_char	anonh[0x1];
	u_char	cl_arbcnt;			/* 0x4a */
	u_char	anoni[0x3];
	u_char	cl_brbsts;			/* 0x4e */
	u_char	cl_arbsts;			/* 0x4f */
	u_char	cl_atbadru;			/* 0x50 */
	u_char	anonj[0x1];
	u_char	cl_atbadrl;			/* 0x52 */
	u_char	anonk[0x1];
	u_char	cl_btbadru;			/* 0x54 */
	u_char	anonl[0x1];
	u_char	cl_btbadrl;			/* 0x56 */
	u_char	anonm[0x1];
	u_char	cl_btbcnt;			/* 0x58 */
	u_char	anonn[0x1];
	u_char	cl_atbcnt;			/* 0x5a */
	u_char	anono[0x3];
	u_char	cl_btbsts;			/* 0x5e */
	u_char	cl_atbsts;			/* 0x5f */
	u_char	anonp[0x20];
	u_char	cl_tftc;			/* 0x80 */
	u_char	cl_gfrcr;			/* 0x81 */
	u_char	anonq[0x2];
	u_char	cl_reoir;			/* 0x84 */
	u_char	cl_teoir;			/* 0x85 */
	u_char	cl_meoir;			/* 0x86 */
	u_char	anonr[0x1];
#define cl_risr risr.risr_risr
#define cl_risrl risr.hl.risr_risrl
#define cl_risrh risr.hl.risr_risrh
	union {
		u_short	risr_risr;		/* 0x88 */
		struct {
			u_char	risr_risrh;	/* 0x88 */
			u_char	risr_risrl;	/* 0x89 */
		} hl;
	} risr;
	u_char	cl_tisr;			/* 0x8a */
	u_char	cl_misr;			/* 0x8b */
	u_char	anons[0x2];
	u_char	cl_bercnt;			/* 0x8e */
	u_char	anont[0x31];
	u_char	cl_tcor;			/* 0xc0 */
	u_char	anonu[0x2];
	u_char	cl_tbpr;			/* 0xc3 */
	u_char	anonv[0x4];
	u_char	cl_rcor;			/* 0xc8 */
	u_char	anonw[0x2];
	u_char	cl_rbpr;			/* 0xcb */
	u_char	anonx[0xa];
	u_char	cl_cpsr;			/* 0xd6 */
	u_char	anony[0x3];
	u_char	cl_tpr;				/* 0xda */
	u_char	anonz[0x3];
	u_char	cl_msvr_rts;			/* 0xde */
	u_char	cl_msvr_dtr;			/* 0xdf */
	u_char	cl_tpilr;			/* 0xe0 */
	u_char	cl_rpilr;			/* 0xe1 */
	u_char	cl_stk;				/* 0xe2 */
	u_char	cl_mpilr;			/* 0xe3 */
	u_char	anonA[0x8];
	u_char	cl_tir;				/* 0xec */
	u_char	cl_rir;				/* 0xed */
	u_char	cl_car;				/* 0xee */
	u_char	cl_mir;				/* 0xef */
	u_char	anonB[0x6];
	u_char	cl_dmr;				/* 0xf6 */
	u_char	anonC[0x1];
#define cl_rdr cl_tdr
	u_char	cl_tdr;				/* 0xf8 */
	u_char	anonD[7];
};						/* 0x200 total */
