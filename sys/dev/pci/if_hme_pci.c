/*	$OpenBSD: if_hme_pci.c,v 1.4 2001/12/14 01:25:29 drahn Exp $	*/
/*	$NetBSD: if_hme_pci.c,v 1.3 2000/12/28 22:59:13 sommerfeld Exp $	*/

/*
 * Copyright (c) 2000 Matthew R. Green
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PCI front-end device driver for the HME ethernet device.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#ifdef __sparc64__
#include <machine/autoconf.h>
#endif
#include <machine/cpu.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/hmevar.h>

struct hme_pci_softc {
	struct	hme_softc	hsc_hme;	/* HME device */
	bus_space_tag_t		hsc_memt;
	bus_space_handle_t	hsc_memh;
	void			*hsc_ih;
};

int	hmematch_pci __P((struct device *, void *, void *));
void	hmeattach_pci __P((struct device *, struct device *, void *));

struct cfattach hme_pci_ca = {
	sizeof(struct hme_pci_softc), hmematch_pci, hmeattach_pci
};

int
hmematch_pci(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN && 
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_HME)
		return (1);

	return (0);
}

void
hmeattach_pci(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct hme_pci_softc *hsc = (void *)self;
	struct hme_softc *sc = &hsc->hsc_hme;
	pci_intr_handle_t intrhandle;
	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr __P((u_char *));
	pcireg_t csr;
	const char *intrstr;
	int type;

	/*
	 * enable io/memory-space accesses.  this is kinda of gross; but
	 * the hme comes up with neither IO space enabled, or memory space.
	 */
	if (pa->pa_memt)
		pa->pa_flags |= PCI_FLAGS_MEM_ENABLED;
	if (pa->pa_iot)
		pa->pa_flags |= PCI_FLAGS_IO_ENABLED;
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	if (pa->pa_memt) {
		type = PCI_MAPREG_TYPE_MEM;
		csr |= PCI_COMMAND_MEM_ENABLE;
		sc->sc_bustag = pa->pa_memt;
	} else {
		type = PCI_MAPREG_TYPE_IO;
		csr |= PCI_COMMAND_IO_ENABLE;
		sc->sc_bustag = pa->pa_iot;
	}
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MEM_ENABLE);

	sc->sc_dmatag = pa->pa_dmat;

	sc->sc_pci = 1; /* XXXXX should all be done in bus_dma. */
	/*
	 * Map five register banks:
	 *
	 *	bank 0: HME SEB registers:	+0x0000
	 *	bank 1: HME ETX registers:	+0x2000
	 *	bank 2: HME ERX registers:	+0x4000
	 *	bank 3: HME MAC registers:	+0x6000
	 *	bank 4: HME MIF registers:	+0x7000
	 *
	 */

#define PCI_HME_BASEADDR	0x10
	if (pci_mapreg_map(pa, PCI_HME_BASEADDR, type, 0,
	    &hsc->hsc_memt, &hsc->hsc_memh, NULL, NULL, 0) != 0)
	{
		printf(": could not map hme registers\n");
		return;
	}
	sc->sc_seb = hsc->hsc_memh;
	sc->sc_etx = hsc->hsc_memh + 0x2000;
	sc->sc_erx = hsc->hsc_memh + 0x4000;
	sc->sc_mac = hsc->hsc_memh + 0x6000;
	sc->sc_mif = hsc->hsc_memh + 0x7000;

#ifdef __sparc__
        myetheraddr(sc->sc_enaddr);
#endif
#ifdef __powerpc__
	pci_ether_hw_addr(pa->pa_pc, sc->sc_enaddr);
#endif


	sc->sc_burst = 16;	/* XXX */

	/*
	 * call the main configure
	 */
	hme_config(sc);

	if (pci_intr_map(pa, &intrhandle) != 0) {
		printf("%s: couldn't map interrupt\n",
		    sc->sc_dev.dv_xname);
		return;	/* bus_unmap ? */
	}	
	intrstr = pci_intr_string(pa->pa_pc, intrhandle);
	hsc->hsc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
	    hme_intr, sc, self->dv_xname);
	if (hsc->hsc_ih != NULL) {
		printf("%s: using %s for interrupt\n",
		    sc->sc_dev.dv_xname,
		    intrstr ? intrstr : "unknown interrupt");
	} else {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;	/* bus_unmap ? */
	}
}
