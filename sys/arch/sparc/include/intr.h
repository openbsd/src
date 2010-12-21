/*	$OpenBSD: intr.h,v 1.2 2010/12/21 14:56:24 claudio Exp $	*/
/*	$NetBSD: cpu.h,v 1.24 1997/03/15 22:25:15 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _SPARC_INTR_H_
#define _SPARC_INTR_H_

#ifdef _KERNEL
#include <sys/evcount.h>

/*
 * Interrupt handler chains.  Interrupt handlers should return 0 for
 * ``not me'' or 1 (``I took care of it'').  intr_establish() inserts a
 * handler into the list.  The handler is called with its (single)
 * argument, or with a pointer to a clockframe if ih_arg is NULL.
 * ih_ipl specifies the interrupt level that should be blocked when
 * executing this handler.
 */
struct intrhand {
	int			(*ih_fun)(void *);
	void			*ih_arg;
	int			ih_ipl;
	int			ih_vec;		/* ipl for vmstat */
	struct	evcount		ih_count;
	struct	intrhand	*ih_next;	/* global list */
};
extern struct intrhand *intrhand[15];		/* XXX obio.c */

void	intr_establish(int, struct intrhand *, int, const char *);
void	vmeintr_establish(int, int, struct intrhand *, int, const char *);

/*
 * intr_fasttrap() is a lot like intr_establish, but is used for ``fast''
 * interrupt vectors (vectors that are not shared and are handled in the
 * trap window).  Such functions must be written in assembly.
 */
int	intr_fasttrap(int, void (*)(void), int (*)(void *), void *);
void	intr_fastuntrap(int);

void	intr_init(void);

/*
 * Soft interrupt handler chains. In addition to a struct intrhand for
 * proper dispatching, we also remember a pending state as well as the
 * bits to frob in the software interrupt register.
 */
struct sintrhand {
	struct intrhand	sih_ih;
	int		sih_pending;	/* nonzero if triggered */
	int		sih_hw;		/* hw dependent */
	int		sih_ipl;	/* ipl it's registered at */
};

void	 softintr_disestablish(void *);
void	*softintr_establish(int, void (*)(void *), void *);
void	 softintr_schedule(void *);

#endif /* _KERNEL */
#endif /* _SPARC_INTR_H_ */
