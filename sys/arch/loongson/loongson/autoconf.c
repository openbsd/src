/*	$OpenBSD: autoconf.c,v 1.1.1.1 2009/11/24 11:28:11 miod Exp $	*/
/*
 * Copyright (c) 2009 Miodrag Vallat.
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
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>

extern void dumpconf(void);

int	cold = 1;
struct device *bootdv = NULL;

void
cpu_configure(void)
{
	(void)splhigh();

	softintr_init();
	(void)config_rootfound("mainbus", NULL);

	splinit();
	cold = 0;
}

void
diskconf(void)
{
	if (bootdv != NULL)
		printf("boot device: %s\n", bootdv->dv_xname);

	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();
}

void
device_register(struct device *dev, void *aux)
{
	if (bootdv != NULL)
		return;

	/* ... */
}

struct nam2blk nam2blk[] = {
	{ "sd",		0 },
	{ "wd",		4 },
	{ "rd",		8 },
	{ "vnd",	2 },
	{ NULL,		-1 }
};
