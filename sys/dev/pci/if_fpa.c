/*	$OpenBSD: if_fpa.c,v 1.12 1999/11/23 04:49:30 jason Exp $	*/
/*	$NetBSD: if_fpa.c,v 1.15 1996/10/21 22:56:40 thorpej Exp $	*/

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
 * Id: if_fpa.c,v 1.8 1996/05/17 01:15:18 thomas Exp
 *
 */

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 *   This module supports the DEC DEFPA PCI FDDI Controller
 */


#include <sys/param.h>
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
#include <pci/pdqvar.h>
#include <pci/pdqreg.h>
#elif defined(__bsdi__)
#if BSDI_VERSION < 199401
#include <i386/isa/isavar.h>
#include <i386/isa/icu.h>
#define	DRQNONE		0
#define IRQSHARE	0
#endif
#include <i386/pci/pci.h>
#include <i386/pci/pdqvar.h>
#include <i386/pci/pdqreg.h>
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/pdqvar.h>
#include <dev/ic/pdqreg.h>
#endif /* __NetBSD__ */


#define	DEC_VENDORID		0x1011
#define	DEFPA_CHIPID		0x000F
#define	PCI_VENDORID(x)		((x) & 0xFFFF)
#define	PCI_CHIPID(x)		(((x) >> 16) & 0xFFFF)

#define	DEFPA_LATENCY	0x88

#define	PCI_CFLT	0x0C	/* Configuration Latency */
#define	PCI_CBMA	0x10	/* Configuration Base Memory Address */
#define	PCI_CBIO	0x14	/* Configuration Base I/O Address */

#if defined(__FreeBSD__)
static pdq_softc_t *pdqs_pci[NFPA];
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	(pdqs_pci[unit])
#if BSD >= 199506
#define	pdq_pci_ifwatchdog		NULL
#endif

#elif defined(__bsdi__)
extern struct cfdriver fpacd;
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)fpacd.cd_devs[unit])

#elif defined(__NetBSD__) || defined(__OpenBSD__)
extern struct cfattach fpa_ca;
extern struct cfdriver fpa_cd;
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)fpa_cd.cd_devs[unit])
#define	pdq_pci_ifwatchdog		NULL
#endif

#ifndef pdq_pci_ifwatchdog
static ifnet_ret_t
pdq_pci_ifwatchdog(
    int unit)
{
    pdq_ifwatchdog(&PDQ_PCI_UNIT_TO_SOFTC(unit)->sc_if);
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
/*
 * This is the PCI configuration support.  Since the PDQ is available
 * on both EISA and PCI boards, one must be careful in how defines the
 * PDQ in the config file.
 */
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
    if ((data & 0xFF00) < (DEFPA_LATENCY << 8)) {
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
    sc->sc_membase = (pdq_bus_memaddr_t) va_csrs;
    sc->sc_pdq = pdq_initialize(PDQ_BUS_PCI, sc->sc_membase,
				sc->sc_if.if_name, sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	free((void *) sc, M_DEVBUF);
	return;
    }
    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
    pdqs_pci[unit] = sc;
    pdq_ifattach(sc, pdq_pci_ifwatchdog);
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
    if (ia->ia_irq == IRQUNK) {
	(void) isa_irqalloc(irq);
	ia->ia_irq = irq;
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
    if ((data & 0xFF00) < (DEFPA_LATENCY << 8)) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_outl(pa, PCI_CFLT, data);
    }
    ia->ia_irq |= IRQSHARE;

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
    sc->sc_membase = (pdq_bus_memaddr_t) mapphys((vm_offset_t)ia->ia_maddr, ia->ia_msize);

    sc->sc_pdq = pdq_initialize(PDQ_BUS_PCI, sc->sc_membase,
				sc->sc_if.if_nme, sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	printf("fpa%d: initialization failed\n", sc->sc_if.if_unit);
	return;
    }

    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);

    pdq_ifattach(sc, pdq_pci_ifwatchdog);

    isa_establish(&sc->sc_id, &sc->sc_dev);

    sc->sc_ih.ih_fun = pdq_pci_ifintr;
    sc->sc_ih.ih_arg = (void *)sc;
    intr_establish(ia->ia_irq, &sc->sc_ih, DV_NET);

    sc->sc_ats.func = (void (*)(void *)) pdq_hwreset;
    sc->sc_ats.arg = (void *) sc->sc_pdq;
    atshutdown(&sc->sc_ats, ATSH_ADD);
}

struct cfdriver fpacd = {
    0, "fpa", pdq_pci_probe, pdq_pci_attach,
#if _BSDI_VERSION >= 199401
    DV_IFNET,
#endif
    sizeof(pdq_softc_t)
};

#elif defined(__NetBSD__) || defined(__OpenBSD__)

static int
pdq_pci_match(
    struct device *parent,
    void *match,
    void *aux)
{
    struct pci_attach_args *pa = (struct pci_attach_args *) aux;

    if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_DEC)
	return 0;
    if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_DEC_DEFPA)
	return 0;

    return 1;
}

static void
pdq_pci_attach(
    struct device * const parent,
    struct device * const self,
    void * const aux)
{
    pdq_softc_t * const sc = (pdq_softc_t *) self;
    struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
    pdq_uint32_t data;
    pci_intr_handle_t intrhandle;
    const char *intrstr;
    bus_addr_t csrbase;
    bus_size_t csrsize;
    int cacheable = 0;

    data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFLT);
    if ((data & 0xFF00) < (DEFPA_LATENCY << 8)) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_CFLT, data);
    }

    bcopy(sc->sc_dev.dv_xname, sc->sc_if.if_xname, IFNAMSIZ);
    sc->sc_if.if_flags = 0;
    sc->sc_if.if_softc = sc;

    /*
     * NOTE: sc_bc is an alias for sc_csrtag and sc_membase is an
     * alias for sc_csrhandle.  sc_iobase is not used in this front-end.
     */
#ifdef PDQ_IOMAPPED
    sc->sc_csrtag = pa->pa_iot;
    if (pci_io_find(pa->pa_pc, pa->pa_tag, PCI_CBIO, &csrbase, &csrsize)) {
	printf(": can't find I/O space!\n");
	return;
    }
#else
    sc->sc_csrtag = pa->pa_memt;
    if (pci_mem_find(pa->pa_pc, pa->pa_tag, PCI_CBMA, &csrbase, &csrsize,
      &cacheable)) {
	printf(": can't find memory space!\n");
	return;
    }
#endif

    if (bus_space_map(sc->sc_csrtag, csrbase, csrsize, cacheable,
      &sc->sc_csrhandle)) {
	printf(": can't map CSRs!\n");
	return;
    }

    if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
		     pa->pa_intrline, &intrhandle)) {
	printf(": couldn't map interrupt\n");
	return;
    }
    intrstr = pci_intr_string(pa->pa_pc, intrhandle);
    sc->sc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET, pdq_pci_ifintr,
	sc, self->dv_xname);
    if (sc->sc_ih == NULL) {
	printf(": couldn't establish interrupt");
	if (intrstr != NULL)
	    printf(" at %s", intrstr);
	printf("\n");
	return;
    }
    if (intrstr != NULL)
	printf(": %s\n", intrstr);

    sc->sc_pdq = pdq_initialize(sc->sc_csrtag, sc->sc_csrhandle,
				sc->sc_if.if_xname, 0,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	printf(": initialization failed\n");
	return;
    }

    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
    pdq_ifattach(sc, pdq_pci_ifwatchdog);

    sc->sc_ats = shutdownhook_establish((void (*)(void *)) pdq_hwreset, sc->sc_pdq);
    if (sc->sc_ats == NULL)
	printf("%s: warning: couldn't establish shutdown hook\n", self->dv_xname);
}

struct cfattach fpa_ca = {
    sizeof(pdq_softc_t), pdq_pci_match, pdq_pci_attach
};

struct cfdriver fpa_cd = {
    0, "fpa", DV_IFNET
};

#endif /* __NetBSD__ */
