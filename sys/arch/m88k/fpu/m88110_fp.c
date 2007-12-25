/*	$OpenBSD: m88110_fp.c,v 1.1 2007/12/25 00:29:49 miod Exp $	*/

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
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)fpu.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/systm.h>

#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/ieeefp.h>
#include <machine/trap.h>
#include <machine/m88110.h>

#include <m88k/fpu/fpu_emu.h>

int	fpu_emulate(struct trapframe *, u_int32_t);

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
 * - a genuinely unimplement feature: fsqrt.
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
		 * ``real'' FPU instruction. We'll try to emulate it.
		 */
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

	if (frame->tf_epsr & PSR_SFD1) {	/* don't bother */
		sig = SIGFPE;
		fault_type = FPE_FLTINV;
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
		KERNEL_PROC_LOCK(p);
		trapsignal(p, sig, 0, fault_type, sv);
		KERNEL_PROC_UNLOCK(p);
	}
}

/*
 * Emulate an FPU instruction.  On return, the trapframe registers
 * will be modified to reflect the settings the hardware would have left.
 */
int
fpu_emulate(struct trapframe *frame, u_int32_t insn)
{
	struct fpemu fe;
	u_int rf, rd, rs1, rs2, t1, t2, td, opcode;
	u_int32_t old_fpsr, old_fpcr;
	u_int32_t scratch;
	int rc;

	struct fpn *fp;
#ifdef notyet
	u_int space[4];
#else
	u_int space[2];
#endif

	fe.fe_fpstate = frame;

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
		if ((t1 != FTYPE_SNG || t1 != FTYPE_DBL) ||
		    (t2 != FTYPE_SNG || t2 != FTYPE_DBL) ||
		    (td != FTYPE_SNG || td != FTYPE_DBL))
		break;
	case 0x04:	/* flt */
		if (t1 != 0x00)	/* flt on XRF */
			return (SIGILL);
		if (td != FTYPE_SNG || td != FTYPE_DBL ||
		    t2 != 0x00 || rs1 != 0)
			return (SIGILL);
		break;
	case 0x07:	/* fcmp, fcmpu */
		if ((t1 != FTYPE_SNG || t1 != FTYPE_DBL) ||
		    (t2 != FTYPE_SNG || t2 != FTYPE_DBL))
			return (SIGILL);
		if (td != 0x00 /* fcmp */ && td != 0x01 /* fcmpu */)
			return (SIGILL);
		break;
	case 0x09:	/* int */
	case 0x0a:	/* nint */
	case 0x0b:	/* trnc */
		if (t2 != FTYPE_SNG || t2 != FTYPE_DBL ||
		    t1 != 0x00 || td != 0x00 || rs1 != 0)
			return (SIGILL);
		break;
	case 0x01:	/* fcvt */
	case 0x0f:	/* fsqrt */
		if ((t2 != FTYPE_SNG || t2 != FTYPE_DBL) ||
		    (td != FTYPE_SNG || td != FTYPE_DBL) ||
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

	switch (opcode) {
	case 0x00:	/* fmul */
		fpu_explode(&fe, &fe.fe_f1, t1, rs1);
		fpu_explode(&fe, &fe.fe_f2, t2, rs2);
		fp = fpu_mul(&fe);
		break;

	case 0x01:	/* fcvt */
		fpu_explode(&fe, &fe.fe_f1, t2, rs2);
		fp = &fe.fe_f1;
		break;

	case 0x04:	/* flt */
		fpu_explode(&fe, &fe.fe_f1, FTYPE_INT, rs2);
		fp = &fe.fe_f1;
		break;

	case 0x05:	/* fadd */
		fpu_explode(&fe, &fe.fe_f1, t1, rs1);
		fpu_explode(&fe, &fe.fe_f2, t2, rs2);
		fp = fpu_add(&fe);
		break;

	case 0x06:	/* fsub */
		fpu_explode(&fe, &fe.fe_f1, t1, rs1);
		fpu_explode(&fe, &fe.fe_f2, t2, rs2);
		fp = fpu_sub(&fe);
		break;

	case 0x07:	/* fcmp, fcmpu */
		fpu_explode(&fe, &fe.fe_f1, t1, rs1);
		fpu_explode(&fe, &fe.fe_f2, t2, rs2);
		scratch = fpu_compare(&fe, td);
		if (rd != 0)
			frame->tf_r[rd] = scratch;
		break;

	case 0x09:	/* int */
do_int:
		fpu_explode(&fe, &fe.fe_f1, t2, rs2);
		td = FTYPE_INT;
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
		fpu_explode(&fe, &fe.fe_f1, t1, rs1);
		fpu_explode(&fe, &fe.fe_f2, t2, rs2);
		fp = fpu_div(&fe);
		break;

	case 0x0f:	/* sqrt */
		fpu_explode(&fe, &fe.fe_f1, t2, rs2);
		fp = fpu_sqrt(&fe);
		break;
	}

	/*
	 * Emulated operation is complete.  Collapse the result into the
	 * destination register(s).
	 */
	if (opcode != 0x07) {
		fpu_implode(&fe, fp, td, space);

		switch (td) {
#ifdef notyet
		case FTYPE_EXT:
			/* ... */
#endif
		case FTYPE_DBL:
			if (rd != 31)
				frame->tf_r[rd + 1] = space[1];
			/* FALLTHROUGH */
		case FTYPE_SNG:
		case FTYPE_INT:
			if (rd != 0)
				frame->tf_r[rd] = space[0];
			break;
		}
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
