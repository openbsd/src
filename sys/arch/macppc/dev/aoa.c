/*	$OpenBSD: aoa.c,v 1.6 2010/02/26 21:52:14 jasper Exp $	*/

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
 * WORK-IN-PROGRESS AOAKeylargo audio driver.
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

#ifdef AOA_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

/* XXX */
#define aoa_softc i2s_softc

int aoa_getdev(void *, struct audio_device *);
int aoa_match(struct device *, void *, void *);
void aoa_attach(struct device *, struct device *, void *);
void aoa_set_volume(struct aoa_softc *, int, int);
void aoa_get_default_params(void *, int, struct audio_params *);

struct cfattach aoa_ca = {
	sizeof(struct aoa_softc), aoa_match, aoa_attach
};

struct cfdriver aoa_cd = {
	NULL, "aoa", DV_DULL
};

struct audio_hw_if aoa_hw_if = {
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
	aoa_getdev,
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
	aoa_get_default_params
};

struct audio_device aoa_device = {
	"AOA",
	"",
	"aoa"
};

int
aoa_match(struct device *parent, void *match, void *aux)
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

	if (strcmp(compat, "AOAKeylargo") == 0)
		return (1);
	if (strcmp(compat, "AOAK2") == 0)
		return (1);

	return (0);
}

void
aoa_attach(struct device *parent, struct device *self, void *aux)
{
	struct aoa_softc *sc = (struct aoa_softc *)self;

	sc->sc_setvolume = aoa_set_volume;

	i2s_attach(parent, sc, aux);
	audio_attach_mi(&aoa_hw_if, sc, &sc->sc_dev);
}

int
aoa_getdev(void *h, struct audio_device *retp)
{
	*retp = aoa_device;
	return (0);
}

void
aoa_set_volume(struct aoa_softc *sc, int left, int right)
{
	/* This device doesn't provide volume control. */
}

void
aoa_get_default_params(void *addr, int mode, struct audio_params *params)
{
	i2s_get_default_params(params);
}
