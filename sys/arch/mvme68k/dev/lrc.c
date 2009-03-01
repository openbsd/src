/*	$OpenBSD: lrc.c,v 1.1 2009/03/01 21:40:49 miod Exp $	*/

/*
 * Copyright (c) 2006, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * LRC Local Resource Controller, found on MVME165
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/lrcreg.h>

struct lrcreg *sys_lrc;

struct lrcsoftc {
	struct device	 sc_dev;
	struct lrcreg	*sc_regs;
	vaddr_t		 sc_vaddr;
	paddr_t		 sc_paddr;
	struct intrhand	 sc_abortih;
};

int	lrcabort(void *);
void	lrcattach(struct device *, struct device *, void *);
int	lrcmatch(struct device *, void *, void *);
int	lrcprint(void *, const char *);
int	lrcscan(struct device *, void *, void *);

struct cfattach lrc_ca = {
	sizeof(struct lrcsoftc), lrcmatch, lrcattach
};

struct cfdriver lrc_cd = {
	NULL, "lrc", DV_DULL
};

int
lrcmatch(struct device *parent, void *cf, void *aux)
{
	if (cputyp != CPU_165 || sys_lrc != NULL)
		return (0);

	return (1);
}

void
lrcattach(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;
	struct lrcsoftc *sc = (struct lrcsoftc *)self;

	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = IIOV(sc->sc_paddr);
	sys_lrc = sc->sc_regs = (struct lrcreg *)sc->sc_vaddr;

	printf("\n");

	/* sync serial clock with DUART */
	sc->sc_regs->lrc_gcr &= ~GCR_SCFREQ;
	/* disable VSB */
	sc->sc_regs->lrc_bcr &= ~(BCR_VA24 | BCR_VSBEN | BCR_ROEN);
	/* set up vector base */
	sc->sc_regs->lrc_icr1 = LRC_VECBASE;
	/* enable interrupts */
	sc->sc_regs->lrc_icr0 = ICR0_GIE;

	sc->sc_abortih.ih_fn = lrcabort;
	sc->sc_abortih.ih_ipl = IPL_HIGH;
	sc->sc_abortih.ih_wantframe = 1;
	lrcintr_establish(LRCVEC_ABORT, &sc->sc_abortih, self->dv_xname);

	config_search(lrcscan, self, aux);
}

int
lrcprint(void *aux, const char *pnp)
{
	struct confargs *ca = aux;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
lrcscan(struct device *parent, void *self, void *aux)
{
	struct cfdata *cf = self;
	struct lrcsoftc *sc = (struct lrcsoftc *)parent;
	struct confargs *ca = aux;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_iot = ca->ca_iot;
	oca.ca_dmat = ca->ca_dmat;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1 && ISIIOVA(sc->sc_vaddr + oca.ca_offset)) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (vaddr_t)-1;
		oca.ca_paddr = (paddr_t)-1;
	}	
	oca.ca_bustype = BUS_LRC;
	oca.ca_name = cf->cf_driver->cd_name;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);

	config_attach(parent, cf, &oca, lrcprint);

	return (1);
}

/*
 * Register an LRC interrupt.
 * This will take care of enabling the right interrupt source in the
 * interrupt controller.
 */
int
lrcintr_establish(u_int vec, struct intrhand *ih, const char *name)
{
	int rc;
	const struct {
		u_int8_t icr0_bit;
		u_int8_t icr1_bit;
	} icr_bits[] = {
		{ 0x00,			0x00 },	/* timer 0 done differently */
		{ 0x00,			0x00 },	/* timer 1 done differently */
		{ 0x00,			0x00 },	/* timer 2 done differently */
		{ 0xff,			0xff },
		{ ICR0_IRQ7G2IE,	0x00 },
		{ ICR0_IRQ6G2IE,	0x00 },
		{ 0xff,			0xff },
		{ ICR0_IRQ3G2IE,	0x00 },
		{ ICR0_IRQ7G2IE,	0x00 },
		{ ICR0_IRQ4G2IE,	0x00 },	/* ICR0_IRQ6G4IE */
		{ 0x00,			ICR1_IRQ7G5IE },
		{ 0x00,			ICR1_IRQ7G6IE },
		{ ICR0_IRQ5G2IE,	0x00 },
		{ 0xff,			0xff },
		{ 0xff,			0xff },
		{ 0xff,			0xff }
	};
		
#ifdef DIAGNOSTIC
	if (vec < 0 || vec >= LRC_NVEC || icr_bits[vec].icr0_bit == 0xff)
		panic("lrcintr_establish: illegal vector for %s: 0x%x",
		    name, vec);
#endif

	rc = intr_establish(LRC_VECBASE + vec, ih, name);

	if (rc == 0) {
		sys_lrc->lrc_icr0 |= icr_bits[vec].icr0_bit;
		sys_lrc->lrc_icr1 |= icr_bits[vec].icr1_bit;
	}

	return (rc);
}

int
lrcabort(void *frame)
{
	nmihand(frame);

	return (1);
}

/*
 * Figure out the speed of the board by measuring how many operations
 * can be issued in a given time, reusing the delay() logic.
 */
int
lrcspeed(struct lrcreg *lrc)
{
	uint cnt;
	int speed;

	/* use timer0 and wait for it to wrap after 200msec */
	cnt = 0;
	lrc->lrc_t0base = 200000 + 1;
	lrc->lrc_tcr0 = TCR_TLD0;	/* reset to one */
	lrc->lrc_stat = STAT_TMR0;	/* clear latch */
	lrc->lrc_tcr0 = TCR_TEN0;
	while ((lrc->lrc_stat & STAT_TMR0) == 0)
		cnt++;

	/*
	 * Empirically determined. However since there are only
	 * 25MHz and 33MHz boards available, it is easy to draw
	 * a line - cnt should be close to cpu MHz * 8000.
	 */
	if (cnt > 30 * 8000)
		speed = 33;
	else
		speed = 25;

	return speed;
}
