/*	$OpenBSD: musycc.c,v 1.8 2005/09/22 12:47:14 claudio Exp $ */

/*
 * Copyright (c) 2004,2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Claudio Jeker <jeker@accoom.net>
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
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/types.h>

#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_sppp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/pci/musyccvar.h>
#include <dev/pci/musyccreg.h>

int	musycc_alloc_groupdesc(struct musycc_softc *);
int	musycc_alloc_intqueue(struct musycc_softc *);
int	musycc_alloc_group(struct musycc_group *);
void	musycc_free_groupdesc(struct musycc_softc *);
void	musycc_free_intqueue(struct musycc_softc *);
void	musycc_free_dmadesc(struct musycc_group *);
void	musycc_free_group(struct musycc_group *);
void	musycc_set_group(struct musycc_group *, int, int, int);
int	musycc_set_tsmap(struct musycc_group *, struct channel_softc *, char);
int	musycc_set_chandesc(struct musycc_group *, int, int, int);
void	musycc_activate_channel(struct musycc_group *, int);
void	musycc_state_engine(struct musycc_group *, int, enum musycc_event);

struct dma_desc		*musycc_dma_get(struct musycc_group *);
void	musycc_dma_free(struct musycc_group *, struct dma_desc *);
int	musycc_list_tx_init(struct musycc_group *, int, int);
int	musycc_list_rx_init(struct musycc_group *, int, int);
void	musycc_list_tx_free(struct musycc_group *, int);
void	musycc_list_rx_free(struct musycc_group *, int);
void	musycc_reinit_dma(struct musycc_group *, int);
int	musycc_newbuf(struct musycc_group *, struct dma_desc *, struct mbuf *);
int	musycc_encap(struct musycc_group *, struct mbuf *, int);

void	musycc_rxeom(struct musycc_group *, int, int);
void	musycc_txeom(struct musycc_group *, int, int);
void	musycc_kick(struct musycc_group *);
void	musycc_sreq(struct musycc_group *, int, u_int32_t, int,
	    enum musycc_event);

#ifndef ACCOOM_DEBUG
#define musycc_dump_group(n, x)
#define musycc_dump_desc(n, x)
#define musycc_dump_dma(n, x, y)
#else
int	accoom_debug = 0;

char	*musycc_intr_print(u_int32_t);
void	 musycc_dump_group(int, struct musycc_group *);
void	 musycc_dump_desc(int, struct musycc_group *);
void	 musycc_dump_dma(int, struct musycc_group *, int);
#endif

int
musycc_attach_common(struct musycc_softc *sc, u_int32_t portmap, u_int32_t mode)
{
	struct musycc_group	*mg;
	int			 i, j;

	/* soft reset device */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_SERREQ(0),
	    MUSYCC_SREQ_SET(1));
	bus_space_barrier(sc->mc_st, sc->mc_sh, MUSYCC_SERREQ(0),
	    sizeof(u_int32_t), BUS_SPACE_BARRIER_WRITE);

	if (musycc_alloc_groupdesc(sc) == -1) {
		printf(": couldn't alloc group descriptors\n");
		return (-1);
	}

	if (musycc_alloc_intqueue(sc) == -1) {
		printf(": couldn't alloc interrupt queue\n");
		musycc_free_groupdesc(sc);
		return (-1);
	}

	/*
	 * global configuration: set EBUS to sane defaults:
	 * intel mode, elapse = 3, blapse = 3, alapse = 3
	 * XXX XXX disable INTB for now
	 */
	sc->mc_global_conf = (portmap & MUSYCC_CONF_PORTMAP) |
	    MUSYCC_CONF_MPUSEL | MUSYCC_CONF_ECKEN |
	    MUSYCC_CONF_ELAPSE_SET(3) | MUSYCC_CONF_ALAPSE_SET(3) |
	    MUSYCC_CONF_BLAPSE_SET(3) | MUSYCC_CONF_INTB;

	/* initialize group descriptors */
	sc->mc_groups = (struct musycc_group *)malloc(sc->mc_ngroups *
	    sizeof(struct musycc_group), M_DEVBUF, M_NOWAIT);
	if (sc->mc_groups == NULL) {
		printf(": couldn't alloc group descriptors\n");
		musycc_free_groupdesc(sc);
		musycc_free_intqueue(sc);
		return (-1);
	}
	bzero(sc->mc_groups, sc->mc_ngroups * sizeof(struct musycc_group));

	for (i = 0; i < sc->mc_ngroups; i++) {
		mg = &sc->mc_groups[i];
		mg->mg_hdlc = sc;
		mg->mg_gnum = i;
		mg->mg_port = i >> (sc->mc_global_conf & 0x3);
		mg->mg_dmat = sc->mc_dmat;

		if (musycc_alloc_group(mg) == -1) {
			printf(": couldn't alloc group structures\n");
			for (j = 0; j < i; j++)
				musycc_free_group(&sc->mc_groups[j]);
			musycc_free_groupdesc(sc);
			musycc_free_intqueue(sc);
			return (-1);
		}

		mg->mg_group = (struct musycc_grpdesc *)
		    (sc->mc_groupkva + MUSYCC_GROUPBASE(i));
		bzero(mg->mg_group, sizeof(struct musycc_grpdesc));
		musycc_set_group(mg, MUSYCC_GRCFG_POLL32, MUSYCC_MAXFRM_MAX,
		    MUSYCC_MAXFRM_MAX);
		musycc_set_port(mg, mode);

		bus_dmamap_sync(sc->mc_dmat, sc->mc_cfgmap,
		    MUSYCC_GROUPBASE(i), sizeof(struct musycc_grpdesc),
		    BUS_DMASYNC_PREWRITE);
		bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_GROUPBASE(i),
		    sc->mc_cfgmap->dm_segs[0].ds_addr + MUSYCC_GROUPBASE(i));
	}

	/* Dual Address Cycle Base Pointer */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_DACB_PTR, 0);
	/* Global Configuration Descriptor */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_GLOBALCONF,
	    sc->mc_global_conf);
	/* Interrupt Queue Descriptor */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_INTQPTR,
	    sc->mc_intrqptr);
	/*
	 * Interrupt Queue Length.
	 * NOTE: a value of 1 indicates a queue length of 2 descriptors!
	 */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_INTQLEN,
	    MUSYCC_INTLEN - 1);

	/* Configure groups, needs to be done only once per group */
	for (i = 0; i < sc->mc_ngroups; i++) {
		mg = &sc->mc_groups[i];
		musycc_sreq(mg, 0, MUSYCC_SREQ_SET(5), MUSYCC_SREQ_BOTH,
		    EV_NULL);
		mg->mg_loaded = 1;
	}

	return (0);
}

int
musycc_alloc_groupdesc(struct musycc_softc *sc)
{
	/*
	 * Allocate per group/port shared memory.
	 * One big cunck of nports * 2048 bytes is allocated. This is
	 * done to ensure that all group structures are 2048 bytes aligned. 
	 */
	if (bus_dmamem_alloc(sc->mc_dmat, sc->mc_ngroups * 2048,
	    2048, 0, sc->mc_cfgseg, 1, &sc->mc_cfgnseg, BUS_DMA_NOWAIT)) {
		return (-1);
	}
	if (bus_dmamem_map(sc->mc_dmat, sc->mc_cfgseg, sc->mc_cfgnseg,
	    sc->mc_ngroups * 2048, &sc->mc_groupkva, BUS_DMA_NOWAIT)) {
		bus_dmamem_free(sc->mc_dmat, sc->mc_cfgseg, sc->mc_cfgnseg);
		return (-1);
	}
	/* create and load bus dma segment, one for all ports */
	if (bus_dmamap_create(sc->mc_dmat, sc->mc_ngroups * 2048,
	    1, sc->mc_ngroups * 2048, 0, BUS_DMA_NOWAIT, &sc->mc_cfgmap)) {
		bus_dmamem_unmap(sc->mc_dmat, sc->mc_groupkva,
		    sc->mc_ngroups * 2048);
		bus_dmamem_free(sc->mc_dmat, sc->mc_cfgseg, sc->mc_cfgnseg);
		return (-1);
	}
	if (bus_dmamap_load(sc->mc_dmat, sc->mc_cfgmap, sc->mc_groupkva,
	    sc->mc_ngroups * 2048, NULL, BUS_DMA_NOWAIT)) {
		musycc_free_groupdesc(sc);
		return (-1);
	}

	return (0);
}

int
musycc_alloc_intqueue(struct musycc_softc *sc)
{
	/*
	 * allocate interrupt queue, use one page for the queue
	 */
	if (bus_dmamem_alloc(sc->mc_dmat, sizeof(struct musycc_intdesc), 4, 0,
	    sc->mc_intrseg, 1, &sc->mc_intrnseg, BUS_DMA_NOWAIT)) {
		return (-1);
	}
	if (bus_dmamem_map(sc->mc_dmat, sc->mc_intrseg, sc->mc_intrnseg,
	    sizeof(struct musycc_intdesc), (caddr_t *)&sc->mc_intrd,
	    BUS_DMA_NOWAIT)) {
		bus_dmamem_free(sc->mc_dmat, sc->mc_intrseg, sc->mc_intrnseg);
		return (-1);
	}

	/* create and load bus dma segment */
	if (bus_dmamap_create(sc->mc_dmat, sizeof(struct musycc_intdesc),
	    1, sizeof(struct musycc_intdesc), 0, BUS_DMA_NOWAIT,
	    &sc->mc_intrmap)) {
		bus_dmamem_unmap(sc->mc_dmat, (caddr_t)sc->mc_intrd,
		    sizeof(struct musycc_intdesc));
		bus_dmamem_free(sc->mc_dmat, sc->mc_intrseg, sc->mc_intrnseg);
		return (-1);
	}
	if (bus_dmamap_load(sc->mc_dmat, sc->mc_intrmap, sc->mc_intrd,
	    sizeof(struct musycc_intdesc), NULL, BUS_DMA_NOWAIT)) {
		musycc_free_intqueue(sc);
		return (-1);
	}

	/* initialize the interrupt queue pointer */
	sc->mc_intrqptr = sc->mc_intrmap->dm_segs[0].ds_addr +
	    offsetof(struct musycc_intdesc, md_intrq[0]);

	return (0);
}

int
musycc_alloc_group(struct musycc_group *mg)
{
	struct dma_desc		*dd;
	int			 j;

	/* Allocate per group dma memory */
	if (bus_dmamem_alloc(mg->mg_dmat, MUSYCC_DMA_MAPSIZE,
	    PAGE_SIZE, 0, mg->mg_listseg, 1, &mg->mg_listnseg,
	    BUS_DMA_NOWAIT))
		return (-1);
	if (bus_dmamem_map(mg->mg_dmat, mg->mg_listseg, mg->mg_listnseg,
	    MUSYCC_DMA_MAPSIZE, &mg->mg_listkva, BUS_DMA_NOWAIT)) {
		bus_dmamem_free(mg->mg_dmat, mg->mg_listseg, mg->mg_listnseg);
		return (-1);
	}

	/* create and load bus dma segment */
	if (bus_dmamap_create(mg->mg_dmat, MUSYCC_DMA_MAPSIZE, 1,
	    MUSYCC_DMA_MAPSIZE, 0, BUS_DMA_NOWAIT, &mg->mg_listmap)) {
		bus_dmamem_unmap(mg->mg_dmat, mg->mg_listkva,
		    MUSYCC_DMA_MAPSIZE);
		bus_dmamem_free(mg->mg_dmat, mg->mg_listseg, mg->mg_listnseg);
		return (-1);
	}
	if (bus_dmamap_load(mg->mg_dmat, mg->mg_listmap, mg->mg_listkva,
	    MUSYCC_DMA_MAPSIZE, NULL, BUS_DMA_NOWAIT)) {
		musycc_free_dmadesc(mg);
		return (-1);
	}

	/*
	 * Create spare maps for musycc_start and musycc_newbuf.
	 * Limit the dma queue to MUSYCC_DMA_SIZE entries even though there
	 * is no actual hard limit from the chip.
	 */
	if (bus_dmamap_create(mg->mg_dmat, MCLBYTES, MUSYCC_DMA_SIZE, MCLBYTES,
	    0, BUS_DMA_NOWAIT, &mg->mg_tx_sparemap) != 0) {
		musycc_free_dmadesc(mg);
		return (-1);
	}
	if (bus_dmamap_create(mg->mg_dmat, MCLBYTES, MUSYCC_DMA_SIZE, MCLBYTES,
	    0, BUS_DMA_NOWAIT, &mg->mg_rx_sparemap) != 0) {
		bus_dmamap_destroy(mg->mg_dmat, mg->mg_tx_sparemap);
		musycc_free_dmadesc(mg);
		return (-1);
	}

	mg->mg_dma_pool = (struct dma_desc *)mg->mg_listkva;
	bzero(mg->mg_dma_pool,
	    MUSYCC_DMA_CNT * sizeof(struct dma_desc));

	/* add all descriptors to the freelist */
	for (j = 0; j < MUSYCC_DMA_CNT; j++) {
		dd = &mg->mg_dma_pool[j];
		/* initalize, same as for spare maps */
		if (bus_dmamap_create(mg->mg_dmat, MCLBYTES, MUSYCC_DMA_SIZE,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &dd->map)) {
			musycc_free_group(mg);
			return (-1);
		}
		/* link */
		dd->nextdesc = mg->mg_freelist;
		mg->mg_freelist = dd;
		mg->mg_freecnt++;
	}

	return (0);
}

void
musycc_free_groupdesc(struct musycc_softc *sc)
{
	bus_dmamap_destroy(sc->mc_dmat, sc->mc_cfgmap);
	bus_dmamem_unmap(sc->mc_dmat, sc->mc_groupkva,
	    sc->mc_ngroups * 2048);
	bus_dmamem_free(sc->mc_dmat, sc->mc_cfgseg, sc->mc_cfgnseg);
}

void
musycc_free_intqueue(struct musycc_softc *sc)
{
	bus_dmamap_destroy(sc->mc_dmat, sc->mc_intrmap);
	bus_dmamem_unmap(sc->mc_dmat, (caddr_t)sc->mc_intrd,
	    sizeof(struct musycc_intdesc));
	bus_dmamem_free(sc->mc_dmat, sc->mc_intrseg, sc->mc_intrnseg);
}

void
musycc_free_dmadesc(struct musycc_group *mg)
{
	bus_dmamap_destroy(mg->mg_dmat, mg->mg_listmap);
	bus_dmamem_unmap(mg->mg_dmat, mg->mg_listkva,
	    MUSYCC_DMA_MAPSIZE);
	bus_dmamem_free(mg->mg_dmat, mg->mg_listseg, mg->mg_listnseg);
}

void
musycc_free_group(struct musycc_group *mg)
{
	bus_dmamap_destroy(mg->mg_dmat, mg->mg_tx_sparemap);
	bus_dmamap_destroy(mg->mg_dmat, mg->mg_tx_sparemap);
	/* XXX dma descriptors ? */
	musycc_free_dmadesc(mg);
	mg->mg_dma_pool = NULL;
	mg->mg_freelist = NULL;
	mg->mg_freecnt = 0;
}

void
musycc_set_group(struct musycc_group *mg, int poll, int maxa, int maxb)
{
	/* set global conf and interrupt descriptor */
	mg->mg_group->global_conf = htole32(mg->mg_hdlc->mc_global_conf);
	/*
	 * Interrupt Queue and Length.
	 * NOTE: a value of 1 indicates the queue length of 2 descriptors!
	 */
	mg->mg_group->int_queuep = htole32(mg->mg_hdlc->mc_intrqptr);
	mg->mg_group->int_queuelen = htole32(MUSYCC_INTLEN - 1);

	/* group config */
	mg->mg_group->group_conf = htole32(MUSYCC_GRCFG_RXENBL |
	    MUSYCC_GRCFG_TXENBL | MUSYCC_GRCFG_SUBDSBL |
	    MUSYCC_GRCFG_MSKCOFA | MUSYCC_GRCFG_MSKOOF |
	    MUSYCC_GRCFG_MCENBL | (poll & MUSYCC_GRCFG_POLL64));

	/* memory protection, not supported by device */

	/* message length config, preinit with useful data */
	/* this is currently not used and the max is limited to 4094 bytes */
	mg->mg_group->msglen_conf = htole32(maxa);
	mg->mg_group->msglen_conf |= htole32(maxb << MUSYCC_MAXFRM2_SHIFT);
}

void
musycc_set_port(struct musycc_group *mg, int mode)
{
	/*
	 * All signals trigger on falling edge only exception is TSYNC
	 * which triggers on rising edge. For the framer TSYNC is set to
	 * falling edge too but Musycc needs rising edge or everything gets
	 * off by one. Don't three-state TX (not needed).
	 */
	mg->mg_group->port_conf = htole32(MUSYCC_PORT_TSYNC_EDGE |
	    MUSYCC_PORT_TRITX | (mode & MUSYCC_PORT_MODEMASK));

	if (mg->mg_loaded)
		musycc_sreq(mg, 0, MUSYCC_SREQ_SET(21), MUSYCC_SREQ_RX,
		    EV_NULL);
}

/*
 * Channel specifc calls
 */
int
musycc_set_tsmap(struct musycc_group *mg, struct channel_softc *cc, char slot)
{
	int		i, nslots = 0, off, scale;
	u_int32_t	tslots = cc->cc_tslots;

	ACCOOM_PRINTF(1, ("%s: musycc_set_tsmap %08x slot %c\n",
	    cc->cc_ifp->if_xname, tslots, slot));
	
	switch (slot) {
	case 'A':		/* single port, non interleaved */
		off = 0;
		scale = 1;
		break;
	case 'a':		/* dual port, interleaved */
	case 'b':
		off = slot - 'a';
		scale = 2;
		break;
	case '1':		/* possible quad port, interleaved */
	case '2':
	case '3':
	case '4':
		off = slot - '1';
		scale = 4;
		break;
	default:
		/* impossible */
		log(LOG_ERR, "%s: accessing unsupported slot %c",
		    cc->cc_ifp->if_xname, slot);
		return (-1);
	}

	/*
	 * setup timeslot map but first make sure no timeslot is already used 
	 * note: 56kbps mode for T1-SF needs to be set in here
	 * note2: if running with port mapping the other group needs to be
	 * checked too or we may get funny results. Currenly not possible
	 * because of the slot offsets (odd, even slots).
	 */
	for (i = 0; i < sizeof(u_int32_t) * 8; i++)
		if (tslots & (1 << i))
			if (mg->mg_group->tx_tsmap[i * scale + off] &
			    MUSYCC_TSLOT_ENABLED ||
			    mg->mg_group->rx_tsmap[i * scale + off] &
			    MUSYCC_TSLOT_ENABLED)
				return (0);

	for (i = 0; i < sizeof(u_int32_t) * 8; i++)
		if (tslots & (1 << i)) {
			nslots++;
			mg->mg_group->tx_tsmap[i * scale + off] =
			    MUSYCC_TSLOT_CHAN(cc->cc_channel) |
			    MUSYCC_TSLOT_ENABLED;
			mg->mg_group->rx_tsmap[i * scale + off] =
			    MUSYCC_TSLOT_CHAN(cc->cc_channel) |
			    MUSYCC_TSLOT_ENABLED;
		}

	return (nslots);
}

int
musycc_set_chandesc(struct musycc_group *mg, int chan, int nslots, int proto)
{
	u_int64_t	mask = ULLONG_MAX;
	int		idx, n;

	ACCOOM_PRINTF(1, ("%s: musycc_set_chandesc nslots %d proto %d\n",
	    mg->mg_channels[chan]->cc_ifp->if_xname, nslots, proto));

	if (nslots == 0 || nslots > 32)
		return (EINVAL);

	n = 64 - 2 * nslots;
	mask >>= n;

	for (idx = 0; idx <= n; idx += 2)
		if (!(mg->mg_fifomask & mask << idx))
			break;

	if (idx > n)
		return (EBUSY);

	mg->mg_fifomask |= mask << idx;

	/* setup channel descriptor */
	mg->mg_group->tx_cconf[chan] = htole32(MUSYCC_CHAN_BUFIDX_SET(idx) |
	    MUSYCC_CHAN_BUFLEN_SET(nslots * 2 - 1) |
	    MUSYCC_CHAN_PROTO_SET(proto));
	mg->mg_group->rx_cconf[chan] = htole32(MUSYCC_CHAN_BUFIDX_SET(idx) |
	    MUSYCC_CHAN_BUFLEN_SET(nslots * 2 - 1) |
	    MUSYCC_CHAN_MSKIDLE | MUSYCC_CHAN_MSKSUERR | MUSYCC_CHAN_MSKSINC |
	    MUSYCC_CHAN_MSKSDEC | MUSYCC_CHAN_MSKSFILT |
	    MUSYCC_CHAN_PROTO_SET(proto));

	return (0);
}

int
musycc_init_channel(struct channel_softc *cc, char slot)
{
	struct musycc_group	*mg;
	struct ifnet		*ifp = cc->cc_ifp;
	int			 nslots, rv, s;

	if (cc->cc_state == CHAN_FLOAT)
		return (ENOTTY);
	mg = cc->cc_group;

	ACCOOM_PRINTF(2, ("%s: musycc_init_channel [state %d] slot %c\n",
	    cc->cc_ifp->if_xname, cc->cc_state, slot));

	if (cc->cc_state != CHAN_IDLE) {
		musycc_sreq(mg, cc->cc_channel, MUSYCC_SREQ_SET(9),
		    MUSYCC_SREQ_BOTH, EV_STOP);
		tsleep(cc, PZERO | PCATCH, "musycc", hz);
		if (cc->cc_state != CHAN_IDLE) {
			ACCOOM_PRINTF(0, ("%s: failed to reset channel\n",
			    cc->cc_ifp->if_xname));
			return (EIO);
		}
	}

	s = splnet();
	/* setup timeslot map */
	nslots = musycc_set_tsmap(mg, cc, slot);
	if (nslots == -1) {
		rv = EINVAL;
		goto fail;
	} else if (nslots == 0) {
		rv = EBUSY;
		goto fail;
	}

	if ((rv = musycc_set_chandesc(mg, cc->cc_channel, nslots,
	    MUSYCC_PROTO_HDLC16)))
		goto fail;

	/* setup tx DMA chain */
	musycc_list_tx_init(mg, cc->cc_channel, MUSYCC_DMA_SIZE);
	/* setup rx DMA chain */
	if ((rv = musycc_list_rx_init(mg, cc->cc_channel, MUSYCC_DMA_SIZE))) {
		ACCOOM_PRINTF(0, ("%s: initialization failed: "
		    "no memory for rx buffers\n", cc->cc_ifp->if_xname));
		goto fail;
	}

	/* IFF_RUNNING set by sppp_ioctl() */
	ifp->if_flags &= ~IFF_OACTIVE;

	cc->cc_state = CHAN_TRANSIENT;
	splx(s);

	musycc_dump_group(3, mg);
	musycc_activate_channel(mg, cc->cc_channel);
	tsleep(cc, PZERO | PCATCH, "musycc", hz);

	/*
	 * XXX we could actually check if the activation of the channels was
	 * successful but what type of error should we return?
	 */
	return (0);

fail:
	splx(s);
	cc->cc_state = CHAN_IDLE; /* force idle state */
	musycc_free_channel(mg, cc->cc_channel);
	return (rv);
}

void
musycc_activate_channel(struct musycc_group *mg, int chan)
{
	ACCOOM_PRINTF(2, ("%s: musycc_activate_channel\n",
	    mg->mg_channels[chan]->cc_ifp->if_xname));
	musycc_sreq(mg, chan, MUSYCC_SREQ_SET(26), MUSYCC_SREQ_BOTH,
	    EV_NULL);
	musycc_sreq(mg, chan, MUSYCC_SREQ_SET(24), MUSYCC_SREQ_BOTH,
	    EV_NULL);
	musycc_sreq(mg, chan, MUSYCC_SREQ_SET(8), MUSYCC_SREQ_BOTH,
	    EV_ACTIVATE);
}

void
musycc_stop_channel(struct channel_softc *cc)
{
	struct musycc_group	*mg;

	if (cc->cc_state == CHAN_FLOAT) {
		/* impossible */
		log(LOG_ERR, "%s: unexpected state in musycc_stop_channel",
		    cc->cc_ifp->if_xname);
		cc->cc_state = CHAN_IDLE; /* reset */
		musycc_free_channel(mg, cc->cc_channel);
		return;
	}

	mg = cc->cc_group;
	ACCOOM_PRINTF(2, ("%s: musycc_stop_channel\n", cc->cc_ifp->if_xname));
	musycc_sreq(mg, cc->cc_channel, MUSYCC_SREQ_SET(9), MUSYCC_SREQ_BOTH,
	    EV_STOP);
	tsleep(cc, PZERO | PCATCH, "musycc", hz);
}

void
musycc_free_channel(struct musycc_group *mg, int chan)
{
	u_int64_t	mask = ULLONG_MAX;
	int		i, idx, s, slots;

	ACCOOM_PRINTF(2, ("%s: musycc_free_channel\n",
	    mg->mg_channels[chan]->cc_ifp->if_xname));

	s = splnet();
	/* Clear the timeout timer. */
	mg->mg_channels[chan]->cc_ifp->if_timer = 0;

	/* clear timeslot map */
	for (i = 0; i < 128; i++) {
		if (mg->mg_group->tx_tsmap[i] & MUSYCC_TSLOT_ENABLED)
			if ((mg->mg_group->tx_tsmap[i] & MUSYCC_TSLOT_MASK) ==
			    chan)
				mg->mg_group->tx_tsmap[i] = 0;
		if (mg->mg_group->rx_tsmap[i] & MUSYCC_TSLOT_ENABLED)
			if ((mg->mg_group->rx_tsmap[i] & MUSYCC_TSLOT_MASK) ==
			    chan)
				mg->mg_group->rx_tsmap[i] = 0;
	}

	/* clear channel descriptor, especially free FIFO space */
	idx = MUSYCC_CHAN_BUFIDX_GET(letoh32(mg->mg_group->tx_cconf[chan]));
	slots = MUSYCC_CHAN_BUFLEN_GET(letoh32(mg->mg_group->tx_cconf[chan]));
	slots = (slots + 1) / 2;
	mask >>= 64 - 2 * slots;
	mask <<= idx;
	mg->mg_fifomask &= ~mask;
	mg->mg_group->tx_cconf[chan] = 0;
	mg->mg_group->rx_cconf[chan] = 0;

	/* free dma rings */
	musycc_list_rx_free(mg, chan);
	musycc_list_tx_free(mg, chan);

	splx(s);

	/* update chip info with sreq */
	musycc_sreq(mg, chan, MUSYCC_SREQ_SET(24), MUSYCC_SREQ_BOTH,
	    EV_NULL);
	musycc_sreq(mg, chan, MUSYCC_SREQ_SET(26), MUSYCC_SREQ_BOTH,
	    EV_IDLE);
}

void
musycc_state_engine(struct musycc_group *mg, int chan, enum musycc_event ev)
{
	enum musycc_state	state;

	if (mg->mg_channels[chan] == NULL)
		return;

	state = mg->mg_channels[chan]->cc_state;

	ACCOOM_PRINTF(1, ("%s: musycc_state_engine state %d event %d\n",
	    mg->mg_channels[chan]->cc_ifp->if_xname, state, ev));

	switch (ev) {
	case EV_NULL:
		/* no state change */
		return;
	case EV_ACTIVATE:
		state = CHAN_RUNNING;
		break;
	case EV_STOP:
		/* channel disabled now free dma rings et al. */
		mg->mg_channels[chan]->cc_state = CHAN_TRANSIENT;
		musycc_free_channel(mg, chan);
		return;
	case EV_IDLE:
		state = CHAN_IDLE;
		break;
	case EV_WATCHDOG:
		musycc_reinit_dma(mg, chan);
		return;
	}

	mg->mg_channels[chan]->cc_state = state;
	wakeup(mg->mg_channels[chan]);
}

/*
 * DMA handling functions
 */

struct dma_desc *
musycc_dma_get(struct musycc_group *mg)
{
	struct dma_desc	*dd;

	splassert(IPL_NET);

	if (mg->mg_freecnt == 0)
		return (NULL);
	mg->mg_freecnt--;
	dd = mg->mg_freelist;
	mg->mg_freelist = dd->nextdesc;
	/* clear some important data */
	dd->nextdesc = NULL;
	dd->mbuf = NULL;

	return (dd);
}

void
musycc_dma_free(struct musycc_group *mg, struct dma_desc *dd)
{
	splassert(IPL_NET);

	dd->nextdesc = mg->mg_freelist;
	mg->mg_freelist = dd;
	mg->mg_freecnt++;
}

/*
 * Initialize the transmit descriptors. Acctually they are left empty until
 * a packet comes in.
 */
int
musycc_list_tx_init(struct musycc_group *mg, int c, int size)
{
	struct musycc_dma_data	*md;
	struct dma_desc		*dd;
	bus_addr_t		 base;
	int			 i;

	splassert(IPL_NET);
	ACCOOM_PRINTF(2, ("musycc_list_tx_init\n"));
	md = &mg->mg_dma_d[c];
	md->tx_pend = NULL;
	md->tx_cur = NULL;
	md->tx_cnt = size;
	md->tx_pkts = 0;

	base = mg->mg_listmap->dm_segs[0].ds_addr;
	for (i = 0; i < md->tx_cnt; i++) {
		dd = musycc_dma_get(mg);
		if (dd == NULL) {
			ACCOOM_PRINTF(0, ("musycc_list_tx_init: "
			    "out of dma_desc\n"));
			musycc_list_tx_free(mg, c);
			return (ENOBUFS);
		}
		dd->status = 0 /* MUSYCC_STATUS_NOPOLL */;
		dd->data = 0;
		if (md->tx_cur) {
			md->tx_cur->nextdesc = dd;
			md->tx_cur->next = htole32(base + (caddr_t)dd -
			    mg->mg_listkva);
			md->tx_cur = dd;
		} else
			md->tx_pend = md->tx_cur = dd;
	}

	dd->nextdesc = md->tx_pend;
	dd->next = htole32(base + (caddr_t)md->tx_pend - mg->mg_listkva);
	md->tx_pend = dd;

	mg->mg_group->tx_headp[c] = htole32(base + (caddr_t)dd -
	    mg->mg_listkva);

	bus_dmamap_sync(mg->mg_dmat, mg->mg_listmap, 0, MUSYCC_DMA_MAPSIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
int
musycc_list_rx_init(struct musycc_group *mg, int c, int size)
{
	struct musycc_dma_data	*md;
	struct dma_desc		*dd = NULL, *last;
	bus_addr_t		 base;
	int			 i;

	splassert(IPL_NET);
	ACCOOM_PRINTF(2, ("musycc_list_rx_init\n"));
	md = &mg->mg_dma_d[c];
	md->rx_cnt = size;

	base = mg->mg_listmap->dm_segs[0].ds_addr;
	for (i = 0; i < size; i++) {
		dd = musycc_dma_get(mg);
		if (dd == NULL) {
			ACCOOM_PRINTF(0, ("musycc_list_rx_init: "
			    "out of dma_desc\n"));
			musycc_list_rx_free(mg, c);
			return (ENOBUFS);
		}
		if (musycc_newbuf(mg, dd, NULL) == ENOBUFS) {
			ACCOOM_PRINTF(0, ("musycc_list_rx_init: "
			    "out of mbufs\n"));
			musycc_list_rx_free(mg, c);
			return (ENOBUFS);
		}
		if (md->rx_prod) {
			md->rx_prod->nextdesc = dd;
			md->rx_prod->next = htole32(base + (caddr_t)dd -
			    mg->mg_listkva);
			md->rx_prod = dd;
		} else
			last = md->rx_prod = dd;
	}

	dd->nextdesc = last;
	dd->next = htole32(base + (caddr_t)last - mg->mg_listkva);

	mg->mg_group->rx_headp[c] = htole32(base + (caddr_t)dd -
	    mg->mg_listkva);

	bus_dmamap_sync(mg->mg_dmat, mg->mg_listmap, 0, MUSYCC_DMA_MAPSIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

void
musycc_list_tx_free(struct musycc_group *mg, int c)
{
	struct musycc_dma_data	*md;
	struct dma_desc		*dd, *tmp;

	md = &mg->mg_dma_d[c];

	splassert(IPL_NET);
	ACCOOM_PRINTF(2, ("musycc_list_tx_free\n"));
	dd = md->tx_pend;
	do {
		if (dd == NULL)
			break;
		if (dd->map->dm_nsegs != 0) {
			bus_dmamap_t map = dd->map;

			bus_dmamap_unload(mg->mg_dmat, map);
		}
		if (dd->mbuf != NULL) {
			m_freem(dd->mbuf);
			dd->mbuf = NULL;
		}
		tmp = dd;
		dd = dd->nextdesc;
		musycc_dma_free(mg, tmp);
	} while (dd != md->tx_pend);
	md->tx_pend = md->tx_cur = NULL;
	md->tx_cnt = md->tx_use = md->tx_pkts = 0;
}

void
musycc_list_rx_free(struct musycc_group *mg, int c)
{
	struct musycc_dma_data	*md;
	struct dma_desc		*dd, *tmp;

	md = &mg->mg_dma_d[c];

	splassert(IPL_NET);
	ACCOOM_PRINTF(2, ("musycc_list_rx_free\n"));
	dd = md->rx_prod;
	do {
		if (dd == NULL)
			break;
		if (dd->map->dm_nsegs != 0) {
			bus_dmamap_t map = dd->map;

			bus_dmamap_unload(mg->mg_dmat, map);
		}
		if (dd->mbuf != NULL) {
			m_freem(dd->mbuf);
			dd->mbuf = NULL;
		}
		tmp = dd;
		dd = dd->nextdesc;
		musycc_dma_free(mg, tmp);
	} while (dd != md->rx_prod);
	md->rx_prod = NULL;
	md->rx_cnt = 0;
}

/* only used by the watchdog timeout */
void
musycc_reinit_dma(struct musycc_group *mg, int c)
{
	int	s;

	s = splnet();

	musycc_list_tx_free(mg, c);
	musycc_list_rx_free(mg, c);

	/* setup tx & rx DMA chain */
	if (musycc_list_tx_init(mg, c, MUSYCC_DMA_SIZE) ||
	    musycc_list_rx_init(mg, c, MUSYCC_DMA_SIZE)) {
		log(LOG_ERR, "%s: Failed to malloc memory\n",
		    mg->mg_channels[c]->cc_ifp->if_xname);
		musycc_free_channel(mg, c);
	}
	splx(s);

	musycc_activate_channel(mg, c);
}

/*
 * Initialize an RX descriptor and attach an mbuf cluster.
 */
int
musycc_newbuf(struct musycc_group *mg, struct dma_desc *c, struct mbuf *m)
{
	struct mbuf	*m_new = NULL;
	bus_dmamap_t	 map;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	if (bus_dmamap_load(mg->mg_dmat, mg->mg_rx_sparemap,
	    mtod(m_new, caddr_t), m_new->m_pkthdr.len, NULL,
	    BUS_DMA_NOWAIT) != 0) {
		ACCOOM_PRINTF(0, ("%s: rx load failed\n",
		    mg->mg_hdlc->mc_dev.dv_xname));
		m_freem(m_new);
		return (ENOBUFS);
	}
	map = c->map;
	c->map = mg->mg_rx_sparemap;
	mg->mg_rx_sparemap = map;

	bus_dmamap_sync(mg->mg_dmat, c->map, 0, c->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	c->mbuf = m_new;
	c->data = htole32(c->map->dm_segs[0].ds_addr);
	c->status = htole32(MUSYCC_STATUS_NOPOLL |
		    MUSYCC_STATUS_LEN(m_new->m_pkthdr.len));

	bus_dmamap_sync(mg->mg_dmat, mg->mg_listmap,
	    ((caddr_t)c - mg->mg_listkva), sizeof(struct dma_desc),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
int
musycc_encap(struct musycc_group *mg, struct mbuf *m_head, int c)
{
	struct dma_desc	*cur, *tmp;
	bus_dmamap_t	 map;
	bus_addr_t	 base;
	u_int32_t	 status;
	int		 i;

	splassert(IPL_NET);

	map = mg->mg_tx_sparemap;
	if (bus_dmamap_load_mbuf(mg->mg_dmat, map, m_head,
	    BUS_DMA_NOWAIT) != 0) {
		ACCOOM_PRINTF(0, ("%s: musycc_encap: dmamap_load failed\n",
		    mg->mg_channels[c]->cc_ifp->if_xname));
		return (ENOBUFS);
	}

	cur = mg->mg_dma_d[c].tx_cur;
	base = mg->mg_listmap->dm_segs[0].ds_addr;

	if (map->dm_nsegs + mg->mg_dma_d[c].tx_use >= mg->mg_dma_d[c].tx_cnt) {
		ACCOOM_PRINTF(1, ("%s: tx out of dma bufs\n",
		    mg->mg_channels[c]->cc_ifp->if_xname));
		return (ENOBUFS);
	}

	i = 0;
	while (i < map->dm_nsegs) {
		status = /* MUSYCC_STATUS_NOPOLL | */
		    MUSYCC_STATUS_LEN(map->dm_segs[i].ds_len);
		if (cur != mg->mg_dma_d[c].tx_cur)
			status |= MUSYCC_STATUS_OWNER;

		cur->status = htole32(status);
		cur->data = htole32(map->dm_segs[i].ds_addr);

		bus_dmamap_sync(mg->mg_dmat, mg->mg_listmap,
		    ((caddr_t)cur - mg->mg_listkva), sizeof(struct dma_desc),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		if (++i >= map->dm_nsegs)
			break;
		cur = cur->nextdesc;
	}

	bus_dmamap_sync(mg->mg_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	cur->mbuf = m_head;
	mg->mg_tx_sparemap = cur->map;
	cur->map = map;
	cur->status |= htole32(MUSYCC_STATUS_EOM);
	tmp = mg->mg_dma_d[c].tx_cur;
	mg->mg_dma_d[c].tx_cur = cur->nextdesc;
	mg->mg_dma_d[c].tx_use += i;
	mg->mg_dma_d[c].tx_pkts++;

	/*
	 * Last but not least, flag the buffer if the buffer is flagged to
	 * early, it may happen, that the buffer is already transmitted
	 * before we changed all relevant variables.
	 */
	tmp->status |= htole32(MUSYCC_STATUS_OWNER);
#if 0
	/* check for transmited packets NO POLLING mode only */
	/*
	 * Note: a bug in the HDLC chip seems to make it impossible to use
	 * no polling mode.
	 */
	musycc_txeom(mg, c);
	if (mg->mg_dma_d[c].tx_pend == tmp) {
		/* and restart as needed */
		printf("%s: tx needs kick\n",
		    mg->mg_channels[c]->cc_ifp->if_xname);
		mg->mg_group->tx_headp[c] = htole32(base +
		    (caddr_t)mg->mg_dma_d[c].tx_pend - mg->mg_listkva);

		musycc_sreq(mg, c, MUSYCC_SREQ_SET(8), MUSYCC_SREQ_TX);
	}
#endif

	return (0);
}


/*
 * API towards the kernel
 */

/* start transmit of new network buffer */
void
musycc_start(struct ifnet *ifp)
{
	struct musycc_group	*mg;
	struct channel_softc	*cc;
	struct mbuf		*m = NULL;
	int			s;

	cc = ifp->if_softc;
	mg = cc->cc_group;

	ACCOOM_PRINTF(3, ("musycc_start\n"));
	if (cc->cc_state != CHAN_RUNNING)
		return;
	if (ifp->if_flags & IFF_OACTIVE)
		return;
	if (sppp_isempty(ifp))
		return;

	s = splnet();
	while ((m = sppp_pick(ifp)) != NULL) {
		if (musycc_encap(mg, m, cc->cc_channel)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* now we are committed to transmit the packet */
		sppp_dequeue(ifp);
	}
	splx(s);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}


/*
 * Watchdog/transmission transmit timeout handler. Called when a
 * transmission is started on the interface, but no interrupt is
 * received before the timeout. This usually indicates that the
 * card has wedged for some reason.
 */
void
musycc_watchdog(struct ifnet *ifp)
{
	struct channel_softc	*cc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", cc->cc_ifp->if_xname);
	ifp->if_oerrors++;

	musycc_sreq(cc->cc_group, cc->cc_channel, MUSYCC_SREQ_SET(9),
	    MUSYCC_SREQ_BOTH, EV_WATCHDOG);
}


/*
 * Interrupt specific functions
 */

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
void
musycc_rxeom(struct musycc_group *mg, int channel, int forcekick)
{
	struct mbuf	*m;
	struct ifnet	*ifp;
	struct dma_desc	*cur_rx, *start_rx;
	int		 total_len = 0, consumed = 0;
	u_int32_t	 rxstat;

	ACCOOM_PRINTF(3, ("musycc_rxeom\n"));

	ifp = mg->mg_channels[channel]->cc_ifp;

	start_rx = cur_rx = mg->mg_dma_d[channel].rx_prod;
	do {
		bus_dmamap_sync(mg->mg_dmat, mg->mg_listmap,
		    ((caddr_t)cur_rx - mg->mg_listkva),
		    sizeof(struct dma_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		rxstat = letoh32(cur_rx->status);
		if (!(rxstat & MUSYCC_STATUS_OWNER))
			break;

		m = cur_rx->mbuf;
		cur_rx->mbuf = NULL;
		total_len = MUSYCC_STATUS_LEN(rxstat);


		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & MUSYCC_STATUS_ERROR) {
			ifp->if_ierrors++;
			ACCOOM_PRINTF(0, ("%s: rx error %08x\n",
			    ifp->if_xname, rxstat));
			musycc_newbuf(mg, cur_rx, m);
			cur_rx = cur_rx->nextdesc;
			consumed++;
			continue;
		}

		/* No errors; receive the packet. */	
		bus_dmamap_sync(mg->mg_dmat, cur_rx->map, 0,
		    cur_rx->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		if (musycc_newbuf(mg, cur_rx, NULL) != 0) {
			cur_rx = cur_rx->nextdesc;
			consumed++;
			continue;
		}

		cur_rx = cur_rx->nextdesc;
		consumed++;

		/* TODO support mbuf chains */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;
		ifp->if_ipackets++;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* pass it on. */
		sppp_input(ifp, m);
	} while (cur_rx != start_rx);

	mg->mg_dma_d[channel].rx_prod = cur_rx;

	if ((cur_rx == start_rx && consumed) || forcekick) {
		/* send SREQ to signal the new buffers */
		ACCOOM_PRINTF(1, ("%s: rx kick, consumed %d pkts\n",
		    mg->mg_channels[channel]->cc_ifp->if_xname, consumed));
		mg->mg_group->rx_headp[channel] = htole32(
		    mg->mg_listmap->dm_segs[0].ds_addr +
		    (caddr_t)cur_rx - mg->mg_listkva);
		musycc_sreq(mg, channel, MUSYCC_SREQ_SET(8),
		    MUSYCC_SREQ_RX, EV_NULL);
	}
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
void
musycc_txeom(struct musycc_group *mg, int channel, int forcekick)
{
	struct dma_desc		*dd, *dd_pend;
	struct ifnet		*ifp;

	ACCOOM_PRINTF(3, ("musycc_txeom\n"));

	ifp = mg->mg_channels[channel]->cc_ifp;
	/* Clear the watchdog timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	for (dd = mg->mg_dma_d[channel].tx_pend;
	    dd != mg->mg_dma_d[channel].tx_cur;
	    dd = dd->nextdesc) {
		bus_dmamap_sync(mg->mg_dmat, mg->mg_listmap,
		    ((caddr_t)dd - mg->mg_listkva), sizeof(struct dma_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		if (letoh32(dd->status) & MUSYCC_STATUS_OWNER)
			/* musycc still owns this descriptor */
			break;

		mg->mg_dma_d[channel].tx_use--;

		dd->status = 0; /* reinit dma status flags */
		/* dd->status |= MUSYCC_STATUS_NOPOLL; *//* disable polling */

		if (dd->map->dm_nsegs != 0) {
			bus_dmamap_sync(mg->mg_dmat, dd->map, 0,
			    dd->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(mg->mg_dmat, dd->map);
		}
		if (dd->mbuf != NULL) {
			m_freem(dd->mbuf);
			dd->mbuf = NULL;
			mg->mg_dma_d[channel].tx_pkts--;
			ifp->if_opackets++;
		}
	}

	dd_pend = mg->mg_dma_d[channel].tx_pend;
	mg->mg_dma_d[channel].tx_pend = dd;

	if (ifp->if_flags & IFF_OACTIVE && dd_pend != dd) {
		ifp->if_flags &= ~IFF_OACTIVE;
		musycc_start(ifp);
	}

	if (forcekick) {
		/* restart */
		ACCOOM_PRINTF(1, ("%s: tx kick forced\n",
		    mg->mg_channels[channel]->cc_ifp->if_xname));
		mg->mg_group->tx_headp[channel] =
		    htole32(mg->mg_listmap->dm_segs[0].ds_addr +
		    (caddr_t)mg->mg_dma_d[channel].tx_pend - mg->mg_listkva);

		musycc_sreq(mg, channel, MUSYCC_SREQ_SET(8), MUSYCC_SREQ_TX,
		    EV_NULL);
	}
}

int
musycc_intr(void *arg)
{
	struct musycc_softc	*mc = arg;
	struct musycc_group	*mg;
	struct ifnet		*ifp;
	u_int32_t		 intstatus, id;
	int			 i, n, chan;

	intstatus = bus_space_read_4(mc->mc_st, mc->mc_sh, MUSYCC_INTRSTATUS);

	if (intstatus & MUSYCC_INTCNT_MASK) {
		bus_dmamap_sync(mc->mc_dmat, mc->mc_intrmap,
		    offsetof(struct musycc_intdesc, md_intrq[0]),
		    MUSYCC_INTLEN * sizeof(u_int32_t), BUS_DMASYNC_POSTREAD);
		
		ACCOOM_PRINTF(4, ("%s: interrupt status %08x\n",
		    mc->mc_dev.dv_xname, intstatus));

		n = MUSYCC_NEXTINT_GET(intstatus);
		for (i = 0; i < (intstatus & MUSYCC_INTCNT_MASK); i++) {
			id = mc->mc_intrd->md_intrq[(n + i) % MUSYCC_INTLEN];
			chan = MUSYCC_INTD_CHAN(id);
			mg = &mc->mc_groups[MUSYCC_INTD_GRP(id)];

			ACCOOM_PRINTF(4, ("%s: interrupt %s\n",
			    mc->mc_dev.dv_xname, musycc_intr_print(id)));

			if (id & MUSYCC_INTD_ILOST)
				ACCOOM_PRINTF(0, ("%s: interrupt lost\n",
				    mc->mc_dev.dv_xname));

			switch (MUSYCC_INTD_EVENT(id)) {
			case MUSYCC_INTEV_NONE:
				break;
			case MUSYCC_INTEV_SACK:
				musycc_state_engine(mg, chan,
				    mg->mg_sreq[mg->mg_sreqpend].event);
				mg->mg_sreqpend =
				    (mg->mg_sreqpend + 1) & MUSYCC_SREQMASK;
				if (mg->mg_sreqpend != mg->mg_sreqprod)
					musycc_kick(mg);
				break;
			case MUSYCC_INTEV_EOM:
			case MUSYCC_INTEV_EOB:
				if (id & MUSYCC_INTD_DIR)
					musycc_txeom(mg, chan, 0);
				else
					musycc_rxeom(mg, chan, 0);
				break;
			default:
				ACCOOM_PRINTF(0, ("%s: unhandled event: %s\n",
				    mc->mc_dev.dv_xname,
				    musycc_intr_print(id)));
				break;
			}
			switch (MUSYCC_INTD_ERROR(id)) {
			case MUSYCC_INTERR_NONE:
				break;
			case MUSYCC_INTERR_COFA:
				if ((id & MUSYCC_INTD_DIR) == 0)
					/* ignore COFA for RX side */
					break;
				if (mg->mg_channels[chan]->cc_state !=
				    CHAN_RUNNING) {
					/*
					 * ignore COFA for TX side if card is
					 * not running
					 */
					break;
				}
				ACCOOM_PRINTF(0, ("%s: error: %s\n",
				    mc->mc_dev.dv_xname,
				    musycc_intr_print(id)));
#if 0
				/* digest already transmitted packets */
				musycc_txeom(mg, chan);

				/* adjust head pointer */
				musycc_dump_dma(mg);
				mg->mg_group->tx_headp[chan] =
				    htole32(mg->mg_listmap->dm_segs[0].ds_addr +
				    (caddr_t)mg->mg_dma_d[chan].tx_pend -
				    mg->mg_listkva);
				musycc_dump_dma(mg);

				musycc_sreq(mg, chan, MUSYCC_SREQ_SET(8),
				    MUSYCC_SREQ_TX, CHAN_RUNNING);
#endif
				break;
			case MUSYCC_INTERR_BUFF:
				/*
				 * log event as this should not happen,
				 * indicates PCI bus congestion
				 */
				log(LOG_ERR, "%s: internal FIFO %s\n",
				    mg->mg_channels[chan]->cc_ifp->if_xname,
				    id & MUSYCC_INTD_DIR ? "underflow" :
				    "overflow");

				/* digest queue and restarting dma engine */
				ifp = mg->mg_channels[chan]->cc_ifp;
				if (id & MUSYCC_INTD_DIR) {
					ifp->if_oerrors++;
					musycc_txeom(mg, chan, 1);
				} else {
					ifp->if_ierrors++;
					musycc_rxeom(mg, chan, 1);
				}
				break;
			case MUSYCC_INTERR_ONR:
				ACCOOM_PRINTF(0, ("%s: error: %s\n",
				    mc->mc_dev.dv_xname,
				    musycc_intr_print(id)));

				/* digest queue and restarting dma engine */
				ifp = mg->mg_channels[chan]->cc_ifp;
				if (id & MUSYCC_INTD_DIR) {
					ifp->if_oerrors++;
					musycc_txeom(mg, chan, 1);
				} else {
					ifp->if_ierrors++;
					musycc_rxeom(mg, chan, 1);
				}
				break;
			default:
				ACCOOM_PRINTF(0, ("%s: unhandled error: %s\n",
				    mc->mc_dev.dv_xname,
				    musycc_intr_print(id)));
				break;
			}
		}
		bus_space_write_4(mc->mc_st, mc->mc_sh, MUSYCC_INTRSTATUS,
		    MUSYCC_NEXTINT_SET((n + i) % MUSYCC_INTLEN));
		bus_space_barrier(mc->mc_st, mc->mc_sh, MUSYCC_INTRSTATUS,
		    sizeof(u_int32_t), BUS_SPACE_BARRIER_WRITE);
		return (1);
	} else
		return (0);
}

void
musycc_kick(struct musycc_group *mg)
{

	bus_dmamap_sync(mg->mg_dmat, mg->mg_hdlc->mc_cfgmap,
	    MUSYCC_GROUPBASE(mg->mg_gnum), sizeof(struct musycc_grpdesc),
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	ACCOOM_PRINTF(4, ("musycc_kick: group %d sreq[%d] req %08x\n",
	    mg->mg_gnum, mg->mg_sreqpend, mg->mg_sreq[mg->mg_sreqpend]));

	bus_space_write_4(mg->mg_hdlc->mc_st, mg->mg_hdlc->mc_sh,
	    MUSYCC_SERREQ(mg->mg_gnum), mg->mg_sreq[mg->mg_sreqpend].sreq);
	bus_space_barrier(mg->mg_hdlc->mc_st, mg->mg_hdlc->mc_sh,
	    MUSYCC_SERREQ(mg->mg_gnum), sizeof(u_int32_t),
	    BUS_SPACE_BARRIER_WRITE);
}

void
musycc_sreq(struct musycc_group *mg, int channel, u_int32_t req, int dir,
    enum musycc_event event)
{
#define MUSYCC_SREQINC(x, y)	\
	do {							\
		(x) = ((x) + 1) & MUSYCC_SREQMASK;		\
		if (x == y)					\
			panic("%s: sreq queue overflow",	\
			    mg->mg_hdlc->mc_dev.dv_xname);	\
	} while (0)

	struct timeval	tv;
	int		needskick;

	needskick = (mg->mg_sreqpend == mg->mg_sreqprod);
	getmicrouptime(&tv);

	ACCOOM_PRINTF(4, ("musycc_sreq: g# %d c# %d req %x dir %x\n",
	    mg->mg_gnum, channel, req, dir));

	if (dir & MUSYCC_SREQ_RX) {
		req &= ~MUSYCC_SREQ_TXDIR & ~MUSYCC_SREQ_MASK;
		req |= MUSYCC_SREQ_CHSET(channel);
		mg->mg_sreq[mg->mg_sreqprod].sreq = req;
		mg->mg_sreq[mg->mg_sreqprod].timeout = tv.tv_sec +
		    MUSYCC_SREQTIMEOUT;
		if (dir == MUSYCC_SREQ_RX)
			mg->mg_sreq[mg->mg_sreqprod].event = event;
		else
			mg->mg_sreq[mg->mg_sreqprod].event = EV_NULL;
		MUSYCC_SREQINC(mg->mg_sreqprod, mg->mg_sreqpend);
	}
	if (dir & MUSYCC_SREQ_TX) {
		req &= ~MUSYCC_SREQ_MASK;
		req |= MUSYCC_SREQ_TXDIR;
		req |= MUSYCC_SREQ_CHSET(channel);
		mg->mg_sreq[mg->mg_sreqprod].timeout = tv.tv_sec +
		    MUSYCC_SREQTIMEOUT;
		mg->mg_sreq[mg->mg_sreqprod].sreq = req;
		mg->mg_sreq[mg->mg_sreqprod].event = event;
		MUSYCC_SREQINC(mg->mg_sreqprod, mg->mg_sreqpend);
	}

	if (needskick)
		musycc_kick(mg);

#undef	MUSYCC_SREQINC
}

void
musycc_tick(struct channel_softc *cc)
{
	struct musycc_group	*mg = cc->cc_group;
	struct timeval		 tv;

	if (mg->mg_sreqpend == mg->mg_sreqprod)
		return;

	getmicrouptime(&tv);
	if (mg->mg_sreq[mg->mg_sreqpend].timeout < tv.tv_sec) {
		log(LOG_ERR, "%s: service request timeout\n",
		    cc->cc_ifp->if_xname);
		mg->mg_sreqpend++;
		/* digest all timed out SREQ */
		while (mg->mg_sreq[mg->mg_sreqpend].timeout < tv.tv_sec &&
		    mg->mg_sreqpend != mg->mg_sreqprod)
			mg->mg_sreqpend++;

		if (mg->mg_sreqpend != mg->mg_sreqprod)
			musycc_kick(mg);
	}
}

/*
 * Extension Bus API
 */
int
ebus_intr(void *arg)
{
	struct musycc_softc	*sc = arg;

	printf("%s: interrupt\n", sc->mc_dev.dv_xname);
	return (1);
}

int
ebus_attach_device(struct ebus_dev *e, struct musycc_softc *mc,
    bus_size_t offset, bus_size_t size)
{
	struct musycc_softc	*ec = mc->mc_other;

	e->base = offset << 2;
	e->size = size;
	e->st = ec->mc_st;
	return (bus_space_subregion(ec->mc_st, ec->mc_sh, offset << 2,
	    size, &e->sh));
}

u_int8_t
ebus_read(struct ebus_dev *e, bus_size_t offset)
{
	u_int8_t	value;

	value = bus_space_read_1(e->st, e->sh, offset << 2);
	bus_space_barrier(e->st, e->sh, 0, e->size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);
	return (value);
}

void
ebus_write(struct ebus_dev *e, bus_size_t offset, u_int8_t value)
{
	bus_space_write_1(e->st, e->sh, offset << 2, value);
	bus_space_barrier(e->st, e->sh, 0, e->size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);
}

void
ebus_read_buf(struct ebus_dev *rom, bus_size_t offset, void *buf, size_t size)
{
	u_int8_t	*b = buf;
	size_t		 i;

	for (i = 0; i < size; i++)
		b[i] = ebus_read(rom, offset + i);
}

void
ebus_set_led(struct channel_softc *cc, u_int8_t value)
{
	struct musycc_softc	*sc = cc->cc_group->mg_hdlc->mc_other;
	u_int8_t		 mask;

	value <<= cc->cc_group->mg_gnum * 2;
	mask = MUSYCC_LED_MASK << (cc->cc_group->mg_gnum * 2);
	
	value = (value & mask) | (sc->mc_ledstate & ~mask);

	bus_space_write_1(sc->mc_st, sc->mc_sh, sc->mc_ledbase, value);
	bus_space_barrier(sc->mc_st, sc->mc_sh, sc->mc_ledbase, 1,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	sc->mc_ledstate = value;
}

/*
 * Channel API
 */

void
musycc_attach_sppp(struct channel_softc *cc,
    int (*if_ioctl)(struct ifnet *, u_long, caddr_t))
{
	struct ifnet		*ifp;

	ifp = &cc->cc_ppp.pp_if;
	cc->cc_ifp = ifp;

	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_mtu = PP_MTU;
	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST /* | IFF_SIMPLEX */;
	cc->cc_ppp.pp_flags |= PP_CISCO;
	cc->cc_ppp.pp_flags |= PP_KEEPALIVE;
	cc->cc_ppp.pp_framebytes = 3;

	ifp->if_ioctl = if_ioctl;
	ifp->if_start = musycc_start;
	ifp->if_watchdog = musycc_watchdog;

	if_attach(ifp);
	if_alloc_sadl(ifp);
	sppp_attach(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_PPP, PPP_HEADER_LEN);
#endif /* NBPFILTER > 0 */

}

struct channel_softc *
musycc_channel_create(const char *name, u_int8_t locked)
{
	struct channel_softc	*cc;

	cc = malloc(sizeof(*cc), M_DEVBUF, M_NOWAIT);
	if (!cc)
		return (NULL);
	bzero(cc, sizeof(*cc));

	cc->cc_state = CHAN_FLOAT;
	cc->cc_locked = locked;

	/* set default timeslot map for E1 */
	cc->cc_tslots = 0xfffffffe; /* all but timeslot 0 */
	strlcpy(cc->cc_ppp.pp_if.if_xname, name,
	    sizeof(cc->cc_ppp.pp_if.if_xname));

	cc->cc_ppp.pp_if.if_softc = cc;

	return (cc);
}

int
musycc_channel_attach(struct musycc_softc *mc, struct channel_softc *cc,
    struct device *dev, u_int8_t gnum)
{
	struct musycc_group	*mg;
	int			 i;

	if (cc->cc_state != CHAN_FLOAT)
		return (-1);	/* already attached */

	if (gnum >= mc->mc_ngroups) {
		ACCOOM_PRINTF(0, ("%s: %s tries to attach to nonexistent group",
		    mc->mc_dev.dv_xname, cc->cc_ifp->if_xname));
		return (-1);
	}

	mg = &mc->mc_groups[gnum];
	for (i = 0; i < MUSYCC_NUMCHAN; i++)
		if (mg->mg_channels[i] == NULL) {
			mg->mg_channels[i] = cc;
			cc->cc_state = CHAN_IDLE;
			cc->cc_group = mg;
			cc->cc_channel = i;
			cc->cc_parent = dev;
			return (i);
		}
	return (-1);
}

void
musycc_channel_detach(struct ifnet *ifp)
{
	struct channel_softc *cc = ifp->if_softc;

	if (cc->cc_state != CHAN_FLOAT) {
		musycc_free_channel(cc->cc_group, cc->cc_channel);
		cc->cc_group->mg_channels[cc->cc_channel] = NULL;
	}

	if_detach(ifp);
}

#ifdef ACCOOM_DEBUG
const char	*musycc_events[] = {
	"NONE", "SACK", "EOB", "EOM", "EOP", "CHABT", "CHIC", "FREC",
	"SINC", "SDEC", "SFILT", "RFU", "RFU", "RFU", "RFU", "RFU"
};
const char	*musycc_errors[] = {
	"NONE", "BUFF", "COFA", "ONR", "PROT", "RFU", "RFU", "RFU",
	"OOF", "FCS", "ALIGN", "ABT", "LNG", "SHT", "SUERR", "PERR"
};
const char	*mu_proto[] = {
	"trans", "ss7", "hdlc16", "hdlc32"
};
const char	*mu_mode[] = {
	"t1", "e1", "2*e1", "4*e1", "n64"
};

char	musycc_intrbuf[48];

char *
musycc_intr_print(u_int32_t id)
{
	snprintf(musycc_intrbuf, sizeof(musycc_intrbuf),
	    "ev %s er %s grp %d chan %d dir %s",
	    musycc_events[MUSYCC_INTD_EVENT(id)],
	    musycc_errors[MUSYCC_INTD_ERROR(id)],
	    MUSYCC_INTD_GRP(id), MUSYCC_INTD_CHAN(id),
	    id & MUSYCC_INTD_DIR ? "T" : "R");
	return (musycc_intrbuf);
}

void
musycc_dump_group(int level, struct musycc_group *mg)
{
	struct musycc_grpdesc	*md = mg->mg_group;
	int			 i;

	if (level > accoom_debug)
		return;

	printf("%s: dumping group %d\n",
	    mg->mg_hdlc->mc_dev.dv_xname, mg->mg_gnum);
	printf("===========================================================\n");
	printf("global conf: %08x\n", md->global_conf);
	printf("group conf: [%08x] %s %s %s int %s%s inhib BSD %s%s poll %d\n",
	    md->group_conf,
	    md->group_conf & MUSYCC_GRCFG_TXENBL ? "TX" : "",
	    md->group_conf & MUSYCC_GRCFG_RXENBL ? "RX" : "",
	    md->group_conf & MUSYCC_GRCFG_SUBDSBL ? "" : "SUB",
	    md->group_conf & MUSYCC_GRCFG_MSKOOF ? "" : "O",
	    md->group_conf & MUSYCC_GRCFG_MSKCOFA ? "" : "C",
	    md->group_conf & MUSYCC_GRCFG_INHTBSD ? "TX" : "",
	    md->group_conf & MUSYCC_GRCFG_INHRBSD ? "RX" : "",
	    (md->group_conf & MUSYCC_GRCFG_POLL64) == MUSYCC_GRCFG_POLL64 ? 64 :
	    md->group_conf & MUSYCC_GRCFG_POLL32 ? 32 :
	    md->group_conf & MUSYCC_GRCFG_POLL16 ? 16 : 1);
	printf("port conf: [%08x] %s %s %s %s %s %s %s\n", md->port_conf,
	    mu_mode[md->port_conf & MUSYCC_PORT_MODEMASK],
	    md->port_conf & MUSYCC_PORT_TDAT_EDGE ? "TXE" : "!TXE",
	    md->port_conf & MUSYCC_PORT_TSYNC_EDGE ? "TXS" : "!TXS",
	    md->port_conf & MUSYCC_PORT_RDAT_EDGE ? "RXE" : "!RXE",
	    md->port_conf & MUSYCC_PORT_RSYNC_EDGE ? "RXS" : "!RXS",
	    md->port_conf & MUSYCC_PORT_ROOF_EDGE ? "ROOF" : "!ROOF",
	    md->port_conf & MUSYCC_PORT_TRITX ? "!tri-state" : "tri-state");
	printf("message len 1: %d 2: %d\n",
	    md->msglen_conf & MUSYCC_MAXFRM_MASK,
	    (md->msglen_conf >> MUSYCC_MAXFRM2_SHIFT) & MUSYCC_MAXFRM_MASK);
	printf("interrupt queue %x len %d\n", md->int_queuep, md->int_queuelen);
	printf("memory protection %x\n", md->memprot);
	printf("===========================================================\n");
	printf("Timeslot Map:TX\t\tRX\n");
	for (i = 0; i < 128; i++) {
		if (md->tx_tsmap[i] & MUSYCC_TSLOT_ENABLED)
			printf("%d: %s%s%s[%02d]\t\t", i,
			    md->tx_tsmap[i] & MUSYCC_TSLOT_ENABLED ? "C" : " ",
			    md->tx_tsmap[i] & MUSYCC_TSLOT_SUB ? "S" : " ",
			    md->tx_tsmap[i] & MUSYCC_TSLOT_56K ? "*" : " ",
			    MUSYCC_TSLOT_CHAN(md->tx_tsmap[i]));
		else if (md->rx_tsmap[i] & MUSYCC_TSLOT_ENABLED)
			printf("%d: \t\t", i);
		if (md->rx_tsmap[i] & MUSYCC_TSLOT_ENABLED)
			printf("%s%s%s[%02d]\n",
			    md->rx_tsmap[i] & MUSYCC_TSLOT_ENABLED ? "C" : " ",
			    md->rx_tsmap[i] & MUSYCC_TSLOT_SUB ? "S" : " ",
			    md->rx_tsmap[i] & MUSYCC_TSLOT_56K ? "*" : " ",
			    MUSYCC_TSLOT_CHAN(md->rx_tsmap[i]));
	}
	printf("===========================================================\n");
	printf("Channel config:\nTX\t\t\tRX\n");
	for (i = 0; i < 32; i++)
		if (md->tx_cconf[i] != 0) {
			printf("%s%s%s%s%s%s%s %s [%x]\t",
			    md->tx_cconf[i] & MUSYCC_CHAN_MSKBUFF ? "B" : " ",
			    md->tx_cconf[i] & MUSYCC_CHAN_MSKEOM ? "E" : " ",
			    md->tx_cconf[i] & MUSYCC_CHAN_MSKMSG ? "M" : " ",
			    md->tx_cconf[i] & MUSYCC_CHAN_MSKIDLE ? "I" : " ",
			    md->tx_cconf[i] & MUSYCC_CHAN_FCS ? "F" : "",
			    md->tx_cconf[i] & MUSYCC_CHAN_MAXLEN1 ? "1" : "",
			    md->tx_cconf[i] & MUSYCC_CHAN_MAXLEN2 ? "2" : "",
			    mu_proto[MUSYCC_CHAN_PROTO_GET(md->tx_cconf[i])],
			    md->tx_cconf[i]);
			printf("%s%s%s%s%s%s%s %s [%x]\n",
			    md->rx_cconf[i] & MUSYCC_CHAN_MSKBUFF ? "B" : " ",
			    md->rx_cconf[i] & MUSYCC_CHAN_MSKEOM ? "E" : " ",
			    md->rx_cconf[i] & MUSYCC_CHAN_MSKMSG ? "M" : " ",
			    md->rx_cconf[i] & MUSYCC_CHAN_MSKIDLE ? "I" : " ",
			    md->rx_cconf[i] & MUSYCC_CHAN_FCS ? "F" : "",
			    md->rx_cconf[i] & MUSYCC_CHAN_MAXLEN1 ? "1" : "",
			    md->rx_cconf[i] & MUSYCC_CHAN_MAXLEN2 ? "2" : "",
			    mu_proto[MUSYCC_CHAN_PROTO_GET(md->rx_cconf[i])],
			    md->rx_cconf[i]);
		}
	printf("===========================================================\n");
	musycc_dump_dma(level, mg, 0);
}

void
musycc_dump_desc(int level, struct musycc_group *mg)
{
#define READ4(x) \
	bus_space_read_4(mg->mg_hdlc->mc_st, mg->mg_hdlc->mc_sh, \
	    MUSYCC_GROUPBASE(mg->mg_gnum) + (x))
	u_int32_t	w;
	int		i;

	if (level > accoom_debug)
		return;

	printf("%s: dumping descriptor %d\n",
	    mg->mg_hdlc->mc_dev.dv_xname, mg->mg_gnum);
	printf("===========================================================\n");
	printf("global conf: %08x\n", READ4(MUSYCC_GLOBALCONF));
	w = READ4(0x060c);
	printf("group conf: [%08x] %s %s %s int %s%s inhib BSD %s%s poll %d\n",
	    w, w & MUSYCC_GRCFG_TXENBL ? "TX" : "",
	    w & MUSYCC_GRCFG_RXENBL ? "RX" : "",
	    w & MUSYCC_GRCFG_SUBDSBL ? "" : "SUB",
	    w & MUSYCC_GRCFG_MSKOOF ? "" : "O",
	    w & MUSYCC_GRCFG_MSKCOFA ? "" : "C",
	    w & MUSYCC_GRCFG_INHTBSD ? "TX" : "",
	    w & MUSYCC_GRCFG_INHRBSD ? "RX" : "",
	    (w & MUSYCC_GRCFG_POLL64) == MUSYCC_GRCFG_POLL64 ? 64 :
	    w & MUSYCC_GRCFG_POLL32 ? 32 :
	    w & MUSYCC_GRCFG_POLL16 ? 16 : 1);
	w = READ4(0x0618);
	printf("port conf: [%08x] %s %s %s %s %s %s %s\n", w,
	    mu_mode[w & MUSYCC_PORT_MODEMASK],
	    w & MUSYCC_PORT_TDAT_EDGE ? "TXE" : "!TXE",
	    w & MUSYCC_PORT_TSYNC_EDGE ? "TXS" : "!TXS",
	    w & MUSYCC_PORT_RDAT_EDGE ? "RXE" : "!RXE",
	    w & MUSYCC_PORT_RSYNC_EDGE ? "RXS" : "!RXS",
	    w & MUSYCC_PORT_ROOF_EDGE ? "ROOF" : "!ROOF",
	    w & MUSYCC_PORT_TRITX ? "!tri-state" : "tri-state");
	w = READ4(0x0614);
	printf("message len 1: %d 2: %d\n",
	    w & MUSYCC_MAXFRM_MASK,
	    (w >> MUSYCC_MAXFRM2_SHIFT) & MUSYCC_MAXFRM_MASK);
	printf("interrupt queue %x len %d\n", READ4(0x0604), READ4(0x0608));
	printf("memory protection %x\n", READ4(0x0610));
	printf("===========================================================\n");

	printf("Channel config:\nTX\t\t\t\tRX\n");
	for (i = 0; i < 32; i++) {
		w = READ4(0x0380 + i * 4);
		if (w != 0) {
			printf("%s%s%s%s%s%s%s %s [%08x]\t",
			    w & MUSYCC_CHAN_MSKBUFF ? "B" : " ",
			    w & MUSYCC_CHAN_MSKEOM ? "E" : " ",
			    w & MUSYCC_CHAN_MSKMSG ? "M" : " ",
			    w & MUSYCC_CHAN_MSKIDLE ? "I" : " ",
			    w & MUSYCC_CHAN_FCS ? "F" : "",
			    w & MUSYCC_CHAN_MAXLEN1 ? "1" : "",
			    w & MUSYCC_CHAN_MAXLEN2 ? "2" : "",
			    mu_proto[MUSYCC_CHAN_PROTO_GET(w)],
			    w);
			w = READ4(0x0580 + i * 4);
			printf("%s%s%s%s%s%s%s %s [%08x]\n",
			    w & MUSYCC_CHAN_MSKBUFF ? "B" : " ",
			    w & MUSYCC_CHAN_MSKEOM ? "E" : " ",
			    w & MUSYCC_CHAN_MSKMSG ? "M" : " ",
			    w & MUSYCC_CHAN_MSKIDLE ? "I" : " ",
			    w & MUSYCC_CHAN_FCS ? "F" : "",
			    w & MUSYCC_CHAN_MAXLEN1 ? "1" : "",
			    w & MUSYCC_CHAN_MAXLEN2 ? "2" : "",
			    mu_proto[MUSYCC_CHAN_PROTO_GET(w)],
			    w);
		}
	}
	printf("===========================================================\n");
	musycc_dump_dma(level, mg, 0);

}

void
musycc_dump_dma(int level, struct musycc_group *mg, int dir)
{
	struct musycc_grpdesc	*md = mg->mg_group;
	struct dma_desc		*dd;
	bus_addr_t		 base, addr;
	int			 i;

	if (level > accoom_debug)
		return;

	printf("DMA Pointers:\n%8s %8s %8s %8s\n",
	    "tx head", "tx msg", "rx head", "rx msg");
	for (i = 0; i < 32; i++) {
		if (md->tx_headp[i] == 0 && md->rx_headp[i] == 0)
			continue;
		printf("%08x %08x %08x %08x\n",
		    md->tx_headp[i], md->tx_msgp[i],
		    md->rx_headp[i], md->rx_msgp[i]);
	}

	base = mg->mg_listmap->dm_segs[0].ds_addr;
	for (i = 0; dir & MUSYCC_SREQ_TX && i < 32; i++) {
		if (md->tx_headp[i] == 0)
			continue;

		printf("==================================================\n");
		printf("TX DMA Ring for channel %d\n", i);
		printf("pend: %p cur: %p cnt: %d use: %d pkgs: %d\n",
		    mg->mg_dma_d[i].tx_pend, mg->mg_dma_d[i].tx_cur,
		    mg->mg_dma_d[i].tx_cnt, mg->mg_dma_d[i].tx_use,
		    mg->mg_dma_d[i].tx_pkts);
		printf("  %10s %8s %8s %8s %8s %10s\n",
		    "addr", "paddr", "next", "status", "data", "mbuf");
		dd = mg->mg_dma_d[i].tx_pend;
		do {
			addr = htole32(base + ((caddr_t)dd - mg->mg_listkva));
			printf("%s %p %08x %08x %08x %08x %p\n",
			    dd == mg->mg_dma_d[i].tx_pend ? ">" :
			    dd == mg->mg_dma_d[i].tx_cur ? "*" : " ",
			    dd, addr, dd->next, dd->status,
			    dd->data, dd->mbuf);
			dd = dd->nextdesc;
		} while (dd != mg->mg_dma_d[i].tx_pend);
	}
	for (i = 0; dir & MUSYCC_SREQ_RX && i < 32; i++) {
		if (md->rx_headp[i] == 0)
			continue;

		printf("==================================================\n");
		printf("RX DMA Ring for channel %d\n", i);
		printf("prod: %p cnt: %d\n",
		    mg->mg_dma_d[i].rx_prod, mg->mg_dma_d[i].rx_cnt);
		printf("  %8s %8s %8s %8s %10s\n",
		    "addr", "paddr", "next", "status", "data", "mbuf");
		dd = mg->mg_dma_d[i].rx_prod;
		do {
			addr = htole32(base + ((caddr_t)dd - mg->mg_listkva));
			printf("%p %08x %08x %08x %08x %p\n", dd, addr,
			    dd->next, dd->status, dd->data, dd->mbuf);
			dd = dd->nextdesc;
		} while (dd != mg->mg_dma_d[i].rx_prod);
	}
}
#endif
