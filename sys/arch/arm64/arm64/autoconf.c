/*	$OpenBSD: autoconf.c,v 1.4 2017/06/29 05:40:35 deraadt Exp $	*/
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
#include <uvm/uvm.h>

#include <machine/bootconfig.h>

extern void dumpconf(void);
void	parsepmonbp(void);

struct device *bootdv = NULL;
enum devclass bootdev_class = DV_DULL;

void
unmap_startup(void)
{
#if 0
	extern void *_start, *endboot;
	vaddr_t p = (vaddr_t)&_start;

	do {
		pmap_kremove(p, PAGE_SIZE);
		p += PAGE_SIZE;
	} while (p < (vaddr_t)&endboot);
#endif
}

void
cpu_configure(void)
{
	(void)splhigh();

	softintr_init();
	(void)config_rootfound("mainbus", NULL);

	unmap_startup();

	cold = 0;
	spl0();
}

void
diskconf(void)
{
	size_t	len;
	char	*p;
	dev_t	tmpdev;

	if (*boot_file != '\0')
		printf("bootfile: %s\n", boot_file);

	if (bootdv == NULL) {

		// boot_file is of the format <device>:/bsd we want the device part
		if ((p = strchr(boot_file, ':')) != NULL)
			len = p - boot_file;
		else
			len = strlen(boot_file);
		bootdv = parsedisk(boot_file, len, 0, &tmpdev);
	}

	if (bootdv != NULL)
		printf("boot device: %s\n", bootdv->dv_xname);
	else
		printf("boot device: lookup %s failed \n", boot_file);

	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();

#ifdef HIBERNATE
	hibernate_resume();
#endif /* HIBERNATE */
}

void
device_register(struct device *dev, void *aux)
{
}

struct nam2blk nam2blk[] = {
	{ "sd",		4 },
	{ "nbd",	20 },
	{ "tmpfsrd",	19 },
	{ "cd",		6},
	{ "wd",		0 },
	{ NULL,		-1 }
};
