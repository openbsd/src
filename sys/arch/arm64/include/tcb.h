/*	$OpenBSD: tcb.h,v 1.1 2016/12/17 23:38:33 patrick Exp $	*/

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

#include <machine/pcb.h>

static inline void
__aarch64_set_tcb(uint64_t tcb) {
	__asm volatile("msr tpidr_el0, %x0" :: "r" (tcb));
	return ;
}
#define TCB_GET(p)              \
	((void *)((struct pcb *)(p)->p_addr)->pcb_tcb)
#define TCB_SET(p, addr)        \
	do {								\
	(((struct pcb *)(p)->p_addr)->pcb_tcb = (uint64_t)(addr));	\
	__aarch64_set_tcb((uint64_t)addr);						\
	} while (0)

#else /* _KERNEL */

/* ELF TLS ABI calls for small TCB, with static TLS data after it */
#define TLS_VARIANT	1

static inline uint64_t
__aarch64_read_tcb(void) {
        uint64_t tcb;
	__asm volatile("mrs %x0, tpidr_el0": "=r" (tcb));

	return tcb;
}

#define TCB_GET(p)              \
	((void *)__aarch64_read_tcb())

#define TCB_GET_MEMBER(member)	\
	(((struct thread_control_block *)__aarch64_read_tcb())->member)

#endif /* _KERNEL */

#endif /* _MACHINE_TCB_H_ */
