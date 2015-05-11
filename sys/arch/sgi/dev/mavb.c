/*	$OpenBSD: mavb.c,v 1.18 2015/05/11 06:46:21 ratchov Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <sgi/localbus/macebus.h>
#include <sgi/localbus/macebusvar.h>
#include <sgi/dev/mavbreg.h>

#include <dev/ic/ad1843reg.h>

#undef MAVB_DEBUG

#ifdef MAVB_DEBUG
#define DPRINTF(l,x)	do { if (mavb_debug & (l)) printf x; } while (0)
#define MAVB_DEBUG_INTR		0x0100
int mavb_debug = ~MAVB_DEBUG_INTR;
#else
#define DPRINTF(l,x)	/* nothing */
#endif

/* Repeat delays for volume buttons.  */
#define MAVB_VOLUME_BUTTON_REPEAT_DEL1	400	/* 400ms to start repeating */
#define MAVB_VOLUME_BUTTON_REPEAT_DELN  100	/* 100ms between repeats */

/* XXX We need access to some of the MACE ISA registers.  */
#define MAVB_ISA_NREGS				0x20

#define MAVB_ISA_RING_SIZE	0x4000 /* Mace ISA DMA ring size. */
#define MAVB_CHAN_RING_SIZE	0x1000 /* DMA buffer size per channel. */
#define MAVB_CHAN_INTR_SIZE	0x0800 /* Interrupt on 50% buffer transfer. */
#define MAVB_CHAN_CHUNK_SIZE	0x0400 /* Move data in 25% buffer chunks. */


/*
 * AD1843 Mixer.
 */

enum {
	AD1843_RECORD_CLASS,
	AD1843_ADC_SOURCE,	/* ADC Source Select */
	AD1843_ADC_GAIN,	/* ADC Input Gain */
	AD1843_ADC_MIC_GAIN,	/* ADC Mic Input Gain */

	AD1843_INPUT_CLASS,
	AD1843_DAC1_GAIN,	/* DAC1 Analog/Digital Gain/Attenuation */
	AD1843_DAC1_MUTE,	/* DAC1 Analog Mute */
	AD1843_DAC2_GAIN,	/* DAC2 Mix Gain */
	AD1843_AUX1_GAIN,	/* Auxilliary 1 Mix Gain */
	AD1843_AUX2_GAIN,	/* Auxilliary 2 Mix Gain */
	AD1843_AUX3_GAIN,	/* Auxilliary 3 Mix Gain */
	AD1843_MIC_GAIN,	/* Microphone Mix Gain */
	AD1843_MONO_GAIN,	/* Mono Mix Gain */
	AD1843_DAC2_MUTE,	/* DAC2 Mix Mute */
	AD1843_AUX1_MUTE,	/* Auxilliary 1 Mix Mute */
	AD1843_AUX2_MUTE,	/* Auxilliary 2 Mix Mute */
	AD1843_AUX3_MUTE,	/* Auxilliary 3 Mix Mute */
	AD1843_MIC_MUTE,	/* Microphone Mix Mute */
	AD1843_MONO_MUTE,	/* Mono Mix Mute */
	AD1843_SUM_MUTE,	/* Sum Mute */

	AD1843_OUTPUT_CLASS,
	AD1843_MNO_MUTE,	/* Mono Output Mute */
	AD1843_HPO_MUTE		/* Headphone Output Mute */
};

/* ADC Source Select.  The order matches the hardware bits.  */
const char *ad1843_source[] = {
	AudioNline,
	AudioNmicrophone,
	AudioNaux "1",
	AudioNaux "2",
	AudioNaux "3",
	AudioNmono,
	AudioNdac "1",
	AudioNdac "2"
};

/* Mix Control.  The order matches the hardware register numbering.  */
const char *ad1843_input[] = {
	AudioNdac "2",		/* AD1843_DAC2__TO_MIXER */
	AudioNaux "1",
	AudioNaux "2",
	AudioNaux "3",
	AudioNmicrophone,
	AudioNmono		/* AD1843_MISC_SETTINGS */
};

struct mavb_chan {
	caddr_t hw_start;
	caddr_t sw_start;
	caddr_t sw_end;
	caddr_t sw_cur;
	void (*intr)(void *);
	void *intrarg;
	u_long rate;
	u_int format;
	int blksize;
};

struct mavb_softc {
	struct device sc_dev;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap;

	/* XXX We need access to some of the MACE ISA registers.  */
	bus_space_handle_t sc_isash;

	caddr_t sc_ring;

	struct mavb_chan play;
	struct mavb_chan rec;

	struct timeout sc_volume_button_to;
};

typedef u_long ad1843_addr_t;

u_int16_t ad1843_reg_read(struct mavb_softc *, ad1843_addr_t);
u_int16_t ad1843_reg_write(struct mavb_softc *, ad1843_addr_t, u_int16_t);
void ad1843_dump_regs(struct mavb_softc *);

int mavb_match(struct device *, void *, void *);
void mavb_attach(struct device *, struct device *, void *);

struct cfattach mavb_ca = {
	sizeof(struct mavb_softc), mavb_match, mavb_attach
};

struct cfdriver mavb_cd = {
	NULL, "mavb", DV_DULL
};

int mavb_open(void *, int);
void mavb_close(void *);
int mavb_query_encoding(void *, struct audio_encoding *);
int mavb_set_params(void *, int, int, struct audio_params *,
		    struct audio_params *);
int mavb_round_blocksize(void *hdl, int bs);
int mavb_halt_output(void *);
int mavb_halt_input(void *);
int mavb_getdev(void *, struct audio_device *);
int mavb_set_port(void *, struct mixer_ctrl *);
int mavb_get_port(void *, struct mixer_ctrl *);
int mavb_query_devinfo(void *, struct mixer_devinfo *);
int mavb_get_props(void *);
int mavb_trigger_output(void *, void *, void *, int, void (*)(void *),
			void *, struct audio_params *);
int mavb_trigger_input(void *, void *, void *, int, void (*)(void *),
		       void *, struct audio_params *);
void mavb_get_default_params(void *, int, struct audio_params *);

struct audio_hw_if mavb_sa_hw_if = {
	mavb_open,
	mavb_close,
	0,
	mavb_query_encoding,
	mavb_set_params,
	mavb_round_blocksize,
	0,
	0,
	0,
	0,
	0,
	mavb_halt_output,
	mavb_halt_input,
	0,
	mavb_getdev,
	0,
	mavb_set_port,
	mavb_get_port,
	mavb_query_devinfo,
	0,
	0,
	0,
	0,
	mavb_get_props,
	mavb_trigger_output,
	mavb_trigger_input,
	mavb_get_default_params
};

struct audio_device mavb_device = {
	"A3",
	"",
	"mavb"
};

int
mavb_open(void *hdl, int flags)
{
	return (0);
}

void
mavb_close(void *hdl)
{
}

int
mavb_query_encoding(void *hdl, struct audio_encoding *ae)
{
	switch (ae->index) {
	case 0:
		/* 24-bit Signed Linear PCM LSB-aligned.  */
		strlcpy(ae->name, AudioEslinear_be, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_SLINEAR_BE;
		ae->precision = 24;
		ae->flags = 0;
		break;
	default:
		return (EINVAL);
	}
	ae->bps = AUDIO_BPS(ae->precision);
	ae->msb = 0;
	return (0);
}

void
mavb_get_default_params(void *hdl, int mode, struct audio_params *p)
{
	p->sample_rate = 48000;
	p->encoding = AUDIO_ENCODING_SLINEAR_BE;
	p->precision = 24;
	p->bps = 4;
	p->msb = 0;
	p->channels = 2;
}

static int
mavb_set_play_rate(struct mavb_softc *sc, u_long sample_rate)
{
	if (sample_rate < 4000 || sample_rate > 48000)
		return (EINVAL);

	if (sc->play.rate != sample_rate) {
		ad1843_reg_write(sc, AD1843_CLOCK2_SAMPLE_RATE, sample_rate);
		sc->play.rate = sample_rate;
	}
	return (0);
}

static int
mavb_set_rec_rate(struct mavb_softc *sc, u_long sample_rate)
{
	if (sample_rate < 4000 || sample_rate > 48000)
		return (EINVAL);

	if (sc->rec.rate != sample_rate) {
		ad1843_reg_write(sc, AD1843_CLOCK1_SAMPLE_RATE, sample_rate);
		sc->rec.rate = sample_rate;
	}
	return (0);
}

static int
mavb_get_format(u_int encoding, u_int *format)
{
	switch(encoding) {
	case AUDIO_ENCODING_ULINEAR_BE:
		*format = AD1843_PCM8;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		*format = AD1843_PCM16;
		break;
	case AUDIO_ENCODING_ULAW:
		*format = AD1843_ULAW;
		break;
	case AUDIO_ENCODING_ALAW:
		*format = AD1843_ALAW;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
mavb_set_play_format(struct mavb_softc *sc, u_int encoding)
{
	u_int16_t value;
	u_int format;
	int err;

	err = mavb_get_format(encoding, &format);
	if (err)
		return (err);

	if (sc->play.format != format) {
		value = ad1843_reg_read(sc, AD1843_SERIAL_INTERFACE);
		value &= ~AD1843_DA1F_MASK;
		value |= (format << AD1843_DA1F_SHIFT);
		ad1843_reg_write(sc, AD1843_SERIAL_INTERFACE, value);
		sc->play.format = format;
	}
	return (0);
}

static int
mavb_set_rec_format(struct mavb_softc *sc, u_int encoding)
{
	u_int16_t value;
	u_int format;
	int err;

	err = mavb_get_format(encoding, &format);
	if (err)
		return (err);

	if (sc->rec.format != format) {
		value = ad1843_reg_read(sc, AD1843_SERIAL_INTERFACE);
		value &= ~(AD1843_ADRF_MASK | AD1843_ADLF_MASK);
		value |= (format << AD1843_ADRF_SHIFT) |
		    (format << AD1843_ADLF_SHIFT);
		ad1843_reg_write(sc, AD1843_SERIAL_INTERFACE, value);
		sc->rec.format = format;
	}
	return (0);
}

int
mavb_set_params(void *hdl, int setmode, int usemode,
    struct audio_params *play, struct audio_params *rec)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;
	int error;

	DPRINTF(1, ("%s: mavb_set_params: sample=%ld precision=%d "
	    "channels=%d\n", sc->sc_dev.dv_xname, play->sample_rate,
	    play->precision, play->channels));

	if (setmode & AUMODE_PLAY) {
		play->encoding = AUDIO_ENCODING_SLINEAR_BE;
		play->channels = 2;
		play->precision = 24;
		play->bps = AUDIO_BPS(play->precision);
		play->msb = 0;
		error = mavb_set_play_rate(sc, play->sample_rate);
		if (error)
			return (error);

		error = mavb_set_play_format(sc, play->encoding);
		if (error)
			return (error);

	}

	if (setmode & AUMODE_RECORD) {
		rec->encoding = AUDIO_ENCODING_SLINEAR_BE;
		rec->channels = 2;
		rec->precision = 24;
		rec->bps = AUDIO_BPS(rec->precision);
		rec->msb = 0;

		error = mavb_set_rec_rate(sc, rec->sample_rate);
		if (error)
			return (error);

		error = mavb_set_rec_format(sc, rec->encoding);
		if (error)
			return (error);
	}

	return (0);
}

int
mavb_round_blocksize(void *hdl, int bs)
{
	if (bs == 0)
		bs = MAVB_CHAN_INTR_SIZE;
	else
		bs = (bs + MAVB_CHAN_INTR_SIZE - 1) &
		    ~(MAVB_CHAN_INTR_SIZE - 1);
	return (bs);
}

int
mavb_halt_output(void *hdl)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;

	DPRINTF(1, ("%s: mavb_halt_output called\n", sc->sc_dev.dv_xname));
	mtx_enter(&audio_lock);
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL, 0);
	mtx_leave(&audio_lock);
	return (0);
}

int
mavb_halt_input(void *hdl)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;

	DPRINTF(1, ("%s: mavb_halt_input called\n", sc->sc_dev.dv_xname));
	mtx_enter(&audio_lock);
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL1_CONTROL, 0);
	mtx_leave(&audio_lock);
	return (0);
}

int
mavb_getdev(void *hdl, struct audio_device *ret)
{
	*ret = mavb_device;
	return (0);
}

int
mavb_set_port(void *hdl, struct mixer_ctrl *mc)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;
	u_char left, right;
	ad1843_addr_t reg;
	u_int16_t value;

	DPRINTF(1, ("%s: mavb_set_port: dev=%d\n", sc->sc_dev.dv_xname,
	    mc->dev));

	switch (mc->dev) {
	case AD1843_ADC_SOURCE:
		value = ad1843_reg_read(sc, AD1843_ADC_SOURCE_GAIN);
		value &= ~(AD1843_LSS_MASK | AD1843_RSS_MASK);
		value |= ((mc->un.ord << AD1843_LSS_SHIFT) & AD1843_LSS_MASK);
		value |= ((mc->un.ord << AD1843_RSS_SHIFT) & AD1843_RSS_MASK);
		ad1843_reg_write(sc, AD1843_ADC_SOURCE_GAIN, value);
		break;
	case AD1843_ADC_GAIN:
		left = mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		right = mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		value = ad1843_reg_read(sc, AD1843_ADC_SOURCE_GAIN);
		value &= ~(AD1843_LIG_MASK | AD1843_RIG_MASK);
		value |= ((left >> 4) << AD1843_LIG_SHIFT);
		value |= ((right >> 4) << AD1843_RIG_SHIFT);
		ad1843_reg_write(sc, AD1843_ADC_SOURCE_GAIN, value);
		break;
	case AD1843_ADC_MIC_GAIN:
		value = ad1843_reg_read(sc, AD1843_ADC_SOURCE_GAIN);
		if (mc->un.ord == 0)
			value &= ~(AD1843_LMGE | AD1843_RMGE);
		else
			value |= (AD1843_LMGE | AD1843_RMGE);
		ad1843_reg_write(sc, AD1843_ADC_SOURCE_GAIN, value);
		break;

	case AD1843_DAC1_GAIN:
		left = AUDIO_MAX_GAIN -
		    mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		right = AUDIO_MAX_GAIN -
                    mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		value = ad1843_reg_read(sc, AD1843_DAC1_ANALOG_GAIN);
		value &= ~(AD1843_LDA1G_MASK | AD1843_RDA1G_MASK);
		value |= ((left >> 2) << AD1843_LDA1G_SHIFT);
		value |= ((right >> 2) << AD1843_RDA1G_SHIFT);
		ad1843_reg_write(sc, AD1843_DAC1_ANALOG_GAIN, value);
		break;
	case AD1843_DAC1_MUTE:
		value = ad1843_reg_read(sc, AD1843_DAC1_ANALOG_GAIN);
		if (mc->un.ord == 0)
			value &= ~(AD1843_LDA1GM | AD1843_RDA1GM);
		else
			value |= (AD1843_LDA1GM | AD1843_RDA1GM);
		ad1843_reg_write(sc, AD1843_DAC1_ANALOG_GAIN, value);
		break;

	case AD1843_DAC2_GAIN:
	case AD1843_AUX1_GAIN:
	case AD1843_AUX2_GAIN:
	case AD1843_AUX3_GAIN:
	case AD1843_MIC_GAIN:
		left = AUDIO_MAX_GAIN -
		    mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		right = AUDIO_MAX_GAIN -
                    mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		reg = AD1843_DAC2_TO_MIXER + mc->dev - AD1843_DAC2_GAIN;
		value = ad1843_reg_read(sc, reg);
		value &= ~(AD1843_LD2M_MASK | AD1843_RD2M_MASK);
		value |= ((left >> 3) << AD1843_LD2M_SHIFT);
		value |= ((right >> 3) << AD1843_RD2M_SHIFT);
		ad1843_reg_write(sc, reg, value);
		break;
	case AD1843_MONO_GAIN:
		left = AUDIO_MAX_GAIN -
		    mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		value &= ~AD1843_MNM_MASK;
		value |= ((left >> 3) << AD1843_MNM_SHIFT);
		ad1843_reg_write(sc, AD1843_MISC_SETTINGS, value);
		break;
	case AD1843_DAC2_MUTE:
	case AD1843_AUX1_MUTE:
	case AD1843_AUX2_MUTE:
	case AD1843_AUX3_MUTE:
	case AD1843_MIC_MUTE:
	case AD1843_MONO_MUTE:	/* matches left channel */
		reg = AD1843_DAC2_TO_MIXER + mc->dev - AD1843_DAC2_MUTE;
		value = ad1843_reg_read(sc, reg);
		if (mc->un.ord == 0)
			value &= ~(AD1843_LD2MM | AD1843_RD2MM);
		else
			value |= (AD1843_LD2MM | AD1843_RD2MM);
		ad1843_reg_write(sc, reg, value);
		break;

	case AD1843_SUM_MUTE:
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		if (mc->un.ord == 0)
			value &= ~AD1843_SUMM;
		else
			value |= AD1843_SUMM;
		ad1843_reg_write(sc, AD1843_MISC_SETTINGS, value);
		break;
		
	case AD1843_MNO_MUTE:
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		if (mc->un.ord == 0)
			value &= ~AD1843_MNOM;
		else
			value |= AD1843_MNOM;
		ad1843_reg_write(sc, AD1843_MISC_SETTINGS, value);
		break;
		
	case AD1843_HPO_MUTE:
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		if (mc->un.ord == 0)
			value &= ~AD1843_HPOM;
		else
			value |= AD1843_HPOM;
		ad1843_reg_write(sc, AD1843_MISC_SETTINGS, value);
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

int
mavb_get_port(void *hdl, struct mixer_ctrl *mc)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;
	u_char left, right;
	ad1843_addr_t reg;
	u_int16_t value;

	DPRINTF(1, ("%s: mavb_get_port: dev=%d\n", sc->sc_dev.dv_xname,
	    mc->dev));

	switch (mc->dev) {
	case AD1843_ADC_SOURCE:
		value = ad1843_reg_read(sc, AD1843_ADC_SOURCE_GAIN);
		mc->un.ord = (value & AD1843_LSS_MASK) >> AD1843_LSS_SHIFT;
		break;
	case AD1843_ADC_GAIN:
		value = ad1843_reg_read(sc, AD1843_ADC_SOURCE_GAIN);
		left = (value & AD1843_LIG_MASK) >> AD1843_LIG_SHIFT;
		right = (value & AD1843_RIG_MASK) >> AD1843_RIG_SHIFT;
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
		    (left << 4) | left;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
		    (right << 4) | right;
		break;
	case AD1843_ADC_MIC_GAIN:
		value = ad1843_reg_read(sc, AD1843_ADC_SOURCE_GAIN);
		mc->un.ord = (value & AD1843_LMGE) ? 1 : 0;
		break;

	case AD1843_DAC1_GAIN:
		value = ad1843_reg_read(sc, AD1843_DAC1_ANALOG_GAIN);
		left = (value & AD1843_LDA1G_MASK) >> AD1843_LDA1G_SHIFT;
		right = (value & AD1843_RDA1G_MASK) >> AD1843_RDA1G_SHIFT;
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
		    AUDIO_MAX_GAIN - (left << 2);
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
		    AUDIO_MAX_GAIN - (right << 2);
		break;
	case AD1843_DAC1_MUTE:
		value = ad1843_reg_read(sc, AD1843_DAC1_ANALOG_GAIN);
		mc->un.ord = (value & AD1843_LDA1GM) ? 1 : 0;
		break;

	case AD1843_DAC2_GAIN:
	case AD1843_AUX1_GAIN:
	case AD1843_AUX2_GAIN:
	case AD1843_AUX3_GAIN:
	case AD1843_MIC_GAIN:
		reg = AD1843_DAC2_TO_MIXER + mc->dev - AD1843_DAC2_GAIN;
		value = ad1843_reg_read(sc, reg);
		left = (value & AD1843_LD2M_MASK) >> AD1843_LD2M_SHIFT;
		right = (value & AD1843_RD2M_MASK) >> AD1843_RD2M_SHIFT;
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
		    AUDIO_MAX_GAIN - (left << 3);
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
		    AUDIO_MAX_GAIN - (right << 3);
		break;
	case AD1843_MONO_GAIN:
		if (mc->un.value.num_channels != 1)
			return (EINVAL);

		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		left = (value & AD1843_MNM_MASK) >> AD1843_MNM_SHIFT;
		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
		    AUDIO_MAX_GAIN - (left << 3);
		break;
	case AD1843_DAC2_MUTE:
	case AD1843_AUX1_MUTE:
	case AD1843_AUX2_MUTE:
	case AD1843_AUX3_MUTE:
	case AD1843_MIC_MUTE:
	case AD1843_MONO_MUTE:	/* matches left channel */
		reg = AD1843_DAC2_TO_MIXER + mc->dev - AD1843_DAC2_MUTE;
		value = ad1843_reg_read(sc, reg);
		mc->un.ord = (value & AD1843_LD2MM) ? 1 : 0;
		break;

	case AD1843_SUM_MUTE:
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		mc->un.ord = (value & AD1843_SUMM) ? 1 : 0;
		break;
		
	case AD1843_MNO_MUTE:
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		mc->un.ord = (value & AD1843_MNOM) ? 1 : 0;
		break;
		
	case AD1843_HPO_MUTE:
		value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
		mc->un.ord = (value & AD1843_HPOM) ? 1 : 0;
		break;
		
	default:
		return (EINVAL);
	}

	return (0);
}

int
mavb_query_devinfo(void *hdl, struct mixer_devinfo *di)
{
	int i;

	di->prev = di->next = AUDIO_MIXER_LAST;

	switch (di->index) {
	case AD1843_RECORD_CLASS:
		di->type = AUDIO_MIXER_CLASS;
		di->mixer_class = AD1843_RECORD_CLASS;
		strlcpy(di->label.name, AudioCrecord, sizeof di->label.name);
		break;

	case AD1843_ADC_SOURCE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = AD1843_RECORD_CLASS;
		di->next = AD1843_ADC_GAIN;
		strlcpy(di->label.name, AudioNsource, sizeof di->label.name);
		di->un.e.num_mem =
			sizeof ad1843_source / sizeof ad1843_source[1];
		for (i = 0; i < di->un.e.num_mem; i++) {
			strlcpy(di->un.e.member[i].label.name,
                            ad1843_source[i],
			    sizeof di->un.e.member[0].label.name);
			di->un.e.member[i].ord = i;
		}
		break;
	case AD1843_ADC_GAIN:
		di->type = AUDIO_MIXER_VALUE;
		di->mixer_class = AD1843_RECORD_CLASS;
		di->prev = AD1843_ADC_SOURCE;
		strlcpy(di->label.name, AudioNvolume, sizeof di->label.name);
		di->un.v.num_channels = 2;
		strlcpy(di->un.v.units.name, AudioNvolume,
		    sizeof di->un.v.units.name);
		break;
	case AD1843_ADC_MIC_GAIN:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = AD1843_RECORD_CLASS;
		strlcpy(di->label.name, AudioNmicrophone "." AudioNpreamp,
		    sizeof di->label.name);
		di->un.e.num_mem = 2;
		strlcpy(di->un.e.member[0].label.name, AudioNoff,
		    sizeof di->un.e.member[0].label.name);
		di->un.e.member[0].ord = 0;
		strlcpy(di->un.e.member[1].label.name, AudioNon,
		    sizeof di->un.e.member[1].label.name);
		di->un.e.member[1].ord = 1;
		break;

	case AD1843_INPUT_CLASS:
		di->type = AUDIO_MIXER_CLASS;
		di->mixer_class = AD1843_INPUT_CLASS;
		strlcpy(di->label.name, AudioCinputs, sizeof di->label.name);
		break;

	case AD1843_DAC1_GAIN:
		di->type = AUDIO_MIXER_VALUE;
		di->mixer_class = AD1843_INPUT_CLASS;
		di->next = AD1843_DAC1_MUTE;
		strlcpy(di->label.name, AudioNdac "1", sizeof di->label.name);
		di->un.v.num_channels = 2;
		strlcpy(di->un.v.units.name, AudioNvolume,
		    sizeof di->un.v.units.name);
		break;
	case AD1843_DAC1_MUTE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = AD1843_INPUT_CLASS;
		di->prev = AD1843_DAC1_GAIN;
		strlcpy(di->label.name, AudioNmute, sizeof di->label.name);
		di->un.e.num_mem = 2;
		strlcpy(di->un.e.member[0].label.name, AudioNoff,
		    sizeof di->un.e.member[0].label.name);
		di->un.e.member[0].ord = 0;
		strlcpy(di->un.e.member[1].label.name, AudioNon,
		    sizeof di->un.e.member[1].label.name);
		di->un.e.member[1].ord = 1;
		break;

	case AD1843_DAC2_GAIN:
	case AD1843_AUX1_GAIN:
	case AD1843_AUX2_GAIN:
	case AD1843_AUX3_GAIN:
	case AD1843_MIC_GAIN:
	case AD1843_MONO_GAIN:
		di->type = AUDIO_MIXER_VALUE;
		di->mixer_class = AD1843_INPUT_CLASS;
		di->next = di->index + AD1843_DAC2_MUTE - AD1843_DAC2_GAIN;
		strlcpy(di->label.name,
                    ad1843_input[di->index - AD1843_DAC2_GAIN],
		    sizeof di->label.name);
		if (di->index == AD1843_MONO_GAIN)
			di->un.v.num_channels = 1;
		else
			di->un.v.num_channels = 2;
		strlcpy(di->un.v.units.name, AudioNvolume,
		    sizeof di->un.v.units.name);
		break;
	case AD1843_DAC2_MUTE:
	case AD1843_AUX1_MUTE:
	case AD1843_AUX2_MUTE:
	case AD1843_AUX3_MUTE:
	case AD1843_MIC_MUTE:
	case AD1843_MONO_MUTE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = AD1843_INPUT_CLASS;
		di->prev = di->index + AD1843_DAC2_GAIN - AD1843_DAC2_MUTE;
		strlcpy(di->label.name, AudioNmute, sizeof di->label.name);
		di->un.e.num_mem = 2;
		strlcpy(di->un.e.member[0].label.name, AudioNoff,
		    sizeof di->un.e.member[0].label.name);
		di->un.e.member[0].ord = 0;
		strlcpy(di->un.e.member[1].label.name, AudioNon,
		    sizeof di->un.e.member[1].label.name);
		di->un.e.member[1].ord = 1;
		break;

	case AD1843_SUM_MUTE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = AD1843_INPUT_CLASS;
		strlcpy(di->label.name, "sum." AudioNmute,
		    sizeof di->label.name);
		di->un.e.num_mem = 2;
		strlcpy(di->un.e.member[0].label.name, AudioNoff,
		    sizeof di->un.e.member[0].label.name);
		di->un.e.member[0].ord = 0;
		strlcpy(di->un.e.member[1].label.name, AudioNon,
		    sizeof di->un.e.member[1].label.name);
		di->un.e.member[1].ord = 1;
		break;

	case AD1843_OUTPUT_CLASS:
		di->type = AUDIO_MIXER_CLASS;
		di->mixer_class = AD1843_OUTPUT_CLASS;
		strlcpy(di->label.name, AudioCoutputs, sizeof di->label.name);
		break;

	case AD1843_MNO_MUTE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = AD1843_OUTPUT_CLASS;
		strlcpy(di->label.name, AudioNmono "." AudioNmute,
		    sizeof di->label.name);
		di->un.e.num_mem = 2;
		strlcpy(di->un.e.member[0].label.name, AudioNoff,
		    sizeof di->un.e.member[0].label.name);
		di->un.e.member[0].ord = 0;
		strlcpy(di->un.e.member[1].label.name, AudioNon,
		    sizeof di->un.e.member[1].label.name);
		di->un.e.member[1].ord = 1;
		break;

	case AD1843_HPO_MUTE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = AD1843_OUTPUT_CLASS;
		strlcpy(di->label.name, AudioNheadphone "." AudioNmute,
		    sizeof di->label.name);
		di->un.e.num_mem = 2;
		strlcpy(di->un.e.member[0].label.name, AudioNoff,
		    sizeof di->un.e.member[0].label.name);
		di->un.e.member[0].ord = 0;
		strlcpy(di->un.e.member[1].label.name, AudioNon,
		    sizeof di->un.e.member[1].label.name);
		di->un.e.member[1].ord = 1;
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

int
mavb_get_props(void *hdl)
{
	return (AUDIO_PROP_FULLDUPLEX | AUDIO_PROP_INDEPENDENT);
}

static void
mavb_dma_output(struct mavb_softc *sc)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	u_int64_t write_ptr;
	caddr_t src, dst, end;
	int count;

	write_ptr = bus_space_read_8(st, sh, MAVB_CHANNEL2_WRITE_PTR);

	end = sc->play.hw_start + MAVB_CHAN_RING_SIZE;
	dst = sc->play.hw_start + write_ptr;
	src = sc->play.sw_cur;

	if (write_ptr % MAVB_CHAN_CHUNK_SIZE) {
		printf("%s: write_ptr=%lld\n", sc->sc_dev.dv_xname, write_ptr);
		return;
	}
	if ((src - sc->play.sw_start) % MAVB_CHAN_CHUNK_SIZE) {
		printf("%s: src=%ld\n", sc->sc_dev.dv_xname,
		    src - sc->play.sw_start);
		return;
	}

	count = MAVB_CHAN_INTR_SIZE / MAVB_CHAN_CHUNK_SIZE;
	while (--count >= 0) {
		memcpy(dst, src, MAVB_CHAN_CHUNK_SIZE);
		dst += MAVB_CHAN_CHUNK_SIZE;
		src += MAVB_CHAN_CHUNK_SIZE;
		if (dst >= end)
			dst = sc->play.hw_start;
		if (src >= sc->play.sw_end)
			src = sc->play.sw_start;
		if (!((src - sc->play.sw_start) % sc->play.blksize)) {
			if (sc->play.intr)
				sc->play.intr(sc->play.intrarg);
		}
	}
	write_ptr = dst - sc->play.hw_start;
	bus_space_write_8(st, sh, MAVB_CHANNEL2_WRITE_PTR, write_ptr);
	sc->play.sw_cur = src;
}

static void
mavb_dma_input(struct mavb_softc *sc)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	u_int64_t read_ptr;
	caddr_t src, dst, end;
	int count;

	read_ptr = bus_space_read_8(st, sh, MAVB_CHANNEL1_READ_PTR);

	end = sc->rec.hw_start + MAVB_CHAN_RING_SIZE;
	src = sc->rec.hw_start + read_ptr;
	dst = sc->rec.sw_cur;

	if (read_ptr % MAVB_CHAN_CHUNK_SIZE) {
		printf("%s: read_ptr=%lld\n", sc->sc_dev.dv_xname, read_ptr);
		return;
	}
	if ((dst - sc->rec.sw_start) % MAVB_CHAN_CHUNK_SIZE) {
		printf("%s: dst=%ld\n", sc->sc_dev.dv_xname,
		    dst - sc->rec.sw_start);
		return;
	}

	count = MAVB_CHAN_INTR_SIZE / MAVB_CHAN_CHUNK_SIZE;
	while (--count >= 0) {
		memcpy(dst, src, MAVB_CHAN_CHUNK_SIZE);
		dst += MAVB_CHAN_CHUNK_SIZE;
		src += MAVB_CHAN_CHUNK_SIZE;
		if (src >= end)
			src = sc->rec.hw_start;
		if (dst >= sc->rec.sw_end)
			dst = sc->rec.sw_start;
		if (!((dst - sc->rec.sw_start) % sc->rec.blksize)) {
			if (sc->rec.intr)
				sc->rec.intr(sc->rec.intrarg);
		}
	}
	read_ptr = src - sc->rec.hw_start;
	bus_space_write_8(st, sh, MAVB_CHANNEL1_READ_PTR, read_ptr);
	sc->rec.sw_cur = dst;
}

int
mavb_trigger_output(void *hdl, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, struct audio_params *param)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;

	DPRINTF(1, ("%s: mavb_trigger_output: start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", sc->sc_dev.dv_xname,
	    start, end, blksize, intr, intrarg));

	mtx_enter(&audio_lock);
	sc->play.blksize = blksize;
	sc->play.intr = intr;
	sc->play.intrarg = intrarg;

	sc->play.sw_start = sc->play.sw_cur = start;
	sc->play.sw_end = end;

	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL,
	    MAVB_CHANNEL_RESET);
	delay(1000);
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL, 0);

	/* Fill first 25% of buffer with silence. */
	bzero(sc->play.hw_start, MAVB_CHAN_CHUNK_SIZE);
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_WRITE_PTR,
	    MAVB_CHAN_CHUNK_SIZE);

	/* Fill next 50% of buffer with audio data. */
	mavb_dma_output(sc);

	/* The buffer is now 75% full.  Start DMA and get interrupts
	 * when the buffer is 25% full.  The interrupt handler fills
	 * in 50% of the buffer size, putting it back to 75% full.
	 */
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL,
	    MAVB_CHANNEL_DMA_ENABLE | MAVB_CHANNEL_INT_25);
	mtx_leave(&audio_lock);
	return (0);
}

int
mavb_trigger_input(void *hdl, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, struct audio_params *param)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;

	DPRINTF(1, ("%s: mavb_trigger_output: start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", sc->sc_dev.dv_xname,
	    start, end, blksize, intr, intrarg));

	mtx_enter(&audio_lock);
	sc->rec.blksize = blksize;
	sc->rec.intr = intr;
	sc->rec.intrarg = intrarg;

	sc->rec.sw_start = sc->rec.sw_cur = start;
	sc->rec.sw_end = end;

	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL1_CONTROL,
	    MAVB_CHANNEL_RESET);
	delay(1000);
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL1_CONTROL, 0);

	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL1_CONTROL,
	    MAVB_CHANNEL_DMA_ENABLE | MAVB_CHANNEL_INT_50);
	mtx_leave(&audio_lock);
	return (0);
}

static void
mavb_button_repeat(void *hdl)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;
	u_int64_t intmask, control;
	u_int16_t value, left, right;

	DPRINTF(1, ("%s: mavb_repeat called\n", sc->sc_dev.dv_xname));

#define  MAVB_CONTROL_VOLUME_BUTTONS \
    (MAVB_CONTROL_VOLUME_BUTTON_UP | MAVB_CONTROL_VOLUME_BUTTON_DOWN)

	control = bus_space_read_8(sc->sc_st, sc->sc_sh, MAVB_CONTROL);
	if (control & MAVB_CONTROL_VOLUME_BUTTONS) {
		value = ad1843_reg_read(sc, AD1843_DAC1_ANALOG_GAIN);
		left = (value & AD1843_LDA1G_MASK) >> AD1843_LDA1G_SHIFT;
		right = (value & AD1843_RDA1G_MASK) >> AD1843_RDA1G_SHIFT;
		if (control & MAVB_CONTROL_VOLUME_BUTTON_UP) {
			control &= ~MAVB_CONTROL_VOLUME_BUTTON_UP;
			if (left > 0)
				left--;		/* attenuation! */
			if (right > 0)
				right--;
		}
		if (control & MAVB_CONTROL_VOLUME_BUTTON_DOWN) {
			control &= ~MAVB_CONTROL_VOLUME_BUTTON_DOWN;
			if (left < 63)
				left++;
			if (right < 63)
				right++;
		}
		bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CONTROL, control);

		value &= ~(AD1843_LDA1G_MASK | AD1843_RDA1G_MASK);
		value |= (left << AD1843_LDA1G_SHIFT);
		value |= (right << AD1843_RDA1G_SHIFT);
		ad1843_reg_write(sc, AD1843_DAC1_ANALOG_GAIN, value);

		timeout_add_msec(&sc->sc_volume_button_to,
		    MAVB_VOLUME_BUTTON_REPEAT_DELN);
	} else {
		/* Enable volume button interrupts again.  */
		intmask = bus_space_read_8(sc->sc_st, sc->sc_isash,
		     MACE_ISA_INT_MASK);
		bus_space_write_8(sc->sc_st, sc->sc_isash, MACE_ISA_INT_MASK,
		     intmask | MACE_ISA_INT_AUDIO_SC);
	}
}

static int
mavb_intr(void *arg)
{
	struct mavb_softc *sc = arg;
	u_int64_t intstat, intmask;

	mtx_enter(&audio_lock);
	intstat = bus_space_read_8(sc->sc_st, sc->sc_isash, MACE_ISA_INT_STAT);
	DPRINTF(MAVB_DEBUG_INTR, ("%s: mavb_intr: intstat = 0x%lx\n",
            sc->sc_dev.dv_xname, intstat));

	if (intstat & MACE_ISA_INT_AUDIO_SC) {
		/* Disable volume button interrupts.  */
		intmask = bus_space_read_8(sc->sc_st, sc->sc_isash,
		     MACE_ISA_INT_MASK);
		bus_space_write_8(sc->sc_st, sc->sc_isash, MACE_ISA_INT_MASK,
		     intmask & ~MACE_ISA_INT_AUDIO_SC);

		timeout_add_msec(&sc->sc_volume_button_to,
		    MAVB_VOLUME_BUTTON_REPEAT_DEL1);
	}

	if (intstat & MACE_ISA_INT_AUDIO_DMA1)
		mavb_dma_input(sc);

	if (intstat & MACE_ISA_INT_AUDIO_DMA2)
		mavb_dma_output(sc);
	mtx_leave(&audio_lock);
	return 1;
}

int
mavb_match(struct device *parent, void *match, void *aux)
{
	struct macebus_attach_args *maa = aux;
	bus_space_handle_t ioh;
	u_int64_t control;

	if (bus_space_map(maa->maa_iot, maa->maa_baseaddr, MAVB_NREGS, 0,
	    &ioh) != 0)
		return (0);
	control = bus_space_read_8(maa->maa_iot, ioh, MAVB_CONTROL);
	bus_space_unmap(maa->maa_iot, ioh, MAVB_NREGS);

	return ((control & MAVB_CONTROL_CODEC_PRESENT) != 0);
}

void
mavb_attach(struct device *parent, struct device *self, void *aux)
{
	struct mavb_softc *sc = (void *)self;
	struct macebus_attach_args *maa = aux;
	bus_dma_segment_t seg;
	u_int16_t value;
	int rseg;

	sc->sc_st = maa->maa_iot;
	if (bus_space_map(sc->sc_st, maa->maa_baseaddr, MAVB_NREGS, 0,
	    &sc->sc_sh) != 0) {
		printf(": can't map i/o space\n");
		return;
	}

	/* XXX We need access to some of the MACE ISA registers.  */
	extern bus_space_handle_t mace_h;
	bus_space_subregion(sc->sc_st, mace_h, 0, MAVB_ISA_NREGS,
	    &sc->sc_isash);

	/* Set up DMA structures.  */
	sc->sc_dmat = maa->maa_dmat;
	if (bus_dmamap_create(sc->sc_dmat, MAVB_ISA_RING_SIZE, 1,
	    MAVB_ISA_RING_SIZE, 0, 0, &sc->sc_dmamap)) {
		printf(": can't create MACE ISA DMA map\n");
		return;
	}

	if (bus_dmamem_alloc(sc->sc_dmat, MAVB_ISA_RING_SIZE,
	    MACE_ISA_RING_ALIGN, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": can't allocate ring buffer\n");
		return;
	}

	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, MAVB_ISA_RING_SIZE,
	    &sc->sc_ring, BUS_DMA_COHERENT)) {
		printf(": can't map ring buffer\n");
		return;
	}

	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap, sc->sc_ring,
	    MAVB_ISA_RING_SIZE, NULL, BUS_DMA_NOWAIT)) {
		printf(": can't load MACE ISA DMA map\n");
		return;
	}

	sc->rec.hw_start = sc->sc_ring;
	sc->play.hw_start = sc->sc_ring + MAVB_CHAN_RING_SIZE;

	bus_space_write_8(sc->sc_st, sc->sc_isash, MACE_ISA_RING_BASE,
	    sc->sc_dmamap->dm_segs[0].ds_addr);

	/* Establish interrupt.  */
	macebus_intr_establish(maa->maa_intr, maa->maa_mace_intr,
	    IST_EDGE, IPL_AUDIO, mavb_intr, sc, sc->sc_dev.dv_xname);

	/* 2. Assert the RESET signal.  */
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CONTROL,
	    MAVB_CONTROL_RESET);
	delay(1);		/* at least 100 ns */

	/* 3. Deassert the RESET signal and enter a wait period to
              allow the AD1843 internal clocks and the external
              crystal oscillator to stabilize.  */
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CONTROL, 0);
	delay(800);		/* typically 400 us to 800 us */
	if (ad1843_reg_read(sc, AD1843_CODEC_STATUS) & AD1843_INIT) {
		printf(": codec not ready\n");
		return;
	}

	/* 4. Put the conversion sources into standby.  */
	value = ad1843_reg_read(sc, AD1843_FUNDAMENTAL_SETTINGS);
	ad1843_reg_write(sc, AD1843_FUNDAMENTAL_SETTINGS,
	    value & ~AD1843_PDNI);
	delay (500000);		/* approximately 474 ms */
	if (ad1843_reg_read(sc, AD1843_CODEC_STATUS) & AD1843_PDNO) {
		printf(": can't power up conversion resources\n");
		return;
	}

	/* 5. Power up the clock generators and enable clock output pins.  */
	value = ad1843_reg_read(sc, AD1843_FUNDAMENTAL_SETTINGS);
	ad1843_reg_write(sc, AD1843_FUNDAMENTAL_SETTINGS,
	    value | AD1843_C1EN | AD1843_C2EN);

	/* 6. Configure conversion resources while they are in standby.  */
	value = ad1843_reg_read(sc, AD1843_SERIAL_INTERFACE);
	ad1843_reg_write(sc, AD1843_SERIAL_INTERFACE, value | AD1843_ADTLK);
	value = ad1843_reg_read(sc, AD1843_CHANNEL_SAMPLE_RATE);
	ad1843_reg_write(sc, AD1843_CHANNEL_SAMPLE_RATE,
	    value | (2 << AD1843_DA1C_SHIFT) |
	    (1 << AD1843_ADRC_SHIFT) | (1 << AD1843_ADLC_SHIFT));

	/* 7. Enable conversion resources.  */
	value = ad1843_reg_read(sc, AD1843_CHANNEL_POWER_DOWN);
	ad1843_reg_write(sc, AD1843_CHANNEL_POWER_DOWN,
	    value | (AD1843_DA1EN | AD1843_ANAEN | AD1843_AAMEN |
	    AD1843_ADREN | AD1843_ADLEN));

	/* 8. Configure conversion resources while they are enabled.  */
	value = ad1843_reg_read(sc, AD1843_DAC1_ANALOG_GAIN);
	ad1843_reg_write(sc, AD1843_DAC1_ANALOG_GAIN,
            value & ~(AD1843_LDA1GM | AD1843_RDA1GM));
	value = ad1843_reg_read(sc, AD1843_DAC1_DIGITAL_GAIN);
	ad1843_reg_write(sc, AD1843_DAC1_DIGITAL_GAIN,
            value & ~(AD1843_LDA1AM | AD1843_RDA1AM));
	value = ad1843_reg_read(sc, AD1843_MISC_SETTINGS);
	ad1843_reg_write(sc, AD1843_MISC_SETTINGS,
            value & ~(AD1843_HPOM | AD1843_MNOM));

	value = ad1843_reg_read(sc, AD1843_CODEC_STATUS);
	printf(": AD1843 rev %d\n", (u_int)value & AD1843_REVISION_MASK);

	sc->play.rate = sc->rec.rate = 48000;
	sc->play.format = sc->rec.format = AD1843_PCM8;

	timeout_set(&sc->sc_volume_button_to, mavb_button_repeat, sc);

	audio_attach_mi(&mavb_sa_hw_if, sc, &sc->sc_dev);

	return;
}

u_int16_t
ad1843_reg_read(struct mavb_softc *sc, ad1843_addr_t addr)
{
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CODEC_CONTROL,
            (addr & MAVB_CODEC_ADDRESS_MASK) << MAVB_CODEC_ADDRESS_SHIFT |
	    MAVB_CODEC_READ);
	delay(200);
	return bus_space_read_8(sc->sc_st, sc->sc_sh, MAVB_CODEC_STATUS);
}

u_int16_t
ad1843_reg_write(struct mavb_softc *sc, ad1843_addr_t addr, u_int16_t value)
{
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CODEC_CONTROL,
	    (addr & MAVB_CODEC_ADDRESS_MASK) << MAVB_CODEC_ADDRESS_SHIFT |
	    (value & MAVB_CODEC_WORD_MASK) << MAVB_CODEC_WORD_SHIFT);
	delay(200);
	return bus_space_read_8(sc->sc_st, sc->sc_sh, MAVB_CODEC_STATUS);
}

void
ad1843_dump_regs(struct mavb_softc *sc)
{
	u_int16_t addr;

	for (addr = 0; addr < AD1843_NREGS; addr++)
		printf("%d: 0x%04x\n", addr, ad1843_reg_read(sc, addr));
}
