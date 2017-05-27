/*	$OpenBSD: lock.h,v 1.11 2017/05/27 15:11:03 mpi Exp $	*/
/*
 * Copyright (c) 2012 Mark Kettenis <kettenis@openbsd.org>
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

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#ifdef _KERNEL

/*
 * On processors with multiple threads we force a thread switch.
 *
 * On UltraSPARC T2 and its successors, the optimal way to do this
 * seems to be to do three nop reads of %ccr.  This works on
 * UltraSPARC T1 as well, even though three nop casx operations seem
 * to be slightly more optimal.  Since these instructions are
 * effectively nops, executing them on earlier non-CMT processors is
 * harmless, so we make this the default.
 *
 * On SPARC T4 and later, we can use the processor-specific pause
 * instruction.
 *
 * On SPARC64 VI and its successors we execute the processor-specific
 * sleep instruction.
 */
#define SPINLOCK_SPIN_HOOK						\
do {									\
	__asm volatile(							\
		"999:	rd	%%ccr, %%g0			\n"	\
		"	rd	%%ccr, %%g0			\n" 	\
		"	rd	%%ccr, %%g0			\n" 	\
		"	.section .sun4v_pause_patch, \"ax\"	\n" 	\
		"	.word	999b				\n" 	\
		"	.word	0xb7802080	! pause	128	\n" 	\
		"	.word	999b + 4			\n" 	\
		"	nop					\n" 	\
		"	.word	999b + 8			\n" 	\
		"	nop					\n" 	\
		"	.previous				\n" 	\
		"	.section .sun4u_mtp_patch, \"ax\"	\n" 	\
		"	.word	999b				\n" 	\
		"	.word	0x81b01060	! sleep		\n" 	\
		"	.word	999b + 4			\n" 	\
		"	nop					\n" 	\
		"	.word	999b + 8			\n" 	\
		"	nop					\n" 	\
		"	.previous				\n" 	\
		: : : "memory");					\
} while (0)


#endif	/* _KERNEL */
#endif	/* _MACHINE_LOCK_H_ */
