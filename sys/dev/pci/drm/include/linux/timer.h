/*	$OpenBSD: timer.h,v 1.1 2019/04/14 10:14:53 jsg Exp $	*/
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

#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <linux/ktime.h>

#define setup_timer(x, y, z)	timeout_set((x), (void (*)(void *))(y), (void *)(z))
#define mod_timer(x, y)		timeout_add((x), (y - jiffies))
#define mod_timer_pinned(x, y)	timeout_add((x), (y - jiffies))
#define del_timer_sync(x)	timeout_del((x))
#define timer_pending(x)	timeout_pending((x))

static inline unsigned long
round_jiffies_up(unsigned long j)
{
	return roundup(j, hz);
}

static inline unsigned long
round_jiffies_up_relative(unsigned long j)
{
	return roundup(j, hz);
}

#endif
