/*	$OpenBSD: hilid.c,v 1.1 2003/02/15 23:50:02 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/hil/hilreg.h>
#include <dev/hil/hilvar.h>
#include <dev/hil/hildevs.h>

struct hilid_softc {
	struct device	sc_dev;

	u_int8_t	sc_id[16];
};

int	hilidprobe(struct device *, void *, void *);
void	hilidattach(struct device *, struct device *, void *);

struct cfdriver hilid_cd = {
	NULL, "hilid", DV_DULL
};

struct cfattach hilid_ca = {
	sizeof(struct hilid_softc), hilidprobe, hilidattach
};

int
hilidprobe(struct device *parent, void *match, void *aux)
{
	struct hil_attach_args *ha = aux;

	if (ha->ha_type != HIL_DEVICE_IDMODULE)
		return (0);

	return (1);
}

void
hilidattach(struct device *parent, struct device *self, void *aux)
{
	struct hilid_softc *sc = (void *)self;
	struct hil_attach_args *ha = aux;
	u_int i, len;

	printf("\n");

	bzero(sc->sc_id, sizeof(sc->sc_id));
	len = sizeof(sc->sc_id);
	send_hildev_cmd((struct hil_softc *)parent, ha->ha_code,
	    HIL_SECURITY, sc->sc_id, &len);

	printf("%s: security code", sc->sc_dev.dv_xname);
	for (i = 0; i < sizeof(sc->sc_id); i++)
		printf(" %02.2x", sc->sc_id[i]);

	printf("\n");
}
