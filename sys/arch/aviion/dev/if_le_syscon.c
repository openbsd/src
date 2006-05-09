/*	$OpenBSD: if_le_syscon.c,v 1.1.1.1 2006/05/09 18:13:40 miod Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

#include <net/if_media.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <machine/av400.h>
#include <aviion/dev/sysconreg.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

/*
 * LANCE registers. Although these are 16 bit registers, on the AV400
 * design, they need to be accessed as 32 bit registers. Bus magic...
 * The real stuff is in dev/ic/am7990reg.h
 */
struct av_lereg {
	volatile u_int32_t	ler1_rdp;	/* data port */
	volatile u_int32_t	ler1_rap;	/* register select port */
};

/*
 * Ethernet software status per interface.
 * The real stuff is in dev/ic/am7990var.h
 */
struct	le_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	struct	av_lereg *sc_r1;	/* LANCE registers */
	struct	intrhand sc_ih;
};

int	le_syscon_match(struct device *, void *, void *);
void	le_syscon_attach(struct device *, struct device *, void *);

struct cfattach le_syscon_ca = {
	sizeof(struct le_softc), le_syscon_match, le_syscon_attach
};

void	le_syscon_wrcsr(struct am7990_softc *, u_int16_t, u_int16_t);
u_int16_t le_syscon_rdcsr(struct am7990_softc *, u_int16_t);

void
le_syscon_wrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct av_lereg *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

u_int16_t
le_syscon_rdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct av_lereg *ler1 = ((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

int
le_syscon_match(parent, cf, aux)
        struct device *parent;
        void *cf, *aux;
{
        return (1);
}

void
le_syscon_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct le_softc *lesc = (struct le_softc *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
        struct confargs *ca = aux;
	extern void *etherbuf;
	extern size_t etherlen;

	if (etherbuf == NULL) {
		printf(": no available memory, kernel is too large\n");
		return;
	}

        lesc->sc_r1 = (struct av_lereg *)ca->ca_paddr;

        sc->sc_mem = (void *)etherbuf;
	etherbuf = NULL;
        sc->sc_conf3 = LE_C3_BSWP;
        sc->sc_addr = (u_long)sc->sc_mem & 0x00ffffff;
        sc->sc_memsize = etherlen;

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_rdcsr = le_syscon_rdcsr;
	sc->sc_wrcsr = le_syscon_wrcsr;
	sc->sc_hwreset = NULL;
	sc->sc_hwinit = NULL;

	am7990_config(sc);

	lesc->sc_ih.ih_fn = am7990_intr;
	lesc->sc_ih.ih_arg = sc;
	lesc->sc_ih.ih_wantframe = 0;
	lesc->sc_ih.ih_ipl = ca->ca_ipl;

	sysconintr_establish(SYSCV_LE, &lesc->sc_ih, self->dv_xname);
}
