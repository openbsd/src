/*	$OpenBSD: if_le_ioasic.c,v 1.8 2001/11/30 17:24:19 art Exp $	*/
/*	$NetBSD: if_le_ioasic.c,v 1.2 1996/05/07 02:24:56 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * LANCE on DEC IOCTL ASIC.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_media.h>

#include <uvm/uvm_extern.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/tc/if_levar.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

int	le_ioasic_match __P((struct device *, void *, void *));
void	le_ioasic_attach __P((struct device *, struct device *, void *));

hide void le_ioasic_copytobuf_gap2 __P((struct am7990_softc *, void *,
	    int, int));
hide void le_ioasic_copyfrombuf_gap2 __P((struct am7990_softc *, void *,
	    int, int));

hide void le_ioasic_copytobuf_gap16 __P((struct am7990_softc *, void *,
	    int, int));
hide void le_ioasic_copyfrombuf_gap16 __P((struct am7990_softc *, void *,
	    int, int));
hide void le_ioasic_zerobuf_gap16 __P((struct am7990_softc *, int, int));

struct cfattach le_ioasic_ca = {
	sizeof(struct le_softc), le_ioasic_match, le_ioasic_attach
};

int
le_ioasic_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct ioasicdev_attach_args *d = aux;

	if (!ioasic_submatch(match, aux))
		return (0);
	if (strncmp("lance", d->iada_modname, TC_ROM_LLEN))
		return (0);

	return (1);
}

#define LE_IOASIC_MEMSIZE	(128*1024)
#define LE_IOASIC_MEMALIGN	(128*1024)
void
le_ioasic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;
	struct le_softc *lesc = (void *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
	caddr_t le_iomem;
	struct pglist pglist;
	struct vm_page *pg;
	vaddr_t va;
	vsize_t size;

	/*
	 * XXX - this vm juggling is so wrong. use bus_dma instead!
	 */
	size = round_page(LE_IOASIC_MEMSIZE);
	if (uvm_pglistalloc(size, 0, 0, LE_IOASIC_MEMALIGN, 0, &pglist, 1, 0) ||
	    uvm_map(kernel_map, &va, size, NULL, UVM_UNKNOWN_OFFSET, 0,
		UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
			UVM_ADV_RANDOM, 0)))
		panic("aha_init: could not allocate mailbox");

	le_iomem = (caddr_t)va;
	for (pg = TAILQ_FIRST(&pglist); pg != NULL;pg = TAILQ_NEXT(pg, pageq)) {
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
			VM_PROT_READ|VM_PROT_WRITE);
		va += PAGE_SIZE;
	}
	/*
	 * XXXEND
	 */

	lesc->sc_r1 = (struct lereg1 *)
		TC_DENSE_TO_SPARSE(TC_PHYS_TO_UNCACHED(d->iada_addr));
	sc->sc_mem = (void *)TC_PHYS_TO_UNCACHED(le_iomem);

	sc->sc_copytodesc = le_ioasic_copytobuf_gap2;
	sc->sc_copyfromdesc = le_ioasic_copyfrombuf_gap2;
	sc->sc_copytobuf = le_ioasic_copytobuf_gap16;
	sc->sc_copyfrombuf = le_ioasic_copyfrombuf_gap16;
	sc->sc_zerobuf = le_ioasic_zerobuf_gap16;

	ioasic_lance_dma_setup(le_iomem);	/* XXX more thought */

	dec_le_common_attach(sc, ioasic_lance_ether_address());

	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_NET,
	    am7990_intr, sc);
}

/*
 * Special memory access functions needed by ioasic-attached LANCE
 * chips.
 */

/*
 * gap2: two bytes of data followed by two bytes of pad.
 *
 * Buffers must be 4-byte aligned.  The code doesn't worry about
 * doing an extra byte.
 */

void
le_ioasic_copytobuf_gap2(sc, fromv, boff, len)
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
le_ioasic_copyfrombuf_gap2(sc, tov, boff, len)
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

/*
 * gap16: 16 bytes of data followed by 16 bytes of pad.
 *
 * Buffers must be 32-byte aligned.
 */

void
le_ioasic_copytobuf_gap16(sc, fromv, boff, len)
	struct am7990_softc *sc;
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bcopy(from, bptr + boff, xfer);
		from += xfer;
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}

void
le_ioasic_copyfrombuf_gap16(sc, tov, boff, len)
	struct am7990_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bcopy(bptr + boff, to, xfer);
		to += xfer;
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}

void
le_ioasic_zerobuf_gap16(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bzero(bptr + boff, xfer);
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}
