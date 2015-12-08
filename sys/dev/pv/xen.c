/*
 * Copyright (c) 2015 Mike Belopuhov
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
#include <sys/atomic.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

#include <uvm/uvm_extern.h>

#include <dev/pv/pvvar.h>
#include <dev/pv/xenvar.h>

struct xen_softc *xen_sc;

int 	xen_match(struct device *, void *, void *);
void	xen_attach(struct device *, struct device *, void *);
void	xen_resume(struct device *);
int	xen_activate(struct device *, int);

const struct cfdriver xen_cd = {
	NULL, "xen", DV_DULL
};

const struct cfattach xen_ca = {
	sizeof(struct xen_softc), xen_match, xen_attach, NULL, xen_activate
};

int
xen_match(struct device *parent, void *match, void *aux)
{
	struct pv_attach_args *pva = aux;
	struct pvbus_hv *hv = &pva->pva_hv[PVBUS_XEN];

	if (hv->hv_base == 0)
		return (0);

	return (1);
}

void
xen_attach(struct device *parent, struct device *self, void *aux)
{
	struct pv_attach_args *pva = (struct pv_attach_args *)aux;
	struct pvbus_hv *hv = &pva->pva_hv[PVBUS_XEN];
	struct xen_softc *sc = (struct xen_softc *)self;

	sc->sc_base = hv->hv_base;

	printf("\n");

	/* Wire it up to the global */
	xen_sc = sc;
}

void
xen_resume(struct device *self)
{
}

int
xen_activate(struct device *self, int act)
{
	int rv = 0;

	switch (act) {
	case DVACT_RESUME:
		xen_resume(self);
		break;
	}
	return (rv);
}
