/*	$OpenBSD: reg.h,v 1.9 2001/12/13 08:55:51 smurph Exp $ */
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
#ifndef _M88K_REG_H_
#define _M88K_REG_H_

/* This must always be an even number of words long */
struct reg {
    unsigned r[32];  /* 0 - 31 */
#define   tf_sp r[31]
    unsigned epsr;   /* 32 */
    unsigned fpsr;
    unsigned fpcr;
    unsigned sxip;
#define exip sxip	/* mc88110 */
    unsigned snip;
#define enip snip	/* mc88110 */
    unsigned sfip;
    unsigned ssbr;
#define duap ssbr	/* mc88110 */
    unsigned dmt0;
#define dsr dmt0	/* mc88110 */
    unsigned dmd0;
#define dlar dmd0	/* mc88110 */
    unsigned dma0;
#define dpar dma0	/* mc88110 */
    unsigned dmt1;
#define isr dmt1	/* mc88110 */
    unsigned dmd1;
#define ilar dmd1	/* mc88110 */
    unsigned dma1;
#define ipar dma1	/* mc88110 */
    unsigned dmt2;
#define isap dmt2	/* mc88110 */
    unsigned dmd2;
#define dsap dmd2	/* mc88110 */
    unsigned dma2;
#define iuap dma2	/* mc88110 */
    unsigned fpecr;
    unsigned fphs1;
    unsigned fpls1;
    unsigned fphs2;
    unsigned fpls2;
    unsigned fppt;
    unsigned fprh;
    unsigned fprl;
    unsigned fpit;
    unsigned vector;	      /* exception vector number */
    unsigned mask;	      /* interrupt mask level */
    unsigned mode;	      /* interrupt mode */
    unsigned scratch1;	   /* used by locore trap handling code */
    unsigned ipfsr;        /* P BUS status - used in inst fault handling */
    unsigned dpfsr;        /* P BUS status - used in data fault handling */
    unsigned cpu;          /* cpu number */
};

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

#endif /* _M88K_REG_H_ */
