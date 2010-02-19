/*	$OpenBSD: m41t8xclock.c,v 1.1 2010/02/19 00:21:45 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
 * M41T8x clock connected to an I2C bus
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/i2c/i2cvar.h>

#include <dev/ic/m41t8xreg.h>

#include <mips64/dev/clockvar.h>

struct m41t8xclock_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;
};

int	m41t8xclock_match(struct device *, void *, void *);
void	m41t8xclock_attach(struct device *, struct device *, void *);

const struct cfattach mfokclock_ca = {
	sizeof(struct m41t8xclock_softc),
	m41t8xclock_match, m41t8xclock_attach
};

struct cfdriver mfokclock_cd = {
	NULL, "mfokclock", DV_DULL
};

void	m41t8xclock_get(void *, time_t, struct tod_time *);
void	m41t8xclock_set(void *, struct tod_time *);

int
m41t8xclock_match(struct device *parent, void *vcf, void *aux)
{
	struct i2c_attach_args *ia = (struct i2c_attach_args *)aux;
	struct cfdata *cf = (struct cfdata *)vcf;

	return strcmp(ia->ia_name, cf->cf_driver->cd_name) == 0;
}

void
m41t8xclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct m41t8xclock_softc *sc = (struct m41t8xclock_softc *)self;
	struct i2c_attach_args *ia = (struct i2c_attach_args *)aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	sys_tod.tod_cookie = sc;
	sys_tod.tod_get = m41t8xclock_get;
	sys_tod.tod_set = m41t8xclock_set;

	printf("\n");
}

static inline int bcd2bin(int);
static inline int
bcd2bin(int datum)
{
	return (datum >> 4) * 10 + (datum & 0x0f);
}
static inline int bin2bcd(int);
static inline int
bin2bcd(int datum)
{
	return ((datum / 10) << 4) + (datum % 10);
}

void
m41t8xclock_get(void *cookie, time_t unused, struct tod_time *tt)
{
	struct m41t8xclock_softc *sc = (struct m41t8xclock_softc *)cookie;
	uint8_t regno, data[M41T8X_TOD_LENGTH];
	int s;

	iic_acquire_bus(sc->sc_tag, 0);
	s = splclock();
	for (regno = M41T8X_TOD_START;
	    regno < M41T8X_TOD_START + M41T8X_TOD_LENGTH; regno++)
		iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
		    &regno, sizeof regno, data + regno, sizeof data[0], 0);
	splx(s);
	iic_release_bus(sc->sc_tag, 0);

	tt->sec = bcd2bin(data[M41T8X_SEC] & ~M41T8X_STOP);
	tt->min = bcd2bin(data[M41T8X_MIN]);
	tt->hour = bcd2bin(data[M41T8X_HR] & ~(M41T8X_CEB | M41T8X_CB));
	tt->dow = data[M41T8X_DOW];
	tt->day = bcd2bin(data[M41T8X_DAY]);
	tt->mon = bcd2bin(data[M41T8X_MON]);
	tt->year = bcd2bin(data[M41T8X_YEAR]) + 100;
	if (data[M41T8X_HR] & M41T8X_CB)
		tt->year += 100;
}

void
m41t8xclock_set(void *cookie, struct tod_time *tt)
{
	struct m41t8xclock_softc *sc = (struct m41t8xclock_softc *)cookie;
	uint8_t regno, data[M41T8X_TOD_LENGTH];
	int s;

	iic_acquire_bus(sc->sc_tag, 0);
	s = splclock();
	/* read current state */
	for (regno = M41T8X_TOD_START;
	    regno < M41T8X_TOD_START + M41T8X_TOD_LENGTH; regno++)
		iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
		    &regno, sizeof regno, data + regno, sizeof data[0], 0);
	/* compute new state */
	data[M41T8X_HSEC] = 0;
	data[M41T8X_SEC] = bin2bcd(tt->sec);
	data[M41T8X_MIN] = bin2bcd(tt->min);
	data[M41T8X_HR] &= M41T8X_CEB;
	if (tt->year >= 200)
		data[M41T8X_HR] |= M41T8X_CB;
	data[M41T8X_HR] |= bin2bcd(tt->hour);
	data[M41T8X_DAY] = bin2bcd(tt->day);
	data[M41T8X_MON] = bin2bcd(tt->mon);
	if (tt->year >= 200)
		data[M41T8X_YEAR] = bin2bcd(tt->year - 200);
	else
		data[M41T8X_YEAR] = bin2bcd(tt->year - 100);
	/* write new state */
	for (regno = M41T8X_TOD_START;
	    regno < M41T8X_TOD_START + M41T8X_TOD_LENGTH; regno++)
		iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
		    &regno, sizeof regno, data + regno, sizeof data[0], 0);
	splx(s);
	iic_release_bus(sc->sc_tag, 0);
}
