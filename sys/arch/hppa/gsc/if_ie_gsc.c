/*	$OpenBSD: if_ie_gsc.c,v 1.2 1999/08/14 04:08:06 mickey Exp $	*/

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

/*
 * Referencies:
 * 1. 82596DX and 82596SX High-Perfomance 32-bit Local Area Network Coprocessor
 *    Intel Corporation, November 1996, Order Number: 290219-006
 *
 * 2. 712 I/O Subsystem ERS Rev 1.0
 *    Hewlwtt-Packard, June 17 1992, Dwg No. A-A2263-66510-31
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>
#include <hppa/gsc/gscbusvar.h>

#include <dev/ic/i82596reg.h>
#include <dev/ic/i82596var.h>

#define	IEGSC_GECKO	IEMD_FLAG0

struct ie_gsc_regs {
	u_int32_t	ie_reset;
	u_int32_t	ie_port;
#define	IE_PORT_RESET	0
#define	IE_PORT_TEST	1
#define	IE_PORT_SCP	2
#define	IE_PORT_DUMP	3
	u_int32_t	ie_attn;
};

#define	IE_SIZE	0x8000

int	ie_gsc_probe __P((struct device *, void *, void *));
void	ie_gsc_attach __P((struct device *, struct device *, void *));

struct cfattach ie_gsc_ca = {
	sizeof(struct ie_softc), ie_gsc_probe, ie_gsc_attach
};

static int ie_gsc_media[] = {
	IFM_ETHER | IFM_10_2,
};
#define	IE_NMEDIA	(sizeof(ie_gsc_media) / sizeof(ie_gsc_media[0]))

void ie_gsc_reset __P((struct ie_softc *sc, int what));
void ie_gsc_attend __P((struct ie_softc *sc));
void ie_gsc_run __P((struct ie_softc *sc));
#ifdef USELEDS
int ie_gsc_intrhook __P((struct ie_softc *sc, int what));
#endif
u_int16_t ie_gsc_read16 __P((struct ie_softc *sc, int offset));
void ie_gsc_write16 __P((struct ie_softc *sc, int offset, u_int16_t v));
void ie_gsc_write24 __P((struct ie_softc *sc, int offset, int addr));
void ie_gsc_memcopyin __P((struct ie_softc *sc, void *p, int offset, size_t));
void ie_gsc_memcopyout __P((struct ie_softc *sc, const void *p, int, size_t));


void
ie_gsc_reset(sc, what)
	struct ie_softc *sc;
	int what;
{
	register volatile struct ie_gsc_regs *r = sc->sc_reg;
	register u_int32_t a;
	register int i;
	
	switch (what) {
	case CHIP_PROBE:
		r->ie_reset = 0;
		break;

	case CARD_RESET:
		r->ie_reset = 0;

		/*
		 * per [2] 4.6.2.1
		 * delay for 10 system clocks + 5 transmit clocks,
		 * NB: works for system clocks over 10MHz
		 */
		DELAY(1000);

		/*
		 * after the hardware reset:
		 * inform i825[89]6 about new SCP address,
		 * maddr must be at least 16-byte aligned
		 */
		a = ((u_int32_t)sc->sc_maddr + sc->scp) | IE_PORT_SCP;
		if (sc->sc_flags & IEGSC_GECKO) {
			register volatile struct ie_gsc_regs *r = sc->sc_reg;
			r->ie_port = a & 0xffff;
			DELAY(1000);
			r->ie_port = a >> 16;
			DELAY(1000);
		} else {
			register volatile struct ie_gsc_regs *r = sc->sc_reg;
			r->ie_port = a >> 16;
			DELAY(1000);
			r->ie_port = a & 0xffff;
			DELAY(1000);
		}
		ie_gsc_attend(sc);

		for (i = 9000; i-- && ie_gsc_read16(sc, IE_ISCP_BUSY(sc->iscp));
		     DELAY(100));

#ifdef I82596_DEBUG
		if (i < 0) {
			printf("timeout for PORT command (%x)%s\n",
			       ie_gsc_read16(sc, IE_ISCP_BUSY(sc->iscp)),
			       (sc->sc_flags & IEGSC_GECKO)? " on gecko":"");
			return;
		}
#endif
		break;
	}
}

void
ie_gsc_attend(sc)
	struct ie_softc *sc;
{
	register volatile struct ie_gsc_regs *r = sc->sc_reg;

	r->ie_attn = 0;
}

void
ie_gsc_run(sc)
	struct ie_softc *sc;
{
}

#ifdef USELEDS
int
ie_gsc_intrhook(sc, where)
	struct ie_softc *sc;
	int where;
{
	switch (where) {
	case INTR_ENTER:
		/* turn it on */
		break;
	case INTR_LOOP:
		/* quick drop and raise */
		break;
	case INTR_EXIT:
		/* drop it */
		break;
	}
	return 0;
}
#endif

u_int16_t
ie_gsc_read16(sc, offset)
	struct ie_softc *sc;
	int offset;
{
	return *(volatile u_int16_t *)(sc->bh + offset);
}

void
ie_gsc_write16(sc, offset, v)
	struct ie_softc *sc;	
	int offset;
	u_int16_t v;
{
	*(volatile u_int16_t *)(sc->bh + offset) = v;
}

void
ie_gsc_write24(sc, offset, addr)
	struct ie_softc *sc;	
	int offset;
	int addr;
{
	*(volatile u_int16_t *)(sc->bh + offset + 0) = (addr      ) & 0xffff;
	*(volatile u_int16_t *)(sc->bh + offset + 2) = (addr >> 16) & 0xffff;
}

void
ie_gsc_memcopyin(sc, p, offset, size)
	struct ie_softc	*sc;
	void *p;
	int offset;
	size_t size;
{
	bcopy ((void *)((u_long)sc->bh + offset), p, size);
}

void
ie_gsc_memcopyout(sc, p, offset, size)
	struct ie_softc	*sc;
	const void *p;
	int offset;
	size_t size;
{
	bcopy (p, (void *)((u_long)sc->bh + offset), size);
}


int
ie_gsc_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct gsc_attach_args *ga = aux;

	if (ga->ga_type.iodc_type != HPPA_TYPE_FIO ||
	    (ga->ga_type.iodc_sv_model != HPPA_FIO_LAN &&
	     ga->ga_type.iodc_sv_model != HPPA_FIO_GLAN))
		return 0;

	return 1;
}

void
ie_gsc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pdc_lan_station_id pdc_mac PDC_ALIGNMENT;
	register struct ie_softc *sc = (struct ie_softc *)self;
	register struct gsc_attach_args *ga = aux;
	register u_int32_t a;
	register int i;
	bus_dma_segment_t seg;
	int rseg;
#ifdef PMAPDEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;
	pmapdebug = 0;
#endif

	if (ga->ga_type.iodc_sv_model == HPPA_FIO_GLAN)
		sc->sc_flags |= IEGSC_GECKO;

	if ((i = pdc_call((iodcio_t)pdc, 0, PDC_LAN_STATION_ID,
			  PDC_LAN_STATION_ID_READ, &pdc_mac, ga->ga_hpa)) < 0)
		bcopy ((void *)ASP_PROM, sc->sc_arpcom.ac_enaddr,
		       ETHER_ADDR_LEN);
	else
		bcopy (pdc_mac.addr, sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->bt = ga->ga_iot;
	sc->sc_reg = (struct ie_gsc_regs *)ga->ga_hpa;
	sc->hwreset = ie_gsc_reset;
	sc->chan_attn = ie_gsc_attend;
	sc->hwinit = ie_gsc_run;
#ifdef USELEDS
	sc->intrhook = ie_gsc_intrhook;
#else
	sc->intrhook = NULL;
#endif
	sc->memcopyout = ie_gsc_memcopyout;
	sc->memcopyin = ie_gsc_memcopyin;
	sc->ie_bus_read16 = ie_gsc_read16;
	sc->ie_bus_write16 = ie_gsc_write16;
	sc->ie_bus_write24 = ie_gsc_write24;

	sc->sc_msize = IE_SIZE;
	if (bus_dmamem_alloc(ga->ga_dmatag, sc->sc_msize, NBPG, 0,
			     &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf (": cannot allocate %d bytes of DMA memory\n",
			sc->sc_msize);
		return;
	}
	if (bus_dmamem_map(ga->ga_dmatag, &seg, rseg, sc->sc_msize,
			   (caddr_t *)&sc->bh, BUS_DMA_NOWAIT)) {
		printf (": cannot map DMA memory\n");
		bus_dmamem_free(ga->ga_dmatag, &seg, rseg);
		return;
	}

	bzero((void *)sc->bh, sc->sc_msize);
	sc->sc_maddr = kvtop((caddr_t)sc->bh);

	printf (": mem %x[%p]/%x", sc->bh, sc->sc_maddr, sc->sc_msize);

	sc->iscp = 0;
	sc->scp = sc->iscp + IE_ISCP_SZ;
	sc->scb = sc->scp + IE_SCP_SZ;
	sc->buf_area = sc->scb + IE_SCB_SZ;
	sc->buf_area_sz = sc->sc_msize - sc->buf_area;

	a = ((u_int32_t)sc->sc_maddr + sc->scp) | IE_PORT_TEST;
	*(volatile int32_t *)(sc->bh + IE_SCP_TEST(sc->scp)) = -1;
	if (sc->sc_flags & IEGSC_GECKO) {
		register volatile struct ie_gsc_regs *r = sc->sc_reg;
		r->ie_port = a & 0xffff;
		DELAY(1000);
		r->ie_port = a >> 16;
		DELAY(1000);
	} else {
		register volatile struct ie_gsc_regs *r = sc->sc_reg;
		r->ie_port = a >> 16;
		DELAY(1000);
		r->ie_port = a & 0xffff;
		DELAY(1000);
	}
	for (i = 9000; i-- && *(volatile int32_t *)
		     (sc->bh + IE_SCP_TEST(sc->scp));
	     DELAY(100));

#ifdef I82596_DEBUG
	printf ("test %x:%x ", *((volatile int32_t *)(sc->bh + sc->scp)),
		*(int32_t *)(sc->bh + IE_SCP_TEST(sc->scp)));

	sc->sc_debug = IED_ALL;
#endif

	ie_gsc_write16(sc, IE_ISCP_BUSY(sc->iscp), 1);
	ie_gsc_write16(sc, IE_ISCP_SCB(sc->iscp), sc->scb);
	ie_gsc_write24(sc, IE_ISCP_BASE(sc->iscp), sc->sc_maddr);

	ie_gsc_write24(sc, IE_SCP_ISCP(sc->scp), sc->sc_maddr + sc->iscp);
	ie_gsc_write16(sc, IE_SCP_BUS_USE(sc->scp), 0x40 | IE_SYSBUS_82586 |
		       IE_SYSBUS_INTLOW | IE_SYSBUS_TRG | IE_SYSBUS_BE);

	ie_gsc_reset(sc, CARD_RESET);
	ie_gsc_attend(sc);
	delay(100);

	sc->sc_type = sc->sc_flags & IEGSC_GECKO? "LASI/i82596CA" : "i82596DX";
	i82596_attach(sc, sc->sc_type, (char *)sc->sc_arpcom.ac_enaddr,
		      ie_gsc_media, IE_NMEDIA, ie_gsc_media[0]);

	sc->sc_ih = gsc_intr_establish((struct gsc_softc *)parent, IPL_NET,
				       ga->ga_irq, i82596_intr, sc,
				       sc->sc_dev.dv_xname);
#ifdef PMAPDEBUG
	pmapdebug = opmapdebug;
#endif
}
