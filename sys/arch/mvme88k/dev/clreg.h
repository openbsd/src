/*	$OpenBSD: clreg.h,v 1.2 1998/12/15 05:52:30 smurph Exp $ */
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

struct clreg {
	volatile u_char anon1[0x7];
	volatile u_char cl_cor7;			/* 0x07 */
	volatile u_char anon2[0x1];
	volatile u_char cl_livr;			/* 0x09 */
	volatile u_char  anon3[0x6];
	volatile u_char cl_cor1;			/* 0x10 */
	volatile u_char cl_ier;				/* 0x11 */
	volatile u_char cl_stcr;			/* 0x12 */
	volatile u_char cl_ccr;				/* 0x13 */
	volatile u_char cl_cor5;			/* 0x14 */
	volatile u_char cl_cor4;			/* 0x15 */
	volatile u_char cl_cor3;			/* 0x16 */
	volatile u_char cl_cor2;			/* 0x17 */
	volatile u_char cl_cor6;			/* 0x18 */
	volatile u_char cl_dmabsts;			/* 0x19 */
	volatile u_char cl_csr;				/* 0x1a */
	volatile u_char cl_cmr;				/* 0x1b */
	volatile u_char cl_schr4;			/* 0x1c */
	volatile u_char cl_schr3;			/* 0x1d */
	volatile u_char cl_schr2;			/* 0x1e */
	volatile u_char cl_schr1;			/* 0x1f */
	volatile u_char anon5[0x2];
	volatile u_char cl_scrh;			/* 0x22 */
	volatile u_char cl_scrl;			/* 0x23 */
#define cl_rtpr rtpr.rtpr_rtpr
#define cl_rtprh rtpr.hl.rtpr_rtprh
#define cl_rtprl rtpr.hl.rtpr_rtprl
	union {
		volatile u_short rtpr_rtpr;		/* 0x24 */
		struct {
			volatile u_char rtpr_rtprh;	/* 0x24 */
			volatile u_char rtpr_rtprl;	/* 0x25 */
		}hl;
	}rtpr;
	volatile u_char cl_licr;			/* 0x26 */
	volatile u_char anon6[0x7];
	volatile u_char cl_lnxt;			/* 0x2e */
	volatile u_char anon7[0x1];
	volatile u_char cl_rfoc;			/* 0x30 */
	volatile u_char anon8[0x7];
	volatile u_short cl_tcbadru;			/* 0x38 */
	volatile u_short cl_tcbadrl;			/* 0x3a */
	volatile u_short cl_rcbadru;			/* 0x3c */
	volatile u_short cl_rcbadrl;			/* 0x3e */
	volatile u_short cl_arbadru;			/* 0x40 */
	volatile u_short cl_arbadrl;			/* 0x42 */
	volatile u_short cl_brbadru;			/* 0x44 */
	volatile u_short cl_brbadrl;			/* 0x46 */
	volatile u_short cl_brbcnt;			/* 0x48 */
	volatile u_short cl_arbcnt;			/* 0x4a */
	volatile u_char anoni[0x2];
	volatile u_char cl_brbsts;			/* 0x4e */
	volatile u_char cl_arbsts;			/* 0x4f */
#define cl_atbadr  atbadr.atbadr
#define cl_atbadru atbadr.hl.atbadru
#define cl_atbadrl atbadr.hl.atbadrl
	union {
		struct {
			volatile u_short atbadru;	/* 0x50 */
			volatile u_short atbadrl;	/* 0x52 */
		}hl;
		volatile u_long atbadr;			/* 0x50 */
	}atbadr;
#define cl_btbadr  btbadr.btbadr
#define cl_btbadru btbadr.hl.btbadru
#define cl_btbadrl btbadr.hl.btbadrl
	union {
		struct {
			volatile u_short btbadru;	/* 0x54 */
			volatile u_short btbadrl;	/* 0x56 */
		}hl;
		volatile u_long btbadr;			/* 0x54 */
	}btbadr;
	volatile u_short cl_btbcnt;			/* 0x58 */
	volatile u_short cl_atbcnt;			/* 0x5a */
	volatile u_char anono[0x2];
	volatile u_char cl_btbsts;			/* 0x5e */
	volatile u_char cl_atbsts;			/* 0x5f */
	volatile u_char anonp[0x20];
	volatile u_char cl_tftc;			/* 0x80 */
	volatile u_char cl_gfrcr;			/* 0x81 */
	volatile u_char anonq[0x2];
	volatile u_char cl_reoir;			/* 0x84 */
	volatile u_char cl_teoir;			/* 0x85 */
	volatile u_char cl_meoir;			/* 0x86 */
	volatile u_char anonr[0x1];
#define cl_risr risr.risr_risr
#define cl_risrl risr.hl.risr_risrl
#define cl_risrh risr.hl.risr_risrh
	union {
		volatile u_short risr_risr;		/* 0x88 */
		struct {
			volatile u_char risr_risrh;	/* 0x88 */
			volatile u_char risr_risrl;	/* 0x89 */
		}hl;
	}risr;
	volatile u_char cl_tisr;			/* 0x8a */
	volatile u_char cl_misr;			/* 0x8b */
	volatile u_char anons[0x2];
	volatile u_char cl_bercnt;			/* 0x8e */
	volatile u_char anont[0x31];
	volatile u_char cl_tcor;			/* 0xc0 */
	volatile u_char anonu[0x2];
	volatile u_char cl_tbpr;			/* 0xc3 */
	volatile u_char anonv[0x4];
	volatile u_char cl_rcor;			/* 0xc8 */
	volatile u_char anonw[0x2];
	volatile u_char cl_rbpr;			/* 0xcb */
	volatile u_char anonx[0xa];
	volatile u_char cl_cpsr;			/* 0xd6 */
	volatile u_char anony[0x3];
	volatile u_char cl_tpr;				/* 0xda */
	volatile u_char anonz[0x3];
	volatile u_char cl_msvr_rts;			/* 0xde */
	volatile u_char cl_msvr_dtr;			/* 0xdf */
	volatile u_char cl_tpilr;			/* 0xe0 */
	volatile u_char cl_rpilr;			/* 0xe1 */
	volatile u_char cl_stk;				/* 0xe2 */
	volatile u_char cl_mpilr;			/* 0xe3 */
	volatile u_char anonA[0x8];
	volatile u_char cl_tir;				/* 0xec */
	volatile u_char cl_rir;				/* 0xed */
	volatile u_char cl_car;				/* 0xee */
	volatile u_char cl_mir;				/* 0xef */
	volatile u_char anonB[0x6];
	volatile u_char cl_dmr;				/* 0xf6 */
	volatile u_char anonC[0x1];
#define cl_rdr cl_tdr
	volatile u_char cl_tdr;				/* 0xf8 */
	volatile u_char anonD[7];
};


#define CD2400_SIZE		0x200

/*
 * Cirrus chip base address on the mvme1x7 boards.
 */
#define CD2400_BASE_ADDR	0xfff45000
