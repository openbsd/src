/* $OpenBSD: sf16fmr2.c,v 1.6 2002/10/15 15:00:11 mickey Exp $ */
/* $RuOBSD: sf16fmr2.c,v 1.12 2001/10/18 16:51:36 pva Exp $ */

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

/* SoundForte RadioLink SF16-FMR2 FM Radio Card device driver */

/*
 * Philips TEA5757H AM/FM Self Tuned Radio:
 *	http://www.semiconductors.philips.com/pip/TEA5757H
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/radioio.h>

#include <dev/isa/isavar.h>
#include <dev/radio_if.h>
#include <dev/ic/tea5757.h>
#include <dev/ic/pt2254a.h>

#define SF16FMR2_BASE_VALID(x)	(x == 0x384)
#define SF16FMR2_CAPABILITIES	RADIO_CAPS_DETECT_STEREO |		\
				RADIO_CAPS_DETECT_SIGNAL |		\
				RADIO_CAPS_SET_MONO |			\
				RADIO_CAPS_LOCK_SENSITIVITY |		\
				RADIO_CAPS_HW_AFC |			\
				RADIO_CAPS_HW_SEARCH

#define SF16FMR2_AMP		0
#define SF16FMR2_NOAMP		1

#define SF16FMR2_AMPLIFIER	(1 << 7)
#define SF16FMR2_SIGNAL		(1 << 3)
#define SF16FMR2_STEREO		(1 << 3)

#define SF16FMR2_MUTE		0x00
#define SF16FMR2_UNMUTE		0x04

#define SF16FMR2_DATA_ON	(1 << 0)
#define SF16FMR2_DATA_OFF	(0 << 0)

#define SF16FMR2_CLCK_ON	(1 << 1)
#define SF16FMR2_CLCK_OFF	(0 << 1)

#define SF16FMR2_WREN_ON	(0 << 2)  /* SF16-FMR2 has inverse WREN */
#define SF16FMR2_WREN_OFF	(1 << 2)

#define SF16FMR2_READ_CLOCK_LOW		\
	SF16FMR2_DATA_ON | SF16FMR2_CLCK_OFF | SF16FMR2_WREN_OFF

#define SF16FMR2_READ_CLOCK_HIGH	\
	SF16FMR2_DATA_ON | SF16FMR2_CLCK_ON | SF16FMR2_WREN_OFF

#define SF16FMR2_VOLU_STROBE_ON		(0 << 2)
#define SF16FMR2_VOLU_STROBE_OFF	(1 << 2)
#define SF16FMR2_VOLU_CLOCK_ON		(1 << 5)
#define SF16FMR2_VOLU_CLOCK_OFF		(0 << 5)
#define SF16FMR2_VOLU_DATA_ON		(1 << 6)
#define SF16FMR2_VOLU_DATA_OFF		(0 << 6)

int	sf2r_probe(struct device *, void *, void *);
void	sf2r_attach(struct device *, struct device * self, void *);

int	sf2r_get_info(void *, struct radio_info *);
int	sf2r_set_info(void *, struct radio_info *);
int	sf2r_search(void *, int);

/* define our interface to the higher level radio driver */
struct radio_hw_if sf2r_hw_if = {
	NULL, /* open */
	NULL, /* close */
	sf2r_get_info,
	sf2r_set_info,
	sf2r_search
};

struct sf2r_softc {
	struct device	sc_dev;

	int		type;
	u_int32_t	freq;
	u_int32_t	stereo;
	u_int32_t	lock;
	u_int8_t	vol;
	int	mute;

	struct tea5757_t	tea;
};

struct cfattach sf2r_ca = {
	sizeof(struct sf2r_softc), sf2r_probe, sf2r_attach
};

struct cfdriver sf2r_cd = {
	NULL, "sf2r", DV_DULL
};

void	sf2r_set_mute(struct sf2r_softc *);
void	sf2r_send_vol_bit(bus_space_tag_t, bus_space_handle_t, u_int32_t);
int	sf2r_find(bus_space_tag_t, bus_space_handle_t, int);

u_int32_t sf2r_read_register(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	sf2r_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf2r_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf2r_write_bit(bus_space_tag_t, bus_space_handle_t, bus_size_t, int);

int
sf2r_probe(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct cfdata *cf = match;
	int iosize = 1, iobase = ia->ia_iobase;

	if (!SF16FMR2_BASE_VALID(iobase)) {
		printf("sf2r: configured iobase 0x%x invalid\n", iobase);
		return (0);
	}

	if (bus_space_map(iot, iobase, iosize, 0, &ioh))
		return (0);

	if (!sf2r_find(iot, ioh, cf->cf_flags)) {
		bus_space_unmap(iot, ioh, iosize);
		return (0);
	}

	bus_space_unmap(iot, ioh, iosize);
	ia->ia_iosize = iosize;
	return (1);
}

void
sf2r_attach(struct device *parent, struct device *self, void *aux)
{
	struct sf2r_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;
	struct cfdata *cf = sc->sc_dev.dv_cfdata;
	int type;

	sc->tea.iot = ia->ia_iot;
	sc->mute = 0;
	sc->vol = 0;
	sc->freq = MIN_FM_FREQ;
	sc->stereo = TEA5757_STEREO;
	sc->lock = TEA5757_S030;

	/* remap I/O */
	if (bus_space_map(sc->tea.iot, ia->ia_iobase, ia->ia_iosize,
	    0, &sc->tea.ioh)) {
		printf(": bus_space_map() failed\n");
		return;
	}

	sc->tea.offset = 0;
	sc->tea.flags = cf->cf_flags;

	sc->tea.init = sf2r_init;
	sc->tea.rset = sf2r_rset;
	sc->tea.write_bit = sf2r_write_bit;
	sc->tea.read = sf2r_read_register;

	printf(": SoundForte RadioLink SF16-FMR2\n");
	tea5757_set_freq(&sc->tea, sc->stereo, sc->lock, sc->freq);
	sf2r_set_mute(sc);

	type = sf2r_read_register(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
	sc->type = (type >> 24) & (1 << 1)? SF16FMR2_AMP : SF16FMR2_NOAMP;

	radio_attach_mi(&sf2r_hw_if, sc, &sc->sc_dev);
}

/*
 * Mute/unmute the card
 */
void
sf2r_set_mute(struct sf2r_softc *sc)
{
	u_int8_t mute;
	u_int32_t reg, vol, i;

	if (sc->type == SF16FMR2_NOAMP) {
		mute = (sc->mute || !sc->vol)? SF16FMR2_MUTE : SF16FMR2_UNMUTE;
		bus_space_write_1(sc->tea.iot, sc->tea.ioh, 0, mute);
		DELAY(64);
		bus_space_write_1(sc->tea.iot, sc->tea.ioh, 0, mute);
	} else {
		mute = sc->mute? SF16FMR2_MUTE : SF16FMR2_UNMUTE;
		bus_space_write_1(sc->tea.iot, sc->tea.ioh, 0, mute);
		DELAY(64);
		bus_space_write_1(sc->tea.iot, sc->tea.ioh, 0, mute);

		vol = pt2254a_encode_volume(&sc->vol, 255);
		reg = pt2254a_compose_register(vol, vol,
		    USE_CHANNEL, USE_CHANNEL);

		bus_space_write_1(sc->tea.iot, sc->tea.ioh,
		    0, SF16FMR2_VOLU_STROBE_OFF);

		for (i = 0; i < PT2254A_REGISTER_LENGTH; i++)
			sf2r_send_vol_bit(sc->tea.iot, sc->tea.ioh,
			    reg & (1 << i));

		bus_space_write_1(sc->tea.iot, sc->tea.ioh,
		    0, SF16FMR2_VOLU_STROBE_ON);
		bus_space_write_1(sc->tea.iot, sc->tea.ioh,
		    0, SF16FMR2_VOLU_STROBE_OFF);
		bus_space_write_1(sc->tea.iot, sc->tea.ioh,
		    0, 0x10 | SF16FMR2_VOLU_STROBE_OFF);
	}
}

void
sf2r_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off, u_int32_t d)
{
	bus_space_write_1(iot, ioh, off, SF16FMR2_MUTE);
}

void
sf2r_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off, u_int32_t d)
{
	bus_space_write_1(iot, ioh, off, SF16FMR2_MUTE);
	bus_space_write_1(iot, ioh, off, SF16FMR2_UNMUTE);
}

int
sf2r_find(bus_space_tag_t iot, bus_space_handle_t ioh, int flags)
{
	struct sf2r_softc sc;
	u_int32_t freq;

	sc.tea.iot = iot;
	sc.tea.ioh = ioh;
	sc.tea.offset = 0;
	sc.tea.flags = flags;
	sc.tea.init = sf2r_init;
	sc.tea.rset = sf2r_rset;
	sc.tea.write_bit = sf2r_write_bit;
	sc.tea.read = sf2r_read_register;
	sc.lock = TEA5757_S030;
	sc.stereo = TEA5757_STEREO;

	if ((bus_space_read_1(iot, ioh, 0) & 0x70) == 0x30) {
		/*
		 * Let's try to write and read a frequency.
		 * If the written and read frequencies are
		 * the same then success.
		 */
		sc.freq = MIN_FM_FREQ;
		tea5757_set_freq(&sc.tea, sc.stereo, sc.lock, sc.freq);
		sf2r_set_mute(&sc);
		freq = sf2r_read_register(iot, ioh, sc.tea.offset);
		if (tea5757_decode_freq(freq, sc.tea.flags & TEA5757_TEA5759)
		    == sc.freq)
			return 1;
	}

	return 0;
}

void
sf2r_write_bit(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off, int bit)
{
	u_int8_t data;

	data = bit? SF16FMR2_DATA_ON : SF16FMR2_DATA_OFF;

	bus_space_write_1(iot, ioh, off,
	    SF16FMR2_WREN_ON | SF16FMR2_CLCK_OFF | data);
	bus_space_write_1(iot, ioh, off,
	    SF16FMR2_WREN_ON | SF16FMR2_CLCK_ON  | data);
	bus_space_write_1(iot, ioh, off,
	    SF16FMR2_WREN_ON | SF16FMR2_CLCK_OFF | data);
}

u_int32_t
sf2r_read_register(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t res = 0;
	u_int8_t i, state = 0;

	bus_space_write_1(iot, ioh, off, SF16FMR2_READ_CLOCK_LOW);
	DELAY(6);
	bus_space_write_1(iot, ioh, off, SF16FMR2_READ_CLOCK_HIGH);

	i = bus_space_read_1(iot, ioh, off);
	DELAY(6);
	/* Amplifier: 0 - not present, 1 - present */
	state = i & SF16FMR2_AMPLIFIER? (1 << 2) : (0 << 2);
	/* Signal: 0 - not tuned, 1 - tuned */
	state |= i & SF16FMR2_SIGNAL? (0 << 1) : (1 << 1);

	bus_space_write_1(iot, ioh, off, SF16FMR2_READ_CLOCK_LOW);
	i = bus_space_read_1(iot, ioh, off);
	/* Stereo: 0 - mono, 1 - stereo */
	state |= i & SF16FMR2_STEREO? (0 << 0) : (1 << 0);
	res = i & SF16FMR2_DATA_ON;

	i = 23;
	while ( i-- ) {
		DELAY(6);
		res <<= 1;
		bus_space_write_1(iot, ioh, off, SF16FMR2_READ_CLOCK_HIGH);
		DELAY(6);
		bus_space_write_1(iot, ioh, off, SF16FMR2_READ_CLOCK_LOW);
		res |= bus_space_read_1(iot, ioh, off) & SF16FMR2_DATA_ON;
	}

	return res | (state << 24);
}

int
sf2r_get_info(void *v, struct radio_info *ri)
{
	struct sf2r_softc *sc = v;
	u_int32_t buf;

	ri->mute = sc->mute;
	ri->volume = sc->vol? 255 : 0;
	ri->stereo = sc->stereo == TEA5757_STEREO? 1 : 0;
	ri->caps = SF16FMR2_CAPABILITIES;
	ri->rfreq = 0;
	ri->lock = tea5757_decode_lock(sc->lock);

	buf = sf2r_read_register(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
	ri->freq  = sc->freq = tea5757_decode_freq(buf,
	    sc->tea.flags & TEA5757_TEA5759);
	ri->info = 3 & (buf >> 24);

	return (0);
}

int
sf2r_set_info(void *v, struct radio_info *ri)
{
	struct sf2r_softc *sc = v;

	sc->mute = ri->mute? 1 : 0;
	sc->vol = ri->volume? 255 : 0;
	sc->stereo = ri->stereo? TEA5757_STEREO: TEA5757_MONO;
	sc->lock = tea5757_encode_lock(ri->lock);
	ri->freq = sc->freq = tea5757_set_freq(&sc->tea,
	    sc->lock, sc->stereo, ri->freq);
	sf2r_set_mute(sc);

	return (0);
}

int
sf2r_search(void *v, int f)
{
	struct sf2r_softc *sc = v;

	tea5757_search(&sc->tea, sc->lock, sc->stereo, f);
	sf2r_set_mute(sc);

	return (0);
}

void
sf2r_send_vol_bit(bus_space_tag_t iot, bus_space_handle_t ioh, u_int32_t d) {
	u_int8_t data;

	data = SF16FMR2_VOLU_STROBE_OFF;
	data |= d? SF16FMR2_VOLU_DATA_ON : SF16FMR2_VOLU_DATA_OFF;

	bus_space_write_1(iot, ioh, 0, data | SF16FMR2_VOLU_CLOCK_OFF);
	bus_space_write_1(iot, ioh, 0, data | SF16FMR2_VOLU_CLOCK_ON);
}
