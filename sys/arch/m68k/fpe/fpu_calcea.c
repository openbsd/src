/*	$NetBSD: fpu_calcea.c,v 1.2 1995/11/05 00:35:15 briggs Exp $	*/

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

#include <stddef.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <machine/frame.h>

#include "fpu_emulate.h"

/*
 * Prototypes of static functions
 */
static int decode_ea6 __P((struct frame *frame, struct instruction *insn,
			   struct insn_ea *ea, int modreg));
static int fetch_immed __P((struct frame *frame, struct instruction *insn,
			    int *dst));
static int fetch_disp __P((struct frame *frame, struct instruction *insn,
			   int size, int *res));
static int calc_ea __P((struct insn_ea *ea, char *ptr, char **eaddr));

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
    int data, sig;

#ifdef DEBUG
    if (insn->is_datasize < 0) {
	panic("decode_ea: called with uninitialized datasize\n");
    }
#endif

    sig = 0;

    /* Set the most common value here. */
    ea->ea_regnum = 8 + (modreg & 7);

    switch (modreg & 070) {
    case 0:			/* Dn */
	ea->ea_regnum &= 7;
    case 010:			/* An */
	ea->ea_flags = EA_DIRECT;
	if (fpu_debug_level & DL_DECODEEA) {
	    printf("  decode_ea: register direct reg=%d\n", ea->ea_regnum);
	}
	break;

    case 020:			/* (An) */
	ea->ea_flags = 0;
	if (fpu_debug_level & DL_DECODEEA) {
	    printf("  decode_ea: register indirect reg=%d\n", ea->ea_regnum);
	}
	break;

    case 030:			/* (An)+ */
	ea->ea_flags = EA_POSTINCR;
	if (fpu_debug_level & DL_DECODEEA) {
	    printf("  decode_ea: reg indirect postincrement reg=%d\n",
		   ea->ea_regnum);
	}
	break;

    case 040:			/* -(An) */
	ea->ea_flags = EA_PREDECR;
	if (fpu_debug_level & DL_DECODEEA) {
	    printf("  decode_ea: reg indirect predecrement reg=%d\n",
		   ea->ea_regnum);
	}
	break;

    case 050:			/* (d16,An) */
	ea->ea_flags = EA_OFFSET;
	sig = fetch_disp(frame, insn, 1, &ea->ea_offset);
	if (fpu_debug_level & DL_DECODEEA) {
	    printf("  decode_ea: reg indirect with displacement reg=%d\n",
		   ea->ea_regnum);
	}
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
	    if (fpu_debug_level & DL_DECODEEA) {
		printf("  decode_ea: absolute address (word)\n");
	    }
	    break;

	case 1:			/* (xxxxxxxx).L */
	    ea->ea_flags = EA_ABS;
	    sig = fetch_disp(frame, insn, 2, &ea->ea_absaddr);
	    if (fpu_debug_level & DL_DECODEEA) {
		printf("  decode_ea: absolute address (long)\n");
	    }
	    break;

	case 2:			/* (d16,PC) */
	    ea->ea_flags = EA_PC_REL | EA_OFFSET;
	    sig = fetch_disp(frame, insn, 1, &ea->ea_absaddr);
	    if (fpu_debug_level & DL_DECODEEA) {
		printf("  decode_ea: pc relative word displacement\n");
	    }
	    break;

	case 3:			/* (d8,PC,Xn) */
	    ea->ea_flags = EA_PC_REL | EA_INDEXED;
	    sig = decode_ea6(frame, insn, ea, modreg);
	    break;

	case 4:			/* #data */
	    ea->ea_flags = EA_IMMED;
	    sig = fetch_immed(frame, insn, &ea->ea_immed[0]);
	    if (fpu_debug_level & DL_DECODEEA) {
		printf("  decode_ea: immediate size=%d\n", insn->is_datasize);
	    }
	    break;

	default:
	    if (fpu_debug_level & DL_DECODEEA) {
		printf("  decode_ea: invalid addr mode (7,%d)\n", modreg & 7);
	    }
	    return SIGILL;
	} /* switch for mode 7 */
	break;
    } /* switch mode */

    ea->ea_tdisp = 0;

    return sig;
}

/*
 * Decode Mode=6 address modes
 */
static int
decode_ea6(frame, insn, ea, modreg)
     struct frame *frame;
     struct instruction *insn;
     struct insn_ea *ea;
     int modreg;
{
    int word, extword, idx;
    int basedisp, outerdisp;
    int bd_size, od_size;
    int sig;

    extword = fusword(frame->f_pc + insn->is_advance);
    if (extword < 0) {
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
    idx <<= ((extword >>9) & 3);

    if ((extword & 0x100) == 0) {
	/* brief extention word - sign-extend the displacement */
	basedisp = (extword & 0xff);
	if (basedisp & 0x80) {
	    basedisp |= 0xffffff00;
	}

	ea->ea_basedisp = idx + basedisp;
	ea->ea_outerdisp = 0;
	if (fpu_debug_level & DL_DECODEEA) {
	    printf("  decode_ea6: brief ext word idxreg=%d, basedisp=%08x\n",
		   ea->ea_idxreg, ea->ea_basedisp);
	}
    } else {
	/* full extention word */
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
	    printf("  decode_ea6: invalid indirect mode: ext word %04x\n",
		   extword);
#endif
	    return SIGILL;
	    break;
	}
	if (fpu_debug_level & DL_DECODEEA) {
	    printf("  decode_ea6: full ext idxreg=%d, basedisp=%x, outerdisp=%x\n",
		   ea->ea_idxreg, ea->ea_basedisp, ea->ea_outerdisp);
	}
    }
    if (fpu_debug_level & DL_DECODEEA) {
	printf("  decode_ea6: regnum=%d, flags=%x\n",
	       ea->ea_regnum, ea->ea_flags);
    }
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
    int data, word, sig;

#ifdef	DIAGNOSTIC
    if (ea->ea_regnum & ~0xF) {
	panic("  load_ea: bad regnum");
    }
#endif

    if (fpu_debug_level & DL_LOADEA) {
	printf("  load_ea: frame at %08x\n", frame);
    }
    /* The dst is always int or larger. */
    len = insn->is_datasize;
    if (len < 4) {
	dst += (4 - len);
    }
    step = (len == 1 && ea->ea_regnum == 15 /* sp */) ? 2 : len;

    if (ea->ea_flags & EA_DIRECT) {
	if (len > 4) {
#ifdef DEBUG
	    printf("  load_ea: operand doesn't fit cpu reg\n");
#endif
	    return SIGILL;
	}
	if (ea->ea_tdisp > 0) {
#ifdef DEBUG
	    printf("  load_ea: more than one move from cpu reg\n");
#endif
	    return SIGILL;
	}
	src = (char *)&frame->f_regs[ea->ea_regnum];
	/* The source is an int. */
	if (len < 4) {
	    src += (4 - len);
	    if (fpu_debug_level & DL_LOADEA) {
		printf("  load_ea: short/byte opr - addr adjusted\n");
	    }
	}
	if (fpu_debug_level & DL_LOADEA) {
	    printf("  load_ea: src 0x%08x\n", src);
	}
	bcopy(src, dst, len);
    } else if (ea->ea_flags & EA_IMMED) {
	if (fpu_debug_level & DL_LOADEA) {
	    printf("  load_ea: immed %08x%08x%08x size %d\n",
		   ea->ea_immed[0], ea->ea_immed[1], ea->ea_immed[2], len);
	}
	src = (char *)&ea->ea_immed[0];
	if (len < 4) {
	    src += (4 - len);
	    if (fpu_debug_level & DL_LOADEA) {
		printf("  load_ea: short/byte immed opr - addr adjusted\n");
	    }
	}
	bcopy(src, dst, len);
    } else if (ea->ea_flags & EA_ABS) {
	if (fpu_debug_level & DL_LOADEA) {
	    printf("  load_ea: abs addr %08x\n", ea->ea_absaddr);
	}
	src = (char *)ea->ea_absaddr;
	copyin(src, dst, len);
    } else /* register indirect */ { 
	if (ea->ea_flags & EA_PC_REL) {
	    if (fpu_debug_level & DL_LOADEA) {
		printf("  load_ea: using PC\n");
	    }
	    reg = NULL;
	    /* Grab the register contents. 4 is offset to the first
	       extention word from the opcode */
	    src = (char *)frame->f_pc + 4;
	    if (fpu_debug_level & DL_LOADEA) {
		printf("  load_ea: pc relative pc+4 = 0x%08x\n", src);
	    }
	} else /* not PC relative */ {
	    if (fpu_debug_level & DL_LOADEA) {
		printf("  load_ea: using register %c%d\n",
		       (ea->ea_regnum >= 8) ? 'a' : 'd', ea->ea_regnum & 7);
	    }
	    /* point to the register */
	    reg = &frame->f_regs[ea->ea_regnum];

	    if (ea->ea_flags & EA_PREDECR) {
		if (fpu_debug_level & DL_LOADEA) {
		    printf("  load_ea: predecr mode - reg decremented\n");
		}
		*reg -= step;
		ea->ea_tdisp = 0;
	    }

	    /* Grab the register contents. */
	    src = (char *)*reg;
	    if (fpu_debug_level & DL_LOADEA) {
		printf("  load_ea: reg indirect reg = 0x%08x\n", src);
	    }
	}

	sig = calc_ea(ea, src, &src);
	if (sig)
	    return sig;

	copyin(src + ea->ea_tdisp, dst, len);

	/* do post-increment */
	if (ea->ea_flags & EA_POSTINCR) {
	    if (ea->ea_flags & EA_PC_REL) {
#ifdef DEBUG
		printf("  load_ea: tried to postincrement PC\n");
#endif
		return SIGILL;
	    }
	    *reg += step;
	    ea->ea_tdisp = 0;
	    if (fpu_debug_level & DL_LOADEA) {
		printf("  load_ea: postinc mode - reg incremented\n");
	    }
	} else {
	    ea->ea_tdisp += len;
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
    int data, word, sig;

#ifdef	DIAGNOSTIC
    if (ea->ea_regnum & ~0xF) {
	panic("  store_ea: bad regnum");
    }
#endif

    if (ea->ea_flags & (EA_IMMED|EA_PC_REL)) {
	/* not alterable address mode */
#ifdef DEBUG
	printf("  store_ea: not alterable address mode\n");
#endif
	return SIGILL;
    }

    if (fpu_debug_level & DL_STOREEA) {
	printf("  store_ea: frame at %08x\n", frame);
    }
    /* The src is always int or larger. */
    len = insn->is_datasize;
    if (len < 4) {
	src += (4 - len);
    }
    step = (len == 1 && ea->ea_regnum == 15 /* sp */) ? 2 : len;

    if (ea->ea_flags & EA_ABS) {
	if (fpu_debug_level & DL_STOREEA) {
	    printf("  store_ea: abs addr %08x\n", ea->ea_absaddr);
	}
	dst = (char *)ea->ea_absaddr;
	copyout(src, dst + ea->ea_tdisp, len);
	ea->ea_tdisp += len;
    } else if (ea->ea_flags & EA_DIRECT) {
	if (len > 4) {
#ifdef DEBUG
	    printf("  store_ea: operand doesn't fit cpu reg\n");
#endif
	    return SIGILL;
	}
	if (ea->ea_tdisp > 0) {
#ifdef DEBUG
	    printf("  store_ea: more than one move to cpu reg\n");
#endif
	    return SIGILL;
	}
	dst = (char*)&frame->f_regs[ea->ea_regnum];
	/* The destination is an int. */
	if (len < 4) {
	    dst += (4 - len);
	    if (fpu_debug_level & DL_STOREEA) {
		printf("  store_ea: short/byte opr - dst addr adjusted\n");
	    }
	}
	if (fpu_debug_level & DL_STOREEA) {
	    printf("  store_ea: dst 0x%08x\n", dst);
	}
	bcopy(src, dst, len);
    } else /* One of MANY indirect forms... */ {
	if (fpu_debug_level & DL_STOREEA) {
	    printf("  store_ea: using register %c%d\n",
		   (ea->ea_regnum >= 8) ? 'a' : 'd', ea->ea_regnum & 7);
	}
	/* point to the register */
	reg = &(frame->f_regs[ea->ea_regnum]);

	/* do pre-decrement */
	if (ea->ea_flags & EA_PREDECR) {
	    if (fpu_debug_level & DL_STOREEA) {
		printf("  store_ea: predecr mode - reg decremented\n");
	    }
	    *reg -= step;
	    ea->ea_tdisp = 0;
	}

	/* calculate the effective address */
	sig = calc_ea(ea, (char *)*reg, &dst);
	if (sig)
	    return sig;

	if (fpu_debug_level & DL_STOREEA) {
	    printf("  store_ea: dst addr=0x%08x+%d\n", dst, ea->ea_tdisp);
	}
	copyout(src, dst + ea->ea_tdisp, len);

	/* do post-increment */
	if (ea->ea_flags & EA_POSTINCR) {
	    *reg += step;
	    ea->ea_tdisp = 0;
	    if (fpu_debug_level & DL_STOREEA) {
		printf("  store_ea: postinc mode - reg incremented\n");
	    }
	} else {
	    ea->ea_tdisp += len;
	}
    }

    return 0;
}

/*
 * fetch_immed: fetch immediate operand
 */
static int
fetch_immed(frame, insn, dst)
     struct frame *frame;
     struct instruction *insn;
     int *dst;
{
    int data, ext_bytes;

    ext_bytes = insn->is_datasize;

    if (0 < ext_bytes) {
	data = fusword(frame->f_pc + insn->is_advance);
	if (data < 0) {
	    return SIGSEGV;
	}
	if (ext_bytes == 1) {
	    /* sign-extend byte to long */
	    data &= 0xff;
	    if (data & 0x80) {
		data |= 0xffffff00;
	    }
	} else if (ext_bytes == 2) {
	    /* sign-extend word to long */
	    data &= 0xffff;
	    if (data & 0x8000) {
		data |= 0xffff0000;
	    }
	}
	insn->is_advance += 2;
	dst[0] = data;
    }
    if (2 < ext_bytes) {
	data = fusword(frame->f_pc + insn->is_advance);
	if (data < 0) {
	    return SIGSEGV;
	}
	insn->is_advance += 2;
	dst[0] <<= 16;
	dst[0] |= data;
    }
    if (4 < ext_bytes) {
	data = fusword(frame->f_pc + insn->is_advance);
	if (data < 0) {
	    return SIGSEGV;
	}
	dst[1] = data << 16;
	data = fusword(frame->f_pc + insn->is_advance + 2);
	if (data < 0) {
	    return SIGSEGV;
	}
	insn->is_advance += 4;
	dst[1] |= data;
    }
    if (8 < ext_bytes) {
	data = fusword(frame->f_pc + insn->is_advance);
	if (data < 0) {
	    return SIGSEGV;
	}
	dst[2] = data << 16;
	data = fusword(frame->f_pc + insn->is_advance + 2);
	if (data < 0) {
	    return SIGSEGV;
	}
	insn->is_advance += 4;
	dst[2] |= data;
    }

    return 0;
}

/*
 * fetch_disp: fetch displacement in full extention words
 */
static int
fetch_disp(frame, insn, size, res)
     struct frame *frame;
     struct instruction *insn;
     int size, *res;
{
    int disp, word;

    if (size == 1) {
	word = fusword(frame->f_pc + insn->is_advance);
	if (word < 0) {
	    return SIGSEGV;
	}
	disp = word & 0xffff;
	if (disp & 0x8000) {
	    /* sign-extend */
	    disp |= 0xffff0000;
	}
	insn->is_advance += 2;
    } else if (size == 2) {
	word = fusword(frame->f_pc + insn->is_advance);
	if (word < 0) {
	    return SIGSEGV;
	}
	disp = word << 16;
	word = fusword(frame->f_pc + insn->is_advance + 2);
	if (word < 0) {
	    return SIGSEGV;
	}
	disp |= (word & 0xffff);
	insn->is_advance += 4;
    } else {
	disp = 0;
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
static int
calc_ea(ea, ptr, eaddr)
     struct insn_ea *ea;
     char *ptr;		/* base address (usually a register content) */
     char **eaddr;	/* pointer to result pointer */
{
    int data, word, sig;

    if (fpu_debug_level & DL_EA) {
	printf("  calc_ea: reg indirect (reg) = 0x%08x\n", ptr);
    }

    if (ea->ea_flags & EA_OFFSET) {
	/* apply the signed offset */
	if (fpu_debug_level & DL_EA) {
	    printf("  calc_ea: offset %d\n", ea->ea_offset);
	}
	ptr += ea->ea_offset;
    } else if (ea->ea_flags & EA_INDEXED) {
	if (fpu_debug_level & DL_EA) {
	    printf("  calc_ea: indexed mode\n");
	}

	if (ea->ea_flags & EA_BASE_SUPPRSS) {
	    /* base register is suppressed */
	    ptr = (char *)ea->ea_basedisp;
	} else {
	    ptr += ea->ea_basedisp;
	}

	if (ea->ea_flags & EA_MEM_INDIR) {
	    if (fpu_debug_level & DL_EA) {
		printf("  calc_ea: mem indir mode: basedisp=%08x, outerdisp=%08x\n",
		       ea->ea_basedisp, ea->ea_outerdisp);
		printf("  calc_ea: addr fetched from 0x%08x\n", ptr);
	    }
	    /* memory indirect modes */
	    word = fusword(ptr);
	    if (word < 0) {
		return SIGSEGV;
	    }
	    word <<= 16;
	    data = fusword(ptr + 2);
	    if (data < 0) {
		return SIGSEGV;
	    }
	    word |= data;
	    if (fpu_debug_level & DL_STOREEA) {
		printf(" calc_ea: fetched ptr 0x%08x\n", word);
	    }
	    ptr = (char *)word + ea->ea_outerdisp;
	}
    }

    *eaddr = ptr;

    return 0;
}
