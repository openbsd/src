/*	$NetBSD: fpu_extern.h,v 1.1 1996/03/14 19:41:56 christos Exp $	*/

/*
 * Copyright (c) 1995 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
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

struct proc;
struct fpstate;
struct trapframe;
union instr;
struct fpemu;
struct fpn;

/* fpu.c */
void fpu_cleanup __P((struct proc *, struct fpstate *));
int fpu_emulate __P((struct proc *, struct trapframe *, struct fpstate *));
int fpu_execute __P((struct fpemu *, union instr));

/* fpu_add.c */
struct fpn *fpu_add __P((struct fpemu *));

/* fpu_compare.c */
void fpu_compare __P((struct fpemu *, int));

/* fpu_div.c */
struct fpn *fpu_div __P((struct fpemu *));

/* fpu_explode.c */
int fpu_itof __P((struct fpn *, u_int));
int fpu_stof __P((struct fpn *, u_int));
int fpu_dtof __P((struct fpn *, u_int, u_int ));
int fpu_xtof __P((struct fpn *, u_int, u_int , u_int , u_int ));
void fpu_explode __P((struct fpemu *, struct fpn *, int, int ));

/* fpu_implode.c */
u_int fpu_ftoi __P((struct fpemu *, struct fpn *));
u_int fpu_ftos __P((struct fpemu *, struct fpn *));
u_int fpu_ftod __P((struct fpemu *, struct fpn *, u_int *));
u_int fpu_ftox __P((struct fpemu *, struct fpn *, u_int *));
void fpu_implode __P((struct fpemu *, struct fpn *, int, u_int *));

/* fpu_mul.c */
struct fpn *fpu_mul __P((struct fpemu *));

/* fpu_sqrt.c */
struct fpn *fpu_sqrt __P((struct fpemu *));

/* fpu_subr.c */
int fpu_shr __P((register struct fpn *, register int));
void fpu_norm __P((register struct fpn *));
struct fpn *fpu_newnan __P((register struct fpemu *));
