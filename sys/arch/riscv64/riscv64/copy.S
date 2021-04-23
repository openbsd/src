/*
 * Copyright (c) 2020 Brian Bamsch <bbamsch@google.com>
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
#include <machine/vmparam.h>
#include <machine/riscvreg.h>
#include <sys/errno.h>


	.text
	.align 4

/*
 * a0 = user space address
 * a1 = kernel space address
 * a2 = length
 *
 * Copies bytes from user space to kernel space
 */
ENTRY(copyin)
	RETGUARD_SETUP(copyio, a6)
	beqz	a2, .Lcopyiodone /* If len = 0, skip loop. */
	/* Check whether source+len overflows. */
	add	a3, a0, a2
	bltu	a3, a0, .Lcopyiofault_nopcb
	/* Check that source+len is in userspace. */
	li	a4, VM_MAXUSER_ADDRESS
	bgt	a3, a4, .Lcopyiofault_nopcb

	la	a3, .Lcopyiofault_user
	SWAP_FAULT_HANDLER(a3, a4, a5)
	ENTER_USER_ACCESS(a4)

// XXX optimize?
.Lcopyio:
1:	lb	a4, 0(a0)
	addi	a0, a0, 1
	sb	a4, 0(a1)
	addi	a1, a1, 1
	addi	a2, a2, -1
	bnez	a2, 1b

	EXIT_USER_ACCESS(a4)
	SET_FAULT_HANDLER(a3, a4)
.Lcopyiodone:
	mv	a0, x0
	RETGUARD_CHECK(copyio, a6)
	ret

.Lcopyiofault_user:
	EXIT_USER_ACCESS(a4)
.Lcopyiofault:
	SET_FAULT_HANDLER(a3, a4)
.Lcopyiofault_nopcb:
	li	a0, EFAULT
	RETGUARD_CHECK(copyio, a6)
	ret
END(copyin)

/*
 * a0 = kernel space address
 * a1 = user space address
 * a2 = length
 *
 * Copies bytes from kernel space to user space
 */

ENTRY(copyout)
	RETGUARD_SETUP(copyio, a6)
	beqz	a2, .Lcopyiodone /* If len = 0, skip loop. */
	/* Check whether source+len overflows. */
	add	a3, a1, a2
	bltu	a3, a1, .Lcopyiofault_nopcb
	/* Check that source+len is in userspace. */
	li	a4, VM_MAXUSER_ADDRESS
	bgt	a3, a4, .Lcopyiofault_nopcb

	la	a4, .Lcopyiofault_user
	SWAP_FAULT_HANDLER(a3, a4, a5)
	ENTER_USER_ACCESS(a4)

	j	.Lcopyio
END(copyout)

/*
 * a0 = kernel space source address
 * a1 = kernel space destination address
 * a2 = length
 *
 * Copies bytes from kernel space to kernel space, aborting on page fault
 */

ENTRY(kcopy)
	RETGUARD_SETUP(copyio, a6)
	beqz	a2, .Lcopyiodone /* If len = 0, skip loop. */

	la	a3, .Lcopyiofault
	SWAP_FAULT_HANDLER(a3, a4, a5)

	j	.Lcopyio
END(kcopy)
