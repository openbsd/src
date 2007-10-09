/*	$OpenBSD: sili.c,v 1.35 2007/10/09 05:43:37 ray Exp $ */

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

/* use SILI_DEBUG for dmesg spam */
#define NO_SILI_DEBUG

#ifdef SILI_DEBUG
#define SILI_D_VERBOSE		(1<<0)
#define SILI_D_INTR		(1<<1)

int silidebug = SILI_D_VERBOSE;

#define DPRINTF(m, a...)	do { if ((m) & silidebug) printf(a); } while (0)
#else
#define DPRINTF(m, a...)
#endif

struct cfdriver sili_cd = {
	NULL, "sili", DV_DULL
};

/* wrapper around dma memory */
struct sili_dmamem {
	bus_dmamap_t		sdm_map;
	bus_dma_segment_t	sdm_seg;
	size_t			sdm_size;
	caddr_t			sdm_kva;
};
#define SILI_DMA_MAP(_sdm)	((_sdm)->sdm_map)
#define SILI_DMA_DVA(_sdm)	((_sdm)->sdm_map->dm_segs[0].ds_addr)
#define SILI_DMA_KVA(_sdm)	((void *)(_sdm)->sdm_kva)

struct sili_dmamem	*sili_dmamem_alloc(struct sili_softc *, bus_size_t,
			    bus_size_t);
void			sili_dmamem_free(struct sili_softc *,
			    struct sili_dmamem *);

/* per port goo */
struct sili_ccb;

/* size of scratch space for use in error recovery. */
#define SILI_SCRATCH_LEN	512	/* must be at least 1 sector */

struct sili_port {
	struct sili_softc	*sp_sc;
	bus_space_handle_t	sp_ioh;

	struct sili_ccb		*sp_ccbs;
	struct sili_dmamem	*sp_cmds;
	struct sili_dmamem	*sp_scratch;

	TAILQ_HEAD(, sili_ccb)	sp_free_ccbs;

	volatile u_int32_t	sp_active;
	TAILQ_HEAD(, sili_ccb)	sp_active_ccbs;
	TAILQ_HEAD(, sili_ccb)	sp_deferred_ccbs;

#ifdef SILI_DEBUG
	char			sp_name[16];
#define PORTNAME(_sp)	((_sp)->sp_name)
#else
#define PORTNAME(_sp)	DEVNAME((_sp)->sp_sc)
#endif
};

int			sili_ports_alloc(struct sili_softc *);
void			sili_ports_free(struct sili_softc *);

/* ccb shizz */

/*
 * the dma memory for each command will be made up of a prb followed by
 * 7 sgts, this is a neat 512 bytes.
 */
#define SILI_CMD_LEN		512

/*
 * you can fit 22 sge's into 7 sgts and a prb:
 * there's 1 sgl in an atapi prb (two in the ata one, but we cant over
 * advertise), but that's needed for the chain element. you get three sges
 * per sgt cos you lose the 4th sge for the chaining, but you keep it in
 * the last sgt. so 3 x 6 + 4 is 22.
 */
#define SILI_DMA_SEGS		22

struct sili_ccb {
	struct ata_xfer		ccb_xa;

	void			*ccb_cmd;
	u_int64_t		ccb_cmd_dva;
	bus_dmamap_t		ccb_dmamap;

	struct sili_port	*ccb_port;

	TAILQ_ENTRY(sili_ccb)	ccb_entry;
};

int			sili_ccb_alloc(struct sili_port *);
void			sili_ccb_free(struct sili_port *);
struct sili_ccb		*sili_get_ccb(struct sili_port *);
void			sili_put_ccb(struct sili_ccb *);

/* bus space ops */
u_int32_t		sili_read(struct sili_softc *, bus_size_t);
void			sili_write(struct sili_softc *, bus_size_t, u_int32_t);
u_int32_t		sili_pread(struct sili_port *, bus_size_t);
void			sili_pwrite(struct sili_port *, bus_size_t, u_int32_t);
int			sili_pwait_eq(struct sili_port *, bus_size_t,
			    u_int32_t, u_int32_t, int);
int			sili_pwait_ne(struct sili_port *, bus_size_t,
			    u_int32_t, u_int32_t, int);

/* command handling */
void			sili_post_direct(struct sili_port *, u_int,
			    void *, size_t buflen);
void			sili_post_indirect(struct sili_port *,
			    struct sili_ccb *);
void			sili_pread_fis(struct sili_port *, u_int,
			    struct ata_fis_d2h *);
u_int32_t		sili_signature(struct sili_port *, u_int);
int			sili_load(struct sili_ccb *, struct sili_sge *, int);
void			sili_unload(struct sili_ccb *);
int			sili_poll(struct sili_ccb *, int, void (*)(void *));
void			sili_start(struct sili_port *, struct sili_ccb *);
int			sili_read_ncq_error(struct sili_port *, int *);

/* port interrupt handler */
u_int32_t		sili_port_intr(struct sili_port *, int);

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

/* completion paths */
void			sili_ata_cmd_done(struct sili_ccb *, int);
void			sili_ata_cmd_timeout(void *);

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
	aaa.aaa_capability = ASAA_CAP_NCQ;

	sc->sc_atascsi = atascsi_attach(&sc->sc_dev, &aaa);

	return (0);
}

int
sili_detach(struct sili_softc *sc, int flags)
{
	return (0);
}

u_int32_t
sili_port_intr(struct sili_port *sp, int timeout_slot)
{
	u_int32_t			is, pss_saved, pss_masked;
	u_int32_t			processed = 0, need_restart = 0;
	int				slot;
	struct sili_ccb			*ccb;

	is = sili_pread(sp, SILI_PREG_IS);
	pss_saved = sili_pread(sp, SILI_PREG_PSS); /* reading acks CMDCOMP */

#ifdef SILI_DEBUG
	if ((pss_saved & SILI_PREG_PSS_ALL_SLOTS) != sp->sp_active ||
	    ((is >> 16) & ~SILI_PREG_IS_CMDCOMP)) {
		DPRINTF(SILI_D_INTR, "%s: IS: 0x%08x (0x%b), PSS: %08x, "
		    "active: %08x\n", PORTNAME(sp), is, is >> 16, SILI_PFMT_IS,
		    pss_saved, sp->sp_active);
	}
#endif

	/* Only interested in slot status bits. */
	pss_saved &= SILI_PREG_PSS_ALL_SLOTS;

	if (is & SILI_PREG_IS_CMDERR) {
		int			err_slot, err_code;
		u_int32_t		sactive = 0;

		sili_pwrite(sp, SILI_PREG_IS, SILI_PREG_IS_CMDERR);
		err_slot = SILI_PREG_PCS_ACTIVE(sili_pread(sp, SILI_PREG_PCS));
		err_code = sili_pread(sp, SILI_PREG_CE);
		ccb = &sp->sp_ccbs[err_slot];

		switch (err_code) {
		case SILI_PREG_CE_DEVICEERROR:
		case SILI_PREG_CE_DATAFISERROR:
			/* Extract error from command slot in LRAM. */
			sili_pread_fis(sp, err_slot, &ccb->ccb_xa.rfis);
			break;

		case SILI_PREG_CE_SDBERROR:
			/* No NCQ commands active?  Treat as a normal error. */
			sactive = sili_pread(sp, SILI_PREG_SACT);/* XXX Pmult */
			if (sactive == 0)
				break;

			/* Extract real NCQ error slot & RFIS from log page. */
			if (!sili_read_ncq_error(sp, &err_slot)) {
				/* got real err_slot */
				ccb = &sp->sp_ccbs[err_slot];
				break;
			}

			/* failed to get error or not NCQ */

			/* FALLTHROUGH */
		default:
			/* All other error types are fatal. */
			printf("%s: fatal error (%d), aborting active slots "
			    "(%08x) and resetting device.\n", PORTNAME(sp),
			    err_code, pss_saved);
			while (pss_saved) {
				slot = ffs(pss_saved) - 1;
				pss_saved &= ~(1 << slot);

				ccb = &sp->sp_ccbs[slot];
				KASSERT(ccb->ccb_xa.state == ATA_S_ONCHIP);
				ccb->ccb_xa.state = ATA_S_ERROR;
			}
			need_restart = SILI_PREG_PCS_DEVRESET;
			goto fatal;
		}

		DPRINTF(SILI_D_VERBOSE, "%s: %serror, code %d, slot %d, "
		    "active %08x\n", PORTNAME(sp), sactive ? "NCQ " : "",
		    err_code, err_slot, sp->sp_active);

		/* Clear the failed commmand in saved PSS so cmd_done runs. */
		pss_saved &= ~(1 << err_slot);

		KASSERT(ccb->ccb_xa.state == ATA_S_ONCHIP);
		ccb->ccb_xa.state = ATA_S_ERROR;

		need_restart = SILI_PREG_PCS_PORTINIT;
	}
fatal:

	/* Process command timeout request only if command is still active. */
	if (timeout_slot >= 0 && (pss_saved & (1 << timeout_slot))) {
		DPRINTF(SILI_D_VERBOSE, "%s: timing out slot %d, active %08x\n",
		    PORTNAME(sp), timeout_slot, sp->sp_active);

		/* Clear the failed commmand in saved PSS so cmd_done runs. */
		pss_saved &= ~(1 << timeout_slot);

		ccb = &sp->sp_ccbs[timeout_slot];
		KASSERT(ccb->ccb_xa.state == ATA_S_ONCHIP);
		ccb->ccb_xa.state = ATA_S_TIMEOUT;

		/* Reset device to abort all commands (including this one). */
		need_restart = SILI_PREG_PCS_DEVRESET;
	}

	/* Command slot is complete if its bit in PSS is 0 but 1 in active. */
	pss_masked = ~pss_saved & sp->sp_active;
	while (pss_masked) {
		slot = ffs(pss_masked) - 1;
		ccb = &sp->sp_ccbs[slot];
		pss_masked &= ~(1 << slot);

		DPRINTF(SILI_D_INTR, "%s: slot %d is complete%s\n",
		    PORTNAME(sp), slot, ccb->ccb_xa.state == ATA_S_ERROR ?
		    " (error)" : (ccb->ccb_xa.state == ATA_S_TIMEOUT ?
		    " (timeout)" : ""));

		sili_ata_cmd_done(ccb, need_restart);

		processed |= 1 << slot;
	}

	if (need_restart) {
		/* Re-enable transfers on port. */
		sili_pwrite(sp, SILI_PREG_PCS, need_restart);
		if (!sili_pwait_eq(sp, SILI_PREG_PCS, SILI_PREG_PCS_PORTRDY,
		    SILI_PREG_PCS_PORTRDY, 1000)) {
			printf("%s: couldn't restart port after error\n",
			    PORTNAME(sp));
		}

		/* Restart CCBs in the order they were originally queued. */
		pss_masked = pss_saved;
		TAILQ_FOREACH(ccb, &sp->sp_active_ccbs, ccb_entry) {
			DPRINTF(SILI_D_VERBOSE, "%s: restarting slot %d "
			    "after error, state %02x\n", PORTNAME(sp),
			    ccb->ccb_xa.tag, ccb->ccb_xa.state);
			if (!(pss_masked & (1 << ccb->ccb_xa.tag))) {
				panic("sili_intr: slot %d not active in "
				    "pss_masked: %08x, state %02x",
				    ccb->ccb_xa.tag, pss_masked,
				    ccb->ccb_xa.state);
			}
			pss_masked &= ~(1 << ccb->ccb_xa.tag);

			KASSERT(ccb->ccb_xa.state == ATA_S_ONCHIP);
			sili_post_indirect(sp, ccb);
		}
		KASSERT(pss_masked == 0);

		/*
		 * Finally, run atascsi completion for any finished CCBs.  If
		 * we had run these during cmd_done above, any ccbs that their
		 * completion generated would have been activated out of order.
		 */
		while ((ccb = TAILQ_FIRST(&sp->sp_deferred_ccbs)) != NULL) {
			TAILQ_REMOVE(&sp->sp_deferred_ccbs, ccb, ccb_entry);

			KASSERT(ccb->ccb_xa.state == ATA_S_COMPLETE ||
			    ccb->ccb_xa.state == ATA_S_ERROR ||
			    ccb->ccb_xa.state == ATA_S_TIMEOUT);
			ccb->ccb_xa.complete(&ccb->ccb_xa);
		}
	}

	return (processed);
}

int
sili_intr(void *arg)
{
	struct sili_softc		*sc = arg;
	u_int32_t			is;
	int				port;

	is = sili_read(sc, SILI_REG_GIS);
	if (is == 0)
		return (0);
	sili_write(sc, SILI_REG_GIS, is);
	DPRINTF(SILI_D_INTR, "sili_intr, GIS: %08x\n", is);

	while (is & SILI_REG_GIS_PIS_MASK) {
		port = ffs(is) - 1;
		sili_port_intr(&sc->sc_ports[port], -1);
		is &= ~(1 << port);
	}

	return (1);
}

int
sili_ports_alloc(struct sili_softc *sc)
{
	struct sili_port		*sp;
	int				i;

	sc->sc_ports = malloc(sizeof(struct sili_port) * sc->sc_nports,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	for (i = 0; i < sc->sc_nports; i++) {
		sp = &sc->sc_ports[i];

		sp->sp_sc = sc;
#ifdef SILI_DEBUG
		snprintf(sp->sp_name, sizeof(sp->sp_name), "%s.%d",
		    DEVNAME(sc), i);
#endif
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
	struct sili_port		*sp;
	int				i;

	for (i = 0; i < sc->sc_nports; i++) {
		sp = &sc->sc_ports[i];

		if (sp->sp_ccbs != NULL)
			sili_ccb_free(sp);
	}

	/* bus_space(9) says subregions dont have to be freed */
	free(sc->sc_ports, M_DEVBUF);
	sc->sc_ports = NULL;
}

int
sili_ccb_alloc(struct sili_port *sp)
{
	struct sili_softc		*sc = sp->sp_sc;
	struct sili_ccb			*ccb;
	struct sili_prb			*prb;
	int				i;

	TAILQ_INIT(&sp->sp_free_ccbs);
	TAILQ_INIT(&sp->sp_active_ccbs);
	TAILQ_INIT(&sp->sp_deferred_ccbs);

	sp->sp_ccbs = malloc(sizeof(struct sili_ccb) * SILI_MAX_CMDS,
	    M_DEVBUF, M_WAITOK);
	sp->sp_cmds = sili_dmamem_alloc(sc, SILI_CMD_LEN * SILI_MAX_CMDS,
	    SILI_PRB_ALIGN);
	if (sp->sp_cmds == NULL)
		goto free_ccbs;
	sp->sp_scratch = sili_dmamem_alloc(sc, SILI_SCRATCH_LEN, PAGE_SIZE);
	if (sp->sp_scratch == NULL)
		goto free_cmds;

	bzero(sp->sp_ccbs, sizeof(struct sili_ccb) * SILI_MAX_CMDS);

	for (i = 0; i < SILI_MAX_CMDS; i++) {
		ccb = &sp->sp_ccbs[i];
		ccb->ccb_port = sp;
		ccb->ccb_cmd = SILI_DMA_KVA(sp->sp_cmds) + i * SILI_CMD_LEN;
		ccb->ccb_cmd_dva = SILI_DMA_DVA(sp->sp_cmds) + i * SILI_CMD_LEN;
		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS, SILI_DMA_SEGS,
		    MAXPHYS, 0, BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap) != 0)
			goto free_scratch;

		prb = ccb->ccb_cmd;
		ccb->ccb_xa.fis = (struct ata_fis_h2d *)&prb->fis;
		ccb->ccb_xa.packetcmd = ((struct sili_prb_packet *)prb)->cdb;
		ccb->ccb_xa.tag = i;
		ccb->ccb_xa.ata_put_xfer = sili_ata_put_xfer;
		ccb->ccb_xa.state = ATA_S_COMPLETE;

		sili_put_ccb(ccb);
	}

	return (0);

free_scratch:
	sili_dmamem_free(sc, sp->sp_scratch);
free_cmds:
	sili_dmamem_free(sc, sp->sp_cmds);
free_ccbs:
	sili_ccb_free(sp);
	return (1);
}

void
sili_ccb_free(struct sili_port *sp)
{
	struct sili_softc		*sc = sp->sp_sc;
	struct sili_ccb			*ccb;

	while ((ccb = sili_get_ccb(sp)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);

	free(sp->sp_ccbs, M_DEVBUF);
	sp->sp_ccbs = NULL;
}

struct sili_ccb *
sili_get_ccb(struct sili_port *sp)
{
	struct sili_ccb			*ccb;

	ccb = TAILQ_FIRST(&sp->sp_free_ccbs);
	if (ccb != NULL) {
		KASSERT(ccb->ccb_xa.state == ATA_S_PUT);
		TAILQ_REMOVE(&sp->sp_free_ccbs, ccb, ccb_entry);
		ccb->ccb_xa.state = ATA_S_SETUP;
	}

	return (ccb);
}

void
sili_put_ccb(struct sili_ccb *ccb)
{
	struct sili_port		*sp = ccb->ccb_port;

#ifdef DIAGNOSTIC
	if (ccb->ccb_xa.state != ATA_S_COMPLETE &&
	    ccb->ccb_xa.state != ATA_S_TIMEOUT &&
	    ccb->ccb_xa.state != ATA_S_ERROR) {
		printf("%s: invalid ata_xfer state %02x in sili_put_ccb, "
		    "slot %d\n", PORTNAME(sp), ccb->ccb_xa.state,
		    ccb->ccb_xa.tag);
	}
#endif

	ccb->ccb_xa.state = ATA_S_PUT;
	TAILQ_INSERT_TAIL(&sp->sp_free_ccbs, ccb, ccb_entry);
}

struct sili_dmamem *
sili_dmamem_alloc(struct sili_softc *sc, bus_size_t size, bus_size_t align)
{
	struct sili_dmamem		*sdm;
	int				nsegs;

	sdm = malloc(sizeof(*sdm), M_DEVBUF, M_WAITOK | M_ZERO);
	sdm->sdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &sdm->sdm_map) != 0)
		goto sdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, align, 0, &sdm->sdm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &sdm->sdm_seg, nsegs, size,
	    &sdm->sdm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, sdm->sdm_map, sdm->sdm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(sdm->sdm_kva, size);

	return (sdm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, sdm->sdm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &sdm->sdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, sdm->sdm_map);
sdmfree:
	free(sdm, M_DEVBUF);

	return (NULL);
}

void
sili_dmamem_free(struct sili_softc *sc, struct sili_dmamem *sdm)
{
	bus_dmamap_unload(sc->sc_dmat, sdm->sdm_map);
	bus_dmamem_unmap(sc->sc_dmat, sdm->sdm_kva, sdm->sdm_size);
	bus_dmamem_free(sc->sc_dmat, &sdm->sdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, sdm->sdm_map);
	free(sdm, M_DEVBUF);
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
		panic("sili_pcopy: buflen of %zu is not 64 or 128", buflen);
#endif

	bus_space_write_raw_region_4(sp->sp_sc->sc_iot_port, sp->sp_ioh, r,
	    buf, buflen);
	bus_space_barrier(sp->sp_sc->sc_iot_port, sp->sp_ioh, r, buflen,
	    BUS_SPACE_BARRIER_WRITE);

	sili_pwrite(sp, SILI_PREG_FIFO, slot);
}

void
sili_pread_fis(struct sili_port *sp, u_int slot, struct ata_fis_d2h *fis)
{
	bus_size_t			r = SILI_PREG_SLOT(slot) + 8;

	bus_space_barrier(sp->sp_sc->sc_iot_port, sp->sp_ioh, r,
	    sizeof(struct ata_fis_d2h), BUS_SPACE_BARRIER_READ);
	bus_space_read_raw_region_4(sp->sp_sc->sc_iot_port, sp->sp_ioh, r,
	    fis, sizeof(struct ata_fis_d2h));
}

void
sili_post_indirect(struct sili_port *sp, struct sili_ccb *ccb)
{
	sili_pwrite(sp, SILI_PREG_CAR_LO(ccb->ccb_xa.tag),
	    (u_int32_t)ccb->ccb_cmd_dva);
	sili_pwrite(sp, SILI_PREG_CAR_HI(ccb->ccb_xa.tag),
	    (u_int32_t)(ccb->ccb_cmd_dva >> 32));
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
	int				port_type;

	sili_pwrite(sp, SILI_PREG_PCC, SILI_PREG_PCC_PORTRESET);
	sili_pwrite(sp, SILI_PREG_PCC, SILI_PREG_PCC_A32B);

	if (!sili_pwait_eq(sp, SILI_PREG_SSTS, SATA_SStatus_DET,
	    SATA_SStatus_DET_DEV, 1000))
		return (ATA_PORT_T_NONE);

	DPRINTF(SILI_D_VERBOSE, "%s: SSTS 0x%08x\n", PORTNAME(sp),
	    sili_pread(sp, SILI_PREG_SSTS));

	bzero(&sreset, sizeof(sreset));
	sreset.control = htole16(SILI_PRB_SOFT_RESET | SILI_PRB_INTERRUPT_MASK);
	/* XXX sreset fis pmp field */

	/* we use slot 0 */
	sili_post_direct(sp, 0, &sreset, sizeof(sreset));
	if (!sili_pwait_eq(sp, SILI_PREG_PSS, (1 << 0), 0, 1000)) {
		DPRINTF(SILI_D_VERBOSE, "%s: timed out while waiting for soft "
		    "reset\n", PORTNAME(sp));
		return (ATA_PORT_T_NONE);
	}

	/* Read device signature from command slot. */
	signature = sili_signature(sp, 0);

	DPRINTF(SILI_D_VERBOSE, "%s: signature 0x%08x\n", PORTNAME(sp),
	    signature);

	switch (signature) {
	case SATA_SIGNATURE_DISK:
		port_type = ATA_PORT_T_DISK;
		break;
	case SATA_SIGNATURE_ATAPI:
		port_type = ATA_PORT_T_ATAPI;
		break;
	case SATA_SIGNATURE_PORT_MULTIPLIER:
	default:
		return (ATA_PORT_T_NONE);
	}

	/* allocate port resources */
	if (sili_ccb_alloc(sp) != 0)
		return (ATA_PORT_T_NONE);

	/* enable port interrupts */
	sili_write(sc, SILI_REG_GC, sili_read(sc, SILI_REG_GC) | 1 << port);
	sili_pwrite(sp, SILI_PREG_IES, SILI_PREG_IE_CMDERR |
	    SILI_PREG_IE_CMDCOMP);

	return (port_type);
}

int
sili_ata_cmd(struct ata_xfer *xa)
{
	struct sili_ccb			*ccb = (struct sili_ccb *)xa;
	struct sili_port		*sp = ccb->ccb_port;
	struct sili_softc		*sc = sp->sp_sc;
	struct sili_prb_ata		*ata;
	struct sili_prb_packet		*atapi;
	struct sili_sge			*sgl;
	int				sgllen;
	int				s;

	KASSERT(xa->state == ATA_S_SETUP);

	if (xa->flags & ATA_F_PACKET) {
		atapi = ccb->ccb_cmd;

		if (xa->flags & ATA_F_WRITE)
			atapi->control = htole16(SILI_PRB_PACKET_WRITE);
		else
			atapi->control = htole16(SILI_PRB_PACKET_READ);

		sgl = atapi->sgl;
		sgllen = sizeofa(atapi->sgl);
	} else {
		ata = ccb->ccb_cmd;

		ata->control = 0;

		sgl = ata->sgl;
		sgllen = sizeofa(ata->sgl);
	}

	if (sili_load(ccb, sgl, sgllen) != 0)
		goto failcmd;

	bus_dmamap_sync(sc->sc_dmat, SILI_DMA_MAP(sp->sp_cmds),
	    xa->tag * SILI_CMD_LEN, SILI_CMD_LEN, BUS_DMASYNC_PREWRITE);

	timeout_set(&xa->stimeout, sili_ata_cmd_timeout, ccb);

	xa->state = ATA_S_PENDING;

	if (xa->flags & ATA_F_POLL) {
		sili_poll(ccb, xa->timeout, sili_ata_cmd_timeout);
		return (ATA_COMPLETE);
	}

	timeout_add(&xa->stimeout, (xa->timeout * hz) / 1000);

	s = splbio();
	sili_start(sp, ccb);
	splx(s);
	return (ATA_QUEUED);

failcmd:
	s = splbio();
	xa->state = ATA_S_ERROR;
	xa->complete(xa);
	splx(s);
	return (ATA_ERROR);
}

void
sili_ata_cmd_done(struct sili_ccb *ccb, int defer_completion)
{
	struct sili_port		*sp = ccb->ccb_port;
	struct sili_softc		*sc = sp->sp_sc;
	struct ata_xfer			*xa = &ccb->ccb_xa;

	splassert(IPL_BIO);

	timeout_del(&xa->stimeout);

	bus_dmamap_sync(sc->sc_dmat, SILI_DMA_MAP(sp->sp_cmds),
	    xa->tag * SILI_CMD_LEN, SILI_CMD_LEN, BUS_DMASYNC_POSTWRITE);

	sili_unload(ccb);

	TAILQ_REMOVE(&sp->sp_active_ccbs, ccb, ccb_entry);
	sp->sp_active &= ~(1 << xa->tag);

	if (xa->state == ATA_S_ONCHIP)
		xa->state = ATA_S_COMPLETE;
#ifdef DIAGNOSTIC
	else if (xa->state != ATA_S_ERROR && xa->state != ATA_S_TIMEOUT)
		printf("%s: invalid ata_xfer state %02x in sili_ata_cmd_done, "
		    "slot %d\n", PORTNAME(sp), xa->state, xa->tag);
#endif
	if (defer_completion)
		TAILQ_INSERT_TAIL(&sp->sp_deferred_ccbs, ccb, ccb_entry);
	else if (xa->state == ATA_S_COMPLETE)
		xa->complete(xa);
#ifdef DIAGNOSTIC
	else
		printf("%s: completion not deferred, but xa->state is %02x?\n",
		    PORTNAME(sp), xa->state);
#endif
}

void
sili_ata_cmd_timeout(void *xccb)
{
	struct sili_ccb			*ccb = xccb;
	struct sili_port		*sp = ccb->ccb_port;
	int				s;

	s = splbio();
	sili_port_intr(sp, ccb->ccb_xa.tag);
	splx(s);
}

int
sili_load(struct sili_ccb *ccb, struct sili_sge *sgl, int sgllen)
{
	struct sili_port		*sp = ccb->ccb_port;
	struct sili_softc		*sc = sp->sp_sc;
	struct ata_xfer			*xa = &ccb->ccb_xa;
	struct sili_sge			*nsge = sgl, *ce = NULL;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;
	u_int64_t			addr;
	int				error;
	int				i;

	if (xa->datalen == 0)
		return (0);

	error = bus_dmamap_load(sc->sc_dmat, dmap, xa->data, xa->datalen, NULL,
	    (xa->flags & ATA_F_NOWAIT) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: error %d loading dmamap\n", PORTNAME(sp), error);
		return (1);
	}

	if (dmap->dm_nsegs > sgllen)
		ce = &sgl[sgllen - 1];

	for (i = 0; i < dmap->dm_nsegs; i++) {
		if (nsge == ce) {
			nsge++;

			addr = ccb->ccb_cmd_dva;
			addr += ((u_int8_t *)nsge - (u_int8_t *)ccb->ccb_cmd);

			ce->addr_lo = htole32((u_int32_t)addr);
			ce->addr_hi = htole32((u_int32_t)(addr >> 32));
			ce->flags = htole32(SILI_SGE_LNK);

			if ((dmap->dm_nsegs - i) > SILI_SGT_SGLLEN)
				ce += SILI_SGT_SGLLEN;
			else
				ce = NULL;
		}

		sgl = nsge;

		addr = dmap->dm_segs[i].ds_addr;
		sgl->addr_lo = htole32((u_int32_t)addr);
		sgl->addr_hi = htole32((u_int32_t)(addr >> 32));
		sgl->data_count = htole32(dmap->dm_segs[i].ds_len);
		sgl->flags = 0;

		nsge++;
	}
	sgl->flags |= htole32(SILI_SGE_TRM);

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xa->flags & ATA_F_READ) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

void
sili_unload(struct sili_ccb *ccb)
{
	struct sili_port		*sp = ccb->ccb_port;
	struct sili_softc		*sc = sp->sp_sc;
	struct ata_xfer			*xa = &ccb->ccb_xa;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;

	if (xa->datalen == 0)
		return;

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xa->flags & ATA_F_READ) ? BUS_DMASYNC_POSTREAD :
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, dmap);

	if (xa->flags & ATA_F_READ)
		xa->resid = xa->datalen - sili_pread(sp,
		    SILI_PREG_RX_COUNT(xa->tag));
	else
		xa->resid = 0;
}

int
sili_poll(struct sili_ccb *ccb, int timeout, void (*timeout_fn)(void *))
{
	struct sili_port		*sp = ccb->ccb_port;
	int				s;

	s = splbio();
	sili_start(sp, ccb);
	do {
		if (sili_port_intr(sp, -1) & (1 << ccb->ccb_xa.tag)) {
			splx(s);
			return (0);
		}

		delay(1000);
	} while (--timeout > 0);

	/* Run timeout while at splbio, otherwise sili_intr could interfere. */
	if (timeout_fn != NULL)
		timeout_fn(ccb);

	splx(s);

	return (1);
}

void
sili_start(struct sili_port *sp, struct sili_ccb *ccb)
{
	int				slot = ccb->ccb_xa.tag;

	splassert(IPL_BIO);
	KASSERT(ccb->ccb_xa.state == ATA_S_PENDING);

	TAILQ_INSERT_TAIL(&sp->sp_active_ccbs, ccb, ccb_entry);
	sp->sp_active |= 1 << slot;
	ccb->ccb_xa.state = ATA_S_ONCHIP;

	sili_post_indirect(sp, ccb);
}

int
sili_read_ncq_error(struct sili_port *sp, int *err_slotp)
{
	struct sili_softc		*sc = sp->sp_sc;
	struct sili_prb_ata		read_10h;
	u_int64_t			addr;
	struct ata_fis_h2d		*fis;
	struct ata_log_page_10h		*log;
	struct sili_ccb			*ccb;
	int				rc;

	sili_pwrite(sp, SILI_PREG_PCS, SILI_PREG_PCS_PORTINIT);
	if (!sili_pwait_eq(sp, SILI_PREG_PCS, SILI_PREG_PCS_PORTRDY,
	    SILI_PREG_PCS_PORTRDY, 1000)) {
		printf("%s: couldn't ready port during log page read\n",
		    PORTNAME(sp));
		return (1);
	}

	/* READ LOG EXT 10h into scratch space */
	bzero(&read_10h, sizeof(read_10h));
	read_10h.control = htole16(SILI_PRB_INTERRUPT_MASK);

	addr = SILI_DMA_DVA(sp->sp_scratch);
	read_10h.sgl[0].addr_lo = htole32((u_int32_t)addr);
	read_10h.sgl[0].addr_hi = htole32((u_int32_t)(addr >> 32));
	read_10h.sgl[0].data_count = htole32(512);
	read_10h.sgl[0].flags = htole32(SILI_SGE_TRM);

	fis = (struct ata_fis_h2d *)read_10h.fis;
	fis->type = ATA_FIS_TYPE_H2D;
	fis->flags = ATA_H2D_FLAGS_CMD;	/* XXX fis pmp field */
	fis->command = ATA_C_READ_LOG_EXT;
	fis->lba_low = 0x10;		/* queued error log page (10h) */
	fis->sector_count = 1;		/* number of sectors (1) */
	fis->sector_count_exp = 0;
	fis->lba_mid = 0;		/* starting offset */
	fis->lba_mid_exp = 0;
	fis->device = 0;

	bus_dmamap_sync(sc->sc_dmat, SILI_DMA_MAP(sp->sp_scratch), 0,
	    512, BUS_DMASYNC_PREREAD);

	/* issue read and poll for completion */
	sili_post_direct(sp, 0, &read_10h, sizeof(read_10h));
	rc = sili_pwait_eq(sp, SILI_PREG_PSS, (1 << 0), 0, 1000);

	bus_dmamap_sync(sc->sc_dmat, SILI_DMA_MAP(sp->sp_scratch), 0,
	    512, BUS_DMASYNC_POSTREAD);

	if (!rc) {
		DPRINTF(SILI_D_VERBOSE, "%s: timed out while waiting for log "
		    "page read\n", PORTNAME(sp));
		return (1);
	}

	/* Extract failed register set and tags from the scratch space. */
	log = (struct ata_log_page_10h *)SILI_DMA_KVA(sp->sp_scratch);
	if (ISSET(log->err_regs.type, ATA_LOG_10H_TYPE_NOTQUEUED)) {
		/* Not queued bit was set - wasn't an NCQ error? */
		printf("%s: read NCQ error page, but not an NCQ error?\n",
		    PORTNAME(sp));
		return (1);
	}

	/* Copy back the log record as a D2H register FIS. */
	*err_slotp = log->err_regs.type & ATA_LOG_10H_TYPE_TAG_MASK;

	ccb = &sp->sp_ccbs[*err_slotp];
	memcpy(&ccb->ccb_xa.rfis, &log->err_regs, sizeof(struct ata_fis_d2h));
	ccb->ccb_xa.rfis.type = ATA_FIS_TYPE_D2H;
	ccb->ccb_xa.rfis.flags = 0;

	return (0);
}

struct ata_xfer *
sili_ata_get_xfer(void *xsc, int port)
{
	struct sili_softc		*sc = xsc;
	struct sili_port		*sp = &sc->sc_ports[port];
	struct sili_ccb			*ccb;

	ccb = sili_get_ccb(sp);
	if (ccb == NULL) {
		printf("sili_ata_get_xfer: NULL ccb\n");
		return (NULL);
	}

	bzero(ccb->ccb_cmd, SILI_CMD_LEN);

	return ((struct ata_xfer *)ccb);
}

void
sili_ata_put_xfer(struct ata_xfer *xa)
{
	struct sili_ccb			*ccb = (struct sili_ccb *)xa;

	sili_put_ccb(ccb);
}
