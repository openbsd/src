/*	$OpenBSD: pi2c.c,v 1.3 2005/11/19 21:45:44 brad Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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
#include <sys/lock.h>
#include <sys/proc.h>

#include <machine/autoconf.h>

#include <dev/i2c/i2cvar.h>

#include <arch/macppc/dev/maci2cvar.h>
#include <arch/macppc/dev/pm_direct.h>

struct pi2c_softc {
	struct device	sc_dev;

	struct lock	sc_buslock;
	struct i2c_controller sc_i2c_tag;
};

int     pi2c_match(struct device *, void *, void *);
void    pi2c_attach(struct device *, struct device *, void *);

struct cfattach pi2c_ca = {
	sizeof(struct pi2c_softc), pi2c_match, pi2c_attach
};

struct cfdriver pi2c_cd = {
        NULL, "pi2c", DV_DULL,
};

int	pi2c_i2c_acquire_bus(void *, int);
void	pi2c_i2c_release_bus(void *, int);
int	pi2c_i2c_exec(void *, i2c_op_t, i2c_addr_t,
	    const void *, size_t, void *buf, size_t, int);

int
pi2c_match(struct device *parent, void *cf, void *aux)
{
	return (1);
}

void
pi2c_attach(struct device *parent, struct device *self, void *aux)
{
	struct pi2c_softc *sc = (struct pi2c_softc *)self;
	struct confargs *ca = aux;
	struct maci2cbus_attach_args iba;

	printf("i2c controller\n");

	lockinit(&sc->sc_buslock, PZERO, sc->sc_dev.dv_xname, 0, 0);

	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = pi2c_i2c_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = pi2c_i2c_release_bus;
	sc->sc_i2c_tag.ic_exec = pi2c_i2c_exec;
	
	iba.iba_node = ca->ca_node;
	iba.iba_tag = &sc->sc_i2c_tag;
	config_found(&sc->sc_dev, &iba, NULL);
}

int
pi2c_i2c_acquire_bus(void *cookie, int flags)
{
	struct pi2c_softc *sc = cookie;

	return (lockmgr(&sc->sc_buslock, LK_EXCLUSIVE, NULL));
}

void
pi2c_i2c_release_bus(void *cookie, int flags)
{
	struct pi2c_softc *sc = cookie;

        lockmgr(&sc->sc_buslock, LK_RELEASE, NULL);
}

int
pi2c_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	u_int8_t pmu_op = PMU_I2C_NORMAL;
	int retries = 10;
	PMData p;

	if (!I2C_OP_STOP_P(op) || cmdlen > 1 || len > 5)
		return (EINVAL);

	if (cmdlen == 0)
		pmu_op = PMU_I2C_SIMPLE;
	else if (I2C_OP_READ_P(op))
		pmu_op = PMU_I2C_COMBINED;

	p.command = PMU_I2C;
	p.num_data = 7 + len;
	p.s_buf = p.r_buf = p.data;

	p.data[0] = addr >> 7;	/* bus number */
	p.data[1] = pmu_op;
	p.data[2] = 0;
	p.data[3] = addr << 1;
	p.data[4] = *(u_int8_t *)cmdbuf;
	p.data[5] = addr << 1 | I2C_OP_READ_P(op);
	p.data[6] = len;
	memcpy(&p.data[7], buf, len);

	if (pmgrop(&p))
		return (EIO);

	while (retries--) {
		p.command = PMU_I2C;
		p.num_data = 1;
		p.s_buf = p.r_buf = p.data;
		p.data[0] = 0;

		if (pmgrop(&p))
			return (EIO);

		if (p.data[0] == 1)
			break;

		DELAY(10 * 1000);
	}

	if (I2C_OP_READ_P(op))
		memcpy(buf, &p.data[1], len);
	return (0);
}
