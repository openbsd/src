/*	$OpenBSD: wscons_machdep.c,v 1.3 2006/01/09 20:51:49 miod Exp $	*/
/*	$NetBSD: maccons.c,v 1.5 2005/01/15 16:00:59 chs Exp $	*/

/*
 * Copyright (C) 1999 Scott Reynolds.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <mac68k/dev/nubus.h>
#include <mac68k/dev/macfbvar.h>
#include <mac68k/dev/akbdvar.h>

#include "wsdisplay.h"
#include "wskbd.h"

cons_decl(ws);

int	maccons_initted = (-1);

/* From Booter via locore */
extern u_int32_t	mac68k_vidphys;

void
wscnprobe(struct consdev *cp)
{
#if NWSDISPLAY > 0
	int     maj, unit;
#endif

	cp->cn_dev = NODEV;
	cp->cn_pri = CN_NORMAL;

#if NWSDISPLAY > 0
	unit = 0;
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;

	if (maj != nchrdev) {
		cp->cn_pri = CN_INTERNAL;
		cp->cn_dev = makedev(maj, unit);
	}
#endif
}

void
wscninit(struct consdev *cp)
{
	/*
	 * XXX evil hack; see consinit() for an explanation.
	 * note:  maccons_initted is initialized to (-1).
	 */
	if (++maccons_initted > 0) {
		macfb_cnattach(mac68k_vidphys);
		akbd_cnattach();
	}
}

int
wscngetc(dev_t dev)
{
#if NWSKBD > 0
	return wskbd_cngetc(dev);
#else
	return 0;
#endif
}

void
wscnputc(dev_t dev, int c)
{
#if NWSDISPLAY > 0
	wsdisplay_cnputc(dev,c);	
#endif
}

void
wscnpollc(dev_t dev, int on)
{
#if NWSKBD > 0
	wskbd_cnpollc(dev,on);
#endif
}
