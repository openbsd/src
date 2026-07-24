/*	$OpenBSD: aplic.c,v 1.1 2026/07/24 10:51:00 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#define APLIC_DOMAINCFG			0x0000
#define  APLIC_DOMAINCFG_IE		(1 << 8)
#define  APLIC_DOMAINCFG_DM_MSI		(1 << 2)
#define APLIC_SOURCECFG(x)		(0x0000 + (x) * 4)
#define  APLIC_SOURCECFG_SM_DETACHED	(1 << 0)
#define  APLIC_SOURCECFG_SM_EDGE1	(4 << 0)
#define  APLIC_SOURCECFG_SM_EDGE0	(5 << 0)
#define  APLIC_SOURCECFG_SM_LEVEL1	(6 << 0)
#define  APLIC_SOURCECFG_SM_LEVEL0	(7 << 0)
#define APLIC_SETIP(x)			(0x1c00 + (x) * 4)
#define APLIC_SETIPNUM			0x1cdc
#define APLIC_SETIE(x)			(0x1e00 + (x) * 4)
#define APLIC_SETIENUM			0x1edc
#define APLIC_SETIPNUM_LE		0x2000
#define APLIC_SETIPNUM_BE		0x2004
#define APLIC_GENMSI			0x3000
#define APLIC_TARGET(x)			(0x3000 + (x) * 4)
#define  APLIC_TARGET_HARTID_SHIFT	18

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct aplic_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;

	struct interrupt_controller sc_ic;
};

struct aplic_intrhand {
	struct aplic_softc	*aih_sc;
	void			*aih_cookie;
	uint16_t		aih_source;
	int			(*aih_func)(void *);
	void			*aih_arg;
	int			aih_eoi;
};

int aplic_match(struct device *, void *, void *);
void aplic_attach(struct device *, struct device *, void *);

const struct cfattach aplic_ca = {
	sizeof (struct aplic_softc), aplic_match, aplic_attach
};

struct cfdriver aplic_cd = {
	NULL, "aplic", DV_DULL
};

void	*aplic_intr_establish(void *, int *, int, struct cpu_info *,
	    int (*)(void *), void *, char *);
void	aplic_intr_disestablish(void *);
void	aplic_intr_barrier(void *);

int
aplic_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "riscv,aplic");
}

void
aplic_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplic_softc *sc = (struct aplic_softc *)self;
	struct fdt_attach_args *faa = aux;
	bus_size_t reg;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_node = faa->fa_node;

	printf("\n");

	/* Disable all interrupts before we enable the controller. */
	for (reg = APLIC_SETIE(0); reg <= APLIC_SETIE(31); reg += 4)
		HWRITE4(sc, reg, 0);

	/* Enable the controller in MSI delivery mode. */
	HWRITE4(sc, APLIC_DOMAINCFG,
	    APLIC_DOMAINCFG_IE | APLIC_DOMAINCFG_DM_MSI);

	sc->sc_ic.ic_node = sc->sc_node;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_establish = aplic_intr_establish;
	sc->sc_ic.ic_disestablish = aplic_intr_disestablish;
	sc->sc_ic.ic_barrier = aplic_intr_barrier;
	fdt_intr_register(&sc->sc_ic);
}

int
aplic_intr(void *arg)
{
	struct aplic_intrhand *aih = arg;
	int handled;

	handled = aih->aih_func(aih->aih_arg);

	if (aih->aih_eoi)
		HWRITE4(aih->aih_sc, APLIC_SETIPNUM, aih->aih_source);

	return handled;
}

void *
aplic_intr_establish(void *cookie, int *cells, int level, struct cpu_info *ci,
    int (*func)(void *), void *arg, char *name)
{
	struct aplic_softc *sc = cookie;
	struct aplic_intrhand *aih;
	uint64_t addr, data;
	uint32_t source = cells[0];
	uint32_t sourcecfg, target;

	if (ci == NULL)
		ci = &cpu_info_primary;

	KASSERT(source > 0);
	KASSERT(source < 1024);

	aih = malloc(sizeof(*aih), M_DEVBUF, M_WAITOK | M_ZERO);
	aih->aih_sc = sc;
	aih->aih_source = source;
	aih->aih_func = func;
	aih->aih_arg = arg;

	aih->aih_cookie = fdt_intr_establish_msi_cpu(sc->sc_node, &addr, &data,
	    level, ci, aplic_intr, aih, name);
	if (aih->aih_cookie == NULL) {
		free(aih, M_DEVBUF, sizeof(*aih));
		return NULL;
	}
	
	KASSERT(data > 0);
	KASSERT(data < 2048);

	/* XXX This assumes hart ID and hart index are the same. */
	target = ci->ci_hartid << APLIC_TARGET_HARTID_SHIFT | data;

	switch (cells[1]) {
	case 1:
		sourcecfg = APLIC_SOURCECFG_SM_EDGE1;
		break;
	case 2:
		sourcecfg = APLIC_SOURCECFG_SM_EDGE0;
		break;
	case 4:
		sourcecfg = APLIC_SOURCECFG_SM_LEVEL1;
		aih->aih_eoi = 1;
		break;
	case 8:
		sourcecfg = APLIC_SOURCECFG_SM_LEVEL0;
		aih->aih_eoi = 1;
		break;
	default:
		sourcecfg = APLIC_SOURCECFG_SM_DETACHED;
		break;
	}

	HWRITE4(sc, APLIC_SOURCECFG(source), sourcecfg);
	HWRITE4(sc, APLIC_TARGET(source), target);
	HWRITE4(sc, APLIC_SETIENUM, source);

	if (aih->aih_eoi)
		HWRITE4(aih->aih_sc, APLIC_SETIPNUM, aih->aih_source);

	return aih;
}

void
aplic_intr_disestablish(void *cookie)
{
	struct aplic_intrhand *aih = cookie;

	fdt_intr_disestablish(aih->aih_cookie);
	free(aih, M_DEVBUF, sizeof(*aih));
}

void
aplic_intr_barrier(void *cookie)
{
	struct aplic_intrhand *aih = cookie;

	intr_barrier(aih->aih_cookie);
}
