/*	$OpenBSD: unixsys.S,v 1.6 2005/05/24 20:38:20 uwe Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
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

#include <machine/asm.h>

#include "compat_linux.h"

	.text

#define RSYSCALL(x) \
	ENTRY(u##x); \
		swi	__NR_##x; \
		b	1f

RSYSCALL(exit)
RSYSCALL(read)
RSYSCALL(write)
RSYSCALL(open)
RSYSCALL(close)
RSYSCALL(lseek32)
RSYSCALL(ioctl)
RSYSCALL(stat)

1:
	cmn	r0, #4096
	movcc	pc, lr
	rsb	r0, r0, #0
	ldr	r1, .Lerrno	/* XXX clobbers arg1 */
	str	r0, [r1]
	mvn	r0, #0
	mov	pc, lr

.Lerrno:
	.word	errno

/* XXX only works for up to four arguments. */
ENTRY(syscall)
	swi	__NR_syscall
	b	1b

ENTRY(uselect)
	str	r4, [sp, #-4]!
	ldr	r4, [sp, #4]
	swi	__NR_select
	ldr	r4, [sp], #4
	cmn	r0, #4096
	movcc	pc, lr
	rsb	r0, r0, #0
	str	r1, [sp, #-4]!
	ldr	r1, .Lerrno
	str	r0, [r1]
	ldr	r1, [sp], #4
	mvn	r0, #0
	mov	pc, lr

.data

	.global errno
errno:
	.word	0
