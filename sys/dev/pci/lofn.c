/*	$OpenBSD: lofn.c,v 1.10 2001/08/25 10:13:29 art Exp $	*/

/*
 * Copyright (c) 2001 Jason L. Wright (jason@thought.net)
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
 * Driver for the Hifn 6500 assymmetric encryption processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>

#include <vm/vm.h>

#include <crypto/cryptodev.h>
#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/lofnreg.h>
#include <dev/pci/lofnvar.h>

/*
 * Prototypes and count for the pci_device structure
 */
int lofn_probe		__P((struct device *, void *, void *));
void lofn_attach	__P((struct device *, struct device *, void *));

struct cfattach lofn_ca = {
	sizeof(struct lofn_softc), lofn_probe, lofn_attach,
};

struct cfdriver lofn_cd = {
	0, "lofn", DV_DULL
};

int lofn_intr	__P((void *));

void lofn_putnum __P((struct lofn_softc *, u_int32_t, u_int32_t,
    u_int32_t *, u_int32_t));
int lofn_getnum __P((struct lofn_softc *, u_int32_t, u_int32_t,
    u_int32_t *num, u_int32_t *numlen));

int
lofn_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_6500)
		return (1);
	return (0);
}

void 
lofn_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lofn_softc *sc = (struct lofn_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t iosize;
	u_int32_t cmd;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}

	if (pci_mapreg_map(pa, LOFN_BAR0, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &iosize, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, lofn_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}

	WRITE_REG_0(sc, LOFN_REL_RNC, LOFN_RNG_SCALAR);

	/* Enable RNG */
	WRITE_REG_0(sc, LOFN_REL_IER,
	    READ_REG_0(sc, LOFN_REL_IER) | LOFN_IER_RDY);
	WRITE_REG_0(sc, LOFN_REL_CFG2,
	    READ_REG_0(sc, LOFN_REL_CFG2) | LOFN_CFG2_RNGENA);

	printf(": %s\n", intrstr, sc->sc_sh);

	return;

fail:
	bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
}

int 
lofn_intr(vsc)
	void *vsc;
{
	struct lofn_softc *sc = vsc;
	u_int32_t sr;
	int r = 0, i;

	sr = READ_REG_0(sc, LOFN_REL_SR);

	if (sr & LOFN_SR_RNG_UF) {
		r = 1;
		printf("%s: rng underflow (disabling)\n", sc->sc_dv.dv_xname);
		WRITE_REG_0(sc, LOFN_REL_CFG2,
		    READ_REG_0(sc, LOFN_REL_CFG2) & (~LOFN_CFG2_RNGENA));
		WRITE_REG_0(sc, LOFN_REL_IER,
		    READ_REG_0(sc, LOFN_REL_IER) & (~LOFN_IER_RDY));
	} else if (sr & LOFN_SR_RNG_RDY) {
		r = 1;

		bus_space_read_region_4(sc->sc_st, sc->sc_sh, LOFN_REL_RNG,
		    sc->sc_rngbuf, LOFN_RNGBUF_SIZE);
		for (i = 0; i < LOFN_RNGBUF_SIZE; i++)
			add_true_randomness(sc->sc_rngbuf[i]);
	}

	return (r);
}

void
lofn_putnum(sc, win, reg, num, numlen)
	struct lofn_softc *sc;
	u_int32_t reg, win, *num, numlen;
{
	u_int32_t i, len;

	len = ((numlen >> 5) + 3) >> 2;
	for (i = 0; i < len; i++)
		WRITE_REG(sc, LOFN_REGADDR(win, reg, i), num[i]);
	WRITE_REG(sc, LOFN_LENADDR(win, reg), numlen);
}

int
lofn_getnum(sc, win, reg, num, numlen)
	struct lofn_softc *sc;
	u_int32_t win, reg, *num, *numlen;
{
	u_int32_t len, i;

	len = READ_REG(sc, LOFN_LENADDR(win, reg)) & LOFN_LENMASK;
	if (len > (*numlen))
		return (-1);
	(*numlen) = len;
	len = ((len >> 5) + 3) >> 2;
	for (i = 0; i < len; i++)
		num[i] = READ_REG(sc, LOFN_REGADDR(win, reg, i));
	return (0);
}
