/*	$OpenBSD: dapmic.c,v 1.2 2022/04/06 18:59:28 naddy Exp $	*/
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
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/i2c/i2cvar.h>

#include <dev/clock_subr.h>

extern void (*cpuresetfn)(void);
extern void (*powerdownfn)(void);

/* Registers */
#define EVENT_A			0x06
#define CONTROL_F		0x13
#define  CONTROL_F_WAKE_UP		(1 << 2)
#define  CONTROL_F_SHUTDOWN		(1 << 1)
#define COUNT_S			0x40
#define  COUNT_S_COUNT_SEC		0x3f
#define COUNT_MI		0x41
#define  COUNT_MI_COUNT_MIN		0x3f
#define COUNT_H			0x42
#define  COUNT_H_COUNT_HOUR		0x1f
#define COUNT_D			0x43
#define  COUNT_D_COUNT_DAY		0x1f
#define COUNT_MO		0x44
#define  COUNT_MO_COUNT_MONTH		0x0f
#define COUNT_Y			0x45
#define  COUNT_Y_MONITOR		(1 << 6)
#define  COUNT_Y_COUNT_YEAR		0x3f
#define ALARM_MO		0x4a
#define  ALARM_MO_TICK_WAKE		(1 << 5)
#define  ALARM_MO_TICK_TYPE		(1 << 4)
#define ALARM_Y			0x4b
#define  ALARM_Y_TICK_ON		(1 << 7)

struct dapmic_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	struct todr_chip_handle sc_todr;
};

int	dapmic_match(struct device *, void *, void *);
void	dapmic_attach(struct device *, struct device *, void *);

const struct cfattach dapmic_ca = {
	sizeof(struct dapmic_softc), dapmic_match, dapmic_attach
};

struct cfdriver dapmic_cd = {
	NULL, "dapmic", DV_DULL
};

uint8_t	dapmic_reg_read(struct dapmic_softc *, int);
void	dapmic_reg_write(struct dapmic_softc *, int, uint8_t);
int	dapmic_clock_read(struct dapmic_softc *, struct clock_ymdhms *);
int	dapmic_clock_write(struct dapmic_softc *, struct clock_ymdhms *);
int	dapmic_gettime(struct todr_chip_handle *, struct timeval *);
int	dapmic_settime(struct todr_chip_handle *, struct timeval *);
void	dapmic_reset(void);
void	dapmic_powerdown(void);

int
dapmic_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	return (strcmp(ia->ia_name, "dlg,da9063") == 0);
}

void
dapmic_attach(struct device *parent, struct device *self, void *aux)
{
	struct dapmic_softc *sc = (struct dapmic_softc *)self;
	struct i2c_attach_args *ia = aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = dapmic_gettime;
	sc->sc_todr.todr_settime = dapmic_settime;
	todr_attach(&sc->sc_todr);

	printf("\n");

	if (cpuresetfn == NULL)
		cpuresetfn = dapmic_reset;
	if (powerdownfn == NULL)
		powerdownfn = dapmic_powerdown;
}

uint8_t
dapmic_reg_read(struct dapmic_softc *sc, int reg)
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
dapmic_reg_write(struct dapmic_softc *sc, int reg, uint8_t val)
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
dapmic_clock_read(struct dapmic_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t regs[6];
	uint8_t cmd = COUNT_S;
	int error;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof(cmd), regs, sizeof(regs), I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error)
		return error;

	dt->dt_sec = (regs[0] & COUNT_S_COUNT_SEC);
	dt->dt_min = (regs[1] & COUNT_MI_COUNT_MIN);
	dt->dt_hour = (regs[2] & COUNT_H_COUNT_HOUR);
	dt->dt_day = (regs[3] & COUNT_D_COUNT_DAY);
	dt->dt_mon = (regs[4] & COUNT_MO_COUNT_MONTH);
	dt->dt_year = (regs[5] & COUNT_Y_COUNT_YEAR) + 2000;

	/* Consider the time to be invalid if the MONITOR bit isn't set. */
	if ((regs[5] & COUNT_Y_MONITOR) == 0)
		return EINVAL;

	return 0;
}

int
dapmic_clock_write(struct dapmic_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t regs[6];
	uint8_t cmd = COUNT_S;
	int error;

	regs[0] = dt->dt_sec;
	regs[1] = dt->dt_min;
	regs[2] = dt->dt_hour;
	regs[3] = dt->dt_day;
	regs[4] = dt->dt_mon;
	regs[5] = (dt->dt_year - 2000) | COUNT_Y_MONITOR;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof(cmd), regs, sizeof(regs), I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error)
		return error;

	return 0;
}

int
dapmic_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct dapmic_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	int error;

	error = dapmic_clock_read(sc, &dt);
	if (error)
		return error;

	if (dt.dt_sec > 59 || dt.dt_min > 59 || dt.dt_hour > 23 ||
	    dt.dt_day > 31 || dt.dt_day == 0 ||
	    dt.dt_mon > 12 || dt.dt_mon == 0 ||
	    dt.dt_year < POSIX_BASE_YEAR)
		return EINVAL;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
dapmic_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct dapmic_softc *sc = handle->cookie;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	return dapmic_clock_write(sc, &dt);
}

void
dapmic_reset(void)
{
	struct dapmic_softc *sc = dapmic_cd.cd_devs[0];
	uint8_t reg;

	/* Enable tick alarm wakeup with a one second interval. */
	reg = dapmic_reg_read(sc, ALARM_MO);
	reg &= ~ALARM_MO_TICK_TYPE;
	reg |= ALARM_MO_TICK_WAKE;
	dapmic_reg_write(sc, ALARM_MO, reg);

	/* Enable tick function. */
	reg = dapmic_reg_read(sc, ALARM_Y);
	reg |= ALARM_Y_TICK_ON;
	dapmic_reg_write(sc, ALARM_Y, reg);

	/* Clear events such that we wake up again. */
	dapmic_reg_write(sc, EVENT_A, dapmic_reg_read(sc, EVENT_A));
	dapmic_reg_write(sc, CONTROL_F, CONTROL_F_SHUTDOWN);
}

void
dapmic_powerdown(void)
{
	struct dapmic_softc *sc = dapmic_cd.cd_devs[0];
	uint8_t reg;

	/* Disable tick function such that it doesn't wake us up. */
	reg = dapmic_reg_read(sc, ALARM_Y);
	reg &= ~ALARM_Y_TICK_ON;
	dapmic_reg_write(sc, ALARM_Y, reg);

	dapmic_reg_write(sc, CONTROL_F, CONTROL_F_SHUTDOWN);
}
