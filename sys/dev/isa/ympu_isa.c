/*	$Id: ympu_isa.c,v 1.1 2002/11/28 22:37:20 mickey Exp $	*/

/*
 * Copyright (c) 2002 Sergey Smitienko. All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/mpuvar.h>

int	ympu_isa_match(struct device *, void *, void *);
void	ympu_isa_attach(struct device *, struct device *, void *);
int	ympu_test(bus_space_tag_t, int);

#ifdef	AUDIO_DEBUG
#define	DPRINTF(x)	if (ympu_debug) printf x
int	ympu_debug = 0;
#else
#define	DPRINTF(x)
#endif

#define MPU_GETSTATUS(iot, ioh) (bus_space_read_1(iot, ioh, MPU_STATUS))

struct ympu_isa_softc {
	struct device sc_dev;

	struct mpu_softc sc_mpu;
};

struct	cfdriver ympu_cd = {
	NULL, "ympu", DV_DULL
};

struct cfattach ympu_isa_ca = {
	sizeof(struct ympu_isa_softc), ympu_isa_match, ympu_isa_attach
};

int
ympu_test (iot, iobase)
	bus_space_tag_t iot;
	int iobase;	/* base port number to try */
{
	bus_space_handle_t ioh;
	int	i, rc;

	rc = 0;
	if (bus_space_map(iot, iobase, MPU401_NPORT, 0, &ioh)) {
		DPRINTF(("ympu_test: can`t map: %x/2\n", iobase));
		return (0);
	}

	DPRINTF(("ympu_test: trying: %x\n", iobase));

	/*
	 * The following code is a shameless copy of mpu401.c
	 * it is here until a redesign of mpu_find() interface
	 */

	if (MPU_GETSTATUS(iot, ioh) == 0xff)
		goto done;

	for (i = 0; i < MPU_MAXWAIT; i++) {
		if (!(MPU_GETSTATUS(iot, ioh) & MPU_OUTPUT_BUSY))
			goto done;
		delay (10);
	}
	bus_space_write_1(iot, ioh, MPU_COMMAND, MPU_RESET);

	for (i = 0; i < 2 * MPU_MAXWAIT; i++)
		if (!(MPU_GETSTATUS(iot, ioh) & MPU_INPUT_EMPTY) &&
		    bus_space_read_1(iot, ioh, MPU_DATA) == MPU_ACK) {
			rc = 1;
			break;
		}
done:
	bus_space_unmap(iot, ioh, MPU401_NPORT);

	return (rc);
}

int
ympu_isa_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;

        if (ympu_test(ia->ia_iot, ia->ia_iobase)) {
		ia->ia_iosize = MPU401_NPORT;
		return (1);
	}

	return (0);
}

void
ympu_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ympu_isa_softc *sc = (struct ympu_isa_softc *)self;
	struct isa_attach_args *ia = aux;

	sc->sc_mpu.iot = ia->ia_iot;

	if (bus_space_map (ia->ia_iot, ia->ia_iobase, MPU401_NPORT,
	    0, &sc->sc_mpu.ioh)) {
		printf(": can`t map i/o space\n");
		return;
	}

	if (!mpu_find(&sc->sc_mpu)) {
		printf(": find failed\n");
		return;
	}

	printf(": generic MPU-401 compatible\n");

	midi_attach_mi(&mpu_midi_hw_if, &sc->sc_mpu, &sc->sc_dev);
}
