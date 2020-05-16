/*	$OpenBSD: locore.S,v 1.1 2020/05/16 17:11:14 kettenis Exp $	*/

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

#define OPAL_TEST		0
#define OPAL_CONSOLE_WRITE	1
#define OPAL_CEC_POWER_DOWN	5
#define OPAL_CEC_REBOOT		6

	.abiversion 2

	.rodata

	.globl sigcode
	.globl sigcoderet
sigcode:
sigcoderet:
	blr
	.globl esigcode
esigcode:

	.globl sigfill
sigfill:
	.long	0
esigfill:
	.globl sigfillsiz
sigfillsiz:
	.long	esigfill - sigfill

	.text

	.globl cpu_idle_enter
cpu_idle_enter:
	blr

	.globl cpu_idle_cycle
cpu_idle_cycle:
	blr

	.globl cpu_idle_leave
cpu_idle_leave:
	blr

	.globl cpu_switchto
cpu_switchto:
	blr

	.data

	.globl opal_base
opal_base:
	.quad	0
	.globl opal_entry
opal_entry:
	.quad	0

	.text

	.globl opal_test
opal_test:
	mflr	%r0
	std	%r0, 16(%r1)
	stdu	%r1, -32(%r1)
	std	%r2, 24(%r1)

	addis	%r11, %r2, opal_base@toc@ha
	addi	%r11, %r11, opal_base@toc@l
	addis	%r12, %r2, opal_entry@toc@ha
	addi	%r12, %r12, opal_entry@toc@l
	ld	%r11, 0(%r11)
	ld	%r12, 0(%r12)

	li	%r0, OPAL_TEST
	mr	%r2, %r11
	mtctr	%r12
	bctrl

	ld	%r2, 24(%r1)
	addi	%r1, %r1, 32
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr

	.globl opal_console_write
opal_console_write:
	mflr	%r0
	std	%r0, 16(%r1)
	stdu	%r1, -32(%r1)
	std	%r2, 24(%r1)

	addis	%r11, %r2, opal_base@toc@ha
	addi	%r11, %r11, opal_base@toc@l
	addis	%r12, %r2, opal_entry@toc@ha
	addi	%r12, %r12, opal_entry@toc@l
	ld	%r11, 0(%r11)
	ld	%r12, 0(%r12)

	li	%r0, OPAL_CONSOLE_WRITE
	mr	%r2, %r11
	mtctr	%r12
	bctrl

	ld	%r2, 24(%r1)
	addi	%r1, %r1, 32
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr

	.globl opal_cec_power_down
opal_cec_power_down:
	mflr	%r0
	std	%r0, 16(%r1)
	stdu	%r1, -32(%r1)
	std	%r2, 24(%r1)

	addis	%r11, %r2, opal_base@toc@ha
	addi	%r11, %r11, opal_base@toc@l
	addis	%r12, %r2, opal_entry@toc@ha
	addi	%r12, %r12, opal_entry@toc@l
	ld	%r11, 0(%r11)
	ld	%r12, 0(%r12)

	li	%r0, OPAL_CEC_POWER_DOWN
	mr	%r2, %r11
	mtctr	%r12
	bctrl

	ld	%r2, 24(%r1)
	addi	%r1, %r1, 32
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr

	.globl opal_cec_reboot
opal_cec_reboot:
	mflr	%r0
	std	%r0, 16(%r1)
	stdu	%r1, -32(%r1)
	std	%r2, 24(%r1)

	addis	%r11, %r2, opal_base@toc@ha
	addi	%r11, %r11, opal_base@toc@l
	addis	%r12, %r2, opal_entry@toc@ha
	addi	%r12, %r12, opal_entry@toc@l
	ld	%r11, 0(%r11)
	ld	%r12, 0(%r12)

	li	%r0, OPAL_CEC_REBOOT
	mr	%r2, %r11
	mtctr	%r12
	bctrl

	ld	%r2, 24(%r1)
	addi	%r1, %r1, 32
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr
