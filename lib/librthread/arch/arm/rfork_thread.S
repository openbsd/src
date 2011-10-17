/* $OpenBSD: rfork_thread.S,v 1.3 2011/10/17 06:39:20 guenther Exp $ */
/*
 * Copyright (c) 2005 Dale Rahn <drahn@openbsd.org>
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
#include <sys/syscall.h>
#include <machine/asm.h>
#include "../../../libc/arch/arm/SYS.h"

/*
 *                r0     r1          r2         r3
 * __tfork_thread(param, stack_addr, start_fnc, start_arg);
 */

ENTRY(__tfork_thread)
	stmdb   sp!, {r4}
	mov	r4, r1


	SYSTRAP(__tfork)
	bcs	1f

	/* check if we are parent or child */
	cmp	r0, #0
	ldmneia   sp!, {r4}
	movne	pc, lr

	/* child */
	mov	sp, r4
	mov	r0, r3
	mov	lr, pc
	mov	pc, r2
	nop
	SYSTRAP(threxit)
1:
	ldmia   sp!, {r4}
	b     PIC_SYM(CERROR, PLT)
