/*	$OpenBSD: fhc.c,v 1.3 2004/09/24 20:50:49 jason Exp $	*/

/*
 * Copyright (c) 2004 Jason L. Wright (jason@thought.net)
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/timeout.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <sparc64/dev/fhcreg.h>
#include <sparc64/dev/fhcvar.h>
#include <sparc64/dev/iommureg.h>

struct cfdriver fhc_cd = {
	NULL, "fhc", DV_DULL
};

int	fhc_print(void *, const char *);

bus_space_tag_t fhc_alloc_bus_tag(struct fhc_softc *);
int _fhc_bus_map(bus_space_tag_t, bus_space_tag_t, bus_addr_t, bus_size_t,
    int, bus_space_handle_t *);
void *fhc_intr_establish(bus_space_tag_t, bus_space_tag_t, int, int, int,
    int (*)(void *), void *, const char *);
bus_space_handle_t *fhc_find_intr_handle(struct fhc_softc *, int);
bus_space_handle_t *fhc_try_intr_handle(struct fhc_softc *,
    bus_space_handle_t *, bus_size_t, int);

void
fhc_attach(struct fhc_softc *sc)
{
	int node0, node;

	printf(": %s\n", getpropstring(sc->sc_node, "board-model"));

	sc->sc_cbt = fhc_alloc_bus_tag(sc);

	getprop(sc->sc_node, "ranges", sizeof(struct fhc_range),
	    &sc->sc_nrange, (void **)&sc->sc_range);

	node0 = firstchild(sc->sc_node);
	for (node = node0; node; node = nextsibling(node)) {
		struct fhc_attach_args fa;

		bzero(&fa, sizeof(fa));

		fa.fa_node = node;
		fa.fa_bustag = sc->sc_cbt;

		if (fhc_get_string(fa.fa_node, "name", &fa.fa_name)) {
			printf("can't fetch name for node 0x%x\n", node);
			continue;
		}
		getprop(node, "reg", sizeof(struct fhc_reg),
		    &fa.fa_nreg, (void **)&fa.fa_reg);
		getprop(node, "interrupts", sizeof(int),
		    &fa.fa_nintr, (void **)&fa.fa_intr);

		(void)config_found(&sc->sc_dv, (void *)&fa, fhc_print);

		if (fa.fa_name != NULL)
			free(fa.fa_name, M_DEVBUF);
		if (fa.fa_reg != NULL)
			free(fa.fa_reg, M_DEVBUF);
		if (fa.fa_nintr != NULL)
			free(fa.fa_intr, M_DEVBUF);
	}
}

int
fhc_print(void *args, const char *busname)
{
	struct fhc_attach_args *fa = args;
	char *class;

	if (busname != NULL) {
		printf("%s at %s", fa->fa_name, busname);
		class = getpropstring(fa->fa_node, "device_type");
		if (*class != '\0')
			printf(" class %s", class);
	}
	return (UNCONF);
}

int
fhc_get_string(int node, char *name, char **buf)
{
	int len;

	len = getproplen(node, name);
	if (len < 0)
		return (len);
	*buf = (char *)malloc(len + 1, M_DEVBUF, M_NOWAIT);
	if (buf == NULL)
		return (-1);

	if (len != 0)
		getpropstringA(node, name, *buf);
	(*buf)[len] = '\0';
	return (0);
}

bus_space_tag_t
fhc_alloc_bus_tag(struct fhc_softc *sc)
{
	struct sparc_bus_space_tag *bt;

	bt = malloc(sizeof(*bt), M_DEVBUF, M_NOWAIT);
	if (bt == NULL)
		panic("fhc: couldn't alloc bus tag");

	bzero(bt, sizeof(*bt));
	snprintf(bt->name, sizeof(bt->name), "%s", sc->sc_dv.dv_xname);
	bt->cookie = sc;
	bt->parent = sc->sc_bt;
	bt->asi = bt->parent->asi;
	bt->sasi = bt->parent->sasi;
	bt->sparc_bus_map = _fhc_bus_map;
	/* XXX bt->sparc_bus_mmap = fhc_bus_mmap; */
	bt->sparc_intr_establish = fhc_intr_establish;
	return (bt);
}

int
_fhc_bus_map(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t addr,
    bus_size_t size, int flags, bus_space_handle_t *hp)
{
	struct fhc_softc *sc = t->cookie;
	int64_t slot = BUS_ADDR_IOSPACE(addr);
	int64_t offset = BUS_ADDR_PADDR(addr);
	int i;

	if (t->parent == NULL || t->parent->sparc_bus_map == NULL) {
		printf("\n_fhc_bus_map: invalid parent");
		return (EINVAL);
	}

	if (flags & BUS_SPACE_MAP_PROMADDRESS)
		return ((*t->parent->sparc_bus_map)(t, t0, addr,
		    size, flags, hp));

	for (i = 0; i < sc->sc_nrange; i++) {
		bus_addr_t paddr;

		if (sc->sc_range[i].cspace != slot)
			continue;

		paddr = offset - sc->sc_range[i].coffset;
		paddr += sc->sc_range[i].poffset;
		paddr |= ((bus_addr_t)sc->sc_range[i].pspace << 32); 

		return ((*t->parent->sparc_bus_map)(t->parent, t0, paddr,
		    size, flags, hp));
	}

	return (EINVAL);
}

bus_space_handle_t *
fhc_try_intr_handle(struct fhc_softc *sc, bus_space_handle_t *hp,
    bus_size_t off, int val)
{
	u_int64_t r;

	r = bus_space_read_8(sc->sc_bt, *hp, off);
	if (INTINO(r) == INTINO(val))
		return (hp);
	return (NULL);
}

bus_space_handle_t *
fhc_find_intr_handle(struct fhc_softc *sc, int val)
{
	bus_space_handle_t *hp;

	hp = fhc_try_intr_handle(sc, &sc->sc_freg, FHC_F_IMAP, val);
	if (hp != NULL)
		return (hp);
	hp = fhc_try_intr_handle(sc, &sc->sc_sreg, FHC_S_IMAP, val);
	if (hp != NULL)
		return (hp);
	hp = fhc_try_intr_handle(sc, &sc->sc_ureg, FHC_U_IMAP, val);
	if (hp != NULL)
		return (hp);
	hp = fhc_try_intr_handle(sc, &sc->sc_treg, FHC_T_IMAP, val);
	if (hp != NULL)
		return (hp);
	return (NULL);
}

void *
fhc_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int ihandle,
    int level, int flags, int (*handler)(void *), void *arg, const char *what)
{
	struct fhc_softc *sc = t->cookie;
	volatile u_int64_t *intrmapptr = NULL, *intrclrptr = NULL;
	struct intrhand *ih;

	if (level == IPL_NONE)
		level = INTLEV(ihandle);
	if (level == IPL_NONE) {
		printf(": no IPL, setting IPL 2.\n");
		level = 2;
	}

	if ((flags & BUS_INTR_ESTABLISH_SOFTINTR) == 0) {
		bus_space_handle_t *hp;
		struct fhc_intr_reg *intrregs;

		hp = fhc_find_intr_handle(sc, ihandle);
		if (hp == NULL) {
			printf(": can't find intr handle\n");
			return (NULL);
		}

		intrregs = bus_space_vaddr(sc->sc_bt, *hp);
		intrmapptr = &intrregs->imap;
		intrclrptr = &intrregs->iclr;
	}

	ih = bus_intr_allocate(t0, handler, arg, INTINO(ihandle), level,
	    intrmapptr, intrclrptr, what);
	if (ih == NULL)
		return (NULL);

	intr_establish(ih->ih_pil, ih);

	if (intrmapptr != NULL) {
		u_int64_t r;

		r = *intrmapptr;
		r |= INTMAP_V;
		*intrmapptr = r;
		r = *intrmapptr;
		ih->ih_number |= r & INTMAP_INR;
	}

	return (ih);
}
