/*	$NetBSD: scc.c,v 1.22 1997/05/25 10:28:22 jonathan Exp $	*/

/* 
 * Copyright (c) 1991,1990,1989,1994,1995,1996 Carnegie Mellon University
 * All rights reserved.
 * 
 * Author: Chris G. Demetriou and Jonathan Stone
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*-
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
 *	@(#)scc.c	8.2 (Berkeley) 11/30/93
 */


/*
 * Intel 82530 dual usart chip driver. Supports the serial port(s) on the
 * Personal DECstation 5000/xx and DECstation 5000/1xx, plus the keyboard
 * and mouse on the 5000/1xx. (Don't ask me where the A channel signals
 * are on the 5000/xx.)
 *
 * See: Intel MicroCommunications Handbook, Section 2, pg. 155-173, 1992.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#ifndef pmax
#include <dev/cons.h>
#endif

#include <pmax/include/pmioctl.h>

#include <pmax/dev/pdma.h>
#include <dev/ic/z8530reg.h>
#include <pmax/dev/lk201.h>
#include <pmax/dev/lk201var.h>

#ifdef pmax
#include <machine/cpuregs.h>	/* phys to uncached */
#include <pmax/pmax/cons.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/asic.h>
#include <pmax/dev/sccreg.h>
#include <pmax/tc/sccvar.h>	/* XXX */
#endif

#ifdef alpha
#include <alpha/tc/sccreg.h>
#include <alpha/tc/sccvar.h>
#include <machine/rpb.h>
#include <alpha/tc/ioasicreg.h>
#endif

#include <machine/autoconf.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

#include <machine/conf.h>

#include "rasterconsole.h"

extern void ttrstrt	__P((void *));

#ifdef alpha
#undef	SCCDEV
#define	SCCDEV		15			/* XXX */
#endif

/*
 * rcons glass-tty console (as used on pmax) needs lk-201 ASCII input
 * support from the tty drivers. This is ugly and broken and won't
 * compile on Alphas. 
 */
#if defined (pmax)
#if NRASTERCONSOLE > 0
#define HAVE_RCONS
#endif
extern int pending_remcons;
#endif

/*
 * True iff the console unit is diverted throught this SCC device.
 * (used to just test if cn_tab->cn_getc was sccGetc, but that
 * breaks with the new-style glass-tty framebuffer console input.
 */

#define CONSOLE_ON_UNIT(unit) \
  (major(cn_tab->cn_dev) == SCCDEV && SCCUNIT(cn_tab->cn_dev) == (unit))

#ifdef alpha
#define RASTER_CONSOLE() 1	/* Treat test for cn_screen as true */
#endif

/*
 * Is there a framebuffer console device using this serial driver?
 * XXX used for ugly special-cased console input that should be redone
 * more cleanly.
 */

static inline int
raster_console(void)
{
	return (cn_tab->cn_pri == CN_NORMAL || cn_tab->cn_pri == CN_INTERNAL);
}


#define	SCCUNIT(dev)	(minor(dev) >> 1)
#define	SCCLINE(dev)	(minor(dev) & 0x1)

/* QVSS-compatible in-kernel X input event parser, pointer tracker */
void	(*sccDivertXInput) __P((int cc)); /* X windows keyboard input routine */
void	(*sccMouseEvent) __P((int));	/* X windows mouse motion event routine */
void	(*sccMouseButtons) __P((int));	/* X windows mouse buttons event routine */

#ifdef DEBUG
int	debugChar;
#endif

struct scc_softc {
	struct device sc_dv;
	struct pdma scc_pdma[2];
	struct {
		u_char	wr1;
		u_char	wr3;
		u_char	wr4;
		u_char	wr5;
		u_char	wr14;
	} scc_wreg[2];
	struct tty *scc_tty[2];
	int	scc_softCAR;
	int scc_flags[2];
#define  SCC_CHAN_NEEDSDELAY	0x01	/* sw must delay 1.6us between output*/
#define  SCC_CHAN_NOMODEM	0x02	/* don't touch modem ctl lines (may
					   be left floating or x-wired */
#define  SCC_CHAN_MODEM_CROSSED	0x04	/* modem lines wired to	other channel*/
#define  SCC_CHAN_KBDLINE	0x08	/* XXX special-case keyboard lines */
	int scc_unitflags;	/* flags for both channels, e.g. */
#define	SCC_PREFERRED_CONSOLE	0x01
};

/*
 * BRG formula is:
 *				ClockFrequency
 *	BRGconstant = 	---------------------------  -  2
 *			2 * BaudRate * ClockDivider
 *
 * Speed selections with Pclk=7.3728Mhz, clock x16
 */
struct speedtab sccspeedtab[] = {
	{ 0,		0,	},
	{ 50,		4606,	},
	{ 75,		3070,	},
	{ 110,		2093,	},
	{ 134.5,	1711,	},
	{ 150,		1534,	},
	{ 200,		1150,	},
	{ 300,		766,	},
	{ 600,		382,	},
	{ 1200,		190,	},
	{ 1800,		126,	},
	{ 2400,		94,	},
	{ 4800,		46,	},
	{ 7200,		30,	}, 	/* non-POSIX */
	{ 9600,		22,	},
	{ 14400,	14,	},	/* non-POSIX */
	{ 19200,	10,	},
	{ 28800,	6,	},	/* non-POSIX */
	{ 38400,	4,	},	/* non-POSIX */
	{ 57600,	2,	},	/* non-POSIX */
	{ 76800,	1,	},	/* non-POSIX, doesn't work reliably */
	{ 115200, 	0	},	/* non-POSIX doesn't work reliably */
	{ -1,		-1,	},
};

#if 0
/* speed selections with clock x1 */
{
	{ 0,		0 },
	{ 300,		24574 },
	{ 300,		12286 },
	{ 600,		6142 },
	{ 1200,		3070 },
	{ 2400,		1534 },
	{ 4800,		766 },
	{ 7200,		510 },
	{ 9600,		382 },
	{ 14400,	254 },
	{ 19200,	190 },
	{ 28800,	126 },
	{ 38400,	94 },
	{ 57600,	62, },
	{ 76800,	46, },
	{ 115200,	30 },
	{ 204800,	16 },
	{ 230400,	14 },
}
#endif

#ifndef	PORTSELECTOR
#define	ISPEED	TTYDEF_SPEED
#define	LFLAG	TTYDEF_LFLAG
#else
#define	ISPEED	B4800
#define	LFLAG	(TTYDEF_LFLAG & ~ECHO)
#endif

/* Definition of the driver for autoconfig. */
static int	sccmatch  __P((struct device * parent, void *cfdata,
			       void *aux)); 
static void	sccattach __P((struct device *parent, struct device *self,
			       void *aux)); 

struct cfattach scc_ca = {
	sizeof (struct scc_softc), sccmatch, sccattach,
};

struct cfdriver scc_cd = {
	NULL, "scc", DV_TTY,
};

int		sccGetc __P((dev_t));
void		sccPutc __P((dev_t, int));
void		sccPollc __P((dev_t, int));
int		sccparam __P((struct tty *, struct termios *));
void		sccstart __P((struct tty *));
int		sccmctl __P((dev_t, int, int));
static int	cold_sccparam __P((struct tty *, struct termios *,
				   struct scc_softc *sc));


#ifdef SCC_DEBUG
static void	rr __P((char *, scc_regmap_t *));
#endif

static void	scc_modem_intr __P((dev_t));
static void	sccreset __P((struct scc_softc *));

int		sccintr __P((void *));
#ifdef alpha
void	scc_alphaintr __P((int));
#endif


/*
 * console variables, for using serial console while still cold and
 * autoconfig has not attached the scc device.
 */ 
extern  int cold;
scc_regmap_t *scc_cons_addr = 0;
static struct scc_softc coldcons_softc;
static struct consdev scccons = {
	NULL, NULL, sccGetc, sccPutc, sccPollc, NODEV, 0
};
void scc_consinit __P((dev_t dev, scc_regmap_t *sccaddr));
void scc_oconsinit __P((struct scc_softc *sc, dev_t dev));


/*
 * Set up a given unit as a serial console device.
 * We need console output when cold, and before any device is configured. 
 * Should be callable when cold, to reset the chip and set parameters
 * for a remote (serial) console or kgdb line.
 * XXX
 * As most DECstations only bring out one rs-232 lead from an SCC
 * to the bulkhead, and use the other for mouse and keyboard, we
 * only allow one unit per SCC to be console.
 */
void
scc_consinit(dev, sccaddr)
	dev_t dev;
	scc_regmap_t *sccaddr;
{
	struct scc_softc *sc;
	struct termios cterm;
	struct tty ctty;
	int s;

	/* Save address in case we're cold. */
	if (cold && scc_cons_addr == 0) {
		scc_cons_addr = sccaddr;
		sc = &coldcons_softc;
		coldcons_softc.scc_pdma[0].p_addr = sccaddr;
		coldcons_softc.scc_pdma[1].p_addr = sccaddr;
	} else {
		/* being called from sccattach() to reset console */
		sc = scc_cd.cd_devs[SCCUNIT(dev)];
	}
	
	/* Reset chip. */
	sccreset(sc);
	/* XXX make sure sccreset() called only once for this chip? */

	/* set console-line parameters */
	s = spltty();
	ctty.t_dev = dev;
	scccons.cn_dev = dev;
	cterm.c_cflag = CS8;
#ifdef pmax
	/* XXX -- why on pmax, not on Alpha? */
	cterm.c_cflag  |= CLOCAL;
#endif
	cterm.c_ospeed = cterm.c_ispeed = 9600;
	(void) cold_sccparam(&ctty, &cterm, sc);
	*cn_tab = scccons;
	DELAY(1000);
	splx(s);
}

void
scc_oconsinit(sc, dev)
	struct scc_softc *sc;
	dev_t dev;
{
	struct termios cterm;
	struct tty ctty;
	int s;

	s = spltty();
	ctty.t_dev = dev;
	cterm.c_cflag = CS8;
#ifdef pmax
	/* XXX -- why on pmax, not on Alpha? */
	cterm.c_cflag  |= CLOCAL;
#endif
	cterm.c_ospeed = cterm.c_ispeed = 9600;
	(void) sccparam(&ctty, &cterm);
	DELAY(1000);
	splx(s);
}

/*
 * Test to see if device is present.
 * Return true if found.
 */
int
sccmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct ioasicdev_attach_args *d = aux;
	void *sccaddr;

	if (parent->dv_cfdata->cf_driver != &ioasic_cd) {
#ifdef DIAGNOSTIC
		printf("Cannot attach scc on %s\n", parent->dv_xname);
#endif
		return (0);
	}

	/* Make sure that we're looking for this type of device. */
	if ((strncmp(d->iada_modname, "z8530   ", TC_ROM_LLEN) != 0) &&
	    (strncmp(d->iada_modname, "scc", TC_ROM_LLEN)!= 0))
	    return (0);
	    
	/*
	 * Check user-specified offset against the ioasic offset.
	 * Allow it to be wildcarded.
	 */
	if (cf->cf_loc[0] != -1 &&
	    cf->cf_loc[0] != d->iada_offset)
		return (0);

	/* Get the address, and check it for validity. */
	sccaddr = (void *)d->iada_addr;
#ifdef SPARSE
	sccaddr = (void *)TC_DENSE_TO_SPARSE((tc_addr_t)sccaddr);
#endif

	if (badaddr(sccaddr, 2))
		return (0);

	return (1);
}

#ifdef alpha
/*
 * Enable ioasic SCC interrupts and scc DMA engine interrupts.
 * XXX does not really belong here.
 */
void
scc_alphaintr(onoff)
	int onoff;
{
	if (onoff) {
		*(volatile u_int *)IOASIC_REG_IMSK(ioasic_base) |=
		    IOASIC_INTR_SCC_1 | IOASIC_INTR_SCC_0;
#if !defined(DEC_3000_300) && defined(SCC_DMA)
		*(volatile u_int *)IOASIC_REG_CSR(ioasic_base) |=
		    IOASIC_CSR_DMAEN_T1 | IOASIC_CSR_DMAEN_R1 |
		    IOASIC_CSR_DMAEN_T2 | IOASIC_CSR_DMAEN_R2;
#endif
	} else {
		*(volatile u_int *)IOASIC_REG_IMSK(ioasic_base) &=
		    ~(IOASIC_INTR_SCC_1 | IOASIC_INTR_SCC_0);
#if !defined(DEC_3000_300) && defined(SCC_DMA)
		*(volatile u_int *)IOASIC_REG_CSR(ioasic_base) &=
		    ~(IOASIC_CSR_DMAEN_T1 | IOASIC_CSR_DMAEN_R1 |
		    IOASIC_CSR_DMAEN_T2 | IOASIC_CSR_DMAEN_R2);
#endif
	}
	alpha_mb();
}
#endif /*alpha*/

void
sccattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct scc_softc *sc = (struct scc_softc *)self;
	struct ioasicdev_attach_args *d = aux;
	struct pdma *pdp;
	struct tty *tp;
	void *sccaddr;
	int cntr;
	struct termios cterm;
	struct tty ctty;
	int s;
	extern int cputype;
	int unit, flags;

	unit = sc->sc_dv.dv_unit;
	flags = sc->sc_dv.dv_cfdata->cf_flags;

	/* serial console debugging */
#if defined(DEBUG) && defined(HAVE_RCONS) && 0
	if (CONSOLE_ON_UNIT(unit) && (cn_tab->cn_pri == CN_REMOTE))
		printf("\nattaching scc%d, currently PROM console\n", unit);
#endif /* defined(DEBUG) && defined(HAVE_RCONS)*/

	sccaddr = (void*)MIPS_PHYS_TO_KSEG1(d->iada_addr);
#ifdef SPARSE
	sccaddr = (void *)TC_DENSE_TO_SPARSE((tc_addr_t)sccaddr);
#endif

	/* Register the interrupt handler. */
	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_TTY,
		sccintr, (void *)sc);

	/* serial console debugging */
#if defined(DEBUG) && defined(HAVE_RCONS) && 0 /*XXX*/
	if (CONSOLE_ON_UNIT(unit) && (cn_tab->cn_pri == CN_REMOTE)) {
		DELAY(10000);
		printf("(attached interrupt, delaying)\n");
	}
#endif /* defined(DEBUG) && defined(HAVE_RCONS)*/

	/*
	 * For a remote console, wait a while for previous output to
	 * complete.
	 */
#ifdef pmax
	if (CONSOLE_ON_UNIT(unit) && (cn_tab->cn_pri == CN_REMOTE))
		DELAY(10000);
#endif

#ifdef alpha
	if ((cputype == ST_DEC_3000_500 && sc->sc_dv.dv_unit == 1) ||
	    (cputype == ST_DEC_3000_300 && sc->sc_dv.dv_unit == 0))
		DELAY(10000);
#endif

	pdp = &sc->scc_pdma[0];

	/* init pseudo DMA structures */
	for (cntr = 0; cntr < 2; cntr++) {
		pdp->p_addr = (void *)sccaddr;
		tp = sc->scc_tty[cntr] = ttymalloc();
		if (cputype == DS_MAXINE || cntr == 0)
			tty_attach(tp);	/* XXX */
		pdp->p_arg = (long)tp;
		pdp->p_fcn = (void (*)__P((struct tty*)))0;
		tp->t_dev = (dev_t)((unit << 1) | cntr);
		pdp++;
	}
	/* What's the warning here? Defaulting to softCAR on line 2? */
	sc->scc_softCAR = flags | 0x2;		/* XXX */

	/* reset chip, initialize  register-copies in softc */
	sccreset(sc);

	/*
	 * Special handling for consoles.
	 */
#ifdef pmax
	if (pending_remcons) {
		/*
		 * We were using PROM callbacks for console I/O,
		 * and we just reset the chip under the console.
		 * wire up this driver as console ASAP.
		 */

		/*XXX*/ /* test for correct unit */
		DELAY(10000);

		/*
		 * XXX PROM  and NetBSD unit numbers swapped
		 * on kn03, maybe kmin?
		 * And what about maxine?
		 */
		if (cn_tab->cn_dev == unit && cputype != DS_MAXINE) {
			printf ("\n");
			return;
		}

		/*
		 * If we are using the PROM serial-console routines
		 * as console, now is the time to set up the scc
		 * driver as console.
		 */
		cn_tab = &scccons;
		cn_tab->cn_dev = makedev(SCCDEV,
		    sc->sc_dv.dv_unit == 0 ? SCCCOMM2_PORT : SCCCOMM3_PORT);
#ifdef notyet
		scc_consinit(cn_tab->cn_dev, sccaddr);
#else
		scc_oconsinit(sc, cn_tab->cn_dev);
#endif

		printf(" (In sccattach: cn_dev = 0x%x)", cn_tab->cn_dev);
	 	printf(" (Unit = %d)", unit);
		printf(": console\n");
		pending_remcons = 0;
		/*
		 * XXX We should support configurations where the PROM
		 * console device is a serial console, and a
		 * framebuffer, keyboard, and mouse are present.
		 */
		return;
	}
#endif /* pmax */
#ifdef HAVE_RCONS
	if ((cn_tab->cn_getc == LKgetc)) {
		/* XXX test below may be too inclusive ? */
		/*(1)*/ /*(CONSOLE_ON_UNIT(unit))*/
		if (major(cn_tab->cn_dev) == SCCDEV) {
			if (unit == 1) {
				s = spltty();
				ctty.t_dev = makedev(SCCDEV, SCCKBD_PORT);
				cterm.c_cflag = CS8;
#ifdef pmax
				/* XXX -- why on pmax, not on Alpha? */
				cterm.c_cflag |= CLOCAL;
#endif /* pmax */
				cterm.c_ospeed = cterm.c_ispeed = 4800;
				(void) sccparam(&ctty, &cterm);
				DELAY(10000);
#ifdef notyet
				/*
				 * For some reason doing this hangs the 3min
				 * during booting. Fortunately the keyboard
				 * works ok without it.
				 */
				KBDReset(ctty.t_dev, sccPutc);
#endif /* notyet */
				DELAY(10000);
				splx(s);
			} else if (unit == 0) {
				s = spltty();
				ctty.t_dev = makedev(SCCDEV, SCCMOUSE_PORT);
				cterm.c_cflag = CS8 | PARENB | PARODD;
				cterm.c_ospeed = cterm.c_ispeed = 4800;
				(void) sccparam(&ctty, &cterm);
#ifdef HAVE_RCONS
				DELAY(10000);
				MouseInit(ctty.t_dev, sccPutc, sccGetc);
				DELAY(10000);
#endif
				splx(s);
			}
		}
	} else
#endif /* HAVE_RCONS */
	if (SCCUNIT(cn_tab->cn_dev) == unit)
	{
		/*XXX console initialization used to go here */
	}

#ifdef alpha
	/*
	 * XXX
	 * Unit 1 is the remote console, wire it up now.
	 */
	if ((cputype == ST_DEC_3000_500 && sc->sc_dv.dv_unit == 1) ||
	    (cputype == ST_DEC_3000_300 && sc->sc_dv.dv_unit == 0))
	 {
		cn_tab = &scccons;
		cn_tab->cn_dev = makedev(SCCDEV, sc->sc_dv.dv_unit * 2);

		printf(": console\n");

		/* wire carrier for console. */
		sc->scc_softCAR |= SCCLINE(cn_tab->cn_dev);
	} else
		printf("\n");
#endif /* !alpha */
	printf("\n");
}


/*
 * Reset the chip and the softc state.
 * Resetting  clobbers chip state and copies of registers for both channels.
 * The driver assumes this is only ever called once per unit.
 */
static void
sccreset(sc)
	register struct scc_softc *sc;
{
	register scc_regmap_t *regs;
	register u_char val;

	regs = (scc_regmap_t *)sc->scc_pdma[0].p_addr;
	/*
	 * Chip once-only initialization
	 *
	 * NOTE: The wiring we assume is the one on the 3min:
	 *
	 *	out	A-TxD	-->	TxD	keybd or mouse
	 *	in	A-RxD	-->	RxD	keybd or mouse
	 *	out	A-DTR~	-->	DTR	comm
	 *	out	A-RTS~	-->	RTS	comm
	 *	in	A-CTS~	-->	SI	comm
	 *	in	A-DCD~	-->	RI	comm
	 *	in	A-SYNCH~-->	DSR	comm
	 *	out	B-TxD	-->	TxD	comm
	 *	in	B-RxD	-->	RxD	comm
	 *	in	B-RxC	-->	TRxCB	comm
	 *	in	B-TxC	-->	RTxCB	comm
	 *	out	B-RTS~	-->	SS	comm
	 *	in	B-CTS~	-->	CTS	comm
	 *	in	B-DCD~	-->	CD	comm
	 */
	SCC_INIT_REG(regs, SCC_CHANNEL_A);
	SCC_INIT_REG(regs, SCC_CHANNEL_B);

	SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR9, ZSWR9_HARD_RESET);
	DELAY(50000);	/*enough ? */
	SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR9, 0);

	/* program the interrupt vector */
	SCC_WRITE_REG(regs, SCC_CHANNEL_A, ZSWR_IVEC, 0xf0);
	SCC_WRITE_REG(regs, SCC_CHANNEL_B, ZSWR_IVEC, 0xf0);
	SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR9, ZSWR9_VECTOR_INCL_STAT);

	/*
	 * Set softc copies of writable (write-only?) registers.
	 */

	/* receive parameters and control */
	sc->scc_wreg[SCC_CHANNEL_A].wr3 = 0;
	sc->scc_wreg[SCC_CHANNEL_B].wr3 = 0;

	/* timing base defaults */
	sc->scc_wreg[SCC_CHANNEL_A].wr4 = ZSWR4_CLK_X16;
	sc->scc_wreg[SCC_CHANNEL_B].wr4 = ZSWR4_CLK_X16	;

	/* enable DTR, RTS and SS */
#ifdef alpha
	/* XXX -- who changed the alpha driver to do this, and why? */
	sc->scc_wreg[SCC_CHANNEL_B].wr5 = 0;
#else
	sc->scc_wreg[SCC_CHANNEL_B].wr5 = ZSWR5_RTS;
#endif
	sc->scc_wreg[SCC_CHANNEL_A].wr5 = ZSWR5_RTS | ZSWR5_DTR;

	/* baud rates */
	val = ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK;
	sc->scc_wreg[SCC_CHANNEL_B].wr14 = val;
	sc->scc_wreg[SCC_CHANNEL_A].wr14 = val;

	/* interrupt conditions */
	val =	ZSWR1_RIE | ZSWR1_PE_SC | ZSWR1_SIE | ZSWR1_TIE;
	sc->scc_wreg[SCC_CHANNEL_A].wr1 = val;
	sc->scc_wreg[SCC_CHANNEL_B].wr1 = val;
}

int
sccopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct scc_softc *sc;
	register struct tty *tp;
	register int unit, line;
	int s, error = 0;

	unit = SCCUNIT(dev);
	if (unit >= scc_cd.cd_ndevs)
		return (ENXIO);
	sc = scc_cd.cd_devs[unit];
	if (!sc)
		return (ENXIO);

	line = SCCLINE(dev);
	if (sc->scc_pdma[line].p_addr == NULL)
		return (ENXIO);
	tp = sc->scc_tty[line];
	if (tp == NULL) {
		tp = sc->scc_tty[line] = ttymalloc();
		tty_attach(tp);
	}
	tp->t_oproc = sccstart;
	tp->t_param = sccparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
#ifndef PORTSELECTOR
		if (tp->t_ispeed == 0) {
#endif
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = LFLAG;
			tp->t_ispeed = tp->t_ospeed = ISPEED;
#ifdef PORTSELECTOR
			tp->t_cflag |= HUPCL;
#else
		}
#endif
		(void) sccparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) && curproc->p_ucred->cr_uid != 0)
		return (EBUSY);
	(void) sccmctl(dev, DML_DTR, DMSET);
	s = spltty();
	while (!(flag & O_NONBLOCK) && !(tp->t_cflag & CLOCAL) &&
	       !(tp->t_state & TS_CARR_ON)) {
		tp->t_state |= TS_WOPEN;
		if ((error = ttysleep(tp, (caddr_t)&tp->t_rawq, TTIPRI | PCATCH,
		    ttopen, 0)) != 0)
			break;
	}
	splx(s);
	if (error)
		return (error);
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

/*ARGSUSED*/
int
sccclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct scc_softc *sc = scc_cd.cd_devs[SCCUNIT(dev)];
	register struct tty *tp;
	register int line;

	line = SCCLINE(dev);
	tp = sc->scc_tty[line];
	if (sc->scc_wreg[line].wr5 & ZSWR5_BREAK) {
		sc->scc_wreg[line].wr5 &= ~ZSWR5_BREAK;
		ttyoutput(0, tp);
	}
	(*linesw[tp->t_line].l_close)(tp, flag);
	if ((tp->t_cflag & HUPCL) || (tp->t_state & TS_WOPEN) ||
	    !(tp->t_state & TS_ISOPEN))
		(void) sccmctl(dev, 0, DMSET);
	return (ttyclose(tp));
}

int
sccread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct scc_softc *sc;
	register struct tty *tp;

	sc = scc_cd.cd_devs[SCCUNIT(dev)];		/* XXX*/
	tp = sc->scc_tty[SCCLINE(dev)];
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
sccwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct scc_softc *sc;
	register struct tty *tp;

	sc = scc_cd.cd_devs[SCCUNIT(dev)];	/* XXX*/
	tp = sc->scc_tty[SCCLINE(dev)];
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
scctty(dev)
        dev_t dev;
{
	register struct scc_softc *sc;
	register struct tty *tp;
	register int unit = SCCUNIT(dev);

	if ((unit >= scc_cd.cd_ndevs) || (sc = scc_cd.cd_devs[unit]) == 0)
		return (0);
	tp = sc->scc_tty[SCCLINE(dev)];
        return (tp);
}

/*ARGSUSED*/
int
sccioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct scc_softc *sc;
	register struct tty *tp;
	int error, line;

	line = SCCLINE(dev);
	sc = scc_cd.cd_devs[SCCUNIT(dev)];
	tp = sc->scc_tty[line];
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		sc->scc_wreg[line].wr5 |= ZSWR5_BREAK;
		ttyoutput(0, tp);
		break;

	case TIOCCBRK:
		sc->scc_wreg[line].wr5 &= ~ZSWR5_BREAK;
		ttyoutput(0, tp);
		break;

	case TIOCSDTR:
		(void) sccmctl(dev, DML_DTR|DML_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) sccmctl(dev, DML_DTR|DML_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) sccmctl(dev, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) sccmctl(dev, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) sccmctl(dev, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = sccmctl(dev, 0, DMGET);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Set line parameters --  tty t_param entry point.
 */
int
sccparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register struct scc_softc *sc;

	/* Extract the softc and call cold_sccparam to do all the work. */
	sc = scc_cd.cd_devs[SCCUNIT(tp->t_dev)];
	return cold_sccparam(tp, t, sc);
}


/* 
 * Do what sccparam() (t_param entry point) does, but callable when cold. 
 */
static int
cold_sccparam(tp, t, sc)
	register struct tty *tp;
	register struct termios *t;
	register struct scc_softc *sc;
{
	register scc_regmap_t *regs;
	register int line;
	register u_char value, wvalue;
	register int cflag = t->c_cflag;
	int ospeed;

        /* Check arguments */
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
                return (EINVAL);
	ospeed = ttspeedtab(t->c_ospeed, sccspeedtab);
        if (ospeed < 0)
                return (EINVAL);
        /* and copy to tty */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	/*
	 * Handle console specially.
	 */
#ifdef HAVE_RCONS
	if (cn_tab->cn_getc == LKgetc) {
		if (minor(tp->t_dev) == SCCKBD_PORT) {
			cflag = CS8;
			ospeed = ttspeedtab(4800, sccspeedtab);
		} else if (minor(tp->t_dev) == SCCMOUSE_PORT) {
			cflag = CS8 | PARENB | PARODD;
			ospeed = ttspeedtab(4800, sccspeedtab);
		}
	} else if (tp->t_dev == cn_tab->cn_dev)
#endif /*HAVE_RCONS*/
	{
		cflag = CS8;
		ospeed = ttspeedtab(9600, sccspeedtab);
	}
	if (ospeed == 0) {
		(void) sccmctl(tp->t_dev, 0, DMSET);	/* hang up line */
		return (0);
	}

	line = SCCLINE(tp->t_dev);
	regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;

	/*
	 * pmax driver used to reset the SCC here. That reset causes the
	 * other channel on the SCC to drop outpur chars: at least that's
	 * what CGD reports for the Alpha.  It's a bug.
	 */
#if 0
	/* reset line */
	if (line == SCC_CHANNEL_A)
		value = ZSWR9_A_RESET;
	else
		value = ZSWR9_B_RESET;
	SCC_WRITE_REG(regs, line, SCC_WR9, value);
	DELAY(25);
#endif

	/* stop bits, normally 1 */
	value = sc->scc_wreg[line].wr4 & 0xf0;
	if (cflag & CSTOPB)
		value |= ZSWR4_TWOSB;
	else
		value |= ZSWR4_ONESB;
	if ((cflag & PARODD) == 0)
		value |= ZSWR4_EVENP;
	if (cflag & PARENB)
		value |= ZSWR4_PARENB;

	/* set it now, remember it must be first after reset */
	sc->scc_wreg[line].wr4 = value;
	SCC_WRITE_REG(regs, line, SCC_WR4, value);

	/* vector again */
	SCC_WRITE_REG(regs, line, ZSWR_IVEC, 0xf0);

	/* clear break, keep rts dtr */
	wvalue = sc->scc_wreg[line].wr5 & (ZSWR5_DTR|ZSWR5_RTS);
	switch (cflag & CSIZE) {
	case CS5:
		value = ZSWR3_RX_5;
		wvalue |= ZSWR5_TX_5;
		break;
	case CS6:
		value = ZSWR3_RX_6;
		wvalue |= ZSWR5_TX_6;
		break;
	case CS7:
		value = ZSWR3_RX_7;
		wvalue |= ZSWR5_TX_7;
		break;
	case CS8:
	default:
		value = ZSWR3_RX_8;
		wvalue |= ZSWR5_TX_8;
	};
	sc->scc_wreg[line].wr3 = value;
	SCC_WRITE_REG(regs, line, SCC_WR3, value);

	sc->scc_wreg[line].wr5 = wvalue;
	SCC_WRITE_REG(regs, line, SCC_WR5, wvalue);

	/*
	 * XXX Does the SCC chip require us to refresh the WR5 register
	 * for the other channel after writing the other, or not?
	 */
#ifdef notdef
	/* XXX */
	{
	int otherline = (line + 1) & 1;
	SCC_WRITE_REG(regs, otherline, SCC_WR5, sc->scc_wreg[otherline].wr5);
	}
#endif

	SCC_WRITE_REG(regs, line, ZSWR_SYNCLO, 0);
	SCC_WRITE_REG(regs, line, ZSWR_SYNCHI, 0);
	SCC_WRITE_REG(regs, line, SCC_WR9, ZSWR9_VECTOR_INCL_STAT);
	SCC_WRITE_REG(regs, line, SCC_WR10, 0);
	value = ZSWR11_RXCLK_BAUD | ZSWR11_TXCLK_BAUD |
		ZSWR11_TRXC_OUT_ENA | ZSWR11_TRXC_BAUD;
	SCC_WRITE_REG(regs, line, SCC_WR11, value);
	SCC_SET_TIMING_BASE(regs, line, ospeed);
	value = sc->scc_wreg[line].wr14;
	SCC_WRITE_REG(regs, line, SCC_WR14, value);

#ifdef alpha
	if (SCCUNIT(tp->t_dev) == 1) {
		/* On unit one, on the flamingo, modem control is floating! */
		value = ZSWR15_BREAK_IE;
	} else
#endif
	{
		value = ZSWR15_BREAK_IE | ZSWR15_CTS_IE | ZSWR15_DCD_IE;
	}
	SCC_WRITE_REG(regs, line, SCC_WR15, value);

	/* and now the enables */
	value = sc->scc_wreg[line].wr3 | ZSWR3_RX_ENABLE;
	SCC_WRITE_REG(regs, line, SCC_WR3, value);
	value = sc->scc_wreg[line].wr5 | ZSWR5_TX_ENABLE;
	sc->scc_wreg[line].wr5 = value;
	SCC_WRITE_REG(regs, line, SCC_WR5, value);

	/* master inter enable */
	value = ZSWR9_MASTER_IE | ZSWR9_VECTOR_INCL_STAT;
	SCC_WRITE_REG(regs, line, SCC_WR9, value);
	SCC_WRITE_REG(regs, line, SCC_WR1, sc->scc_wreg[line].wr1);
	tc_mb();

#ifdef	alpha
	scc_alphaintr(1);			/* XXX XXX XXX */
#endif	/*alpha*/

	return (0);
}


/*
 * Check for interrupts from all devices.
 */
int
sccintr(xxxsc)
	void *xxxsc;
{
	register struct scc_softc *sc = (struct scc_softc *)xxxsc;
	register int unit = (long)sc->sc_dv.dv_unit;
	register scc_regmap_t *regs;
	register struct tty *tp;
	register struct pdma *dp;
	register int cc, chan, rr1, rr2, rr3;
	int overrun = 0;

	rr1 = 0;		/* shut up gcc -Wall */
	regs = (scc_regmap_t *)sc->scc_pdma[0].p_addr;
	unit <<= 1;
	for (;;) {
	    SCC_READ_REG(regs, SCC_CHANNEL_B, ZSRR_IVEC, rr2);
	    rr2 = SCC_RR2_STATUS(rr2);
	    /* are we done yet ? */
	    if (rr2 == 6) {	/* strange, distinguished value */
		SCC_READ_REG(regs, SCC_CHANNEL_A, ZSRR_IPEND, rr3);
		if (rr3 == 0)
			return 0 ;/* XXX FIXME why ? */
	    }

	    SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_RR0, ZSWR0_CLR_INTR);
	    if ((rr2 == SCC_RR2_A_XMIT_DONE) || (rr2 == SCC_RR2_B_XMIT_DONE)) {
		chan = (rr2 == SCC_RR2_A_XMIT_DONE) ?
			SCC_CHANNEL_A : SCC_CHANNEL_B;
		tp = sc->scc_tty[chan];
		dp = &sc->scc_pdma[chan];
		if (dp->p_mem < dp->p_end) {
			SCC_WRITE_DATA(regs, chan, *dp->p_mem++);
#ifdef pmax	/* Alpha handles the 1.6 msec settle time in hardware */
			DELAY(2);
#endif
			tc_mb();
		} else {
			tp->t_state &= ~TS_BUSY;
			if (tp->t_state & TS_FLUSH)
				tp->t_state &= ~TS_FLUSH;
			else {
				ndflush(&tp->t_outq, dp->p_mem -
					(caddr_t) tp->t_outq.c_cf);
				dp->p_end = dp->p_mem = tp->t_outq.c_cf;
			}
			if (tp->t_line)
				(*linesw[tp->t_line].l_start)(tp);
			else
				sccstart(tp);
			if (tp->t_outq.c_cc == 0 || !(tp->t_state & TS_BUSY)) {
				SCC_READ_REG(regs, chan, SCC_RR15, cc);
				cc &= ~ZSWR15_TXUEOM_IE;
				SCC_WRITE_REG(regs, chan, SCC_WR15, cc);
				cc = sc->scc_wreg[chan].wr1 & ~ZSWR1_TIE;
				SCC_WRITE_REG(regs, chan, SCC_WR1, cc);
				sc->scc_wreg[chan].wr1 = cc;
				tc_mb();
			}
		}
	    } else if (rr2 == SCC_RR2_A_RECV_DONE ||
		rr2 == SCC_RR2_B_RECV_DONE || rr2 == SCC_RR2_A_RECV_SPECIAL ||
		rr2 == SCC_RR2_B_RECV_SPECIAL) {
		if (rr2 == SCC_RR2_A_RECV_DONE || rr2 == SCC_RR2_A_RECV_SPECIAL)
			chan = SCC_CHANNEL_A;
		else
			chan = SCC_CHANNEL_B;
		tp = sc->scc_tty[chan];
		SCC_READ_DATA(regs, chan, cc);
		if (rr2 == SCC_RR2_A_RECV_SPECIAL ||
			rr2 == SCC_RR2_B_RECV_SPECIAL) {
			SCC_READ_REG(regs, chan, SCC_RR1, rr1);
			SCC_WRITE_REG(regs, chan, SCC_RR0, ZSWR0_RESET_ERRORS);
			if ((rr1 & ZSRR1_DO) && overrun == 0) {
				log(LOG_WARNING, "scc%d,%d: silo overflow\n",
					unit >> 1, chan);
				overrun = 1;
			}
		}

		/*
		 * Keyboard needs special treatment.
		 */
		if (tp == scctty(makedev(SCCDEV, SCCKBD_PORT)) && 
		    raster_console()) {
#ifdef KADB
			if (cc == LK_DO) {
				spl0();
				kdbpanic();
				return -1;
			}
#endif
#ifdef DEBUG
			debugChar = cc;
#endif
			if (sccDivertXInput) {
				(*sccDivertXInput)(cc);
				continue;
			}
#ifdef HAVE_RCONS
			if ((cc = kbdMapChar(cc)) < 0)
				continue;
#endif
		/*
		 * Now for mousey
		 */
		} else if (tp == scctty(makedev(SCCDEV, SCCMOUSE_PORT)) &&
			   sccMouseButtons) {
#ifdef HAVE_RCONS
			/*XXX*/
			mouseInput(cc);
			continue;
#endif
		}
		if (!(tp->t_state & TS_ISOPEN)) {
			wakeup((caddr_t)&tp->t_rawq);
#ifdef PORTSELECTOR
			if (!(tp->t_state & TS_WOPEN))
#endif
				continue;
		}
		if (rr2 == SCC_RR2_A_RECV_SPECIAL ||
			rr2 == SCC_RR2_B_RECV_SPECIAL) {
			if (rr1 & ZSRR1_PE)
				cc |= TTY_PE;
			if (rr1 & ZSRR1_FE)
				cc |= TTY_FE;
		}
		(*linesw[tp->t_line].l_rint)(cc, tp);
	    } else if ((rr2 == SCC_RR2_A_EXT_STATUS) || (rr2 == SCC_RR2_B_EXT_STATUS)) {
		chan = (rr2 == SCC_RR2_A_EXT_STATUS) ?
			SCC_CHANNEL_A : SCC_CHANNEL_B;
		SCC_WRITE_REG(regs, chan, SCC_RR0, ZSWR0_RESET_STATUS);
		scc_modem_intr(unit | chan);
	    }
	}
}

void
sccstart(tp)
	register struct tty *tp;
{
	register struct pdma *dp;
	register scc_regmap_t *regs;
	register struct scc_softc *sc;
	register int cc, chan;
	u_char temp;
	int s, sendone;

	sc = scc_cd.cd_devs[SCCUNIT(tp->t_dev)];
	dp = &sc->scc_pdma[SCCLINE(tp->t_dev)];
	regs = (scc_regmap_t *)dp->p_addr;
	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	if (tp->t_outq.c_cc == 0)
		goto out;
	/* handle console specially */
	if (tp == scctty(makedev(SCCDEV,SCCKBD_PORT)) && raster_console()) {
		while (tp->t_outq.c_cc > 0) {
			cc = getc(&tp->t_outq) & 0x7f;
			cnputc(cc);
		}
		/*
		 * After we flush the output queue we may need to wake
		 * up the process that made the output.
		 */
		if (tp->t_outq.c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t)&tp->t_outq);
			}
			selwakeup(&tp->t_wsel);
		}
		goto out;
	}
	cc = ndqb(&tp->t_outq, 0);

	tp->t_state |= TS_BUSY;
	dp->p_end = dp->p_mem = tp->t_outq.c_cf;
	dp->p_end += cc;

	/*
	 * Enable transmission and send the first char, as required.
	 */
	chan = SCCLINE(tp->t_dev);
	SCC_READ_REG(regs, chan, SCC_RR0, temp);
	sendone = (temp & ZSRR0_TX_READY);
	SCC_READ_REG(regs, chan, SCC_RR15, temp);
	temp |= ZSWR15_TXUEOM_IE;
	SCC_WRITE_REG(regs, chan, SCC_WR15, temp);
	temp = sc->scc_wreg[chan].wr1 | ZSWR1_TIE;
	SCC_WRITE_REG(regs, chan, SCC_WR1, temp);
	sc->scc_wreg[chan].wr1 = temp;
	if (sendone) {
#ifdef DIAGNOSTIC
		if (cc == 0)
			panic("sccstart: No chars");
#endif 
		SCC_WRITE_DATA(regs, chan, *dp->p_mem++);
#ifdef pmax /* Alpha handles the 1.6 msec settle time in hardware */
		DELAY(2);
#endif
	}
	tc_mb();
out:
	splx(s);
}

/*
 * Stop output on a line.
 */
/*ARGSUSED*/
int /* TTTTT was void */
sccstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register struct pdma *dp;
	register struct scc_softc *sc;
	register int s;

	sc = scc_cd.cd_devs[SCCUNIT(tp->t_dev)];
	dp = &sc->scc_pdma[SCCLINE(tp->t_dev)];
	s = spltty();
	if (tp->t_state & TS_BUSY) {
		dp->p_end = dp->p_mem;
		if (!(tp->t_state & TS_TTSTOP))
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

int
sccmctl(dev, bits, how)
	dev_t dev;
	int bits, how;
{
	register struct scc_softc *sc;
	register scc_regmap_t *regs;
	register int line, mbits;
	register u_char value;
	int s;

	sc = scc_cd.cd_devs[SCCUNIT(dev)];
	line = SCCLINE(dev);
	regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;
	s = spltty();
	/*
	 * only channel B has modem control, however the DTR and RTS
	 * pins on the comm port are wired to the DTR and RTS A channel
	 * signals.
	 */
	mbits = DML_DTR | DML_DSR | DML_CAR;
	if (line == SCC_CHANNEL_B) {
		if (sc->scc_wreg[SCC_CHANNEL_A].wr5 & ZSWR5_DTR)
			mbits = DML_DTR | DML_DSR;
		else
			mbits = 0;
		SCC_READ_REG_ZERO(regs, SCC_CHANNEL_B, value);
		if (value & ZSRR0_DCD)
			mbits |= DML_CAR;
	}
	switch (how) {
	case DMSET:
		mbits = bits;
		break;

	case DMBIS:
		mbits |= bits;
		break;

	case DMBIC:
		mbits &= ~bits;
		break;

	case DMGET:
		(void) splx(s);
		return (mbits);
	}
	if (line == SCC_CHANNEL_B) {
		if (mbits & DML_DTR)
			sc->scc_wreg[SCC_CHANNEL_A].wr5 |= ZSWR5_DTR;
		else
			sc->scc_wreg[SCC_CHANNEL_A].wr5 &= ~ZSWR5_DTR;
		SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR5,
			sc->scc_wreg[SCC_CHANNEL_A].wr5);
	}
	if ((mbits & DML_DTR) || (sc->scc_softCAR & (1 << line)))
		sc->scc_tty[line]->t_state |= TS_CARR_ON;
	(void) splx(s);
	return (mbits);
}

/*
 * Check for carrier transition.
 */
static void
scc_modem_intr(dev)
	dev_t dev;
{
	register scc_regmap_t *regs;
	register struct scc_softc *sc;
	register struct tty *tp;
	register int car, chan;
	register u_char value;
	int s;

	chan = SCCLINE(dev);
	sc = scc_cd.cd_devs[SCCUNIT(dev)];
	tp = sc->scc_tty[chan];
	regs = (scc_regmap_t *)sc->scc_pdma[chan].p_addr;
	if (chan == SCC_CHANNEL_A)
		return;
	s = spltty();
	if (sc->scc_softCAR & (1 << chan))
		car = 1;
	else {
		SCC_READ_REG_ZERO(regs, chan, value);
		car = value & ZSRR0_DCD;
	}

	/*
	 * The pmax driver follows carrier-detect. The Alpha does not.
	 * XXX Why doesn't the Alpha driver follow carrier-detect?
	 * (in the Alpha driver, this is an "#ifdef notdef").
	 * Is it related to  console handling?
	 */
#ifndef alpha
	if (car) {
		/* carrier present */
		if (!(tp->t_state & TS_CARR_ON))
			(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	} else if (tp->t_state & TS_CARR_ON)
		(void)(*linesw[tp->t_line].l_modem)(tp, 0);
#endif /* !alpha */
	splx(s);
}

/*
 * Get a char off the appropriate line via. a busy wait loop.
 */
int
sccGetc(dev)
	dev_t dev;
{
	register scc_regmap_t *regs;
	register int c, line;
	register u_char value;
	int s;

	line = SCCLINE(dev);
	if (cold && scc_cons_addr) {
		regs = scc_cons_addr;
	} else {
		register struct scc_softc *sc;
		sc = scc_cd.cd_devs[SCCUNIT(dev)];
		regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;
	}

	if (!regs)
		return (0);
#ifdef pmax
	/*s = spltty(); */	/* XXX  why different spls? */
	s = splhigh();
#else
	s = splhigh();
#endif
	for (;;) {
		SCC_READ_REG(regs, line, SCC_RR0, value);
		if (value & ZSRR0_RX_READY) {
			SCC_READ_REG(regs, line, SCC_RR1, value);
			SCC_READ_DATA(regs, line, c);
			if (value & (ZSRR1_PE | ZSRR1_DO | ZSRR1_FE)) {
				SCC_WRITE_REG(regs, line, SCC_WR0,
					ZSWR0_RESET_ERRORS);
				SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR0,
					ZSWR0_CLR_INTR);
			} else {
				SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR0,
					ZSWR0_CLR_INTR);
				splx(s);
				return (c & 0xff);
			}
		} else
			DELAY(10);
	}
}

/*
 * Send a char on a port, via a busy wait loop.
 */
void
sccPutc(dev, c)
	dev_t dev;
	int c;
{
	register scc_regmap_t *regs;
	register int line;
	register u_char value;
	int s;

#ifdef pmax
	s = spltty();	/* XXX  why different spls? */
#else
	s = splhigh();
#endif
	line = SCCLINE(dev);
	if (cold && scc_cons_addr) {
		regs = scc_cons_addr;
	} else {
		register struct scc_softc *sc;
		sc = scc_cd.cd_devs[SCCUNIT(dev)];
		regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;
	}

	/*
	 * Wait for transmitter to be not busy.
	 */
	do {
		SCC_READ_REG(regs, line, SCC_RR0, value);
		if (value & ZSRR0_TX_READY)
			break;
		DELAY(100);
	} while (1);

	/*
	 * Send the char.
	 */
	SCC_WRITE_DATA(regs, line, c);
	tc_mb();
	splx(s);

	return;
}

/*
 * Enable/disable polling mode
 */
void
sccPollc(dev, on)
	dev_t dev;
	int on;
{
}

#ifdef	SCC_DEBUG
static void
rr(msg, regs)
	char *msg;
	scc_regmap_t *regs;
{
	u_char value;
	int r0, r1, r2, r3, r10, r15;

	printf("%s: register: %lx\n", msg, regs);
#define	L(reg, r) {							\
	SCC_READ_REG(regs, SCC_CHANNEL_A, reg, value);			\
	r = value;							\
}
	L(SCC_RR0, r0);
	L(SCC_RR1, r1);
	L(ZSRR_IVEC, r2);
	L(ZSRR_IPEND, r3);
	L(SCC_RR10, r10);
	L(SCC_RR15, r15);
	printf("A: 0: %x  1: %x    2(vec): %x  3: %x  10: %x  15: %x\n",
	    r0, r1, r2, r3, r10, r15);
#undef L
#define	L(reg, r) {							\
	SCC_READ_REG(regs, SCC_CHANNEL_B, reg, value);			\
	r = value;							\
}
	L(SCC_RR0, r0);
	L(SCC_RR1, r1);
	L(ZSRR_IVEC, r2);
	L(SCC_RR10, r10);
	L(SCC_RR15, r15);
	printf("B: 0: %x  1: %x  2(state): %x        10: %x  15: %x\n",
	    r0, r1, r2, r10, r15);
}
#endif /* SCC_DEBUG */
