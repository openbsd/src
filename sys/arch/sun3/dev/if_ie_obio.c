/*	$NetBSD: if_ie_obio.c,v 1.6 1996/11/20 18:56:51 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-dependent glue for the Intel Ethernet (ie) driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/dvma.h>
#include <machine/isr.h>
#include <machine/obio.h>
#include <machine/idprom.h>
#include <machine/vmparam.h>

#include "i82586.h"
#include "if_iereg.h"
#include "if_ievar.h"

static void ie_obreset __P((struct ie_softc *));
static void ie_obattend __P((struct ie_softc *));
static void ie_obrun __P((struct ie_softc *));

/*
 * New-style autoconfig attachment
 */

static int  ie_obio_match __P((struct device *, void *, void *));
static void ie_obio_attach __P((struct device *, struct device *, void *));

struct cfattach ie_obio_ca = {
	sizeof(struct ie_softc), ie_obio_match, ie_obio_attach
};


static int
ie_obio_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;

	/* Make sure there is something there... */
	if (bus_peek(ca->ca_bustype, ca->ca_paddr, 1) == -1)
		return (0);

	/* Default interrupt priority. */
	if (ca->ca_intpri == -1)
		ca->ca_intpri = 3;

	return (1);
}

void
ie_obio_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct ie_softc *sc = (void *) self;
	struct cfdata *cf = self->dv_cfdata;
	struct confargs *ca = args;

	sc->hard_type = IE_OBIO;
	sc->reset_586 = ie_obreset;
	sc->chan_attn = ie_obattend;
	sc->run_586 = ie_obrun;
	sc->sc_bcopy = bcopy;
	sc->sc_bzero = bzero;

	/*
	 * The on-board "ie" just uses main memory, so
	 * we can choose how much memory to give it.
	 * XXX: Would like to use less than 64K...
	 */
	sc->sc_msize = 0x8000; /* MEMSIZE 32K */

	/* Map in the control registers. */
	sc->sc_reg = obio_alloc(ca->ca_paddr, OBIO_INTEL_ETHER_SIZE);

	/* Allocate "shared" memory (in DVMA space). */
	sc->sc_maddr = dvma_malloc(sc->sc_msize);
	if (sc->sc_maddr == NULL)
		panic(": not enough dvma space");

	/*
	 * The on-board "ie" is wired-up such that its
	 * memory access goes to the high 16 megabytes
	 * of the on-board memory space (on-board DVMA).
	 */
	sc->sc_iobase = (caddr_t)DVMA_OBIO_SLAVE_BASE;

	/*
	 * Set the System Configuration Pointer (SCP).
	 * Its location is system-dependent because the
	 * i82586 reads it from a fixed physical address.
	 * On this hardware, the i82586 address maps to
	 * a 24-bit offset in on-board DVMA space. The
	 * SCP happens to fall in a page used by the
	 * PROM monitor, which the PROM knows about.
	 */
	sc->scp = (volatile void *) (sc->sc_iobase + IE_SCP_ADDR);

	/*
	 * The rest of ram is used for buffers.
	 */
	sc->buf_area    = sc->sc_maddr;
	sc->buf_area_sz = sc->sc_msize;

	/* Set the ethernet address. */
	idprom_etheraddr(sc->sc_addr);

	/* Do machine-independent parts of attach. */
	ie_attach(sc);

	/* Install interrupt handler. */
	isr_add_autovect(ie_intr, (void *)sc, ca->ca_intpri);
}


/*
 * onboard ie support
 */
void
ie_obreset(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;
	ieo->obctrl = 0;
	delay(100);			/* XXX could be shorter? */
	ieo->obctrl = IEOB_NORSET;
}
void
ie_obattend(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;

	ieo->obctrl |= IEOB_ATTEN;	/* flag! */
	ieo->obctrl &= ~IEOB_ATTEN;	/* down. */
}

void
ie_obrun(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;

	ieo->obctrl |= (IEOB_ONAIR|IEOB_IENAB|IEOB_NORSET);
}

