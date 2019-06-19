/* $OpenBSD: copystr.S,v 1.4 2018/08/12 17:15:10 mortimer Exp $ */
/*
 * Copyright (c) 2015 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2014 Patrick Wildt <patrick@blueri.se>
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

#include "assym.h"
#include <machine/asm.h>
#include <sys/errno.h>
#include <sys/errno.h>

	.text
	.align	2

/*
 * x0 - from
 * x1 - to
 * x2 - maxlens
 * x3 - lencopied
 * x4 - scratch
 * x5 - hold old onfault
 *
 * Copy string from x0 to x1
 */
ENTRY(copystr)
	RETGUARD_SETUP(copystr, x15)
	mrs	x6, tpidr_el1			// load curcpu
	ldr	x6, [x6, #(CI_CURPCB)]
	ldr	x5, [x6, #(PCB_ONFAULT)]
	adr	x7, .Lcopystrfault
	str	x7, [x6, #(PCB_ONFAULT)]	// set handler

	mov	x8, xzr

1:	ldrb	w4, [x0], 1
	strb	w4, [x1], #1
	sub	x2, x2, #1
	add	x8, x8, #1
	cbz	w4, .Lcopystrsuccess
	cbnz	x2, 1b

	mov	x0, #ENAMETOOLONG
	b	.Lcopystrcleanup

.Lcopystrfault:
	mov	x0, #EFAULT
	b	.Lcopystrcleanup

.Lcopystrsuccess:
	mov	x0, xzr

.Lcopystrcleanup:
	cbz	x3, 2f
	str	x8, [x3]
2:
	str	x5, [x6, #(PCB_ONFAULT)]
	RETGUARD_CHECK(copystr, x15)
	ret

/*
 * x0 - user space address
 * x1 - kernel space address
 * x2 - maxlens
 * x3 - lencopied
 *
 * Copy string from user space to kernel space
 */
ENTRY(copyinstr)
	RETGUARD_SETUP(copystr, x15)
	mrs	x6, tpidr_el1			// load curcpu
	ldr	x6, [x6, #(CI_CURPCB)]
	ldr	x5, [x6, #(PCB_ONFAULT)]
	adr	x7, .Lcopystrfault
	str	x7, [x6, #(PCB_ONFAULT)]	// set handler

	mov	x8, xzr

1:	ldtrb	w4, [x0]
	strb	w4, [x1], #1
	add	x0, x0, #1
	sub	x2, x2, #1
	add	x8, x8, #1
	cbz	w4, .Lcopystrsuccess
	cbnz	x2, 1b

	mov	x0, #ENAMETOOLONG
	b	.Lcopystrcleanup

/*
 * x0 - kernel space address
 * x1 - user space address
 * x2 - maxlens
 * x3 - lencopied
 *
 * Copy string from kernel space to user space
 */
ENTRY(copyoutstr)
	RETGUARD_SETUP(copystr, x15)
	mrs	x6, tpidr_el1			// load curcpu
	ldr	x6, [x6, #(CI_CURPCB)]
	ldr	x5, [x6, #(PCB_ONFAULT)]
	adr	x7, .Lcopystrfault
	str	x7, [x6, #(PCB_ONFAULT)]	// set handler

	mov	x8, xzr

1:	ldrb	w4, [x0], 1
	sttrb	w4, [x1]
	add	x1, x1, #1
	sub	x2, x2, #1
	add	x8, x8, #1
	cbz	w4, .Lcopystrsuccess
	cbnz	x2, 1b

	mov	x0, #ENAMETOOLONG
	b	.Lcopystrcleanup
