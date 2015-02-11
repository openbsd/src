/* $OpenBSD: lock.h,v 1.8 2015/02/11 00:14:11 dlg Exp $	*/
/* $NetBSD: lock.h,v 1.16 2001/12/17 23:34:57 thorpej Exp $ */

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-dependent spin lock operations.
 */

#ifndef _MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#if defined(MULTIPROCESSOR)
/*
 * On the Alpha, interprocessor interrupts come in at device priority
 * level.  This can cause some problems while waiting for r/w spinlocks
 * from a high'ish priority level: IPIs that come in will not be processed.
 * This can lead to deadlock.
 *
 * This hook allows IPIs to be processed while a spinlock's interlock
 * is released.
 */
#define	SPINLOCK_SPIN_HOOK						\
do {									\
	struct cpu_info *__ci = curcpu();				\
	int __s;							\
									\
	if (__ci->ci_ipis != 0) {					\
		__s = splipi();						\
		alpha_ipi_process_with_frame(__ci);			\
		splx(__s);						\
	}								\
} while (0)
#endif /* MULTIPROCESSOR */

static inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
	unsigned long t0, v0;

	__asm volatile(
		"1:	ldq_l	%1, 0(%2)	\n"	/* v0 = *addr */
		"	cmpeq	%1, %3, %0	\n"	/* t0 = v0 == old */
		"	beq	%0, 2f		\n"
		"	mov	%4, %0		\n"	/* t0 = new */
		"	stq_c	%0, 0(%2)	\n"	/* *addr = new */
		"	beq	%0, 3f		\n"
		"	mb			\n"
		"2:	br	4f		\n"
		"3:	br	1b		\n"	/* update failed */
		"4:				\n"
		: "=&r" (t0), "=&r" (v0)
		: "r" (addr), "r" (old), "r" (new)
		: "memory");

	return (v0 != old);
}

#endif /* _MACHINE_LOCK_H_ */
