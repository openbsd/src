/*	$OpenBSD: lm75.c,v 1.2 2005/11/11 16:14:14 kettenis Exp $	*/
/*	$NetBSD: lm75.c,v 1.1 2003/09/30 00:35:31 thorpej Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * National Semiconductor LM75/LM77 temperature sensor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/sensors.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/lm75reg.h>

struct lmtemp_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	int sc_address;
	int sc_model;

	struct sensor sc_sensor;
};

int  lmtemp_match(struct device *, void *, void *);
void lmtemp_attach(struct device *, struct device *, void *);

struct cfattach lmtemp_ca = {
	sizeof(struct lmtemp_softc),
	lmtemp_match,
	lmtemp_attach
};

struct cfdriver lmtemp_cd = {
	NULL, "lmtemp", DV_DULL
};

int  lmtemp_config_write(struct lmtemp_softc *, uint8_t);
int  lmtemp_temp_read(struct lmtemp_softc *, uint8_t, int *);
void lmtemp_refresh_sensor_data(void *);

int
lmtemp_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	/* XXX: we allow wider mask for LM77 */
	if ((ia->ia_addr & LM75_ADDRMASK) == LM75_ADDR)
		return (1);

	return (0);
}

void
lmtemp_attach(struct device *parent, struct device *self, void *aux)
{
	struct lmtemp_softc *sc = (struct lmtemp_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t ptr[1], reg[LM75_TEMP_LEN];

	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;

	/* Try to detect LM77 by poking Thigh register */
	ptr[0] = LM77_REG_THIGH;
	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_address, ptr, 1, reg, LM75_TEMP_LEN, 0) == 0) {
		/* Power up default is 64 degC */
		if (lm77_wordtotemp((reg[0] << 8) | reg[1]) == 64 * 2) {
			sc->sc_model = LM_MODEL_LM77;
			printf(": LM77\n");
		}
	} else {
		sc->sc_model = LM_MODEL_LM75;
		printf(": LM75\n");
	}
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/* Set the configuration to defaults */
	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	if (lmtemp_config_write(sc, 0) != 0) {
		printf("%s: unable to write config register\n",
		    sc->sc_dev.dv_xname);
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		return;
	}
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/* Initialize sensor data */
	strlcpy(sc->sc_sensor.device, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensor.device));
	sc->sc_sensor.type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor.desc, "TEMP", sizeof(sc->sc_sensor.desc));

	/* Hook into the hw.sensors sysctl */
	SENSOR_ADD(&sc->sc_sensor);

	sensor_task_register(sc, lmtemp_refresh_sensor_data, LM_POLLTIME);
}

int
lmtemp_config_write(struct lmtemp_softc *sc, uint8_t val)
{
	uint8_t cmdbuf[2];

	cmdbuf[0] = LM75_REG_CONFIG;
	cmdbuf[1] = val;

	return (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_address, cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL));
}

int
lmtemp_temp_read(struct lmtemp_softc *sc, uint8_t which, int *valp)
{
	u_int8_t cmdbuf[1];
	u_int8_t buf[LM75_TEMP_LEN];
	int error;

	cmdbuf[0] = which;

	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_address, cmdbuf, 1, buf, LM75_TEMP_LEN, 0);
	if (error)
		return (error);

	switch (sc->sc_model) {
	case LM_MODEL_LM75:
		*valp = lm75_wordtotemp((buf[0] << 8) | buf[1]);
		break;
	case LM_MODEL_LM77:
		*valp = lm77_wordtotemp((buf[0] << 8) | buf[1]);
		break;
	default:
		printf("%s: unknown model (%d)\n",
		    sc->sc_dev.dv_xname, sc->sc_model);
		return (1);
	}

	return (0);
}

void
lmtemp_refresh_sensor_data(void *aux)
{
	struct lmtemp_softc *sc = aux;
	int val;
	int error;

	error = lmtemp_temp_read(sc, LM75_REG_TEMP, &val);
	if (error) {
#if 0
		printf("%s: unable to read temperature, error = %d\n",
		    sc->sc_dev.dv_xname, error);
#endif
		sc->sc_sensor.flags |= SENSOR_FINVALID;
		return;
	}

	sc->sc_sensor.value = val * 500000 + 273150000;
	sc->sc_sensor.flags &= ~SENSOR_FINVALID;
}
