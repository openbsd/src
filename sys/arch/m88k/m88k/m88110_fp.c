/*	$OpenBSD: m88110_fp.c,v 1.4 2011/07/06 21:41:37 art Exp $	*/

/*
 * Copyright (c) 2007, Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/ieeefp.h>
#include <machine/trap.h>
#include <machine/m88110.h>

#include <lib/libkern/softfloat.h>

/*
 * Values for individual bits in fcmp results.
 */
#define	CC_UN	0x00000001	/* unordered */
#define	CC_LEG	0x00000002	/* less than, equal or greater than */
#define	CC_EQ	0x00000004	/* equal */
#define	CC_NE	0x00000008	/* not equal */
#define	CC_GT	0x00000010	/* greater than */
#define	CC_LE	0x00000020	/* less than or equal */
#define	CC_LT	0x00000040	/* less than */
#define	CC_GE	0x00000080	/* greater than or equal */
#define	CC_OU	0x00000100	/* out of range */
#define	CC_IB	0x00000200	/* in range or on boundary */
#define	CC_IN	0x00000400	/* in range */
#define	CC_OB	0x00000800	/* out of range or on boundary */
#define	CC_UE	0x00001000	/* unordered or equal */
#define	CC_LG	0x00002000	/* less than or greater than */
#define	CC_UG	0x00004000	/* unordered or greater than */
#define	CC_ULE	0x00008000	/* unordered or less than or equal */
#define	CC_UL	0x00010000	/* unordered or less than */
#define	CC_UGE	0x00020000	/* unordered or greater than or equal */

/*
 * Data width (matching the TD field of the instructions)
 */
#define	FTYPE_SNG	0
#define	FTYPE_DBL	1
#define	FTYPE_EXT	2
#define	FTYPE_INT	3	/* not a real T value */

#define	IGNORE_PRECISION	FTYPE_SNG

/* floating point value */
typedef union {
	float32		sng;
	float64		dbl;
} fparg;

void	fpu_compare(struct trapframe *, fparg *, fparg *, u_int, u_int, u_int);
int	fpu_emulate(struct trapframe *, u_int32_t);
void	fpu_fetch(struct trapframe *, u_int, u_int, u_int, fparg *);
u_int	fpu_precision(u_int, u_int, u_int);
void	fpu_store(struct trapframe *, u_int, u_int, u_int, fparg *);

/*
 * Inlines from softfloat-specialize.h which are not made public, needed
 * for fpu_compare.
 */
#define	float32_is_nan(a) \
	(0xff000000 < (a << 1))
#define	float32_is_signaling_nan(a) \
	((((a >> 22) & 0x1ff) == 0x1fe) && (a & 0x003fffff))

/*
 * All 88110 floating-point exceptions are handled there.
 *
 * We can unfortunately not trust the floating-point exception cause
 * register, as the 88110 will conveniently only set the ``unimplemented
 * instruction'' bit, more often than not.
 *
 * So we ignore it completely, and try to emulate the faulting instruction.
 * The instruction can be:
 *
 * - an invalid SFU1 opcode, in which case we'll send SIGILL to the process.
 *
 * - a genuinely unimplemented feature: fsqrt.
 *
 * - an opcode involving an odd-numbered register pair (as a double precision
 *   operand). Rather than issueing a correctly formed flavour in kernel mode,
 *   and having to handle a possible nested exception, we emulate it. This
 *   will of course be slower, but we have to draw the line somewhere.
 *   Gcc will however never produce such code, so we don't have to worry
 *   too much about this under OpenBSD.
 *
 * Note that, currently, opcodes involving the extended register file (XRF)
 * are handled as invalid opcodes. This will eventually change once the
 * toolchain can correctly assemble XRF instructions, and the XRF is saved
 * accross context switches (or not... lazy switching for XRF makes more
 * sense).
 */

void
m88110_fpu_exception(struct trapframe *frame)
{
	struct proc *p = curproc;
	int fault_type;
	vaddr_t fault_addr;
	union sigval sv;
	u_int32_t insn;
	int sig;

	fault_addr = frame->tf_exip & XIP_ADDR;

	/*
	 * Skip the instruction now. Signals will blame the correct
	 * address, and this has to be done before trapsignal() is
	 * invoked, or we won't run the first instruction of the signal
	 * handler...
	 */
	m88110_skip_insn(frame);

	/*
	 * The low-level exception code did not save the floating point
	 * exception registers. Do it now, and reset the exception
	 * cause register.
	 */
	__asm__ __volatile__ ("fldcr %0, fcr0" : "=r"(frame->tf_fpecr));
	__asm__ __volatile__ ("fldcr %0, fcr62" : "=r"(frame->tf_fpsr));
	__asm__ __volatile__ ("fldcr %0, fcr63" : "=r"(frame->tf_fpcr));
	__asm__ __volatile__ ("fstcr r0, fcr0");

	/*
	 * Fetch the faulting instruction. This should not fail, if it
	 * does, it's probably not your lucky day.
	 */
	if (copyin((void *)fault_addr, &insn, sizeof insn) != 0) {
		sig = SIGBUS;
		fault_type = BUS_OBJERR;
		goto deliver;
	}

	switch (insn >> 26) {
	case 0x20:
		/*
		 * f{ld,st,x}cr instruction. If it caused a fault in
		 * user mode, this is a privilege violation.
		 */
		sig = SIGILL;
		fault_type = ILL_PRVREG;
		goto deliver;
	case 0x21:
		/*
		 * ``real'' FPU instruction. We'll try to emulate it,
		 * unless FPU is disabled.
		 */
		if (frame->tf_epsr & PSR_SFD1) {	/* don't bother */
			sig = SIGFPE;
			fault_type = FPE_FLTINV;
			goto deliver;
		}
		sig = fpu_emulate(frame, insn);
		fault_type = SI_NOINFO;
		/*
		 * Update the floating point status register regardless of
		 * whether we'll deliver a signal or not.
		 */
		__asm__ __volatile__ ("fstcr %0, fcr62" :: "r"(frame->tf_fpsr));
		break;
	default:
		/*
		 * Not a FPU instruction. Should not have raised this
		 * exception, so bail out.
		 */
		sig = SIGILL;
		fault_type = ILL_ILLOPC;
		goto deliver;
	}

	if (sig != 0) {
		if (sig == SIGILL)
			fault_type = ILL_ILLOPC;
		else {
			if (frame->tf_fpecr & FPECR_FIOV)
				fault_type = FPE_FLTSUB;
			else if (frame->tf_fpecr & FPECR_FROP)
				fault_type = FPE_FLTINV;
			else if (frame->tf_fpecr & FPECR_FDVZ)
				fault_type = FPE_INTDIV;
			else if (frame->tf_fpecr & FPECR_FUNF) {
				if (frame->tf_fpsr & FPSR_EFUNF)
					fault_type = FPE_FLTUND;
				else if (frame->tf_fpsr & FPSR_EFINX)
					fault_type = FPE_FLTRES;
			} else if (frame->tf_fpecr & FPECR_FOVF) {
				if (frame->tf_fpsr & FPSR_EFOVF)
					fault_type = FPE_FLTOVF;
				else if (frame->tf_fpsr & FPSR_EFINX)
					fault_type = FPE_FLTRES;
			} else if (frame->tf_fpecr & FPECR_FINX)
				fault_type = FPE_FLTRES;
		}

deliver:
		sv.sival_ptr = (void *)fault_addr;
		KERNEL_LOCK();
		trapsignal(p, sig, 0, fault_type, sv);
		KERNEL_UNLOCK();
	}
}

/*
 * Load a floating-point argument into a fparg union, then convert it to
 * the required format if it is of larger precision.
 *
 * This assumes the final format (width) is not FTYPE_INT, and the original
 * format (orig_width) <= width.
 */
void
fpu_fetch(struct trapframe *frame, u_int regno, u_int orig_width, u_int width,
    fparg *dest)
{
	u_int32_t tmp;

	switch (orig_width) {
	case FTYPE_INT:
		tmp = regno == 0 ? 0 : frame->tf_r[regno];
		switch (width) {
		case FTYPE_SNG:
			dest->sng = int32_to_float32(tmp);
			break;
		case FTYPE_DBL:
			dest->dbl = int32_to_float64(tmp);
			break;
		}
		break;
	case FTYPE_SNG:
		tmp = regno == 0 ? 0 : frame->tf_r[regno];
		switch (width) {
		case FTYPE_SNG:
			dest->sng = tmp;
			break;
		case FTYPE_DBL:
			dest->dbl = float32_to_float64(tmp);
			break;
		}
		break;
	case FTYPE_DBL:
		tmp = regno == 0 ? 0 : frame->tf_r[regno];
		dest->dbl = ((float64)tmp) << 32;
		tmp = regno == 31 ? 0 : frame->tf_r[regno + 1];
		dest->dbl |= (float64)tmp;
		break;
	}
}

/*
 * Store a floating-point result, converting it to the required format if it
 * is of smaller precision.
 *
 * This assumes the original format (orig_width) is not FTYPE_INT, and the
 * final format (width) <= orig_width.
 */
void
fpu_store(struct trapframe *frame, u_int regno, u_int orig_width, u_int width,
    fparg *src)
{
	u_int32_t tmp;
	u_int rd;

	switch (width) {
	case FTYPE_INT:
		rd = float_get_round(frame->tf_fpcr);
		switch (orig_width) {
		case FTYPE_SNG:
			if (rd == FP_RZ)
				tmp = float32_to_int32_round_to_zero(src->sng);
			else
				tmp = float32_to_int32(src->sng);
			break;
		case FTYPE_DBL:
			if (rd == FP_RZ)
				tmp = float64_to_int32_round_to_zero(src->dbl);
			else
				tmp = float64_to_int32(src->dbl);
			break;
		}
		if (regno != 0)
			frame->tf_r[regno] = tmp;
		break;
	case FTYPE_SNG:
		switch (orig_width) {
		case FTYPE_SNG:
			tmp = src->sng;
			break;
		case FTYPE_DBL:
			tmp = float64_to_float32(src->dbl);
			break;
		}
		if (regno != 0)
			frame->tf_r[regno] = tmp;
		break;
	case FTYPE_DBL:
		switch (orig_width) {
		case FTYPE_DBL:
			tmp = (u_int32_t)(src->dbl >> 32);
			if (regno != 0)
				frame->tf_r[regno] = tmp;
			tmp = (u_int32_t)src->dbl;
			if (regno != 31)
				frame->tf_r[regno + 1] = tmp;
			break;
		}
		break;
	}
}

/*
 * Return the largest precision of all precision inputs.
 *
 * This assumes none of the inputs is FTYPE_INT.
 */
u_int
fpu_precision(u_int ts1, u_int ts2, u_int td)
{
	return max(td, max(ts1, ts2));
}

/*
 * Emulate an FPU instruction.  On return, the trapframe registers
 * will be modified to reflect the settings the hardware would have left.
 */
int
fpu_emulate(struct trapframe *frame, u_int32_t insn)
{
	u_int rf, rd, rs1, rs2, t1, t2, td, tmax, opcode;
	u_int32_t old_fpsr, old_fpcr;
	int rc;

	fparg arg1, arg2, dest;

	/*
	 * Crack the instruction.
	 */
	rd = (insn >> 21) & 0x1f;
	rs1 = (insn >> 16) & 0x1f;
	rs2 = insn & 0x1f;
	rf = (insn >> 15) & 0x01;
	opcode = (insn >> 11) & 0x0f;
	t1 = (insn >> 9) & 0x03;
	t2 = (insn >> 7) & 0x03;
	td = (insn >> 5) & 0x03;

	/*
	 * Discard invalid opcodes, as well as instructions involving XRF,
	 * since we do not support them yet.
	 */
	if (rf != 0)
		return (SIGILL);

	switch (opcode) {
	case 0x00:	/* fmul */
	case 0x05:	/* fadd */
	case 0x06:	/* fsub */
	case 0x0e:	/* fdiv */
		if ((t1 != FTYPE_SNG && t1 != FTYPE_DBL) ||
		    (t2 != FTYPE_SNG && t2 != FTYPE_DBL) ||
		    (td != FTYPE_SNG && td != FTYPE_DBL))
			return (SIGILL);
		break;
	case 0x04:	/* flt */
		if (t1 != 0x00)	/* flt on XRF */
			return (SIGILL);
		if ((td != FTYPE_SNG && td != FTYPE_DBL) ||
		    t2 != 0x00 || rs1 != 0)
			return (SIGILL);
		break;
	case 0x07:	/* fcmp, fcmpu */
		if ((t1 != FTYPE_SNG && t1 != FTYPE_DBL) ||
		    (t2 != FTYPE_SNG && t2 != FTYPE_DBL))
			return (SIGILL);
		if (td != 0x00 /* fcmp */ && td != 0x01 /* fcmpu */)
			return (SIGILL);
		break;
	case 0x09:	/* int */
	case 0x0a:	/* nint */
	case 0x0b:	/* trnc */
		if ((t2 != FTYPE_SNG && t2 != FTYPE_DBL) ||
		    t1 != 0x00 || td != 0x00 || rs1 != 0)
			return (SIGILL);
		break;
	case 0x01:	/* fcvt */
		if (t2 == td)
			return (SIGILL);
		/* FALLTHROUGH */
	case 0x0f:	/* fsqrt */
		if ((t2 != FTYPE_SNG && t2 != FTYPE_DBL) ||
		    (td != FTYPE_SNG && td != FTYPE_DBL) ||
		    t1 != 0x00 || rs1 != 0)
			return (SIGILL);
		break;
	default:
	case 0x08:	/* mov */
		return (SIGILL);
	}

	/*
	 * Temporarily reset the status register, so that we can tell
	 * which exceptions are new after processing the opcode.
	 */
	old_fpsr = frame->tf_fpsr;
	frame->tf_fpsr = 0;

	/*
	 * Save fpcr as well, since we might need to change rounding mode
	 * temporarily.
	 */
	old_fpcr = frame->tf_fpcr;

	/*
	 * The logic for instruction emulation is:
	 *
	 * - the computation precision is the largest one of all the operands.
	 * - all source operands are converted to this precision if needed.
	 * - computation is performed.
	 * - the result is stored into the destination operand, converting it
	 *   to the destination precision if lower.
	 */

	switch (opcode) {
	case 0x00:	/* fmul */
		tmax = fpu_precision(t1, t2, td);
		fpu_fetch(frame, rs1, t1, tmax, &arg1);
		fpu_fetch(frame, rs2, t2, tmax, &arg2);
		switch (tmax) {
		case FTYPE_SNG:
			dest.sng = float32_mul(arg1.sng, arg2.sng);
			break;
		case FTYPE_DBL:
			dest.dbl = float64_mul(arg1.dbl, arg2.dbl);
			break;
		}
		fpu_store(frame, rd, tmax, td, &dest);
		break;

	case 0x01:	/* fcvt */
		tmax = fpu_precision(IGNORE_PRECISION, t2, td);
		fpu_fetch(frame, rs2, t2, tmax, &dest);
		fpu_store(frame, rd, tmax, td, &dest);
		break;

	case 0x04:	/* flt */
		fpu_fetch(frame, rs2, FTYPE_INT, td, &dest);
		fpu_store(frame, rd, td, td, &dest);
		break;

	case 0x05:	/* fadd */
		tmax = fpu_precision(t1, t2, td);
		fpu_fetch(frame, rs1, t1, tmax, &arg1);
		fpu_fetch(frame, rs2, t2, tmax, &arg2);
		switch (tmax) {
		case FTYPE_SNG:
			dest.sng = float32_add(arg1.sng, arg2.sng);
			break;
		case FTYPE_DBL:
			dest.dbl = float64_add(arg1.dbl, arg2.dbl);
			break;
		}
		fpu_store(frame, rd, tmax, td, &dest);
		break;

	case 0x06:	/* fsub */
		tmax = fpu_precision(t1, t2, td);
		fpu_fetch(frame, rs1, t1, tmax, &arg1);
		fpu_fetch(frame, rs2, t2, tmax, &arg2);
		switch (tmax) {
		case FTYPE_SNG:
			dest.sng = float32_sub(arg1.sng, arg2.sng);
			break;
		case FTYPE_DBL:
			dest.dbl = float64_sub(arg1.dbl, arg2.dbl);
			break;
		}
		fpu_store(frame, rd, tmax, td, &dest);
		break;

	case 0x07:	/* fcmp, fcmpu */
		tmax = fpu_precision(t1, t2, IGNORE_PRECISION);
		fpu_fetch(frame, rs1, t1, tmax, &arg1);
		fpu_fetch(frame, rs2, t2, tmax, &arg2);
		fpu_compare(frame, &arg1, &arg2, tmax, rd, td /* fcmpu */);
		break;

	case 0x09:	/* int */
do_int:
		fpu_fetch(frame, rs2, t2, t2, &dest);
		fpu_store(frame, rd, t2, FTYPE_INT, &dest);
		break;

	case 0x0a:	/* nint */
		/* round to nearest */
		frame->tf_fpcr = (old_fpcr & ~(FPCR_RD_MASK << FPCR_RD_SHIFT)) |
		    (FP_RN << FPCR_RD_SHIFT);
		goto do_int;

	case 0x0b:	/* trnc */
		/* round towards zero */
		frame->tf_fpcr = (old_fpcr & ~(FPCR_RD_MASK << FPCR_RD_SHIFT)) |
		    (FP_RZ << FPCR_RD_SHIFT);
		goto do_int;

	case 0x0e:	/* fdiv */
		tmax = fpu_precision(t1, t2, td);
		fpu_fetch(frame, rs1, t1, tmax, &arg1);
		fpu_fetch(frame, rs2, t2, tmax, &arg2);
		switch (tmax) {
		case FTYPE_SNG:
			dest.sng = float32_div(arg1.sng, arg2.sng);
			break;
		case FTYPE_DBL:
			dest.dbl = float64_div(arg1.dbl, arg2.dbl);
			break;
		}
		fpu_store(frame, rd, tmax, td, &dest);
		break;

	case 0x0f:	/* sqrt */
		tmax = fpu_precision(IGNORE_PRECISION, t2, td);
		fpu_fetch(frame, rs2, t2, tmax, &arg1);
		switch (tmax) {
		case FTYPE_SNG:
			dest.sng = float32_sqrt(arg1.sng);
			break;
		case FTYPE_DBL:
			dest.dbl = float64_sqrt(arg1.dbl);
			break;
		}
		fpu_store(frame, rd, tmax, td, &dest);
		break;
	}

	/*
	 * Mark new exceptions, if any, in the fpsr, and decide whether
	 * to send a signal or not.
	 */

	if (frame->tf_fpsr & old_fpcr)
		rc = SIGFPE;
	else
		rc = 0;
	frame->tf_fpsr |= old_fpsr;

	/*
	 * Restore fpcr as well.
	 */
	frame->tf_fpcr = old_fpcr;

	return (rc);
}

/*
 * Perform a compare instruction (fcmp, fcmpu).
 *
 * If either operand is NaN, the result is unordered.  This causes an
 * reserved operand exception (except for nonsignalling NaNs for fcmpu).
 */
void
fpu_compare(struct trapframe *frame, fparg *s1, fparg *s2, u_int width,
    u_int rd, u_int fcmpu)
{
	u_int32_t cc;
	int zero, s1positive, s2positive;

	/*
	 * Handle NaNs first, and raise invalid if fcmp or signaling NaN.
	 */
	switch (width) {
	case FTYPE_SNG:
		if (float32_is_nan(s1->sng)) {
			if (!fcmpu || float32_is_signaling_nan(s1->sng))
				float_set_invalid();
			cc = CC_UN;
			goto done;
		}
		if (float32_is_nan(s2->sng)) {
			if (!fcmpu || float32_is_signaling_nan(s2->sng))
				float_set_invalid();
			cc = CC_UN;
			goto done;
		}
		break;
	case FTYPE_DBL:
		if (float64_is_nan(s1->dbl)) {
			if (!fcmpu || float64_is_signaling_nan(s1->dbl))
				float_set_invalid();
			cc = CC_UN;
			goto done;
		}
		if (float64_is_nan(s2->dbl)) {
			if (!fcmpu || float64_is_signaling_nan(s2->dbl))
				float_set_invalid();
			cc = CC_UN;
			goto done;
		}
		break;
	}

	/*
	 * Now order the two numbers.
	 */
	switch (width) {
	case FTYPE_SNG:
		if (float32_eq(s1->sng, s2->sng))
			cc = CC_EQ;
		else if (float32_lt(s1->sng, s2->sng))
			cc = CC_LT;
		else
			cc = CC_GT;
		break;
	case FTYPE_DBL:
		if (float64_eq(s1->dbl, s2->dbl))
			cc = CC_EQ;
		else if (float64_lt(s1->dbl, s2->dbl))
			cc = CC_LT;
		else
			cc = CC_GT;
		break;
	}

done:

	/*
	 * Complete condition code mask.
	 */

	if (cc & CC_UN)
		cc |= CC_UE | CC_UG | CC_ULE | CC_UL | CC_UGE;
	if (cc & CC_EQ)
		cc |= CC_LE | CC_GE | CC_UE;
	if (cc & CC_GT)
		cc |= CC_GE;
	if (cc & CC_LT)
		cc |= CC_LE;
	if (cc & (CC_LT | CC_GT))
		cc |= CC_LG;
	if (cc & (CC_LT | CC_GT | CC_EQ))
		cc |= CC_LEG;
	if (cc & CC_GT)
		cc |= CC_UG;
	if (cc & CC_LE)
		cc |= CC_ULE;
	if (cc & CC_LT)
		cc |= CC_UL;
	if (cc & CC_GE)
		cc |= CC_UGE;

	/*
	 * Fill the interval bits.
	 * s1 is compared to the interval [0, s2].
	 */
	if (!(cc & CC_UN)) {
		if (cc & CC_EQ) {
			/* if s1 and s2 are equal, s1 is on boundary */
			cc |= CC_IB | CC_OB;
			goto completed;
		}

		/* s1 and s2 are either Zero, numbers or Inf */
		switch (width) {
		case FTYPE_SNG:
			zero = float32_eq(s1->sng, 0);
			break;
		case FTYPE_DBL:
			zero = float64_eq(s1->dbl, 0LL);
			break;
		}
		if (zero) {
			/* if s1 is zero, it is on boundary */
			cc |= CC_IB | CC_OB;
			goto completed;
		}

		switch (width) {
		case FTYPE_SNG:
			s1positive = s1->sng >> 31 == 0;
			s2positive = s2->sng >> 31 == 0;
			break;
		case FTYPE_DBL:
			s1positive = s1->dbl >> 63 == 0;
			s2positive = s2->dbl >> 63 == 0;
			break;
		}
		if (s2positive) {
			/* s2 is positive, the interval is [0, s2] */
			if (cc & CC_GT) {
				/* 0 <= s2 < s1 -> out of interval */
				cc |= CC_OU | CC_OB;
			} else if (s1positive) {
				/* 0 < s1 < s2 -> in interval */
				cc |= CC_IB | CC_IN;
			} else {
				/* s1 < 0 <= s2 */
				cc |= CC_OU | CC_OB;
			}
		} else {
			/* s2 is negative, the interval is [s2, 0] */
			if (cc & CC_LT) {
				/* s1 < s2 <= 0 */
				cc |= CC_OU | CC_OB;
			} else if (!s1positive) {
				/* s2 < s1 < 0 */
				cc |= CC_IB | CC_IN;
			} else {
				/* s2 < 0 < s1 */
				cc |= CC_OU | CC_OB;
			}
		}
	}

completed:
	if (rd != 0)
		frame->tf_r[rd] = cc;
}
