/*	$OpenBSD: sun4i.c,v 1.4 2016/08/13 13:55:25 kettenis Exp $	*/
/*
 * Copyright (c) 2011 Uwe Stuehler <uwe@openbsd.org>
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

#include <sys/types.h>
#include <sys/param.h>

#include <machine/bus.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>

struct armv7_dev sxia1x_devs[] = {

	/* 'Port IO' */
	{ .name = "sxipio",
	  .unit = 0,
	  .mem = { { PIO_ADDR, PIOx_SIZE } },
	  .irq = { PIO_IRQ }
	},

	/* Clock Control Module/Unit */
	{ .name = "sxiccmu",
	  .unit = 0,
	  .mem = { { CCMU_ADDR, CCMU_SIZE } },
	},

	/* Timers/Counters, resources mapped on first unit */
	{ .name = "sxitimer",
	  .unit = 0,
	  .mem = {	{ TIMER_ADDR, TIMERx_SIZE },
			{ CPUCNTRS_ADDR, CPUCNTRS_ADDR } }
	},
	{ .name = "sxitimer",
	  .unit = 1,
	},
	{ .name = "sxitimer",
	  .unit = 2,
	},

	/* Terminator */
	{ .name = NULL,
	  .unit = 0,
	}
};

void
sxia1x_init(void)
{
	armv7_set_devs(sxia1x_devs);
}
