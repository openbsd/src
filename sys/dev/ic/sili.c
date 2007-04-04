/*	$OpenBSD: sili.c,v 1.6 2007/04/04 10:49:42 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <machine/bus.h>

#include <dev/ata/atascsi.h>

#include <dev/ic/silireg.h>
#include <dev/ic/silivar.h>

struct cfdriver sili_cd = {
	NULL, "sili", DV_DULL
};

struct sili_port {
	struct sili_softc	*sp_sc;
	bus_space_handle_t	sp_ioh;
};

int			sili_ports_alloc(struct sili_softc *);
void			sili_ports_free(struct sili_softc *);

u_int32_t		sili_read(struct sili_softc *, bus_size_t);
void			sili_write(struct sili_softc *, bus_size_t, u_int32_t);
u_int32_t		sili_pread(struct sili_port *, bus_size_t);
void			sili_pwrite(struct sili_port *, bus_size_t, u_int32_t);

/* atascsi interface */
int			sili_ata_probe(void *, int);
struct ata_xfer		*sili_ata_get_xfer(void *, int);
void			sili_ata_put_xfer(struct ata_xfer *);
int			sili_ata_cmd(struct ata_xfer *);

struct atascsi_methods sili_atascsi_methods = {
	sili_ata_probe,
	sili_ata_get_xfer,
	sili_ata_cmd
};

int
sili_attach(struct sili_softc *sc)
{
	struct atascsi_attach_args	aaa;

	printf("\n");

	if (sili_ports_alloc(sc) != 0) {
		/* error already printed by sili_port_alloc */
		return (1);
	}

	/* bounce the controller */
	sili_write(sc, SILI_REG_GC, SILI_REG_GC_GR);
	sili_write(sc, SILI_REG_GC, 0x0);

	bzero(&aaa, sizeof(aaa));
	aaa.aaa_cookie = sc;
	aaa.aaa_methods = &sili_atascsi_methods;
	aaa.aaa_minphys = minphys;
	aaa.aaa_nports = sc->sc_nports;
	aaa.aaa_ncmds = SILI_MAX_CMDS;

	sc->sc_atascsi = atascsi_attach(&sc->sc_dev, &aaa);

	return (0);
}

int
sili_detach(struct sili_softc *sc, int flags)
{
	return (0);
}

int
sili_intr(void *arg)
{
#if 0
	struct sili_softc		*sc = arg;
#endif

	return (0);
}

int
sili_ports_alloc(struct sili_softc *sc)
{
	struct sili_port		*sp;
	int				i;

	sc->sc_ports = malloc(sizeof(struct sili_port) * sc->sc_nports,
	    M_DEVBUF, M_WAITOK);
	bzero(sc->sc_ports, sizeof(struct sili_port) * sc->sc_nports);

	for (i = 0; i < sc->sc_nports; i++) {
		sp = &sc->sc_ports[i];

		sp->sp_sc = sc;
		if (bus_space_subregion(sc->sc_iot_port, sc->sc_ioh_port,
		    SILI_PORT_OFFSET(i), SILI_PORT_SIZE, &sp->sp_ioh) != 0) {
			printf("%s: unable to create register window "
			    "for port %d\n", DEVNAME(sc), i);
			goto freeports;
		}
	}

	return (0);

freeports:
	/* bus_space(9) says subregions dont have to be freed */
	free(sp, M_DEVBUF);
	sc->sc_ports = NULL;
	return (1);
}

void
sili_ports_free(struct sili_softc *sc)
{
	/* bus_space(9) says subregions dont have to be freed */
	free(sc->sc_ports, M_DEVBUF);
	sc->sc_ports = NULL;
}

u_int32_t
sili_read(struct sili_softc *sc, bus_size_t r)
{
	u_int32_t			rv;

	bus_space_barrier(sc->sc_iot_global, sc->sc_ioh_global, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->sc_iot_global, sc->sc_ioh_global, r);

	return (rv);
}

void
sili_write(struct sili_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_iot_global, sc->sc_ioh_global, r, v);
	bus_space_barrier(sc->sc_iot_global, sc->sc_ioh_global, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
sili_pread(struct sili_port *sp, bus_size_t r)
{
	u_int32_t			rv;

	bus_space_barrier(sp->sp_sc->sc_iot_port, sp->sp_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sp->sp_sc->sc_iot_port, sp->sp_ioh, r);

	return (rv);
}

void
sili_pwrite(struct sili_port *sp, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sp->sp_sc->sc_iot_port, sp->sp_ioh, r, v);
	bus_space_barrier(sp->sp_sc->sc_iot_port, sp->sp_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
sili_ata_probe(void *xsc, int port)
{
	return (ATA_PORT_T_NONE);
}

int
sili_ata_cmd(struct ata_xfer *xa)
{
	return (ATA_ERROR);
}

struct ata_xfer *
sili_ata_get_xfer(void *xsc, int port)
{
	return (NULL);
}
