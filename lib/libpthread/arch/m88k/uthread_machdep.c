/*	$OpenBSD: uthread_machdep.c,v 1.2 2004/03/02 23:41:29 miod Exp $	*/

/*
 * Copyright (c) 2004 Theo de Raadt
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

/*
 * Machine-dependent thread state functions for m88k
 */

#include <pthread.h>
#include "pthread_private.h"

#define ALIGNBYTES	7

struct frame {
	long	regs[28];	/* r4-r30, r1 */
};

/*
 * Given a stack and an entry function, initialise a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(statep, base, len, entry)
	struct _machdep_state* statep;
	void *base;
	int len;
	void (*entry)(void);
{
	struct frame *f;

	f = (struct frame *)(((u_int32_t)base + len - sizeof *f) & ~ALIGNBYTES);
	f->regs[27] = (u_int32_t)entry;		/* ``saved'' r1 */

	statep->sp = (int)f;
}

void
_thread_machdep_save_float_state(statep)
	struct _machdep_state* statep;
{
	int fpreg;

	__asm__ __volatile__ ("fldcr %0, fcr62" : "=r" (fpreg));
	statep->fpsr = fpreg;
	__asm__ __volatile__ ("fldcr %0, fcr63" : "=r" (fpreg));
	statep->fpcr = fpreg;
}

void
_thread_machdep_restore_float_state(statep)
	struct _machdep_state* statep;
{
	int fpreg;

	fpreg = statep->fpsr;
	__asm__ __volatile__ ("fstcr %0, fcr62" : : "r" (fpreg));
	fpreg = statep->fpcr;
	__asm__ __volatile__ ("fstcr %0, fcr63" : : "r" (fpreg));
}
