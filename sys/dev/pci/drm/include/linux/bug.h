/*	$OpenBSD: bug.h,v 1.1 2019/04/14 10:14:53 jsg Exp $	*/
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
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

#ifndef _LINUX_BUG_H
#define _LINUX_BUG_H

#include <sys/types.h>
#include <sys/systm.h>
#include <linux/compiler.h>

#define BUG()								\
do {									\
	panic("BUG at %s:%d", __FILE__, __LINE__);			\
} while (0)

#ifndef DIAGNOSTIC
#define BUG_ON(x)	((void)(x))
#else
#define BUG_ON(x)	KASSERT(!(x))
#endif

#define BUILD_BUG()
#define BUILD_BUG_ON(x) CTASSERT(!(x))
#define BUILD_BUG_ON_NOT_POWER_OF_2(x)	0
#define BUILD_BUG_ON_MSG(x, y)		do { } while (0)
#define BUILD_BUG_ON_INVALID(x)		((void)0)
#define BUILD_BUG_ON_ZERO(x)		0

#define WARN(condition, fmt...) ({ 					\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf(fmt);						\
	unlikely(__ret);						\
})

#define WARN_ONCE(condition, fmt...) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf(fmt);						\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#define _WARN_STR(x) #x

#define WARN_ON(condition) ({						\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
	unlikely(__ret);						\
})

#define WARN_ON_ONCE(condition) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#endif
