/*	$OpenBSD: lpr.c,v 1.1 1996/06/24 20:10:34 pefo Exp $ */

/*
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Device Driver for AT parallel printer port
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>
#include <dev/isa/lptreg.h>

#include <lpr.h>

#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	LPT_BSIZE	1024

#if !defined(DEBUG) || !defined(notdef)
#define LPRINTF(a)
#else
#define LPRINTF		if (lprdebug) printf a
int lprdebug = 1;
#endif

struct lpr_softc {
	struct device sc_dev;
	void *sc_ih;

	size_t sc_count;
	struct buf *sc_inbuf;
	u_char *sc_cp;
	int sc_spinmax;
	int sc_iobase;
	bus_chipset_tag_t sc_bc;
	bus_io_handle_t sc_ioh;
	int sc_irq;
	u_char sc_state;
#define	LPT_OPEN	0x01	/* device is open */
#define	LPT_OBUSY	0x02	/* printer is busy doing output */
#define	LPT_INIT	0x04	/* waiting to initialize for open */
	u_char sc_flags;
#define	LPT_AUTOLF	0x20	/* automatic LF on CR */
#define	LPT_NOPRIME	0x40	/* don't prime on open */
#define	LPT_NOINTR	0x80	/* do not use interrupt */
	u_char sc_control;
	u_char sc_laststatus;
};

/* XXX does not belong here */
cdev_decl(lpr);

int lprintr __P((void *));

#if NLPR_ISA
int lpr_isa_probe __P((struct device *, void *, void *));
void lpr_isa_attach __P((struct device *, struct device *, void *));
struct cfattach lpr_ca = {
	sizeof(struct lpr_softc), lpr_isa_probe, lpr_isa_attach
};
#endif

#if NLPR_PICA
int lpr_pica_probe __P((struct device *, void *, void *));
void lpr_pica_attach __P((struct device *, struct device *, void *));
struct cfattach lpr_pica_ca = {
	sizeof(struct lpr_softc), lpr_pica_probe, lpr_pica_attach
};
#endif

struct cfdriver lpr_cd = {
	NULL, "lpr", DV_TTY
};

#define	LPTUNIT(s)	(minor(s) & 0x1f)
#define	LPTFLAGS(s)	(minor(s) & 0xe0)

#define	LPS_INVERT	(LPS_SELECT|LPS_NERR|LPS_NBSY|LPS_NACK)
#define	LPS_MASK	(LPS_SELECT|LPS_NERR|LPS_NBSY|LPS_NACK|LPS_NOPAPER)
#define	NOT_READY()	((bus_io_read_1(bc, ioh, lpt_status) ^ LPS_INVERT) & LPS_MASK)
#define	NOT_READY_ERR()	not_ready(bus_io_read_1(bc, ioh, lpt_status), sc)
static int not_ready __P((u_char, struct lpr_softc *));

static void lprwakeup __P((void *arg));
static int pushbytes __P((struct lpr_softc *));

int	lpr_port_test __P((bus_chipset_tag_t, bus_io_handle_t, bus_io_addr_t,
	    bus_io_size_t, u_char, u_char));

/*
 * Internal routine to lprprobe to do port tests of one byte value.
 */
int
lpr_port_test(bc, ioh, base, off, data, mask)
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	bus_io_addr_t base;
	bus_io_size_t off;
	u_char data, mask;
{
	int timeout;
	u_char temp;

	data &= mask;
	bus_io_write_1(bc, ioh, off, data);
	timeout = 1000;
	do {
		delay(10);
		temp = bus_io_read_1(bc, ioh, off) & mask;
	} while (temp != data && --timeout);
	LPRINTF(("lpr: port=0x%x out=0x%x in=0x%x timeout=%d\n", base + off,
	    data, temp, timeout));
	return (temp == data);
}

/*
 * Logic:
 *	1) You should be able to write to and read back the same value
 *	   to the data port.  Do an alternating zeros, alternating ones,
 *	   walking zero, and walking one test to check for stuck bits.
 *
 *	2) You should be able to write to and read back the same value
 *	   to the control port lower 5 bits, the upper 3 bits are reserved
 *	   per the IBM PC technical reference manauls and different boards
 *	   do different things with them.  Do an alternating zeros, alternating
 *	   ones, walking zero, and walking one test to check for stuck bits.
 *
 *	   Some printers drag the strobe line down when the are powered off
 * 	   so this bit has been masked out of the control port test.
 *
 *	   XXX Some printers may not like a fast pulse on init or strobe, I
 *	   don't know at this point, if that becomes a problem these bits
 *	   should be turned off in the mask byte for the control port test.
 *
 *	3) Set the data and control ports to a value of 0
 */
#if NLPR_ISA
int
lpr_isa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	u_long base;
	u_char mask, data;
	int i, rv;

#ifdef DEBUG
#define	ABORT	do {printf("lprprobe: mask %x data %x failed\n", mask, data); \
		    goto out;} while (0)
#else
#define	ABORT	goto out
#endif

	bc = ia->ia_bc;
	base = ia->ia_iobase;
	if (bus_io_map(bc, base, LPT_NPORTS, &ioh))
		return 0;

	rv = 0;
	mask = 0xff;

	data = 0x55;				/* Alternating zeros */
	if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
		ABORT;

	data = 0xaa;				/* Alternating ones */
	if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
		ABORT;

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking zero */
		data = ~(1 << i);
		if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking one */
		data = (1 << i);
		if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	bus_io_write_1(bc, ioh, lpt_data, 0);
	bus_io_write_1(bc, ioh, lpt_control, 0);

	ia->ia_iosize = LPT_NPORTS;
	ia->ia_msize = 0;

	rv = 1;

out:
	bus_io_unmap(bc, ioh, LPT_NPORTS);
	return rv;
}
#endif

#if NLPR_PICA
int
lpr_pica_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct confargs *ca = aux;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	u_long base;
	u_char mask, data;
	int i;

#ifdef DEBUG
#define	ABORT	do {printf("lprprobe: mask %x data %x failed\n", mask, data); \
		    return 0;} while (0)
#else
#define	ABORT	return 0
#endif

	if(!BUS_MATCHNAME(ca, "lpr"))
		 return(0);

	bc = 0;
	base = (int)BUS_CVTADDR(ca);
	ioh = base;

	mask = 0xff;

	data = 0x55;				/* Alternating zeros */
	if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
		ABORT;

	data = 0xaa;				/* Alternating ones */
	if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
		ABORT;

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking zero */
		data = ~(1 << i);
		if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking one */
		data = (1 << i);
		if (!lpr_port_test(bc, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	bus_io_write_1(bc, ioh, lpt_data, 0);
	bus_io_write_1(bc, ioh, lpt_control, 0);

	return 1;
}
#endif

#if NLPR_ISA
void
lpr_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lpr_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;

	if (ia->ia_irq != IRQUNK)
		printf("\n");
	else
		printf(": polled\n");

	sc->sc_iobase = ia->ia_iobase;
	sc->sc_irq = ia->ia_irq;
	sc->sc_state = 0;

	bc = sc->sc_bc = ia->ia_bc;
	if (bus_io_map(bc, sc->sc_iobase, LPT_NPORTS, &ioh))
		panic("lprattach: couldn't map I/O ports");
	sc->sc_ioh = ioh;

	bus_io_write_1(bc, ioh, lpt_control, LPC_NINIT);

	if (ia->ia_irq != IRQUNK)
		sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
		    IPL_TTY, lprintr, sc, sc->sc_dev.dv_xname);
}
#endif

#if NLPR_PICA
void
lpr_pica_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lpr_softc *sc = (void *)self;
	struct confargs *ca = aux;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;

	printf("\n");

	sc->sc_iobase = (int)BUS_CVTADDR(ca);
	sc->sc_irq = 0;
	sc->sc_state = 0;

	bc = sc->sc_bc = 0;
	sc->sc_ioh = sc->sc_iobase;

	bus_io_write_1(bc, ioh, lpt_control, LPC_NINIT);

	BUS_INTR_ESTABLISH(ca, lprintr, sc);
}
#endif

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
lpropen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = LPTUNIT(dev);
	u_char flags = LPTFLAGS(dev);
	struct lpr_softc *sc;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	u_char control;
	int error;
	int spin;

	if (unit >= lpr_cd.cd_ndevs)
		return ENXIO;
	sc = lpr_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_irq == IRQUNK && (flags & LPT_NOINTR) == 0)
		return ENXIO;

#ifdef DIAGNOSTIC
	if (sc->sc_state)
		printf("%s: stat=0x%x not zero\n", sc->sc_dev.dv_xname,
		    sc->sc_state);
#endif

	if (sc->sc_state)
		return EBUSY;

	sc->sc_state = LPT_INIT;
	sc->sc_flags = flags;
	LPRINTF(("%s: open: flags=0x%x\n", sc->sc_dev.dv_xname, flags));
	bc = sc->sc_bc;
	ioh = sc->sc_ioh;

	if ((flags & LPT_NOPRIME) == 0) {
		/* assert INIT for 100 usec to start up printer */
		bus_io_write_1(bc, ioh, lpt_control, LPC_SELECT);
		delay(100);
	}

	control = LPC_SELECT | LPC_NINIT;
	bus_io_write_1(bc, ioh, lpt_control, control);

	/* wait till ready (printer running diagnostics) */
	for (spin = 0; NOT_READY_ERR(); spin += STEP) {
		if (spin >= TIMEOUT) {
			sc->sc_state = 0;
			return EBUSY;
		}

		/* wait 1/4 second, give up if we get a signal */
		error = tsleep((caddr_t)sc, LPTPRI | PCATCH, "lpropen", STEP);
		if (error != EWOULDBLOCK) {
			sc->sc_state = 0;
			return error;
		}
	}

	if ((flags & LPT_NOINTR) == 0)
		control |= LPC_IENABLE;
	if (flags & LPT_AUTOLF)
		control |= LPC_AUTOLF;
	sc->sc_control = control;
	bus_io_write_1(bc, ioh, lpt_control, control);

	sc->sc_inbuf = geteblk(LPT_BSIZE);
	sc->sc_count = 0;
	sc->sc_state = LPT_OPEN;

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		lprwakeup(sc);

	LPRINTF(("%s: opened\n", sc->sc_dev.dv_xname));
	return 0;
}

int
not_ready(status, sc)
	u_char status;
	struct lpr_softc *sc;
{
	u_char new;

	status = (status ^ LPS_INVERT) & LPS_MASK;
	new = status & ~sc->sc_laststatus;
	sc->sc_laststatus = status;

	if (new & LPS_SELECT)
		log(LOG_NOTICE, "%s: offline\n", sc->sc_dev.dv_xname);
	else if (new & LPS_NOPAPER)
		log(LOG_NOTICE, "%s: out of paper\n", sc->sc_dev.dv_xname);
	else if (new & LPS_NERR)
		log(LOG_NOTICE, "%s: output error\n", sc->sc_dev.dv_xname);

	return status;
}

void
lprwakeup(arg)
	void *arg;
{
	struct lpr_softc *sc = arg;
	int s;

	s = spltty();
	lprintr(sc);
	splx(s);

	timeout(lprwakeup, sc, STEP);
}

/*
 * Close the device, and free the local line buffer.
 */
int
lprclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = LPTUNIT(dev);
	struct lpr_softc *sc = lpr_cd.cd_devs[unit];
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;

	if (sc->sc_count)
		(void) pushbytes(sc);

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		untimeout(lprwakeup, sc);

	bus_io_write_1(bc, ioh, lpt_control, LPC_NINIT);
	sc->sc_state = 0;
	bus_io_write_1(bc, ioh, lpt_control, LPC_NINIT);
	brelse(sc->sc_inbuf);

	LPRINTF(("%s: closed\n", sc->sc_dev.dv_xname));
	return 0;
}

int
pushbytes(sc)
	struct lpr_softc *sc;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int error;

	if (sc->sc_flags & LPT_NOINTR) {
		int spin, tic;
		u_char control = sc->sc_control;

		while (sc->sc_count > 0) {
			spin = 0;
			while (NOT_READY()) {
				if (++spin < sc->sc_spinmax)
					continue;
				tic = 0;
				/* adapt busy-wait algorithm */
				sc->sc_spinmax++;
				while (NOT_READY_ERR()) {
					/* exponential backoff */
					tic = tic + tic + 1;
					if (tic > TIMEOUT)
						tic = TIMEOUT;
					error = tsleep((caddr_t)sc,
					    LPTPRI | PCATCH, "lprpsh", tic);
					if (error != EWOULDBLOCK)
						return error;
				}
				break;
			}

			bus_io_write_1(bc, ioh, lpt_data, *sc->sc_cp++);
			bus_io_write_1(bc, ioh, lpt_control, control | LPC_STROBE);
			sc->sc_count--;
			bus_io_write_1(bc, ioh, lpt_control, control);

			/* adapt busy-wait algorithm */
			if (spin*2 + 16 < sc->sc_spinmax)
				sc->sc_spinmax--;
		}
	} else {
		int s;

		while (sc->sc_count > 0) {
			/* if the printer is ready for a char, give it one */
			if ((sc->sc_state & LPT_OBUSY) == 0) {
				LPRINTF(("%s: write %d\n", sc->sc_dev.dv_xname,
				    sc->sc_count));
				s = spltty();
				(void) lprintr(sc);
				splx(s);
			}
			error = tsleep((caddr_t)sc, LPTPRI | PCATCH,
			    "lprwrite2", 0);
			if (error)
				return error;
		}
	}
	return 0;
}

/* 
 * Copy a line from user space to a local buffer, then call putc to get the
 * chars moved to the output queue.
 */
int
lprwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct lpr_softc *sc = lpr_cd.cd_devs[LPTUNIT(dev)];
	size_t n;
	int error = 0;

	while ((n = min(LPT_BSIZE, uio->uio_resid)) != 0) {
		uiomove(sc->sc_cp = sc->sc_inbuf->b_data, n, uio);
		sc->sc_count = n;
		error = pushbytes(sc);
		if (error) {
			/*
			 * Return accurate residual if interrupted or timed
			 * out.
			 */
			uio->uio_resid += sc->sc_count;
			sc->sc_count = 0;
			return error;
		}
	}
	return 0;
}

/*
 * Handle printer interrupts which occur when the printer is ready to accept
 * another char.
 */
int
lprintr(arg)
	void *arg;
{
	struct lpr_softc *sc = arg;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;

	if (((sc->sc_state & LPT_OPEN) == 0 && sc->sc_count == 0) || (sc->sc_flags & LPT_NOINTR))
		return 0;

	/* is printer online and ready for output */
	if (NOT_READY() && NOT_READY_ERR())
		return -1;

	if (sc->sc_count) {
		u_char control = sc->sc_control;
		/* send char */
		bus_io_write_1(bc, ioh, lpt_data, *sc->sc_cp++);
		bus_io_write_1(bc, ioh, lpt_control, control | LPC_STROBE);
		sc->sc_count--;
		bus_io_write_1(bc, ioh, lpt_control, control);
		sc->sc_state |= LPT_OBUSY;
	} else
		sc->sc_state &= ~LPT_OBUSY;

	if (sc->sc_count == 0) {
		/* none, wake up the top half to get more */
		wakeup((caddr_t)sc);
	}

	return 1;
}

int
lprioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error = 0;

	switch (cmd) {
	default:
		error = ENODEV;
	}

	return error;
}
