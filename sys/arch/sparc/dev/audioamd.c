/*	$OpenBSD: audioamd.c,v 1.4 2015/05/11 06:46:21 ratchov Exp $	*/
/*	$NetBSD: audioamd.c,v 1.26 2011/06/04 01:27:57 tsutsui Exp $	*/

/*
 * Copyright (c) 1995 Rolf Grossmann
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
 *      This product includes software developed by Rolf Grossmann.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/am7930reg.h>
#include <dev/ic/am7930var.h>
#include <sparc/dev/audioamdvar.h>

#define AUDIO_ROM_NAME "audio"

#ifdef AUDIO_DEBUG
#define DPRINTF(x)      if (am7930debug) printf x
#define DPRINTFN(n,x)   if (am7930debug>(n)) printf x
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif	/* AUDIO_DEBUG */

/*
 * Define AUDIO_C_HANDLER to force using non-fast trap routines.
 */
/* #define AUDIO_C_HANDLER */

struct audioamd_softc {
	struct am7930_softc sc_am7930;	/* glue to MI code */

	void	(*sc_rintr)(void*);	/* input completion intr handler */
	void	*sc_rarg;		/* arg for sc_rintr() */
	void	(*sc_pintr)(void*);	/* output completion intr handler */
	void	*sc_parg;		/* arg for sc_pintr() */

	/* sc_au is special in that the hardware interrupt handler uses it */
	struct  auio sc_au;		/* recv and xmit buffers, etc */
#define	sc_hwih sc_au.au_ih		/* hardware interrupt vector */
#define	sc_swih sc_au.au_swih		/* software interrupt cookie */
};

void	audioamd_attach(struct device *, struct device *, void *);
int	audioamd_match(struct device *, void *, void *);

struct cfdriver audioamd_cd = {
	NULL, "audioamd", DV_DULL
};

const struct cfattach audioamd_ca = {
	sizeof(struct audioamd_softc), audioamd_match, audioamd_attach
};

/*
 * Define our interface into the am7930 MI driver.
 */

uint8_t	 audioamd_codec_iread(struct am7930_softc *, int);
uint16_t audioamd_codec_iread16(struct am7930_softc *, int);
uint8_t	 audioamd_codec_dread(struct audioamd_softc *, int);
void	 audioamd_codec_iwrite(struct am7930_softc *, int, uint8_t);
void	 audioamd_codec_iwrite16(struct am7930_softc *, int, uint16_t);
void	 audioamd_codec_dwrite(struct audioamd_softc *, int, uint8_t);
void	 audioamd_onopen(struct am7930_softc *);
void	 audioamd_onclose(struct am7930_softc *);

struct am7930_glue audioamd_glue = {
	audioamd_codec_iread,
	audioamd_codec_iwrite,
	audioamd_codec_iread16,
	audioamd_codec_iwrite16,
	audioamd_onopen,
	audioamd_onclose,
	8
};

/*
 * Define our interface to the higher level audio driver.
 */
int	audioamd_start_output(void *, void *, int, void (*)(void *), void *);
int	audioamd_start_input(void *, void *, int, void (*)(void *), void *);
int	audioamd_getdev(void *, struct audio_device *);

struct audio_hw_if sa_hw_if = {
	am7930_open,
	am7930_close,
	NULL,
	am7930_query_encoding,
	am7930_set_params,
	am7930_round_blocksize,
	am7930_commit_settings,
	NULL,
	NULL,
	audioamd_start_output,
	audioamd_start_input,
	am7930_halt_output,
	am7930_halt_input,
	NULL,
	audioamd_getdev,
	NULL,
	am7930_set_port,
	am7930_get_port,
	am7930_query_devinfo,
	NULL,
	NULL,
	NULL,
	NULL,
	am7930_get_props,
	NULL,
	NULL,
	NULL
};

struct audio_device audioamd_device = {
	"am7930",
	"x",
	"audioamd"
};

/* forward declarations */
int	amd7930_shareintr(void *);
#ifndef AUDIO_C_HANDLER
struct auio *auiop;
#endif	/* AUDIO_C_HANDLER */
int	am7930hwintr(void *);
void	am7930swintr(void *);

int
audioamd_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (CPU_ISSUN4)
		return 0;
	return strcmp(AUDIO_ROM_NAME, ra->ra_name) == 0;
}

void
audioamd_attach(struct device *parent, struct device *self, void *aux)
{
	struct audioamd_softc *sc = (struct audioamd_softc *)self;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	int pri;

	if (ra->ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ra->ra_nintr);
		return;
	}
	pri = ra->ra_intr[0].int_pri;
	printf(" pri %d, softpri %d\n", pri, IPL_AUSOFT);
	sc->sc_au.au_sc = sc;
	sc->sc_au.au_amd = (volatile uint8_t *)(ra->ra_vaddr ?
	    ra->ra_vaddr : mapiodev(ra->ra_reg, 0, AM7930_DREG_SIZE));

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_am7930.sc_glue = &audioamd_glue;
	am7930_init(&sc->sc_am7930, AUDIOAMD_POLL_MODE);

	/*
	 * Register interrupt handlers.  We'll prefer a fast trap (unless
	 * AUDIO_C_HANDLER is defined), with a sharing callback so that we
	 * can revert into a regular trap vector if necessary.
	 */
#ifndef AUDIO_C_HANDLER
	sc->sc_hwih.ih_vec = pri;
	if (intr_fasttrap(pri, amd7930_trap, amd7930_shareintr, sc) == 0) {
		auiop = &sc->sc_au;
		evcount_attach(&sc->sc_hwih.ih_count, self->dv_xname,
		    &sc->sc_hwih.ih_vec);
	} else {
#ifdef AUDIO_DEBUG
		printf("%s: unable to register fast trap handler\n",
		    self->dv_xname);
#endif
#else
	{
#endif
		sc->sc_hwih.ih_fun = am7930hwintr;
		sc->sc_hwih.ih_arg = &sc->sc_au;
		intr_establish(pri, &sc->sc_hwih, IPL_AUHARD, self->dv_xname);
	}

	sc->sc_swih = softintr_establish(IPL_AUSOFT, am7930swintr, sc);

	audio_attach_mi(&sa_hw_if, sc, self);
}

void
audioamd_onopen(struct am7930_softc *sc)
{
	struct audioamd_softc *ausc = (struct audioamd_softc *)sc;

	/* reset pdma state */
	ausc->sc_rintr = NULL;
	ausc->sc_rarg = 0;
	ausc->sc_pintr = NULL;
	ausc->sc_parg = 0;

	ausc->sc_au.au_rdata = NULL;
	ausc->sc_au.au_pdata = NULL;
}


void
audioamd_onclose(struct am7930_softc *sc)
{
	/* On sparc, just do the chipset-level halt. */
	am7930_halt_input(sc);
	am7930_halt_output(sc);
}

/*
 * called in interrupt code-path, don't lock
 */
int
audioamd_start_output(void *addr, void *p, int cc,
    void (*intr)(void *), void *arg)
{
	struct audioamd_softc *sc = addr;

	DPRINTFN(1, ("sa_start_output: cc=%d %p (%p)\n", cc, intr, arg));

	if (!sc->sc_am7930.sc_locked) {
		audioamd_codec_iwrite(&sc->sc_am7930,
		    AM7930_IREG_INIT, AM7930_INIT_PMS_ACTIVE);
		sc->sc_am7930.sc_locked = 1;
		DPRINTF(("sa_start_output: started intrs.\n"));
	}
	sc->sc_pintr = intr;
	sc->sc_parg = arg;
	sc->sc_au.au_pdata = p;
	sc->sc_au.au_pend = (char *)p + cc - 1;
	return 0;
}

/*
 * called in interrupt code-path, don't lock
 */
int
audioamd_start_input(void *addr, void *p, int cc,
    void (*intr)(void *), void *arg)
{
	struct audioamd_softc *sc = addr;

	DPRINTFN(1, ("sa_start_input: cc=%d %p (%p)\n", cc, intr, arg));

	if (!sc->sc_am7930.sc_locked) {
		audioamd_codec_iwrite(&sc->sc_am7930,
		    AM7930_IREG_INIT, AM7930_INIT_PMS_ACTIVE);
		sc->sc_am7930.sc_locked = 1;
		DPRINTF(("sa_start_input: started intrs.\n"));
	}
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;
	sc->sc_au.au_rdata = p;
	sc->sc_au.au_rend = (char *)p + cc -1;
	return 0;
}

/*
 * Pseudo-DMA support: either C or locore assember.
 */

int
am7930hwintr(void *v)
{
	struct auio *au = v;
	struct audioamd_softc *sc = au->au_sc;
	uint8_t *d, *e;
	int k;

	/* clear interrupt */
	k = audioamd_codec_dread(sc, AM7930_DREG_IR);
	if ((k & (AM7930_IR_DTTHRSH | AM7930_IR_DRTHRSH | AM7930_IR_DSRI |
	    AM7930_IR_DERI | AM7930_IR_BBUFF)) == 0)
		return 0;

	/* receive incoming data */
	d = au->au_rdata;
	e = au->au_rend;
	if (d != NULL && d <= e) {
		*d = audioamd_codec_dread(sc, AM7930_DREG_BBRB);
		au->au_rdata++;
		if (d == e) {
			DPRINTFN(1, ("am7930hwintr: swintr(r) requested"));
			softintr_schedule(au->au_swih);
		}
	}

	/* send outgoing data */
	d = au->au_pdata;
	e = au->au_pend;
	if (d != NULL && d <= e) {
		audioamd_codec_dwrite(sc, AM7930_DREG_BBTB, *d);
		au->au_pdata++;
		if (d == e) {
			DPRINTFN(1, ("am7930hwintr: swintr(p) requested"));
			softintr_schedule(au->au_swih);
		}
	}

	return 1;
}

void
am7930swintr(void *v)
{
	struct audioamd_softc *sc = v;
	struct auio *au;
	int dor, dow;

	DPRINTFN(1, ("audiointr: sc=%p\n", sc););

	au = &sc->sc_au;
	dor = dow = 0;
	mtx_enter(&audio_lock);
	if (au->au_rdata > au->au_rend && sc->sc_rintr != NULL)
		dor = 1;
	if (au->au_pdata > au->au_pend && sc->sc_pintr != NULL)
		dow = 1;
	mtx_leave(&audio_lock);

	if (dor != 0)
		(*sc->sc_rintr)(sc->sc_rarg);
	if (dow != 0)
		(*sc->sc_pintr)(sc->sc_parg);
}

#ifndef AUDIO_C_HANDLER
int
amd7930_shareintr(void *arg)
{
	struct audioamd_softc *sc = arg;

	/*
	 * We are invoked at splhigh(), so there is no need to prevent the chip
	 * from interrupting while we are messing with the handlers. We
	 * however need to properly untie the event counter from the chain,
	 * since it will be reused immediately by intr_establish()...
	 */

	intr_fastuntrap(sc->sc_hwih.ih_vec);
	evcount_detach(&sc->sc_hwih.ih_count);

	sc->sc_hwih.ih_fun = am7930hwintr;
	sc->sc_hwih.ih_arg = &sc->sc_au;
	intr_establish(sc->sc_hwih.ih_vec, &sc->sc_hwih, IPL_AUHARD,
	    sc->sc_am7930.sc_dev.dv_xname);

	return 0;
}
#endif

/* indirect write */
void
audioamd_codec_iwrite(struct am7930_softc *sc, int reg, uint8_t val)
{
	struct audioamd_softc *ausc = (struct audioamd_softc *)sc;

	audioamd_codec_dwrite(ausc, AM7930_DREG_CR, reg);
	audioamd_codec_dwrite(ausc, AM7930_DREG_DR, val);
}

void
audioamd_codec_iwrite16(struct am7930_softc *sc, int reg, uint16_t val)
{
	struct audioamd_softc *ausc = (struct audioamd_softc *)sc;

	audioamd_codec_dwrite(ausc, AM7930_DREG_CR, reg);
	audioamd_codec_dwrite(ausc, AM7930_DREG_DR, val);
	audioamd_codec_dwrite(ausc, AM7930_DREG_DR, val >> 8);
}


/* indirect read */
uint8_t
audioamd_codec_iread(struct am7930_softc *sc, int reg)
{
	struct audioamd_softc *ausc = (struct audioamd_softc *)sc;

	audioamd_codec_dwrite(ausc, AM7930_DREG_CR, reg);
	return audioamd_codec_dread(ausc, AM7930_DREG_DR);
}

uint16_t
audioamd_codec_iread16(struct am7930_softc *sc, int reg)
{
	struct audioamd_softc *ausc = (struct audioamd_softc *)sc;
	uint lo, hi;

	audioamd_codec_dwrite(ausc, AM7930_DREG_CR, reg);
	lo = audioamd_codec_dread(ausc, AM7930_DREG_DR);
	hi = audioamd_codec_dread(ausc, AM7930_DREG_DR);
	return (hi << 8) | lo;
}

/* direct read */
uint8_t
audioamd_codec_dread(struct audioamd_softc *sc, int reg)
{
	return sc->sc_au.au_amd[reg];
}

/* direct write */
void
audioamd_codec_dwrite(struct audioamd_softc *sc, int reg, uint8_t val)
{
	sc->sc_au.au_amd[reg] = val;
}

int
audioamd_getdev(void *addr, struct audio_device *retp)
{
	*retp = audioamd_device;
	return 0;
}
