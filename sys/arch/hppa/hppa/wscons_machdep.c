/*	$OpenBSD: wscons_machdep.c,v 1.3 2002/11/08 22:27:17 mickey Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <machine/bus.h>
#include <machine/pdc.h>
#include <machine/iomod.h>

#include <dev/cons.h>

#include "sti.h"
#if NSTI > 0
#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>
#endif

#include "ps2p.h"
#if NPS2P > 0
#include <hppa/gsc/ps2pvar.h>
#endif

#include "wsdisplay.h"
#if NWSDISPLAY > 0
#include <dev/wscons/wsdisplayvar.h>
#endif

#include "wskbd.h"
#if NWSKBD > 0
#include <dev/wscons/wskbdvar.h>
#endif

cons_decl(ws);

void
wscnprobe(cp)
	struct consdev *cp;
{
	int maj;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;

	if (maj == nchrdev)
		panic("wsdisplay is not in cdevsw[]");

#if NSTI > 0
	if (PAGE0->mem_cons.pz_class == PCL_DISPL) {

	} else
		return;
#else
	return;
#endif
#if NPS2P > 0
	if (PAGE0->mem_kbd.pz_class == PCL_KEYBD) {

	} else
		return;
#else
	return;
#endif

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_INTERNAL;
}

void
wscninit(cp)
	struct consdev *cp;
{
}

void
wscnputc(dev, i)
	dev_t dev;
	char i;
{
	wsdisplay_cnputc(dev, (int)i);
}

int
wscngetc(dev)
	dev_t dev;
{
#if NWSKBD > 0
	return (wskbd_cngetc(dev));
#else
	return (0);
#endif
}

void
wscnpollc(dev, on)
	dev_t dev;
	int on;
{
#if NWSKBD > 0
	wskbd_cnpollc(dev, on);
#endif
}
