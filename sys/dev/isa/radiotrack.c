/* $OpenBSD: radiotrack.c,v 1.1 2001/12/05 10:27:06 mickey Exp $ */
/* $RuOBSD: radiotrack.c,v 1.3 2001/10/18 16:51:36 pva Exp $ */

/*
 * Copyright (c) 2001 Maxim Tsyplakov <tm@oganer.net>,
 *                    Vladimir Popov <jumbo@narod.ru>
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
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/radioio.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/ic/lm700x.h>
#include <dev/radio_if.h>

#define RF_25K	25
#define RF_50K	50
#define RF_100K	100

#define MAX_VOL	5	/* XXX Find real value */
#define VOLUME_RATIO(x)	(255 * x / MAX_VOL)

#define RT_BASE_VALID(x)	\
		((x == 0x30C) || (x == 0x20C) || (x == 0x284) || (x == 0x384))

#define CARD_RADIOTRACK		0x01
#define CARD_SF16FMI		0x02
#define CARD_UNKNOWN		0xFF

#define RTRACK_CAPABILITIES	RADIO_CAPS_DETECT_STEREO | 		\
				RADIO_CAPS_DETECT_SIGNAL | 		\
				RADIO_CAPS_SET_MONO | 			\
				RADIO_CAPS_REFERENCE_FREQ

#define	RT_WREN_ON		(1 << 0)
#define	RT_WREN_OFF		(0 << 0)

#define RT_CLCK_ON		(1 << 1)
#define RT_CLCK_OFF		(0 << 1)

#define RT_DATA_ON		(1 << 2)
#define RT_DATA_OFF		(0 << 2)

#define RT_CARD_ON		(1 << 3)
#define RT_CARD_OFF		(0 << 3)

#define RT_SIGNAL_METER		(1 << 4)
#define RT_SIGNAL_METER_DELAY	150000

#define RT_VOLUME_DOWN		(1 << 6)
#define RT_VOLUME_UP		(2 << 6)
#define RT_VOLUME_STEADY	(3 << 6)
#define RT_VOLUME_DELAY		100000

int	rt_probe(struct device *, void *, void *);
void	rt_attach(struct device *, struct device * self, void *);
int	rt_get_info(void *, struct radio_info *);
int	rt_set_info(void *, struct radio_info *);

struct radio_hw_if rt_hw_if = {
	NULL,   /* open */
	NULL,   /* close */
	rt_get_info,
	rt_set_info,
	NULL
};

struct rt_softc {
	struct device	sc_dev;

	int		mute;
	u_int8_t	vol;
	u_int8_t	cardtype;
	u_int32_t	freq;
	u_int32_t	rf;
	u_int32_t	stereo;

	struct lm700x_t	lm;
};

struct cfattach rt_ca = {
	sizeof(struct rt_softc), rt_probe, rt_attach
};

struct cfdriver rt_cd = {
	NULL, "rt", DV_DULL
};

int	rt_find(bus_space_tag_t, bus_space_handle_t);
void	rt_set_mute(struct rt_softc *, int);
void	rt_set_freq(struct rt_softc *, u_int32_t);
u_int8_t	rt_state(bus_space_tag_t, bus_space_handle_t);

void	rt_lm700x_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	rt_lm700x_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);

u_int8_t	rt_conv_vol(u_int8_t);
u_int8_t	rt_unconv_vol(u_int8_t);

int
rt_probe(struct device *parent, void *self, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	int iosize = 1, iobase = ia->ia_iobase;

	if (!RT_BASE_VALID(iobase)) {
		printf("rt: configured iobase 0x%x invalid", iobase);
		return 0;
	}

	if (bus_space_map(iot, iobase, iosize, 0, &ioh))
		return 0;

	bus_space_unmap(iot, ioh, iosize);

	if (!rt_find(iot, ioh))
		return 0;

	ia->ia_iosize = iosize;
	return 1;
}

void
rt_attach(struct device *parent, struct device *self, void *aux)
{
	struct rt_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;

	sc->lm.iot = ia->ia_iot;
	sc->rf = LM700X_REF_050;
	sc->stereo = LM700X_STEREO;
	sc->mute = 0;
	sc->freq = MIN_FM_FREQ;
	sc->vol = 0;

	/* remap I/O */
	if (bus_space_map(sc->lm.iot, ia->ia_iobase, ia->ia_iosize,
			  0, &sc->lm.ioh))
		panic(": bus_space_map() of %s failed", sc->sc_dev.dv_xname);

	switch (sc->lm.iot) {
	case 0x20C:
		/* FALLTHROUGH */
	case 0x30C:
		sc->cardtype = CARD_RADIOTRACK;
		printf(": AIMS Lab Radiotrack or compatible");
		break;
	case 0x284:
		/* FALLTHROUGH */
	case 0x384:
		sc->cardtype = CARD_SF16FMI;
		printf(": SoundForte RadioX SF16-FMI");
		break;
	default:
		sc->cardtype = CARD_UNKNOWN;
		printf(": Unknown card");
		break;
	}

	/* Configure struct lm700x_t lm */
	sc->lm.offset = 0;
	sc->lm.wzcl = RT_WREN_ON | RT_CLCK_OFF | RT_DATA_OFF;
	sc->lm.wzch = RT_WREN_ON | RT_CLCK_ON  | RT_DATA_OFF;
	sc->lm.wocl = RT_WREN_ON | RT_CLCK_OFF | RT_DATA_ON;
	sc->lm.woch = RT_WREN_ON | RT_CLCK_ON  | RT_DATA_ON;
	sc->lm.initdata = 0;
	sc->lm.rsetdata = RT_DATA_ON | RT_CLCK_ON | RT_WREN_OFF;
	sc->lm.init = rt_lm700x_init;
	sc->lm.rset = rt_lm700x_rset;

	rt_set_freq(sc, sc->freq);

	radio_attach_mi(&rt_hw_if, sc, &sc->sc_dev);
}

/*
 * Mute the card
 */
void
rt_set_mute(struct rt_softc *sc, int vol)
{
	int val;

	if (sc->mute) {
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
				RT_VOLUME_DOWN | RT_CARD_ON);
		DELAY(MAX_VOL * RT_VOLUME_DELAY);
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
				RT_VOLUME_STEADY | RT_CARD_ON);
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0, RT_CARD_OFF);
		bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0, RT_CARD_OFF);
	} else {
		val = sc->vol - vol;
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

	sc->freq = nfreq;

	reg = lm700x_encode_freq(nfreq, sc->rf);
	reg |= sc->stereo | sc->rf | LM700X_DIVIDER_FM;

	lm700x_hardware_write(&sc->lm, reg, RT_VOLUME_STEADY);

	rt_set_mute(sc, sc->vol);
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

int
rt_find(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	struct rt_softc sc;
#if 0
	u_int i, scanres = 0;
#endif

	sc.lm.iot = iot;
	sc.lm.ioh = ioh;
	sc.lm.offset = 0;
	sc.lm.wzcl = RT_WREN_ON | RT_CLCK_OFF | RT_DATA_OFF;
	sc.lm.wzch = RT_WREN_ON | RT_CLCK_ON  | RT_DATA_OFF;
	sc.lm.wocl = RT_WREN_ON | RT_CLCK_OFF | RT_DATA_ON;
	sc.lm.woch = RT_WREN_ON | RT_CLCK_ON  | RT_DATA_ON;
	sc.lm.initdata = 0;
	sc.lm.rsetdata = RT_SIGNAL_METER;
	sc.lm.init = rt_lm700x_init;
	sc.lm.rset = rt_lm700x_rset;
	sc.rf = LM700X_REF_050;
	sc.mute = 0;
	sc.stereo = LM700X_STEREO;
	sc.vol = 0;

	/*
	 * Scan whole FM range. If there is a card it'll
	 * respond on some frequency.
	 */
	return 0;
#if 0
	for (i = MIN_FM_FREQ; !scanres && i < MAX_FM_FREQ; i += 10) {
		rt_set_freq(&sc, i);
		scanres += rt_state(iot, ioh);
	}

	return scanres;
#endif
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

int
rt_set_info(void *v, struct radio_info *ri)
{
	struct rt_softc *sc = v;

	sc->mute = ri->mute ? 1 : 0;
	sc->vol = rt_conv_vol(ri->volume);
	sc->stereo = ri->stereo ? LM700X_STEREO : LM700X_MONO;
	sc->rf = lm700x_encode_ref(ri->rfreq);

	rt_set_freq(sc, ri->freq);
	rt_set_mute(sc, sc->vol);

	return (0);
}

int
rt_get_info(void *v, struct radio_info *ri)
{
	struct rt_softc *sc = v;

	ri->mute = sc->mute;
	ri->volume = rt_unconv_vol(sc->vol);
	ri->stereo = sc->stereo == LM700X_STEREO ? 0 : 1;
	ri->caps = RTRACK_CAPABILITIES;
	ri->rfreq = lm700x_decode_ref(sc->rf);
	ri->info = 3 & rt_state(sc->lm.iot, sc->lm.ioh);
	ri->freq = sc->freq;

	/* UNSUPPORTED */
	ri->lock = 0;

	return (0);
}
