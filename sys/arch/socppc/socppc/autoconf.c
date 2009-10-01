/*	$OpenBSD: autoconf.c,v 1.4 2009/10/01 20:19:19 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <sys/reboot.h>

#include <uvm/uvm_extern.h>

#include <machine/powerpc.h>

void	dumpconf(void);

int	cold = 1;

struct dumpmem dumpmem[VM_PHYSSEG_MAX];
u_int ndumpmem;

void
cpu_configure(void)
{
	splhigh();

	softintr_init();

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");

	/* Configuration is finished, turn on interrupts. */
	spl0();
	cold = 0;
}

void
device_register(struct device *dev, void *aux)
{
}

void
diskconf(void)
{
	struct device *dv;
	dev_t tmpdev;
	int len;
	char *p;

	if ((p = strchr(bootpath, ':')) != NULL)
		len = p - bootpath;
	else
		len = strlen(bootpath);

	dv = parsedisk(bootpath, len, 0, &tmpdev);
	setroot(dv, 0, RB_USERREQ);
	dumpconf();
}

struct nam2blk nam2blk[] = {
	{ "wd",		0 },
	{ "sd",		2 },
	{ "rd",		17 },
	{ "raid",	19 },
	{ "vnd",	14 },
	{ NULL,		-1 }
};
