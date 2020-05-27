/*	$OpenBSD: trap.c,v 1.3 2020/05/27 22:22:04 gkoehler Exp $	*/

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

void
trap(struct trapframe *frame)
{
#ifdef DDB
	/* At a trap instruction, enter the debugger. */
	if (frame->exc == EXC_PGM && (frame->srr1 & EXC_PGM_TRAP)) {
		db_ktrap(T_BREAKPOINT, frame);
		frame->srr0 += 4; /* Step to next instruction. */
		return;
	}
#endif

	panic("trap type %lx at lr %lx", frame->exc, frame->lr);
}
