/*	$OpenBSD: smtpmic.c,v 1.2 2026/07/22 20:33:15 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/task.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/i2c/i2cvar.h>

#include <dev/clock_subr.h>

#include <machine/fdt.h>

extern void (*cpuresetfn)(void);
extern void (*powerdownfn)(void);

/* Registers */
#define COUNT_S			0x0d
#define  COUNT_S_COUNT_SEC		0x3f
#define COUNT_MI		0x0e
#define  COUNT_MI_COUNT_MIN		0x3f
#define COUNT_H			0x0f
#define  COUNT_H_COUNT_HOUR		0x1f
#define COUNT_D			0x10
#define  COUNT_D_COUNT_DAY		0x1f
#define COUNT_MO		0x11
#define  COUNT_MO_COUNT_MONTH		0x0f
#define COUNT_Y			0x12
#define  COUNT_Y_COUNT_YEAR		0x3f
#define RTC_CTRL		0x1d
#define  RTC_CTRL_RTC_EN		(1 << 2)
#define PWR_CTRL2		0x7e
#define  PWR_CTRL2_SW_SD		(1 << 2)
#define  PWR_CTRL2_SW_RST		(1 << 1)

struct smtpmic_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	int (*sc_ih)(void *);

	struct todr_chip_handle sc_todr;
};

int	smtpmic_match(struct device *, void *, void *);
void	smtpmic_attach(struct device *, struct device *, void *);

const struct cfattach smtpmic_ca = {
	sizeof(struct smtpmic_softc), smtpmic_match, smtpmic_attach
};

struct cfdriver smtpmic_cd = {
	NULL, "smtpmic", DV_DULL
};

uint8_t	smtpmic_reg_read(struct smtpmic_softc *, int);
void	smtpmic_reg_write(struct smtpmic_softc *, int, uint8_t);
int	smtpmic_clock_read(struct smtpmic_softc *, struct clock_ymdhms *);
int	smtpmic_clock_write(struct smtpmic_softc *, struct clock_ymdhms *);
int	smtpmic_gettime(struct todr_chip_handle *, struct timeval *);
int	smtpmic_settime(struct todr_chip_handle *, struct timeval *);
void	smtpmic_reset_irq_mask(struct smtpmic_softc *);
void	smtpmic_reset(void);
void	smtpmic_powerdown(void);
int	smtpmic_intr(void *);

int
smtpmic_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	return (strcmp(ia->ia_name, "spacemit,p1") == 0);
}

void
smtpmic_attach(struct device *parent, struct device *self, void *aux)
{
	struct smtpmic_softc *sc = (struct smtpmic_softc *)self;
	struct i2c_attach_args *ia = aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = smtpmic_gettime;
	sc->sc_todr.todr_settime = smtpmic_settime;
	sc->sc_todr.todr_quality = 0;
	todr_attach(&sc->sc_todr);

	cpuresetfn = smtpmic_reset;
	powerdownfn = smtpmic_powerdown;

	printf("\n");
}

uint8_t
smtpmic_reg_read(struct smtpmic_softc *sc, int reg)
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
smtpmic_reg_write(struct smtpmic_softc *sc, int reg, uint8_t val)
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
smtpmic_clock_read(struct smtpmic_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t regs[6];
	uint8_t cmd = COUNT_S;
	uint8_t ctrl;
	int retry, sec = -1;
	int error;

	/* Consider the time to be invalid if the RTC_EN bit isn't set. */
	ctrl = smtpmic_reg_read(sc, RTC_CTRL);
	if ((ctrl & RTC_CTRL_RTC_EN) == 0)
		return EINVAL;

	/*
	 * The datasheet says that reading COUNT_S latches the current
	 * calendar values into COUNT_S through COUNT_Y. But according
	 * to the Linux driver this isn't how the hardware behaves.
	 * Repeatedly read the registers until we get a consistent
	 * reading.
	 */
	for (retry = 0; retry < 20; retry++) {
		iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
		error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof(cmd), regs, sizeof(regs),
		    I2C_F_POLL);
		iic_release_bus(sc->sc_tag, I2C_F_POLL);

		if (error)
			return error;

		dt->dt_sec = (regs[0] & COUNT_S_COUNT_SEC);
		dt->dt_min = (regs[1] & COUNT_MI_COUNT_MIN);
		dt->dt_hour = (regs[2] & COUNT_H_COUNT_HOUR);
		dt->dt_day = (regs[3] & COUNT_D_COUNT_DAY);
		dt->dt_mon = (regs[4] & COUNT_MO_COUNT_MONTH);
		dt->dt_year = (regs[5] & COUNT_Y_COUNT_YEAR) + 2000;

		if (dt->dt_sec == sec)
			break;
		sec = dt->dt_sec;
	}

	/*
	 * If we still don't have a consistent reading, declare the
	 * hardware broken.
	 */
	if (sec != dt->dt_sec)
		return EIO;

	return 0;
}

int
smtpmic_clock_write(struct smtpmic_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t regs[6];
	uint8_t cmd = COUNT_S;
	uint8_t ctrl;
	int error;

	/*
	 * The datasheet says that writing COUNT_Y updates the
	 * calendar counter with the current COUNT_S through COUNT_Y
	 * values.  Again the Linux driver tells us this isn't how the
	 * hardware behaves.  Disable the RTC before updating the
	 * registers and enable it again afterwards.
	 */

	ctrl = smtpmic_reg_read(sc, RTC_CTRL);
	ctrl &= ~RTC_CTRL_RTC_EN;
	smtpmic_reg_write(sc, RTC_CTRL, ctrl);

	regs[0] = dt->dt_sec;
	regs[1] = dt->dt_min;
	regs[2] = dt->dt_hour;
	regs[3] = dt->dt_day;
	regs[4] = dt->dt_mon;
	regs[5] = dt->dt_year - 2000;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof(cmd), regs, sizeof(regs), I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error)
		return error;

	smtpmic_reg_write(sc, RTC_CTRL, ctrl | RTC_CTRL_RTC_EN);

	return 0;
}

int
smtpmic_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct smtpmic_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	int error;

	error = smtpmic_clock_read(sc, &dt);
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
smtpmic_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct smtpmic_softc *sc = handle->cookie;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	return smtpmic_clock_write(sc, &dt);
}

void
smtpmic_reset(void)
{
	struct smtpmic_softc *sc = smtpmic_cd.cd_devs[0];
	uint8_t ctrl;

	ctrl = smtpmic_reg_read(sc, PWR_CTRL2);
	smtpmic_reg_write(sc, PWR_CTRL2, ctrl | PWR_CTRL2_SW_RST);
}

void
smtpmic_powerdown(void)
{
	struct smtpmic_softc *sc = smtpmic_cd.cd_devs[0];
	uint8_t ctrl;

	ctrl = smtpmic_reg_read(sc, PWR_CTRL2);
	smtpmic_reg_write(sc, PWR_CTRL2, ctrl | PWR_CTRL2_SW_SD);
}
