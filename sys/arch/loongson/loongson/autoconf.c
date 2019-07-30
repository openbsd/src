/*	$OpenBSD: autoconf.c,v 1.9 2018/01/27 22:55:23 naddy Exp $	*/
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
#include <sys/hibernate.h>

#include <machine/autoconf.h>

extern void dumpconf(void);
void	parsepmonbp(void);

int	cold = 1;
struct device *bootdv = NULL;
char    bootdev[16];
enum devclass bootdev_class = DV_DULL;

extern char pmon_bootp[];

void
cpu_configure(void)
{
	(void)splhigh();

	softintr_init();
	(void)config_rootfound("mainbus", NULL);

	unmap_startup();

	splinit();
	cold = 0;
}

void
parsepmonbp(void)
{
	char *p, *q;
	size_t len;

	if (strncmp(pmon_bootp, "tftp://", 7) == 0) {
		bootdev_class = DV_IFNET;
		strlcpy(bootdev, "netboot", sizeof bootdev);
		return;
	}
	strlcpy(bootdev, "unknown", sizeof bootdev);

	if (strncmp(pmon_bootp, "/dev/disk/", 10) == 0) {
		/* kernel loaded by our boot blocks */
		p = pmon_bootp + 10;
		len = strlen(p);
	} else {
		/* kernel loaded by PMON */
		p = strchr(pmon_bootp, '@');
		if (p == NULL)
			return;
		p++;

		q = strchr(p, '/');
		if (q == NULL)
			return;
		len = q - p;
	}

	if (len <= 2 || len >= sizeof bootdev - 1)
		return;
	memcpy(bootdev, p, len);
	bootdev[len] = '\0';
	bootdev_class = DV_DISK;
}

void
diskconf(void)
{
	if (*pmon_bootp != '\0')
		printf("pmon bootpath: %s\n", pmon_bootp);

	if (bootdv != NULL)
		printf("boot device: %s\n", bootdv->dv_xname);

	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();

#ifdef HIBERNATE
	hibernate_resume();
#endif /* HIBERNATE */
}

void
device_register(struct device *dev, void *aux)
{
	if (bootdv != NULL)
		return;

	(*sys_platform->device_register)(dev, aux);
}

struct nam2blk nam2blk[] = {
	{ "sd",		0 },
	{ "vnd",	2 },
	{ "cd",		3 },
	{ "wd",		4 },
	{ "rd",		8 },
	{ NULL,		-1 }
};
