/*	$OpenBSD: trampoline.S,v 1.2 2019/12/17 22:25:56 deraadt Exp $	*/

/*
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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
#include <machine/param.h>
#include "assym.h"

	.text

/*
 * The next page contains everything that needs to be "exposed" to
 * userland.  This is the exception vector table that contains the
 * entries for EL0, and the tail end of the EL0 exception handlers.
 * This code is deliberately laid out in a way that avoids leaking
 * kernel virtual addresses.
 */
	.align 12

.macro	vempty
	.align 7
	brk	0xfff
	1: b	1b
.endm

.macro	vector	name
	.align 7
	msr	tpidrro_el0, x18
	mrs	x18, ttbr1_el1
	bic	x18, x18, #(1UL << 48)
	sub	x18, x18, #(2 * PAGE_SIZE)
	msr	ttbr1_el1, x18
	isb
	b	tramp_\name
.endm

	.align 11
	.globl trampoline_vectors
trampoline_vectors:
	vempty			/* Synchronous EL1t */
	vempty			/* IRQ EL1t */
	vempty			/* FIQ EL1t */
	vempty			/* Error EL1t */

	vempty			/* Synchronous EL1h */
	vempty			/* IRQ EL1h */
	vempty			/* FIQ EL1h */
	vempty			/* Error EL1h */

	vector el0_sync		/* Synchronous 64-bit EL0 */
	vector el0_irq		/* IRQ 64-bit EL0 */
	vempty			/* FIQ 64-bit EL0 */
	vector el0_error	/* Error 64-bit EL0 */

	vempty			/* Synchronous 32-bit EL0 */
	vempty			/* IRQ 32-bit EL0 */
	vempty			/* FIQ 32-bit EL0 */
	vempty			/* Error 32-bit EL0 */

	.align 11
	.globl tramp_return
tramp_return:
	mrs	x18, ttbr1_el1
	orr	x18, x18, #(1UL << 48)
	add	x18, x18, #(2 * PAGE_SIZE)
	msr	ttbr1_el1, x18
	isb
	mrs	x18, tpidrro_el0
	msr	tpidrro_el0, xzr
	eret
	dsb nsh
	isb

/*
 * The next page contains the start of the EL0 exception handlers.
 * This page is not "exposed" to userland, but should immediately
 * follow the page with the EL0 exception vector table such that
 * relative branches don't give away anything about the layout of our
 * kernel.
 */
	.align 12

.macro	tramp_enter
	ldr	x18, =exception_vectors
	msr	vbar_el1, x18
	isb
	mrs	x18, tpidrro_el0
	msr	tpidrro_el0, xzr
.endm

tramp_el0_sync:
	tramp_enter
	b	handle_el0_sync

tramp_el0_irq:
	tramp_enter
	b	handle_el0_irq

tramp_el0_error:
	tramp_enter
	b	handle_el0_error
