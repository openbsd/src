/*	$OpenBSD: sambat.c,v 1.3 2026/05/23 11:10:57 mglocker Exp $ */

/*
 * Copyright (c) 2026 Marcus Glocker <mglocker@openbsd.org>
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
 * Battery monitor for Samsung laptops with the "SAM060B" embedded
 * controller (ENE KB9058 silicon with Samsung firmware).  First
 * confirmed on the Galaxy Book4 Edge; the same EC personality is
 * likely shared across the Galaxy Book line.
 *
 * The host talks to the EC via a vendor "Mbox" command protocol on
 * I2C, reverse-engineered for Linux by the Saddytech project; this
 * driver is an independent reimplementation against that reference:
 *   https://github.com/Saddytech/Galaxy-Book4-Edge-linux
 *
 * EC register layout (from the DSDT _SB.ECTC region the Windows
 * driver writes to):
 *   0x80  bit0 = battery present, bit2 = AC online
 *   0x84  bit0 = discharging, bit1 = charging, bit3 = full
 *   0xA0  upper big-endian word = remaining capacity (mAh)
 *   0xA4  upper BE = voltage (mV), lower BE = signed current (mA)
 *   0xB0  lower BE = design capacity (mAh), upper BE = full-charge (mAh)
 *   0xB4  lower BE = design voltage (mV)
 *   0xD0  BE 16-bit  = discharge cycle count
 *
 * Each EC register read requires three Mbox transactions:
 *   write target address (cmd 0xF480 = reg)
 *   write execute       (cmd 0xFF10 = 0x88 "read")
 *   read result         (cmd 0xF480)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sensors.h>

#include <machine/apmvar.h>

#include <dev/i2c/i2cvar.h>

#include "apm.h"

/* Mbox framing. */
#define SAM_MBOX_WRITE_PREFIX	0x40
#define SAM_MBOX_READ_PREFIX	0x30
#define SAM_MBOX_READ_SUCCESS	0x50

/* Mbox command address (the "hi"/"lo" pair). */
#define SAM_CMD_TARGET_HI	0xf4
#define SAM_CMD_TARGET_LO	0x80
#define SAM_CMD_EXEC_HI		0xff
#define SAM_CMD_EXEC_LO		0x10
#define SAM_EXEC_READ		0x88

/* EC register offsets. */
#define SAM_REG_FLAGS		0x80
#define SAM_REG_B1ST		0x84
#define SAM_REG_B1RR		0xa0
#define SAM_REG_B1PV		0xa4
#define SAM_REG_B1AF		0xb0
#define SAM_REG_B1VL		0xb4
#define SAM_REG_CYLC		0xd0

#define SAM_FLAG_B1EX		(1 << 0)
#define SAM_FLAG_ACEX		(1 << 2)

#define SAM_B1ST_DISCHARGE	(1 << 0)
#define SAM_B1ST_CHARGE		(1 << 1)
#define SAM_B1ST_FULL		(1 << 3)

/*
 * Poll the EC every 30 seconds.  The keyboard shares the same EC
 * chain on I2C, so each poll briefly stalls keyboard input; a
 * 30 second interval keeps the stall infrequent enough to be
 * invisible during typing while still being responsive enough
 * for battery reporting.
 */
#define SAM_REFRESH_INTERVAL	30	/* seconds */

/* Sensor indices. */
enum sambat_sensors {
	SAMBAT_SENSOR_CHARGE,		/* percent */
	SAMBAT_SENSOR_VOLT_NOW,		/* current voltage */
	SAMBAT_SENSOR_VOLT_DESIGN,	/* design voltage */
	SAMBAT_SENSOR_CURRENT,		/* signed current */
	SAMBAT_SENSOR_CHARGE_NOW,	/* remaining capacity (Ah) */
	SAMBAT_SENSOR_CHARGE_FULL,	/* full-charge capacity */
	SAMBAT_SENSOR_CHARGE_DESIGN,	/* design capacity */
	SAMBAT_SENSOR_CYCLES,		/* discharge cycle count */
	SAMBAT_SENSOR_STATE,		/* drive state */
	SAMBAT_NSENSORS
};

struct sambat_softc {
	struct device		sc_dev;
	i2c_tag_t		sc_tag;
	int			sc_addr;

	struct ksensor		sc_sensor[SAMBAT_NSENSORS];
	struct ksensordev	sc_sensordev;

	/* Latest decoded readings. */
	int			sc_have_data;
	int			sc_present;
	int			sc_ac_online;
	uint8_t			sc_b1st;
	uint16_t		sc_remaining_mah;
	uint16_t		sc_voltage_mv;
	int16_t			sc_current_ma;
	uint16_t		sc_design_mah;
	uint16_t		sc_fullchg_mah;
	uint16_t		sc_design_mv;
};

struct sambat_softc *sambat_sc;

int	sambat_match(struct device *, void *, void *);
void	sambat_attach(struct device *, struct device *, void *);

int	sambat_mbox_write(struct sambat_softc *, uint8_t, uint8_t, uint8_t);
int	sambat_mbox_read(struct sambat_softc *, uint8_t, uint8_t, uint8_t *);
int	sambat_ec_read_byte(struct sambat_softc *, uint8_t, uint8_t *);
int	sambat_ec_read_block(struct sambat_softc *, uint8_t, uint8_t *, int);

void	sambat_refresh(void *);
int	sambat_apminfo(struct apm_power_info *);

const struct cfattach sambat_ca = {
	sizeof(struct sambat_softc), sambat_match, sambat_attach
};

struct cfdriver sambat_cd = {
	NULL, "sambat", DV_DULL
};

int
sambat_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "samsung,galaxybook-battery") == 0)
		return 1;

	return 0;
}

void
sambat_attach(struct device *parent, struct device *self, void *aux)
{
	struct sambat_softc *sc = (struct sambat_softc *)self;
	struct i2c_attach_args *ia = aux;
	uint8_t probe;
	int i;

	sambat_sc = sc;
	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	/*
	 * Probe the EC with a single byte read to confirm we're
	 * actually talking to it before exposing sensors.
	 */
	if (sambat_ec_read_byte(sc, SAM_REG_FLAGS, &probe) != 0) {
		printf(": EC probe failed\n");
		return;
	}

	/* Sensor framework setup. */
	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_CHARGE].desc,
	    "battery charge", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_CHARGE].type = SENSOR_PERCENT;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_VOLT_NOW].desc,
	    "battery voltage", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_VOLT_NOW].type = SENSOR_VOLTS_DC;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_VOLT_DESIGN].desc,
	    "battery design voltage", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_VOLT_DESIGN].type = SENSOR_VOLTS_DC;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_CURRENT].desc,
	    "battery current", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_CURRENT].type = SENSOR_AMPS;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_CHARGE_NOW].desc,
	    "battery remaining", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_NOW].type = SENSOR_AMPHOUR;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_CHARGE_FULL].desc,
	    "battery full charge", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_FULL].type = SENSOR_AMPHOUR;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_CHARGE_DESIGN].desc,
	    "battery design capacity", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_DESIGN].type = SENSOR_AMPHOUR;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_CYCLES].desc,
	    "battery discharge cycles", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_CYCLES].type = SENSOR_INTEGER;

	strlcpy(sc->sc_sensor[SAMBAT_SENSOR_STATE].desc,
	    "battery state", sizeof(sc->sc_sensor[0].desc));
	sc->sc_sensor[SAMBAT_SENSOR_STATE].type = SENSOR_INTEGER;

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	for (i = 0; i < SAMBAT_NSENSORS; i++) {
		sc->sc_sensor[i].flags |= SENSOR_FINVALID;
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
	}
	sensordev_install(&sc->sc_sensordev);

	if (sensor_task_register(sc, sambat_refresh,
	    SAM_REFRESH_INTERVAL) == NULL) {
		printf(": can't register update task\n");
		return;
	}

	printf(": EC flags 0x%02x\n", probe);

#if NAPM > 0
	apm_setinfohook(sambat_apminfo);
#endif
}

/*
 * Mbox primitives.  All Mbox traffic uses the same I2C slave address
 * (sc->sc_addr); the "hi"/"lo" pair selects the Mbox register inside
 * the EC.  Write: 5-byte transfer.  Read: 4-byte write followed by
 * 2-byte read (first byte must be SAM_MBOX_READ_SUCCESS = 0x50).
 */
int
sambat_mbox_write(struct sambat_softc *sc, uint8_t hi, uint8_t lo,
    uint8_t data)
{
	uint8_t buf[5];
	int error;

	buf[0] = SAM_MBOX_WRITE_PREFIX;
	buf[1] = 0x00;
	buf[2] = hi;
	buf[3] = lo;
	buf[4] = data;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    NULL, 0, buf, sizeof(buf), I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error)
		return error;

	/*
	 * Settle delay between Mbox commands.  Without this, the EC
	 * sometimes returns the still-latched target register byte
	 * instead of the fetched data, which surfaces as nonsense
	 * sensor values (e.g. remaining == register address).
	 *
	 * Busy-wait only during cold boot, otherwise use tsleep to
	 * prevent hiccups on input devices sharing the same I2C bus.
	 */
	if (cold)
		delay(5000);
	else
		tsleep_nsec(&nowake, PWAIT, "sambat", USEC_TO_NSEC(5000));

	return 0;
}

int
sambat_mbox_read(struct sambat_softc *sc, uint8_t hi, uint8_t lo,
    uint8_t *out)
{
	uint8_t cmd[4];
	uint8_t rsp[2];
	int error;

	cmd[0] = SAM_MBOX_READ_PREFIX;
	cmd[1] = 0x00;
	cmd[2] = hi;
	cmd[3] = lo;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    cmd, sizeof(cmd), rsp, sizeof(rsp), I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error)
		return error;
	if (rsp[0] != SAM_MBOX_READ_SUCCESS)
		return EIO;

	*out = rsp[1];
	return 0;
}

int
sambat_ec_read_byte(struct sambat_softc *sc, uint8_t reg, uint8_t *out)
{
	int error;

	error = sambat_mbox_write(sc, SAM_CMD_TARGET_HI, SAM_CMD_TARGET_LO,
	    reg);
	if (error)
		return error;
	error = sambat_mbox_write(sc, SAM_CMD_EXEC_HI, SAM_CMD_EXEC_LO,
	    SAM_EXEC_READ);
	if (error)
		return error;
	return sambat_mbox_read(sc, SAM_CMD_TARGET_HI, SAM_CMD_TARGET_LO,
	    out);
}

int
sambat_ec_read_block(struct sambat_softc *sc, uint8_t reg, uint8_t *buf,
    int n)
{
	int i, error;

	for (i = 0; i < n; i++) {
		error = sambat_ec_read_byte(sc, reg + i, &buf[i]);
		if (error)
			return error;
	}
	return 0;
}

void
sambat_refresh(void *arg)
{
	struct sambat_softc *sc = arg;
	uint8_t flags, b1st;
	uint8_t rr[4], pv[4], af[4], vl[4], cc[2];
	uint16_t remaining, voltage, design_c, fullchg_c, design_v, cycles;
	int16_t cur_ma;
	int i;

	for (i = 0; i < SAMBAT_NSENSORS; i++)
		sc->sc_sensor[i].flags |= SENSOR_FINVALID;

	if (sambat_ec_read_byte(sc, SAM_REG_FLAGS, &flags) != 0 ||
	    sambat_ec_read_byte(sc, SAM_REG_B1ST, &b1st) != 0 ||
	    sambat_ec_read_block(sc, SAM_REG_B1RR, rr, sizeof(rr)) != 0 ||
	    sambat_ec_read_block(sc, SAM_REG_B1PV, pv, sizeof(pv)) != 0 ||
	    sambat_ec_read_block(sc, SAM_REG_B1AF, af, sizeof(af)) != 0 ||
	    sambat_ec_read_block(sc, SAM_REG_B1VL, vl, sizeof(vl)) != 0 ||
	    sambat_ec_read_block(sc, SAM_REG_CYLC, cc, sizeof(cc)) != 0)
		return;

	/*
	 * Each 4-byte EC field carries two big-endian 16-bit values.
	 * The DSDT ByteSwap16(reg >> 16) and ByteSwap16(reg & 0xffff)
	 * idioms extract the upper and lower BE words respectively.
	 */
	remaining = ((uint16_t)rr[2] << 8) | rr[3];
	voltage   = ((uint16_t)pv[2] << 8) | pv[3];
	cur_ma    = (int16_t)(((uint16_t)pv[0] << 8) | pv[1]);
	design_c  = ((uint16_t)af[0] << 8) | af[1];
	fullchg_c = ((uint16_t)af[2] << 8) | af[3];
	design_v  = ((uint16_t)vl[0] << 8) | vl[1];
	cycles    = ((uint16_t)cc[0] << 8) | cc[1];

	if (remaining == 0xffff)
		remaining = 0;
	if (design_c == 0xffff)
		design_c = 0;
	if (fullchg_c == 0xffff)
		fullchg_c = 0;

	sc->sc_present       = !!(flags & SAM_FLAG_B1EX);
	sc->sc_ac_online     = !!(flags & SAM_FLAG_ACEX);
	sc->sc_b1st          = b1st;
	sc->sc_remaining_mah = remaining;
	sc->sc_voltage_mv    = voltage;
	sc->sc_current_ma    = cur_ma;
	sc->sc_design_mah    = design_c;
	sc->sc_fullchg_mah   = fullchg_c ? fullchg_c : design_c;
	sc->sc_design_mv     = design_v;
	sc->sc_have_data     = 1;

	if (sc->sc_fullchg_mah > 0) {
		/* SENSOR_PERCENT: value = percent * 1000 (100% = 100000). */
		sc->sc_sensor[SAMBAT_SENSOR_CHARGE].value =
		    (1000ULL * 100 * remaining) / sc->sc_fullchg_mah;
		sc->sc_sensor[SAMBAT_SENSOR_CHARGE].flags &= ~SENSOR_FINVALID;
	}
	sc->sc_sensor[SAMBAT_SENSOR_VOLT_NOW].value =
	    (uint64_t)voltage * 1000;	/* mV -> uV */
	sc->sc_sensor[SAMBAT_SENSOR_VOLT_NOW].flags &= ~SENSOR_FINVALID;

	sc->sc_sensor[SAMBAT_SENSOR_VOLT_DESIGN].value =
	    (uint64_t)design_v * 1000;
	sc->sc_sensor[SAMBAT_SENSOR_VOLT_DESIGN].flags &= ~SENSOR_FINVALID;

	sc->sc_sensor[SAMBAT_SENSOR_CURRENT].value =
	    (int64_t)cur_ma * 1000;	/* mA -> uA */
	sc->sc_sensor[SAMBAT_SENSOR_CURRENT].flags &= ~SENSOR_FINVALID;

	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_NOW].value =
	    (uint64_t)remaining * 1000;	/* mAh -> uAh */
	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_NOW].flags &= ~SENSOR_FINVALID;

	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_FULL].value =
	    (uint64_t)sc->sc_fullchg_mah * 1000;
	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_FULL].flags &= ~SENSOR_FINVALID;

	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_DESIGN].value =
	    (uint64_t)design_c * 1000;
	sc->sc_sensor[SAMBAT_SENSOR_CHARGE_DESIGN].flags &= ~SENSOR_FINVALID;

	sc->sc_sensor[SAMBAT_SENSOR_CYCLES].value = cycles;
	sc->sc_sensor[SAMBAT_SENSOR_CYCLES].flags &= ~SENSOR_FINVALID;

	sc->sc_sensor[SAMBAT_SENSOR_STATE].value = b1st;
	sc->sc_sensor[SAMBAT_SENSOR_STATE].flags &= ~SENSOR_FINVALID;
}

#if NAPM > 0
int
sambat_apminfo(struct apm_power_info *info)
{
	struct sambat_softc *sc = sambat_sc;

	info->battery_state = APM_BATT_UNKNOWN;
	info->ac_state = APM_AC_UNKNOWN;
	info->battery_life = 0;
	info->minutes_left = -1;

	if (!sc->sc_have_data)
		return 0;

	if (sc->sc_ac_online)
		info->ac_state = APM_AC_ON;
	else
		info->ac_state = APM_AC_OFF;

	if (!sc->sc_present) {
		info->battery_state = APM_BATTERY_ABSENT;
		return 0;
	}

	if (sc->sc_fullchg_mah > 0)
		info->battery_life =
		    (100 * sc->sc_remaining_mah) / sc->sc_fullchg_mah;

	if (sc->sc_b1st & SAM_B1ST_FULL) {
		info->battery_state = APM_BATT_HIGH;
	} else if (sc->sc_b1st & SAM_B1ST_CHARGE) {
		info->battery_state = APM_BATT_CHARGING;
	} else {
		if (info->battery_life > 50)
			info->battery_state = APM_BATT_HIGH;
		else if (info->battery_life > 25)
			info->battery_state = APM_BATT_LOW;
		else
			info->battery_state = APM_BATT_CRITICAL;
	}

	/*
	 * Estimate minutes-left from instantaneous current (mA) and
	 * remaining capacity (mAh).  Only meaningful while discharging.
	 */
	if ((sc->sc_b1st & SAM_B1ST_CHARGE) == 0 && sc->sc_current_ma < 0) {
		int draw = -sc->sc_current_ma;
		if (draw > 0)
			info->minutes_left =
			    (60 * sc->sc_remaining_mah) / draw;
	}

	return 0;
}
#endif
