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
struct fpn * fpu_add __P((struct fpemu *fe));

/* fpu_div.c */
struct fpn * fpu_div __P((struct fpemu *fe));

/* fpu_exp.c */
struct fpn * fpu_etox __P((struct fpemu *fe));
struct fpn * fpu_etoxm1 __P((struct fpemu *fe));
struct fpn * fpu_tentox __P((struct fpemu *fe));
struct fpn * fpu_twotox __P((struct fpemu *fe));

/* fpu_getexp.c */
struct fpn * fpu_getexp __P((struct fpemu *fe));
struct fpn * fpu_getman __P((struct fpemu *fe));

/* fpu_hyperb.c */
struct fpn * fpu_atanh __P((struct fpemu *fe));
struct fpn * fpu_cosh __P((struct fpemu *fe));
struct fpn * fpu_sinh __P((struct fpemu *fe));
struct fpn * fpu_tanh __P((struct fpemu *fe));

/* fpu_int.c */
struct fpn * fpu_intrz __P((struct fpemu *fe));
struct fpn * fpu_int __P((struct fpemu *fe));

/* fpu_log.c */
struct fpn * fpu_log10 __P((struct fpemu *fe));
struct fpn * fpu_log2 __P((struct fpemu *fe));
struct fpn * fpu_logn __P((struct fpemu *fe));
struct fpn * fpu_lognp1 __P((struct fpemu *fe));

/* fpu_mulc */
struct fpn * fpu_mul __P((struct fpemu *fe));

/* fpu_rem.c */
struct fpn * fpu_rem __P((struct fpemu *fe));
struct fpn * fpu_mod __P((struct fpemu *fe));

/* fpu_sqrt.c */
struct fpn * fpu_sqrt __P((struct fpemu *fe));

/* fpu_trig.c */
struct fpn * fpu_acos __P((struct fpemu *fe));
struct fpn * fpu_asin __P((struct fpemu *fe));
struct fpn * fpu_atan __P((struct fpemu *fe));
struct fpn * fpu_cos __P((struct fpemu *fe));
struct fpn * fpu_sin __P((struct fpemu *fe));
struct fpn * fpu_tan __P((struct fpemu *fe));
struct fpn * fpu_sincos __P((struct fpemu *fe, int regc));

#endif /* _FPU_ARITH_PROTO_H_ */
