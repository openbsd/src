/*	$OpenBSD: asm.h,v 1.3 2020/06/25 09:03:01 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Dale Rahn <drahn@openbsd.org>
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

#ifndef _POWERPC64_ASM_H_
#define _POWERPC64_ASM_H_

#define _C_LABEL(x)	x
#define _ASM_LABEL(x)	x

#define _TMP_LABEL(x)	.L_ ## x
#define _GEP_LABEL(x)	.L_ ## x ## _gep0
#define _LEP_LABEL(x)	.L_ ## x ## _lep0

#define _ENTRY(x)						\
	.text; .align 2; .globl x; .type x,@function; x:	\
	_GEP_LABEL(x):						\
	addis	%r2, %r12, .TOC.-_GEP_LABEL(x)@ha;		\
	addi	%r2, %r2, .TOC.-_GEP_LABEL(x)@l;		\
	_LEP_LABEL(x):						\
	.localentry     _C_LABEL(x), _LEP_LABEL(x)-_GEP_LABEL(x);

#if defined(PROF) || defined(GPROF)
# define _PROF_PROLOGUE(y)					\
	.section ".data";					\
	.align 2;						\
_TMP_LABEL(y):;							\
	.long	0;						\
	.section ".text";					\
	mflr	%r0;						\
	addis	%r11, %r2, _TMP_LABEL(y)@toc@ha;		\
	std	%r0, 8(%r1);					\
	addi	%r0, %r11, _TMP_LABEL(y)@toc@l;			\
	bl _mcount; 
#else
# define _PROF_PROLOGUE(y)
#endif

#define ENTRY(y)	_ENTRY(_C_LABEL(y)); _PROF_PROLOGUE(y)
#define ASENTRY(y)	_ENTRY(_ASM_LABEL(y)); _PROF_PROLOGUE(y)
#define END(y)		.size y, . - y

#define STRONG_ALIAS(alias,sym) \
	.global alias; .set alias,sym
#define WEAK_ALIAS(alias,sym) \
	.weak alias; .set alias,sym

#endif /* !_POWERPC64_ASM_H_ */
