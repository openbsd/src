/* $OpenBSD: aztech.c,v 1.4 2002/01/23 20:30:38 mickey Exp $ */
/* $RuOBSD: aztech.c,v 1.11 2001/10/20 13:23:47 pva Exp $ */

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

/* Aztech/PackardBell FM Radio Card device driver */

/*
 * Sanyo LM7001J Direct PLL Frequency Synthesizer:
 *	??? See http://www.redsword.com/tjacobs/geeb/fmcard.htm
 *
 * Philips TEA5712T AM/FM Stereo DTS Radio:
 *	http://www.semiconductors.philips.com/pip/TEA5712
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

#define MAX_VOL	3
#define VOLUME_RATIO(x)	(255 * x / MAX_VOL)

#define AZ_BASE_VALID(x)	((x == 0x350) || (x == 0x358))
#define AZTECH_CAPABILITIES	RADIO_CAPS_DETECT_STEREO |		\
				RADIO_CAPS_DETECT_SIGNAL |		\
				RADIO_CAPS_SET_MONO |			\
				RADIO_CAPS_REFERENCE_FREQ


#define AZTECH_STEREO	(1 << 0)
#define AZTECH_SIGNAL	(1 << 1)

#define	AZ_WREN_ON	(1 << 1)
#define	AZ_WREN_OFF	(0 << 1)

#define AZ_CLCK_ON	(1 << 6)
#define AZ_CLCK_OFF	(0 << 6)

#define AZ_DATA_ON	(1 << 7)
#define AZ_DATA_OFF	(0 << 7)

int	az_probe(struct device *, void *, void *);
void	az_attach(struct device *, struct device * self, void *);

int	az_get_info(void *, struct radio_info *);
int	az_set_info(void *, struct radio_info *);

struct radio_hw_if az_hw_if = {
	NULL,	/* open */
	NULL,	/* close */
	az_get_info,
	az_set_info,
	NULL
};

struct az_softc {
	struct device	sc_dev;

	int		mute;
	u_int8_t	vol;
	u_int32_t	freq;
	u_int32_t	rf;
	u_int32_t	stereo;

	struct lm700x_t	lm;
};

struct cfattach az_ca = {
	sizeof(struct az_softc), az_probe, az_attach
};

struct cfdriver az_cd = {
	NULL, "az", DV_DULL
};

u_int	az_find(bus_space_tag_t, bus_space_handle_t);
void	az_set_mute(struct az_softc *);
void	az_set_freq(struct az_softc *, u_int32_t);
u_int8_t	az_state(bus_space_tag_t, bus_space_handle_t);

void	az_lm700x_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	az_lm700x_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);

u_int8_t	az_conv_vol(u_int8_t);
u_int8_t	az_unconv_vol(u_int8_t);

int
az_probe(struct device *parent, void *self, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	int iosize = 1, iobase = ia->ia_iobase;

	if (!AZ_BASE_VALID(iobase)) {
		printf("az: configured iobase 0x%x invalid\n", iobase);
		return (0);
	}

	if (bus_space_map(iot, iobase, iosize, 0, &ioh))
		return (0);

	if (!az_find(iot, ioh)) {
		bus_space_unmap(iot, ioh, iosize);
		return (0);
	}

	bus_space_unmap(iot, ioh, iosize);
	ia->ia_iosize = iosize;
	return (1);
}

void
az_attach(struct device *parent, struct device *self, void *aux)
{
	struct az_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;

	sc->lm.iot = ia->ia_iot;
	sc->rf = LM700X_REF_050;
	sc->stereo = LM700X_STEREO;
	sc->mute = 0;
	sc->freq = MIN_FM_FREQ;
	sc->vol = 0;

	/* remap I/O */
	if (bus_space_map(sc->lm.iot, ia->ia_iobase, ia->ia_iosize,
			  0, &sc->lm.ioh)) {
		printf(": bus_space_map() failed\n");
		return;
	}

	printf(": Aztech/PackardBell\n");

	/* Configure struct lm700x_t lm */
	sc->lm.offset = 0;
	sc->lm.wzcl = AZ_WREN_ON | AZ_CLCK_OFF | AZ_DATA_OFF;
	sc->lm.wzch = AZ_WREN_ON | AZ_CLCK_ON  | AZ_DATA_OFF;
	sc->lm.wocl = AZ_WREN_ON | AZ_CLCK_OFF | AZ_DATA_ON;
	sc->lm.woch = AZ_WREN_ON | AZ_CLCK_ON  | AZ_DATA_ON;
	sc->lm.initdata = 0;
	sc->lm.rsetdata = AZ_DATA_ON | AZ_CLCK_ON | AZ_WREN_OFF;
	sc->lm.init = az_lm700x_init;
	sc->lm.rset = az_lm700x_rset;

	az_set_freq(sc, sc->freq);

	radio_attach_mi(&az_hw_if, sc, &sc->sc_dev);
}

/*
 * Mute the card
 */
void
az_set_mute(struct az_softc *sc)
{
	bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
	    sc->mute ? 0 : sc->vol);
	DELAY(6);
	bus_space_write_1(sc->lm.iot, sc->lm.ioh, 0,
	    sc->mute ? 0 : sc->vol);
}

void
az_set_freq(struct az_softc *sc, u_int32_t nfreq)
{
	u_int8_t vol;
	u_int32_t reg;

	vol = sc->mute ? 0 : sc->vol;

	if (nfreq > MAX_FM_FREQ)
		nfreq = MAX_FM_FREQ;
	if (nfreq < MIN_FM_FREQ)
		nfreq = MIN_FM_FREQ;

	sc->freq = nfreq;

	reg = lm700x_encode_freq(nfreq, sc->rf);
	reg |= sc->stereo | sc->rf | LM700X_DIVIDER_FM;

	lm700x_hardware_write(&sc->lm, reg, vol);

	az_set_mute(sc);
}

/*
 * Return state of the card - tuned/not tuned, mono/stereo
 */
u_int8_t
az_state(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	u_int8_t info = 0, portdata;

	portdata = bus_space_read_1(iot, ioh, 0);

	info |= portdata & AZTECH_STEREO ? 0 : RADIO_INFO_STEREO;
	info |= portdata & AZTECH_SIGNAL ? 0 : RADIO_INFO_SIGNAL;

	return info;
}

/*
 * Convert volume to hardware representation.
 * The card uses bits 00000x0x to set volume.
 */
u_int8_t
az_conv_vol(u_int8_t vol)
{
	if (vol < VOLUME_RATIO(1))
		return 0;
	else if (vol >= VOLUME_RATIO(1) && vol < VOLUME_RATIO(2))
		return 1;
	else if (vol >= VOLUME_RATIO(2) && vol < VOLUME_RATIO(3))
		return 4;
	else
		return 5;
}

/*
 * Convert volume from hardware representation
 */
u_int8_t
az_unconv_vol(u_int8_t vol)
{
	switch (vol) {
	case 0:
		return VOLUME_RATIO(0);
	case 1:
		return VOLUME_RATIO(1);
	case 4:
		return VOLUME_RATIO(2);
	}
	return VOLUME_RATIO(3);
}

u_int
az_find(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	struct az_softc sc;
	u_int i;

	sc.lm.iot = iot;
	sc.lm.ioh = ioh;
	sc.lm.offset = 0;
	sc.lm.wzcl = AZ_WREN_ON | AZ_CLCK_OFF | AZ_DATA_OFF;
	sc.lm.wzch = AZ_WREN_ON | AZ_CLCK_ON  | AZ_DATA_OFF;
	sc.lm.wocl = AZ_WREN_ON | AZ_CLCK_OFF | AZ_DATA_ON;
	sc.lm.woch = AZ_WREN_ON | AZ_CLCK_ON  | AZ_DATA_ON;
	sc.lm.initdata = 0;
	sc.lm.rsetdata = AZ_DATA_ON | AZ_CLCK_ON | AZ_WREN_OFF;
	sc.lm.init = az_lm700x_init;
	sc.lm.rset = az_lm700x_rset;
	sc.rf = LM700X_REF_050;
	sc.mute = 0;
	sc.stereo = LM700X_STEREO;
	sc.vol = 0;

	/*
	 * Scan whole FM range. If there is a card it'll
	 * respond on some frequency.
	 */
	for (i = MIN_FM_FREQ; i < MAX_FM_FREQ; i += 10) {
		az_set_freq(&sc, i);
		if (az_state(iot, ioh))
			return 1;
	}

	return 0;
}

void
az_lm700x_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		u_int32_t data)
{
	/* Do nothing */
	return;
}

void
az_lm700x_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		u_int32_t data)
{
	bus_space_write_1(iot, ioh, off, data);
}

int
az_get_info(void *v, struct radio_info *ri)
{
	struct az_softc *sc = v;

	ri->mute = sc->mute;
	ri->volume = az_unconv_vol(sc->vol);
	ri->stereo = sc->stereo == LM700X_STEREO ? 1 : 0;
	ri->caps = AZTECH_CAPABILITIES;
	ri->rfreq = lm700x_decode_ref(sc->rf);
	ri->info = az_state(sc->lm.iot, sc->lm.ioh);
	ri->freq = sc->freq;

	/* UNSUPPORTED */
	ri->lock = 0;

	return (0);
}

int
az_set_info(void *v, struct radio_info *ri)
{
	struct az_softc *sc = v;

	sc->mute = ri->mute ? 1 : 0;
	sc->vol = az_conv_vol(ri->volume);
	sc->stereo = ri->stereo ? LM700X_STEREO : LM700X_MONO;
	sc->rf = lm700x_encode_ref(ri->rfreq);

	az_set_freq(sc, ri->freq);
	az_set_mute(sc);

	return (0);
}
