/*	$OpenBSD: fwlynx_pci.c,v 1.5 2004/10/01 04:08:46 jsg Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fwlynx_pci.c,v 1.3 2001/11/15 09:48:12 lukem Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ieee1394/ieee1394reg.h>
#include <dev/ieee1394/ieee1394var.h>
#include <dev/ieee1394/fwlynxreg.h>
#include <dev/ieee1394/fwlynxvar.h>

struct fwlynx_pci_softc {
	struct fwlynx_softc psc_sc;
	pci_chipset_tag_t psc_pc;
	void *psc_ih;
};

static int fwlynx_pci_match(struct device *, struct cfdata *, void *);
static void fwlynx_pci_attach(struct device *, struct device *, void *);

struct cfattach fwlynx_pci_ca = {
	sizeof(struct fwlynx_pci_softc), fwlynx_pci_match, fwlynx_pci_attach,
#if 0
	fwlynx_pci_detach, fwlynx_activate
#endif
};

static int
fwlynx_pci_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_TI &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_TI_TSB12LV21)
		return 1;

	return 0;
}

static void
fwlynx_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	struct fwlynx_pci_softc *psc = (struct fwlynx_pci_softc *) self;
	char const *intrstr;
	pci_intr_handle_t ih;
	u_int32_t csr;

#ifdef __NetBSD__
	char devinfo[256];
	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));
#endif

	psc->psc_sc.sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;
	psc->psc_tag = pa->pa_tag;

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_LYNX_MAP_REGISTER, PCI_MAPREG_TYPE_MEM, 0,
	    &psc->psc_sc.sc_memt, &psc->psc_sc.sc_memh,
			   NULL, &psc->psc_sc.sc_memsize)) {
		printf(": can't map register space\n");
		return;
	}

	/* Disable interrupts, so we don't get any spurious ones. */
	pci_conf_write(psc->psc_pc, psc->psc_tag, 0);

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(psc->psc_sc.sc_memt, psc->psc_sc.sc_memh,
		    psc->psc_sc.sc_memsize);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	psc->psc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, fwlynx_intr, &psc->psc_sc);
	if (psc->psc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(psc->psc_sc.sc_memt, psc->psc_sc.sc_memh,
		    psc->psc_sc.sc_memsize);
		return;
	}
	printf(": %s\n", intrstr);

	if (fwlynx_init(&psc->psc_sc, pci_intr_evcnt(pa->pa_pc, ih)) != 0) {
		pci_intr_disestablish(pa->pa_pc, psc->psc_ih);
		bus_space_unmap(psc->psc_sc.sc_memt, psc->psc_sc.sc_memh,
		    psc->psc_sc.sc_memsize);
	}
}
