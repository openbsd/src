/*	$NetBSD: if_le_ioasic.c,v 1.1 1996/04/18 00:50:13 cgd Exp $	*/

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

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/tc/if_levar.h>
#include <dev/ic/am7990reg.h>
#define LE_NEED_BUF_GAP2
#define LE_NEED_BUF_GAP16
#include <dev/ic/am7990var.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

extern caddr_t le_iomem;

int	le_ioasic_match __P((struct device *, void *, void *));
void	le_ioasic_attach __P((struct device *, struct device *, void *));

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

void
le_ioasic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;
	register struct le_softc *sc = (void *)self;

	sc->sc_r1 = (struct lereg1 *)
		TC_DENSE_TO_SPARSE(TC_PHYS_TO_UNCACHED(d->iada_addr));
	sc->sc_mem = (void *)TC_PHYS_TO_UNCACHED(le_iomem);

	sc->sc_copytodesc = am7990_copytobuf_gap2;
	sc->sc_copyfromdesc = am7990_copyfrombuf_gap2;
	sc->sc_copytobuf = am7990_copytobuf_gap16;
	sc->sc_copyfrombuf = am7990_copyfrombuf_gap16;
	sc->sc_zerobuf = am7990_zerobuf_gap16;

	ioasic_lance_dma_setup(le_iomem);	/* XXX more thought */

	dec_le_common_attach(sc, ioasic_lance_ether_address());

	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_NET, leintr, sc);
}
