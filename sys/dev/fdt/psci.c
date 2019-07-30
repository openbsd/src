/*	$OpenBSD: psci.c,v 1.6 2018/02/23 19:08:56 kettenis Exp $	*/

/*
 * Copyright (c) 2016 Jonathan Gray <jsg@openbsd.org>
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
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/fdt/pscivar.h>

extern void (*cpuresetfn)(void);
extern void (*powerdownfn)(void);

#define PSCI_VERSION	0x84000000
#define SYSTEM_OFF	0x84000008
#define SYSTEM_RESET	0x84000009
#ifdef __LP64__
#define CPU_ON		0xc4000003
#else
#define CPU_ON		0x84000003
#endif

struct psci_softc {
	struct device	 sc_dev;
	register_t	 (*sc_callfn)(register_t, register_t, register_t,
			     register_t);
	uint32_t	 sc_psci_version; 
	uint32_t	 sc_system_off;
	uint32_t	 sc_system_reset;
	uint32_t	 sc_cpu_on;
};

struct psci_softc *psci_sc;

int	psci_match(struct device *, void *, void *);
void	psci_attach(struct device *, struct device *, void *);
void	psci_reset(void);
void	psci_powerdown(void);

extern register_t hvc_call(register_t, register_t, register_t, register_t);
extern register_t smc_call(register_t, register_t, register_t, register_t);

struct cfattach psci_ca = {
	sizeof(struct psci_softc), psci_match, psci_attach
};

struct cfdriver psci_cd = {
	NULL, "psci", DV_DULL
};

int
psci_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "arm,psci") ||
	    OF_is_compatible(faa->fa_node, "arm,psci-0.2") ||
	    OF_is_compatible(faa->fa_node, "arm,psci-1.0");
}

void
psci_attach(struct device *parent, struct device *self, void *aux)
{
	struct psci_softc *sc = (struct psci_softc *)self;
	struct fdt_attach_args *faa = aux;
	char method[128];
	uint32_t version;

	if (OF_getprop(faa->fa_node, "method", method, sizeof(method))) {
		if (strcmp(method, "hvc") == 0)
			sc->sc_callfn = hvc_call;
		else if (strcmp(method, "smc") == 0)
			sc->sc_callfn = smc_call;
	}

	/*
	 * The function IDs are only to be parsed for the old specification
	 * (as in version 0.1).  All newer implementations are supposed to
	 * use the specified values.
	 */
	if (OF_is_compatible(faa->fa_node, "arm,psci-0.2") ||
	    OF_is_compatible(faa->fa_node, "arm,psci-1.0")) {
		sc->sc_psci_version = PSCI_VERSION;
		sc->sc_system_off = SYSTEM_OFF;
		sc->sc_system_reset = SYSTEM_RESET;
		sc->sc_cpu_on = CPU_ON;
	} else if (OF_is_compatible(faa->fa_node, "arm,psci")) {
		sc->sc_system_off = OF_getpropint(faa->fa_node,
		    "system_off", 0);
		sc->sc_system_reset = OF_getpropint(faa->fa_node,
		    "system_reset", 0);
		sc->sc_cpu_on = OF_getpropint(faa->fa_node, "cpu_on", 0);
	}

	psci_sc = sc;

	version = psci_version();
	printf(": PSCI %d.%d\n", version >> 16, version & 0xffff);

	if (sc->sc_system_off != 0)
		powerdownfn = psci_powerdown;
	if (sc->sc_system_reset != 0)
		cpuresetfn = psci_reset;
}

uint32_t
psci_version(void)
{
	struct psci_softc *sc = psci_sc;

	if (sc && sc->sc_callfn && sc->sc_psci_version != 0)
		return (*sc->sc_callfn)(sc->sc_psci_version, 0, 0, 0);

	/* No version support; return 0.0. */
	return 0;
}

void
psci_reset(void)
{
	struct psci_softc *sc = psci_sc;
	if (sc->sc_callfn)
		(*sc->sc_callfn)(sc->sc_system_reset, 0, 0, 0);
}

void
psci_powerdown(void)
{
	struct psci_softc *sc = psci_sc;
	if (sc->sc_callfn)
		(*sc->sc_callfn)(sc->sc_system_off, 0, 0, 0);
}

int32_t
psci_cpu_on(register_t target_cpu, register_t entry_point_address,
    register_t context_id)
{
	struct psci_softc *sc = psci_sc;

	if (sc && sc->sc_callfn && sc->sc_cpu_on != 0)
		return (*sc->sc_callfn)(sc->sc_cpu_on, target_cpu,
		    entry_point_address, context_id);

	return PSCI_NOT_SUPPORTED;
}
