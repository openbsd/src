/* $OpenBSD: radiotrack2.c,v 1.4 2011/06/20 01:09:25 matthew Exp $ */
/* $RuOBSD: radiotrack2.c,v 1.2 2001/10/18 16:51:36 pva Exp $ */

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

/* AIMS Lab Radiotrack II FM Radio Card device driver */

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

#define RTII_BASE_VALID(x)	((x == 0x20C) || (x == 0x30C))
#define RTII_CAPABILITIES	RADIO_CAPS_DETECT_STEREO |		\
				RADIO_CAPS_DETECT_SIGNAL |		\
				RADIO_CAPS_SET_MONO |			\
				RADIO_CAPS_LOCK_SENSITIVITY |		\
				RADIO_CAPS_HW_AFC |			\
				RADIO_CAPS_HW_SEARCH

#if 0
#define RTII_SIGNAL		(1 << 3)
#define RTII_STEREO		(1 << 3)
#endif /* 0 */

#define RTII_MUTE		0x01
#define RTII_UNMUTE		0x00

#define RTII_RESET		0xC8

#define RTII_DATA_ON		(1 << 2)
#define RTII_DATA_OFF		(0 << 2)

#define RTII_CLCK_ON		(1 << 1)
#define RTII_CLCK_OFF		(0 << 1)

#define RTII_WREN_ON		(0 << 0)
#define RTII_WREN_OFF		(1 << 0)

#define RTII_READ_CLOCK_LOW	(RTII_DATA_ON | RTII_CLCK_OFF | RTII_WREN_OFF)
#define RTII_READ_CLOCK_HIGH	(RTII_DATA_ON | RTII_CLCK_ON | RTII_WREN_OFF)

int	rtii_probe(struct device *, void *, void *);
void	rtii_attach(struct device *, struct device * self, void *);

int	rtii_get_info(void *, struct radio_info *);
int	rtii_set_info(void *, struct radio_info *);
int	rtii_search(void *, int);

/* define our interface to the higher level radio driver */
struct radio_hw_if rtii_hw_if = {
	NULL,	/* open */
	NULL,	/* close */
	rtii_get_info,
	rtii_set_info,
	rtii_search
};

struct rtii_softc {
	struct device	dev;

	u_int32_t	freq;
	u_int32_t	stereo;
	u_int32_t	lock;
	u_int8_t	vol;
	int	mute;

	struct tea5757_t	tea;
};

struct cfattach rtii_ca = {
	sizeof(struct rtii_softc), rtii_probe, rtii_attach
};

struct cfdriver rtii_cd = {
	NULL, "rtii", DV_DULL
};

void	rtii_set_mute(struct rtii_softc *);
int	rtii_find(bus_space_tag_t, bus_space_handle_t, int);

u_int32_t	rtii_hw_read(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	rtii_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	rtii_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	rtii_write_bit(bus_space_tag_t, bus_space_handle_t, bus_size_t, int);

int
rtii_probe(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct cfdata *cf = ((struct device *)match)->dv_cfdata;
	int iosize = 1, iobase = ia->ia_iobase;

	if (!RTII_BASE_VALID(iobase)) {
		printf("rtii: configured iobase 0x%x invalid\n", iobase);
		return (0);
	}

	if (bus_space_map(iot, iobase, iosize, 0, &ioh))
		return (0);

	if (!rtii_find(iot, ioh, cf->cf_flags)) {
		bus_space_unmap(iot, ioh, iosize);
		return (0);
	}

	bus_space_unmap(iot, ioh, iosize);
	ia->ia_iosize = iosize;
	return (1);
}

void
rtii_attach(struct device *parent, struct device *self, void *aux)
{
	struct rtii_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;
	struct cfdata *cf = sc->dev.dv_cfdata;

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

	sc->tea.init = rtii_init;
	sc->tea.rset = rtii_rset;
	sc->tea.write_bit = rtii_write_bit;
	sc->tea.read = rtii_hw_read;

	printf(": AIMS Lab Radiotrack II\n");
	tea5757_set_freq(&sc->tea, sc->stereo, sc->lock, sc->freq);
	rtii_set_mute(sc);

	radio_attach_mi(&rtii_hw_if, sc, &sc->dev);
}

/*
 * Mute/unmute the card
 */
void
rtii_set_mute(struct rtii_softc *sc)
{
	u_int8_t mute;

	mute = (sc->mute || !sc->vol) ? RTII_MUTE : RTII_UNMUTE;
	bus_space_write_1(sc->tea.iot, sc->tea.ioh, 0, mute);
	DELAY(6);
	bus_space_write_1(sc->tea.iot, sc->tea.ioh, 0, mute);
}

void
rtii_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off, u_int32_t d)
{
	bus_space_write_1(iot, ioh, off, RTII_RESET | RTII_WREN_OFF);
	bus_space_write_1(iot, ioh, off, RTII_RESET | RTII_WREN_ON);
	bus_space_write_1(iot, ioh, off, RTII_RESET | RTII_WREN_ON);
}

void
rtii_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off, u_int32_t d)
{
	bus_space_write_1(iot, ioh, off, RTII_RESET | RTII_WREN_OFF);
}

int
rtii_find(bus_space_tag_t iot, bus_space_handle_t ioh, int flags)
{
	struct rtii_softc sc;
	u_int32_t freq;

	sc.tea.iot = iot;
	sc.tea.ioh = ioh;
	sc.tea.offset = 0;
	sc.tea.flags = flags;
	sc.tea.init = rtii_init;
	sc.tea.rset = rtii_rset;
	sc.tea.write_bit = rtii_write_bit;
	sc.tea.read = rtii_hw_read;
	sc.lock = TEA5757_S030;
	sc.stereo = TEA5757_STEREO;

	/*
	 * Let's try to write and read a frequency.
	 * If the written and read frequencies are
	 * the same then success.
	 */
	sc.freq = MIN_FM_FREQ;
	tea5757_set_freq(&sc.tea, sc.stereo, sc.lock, sc.freq);
	rtii_set_mute(&sc);
	freq = rtii_hw_read(iot, ioh, sc.tea.offset);
	if (tea5757_decode_freq(freq, sc.tea.flags & TEA5757_TEA5759)
			== sc.freq)
		return 1;

	return 0;
}

void
rtii_write_bit(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off, int bit)
{
	u_int8_t data;

	data = bit ? RTII_DATA_ON : RTII_DATA_OFF;

	bus_space_write_1(iot, ioh, off, RTII_WREN_ON | RTII_CLCK_OFF | data);
	bus_space_write_1(iot, ioh, off, RTII_WREN_ON | RTII_CLCK_ON  | data);
	bus_space_write_1(iot, ioh, off, RTII_WREN_ON | RTII_CLCK_OFF | data);
}

u_int32_t
rtii_hw_read(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off)
{
	u_int8_t i;
	u_int32_t res = 0;

	bus_space_write_1(iot, ioh, off, RTII_READ_CLOCK_LOW);
	DELAY(6);

	i = 24;
	while ( i-- ) {
		bus_space_write_1(iot, ioh, off, RTII_READ_CLOCK_HIGH);
		DELAY(6);
		bus_space_write_1(iot, ioh, off, RTII_READ_CLOCK_LOW);
		res |= bus_space_read_1(iot, ioh, off) & RTII_DATA_ON ? 1 : 0;
		DELAY(6);
		res <<= 1;
	}

	return (res & (TEA5757_DATA | TEA5757_FREQ)) >> 1;
}

int
rtii_get_info(void *v, struct radio_info *ri)
{
	struct rtii_softc *sc = v;

	ri->mute = sc->mute;
	ri->volume = sc->vol ? 255 : 0;
	ri->stereo = sc->stereo == TEA5757_STEREO ? 1 : 0;
	ri->caps = RTII_CAPABILITIES;
	ri->rfreq = 0;
	ri->lock = tea5757_decode_lock(sc->lock);

	ri->freq  = sc->freq = tea5757_decode_freq(rtii_hw_read(sc->tea.iot,
	    sc->tea.ioh, sc->tea.offset), sc->tea.flags & TEA5757_TEA5759);

	switch (bus_space_read_1(sc->tea.iot, sc->tea.ioh, 0)) {
	case 0xFD:
		ri->info = RADIO_INFO_SIGNAL | RADIO_INFO_STEREO;
		break;
	case 0xFF:
		ri->info = 0;
		break;
	default:
		ri->info = RADIO_INFO_SIGNAL;
	}

	return (0);
}

int
rtii_set_info(void *v, struct radio_info *ri)
{
	struct rtii_softc *sc = v;

	sc->mute = ri->mute ? 1 : 0;
	sc->vol = ri->volume ? 255 : 0;
	sc->stereo = ri->stereo ? TEA5757_STEREO: TEA5757_MONO;
	sc->lock = tea5757_encode_lock(ri->lock);
	ri->freq = sc->freq = tea5757_set_freq(&sc->tea,
			sc->lock, sc->stereo, ri->freq);
	rtii_set_mute(sc);

	return (0);
}

int
rtii_search(void *v, int f)
{
	struct rtii_softc *sc = v;

	tea5757_search(&sc->tea, sc->lock, sc->stereo, f);
	rtii_set_mute(sc);

	return (0);
}
