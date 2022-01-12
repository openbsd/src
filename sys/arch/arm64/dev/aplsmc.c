/*	$OpenBSD: aplsmc.c,v 1.2 2022/01/12 01:19:24 jsg Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sensors.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#include <arm64/dev/aplmbox.h>
#include <arm64/dev/rtkit.h>

#define SMC_EP			32

#define SMC_READ_KEY		0x10
#define SMC_WRITE_KEY		0x11
#define SMC_GET_KEY_BY_INDEX	0x12
#define SMC_GET_KEY_INFO	0x13
#define SMC_GET_SRAM_ADDR	0x17

#define SMC_ERROR(d)		((d) & 0xff)
#define SMC_OK			0x00
#define SMC_KEYNOTFOUND		0x84

#define SMC_SRAM_SIZE		0x4000

#define SMC_KEY(s)	((s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3])

struct smc_key_info {
	uint8_t		size;
	uint8_t		type[4];
	uint8_t		flags;
};

struct aplsmc_sensor {
	const char	*key;
	const char	*key_type;
	enum sensor_type type;
	int		scale;
	const char	*desc;
	int		flags;
};

#define APLSMC_BE		(1 << 0)

#define APLSMC_MAX_SENSORS	16

struct aplsmc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_space_handle_t	sc_sram_ioh;

	void			*sc_ih;

	struct rtkit_state	*sc_rs;
	uint8_t			sc_msgid;
	uint64_t		sc_data;

	struct aplsmc_sensor	*sc_smcsensors[APLSMC_MAX_SENSORS];
	struct ksensor		sc_sensors[APLSMC_MAX_SENSORS];
	int			sc_nsensors;
	struct ksensordev	sc_sensordev;
};

struct aplsmc_sensor aplsmc_sensors[] = {
	{ "B0RM", "ui16", SENSOR_AMPHOUR, 1000, "remaining battery capacity",
	  APLSMC_BE },
	{ "B0FC", "ui16", SENSOR_AMPHOUR, 1000, "last full battery capacity" },
	{ "B0DC", "ui16", SENSOR_AMPHOUR, 1000, "battery design capacity" },
	{ "B0AV", "ui16", SENSOR_VOLTS_DC, 1000, "battery" },
	{ "B0CT", "ui16", SENSOR_INTEGER, 1, "battery discharge cycles" },
	{ "F0Ac", "flt ", SENSOR_FANRPM, 1, "" },
	{ "ID0R", "flt ", SENSOR_AMPS, 1000000, "input" },
	{ "PDTR", "flt ", SENSOR_WATTS, 1000000, "input" },
	{ "PSTR", "flt ", SENSOR_WATTS, 1000000, "system" },
	{ "TB0T", "flt ", SENSOR_TEMP, 1000000, "battery" },
	{ "TCHP", "flt ", SENSOR_TEMP, 1000000, "charger" },
	{ "TW0P", "flt ", SENSOR_TEMP, 1000000, "wireless" },
	{ "Ts0P", "flt ", SENSOR_TEMP, 1000000, "palm rest" },
	{ "Ts1P", "flt ", SENSOR_TEMP, 1000000, "palm rest" },
	{ "VD0R", "flt ", SENSOR_VOLTS_DC, 1000000, "input" },
};

int	aplsmc_match(struct device *, void *, void *);
void	aplsmc_attach(struct device *, struct device *, void *);

const struct cfattach aplsmc_ca = {
	sizeof (struct aplsmc_softc), aplsmc_match, aplsmc_attach
};

struct cfdriver aplsmc_cd = {
	NULL, "aplsmc", DV_DULL
};

void	aplsmc_callback(void *, uint64_t);
int	aplsmc_send_cmd(struct aplsmc_softc *, uint16_t, uint32_t, uint16_t);
int	aplsmc_wait_cmd(struct aplsmc_softc *sc);
int	aplsmc_read_key(struct aplsmc_softc *, uint32_t, void *, size_t);
void	aplsmc_refresh_sensors(void *);

int
aplsmc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,smc");
}

void
aplsmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplsmc_softc *sc = (struct aplsmc_softc *)self;
	struct fdt_attach_args *faa = aux;
	int error;
	int i;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_rs = rtkit_init(faa->fa_node, NULL);
	if (sc->sc_rs == NULL) {
		printf(": can't map mailbox channel\n");
		return;
	}

	error = rtkit_boot(sc->sc_rs);
	if (error) {
		printf(": can't boot firmware\n");
		return;
	}

	error = rtkit_start_endpoint(sc->sc_rs, SMC_EP, aplsmc_callback, sc);
	if (error) {
		printf(": can't start SMC endpoint\n");
		return;
	}

	aplsmc_send_cmd(sc, SMC_GET_SRAM_ADDR, 0, 0);
	error = aplsmc_wait_cmd(sc);
	if (error) {
		printf(": can't get SRAM address\n");
		return;
	}

	if (bus_space_map(sc->sc_iot, sc->sc_data,
	    SMC_SRAM_SIZE, 0, &sc->sc_sram_ioh)) {
		printf(": can't map SRAM\n");
		return;
	}

	printf("\n");

	for (i = 0; i < nitems(aplsmc_sensors); i++) {
		struct smc_key_info info;

		aplsmc_send_cmd(sc, SMC_GET_KEY_INFO,
		    SMC_KEY(aplsmc_sensors[i].key), 0);
		error = aplsmc_wait_cmd(sc);
		if (error || SMC_ERROR(sc->sc_data) != SMC_OK)
			continue;

		bus_space_read_region_1(sc->sc_iot, sc->sc_sram_ioh, 0,
		    (uint8_t *)&info, sizeof(info));

		/* Skip if the key type doesn't match. */
		if (memcmp(aplsmc_sensors[i].key_type, info.type,
		    sizeof(info.type)) != 0)
			continue;

		if (sc->sc_nsensors >= APLSMC_MAX_SENSORS) {
			printf("%s: maximum number of sensors exceeded\n",
			    sc->sc_dev.dv_xname);
			break;
		}

		sc->sc_smcsensors[sc->sc_nsensors] = &aplsmc_sensors[i];
		strlcpy(sc->sc_sensors[sc->sc_nsensors].desc,
		    aplsmc_sensors[i].desc, sizeof(sc->sc_sensors[0].desc));
		sc->sc_sensors[sc->sc_nsensors].type = aplsmc_sensors[i].type;
		sensor_attach(&sc->sc_sensordev,
		    &sc->sc_sensors[sc->sc_nsensors]);
		sc->sc_nsensors++;
	}

	aplsmc_refresh_sensors(sc);

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	sensordev_install(&sc->sc_sensordev);
	sensor_task_register(sc, aplsmc_refresh_sensors, 5);
}

void
aplsmc_callback(void *arg, uint64_t data)
{
	struct aplsmc_softc *sc = arg;

	sc->sc_data = data;
	wakeup(&sc->sc_data);
}

int
aplsmc_send_cmd(struct aplsmc_softc *sc, uint16_t cmd, uint32_t key,
    uint16_t len)
{
	uint64_t data;

	data = cmd;
	data |= (uint64_t)len << 16;
	data |= (uint64_t)key << 32;
	data |= (sc->sc_msgid++ & 0xf) << 12;

	return rtkit_send_endpoint(sc->sc_rs, SMC_EP, data);
}

int
aplsmc_wait_cmd(struct aplsmc_softc *sc)
{
	if (cold) {
		int error, timo;

		/* Poll for completion. */
		for (timo = 1000; timo > 0; timo--) {
			error = rtkit_poll(sc->sc_rs);
			if (error == 0)
				return 0;
			delay(10);
		}

		return EWOULDBLOCK;
	}

	/* Sleep until the callback wakes us up. */
	return tsleep_nsec(&sc->sc_data, PWAIT, "apsmc", 10000000);
}

int
aplsmc_read_key(struct aplsmc_softc *sc, uint32_t key, void *data, size_t len)
{
	int error;

	aplsmc_send_cmd(sc, SMC_READ_KEY, key, len);
	error = aplsmc_wait_cmd(sc);
	if (error)
		return error;

	len = min(len, (sc->sc_data >> 16) & 0xffff);
	if (len > sizeof(uint32_t)) {
		bus_space_read_region_1(sc->sc_iot, sc->sc_sram_ioh, 0,
		    data, len);
	} else {
		uint32_t tmp = (sc->sc_data >> 32);
		memcpy(data, &tmp, len);
	}

	return 0;
}

void
aplsmc_refresh_sensors(void *arg)
{
	struct aplsmc_softc *sc = arg;
	struct aplsmc_sensor *sensor;
	int64_t value;
	uint32_t key;
	int i, error;

	for (i = 0; i < sc->sc_nsensors; i++) {
		sensor = sc->sc_smcsensors[i];
		key = SMC_KEY(sensor->key);

		if (strcmp(sensor->key_type, "ui8 ") == 0) {
			uint8_t ui8;

			error = aplsmc_read_key(sc, key, &ui8, sizeof(ui8));
			value = (int64_t)ui8 * sensor->scale;
		} else if (strcmp(sensor->key_type, "ui16") == 0) {
			uint16_t ui16;

			error = aplsmc_read_key(sc, key, &ui16, sizeof(ui16));
			if (sensor->flags & APLSMC_BE)
				ui16 = betoh16(ui16);
			value = (int64_t)ui16 * sensor->scale;
		} else if (strcmp(sensor->key_type, "flt ") == 0) {
			uint32_t flt;
			int64_t mant;
			int sign, exp;

			error = aplsmc_read_key(sc, key, &flt, sizeof(flt));
			if (sensor->flags & APLSMC_BE)
				flt = betoh32(flt);

			/*
			 * Convert floating-point to integer, trying
			 * to keep as much resolution as possible
			 * given the scaling factor for this sensor.
			 */
			sign = (flt >> 31) ? -1 : 1;
			exp = ((flt >> 23) & 0xff) - 127;
			mant = (flt & 0x7fffff) | 0x800000;
			mant *= sensor->scale;
			if (exp < 23)
				value = sign * (mant >> (23 - exp));
			else
				value = sign * (mant << (exp - 23));
		}

		/* Apple reports temperatures in degC. */
		if (sensor->type == SENSOR_TEMP)
			value += 273150000;

		if (error) {
			sc->sc_sensors[0].flags |= SENSOR_FUNKNOWN;
		} else {
			sc->sc_sensors[i].flags &= ~SENSOR_FUNKNOWN;
			sc->sc_sensors[i].value = value;
		}
	}
}
