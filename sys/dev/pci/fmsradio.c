/*	$OpenBSD: fmsradio.c,v 1.2 2002/05/09 14:52:28 mickey Exp $	*/

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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Device Driver for FM Tuners attached to FM801 */

/* Currently supported tuners:
 *  o SoundForte RadioLink SF64-PCR PCI Radio Card
 *  o SoundForte Quad X-treme SF256-PCP-R PCI Sound Card with FM Radio
 *  o SoundForte Theatre X-treme 5.1 SF256-PCS PCI Sound Card with FM Radio
 */

#include "radio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/radioio.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/audio_if.h>
#include <dev/radio_if.h>

#include <dev/ic/ac97.h>
#include <dev/pci/fmsradio.h>
#include <dev/pci/fmsreg.h>
#include <dev/pci/fmsvar.h>

#include <dev/ic/tea5757.h>

#define TUNER_UNKNOWN		0
#define TUNER_SF256PCPR		1
#define TUNER_SF64PCR		2
#define TUNER_SF256PCS		3
#define TUNER_NOT_ATTACHED	0xFFFF

#define SF64PCR_CAPS		RADIO_CAPS_DETECT_STEREO |	\
				RADIO_CAPS_DETECT_SIGNAL |	\
				RADIO_CAPS_SET_MONO |	\
				RADIO_CAPS_HW_SEARCH |	\
				RADIO_CAPS_HW_AFC |	\
				RADIO_CAPS_LOCK_SENSITIVITY

#define SF256PCPR_CAPS		RADIO_CAPS_SET_MONO |	\
				RADIO_CAPS_HW_SEARCH |	\
				RADIO_CAPS_HW_AFC |	\
				RADIO_CAPS_LOCK_SENSITIVITY

#define SF256PCS_CAPS		RADIO_CAPS_SET_MONO |	\
				RADIO_CAPS_HW_SEARCH |	\
				RADIO_CAPS_HW_AFC |	\
				RADIO_CAPS_LOCK_SENSITIVITY

#define PCR_WREN_ON		(0 << 1)
#define PCR_WREN_OFF		(1 << 1)
#define PCR_CLOCK_ON		(1 << 0)
#define PCR_CLOCK_OFF		(0 << 0)
#define PCR_DATA_ON		(1 << 2)
#define PCR_DATA_OFF		(0 << 2)

#define PCR_SIGNAL		0x80
#define PCR_STEREO		0x80
#define PCR_INFO_SIGNAL		(1 << 24)
#define PCR_INFO_STEREO		(1 << 25)

#define PCPR_WREN_ON		(0 << 2)
#define PCPR_WREN_OFF		(1 << 2)
#define PCPR_CLOCK_ON		(1 << 0)
#define PCPR_CLOCK_OFF		(0 << 0)
#define PCPR_DATA_ON		(1 << 1)
#define PCPR_DATA_OFF		(0 << 1)

#define PCS_WREN_ON		(0 << 2)
#define PCS_WREN_OFF		(1 << 2)
#define PCS_CLOCK_ON		(1 << 3)
#define PCS_CLOCK_OFF		(0 << 3)
#define PCS_DATA_ON		(1 << 1)
#define PCS_DATA_OFF		(0 << 1)

/*
 * Function prototypes
 */
void	fmsradio_set_mute(struct fmsradio_if *);

void	sf64pcr_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf64pcr_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf64pcr_write_bit(bus_space_tag_t, bus_space_handle_t, bus_size_t, int);
u_int32_t	sf64pcr_hw_read(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	sf64pcr_probe(struct fmsradio_if *);

void	sf256pcpr_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf256pcpr_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf256pcpr_write_bit(bus_space_tag_t, bus_space_handle_t, bus_size_t, int);
u_int32_t	sf256pcpr_hw_read(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	sf256pcpr_probe(struct fmsradio_if *);

void	sf256pcs_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf256pcs_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void	sf256pcs_write_bit(bus_space_tag_t, bus_space_handle_t, bus_size_t, int);
u_int32_t	sf256pcs_hw_read(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	sf256pcs_probe(struct fmsradio_if *);

int
fmsradio_attach(struct fmsradio_if *sc, char *devname)
{
	sc->vol = 0;
	sc->mute = 0;
	sc->freq = MIN_FM_FREQ;
	sc->stereo = TEA5757_STEREO;
	sc->lock = TEA5757_S030;

	sc->card = TUNER_UNKNOWN;
	if ((sc->card = sf64pcr_probe(sc)) == TUNER_SF64PCR)
		printf("%s: SF64-PCR FM Radio\n", devname);
	else if ((sc->card = sf256pcpr_probe(sc)) == TUNER_SF256PCPR)
		printf("%s: SF256-PCP-R FM Radio\n", devname);
	else if ((sc->card = sf256pcs_probe(sc)) == TUNER_SF256PCS)
		printf("%s: SF256-PCS FM Radio\n", devname);
	else {
		sc->card = TUNER_NOT_ATTACHED;
		return 0;
	}

	fmsradio_set_mute(sc);

	return sc->card;
}

/* SF256-PCS specific routines */
int
sf256pcs_probe(struct fmsradio_if *sc)
{
	u_int32_t freq;

	sc->tea.init = sf256pcs_init;
	sc->tea.rset = sf256pcs_rset;
	sc->tea.write_bit = sf256pcs_write_bit;
	sc->tea.read = sf256pcs_hw_read;

	tea5757_set_freq(&sc->tea, sc->stereo, sc->lock, sc->freq);
	freq = sf256pcs_hw_read(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
	if (tea5757_decode_freq(freq, sc->tea.flags & TEA5757_TEA5759)
			!= sc->freq)
			return TUNER_UNKNOWN;

	return TUNER_SF256PCS;
}

u_int32_t
sf256pcs_hw_read(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset)
{
	u_int32_t res = 0ul;
	u_int16_t i, d;

	d  = FM_IO_GPIO | PCS_WREN_OFF;

	/* Now read data in */
	d |= FM_IO_GPIO1_IN | PCS_DATA_ON;

	bus_space_write_2(iot, ioh, offset, d | PCS_CLOCK_OFF);

	/* Read the register */
	i = 24;
	while (i--) {
		res <<= 1;
		bus_space_write_2(iot, ioh, offset, d | PCS_CLOCK_ON);
		bus_space_write_2(iot, ioh, offset, d | PCS_CLOCK_OFF);
		res |= bus_space_read_2(iot, ioh, offset) & PCS_DATA_ON ? 1 : 0;
	}

	return (res & (TEA5757_DATA | TEA5757_FREQ));
}

void
sf256pcs_write_bit(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		int bit)
{
	u_int16_t data, wren;

	wren = FM_IO_GPIO | FM_IO_GPIO2_IN | PCS_WREN_ON;
	data = bit ? PCPR_DATA_ON : PCS_DATA_OFF;

	bus_space_write_2(iot, ioh, off, PCS_CLOCK_OFF | wren | data);
	bus_space_write_2(iot, ioh, off, PCS_CLOCK_ON  | wren | data);
	bus_space_write_2(iot, ioh, off, PCS_CLOCK_OFF | wren | data);
}

void
sf256pcs_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset,
		u_int32_t d)
{
	d  = FM_IO_GPIO | FM_IO_GPIO1_IN;
	d |= PCS_WREN_ON | PCS_DATA_OFF | PCS_CLOCK_OFF;

	bus_space_write_2(iot, ioh, offset, d);
	bus_space_write_2(iot, ioh, offset, d);
}

void
sf256pcs_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset,
		u_int32_t d)
{
	d  = FM_IO_GPIO | FM_IO_GPIO1_IN;
	d |= PCS_WREN_OFF | PCS_DATA_OFF | PCS_CLOCK_OFF;

	bus_space_write_2(iot, ioh, offset, d);
	bus_space_write_2(iot, ioh, offset, d);
}

/* SF256-PCP-R specific routines */
int
sf256pcpr_probe(struct fmsradio_if *sc)
{
	u_int32_t freq;

	sc->tea.init = sf256pcpr_init;
	sc->tea.rset = sf256pcpr_rset;
	sc->tea.write_bit = sf256pcpr_write_bit;
	sc->tea.read = sf256pcpr_hw_read;

	tea5757_set_freq(&sc->tea, sc->stereo, sc->lock, sc->freq);
	freq = sf256pcpr_hw_read(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
	if (tea5757_decode_freq(freq, sc->tea.flags & TEA5757_TEA5759)
			!= sc->freq)
			return TUNER_UNKNOWN;

	return TUNER_SF256PCPR;
}

u_int32_t
sf256pcpr_hw_read(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset)
{
	u_int32_t res = 0ul;
	u_int16_t i, d;

	d  = FM_IO_GPIO;
	d |= FM_IO_GPIO3_IN | PCPR_WREN_OFF;

	/* Now read data in */
	d |= FM_IO_GPIO1_IN | PCPR_DATA_ON;

	bus_space_write_2(iot, ioh, offset, d | PCPR_CLOCK_OFF);

	/* Read the register */
	i = 24;
	while (i--) {
		res <<= 1;
		bus_space_write_2(iot, ioh, offset, d | PCPR_CLOCK_ON);
		bus_space_write_2(iot, ioh, offset, d | PCPR_CLOCK_OFF);
		res |= bus_space_read_2(iot, ioh, offset) & PCPR_DATA_ON ? 1 : 0;
	}

	return (res & (TEA5757_DATA | TEA5757_FREQ));
}

void
sf256pcpr_write_bit(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		int bit)
{
	u_int16_t data, wren;

	wren = FM_IO_GPIO | FM_IO_GPIO3_IN | PCPR_WREN_ON;
	data = bit ? PCPR_DATA_ON : PCPR_DATA_OFF;

	bus_space_write_2(iot, ioh, off, PCPR_CLOCK_OFF | wren | data);
	bus_space_write_2(iot, ioh, off, PCPR_CLOCK_ON  | wren | data);
	bus_space_write_2(iot, ioh, off, PCPR_CLOCK_OFF | wren | data);
}

void
sf256pcpr_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset,
		u_int32_t d)
{
	d  = FM_IO_GPIO | FM_IO_GPIO3_IN;
	d |= PCPR_WREN_ON | PCPR_DATA_OFF | PCPR_CLOCK_OFF;

	bus_space_write_2(iot, ioh, offset, d);
	bus_space_write_2(iot, ioh, offset, d);
}

void
sf256pcpr_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset,
		u_int32_t d)
{
	d  = FM_IO_GPIO | FM_IO_GPIO3_IN;
	d |= PCPR_WREN_OFF | PCPR_DATA_OFF | PCPR_CLOCK_OFF;

	bus_space_write_2(iot, ioh, offset, d);
	bus_space_write_2(iot, ioh, offset, d);
}

/* SF64-PCR specific routines */
int
sf64pcr_probe(struct fmsradio_if *sc)
{
	u_int32_t freq;

	sc->tea.init = sf64pcr_init;
	sc->tea.rset = sf64pcr_rset;
	sc->tea.write_bit = sf64pcr_write_bit;
	sc->tea.read = sf64pcr_hw_read;

	tea5757_set_freq(&sc->tea, sc->stereo, sc->lock, sc->freq);
	freq = sf64pcr_hw_read(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
	if (tea5757_decode_freq(freq, sc->tea.flags & TEA5757_TEA5759)
			!= sc->freq)
			return TUNER_UNKNOWN;

	return TUNER_SF64PCR;
}

u_int32_t
sf64pcr_hw_read(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset)
{
	u_int32_t res = 0ul;
	u_int16_t d, i, ind = 0;

	d  =  FM_IO_GPIO;
	d |= FM_IO_GPIO3_IN | PCR_WREN_OFF;

	/* Now read data in */
	d |= FM_IO_GPIO2_IN | PCR_DATA_ON;

	bus_space_write_2(iot, ioh, offset, d | PCR_CLOCK_OFF);
	DELAY(4);

	/* Read the register */
	i = 23;
	while (i--) {
		bus_space_write_2(iot, ioh, offset, d | PCR_CLOCK_ON);
		DELAY(4);

		bus_space_write_2(iot, ioh, offset, d | PCR_CLOCK_OFF);
		DELAY(4);

		res |= bus_space_read_2(iot, ioh, offset) & PCR_DATA_ON ? 1 : 0;
		res <<= 1;
	}

	bus_space_write_2(iot, ioh, offset, d | PCR_CLOCK_ON);
	DELAY(4);

	i = bus_space_read_1(iot, ioh, offset);
	ind = i & PCR_SIGNAL ? (1 << 1) : (0 << 1); /* Tuning */

	bus_space_write_2(iot, ioh, offset, d | PCR_CLOCK_OFF);

	i = bus_space_read_2(iot, ioh, offset);
	ind |= i & PCR_STEREO ? (1 << 0) : (0 << 0); /* Mono */
	res |= i & PCR_DATA_ON ? (1 << 0) : (0 << 0);

	return (res & (TEA5757_DATA | TEA5757_FREQ)) | (ind << 24);
}

void
sf64pcr_write_bit(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		int bit)
{
	u_int16_t data, wren;

	wren = FM_IO_GPIO | FM_IO_GPIO3_IN | PCR_WREN_ON;
	data = bit ? PCR_DATA_ON : PCR_DATA_OFF;

	bus_space_write_2(iot, ioh, off, PCR_CLOCK_OFF | wren | data);
	DELAY(4);
	bus_space_write_2(iot, ioh, off, PCR_CLOCK_ON | wren | data);
	DELAY(4);
	bus_space_write_2(iot, ioh, off, PCR_CLOCK_OFF | wren | data);
	DELAY(4);
}

void
sf64pcr_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset,
		u_int32_t d)
{
	d  = FM_IO_GPIO | FM_IO_GPIO3_IN;
	d |= PCR_WREN_ON | PCR_DATA_ON | PCR_CLOCK_OFF;

	bus_space_write_2(iot, ioh, offset, d);
	DELAY(4);
}

void
sf64pcr_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset,
		u_int32_t d)
{
	/* Do nothing */
	return;
}


/* Common tuner routines */
/*
 * Mute/unmute
 */
void
fmsradio_set_mute(struct fmsradio_if *sc)
{
	u_int16_t v, mute, unmute;

	mute = unmute = FM_IO_GPIO;
	switch (sc->card) {
	case TUNER_SF256PCS:
		mute |= FM_IO_GPIO1_IN;
		unmute |= FM_IO_GPIO1_IN | FM_IO_GPIO2;
		break;
	case TUNER_SF256PCPR:
		mute |= FM_IO_GPIO3_IN;
		unmute |= FM_IO_GPIO3_IN | FM_IO_GPIO2;
		break;
	case TUNER_SF64PCR:
		mute |= FM_IO_GPIO3_IN;
		unmute |= FM_IO_GPIO3_IN | FM_IO_GPIO1;
		break;
	default:
		return;
	}
	v = (sc->mute || !sc->vol) ? mute : unmute;
	bus_space_write_2(sc->tea.iot, sc->tea.ioh, sc->tea.offset, v);
	DELAY(64);
	bus_space_write_2(sc->tea.iot, sc->tea.ioh, sc->tea.offset, v);
}

int
fmsradio_get_info(void *v, struct radio_info *ri)
{
	struct fms_softc *fms_sc = v;
	struct fmsradio_if *sc = &fms_sc->radio;
	u_int32_t buf;

	if (sc->card == TUNER_NOT_ATTACHED)
		return (ENXIO);

	ri->mute = sc->mute;
	ri->volume = sc->vol ? 255 : 0;
	ri->stereo = sc->stereo == TEA5757_STEREO ? 1 : 0;
	ri->lock = tea5757_decode_lock(sc->lock);

	switch (sc->card) {
	case TUNER_SF256PCS:
		ri->caps = SF256PCS_CAPS;
		buf = sf256pcs_hw_read(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
		ri->info = 0; /* UNSUPPORTED */
		break;
	case TUNER_SF256PCPR:
		ri->caps = SF256PCPR_CAPS;
		buf = sf256pcpr_hw_read(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
		ri->info = 0; /* UNSUPPORTED */
		break;
	case TUNER_SF64PCR:
		ri->caps = SF64PCR_CAPS;
		buf = sf64pcr_hw_read(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
		ri->info  = buf & PCR_INFO_SIGNAL ? 0 : RADIO_INFO_SIGNAL;
		ri->info |= buf & PCR_INFO_STEREO ? 0 : RADIO_INFO_STEREO;
		break;
	default:
		break;
	}

	ri->freq = sc->freq = tea5757_decode_freq(buf,
			sc->tea.flags & TEA5757_TEA5759);

	fmsradio_set_mute(sc);

	/* UNSUPPORTED */
	ri->rfreq = 0;

	return (0);
}

int
fmsradio_set_info(void *v, struct radio_info *ri)
{
	struct fms_softc *fms_sc = v;
	struct fmsradio_if *sc = &fms_sc->radio;

	if (sc->card == TUNER_NOT_ATTACHED)
		return (ENXIO);

	sc->mute = ri->mute ? 1 : 0;
	sc->vol = ri->volume ? 255 : 0;
	sc->stereo = ri->stereo ? TEA5757_STEREO: TEA5757_MONO;
	sc->lock = tea5757_encode_lock(ri->lock);
	ri->freq = sc->freq = tea5757_set_freq(&sc->tea,
		sc->lock, sc->stereo, ri->freq);
	fmsradio_set_mute(sc);

	return (0);
}

int
fmsradio_search(void *v, int f)
{
	struct fms_softc *fms_sc = v;
	struct fmsradio_if *sc = &fms_sc->radio;

	if (sc->card == TUNER_NOT_ATTACHED)
		return (ENXIO);

	tea5757_search(&sc->tea, sc->lock, sc->stereo, f);
	fmsradio_set_mute(sc);

	return (0);
}
