/*	$OpenBSD: sf16fmr.c,v 1.1 2002/04/25 04:56:59 mickey Exp $	*/

/*
 * Copyright (c) 2002 Vladimir Popov <jumbo@narod.ru>
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

/* SoundForte RadioLink SF16-FMR FM Radio Card device driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/radioio.h>

#include <dev/isa/isavar.h>
#include <dev/radio_if.h>

#include <dev/ic/tc921x.h>
#include <dev/ic/pt2254a.h>

#define SF16FMR_BASE_VALID(x)	(x == 0x384 || x == 0x284)

#define SF16FMR_CAPABILITIES	0

#define SF16FMR_FREQ_DATA	0
#define SF16FMR_FREQ_CLOCK	1
#define SF16FMR_FREQ_PERIOD 	2

#define SF16FMR_FREQ_STEADY	(1 << SF16FMR_FREQ_DATA) | \
				(1 << SF16FMR_FREQ_CLOCK) | \
				(1 << SF16FMR_FREQ_PERIOD)

#define SF16FMR_VOLU_STROBE_ON	(1 << 3)
#define SF16FMR_VOLU_STROBE_OFF	(0 << 3)
#define SF16FMR_VOLU_CLOCK_ON	(1 << 4)
#define SF16FMR_VOLU_CLOCK_OFF	(0 << 4)
#define SF16FMR_VOLU_DATA_ON	(1 << 5)
#define SF16FMR_VOLU_DATA_OFF	(0 << 5)

int	sfr_probe(struct device *, void *, void *);
void	sfr_attach(struct device *, struct device * self, void *);

int	sfr_get_info(void *, struct radio_info *);
int	sfr_set_info(void *, struct radio_info *);

/* define our interface to the higher level radio driver */
struct radio_hw_if sfr_hw_if = {
	NULL, /* open */
	NULL, /* close */
	sfr_get_info,
	sfr_set_info,
	NULL
};

struct sfr_softc {
	struct device	sc_dev;

	u_int32_t	freq;
	u_int8_t	vol;
	int	mute;

	struct tc921x_t	c;
};

struct cfattach sfr_ca = {
	sizeof(struct sfr_softc), sfr_probe, sfr_attach
};

struct cfdriver sfr_cd = {
	NULL, "sfr", DV_DULL
};

int	sfr_find(bus_space_tag_t, bus_space_handle_t);
u_int32_t	sfr_set_freq(struct tc921x_t *, u_int32_t);
u_int32_t	sfr_get_freq(struct tc921x_t *);
u_int8_t	sfr_set_vol(bus_space_tag_t, bus_space_handle_t, u_int8_t, int);
void	sfr_send_volume(bus_space_tag_t, bus_space_handle_t, u_int32_t);

int
sfr_probe(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int iosize = 1, iobase = ia->ia_iobase;

	if (!SF16FMR_BASE_VALID(iobase)) {
		printf("sfr: configured iobase 0x%x invalid\n", iobase);
		return (0);
	}

	if (bus_space_map(iot, iobase, iosize, 0, &ioh))
		return (0);

	if (!sfr_find(iot, ioh)) {
		bus_space_unmap(iot, ioh, iosize);
		return (0);
	}

	bus_space_unmap(iot, ioh, iosize);
	ia->ia_iosize = iosize;
	return (1);
}

void
sfr_attach(struct device *parent, struct device *self, void *aux)
{
	struct sfr_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;

	sc->c.iot = ia->ia_iot;
	sc->mute = 0;
	sc->vol = 0;
	sc->freq = MIN_FM_FREQ;
	sc->c.period = SF16FMR_FREQ_PERIOD;
	sc->c.clock = SF16FMR_FREQ_CLOCK;
	sc->c.data = SF16FMR_FREQ_DATA;

	/* remap I/O */
	if (bus_space_map(sc->c.iot, ia->ia_iobase, ia->ia_iosize,
			  0, &sc->c.ioh)) {
		printf(": bus_space_map() failed\n");
		return;
	}

	printf(": SoundForte RadioLink SF16-FMR\n");
	sfr_set_freq(&sc->c, sc->freq);
	sfr_set_vol(sc->c.iot, sc->c.ioh, sc->vol, sc->mute);

	radio_attach_mi(&sfr_hw_if, sc, &sc->sc_dev);
}

int
sfr_find(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	struct sfr_softc sc;
	u_int32_t freq;

	sc.c.iot = iot;
	sc.c.ioh = ioh;
	sc.c.offset = 0;
	sc.c.period = SF16FMR_FREQ_PERIOD;
	sc.c.clock = SF16FMR_FREQ_CLOCK;
	sc.c.data = SF16FMR_FREQ_DATA;

	/*
	 * Let's try to write and read a frequency.
	 * If the written and read frequencies are
	 * the same then success.
	 */
	sc.freq = MIN_FM_FREQ;
	/* Initialize the tc921x chip */
	sfr_set_freq(&sc.c, sc.freq);
	/* Do actual frequency setting */
	freq = sfr_set_freq(&sc.c, sc.freq);
	if (sc.freq == freq)
		return 1;

	return 0;
}

int
sfr_get_info(void *v, struct radio_info *ri)
{
	struct sfr_softc *sc = v;

	ri->mute = sc->mute;
	ri->volume = sc->vol;
	ri->caps = SF16FMR_CAPABILITIES;
	ri->freq  = sc->freq = sfr_get_freq(&sc->c);

	/* Not supported */
	ri->stereo = 1; /* Always stereo */
	ri->rfreq = 0;
	ri->lock = 0;

	return (0);
}

int
sfr_set_info(void *v, struct radio_info *ri)
{
	struct sfr_softc *sc = v;

	sc->mute = ri->mute ? 1 : 0;
	sc->vol = ri->volume;
	sc->freq = sfr_set_freq(&sc->c, ri->freq);
	sc->vol = sfr_set_vol(sc->c.iot, sc->c.ioh, sc->vol, sc->mute);

	return (0);
}

u_int32_t
sfr_set_freq(struct tc921x_t *c, u_int32_t freq) {
        u_int32_t data = 0ul;

	data |= tc921x_encode_freq(freq);
	data |= TC921X_D0_REF_FREQ_10_KHZ;
	data |= TC921X_D0_PULSE_SWALLOW_FM_MODE;
	data |= TC921X_D0_OSC_7POINT2_MHZ;
	data |= TC921X_D0_OUT_CONTROL_ON;
	tc921x_write_addr(c, 0xD0, data);

	data  = TC921X_D2_IO_PORT_OUTPUT(4);
	tc921x_write_addr(c, 0xD2, data);

	return sfr_get_freq(c);
}

u_int32_t
sfr_get_freq(struct tc921x_t *c) {
	return tc921x_decode_freq(tc921x_read_addr(c, 0xD1));
}

u_int8_t
sfr_set_vol(bus_space_tag_t iot, bus_space_handle_t ioh, u_int8_t vol, int mute) {
	u_int32_t v;
	u_int8_t ret;

	ret = mute ? 0 : vol;

	v = pt2254a_encode_volume(&ret, 255);

	sfr_send_volume(iot, ioh,
		pt2254a_compose_register(v, v, USE_CHANNEL, USE_CHANNEL));

	return ret;
}

void
sfr_send_volume(bus_space_tag_t iot, bus_space_handle_t ioh, u_int32_t vol) {
	u_int8_t one, zero;
	int i;

	one = zero  = SF16FMR_FREQ_STEADY;
	one = zero |= SF16FMR_VOLU_STROBE_OFF;

	one  |= SF16FMR_VOLU_DATA_ON;
	zero |= SF16FMR_VOLU_DATA_OFF;

	bus_space_write_1(iot, ioh, 0, SF16FMR_VOLU_STROBE_OFF | SF16FMR_FREQ_STEADY);

	for (i = 0; i < PT2254A_REGISTER_LENGTH; i++) {
		if (vol & (1 << i)) {
			bus_space_write_1(iot, ioh, 0,
					one  | SF16FMR_VOLU_CLOCK_OFF);
			bus_space_write_1(iot, ioh, 0,
					one  | SF16FMR_VOLU_CLOCK_ON);
		} else {
			bus_space_write_1(iot, ioh, 0,
					zero | SF16FMR_VOLU_CLOCK_OFF);
			bus_space_write_1(iot, ioh, 0,
					zero | SF16FMR_VOLU_CLOCK_ON);
		}
	}

	/* Latch the data */
	bus_space_write_1(iot, ioh, 0, SF16FMR_VOLU_STROBE_ON | SF16FMR_FREQ_STEADY);
	bus_space_write_1(iot, ioh, 0, SF16FMR_VOLU_STROBE_OFF | SF16FMR_FREQ_STEADY);
}
