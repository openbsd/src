/*	$OpenBSD: avic_intr.h,v 1.2 2009/08/22 02:54:50 mk Exp $ */
/*	$NetBSD: pxa2x0_intr.h,v 1.4 2003/07/05 06:53:08 dogcow Exp $ */

/* Derived from i80321_intr.h */
/* Derived from xscale_intr.h */

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

#ifndef _ARMv7_AVIC_INTR_H_
#define _ARMv7_AVIC_INTR_H_

#define	ARM_IRQ_HANDLER	_C_LABEL(armv7avic_irq_handler)

#ifndef _LOCORE

#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <machine/intr.h>
#include <arm/softintr.h>

extern vaddr_t armv7avic_base;		/* Shared with armv7avic_irq.S */
#define read_icu(offset) (*(volatile uint32_t *)(armv7avic_base+(offset)))
#define write_icu(offset,value) \
 (*(volatile uint32_t *)(armv7avic_base+(offset))=(value))

extern __volatile int current_spl_level;
extern __volatile int softint_pending;
extern int armv7avic_imask[];
void armv7avic_do_pending(void);

#define SI_TO_IRQBIT(si)  (1U<<(si))
void armv7avic_setipl(int new);
void armv7avic_splx(int new);
int armv7avic_splraise(int ipl);
int armv7avic_spllower(int ipl);
void armv7avic_setsoftintr(int si);


/*
 * An useful function for interrupt handlers.
 * XXX: This shouldn't be here.
 */
static __inline int
find_first_bit( uint32_t bits )
{
	int count;

	/* since CLZ is available only on ARMv5, this isn't portable
	 * to all ARM CPUs.  This file is for ARMv7_AVIC processor. 
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
 * The parameter is the virtual address of the ARMv7_AVIC's Interrupt
 * Controller registers.
 */
void armv7avic_intr_bootstrap(vaddr_t);

void armv7avic_irq_handler(void *);
void *armv7avic_intr_establish(int irqno, int level, int (*func)(void *),
    void *cookie, const char *name);
void armv7avic_intr_disestablish(void *cookie);
const char *armv7avic_intr_string(void *cookie);

#endif /* ! _LOCORE */

#endif /* _ARMv7_AVIC_INTR_H_ */

