/*	$OpenBSD: ktime.h,v 1.1 2019/04/14 10:14:53 jsg Exp $	*/
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

#ifndef _LINUX_KTIME_H
#define _LINUX_KTIME_H

#include <sys/time.h>
#include <linux/time.h>
#include <linux/jiffies.h>

typedef struct timeval ktime_t;

static inline struct timeval
ktime_get(void)
{
	struct timeval tv;
	
	getmicrouptime(&tv);
	return tv;
}

static inline struct timeval
ktime_get_raw(void)
{
	struct timeval tv;
	
	microuptime(&tv);
	return tv;
}

static inline struct timeval
ktime_get_monotonic_offset(void)
{
	struct timeval tv = {0, 0};
	return tv;
}

static inline int64_t
ktime_to_ms(struct timeval tv)
{
	return timeval_to_ms(&tv);
}

static inline int64_t
ktime_to_us(struct timeval tv)
{
	return timeval_to_us(&tv);
}

static inline int64_t
ktime_to_ns(struct timeval tv)
{
	return timeval_to_ns(&tv);
}

static inline int64_t
ktime_get_raw_ns(void)
{
	return ktime_to_ns(ktime_get());
}

#define ktime_to_timeval(tv) (tv)

static inline struct timespec64
ktime_to_timespec64(struct timeval tv)
{
	struct timespec64 ts;
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * NSEC_PER_USEC;
	return ts;
}

static inline struct timeval
ktime_sub(struct timeval a, struct timeval b)
{
	struct timeval res;
	timersub(&a, &b, &res);
	return res;
}

static inline struct timeval
ktime_add(struct timeval a, struct timeval b)
{
	struct timeval res;
	timeradd(&a, &b, &res);
	return res;
}

static inline struct timeval
ktime_add_ns(struct timeval tv, int64_t ns)
{
	return ns_to_timeval(timeval_to_ns(&tv) + ns);
}

static inline struct timeval
ktime_sub_ns(struct timeval tv, int64_t ns)
{
	return ns_to_timeval(timeval_to_ns(&tv) - ns);
}

static inline int64_t
ktime_us_delta(struct timeval a, struct timeval b)
{
	return ktime_to_us(ktime_sub(a, b));
}

static inline int64_t
ktime_ms_delta(struct timeval a, struct timeval b)
{
	return ktime_to_ms(ktime_sub(a, b));
}

static inline bool
ktime_after(const struct timeval a, const struct timeval b)
{
	return timercmp(&a, &b, >);
}

static inline struct timeval
ns_to_ktime(uint64_t ns)
{
	return ns_to_timeval(ns);
}

#include <linux/timekeeping.h>

#endif
