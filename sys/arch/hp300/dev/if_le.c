/*	$OpenBSD: if_le.c,v 1.12 1998/09/16 22:41:17 jason Exp $	*/
/*	$NetBSD: if_le.c,v 1.43 1997/05/05 21:05:32 thorpej Exp $	*/

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
#include <machine/intr.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/if_lereg.h>
#include <hp300/dev/if_levar.h>

#include "opt_useleds.h"
#ifdef USELEDS
#include <hp300/hp300/leds.h>
#endif

int	lematch __P((struct device *, void *, void *));
void	leattach __P((struct device *, struct device *, void *));

struct cfattach le_ca = {
	sizeof(struct le_softc), lematch, leattach
};

int	leintr __P((void *));

/* offsets for:	   ID,   REGS,    MEM,  NVRAM */
int	lestd[] = { 0, 0x4000, 0x8000, 0xC008 };

hide void lewrcsr __P((struct am7990_softc *, u_int16_t, u_int16_t));
hide u_int16_t lerdcsr __P((struct am7990_softc *, u_int16_t));  

hide void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct lereg0 *ler0 = ((struct le_softc *)sc)->sc_r0;
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;

	do {
		ler1->ler1_rap = port;
	} while ((ler0->ler0_status & LE_ACK) == 0);
	do {
		ler1->ler1_rdp = val;
	} while ((ler0->ler0_status & LE_ACK) == 0);
}

hide u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct lereg0 *ler0 = ((struct le_softc *)sc)->sc_r0;
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	do {
		ler1->ler1_rap = port;
	} while ((ler0->ler0_status & LE_ACK) == 0);
	do {
		val = ler1->ler1_rdp;
	} while ((ler0->ler0_status & LE_ACK) == 0);
	return (val);
}

int
lematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct dio_attach_args *da = aux;

	if ((da->da_id == DIO_DEVICE_ID_LAN) ||
	    (da->da_id == DIO_DEVICE_ID_LANREM))
		return (1);
	return (0);
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lereg0 *ler0;
	struct dio_attach_args *da = aux;
	struct le_softc *lesc = (struct le_softc *)self;
	caddr_t addr;
	struct am7990_softc *sc = &lesc->sc_am7990;
	char *cp;
	int i, ipl;

	addr = iomap(dio_scodetopa(da->da_scode), da->da_size);
	if (addr == 0) {
		printf("\n%s: can't map LANCE registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	ler0 = lesc->sc_r0 = (struct lereg0 *)(lestd[0] + (int)addr);
	ler0->ler0_id = 0xFF;
	DELAY(100);

	ipl = DIO_IPL(addr);
	printf(" ipl %d", ipl);

	lesc->sc_r1 = (struct lereg1 *)(lestd[1] + (int)addr);
	sc->sc_mem = (void *)(lestd[2] + (int)addr);
	sc->sc_conf3 = LE_C3_BSWP;
	sc->sc_addr = 0;
	sc->sc_memsize = 16384;

	/*
	 * Read the ethernet address off the board, one nibble at a time.
	 */
	cp = (char *)(lestd[3] + (int)addr);
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++) {
		sc->sc_arpcom.ac_enaddr[i] = (*++cp & 0xF) << 4;
		cp++;
		sc->sc_arpcom.ac_enaddr[i] |= *++cp & 0xF;
		cp++;
	}

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

	/* Establish the interrupt handler. */
	(void) dio_intr_establish(leintr, sc, ipl, IPL_NET);
	ler0->ler0_status = LE_IE;
}

int
leintr(arg)
	void *arg;
{
	struct am7990_softc *sc = arg;
#ifdef USELEDS
	u_int16_t isr;

	isr = lerdcsr(sc, LE_CSR0);

	if ((isr & LE_C0_INTR) == 0)
		return (0);

	if (isr & LE_C0_RINT)
		ledcontrol(0, 0, LED_LANRCV);

	if (isr & LE_C0_TINT)
		ledcontrol(0, 0, LED_LANXMT);
#endif /* USELEDS */

	return (am7990_intr(sc));
}
