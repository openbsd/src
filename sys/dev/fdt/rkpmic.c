/*	$OpenBSD: rkpmic.c,v 1.6 2018/07/31 10:09:25 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/i2c/i2cvar.h>

#include <dev/clock_subr.h>

extern todr_chip_handle_t todr_handle;

#define RK808_SECONDS		0x00
#define RK808_MINUTES		0x01
#define RK808_HOURS		0x02
#define RK808_DAYS		0x03
#define RK808_MONTHS		0x04
#define RK808_YEARS		0x05
#define RK808_WEEKS		0x06
#define RK808_RTC_CTRL		0x10
#define  RK808_RTC_CTRL_STOP_RTC	0x01
#define RK808_RTC_STATUS	0x11
#define  RK808_RTC_STATUS_POWER_UP	0x80

#define RK808_NRTC_REGS	7

struct rkpmic_regdata {
	const char *name;
	uint8_t reg, mask;
	uint32_t base, delta;
};

struct rkpmic_regdata rk805_regdata[] = {
	{ "DCDC_REG1", 0x2f, 0x3f, 712500, 12500 },
	{ "DCDC_REG2", 0x33, 0x3f, 712500, 12500 },
	{ "DCDC_REG4", 0x38, 0x1f, 800000, 100000 },
	{ "LDO_REG1", 0x3b, 0x1f, 800000, 100000 },
	{ "LDO_REG2", 0x3d, 0x1f, 800000, 100000 },
	{ "LDO_REG3", 0x3f, 0x1f, 800000, 100000 },
	{ }
};

struct rkpmic_regdata rk808_regdata[] = {
	{ "DCDC_REG1", 0x2f, 0x3f, 712500, 12500 },
	{ "DCDC_REG2", 0x33, 0x3f, 712500, 12500 },
	{ "DCDC_REG4", 0x38, 0x0f, 1800000, 100000 },
	{ "LDO_REG1", 0x3b, 0x1f, 1800000, 100000 },
	{ "LDO_REG2", 0x3d, 0x1f, 1800000, 100000 },
	{ "LDO_REG3", 0x3f, 0x0f, 800000, 100000 },
	{ "LDO_REG4", 0x41, 0x1f, 1800000, 100000 },
	{ "LDO_REG5", 0x43, 0x1f, 1800000, 100000 },
	{ "LDO_REG6", 0x45, 0x1f, 800000, 100000 },
	{ "LDO_REG7", 0x47, 0x1f, 800000, 100000 },
	{ "LDO_REG8", 0x49, 0x1f, 1800000, 100000 },
	{ }
};

struct rkpmic_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	struct todr_chip_handle sc_todr;
	struct rkpmic_regdata *sc_regdata;
};

int	rkpmic_match(struct device *, void *, void *);
void	rkpmic_attach(struct device *, struct device *, void *);

struct cfattach rkpmic_ca = {
	sizeof(struct rkpmic_softc), rkpmic_match, rkpmic_attach
};

struct cfdriver rkpmic_cd = {
	NULL, "rkpmic", DV_DULL
};

void	rkpmic_attach_regulator(struct rkpmic_softc *, int);
uint8_t	rkpmic_reg_read(struct rkpmic_softc *, int);
void	rkpmic_reg_write(struct rkpmic_softc *, int, uint8_t);
int	rkpmic_clock_read(struct rkpmic_softc *, struct clock_ymdhms *);
int	rkpmic_clock_write(struct rkpmic_softc *, struct clock_ymdhms *);
int	rkpmic_gettime(struct todr_chip_handle *, struct timeval *);
int	rkpmic_settime(struct todr_chip_handle *, struct timeval *);

int
rkpmic_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;

	return (OF_is_compatible(node, "rockchip,rk805") ||
		OF_is_compatible(node, "rockchip,rk808"));
}

void
rkpmic_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkpmic_softc *sc = (struct rkpmic_softc *)self;
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;
	const char *chip;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = rkpmic_gettime;
	sc->sc_todr.todr_settime = rkpmic_settime;
	if (todr_handle == NULL)
		todr_handle = &sc->sc_todr;

	if (OF_is_compatible(node, "rockchip,rk805")) {
		chip = "RK805";
		sc->sc_regdata = rk805_regdata;
	} else {
		chip = "RK808";
		sc->sc_regdata = rk808_regdata;
	}
	printf(": %s\n", chip);

	node = OF_getnodebyname(node, "regulators");
	if (node == 0)
		return;
	for (node = OF_child(node); node; node = OF_peer(node))
		rkpmic_attach_regulator(sc, node);
}

struct rkpmic_regulator {
	struct rkpmic_softc *rr_sc;

	uint8_t rr_reg, rr_mask;
	uint32_t rr_base, rr_delta;

	struct regulator_device rr_rd;
};

uint32_t rkpmic_get_voltage(void *);
int	rkpmic_set_voltage(void *, uint32_t);

void
rkpmic_attach_regulator(struct rkpmic_softc *sc, int node)
{
	struct rkpmic_regulator *rr;
	char name[32];
	int i;

	name[0] = 0;
	OF_getprop(node, "name", name, sizeof(name));
	name[sizeof(name) - 1] = 0;
	for (i = 0; sc->sc_regdata[i].name; i++) {
		if (strcmp(sc->sc_regdata[i].name, name) == 0)
			break;
	}
	if (sc->sc_regdata[i].name == NULL)
		return;

	rr = malloc(sizeof(*rr), M_DEVBUF, M_WAITOK | M_ZERO);
	rr->rr_sc = sc;

	rr->rr_reg = sc->sc_regdata[i].reg;
	rr->rr_mask = sc->sc_regdata[i].mask;
	rr->rr_base = sc->sc_regdata[i].base;
	rr->rr_delta = sc->sc_regdata[i].delta;

	rr->rr_rd.rd_node = node;
	rr->rr_rd.rd_cookie = rr;
	rr->rr_rd.rd_get_voltage = rkpmic_get_voltage;
	rr->rr_rd.rd_set_voltage = rkpmic_set_voltage;
	regulator_register(&rr->rr_rd);
}

uint32_t
rkpmic_get_voltage(void *cookie)
{
	struct rkpmic_regulator *rr = cookie;
	uint8_t vsel;
	
	vsel = rkpmic_reg_read(rr->rr_sc, rr->rr_reg);
	return rr->rr_base + (vsel & rr->rr_mask) * rr->rr_delta;
}

int
rkpmic_set_voltage(void *cookie, uint32_t voltage)
{
	struct rkpmic_regulator *rr = cookie;
	uint32_t vmin = rr->rr_base;
	uint32_t vmax = vmin + rr->rr_mask * rr->rr_delta;
	uint8_t vsel;

	if (voltage < vmin || voltage > vmax)
		return EINVAL;

	vsel = rkpmic_reg_read(rr->rr_sc, rr->rr_reg);
	vsel &= ~rr->rr_mask;
	vsel |= (voltage - rr->rr_base) / rr->rr_delta;
	rkpmic_reg_write(rr->rr_sc, rr->rr_reg, vsel);

	return 0;
}

int
rkpmic_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct rkpmic_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	time_t secs;
	int error;

	error = rkpmic_clock_read(sc, &dt);
	if (error)
		return error;

	if (dt.dt_sec > 59 || dt.dt_min > 59 || dt.dt_hour > 23 ||
	    dt.dt_day > 31 || dt.dt_day == 0 ||
	    dt.dt_mon > 12 || dt.dt_mon == 0 ||
	    dt.dt_year < POSIX_BASE_YEAR)
		return EINVAL;

	/*
	 * The RTC thinks November has 31 days.  Match what Linux does
	 * and undo the damage by considering the calenders to be in
	 * sync on January 1st 2016.
	 */
	secs = clock_ymdhms_to_secs(&dt);
	secs += (dt.dt_year - 2016 + (dt.dt_mon == 12 ? 1 : 0)) * 86400;

	tv->tv_sec = secs;
	tv->tv_usec = 0;
	return 0;
}

int
rkpmic_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct rkpmic_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	time_t secs;

	/*
	 * Take care of the November 31st braindamage here as well.
	 * Don't try to be clever, just do the conversion in two
	 * steps, first taking care of November 31 in previous years,
	 * and then taking care of days in December of the current
	 * year.  Decmber 1st turns into November 31st!
	 */
	secs = tv->tv_sec;
	clock_secs_to_ymdhms(secs, &dt);
	secs -= (dt.dt_year - 2016) * 86400;
	clock_secs_to_ymdhms(secs, &dt);
	if (dt.dt_mon == 12) {
		dt.dt_day--;
		if (dt.dt_day == 0) {
			dt.dt_mon = 11;
			dt.dt_day = 31;
		}
	}

	return rkpmic_clock_write(sc, &dt);
}

uint8_t
rkpmic_reg_read(struct rkpmic_softc *sc, int reg)
{
	uint8_t cmd = reg;
	uint8_t val;
	int error;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof cmd, &val, sizeof val, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error) {
		printf("%s: can't read register 0x%02x\n",
		    sc->sc_dev.dv_xname, reg);
		val = 0xff;
	}

	return val;
}

void
rkpmic_reg_write(struct rkpmic_softc *sc, int reg, uint8_t val)
{
	uint8_t cmd = reg;
	int error;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof cmd, &val, sizeof val, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error) {
		printf("%s: can't write register 0x%02x\n",
		    sc->sc_dev.dv_xname, reg);
	}
}

int
rkpmic_clock_read(struct rkpmic_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t regs[RK808_NRTC_REGS];
	uint8_t cmd = RK808_SECONDS;
	uint8_t status;
	int error;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof(cmd), regs, RK808_NRTC_REGS, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error) {
		printf("%s: can't read RTC\n", sc->sc_dev.dv_xname);
		return error;
	}

	/*
	 * Convert the RK808's register values into something useable.
	 */
	dt->dt_sec = FROMBCD(regs[0]);
	dt->dt_min = FROMBCD(regs[1]);
	dt->dt_hour = FROMBCD(regs[2]);
	dt->dt_day = FROMBCD(regs[3]);
	dt->dt_mon = FROMBCD(regs[4]);
	dt->dt_year = FROMBCD(regs[5]) + 2000;

	/* Consider the time to be invalid if the POWER_UP bit is set. */
	status = rkpmic_reg_read(sc, RK808_RTC_STATUS);
	if (status & RK808_RTC_STATUS_POWER_UP)
		return EINVAL;

	return 0;
}

int
rkpmic_clock_write(struct rkpmic_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t regs[RK808_NRTC_REGS];
	uint8_t cmd = RK808_SECONDS;
	int error;

	/*
	 * Convert our time representation into something the RK808
	 * can understand.
	 */
	regs[0] = TOBCD(dt->dt_sec);
	regs[1] = TOBCD(dt->dt_min);
	regs[2] = TOBCD(dt->dt_hour);
	regs[3] = TOBCD(dt->dt_day);
	regs[4] = TOBCD(dt->dt_mon);
	regs[5] = TOBCD(dt->dt_year - 2000);
	regs[6] = TOBCD(dt->dt_wday);

	/* Stop RTC such that we can write to it. */
	rkpmic_reg_write(sc, RK808_RTC_CTRL, RK808_RTC_CTRL_STOP_RTC);

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof(cmd), regs, RK808_NRTC_REGS, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/* Restart RTC. */
	rkpmic_reg_write(sc, RK808_RTC_CTRL, 0);

	if (error) {
		printf("%s: can't write RTC\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* Clear POWER_UP bit to indicate the time is now valid. */
	rkpmic_reg_write(sc, RK808_RTC_STATUS, RK808_RTC_STATUS_POWER_UP);

	return 0;
}
