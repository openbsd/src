/*	$OpenBSD: uthread_machdep.c,v 1.1 2007/02/19 21:03:50 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <pthread.h>
#include "pthread_private.h"

#define STACK_ALIGNMENT		8

struct regframe {
	register_t	pr;
	register_t	r14;
	register_t	r13;
	register_t	r12;
	register_t	r11;
	register_t	r10;
	register_t	r9;
	register_t	r8;
};

void
_thread_machdep_init(struct _machdep_state* statep, void *base, int len,
    void (*entry)(void))
{
	struct regframe *regs;

	regs = (struct regframe *)
	    (((u_int32_t)base + len - sizeof *regs) & ~(STACK_ALIGNMENT - 1));
	regs->pr = (register_t)entry;

	statep->sp = (u_int)regs;
}

void
_thread_machdep_save_float_state(struct _machdep_state* statep)
{
}

void
_thread_machdep_restore_float_state(struct _machdep_state* statep)
{
}
