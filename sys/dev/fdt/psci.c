/*	$OpenBSD: psci.c,v 1.1 2017/01/25 10:14:40 jsg Exp $	*/

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

extern void (*cpuresetfn)(void);
extern void (*powerdownfn)(void);

#define SYSTEM_OFF	0x84000008
#define SYSTEM_RESET	0x84000009

struct psci_softc {
	struct device		 sc_dev;
	void			 (*callfn)(uint32_t, uint32_t, uint32_t, uint32_t);
};

struct psci_softc *psci_sc;

int	psci_match(struct device *, void *, void *);
void	psci_attach(struct device *, struct device *, void *);
void	psci_reset(void);
void	psci_powerdown(void);

extern void hvc_call(uint32_t, uint32_t, uint32_t, uint32_t);
extern void smc_call(uint32_t, uint32_t, uint32_t, uint32_t);

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

	/* reset and shutdown added in 0.2 */
	return OF_is_compatible(faa->fa_node, "arm,psci-0.2");
}

void
psci_attach(struct device *parent, struct device *self, void *aux)
{
	struct psci_softc		*sc = (struct psci_softc *) self;
	struct fdt_attach_args		*faa = aux;
	char				 method[128];

	if (OF_getprop(faa->fa_node, "method", method, sizeof(method))) {
		if (strcmp(method, "hvc") == 0)
			sc->callfn = hvc_call;
		else if (strcmp(method, "smc") == 0)
			sc->callfn = smc_call;
	}

	printf("\n");

	psci_sc = sc;
	cpuresetfn = psci_reset;
	powerdownfn = psci_powerdown;
}

void
psci_reset(void)
{
	struct psci_softc *sc = psci_sc;
	if (sc->callfn)
		(*sc->callfn)(SYSTEM_RESET, 0, 0, 0);
}

void
psci_powerdown(void)
{
	struct psci_softc *sc = psci_sc;
	if (sc->callfn)
		(*sc->callfn)(SYSTEM_OFF, 0, 0, 0);
}
