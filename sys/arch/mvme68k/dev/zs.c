/*	$OpenBSD: zs.c,v 1.35 2013/12/01 21:56:42 miod Exp $	*/
/*	$NetBSD: zs.c,v 1.29 2001/05/30 15:24:24 lukem Exp $	*/

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
 * Plain tty/async lines use the zstty slave.
 * Sun keyboard/mouse uses the zskbd/zsms slaves.
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
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/z8530var.h>

#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <dev/cons.h>

#include <dev/ic/z8530reg.h>

#include "mc.h"
#include "pcc.h"

#if NPCC > 0
#include <mvme68k/dev/pccreg.h>
#endif
#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif

#include "zs.h"

/*
 * Some warts needed by z8530tty.c -
 * The default parity REALLY needs to be the same as the PROM uses,
 * or you can not see messages done with printf during boot-up...
 */
int zs_def_cflag = (CREAD | CS8 | HUPCL);
int zs_major = 12;

#define PCLK_FREQ_147	5000000
#define PCLK_FREQ_162	10000000

#define	ZS_DELAY()

/* physical layout on the MVME147 */
struct zschan_147 {
	volatile uint8_t	zc_csr;
	volatile uint8_t	zc_data;
};

/* physical layout on the MVME1x2 */
struct zschan_162 {
	uint8_t			zc_xxx0;
	volatile uint8_t	zc_csr;
	uint8_t			zc_xxx1;
	volatile uint8_t	zc_data;
};

#if NMC > 0
/*
 * MC rev 0x01 has a bug and can not access the data register directly.
 */
int mc_rev1_bug = 0;
#endif

static uint8_t zs_init_reg[17] = {
	0,				/* 0: CMD (reset, etc.) */
	0,				/* 1: No interrupts yet. */
	0,				/* 2: IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,				/* 6: TXSYNC/SYNCLO */
	0,				/* 7: RXSYNC/SYNCHI */
	0,				/* 8: alias for data port */
	ZSWR9_MASTER_IE ,
	0,				/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	0,				/*12: BAUDLO (default=9600) */
	0,				/*13: BAUDHI (default=9600) */
	ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK,
	ZSWR15_BREAK_IE,
	ZSWR7P_TX_FIFO			/*7': TX FIFO interrupt level */
};

/* Console ops */
cons_decl(zs);
struct consdev zs_consdev = {
	zscnprobe,
	zscninit,
	zscngetc,
	zscnputc,
	zscnpollc,
	NULL,
};

static int zs_consunit = -1;
static int zs_conschan = -1;


/****************************************************************
 * Autoconfig
 ****************************************************************/

/* Definition of the driver for autoconfig. */
int	zs_match(struct device *, void *, void *);
void	zs_attach(struct device *, struct device *, void *);
int	zs_print(void *, const char *);

struct cfdriver zs_cd = {
	NULL, "zs", DV_TTY
};

const struct cfattach zs_ca = {
	sizeof(struct zsc_softc), zs_match, zs_attach
};

/* Interrupt handlers. */
int	zshard(void *);
void	zssoft(void *);

int	zs_get_speed(struct zs_chanstate *);

/*
 * Is the zs chip present?
 */
int
zs_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	unsigned char *zstest = (unsigned char *)ca->ca_vaddr;

	/* 
	 * If zs1 is in the config, we must test to see if it really exists.  
	 * Some 162s only have one scc device, but the memory location for 
	 * the second scc still checks valid and every byte contains 0xFF. So 
	 * this is what we test with for now. XXX - smurph
	 */
	if (!badvaddr((vaddr_t)ca->ca_vaddr, 1))
		if (*zstest == 0xFF)
			return (0);
		else
			return (1);
	else
		return (0);
}

void
zs_attach(struct device *parent, struct device *self, void *aux)
{
	struct zsc_softc *zsc = (void *)self;
	struct confargs *ca = aux;
	struct zsc_attach_args zsc_args;
	struct zs_chanstate *cs;
	int s, channel, has_fifo;
	int pclk;
	uint8_t ir;

	switch (cputyp) {
	default:
#ifdef MVME147
	case CPU_147:
		pclk = PCLK_FREQ_147;
		break;
#endif
#if defined(MVME162) || defined(MVME172)
	case CPU_162:
	case CPU_172:
		pclk = PCLK_FREQ_162;
		break;
#endif
	}

	switch (ca->ca_bustype) {
#if NPCC > 0
	case BUS_PCC:
		zs_init_reg[9] |= ZSWR9_NO_VECTOR;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		if (sys_mc->mc_chiprev != 0x01)
			mc_rev1_bug = 0;
		zs_init_reg[2] = MC_VECBASE + MCV_ZS;
		break;
#endif
	}

	/*
	 * Initialize software state for each channel.
	 */
	for (channel = 0; channel < 2; channel++) {
		zsc_args.channel = channel;
		cs = &zsc->zsc_cs_store[channel];
		zsc->zsc_cs[channel] = cs;

		cs->cs_channel = channel;
		cs->cs_private = NULL;
		cs->cs_ops = &zsops_null;
		cs->cs_brg_clk = pclk / 16;

		switch (cputyp) {
		default:
#ifdef MVME147
		case CPU_147:
		    {
			struct zschan_147 *zc;

			zc = (struct zschan_147 *)ca->ca_vaddr;
			cs->cs_reg_csr = &zc[channel ^ 1].zc_csr;
			cs->cs_reg_data = &zc[channel ^ 1].zc_data;
		    }
			break;
#endif
#if defined(MVME162) || defined(MVME172)
		case CPU_162:
		case CPU_172:
		    {
			struct zschan_162 *zc;

			zc = (struct zschan_162 *)ca->ca_vaddr;
			cs->cs_reg_csr = &zc[channel ^ 1].zc_csr;
			cs->cs_reg_data = &zc[channel ^ 1].zc_data;
		    }
			break;
#endif
		}

		/*
		 * Figure out whether this chip is a 8530 or a 85230.
		 */
		if (channel == 0) {
			zs_write_reg(cs, 15, ZSWR15_ENABLE_ENHANCED);
			has_fifo = zs_read_reg(cs, 15) & ZSWR15_ENABLE_ENHANCED;

			if (has_fifo) {
				zs_write_reg(cs, 15, 0);
				printf(": 85230\n");
			} else
				printf(": 8530\n");
		}

		if (has_fifo)
			zs_init_reg[15] |= ZSWR15_ENABLE_ENHANCED;
		else
			zs_init_reg[15] &= ~ZSWR15_ENABLE_ENHANCED;

		if (self->dv_unit == zs_consunit && channel == zs_conschan) {
			zsc_args.consdev = &zs_consdev;
			zsc_args.hwflags =
			    ZS_HWFLAG_CONSOLE | ZS_HWFLAG_USE_CONSDEV;
		} else {
			zsc_args.consdev = NULL;
			zsc_args.hwflags = 0;
		}

		bcopy(zs_init_reg, cs->cs_creg, 16);
		bcopy(zs_init_reg, cs->cs_preg, 16);

		cs->cs_defspeed = zs_get_speed(cs);
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
		if (!config_found(&zsc->zsc_dev, (void *)&zsc_args, zs_print)) {
			/* No sub-driver.  Just reset it. */
			u_char reset = (channel == 0) ?
				ZSWR9_A_RESET : ZSWR9_B_RESET;
			s = splzs();
			zs_write_reg(cs,  9, reset);
			splx(s);
		} 
	}

	/*
	 * Now safe to install interrupt handlers.
	 */
	zsc->zsc_ih.ih_fn = zshard;
	zsc->zsc_ih.ih_arg = zsc;
	zsc->zsc_ih.ih_ipl = IPL_ZS;
	zsc->zsc_ih.ih_wantframe = 0;

	switch (ca->ca_bustype) {
#if NPCC > 0
	case BUS_PCC:
		pccintr_establish(PCCV_ZS, &zsc->zsc_ih, self->dv_xname);
		break;
#endif
#if NMC > 0
	case BUS_MC:
		mcintr_establish(MCV_ZS, &zsc->zsc_ih, self->dv_xname);
		break;
#endif
	}

	zsc->zsc_softih = softintr_establish(IPL_SOFTTTY, zssoft, zsc);

	/*
	 * Set the master interrupt enable and interrupt vector.
	 * (common to both channels, do it on A)
	 */
	cs = zsc->zsc_cs[0];
	s = splhigh();
	/* interrupt vector */
	zs_write_reg(cs, 2, zs_init_reg[2]);
	/* master interrupt control (enable) */
	zs_write_reg(cs, 9, zs_init_reg[9]);
	splx(s);

	switch (ca->ca_bustype) {
#if NPCC > 0
	case BUS_PCC:
		ir = sys_pcc->pcc_zsirq;
		if ((ir & PCC_IRQ_IPL) != 0 && (ir & PCC_IRQ_IPL) != IPL_ZS)
			panic("zs configured at different IPLs");
		sys_pcc->pcc_zsirq = IPL_ZS | PCC_IRQ_IEN | PCC_ZS_PCCVEC;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		ir = sys_mc->mc_zsirq;
		if ((ir & MC_IRQ_IPL) != 0 && (ir & MC_IRQ_IPL) != IPL_ZS)
			panic("zs configured at different IPLs");
		sys_mc->mc_zsirq = IPL_ZS | MC_IRQ_IEN;
		break;
#endif
	}
}

int
zs_print(void *aux, const char *name)
{
	struct zsc_attach_args *args = aux;

	if (name != NULL)
		printf("%s: ", name);

	if (args->channel != -1)
		printf(" channel %d", args->channel);

	return (UNCONF);
}

int
zshard(void *arg)
{
	struct zsc_softc *zsc = (struct zsc_softc *)arg;
	int rr3, rval;

	rval = 0;
	while ((rr3 = zsc_intr_hard(zsc))) {
		/* Count up the interrupts. */
		rval |= rr3;
	}
	if (((zsc->zsc_cs[0] && zsc->zsc_cs[0]->cs_softreq) ||
	     (zsc->zsc_cs[1] && zsc->zsc_cs[1]->cs_softreq)) &&
	    zsc->zsc_softih) {
		softintr_schedule(zsc->zsc_softih);
	}
	return (rval);
}

void
zssoft(void *arg)
{
	struct zsc_softc *zsc = (struct zsc_softc *)arg;
	int s;

	/* Make sure we call the tty layer at spltty. */
	s = spltty();
	zsc_intr_soft(zsc);
	splx(s);
}

/*
 * Compute the current baud rate given a ZS channel.
 */
int
zs_get_speed(struct zs_chanstate *cs)
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
zs_set_speed(struct zs_chanstate *cs, int bps)
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
zs_set_modes(struct zs_chanstate *cs, int cflag)
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

uint8_t
zs_read_reg(struct zs_chanstate *cs, uint8_t reg)
{
	uint8_t val;

	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	val = *cs->cs_reg_csr;
	ZS_DELAY();
	return (val);
}

void
zs_write_reg(struct zs_chanstate *cs, uint8_t reg, uint8_t val)
{
	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

uint8_t
zs_read_csr(struct zs_chanstate *cs)
{
	uint8_t val;

	val = *cs->cs_reg_csr;
	ZS_DELAY();
	return (val);
}

void
zs_write_csr(struct zs_chanstate *cs, uint8_t val)
{
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

uint8_t
zs_read_data(struct zs_chanstate *cs)
{
	uint8_t val;

#if NMC > 0
	if (mc_rev1_bug)
		return zs_read_reg(cs, 8);
#endif
	val = *cs->cs_reg_data;
	ZS_DELAY();
	return (val);
}

void 
zs_write_data(struct zs_chanstate *cs, uint8_t val)
{
#if NMC > 0
	if (mc_rev1_bug) {
		zs_write_reg(cs, 8, val);
		return;
	}
#endif
	*cs->cs_reg_data = val;
	ZS_DELAY();
}

/*
 * Handle user request to enter kernel debugger.
 */
void
zs_abort(struct zs_chanstate *cs)
{
	int rr0;

	/* Wait for end of break to avoid PROM abort. */
	/* XXX - Limit the wait? */
	do {
		rr0 = zs_read_csr(cs);
	} while (rr0 & ZSRR0_BREAK);

#if defined(KGDB)
	zskgdb(cs);
#elif defined(DDB)
	Debugger();
#endif
}

/****************************************************************
 * Console support functions
 ****************************************************************/

struct zs_consptr {
	volatile uint8_t *zc_csr;
	volatile uint8_t *zc_data;
};

static struct zs_consptr zs_consptr;

void
zscnprobe(struct consdev *cp)
{
	switch (cputyp) {
	default:
		return;
#ifdef MVME147
	case CPU_147:
		break;
#endif
#if defined(MVME162) || defined(MVME172)
	case CPU_162:
	case CPU_172:
		break;
#endif
	}

	cp->cn_dev = makedev(zs_major, 0);
	cp->cn_pri = CN_LOWPRI;
}

void
zscninit(struct consdev *cp)
{
	switch (cputyp) {
	default:
#ifdef MVME147
	case CPU_147:
	    {
		struct zschan_147 *zc;

		zc = (struct zschan_147 *)IIOV(ZS0_PHYS_147);
		zs_consptr.zc_csr = &zc[1].zc_csr;
		zs_consptr.zc_data = &zc[1].zc_data;
	    }
#if NMC > 0
	    	mc_rev1_bug = 0;
#endif
		break;
#endif
#if defined(MVME162) || defined(MVME172)
	case CPU_162:
	case CPU_172:
	    {
		struct zschan_162 *zc;

		zc = (struct zschan_162 *)IIOV(ZS0_PHYS_162);
		zs_consptr.zc_csr = &zc[1].zc_csr;
		zs_consptr.zc_data = &zc[1].zc_data;
	    }
		break;
#endif
	}

	zs_consunit = 0;
	zs_conschan = 0;
}

/*
 * Polled console input putchar.
 */
int
zscngetc(dev_t dev)
{
	struct zs_consptr *zc = &zs_consptr;
	int s, c, rr0;

	s = splhigh();
	/* Wait for a character to arrive. */
	do {
		rr0 = *zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_RX_READY) == 0);

#if NMC > 0
	if (mc_rev1_bug) {
		*zc->zc_csr = 8;
		ZS_DELAY();
		c = *zc->zc_csr;
		ZS_DELAY();
	} else
#endif
	{
		c = *zc->zc_data;
		ZS_DELAY();
	}
	splx(s);

	return (c);
}

/*
 * Polled console output putchar.
 */
void
zscnputc(dev_t dev, int c)
{
	struct zs_consptr *zc = &zs_consptr;
	int s, rr0;

	s = splhigh();

	/* Wait for transmitter to become ready. */
	do {
		rr0 = *zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_TX_READY) == 0);

	/*
	 * Send the next character.
	 */
#if NMC > 0
	if (mc_rev1_bug) {
		*zc->zc_csr = 8;
		ZS_DELAY();
		*zc->zc_csr = c;
		ZS_DELAY();
	} else
#endif
	{
		*zc->zc_data = c;
		ZS_DELAY();
	}

	splx(s);
}

void
zscnpollc(dev_t dev, int on)
{
}
