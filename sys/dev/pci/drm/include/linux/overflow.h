/* Public domain. */

#ifndef _LINUX_OVERFLOW_H
#define _LINUX_OVERFLOW_H

#include <linux/limits.h>

#define array_size(x, y)	((x) * (y))

#define struct_size(p, member, n) \
	(sizeof(*(p)) + ((n) * (sizeof(*(p)->member))))

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 5)
#define check_add_overflow(x, y, sum)	__builtin_add_overflow(x, y, sum)
#define check_sub_overflow(x, y, z)	__builtin_sub_overflow(x, y, z)
#define check_mul_overflow(x, y, z)	__builtin_mul_overflow(x, y, z)
#else
#define check_mul_overflow(x, y, z) ({		\
	*(z) = (x) * (y);			\
	0;					\
})
#endif

static inline size_t
size_mul(size_t x, size_t y)
{
	size_t r;
	if (check_mul_overflow(x, y, &r))
		return SIZE_MAX;
	return r;
}

/* from i915_utils.h */
/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#define range_overflows(start, size, max) ({ \
	typeof(start) start__ = (start); \
	typeof(size) size__ = (size); \
	typeof(max) max__ = (max); \
	(void)(&start__ == &size__); \
	(void)(&start__ == &max__); \
	start__ >= max__ || size__ > max__ - start__; \
})

#define range_overflows_t(type, start, size, max) \
	range_overflows((type)(start), (type)(size), (type)(max))

#define range_end_overflows(start, size, max) ({ \
	typeof(start) start__ = (start); \
	typeof(size) size__ = (size); \
	typeof(max) max__ = (max); \
	(void)(&start__ == &size__); \
	(void)(&start__ == &max__); \
	start__ > max__ || size__ > max__ - start__; \
})

#define range_end_overflows_t(type, start, size, max) \
	range_end_overflows((type)(start), (type)(size), (type)(max))

/* Note we don't consider signbits :| */
#define overflows_type(x, T) \
        (sizeof(x) > sizeof(T) && (x) >> BITS_PER_TYPE(T))

#endif
