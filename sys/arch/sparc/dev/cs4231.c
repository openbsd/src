/*	$OpenBSD: cs4231.c,v 1.16 2002/08/16 18:55:42 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Driver for CS4231 based audio found in some sun4m systems (cs4231)
 * based on ideas from the S/Linux project and the NetBSD project.
 */

#include "audio.h"
#if NAUDIO > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>
#include <dev/ic/apcdmareg.h>
#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>
#include <sparc/dev/cs4231reg.h>
#include <sparc/dev/cs4231var.h>

#define	CSAUDIO_DAC_LVL		0
#define	CSAUDIO_LINE_IN_LVL	1
#define	CSAUDIO_MIC_LVL		2
#define	CSAUDIO_CD_LVL		3
#define	CSAUDIO_MONITOR_LVL	4
#define	CSAUDIO_OUTPUT_LVL	5
#define	CSAUDIO_LINE_IN_MUTE	6
#define	CSAUDIO_DAC_MUTE	7
#define	CSAUDIO_CD_MUTE		8
#define	CSAUDIO_MIC_MUTE	9
#define	CSAUDIO_MONITOR_MUTE	10
#define	CSAUDIO_OUTPUT_MUTE	11
#define	CSAUDIO_REC_LVL		12
#define	CSAUDIO_RECORD_SOURCE	13
#define	CSAUDIO_OUTPUT		14
#define	CSAUDIO_INPUT_CLASS	15
#define	CSAUDIO_OUTPUT_CLASS	16
#define	CSAUDIO_RECORD_CLASS	17
#define	CSAUDIO_MONITOR_CLASS	18

#define	CSPORT_AUX2		0
#define	CSPORT_AUX1		1
#define	CSPORT_DAC		2
#define	CSPORT_LINEIN		3
#define	CSPORT_MONO		4
#define	CSPORT_MONITOR		5
#define	CSPORT_SPEAKER		6
#define	CSPORT_LINEOUT		7
#define	CSPORT_HEADPHONE	8

#define MIC_IN_PORT	0
#define LINE_IN_PORT	1
#define AUX1_IN_PORT	2
#define DAC_IN_PORT	3

#ifdef AUDIO_DEBUG
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)
#endif

/* Sun uses these pins in pin control register as GPIO */
#define	CS_PC_LINEMUTE		XCTL0_ENABLE	/* mute line */
#define	CS_PC_HDPHMUTE		XCTL1_ENABLE	/* mute headphone */

/* cs4231 playback interrupt */
#define CS_AFS_PI		0x10		/* playback interrupt */

#define CS_TIMEOUT		90000		/* recalibration timeout */

int	cs4231_match(struct device *, void *, void *);
void	cs4231_attach(struct device *, struct device *, void *);
int	cs4231_intr(void *);

void	cs4231_wait(struct cs4231_softc *);
int	cs4231_set_speed(struct cs4231_softc *, u_long *);
void	cs4231_mute_monitor(struct cs4231_softc *, int);
void	cs4231_setup_output(struct cs4231_softc *sc);

/* Audio interface */
int	cs4231_open(void *, int);
void	cs4231_close(void *);
int	cs4231_query_encoding(void *, struct audio_encoding *);
int	cs4231_set_params(void *, int, int, struct audio_params *,
    struct audio_params *);
int	cs4231_round_blocksize(void *, int);
int	cs4231_commit_settings(void *);
int	cs4231_halt_output(void *);
int	cs4231_halt_input(void *);
int	cs4231_getdev(void *, struct audio_device *);
int	cs4231_set_port(void *, mixer_ctrl_t *);
int	cs4231_get_port(void *, mixer_ctrl_t *);
int	cs4231_query_devinfo(void *addr, mixer_devinfo_t *);
void *	cs4231_alloc(void *, int, size_t, int, int);
void	cs4231_free(void *, void *, int);
size_t	cs4231_round_buffersize(void *, int, size_t);
int	cs4231_get_props(void *);
int	cs4231_trigger_output(void *, void *, void *, int,
    void (*intr)(void *), void *arg, struct audio_params *);
int	cs4231_trigger_input(void *, void *, void *, int,
    void (*intr)(void *), void *arg, struct audio_params *);

struct audio_hw_if cs4231_sa_hw_if = {
	cs4231_open,
	cs4231_close,
	0,
	cs4231_query_encoding,
	cs4231_set_params,
	cs4231_round_blocksize,
	cs4231_commit_settings,
	0,
	0,
	0,
	0,
	cs4231_halt_output,
	cs4231_halt_input,
	0,
	cs4231_getdev,
	0,
	cs4231_set_port,
	cs4231_get_port,
	cs4231_query_devinfo,
	cs4231_alloc,
	cs4231_free,
	cs4231_round_buffersize,
	0,
	cs4231_get_props,
	cs4231_trigger_output,
	cs4231_trigger_input
};

struct cfattach audiocs_ca = {
	sizeof (struct cs4231_softc), cs4231_match, cs4231_attach
};

struct cfdriver audiocs_cd = {
	NULL, "audiocs", DV_DULL
};

struct audio_device cs4231_device = {
	"SUNW,CS4231",
	"a",			/* XXX b for ultra */
	"onboard1",		/* XXX unknown for ultra */
};

int
cs4231_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name) &&
	    strcmp("SUNW,CS4231", ra->ra_name)) {
		return (0);
	}
	return (1);
}

void    
cs4231_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct cs4231_softc *sc = (struct cs4231_softc *)self;
	int pri;

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;

	if (ca->ca_ra.ra_nreg != 1) {
		printf(": expected 1 register set, got %d\n",
		    ca->ca_ra.ra_nreg);
		return;
	}
	sc->sc_regs = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
	    ca->ca_ra.ra_reg[0].rr_len);

	sc->sc_node = ca->ca_ra.ra_node;

	sc->sc_burst = getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		sc->sc_burst = ((struct sbus_softc *)parent)->sc_burst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= ((struct sbus_softc *)parent)->sc_burst;

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	sc->sc_ih.ih_fun = cs4231_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih, IPL_AUHARD);

	printf(" pri %d, softpri %d\n", pri, IPL_AUSOFT);

	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);

	audio_attach_mi(&cs4231_sa_hw_if, sc, &sc->sc_dev);

	/* Default to speaker, unmuted, reasonable volume */
	sc->sc_out_port = CSPORT_SPEAKER;
	sc->sc_mute[CSPORT_SPEAKER] = 1;
	sc->sc_volume[CSPORT_SPEAKER].left = 192;
	sc->sc_volume[CSPORT_SPEAKER].right = 192;
}

/*
 * Hardware interrupt handler
 */
int
cs4231_intr(v)
	void *v;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)v;
	struct cs4231_regs *regs = sc->sc_regs;
	u_int32_t csr;
	u_int8_t reg, status;
	struct cs_dma *p;
	int r = 0;

	csr = regs->dma_csr;
	regs->dma_csr = csr;

	if ((csr & APC_CSR_EIE) && (csr & APC_CSR_EI)) {
		printf("%s: error interrupt\n", sc->sc_dev.dv_xname);
		r = 1;
	}

	if ((csr & APC_CSR_PIE) && (csr & APC_CSR_PI)) {
		/* playback interrupt */
		r = 1;
	}

	if ((csr & APC_CSR_GIE) && (csr & APC_CSR_GI)) {
		/* general interrupt */
		status = regs->status;
		if (status & (INTERRUPT_STATUS | SAMPLE_ERROR)) {
			regs->iar = CS_IRQ_STATUS;
			reg = regs->idr;
			if (reg & CS_AFS_PI) {
				regs->iar = SP_LOWER_BASE_COUNT;
				regs->idr = 0xff;
				regs->iar = SP_UPPER_BASE_COUNT;
				regs->idr = 0xff;
			}
			regs->status = 0;
		}
		r = 1;
	}

	if ((csr & APC_CSR_PMIE) && (csr & APC_CSR_PMI)) {
		u_int32_t nextaddr, togo;

		p = sc->sc_nowplaying;
		togo = sc->sc_playsegsz - sc->sc_playcnt;
		if (togo == 0) {
			nextaddr = (u_int32_t)p->addr_dva;
			sc->sc_playcnt = togo = sc->sc_blksz;
		} else {
			nextaddr = regs->dma_pnva + sc->sc_blksz;
			if (togo > sc->sc_blksz)
				togo = sc->sc_blksz;
			sc->sc_playcnt += togo;
		}

		regs->dma_pnva = nextaddr;
		regs->dma_pnc = togo;
		if (sc->sc_pintr != NULL)
			(*sc->sc_pintr)(sc->sc_parg);
		r = 1;
	}

#if 0
	if (csr & APC_CSR_CI) {
		if (sc->sc_rintr != NULL) {
			r = 1;
			(*sc->sc_rintr)(sc->sc_rarg);
		}
	}
#endif

	return (r);
}

void
cs4231_mute_monitor(sc, mute)
	struct cs4231_softc *sc;
	int mute;
{
	struct cs4231_regs *regs = sc->sc_regs;

	if (mute) {
		regs->iar = SP_LEFT_OUTPUT_CONTROL;	/* left dac */
		regs->idr |= OUTPUT_MUTE;
		regs->iar = SP_RIGHT_OUTPUT_CONTROL;	/* right dac */
		regs->idr |= OUTPUT_MUTE;
#if 0
		regs->iar = CS_MONO_IO_CONTROL;		/* mono */
		regs->idr |= MONO_OUTPUT_MUTE;
#endif
	}
	else {
		regs->iar = SP_LEFT_OUTPUT_CONTROL;	/* left dac */
		regs->idr &= ~OUTPUT_MUTE;
		regs->iar = SP_RIGHT_OUTPUT_CONTROL;	/* right dac */
		regs->idr &= ~OUTPUT_MUTE;
#if 0
		regs->iar = CS_MONO_IO_CONTROL;		/* mono */
		regs->idr &= ~MONO_OUTPUT_MUTE;
#endif
	}
}

int
cs4231_set_speed(sc, argp)
	struct cs4231_softc *sc;
	u_long *argp;

{
	/*
	 * The available speeds are in the following table. Keep the speeds in
	 * the increasing order.
	 */
	typedef struct {
		int speed;
		u_char bits;
	} speed_struct;
	u_long arg = *argp;

	static speed_struct speed_table[] = {
		{5510,	(0 << 1) | CLOCK_XTAL2},
		{5510,	(0 << 1) | CLOCK_XTAL2},
		{6620,	(7 << 1) | CLOCK_XTAL2},
		{8000,	(0 << 1) | CLOCK_XTAL1},
		{9600,	(7 << 1) | CLOCK_XTAL1},
		{11025,	(1 << 1) | CLOCK_XTAL2},
		{16000,	(1 << 1) | CLOCK_XTAL1},
		{18900,	(2 << 1) | CLOCK_XTAL2},
		{22050,	(3 << 1) | CLOCK_XTAL2},
		{27420,	(2 << 1) | CLOCK_XTAL1},
		{32000,	(3 << 1) | CLOCK_XTAL1},
		{33075,	(6 << 1) | CLOCK_XTAL2},
		{33075,	(4 << 1) | CLOCK_XTAL2},
		{44100,	(5 << 1) | CLOCK_XTAL2},
		{48000,	(6 << 1) | CLOCK_XTAL1},
	};

	int i, n, selected = -1;

	n = sizeof(speed_table) / sizeof(speed_struct);

	if (arg < speed_table[0].speed)
		selected = 0;
	if (arg > speed_table[n - 1].speed)
		selected = n - 1;

	for (i = 1; selected == -1 && i < n; i++) {
		if (speed_table[i].speed == arg)
			selected = i;
		else if (speed_table[i].speed > arg) {
			int diff1, diff2;

			diff1 = arg - speed_table[i - 1].speed;
			diff2 = speed_table[i].speed - arg;
			if (diff1 < diff2)
				selected = i - 1;
			else
				selected = i;
		}
	}

	if (selected == -1) {
		printf("%s: can't find speed\n", sc->sc_dev.dv_xname);
		selected = 3;
	}

	sc->sc_speed_bits = speed_table[selected].bits;
	sc->sc_need_commit = 1;
	*argp = speed_table[selected].speed;

	return (0);
}

void
cs4231_wait(sc)
	struct cs4231_softc *sc;
{
	struct cs4231_regs *regs = sc->sc_regs;
	int tries;

	DELAY(100);

	regs->iar = ~(MODE_CHANGE_ENABLE);
	tries = CS_TIMEOUT;
	while (regs->iar == SP_IN_INIT && tries--) {
		DELAY(100);
	}
	if (!tries)
		printf("%s: waited too long to reset iar\n",
		    sc->sc_dev.dv_xname);

	regs->iar = SP_TEST_AND_INIT;
	tries = CS_TIMEOUT;
	while (regs->idr == AUTO_CAL_IN_PROG && tries--) {
		DELAY(100);
	}
	if (!tries)
		printf("%s: waited too long to reset errinit\n",
		    sc->sc_dev.dv_xname);
}

/*
 * Audio interface functions
 */
int
cs4231_open(addr, flags)
	void *addr;
	int flags;
{
	struct cs4231_softc *sc = addr;
	struct cs4231_regs *regs = sc->sc_regs;
	u_int8_t reg;

	if (sc->sc_open)
		return (EBUSY);
	sc->sc_open = 1;
	sc->sc_locked = 0;
	sc->sc_rintr = 0;
	sc->sc_rarg = 0;
	sc->sc_pintr = 0;
	sc->sc_parg = 0;

	regs->dma_csr = APC_CSR_RESET;
	DELAY(10);
	regs->dma_csr = 0;
	DELAY(10);
	regs->dma_csr |= APC_CSR_CODEC_RESET;

	DELAY(20);

	regs->dma_csr &= ~(APC_CSR_CODEC_RESET);
	regs->iar |= MODE_CHANGE_ENABLE;

	cs4231_wait(sc);

	regs->iar = MODE_CHANGE_ENABLE | SP_MISC_INFO;
	regs->idr = MODE2;

	/* XXX TODO: setup some defaults */

	regs->iar = ~(MODE_CHANGE_ENABLE);
	cs4231_wait(sc);

	regs->iar = SP_PIN_CONTROL;
	regs->idr |= INTERRUPT_ENABLE;

	regs->iar = MODE_CHANGE_ENABLE | SP_INTERFACE_CONFIG;
	reg = regs->idr;
	regs->iar = MODE_CHANGE_ENABLE | SP_INTERFACE_CONFIG;
	regs->idr = reg & ~(AUTO_CAL_ENABLE);

	regs->iar = ~(MODE_CHANGE_ENABLE);
	cs4231_wait(sc);

	cs4231_setup_output(sc);
	return (0);
}

void
cs4231_setup_output(sc)
	struct cs4231_softc *sc;
{
	struct cs4231_regs *regs = sc->sc_regs;

	regs->iar = SP_PIN_CONTROL;
	regs->idr |= CS_PC_HDPHMUTE | CS_PC_LINEMUTE;
	regs->iar = CS_MONO_IO_CONTROL;
	regs->idr |= MONO_OUTPUT_MUTE;

	switch (sc->sc_out_port) {
	case CSPORT_HEADPHONE:
		if (sc->sc_mute[CSPORT_SPEAKER]) {
			regs->iar = SP_PIN_CONTROL;
			regs->idr &= ~CS_PC_HDPHMUTE;
		}
		break;
	case CSPORT_SPEAKER:
		if (sc->sc_mute[CSPORT_SPEAKER]) {
			regs->iar = CS_MONO_IO_CONTROL;
			regs->idr &= ~MONO_OUTPUT_MUTE;
		}
		break;
	case CSPORT_LINEOUT:
		if (sc->sc_mute[CSPORT_SPEAKER]) {
			regs->iar = SP_PIN_CONTROL;
			regs->idr &= ~CS_PC_LINEMUTE;
		}
		break;
	}

	regs->iar = SP_LEFT_OUTPUT_CONTROL;
	regs->idr &= ~OUTPUT_ATTEN_BITS;
	regs->idr |= (~(sc->sc_volume[CSPORT_SPEAKER].left >> 2)) &
	    OUTPUT_ATTEN_BITS;
	regs->iar = SP_RIGHT_OUTPUT_CONTROL;
	regs->idr &= ~OUTPUT_ATTEN_BITS;
	regs->idr |= (~(sc->sc_volume[CSPORT_SPEAKER].right >> 2)) &
	    OUTPUT_ATTEN_BITS;
}

void
cs4231_close(addr)
	void *addr;
{
	struct cs4231_softc *sc = addr;
	struct cs4231_regs *regs = sc->sc_regs;

	cs4231_halt_input(sc);
	cs4231_halt_output(sc);
	regs->iar = SP_PIN_CONTROL;
	regs->idr &= ~INTERRUPT_ENABLE;
	sc->sc_open = 0;
}

int
cs4231_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	int err = 0;

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 2:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 3:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 8:
		strcpy(fp->name, AudioEadpcm);
		fp->encoding = AUDIO_ENCODING_ADPCM;
		fp->precision = 8;
		fp->flags = 0;
		break;
	default:
		err = EINVAL;
	}
	return (err);
}

int
cs4231_set_params(addr, setmode, usemode, p, r)
	void *addr;
	int setmode, usemode;
	struct audio_params *p, *r;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	int err, bits, enc;
	void (*pswcode)(void *, u_char *, int cnt);
	void (*rswcode)(void *, u_char *, int cnt);

	enc = p->encoding;
	pswcode = rswcode = 0;
	switch (enc) {
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 8) {
			enc = AUDIO_ENCODING_ULINEAR_LE;
			pswcode = rswcode = change_sign8;
		}
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 16) {
			enc = AUDIO_ENCODING_SLINEAR_LE;
			pswcode = rswcode = change_sign16;
		}
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (p->precision == 16) {
			enc = AUDIO_ENCODING_SLINEAR_BE;
			pswcode = rswcode = change_sign16;
		}
		break;
	}

	switch (enc) {
	case AUDIO_ENCODING_ULAW:
		bits = FMT_ULAW >> 5;
		break;
	case AUDIO_ENCODING_ALAW:
		bits = FMT_ALAW >> 5;
		break;
	case AUDIO_ENCODING_ADPCM:
		bits = FMT_ADPCM >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 16)
			bits = FMT_TWOS_COMP >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision == 16)
			bits = FMT_TWOS_COMP_BE >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 8)
			bits = FMT_PCM8 >> 5;
		else
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	if (p->channels != 1 && p->channels != 2)
		return (EINVAL);

	err = cs4231_set_speed(sc, &p->sample_rate);
	if (err)
		return (err);

	p->sw_code = pswcode;
	r->sw_code = rswcode;

	sc->sc_format_bits = bits;
	sc->sc_channels = p->channels;
	sc->sc_precision = p->precision;
	sc->sc_need_commit = 1;
	return (0);
}

int
cs4231_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	return (blk & (-4));
}

int
cs4231_commit_settings(addr)
	void *addr;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	struct cs4231_regs *regs = sc->sc_regs;
	int s, tries;
	u_char fs;
	volatile u_int8_t x;

	if (sc->sc_need_commit == 0)
		return (0);

	s = splaudio();

	cs4231_mute_monitor(sc, 1);

	fs = sc->sc_speed_bits | (sc->sc_format_bits << 5);
	if (sc->sc_channels == 2)
		fs |= FMT_STEREO;

	regs->iar = MODE_CHANGE_ENABLE | SP_CLOCK_DATA_FORMAT;
	regs->idr = fs;
	x = regs->idr;
	x = regs->idr;
	tries = 100000;
	while (tries-- && regs->idr == SP_IN_INIT);
	if (tries == 0) {
		printf("%s: timeout committing fspb\n", sc->sc_dev.dv_xname);
		splx(s);
		return (0);
	}

	regs->iar = MODE_CHANGE_ENABLE | CS_REC_FORMAT;
	regs->idr = fs;
	x = regs->idr;
	x = regs->idr;
	tries = 100000;
	while (tries-- && regs->idr == SP_IN_INIT);
	if (tries == 0) {
		printf("%s: timeout committing cdf\n", sc->sc_dev.dv_xname);
		splx(s);
		return (0);
	}

	cs4231_wait(sc);

	cs4231_mute_monitor(sc, 0);

	splx(s);

	sc->sc_need_commit = 0;
	return (0);
}

int
cs4231_halt_output(addr)
	void *addr;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	struct cs4231_regs *regs = sc->sc_regs;
	u_int8_t r;

	regs->dma_csr &= ~(APC_CSR_EI | APC_CSR_GIE | APC_CSR_PIE |
	    APC_CSR_EIE | APC_CSR_PDMA_GO | APC_CSR_PMIE);
	regs->iar = SP_INTERFACE_CONFIG;
	r = regs->idr & (~PLAYBACK_ENABLE);
	regs->iar = SP_INTERFACE_CONFIG;
	regs->idr = r;
	sc->sc_locked = 0;
	return (0);
}

int
cs4231_halt_input(addr)
	void *addr;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	struct cs4231_regs *regs = sc->sc_regs;

	regs->dma_csr = APC_CSR_CAPTURE_PAUSE;
	regs->iar = SP_INTERFACE_CONFIG;
	regs->idr &= ~CAPTURE_ENABLE;
	sc->sc_locked = 0;
	return (0);
}

int
cs4231_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = cs4231_device;
	return (0);
}

int
cs4231_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	int error = EINVAL;

	DPRINTF(("cs4231_set_port: port=%d type=%d\n", cp->dev, cp->type));

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = SP_LEFT_AUX1_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    AUX_INPUT_ATTEN_BITS;
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_regs->iar = SP_LEFT_AUX1_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    AUX_INPUT_ATTEN_BITS;
			sc->sc_regs->iar = SP_RIGHT_AUX1_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    AUX_INPUT_ATTEN_BITS;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = CS_LEFT_LINE_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    AUX_INPUT_ATTEN_BITS;
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_regs->iar = CS_LEFT_LINE_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    AUX_INPUT_ATTEN_BITS;
			sc->sc_regs->iar = CS_RIGHT_LINE_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    AUX_INPUT_ATTEN_BITS;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
#if 0
			sc->sc_regs->iar = CS_MONO_IO_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    MONO_INPUT_ATTEN_BITS;
#endif
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_CD_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = SP_LEFT_AUX2_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    AUX_INPUT_ATTEN_BITS;
			error = 0;
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_regs->iar = SP_LEFT_AUX2_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    AUX_INPUT_ATTEN_BITS;
			sc->sc_regs->iar = SP_RIGHT_AUX2_CONTROL;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    AUX_INPUT_ATTEN_BITS;
			error = 0;
		}
		else
			break;
		break;
	case CSAUDIO_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = SP_DIGITAL_MIX;
			sc->sc_regs->idr =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] << 2;
		}
		else
			break;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_volume[CSPORT_SPEAKER].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[CSPORT_SPEAKER].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_volume[CSPORT_SPEAKER].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[CSPORT_SPEAKER].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		}
		else
			break;

		cs4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_OUTPUT:
		if (cp->un.ord != CSPORT_LINEOUT &&
		    cp->un.ord != CSPORT_SPEAKER &&
		    cp->un.ord != CSPORT_HEADPHONE)
			return (EINVAL);
		sc->sc_out_port = cp->un.ord;
		cs4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_LINE_IN_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_LINEIN] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_DAC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_AUX1] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_CD_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_AUX2] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MIC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_MONO] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MONITOR_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_MONITOR] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_SPEAKER] = cp->un.ord ? 1 : 0;
		cs4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_REC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		break;
	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		break;
	}

	return (error);
}

int
cs4231_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	int error = EINVAL;

	DPRINTF(("cs4231_get_port: port=%d type=%d\n", cp->dev, cp->type));

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = SP_LEFT_AUX1_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_regs->iar = SP_LEFT_AUX1_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
			sc->sc_regs->iar = SP_RIGHT_AUX1_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = CS_LEFT_LINE_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_regs->iar = CS_LEFT_LINE_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
			sc->sc_regs->iar = CS_RIGHT_LINE_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
#if 0
			sc->sc_regs->iar = CS_MONO_IO_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_regs->idr & MONO_INPUT_ATTEN_BITS;
#endif
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_CD_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = SP_LEFT_AUX2_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
			error = 0;
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_regs->iar = SP_LEFT_AUX2_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
			sc->sc_regs->iar = SP_RIGHT_AUX2_CONTROL;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_regs->idr & AUX_INPUT_ATTEN_BITS;
			error = 0;
		}
		else
			break;
		break;
	case CSAUDIO_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_regs->iar = SP_DIGITAL_MIX;
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_regs->idr >> 2;
		}
		else
			break;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_volume[CSPORT_SPEAKER].left;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_volume[CSPORT_SPEAKER].left;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_volume[CSPORT_SPEAKER].right;
		}
		else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_LINEIN] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_DAC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_AUX1] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_CD_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_AUX2] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MIC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_MONO] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MONITOR_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_MONITOR] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_SPEAKER] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_REC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    AUDIO_MIN_GAIN;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    AUDIO_MIN_GAIN;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    AUDIO_MIN_GAIN;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM) break;
		cp->un.ord = MIC_IN_PORT;
		error = 0;
		break;
	case CSAUDIO_OUTPUT:
		if (cp->type != AUDIO_MIXER_ENUM) break;
		cp->un.ord = sc->sc_out_port;
		error = 0;
		break;
	}
	return (error);
}

int
cs4231_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	int err = 0;

	switch (dip->index) {
	case CSAUDIO_MIC_LVL:		/* mono/microphone mixer */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_MIC_MUTE;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_DAC_LVL:		/* dacout */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_DAC_MUTE;
		strcpy(dip->label.name, AudioNdac);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_LINE_IN_LVL:	/* line */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_LINE_IN_MUTE;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_CD_LVL:		/* cd */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_CD_MUTE;
		strcpy(dip->label.name, AudioNcd);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_MONITOR_LVL:	/* monitor level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_MONITOR_MUTE;
		strcpy(dip->label.name, AudioNmonitor);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_OUTPUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_OUTPUT_MUTE;
		strcpy(dip->label.name, AudioNoutput);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_LINE_IN_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_LINE_IN_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_DAC_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_DAC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_CD_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_CD_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_MIC_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_MIC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_MONITOR_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_MONITOR_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_OUTPUT_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_OUTPUT_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;

	mute:
		strcpy(dip->label.name, AudioNmute);
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNon);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNoff);
		dip->un.e.member[1].ord = 1;
		break;
	case CSAUDIO_REC_LVL:		/* record level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_RECORD_SOURCE;
		strcpy(dip->label.name, AudioNrecord);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_RECORD_SOURCE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = CSAUDIO_REC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->un.e.num_mem = 3;
		strcpy(dip->un.e.member[0].label.name, AudioNcd);
		dip->un.e.member[0].ord = DAC_IN_PORT;
		strcpy(dip->un.e.member[1].label.name, AudioNmicrophone);
		dip->un.e.member[1].ord = MIC_IN_PORT;
		strcpy(dip->un.e.member[2].label.name, AudioNdac);
		dip->un.e.member[2].ord = AUX1_IN_PORT;
		strcpy(dip->un.e.member[3].label.name, AudioNline);
		dip->un.e.member[3].ord = LINE_IN_PORT;
		break;
	case CSAUDIO_OUTPUT:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNoutput);
		dip->un.e.num_mem = 3;
		strcpy(dip->un.e.member[0].label.name, AudioNspeaker);
		dip->un.e.member[0].ord = CSPORT_SPEAKER;
		strcpy(dip->un.e.member[1].label.name, AudioNline);
		dip->un.e.member[1].ord = CSPORT_LINEOUT;
		strcpy(dip->un.e.member[2].label.name, AudioNheadphone);
		dip->un.e.member[2].ord = CSPORT_HEADPHONE;
		break;
	case CSAUDIO_INPUT_CLASS:	/* input class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		break;
	case CSAUDIO_OUTPUT_CLASS:	/* output class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		break;
	case CSAUDIO_MONITOR_CLASS:	/* monitor class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCmonitor);
		break;
	case CSAUDIO_RECORD_CLASS:	/* record class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		break;
	default:
		err = ENXIO;
	}

	return (err);
}

void *
cs4231_alloc(addr, direction, size, pool, flags)
	void *addr;
	int direction;
	size_t size;
	int pool;
	int flags;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	struct cs_dma *p;

	p = (struct cs_dma *)malloc(sizeof(struct cs_dma), pool, flags);
	if (p == NULL)
		return (NULL);

	p->addr_dva = dvma_malloc(size, &p->addr, flags);
	if (p->addr_dva == NULL) {
		free(p, pool);
		return (NULL);
	}

	p->size = size;
	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return (p->addr);
}

void
cs4231_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	struct cs4231_softc *sc = addr;
	struct cs_dma *p, **pp;

	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &(*pp)->next) {
		if (p->addr != ptr)
			continue;
		dvma_free(p->addr_dva, 16*1024, &p->addr);
		*pp = p->next;
		free(p, pool);
		return;
	}
	printf("%s: attempt to free rogue pointer\n", sc->sc_dev.dv_xname);
}

size_t
cs4231_round_buffersize(addr, direction, size)
	void *addr;
	int direction;
	size_t size;
{
	return (size);
}

int
cs4231_get_props(addr)
	void *addr;
{
	return (AUDIO_PROP_FULLDUPLEX);
}

int
cs4231_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr, *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct cs4231_softc *sc = addr;
	struct cs4231_regs *regs = sc->sc_regs;
	struct cs_dma *p;
	u_int8_t reg;
	u_int32_t n, csr;

	if (sc->sc_locked != 0) {
		printf("cs4231_trigger_output: already running\n");
		return (EINVAL);
	}

	sc->sc_locked = 1;
	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	p = sc->sc_dmas;
	while (p != NULL && p->addr != start)
		p = p->next;
	if (p == NULL) {
		printf("cs4231_trigger_output: bad addr: %x\n", start);
		return (EINVAL);
	}

	n = (char *)end - (char *)start;

	/*
	 * Do only `blksize' at a time, so audio_pint() is kept
	 * synchronous with us...
	 */
	sc->sc_blksz = blksize;
	sc->sc_nowplaying = p;
	sc->sc_playsegsz = n;

	if (n > sc->sc_blksz)
		n = sc->sc_blksz;

	sc->sc_playcnt = n;

	csr = regs->dma_csr;
	regs->dma_pnva = (u_int32_t)p->addr_dva;
	regs->dma_pnc = n;

	if ((csr & APC_CSR_PDMA_GO) == 0 || (csr & APC_CSR_PPAUSE) != 0) {
		regs->dma_csr &= ~(APC_CSR_PIE | APC_CSR_PPAUSE);
		regs->dma_csr |= APC_CSR_EI | APC_CSR_GIE |
				 APC_CSR_PIE | APC_CSR_EIE |
				 APC_CSR_PMIE | APC_CSR_PDMA_GO;
		regs->iar = SP_LOWER_BASE_COUNT;
		regs->idr = 0xff;
		regs->iar = SP_UPPER_BASE_COUNT;
		regs->idr = 0xff;
		regs->iar = SP_INTERFACE_CONFIG;
		reg = regs->idr | PLAYBACK_ENABLE;
		regs->iar = SP_INTERFACE_CONFIG;
		regs->idr = reg;
	}
	return (0);
}

int
cs4231_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr, *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	return (ENXIO);
}

#endif /* NAUDIO > 0 */
