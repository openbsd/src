/*	$OpenBSD: crypto_assembly.h,v 1.5 2026/05/07 15:50:47 jsing Exp $ */
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

#if defined(__APPLE__) && defined(__aarch64__)
#define CRYPTO_ASSEMBLY_SEPARATOR		%%
#else
#define CRYPTO_ASSEMBLY_SEPARATOR		;
#endif

#if defined(__APPLE__)
#define CRYPTO_ASSEMBLY_SECTION_TEXT		__TEXT,__text
#define CRYPTO_ASSEMBLY_SECTION_RODATA		__DATA,__const

#define CRYPTO_ASSEMBLY_SYMBOL_NAME(name)	_name
#define CRYPTO_ASSEMBLY_TYPE_FUNCTION(name)
#define CRYPTO_ASSEMBLY_TYPE_OBJECT(name)
#define CRYPTO_ASSEMBLY_OBJECT_SIZE(name)

#define CRYPTO_ASSEMBLY_AARCH64_SYM_HI(name)	name@PAGE
#define CRYPTO_ASSEMBLY_AARCH64_SYM_LO(name)	name@PAGEOFF

#else
#define CRYPTO_ASSEMBLY_SECTION_TEXT		.text
#define CRYPTO_ASSEMBLY_SECTION_RODATA		.rodata

#define CRYPTO_ASSEMBLY_SYMBOL_NAME(name)	name
#define CRYPTO_ASSEMBLY_TYPE_FUNCTION(name)	.type	name,@function
#define CRYPTO_ASSEMBLY_TYPE_OBJECT(name)	.type   name,@object
#define CRYPTO_ASSEMBLY_OBJECT_SIZE(name)	.size   name,.-name

#define CRYPTO_ASSEMBLY_AARCH64_SYM_HI(name)	name
#define CRYPTO_ASSEMBLY_AARCH64_SYM_LO(name)	:lo12:name
#endif

#define CRYPTO_ASSEMBLY_GLOBAL_FUNCTION(name) \
	.global CRYPTO_ASSEMBLY_SYMBOL_NAME(name) CRYPTO_ASSEMBLY_SEPARATOR	\
	CRYPTO_ASSEMBLY_TYPE_FUNCTION(name) CRYPTO_ASSEMBLY_SEPARATOR		\
	CRYPTO_ASSEMBLY_SYMBOL_NAME(name)

#define CRYPTO_ASSEMBLY_OBJECT_START(name) \
	CRYPTO_ASSEMBLY_TYPE_OBJECT(name) CRYPTO_ASSEMBLY_SEPARATOR \
	name

#define CRYPTO_ASSEMBLY_OBJECT_END(name) \
	CRYPTO_ASSEMBLY_OBJECT_SIZE(name)

#endif
