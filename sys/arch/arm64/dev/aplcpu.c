/*	$OpenBSD: aplcpu.c,v 1.1 2022/02/20 19:25:57 kettenis Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/*
 * This driver is based on preliminary device tree bindings and will
 * almost certainly need changes once the official bindings land in
 * mainline Linux.  Support for these preliminary bindings will be
 * dropped as soon as official bindings are available.
 */

#define CLUSTER_PSTATE			0x0020
#define CLUSTER_PSTATE_BUSY		(1U << 31)
#define CLUSTER_PSTATE_SET		(1 << 25)
#define CLUSTER_PSTATE_DESIRED2_MASK	(0xf << 12)
#define CLUSTER_PSTATE_DESIRED2_SHIFT	12
#define CLUSTER_PSTATE_DESIRED1_MASK	(0xf << 0)
#define CLUSTER_PSTATE_DESIRED1_SHIFT	0

struct opp {
	uint64_t opp_hz;
	uint32_t opp_level;
};

struct opp_table {
	LIST_ENTRY(opp_table) ot_list;
	uint32_t ot_phandle;

	struct opp *ot_opp;
	u_int ot_nopp;
	uint64_t ot_opp_hz_min;
	uint64_t ot_opp_hz_max;
};

#define APLCPU_MAX_CLUSTERS	8

struct aplcpu_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh[APLCPU_MAX_CLUSTERS];
	bus_size_t		sc_ios[APLCPU_MAX_CLUSTERS];

	int			sc_node;
	u_int			sc_nclusters;
	int			sc_perflevel;

	LIST_HEAD(, opp_table)	sc_opp_tables;
	struct opp_table	*sc_opp_table[APLCPU_MAX_CLUSTERS];
	uint64_t		sc_opp_hz_min;
	uint64_t		sc_opp_hz_max;
};

struct aplcpu_softc *aplcpu_sc;

int	aplcpu_match(struct device *, void *, void *);
void	aplcpu_attach(struct device *, struct device *, void *);

const struct cfattach aplcpu_ca = {
	sizeof (struct aplcpu_softc), aplcpu_match, aplcpu_attach
};

struct cfdriver aplcpu_cd = {
	NULL, "aplcpu", DV_DULL
};

void	aplcpu_opp_init(struct aplcpu_softc *, int);
int	aplcpu_clockspeed(int *);
void	aplcpu_setperf(int level);

int
aplcpu_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,cluster-cpufreq");
}

void
aplcpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplcpu_softc *sc = (struct aplcpu_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;
	int i;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	if (faa->fa_nreg > APLCPU_MAX_CLUSTERS) {
		printf(": too many registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	for (i = 0; i < faa->fa_nreg; i++) {
		if (bus_space_map(sc->sc_iot, faa->fa_reg[i].addr,
		    faa->fa_reg[i].size, 0, &sc->sc_ioh[i])) {
			printf(": can't map registers\n");
			goto unmap;
		}
		sc->sc_ios[i] = faa->fa_reg[i].size;
	}

	printf("\n");

	sc->sc_node = faa->fa_node;
	sc->sc_nclusters = faa->fa_nreg;

	sc->sc_opp_hz_min = UINT64_MAX;
	sc->sc_opp_hz_max = 0;

	LIST_INIT(&sc->sc_opp_tables);
	CPU_INFO_FOREACH(cii, ci) {
		aplcpu_opp_init(sc, ci->ci_node);
	}

	aplcpu_sc = sc;
	cpu_cpuspeed = aplcpu_clockspeed;
	cpu_setperf = aplcpu_setperf;
	return;

unmap:
	for (i = 0; i < faa->fa_nreg; i++) {
		if (sc->sc_ios[i] == 0)
			continue;
		bus_space_unmap(sc->sc_iot, sc->sc_ioh[i], sc->sc_ios[i]);
	}
}

void
aplcpu_opp_init(struct aplcpu_softc *sc, int node)
{
	struct opp_table *ot;
	int count, child;
	uint32_t freq_domain[2], phandle;
	uint32_t opp_hz, opp_level;
	int i, j;

	if (OF_getpropintarray(node, "apple,freq-domain", freq_domain,
	    sizeof(freq_domain)) != sizeof(freq_domain))
		return;
	if (freq_domain[0] != OF_getpropint(sc->sc_node, "phandle", 0))
		return;
	if (freq_domain[1] > APLCPU_MAX_CLUSTERS)
		return;

	phandle = OF_getpropint(node, "operating-points-v2", 0);
	if (phandle == 0)
		return;

	LIST_FOREACH(ot, &sc->sc_opp_tables, ot_list) {
		if (ot->ot_phandle == phandle) {
			sc->sc_opp_table[freq_domain[1]] = ot;
			return;
		}
	}

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return;

	if (!OF_is_compatible(node, "operating-points-v2"))
		return;

	count = 0;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getproplen(child, "turbo-mode") == 0)
			continue;
		count++;
	}
	if (count == 0)
		return;

	ot = malloc(sizeof(struct opp_table), M_DEVBUF, M_ZERO | M_WAITOK);
	ot->ot_phandle = phandle;
	ot->ot_opp = mallocarray(count, sizeof(struct opp),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	ot->ot_nopp = count;

	count = 0;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getproplen(child, "turbo-mode") == 0)
			continue;
		opp_hz = OF_getpropint64(child, "opp-hz", 0);
		opp_level = OF_getpropint(child, "opp-level", 0);

		/* Insert into the array, keeping things sorted. */
		for (i = 0; i < count; i++) {
			if (opp_hz < ot->ot_opp[i].opp_hz)
				break;
		}
		for (j = count; j > i; j--)
			ot->ot_opp[j] = ot->ot_opp[j - 1];
		ot->ot_opp[i].opp_hz = opp_hz;
		ot->ot_opp[i].opp_level = opp_level;
		count++;
	}

	ot->ot_opp_hz_min = ot->ot_opp[0].opp_hz;
	ot->ot_opp_hz_max = ot->ot_opp[count - 1].opp_hz;

	LIST_INSERT_HEAD(&sc->sc_opp_tables, ot, ot_list);
	sc->sc_opp_table[freq_domain[1]] = ot;

	/* Keep track of overall min/max frequency. */
	if (sc->sc_opp_hz_min > ot->ot_opp_hz_min)
		sc->sc_opp_hz_min = ot->ot_opp_hz_min;
	if (sc->sc_opp_hz_max < ot->ot_opp_hz_max)
		sc->sc_opp_hz_max = ot->ot_opp_hz_max;
}

int
aplcpu_clockspeed(int *freq)
{
	struct aplcpu_softc *sc = aplcpu_sc;
	struct opp_table *ot;
	uint32_t opp_hz = 0, opp_level;
	uint64_t pstate;
	int i, j;

	/*
	 * Clusters can run at different frequencies.  We report the
	 * highest frequency among all clusters.
	 */

	for (i = 0; i < sc->sc_nclusters; i++) {
		if (sc->sc_opp_table[i] == NULL)
			continue;

		pstate = bus_space_read_8(sc->sc_iot, sc->sc_ioh[i],
		    CLUSTER_PSTATE);
		opp_level = (pstate & CLUSTER_PSTATE_DESIRED1_MASK);
		opp_level >>= CLUSTER_PSTATE_DESIRED1_SHIFT;

		/* Translate P-state to frequency. */
		ot = sc->sc_opp_table[i];
		for (j = 0; j < ot->ot_nopp; j++) {
			if (ot->ot_opp[j].opp_level == opp_level)
				opp_hz = MAX(opp_hz, ot->ot_opp[j].opp_hz);
		}
	}
	if (opp_hz == 0)
		return EINVAL;

	*freq = opp_hz / 1000000;
	return 0;
}

void
aplcpu_setperf(int level)
{
	struct aplcpu_softc *sc = aplcpu_sc;
	struct opp_table *ot;
	uint64_t min, max;
	uint64_t level_hz;
	uint32_t opp_level;
	uint64_t reg;
	int i, j, timo;

	if (sc->sc_perflevel == level)
		return;

	/*
	 * We let the CPU performance level span the entire range
	 * between the lowest frequency on any of the clusters and the
	 * highest frequency on any of the clusters.  We pick a
	 * frequency within that range based on the performance level
	 * and set all the clusters to the frequency that is closest
	 * to but less than that frequency.  This isn't a particularly
	 * sensible method but it is easy to implement and it is hard
	 * to come up with something more sensible given the
	 * constraints of the hw.setperf sysctl interface.
	 */
	min = sc->sc_opp_hz_min;
	max = sc->sc_opp_hz_max;
	level_hz = min + (level * (max - min)) / 100;

	for (i = 0; i < sc->sc_nclusters; i++) {
		if (sc->sc_opp_table[i] == NULL)
			continue;

		/* Translate performance level to a P-state. */
		opp_level = 0;
		ot = sc->sc_opp_table[i];
		for (j = 0; j < ot->ot_nopp; j++) {
			if (ot->ot_opp[j].opp_hz <= level_hz &&
			    ot->ot_opp[j].opp_level >= opp_level)
				opp_level = ot->ot_opp[j].opp_level;
		}

		/* Wait until P-state logic isn't busy. */
		for (timo = 100; timo > 0; timo--) {
			reg = bus_space_read_8(sc->sc_iot, sc->sc_ioh[i],
			    CLUSTER_PSTATE);
			if ((reg & CLUSTER_PSTATE_BUSY) == 0)
				break;
			delay(1);
		}
		if (reg & CLUSTER_PSTATE_BUSY)
			continue;

		/* Set desired P-state. */
		reg &= ~CLUSTER_PSTATE_DESIRED1_MASK;
		reg &= ~CLUSTER_PSTATE_DESIRED2_MASK;
		reg |= (opp_level << CLUSTER_PSTATE_DESIRED1_SHIFT);
		reg |= (opp_level << CLUSTER_PSTATE_DESIRED2_SHIFT);
		reg |= CLUSTER_PSTATE_SET;
		bus_space_write_8(sc->sc_iot, sc->sc_ioh[i],
		    CLUSTER_PSTATE, reg);
	}

	sc->sc_perflevel = level;
}
