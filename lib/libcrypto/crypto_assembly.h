/*	$OpenBSD: crypto_assembly.h,v 1.1 2026/03/28 13:09:55 jsing Exp $ */
/*
 * Copyright (c) 2026 Joel Sing <jsing@openbsd.org>
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

#ifndef HEADER_CRYPTO_ASSEMBLY_H
#define HEADER_CRYPTO_ASSEMBLY_H

/* Ensure _CET_ENDBR is always defined on amd64. */
#ifdef __amd64__
#ifdef __CET__
#include <cet.h>
#else
#define _CET_ENDBR
#endif
#endif

#endif
