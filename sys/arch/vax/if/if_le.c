/*	$OpenBSD: if_le.c,v 1.4 1998/09/16 22:41:19 jason Exp $	*/
/*	$NetBSD: if_le.c,v 1.8 1997/04/21 22:04:23 ragge Exp $	*/

/* #define LE_CHIP_IS_POKEY	/* does VS2000 need this ??? */

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <net/if.h>

#if INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

/*
 * This would be nice, but it's not yet there...
 *
 * #include <machine/autoconf.h>
 */

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/vsbus.h>
#include <machine/rpb.h>

#include <dev/ic/am7990reg.h>
#define LE_NEED_BUF_CONTIG
#include <dev/ic/am7990var.h>

#include <dev/tc/if_levar.h>

#define xdebug(x)

#ifdef LE_CHIP_IS_POKEY
/* 
 * access LANCE registers and double-check their contents
 */
#define wbflush()	/* do nothing */
void lewritereg();
#define LERDWR(cntl, src, dst)	{ (dst) = (src); wbflush(); }
#define LEWREG(src, dst)	lewritereg(&(dst), (src))
#endif

#define LE_IOSIZE 64*1024	/* 64K of real-mem are reserved and already */
extern void *le_iomem;		/* mapped into virt-mem by cpu_steal_pages */
extern u_long le_ioaddr;	/* le_iomem is virt, le_ioaddr is phys */

#define LE_SOFTC(unit)	le_cd.cd_devs[unit]
#define LE_DELAY(x)	DELAY(x)

int lematch __P((struct device *, void *, void *));
void leattach __P((struct device *, struct device *, void *));

int leintr __P((void *sc));

struct cfattach le_ca = {
	sizeof(struct le_softc), lematch, leattach
};

hide void lewrcsr __P ((struct am7990_softc *, u_int16_t, u_int16_t));
hide u_int16_t lerdcsr __P ((struct am7990_softc *, u_int16_t));

hide void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;

#ifdef LE_CHIP_IS_POKEY
	LEWREG(port, ler1->ler1_rap);
	LERDWR(port, val, ler1->ler1_rdp);
#else
	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
#endif
}

hide u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

#ifdef LE_CHIP_IS_POKEY
	LEWREG(port, ler1->ler1_rap);
	LERDWR(0, ler1->ler1_rdp, val);
#else
	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
#endif
	return (val);
}

integrate void
lehwinit(sc)
	struct am7990_softc *sc;
{
}

int
lematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;

	/*
	 * There could/should be more checks, but for now...
	 */
	if (strcmp(ca->ca_name, "le") &&
	    strcmp(ca->ca_name, "am7990") &&
	    strcmp(ca->ca_name, "AM7990"))
		return (0);

	return (1);
}

/*
 *
 */
void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct le_softc *sc = (void *)self;
	struct confargs *ca = aux;
	u_char *cp;	/* pointer to MAC address */
	int i;

	sc->sc_r1  = (void*)uvax_phys2virt(ca->ca_ioaddr);

	sc->sc_am7990.sc_conf3 = 0;
	sc->sc_am7990.sc_mem = le_iomem;
	sc->sc_am7990.sc_addr = le_ioaddr;
	sc->sc_am7990.sc_memsize = LE_IOSIZE;
	sc->sc_am7990.sc_wrcsr = lewrcsr;
	sc->sc_am7990.sc_rdcsr = lerdcsr;
	sc->sc_am7990.sc_hwinit = lehwinit;
	sc->sc_am7990.sc_nocarrier = NULL;

	xdebug(("leattach: mem=%x, addr=%x, size=%x (%d)\n",
	    sc->sc_am7990.sc_mem, sc->sc_am7990.sc_addr,
	    sc->sc_am7990.sc_memsize, sc->sc_am7990.sc_memsize));

	sc->sc_am7990.sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_am7990.sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_am7990.sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_am7990.sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_am7990.sc_zerobuf = am7990_zerobuf_contig;

	/*
	 * Get the ethernet address out of rom
	 */
	for (i = 0; i < sizeof(sc->sc_am7990.sc_arpcom.ac_enaddr); i++) {
		int *eaddr = (void*)uvax_phys2virt(ca->ca_enaddr);
		sc->sc_am7990.sc_arpcom.ac_enaddr[i] = (u_char)eaddr[i];
	}

	bcopy(self->dv_xname, sc->sc_am7990.sc_arpcom.ac_if.if_xname, IFNAMSIZ);
	am7990_config(&sc->sc_am7990);

#ifdef LEDEBUG
	sc->sc_am7990.sc_debug = LEDEBUG;
#endif

	vsbus_intr_register(ca, am7990_intr, &sc->sc_am7990);
	vsbus_intr_enable(ca);

	/*
	 * Register this device as boot device if we booted from it.
	 * This will fail if there are more than one le in a machine,
	 * fortunately there may be only one.
	 */
	if (B_TYPE(bootdev) == BDEV_LE)
		booted_from = self;
}

#ifdef LE_CHIP_IS_POKEY
/*
 * Write a lance register port, reading it back to ensure success. This seems
 * to be necessary during initialization, since the chip appears to be a bit
 * pokey sometimes.
 */
void
lewritereg(regptr, val)
	register volatile u_short *regptr;
	register u_short val;
{
	register int i = 0;

	while (*regptr != val) {
		*regptr = val;
		wbflush();
		if (++i > 10000) {
			printf("le: Reg did not settle (to x%x): x%x\n", val,
			    *regptr);
			return;
		}
		DELAY(100);
	}
}
#endif
