/*	$OpenBSD: fp.S,v 1.9 2010/02/08 19:26:46 miod Exp $	*/
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)fp.s	8.1 (Berkeley) 6/10/93
 *      $Id: fp.S,v 1.9 2010/02/08 19:26:46 miod Exp $
 */

/*
 * Standard header stuff.
 */

#include <machine/regdef.h>
#include <machine/asm.h>
#include <machine/regnum.h>
#include <machine/cpu.h>

#include "assym.h"

#define SEXP_INF	0xff
#define DEXP_INF	0x7ff
#define SEXP_BIAS	127
#define DEXP_BIAS	1023
#define SEXP_MIN	-126
#define DEXP_MIN	-1022
#define SEXP_MAX	127
#define DEXP_MAX	1023
#define WEXP_MAX	30		/* maximum unbiased exponent for int */
#define WEXP_MIN	-1		/* minimum unbiased exponent for int */
#define LEXP_MAX	62		/* maximum unbiased exponent for long */
#define LEXP_MIN	-1		/* minimum unbiased exponent for long */
#define SFRAC_BITS	23
#define DFRAC_BITS	52
#define SIMPL_ONE	0x00800000
#define DIMPL_ONE	0x0010000000000000
#define SLEAD_ZEROS	63 - 55
#define DLEAD_ZEROS	63 - 52
#define STICKYBIT	1
#define GUARDBIT	0x0000000080000000
#define DGUARDBIT	0x8000000000000000

#define SSIGNAL_NAN	0x00400000
#define DSIGNAL_NAN	0x00080000
#define SQUIET_NAN	0x003fffff
#define DQUIET_NAN	0x0007ffffffffffff
#define INT_MIN		0x80000000
#define INT_MAX		0x7fffffff
#define LONG_MIN	0x8000000000000000
#define LONG_MAX	0x7fffffffffffffff

#define COND_UNORDERED	0x1
#define COND_EQUAL	0x2
#define COND_LESS	0x4
#define COND_SIGNAL	0x8

/*----------------------------------------------------------------------------
 *
 * MipsEmulateFP --
 *
 *	Emulate unimplemented floating point operations.
 *	This routine should only be called by MipsFPInterrupt()
 *	and only if this is a COP1 instruction.
 *
 *	MipsEmulateFP(instr)
 *		unsigned instr;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Floating point registers are modified according to instruction.
 *
 *----------------------------------------------------------------------------
 */
NON_LEAF(MipsEmulateFP, FRAMESZ(CF_SZ), ra)
	PTR_SUB	sp, sp, FRAMESZ(CF_SZ)
	PTR_S	ra, CF_RA_OFFS(sp)

	srl	v0, a0, 21			# get FMT field
	and	v0, v0, 0x1f			# mask FMT field
	dla	a3, func_s
	beq	v0, 0x10, 1f
	dla	a3, func_d
	beq	v0, 0x11, 1f
	dla	a3, func_w
	beq	v0, 0x14, 1f
	dla	a3, func_l
	beq	v0, 0x15, 1f
	b	ill				# illegal format

1:
	and	v1, a0, 0x3f			# mask FUNC field
	sll	v1, v1, 3			# align for table lookup
	daddu	v1, a3
	cfc1	a1, FPC_CSR			# get exception register
	ld	a3, (v1)			# switch on FUNC & FMT
	and	a1, a1, ~FPC_EXCEPTION_UNIMPL	# clear exception
	ctc1	a1, FPC_CSR
	j	a3

	.rdata
func_s:
	.dword	add_s		# 0
	.dword	sub_s		# 1
	.dword	mul_s		# 2
	.dword	div_s		# 3
	.dword	ill		# 4 (sqrt)
	.dword	abs_s		# 5
	.dword	mov_s		# 6
	.dword	neg_s		# 7
	.dword	round_l_s	# 8
	.dword	trunc_l_s	# 9
	.dword	ceil_l_s	# 10
	.dword	floor_l_s	# 11
	.dword	round_w_s	# 12
	.dword	trunc_w_s	# 13
	.dword	ceil_w_s	# 14
	.dword	floor_w_s	# 15
	.dword	ill		# 16
	.dword	ill		# 17
	.dword	ill		# 18
	.dword	ill		# 19
	.dword	ill		# 20
	.dword	ill		# 21
	.dword	ill		# 22
	.dword	ill		# 23
	.dword	ill		# 24
	.dword	ill		# 25
	.dword	ill		# 26
	.dword	ill		# 27
	.dword	ill		# 28
	.dword	ill		# 29
	.dword	ill		# 30
	.dword	ill		# 31
	.dword	ill		# 32
	.dword	cvt_d_s		# 33
	.dword	ill		# 34
	.dword	ill		# 35
	.dword	cvt_w_s		# 36
	.dword	cvt_l_s		# 37
	.dword	ill		# 38
	.dword	ill		# 39
	.dword	ill		# 40
	.dword	ill		# 41
	.dword	ill		# 42
	.dword	ill		# 43
	.dword	ill		# 44
	.dword	ill		# 45
	.dword	ill		# 46
	.dword	ill		# 47
	.dword	cmp_s		# 48
	.dword	cmp_s		# 49
	.dword	cmp_s		# 50
	.dword	cmp_s		# 51
	.dword	cmp_s		# 52
	.dword	cmp_s		# 53
	.dword	cmp_s		# 54
	.dword	cmp_s		# 55
	.dword	cmp_s		# 56
	.dword	cmp_s		# 57
	.dword	cmp_s		# 58
	.dword	cmp_s		# 59
	.dword	cmp_s		# 60
	.dword	cmp_s		# 61
	.dword	cmp_s		# 62
	.dword	cmp_s		# 63

func_d:
	.dword	add_d		# 0
	.dword	sub_d		# 1
	.dword	mul_d		# 2
	.dword	div_d		# 3
	.dword	ill		# 4 (sqrt)
	.dword	abs_d		# 5
	.dword	mov_d		# 6
	.dword	neg_d		# 7
	.dword	round_l_d	# 8
	.dword	trunc_l_d	# 9
	.dword	ceil_l_d	# 10
	.dword	floor_l_d	# 11
	.dword	round_w_d	# 12
	.dword	trunc_w_d	# 13
	.dword	ceil_w_d	# 14
	.dword	floor_w_d	# 15
	.dword	ill		# 16
	.dword	ill		# 17
	.dword	ill		# 18
	.dword	ill		# 19
	.dword	ill		# 20
	.dword	ill		# 21
	.dword	ill		# 22
	.dword	ill		# 23
	.dword	ill		# 24
	.dword	ill		# 25
	.dword	ill		# 26
	.dword	ill		# 27
	.dword	ill		# 28
	.dword	ill		# 29
	.dword	ill		# 30
	.dword	ill		# 31
	.dword	cvt_s_d		# 32
	.dword	ill		# 33
	.dword	ill		# 34
	.dword	ill		# 35
	.dword	cvt_w_d		# 36
	.dword	cvt_l_d		# 37
	.dword	ill		# 38
	.dword	ill		# 39
	.dword	ill		# 40
	.dword	ill		# 41
	.dword	ill		# 42
	.dword	ill		# 43
	.dword	ill		# 44
	.dword	ill		# 45
	.dword	ill		# 46
	.dword	ill		# 47
	.dword	cmp_d		# 48
	.dword	cmp_d		# 49
	.dword	cmp_d		# 50
	.dword	cmp_d		# 51
	.dword	cmp_d		# 52
	.dword	cmp_d		# 53
	.dword	cmp_d		# 54
	.dword	cmp_d		# 55
	.dword	cmp_d		# 56
	.dword	cmp_d		# 57
	.dword	cmp_d		# 58
	.dword	cmp_d		# 59
	.dword	cmp_d		# 60
	.dword	cmp_d		# 61
	.dword	cmp_d		# 62
	.dword	cmp_d		# 63

func_w:
	.dword	ill		# 0
	.dword	ill		# 1
	.dword	ill		# 2
	.dword	ill		# 3
	.dword	ill		# 4
	.dword	ill		# 5
	.dword	ill		# 6
	.dword	ill		# 7
	.dword	ill		# 8
	.dword	ill		# 9
	.dword	ill		# 10
	.dword	ill		# 11
	.dword	ill		# 12
	.dword	ill		# 13
	.dword	ill		# 14
	.dword	ill		# 15
	.dword	ill		# 16
	.dword	ill		# 17
	.dword	ill		# 18
	.dword	ill		# 19
	.dword	ill		# 20
	.dword	ill		# 21
	.dword	ill		# 22
	.dword	ill		# 23
	.dword	ill		# 24
	.dword	ill		# 25
	.dword	ill		# 26
	.dword	ill		# 27
	.dword	ill		# 28
	.dword	ill		# 29
	.dword	ill		# 30
	.dword	ill		# 31
	.dword	cvt_s_w		# 32
	.dword	cvt_d_w		# 33
	.dword	ill		# 34
	.dword	ill		# 35
	.dword	ill		# 36
	.dword	ill		# 37
	.dword	ill		# 38
	.dword	ill		# 39
	.dword	ill		# 40
	.dword	ill		# 41
	.dword	ill		# 42
	.dword	ill		# 43
	.dword	ill		# 44
	.dword	ill		# 45
	.dword	ill		# 46
	.dword	ill		# 47
	.dword	ill		# 48
	.dword	ill		# 49
	.dword	ill		# 50
	.dword	ill		# 51
	.dword	ill		# 52
	.dword	ill		# 53
	.dword	ill		# 54
	.dword	ill		# 55
	.dword	ill		# 56
	.dword	ill		# 57
	.dword	ill		# 58
	.dword	ill		# 59
	.dword	ill		# 60
	.dword	ill		# 61
	.dword	ill		# 62
	.dword	ill		# 63

func_l:
	.dword	ill		# 0
	.dword	ill		# 1
	.dword	ill		# 2
	.dword	ill		# 3
	.dword	ill		# 4
	.dword	ill		# 5
	.dword	ill		# 6
	.dword	ill		# 7
	.dword	ill		# 8
	.dword	ill		# 9
	.dword	ill		# 10
	.dword	ill		# 11
	.dword	ill		# 12
	.dword	ill		# 13
	.dword	ill		# 14
	.dword	ill		# 15
	.dword	ill		# 16
	.dword	ill		# 17
	.dword	ill		# 18
	.dword	ill		# 19
	.dword	ill		# 20
	.dword	ill		# 21
	.dword	ill		# 22
	.dword	ill		# 23
	.dword	ill		# 24
	.dword	ill		# 25
	.dword	ill		# 26
	.dword	ill		# 27
	.dword	ill		# 28
	.dword	ill		# 29
	.dword	ill		# 30
	.dword	ill		# 31
	.dword	cvt_s_l		# 32
	.dword	cvt_d_l		# 33
	.dword	ill		# 34
	.dword	ill		# 35
	.dword	ill		# 36
	.dword	ill		# 37
	.dword	ill		# 38
	.dword	ill		# 39
	.dword	ill		# 40
	.dword	ill		# 41
	.dword	ill		# 42
	.dword	ill		# 43
	.dword	ill		# 44
	.dword	ill		# 45
	.dword	ill		# 46
	.dword	ill		# 47
	.dword	ill		# 48
	.dword	ill		# 49
	.dword	ill		# 50
	.dword	ill		# 51
	.dword	ill		# 52
	.dword	ill		# 53
	.dword	ill		# 54
	.dword	ill		# 55
	.dword	ill		# 56
	.dword	ill		# 57
	.dword	ill		# 58
	.dword	ill		# 59
	.dword	ill		# 60
	.dword	ill		# 61
	.dword	ill		# 62
	.dword	ill		# 63

	.text

/*
 * Single precision subtract.
 */
sub_s:
	jal	get_ft_fs_s
	xor	ta0, 1				# negate FT sign bit
	b	add_sub_s
/*
 * Single precision add.
 */
add_s:
	jal	get_ft_fs_s
add_sub_s:
	bne	t1, SEXP_INF, 1f		# is FS an infinity?
	bne	ta1, SEXP_INF, result_fs_s	# if FT is not inf, result=FS
	bne	t2, zero, result_fs_s		# if FS is NAN, result is FS
	bne	ta2, zero, result_ft_s		# if FT is NAN, result is FT
	bne	t0, ta0, invalid_s		# both infinities same sign?
	b	result_fs_s			# result is in FS
1:
	beq	ta1, SEXP_INF, result_ft_s	# if FT is inf, result=FT
	bne	t1, zero, 4f			# is FS a denormalized num?
	beq	t2, zero, 3f			# is FS zero?
	bne	ta1, zero, 2f			# is FT a denormalized num?
	beq	ta2, zero, result_fs_s		# FT is zero, result=FS
	jal	renorm_fs_s
	jal	renorm_ft_s
	b	5f
2:
	jal	renorm_fs_s
	subu	ta1, ta1, SEXP_BIAS		# unbias FT exponent
	or	ta2, ta2, SIMPL_ONE		# set implied one bit
	b	5f
3:
	bne	ta1, zero, result_ft_s		# if FT != 0, result=FT
	bne	ta2, zero, result_ft_s
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	bne	v0, FPC_ROUND_RM, 1f	# round to -infinity?
	or	t0, t0, ta0			# compute result sign
	b	result_fs_s
1:
	and	t0, ta0				# compute result sign
	b	result_fs_s
4:
	bne	ta1, zero, 2f			# is FT a denormalized num?
	beq	ta2, zero, result_fs_s		# FT is zero, result=FS
	subu	t1, SEXP_BIAS			# unbias FS exponent
	or	t2, SIMPL_ONE			# set implied one bit
	jal	renorm_ft_s
	b	5f
2:
	subu	t1, SEXP_BIAS			# unbias FS exponent
	or	t2, SIMPL_ONE			# set implied one bit
	subu	ta1, SEXP_BIAS			# unbias FT exponent
	or	ta2, SIMPL_ONE			# set implied one bit
/*
 * Perform the addition.
 */
5:
	move	t8, zero			# no shifted bits (sticky reg)
	beq	t1, ta1, 4f			# exp equal, no shift needed
	subu	v0, t1, ta1			# v0 = difference of exponents
	move	v1, v0				# v1 = abs(difference)
	bge	v0, zero, 1f
	negu	v1
1:
	ble	v1, SFRAC_BITS+2, 2f		# is difference too great?
	li	t8, STICKYBIT			# set the sticky bit
	bge	v0, zero, 1f			# check which exp is larger
	move	t1, ta1				# result exp is FTs
	move	t2, zero			# FSs fraction shifted is zero
	b	4f
1:
	move	ta2, zero			# FTs fraction shifted is zero
	b	4f
2:
	li	t9, 32				# compute 32 - abs(exp diff)
	subu	t9, t9, v1
	bgt	v0, zero, 3f			# if FS > FT, shift FTs frac
	move	t1, ta1				# FT > FS, result exp is FTs
	sll	t8, t2, t9			# save bits shifted out
	srl	t2, t2, v1			# shift FSs fraction
	b	4f
3:
	sll	t8, ta2, t9			# save bits shifted out
	srl	ta2, ta2, v1			# shift FTs fraction
4:
	bne	t0, ta0, 1f			# if signs differ, subtract
	addu	t2, t2, ta2			# add fractions
	b	norm_s
1:
	blt	t2, ta2, 3f			# subtract larger from smaller
	bne	t2, ta2, 2f			# if same, result=0
	move	t1, zero			# result=0
	move	t2, zero
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	bne	v0, FPC_ROUND_RM, 1f	# round to -infinity?
	or	t0, t0, ta0			# compute result sign
	b	result_fs_s
1:
	and	t0, t0, ta0			# compute result sign
	b	result_fs_s
2:
	sltu	t9, zero, t8			# compute t2:zero - ta2:t8
	subu	t8, zero, t8
	subu	t2, t2, ta2			# subtract fractions
	subu	t2, t2, t9			# subtract barrow
	b	norm_s
3:
	move	t0, ta0				# sign of result = FTs
	sltu	t9, zero, t8			# compute ta2:zero - t2:t8
	subu	t8, zero, t8
	subu	t2, ta2, t2			# subtract fractions
	subu	t2, t2, t9			# subtract barrow
	b	norm_s

/*
 * Double precision subtract.
 */
sub_d:
	jal	get_ft_fs_d
	xor	ta0, ta0, 1			# negate sign bit
	b	add_sub_d
/*
 * Double precision add.
 */
add_d:
	jal	get_ft_fs_d
add_sub_d:
	bne	t1, DEXP_INF, 1f		# is FS an infinity?
	bne	ta1, DEXP_INF, result_fs_d	# if FT is not inf, result=FS
	bne	t2, zero, result_fs_d		# if FS is NAN, result is FS
	bne	ta2, zero, result_ft_d		# if FT is NAN, result is FT
	bne	t0, ta0, invalid_d		# both infinities same sign?
	b	result_fs_d			# result is in FS
1:
	beq	ta1, DEXP_INF, result_ft_d	# if FT is inf, result=FT
	bne	t1, zero, 4f			# is FS a denormalized num?
	beq	t2, zero, 3f			# is FS zero?
	bne	ta1, zero, 2f			# is FT a denormalized num?
	beq	ta2, zero, result_fs_d		# FT is zero, result=FS
	jal	renorm_fs_d
	jal	renorm_ft_d
	b	5f
2:
	jal	renorm_fs_d
	subu	ta1, ta1, DEXP_BIAS		# unbias FT exponent
	or	ta2, ta2, DIMPL_ONE		# set implied one bit
	b	5f
3:
	bne	ta1, zero, result_ft_d		# if FT != 0, result=FT
	bne	ta2, zero, result_ft_d
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	bne	v0, FPC_ROUND_RM, 1f	# round to -infinity?
	or	t0, t0, ta0			# compute result sign
	b	result_fs_d
1:
	and	t0, t0, ta0			# compute result sign
	b	result_fs_d
4:
	bne	ta1, zero, 2f			# is FT a denormalized num?
	beq	ta2, zero, result_fs_d		# FT is zero, result=FS
	subu	t1, t1, DEXP_BIAS		# unbias FS exponent
	or	t2, t2, DIMPL_ONE		# set implied one bit
	jal	renorm_ft_d
	b	5f
2:
	subu	t1, t1, DEXP_BIAS		# unbias FS exponent
	or	t2, t2, DIMPL_ONE		# set implied one bit
	subu	ta1, ta1, DEXP_BIAS		# unbias FT exponent
	or	ta2, ta2, DIMPL_ONE		# set implied one bit
/*
 * Perform the addition.
 */
5:
	move	t8, zero			# no shifted bits (sticky reg)
	beq	t1, ta1, 4f			# no shift needed
	subu	v0, t1, ta1			# v0 = difference of exponents
	move	v1, v0				# v1 = abs(difference)
	bge	v0, zero, 1f
	negu	v1
1:
	ble	v1, DFRAC_BITS+2, 2f		# is difference too great?
	li	t8, STICKYBIT			# set the sticky bit
	bge	v0, zero, 1f			# check which exp is larger
	move	t1, ta1				# result exp is FTs
	move	t2, zero			# FSs fraction shifted is zero
	b	4f
1:
	move	ta2, zero			# FTs fraction shifted is zero
	b	4f
2:
	li	t9, 64
	subu	t9, t9, v1
	bge	v0, zero, 3f			# if FS > FT, shift FTs frac
	move	t1, ta1				# FT > FS, result exp is FTs
	dsll	t8, t2, t9			# save bits shifted out
	dsrl	t2, t2, v1
	b	4f
3:
	dsll	t8, ta2, t9			# save bits shifted out
	dsrl	ta2, ta2, v1
4:
	bne	t0, ta0, 1f			# if signs differ, subtract
	daddu	t2, ta2				# add fractions
	b	norm_d
1:
	blt	t2, ta2, 3f			# subtract larger from smaller
	bne	t2, ta2, 2f
	move	t1, zero			# result=0
	move	t2, zero
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	bne	v0, FPC_ROUND_RM, 1f	# round to -infinity?
	or	t0, t0, ta0			# compute result sign
	b	result_fs_d
1:
	and	t0, t0, ta0			# compute result sign
	b	result_fs_d
2:
	sltu	t9, zero, t8			# compute t2:zero - ta2:t8
	dsubu	t8, zero, t8
	dsubu	t2, t2, ta2			# subtract fractions
	dsubu	t2, t2, t9			# subtract barrow
	b	norm_d
3:
	move	t0, ta0				# sign of result = FTs
	sltu	t9, zero, t8
	dsubu	t2, ta2, t2			# subtract fractions
	dsubu	t2, t2, t9			# subtract barrow
	b	norm_d

/*
 * Single precision multiply.
 */
mul_s:
	jal	get_ft_fs_s
	xor	t0, t0, ta0			# compute sign of result
	move	ta0, t0
	bne	t1, SEXP_INF, 2f		# is FS an infinity?
	bne	t2, zero, result_fs_s		# if FS is a NAN, result=FS
	bne	ta1, SEXP_INF, 1f		# FS is inf, is FT an infinity?
	bne	ta2, zero, result_ft_s		# if FT is a NAN, result=FT
	b	result_fs_s			# result is infinity
1:
	bne	ta1, zero, result_fs_s		# inf * zero? if no, result=FS
	bne	ta2, zero, result_fs_s
	b	invalid_s			# infinity * zero is invalid
2:
	bne	ta1, SEXP_INF, 1f		# FS != inf, is FT an infinity?
	bne	t1, zero, result_ft_s		# zero * inf? if no, result=FT
	bne	t2, zero, result_ft_s
	bne	ta2, zero, result_ft_s		# if FT is a NAN, result=FT
	b	invalid_s			# zero * infinity is invalid
1:
	bne	t1, zero, 1f			# is FS zero?
	beq	t2, zero, result_fs_s		# result is zero
	jal	renorm_fs_s
	b	2f
1:
	subu	t1, t1, SEXP_BIAS		# unbias FS exponent
	or	t2, t2, SIMPL_ONE		# set implied one bit
2:
	bne	ta1, zero, 1f			# is FT zero?
	beq	ta2, zero, result_ft_s		# result is zero
	jal	renorm_ft_s
	b	2f
1:
	subu	ta1, ta1, SEXP_BIAS		# unbias FT exponent
	or	ta2, ta2, SIMPL_ONE		# set implied one bit
2:
	addu	t1, t1, ta1			# compute result exponent
	addu	t1, t1, 9			# account for binary point
	multu	t2, ta2				# multiply fractions
	mflo	t8
	mfhi	t2
	b	norm_s

/*
 * Double precision multiply.
 */
mul_d:
	jal	get_ft_fs_d
	xor	t0, t0, ta0			# compute sign of result
	move	ta0, t0
	bne	t1, DEXP_INF, 2f		# is FS an infinity?
	bne	t2, zero, result_fs_d		# if FS is a NAN, result=FS
	bne	ta1, DEXP_INF, 1f		# FS is inf, is FT an infinity?
	bne	ta2, zero, result_ft_d		# if FT is a NAN, result=FT
	b	result_fs_d			# result is infinity
1:
	bne	ta1, zero, result_fs_d		# inf * zero? if no, result=FS
	bne	ta2, zero, result_fs_d
	b	invalid_d			# infinity * zero is invalid
2:
	bne	ta1, DEXP_INF, 1f		# FS != inf, is FT an infinity?
	bne	t1, zero, result_ft_d		# zero * inf? if no, result=FT
	bne	t2, zero, result_ft_d		# if FS is a NAN, result=FS
	bne	ta2, zero, result_ft_d		# if FT is a NAN, result=FT
	b	invalid_d			# zero * infinity is invalid
1:
	bne	t1, zero, 2f			# is FS zero?
	beq	t2, zero, result_fs_d		# result is zero
	jal	renorm_fs_d
	b	3f
2:
	subu	t1, t1, DEXP_BIAS		# unbias FS exponent
	or	t2, t2, DIMPL_ONE		# set implied one bit
3:
	bne	ta1, zero, 2f			# is FT zero?
	beq	ta2, zero, result_ft_d		# result is zero
	jal	renorm_ft_d
	b	3f
2:
	subu	ta1, ta1, DEXP_BIAS		# unbias FT exponent
	or	ta2, ta2, DIMPL_ONE		# set implied one bit
3:
	addu	t1, t1, ta1			# compute result exponent
	addu	t1, t1, 12			# ???
	dmultu	t2, ta2				# multiply fractions
	mflo	t8
	mfhi	t2
	b	norm_d

/*
 * Single precision divide.
 */
div_s:
	jal	get_ft_fs_s
	xor	t0, t0, ta0			# compute sign of result
	move	ta0, t0
	bne	t1, SEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, result_fs_s		# if FS is NAN, result is FS
	bne	ta1, SEXP_INF, result_fs_s	# is FT an infinity?
	bne	ta2, zero, result_ft_s		# if FT is NAN, result is FT
	b	invalid_s			# infinity/infinity is invalid
1:
	bne	ta1, SEXP_INF, 1f		# is FT an infinity?
	bne	ta2, zero, result_ft_s		# if FT is NAN, result is FT
	move	t1, zero			# x / infinity is zero
	move	t2, zero
	b	result_fs_s
1:
	bne	t1, zero, 2f			# is FS zero?
	bne	t2, zero, 1f
	bne	ta1, zero, result_fs_s		# FS=zero, is FT zero?
	beq	ta2, zero, invalid_s		# 0 / 0
	b	result_fs_s			# result = zero
1:
	jal	renorm_fs_s
	b	3f
2:
	subu	t1, t1, SEXP_BIAS		# unbias FS exponent
	or	t2, t2, SIMPL_ONE		# set implied one bit
3:
	bne	ta1, zero, 2f			# is FT zero?
	bne	ta2, zero, 1f
	or	a1, a1, FPC_EXCEPTION_DIV0 | FPC_STICKY_DIV0
	and	v0, a1, FPC_ENABLE_DIV0	# trap enabled?
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	li	t1, SEXP_INF			# result is infinity
	move	t2, zero
	b	result_fs_s
1:
	jal	renorm_ft_s
	b	3f
2:
	subu	ta1, ta1, SEXP_BIAS		# unbias FT exponent
	or	ta2, ta2, SIMPL_ONE		# set implied one bit
3:
	subu	t1, t1, ta1			# compute exponent
	subu	t1, t1, 3			# compensate for result position
	li	v0, SFRAC_BITS+3		# number of bits to divide
	move	t8, t2				# init dividend
	move	t2, zero			# init result
1:
	bltu	t8, ta2, 3f			# is dividend >= divisor?
2:
	subu	t8, t8, ta2			# subtract divisor from dividend
	or	t2, t2, 1			# remember that we did
	bne	t8, zero, 3f			# if not done, continue
	sll	t2, t2, v0			# shift result to final position
	b	norm_s
3:
	sll	t8, t8, 1			# shift dividend
	sll	t2, t2, 1			# shift result
	subu	v0, v0, 1			# are we done?
	bne	v0, zero, 1b			# no, continue
	b	norm_s

/*
 * Double precision divide.
 */
div_d:
	jal	get_ft_fs_d
	xor	t0, t0, ta0			# compute sign of result
	move	ta0, t0
	bne	t1, DEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, result_fs_d		# if FS is NAN, result is FS
	bne	ta1, DEXP_INF, result_fs_d	# is FT an infinity?
	bne	ta2, zero, result_ft_d		# if FT is NAN, result is FT
	b	invalid_d			# infinity/infinity is invalid
1:
	bne	ta1, DEXP_INF, 1f		# is FT an infinity?
	bne	ta2, zero, result_ft_d		# if FT is NAN, result is FT
	move	t1, zero			# x / infinity is zero
	move	t2, zero
	b	result_fs_d
1:
	bne	t1, zero, 2f			# is FS zero?
	bne	t2, zero, 1f
	bne	ta1, zero, result_fs_d		# FS=zero, is FT zero?
	beq	ta2, zero, invalid_d		# 0 / 0
	b	result_fs_d			# result = zero
1:
	jal	renorm_fs_d
	b	3f
2:
	subu	t1, t1, DEXP_BIAS		# unbias FS exponent
	or	t2, t2, DIMPL_ONE		# set implied one bit
3:
	bne	ta1, zero, 2f			# is FT zero?
	bne	ta2, zero, 1f
	or	a1, a1, FPC_EXCEPTION_DIV0 | FPC_STICKY_DIV0
	and	v0, a1, FPC_ENABLE_DIV0	# trap enabled?
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR			# Save exceptions
	li	t1, DEXP_INF			# result is infinity
	move	t2, zero
	b	result_fs_d
1:
	jal	renorm_ft_d
	b	3f
2:
	subu	ta1, ta1, DEXP_BIAS		# unbias FT exponent
	or	ta2, ta2, DIMPL_ONE		# set implied one bit
3:
	subu	t1, t1, ta1			# compute exponent
	subu	t1, t1, 3			# compensate for result position
	li	v0, DFRAC_BITS+3		# number of bits to divide
	move	t8, t2				# init dividend
	move	t2, zero			# init result
1:
	bltu	t8, ta2, 3f			# is dividend >= divisor?
2:
	dsubu	t8, t8, ta2			# subtract divisor from dividend
	or	t2, t2, 1			# remember that we did
	bne	t8, zero, 3f			# if not done, continue
	dsll	t2, t2, v0			# shift upper part
	b	norm_d
3:
	dsll	t8, t8, 1			# shift dividend
	dsll	t2, t2, 1			# shift result
	subu	v0, v0, 1			# are we done?
	bne	v0, zero, 1b			# no, continue
	b	norm_d

/*
 * Single precision absolute value.
 */
abs_s:
	jal	get_fs_s
	move	t0, zero			# set sign positive
	b	result_fs_s

/*
 * Double precision absolute value.
 */
abs_d:
	jal	get_fs_d
	move	t0, zero			# set sign positive
	b	result_fs_d

/*
 * Single precision move.
 */
mov_s:
	jal	get_fs_s
	b	result_fs_s

/*
 * Double precision move.
 */
mov_d:
	jal	get_fs_d
	b	result_fs_d

/*
 * Single precision negate.
 */
neg_s:
	jal	get_fs_s
	xor	t0, t0, 1			# reverse sign
	b	result_fs_s

/*
 * Double precision negate.
 */
neg_d:
	jal	get_fs_d
	xor	t0, t0, 1			# reverse sign
	b	result_fs_d

/*
 * Convert double to single.
 */
cvt_s_d:
	jal	get_fs_d
	bne	t1, DEXP_INF, 1f		# is FS an infinity?
	li	t1, SEXP_INF			# convert to single
	dsll	t2, t2, 3			# convert D fraction to S
	b	result_fs_s
1:
	bne	t1, zero, 2f			# is FS zero?
	beq	t2, zero, result_fs_s		# result=0
	jal	renorm_fs_d
	subu	t1, t1, 3			# correct exp for shift below
	b	3f
2:
	subu	t1, t1, DEXP_BIAS		# unbias exponent
	or	t2, t2, DIMPL_ONE		# add implied one bit
3:
	dsll	t2, t2, 3			# convert D fraction to S
	b	norm_noshift_s

/*
 * Convert long integer to single.
 */
cvt_s_l:
	jal	get_fs_long
	b	cvt_s_int
/*
 * Convert integer to single.
 */
cvt_s_w:
	jal	get_fs_int
cvt_s_int:
	bne	t2, zero, 1f			# check for zero
	move	t1, zero
	b	result_fs_s
/*
 * Find out how many leading zero bits are in t2 and put in t9.
 */
1:
	move	v0, t2
	move	t9, zero
	dsrl	v1, v0, 32
	bne	v1, zero, 1f
	addu	t9, 32
	dsll	v0, 32
1:
	dsrl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	dsll	v0, 16
1:
	dsrl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	dsll	v0, 8
1:
	dsrl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	dsll	v0, 4
1:
	dsrl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	dsll	v0, 2
1:
	dsrl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift t2 the correct number of bits.
 */
1:
	subu	t9, SLEAD_ZEROS			# dont count leading zeros
	li	t1, 23+32			# init exponent
	subu	t1, t1, t9			# compute exponent
	beq	t9, zero, 1f
	li	v0, 32
	blt	t9, zero, 2f			# if shift < 0, shift right
	subu	v0, v0, t9
	sll	t2, t2, t9			# shift left
1:
	add	t1, t1, SEXP_BIAS		# bias exponent
	and	t2, t2, ~SIMPL_ONE		# clear implied one bit
	b	result_fs_s
2:
	negu	t9				# shift right by t9
	subu	v0, v0, t9
	sll	t8, t2, v0			# save bits shifted out
	srl	t2, t2, t9
	b	norm_noshift_s

/*
 * Convert single to double.
 */
cvt_d_s:
	jal	get_fs_s
	dsll	t2, 32
	bne	t1, SEXP_INF, 1f		# is FS an infinity?
	li	t1, DEXP_INF			# convert to double
	b	result_fs_d
1:
	bne	t1, zero, 2f			# is FS denormalized or zero?
	beq	t2, zero, result_fs_d		# is FS zero?
	jal	renorm_fs_s
	move	t8, zero
	b	norm_d
2:
	addu	t1, t1, DEXP_BIAS - SEXP_BIAS	# bias exponent correctly
	dsrl	t2, t2, 3
	b	result_fs_d

/*
 * Convert long integer to double.
 */
cvt_d_l:
	jal	get_fs_long
	b	cvt_d_int
/*
 * Convert integer to double.
 */
cvt_d_w:
	jal	get_fs_int
cvt_d_int:
	bne	t2, zero, 1f			# check for zero
	move	t1, zero			# result=0
	b	result_fs_d
/*
 * Find out how many leading zero bits are in t2 and put in t9.
 */
1:
	move	v0, t2
	move	t9, zero
	dsrl	v1, v0, 32
	bne	v1, zero, 1f
	addu	t9, 32
	dsll	v0, 32
1:
	dsrl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	dsll	v0, 16
1:
	dsrl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	dsll	v0, 8
1:
	dsrl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	dsll	v0, 4
1:
	dsrl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	dsll	v0, 2
1:
	dsrl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift t2 the correct number of bits.
 */
1:
	subu	t9, t9, DLEAD_ZEROS		# dont count leading zeros
	li	t1, DEXP_BIAS + 20		# init exponent
	subu	t1, t1, t9			# compute exponent
	beq	t9, zero, 1f
	li	v0, 64
	blt	t9, zero, 2f			# if shift < 0, shift right
	subu	v0, v0, t9
	dsll	t2, t2, t9			# shift left
1:
	and	t2, t2, ~DIMPL_ONE		# clear implied one bit
	b	result_fs_d
2:
	negu	t9				# shift right by t9
	subu	v0, v0, t9
	dsrl	t2, t2, t9
	and	t2, t2, ~DIMPL_ONE		# clear implied one bit
	b	result_fs_d

/*
 * Convert single to integer with specific rounding.
 */
round_w_s:
	li	t3, FPC_ROUND_RN
	b	do_cvt_w_s
trunc_w_s:
	li	t3, FPC_ROUND_RZ
	b	do_cvt_w_s
ceil_w_s:
	li	t3, FPC_ROUND_RP
	b	do_cvt_w_s
floor_w_s:
	li	t3, FPC_ROUND_RM
	b	do_cvt_w_s

/*
 * Convert single to integer.
 */
cvt_w_s:
	and	t3, a1, FPC_ROUNDING_BITS	# get rounding mode
do_cvt_w_s:
	jal	get_fs_s
	bne	t1, SEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, invalid_w		# invalid conversion
1:
	bne	t1, zero, 1f			# is FS zero?
	beq	t2, zero, result_fs_w		# result is zero
	move	t2, zero			# result is an inexact zero
	b	inexact_w
1:
	subu	t1, t1, SEXP_BIAS		# unbias exponent
	or	t2, t2, SIMPL_ONE		# add implied one bit
	dsll	t2, t2, DFRAC_BITS - SFRAC_BITS	# convert S fraction to D
	b	cvt_w

/*
 * Convert double to integer with specific rounding.
 */
round_w_d:
	li	t3, FPC_ROUND_RN
	b	do_cvt_w_d
trunc_w_d:
	li	t3, FPC_ROUND_RZ
	b	do_cvt_w_d
ceil_w_d:
	li	t3, FPC_ROUND_RP
	b	do_cvt_w_d
floor_w_d:
	li	t3, FPC_ROUND_RM
	b	do_cvt_w_d

/*
 * Convert double to integer.
 */
cvt_w_d:
	and	t3, a1, FPC_ROUNDING_BITS	# get rounding mode
do_cvt_w_d:
	jal	get_fs_d
	bne	t1, DEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, invalid_w		# invalid conversion
1:
	bne	t1, zero, 2f			# is FS zero?
	beq	t2, zero, result_fs_w		# result is zero
	move	t2, zero			# result is an inexact zero
	b	inexact_w
2:
	subu	t1, t1, DEXP_BIAS		# unbias exponent
	or	t2, t2, DIMPL_ONE		# add implied one bit
cvt_w:
	blt	t1, WEXP_MIN, underflow_w	# is exponent too small?
	li	v0, WEXP_MAX+1
	bgt	t1, v0, overflow_w		# is exponent too large?
	bne	t1, v0, 1f			# special check for INT_MIN
	beq	t0, zero, overflow_w		# if positive, overflow
	bne	t2, DIMPL_ONE, overflow_w
	li	t2, INT_MIN			# result is INT_MIN
	b	result_fs_w
1:
	subu	v0, t1, 20			# compute amount to shift
	beq	v0, zero, 2f			# is shift needed?
	li	v1, 64
	blt	v0, zero, 1f			# if shift < 0, shift right
	subu	v1, v1, v0			# shift left
	dsll	t2, t2, v0
	b	2f
1:
	negu	v0				# shift right by v0
	subu	v1, v1, v0
	dsll	t8, t2, v1			# save bits shifted out
	sltu	t8, zero, t8			# dont lose any ones
	dsrl	t2, t2, v0
/*
 * round (t0 is sign, t2:63-32 is integer part, t2:31-0 is fractional part).
 */
2:
	beq	t3, FPC_ROUND_RN, 3f		# round to nearest
	beq	t3, FPC_ROUND_RZ, 5f		# round to zero (truncate)
	beq	t3, FPC_ROUND_RP, 1f		# round to +infinity
	beq	t0, zero, 5f			# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, 5f			# if sign is negative, truncate
2:
	daddu	t2, t2, GUARDBIT		# add in fractional
	blt	t2, zero, overflow_w		# overflow?
	b	5f
3:
	daddu	t2, t2, GUARDBIT		# add in fractional
	blt	t2, zero, overflow_w		# overflow?
4:
	bne	v0, zero, 5f			# if rounded remainder is zero
	and	t2, 0xfffffffe00000000		#  clear LSB (round to nearest)
5:
	beq	t0, zero, 1f			# result positive?
	negu	t2				# convert to negative integer
1:
	dsll	v0, 32				# save fraction
	dsrl	t2, 32				# shift out fractional part
	beq	v0, zero, result_fs_w		# is result exact?
/*
 * Handle inexact exception.
 */
inexact_w:
	or	a1, a1, FPC_EXCEPTION_INEXACT | FPC_STICKY_INEXACT
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	b	result_fs_w

/*
 * Conversions to integer which overflow will trap (if enabled),
 * or generate an inexact trap (if enabled),
 * or generate an invalid exception.
 */
overflow_w:
	or	a1, a1, FPC_EXCEPTION_OVERFLOW | FPC_STICKY_OVERFLOW
	and	v0, a1, FPC_ENABLE_OVERFLOW
	bne	v0, zero, fpe_trap
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, inexact_w		# inexact traps enabled?
	b	invalid_w

/*
 * Conversions to integer which underflow will trap (if enabled),
 * or generate an inexact trap (if enabled),
 * or generate an invalid exception.
 */
underflow_w:
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
	and	v0, a1, FPC_ENABLE_UNDERFLOW
	bne	v0, zero, fpe_trap
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, inexact_w		# inexact traps enabled?
	b	invalid_w

/*
 * Convert single to long integer with specific rounding.
 */
round_l_s:
	li	t3, FPC_ROUND_RN
	b	do_cvt_l_s
trunc_l_s:
	li	t3, FPC_ROUND_RZ
	b	do_cvt_l_s
ceil_l_s:
	li	t3, FPC_ROUND_RP
	b	do_cvt_l_s
floor_l_s:
	li	t3, FPC_ROUND_RM
	b	do_cvt_l_s

/*
 * Convert single to long integer.
 */
cvt_l_s:
	and	t3, a1, FPC_ROUNDING_BITS	# get rounding mode
do_cvt_l_s:
	jal	get_fs_s
	bne	t1, SEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, invalid_l		# invalid conversion
1:
	bne	t1, zero, 1f			# is FS zero?
	beq	t2, zero, result_fs_l		# result is zero
	move	t2, zero			# result is an inexact zero
	b	inexact_l
1:
	subu	t1, t1, SEXP_BIAS		# unbias exponent
	or	t2, t2, SIMPL_ONE		# add implied one bit
	dsll	t2, t2, DFRAC_BITS - SFRAC_BITS	# convert S fraction to D
	b	cvt_l

/*
 * Convert double to long integer with specific rounding.
 */
round_l_d:
	li	t3, FPC_ROUND_RN
	b	do_cvt_l_d
trunc_l_d:
	li	t3, FPC_ROUND_RZ
	b	do_cvt_l_d
ceil_l_d:
	li	t3, FPC_ROUND_RP
	b	do_cvt_l_d
floor_l_d:
	li	t3, FPC_ROUND_RM
	b	do_cvt_l_d

/*
 * Convert double to long integer.
 */
cvt_l_d:
	and	t3, a1, FPC_ROUNDING_BITS	# get rounding mode
do_cvt_l_d:
	jal	get_fs_d
	bne	t1, DEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, invalid_l		# invalid conversion
1:
	bne	t1, zero, 2f			# is FS zero?
	beq	t2, zero, result_fs_l		# result is zero
	move	t2, zero			# result is an inexact zero
	b	inexact_l
2:
	subu	t1, t1, DEXP_BIAS		# unbias exponent
	or	t2, t2, DIMPL_ONE		# add implied one bit
cvt_l:
	blt	t1, LEXP_MIN, underflow_l	# is exponent too small?
	li	v0, LEXP_MAX+1
	bgt	t1, v0, overflow_l		# is exponent too large?
	bne	t1, v0, 1f			# special check for LONG_MIN
	beq	t0, zero, overflow_l		# if positive, overflow
	bne	t2, DIMPL_ONE, overflow_l
	dli	t2, LONG_MIN			# result is LONG_MIN
	b	result_fs_l
1:
	subu	v0, t1, DFRAC_BITS		# compute amount to shift
	beq	v0, zero, 2f			# is shift needed?
	li	v1, 64
	blt	v0, zero, 1f			# if shift < 0, shift right
	subu	v1, v1, v0			# shift left
	dsll	t2, t2, v0
	b	2f
1:
	negu	v0				# shift right by v0
	subu	v1, v1, v0
	dsll	t8, t2, v1			# save bits shifted out
	sltu	t8, zero, t8			# dont lose any ones
	dsrl	t2, t2, v0
/*
 * round (t0 is sign, t2 is integer part).
 */
2:
	beq	t3, FPC_ROUND_RN, 3f		# round to nearest
	beq	t3, FPC_ROUND_RZ, 5f		# round to zero (truncate)
	beq	t3, FPC_ROUND_RP, 1f		# round to +infinity
	beq	t0, zero, 5f			# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, 5f			# if sign is negative, truncate
2:
	daddu	t2, t2, DGUARDBIT		# add in fractional
	blt	t2, zero, overflow_l		# overflow?
	b	5f
3:
	daddu	t2, t2, DGUARDBIT		# add in fractional
	blt	t2, zero, overflow_l		# overflow?
4:
	bne	v0, zero, 5f			# if rounded remainder is zero
	and	t2, 0xe000000000000000		#  clear LSB (round to nearest)
5:
	beq	t0, zero, 1f			# result positive?
	negu	t2				# convert to negative integer
1:
	b	result_fs_l
	nop
/*
 * Handle inexact exception.
 */
inexact_l:
	or	a1, a1, FPC_EXCEPTION_INEXACT | FPC_STICKY_INEXACT
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	b	result_fs_l

/*
 * Conversions to integer which overflow will trap (if enabled),
 * or generate an inexact trap (if enabled),
 * or generate an invalid exception.
 */
overflow_l:
	or	a1, a1, FPC_EXCEPTION_OVERFLOW | FPC_STICKY_OVERFLOW
	and	v0, a1, FPC_ENABLE_OVERFLOW
	bne	v0, zero, fpe_trap
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, inexact_l		# inexact traps enabled?
	b	invalid_l

/*
 * Conversions to integer which underflow will trap (if enabled),
 * or generate an inexact trap (if enabled),
 * or generate an invalid exception.
 */
underflow_l:
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
	and	v0, a1, FPC_ENABLE_UNDERFLOW
	bne	v0, zero, fpe_trap
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, inexact_l		# inexact traps enabled?
	b	invalid_l

/*
 * Compare single.
 */
cmp_s:
	jal	get_cmp_s
	bne	t1, SEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, unordered		# FS is a NAN
1:
	bne	ta1, SEXP_INF, 2f		# is FT an infinity?
	bne	ta2, zero, unordered		# FT is a NAN
2:
	sll	t1, t1, SFRAC_BITS		# reassemble exp & frac
	or	t1, t1, t2
	sll	ta1, ta1, SFRAC_BITS		# reassemble exp & frac
	or	ta1, ta1, ta2
	beq	t0, zero, 1f			# is FS positive?
	negu	t1
1:
	beq	ta0, zero, 1f			# is FT positive?
	negu	ta1
1:
	li	v0, COND_LESS
	blt	t1, ta1, test_cond		# is FS < FT?
	li	v0, COND_EQUAL
	beq	t1, ta1, test_cond		# is FS == FT?
	move	v0, zero			# FS > FT
	b	test_cond

/*
 * Compare double.
 */
cmp_d:
	jal	get_cmp_d
	bne	t1, DEXP_INF, 1f		# is FS an infinity?
	bne	t2, zero, unordered		# FS is a NAN
1:
	bne	ta1, DEXP_INF, 2f		# is FT an infinity?
	bne	ta2, zero, unordered		# FT is a NAN
2:
	dsll	t1, t1, DFRAC_BITS		# reassemble exp & frac
	or	t1, t1, t2
	dsll	ta1, ta1, DFRAC_BITS		# reassemble exp & frac
	or	ta1, ta1, ta2
	beq	t0, zero, 1f			# is FS positive?
	dnegu	t1				# negate t1
1:
	beq	ta0, zero, 1f			# is FT positive?
	dnegu	ta1
1:
	li	v0, COND_LESS
	blt	t1, ta1, test_cond		# is FS(MSW) < FT(MSW)?
	li	v0, COND_EQUAL
	beq	t1, ta1, test_cond		# is FS(LSW) == FT(LSW)?
	move	v0, zero			# FS > FT
test_cond:
	and	v0, v0, a0			# condition match instruction?
set_cond:
	bne	v0, zero, 1f
	and	a1, a1, ~FPC_COND_BIT	# clear condition bit
	b	2f
1:
	or	a1, a1, FPC_COND_BIT	# set condition bit
2:
	ctc1	a1, FPC_CSR		# save condition bit
	b	done

unordered:
	and	v0, a0, COND_UNORDERED		# this cmp match unordered?
	bne	v0, zero, 1f
	and	a1, a1, ~FPC_COND_BIT	# clear condition bit
	b	2f
1:
	or	a1, a1, FPC_COND_BIT	# set condition bit
2:
	and	v0, a0, COND_SIGNAL
	beq	v0, zero, 1f			# is this a signaling cmp?
	or	a1, a1, FPC_EXCEPTION_INVALID | FPC_STICKY_INVALID
	and	v0, a1, FPC_ENABLE_INVALID
	bne	v0, zero, fpe_trap
1:
	ctc1	a1, FPC_CSR		# save condition bit
	b	done

/*
 * Determine the amount to shift the fraction in order to restore the
 * normalized position. After that, round and handle exceptions.
 */
norm_s:
	move	v0, t2
	move	t9, zero			# t9 = num of leading zeros
	bne	t2, zero, 1f
	move	v0, t8
	addu	t9, 32
1:
	srl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	sll	v0, 16
1:
	srl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	sll	v0, 8
1:
	srl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	sll	v0, 4
1:
	srl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	sll	v0, 2
1:
	srl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift t2,t8 the correct number of bits.
 */
1:
	subu	t9, t9, SLEAD_ZEROS		# dont count leading zeros
	subu	t1, t1, t9			# adjust the exponent
	beq	t9, zero, norm_noshift_s
	li	v1, 32
	blt	t9, zero, 1f			# if shift < 0, shift right
	subu	v1, v1, t9
	sll	t2, t2, t9			# shift t2,t8 left
	srl	v0, t8, v1			# save bits shifted out
	or	t2, t2, v0
	sll	t8, t8, t9
	b	norm_noshift_s
1:
	negu	t9				# shift t2,t8 right by t9
	subu	v1, v1, t9
	sll	v0, t8, v1			# save bits shifted out
	sltu	v0, zero, v0			# be sure to save any one bits
	srl	t8, t8, t9
	or	t8, t8, v0
	sll	v0, t2, v1			# save bits shifted out
	or	t8, t8, v0
	srl	t2, t2, t9
norm_noshift_s:
	move	ta1, t1				# save unrounded exponent
	move	ta2, t2				# save unrounded fraction
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, 3f	# round to nearest
	beq	v0, FPC_ROUND_RZ, 5f	# round to zero (truncate)
	beq	v0, FPC_ROUND_RP, 1f	# round to +infinity
	beq	t0, zero, 5f			# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, 5f			# if sign is negative, truncate
2:
	beq	t8, zero, 5f			# if exact, continue
	addu	t2, t2, 1			# add rounding bit
	bne	t2, SIMPL_ONE<<1, 5f		# need to adjust exponent?
	addu	t1, t1, 1			# adjust exponent
	srl	t2, t2, 1			# renormalize fraction
	b	5f
3:
	li	v0, GUARDBIT			# load guard bit for rounding
	addu	v0, v0, t8			# add remainder
	sltu	v1, v0, t8			# compute carry out
	beq	v1, zero, 4f			# if no carry, continue
	addu	t2, t2, 1			# add carry to result
	bne	t2, SIMPL_ONE<<1, 4f		# need to adjust exponent?
	addu	t1, t1, 1			# adjust exponent
	srl	t2, t2, 1			# renormalize fraction
4:
	bne	v0, zero, 5f			# if rounded remainder is zero
	and	t2, t2, ~1			#  clear LSB (round to nearest)
5:
	bgt	t1, SEXP_MAX, overflow_s	# overflow?
	blt	t1, SEXP_MIN, underflow_s	# underflow?
	bne	t8, zero, inexact_s		# is result inexact?
	addu	t1, t1, SEXP_BIAS		# bias exponent
	and	t2, t2, ~SIMPL_ONE		# clear implied one bit
	b	result_fs_s

/*
 * Handle inexact exception.
 */
inexact_s:
	addu	t1, t1, SEXP_BIAS		# bias exponent
	and	t2, t2, ~SIMPL_ONE		# clear implied one bit
inexact_nobias_s:
	jal	set_fd_s			# save result
	or	a1, a1, FPC_EXCEPTION_INEXACT | FPC_STICKY_INEXACT
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	b	done

/*
 * Overflow will trap (if enabled),
 * or generate an inexact trap (if enabled),
 * or generate an infinity.
 */
overflow_s:
	or	a1, a1, FPC_EXCEPTION_OVERFLOW | FPC_STICKY_OVERFLOW
	and	v0, a1, FPC_ENABLE_OVERFLOW
	beq	v0, zero, 1f
	subu	t1, t1, 192			# bias exponent
	and	t2, t2, ~SIMPL_ONE		# clear implied one bit
	jal	set_fd_s			# save result
	b	fpe_trap
1:
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, 3f	# round to nearest
	beq	v0, FPC_ROUND_RZ, 1f	# round to zero (truncate)
	beq	v0, FPC_ROUND_RP, 2f	# round to +infinity
	bne	t0, zero, 3f
1:
	li	t1, SEXP_MAX			# result is max finite
	li	t2, 0x007fffff
	b	inexact_s
2:
	bne	t0, zero, 1b
3:
	li	t1, SEXP_MAX + 1		# result is infinity
	move	t2, zero
	b	inexact_s

/*
 * In this implementation, "tininess" is detected "after rounding" and
 * "loss of accuracy" is detected as "an inexact result".
 */
underflow_s:
	and	v0, a1, FPC_ENABLE_UNDERFLOW
	beq	v0, zero, 1f
/*
 * Underflow is enabled so compute the result and trap.
 */
	addu	t1, t1, 192			# bias exponent
	and	t2, t2, ~SIMPL_ONE		# clear implied one bit
	jal	set_fd_s			# save result
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
	b	fpe_trap
/*
 * Underflow is not enabled so compute the result,
 * signal inexact result (if it is) and trap (if enabled).
 */
1:
	move	t1, ta1				# get unrounded exponent
	move	t2, ta2				# get unrounded fraction
	li	t9, SEXP_MIN			# compute shift amount
	subu	t9, t9, t1			# shift t2,t8 right by t9
	blt	t9, SFRAC_BITS+2, 3f		# shift all the bits out?
	move	t1, zero			# result is inexact zero
	move	t2, zero
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
/*
 * Now round the zero result.
 * Only need to worry about rounding to +- infinity when the sign matches.
 */
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, inexact_nobias_s	# round to nearest
	beq	v0, FPC_ROUND_RZ, inexact_nobias_s	# round to zero
	beq	v0, FPC_ROUND_RP, 1f	# round to +infinity
	beq	t0, zero, inexact_nobias_s	# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, inexact_nobias_s	# if sign is negative, truncate
2:
	addu	t2, t2, 1			# add rounding bit
	b	inexact_nobias_s
3:
	li	v1, 32
	subu	v1, v1, t9
	sltu	v0, zero, t8			# be sure to save any one bits
	sll	t8, t2, v1			# save bits shifted out
	or	t8, t8, v0			# include sticky bits
	srl	t2, t2, t9
/*
 * Now round the denormalized result.
 */
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, 3f	# round to nearest
	beq	v0, FPC_ROUND_RZ, 5f	# round to zero (truncate)
	beq	v0, FPC_ROUND_RP, 1f	# round to +infinity
	beq	t0, zero, 5f			# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, 5f			# if sign is negative, truncate
2:
	beq	t8, zero, 5f			# if exact, continue
	addu	t2, t2, 1			# add rounding bit
	b	5f
3:
	li	v0, GUARDBIT			# load guard bit for rounding
	addu	v0, v0, t8			# add remainder
	sltu	v1, v0, t8			# compute carry out
	beq	v1, zero, 4f			# if no carry, continue
	addu	t2, t2, 1			# add carry to result
4:
	bne	v0, zero, 5f			# if rounded remainder is zero
	and	t2, t2, ~1			#  clear LSB (round to nearest)
5:
	move	t1, zero			# denorm or zero exponent
	jal	set_fd_s			# save result
	beq	t8, zero, done			# check for exact result
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
	or	a1, a1, FPC_EXCEPTION_INEXACT | FPC_STICKY_INEXACT
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	b	done

/*
 * Determine the amount to shift the fraction in order to restore the
 * normalized position. After that, round and handle exceptions.
 */
norm_d:
	move	v0, t2
	move	t9, zero			# t9 = num of leading zeros
	dsrl	v1, v0, 32
	bne	v1, zero, 1f
	addu	t9, 32
	dsll	v0, 32
1:
	dsrl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	dsll	v0, 16
1:
	dsrl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	dsll	v0, 8
1:
	dsrl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	dsll	v0, 4
1:
	dsrl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	dsll	v0, 2
1:
	dsrl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift t2,t8 the correct number of bits.
 */
1:
	subu	t9, t9, DLEAD_ZEROS		# dont count leading zeros
	subu	t1, t1, t9			# adjust the exponent
	beq	t9, zero, norm_noshift_d
	li	v1, 64
	blt	t9, zero, 2f			# if shift < 0, shift right
	subu	v1, v1, t9
	dsll	t2, t2, t9			# shift left by t9
	dsrl	v0, t8, v1			# save bits shifted out
	or	t2, t2, v0
	dsll	t8, t8, t9
	b	norm_noshift_d
2:
	negu	t9				# shift right by t9
	subu	v1, v1, t9			#  (known to be < 32 bits)
	dsll	v0, t8, v1			# save bits shifted out
	sltu	v0, zero, v0			# be sure to save any one bits
	dsrl	t8, t8, t9
	or	t8, t8, v0
	dsll	v0, t2, v1			# save bits shifted out
	or	t8, t8, v0
	dsrl	t2, t2, t9
norm_noshift_d:
	move	ta1, t1				# save unrounded exponent
	move	ta2, t2				# save unrounded fraction (MS)
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, 3f	# round to nearest
	beq	v0, FPC_ROUND_RZ, 5f	# round to zero (truncate)
	beq	v0, FPC_ROUND_RP, 1f	# round to +infinity
	beq	t0, zero, 5f			# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, 5f			# if sign is negative, truncate
2:
	beq	t8, zero, 5f			# if exact, continue
	daddu	t2, t2, 1			# add rounding bit
	bne	t2, DIMPL_ONE<<1, 5f		# need to adjust exponent?
	addu	t1, t1, 1			# adjust exponent
	dsrl	t2, t2, 1			# renormalize fraction
	b	5f
3:
	dli	v0, DGUARDBIT			# load guard bit for rounding
	addu	v0, v0, t8			# add remainder
	sltu	v1, v0, t8			# compute carry out
	beq	v1, zero, 4f			# branch if no carry
	daddu	t2, t2, 1			# add carry to result
	bne	t2, DIMPL_ONE<<1, 4f		# need to adjust exponent?
	addu	t1, t1, 1			# adjust exponent
	srl	t2, t2, 1			# renormalize fraction
4:
	bne	v0, zero, 5f			# if rounded remainder is zero
	and	t2, t2, ~1			#  clear LSB (round to nearest)
5:
	bgt	t1, DEXP_MAX, overflow_d	# overflow?
	blt	t1, DEXP_MIN, underflow_d	# underflow?
	bne	t8, zero, inexact_d		# is result inexact?
	addu	t1, t1, DEXP_BIAS		# bias exponent
	and	t2, t2, ~DIMPL_ONE		# clear implied one bit
	b	result_fs_d

/*
 * Handle inexact exception.
 */
inexact_d:
	addu	t1, t1, DEXP_BIAS		# bias exponent
	and	t2, t2, ~DIMPL_ONE		# clear implied one bit
inexact_nobias_d:
	jal	set_fd_d			# save result
	or	a1, a1, FPC_EXCEPTION_INEXACT | FPC_STICKY_INEXACT
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	b	done

/*
 * Overflow will trap (if enabled),
 * or generate an inexact trap (if enabled),
 * or generate an infinity.
 */
overflow_d:
	or	a1, a1, FPC_EXCEPTION_OVERFLOW | FPC_STICKY_OVERFLOW
	and	v0, a1, FPC_ENABLE_OVERFLOW
	beq	v0, zero, 1f
	subu	t1, t1, 1536			# bias exponent
	and	t2, t2, ~DIMPL_ONE		# clear implied one bit
	jal	set_fd_d			# save result
	b	fpe_trap
1:
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, 3f	# round to nearest
	beq	v0, FPC_ROUND_RZ, 1f	# round to zero (truncate)
	beq	v0, FPC_ROUND_RP, 2f	# round to +infinity
	bne	t0, zero, 3f
1:
	li	t1, DEXP_MAX			# result is max finite
	dli	t2, 0x000fffffffffffff
	b	inexact_d
2:
	bne	t0, zero, 1b
3:
	li	t1, DEXP_MAX + 1		# result is infinity
	move	t2, zero
	b	inexact_d

/*
 * In this implementation, "tininess" is detected "after rounding" and
 * "loss of accuracy" is detected as "an inexact result".
 */
underflow_d:
	and	v0, a1, FPC_ENABLE_UNDERFLOW
	beq	v0, zero, 1f
/*
 * Underflow is enabled so compute the result and trap.
 */
	addu	t1, t1, 1536			# bias exponent
	and	t2, t2, ~DIMPL_ONE		# clear implied one bit
	jal	set_fd_d			# save result
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
	b	fpe_trap
/*
 * Underflow is not enabled so compute the result,
 * signal inexact result (if it is) and trap (if enabled).
 */
1:
	move	t1, ta1				# get unrounded exponent
	move	t2, ta2				# get unrounded fraction (MS)
	li	t9, DEXP_MIN			# compute shift amount
	subu	t9, t9, t1			# shift t2,t8 right by t9
	blt	t9, DFRAC_BITS+2, 3f		# shift all the bits out?
	move	t1, zero			# result is inexact zero
	move	t2, zero
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
/*
 * Now round the zero result.
 * Only need to worry about rounding to +- infinity when the sign matches.
 */
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, inexact_nobias_d	# round to nearest
	beq	v0, FPC_ROUND_RZ, inexact_nobias_d	# round to zero
	beq	v0, FPC_ROUND_RP, 1f	# round to +infinity
	beq	t0, zero, inexact_nobias_d	# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, inexact_nobias_d	# if sign is negative, truncate
2:
	daddu	t2, t2, 1			# add rounding bit
	b	inexact_nobias_d
3:
	li	v1, 64
	subu	v1, v1, t9
	sltu	v0, zero, t8			# be sure to save any one bits
	dsll	t8, t2, v1			# save bits shifted out
	or	t8, t8, v0			# include sticky bits
	dsrl	t2, t2, t9
/*
 * Now round the denormalized result.
 */
	and	v0, a1, FPC_ROUNDING_BITS	# get rounding mode
	beq	v0, FPC_ROUND_RN, 3f	# round to nearest
	beq	v0, FPC_ROUND_RZ, 5f	# round to zero (truncate)
	beq	v0, FPC_ROUND_RP, 1f	# round to +infinity
	beq	t0, zero, 5f			# if sign is positive, truncate
	b	2f
1:
	bne	t0, zero, 5f			# if sign is negative, truncate
2:
	beq	t8, zero, 5f			# if exact, continue
	daddu	t2, t2, 1			# add rounding bit
	b	5f
3:
	dli	v0, DGUARDBIT			# load guard bit for rounding
	daddu	v0, v0, t8			# add remainder
	sltu	v1, v0, t8			# compute carry out
	beq	v1, zero, 4f			# if no carry, continue
	daddu	t2, t2, 1			# add carry
4:
	bne	v0, zero, 5f			# if rounded remainder is zero
	and	t2, t2, ~1			#  clear LSB (round to nearest)
5:
	move	t1, zero			# denorm or zero exponent
	jal	set_fd_d			# save result
	beq	t8, zero, done			# check for exact result
	or	a1, a1, FPC_EXCEPTION_UNDERFLOW | FPC_STICKY_UNDERFLOW
	or	a1, a1, FPC_EXCEPTION_INEXACT | FPC_STICKY_INEXACT
	and	v0, a1, FPC_ENABLE_INEXACT
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	b	done

/*
 * Signal an invalid operation if the trap is enabled; otherwise,
 * the result is a quiet NAN.
 */
invalid_s:					# trap invalid operation
	or	a1, a1, FPC_EXCEPTION_INVALID | FPC_STICKY_INVALID
	and	v0, a1, FPC_ENABLE_INVALID
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	move	t0, zero			# result is a quiet NAN
	li	t1, SEXP_INF
	li	t2, SQUIET_NAN
	jal	set_fd_s			# save result (in t0,t1,t2)
	b	done

/*
 * Signal an invalid operation if the trap is enabled; otherwise,
 * the result is a quiet NAN.
 */
invalid_d:					# trap invalid operation
	or	a1, a1, FPC_EXCEPTION_INVALID | FPC_STICKY_INVALID
	and	v0, a1, FPC_ENABLE_INVALID
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR		# save exceptions
	move	t0, zero			# result is a quiet NAN
	li	t1, DEXP_INF
	dli	t2, DQUIET_NAN
	jal	set_fd_d			# save result (in t0,t1,t2)
	b	done

/*
 * Signal an invalid operation if the trap is enabled; otherwise,
 * the result is INT_MAX or INT_MIN.
 */
invalid_w:					# trap invalid operation
	or	a1, a1, FPC_EXCEPTION_INVALID | FPC_STICKY_INVALID
	and	v0, a1, FPC_ENABLE_INVALID
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR			# save exceptions
	bne	t0, zero, 1f
	li	t2, INT_MAX			# result is INT_MAX
	b	result_fs_w
1:
	li	t2, INT_MIN			# result is INT_MIN
	b	result_fs_w

/*
 * Signal an invalid operation if the trap is enabled; otherwise,
 * the result is LONG_MAX or LONG_MIN.
 */
invalid_l:					# trap invalid operation
	or	a1, a1, FPC_EXCEPTION_INVALID | FPC_STICKY_INVALID
	and	v0, a1, FPC_ENABLE_INVALID
	bne	v0, zero, fpe_trap
	ctc1	a1, FPC_CSR			# save exceptions
	bne	t0, zero, 1f
	dli	t2, LONG_MAX			# result is INT_MAX
	b	result_fs_l
1:
	dli	t2, LONG_MIN			# result is INT_MIN
	b	result_fs_l

/*
 * Trap if the hardware should have handled this case.
 */
fpe_trap:
	move	a2, a1				# code = FP CSR
	ctc1	a1, FPC_CSR			# save exceptions
	li	v0, 1
	b	done_err

/*
 * Send an illegal instruction signal to the current process.
 */
ill:
	ctc1	a1, FPC_CSR			# save exceptions
	move	a2, a0				# code = FP instruction
	li	v0, 1
	b	done_err

result_ft_s:
	move	t0, ta0				# result is FT
	move	t1, ta1
	move	t2, ta2
result_fs_s:					# result is FS
	jal	set_fd_s			# save result (in t0,t1,t2)
	b	done

result_fs_w:
	jal	set_fd_word			# save result (in t2)
	b	done

result_fs_l:
	move	t0, t2
	jal	set_fd_dword			# save result (in t0)
	b	done

result_ft_d:
	move	t0, ta0				# result is FT
	move	t1, ta1
	move	t2, ta2
result_fs_d:					# result is FS
	jal	set_fd_d			# save result (in t0,t1,t2)

done:
	li	v0, 0
done_err:
	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_ADD	sp, sp, FRAMESZ(CF_SZ)
	j	ra
END(MipsEmulateFP)

/*----------------------------------------------------------------------------
 * get_fs_int --
 *
 *	Read (integer) the FS register (bits 15-11).
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the sign
 *	t2	contains the fraction
 *
 *----------------------------------------------------------------------------
 */
#define GET_FS_INT(n) \
	.rdata;				\
	.dword	get_fs_int_ ## n;	\
	.text;				\
get_fs_int_ ## n:			\
	mfc1	t2, $ ## n;		\
	b	get_fs_int_done

LEAF(get_fs_int, 0)
	srl	a3, a0, 11 - 3			# get FS field
	and	a3, a3, 0x1f << 3		# mask FS field
	ld	a3, get_fs_int_tbl(a3)		# switch on register number
	j	a3

	.rdata
get_fs_int_tbl:
	.text

	GET_FS_INT(f0)
	GET_FS_INT(f1)
	GET_FS_INT(f2)
	GET_FS_INT(f3)
	GET_FS_INT(f4)
	GET_FS_INT(f5)
	GET_FS_INT(f6)
	GET_FS_INT(f7)
	GET_FS_INT(f8)
	GET_FS_INT(f9)
	GET_FS_INT(f10)
	GET_FS_INT(f11)
	GET_FS_INT(f12)
	GET_FS_INT(f13)
	GET_FS_INT(f14)
	GET_FS_INT(f15)
	GET_FS_INT(f16)
	GET_FS_INT(f17)
	GET_FS_INT(f18)
	GET_FS_INT(f19)
	GET_FS_INT(f20)
	GET_FS_INT(f21)
	GET_FS_INT(f22)
	GET_FS_INT(f23)
	GET_FS_INT(f24)
	GET_FS_INT(f25)
	GET_FS_INT(f26)
	GET_FS_INT(f27)
	GET_FS_INT(f28)
	GET_FS_INT(f29)
	GET_FS_INT(f30)
	GET_FS_INT(f31)

get_fs_int_done:
	srl	t0, t2, 31		# init the sign bit
	bge	t2, zero, 1f
	negu	t2
	dsll	t2, 33
	dsrl	t2, 33
1:
	j	ra
END(get_fs_int)

/*----------------------------------------------------------------------------
 * get_fs_long --
 *
 *	Read (long integer) the FS register (bits 15-11).
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the sign
 *	t2	contains the fraction
 *
 *----------------------------------------------------------------------------
 */
#define GET_FS_LONG(n) \
	.rdata;				\
	.dword	get_fs_long_ ## n;	\
	.text;				\
get_fs_long_ ## n:			\
	dmfc1	t2, $ ## n;		\
	b	get_fs_long_done

LEAF(get_fs_long, 0)
	srl	a3, a0, 11 - 3			# get FS field
	and	a3, a3, 0x1f << 3		# mask FS field
	ld	a3, get_fs_long_tbl(a3)		# switch on register number
	j	a3

	.rdata
get_fs_long_tbl:
	.text

	GET_FS_LONG(f0)
	GET_FS_LONG(f1)
	GET_FS_LONG(f2)
	GET_FS_LONG(f3)
	GET_FS_LONG(f4)
	GET_FS_LONG(f5)
	GET_FS_LONG(f6)
	GET_FS_LONG(f7)
	GET_FS_LONG(f8)
	GET_FS_LONG(f9)
	GET_FS_LONG(f10)
	GET_FS_LONG(f11)
	GET_FS_LONG(f12)
	GET_FS_LONG(f13)
	GET_FS_LONG(f14)
	GET_FS_LONG(f15)
	GET_FS_LONG(f16)
	GET_FS_LONG(f17)
	GET_FS_LONG(f18)
	GET_FS_LONG(f19)
	GET_FS_LONG(f20)
	GET_FS_LONG(f21)
	GET_FS_LONG(f22)
	GET_FS_LONG(f23)
	GET_FS_LONG(f24)
	GET_FS_LONG(f25)
	GET_FS_LONG(f26)
	GET_FS_LONG(f27)
	GET_FS_LONG(f28)
	GET_FS_LONG(f29)
	GET_FS_LONG(f30)
	GET_FS_LONG(f31)

get_fs_long_done:
	dsrl	t0, t2, 63		# init the sign bit
	bge	t2, zero, 1f
	dnegu	t2
1:
	j	ra
END(get_fs_long)

/*----------------------------------------------------------------------------
 * get_ft_fs_s --
 *
 *	Read (single precision) the FT register (bits 20-16) and
 *	the FS register (bits 15-11) and break up into fields.
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the FS sign
 *	t1	contains the FS (biased) exponent
 *	t2	contains the FS fraction
 *	ta0	contains the FT sign
 *	ta1	contains the FT (biased) exponent
 *	ta2	contains the FT fraction
 *
 *----------------------------------------------------------------------------
 */
#define GET_FT_S(n) \
	.rdata;				\
	.dword	get_ft_s_ ## n;		\
	.text;				\
get_ft_s_ ## n:				\
	mfc1	ta0, $ ## n;		\
	b	get_ft_s_done

LEAF(get_ft_fs_s, 0)
	srl	a3, a0, 16 - 3			# get FT field
	and	a3, a3, 0x1f << 3		# mask FT field
	ld	a3, get_ft_s_tbl(a3)		# switch on register number
	j	a3

	.rdata
get_ft_s_tbl:
	.text

	GET_FT_S(f0)
	GET_FT_S(f1)
	GET_FT_S(f2)
	GET_FT_S(f3)
	GET_FT_S(f4)
	GET_FT_S(f5)
	GET_FT_S(f6)
	GET_FT_S(f7)
	GET_FT_S(f8)
	GET_FT_S(f9)
	GET_FT_S(f10)
	GET_FT_S(f11)
	GET_FT_S(f12)
	GET_FT_S(f13)
	GET_FT_S(f14)
	GET_FT_S(f15)
	GET_FT_S(f16)
	GET_FT_S(f17)
	GET_FT_S(f18)
	GET_FT_S(f19)
	GET_FT_S(f20)
	GET_FT_S(f21)
	GET_FT_S(f22)
	GET_FT_S(f23)
	GET_FT_S(f24)
	GET_FT_S(f25)
	GET_FT_S(f26)
	GET_FT_S(f27)
	GET_FT_S(f28)
	GET_FT_S(f29)
	GET_FT_S(f30)
	GET_FT_S(f31)

get_ft_s_done:
	srl	ta1, ta0, SFRAC_BITS		# get exponent
	and	ta1, ta1, 0xFF
	and	ta2, ta0, 0x7FFFFF		# get fraction
	srl	ta0, ta0, 31			# get sign
	bne	ta1, SEXP_INF, 1f		# is it a signaling NAN?
	and	v0, ta2, SSIGNAL_NAN
	bne	v0, zero, invalid_s
1:
	/* fall through to get FS */

/*----------------------------------------------------------------------------
 * get_fs_s --
 *
 *	Read (single precision) the FS register (bits 15-11) and
 *	break up into fields.
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the sign
 *	t1	contains the (biased) exponent
 *	t2	contains the fraction
 *
 *----------------------------------------------------------------------------
 */
#define GET_FS_S(n) \
	.rdata;				\
	.dword	get_fs_s_ ## n;		\
	.text;				\
get_fs_s_ ## n:				\
	mfc1	t0, $ ## n;		\
	b	get_fs_s_done

ALEAF(get_fs_s)
	srl	a3, a0, 11 - 3			# get FS field
	and	a3, a3, 0x1f << 3		# mask FS field
	ld	a3, get_fs_s_tbl(a3)		# switch on register number
	j	a3

	.rdata
get_fs_s_tbl:
	.text

	GET_FS_S(f0)
	GET_FS_S(f1)
	GET_FS_S(f2)
	GET_FS_S(f3)
	GET_FS_S(f4)
	GET_FS_S(f5)
	GET_FS_S(f6)
	GET_FS_S(f7)
	GET_FS_S(f8)
	GET_FS_S(f9)
	GET_FS_S(f10)
	GET_FS_S(f11)
	GET_FS_S(f12)
	GET_FS_S(f13)
	GET_FS_S(f14)
	GET_FS_S(f15)
	GET_FS_S(f16)
	GET_FS_S(f17)
	GET_FS_S(f18)
	GET_FS_S(f19)
	GET_FS_S(f20)
	GET_FS_S(f21)
	GET_FS_S(f22)
	GET_FS_S(f23)
	GET_FS_S(f24)
	GET_FS_S(f25)
	GET_FS_S(f26)
	GET_FS_S(f27)
	GET_FS_S(f28)
	GET_FS_S(f29)
	GET_FS_S(f30)
	GET_FS_S(f31)

get_fs_s_done:
	srl	t1, t0, SFRAC_BITS		# get exponent
	and	t1, t1, 0xFF
	and	t2, t0, 0x7FFFFF		# get fraction
	srl	t0, t0, 31			# get sign
	bne	t1, SEXP_INF, 1f		# is it a signaling NAN?
	and	v0, t2, SSIGNAL_NAN
	bne	v0, zero, invalid_s
1:
	j	ra
END(get_ft_fs_s)

/*----------------------------------------------------------------------------
 * get_ft_fs_d --
 *
 *	Read (double precision) the FT register (bits 20-16) and
 *	the FS register (bits 15-11) and break up into fields.
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the FS sign
 *	t1	contains the FS (biased) exponent
 *	t2	contains the FS fraction
 *	ta0	contains the FT sign
 *	ta1	contains the FT (biased) exponent
 *	ta2	contains the FT fraction
 *
 *----------------------------------------------------------------------------
 */
#define GET_FT_FS_D(n) \
	.rdata;				\
	.dword	get_ft_fs_d_ ## n;	\
	.text;				\
get_ft_fs_d_ ## n:			\
	dmfc1	ta2, $ ## n;		\
	b	get_ft_d_done

LEAF(get_ft_fs_d, 0)
	srl	a3, a0, 16 - 3			# get FT field
	and	a3, a3, 0x1f << 3		# mask FT field
	ld	a3, get_ft_d_tbl(a3)		# switch on register number
	j	a3

	.rdata
get_ft_d_tbl:
	.text

	GET_FT_FS_D(f0)
	GET_FT_FS_D(f1)
	GET_FT_FS_D(f2)
	GET_FT_FS_D(f3)
	GET_FT_FS_D(f4)
	GET_FT_FS_D(f5)
	GET_FT_FS_D(f6)
	GET_FT_FS_D(f7)
	GET_FT_FS_D(f8)
	GET_FT_FS_D(f9)
	GET_FT_FS_D(f10)
	GET_FT_FS_D(f11)
	GET_FT_FS_D(f12)
	GET_FT_FS_D(f13)
	GET_FT_FS_D(f14)
	GET_FT_FS_D(f15)
	GET_FT_FS_D(f16)
	GET_FT_FS_D(f17)
	GET_FT_FS_D(f18)
	GET_FT_FS_D(f19)
	GET_FT_FS_D(f20)
	GET_FT_FS_D(f21)
	GET_FT_FS_D(f22)
	GET_FT_FS_D(f23)
	GET_FT_FS_D(f24)
	GET_FT_FS_D(f25)
	GET_FT_FS_D(f26)
	GET_FT_FS_D(f27)
	GET_FT_FS_D(f28)
	GET_FT_FS_D(f29)
	GET_FT_FS_D(f30)
	GET_FT_FS_D(f31)

get_ft_d_done:
	dsrl	ta0, ta2, 63			# get sign
	dsrl	ta1, ta2, DFRAC_BITS		# get exponent
	and	ta1, ta1, 0x7FF
	dsll	ta2, 12
	dsrl	ta2, 12				# get fraction
	bne	ta1, DEXP_INF, 1f		# is it a signaling NAN?
	and	v0, ta2, DSIGNAL_NAN
	bne	v0, zero, invalid_d
1:
	/* fall through to get FS */

/*----------------------------------------------------------------------------
 * get_fs_d --
 *
 *	Read (double precision) the FS register (bits 15-11) and
 *	break up into fields.
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the sign
 *	t1	contains the (biased) exponent
 *	t2	contains the fraction
 *
 *----------------------------------------------------------------------------
 */
#define GET_FS_D(n) \
	.rdata;				\
	.dword	get_fs_d_ ## n;		\
	.text;				\
get_fs_d_ ## n:				\
	dmfc1	t2, $ ## n;		\
	b	get_fs_d_done

ALEAF(get_fs_d)
	srl	a3, a0, 11 - 3			# get FS field
	and	a3, a3, 0x1f << 3		# mask FS field
	ld	a3, get_fs_d_tbl(a3)		# switch on register number
	j	a3

	.rdata
get_fs_d_tbl:
	.text

	GET_FS_D(f0)
	GET_FS_D(f1)
	GET_FS_D(f2)
	GET_FS_D(f3)
	GET_FS_D(f4)
	GET_FS_D(f5)
	GET_FS_D(f6)
	GET_FS_D(f7)
	GET_FS_D(f8)
	GET_FS_D(f9)
	GET_FS_D(f10)
	GET_FS_D(f11)
	GET_FS_D(f12)
	GET_FS_D(f13)
	GET_FS_D(f14)
	GET_FS_D(f15)
	GET_FS_D(f16)
	GET_FS_D(f17)
	GET_FS_D(f18)
	GET_FS_D(f19)
	GET_FS_D(f20)
	GET_FS_D(f21)
	GET_FS_D(f22)
	GET_FS_D(f23)
	GET_FS_D(f24)
	GET_FS_D(f25)
	GET_FS_D(f26)
	GET_FS_D(f27)
	GET_FS_D(f28)
	GET_FS_D(f29)
	GET_FS_D(f30)
	GET_FS_D(f31)

get_fs_d_done:
	dsrl	t0, t2, 63			# get sign
	dsrl	t1, t2, DFRAC_BITS		# get exponent
	and	t1, t1, 0x7FF
	dsll	t2, 12
	dsrl	t2, 12				# get fraction
	bne	t1, DEXP_INF, 1f		# is it a signaling NAN?
	and	v0, t2, DSIGNAL_NAN
	bne	v0, zero, invalid_d
1:
	j	ra
END(get_ft_fs_d)

/*----------------------------------------------------------------------------
 * get_cmp_s --
 *
 *	Read (single precision) the FS register (bits 15-11) and
 *	the FT register (bits 20-16) and break up into fields.
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the sign
 *	t1	contains the (biased) exponent
 *	t2	contains the fraction
 *	ta0	contains the sign
 *	ta1	contains the (biased) exponent
 *	ta2	contains the fraction
 *
 *----------------------------------------------------------------------------
 */
#define CMP_FS_S(n) \
	.rdata;				\
	.dword	cmp_fs_s_ ## n;		\
	.text;				\
cmp_fs_s_ ## n:				\
	mfc1	t0, $ ## n;		\
	b	cmp_fs_s_done

LEAF(get_cmp_s, 0)
	srl	a3, a0, 11 - 3			# get FS field
	and	a3, a3, 0x1f << 3		# mask FS field
	ld	a3, cmp_fs_s_tbl(a3)		# switch on register number
	j	a3

	.rdata
cmp_fs_s_tbl:
	.text

	CMP_FS_S(f0)
	CMP_FS_S(f1)
	CMP_FS_S(f2)
	CMP_FS_S(f3)
	CMP_FS_S(f4)
	CMP_FS_S(f5)
	CMP_FS_S(f6)
	CMP_FS_S(f7)
	CMP_FS_S(f8)
	CMP_FS_S(f9)
	CMP_FS_S(f10)
	CMP_FS_S(f11)
	CMP_FS_S(f12)
	CMP_FS_S(f13)
	CMP_FS_S(f14)
	CMP_FS_S(f15)
	CMP_FS_S(f16)
	CMP_FS_S(f17)
	CMP_FS_S(f18)
	CMP_FS_S(f19)
	CMP_FS_S(f20)
	CMP_FS_S(f21)
	CMP_FS_S(f22)
	CMP_FS_S(f23)
	CMP_FS_S(f24)
	CMP_FS_S(f25)
	CMP_FS_S(f26)
	CMP_FS_S(f27)
	CMP_FS_S(f28)
	CMP_FS_S(f29)
	CMP_FS_S(f30)
	CMP_FS_S(f31)

cmp_fs_s_done:
	srl	t1, t0, SFRAC_BITS		# get exponent
	and	t1, t1, 0xFF
	and	t2, t0, 0x7FFFFF		# get fraction
	srl	t0, t0, 31			# get sign

#define CMP_FT_S(n) \
	.rdata;				\
	.dword	cmp_ft_s_ ## n;		\
	.text;				\
cmp_ft_s_ ## n:				\
	mfc1	ta0, $ ## n;		\
	b	cmp_ft_s_done

	srl	a3, a0, 16 - 3			# get FT field
	and	a3, a3, 0x1f << 3		# mask FT field
	ld	a3, cmp_ft_s_tbl(a3)		# switch on register number
	j	a3

	.rdata
cmp_ft_s_tbl:
	.text

	CMP_FT_S(f0)
	CMP_FT_S(f1)
	CMP_FT_S(f2)
	CMP_FT_S(f3)
	CMP_FT_S(f4)
	CMP_FT_S(f5)
	CMP_FT_S(f6)
	CMP_FT_S(f7)
	CMP_FT_S(f8)
	CMP_FT_S(f9)
	CMP_FT_S(f10)
	CMP_FT_S(f11)
	CMP_FT_S(f12)
	CMP_FT_S(f13)
	CMP_FT_S(f14)
	CMP_FT_S(f15)
	CMP_FT_S(f16)
	CMP_FT_S(f17)
	CMP_FT_S(f18)
	CMP_FT_S(f19)
	CMP_FT_S(f20)
	CMP_FT_S(f21)
	CMP_FT_S(f22)
	CMP_FT_S(f23)
	CMP_FT_S(f24)
	CMP_FT_S(f25)
	CMP_FT_S(f26)
	CMP_FT_S(f27)
	CMP_FT_S(f28)
	CMP_FT_S(f29)
	CMP_FT_S(f30)

cmp_ft_s_done:
	srl	ta1, ta0, SFRAC_BITS		# get exponent
	and	ta1, ta1, 0xFF
	and	ta2, ta0, 0x7FFFFF		# get fraction
	srl	ta0, ta0, 31			# get sign
	j	ra
END(get_cmp_s)

/*----------------------------------------------------------------------------
 * get_cmp_d --
 *
 *	Read (double precision) the FS register (bits 15-11) and
 *	the FT register (bits 20-16) and break up into fields.
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Results:
 *	t0	contains the sign
 *	t1	contains the (biased) exponent
 *	t2	contains the fraction
 *	ta0	contains the sign
 *	ta1	contains the (biased) exponent
 *	ta2	contains the fraction
 *
 *----------------------------------------------------------------------------
 */
#define CMP_FS_D(n) \
	.rdata;				\
	.dword	cmp_fs_d_ ## n;		\
	.text;				\
cmp_fs_d_ ## n:				\
	dmfc1	t2, $ ## n;		\
	b	cmp_fs_d_done

LEAF(get_cmp_d, 0)
	srl	a3, a0, 11 - 3			# get FS field
	and	a3, a3, 0x1f << 3		# mask FS field
	ld	a3, cmp_fs_d_tbl(a3)		# switch on register number
	j	a3

	.rdata
cmp_fs_d_tbl:
	.text

	CMP_FS_D(f0)
	CMP_FS_D(f1)
	CMP_FS_D(f2)
	CMP_FS_D(f3)
	CMP_FS_D(f4)
	CMP_FS_D(f5)
	CMP_FS_D(f6)
	CMP_FS_D(f7)
	CMP_FS_D(f8)
	CMP_FS_D(f9)
	CMP_FS_D(f10)
	CMP_FS_D(f11)
	CMP_FS_D(f12)
	CMP_FS_D(f13)
	CMP_FS_D(f14)
	CMP_FS_D(f15)
	CMP_FS_D(f16)
	CMP_FS_D(f17)
	CMP_FS_D(f18)
	CMP_FS_D(f19)
	CMP_FS_D(f20)
	CMP_FS_D(f21)
	CMP_FS_D(f22)
	CMP_FS_D(f23)
	CMP_FS_D(f24)
	CMP_FS_D(f25)
	CMP_FS_D(f26)
	CMP_FS_D(f27)
	CMP_FS_D(f28)
	CMP_FS_D(f29)
	CMP_FS_D(f30)
	CMP_FS_D(f31)

cmp_fs_d_done:
	dsrl	t0, t2, 63			# get sign
	dsrl	t1, t2, DFRAC_BITS		# get exponent
	and	t1, t1, 0x7FF
	dsll	t2, 12
	dsrl	t2, 12				# get fraction

#define CMP_FT_D(n) \
	.rdata;				\
	.dword	cmp_ft_d_ ## n;		\
	.text;				\
cmp_ft_d_ ## n:				\
	dmfc1	ta2, $ ## n;		\
	b	cmp_ft_d_done

	srl	a3, a0, 16 - 3			# get FT field
	and	a3, a3, 0x1f << 3		# mask FT field
	ld	a3, cmp_ft_d_tbl(a3)		# switch on register number
	j	a3

	.rdata
cmp_ft_d_tbl:
	.text

	CMP_FT_D(f0)
	CMP_FT_D(f1)
	CMP_FT_D(f2)
	CMP_FT_D(f3)
	CMP_FT_D(f4)
	CMP_FT_D(f5)
	CMP_FT_D(f6)
	CMP_FT_D(f7)
	CMP_FT_D(f8)
	CMP_FT_D(f9)
	CMP_FT_D(f10)
	CMP_FT_D(f11)
	CMP_FT_D(f12)
	CMP_FT_D(f13)
	CMP_FT_D(f14)
	CMP_FT_D(f15)
	CMP_FT_D(f16)
	CMP_FT_D(f17)
	CMP_FT_D(f18)
	CMP_FT_D(f19)
	CMP_FT_D(f20)
	CMP_FT_D(f21)
	CMP_FT_D(f22)
	CMP_FT_D(f23)
	CMP_FT_D(f24)
	CMP_FT_D(f25)
	CMP_FT_D(f26)
	CMP_FT_D(f27)
	CMP_FT_D(f28)
	CMP_FT_D(f29)
	CMP_FT_D(f30)
	CMP_FT_D(f31)

cmp_ft_d_done:
	dsrl	ta0, ta2, 63			# get sign
	dsrl	ta1, ta2, DFRAC_BITS		# get exponent
	and	ta1, ta1, 0x7FF
	dsll	ta2, 12
	dsrl	ta2, 12				# get fraction
	j	ra
END(get_cmp_d)

/*----------------------------------------------------------------------------
 * set_fd_s --
 *
 *	Write (single precision) the FD register (bits 10-6).
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Arguments:
 *	a0	contains the FP instruction
 *	t0	contains the sign
 *	t1	contains the (biased) exponent
 *	t2	contains the fraction
 *
 * set_fd_word --
 *
 *	Write (integer) the FD register (bits 10-6).
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Arguments:
 *	a0	contains the FP instruction
 *	t2	contains the integer
 *
 *----------------------------------------------------------------------------
 */
#define SET_FD_S(n) \
	.rdata;				\
	.dword	set_fd_s_ ## n;		\
	.text;				\
set_fd_s_ ## n:				\
	mtc1	t2, $ ## n;		\
	j	ra

LEAF(set_fd_s, 0)
	sll	t0, t0, 31			# position sign
	sll	t1, t1, SFRAC_BITS		# position exponent
	or	t2, t2, t0
	or	t2, t2, t1
ALEAF(set_fd_word)
	srl	a3, a0, 6 - 3			# get FD field
	and	a3, a3, 0x1f << 3		# mask FT field
	ld	a3, set_fd_s_tbl(a3)		# switch on register number
	j	a3

	.rdata
set_fd_s_tbl:
	.text

	SET_FD_S(f0)
	SET_FD_S(f1)
	SET_FD_S(f2)
	SET_FD_S(f3)
	SET_FD_S(f4)
	SET_FD_S(f5)
	SET_FD_S(f6)
	SET_FD_S(f7)
	SET_FD_S(f8)
	SET_FD_S(f9)
	SET_FD_S(f10)
	SET_FD_S(f11)
	SET_FD_S(f12)
	SET_FD_S(f13)
	SET_FD_S(f14)
	SET_FD_S(f15)
	SET_FD_S(f16)
	SET_FD_S(f17)
	SET_FD_S(f18)
	SET_FD_S(f19)
	SET_FD_S(f20)
	SET_FD_S(f21)
	SET_FD_S(f22)
	SET_FD_S(f23)
	SET_FD_S(f24)
	SET_FD_S(f25)
	SET_FD_S(f26)
	SET_FD_S(f27)
	SET_FD_S(f28)
	SET_FD_S(f29)
	SET_FD_S(f30)
	SET_FD_S(f31)

END(set_fd_s)

/*----------------------------------------------------------------------------
 * set_fd_d --
 *
 *	Write (double precision) the FT register (bits 10-6).
 *	This is an internal routine used by MipsEmulateFP only.
 *
 * Arguments:
 *	a0	contains the FP instruction
 *	t0	contains the sign
 *	t1	contains the (biased) exponent
 *	t2	contains the fraction
 *
 *----------------------------------------------------------------------------
 */
#define SET_FD_D(n) \
	.rdata;				\
	.dword	set_fd_d_ ## n;		\
	.text;				\
set_fd_d_ ## n:				\
	dmtc1	t0, $ ## n;		\
	j	ra

LEAF(set_fd_d, 0)
	dsll	t0, 63				# set sign
	dsll	t1, t1, DFRAC_BITS		# set exponent
	or	t0, t0, t1
	or	t0, t0, t2			# set fraction
ALEAF(set_fd_dword)
	srl	a3, a0, 6 - 3			# get FD field
	and	a3, a3, 0x1f << 3		# mask FD field
	ld	a3, set_fd_d_tbl(a3)		# switch on register number
	j	a3

	.rdata
set_fd_d_tbl:
	.text

	SET_FD_D(f0)
	SET_FD_D(f1)
	SET_FD_D(f2)
	SET_FD_D(f3)
	SET_FD_D(f4)
	SET_FD_D(f5)
	SET_FD_D(f6)
	SET_FD_D(f7)
	SET_FD_D(f8)
	SET_FD_D(f9)
	SET_FD_D(f10)
	SET_FD_D(f11)
	SET_FD_D(f12)
	SET_FD_D(f13)
	SET_FD_D(f14)
	SET_FD_D(f15)
	SET_FD_D(f16)
	SET_FD_D(f17)
	SET_FD_D(f18)
	SET_FD_D(f19)
	SET_FD_D(f20)
	SET_FD_D(f21)
	SET_FD_D(f22)
	SET_FD_D(f23)
	SET_FD_D(f24)
	SET_FD_D(f25)
	SET_FD_D(f26)
	SET_FD_D(f27)
	SET_FD_D(f28)
	SET_FD_D(f29)
	SET_FD_D(f30)
	SET_FD_D(f31)

END(set_fd_d)

/*----------------------------------------------------------------------------
 * renorm_fs_s --
 *
 * Results:
 *	t1	unbiased exponent
 *	t2	normalized fraction
 *
 *----------------------------------------------------------------------------
 */
LEAF(renorm_fs_s, 0)
/*
 * Find out how many leading zero bits are in t2 and put in t9.
 */
	move	v0, t2
	move	t9, zero
	srl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	sll	v0, 16
1:
	srl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	sll	v0, 8
1:
	srl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	sll	v0, 4
1:
	srl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	sll	v0, 2
1:
	srl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift t2 the correct number of bits.
 */
1:
	subu	t9, t9, SLEAD_ZEROS	# dont count normal leading zeros
	li	t1, SEXP_MIN
	subu	t1, t1, t9		# adjust exponent
	sll	t2, t2, t9
	j	ra
END(renorm_fs_s)

/*----------------------------------------------------------------------------
 * renorm_fs_d --
 *
 * Results:
 *	t1	unbiased exponent
 *	t2	normalized fraction
 *
 *----------------------------------------------------------------------------
 */
LEAF(renorm_fs_d, 0)
/*
 * Find out how many leading zero bits are in t2 and put in t9.
 */
	move	v0, t2
	move	t9, zero
	dsrl	v1, v0, 32
	bne	v1, zero, 1f
	addu	t9, 32
	dsll	v0, 32
1:
	dsrl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	dsll	v0, 16
1:
	dsrl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	dsll	v0, 8
1:
	dsrl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	dsll	v0, 4
1:
	dsrl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	dsll	v0, 2
1:
	dsrl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift t2 the correct number of bits.
 */
1:
	subu	t9, t9, DLEAD_ZEROS	# dont count normal leading zeros
	li	t1, DEXP_MIN
	subu	t1, t9			# adjust exponent
	dsll	t2, t9
	j	ra
END(renorm_fs_d)

/*----------------------------------------------------------------------------
 * renorm_ft_s --
 *
 * Results:
 *	ta1	unbiased exponent
 *	ta2	normalized fraction
 *
 *----------------------------------------------------------------------------
 */
LEAF(renorm_ft_s, 0)
/*
 * Find out how many leading zero bits are in ta2 and put in t9.
 */
	move	v0, ta2
	move	t9, zero
	srl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	sll	v0, 16
1:
	srl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	sll	v0, 8
1:
	srl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	sll	v0, 4
1:
	srl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	sll	v0, 2
1:
	srl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift ta2 the correct number of bits.
 */
1:
	subu	t9, t9, SLEAD_ZEROS	# dont count normal leading zeros
	li	ta1, SEXP_MIN
	subu	ta1, t9			# adjust exponent
	sll	ta2, t9
	j	ra
END(renorm_ft_s)

/*----------------------------------------------------------------------------
 * renorm_ft_d --
 *
 * Results:
 *	ta1	unbiased exponent
 *	ta2	normalized fraction
 *
 *----------------------------------------------------------------------------
 */
LEAF(renorm_ft_d, 0)
/*
 * Find out how many leading zero bits are in ta2 and put in t9.
 */
	move	v0, ta2
	move	t9, zero
	dsrl	v1, v0, 32
	bne	v1, zero, 1f
	addu	t9, 32
	dsll	v0, 32
1:
	dsrl	v1, v0, 16
	bne	v1, zero, 1f
	addu	t9, 16
	dsll	v0, 16
1:
	dsrl	v1, v0, 24
	bne	v1, zero, 1f
	addu	t9, 8
	dsll	v0, 8
1:
	dsrl	v1, v0, 28
	bne	v1, zero, 1f
	addu	t9, 4
	dsll	v0, 4
1:
	dsrl	v1, v0, 30
	bne	v1, zero, 1f
	addu	t9, 2
	dsll	v0, 2
1:
	dsrl	v1, v0, 31
	bne	v1, zero, 1f
	addu	t9, 1
/*
 * Now shift ta2 the correct number of bits.
 */
1:
	subu	t9, t9, DLEAD_ZEROS	# dont count normal leading zeros
	li	ta1, DEXP_MIN
	subu	ta1, t9			# adjust exponent
	dsll	ta2, t9
	j	ra
END(renorm_ft_d)
