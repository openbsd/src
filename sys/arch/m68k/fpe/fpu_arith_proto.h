/*	$OpenBSD: fpu_arith_proto.h,v 1.3 2002/03/14 01:26:34 millert Exp $	*/
/*	$NetBSD: fpu_arith_proto.h,v 1.1 1995/11/03 04:47:00 briggs Exp $	*/

/*
 * Copyright (c) 1995  Ken Nakata
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	@(#)fpu_arith_proto.c	10/24/95
 */

#ifndef _FPU_ARITH_PROTO_H_
#define _FPU_ARITH_PROTO_H_

/*
 * Arithmetic functions - called from fpu_emul_arith().
 * Each of these may modify its inputs (f1,f2) and/or the temporary.
 * Each returns a pointer to the result and/or sets exceptions.
 */

/* fpu_add.c */
struct fpn * fpu_add(struct fpemu *fe);

/* fpu_div.c */
struct fpn * fpu_div(struct fpemu *fe);

/* fpu_exp.c */
struct fpn * fpu_etox(struct fpemu *fe);
struct fpn * fpu_etoxm1(struct fpemu *fe);
struct fpn * fpu_tentox(struct fpemu *fe);
struct fpn * fpu_twotox(struct fpemu *fe);

/* fpu_getexp.c */
struct fpn * fpu_getexp(struct fpemu *fe);
struct fpn * fpu_getman(struct fpemu *fe);

/* fpu_hyperb.c */
struct fpn * fpu_atanh(struct fpemu *fe);
struct fpn * fpu_cosh(struct fpemu *fe);
struct fpn * fpu_sinh(struct fpemu *fe);
struct fpn * fpu_tanh(struct fpemu *fe);

/* fpu_int.c */
struct fpn * fpu_intrz(struct fpemu *fe);
struct fpn * fpu_int(struct fpemu *fe);

/* fpu_log.c */
struct fpn * fpu_log10(struct fpemu *fe);
struct fpn * fpu_log2(struct fpemu *fe);
struct fpn * fpu_logn(struct fpemu *fe);
struct fpn * fpu_lognp1(struct fpemu *fe);

/* fpu_mulc */
struct fpn * fpu_mul(struct fpemu *fe);

/* fpu_rem.c */
struct fpn * fpu_rem(struct fpemu *fe);
struct fpn * fpu_mod(struct fpemu *fe);

/* fpu_sqrt.c */
struct fpn * fpu_sqrt(struct fpemu *fe);

/* fpu_trig.c */
struct fpn * fpu_acos(struct fpemu *fe);
struct fpn * fpu_asin(struct fpemu *fe);
struct fpn * fpu_atan(struct fpemu *fe);
struct fpn * fpu_cos(struct fpemu *fe);
struct fpn * fpu_sin(struct fpemu *fe);
struct fpn * fpu_tan(struct fpemu *fe);
struct fpn * fpu_sincos(struct fpemu *fe, int regc);

#endif /* _FPU_ARITH_PROTO_H_ */
