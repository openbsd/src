/*	$OpenBSD: virtio_pci.c,v 1.9 2015/03/14 03:38:49 jsg Exp $	*/
/*	$NetBSD: virtio.c,v 1.3 2011/11/02 23:05:52 njoly Exp $	*/

/*
 * Copyright (c) 2012 Stefan Fritsch.
 * Copyright (c) 2010 Minoura Makoto.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mutex.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/virtioreg.h>
#include <dev/pci/virtiovar.h>

/*
 * XXX: Before being used on big endian arches, the access to config registers
 * XXX: needs to be reviewed/fixed. The non-device specific registers are
 * XXX: PCI-endian while the device specific registers are native endian.
 */

#define virtio_set_status(sc, s) virtio_pci_set_status(sc, s)
#define virtio_device_reset(sc) virtio_set_status((sc), 0)

int		virtio_pci_match(struct device *, void *, void *);
void		virtio_pci_attach(struct device *, struct device *, void *);
int		virtio_pci_detach(struct device *, int);

void		virtio_pci_kick(struct virtio_softc *, uint16_t);
uint8_t		virtio_pci_read_device_config_1(struct virtio_softc *, int);
uint16_t	virtio_pci_read_device_config_2(struct virtio_softc *, int);
uint32_t	virtio_pci_read_device_config_4(struct virtio_softc *, int);
uint64_t	virtio_pci_read_device_config_8(struct virtio_softc *, int);
void		virtio_pci_write_device_config_1(struct virtio_softc *, int, uint8_t);
void		virtio_pci_write_device_config_2(struct virtio_softc *, int, uint16_t);
void		virtio_pci_write_device_config_4(struct virtio_softc *, int, uint32_t);
void		virtio_pci_write_device_config_8(struct virtio_softc *, int, uint64_t);
uint16_t	virtio_pci_read_queue_size(struct virtio_softc *, uint16_t);
void		virtio_pci_setup_queue(struct virtio_softc *, uint16_t, uint32_t);
void		virtio_pci_set_status(struct virtio_softc *, int);
uint32_t	virtio_pci_negotiate_features(struct virtio_softc *, uint32_t,
					      const struct virtio_feature_name *);
int		virtio_pci_intr(void *);

struct virtio_pci_softc {
	struct virtio_softc	sc_sc;
	pci_chipset_tag_t	sc_pc;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_iosize;

	void                    *sc_ih;

	int			sc_config_offset;
};

struct cfattach virtio_pci_ca = {
	sizeof(struct virtio_pci_softc),
	virtio_pci_match,
	virtio_pci_attach,
	virtio_pci_detach,
	NULL
};

struct virtio_ops virtio_pci_ops = {
	virtio_pci_kick,
	virtio_pci_read_device_config_1,
	virtio_pci_read_device_config_2,
	virtio_pci_read_device_config_4,
	virtio_pci_read_device_config_8,
	virtio_pci_write_device_config_1,
	virtio_pci_write_device_config_2,
	virtio_pci_write_device_config_4,
	virtio_pci_write_device_config_8,
	virtio_pci_read_queue_size,
	virtio_pci_setup_queue,
	virtio_pci_set_status,
	virtio_pci_negotiate_features,
	virtio_pci_intr,
};

uint16_t
virtio_pci_read_queue_size(struct virtio_softc *vsc, uint16_t idx)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, VIRTIO_CONFIG_QUEUE_SELECT,
	    idx);
	return bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    VIRTIO_CONFIG_QUEUE_SIZE);
}

void
virtio_pci_setup_queue(struct virtio_softc *vsc, uint16_t idx, uint32_t addr)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, VIRTIO_CONFIG_QUEUE_SELECT,
	    idx);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VIRTIO_CONFIG_QUEUE_ADDRESS,
	    addr);
}

void
virtio_pci_set_status(struct virtio_softc *vsc, int status)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	int old = 0;

	if (status != 0)
		old = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
				       VIRTIO_CONFIG_DEVICE_STATUS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIRTIO_CONFIG_DEVICE_STATUS,
			  status|old);
}

int
virtio_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_QUMRANET &&
	    PCI_PRODUCT(pa->pa_id) >= 0x1000 &&
	    PCI_PRODUCT(pa->pa_id) <= 0x103f &&
	    PCI_REVISION(pa->pa_class) == 0)
		return 1;
	return 0;
}

void
virtio_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)self;
	struct virtio_softc *vsc = &sc->sc_sc;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	int revision;
	pcireg_t id;
	char const *intrstr;
	pci_intr_handle_t ih;

	revision = PCI_REVISION(pa->pa_class);
	if (revision != 0) {
		printf("unknown revision 0x%02x; giving up\n", revision);
		return;
	}

	/* subsystem ID shows what I am */
	id = PCI_PRODUCT(pci_conf_read(pc, tag, PCI_SUBSYS_ID_REG));
	printf(": Virtio %s Device", virtio_device_string(id));

#ifdef notyet
	if (pci_get_capability(pc, tag, PCI_CAP_MSIX, NULL, NULL))
		printf(", msix capable");
#endif
	printf("\n");

	vsc->sc_ops = &virtio_pci_ops;
	sc->sc_pc = pc;
	vsc->sc_dmat = pa->pa_dmat;
	sc->sc_config_offset = VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI;

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_IO, 0,
			   &sc->sc_iot, &sc->sc_ioh, NULL, &sc->sc_iosize, 0)) {
		printf("%s: can't map i/o space\n", vsc->sc_dev.dv_xname);
		return;
	}

	virtio_device_reset(vsc);
	virtio_pci_set_status(vsc, VIRTIO_CONFIG_DEVICE_STATUS_ACK);
	virtio_pci_set_status(vsc, VIRTIO_CONFIG_DEVICE_STATUS_DRIVER);

	/* XXX: use softc as aux... */
	vsc->sc_childdevid = id;
	vsc->sc_child = NULL;
	config_found(self, sc, NULL);
	if (vsc->sc_child == NULL) {
		printf("%s: no matching child driver; not configured\n", vsc->sc_dev.dv_xname);
		goto fail_1;
	}
	if (vsc->sc_child == VIRTIO_CHILD_ERROR) {
		printf("%s: virtio configuration failed\n", vsc->sc_dev.dv_xname);
		goto fail_1;
	}

	if (pci_intr_map_msi(pa, &ih) != 0 && pci_intr_map(pa, &ih) != 0) {
		printf("%s: couldn't map interrupt\n", vsc->sc_dev.dv_xname);
		goto fail_2;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, vsc->sc_ipl, virtio_pci_intr, sc, vsc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt", vsc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail_2;
	}
	printf("%s: %s\n", vsc->sc_dev.dv_xname, intrstr);

	virtio_set_status(vsc, VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK);
	return;

fail_2:
	config_detach(vsc->sc_child, 0);
fail_1:
	/* no pci_mapreg_unmap() or pci_intr_unmap() */
	virtio_set_status(vsc, VIRTIO_CONFIG_DEVICE_STATUS_FAILED);
}

int
virtio_pci_detach(struct device *self, int flags)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)self;
	struct virtio_softc *vsc = &sc->sc_sc;
	int r;

	if (vsc->sc_child != 0 && vsc->sc_child != VIRTIO_CHILD_ERROR) {
		r = config_detach(vsc->sc_child, flags);
		if (r)
			return r;
	}
	KASSERT(vsc->sc_child == 0 || vsc->sc_child == VIRTIO_CHILD_ERROR);
	KASSERT(vsc->sc_vqs == 0);
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	sc->sc_ih = 0;
	if (sc->sc_iosize)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
	sc->sc_iosize = 0;

	return 0;
}

/*
 * Feature negotiation.
 * Prints available / negotiated features if guest_feature_names != NULL and
 * VIRTIO_DEBUG is 1
 */
uint32_t
virtio_pci_negotiate_features(struct virtio_softc *vsc, uint32_t guest_features,
			  const struct virtio_feature_name *guest_feature_names)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	uint32_t host, neg;

	/*
	 * indirect descriptors can be switched off by setting bit 1 in the
	 * driver flags, see config(8)
	 */
	if (!(vsc->sc_dev.dv_cfdata->cf_flags & 1) &&
	    !(vsc->sc_child->dv_cfdata->cf_flags & 1)) {
		guest_features |= VIRTIO_F_RING_INDIRECT_DESC;
	} else {
		printf("RingIndirectDesc disabled by UKC\n");
	}
	host = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				VIRTIO_CONFIG_DEVICE_FEATURES);
	neg = host & guest_features;
#if VIRTIO_DEBUG
	if (guest_feature_names)
		virtio_log_features(host, neg, guest_feature_names);
#endif
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			  VIRTIO_CONFIG_GUEST_FEATURES, neg);
	vsc->sc_features = neg;
	if (neg & VIRTIO_F_RING_INDIRECT_DESC)
		vsc->sc_indirect = 1;
	else
		vsc->sc_indirect = 0;

	return neg;
}

/*
 * Device configuration registers.
 */
uint8_t
virtio_pci_read_device_config_1(struct virtio_softc *vsc, int index)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh,
				sc->sc_config_offset + index);
}

uint16_t
virtio_pci_read_device_config_2(struct virtio_softc *vsc, int index)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	return bus_space_read_2(sc->sc_iot, sc->sc_ioh,
				sc->sc_config_offset + index);
}

uint32_t
virtio_pci_read_device_config_4(struct virtio_softc *vsc, int index)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				sc->sc_config_offset + index);
}

uint64_t
virtio_pci_read_device_config_8(struct virtio_softc *vsc, int index)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	uint64_t r;

	r = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			     sc->sc_config_offset + index + sizeof(uint32_t));
	r <<= 32;
	r += bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			      sc->sc_config_offset + index);
	return r;
}

void
virtio_pci_write_device_config_1(struct virtio_softc *vsc,
			     int index, uint8_t value)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			  sc->sc_config_offset + index, value);
}

void
virtio_pci_write_device_config_2(struct virtio_softc *vsc,
			     int index, uint16_t value)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			  sc->sc_config_offset + index, value);
}

void
virtio_pci_write_device_config_4(struct virtio_softc *vsc,
			     int index, uint32_t value)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			  sc->sc_config_offset + index, value);
}

void
virtio_pci_write_device_config_8(struct virtio_softc *vsc,
			     int index, uint64_t value)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			  sc->sc_config_offset + index,
			  value & 0xffffffff);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			  sc->sc_config_offset + index + sizeof(uint32_t),
			  value >> 32);
}

/*
 * Interrupt handler.
 */
int
virtio_pci_intr(void *arg)
{
	struct virtio_pci_softc *sc = arg;
	struct virtio_softc *vsc = &sc->sc_sc;
	int isr, r = 0;

	/* check and ack the interrupt */
	isr = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			       VIRTIO_CONFIG_ISR_STATUS);
	if (isr == 0)
		return 0;
	if ((isr & VIRTIO_CONFIG_ISR_CONFIG_CHANGE) &&
	    (vsc->sc_config_change != NULL))
		r = (vsc->sc_config_change)(vsc);
	if (vsc->sc_intrhand != NULL)
		r |= (vsc->sc_intrhand)(vsc);

	return r;
}

void
virtio_pci_kick(struct virtio_softc *vsc, uint16_t idx)
{
	struct virtio_pci_softc *sc = (struct virtio_pci_softc *)vsc;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, VIRTIO_CONFIG_QUEUE_NOTIFY,
	    idx);
}
