/*	$OpenBSD: nofn.c,v 1.1 2002/01/07 23:16:38 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Driver for the Hifn 7811 encryption processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <crypto/cryptodev.h>
#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/nofnreg.h>
#include <dev/pci/nofnvar.h>

/*
 * Prototypes and count for the pci_device structure
 */
int nofn_probe		__P((struct device *, void *, void *));
void nofn_attach	__P((struct device *, struct device *, void *));

struct cfattach nofn_ca = {
	sizeof(struct nofn_softc), nofn_probe, nofn_attach,
};

struct cfdriver nofn_cd = {
	0, "nofn", DV_DULL
};

int	nofn_intr __P((void *));
void	nofn_rng __P((void *));

int
nofn_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_7811)
		return (1);
	return (0);
}

void 
nofn_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct nofn_softc *sc = (struct nofn_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t iosize0, iosize1, iosize2;
	u_int32_t cmd;

	sc->sc_pci_pc = pa->pa_pc;
	sc->sc_pci_tag = pa->pa_tag;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		goto fail;
	}

	if (!(cmd & PCI_COMMAND_MASTER_ENABLE)) {
		printf(": failed to enable bus mastering\n");
		goto fail;
	}

	if (pci_mapreg_map(pa, NOFN_BAR0, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st0, &sc->sc_sh0, NULL, &iosize0, 0)) {
		printf(": can't find mem space %d\n", 0);
		goto fail_io0;
	}

	if (pci_mapreg_map(pa, NOFN_BAR1, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st1, &sc->sc_sh1, NULL, &iosize1, 0)) {
		printf(": can't find mem space %d\n", 1);
		goto fail_io1;
	}

	if (pci_mapreg_map(pa, NOFN_BAR2, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st2, &sc->sc_sh2, NULL, &iosize2, 0)) {
		printf(": can't find mem space %d\n", 1);
		goto fail_io2;
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail_intr;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, nofn_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail_intr1;
	}

	printf(": %s\n", intrstr);

	/* Setup RNG */
	G1_WRITE_4(sc, NOFN_G1_RNGCR, RNGCR_DEFAULT);
	G1_WRITE_4(sc, NOFN_G1_RNGER,
	    G1_READ_4(sc, NOFN_G1_RNGER) | RNGER_ENABLE);
	timeout_set(&sc->sc_rngto, nofn_rng, sc);
	if (hz >= 100)
		sc->sc_rnghz = hz / 100;
	else
		sc->sc_rnghz = 1;
	timeout_add(&sc->sc_rngto, sc->sc_rnghz);

	return;

fail_intr1:
	pci_intr_disestablish(pc, sc->sc_ih);
fail_intr:
	bus_space_unmap(sc->sc_st2, sc->sc_sh2, iosize2);
fail_io2:
	bus_space_unmap(sc->sc_st1, sc->sc_sh1, iosize1);
fail_io1:
	bus_space_unmap(sc->sc_st0, sc->sc_sh0, iosize0);
fail_io0:
fail:
	return;
}

int 
nofn_intr(vsc)
	void *vsc;
{
	return (0);
}

void
nofn_rng(vsc)
	void *vsc;
{
	struct nofn_softc *sc = vsc;
	u_int32_t r, v;

	while (1) {
		r = G1_READ_4(sc, NOFN_G1_RNGSTS);
		if (r & RNGSTS_UFL)
			printf("%s: rng underflow\n", sc->sc_dv.dv_xname);
		if ((r & RNGSTS_RDY) == 0)
			break;

		v = G1_READ_4(sc, NOFN_G1_RNGDAT);
		add_true_randomness(v);
		v = G1_READ_4(sc, NOFN_G1_RNGDAT);
		add_true_randomness(v);
	}
	timeout_add(&sc->sc_rngto, sc->sc_rnghz);
}
