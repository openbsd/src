/*	$OpenBSD: imxehci.c,v 1.7 2015/03/29 03:24:17 jsg Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/imx/imxccmvar.h>
#include <armv7/imx/imxgpiovar.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

/* usb phy */
#define USBPHY_PWD			0x00
#define USBPHY_CTRL			0x30
#define USBPHY_CTRL_SET			0x34
#define USBPHY_CTRL_CLR			0x38
#define USBPHY_CTRL_TOG			0x3c

#define USBPHY_CTRL_ENUTMILEVEL2	(1 << 14)
#define USBPHY_CTRL_ENUTMILEVEL3	(1 << 15)
#define USBPHY_CTRL_CLKGATE		(1 << 30)
#define USBPHY_CTRL_SFTRST		(1U << 31)

/* ehci */
#define EHCI_USBMODE			0x68

#define EHCI_USBMODE_HOST		(3 << 0)
#define EHCI_PS_PTS_UTMI_MASK	((1 << 25) | (3 << 30))

/* usb non-core */
#define USBNC_USB_UH1_CTRL		0x04

#define USBNC_USB_UH1_CTRL_OVER_CUR_POL	(1 << 8)
#define USBNC_USB_UH1_CTRL_OVER_CUR_DIS	(1 << 7)

/* board specific */
#define EHCI_HUMMINGBOARD_USB_H1_PWR	0
#define EHCI_HUMMINGBOARD_USB_OTG_PWR	(2*32+22)
#define EHCI_NITROGEN6X_USB_HUB_RST	(6*32+12)
#define EHCI_PHYFLEX_USB_H1_PWR		0
#define EHCI_PHYFLEX_USB_OTG_PWR	111
#define EHCI_SABRESD_USB_PWR		(0*32+29)
#define EHCI_UTILITE_USB_HUB_RST	(6*32+8)

void	imxehci_attach(struct device *, struct device *, void *);
int	imxehci_detach(struct device *, int);

struct imxehci_softc {
	struct ehci_softc	sc;
	void			*sc_ih;
	bus_space_handle_t	uh_ioh;
	bus_space_handle_t	ph_ioh;
	bus_space_handle_t	nc_ioh;
};

struct cfattach imxehci_ca = {
	sizeof (struct imxehci_softc), NULL, imxehci_attach,
	imxehci_detach, NULL
};

void
imxehci_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxehci_softc	*sc = (struct imxehci_softc *)self;
	struct armv7_attach_args *aa = aux;
	usbd_status		r;
	char *devname = sc->sc.sc_bus.bdev.dv_xname;

	sc->sc.iot = aa->aa_iot;
	sc->sc.sc_bus.dmatag = aa->aa_dmat;
	sc->sc.sc_size = aa->aa_dev->mem[0].size;

	/* Map I/O space */
	if (bus_space_map(sc->sc.iot, aa->aa_dev->mem[0].addr,
		aa->aa_dev->mem[0].size, 0, &sc->sc.ioh)) {
		printf(": cannot map mem space\n");
		goto out;
	}

	if (bus_space_map(sc->sc.iot, aa->aa_dev->mem[1].addr,
		aa->aa_dev->mem[1].size, 0, &sc->uh_ioh)) {
		printf(": cannot map mem space\n");
		goto mem0;
	}

	if (bus_space_map(sc->sc.iot, aa->aa_dev->mem[2].addr,
		aa->aa_dev->mem[2].size, 0, &sc->ph_ioh)) {
		printf(": cannot map mem space\n");
		goto mem1;
	}

	if (bus_space_map(sc->sc.iot, aa->aa_dev->mem[3].addr,
		aa->aa_dev->mem[3].size, 0, &sc->nc_ioh)) {
		printf(": cannot map mem space\n");
		goto mem2;
	}

	printf("\n");

	/* enable usb port power */
	switch (board_id)
	{
	case BOARD_ID_IMX6_PHYFLEX:
		imxgpio_set_dir(EHCI_PHYFLEX_USB_H1_PWR, IMXGPIO_DIR_OUT);
		delay(10);
		imxgpio_set_bit(EHCI_PHYFLEX_USB_H1_PWR);
		delay(10);
		break;
	case BOARD_ID_IMX6_CUBOXI:
	case BOARD_ID_IMX6_HUMMINGBOARD:
		imxgpio_set_bit(EHCI_HUMMINGBOARD_USB_H1_PWR);
		imxgpio_set_dir(EHCI_HUMMINGBOARD_USB_H1_PWR, IMXGPIO_DIR_OUT);
		delay(10);
		break;
	case BOARD_ID_IMX6_SABRELITE:
		imxgpio_clear_bit(EHCI_NITROGEN6X_USB_HUB_RST);
		imxgpio_set_dir(EHCI_NITROGEN6X_USB_HUB_RST, IMXGPIO_DIR_OUT);
		delay(1000 * 2);
		imxgpio_set_bit(EHCI_NITROGEN6X_USB_HUB_RST);
		delay(10);
		break;
	case BOARD_ID_IMX6_SABRESD:
		imxgpio_set_bit(EHCI_SABRESD_USB_PWR);
		imxgpio_set_dir(EHCI_SABRESD_USB_PWR, IMXGPIO_DIR_OUT);
		delay(10);
		break;
	case BOARD_ID_IMX6_UTILITE:
		imxgpio_clear_bit(EHCI_UTILITE_USB_HUB_RST);
		imxgpio_set_dir(EHCI_UTILITE_USB_HUB_RST, IMXGPIO_DIR_OUT);
		delay(10);
		imxgpio_set_bit(EHCI_UTILITE_USB_HUB_RST);
		delay(1000);
		break;
	}

	imxccm_enable_usboh3();
	delay(1000);
	/* disable the carger detection, else signal on DP will be poor */
	imxccm_disable_usb2_chrg_detect();
	/* power host 1 */
	imxccm_enable_pll_usb2();

	/* over current and polarity setting */
	bus_space_write_4(sc->sc.iot, sc->nc_ioh, USBNC_USB_UH1_CTRL,
	    bus_space_read_4(sc->sc.iot, sc->nc_ioh, USBNC_USB_UH1_CTRL) |
	    (USBNC_USB_UH1_CTRL_OVER_CUR_POL | USBNC_USB_UH1_CTRL_OVER_CUR_DIS));

	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_CLR,
	    USBPHY_CTRL_CLKGATE);

	/* Disable interrupts, so we don't get any spurious ones. */
	sc->sc.sc_offs = EREAD1(&sc->sc, EHCI_CAPLENGTH);
	EOWRITE2(&sc->sc, EHCI_USBINTR, 0);

	/* Stop then Reset */
	uint32_t val = EOREAD4(&sc->sc, EHCI_USBCMD);
	val &= ~EHCI_CMD_RS;
	EOWRITE4(&sc->sc, EHCI_USBCMD, val);

	while (EOREAD4(&sc->sc, EHCI_USBCMD) & EHCI_CMD_RS)
		;

	val = EOREAD4(&sc->sc, EHCI_USBCMD);
	val |= EHCI_CMD_HCRESET;
	EOWRITE4(&sc->sc, EHCI_USBCMD, val);

	while (EOREAD4(&sc->sc, EHCI_USBCMD) & EHCI_CMD_HCRESET)
		;

	/* Reset USBPHY module */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_SET, USBPHY_CTRL_SFTRST);

	delay(10);

	/* Remove CLKGATE and SFTRST */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_CLR,
	    USBPHY_CTRL_CLKGATE | USBPHY_CTRL_SFTRST);

	delay(10);

	/* Power up the PHY */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_PWD, 0);

	/* enable FS/LS device */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_SET,
	    USBPHY_CTRL_ENUTMILEVEL2 | USBPHY_CTRL_ENUTMILEVEL3);

	/* set host mode */
	EWRITE4(&sc->sc, EHCI_USBMODE,
	    EREAD4(&sc->sc, EHCI_USBMODE) | EHCI_USBMODE_HOST);

	/* set to UTMI mode */
	EOWRITE4(&sc->sc, EHCI_PORTSC(1),
	    EOREAD4(&sc->sc, EHCI_PORTSC(1)) & ~EHCI_PS_PTS_UTMI_MASK);

	sc->sc_ih = arm_intr_establish(aa->aa_dev->irq[0], IPL_USB,
	    ehci_intr, &sc->sc, devname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto mem3;
	}

	strlcpy(sc->sc.sc_vendor, "i.MX6", sizeof(sc->sc.sc_vendor));
	r = ehci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", devname, r);
		goto intr;
	}

	config_found(self, &sc->sc.sc_bus, usbctlprint);

	goto out;

intr:
	arm_intr_disestablish(sc->sc_ih);
	sc->sc_ih = NULL;
mem3:
	bus_space_unmap(sc->sc.iot, sc->nc_ioh, aa->aa_dev->mem[3].addr);
mem2:
	bus_space_unmap(sc->sc.iot, sc->ph_ioh, aa->aa_dev->mem[2].addr);
mem1:
	bus_space_unmap(sc->sc.iot, sc->uh_ioh, aa->aa_dev->mem[1].addr);
mem0:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
	sc->sc.sc_size = 0;
out:
	return;
}

int
imxehci_detach(struct device *self, int flags)
{
	struct imxehci_softc		*sc = (struct imxehci_softc *)self;
	int				rv;

	rv = ehci_detach(self, flags);
	if (rv)
		return (rv);

	if (sc->sc_ih != NULL) {
		arm_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc.sc_size) {
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}

	return (0);
}
