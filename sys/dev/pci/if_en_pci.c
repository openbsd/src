/*	$OpenBSD: if_en_pci.c,v 1.1 1996/06/21 15:36:34 chuck Exp $	*/

/*
 *
 * Copyright (c) 1996 Charles D. Cranor
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
 *      This product includes software developed by Charles D. Cranor.
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
#include <sys/types.h>
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
 * tonga (pci bridge)
 */

#define EN_TONGA        0x60            /* PCI config addr of tonga reg */

#define TONGA_SWAP_DMA  0x80            /* endian swap control */
#define TONGA_SWAP_BYTE 0x40
#define TONGA_SWAP_WORD 0x20

/*
 * prototypes
 */

static	int en_pci_match __P((struct device *, void *, void *));
static	void en_pci_attach __P((struct device *, struct device *, void *));

/*
 * PCI autoconfig attachments
 */

struct cfattach en_pci_ca = {
    sizeof(struct en_pci_softc), en_pci_match, en_pci_attach,
};

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

  if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_EFFICIENTNETS)
    return 0;

  if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_EFFICIENTNETS_ENI155P)
    return 1;

  return 0;
}


static void en_pci_attach(parent, self, aux)

struct device *parent, *self;
void *aux;

{
  struct en_softc *sc = (void *)self;
  struct en_pci_softc *scp = (void *)self;
  struct pci_attach_args *pa = aux;
  bus_mem_addr_t membase;
  pci_intr_handle_t ih;
  const char *intrstr;
  int retval;

  printf("\n");

  sc->en_bc = pa->pa_bc;
  scp->en_pc = pa->pa_pc;

  /*
   * interrupt map
   */

  if (pci_intr_map(scp->en_pc, pa->pa_intrtag, pa->pa_intrpin, 
					pa->pa_intrline, &ih)) {
    printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
    return;
  }
  intrstr = pci_intr_string(scp->en_pc, ih);
  scp->sc_ih = pci_intr_establish(scp->en_pc, ih, IPL_NET, en_intr, sc);
  if (scp->sc_ih == NULL) {
    printf("%s: couldn't establish interrupt\n", sc->sc_dev.dv_xname);
    if (intrstr != NULL)
      printf(" at %s", intrstr);
    printf("\n");
    return;
  }
  printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);
  sc->ipl = 1; /* XXX */

  /*
   * memory map
   */

  retval = pci_mem_find(scp->en_pc, pa->pa_tag, PCI_CBMA,
				&membase, &sc->en_obmemsz, NULL);
  if (retval == 0)
    retval = bus_mem_map(sc->en_bc, membase, sc->en_obmemsz, 0, &sc->en_base);
 
  if (retval) {
    printf("%s: couldn't map memory\n", sc->sc_dev.dv_xname);
    return;
  }

  /*
   * set up swapping
   */

  pci_conf_write(scp->en_pc, pa->pa_tag, EN_TONGA, 
		(TONGA_SWAP_DMA|TONGA_SWAP_WORD));

  /*
   * done PCI specific stuff
   */

  en_attach(sc);

}
