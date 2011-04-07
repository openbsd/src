/*	$OpenBSD: wscons_machdep.c,v 1.11 2011/04/07 15:30:15 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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
 * 3. The name of the author may not be used to endorse or promote products
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

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/hp300spu.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include "wsdisplay.h"
#include "wskbd.h"
#if NWSKBD > 0
#include <dev/wscons/wskbdvar.h>
#endif

#include "dvbox.h"
#include "gbox.h"
#include "hyper.h"
#include "rbox.h"
#include "topcat.h"
#include "tvrx.h"
#if NDVBOX > 0 || NGBOX > 0 || NHYPER > 0 || NRBOX > 0 || NTOPCAT > 0 || NTVRX > 0
#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diofbreg.h>
#include <hp300/dev/diofbvar.h>
struct diofb diofb_cn;
#endif

#include "sti.h"
#if NSTI > 0
#include <machine/bus.h>
#include <hp300/dev/sgcreg.h>
#include <hp300/dev/sgcvar.h>
#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>
extern	int sti_console_scan(int);
extern	void sticninit(void);
#endif

extern caddr_t internalhpib;

cons_decl(ws);

void (*wsfbcninit)(void) = NULL;

#if NDVBOX > 0 || NGBOX > 0 || NHYPER > 0 || NRBOX > 0 || NTOPCAT > 0 || NTVRX > 0
int	dio_fbidentify(struct diofbreg *);

/*
 * Identify a DIO frame buffer and set up wsfbcninit accordingly.
 */
int
dio_fbidentify(struct diofbreg *fbr)
{
	if (fbr->id == GRFHWID)
		switch (fbr->fbid) {
#if NDVBOX > 0
		case GID_DAVINCI:
			wsfbcninit = dvboxcninit;
			return (1);
#endif
#if NGBOX > 0
		case GID_GATORBOX:
			wsfbcninit = gboxcninit;
			return (1);
#endif
#if NHYPER > 0
		case GID_HYPERION:
			wsfbcninit = hypercninit;
			return (1);
#endif
#if NRBOX > 0
		case GID_RENAISSANCE:
			wsfbcninit = rboxcninit;
			return (1);
#endif
#if NTOPCAT > 0
		case GID_TOPCAT:
		case GID_LRCATSEYE:
		case GID_HRCCATSEYE:
		case GID_HRMCATSEYE:
			wsfbcninit = topcatcninit;
			return (1);
#endif
#if NTVRX > 0
		case GID_TIGER:
			wsfbcninit = tvrxcninit;
			return (1);
#endif
		default:
			break;
		}

	return (0);
}
#endif

/*
 * This routine handles the dirty work of picking the best frame buffer
 * suitable for the console.
 * We try to behave as close as possible to the PROM's logic, by preferring
 * devices for which we have drivers, in that order:
 * - internal video.
 * - lowest select code on DIO bus.
 * - lowest slot on SGC bus.
 */
void
wscnprobe(struct consdev *cp)
{
	int maj, tmpconscode;
	vsize_t mapsize;
	vaddr_t va;
#if NDVBOX > 0 || NGBOX > 0 || NHYPER > 0 || NRBOX > 0 || NTOPCAT > 0 || NTVRX > 0
	paddr_t pa;
	u_int scode, sctop, sctmp;
	struct diofbreg *fbr;
#endif

	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;
	}

	cp->cn_dev = makedev(maj, 0);
	wsfbcninit = NULL;

#if NDVBOX > 0 || NGBOX > 0 || NRBOX > 0 || NTOPCAT > 0
	/*
	 * Look for an ``internal'' frame buffer.
	 */
	va = IIOV(GRFIADDR);
	fbr = (struct diofbreg *)va;
	if (!badaddr((caddr_t)va)) {
		if (dio_fbidentify(fbr)) {
			tmpconscode = CONSCODE_INTERNAL;
			mapsize = 0;
			goto found;
		}
	}
#endif

#if NDVBOX > 0 || NGBOX > 0 || NHYPER > 0 || NRBOX > 0 || NTOPCAT > 0 || NTVRX > 0
	/*
	 * Scan the DIO bus.
	 */
	sctop = DIO_SCMAX(machineid);
	for (scode = 0; scode < sctop; scode++) {
		/*
		 * Skip over the select code hole and the internal
		 * HP-IB controller.
		 */
		if ((sctmp = dio_inhole(scode)) != 0) {
			scode = sctmp - 1;
			continue;
		}
		if (scode == 7 && internalhpib)
			continue;

		/* Map current PA. */
		pa = (paddr_t)dio_scodetopa(scode);
		va = (vaddr_t)iomap((caddr_t)pa, PAGE_SIZE);
		if (va == 0)
			continue;

		/* Check to see if hardware exists. */
		if (badaddr((caddr_t)va)) {
			iounmap((caddr_t)va, PAGE_SIZE);
			continue;
		}

		/* Check hardware. */
		fbr = (struct diofbreg *)va;
		if (dio_fbidentify(fbr)) {
			tmpconscode = scode;
			mapsize = DIO_SIZE(scode, va);
			iounmap((caddr_t)va, PAGE_SIZE);
			va = (vaddr_t)iomap((caddr_t)pa, mapsize);
			if (va == 0)
				continue;
			goto found;
		} else
			iounmap((caddr_t)va, PAGE_SIZE);
	}
#endif

#if NSTI > 0
	/*
	 * Scan the SGC bus.
	 */
	for (scode = 0; scode < SGC_NSLOTS; scode++) {
		int rv;

		/* Map current PA. */
		pa = (paddr_t)sgc_slottopa(scode);
		va = (vaddr_t)iomap((caddr_t)pa, PAGE_SIZE);
		if (va == 0)
			continue;

		/* Check to see if hardware exists. */
		rv = badaddr((caddr_t)va);
		iounmap((caddr_t)va, PAGE_SIZE);
		if (rv != 0)
			continue;

		/* Check hardware. */
		if (sti_console_scan(scode) != 0) {
			wsfbcninit = sticninit;
			tmpconscode = SGC_SLOT_TO_CONSCODE(scode);
			mapsize = 0;
			va = 0;
			goto found;
		}
	}
#endif

	return;

found:
	cp->cn_pri = CN_MIDPRI;
#ifdef CONSCODE
	if (CONSCODE == tmpconscode)
		cp->cn_pri = CN_FORCED;
#endif

	/*
	 * If our priority is higher than the currently remembered console,
	 * install ourselves, and unmap whichever device might be currently
	 * mapped.
	 */
	if (cn_tab == NULL || cp->cn_pri > cn_tab->cn_pri) {
		cn_tab = cp;
		/* Free last mapping. */
		if (convasize)
			iounmap(conaddr, convasize);
		conscode = tmpconscode;
		conaddr = (caddr_t)va;
		convasize = mapsize;
	}
}

void
wscninit(struct consdev *cp)
{
	/*
	 * Note that this relies on the fact that DIO frame buffers will cause
	 * cn_tab to switch to wsdisplaycons, so their cninit function will
	 * never get invoked a second time during the second console pass.
	 */
	if (wsfbcninit != NULL)
		(*wsfbcninit)();
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
#if NWSKBD > 0
	return (wskbd_cngetc(dev));
#else
	return (0);
#endif
}

void
wscnpollc(dev_t dev, int on)
{
#if NWSKBD > 0
	wskbd_cnpollc(dev, on);
#endif
}
