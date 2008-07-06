/*	$OpenBSD: sbbc.c,v 1.1 2008/07/06 07:27:43 kettenis Exp $	*/
/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/clock_subr.h>

extern todr_chip_handle_t todr_handle;

#define SBBC_PCI_BAR	PCI_MAPREG_START

#define SBBC_REGS_OFFSET	0x800000
#define SBBC_REGS_SIZE		0x6230
#define SBBC_SRAM_OFFSET	0x900000
#define SBBC_SRAM_SIZE		0x20000	/* 128KB SRAM */

#define SBBC_MAX_TAGS	32

struct sbbc_sram_tag {
	char		tag_key[8];
	uint32_t	tag_size;
	uint32_t	tag_offset;
};

struct sbbc_sram_toc {
	char			toc_magic[8];
	uint8_t			toc_reserved;
	uint8_t			toc_type;
	uint16_t		toc_version;
	uint32_t		toc_ntags;
	struct sbbc_sram_tag 	toc_tag[SBBC_MAX_TAGS];
};

/* Time of day service. */
struct sbbc_sram_tod {
	uint32_t	tod_magic;
	uint32_t	tod_version;
	uint64_t	tod_time;
	uint64_t	tod_skew;
	uint32_t	tod_reserved;
	uint32_t	tod_heartbeat;
	uint32_t	tod_timeout;
};

#define SBBC_TOD_MAGIC		0x54443100
#define SBBC_TOD_VERSION	1

struct sbbc_softc {
	struct device		sc_dv;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_reg_ioh;
	bus_space_handle_t	sc_sram_ioh;
	caddr_t			sc_sram;
	uint32_t		sc_sram_toc;

	struct sparc_bus_space_tag sc_bbt;
};

int	sbbc_match(struct device *, void *, void *);
void	sbbc_attach(struct device *, struct device *, void *);

struct cfattach sbbc_ca = {
	sizeof(struct sbbc_softc), sbbc_match, sbbc_attach
};

struct cfdriver sbbc_cd = {
	NULL, "sbbc", DV_DULL
};

void	sbbc_attach_tod(struct sbbc_softc *, uint32_t);
int	sbbc_tod_gettime(todr_chip_handle_t, struct timeval *);
int	sbbc_tod_settime(todr_chip_handle_t, struct timeval *);

int
sbbc_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_SBBC))
		return (1);

	return (0);
}

void
sbbc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sbbc_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	struct sbbc_sram_toc *toc;
	bus_addr_t base;
	bus_size_t size;
	int chosen, iosram;
	int i;

	/* XXX Don't byteswap. */
	sc->sc_bbt = *pa->pa_memt;
	sc->sc_bbt.sasi = ASI_PRIMARY;
	sc->sc_iot = &sc->sc_bbt;

	if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, SBBC_PCI_BAR,
	    PCI_MAPREG_TYPE_MEM, &base, &size, NULL)) {
		printf(": can't find register space\n");
		return;
	}

	if (bus_space_map(sc->sc_iot, base + SBBC_SRAM_OFFSET,
	    SBBC_SRAM_SIZE, 0, &sc->sc_sram_ioh)) {
		printf(": can't map SRAM\n");
		return;
	}

	printf("\n");

	/* Check if we are the chosen one. */
	chosen = OF_finddevice("/chosen");
	if (OF_getprop(chosen, "iosram", &iosram, sizeof(iosram)) <= 0 ||
	    PCITAG_NODE(pa->pa_tag) != iosram)
		return;

	/* SRAM TOC offset defaults to 0. */
	if (OF_getprop(chosen, "iosram-toc", &sc->sc_sram_toc,
	    sizeof(sc->sc_sram_toc)) <= 0)
		sc->sc_sram_toc = 0;

	sc->sc_sram = bus_space_vaddr(sc->sc_iot, sc->sc_sram_ioh);
	toc = (struct sbbc_sram_toc *)(sc->sc_sram + sc->sc_sram_toc);

	for (i = 0; i < toc->toc_ntags; i++) {
		if (strcmp(toc->toc_tag[i].tag_key, "TODDATA") == 0)
			sbbc_attach_tod(sc, toc->toc_tag[i].tag_offset);
	}
}

void
sbbc_attach_tod(struct sbbc_softc *sc, uint32_t offset)
{
	struct sbbc_sram_tod *tod;
	todr_chip_handle_t handle;

	tod = (struct sbbc_sram_tod *)(sc->sc_sram + offset);
	if (tod->tod_magic != SBBC_TOD_MAGIC ||
	    tod->tod_version < SBBC_TOD_VERSION)
		return;

	handle = malloc(sizeof(struct todr_chip_handle), M_DEVBUF, M_NOWAIT);
	if (handle == NULL)
		panic("couldn't allocate todr_handle");

	handle->cookie = tod;
	handle->todr_gettime = sbbc_tod_gettime;
	handle->todr_settime = sbbc_tod_settime;

	handle->bus_cookie = NULL;
	handle->todr_setwen = NULL;
	todr_handle = handle;
}

int
sbbc_tod_gettime(todr_chip_handle_t handle, struct timeval *tv)
{
	struct sbbc_sram_tod *tod = handle->cookie;

	tv->tv_sec = tod->tod_time + tod->tod_skew;
	tv->tv_usec = 0;
	return (0);
}

int
sbbc_tod_settime(todr_chip_handle_t handle, struct timeval *tv)
{
	struct sbbc_sram_tod *tod = handle->cookie;

	tod->tod_skew = tv->tv_sec - tod->tod_time;
	return (0);
}
