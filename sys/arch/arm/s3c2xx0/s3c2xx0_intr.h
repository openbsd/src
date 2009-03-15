/*	$OpenBSD: s3c2xx0_intr.h,v 1.3 2009/03/15 19:40:40 miod Exp $ */
/*	$NetBSD: s3c2xx0_intr.h,v 1.13 2008/11/19 06:35:55 matt Exp $ */

/*
 * Copyright (c) 2002, 2003 Fujitsu Component Limited
 * Copyright (c) 2002, 2003 Genetec Corporation
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
 * 3. Neither the name of The Fujitsu Component Limited nor the name of
 *    Genetec corporation may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
 * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
 * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Derived from i80321_intr.h */

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

#ifndef _S3C2XX0_INTR_H_
#define _S3C2XX0_INTR_H_

#ifndef _LOCORE

#include <arm/cpu.h>
#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <arm/softintr.h>

void s3c2xx0_setipl(int new);
void s3c2xx0_splx(int new);
int s3c2xx0_splraise(int ipl);
int s3c2xx0_spllower(int ipl);
void s3c2xx0_setsoftintr(int si);

int s3c2xx0_curcpl(void);

void s3c2xx0_set_curcpl(int new);

int	_splraise(int);
int	_spllower(int);
void	splx(int);
void    _setsoftintr(int);

#define	splx(new)		s3c2xx0_splx(new)
#define	_spllower(ipl)		s3c2xx0_spllower(ipl)
#define	_splraise(ipl)		s3c2xx0_splraise(ipl)
#define _setsoftintr(ipl)	s3c2xx0_setsoftintr(ipl)

typedef int (* s3c2xx0_irq_handler_t)(void *);

void s3c2xx0_update_intr_masks( int, int );

extern int s3c2xx0_imask[];
extern int s3c2xx0_smask[];
extern int s3c2xx0_ilevel[];

void s3c2xx0_mask_interrupts(int mask);
void s3c2xx0_unmask_interrupts(int mask);



#if 0

#include <arm/s3c2xx0/s3c2xx0reg.h>

typedef int (* s3c2xx0_irq_handler_t)(void *);

extern volatile uint32_t *s3c2xx0_intr_mask_reg;

extern volatile int global_intr_mask;
#ifdef __HAVE_FAST_SOFTINTS
extern volatile int softint_pending;
#endif
extern int s3c2xx0_imask[];
extern int s3c2xx0_ilevel[];

void s3c2xx0_update_intr_masks( int, int );


int	_splraise(int);
int	_spllower(int);
void	splx(int);

#if !defined(EVBARM_SPL_NOINLINE)

#define	splx(new)		s3c2xx0_splx(new)
#define	_spllower(ipl)		s3c2xx0_spllower(ipl)
#define	_splraise(ipl)		s3c2xx0_splraise(ipl)

#endif	/* !EVBARM_SPL_NOINTR */

#endif

#ifdef DIAGNOSTIC
/*
 * Although this function is implemented in MI code, it must be in this MD
 * header because we don't want this header to include MI includes.
 */
void splassert_fail(int, int, const char *);
extern int splassert_ctl;
void pxa2x0_splassert_check(int, const char *);
#define splassert(__wantipl) do {				\
	if (splassert_ctl > 0) {				\
		pxa2x0_splassert_check(__wantipl, __func__);	\
	}							\
} while (0)
#define splsoftassert(wantipl) splassert(wantipl)
#else
#define splassert(wantipl)      do { /* nothing */ } while (0)
#define splsoftassert(wantipl)      do { /* nothing */ } while (0)
#endif


/*
 * interrupt dispatch table.
 */
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
struct intrhand {
	TAILQ_ENTRY(intrhand) ih_list;	/* link on intrq list */
	s3c2xx0_irq_handler_t ih_func;	/* handler */
	void *ih_arg;			/* arg for handler */
};
#endif

struct s3c2xx0_intr_dispatch {
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	TAILQ_HEAD(,intrhand) list;
#else
	s3c2xx0_irq_handler_t func;
#endif
	void *cookie;		/* NULL for stackframe */
	int level;
	/* struct evbnt ev; */
};

/* used by s3c2{80,40,41}0 interrupt handler */
void s3c2xx0_intr_init(struct s3c2xx0_intr_dispatch *, int );

/* initialize some variable so that splfoo() doesn't touch ileegal
   address during bootstrap */
void s3c2xx0_intr_bootstrap(vaddr_t);
#endif

void s3c2xx0_irq_do_pending(void);

#endif /* _S3C2XX0_INTR_H_ */
