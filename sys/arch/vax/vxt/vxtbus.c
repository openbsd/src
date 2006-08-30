/*	$OpenBSD: vxtbus.c,v 1.2 2006/08/30 19:23:57 miod Exp $	*/
/*
 * Copyright (c) 2006 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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
#include <sys/evcount.h>
#include <sys/queue.h>

#include <machine/cpu.h>
#include <machine/nexus.h>
#include <machine/scb.h>

#include <vax/vxt/vxtbusvar.h>

struct vxtbus_softc {
	struct device 		sc_dev;
	LIST_HEAD(, vxtbus_ih)	sc_intrlist;
};

void	vxtbus_attach(struct device *, struct device *, void *);
int	vxtbus_match(struct device *, void*, void *);

struct	cfdriver vxtbus_cd = {
	NULL, "vxtbus", DV_DULL
};

struct	cfattach vxtbus_ca = {
	sizeof(struct vxtbus_softc), vxtbus_match, vxtbus_attach
};

void	vxtbus_intr(void *);
int	vxtbus_print(void *, const char *);

int
vxtbus_match(struct device *parent, void *vcf, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	return (maa->maa_bustype == VAX_VXTBUS ? 1 : 0);
}

void
vxtbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct vxtbus_softc *sc = (void *)self;
	struct bp_conf bp;

	LIST_INIT(&sc->sc_intrlist);
	scb_vecalloc(VXT_INTRVEC, vxtbus_intr, sc, SCB_ISTACK, NULL);

	printf("\n");

	bp.type = "sgec";
	config_found(self, &bp, vxtbus_print);

	bp.type = "qsc";
	config_found(self, &bp, vxtbus_print);

	bp.type = "lcspx";
	config_found(self, &bp, vxtbus_print);
}

int
vxtbus_print(void *aux, const char *name)
{
	struct bp_conf *bp = aux;

	if (name)
		printf("%s at %s", bp->type, name);

	return (UNCONF);
}

/*
 * VXT2000 interrupt code.
 *
 * All device interrupts end up on the same vector, which is controllable
 * by the SC26C94 chip.
 *
 * Interrupts are handled at spl4 (ipl 0x14).
 *
 * The following routines implement shared interrupts for vxtbus subdevices.
 */

struct vxtbus_ih {
	LIST_ENTRY(vxtbus_ih)	ih_link;
	int			(*ih_fn)(void *);
	void *			ih_arg;
	int			ih_vec;
	struct evcount		ih_cnt;
};

void
vxtbus_intr_establish(const char *name, int ipl, int (*fn)(void *), void *arg)
{
	struct vxtbus_softc *sc = (void *)vxtbus_cd.cd_devs[0];
	struct vxtbus_ih *ih;

	ih = (struct vxtbus_ih *)malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);

	ih->ih_fn = fn;
	ih->ih_arg = arg;
	ih->ih_vec = VXT_INTRVEC;
	evcount_attach(&ih->ih_cnt, name, (void *)&ih->ih_vec, &evcount_intr);

	LIST_INSERT_HEAD(&sc->sc_intrlist, ih, ih_link);
}

void
vxtbus_intr(void *arg)
{
	struct vxtbus_softc *sc = arg;
	struct vxtbus_ih *ih;
	int rc;
#ifdef DIAGNOSTIC
	int handled = 0;
	static int strayintr = 0;
#endif

	LIST_FOREACH(ih, &sc->sc_intrlist, ih_link) {
		rc = (*ih->ih_fn)(ih->ih_arg);
		if (rc != 0) {
#ifdef DIAGNOSTIC
			handled = 1;
#endif
			ih->ih_cnt.ec_count++;
			if (rc > 0)
				break;
		}
	}

#ifdef DIAGNOSTIC
	if (handled == 0) {
		if (++strayintr == 10)
			panic("%s: too many stray interrupts",
			    sc->sc_dev.dv_xname);
		else
			printf("%s: stray interrupt\n", sc->sc_dev.dv_xname);
	}
#endif
}
