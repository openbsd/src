/*	$OpenBSD: uthread_machdep.c,v 1.2 2007/03/02 06:11:54 miod Exp $	*/

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

#define STACK_ALIGNMENT		4

struct regframe {
	/* return address */
	register_t	pr;
	/* call-saved general registers */
	register_t	r14;
	register_t	r13;
	register_t	r12;
	register_t	r11;
	register_t	r10;
	register_t	r9;
	register_t	r8;
	register_t	macl;
	register_t	mach;
#if defined(__SH4__) && !defined(__SH4_NOFPU__)
	/* call-saved floating point registers */
	register_t	fr12;
	register_t	fr13;
	register_t	fr14;
	register_t	fr15;
	register_t	xd12;
	register_t	xd13;
	register_t	xd14;
	register_t	xd15;
	/* floating point control registers */
	register_t	fpul;
	register_t	fpscr;
#endif
};

void
_thread_machdep_init(struct _machdep_state* statep, void *base, int len,
    void (*entry)(void))
{
	struct regframe *regs;

	regs = (struct regframe *)
	    (((u_int32_t)base + len - sizeof *regs) & ~(STACK_ALIGNMENT - 1));
	regs->pr = (register_t)entry;
#if defined(__SH4__) && !defined(__SH4_NOFPU__)
	__asm__ __volatile__ ("sts fpscr, %0" : "=r" (regs->fpscr));
#endif

	statep->sp = (u_int)regs;
}

/*
 * Floating point state is saved with the general registers in
 * _thread_machdep_switch().
 */

void
_thread_machdep_save_float_state(struct _machdep_state* statep)
{
}

void
_thread_machdep_restore_float_state(struct _machdep_state* statep)
{
}
