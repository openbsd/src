/*	$OpenBSD: fpemu.S,v 1.6 2002/09/15 09:34:09 mickey Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <machine/asm.h>
#include "assym.h"

#define	FPEMU_VERSION	(1 << 11)

#define	FP_TABLE2(name, ep0, ep1, ep2, ep3) \
	ldil	L%$fpemu_tbl$name, t1			! \
	ldo	R%$fpemu_tbl$name(t1), t1		! \
	ldwx,s	r1(t1), t2				! \
	bv	r0(t2)					! \
	nop						! \
	.label $fpemu_tbl$name				! \
	.import	__CONCAT(__CONCAT(ep0,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep1,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep2,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep3,_),name), code	! \
	.word	__CONCAT(__CONCAT(ep0,_),name),	__CONCAT(__CONCAT(ep1,_),name),	__CONCAT(__CONCAT(ep2,_),name),	__CONCAT(__CONCAT(ep3,_),name)

#define	FP_TABLE3(name,ep0,ep1,ep2,ep3,ep4,ep5,ep6,ep7,ep8,ep9,epa,epb,epc,epd,epe,epf) \
	ldil	L%$fpemu_tbl$name, t1			! \
	ldo	R%$fpemu_tbl$name(t1), t1		! \
	ldwx,s	r1(t1), t2				! \
	bv	r0(t2)					! \
	nop						! \
	.label	$fpemu_tbl$name				! \
	.import	__CONCAT(__CONCAT(ep0,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep1,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep2,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep3,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep4,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep5,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep6,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep7,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep8,_),name), code	! \
	.import	__CONCAT(__CONCAT(ep9,_),name), code	! \
	.import	__CONCAT(__CONCAT(epa,_),name), code	! \
	.import	__CONCAT(__CONCAT(epb,_),name), code	! \
	.import	__CONCAT(__CONCAT(epc,_),name), code	! \
	.import	__CONCAT(__CONCAT(epd,_),name), code	! \
	.import	__CONCAT(__CONCAT(epe,_),name), code	! \
	.import	__CONCAT(__CONCAT(epf,_),name), code	! \
	.word	__CONCAT(__CONCAT(ep0,_),name),	__CONCAT(__CONCAT(ep1,_),name),	__CONCAT(__CONCAT(ep2,_),name), __CONCAT(__CONCAT(ep3,_),name), __CONCAT(__CONCAT(ep4,_),name), __CONCAT(__CONCAT(ep5,_),name), __CONCAT(__CONCAT(ep6,_),name), __CONCAT(__CONCAT(ep7,_),name), __CONCAT(__CONCAT(ep8,_),name), __CONCAT(__CONCAT(ep9,_),name), __CONCAT(__CONCAT(epa,_),name), __CONCAT(__CONCAT(epb,_),name), __CONCAT(__CONCAT(epc,_),name), __CONCAT(__CONCAT(epd,_),name), __CONCAT(__CONCAT(epe,_),name), __CONCAT(__CONCAT(epf,_),name)

	.text
/*
 * fpu_emulate(iir)
 */
LEAF_ENTRY(fpu_emulate)

	extru	arg0, 22, 2, arg3
	extru	arg0, 18, 3, r31
	comib,=	1, arg3, $fpu_cln1
	nop
	extru	arg0, 16, 2, r31

$fpu_cln1
	/*
	 * theoreticaly we would need to determine the fpu instruction
	 * exception type (there could be 4 of those, but stick w/
	 * non-timex fpus for now.
	 */
	ldi	1, ret0
	extru,<> arg0, 10, 5, r1
	ldi	32, r1	/* fpemu zero reg */
	extru,<> arg0, 31, 5, t1
	b,n	$fpemu_nzt
	comib,=,n 2, arg3, $fpemu_exit
$fpemu_nzt
	copy	arg0, t4
	sh3add	r1, arg2, arg0
	extru	arg1, 20, 2, r1
	sh3add	t1, arg2, arg1

	/*
	 * arg0 -- source register (address)
	 * arg1 -- target register (address)
	 * arg2 -- fpregs context
	 * arg3 -- class
	 * r31 -- subop
	 * r1 -- format specifier
	 * (t4 -- copy or arg0, ie iir)
	 */
	comib,=,n 0, arg3, $fpemu0c_0
	comib,=,n 1, arg3, $fpemu0c_1
	comib,=,n 2, arg3, $fpemu0c_2
	comib,=,n 3, arg3, $fpemu0c_3

$fpemu0c_0
	comib,=,n 0, r31, $fpemu0c_0_0
	comib,=,n 1, r31, $fpemu_exit
	comib,=,n 2, r31, $fpemu0c_0_2
	comib,=,n 3, r31, $fpemu0c_0_3
	comib,=,n 4, r31, $fpemu0c_0_4
	comib,=,n 5, r31, $fpemu0c_0_5
	comib,=,n 6, r31, $fpemu_exit
	comib,=,n 7, r31, $fpemu_exit

$fpemu0c_0_0
	ldi	FPEMU_VERSION, t4
	stw	t4, 0(arg2)
	bv	0(rp)
	copy	r0, ret0

$fpemu0c_0_2	/* fcpy */
	comib,=,n 2, r1, $fpemu_exit
	subi	3, r1, r1
	ldw	0*4(arg0), t1
	ldw	1*4(arg0), t2
	ldw	2*4(arg0), t3
	ldw	3*4(arg0), t4
	blr,n	r1, r0
	nop
	stw	t3, 2*4(arg1)
	stw	t4, 3*4(arg1)
	stw	t2, 1*4(arg1)
	nop
	nop
	nop
	stw	t1, 0*4(arg1)
	bv	0(rp)
	copy	r0, ret0

$fpemu0c_0_3	/* fabs */
	comib,=,n 2, r1, $fpemu_exit
	subi	3, r1, r1
	ldw	0*4(arg0), t1
	ldw	1*4(arg0), t2
	ldw	2*4(arg0), t3
	ldw	3*4(arg0), t4
	depi	0, 0, 1, t1
	blr,n	r1, r0
	nop
	stw	t3, 2*4(arg1)
	stw	t4, 3*4(arg1)
	stw	t2, 1*4(arg1)
	nop
	nop
	nop
	stw	t1, 0*4(arg1)
	bv	0(rp)
	copy	r0, ret0

$fpemu0c_0_4	/* fsqrt */
	/* quad not implemented */
	FP_TABLE2(fsqrt,sgl,dbl,invalid,invalid)

$fpemu0c_0_5	/* frnd */
	/* quad not implemented */
	FP_TABLE2(frnd,sgl,dbl,invalid,quad)

$fpemu0c_1
	extru	t4, 18, 2, t2
	sh2add	r1, t2, r1
	comib,=,n 0, r31, $fpemu0c_1_0
	comib,=,n 1, r31, $fpemu0c_1_1
	comib,=,n 2, r31, $fpemu0c_1_2
	comib,=,n 3, r31, $fpemu0c_1_3

$fpemu0c_1_0	/* fcnvff */
#define	sgl_to_quad_fcnvff	invalid_fcnvff
#define	dbl_to_quad_fcnvff	invalid_fcnvff
#define	quad_to_sgl_fcnvff	invalid_fcnvff
#define	quad_to_dbl_fcnvff	invalid_fcnvff
	FP_TABLE3(fcnvff, invalid, sgl_to_dbl, invalid, sgl_to_quad, dbl_to_sgl, invalid, invalid, dbl_to_quad, invalid, invalid, invalid, invalid, quad_to_sgl, quad_to_dbl, invalid, invalid)

$fpemu0c_1_1	/* fcnvxf */
#define	sgl_to_quad_fcnvxf	invalid_fcnvxf
#define	dbl_to_quad_fcnvxf	invalid_fcnvxf
#define	quad_to_sgl_fcnvxf	invalid_fcnvxf
#define	quad_to_dbl_fcnvxf	invalid_fcnvxf
#define	quad_to_quad_fcnvxf	invalid_fcnvxf
	FP_TABLE3(fcnvxf, sgl_to_sgl, sgl_to_dbl, invalid, sgl_to_quad, dbl_to_sgl, dbl_to_dbl, invalid, dbl_to_quad, invalid, invalid, invalid, invalid, quad_to_sgl, quad_to_dbl, invalid, quad_to_quad)

$fpemu0c_1_2	/* fcnvfx */
#define	sgl_to_quad_fcnvfx	invalid_fcnvfx
#define	dbl_to_quad_fcnvfx	invalid_fcnvfx
#define	quad_to_sgl_fcnvfx	invalid_fcnvfx
#define	quad_to_dbl_fcnvfx	invalid_fcnvfx
#define	quad_to_quad_fcnvfx	invalid_fcnvfx
	FP_TABLE3(fcnvfx, sgl_to_sgl, sgl_to_dbl, invalid, sgl_to_quad, dbl_to_sgl, dbl_to_dbl, invalid, dbl_to_quad, invalid, invalid, invalid, invalid, quad_to_sgl, quad_to_dbl, invalid, quad_to_quad)

$fpemu0c_1_3	/* fcnvfxt */
#define	sgl_to_quad_fcnvfxt	invalid_fcnvfxt
#define	dbl_to_quad_fcnvfxt	invalid_fcnvfxt
#define	quad_to_sgl_fcnvfxt	invalid_fcnvfxt
#define	quad_to_dbl_fcnvfxt	invalid_fcnvfxt
#define	quad_to_quad_fcnvfxt	invalid_fcnvfxt
	FP_TABLE3(fcnvfxt, sgl_to_sgl, sgl_to_dbl, invalid, sgl_to_quad, dbl_to_sgl, dbl_to_dbl, invalid, dbl_to_quad, invalid, invalid, invalid, invalid, quad_to_sgl, quad_to_dbl, invalid, quad_to_quad)


$fpemu0c_2
	comib,=,n 0, r31, $fpemu0c_2_0
	comib,=,n 1, r31, $fpemu0c_2_1
	comib,=,n 2, r31, $fpemu_exit
	comib,=,n 3, r31, $fpemu_exit
	comib,=,n 4, r31, $fpemu_exit
	comib,=,n 5, r31, $fpemu_exit
	comib,=,n 6, r31, $fpemu_exit
	comib,=,n 7, r31, $fpemu_exit

$fpemu0c_2_0
	copy	arg2, arg3
	extru,<> t4, 15, 5, t1
	ldi	32, t1
	sh3add	t1, arg3, arg1
	extru	t4, 31, 5, arg2
	FP_TABLE2(fcmp,sgl,dbl,invalid,invalid)

$fpemu0c_2_1
	comib,<>,n 0, r1, $fpemu_exit

	/* extru	t4, 31, 5, arg1 */
	/* XXX timex is much more compilicated */
	ldw	0(arg2), t1
	ldi	0, ret0
	extru,<> t1, 5, 1, r0
	bv,n	r0(rp)

	/* advance the pcqueue */
	mtctl	r0, pcsq
	mfctl	pcsq, t2
	mtctl	t2, pcsq
	mtctl	t2, pcsq
	mtctl	r0, pcoq
	mfctl	pcoq, t2
	mtctl	t2, pcoq
	ldo	4(t2), t2
	bv	r0(rp)
	mtctl	t2, pcoq

$fpemu0c_3
	copy	arg2, arg3
	extru,<> t4, 31, 5, t1
	ldi	32, t1
	sh3add	t1, arg3, arg2

	comib,=,n 0, r31, $fpemu0c_3_0
	comib,=,n 1, r31, $fpemu0c_3_1
	comib,=,n 2, r31, $fpemu0c_3_2
	comib,=,n 3, r31, $fpemu0c_3_3
	comib,=,n 4, r31, $fpemu0c_3_4
	comib,=,n 5, r31, $fpemu_exit
	comib,=,n 6, r31, $fpemu_exit
	comib,=,n 7, r31, $fpemu_exit

$fpemu0c_3_0	/* fadd */
	FP_TABLE2(fadd,sgl,dbl,invalid,invalid)

$fpemu0c_3_1	/* fsub */
	FP_TABLE2(fsub,sgl,dbl,invalid,invalid)

$fpemu0c_3_2	/* fmpy */
	FP_TABLE2(fmpy,sgl,dbl,invalid,invalid)

$fpemu0c_3_3	/* fdiv */
	FP_TABLE2(fdiv,sgl,dbl,invalid,invalid)

$fpemu0c_3_4	/* frem */
	FP_TABLE2(frem,sgl,dbl,invalid,invalid)

	.export	$fpemu_exit, code
$fpemu_exit
	/* these look very ugly, but we don't want to mess up w/ m4 just
	 * for the sake of overall world prettieness value growth XXX */
invalid_fsqrt
invalid_frnd
invalid_fcnvff
invalid_fcnvxf
invalid_fcnvfx
invalid_fcnvfxt
invalid_fcmp
invalid_fadd
invalid_fsub
invalid_fmpy
invalid_fdiv
invalid_frem
	bv,n	0(rp)
EXIT(fpu_emulate)

	.end
