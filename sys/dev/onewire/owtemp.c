/*	$OpenBSD: owtemp.c,v 1.1 2006/03/04 16:27:03 grange Exp $	*/

/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
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
 * 1-Wire temperature family type device driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sensors.h>

#include <dev/onewire/onewiredevs.h>
#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#define DS1920_CMD_CONVERT		0x44
#define DS1920_CMD_READ_SCRATCHPAD	0xbe

struct owtemp_softc {
	struct device		sc_dev;

	void *			sc_onewire;
	u_int64_t		sc_rom;

	struct sensor		sc_sensor;

	int			sc_dying;
};

int	owtemp_match(struct device *, void *, void *);
void	owtemp_attach(struct device *, struct device *, void *);
int	owtemp_detach(struct device *, int);
int	owtemp_activate(struct device *, enum devact);

void	owtemp_update(void *);

struct cfattach owtemp_ca = {
	sizeof(struct owtemp_softc),
	owtemp_match,
	owtemp_attach,
	owtemp_detach,
	owtemp_activate
};

struct cfdriver owtemp_cd = {
	NULL, "owtemp", DV_DULL
};

static const struct onewire_matchfam owtemp_fams[] = {
	{ ONEWIRE_FAMILY_DS1920 }
};

int
owtemp_match(struct device *parent, void *match, void *aux)
{
	return (onewire_matchbyfam(aux, owtemp_fams,
	    sizeof(owtemp_fams) /sizeof(owtemp_fams[0])));
}

void
owtemp_attach(struct device *parent, struct device *self, void *aux)
{
	struct owtemp_softc *sc = (struct owtemp_softc *)self;
	struct onewire_attach_args *oa = aux;

	sc->sc_onewire = oa->oa_onewire;
	sc->sc_rom = oa->oa_rom;

	/* Initialize sensor */
	strlcpy(sc->sc_sensor.device, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensor.device));
	sc->sc_sensor.type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor.desc, "Temp", sizeof(sc->sc_sensor.desc));

	if (sensor_task_register(sc, owtemp_update, 5)) {
		printf(": unable to register update task\n");
		return;
	}
	sensor_add(&sc->sc_sensor);

	printf("\n");
}

int
owtemp_detach(struct device *self, int flags)
{
	struct owtemp_softc *sc = (struct owtemp_softc *)self;

	sensor_del(&sc->sc_sensor);
	sensor_task_unregister(sc);

	return (0);
}

int
owtemp_activate(struct device *self, enum devact act)
{
	struct owtemp_softc *sc = (struct owtemp_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}

	return (0);
}

void
owtemp_update(void *arg)
{
	struct owtemp_softc *sc = arg;
	u_int8_t data[9];

	onewire_lock(sc->sc_onewire, 0);
	if (onewire_reset(sc->sc_onewire) != 0)
		goto done;
	onewire_matchrom(sc->sc_onewire, sc->sc_rom);

	/*
	 * Start temperature conversion. The conversion takes up to 750ms.
	 * After sending the command, the data line must be held high for
	 * at least 750ms to provide power during the conversion process.
	 * As such, no other activity may take place on the 1-Wire bus for
	 * at least this period.
	 */
	onewire_write_byte(sc->sc_onewire, DS1920_CMD_CONVERT);
	tsleep(sc, PRIBIO, "owtemp", hz);

	if (onewire_reset(sc->sc_onewire) != 0)
		goto done;
	onewire_matchrom(sc->sc_onewire, sc->sc_rom);

	/*
	 * The result of the temperature measurement is placed in the
	 * first two bytes of the scratchpad.
	 */
	onewire_write_byte(sc->sc_onewire, DS1920_CMD_READ_SCRATCHPAD);
	onewire_read_block(sc->sc_onewire, data, 9);
	if (onewire_crc(data, 8) == data[8]) {
		sc->sc_sensor.value = 273150000 +
		    (int)((u_int16_t)data[1] << 8 | data[0]) * 500000;
	}

done:
	onewire_unlock(sc->sc_onewire);
}
