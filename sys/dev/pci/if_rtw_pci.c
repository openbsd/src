/*	$OpenBSD: if_rtw_pci.c,v 1.2 2004/12/31 00:16:15 jsg Exp $	*/
/*	$NetBSD: if_rtw_pci.c,v 1.1 2004/09/26 02:33:36 dyoung Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center; Charles M. Hannum; and David Young.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*
 * PCI bus front-end for the Realtek RTL8180 802.11 MAC/BBP chip.
 *
 * Derived from the ADMtek ADM8211 PCI bus front-end.
 *
 * Derived from the ``Tulip'' PCI bus front-end.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_var.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/rtwreg.h>
#include <dev/ic/sa2400reg.h>
#include <dev/ic/rtwvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

const struct rtw_pci_product * rtw_pci_lookup(const struct pci_attach_args *);
int rtw_pci_enable(struct rtw_softc *);
void rtw_pci_disable(struct rtw_softc *);

/*
 * PCI configuration space registers used by the ADM8211.
 */
#define	RTW_PCI_IOBA		0x10	/* i/o mapped base */
#define	RTW_PCI_MMBA		0x14	/* memory mapped base */

struct rtw_pci_softc {
	struct rtw_softc	psc_rtw;	/* real ADM8211 softc */

	pci_intr_handle_t	psc_ih;		/* interrupt handle */
	void			*psc_intrcookie;

	pci_chipset_tag_t	psc_pc;		/* our PCI chipset */
	pcitag_t		psc_pcitag;	/* our PCI tag */
};

int	rtw_pci_match(struct device *, void *, void *);
void	rtw_pci_attach(struct device *, struct device *, void *);

struct cfattach rtw_pci_ca = {
	sizeof (struct rtw_pci_softc), rtw_pci_match, rtw_pci_attach
};

const struct rtw_pci_product {
	u_int32_t	app_vendor;	/* PCI vendor ID */
	u_int32_t	app_product;	/* PCI product ID */
} rtw_pci_products[] = {
	{ PCI_VENDOR_REALTEK,		PCI_PRODUCT_REALTEK_RT8180 },
	{ PCI_VENDOR_BELKIN2,		PCI_PRODUCT_BELKIN2_F5D6001 },

	{ 0,				0 },
};

const struct rtw_pci_product *
rtw_pci_lookup(const struct pci_attach_args *pa)
{
	const struct rtw_pci_product *app;

	for (app = rtw_pci_products;
	     app->app_vendor != 0 && app->app_product != 0;
	     app++) {
		if (PCI_VENDOR(pa->pa_id) == app->app_vendor &&
		    PCI_PRODUCT(pa->pa_id) == app->app_product)
			return (app);
	}
	return (NULL);
}

int
rtw_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (rtw_pci_lookup(pa) != NULL)
		return (1);

	return (0);
}

int
rtw_pci_enable(struct rtw_softc *sc)
{
	struct rtw_pci_softc *psc = (void *)sc;

	/* Establish the interrupt. */
	psc->psc_intrcookie = pci_intr_establish(psc->psc_pc, psc->psc_ih,
	    IPL_NET, rtw_intr, sc, sc->sc_dev.dv_xname);
	if (psc->psc_intrcookie == NULL) {
		printf("%s: unable to establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	return (0);
}

void
rtw_pci_disable(struct rtw_softc *sc)
{
	struct rtw_pci_softc *psc = (void *)sc;

	/* Unhook the interrupt handler. */
	pci_intr_disestablish(psc->psc_pc, psc->psc_intrcookie);
	psc->psc_intrcookie = NULL;
}

void
rtw_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct rtw_pci_softc *psc = (void *) self;
	struct rtw_softc *sc = &psc->psc_rtw;
	struct rtw_regs *regs = &sc->sc_regs;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	int ioh_valid, memh_valid;
	const struct rtw_pci_product *app;
	pcireg_t reg;
	int pmreg;

	psc->psc_pc = pa->pa_pc;
	psc->psc_pcitag = pa->pa_tag;

	app = rtw_pci_lookup(pa);
	if (app == NULL) {
		printf("\n");
		panic("rtw_pci_attach: impossible");
	}

	/*
	 * No power management hooks.
	 * XXX Maybe we should add some!
	 */
	sc->sc_flags |= RTW_F_ENABLED;

	/*
	 * Get revision info, and set some chip-specific variables.
	 */
	sc->sc_rev = PCI_REVISION(pa->pa_class);

	/*
	 * Check to see if the device is in power-save mode, and
	 * being it out if necessary.
	 *
	 * XXX This code comes almost verbatim from if_tlp_pci.c. I do
	 * not understand it. Tulip clears the "sleep mode" bit in the
	 * CFDA register, first.  There is an equivalent (?) register at the
	 * same place in the ADM8211, but the docs do not assign its bits
	 * any meanings. -dcy
	 */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, 0)) {
		reg = pci_conf_read(pc, pa->pa_tag, pmreg + PCI_PMCSR);
		switch (reg & PCI_PMCSR_STATE_MASK) {
		case PCI_PMCSR_STATE_D1:
		case PCI_PMCSR_STATE_D2:
			printf(": waking up from power state D%d\n",
			    reg & PCI_PMCSR_STATE_MASK);
			pci_conf_write(pc, pa->pa_tag, pmreg + PCI_PMCSR,
			    (reg & ~PCI_PMCSR_STATE_MASK) |
			    PCI_PMCSR_STATE_D0);
			break;
		case PCI_PMCSR_STATE_D3:
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			printf(": unable to wake up from power state D3, "
			       "reboot required.\n");
			pci_conf_write(pc, pa->pa_tag, pmreg + PCI_PMCSR,
			    (reg & ~PCI_PMCSR_STATE_MASK) |
			    PCI_PMCSR_STATE_D0);
			return;
		}
	}

	/*
	 * Map the device.
	 */
	ioh_valid = (pci_mapreg_map(pa, RTW_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL, 0) == 0);
	memh_valid = (pci_mapreg_map(pa, RTW_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, NULL, 0) == 0);

	if (memh_valid) {
		regs->r_bt = memt;
		regs->r_bh = memh;
	} else if (ioh_valid) {
		regs->r_bt = iot;
		regs->r_bh = ioh;
	} else {
		printf(": unable to map device registers\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Make sure bus mastering is enabled.
	 */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &psc->psc_ih)) {
		printf(": unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, psc->psc_ih); 
	psc->psc_intrcookie = pci_intr_establish(pc, psc->psc_ih, IPL_NET,
	    rtw_intr, sc, sc->sc_dev.dv_xname);
	if (psc->psc_intrcookie == NULL) {
		printf(": unable to establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	printf(": %s\n", intrstr);

	sc->sc_enable = rtw_pci_enable;
	sc->sc_disable = rtw_pci_disable;

	/*
	 * Finish off the attach.
	 */
	rtw_attach(sc);
}
