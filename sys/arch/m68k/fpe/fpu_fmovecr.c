/*	$NetBSD: fpu_fmovecr.c,v 1.2 1995/11/05 00:35:23 briggs Exp $	*/

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
 *	@(#)fpu_fmovecr.c	10/8/95
 */

#include <sys/types.h>
#include <machine/frame.h>

#include <stddef.h>

#include "fpu_emulate.h"

static struct fpn constrom[] = {
    /* fp_class, fp_sign, fp_exp, fp_sticky, fp_mant[0] ... [3] */
    { FPC_NUM, 0, 1, 0, 0x6487e, 0xd5110b46, 0x11a80000, 0x0 },
    { FPC_NUM, 0, -2, 0, 0x4d104, 0xd427de7f, 0xbcc00000, 0x0 },
    { FPC_NUM, 0, 1, 0, 0x56fc2, 0xa2c515da, 0x54d00000, 0x0 },
    { FPC_NUM, 0, 0, 0, 0x5c551, 0xd94ae0bf, 0x85e00000, 0x0 },
    { FPC_NUM, 0, -2, 0, 0x6f2de, 0xc549b943, 0x8ca80000, 0x0 },
    { FPC_ZERO, 0, 0, 0, 0x0, 0x0, 0x0, 0x0 },
    { FPC_NUM, 0, -1, 0, 0x58b90, 0xbfbe8e7b, 0xcd600000, 0x0 },
    { FPC_NUM, 0, 1, 0, 0x49aec, 0x6eed5545, 0x60b80000, 0x0 },
    { FPC_NUM, 0, 0, 0, 0x40000, 0x0, 0x0, 0x0 },
    { FPC_NUM, 0, 3, 0, 0x50000, 0x0, 0x0, 0x0 },
    { FPC_NUM, 0, 6, 0, 0x64000, 0x0, 0x0, 0x0 },
    { FPC_NUM, 0, 13, 0, 0x4e200, 0x0, 0x0, 0x0 },
    { FPC_NUM, 0, 26, 0, 0x5f5e1, 0x0, 0x0, 0x0 },
    { FPC_NUM, 0, 53, 0, 0x470de, 0x4df82000, 0x0, 0x0 },
    { FPC_NUM, 0, 106, 0, 0x4ee2d, 0x6d415b85, 0xacf00000, 0x0 },
    { FPC_NUM, 0, 212, 0, 0x613c0, 0xfa4ffe7d, 0x36a80000, 0x0 },
    { FPC_NUM, 0, 425, 0, 0x49dd2, 0x3e4c074c, 0x67000000, 0x0 },
    { FPC_NUM, 0, 850, 0, 0x553f7, 0x5fdcefce, 0xf4700000, 0x0 },
    { FPC_NUM, 0, 1700, 0, 0x718cd, 0x5753074, 0x8e380000, 0x0 },
    { FPC_NUM, 0, 3401, 0, 0x64bb3, 0xac340ba8, 0x60b80000, 0x0 },
    { FPC_NUM, 0, 6803, 0, 0x4f459, 0xdaee29ea, 0xef280000, 0x0 },
    { FPC_NUM, 0, 13606, 0, 0x62302, 0x90145104, 0xbcd80000, 0x0 },
};

struct fpn *
fpu_const(fp, offset)
     struct fpn *fp;
     u_int offset;
{
    struct fpn *r;
    int i;

#ifdef DEBUG
    if (fp == NULL) {
	panic("fpu_const: NULL pointer passed\n");
    }
#endif
    if (offset == 0) {
	r = &constrom[0];
    } else if (0xb <= offset && offset <= 0xe) {
	r = &constrom[offset - 0xb + 1];
    } else if (0x30 <= offset && offset <= 0x3f) {
	r = &constrom[offset - 0x30 + 6];
    } else {
	/* return 0.0 for anything else (incl. valid offset 0xf) */
	r = &constrom[5];
    }

    CPYFPN(fp, r);

    return fp;
}

int
fpu_emul_fmovecr(fe, insn)
     struct fpemu *fe;
     struct instruction *insn;
{
    int dstreg, offset, sig;
    u_int *fpreg;

    dstreg = (insn->is_word1 >> 7) & 0x7;
    offset = insn->is_word1 & 0x7F;
    fpreg = &(fe->fe_fpframe->fpf_regs[0]);

    (void)fpu_const(&fe->fe_f3, offset);
    (void)fpu_upd_fpsr(fe, &fe->fe_f3);
    fpu_implode(fe, &fe->fe_f3, FTYPE_EXT, &fpreg[dstreg * 3]);
    if (fpu_debug_level & DL_RESULT) {
	printf("  fpu_emul_fmovecr: result %08x,%08x,%08x to FP%d\n",
	       fpreg[dstreg * 3], fpreg[dstreg * 3 + 1], fpreg[dstreg * 3 + 2],
	       dstreg);
    }
    return 0;
}
