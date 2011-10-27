/*	$OpenBSD: tcb.h,v 1.1 2011/10/27 04:01:17 guenther Exp $	*/

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

#error "not yet"

#else /* _KERNEL */

/* Not ELF, so for now use a big TCB to save a memory reference for errno */
#define TLS_VARIANT	2

#endif /* _KERNEL */

#endif /* _MACHINE_TCB_H_ */
