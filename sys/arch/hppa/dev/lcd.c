/*	$OpenBSD: lcd.c,v 1.1 2007/07/15 20:11:12 kettenis Exp $	*/

/*
 * Copyright (c) 2007 Mark Kettenis
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/pdc.h>

#define LCD_CLS		0x01
#define LCD_HOME	0x02
#define LCD_LOCATE(X, Y)	(((Y) & 1 ? 0xc0 : 0x80) | ((X) & 0x0f))

struct lcd_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_cmdh, sc_datah;

	u_int sc_delay;
};

int	lcd_match(struct device *, void *, void *);
void	lcd_attach(struct device *, struct device *, void *);

struct cfattach lcd_ca = {
	sizeof(struct lcd_softc), lcd_match, lcd_attach
};

struct cfdriver lcd_cd = {
	NULL, "lcd", DV_DULL
};

void	lcd_write(struct lcd_softc *, const char *);

int
lcd_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "lcd") == 0)
		return (1);

	return (0);
}

void
lcd_attach(struct device *parent, struct device *self, void *aux)
{
	struct lcd_softc *sc = (struct lcd_softc *)self;
	struct confargs *ca = aux;
	struct pdc_chassis_lcd *pdc_lcd = (void *)ca->ca_pdc_iodc_read;

	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, pdc_lcd->cmd_addr,
		1, 0, &sc->sc_cmdh)) {
		printf(": cannot map cmd register\n");
		return;
	}
		
	if (bus_space_map(sc->sc_iot, pdc_lcd->data_addr,
		1, 0, &sc->sc_datah)) {
		printf(": cannot map data register\n");
		bus_space_unmap(sc->sc_iot, sc->sc_cmdh, 1);
		return;
	}

	sc->sc_delay = pdc_lcd->delay;

	bus_space_write_1(sc->sc_iot, sc->sc_cmdh, 0, LCD_CLS);
	delay(100 * sc->sc_delay);

	bus_space_write_1(sc->sc_iot, sc->sc_cmdh, 0, LCD_LOCATE(0, 0));
	delay(sc->sc_delay);
	lcd_write(sc, "OpenBSD/" MACHINE);

	printf("\n");
}

void
lcd_write(struct lcd_softc *sc, const char *str)
{
	while (*str) {
		bus_space_write_1(sc->sc_iot, sc->sc_datah, 0, *str++);
		delay(sc->sc_delay);
	}
}
