/*	$OpenBSD: cpufunc.h,v 1.1 2020/05/22 15:07:47 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#ifndef _MACHINE_CPUFUNC_H_
#define _MACHINE_CPUFUNC_H_

static inline uint64_t
mfmsr(void)
{
	uint64_t value;
	__asm volatile ("mfmsr %0" : "=r"(value));
	return value;
}

static inline void
mtmsr(uint64_t value)
{
	__asm volatile ("mtmsr %0" :: "r"(value));
}

static inline uint64_t
mflpcr(void)
{
	uint64_t value;
	__asm volatile ("mfspr %0,318" : "=r"(value));
	return value;
}

static inline void
mtlpcr(uint64_t value)
{
	__asm volatile ("mtspr 318,%0" :: "r"(value));
}


extern int cacheline_size;

void	__syncicache(void *, size_t);

#endif /* _MACHINE_CPUFUNC_H_ */
