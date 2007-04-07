/*	$OpenBSD: sili.c,v 1.19 2007/04/07 13:15:03 pascoe Exp $ */

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

/* wrapper around dma memory */
struct sili_dmamem {
	bus_dmamap_t		sdm_map;
	bus_dma_segment_t	sdm_seg;
	size_t			sdm_size;
	caddr_t			sdm_kva;
};
#define SILI_DMA_MAP(_sdm)      ((_sdm)->sdm_map)
#define SILI_DMA_DVA(_sdm)      ((_sdm)->sdm_map->dm_segs[0].ds_addr)
#define SILI_DMA_KVA(_sdm)      ((void *)(_sdm)->sdm_kva)

struct sili_dmamem	*sili_dmamem_alloc(struct sili_softc *, bus_size_t,
			    bus_size_t);
void			sili_dmamem_free(struct sili_softc *,
			    struct sili_dmamem *);

/* per port goo */
struct sili_ccb;

struct sili_port {
	struct sili_softc	*sp_sc;
	bus_space_handle_t	sp_ioh;

	struct sili_ccb		*sp_ccbs;
	struct sili_dmamem	*sp_cmds;

	TAILQ_HEAD(, sili_ccb)	sp_free_ccbs;

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
struct sili_ccb 	*sili_get_ccb(struct sili_port *);
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
u_int32_t		sili_signature(struct sili_port *, u_int);
int			sili_load(struct sili_ccb *, struct sili_sge *, int);
void			sili_unload(struct sili_ccb *);

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

	sp->sp_ccbs = malloc(sizeof(struct sili_ccb) * SILI_MAX_CMDS,
	    M_DEVBUF, M_WAITOK);
	sp->sp_cmds = sili_dmamem_alloc(sc, SILI_CMD_LEN * SILI_MAX_CMDS,
	    SILI_PRB_ALIGN);
	if (sp->sp_cmds == NULL)
		goto free_ccbs;

	bzero(sp->sp_ccbs, sizeof(struct sili_ccb) * SILI_MAX_CMDS);

	for (i = 0; i < SILI_MAX_CMDS; i++) {
		ccb = &sp->sp_ccbs[i];
		ccb->ccb_port = sp;
		ccb->ccb_cmd = SILI_DMA_KVA(sp->sp_cmds) + i * SILI_CMD_LEN;
		ccb->ccb_cmd_dva = SILI_DMA_DVA(sp->sp_cmds) + i * SILI_CMD_LEN;
		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS, SILI_DMA_SEGS,
		    MAXPHYS, 0, BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap) != 0)
			goto free_cmds;

		prb = ccb->ccb_cmd;
		ccb->ccb_xa.fis = (struct ata_fis_h2d *)&prb->fis;
		ccb->ccb_xa.packetcmd = ((struct sili_prb_packet *)prb)->cdb;
		ccb->ccb_xa.tag = i;
		ccb->ccb_xa.ata_put_xfer = sili_ata_put_xfer;
		ccb->ccb_xa.state = ATA_S_COMPLETE;

		sili_put_ccb(ccb);
	}

	return (0);

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

	sdm = malloc(sizeof(struct sili_dmamem), M_DEVBUF, M_WAITOK);
	bzero(sdm, sizeof(struct sili_dmamem));
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
		panic("sili_pcopy: buflen of %d is not 64 or 128", buflen);
#endif

	bus_space_write_raw_region_4(sp->sp_sc->sc_iot_port, sp->sp_ioh, r,
	    buf, buflen);
	bus_space_barrier(sp->sp_sc->sc_iot_port, sp->sp_ioh, r, buflen,
	    BUS_SPACE_BARRIER_WRITE);

	sili_pwrite(sp, SILI_PREG_FIFO, slot);
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
		/* DPRINTF timeout waiting for soft reset */
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

	if (xa->flags & ATA_F_PACKET) {
		atapi = ccb->ccb_cmd;

		sgl = atapi->sgl;
		sgllen = sizeofa(atapi->sgl);

		xa->state = ATA_S_ERROR;
		return (ATA_ERROR);
	} else {
		ata = ccb->ccb_cmd;

		ata->control = htole16(SILI_PRB_INTERRUPT_MASK);

		sgl = ata->sgl;
		sgllen = sizeofa(ata->sgl);
	}

	if (sili_load(ccb, sgl, sgllen) != 0) {
		xa->state = ATA_S_ERROR;
		return (ATA_ERROR);
	}
	bus_dmamap_sync(sc->sc_dmat, SILI_DMA_MAP(sp->sp_cmds),
	    xa->tag * SILI_CMD_LEN, SILI_CMD_LEN, BUS_DMASYNC_PREWRITE);

#if 0
	sili_post_direct(sp, 0, &ccb->ccb_prb, sizeof(ccb->ccb_prb));
#endif
	sili_post_indirect(sp, ccb);
	if (!sili_pwait_eq(sp, SILI_PREG_PSS, (1 << ccb->ccb_xa.tag), 0,
	    ccb->ccb_xa.timeout)) {
		printf("%s: cmd failed\n", PORTNAME(sp));
		xa->state = ATA_S_ERROR;
		return (ATA_ERROR);
	}

	bus_dmamap_sync(sc->sc_dmat, SILI_DMA_MAP(sp->sp_cmds),
	    xa->tag * SILI_CMD_LEN, SILI_CMD_LEN, BUS_DMASYNC_POSTWRITE);

	sili_unload(ccb);

	xa->state = ATA_S_COMPLETE;

	s = splbio();
	xa->complete(xa);
	splx(s);

	return (ATA_COMPLETE);
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

	xa->resid = xa->datalen - sili_pread(sp, SILI_PREG_RX_COUNT(xa->tag));
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
