/*	$OpenBSD: reg.h,v 1.4 1999/02/09 06:36:27 smurph Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */
#include <machine/pcb.h>

struct reg {
    unsigned r_r[32];
    unsigned r_fpsr;
    unsigned r_fpcr;
    unsigned r_epsr;
    unsigned r_sxip;
    unsigned r_snip;
    unsigned r_sfip;
    unsigned r_ssbr;
    unsigned r_dmt0;
    unsigned r_dmd0;
    unsigned r_dma0;
    unsigned r_dmt1;
    unsigned r_dmd1;
    unsigned r_dma1;
    unsigned r_dmt2;
    unsigned r_dmd2;
    unsigned r_dma2;
    unsigned r_fpecr;
    unsigned r_fphs1;
    unsigned r_fpls1;
    unsigned r_fphs2;
    unsigned r_fpls2;
    unsigned r_fppt;
    unsigned r_fprh;
    unsigned r_fprl;
    unsigned r_fpit;
    unsigned r_vector;   /* exception vector number */
    unsigned r_mask;	   /* interrupt mask level */
    unsigned r_mode;     /* interrupt mode */
    unsigned r_scratch1; /* used by locore trap handling code */
    unsigned r_pad;      /* to make an even length */
} ;

struct fpreg {
    unsigned fp_fpecr;
    unsigned fp_fphs1;
    unsigned fp_fpls1;
    unsigned fp_fphs2;
    unsigned fp_fpls2;
    unsigned fp_fppt;
    unsigned fp_fprh;
    unsigned fp_fprl;
    unsigned fp_fpit;
};
