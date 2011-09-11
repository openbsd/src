/*	$OpenBSD: vsaudio.c,v 1.2 2011/09/11 19:29:01 miod Exp $	*/

/*
 * Copyright (c) 2011 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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

/*
 * Audio backend for the VAXstation 4000 AMD79C30 audio chip.
 * Currently working in pseudo-DMA mode; DMA operation may be possible and
 * needs to be investigated.
 */
/*
 * Although he did not claim copyright for his work, this code owes a lot
 * to Blaz Antonic <blaz.antonic@siol.net> who figured out a working
 * interrupt triggering routine in vsaudio_match().
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/scb.h>
#include <machine/vsbus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/am7930reg.h>
#include <dev/ic/am7930var.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (am7930debug) printf x
#define DPRINTFN(n,x)	if (am7930debug>(n)) printf x
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif	/* AUDIO_DEBUG */

/* physical addresses of the AM79C30 chip */
#define	VSAUDIO_CSR			0x200d0000
#define	VSAUDIO_CSR_KA49		0x26800000

struct vsaudio_softc {
	struct am7930_softc sc_am7930;	/* base device */
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_bh;	/* device registers */

	void	(*sc_rintr)(void*);	/* input completion intr handler */
	void	*sc_rarg;		/* arg for sc_rintr() */
	void	(*sc_pintr)(void*);	/* output completion intr handler */
	void	*sc_parg;		/* arg for sc_pintr() */

	uint8_t	*sc_rdata;		/* record data */
	uint8_t	*sc_rend;		/* end of record data */
	uint8_t	*sc_pdata;		/* play data */
	uint8_t	*sc_pend;		/* end of play data */

	void	*sc_swintr;		/* soft interrupt cookie */
	int	sc_cvec;
	struct evcount sc_intrcnt;
};

void	vsaudio_attach(struct device *, struct device *, void *);
int	vsaudio_match(struct device *, void *, void *);

struct cfdriver vsaudio_cd = {
	NULL, "vsaudio", DV_DULL
};

const struct cfattach vsaudio_ca = {
	sizeof(struct vsaudio_softc), vsaudio_match, vsaudio_attach
};

/*
 * Hardware access routines for the MI code.
 */

uint8_t	 vsaudio_codec_iread(struct am7930_softc *, int);
uint16_t vsaudio_codec_iread16(struct am7930_softc *, int);
uint8_t	 vsaudio_codec_dread(struct vsaudio_softc *, int);
void	 vsaudio_codec_iwrite(struct am7930_softc *, int, uint8_t);
void	 vsaudio_codec_iwrite16(struct am7930_softc *, int, uint16_t);
void	 vsaudio_codec_dwrite(struct vsaudio_softc *, int, uint8_t);
void	 vsaudio_onopen(struct am7930_softc *sc);
void	 vsaudio_onclose(struct am7930_softc *sc);

struct am7930_glue vsaudio_glue = {
	vsaudio_codec_iread,
	vsaudio_codec_iwrite,
	vsaudio_codec_iread16,
	vsaudio_codec_iwrite16,
	vsaudio_onopen,
	vsaudio_onclose
};

/*
 * Interface to the MI audio layer.
 */
int	vsaudio_start_output(void *, void *, int, void (*)(void *), void *);
int	vsaudio_start_input(void *, void *, int, void (*)(void *), void *);
int	vsaudio_getdev(void *, struct audio_device *);

struct audio_hw_if vsaudio_hw_if = {
	am7930_open,
	am7930_close,
	NULL,
	am7930_query_encoding,
	am7930_set_params,
	am7930_round_blocksize,
	am7930_commit_settings,
	NULL,
	NULL,
	vsaudio_start_output,
	vsaudio_start_input,
	am7930_halt_output,
	am7930_halt_input,
	NULL,
	vsaudio_getdev,
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

struct audio_device vsaudio_device = {
	"am7930",
	"x",
	"vsaudio"
};

void	vsaudio_hwintr(void *);
void	vsaudio_swintr(void *);

int
vsaudio_match(struct device *parent, void *vcf, void *aux)
{
	struct vsbus_attach_args *va = aux;
	volatile uint32_t *regs;
	int i;

	switch (vax_boardtype) {
#if defined(VAX46) || defined(VAX48)
	case VAX_BTYP_46:
	case VAX_BTYP_48:
		if (va->va_paddr != VSAUDIO_CSR)
			return 0;
		break;
#endif
#if defined(VAX49)
	case VAX_BTYP_49:
		if (va->va_paddr != VSAUDIO_CSR_KA49)
			return 0;
		break;
#endif
	default:
		return 0;
	}

	regs = (volatile uint32_t *)va->va_addr;
	regs[AM7930_DREG_CR] = AM7930_IREG_INIT;
	regs[AM7930_DREG_DR] = AM7930_INIT_PMS_ACTIVE | AM7930_INIT_INT_ENABLE;

	regs[AM7930_DREG_CR] = AM7930_IREG_MUX_MCR1;
	regs[AM7930_DREG_DR] = 0;

	regs[AM7930_DREG_CR] = AM7930_IREG_MUX_MCR2;
	regs[AM7930_DREG_DR] = 0;

	regs[AM7930_DREG_CR] = AM7930_IREG_MUX_MCR3;
	regs[AM7930_DREG_DR] = (AM7930_MCRCHAN_BB << 4) | AM7930_MCRCHAN_BA;

	regs[AM7930_DREG_CR] = AM7930_IREG_MUX_MCR4;
	regs[AM7930_DREG_DR] = AM7930_MCR4_INT_ENABLE;

	for (i = 10; i < 20; i++)
		regs[AM7930_DREG_BBTB] = i;
	delay(1000000);	/* XXX too large */

	return 1;
}

void
vsaudio_attach(struct device *parent, struct device *self, void *aux)
{
	struct vsbus_attach_args *va = aux;
	struct vsaudio_softc *sc = (struct vsaudio_softc *)self;

	if (bus_space_map(va->va_iot, va->va_paddr, AM7930_DREG_SIZE << 2, 0,
	    &sc->sc_bh) != 0) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_bt = va->va_iot;

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_am7930.sc_glue = &vsaudio_glue;
	am7930_init(&sc->sc_am7930, AUDIOAMD_POLL_MODE);

	scb_vecalloc(va->va_cvec, vsaudio_hwintr, sc, SCB_ISTACK,
	    &sc->sc_intrcnt);
	sc->sc_cvec = va->va_cvec;
	evcount_attach(&sc->sc_intrcnt, self->dv_xname, &sc->sc_cvec);

	sc->sc_swintr = softintr_establish(IPL_SOFT, &vsaudio_swintr, sc);

	printf("\n");
	audio_attach_mi(&vsaudio_hw_if, sc, &sc->sc_am7930.sc_dev);
}

void
vsaudio_onopen(struct am7930_softc *sc)
{
	struct vsaudio_softc *vssc = (struct vsaudio_softc *)sc;

	/* reset pdma state */
	vssc->sc_rintr = NULL;
	vssc->sc_rarg = 0;
	vssc->sc_pintr = NULL;
	vssc->sc_parg = 0;

	vssc->sc_rdata = NULL;
	vssc->sc_pdata = NULL;
}

void
vsaudio_onclose(struct am7930_softc *sc)
{
	am7930_halt_input(sc);
	am7930_halt_output(sc);
}

int
vsaudio_start_output(void *addr, void *p, int cc,
    void (*intr)(void *), void *arg)
{
	struct vsaudio_softc *sc = addr;

	DPRINTFN(1, ("sa_start_output: cc=%d %p (%p)\n", cc, intr, arg));

	if (!sc->sc_am7930.sc_locked) {
		vsaudio_codec_iwrite(&sc->sc_am7930,
		    AM7930_IREG_INIT, AM7930_INIT_PMS_ACTIVE);
		sc->sc_am7930.sc_locked = 1;
		DPRINTF(("sa_start_output: started intrs.\n"));
	}
	sc->sc_pintr = intr;
	sc->sc_parg = arg;
	sc->sc_pdata = p;
	sc->sc_pend = (char *)p + cc - 1;
	return 0;
}

int
vsaudio_start_input(void *addr, void *p, int cc,
    void (*intr)(void *), void *arg)
{
	struct vsaudio_softc *sc = addr;

	DPRINTFN(1, ("sa_start_input: cc=%d %p (%p)\n", cc, intr, arg));

	if (!sc->sc_am7930.sc_locked) {
		vsaudio_codec_iwrite(&sc->sc_am7930,
			AM7930_IREG_INIT, AM7930_INIT_PMS_ACTIVE);
		sc->sc_am7930.sc_locked = 1;
		DPRINTF(("sa_start_input: started intrs.\n"));
	}
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;
	sc->sc_rdata = p;
	sc->sc_rend = (char *)p + cc -1;
	return 0;
}

/*
 * Pseudo-DMA support
 */

void
vsaudio_hwintr(void *v)
{
	struct vsaudio_softc *sc = v;
	uint8_t *d, *e;
	int k;

	/* clear interrupt */
	k = vsaudio_codec_dread(sc, AM7930_DREG_IR);
#if 0	/* interrupt is not shared, this shouldn't happen */
	if ((k & (AM7930_IR_DTTHRSH | AM7930_IR_DRTHRSH | AM7930_IR_DSRI |
	    AM7930_IR_DERI | AM7930_IR_BBUFF)) == 0)
		return 0;
#endif

	/* receive incoming data */
	d = sc->sc_rdata;
	e = sc->sc_rend;
	if (d != NULL && d <= e) {
		*d = vsaudio_codec_dread(sc, AM7930_DREG_BBRB);
		sc->sc_rdata++;
		if (d == e) {
			DPRINTFN(1, ("vsaudio_hwintr: swintr(r) requested"));
			softintr_schedule(sc->sc_swintr);
		}
	}

	/* send outgoing data */
	d = sc->sc_pdata;
	e = sc->sc_pend;
	if (d != NULL && d <= e) {
		vsaudio_codec_dwrite(sc, AM7930_DREG_BBTB, *d);
		sc->sc_pdata++;
		if (d == e) {
			DPRINTFN(1, ("vsaudio_hwintr: swintr(p) requested"));
			softintr_schedule(sc->sc_swintr);
		}
	}
}

void
vsaudio_swintr(void *v)
{
	struct vsaudio_softc *sc = v;
	int s, dor, dow;

	DPRINTFN(1, ("audiointr: sc=%p\n", sc));

	dor = dow = 0;
	s = splaudio();
	if (sc->sc_rdata > sc->sc_rend && sc->sc_rintr != NULL)
		dor = 1;
	if (sc->sc_pdata > sc->sc_pend && sc->sc_pintr != NULL)
		dow = 1;
	splx(s);

	if (dor != 0)
		(*sc->sc_rintr)(sc->sc_rarg);
	if (dow != 0)
		(*sc->sc_pintr)(sc->sc_parg);
}

/* indirect write */
void
vsaudio_codec_iwrite(struct am7930_softc *sc, int reg, uint8_t val)
{
	struct vsaudio_softc *vssc = (struct vsaudio_softc *)sc;

	vsaudio_codec_dwrite(vssc, AM7930_DREG_CR, reg);
	vsaudio_codec_dwrite(vssc, AM7930_DREG_DR, val);
}

void
vsaudio_codec_iwrite16(struct am7930_softc *sc, int reg, uint16_t val)
{
	struct vsaudio_softc *vssc = (struct vsaudio_softc *)sc;

	vsaudio_codec_dwrite(vssc, AM7930_DREG_CR, reg);
	vsaudio_codec_dwrite(vssc, AM7930_DREG_DR, val);
	vsaudio_codec_dwrite(vssc, AM7930_DREG_DR, val >> 8);
}

/* indirect read */
uint8_t
vsaudio_codec_iread(struct am7930_softc *sc, int reg)
{
	struct vsaudio_softc *vssc = (struct vsaudio_softc *)sc;

	vsaudio_codec_dwrite(vssc, AM7930_DREG_CR, reg);
	return vsaudio_codec_dread(vssc, AM7930_DREG_DR);
}

uint16_t
vsaudio_codec_iread16(struct am7930_softc *sc, int reg)
{
	struct vsaudio_softc *vssc = (struct vsaudio_softc *)sc;
	uint lo, hi;

	vsaudio_codec_dwrite(vssc, AM7930_DREG_CR, reg);
	lo = vsaudio_codec_dread(vssc, AM7930_DREG_DR);
	hi = vsaudio_codec_dread(vssc, AM7930_DREG_DR);
	return (hi << 8) | lo;
}

/* direct read */
uint8_t
vsaudio_codec_dread(struct vsaudio_softc *sc, int reg)
{
	return bus_space_read_1(sc->sc_bt, sc->sc_bh, reg << 2);
}

/* direct write */
void
vsaudio_codec_dwrite(struct vsaudio_softc *sc, int reg, uint8_t val)
{
	bus_space_write_1(sc->sc_bt, sc->sc_bh, reg << 2, val);
}

int
vsaudio_getdev(void *addr, struct audio_device *retp)
{
	*retp = vsaudio_device;
	return 0;
}
