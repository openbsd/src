/*	$OpenBSD: if_le_ibus.c,v 1.3 1998/09/16 22:41:21 jason Exp $	*/
/*	$NetBSD: if_le_ibus.c,v 1.3 1996/05/20 23:19:16 jonathan Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * This driver was contributed by Jonathan Stone.
 */

/*
 * LANCE on Decstation kn01/kn220(?) baseboard.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/tc/if_levar.h>
#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>
#include <pmax/pmax/kn01.h>
#include <pmax/pmax/kn01var.h>

extern struct cfdriver mainbus_cd;	/* should be in header but where? */

extern void le_dec_copytobuf_gap2 __P((struct am7990_softc *, void *,
	    int, int));
extern void le_dec_copyfrombuf_gap2 __P((struct am7990_softc *, void *,
	    int, int));

hide void le_dec_zerobuf_gap2 __P((struct am7990_softc *, int, int));


int	le_pmax_match __P((struct device *, void *, void *));
void	le_pmax_attach __P((struct device *, struct device *, void *));

struct cfattach le_pmax_ca = {
	sizeof(struct le_softc), le_pmax_match, le_pmax_attach
};


int
le_pmax_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	if (parent->dv_cfdata->cf_driver == &mainbus_cd) {
	  	struct confargs *d = aux;
		if (strcmp("lance", d->ca_name) == 0)
			return (1);
	}
	return (0);
}

void
le_pmax_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct le_softc *lesc = (void *)self;
	register struct am7990_softc *sc = &lesc->sc_am7990;
	register u_char *cp;
	register struct confargs *ca = aux;

	/*
	 * It's on the baseboard, with a dedicated interrupt line.
	 */
	lesc->sc_r1 = (struct lereg1 *)(ca->ca_addr);
/*XXX*/	sc->sc_mem = (void *)TC_PHYS_TO_UNCACHED(0x19000000);
/*XXX*/	cp = (u_char *)(TC_PHYS_TO_UNCACHED(KN01_SYS_CLOCK) + 1);

	sc->sc_copytodesc = le_dec_copytobuf_gap2;
	sc->sc_copyfromdesc = le_dec_copyfrombuf_gap2;
	sc->sc_copytobuf = le_dec_copytobuf_gap2;
	sc->sc_copyfrombuf = le_dec_copyfrombuf_gap2;
	sc->sc_zerobuf = le_dec_zerobuf_gap2;

	dec_le_common_attach(sc, cp);
	/* XXX more thought about ca->slotpri */
	kn01_intr_establish(parent, (void*)ca->ca_slotpri, TC_IPL_NET,
			  am7990_intr, sc);
}

/*
 * gap2: two bytes of data followed by two bytes of pad.
 *
 * Buffers must be 4-byte aligned.  The code doesn't worry about
 * doing an extra byte.
 */

void
le_dec_copytobuf_gap2(sc, fromv, boff, len)
	struct am7990_softc *sc;  
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register volatile u_int16_t *bptr;  

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*bptr = (*from++ << 8) | (*bptr & 0xff);
		bptr += 2;  
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		*bptr = (from[1] << 8) | (from[0] & 0xff);
		bptr += 2;
		from += 2;
		len -= 2;
	}
	if (len == 1)
		*bptr = (u_int16_t)*from;
}

void
le_dec_copyfrombuf_gap2(sc, tov, boff, len)
	struct am7990_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register volatile u_int16_t *bptr;
	register u_int16_t tmp;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*to++ = (*bptr >> 8) & 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		tmp = *bptr;
		*to++ = tmp & 0xff;
		*to++ = (tmp >> 8) & 0xff;
		bptr += 2;
		len -= 2;
	}
	if (len == 1)
		*to = *bptr & 0xff;
}

void
le_dec_zerobuf_gap2(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register volatile u_int16_t *bptr;

	if ((unsigned)boff & 0x1) {
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*bptr &= 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 0) {
		*bptr = 0;
		bptr += 2;
		len -= 2;
	}
}
