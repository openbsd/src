/* $NetBSD: psl.h,v 1.2 1996/03/08 16:35:17 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * psl.h
 *
 * spl prototypes.
 * Eventually this will become a set of defines.
 *
 * Created      : 21/07/95
 */

/*
 * These are the different SPL states
 *
 * Each state has an interrupt mask associated with it which
 * indicate which interrupts are allowed.
 */

#define SPL_0		0
#define SPL_SOFT	1
#define SPL_BIO		2
#define SPL_NET		3
#define SPL_TTY		4
#define SPL_CLOCK	5
#define SPL_IMP		6
#define SPL_HIGH	7
#define SPL_LEVELS	8

#define spl0()		splx(SPL_0)
#define splhigh()	splx(SPL_HIGH)
#define splbio()	raisespl(SPL_BIO)
#define splnet()	raisespl(SPL_NET)
#define spltty()	raisespl(SPL_TTY)
#define splimp()	raisespl(SPL_IMP)
#define splclock()	raisespl(SPL_CLOCK)
#define splstatclock()	raisespl(SPL_CLOCK)
#define splsoftnet()	raisespl(SPL_SOFT)

#ifndef _LOCORE
int raisespl __P((int));
int lowerspl __P((int));
int splx __P((int));
int splsoftclock __P(());

#ifdef _KERNEL
extern int current_spl_level;

extern u_int spl_masks[SPL_LEVELS];
#endif /* _KERNEL */
#endif /* _LOCORE */

/* End of psl.h */
