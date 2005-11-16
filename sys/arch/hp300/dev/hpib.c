/*	$OpenBSD: hpib.c,v 1.13 2005/11/16 21:23:55 miod Exp $	*/
/*	$NetBSD: hpib.c,v 1.16 1997/04/27 20:58:57 thorpej Exp $	*/

/*
 * Copyright (c) 1996, 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)hpib.c	8.2 (Berkeley) 1/12/94
 */

/*
 * HP-IB bus driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <hp300/dev/dmavar.h>

#include <hp300/dev/hpibvar.h>

#include <machine/cpu.h>
#include <machine/hp300spu.h>

int	hpibbusmatch(struct device *, void *, void *);
void	hpibbusattach(struct device *, struct device *, void *);

struct cfattach hpibbus_ca = {
	sizeof(struct hpibbus_softc), hpibbusmatch, hpibbusattach
};

struct cfdriver hpibbus_cd = {
	NULL, "hpibbus", DV_DULL
};

void	hpibbus_attach_children(struct hpibbus_softc *);
int	hpibbussubmatch(struct device *, void *, void *);
int	hpibbusprint(void *, const char *);

void	hpibstart(void *);
void	hpibdone(void *);

int	hpibtimeout = 100000;	/* # of status tests before we give up */
int	hpibidtimeout = 10000;	/* # of status tests for hpibid() calls */
int	hpibdmathresh = 3;	/* byte count beyond which to attempt dma */

/*
 * HP-IB is essentially an IEEE 488 bus, with an HP command
 * set (CS/80 on `newer' devices, Amigo on before-you-were-born
 * devices) thrown on top.  Devices that respond to CS/80 (and
 * probably Amigo, too) are tagged with a 16-bit ID.
 *
 * HP-IB has a 2-level addressing scheme; slave, the analog
 * of a SCSI ID, and punit, the analog of a SCSI LUN.  Unfortunately,
 * IDs are on a per-slave basis; punits are often used for disk
 * drives that have an accompanying tape drive on the second punit.
 *
 * In addition, not all HP-IB devices speak CS/80 or Amigo.
 * Examples of such devices are HP-IB plotters, which simply
 * take raw plotter commands over 488.  These devices do not
 * have ID tags, and often the host cannot even tell if such
 * a device is attached to the system!
 *
 * We nevertheless probe the whole (slave, punit) tuple space, since
 * drivers for devices with a unique ID know exactly where to attach; 
 * and we disallow ``star'' locators for other drivers.
 */

int
hpibbusmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{

	return (1);
}

void
hpibbusattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct hpibbus_softc *sc = (struct hpibbus_softc *)self;
	struct hpibdev_attach_args *ha = aux;

	printf("\n");

	/* Get the operations vector for the controller. */
	sc->sc_ops = ha->ha_ops;
	sc->sc_type = ha->ha_type;		/* XXX */
	sc->sc_ba = ha->ha_ba;
	*(ha->ha_softcpp) = sc;			/* XXX */

	hpibreset(self->dv_unit);		/* XXX souldn't be here */

	/*
	 * Initialize the DMA queue entry.
	 */
	sc->sc_dq = (struct dmaqueue *)malloc(sizeof(struct dmaqueue),
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_dq == NULL) {
		printf("%s: can't allocate DMA queue entry\n", self->dv_xname);
		return;
	}
	sc->sc_dq->dq_softc = sc;
	sc->sc_dq->dq_start = hpibstart;
	sc->sc_dq->dq_done = hpibdone;

	/* Initialize the slave request queue. */
	TAILQ_INIT(&sc->sc_queue);

	/* Attach any devices on the bus. */
	hpibbus_attach_children(sc);
}

void
hpibbus_attach_children(sc)
	struct hpibbus_softc *sc;
{
	struct hpibbus_attach_args ha;
	int id, slave, punit;
	int i;

	for (slave = 0; slave < HPIB_NSLAVES; slave++) {
		/*
		 * Get the ID tag for the device, if any.
		 * Plotters won't identify themselves, and
		 * get the same value as non-existent devices.
		 * However, aging HP-IB drives are slow to respond; try up
		 * to three times to get a valid ID.
		 */
		for (i = 0; i < 3; i++) {
			id = hpibid(sc->sc_dev.dv_unit, slave);
			if ((id & 0x200) != 0)
				break;
			delay(10000);
		}

		for (punit = 0; punit < HPIB_NPUNITS; punit++) {
			/*
			 * Search through all configured children for this bus.
			 */
			ha.ha_id = id;
			ha.ha_slave = slave;
			ha.ha_punit = punit;
			(void)config_found_sm(&sc->sc_dev, &ha, hpibbusprint,
			    hpibbussubmatch);
		}
	}
}

int
hpibbussubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct hpibbus_attach_args *ha = aux;

	if (cf->hpibbuscf_slave != HPIBBUS_SLAVE_UNK &&
	    cf->hpibbuscf_slave != ha->ha_slave)
		return (0);
	if (cf->hpibbuscf_punit != HPIBBUS_PUNIT_UNK &&
	    cf->hpibbuscf_punit != ha->ha_punit)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}

int
hpibbusprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct hpibbus_attach_args *ha = aux;

	if (pnp != NULL) {
		if (ha->ha_id == 0 || ha->ha_punit != 0 /* XXX */)
			return (QUIET);
		printf("HP-IB device (id %04X) at %s", ha->ha_id, pnp);
	}
	printf(" slave %d punit %d", ha->ha_slave, ha->ha_punit);
	return (UNCONF);
}

int
hpibdevprint(aux, pnp)
	void *aux;
	const char *pnp;
{

	/* only hpibbus's can attach to hpibdev's -- easy. */
	if (pnp != NULL)
		printf("hpibbus at %s", pnp);
	return (UNCONF);
}

void
hpibreset(unit)
	int unit;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	(*sc->sc_ops->hpib_reset)(sc);
}

int
hpibreq(pdev, hq)
	struct device *pdev;
	struct hpibqueue *hq;
{
	struct hpibbus_softc *sc = (struct hpibbus_softc *)pdev;
	int s;

	s = splhigh();	/* XXXthorpej */
	TAILQ_INSERT_TAIL(&sc->sc_queue, hq, hq_list);
	splx(s);

	if (TAILQ_FIRST(&sc->sc_queue) == hq)
		return (1);

	return (0);
}

void
hpibfree(pdev, hq)
	struct device *pdev;
	struct hpibqueue *hq;
{
	struct hpibbus_softc *sc = (struct hpibbus_softc *)pdev;
	int s;

	s = splhigh();	/* XXXthorpej */
	TAILQ_REMOVE(&sc->sc_queue, hq, hq_list);
	splx(s);

	if ((hq = TAILQ_FIRST(&sc->sc_queue)) != NULL)
		(*hq->hq_start)(hq->hq_softc);
}

int
hpibid(unit, slave)
	int unit, slave;
{
	short id;
	int ohpibtimeout;

	/*
	 * XXX shorten timeout value so autoconfig doesn't
	 * take forever on slow CPUs.
	 */
	ohpibtimeout = hpibtimeout;
	hpibtimeout = hpibidtimeout * (cpuspeed / 8);
	if (hpibrecv(unit, 31, slave, &id, 2) != 2)
		id = 0;
	hpibtimeout = ohpibtimeout;
	return(id);
}

int
hpibsend(unit, slave, sec, addr, cnt)
	int unit, slave, sec, cnt;
	void *addr;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	return ((*sc->sc_ops->hpib_send)(sc, slave, sec, addr, cnt));
}

int
hpibrecv(unit, slave, sec, addr, cnt)
	int unit, slave, sec, cnt;
	void *addr;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	return ((*sc->sc_ops->hpib_recv)(sc, slave, sec, addr, cnt));
}

int
hpibpptest(unit, slave)
	int unit;
	int slave;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	return ((*sc->sc_ops->hpib_ppoll)(sc) & (0x80 >> slave));
}

void
hpibppclear(unit)
	int unit;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	sc->sc_flags &= ~HPIBF_PPOLL;
}

void
hpibawait(unit)
	int unit;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	sc->sc_flags |= HPIBF_PPOLL;
	(*sc->sc_ops->hpib_ppwatch)(sc);
}

int
hpibswait(unit, slave)
	int unit;
	int slave;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];
	int timo = hpibtimeout;
	int mask, (*ppoll)(struct hpibbus_softc *);

	ppoll = sc->sc_ops->hpib_ppoll;
	mask = 0x80 >> slave;
	while (((*ppoll)(sc) & mask) == 0) {
		if (--timo == 0) {
			printf("%s: swait timeout\n", sc->sc_dev.dv_xname);
			return(-1);
		}
	}
	return(0);
}

int
hpibustart(unit)
	int unit;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	if (sc->sc_type == HPIBA)
		sc->sc_dq->dq_chan = DMA0;
	else
		sc->sc_dq->dq_chan = DMA0 | DMA1;
	if (dmareq(sc->sc_dq))
		return(1);
	return(0);
}

void
hpibstart(arg)
	void *arg;
{
	struct hpibbus_softc *sc = arg;
	struct hpibqueue *hq;

	hq = TAILQ_FIRST(&sc->sc_queue);
	(*hq->hq_go)(hq->hq_softc);
}

void
hpibgo(unit, slave, sec, vbuf, count, rw, timo)
	int unit, slave, sec;
	void *vbuf;
	int count, rw, timo;
{
	struct hpibbus_softc *sc = hpibbus_cd.cd_devs[unit];

	(*sc->sc_ops->hpib_go)(sc, slave, sec, vbuf, count, rw, timo);
}

void
hpibdone(arg)
	void *arg;
{
	struct hpibbus_softc *sc = arg;

	(*sc->sc_ops->hpib_done)(sc);
}

int
hpibintr(arg)
	void *arg;
{
	struct hpibbus_softc *sc = arg;

	return ((sc->sc_ops->hpib_intr)(arg));
}
