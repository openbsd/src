/*	$NetBSD: fpu_emulate.c,v 1.2 1995/03/10 01:43:05 gwr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * mc68881 emulator
 * XXX - Just a start at it for now...
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <machine/frame.h>

#define	DEBUG 1	/* XXX */

/*
 * Internal info about a decoded effective address.
 */
struct insn_ea {
	int regnum;
	int immed;
	int flags;
#define	EA_DIRECT	0x01
#define EA_PREDECR	0x02
#define	EA_POSTINCR	0x04
#define EA_OFFSET	0x08	/* mode 5: base+offset */
#define	EA_INDEXED	0x10	/* mode 6: complicated */
#define EA_ABS  	0x20	/* mode 7: reg 0 or 1 */
#define EA_PC_REL	0x40	/* mode 7: reg 2 or 3 */
#define	EA_IMMED	0x80	/* mode 7: reg 4 */
};

struct instruction {
	int advance;	/* length of instruction */
	int datasize;	/* byte, word, long, float, double, ... */
	int	opcode;
	int word1;
	struct insn_ea ea0;
	struct insn_ea ea1;
};

int fpu_emul_fmovm(struct frame *frame,
				   struct fpframe *fpf,
				   struct instruction *insn);
int fpu_emul_type0(struct frame *frame,
				   struct fpframe *fpf,
				   struct instruction *insn);
int fpu_emul_type1(struct frame *frame,
				   struct fpframe *fpf,
				   struct instruction *insn);
int fpu_emul_brcc(struct frame *frame,
				  struct fpframe *fpf,
				  struct instruction *insn);

static int decode_ea(struct frame *frame,
					 struct instruction *insn,
					 struct insn_ea *ea,
					 int modreg);
static int load_ea(struct frame *frame,
				   struct instruction *insn,
				   struct insn_ea *ea,
				   char *cpureg);
static int store_ea(struct frame *frame,
					struct instruction *insn,
					struct insn_ea *ea,
					char *cpureg);


/*
 * Emulate a floating-point instruction.
 * Return zero for success, else signal number.
 * (Typically: zero, SIGFPE, SIGILL, SIGSEGV)
 */
int fpu_emulate(struct frame *frame, struct fpframe *fpf)
{
	struct instruction insn;
	int word, optype, sig;

	word = fusword(frame->f_pc);
	if (word < 0) {
#ifdef	DEBUG
		printf("fpu_emulate: fault reading opcode\n");
#endif
		return SIGSEGV;
	}

	if ((word & 0xF000) != 0xF000) {
#ifdef	DEBUG
		printf("fpu_emulate: not coproc. insn.: opcode=0x%x\n", word);
#endif
		return SIGILL;
	}

	if ((word & 0x0E00) != 0x0200) {
#ifdef	DEBUG
		printf("fpu_emulate: bad coproc. id: opcode=0x%x\n", word);
#endif
		return SIGILL;
	}

	insn.opcode = word;
	optype = (word & 0x01C0);

	word = fusword(frame->f_pc + 2);
	if (word < 0) {
#ifdef	DEBUG
		printf("fpu_emulate: fault reading word1\n");
#endif
		return SIGSEGV;
	}
	insn.word1 = word;

	/*
	 * Which family (or type) of opcode is it?
	 * Tests ordered by likelihood (hopefully).
	 * Certainly, type 0 is the most common.
	 */
	if (optype == 0x0000) {
		/* type=0: generic */
		if (insn.word1 & 0x8000) {
			sig = fpu_emul_fmovm(frame, fpf, &insn);
		} else {
			sig = fpu_emul_type0(frame, fpf, &insn);
		}
	}
	else if (optype == 0x0080) {
		/* type=2: fbcc, short disp. */
		sig = fpu_emul_brcc(frame, fpf, &insn);
	}
	else if (optype == 0x00C0) {
		/* type=3: fbcc, long disp. */
		sig = fpu_emul_brcc(frame, fpf, &insn);
	}
	else if (optype == 0x0040) {
		/* type=1: fdbcc, fscc, ftrapcc */
		sig = fpu_emul_type1(frame, fpf, &insn);
	}
	else {
		/* type=4: fsave    (privileged) */
		/* type=5: frestore (privileged) */
		/* type=6: reserved */
		/* type=7: reserved */
#ifdef	DEBUG
		printf("fpu_emulate: bad opcode type: opcode=0x%x\n", insn.opcode);
#endif
		sig = SIGILL;
	}

	if (sig == 0) {
		frame->f_pc += insn.advance;
	}
#if defined(DDB) && defined(DEBUG)
	else kdb_trap(-1, frame);
#endif

	return (sig);
}

/*
 * type 0: fmovem, fmove <cr>
 * Separated out of fpu_emul_type0 for efficiency.
 * In this function, we know:
 *   (opcode & 0x01C0) == 0
 *   (word1 & 0x8000) == 0x8000
 *
 * No conversion or rounding is done by this instruction,
 * and the FPSR is not affected.
 */
int fpu_emul_fmovm(struct frame *frame,
				   struct fpframe *fpf,
				   struct instruction *insn)
{
	int word1, sig;
	int reglist, regmask, regnum;
	int fpu_to_mem, order;
	int w1_post_incr;	/* XXX - FP regs order? */
	int *fpregs;

	insn->advance = 4;
	insn->datasize = 12;
	word1 = insn->word1;

	/* Bit 14 selects FPn or FP control regs. */
	if (word1 & 0x4000) {
		/*
		 * Bits 12,11 select register list mode:
		 * 0,0: Static  reg list, pre-decr.
		 * 0,1: Dynamic reg list, pre-decr.
		 * 1,0: Static  reg list, post-incr.
		 * 1,1: Dynamic reg list, post-incr
		 */
		w1_post_incr = word1 & 0x1000;
		if (word1 & 0x0800) {
			/* dynamic reg list */
			reglist = frame->f_regs[(word1 & 0x70) >> 4];
		} else
			reglist = word1;
		reglist &= 0xFF;
	} else {
		/* XXX: move to/from control registers */
		reglist = word1 & 0x1C00;
		return SIGILL;
	}

	/* Bit 13 selects direction (FPU to/from Mem) */
	fpu_to_mem = word1 & 0x2000;

	/* Get effective address. (modreg=opcode&077) */
	sig = decode_ea(frame, insn, &insn->ea0, insn->opcode);
	if (sig) return sig;

	/* Get address of soft coprocessor regs. */
	fpregs = &fpf->fpf_regs[0];

	if (insn->ea0.flags & EA_PREDECR) {
		regnum = 7;
		order = -1;
	} else {
		regnum = 0;
		order = 1;
	}

	while ((0 <= regnum) && (regnum < 8)) {
		regmask = 1 << regnum;
		if (regmask & reglist) {
			if (fpu_to_mem)
				sig = store_ea(frame, insn, &insn->ea0,
							   (char*) &fpregs[regnum]);
			else /* mem to fpu */
				sig = load_ea(frame, insn, &insn->ea0,
							  (char*) &fpregs[regnum]);
			if (sig) break;
		}
		regnum += order;
	}

	return 0;
}

int fpu_emul_type0(struct frame *frame,
				   struct fpframe *fpf,
				   struct instruction *insn)
{
	int sig;

	/* Get effective address */
	/* XXX */

	switch(insn->word1 & 0x3F) {

	case 0x00:	/* fmove */

	case 0x01:	/* fint */
	case 0x02:	/* fsinh */
	case 0x03:	/* fintrz */
	case 0x04:	/* fsqrt */
	case 0x06:	/* flognp1 */

	case 0x09:	/* ftanh */
	case 0x0A:	/* fatan */
	case 0x0C:	/* fasin */
	case 0x0D:	/* fatanh */
	case 0x0E:	/* fsin */
	case 0x0F:	/* ftan */

	case 0x10:	/* fetox */
	case 0x11:	/* ftwotox */
	case 0x12:	/* ftentox */
	case 0x14:	/* flogn */
	case 0x15:	/* flog10 */
	case 0x16:	/* flog2 */

	case 0x18:	/* fabs */
	case 0x19:	/* fcosh */
	case 0x1A:	/* fneg */
	case 0x1C:	/* facos */
	case 0x1D:	/* fcos */
	case 0x1E:	/* fgetexp */
	case 0x1F:	/* fgetman */

	case 0x20:	/* fdiv */
	case 0x21:	/* fmod */
	case 0x22:	/* fadd */
	case 0x23:	/* fmul */
	case 0x24:	/* fsgldiv */
	case 0x25:	/* frem */
	case 0x26:	/* fscale */
	case 0x27:	/* fsglmul */

	case 0x28:	/* fsub */
	case 0x38:	/* fcmp */
	case 0x3A:	/* ftst */

	default:
#ifdef	DEBUG
		printf("fpu_emul_type0: unknown: opcode=0x%x, word1=0x%x\n",
			   insn->opcode, insn->word1);
#endif
		sig = SIGILL;

	} /* switch */
	return (sig);
}

/*
 * type 1: fdbcc, fscc, ftrapcc
 * In this function, we know:
 *   (opcode & 0x01C0) == 0x0040
 */
int fpu_emul_type1(struct frame *frame,
				   struct fpframe *fpf,
				   struct instruction *insn)
{
	int sig;

	/* Get effective address */
	/* XXX */

	switch (insn->opcode & 070) {

	case 010:	/* fdbcc */
		/* XXX: If not CC { Decrement Dn; if (Dn >= 0) branch; } */

	case 070:	/* fscc or ftrapcc */
		if ((insn->opcode & 07) > 1) {
			/* ftrapcc */
			/* XXX: If CC, advance and return SIGFPE */
			break;
		}
		/* fallthrough */
	default:	/* fscc */
		/* XXX: If CC, store ones, else store zero */
		sig = SIGILL;
		break;

	}
	return (sig);
}

/*
 * Type 2 or 3: fbcc (also fnop)
 * In this function, we know:
 *   (opcode & 0x0180) == 0x0080
 */
int fpu_emul_brcc(struct frame *frame,
				  struct fpframe *fpf,
				  struct instruction *insn)
{
	int displ, word2;
	int sig, advance;

	/*
	 * Get branch displacement.
	 */
	advance = 4;
	displ = insn->word1;
	if (displ & 0x8000)
		displ |= 0xFFFF0000;

	if (insn->opcode & 0x40) {
		word2 = fusword(frame->f_pc + 4);
		if (word2 < 0) {
#ifdef	DEBUG
			printf("fpu_emul_brcc: fault reading word2\n");
#endif
			return SIGSEGV;
		}
		displ << 16;
		displ |= word2;
		advance += 2;
	}

	/* XXX: If CC, frame->f_pc += displ */
	return SIGILL;
}

/*
 * Helper routines for dealing with "effective address" values.
 */

/*
 * Decode an effective address into internal form.
 * Returns zero on success, else signal number.
 */
static int decode_ea(struct frame *frame,
					 struct instruction *insn,
					 struct insn_ea *ea,
					 int modreg)
{
	int immed_bytes = 0;
	int data;

	/* Set the most common value here. */
	ea->regnum = 8 + (modreg & 7);

	switch (modreg & 070) {

	case 0:	/* Dn */
		ea->regnum = (modreg & 7);
		ea->flags = EA_DIRECT;
		break;

	case 010:	/* An */
		ea->flags = EA_DIRECT;
		break;

	case 020:	/* (An) */
		ea->flags = 0;
		break;

	case 030: /* (An)+ */
		ea->flags = EA_POSTINCR;
		break;

	case 040: /* -(An) */
		ea->flags = EA_PREDECR;
		break;

	case 050: /* (d16,An) */
		ea->flags = EA_OFFSET;
		immed_bytes = 2;
		break;

	case 060:	/* (d8,An,Xn) */
		ea->flags = EA_INDEXED;
		immed_bytes = 2;
		break;

	case 070:	/* misc. */
		ea->regnum = (modreg & 7);
		switch (modreg & 7) {

		case 0: /* (xxxx).W */
			ea->flags = EA_ABS;
			immed_bytes = 2;
			break;

		case 1: /* (xxxxxxxx).L */
			ea->flags = EA_ABS;
			immed_bytes = 4;
			break;

		case 2: /* (d16,PC) */
			ea->flags = EA_PC_REL | EA_OFFSET;
			immed_bytes = 2;
			break;

		case 3: /* (d8,PC,Xn) */
			ea->flags = EA_PC_REL | EA_INDEXED;
			immed_bytes = 2;
			break;

		case 4: /* #data */
			ea->flags = EA_IMMED;
			immed_bytes = insn->datasize;
			break;

		default:
			return SIGILL;
		} /* switch for mode 7 */
		break;
	} /* switch mode */

	/* Now fetch any immediate data and advance. */
	if (immed_bytes > 0) {
		data = fusword(frame->f_pc + insn->advance);
		if (data < 0)
			return SIGSEGV;
		insn->advance += 2;
		if (data & 0x8000)
			data |= 0xFFFF0000;
		ea->immed = data;
	}
	if (immed_bytes > 2) {
		data = fusword(frame->f_pc + insn->advance);
		if (data < 0)
			return SIGSEGV;
		insn->advance += 2;
		ea->immed <<= 16;
		ea->immed |= data;
	}
	return 0;
}


/*
 * Load a value from an effective address.
 * Returns zero on success, else signal number.
 */
static int load_ea(struct frame *frame,
				   struct instruction *insn,
				   struct insn_ea *ea,
				   char *dst)
{
	int *reg;
	char *src;
	int len;

#ifdef	DIAGNOSTIC
	if (ea->regnum & ~0xF)
		panic("load_ea: bad regnum");
#endif

	/* The dst is always int or larger. */
	len = insn->datasize;
	if (len < 4)
		dst += (4 - len);

	/* point to the register */
	if (ea->flags & EA_PC_REL)
		reg = &frame->f_pc;
	else
		reg = &frame->f_regs[ea->regnum];

	if (ea->flags & (EA_DIRECT | EA_IMMED)) {
		if (ea->flags & EA_DIRECT)
			src = (char*) reg;
		if (ea->flags & EA_IMMED)
			src = (char*) &ea->immed;
		if (len > 4)
			return SIGILL;
		/* The source is an int. */
		if (len < 4)
			src += (4 - len);
		bcopy(src, dst, len);
	} else {
		/* One of MANY indirect forms... */

		/* do pre-decrement */
		if (ea->flags & EA_PREDECR)
			*reg -= len;

		/* Grab the register contents. */
		src = (char*) *reg;

		/* apply the signed offset */
		if (ea->flags & EA_OFFSET)
			src += ea->immed;

		/* XXX - Don't know how to handle this yet. */
		if (ea->flags & EA_INDEXED)
			return SIGILL;

		copyin(src, dst, len);

		/* do post-increment */
		if (ea->flags & EA_POSTINCR)
			*reg += len;
	}

	return 0;
}

/*
 * Store a value at the effective address.
 * Returns zero on success, else signal number.
 */
static int store_ea(struct frame *frame,
					struct instruction *insn,
					struct insn_ea *ea,
					char *src)
{
	int *reg;
	char *dst;
	int len;

#ifdef	DIAGNOSTIC
	if (ea->regnum & ~0xF)
		panic("load_ea: bad regnum");
#endif

	/* The src is always int or larger. */
	len = insn->datasize;
	if (len < 4)
		src += (4 - len);

	/* point to the register */
	if (ea->flags & EA_PC_REL)
		reg = &frame->f_pc;
	else
		reg = &frame->f_regs[ea->regnum];

	if (ea->flags & EA_IMMED)
		return SIGILL;

	if (ea->flags & EA_DIRECT) {
		dst = (char*) reg;
		if (len > 4)
			return SIGILL;
		/* The destination is an int. */
		if (len < 4)
			dst += (4 - len);
		bcopy(src, dst, len);
	} else {
		/* One of MANY indirect forms... */

		/* do pre-decrement */
		if (ea->flags & EA_PREDECR)
			*reg -= len;

		/* Grab the register contents. */
		dst = (char*) *reg;

		/* apply the signed offset */
		if (ea->flags & EA_OFFSET)
			dst += ea->immed;

		/* XXX - Don't know how to handle this yet. */
		if (ea->flags & EA_INDEXED)
			return SIGILL;

		copyout(src, dst, len);

		/* do post-increment */
		if (ea->flags & EA_POSTINCR)
			*reg += len;
	}

	return 0;
}
