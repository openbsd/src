/*	$OpenBSD: ppi.c,v 1.15 2006/01/19 23:54:41 miod Exp $	*/
/*	$NetBSD: ppi.c,v 1.13 1997/04/02 22:37:33 scottr Exp $	*/

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
 *	@(#)ppi.c	8.1 (Berkeley) 6/16/93
 */

/*
 * Printer/Plotter HPIB interface
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/timeout.h>
#include <sys/uio.h>

#include <hp300/dev/hpibvar.h>

#include <hp300/dev/ppiioctl.h>

struct	ppi_softc {
	struct device sc_dev;
	int	sc_flags;
	struct	hpibqueue sc_hq;	/* HP-IB job queue entry */
	struct	ppiparam sc_param;
#define sc_burst sc_param.burst
#define sc_timo  sc_param.timo
#define sc_delay sc_param.delay
	int	sc_sec;
	int	sc_slave;		/* HP-IB slave address */
	struct timeout sc_to;		/* ppitimo timeout */
	struct timeout sc_start_to;	/* ppistart timeout */
};

/* sc_flags values */
#define	PPIF_ALIVE	0x01
#define	PPIF_OPEN	0x02
#define PPIF_UIO	0x04
#define PPIF_TIMO	0x08
#define PPIF_DELAY	0x10

int	ppimatch(struct device *, void *, void *);
void	ppiattach(struct device *, struct device *, void *);

struct cfattach ppi_ca = {
	sizeof(struct ppi_softc), ppimatch, ppiattach
};

struct cfdriver ppi_cd = {
	NULL, "ppi", DV_DULL
};

void	ppistart(void *);
void	ppinoop(void *);

void	ppitimo(void *);
int	ppirw(dev_t, struct uio *);
int	ppihztoms(int);
int	ppimstohz(int);

bdev_decl(ppi);
cdev_decl(ppi);

#define UNIT(x)		minor(x)

#ifdef DEBUG
int	ppidebug = 0x80;
#define PDB_FOLLOW	0x01
#define PDB_IO		0x02
#define PDB_NOCHECK	0x80
#endif

int
ppimatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct hpibbus_attach_args *ha = aux;

	/*
	 * The printer/plotter doesn't return an ID tag.
	 * The check below prevents us from matching a CS80
	 * device by mistake.
	 */
	if (ha->ha_id & 0x200)
		return (0);

	/*
	 * To prevent matching all unused slots on the bus, we
	 * don't allow wildcarded locators.
	 */
	if (cf->hpibbuscf_slave == HPIBBUS_SLAVE_UNK ||
	    cf->hpibbuscf_punit == HPIBBUS_PUNIT_UNK)
		return (0);

	return (1);
}

void
ppiattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ppi_softc *sc = (struct ppi_softc *)self;
	struct hpibbus_attach_args *ha = aux;

	printf("\n");

	sc->sc_slave = ha->ha_slave;

	/* Initialize the hpib queue entry. */
	sc->sc_hq.hq_softc = sc;
	sc->sc_hq.hq_slave = sc->sc_slave;
	sc->sc_hq.hq_start = ppistart;
	sc->sc_hq.hq_go = ppinoop;
	sc->sc_hq.hq_intr = ppinoop;

	sc->sc_flags = PPIF_ALIVE;

	/* Initialize timeout structures */
	timeout_set(&sc->sc_to, ppitimo, sc);
	timeout_set(&sc->sc_to, ppistart, sc);
}

void
ppinoop(arg)
	void *arg;
{
	/* Noop! */
}

int
ppiopen(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = UNIT(dev);
	struct ppi_softc *sc;

	if (unit >= ppi_cd.cd_ndevs ||
	    (sc = ppi_cd.cd_devs[unit]) == NULL ||
	    (sc->sc_flags & PPIF_ALIVE) == 0)
		return (ENXIO);

#ifdef DEBUG
	if (ppidebug & PDB_FOLLOW)
		printf("ppiopen(%x, %x): flags %x\n",
		       dev, flags, sc->sc_flags);
#endif
	if (sc->sc_flags & PPIF_OPEN)
		return (EBUSY);
	sc->sc_flags |= PPIF_OPEN;
	sc->sc_burst = PPI_BURST;
	sc->sc_timo = ppimstohz(PPI_TIMO);
	sc->sc_delay = ppimstohz(PPI_DELAY);
	sc->sc_sec = -1;
	return(0);
}

int
ppiclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = UNIT(dev);
	struct ppi_softc *sc = ppi_cd.cd_devs[unit];

#ifdef DEBUG
	if (ppidebug & PDB_FOLLOW)
		printf("ppiclose(%x, %x): flags %x\n",
		       dev, flags, sc->sc_flags);
#endif
	sc->sc_flags &= ~PPIF_OPEN;
	return(0);
}

void
ppistart(arg)
	void *arg;
{
	struct ppi_softc *sc = arg;

#ifdef DEBUG
	if (ppidebug & PDB_FOLLOW)
		printf("ppistart(%x)\n", sc->sc_dev.dv_unit);
#endif
	sc->sc_flags &= ~PPIF_DELAY;
	wakeup(sc);
}

void
ppitimo(arg)
	void *arg;
{
	struct ppi_softc *sc = arg;

#ifdef DEBUG
	if (ppidebug & PDB_FOLLOW)
		printf("ppitimo(%x)\n", sc->sc_dev.dv_unit);
#endif
	sc->sc_flags &= ~(PPIF_UIO|PPIF_TIMO);
	wakeup(sc);
}

int
ppiread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

#ifdef DEBUG
	if (ppidebug & PDB_FOLLOW)
		printf("ppiread(%x, %p)\n", dev, uio);
#endif
	return (ppirw(dev, uio));
}

int
ppiwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

#ifdef DEBUG
	if (ppidebug & PDB_FOLLOW)
		printf("ppiwrite(%x, %p)\n", dev, uio);
#endif
	return (ppirw(dev, uio));
}

int
ppirw(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	int unit = UNIT(dev);
	struct ppi_softc *sc = ppi_cd.cd_devs[unit];
	int s, len, cnt;
	char *cp;
	int error = 0, gotdata = 0;
	int buflen, ctlr, slave;
	char *buf;

	if (uio->uio_resid == 0)
		return(0);

	ctlr = sc->sc_dev.dv_parent->dv_unit;
	slave = sc->sc_slave;

#ifdef DEBUG
	if (ppidebug & (PDB_FOLLOW|PDB_IO))
		printf("ppirw(%x, %p, %c): burst %d, timo %d, resid %x\n",
		       dev, uio, uio->uio_rw == UIO_READ ? 'R' : 'W',
		       sc->sc_burst, sc->sc_timo, uio->uio_resid);
#endif
	buflen = min(sc->sc_burst, uio->uio_resid);
	buf = (char *)malloc(buflen, M_DEVBUF, M_WAITOK);
	sc->sc_flags |= PPIF_UIO;
	if (sc->sc_timo > 0) {
		sc->sc_flags |= PPIF_TIMO;
		timeout_add(&sc->sc_to, sc->sc_timo);
	}
	len = cnt = 0;
	while (uio->uio_resid > 0) {
		len = min(buflen, uio->uio_resid);
		cp = buf;
		if (uio->uio_rw == UIO_WRITE) {
			error = uiomove(cp, len, uio);
			if (error)
				break;
		}
again:
		s = splbio();
		if ((sc->sc_flags & PPIF_UIO) &&
		    hpibreq(sc->sc_dev.dv_parent, &sc->sc_hq) == 0)
			tsleep(sc, PRIBIO+1, "ppirw", 0);
		/*
		 * Check if we timed out during sleep or uiomove
		 */
		if ((sc->sc_flags & PPIF_UIO) == 0) {
#ifdef DEBUG
			if (ppidebug & PDB_IO)
				printf("ppirw: uiomove/sleep timo, flags %x\n",
				       sc->sc_flags);
#endif
			if (sc->sc_flags & PPIF_TIMO) {
				timeout_del(&sc->sc_to);
				sc->sc_flags &= ~PPIF_TIMO;
			}
			splx(s);
			break;
		}
		/*
		 * Perform the operation
		 */
		if (uio->uio_rw == UIO_WRITE)
			cnt = hpibsend(ctlr, slave, sc->sc_sec, cp, len);
		else
			cnt = hpibrecv(ctlr, slave, sc->sc_sec, cp, len);
		hpibfree(sc->sc_dev.dv_parent, &sc->sc_hq);
#ifdef DEBUG
		if (ppidebug & PDB_IO)
			printf("ppirw: %s(%d, %d, %x, %p, %d) -> %d\n",
			       uio->uio_rw == UIO_READ ? "recv" : "send",
			       ctlr, slave, sc->sc_sec, cp, len, cnt);
#endif
		if (uio->uio_rw == UIO_READ) {
			if (cnt) {
				error = uiomove(cp, cnt, uio);
				if (error) {
					splx(s);
					break;
				}
				gotdata++;
			}
			/*
			 * Didn't get anything this time, but did in the past.
			 * Consider us done.
			 */
			else if (gotdata) {
				splx(s);
				break;
			}
		}
		/*
		 * Operation timeout (or non-blocking), quit now.
		 */
		if ((sc->sc_flags & PPIF_UIO) == 0) {
#ifdef DEBUG
			if (ppidebug & PDB_IO)
				printf("ppirw: timeout/done\n");
#endif
			splx(s);
			break;
		}
		/*
		 * Implement inter-read delay
		 */
		if (sc->sc_delay > 0) {
			sc->sc_flags |= PPIF_DELAY;
			timeout_add(&sc->sc_start_to, sc->sc_delay);
			error = tsleep(sc, (PCATCH|PZERO) + 1, "hpib", 0);
			if (error) {
				splx(s);
				break;
			}
		}
		/*
		 * Must not call uiomove again til we've used all data
		 * that we already grabbed.
		 */
		if (uio->uio_rw == UIO_WRITE && cnt != len) {
			cp += cnt;
			len -= cnt;
			cnt = 0;
			splx(s);
			goto again;
		}
		splx(s);
	}
	if (sc->sc_flags & PPIF_TIMO) {
		timeout_del(&sc->sc_to);
		sc->sc_flags &= ~PPIF_TIMO;
	}
	if (sc->sc_flags & PPIF_DELAY) {
		timeout_del(&sc->sc_start_to);
		sc->sc_flags &= ~PPIF_DELAY;
	}
	/*
	 * Adjust for those chars that we uiomove'ed but never wrote
	 */
	if (uio->uio_rw == UIO_WRITE && cnt != len) {
		uio->uio_resid += (len - cnt);
#ifdef DEBUG
		if (ppidebug & PDB_IO)
			printf("ppirw: short write, adjust by %d\n",
			       len-cnt);
#endif
	}
	free(buf, M_DEVBUF);
#ifdef DEBUG
	if (ppidebug & (PDB_FOLLOW|PDB_IO))
		printf("ppirw: return %d, resid %d\n", error, uio->uio_resid);
#endif
	return (error);
}

int
ppiioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct ppi_softc *sc = ppi_cd.cd_devs[UNIT(dev)];
	struct ppiparam *pp, *upp;
	int tmp;
	int error = 0;

	switch (cmd) {
	case PPIIOCGPARAM:
		pp = &sc->sc_param;
		upp = (struct ppiparam *)data;
		upp->burst = pp->burst;
		upp->timo = ppihztoms(pp->timo);
		upp->delay = ppihztoms(pp->delay);
		break;
	case PPIIOCSPARAM:
		pp = &sc->sc_param;
		upp = (struct ppiparam *)data;
		if (upp->burst < PPI_BURST_MIN || upp->burst > PPI_BURST_MAX ||
		    upp->delay < PPI_DELAY_MIN || upp->delay > PPI_DELAY_MAX)
			return(EINVAL);
		pp->burst = upp->burst;
		pp->timo = ppimstohz(upp->timo);
		pp->delay = ppimstohz(upp->delay);
		break;
	case PPIIOCSSEC:
		tmp = *(int *)data;
		if (tmp == -1 || (tmp >= 0 && tmp <= 0x1f))
			sc->sc_sec = tmp;
		else
			error = EINVAL;
		break;
	default:
		return (ENOTTY);
	}
	return (error);
}

int
ppihztoms(h)
	int h;
{
	int m = h;

	if (m > 0)
		m = m * 1000 / hz;
	return(m);
}

int
ppimstohz(m)
	int m;
{
	int h = m;

	if (h > 0) {
		h = h * hz / 1000;
		if (h == 0)
			h = 1000 / hz;
	}
	return(h);
}
