/*	$OpenBSD: vexpress_machdep.c,v 1.5 2016/08/31 14:23:37 jsg Exp $	*/
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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
#include <machine/bootconfig.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <arm/cortex/smc.h>
#include <arm/armv7/armv7var.h>
#include <arm/mainbus/mainbus.h>
#include <armv7/armv7/armv7var.h>
#include <armv7/armv7/armv7_machdep.h>

extern void sysconf_reboot(void);
extern void sysconf_shutdown(void);
extern struct board_dev *vexpress_board_devs(void);
extern void vexpress_board_init(void);
extern int vexpress_legacy_map(void);

void
vexpress_platform_smc_write(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    uint32_t op, uint32_t val)
{
	bus_space_write_4(iot, ioh, off, val);
}

void
vexpress_platform_init_mainbus(struct device *self)
{
	mainbus_legacy_found(self, "cortex");
	mainbus_legacy_found(self, "vexpress");
}

void
vexpress_platform_watchdog_reset(void)
{
	sysconf_reboot();
}

void
vexpress_platform_powerdown(void)
{
	sysconf_shutdown();
}

void
vexpress_platform_disable_l2_if_needed(void)
{

}

void
vexpress_platform_board_init(void)
{
	vexpress_board_init();
}

struct armv7_platform vexpress_platform = {
	.board_init = vexpress_platform_board_init,
	.smc_write = vexpress_platform_smc_write,
	.watchdog_reset = vexpress_platform_watchdog_reset,
	.powerdown = vexpress_platform_powerdown,
	.disable_l2_if_needed = vexpress_platform_disable_l2_if_needed,
	.init_mainbus = vexpress_platform_init_mainbus,
};

struct armv7_platform *
vexpress_platform_match(void)
{
	struct board_dev *devs;

	devs = vexpress_board_devs();
	if (devs == NULL)
		return (NULL);

	vexpress_platform.devs = devs;
	return (&vexpress_platform);
}
