/*	$OpenBSD: fpu_qp.c,v 1.2 2004/09/28 18:03:36 otto Exp $	*/

/*-
 * Copyright (c) 2002 Jake Burkholder.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if 0
__FBSDID("$FreeBSD: src/lib/libc/sparc64/fpu/fpu_qp.c,v 1.3 2002/09/02 02:30:20 jake Exp $");
#endif

#include <sys/types.h>
#include <machine/fsr.h>

#include "fpu_emu.h"
#include "fpu_extern.h"

#define	_QP_OP(op) \
void _Qp_ ## op(u_int *c, u_int *a, u_int *b); \
void \
_Qp_ ## op(u_int *c, u_int *a, u_int *b) \
{ \
	struct fpemu fe; \
	struct fpn *r; \
	__asm __volatile("stx %%fsr, [%0]" : : "r" (&fe.fe_fsr)); \
	fe.fe_f1.fp_sign = a[0] >> 31; \
	fe.fe_f1.fp_sticky = 0; \
	fe.fe_f1.fp_class = __fpu_qtof(&fe.fe_f1, a[0], a[1], a[2], a[3]); \
	fe.fe_f2.fp_sign = b[0] >> 31; \
	fe.fe_f2.fp_sticky = 0; \
	fe.fe_f2.fp_class = __fpu_qtof(&fe.fe_f2, b[0], b[1], b[2], b[3]); \
	r = __fpu_ ## op(&fe); \
	c[0] = __fpu_ftoq(&fe, r, c); \
}

#define	_QP_TTOQ(qname, fname, ntype, atype, signed, ...) \
void _Qp_ ## qname ## toq(u_int *c, ntype n); \
void \
_Qp_ ## qname ## toq(u_int *c, ntype n) \
{ \
	struct fpemu fe; \
	atype *a; \
	__asm __volatile("stx %%fsr, %0" : "=m" (fe.fe_fsr) :); \
	a = (atype *)&n; \
	fe.fe_f1.fp_sign = signed ? a[0] >> 31 : 0; \
	fe.fe_f1.fp_sticky = 0; \
	fe.fe_f1.fp_class = __fpu_ ## fname ## tof(&fe.fe_f1, __VA_ARGS__); \
	c[0] = __fpu_ftoq(&fe, &fe.fe_f1, c); \
}

#define	_QP_QTOT4(qname, fname, type, x)		\
type _Qp_qto ## qname(u_int *c); \
type \
_Qp_qto ## qname(u_int *c) \
{ \
	struct fpemu fe; \
	u_int *a; \
	type n; \
	__asm __volatile("stx %%fsr, %0" : "=m" (fe.fe_fsr) :); \
	a = (u_int *)&n; \
	fe.fe_f1.fp_sign = c[0] >> 31; \
	fe.fe_f1.fp_sticky = 0; \
	fe.fe_f1.fp_class = __fpu_qtof(&fe.fe_f1, c[0], c[1], c[2], c[3]); \
	a[0] = __fpu_fto ## fname(&fe, &fe.fe_f1, x); \
	return (n); \
}

#define	_QP_QTOT3(qname, fname, type)		\
type _Qp_qto ## qname(u_int *c); \
type \
_Qp_qto ## qname(u_int *c) \
{ \
	struct fpemu fe; \
	u_int *a; \
	type n; \
	__asm __volatile("stx %%fsr, %0" : "=m" (fe.fe_fsr) :); \
	a = (u_int *)&n; \
	fe.fe_f1.fp_sign = c[0] >> 31; \
	fe.fe_f1.fp_sticky = 0; \
	fe.fe_f1.fp_class = __fpu_qtof(&fe.fe_f1, c[0], c[1], c[2], c[3]); \
	a[0] = __fpu_fto ## fname(&fe, &fe.fe_f1); \
	return (n); \
}

#define	_QP_QTOT(qname, fname, type, ...) \
type _Qp_qto ## qname(u_int *c); \
type \
_Qp_qto ## qname(u_int *c) \
{ \
	struct fpemu fe; \
	u_int *a; \
	type n; \
	__asm __volatile("stx %%fsr, %0" : "=m" (fe.fe_fsr) :); \
	a = (u_int *)&n; \
	fe.fe_f1.fp_sign = c[0] >> 31; \
	fe.fe_f1.fp_sticky = 0; \
	fe.fe_f1.fp_class = __fpu_qtof(&fe.fe_f1, c[0], c[1], c[2], c[3]); \
	a[0] = __fpu_fto ## fname(&fe, &fe.fe_f1, ## __VA_ARGS__); \
	return (n); \
}

#define	FCC_EQ(fcc)	((fcc) == FSR_CC_EQ)
#define	FCC_GE(fcc)	((fcc) == FSR_CC_EQ || (fcc) == FSR_CC_GT)
#define	FCC_GT(fcc)	((fcc) == FSR_CC_GT)
#define	FCC_LE(fcc)	((fcc) == FSR_CC_EQ || (fcc) == FSR_CC_LT)
#define	FCC_LT(fcc)	((fcc) == FSR_CC_LT)
#define	FCC_NE(fcc)	((fcc) != FSR_CC_EQ)

#define	FSR_GET_FCC0(fsr)	(((fsr) >> FSR_FCC_SHIFT) & FSR_FCC_MASK)

#define	_QP_CMP(name, cmpe, test) \
int _Qp_f ## name(u_int *a, u_int *b) ; \
int \
_Qp_f ## name(u_int *a, u_int *b) \
{ \
	struct fpemu fe; \
	__asm __volatile("stx %%fsr, %0" : "=m" (fe.fe_fsr) :); \
	fe.fe_f1.fp_sign = a[0] >> 31; \
	fe.fe_f1.fp_sticky = 0; \
	fe.fe_f1.fp_class = __fpu_qtof(&fe.fe_f1, a[0], a[1], a[2], a[3]); \
	fe.fe_f2.fp_sign = b[0] >> 31; \
	fe.fe_f2.fp_sticky = 0; \
	fe.fe_f2.fp_class = __fpu_qtof(&fe.fe_f2, b[0], b[1], b[2], b[3]); \
	__fpu_compare(&fe, cmpe, 0); \
	return (test(FSR_GET_FCC0(fe.fe_fsr))); \
}

void _Qp_sqrt(u_int *c, u_int *a);
void
_Qp_sqrt(u_int *c, u_int *a)
{
	struct fpemu fe;
	struct fpn *r;
	__asm __volatile("stx %%fsr, %0" : "=m" (fe.fe_fsr) :);
	fe.fe_f1.fp_sign = a[0] >> 31;
	fe.fe_f1.fp_sticky = 0;
	fe.fe_f1.fp_class = __fpu_qtof(&fe.fe_f1, a[0], a[1], a[2], a[3]);
	r = __fpu_sqrt(&fe);
	c[0] = __fpu_ftoq(&fe, r, c);
}

_QP_OP(add)
_QP_OP(div)
_QP_OP(mul)
_QP_OP(sub)

_QP_TTOQ(d,	d,	double,	u_int,	1, a[0], a[1])
_QP_TTOQ(i,	i,	int,	u_int,	1, a[0])
_QP_TTOQ(s,	s,	float,	u_int,	1, a[0])
_QP_TTOQ(x,	x,	long,	u_long,	1, a[0])
_QP_TTOQ(ui,	ui,	u_int,	u_int,	0, a[0])
_QP_TTOQ(ux,	ux,	u_long,	u_long,	0, a[0])

_QP_QTOT4(d,	d,	double,	a)
_QP_QTOT3(i,	i,	int)
_QP_QTOT3(s,	s,	float)
_QP_QTOT4(x,	x,	long,	a)
_QP_QTOT3(ui,	i,	u_int)
_QP_QTOT4(ux,	x,	u_long,	a)

_QP_CMP(eq,	0,	FCC_EQ)
_QP_CMP(ge,	0,	FCC_GE)
_QP_CMP(gt,	0,	FCC_GT)
_QP_CMP(le,	0,	FCC_LE)
_QP_CMP(lt,	0,	FCC_LT)
_QP_CMP(ne,	0, 	FCC_NE)
