/*	$OpenBSD: m88110_fp.S,v 1.4 2007/12/08 18:39:50 miod Exp $	*/

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

#include "assym.h"

#include <machine/asm.h>
#include <machine/trap.h>

/* FPECR bits */
#define	FIOV_BIT	7
#define	FUNIMP_BIT	6
#define	FPRV_BIT	5
#define	FROP_BIT	4
#define	FDVZ_BIT	3
#define	FUNF_BIT	2
#define	FOVF_BIT	1
#define	FINX_BIT	0

/* FPSR and FPCR exception bits */
#define	EFINV_BIT	4
#define	EFDVZ_BIT	3
#define	EFUNF_BIT	2
#define	EFOVF_BIT	1
#define	EFINX_BIT	0

ASENTRY(m88110_fpu_exception)
	/*
	 * On entry, r30 point to the exception frame.
	 * Save our guts so that we can call other functions.
	 */
	subu	r31, r31, 16
	st	r1,  r31, 0
	st	r30, r31, 4

	/*
	 * We never saved the floating point exception and control
	 * registers. Do it now, and then decide what kind of exception
	 * we've got.
	 * Note that we can access the control registers even if the
	 * FPU is disabled.
	 */
	fldcr	r2,  FPECR
	fldcr	r3,  FPSR
	fldcr	r4,  FPCR
	st	r2,  r30, EF_FPECR * 4
	st	r3,  r30, EF_FPSR * 4
	st	r4,  r30, EF_FPCR * 4

	/*
	 * Check for floating point unimplemented bit first, as other
	 * bits are undefined if this bit is set.
	 */
	bb1	FUNIMP_BIT, r2, _ASM_LABEL(m88110_funimp)

	/*
	 * Check for the other exceptions.
	 *
	 * FOVF and FUNF need to be checked before FINX.
	 */
	bb1	FIOV_BIT, r2, _ASM_LABEL(m88110_fiov)
	bb1	FPRV_BIT, r2, _ASM_LABEL(m88110_fprv)
	bb1	FROP_BIT, r2, _ASM_LABEL(m88110_frop)
	bb1	FDVZ_BIT, r2, _ASM_LABEL(m88110_fdvz)
	bb1	FUNF_BIT, r2, _ASM_LABEL(m88110_funf)
	bb1	FOVF_BIT, r2, _ASM_LABEL(m88110_fovf)
	bb1	FINX_BIT, r2, _ASM_LABEL(m88110_finx)

	/*
	 * If control goes here, we got a floating point exception,
	 * but no cause bit is set. This shouldn't happen; if it does,
	 * just fall through the unimplemented instruction code.
	 */

ASLOCAL(m88110_funimp)
	/*
	 * Check if the fpu was enabled.
	 */
	ld	r5, r30, EF_EPSR * 4
	/* FPE_FLTINV */
	bb1	PSR_FPU_DISABLE_BIT, r5, _ASM_LABEL(m88110_fpeflt)

	/*
	 * If this is a kernel fault, we were probably trying to reissue
	 * a previously-faulting instruction.
	 * Just return without altering the fpecr, the ``caller'' will
	 * check it afterwards.
	 */
	bb1	PSR_SUPERVISOR_MODE_BIT, r5, _ASM_LABEL(m88110_fp_return_noreset)

	/*
	 * We should check the faulting instruction here.
	 *
	 * According to the documentation, this can be:
	 * - fsqrt (unimplemented)
	 * - any valid fp instruction operating on an odd register pair
	 * - any bogus fp instruction.
	 *
	 * However, real life shows that the 88110 will conveniently only
	 * flag the ``unimplemented instruction'' exception bit, regardless
	 * of the real exception cause. In this case, the fpsr register is
	 * correctly populated. But since bits in it are sticky... we can
	 * not trust its value )-:
	 *
	 * Because of this, we need to sort out real unimplemented opcodes
	 * from incorrectly reported exceptions.
	 * We will only handle odd register pairs for instructions issued
	 * in user mode (since the kernel will not issue any fp instructions
	 * except in this file).
	 */

#ifdef notyet
	/* Fetch the offending instruction */
	ld	r6,  r30, EF_EXIP * 4
	ld.usr	r2,  r6,  r0

	/*
	 * Check the instruction format. All triadic floating point
	 * instruction are built this way:
	 *
	 *	100001 | D | S1 | X | opcode | T1 | T2 | TD | S2
	 *
	 * with:
	 * D (bits 25-21) = destination register
	 * S1 (bits 20-16) = source 1 register
	 * X (bit 15) = extended register file
	 * T1 (bits 10-9), T2 (bits 8-7), TD (bits 6-5) = size of the
	 *    respective registers (00 = single, 01 = double, 10 = X)
	 * S1 (bits 4-0) = source 2 register
	 *
	 * flt is slightly different, in that the X bit is bit 9 (in T1).
	 *
	 * Note that we currently do not support the extended register
	 * file, so instructions referring to the X registers will not
	 * be processed. This includes all forms of the mov instruction.
	 */

	extu	r3,  r2,  6<26>
	cmp	r4,  r3,  0x21	/* 10 0001 */
	bcnd	ne0, r4,  _ASM_LABEL(m88110_bad_opcode)

	/*
	 * Extract the 4-bit opcode and check for the XRF bit.
	 */
	extu	r5,  r2,  4<11>	/* opcode */
	cmp	r4,  r5,  0x04	/* flt */
	bcnd	eq0, r4,  _ASM_LABEL(m88110_check_flt)

	bb1	15,  r2,  _ASM_LABEL(m88110_bad_opcode)

	/* For fcmp, we do not want to check TD. */
	cmp	r4,  r5,  0x07	/* fcmp/fcmpu */
	bcnd	eq0, r4,  _ASM_LABEL(m88110_check_t1)

	/*
	 * Check TD and RD. If TD is 01 and RD is not even,
	 * this is indeed an odd-register pair exception.
	 */
	extu	r6,  r2,  2<5>
	cmp	r7,  r6,  0x01
	bcnd	ne0, r7,  _ASM_LABEL(m88110_check_t1)

	bb1	21,  r2,  _ASM_LABEL(m88110_odd_reg)

ASLOCAL(m88110_check_t1)
	/*
	 * Check T1 and R1. If T1 is 01 and R1 is not even,
	 * this is indeed an odd-register pair exception.
	 */
	extu	r6,  r2,  2<9>
	cmp	r7,  r6,  0x01
	bcnd	ne0, r7,  _ASM_LABEL(m88110_check_t2)

	bb1	16,  r2,  _ASM_LABEL(m88110_odd_reg)

ASLOCAL(m88110_check_t2)
	/*
	 * Check T2 and R2. If T2 is 01 and R2 is not even,
	 * this is indeed an odd-register pair exception.
	 */
	extu	r6,  r2,  2<7>
	cmp	r7,  r6,  0x01
	bcnd	ne0, r7,  _ASM_LABEL(m88110_bad_opcode)

	bb1	 0,  r2,  _ASM_LABEL(m88110_odd_reg)

	br	_ASM_LABEL(m88110_bad_opcode)

ASLOCAL(m88110_check_flt)
	/*
	 * For flt, we only want to check TD. But we also want
	 * to bail out if operating on XRF.
	 */
	bb1	9,   r2,  _ASM_LABEL(m88110_bad_opcode)

	/*
	 * Check TD and RD. If TD is 01 and RD is not even,
	 * this is indeed an odd-register pair exception.
	 */
	extu	r6,  r2,  2<5>
	cmp	r7,  r6,  0x01
	bcnd	ne0, r7,  _ASM_LABEL(m88110_bad_opcode)

	bb1	21,  r2,  _ASM_LABEL(m88110_odd_reg)

ASLOCAL(m88110_bad_opcode)
	/*
	 * This is not an odd-register pair exception.
	 *
	 * We should check for an fsqrt instruction and emulate it.
	 * However since the compiler will not produce it, we can
	 * skip emulating it for now...
	 */
#endif
	br	_ASM_LABEL(m88110_fpeflt)

/*
 * Floating-point integer overflow.
 *
 * Register it in the status register, and if this exception is allowed
 * in the control register, send a trap.
 */
ASLOCAL(m88110_fiov)
	set	r3,  r3,  1<EFINV_BIT>
	fstcr	r3,  FPSR

	/* FPE_FLTSUB */
	bb1	EFINV_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	jmp.n	r1
	 addu	r31, r31, 16

/*
 * Floating-point privilege violation.
 *
 * This exception is caused by fldcr, fstcr or fxcr with either a
 * non-existing register (fcr1-fcr61), or fcr0 from userland.
 */ 
ASLOCAL(m88110_fprv)
	/* ILL_PRVREG */
	or	r2, r0, T_PRIVINFLT
	bsr.n	_C_LABEL(m88110_trap)
	 or	r3, r0, r30

	ld	r1,  r31, 0
	jmp.n	r1
	 addu	r31, r31, 16

/*
 * Floating-point reserved operand.
 */
ASLOCAL(m88110_frop)
	set	r3,  r3,  1<EFINV_BIT>
	fstcr	r3,  FPSR

	/* XXX TBD lots of analysis to do here... */

	/* FPE_FLTINV */
	bb1	EFINV_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	/* XXX TBD ... and then decide on a value... */

	jmp.n	r1
	 addu	r31, r31, 16

/*
 * Floating-point divide by zero.
 *
 * Register it in the status register, and if this exception is allowed
 * in the control register, send a trap.
 */
ASLOCAL(m88110_fdvz)
	set	r3,  r3,  1<EFDVZ_BIT>
	fstcr	r3,  FPSR

	/* FPE_INTDIV */
	bb1	EFDVZ_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	jmp.n	r1
	 addu	r31, r31, 16

/*
 * Floating-point underflow.
 */
ASLOCAL(m88110_funf)
	set	r3,  r3,  1<EFUNF_BIT>
	set	r3,  r3,  1<EFINX_BIT>
	fstcr	r3,  FPSR

	/* FPE_FLTUND */
	bb1	EFUNF_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	/* FPE_FLTRES */
	bb1	EFINX_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	/* XXX TBD check rounding mode, check destination register, write
	   default value to it/them. */

	jmp.n	r1
	 addu	r31, r31, 16


/*
 * Floating-point overflow.
 */
ASLOCAL(m88110_fovf)
	set	r3,  r3,  1<EFOVF_BIT>
	set	r3,  r3,  1<EFINX_BIT>
	fstcr	r3,  FPSR

	/* FPE_FLTOVF */
	bb1	EFOVF_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	/* FPE_FLTRES */
	bb1	EFINX_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	/* XXX TBD check rounding mode, check destination register, write
	   default value to it/them. */

	jmp.n	r1
	 addu	r31, r31, 16

/*
 * Floating-point inexact.
 *
 * Register it in the status register, and if this exception is allowed
 * in the control register, send a trap.
 */
ASLOCAL(m88110_finx)
	set	r3,  r3,  1<EFINX_BIT>
	fstcr	r3,  FPSR

	/* FPE_FLTRES */
	bb1	EFINX_BIT, r4, _ASM_LABEL(m88110_fpeflt)

	jmp.n	r1
	 addu	r31, r31, 16

ASLOCAL(m88110_fpeflt)
	/*
	 * Do not call trap() if the exception comes from kernel mode.
	 */
	ld	r5, r30, EF_EPSR * 4
	bb1	PSR_SUPERVISOR_MODE_BIT, r5, _ASM_LABEL(m88110_fp_return)

	or	r2, r0, T_FPEPFLT
	bsr.n	_C_LABEL(m88110_trap)
	 or	r3, r0, r30

ASLOCAL(m88110_fp_return)
	/*
	 * Reset the exception cause register
	 */
	fstcr	r0,  FPECR

ASLOCAL(m88110_fp_return_noreset)
	ld	r1,  r31, 0
	jmp.n	r1
	 addu	r31, r31, 16

#ifdef notyet
/*
 * Odd-numbered register pair emulation.
 *
 * On entry:
 *	r2 = faulting instruction
 *	r5 = instruction sub opcode (bits 14-11)
 *	r30 = exception frame
 *
 * We'll issue a similar instruction using only even-numbered register pairs,
 * update the exception frame registers with the result, and skip the
 * emulated instruction.
 */
ASLOCAL(m88110_odd_reg)
	or.u	r10, r0,  hi16(_ASM_LABEL(m88110_odd_table))
	or	r10, r10, lo16(_ASM_LABEL(m88110_odd_table))
	ld	r9,  r10 [r5]
	jmp	r9

ASLOCAL(m88110_odd_table)
	.word	_ASM_LABEL(m88110_odd_fmul)
	.word	_ASM_LABEL(m88110_odd_fcvt)
	.word	_ASM_LABEL(m88110_bad_opcode)
	.word	_ASM_LABEL(m88110_bad_opcode)

	.word	_ASM_LABEL(m88110_odd_flt)
	.word	_ASM_LABEL(m88110_odd_fadd)
	.word	_ASM_LABEL(m88110_odd_fsub)
	.word	_ASM_LABEL(m88110_odd_fcmp)

	.word	_ASM_LABEL(m88110_bad_opcode)
	.word	_ASM_LABEL(m88110_odd_int)
	.word	_ASM_LABEL(m88110_odd_nint)
	.word	_ASM_LABEL(m88110_odd_trnc)

	.word	_ASM_LABEL(m88110_bad_opcode)
	.word	_ASM_LABEL(m88110_bad_opcode)
	.word	_ASM_LABEL(m88110_odd_fdiv)
	.word	_ASM_LABEL(m88110_bad_opcode)

ASLOCAL(m88110_odd_fmul)
ASLOCAL(m88110_odd_fcvt)
ASLOCAL(m88110_odd_flt)
ASLOCAL(m88110_odd_fadd)
ASLOCAL(m88110_odd_fsub)
ASLOCAL(m88110_odd_fcmp)
ASLOCAL(m88110_odd_int)
ASLOCAL(m88110_odd_nint)
ASLOCAL(m88110_odd_trnc)
ASLOCAL(m88110_odd_fdiv)
	/* XXX TBD */
	br	_ASM_LABEL(m88110_fp_return)
#endif
