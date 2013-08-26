/*	$OpenBSD: tcb.h,v 1.2 2013/08/26 21:38:08 miod Exp $	*/

/*
 * Copyright (c) 2011 Philip Guenther <guenther@openbsd.org>
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

#ifndef _MACHINE_TCB_H_
#define _MACHINE_TCB_H_

/*
 * In userspace, register %r27 contains the address of the thread's TCB,
 * and register %r26 contains the address of the thread's errno.
 * It is the responsibility of the kernel to set %r27 to the proper value
 * when creating the thread, while initialization of %r26 is done in
 * userland within libpthread on a needed basis.
 */

#ifdef _KERNEL

#include <machine/reg.h>

#define TCB_GET(p)		\
	((void *)(p)->p_md.md_tf->tf_r[27])
#define TCB_SET(p, addr)	\
	((p)->p_md.md_tf->tf_r[27] = (register_t)(addr))

#else /* _KERNEL */

/*
 * It is unknown whether the m88k ELF ABI mentions TLS. On m88k, since only
 * unsigned offsets in (register + immediate offset) addressing is supported
 * on all processors, it makes sense to use a small TCB, with static TLS data
 * after it.
 */
#define TLS_VARIANT	1

#if defined(__GNUC__) && __GNUC__ > 4

struct thread_control_block;
__register__ struct thread_control_block *__tcb __asm__ ("%r27");
#define	TCB_GET()	(__tcb)
#define	TCB_SET(tcb)	((__tcb) = (tcb))
#define	TCB_GET_MEMBER(member)	((void *)(__tcb->member))

#else /* __GNUC__ > 4 */

#include <stddef.h>		/* for offsetof */

/* Get a pointer to the TCB itself */
static inline void *
__m88k_get_tcb(void)
{
	void *val;
	__asm__ ("or %0,%%r27,%%r0" : "=r" (val));
	return val;
}

/* Get the value of a specific member in the TCB */
static inline void *
__m88k_read_tcb(size_t offset)
{
	void	*val;
	/* XXX the `offset' constraint ought to be "I" but this causes a warning */
	__asm__ ("ld %0,%%r27,%1" : "=r" (val) : "r" (offset));
	return val;
}

#define TCB_GET()	__m88k_get_tcb()
#define TCB_SET(tcb)	__asm __volatile("or %%r27,%0,%r0" : : "r" (tcb))

#define TCB_GET_MEMBER(member)	\
	__m88k_read_tcb(offsetof(struct thread_control_block, member))

#endif /* __GNUC__ > 4 */

#endif /* _KERNEL */

#endif /* _MACHINE_TCB_H_ */
