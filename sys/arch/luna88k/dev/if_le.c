/*	$OpenBSD: if_le.c,v 1.4 2004/08/30 13:10:32 aoyama Exp $	*/
/*	$NetBSD: if_le.c,v 1.33 1996/11/20 18:56:52 gwr Exp $	*/

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

/* based on OpenBSD: sys/arch/sun3/dev/if_le.c */

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
#include <machine/board.h>
#include <machine/cpu.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <luna88k/luna88k/isr.h>

/*
 * LANCE registers.
 * The real stuff is in dev/ic/am7990reg.h
 */
struct lereg1 {
	volatile u_int16_t	ler1_rdp;	/* data port */
	volatile unsigned	: 16 ;		/* LUNA-88K2 has a 16 bit gap */
	volatile u_int16_t	ler1_rap;	/* register select port */
};

/*
 * Ethernet software status per interface.
 * The real stuff is in dev/ic/am7990var.h
 */
struct	le_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	struct	lereg1 *sc_r1;		/* LANCE registers */
};

static int	le_match(struct device *, void *, void *);
static void	le_attach(struct device *, struct device *, void *);

struct cfattach le_ca = {
	sizeof(struct le_softc), le_match, le_attach
};

hide void lewrcsr(struct am7990_softc *, u_int16_t, u_int16_t);
hide u_int16_t lerdcsr(struct am7990_softc *, u_int16_t);
hide void myetheraddr(u_int8_t *);

hide void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	register struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

hide u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	register struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

static int
le_match(parent, cf, aux)
        struct device *parent;
        void *cf, *aux;
{
        struct mainbus_attach_args *ma = aux;

        if (strcmp(ma->ma_name, le_cd.cd_name))
                return (0);

        return (1);
}

void
le_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct le_softc *lesc = (struct le_softc *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
        struct mainbus_attach_args *ma = aux;

        lesc->sc_r1 = (struct lereg1 *)ma->ma_addr;     /* LANCE */

        sc->sc_mem = (void *)0x71000000;                /* SRAM */
        sc->sc_conf3 = LE_C3_BSWP;
        sc->sc_addr = (u_long)sc->sc_mem & 0xffffff;
        sc->sc_memsize = 64 * 1024;                     /* 64KB */

        myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_rdcsr = lerdcsr;
	sc->sc_wrcsr = lewrcsr;
	sc->sc_hwreset = NULL;
	sc->sc_hwinit = NULL;

	am7990_config(sc);

        isrlink_autovec(am7990_intr, (void *)sc, ma->ma_ilvl, ISRPRI_NET,
	    self->dv_xname);
}

/*
 * Partially taken from NetBSD/luna68k
 * 
 * LUNA-88K has FUSE ROM, which contains MAC address.  The FUSE ROM
 * contents are stored in fuse_rom_data[] during cpu_startup(). 
 * 
 * LUNA-88K2 has 16Kbit NVSRAM on its ethercard, whose contents are
 * accessible 4bit-wise by ctl register operation.  The register is
 * mapped at 0xF1000008.
 */

extern int machtype;
extern char fuse_rom_data[];

hide void
myetheraddr(ether)
        u_int8_t *ether;
{
        unsigned i, loc;
	volatile struct { u_int32_t ctl; } *ds1220;

	switch (machtype) {
	case LUNA_88K:
		/*
		 * fuse_rom_data[] begins with "ENADDR=00000Axxxxxx"
		 */
		loc = 7;
		for (i = 0; i < 6; i++) {
			int u, l;

			u = fuse_rom_data[loc];
			u = (u < 'A') ? u & 0xf : u - 'A' + 10;
			l = fuse_rom_data[loc + 1];
			l = (l < 'A') ? l & 0xf : l - 'A' + 10;
		
			ether[i] = l | (u << 4);
			loc += 2;
		}
		break;
	case LUNA_88K2: 
		ds1220 = (void *)0xF1000008;
		loc = 12;
		for (i = 0; i < 6; i++) {
			unsigned u, l, hex;

			ds1220->ctl = (loc) << 16;
			u = 0xf0 & (ds1220->ctl >> 12);
			ds1220->ctl = (loc + 1) << 16;
			l = 0x0f & (ds1220->ctl >> 16);
			hex = (u < '9') ? l : l + 9;

			ds1220->ctl = (loc + 2) << 16;
			u = 0xf0 & (ds1220->ctl >> 12);
			ds1220->ctl = (loc + 3) << 16;
			l = 0x0f & (ds1220->ctl >> 16);

			ether[i] = ((u < '9') ? l : l + 9) | (hex << 4);
			loc += 4;
		}
		break;
	default:
		ether[0] = 0x00; ether[1] = 0x00; ether[2] = 0x0a;
		ether[3] = 0xDE; ether[4] = 0xAD; ether[5] = 0x00;
		break;
	}
}
