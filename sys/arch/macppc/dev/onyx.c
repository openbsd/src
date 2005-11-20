/*	$OpenBSD: onyx.c,v 1.4 2005/11/20 19:54:04 brad Exp $	*/

/*-
 * Copyright (c) 2005 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * http://focus.ti.com/docs/prod/folders/print/pcm3052.html
 *
 * Datasheet is available from
 * http://focus.ti.com/docs/prod/folders/print/pcm3052a.html
 */

#include <sys/param.h>
#include <sys/audioio.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/audio_if.h>
#include <dev/ofw/openfirm.h>
#include <macppc/dev/dbdma.h>

#include <machine/autoconf.h>

#include <macppc/dev/i2svar.h>
#include <macppc/dev/ki2cvar.h>

#ifdef ONYX_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

/* XXX */
#define PCM3052_I2C_ADDR	0x8c

/* PCM3052 registers */
#define PCM3052_REG_LEFT_VOLUME		0x41
#define PCM3052_REG_RIGHT_VOLUME	0x42

/* XXX */
#define onyx_softc i2s_softc

/* XXX */
void ki2c_setmode(struct ki2c_softc *, u_int, u_int);
int ki2c_write(struct device *, int, int, const void *, int);

int onyx_getdev(void *, struct audio_device *);
int onyx_match(struct device *, void *, void *);
void onyx_attach(struct device *, struct device *, void *);
void onyx_defer(struct device *);
void onyx_set_volume(struct onyx_softc *, int, int);

struct cfattach onyx_ca = {
	sizeof(struct onyx_softc), onyx_match, onyx_attach
};

struct cfdriver onyx_cd = {
	NULL, "onyx", DV_DULL
};

struct audio_hw_if onyx_hw_if = {
	i2s_open,
	i2s_close,
	NULL,
	i2s_query_encoding,
	i2s_set_params,
	i2s_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	i2s_halt_output,
	i2s_halt_input,
	NULL,
	onyx_getdev,
	NULL,
	i2s_set_port,
	i2s_get_port,
	i2s_query_devinfo,
	i2s_allocm,
	NULL,
	i2s_round_buffersize,
	i2s_mappage,
	i2s_get_props,
	i2s_trigger_output,
	i2s_trigger_input,
};

struct audio_device onyx_device = {
	"ONYX",
	"",
	"onyx"
};

int
onyx_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;
	int soundbus, soundchip;
	char compat[32];

	if (strcmp(ca->ca_name, "i2s") != 0)
		return (0);

	if ((soundbus = OF_child(ca->ca_node)) == 0 ||
	    (soundchip = OF_child(soundbus)) == 0)
		return (0);

	bzero(compat, sizeof compat);
	OF_getprop(soundchip, "compatible", compat, sizeof compat);

	if (strcmp(compat, "AOAbase") == 0)
		return (1);

	return (0);
}

void
onyx_attach(struct device *parent, struct device *self, void *aux)
{
	struct onyx_softc *sc = (struct onyx_softc *)self;

	sc->sc_setvolume = onyx_set_volume;

	i2s_attach(parent, sc, aux);
	config_defer(self, onyx_defer);
}

void
onyx_defer(struct device *dev)
{
	struct onyx_softc *sc = (struct onyx_softc *)dev;
	struct device *dv;

	TAILQ_FOREACH(dv, &alldevs, dv_list)
		if (strncmp(dv->dv_xname, "ki2c", 4) == 0 &&
		    strncmp(dv->dv_parent->dv_xname, "macobio", 7) == 0)
			sc->sc_i2c = dv;
	if (sc->sc_i2c == NULL) {
		printf("%s: unable to find i2c\n", sc->sc_dev.dv_xname);
		return;
	}

	/* XXX If i2c has failed to attach, what should we do? */

	audio_attach_mi(&onyx_hw_if, sc, &sc->sc_dev);

	deq_reset(sc);
	onyx_set_volume(sc, 192, 192);
}

int
onyx_getdev(void *h, struct audio_device *retp)
{
	*retp = onyx_device;
	return (0);
}

void
onyx_set_volume(struct onyx_softc *sc, int left, int right)
{
	u_int8_t data;

	sc->sc_vol_l = left;
	sc->sc_vol_r = right;

	ki2c_setmode(sc->sc_i2c, I2C_STDSUBMODE, 0);
	data = 128 + (left >> 1);
	ki2c_write(sc->sc_i2c, PCM3052_I2C_ADDR,
	    PCM3052_REG_LEFT_VOLUME, &data, 1);
	data = 128 + (right >> 1);
	ki2c_write(sc->sc_i2c, PCM3052_I2C_ADDR,
	    PCM3052_REG_RIGHT_VOLUME, &data, 1);
}
