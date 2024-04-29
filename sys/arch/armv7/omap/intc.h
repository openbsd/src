/*	$OpenBSD: intc.h,v 1.7 2024/04/29 12:42:06 jsg Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _OMAPINTC_INTR_H_
#define _OMAPINTC_INTR_H_

#ifndef _LOCORE

#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <machine/intr.h>
#include <arm/softintr.h>

#define SI_TO_IRQBIT(si)  (1U<<(si))
void intc_setipl(int new);
void intc_splx(int new);
int intc_splraise(int ipl);
int intc_spllower(int ipl);

/*
 * An useful function for interrupt handlers.
 * XXX: This shouldn't be here.
 */
static __inline int
find_first_bit( uint32_t bits )
{
	int count;

	/* since CLZ is available only on ARMv5, this isn't portable
	 * to all ARM CPUs.  This file is for OMAPINTC processor.
	 */
	asm( "clz %0, %1" : "=r" (count) : "r" (bits) );
	return 31-count;
}

void intc_irq_handler(void *);
void *intc_intr_establish(int irqno, int level, struct cpu_info *ci,
    int (*func)(void *), void *cookie, char *name);
void intc_intr_disestablish(void *cookie);
const char *intc_intr_string(void *cookie);

#endif /* ! _LOCORE */

#endif /* _OMAPINTC_INTR_H_ */

