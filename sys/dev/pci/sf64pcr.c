/*	$OpenBSD: sf64pcr.c,v 1.1 2001/10/04 20:22:11 gluk Exp $	*/

/*
 * Copyright (c) 2001 Vladimir Popov <jumbo@narod.ru>
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

/* MediaForte SoundForte SF64-PCR PCI Radio Card Device Driver */

/*
 * Philips TEA5757H AM/FM Self Tuned Radio:
 *	http://www.semiconductors.philips.com/pip/TEA5757H
 *
 * ForteMedia FM801:
 *	???
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/radioio.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/radio_if.h>
#include <dev/ic/tea5757.h>

/* config base I/O address ? */
#define PCI_CBIO 0x6400	
#define SF64PCR_PCI_OFFSET	0x52

#define CARD_RADIO_CAPS	RADIO_CAPS_DETECT_STEREO |			\
			RADIO_CAPS_DETECT_SIGNAL |			\
			RADIO_CAPS_SET_MONO |				\
			RADIO_CAPS_HW_SEARCH |				\
			RADIO_CAPS_HW_AFC |				\
			RADIO_CAPS_LOCK_SENSITIVITY

#define SF64PCR_MUTE		0xF800
#define SF64PCR_UNMUTE		0xF802

#define SF64PCR_SIGNAL		0x80
#define SF64PCR_STEREO		0x80

#define SF64PCR_CLCK_ON		(1 << 0)
#define SF64PCR_CLCK_OFF	(0 << 0)

#define SF64PCR_WREN1_ON	(0 << 1)
#define SF64PCR_WREN1_OFF	(1 << 1)

#define SF64PCR_DATA_ON		(1 << 2)
#define SF64PCR_DATA_OFF	(0 << 2)

#define SF64PCR_WREN2_ON	(0 << 10)
#define SF64PCR_WREN2_OFF	(1 << 10)

#define SF64PCR_0xF800		0xF800

#define SF64PCR_WRITE_ONE_CLOCK_LOW			\
			SF64PCR_0xF800   |		\
			SF64PCR_WREN1_ON | 		\
			SF64PCR_WREN2_ON |		\
			SF64PCR_DATA_ON  |		\
			SF64PCR_CLCK_OFF

/* 0xFC02 */
#define SF64PCR_READ_CLOCK_LOW				\
			SF64PCR_0xF800    |		\
			SF64PCR_CLCK_OFF  |		\
			SF64PCR_WREN1_OFF |		\
			SF64PCR_DATA_OFF  |		\
			SF64PCR_WREN2_OFF 

/* 0xFC03 */
#define SF64PCR_READ_CLOCK_HIGH				\
			SF64PCR_0xF800    |		\
			SF64PCR_CLCK_ON   |		\
			SF64PCR_WREN1_OFF |		\
			SF64PCR_DATA_OFF  |		\
			SF64PCR_WREN2_OFF

int	sf64pcr_match(struct device *, void *, void *);
void	sf64pcr_attach(struct device *, struct device * self, void *);
int	sf64pcr_open(dev_t, int, int, struct proc *);
int	sf64pcr_close(dev_t, int, int, struct proc *);
int	sf64pcr_ioctl(dev_t, u_long, caddr_t, int, struct proc *);

/* define our interface to the high-level radio driver */
struct radio_hw_if sf4r_hw_if = {
	sf64pcr_open,
	sf64pcr_close,
	sf64pcr_ioctl
};

struct sf64pcr_softc {
	struct device	dev;

	u_char	mute;
	u_char	vol;
	u_long	freq;
	u_long	stereo;
	u_long	lock;

	struct tea5757_t	tea;
};

struct cfattach sf4r_ca = {
	sizeof(struct sf64pcr_softc), sf64pcr_match, sf64pcr_attach
};

struct cfdriver sf4r_cd = {
	NULL, "sf4r", DV_DULL
};

/*
 * Function prototypes
 */
void	sf64pcr_set_mute(struct sf64pcr_softc *);

void	sf64pcr_init(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_long);
void	sf64pcr_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_long);
void	sf64pcr_write_bit(bus_space_tag_t, bus_space_handle_t,
		bus_size_t, u_char);
u_long	sf64pcr_hw_read(bus_space_tag_t, bus_space_handle_t, bus_size_t);

/*
 * PCI initialization stuff
 */
int
sf64pcr_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	/* FIXME: more thorough testing */
	/* desc = "SoundForte RadioLink SF64-PCR PCI"; */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_FORTEMEDIA &&
		PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_FORTEMEDIA_FM801)
		return (1);
	return (0);
}

void
sf64pcr_attach(struct device *parent, struct device *self, void *aux)
{
	struct sf64pcr_softc *sc = (struct sf64pcr_softc *) self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	bus_addr_t iobase;
	bus_size_t iosize;
	pcireg_t csr;

	if (pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize)) {
		printf (": can't find i/o base\n");
		return;
	}

	if (bus_space_map(sc->tea.iot = pa->pa_iot, iobase, iosize, 0,
				&sc->tea.ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	/* Enable the card */
	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
			csr | PCI_COMMAND_MASTER_ENABLE);

#ifdef RADIO_INIT_VOLU
	sc->vol = RADIO_INIT_VOLU;
#else
	sc->vol = 0;
#endif /* RADIO_INIT_VOLU */
#ifdef RADIO_INIT_MUTE
	sc->mute = RADIO_INIT_MUTE;
#else
	sc->mute = 0;
#endif /* RADIO_INIT_MUTE */
#ifdef RADIO_INIT_FREQ
	sc->freq = RADIO_INIT_FREQ;
#else
	sc->freq = MIN_FM_FREQ;
#endif /* RADIO_INIT_FREQ */
	sc->stereo = TEA5757_STEREO;
	sc->lock = TEA5757_S030;
	sc->tea.offset = SF64PCR_PCI_OFFSET;
	sc->tea.init = sf64pcr_init;
	sc->tea.rset = sf64pcr_rset;
	sc->tea.write_bit = sf64pcr_write_bit;
	sc->tea.read = sf64pcr_hw_read;

	tea5757_set_freq(&sc->tea, sc->lock, sc->stereo, sc->freq);
	sf64pcr_set_mute(sc);

	radio_attach_mi(&sf4r_hw_if, sc, &sc->dev);
}

int
sf64pcr_open(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct sf64pcr_softc *sc;
	return !(sc = sf4r_cd.cd_devs[0]) ? ENXIO : 0;
}

int
sf64pcr_close(dev_t dev, int flags, int fmt, struct proc *p)
{
	return 0;
}

/*
 * Mute/unmute
 */
void
sf64pcr_set_mute(struct sf64pcr_softc *sc)
{
	u_long mute;

	mute = (sc->mute || !sc->vol) ? SF64PCR_MUTE : SF64PCR_UNMUTE;
	bus_space_write_2(sc->tea.iot, sc->tea.ioh, sc->tea.offset, mute);
}

void
sf64pcr_write_bit(bus_space_tag_t iot, bus_space_handle_t ioh,
		bus_size_t off, u_char bit)
{
	u_int data, wren;

	wren = SF64PCR_0xF800 | SF64PCR_WREN1_ON | SF64PCR_WREN2_ON;
	data = bit ? SF64PCR_DATA_ON : SF64PCR_DATA_OFF;

	bus_space_write_2(iot, ioh, off, SF64PCR_CLCK_OFF | wren | data);
	DELAY(4);
	bus_space_write_2(iot, ioh, off, SF64PCR_CLCK_ON | wren | data);
	DELAY(4);
	bus_space_write_2(iot, ioh, off, SF64PCR_CLCK_OFF | wren | data);
	DELAY(4);
}

void
sf64pcr_init(bus_space_tag_t iot, bus_space_handle_t ioh,
		bus_size_t offset, u_long d)
{
	d = SF64PCR_WRITE_ONE_CLOCK_LOW;

	bus_space_write_2(iot, ioh, offset, d);
	DELAY(4);
}

void
sf64pcr_rset(bus_space_tag_t iot, bus_space_handle_t ioh,
		bus_size_t offset, u_long d)
{
	/* Do nothing */
	return;
}

/*
 * Read TEA5757 shift register
 */
u_long
sf64pcr_hw_read(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t offset)
{
	u_long res = 0ul;
	int rb, ind = 0;

	bus_space_write_2(iot, ioh, offset, SF64PCR_READ_CLOCK_LOW);
	DELAY(4);

	/* Read the register */
	rb = 23;
	while (rb--) {
		bus_space_write_2(iot, ioh, offset, SF64PCR_READ_CLOCK_HIGH);
		DELAY(4);			

		bus_space_write_2(iot, ioh, offset, SF64PCR_READ_CLOCK_LOW);
		DELAY(4);

		res |= bus_space_read_2(iot, ioh, offset) & SF64PCR_DATA_ON ?
			1 : 0;
		res <<= 1;
	}

	bus_space_write_2(iot, ioh, offset, SF64PCR_READ_CLOCK_HIGH);
	DELAY(4);			

	rb = bus_space_read_1(iot, ioh, offset);
	ind = rb & SF64PCR_SIGNAL ? (1 << 1) : (0 << 1); /* Tuning */

	bus_space_write_2(iot, ioh, offset, SF64PCR_READ_CLOCK_LOW);

	rb = bus_space_read_2(iot, ioh, offset);
	ind |= rb & SF64PCR_STEREO ? (1 << 0) : (0 << 0); /* Mono */
	res |= rb & SF64PCR_DATA_ON ? 1 : 0;

	return (res & (TEA5757_DATA | TEA5757_FREQ)) | (ind << 24);
}

int
sf64pcr_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct proc *pr)
{
	struct sf64pcr_softc *sc = sf4r_cd.cd_devs[0];
	int error = 0;
	u_long freq;

	switch (cmd) {
	case RIOCGINFO:
		freq = sf64pcr_hw_read(sc->tea.iot, sc->tea.ioh, sc->tea.offset);
		*(u_long *)arg = 0;
		if (freq & (1 << 24))
			*(u_long *)arg |= !RADIO_INFO_STEREO;
		if (freq & (1 << 25))
			*(u_long *)arg |= !RADIO_INFO_SIGNAL;
		break;
	case RIOCGMONO:
		*(u_long *)arg = sc->stereo == TEA5757_STEREO ? 0 : 1;
		break;
	case RIOCSMONO:
		sc->stereo = *(u_long *)arg ? TEA5757_MONO : TEA5757_STEREO;
		tea5757_set_freq(&sc->tea, sc->lock, sc->stereo, sc->freq);
		sf64pcr_set_mute(sc);
		break;
	case RIOCGCAPS:
		*(u_long *)arg = CARD_RADIO_CAPS;
		break;
	case RIOCGVOLU:
		*(u_long *)arg = sc->vol ? 255 : 0;
		break;
	case RIOCGMUTE:
		*(u_long *)arg = sc->mute ? 1 : 0;
		break;
	case RIOCSVOLU:
		sc->vol = *(u_long *)arg ? 255 : 0;
		sf64pcr_set_mute(sc);
		break;
	case RIOCSMUTE:
		sc->mute = *(u_long *)arg ? 1 : 0;
		sf64pcr_set_mute(sc);
		break;
	case RIOCGFREQ:
		freq = tea5757_decode_freq(sf64pcr_hw_read(sc->tea.iot,
					sc->tea.ioh, sc->tea.offset));
		*(u_long *)arg = freq;
		sc->freq = freq;
		break;
	case RIOCSFREQ:
		sc->freq = tea5757_set_freq(&sc->tea, sc->lock,
				sc->stereo, *(u_long *)arg);
		sf64pcr_set_mute(sc);
		break;
	case RIOCGLOCK:
		*(u_long *)arg = tea5757_decode_lock(sc->lock);
		break;
	case RIOCSLOCK:
		sc->lock = tea5757_encode_lock(*(u_char *)arg);
		tea5757_set_freq(&sc->tea, sc->lock, sc->stereo, sc->freq);
		sf64pcr_set_mute(sc);
		break;
	case RIOCSSRCH:
		tea5757_search(&sc->tea, sc->lock, sc->stereo, *(u_long *)arg);
		sf64pcr_set_mute(sc);
		break;
	case RIOCSREFF:
		/* FALLTHROUGH */
	case RIOCGREFF:
		/* FALLTHROUGH */
	default:
		error = EINVAL; /* not supported */
	}
	return (error);
}
