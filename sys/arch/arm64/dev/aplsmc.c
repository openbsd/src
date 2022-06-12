/*	$OpenBSD: aplsmc.c,v 1.12 2022/06/12 16:00:12 kettenis Exp $	*/
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

#include <machine/apmvar.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/clock_subr.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#include <arm64/dev/aplmbox.h>
#include <arm64/dev/rtkit.h>

#include "apm.h"

extern void (*cpuresetfn)(void);
extern void (*powerdownfn)(void);

/* SMC mailbox endpoint */
#define SMC_EP			32

/* SMC commands */
#define SMC_READ_KEY		0x10
#define SMC_WRITE_KEY		0x11
#define SMC_GET_KEY_BY_INDEX	0x12
#define SMC_GET_KEY_INFO	0x13
#define SMC_GET_SRAM_ADDR	0x17
#define  SMC_SRAM_SIZE		0x4000

/* SMC errors */
#define SMC_ERROR(d)		((d) & 0xff)
#define SMC_OK			0x00
#define SMC_KEYNOTFOUND		0x84

/* SMC keys */
#define SMC_KEY(s)	((s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3])

struct smc_key_info {
	uint8_t		size;
	uint8_t		type[4];
	uint8_t		flags;
};

/* SMC GPIO commands */
#define SMC_GPIO_CMD_OUTPUT	(0x01 << 24)

/* RTC related constants */
#define RTC_OFFSET_LEN		6
#define SMC_CLKM_LEN		6

struct aplsmc_sensor {
	const char	*key;
	const char	*key_type;
	enum sensor_type type;
	int		scale;
	const char	*desc;
	int		flags;
};

#define APLSMC_BE		(1 << 0)
#define APLSMC_HIDDEN		(1 << 1)

#define APLSMC_MAX_SENSORS	19

struct aplsmc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_space_handle_t	sc_sram_ioh;

	void			*sc_ih;

	struct rtkit_state	*sc_rs;
	uint8_t			sc_msgid;
	uint64_t		sc_data;

	struct gpio_controller	sc_gc;

	int			sc_rtc_node;
	struct todr_chip_handle sc_todr;

	int			sc_reboot_node;

	struct aplsmc_sensor	*sc_smcsensors[APLSMC_MAX_SENSORS];
	struct ksensor		sc_sensors[APLSMC_MAX_SENSORS];
	int			sc_nsensors;
	struct ksensordev	sc_sensordev;
};

struct aplsmc_softc *aplsmc_sc;

#ifndef SMALL_KERNEL

struct aplsmc_sensor aplsmc_sensors[] = {
	{ "ACDI", "ui16", SENSOR_INDICATOR, 1, "power supply" },
	{ "B0RM", "ui16", SENSOR_AMPHOUR, 1000, "remaining battery capacity",
	  APLSMC_BE },
	{ "B0FC", "ui16", SENSOR_AMPHOUR, 1000, "last full battery capacity" },
	{ "B0DC", "ui16", SENSOR_AMPHOUR, 1000, "battery design capacity" },
	{ "B0AV", "ui16", SENSOR_VOLTS_DC, 1000, "battery" },
	{ "B0CT", "ui16", SENSOR_INTEGER, 1, "battery discharge cycles" },
	{ "B0TF", "ui16", SENSOR_INTEGER, 1, "battery time-to-full",
	  APLSMC_HIDDEN },
	{ "B0TE", "ui16", SENSOR_INTEGER, 1, "battery time-to-empty",
	  APLSMC_HIDDEN },
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

#endif

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
int	aplsmc_apminfo(struct apm_power_info *);
void	aplsmc_set_pin(void *, uint32_t *, int);
int	aplsmc_gettime(struct todr_chip_handle *, struct timeval *);
int	aplsmc_settime(struct todr_chip_handle *, struct timeval *);
void	aplsmc_reset(void);
void	aplsmc_powerdown(void);
void	aplsmc_reboot_attachhook(struct device *);

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
	uint8_t data[SMC_CLKM_LEN];
	int error, node;
#ifndef SMALL_KERNEL
	int i;
#endif

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

	sc->sc_rs = rtkit_init(faa->fa_node, NULL, NULL);
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

	aplsmc_sc = sc;

	node = OF_getnodebyname(faa->fa_node, "gpio");
	if (node) {
		sc->sc_gc.gc_node = node;
		sc->sc_gc.gc_cookie = sc;
		sc->sc_gc.gc_set_pin = aplsmc_set_pin;
		gpio_controller_register(&sc->sc_gc);
	}

	/*
	 * Only provide TODR implementation if the "CLKM" key is
	 * supported by the SMC firmware.
	 */
	error = aplsmc_read_key(sc, SMC_KEY("CLKM"), &data, SMC_CLKM_LEN);
	node = OF_getnodebyname(faa->fa_node, "rtc");
	if (node && error == 0) {
		sc->sc_rtc_node = node;
		sc->sc_todr.cookie = sc;
		sc->sc_todr.todr_gettime = aplsmc_gettime;
		sc->sc_todr.todr_settime = aplsmc_settime;
		todr_attach(&sc->sc_todr);
	}

	node = OF_getnodebyname(faa->fa_node, "reboot");
	if (node) {
		sc->sc_reboot_node = node;
		cpuresetfn = aplsmc_reset;
		powerdownfn = aplsmc_powerdown;
		config_mountroot(self, aplsmc_reboot_attachhook);
	}

#ifndef SMALL_KERNEL

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
		if (!(aplsmc_sensors[i].flags & APLSMC_HIDDEN)) {
			sensor_attach(&sc->sc_sensordev,
			    &sc->sc_sensors[sc->sc_nsensors]);
		}
		sc->sc_nsensors++;
	}

	aplsmc_refresh_sensors(sc);

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	sensordev_install(&sc->sc_sensordev);
	sensor_task_register(sc, aplsmc_refresh_sensors, 5);

#if NAPM > 0
	apm_setinfohook(aplsmc_apminfo);
#endif

#endif
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
	switch (SMC_ERROR(sc->sc_data)) {
	case SMC_OK:
		break;
	case SMC_KEYNOTFOUND:
		return EINVAL;
		break;
	default:
		return EIO;
		break;
	}

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

int
aplsmc_write_key(struct aplsmc_softc *sc, uint32_t key, void *data, size_t len)
{
	bus_space_write_region_1(sc->sc_iot, sc->sc_sram_ioh, 0, data, len);
	bus_space_barrier(sc->sc_iot, sc->sc_sram_ioh, 0, len,
	    BUS_SPACE_BARRIER_WRITE);
	aplsmc_send_cmd(sc, SMC_WRITE_KEY, key, len);
	return aplsmc_wait_cmd(sc);
}

#ifndef SMALL_KERNEL

void
aplsmc_refresh_sensors(void *arg)
{
	extern int hw_power;
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
			sc->sc_sensors[i].flags |= SENSOR_FUNKNOWN;
		} else {
			sc->sc_sensors[i].flags &= ~SENSOR_FUNKNOWN;
			sc->sc_sensors[i].value = value;
		}

		if (strcmp(sensor->key, "ACDI") == 0)
			hw_power = (value > 0);
	}
}

#if NAPM > 0

int
aplsmc_apminfo(struct apm_power_info *info)
{
	struct aplsmc_sensor *sensor;
	struct ksensor *ksensor;
	struct aplsmc_softc *sc = aplsmc_sc; 
	int remaining = -1, capacity = -1, i;

	info->battery_state = APM_BATT_UNKNOWN;
	info->ac_state = APM_AC_UNKNOWN;
	info->battery_life = 0;
	info->minutes_left = -1;

	for (i = 0; i < sc->sc_nsensors; i++) {
		sensor = sc->sc_smcsensors[i];
		ksensor = &sc->sc_sensors[i];

		if (ksensor->flags & SENSOR_FUNKNOWN)
			continue;

		if (strcmp(sensor->key, "ACDI") == 0) {
			info->ac_state = ksensor->value ?
				APM_AC_ON : APM_AC_OFF;
		} else if (strcmp(sensor->key, "B0RM") == 0)
			remaining = ksensor->value;
		else if (strcmp(sensor->key, "B0FC") == 0)
			capacity = ksensor->value;
		else if ((strcmp(sensor->key, "B0TE") == 0) &&
			 (ksensor->value != 0xffff))
			info->minutes_left = ksensor->value;
		else if ((strcmp(sensor->key, "B0TF") == 0) &&
			 (ksensor->value != 0xffff)) {
			info->battery_state = APM_BATT_CHARGING;
			info->minutes_left = ksensor->value;
		}
	}

	/* calculate remaining battery if we have sane values */
	if (remaining > -1 && capacity > 0) {
		info->battery_life = ((remaining * 100) / capacity);
		if (info->battery_state != APM_BATT_CHARGING) {
			if (info->battery_life > 50)
				info->battery_state = APM_BATT_HIGH;
			else if (info->battery_life > 25)
				info->battery_state = APM_BATT_LOW;
			else
				info->battery_state = APM_BATT_CRITICAL;
		}
	}

	return 0;
}

#endif
#endif

void
aplsmc_set_pin(void *cookie, uint32_t *cells, int val)
{
	struct aplsmc_softc *sc = cookie;
	static char *digits = "0123456789abcdef";
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];
	uint32_t key = SMC_KEY("gP\0\0");
	uint32_t data;

	KASSERT(pin < 256);

	key |= (digits[(pin >> 0) & 0xf] << 0);
	key |= (digits[(pin >> 4) & 0xf] << 8);

	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	data = SMC_GPIO_CMD_OUTPUT | !!val;

	aplsmc_write_key(sc, key, &data, sizeof(data));
}

int
aplsmc_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct aplsmc_softc *sc = handle->cookie;
	uint8_t data[8] = {};
	uint64_t offset, time;
	int error;

	error = nvmem_read_cell(sc->sc_rtc_node, "rtc_offset", &data,
	    RTC_OFFSET_LEN);
	if (error)
		return error;
	offset = lemtoh64(data);

	error = aplsmc_read_key(sc, SMC_KEY("CLKM"), &data, SMC_CLKM_LEN);
	if (error)
		return error;
	time = lemtoh64(data) + offset;

	tv->tv_sec = (time >> 15);
	tv->tv_usec = (((time & 0x7fff) * 1000000) >> 15);
	return 0;
}

int
aplsmc_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct aplsmc_softc *sc = handle->cookie;
	uint8_t data[8] = {};
	uint64_t offset, time;
	int error;

	error = aplsmc_read_key(sc, SMC_KEY("CLKM"), &data, SMC_CLKM_LEN);
	if (error)
		return error;

	time = ((uint64_t)tv->tv_sec << 15);
	time |= ((uint64_t)tv->tv_usec << 15) / 1000000;
	offset = time - lemtoh64(data);

	htolem64(data, offset);
	return nvmem_write_cell(sc->sc_rtc_node, "rtc_offset", &data,
	    RTC_OFFSET_LEN);
}

void
aplsmc_reboot_attachhook(struct device *self)
{
	struct aplsmc_softc *sc = (struct aplsmc_softc *)self;
	uint8_t count = 0;

	/* Reset error counters. */
	nvmem_write_cell(sc->sc_reboot_node, "boot_error_count",
	    &count, sizeof(count));
	nvmem_write_cell(sc->sc_reboot_node, "panic_count",
	    &count, sizeof(count));
}

void
aplsmc_reset(void)
{
	struct aplsmc_softc *sc = aplsmc_sc;
	uint32_t key = SMC_KEY("MBSE");
	uint32_t rest = SMC_KEY("rest");
	uint32_t phra = SMC_KEY("phra");
	uint8_t boot_stage = 0;

	aplsmc_write_key(sc, key, &rest, sizeof(rest));
	nvmem_write_cell(sc->sc_reboot_node, "boot_stage",
	    &boot_stage, sizeof(boot_stage));
	aplsmc_write_key(sc, key, &phra, sizeof(phra));
}

void
aplsmc_powerdown(void)
{
	struct aplsmc_softc *sc = aplsmc_sc;
	uint32_t key = SMC_KEY("MBSE");
	uint32_t offw = SMC_KEY("offw");
	uint32_t off1 = SMC_KEY("off1");
	uint8_t boot_stage = 0;
	uint8_t shutdown_flag = 1;

	aplsmc_write_key(sc, key, &offw, sizeof(offw));
	nvmem_write_cell(sc->sc_reboot_node, "boot_stage",
	    &boot_stage, sizeof(boot_stage));
	nvmem_write_cell(sc->sc_reboot_node, "shutdown_flag",
	    &shutdown_flag, sizeof(shutdown_flag));
	aplsmc_write_key(sc, key, &off1, sizeof(off1));
}
