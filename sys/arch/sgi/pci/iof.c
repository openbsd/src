/*	$OpenBSD: iof.c,v 1.8 2011/10/10 19:49:16 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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

/*
 * IOC4 device driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <sgi/pci/iofreg.h>
#include <sgi/pci/iofvar.h>

int	iof_match(struct device *, void *, void *);
void	iof_attach(struct device *, struct device *, void *);
void	iof_attach_child(struct device *, const char *, bus_addr_t, uint);
int	iof_search(struct device *, void *, void *);
int	iof_print(void *, const char *);

struct iof_intr {
	struct iof_softc	*ii_iof;

	int			 (*ii_func)(void *);
	void			*ii_arg;

	struct evcount		 ii_count;
	int			 ii_level;
};

struct iof_softc {
	struct device		 sc_dev;

	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_dma_tag_t		 sc_dmat;
	pci_chipset_tag_t	 sc_pc;
	pcitag_t		 sc_tag;

	uint32_t		 sc_mcr;

	void			*sc_ih;	
	struct iof_intr		*sc_intr[IOC4_NDEVS];
};

struct cfattach iof_ca = {
	sizeof(struct iof_softc), iof_match, iof_attach,
};

struct cfdriver iof_cd = {
	NULL, "iof", DV_DULL,
};

int	iof_intr_dispatch(struct iof_softc *, int);
int	iof_intr(void *);

int
iof_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SGI &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SGI_IOC4)
		return 1;

	return 0;
}

int
iof_print(void *aux, const char *iofname)
{
	struct iof_attach_args *iaa = aux;

	if (iofname != NULL)
		printf("%s at %s", iaa->iaa_name, iofname);

	printf(" base 0x%x", iaa->iaa_base);

	return UNCONF;
}

void
iof_attach(struct device *parent, struct device *self, void *aux)
{
	struct iof_softc *sc = (struct iof_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_size_t memsize;
	const char *intrstr;

	printf(": ");

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM, 0,
	    &memt, &memh, NULL, &memsize, 0)) {
		printf("can't map mem space\n");
		return;
	}

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	sc->sc_memt = memt;
	sc->sc_memh = memh;

	sc->sc_mcr = bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC4_MCR);

	/*
	 * Acknowledge all pending interrupts, and disable them.
	 */

	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IEC, ~0x0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IES, 0x0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IR,
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IR));

	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IEC, ~0x0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IES, 0x0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IR,
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IR));

	if (pci_intr_map(pa, &ih) != 0) {
		printf("failed to map interrupt!\n");
		goto unmap;
	}
	intrstr = pci_intr_string(sc->sc_pc, ih);

	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_TTY, iof_intr,
	    sc, self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf("failed to establish interrupt at %s\n", intrstr);
		goto unmap;
	}
	printf("%s\n", intrstr);

	/*
	 * Attach other sub-devices.
	 */

	iof_attach_child(self, "com", IOC4_UARTA_BASE, IOC4DEV_SERIAL_A);
	iof_attach_child(self, "com", IOC4_UARTB_BASE, IOC4DEV_SERIAL_B);
	iof_attach_child(self, "com", IOC4_UARTC_BASE, IOC4DEV_SERIAL_C);
	iof_attach_child(self, "com", IOC4_UARTD_BASE, IOC4DEV_SERIAL_D);
	iof_attach_child(self, "iockbc", IOC4_KBC_BASE, IOC4DEV_KBC);
	iof_attach_child(self, "dsrtc", IOC4_BYTEBUS_0, IOC4DEV_RTC);

	return;

unmap:
	bus_space_unmap(memt, memh, memsize);
}

void
iof_attach_child(struct device *iof, const char *name, bus_addr_t base,
    uint dev)
{
	struct iof_softc *sc = (struct iof_softc *)iof;
	struct iof_attach_args iaa;

	iaa.iaa_name = name;
	pci_get_device_location(sc->sc_pc, sc->sc_tag, &iaa.iaa_location);
	iaa.iaa_memt = sc->sc_memt;
	iaa.iaa_memh = sc->sc_memh;
	iaa.iaa_dmat = sc->sc_dmat;
	iaa.iaa_base = base;
	iaa.iaa_dev = dev;
	iaa.iaa_clock = sc->sc_mcr & IOC4_MCR_PCI_66MHZ ?  66666667 : 33333333;

	config_found_sm(iof, &iaa, iof_print, iof_search);
}

int
iof_search(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct iof_attach_args *iaa = (struct iof_attach_args *)args;

	if (strcmp(cf->cf_driver->cd_name, iaa->iaa_name) != 0)
		return 0;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != (int)iaa->iaa_base)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, iaa);
}

/*
 * Interrupt handling.
 */

/*
 * List of interrupt bits to enable for each device.
 *
 * For the serial ports, we only enable the passthrough interrupt and
 * let com(4) tinker with the appropriate registers, instead of adding
 * an unnecessary layer there.
 */
static const struct {
	uint32_t sio;
	uint32_t other;
} ioc4_intrbits[IOC4_NDEVS] = {
	{ IOC4_SIRQ_UARTA, 0 },
	{ IOC4_SIRQ_UARTB, 0 },
	{ IOC4_SIRQ_UARTC, 0 },
	{ IOC4_SIRQ_UARTD, 0 },
	{ 0, IOC4_OIRQ_KBC },
	{ 0, IOC4_OIRQ_ATAPI },
	{ 0, 0 }	/* no RTC interrupt */
};

void *
iof_intr_establish(void *cookie, uint dev, int level, int (*func)(void *),
    void *arg, char *name)
{
	struct iof_softc *sc = cookie;
	struct iof_intr *ii;

	if (dev < 0 || dev >= IOC4_NDEVS)
		return NULL;

	if (ioc4_intrbits[dev].sio == 0 && ioc4_intrbits[dev].other == 0)
		return NULL;

	ii = (struct iof_intr *)malloc(sizeof(*ii), M_DEVBUF, M_NOWAIT);
	if (ii == NULL)
		return NULL;

	ii->ii_iof = sc;
	ii->ii_func = func;
	ii->ii_arg = arg;
	ii->ii_level = level;

	evcount_attach(&ii->ii_count, name, &ii->ii_level);
	sc->sc_intr[dev] = ii;

	/* enable hardware source if necessary */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IES,
	    ioc4_intrbits[dev].sio);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IES,
	    ioc4_intrbits[dev].other);
	
	return (ii);
}

int
iof_intr(void *v)
{
	struct iof_softc *sc = (struct iof_softc *)v;
	uint32_t spending, opending, mask;
	int dev;

	spending = bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IR) &
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IES);
	opending = bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IR) &
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IES);

	if (spending == 0 && opending == 0)
		return 0;

	/* Disable pending interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_SIO_IEC, spending);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC4_OTHER_IEC, opending);

	for (dev = 0; dev < IOC4_NDEVS; dev++) {
		mask = spending & ioc4_intrbits[dev].sio;
		if (mask != 0) {
			(void)iof_intr_dispatch(sc, dev);

			/* Ack, then reenable, pending interrupts */
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC4_SIO_IR, mask);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC4_SIO_IES, mask);
		}
		mask = opending & ioc4_intrbits[dev].other;
		if (mask != 0) {
			(void)iof_intr_dispatch(sc, dev);

			/* Ack, then reenable, pending interrupts */
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC4_OTHER_IR, mask);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC4_OTHER_IES, mask);
		}
	}

	return 1;
}

int
iof_intr_dispatch(struct iof_softc *sc, int dev)
{
	struct iof_intr *ii;
	int rc = 0;

	/* Call registered interrupt function. */
	if ((ii = sc->sc_intr[dev]) != NULL && ii->ii_func != NULL) {
		rc = (*ii->ii_func)(ii->ii_arg);
		if (rc != 0)
			ii->ii_count.ec_count++;
	}

	return rc;
}
