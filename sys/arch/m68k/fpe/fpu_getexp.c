/*	$NetBSD: fpu_getexp.c,v 1.1 1995/11/03 04:47:11 briggs Exp $	*/

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
 *	@(#)fpu_getexp.c	10/8/95
 */

#include <sys/types.h>

#include "fpu_emulate.h"

struct fpn *
fpu_getexp(fe)
     struct fpemu *fe;
{
  struct fpn *fp = &fe->fe_f2;

  fe->fe_fpsr &= ~FPSR_EXCP; /* clear all exceptions */

  if (fp->fp_class == FPC_INF) {
    fp = fpu_newnan(fe);
    fe->fe_fpsr |= FPSR_OPERR;
  } else if (fp->fp_class == FPC_NUM) { /* a number */
    fpu_explode(fe, &fe->fe_f3, FTYPE_LNG, &fp->fp_exp);
    fp = &fe->fe_f3;
  } else if (fp->fp_class == FPC_SNAN) { /* signaling NaN */
    fe->fe_fpsr |= FPSR_SNAN;
  } /* else if fp == zero or fp == quiet NaN, return itself */
  return fp;
}

struct fpn *
fpu_getman(fe)
     struct fpemu *fe;
{
  struct fpn *fp = &fe->fe_f2;

  fe->fe_fpsr &= ~FPSR_EXCP; /* clear all exceptions */

  if (fp->fp_class == FPC_INF) {
    fp = fpu_newnan(fe);
    fe->fe_fpsr |= FPSR_OPERR;
  } else if (fp->fp_class == FPC_NUM) { /* a number */
    fp->fp_exp = 0;
  } else if (fp->fp_class == FPC_SNAN) { /* signaling NaN */
    fe->fe_fpsr |= FPSR_SNAN;
  } /* else if fp == zero or fp == quiet NaN, return itself */
  return fp;
}

