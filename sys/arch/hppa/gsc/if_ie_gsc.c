/*	$OpenBSD: if_ie_gsc.c,v 1.1 1999/01/03 23:59:18 mickey Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

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

#include <dev/ic/i82586reg.h>
#include <dev/ic/i82586var.h>

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

#define	IE_SCB	0x001e
#define	IE_ISCP	0x0040
#define	IE_SCP	0x0060
#define	IE_BUF	0x1000
#define	IE_EBUF	IE_SIZE
#define	IE_SIZE	0x4000

int	ie_gsc_probe __P((struct device *, void *, void *));
void	ie_gsc_attach __P((struct device *, struct device *, void *));

struct cfattach ie_gsc_ca = {
	sizeof(struct ie_softc), ie_gsc_probe, ie_gsc_attach
};

static int ie_media[] = {
	IFM_ETHER | IFM_10_2,
};
#define	IE_NMEDIA	(sizeof(ie_media) / sizeof(ie_media[0]))

void ie_gsc_reset __P((struct ie_softc *sc, int what));
int ie_gsc_intrhook __P((struct ie_softc *sc, int what));
void ie_gsc_attend __P((struct ie_softc *sc));
void ie_gsc_run __P((struct ie_softc *sc));
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

	r->ie_reset = 0;
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

int
ie_gsc_intrhook(sc, where)
	struct ie_softc *sc;
	int where;
{
	return 0;
}

u_int16_t
ie_gsc_read16(sc, offset)
	struct ie_softc *sc;
	int offset;
{
	return *(u_int16_t *)(sc->bh + offset);
}

void
ie_gsc_write16(sc, offset, v)
	struct ie_softc *sc;	
	int offset;
	u_int16_t v;
{
	*(u_int16_t *)(sc->bh + offset) = v;
}

void
ie_gsc_write24(sc, offset, addr)
	struct ie_softc *sc;	
	int offset;
	int addr;
{
	*(u_int16_t *)(sc->bh + offset + 0) = (addr      ) & 0xffff;
	*(u_int16_t *)(sc->bh + offset + 2) = (addr >> 16) & 0xffff;
}

void
ie_gsc_memcopyin(sc, p, offset, size)
	struct ie_softc	*sc;
	void *p;
	int offset;
	size_t size;
{
	register const u_int16_t *src = (void *)((u_long)sc->bh + offset);
	register u_int16_t *dst = p;
#ifdef DIAGNOSTIC
	if (size & 1)
		panic ("ie_gsc_memcopyin: odd size");
#endif
	bus_space_barrier(sc->bt, sc->bh, offset, size,
			  BUS_SPACE_BARRIER_READ);
	for (; size; size -= 2)
		*dst++ = *src++;
}

void
ie_gsc_memcopyout(sc, p, offset, size)
	struct ie_softc	*sc;
	const void *p;
	int offset;
	size_t size;
{
	register u_int16_t *dst = (void *)((u_long)sc->bh + offset);
	register const u_int16_t *src = p;
#ifdef DIAGNOSTIC
	if (size & 1)
		panic ("ie_gsc_memcopyout: odd size");
#endif
	for (; size; size -= 2)
		*dst++ = *src++;
	bus_space_barrier(sc->bt, sc->bh, offset, size,
			  BUS_SPACE_BARRIER_WRITE);
}


int
ie_gsc_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct gsc_attach_args *ga = aux;
	bus_space_handle_t ioh;
	int rv;

	if (ga->ga_type.iodc_type != HPPA_TYPE_FIO ||
	    (ga->ga_type.iodc_sv_model != HPPA_FIO_LAN &&
	     ga->ga_type.iodc_sv_model != HPPA_FIO_GLAN))
		return 0;

	if (bus_space_map(ga->ga_iot, ga->ga_hpa, IOMOD_HPASIZE, 0, &ioh))
		return 0;

	rv = 1 /* i82586_probe(ga->ga_iot, ioh) */;

	bus_space_unmap(ga->ga_iot, ioh, IOMOD_HPASIZE);
	return rv;
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

	if (ga->ga_type.iodc_sv_model == HPPA_FIO_GLAN)
		sc->sc_flags |= IEGSC_GECKO;

	if ((i = pdc_call((iodcio_t)pdc, 0, PDC_LAN_STATION_ID,
			  PDC_LAN_STATION_ID_READ, &pdc_mac, ga->ga_hpa)) < 0){
		pdc_mac.addr[0] = ((u_int8_t *)ASP_PROM)[0];
		pdc_mac.addr[1] = ((u_int8_t *)ASP_PROM)[1];
		pdc_mac.addr[2] = ((u_int8_t *)ASP_PROM)[2];
		pdc_mac.addr[3] = ((u_int8_t *)ASP_PROM)[3];
		pdc_mac.addr[4] = ((u_int8_t *)ASP_PROM)[4];
		pdc_mac.addr[5] = ((u_int8_t *)ASP_PROM)[5];
	}

	sc->bt = ga->ga_iot;
	if (bus_space_map(sc->bt, ga->ga_hpa, IOMOD_HPASIZE,
			  0, (bus_space_handle_t *)&sc->sc_reg))
		panic("ie_gsc_attach: couldn't map I/O ports");

	sc->hwreset = ie_gsc_reset;
	sc->chan_attn = ie_gsc_attend;
	sc->hwinit = ie_gsc_run;
	sc->intrhook = ie_gsc_intrhook;
	sc->memcopyout = ie_gsc_memcopyout;
	sc->memcopyin = ie_gsc_memcopyin;
	sc->ie_bus_read16 = ie_gsc_read16;
	sc->ie_bus_write16 = ie_gsc_write16;
	sc->ie_bus_write24 = ie_gsc_write24;

	sc->sc_msize = IE_SIZE;
	if (!(sc->bh = (bus_space_handle_t)
	      kmem_malloc(kmem_map, sc->sc_msize, 0))) {
		printf (": cannot allocate %d bytes for IO\n", IE_SIZE);
		return;
	}

	bzero((void *)sc->bh, sc->sc_msize);
	sc->sc_maddr = (void *)kvtop((void *)sc->bh);

	printf (" irq %d: mem %p size %x\n%s: ", sc->sc_dev.dv_cfdata->cf_irq,
		sc->sc_maddr, sc->sc_msize, sc->sc_dev.dv_xname);

	sc->iscp = IE_ISCP;
	sc->scb = IE_SCB;
	sc->scp = IE_SCP;
	sc->buf_area = IE_BUF;
	sc->buf_area_sz = IE_EBUF - IE_BUF;

	a = ((u_int32_t)sc->sc_maddr + IE_SCP) | IE_PORT_TEST;
	*(int32_t *)(IE_SCP_TEST(sc->scp)) = -1;
	if (sc->sc_flags & IEGSC_GECKO) {
		register volatile struct ie_gsc_regs *r = sc->sc_reg;
		r->ie_port = a;
		DELAY(1000);
		r->ie_port = a >> 16;
		DELAY(1000);
	} else {
		register volatile struct ie_gsc_regs *r = sc->sc_reg;
		r->ie_port = a >> 16;
		DELAY(1000);
		r->ie_port = a;
		DELAY(1000);
	}
	for (i = 900; i-- && *(int32_t *)(IE_SCP_TEST(sc->scp)); DELAY(100))
		bus_space_barrier(sc->bt, sc->bh, IE_SCP, 8,
				  BUS_SPACE_BARRIER_READ);

#ifdef I82586_DEBUG
	printf ("test %x:%x ", ((int32_t *)(sc->bh + IE_SCP))[0],
		((int32_t *)(sc->bh + IE_SCP))[1]);
#endif

	ie_gsc_write16(sc, IE_ISCP_BUSY(sc->iscp), 1);
	ie_gsc_write16(sc, IE_ISCP_SCB(sc->iscp), sc->scb);
	ie_gsc_write24(sc, IE_ISCP_BASE(sc->iscp), (u_long)sc->sc_maddr);
	bus_space_barrier(sc->bt, sc->bh, sc->iscp, IE_ISCP_SZ,
			  BUS_SPACE_BARRIER_READ);
	ie_gsc_write16(sc, IE_SCP_BUS_USE(sc->scp), 0x68);
	ie_gsc_write24(sc, IE_SCP_ISCP(sc->scp),
		       (u_long)sc->sc_maddr + sc->iscp);
	bus_space_barrier(sc->bt, sc->bh, sc->scp, IE_SCP_SZ,
			  BUS_SPACE_BARRIER_READ);

	/* inform i825[89]6 about new SCP address,
	   maddr must be at least 16-byte aligned */
	a = ((u_int32_t)sc->sc_maddr + IE_SCP) | IE_PORT_SCP;
	if (sc->sc_flags & IEGSC_GECKO) {
		register volatile struct ie_gsc_regs *r = sc->sc_reg;
		r->ie_port = a;
		DELAY(1000);
		r->ie_port = a >> 16;
		DELAY(1000);
	} else {
		register volatile struct ie_gsc_regs *r = sc->sc_reg;
		r->ie_port = a >> 16;
		DELAY(1000);
		r->ie_port = a;
		DELAY(1000);
	}
	ie_gsc_attend(sc);

	for (i = 900; i-- &&
		     ie_gsc_read16(sc, IE_ISCP_BUSY(sc->iscp)); DELAY(100))
		bus_space_barrier(sc->bt, sc->bh, IE_ISCP_BUSY(sc->iscp), 2,
				  BUS_SPACE_BARRIER_READ);

#ifdef I82586_DEBUG
	if (i < 0) {
		printf("timeout for PORT command (%x)%s\n",
		       ie_gsc_read16(sc, IE_ISCP_BUSY(sc->iscp)),
		       (sc->sc_flags & IEGSC_GECKO)? " on gecko":"");
		return;
	}

	sc->sc_debug = IED_ALL;
#endif

	i82586_attach(sc, ga->ga_name, pdc_mac.addr,
		      ie_media, IE_NMEDIA, ie_media[0]);

	gsc_intr_establish((struct gsc_softc *)parent,
			   sc->sc_dev.dv_cfdata->cf_irq,
			   IPL_NET, i82586_intr, sc, sc->sc_dev.dv_xname);
}


