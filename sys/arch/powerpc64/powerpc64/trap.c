/*	$OpenBSD: trap.c,v 1.5 2020/06/12 22:01:01 gkoehler Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif
#include <machine/trap.h>

void	decr_intr(struct trapframe *); /* clock.c */

void
trap(struct trapframe *frame)
{
	switch (frame->exc) {
	case EXC_DECR:
		decr_intr(frame);
		return;
#ifdef DDB
	case EXC_PGM:
		/* At a trap instruction, enter the debugger. */
		if (frame->srr1 & EXC_PGM_TRAP) {
			/* Return from db_enter(). */
			if (frame->srr0 == (register_t)db_enter)
				frame->srr0 = frame->lr;
			db_ktrap(T_BREAKPOINT, frame);
			return;
		}
		break;
	case EXC_TRC:
		db_ktrap(T_BREAKPOINT, frame); /* single-stepping */
		return;
#endif
	}

	if (frame->exc == EXC_DSI)
		printf("dsisr %lx dar %lx\n", frame->dsisr, frame->dar);

	panic("trap type %lx at lr %lx", frame->exc, frame->lr);
}
