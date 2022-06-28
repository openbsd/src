/*	$OpenBSD: rkpmic.c,v 1.10 2022/06/28 23:43:12 naddy Exp $	*/
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

#define RK80X_SECONDS		0x00
#define RK80X_MINUTES		0x01
#define RK80X_HOURS		0x02
#define RK80X_DAYS		0x03
#define RK80X_MONTHS		0x04
#define RK80X_YEARS		0x05
#define RK80X_WEEKS		0x06
#define RK80X_NRTC_REGS	7

#define RK805_RTC_CTRL		0x10
#define RK808_RTC_CTRL		0x10
#define RK809_RTC_CTRL		0x0d
#define  RK80X_RTC_CTRL_STOP_RTC	0x01

#define RK805_RTC_STATUS	0x11
#define RK808_RTC_STATUS	0x11
#define RK809_RTC_STATUS	0x0e
#define  RK80X_RTC_STATUS_POWER_UP	0x80

struct rkpmic_vsel_range {
	uint32_t base, delta;
	uint8_t vsel_min, vsel_max;
};

struct rkpmic_regdata {
	const char *name;
	uint8_t reg, mask;
	const struct rkpmic_vsel_range *vsel_range;
};

/* 
 * Used by RK805 for BUCK1, BUCK2
 *  0-59:	0.7125V-1.45V, step=12.5mV
 *  60-62:	1.8V-2.2V, step=200mV
 *  63:		2.3V
 */
const struct rkpmic_vsel_range rk805_vsel_range1[] = {
	{ 712500, 12500, 0, 59 },
	{ 1800000, 200000, 60, 62 },
	{ 2300000, 0, 63, 63 },
	{}
};

/*
 * Used by RK805 for BUCK4 
 *  0-27:	0.8V-3.5V, step=100mV
 */
const struct rkpmic_vsel_range rk805_vsel_range2[] = {
	{ 800000, 100000, 0, 27 },
	{}
};

/*
 * Used by RK805 for LDO1-3 
 *  0-26:	0.8V-3.4V, step=100mV
 */
const struct rkpmic_vsel_range rk805_vsel_range3[] = {
	{ 800000, 100000, 0, 26 },
	{}
};

const struct rkpmic_regdata rk805_regdata[] = {
	{ "DCDC_REG1", 0x2f, 0x3f, rk805_vsel_range1 },
	{ "DCDC_REG2", 0x33, 0x3f, rk805_vsel_range1 },
	{ "DCDC_REG4", 0x38, 0x1f, rk805_vsel_range2 },
	{ "LDO_REG1", 0x3b, 0x1f, rk805_vsel_range3 },
	{ "LDO_REG2", 0x3d, 0x1f, rk805_vsel_range3 },
	{ "LDO_REG3", 0x3f, 0x1f, rk805_vsel_range3 },
	{ }
};

/*
 * Used by RK808 for BUCK1 & BUCK2
 *  0-63:	0.7125V-1.5V, step=12.5mV
 */
const struct rkpmic_vsel_range rk808_vsel_range1[] = {
	{ 712500, 12500, 0, 63 },
	{}
};

/*
 * Used by RK808 for BUCK4
 *  0-15:	1.8V-3.3V,step=100mV
 */
const struct rkpmic_vsel_range rk808_vsel_range2[] = {
	{ 1800000, 100000, 0, 15 },
	{}
};

/*
 * Used by RK808 for LDO1-2, 4-5, 8
 *  0-16:	1.8V-3.4V, step=100mV
 */
const struct rkpmic_vsel_range rk808_vsel_range3[] = {
	{ 1800000, 100000, 0, 16 },
	{}
};

/*
 * Used by RK808 for LDO3
 *   0-12:	0.8V~2.0V, step=100mV
 *   13:	2.2V
 *   15:	2.5V
 */
const struct rkpmic_vsel_range rk808_vsel_range4[] = {
	{ 800000, 100000, 0, 12 },
	{ 2200000, 0, 13, 13 },
	{ 2500000, 0, 15, 15 },
	{}
};

/*
 * Used by RK808 for LDO6-7
 *  0-17:	0.8V-2.5V,step=100mV
 */
const struct rkpmic_vsel_range rk808_vsel_range5[] = {
	{ 800000, 100000, 0, 17 },
	{}
};

const struct rkpmic_regdata rk808_regdata[] = {
	{ "DCDC_REG1", 0x2f, 0x3f, rk808_vsel_range1 },
	{ "DCDC_REG2", 0x33, 0x3f, rk808_vsel_range1 },
	{ "DCDC_REG4", 0x38, 0x0f, rk808_vsel_range2 },
	{ "LDO_REG1", 0x3b, 0x1f, rk808_vsel_range3 },
	{ "LDO_REG2", 0x3d, 0x1f, rk808_vsel_range3 },
	{ "LDO_REG3", 0x3f, 0x0f, rk808_vsel_range4 },
	{ "LDO_REG4", 0x41, 0x1f, rk808_vsel_range3 },
	{ "LDO_REG5", 0x43, 0x1f, rk808_vsel_range3 },
	{ "LDO_REG6", 0x45, 0x1f, rk808_vsel_range5 },
	{ "LDO_REG7", 0x47, 0x1f, rk808_vsel_range5 },
	{ "LDO_REG8", 0x49, 0x1f, rk808_vsel_range3 },
	{ }
};

/*
 * Used by RK809 for BUCK1-3
 *  0-80:	0.5V-1.5V,step=12.5mV
 *  81-89:	1.6V-2.4V,step=100mV
 */
const struct rkpmic_vsel_range rk809_vsel_range1[] = {
	{ 500000, 12500, 0, 80 },
	{ 1600000, 100000, 81, 89 },
	{}
};

/*
 * Used by RK809 for BUCK4
 *  0-80:	0.5V-1.5V,step=12.5mV
 *  81-99:	1.6V-3.4V,step=100mV
 */
const struct rkpmic_vsel_range rk809_vsel_range2[] = {
	{ 500000, 12500, 0, 80 },
	{ 1600000, 100000, 81, 99 },
	{}
};

/*
 * Used by RK809 for BUCK5
 *  0:		1.5V
 *  1-3:	1.8V-2.2V,step=200mV
 *  4-5:	2.8V-3.0V,step=200mV
 *  6-7:	3.3V-3.6V,step=300mV
 */
const struct rkpmic_vsel_range rk809_vsel_range3[] = {
	{ 1500000, 0, 0, 0 },
	{ 1800000, 200000, 1, 3 },
	{ 2800000, 200000, 4, 5 },
	{ 3300000, 300000, 6, 7 },
	{}
};

/*
 * Used by RK809 for LDO1-7
 *  0-112: 0.6V-3.4V,step=25mV
 */
const struct rkpmic_vsel_range rk809_vsel_range4[] = {
	{ 600000, 25000, 0, 112 },
	{}
};

const struct rkpmic_regdata rk809_regdata[] = {
	{ "DCDC_REG1", 0xbb, 0x7f, rk809_vsel_range1 },
	{ "DCDC_REG2", 0xbe, 0x7f, rk809_vsel_range1 },
	{ "DCDC_REG3", 0xc1, 0x7f, rk809_vsel_range1 },
	{ "DCDC_REG4", 0xc4, 0x7f, rk809_vsel_range2 },
	{ "DCDC_REG5", 0xde, 0x0f, rk809_vsel_range3},
	{ "LDO_REG1", 0xcc, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG2", 0xce, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG3", 0xd0, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG4", 0xd2, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG5", 0xd4, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG6", 0xd6, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG7", 0xd8, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG8", 0xda, 0x7f, rk809_vsel_range4 },
	{ "LDO_REG9", 0xdc, 0x7f, rk809_vsel_range4 },
	{ }
};

struct rkpmic_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	int sc_rtc_ctrl_reg, sc_rtc_status_reg;
	struct todr_chip_handle sc_todr;
	const struct rkpmic_regdata *sc_regdata;
};

int	rkpmic_match(struct device *, void *, void *);
void	rkpmic_attach(struct device *, struct device *, void *);

const struct cfattach rkpmic_ca = {
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

	return (strcmp(ia->ia_name, "rockchip,rk805") == 0 ||
	    strcmp(ia->ia_name, "rockchip,rk808") == 0 ||
	    strcmp(ia->ia_name, "rockchip,rk809") == 0);
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
		sc->sc_rtc_ctrl_reg = RK805_RTC_CTRL;
		sc->sc_rtc_status_reg = RK805_RTC_STATUS;
		sc->sc_regdata = rk805_regdata;
	} else if (OF_is_compatible(node, "rockchip,rk808")) {
		chip = "RK808";
		sc->sc_rtc_ctrl_reg = RK808_RTC_CTRL;
		sc->sc_rtc_status_reg = RK808_RTC_STATUS;
		sc->sc_regdata = rk808_regdata;
	} else {
		chip = "RK809";
		sc->sc_rtc_ctrl_reg = RK809_RTC_CTRL;
		sc->sc_rtc_status_reg = RK809_RTC_STATUS;
		sc->sc_regdata = rk809_regdata;
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
	const struct rkpmic_vsel_range *rr_vsel_range;

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
	rr->rr_vsel_range = sc->sc_regdata[i].vsel_range;

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
	const struct rkpmic_vsel_range *vsel_range = rr->rr_vsel_range;
	uint8_t vsel;
	uint32_t ret = 0;

	vsel = rkpmic_reg_read(rr->rr_sc, rr->rr_reg) & rr->rr_mask;
	
	while (vsel_range->base) {
		ret = vsel_range->base;
		if (vsel >= vsel_range->vsel_min &&
		    vsel <= vsel_range->vsel_max) {
			ret += (vsel - vsel_range->vsel_min) *
			    vsel_range->delta;
			break;
		} else
			ret += (vsel_range->vsel_max - vsel_range->vsel_min) *
			    vsel_range->delta;
		vsel_range++;
			
	}

	return ret;
}

int
rkpmic_set_voltage(void *cookie, uint32_t voltage)
{
	struct rkpmic_regulator *rr = cookie;
	const struct rkpmic_vsel_range *vsel_range = rr->rr_vsel_range;
	uint32_t vmin, vmax, volt;
	uint8_t reg, vsel;

	while (vsel_range->base) {
		vmin = vsel_range->base;
		vmax = vmin + (vsel_range->vsel_max - vsel_range->vsel_min) *
		    vsel_range->delta;
		if (voltage < vmin)
			return EINVAL;
		if (voltage <= vmax) {
			vsel = vsel_range->vsel_min;
			volt = vsel_range->base;
			while (vsel <= vsel_range->vsel_max) {
				if (volt == voltage)
					break;
				else {
					vsel++;
					volt += vsel_range->delta;
				}
			}
			if (volt != voltage)
				return EINVAL;
			break;
		}
		vsel_range++;
	}

	if (vsel_range->base == 0)
		return EINVAL;

	reg = rkpmic_reg_read(rr->rr_sc, rr->rr_reg);
	reg &= ~rr->rr_mask;
	reg |= vsel;
	rkpmic_reg_write(rr->rr_sc, rr->rr_reg, reg);

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
	uint8_t regs[RK80X_NRTC_REGS];
	uint8_t cmd = RK80X_SECONDS;
	uint8_t status;
	int error;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof(cmd), regs, RK80X_NRTC_REGS, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error) {
		printf("%s: can't read RTC\n", sc->sc_dev.dv_xname);
		return error;
	}

	/*
	 * Convert the RK80x's register values into something useable.
	 */
	dt->dt_sec = FROMBCD(regs[0]);
	dt->dt_min = FROMBCD(regs[1]);
	dt->dt_hour = FROMBCD(regs[2]);
	dt->dt_day = FROMBCD(regs[3]);
	dt->dt_mon = FROMBCD(regs[4]);
	dt->dt_year = FROMBCD(regs[5]) + 2000;

	/* Consider the time to be invalid if the POWER_UP bit is set. */
	status = rkpmic_reg_read(sc, sc->sc_rtc_status_reg);
	if (status & RK80X_RTC_STATUS_POWER_UP)
		return EINVAL;

	return 0;
}

int
rkpmic_clock_write(struct rkpmic_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t regs[RK80X_NRTC_REGS];
	uint8_t cmd = RK80X_SECONDS;
	int error;

	/*
	 * Convert our time representation into something the RK80x
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
	rkpmic_reg_write(sc, sc->sc_rtc_ctrl_reg, RK80X_RTC_CTRL_STOP_RTC);

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof(cmd), regs, RK80X_NRTC_REGS, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/* Restart RTC. */
	rkpmic_reg_write(sc, sc->sc_rtc_ctrl_reg, 0);

	if (error) {
		printf("%s: can't write RTC\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* Clear POWER_UP bit to indicate the time is now valid. */
	rkpmic_reg_write(sc, sc->sc_rtc_status_reg, RK80X_RTC_STATUS_POWER_UP);

	return 0;
}
