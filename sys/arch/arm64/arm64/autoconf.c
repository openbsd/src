/*	$OpenBSD: autoconf.c,v 1.10 2019/01/09 13:18:50 yasuoka Exp $	*/
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
#include <sys/disklabel.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/hibernate.h>
#include <uvm/uvm.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bootconfig.h>

extern void dumpconf(void);
void	parsepmonbp(void);

struct device *bootdv = NULL;
enum devclass bootdev_class = DV_DULL;

void
unmap_startup(void)
{
	extern void *_start, *endboot;
	vaddr_t p = (vaddr_t)&_start;

	do {
		pmap_kremove(p, PAGE_SIZE);
		p += PAGE_SIZE;
	} while (p < (vaddr_t)&endboot);
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
	dev_t	tmpdev = NODEV;
	int	part = 0;
	extern uint8_t *bootmac;

	if (*boot_file != '\0')
		printf("bootfile: %s\n", boot_file);

	if (bootdv == NULL) {

		// boot_file is of the format <device>:/bsd we want the device part
		if ((p = strchr(boot_file, ':')) != NULL)
			len = p - boot_file;
		else
			len = strlen(boot_file);
		bootdv = parsedisk(boot_file, len, 0, &tmpdev);
		if (tmpdev != NODEV)
			part = DISKPART(tmpdev);
	}

#if defined(NFSCLIENT)
	if (bootmac) {
		struct ifnet *ifp;

		TAILQ_FOREACH(ifp, &ifnet, if_list) {
			if (ifp->if_type == IFT_ETHER &&
			    memcmp(bootmac, ((struct arpcom *)ifp)->ac_enaddr,
			    ETHER_ADDR_LEN) == 0)
				break;
		}
		if (ifp)
			bootdv = parsedisk(ifp->if_xname, strlen(ifp->if_xname),
			    0, &tmpdev);
	}
#endif

	if (bootdv != NULL)
		printf("boot device: %s\n", bootdv->dv_xname);
	else
		printf("boot device: lookup %s failed \n", boot_file);

	setroot(bootdv, part, RB_USERREQ);
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
	{ "wd",		 0 },
	{ "sd",		 4 },
	{ "cd",		 6 },
	{ "vnd",	14 },
	{ "rd",		17 },
	{ NULL,		-1 }
};
