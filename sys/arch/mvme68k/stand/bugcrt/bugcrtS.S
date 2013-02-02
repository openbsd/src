/*	$OpenBSD: bugcrtS.S,v 1.2 2013/02/02 13:36:06 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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
#include <machine/prom.h>

	.text

GLOBAL(__bootxx_start)
ASLOCAL(__stack_end)
	.long	_ASM_LABEL(__stack_end)		| sp
	.long	_C_LABEL(__start)		| pc

ENTRY(__start)
	/*
	 * On entry, d0-d1, d4, a0-a6 contain the BUG incoming parameters.
	 */
	/*
	 * Set up stack. This is already done if booted by the BUG, but not
	 * if invoked from an old bootxx' bugexec().
	 */
	leal	_ASM_LABEL(__stack_end):l, %sp

	/*
	 * Push arguments on stack.
	 */
	movl	MVMEPROM_REG_NBARGEND, %sp@-
	movl	MVMEPROM_REG_NBARGSTART, %sp@-
	movl	MVMEPROM_REG_ARGEND, %sp@-
	movl	MVMEPROM_REG_ARGSTART, %sp@-
	movl	MVMEPROM_REG_CONFBLK, %sp@-
	movl	MVMEPROM_REG_ENTRY, %sp@-
	movl	MVMEPROM_REG_CTRLADDR, %sp@-
	movl	MVMEPROM_REG_FLAGS, %sp@-
	movl	MVMEPROM_REG_CTRLLUN, %sp@-
	movl	MVMEPROM_REG_DEVLUN, %sp@-

	/*
	 * Invoke the rest of the startup as C code.
	 */
	bsr	_C_LABEL(bugcrt_start)

	bsr	_C_LABEL(_rtt)
	/* NOTREACHED */

ENTRY(bugexec_final)
	/*
	 * Pop arguments to the appropriate registers.
	 */
	movl	%sp@(4 * 1), MVMEPROM_REG_DEVLUN
	movl	%sp@(4 * 2), MVMEPROM_REG_CTRLLUN
	movl	%sp@(4 * 3), MVMEPROM_REG_FLAGS
	movl	%sp@(4 * 4), MVMEPROM_REG_CTRLADDR
	movl	%sp@(4 * 5), MVMEPROM_REG_ENTRY
	movl	%sp@(4 * 6), MVMEPROM_REG_CONFBLK
	movl	%sp@(4 * 7), MVMEPROM_REG_ARGSTART
	movl	%sp@(4 * 8), MVMEPROM_REG_ARGEND
	movl	%sp@(4 * 9), MVMEPROM_REG_NBARGSTART
	movl	%sp@(4 * 10), MVMEPROM_REG_NBARGEND

	/*
	 * Pop return address
	 */
	movl	%sp@(4 * 11), %d2

	/*
	 * Pop stack address
	 */
	movl	%sp@(4 * 12), %sp

	/*
	 * Push return address
	 */
	movl	_ASM_LABEL(bugexec_returned), %sp@-
	movl	%d2, %sp@-
	rts

ASLOCAL(bugexec_returned)
	bsr	_C_LABEL(_rtt)
	/* NOTREACHED */
