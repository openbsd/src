/*	$OpenBSD: sili.c,v 1.11 2007/04/05 10:45:25 dlg Exp $ */

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

#define SILI_DEBUG

#ifdef SILI_DEBUG
#define SILI_D_VERBOSE		(1<<0)

int silidebug = SILI_D_VERBOSE;

#define DPRINTF(m, a...)	do { if ((m) & silidebug) printf(a); } while (0)
#else
#define DPRINTF(m, a...)
#endif

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
int			sili_pwait_eq(struct sili_port *, bus_size_t,
			    u_int32_t, u_int32_t, int);
int			sili_pwait_ne(struct sili_port *, bus_size_t,
			    u_int32_t, u_int32_t, int);
void			sili_post_direct(struct sili_port *, u_int,
			    void *, size_t buflen);
u_int32_t		sili_signature(struct sili_port *, u_int);

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
sili_pwait_eq(struct sili_port *sp, bus_size_t r, u_int32_t mask, 
    u_int32_t value, int timeout)
{
	while ((sili_pread(sp, r) & mask) != value) {
		if (timeout == 0)
			return (0);

		delay(1000);
		timeout--;
	}

	return (1);
}

int
sili_pwait_ne(struct sili_port *sp, bus_size_t r, u_int32_t mask, 
    u_int32_t value, int timeout)
{
	while ((sili_pread(sp, r) & mask) == value) {
		if (timeout == 0)
			return (0);

		delay(1000);
		timeout--;
	}

	return (1);
}

void
sili_post_direct(struct sili_port *sp, u_int slot, void *buf, size_t buflen)
{
	bus_size_t			r = SILI_PREG_SLOT(slot);

#ifdef DIAGNOSTIC
	if (buflen != 64 && buflen != 128)
		panic("sili_pcopy: buflen of %d is not 64 or 128", buflen);
#endif

	bus_space_write_raw_region_4(sp->sp_sc->sc_iot_port, sp->sp_ioh, r,
	    buf, buflen);
	bus_space_barrier(sp->sp_sc->sc_iot_port, sp->sp_ioh, r, buflen,
	    BUS_SPACE_BARRIER_WRITE);

	sili_pwrite(sp, SILI_PREG_FIFO, slot);
}

u_int32_t
sili_signature(struct sili_port *sp, u_int slot)
{
	u_int32_t			sig_hi, sig_lo;

	sig_hi = sili_pread(sp, SILI_PREG_SIG_HI(slot));
	sig_hi <<= SILI_PREG_SIG_HI_SHIFT;
	sig_lo = sili_pread(sp, SILI_PREG_SIG_LO(slot));
	sig_lo &= SILI_PREG_SIG_LO_MASK;

	return (sig_hi | sig_lo);
}

int
sili_ata_probe(void *xsc, int port)
{
	struct sili_softc		*sc = xsc;
	struct sili_port		*sp = &sc->sc_ports[port];
	struct sili_prb_softreset	sreset;
	u_int32_t			signature;

	sili_pwrite(sp, SILI_PREG_PCC, SILI_PREG_PCC_PORTRESET);
	sili_pwrite(sp, SILI_PREG_PCS, SILI_PREG_PCS_A32B);

	if (!sili_pwait_eq(sp, SILI_PREG_SSTS, SATA_SStatus_DET,
	    SATA_SStatus_DET_DEV, 1000))
		return (ATA_PORT_T_NONE);

	DPRINTF(SILI_D_VERBOSE, "%s.%d: SSTS 0x%08x\n", DEVNAME(sc), port,
	    sili_pread(sp, SILI_PREG_SSTS));

	bzero(&sreset, sizeof(sreset));
	sreset.control = htole16(SILI_PRB_SOFT_RESET | SILI_PRB_INTERRUPT_MASK);
	/* XXX sreset fis pmp field */

	/* we use slot 0 */
	sili_post_direct(sp, 0, &sreset, sizeof(sreset));
	if (!sili_pwait_eq(sp, SILI_PREG_PSS, (1 << 0), 0, 1000)) {
		/* DPRINTF timeout waiting for soft reset */
		return (ATA_PORT_T_NONE);
	}

        /* Read device signature from command slot. */
	signature = sili_signature(sp, 0);

	DPRINTF(SILI_D_VERBOSE, "%s.%d: signature 0x%08x\n", DEVNAME(sc), port,
	    signature);

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
