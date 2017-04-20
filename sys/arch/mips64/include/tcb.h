/*	$OpenBSD: tcb.h,v 1.3 2017/04/20 15:42:26 visa Exp $	*/

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

static inline void
__mips64_set_tcb(struct proc *p, void *tcb)
{
#ifdef CPU_MIPS64R2
	extern int cpu_has_userlocal;

	if (cpu_has_userlocal)
		cp0_set_userlocal(tcb);
#endif

	p->p_md.md_tcb = tcb;
}

#define TCB_SET(p, addr)	__mips64_set_tcb(p, addr)
#define TCB_GET(p)		((p)->p_md.md_tcb)

#else /* _KERNEL */

/* ELF TLS ABI calls for small TCB, with static TLS data after it */
#define TLS_VARIANT	1

#endif /* _KERNEL */

#endif /* _MACHINE_TCB_H_ */
