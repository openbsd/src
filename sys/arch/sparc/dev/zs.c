/*	$OpenBSD: zs.c,v 1.21 1998/07/21 22:33:42 marc Exp $	*/
/*	$NetBSD: zs.c,v 1.49 1997/08/31 21:26:37 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)zs.c	8.1 (Berkeley) 7/19/93
 */

/*
 * Zilog Z8530 (ZSCC) driver.
 *
 * Runs two tty ports (ttya and ttyb) on zs0,
 * and runs a keyboard and mouse on zs1, and
 * possibly two more tty ports (ttyc and ttyd) on zs2.
 *
 * This driver knows far too much about chip to usage mappings.
 */
#include "zs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/kbd.h>

#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/auxioreg.h>
#include <dev/ic/z8530reg.h>
#include <sparc/dev/zsvar.h>

#ifdef KGDB
#include <sys/kgdb.h>
#include <machine/remote-sl.h>
#endif

#define DEVUNIT(x)      (minor(x) & 0x7f)
#define DEVCUA(x)       (minor(x) & 0x80)

/* Macros to clear/set/test flags. */
#define SET(t, f)       (t) |= (f)
#define CLR(t, f)       (t) &= ~(f)
#define ISSET(t, f)     ((t) & (f))

#define	ZSMAJOR	12		/* XXX */

#define	ZS_KBD		2	/* XXX */
#define	ZS_MOUSE	3	/* XXX */

/* the magic number below was stolen from the Sprite source. */
#define PCLK	(19660800/4)	/* PCLK pin input clock rate */

/*
 * Select software interrupt bit based on TTY ipl.
 */
#if PIL_TTY == 1
# define IE_ZSSOFT IE_L1
#elif PIL_TTY == 4
# define IE_ZSSOFT IE_L4
#elif PIL_TTY == 6
# define IE_ZSSOFT IE_L6
#else
# error "no suitable software interrupt bit"
#endif

/*
 * Software state per found chip.
 */
struct zs_softc {
	struct	device sc_dev;			/* base device */
	volatile struct zsdevice *sc_zs;	/* chip registers */
	struct	evcnt sc_intrcnt;		/* count interrupts */
	struct	zs_chanstate sc_cs[2];		/* chan A/B software state */
};

/* Definition of the driver for autoconfig. */
static int	zsmatch __P((struct device *, void *, void *));
static void	zsattach __P((struct device *, struct device *, void *));

struct cfattach zs_ca = {
	sizeof(struct zs_softc), zsmatch, zsattach
};

struct cfdriver zs_cd = {
	NULL, "zs", DV_TTY
};

/* Interrupt handlers. */
static int	zshard __P((void *));
static struct intrhand levelhard = { zshard };
static int	zssoft __P((void *));
static struct intrhand levelsoft = { zssoft };

struct zs_chanstate *zslist;

/* Routines called from other code. */
static void	zsiopen __P((struct tty *));
static void	zsiclose __P((struct tty *));
static void	zsstart __P((struct tty *));
static int	zsparam __P((struct tty *, struct termios *));

/* Routines purely local to this driver. */
static int	zs_getspeed __P((volatile struct zschan *));
#ifdef KGDB
static void	zs_reset __P((volatile struct zschan *, int, int));
#endif
static void	zs_modem __P((struct zs_chanstate *, int));
static void	zs_loadchannelregs __P((volatile struct zschan *, u_char *));
static void	tiocm_to_zs __P((struct zs_chanstate *, int how, int data));

/* Console stuff. */
static struct tty *zs_ctty;	/* console `struct tty *' */
static int zs_consin = -1, zs_consout = -1;
static struct zs_chanstate *zs_conscs = NULL; /*console channel state */
static void zscnputc __P((int));	/* console putc function */
static volatile struct zschan *zs_conschan;
static struct tty *zs_checkcons __P((struct zs_softc *, int,
    struct zs_chanstate *));

#ifdef KGDB
/* KGDB stuff.  Must reboot to change zs_kgdbunit. */
extern int kgdb_dev, kgdb_rate;
static int zs_kgdb_savedspeed;
static void zs_checkkgdb __P((int, struct zs_chanstate *, struct tty *));
void zskgdb __P((int));
static int zs_kgdb_getc __P((void *));
static void zs_kgdb_putc __P((void *, int));
#endif

static int zsrint __P((struct zs_chanstate *, volatile struct zschan *));
static int zsxint __P((struct zs_chanstate *, volatile struct zschan *));
static int zssint __P((struct zs_chanstate *, volatile struct zschan *));

void zsabort __P((int));
static void zsoverrun __P((int, long *, char *));

static volatile struct zsdevice *zsaddr[NZS];	/* XXX, but saves work */

/*
 * Console keyboard L1-A processing is done in the hardware interrupt code,
 * so we need to duplicate some of the console keyboard decode state.  (We
 * must not use the regular state as the hardware code keeps ahead of the
 * software state: the software state tracks the most recent ring input but
 * the hardware state tracks the most recent ZSCC input.)  See also kbd.h.
 */
static struct conk_state {	/* console keyboard state */
	char	conk_id;	/* true => ID coming up (console only) */
	char	conk_l1;	/* true => L1 pressed (console only) */
} zsconk_state;

int zshardscope;
int zsshortcuts;		/* number of "shortcut" software interrupts */

#ifdef SUN4
static u_int zs_read __P((volatile struct zschan *, u_int reg));
static u_int zs_write __P((volatile struct zschan *, u_int, u_int));

static u_int
zs_read(zc, reg)
	volatile struct zschan *zc;
	u_int reg;
{
	u_char val;

	zc->zc_csr = reg;
	ZS_DELAY();
	val = zc->zc_csr;
	ZS_DELAY();
	return val;
}

static u_int
zs_write(zc, reg, val)
	volatile struct zschan *zc;
	u_int reg, val;
{
	zc->zc_csr = reg;
	ZS_DELAY();
	zc->zc_csr = val;
	ZS_DELAY();
	return val;
}
#endif /* SUN4 */

/*
 * Match slave number to zs unit number, so that misconfiguration will
 * not set up the keyboard as ttya, etc.
 */
static int
zsmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	if ((ca->ca_bustype == BUS_MAIN && !CPU_ISSUN4) ||
	    (ca->ca_bustype == BUS_OBIO && CPU_ISSUN4M))
		return (getpropint(ra->ra_node, "slave", -2) == cf->cf_unit);
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 1) != -1);
}

/*
 * Attach a found zs.
 *
 * USE ROM PROPERTIES port-a-ignore-cd AND port-b-ignore-cd FOR
 * SOFT CARRIER, AND keyboard PROPERTY FOR KEYBOARD/MOUSE?
 */
static void
zsattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
	register int zs = dev->dv_unit, unit;
	register struct zs_softc *sc;
	register struct zs_chanstate *cs;
	register volatile struct zsdevice *addr;
	register struct tty *tp, *ctp;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;
	int pri;
	static int didintr, prevpri;
	int ringsize;

	if ((addr = zsaddr[zs]) == NULL)
		addr = zsaddr[zs] = (volatile struct zsdevice *)findzs(zs);
	if (ca->ca_bustype==BUS_MAIN)
		if ((void *)addr != ra->ra_vaddr)
			panic("zsattach");
	if (ra->ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ra->ra_nintr);
		return;
	}
	pri = ra->ra_intr[0].int_pri;
	printf(" pri %d, softpri %d\n", pri, PIL_TTY);
	if (!didintr) {
		didintr = 1;
		prevpri = pri;
		intr_establish(pri, &levelhard);
		intr_establish(PIL_TTY, &levelsoft);
	} else if (pri != prevpri)
		panic("broken zs interrupt scheme");
	sc = (struct zs_softc *)dev;
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);
	sc->sc_zs = addr;
	unit = zs * 2;
	cs = sc->sc_cs;

	/* link into interrupt list with order (A,B) (B=A+1) */
	cs[0].cs_next = &cs[1];
	cs[0].cs_sc = sc;
	cs[1].cs_next = zslist;
	cs[1].cs_sc = sc;
	zslist = cs;

	cs->cs_unit = unit;
	cs->cs_speed = zs_getspeed(&addr->zs_chan[ZS_CHAN_A]);
	cs->cs_zc = &addr->zs_chan[ZS_CHAN_A];
	if ((ctp = zs_checkcons(sc, unit, cs)) != NULL)
		tp = ctp;
	else {
		tp = ttymalloc();
		tp->t_dev = makedev(ZSMAJOR, unit);
		tp->t_oproc = zsstart;
		tp->t_param = zsparam;
	}
	cs->cs_ttyp = tp;
#ifdef KGDB
	if (ctp == NULL)
		zs_checkkgdb(unit, cs, tp);
#endif
	if (unit == ZS_KBD) {
		/*
		 * Keyboard: tell /dev/kbd driver how to talk to us.
		 */
		tp->t_ispeed = tp->t_ospeed = cs->cs_speed;
		tp->t_cflag = CS8;
		kbd_serial(tp, zsiopen, zsiclose);
		cs->cs_conk = 1;		/* do L1-A processing */
		ringsize = 128;
	} else {
		if (tp != ctp)
			tty_attach(tp);
		ringsize = 4096;
		if (unit == zs_consout)
			zs_conscs = cs;
	}
	cs->cs_ringmask = ringsize - 1;
	cs->cs_rbuf = malloc((u_long)ringsize * sizeof(*cs->cs_rbuf),
			      M_DEVBUF, M_NOWAIT);

	unit++;
	cs++;
	cs->cs_unit = unit;
	cs->cs_speed = zs_getspeed(&addr->zs_chan[ZS_CHAN_B]);
	cs->cs_zc = &addr->zs_chan[ZS_CHAN_B];
	if ((ctp = zs_checkcons(sc, unit, cs)) != NULL)
		tp = ctp;
	else {
		tp = ttymalloc();
		tp->t_dev = makedev(ZSMAJOR, unit);
		tp->t_oproc = zsstart;
		tp->t_param = zsparam;
	}
	cs->cs_ttyp = tp;
#ifdef KGDB
	if (ctp == NULL)
		zs_checkkgdb(unit, cs, tp);
#endif
	if (unit == ZS_MOUSE) {
		/*
		 * Mouse: tell /dev/mouse driver how to talk to us.
		 */
		tp->t_ispeed = tp->t_ospeed = B1200;
		tp->t_cflag = CS8;
		ms_serial(tp, zsiopen, zsiclose);
		ringsize = 128;
	} else {
		if (tp != ctp)
			tty_attach(tp);
		ringsize = 4096;
		if (unit == zs_consout)
			zs_conscs = cs;
	}
	cs->cs_ringmask = ringsize - 1;
	cs->cs_rbuf = malloc((u_long)ringsize * sizeof(*cs->cs_rbuf),
			      M_DEVBUF, M_NOWAIT);
}

#ifdef KGDB
/*
 * Put a channel in a known state.  Interrupts may be left disabled
 * or enabled, as desired.
 */
static void
zs_reset(zc, inten, speed)
	volatile struct zschan *zc;
	int inten, speed;
{
	int tconst;
	static u_char reg[16] = {
		0,
		0,
		0,
		ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
		ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
		ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
		0,
		0,
		0,
		0,
		ZSWR10_NRZ,
		ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
		0,
		0,
		ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA,
		ZSWR15_BREAK_IE | ZSWR15_DCD_IE,
	};

	reg[9] = inten ? ZSWR9_MASTER_IE | ZSWR9_NO_VECTOR : ZSWR9_NO_VECTOR;
	tconst = BPS_TO_TCONST(PCLK / 16, speed);
	reg[12] = tconst;
	reg[13] = tconst >> 8;
	zs_loadchannelregs(zc, reg);
}
#endif

/*
 * Declare the given tty (which is in fact &cons) as a console input
 * or output.  This happens before the zs chip is attached; the hookup
 * is finished later, in zs_setcons() below.
 *
 * This is used only for ports a and b.  The console keyboard is decoded
 * independently (we always send unit-2 input to /dev/kbd, which will
 * direct it to /dev/console if appropriate).
 */
void
zsconsole(tp, unit, out, fnstop)
	register struct tty *tp;
	register int unit;
	int out;
	int (**fnstop) __P((struct tty *, int));
{
	int zs;
	volatile struct zsdevice *addr;

	if (unit >= ZS_KBD)
		panic("zsconsole");
	if (out) {
		zs_consout = unit;
		zs = unit >> 1;
		if ((addr = zsaddr[zs]) == NULL)
			addr = zsaddr[zs] = (volatile struct zsdevice *)findzs(zs);
		zs_conschan = (unit & 1) == 0 ? &addr->zs_chan[ZS_CHAN_A] :
		    &addr->zs_chan[ZS_CHAN_B];
		v_putc = zscnputc;
	} else
		zs_consin = unit;
	if (fnstop)
		*fnstop = &zsstop;
	zs_ctty = tp;
}

/*
 * Polled console output putchar.
 */
static void
zscnputc(c)
	int c;
{
	register volatile struct zschan *zc = zs_conschan;
	register int s;

	if (c == '\n')
		zscnputc('\r');
	/*
	 * Must block output interrupts (i.e., raise to >= splzs) without
	 * lowering current ipl.  Need a better way.
	 */
	s = splhigh();
	if (CPU_ISSUN4C && s <= (12 << 8)) /* XXX */
		(void) splzs();
	while ((zc->zc_csr & ZSRR0_TX_READY) == 0)
		ZS_DELAY();
	/*
	 * If transmitter was busy doing regular tty I/O (ZSWR1_TIE on),
	 * defer our output until the transmit interrupt runs. We still
	 * sync with TX_READY so we can get by with a single-char "queue".
	 */
	if (zs_conscs != NULL && (zs_conscs->cs_creg[1] & ZSWR1_TIE)) {
		/*
		 * If previous not yet done, send it now; zsxint()
		 * will field the interrupt for our char, but doesn't
		 * care. We're running at sufficiently high spl for
		 * this to work.
		 */
		if (zs_conscs->cs_deferred_cc != 0)
			zc->zc_data = zs_conscs->cs_deferred_cc;
		zs_conscs->cs_deferred_cc = c;
		splx(s);
		return;
	}
	zc->zc_data = c;
	ZS_DELAY();
	splx(s);
}

/*
 * Set up the given unit as console input, output, both, or neither, as
 * needed.  Return console tty if it is to receive console input.
 */
static struct tty *
zs_checkcons(sc, unit, cs)
	struct zs_softc *sc;
	int unit;
	struct zs_chanstate *cs;
{
	register struct tty *tp;
	char *i, *o;

	if ((tp = zs_ctty) == NULL) /* XXX */
		return (0);
	i = zs_consin == unit ? "input" : NULL;
	o = zs_consout == unit ? "output" : NULL;
	if (i == NULL && o == NULL)
		return (0);

	/* rewire the minor device (gack) */
	tp->t_dev = makedev(major(tp->t_dev), unit);

	/*
	 * Rewire input and/or output.  Note that baud rate reflects
	 * input settings, not output settings, but we can do no better
	 * if the console is split across two ports.
	 *
	 * XXX	split consoles don't work anyway -- this needs to be
	 *	thrown away and redone
	 */
	if (i) {
		tp->t_param = zsparam;
		tp->t_ispeed = tp->t_ospeed = cs->cs_speed;
		tp->t_cflag = CS8;
		ttsetwater(tp);
	}
	if (o) {
		tp->t_oproc = zsstart;
	}
	printf("%s%c: console %s\n",
	    sc->sc_dev.dv_xname, (unit & 1) + 'a', i ? (o ? "i/o" : i) : o);
	cs->cs_consio = 1;
	cs->cs_brkabort = 1;
	return (tp);
}

#ifdef KGDB
/*
 * The kgdb zs port, if any, was altered at boot time (see zs_kgdb_init).
 * Pick up the current speed and character size and restore the original
 * speed.
 */
static void
zs_checkkgdb(unit, cs, tp)
	int unit;
	struct zs_chanstate *cs;
	struct tty *tp;
{

	if (kgdb_dev == makedev(ZSMAJOR, unit)) {
		tp->t_ispeed = tp->t_ospeed = kgdb_rate;
		tp->t_cflag = CS8;
		cs->cs_kgdb = 1;
		cs->cs_speed = zs_kgdb_savedspeed;
		(void) zsparam(tp, &tp->t_termios);
	}
}
#endif

/*
 * Compute the current baud rate given a ZSCC channel.
 */
static int
zs_getspeed(zc)
	register volatile struct zschan *zc;
{
	register int tconst;

	tconst = ZS_READ(zc, 12);
	tconst |= ZS_READ(zc, 13) << 8;
	return (TCONST_TO_BPS(PCLK / 16, tconst));
}


/*
 * Do an internal open.
 */
static void
zsiopen(tp)
	struct tty *tp;
{

	(void) zsparam(tp, &tp->t_termios);
	ttsetwater(tp);
	tp->t_state = TS_ISOPEN | TS_CARR_ON;
}

/*
 * Do an internal close.  Eventually we should shut off the chip when both
 * ports on it are closed.
 */
static void
zsiclose(tp)
	struct tty *tp;
{

	ttylclose(tp, 0);	/* ??? */
	ttyclose(tp);		/* ??? */
	tp->t_state = 0;
}



/*
 * Open a zs serial port.  This interface may not be used to open
 * the keyboard and mouse ports. (XXX)
 */
int
zsopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	register struct tty *tp;
	register struct zs_chanstate *cs;
	struct zs_softc *sc;
	int unit = DEVUNIT(dev);
	int zs = unit >> 1, error, s;

	if (zs >= zs_cd.cd_ndevs || (sc = zs_cd.cd_devs[zs]) == NULL ||
	    unit == ZS_KBD || unit == ZS_MOUSE)
		return (ENXIO);
	cs = &sc->sc_cs[unit & 1];
	if (cs->cs_consio)
		return (ENXIO);		/* ??? */
	tp = cs->cs_ttyp;
	s = spltty();
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = cs->cs_speed;
		}
		(void) zsparam(tp, &tp->t_termios);
		ttsetwater(tp);
/* XXX start CUA mods */
		if (DEVCUA(dev)) 
                  SET(tp->t_state, TS_CARR_ON);
                else 
                  CLR(tp->t_state, TS_CARR_ON);
/* end CUA mods */

	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return (EBUSY);
	}

/* XXX start CUA mods */
        if (DEVCUA(dev)) {
                if (ISSET(tp->t_state, TS_ISOPEN)) {
                        /* Ah, but someone already is dialed in... */
                        splx(s);
                        return EBUSY;
                }
                cs->cs_cua = 1;         /* We go into CUA mode */
        }


        error = 0;
        /* wait for carrier if necessary */
        if (ISSET(flags, O_NONBLOCK)) {
                if (!DEVCUA(dev) && cs->cs_cua) {
                        /* Opening TTY non-blocking... but the CUA is busy */
                        splx(s);
                        return EBUSY;
                }
        } else {
	    while (cs->cs_cua ||
	      (!ISSET(tp->t_cflag, CLOCAL) &&
	      !ISSET(tp->t_state, TS_CARR_ON))) {
		register int rr0;

                error = 0;
		SET(tp->t_state, TS_WOPEN);


              if (!DEVCUA(dev) && !cs->cs_cua) {
		/* loop, turning on the device, until carrier present */
		zs_modem(cs, 1);
		/* May never get status intr if carrier already on. -gwr */
		rr0 = cs->cs_zc->zc_csr;
		ZS_DELAY();
		if ((rr0 & ZSRR0_DCD) || cs->cs_softcar)
			tp->t_state |= TS_CARR_ON;
              }

		if ((tp->t_cflag & CLOCAL || tp->t_state & TS_CARR_ON) && 
			!cs->cs_cua)
			break;

		error = ttysleep(tp, (caddr_t)&tp->t_rawq, TTIPRI | PCATCH,
				 ttopen, 0);

		if (!DEVCUA(dev) && cs->cs_cua && error == EINTR) {
			error=0;
			continue;
                }

		if (error) {
			if (!(tp->t_state & TS_ISOPEN)) {
				zs_modem(cs, 0);
				CLR(tp->t_state, TS_WOPEN);
				ttwakeup(tp);
			}
/* XXX ordering of this might be important?? */
                        if (DEVCUA(dev))
                                cs->cs_cua = 0;
			CLR(tp->t_state, TS_WOPEN);
			splx(s);
			return error;
		}
                if (!DEVCUA(dev) && cs->cs_cua)
                        continue;
	    }
        } 
	splx(s);
/* end CUA mods */
	if (error == 0)
		error = linesw[tp->t_line].l_open(dev, tp);
	if (error)
		zs_modem(cs, 0);
	return (error);
}

/*
 * Close a zs serial port.
 */
int
zsclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	register struct zs_chanstate *cs;
	register struct tty *tp;
	struct zs_softc *sc;
	int unit = DEVUNIT(dev);
	int s, st;

	sc = zs_cd.cd_devs[unit >> 1];
	cs = &sc->sc_cs[unit & 1];
	tp = cs->cs_ttyp;
	linesw[tp->t_line].l_close(tp, flags);

/* XXX start CUA mods */
        st = spltty();
/* end CUA mods */
	if (tp->t_cflag & HUPCL || tp->t_state & TS_WOPEN ||
	    (tp->t_state & TS_ISOPEN) == 0) {
		zs_modem(cs, 0);
		/* hold low for 1 second */
		(void) tsleep((caddr_t)cs, TTIPRI, ttclos, hz);
	}
	if (cs->cs_creg[5] & ZSWR5_BREAK)
	{
		s = splzs();
		cs->cs_preg[5] &= ~ZSWR5_BREAK;
		cs->cs_creg[5] &= ~ZSWR5_BREAK;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		splx(s);
	}
/* XXX start CUA mods */
        CLR(tp->t_state, TS_CARR_ON | TS_BUSY | TS_FLUSH);
        cs->cs_cua = 0;
	splx(st);
/* end CUA mods */
	ttyclose(tp);
#ifdef KGDB
	/* Reset the speed if we're doing kgdb on this port */
	if (cs->cs_kgdb) {
		tp->t_ispeed = tp->t_ospeed = kgdb_rate;
		(void) zsparam(tp, &tp->t_termios);
	}
#endif
	return (0);
}

/*
 * Read/write zs serial port.
 */
int
zsread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register struct zs_chanstate *cs;
	register struct zs_softc *sc;
	register struct tty *tp;
	int unit = DEVUNIT(dev);

	sc = zs_cd.cd_devs[unit >> 1];
	cs = &sc->sc_cs[unit & 1];
	tp = cs->cs_ttyp;

	return (linesw[tp->t_line].l_read(tp, uio, flags));

}

int
zswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register struct zs_chanstate *cs;
	register struct zs_softc *sc;
	register struct tty *tp;
	int unit = DEVUNIT(dev);

	sc = zs_cd.cd_devs[unit >> 1];
	cs = &sc->sc_cs[unit & 1];
	tp = cs->cs_ttyp;

	return (linesw[tp->t_line].l_write(tp, uio, flags));
}

struct tty *
zstty(dev)
	dev_t dev;
{
	register struct zs_chanstate *cs;
	register struct zs_softc *sc;
	int unit = DEVUNIT(dev);

	sc = zs_cd.cd_devs[unit >> 1];
	cs = &sc->sc_cs[unit & 1];

	return (cs->cs_ttyp);
}

static int zsrint __P((struct zs_chanstate *, volatile struct zschan *));
static int zsxint __P((struct zs_chanstate *, volatile struct zschan *));
static int zssint __P((struct zs_chanstate *, volatile struct zschan *));

/*
 * ZS hardware interrupt.  Scan all ZS channels.  NB: we know here that
 * channels are kept in (A,B) pairs.
 *
 * Do just a little, then get out; set a software interrupt if more
 * work is needed.
 *
 * We deliberately ignore the vectoring Zilog gives us, and match up
 * only the number of `reset interrupt under service' operations, not
 * the order.
 */
/* ARGSUSED */
int
zshard(intrarg)
	void *intrarg;
{
	register struct zs_chanstate *a;
#define	b (a + 1)
	register volatile struct zschan *zc;
	register int rr3, intflags = 0, v, i, ringmask;

#define ZSHARD_NEED_SOFTINTR	1
#define ZSHARD_WAS_SERVICED	2
#define ZSHARD_CHIP_GOTINTR	4

	for (a = zslist; a != NULL; a = b->cs_next) {
		ringmask = a->cs_ringmask;
		rr3 = ZS_READ(a->cs_zc, 3);
		if (rr3 & (ZSRR3_IP_A_RX|ZSRR3_IP_A_TX|ZSRR3_IP_A_STAT)) {
			intflags |= (ZSHARD_CHIP_GOTINTR|ZSHARD_WAS_SERVICED);
			zc = a->cs_zc;
			i = a->cs_rbput;
			if (rr3 & ZSRR3_IP_A_RX && (v = zsrint(a, zc)) != 0) {
				a->cs_rbuf[i++ & ringmask] = v;
				intflags |= ZSHARD_NEED_SOFTINTR;
			}
			if (rr3 & ZSRR3_IP_A_TX && (v = zsxint(a, zc)) != 0) {
				a->cs_rbuf[i++ & ringmask] = v;
				intflags |= ZSHARD_NEED_SOFTINTR;
			}
			if (rr3 & ZSRR3_IP_A_STAT && (v = zssint(a, zc)) != 0) {
				a->cs_rbuf[i++ & ringmask] = v;
				intflags |= ZSHARD_NEED_SOFTINTR;
			}
			a->cs_rbput = i;
		}
		if (rr3 & (ZSRR3_IP_B_RX|ZSRR3_IP_B_TX|ZSRR3_IP_B_STAT)) {
			intflags |= (ZSHARD_CHIP_GOTINTR|ZSHARD_WAS_SERVICED);
			zc = b->cs_zc;
			i = b->cs_rbput;
			if (rr3 & ZSRR3_IP_B_RX && (v = zsrint(b, zc)) != 0) {
				b->cs_rbuf[i++ & ringmask] = v;
				intflags |= ZSHARD_NEED_SOFTINTR;
			}
			if (rr3 & ZSRR3_IP_B_TX && (v = zsxint(b, zc)) != 0) {
				b->cs_rbuf[i++ & ringmask] = v;
				intflags |= ZSHARD_NEED_SOFTINTR;
			}
			if (rr3 & ZSRR3_IP_B_STAT && (v = zssint(b, zc)) != 0) {
				b->cs_rbuf[i++ & ringmask] = v;
				intflags |= ZSHARD_NEED_SOFTINTR;
			}
			b->cs_rbput = i;
		}
		if (intflags & ZSHARD_CHIP_GOTINTR) {
			a->cs_sc->sc_intrcnt.ev_count++;
			intflags &= ~ZSHARD_CHIP_GOTINTR;
		}
	}
#undef b

	if (intflags & ZSHARD_NEED_SOFTINTR) {
		if (CPU_ISSUN4COR4M) {
			/* XXX -- but this will go away when zshard moves to locore.s */
			struct clockframe *p = intrarg;

			if ((p->psr & PSR_PIL) < (PIL_TTY << 8)) {
				zsshortcuts++;
				(void) spltty();
				if (zshardscope) {
					LED_ON;
					LED_OFF;
				}
				return (zssoft(intrarg));
			}
		}

#if defined(SUN4M)
		if (CPU_ISSUN4M)
			raise(0, PIL_TTY);
		else
#endif
		ienab_bis(IE_ZSSOFT);
	}
	return (intflags & ZSHARD_WAS_SERVICED);
}

static int
zsrint(cs, zc)
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
{
	register u_int c = zc->zc_data;

	ZS_DELAY();
	if (cs->cs_conk) {
		register struct conk_state *conk = &zsconk_state;

		/*
		 * Check here for console abort function, so that we
		 * can abort even when interrupts are locking up the
		 * machine.
		 */
		if (c == KBD_RESET) {
			conk->conk_id = 1;	/* ignore next byte */
			conk->conk_l1 = 0;
		} else if (conk->conk_id)
			conk->conk_id = 0;	/* stop ignoring bytes */
		else if (c == KBD_L1)
			conk->conk_l1 = 1;	/* L1 went down */
		else if (c == (KBD_L1|KBD_UP))
			conk->conk_l1 = 0;	/* L1 went up */
		else if (c == KBD_A && conk->conk_l1) {
			zsabort(cs->cs_unit);
			conk->conk_l1 = 0;	/* we never see the up */
			goto clearit;		/* eat the A after L1-A */
		}
	}
#ifdef KGDB
	if (c == FRAME_START && cs->cs_kgdb &&
	    (cs->cs_ttyp->t_state & TS_ISOPEN) == 0) {
		zskgdb(cs->cs_unit);
		goto clearit;
	}
#endif
	/* compose receive character and status */
	c <<= 8;
	c |= ZS_READ(zc, 1);

	/* clear receive error & interrupt condition */
	zc->zc_csr = ZSWR0_RESET_ERRORS;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();

	return (ZRING_MAKE(ZRING_RINT, c));

clearit:
	zc->zc_csr = ZSWR0_RESET_ERRORS;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();
	return (0);
}

static int
zsxint(cs, zc)
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
{
	register int i = cs->cs_tbc;

	if (cs->cs_deferred_cc != 0) {
		/* Handle deferred zscnputc() output first */
		zc->zc_data = cs->cs_deferred_cc;
		cs->cs_deferred_cc = 0;
		ZS_DELAY();
		zc->zc_csr = ZSWR0_CLR_INTR;
		ZS_DELAY();
		return (0);
	}
	if (i == 0) {
		zc->zc_csr = ZSWR0_RESET_TXINT;
		ZS_DELAY();
		zc->zc_csr = ZSWR0_CLR_INTR;
		ZS_DELAY();
		return (ZRING_MAKE(ZRING_XINT, 0));
	}
	cs->cs_tbc = i - 1;
	zc->zc_data = *cs->cs_tba++;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();
	return (0);
}

static int
zssint(cs, zc)
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
{
	register u_int rr0;

	rr0 = zc->zc_csr;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_RESET_STATUS;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();
	/*
	 * The chip's hardware flow control is, as noted in zsreg.h,
	 * busted---if the DCD line goes low the chip shuts off the
	 * receiver (!).  If we want hardware CTS flow control but do
	 * not have it, and carrier is now on, turn HFC on; if we have
	 * HFC now but carrier has gone low, turn it off.
	 */
	if (rr0 & ZSRR0_DCD) {
		if (cs->cs_ttyp->t_cflag & CCTS_OFLOW &&
		    (cs->cs_creg[3] & ZSWR3_HFC) == 0) {
			cs->cs_creg[3] |= ZSWR3_HFC;
			ZS_WRITE(zc, 3, cs->cs_creg[3]);
		}
	} else {
		if (cs->cs_creg[3] & ZSWR3_HFC) {
			cs->cs_creg[3] &= ~ZSWR3_HFC;
			ZS_WRITE(zc, 3, cs->cs_creg[3]);
		}
	}
	if ((rr0 & ZSRR0_BREAK) && cs->cs_brkabort) {
		/*
		 * XXX This might not be necessary. Test and
		 * delete if it isn't.
		 */
		if (CPU_ISSUN4) {
			while (zc->zc_csr & ZSRR0_BREAK)
				ZS_DELAY();
		}
		zsabort(cs->cs_unit);
		return (0);
	}
	return (ZRING_MAKE(ZRING_SINT, rr0));
}

void
zsabort(unit)
	int unit;
{

#if defined(KGDB)
	zskgdb(unit);
#elif defined(DDB)
	if (db_console)
		Debugger();
#else
	printf("stopping on keyboard abort\n");
	callrom();
#endif
}

#ifdef KGDB
/*
 * KGDB framing character received: enter kernel debugger.  This probably
 * should time out after a few seconds to avoid hanging on spurious input.
 */
void
zskgdb(unit)
	int unit;
{

	printf("zs%d%c: kgdb interrupt\n", unit >> 1, (unit & 1) + 'a');
	kgdb_connect(1);
}
#endif

/*
 * Print out a ring or fifo overrun error message.
 */
static void
zsoverrun(unit, ptime, what)
	int unit;
	long *ptime;
	char *what;
{

	if (*ptime != time.tv_sec) {
		*ptime = time.tv_sec;
		log(LOG_WARNING, "zs%d%c: %s overrun\n", unit >> 1,
		    (unit & 1) + 'a', what);
	}
}

/*
 * ZS software interrupt.  Scan all channels for deferred interrupts.
 */
int
zssoft(arg)
	void *arg;
{
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
	register struct linesw *line;
	register struct tty *tp;
	register int get, n, c, cc, unit, s, ringmask, ringsize;
	int	retval = 0;

	for (cs = zslist; cs != NULL; cs = cs->cs_next) {
		ringmask = cs->cs_ringmask;
		get = cs->cs_rbget;
again:
		n = cs->cs_rbput;	/* atomic */
		if (get == n)		/* nothing more on this line */
			continue;
		retval = 1;
		unit = cs->cs_unit;	/* set up to handle interrupts */
		zc = cs->cs_zc;
		tp = cs->cs_ttyp;
		line = &linesw[tp->t_line];
		/*
		 * Compute the number of interrupts in the receive ring.
		 * If the count is overlarge, we lost some events, and
		 * must advance to the first valid one.  It may get
		 * overwritten if more data are arriving, but this is
		 * too expensive to check and gains nothing (we already
		 * lost out; all we can do at this point is trade one
		 * kind of loss for another).
		 */
		ringsize = ringmask + 1;
		n -= get;
		if (n > ringsize) {
			zsoverrun(unit, &cs->cs_rotime, "ring");
			get += n - ringsize;
			n = ringsize;
		}
		while (--n >= 0) {
			/* race to keep ahead of incoming interrupts */
			c = cs->cs_rbuf[get++ & ringmask];
			switch (ZRING_TYPE(c)) {

			case ZRING_RINT:
				c = ZRING_VALUE(c);
				if (c & ZSRR1_DO)
					zsoverrun(unit, &cs->cs_fotime, "fifo");
				cc = c >> 8;
				if (c & ZSRR1_FE)
					cc |= TTY_FE;
				if (c & ZSRR1_PE)
					cc |= TTY_PE;
				/*
				 * this should be done through
				 * bstreams	XXX gag choke
				 */
				if (unit == ZS_KBD)
					kbd_rint(cc);
				else if (unit == ZS_MOUSE)
					ms_rint(cc);
				else
					line->l_rint(cc, tp);
				break;

			case ZRING_XINT:
				/*
				 * Transmit done: change registers and resume,
				 * or clear BUSY.
				 */
				if (cs->cs_heldchange) {
					s = splzs();
					c = zc->zc_csr;
					ZS_DELAY();
					if ((c & ZSRR0_DCD) == 0)
						cs->cs_preg[3] &= ~ZSWR3_HFC;
					bcopy((caddr_t)cs->cs_preg,
					    (caddr_t)cs->cs_creg, 16);
					zs_loadchannelregs(zc, cs->cs_creg);
					splx(s);
					cs->cs_heldchange = 0;
					if (cs->cs_heldtbc &&
					    (tp->t_state & TS_TTSTOP) == 0) {
						cs->cs_tbc = cs->cs_heldtbc - 1;
						zc->zc_data = *cs->cs_tba++;
						ZS_DELAY();
						goto again;
					}
				}
				tp->t_state &= ~TS_BUSY;
				if (tp->t_state & TS_FLUSH)
					tp->t_state &= ~TS_FLUSH;
				else
					ndflush(&tp->t_outq,
					 cs->cs_tba - (caddr_t)tp->t_outq.c_cf);
				line->l_start(tp);
				break;

			case ZRING_SINT:
				/*
				 * Status line change.  HFC bit is run in
				 * hardware interrupt, to avoid locking
				 * at splzs here.
				 */
				c = ZRING_VALUE(c);
				if ((c ^ cs->cs_rr0) & ZSRR0_DCD) {
					cc = (c & ZSRR0_DCD) != 0;
					if (line->l_modem(tp, cc) == 0)
						zs_modem(cs, cc);
				}
				cs->cs_rr0 = c;
				break;

			default:
				log(LOG_ERR, "zs%d%c: bad ZRING_TYPE (0x%x)\n",
				    unit >> 1, (unit & 1) + 'a', c);
				break;
			}
		}
		cs->cs_rbget = get;
		goto again;
	}
	return (retval);
}

int
zsioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DEVUNIT(dev);
	struct zs_softc *sc = zs_cd.cd_devs[unit >> 1];
	register struct zs_chanstate *cs = &sc->sc_cs[unit & 1];
	register struct tty *tp = cs->cs_ttyp;
	register int error, s;

	error = linesw[tp->t_line].l_ioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {
	case TIOCSBRK:
		s = splzs();
		cs->cs_preg[5] |= ZSWR5_BREAK;
		cs->cs_creg[5] |= ZSWR5_BREAK;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		splx(s);
		break;
	case TIOCCBRK:
		s = splzs();
		cs->cs_preg[5] &= ~ZSWR5_BREAK;
		cs->cs_creg[5] &= ~ZSWR5_BREAK;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		splx(s);
		break;
	case TIOCGFLAGS: {
		int bits = 0;

		if (cs->cs_softcar)
			bits |= TIOCFLAG_SOFTCAR;
		if (cs->cs_creg[15] & ZSWR15_DCD_IE)
			bits |= TIOCFLAG_CLOCAL;
		if (cs->cs_creg[3] & ZSWR3_HFC)
			bits |= TIOCFLAG_CRTSCTS;
		*(int *)data = bits;
		break;
	}
	case TIOCSFLAGS: {
		int userbits;

		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0)
			return (EPERM);

		userbits = *(int *)data;

		/*
		 * can have `local' or `softcar', and `rtscts' or `mdmbuf'
		 # defaulting to software flow control.
		 */
		if (userbits & TIOCFLAG_SOFTCAR && userbits & TIOCFLAG_CLOCAL)
			return(EINVAL);
		if (userbits & TIOCFLAG_MDMBUF)	/* don't support this (yet?) */
			return(ENXIO);

		s = splzs();
		if ((userbits & TIOCFLAG_SOFTCAR) || cs->cs_consio) {
			cs->cs_softcar = 1;	/* turn on softcar */
			cs->cs_preg[15] &= ~ZSWR15_DCD_IE; /* turn off dcd */
			cs->cs_creg[15] &= ~ZSWR15_DCD_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
		} else if (userbits & TIOCFLAG_CLOCAL) {
			cs->cs_softcar = 0; 	/* turn off softcar */
			cs->cs_preg[15] |= ZSWR15_DCD_IE; /* turn on dcd */
			cs->cs_creg[15] |= ZSWR15_DCD_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
			tp->t_termios.c_cflag |= CLOCAL;
		}
		if (userbits & TIOCFLAG_CRTSCTS) {
			cs->cs_preg[15] |= ZSWR15_CTS_IE;
			cs->cs_creg[15] |= ZSWR15_CTS_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
			cs->cs_preg[3] |= ZSWR3_HFC;
			cs->cs_creg[3] |= ZSWR3_HFC;
			ZS_WRITE(cs->cs_zc, 3, cs->cs_creg[3]);
			tp->t_termios.c_cflag |= CRTSCTS;
		} else {
			/* no mdmbuf, so we must want software flow control */
			cs->cs_preg[15] &= ~ZSWR15_CTS_IE;
			cs->cs_creg[15] &= ~ZSWR15_CTS_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
			cs->cs_preg[3] &= ~ZSWR3_HFC;
			cs->cs_creg[3] &= ~ZSWR3_HFC;
			ZS_WRITE(cs->cs_zc, 3, cs->cs_creg[3]);
			tp->t_termios.c_cflag &= ~CRTSCTS;
		}
		splx(s);
		break;
	}
	case TIOCSDTR:
		zs_modem(cs, 1);
		break;
	case TIOCCDTR:
		zs_modem(cs, 0);
		break;
	case TIOCMSET:
		tiocm_to_zs(cs, TIOCMSET, *(int *)data);
		break;
	case TIOCMBIS:
		tiocm_to_zs(cs, TIOCMBIS, *(int *)data);
		break;
	case TIOCMBIC:
		tiocm_to_zs(cs, TIOCMBIC, *(int *)data);
		break;
	case TIOCMGET: {
		int bits = 0;
		u_char m;

		if (cs->cs_preg[5] & ZSWR5_DTR)
			bits |= TIOCM_DTR;
		if (cs->cs_preg[5] & ZSWR5_RTS)
			bits |= TIOCM_RTS;
		m = cs->cs_zc->zc_csr;
		if (m & ZSRR0_DCD)
			bits |= TIOCM_CD;
		if (m & ZSRR0_CTS)
			bits |= TIOCM_CTS;
		*(int *)data = bits;
		break;
	}
	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Start or restart transmission.
 */
static void
zsstart(tp)
	register struct tty *tp;
{
	register struct zs_chanstate *cs;
	register int s, nch;
	int unit = DEVUNIT(tp->t_dev);
	struct zs_softc *sc = zs_cd.cd_devs[unit >> 1];

	cs = &sc->sc_cs[unit & 1];
	s = spltty();

	/*
	 * If currently active or delaying, no need to do anything.
	 */
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto out;

	/*
	 * If there are sleepers, and output has drained below low
	 * water mark, awaken.
	 */
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}

	nch = ndqb(&tp->t_outq, 0);	/* XXX */
	if (nch) {
		register char *p = tp->t_outq.c_cf;

		/* mark busy, enable tx done interrupts, & send first byte */
		tp->t_state |= TS_BUSY;
		(void) splzs();
		cs->cs_preg[1] |= ZSWR1_TIE;
		cs->cs_creg[1] |= ZSWR1_TIE;
		ZS_WRITE(cs->cs_zc, 1, cs->cs_creg[1]);
		cs->cs_zc->zc_data = *p;
		ZS_DELAY();
		cs->cs_tba = p + 1;
		cs->cs_tbc = nch - 1;
	} else {
		/*
		 * Nothing to send, turn off transmit done interrupts.
		 * This is useful if something is doing polled output.
		 */
		(void) splzs();
		cs->cs_preg[1] &= ~ZSWR1_TIE;
		cs->cs_creg[1] &= ~ZSWR1_TIE;
		ZS_WRITE(cs->cs_zc, 1, cs->cs_creg[1]);
	}
out:
	splx(s);
}

/*
 * Stop output, e.g., for ^S or output flush.
 */
int
zsstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register struct zs_chanstate *cs;
	register int s, unit = DEVUNIT(tp->t_dev);
	struct zs_softc *sc = zs_cd.cd_devs[unit >> 1];

	cs = &sc->sc_cs[unit & 1];
	s = splzs();
	if (tp->t_state & TS_BUSY) {
		/*
		 * Device is transmitting; must stop it.
		 */
		cs->cs_tbc = 0;
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
	return 0;
}

/*
 * Set ZS tty parameters from termios.
 *
 * This routine makes use of the fact that only registers
 * 1, 3, 4, 5, 9, 10, 11, 12, 13, 14, and 15 are written.
 */
static int
zsparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	int unit = DEVUNIT(tp->t_dev);
	struct zs_softc *sc = zs_cd.cd_devs[unit >> 1];
	register struct zs_chanstate *cs = &sc->sc_cs[unit & 1];
	register int tmp, tmp5, cflag, s;

	/*
	 * Because PCLK is only run at 4.9 MHz, the fastest we
	 * can go is 51200 baud (this corresponds to TC=1).
	 * This is somewhat unfortunate as there is no real
	 * reason we should not be able to handle higher rates.
	 */
	tmp = t->c_ospeed;
	if (tmp < 0 || (t->c_ispeed && t->c_ispeed != tmp))
		return (EINVAL);
	if (tmp == 0) {
		/* stty 0 => drop DTR and RTS */
		zs_modem(cs, 0);
		return (0);
	}
	tmp = BPS_TO_TCONST(PCLK / 16, tmp);
#ifdef ALLOW_TC_EQUAL_ZERO
	if (tmp < 0)
#else
	if (tmp < 1)
#endif
		return (EINVAL);

	cflag = t->c_cflag;
	tp->t_ispeed = tp->t_ospeed = TCONST_TO_BPS(PCLK / 16, tmp);
	tp->t_cflag = cflag;

	/*
	 * Block interrupts so that state will not
	 * be altered until we are done setting it up.
	 */
	s = splzs();
	cs->cs_preg[12] = tmp;
	cs->cs_preg[13] = tmp >> 8;
	cs->cs_preg[1] = ZSWR1_RIE | ZSWR1_TIE | ZSWR1_SIE;
	switch (cflag & CSIZE) {
	case CS5:
		tmp = ZSWR3_RX_5;
		tmp5 = ZSWR5_TX_5;
		break;
	case CS6:
		tmp = ZSWR3_RX_6;
		tmp5 = ZSWR5_TX_6;
		break;
	case CS7:
		tmp = ZSWR3_RX_7;
		tmp5 = ZSWR5_TX_7;
		break;
	case CS8:
	default:
		tmp = ZSWR3_RX_8;
		tmp5 = ZSWR5_TX_8;
		break;
	}

	/*
	 * Output hardware flow control on the chip is horrendous: if
	 * carrier detect drops, the receiver is disabled.  Hence we
	 * can only do this when the carrier is on.
	 */
	tmp |= ZSWR3_RX_ENABLE;
	if (cflag & CCTS_OFLOW) {
		if (cs->cs_zc->zc_csr & ZSRR0_DCD)
			tmp |= ZSWR3_HFC;
		ZS_DELAY();
	}
	cs->cs_preg[3] = tmp;
	cs->cs_preg[5] = tmp5 | ZSWR5_TX_ENABLE | ZSWR5_DTR | ZSWR5_RTS;

	tmp = ZSWR4_CLK_X16 | (cflag & CSTOPB ? ZSWR4_TWOSB : ZSWR4_ONESB);
	if ((cflag & PARODD) == 0)
		tmp |= ZSWR4_EVENP;
	if (cflag & PARENB)
		tmp |= ZSWR4_PARENB;
	cs->cs_preg[4] = tmp;
	cs->cs_preg[9] = ZSWR9_MASTER_IE | ZSWR9_NO_VECTOR;
	cs->cs_preg[10] = ZSWR10_NRZ;
	cs->cs_preg[11] = ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD;
	cs->cs_preg[14] = ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA;
	cs->cs_preg[15] = ZSWR15_BREAK_IE | ZSWR15_DCD_IE;

	/*
	 * If nothing is being transmitted, set up new current values,
	 * else mark them as pending.
	 */
	if (cs->cs_heldchange == 0) {
		if (cs->cs_ttyp->t_state & TS_BUSY) {
			cs->cs_heldtbc = cs->cs_tbc;
			cs->cs_tbc = 0;
			cs->cs_heldchange = 1;
		} else {
			bcopy((caddr_t)cs->cs_preg, (caddr_t)cs->cs_creg, 16);
			zs_loadchannelregs(cs->cs_zc, cs->cs_creg);
		}
	}
	splx(s);
	return (0);
}

/*
 * Raise or lower modem control (DTR/RTS) signals.  If a character is
 * in transmission, the change is deferred.
 */
static void
zs_modem(cs, onoff)
	struct zs_chanstate *cs;
	int onoff;
{
	int s, bis, and;

	if (onoff) {
		bis = ZSWR5_DTR | ZSWR5_RTS;
		and = ~0;
	} else {
		bis = 0;
		and = ~(ZSWR5_DTR | ZSWR5_RTS);
	}
	s = splzs();
	cs->cs_preg[5] = (cs->cs_preg[5] | bis) & and;
	if (cs->cs_heldchange == 0) {
		if (cs->cs_ttyp->t_state & TS_BUSY) {
			cs->cs_heldtbc = cs->cs_tbc;
			cs->cs_tbc = 0;
			cs->cs_heldchange = 1;
		} else {
			cs->cs_creg[5] = (cs->cs_creg[5] | bis) & and;
			ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		}
	}
	splx(s);
}

/*
 * Write the given register set to the given zs channel in the proper order.
 * The channel must not be transmitting at the time.  The receiver will
 * be disabled for the time it takes to write all the registers.
 */
static void
zs_loadchannelregs(zc, reg)
	volatile struct zschan *zc;
	u_char *reg;
{
	int i;

	zc->zc_csr = ZSM_RESET_ERR;	/* reset error condition */
	ZS_DELAY();
	i = zc->zc_data;		/* drain fifo */
	ZS_DELAY();
	i = zc->zc_data;
	ZS_DELAY();
	i = zc->zc_data;
	ZS_DELAY();
	ZS_WRITE(zc, 4, reg[4]);
	ZS_WRITE(zc, 10, reg[10]);
	ZS_WRITE(zc, 3, reg[3] & ~ZSWR3_RX_ENABLE);
	ZS_WRITE(zc, 5, reg[5] & ~ZSWR5_TX_ENABLE);
	ZS_WRITE(zc, 1, reg[1]);
	ZS_WRITE(zc, 9, reg[9]);
	ZS_WRITE(zc, 11, reg[11]);
	ZS_WRITE(zc, 12, reg[12]);
	ZS_WRITE(zc, 13, reg[13]);
	ZS_WRITE(zc, 14, reg[14]);
	ZS_WRITE(zc, 15, reg[15]);
	ZS_WRITE(zc, 3, reg[3]);
	ZS_WRITE(zc, 5, reg[5]);
}

static void
tiocm_to_zs(cs, how, val)
	struct zs_chanstate *cs;
	int how, val;
{
	int bits = 0, s;

	if (val & TIOCM_DTR);
		bits |= ZSWR5_DTR;
	if (val & TIOCM_RTS)
		bits |= ZSWR5_RTS;

	s = splzs();
	switch (how) {
		case TIOCMBIC:
			cs->cs_preg[5] &= ~bits;
			break;
		case TIOCMBIS:
			cs->cs_preg[5] |= bits;
			break;
		case TIOCMSET:
			cs->cs_preg[5] &= ~(ZSWR5_RTS | ZSWR5_DTR);
			cs->cs_preg[5] |= bits;
			break;
	}

	if (cs->cs_heldchange == 0) {
		if (cs->cs_ttyp->t_state & TS_BUSY) {
			cs->cs_heldtbc = cs->cs_tbc; 
			cs->cs_tbc = 0;
			cs->cs_heldchange = 1;
		} else {
			cs->cs_creg[5] = cs->cs_preg[5];
			ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		}
	}
	splx(s);

}

#ifdef KGDB
/*
 * Get a character from the given kgdb channel.  Called at splhigh().
 */
static int
zs_kgdb_getc(arg)
	void *arg;
{
	register volatile struct zschan *zc = (volatile struct zschan *)arg;
	u_char c;

	while ((zc->zc_csr & ZSRR0_RX_READY) == 0)
		ZS_DELAY();
	c = zc->zc_data;
	ZS_DELAY();
	return c;
}

/*
 * Put a character to the given kgdb channel.  Called at splhigh().
 */
static void
zs_kgdb_putc(arg, c)
	void *arg;
	int c;
{
	register volatile struct zschan *zc = (volatile struct zschan *)arg;

	while ((zc->zc_csr & ZSRR0_TX_READY) == 0)
		ZS_DELAY();
	zc->zc_data = c;
	ZS_DELAY();
}

/*
 * Set up for kgdb; called at boot time before configuration.
 * KGDB interrupts will be enabled later when zs0 is configured.
 */
void
zs_kgdb_init()
{
	volatile struct zsdevice *addr;
	volatile struct zschan *zc;
	int unit, zs;

	if (major(kgdb_dev) != ZSMAJOR)
		return;
	unit = DEVUNIT(kgdb_dev);
	/*
	 * Unit must be 0 or 1 (zs0).
	 */
	if ((unsigned)unit >= ZS_KBD) {
		printf("zs_kgdb_init: bad minor dev %d\n", unit);
		return;
	}
	zs = unit >> 1;
	if ((addr = zsaddr[zs]) == NULL)
		addr = zsaddr[zs] = (volatile struct zsdevice *)findzs(zs);
	unit &= 1;
	zc = unit == 0 ? &addr->zs_chan[ZS_CHAN_A] : &addr->zs_chan[ZS_CHAN_B];
	zs_kgdb_savedspeed = zs_getspeed(zc);
	printf("zs_kgdb_init: attaching zs%d%c at %d baud\n",
	    zs, unit + 'a', kgdb_rate);
	zs_reset(zc, 1, kgdb_rate);
	kgdb_attach(zs_kgdb_getc, zs_kgdb_putc, (void *)zc);
}
#endif /* KGDB */
