/*	$OpenBSD: a1xintc.h,v 1.1 2013/10/22 13:22:18 jasper Exp $ */
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

#ifndef _A1XINTC_H_
#define _A1XINTC_H_

#ifndef _LOCORE

#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <machine/intr.h>
#include <arm/softintr.h>

extern __volatile int current_spl_level;
extern __volatile int softint_pending;
void intc_do_pending(void);

#define SI_TO_IRQBIT(si)  (1U<<(si))
void intc_setipl(int);
void intc_splx(int);
int intc_splraise(int);
int intc_spllower(int);
void intc_setsoftintr(int);

void intc_irq_handler(void *);
void *intc_intr_establish(int, int, int (*)(void *), void *, char *);
void intc_intr_disestablish(void *);
const char *intc_intr_string(void *);

#endif /* ! _LOCORE */

#endif /* _A1XINTC_H_ */

