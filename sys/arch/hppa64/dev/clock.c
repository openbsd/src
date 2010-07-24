/*	$OpenBSD: clock.c,v 1.2 2010/07/24 21:27:57 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/timetc.h>

#include <machine/iomod.h>
#include <machine/reg.h>

#include <dev/clock_subr.h>

u_long	cpu_hzticks;

u_int	itmr_get_timecount(struct timecounter *);

struct timecounter itmr_timecounter = {
	itmr_get_timecount, NULL, 0xffffffff, 0, "itmr", 0, NULL
};

void
cpu_initclocks()
{
	struct cpu_info *ci = curcpu();
	u_long __itmr;

	cpu_hzticks = (PAGE0->mem_10msec * 100) / hz;

	itmr_timecounter.tc_frequency = PAGE0->mem_10msec * 100;
	tc_init(&itmr_timecounter);

	__itmr = mfctl(CR_ITMR);
	ci->ci_itmr = __itmr;
	__itmr += cpu_hzticks;
	mtctl(__itmr, CR_ITMR);
}

void
setstatclockrate(int newhz)
{
	/* nothing we can do */
}

u_int
itmr_get_timecount(struct timecounter *tc)
{
	return (mfctl(CR_ITMR));
}
