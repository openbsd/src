/*	$OpenBSD: pl031.c,v 1.1 2015/06/14 05:01:31 jsg Exp $	*/

/*
 * Copyright (c) 2015 Jonathan Gray <jsg@openbsd.org>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <armv7/armv7/armv7var.h>
#include <dev/clock_subr.h>

#define RTCDR	0x00
#define RTCMR	0x04
#define RTCLR	0x08
#define RTCCR	0x0c
#define RTCIMSC	0x10
#define RTCRIS	0x14
#define RTCMIS	0x18
#define RTCICR	0x1c

#define RTCCR_START	(1 << 0)

extern todr_chip_handle_t todr_handle;

struct plrtc_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
};

void	plrtc_attach(struct device *, struct device *, void *);
int	plrtc_gettime(struct todr_chip_handle *, struct timeval *);
int	plrtc_settime(struct todr_chip_handle *, struct timeval *);


struct cfattach plrtc_ca = {
	sizeof(struct plrtc_softc), NULL , plrtc_attach
};

struct cfdriver plrtc_cd = {
	NULL, "plrtc", DV_DULL
};

int
plrtc_gettime(todr_chip_handle_t handle, struct timeval *tv)
{
	struct plrtc_softc	*sc = handle->cookie;
	uint32_t		 tod;

	tod = bus_space_read_4(sc->sc_iot, sc->sc_ioh, RTCDR);

	tv->tv_sec = tod;
	tv->tv_usec = 0;

	return (0);
}

int
plrtc_settime(todr_chip_handle_t handle, struct timeval *tv)
{
	struct plrtc_softc	*sc = handle->cookie;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, RTCLR, tv->tv_sec);

	return (0);
}

void
plrtc_attach(struct device *parent, struct device *self, void *aux)
{
	struct armv7_attach_args	*aa = aux;
	struct plrtc_softc		*sc = (struct plrtc_softc *) self;
	todr_chip_handle_t		 handle;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh)) {
		printf(": failed to map mem space\n");
		return;
	}

	handle = malloc(sizeof(struct todr_chip_handle), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (handle == NULL)
		panic("couldn't allocate todr_handle");

	handle->cookie = sc;
	handle->todr_gettime = plrtc_gettime;
	handle->todr_settime = plrtc_settime;
	todr_handle = handle;

	/* enable the rtc */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, RTCCR, RTCCR_START);

	printf("\n");
}
