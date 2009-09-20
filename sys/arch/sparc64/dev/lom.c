/*	$OpenBSD: lom.c,v 1.2 2009/09/20 19:31:57 kettenis Exp $	*/
/*
 * Copyright (c) 2009 Mark Kettenis
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
#include <sys/sensors.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>

/*
 * The LOM is implemented as a H8/3437 microcontroller which has its
 * on-chip host interface hooked up to EBus.
 */
#define LOM_DATA		0x00	/* R/W */
#define LOM_CMD			0x01	/* W */
#define LOM_STATUS		0x01	/* R */
#define  LOM_STATUS_OBF		0x01	/* Output Buffer Full */
#define  LOM_STATUS_IBF		0x02	/* Input Buffer Full  */

#define LOM_IDX_CMD		0x00
#define  LOM_IDX_CMD_GENERIC	0x00
#define  LOM_IDX_CMD_TEMP	0x04

#define LOM_IDX_FW_REV		0x01	/* Firmware revision  */

#define LOM_IDX_TEMP1		0x18	/* Temperature */

#define LOM_IDX_LED1		0x25

#define LOM_IDX_ALARM		0x30

#define LOM_IDX_PROBE55		0x7e	/* Always returns 0x55 */
#define LOM_IDX_PROBEAA		0x7f	/* Always returns 0xaa */

#define LOM_IDX4_TEMP_NAME_START	0x40
#define LOM_IDX4_TEMP_NAME_END		0xff

struct lom_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int			sc_space;

	struct ksensor		sc_sensor;
	struct ksensordev	sc_sensordev;
};

int	lom_match(struct device *, void *, void *);
void	lom_attach(struct device *, struct device *, void *);

struct cfattach lom_ca = {
	sizeof(struct lom_softc), lom_match, lom_attach
};

struct cfdriver lom_cd = {
	NULL, "lom", DV_DULL
};

int	lom_read(struct lom_softc *, uint8_t, uint8_t *);
int	lom_write(struct lom_softc *, uint8_t, uint8_t);

int	lom_init_desc(struct lom_softc *sc);
void	lom_refresh(void *);

int
lom_match(struct device *parent, void *match, void *aux)
{
	struct ebus_attach_args *ea = aux;

	if (strcmp(ea->ea_name, "SUNW,lomh") == 0)
		return (1);

	return (0);
}

void
lom_attach(struct device *parent, struct device *self, void *aux)
{
	struct lom_softc *sc = (void *)self;
	struct ebus_attach_args *ea = aux;
	uint8_t reg, fw_rev;

	sc->sc_iot = ea->ea_memtag;
	if (ebus_bus_map(ea->ea_memtag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[0]),
	    ea->ea_regs[0].size, 0, 0, &sc->sc_ioh)) {
		printf(": can't map register space\n");
                return;
	}

	if (lom_read(sc, LOM_IDX_PROBE55, &reg) || reg != 0x55 ||
	    lom_read(sc, LOM_IDX_PROBEAA, &reg) || reg != 0xaa ||
	    lom_read(sc, LOM_IDX_FW_REV, &fw_rev)) {
		printf(": not responding\n");
		return;
	}

	/* Initialize sensor data. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	sc->sc_sensor.type = SENSOR_TEMP;
	if (lom_init_desc(sc)) {
		printf(": can't read sensor names\n");
		return;
	}

	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor);

	if (sensor_task_register(sc, lom_refresh, 5) == NULL) {
		printf(": unable to register update task\n");
		return;
	}

	sensordev_install(&sc->sc_sensordev);

	printf(": rev %d.%d\n", fw_rev >> 4, fw_rev & 0x0f);
}

int
lom_read(struct lom_softc *sc, uint8_t reg, uint8_t *val)
{
	uint8_t str;
	int i;

	/* Wait for input buffer to become available. */
	for (i = 1000; i > 0; i--) {
		str = bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_STATUS);
		delay(10);
		if ((str & LOM_STATUS_IBF) == 0)
			break;
	}
	if (i == 0)
		return (ETIMEDOUT);

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LOM_CMD, reg);

	/* Wait until the microcontroller fills output buffer. */
	for (i = 1000; i > 0; i--) {
		str = bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_STATUS);
		delay(10);
		if (str & LOM_STATUS_OBF)
			break;
	}
	if (i == 0)
		return (ETIMEDOUT);

	*val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_DATA);
	return (0);
}

int
lom_write(struct lom_softc *sc, uint8_t reg, uint8_t val)
{
	uint8_t str;
	int i;

	/* Wait for input buffer to become available. */
	for (i = 1000; i > 0; i--) {
		str = bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_STATUS);
		delay(10);
		if ((str & LOM_STATUS_IBF) == 0)
			break;
	}
	if (i == 0)
		return (ETIMEDOUT);

	if (sc->sc_space == LOM_IDX_CMD_GENERIC && reg != LOM_IDX_CMD)
		reg |= 0x80;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LOM_CMD, reg);

	/* Wait until the microcontroller fills output buffer. */
	for (i = 1000; i > 0; i--) {
		str = bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_STATUS);
		delay(10);
		if (str & LOM_STATUS_OBF)
			break;
	}
	if (i == 0)
		return (ETIMEDOUT);

	bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_DATA);

	/* Wait for input buffer to become available. */
	for (i = 1000; i > 0; i--) {
		str = bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_STATUS);
		delay(10);
		if ((str & LOM_STATUS_IBF) == 0)
			break;
	}
	if (i == 0)
		return (ETIMEDOUT);

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LOM_DATA, val);

	/* Wait until the microcontroller fills output buffer. */
	for (i = 1000; i > 0; i--) {
		str = bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_STATUS);
		delay(10);
		if (str & LOM_STATUS_OBF)
			break;
	}
	if (i == 0)
		return (ETIMEDOUT);

	bus_space_read_1(sc->sc_iot, sc->sc_ioh, LOM_DATA);

	/* If we switched spaces, remember the one we're in now. */
	if (reg == LOM_IDX_CMD)
		sc->sc_space = val;

	return (0);
}

int
lom_init_desc(struct lom_softc *sc)
{
	uint8_t val;
	int i, j, error;

	error = lom_write(sc, LOM_IDX_CMD, LOM_IDX_CMD_TEMP);
	if (error)
		return (error);

	i = LOM_IDX4_TEMP_NAME_START, j = 0;
	while (i <= LOM_IDX4_TEMP_NAME_END) {
		error = lom_read(sc, i, &val);
		if (error)
			goto fail;

		if (val == 0xff)
			break;

		sc->sc_sensor.desc[j++] = val;
		if (j > sizeof (sc->sc_sensor.desc) - 1)
			break;
		i++;
	}

fail:
	lom_write(sc, LOM_IDX_CMD, LOM_IDX_CMD_GENERIC);
	return (error);
}

void
lom_refresh(void *arg)
{
	struct lom_softc *sc = arg;
	uint8_t val;

	if (lom_read(sc, LOM_IDX_TEMP1, &val)) {
		sc->sc_sensor.flags |= SENSOR_FINVALID;
		return;
	}

	sc->sc_sensor.value = val * 1000000 + 273150000;
	sc->sc_sensor.flags &= ~SENSOR_FINVALID;
}
