/*	$OpenBSD: if_fpa.c,v 1.6 1996/05/10 12:41:26 deraadt Exp $	*/
/*	$NetBSD: if_fpa.c,v 1.8 1996/05/07 02:17:23 thorpej Exp $	*/

/*-
 * Copyright (c) 1995 Matt Thomas (thomas@lkg.dec.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 * Id: if_fpa.c,v 1.2 1995/08/20 18:56:11 thomas Exp
 *
 * Log: if_fpa.c,v
 * Revision 1.2  1995/08/20  18:56:11  thomas
 * Misc. changes for NetBSD
 *
 * Revision 1.1  1995/08/16  22:57:28  thomas
 * Initial revision
 *
 * Revision 1.13  1995/08/04  21:54:56  thomas
 * Clean IRQ processing under BSD/OS.
 * A receive tweaks.  (print source of MAC CRC errors, etc.)
 *
 * Revision 1.12  1995/06/02  16:04:22  thomas
 * Use correct PCI defs for BSDI now that they have fixed them.
 * Increment the slot number 0x1000, not one! (*duh*)
 *
 * Revision 1.11  1995/04/21  13:23:55  thomas
 * Fix a few pub in the DEFPA BSDI support
 *
 * Revision 1.10  1995/04/20  21:46:42  thomas
 * Why???
 * ,
 *
 * Revision 1.9  1995/04/20  20:17:33  thomas
 * Add PCI support for BSD/OS.
 * Fix BSD/OS EISA support.
 * Set latency timer for DEFPA to recommended value if 0.
 *
 * Revision 1.8  1995/04/04  22:54:29  thomas
 * Fix DEFEA support
 *
 * Revision 1.7  1995/03/14  01:52:52  thomas
 * Update for new FreeBSD PCI Interrupt interface
 *
 * Revision 1.6  1995/03/10  17:06:59  thomas
 * Update for latest version of FreeBSD.
 * Compensate for the fast that the ifp will not be first thing
 * in softc on BSDI.
 *
 * Revision 1.5  1995/03/07  19:59:42  thomas
 * First pass at BSDI EISA support
 *
 * Revision 1.4  1995/03/06  17:06:03  thomas
 * Add transmit timeout support.
 * Add support DEFEA (untested).
 *
 * Revision 1.3  1995/03/03  13:48:35  thomas
 * more fixes
 *
 *
 */

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 * Written by Matt Thomas
 *
 *   This module supports the DEC DEFPA PCI FDDI Controller
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#if defined(__FreeBSD__)
#include <sys/devconf.h>
#elif defined(__bsdi__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif
#if defined(__FreeBSD__)
#include <netinet/if_fddi.h>
#else
#include <net/if_fddi.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>

#if defined(__FreeBSD__)
#include "fpa.h"
#include <pci/pcivar.h>
#include <i386/isa/icu.h>
#include <pci/pdqreg.h>
#include <pci/pdq_os.h>
#elif defined(__bsdi__)
#include <i386/pci/pci.h>
#include <i386/pci/pdqreg.h>
#include <i386/pci/pdq_os.h>
#elif defined(__NetBSD__) || defined (__OpenBSD__)
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/pdqreg.h>
#include <dev/ic/pdqvar.h>
#endif /* __NetBSD__ || __OpenBSD */


#define	DEC_VENDORID		0x1011
#define	DEFPA_CHIPID		0x000F
#define	PCI_VENDORID(x)		((x) & 0xFFFF)
#define	PCI_CHIPID(x)		(((x) >> 16) & 0xFFFF)

#define	DEFPA_LATENCY	0x88

#define	PCI_CFLT	0x0C	/* Configuration Latency */
#define	PCI_CBMA	0x10	/* Configuration Base Memory Address */

#if defined(__FreeBSD__)
/*
 * This is the PCI configuration support.  Since the PDQ is available
 * on both EISA and PCI boards, one must be careful in how defines the
 * PDQ in the config file.
 */
static char *pdq_pci_probe (pcici_t config_id, pcidi_t device_id);
static void pdq_pci_attach(pcici_t config_id, int unit);
static int pdq_pci_shutdown(struct kern_devconf *, int);
static u_long pdq_pci_count;

struct pci_device fpadevice = {
    "fpa",
    pdq_pci_probe,
    pdq_pci_attach,
    &pdq_pci_count,
    pdq_pci_shutdown,
};

#ifdef DATA_SET
DATA_SET (pcidevice_set, fpadevice);
#endif
static pdq_softc_t *pdqs_pci[NFPA];
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	(pdqs_pci[unit])
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
extern struct cfdriver fpacd;
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)fpacd.cd_devs[unit])
#endif

#if defined(__NetBSD__) || defined (__OpenBSD__)
extern struct cfattach fpa_ca;
extern struct cfdriver fpa_cd;
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)fpa_cd.cd_devs[unit])
#endif

#if defined(__NetBSD__)
static ifnet_ret_t
pdq_pci_ifinit(
    struct ifnet *ifp)
{
    pdq_ifinit((pdq_softc_t *)(ifp->if_softc));
}

static ifnet_ret_t
pdq_pci_ifwatchdog(
    struct ifnet *ifp)
{
    pdq_ifwatchdog((pdq_softc_t *)(ifp->if_softc));
}
#else
static ifnet_ret_t 
pdq_pci_ifinit(
    int unit) 
{
    pdq_ifinit(PDQ_PCI_UNIT_TO_SOFTC(unit));
}

static ifnet_ret_t
pdq_pci_ifwatchdog(
    int unit)
{
    pdq_ifwatchdog(PDQ_PCI_UNIT_TO_SOFTC(unit));
}
#endif

static int
pdq_pci_ifintr(
    void *arg)
{
    pdq_softc_t * const sc = (pdq_softc_t *) arg;
#ifdef __FreeBSD__
    return pdq_interrupt(sc->sc_pdq);
#elif defined(__bsdi__) || defined(__NetBSD__) || defined(__OpenBSD__)
    (void) pdq_interrupt(sc->sc_pdq);
    return 1;
#endif
}

#if defined(__FreeBSD__)
static char *
pdq_pci_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
    if (PCI_VENDORID(device_id) == DEC_VENDORID &&
	    PCI_CHIPID(device_id) == DEFPA_CHIPID)
	return "Digital DEFPA PCI FDDI Controller";
    return NULL;
}

static void
pdq_pci_attach(
    pcici_t config_id,
    int unit)
{
    pdq_softc_t *sc;
    vm_offset_t va_csrs, pa_csrs;
    pdq_uint32_t data;

    if (unit > NFPA) {
	printf("fpa%d: not configured; kernel is built for only %d device%s.\n",
	       unit, NFPA, NFPA == 1 ? "" : "s");
	return;
    }

    data = pci_conf_read(config_id, PCI_CFLT);
    if ((data & 0xFF00) == 0) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_conf_write(config_id, PCI_CFLT, data);
    }

    sc = (pdq_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
    if (sc == NULL)
	return;

    bzero(sc, sizeof(pdq_softc_t));	/* Zero out the softc*/
    if (!pci_map_mem(config_id, PCI_CBMA, &va_csrs, &pa_csrs)) {
	free((void *) sc, M_DEVBUF);
	return;
    }

    sc->sc_if.if_name = "fpa";
    sc->sc_if.if_unit = unit;
    sc->sc_pdq = pdq_initialize((void *) va_csrs, "fpa", unit,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	free((void *) sc, M_DEVBUF);
	return;
    }
    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
    pdqs_pci[unit] = sc;
    pdq_ifattach(sc, pdq_pci_ifinit, pdq_pci_ifwatchdog);
    pci_map_int(config_id, pdq_pci_ifintr, (void*) sc, &net_imask);
}

static int
pdq_pci_shutdown(
    struct kern_devconf *kdc,
    int force)
{
    if (kdc->kdc_unit < NFPA)
	pdq_hwreset(PDQ_PCI_UNIT_TO_SOFTC(kdc->kdc_unit)->sc_pdq);
    (void) dev_detach(kdc);
    return 0;
}
#elif defined(__bsdi__)

static int
pdq_pci_match(
    pci_devaddr_t *pa)
{
    int irq;
    int id;

    id = pci_inl(pa, PCI_VENDOR_ID);
    if (PCI_VENDORID(id) != DEC_VENDORID || PCI_CHIPID(id) != DEFPA_CHIPID)
	return 0;

    irq = pci_inl(pa, PCI_I_LINE) & 0xFF;
    if (irq == 0 || irq >= 16)
	return 0;

    return 1;
}

int
pdq_pci_probe(
    struct device *parent,
    struct cfdata *cf,
    void *aux)
{
    struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    pdq_uint32_t irq, data;
    pci_devaddr_t *pa;

    pa = pci_scan(pdq_pci_match);
    if (pa == NULL)
	return 0;

    irq = (1 << (pci_inl(pa, PCI_I_LINE) & 0xFF));

    if (ia->ia_irq != IRQUNK && irq != ia->ia_irq) {
	printf("fpa%d: error: desired IRQ of %d does not match device's actual IRQ of %d\n",
	       cf->cf_unit,
	       ffs(ia->ia_irq) - 1, ffs(irq) - 1);
	return 0;
    }
    if (ia->ia_irq == IRQUNK && (ia->ia_irq = isa_irqalloc(irq)) == 0) {
	printf("fpa%d: error: IRQ %d is already in use\n", cf->cf_unit,
	       ffs(irq) - 1);
	return 0;
    }

    /* PCI bus masters don't use host DMA channels */
    ia->ia_drq = DRQNONE;

    /* Get the memory base address; assume the BIOS set it up correctly */
    ia->ia_maddr = (caddr_t) (pci_inl(pa, PCI_CBMA) & ~7);
    pci_outl(pa, PCI_CBMA, 0xFFFFFFFF);
    ia->ia_msize = ((~pci_inl(pa, PCI_CBMA)) | 7) + 1;
    pci_outl(pa, PCI_CBMA, (int) ia->ia_maddr);

    /* Disable I/O space access */
    pci_outl(pa, PCI_COMMAND, pci_inl(pa, PCI_COMMAND) & ~1);
    ia->ia_iobase = 0;
    ia->ia_iosize = 0;

    /* Make sure the latency timer is what the DEFPA likes */
    data = pci_inl(pa, PCI_CFLT);
    if ((data & 0xFF00) == 0) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_outl(pa, PCI_CFLT, data);
    }

    return 1;
}

void
pdq_pci_attach(
    struct device *parent,
    struct device *self,
    void *aux)
{
    pdq_softc_t *sc = (pdq_softc_t *) self;
    register struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    register struct ifnet *ifp = &sc->sc_if;
    int i;

    sc->sc_if.if_unit = sc->sc_dev.dv_unit;
    sc->sc_if.if_name = "fpa";
    sc->sc_if.if_flags = 0;

    sc->sc_pdq = pdq_initialize((void *) mapphys((vm_offset_t)ia->ia_maddr, ia->ia_msize), "fpa",
				sc->sc_if.if_unit, (void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	printf("fpa%d: initialization failed\n", sc->sc_if.if_unit);
	return;
    }

    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);

    pdq_ifattach(sc, pdq_pci_ifinit, pdq_pci_ifwatchdog);

    isa_establish(&sc->sc_id, &sc->sc_dev);

    sc->sc_ih.ih_fun = pdq_pci_ifintr;
    sc->sc_ih.ih_arg = (void *)sc;
    intr_establish(ia->ia_irq, &sc->sc_ih, DV_NET, sc->sc_dv.dv_xname);

    sc->sc_ats.func = (void (*)(void *)) pdq_hwreset;
    sc->sc_ats.arg = (void *) sc->sc_pdq;
    atshutdown(&sc->sc_ats, ATSH_ADD);
}

struct cfdriver fpacd = {
    0, "fpa", pdq_pci_probe, pdq_pci_attach, DV_IFNET, sizeof(pdq_softc_t)
};

#elif defined(__NetBSD__) || defined (__OpenBSD__)

static int
pdq_pci_probe(
    struct device *parent,
    void *match,
    void *aux)
{
    struct pci_attach_args *pa = (struct pci_attach_args *) aux;

    if (PCI_VENDORID(pa->pa_id) != DEC_VENDORID)
	return 0;
    if (PCI_CHIPID(pa->pa_id) == DEFPA_CHIPID)
	return 1;

    return 0;
}

static void
pdq_pci_attach(
    struct device * const parent,
    struct device * const self,
    void * const aux)
{
    vm_offset_t va_csrs, pa_csrs;
    pdq_uint32_t data;
    pdq_softc_t * const sc = (pdq_softc_t *) self;
    struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
    pci_chipset_tag_t pc = pa->pa_pc;

    data = pci_conf_read(pc, pa->pa_tag, PCI_CFLT);
    if ((data & 0xFF00) == 0) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_conf_write(pc, pa->pa_tag, PCI_CFLT, data);
    }

    if (pci_map_mem(pa->pa_tag, PCI_CBMA, &va_csrs, &pa_csrs))
	return;

    bcopy(sc->sc_dev.dv_xname, sc->sc_if.if_xname, IFNAMSIZ);
    sc->sc_if.if_softc = sc;
    sc->sc_if.if_flags = 0;
    sc->sc_pdq = pdq_initialize((void *) va_csrs,
        sc->sc_dev.dv_cfdata->cf_driver->cd_name, sc->sc_dev.dv_unit,
	(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL)
	return;
    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr,
	6);
    pdq_ifattach(sc, pdq_pci_ifinit, pdq_pci_ifwatchdog);

    sc->sc_ih = pci_map_int(pa->pa_tag, IPL_NET, pdq_pci_ifintr, sc);
    if (sc->sc_ih == NULL) {
	printf("%s: error: couldn't map interrupt\n", sc->sc_dev.dv_xname);
	return;
    }
#if 0
    sc->sc_ats = shutdownhook_establish(pdq_hwreset, sc);
    if (sc->sc_ats == NULL)
	printf("%s: warning: couldn't establish shutdown hook\n",
	       sc->sc_dev.dv_xname);
#endif
}

struct cfattach fpa_ca = {
    sizeof(pdq_softc_t), pdq_pci_probe, pdq_pci_attach
};

struct cfdriver fpa_cd = {
    0, "fpa", DV_IFNET
};
#endif /* __NetBSD__ || __OpenBSD__ */
