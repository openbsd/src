/*	$OpenBSD: armish_intr.h,v 1.7 2011/03/23 16:54:34 pirofti Exp $ */
/*	$NetBSD: i80321_intr.h,v 1.4 2003/07/05 06:53:08 dogcow Exp $ */

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_ARMISH_INTR_H_
#define _MACHINE_ARMISH_INTR_H_

#define	ARM_IRQ_HANDLER	_C_LABEL(i80321_irq_handler)

#ifndef _LOCORE

#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <arm/softintr.h>

extern __volatile int current_ipl_level;
extern __volatile int softint_pending;
extern int i80321_imask[];
void i80321_do_pending(void);

void i80321_setipl(int new);
void i80321_splx(int new);
int i80321_splraise(int ipl);
int i80321_spllower(int ipl);
void i80321_setsoftintr(int si);

/*
 * An useful function for interrupt handlers.
 * XXX: This shouldn't be here.
 */
static __inline int
find_first_bit( uint32_t bits )
{
	int count;

	/* since CLZ is available only on ARMv5, this isn't portable
	 * to all ARM CPUs.  This file is for I80321 processor. 
	 */
	asm( "clz %0, %1" : "=r" (count) : "r" (bits) );
	return 31-count;
}


int	_splraise(int);
int	_spllower(int);
void	splx(int);
void	_setsoftintr(int);

/*
 * This function *MUST* be called very early on in a port's
 * initarm() function, before ANY spl*() functions are called.
 *
 * The parameter is the virtual address of the I80321's Interrupt
 * Controller registers.
 */
void i80321_intr_bootstrap(vaddr_t);

void i80321_irq_handler(void *);
void *i80321_intr_establish(int irqno, int level, int (*func)(void *),
    void *cookie, const char *name);
void i80321_intr_disestablish(void *cookie);
const char *i80321_intr_string(void *cookie);

#ifdef DIAGNOSTIC
/*
 * Although this function is implemented in MI code, it must be in this MD
 * header because we don't want this header to include MI includes.
 */
void splassert_fail(int, int, const char *);
extern int splassert_ctl;
void i80321_splassert_check(int, const char *);
#define splassert(__wantipl) do {				\
	if (splassert_ctl > 0) {				\
		i80321_splassert_check(__wantipl, __func__);	\
	}							\
} while (0)
#define	splsoftassert(wantipl) splassert(wantipl)
#else
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#define	splsoftassert(wantipl)	do { /* nothing */ } while (0)
#endif

#endif /* ! _LOCORE */

#endif /* _MACHINE_ARMISH_INTR_H_ */

