/* $OpenBSD: com_cardbus.c,v 1.9 2004/10/07 21:16:59 brad Exp $ */
/* $NetBSD: com_cardbus.c,v 1.4 2000/04/17 09:21:59 joda Exp $ */

/*
 * Copyright (c) 2000 Johan Danielsson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of author nor the names of any contributors may
 *    be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This is a driver for CardBus based serial devices. It is less
   generic than it could be, but it keeps the complexity down. So far
   it assumes that anything that reports itself as a `serial' device
   is infact a 16x50 or 8250, which is not necessarily true (in
   practice this shouldn't be a problem). It also does not handle
   devices in the `multiport serial' or `modem' sub-classes, I've
   never seen any of these, so I don't know what they might look like.

   If the CardBus device only has one BAR (that is not also the CIS
   BAR) listed in the CIS, it is assumed to be the one to use. For
   devices with more than one BAR, the list of known devies has to be
   updated below.  */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/device.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pcmcia/pcmciareg.h>

#include <dev/isa/isavar.h>

#include "com.h"
#ifdef i386
#include "pccom.h"
#endif

#include <dev/ic/comreg.h>
#if NPCCOM > 0
#include <i386/isa/pccomvar.h>
#endif
#if NCOM > 0
#include <dev/ic/comvar.h>
#endif
#include <dev/ic/ns16550reg.h>

#define	com_lcr		com_cfcr

struct com_cardbus_softc {
	struct com_softc	cc_com;
	void			*cc_ih;
	cardbus_devfunc_t	cc_ct;
	bus_addr_t		cc_addr;
	cardbusreg_t		cc_base;
	bus_size_t		cc_size;
	cardbusreg_t		cc_csr;
	int			cc_cben;
	cardbustag_t		cc_tag;
	cardbusreg_t		cc_reg;
	int			cc_type;
};

#define DEVNAME(CSC) ((CSC)->cc_com.sc_dev.dv_xname)

int	com_cardbus_match(struct device *, void *, void *);
void	com_cardbus_attach(struct device *, struct device *, void *);
int	com_cardbus_detach(struct device *, int);

void	com_cardbus_setup(struct com_cardbus_softc *);
int	com_cardbus_enable(struct com_softc *);
void	com_cardbus_disable(struct com_softc *);
struct csdev *com_cardbus_find_csdev(struct cardbus_attach_args *);
int	com_cardbus_gofigure(struct cardbus_attach_args *,
    struct com_cardbus_softc *);
void	com_cardbus_attach2(struct com_softc *);

#if NCOM_CARDBUS
struct cfattach com_cardbus_ca = {
	sizeof(struct com_cardbus_softc), com_cardbus_match,
	com_cardbus_attach, com_cardbus_detach, com_activate
};
#elif NPCCOM_CARDBUS
struct cfattach pccom_cardbus_ca = {
	sizeof(struct com_cardbus_softc), com_cardbus_match,
	com_cardbus_attach, com_cardbus_detach, com_activate
};
#endif

static struct csdev {
	int		vendor;
	int		product;
	cardbusreg_t	reg;
	int		type;
} csdevs[] = {
	{ PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_MODEM56,
	  CARDBUS_BASE0_REG, CARDBUS_MAPREG_TYPE_IO },
	{ PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_CBEM56G,
	  CARDBUS_BASE0_REG, CARDBUS_MAPREG_TYPE_IO },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_MODEM56,
	  CARDBUS_BASE0_REG, CARDBUS_MAPREG_TYPE_IO },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_MODEM56,
	  CARDBUS_BASE0_REG, CARDBUS_MAPREG_TYPE_IO },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_GLOBALMODEM56,
	  CARDBUS_BASE0_REG, CARDBUS_MAPREG_TYPE_IO }
};

static const int ncsdevs = sizeof(csdevs) / sizeof(csdevs[0]);

struct csdev*
com_cardbus_find_csdev(struct cardbus_attach_args *ca)
{
	struct csdev *cp;

	for (cp = csdevs; cp < csdevs + ncsdevs; cp++)
		if (cp->vendor == CARDBUS_VENDOR(ca->ca_id) &&
		    cp->product == CARDBUS_PRODUCT(ca->ca_id))
			return (cp);
	return (NULL);
}

int
com_cardbus_match(struct device *parent, void *match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	/* known devices are ok */
	if (com_cardbus_find_csdev(ca) != NULL)
	    return (10);

	/* as are serial devices with a known UART */
	if (ca->ca_cis.funcid == PCMCIA_FUNCTION_SERIAL &&
	    ca->ca_cis.funce.serial.uart_present != 0 &&
	    (ca->ca_cis.funce.serial.uart_type == 0 ||	/* 8250 */
	    ca->ca_cis.funce.serial.uart_type == 1 ||	/* 16450 */
	    ca->ca_cis.funce.serial.uart_type == 2))	/* 16550 */
	    return (1);

	return (0);
}

int
com_cardbus_gofigure(struct cardbus_attach_args *ca,
    struct com_cardbus_softc *csc)
{
	int i, index = -1;
	cardbusreg_t cis_ptr;
	struct csdev *cp;

	/* If this device is listed above, use the known values, */
	cp = com_cardbus_find_csdev(ca);
	if (cp != NULL) {
		csc->cc_reg = cp->reg;
		csc->cc_type = cp->type;
		return (0);
	}

	cis_ptr = Cardbus_conf_read(csc->cc_ct, csc->cc_tag, CARDBUS_CIS_REG);

	/* otherwise try to deduce which BAR and type to use from CIS.  If
	   there is only one BAR, it must be the one we should use, if
	   there are more, we're out of luck.  */
	for (i = 0; i < 7; i++) {
		/* ignore zero sized BARs */
		if (ca->ca_cis.bar[i].size == 0)
			continue;
		/* ignore the CIS BAR */
		if (CARDBUS_CIS_ASI_BAR(cis_ptr) ==
		    CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[i].flags))
			continue;
		if (index != -1)
			goto multi_bar;
		index = i;
	}
	if (index == -1) {
		printf(": couldn't find any base address tuple\n");
		return (1);
	}
	csc->cc_reg = CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[index].flags);
	if ((ca->ca_cis.bar[index].flags & 0x10) == 0)
		csc->cc_type = CARDBUS_MAPREG_TYPE_MEM;
	else
		csc->cc_type = CARDBUS_MAPREG_TYPE_IO;
	return (0);

  multi_bar:
	printf(": there are more than one possible base\n");

	printf("%s: address for this device, "
	    "please report the following information\n",
	    DEVNAME(csc));
	printf("%s: vendor 0x%x product 0x%x\n", DEVNAME(csc),
	    CARDBUS_VENDOR(ca->ca_id), CARDBUS_PRODUCT(ca->ca_id));
	for (i = 0; i < 7; i++) {
		/* ignore zero sized BARs */
		if (ca->ca_cis.bar[i].size == 0)
			continue;
		/* ignore the CIS BAR */
		if (CARDBUS_CIS_ASI_BAR(cis_ptr) ==
		    CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[i].flags))
			continue;
		printf("%s: base address %x type %s size %x\n",
		    DEVNAME(csc), CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[i].flags),
		    (ca->ca_cis.bar[i].flags & 0x10) ? "i/o" : "mem",
		    ca->ca_cis.bar[i].size);
	}
	return (1);
}

void
com_cardbus_attach (struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (struct com_softc*)self;
	struct com_cardbus_softc *csc = (struct com_cardbus_softc*)self;
	struct cardbus_attach_args *ca = aux;

	csc->cc_ct = ca->ca_ct;
	csc->cc_tag = Cardbus_make_tag(csc->cc_ct);

	if (com_cardbus_gofigure(ca, csc) != 0)
		return;

	if (Cardbus_mapreg_map(ca->ca_ct, csc->cc_reg, csc->cc_type, 0,
	    &sc->sc_iot, &sc->sc_ioh, &csc->cc_addr, &csc->cc_size) != 0) {
		printf("failed to map memory");
		return;
	}

	csc->cc_base = csc->cc_addr;
	csc->cc_csr = CARDBUS_COMMAND_MASTER_ENABLE;
	if (csc->cc_type == CARDBUS_MAPREG_TYPE_IO) {
		csc->cc_base |= CARDBUS_MAPREG_TYPE_IO;
		csc->cc_csr |= CARDBUS_COMMAND_IO_ENABLE;
		csc->cc_cben = CARDBUS_IO_ENABLE;
	} else {
		csc->cc_csr |= CARDBUS_COMMAND_MEM_ENABLE;
		csc->cc_cben = CARDBUS_MEM_ENABLE;
	}
	sc->sc_iobase = csc->cc_addr;

#if 0
	sc->sc_frequency = COM_FREQ;
#endif

	sc->enable = com_cardbus_enable;
	sc->disable = com_cardbus_disable;
	sc->enabled = 0;

	if (ca->ca_cis.cis1_info[0] && ca->ca_cis.cis1_info[1]) {
		printf(": %s %s\n", ca->ca_cis.cis1_info[0],
		    ca->ca_cis.cis1_info[1]);
		printf("%s", DEVNAME(csc));
	}

#ifdef __OpenBSD__
	if (com_cardbus_enable(sc))
		printf(": function enable failed\n");

	sc->enabled = 1;

	com_cardbus_attach2(sc);
#else
	com_cardbus_setup(csc);

	com_attach_subr(sc);

	Cardbus_function_disable(csc->cc_ct);
#endif
}

void
com_cardbus_setup(struct com_cardbus_softc *csc)
{
        cardbus_devfunc_t ct = csc->cc_ct;
        cardbus_chipset_tag_t cc = ct->ct_cc;
        cardbus_function_tag_t cf = ct->ct_cf;
	cardbusreg_t reg;

	Cardbus_conf_write(ct, csc->cc_tag, csc->cc_reg, csc->cc_base);

	/* enable accesses on cardbus bridge */
	cf->cardbus_ctrl(cc, csc->cc_cben);
	cf->cardbus_ctrl(cc, CARDBUS_BM_ENABLE);

	/* and the card itself */
	reg = Cardbus_conf_read(ct, csc->cc_tag, CARDBUS_COMMAND_STATUS_REG);
	reg &= ~(CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE);
	reg |= csc->cc_csr;
	Cardbus_conf_write(ct, csc->cc_tag, CARDBUS_COMMAND_STATUS_REG, reg);

        /*
         * Make sure the latency timer is set to some reasonable
         * value.
         */
        reg = cardbus_conf_read(cc, cf, csc->cc_tag, CARDBUS_BHLC_REG);
        if (CARDBUS_LATTIMER(reg) < 0x20) {
                reg &= ~(CARDBUS_LATTIMER_MASK << CARDBUS_LATTIMER_SHIFT);
                reg |= (0x20 << CARDBUS_LATTIMER_SHIFT);
                cardbus_conf_write(cc, cf, csc->cc_tag, CARDBUS_BHLC_REG, reg);
        }
}

int
com_cardbus_enable(struct com_softc *sc)
{
	struct com_cardbus_softc *csc = (struct com_cardbus_softc*)sc;
	struct cardbus_softc *psc =
	    (struct cardbus_softc *)sc->sc_dev.dv_parent;
	cardbus_chipset_tag_t cc = psc->sc_cc;
	cardbus_function_tag_t cf = psc->sc_cf;

	Cardbus_function_enable(csc->cc_ct);

	com_cardbus_setup(csc);

	/* establish the interrupt. */
	csc->cc_ih = cardbus_intr_establish(cc, cf, psc->sc_intrline,
	    IPL_TTY, comintr, sc);
	if (csc->cc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n", DEVNAME(csc));
		return (1);
	}

	printf(": irq %d", psc->sc_intrline);

	return (0);
}

void
com_cardbus_disable(struct com_softc *sc)
{
	struct com_cardbus_softc *csc = (struct com_cardbus_softc*)sc;
	struct cardbus_softc *psc =
	    (struct cardbus_softc *)sc->sc_dev.dv_parent;
	cardbus_chipset_tag_t cc = psc->sc_cc;
	cardbus_function_tag_t cf = psc->sc_cf;

	cardbus_intr_disestablish(cc, cf, csc->cc_ih);
	Cardbus_function_disable(csc->cc_ct);
}

int
com_cardbus_detach(struct device *self, int flags)
{
	struct com_cardbus_softc *csc = (struct com_cardbus_softc *) self;
	struct com_softc *sc = (struct com_softc *) self;
	struct cardbus_softc *psc = (struct cardbus_softc *)self->dv_parent;
	int error;

	if ((error = com_detach(self, flags)) != 0)
		return (error);

	cardbus_intr_disestablish(psc->sc_cc, psc->sc_cf, csc->cc_ih);

	Cardbus_mapreg_unmap(csc->cc_ct, csc->cc_reg, sc->sc_iot, sc->sc_ioh,
	    csc->cc_size);

	return (0);
}


/*
 * XXX This should be handled by a generic attach
 */
void
com_cardbus_attach2(sc)
	struct com_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t lcr;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;

	/*
	 * Probe for all known forms of UART.
	 */
	lcr = bus_space_read_1(iot, ioh, com_lcr);

	bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
	bus_space_write_1(iot, ioh, com_efr, 0);
	bus_space_write_1(iot, ioh, com_lcr, 0);

	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	delay(100);

	switch(bus_space_read_1(iot, ioh, com_iir) >> 6) {
	case 0:
		sc->sc_uarttype = COM_UART_16450;
		break;
	case 2:
		sc->sc_uarttype = COM_UART_16550;
		break;
	case 3:
		sc->sc_uarttype = COM_UART_16550A;
		break;
	default:
		sc->sc_uarttype = COM_UART_UNKNOWN;
		break;
	}

	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for ST16650s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		if (bus_space_read_1(iot, ioh, com_efr) == 0) {
			sc->sc_uarttype = COM_UART_ST16650;
		} else {
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			if (bus_space_read_1(iot, ioh, com_efr) == 0)
				sc->sc_uarttype = COM_UART_ST16650V2;
		}
	}

#if NPCCOM > 0
#ifdef i386
	if (sc->sc_uarttype == COM_UART_ST16650V2) {	/* Probe for XR16850s */
		u_int8_t dlbl, dlbh;

		/* Enable latch access and get the current values. */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		dlbl = bus_space_read_1(iot, ioh, com_dlbl);
		dlbh = bus_space_read_1(iot, ioh, com_dlbh);

		/* Zero out the latch divisors */
		bus_space_write_1(iot, ioh, com_dlbl, 0);
		bus_space_write_1(iot, ioh, com_dlbh, 0);

		if (bus_space_read_1(iot, ioh, com_dlbh) == 0x10) {
			sc->sc_uarttype = COM_UART_XR16850;
			sc->sc_uartrev = bus_space_read_1(iot, ioh, com_dlbl);
		}

		/* Reset to original. */
		bus_space_write_1(iot, ioh, com_dlbl, dlbl);
		bus_space_write_1(iot, ioh, com_dlbh, dlbh);
	}
#endif
#endif

	/* Reset the LCR (latch access is probably enabled). */
	bus_space_write_1(iot, ioh, com_lcr, lcr);
	if (sc->sc_uarttype == COM_UART_16450) { /* Probe for 8250 */
		u_int8_t scr0, scr1, scr2;

		scr0 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0xa5);
		scr1 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0x5a);
		scr2 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, scr0);

		if ((scr1 != 0xa5) || (scr2 != 0x5a))
			sc->sc_uarttype = COM_UART_8250;
	}

	/*
	 * Print UART type and initialize ourself.
	 */
	sc->sc_fifolen = 1;	/* default */
	switch (sc->sc_uarttype) {
	case COM_UART_UNKNOWN:
		printf("unknown uart\n");
		break;
	case COM_UART_8250:
		printf("ns8250, no fifo\n");
		break;
	case COM_UART_16450:
		printf("ns16450, no fifo\n");
		break;
	case COM_UART_16550:
		printf("ns16550, no working fifo\n");
		break;
	case COM_UART_16550A:
		printf("ns16550a, 16 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 16;
		break;
	case COM_UART_ST16650:
		printf("st16650, no working fifo\n");
		break;
	case COM_UART_ST16650V2:
		printf("st16650, 32 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 32;
		break;
#if NPCCOM > 0
#ifdef i386
	case COM_UART_XR16850:
		printf("xr16850 (rev %d), 128 byte fifo\n", sc->sc_uartrev);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 128;
		break;
#endif
#endif
	default:
		panic("comattach: bad fifo type");
	}

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	(void)bus_space_read_1(iot, ioh, com_data);
	bus_space_write_1(iot, ioh, com_fifo, 0);
}
