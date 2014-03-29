/*	$OpenBSD: tcb.h,v 1.3 2014/03/29 18:09:30 guenther Exp $	*/

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

#ifdef _KERNEL

#define TCB_GET(p)		\
	((void *)trapframe(p)->fixreg[2])
#define TCB_SET(p, addr)	\
	(trapframe(p)->fixreg[2] = (__register_t)(addr))

#else /* _KERNEL */

/* ELF TLS ABI calls for small TCB, with static TLS data after it */
#define TLS_VARIANT	1

#if 0	/* XXX perhaps use the gcc global register extension? */
struct thread_control_block;
__register__ struct thread_control_block *__tcb __asm__ ("%r2");
#define TCB_GET()		(__tcb)
#define TCB_GET_MEMBER(member)	((void *)(__tcb->member))
#define TCB_SET(tcb)		((__tcb) = (tcb))

#else

#include <stddef.h>		/* for offsetof */

/* Get a pointer to the TCB itself */
static inline void *
__powerpc_get_tcb(void)
{
	void *val;
	__asm__ ("mr %0, %%r2" : "=r" (val));
	return val;
}
#define TCB_GET()	__powerpc_get_tcb()

/* Get the value of a specific member in the TCB */
static inline void *
__powerpc_read_tcb(int offset)
{
	void *val;
	__asm__ ("lwzx %0, %%r2, %1" : "=r" (val) : "r" (offset));
	return val;
}
#define TCB_GET_MEMBER(member) \
	__powerpc_read_tcb(offsetof(struct thread_control_block, member))

#define TCB_SET(tcb)	__asm volatile("mr %%r2, %0" : : "r" (tcb))

#endif /* 0 */

#endif /* _KERNEL */

#endif /* _MACHINE_TCB_H_ */
