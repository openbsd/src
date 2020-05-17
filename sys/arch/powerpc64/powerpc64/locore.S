/*	$OpenBSD: locore.S,v 1.2 2020/05/17 23:27:45 gkoehler Exp $	*/

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

#ifdef DDB
	.globl db_enter
db_enter:
	blr
	
	.globl setjmp
setjmp:
	mflr	%r0
	mfcr	%r5
	std	%r0, 0x00(%r3)	/* lr */
	std	%r5, 0x08(%r3)	/* cr */
	std	%r1, 0x10(%r3)
	std	%r2, 0x18(%r3)
	std	%r14, 0x20(%r3)
	std	%r15, 0x28(%r3)
	std	%r16, 0x30(%r3)
	std	%r17, 0x38(%r3)
	std	%r18, 0x40(%r3)
	std	%r19, 0x48(%r3)
	std	%r20, 0x50(%r3)
	std	%r21, 0x58(%r3)
	std	%r22, 0x60(%r3)
	std	%r23, 0x68(%r3)
	std	%r24, 0x70(%r3)
	std	%r25, 0x78(%r3)
	std	%r26, 0x80(%r3)
	std	%r27, 0x88(%r3)
	std	%r28, 0x90(%r3)
	std	%r29, 0x98(%r3)
	std	%r30, 0xa0(%r3)
	std	%r31, 0xa8(%r3)
	li	%r3, 0		/* return 0 */
	blr

	.globl longjmp
longjmp:
	ld	%r0, 0x00(%r3)	/* lr */
	ld	%r5, 0x08(%r3)	/* cr */
	mtlr	%r0
	mtcr	%r5
	ld	%r1, 0x10(%r3)
	ld	%r2, 0x18(%r3)
	ld	%r14, 0x20(%r3)
	ld	%r15, 0x28(%r3)
	ld	%r16, 0x30(%r3)
	ld	%r17, 0x38(%r3)
	ld	%r18, 0x40(%r3)
	ld	%r19, 0x48(%r3)
	ld	%r20, 0x50(%r3)
	ld	%r21, 0x58(%r3)
	ld	%r22, 0x60(%r3)
	ld	%r23, 0x68(%r3)
	ld	%r24, 0x70(%r3)
	ld	%r25, 0x78(%r3)
	ld	%r26, 0x80(%r3)
	ld	%r27, 0x88(%r3)
	ld	%r28, 0x90(%r3)
	ld	%r29, 0x98(%r3)
	ld	%r30, 0xa0(%r3)
	ld	%r31, 0xa8(%r3)
	mr	%r4, %r3	/* return val */
	blr
#endif
