/* $OpenBSD: radiotrack.c,v 1.4 2002/08/28 21:20:48 mickey Exp $ */
/* $RuOBSD: radiotrack.c,v 1.3 2001/10/18 16:51:36 pva Exp $ */

/*
 * Copyright (c) 2001, 2002 Maxim Tsyplakov <tm@oganer.net>,
 *			    Vladimir Popov <jumbo@narod.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* AIMS Lab Radiotrack FM Radio Card device driver */

/*
 * Sanyo LM7000 Direct PLL Frequency Synthesizer
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/radioio.h>

#include <machine/bus.h>

#include <dev/ic/lm700x.h>
#include <dev/isa/isavar.h>
#include <dev/isa/rtreg.h>
#include <dev/isa/rtvar.h>
#include <dev/radio_if.h>

void	rtattach(struct rt_softc *);
int	rt_get_info(void *, struct radio_info *);
int	rt_set_info(void *, struct radio_info *);

struct radio_hw_if rt_hw_if = {
	NULL,	/* open */
	NULL,	/* close */
	rt_get_info,
	rt_set_info,
	NULL
};

struct cfdriver rt_cd = {
	NULL, "rt", DV_DULL
};

void	rt_set_mute(struct rt_softc *, int);
void	rt_set_freq(struct rt_softc *, u_int32_t);
u_int8_t	rt_state(bus_space_tag_t, bus_space_handle_t);

void	sfi_lm700x_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	rt_lm700x_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	rt_lm700x_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);

u_int8_t	rt_conv_vol(u_int8_t);
u_int8_t	rt_unconv_vol(u_int8_t);

void
rtattach(struct rt_softc *sc) {
	sc->sc_freq = MIN_FM_FREQ;
	sc->sc_mute = 0;
	sc->sc_vol = 0;
	sc->sc_rf = LM700X_REF_050;
	sc->sc_stereo = LM700X_STEREO;

	sc->lm.wzcl = RT_WREN_ON | RT_CLCK_OFF | RT_DATA_OFF;
	sc->lm.wzch = RT_WREN_ON | RT_CLCK_ON  | RT_DATA_OFF;
	sc->lm.wocl = RT_WREN_ON | RT_CLCK_OFF | RT_DATA_ON;
	sc->lm.woch = RT_WREN_ON | RT_CLCK_ON  | RT_DATA_ON;

	switch (sc->sc_ct) {
	case CARD_RADIOTRACK:
		sc->lm.initdata = 0;
		sc->lm.rsetdata = RT_SIGNAL_METER;
		sc->lm.init = rt_lm700x_init;
		sc->lm.rset = rt_lm700x_rset;
		break;
	case CARD_SF16FMI:
		sc->lm.initdata = RT_CARD_OFF;
		sc->lm.rsetdata = RT_CARD_ON;
		sc->lm.init = sfi_lm700x_init;
		sc->lm.rset = sfi_lm700x_init;
		break;
	}

	rt_set_freq(sc, sc->sc_freq);
	rt_set_mute(sc, sc->sc_vol);

	radio_attach_mi(&rt_hw_if, sc, &sc->sc_dev);
}

int
rt_set_info(void *v, struct radio_info *ri)
{
	struct rt_softc *sc = v;

	sc->sc_mute = ri->mute ? 1 : 0;
	sc->sc_rf = lm700x_encode_ref(ri->rfreq);

	switch (sc->sc_ct) {
	case CARD_RADIOTRACK:
		sc->sc_vol = rt_conv_vol(ri->volume);
		break;
	case CARD_SF16FMI:
		sc->sc_vol = ri->volume ? 1 : 0;
		break;
	}
	/*
	 * Though SF16-FMI does not set stereo/mono
	 * it won't hurt to have this
	 */
	sc->sc_stereo = ri->stereo ? LM700X_STEREO : LM700X_MONO;

	rt_set_freq(sc, ri->freq);
	rt_set_mute(sc, sc->sc_vol);

	return (0);
}

int
rt_get_info(void *v, struct radio_info *ri)
{
	struct rt_softc *sc = v;

	switch (sc->sc_ct) {
	case CARD_RADIOTRACK:
		ri->caps = RTRACK_CAPABILITIES;
		ri->info = 3 & rt_state(sc->lm.iot, sc->lm.ioh);
		ri->volume = rt_unconv_vol(sc->sc_vol);
		break;
	case CARD_SF16FMI:
		ri->caps = SF16FMI_CAPABILITIES;
		ri->volume = sc->sc_vol ? 255 : 0;
		ri->info = 0; /* UNSUPPORTED */
		break;
	default:
		/* No such card */
		return (1);
	}

	ri->mute = sc->sc_mute;
	ri->stereo = sc->sc_stereo == LM700X_STEREO ? 0 : 1;
	ri->rfreq = lm700x_decode_ref(sc->sc_rf);
	ri->freq = sc->sc_freq;

	/* UNSUPPORTED */
	ri->lock = 0;

	return (0);
}

/*
 * Mute the card
 */
void
rt_set_mute(struct rt_softc *sc, int vol)
{
	int val;

	if (sc->sc_ct == CARD_UNKNOWN)
		return;

	if (sc->sc_ct == CARD_SF16FMI) {
		val = vol ? RT_CARD_ON : RT_CARD_OFF;
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
				sc->sc_mute ? RT_CARD_OFF : val);
		return;
	}

	/* CARD_RADIOTRACK */
	if (sc->sc_mute) {
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
				RT_VOLUME_DOWN | RT_CARD_ON);
		DELAY(MAX_VOL * RT_VOLUME_DELAY);
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
				RT_VOLUME_STEADY | RT_CARD_ON);
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0, RT_CARD_OFF);
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0, RT_CARD_OFF);
	} else {
		val = sc->sc_vol - vol;
		if (val < 0) {
			val *= -1;
			bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
					RT_VOLUME_DOWN | RT_CARD_ON);
		} else {
			bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
					RT_VOLUME_UP | RT_CARD_ON);
		}
		DELAY(val * RT_VOLUME_DELAY);
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
				RT_VOLUME_STEADY | RT_CARD_ON);
	}
}

void
rt_set_freq(struct rt_softc *sc, u_int32_t nfreq)
{
	u_int32_t reg;

	if (nfreq > MAX_FM_FREQ)
		nfreq = MAX_FM_FREQ;
	if (nfreq < MIN_FM_FREQ)
		nfreq = MIN_FM_FREQ;

	sc->sc_freq = nfreq;

	reg = lm700x_encode_freq(nfreq, sc->sc_rf);
	reg |= sc->sc_stereo | sc->sc_rf | LM700X_DIVIDER_FM;

	lm700x_hardware_write(&sc->lm, reg, RT_VOLUME_STEADY);

	rt_set_mute(sc, sc->sc_vol);
}

/*
 * Return state of the card - tuned/not tuned, mono/stereo
 */
u_int8_t
rt_state(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	u_int8_t ret;

	bus_space_write_1(iot, ioh, 0,
			RT_VOLUME_STEADY | RT_SIGNAL_METER | RT_CARD_ON);
	DELAY(RT_SIGNAL_METER_DELAY);
	ret = bus_space_read_1(iot, ioh, 0);

	switch (ret) {
	case 0xFD:
		ret = RADIO_INFO_SIGNAL | RADIO_INFO_STEREO;
		break;
	case 0xFF:
		ret = 0;
		break;
	default:
		ret = RADIO_INFO_SIGNAL;
		break;
	}

	return ret;
}

/*
 * Convert volume to hardware representation.
 */
u_int8_t
rt_conv_vol(u_int8_t vol)
{
	if (vol < VOLUME_RATIO(1))
		return 0;
	else if (vol >= VOLUME_RATIO(1) && vol < VOLUME_RATIO(2))
		return 1;
	else if (vol >= VOLUME_RATIO(2) && vol < VOLUME_RATIO(3))
		return 2;
	else if (vol >= VOLUME_RATIO(3) && vol < VOLUME_RATIO(4))
		return 3;
	else
		return 4;
}

/*
 * Convert volume from hardware representation
 */
u_int8_t
rt_unconv_vol(u_int8_t vol)
{
	return VOLUME_RATIO(vol);
}

void
sfi_lm700x_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		u_int32_t data)
{
	bus_space_write_1(iot, ioh, off, data);
}

void
rt_lm700x_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		u_int32_t data)
{
	/* Do nothing */
	return;
}

void
rt_lm700x_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		u_int32_t data)
{
	DELAY(1000);
	bus_space_write_1(iot, ioh, off, RT_CARD_OFF | data);
	DELAY(50000);
	bus_space_write_1(iot, ioh, off, RT_VOLUME_STEADY | RT_CARD_ON | data);
}
