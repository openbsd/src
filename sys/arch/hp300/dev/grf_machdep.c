/*	$NetBSD: grf_machdep.c,v 1.4 1996/02/24 00:55:13 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1991 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: grf_machdep.c 1.1 92/01/21
 *
 *	@(#)grf_machdep.c	8.2 (Berkeley) 1/12/94
 */

/*
 * Graphics display driver for the HP300/400 DIO/DIO-II based machines.
 * This is the hardware-dependent configuration portion of the driver.
 */

#include "grf.h"
#if NGRF > 0

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/autoconf.h>

#include <hp300/dev/device.h>
#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>
#include <hp300/dev/grfreg.h>

#include <hp300/dev/itevar.h>

#include "ite.h"

int grfmatch();
void grfattach();

int	grfinit __P((struct hp_device *, struct grf_data *, int));

struct	driver grfdriver = { grfmatch, grfattach, "grf" };

int
grfmatch(hd)
	struct hp_device *hd;
{
	struct grf_softc *sc = &grf_softc[hd->hp_unit];
	int scode;

	if (hd->hp_args->hw_pa == (caddr_t)GRFIADDR) /* XXX */
		scode = -1;
	else
		scode = hd->hp_args->hw_sc;

	if (scode == conscode) {
		/*
		 * We've already been initialized.
		 */
		sc->sc_data = &grf_cn;
		return (1);
	}

	/*
	 * Allocate storage space for the grf_data.
	 */
	sc->sc_data = (struct grf_data *)malloc(sizeof(struct grf_data),
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_data == NULL) {
		printf("grfmatch: malloc for grf_data failed\n");
		return (0);
	}
	bzero(sc->sc_data, sizeof(struct grf_data));

	return (grfinit(hd, sc->sc_data, scode));
}

void
grfattach(hd)
	struct hp_device *hd;
{
	struct grf_softc *sc = &grf_softc[hd->hp_unit];
	struct grf_data *gp = sc->sc_data;
	int scode, isconsole;

	if (hd->hp_args->hw_pa == (caddr_t)GRFIADDR) /* XXX */
		scode = -1;
	else
		scode = hd->hp_args->hw_sc;

	if (scode == conscode)
		isconsole = 1;
	else
		isconsole = 0;

	printf(": %d x %d ", gp->g_display.gd_dwidth,
	    gp->g_display.gd_dheight);
	if (gp->g_display.gd_colors == 2)
		printf("monochrome");
	else
		printf("%d color", gp->g_display.gd_colors);
	printf(" %s display", gp->g_sw->gd_desc);
	if (isconsole)
		printf(" (console)");
	printf("\n");

#if NITE > 0
	/* XXX hack */
	ite_attach_grf(hd->hp_unit, isconsole);
#endif /* NITE > 0 */
}

int
grfinit(hd, gp, scode)
	struct hp_device *hd;
	struct grf_data *gp;
	int scode;
{
	register struct grfsw *gsw;
	struct grfreg *gr;
	caddr_t addr = hd->hp_addr;
	int i;

	gr = (struct grfreg *) addr;
	if (gr->gr_id != GRFHWID)
		return(0);
	for (i = 0; i < ngrfsw; ++i) {
		gsw = grfsw[i];
		if (gsw->gd_hwid == gr->gr_id2)
			break;
	}
	if ((i < ngrfsw) && (*gsw->gd_init)(gp, scode, addr)) {
		gp->g_sw = gsw;
		gp->g_display.gd_id = gsw->gd_swid;
		gp->g_flags = GF_ALIVE;
		return(1);
	}
	return(0);
}
#endif /* NGRF > 0 */
