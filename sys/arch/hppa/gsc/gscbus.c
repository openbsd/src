/*	$OpenBSD: gscbus.c,v 1.2 1999/02/25 21:07:48 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#define GSCDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/gsc/gscbusvar.h>

int	gscmatch __P((struct device *, void *, void *));
void	gscattach __P((struct device *, struct device *, void *));

struct cfattach gsc_ca = {
	sizeof(struct gsc_softc), gscmatch, gscattach
};

struct cfdriver gsc_cd = {
	NULL, "gsc", DV_DULL
};

int	gsc_dmamap_create __P((void *, bus_size_t, int,
			       bus_size_t, bus_size_t, int, bus_dmamap_t *));
void	gsc_dmamap_destroy __P((void *, bus_dmamap_t));
int	gsc_dmamap_load __P((void *, bus_dmamap_t, void *,
			     bus_size_t, struct proc *, int));
int	gsc_dmamap_load_mbuf __P((void *, bus_dmamap_t, struct mbuf *, int));
int	gsc_dmamap_load_uio __P((void *, bus_dmamap_t, struct uio *, int));
int	gsc_dmamap_load_raw __P((void *, bus_dmamap_t,
				 bus_dma_segment_t *, int, bus_size_t, int));
void	gsc_dmamap_unload __P((void *, bus_dmamap_t));
void	gsc_dmamap_sync __P((void *, bus_dmamap_t, bus_dmasync_op_t));

int	gsc_dmamem_alloc __P((void *, bus_size_t, bus_size_t,
			      bus_size_t, bus_dma_segment_t *, int, int *, int));
void	gsc_dmamem_free __P((void *, bus_dma_segment_t *, int));
int	gsc_dmamem_map __P((void *, bus_dma_segment_t *,
			    int, size_t, caddr_t *, int));
void	gsc_dmamem_unmap __P((void *, caddr_t, size_t));
int	gsc_dmamem_mmap __P((void *, bus_dma_segment_t *, int, int, int, int));

int
gscmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

	return !strcmp(ca->ca_name, "gsc");
}

void
gscattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct gsc_softc *sc = (struct gsc_softc *)self;
	register struct gsc_attach_args *ga = aux;

	sc->sc_iot = ga->ga_iot;
	sc->sc_ic = ga->ga_ic;
	sc->sc_intrmask = 0;
	bzero(sc->sc_intrvs, sizeof(sc->sc_intrvs));

	printf ("\n");

	sc->sc_ih = cpu_intr_establish(IPL_HIGH, ga->ga_irq,
				       gsc_intr, sc, sc->sc_dev.dv_xname);
	/* DMA guts */
	sc->sc_dmatag._cookie = sc;
	sc->sc_dmatag._dmamap_create = gsc_dmamap_create;
	sc->sc_dmatag._dmamap_destroy = gsc_dmamap_destroy;
	sc->sc_dmatag._dmamap_load = gsc_dmamap_load;
	sc->sc_dmatag._dmamap_load_mbuf = gsc_dmamap_load_mbuf;
	sc->sc_dmatag._dmamap_load_uio = gsc_dmamap_load_uio;
	sc->sc_dmatag._dmamap_load_raw = gsc_dmamap_load_raw;
	sc->sc_dmatag._dmamap_unload = gsc_dmamap_unload;
	sc->sc_dmatag._dmamap_sync = gsc_dmamap_sync;

	sc->sc_dmatag._dmamem_alloc = gsc_dmamem_alloc;
	sc->sc_dmatag._dmamem_free = gsc_dmamem_free;
	sc->sc_dmatag._dmamem_map = gsc_dmamem_map;
	sc->sc_dmatag._dmamem_unmap = gsc_dmamem_unmap;
	sc->sc_dmatag._dmamem_mmap = gsc_dmamem_mmap;

	pdc_scanbus(self, &ga->ga_ca, ga->ga_mod, MAXMODBUS);
}

int
gscprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	return (UNCONF);
}


void *
gsc_intr_establish(sc, pri, irq, handler, arg, name)
	struct gsc_softc *sc;
	int pri;
	int irq;
	int (*handler) __P((void *v));
	void *arg;
	const char *name;
{
	register struct gscbus_intr *iv;
	register u_int32_t mask;

	mask = 1 << irq;
	if (sc->sc_intrmask & mask) {
#ifdef GSCDEBUG
		printf("%s: attaching irq %d, already occupied\n",
		       sc->sc_dev.dv_xname, irq);
#endif
		return NULL;
	}
	sc->sc_intrmask |= mask;
	iv = &sc->sc_intrvs[irq];
	iv->pri = pri;
	iv->handler = handler;
	iv->arg = arg;
	evcnt_attach(&sc->sc_dev, name, &iv->evcnt);
	(sc->sc_ic->gsc_intr_establish)(sc->sc_ic->gsc_dv, mask);
#ifdef GSCDEBUG
	printf("gsc_intr_stablish: mask=0x%08x irq=%d iv=%p\n", mask, irq, iv);
#endif

	return &sc->sc_intrvs[irq];
}

void
gsc_intr_disestablish(sc, v)
	struct gsc_softc *sc;
	void *v;
{
	register u_int32_t mask;

	mask = 1 << (sc->sc_intrvs - (struct gscbus_intr *)v);
	sc->sc_intrmask &= ~mask;
	((struct gscbus_intr *)v)->handler = NULL;
	/* evcnt_detach(); */
	(sc->sc_ic->gsc_intr_disestablish)(sc->sc_ic->gsc_dv, mask);
}

int
gsc_intr(v)
	void *v;
{
	register struct gsc_softc *sc = v;
	register struct gscbus_ic *ic = sc->sc_ic;
	register u_int32_t mask;
	int ret;

#ifdef GSCDEBUG
	printf("gsc_intr(%p)\n", v);
#endif
	ret = 0;
	while ((mask = (ic->gsc_intr_check)(ic->gsc_dv))) {
		register int i;
		register struct gscbus_intr *iv;

		i = ffs(mask) - 1;
		iv = &sc->sc_intrvs[i];

#ifdef GSCDEBUG
		printf("gsc_intr: got mask=0x%08x i=%d iv=%p\n", mask, i, iv);
#endif
		if (iv->handler) {
			int s;
#ifdef GSCDEBUG
			printf("gsc_intr: calling %p for irq %d\n", v, i);
#endif
			s = splx(iv->pri);
			ret += (iv->handler)(iv->arg);
			splx(s);
		} else
			printf("%s: stray interrupt %d\n",
			       sc->sc_dev.dv_xname, i);

		(ic->gsc_intr_ack)(ic->gsc_dv, 1 << i);
	}

	return ret;
}

int
gsc_dmamap_create(v, size, nseg, maxsegsz, boundary, flags, dmamp)
	void *v;
	bus_size_t size;
	int nseg;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	return 0;
}

void
gsc_dmamap_destroy(v, map)
	void *v;
	bus_dmamap_t map;
{
}

int
gsc_dmamap_load(v, map, buf, buflen, p, flags)
	void *v;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	return 0;
}

int
gsc_dmamap_load_mbuf(v, map, mbuf, flags)
	void *v;
	bus_dmamap_t map;
	struct mbuf *mbuf;
	int flags;
{
	return 0;
}

int
gsc_dmamap_load_uio(v, map, uio, flags)
	void *v;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	return 0;
}

int
gsc_dmamap_load_raw(v, map, segs, nsegs, size, flags)
	void *v;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	return 0;
}

void
gsc_dmamap_unload(v, map)
	void *v;
	bus_dmamap_t map;
{

}

void
gsc_dmamap_sync(v, map, op)
	void *v;
	bus_dmamap_t map;
	bus_dmasync_op_t op;
{

}

int
gsc_dmamem_alloc(v, size, alignment, boundary, segs, nsegs, rsegs, flags)
	void *v;
	bus_size_t size;
	bus_size_t alignment;
	bus_size_t boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	return 0;
}

void
gsc_dmamem_free(v, segs, nsegs)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
{

}

int
gsc_dmamem_map(v, segs, nsegs, size, kvap, flags)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	return 0;
}

void
gsc_dmamem_unmap(v, kva, size)
	void *v;
	caddr_t kva;
	size_t size;
{

}

int
gsc_dmamem_mmap(v, segs, nsegs, off, prot, flags)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
	int off;
	int prot;
	int flags;
{
	return 0;
}
