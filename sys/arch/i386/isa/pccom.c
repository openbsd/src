/*	$OpenBSD: pccom.c,v 1.28 1999/01/21 08:55:08 niklas Exp $	*/
/*	$NetBSD: com.c,v 1.82.4.1 1996/06/02 09:08:00 mrg Exp $	*/

/*
 * Copyright (c) 1997 - 1998, Jason Downs.  All rights reserved.
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
 *      This product includes software developed by Jason Downs for the
 *      OpenBSD system.
 * 4. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Charles M. Hannum.  All rights reserved.
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
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

/*
 * PCCOM driver, uses National Semiconductor NS16450/NS16550AF UART
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/cons.h>
#include <dev/isa/isavar.h>
#include <dev/ic/comreg.h>
#include <dev/ic/ns16550reg.h>
#ifdef COM_HAYESP
#include <dev/ic/hayespreg.h>
#endif
#define	com_lcr	com_cfcr
#ifdef DDB
#include <ddb/db_var.h>
#endif

#include "pccomvar.h"
#include "pccom.h"

/* XXX: These belong elsewhere */
cdev_decl(com);
bdev_decl(com);

static u_char tiocm_xxx2mcr __P((int));

void pccom_xr16850_fifo_init __P((bus_space_tag_t, bus_space_handle_t));

/*
 * XXX the following two cfattach structs should be different, and possibly
 * XXX elsewhere.
 */
int	comprobe __P((struct device *, void *, void *));
void	comattach __P((struct device *, struct device *, void *));
void	com_absent_notify __P((struct com_softc *sc));
void	comstart_pending __P((void *));

#if NPCCOM_ISA
struct cfattach pccom_isa_ca = {
	sizeof(struct com_softc), comprobe, comattach
};
#endif

#if NPCCOM_ISAPNP
struct cfattach pccom_isapnp_ca = {
	sizeof(struct com_softc), comprobe, comattach
};
#endif

#if NPCCOM_COMMULTI
struct cfattach pccom_commulti_ca = {
	sizeof(struct com_softc), comprobe, comattach
};
#endif

struct cfdriver pccom_cd = {
	NULL, "pccom", DV_TTY
};

void cominit __P((bus_space_tag_t, bus_space_handle_t, int));

#ifndef CONSPEED
#define	CONSPEED B9600
#endif

#if defined(COMCONSOLE) || defined(PCCOMCONSOLE)
int	comdefaultrate = CONSPEED;		/* XXX why set default? */
#else
int	comdefaultrate = TTYDEF_SPEED;
#endif
int	comconsaddr;
int	comconsinit;
int	comconsattached;
bus_space_tag_t comconsiot;
bus_space_handle_t comconsioh;
tcflag_t comconscflag = TTYDEF_CFLAG;

int	commajor;
int	comsopen = 0;
int	comevents = 0;

#ifdef KGDB
#include <machine/remote-sl.h>
extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	DEVUNIT(x)	(minor(x) & 0x7f)
#define	DEVCUA(x)	(minor(x) & 0x80)

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

/* Macros for determining bus type. */
#if NPCCOM_ISA || NPCCOM_PCMCIA
#define IS_ISA(parent) \
	(!strcmp((parent)->dv_cfdata->cf_driver->cd_name, "isa") || \
	 !strcmp((parent)->dv_cfdata->cf_driver->cd_name, "pcmcia"))
#elif NPCCOM_ISA
#define IS_ISA(parent) \
	!strcmp((parent)->dv_cfdata->cf_driver->cd_name, "isa")
#elif NPCCOM_ISAPNP
#define IS_ISA(parent) 0
#endif

#if NPCCOM_ISAPNP
#define IS_ISAPNP(parent) \
	 !strcmp((parent)->dv_cfdata->cf_driver->cd_name, "isapnp")
#else
#define IS_ISAPNP(parent)	0
#endif

#if 0
#if NPCCOM_PCMCIA
#include <dev/pcmcia/pcmciavar.h>

int	com_pcmcia_match __P((struct device *, void *, void *));
void	com_pcmcia_attach __P((struct device *, struct device *, void *));
int	com_pcmcia_detach __P((struct device *));

struct cfattach pccom_pcmcia_ca = {
	sizeof(struct com_softc), com_pcmcia_match, comattach,
	com_pcmcia_detach
};

int	com_pcmcia_mod __P((struct pcmcia_link *pc_link, struct device *self,
	    struct pcmcia_conf *pc_cf, struct cfdata *cf));

/* additional setup needed for pcmcia devices */
/* modify config entry */
int 
com_pcmcia_mod(pc_link, self, pc_cf, cf)
    struct pcmcia_link *pc_link;
    struct device *self;
    struct pcmcia_conf *pc_cf; 
    struct cfdata *cf;
{               
    int err; 
    if (!(err = PCMCIA_BUS_CONFIG(pc_link->adapter, pc_link, self,
				  pc_cf, cf))) {
        pc_cf->memwin = 0;
	if (pc_cf->cfgtype == 0) 
	    pc_cf->cfgtype = CFGENTRYID; /* determine from ioaddr */
    }
    return err;
}

int com_pcmcia_isa_attach __P((struct device *, void *, void *,
			       struct pcmcia_link *));
int com_pcmcia_remove __P((struct pcmcia_link *, struct device *));

static struct pcmcia_com {
    struct pcmcia_device pcd;
} pcmcia_com =  {
    {"PCMCIA Modem card", com_pcmcia_mod, com_pcmcia_isa_attach,
     NULL, com_pcmcia_remove}
};          


struct pcmciadevs pcmcia_com_devs[] = {
  { "pccom", 0,
  NULL, "*MODEM*", NULL, NULL,
  NULL, (void *)&pcmcia_com 
  },
  { "pccom", 0,
  NULL, "*Modem*", NULL, NULL,
  NULL, (void *)&pcmcia_com   
  },
  { "pccom", 0,
  NULL, "*modem*", NULL, NULL,
  NULL, (void *)&pcmcia_com   
  },
  { "pccom", 0,
  NULL, NULL, "*MODEM*", NULL,
  NULL, (void *)&pcmcia_com 
  },
  { "pccom", 0,
  NULL, NULL, "*Modem*", NULL,
  NULL, (void *)&pcmcia_com   
  },
  { "pccom", 0,
  NULL, NULL, "*modem*", NULL,
  NULL, (void *)&pcmcia_com   
  },
  { "pccom", 0,
  NULL, NULL, NULL, "*MODEM*",
  NULL, (void *)&pcmcia_com 
  },
  { "pccom", 0,
  NULL, NULL, NULL, "*Modem*",
  NULL, (void *)&pcmcia_com   
  },
  { "pccom", 0,
  NULL, NULL, NULL, "*modem*",
  NULL, (void *)&pcmcia_com   
  },
  {NULL}
};
#define ncom_pcmcia_devs sizeof(pcmcia_com_devs)/sizeof(pcmcia_com_devs[0])

int
com_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	return pcmcia_slave_match(parent, match, aux, pcmcia_com_devs,
				  ncom_pcmcia_devs);
}

int
com_pcmcia_isa_attach(parent, match, aux, pc_link)
	struct device *parent;
	void *match;
	void *aux;
	struct pcmcia_link *pc_link;
{
	struct isa_attach_args *ia = aux;
	struct com_softc *sc = match;

	int rval;
	if ((rval = comprobe(parent, sc->sc_dev.dv_cfdata, ia))) {
		if (ISSET(pc_link->flags, PCMCIA_REATTACH)) {
#ifdef PCCOM_DEBUG
			printf("comreattach, hwflags=%x\n", sc->sc_hwflags);
#endif
			sc->sc_hwflags = COM_HW_REATTACH |
				(sc->sc_hwflags & (COM_HW_ABSENT_PENDING|COM_HW_CONSOLE));
		} else
			sc->sc_hwflags = 0;
		sc->sc_ic = ia->ia_ic;
	}
	return rval;
}


/*
 * Called by config_detach attempts, shortly after com_pcmcia_remove
 * was called.
 */
int
com_pcmcia_detach(self)
	struct device *self;
{
	struct com_softc *sc = (void *)self;

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT_PENDING)) {
		/* don't let it really be detached, it is still open */
		return EBUSY;
	}
	return 0;		/* OK! */
}

/*
 * called by pcmcia framework to accept/reject remove attempts.
 * If we return 0, then the detach will proceed.
 */
int
com_pcmcia_remove(pc_link, self)
	struct pcmcia_link *pc_link;
	struct device *self;
{
	struct com_softc *sc = (void *)self;
	struct tty *tp;
	int s;

	if (!sc->sc_tty)
		goto ok;
	tp = sc->sc_tty;

	/* not in use ?  if so, return "OK" */
	if (!ISSET(tp->t_state, TS_ISOPEN) &&
	    !ISSET(tp->t_state, TS_WOPEN)) {
		ttyfree(sc->sc_tty);
		sc->sc_tty = NULL;
    ok:
		isa_intr_disestablish(sc->sc_ic, sc->sc_ih);
		sc->sc_ih = NULL;
		SET(sc->sc_hwflags, COM_HW_ABSENT);
		return 0;		/* OK! */
	}
	/*
	 * Not easily removed.  Put device into a dead state, clean state
	 * as best we can.  notify all waiters.
	 */
	SET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING);
#ifdef PCCOM_DEBUG
	printf("pending detach flags %x\n", sc->sc_hwflags);
#endif

	s = spltty();
	com_absent_notify(sc);
	splx(s);

	return 0;
}

#if 0
void
com_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcmcia_attach_args *paa = aux;
	
	printf("com_pcmcia_attach %p %p %p\n", parent, self, aux);
	delay(2000000);
	if (!pcmcia_configure(parent, self, paa->paa_link)) {
		struct com_softc *sc = (void *)self;
		sc->sc_hwflags |= COM_HW_ABSENT;
		printf(": not attached\n");
	}
}
#endif
#endif
#endif

/*
 * must be called at spltty() or higher.
 */
void
com_absent_notify(sc)
	struct com_softc *sc;
{
	struct tty *tp = sc->sc_tty;

	if (tp) {
		CLR(tp->t_state, TS_CARR_ON|TS_BUSY);
		ttyflush(tp, FREAD|FWRITE);
	}
}

int
comspeed(speed)
	long speed;
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;

	if (speed == 0)
		return 0;
	if (speed < 0)
		return -1;
	x = divrnd((COM_FREQ / 16), speed);
	if (x <= 0)
		return -1;
	err = divrnd((COM_FREQ / 16) * 1000, speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return -1;
	return x;

#undef	divrnd(n, q)
}

int
comprobe1(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	int i, k;

	/* force access to id reg */
	bus_space_write_1(iot, ioh, com_lcr, 0);
	bus_space_write_1(iot, ioh, com_iir, 0);
	for (i = 0; i < 32; i++) {
	    k = bus_space_read_1(iot, ioh, com_iir);
	    if (k & 0x38) {
		bus_space_read_1(iot, ioh, com_data); /* cleanup */
	    } else
		break;
	}
	if (i >= 32) 
	    return 0;

	return 1;
}

#ifdef COM_HAYESP
int
comprobeHAYESP(hayespioh, sc)
	bus_space_handle_t hayespioh;
	struct com_softc *sc;
{
	char	val, dips;
	int	combaselist[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
	bus_space_tag_t iot = sc->sc_iot;

	/*
	 * Hayes ESP cards have two iobases.  One is for compatibility with
	 * 16550 serial chips, and at the same ISA PC base addresses.  The
	 * other is for ESP-specific enhanced features, and lies at a
	 * different addressing range entirely (0x140, 0x180, 0x280, or 0x300).
	 */

	/* Test for ESP signature */
	if ((bus_space_read_1(iot, hayespioh, 0) & 0xf3) == 0)
		return 0;

	/*
	 * ESP is present at ESP enhanced base address; unknown com port
	 */

	/* Get the dip-switch configurations */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETDIPS);
	dips = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1);

	/* Determine which com port this ESP card services: bits 0,1 of  */
	/*  dips is the port # (0-3); combaselist[val] is the com_iobase */
	if (sc->sc_iobase != combaselist[dips & 0x03])
		return 0;

	printf(": ESP");

 	/* Check ESP Self Test bits. */
	/* Check for ESP version 2.0: bits 4,5,6 == 010 */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETTEST);
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1); /* Clear reg 1 */
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS2);
	if ((val & 0x70) < 0x20) {
		printf("-old (%o)", val & 0x70);
		/* we do not support the necessary features */
		return 0;
	}

	/* Check for ability to emulate 16550: bit 8 == 1 */
	if ((dips & 0x80) == 0) {
		printf(" slave");
		/* XXX Does slave really mean no 16550 support?? */
		return 0;
	}

	/*
	 * If we made it this far, we are a full-featured ESP v2.0 (or
	 * better), at the correct com port address.
	 */

	SET(sc->sc_hwflags, COM_HW_HAYESP);
	printf(", 1024 byte fifo\n");
	return 1;
}
#endif

int
comprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int iobase, needioh;
	int rv = 1;

	/*
	 * XXX should be broken out into functions for isa probe and
	 * XXX for commulti probe, with a helper function that contains
	 * XXX most of the interesting stuff.
	 */
/* #if NPCCOM_ISA || NPCCOM_PCMCIA || NPCCOM_ISAPNP */
#if NPCCOM_ISA || NPCCOM_ISAPNP
	if (IS_ISA(parent) || IS_ISAPNP(parent)) {
		struct isa_attach_args *ia = aux;

		iot = ia->ia_iot;
		iobase = ia->ia_iobase;
		if (IS_ISAPNP(parent)) {
			ioh = ia->ia_ioh;
			needioh = 0;
		} else
			needioh = 1;
	} else
#endif
#if NPCCOM_COMMULTI
	if (1) {
		struct cfdata *cf = match;
		struct commulti_attach_args *ca = aux;
 
		if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != ca->ca_slave)
			return (0);

		iot = ca->ca_iot;
		iobase = ca->ca_iobase;
		ioh = ca->ca_ioh;
		needioh = 0;
	} else
#endif
		return(0);			/* This cannot happen */

	/* if it's in use as console, it's there. */
	if (iobase == comconsaddr && !comconsattached)
		goto out;

	if (needioh && bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
		rv = 0;
		goto out;
	}
	rv = comprobe1(iot, ioh);
	if (needioh)
		bus_space_unmap(iot, ioh, COM_NPORTS);

out:
/* #if NPCCOM_ISA || NPCCOM_PCMCIA */
#if NPCCOM_ISA
	if (rv) {
		struct isa_attach_args *ia = aux;

		ia->ia_iosize = COM_NPORTS;
		ia->ia_msize = 0;
	}
#endif
	return (rv);
}

void
comattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	int iobase, irq;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
#ifdef COM_HAYESP
	int	hayesp_ports[] = { 0x140, 0x180, 0x280, 0x300, 0 };
	int	*hayespp;
#endif
	u_int8_t lcr;

	/*
	 * XXX should be broken out into functions for isa attach and
	 * XXX for commulti attach, with a helper function that contains
	 * XXX most of the interesting stuff.
	 */
	if (ISSET(sc->sc_hwflags, COM_HW_REATTACH)) {
		int s;
		s = spltty();
		com_absent_notify(sc);
		splx(s);
	} else
	    sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
/* #if NPCCOM_ISA || NPCCOM_PCMCIA || NPCCOM_ISAPNP */
#if NPCCOM_ISA || NPCCOM_ISAPNP
	if (IS_ISA(parent) || IS_ISAPNP(parent)) {
		struct isa_attach_args *ia = aux;

		/*
		 * We're living on an isa.
		 */
		iobase = ia->ia_iobase;
		iot = ia->ia_iot;
		if (IS_ISAPNP(parent)) {
			/* No console support! */
			ioh = ia->ia_ioh;
		} else {
	       		if (iobase != comconsaddr) {
				if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh))
					panic("comattach: io mapping failed");
			} else
				ioh = comconsioh;
		}
		irq = ia->ia_irq;
	} else
#endif
#if NPCCOM_COMMULTI
	if (1) {
		struct commulti_attach_args *ca = aux;

		/*
		 * We're living on a commulti.
		 */
		iobase = ca->ca_iobase;
		iot = ca->ca_iot;
		ioh = ca->ca_ioh;
		irq = IRQUNK;

		if (ca->ca_noien)
			SET(sc->sc_hwflags, COM_HW_NOIEN);
	} else
#endif
		panic("comattach: impossible");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_iobase = iobase;

	if (iobase == comconsaddr) {
		comconsattached = 1;

		/* 
		 * Need to reset baud rate, etc. of next print so reset
		 * comconsinit.  Also make sure console is always "hardwired".
		 */
		delay(1000);			/* wait for output to finish */
		comconsinit = 0;
		SET(sc->sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc_swflags, COM_SW_SOFTCAR);
	}

#ifdef COM_HAYESP
	/* Look for a Hayes ESP board. */
	for (hayespp = hayesp_ports; *hayespp != 0; hayespp++) {
		bus_space_handle_t hayespioh;

#define	HAYESP_NPORTS	8			/* XXX XXX XXX ??? ??? ??? */
		if (bus_space_map(iot, *hayespp, HAYESP_NPORTS, 0, &hayespioh))
			continue;
		if (comprobeHAYESP(hayespioh, sc)) {
			sc->sc_hayespbase = *hayespp;
			sc->sc_hayespioh = hayespioh;
			sc->sc_fifolen = 1024;
			break;
		}
		bus_space_unmap(iot, hayespioh, HAYESP_NPORTS);
	}
	/* No ESP; look for other things. */
	if (*hayespp == 0) {
#endif

	/*
	 * Probe for all known forms of UART.
	 */
	lcr = bus_space_read_1(iot, ioh, com_lcr);

	bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
	bus_space_write_1(iot, ioh, com_efr, 0);
	bus_space_write_1(iot, ioh, com_lcr, 0);

	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	delay(100);

	switch(bus_space_read_1(iot, ioh, com_iir) >> 6) {
	case 0:
		sc->sc_uarttype = COM_UART_16450;
		break;
	case 2:
		sc->sc_uarttype = COM_UART_16550;
		break;
	case 3:
		sc->sc_uarttype = COM_UART_16550A;
		break;
	default:
		sc->sc_uarttype = COM_UART_UNKNOWN;
		break;
	}

	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for ST16650s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		if (bus_space_read_1(iot, ioh, com_efr) == 0) {
			sc->sc_uarttype = COM_UART_ST16650;
		} else {
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			if (bus_space_read_1(iot, ioh, com_efr) == 0)
				sc->sc_uarttype = COM_UART_ST16650V2;
		}
	}

	if (sc->sc_uarttype == COM_UART_ST16650V2) {	/* Probe for XR16850s */
		u_int8_t dlbl, dlbh;

		/* Enable latch access and get the current values. */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		dlbl = bus_space_read_1(iot, ioh, com_dlbl);
		dlbh = bus_space_read_1(iot, ioh, com_dlbh);

		/* Zero out the latch divisors */
		bus_space_write_1(iot, ioh, com_dlbl, 0);
		bus_space_write_1(iot, ioh, com_dlbh, 0);

		if (bus_space_read_1(iot, ioh, com_dlbh) == 0x10) {
			sc->sc_uarttype = COM_UART_XR16850;
			sc->sc_uartrev = bus_space_read_1(iot, ioh, com_dlbl);
		}

		/* Reset to original. */
		bus_space_write_1(iot, ioh, com_dlbl, dlbl);
		bus_space_write_1(iot, ioh, com_dlbh, dlbh);
	}

#ifdef notyet
	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for TI16750s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		bus_space_write_1(iot, ioh, com_fifo,
		    FIFO_ENABLE | FIFO_ENABLE_64BYTE);
		if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 7) {
			bus_space_write_1(iot, ioh, com_lcr, 0);
			if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 6)
				sc->sc_uarttype = COM_UART_TI16750;
		}
		bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	}
#endif

	/* Reset the LCR (latch access is probably enabled). */
	bus_space_write_1(iot, ioh, com_lcr, lcr);
	if (sc->sc_uarttype == COM_UART_16450) { /* Probe for 8250 */
		u_int8_t scr0, scr1, scr2;

		scr0 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0xa5);
		scr1 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0x5a);
		scr2 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, scr0);

		if ((scr1 != 0xa5) || (scr2 != 0x5a))
			sc->sc_uarttype = COM_UART_8250;
	}

	/*
	 * Print UART type and initialize ourself.
	 */
	sc->sc_fifolen = 1;	/* default */
	switch (sc->sc_uarttype) {
	case COM_UART_UNKNOWN:
		printf(": unknown uart\n");
		break;
	case COM_UART_8250:
		printf(": ns8250, no fifo\n");
		break;
	case COM_UART_16450:
		printf(": ns16450, no fifo\n");
		break;
	case COM_UART_16550:
		printf(": ns16550, no working fifo\n");
		break;
	case COM_UART_16550A:
		printf(": ns16550a, 16 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 16;
		break;
	case COM_UART_ST16650:
		printf(": st16650, no working fifo\n");
		break;
	case COM_UART_ST16650V2:
		printf(": st16650, 32 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 32;
		break;
#ifdef notyet
	case COM_UART_TI16750:
		printf(": ti16750, 64 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 64;
		break;
#endif
	case COM_UART_XR16850:
		printf(": xr16850 (rev %d), 128 byte fifo\n", sc->sc_uartrev);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 128;
		break;
	default:
		panic("comattach: bad fifo type");
	}

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	(void)bus_space_read_1(iot, ioh, com_data);
	bus_space_write_1(iot, ioh, com_fifo, 0);
#ifdef COM_HAYESP
	}
#endif

	/* disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);

	if (irq != IRQUNK) {
/* #if NPCCOM_ISA || NPCCOM_PCMCIA || NPCCOM_ISAPNP */
#if NPCCOM_ISA || NPCCOM_ISAPNP
		if (IS_ISA(parent) || IS_ISAPNP(parent)) {
			struct isa_attach_args *ia = aux;

			sc->sc_ih = isa_intr_establish(ia->ia_ic, irq,
			    IST_EDGE, IPL_HIGH, comintr, sc,
			    sc->sc_dev.dv_xname);
		} else
#endif
			panic("comattach: IRQ but can't have one");
	}

#ifdef KGDB
	if (kgdb_dev == makedev(commajor, unit)) {
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			cominit(iot, ioh, kgdb_rate);
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("%s: ", sc->sc_dev.dv_xname);
				kgdb_connect(1);
			} else
				printf("%s: kgdb enabled\n",
				    sc->sc_dev.dv_xname);
		}
	}
#endif

	/* XXX maybe move up some? */
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
		printf("%s: console\n", sc->sc_dev.dv_xname);
}

int
comopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct tty *tp;
	int s;
	int error = 0;
 
	if (unit >= pccom_cd.cd_ndevs)
		return ENXIO;
	sc = pccom_cd.cd_devs[unit];
	if (!sc || ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING))
		return ENXIO;

	s = spltty();
	if (!sc->sc_tty) {
		tp = sc->sc_tty = ttymalloc();
		tty_attach(tp);
	} else
		tp = sc->sc_tty;
	splx(s);

	tp->t_oproc = comstart;
	tp->t_param = comparam;
	tp->t_hwiflow = comhwiflow;
	tp->t_dev = dev;
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			tp->t_cflag = comconscflag;
		else
			tp->t_cflag = TTYDEF_CFLAG;
		if (ISSET(sc->sc_swflags, COM_SW_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, COM_SW_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, COM_SW_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = comdefaultrate;

		s = spltty();

		sc->sc_initialize = 1;
		comparam(tp, &tp->t_termios);
		ttsetwater(tp);

		sc->sc_rxput = sc->sc_rxget = sc->sc_tbc = 0;

		iot = sc->sc_iot;
		ioh = sc->sc_ioh;

		/*
		 * Wake up the sleepy heads.
		 */
		switch (sc->sc_uarttype) {
		case COM_UART_ST16650:
		case COM_UART_ST16650V2:
		case COM_UART_XR16850:
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
			bus_space_write_1(iot, ioh, com_ier, 0);
			bus_space_write_1(iot, ioh, com_efr, 0);
			bus_space_write_1(iot, ioh, com_lcr, 0);
			break;
#ifdef notyet
		case COM_UART_TI16750:
			bus_space_write_1(iot, ioh, com_ier, 0);
			break;
#endif
		}

#ifdef COM_HAYESP
		/* Setup the ESP board */
		if (ISSET(sc->sc_hwflags, COM_HW_HAYESP)) {
			bus_space_handle_t hayespioh = sc->sc_hayespioh;

			bus_space_write_1(iot, ioh, com_fifo,
			     FIFO_DMA_MODE|FIFO_ENABLE|
			     FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_8);

			/* Set 16550 compatibility mode */
			bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETMODE);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, 
			     HAYESP_MODE_FIFO|HAYESP_MODE_RTS|
			     HAYESP_MODE_SCALE);

			/* Set RTS/CTS flow control */
			bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETFLOWTYPE);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, HAYESP_FLOW_RTS);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, HAYESP_FLOW_CTS);

			/* Set flow control levels */
			bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETRXFLOW);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, 
			     HAYESP_HIBYTE(HAYESP_RXHIWMARK));
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
			     HAYESP_LOBYTE(HAYESP_RXHIWMARK));
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
			     HAYESP_HIBYTE(HAYESP_RXLOWMARK));
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
			     HAYESP_LOBYTE(HAYESP_RXLOWMARK));
		} else
#endif
		if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
			u_int8_t fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST;
			u_int8_t lcr;

			switch (sc->sc_uarttype) {
			case COM_UART_ST16650V2:
				if (tp->t_ispeed <= 1200)
					fifo |= FIFO_RCV_TRIGGER_8|FIFO_XMT_TRIGGER_8; /* XXX */
				else
					fifo |= FIFO_RCV_TRIGGER_28|FIFO_XMT_TRIGGER_30;
				break;
			case COM_UART_XR16850:
				pccom_xr16850_fifo_init(iot, ioh);
				if (tp->t_ispeed <= 1200)
					fifo |= FIFO_RCV3_TRIGGER_8|FIFO_XMT3_TRIGGER_8; /* XXX */
				else
					fifo |= FIFO_RCV3_TRIGGER_60|FIFO_XMT3_TRIGGER_56;
				break;
#ifdef notyet
			case COM_UART_TI16750:
				fifo |= FIFO_ENABLE_64BYTE;
				lcr = bus_space_read_1(iot, ioh, com_lcr);
				bus_space_write_1(iot, ioh, com_lcr,
				    lcr | LCR_DLAB);
#endif
			default:
				if (tp->t_ispeed <= 1200)
					fifo |= FIFO_TRIGGER_1;
				else
					fifo |= FIFO_TRIGGER_8;
			}

			/*
			 * (Re)enable and drain FIFOs.
			 *
			 * Certain SMC chips cause problems if the FIFOs are
			 * enabled while input is ready. Turn off the FIFO
			 * if necessary to clear the input. Test the input
			 * ready bit after enabling the FIFOs to handle races
			 * between enabling and fresh input.
			 *
			 * Set the FIFO threshold based on the receive speed.
			 */
			for (;;) {
			 	bus_space_write_1(iot, ioh, com_fifo, 0);
				delay(100);
				(void) bus_space_read_1(iot, ioh, com_data);
				bus_space_write_1(iot, ioh, com_fifo, fifo |
				    FIFO_RCV_RST | FIFO_XMT_RST);
				delay(100);
				if(!ISSET(bus_space_read_1(iot, ioh,
				    com_lsr), LSR_RXRDY))
				    	break;
			}
#ifndef notyet
			if (sc->sc_uarttype == COM_UART_TI16750)
				bus_space_write_1(iot, ioh, com_lcr, lcr);
#endif
		}

		/* flush any pending I/O */
		while (ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
			(void) bus_space_read_1(iot, ioh, com_data);
		/* you turn me on, baby */
		sc->sc_mcr = MCR_DTR | MCR_RTS;
		if (!ISSET(sc->sc_hwflags, COM_HW_NOIEN))
			SET(sc->sc_mcr, MCR_IENABLE);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		sc->sc_ier = IER_ERXRDY | IER_ERLS | IER_EMSC;
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

		sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);
		if (ISSET(sc->sc_swflags, COM_SW_SOFTCAR) || DEVCUA(dev) ||
		    ISSET(sc->sc_msr, MSR_DCD) || ISSET(tp->t_cflag, MDMBUF))
			SET(tp->t_state, TS_CARR_ON);
		else
			CLR(tp->t_state, TS_CARR_ON);
	} else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;
	else
		s = spltty();

	if (DEVCUA(dev)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return EBUSY;
		}
		sc->sc_cua = 1;		/* We go into CUA mode */
	}

	/* wait for carrier if necessary */
	if (ISSET(flag, O_NONBLOCK)) {
		if (!DEVCUA(dev) && sc->sc_cua) {
			/* Opening TTY non-blocking... but the CUA is busy */
			splx(s);
			return EBUSY;
		}
	} else {
		while (sc->sc_cua ||
		    (!ISSET(tp->t_cflag, CLOCAL) &&
		    !ISSET(tp->t_state, TS_CARR_ON))) {
			SET(tp->t_state, TS_WOPEN);
			error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH,
			    ttopen, 0);
			if (!DEVCUA(dev) && sc->sc_cua && error == EINTR)
				continue;
			if (error) {
				/* XXX should turn off chip if we're the
				   only waiter */
				if (DEVCUA(dev))
					sc->sc_cua = 0;
				CLR(tp->t_state, TS_WOPEN);
				splx(s);
				return error;
			}
			if (!DEVCUA(dev) && sc->sc_cua)
				continue;
		}
	}
	splx(s);

	return (*linesw[tp->t_line].l_open)(dev, tp);
}
 
int
comclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc = pccom_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flag);
	s = spltty();
	if (!ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		/* can't do any of this stuff .... */
		CLR(sc->sc_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
		bus_space_write_1(iot, ioh, com_ier, 0);
		if (ISSET(tp->t_cflag, HUPCL) &&
		    !ISSET(sc->sc_swflags, COM_SW_SOFTCAR)) {
			/* XXX perhaps only clear DTR */
			bus_space_write_1(iot, ioh, com_mcr, 0);
		}

		/*
	 	 * Turn FIFO off; enter sleep mode if possible.
	 	 */
		bus_space_write_1(iot, ioh, com_fifo, 0);
		delay(100);
		(void) bus_space_read_1(iot, ioh, com_data);
		delay(100);
		bus_space_write_1(iot, ioh, com_fifo,
		    FIFO_RCV_RST | FIFO_XMT_RST);

		switch (sc->sc_uarttype) {
		case COM_UART_ST16650:
		case COM_UART_ST16650V2:
		case COM_UART_XR16850:
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
			bus_space_write_1(iot, ioh, com_ier, IER_SLEEP);
			bus_space_write_1(iot, ioh, com_lcr, 0);
			break;
#ifdef notyet
		case COM_UART_TI16750:
			bus_space_write_1(iot, ioh, com_ier, IER_SLEEP);
			break;
#endif
		}
	}
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);
	sc->sc_cua = 0;
	splx(s);
	ttyclose(tp);
#ifdef PCCOM_DEBUG
	/* mark it ready for more use if reattached earlier */
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT_PENDING)) {
	    printf("comclose pending cleared\n");
	}
#endif
	CLR(sc->sc_hwflags, COM_HW_ABSENT_PENDING);

#ifdef notyet /* XXXX */
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		ttyfree(tp);
		sc->sc_tty = 0;
	}
#endif
	return 0;
}
 
int
comread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct com_softc *sc = pccom_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
comwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct com_softc *sc = pccom_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
comtty(dev)
	dev_t dev;
{
	struct com_softc *sc = pccom_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}
 
static u_char
tiocm_xxx2mcr(data)
	int data;
{
	u_char m = 0;

	if (ISSET(data, TIOCM_DTR))
		SET(m, MCR_DTR);
	if (ISSET(data, TIOCM_RTS))
		SET(m, MCR_RTS);
	return m;
}

int
comioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc = pccom_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int error;

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch (cmd) {
	case TIOCSBRK:
		SET(sc->sc_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
		break;
	case TIOCCBRK:
		CLR(sc->sc_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
		break;
	case TIOCSDTR:
		SET(sc->sc_mcr, sc->sc_dtr);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCCDTR:
		CLR(sc->sc_mcr, sc->sc_dtr);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMSET:
		CLR(sc->sc_mcr, MCR_DTR | MCR_RTS);
	case TIOCMBIS:
		SET(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMBIC:
		CLR(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMGET: {
		u_char m;
		int bits = 0;

		m = sc->sc_mcr;
		if (ISSET(m, MCR_DTR))
			SET(bits, TIOCM_DTR);
		if (ISSET(m, MCR_RTS))
			SET(bits, TIOCM_RTS);
		m = sc->sc_msr;
		if (ISSET(m, MSR_DCD))
			SET(bits, TIOCM_CD);
		if (ISSET(m, MSR_CTS))
			SET(bits, TIOCM_CTS);
		if (ISSET(m, MSR_DSR))
			SET(bits, TIOCM_DSR);
		if (ISSET(m, MSR_RI | MSR_TERI))
			SET(bits, TIOCM_RI);
		if (bus_space_read_1(iot, ioh, com_ier))
			SET(bits, TIOCM_LE);
		*(int *)data = bits;
		break;
	}
	case TIOCGFLAGS: {
		int driverbits, userbits = 0;

		driverbits = sc->sc_swflags;
		if (ISSET(driverbits, COM_SW_SOFTCAR))
			SET(userbits, TIOCFLAG_SOFTCAR);
		if (ISSET(driverbits, COM_SW_CLOCAL))
			SET(userbits, TIOCFLAG_CLOCAL);
		if (ISSET(driverbits, COM_SW_CRTSCTS))
			SET(userbits, TIOCFLAG_CRTSCTS);
		if (ISSET(driverbits, COM_SW_MDMBUF))
			SET(userbits, TIOCFLAG_MDMBUF);

		*(int *)data = userbits;
		break;
	}
	case TIOCSFLAGS: {
		int userbits, driverbits = 0;

		error = suser(p->p_ucred, &p->p_acflag); 
		if (error != 0)
			return(EPERM); 

		userbits = *(int *)data;
		if (ISSET(userbits, TIOCFLAG_SOFTCAR) ||
		    ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			SET(driverbits, COM_SW_SOFTCAR);
		if (ISSET(userbits, TIOCFLAG_CLOCAL))
			SET(driverbits, COM_SW_CLOCAL);
		if (ISSET(userbits, TIOCFLAG_CRTSCTS))
			SET(driverbits, COM_SW_CRTSCTS);
		if (ISSET(userbits, TIOCFLAG_MDMBUF))
			SET(driverbits, COM_SW_MDMBUF);

		sc->sc_swflags = driverbits;
		break;
	}
	default:
		return ENOTTY;
	}

	return 0;
}

int
comparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct com_softc *sc = pccom_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int ospeed = comspeed(t->c_ospeed);
	u_int8_t lcr;
	tcflag_t oldcflag;
	int s;

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

	/* check requested parameters */
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return EINVAL;

	lcr = ISSET(sc->sc_lcr, LCR_SBREAK);

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		SET(lcr, LCR_5BITS);
		break;
	case CS6:
		SET(lcr, LCR_6BITS);
		break;
	case CS7:
		SET(lcr, LCR_7BITS);
		break;
	case CS8:
		SET(lcr, LCR_8BITS);
		break;
	}
	if (ISSET(t->c_cflag, PARENB)) {
		SET(lcr, LCR_PENAB);
		if (!ISSET(t->c_cflag, PARODD))
			SET(lcr, LCR_PEVEN);
	}
	if (ISSET(t->c_cflag, CSTOPB))
		SET(lcr, LCR_STOPB);

	sc->sc_lcr = lcr;

	s = spltty();

	if (ospeed == 0) {
		CLR(sc->sc_mcr, MCR_DTR);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
	}

	/*
	 * Set the FIFO threshold based on the receive speed, if we are
	 * changing it.
	 */
	if (sc->sc_initialize || (tp->t_ispeed != t->c_ispeed)) {
		sc->sc_initialize = 0;

		if (ospeed != 0) {
			/*
			 * Make sure the transmit FIFO is empty before
			 * proceeding.  If we don't do this, some revisions
			 * of the UART will hang.  Interestingly enough,
			 * even if we do this while the last character is
			 * still being pushed out, they don't hang.  This
			 * seems good enough.
			 */
			while (ISSET(tp->t_state, TS_BUSY)) {
				int error;

				++sc->sc_halt;
				error = ttysleep(tp, &tp->t_outq,
				    TTOPRI | PCATCH, "comprm", 0);
				--sc->sc_halt;
				if (error) {
					splx(s);
					comstart(tp);
					return (error);
				}
			}

			bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
			bus_space_write_1(iot, ioh, com_dlbl, ospeed);
			bus_space_write_1(iot, ioh, com_dlbh, ospeed >> 8);
			bus_space_write_1(iot, ioh, com_lcr, lcr);
			SET(sc->sc_mcr, MCR_DTR);
			bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		} else
			bus_space_write_1(iot, ioh, com_lcr, lcr);

		if (!ISSET(sc->sc_hwflags, COM_HW_HAYESP) &&
		    ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
			u_int8_t fifo = FIFO_ENABLE;
#ifdef notyet
			u_int8_t lcr2;
#endif

			switch (sc->sc_uarttype) {
			case COM_UART_ST16650V2:
				if (t->c_ispeed <= 1200)
					fifo |= FIFO_RCV_TRIGGER_8|FIFO_XMT_TRIGGER_8; /* XXX */
				else
					fifo |= FIFO_RCV_TRIGGER_28|FIFO_XMT_TRIGGER_30;
				break;
			case COM_UART_XR16850:
				if (t->c_ispeed <= 1200)
					fifo |= FIFO_RCV3_TRIGGER_8|FIFO_XMT3_TRIGGER_8; /* XXX */
				else
					fifo |= FIFO_RCV3_TRIGGER_60|FIFO_XMT3_TRIGGER_56;
				break;
#ifdef notyet
			case COM_UART_TI16750:
				fifo |= FIFO_ENABLE_64BYTE;
				lcr2 = bus_space_read_1(iot, ioh, com_lcr);
				bus_space_write_1(iot, ioh, com_lcr,
				    lcr2 | LCR_DLAB);
#endif
			default:
				if (t->c_ispeed <= 1200)
					fifo |= FIFO_TRIGGER_1;
				else
					fifo |= FIFO_TRIGGER_8;
			}
			bus_space_write_1(iot, ioh, com_fifo, fifo);

#ifdef notyet
			if (sc->sc_uarttype == COM_UART_TI16750)
				bus_space_write_1(iot, ioh, com_lcr, lcr2);
#endif
		}
	} else
		bus_space_write_1(iot, ioh, com_lcr, lcr);

	/* When not using CRTSCTS, RTS follows DTR. */
	if (!ISSET(t->c_cflag, CRTSCTS)) {
		if (ISSET(sc->sc_mcr, MCR_DTR)) {
			if (!ISSET(sc->sc_mcr, MCR_RTS)) {
				SET(sc->sc_mcr, MCR_RTS);
				bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
			}
		} else {
			if (ISSET(sc->sc_mcr, MCR_RTS)) {
				CLR(sc->sc_mcr, MCR_RTS);
				bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
			}
		}
		sc->sc_dtr = MCR_DTR | MCR_RTS;
	} else
		sc->sc_dtr = MCR_DTR;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	oldcflag = tp->t_cflag;
	tp->t_cflag = t->c_cflag;

	/*
	 * If DCD is off and MDMBUF is changed, ask the tty layer if we should
	 * stop the device.
	 */
	if (!ISSET(sc->sc_msr, MSR_DCD) &&
	    !ISSET(sc->sc_swflags, COM_SW_SOFTCAR) &&
	    ISSET(oldcflag, MDMBUF) != ISSET(tp->t_cflag, MDMBUF) &&
	    (*linesw[tp->t_line].l_modem)(tp, 0) == 0) {
		CLR(sc->sc_mcr, sc->sc_dtr);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
	}

	/* Just to be sure... */
	splx(s);
	comstart(tp);
	return 0;
}

void
comstart_pending(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	int s;

	s = spltty();
	com_absent_notify(sc);
	splx(s);
}

/*
 * (un)block input via hw flowcontrol
 */
int
comhwiflow(tp, block)
	struct tty *tp;
	int block;
{
	struct com_softc *sc = pccom_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int	s;

/*
 * XXX
 * Is spltty needed at all ? sc->sc_mcr is only in comsoft() not comintr()
 */
	s = spltty();
	if (block) {
		/* When not using CRTSCTS, RTS follows DTR. */
		if (ISSET(tp->t_cflag, MDMBUF)) {
			CLR(sc->sc_mcr, (MCR_DTR | MCR_RTS));
			bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		}
		else {
			CLR(sc->sc_mcr, MCR_RTS);
			bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		}
	}
	else {
		/* When not using CRTSCTS, RTS follows DTR. */
		if (ISSET(tp->t_cflag, MDMBUF)) {
			SET(sc->sc_mcr, (MCR_DTR | MCR_RTS));
			bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		}
		else {
			SET(sc->sc_mcr, MCR_RTS);
			bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		}
	}
	splx(s);
	return 1;
}

void
comstart(tp)
	struct tty *tp;
{
	struct com_softc *sc = pccom_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s, count;

	s = spltty();
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		/*
		 * not quite good enough: if caller is ttywait() it will
		 * go to sleep immediately, so hang out a bit and then
		 * prod caller again.
		 */
		com_absent_notify(sc);
		timeout(comstart_pending, sc, 1);
		goto out;
	}
	if (ISSET(tp->t_state, TS_BUSY))
		goto out;
	if (ISSET(tp->t_state, TS_TIMEOUT | TS_TTSTOP) || sc->sc_halt > 0)
		goto stopped;
	if (ISSET(tp->t_cflag, CRTSCTS) && !ISSET(sc->sc_msr, MSR_CTS))
		goto stopped;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	count = ndqb(&tp->t_outq, 0);
	splhigh();
	if (count > 0) {
		int n;

		SET(tp->t_state, TS_BUSY);
		if (!ISSET(sc->sc_ier, IER_ETXRDY)) {
			SET(sc->sc_ier, IER_ETXRDY);
			bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);
		}
		n = sc->sc_fifolen;
		if (n > count)
			n = count;
		sc->sc_tba = tp->t_outq.c_cf;
		while (--n >= 0) {
			bus_space_write_1(iot, ioh, com_data, *sc->sc_tba++);
			--count;
		}
		sc->sc_tbc = count;
		goto out;
	}
stopped:
	if (ISSET(sc->sc_ier, IER_ETXRDY)) {
		CLR(sc->sc_ier, IER_ETXRDY);
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);
	}
out:
	splx(s);
	return;
}

/*
 * Stop output on a line.
 */
int
comstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;
	struct com_softc *sc = pccom_cd.cd_devs[DEVUNIT(tp->t_dev)];

	s = splhigh();
	if (ISSET(tp->t_state, TS_BUSY)) {
		sc->sc_tbc = 0;
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	}
	splx(s);
	return 0;
}

void
comdiag(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	int overflows;
	int s;

	s = spltty();
	overflows = sc->sc_overflows;
	sc->sc_overflows = 0;
	splx(s);

	if (overflows)
		log(LOG_WARNING, "%s: %d silo overflow%s\n",
		    sc->sc_dev.dv_xname, overflows, overflows == 1 ? "" : "s");
}

#ifdef PCCOM_DEBUG
int	maxcc = 0;
#endif

void
comsoft()
{
	struct com_softc	*sc;
	struct tty *tp;
	struct linesw	*line;
	int	unit, s, c;
	u_int rxget;
	static int lsrmap[8] = {
		0,      TTY_PE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE
	};

	for (unit = 0; unit < pccom_cd.cd_ndevs; unit++) {
		sc = pccom_cd.cd_devs[unit];
		if (sc == NULL)
			continue;
		tp = sc->sc_tty;
/*
 * XXX only use (tp == NULL) ???
 */
		if (tp == NULL || !ISSET(tp->t_state, TS_ISOPEN | TS_WOPEN))
			continue;
	 	line = &linesw[tp->t_line];
/*
 * XXX where do we _really_ need spltty(), if at all ???
 */
		s = spltty();
		rxget = sc->sc_rxget;
		while (rxget != sc->sc_rxput) {
			u_int8_t lsr;

			lsr = sc->sc_rxbuf[rxget];
			rxget = (rxget + 1) & RBUFMASK;
			if (ISSET(lsr, LSR_RCV_MASK)) {
				if (ISSET(lsr, LSR_BI)) {
#ifdef DDB
					if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
						if (db_console)
					 		Debugger();
						rxget = (rxget + 1) & RBUFMASK;
						continue;
 					}
#endif
					c = 0;
				}
				else
					c = sc->sc_rxbuf[rxget];
				if (ISSET(lsr, LSR_OE)) {
					sc->sc_overflows++;
					if (sc->sc_errors++ == 0)
						timeout(comdiag, sc, 60 * hz);
				}
				rxget = (rxget + 1) & RBUFMASK;
				c |= lsrmap[(lsr & (LSR_BI|LSR_FE|LSR_PE)) >> 2];
				line->l_rint(c, tp);
			}
			else if (ISSET(lsr, LSR_TXRDY)) {
				CLR(tp->t_state, TS_BUSY);
				if (ISSET(tp->t_state, TS_FLUSH))
					CLR(tp->t_state, TS_FLUSH);
				else
					ndflush(&tp->t_outq,
				 	(int)(sc->sc_tba - tp->t_outq.c_cf));
				if (sc->sc_halt > 0)
					wakeup(&tp->t_outq);
				line->l_start(tp);
			}
			else if (lsr == 0) {
				u_int8_t msr;

				msr = sc->sc_rxbuf[rxget];
				rxget = (rxget + 1) & RBUFMASK;
				if (ISSET(msr, MSR_DDCD) &&
		    		   !ISSET(sc->sc_swflags, COM_SW_SOFTCAR)) {
					if (ISSET(msr, MSR_DCD))
						line->l_modem(tp, 1);
					else if (line->l_modem(tp, 0) == 0) {
						CLR(sc->sc_mcr, sc->sc_dtr);
						bus_space_write_1(sc->sc_iot,
							       sc->sc_ioh,
							       com_mcr,
							       sc->sc_mcr);
					}
				}
				if (ISSET(msr, MSR_DCTS) &&
				    ISSET(msr, MSR_CTS) &&
				    ISSET(tp->t_cflag, CRTSCTS))
					line->l_start(tp);
			}
		}
		sc->sc_rxget = rxget;
/*
 * XXX this is the place where we could unblock the input
 */
		splx(s);
	}
}

int
comintr(arg)
	void	*arg;
{
	struct com_softc *sc = arg;
	struct tty *tp = sc->sc_tty;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t lsr;
	u_int	rxput;

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT) || !sc->sc_tty)
		return (0);	/* can't do squat. */

	if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
		return (0);

	rxput = sc->sc_rxput;
	do {
		u_int8_t msr, delta;

		for (;;) {
			lsr = bus_space_read_1(iot, ioh, com_lsr);
			if (!ISSET(lsr, LSR_RCV_MASK))
				break;
			sc->sc_rxbuf[rxput] = lsr;
			rxput = (rxput + 1) & RBUFMASK;
			sc->sc_rxbuf[rxput] = bus_space_read_1(iot, ioh, com_data);
			rxput = (rxput + 1) & RBUFMASK;
		}
		msr = bus_space_read_1(iot, ioh, com_msr);
		delta = msr ^ sc->sc_msr;
		if (!ISSET(delta, MSR_DCD | MSR_CTS | MSR_RI | MSR_DSR))
			continue;
		sc->sc_msr = msr;
/*
 * stop output straight away if CTS drops and RTS/CTS flowcontrol is used
 * XXX what about DTR/DCD flowcontrol (ISSET(t_cflag, MDMBUF))
 */
		msr = (msr & 0xf0) | (delta >> 4);
		if (ISSET(tp->t_cflag, CRTSCTS) && ISSET(msr, MSR_DCTS)) {
			if (!ISSET(msr, MSR_CTS))
				sc->sc_tbc = 0;
		}
		sc->sc_rxbuf[rxput] = 0;
		rxput = (rxput + 1) & RBUFMASK;
		sc->sc_rxbuf[rxput] = msr;
		rxput = (rxput + 1) & RBUFMASK;
	} while (!ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND));
	if (ISSET(lsr, LSR_TXRDY)) {
		if (sc->sc_tbc > 0) {
			int	n;

			n = sc->sc_fifolen;
			if (n > sc->sc_tbc)
				n = sc->sc_tbc;
			while (--n >= 0) {
				bus_space_write_1(iot, ioh, com_data, *sc->sc_tba++);
				--sc->sc_tbc;
			}
		}
		else if (ISSET(tp->t_state, TS_BUSY)) {
			sc->sc_rxbuf[rxput] = lsr;
			rxput = (rxput + 1) & RBUFMASK;
		}
	}
	if (sc->sc_rxput != rxput) {
/*
 * XXX
 * This is the place to do input flow control by dropping RTS or DTR.
 * However, 115200 bps transfers get maxcc only up to 112 while there's
 * room for 512 so should we bother ?
 */
#ifdef PCCOM_DEBUG
		int	cc;

		cc = rxput - sc->sc_rxget;
		if (cc < 0)
			cc += RBUFSIZE;
		if (cc > maxcc)
			maxcc = cc;
#endif
		sc->sc_rxput = rxput;
		setsofttty();
	}
	return 1;
}

void
pccom_xr16850_fifo_init(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	u_int8_t lcr, efr, fctl;

	lcr = bus_space_read_1(iot, ioh, com_lcr);
	bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
	efr = bus_space_read_1(iot, ioh, com_efr);
	bus_space_write_1(iot, ioh, com_efr, efr | EFR_ECB);
	fctl = bus_space_read_1(iot, ioh, com_fctl);
	bus_space_write_1(iot, ioh, com_fctl, fctl | FCTL_TRIGGER3);
	bus_space_write_1(iot, ioh, com_lcr, lcr);
}

/*
 * Following are all routines needed for PCCOM to act as console
 */

void
comcnprobe(cp)
	struct consdev *cp;
{
	/* XXX NEEDS TO BE FIXED XXX */
	bus_space_tag_t iot = 0;
	bus_space_handle_t ioh;
	int found;

	if (bus_space_map(iot, CONADDR, COM_NPORTS, 0, &ioh)) {
		cp->cn_pri = CN_DEAD;
		return;
	}
	found = comprobe1(iot, ioh);
	bus_space_unmap(iot, ioh, COM_NPORTS);
	if (!found) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* locate the major number */
	for (commajor = 0; commajor < nchrdev; commajor++)
		if (cdevsw[commajor].d_open == comopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(commajor, CONUNIT);
#if defined(COMCONSOLE) || defined(PCCOMCONSOLE)
	cp->cn_pri = CN_REMOTE;		/* Force a serial port console */
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

void
comcninit(cp)
	struct consdev *cp;
{

#if 0
	XXX NEEDS TO BE FIXED XXX
	comconsiot = ???;
#endif
	if (bus_space_map(comconsiot, CONADDR, COM_NPORTS, 0, &comconsioh))
		panic("comcninit: mapping failed");

	cominit(comconsiot, comconsioh, comdefaultrate);
	comconsaddr = CONADDR;
	comconsinit = 0;
}

void
cominit(iot, ioh, rate)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int rate;
{
	int s = splhigh();
	u_int8_t stat;

	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	rate = comspeed(comdefaultrate);
	bus_space_write_1(iot, ioh, com_dlbl, rate);
	bus_space_write_1(iot, ioh, com_dlbh, rate >> 8);
	bus_space_write_1(iot, ioh, com_lcr, LCR_8BITS);
	bus_space_write_1(iot, ioh, com_ier, IER_ERXRDY | IER_ETXRDY);
	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_4);
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
}

int
comcngetc(dev)
	dev_t dev;
{
	int s = splhigh();
	bus_space_tag_t iot = comconsiot;
	bus_space_handle_t ioh = comconsioh;
	u_int8_t stat, c;

	while (!ISSET(stat = bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
		;
	c = bus_space_read_1(iot, ioh, com_data);
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
	return c;
}

/*
 * Console kernel output character routine.
 */
void
comcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s = splhigh();
	bus_space_tag_t iot = comconsiot;
	bus_space_handle_t ioh = comconsioh;
	u_int8_t stat;
	register int timo;

#ifdef KGDB
	if (dev != kgdb_dev)
#endif
	if (comconsinit == 0) {
		cominit(iot, ioh, comdefaultrate);
		comconsinit = 1;
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (!ISSET(stat = bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		;
	bus_space_write_1(iot, ioh, com_data, c);
	/* wait for this transmission to complete */
	timo = 1500000;
	while (!ISSET(stat = bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		;
	/* clear any interrupts generated by this transmission */
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
}

void
comcnpollc(dev, on)
	dev_t dev;
	int on;
{

}
