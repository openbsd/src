/*	$OpenBSD: zaurus_audio.c,v 1.9 2007/11/05 00:17:28 jakemsr Exp $	*/

/*
 * Copyright (c) 2005 Christopher Pascoe <pascoe@openbsd.org>
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
 * TODO:
 *	- powerhooks (currently only works until first suspend)
 *	- record support
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/audioio.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_i2c.h>
#include <arm/xscale/pxa2x0_i2s.h>
#include <arm/xscale/pxa2x0_dmac.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <zaurus/dev/zaurus_scoopvar.h>
#include <dev/i2c/wm8750reg.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#define WM8750_ADDRESS  0x1B
#define SPKR_VOLUME	112

#define wm8750_write(sc, reg, val)	pxa2x0_i2c_write_2(&sc->sc_i2c, \
    WM8750_ADDRESS, (((reg) << 9) | ((val) & 0x1ff)))

int	zaudio_match(struct device *, void *, void *);
void	zaudio_attach(struct device *, struct device *, void *);
int	zaudio_detach(struct device *, int);
void	zaudio_power(int, void *);

#define ZAUDIO_OP_SPKR	0
#define ZAUDIO_OP_HP	1

#define ZAUDIO_JACK_STATE_OUT	0
#define ZAUDIO_JACK_STATE_IN	1
#define ZAUDIO_JACK_STATE_INS	2
#define ZAUDIO_JACK_STATE_REM	3

/* GPIO pins */
#define GPIO_HP_IN_C3000	116

struct zaudio_volume {
	u_int8_t		left;
	u_int8_t		right;
};

struct zaudio_softc {
	struct device		sc_dev;

	/* i2s device softc */
	/* NB: pxa2x0_i2s requires this to be the second struct member */
	struct pxa2x0_i2s_softc	sc_i2s;

	/* i2c device softc */
	struct pxa2x0_i2c_softc	sc_i2c;

	void			*sc_powerhook;
	int			sc_playing;

	struct zaudio_volume	sc_volume[2];
	char			sc_unmute[2];

	int			sc_state;
	int			sc_icount;
	struct timeout		sc_to; 
};

struct cfattach zaudio_ca = {
	sizeof(struct zaudio_softc), zaudio_match, zaudio_attach,
	zaudio_detach
};

struct cfdriver zaudio_cd = {
	NULL, "zaudio", DV_DULL
};

struct audio_device wm8750_device = {
	"WM8750",
	"1.0",
	"wm"
};

void zaudio_init(struct zaudio_softc *);
int zaudio_jack_intr(void *);
void zaudio_jack(void *);
void zaudio_standby(struct zaudio_softc *);
void zaudio_update_volume(struct zaudio_softc *, int);
void zaudio_update_mutes(struct zaudio_softc *);
void zaudio_play_setup(struct zaudio_softc *);
int zaudio_open(void *, int);
void zaudio_close(void *);
int zaudio_query_encoding(void *, struct audio_encoding *);
int zaudio_set_params(void *, int, int, struct audio_params *,
    struct audio_params *);
int zaudio_halt_output(void *);
int zaudio_halt_input(void *);
int zaudio_getdev(void *, struct audio_device *);
int zaudio_set_port(void *, struct mixer_ctrl *);
int zaudio_get_port(void *, struct mixer_ctrl *);
int zaudio_query_devinfo(void *, struct mixer_devinfo *);
int zaudio_get_props(void *);
int zaudio_start_output(void *, void *, int, void (*)(void *), void *);
int zaudio_start_input(void *, void *, int, void (*)(void *), void *);

struct audio_hw_if wm8750_hw_if = {
	zaudio_open,
	zaudio_close,
	NULL /* zaudio_drain */,
	zaudio_query_encoding,
	zaudio_set_params,
	pxa2x0_i2s_round_blocksize,
	NULL /* zaudio_commit_settings */,
	NULL /* zaudio_init_output */,
	NULL /* zaudio_init_input */,
	zaudio_start_output,
	zaudio_start_input,
	zaudio_halt_output,
	zaudio_halt_input,
	NULL /* zaudio_speaker_ctl */,
	zaudio_getdev,
	NULL /* zaudio_setfd */,
	zaudio_set_port,
	zaudio_get_port,
	zaudio_query_devinfo,
	pxa2x0_i2s_allocm,
	pxa2x0_i2s_freem,
	pxa2x0_i2s_round_buffersize,
	pxa2x0_i2s_mappage,
	zaudio_get_props,
	NULL /* zaudio_trigger_output */,
	NULL /* zaudio_trigger_input */
};

static const unsigned short playback_registers[][2] = {
	/* Unmute DAC */
	{ ADCDACCTL_REG, 0x000 },

	/* 16 bit audio words */
	{ AUDINT_REG, AUDINT_SET_FORMAT(2) },

	/* Enable thermal protection, power */
	{ ADCTL1_REG, ADCTL1_TSDEN | ADCTL1_SET_VSEL(3) },

	/* Enable speaker driver, DAC oversampling */
	{ ADCTL2_REG, ADCTL2_ROUT2INV | ADCTL2_DACOSR },

	/* Set DAC voltage references */
	{ PWRMGMT1_REG, PWRMGMT1_SET_VMIDSEL(1) | PWRMGMT1_VREF },

	/* Direct DACs to output mixers */
	{ LOUTMIX1_REG, LOUTMIX1_LD2LO },
	{ ROUTMIX2_REG, ROUTMIX2_RD2RO },

	/* End of list */
	{ 0xffff, 0xffff }
};

int
zaudio_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
zaudio_attach(struct device *parent, struct device *self, void *aux)
{
	struct zaudio_softc		*sc = (struct zaudio_softc *)self;
	struct pxaip_attach_args	*pxa = aux;
	int err;

	sc->sc_powerhook = powerhook_establish(zaudio_power, sc);
	if (sc->sc_powerhook == NULL) {
		printf(": unable to establish powerhook\n");
		return;
	}

	sc->sc_i2s.sc_iot = pxa->pxa_iot;
	sc->sc_i2s.sc_dmat = pxa->pxa_dmat;
	sc->sc_i2s.sc_size = PXA2X0_I2S_SIZE;
	if (pxa2x0_i2s_attach_sub(&sc->sc_i2s)) {
		printf(": unable to attach I2S\n");
		goto fail_i2s;
	}

	sc->sc_i2c.sc_iot = pxa->pxa_iot;
	sc->sc_i2c.sc_size = PXA2X0_I2C_SIZE;
	if (pxa2x0_i2c_attach_sub(&sc->sc_i2c)) {
		printf(": unable to attach I2C\n");
		goto fail_i2c;
	}

	/* Check for an I2C response from the wm8750 */
	pxa2x0_i2c_open(&sc->sc_i2c);
	err = wm8750_write(sc, RESET_REG, 0);
	pxa2x0_i2c_close(&sc->sc_i2c);

	if (err) {
		printf(": codec failed to respond\n");
		goto fail_probe;
	}
	delay(100);

	/* Speaker on, headphones off by default. */
	sc->sc_volume[ZAUDIO_OP_SPKR].left = 240;
	sc->sc_unmute[ZAUDIO_OP_SPKR] = 1;
	sc->sc_volume[ZAUDIO_OP_HP].left = 180;
	sc->sc_volume[ZAUDIO_OP_HP].right = 180;
	sc->sc_unmute[ZAUDIO_OP_HP] = 0;

	/* Configure headphone jack state change handling. */
	timeout_set(&sc->sc_to, zaudio_jack, sc);
	pxa2x0_gpio_set_function(GPIO_HP_IN_C3000, GPIO_IN);
	(void)pxa2x0_gpio_intr_establish(GPIO_HP_IN_C3000,
	    IST_EDGE_BOTH, IPL_BIO, zaudio_jack_intr, sc, "hpjk");

	zaudio_init(sc);

	printf(": I2C, I2S, WM8750 Audio\n");

	audio_attach_mi(&wm8750_hw_if, sc, &sc->sc_dev);

	return;

fail_probe:
	pxa2x0_i2c_detach_sub(&sc->sc_i2c);
fail_i2c:
	pxa2x0_i2s_detach_sub(&sc->sc_i2s);
fail_i2s:
	powerhook_disestablish(sc->sc_powerhook);
}

int
zaudio_detach(struct device *self, int flags)
{
	struct zaudio_softc *sc = (struct zaudio_softc *)self;

	if (sc->sc_powerhook != NULL) {
		powerhook_disestablish(sc->sc_powerhook);
		sc->sc_powerhook = NULL;
	}

	pxa2x0_i2c_detach_sub(&sc->sc_i2c);
	pxa2x0_i2s_detach_sub(&sc->sc_i2s);

	return (0);
}

void
zaudio_power(int why, void *arg)
{
	struct zaudio_softc *sc = arg;

	switch (why) {
	case PWR_STANDBY:
	case PWR_SUSPEND:
		timeout_del(&sc->sc_to);
		zaudio_standby(sc);
		break;

	case PWR_RESUME:
		pxa2x0_i2s_init(&sc->sc_i2s);
		pxa2x0_i2c_init(&sc->sc_i2c);
		zaudio_init(sc);
		break;
	}
}

void
zaudio_init(struct zaudio_softc *sc)
{
	pxa2x0_i2c_open(&sc->sc_i2c);

	/* Reset the codec */
	wm8750_write(sc, RESET_REG, 0);
	delay(100);

	/* Switch to standby power only */
	wm8750_write(sc, PWRMGMT1_REG, PWRMGMT1_SET_VMIDSEL(2));
	wm8750_write(sc, PWRMGMT2_REG, 0);

	/* Configure digital interface for I2S */
	wm8750_write(sc, AUDINT_REG, AUDINT_SET_FORMAT(2));

	/* Initialise volume levels */
	zaudio_update_volume(sc, ZAUDIO_OP_SPKR);
	zaudio_update_volume(sc, ZAUDIO_OP_HP);
	scoop_set_headphone(0);

	pxa2x0_i2c_close(&sc->sc_i2c);

	/* Assume that the jack state has changed. */ 
	zaudio_jack(sc);

}

int
zaudio_jack_intr(void *v)
{
	struct zaudio_softc *sc = v;

	if (!timeout_triggered(&sc->sc_to))
		zaudio_jack(sc);
	
	return (1);
}

void
zaudio_jack(void *v)
{
	struct zaudio_softc *sc = v;

	switch (sc->sc_state) {
	case ZAUDIO_JACK_STATE_OUT:
		if (pxa2x0_gpio_get_bit(GPIO_HP_IN_C3000)) {
			sc->sc_state = ZAUDIO_JACK_STATE_INS;
			sc->sc_icount = 0;
		}
		break;
	case ZAUDIO_JACK_STATE_INS:
		if (sc->sc_icount++ > 2) {
			if (pxa2x0_gpio_get_bit(GPIO_HP_IN_C3000)) {
				sc->sc_state = ZAUDIO_JACK_STATE_IN;
				sc->sc_unmute[ZAUDIO_OP_SPKR] = 0;
				sc->sc_unmute[ZAUDIO_OP_HP] = 1;
				goto update_mutes;
			} else 
				sc->sc_state = ZAUDIO_JACK_STATE_OUT;
		}
		break;
	case ZAUDIO_JACK_STATE_IN:
		if (!pxa2x0_gpio_get_bit(GPIO_HP_IN_C3000)) {
			sc->sc_state = ZAUDIO_JACK_STATE_REM;
			sc->sc_icount = 0;
		}
		break;
	case ZAUDIO_JACK_STATE_REM: 
		if (sc->sc_icount++ > 2) {
			if (!pxa2x0_gpio_get_bit(GPIO_HP_IN_C3000)) {
				sc->sc_state = ZAUDIO_JACK_STATE_OUT;
				sc->sc_unmute[ZAUDIO_OP_SPKR] = 1;
				sc->sc_unmute[ZAUDIO_OP_HP] = 0;
				goto update_mutes;
			} else
				sc->sc_state = ZAUDIO_JACK_STATE_IN;
		}
		break;
	}
	
	timeout_add(&sc->sc_to, hz/4);
	return;

update_mutes:
	timeout_del(&sc->sc_to);

	if (sc->sc_playing) {
		pxa2x0_i2c_open(&sc->sc_i2c);
		zaudio_update_mutes(sc);
		pxa2x0_i2c_close(&sc->sc_i2c);
	}
}

void
zaudio_standby(struct zaudio_softc *sc)
{
	pxa2x0_i2c_open(&sc->sc_i2c);

	/* Switch codec to standby power only */
	wm8750_write(sc, PWRMGMT1_REG, PWRMGMT1_SET_VMIDSEL(2));
	wm8750_write(sc, PWRMGMT2_REG, 0);

	scoop_set_headphone(0);

	pxa2x0_i2c_close(&sc->sc_i2c);
}

void
zaudio_update_volume(struct zaudio_softc *sc, int output)
{
	switch(output) {
	case ZAUDIO_OP_SPKR:
		wm8750_write(sc, LOUT2VOL_REG, LOUT2VOL_LO2VU | LOUT2VOL_LO2ZC |
		    LOUT2VOL_SET_LOUT2VOL(sc->sc_volume[ZAUDIO_OP_SPKR
		    ].left >> 1));
		wm8750_write(sc, ROUT2VOL_REG, ROUT2VOL_RO2VU | ROUT2VOL_RO2ZC |
		    ROUT2VOL_SET_ROUT2VOL(sc->sc_volume[ZAUDIO_OP_SPKR
		    ].left >> 1));
		break;
	case ZAUDIO_OP_HP:
		wm8750_write(sc, LOUT1VOL_REG, LOUT1VOL_LO1VU | LOUT1VOL_LO1ZC |
		    LOUT1VOL_SET_LOUT1VOL(sc->sc_volume[ZAUDIO_OP_HP
		    ].left >> 1));
		wm8750_write(sc, ROUT1VOL_REG, ROUT1VOL_RO1VU | ROUT1VOL_RO1ZC |
		    ROUT1VOL_SET_ROUT1VOL(sc->sc_volume[ZAUDIO_OP_HP
		    ].right >> 1));
		break;
	}
}

void
zaudio_update_mutes(struct zaudio_softc *sc)
{
	unsigned short val;

	val = PWRMGMT2_DACL | PWRMGMT2_DACR;

	if (sc->sc_unmute[ZAUDIO_OP_SPKR])
		val |= PWRMGMT2_LOUT2 | PWRMGMT2_ROUT2;

	if (sc->sc_unmute[ZAUDIO_OP_HP])
		val |= PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1;

	wm8750_write(sc, PWRMGMT2_REG, val);

	scoop_set_headphone(sc->sc_unmute[ZAUDIO_OP_HP]);
}

void
zaudio_play_setup(struct zaudio_softc *sc)
{
	int i = 0;

	pxa2x0_i2c_open(&sc->sc_i2c);

	/* Program the codec with playback settings */
	while (playback_registers[i][0] != 0xffff) {
		wm8750_write(sc, playback_registers[i][0],
		    playback_registers[i][1]);
		i++;
	}
	zaudio_update_mutes(sc);

	pxa2x0_i2c_close(&sc->sc_i2c);
}

int
zaudio_open(void *hdl, int flags)
{
	struct zaudio_softc *sc = hdl;

	/* Power on the I2S bus and codec */
	pxa2x0_i2s_open(&sc->sc_i2s);

	return 0;
}

void
zaudio_close(void *hdl)
{
	struct zaudio_softc *sc = hdl;

	/* Power off the I2S bus and codec */
	pxa2x0_i2s_close(&sc->sc_i2s);
}

int
zaudio_query_encoding(void *hdl, struct audio_encoding *aep)
{
	switch (aep->index) {
	case 0:
		strlcpy(aep->name, AudioEulinear, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_ULINEAR;
		aep->precision = 8;
		aep->flags = 0;
		return (0);
	case 1:
		strlcpy(aep->name, AudioEmulaw, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_ULAW;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strlcpy(aep->name, AudioEalaw, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_ALAW;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strlcpy(aep->name, AudioEslinear, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_SLINEAR;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 4:
		strlcpy(aep->name, AudioEslinear_le, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_SLINEAR_LE;
		aep->precision = 16;
		aep->flags = 0;
		return (0);
	case 5:
		strlcpy(aep->name, AudioEulinear_le, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_ULINEAR_LE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strlcpy(aep->name, AudioEslinear_be, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_SLINEAR_BE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strlcpy(aep->name, AudioEulinear_be, sizeof(aep->name));
		aep->encoding = AUDIO_ENCODING_ULINEAR_BE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}

int
zaudio_set_params(void *hdl, int setmode, int usemode,
    struct audio_params *play, struct audio_params *rec)
{
	struct zaudio_softc *sc = hdl;

	if (setmode & AUMODE_PLAY) {
		play->factor = 1;
		play->sw_code = NULL;
		switch(play->encoding) {
		case AUDIO_ENCODING_ULAW:
			switch (play->channels) {
			case 1:
				play->factor = 4;
				play->sw_code = mulaw_to_slinear16_le_mts;
				break;
			case 2:
				play->factor = 2;
				play->sw_code = mulaw_to_slinear16_le;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code = linear8_to_linear16_le_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = linear8_to_linear16_le;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code = noswap_bytes_mts;
					break;
				case 2:
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code =
					    ulinear8_to_linear16_le_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = ulinear8_to_linear16_le;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code = change_sign16_le_mts;
					break;
				case 2:
					play->sw_code = change_sign16_le;
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ALAW:
			switch (play->channels) {
			case 1:
				play->factor = 4;
				play->sw_code = alaw_to_slinear16_le_mts;
			case 2:
				play->factor = 2;
				play->sw_code = alaw_to_slinear16_le;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code =
					    linear8_to_linear16_le_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = linear8_to_linear16_le;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code = swap_bytes_mts;
					break;
				case 2:
					play->sw_code = swap_bytes;
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code =
					    ulinear8_to_linear16_le_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = ulinear8_to_linear16_le;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code =
					    swap_bytes_change_sign16_le_mts;
					break;
				case 2:
					play->sw_code =
					    swap_bytes_change_sign16_le;
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}

		pxa2x0_i2s_setspeed(&sc->sc_i2s, &play->sample_rate);
	}

#if RECORD_XXX_NOT_YET
	if (setmode & AUMODE_RECORD) {
		rec->factor = 1;
		rec->sw_code = NULL;
		switch(rec->encoding) {
		case AUDIO_ENCODING_ULAW:
			rec->sw_code = ulinear8_to_mulaw;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (rec->precision == 8)
				rec->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (rec->precision == 16)
				rec->sw_code = change_sign16_le;
			break;
		case AUDIO_ENCODING_ALAW:
			rec->sw_code = ulinear8_to_alaw;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (rec->precision == 16)
				rec->sw_code = swap_bytes;
			else
				rec->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (rec->precision == 16)
				rec->sw_code = change_sign16_swap_bytes_le;
			break;
		default:
			return (EINVAL);
		}

		pxa2x0_i2s_setspeed(sc, &rec->sample_rate);
	}
#endif

	return (0);
}

int
zaudio_halt_output(void *hdl)
{
	struct zaudio_softc *sc = hdl;

	/* XXX forcibly stop output DMA? */

	zaudio_standby(sc);
	sc->sc_playing = 0;

	return 0;
}

int
zaudio_halt_input(void *hdl)
{
	/* struct zaudio_softc *sc = hdl; */

	return 0;
}

int
zaudio_getdev(void *hdl, struct audio_device *ret)
{
	/* struct zaudio_softc *sc = hdl; */

	*ret = wm8750_device;
	return 0;
}

#define ZAUDIO_SPKR_LVL		0
#define ZAUDIO_SPKR_MUTE	1
#define ZAUDIO_HP_LVL		2
#define ZAUDIO_HP_MUTE		3
#define ZAUDIO_OUTPUT_CLASS	4

int
zaudio_set_port(void *hdl, struct mixer_ctrl *mc)
{
	struct zaudio_softc *sc = hdl;
	int error = EINVAL, s;

	s = splbio();
	pxa2x0_i2c_open(&sc->sc_i2c);

	switch (mc->dev) {
	case ZAUDIO_SPKR_LVL:
		if (mc->type != AUDIO_MIXER_VALUE)
			break;
		if (mc->un.value.num_channels == 1)
			sc->sc_volume[ZAUDIO_OP_SPKR].left =
			    mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		else
			break;
		zaudio_update_volume(sc, ZAUDIO_OP_SPKR);
		error = 0;
		break;
	case ZAUDIO_SPKR_MUTE:
		if (mc->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_unmute[ZAUDIO_OP_SPKR] = mc->un.ord ? 1 : 0;
		zaudio_update_mutes(sc);
		error = 0;
		break;
	case ZAUDIO_HP_LVL:
		if (mc->type != AUDIO_MIXER_VALUE)
			break;
		if (mc->un.value.num_channels == 1) {
			sc->sc_volume[ZAUDIO_OP_HP].left =
			    mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[ZAUDIO_OP_HP].right =
			    mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else if (mc->un.value.num_channels == 2) {
			sc->sc_volume[ZAUDIO_OP_HP].left =
			    mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[ZAUDIO_OP_HP].right =
			    mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		}
		else
			break;
		zaudio_update_volume(sc, ZAUDIO_OP_HP);
		error = 0;
		break;
	case ZAUDIO_HP_MUTE:
		if (mc->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_unmute[ZAUDIO_OP_HP] = mc->un.ord ? 1 : 0;
		zaudio_update_mutes(sc);
		error = 0;
		break;
	}

	pxa2x0_i2c_close(&sc->sc_i2c);
	splx(s);

	return error;
}

int
zaudio_get_port(void *hdl, struct mixer_ctrl *mc)
{
	struct zaudio_softc *sc = hdl;
	int error = EINVAL;

	switch (mc->dev) {
	case ZAUDIO_SPKR_LVL:
		if (mc->type != AUDIO_MIXER_VALUE)
			break;
		if (mc->un.value.num_channels == 1)
			mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_volume[ZAUDIO_OP_SPKR].left;
		else
			break;
		error = 0;
		break;
	case ZAUDIO_SPKR_MUTE:
		if (mc->type != AUDIO_MIXER_ENUM)
			break;
		mc->un.ord = sc->sc_unmute[ZAUDIO_OP_SPKR] ? 1 : 0;
		error = 0;
		break;
	case ZAUDIO_HP_LVL:
		if (mc->type != AUDIO_MIXER_VALUE)
			break;
		if (mc->un.value.num_channels == 1)
			mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_volume[ZAUDIO_OP_HP].left;
		else if (mc->un.value.num_channels == 2) {
			mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_volume[ZAUDIO_OP_HP].left;
			mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_volume[ZAUDIO_OP_HP].right;
		}
		else
			break;
		error = 0;
		break;
	case ZAUDIO_HP_MUTE:
		if (mc->type != AUDIO_MIXER_ENUM)
			break;
		mc->un.ord = sc->sc_unmute[ZAUDIO_OP_HP] ? 1 : 0;
		error = 0;
		break;
	}

	return error;
}

int
zaudio_query_devinfo(void *hdl, struct mixer_devinfo *di)
{
	/* struct zaudio_softc *sc = hdl; */

	switch (di->index) {
	case ZAUDIO_SPKR_LVL:
		di->type = AUDIO_MIXER_VALUE;
		di->mixer_class = ZAUDIO_OUTPUT_CLASS;
		di->prev = AUDIO_MIXER_LAST;
		di->next = ZAUDIO_SPKR_MUTE;
		strlcpy(di->label.name, AudioNspeaker,
		    sizeof(di->label.name));
		strlcpy(di->un.v.units.name, AudioNvolume,
		    sizeof(di->un.v.units.name));
		di->un.v.num_channels = 1;
		break;
	case ZAUDIO_SPKR_MUTE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = ZAUDIO_OUTPUT_CLASS;
		di->prev = ZAUDIO_SPKR_LVL;
		di->next = AUDIO_MIXER_LAST;
		goto mute;
	case ZAUDIO_HP_LVL:
		di->type = AUDIO_MIXER_VALUE;
		di->mixer_class = ZAUDIO_OUTPUT_CLASS;
		di->prev = AUDIO_MIXER_LAST;
		di->next = ZAUDIO_HP_MUTE;
		strlcpy(di->label.name, AudioNheadphone,
		    sizeof(di->label.name));
		di->un.v.num_channels = 1;
		strlcpy(di->un.v.units.name, AudioNvolume,
		    sizeof(di->un.v.units.name));
		break;
	case ZAUDIO_HP_MUTE:
		di->type = AUDIO_MIXER_ENUM;
		di->mixer_class = ZAUDIO_OUTPUT_CLASS;
		di->prev = ZAUDIO_HP_LVL;
		di->next = AUDIO_MIXER_LAST;
mute:
		strlcpy(di->label.name, AudioNmute, sizeof(di->label.name));
		di->un.e.num_mem = 2;
		strlcpy(di->un.e.member[0].label.name, AudioNon,
		    sizeof(di->un.e.member[0].label.name));
		di->un.e.member[0].ord = 0;
		strlcpy(di->un.e.member[1].label.name, AudioNoff,
		    sizeof(di->un.e.member[1].label.name));
		di->un.e.member[1].ord = 1;
		break;
	case ZAUDIO_OUTPUT_CLASS:
		di->type = AUDIO_MIXER_CLASS;
		di->mixer_class = ZAUDIO_OUTPUT_CLASS;
		di->prev = AUDIO_MIXER_LAST;
		di->next = AUDIO_MIXER_LAST;
		strlcpy(di->label.name, AudioCoutputs,
		    sizeof(di->label.name));
		break;
	default:
		return ENXIO;
	}

	return 0;
}

int
zaudio_get_props(void *hdl)
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

int
zaudio_start_output(void *hdl, void *block, int bsize, void (*intr)(void *),
    void *intrarg)
{
	struct zaudio_softc *sc = hdl;
	int err;

	/* Power up codec if we are not already playing. */
	if (!sc->sc_playing) {
		sc->sc_playing = 1;
		zaudio_play_setup(sc);
	}

	/* Start DMA via I2S */
	err = pxa2x0_i2s_start_output(&sc->sc_i2s, block, bsize, intr, intrarg);
	if (err) {
		zaudio_standby(sc);
		sc->sc_playing = 0;
	}
	return err;
}

int
zaudio_start_input(void *hdl, void *block, int bsize, void (*intr)(void *),
    void *intrarg)
{
	return ENXIO;
}
