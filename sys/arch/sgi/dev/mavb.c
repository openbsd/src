/*	$OpenBSD: mavb.c,v 1.3 2005/01/02 19:25:41 kettenis Exp $	*/

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
#include <machine/autoconf.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <mips64/archtype.h>

#include <sgi/localbus/macebus.h>
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

/*
 * AD1843 Mixer.
 */

enum {
	AD1843_RECORD_CLASS,
	AD1843_ADC_SOURCE,	/* ADC Source Select */
	AD1843_ADC_GAIN,	/* ADC Input Gain */

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

struct mavb_softc {
	struct device sc_dev;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap;

	/* XXX We need access to some of the MACE ISA registers.  */
	bus_space_handle_t sc_isash;

#define MAVB_ISA_RING_SIZE		0x1000
	caddr_t sc_ring;

	caddr_t sc_start, sc_end;
	int sc_blksize;
	void (*sc_intr)(void *);
	void *sc_intrarg;

	caddr_t sc_get;
	int sc_count;

	u_long sc_play_rate;
	u_int sc_play_format;

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
	mavb_trigger_input
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
		/* 8-bit Unsigned Linear PCM.  */
		strlcpy(ae->name, AudioEulinear, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ULINEAR;
		ae->precision = 8;
		ae->flags = 0;
		break;
	case 1:
		/* 16-bit Signed Linear PCM.  */
		strlcpy(ae->name, AudioEslinear_be, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_SLINEAR_BE;
		ae->precision = 16;
		ae->flags = 0;
		break;
	case 2:
		/* 8-bit mu-Law Companded.  */
		strlcpy(ae->name, AudioEmulaw, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ULAW;
		ae->precision = 8;
		ae->flags = 0;
		break;
	case 3:
		/* 8-bit A-Law Companded.  */
		strlcpy(ae->name, AudioEalaw, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ALAW;
		ae->precision = 8;
		ae->flags = 0;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

/*
 * For some reason SGI has decided to standardize their sound hardware
 * interfaces on 24-bit PCM even though the AD1843 codec used in the
 * Moosehead A/V Board only supports 16-bit and 8-bit formats.
 * Therefore we must convert everything to 24-bit samples only to have
 * the MACE hardware convert them back into 16-bit samples again.  To
 * complicate matters further, the 24-bit samples are embedded 32-bit
 * integers.  The 8-bit and 16-bit samples are first converted into
 * 24-bit samples by padding them to the right with zeroes.  Then they
 * are sign-extended into 32-bit integers.  This conversion is
 * conveniently done through the software encoding layer of the high
 * level audio driver by using the functions below.  Conversion of
 * mu-law and A-law formats is done by the hardware.
 */

static void
linear16_to_linear24_be(void *hdl, u_char *p, int cc)
{
	u_char *q = p;

	p += cc;
	q += cc * 2;
	while ((cc -= 2) >= 0) {
		q -= 4;
		q[3] = 0;
		q[2] = *--p;
		q[1] = *--p;
		q[0] = (*p & 0x80) ? 0xff : 0;
	}
}

static void
linear24_to_linear16_be(void *hdl, u_char *p, int cc)
{
	u_char *q = p;

	while ((cc -= 4) >= 0) {
		*q++ = p[1];
		*q++ = p[2];
		p += 4;
	}
}

static void
ulinear8_to_ulinear24_be(void *hdl, u_char *p, int cc)
{
	u_char *q = p;

	p += cc;
	q += cc * 4;
	while (--cc >= 0) {
		q -= 4;
		q[3] = 0;
		q[2] = 0;
		q[1] = *--p;
		q[0] = (*p & 0x80) ? 0xff : 0;
	}
}

static void
ulinear24_to_ulinear8_be(void *hdl, u_char *p, int cc)
{
	u_char *q = p;

	while ((cc -= 4) >= 0) {
		*q++ = p[1];
		p += 4;
	}
}

static void
linear16_to_linear24_be_mts(void *hdl, u_char *p, int cc)
{
	u_char *q = p;

	p += cc;
	q += cc * 4;
	while ((cc -= 2) >= 0) {
		q -= 8;
		q[3] = q[7] = 0;
		q[2] = q[6] = *--p;
		q[1] = q[5] = *--p;
		q[0] = q[4] = (*p & 0x80) ? 0xff : 0;
	}
}

static void
ulinear8_to_ulinear24_be_mts(void *hdl, u_char *p, int cc)
{
	u_char *q = p;

	p += cc;
	q += cc * 8;
	while (--cc >= 0) {
		q -= 8;
		q[3] = q[7] = 0;
		q[2] = q[6] = 0;
		q[1] = q[5] = *--p;
		q[0] = q[4] = (*p & 0x80) ? 0xff : 0;
	}
}

static int
mavb_set_play_rate(struct mavb_softc *sc, u_long sample_rate)
{
	if (sample_rate < 4000 || sample_rate > 48000)
		return (EINVAL);

	if (sc->sc_play_rate != sample_rate) {
		ad1843_reg_write(sc, AD1843_CLOCK2_SAMPLE_RATE, sample_rate);
		sc->sc_play_rate = sample_rate;
	}
	return (0);
}

static int
mavb_set_play_format(struct mavb_softc *sc, u_int encoding)
{
	u_int16_t value;
	u_int format;

	switch(encoding) {
	case AUDIO_ENCODING_ULINEAR_BE:
		format = AD1843_PCM8;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		format = AD1843_PCM16;
		break;
	case AUDIO_ENCODING_ULAW:
		format = AD1843_ULAW;
		break;
	case AUDIO_ENCODING_ALAW:
		format = AD1843_ALAW;
		break;
	default:
		return (EINVAL);
	}

	if (sc->sc_play_format != format) {
		value = ad1843_reg_read(sc, AD1843_SERIAL_INTERFACE);
		value &= ~AD1843_DA1F_MASK;
		value |= (format << AD1843_DA1F_SHIFT);
		ad1843_reg_write(sc, AD1843_SERIAL_INTERFACE, value);
		sc->sc_play_format = format;
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
		switch (play->encoding) {
		case AUDIO_ENCODING_ULAW:
		case AUDIO_ENCODING_ALAW:
		case AUDIO_ENCODING_ULINEAR_BE:
			if (play->precision != 8)
				return (EINVAL);
			switch (play->channels) {
			case 1:
				play->factor = 8;
				play->sw_code = ulinear8_to_ulinear24_be_mts;
				break;
			case 2:
				play->factor = 4;
				play->sw_code = ulinear8_to_ulinear24_be;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (play->precision == 16) {
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code =
						linear16_to_linear24_be_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code =
						linear16_to_linear24_be;
					break;
				default:
					return (EINVAL);
				}
			} else {
				return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}

		error = mavb_set_play_rate(sc, play->sample_rate);
		if (error)
			return (error);

		error = mavb_set_play_format(sc, play->encoding);
		if (error)
			return (error);
	}

	if (setmode & AUMODE_RECORD) {
		switch (rec->encoding) {
		case AUDIO_ENCODING_ULAW:
		case AUDIO_ENCODING_ALAW:
		case AUDIO_ENCODING_ULINEAR_BE:
			if (rec->precision != 8)
				return (EINVAL);
			rec->factor = 4;
			rec->sw_code = ulinear24_to_ulinear8_be;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (rec->precision == 16) {
				rec->factor = 2;
				rec->sw_code = linear24_to_linear16_be;
			} else {
				return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}
	}

	return (0);
}

int
mavb_round_blocksize(void *hdl, int bs)
{
	/* Block size should be a multiple of 32.  */
	return bs & ~0x1f;
}

int
mavb_halt_output(void *hdl)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;

	DPRINTF(1, ("%s: mavb_halt_output called\n", sc->sc_dev.dv_xname));

	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL, 0);
	return (0);
}

int
mavb_halt_input(void *hdl)
{
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
		    (right << 2) | right;
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

	case AD1843_INPUT_CLASS:
		di->type = AUDIO_MIXER_CLASS;
		di->mixer_class = AD1843_INPUT_CLASS;
		strlcpy(di->label.name, AudioCinputs, sizeof di->label.name);
		break;

	case AD1843_DAC1_GAIN:
		di->type = AUDIO_MIXER_VALUE;
		di->mixer_class = AD1843_INPUT_CLASS;
		di->next = AD1843_DAC1_MUTE;
		strlcpy(di->label.name, AudioNmaster, sizeof di->label.name);
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
	u_int64_t depth;
	caddr_t src, dst;
	int count;

	write_ptr = bus_space_read_8(st, sh, MAVB_CHANNEL2_WRITE_PTR);
	depth = bus_space_read_8(st, sh, MAVB_CHANNEL2_DEPTH);

	dst = sc->sc_ring + write_ptr;
	src = sc->sc_get;

	count = (MAVB_ISA_RING_SIZE - depth - 32);
	while (--count >= 0) {
		*dst++ = *src++;
		if (dst >= sc->sc_ring + MAVB_ISA_RING_SIZE)
			dst = sc->sc_ring;
		if (src >= sc->sc_end)
			src = sc->sc_start;
		if (++sc->sc_count >= sc->sc_blksize) {
			if (sc->sc_intr)
				sc->sc_intr(sc->sc_intrarg);
			sc->sc_count = 0;
		}
	}

	write_ptr = dst - sc->sc_ring;
	bus_space_write_8(st, sh, MAVB_CHANNEL2_WRITE_PTR, write_ptr);
	sc->sc_get = src;
}

int
mavb_trigger_output(void *hdl, void *start, void *end, int blksize,
		    void (*intr)(void *), void *intrarg,
		    struct audio_params *param)
{
	struct mavb_softc *sc = (struct mavb_softc *)hdl;

	DPRINTF(1, ("%s: mavb_trigger_output: start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", sc->sc_dev.dv_xname,
	    start, end, blksize, intr, intrarg));

	sc->sc_blksize = blksize;
	sc->sc_intr = intr;
	sc->sc_intrarg = intrarg;

	sc->sc_start = sc->sc_get = start;
	sc->sc_end = end;

	sc->sc_count = 0;

	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL,
	    MAVB_CHANNEL_RESET);
	delay(1000);
	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL, 0);

	mavb_dma_output(sc);

	bus_space_write_8(sc->sc_st, sc->sc_sh, MAVB_CHANNEL2_CONTROL,
	    MAVB_CHANNEL_DMA_ENABLE | MAVB_CHANNEL_INT_50);
	return (0);
}

int
mavb_trigger_input(void *hdl, void *start, void *end, int blksize,
		   void (*intr)(void *), void *intrarg,
		   struct audio_params *param)
{
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

		timeout_add(&sc->sc_volume_button_to,
		    (hz * MAVB_VOLUME_BUTTON_REPEAT_DELN) / 1000);
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

	intstat = bus_space_read_8(sc->sc_st, sc->sc_isash, MACE_ISA_INT_STAT);
	DPRINTF(MAVB_DEBUG_INTR, ("%s: mavb_intr: intstat = 0x%lx\n",
            sc->sc_dev.dv_xname, intstat));

	if (intstat & MACE_ISA_INT_AUDIO_SC) {
		/* Disable volume button interrupts.  */
		intmask = bus_space_read_8(sc->sc_st, sc->sc_isash,
		     MACE_ISA_INT_MASK);
		bus_space_write_8(sc->sc_st, sc->sc_isash, MACE_ISA_INT_MASK,
		     intmask & ~MACE_ISA_INT_AUDIO_SC);

		timeout_add(&sc->sc_volume_button_to,
		    (hz * MAVB_VOLUME_BUTTON_REPEAT_DEL1) / 1000);
	}

	if (intstat & MACE_ISA_INT_AUDIO_DMA2)
		mavb_dma_output(sc);

	return 1;
}

int
mavb_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;

	if (ca->ca_sys != SGI_O2 || strcmp(ca->ca_name, mavb_cd.cd_name))
		return (0);

	return (1);
}

void
mavb_attach(struct device *parent, struct device *self, void *aux)
{
	struct mavb_softc *sc = (void *)self;
	struct confargs *ca = aux;
	bus_dma_segment_t seg;
	u_int64_t control;
	u_int16_t value;
	int rseg;

	sc->sc_st = ca->ca_iot;
	if (bus_space_map(sc->sc_st, ca->ca_baseaddr, MAVB_NREGS, 0,
	    &sc->sc_sh) != 0) {
		printf(": can't map i/o space\n");
		return;
	}

	/* XXX We need access to some of the MACE ISA registers.  */
	extern bus_space_handle_t mace_h;
	bus_space_subregion(sc->sc_st, mace_h, 0, MAVB_ISA_NREGS,
	    &sc->sc_isash);

	/* Set up DMA structures.  */
	sc->sc_dmat = ca->ca_dmat;
	if (bus_dmamap_create(sc->sc_dmat, 4 * MAVB_ISA_RING_SIZE, 1,
	    4 * MAVB_ISA_RING_SIZE, 0, 0, &sc->sc_dmamap)) {
		printf(": can't create MACE ISA DMA map\n");
		return;
	}

	if (bus_dmamem_alloc(sc->sc_dmat, 4 * MAVB_ISA_RING_SIZE,
	    sizeof (u_int64_t), 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": can't allocate ring buffer\n");
		return;
	}

	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, 4 * MAVB_ISA_RING_SIZE,
	    &sc->sc_ring, BUS_DMA_COHERENT)) {
		printf(": can't map ring buffer\n");
		return;
	}

	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap, sc->sc_ring,
	    4 * MAVB_ISA_RING_SIZE, NULL, BUS_DMA_NOWAIT)) {
		printf(": can't load MACE ISA DMA map\n");
		return;
	}

	sc->sc_ring += MAVB_ISA_RING_SIZE; /* XXX */

	bus_space_write_8(sc->sc_st, sc->sc_isash, MACE_ISA_RING_BASE,
	   sc->sc_dmamap->dm_segs[0].ds_addr);

	/* Establish interrupt.  */
	BUS_INTR_ESTABLISH(ca, NULL, ca->ca_intr, IST_EDGE, IPL_AUDIO,
	    mavb_intr, sc, sc->sc_dev.dv_xname);

	control = bus_space_read_8(sc->sc_st, sc->sc_sh, MAVB_CONTROL);
	if (!(control & MAVB_CONTROL_CODEC_PRESENT)) {
		printf(": no codec present\n");
		return;
	}

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
	ad1843_reg_write(sc, AD1843_FUNDAMENTAL_SETTINGS, value | AD1843_C2EN);

	/* 6. Configure conversion resources while they are in standby.  */
	value = ad1843_reg_read(sc, AD1843_CHANNEL_SAMPLE_RATE);
	ad1843_reg_write(sc, AD1843_CHANNEL_SAMPLE_RATE,
	     value | (2 << AD1843_DA1C_SHIFT));

	/* 7. Enable conversion resources.  */
	value = ad1843_reg_read(sc, AD1843_CHANNEL_POWER_DOWN);
	ad1843_reg_write(sc, AD1843_CHANNEL_POWER_DOWN,
	     value | (AD1843_DA1EN | AD1843_AAMEN));

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

	sc->sc_play_rate = 48000;
	sc->sc_play_format = AD1843_PCM8;

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
