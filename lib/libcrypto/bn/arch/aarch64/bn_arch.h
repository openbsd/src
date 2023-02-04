/*	$OpenBSD: bn_arch.h,v 1.3 2023/02/04 11:48:55 jsing Exp $ */
/*
 * Copyright (c) 2023 Joel Sing <jsing@openbsd.org>
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

#include <openssl/bn.h>

#ifndef HEADER_BN_ARCH_H
#define HEADER_BN_ARCH_H

#ifndef OPENSSL_NO_ASM

#if defined(__GNUC__)
#define HAVE_BN_UMUL_HILO

static inline void
bn_umul_hilo(BN_ULONG a, BN_ULONG b, BN_ULONG *out_h, BN_ULONG *out_l)
{
	BN_ULONG h, l;

	/* Unsigned multiplication using a umulh/mul pair. */
	__asm__ ("umulh %0, %2, %3; mul %1, %2, %3"
	    : "=&r"(h), "=r"(l)
	    : "r"(a), "r"(b));

	*out_h = h;
	*out_l = l;
}
#endif /* __GNUC__ */

#endif
#endif
