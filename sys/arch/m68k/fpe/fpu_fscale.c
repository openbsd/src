/*	$NetBSD: fpu_fscale.c,v 1.2 1995/11/05 00:35:25 briggs Exp $	*/

/*
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

/*
 * FSCALE - separated from the other type0 arithmetic instructions
 * for performance reason; maybe unnecessary, but FSCALE assumes
 * the source operand be an integer.  It performs type conversion
 * only if the source operand is *not* an integer.
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <machine/frame.h>

#include "fpu_emulate.h"

int
fpu_emul_fscale(fe, insn)
     struct fpemu *fe;
     struct instruction *insn;
{
    struct frame *frame;
    u_int *fpregs;
    int word1, sig;
    int regnum, format;
    int scale, sign, exp;
    u_int m0, m1;
    u_int buf[3], fpsr;
    int flags;
    char regname;

    sig = 0;
    frame = fe->fe_frame;
    fpregs = &(fe->fe_fpframe->fpf_regs[0]);
    /* clear all exceptions and conditions */
    fpsr = fe->fe_fpsr & ~FPSR_EXCP & ~FPSR_CCB;
    if (fpu_debug_level & DL_FSCALE) {
	printf("  fpu_emul_fscale: FPSR = %08x, FPCR = %08x\n", fpsr, fe->fe_fpcr);
    }

    word1 = insn->is_word1;
    format = (word1 >> 10) & 7;
    regnum = (word1 >> 7) & 7;

    fe->fe_fpcr &= FPCR_ROUND;
    fe->fe_fpcr |= FPCR_ZERO;

    /* get the source operand */
    if ((word1 & 0x4000) == 0) {
	if (fpu_debug_level & DL_FSCALE) {
	    printf("  fpu_emul_fscale: FP%d op FP%d => FP%d\n",
		   format, regnum, regnum);
	}
	/* the operand is an FP reg */
	if (fpu_debug_level & DL_FSCALE) {
	    printf("  fpu_emul_scale: src opr FP%d=%08x%08x%08x\n",
		   format, fpregs[format*3], fpregs[format*3+1],
		   fpregs[format*3+2]);
	}
	fpu_explode(fe, &fe->fe_f2, FTYPE_EXT, &fpregs[format * 3]);
	fpu_implode(fe, &fe->fe_f2, FTYPE_LNG, buf);
    } else {
	/* the operand is in memory */
	if (format == FTYPE_DBL) {
	    insn->is_datasize = 8;
	} else if (format == FTYPE_SNG || format == FTYPE_LNG) {
	    insn->is_datasize = 4;
	} else if (format == FTYPE_WRD) {
	    insn->is_datasize = 2;
	} else if (format == FTYPE_BYT) {
	    insn->is_datasize = 1;
	} else if (format == FTYPE_EXT) {
	    insn->is_datasize = 12;
	} else {
	    /* invalid or unsupported operand format */
	    sig = SIGFPE;
	    return sig;
	}

	/* Get effective address. (modreg=opcode&077) */
	sig = fpu_decode_ea(frame, insn, &insn->is_ea0, insn->is_opcode);
	if (sig) {
	    if (fpu_debug_level & DL_FSCALE) {
		printf("  fpu_emul_fscale: error in decode_ea\n");
	    }
	    return sig;
	}

	if (fpu_debug_level & DL_FSCALE) {
	    printf("  fpu_emul_fscale: addr mode = ");
	    flags = insn->is_ea0.ea_flags;
	    regname = (insn->is_ea0.ea_regnum & 8) ? 'a' : 'd';

	    if (flags & EA_DIRECT) {
		printf("%c%d\n", regname, insn->is_ea0.ea_regnum & 7);
	    } else if (insn->is_ea0.ea_flags & EA_PREDECR) {
		printf("%c%d@-\n", regname, insn->is_ea0.ea_regnum & 7);
	    } else if (insn->is_ea0.ea_flags & EA_POSTINCR) {
		printf("%c%d@+\n", regname, insn->is_ea0.ea_regnum & 7);
	    } else if (insn->is_ea0.ea_flags & EA_OFFSET) {
		printf("%c%d@(%d)\n", regname, insn->is_ea0.ea_regnum & 7,
		       insn->is_ea0.ea_offset);
	    } else if (insn->is_ea0.ea_flags & EA_INDEXED) {
		printf("%c%d@(...)\n", regname, insn->is_ea0.ea_regnum & 7);
	    } else if (insn->is_ea0.ea_flags & EA_ABS) {
		printf("0x%08x\n", insn->is_ea0.ea_absaddr);
	    } else if (insn->is_ea0.ea_flags & EA_PC_REL) {
		printf("pc@(%d)\n", insn->is_ea0.ea_offset);
	    } else if (flags & EA_IMMED) {
		printf("#0x%08x%08x%08x\n",
		       insn->is_ea0.ea_immed[0], insn->is_ea0.ea_immed[1],
		       insn->is_ea0.ea_immed[2]);
	    } else {
		printf("%c%d@\n", regname, insn->is_ea0.ea_regnum & 7);
	    }
	}
	fpu_load_ea(frame, insn, &insn->is_ea0, (char*)buf);

	if (fpu_debug_level & DL_FSCALE) {
	    printf(" fpu_emul_fscale: src = %08x%08x%08x, siz = %d\n",
		   buf[0], buf[1], buf[2], insn->is_datasize);
	}
	if (format == FTYPE_LNG) {
	    /* nothing */
	} else if (format == FTYPE_WRD) {
	    /* sign-extend */
	    scale = buf[0] & 0xffff;
	    if (scale & 0x8000) {
		scale |= 0xffff0000;
	    }
	} else if (format == FTYPE_BYT) {
	    /* sign-extend */
	    scale = buf[0] & 0xff;
	    if (scale & 0x80) {
		scale |= 0xffffff00;
	    }
	} else if (format == FTYPE_DBL || format == FTYPE_SNG ||
		   format == FTYPE_EXT) {
	    fpu_explode(fe, &fe->fe_f2, format, buf);
	    fpu_implode(fe, &fe->fe_f2, FTYPE_LNG, buf);
	}
	/* make it look like we've got an FP oprand */
	fe->fe_f2.fp_class = (buf[0] == 0) ? FPC_ZERO : FPC_NUM;
    }

    /* assume there's no exception */
    sig = 0;

    /* it's barbaric but we're going to operate directly on
     * the dst operand's bit pattern */
    sign = fpregs[regnum * 3] & 0x80000000;
    exp = (fpregs[regnum * 3] & 0x7fff0000) >> 16;
    m0 = fpregs[regnum * 3 + 1];
    m1 = fpregs[regnum * 3 + 2];

    switch (fe->fe_f2.fp_class) {
    case FPC_SNAN:
	fpsr |= FPSR_SNAN;
    case FPC_QNAN:
	/* dst = NaN */
	exp = 0x7fff;
	m0 = m1 = 0xffffffff;
	break;
    case FPC_ZERO:
    case FPC_NUM:
	if ((0 < exp && exp < 0x7fff) ||
	    (exp == 0 && (m0 | m1) != 0)) {
	    /* normal or denormal */
	    exp += scale;
	    if (exp < 0) {
		/* underflow */
		u_int grs;	/* guard, round and sticky */

		exp = 0;
		grs = m1 << (32 + exp);
		m1 = m0 << (32 + exp) | m1 >> -exp;
		m0 >>= -exp;
		if (grs != 0) {
		    fpsr |= FPSR_INEX2;

		    switch (fe->fe_fpcr & 0x30) {
		    case FPCR_MINF:
			if (sign != 0) {
			    if (++m1 == 0 &&
				++m0 == 0) {
				m0 = 0x80000000;
				exp++;
			    }
			}
			break;
		    case FPCR_NEAR:
			if (grs == 0x80000000) {
			    /* tie */
			    if ((m1 & 1) &&
				++m1 == 0 &&
				++m0 == 0) {
				m0 = 0x80000000;
				exp++;
			    }
			} else if (grs & 0x80000000) {
			    if (++m1 == 0 &&
				++m0 == 0) {
				m0 = 0x80000000;
				exp++;
			    }
			}
			break;
		    case FPCR_PINF:
			if (sign == 0) {
			    if (++m1 == 0 &&
				++m0 == 0) {
				m0 = 0x80000000;
				exp++;
			    }
			}
			break;
		    case FPCR_ZERO:
			break;
		    }
		}
		if (exp == 0 && (m0 & 0x80000000) == 0) {
		    fpsr |= FPSR_UNFL;
		    if ((m0 | m1) == 0) {
			fpsr |= FPSR_ZERO;
		    }
		}
	    } else if (exp >= 0x7fff) {
		/* overflow --> result = Inf */
		/* but first, try to normalize in case it's an unnormalized */
		while ((m0 & 0x80000000) == 0) {
		    exp--;
		    m0 = (m0 << 1) | (m1 >> 31);
		    m1 = m1 << 1;
		}
		/* if it's still too large, then return Inf */
		if (exp >= 0x7fff) {
		    exp = 0x7fff;
		    m0 = m1 = 0;
		    fpsr |= FPSR_OVFL | FPSR_INF;
		}
	    } else if ((m0 & 0x80000000) == 0) {
		/*
		 * it's a denormal; we try to normalize but
		 * result may and may not be a normal.
		 */
		while (exp > 0 && (m0 & 0x80000000) == 0) {
		    exp--;
		    m0 = (m0 << 1) | (m1 >> 31);
		    m1 = m1 << 1;
		}
		if ((m0 & 0x80000000) == 0) {
		    fpsr |= FPSR_UNFL;
		}
	    } /* exp in range and mantissa normalized */
	} else if (exp == 0 && m0 == 0 && m1 == 0) {
	    /* dst is Zero */
	    fpsr |= FPSR_ZERO;
	} /* else we know exp == 0x7fff */
	else if ((m0 | m1) == 0) {
	    fpsr |= FPSR_INF;
	} else if ((m0 & 0x40000000) == 0) {
	    /* a signaling NaN */
	    fpsr |= FPSR_NAN | FPSR_SNAN;
	} else {
	    /* a quiet NaN */
	    fpsr |= FPSR_NAN;
	}
	break;
    case FPC_INF:
	/* dst = NaN */
	exp = 0x7fff;
	m0 = m1 = 0xffffffff;
	fpsr |= FPSR_OPERR | FPSR_NAN;
	break;
    default:
#ifdef DEBUG
	panic("  fpu_emul_fscale: invalid fp class");
#endif
	break;
    }

    /* store the result */
    fpregs[regnum * 3] = sign | (exp << 16);
    fpregs[regnum * 3 + 1] = m0;
    fpregs[regnum * 3 + 2] = m1;

    if (sign) {
	fpsr |= FPSR_NEG;
    }

    /* update fpsr according to the result of operation */
    fe->fe_fpframe->fpf_fpsr = fe->fe_fpsr = fpsr;

    if (fpu_debug_level & DL_FSCALE) {
	printf("  fpu_emul_fscale: FPSR = %08x, FPCR = %08x\n",
	       fe->fe_fpsr, fe->fe_fpcr);
    }

    return (fpsr & fe->fe_fpcr & FPSR_EXCP) ? SIGFPE : sig;
}
