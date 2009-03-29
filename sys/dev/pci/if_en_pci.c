/*	$OpenBSD: if_en_pci.c,v 1.12 2009/03/29 21:53:52 sthen Exp $	*/

/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *	Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 *
 * i f _ e n _ p c i . c  
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * started: spring, 1996.
 *
 * PCI glue for the eni155p card.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/midwayreg.h>
#include <dev/ic/midwayvar.h>


/*
 * local structures
 */

struct en_pci_softc {
  /* bus independent stuff */
  struct en_softc esc;		/* includes "device" structure */

  /* PCI bus glue */
  void *sc_ih;			/* interrupt handle */
  pci_chipset_tag_t en_pc;	/* for PCI calls */

};

/*
 * local defines (PCI specific stuff)
 */

/* 
 * address of config base memory address register in PCI config space
 * (this is card specific)
 */
        
#define PCI_CBMA        0x10

/*
 * tonga (pci bridge).   ENI cards only!
 */

#define EN_TONGA        0x60            /* PCI config addr of tonga reg */

#define TONGA_SWAP_DMA  0x80            /* endian swap control */
#define TONGA_SWAP_BYTE 0x40
#define TONGA_SWAP_WORD 0x20

/*
 * adaptec pci bridge.   ADP cards only!
 */

#define ADP_PCIREG	0x050040	/* PCI control register */

#define ADP_PCIREG_RESET	0x1	/* reset card */
#define ADP_PCIREG_IENABLE	0x2	/* interrupt enable */
#define ADP_PCIREG_SWAP_WORD	0x4	/* swap byte on slave access */
#define ADP_PCIREG_SWAP_DMA	0x8	/* swap bytes on DMA */

/*
 * prototypes
 */

static	int en_pci_match(struct device *, void *, void *);
static	void en_pci_attach(struct device *, struct device *, void *);

/*
 * PCI autoconfig attachments
 */

struct cfattach en_pci_ca = {
    sizeof(struct en_pci_softc), en_pci_match, en_pci_attach,
};

#if !defined(MIDWAY_ENIONLY)

static void adp_busreset(void *);

/*
 * bus specific reset function [ADP only!]
 */

static void adp_busreset(v)

void *v;

{
  struct en_softc *sc = (struct en_softc *) v;
  u_int32_t dummy;

  bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG, ADP_PCIREG_RESET);
  DELAY(1000);  /* let it reset */
  dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
  bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG, 
                (ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA|ADP_PCIREG_IENABLE));
  dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
  if ((dummy & (ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA)) !=
                (ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA))
    printf("adp_busreset: Adaptec ATM did NOT reset!\n");

}
#endif

/***********************************************************************/

/*
 * autoconfig stuff
 */

static int en_pci_match(parent, match, aux)

struct device *parent;
void *match;
void *aux;

{
  struct pci_attach_args *pa = (struct pci_attach_args *) aux;

#if !defined(MIDWAY_ADPONLY)
  if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_EFFICIENTNETS && 
      (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_EFFICIENTNETS_ENI155PF ||
       PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_EFFICIENTNETS_ENI155PA))
    return 1;
#endif

#if !defined(MIDWAY_ENIONLY)
  if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADP && 
      (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADP_AIC5900 ||
       PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADP_AIC5905))
    return 1;
#endif

  return 0;
}


static void en_pci_attach(parent, self, aux)

struct device *parent, *self;
void *aux;

{
  struct en_softc *sc = (void *)self;
  struct en_pci_softc *scp = (void *)self;
  struct pci_attach_args *pa = aux;
  pci_intr_handle_t ih;
  const char *intrstr;
  int retval;

  sc->is_adaptec = (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADP) ? 1 : 0;
  scp->en_pc = pa->pa_pc;

  /*
   * interrupt map
   */

  if (pci_intr_map(pa, &ih)) {
    printf(": can't map interrupt\n");
    return;
  }
  intrstr = pci_intr_string(scp->en_pc, ih);
  scp->sc_ih = pci_intr_establish(scp->en_pc, ih, IPL_NET, en_intr, sc,
      sc->sc_dev.dv_xname);
  if (scp->sc_ih == NULL) {
    printf(": can't establish interrupt");
    if (intrstr != NULL)
      printf(" at %s", intrstr);
    printf("\n");
    return;
  }
  sc->ipl = 1; /* XXX */

  /*
   * memory map
   */

  retval = pci_mapreg_map(pa, PCI_CBMA, PCI_MAPREG_TYPE_MEM, 0,
    &sc->en_memt, &sc->en_base, NULL, &sc->en_obmemsz, 0);
 
  if (retval) {
    printf(": can't map mem space\n");
    return;
  }

  printf(": %s\n", intrstr);

  /*
   * set up pci bridge
   */

#if !defined(MIDWAY_ENIONLY)
  if (sc->is_adaptec) {
    sc->en_busreset = adp_busreset;
    adp_busreset(sc);
  }
#endif

#if !defined(MIDWAY_ADPONLY)
  if (!sc->is_adaptec) {
    sc->en_busreset = NULL;
    pci_conf_write(scp->en_pc, pa->pa_tag, EN_TONGA, 
		  (TONGA_SWAP_DMA|TONGA_SWAP_WORD));
  }
#endif

  /*
   * done PCI specific stuff
   */

  en_attach(sc);

}
