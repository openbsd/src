/*	$OpenBSD: exynos_machdep.c,v 1.9 2016/06/08 15:27:05 jsg Exp $	*/
/*
 * Copyright (c) 2013 Patrick Wildt <patrick@blueri.se>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/termios.h>

#include <machine/bus.h>

#include <arm/cortex/smc.h>
#include <arm/armv7/armv7var.h>
#include <arm/mainbus/mainbus.h>
#include <armv7/armv7/armv7var.h>
#include <armv7/exynos/exdisplayvar.h>
#include <armv7/exynos/exuartvar.h>
#include <armv7/armv7/armv7_machdep.h>

extern void exdog_reset(void);
extern struct board_dev *exynos_board_devs(void);
extern void exynos_board_init(void);

static void
exynos_platform_smc_write(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    uint32_t op, uint32_t val)
{
	bus_space_write_4(iot, ioh, off, val);
}

void
exynos_platform_init_mainbus(struct device *self)
{
	mainbus_legacy_found(self, "cortex");
	mainbus_legacy_found(self, "exynos");
}

static void
exynos_platform_watchdog_reset(void)
{
	exdog_reset();
}

static void
exynos_platform_powerdown(void)
{

}

static void
exynos_platform_disable_l2_if_needed(void)
{

}

void
exynos_platform_board_init(void)
{
	exynos_board_init();
}

struct armv7_platform exynos_platform = {
	.board_init = exynos_platform_board_init,
	.smc_write = exynos_platform_smc_write,
	.watchdog_reset = exynos_platform_watchdog_reset,
	.powerdown = exynos_platform_powerdown,
	.disable_l2_if_needed = exynos_platform_disable_l2_if_needed,
	.init_mainbus = exynos_platform_init_mainbus,
};

struct armv7_platform *
exynos_platform_match(void)
{
	struct board_dev *devs;

	devs = exynos_board_devs();
	if (devs == NULL)
		return (NULL);

	exynos_platform.devs = devs;
	return (&exynos_platform);
}
