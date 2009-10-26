/*	$OpenBSD: wscons_machdep.c,v 1.4 2009/10/26 18:00:06 miod Exp $ */

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

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebusvar.h>

#include <sgi/dev/gbereg.h>
#include <sgi/dev/mkbcreg.h>

#include <dev/cons.h>
#include <dev/ic/pckbcvar.h>
#include <dev/usb/ukbdvar.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsconsio.h>

#include "gbe.h"
#include "mkbc.h"

#include "wsdisplay.h"
#if NWSDISPLAY > 0
#include <dev/wscons/wsdisplayvar.h>
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
	cp->cn_pri = CN_DEAD;

        switch (sys_config.system_type) {
	case SGI_O2:
#if NGBE > 0
		if (gbe_cnprobe(&crimebus_tag, GBE_BASE)) {
			if (strncmp(bios_console, "video", 5) == 0)
				cp->cn_pri = CN_FORCED;
			else
				cp->cn_pri = CN_MIDPRI;
		}
#endif
		break;
	default:
		break;
	}
}

void
wscninit(struct consdev *cp)
{
static int initted;

	if (initted)
		return;

	initted = 1;

#if NGBE > 0
	if (!gbe_cnattach(&crimebus_tag, GBE_BASE))
		return;
#endif
#if NMKBC > 0
	if (!mkbc_cnattach(&macebus_tag, 0x00320000, PCKBC_KBD_SLOT))
		return;
#endif
}

void
wscnputc(dev_t dev, int i)
{
	wsdisplay_cnputc(dev, i);
}

int
wscngetc(dev_t dev)
{
	int c;

	wskbd_cnpollc(dev, 1);
	c = wskbd_cngetc(dev);
	wskbd_cnpollc(dev, 0);

	return c;
}

void
wscnpollc(dev_t dev, int on)
{
	wskbd_cnpollc(dev, on);
}
