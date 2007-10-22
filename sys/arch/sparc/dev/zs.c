/*	$OpenBSD: zs.c,v 1.44 2007/10/22 14:46:46 jsing Exp $	*/
/*	$NetBSD: zs.c,v 1.50 1997/10/18 00:00:40 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * Zilog Z8530 Dual UART driver (machine-dependent part)
 *
 * Runs two serial lines per chip using slave drivers.
 * Plain tty/async lines use the zs_async slave.
 * Sun keyboard/mouse uses the zs_kbd/zs_ms slaves.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/eeprom.h>
#if defined(SUN4)
#include <machine/oldmon.h>
#endif
#include <machine/psl.h>
#include <machine/z8530var.h>

#include <dev/cons.h>
#include <sparc/dev/z8530reg.h>

#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/auxioreg.h>
#include <sparc/dev/cons.h>

#ifdef solbourne
#include <machine/prom.h>
#endif

#include <uvm/uvm_extern.h>

#include "zskbd.h"
#include "zs.h"

/* Make life easier for the initialized arrays here. */
#if NZS < 3
#undef  NZS
#define NZS 3
#endif

/*
 * Some warts needed by z8530tty.c -
 * The default parity REALLY needs to be the same as the PROM uses,
 * or you can not see messages done with printf during boot-up...
 */
int zs_def_cflag = (CREAD | CS8 | HUPCL);
int zs_major = 12;

/*
 * The Sun provides a 4.9152 MHz clock to the ZS chips.
 */
#define PCLK	(9600 * 512)	/* PCLK pin input clock rate */

/*
 * Select software interrupt bit based on TTY ipl.
 */
#if IPL_TTY == 1
# define IE_ZSSOFT IE_L1
#elif IPL_TTY == 4
# define IE_ZSSOFT IE_L4
#elif IPL_TTY == 6
# define IE_ZSSOFT IE_L6
#else
# error "no suitable software interrupt bit"
#endif

#define	ZS_DELAY()		(CPU_ISSUN4C ? (0) : delay(2))

/* The layout of this is hardware-dependent (padding, order). */
struct zschan {
#if defined(SUN4) || defined(SUN4C) || defined(SUN4M)
	volatile u_char	zc_csr;		/* ctrl,status, and indirect access */
	u_char		zc_xxx0;
	volatile u_char	zc_data;	/* data */
	u_char		zc_xxx1;
#endif
#if defined(solbourne)
	volatile u_char	zc_csr;		/* ctrl,status, and indirect access */
	u_char		zc_xxx0[7];
	volatile u_char	zc_data;	/* data */
	u_char		zc_xxx1[7];
#endif
};
struct zsdevice {
	/* Yes, they are backwards. */
	struct	zschan zs_chan_b;
	struct	zschan zs_chan_a;
};

/* Saved PROM mappings */
struct zsdevice *zsaddr[NZS];

/* Flags from cninit() */
int zs_hwflags[NZS][2];

/* Default speed for each channel */
int zs_defspeed[NZS][2] = {
	{ 9600, 	/* ttya */
	  9600 },	/* ttyb */
	{ 1200, 	/* keyboard */
	  1200 },	/* mouse */
	{ 9600, 	/* ttyc */
	  9600 },	/* ttyd */
};

u_char zs_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	0,	/* 1: No interrupts yet. */
	0,	/* 2: IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE | ZSWR9_NO_VECTOR,
	0,	/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	((PCLK/32)/9600)-2,	/*12: BAUDLO (default=9600) */
	0,			/*13: BAUDHI (default=9600) */
	ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK,
	ZSWR15_BREAK_IE /* | ZSWR15_DCD_IE */,
};

struct zschan *
zs_get_chan_addr(zs_unit, channel)
	int zs_unit, channel;
{
	struct zsdevice *addr;
	struct zschan *zc;

	if (zs_unit >= NZS)
		return NULL;
	addr = zsaddr[zs_unit];
	if (addr == NULL)
		addr = zsaddr[zs_unit] = findzs(zs_unit);
	if (addr == NULL)
		return NULL;
	if (channel == 0) {
		zc = &addr->zs_chan_a;
	} else {
		zc = &addr->zs_chan_b;
	}
	return (zc);
}


/****************************************************************
 * Autoconfig
 ****************************************************************/

/* Definition of the driver for autoconfig. */
int	zs_match(struct device *, void *, void *);
void	zs_attach(struct device *, struct device *, void *);
int	zs_print(void *, const char *nam);

/* Power management hooks (for Tadpole SPARCbooks) */
void	zs_disable(struct zs_chanstate *);
int	zs_enable(struct zs_chanstate *);

struct cfattach zs_ca = {
	sizeof(struct zsc_softc), zs_match, zs_attach
};

struct cfdriver zs_cd = {
	NULL, "zs", DV_DULL
};

/* Interrupt handlers. */
int zshard(void *);
int zssoft(void *);
struct intrhand levelhard = { zshard };
struct intrhand levelsoft = { zssoft };

int zs_get_speed(struct zs_chanstate *);


/*
 * Is the zs chip present?
 */
int
zs_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = (struct cfdata *)vcf;
	struct confargs *ca = (struct confargs *)aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

#ifdef solbourne
	if (CPU_ISKAP)
		return (ca->ca_bustype == BUS_OBIO);
#endif

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
void
zs_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct zsc_softc *zsc = (void *) self;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	struct zsc_attach_args zsc_args;
	volatile struct zschan *zc;
	struct zs_chanstate *cs;
	int pri, s, zs_unit, channel;
	static int didintr, prevpri;

	zs_unit = zsc->zsc_dev.dv_unit;

	/* Use the mapping setup by the Sun PROM. */
	if (zsaddr[zs_unit] == NULL)
		zsaddr[zs_unit] = findzs(zs_unit);

	if (ca->ca_bustype==BUS_MAIN)
		if ((void*)zsaddr[zs_unit] != ra->ra_vaddr)
			panic("zsattach");
	if (ra->ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ra->ra_nintr);
		return;
	}
	pri = ra->ra_intr[0].int_pri;
	printf(" pri %d, softpri %d\n", pri, IPL_TTY);

	/*
	 * Initialize software state for each channel.
	 */
	for (channel = 0; channel < 2; channel++) {
		zsc_args.type = "serial";
		/* XXX hardcoded */
		if (zs_unit == 1) {
			if (channel == 0)
				zsc_args.type = "keyboard";
			if (channel == 1)
				zsc_args.type = "mouse";
		}

		zsc_args.channel = channel;
		zsc_args.hwflags = zs_hwflags[zs_unit][channel];
		cs = &zsc->zsc_cs[channel];

		cs->cs_channel = channel;
		cs->cs_private = NULL;
		cs->cs_ops = &zsops_null;
		cs->cs_brg_clk = PCLK / 16;

		zc = zs_get_chan_addr(zs_unit, channel);

		cs->cs_reg_csr  = &zc->zc_csr;
		cs->cs_reg_data = &zc->zc_data;

		bcopy(zs_init_reg, cs->cs_creg, 16);
		bcopy(zs_init_reg, cs->cs_preg, 16);

		/* XXX: Get these from the PROM properties! */
		/* XXX: See the mvme167 code.  Better. */
		if (zsc_args.hwflags & ZS_HWFLAG_CONSOLE)
			cs->cs_defspeed = zs_get_speed(cs);
		else
			cs->cs_defspeed = zs_defspeed[zs_unit][channel];
		cs->cs_defcflag = zs_def_cflag;

		/* Make these correspond to cs_defcflag (-crtscts) */
		cs->cs_rr0_dcd = ZSRR0_DCD;
		cs->cs_rr0_cts = 0;
		cs->cs_wr5_dtr = ZSWR5_DTR | ZSWR5_RTS;
		cs->cs_wr5_rts = 0;

		/*
		 * Clear the master interrupt enable.
		 * The INTENA is common to both channels,
		 * so just do it on the A channel.
		 */
		if (channel == 0) {
			zs_write_reg(cs, 9, 0);
		}

		/*
		 * Look for a child driver for this channel.
		 * The child attach will setup the hardware.
		 */
		if (!config_found(self, (void *)&zsc_args, zs_print)) {
			/* No sub-driver.  Just reset it. */
			u_char reset = (channel == 0) ?
				ZSWR9_A_RESET : ZSWR9_B_RESET;
			s = splzs();
			zs_write_reg(cs,  9, reset);
			splx(s);
		}
	}

	/*
	 * Now safe to install interrupt handlers.  Note the arguments
	 * to the interrupt handlers aren't used.  Note, we only do this
	 * once since both SCCs interrupt at the same level and vector.
	 */
	if (!didintr) {
		didintr = 1;
		prevpri = pri;
		intr_establish(pri, &levelhard, IPL_ZS, self->dv_xname);
		intr_establish(IPL_TTY, &levelsoft, IPL_TTY, self->dv_xname);
	} else if (pri != prevpri)
		panic("broken zs interrupt scheme");

	/*
	 * Set the master interrupt enable and interrupt vector.
	 * (common to both channels, do it on A)
	 */
	cs = &zsc->zsc_cs[0];
	s = splhigh();
	/* interrupt vector */
	zs_write_reg(cs, 2, zs_init_reg[2]);
	/* master interrupt control (enable) */
	zs_write_reg(cs, 9, zs_init_reg[9]);
	splx(s);

#ifdef SUN4M
	/* register power management routines if necessary */
	if (CPU_ISSUN4M) {
		if (getpropint(ra->ra_node, "pwr-on-auxio2", 0))
			for (channel = 0; channel < 2; channel++) {
				cs = &zsc->zsc_cs[channel];
				cs->disable = zs_disable;
				cs->enable = zs_enable;
				cs->enabled = 0;
			}
	}
#endif

#if 0
	/*
	 * XXX: L1A hack - We would like to be able to break into
	 * the debugger during the rest of autoconfiguration, so
	 * lower interrupts just enough to let zs interrupts in.
	 * This is done after both zs devices are attached.
	 */
	if (zs_unit == 1) {
		printf("zs1: enabling zs interrupts\n");
		(void)splfd(); /* XXX: splzs - 1 */
	}
#endif
}

int
zs_print(aux, name)
	void *aux;
	const char *name;
{
	struct zsc_attach_args *args = aux;

	if (name != NULL)
		printf("%s:", name);

	if (args->channel != -1)
		printf(" channel %d", args->channel);

	return UNCONF;
}

volatile int zssoftpending;

/*
 * Our ZS chips all share a common, autovectored interrupt,
 * so we have to look at all of them on each interrupt.
 */
int
zshard(arg)
	void *arg;
{
	struct zsc_softc *zsc;
	int unit, rr3, rval, softreq;

	rval = softreq = 0;
	for (unit = 0; unit < zs_cd.cd_ndevs; unit++) {
		zsc = zs_cd.cd_devs[unit];
		if (zsc == NULL)
			continue;
		rr3 = zsc_intr_hard(zsc);
		/* Count up the interrupts. */
		if (rr3) {
			rval |= rr3;
		}
		softreq |= zsc->zsc_cs[0].cs_softreq;
		softreq |= zsc->zsc_cs[1].cs_softreq;
	}

	/* We are at splzs here, so no need to lock. */
	if (softreq && (zssoftpending == 0)) {
		zssoftpending = IE_ZSSOFT;
#if defined(SUN4M)
		if (CPU_ISSUN4M)
			raise(0, IPL_TTY);
		else
#endif
		ienab_bis(IE_ZSSOFT);
	}
	return (rval);
}

/*
 * Similar scheme as for zshard (look at all of them)
 */
int
zssoft(arg)
	void *arg;
{
	struct zsc_softc *zsc;
	int s, unit;

	/* This is not the only ISR on this IPL. */
	if (zssoftpending == 0)
		return (0);

	/*
	 * The soft intr. bit will be set by zshard only if
	 * the variable zssoftpending is zero.  The order of
	 * these next two statements prevents our clearing
	 * the soft intr bit just after zshard has set it.
	 */
	/* ienab_bic(IE_ZSSOFT); */
	zssoftpending = 0;

	/* Make sure we call the tty layer at spltty. */
	s = spltty();
	for (unit = 0; unit < zs_cd.cd_ndevs; unit++) {
		zsc = zs_cd.cd_devs[unit];
		if (zsc == NULL)
			continue;
		(void)zsc_intr_soft(zsc);
	}
	splx(s);
	return (1);
}


/*
 * Compute the current baud rate given a ZS channel.
 */
int
zs_get_speed(cs)
	struct zs_chanstate *cs;
{
	int tconst;

	tconst = zs_read_reg(cs, 12);
	tconst |= zs_read_reg(cs, 13) << 8;
	return (TCONST_TO_BPS(cs->cs_brg_clk, tconst));
}

/*
 * MD functions for setting the baud rate and control modes.
 */
int
zs_set_speed(cs, bps)
	struct zs_chanstate *cs;
	int bps;	/* bits per second */
{
	int tconst, real_bps;

	if (bps == 0)
		return (0);

#ifdef	DIAGNOSTIC
	if (cs->cs_brg_clk == 0)
		panic("zs_set_speed");
#endif

	tconst = BPS_TO_TCONST(cs->cs_brg_clk, bps);
	if (tconst < 0)
		return (EINVAL);

	/* Convert back to make sure we can do it. */
	real_bps = TCONST_TO_BPS(cs->cs_brg_clk, tconst);

	/* XXX - Allow some tolerance here? */
	if (real_bps != bps)
		return (EINVAL);

	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;

	/* Caller will stuff the pending registers. */
	return (0);
}

int
zs_set_modes(cs, cflag)
	struct zs_chanstate *cs;
	int cflag;	/* bits per second */
{
	int s;

	/*
	 * Output hardware flow control on the chip is horrendous:
	 * if carrier detect drops, the receiver is disabled, and if
	 * CTS drops, the transmitter is stopped IN MID CHARACTER!
	 * Therefore, NEVER set the HFC bit, and instead use the
	 * status interrupt to detect CTS changes.
	 */
	s = splzs();
	cs->cs_rr0_pps = 0;
	if ((cflag & (CLOCAL | MDMBUF)) != 0) {
		cs->cs_rr0_dcd = 0;
		if ((cflag & MDMBUF) == 0)
			cs->cs_rr0_pps = ZSRR0_DCD;
	} else
		cs->cs_rr0_dcd = ZSRR0_DCD;
	if ((cflag & CRTSCTS) != 0) {
		cs->cs_wr5_dtr = ZSWR5_DTR;
		cs->cs_wr5_rts = ZSWR5_RTS;
		cs->cs_rr0_cts = ZSRR0_CTS;
#if 0 /* JLW */
	} else if ((cflag & CDTRCTS) != 0) {
		cs->cs_wr5_dtr = 0;
		cs->cs_wr5_rts = ZSWR5_DTR;
		cs->cs_rr0_cts = ZSRR0_CTS;
#endif
	} else if ((cflag & MDMBUF) != 0) {
		cs->cs_wr5_dtr = 0;
		cs->cs_wr5_rts = ZSWR5_DTR;
		cs->cs_rr0_cts = ZSRR0_DCD;
	} else {
		cs->cs_wr5_dtr = ZSWR5_DTR | ZSWR5_RTS;
		cs->cs_wr5_rts = 0;
		cs->cs_rr0_cts = 0;
	}
	splx(s);

	/* Caller will stuff the pending registers. */
	return (0);
}


/*
 * Read or write the chip with suitable delays.
 */

u_char
zs_read_reg(cs, reg)
	struct zs_chanstate *cs;
	u_char reg;
{
	u_char val;

	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	val = *cs->cs_reg_csr;
	ZS_DELAY();
	return val;
}

void
zs_write_reg(cs, reg, val)
	struct zs_chanstate *cs;
	u_char reg, val;
{
	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

u_char zs_read_csr(cs)
	struct zs_chanstate *cs;
{
	register u_char val;

	val = *cs->cs_reg_csr;
	ZS_DELAY();
	return val;
}

void  zs_write_csr(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

u_char zs_read_data(cs)
	struct zs_chanstate *cs;
{
	register u_char val;

	val = *cs->cs_reg_data;
	ZS_DELAY();
	return val;
}

void  zs_write_data(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	*cs->cs_reg_data = val;
	ZS_DELAY();
}

#ifdef SUN4M
/*
 * Power management hooks for zsopen() and zsclose().
 * We use them to power on/off the ports on the Tadpole SPARCbook machines
 * (on other sun4m machines, this is a no-op).
 */

/*
 * Since the serial power control is global, we need to remember which channels
 * have their ports open, so as not to power off when closing one channel if
 * both were open. Simply xor'ing the zs_chanstate pointers is enough to let us
 * know if the serial lines are used or not.
 */
static vaddr_t zs_sb_enable = 0;

int
zs_enable(struct zs_chanstate *cs)
{
	if (cs->enabled == 0) {
		if (zs_sb_enable == 0)
			sb_auxregbisc(1, AUXIO2_SERIAL, 0);
		zs_sb_enable ^= (vaddr_t)cs;
		cs->enabled = 1;
	}
	return (0);
}

void
zs_disable(struct zs_chanstate *cs)
{
	if (cs->enabled != 0) {
		cs->enabled = 0;
		zs_sb_enable ^= (vaddr_t)cs;
		if (zs_sb_enable == 0)
			sb_auxregbisc(1, 0, AUXIO2_SERIAL);
	}
}

#endif	/* SUN4M */

/****************************************************************
 * Console support functions (Sun specific!)
 * Note: this code is allowed to know about the layout of
 * the chip registers, and uses that to keep things simple.
 * XXX - I think I like the mvme167 code better. -gwr
 ****************************************************************/

extern void Debugger(void);
void *zs_conschan;

/*
 * Handle user request to enter kernel debugger.
 */
void
zs_abort(cs)
	struct zs_chanstate *cs;
{
	volatile struct zschan *zc = zs_conschan;
	int rr0;

	/* Wait for end of break to avoid PROM abort. */
	/* XXX - Limit the wait? */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while (rr0 & ZSRR0_BREAK);

#if defined(KGDB)
	zskgdb(cs);
#elif defined(DDB)
	{
		extern int db_active;

		if (!db_active)
			Debugger();
		else
			/* Debugger is probably hosed */
			callrom();
	}
#else
	printf("stopping on keyboard abort\n");
	callrom();
#endif
}

/*
 * Polled input char.
 */
int
zs_getc(arg)
	void *arg;
{
	volatile struct zschan *zc = arg;
	int s, c, rr0;

	s = splhigh();
	/* Wait for a character to arrive. */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_RX_READY) == 0);

	c = zc->zc_data;
	ZS_DELAY();
	splx(s);

	return (c);
}

/*
 * Polled output char.
 */
void
zs_putc(arg, c)
	void *arg;
	int c;
{
	volatile struct zschan *zc = arg;
	int s, rr0;

	s = splhigh();
	/* Wait for transmitter to become ready. */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_TX_READY) == 0);

	/*
	 * Send the next character.
	 * Now you'd think that this could be followed by a ZS_DELAY()
	 * just like all the other chip accesses, but it turns out that
	 * the `transmit-ready' interrupt isn't de-asserted until
	 * some period of time after the register write completes
	 * (more than a couple instructions).  So to avoid stray
	 * interrupts we put in the 2us delay regardless of cpu model.
	 */
        zc->zc_data = c;
        delay(2);

	splx(s);
}

/*****************************************************************/

cons_decl(zs);

/*
 * Console table shared by ttya, ttyb
 */
struct consdev consdev_tty = {
	zscnprobe,
	zscninit,
	zscngetc,
	zscnputc,
	zscnpollc,
};

int zstty_unit;	/* set in consinit() */

void
zscnprobe(cn)
	struct consdev *cn;
{
	cn->cn_dev = makedev(zs_major, zstty_unit);
	cn->cn_pri = CN_REMOTE;
}

void
zscninit(cn)
	struct consdev *cn;
{
}

/*
 * Polled console input putchar.
 */
int
zscngetc(dev)
	dev_t dev;
{
	return (zs_getc(zs_conschan));
}

/*
 * Polled console output putchar.
 */
void
zscnputc(dev, c)
	dev_t dev;
	int c;
{
	zs_putc(zs_conschan, c);
}

int swallow_zsintrs;

void
zscnpollc(dev, on)
	dev_t dev;
	int on;
{
	/*
	 * Need to tell zs driver to acknowledge all interrupts or we get
	 * annoying spurious interrupt messages.  This is because mucking
	 * with spl() levels during polling does not prevent interrupts from
	 * being generated.
	 */

	if (on) swallow_zsintrs++;
	else swallow_zsintrs--;
}

/*****************************************************************/

#if defined(SUN4) || defined(SUN4C) || defined(SUN4M)

cons_decl(prom);

/*
 * The console is set to this one initially,
 * which lets us use the PROM until consinit()
 * is called to select a real console.
 */
struct consdev consdev_prom = {
	promcnprobe,
	promcninit,
	promcngetc,
	promcnputc,
	nullcnpollc,
};

/*
 * The console table pointer is statically initialized
 * to point to the PROM (output only) table, so that
 * early calls to printf will work.
 */
struct consdev *cn_tab = &consdev_prom;

void
promcnprobe(cn)
	struct consdev *cn;
{
	cn->cn_dev = makedev(0, 0);
	cn->cn_pri = CN_INTERNAL;
}

void
promcninit(cn)
	struct consdev *cn;
{
}

/*
 * PROM console input putchar.
 */
int
promcngetc(dev)
	dev_t dev;
{
	int s, c;

	if (promvec->pv_romvec_vers > 2) {
		int n = 0;
		unsigned char c0;

		s = splhigh();
		while (n <= 0) {
			n = (*promvec->pv_v2devops.v2_read)
			        (*promvec->pv_v2bootargs.v2_fd0, &c0, 1);
		}
		splx(s);

		c = c0;
	} else {
#if defined(SUN4)
		/* SUN4 PROM: must turn off local echo */
		extern struct om_vector *oldpvec;
		int saveecho = 0;
#endif
		s = splhigh();
#if defined(SUN4)
		if (CPU_ISSUN4) {
			saveecho = *(oldpvec->echo);
			*(oldpvec->echo) = 0;
		}
#endif
		c = (*promvec->pv_getchar)();
#if defined(SUN4)
		if (CPU_ISSUN4)
			*(oldpvec->echo) = saveecho;
#endif
		splx(s);
	}

	if (c == '\r')
		c = '\n';

	return (c);
}

/*
 * PROM console output putchar.
 */
void
promcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s;
	char c0 = (c & 0x7f);

	s = splhigh();
	if (promvec->pv_romvec_vers > 2)
		(*promvec->pv_v2devops.v2_write)
			(*promvec->pv_v2bootargs.v2_fd1, &c0, 1);
	else
		(*promvec->pv_putchar)(c);
	splx(s);
}

#endif	/* SUN4 || SUN4C || SUN4M */

/*****************************************************************/

#if 0
extern struct consdev consdev_kd;
#endif

char *prom_inSrc_name[] = {
	"keyboard/display",
	"ttya", "ttyb",
	"ttyc", "ttyd" };

/*
 * This function replaces sys/dev/cninit.c
 * Determine which device is the console using
 * the PROM "input source" and "output sink".
 */
void
consinit()
{
	struct zschan *zc;
	struct consdev *console = cn_tab;
	int channel, zs_unit;
	int inSource, outSink;

#if defined(SUN4) || defined(SUN4C) || defined(SUN4M)
	if (promvec->pv_romvec_vers > 2) {
		/* We need to probe the PROM device tree */
		int node,fd;
		char buffer[128];
		struct nodeops *no;
		struct v2devops *op;
		char *cp;
		extern int fbnode;

		inSource = outSink = -1;
		no = promvec->pv_nodeops;
		op = &promvec->pv_v2devops;

		node = findroot();
		if (no->no_proplen(node, "stdin-path") >= sizeof(buffer)) {
			printf("consinit: increase buffer size and recompile\n");
			goto setup_output;
		}
		/* XXX: fix above */

		no->no_getprop(node, "stdin-path",buffer);

		/*
		 * Open an "instance" of this device.
		 * You'd think it would be appropriate to call v2_close()
		 * on the handle when we're done with it. But that seems
		 * to cause the device to shut down somehow; for the moment,
		 * we simply leave it open...
		 */
		if ((fd = op->v2_open(buffer)) == 0 ||
		     (node = op->v2_fd_phandle(fd)) == 0) {
			printf("consinit: bogus stdin path %s.\n",buffer);
			goto setup_output;
		}
		if (no->no_proplen(node,"keyboard") >= 0) {
			inSource = PROMDEV_KBD;
			goto setup_output;
		}
		if (strcmp(getpropstring(node,"device_type"),"serial") != 0) {
			/* not a serial, not keyboard. what is it?!? */
			inSource = -1;
			goto setup_output;
		}
		/*
		 * At this point we assume the device path is in the form
		 *   ....device@x,y:a for ttya and ...device@x,y:b for ttyb.
		 * If it isn't, we defer to the ROM
		 */
		cp = buffer;
		while (*cp)
		    cp++;
		cp -= 2;
#ifdef DEBUG
		if (cp < buffer)
		    panic("consinit: bad stdin path %s",buffer);
#endif
		/* XXX: only allows tty's a->z, assumes PROMDEV_TTYx contig */
		if (cp[0]==':' && cp[1] >= 'a' && cp[1] <= 'z')
		    inSource = PROMDEV_TTYA + (cp[1] - 'a');
		/* else use rom */
setup_output:
		node = findroot();
		if (no->no_proplen(node, "stdout-path") >= sizeof(buffer)) {
			printf("consinit: increase buffer size and recompile\n");
			goto setup_console;
		}
		/* XXX: fix above */

		no->no_getprop(node, "stdout-path", buffer);

		if ((fd = op->v2_open(buffer)) == 0 ||
		     (node = op->v2_fd_phandle(fd)) == 0) {
			printf("consinit: bogus stdout path %s.\n",buffer);
			goto setup_output;
		}
		if (strcmp(getpropstring(node,"device_type"),"display") == 0) {
			/* frame buffer output */
			outSink = PROMDEV_SCREEN;
			fbnode = node;
		} else if (strcmp(getpropstring(node,"device_type"), "serial")
			   != 0) {
			/* not screen, not serial. Whatzit? */
			outSink = -1;
		} else { /* serial console. which? */
			/*
			 * At this point we assume the device path is in the
			 * form:
			 * ....device@x,y:a for ttya, etc.
			 * If it isn't, we defer to the ROM
			 */
			cp = buffer;
			while (*cp)
			    cp++;
			cp -= 2;
#ifdef DEBUG
			if (cp < buffer)
				panic("consinit: bad stdout path %s",buffer);
#endif
			/* XXX: only allows tty's a->z, assumes PROMDEV_TTYx contig */
			if (cp[0]==':' && cp[1] >= 'a' && cp[1] <= 'z')
			    outSink = PROMDEV_TTYA + (cp[1] - 'a');
			else outSink = -1;
		}
	} else {
		inSource = *promvec->pv_stdin;
		outSink  = *promvec->pv_stdout;
	}
#endif	/* SUN4 || SUN4C || SUN4M */
#ifdef solbourne
	if (CPU_ISKAP) {
		const char *dev;

		inSource = PROMDEV_TTYA;	/* default */
		dev = prom_getenv(ENV_INPUTDEVICE);
		if (dev != NULL) {
			if (strcmp(dev, "ttyb") == 0)
				inSource = PROMDEV_TTYB;
			if (strcmp(dev, "keyboard") == 0)
				inSource = PROMDEV_KBD;
		}

		outSink = PROMDEV_TTYA;	/* default */
		dev = prom_getenv(ENV_OUTPUTDEVICE);
		if (dev != NULL) {
			if (strcmp(dev, "ttyb") == 0)
				outSink = PROMDEV_TTYB;
			if (strcmp(dev, "screen") == 0)
				outSink = PROMDEV_SCREEN;
		}
	}
#endif

#if defined(SUN4) || defined(SUN4C) || defined(SUN4M)
setup_console:
#endif

	if (inSource != outSink) {
		printf("cninit: mismatched PROM output selector\n");
	}

	switch (inSource) {
	default:
		printf("cninit: invalid inSource=%d\n", inSource);
		callrom();
		inSource = PROMDEV_KBD;
		/* FALLTHROUGH */

	case PROMDEV_KBD: /* keyboard/display */
#if NZSKBD > 0
		zs_unit = 1;
		channel = 0;
		break;
#else	/* NZSKBD */
		printf("cninit: kdb/display not configured\n");
		callrom();
		inSource = PROMDEV_TTYA;
		/* FALLTHROUGH */
#endif	/* NZSKBD */

	case PROMDEV_TTYA:
	case PROMDEV_TTYB:
		zstty_unit = inSource - PROMDEV_TTYA;
		zs_unit = 0;
		channel = zstty_unit & 1;
		console = &consdev_tty;
		break;

	}
	/* Now that inSource has been validated, print it. */
	printf("console is %s\n", prom_inSrc_name[inSource]);

	zc = zs_get_chan_addr(zs_unit, channel);
	if (zc == NULL) {
		printf("cninit: zs not mapped.\n");
		return;
	}
	zs_conschan = zc;
	zs_hwflags[zs_unit][channel] = ZS_HWFLAG_CONSOLE;
	/* switch to selected console */
	cn_tab = console;
	(*cn_tab->cn_probe)(cn_tab);
	(*cn_tab->cn_init)(cn_tab);
#ifdef	KGDB
	zs_kgdb_init();
#endif
}
