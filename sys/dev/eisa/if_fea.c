/*	$OpenBSD: if_fea.c,v 1.19 2009/08/13 14:24:46 jasper Exp $	*/
/*	$NetBSD: if_fea.c,v 1.9 1996/10/21 22:31:05 thorpej Exp $	*/

/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * Id: if_fea.c,v 1.6 1996/06/07 20:02:25 thomas Exp
 */

/*
 * DEC PDQ FDDI Controller
 *
 *	This module support the DEFEA EISA FDDI Controller.
 */


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_fddi.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <dev/ic/pdqvar.h>
#include <dev/ic/pdqreg.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

/*
 *
 */

void pdq_eisa_subprobe(bus_space_tag_t, bus_space_handle_t,
    u_int32_t *, u_int32_t *, u_int32_t *);
void pdq_eisa_devinit(pdq_softc_t *);
int pdq_eisa_match(struct device *, void *, void *);
void pdq_eisa_attach(struct device *, struct device *, void *);

#define	DEFEA_INTRENABLE		0x8	/* level interrupt */
static int pdq_eisa_irqs[4] = { 9, 10, 11, 15 };

void
pdq_eisa_subprobe(bc, iobase, maddr, msize, irq)
	bus_space_tag_t bc;
	bus_space_handle_t iobase;
	u_int32_t *maddr;
	u_int32_t *msize;
	u_int32_t *irq;
{
	if (irq != NULL)
		*irq = pdq_eisa_irqs[PDQ_OS_IORD_8(bc, iobase,
		    PDQ_EISA_IO_CONFIG_STAT_0) & 3];

	*maddr = (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_CMP_0) << 8)
	    | (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_CMP_1) << 16);
	*msize = (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_MASK_0) + 4) << 8;
}

void
pdq_eisa_devinit(sc)
	pdq_softc_t *sc;
{
	u_int8_t data;
	bus_space_tag_t tag;

	tag = sc->sc_bc;

	/*
	 * Do the standard initialization for the DEFEA registers.
	 */
	PDQ_OS_IOWR_8(tag, sc->sc_iobase, PDQ_EISA_FUNCTION_CTRL, 0x23);
	PDQ_OS_IOWR_8(tag, sc->sc_iobase, PDQ_EISA_IO_CMP_1_1,
	    (sc->sc_iobase >> 8) & 0xF0);
	PDQ_OS_IOWR_8(tag, sc->sc_iobase, PDQ_EISA_IO_CMP_0_1,
	    (sc->sc_iobase >> 8) & 0xF0);
	PDQ_OS_IOWR_8(tag, sc->sc_iobase, PDQ_EISA_SLOT_CTRL, 0x01);
	data = PDQ_OS_IORD_8(tag, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF);
#if defined(PDQ_IOMAPPED)
	PDQ_OS_IOWR_8(tag, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF, data & ~1);
#else
	PDQ_OS_IOWR_8(tag, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF, data | 1);
#endif
	data = PDQ_OS_IORD_8(tag, sc->sc_iobase, PDQ_EISA_IO_CONFIG_STAT_0);
	PDQ_OS_IOWR_8(tag, sc->sc_iobase, PDQ_EISA_IO_CONFIG_STAT_0,
	    data | DEFEA_INTRENABLE);
}

int
pdq_eisa_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct eisa_attach_args *ea = (struct eisa_attach_args *) aux;

	if (strncmp(ea->ea_idstring, "DEC300", 6) == 0)
		return (1);
	return (0);
}

void
pdq_eisa_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	pdq_softc_t *sc = (pdq_softc_t *) self;
	struct eisa_attach_args *ea = (struct eisa_attach_args *) aux;
	u_int32_t irq, maddr, msize;
	eisa_intr_handle_t ih;
	const char *intrstr;

	sc->sc_iotag = ea->ea_iot;
	bcopy(sc->sc_dev.dv_xname, sc->sc_if.if_xname, IFNAMSIZ);
	sc->sc_if.if_flags = 0;
	sc->sc_if.if_softc = sc;

	/*
	 * NOTE: sc_bc is an alias for sc_csrtag and sc_membase is
	 * an alias for sc_csrhandle.  sc_iobase is used here to
	 * check the card's configuration.
	 */

	if (bus_space_map(sc->sc_iotag, EISA_SLOT_ADDR(ea->ea_slot),
	    EISA_SLOT_SIZE, 0, &sc->sc_iobase)) {
		printf("\n%s: failed to map I/O!\n", sc->sc_dev.dv_xname);
		return;
	}

	pdq_eisa_subprobe(sc->sc_iotag, sc->sc_iobase, &maddr, &msize, &irq);

#if defined(PDQ_IOMAPPED)
	sc->sc_csrtag = sc->sc_iotag;
	sc->sc_csrhandle = sc->sc_iobase;
#else
	if (maddr == 0 || msize == 0) {
		printf("\n%s: error: memory not enabled! ECU reconfiguration"
		    " required\n", sc->sc_dev.dv_xname);
		return;
	}

	if (bus_space_map(sc->sc_csrtag, maddr, msize, 0, &sc->sc_csrhandle)) {
		bus_space_unmap(sc->sc_iotag, sc->sc_iobase, EISA_SLOT_SIZE);
		printf("\n%s: can't map mem space (0x%x-0x%x)!\n",
		    sc->sc_dev.dv_xname, maddr, maddr + msize - 1);
		return;
	}
#endif
	pdq_eisa_devinit(sc);
	sc->sc_pdq = pdq_initialize(sc->sc_bc, sc->sc_membase,
	    sc->sc_if.if_xname, 0, (void *) sc, PDQ_DEFEA);
	if (sc->sc_pdq == NULL) {
		printf("%s: initialization failed\n", sc->sc_dev.dv_xname);
		return;
	}

	if (eisa_intr_map(ea->ea_ec, irq, &ih)) {
		printf("%s: can't map interrupt (%d)\n",
		    sc->sc_dev.dv_xname, irq);
		return;
	}
	intrstr = eisa_intr_string(ea->ea_ec, ih);
	sc->sc_ih = eisa_intr_establish(ea->ea_ec, ih, IST_LEVEL, IPL_NET,
	    (int (*)(void *)) pdq_interrupt, sc->sc_pdq, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: can't establish interrupt", sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf(": interrupting at %s\n", intrstr);

	bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes,
	    sc->sc_arpcom.ac_enaddr, 6);

	pdq_ifattach(sc, NULL);

	sc->sc_ats = shutdownhook_establish((void (*)(void *)) pdq_hwreset,
	    sc->sc_pdq);
	if (sc->sc_ats == NULL)
		printf("%s: warning: can't establish shutdown hook\n",
		    self->dv_xname);
#if !defined(PDQ_IOMAPPED)
	printf("%s: using iomem 0x%x-0x%x\n", sc->sc_dev.dv_xname, maddr,
	    maddr + msize - 1);
#endif
}

struct cfattach fea_ca = {
	sizeof(pdq_softc_t), pdq_eisa_match, pdq_eisa_attach
};

struct cfdriver fea_cd = {
	NULL, "fea", DV_IFNET
};
