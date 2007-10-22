/*	$OpenBSD: zs.c,v 1.24 2007/10/22 14:46:46 jsing Exp $	*/
/*	$NetBSD: zs.c,v 1.19 1998/01/12 19:22:18 thorpej Exp $	*/

/*
 * Copyright (c) 1996-1998 Bill Studenmund
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
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
 * Zilog Z8530 Dual UART driver (machine-dependent part)
 *
 * Runs two serial lines per chip using slave drivers.
 * Plain tty/async lines use the zs_async slave.
 * Sun keyboard/mouse uses the zs_kbd/zs_ms slaves.
 * Other ports use their own mice & keyboard slaves.
 *
 * Credits & history:
 *
 * With NetBSD 1.1, port-mac68k started using a port of the port-sparc
 * (port-sun3?) zs.c driver (which was in turn based on code in the
 * Berkeley 4.4 Lite release). Bill Studenmund did the port, with
 * help from Allen Briggs and Gordon Ross <gwr@netbsd.org>. Noud de
 * Brouwer field-tested the driver at a local ISP.
 *
 * Bill Studenmund and Gordon Ross then ported the machine-independant
 * z8530 driver to work with port-mac68k. NetBSD 1.2 contained an
 * intermediate version (mac68k using a local, patched version of
 * the m.i. drivers), with NetBSD 1.3 containing a full version.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/cons.h>

#include <mac68k/dev/z8530reg.h>
#include <machine/z8530var.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/psc.h>
#include <machine/viareg.h>

#ifdef DDB
#include <ddb/db_var.h>
#endif

/* Are these in a header file anywhere? */
/* Booter flags interface */
#define ZSMAC_RAW	0x01
#define ZSMAC_LOCALTALK	0x02
#define	ZS_STD_BRG	(57600*4)

#include "zsc.h"	/* get the # of zs chips defined */

/*
 * Some warts needed by z8530tty.c -
 */
int zs_def_cflag = (CREAD | CS8 | HUPCL);
int zs_major = 12;

/*
 * abort detection on console will now timeout after iterating on a loop
 * the following # of times. Cheep hack. Also, abort detection is turned
 * off after a timeout (i.e. maybe there's not a terminal hooked up).
 */
#define ZSABORT_DELAY 3000000

/*
 * Define interrupt levels.
 */
#define ZSHARD_PRI	4	/* Wired on the CPU board... */
/*
 * Serial port cards with zs chips on them are actually at the
 * NuBus interrupt level, which is lower than 4. But blocking
 * level 4 interrupts will block those interrupts too, so level
 * 4 is fine.
 */

/* The layout of this is hardware-dependent (padding, order). */
struct zschan {
	volatile u_char	zc_csr;		/* ctrl,status, and indirect access */
	u_char		zc_xxx0;
	u_char		zc_xxx1;	/* part of the other channel lives here!*/
	u_char		zc_xxx2;	/* Yea Apple! */
	volatile u_char	zc_data;	/* data */
	u_char		zc_xxx3;
	u_char		zc_xxx4;
	u_char		zc_xxx5;
};

/* Saved PROM mappings */
static char *zsaddr[NZSC];	/* See zs_init() */
/* Flags from cninit() */
static int zs_hwflags[NZSC][2];
/* Default speed for each channel */
static int zs_defspeed[NZSC][2] = {
	{ 9600, 	/* tty00 */
	  9600 },	/* tty01 */
};
void	*zs_conschan = 0;
int	zs_consunit;
/* device to which the console is attached--if serial. */
dev_t	mac68k_zsdev;
/* Mac stuff */
volatile unsigned char *sccA = 0;
int	nzsc_attached = 0;	/* needed as long as we have spurious
				 * interrupt problems.
				 */

static struct zschan	*zs_get_chan_addr(int zsc_unit, int channel);
void			zs_init(void);
int			zs_cn_check_speed(int bps);

static struct zschan *
zs_get_chan_addr(zsc_unit, channel)
	int zsc_unit, channel;
{
	char *addr;
	struct zschan *zc;

	if (zsc_unit >= NZSC)
		return NULL;
	addr = zsaddr[zsc_unit];
	if (addr == NULL)
		return NULL;
	if (channel == 0) {
		zc = (struct zschan *)(addr +2);
		/* handle the fact the ports are intertwined. */
	} else {
		zc = (struct zschan *)(addr);
	}
	return (zc);
}


/* Find PROM mappings (for console support). */
int zsinited = 0; /* 0 = not, 1 = inited, not attached, 2= attached */

void
zs_init()
{
	if ((zsinited == 2)&&(zsaddr[0] != (char *) sccA))
		panic("Moved zs0 address after attached!");
	zsaddr[0] = (char *) sccA;
	zsinited = 1;
	if (zs_conschan != 0){ /* we might have moved io under the console */
		zs_conschan = zs_get_chan_addr(0, zs_consunit);
		/* so recalc the console port */
	}
}	


/*
 * Even though zsparam will set up the clock multiples, etc., we
 * still set them here as: 1) mice & keyboards don't use zsparam,
 * and 2) the console stuff uses these defaults before device
 * attach.
 */

static u_char zs_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	ZSWR1_RIE | ZSWR1_TIE | ZSWR1_SIE,
	0x18 + ZSHARD_PRI,	/* IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE,
	0,	/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	14,	/*12: BAUDLO (default=9600) */
	0,	/*13: BAUDHI (default=9600) */
	ZSWR14_BAUD_ENA,
	ZSWR15_BREAK_IE | ZSWR15_DCD_IE | ZSWR15_CTS_IE,
};


/****************************************************************
 * Autoconfig
 ****************************************************************/

/* Definition of the driver for autoconfig. */
static int	zsc_match(struct device *, void *, void *);
static void	zsc_attach(struct device *, struct device *, void *);
static int	zsc_print(void *, const char *name);

struct cfattach zsc_ca = {
	sizeof(struct zsc_softc), zsc_match, zsc_attach
};

struct cfdriver zsc_cd = {
	NULL, "zsc", DV_DULL
};

int zshard(void *);
int zssoft(void *);


/*
 * Is the zs chip present?
 */
static int
zsc_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	return 1;
}

/*
 * Attach a found zs.
 *
 * Match slave number to zs unit number, so that misconfiguration will
 * not set up the keyboard as ttya, etc.
 */
static void
zsc_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct zsc_softc *zsc = (void *)self;
	struct zsc_attach_args zsc_args;
	volatile struct zschan *zc;
	struct xzs_chanstate *xcs;
	struct zs_chanstate *cs;
	int zsc_unit, channel;
	int s, chip, theflags;

	if (!zsinited)
		zs_init();	
	zsinited = 2;

	zsc_unit = zsc->zsc_dev.dv_unit;

	/* Make sure everything's inited ok. */
	if (zsaddr[zsc_unit] == NULL)
		panic("zs_attach: zs%d not mapped", zsc_unit);

	chip = 0;
#ifdef DEBUG
	printf(" chip type %d",chip);
#endif
	printf("\n");

	/*
	 * Initialize software state for each channel.
	 */
	for (channel = 0; channel < 2; channel++) {
		zsc_args.channel = channel;
		zsc_args.hwflags = zs_hwflags[zsc_unit][channel];
		xcs = &zsc->xzsc_xcs_store[channel];
		cs  = &xcs->xzs_cs;
		zsc->zsc_cs[channel] = cs;

		cs->cs_channel = channel;
		cs->cs_private = NULL;
		cs->cs_ops = &zsops_null;

		zc = zs_get_chan_addr(zsc_unit, channel);
		cs->cs_reg_csr  = &zc->zc_csr;
		cs->cs_reg_data = &zc->zc_data;

		if (channel == 0) /* Double check interrupts are off */
			zs_write_reg(cs, 9, 0);

		bcopy(zs_init_reg, cs->cs_creg, 16);
		bcopy(zs_init_reg, cs->cs_preg, 16);

		/* Current BAUD rate generator clock. */
		cs->cs_brg_clk = ZS_STD_BRG;	/* RTxC is 230400*16, so use 230400 */
		cs->cs_defspeed = zs_defspeed[zsc_unit][channel];
		cs->cs_defcflag = zs_def_cflag;

		/* Make these correspond to cs_defcflag (-crtscts) */
		cs->cs_rr0_dcd = ZSRR0_DCD;
		cs->cs_rr0_cts = 0;
		cs->cs_wr5_dtr = ZSWR5_DTR;
		cs->cs_wr5_rts = 0;

#ifdef __notyet__
		cs->cs_slave_type = ZS_SLAVE_NONE;
#endif

		/* Define BAUD rate stuff. */
		xcs->cs_clocks[0].clk = ZS_STD_BRG * 16;
		xcs->cs_clocks[0].flags = ZSC_RTXBRG;
		xcs->cs_clocks[1].flags =
			ZSC_RTXBRG | ZSC_RTXDIV | ZSC_VARIABLE | ZSC_EXTERN;
		xcs->cs_clocks[2].flags = ZSC_TRXDIV | ZSC_VARIABLE;
		xcs->cs_clock_count = 3;

		if (channel == 0) {
			theflags = mac68k_machine.modem_flags;
			xcs->cs_clocks[1].clk = mac68k_machine.modem_dcd_clk;
			xcs->cs_clocks[2].clk = mac68k_machine.modem_cts_clk;
		} else {
			theflags = mac68k_machine.print_flags;
			xcs->cs_clocks[1].flags = ZSC_VARIABLE;
			/*
			 * Yes, we aren't defining ANY clock source enables for
			 * the printer's DCD clock in. The hardware won't let
			 * us use it. But a clock will freak out the chip, so
			 * we let you set it, telling us to bar interrupts on
			 * the line.
			 */
			xcs->cs_clocks[1].clk = mac68k_machine.print_dcd_clk;
			xcs->cs_clocks[2].clk = mac68k_machine.print_cts_clk;
		}
		if (xcs->cs_clocks[1].clk)
			zsc_args.hwflags |= ZS_HWFLAG_NO_DCD;
		if (xcs->cs_clocks[2].clk)
			zsc_args.hwflags |= ZS_HWFLAG_NO_CTS;

#ifdef DEBUG
		printf("zsc%d channel %d: d_speed %6d DCD clk %ld CTS clk %ld",
			zsc_unit, channel, cs->cs_defspeed,
			xcs->cs_clocks[1].clk, xcs->cs_clocks[2].clk);
#endif

		/* Set defaults in our "extended" chanstate. */
		xcs->cs_csource = 0;
		xcs->cs_psource = 0;
		xcs->cs_cclk_flag = 0;	/* Nothing fancy by default */
		xcs->cs_pclk_flag = 0;

		if (theflags & ZSMAC_RAW) {
			zsc_args.hwflags |= ZS_HWFLAG_RAW;
#ifdef DEBUG
			printf(" (raw defaults)");
#endif
		}

		/*
		 * XXX - This might be better done with a "stub" driver
		 * (to replace zstty) that ignores LocalTalk for now.
		 */
		if (theflags & ZSMAC_LOCALTALK) {
			printf(" shielding from LocalTalk");
			cs->cs_defspeed = 1;
			cs->cs_creg[ZSRR_BAUDLO] = cs->cs_preg[ZSRR_BAUDLO] = 0xff;
			cs->cs_creg[ZSRR_BAUDHI] = cs->cs_preg[ZSRR_BAUDHI] = 0xff;
			zs_write_reg(cs, ZSRR_BAUDLO, 0xff);
			zs_write_reg(cs, ZSRR_BAUDHI, 0xff);
			/*
			 * If we might have LocalTalk, then make sure we have
			 * the Baud rate low-enough to not do any damage.
			 */
		}

		/*
		 * We used to disable chip interrupts here, but we now
		 * do that in zscnprobe, just in case MacOS left the chip on.
		 */

		xcs->cs_chip = chip;

		/* Stash away a copy of the final H/W flags. */
		xcs->cs_hwflags = zsc_args.hwflags;

#ifdef DEBUG
		printf("\n");
#endif

		/*
		 * Look for a child driver for this channel.
		 * The child attach will setup the hardware.
		 */
		if (!config_found(self, (void *)&zsc_args, zsc_print)) {
			/* No sub-driver.  Just reset it. */
			u_char reset = (channel == 0) ?
				ZSWR9_A_RESET : ZSWR9_B_RESET;
			s = splzs();
			zs_write_reg(cs,  9, reset);
			splx(s);
		}
	}

	if (current_mac_model->class == MACH_CLASSAV) {
		add_psc_lev4_intr(PSCINTR_SCCA, zshard, zsc);
		add_psc_lev4_intr(PSCINTR_SCCB, zshard, zsc);
	} else {
		intr_establish(zshard, zsc, ZSHARD_PRI, self->dv_xname);
	}

	/* XXX - Now safe to install interrupt handlers. */

	/*
	 * Set the master interrupt enable and interrupt vector.
	 * (common to both channels, do it on A)
	 */
	cs = zsc->zsc_cs[0];
	s = splzs();
	/* interrupt vector */
	zs_write_reg(cs, 2, zs_init_reg[2]);
	/* master interrupt control (enable) */
	zs_write_reg(cs, 9, zs_init_reg[9]);
	nzsc_attached++;
	splx(s);
}

static int
zsc_print(aux, name)
	void *aux;
	const char *name;
{
	struct zsc_attach_args *args = aux;

	if (name != NULL)
		printf("%s: ", name);

	if (args->channel != -1)
		printf(" channel %d", args->channel);
	return UNCONF;
}

int
zsmdioctl(cs, cmd, data)
	struct zs_chanstate *cs;
	u_long cmd;
	caddr_t data;
{
	switch (cmd) {
	default:
		return (-1);
	}
	return (0);
}

void
zsmd_setclock(cs)
	struct zs_chanstate *cs;
{
	struct xzs_chanstate *xcs = (void *)cs;

	if (cs->cs_channel != 0)
		return;

	/*
	 * If the new clock has the external bit set, then select the
	 * external source.
	 */
	via_set_modem((xcs->cs_pclk_flag & ZSC_EXTERN) ? 1 : 0);
}

static int zssoftpending;

/*
 * Do the minimum work to pull data off of the chip and queue it up
 * for later processing.
 */
int
zshard(arg)
	void *arg;
{
	struct zsc_softc *zsc = (struct zsc_softc *)arg;
	int rval;

	if (zsc == NULL)
		return 0;

	rval = 0;
	rval |= zsc_intr_hard(zsc);
	if ((zsc->zsc_cs[0]->cs_softreq) || (zsc->zsc_cs[1]->cs_softreq)) {
		/* zsc_req_softint(zsc); */
		/* We are at splzs here, so no need to lock. */
		if (zssoftpending == 0) {
			zssoftpending = 1;
			setsoftserial();
		}
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
	register struct zsc_softc *zsc;
	register int unit;

	/* This is not the only ISR on this IPL. */
	if (zssoftpending == 0)
		return (0);

	/*
	 * The soft intr. bit will be set by zshard only if
	 * the variable zssoftpending is zero.
	 */
	zssoftpending = 0;

	for (unit = 0; unit < zsc_cd.cd_ndevs; ++unit) {
		zsc = zsc_cd.cd_devs[unit];
		if (zsc == NULL)
			continue;
		(void) zsc_intr_soft(zsc);
	}
	return (1);
}


#ifndef ZS_TOLERANCE
#define ZS_TOLERANCE 51
/* 5% in tenths of a %, plus 1 so that exactly 5% will be ok. */
#endif

/*
 * check out a rate for acceptability from the internal clock
 * source. Used in console config to validate a requested
 * default speed. Placed here so that all the speed checking code is
 * in one place.
 *
 * != 0 means ok.
 */
int
zs_cn_check_speed(bps)
	int bps;	/* target rate */
{
	int tc, rate;

	tc = BPS_TO_TCONST(ZS_STD_BRG, bps);
	if (tc < 0)
		return 0;
	rate = TCONST_TO_BPS(ZS_STD_BRG, tc);
	if (ZS_TOLERANCE > abs(((rate - bps)*1000)/bps))
		return 1;
	else
		return 0;
}

/*
 * Search through the signal sources in the channel, and
 * pick the best one for the baud rate requested. Return
 * a -1 if not achievable in tolerance. Otherwise return 0
 * and fill in the values.
 *
 * This routine draws inspiration from the Atari port's zs.c
 * driver in NetBSD 1.1 which did the same type of source switching.
 * Tolerance code inspired by comspeed routine in isa/com.c.
 *
 * By Bill Studenmund, 1996-05-12
 */
int
zs_set_speed(cs, bps)
	struct zs_chanstate *cs;
	int bps;	/* bits per second */
{
	struct xzs_chanstate *xcs = (void *) cs;
	int i, tc, tc0 = 0, tc1, s, sf = 0;
	int src, rate0, rate1, err, tol;

	if (bps == 0)
		return (0);

	src = -1;		/* no valid source yet */
	tol = ZS_TOLERANCE;

	/*
	 * Step through all the sources and see which one matches
	 * the best. A source has to match BETTER than tol to be chosen.
	 * Thus if two sources give the same error, the first one will be
	 * chosen. Also, allow for the possability that one source might run
	 * both the BRG and the direct divider (i.e. RTxC).
	 */
	for (i=0; i < xcs->cs_clock_count; i++) {
		if (xcs->cs_clocks[i].clk <= 0)
			continue;	/* skip non-existant or bad clocks */
		if (xcs->cs_clocks[i].flags & ZSC_BRG) {
			/* check out BRG at /16 */
			tc1 = BPS_TO_TCONST(xcs->cs_clocks[i].clk >> 4, bps);
			if (tc1 >= 0) {
				rate1 = TCONST_TO_BPS(xcs->cs_clocks[i].clk >> 4, tc1);
				err = abs(((rate1 - bps)*1000)/bps);
				if (err < tol) {
					tol = err;
					src = i;
					sf = xcs->cs_clocks[i].flags & ~ZSC_DIV;					tc0 = tc1;
					rate0 = rate1;
				}
			}
		}
		if (xcs->cs_clocks[i].flags & ZSC_DIV) {
			/*
			 * Check out either /1, /16, /32, or /64
			 * Note: for /1, you'd better be using a synchronized
			 * clock!
			 */
			int b0 = xcs->cs_clocks[i].clk, e0 = abs(b0-bps);
			int b1 = b0 >> 4, e1 = abs(b1-bps);
			int b2 = b1 >> 1, e2 = abs(b2-bps);
			int b3 = b2 >> 1, e3 = abs(b3-bps);

			if (e0 < e1 && e0 < e2 && e0 < e3) {
				err = e0;
				rate1 = b0;
				tc1 = ZSWR4_CLK_X1;
			} else if (e0 > e1 && e1 < e2  && e1 < e3) {
				err = e1;
				rate1 = b1;
				tc1 = ZSWR4_CLK_X16;
			} else if (e0 > e2 && e1 > e2 && e2 < e3) {
				err = e2;
				rate1 = b2;
				tc1 = ZSWR4_CLK_X32;
			} else {
				err = e3;
				rate1 = b3;
				tc1 = ZSWR4_CLK_X64;
			}

			err = (err * 1000)/bps;
			if (err < tol) {
				tol = err;
				src = i;
				sf = xcs->cs_clocks[i].flags & ~ZSC_BRG;
				tc0 = tc1;
				rate0 = rate1;
			}
		}
	}
#ifdef ZSMACDEBUG
	printf("Checking for rate %d. Found source #%d.\n",bps, src);
#endif
	if (src == -1)
		return (EINVAL); /* no can do */

	/*
	 * The M.I. layer likes to keep cs_brg_clk current, even though
	 * we are the only ones who should be touching the BRG's rate.
	 *
	 * Note: we are assuming that any ZSC_EXTERN signal source comes in
	 * on the RTxC pin. Correct for the mac68k obio zsc.
	 */
	if (sf & ZSC_EXTERN)
		cs->cs_brg_clk = xcs->cs_clocks[i].clk >> 4;
	else
		cs->cs_brg_clk = ZS_STD_BRG;

	/*
	 * Now we have a source, so set it up.
	 */
	s = splzs();
	xcs->cs_psource = src;
	xcs->cs_pclk_flag = sf;
	bps = rate0;
	if (sf & ZSC_BRG) {
		cs->cs_preg[4] = ZSWR4_CLK_X16;
		cs->cs_preg[11]= ZSWR11_RXCLK_BAUD | ZSWR11_TXCLK_BAUD;
		if (sf & ZSC_PCLK) {
			cs->cs_preg[14] = ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK;
		} else {
			cs->cs_preg[14] = ZSWR14_BAUD_ENA;
		}
		tc = tc0;
	} else {
		cs->cs_preg[4] = tc0;
		if (sf & ZSC_RTXDIV) {
			cs->cs_preg[11] = ZSWR11_RXCLK_RTXC | ZSWR11_TXCLK_RTXC;		} else {
			cs->cs_preg[11] = ZSWR11_RXCLK_TRXC | ZSWR11_TXCLK_TRXC;		}
		cs->cs_preg[14]= 0;
		tc = 0xffff;
	}
	/* Set the BAUD rate divisor. */
	cs->cs_preg[12] = tc;
	cs->cs_preg[13] = tc >> 8;
	splx(s);

#ifdef ZSMACDEBUG
	printf("Rate is %7d, tc is %7d, source no. %2d, flags %4x\n", \
		bps, tc, src, sf);
	printf("Registers are: 4 %x, 11 %x, 14 %x\n\n",
		cs->cs_preg[4], cs->cs_preg[11], cs->cs_preg[14]);
#endif

	cs->cs_preg[5] |= ZSWR5_RTS;    /* Make sure the drivers are on! */	

	/* Caller will stuff the pending registers. */
	return (0);
}

int
zs_set_modes(cs, cflag)
	struct zs_chanstate *cs;
	int cflag;	/* bits per second */
{
	struct xzs_chanstate *xcs = (void*)cs;
	int s;

	/*
	 * Make sure we don't enable hfc on a signal line we're ignoring.
	 * As we enable CTS interrupts only if we have CRTSCTS or CDTRCTS,
	 * this code also effectivly turns off ZSWR15_CTS_IE.
	 *
	 * Also, disable DCD interrupts if we've been told to ignore
	 * the DCD pin. Happens on mac68k because the input line for
	 * DCD can also be used as a clock input.  (Just set CLOCAL.)
	 *
	 * If someone tries to turn an invalid flow mode on, Just Say No
	 * (Suggested by gwr)
	 */
#if 0
	if ((cflag & CDTRCTS) && (cflag & (CRTSCTS | MDMBUF)))
		return (EINVAL);
#endif
	if (xcs->cs_hwflags & ZS_HWFLAG_NO_DCD) {
		if (cflag & MDMBUF)
			return (EINVAL);
		cflag |= CLOCAL;
	}
#if 0
	if ((xcs->cs_hwflags & ZS_HWFLAG_NO_CTS) && (cflag & (CRTSCTS | CDTRCTS)))
		return (EINVAL);
#endif

	/*
	 * Output hardware flow control on the chip is horrendous:
	 * if carrier detect drops, the receiver is disabled, and if
	 * CTS drops, the transmitter is stopped IN MID CHARACTER!
	 * Therefore, NEVER set the HFC bit, and instead use the
	 * status interrupt to detect CTS changes.
	 */
	s = splzs();
	if ((cflag & (CLOCAL | MDMBUF)) != 0)
		cs->cs_rr0_dcd = 0;
	else
		cs->cs_rr0_dcd = ZSRR0_DCD;
	/*
	 * The mac hardware only has one output, DTR (HSKo in Mac
	 * parlance). In HFC mode, we use it for the functions
	 * typically served by RTS and DTR on other ports, so we
	 * have to fake the upper layer out some.
	 *
	 * CRTSCTS we use CTS as an input which tells us when to shut up.
	 * We make no effort to shut up the other side of the connection.
	 * DTR is used to hang up the modem.
	 *
	 * In CDTRCTS, we use CTS to tell us to stop, but we use DTR to
	 * shut up the other side.
	 */
	if ((cflag & CRTSCTS) != 0){
		cs->cs_wr5_dtr = ZSWR5_DTR;
		cs->cs_wr5_rts = 0;
		cs->cs_rr0_cts = ZSRR0_CTS;
#if 0
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
		cs->cs_wr5_dtr = ZSWR5_DTR;
		cs->cs_wr5_rts = 0;
		cs->cs_rr0_cts = 0;
	}
	splx(s);

	/* Caller will stuff the pending registers. */
	return(0);
}

/*
 * Read or write the chip with suitable delays.
 * MacII hardware has the delay built in.
 * No need for extra delay. :-) However, some clock-chiped
 * macs, or zsc's on serial add-on boards might need it.
 */
#define	ZS_DELAY()

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
	u_char val;

	val = *cs->cs_reg_csr;
	ZS_DELAY();
	/* make up for the fact CTS is wired backwards */
	val ^= ZSRR0_CTS;
	return val;
}

void  zs_write_csr(cs, val)
	struct zs_chanstate *cs;
	register u_char val;
{
	/* Note, the csr does not write CTS... */
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

/****************************************************************
 * Console support functions (mac68k specific!)
 * Note: this code is allowed to know about the layout of
 * the chip registers, and uses that to keep things simple.
 * XXX - I think I like the mvme167 code better. -gwr
 * XXX - Well :-P  :-)  -wrs
 ****************************************************************/

#define zscnpollc	nullcnpollc
cons_decl(zs);

static void	zs_putc(register volatile struct zschan *, int);
static int	zs_getc(register volatile struct zschan *);
static void	zscnsetup(void);
extern int	zsopen( dev_t dev, int flags, int mode, struct proc *p);

/*
 * Console functions.
 */

/*
 * This code modled after the zs_setparam routine in zskgdb
 * It sets the console unit to a known state so we can output
 * correctly.
 */
static void
zscnsetup()
{
	struct xzs_chanstate xcs;
	struct zs_chanstate *cs;
	struct zschan *zc;
	int    tconst, s;
	
	/* Setup temporary chanstate. */
	bzero((caddr_t)&xcs, sizeof(xcs));
	cs = &xcs.xzs_cs;
	zc = zs_conschan;
	cs->cs_reg_csr  = &zc->zc_csr;
	cs->cs_reg_data = &zc->zc_data;
	cs->cs_channel = zs_consunit;
	cs->cs_brg_clk = ZS_STD_BRG;

	bcopy(zs_init_reg, cs->cs_preg, 16);
	cs->cs_preg[5] |= ZSWR5_DTR | ZSWR5_RTS;
	cs->cs_preg[15] = ZSWR15_BREAK_IE;
	tconst = BPS_TO_TCONST(cs->cs_brg_clk,
		zs_defspeed[0][zs_consunit]);
	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;
	/* can't use zs_set_speed as we haven't set up the
	 * signal sources, and it's not worth it for now
	 */

	/*
	 * As zs_loadchannelregs doesn't touch reg 9 (interrupt control),
	 * we won't accidentally turn on interrupts below
	 */
	s = splhigh();
	zs_loadchannelregs(cs);
	splx(s);
}

/*
 * zscnprobe is the routine which gets called as the kernel is trying to
 * figure out where the console should be. Each io driver which might
 * be the console (as defined in mac68k/conf.c) gets probed. The probe
 * fills in the consdev structure. Important parts are the device #,
 * and the console priority. Values are CN_DEAD (don't touch me),
 * CN_NORMAL (I'm here, but elsewhere might be better), CN_INTERNAL
 * (the video, better than CN_NORMAL), and CN_REMOTE (pick me!)
 *
 * As the mac's a bit different, we do extra work here. We mainly check
 * to see if we have serial echo going on. Also chould check for default
 * speeds.
 */
void
zscnprobe(struct consdev * cp)
{
	extern u_long   IOBase;
	int     maj, unit, i;

	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == zsopen) {
			break;
		}
	}
	if (maj != nchrdev) {
		cp->cn_pri = CN_NORMAL;		/* Lower than CN_INTERNAL */
		if (mac68k_machine.serial_console != 0) {
			cp->cn_pri = CN_REMOTE;	/* Higher than CN_INTERNAL */
			mac68k_machine.serial_boot_echo =0;
		}

		unit = (mac68k_machine.serial_console == 1) ? 0 : 1;
		zs_consunit = unit;
		zs_conschan = (struct zschan *) -1; /* dummy flag for zs_init() */
		
		mac68k_zsdev = cp->cn_dev = makedev(maj, unit);
	}
	if (mac68k_machine.serial_boot_echo) {
		/*
		 * at this point, we know that we don't have a serial
		 * console, but are doing echo
		 */
		zs_conschan = (struct zschan *) -1; /* dummy flag for zs_init() */
		zs_consunit = 1;		/* printer port */
	}

	if ((i = mac68k_machine.modem_d_speed) > 0) {
		if (zs_cn_check_speed(i))
			zs_defspeed[0][0] = i;
	}
	if ((i = mac68k_machine.print_d_speed) > 0) {
		if (zs_cn_check_speed(i))
			zs_defspeed[0][1] = i;
	}
	mac68k_set_io_offsets(IOBase);
	zs_init();
	/*
	 * zsinit will set up the addresses of the scc. It will also, if
	 * zs_conschan != 0, calculate the new address of the conschan for
	 * unit zs_consunit. So if we are (or think we are) going to use the
	 * chip for console I/O, we just set up the internal addresses for it.
	 *
	 * Now turn off interrupts for the chip. Note: this code piece is the
	 * only vestage of the NetBSD 1.0 ser driver. :-)
	 */
	unit = sccA[2];		 	/* reset reg. access */
	unit = sccA[0];
	sccA[2] = 9; sccA[2] = 0;	/* write 0 to reg. 9, clearing MIE */
	sccA[2] = ZSWR0_CLR_INTR; unit = sccA[2]; /* reset any pending ints. */
	sccA[0] = ZSWR0_CLR_INTR; unit = sccA[0];

	if (mac68k_machine.serial_boot_echo)
		zscnsetup();
}

void
zscninit(struct consdev * cp)
{
	zs_hwflags[0][zs_consunit] = ZS_HWFLAG_CONSOLE;
	/*
	 * zsinit will set up the addresses of the scc. It will also, if
	 * zs_conschan != 0, calculate the new address of the conschan for
	 * unit zs_consunit. So zs_init implicitly sets zs_conschan to the right
	 * number. :-)
	 */
	zscnsetup();
}


/*
 * Polled input char.
 */
static int
zs_getc(zc)
	register volatile struct zschan *zc;
{
	register int s, c, rr0;

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
static void
zs_putc(zc, c)
	register volatile struct zschan *zc;
	int c;
{
	register int s, rr0;
	register long wait = 0;

	s = splhigh();
	/* Wait for transmitter to become ready. */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while (((rr0 & ZSRR0_TX_READY) == 0) && (wait++ < 1000000));

	if ((rr0 & ZSRR0_TX_READY) != 0) {
		zc->zc_data = c;
		ZS_DELAY();
	}
	splx(s);
}


/*
 * Polled console input putchar.
 */
int
zscngetc(dev)
	dev_t dev;
{
	register volatile struct zschan *zc = zs_conschan;
	register int c;

	c = zs_getc(zc);
	return (c);
}

/*
 * Polled console output putchar.
 */
void
zscnputc(dev, c)
	dev_t dev;
	int c;
{
	register volatile struct zschan *zc = zs_conschan;

	zs_putc(zc, c);
}



/*
 * Handle user request to enter kernel debugger.
 */
void
zs_abort(cs)
	struct zs_chanstate *cs;
{
	volatile struct zschan *zc = zs_conschan;
	int rr0;
	register long wait = 0;

	/* Wait for end of break to avoid PROM abort. */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_BREAK) && (wait++ < ZSABORT_DELAY));

	if (wait > ZSABORT_DELAY)
		return;	/* XXX */
#ifdef DDB
	if (db_console)
		Debugger();
#endif
}
