/*	$OpenBSD: fpu_extern.h,v 1.1 2001/09/08 22:33:51 jason Exp $	*/
/*	$NetBSD: fpu_extern.h,v 1.4 2000/08/03 18:32:08 eeh Exp $	*/

/*-
 * Copyright (c) 1995 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct proc;
#ifndef SUN4U
struct fpstate;
struct trapframe;
#else /* SUN4U */
struct fpstate64;
struct trapframe64;
#endif /* SUN4U */
union instr;
struct fpemu;
struct fpn;

/* fpu.c */
#ifndef SUN4U
void fpu_cleanup __P((struct proc *, struct fpstate *));
int fpu_emulate __P((struct proc *, struct trapframe *, struct fpstate *));
#else /* SUN4U */
void fpu_cleanup __P((struct proc *, struct fpstate64 *));
int fpu_emulate __P((struct proc *, struct trapframe64 *, struct fpstate64 *));
#endif /* SUN4U */
int fpu_execute __P((struct fpemu *, union instr));

/* fpu_add.c */
struct fpn *fpu_add __P((struct fpemu *));

/* fpu_compare.c */
void fpu_compare __P((struct fpemu *, int));

/* fpu_div.c */
struct fpn *fpu_div __P((struct fpemu *));

/* fpu_explode.c */
int fpu_itof __P((struct fpn *, u_int));
#ifdef SUN4U
int fpu_xtof __P((struct fpn *, u_int64_t));
#endif /* SUN4U */
int fpu_stof __P((struct fpn *, u_int));
int fpu_dtof __P((struct fpn *, u_int, u_int ));
int fpu_qtof __P((struct fpn *, u_int, u_int , u_int , u_int ));
void fpu_explode __P((struct fpemu *, struct fpn *, int, int ));

/* fpu_implode.c */
u_int fpu_ftoi __P((struct fpemu *, struct fpn *));
#ifdef SUN4U
u_int fpu_ftox __P((struct fpemu *, struct fpn *, u_int *));
#endif /* SUN4U */
u_int fpu_ftos __P((struct fpemu *, struct fpn *));
u_int fpu_ftod __P((struct fpemu *, struct fpn *, u_int *));
u_int fpu_ftoq __P((struct fpemu *, struct fpn *, u_int *));
void fpu_implode __P((struct fpemu *, struct fpn *, int, u_int *));

/* fpu_mul.c */
struct fpn *fpu_mul __P((struct fpemu *));

/* fpu_sqrt.c */
struct fpn *fpu_sqrt __P((struct fpemu *));

/* fpu_subr.c */
int fpu_shr __P((register struct fpn *, register int));
void fpu_norm __P((register struct fpn *));
struct fpn *fpu_newnan __P((register struct fpemu *));
