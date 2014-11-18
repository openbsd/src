/*	$OpenBSD: zsclock.c,v 1.4 2014/11/18 20:51:01 krw Exp $	*/
/*	$NetBSD: clock.c,v 1.11 1995/05/16 07:30:46 phil Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	@(#)clock.c	7.2 (Berkeley) 5/12/91
 *
 */

/*
 * Primitive clock interrupt routines.
 *
 * Improved by Phil Budne ... 10/17/94.
 * Pulled over code from i386/isa/clock.c (Matthias Pfaller 12/03/94).
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/ioctl.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

/*
 * Zilog Z8530 Dual UART driver (clock interface)
 */

/* Clock state.  */
struct zsclock_softc {
	struct device zsc_dev;
	struct zs_chanstate *zsc_cs;
};

int	zsclock_match(struct device *, void *, void *);
void	zsclock_attach(struct device *, struct device *, void *);

struct cfattach zsclock_ca = {
	sizeof(struct zsclock_softc), zsclock_match, zsclock_attach
};

struct cfdriver zsclock_cd = {
	NULL, "zsclock", DV_DULL
};

void	zsclock_stint(struct zs_chanstate *, int);

struct zsops zsops_clock = {
	NULL,
	zsclock_stint,
	NULL,
	NULL
};

static int zsclock_attached;

/*
 * clock_match: how is this zs channel configured?
 */
int 
zsclock_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct zsc_attach_args *args = aux;

	/* only attach one instance */
	if (zsclock_attached)
		return (0);

	/* make sure we'll win a probe over zstty or zskbd */
	if (cf->cf_loc[ZSCCF_CHANNEL] == args->channel)
		return (2 + 2);

	if (cf->cf_loc[ZSCCF_CHANNEL] == ZSCCF_CHANNEL_DEFAULT)
		return (2 + 1);

	return (0);
}

/*
 * The Solbourne IDT systems provide a 4.9152 MHz clock to the ZS chips.
 */
#define	PCLK	(9600 * 512)	/* PCLK pin input clock rate */

void 
zsclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct zsc_softc *zsc = (void *)parent;
	struct zsclock_softc *sc = (void *)self;
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	int channel;
	int reset, s, tconst;

	channel = args->channel;

	cs = &zsc->zsc_cs[channel];
	cs->cs_private = zsc;
	cs->cs_ops = &zsops_clock;

	sc->zsc_cs = cs;

	printf("\n");

	hz = 100;
	tconst = ((PCLK / 2) / hz) - 2;

	s = splclock();

	reset = (channel == 0) ?  ZSWR9_A_RESET : ZSWR9_B_RESET;
	zs_write_reg(cs, 9, reset);

	cs->cs_preg[1] = 0;
	cs->cs_preg[3] = ZSWR3_RX_8 | ZSWR3_RX_ENABLE;
	cs->cs_preg[4] = ZSWR4_CLK_X1 | ZSWR4_ONESB | ZSWR4_PARENB;
	cs->cs_preg[5] = ZSWR5_TX_8 | ZSWR5_TX_ENABLE;
	cs->cs_preg[9] = ZSWR9_MASTER_IE;
	cs->cs_preg[10] = 0;
	cs->cs_preg[11] = ZSWR11_RXCLK_RTXC | ZSWR11_TXCLK_RTXC |
	    ZSWR11_TRXC_OUT_ENA | ZSWR11_TRXC_BAUD;
	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;
	cs->cs_preg[14] = ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA;
	cs->cs_preg[15] = ZSWR15_ZERO_COUNT_IE;

	zs_loadchannelregs(cs);

	splx(s);

	/* enable interrupts */
	cs->cs_preg[1] |= ZSWR1_SIE;
	zs_write_reg(cs, 1, cs->cs_preg[1]);

	zsclock_attached = 1;
}

void
zsclock_stint(struct zs_chanstate *cs, int force)
{
	u_char rr0;

	rr0 = zs_read_csr(cs);
	cs->cs_rr0 = rr0;

	/*
	 * Retrigger the interrupt as a soft interrupt, because we need
	 * a trap frame for hardclock().
	 */
	ienab_bis(IE_L10);

	zs_write_csr(cs, ZSWR0_RESET_STATUS);
}
