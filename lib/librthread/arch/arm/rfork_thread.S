/* $OpenBSD: rfork_thread.S,v 1.1 2005/12/14 19:42:25 drahn Exp $ */
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
 *              r0     r1          r2         r3
 * rfork_thread(flags, stack_addr, start_fnc, start_arg);
 */

ENTRY(rfork_thread)
	stmdb   sp!, {r1-r4,sl,fp,lr}


	SYSTRAP(rfork)
	bcs	1f

	/* check if we are parent or child */
	cmp	r0, #0
	ldmia   sp!, {r1-r4,sl,fp,lr}
	movne	pc, lr

	/* child */
	mov	sp, r1
	mov	r0, r3
	mov	lr, pc
	mov	pc, r2
	SYSTRAP(exit)
1:
	ldmia   sp!, {r1-r4,sl,fp,lr}
	b     PIC_SYM(CERROR, PLT)
