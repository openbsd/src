/*	$OpenBSD: wscons_machdep.c,v 1.6 2010/02/28 22:32:50 miod Exp $ */

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
/*
 * Copyright (c) 2001 Aaron Campbell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/extent.h>

#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <dev/cons.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include "wsdisplay.h"
#if NWSDISPLAY > 0
#include <dev/wscons/wsdisplayvar.h>
#endif
#include "sisfb.h"
extern int sisfb_cnattach(bus_space_tag_t, bus_space_tag_t, pcitag_t, pcireg_t);
#include "smfb.h"
extern int smfb_cnattach(bus_space_tag_t, bus_space_tag_t, pcitag_t, pcireg_t);

#include "pckbc.h"
#if NPCKBC > 0
#include <dev/isa/isareg.h>
#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#endif
#include "pckbd.h"
#include "ukbd.h"
#if NUKBD > 0
#include <dev/usb/ukbdvar.h>
#endif
#if NPCKBD > 0 || NUKBD > 0
#include <dev/wscons/wskbdvar.h>
#endif

cons_decl(ws);

void
wscnprobe(struct consdev *cp)
{
	int maj;

	/* Locate the major number. */
	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;
	}

	if (maj == nchrdev) {
		/* We are not in cdevsw[], give up. */
		panic("wsdisplay is not in cdevsw[]");
	}

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_MIDPRI;
}

void
wscninit(struct consdev *cp)
{
static	int initted;
	pcitag_t tag;
	pcireg_t id, class;
	int dev, rc;
	extern struct mips_bus_space bonito_pci_io_space_tag;
	extern struct mips_bus_space bonito_pci_mem_space_tag;

	if (initted)
		return;

	initted = 1;

	/*
	 * Look for a suitable video device.
	 */

	for (dev = 0; dev < 32; dev++) {
		tag = pci_make_tag_early(0, dev, 0);
		id = pci_conf_read_early(tag, PCI_ID_REG);
		if (id == 0 || PCI_VENDOR(id) == PCI_VENDOR_INVALID)
			continue;

		class = pci_conf_read_early(tag, PCI_CLASS_REG);
		if (PCI_CLASS(class) != PCI_CLASS_DISPLAY)
			continue;

		/*
		 * Try to configure this device as glass console.
		 */

		rc = ENXIO;
#if NSISFB > 0
		if (rc != 0)
			rc = sisfb_cnattach(&bonito_pci_mem_space_tag,
			    &bonito_pci_io_space_tag, tag, id);
#endif
#if NSMFB > 0
		if (rc != 0)
			rc = smfb_cnattach(&bonito_pci_mem_space_tag,
			    &bonito_pci_io_space_tag, tag, id);
#endif
		if (rc == 0)
			goto setup_kbd;
	}

	/* no glass console... */
	return;

setup_kbd:

	/*
	 * Look for a suitable input device.
	 */

	rc = ENXIO;

#if NPCKBC > 0
	switch (sys_platform->system_type) {
	default:
		/* no pckbc or no legacy hardware */
		break;
	case LOONGSON_YEELOONG:
		if (rc != 0)
			rc = pckbc_cnattach(&bonito_pci_io_space_tag, IO_KBD,
			    KBCMDP, PCKBC_KBD_SLOT, 0);
		break;
	}
#endif
#if NUKBD > 0
	if (rc != 0)
		rc = ukbd_cnattach();
#endif
}

void
wscnputc(dev_t dev, int i)
{
#if NWSDISPLAY > 0
	wsdisplay_cnputc(dev, i);
#endif
}

int
wscngetc(dev_t dev)
{
#if NPCKBD > 0
	int c;

	wskbd_cnpollc(dev, 1);
	c = wskbd_cngetc(dev);
	wskbd_cnpollc(dev, 0);

	return c;
#else
	for (;;) ;
#endif
}

void
wscnpollc(dev_t dev, int on)
{
#if NPCKBD > 0
	wskbd_cnpollc(dev, on);
#endif
}
