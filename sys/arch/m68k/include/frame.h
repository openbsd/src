/*	$OpenBSD: frame.h,v 1.8 2003/11/16 20:30:06 avsm Exp $	*/
/*	$NetBSD: frame.h,v 1.15 1997/05/03 12:49:05 mycroft Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: frame.h 1.8 92/12/20$
 *
 *	@(#)frame.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_M68K_FRAME_H_
#define	_M68K_FRAME_H_

struct frame {
	struct trapframe {
		int	tf_regs[16];
		short	tf_pad;
		short	tf_stackadj;
		u_short	tf_sr;
		u_int	tf_pc;
		u_short	tf_format:4,
			tf_vector:12;
	} __packed F_t;
	union F_u {
		struct fmt2 {
			u_int	f_iaddr;
		} F_fmt2;

		struct fmt3 {
			u_int	f_ea;
		} F_fmt3;

		struct fmt4 {
			u_int	f_fa;
			u_int	f_fslw;
			/* for 060FP type 4 FP disabled frames: */
#define 		f_fea	f_fa	
#define 		f_pcfi	f_fslw
		} F_fmt4;

		struct fmt7 {
			u_int	f_ea;
			u_short	f_ssw;
			u_short	f_wb3s, f_wb2s, f_wb1s;
			u_int	f_fa;
			u_int	f_wb3a, f_wb3d;
			u_int	f_wb2a, f_wb2d;
			u_int	f_wb1a, f_wb1d;
#define				f_pd0 f_wb1d
			u_int	f_pd1, f_pd2, f_pd3;
		} F_fmt7;

		struct fmt9 {
			u_int	f_iaddr;
			u_short	f_iregs[4];
		} F_fmt9;

		struct fmtA {
			u_short	f_ir0;
			u_short	f_ssw;
			u_short	f_ipsc;
			u_short	f_ipsb;
			u_int	f_dcfa;
			u_short	f_ir1, f_ir2;
			u_int	f_dob;
			u_short	f_ir3, f_ir4;
		} F_fmtA;

		struct fmtB {
			u_short	f_ir0;
			u_short	f_ssw;
			u_short	f_ipsc;
			u_short	f_ipsb;
			u_int	f_dcfa;
			u_short	f_ir1, f_ir2;
			u_int	f_dob;
			u_short	f_ir3, f_ir4;
			u_short	f_ir5, f_ir6;
			u_int	f_sba;
			u_short	f_ir7, f_ir8;
			u_int	f_dib;
			u_short	f_iregs[22];
		} F_fmtB;
	} F_u;
};

#define	f_regs		F_t.tf_regs
#define	f_pad		F_t.tf_pad
#define	f_stackadj	F_t.tf_stackadj
#define	f_sr		F_t.tf_sr
#define	f_pc		F_t.tf_pc
#define	f_format	F_t.tf_format
#define	f_vector	F_t.tf_vector
#define	f_fmt2		F_u.F_fmt2
#define	f_fmt3		F_u.F_fmt3
#define	f_fmt4		F_u.F_fmt4
#define	f_fmt7		F_u.F_fmt7
#define	f_fmt9		F_u.F_fmt9
#define	f_fmtA		F_u.F_fmtA
#define	f_fmtB		F_u.F_fmtB

struct switchframe {
	u_int	sf_pc;
};

/* common frame size */
#define	CFSIZE		(sizeof(struct frame) - sizeof(union F_u))
#define	NFMTSIZE	9

#define	FMT0		0x0
#define	FMT1		0x1
#define	FMT2		0x2
#define	FMT3		0x3
#define	FMT4		0x4
#define	FMT7		0x7
#define	FMT9		0x9
#define	FMTA		0xA
#define	FMTB		0xB

/* frame specific info sizes */
#define	FMT0SIZE	0
#define	FMT1SIZE	0
#define	FMT2SIZE	sizeof(struct fmt2)
#define	FMT3SIZE	sizeof(struct fmt3)
#define	FMT4SIZE	sizeof(struct fmt4)
#define	FMT7SIZE	sizeof(struct fmt7)
#define	FMT9SIZE	sizeof(struct fmt9)
#define	FMTASIZE	sizeof(struct fmtA)
#define	FMTBSIZE	sizeof(struct fmtB)

#define	V_BUSERR	0x008
#define	V_ADDRERR	0x00C
#define	V_TRAP1		0x084

/* 68020/68030 SSW bits */
#define	SSW_RC		0x2000
#define	SSW_RB		0x1000
#define	SSW_DF		0x0100
#define	SSW_RM		0x0080
#define	SSW_RW		0x0040
#define	SSW_FCMASK	0x0007

/* 68040 SSW bits */
#define	SSW4_CP		0x8000
#define	SSW4_CU		0x4000
#define	SSW4_CT		0x2000
#define	SSW4_CM		0x1000
#define	SSW4_MA		0x0800
#define	SSW4_ATC	0x0400
#define	SSW4_LK		0x0200
#define	SSW4_RW		0x0100
#define SSW4_WBSV	0x0080	/* really in WB status, not SSW */
#define	SSW4_SZMASK	0x0060
#define	SSW4_SZLW	0x0000
#define	SSW4_SZB	0x0020
#define	SSW4_SZW	0x0040
#define	SSW4_SZLN	0x0060
#define	SSW4_TTMASK	0x0018
#define	SSW4_TTNOR	0x0000
#define	SSW4_TTM16	0x0008
#define	SSW4_TMMASK	0x0007
#define	SSW4_TMDCP	0x0000
#define	SSW4_TMUD	0x0001
#define	SSW4_TMUC	0x0002
#define	SSW4_TMKD	0x0005
#define	SSW4_TMKC	0x0006

/* 060 Fault Status Long Word (FPSP) */

#define FSLW_MA		0x08000000
#define FSLW_LK		0x02000000
#define FSLW_RW		0x01800000

#define FSLW_RW_R	0x01000000
#define FSLW_RW_W	0x00800000

#define FSLW_SIZE	0x00600000
/*
 * We better define the FSLW_SIZE values here, as the table given in the 
 * MC68060UM/AD rev. 0/1 p. 8-23 is wrong, and was corrected in the errata 
 * document.
 */
#define FSLW_SIZE_LONG	0x00000000
#define FSLW_SIZE_BYTE	0x00200000
#define FSLW_SIZE_WORD	0x00400000
#define FSLW_SIZE_MV16	0x00600000

#define FLSW_TT		0x00180000
#define FSLW_TM		0x00070000
#define FSLW_TM_SV	0x00040000



#define FSLW_IO		0x00008000
#define FSLW_PBE	0x00004000
#define FSLW_SBE	0x00002000
#define FSLW_PTA	0x00001000
#define FSLW_PTB 	0x00000800
#define FSLW_IL 	0x00000400
#define FSLW_PF 	0x00000200
#define FSLW_SP 	0x00000100
#define FSLW_WP 	0x00000080
#define FSLW_TWE 	0x00000040
#define FSLW_RE 	0x00000020
#define FSLW_WE 	0x00000010
#define FSLW_TTR 	0x00000008
#define FSLW_BPE 	0x00000004
#define FSLW_SEE 	0x00000001

struct fpframe {
	union FPF_u1 {
		u_int	FPF_null;
		struct {
			u_char	FPF_version;
			u_char	FPF_fsize;
			u_short	FPF_res1;
		} FPF_nonnull;
	} FPF_u1;
	union FPF_u2 {
		struct fpidle {
			u_short	fpf_ccr;
			u_short	fpf_res2;
			u_int	fpf_iregs1[8];
			u_int	fpf_xops[3];
			u_int	fpf_opreg;
			u_int	fpf_biu;
		} FPF_idle;

		struct fpbusy {
			u_int	fpf_iregs[53];
		} FPF_busy;

		struct fpunimp {
			u_int	fpf_state[10];
		} FPF_unimp;
	} FPF_u2;
	u_int	fpf_regs[8*3];
	u_int	fpf_fpcr;
	u_int	fpf_fpsr;
	u_int	fpf_fpiar;
};

#define fpf_null	FPF_u1.FPF_null
#define fpf_version	FPF_u1.FPF_nonnull.FPF_version
#define fpf_fsize	FPF_u1.FPF_nonnull.FPF_fsize
#define fpf_res1	FPF_u1.FPF_nonnull.FPF_res1
#define fpf_idle	FPF_u2.FPF_idle
#define fpf_busy	FPF_u2.FPF_busy
#define fpf_unimp	FPF_u2.FPF_unimp

/* 
 * This is incompatible with the earlier one; expecially, an earlier frame 
 * must not be FRESTOREd on a 060 or vv, because a frame error exception is
 * not guaranteed.
 */


struct fpframe060 {
	u_short	fpf6_excp_exp;
	u_char	fpf6_frmfmt;
#define FPF6_FMT_NULL	0x00
#define FPF6_FMT_IDLE	0x60
#define FPF6_FMT_EXCP	0xe0

	u_char	fpf6_v;
#define	FPF6_V_BSUN	0
#define	FPF6_V_INEX12	1
#define	FPF6_V_DZ	2
#define	FPF6_V_UNFL	3
#define	FPF6_V_OPERR	4
#define	FPF6_V_OVFL	5
#define	FPF6_V_SNAN	6
#define	FPF6_V_UNSUP	7

	u_long	fpf6_upper, fpf6_lower;
};

#endif	/* _M68K_FRAME_H_ */
