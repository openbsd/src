/*	$OpenBSD: cy_pci.c,v 1.4 1997/08/12 18:33:25 niklas Exp $	*/

/*
 * cy.c
 *
 * Driver for Cyclades Cyclom-8/16/32 multiport serial cards
 * (currently not tested with Cyclom-32 cards)
 *
 * Timo Rossi, 1996
 *
 * Supports both ISA and PCI Cyclom cards
 *
 * Uses CD1400 automatic CTS flow control, and
 * if CY_HW_RTS is defined, uses CD1400 automatic input flow control.
 * This requires a special cable that exchanges the RTS and DTR lines.
 *
 * Lots of debug output can be enabled by defining CY_DEBUG
 * Some debugging counters (number of receive/transmit interrupts etc.)
 * can be enabled by defining CY_DEBUG1
 *
 * This version uses the bus_mem/io_??() stuff
 *
 * NOT TESTED !!!
 *
 */

#undef CY_DEBUG
#undef CY_DEBUG1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/cd1400reg.h>
#include <dev/ic/cyreg.h>

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

int cy_probe_pci __P((struct device *, void *, void *));
int cy_probe_common __P((int card, bus_space_tag_t,
			 bus_space_handle_t, int bustype));

void cyattach __P((struct device *, struct device *, void *));

struct cfattach cy_pci_ca = {
  sizeof(struct cy_softc), cy_probe_pci, cyattach
};

/*
 * PCI probe
 */
int
cy_probe_pci(parent, match, aux)
     struct device *parent;
     void *match, *aux;
{
  int card = ((struct device *)match)->dv_unit;
  struct pci_attach_args *pa = aux;
  bus_space_tag_t memt;
  bus_space_handle_t memh;
  bus_addr_t memaddr;
  bus_size_t memsize;
  bus_space_tag_t iot;
  bus_space_handle_t ioh;
  bus_addr_t iobase;
  bus_size_t iosize;
  int cacheable;

  if(!(PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CYCLADES &&
       (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CYCLADES_CYCLOMY_1 ||
	PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CYCLADES_CYCLOMY_2)))
    return 0;

#ifdef CY_DEBUG
  printf("cy: Found Cyclades PCI device, id = 0x%x\n", pa->pa_id);
#endif

  memt = pa->pa_memt;
  iot = pa->pa_iot;

  if(pci_mem_find(pa->pa_pc, pa->pa_tag, 0x18,
		  &memaddr, &memsize, &cacheable) != 0) {
    printf("cy%d: can't find PCI card memory", card);
    return 0;
  }

  /* map the memory (non-cacheable) */
  if(bus_space_map(memt, memaddr, memsize, 0, &memh) != 0) {
    printf("cy%d: couldn't map PCI memory region\n", card);
    return 0;
  }

  /* the PCI Cyclom IO space is only used for enabling interrupts */
  if(pci_io_find(pa->pa_pc, pa->pa_tag, 0x14, &iobase, &iosize) != 0) {
    bus_space_unmap(memt, memh, memsize);
    printf("cy%d: couldn't find PCI io region\n", card);
    return 0;
  }

  if(bus_space_map(iot, iobase, iosize, 0, &ioh) != 0) {
    bus_space_unmap(memt, memh, memsize);
    printf("cy%d: couldn't map PCI io region\n", card);
    return 0; 
  }

#ifdef CY_DEBUG
  printf("cy%d: pci mapped mem 0x%lx (size %d), io 0x%x (size %d)\n",
	 card, memaddr, memsize, iobase, iosize);
#endif

  if(cy_probe_common(card, memt, memh, CY_BUSTYPE_PCI) == 0) {
    bus_space_unmap(memt, memh, memsize);
    bus_space_unmap(iot, ioh, iosize);
    printf("cy%d: PCI Cyclom card with no CD1400s!?\n", card);
    return 0;
  }

  /* Enable PCI card interrupts */
  bus_space_write_2(iot, ioh, CY_PCI_INTENA,
      bus_space_read_2(iot, ioh, CY_PCI_INTENA) | 0x900);

  return 1;
}
