/*	$NetBSD: fpu_emulate.h,v 1.2 1995/11/05 00:35:20 briggs Exp $	*/

/*
 * Copyright (c) 1995 Gordon Ross
 * Copyright (c) 1995 Ken Nakata
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
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

#ifndef _FPU_EMULATE_H_
#define _FPU_EMULATE_H_

#include <sys/types.h>

/*
 * Floating point emulator (tailored for SPARC/modified for m68k, but
 * structurally machine-independent).
 *
 * Floating point numbers are carried around internally in an `expanded'
 * or `unpacked' form consisting of:
 *	- sign
 *	- unbiased exponent
 *	- mantissa (`1.' + 112-bit fraction + guard + round)
 *	- sticky bit
 * Any implied `1' bit is inserted, giving a 113-bit mantissa that is
 * always nonzero.  Additional low-order `guard' and `round' bits are
 * scrunched in, making the entire mantissa 115 bits long.  This is divided
 * into four 32-bit words, with `spare' bits left over in the upper part
 * of the top word (the high bits of fp_mant[0]).  An internal `exploded'
 * number is thus kept within the half-open interval [1.0,2.0) (but see
 * the `number classes' below).  This holds even for denormalized numbers:
 * when we explode an external denorm, we normalize it, introducing low-order
 * zero bits, so that the rest of the code always sees normalized values.
 *
 * Note that a number of our algorithms use the `spare' bits at the top.
 * The most demanding algorithm---the one for sqrt---depends on two such
 * bits, so that it can represent values up to (but not including) 8.0,
 * and then it needs a carry on top of that, so that we need three `spares'.
 *
 * The sticky-word is 32 bits so that we can use `OR' operators to goosh
 * whole words from the mantissa into it.
 *
 * All operations are done in this internal extended precision.  According
 * to Hennesey & Patterson, Appendix A, rounding can be repeated---that is,
 * it is OK to do a+b in extended precision and then round the result to
 * single precision---provided single, double, and extended precisions are
 * `far enough apart' (they always are), but we will try to avoid any such
 * extra work where possible.
 */
struct fpn {
	int	fp_class;		/* see below */
	int	fp_sign;		/* 0 => positive, 1 => negative */
	int	fp_exp;			/* exponent (unbiased) */
	int	fp_sticky;		/* nonzero bits lost at right end */
	u_int	fp_mant[4];		/* 115-bit mantissa */
};

#define	FP_NMANT	115		/* total bits in mantissa (incl g,r) */
#define	FP_NG		2		/* number of low-order guard bits */
#define	FP_LG		((FP_NMANT - 1) & 31)	/* log2(1.0) for fp_mant[0] */
#define	FP_QUIETBIT	(1 << (FP_LG - 1))	/* Quiet bit in NaNs (0.5) */
#define	FP_1		(1 << FP_LG)		/* 1.0 in fp_mant[0] */
#define	FP_2		(1 << (FP_LG + 1))	/* 2.0 in fp_mant[0] */

#define CPYFPN(dst, src)						\
if ((dst) != (src)) {							\
    (dst)->fp_class = (src)->fp_class;					\
    (dst)->fp_sign = (src)->fp_sign;					\
    (dst)->fp_exp = (src)->fp_exp;					\
    (dst)->fp_sticky = (src)->fp_sticky;				\
    (dst)->fp_mant[0] = (src)->fp_mant[0];				\
    (dst)->fp_mant[1] = (src)->fp_mant[1];				\
    (dst)->fp_mant[2] = (src)->fp_mant[2];				\
    (dst)->fp_mant[3] = (src)->fp_mant[3];				\
}

/*
 * Number classes.  Since zero, Inf, and NaN cannot be represented using
 * the above layout, we distinguish these from other numbers via a class.
 */
#define	FPC_SNAN	-2		/* signalling NaN (sign irrelevant) */
#define	FPC_QNAN	-1		/* quiet NaN (sign irrelevant) */
#define	FPC_ZERO	0		/* zero (sign matters) */
#define	FPC_NUM		1		/* number (sign matters) */
#define	FPC_INF		2		/* infinity (sign matters) */

#define	ISNAN(fp)	((fp)->fp_class < 0)
#define	ISZERO(fp)	((fp)->fp_class == 0)
#define	ISINF(fp)	((fp)->fp_class == FPC_INF)

/*
 * ORDER(x,y) `sorts' a pair of `fpn *'s so that the right operand (y) points
 * to the `more significant' operand for our purposes.  Appendix N says that
 * the result of a computation involving two numbers are:
 *
 *	If both are SNaN: operand 2, converted to Quiet
 *	If only one is SNaN: the SNaN operand, converted to Quiet
 *	If both are QNaN: operand 2
 *	If only one is QNaN: the QNaN operand
 *
 * In addition, in operations with an Inf operand, the result is usually
 * Inf.  The class numbers are carefully arranged so that if
 *	(unsigned)class(op1) > (unsigned)class(op2)
 * then op1 is the one we want; otherwise op2 is the one we want.
 */
#define	ORDER(x, y) { \
	if ((u_int)(x)->fp_class > (u_int)(y)->fp_class) \
		SWAP(x, y); \
}
#define	SWAP(x, y) {				\
	register struct fpn *swap;		\
	swap = (x), (x) = (y), (y) = swap;	\
}

/*
 * Emulator state.
 */
struct fpemu {
    struct	frame *fe_frame; /* integer regs, etc */
    struct	fpframe *fe_fpframe; /* FP registers, etc */
    u_int	fe_fpsr;	/* fpsr copy (modified during op) */
    u_int	fe_fpcr;	/* fpcr copy */
    struct	fpn fe_f1;	/* operand 1 */
    struct	fpn fe_f2;	/* operand 2, if required */
    struct	fpn fe_f3;	/* available storage for result */
};

/*****************************************************************************
 * End of definitions derived from Sparc FPE
 *****************************************************************************/

/*
 * Internal info about a decoded effective address.
 */
struct insn_ea {
    int	ea_regnum;
    int	ea_ext[3];		/* extention words if any */
    int	ea_flags;		/* flags == 0 means mode 2: An@ */
#define	EA_DIRECT	0x001	/* mode [01]: Dn or An */
#define EA_PREDECR	0x002	/* mode 4: An@- */
#define	EA_POSTINCR	0x004	/* mode 3: An@+ */
#define EA_OFFSET	0x008	/* mode 5 or (7,2): APC@(d16) */
#define	EA_INDEXED	0x010	/* mode 6 or (7,3): APC@(Xn:*:*,d8) etc */
#define EA_ABS  	0x020	/* mode (7,[01]): abs */
#define EA_PC_REL	0x040	/* mode (7,[23]): PC@(d16) etc */
#define	EA_IMMED	0x080	/* mode (7,4): #immed */
#define EA_MEM_INDIR	0x100	/* mode 6 or (7,3): APC@(Xn:*:*,*)@(*) etc */
#define EA_BASE_SUPPRSS	0x200	/* mode 6 or (7,3): base register suppressed */
    int	ea_tdisp;		/* temp. displ. used to xfer many words */
};

#define ea_offset	ea_ext[0]	/* mode 5: offset word */
#define ea_absaddr	ea_ext[0]	/* mode (7,[01]): absolute address */
#define ea_immed	ea_ext		/* mode (7,4): immediate value */
#define ea_basedisp	ea_ext[0]	/* mode 6: base displacement */
#define ea_outerdisp	ea_ext[1]	/* mode 6: outer displacement */
#define	ea_idxreg	ea_ext[2]	/* mode 6: index register number */

struct instruction {
    int		is_advance;	/* length of instruction */
    int		is_datasize;	/* byte, word, long, float, double, ... */
    int		is_opcode;	/* opcode word */
    int		is_word1;	/* second word */
    struct	insn_ea	is_ea0;	/* decoded effective address mode */
};

/*
 * FP data types
 */
#define FTYPE_LNG 0 /* Long Word Integer */
#define FTYPE_SNG 1 /* Single Prec */
#define FTYPE_EXT 2 /* Extended Prec */
#define FTYPE_BCD 3 /* Packed BCD */
#define FTYPE_WRD 4 /* Word Integer */
#define FTYPE_DBL 5 /* Double Prec */
#define FTYPE_BYT 6 /* Byte Integer */

/*
 * MC68881/68882 FPcr bit definitions (should these go to <m68k/reg.h>
 * or <m68k/fpu.h> or something?)
 */

/* fpsr */
#define FPSR_CCB    0xff000000
# define FPSR_NEG   0x08000000
# define FPSR_ZERO  0x04000000
# define FPSR_INF   0x02000000
# define FPSR_NAN   0x01000000
#define FPSR_QTT    0x00ff0000
# define FPSR_QSG   0x00800000
# define FPSR_QUO   0x007f0000
#define FPSR_EXCP   0x0000ff00
# define FPSR_BSUN  0x00008000
# define FPSR_SNAN  0x00004000
# define FPSR_OPERR 0x00002000
# define FPSR_OVFL  0x00001000
# define FPSR_UNFL  0x00000800
# define FPSR_DZ    0x00000400
# define FPSR_INEX2 0x00000200
# define FPSR_INEX1 0x00000100
#define FPSR_AEX    0x000000ff
# define FPSR_AIOP  0x00000080
# define FPSR_AOVFL 0x00000040
# define FPSR_AUNFL 0x00000020
# define FPSR_ADZ   0x00000010
# define FPSR_AINEX 0x00000008

/* fpcr */
#define FPCR_EXCP   FPSR_EXCP
# define FPCR_BSUN  FPSR_BSUN
# define FPCR_SNAN  FPSR_SNAN
# define FPCR_OPERR FPSR_OPERR
# define FPCR_OVFL  FPSR_OVFL
# define FPCR_UNFL  FPSR_UNFL
# define FPCR_DZ    FPSR_DZ
# define FPCR_INEX2 FPSR_INEX2
# define FPCR_INEX1 FPSR_INEX1
#define FPCR_MODE   0x000000ff
# define FPCR_PREC  0x000000c0
#  define FPCR_EXTD 0x00000000
#  define FPCR_SNGL 0x00000040
#  define FPCR_DBL  0x00000080
# define FPCR_ROUND 0x00000030
#  define FPCR_NEAR 0x00000000
#  define FPCR_ZERO 0x00000010
#  define FPCR_MINF 0x00000020
#  define FPCR_PINF 0x00000030

/*
 * Other functions.
 */

/* Build a new Quiet NaN (sign=0, frac=all 1's). */
struct	fpn *fpu_newnan __P((struct fpemu *fe));

/*
 * Shift a number right some number of bits, taking care of round/sticky.
 * Note that the result is probably not a well-formed number (it will lack
 * the normal 1-bit mant[0]&FP_1).
 */
int	fpu_shr __P((struct fpn * fp, int shr));
/*
 * Round a number according to the round mode in FPCR
 */
int	round __P((register struct fpemu *fe, register struct fpn *fp));

/* type conversion */
void	fpu_explode __P((struct fpemu *fe, struct fpn *fp, int t, u_int *src));
void	fpu_implode __P((struct fpemu *fe, struct fpn *fp, int t, u_int *dst));

/*
 * non-static emulation functions
 */
/* type 0 */
int fpu_emul_fmovecr __P((struct fpemu *fe, struct instruction *insn));
int fpu_emul_fstore __P((struct fpemu *fe, struct instruction *insn));
int fpu_emul_fscale __P((struct fpemu *fe, struct instruction *insn));

/*
 * include function declarations of those which are called by fpu_emul_arith()
 */
#include "fpu_arith_proto.h"

/*
 * "helper" functions
 */
/* return values from constant rom */
struct fpn *fpu_const __P((struct fpn *fp, u_int offset));
/* update exceptions and FPSR */
int fpu_upd_excp __P((struct fpemu *fe));
u_int fpu_upd_fpsr __P((struct fpemu *fe, struct fpn *fp));

/* address mode decoder, and load/store */
int fpu_decode_ea __P((struct frame *frame, struct instruction *insn,
		   struct insn_ea *ea, int modreg));
int fpu_load_ea __P((struct frame *frame, struct instruction *insn,
		 struct insn_ea *ea, char *dst));
int fpu_store_ea __P((struct frame *frame, struct instruction *insn,
		  struct insn_ea *ea, char *src));

/* macros for debugging */
#define	DEBUG			/* XXX */

extern int fpu_debug_level;

/* debug classes */
#define DL_DUMPINSN 0x0001
#define DL_DECODEEA 0x0002
#define DL_LOADEA   0x0004
#define DL_STOREEA  0x0008
#define DL_OPERANDS 0x0010
#define DL_RESULT   0x0020
#define DL_TESTCC   0x0040
#define DL_BRANCH   0x0080
#define DL_FSTORE   0x0100
#define DL_FSCALE   0x0200
#define DL_ARITH    0x0400
#define DL_INSN     0x0800
#define DL_FMOVEM   0x1000
/* not defined yet
#define DL_2000     0x2000
#define DL_4000     0x4000
*/
#define DL_VERBOSE  0x8000
/* composit debug classes */
#define DL_EA       (DL_DECODEEA|DL_LOADEA|DL_STOREEA)
#define DL_VALUES   (DL_OPERANDS|DL_RESULT)
#define DL_COND     (DL_TESTCC|DL_BRANCH)
#define DL_ALL      0xffff

#endif /* _FPU_EMULATE_H_ */
