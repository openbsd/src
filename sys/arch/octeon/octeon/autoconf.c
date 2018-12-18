/*	$OpenBSD: autoconf.c,v 1.14 2018/12/18 13:44:11 visa Exp $	*/
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

#include <machine/autoconf.h>

#define DUID_SIZE	8

extern void dumpconf(void);
int parseduid(const char *, u_char *);
void parse_uboot_root(void);

int	cold = 1;
struct device *bootdv = NULL;
char    bootdev[16];
enum devclass bootdev_class = DV_DULL;
extern char uboot_rootdev[];

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

struct devmap {
	char		*dev;
	enum devclass	 class;
};

struct devmap *
findtype(void)
{
	static struct devmap devmap[] = {
		{ "wd", DV_DISK },
		{ "sd", DV_DISK },
		{ "octcf", DV_DISK },
		{ "amdcf", DV_DISK },
		{ "cnmac", DV_IFNET },
		{ NULL, DV_DULL }
	};
	struct devmap *dp = &devmap[0];

	if (strlen(bootdev) < 2)
		return dp;

	while (dp->dev) {
		if (strncmp(bootdev, dp->dev, strlen(dp->dev)) == 0)
			break;
		dp++;
	}

	if (dp->dev == NULL)
		printf("%s is not a valid rootdev\n", bootdev);

	return dp;
}

void
parse_uboot_root(void)
{
	struct devmap *dp;
	char *p;
	size_t len;

	/*
	 * Turn the U-Boot root device (rootdev=/dev/octcf0) into a boot device.
	 */
	p = strrchr(uboot_rootdev, '/');
	if (p == NULL) {
		p = strchr(uboot_rootdev, '=');
		if (p == NULL)
			return;
	}
	p++;

	if (parseduid(p, bootduid) == 0) {
		bootdev_class = DV_DISK;
		return;
	}

	len = strlen(p);
	if (len <= 2 || len >= sizeof bootdev - 1)
		return;

	strlcpy(bootdev, p, sizeof(bootdev));

	dp = findtype();
	bootdev_class = dp->class;
}

static unsigned int
parsehex(int c)
{
	if (c >= 'a')
		return c - 'a' + 10;
	else
		return c - '0';
}

int
parseduid(const char *str, u_char *duid)
{
	int i;

	for (i = 0; i < DUID_SIZE * 2; i++) {
		if (!(str[i] >= '0' && str[i] <= '9') &&
		    !(str[i] >= 'a' && str[i] <= 'f'))
			return -1;
	}
	if (str[DUID_SIZE * 2] != '\0')
		return -1;

	for (i = 0; i < DUID_SIZE; i++) {
		duid[i] = parsehex(str[i * 2]) * 0x10 +
		    parsehex(str[i * 2 + 1]);
	}

	return 0;
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
	if (bootdv != NULL || dev->dv_class != bootdev_class)
		return;

	switch (bootdev_class) {
	case DV_DISK:
	case DV_IFNET:
		if (strcmp(dev->dv_xname, bootdev) == 0)
			bootdv = dev;
		break;
	default:
		break;
	}
}

struct nam2blk nam2blk[] = {
	{ "sd",		0 },
	{ "vnd",	2 },
	{ "cd",		3 },
	{ "wd",		4 },
	{ "rd",		8 },
	{ "octcf",	15 },
	{ "amdcf",	19 },
	{ NULL,		-1 }
};
