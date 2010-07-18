/*	$OpenBSD: wbsio.c,v 1.6 2010/07/18 12:44:55 kettenis Exp $	*/
/*
 * Copyright (c) 2008 Mark Kettenis <kettenis@openbsd.org>
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
 * Winbond LPC Super I/O driver.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

/* ISA bus registers */
#define WBSIO_INDEX		0x00	/* Configuration Index Register */
#define WBSIO_DATA		0x01	/* Configuration Data Register */

#define WBSIO_IOSIZE		0x02	/* ISA I/O space size */

#define WBSIO_CONF_EN_MAGIC	0x87	/* enable configuration mode */
#define WBSIO_CONF_DS_MAGIC	0xaa	/* disable configuration mode */

/* Configuration Space Registers */
#define WBSIO_LDN		0x07	/* Logical Device Number */
#define WBSIO_ID		0x20	/* Device ID */
#define WBSIO_REV		0x21	/* Device Revision */

#define WBSIO_ID_W83627HF	0x52
#define WBSIO_ID_W83627THF	0x82
#define WBSIO_ID_W83627EHF	0x88
#define WBSIO_ID_W83627DHG	0xa0
#define WBSIO_ID_W83627DHGP	0xb0
#define WBSIO_ID_W83627SF	0x59
#define WBSIO_ID_W83637HF	0x70
#define WBSIO_ID_W83697HF	0x60

/* Logical Device Number (LDN) Assignments */
#define WBSIO_LDN_HM		0x0b

/* Hardware Monitor Control Registers (LDN B) */
#define WBSIO_HM_ADDR_MSB	0x60	/* Address [15:8] */
#define WBSIO_HM_ADDR_LSB	0x61	/* Address [7:0] */

#ifdef WBSIO_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

struct wbsio_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int	wbsio_probe(struct device *, void *, void *);
void	wbsio_attach(struct device *, struct device *, void *);
int	wbsio_print(void *, const char *);

struct cfattach wbsio_ca = {
	sizeof(struct wbsio_softc),
	wbsio_probe,
	wbsio_attach
};

struct cfdriver wbsio_cd = {
	NULL, "wbsio", DV_DULL
};

static __inline void
wbsio_conf_enable(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, WBSIO_CONF_EN_MAGIC);
	bus_space_write_1(iot, ioh, WBSIO_INDEX, WBSIO_CONF_EN_MAGIC);
}

static __inline void
wbsio_conf_disable(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, WBSIO_CONF_DS_MAGIC);
}

static __inline u_int8_t
wbsio_conf_read(bus_space_tag_t iot, bus_space_handle_t ioh, u_int8_t index)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, index);
	return (bus_space_read_1(iot, ioh, WBSIO_DATA));
}

static __inline void
wbsio_conf_write(bus_space_tag_t iot, bus_space_handle_t ioh, u_int8_t index,
    u_int8_t data)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, index);
	bus_space_write_1(iot, ioh, WBSIO_DATA, data);
}

int
wbsio_probe(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t reg;

	/* Match by device ID */
	iot = ia->ia_iot;
	if (bus_space_map(iot, ia->ipa_io[0].base, WBSIO_IOSIZE, 0, &ioh))
		return (0);
	wbsio_conf_enable(iot, ioh);
	reg = wbsio_conf_read(iot, ioh, WBSIO_ID);
	DPRINTF(("wbsio_probe: id 0x%02x\n", reg));
	wbsio_conf_disable(iot, ioh);
	bus_space_unmap(iot, ioh, WBSIO_IOSIZE);
	switch (reg) {
	case WBSIO_ID_W83627HF:
	case WBSIO_ID_W83627THF:
	case WBSIO_ID_W83627EHF:
	case WBSIO_ID_W83627DHG:
	case WBSIO_ID_W83627DHGP:
	case WBSIO_ID_W83637HF:
	case WBSIO_ID_W83697HF:
		ia->ipa_nio = 1;
		ia->ipa_io[0].length = WBSIO_IOSIZE;
		ia->ipa_nmem = 0;
		ia->ipa_nirq = 0;
		ia->ipa_ndrq = 0;
		return (1);
	}

	return (0);
}

void
wbsio_attach(struct device *parent, struct device *self, void *aux)
{
	struct wbsio_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct isa_attach_args nia;
	u_int8_t reg, reg0, reg1;
	u_int16_t iobase;

	/* Map ISA I/O space */
	sc->sc_iot = ia->ia_iot;
	if (bus_space_map(sc->sc_iot, ia->ipa_io[0].base,
	    WBSIO_IOSIZE, 0, &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	/* Enter configuration mode */
	wbsio_conf_enable(sc->sc_iot, sc->sc_ioh);

	/* Read device ID */
	reg = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_ID);
	switch (reg) {
	case WBSIO_ID_W83627HF:
		printf(": W83627HF");
		break;
	case WBSIO_ID_W83627THF:
		printf(": W83627THF");
		break;
	case WBSIO_ID_W83627EHF:
		printf(": W83627EHF");
		break;
	case WBSIO_ID_W83627DHG:
		printf(": W83627DHG");
		break;
	case WBSIO_ID_W83627DHGP:
		printf(": W83627DHG-P");
		break;
	case WBSIO_ID_W83637HF:
		printf(": W83637HF");
		break;
	case WBSIO_ID_W83697HF:
		printf(": W83697HF");
		break;
	}

	/* Read device revision */
	reg = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_REV);
	printf(" rev 0x%02x", reg);

	/* Select HM logical device */
	wbsio_conf_write(sc->sc_iot, sc->sc_ioh, WBSIO_LDN, WBSIO_LDN_HM);

	/*
	 * The address should be 8-byte aligned, but it seems some
	 * BIOSes ignore this.  They get away with it, because
	 * Apparently the hardware simply ignores the lower three
	 * bits.  We do the same here.
	 */
	reg0 = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_HM_ADDR_LSB);
	reg1 = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_HM_ADDR_MSB);
	iobase = (reg1 << 8) | (reg0 & ~0x7);

	printf("\n");

	/* Escape from configuration mode */
	wbsio_conf_disable(sc->sc_iot, sc->sc_ioh);

	if (iobase == 0)
		return;

	nia = *ia;
	nia.ia_iobase = iobase;
	config_found(self, &nia, wbsio_print);
}

int
wbsio_print(void *aux, const char *pnp)
{
	struct isa_attach_args *ia = aux;

	if (pnp)
		printf("%s", pnp);
	if (ia->ia_iosize)
		printf(" port 0x%x", ia->ia_iobase);
	if (ia->ia_iosize > 1)
		printf("/%d", ia->ia_iosize);
	return (UNCONF);
}
