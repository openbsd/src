/*	$OpenBSD: com_puc.c,v 1.11 2005/12/24 04:42:19 djm Exp $	*/

/*
 * Copyright (c) 1997 - 1999, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/selinfo.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>

#ifdef i386
#include <dev/isa/isavar.h>	/* XXX */
#endif

#include "com.h"
#ifdef i386
#include "pccom.h"
#endif

#include <dev/ic/comreg.h>
#if NPCCOM > 0
#include <i386/isa/pccomvar.h>
#endif
#if NCOM > 0
#include <dev/ic/comvar.h>
#endif
#include <dev/ic/ns16550reg.h>

#define	com_lcr		com_cfcr

int com_puc_match(struct device *, void *, void *);
void com_puc_attach(struct device *, struct device *, void *);

#if NCOM > 0
struct cfattach com_puc_ca = {
	sizeof(struct com_softc), com_puc_match, com_puc_attach
};
#endif

#if NPCCOM > 0
struct cfattach pccom_puc_ca = {
	sizeof(struct com_softc), com_puc_match, com_puc_attach
};
#endif

int
com_puc_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct puc_attach_args *pa = aux;

	if (pa->type == PUC_PORT_TYPE_COM)
		return(1);

	return(0);
}

void
com_puc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	struct puc_attach_args *pa = aux;
	const char *intrstr;

	/* Grab a PCI interrupt. */
	intrstr = pci_intr_string(pa->pc, pa->intrhandle);
	sc->sc_ih = pci_intr_establish(pa->pc, pa->intrhandle,
			IPL_TTY, comintr, sc,
			sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(" %s", intrstr);

	sc->sc_iot = pa->t;
	sc->sc_ioh = pa->h;
	sc->sc_iobase = pa->a;
	sc->sc_frequency = COM_FREQ;

	if (pa->flags)
		sc->sc_frequency = pa->flags & PUC_COM_CLOCKMASK;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;

	com_attach_subr(sc);
}
