/*	$OpenBSD: fpu_calcea.c,v 1.10 2006/01/16 22:08:26 miod Exp $	*/
/*	$NetBSD: fpu_calcea.c,v 1.16 2004/02/13 11:36:14 wiz Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
 * portion Copyright (c) 1995 Ken Nakata
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

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <machine/frame.h>

#include "fpu_emulate.h"

/*
 * Prototypes of local functions
 */
int decode_ea6(struct frame *frame, struct instruction *insn,
			   struct insn_ea *ea, int modreg);
int fetch_immed(struct frame *frame, struct instruction *insn,
			    int *dst);
int fetch_disp(struct frame *frame, struct instruction *insn,
			   int size, int *res);
int calc_ea(struct insn_ea *ea, char *ptr, char **eaddr);

/*
 * Helper routines for dealing with "effective address" values.
 */

/*
 * Decode an effective address into internal form.
 * Returns zero on success, else signal number.
 */
int
fpu_decode_ea(frame, insn, ea, modreg)
     struct frame *frame;
     struct instruction *insn;
     struct insn_ea *ea;
     int modreg;
{
    int sig;

#ifdef DEBUG
    if (insn->is_datasize < 0) {
	panic("decode_ea: called with uninitialized datasize");
    }
#endif

    sig = 0;

    /* Set the most common value here. */
    ea->ea_regnum = 8 + (modreg & 7);

    if ((modreg & 060) == 0) {
	/* register direct */
	ea->ea_regnum = modreg & 0xf;
	ea->ea_flags = EA_DIRECT;
#ifdef DEBUG_FPE
	printf("decode_ea: register direct reg=%d\n", ea->ea_regnum);
#endif
    } else if ((modreg & 077) == 074) {
	/* immediate */
	ea->ea_flags = EA_IMMED;
	sig = fetch_immed(frame, insn, &ea->ea_immed[0]);
#ifdef DEBUG_FPE
	printf("decode_ea: immediate size=%d\n", insn->is_datasize);
#endif
    }
    /*
     * rest of the address modes need to be separately
     * handled for the LC040 and the others.
     */
#if 0 /* XXX */
    else if (frame->f_format == 4 && frame->f_fmt4.f_fa) {
	/* LC040 */
	ea->ea_flags = EA_FRAME_EA;
	ea->ea_fea = frame->f_fmt4.f_fa;
#ifdef DEBUG_FPE
	printf("decode_ea: 68LC040 - in-frame EA (%p) size %d\n",
		(void *)ea->ea_fea, insn->is_datasize);
#endif
	if ((modreg & 070) == 030) {
	    /* postincrement mode */
	    ea->ea_flags |= EA_POSTINCR;
	} else if ((modreg & 070) == 040) {
	    /* predecrement mode */
	    ea->ea_flags |= EA_PREDECR;
#ifdef M68060
#if defined(M68020) || defined(M68030) || defined(M68040)
	    if (cputype == CPU_68060)
#endif
		if (insn->is_datasize == 12)
			ea->ea_fea -= 8;
#endif
	}
    }
#endif /* XXX */
    else {
	/* 020/030 */
	switch (modreg & 070) {

	case 020:			/* (An) */
	    ea->ea_flags = 0;
#ifdef DEBUG_FPE
	    printf("decode_ea: register indirect reg=%d\n", ea->ea_regnum);
#endif
	    break;

	case 030:			/* (An)+ */
	    ea->ea_flags = EA_POSTINCR;
#ifdef DEBUG_FPE
	    printf("decode_ea: reg indirect postincrement reg=%d\n",
		   ea->ea_regnum);
#endif
	    break;

	case 040:			/* -(An) */
	    ea->ea_flags = EA_PREDECR;
#ifdef DEBUG_FPE
	    printf("decode_ea: reg indirect predecrement reg=%d\n",
		   ea->ea_regnum);
#endif
	    break;

	case 050:			/* (d16,An) */
	    ea->ea_flags = EA_OFFSET;
	    sig = fetch_disp(frame, insn, 1, &ea->ea_offset);
#ifdef DEBUG_FPE
	    printf("decode_ea: reg indirect with displacement reg=%d\n",
		   ea->ea_regnum);
#endif
	    break;

	case 060:			/* (d8,An,Xn) */
	    ea->ea_flags = EA_INDEXED;
	    sig = decode_ea6(frame, insn, ea, modreg);
	    break;

	case 070:			/* misc. */
	    ea->ea_regnum = (modreg & 7);
	    switch (modreg & 7) {

	    case 0:			/* (xxxx).W */
		ea->ea_flags = EA_ABS;
		sig = fetch_disp(frame, insn, 1, &ea->ea_absaddr);
#ifdef DEBUG_FPE
		printf("decode_ea: absolute address (word)\n");
#endif
		break;

	    case 1:			/* (xxxxxxxx).L */
		ea->ea_flags = EA_ABS;
		sig = fetch_disp(frame, insn, 2, &ea->ea_absaddr);
#ifdef DEBUG_FPE
		printf("decode_ea: absolute address (long)\n");
#endif
		break;

	    case 2:			/* (d16,PC) */
		ea->ea_flags = EA_PC_REL | EA_OFFSET;
		sig = fetch_disp(frame, insn, 1, &ea->ea_absaddr);
#ifdef DEBUG_FPE
		printf("decode_ea: pc relative word displacement\n");
#endif
		break;

	    case 3:			/* (d8,PC,Xn) */
		ea->ea_flags = EA_PC_REL | EA_INDEXED;
		sig = decode_ea6(frame, insn, ea, modreg);
		break;

	    case 4:			/* #data */
		/* it should have been taken care of earlier */
	    default:
#ifdef DEBUG_FPE
		printf("decode_ea: invalid addr mode (7,%d)\n", modreg & 7);
#endif
		return SIGILL;
	    } /* switch for mode 7 */
	    break;
	} /* switch mode */
    }
    ea->ea_moffs = 0;

    return sig;
}

/*
 * Decode Mode=6 address modes
 */
int
decode_ea6(frame, insn, ea, modreg)
     struct frame *frame;
     struct instruction *insn;
     struct insn_ea *ea;
     int modreg;
{
    int idx;
    int basedisp, outerdisp;
    int bd_size, od_size;
    int sig;
    u_int16_t extword;

    if (copyin((void *)(insn->is_pc + insn->is_advance), &extword,
	sizeof(extword)) != 0) {
	return SIGSEGV;
    }
    insn->is_advance += 2;

    /* get register index */
    ea->ea_idxreg = (extword >> 12) & 0xf;
    idx = frame->f_regs[ea->ea_idxreg];
    if ((extword & 0x0800) == 0) {
	/* if word sized index, sign-extend */
	idx &= 0xffff;
	if (idx & 0x8000) {
	    idx |= 0xffff0000;
	}
    }
    /* scale register index */
    idx <<= ((extword >> 9) & 3);

    if ((extword & 0x100) == 0) {
	/* brief extension word - sign-extend the displacement */
	basedisp = (extword & 0xff);
	if (basedisp & 0x80) {
	    basedisp |= 0xffffff00;
	}

	ea->ea_basedisp = idx + basedisp;
	ea->ea_outerdisp = 0;
#if DEBUG_FPE
	printf("decode_ea6: brief ext word idxreg=%d, basedisp=%08x\n",
	       ea->ea_idxreg, ea->ea_basedisp);
#endif
    } else {
	/* full extension word */
	if (extword & 0x80) {
	    ea->ea_flags |= EA_BASE_SUPPRSS;
	}
	bd_size = ((extword >> 4) & 3) - 1;
	od_size = (extword & 3) - 1;
	sig = fetch_disp(frame, insn, bd_size, &basedisp);
	if (sig) {
	    return sig;
	}
	if (od_size >= 0) {
	    ea->ea_flags |= EA_MEM_INDIR;
	}
	sig = fetch_disp(frame, insn, od_size, &outerdisp);
	if (sig) {
	    return sig;
	}

	switch (extword & 0x44) {
	case 0:			/* preindexed */
	    ea->ea_basedisp = basedisp + idx;
	    ea->ea_outerdisp = outerdisp;
	    break;
	case 4:			/* postindexed */
	    ea->ea_basedisp = basedisp;
	    ea->ea_outerdisp = outerdisp + idx;
	    break;
	case 0x40:		/* no index */
	    ea->ea_basedisp = basedisp;
	    ea->ea_outerdisp = outerdisp;
	    break;
	default:
#ifdef DEBUG
	    printf("decode_ea6: invalid indirect mode: ext word %02x\n",
		   extword);
#endif
	    return SIGILL;
	    break;
	}
#if DEBUG_FPE
	printf("decode_ea6: full ext idxreg=%d, basedisp=%x, outerdisp=%x\n",
	       ea->ea_idxreg, ea->ea_basedisp, ea->ea_outerdisp);
#endif
    }
#if DEBUG_FPE
    printf("decode_ea6: regnum=%d, flags=%x\n",
	   ea->ea_regnum, ea->ea_flags);
#endif
    return 0;
}

/*
 * Load a value from an effective address.
 * Returns zero on success, else signal number.
 */
int
fpu_load_ea(frame, insn, ea, dst)
     struct frame *frame;
     struct instruction *insn;
     struct insn_ea *ea;
     char *dst;
{
    int *reg;
    char *src;
    int len, step;
    int sig;

#ifdef DIAGNOSTIC
    if (ea->ea_regnum & ~0xF) {
	panic("load_ea: bad regnum");
    }
#endif

#ifdef DEBUG_FPE
    printf("load_ea: frame at %p\n", frame);
#endif
    /* dst is always int or larger. */
    len = insn->is_datasize;
    if (len < 4) {
	dst += (4 - len);
    }
    step = (len == 1 && ea->ea_regnum == 15 /* sp */) ? 2 : len;

#if 0
    if (ea->ea_flags & EA_FRAME_EA) {
	/* Using LC040 frame EA */
#ifdef DEBUG_FPE
	if (ea->ea_flags & (EA_PREDECR|EA_POSTINCR)) {
	    printf("load_ea: frame ea %08x w/r%d\n",
		   ea->ea_fea, ea->ea_regnum);
	} else {
	    printf("load_ea: frame ea %08x\n", ea->ea_fea);
	}
#endif
	src = (char *)ea->ea_fea;
	if (copyin(src + ea->ea_moffs, dst, len) != 0)
	    return (SIGSEGV);
	if (ea->ea_flags & EA_PREDECR) {
	    frame->f_regs[ea->ea_regnum] = ea->ea_fea;
	    ea->ea_fea -= step;
	    ea->ea_moffs = 0;
	} else if (ea->ea_flags & EA_POSTINCR) {
	    ea->ea_fea += step;
	    frame->f_regs[ea->ea_regnum] = ea->ea_fea;
	    ea->ea_moffs = 0;
	} else {
	    ea->ea_moffs += step;
	}
	/* That's it, folks */
    } else if (ea->ea_flags & EA_DIRECT) {
	if (len > 4) {
#ifdef DEBUG
	    printf("load_ea: operand doesn't fit CPU reg\n");
#endif
	    return SIGILL;
	}
	if (ea->ea_moffs > 0) {
#ifdef DEBUG
	    printf("load_ea: more than one move from CPU reg\n");
#endif
	    return SIGILL;
	}
	src = (char *)&frame->f_regs[ea->ea_regnum];
	/* The source is an int. */
	if (len < 4) {
	    src += (4 - len);
#ifdef DEBUG_FPE
	    printf("load_ea: short/byte opr - addr adjusted\n");
#endif
	}
#ifdef DEBUG_FPE
	printf("load_ea: src %p\n", src);
#endif
	memcpy(dst, src, len);
    } else
#endif
    if (ea->ea_flags & EA_IMMED) {
#ifdef DEBUG_FPE
	printf("load_ea: immed %08x%08x%08x size %d\n",
	       ea->ea_immed[0], ea->ea_immed[1], ea->ea_immed[2], len);
#endif
	src = (char *)&ea->ea_immed[0];
	if (len < 4) {
	    src += (4 - len);
#ifdef DEBUG_FPE
	    printf("load_ea: short/byte immed opr - addr adjusted\n");
#endif
	}
	memcpy(dst, src, len);
    } else if (ea->ea_flags & EA_ABS) {
#ifdef DEBUG_FPE
	printf("load_ea: abs addr %08x\n", ea->ea_absaddr);
#endif
	src = (char *)ea->ea_absaddr;
	if (copyin(src, dst, len) != 0)
	    return (SIGSEGV);
    } else /* register indirect */ { 
	if (ea->ea_flags & EA_PC_REL) {
#ifdef DEBUG_FPE
	    printf("load_ea: using PC\n");
#endif
	    reg = NULL;
	    /* Grab the register contents. 4 is offset to the first
	       extension word from the opcode */
	    src = (char *)insn->is_pc + 4;
#ifdef DEBUG_FPE
	    printf("load_ea: pc relative pc+4 = %p\n", src);
#endif
	} else /* not PC relative */ {
#ifdef DEBUG_FPE
	    printf("load_ea: using register %c%d\n",
		   (ea->ea_regnum >= 8) ? 'a' : 'd', ea->ea_regnum & 7);
#endif
	    /* point to the register */
	    reg = &frame->f_regs[ea->ea_regnum];

	    if (ea->ea_flags & EA_PREDECR) {
#ifdef DEBUG_FPE
		printf("load_ea: predecr mode - reg decremented\n");
#endif
		*reg -= step;
		ea->ea_moffs = 0;
	    }

	    /* Grab the register contents. */
	    src = (char *)*reg;
#ifdef DEBUG_FPE
	    printf("load_ea: reg indirect reg = %p\n", src);
#endif
	}

	sig = calc_ea(ea, src, &src);
	if (sig)
	    return sig;

	if (copyin(src + ea->ea_moffs, dst, len) != 0)
	    return (SIGSEGV);

	/* do post-increment */
	if (ea->ea_flags & EA_POSTINCR) {
	    if (ea->ea_flags & EA_PC_REL) {
#ifdef DEBUG
		printf("load_ea: tried to postincrement PC\n");
#endif
		return SIGILL;
	    }
	    *reg += step;
	    ea->ea_moffs = 0;
#ifdef DEBUG_FPE
	    printf("load_ea: postinc mode - reg incremented\n");
#endif
	} else {
	    ea->ea_moffs += len;
	}
    }

    return 0;
}

/*
 * Store a value at the effective address.
 * Returns zero on success, else signal number.
 */
int
fpu_store_ea(frame, insn, ea, src)
     struct frame *frame;
     struct instruction *insn;
     struct insn_ea *ea;
     char *src;
{
    int *reg;
    char *dst;
    int len, step;
    int sig;

#ifdef	DIAGNOSTIC
    if (ea->ea_regnum & ~0xf) {
	panic("store_ea: bad regnum");
    }
#endif

    if (ea->ea_flags & (EA_IMMED|EA_PC_REL)) {
	/* not alterable address mode */
#ifdef DEBUG
	printf("store_ea: not alterable address mode\n");
#endif
	return SIGILL;
    }

    /* src is always int or larger. */
    len = insn->is_datasize;
    if (len < 4) {
	src += (4 - len);
    }
    step = (len == 1 && ea->ea_regnum == 15 /* sp */) ? 2 : len;

    if (ea->ea_flags & EA_FRAME_EA) {
	/* Using LC040 frame EA */
#ifdef DEBUG_FPE
	if (ea->ea_flags & (EA_PREDECR|EA_POSTINCR)) {
	    printf("store_ea: frame ea %08x w/r%d\n",
		   ea->ea_fea, ea->ea_regnum);
	} else {
	    printf("store_ea: frame ea %08x\n", ea->ea_fea);
	}
#endif
	dst = (char *)ea->ea_fea;
	copyout(src, dst + ea->ea_moffs, len);
	if (ea->ea_flags & EA_PREDECR) {
	    frame->f_regs[ea->ea_regnum] = ea->ea_fea;
	    ea->ea_fea -= step;
	    ea->ea_moffs = 0;
	} else if (ea->ea_flags & EA_POSTINCR) {
	    ea->ea_fea += step;
	    frame->f_regs[ea->ea_regnum] = ea->ea_fea;
	    ea->ea_moffs = 0;
	} else {
	    ea->ea_moffs += step;
	}
	/* That's it, folks */
    } else if (ea->ea_flags & EA_ABS) {
#ifdef DEBUG_FPE
	printf("store_ea: abs addr %08x\n", ea->ea_absaddr);
#endif
	dst = (char *)ea->ea_absaddr;
	copyout(src, dst + ea->ea_moffs, len);
	ea->ea_moffs += len;
    } else if (ea->ea_flags & EA_DIRECT) {
	if (len > 4) {
#ifdef DEBUG
	    printf("store_ea: operand doesn't fit CPU reg\n");
#endif
	    return SIGILL;
	}
	if (ea->ea_moffs > 0) {
#ifdef DEBUG
	    printf("store_ea: more than one move to CPU reg\n");
#endif
	    return SIGILL;
	}
	dst = (char *)&frame->f_regs[ea->ea_regnum];
	/* The destination is an int. */
	if (len < 4) {
	    dst += (4 - len);
#ifdef DEBUG_FPE
	    printf("store_ea: short/byte opr - dst addr adjusted\n");
#endif
	}
#ifdef DEBUG_FPE
	printf("store_ea: dst %p\n", dst);
#endif
	memcpy(dst, src, len);
    } else /* One of MANY indirect forms... */ {
#ifdef DEBUG_FPE
	printf("store_ea: using register %c%d\n",
	       (ea->ea_regnum >= 8) ? 'a' : 'd', ea->ea_regnum & 7);
#endif
	/* point to the register */
	reg = &(frame->f_regs[ea->ea_regnum]);

	/* do pre-decrement */
	if (ea->ea_flags & EA_PREDECR) {
#ifdef DEBUG_FPE
	    printf("store_ea: predecr mode - reg decremented\n");
#endif
	    *reg -= step;
	    ea->ea_moffs = 0;
	}

	/* calculate the effective address */
	sig = calc_ea(ea, (char *)*reg, &dst);
	if (sig)
	    return sig;

#ifdef DEBUG_FPE
	printf("store_ea: dst addr=%p+%d\n", dst, ea->ea_moffs);
#endif
	copyout(src, dst + ea->ea_moffs, len);

	/* do post-increment */
	if (ea->ea_flags & EA_POSTINCR) {
	    *reg += step;
	    ea->ea_moffs = 0;
#ifdef DEBUG_FPE
	    printf("store_ea: postinc mode - reg incremented\n");
#endif
	} else {
	    ea->ea_moffs += len;
	}
    }

    return 0;
}

/*
 * fetch_immed: fetch immediate operand
 */
int
fetch_immed(frame, insn, dst)
     struct frame *frame;
     struct instruction *insn;
     int *dst;
{
	int data, ext_bytes;
	u_int16_t tmp;

	ext_bytes = insn->is_datasize;
	if (ext_bytes < 0)
		return (0);

	if (ext_bytes <= 2) {
		if (copyin((void *)(insn->is_pc + insn->is_advance), &tmp,
		    sizeof(tmp)) != 0) {
			return SIGSEGV;
		}
		if (ext_bytes == 1) {
			/* sign-extend byte to long */
			data = (char)tmp;
		} else {
			/* sign-extend word to long */
			data = (int)tmp;
		}
		insn->is_advance += 2;
		dst[0] = data;
		return (0);
	}

	/* if (ext_bytes > 2) { */
		if (copyin((void *)(insn->is_pc + insn->is_advance), &dst[0],
		    sizeof(dst[0])) != 0) {
			return SIGSEGV;
		}
		insn->is_advance += 4;
	/* } */

	if (ext_bytes > 4) {
		if (copyin((void *)(insn->is_pc + insn->is_advance), &dst[1],
		    sizeof(dst[1])) != 0) {
			return SIGSEGV;
		}
		insn->is_advance += 4;
	}

	if (ext_bytes > 8) {
		if (copyin((void *)(insn->is_pc + insn->is_advance), &dst[2],
		     sizeof(dst[2])) != 0) {
			return SIGSEGV;
		}
		insn->is_advance += 4;
	}

	return 0;
}

/*
 * fetch_disp: fetch displacement in full extension words
 */
int
fetch_disp(frame, insn, size, res)
     struct frame *frame;
     struct instruction *insn;
     int size, *res;
{
	int disp;
	u_int16_t word;

	switch (size) {
	case 1:
		if (copyin((void *)(insn->is_pc + insn->is_advance), &word,
		    sizeof(word)) != 0) {
			return SIGSEGV;
		}
		/* sign-extend */
		disp = (int)word;
		insn->is_advance += 2;
		break;
	case 2:
		if (copyin((void *)(insn->is_pc + insn->is_advance), &disp,
		    sizeof(disp)) != 0) {
			return SIGSEGV;
		}
		insn->is_advance += 4;
		break;
	default:
		disp = 0;
		break;
	}

	*res = disp;

	return 0;
}

/*
 * Calculates an effective address for all address modes except for
 * register direct, absolute, and immediate modes.  However, it does
 * not take care of predecrement/postincrement of register content.
 * Returns a signal value (0 == no error).
 */
int
calc_ea(ea, ptr, eaddr)
     struct insn_ea *ea;
     char *ptr;		/* base address (usually a register content) */
     char **eaddr;	/* pointer to result pointer */
{
    int word;

#if DEBUG_FPE
    printf("calc_ea: reg indirect (reg) = %p\n", ptr);
#endif

    if (ea->ea_flags & EA_OFFSET) {
	/* apply the signed offset */
#if DEBUG_FPE
	printf("calc_ea: offset %d\n", ea->ea_offset);
#endif
	ptr += ea->ea_offset;
    } else if (ea->ea_flags & EA_INDEXED) {
#if DEBUG_FPE
	printf("calc_ea: indexed mode\n");
#endif

	if (ea->ea_flags & EA_BASE_SUPPRSS) {
	    /* base register is suppressed */
	    ptr = (char *)ea->ea_basedisp;
	} else {
	    ptr += ea->ea_basedisp;
	}

	if (ea->ea_flags & EA_MEM_INDIR) {
#if DEBUG_FPE
	    printf("calc_ea: mem indir mode: basedisp=%08x, outerdisp=%08x\n",
		   ea->ea_basedisp, ea->ea_outerdisp);
	    printf("calc_ea: addr fetched from %p\n", ptr);
#endif
	    /* memory indirect modes */
	    if (copyin(ptr, &word, sizeof(word)) != 0) {
		return SIGSEGV;
	    }
#if DEBUG_FPE
	    printf("calc_ea: fetched ptr 0x%08x\n", word);
#endif
	    ptr = (char *)word + ea->ea_outerdisp;
	}
    }

    *eaddr = ptr;

    return 0;
}
