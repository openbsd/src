/*	$OpenBSD: cs4231.c,v 1.21 2004/09/29 07:35:11 miod Exp $	*/

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
#define	CSPORT_MICROPHONE	9

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
#define	CS_AFS_TI		0x40		/* timer interrupt */
#define	CS_AFS_CI		0x20		/* capture interrupt */
#define	CS_AFS_PI		0x10		/* playback interrupt */
#define	CS_AFS_CU		0x08		/* capture underrun */
#define	CS_AFS_CO		0x04		/* capture overrun */
#define	CS_AFS_PO		0x02		/* playback overrun */
#define	CS_AFS_PU		0x01		/* playback underrun */

#define CS_TIMEOUT		90000		/* recalibration timeout */

int	cs4231_match(struct device *, void *, void *);
void	cs4231_attach(struct device *, struct device *, void *);
int	cs4231_intr(void *);

int	cs4231_set_speed(struct cs4231_softc *, u_long *);
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
void cs4231_write(struct cs4231_softc *, u_int8_t, u_int8_t);
u_int8_t cs4231_read(struct cs4231_softc *, u_int8_t);

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
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih, IPL_AUHARD,
	    self->dv_xname);

	printf(" pri %d, softpri %d\n", pri, IPL_AUSOFT);

	audio_attach_mi(&cs4231_sa_hw_if, sc, &sc->sc_dev);

	/* Default to speaker, unmuted, reasonable volume */
	sc->sc_out_port = CSPORT_SPEAKER;
	sc->sc_in_port = CSPORT_MICROPHONE;
	sc->sc_mute[CSPORT_SPEAKER] = 1;
	sc->sc_mute[CSPORT_MONITOR] = 1;
	sc->sc_volume[CSPORT_SPEAKER].left = 192;
	sc->sc_volume[CSPORT_SPEAKER].right = 192;
}

void
cs4231_write(sc, r, v)
	struct cs4231_softc *sc;
	u_int8_t r, v;
{
	sc->sc_regs->iar = r;
	sc->sc_regs->idr = v;
}

u_int8_t
cs4231_read(sc, r)
	struct cs4231_softc *sc;
	u_int8_t r;
{
	sc->sc_regs->iar = r;
	return (sc->sc_regs->idr);
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
	struct cs_channel *chan;
	struct cs_dma *p;
	u_int32_t csr;
	u_int8_t reg, status;
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
				cs4231_write(sc, SP_LOWER_BASE_COUNT, 0xff);
				cs4231_write(sc, SP_UPPER_BASE_COUNT, 0xff);
			}
			if (reg & CS_AFS_CI) {
				cs4231_write(sc, CS_LOWER_REC_CNT, 0xff);
				cs4231_write(sc, CS_UPPER_REC_CNT, 0xff);
			}
			regs->status = 0;
		}
		r = 1;
	}

	if ((csr & APC_CSR_PMIE) && (csr & APC_CSR_PMI)) {
		u_int32_t nextaddr, togo;

		chan = &sc->sc_playback;

		p = chan->cs_curdma;
		togo = chan->cs_segsz - chan->cs_cnt;
		if (togo == 0) {
			nextaddr = (u_int32_t)p->addr_dva;
			chan->cs_cnt = togo = chan->cs_blksz;
		} else {
			nextaddr = regs->dma_pnva + chan->cs_blksz;
			if (togo > chan->cs_blksz)
				togo = chan->cs_blksz;
			chan->cs_cnt += togo;
		}

		regs->dma_pnva = nextaddr;
		regs->dma_pnc = togo;

		if (chan->cs_intr != NULL)
			(*chan->cs_intr)(chan->cs_arg);
		r = 1;
	}

	if ((csr & APC_CSR_CIE) && (csr & APC_CSR_CI)) {
		if (csr & APC_CSR_CD) {
			u_int32_t nextaddr, togo;

			chan = &sc->sc_capture;
			p = chan->cs_curdma;
			togo = chan->cs_segsz - chan->cs_cnt;
			if (togo == 0) {
				nextaddr = (u_int32_t)p->addr_dva;
				chan->cs_cnt = togo = chan->cs_blksz;
			} else {
				nextaddr = regs->dma_cnva + chan->cs_blksz;
				if (togo > chan->cs_blksz)
					togo = chan->cs_blksz;
				chan->cs_cnt += togo;
			}

			regs->dma_cnva = nextaddr;
			regs->dma_cnc = togo;

			if (chan->cs_intr != NULL)
				(*chan->cs_intr)(chan->cs_arg);
		}
		r = 1;
	}

	if ((csr & APC_CSR_CMIE) && (csr & APC_CSR_CMI)) {
		/* capture empty */
		r = 1;
	}

	return (r);
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

	if (selected == -1)
		selected = 3;

	sc->sc_speed_bits = speed_table[selected].bits;
	sc->sc_need_commit = 1;
	*argp = speed_table[selected].speed;

	return (0);
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
	int tries;

	if (sc->sc_open)
		return (EBUSY);
	sc->sc_open = 1;

	sc->sc_capture.cs_intr = NULL;
	sc->sc_capture.cs_arg = NULL;
	sc->sc_capture.cs_locked = 0;

	sc->sc_playback.cs_intr = NULL;
	sc->sc_playback.cs_arg = NULL;
	sc->sc_playback.cs_locked = 0;

	regs->dma_csr = APC_CSR_RESET;
	DELAY(10);
	regs->dma_csr = 0;
	DELAY(10);
	regs->dma_csr |= APC_CSR_CODEC_RESET;

	DELAY(20);

	regs->dma_csr &= ~(APC_CSR_CODEC_RESET);

	for (tries = CS_TIMEOUT; tries && regs->iar == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout waiting for reset\n", sc->sc_dev.dv_xname);

	/* Turn on cs4231 mode */
	cs4231_write(sc, SP_MISC_INFO,
	    cs4231_read(sc, SP_MISC_INFO) | MODE2);

	cs4231_setup_output(sc);

	cs4231_write(sc, SP_PIN_CONTROL,
	    cs4231_read(sc, SP_PIN_CONTROL) | INTERRUPT_ENABLE);

	return (0);
}

void
cs4231_setup_output(sc)
	struct cs4231_softc *sc;
{
	u_int8_t pc, mi, rm, lm;

	pc = cs4231_read(sc, SP_PIN_CONTROL) | CS_PC_HDPHMUTE | CS_PC_LINEMUTE;

	mi = cs4231_read(sc, CS_MONO_IO_CONTROL) | MONO_OUTPUT_MUTE;

	lm = cs4231_read(sc, SP_LEFT_OUTPUT_CONTROL);
	lm &= ~OUTPUT_ATTEN_BITS;
	lm |= ((~(sc->sc_volume[CSPORT_SPEAKER].left >> 2)) &
	    OUTPUT_ATTEN_BITS) | OUTPUT_MUTE;

	rm = cs4231_read(sc, SP_RIGHT_OUTPUT_CONTROL);
	rm &= ~OUTPUT_ATTEN_BITS;
	rm |= ((~(sc->sc_volume[CSPORT_SPEAKER].right >> 2)) &
	    OUTPUT_ATTEN_BITS) | OUTPUT_MUTE;

	if (sc->sc_mute[CSPORT_MONITOR]) {
		lm &= ~OUTPUT_MUTE;
		rm &= ~OUTPUT_MUTE;
	}

	switch (sc->sc_out_port) {
	case CSPORT_HEADPHONE:
		if (sc->sc_mute[CSPORT_SPEAKER])
			pc &= ~CS_PC_HDPHMUTE;
		break;
	case CSPORT_SPEAKER:
		if (sc->sc_mute[CSPORT_SPEAKER])
			mi &= ~MONO_OUTPUT_MUTE;
		break;
	case CSPORT_LINEOUT:
		if (sc->sc_mute[CSPORT_SPEAKER])
			pc &= ~CS_PC_LINEMUTE;
		break;
	}

	cs4231_write(sc, SP_LEFT_OUTPUT_CONTROL, lm);
	cs4231_write(sc, SP_RIGHT_OUTPUT_CONTROL, rm);
	cs4231_write(sc, SP_PIN_CONTROL, pc);
	cs4231_write(sc, CS_MONO_IO_CONTROL, mi);

	/* XXX doesn't really belong here... */
	switch (sc->sc_in_port) {
	case CSPORT_LINEIN:
		pc = LINE_INPUT;
		break;
	case CSPORT_AUX1:
		pc = AUX_INPUT;
		break;
	case CSPORT_DAC:
		pc = MIXED_DAC_INPUT;
		break;
	case CSPORT_MICROPHONE:
	default:
		pc = MIC_INPUT;
		break;
	}
	lm = cs4231_read(sc, SP_LEFT_INPUT_CONTROL);
	rm = cs4231_read(sc, SP_RIGHT_INPUT_CONTROL);
	lm &= ~(MIXED_DAC_INPUT | ATTEN_22_5);
	rm &= ~(MIXED_DAC_INPUT | ATTEN_22_5);
	lm |= pc | (sc->sc_adc.left >> 4);
	rm |= pc | (sc->sc_adc.right >> 4);
	cs4231_write(sc, SP_LEFT_INPUT_CONTROL, lm);
	cs4231_write(sc, SP_RIGHT_INPUT_CONTROL, rm);
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
		strlcpy(fp->name, AudioEmulaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strlcpy(fp->name, AudioEalaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 2:
		strlcpy(fp->name, AudioEslinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 3:
		strlcpy(fp->name, AudioEulinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strlcpy(fp->name, AudioEslinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strlcpy(fp->name, AudioEslinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strlcpy(fp->name, AudioEulinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strlcpy(fp->name, AudioEulinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 8:
		strlcpy(fp->name, AudioEadpcm, sizeof fp->name);
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
	int err, bits, enc = p->encoding;
	void (*pswcode)(void *, u_char *, int cnt) = NULL;
	void (*rswcode)(void *, u_char *, int cnt) = NULL;

	switch (enc) {
	case AUDIO_ENCODING_ULAW:
		if (p->precision != 8)
			return (EINVAL);
		bits = FMT_ULAW >> 5;
		break;
	case AUDIO_ENCODING_ALAW:
		if (p->precision != 8)
			return (EINVAL);
		bits = FMT_ALAW >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 8) {
			bits = FMT_PCM8 >> 5;
			pswcode = rswcode = change_sign8;
		} else if (p->precision == 16)
			bits = FMT_TWOS_COMP >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_ULINEAR:
		if (p->precision != 8)
			return (EINVAL);
		bits = FMT_PCM8 >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision == 8) {
			bits = FMT_PCM8 >> 5;
			pswcode = rswcode = change_sign8;
		} else if (p->precision == 16)
			bits = FMT_TWOS_COMP_BE >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_SLINEAR:
		if (p->precision != 8)
			return (EINVAL);
		bits = FMT_PCM8 >> 5;
		pswcode = rswcode = change_sign8;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 8)
			bits = FMT_PCM8 >> 5;
		else if (p->precision == 16) {
			bits = FMT_TWOS_COMP >> 5;
			pswcode = rswcode = change_sign16_le;
		} else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (p->precision == 8)
			bits = FMT_PCM8 >> 5;
		else if (p->precision == 16) {
			bits = FMT_TWOS_COMP_BE >> 5;
			pswcode = rswcode = change_sign16_be;
		} else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_ADPCM:
		if (p->precision != 8)
			return (EINVAL);
		bits = FMT_ADPCM >> 5;
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
	u_int8_t r, fs;

	if (sc->sc_need_commit == 0)
		return (0);

	fs = sc->sc_speed_bits | (sc->sc_format_bits << 5);
	if (sc->sc_channels == 2)
		fs |= FMT_STEREO;

	s = splaudio();

	r = cs4231_read(sc, SP_INTERFACE_CONFIG) | AUTO_CAL_ENABLE;
	regs->iar = MODE_CHANGE_ENABLE;
	regs->iar = MODE_CHANGE_ENABLE | SP_INTERFACE_CONFIG;
	regs->idr = r;

	regs->iar = MODE_CHANGE_ENABLE | SP_CLOCK_DATA_FORMAT;
	regs->idr = fs;
	r = regs->idr;
	r = regs->idr;
	tries = CS_TIMEOUT;
	for (tries = CS_TIMEOUT; tries && regs->iar == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout committing fspb\n", sc->sc_dev.dv_xname);

	regs->iar = MODE_CHANGE_ENABLE | CS_REC_FORMAT;
	regs->idr = fs;
	r = regs->idr;
	r = regs->idr;
	for (tries = CS_TIMEOUT; tries && regs->iar == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout committing cdf\n", sc->sc_dev.dv_xname);

	regs->iar = 0;
	for (tries = CS_TIMEOUT; tries && regs->iar == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout waiting for !mce\n", sc->sc_dev.dv_xname);

	regs->iar = SP_TEST_AND_INIT;
	for (tries = CS_TIMEOUT; tries && regs->idr & AUTO_CAL_IN_PROG; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout waiting for autocalibration\n",
		    sc->sc_dev.dv_xname);

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

	/* XXX Kills some capture bits */
	regs->dma_csr &= ~(APC_CSR_EI | APC_CSR_GIE | APC_CSR_PIE |
	    APC_CSR_EIE | APC_CSR_PDMA_GO | APC_CSR_PMIE);
	regs->iar = SP_INTERFACE_CONFIG;
	r = regs->idr & (~PLAYBACK_ENABLE);
	regs->iar = SP_INTERFACE_CONFIG;
	regs->idr = r;
	sc->sc_playback.cs_locked = 0;
	return (0);
}

int
cs4231_halt_input(addr)
	void *addr;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	struct cs4231_regs *regs = sc->sc_regs;

	/* XXX Kills some playback bits */
	regs->dma_csr = APC_CSR_CAPTURE_PAUSE;
	regs->iar = SP_INTERFACE_CONFIG;
	regs->idr &= ~CAPTURE_ENABLE;
	sc->sc_capture.cs_locked = 0;
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
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
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
		if (cp->un.value.num_channels == 1) {
			sc->sc_adc.left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_adc.right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_adc.left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_adc.right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		cs4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		if (cp->un.ord == CSPORT_MICROPHONE ||
		    cp->un.ord == CSPORT_LINEIN ||
		    cp->un.ord == CSPORT_AUX1 ||
		    cp->un.ord == CSPORT_DAC) {
			sc->sc_in_port = cp->un.ord;
			error = 0;
			cs4231_setup_output(sc);
		}
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
			    sc->sc_adc.left;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_adc.left;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_adc.right;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_in_port;
		error = 0;
		break;
	case CSAUDIO_OUTPUT:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
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
		strlcpy(dip->label.name, AudioNmicrophone,
		    sizeof dip->label.name);
		dip->un.v.num_channels = 1;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_DAC_LVL:		/* dacout */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_DAC_MUTE;
		strlcpy(dip->label.name, AudioNdac, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_LINE_IN_LVL:	/* line */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_LINE_IN_MUTE;
		strlcpy(dip->label.name, AudioNline, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_CD_LVL:		/* cd */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_CD_MUTE;
		strlcpy(dip->label.name, AudioNcd, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_MONITOR_LVL:	/* monitor level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_MONITOR_MUTE;
		strlcpy(dip->label.name, AudioNmonitor, sizeof dip->label.name);
		dip->un.v.num_channels = 1;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_OUTPUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_OUTPUT_MUTE;
		strlcpy(dip->label.name, AudioNoutput, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
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
		strlcpy(dip->label.name, AudioNmute, sizeof dip->label.name);
		dip->un.e.num_mem = 2;
		strlcpy(dip->un.e.member[0].label.name, AudioNon,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = 0;
		strlcpy(dip->un.e.member[1].label.name, AudioNoff,
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = 1;
		break;
	case CSAUDIO_REC_LVL:		/* record level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_RECORD_SOURCE;
		strlcpy(dip->label.name, AudioNrecord, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_RECORD_SOURCE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = CSAUDIO_REC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioNsource, sizeof dip->label.name);
		dip->un.e.num_mem = 4;
		strlcpy(dip->un.e.member[0].label.name, AudioNmicrophone,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = CSPORT_MICROPHONE;
		strlcpy(dip->un.e.member[1].label.name, AudioNline,
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = CSPORT_LINEIN;
		strlcpy(dip->un.e.member[2].label.name, AudioNcd,
		    sizeof dip->un.e.member[2].label.name);
		dip->un.e.member[2].ord = CSPORT_AUX1;
		strlcpy(dip->un.e.member[3].label.name, AudioNdac,
		    sizeof dip->un.e.member[3].label.name);
		dip->un.e.member[3].ord = CSPORT_DAC;
		break;
	case CSAUDIO_OUTPUT:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioNoutput, sizeof dip->label.name);
		dip->un.e.num_mem = 3;
		strlcpy(dip->un.e.member[0].label.name, AudioNspeaker,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = CSPORT_SPEAKER;
		strlcpy(dip->un.e.member[1].label.name, AudioNline,
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = CSPORT_LINEOUT;
		strlcpy(dip->un.e.member[2].label.name, AudioNheadphone,
		    sizeof dip->un.e.member[2].label.name);
		dip->un.e.member[2].ord = CSPORT_HEADPHONE;
		break;
	case CSAUDIO_INPUT_CLASS:	/* input class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioCinputs, sizeof dip->label.name);
		break;
	case CSAUDIO_OUTPUT_CLASS:	/* output class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioCoutputs, sizeof dip->label.name);
		break;
	case CSAUDIO_MONITOR_CLASS:	/* monitor class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioCmonitor, sizeof dip->label.name);
		break;
	case CSAUDIO_RECORD_CLASS:	/* record class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioCrecord, sizeof dip->label.name);
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
	struct cs_channel *chan = &sc->sc_playback;
	struct cs_dma *p;
	u_int8_t reg;
	u_int32_t n, csr;

	if (chan->cs_locked != 0) {
		printf("cs4231_trigger_output: already running\n");
		return (EINVAL);
	}

	chan->cs_locked = 1;
	chan->cs_intr = intr;
	chan->cs_arg = arg;

	p = sc->sc_dmas;
	while (p != NULL && p->addr != start)
		p = p->next;
	if (p == NULL) {
		printf("cs4231_trigger_output: bad addr: %p\n", start);
		return (EINVAL);
	}

	n = (char *)end - (char *)start;

	/*
	 * Do only `blksize' at a time, so audio_pint() is kept
	 * synchronous with us...
	 */
	chan->cs_blksz = blksize;
	chan->cs_curdma = p;
	chan->cs_segsz = n;

	if (n > chan->cs_blksz)
		n = chan->cs_blksz;

	chan->cs_cnt = n;

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
	struct cs4231_softc *sc = addr;
	struct cs_channel *chan = &sc->sc_capture;
	struct cs_dma *p;
	u_int32_t csr;
	u_long n;

	if (chan->cs_locked != 0) {
		printf("%s: trigger_input: already running\n",
		    sc->sc_dev.dv_xname);
		return (EINVAL);
	}
	chan->cs_locked = 1;
	chan->cs_intr = intr;
	chan->cs_arg = arg;

	for (p = sc->sc_dmas; p->addr != start; p = p->next)
		/*EMPTY*/;
	if (p == NULL) {
		printf("%s: trigger_input: bad addr: %p\n",
		    sc->sc_dev.dv_xname, start);
		return (EINVAL);
	}

	n = (char *)end - (char *)start;

	/*
	 * Do only `blksize' at a time, so audio_cint() is kept
	 * synchronous with us...
	 */
	chan->cs_blksz = blksize;
	chan->cs_curdma = p;
	chan->cs_segsz = n;

	if (n > chan->cs_blksz)
		n = chan->cs_blksz;
	chan->cs_cnt = n;

	sc->sc_regs->dma_cnva = (u_int32_t)p->addr_dva;
	sc->sc_regs->dma_cnc = n;

	csr = sc->sc_regs->dma_csr;
	if ((csr & APC_CSR_CDMA_GO) == 0 || (csr & APC_CSR_CPAUSE) != 0) {
		csr &= APC_CSR_CPAUSE;
		csr |= APC_CSR_GIE | APC_CSR_CMIE | APC_CSR_CIE | APC_CSR_EI |
		    APC_CSR_CDMA_GO;
		sc->sc_regs->dma_csr = csr;
		cs4231_write(sc, CS_LOWER_REC_CNT, 0xff);
		cs4231_write(sc, CS_UPPER_REC_CNT, 0xff);
		cs4231_write(sc, SP_INTERFACE_CONFIG,
		    cs4231_read(sc, SP_INTERFACE_CONFIG) | CAPTURE_ENABLE);
	}

	if (sc->sc_regs->dma_csr & APC_CSR_CD) {
		u_long nextaddr, togo;

		p = chan->cs_curdma;
		togo = chan->cs_segsz - chan->cs_cnt;
		if (togo == 0) {
			nextaddr = (u_int32_t)p->addr_dva;
			chan->cs_cnt = togo = chan->cs_blksz;
		} else {
			nextaddr = sc->sc_regs->dma_cnva + chan->cs_blksz;
			if (togo > chan->cs_blksz)
				togo = chan->cs_blksz;
			chan->cs_cnt += togo;
		}

		sc->sc_regs->dma_cnva = nextaddr;
		sc->sc_regs->dma_cnc = togo;
	}

	return (0);
}

#endif /* NAUDIO > 0 */
