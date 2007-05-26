/*	$OpenBSD: if_che.c,v 1.1 2007/05/26 17:39:53 claudio Exp $ */

/*
 * Copyright (c) 2007 Claudio Jeker <claudio@openbsd.org>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#define DEVNAME(_sc)	((_sc)->sc_dev.dv_xname)


struct cheg_softc {
	struct device		sc_dev;

	bus_dma_tag_t		sc_dmat;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_mems;

	u_int32_t		sc_rev;	/* card revision */
};

struct che_softc {
	struct device		sc_dev;
	struct arpcom		sc_ac;
	struct ifmedia		sc_media;

	void			*sc_if;
};

/* function protos */
int		cheg_match(struct device *, void *, void *);
void		cheg_attach(struct device *, struct device *, void *);
int		cheg_print(void *, const char *);
int		che_match(struct device *, void *, void *);
void		che_attach(struct device *, struct device *, void *);
u_int32_t 	che_read(struct cheg_softc *, bus_size_t);
void		che_write(struct cheg_softc *, bus_size_t, u_int32_t);
int		che_waitfor(struct cheg_softc *, bus_size_t, u_int32_t, int);
int		che_write_flash_reg(struct cheg_softc *, size_t, int,
		    u_int32_t);
int		che_read_flash_reg(struct cheg_softc *, size_t, int,
		    u_int32_t *);
int		che_read_flash_multi4(struct cheg_softc *, u_int, u_int32_t *,
		    size_t);
void		che_reset(struct cheg_softc *);

/* registers & defines */
#define CHE_PCI_BAR		0x10

#define CHE_REG_PL_RST		0x6f0
#define CHE_RST_F_CRSTWRM	0x2
#define CHE_RST_F_CRSTWRMMODE	0x1
#define CHE_REG_PL_REV		0x6f4

/* serial flash and firmware definitions */
#define CHE_REG_SF_DATA		0x6d8
#define CHE_REG_SF_OP		0x6dc
#define CHE_SF_SEC_SIZE		(64 * 1024)	/* serial flash sector size */
#define CHE_SF_SIZE		(8 * CHE_SF_SEC_SIZE)	/* serial flash size */
#define CHE_SF_PROG_PAGE	2
#define CHE_SF_WR_DISABLE	4
#define CHE_SF_RD_STATUS	5	/* read status register */
#define CHE_SF_WR_ENABLE	6
#define CHE_SF_RD_DATA		11
#define CHE_SF_SEC_ERASE	216
#define CHE_SF_F_BUSY		(1U << 31)
#define CHE_SF_F_OP		0x1
#define CHE_SF_CONT(_x)		((_x) << 3)
#define CHE_SF_BYTECNT_MASK	0x3
#define CHE_SF_BYTECNT(_x)	(((_x) & CHE_SF_BYTECNT_MASK) << 1)

#define FW_FLASH_BOOT_ADDR	0x70000	/* start address of FW in flash */
#define FW_VERS_ADDR		0x77ffc	/* flash address holding FW version */
#define FW_VERS_TYPE_N3		0
#define FW_VERS_TYPE_T3		1
#define FW_VERS_TYPE(_x)	(((_x) >> 28) & 0xf)
#define FW_VERS_MAJOR(_x)	(((_x) >> 16) & 0xfff)
#define FW_VERS_MINOR(_x)	(((_x) >> 8) & 0xff)
#define FW_VERS_MICRO(_x)	((_x) & 0xff)

struct cfattach cheg_ca = {
	sizeof(struct cheg_softc), cheg_match, cheg_attach
};

struct cfdriver cheg_cd = {
	NULL, "cheg", DV_DULL
};

/* glue between the controller and the port */
struct che_attach_args {
	struct pci_attach_args	*caa_pa;
	pci_intr_handle_t	caa_ih;
	int			caa_port;
};

struct cfattach che_ca = {
	sizeof(struct che_softc), che_match, che_attach
};

struct cfdriver che_cd = {
	NULL, "che", DV_IFNET
};

struct cheg_device {
	pci_vendor_id_t	cd_vendor;
	pci_vendor_id_t	cd_product;
	u_int		cd_nports;
};

const struct cheg_device *cheg_lookup(struct pci_attach_args *);

const struct cheg_device che_devices[] = {
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_PE9000, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T302E, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T302X, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T310E, 1 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T310X, 1 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T320E, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T320X, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T3B02, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T3B10, 1 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T3B20, 2 }
};

const struct cheg_device *
cheg_lookup(struct pci_attach_args *pa)
{
	int i;
	const struct cheg_device *cd;

	for (i = 0; i < sizeof(che_devices)/sizeof(che_devices[0]); i++) {
		cd = &che_devices[i];
		if (cd->cd_vendor == PCI_VENDOR(pa->pa_id) &&
		    cd->cd_product == PCI_PRODUCT(pa->pa_id))
			return (cd);
	}

	return (NULL);
}

int
cheg_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (cheg_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
cheg_attach(struct device *parent, struct device *self, void *aux)
{
	struct cheg_softc *sc = (struct cheg_softc *)self;
	struct pci_attach_args *pa = aux;
	const struct cheg_device *cd;
	struct che_attach_args caa;
	pcireg_t memtype;
	u_int32_t vers;
	u_int i;

	bzero(&caa, sizeof(caa));
	cd = cheg_lookup(pa);

	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, CHE_PCI_BAR);
	if (pci_mapreg_map(pa, CHE_PCI_BAR, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0) != 0) {
		printf(": unable to map host registers\n");
		return;
	}

	if (pci_intr_map(pa, &caa.caa_ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	printf(": %s", pci_intr_string(pa->pa_pc, caa.caa_ih));

	sc->sc_rev = che_read(sc, CHE_REG_PL_REV);

	/* reset the beast */
	che_reset(sc);

	che_read_flash_multi4(sc, FW_VERS_ADDR, &vers, 1);
	printf(", rev %d, fw %d-%d.%d.%d\n", sc->sc_rev, FW_VERS_TYPE(vers),
	    FW_VERS_MAJOR(vers), FW_VERS_MINOR(vers), FW_VERS_MICRO(vers));

	caa.caa_pa = pa;
	for (i = 0; i < cd->cd_nports; i++) {
		caa.caa_port = i;

		config_found(self, &caa, cheg_print);
	}

	return;

unmap:   
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
cheg_print(void *aux, const char *pnp)
{
	struct che_attach_args *caa = aux;

	if (pnp != NULL)
		printf("\"%s\" at %s", che_cd.cd_name, pnp);

	printf(" port %d", caa->caa_port);

	return (UNCONF);
}

int
che_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
che_attach(struct device *parent, struct device *self, void *aux)
{
	printf(": not done yet\n");
	return;
}

u_int32_t
che_read(struct cheg_softc *sc, bus_size_t r)
{
        bus_space_barrier(sc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_memh, r));
}

void
che_write(struct cheg_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_memt, sc->sc_memh, r, v);
        bus_space_barrier(sc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
che_waitfor(struct cheg_softc *sc, bus_size_t r, u_int32_t mask, int tries)
{
	u_int32_t v;
	int i;

	for (i = 0; i < tries; i++) {
		v = che_read(sc, r);
		if ((v & mask) == 0)
			return (0);
		delay(10);
	}
	return (EAGAIN);
}

int
che_write_flash_reg(struct cheg_softc *sc, size_t bcnt, int cont, u_int32_t v)
{
	if (che_read(sc, CHE_REG_SF_OP) & CHE_SF_F_BUSY)
		return (EBUSY);

	che_write(sc, CHE_REG_SF_DATA, v);
	che_write(sc, CHE_REG_SF_OP, CHE_SF_CONT(cont) |
	    CHE_SF_BYTECNT(bcnt - 1) | CHE_SF_F_OP);

	return (che_waitfor(sc, CHE_REG_SF_OP, CHE_SF_F_BUSY, 5));
}

int
che_read_flash_reg(struct cheg_softc *sc, size_t bcnt, int cont, u_int32_t *vp)
{
	if (che_read(sc, CHE_REG_SF_OP) & CHE_SF_F_BUSY)
		return (EBUSY);

	che_write(sc, CHE_REG_SF_OP, CHE_SF_CONT(cont) |
	    CHE_SF_BYTECNT(bcnt - 1));

	if (che_waitfor(sc, CHE_REG_SF_OP, CHE_SF_F_BUSY, 5))
		return (EAGAIN);

	*vp = che_read(sc, CHE_REG_SF_DATA);
	return (0);
}

int
che_read_flash_multi4(struct cheg_softc *sc, u_int addr, u_int32_t *datap,
	size_t count)
{
	int rv;

	if (addr + count * sizeof(u_int32_t) > CHE_SF_SIZE || (addr & 3))
		panic("%s: che_read_flash_multi4 bad params\n", DEVNAME(sc));

	addr = swap32(addr) | CHE_SF_RD_DATA;

	if ((rv = che_write_flash_reg(sc, 4, 1, addr)))
		return (rv);
	if ((rv = che_read_flash_reg(sc, 1, 1, datap)))
		return (rv);

	while (count) {
		if ((rv = che_read_flash_reg(sc, 4, count > 1, datap)))
			return (rv);
		count--;
		datap++;
	}
	return (0);
}

void
che_reset(struct cheg_softc *sc)
{
	che_write(sc, CHE_REG_PL_RST, CHE_RST_F_CRSTWRM |
	    CHE_RST_F_CRSTWRMMODE);

	/* Give the card some time to boot */
	delay(500);
}
