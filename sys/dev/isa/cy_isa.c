/*	$OpenBSD: cy_isa.c,v 1.4 1999/11/30 23:48:07 aaron Exp $	*/

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/user.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>

#include <dev/ic/cd1400reg.h>
#include <dev/ic/cyreg.h>

int cy_probe_isa __P((struct device *, void *, void *));
int cy_probe_common __P((int card, bus_space_tag_t,
			 bus_space_handle_t, int bustype));

void cyattach __P((struct device *, struct device *, void *));

struct cfattach cy_isa_ca = {
  sizeof(struct cy_softc), cy_probe_isa, cyattach
};

/*
 * ISA probe
 */
int
cy_probe_isa(parent, match, aux)
     struct device *parent;
     void *match, *aux;
{
  int card = ((struct device *)match)->dv_unit;
  struct isa_attach_args *ia = aux;
  bus_space_tag_t memt;
  bus_space_handle_t memh;

  if(ia->ia_irq == IRQUNK) {
    printf("cy%d error: interrupt not defined\n", card);
    return 0;
  }

  memt = ia->ia_memt;
  if(bus_space_map(memt, ia->ia_maddr, 0x2000, 0, &memh) != 0)
    return 0;

  if(cy_probe_common(card, memt, memh, CY_BUSTYPE_ISA) == 0) {
    bus_space_unmap(memt, memh, 0x2000);
    return 0;
  }

  ia->ia_iosize = 0;
  ia->ia_msize = 0x2000;
  return 1;
}
