/*	$OpenBSD: sscom.c,v 1.16 2010/02/01 23:53:58 drahn Exp $ */
/*	$NetBSD: sscom.c,v 1.29 2008/06/11 22:37:21 cegger Exp $ */

/*
 * Copyright (c) 2002, 2003 Fujitsu Component Limited
 * Copyright (c) 2002, 2003 Genetec Corporation
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
 * 3. Neither the name of The Fujitsu Component Limited nor the name of
 *    Genetec corporation may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
 * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
 * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

/*
 * Support integrated UARTs of Samsung S3C2800/2400X/2410X
 * Derived from sys/dev/ic/com.c
 */

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: sscom.c,v 1.29 2008/06/11 22:37:21 cegger Exp $");

#include "opt_sscom.h"
#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"

#include "rnd.h"
#if NRND > 0 && defined(RND_COM)
#include <sys/rnd.h>
#endif
*/

/*
 * Override cnmagic(9) macro before including <sys/systm.h>.
 * We need to know if cn_check_magic triggered debugger, so set a flag.
 * Callers of cn_check_magic must declare int cn_trapped = 0;
 * XXX: this is *ugly*!
 */
#define cn_trap()				\
	do {					\
		console_debugger();		\
		cn_trapped = 1;			\
	} while (/* CONSTCOND */ 0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/selinfo.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/s3c2xx0/s3c2xx0reg.h>
#include <arm/s3c2xx0/s3c2xx0var.h>
#if defined(SSCOM_S3C2410) || defined(SSCOM_S3C2400)
#include <arm/s3c2xx0/s3c24x0reg.h>
#elif defined(SSCOM_S3C2800)
#include <arm/s3c2xx0/s3c2800reg.h>
#endif
#include <arm/s3c2xx0/sscom_var.h>
#include <dev/cons.h>
#ifdef DDB
#include <ddb/db_var.h>
#endif

dev_type_open(sscomopen);
dev_type_close(sscomclose);
dev_type_read(sscomread);
dev_type_write(sscomwrite);
dev_type_ioctl(sscomioctl);
dev_type_stop(sscomstop);
dev_type_tty(sscomtty);
dev_type_poll(sscompoll);

int	sscomcngetc	(dev_t);
void	sscomcnputc	(dev_t, int);
void	sscomcnpollc	(dev_t, int);

#define	integrate	static inline
void 	sscomsoft	(void *);

void sscom_rxsoft	(struct sscom_softc *, struct tty *);
void sscom_stsoft	(struct sscom_softc *, struct tty *);
void sscom_schedrx	(struct sscom_softc *);

void	sscom_modem(struct sscom_softc *, int);
void	sscom_break(struct sscom_softc *, int);
void	sscom_iflush(struct sscom_softc *);
void	sscom_hwiflow(struct sscom_softc *);
void	sscom_loadchannelregs(struct sscom_softc *);
static void	tiocm_to_sscom(struct sscom_softc *, u_long, int);
static int	sscom_to_tiocm(struct sscom_softc *);
static void	tiocm_to_sscom(struct sscom_softc *, u_long, int);
static int	sscom_to_tiocm(struct sscom_softc *);

static int	sscomhwiflow(struct tty *tp, int block);
int	sscom_init(bus_space_tag_t, const struct sscom_uart_info *,
		    int, int, tcflag_t, bus_space_handle_t *);

extern struct cfdriver sscom_cd;

/*
 * Make this an option variable one can patch.
 * But be warned:  this must be a power of 2!
 */
u_int sscom_rbuf_size = SSCOM_RING_SIZE;

/* Stop input when 3/4 of the ring is full; restart when only 1/4 is full. */
u_int sscom_rbuf_hiwat = (SSCOM_RING_SIZE * 1) / 4;
u_int sscom_rbuf_lowat = (SSCOM_RING_SIZE * 3) / 4;

static int	sscomconsunit = -1;
static bus_space_tag_t sscomconstag;
static bus_space_handle_t sscomconsioh;
static int	sscomconsattached;
static int	sscomconsrate;
static tcflag_t sscomconscflag;
#if 0
static struct cnm_state sscom_cnm_state;
#endif

#ifdef KGDB
#include <sys/kgdb.h>

static int sscom_kgdb_unit = -1;
static bus_space_tag_t sscom_kgdb_iot;
static bus_space_handle_t sscom_kgdb_ioh;
static int sscom_kgdb_attached;

int	sscom_kgdb_getc (void *);
void	sscom_kgdb_putc (void *, int);
#endif /* KGDB */

#define	SSCOMUNIT_MASK  	0x7f
#define	SSCOMDIALOUT_MASK	0x80

#define	DEVUNIT(x)	(minor(x) & SSCOMUNIT_MASK)
#define	SSCOMDIALOUT(x)	(minor(x) & SSCOMDIALOUT_MASK)

#if 0
#define	SSCOM_ISALIVE(sc)	((sc)->enabled != 0 && \
				 device_is_active(&(sc)->sc_dev))
#else
#define	SSCOM_ISALIVE(sc)	device_is_active(&(sc)->sc_dev)
#endif

#define	BR	BUS_SPACE_BARRIER_READ
#define	BW	BUS_SPACE_BARRIER_WRITE
#define SSCOM_BARRIER(t, h, f) /* no-op */

#if (defined(MULTIPROCESSOR) || defined(LOCKDEBUG)) && defined(SSCOM_MPLOCK)

#define SSCOM_LOCK(sc) simple_lock(&(sc)->sc_lock)
#define SSCOM_UNLOCK(sc) simple_unlock(&(sc)->sc_lock)

#else

#define SSCOM_LOCK(sc)
#define SSCOM_UNLOCK(sc)

#endif

#ifndef SSCOM_TOLERANCE
#define	SSCOM_TOLERANCE	30	/* XXX: baud rate tolerance, in 0.1% units */
#endif

/* value for UCON */
#define UCON_RXINT_MASK	  \
	(UCON_RXMODE_MASK|UCON_ERRINT|UCON_TOINT|UCON_RXINT_TYPE)
#define UCON_RXINT_ENABLE \
	(UCON_RXMODE_INT|UCON_ERRINT|UCON_TOINT|UCON_RXINT_TYPE_LEVEL)
#define UCON_TXINT_MASK   (UCON_TXMODE_MASK|UCON_TXINT_TYPE)
#define UCON_TXINT_ENABLE (UCON_TXMODE_INT|UCON_TXINT_TYPE_LEVEL)

/* we don't want tx interrupt on debug port, but it is needed to
   have transmitter active */
#define UCON_DEBUGPORT	  (UCON_RXINT_ENABLE|UCON_TXINT_ENABLE)


void __sscom_output_chunk(struct sscom_softc *sc, int ufstat);
void sscom_output_chunk(struct sscom_softc *sc);

void
__sscom_output_chunk(struct sscom_softc *sc, int ufstat)
{
	int cnt, max;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint8_t buffer[16+1];
	uint8_t *tptr;
	struct tty *tp = sc->sc_tty;

	max = 16 - ((ufstat & UFSTAT_TXCOUNT) >> UFSTAT_TXCOUNT_SHIFT);
	cnt = min ((int)max,tp->t_outq.c_cc);
	if (cnt != 0) {
		cnt = q_to_b(&tp->t_outq, buffer, cnt);
		for (tptr = buffer; tptr < &buffer[cnt]; tptr ++) {
			bus_space_write_4(iot, ioh, SSCOM_UTXH, *tptr);
		}
	}
	
}

void
sscom_output_chunk(struct sscom_softc *sc)
{
	int ufstat = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSCOM_UFSTAT);

	if (!(ufstat & UFSTAT_TXFULL))
		__sscom_output_chunk(sc, ufstat);
}

int
sscomspeed(long speed, long frequency)
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;

	if (speed == 115200 && frequency == 100000000)
		return 0x1a;

	if (speed <= 0)
		return -1;
	x = divrnd(frequency / 16, speed);
	if (x <= 0)
		return -1;
	err = divrnd(((quad_t)frequency) * 1000 / 16, speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > SSCOM_TOLERANCE)
		return -1;
	return x-1;

#undef	divrnd
}

void sscomstatus (struct sscom_softc *, const char *);

#ifdef SSCOM_DEBUG
int	sscom_debug = 0;

void
sscomstatus(struct sscom_softc *sc, const char *str)
{
	struct tty *tp = sc->sc_tty;
	int umstat = bus_space_read_4(sc->sc_iot, sc->sc_iot, SSCOM_UMSTAT);
	int umcon = bus_space_read_4(sc->sc_iot, sc->sc_iot, SSCOM_UMCON);

	printf("%s: %s %sclocal  %sdcd %sts_carr_on %sdtr %stx_stopped\n",
	    sc->sc_dev.dv_xname, str,
	    ISSET(tp->t_cflag, CLOCAL) ? "+" : "-",
	    "+",			/* DCD */
	    ISSET(tp->t_state, TS_CARR_ON) ? "+" : "-",
	    "+",			/* DTR */
	    sc->sc_tx_stopped ? "+" : "-");

	printf("%s: %s %scrtscts %scts %sts_ttstop  %srts %xrx_flags\n",
	    sc->sc_dev.dv_xname, str,
	    ISSET(tp->t_cflag, CRTSCTS) ? "+" : "-",
	    ISSET(umstat, UMSTAT_CTS) ? "+" : "-",
	    ISSET(tp->t_state, TS_TTSTOP) ? "+" : "-",
	    ISSET(umcon, UMCON_RTS) ? "+" : "-",
	    sc->sc_rx_flags);
}
#else
#define sscom_debug  0
#endif

#if 0
static void
sscom_enable_debugport(struct sscom_softc *sc)
{
	int s;

	/* Turn on line break interrupt, set carrier. */
	s = spltty();
	SSCOM_LOCK(sc);
	sc->sc_ucon = UCON_DEBUGPORT;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSCOM_UCON, sc->sc_ucon);
	sc->sc_umcon = UMCON_RTS|UMCON_DTR;
	sc->set_modem_control(sc);
	sscom_enable_rxint(sc);
	sscom_disable_txint(sc);
	SSCOM_UNLOCK(sc);
	splx(s);
}
#endif

static void
sscom_set_modem_control(struct sscom_softc *sc)
{
	/* flob RTS */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
	    SSCOM_UMCON, sc->sc_umcon & UMCON_HW_MASK);
	/* ignore DTR */
}

static int
sscom_read_modem_status(struct sscom_softc *sc)
{
	int msts;

	msts = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSCOM_UMSTAT);

	/* DCD and DSR are always on */
	return (msts & UMSTAT_CTS) | MSTS_DCD | MSTS_DSR;
}

void
sscom_attach_subr(struct sscom_softc *sc)
{
	int unit = sc->sc_unit;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp;

	/*
	 * set default for modem control hook
	 */
	if (sc->set_modem_control == NULL)
		sc->set_modem_control = sscom_set_modem_control;
	if (sc->read_modem_status == NULL)
		sc->read_modem_status = sscom_read_modem_status;

	timeout_set(&sc->sc_diag_timeout, sscomdiag, sc);
#if (defined(MULTIPROCESSOR) || defined(LOCKDEBUG)) && defined(SSCOM_MPLOCK)
	simple_lock_init(&sc->sc_lock);
#endif

	sc->sc_ucon = UCON_RXINT_ENABLE|UCON_TXINT_ENABLE;


	/* Disable interrupts before configuring the device. */
	sscom_disable_txrxint(sc);

#ifdef KGDB
	/*
	 * Allow kgdb to "take over" this port.  If this is
	 * the kgdb device, it has exclusive use.
	 */
	if (unit == sscom_kgdb_unit) {
		SET(sc->sc_hwflags, SSCOM_HW_KGDB);
		sc->sc_ucon = UCON_DEBUGPORT;
	}
#endif

	if (unit == sscomconsunit) {
		sscomconsattached = 1;

		sscomconstag = iot;
		sscomconsioh = ioh;

		/* Make sure the console is always "hardwired". */
		delay(1000);			/* XXX: wait for output to finish */
		SET(sc->sc_hwflags, SSCOM_HW_CONSOLE);
		SET(sc->sc_swflags, TIOCFLAG_SOFTCAR);

		sc->sc_ucon = UCON_DEBUGPORT;
	}

	bus_space_write_4(iot, ioh, SSCOM_UFCON,
	    UFCON_TXTRIGGER_8|UFCON_RXTRIGGER_8|UFCON_FIFO_ENABLE|
	    UFCON_TXFIFO_RESET|UFCON_RXFIFO_RESET);

	bus_space_write_4(iot, ioh, SSCOM_UCON, sc->sc_ucon);

#ifdef KGDB
	if (ISSET(sc->sc_hwflags, SSCOM_HW_KGDB)) {
		sscom_kgdb_attached = 1;
		printf("%s: kgdb\n", sc->sc_dev.dv_xname);
		sscom_enable_debugport(sc);
		return;
	}
#endif



	tp = ttymalloc();
	tp->t_oproc = sscomstart;
	tp->t_param = sscomparam;
	tp->t_hwiflow = sscomhwiflow;

	sc->sc_tty = tp;
	sc->sc_rbuf = malloc(sscom_rbuf_size << 1, M_DEVBUF, M_NOWAIT);
	sc->sc_rbput = sc->sc_rbget = sc->sc_rbuf;
	sc->sc_rbavail = sscom_rbuf_size;
	if (sc->sc_rbuf == NULL) {
		printf("%s: unable to allocate ring buffer\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	sc->sc_ebuf = sc->sc_rbuf + (sscom_rbuf_size << 1);

#if 0 /* XXX */
	tty_attach(tp);
#endif

	if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE)) {
		int maj;

		/* Locate the major number. */
		for (maj = 0; maj < nchrdev; maj++)
			if (cdevsw[maj].d_open == sscomopen)
				break;


		cn_tab->cn_dev = makedev(maj, sc->sc_dev.dv_unit);

		printf(": console", sc->sc_dev.dv_xname, maj);
	}



#if NRND > 0 && defined(RND_COM)
	rnd_attach_source(&sc->rnd_source, sc->sc_dev.dv_xname,
			  RND_TYPE_TTY, 0);
#endif

	/* if there are no enable/disable functions, assume the device
	   is always enabled */

	sc->sc_si = softintr_establish(IPL_TTY, sscomsoft, sc);
	if (sc->sc_si == NULL)
		panic("%s: can't establish soft interrupt",
		    sc->sc_dev.dv_xname);

	SET(sc->sc_hwflags, SSCOM_HW_DEV_OK);

#if 0
	if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE))
		sscom_enable_debugport(sc);
	else 
		sscom_disable_txrxint(sc);
#endif
}

int
sscom_detach(struct device *self, int flags)
{
	return 0;
}

int
sscom_activate(struct device *self, int act)
{
#ifdef notyet
	struct sscom_softc *sc = (struct sscom_softc *)self;
	int s, rv = 0;

	s = spltty();
	SSCOM_LOCK(sc);
	switch (act) {
	case DVACT_ACTIVATE:
		rv = 0;
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_hwflags & (SSCOM_HW_CONSOLE|SSCOM_HW_KGDB)) {
			rv = EBUSY;
			break;
		}

		sc->enabled = 0;
		break;
	}

	SSCOM_UNLOCK(sc);	
	splx(s);
	return rv;
#else
	return 0;
#endif
}

void
sscom_shutdown(struct sscom_softc *sc)
{
#ifdef notyet
	struct tty *tp = sc->sc_tty;
	int s;

	s = spltty();
	SSCOM_LOCK(sc);	

	/* If we were asserting flow control, then deassert it. */
	SET(sc->sc_rx_flags, RX_IBUF_BLOCKED);
	sscom_hwiflow(sc);

	/* Clear any break condition set with TIOCSBRK. */
	sscom_break(sc, 0);

	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 * Avoid tsleeping above splhigh().
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		sscom_modem(sc, 0);
		SSCOM_UNLOCK(sc);
		splx(s);
		/* XXX tsleep will only timeout */
		(void) tsleep(sc, TTIPRI, ttclos, hz);
		s = spltty();
		SSCOM_LOCK(sc);	
	}

	if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE))
		/* interrupt on break */
		sc->sc_ucon = UCON_DEBUGPORT;
	else
		sc->sc_ucon = 0;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSCOM_UCON, sc->sc_ucon);

#ifdef DIAGNOSTIC
	if (!sc->enabled)
		panic("sscom_shutdown: not enabled?");
#endif
	sc->enabled = 0;
	SSCOM_UNLOCK(sc);
	splx(s);
#endif
}

int
sscomopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct sscom_softc *sc;
	struct tty *tp;
	int s;
	int error;
	int unit = DEVUNIT(dev);

	if (unit >= sscom_cd.cd_ndevs)
		return ENXIO;
	sc = sscom_cd.cd_devs[unit];

	if (sc == NULL || !ISSET(sc->sc_hwflags, SSCOM_HW_DEV_OK) ||
		sc->sc_rbuf == NULL)
		return ENXIO;

#if 0
	if (!device_is_active(&sc->sc_dev))
		return ENXIO;
#endif

#ifdef KGDB
	/*
	 * If this is the kgdb port, no other use is permitted.
	 */
	if (ISSET(sc->sc_hwflags, SSCOM_HW_KGDB))
		return EBUSY;
#endif

	tp = sc->sc_tty;
	tp->t_dev = dev; /* XXX - could be done before? */

	s = spltty();

	/*
	 * Do the following iff this is a first open.
	 */
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		tp->t_dev = dev;

		SSCOM_LOCK(sc);

		/* Fetch the current modem control status, needed later. */
		sc->sc_msts = sc->read_modem_status(sc);

#if 0
		/* Clear PPS capture state on first open. */
		sc->sc_ppsmask = 0;
		sc->ppsparam.mode = 0;
#endif

		SSCOM_UNLOCK(sc);

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE)) {
			tp->t_ispeed = tp->t_ospeed = sscomconsrate;
			tp->t_cflag = sscomconscflag;
		} else {
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
			tp->t_cflag = TTYDEF_CFLAG;
		}
		if (ISSET(sc->sc_swflags, TIOCFLAG_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, TIOCFLAG_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, TIOCFLAG_MDMBUF))
			SET(tp->t_cflag, MDMBUF);

		(void) sscomparam(tp, &tp->t_termios);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		SSCOM_LOCK(sc);

		sscom_loadchannelregs(sc);

		/*
		 * Turn on DTR.  We must always do this, even if carrier is not
		 * present, because otherwise we'd have to use TIOCSDTR
		 * immediately after setting CLOCAL, which applications do not
		 * expect.  We always assert DTR while the device is open
		 * unless explicitly requested to deassert it.
		 */
		sscom_modem(sc, 1);

		/* Clear the input ring, and unblock. */
		sc->sc_rbput = sc->sc_rbget = sc->sc_rbuf;
		sc->sc_rbavail = sscom_rbuf_size;
		sscom_iflush(sc);
		CLR(sc->sc_rx_flags, RX_ANY_BLOCK);
		sscom_hwiflow(sc);

		if (sscom_debug)
			sscomstatus(sc, "sscomopen  ");

		/* Turn on interrupts. */
		sscom_enable_rxint(sc);


		SSCOM_UNLOCK(sc);
	}
	
	if (SSCOMDIALOUT(dev)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return EBUSY;
		}
		sc->sc_cua = 1;         /* We go into CUA mode. */
	} else {
		/* tty (not cua) device; wait for carrier if necessary. */
		if (ISSET(flag, O_NONBLOCK)) {
			if (sc->sc_cua) {
				/* Opening TTY non-blocking... but the CUA is busy. */
				splx(s);
				return EBUSY;
			}
		} else {
			while (sc->sc_cua ||
			    (!ISSET(tp->t_cflag, CLOCAL) &&
				!ISSET(tp->t_state, TS_CARR_ON))) {
				SET(tp->t_state, TS_WOPEN);
				error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH, ttopen, 0);
				/*
				 * If TS_WOPEN has been reset, that means the cua device
				 * has been closed.  We don't want to fail in that case,
				 * so just go around again.
				 */
				if (error && ISSET(tp->t_state, TS_WOPEN)) {
					CLR(tp->t_state, TS_WOPEN);
					splx(s);
					return error;
				}
			}
		}
	}
	splx(s);

        error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error)
		goto bad;

	return 0;

bad:
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		sscom_shutdown(sc);
	}

	return error;
}
 
int
sscomclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct sscom_softc *sc;
	struct tty *tp;
	int unit = DEVUNIT(dev);
	if (unit >= sscom_cd.cd_ndevs)
		return ENXIO;
	sc = sscom_cd.cd_devs[unit];

	tp = sc->sc_tty;

	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

        (*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return 0;
#endif

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		sscom_shutdown(sc);
	}

	return 0;
}
 
int
sscomread(dev_t dev, struct uio *uio, int flag)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
        struct tty *tp = sc->sc_tty;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return EIO;
#endif
 
        return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
sscomwrite(dev_t dev, struct uio *uio, int flag)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
        struct tty *tp = sc->sc_tty;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return EIO;
#endif
 
        return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

#if 0
int
sscompoll(dev_t dev, int events, struct proc *p)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
        struct tty *tp = sc->sc_tty;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return EIO;
#endif
 
        return ((*linesw[tp->t_line].l_poll)(tp, uio, flag));
}
#endif

struct tty *
sscomtty(dev_t dev)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
        struct tty *tp = sc->sc_tty;
 
        return (tp);
}

int
sscomioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
        struct tty *tp = sc->sc_tty;
	int error;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return EIO;
#endif

        error = ((*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p));
	if (error >= 0)
		return error;

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = 0;

#if 0
	SSCOM_LOCK(sc);	
#endif

	switch (cmd) {
	case TIOCSBRK:
		sscom_break(sc, 1);
		break;

	case TIOCCBRK:
		sscom_break(sc, 0);
		break;

	case TIOCSDTR:
		sscom_modem(sc, 1);
		break;

	case TIOCCDTR:
		sscom_modem(sc, 0);
		break;

	case TIOCGFLAGS:
		*(int *)data = sc->sc_swflags;
		break;

	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error)
			break;
		sc->sc_swflags = *(int *)data;
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
		tiocm_to_sscom(sc, cmd, *(int *)data);
		break;

	case TIOCMGET:
		*(int *)data = sscom_to_tiocm(sc);
		break;

	default:
		error = ENOTTY;
		break;
	}

#if 0
	SSCOM_UNLOCK(sc);
#endif

	if (sscom_debug)
		sscomstatus(sc, "sscomioctl ");

	return error;
}

void
sscom_schedrx(struct sscom_softc *sc)
{

	sc->sc_rx_ready = 1;

	/* Wake up the poller. */
	softintr_schedule(sc->sc_si);
}

void
sscom_break(struct sscom_softc *sc, int onoff)
{

	if (onoff)
		SET(sc->sc_ucon, UCON_SBREAK);
	else
		CLR(sc->sc_ucon, UCON_SBREAK);

	if (!sc->sc_heldchange) {
		if (sc->sc_tx_busy) {
			sc->sc_heldtbc = sc->sc_tbc;
			sc->sc_tbc = 0;
			sc->sc_heldchange = 1;
		} else
			sscom_loadchannelregs(sc);
	}
}

void
sscom_modem(struct sscom_softc *sc, int onoff)
{
	if (onoff)
		SET(sc->sc_umcon, UMCON_DTR);
	else
		CLR(sc->sc_umcon, UMCON_DTR);

	if (!sc->sc_heldchange) {
		if (sc->sc_tx_busy) {
			sc->sc_heldtbc = sc->sc_tbc;
			sc->sc_tbc = 0;
			sc->sc_heldchange = 1;
		} else
			sscom_loadchannelregs(sc);
	}
}

static void
tiocm_to_sscom(struct sscom_softc *sc, u_long how, int ttybits)
{
	u_char sscombits;

	sscombits = 0;
	if (ISSET(ttybits, TIOCM_DTR))
		sscombits = UMCON_DTR;
	if (ISSET(ttybits, TIOCM_RTS))
		SET(sscombits, UMCON_RTS);
 
	switch (how) {
	case TIOCMBIC:
		CLR(sc->sc_umcon, sscombits);
		break;

	case TIOCMBIS:
		SET(sc->sc_umcon, sscombits);
		break;

	case TIOCMSET:
		CLR(sc->sc_umcon, UMCON_DTR);
		SET(sc->sc_umcon, sscombits);
		break;
	}

	if (!sc->sc_heldchange) {
		if (sc->sc_tx_busy) {
			sc->sc_heldtbc = sc->sc_tbc;
			sc->sc_tbc = 0;
			sc->sc_heldchange = 1;
		} else
			sscom_loadchannelregs(sc);
	}
}

static int
sscom_to_tiocm(struct sscom_softc *sc)
{
	u_char sscombits;
	int ttybits = 0;

	sscombits = sc->sc_umcon;
#if 0
	if (ISSET(sscombits, MCR_DTR))
		SET(ttybits, TIOCM_DTR);
#endif
	if (ISSET(sscombits, UMCON_RTS))
		SET(ttybits, TIOCM_RTS);

	sscombits = sc->sc_msts;
	if (ISSET(sscombits, MSTS_DCD))
		SET(ttybits, TIOCM_CD);
	if (ISSET(sscombits, MSTS_DSR))
		SET(ttybits, TIOCM_DSR);
	if (ISSET(sscombits, MSTS_CTS))
		SET(ttybits, TIOCM_CTS);

	if (sc->sc_ucon != 0)
		SET(ttybits, TIOCM_LE);

	return ttybits;
}

void pr(char *, ...);
void
pr(char *arg, ...)
{
}

static int
cflag2lcr(tcflag_t cflag)
{
	u_char lcr = ULCON_PARITY_NONE;

	switch (cflag & (PARENB|PARODD)) {
	case PARENB|PARODD:
		lcr = ULCON_PARITY_ODD;
		break;
	case PARENB:
		lcr = ULCON_PARITY_EVEN;
	}

	switch (ISSET(cflag, CSIZE)) {
	case CS5:
		SET(lcr, ULCON_LENGTH_5);
		break;
	case CS6:
		SET(lcr, ULCON_LENGTH_6);
		break;
	case CS7:
		SET(lcr, ULCON_LENGTH_7);
		break;
	case CS8:
		SET(lcr, ULCON_LENGTH_8);
		break;
	}
	if (ISSET(cflag, CSTOPB))
		SET(lcr, ULCON_STOP);
	pr (" lcr %x %d %d %d \n", lcr, cflag & (PARENB|PARODD), ISSET(cflag, CSIZE), CS8);

	return lcr;
}

int
sscomparam(struct tty *tp, struct termios *t)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
	int ospeed;
	u_char lcr;
	int s;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return EIO;
#endif

	ospeed = sscomspeed(t->c_ospeed, sc->sc_frequency);

	/* Check requested parameters. */
	if (ospeed < 0)
		return EINVAL;
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return EINVAL;

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if (ISSET(sc->sc_swflags, TIOCFLAG_SOFTCAR) ||
	    ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE)) {
		SET(t->c_cflag, CLOCAL);
		CLR(t->c_cflag, HUPCL);
	}

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag && sc->sc_ubrdiv == ospeed)
		return 0;

	lcr = cflag2lcr(t->c_cflag);

	s = spltty();
	SSCOM_LOCK(sc);	

	sc->sc_ulcon = lcr;

	/*
	 * If we're not in a mode that assumes a connection is present, then
	 * ignore carrier changes.
	 */
	if (ISSET(t->c_cflag, CLOCAL | MDMBUF))
		sc->sc_msr_dcd = 0;
	else
		sc->sc_msr_dcd = MSTS_DCD;

	/*
	 * Set the flow control pins depending on the current flow control
	 * mode.
	 */
	if (ISSET(t->c_cflag, CRTSCTS)) {
		sc->sc_mcr_dtr = UMCON_DTR;
		sc->sc_mcr_rts = UMCON_RTS;
		sc->sc_msr_cts = MSTS_CTS;
	}
	else if (ISSET(t->c_cflag, MDMBUF)) {
		/*
		 * For DTR/DCD flow control, make sure we don't toggle DTR for
		 * carrier detection.
		 */
		sc->sc_mcr_dtr = 0;
		sc->sc_mcr_rts = UMCON_DTR;
		sc->sc_msr_cts = MSTS_DCD;
	} 
	else {
		/*
		 * If no flow control, then always set RTS.  This will make
		 * the other side happy if it mistakenly thinks we're doing
		 * RTS/CTS flow control.
		 */
		sc->sc_mcr_dtr = UMCON_DTR | UMCON_RTS;
		sc->sc_mcr_rts = 0;
		sc->sc_msr_cts = 0;
		if (ISSET(sc->sc_umcon, UMCON_DTR))
			SET(sc->sc_umcon, UMCON_RTS);
		else
			CLR(sc->sc_umcon, UMCON_RTS);
	}
	sc->sc_msr_mask = sc->sc_msr_cts | sc->sc_msr_dcd;

	if (ospeed == 0)
		CLR(sc->sc_umcon, sc->sc_mcr_dtr);
	else
		SET(sc->sc_umcon, sc->sc_mcr_dtr);

	sc->sc_ubrdiv = ospeed;

	/* And copy to tty. */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	if (!sc->sc_heldchange) {
		if (sc->sc_tx_busy) {
			sc->sc_heldtbc = sc->sc_tbc;
			sc->sc_tbc = 0;
			sc->sc_heldchange = 1;
		} else
			sscom_loadchannelregs(sc);
	}

	if (!ISSET(t->c_cflag, CHWFLOW)) {
		/* Disable the high water mark. */
		sc->sc_r_hiwat = 0;
		sc->sc_r_lowat = 0;
		if (ISSET(sc->sc_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(sc->sc_rx_flags, RX_TTY_OVERFLOWED);
			sscom_schedrx(sc);
		}
		if (ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED)) {
			CLR(sc->sc_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED);
			sscom_hwiflow(sc);
		}
	} else {
		sc->sc_r_hiwat = sscom_rbuf_hiwat;
		sc->sc_r_lowat = sscom_rbuf_lowat;
	}

	SSCOM_UNLOCK(sc);
	splx(s);

	/*
	 * Update the tty layer's idea of the carrier bit, in case we changed
	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
	 * explicit request.
	 */
	(*linesw[tp->t_line].l_modem)(tp, ISSET(sc->sc_msts, MSTS_DCD));


	if (sscom_debug)
		sscomstatus(sc, "sscomparam ");

	if (!ISSET(t->c_cflag, CHWFLOW)) {
		if (sc->sc_tx_stopped) {
			sc->sc_tx_stopped = 0;
			sscomstart(tp);
		}
	}

	return 0;
}

void
sscom_iflush(struct sscom_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int timo;


	timo = 50000;
	/* flush any pending I/O */
	while ( sscom_rxrdy(iot, ioh) && --timo)
		(void)sscom_getc(iot,ioh);
#ifdef DIAGNOSTIC
	if (!timo)
		printf("%s: sscom_iflush timeout\n", sc->sc_dev.dv_xname);
#endif
}

void
sscom_loadchannelregs(struct sscom_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* XXXXX necessary? */
	sscom_iflush(sc);

	bus_space_write_4(iot, ioh, SSCOM_UCON, 0);

	bus_space_write_4(iot, ioh, SSCOM_UBRDIV, sc->sc_ubrdiv);
	bus_space_write_4(iot, ioh, SSCOM_ULCON, sc->sc_ulcon);
	sc->set_modem_control(sc);
	bus_space_write_4(iot, ioh, SSCOM_UCON, sc->sc_ucon);
}

static int
sscomhwiflow(struct tty *tp, int block)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
	int s;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return 0;
#endif

	if (sc->sc_mcr_rts == 0)
		return 0;

	s = spltty();
	SSCOM_LOCK(sc);
	
	if (block) {
		if (!ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED)) {
			SET(sc->sc_rx_flags, RX_TTY_BLOCKED);
			sscom_hwiflow(sc);
		}
	} else {
		if (ISSET(sc->sc_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(sc->sc_rx_flags, RX_TTY_OVERFLOWED);
			sscom_schedrx(sc);
		}
		if (ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED)) {
			CLR(sc->sc_rx_flags, RX_TTY_BLOCKED);
			sscom_hwiflow(sc);
		}
	}

	SSCOM_UNLOCK(sc);
	splx(s);
	return 1;
}
	
/*
 * (un)block input via hw flowcontrol
 */
void
sscom_hwiflow(struct sscom_softc *sc)
{
	if (sc->sc_mcr_rts == 0)
		return;

	if (ISSET(sc->sc_rx_flags, RX_ANY_BLOCK)) {
		CLR(sc->sc_umcon, sc->sc_mcr_rts);
		CLR(sc->sc_mcr_active, sc->sc_mcr_rts);
	} else {
		SET(sc->sc_umcon, sc->sc_mcr_rts);
		SET(sc->sc_mcr_active, sc->sc_mcr_rts);
	}
	sc->set_modem_control(sc);
}


void
sscomstart(struct tty *tp)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
	int s;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return;
#endif

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP))
		goto out;
	if (sc->sc_tx_stopped)
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto out;
	}

	SET(tp->t_state, TS_BUSY);
	sc->sc_tx_busy = 1;

	/* Output the first fifo worth of the buffer. */
	sscom_output_chunk(sc);

out:
	/* Enable transmit completion interrupts if necessary. */
	if (tp->t_outq.c_cc != 0)
		sscom_enable_txint(sc);
	else  {
		if (ISSET(tp->t_state, TS_BUSY)) {
			CLR(tp->t_state, TS_BUSY | TS_FLUSH);
		}
		sscom_disable_txint(sc); /* track state in software? */
	}

	splx(s);
	return;
}

/*
 * Stop output on a line.
 */
int
sscomstop(struct tty *tp, int flag)
{
        struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
	int s;

	s = spltty();
	SSCOM_LOCK(sc);
	if (ISSET(tp->t_state, TS_BUSY)) {
		/* Stop transmitting at the next chunk. */
		sc->sc_tbc = 0;
		sc->sc_heldtbc = 0;
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	}
	SSCOM_UNLOCK(sc);	
	splx(s);
	return 0;
}

void
sscomdiag(void *arg)
{
	struct sscom_softc *sc = arg;
	int overflows, floods;
	int s;

	s = spltty();
	SSCOM_LOCK(sc);
	overflows = sc->sc_overflows;
	sc->sc_overflows = 0;
	floods = sc->sc_floods;
	sc->sc_floods = 0;
	sc->sc_errors = 0;
	SSCOM_UNLOCK(sc);
	splx(s);

	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf flood%s\n",
	    sc->sc_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s");
}

void
sscom_rxsoft(struct sscom_softc *sc, struct tty *tp)
{
	u_char *get, *end;
	u_int cc, scc;
	u_char rsr;
	int code;
	int s;

	end = sc->sc_ebuf;
	get = sc->sc_rbget;
	scc = cc = sscom_rbuf_size - sc->sc_rbavail;

	if (cc == sscom_rbuf_size) {
		sc->sc_floods++;
		if (sc->sc_errors++ == 0)
			timeout_add(&sc->sc_diag_timeout, 60 * hz);
	}

	while (cc) {
		code = get[0];
		rsr = get[1];
		if (rsr) {
			if (ISSET(rsr, UERSTAT_OVERRUN)) {
				sc->sc_overflows++;
				if (sc->sc_errors++ == 0)
					timeout_add(&sc->sc_diag_timeout,
					    60 * hz);
			}
			if (ISSET(rsr, UERSTAT_BREAK | UERSTAT_FRAME))
				SET(code, TTY_FE);
			if (ISSET(rsr, UERSTAT_PARITY))
				SET(code, TTY_PE);
		}
//                (*linesw[tp->t_line].l_rint)(rsr << 8 | code, tp);
		/* what to do with rsr?, -> TTY_PE, TTYFE, or both */
                (*linesw[tp->t_line].l_rint)(code, tp);

#if 0
		if ((*rint)(code, tp) == -1) {
			/*
			 * The line discipline's buffer is out of space.
			 */
			if (!ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED)) {
				/*
				 * We're either not using flow control, or the
				 * line discipline didn't tell us to block for
				 * some reason.  Either way, we have no way to
				 * know when there's more space available, so
				 * just drop the rest of the data.
				 */
				get += cc << 1;
				if (get >= end)
					get -= sscom_rbuf_size << 1;
				cc = 0;
			} else {
				/*
				 * Don't schedule any more receive processing
				 * until the line discipline tells us there's
				 * space available (through sscomhwiflow()).
				 * Leave the rest of the data in the input
				 * buffer.
				 */
				SET(sc->sc_rx_flags, RX_TTY_OVERFLOWED);
			}
			break;
		}
#endif
		get += 2;
		if (get >= end)
			get = sc->sc_rbuf;
		cc--;
	}

	if (cc != scc) {
		sc->sc_rbget = get;
		s = spltty();
		SSCOM_LOCK(sc);
		
		cc = sc->sc_rbavail += scc - cc;
		/* Buffers should be ok again, release possible block. */
		if (cc >= sc->sc_r_lowat) {
			if (ISSET(sc->sc_rx_flags, RX_IBUF_OVERFLOWED)) {
				CLR(sc->sc_rx_flags, RX_IBUF_OVERFLOWED);
				sscom_enable_rxint(sc);
				sc->sc_ucon |= UCON_ERRINT;
				bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSCOM_UCON, 
						  sc->sc_ucon);

			}
			if (ISSET(sc->sc_rx_flags, RX_IBUF_BLOCKED)) {
				CLR(sc->sc_rx_flags, RX_IBUF_BLOCKED);
				sscom_hwiflow(sc);
			}
		}
		SSCOM_UNLOCK(sc);
		splx(s);
	}
}

void
sscom_stsoft(struct sscom_softc *sc, struct tty *tp)
{
	u_char msr, delta;
	int s;

	s = spltty();
	SSCOM_LOCK(sc);
	msr = sc->sc_msts;
	delta = sc->sc_msr_delta;
	sc->sc_msr_delta = 0;
	SSCOM_UNLOCK(sc);	
	splx(s);

	if (ISSET(delta, sc->sc_msr_dcd)) {
		/*
		 * Inform the tty layer that carrier detect changed.
		 */
		(*linesw[tp->t_line].l_modem)(tp, ISSET(msr, MSTS_DCD));

	}

	if (ISSET(delta, sc->sc_msr_cts)) {
		/* Block or unblock output according to flow control. */
		if (ISSET(msr, sc->sc_msr_cts)) {
			struct tty *tp = sc->sc_tty;
			sc->sc_tx_stopped = 0;
			(*linesw[tp->t_line].l_start)(tp);
		} else {
			printf("txstopped\n");
			sc->sc_tx_stopped = 1;
		}
	}

	if (sscom_debug)
		sscomstatus(sc, "sscom_stsoft");
}

void
sscomsoft(void *arg)
{
	struct sscom_softc *sc = arg;
	struct tty *tp;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return;
#endif

	{
		tp = sc->sc_tty;
		
		if (sc->sc_rx_ready) {
			sc->sc_rx_ready = 0;
			sscom_rxsoft(sc, tp);
		}

		if (sc->sc_st_check) {
			sc->sc_st_check = 0;
			sscom_stsoft(sc, tp);
		}
	}
}


int
sscomrxintr(void *arg)
{
	struct sscom_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char *put, *end;
	u_int cc;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return 0;
#endif

	SSCOM_LOCK(sc);

	end = sc->sc_ebuf;
	put = sc->sc_rbput;
	cc = sc->sc_rbavail;

	do {
		u_char	msts, delta;
		uint16_t ufstat;
		uint8_t utrstat, uerstat;

		ufstat = bus_space_read_4(iot, ioh, SSCOM_UFSTAT);

		/* XXX: break interrupt with no character? */

		utrstat = bus_space_read_4(iot, ioh, SSCOM_UTRSTAT);
		if (utrstat & UTRSTAT_RXREADY) {
			uerstat = bus_space_read_4((iot), (ioh), SSCOM_UERSTAT);
			if (uerstat & UERSTAT_BREAK) {
#if defined(DDB)
				if (db_console)
					Debugger();
#endif
				goto next;

			}
			/* xxx overflow */
			put[0] = bus_space_read_4((iot), (ioh),
			    SSCOM_URXH) & 0xff;
			put[1] = uerstat;
			put += 2;
			if (put >= end)
				put = sc->sc_rbuf;
			cc--;
next:
			ufstat = bus_space_read_4(iot, ioh, SSCOM_UFSTAT);
		}
			/*
			 * Current string of incoming characters ended because
			 * no more data was available or we ran out of space.
			 * Schedule a receive event if any data was received.
			 * If we're out of space, turn off receive interrupts.
			 */
			sc->sc_rbput = put;
			sc->sc_rbavail = cc;
			if (!ISSET(sc->sc_rx_flags, RX_TTY_OVERFLOWED))
				sc->sc_rx_ready = 1;

			/*
			 * See if we are in danger of overflowing a buffer. If
			 * so, use hardware flow control to ease the pressure.
			 */
			if (!ISSET(sc->sc_rx_flags, RX_IBUF_BLOCKED) &&
			    cc < sc->sc_r_hiwat) {
				SET(sc->sc_rx_flags, RX_IBUF_BLOCKED);
				sscom_hwiflow(sc);
			}

			/*
			 * If we're out of space, disable receive interrupts
			 * until the queue has drained a bit.
			 */
			if (!cc) {
				SET(sc->sc_rx_flags, RX_IBUF_OVERFLOWED);
				sscom_disable_rxint(sc);
				sc->sc_ucon &= ~UCON_ERRINT;
				bus_space_write_4(iot, ioh, SSCOM_UCON, sc->sc_ucon);
			}
		if (sc->sc_rbput != put) {
			sc->sc_rx_ready = 1;
			softintr_schedule(sc->sc_si);
			sc->sc_st_check = 1;
		}
		sc->sc_rbput = put;
		sc->sc_rbavail = cc;


		msts = sc->read_modem_status(sc);
		delta = msts ^ sc->sc_msts;
		sc->sc_msts = msts;

#ifdef notyet
		/*
		 * Pulse-per-second (PSS) signals on edge of DCD?
		 * Process these even if line discipline is ignoring DCD.
		 */
		if (delta & sc->sc_ppsmask) {
			struct timeval tv;
		    	if ((msr & sc->sc_ppsmask) == sc->sc_ppsassert) {
				/* XXX nanotime() */
				microtime(&tv);
				TIMEVAL_TO_TIMESPEC(&tv, 
				    &sc->ppsinfo.assert_timestamp);
				if (sc->ppsparam.mode & PPS_OFFSETASSERT) {
					timespecadd(&sc->ppsinfo.assert_timestamp,
					    &sc->ppsparam.assert_offset,
						    &sc->ppsinfo.assert_timestamp);
				}

#ifdef PPS_SYNC
				if (sc->ppsparam.mode & PPS_HARDPPSONASSERT)
					hardpps(&tv, tv.tv_usec);
#endif
				sc->ppsinfo.assert_sequence++;
				sc->ppsinfo.current_mode = sc->ppsparam.mode;

			} else if ((msr & sc->sc_ppsmask) == sc->sc_ppsclear) {
				/* XXX nanotime() */
				microtime(&tv);
				TIMEVAL_TO_TIMESPEC(&tv, 
				    &sc->ppsinfo.clear_timestamp);
				if (sc->ppsparam.mode & PPS_OFFSETCLEAR) {
					timespecadd(&sc->ppsinfo.clear_timestamp,
					    &sc->ppsparam.clear_offset,
					    &sc->ppsinfo.clear_timestamp);
				}

#ifdef PPS_SYNC
				if (sc->ppsparam.mode & PPS_HARDPPSONCLEAR)
					hardpps(&tv, tv.tv_usec);
#endif
				sc->ppsinfo.clear_sequence++;
				sc->ppsinfo.current_mode = sc->ppsparam.mode;
			}
		}
#endif

		/*
		 * Process normal status changes
		 */
		if (ISSET(delta, sc->sc_msr_mask)) {
			SET(sc->sc_msr_delta, delta);

			/*
			 * Stop output immediately if we lose the output
			 * flow control signal or carrier detect.
			 */
			if (ISSET(~msts, sc->sc_msr_mask)) {
				sc->sc_tbc = 0;
				sc->sc_heldtbc = 0;
#ifdef SSCOM_DEBUG
				if (sscom_debug)
					sscomstatus(sc, "sscomintr  ");
#endif
			}

			sc->sc_st_check = 1;
		}

		/* 
		 * Done handling any receive interrupts. 
		 */

		/*
		 * If we've delayed a parameter change, do it
		 * now, and restart * output.
		 */
		if ((ufstat & UFSTAT_TXCOUNT) == 0) {
			/* XXX: we should check transmitter empty also */

			if (sc->sc_heldchange) {
				sscom_loadchannelregs(sc);
				sc->sc_heldchange = 0;
				sc->sc_tbc = sc->sc_heldtbc;
				sc->sc_heldtbc = 0;
			}
		}


	} while (0);

	SSCOM_UNLOCK(sc);

	/* Wake up the poller. */
	softintr_schedule(sc->sc_si);


#if NRND > 0 && defined(RND_COM)
	rnd_add_uint32(&sc->rnd_source, iir | rsr);
#endif

	return 1;
}

int
sscomtxintr(void *arg)
{
	struct sscom_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint16_t ufstat;

#if 0
	if (SSCOM_ISALIVE(sc) == 0)
		return 0;
#endif

	SSCOM_LOCK(sc);

	ufstat = bus_space_read_4(iot, ioh, SSCOM_UFSTAT);

	/*
	 * If we've delayed a parameter change, do it
	 * now, and restart * output.
	 */
	if (sc->sc_heldchange && (ufstat & UFSTAT_TXCOUNT) == 0) {
		/* XXX: we should check transmitter empty also */
		sscom_loadchannelregs(sc);
		sc->sc_heldchange = 0;
		sc->sc_tbc = sc->sc_heldtbc;
		sc->sc_heldtbc = 0;
	}

	/*
	 * See if data can be transmitted as well. Schedule tx
	 * done event if no data left and tty was marked busy.
	 */
	if (!ISSET(ufstat,UFSTAT_TXFULL)) {
		/* 
		 * Output the next chunk of the contiguous
		 * buffer, if any.
		 */
		if ( sc->sc_tty->t_outq.c_cc > 0) {
			struct tty *tp = sc->sc_tty;
			if (ISSET(tp->t_state, TS_BUSY)) {
				CLR(tp->t_state, TS_BUSY | TS_FLUSH);
			}
			(*linesw[tp->t_line].l_start)(tp);
		} else {
			struct tty *tp = sc->sc_tty;
			if (ISSET(tp->t_state, TS_BUSY)) {
				CLR(tp->t_state, TS_BUSY | TS_FLUSH);
			}
			sscom_disable_txint(sc);
		}
	}

	SSCOM_UNLOCK(sc);

	/* Wake up the poller. */
	softintr_schedule(sc->sc_si);

#if NRND > 0 && defined(RND_COM)
	rnd_add_uint32(&sc->rnd_source, iir | rsr);
#endif

	return 1;
}

int inited0;
uint32_t init_ulcon;
uint32_t init_ucon;
uint32_t init_ufcon;
uint32_t init_umcon;
uint32_t init_utrstat;
uint32_t init_uerstat;
uint32_t init_ufstat;
uint32_t init_umstat;
uint32_t init_ubrdiv;

void sscom_dump_init_state(void);
void
sscom_dump_init_state()
{
	printf("init_ulcon %x", init_ulcon);
	printf("init_ucon %x", init_ucon);
	printf("init_ufcon %x", init_ufcon);
	printf("init_umcon %x", init_umcon);
	printf("init_utrstat %x", init_utrstat);
	printf("init_uerstat %x", init_uerstat);
	printf("init_ufstat %x", init_ufstat);
	printf("init_umstat %x", init_umstat);
	printf("init_ubrdiv %x", init_ubrdiv);
}

#if defined(KGDB) || defined(SSCOM0CONSOLE) || defined(SSCOM1CONSOLE)
/*
 * Initialize UART for use as console or KGDB line.
 */
int
sscom_init(bus_space_tag_t iot, const struct sscom_uart_info *config,
    int rate, int frequency, tcflag_t cflag, bus_space_handle_t *iohp)
{
	bus_space_handle_t ioh;
	bus_addr_t iobase = config->iobase;
	uint32_t ufcon;

	if (bus_space_map(iot, iobase, SSCOM_SIZE, 0, &ioh))
		return ENOMEM; /* ??? */

	if (inited0 == 0) {
		init_ulcon = bus_space_read_4(iot, ioh, SSCOM_ULCON);
		init_ucon = bus_space_read_4(iot, ioh, SSCOM_UCON);
		init_ufcon = bus_space_read_4(iot, ioh, SSCOM_UFCON);
		init_umcon = bus_space_read_4(iot, ioh, SSCOM_UMCON);
		init_utrstat = bus_space_read_4(iot, ioh, SSCOM_UTRSTAT);
		init_uerstat = bus_space_read_4(iot, ioh, SSCOM_UERSTAT);
		init_ufstat = bus_space_read_4(iot, ioh, SSCOM_UFSTAT);
		init_umstat = bus_space_read_4(iot, ioh, SSCOM_UMSTAT);
		init_ubrdiv = bus_space_read_4(iot, ioh, SSCOM_UBRDIV);
	}


	bus_space_write_4(iot, ioh, SSCOM_UCON, 0);
	bus_space_write_4(iot, ioh, SSCOM_UFCON, 
	    UFCON_TXTRIGGER_8 | UFCON_RXTRIGGER_8 |
	    UFCON_TXFIFO_RESET | UFCON_RXFIFO_RESET |
	    UFCON_FIFO_ENABLE );
	/* tx/rx fifo reset are auto-cleared */

	/* wait for fifo reset to complete */
	do {
		ufcon = bus_space_read_4(iot, ioh, SSCOM_UFCON);
	} while (ufcon & (UFCON_TXFIFO_RESET | UFCON_RXFIFO_RESET));


	rate = sscomspeed(rate, frequency);
	bus_space_write_4(iot, ioh, SSCOM_UBRDIV, rate);
	bus_space_write_4(iot, ioh, SSCOM_ULCON, cflag2lcr(cflag));

	/* enable UART */
	bus_space_write_4(iot, ioh, SSCOM_UCON, 
	    UCON_TXMODE_INT|UCON_RXMODE_INT);
	bus_space_write_4(iot, ioh, SSCOM_UMCON, UMCON_RTS);

	*iohp = ioh;
	return 0;
}

#endif

#if defined(SSCOM0CONSOLE) || defined(SSCOM1CONSOLE)
/*
 * Following are all routines needed for SSCOM to act as console
 */
struct consdev sscomcons = {
	NULL, NULL, sscomcngetc, sscomcnputc, sscomcnpollc, NULL,
	NODEV, CN_HIGHPRI
};

int
sscom_cnattach(bus_space_tag_t iot, const struct sscom_uart_info *config,
    int rate, int frequency, tcflag_t cflag)
{
	int res;


	res = sscom_init(iot, config, rate, frequency, cflag, &sscomconsioh);
	if (res)
		return res;

	cn_tab = &sscomcons;
#if 0
	cn_init_magic(&sscom_cnm_state);
	cn_set_magic("\047\001"); /* default magic is BREAK */
#endif

	sscomconstag = iot;
	sscomconsunit = config->unit;
	sscomconsrate = rate;
	sscomconscflag = cflag;

	return 0;
}

void
sscom_cndetach(void)
{
	bus_space_unmap(sscomconstag, sscomconsioh, SSCOM_SIZE);
	sscomconstag = NULL;

	cn_tab = NULL;
}

/*
 * The read-ahead code is so that you can detect pending in-band
 * cn_magic in polled mode while doing output rather than having to
 * wait until the kernel decides it needs input.
 */

#define MAX_READAHEAD	20
static int sscom_readahead[MAX_READAHEAD];
static int sscom_readaheadcount = 0;

int
sscomcngetc(dev_t dev)
{
	int s = spltty();
	u_char stat, c;

	/* got a character from reading things earlier */
	if (sscom_readaheadcount > 0) {
		int i;

		c = sscom_readahead[0];
		for (i = 1; i < sscom_readaheadcount; i++) {
			sscom_readahead[i-1] = sscom_readahead[i];
		}
		sscom_readaheadcount--;
		splx(s);
		return c;
	}

	/* block until a character becomes available */
	while (!sscom_rxrdy(sscomconstag, sscomconsioh))
		;

	c = sscom_getc(sscomconstag, sscomconsioh);
	stat = sscom_geterr(sscomconstag, sscomconsioh);
	{
#ifdef DDB
		extern int db_active;
		if (!db_active)
#endif
#if 0
			cn_check_magic(dev, c, sscom_cnm_state);
#else
			;
#endif
	}
	splx(s);
	return c;
}

/*
 * Console kernel output character routine.
 */
void
sscomcnputc(dev_t dev, int c)
{
	int s = spltty();
	int timo;

	int cin, stat;
	if (sscom_readaheadcount < MAX_READAHEAD && 
	    sscom_rxrdy(sscomconstag, sscomconsioh)) {
	    
		cin = sscom_getc(sscomconstag, sscomconsioh);
		stat = sscom_geterr(sscomconstag, sscomconsioh);
#if 0
		cn_check_magic(dev, cin, sscom_cnm_state);
#endif
		sscom_readahead[sscom_readaheadcount++] = cin;
	}

	/* wait for any pending transmission to finish */
	timo = 150000;
	while (ISSET(bus_space_read_4(sscomconstag, sscomconsioh, SSCOM_UFSTAT), 
		   UFSTAT_TXFULL) && --timo)
		continue;

	bus_space_write_4(sscomconstag, sscomconsioh, SSCOM_UTXH, c);
	SSCOM_BARRIER(sscomconstag, sscomconsioh, BR | BW);

#if 0	
	/* wait for this transmission to complete */
	timo = 1500000;
	while (!ISSET(bus_space_read_4(sscomconstag, sscomconsioh, SSCOM_UTRSTAT), 
		   UTRSTAT_TXEMPTY) && --timo)
		continue;
#endif
	splx(s);
}

void
sscomcnpollc(dev_t dev, int on)
{

}

#endif /* SSCOM0CONSOLE||SSCOM1CONSOLE */

#ifdef KGDB
int
sscom_kgdb_attach(bus_space_tag_t iot, const struct sscom_uart_info *config,
    int rate, int frequency, tcflag_t cflag)
{
	int res;

	if (iot == sscomconstag && config->unit == sscomconsunit) {
		printf( "console==kgdb_port (%d): kgdb disabled\n", sscomconsunit);
		return EBUSY; /* cannot share with console */
	}

	res = sscom_init(iot, config, rate, frequency, cflag, &sscom_kgdb_ioh);
	if (res)
		return res;

	kgdb_attach(sscom_kgdb_getc, sscom_kgdb_putc, NULL);
	kgdb_dev = 123; /* unneeded, only to satisfy some tests */

	sscom_kgdb_iot = iot;
	sscom_kgdb_unit = config->unit;

	return 0;
}

/* ARGSUSED */
int
sscom_kgdb_getc(void *arg)
{
	int c, stat;

	/* block until a character becomes available */
	while (!sscom_rxrdy(sscom_kgdb_iot, sscom_kgdb_ioh))
		;

	c = sscom_getc(sscom_kgdb_iot, sscom_kgdb_ioh);
	stat = sscom_geterr(sscom_kgdb_iot, sscom_kgdb_ioh);

	return c;
}

/* ARGSUSED */
void
sscom_kgdb_putc(void *arg, int c)
{
	int timo;

	/* wait for any pending transmission to finish */
	timo = 150000;
	while (ISSET(bus_space_read_4(sscom_kgdb_iot, sscom_kgdb_ioh,
	    SSCOM_UFSTAT), UFSTAT_TXFULL) && --timo)
		continue;

	bus_space_write_4(sscom_kgdb_iot, sscom_kgdb_ioh, SSCOM_UTXH, c);
	SSCOM_BARRIER(sscom_kgdb_iot, sscom_kgdb_ioh, BR | BW);

#if 0	
	/* wait for this transmission to complete */
	timo = 1500000;
	while (!ISSET(bus_space_read_4(sscom_kgdb_iot, sscom_kgdb_ioh,
	    SSCOM_UTRSTAT), UTRSTAT_TXEMPTY) && --timo)
		continue;
#endif
}
#endif /* KGDB */

/* helper function to identify the sscom ports used by
 console or KGDB (and not yet autoconf attached) */
int
sscom_is_console(bus_space_tag_t iot, int unit,
    bus_space_handle_t *ioh)
{
	bus_space_handle_t help;

	if (!sscomconsattached &&
	    iot == sscomconstag && unit == sscomconsunit)
		help = sscomconsioh;
#ifdef KGDB
	else if (!sscom_kgdb_attached &&
	    iot == sscom_kgdb_iot && unit == sscom_kgdb_unit)
		help = sscom_kgdb_ioh;
#endif
	else
		return 0;

	if (ioh)
		*ioh = help;
	return 1;
}
