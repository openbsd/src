/* $OpenBSD: copy.S,v 1.5 2018/08/12 17:15:10 mortimer Exp $ */
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
#include <sys/syscall.h>
#include <machine/asm.h>
#include <sys/errno.h>

	.text
	.align	2

/*
 * x0 = user space address
 * x1 = kernel space address
 * x2 = length
 *
 * Copies bytes from user space to kernel space
 *
 * XXX should this assert that address spaces are correct for each address?
 */
ENTRY(copyin)
	RETGUARD_SETUP(copy, x15)
	cbnz	x2, 1f
	mov	x0, 0
	RETGUARD_CHECK(copy, x15)
	ret
1:
	mrs	x3, tpidr_el1			// load cpuinfo
	ldr	x3, [x3, #(CI_CURPCB)]
	ldr	x4, [x3, #(PCB_ONFAULT)]
	adr	x5, .Lcopyfault
	str	x5, [x3, #(PCB_ONFAULT)]	// set handler

// This probably should be optimized
2:	ldtrb	w6, [x0]
	strb	w6, [x1], #1
	add	x0, x0, #1
	sub	x2, x2, #1
	cbnz	x2, 2b

	str	x4, [x3, #(PCB_ONFAULT)]	// clear handler
	mov	x0, xzr
	RETGUARD_CHECK(copy, x15)
	ret

.Lcopyfault:
	mov	x0, #EFAULT
	ldr	x4, [x3, #(PCB_ONFAULT)]
	RETGUARD_CHECK(copy, x15)
	ret

/*
 * x0 = user space address
 * x1 = kernel space address
 *
 * Atomically copies a 32-bit word from user space to kernel space
 *
 * XXX should this assert that address spaces are correct for each address?
 */
ENTRY(copyin32)
	RETGUARD_SETUP(copy, x15)
	mrs	x3, tpidr_el1			// load cpuinfo
	ldr	x3, [x3, #(CI_CURPCB)]
	ldr	x4, [x3, #(PCB_ONFAULT)]
	adr	x5, .Lcopyfault
	str	x5, [x3, #(PCB_ONFAULT)]	// set handler

	ldtr	w6, [x0]
	str	w6, [x1]

	str	x4, [x3, #(PCB_ONFAULT)]	// clear handler
	mov	x0, xzr
	RETGUARD_CHECK(copy, x15)
	ret

/*
 * x0 = kernel space address
 * x1 = user space address
 * x2 = length
 *
 * Copies bytes from kernel space to user space
 *
 * XXX should this assert that address spaces are correct for each address?
 */

ENTRY(copyout)
	RETGUARD_SETUP(copy, x15)
	cbnz	x2, 1f
	mov	x0, 0
	RETGUARD_CHECK(copy, x15)
	ret
1:
	mrs	x3, tpidr_el1			// load cpuinfo
	ldr	x3, [x3, #(CI_CURPCB)]
	ldr	x4, [x3, #(PCB_ONFAULT)]
	adr	x5, .Lcopyfault
	str	x5, [x3, #(PCB_ONFAULT)]	// set handler

// This probably should be optimized
2:	ldrb	w6, [x0], #1
	sttrb	w6, [x1]
	add	x1, x1, #1
	sub	x2, x2, #1
	cbnz	x2, 2b

	str	x4, [x3, #(PCB_ONFAULT)]	// clear handler
	mov	x0, xzr
	RETGUARD_CHECK(copy, x15)
	ret

/*
 * x0 = kernel space source address
 * x1 = kernel space destination address
 * x2 = length
 *
 * Copies bytes from kernel space to kernel space, aborting on page fault
 */

ENTRY(kcopy)
	RETGUARD_SETUP(copy, x15)
	cbnz	x2, 1f
	mov	x0, 0
	RETGUARD_CHECK(copy, x15)
	ret
1:
	mrs	x3, tpidr_el1			// load cpuinfo
	ldr	x3, [x3, #(CI_CURPCB)]
	ldr	x4, [x3, #(PCB_ONFAULT)]
	adr	x5, .Lcopyfault
	str	x5, [x3, #(PCB_ONFAULT)]	// set handler

// This probably should be optimized
2:	ldrb	w6, [x0], #1
	strb	w6, [x1], #1
	sub	x2, x2, #1
	cbnz	x2, 2b

	str	x4, [x3, #(PCB_ONFAULT)]	// clear handler
	mov	x0, xzr
	RETGUARD_CHECK(copy, x15)
	ret
