/*	$OpenBSD: lom.c,v 1.4 2009/09/20 21:58:35 kettenis Exp $	*/
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
#include <sys/kernel.h>
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
#define  LOM_IDX_CMD_FAN	0x05

#define LOM_IDX_FW_REV		0x01	/* Firmware revision  */

#define LOM_IDX_FAN1		0x04	/* Fan speed */
#define LOM_IDX_FAN2		0x05
#define LOM_IDX_FAN3		0x06
#define LOM_IDX_FAN4		0x07

#define LOM_IDX_TEMP1		0x18	/* Temperature */
#define LOM_IDX_TEMP2		0x19
#define LOM_IDX_TEMP3		0x1a
#define LOM_IDX_TEMP4		0x1b
#define LOM_IDX_TEMP5		0x1c
#define LOM_IDX_TEMP6		0x1d
#define LOM_IDX_TEMP7		0x1e
#define LOM_IDX_TEMP8		0x1f

#define LOM_IDX_LED1		0x25

#define LOM_IDX_ALARM		0x30

#define LOM_IDX_HOSTNAMELEN	0x38
#define LOM_IDX_HOSTNAME	0x39

#define LOM_IDX_CONFIG		0x5d
#define LOM_IDX_FAN1_CAL	0x5e
#define LOM_IDX_FAN2_CAL	0x5f
#define LOM_IDX_FAN3_CAL	0x60
#define LOM_IDX_FAN4_CAL	0x61
#define LOM_IDX_FAN1_LOW	0x62
#define LOM_IDX_FAN2_LOW	0x63
#define LOM_IDX_FAN3_LOW	0x64
#define LOM_IDX_FAN4_LOW	0x65

#define LOM_IDX_CONFIG2		0x66
#define LOM_IDX_CONFIG3		0x67

#define LOM_IDX_PROBE55		0x7e	/* Always returns 0x55 */
#define LOM_IDX_PROBEAA		0x7f	/* Always returns 0xaa */

#define LOM_IDX4_TEMP_NAME_START	0x40
#define LOM_IDX4_TEMP_NAME_END		0xff

#define LOM_IDX5_FAN_NAME_START		0x40
#define LOM_IDX5_FAN_NAME_END		0xff

#define LOM_MAX_FAN	4
#define LOM_MAX_PSU	3
#define LOM_MAX_TEMP	8

#define LOM_MAX_SENSORS (LOM_MAX_FAN + LOM_MAX_PSU + LOM_MAX_TEMP)

struct lom_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int			sc_space;

	struct ksensor		sc_fan[LOM_MAX_FAN];
	struct ksensor		sc_temp[LOM_MAX_TEMP];
	struct ksensordev	sc_sensordev;

	int			sc_num_fan;
	int			sc_num_psu;
	int			sc_num_temp;

	uint8_t			sc_fan_cal[LOM_MAX_FAN];
	uint8_t			sc_fan_low[LOM_MAX_FAN];

	char			sc_hostname[MAXHOSTNAMELEN];
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
	uint8_t reg, fw_rev, config, config2, config3;
	uint8_t cal, low, len;
	int i;

	sc->sc_iot = ea->ea_memtag;
	if (ebus_bus_map(ea->ea_memtag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[0]),
	    ea->ea_regs[0].size, 0, 0, &sc->sc_ioh)) {
		printf(": can't map register space\n");
                return;
	}

	if (lom_read(sc, LOM_IDX_PROBE55, &reg) || reg != 0x55 ||
	    lom_read(sc, LOM_IDX_PROBEAA, &reg) || reg != 0xaa ||
	    lom_read(sc, LOM_IDX_FW_REV, &fw_rev) ||
	    lom_read(sc, LOM_IDX_CONFIG, &config) ||
	    lom_read(sc, LOM_IDX_CONFIG2, &config2) ||
	    lom_read(sc, LOM_IDX_CONFIG3, &config3))
	{
		printf(": not responding\n");
		return;
	}

	sc->sc_num_fan = min((config >> 5) & 0x7, LOM_MAX_FAN);
	sc->sc_num_psu = min((config >> 3) & 0x3, LOM_MAX_PSU);
	sc->sc_num_temp = min((config2 >> 4) & 0xf, LOM_MAX_TEMP);

	for (i = 0; i < sc->sc_num_fan; i++) {
		if (lom_read(sc, LOM_IDX_FAN1_CAL + i, &cal) ||
		    lom_read(sc, LOM_IDX_FAN1_LOW + i, &low)) {
			printf(": can't read fan information\n");
			return;
		}
		sc->sc_fan_cal[i] = cal;
		sc->sc_fan_low[i] = low;
	}

	/* Initialize sensor data. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	for (i = 0; i < sc->sc_num_fan; i++) {
		sc->sc_fan[i].type = SENSOR_FANRPM;
		sensor_attach(&sc->sc_sensordev, &sc->sc_fan[i]);
	}
	for (i = 0; i < sc->sc_num_temp; i++) {
		sc->sc_temp[i].type = SENSOR_TEMP;
		sensor_attach(&sc->sc_sensordev, &sc->sc_temp[i]);
	}
	if (lom_init_desc(sc)) {
		printf(": can't read sensor names\n");
		return;
	}

	if (sensor_task_register(sc, lom_refresh, 5) == NULL) {
		printf(": unable to register update task\n");
		return;
	}

	sensordev_install(&sc->sc_sensordev);

	/* Read hostname from LOM. */
	lom_read(sc, LOM_IDX_HOSTNAMELEN, &len);
	for (i = 0; i < len; i++) {
		lom_read(sc, LOM_IDX_HOSTNAME, &reg);
		sc->sc_hostname[i] = reg;
	}

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
	int i, j, k;
	int error;

	/*
	 * Read temperature sensor names.
	 */
	error = lom_write(sc, LOM_IDX_CMD, LOM_IDX_CMD_TEMP);
	if (error)
		return (error);

	i = 0;
	j = 0;
	k = LOM_IDX4_TEMP_NAME_START;
	while (k <= LOM_IDX4_TEMP_NAME_END) {
		error = lom_read(sc, k++, &val);
		if (error)
			goto fail;

		if (val == 0xff)
			break;

		if (val == '\0') {
			i++;
			j = 0;
			if (i < sc->sc_num_temp)
				continue;

			break;
		}

		sc->sc_temp[i].desc[j++] = val;
		if (j > sizeof (sc->sc_temp[i].desc) - 1)
			break;
	}

	/*
	 * Read fan names.
	 */
	error = lom_write(sc, LOM_IDX_CMD, LOM_IDX_CMD_FAN);
	if (error)
		return (error);

	i = 0;
	j = 0;
	k = LOM_IDX5_FAN_NAME_START;
	while (k <= LOM_IDX5_FAN_NAME_END) {
		error = lom_read(sc, k++, &val);
		if (error)
			goto fail;

		if (val == 0xff)
			break;

		if (val == '\0') {
			i++;
			j = 0;
			if (i < sc->sc_num_fan)
				continue;

			break;
		}

		sc->sc_fan[i].desc[j++] = val;
		if (j > sizeof (sc->sc_fan[i].desc) - 1)
			break;
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
	int i;

	for (i = 0; i < sc->sc_num_temp; i++) {
		if (lom_read(sc, LOM_IDX_TEMP1 + i, &val)) {
			sc->sc_temp[i].flags |= SENSOR_FINVALID;
			continue;
		}

		sc->sc_temp[i].value = val * 1000000 + 273150000;
		sc->sc_temp[i].flags &= ~SENSOR_FINVALID;
	}

	for (i = 0; i < sc->sc_num_fan; i++) {
		if (lom_read(sc, LOM_IDX_FAN1 + i, &val)) {
			sc->sc_fan[i].flags |= SENSOR_FINVALID;
			continue;
		}

		sc->sc_fan[i].value = (60 * sc->sc_fan_cal[i] * val) / 100;
		sc->sc_fan[i].flags &= ~SENSOR_FINVALID;
	}

	/*
	 * If our hostname is set and differs from what's stored in
	 * the LOM, write the new hostname back to the LOM.  Note that
	 * we include the terminating NUL when writing the hostname
	 * back to the LOM, otherwise the LOM will print any traling
	 * garbage.
	 */
	if (hostnamelen > 0 &&
	    strncmp(sc->sc_hostname, hostname, sizeof(hostname)) != 0) {
		lom_write(sc, LOM_IDX_HOSTNAMELEN, hostnamelen + 1);
		for (i = 0; i < hostnamelen + 1; i++)
			lom_write(sc, LOM_IDX_HOSTNAME, hostname[i]);
		strlcpy(sc->sc_hostname, hostname, sizeof(hostname));
	}
}
