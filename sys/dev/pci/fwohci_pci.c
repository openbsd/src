/*	$OpenBSD: fwohci_pci.c,v 1.4 2003/01/09 15:28:53 mickey Exp $	*/
/*	$NetBSD: fwohci_pci.c,v 1.13 2002/01/26 16:30:00 ichiro Exp $	*/

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
#ifdef __KERNEL_RCSID
__KERNEL_RCSID(0, "$NetBSD: fwohci_pci.c,v 1.13 2002/01/26 16:30:00 ichiro Exp $");
#endif

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
#include <dev/ieee1394/fwohcireg.h>
#include <dev/ieee1394/fwohcivar.h>

struct fwohci_pci_softc {
	struct fwohci_softc psc_sc;
	pci_chipset_tag_t psc_pc;
	void *psc_ih;
};

#ifdef __NetBSD__
int fwohci_pci_match(struct device *, struct cfdata *, void *);
#else
int fwohci_pci_match(struct device *, void *, void *);
#endif
void fwohci_pci_attach(struct device *, struct device *, void *);

struct cfattach fwohci_pci_ca = {
	sizeof(struct fwohci_pci_softc), fwohci_pci_match, fwohci_pci_attach,
#if 0
	fwohci_pci_detach, fwohci_activate
#endif
};

#ifdef __NetBSD__
int
fwohci_pci_match(struct device *parent, struct cfdata *match, void *aux)
#else
int
fwohci_pci_match(struct device *parent, void *match, void *aux)
#endif
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_FIREWIRE &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_OHCI)
		return (1);
 
	return (0);
}

void
fwohci_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	struct fwohci_pci_softc *psc = (struct fwohci_pci_softc *) self;
	char const *intrstr;
	pci_intr_handle_t ih;
	u_int32_t csr;

	psc->psc_sc.sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;

	/* Map I/O registers */
#ifdef __NetBSD__
	if (pci_mapreg_map(pa, PCI_OHCI_MAP_REGISTER, PCI_MAPREG_TYPE_MEM, 0,
	    &psc->psc_sc.sc_memt, &psc->psc_sc.sc_memh,
	    NULL, &psc->psc_sc.sc_memsize))
#else
	if (pci_mapreg_map(pa, PCI_OHCI_MAP_REGISTER, PCI_MAPREG_TYPE_MEM, 0,
	    &psc->psc_sc.sc_memt, &psc->psc_sc.sc_memh,
	    NULL, &psc->psc_sc.sc_memsize, 0))
#endif
	{
		printf(": can't map OHCI register space\n");
		return;
	}

	/* Disable interrupts, so we don't get any spurious ones. */
	OHCI_CSR_WRITE(&psc->psc_sc, OHCI_REG_IntMaskClear,
	    OHCI_Int_MasterEnable);

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

#if BYTE_ORDER == BIG_ENDIAN
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_OHCI_CONTROL_REGISTER);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_OHCI_CONTROL_REGISTER,
	    csr | PCI_GLOBAL_SWAP_BE);
#endif

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(psc->psc_sc.sc_memt, psc->psc_sc.sc_memh,
		    psc->psc_sc.sc_memsize);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
#ifdef __NetBSD__
	psc->psc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, fwohci_intr,
	    &psc->psc_sc);
#else
	psc->psc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, fwohci_intr,
	    &psc->psc_sc, self->dv_xname);
#endif
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

#ifdef __NetBSD__
	if (fwohci_init(&psc->psc_sc, pci_intr_evcnt(pa->pa_pc, ih)) != 0)
#else
	if (fwohci_init(&psc->psc_sc, NULL) != 0)
#endif
	{
		pci_intr_disestablish(pa->pa_pc, psc->psc_ih);
		bus_space_unmap(psc->psc_sc.sc_memt, psc->psc_sc.sc_memh,
		    psc->psc_sc.sc_memsize);
	}
}
