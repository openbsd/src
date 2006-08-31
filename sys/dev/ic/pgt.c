/*-
 * Copyright (c) 2004 Fujitsu Laboratories of America, Inc.
 * Copyright (c) 2004 Brian Fundakowski Feldman
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/pgtreg.h>
#include <dev/ic/pgtvar.h>

#include <dev/ic/if_wireg.h>
#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wivar.h>

#define PGT_DEBUG

#ifdef PGT_DEBUG
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

/*
 * This is a driver for the Intersil Prism family of 802.11g network cards,
 * based upon version 1.2 of the Linux driver and firmware found at
 * http://www.prism54.org/.
 */

/*
 * hack to get it compiled
 */
/* got from if_wi */
#define WI_PRISM2_RES_SIZE	62
#define WI_RID_SCAN_REQ		0xFCE1
#define WI_RID_SCAN_APS		0x0600
#define WI_RID_CHANNEL_LIST	0xFD10
#define WI_RID_SCAN_RES		0xFD88
#define WI_RID_COMMS_QUALITY	0xFD43 
/* got from FreeBSD */
#define IEEE80211_WEP_OFF		0
#define IEEE80211_WEP_ON		1
#define IEEE80211_WEP_MIXED		2
#define IEEE80211_IOC_MLME		21
#define IEEE80211_IOC_AUTHMODE		7
#define IEEE80211_IOC_WEP		3
#define IEEE80211_IOC_WEPTXKEY		6
#define IEEE80211_IOC_WEPKEY		4
#define IEEE80211_POWERSAVE_ON		1
#define IEEE80211_POWERSAVE_OFF		0
#define IEEE80211_MLME_UNAUTHORIZE	5
#define IEEE80211_MLME_AUTHORIZE	4

struct cfdriver pgt_cd = {
        NULL, "pgt", DV_IFNET
};

void	 pgt_write_memory_barrier(struct pgt_softc *);
uint32_t pgt_read_4(struct pgt_softc *, uint16_t);
void	 pgt_write_4(struct pgt_softc *, uint16_t, uint32_t);
void	 pgt_write_4_flush(struct pgt_softc *, uint16_t, uint32_t);
void	 pgt_debug_events(struct pgt_softc *, const char *);
uint32_t pgt_queue_frags_pending(struct pgt_softc *, enum pgt_queue);
void	 pgt_reinit_rx_desc_frag(struct pgt_softc *, struct pgt_desc *);
int	 pgt_load_tx_desc_frag(struct pgt_softc *, enum pgt_queue,
	     struct pgt_desc *);
void	 pgt_unload_tx_desc_frag(struct pgt_softc *, struct pgt_desc *);
void	 pgt_enter_critical(struct pgt_softc *);
int	 pgt_try_enter_data_critical(struct pgt_softc *);
void	 pgt_exit_critical(struct pgt_softc *);
#if 0
void	 pgt_try_exit_data_critical(struct pgt_softc *);
#endif
int	 pgt_load_firmware(struct pgt_softc *);
void	 pgt_cleanup_queue(struct pgt_softc *, enum pgt_queue,
	     struct pgt_frag []);
int	 pgt_reset(struct pgt_softc *);
void	 pgt_disable(struct pgt_softc *, unsigned int);
void	 pgt_init_intr(struct pgt_softc *);
void	 pgt_update_intr(struct pgt_softc *, struct mbuf ***, int);
struct mbuf
	*pgt_ieee80211_encap(struct pgt_softc *, struct ether_header *,
	     struct mbuf *, struct ieee80211_node **);
void	 pgt_input_frames(struct pgt_softc *, struct mbuf *);
void	 pgt_wakeup_intr(struct pgt_softc *);
void	 pgt_sleep_intr(struct pgt_softc *);
void	 pgt_empty_traps(struct pgt_softc_kthread *);
void	 pgt_per_device_kthread(void *);
struct mbuf
	*pgt_alloc_async(size_t);
void	 pgt_async_reset(struct pgt_softc *);
void	 pgt_async_trap(struct pgt_softc *, uint32_t, void *, size_t);
void	 pgt_async_update(struct pgt_softc *);
//void	 pgt_poll(struct ifnet *, enum poll_cmd, int);
void	 pgt_intr_body(struct pgt_softc *, struct mbuf **, int);
void	 pgt_txdone(struct pgt_softc *, enum pgt_queue);
void	 pgt_rxdone(struct pgt_softc *, enum pgt_queue);
void	 pgt_trap_received(struct pgt_softc *, uint32_t, void *, size_t);
void	 pgt_mgmtrx_completion(struct pgt_softc *, struct pgt_mgmt_desc *);
int	 pgt_datarx_completion(struct pgt_softc *, enum pgt_queue,
	     struct mbuf ***, int);
int	 pgt_oid_get(struct pgt_softc *, enum pgt_oid, void *, size_t);
int	 pgt_oid_retrieve(struct pgt_softc *, enum pgt_oid, void *, size_t);
int	 pgt_oid_set(struct pgt_softc *, enum pgt_oid, const void *, size_t);
void	 pgt_state_dump(struct pgt_softc *);
int	 pgt_mgmt_request(struct pgt_softc *, struct pgt_mgmt_desc *);
void	 pgt_desc_transmit(struct pgt_softc *, enum pgt_queue,
	     struct pgt_desc *, uint16_t, int);
void	 pgt_maybe_trigger(struct pgt_softc *, enum pgt_queue);
struct ieee80211_node
	*pgt_ieee80211_node_alloc(struct ieee80211com *);
void	 pgt_ieee80211_newassoc(struct ieee80211com *,
	     struct ieee80211_node *, int);
void	 pgt_ieee80211_node_free(struct ieee80211com *,
	    struct ieee80211_node *);
void	 pgt_ieee80211_node_copy(struct ieee80211com *,
	     struct ieee80211_node *,
	     const struct ieee80211_node *);
int	 pgt_ieee80211_send_mgmt(struct ieee80211com *,
	     struct ieee80211_node *, int, int);
int	 pgt_net_attach(struct pgt_softc *);
void	 pgt_net_detach(struct pgt_softc *);
void	 pgt_start(struct ifnet *);
void	 pgt_start_body(struct pgt_softc *, struct ieee80211com *,
	     struct ifnet *);
int	 pgt_ioctl(struct ifnet *, u_long, caddr_t);
void	 pgt_obj_bss2scanres(struct pgt_softc *,
	     struct pgt_obj_bss *, struct wi_scan_res *, uint32_t);
int	 pgt_node_set_authorization(struct pgt_softc *,
	     struct pgt_ieee80211_node *,
	     enum pin_dot1x_authorization);
#if 0
int	 pgt_do_mlme_sta(struct pgt_softc *, struct ieee80211req_mlme *);
int	 pgt_do_mlme_hostap(struct pgt_softc *, struct ieee80211req_mlme *);
int	 pgt_do_mlme_adhoc(struct pgt_softc *, struct ieee80211req_mlme *);
int	 pgt_80211_set(struct pgt_softc *, struct ieee80211req *);
#endif
int	 pgt_wavelan_get(struct pgt_softc *, struct wi_req *);
int	 pgt_wavelan_set(struct pgt_softc *, struct wi_req *);
void	 node_mark_active_ap(void *, struct ieee80211_node *);
void	 node_mark_active_adhoc(void *, struct ieee80211_node *);
void	 pgt_periodic(struct ifnet *);
int	 pgt_init(struct ifnet *);
void	 pgt_update_hw_from_sw(struct pgt_softc *, int, int);
void	 pgt_update_hw_from_nodes(struct pgt_softc *);
void	 pgt_hostap_handle_mlme(struct pgt_softc *, uint32_t,
	     struct pgt_obj_mlme *);
void	 pgt_update_sw_from_hw(struct pgt_softc *,
	     struct pgt_async_trap *, struct mbuf *);
int	 pgt_media_change(struct ifnet *);
void	 pgt_media_status(struct ifnet *, struct ifmediareq *);
int	 pgt_new_state(struct ieee80211com *, enum ieee80211_state, int);
int	 pgt_drain_tx_queue(struct pgt_softc *, enum pgt_queue);
int	 pgt_dma_alloc(struct pgt_softc *);
int	 pgt_dma_alloc_queue(struct pgt_softc *sc, enum pgt_queue pq);
void	 pgt_dma_free(struct pgt_softc *);
void	 pgt_dma_free_queue(struct pgt_softc *sc, enum pgt_queue pq);

void
pgt_attachhook(void *xsc)
{
	struct pgt_softc *sc = xsc;
	int error;

	error = pgt_attach(sc);
	if (error) {
		printf("%s: attach error\n", sc->sc_dev.dv_xname);
		return;
	}
}

void
pgt_write_memory_barrier(struct pgt_softc *sc)
{
	bus_space_barrier(sc->sc_iotag, sc->sc_iohandle, 0, 0,
	    BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
pgt_read_4(struct pgt_softc *sc, uint16_t offset)
{
	return (bus_space_read_4(sc->sc_iotag, sc->sc_iohandle, offset));
}

void
pgt_write_4(struct pgt_softc *sc, uint16_t offset, uint32_t value)
{
	bus_space_write_4(sc->sc_iotag, sc->sc_iohandle, offset, value);
}

/*
 * Write out 4 bytes and cause a PCI flush by reading back in on a
 * harmless register.
 */
void
pgt_write_4_flush(struct pgt_softc *sc, uint16_t offset, uint32_t value)
{
	bus_space_write_4(sc->sc_iotag, sc->sc_iohandle, offset, value);
	(void)bus_space_read_4(sc->sc_iotag, sc->sc_iohandle, PGT_REG_INT_EN);
}

/*
 * Print the state of events in the queues from an interrupt or a trigger.
 */
void
pgt_debug_events(struct pgt_softc *sc, const char *when)
{
#define	COUNT(i)							\
	letoh32(sc->sc_cb->pcb_driver_curfrag[i]) -			\
	letoh32(sc->sc_cb->pcb_device_curfrag[i])
	if (sc->sc_debug & SC_DEBUG_EVENTS)
		printf("%s: ev%s: %u %u %u %u %u %u\n",
		    sc->sc_dev.dv_xname, when,
		    COUNT(0), COUNT(1), COUNT(2), COUNT(3), COUNT(4), COUNT(5));
#undef COUNT
}

uint32_t
pgt_queue_frags_pending(struct pgt_softc *sc, enum pgt_queue pq)
{
	return (letoh32(sc->sc_cb->pcb_driver_curfrag[pq]) -
	    letoh32(sc->sc_cb->pcb_device_curfrag[pq]));
}

void
pgt_reinit_rx_desc_frag(struct pgt_softc *sc, struct pgt_desc *pd)
{
	pd->pd_fragp->pf_addr = htole32((uint32_t)pd->pd_dmaaddr);
	pd->pd_fragp->pf_size = htole16(PGT_FRAG_SIZE);
	pd->pd_fragp->pf_flags = htole16(0);

	bus_dmamap_sync(sc->sc_dmat, pd->pd_dmam, 0, pd->pd_dmam->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
}

int
pgt_load_tx_desc_frag(struct pgt_softc *sc, enum pgt_queue pq,
    struct pgt_desc *pd)
{
	int error;

	error = bus_dmamap_load(sc->sc_dmat, pd->pd_dmam, pd->pd_mem,
	    PGT_FRAG_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to load %s tx DMA: %d\n",
		    sc->sc_dev.dv_xname,
		    pgt_queue_is_data(pq) ? "data" : "mgmt", error);
		return (error);
	}
	pd->pd_fragp->pf_addr = htole32((uint32_t)pd->pd_dmaaddr);
	pd->pd_fragp->pf_size = htole16(PGT_FRAG_SIZE);
	pd->pd_fragp->pf_flags = htole16(0);

	bus_dmamap_sync(sc->sc_dmat, pd->pd_dmam, 0, pd->pd_dmam->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	return (0);
}

void
pgt_unload_tx_desc_frag(struct pgt_softc *sc, struct pgt_desc *pd)
{
        bus_dmamap_unload(sc->sc_dmat, pd->pd_dmam);
	pd->pd_dmaaddr = 0;
}

/*
 * The critical lock uses int sc_critical to signify whether there are
 * currently transmissions in flight (> 0), is a management in progress
 * (<= -1), or the device is not critical (== 0).  Management waiters
 * prevent further transmissions from contributing to sc_critical --
 * that is, make it monotonically decrease again back to 0 as
 * transmissions complete or time out.
 */
void
pgt_enter_critical(struct pgt_softc *sc)
{
	//if (sc->sc_critical < 0 && sc->sc_critical_thread == curthread) {
	if (sc->sc_critical < 0) {
		sc->sc_critical--;
	} else {
		while (sc->sc_critical != 0) {
			sc->sc_refcnt++;
			//cv_wait(&sc->sc_critical_cv, &sc->sc_lock);
			sc->sc_refcnt--;
		}
		sc->sc_critical = -1;
		//sc->sc_critical_thread = curthread;
	}
}

void
pgt_exit_critical(struct pgt_softc *sc)
{
	if (++sc->sc_critical == 0) {
		sc->sc_critical_thread = NULL;
		//if (sc->sc_critical_cv.cv_waiters == 0 &&
		    if (sc->sc_flags & SC_START_DESIRED) {
			sc->sc_flags &= ~SC_START_DESIRED;
			if (sc->sc_ic.ic_if.if_flags & IFF_RUNNING)
				pgt_start_body(sc, &sc->sc_ic,
				    &sc->sc_ic.ic_if);
		} else {
			//cv_signal(&sc->sc_critical_cv);
		}
	}
}

int
pgt_try_enter_data_critical(struct pgt_softc *sc)
{
	if (sc->sc_critical <= -1) {
	    // || sc->sc_critical_cv.cv_waiters > 0) {
		/*
		 * Don't start new data packets if management
		 * wants a chance.
		 */
		sc->sc_flags |= SC_START_DESIRED;
		return (0);
	} else  {
		sc->sc_critical++;
		return (1);
	}
}

#if 0
void
pgt_try_exit_data_critical(struct pgt_softc *sc)
{
	if (--sc->sc_critical == 0)
		cv_signal(&sc->sc_critical_cv);
}
#endif

int
pgt_load_firmware(struct pgt_softc *sc)
{
	int error, reg, dirreg, fwoff, ucodeoff, fwlen;
	uint8_t *ucode;
	const uint32_t *uc;
	size_t size;
	char *name;

	if (sc->sc_flags & SC_ISL3877)
		name = "pgt-isl3877";
	else
		name = "pgt-isl3890";	/* includes isl3880 */

	error = loadfirmware(name, &ucode, &size);

	if (error != 0) {
		printf("%s: error %d, could not read microcode %s!\n",
		    sc->sc_dev.dv_xname, error, name);
		return (EIO);
	}

	if (size & 3) {
		printf("%s: bad firmware size %u\n",
		    sc->sc_dev.dv_xname, size);
		free(ucode, M_DEVBUF);
		return (EINVAL);
	}

	pgt_reboot(sc);

	fwoff = 0;
	ucodeoff = 0;
	uc = (const uint32_t *)ucode;
	reg = PGT_FIRMWARE_INTERNAL_OFFSET;
	while (fwoff < size) {
		pgt_write_4_flush(sc, PGT_REG_DIR_MEM_BASE, reg);

		if ((size - fwoff) >= PGT_DIRECT_MEMORY_SIZE)
			fwlen = PGT_DIRECT_MEMORY_SIZE;
		else
			fwlen = size - fwoff;

		dirreg = PGT_DIRECT_MEMORY_OFFSET;
		while (fwlen > 4) {
			pgt_write_4(sc, dirreg, uc[ucodeoff]);
			fwoff += 4;
			dirreg += 4;
			reg += 4;
			fwlen -= 4;
			ucodeoff++;
		}
		pgt_write_4_flush(sc, dirreg, uc[ucodeoff]);
		fwoff += 4;
		dirreg += 4;
		reg += 4;
		fwlen -= 4;
		ucodeoff++;
	}
	DPRINTF(("%s: %d bytes microcode loaded from %s\n",
	    sc->sc_dev.dv_xname, fwoff, name));

	reg = pgt_read_4(sc, PGT_REG_CTRL_STAT);
	reg &= ~(PGT_CTRL_STAT_RESET | PGT_CTRL_STAT_CLOCKRUN);
	reg |= PGT_CTRL_STAT_RAMBOOT;
	pgt_write_4_flush(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);

	reg |= PGT_CTRL_STAT_RESET;
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);

	reg &= ~PGT_CTRL_STAT_RESET;
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);

	free(ucode, M_DEVBUF);
	
	return (0);
}

void
pgt_cleanup_queue(struct pgt_softc *sc, enum pgt_queue pq,
    struct pgt_frag pqfrags[])
{
	struct pgt_desc *pd;
	unsigned int i;

	sc->sc_cb->pcb_device_curfrag[pq] = htole32(0);
	i = 0;
	TAILQ_FOREACH(pd, &sc->sc_freeq[pq], pd_link) {
		pd->pd_fragnum = i;
		pd->pd_fragp = &pqfrags[i];
		if (pgt_queue_is_rx(pq))
			pgt_reinit_rx_desc_frag(sc, pd);
		i++;
	}
	sc->sc_freeq_count[pq] = i;
	/*
	 * The ring buffer describes how many free buffers are available from
	 * the host (for receive queues) or how many are pending (for
	 * transmit queues).
	 */
	if (pgt_queue_is_rx(pq))
		sc->sc_cb->pcb_driver_curfrag[pq] = htole32(i);
	else
		sc->sc_cb->pcb_driver_curfrag[pq] = htole32(0);
}

/*
 * Turn off interrupts, reset the device (possibly loading firmware),
 * and put everything in a known state.
 */
int
pgt_reset(struct pgt_softc *sc)
{
	int error;

	/* disable all interrupts */
	pgt_write_4_flush(sc, PGT_REG_INT_EN, 0x00000000);
	DELAY(PGT_WRITEIO_DELAY);

	/*
	 * Set up the management receive queue, assuming there are no
	 * requests in progress.
	 */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_PREWRITE);
	pgt_cleanup_queue(sc, PGT_QUEUE_DATA_LOW_RX,
	    &sc->sc_cb->pcb_data_low_rx[0]);
	pgt_cleanup_queue(sc, PGT_QUEUE_DATA_LOW_TX,
	    &sc->sc_cb->pcb_data_low_tx[0]);
	pgt_cleanup_queue(sc, PGT_QUEUE_DATA_HIGH_RX,
	    &sc->sc_cb->pcb_data_high_rx[0]);
	pgt_cleanup_queue(sc, PGT_QUEUE_DATA_HIGH_TX,
	    &sc->sc_cb->pcb_data_high_tx[0]);
	pgt_cleanup_queue(sc, PGT_QUEUE_MGMT_RX,
	    &sc->sc_cb->pcb_mgmt_rx[0]);
	pgt_cleanup_queue(sc, PGT_QUEUE_MGMT_TX,
	    &sc->sc_cb->pcb_mgmt_tx[0]);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_PREREAD);

	/* load firmware */
	if (sc->sc_flags & SC_NEEDS_FIRMWARE) {
		error = pgt_load_firmware(sc);
		if (error) {
			printf("%s: firmware load failed\n",
			    sc->sc_dev.dv_xname);
			return (error);
		}
		sc->sc_flags &= ~SC_NEEDS_FIRMWARE;
		DPRINTF(("%s: firmware loaded\n", sc->sc_dev.dv_xname));
	}

	/* upload the control block's DMA address */
	//pgt_write_4_flush(sc, PGT_REG_CTRL_BLK_BASE,
	//    htole32((uint32_t)sc->sc_cbdmabusaddr));
	//DELAY(PGT_WRITEIO_DELAY);

	/* send a reset event */
	pgt_write_4_flush(sc, PGT_REG_DEV_INT, PGT_DEV_INT_RESET);
	DELAY(PGT_WRITEIO_DELAY);

	/* await only the initialization interrupt */
	pgt_write_4_flush(sc, PGT_REG_INT_EN, PGT_INT_STAT_INIT);	
	DELAY(PGT_WRITEIO_DELAY);

	return (0);
}

/*
 * If we're trying to reset and the device has seemingly not been detached,
 * we'll spend a minute seeing if we can't do the reset.
 */
void
pgt_disable(struct pgt_softc *sc, unsigned int flag)
{
	struct ieee80211com *ic;
	uint32_t reg;
	unsigned int wokeup;
	int tries = 6, tryagain;

	ic = &sc->sc_ic;

	if (flag == SC_DYING && sc->sc_flags & SC_DYING) {
		while (sc->sc_drainer != NULL);
			//(void)msleep(&sc->sc_drainer, &sc->sc_lock,
			//    PZERO, "pffol1", hz / 10);
		//goto out2;
		return;
	}
	if (flag == SC_NEEDS_RESET) {
		if (sc->sc_drainer != NULL || sc->sc_flags & SC_GONE)
			/*
			 * Multiple pending resets are not useful.
			 */
			//goto out2;
			return;
	} else {
		while (sc->sc_drainer != NULL);
			//(void)msleep(&sc->sc_drainer, &sc->sc_lock,
			//    PZERO, "pffol1", hz / 10);
	}
	ic->ic_if.if_flags &= ~IFF_RUNNING;
	sc->sc_flags |= SC_UNINITIALIZED;
#ifdef DEVICE_POLLING
	ether_poll_deregister(&ic->ic_if);
	/* Turn back on interrupts. */
	pgt_write_4_flush(sc, PGT_REG_INT_EN, PGT_INT_STAT_SOURCES);	
	DELAY(PGT_WRITEIO_DELAY);
#endif
	//sc->sc_drainer = curthread;
	sc->sc_flags |= flag;
	/*
	 * The detacher has to wait for all activity to cease (the refcount
	 * will reach 1 and sc_async_events will have been emptied).  If
	 * we were to drain while doing just a "reset" then this could
	 * deadlock.
	 */
	pgt_drain_tx_queue(sc, PGT_QUEUE_DATA_LOW_TX);
	pgt_drain_tx_queue(sc, PGT_QUEUE_DATA_HIGH_TX);
	pgt_drain_tx_queue(sc, PGT_QUEUE_MGMT_TX);
	if (flag == SC_DYING) {
		while (sc->sc_refcnt > 1);
			//(void)msleep(&sc->sc_drainer, &sc->sc_lock,
			//    PZERO, "pffol2", hz / 10);
	}
trying_again:
	tryagain = 0;
	/* disable all interrupts */
	pgt_write_4_flush(sc, PGT_REG_INT_EN, 0x00000000);
	DELAY(PGT_WRITEIO_DELAY);
	if (sc->sc_intcookie != NULL) {
		//bus_teardown_intr(sc->sc_dev, sc->sc_intres, sc->sc_intcookie);
		sc->sc_intcookie = NULL;
	}
	reg = pgt_read_4(sc, PGT_REG_CTRL_STAT);
	reg &= ~(PGT_CTRL_STAT_RESET | PGT_CTRL_STAT_RAMBOOT);
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);
	reg |= PGT_CTRL_STAT_RESET;
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);
	reg &= ~PGT_CTRL_STAT_RESET;
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);
	do {
		wokeup = 0;
		/*
		 * We don't expect to be woken up, just to drop the lock
		 * and time out.  Only tx queues can have anything valid
		 * on them outside of an interrupt.
		 */
		while (!TAILQ_EMPTY(&sc->sc_mgmtinprog)) {
			struct pgt_mgmt_desc *pmd;

			pmd = TAILQ_FIRST(&sc->sc_mgmtinprog);
			TAILQ_REMOVE(&sc->sc_mgmtinprog, pmd, pmd_link);
			pmd->pmd_error = ENETRESET;
			wakeup_one(pmd);
			if (sc->sc_debug & SC_DEBUG_MGMT)
				printf("%s: queue: mgmt %p <- 0x%x (drained)\n",
				    sc->sc_dev.dv_xname, pmd, pmd->pmd_oid);
			wokeup++;
		}
		if (wokeup > 0) {
			//(void)msleep(&sc->sc_drainer, &sc->sc_lock,
			//    PZERO, "pffol3", hz / 10);
			if (flag == SC_NEEDS_RESET && sc->sc_flags & SC_DYING) {
				sc->sc_flags &= ~flag;
				goto out;
			}
		}
	} while (wokeup > 0);
	if (flag == SC_NEEDS_RESET && !(sc->sc_flags & SC_GONE)) {
		int error;

		printf("%s: resetting\n", sc->sc_dev.dv_xname);
		sc->sc_refcnt++;
		sc->sc_flags &= ~SC_POWERSAVE;
		sc->sc_flags |= SC_NEEDS_FIRMWARE;
		//error = bus_setup_intr(sc->sc_dev, sc->sc_intres,
		//    INTR_TYPE_NET | INTR_MPSAFE, pgt_intr,
		//    &ic->ic_if, &sc->sc_intcookie);
		if (error != 0 || sc->sc_flags & SC_DYING) {
			if (error != 0) {
				printf("%s: failure establishing irq in "
				    "reset: %d\n",
				    sc->sc_dev.dv_xname, error);
				sc->sc_flags |= SC_DYING;
			}
			sc->sc_refcnt--;
			goto out;
		}
		error = pgt_reset(sc);
		if (error == 0) {
			//(void)msleep(&sc->sc_flags, &sc->sc_lock, PZERO,
			//    "pffres", hz * 10);
			if (sc->sc_flags & SC_UNINITIALIZED) {
				printf("%s: not responding\n",
				    sc->sc_dev.dv_xname);
				/* Thud.  It was probably removed. */
				if (--tries == 0)
					sc->sc_flags |= SC_GONE;
				else
					tryagain = 1;
			} else {
				/* await all interrupts */
				pgt_write_4_flush(sc, PGT_REG_INT_EN,
				    PGT_INT_STAT_SOURCES);	
				DELAY(PGT_WRITEIO_DELAY);
				ic->ic_if.if_flags |= IFF_RUNNING;
			}
		}
		sc->sc_refcnt--;
		if (tryagain)
			goto trying_again;
		sc->sc_flags &= ~flag;
		if (ic->ic_if.if_flags & IFF_RUNNING)
			pgt_update_hw_from_sw(sc,
			    ic->ic_state != IEEE80211_S_INIT,
			    ic->ic_opmode != IEEE80211_M_MONITOR);
	}
out:
	sc->sc_drainer = NULL;
	wakeup(&sc->sc_drainer);
}

int
pgt_attach(struct pgt_softc *sc)
{
	int error;

	error = pgt_dma_alloc(sc);
	if (error)
		return (error);

	sc->sc_ic.ic_if.if_softc = sc;
	sc->sc_refcnt = 1;
	TAILQ_INIT(&sc->sc_mgmtinprog);
	TAILQ_INIT(&sc->sc_kthread.sck_traps);
	sc->sc_flags |= SC_NEEDS_FIRMWARE | SC_UNINITIALIZED;
	/*
	error = bus_setup_intr(dev, sc->sc_intres, INTR_TYPE_NET | INTR_MPSAFE,
	    pgt_intr, &sc->sc_ic.ic_if, &sc->sc_intcookie);
	if (error != 0) {
		printf("%s: failure establishing interrupt: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto failed;
	}
	*/
	sc->sc_80211_ioc_wep = IEEE80211_WEP_OFF;
	sc->sc_80211_ioc_auth = IEEE80211_AUTH_OPEN;

	error = pgt_reset(sc);
	if (error)
		goto failed;

	sc->sc_refcnt++;
	tsleep(&sc->sc_flags, 0, "pftres", hz);
	sc->sc_refcnt--;
	if (sc->sc_flags & SC_UNINITIALIZED) {
		printf("%s: not responding\n", sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
	} else {
		/* await all interrupts */
		pgt_write_4_flush(sc, PGT_REG_INT_EN, PGT_INT_STAT_SOURCES);
		DELAY(PGT_WRITEIO_DELAY);
	}
	if (error)
		goto failed;

	return (0);

	error = pgt_net_attach(sc);
	if (error)
		goto failed;

	ieee80211_new_state(&sc->sc_ic, IEEE80211_S_INIT, -1);

failed:
	pgt_disable(sc, SC_DYING);
	pgt_reboot(sc);

	return (error);
}

int
pgt_detach(struct pgt_softc *sc)
{
	pgt_net_detach(sc);
	sc->sc_flags |= SC_GONE;
	pgt_disable(sc, SC_DYING);
	pgt_reboot(sc);

	return (0);
}

void
pgt_reboot(struct pgt_softc *sc)
{
	uint32_t reg;

	reg = pgt_read_4(sc, PGT_REG_CTRL_STAT);
	reg &= ~(PGT_CTRL_STAT_RESET | PGT_CTRL_STAT_RAMBOOT);
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);

	reg |= PGT_CTRL_STAT_RESET;
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_WRITEIO_DELAY);

	reg &= ~PGT_CTRL_STAT_RESET;
	pgt_write_4(sc, PGT_REG_CTRL_STAT, reg);
	pgt_write_memory_barrier(sc);
	DELAY(PGT_RESET_DELAY);
}

void
pgt_init_intr(struct pgt_softc *sc)
{
	if ((sc->sc_flags & SC_UNINITIALIZED) == 0) {
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s: spurious initialization\n",
			    sc->sc_dev.dv_xname);
	} else {
		sc->sc_flags &= ~SC_UNINITIALIZED;
		wakeup(&sc->sc_flags);
	}
}

/*
 * If called with a NULL last_nextpkt, only the mgmt queue will be checked
 * for new packets.
 */
void
pgt_update_intr(struct pgt_softc *sc, struct mbuf ***last_nextpkt,
    int max_datarx_count)
{
	/* priority order */
	enum pgt_queue pqs[PGT_QUEUE_COUNT] = {
	    PGT_QUEUE_MGMT_TX, PGT_QUEUE_MGMT_RX, 
	    PGT_QUEUE_DATA_HIGH_TX, PGT_QUEUE_DATA_HIGH_RX, 
	    PGT_QUEUE_DATA_LOW_TX, PGT_QUEUE_DATA_LOW_RX
	};
	uint32_t npend;
	unsigned int dirtycount;
	int i, prevwasmf;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_PREWRITE);
	pgt_debug_events(sc, "intr");
	/*
	 * Check for completion of tx in their dirty queues.
	 * Check completion of rx into their dirty queues.
	 */
	for (i = 0; i < PGT_QUEUE_COUNT; i++) {
		size_t qdirty, qfree, qtotal;

		qdirty = sc->sc_dirtyq_count[pqs[i]];
		qfree = sc->sc_freeq_count[pqs[i]];
		qtotal = qdirty + qfree;
		/*
		 * We want the wrap-around here.
		 */
		if (pgt_queue_is_rx(pqs[i])) {
			int data;

			data = pgt_queue_is_data(pqs[i]);
#ifdef PGT_BUGGY_INTERRUPT_RECOVERY
			if (last_nextpkt == NULL && data)
				continue;
#endif
			npend = pgt_queue_frags_pending(sc, pqs[i]);
			/*
			 * Receive queues clean up below, so qfree must
			 * always be qtotal (qdirty is 0).
			 */
			if (npend > qfree) {
				if (sc->sc_flags & SC_DEBUG_UNEXPECTED)
					printf("%s: rx queue [%u] overflowed "
					    "by %u\n",
					    sc->sc_dev.dv_xname,
					    pqs[i], npend - qfree);
				sc->sc_flags |= SC_INTR_RESET;
				break;
			}
			while (qfree-- > npend) {
#ifdef DEVICE_POLLING
				if (data && max_datarx_count != -1) {
					if (max_datarx_count-- == 0)
						break;
				}
#endif
				pgt_rxdone(sc, pqs[i]);
			}
		} else {
			npend = pgt_queue_frags_pending(sc, pqs[i]);
			if (npend > qdirty) {
				if (sc->sc_flags & SC_DEBUG_UNEXPECTED)
					printf("%s: tx queue [%u] underflowed "
					    "by %u\n",
					    sc->sc_dev.dv_xname,
					    pqs[i], npend - qdirty);
				sc->sc_flags |= SC_INTR_RESET;
				break;
			}
			/*
			 * If the free queue was empty, or the data transmit
			 * queue just became empty, wake up any waiters.
			 */
			if (qdirty > npend) {
				if (TAILQ_EMPTY(&sc->sc_freeq[pqs[i]]))
					wakeup(&sc->sc_freeq[pqs[i]]);
				while (qdirty-- > npend)
					pgt_txdone(sc, pqs[i]);
			}
		}
	}

	/*
	 * This is the deferred completion for received management frames
	 * and where we queue network frames for stack input. 
	 */
	dirtycount = sc->sc_dirtyq_count[PGT_QUEUE_MGMT_RX];
	while (!TAILQ_EMPTY(&sc->sc_dirtyq[PGT_QUEUE_MGMT_RX])) {
		struct pgt_mgmt_desc *pmd;

		pmd = TAILQ_FIRST(&sc->sc_mgmtinprog);
		/*
		 * If there is no mgmt request in progress or the operation
		 * returned is explicitly a trap, this pmd will essentially
		 * be ignored.
		 */
		pgt_mgmtrx_completion(sc, pmd);
	}
	sc->sc_cb->pcb_driver_curfrag[PGT_QUEUE_MGMT_RX] =
	    htole32(dirtycount +
		letoh32(sc->sc_cb->pcb_driver_curfrag[PGT_QUEUE_MGMT_RX]));

	dirtycount = sc->sc_dirtyq_count[PGT_QUEUE_DATA_HIGH_RX];
	prevwasmf = 0;
	while (!TAILQ_EMPTY(&sc->sc_dirtyq[PGT_QUEUE_DATA_HIGH_RX]))
		prevwasmf = pgt_datarx_completion(sc, PGT_QUEUE_DATA_HIGH_RX,
		    last_nextpkt, prevwasmf);
	sc->sc_cb->pcb_driver_curfrag[PGT_QUEUE_DATA_HIGH_RX] =
	    htole32(dirtycount +
		letoh32(sc->sc_cb->pcb_driver_curfrag[PGT_QUEUE_DATA_HIGH_RX]));

	dirtycount = sc->sc_dirtyq_count[PGT_QUEUE_DATA_LOW_RX];
	prevwasmf = 0;
	while (!TAILQ_EMPTY(&sc->sc_dirtyq[PGT_QUEUE_DATA_LOW_RX]))
		prevwasmf = pgt_datarx_completion(sc, PGT_QUEUE_DATA_LOW_RX,
		    last_nextpkt, prevwasmf);
	sc->sc_cb->pcb_driver_curfrag[PGT_QUEUE_DATA_LOW_RX] =
	    htole32(dirtycount +
		letoh32(sc->sc_cb->pcb_driver_curfrag[PGT_QUEUE_DATA_LOW_RX]));

	/*
	 * Write out what we've finished with.
	 */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_PREREAD);
}

struct mbuf *
pgt_ieee80211_encap(struct pgt_softc *sc, struct ether_header *eh,
    struct mbuf *m, struct ieee80211_node **ni)
{
	struct ieee80211com *ic;
	struct ieee80211_frame *frame;
	struct llc *snap;

	ic = &sc->sc_ic;
	M_PREPEND(m, sizeof(frame) + sizeof(snap), M_DONTWAIT);
	if (m != NULL)
		m = m_pullup(m, sizeof(frame) + sizeof(snap));
	if (m == NULL)
		return (m);
	frame = mtod(m, struct ieee80211_frame *);
	snap = (struct llc *)&frame[1];
	if (ni != NULL) {
		if (ic->ic_opmode == IEEE80211_M_STA ||
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			*ni = ieee80211_ref_node(ic->ic_bss);
		} else {
			*ni = ieee80211_find_node(ic, eh->ether_shost);
			/*
			 * Make up associations for ad-hoc mode.  To support
			 * ad-hoc WPA, we'll need to maintain a bounded
			 * pool of ad-hoc stations.
			 */
			if (*ni == NULL &&
			    ic->ic_opmode != IEEE80211_M_HOSTAP) {
				*ni = ieee80211_dup_bss(ic, eh->ether_shost);
				if (*ni != NULL) {
					(*ni)->ni_associd = 1;
					ic->ic_newassoc(ic, *ni, 1);
				}
			}
			if (*ni == NULL) {
				m_freem(m);
				return (NULL);
			}
		}
		(*ni)->ni_inact = 0;
	}
	snap->llc_dsap = snap->llc_ssap = LLC_SNAP_LSAP;
	snap->llc_control = LLC_UI;
	snap->llc_snap.org_code[0] = 0;
	snap->llc_snap.org_code[1] = 0;
	snap->llc_snap.org_code[2] = 0;
	snap->llc_snap.ether_type = eh->ether_type;
	frame->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	/* Doesn't look like much of the 802.11 header is available. */
	*(uint16_t *)frame->i_dur = *(uint16_t *)frame->i_seq = 0;
	/*
	 * Translate the addresses; WDS is not handled.
	 */
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		frame->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
		IEEE80211_ADDR_COPY(frame->i_addr1, eh->ether_dhost);
		IEEE80211_ADDR_COPY(frame->i_addr2, ic->ic_bss->ni_bssid);
		IEEE80211_ADDR_COPY(frame->i_addr3, eh->ether_shost);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		frame->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(frame->i_addr1, eh->ether_dhost);
		IEEE80211_ADDR_COPY(frame->i_addr2, eh->ether_shost);
		IEEE80211_ADDR_COPY(frame->i_addr3, ic->ic_bss->ni_bssid);
		break;
	case IEEE80211_M_HOSTAP:
		/* HostAP forwarding defaults to being done on firmware. */
		frame->i_fc[1] = IEEE80211_FC1_DIR_TODS;
		IEEE80211_ADDR_COPY(frame->i_addr1, ic->ic_bss->ni_bssid);
		IEEE80211_ADDR_COPY(frame->i_addr2, eh->ether_shost);
		IEEE80211_ADDR_COPY(frame->i_addr3, eh->ether_dhost);
		break;
	default:
		/*
		 * What format do monitor-mode's frames take?
		 */
		break;
	}
	return (m);
}

void
pgt_input_frames(struct pgt_softc *sc, struct mbuf *m)
{
	struct ether_header eh;
	struct ifnet *ifp;
	struct ieee80211_channel *chan;
	struct ieee80211_node *ni;
	struct ieee80211com *ic;
	struct pgt_data_frame pdf;
	struct pgt_rx_annex *pra;
	struct mbuf *next, *m2;
	unsigned int n;
	uint32_t rstamp;
	uint16_t dataoff;
	int encrypted;
	uint8_t rate, rssi;

	ic = &sc->sc_ic;
	ifp = &ic->ic_if;
	for (next = m; m != NULL; m = next) {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;
		//if (m->m_flags & M_PROTO2) {
		if (m->m_flags) {
			/*
			 * We either ended up losing the previous
			 * frag, or we're trying to receive more than
			 * two of them.
			 */
			ifp->if_ierrors++;
			m_freem(m);
			continue;
		}
		if (m->m_flags & M_PROTO1) {
			if (next == NULL) {
				if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
					printf("%s: more frags set, but not "
					    "found\n",
					    sc->sc_dev.dv_xname);
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			} else {
				//if (!(next->m_flags & M_PROTO2)) {
				if (!(next->m_flags)) {
					if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
						printf("%s: more frags set, "
						   "but next is not one\n",
						    sc->sc_dev.dv_xname);
					ifp->if_ierrors++;
					m_freem(m);
					continue;
				}
				/*
				 * If there are yet more frags after the
				 * second, we're not touching them here.
				 */
				//next->m_flags &= ~(M_PROTO1 | M_PROTO2);
			}
			m->m_flags &= ~M_PROTO1;
			m2 = next;
			next = m2->m_nextpkt;
			m2->m_nextpkt = NULL;
			/* Remove any preceding junk from the latter frag. */
			m_adj(m2, *mtod(m2, uint16_t *));
			m_cat(m, m2);
		}
		dataoff = *mtod(m, uint16_t *);
		m_adj(m, 2);
		if (dataoff < sizeof(pdf)) {
			if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
				printf("%s: missing pgt_data_frame header\n",
				    sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
			m_freem(m);
			continue;
		}
		bcopy(mtod(m, struct pgt_data_frame *), &pdf, sizeof(pdf));
		m_adj(m, dataoff);
		m = m_pullup(m, sizeof(*pra));
		if (m == NULL) {
			if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
				printf("%s: m_pullup failure\n",
				    sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
			continue;
		}
		pra = mtod(m, struct pgt_rx_annex *);
		if (sc->sc_debug & SC_DEBUG_RXANNEX)
			printf("%s: rx annex: ? %04x ? %04x "
			    "len %u clock %u flags %02x ? %02x rate %u ? %02x "
			    "freq %u ? %04x rssi %u pad %02x%02x%02x\n",
			    sc->sc_dev.dv_xname,
			    letoh16(pdf.pdf_unknown),
			    letoh16(pra->pra_unknown0),
			    letoh16(pra->pra_length),
			    letoh32(pra->pra_clock), pra->pra_flags,
			    pra->pra_unknown1, pra->pra_rate,
			    pra->pra_unknown2, letoh32(pra->pra_frequency),
			    pra->pra_unknown3, pra->pra_rssi,
			    pra->pra_pad[0], pra->pra_pad[1], pra->pra_pad[2]);
		if (sc->sc_debug & SC_DEBUG_RXETHER)
			printf("%s: rx ether: "
			    "%02x:%02x:%02x:%02x:%02x:%02x < "
			    "%02x:%02x:%02x:%02x:%02x:%02x 0x%04x\n",
			    sc->sc_dev.dv_xname,
			    pra->pra_ether_dhost[0], pra->pra_ether_dhost[1],
			    pra->pra_ether_dhost[2], pra->pra_ether_dhost[3],
			    pra->pra_ether_dhost[4], pra->pra_ether_dhost[5],
			    pra->pra_ether_shost[0], pra->pra_ether_shost[1],
			    pra->pra_ether_shost[2], pra->pra_ether_shost[3],
			    pra->pra_ether_shost[4], pra->pra_ether_shost[5],
			    ntohs(pra->pra_ether_type));
		/*
		 * This flag is set if e.g. packet could not be decrypted.
		 */
		if (pra->pra_flags & PRA_FLAG_BAD) {
			ifp->if_ierrors++;
			m_freem(m);
			continue;
		}
		/*
		 * The 16-bit word preceding the received frame contains
		 * values that seem to have a very non-random distribution
		 * and possibly follow a periodic distribution.  The only
		 * two values for it that seem to occur for WEP-decrypted
		 * packets (assuming it is indeed a 16-bit word and not
		 * something else) are 0x4008 and 0x4808.
		 *
		 * Those two values can be found in large runs in the
		 * histogram that get zero hits over the course of
		 * hundreds of thousands of samples from an 802.11b
		 * sender source (ping -f).  Further analysis shows
		 * that the 0x000c bits are always 0x0008 when WEP
		 * has been used, and never otherwise.  The bottom
		 * two bits seem to not be set in any known
		 * circumstances.
		 */
		encrypted = sc->sc_80211_ioc_wep != IEEE80211_WEP_OFF &&
		    (letoh16(pdf.pdf_unknown) & 0xc) == 0x8;
		memcpy(eh.ether_dhost, pra->pra_ether_dhost, ETHER_ADDR_LEN);
		memcpy(eh.ether_shost, pra->pra_ether_shost, ETHER_ADDR_LEN);
		eh.ether_type = pra->pra_ether_type;
		/*
		 * After getting what we want, chop off the annex, then
		 * turn into something that looks like it really was
		 * 802.11.
		 */
		rssi = pra->pra_rssi;
		rstamp = letoh32(pra->pra_clock);
		rate = pra->pra_rate;
		n = ieee80211_mhz2ieee(letoh32(pra->pra_frequency), 0);
		if (n <= IEEE80211_CHAN_MAX)
			chan = &ic->ic_channels[n];
		else
			chan = ic->ic_bss->ni_chan;
		/* Send to 802.3 listeners. */
		m_adj(m, sizeof(*pra) - sizeof(eh));
		memcpy(mtod(m, struct ether_header *), &eh, sizeof(eh));
		m_adj(m, sizeof(eh));
		m = pgt_ieee80211_encap(sc, &eh, m, &ni);
		if (m != NULL) {
			if (sc->sc_drvbpf != NULL) {
				struct pgt_ieee80211_radiotap pir;

				bzero(&pir, sizeof(pir));
				pir.pir_header.it_len = htole16(sizeof(pir));
				pir.pir_header.it_present =
				    htole32(PGT_IEEE80211_RADIOTAP_PRESENT);
				if (encrypted)
					pir.pir_flags |=
					    IEEE80211_RADIOTAP_F_WEP;
				pir.pir_rate = rate;
				pir.pir_channel = htole16(chan->ic_freq);
				pir.pir_channel_flags = htole16(chan->ic_flags);
				pir.pir_db_antsignal = rssi;
				pir.pir_db_antnoise = sc->sc_noise;
				//bpf_mtap2(sc->sc_drvbpf, &pir, sizeof(pir), m);
			}
			ni->ni_rssi = rssi;
			ni->ni_rstamp = rstamp;
			ieee80211_input(ifp, m, ni, rssi, rstamp);
			/*
			 * The frame may have caused the node to be marked for
			 * reclamation (e.g. in response to a DEAUTH message)
			 * so use free_node here instead of unref_node.
			 */
			if (ni == ic->ic_bss)
				ieee80211_unref_node(&ni);
			else
				ieee80211_release_node(&sc->sc_ic, ni);
		} else {
			ifp->if_ierrors++;
		}
	}
}

void
pgt_wakeup_intr(struct pgt_softc *sc)
{
	int shouldupdate;
	int i;

	shouldupdate = 0;
	/* Check for any queues being empty before updating. */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	for (i = 0; !shouldupdate && i < PGT_QUEUE_COUNT; i++) {
		if (pgt_queue_is_tx(i))
			shouldupdate = pgt_queue_frags_pending(sc, i);
		else
			shouldupdate = pgt_queue_frags_pending(sc, i) <
			    sc->sc_freeq_count[i];
	}
	if (!TAILQ_EMPTY(&sc->sc_mgmtinprog))
		shouldupdate = 1;
	if (sc->sc_debug & SC_DEBUG_POWER)
		printf("%s: wakeup interrupt (update = %d)\n",
		    sc->sc_dev.dv_xname, shouldupdate);
	sc->sc_flags &= ~SC_POWERSAVE;
	if (shouldupdate) {
		pgt_write_4_flush(sc, PGT_REG_DEV_INT, PGT_DEV_INT_UPDATE);
		DELAY(PGT_WRITEIO_DELAY);
	}
}

void
pgt_sleep_intr(struct pgt_softc *sc)
{
	int allowed;
	int i;

	allowed = 1;
	/* Check for any queues not being empty before allowing. */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	for (i = 0; allowed && i < PGT_QUEUE_COUNT; i++) {
		if (pgt_queue_is_tx(i))
			allowed = pgt_queue_frags_pending(sc, i) == 0;
		else
			allowed = pgt_queue_frags_pending(sc, i) >=
			    sc->sc_freeq_count[i];
	}
	if (!TAILQ_EMPTY(&sc->sc_mgmtinprog))
		allowed = 0;
	if (sc->sc_debug & SC_DEBUG_POWER)
		printf("%s: sleep interrupt (allowed = %d)\n",
		    sc->sc_dev.dv_xname, allowed);
	if (allowed && sc->sc_ic.ic_flags & IEEE80211_F_PMGTON) {
		sc->sc_flags |= SC_POWERSAVE;
		pgt_write_4_flush(sc, PGT_REG_DEV_INT, PGT_DEV_INT_SLEEP);
		DELAY(PGT_WRITEIO_DELAY);
	}
}

void
pgt_empty_traps(struct pgt_softc_kthread *sck)
{
	struct pgt_async_trap *pa;
	struct mbuf *m;

	while (!TAILQ_EMPTY(&sck->sck_traps)) {
		pa = TAILQ_FIRST(&sck->sck_traps);
		TAILQ_REMOVE(&sck->sck_traps, pa, pa_link);
		m = pa->pa_mbuf;
		m_freem(m);
	}
}

void
pgt_per_device_kthread(void *argp)
{
	struct pgt_softc *sc;
	struct pgt_softc_kthread *sck;
	struct pgt_async_trap *pa;
	struct mbuf *m;

	sc = argp;
	sck = &sc->sc_kthread;
	while (!sck->sck_exit) {
		if (!sck->sck_update && !sck->sck_reset &&
		    TAILQ_EMPTY(&sck->sck_traps)) {
			//cv_wait(&sck->sck_needed, &sc->sc_lock);
		}
		if (sck->sck_reset) {
			sck->sck_reset = 0;
			sck->sck_update = 0;
			pgt_empty_traps(sck);
			pgt_enter_critical(sc);
			pgt_disable(sc, SC_NEEDS_RESET);
			pgt_exit_critical(sc);
		} else if (!TAILQ_EMPTY(&sck->sck_traps)) {
			pa = TAILQ_FIRST(&sck->sck_traps);
			TAILQ_REMOVE(&sck->sck_traps, pa, pa_link);
			m = pa->pa_mbuf;
			m_adj(m, sizeof(*pa));
			pgt_update_sw_from_hw(sc, pa, m);
			m_freem(m);
		} else {
			sck->sck_update = 0;
			pgt_update_sw_from_hw(sc, NULL, NULL);
		}
	}
	pgt_empty_traps(sck);
	kthread_exit(0);
}

struct mbuf *
pgt_alloc_async(size_t trapdata)
{
	struct mbuf *m;
	size_t total;

	total = trapdata + sizeof(struct pgt_async_trap);
	if (total >= MINCLSIZE)
		MGETHDR(m, MT_DATA, 0);
	else
		m = m_get(M_DONTWAIT, MT_DATA);
	if (m != NULL)
		m->m_len = total;
	return (m);
}

void
pgt_async_reset(struct pgt_softc *sc)
{
	if (sc->sc_flags & (SC_DYING | SC_NEEDS_RESET))
		return;
	sc->sc_kthread.sck_reset = 1;
	//cv_signal(&sc->sc_kthread.sck_needed);
}

void
pgt_async_trap(struct pgt_softc *sc, uint32_t oid, void *data, size_t len)
{
	struct pgt_async_trap *pa;
	struct mbuf *m;
	char *p;

	if (sc->sc_flags & SC_DYING)
		return;
	m = pgt_alloc_async(sizeof(oid) + len);
	if (m == NULL)
		return;
	pa = mtod(m, struct pgt_async_trap *);
	p = mtod(m, char *) + sizeof(*pa);
	*(uint32_t *)p = oid;
	p += sizeof(uint32_t);
	memcpy(p, data, len);
	pa->pa_mbuf = m;
	TAILQ_INSERT_TAIL(&sc->sc_kthread.sck_traps, pa, pa_link);
	//cv_signal(&sc->sc_kthread.sck_needed);
}

void
pgt_async_update(struct pgt_softc *sc)
{
	if (sc->sc_flags & SC_DYING)
		return;
	sc->sc_kthread.sck_update = 1;
	//cv_signal(&sc->sc_kthread.sck_needed);
}

#ifdef DEVICE_POLLING
void
pgt_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct pgt_softc *sc;
	struct mbuf *datarx = NULL;

	sc = ifp->if_softc;
	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);	/* already have Giant, no LOR */
		cmd = POLL_DEREGISTER;
	}
	if (cmd == POLL_DEREGISTER) {   /* final call, enable interrupts */
		pgt_write_4_flush(sc, PGT_REG_INT_EN, PGT_INT_STAT_SOURCES);	
		DELAY(PGT_WRITEIO_DELAY);
                return;
        }
	pgt_intr_body(sc, &datarx, count);
	if (cmd == POLL_AND_CHECK_STATUS) {
		/* Do more expensive periodic stuff. */
		pgt_async_update(sc);
	}
	/*
	 * Now that we have unlocked the softc, decode and enter the
	 * data frames we've received.
	 */
	if (datarx != NULL)
		pgt_input_frames(sc, datarx);
	//if (!IFQ_DRV_IS_EMPTY(&sc->sc_ic.ic_if.if_snd))
	//	pgt_start(&sc->sc_ic.ic_if);
}
#endif

int
pgt_intr(void *arg)
{
	struct pgt_softc *sc;
	struct ifnet *ifp;
	struct mbuf *datarx = NULL;

	sc = arg;
	ifp = &sc->sc_ic.ic_if;

#ifdef DEVICE_POLLING
	if (ifp->if_flags & IFF_POLLING)
		return;
	if (ifp->if_capenable & IFCAP_POLLING &&
	    !(sc->sc_flags & SC_UNINITIALIZED) &&
	    ether_poll_register(pgt_poll, ifp)) {
		/* Turn off interrupts. */
		pgt_write_4_flush(sc, PGT_REG_INT_EN, 0);	
		DELAY(PGT_WRITEIO_DELAY);
		pgt_poll(ifp, POLL_ONLY, 1);
		return;
	}
#endif
	pgt_intr_body(sc, &datarx, -1);

	/*
	 * Now that we have unlocked the softc, decode and enter the
	 * data frames we've received.
	 */
	if (datarx != NULL)
		pgt_input_frames(sc, datarx);
	//if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
	//	pgt_start(ifp);

	return (0);
}

void
pgt_intr_body(struct pgt_softc *sc, struct mbuf **datarx,
    int max_datarx_count)
{
	u_int32_t reg;

	/*
	 * Here the Linux driver ands in the value of the INT_EN register,
	 * and masks off everything but the documented interrupt bits.  Why?
	 *
	 * Unknown bit 0x4000 is set upon initialization, 0x8000000 some
	 * other times.
	 */
	if (sc->sc_ic.ic_flags & IEEE80211_F_PMGTON &&
	    sc->sc_flags & SC_POWERSAVE) {
		/*
		 * Don't try handling the interrupt in sleep mode.
		 */
		reg = pgt_read_4(sc, PGT_REG_CTRL_STAT);
		if (reg & PGT_CTRL_STAT_SLEEPMODE)
			return;
	}
#ifdef DEVICE_POLLING
	if (sc->sc_ic.ic_if.if_flags & IFF_POLLING)
		reg = PGT_INT_STAT_UPDATE;
	else
#endif
		reg = pgt_read_4(sc, PGT_REG_INT_STAT);
	if (reg != 0) {
#ifdef DEVICE_POLLING
		if (!(sc->sc_ic.ic_if.if_flags & IFF_POLLING))
#endif
			pgt_write_4_flush(sc, PGT_REG_INT_ACK, reg);
		if (reg & PGT_INT_STAT_INIT)
			pgt_init_intr(sc);
		if (reg & PGT_INT_STAT_UPDATE) {
			pgt_update_intr(sc, &datarx, max_datarx_count);
			/*
			 * If we got an update, it's not really asleep.
			 */
			sc->sc_flags &= ~SC_POWERSAVE;
			/*
			 * Pretend I have any idea what the documentation
			 * would say, and just give it a shot sending an
			 * "update" after acknowledging the interrupt
			 * bits and writing out the new control block.
			 */
			pgt_write_4_flush(sc, PGT_REG_DEV_INT,
			    PGT_DEV_INT_UPDATE);
			DELAY(PGT_WRITEIO_DELAY);
		}
		if (reg & PGT_INT_STAT_SLEEP && !(reg & PGT_INT_STAT_WAKEUP))
			pgt_sleep_intr(sc);
		if (reg & PGT_INT_STAT_WAKEUP)
			pgt_wakeup_intr(sc);
	}
	if (sc->sc_flags & SC_INTR_RESET) {
		sc->sc_flags &= ~SC_INTR_RESET;
		pgt_async_reset(sc);
	}
	if (reg & ~PGT_INT_STAT_SOURCES && sc->sc_debug & SC_DEBUG_UNEXPECTED) {
		printf("%s: unknown interrupt bits %#x (stat %#x)\n",
		    sc->sc_dev.dv_xname,
		    reg & ~PGT_INT_STAT_SOURCES,
		    pgt_read_4(sc, PGT_REG_CTRL_STAT));
	}
}

void
pgt_txdone(struct pgt_softc *sc, enum pgt_queue pq)
{
	struct pgt_desc *pd;

	pd = TAILQ_FIRST(&sc->sc_dirtyq[pq]);
	TAILQ_REMOVE(&sc->sc_dirtyq[pq], pd, pd_link);
	sc->sc_dirtyq_count[pq]--;
	TAILQ_INSERT_TAIL(&sc->sc_freeq[pq], pd, pd_link);
	sc->sc_freeq_count[pq]++;
	bus_dmamap_sync(sc->sc_dmat, pd->pd_dmam, 0,
	    pd->pd_dmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	/* Management frames want completion information. */
	if (sc->sc_debug & SC_DEBUG_QUEUES) {
		printf("%s: queue: tx %u <- [%u]\n",
		    sc->sc_dev.dv_xname, pd->pd_fragnum, pq);
		if (sc->sc_debug & SC_DEBUG_MGMT && pgt_queue_is_mgmt(pq)) {
			struct pgt_mgmt_frame *pmf;

			pmf = (struct pgt_mgmt_frame *)pd->pd_mem;
			printf("%s: queue: txmgmt %p <- "
			    "(ver %u, op %u, flags 0x%x)\n",
			    sc->sc_dev.dv_xname,
			    pd, pmf->pmf_version, pmf->pmf_operation,
			    pmf->pmf_flags);
		}
	}
	pgt_unload_tx_desc_frag(sc, pd);
	/*
	if (pgt_queue_is_data(pq))
		pgt_try_exit_data_critical(sc);
	*/
}

void
pgt_rxdone(struct pgt_softc *sc, enum pgt_queue pq)
{
	struct pgt_desc *pd;

	pd = TAILQ_FIRST(&sc->sc_freeq[pq]);
	TAILQ_REMOVE(&sc->sc_freeq[pq], pd, pd_link);
	sc->sc_freeq_count[pq]--;
	TAILQ_INSERT_TAIL(&sc->sc_dirtyq[pq], pd, pd_link);
	sc->sc_dirtyq_count[pq]++;
	bus_dmamap_sync(sc->sc_dmat, pd->pd_dmam, 0,
	    pd->pd_dmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	if (sc->sc_debug & SC_DEBUG_QUEUES)
		printf("%s: queue: rx %u <- [%u]\n",
		    sc->sc_dev.dv_xname, pd->pd_fragnum, pq);
	if (sc->sc_debug & SC_DEBUG_UNEXPECTED &&
	    pd->pd_fragp->pf_flags & ~htole16(PF_FLAG_MF))
		printf("%s: unknown flags on rx [%u]: 0x%x\n",
		    sc->sc_dev.dv_xname, pq, letoh16(pd->pd_fragp->pf_flags));
}

/*
 * Traps are generally used for the firmware to report changes in state
 * back to the host.  Mostly this processes changes in link state, but
 * it needs to also be used to initiate WPA and other authentication
 * schemes in terms of client (station) or server (access point).
 */
void
pgt_trap_received(struct pgt_softc *sc, uint32_t oid, void *trapdata,
    size_t size)
{
	pgt_async_trap(sc, oid, trapdata, size);
}

/*
 * Process a completed management response (all requests should be
 * responded to, quickly) or an event (trap).
 */
void
pgt_mgmtrx_completion(struct pgt_softc *sc, struct pgt_mgmt_desc *pmd)
{
	struct pgt_desc *pd;
	struct pgt_mgmt_frame *pmf;
	uint32_t oid, size;

	pd = TAILQ_FIRST(&sc->sc_dirtyq[PGT_QUEUE_MGMT_RX]);
	TAILQ_REMOVE(&sc->sc_dirtyq[PGT_QUEUE_MGMT_RX], pd, pd_link);
	sc->sc_dirtyq_count[PGT_QUEUE_MGMT_RX]--;
	TAILQ_INSERT_TAIL(&sc->sc_freeq[PGT_QUEUE_MGMT_RX],
	    pd, pd_link);
	sc->sc_freeq_count[PGT_QUEUE_MGMT_RX]++;
	if (letoh16(pd->pd_fragp->pf_size) < sizeof(*pmf)) {
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s: mgmt desc too small: %u\n",
			    sc->sc_dev.dv_xname,
			    letoh16(pd->pd_fragp->pf_size));
		goto out_nopmd;
	}
	pmf = (struct pgt_mgmt_frame *)pd->pd_mem;
	if (pmf->pmf_version != PMF_VER) {
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s: unknown mgmt version %u\n",
			    sc->sc_dev.dv_xname, pmf->pmf_version);
		goto out_nopmd;
	}
	if (pmf->pmf_device != PMF_DEV) {
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s: unknown mgmt dev %u\n",
			    sc->sc_dev.dv_xname, pmf->pmf_device);
		goto out;
	}
	if (pmf->pmf_flags & ~PMF_FLAG_VALID) {
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s: unknown mgmt flags %u\n",
			    sc->sc_dev.dv_xname,
			    pmf->pmf_flags & ~PMF_FLAG_VALID);
		goto out;
	}
	if (pmf->pmf_flags & PMF_FLAG_LE) {
		oid = letoh32(pmf->pmf_oid);
		size = letoh32(pmf->pmf_size);
	} else {
		oid = betoh32(pmf->pmf_oid);
		size = betoh32(pmf->pmf_size);
	}
	if (pmf->pmf_operation == PMF_OP_TRAP) {
		pmd = NULL; /* ignored */
		pgt_trap_received(sc, oid, (char *)pmf + sizeof(*pmf),
		    min(size, PGT_FRAG_SIZE - sizeof(*pmf)));
		goto out_nopmd;
	}
	if (pmd == NULL) {
		if (sc->sc_debug & (SC_DEBUG_UNEXPECTED | SC_DEBUG_MGMT))
			printf("%s: spurious mgmt received "
			    "(op %u, oid 0x%x, len %u\n",
			    sc->sc_dev.dv_xname, pmf->pmf_operation, oid, size);
		goto out_nopmd;
	}
	switch (pmf->pmf_operation) {
	case PMF_OP_RESPONSE:
		pmd->pmd_error = 0;
		break;
	case PMF_OP_ERROR:
		pmd->pmd_error = EPERM;
		goto out;
	default:
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s: unknown mgmt op %u\n",
			    sc->sc_dev.dv_xname, pmf->pmf_operation);
		pmd->pmd_error = EIO;
		goto out;
	}
	if (oid != pmd->pmd_oid) {
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s: mgmt oid changed from 0x%x "
			    "-> 0x%x\n",
			    sc->sc_dev.dv_xname, pmd->pmd_oid, oid);
		pmd->pmd_oid = oid;
	}
	if (pmd->pmd_recvbuf != NULL) {
		if (size > PGT_FRAG_SIZE) {
			if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
				printf("%s: mgmt oid 0x%x "
				    "has bad size %u\n",
				    sc->sc_dev.dv_xname, oid, size);
			pmd->pmd_error = EIO;
			goto out;
		}
		if (size > pmd->pmd_len)
			pmd->pmd_error = ENOMEM;
		else
			memcpy(pmd->pmd_recvbuf, (char *)pmf + sizeof(*pmf),
			    size);
		pmd->pmd_len = size;
	}
out:
	TAILQ_REMOVE(&sc->sc_mgmtinprog, pmd, pmd_link);
#ifdef DEVICE_POLLING
	if (!(sc->sc_ic.ic_if.if_flags & IFF_POLLING))
#endif
		wakeup_one(pmd);
	if (sc->sc_debug & SC_DEBUG_MGMT)
		printf("%s: queue: mgmt %p <- (op %u, "
		    "oid 0x%x, len %u)\n",
		    sc->sc_dev.dv_xname, pmd, pmf->pmf_operation,
		    pmd->pmd_oid, pmd->pmd_len);
out_nopmd:
	pgt_reinit_rx_desc_frag(sc, pd);
}

/*
 * Queue packets for reception and defragmentation.  I don't know now
 * whether the rx queue being full enough to start, but not finish,
 * queueing a fragmented packet, can happen.
 */
int
pgt_datarx_completion(struct pgt_softc *sc, enum pgt_queue pq,
    struct mbuf ***last_nextpkt, int prevwasmf)
{
	struct ifnet *ifp;
	struct pgt_desc *pd;
	struct mbuf *m;
	size_t datalen;
	uint16_t dataoff;
	int morefrags;

	ifp = &sc->sc_ic.ic_if;
	pd = TAILQ_FIRST(&sc->sc_dirtyq[pq]);
	TAILQ_REMOVE(&sc->sc_dirtyq[pq], pd, pd_link);
	sc->sc_dirtyq_count[pq]--;
	datalen = letoh16(pd->pd_fragp->pf_size);
	dataoff = letoh32(pd->pd_fragp->pf_addr) - pd->pd_dmaaddr;
	morefrags = pd->pd_fragp->pf_flags & htole16(PF_FLAG_MF);
	if (sc->sc_debug & SC_DEBUG_RXFRAG)
		printf("%s: rx frag: len %u memoff %u\n",
		    sc->sc_dev.dv_xname, datalen, dataoff);
	/* Add the (two+?) bytes for the header. */
	datalen += dataoff;
	if (datalen > PGT_FRAG_SIZE) {
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			printf("%s data rx too big: %u\n",
			    sc->sc_dev.dv_xname, datalen);
		ifp->if_ierrors++;
		goto out;
	}
	/* Add a uint16_t at the beginning containing the actual data offset. */
	if (prevwasmf) {
		if (datalen + 2 >= MINCLSIZE)
			MGETHDR(m, MT_DATA, 0);
		else
			m = m_get(M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			ifp->if_ierrors++;
			goto out;
		}
		//m->m_flags |= M_PROTO2;
		bcopy(pd->pd_mem, mtod(m, char *) + 2, datalen);
		m->m_len = datalen;
	} else {
		m = m_devget(pd->pd_mem, datalen,
		    sizeof(struct ieee80211_frame) + 2, ifp, NULL);
		if (m != NULL)
			M_PREPEND(m, 2, M_DONTWAIT);
		if (m == NULL) {
			ifp->if_ierrors++;
			goto out;
		}
	}
	*mtod(m, uint16_t *) = dataoff;
	if (morefrags)
		m->m_flags |= M_PROTO1;
	else
		/*
		 * Count non-fragmented packets and the last fragment
		 * in fragmented packets.
		 */
		ifp->if_ipackets++;
	**last_nextpkt = m;
	*last_nextpkt = &m->m_nextpkt;
out:
	TAILQ_INSERT_TAIL(&sc->sc_freeq[pq], pd, pd_link);
	sc->sc_freeq_count[pq]++;
	pgt_reinit_rx_desc_frag(sc, pd);
	return (morefrags);
}

int
pgt_oid_get(struct pgt_softc *sc, enum pgt_oid oid,
    void *arg, size_t arglen)
{
	struct pgt_mgmt_desc pmd;
	int error;

	bzero(&pmd, sizeof(pmd));
	pmd.pmd_recvbuf = arg;
	pmd.pmd_len = arglen;
	pmd.pmd_oid = oid;
	error = pgt_mgmt_request(sc, &pmd);
	if (error == 0)
		error = pmd.pmd_error;
	if (error != 0 && error != EPERM && sc->sc_debug & SC_DEBUG_UNEXPECTED)
		printf("%s: failure getting oid 0x%x: %d\n",
		    sc->sc_dev.dv_xname,
		    oid, error);
	return (error);
}

int
pgt_oid_retrieve(struct pgt_softc *sc, enum pgt_oid oid,
    void *arg, size_t arglen)
{
	struct pgt_mgmt_desc pmd;
	int error;

	bzero(&pmd, sizeof(pmd));
	pmd.pmd_sendbuf = arg;
	pmd.pmd_recvbuf = arg;
	pmd.pmd_len = arglen;
	pmd.pmd_oid = oid;
	error = pgt_mgmt_request(sc, &pmd);
	if (error == 0)
		error = pmd.pmd_error;
	if (error != 0 && error != EPERM && sc->sc_debug & SC_DEBUG_UNEXPECTED)
		printf("%s: failure retrieving oid 0x%x: %d\n",
		    sc->sc_dev.dv_xname, oid, error);
	return (error);
}

int
pgt_oid_set(struct pgt_softc *sc, enum pgt_oid oid,
    const void *arg, size_t arglen)
{
	struct pgt_mgmt_desc pmd;
	int error;

	bzero(&pmd, sizeof(pmd));
	pmd.pmd_sendbuf = arg;
	pmd.pmd_len = arglen;
	pmd.pmd_oid = oid;
	error = pgt_mgmt_request(sc, &pmd);
	if (error == 0)
		error = pmd.pmd_error;
	if (error != 0 && error != EPERM && sc->sc_debug & SC_DEBUG_UNEXPECTED)
		printf("%s: failure setting oid 0x%x: %d\n",
		    sc->sc_dev.dv_xname, oid, error);
	return (error);
}

void
pgt_state_dump(struct pgt_softc *sc)
{
	printf("%s: state dump: control 0x%08x "
	    "interrupt 0x%08x\n", sc->sc_dev.dv_xname,
	    pgt_read_4(sc, PGT_REG_CTRL_STAT),
	    pgt_read_4(sc, PGT_REG_INT_STAT));

	printf("%s: state dump: driver curfrag[]\n",
	    sc->sc_dev.dv_xname);

	printf("%s: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    letoh32(sc->sc_cb->pcb_driver_curfrag[0]),
	    letoh32(sc->sc_cb->pcb_driver_curfrag[1]),
	    letoh32(sc->sc_cb->pcb_driver_curfrag[2]),
	    letoh32(sc->sc_cb->pcb_driver_curfrag[3]),
	    letoh32(sc->sc_cb->pcb_driver_curfrag[4]),
	    letoh32(sc->sc_cb->pcb_driver_curfrag[5]));

	printf("%s: state dump: device curfrag[]\n",
	    sc->sc_dev.dv_xname);

	printf("%s: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    letoh32(sc->sc_cb->pcb_device_curfrag[0]),
	    letoh32(sc->sc_cb->pcb_device_curfrag[1]),
	    letoh32(sc->sc_cb->pcb_device_curfrag[2]),
	    letoh32(sc->sc_cb->pcb_device_curfrag[3]),
	    letoh32(sc->sc_cb->pcb_device_curfrag[4]),
	    letoh32(sc->sc_cb->pcb_device_curfrag[5]));
}

int
pgt_mgmt_request(struct pgt_softc *sc, struct pgt_mgmt_desc *pmd)
{
	struct pgt_desc *pd;
	struct pgt_mgmt_frame *pmf;
	int error, i;

	if (sc->sc_flags & (SC_DYING | SC_NEEDS_RESET))
		return (EIO);
	if (pmd->pmd_len > PGT_FRAG_SIZE - sizeof(*pmf))
		return (ENOMEM);
	pd = TAILQ_FIRST(&sc->sc_freeq[PGT_QUEUE_MGMT_TX]);
	if (pd == NULL)
		return (ENOMEM);
	error = pgt_load_tx_desc_frag(sc, PGT_QUEUE_MGMT_TX, pd);
	if (error)
		return (error);
	pmf = (struct pgt_mgmt_frame *)pd->pd_mem;
	pmf->pmf_version = PMF_VER;
	/* "get" and "retrieve" operations look the same */
	if (pmd->pmd_recvbuf != NULL)
		pmf->pmf_operation = PMF_OP_GET;
	else
		pmf->pmf_operation = PMF_OP_SET;
	pmf->pmf_oid = htobe32(pmd->pmd_oid);
	pmf->pmf_device = PMF_DEV;
	pmf->pmf_flags = 0;
	pmf->pmf_size = htobe32(pmd->pmd_len);
	/* "set" and "retrieve" operations both send data */
	if (pmd->pmd_sendbuf != NULL)
		memcpy((char *)pmf + sizeof(*pmf), pmd->pmd_sendbuf,
		    pmd->pmd_len);
	else
		bzero((char *)pmf + sizeof(*pmf), pmd->pmd_len);
	pmd->pmd_error = EINPROGRESS;
	TAILQ_INSERT_TAIL(&sc->sc_mgmtinprog, pmd, pmd_link);
	if (sc->sc_debug & SC_DEBUG_MGMT)
		printf("%s: queue: mgmt %p -> (op %u, "
		    "oid 0x%x, len %u)\n", sc->sc_dev.dv_xname,
		    pmd, pmf->pmf_operation,
		    pmd->pmd_oid, pmd->pmd_len);
	pgt_desc_transmit(sc, PGT_QUEUE_MGMT_TX, pd,
	    sizeof(*pmf) + pmd->pmd_len, 0);
	sc->sc_refcnt++;
#ifdef DEVICE_POLLING
	/*
	 * If we're polling, try 1/10th second initially at the smallest
	 * interval we can sleep for.
	 */
	for (i = 0; sc->sc_ic.ic_if.if_flags & IFF_POLLING && i < 100; i++) {
		pgt_update_intr(sc, NULL, 0);
		if (pmd->pmd_error != EINPROGRESS)
			goto usedpoll;
		/*
		if (msleep(pmd, &sc->sc_lock, PZERO, "pffmgp", 1) !=
		    EWOULDBLOCK)
			break;
		*/
	}
#endif
	/*
	 * Try for one second, triggering 10 times.
	 *
	 * Do our best to work around seemingly buggy CardBus controllers
	 * on Soekris 4521 that fail to get interrupts with alarming
	 * regularity: run as if an interrupt occurred and service every
	 * queue except for mbuf reception.
	 */
	i = 0;
	do {
		/*
		if (msleep(pmd, &sc->sc_lock, PZERO, "pffmgm", hz / 10) !=
		    EWOULDBLOCK)
			break;
		*/
		if (pmd->pmd_error != EINPROGRESS)
			break;
		if (sc->sc_flags & (SC_DYING | SC_NEEDS_RESET)) {
			pmd->pmd_error = EIO;
			TAILQ_REMOVE(&sc->sc_mgmtinprog, pmd,
			    pmd_link);	
			break;
		}
		if (i != 9)
			pgt_maybe_trigger(sc, PGT_QUEUE_MGMT_RX);
#ifdef PGT_BUGGY_INTERRUPT_RECOVERY
		pgt_update_intr(sc, NULL, 0);
#endif
	} while (i++ < 10);
#ifdef DEVICE_POLLING
usedpoll:
#endif
	if (pmd->pmd_error == EINPROGRESS) {
		printf("%s: timeout waiting for management "
		    "packet response to 0x%x\n", sc->sc_dev.dv_xname, pmd->pmd_oid);
		TAILQ_REMOVE(&sc->sc_mgmtinprog, pmd,
		    pmd_link);	
		if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
			pgt_state_dump(sc);
		pgt_async_reset(sc);
		error = ETIMEDOUT;
	} else {
		error = 0;
	}
	sc->sc_refcnt--;
	return (error);
}

void
pgt_desc_transmit(struct pgt_softc *sc, enum pgt_queue pq, struct pgt_desc *pd,
    uint16_t len, int morecoming)
{
	TAILQ_REMOVE(&sc->sc_freeq[pq], pd, pd_link);
	sc->sc_freeq_count[pq]--;
	TAILQ_INSERT_TAIL(&sc->sc_dirtyq[pq], pd, pd_link);
	sc->sc_dirtyq_count[pq]++;
	if (sc->sc_debug & SC_DEBUG_QUEUES)
		printf("%s: queue: tx %u -> [%u]\n", sc->sc_dev.dv_xname,
		    pd->pd_fragnum, pq);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_PREWRITE);
	if (morecoming)
		pd->pd_fragp->pf_flags |= htole16(PF_FLAG_MF);
	pd->pd_fragp->pf_size = htole16(len);
	bus_dmamap_sync(sc->sc_dmat, pd->pd_dmam, 0,
	    pd->pd_dmam->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	sc->sc_cb->pcb_driver_curfrag[pq] =
	    htole32(letoh32(sc->sc_cb->pcb_driver_curfrag[pq]) + 1);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_PREREAD);
	if (!morecoming)
		pgt_maybe_trigger(sc, pq);
}

void
pgt_maybe_trigger(struct pgt_softc *sc, enum pgt_queue pq)
{
	unsigned int tries = 1000000 / PGT_WRITEIO_DELAY; /* one second */
	uint32_t reg;

	if (sc->sc_debug & SC_DEBUG_TRIGGER)
		printf("%s: triggered by queue [%u]\n",
		    sc->sc_dev.dv_xname, pq);
	pgt_debug_events(sc, "trig");
	if (sc->sc_flags & SC_POWERSAVE) {
		/* Magic values ahoy? */
		if (pgt_read_4(sc, PGT_REG_INT_STAT) == 0xabadface) {
			do {
				reg = pgt_read_4(sc, PGT_REG_CTRL_STAT);
				if (!(reg & PGT_CTRL_STAT_SLEEPMODE))
					DELAY(PGT_WRITEIO_DELAY);
			} while (tries-- != 0);
			if (!(reg & PGT_CTRL_STAT_SLEEPMODE)) {
				if (sc->sc_debug & SC_DEBUG_UNEXPECTED)
					printf("%s: timeout triggering from "
					    "sleep mode\n",
					    sc->sc_dev.dv_xname);
				pgt_async_reset(sc);
				return;
			}
		}
		pgt_write_4_flush(sc, PGT_REG_DEV_INT,
		    PGT_DEV_INT_WAKEUP);
		DELAY(PGT_WRITEIO_DELAY);
		/* read the status back in */
		(void)pgt_read_4(sc, PGT_REG_CTRL_STAT);
		DELAY(PGT_WRITEIO_DELAY);
	} else {
		pgt_write_4_flush(sc, PGT_REG_DEV_INT, PGT_DEV_INT_UPDATE);
		DELAY(PGT_WRITEIO_DELAY);
	}
}

struct ieee80211_node *
pgt_ieee80211_node_alloc(struct ieee80211com *ic)
{
	struct pgt_ieee80211_node *pin;

	pin = malloc(sizeof(*pin), M_DEVBUF, M_NOWAIT);
	bzero(pin, sizeof *pin);
	if (pin != NULL)
		pin->pin_dot1x_auth = PIN_DOT1X_UNAUTHORIZED;
	return (struct ieee80211_node *)pin;
}

void
pgt_ieee80211_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni,
	    int reallynew)
{
	ieee80211_ref_node(ni);
}

void
pgt_ieee80211_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct pgt_ieee80211_node *pin;

	pin = (struct pgt_ieee80211_node *)ni;
	free(pin, M_DEVBUF);
}

void
pgt_ieee80211_node_copy(struct ieee80211com *ic, struct ieee80211_node *dst,
	    const struct ieee80211_node *src)
{
	const struct pgt_ieee80211_node *psrc;
	struct pgt_ieee80211_node *pdst;

	psrc = (const struct pgt_ieee80211_node *)src;
	pdst = (struct pgt_ieee80211_node *)dst;
	bcopy(psrc, pdst, sizeof(*psrc));
}

int
pgt_ieee80211_send_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni,
	    int type, int arg)
{
	return (EOPNOTSUPP);
}


int
pgt_net_attach(struct pgt_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct arpcom *ac = &ic->ic_ac;
	struct ifnet *ifp = &ac->ac_if;
	struct ieee80211_rateset *rs;
	uint8_t rates[IEEE80211_RATE_MAXSIZE];
	struct pgt_obj_buffer psbuffer;
	struct pgt_obj_frequencies *freqs;
	uint32_t phymode, country;
	unsigned int chan, i, j, firstchan = -1;
	int error;

	psbuffer.pob_size = htole32(PGT_FRAG_SIZE * PGT_PSM_BUFFER_FRAME_COUNT);
	psbuffer.pob_addr = htole32((uint32_t)sc->sc_psmdmabusaddr);
	error = pgt_oid_set(sc, PGT_OID_PSM_BUFFER, &psbuffer,
	    sizeof(psbuffer));
	if (error == 0)
		error = pgt_oid_get(sc, PGT_OID_PHY, &phymode, sizeof(phymode));
	if (error == 0)
		error = pgt_oid_get(sc, PGT_OID_MAC_ADDRESS, ac->ac_enaddr,
		    sizeof(ac->ac_enaddr));
	if (error == 0)
		error = pgt_oid_get(sc, PGT_OID_COUNTRY, &country,
		    sizeof(country));
	if (error)
		return (error);

	//if_initname(ifp, device_get_name(sc->sc_dev),
	//    device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST | IFF_RUNNING;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif
	//ifp->if_capenable = IFCAP_VLAN_MTU;
	ifp->if_start = pgt_start;
	ifp->if_ioctl = pgt_ioctl;
	ifp->if_watchdog = pgt_periodic;
	ifp->if_init = pgt_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, PGT_QUEUE_FULL_THRESHOLD);
	//ifp->if_snd.ifq_drv_maxlen = PGT_QUEUE_FULL_THRESHOLD;
	IFQ_SET_READY(&ifp->if_snd);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, ac->ac_enaddr);
	j = sizeof(*freqs) + (IEEE80211_CHAN_MAX + 1) * sizeof(uint16_t);
	freqs = malloc(j, M_DEVBUF, M_WAITOK);
	error = pgt_oid_get(sc, PGT_OID_SUPPORTED_FREQUENCIES, freqs, j);
	if (error) {
		free(freqs, M_DEVBUF);
		return (error);
	}
	/*
	 * Prism hardware likes to report supported frequencies that are
	 * not actually available for the country of origin.
	 */
	j = letoh16(freqs->pof_count);
	for (i = 0; i < j; i++) {
		chan = ieee80211_mhz2ieee(letoh16(freqs->pof_freqlist_mhz[i]),
		    0);
		if (chan > IEEE80211_CHAN_MAX) {
			printf("%s: reported bogus channel (%uMHz)\n",
			    sc->sc_dev.dv_xname, chan);
			free(freqs, M_DEVBUF);
			return (EIO);
		}
		if (letoh16(freqs->pof_freqlist_mhz[i]) < 5000) {
			if (!(phymode & htole32(PGT_OID_PHY_2400MHZ)))
				continue;
			if (country == letoh32(PGT_COUNTRY_USA)) {
				if (chan >= 12 && chan <= 14)
					continue;
			}
			if (chan <= 14)
				ic->ic_channels[chan].ic_flags |=
				    IEEE80211_CHAN_B;
			ic->ic_channels[chan].ic_flags |= IEEE80211_CHAN_PUREG;
		} else {
			if (!(phymode & htole32(PGT_OID_PHY_5000MHZ)))
				continue;
			ic->ic_channels[chan].ic_flags |= IEEE80211_CHAN_A;
		}
		ic->ic_channels[chan].ic_freq =
		    letoh16(freqs->pof_freqlist_mhz[i]);
		if (firstchan == -1)
			firstchan = chan;
	}
	free(freqs, M_DEVBUF);
	if (firstchan == -1) {
		printf("%s: no channels found\n", sc->sc_dev.dv_xname);
		return (EIO);
	}
	bzero(rates, sizeof(rates));
	error = pgt_oid_get(sc, PGT_OID_SUPPORTED_RATES, rates, sizeof(rates));
	if (error)
		return (error);
	for (i = 0; i < sizeof(rates) && rates[i] != 0; i++) {
		switch (rates[i]) {
		case 2:
		case 4:
		case 11:
		case 22:
		case 44:	/* maybe */
			if (phymode & htole32(PGT_OID_PHY_2400MHZ)) {
				rs = &ic->ic_sup_rates[IEEE80211_MODE_11B];
				rs->rs_rates[rs->rs_nrates++] = rates[i];
			}
		default:
			if (phymode & htole32(PGT_OID_PHY_2400MHZ)) {
				rs = &ic->ic_sup_rates[IEEE80211_MODE_11G];
				rs->rs_rates[rs->rs_nrates++] = rates[i];
			}
			if (phymode & htole32(PGT_OID_PHY_5000MHZ)) {
				rs = &ic->ic_sup_rates[IEEE80211_MODE_11A];
				rs->rs_rates[rs->rs_nrates++] = rates[i];
			}
			rs = &ic->ic_sup_rates[IEEE80211_MODE_AUTO];
			rs->rs_rates[rs->rs_nrates++] = rates[i];
		}
	}
	ic->ic_caps = IEEE80211_C_WEP | IEEE80211_C_IBSS | IEEE80211_C_PMGT |
	    IEEE80211_C_HOSTAP | IEEE80211_C_TXPMGT | IEEE80211_C_SHSLOT |
	    IEEE80211_C_SHPREAMBLE | IEEE80211_C_MONITOR;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* XXX not really used */
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;
	ic->ic_protmode = IEEE80211_PROT_NONE;
	ieee80211_ifattach(ifp);
	/* Set up post-attach/pre-lateattach vector functions. */
	ic->ic_newstate = pgt_new_state;
	ic->ic_node_alloc = pgt_ieee80211_node_alloc;
	ic->ic_newassoc = pgt_ieee80211_newassoc;
	ic->ic_node_free = pgt_ieee80211_node_free;
	ic->ic_node_copy = pgt_ieee80211_node_copy;
	ic->ic_send_mgmt = pgt_ieee80211_send_mgmt;
	/* let net80211 handle switching around the media + resetting */
	ieee80211_media_init(ifp, pgt_media_change, pgt_media_status);
	//bpfattach2(ifp, DLT_IEEE802_11_RADIO, sizeof(struct ieee80211_frame) +
	//    sizeof(struct pgt_ieee80211_radiotap), &sc->sc_drvbpf);
	/* default to the first channel we know of */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	sc->sc_if_flags = ifp->if_flags;
	return (0);
}

void
pgt_net_detach(struct pgt_softc *sc)
{
	ieee80211_new_state(&sc->sc_ic, IEEE80211_S_INIT, -1);
	bpfdetach(&sc->sc_ic.ic_if);
	ieee80211_ifdetach(&sc->sc_ic.ic_if);
}

void
pgt_start(struct ifnet *ifp)
{
	struct pgt_softc *sc;
	struct ieee80211com *ic;

	sc = ifp->if_softc;
	ic = &sc->sc_ic;
	if (sc->sc_flags & (SC_DYING | SC_NEEDS_RESET) ||
	    !(ifp->if_flags & IFF_RUNNING) ||
	    ic->ic_state != IEEE80211_S_RUN) {
		return;
	}
	pgt_start_body(sc, ic, ifp);
}

/*
 * Start data frames.  Critical sections surround the boundary of
 * management frame transmission / transmission acknowledgement / response
 * and data frame transmission / transmission acknowledgement.
 */
void
pgt_start_body(struct pgt_softc *sc, struct ieee80211com *ic, struct ifnet *ifp)
{
	struct pgt_desc *pd;
	struct mbuf *m;
	int error;

	if (!pgt_try_enter_data_critical(sc))
		return;
	/*
	 * Management packets should probably be MLME frames
	 * (i.e. hostap "managed" mode); we don't touch the
	 * net80211 management queue.
	 */
	for (; sc->sc_dirtyq_count[PGT_QUEUE_DATA_LOW_TX] <
	    //PGT_QUEUE_FULL_THRESHOLD && !IFQ_DRV_IS_EMPTY(&ifp->if_snd);) {
	    PGT_QUEUE_FULL_THRESHOLD;) {
		pd = TAILQ_FIRST(&sc->sc_freeq[PGT_QUEUE_DATA_LOW_TX]);
		//IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		if (m->m_pkthdr.len <= PGT_FRAG_SIZE) {
			error = pgt_load_tx_desc_frag(sc,
			    PGT_QUEUE_DATA_LOW_TX, pd);
			if (error) {
				//IFQ_DRV_PREPEND(&ifp->if_snd, m);
				break;
			}
			m_copydata(m, 0, m->m_pkthdr.len, pd->pd_mem);
			pgt_desc_transmit(sc, PGT_QUEUE_DATA_LOW_TX,
			    pd, m->m_pkthdr.len, 0);
			//BPF_MTAP(ifp, m);
			ifp->if_opackets++;
			sc->sc_critical++;
		} else if (m->m_pkthdr.len <= PGT_FRAG_SIZE * 2) {
			struct pgt_desc *pd2;

			/*
			 * Transmit a fragmented frame if there is
			 * not enough room in one fragment; limit
			 * to two fragments (802.11 itself couldn't
			 * even support a full two.)
			 */
			if (sc->sc_dirtyq_count[PGT_QUEUE_DATA_LOW_TX] + 2 >
			    PGT_QUEUE_FULL_THRESHOLD) {
				//IFQ_DRV_PREPEND(&ifp->if_snd, m);
				break;
			}
			pd2 = TAILQ_NEXT(pd, pd_link);
			error = pgt_load_tx_desc_frag(sc,
			    PGT_QUEUE_DATA_LOW_TX, pd);
			if (error == 0) {
				error = pgt_load_tx_desc_frag(sc,
				    PGT_QUEUE_DATA_LOW_TX, pd2);
				if (error) {
					pgt_unload_tx_desc_frag(sc, pd);
					TAILQ_INSERT_HEAD(&sc->sc_freeq[
					    PGT_QUEUE_DATA_LOW_TX], pd,
					    pd_link);
				}
			}
			if (error) {
				//IFQ_DRV_PREPEND(&ifp->if_snd, m);
				break;
			}
			m_copydata(m, 0, PGT_FRAG_SIZE, pd->pd_mem);
			pgt_desc_transmit(sc, PGT_QUEUE_DATA_LOW_TX,
			    pd, PGT_FRAG_SIZE, 1);
			m_copydata(m, PGT_FRAG_SIZE,
			    m->m_pkthdr.len - PGT_FRAG_SIZE, pd2->pd_mem);
			pgt_desc_transmit(sc, PGT_QUEUE_DATA_LOW_TX,
			    pd2, m->m_pkthdr.len - PGT_FRAG_SIZE, 0);
			//BPF_MTAP(ifp, m);
			ifp->if_opackets++;
			sc->sc_critical += 2;
		} else {
			ifp->if_oerrors++;
			m_freem(m);
			m = NULL;
		}
		if (m != NULL) {
			struct ieee80211_node *ni;

			ifp->if_timer = 1;
			//getbinuptime(&sc->sc_data_tx_started);
			ni = ieee80211_find_txnode(&sc->sc_ic,
			    mtod(m, struct ether_header *)->ether_dhost);
			if (ni != NULL) {
				ni->ni_inact = 0;
				if (ni != ic->ic_bss)
					ieee80211_release_node(&sc->sc_ic, ni);
			}
			if (sc->sc_drvbpf != NULL) {
				struct pgt_ieee80211_radiotap pir;
				struct ether_header eh;

				/*
				 * Fill out what we can when faking
				 * up a radiotapified outgoing frame.
				 */
				bzero(&pir, sizeof(pir));
				pir.pir_header.it_len = htole16(sizeof(pir));
				pir.pir_header.it_present =
				    htole32(PGT_IEEE80211_RADIOTAP_PRESENT);
				if (sc->sc_80211_ioc_wep != IEEE80211_WEP_OFF)
					pir.pir_flags |=
					    IEEE80211_RADIOTAP_F_WEP;
				pir.pir_channel =
				    htole16(ic->ic_bss->ni_chan->ic_freq);
				pir.pir_channel_flags =
				    htole16(ic->ic_bss->ni_chan->ic_flags);
				pir.pir_db_antnoise = sc->sc_noise;
				memcpy(mtod(m, struct ether_header *), &eh,
				    sizeof(eh));
				m_adj(m, sizeof(eh));
				m = pgt_ieee80211_encap(sc, &eh, m, NULL);
				if (m != NULL) {
					//bpf_mtap2(sc->sc_drvbpf, &pir,
					//    sizeof(pir), m);
					m_freem(m);
				}
			} else {
				m_freem(m);
			}
		}
	}
	//pgt_try_exit_data_critical(sc);
}

int
pgt_ioctl(struct ifnet *ifp, u_long cmd, caddr_t req)
{
	struct pgt_softc *sc = ifp->if_softc;
	struct ifreq *ifr;
	struct wi_req *wreq;
	//struct ifprismoidreq *preq;
	//struct ieee80211req *ireq;
	int error, oldflags;

	ifr = (struct ifreq *)req;
	switch (cmd) {
#if 0
	case SIOCGPRISMOID:
	case SIOCSPRISMOID:
		//error = suser(curthread);
		if (error)
			return (error);
		preq = (struct ifprismoidreq *)req;
		if (preq->ifr_oidlen > sizeof(preq->ifr_oiddata))
			return (ENOMEM);
		pgt_enter_critical(sc);
		if (cmd == SIOCGPRISMOID)
			error = pgt_oid_retrieve(sc, preq->ifr_oid,
			    preq->ifr_oiddata, preq->ifr_oidlen);
		else
			error = pgt_oid_set(sc, preq->ifr_oid,
			    preq->ifr_oiddata, preq->ifr_oidlen);
		pgt_exit_critical(sc);
		break;
#endif
	case SIOCGWAVELAN:
	case SIOCSWAVELAN:
		wreq = malloc(sizeof(*wreq), M_DEVBUF, M_WAITOK);
		error = copyin(ifr->ifr_data, wreq, sizeof(*wreq));
		if (error == 0) {
			if (cmd == SIOCGWAVELAN) {
				error = pgt_wavelan_get(sc, wreq);
				if (error == 0)
					error = copyout(wreq, ifr->ifr_data,
					    sizeof(*wreq));
			} else {
				error = pgt_wavelan_set(sc, wreq);
			}
		}
		free(wreq, M_DEVBUF);
		if (error == EOPNOTSUPP)
			goto notours;
		break;
	case SIOCSIFFLAGS:
		error = 0;
		oldflags = sc->sc_if_flags;
		sc->sc_if_flags = ifp->if_flags;
		if ((oldflags & (IFF_PROMISC | IFF_UP)) !=
		    (ifp->if_flags & (IFF_PROMISC | IFF_UP))) {
			if (!(oldflags & IFF_UP) && ifp->if_flags & IFF_UP) {
				ieee80211_new_state(&sc->sc_ic,
				    IEEE80211_S_SCAN, -1);
				error = ENETRESET;
			} else if (oldflags & IFF_UP &&
			    !(ifp->if_flags & IFF_UP)) {
				ieee80211_new_state(&sc->sc_ic,
				    IEEE80211_S_INIT, -1);
				error = ENETRESET;
			}
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > PGT_FRAG_SIZE) {
			uprintf("%s: bad MTU (values > %u non-functional)\n",
			    ifp->if_xname, PGT_FRAG_SIZE);
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
			error = 0;
		}
		break;
#ifdef DEVICE_POLLING
	case SIOCSIFCAP:
		if (!(ifp->if_capabilities & IFF_RUNNING)) {
			error = EIO;
		} else {
			if ((ifr->ifr_reqcap & IFCAP_POLLING) !=
			    (ifp->if_capenable & IFCAP_POLLING))
				ifp->if_capenable ^= IFCAP_POLLING;
			error = 0;
		}
		break;
#endif
	default:
notours:
		/*
		 * XXX net80211 does not prevent modification of the
		 * ieee80211com while it fondles it.
		 */
		error = ieee80211_ioctl(ifp, cmd, req);
		break;
	}
	if (error == ENETRESET) {
		pgt_update_hw_from_sw(sc, 0, 0);
		error = 0;
	}
	return (error);
}

void
pgt_obj_bss2scanres(struct pgt_softc *sc, struct pgt_obj_bss *pob,
	    struct wi_scan_res *scanres, uint32_t noise)
{
	struct ieee80211_rateset *rs;
	struct wi_scan_res ap;
	unsigned int i, n;

	rs = &sc->sc_ic.ic_sup_rates[IEEE80211_MODE_AUTO];
	bzero(&ap, sizeof(ap));
	ap.wi_chan = ieee80211_mhz2ieee(letoh16(pob->pob_channel), 0);
	ap.wi_noise = noise;
	ap.wi_signal = letoh16(pob->pob_rssi);
	IEEE80211_ADDR_COPY(ap.wi_bssid, pob->pob_address);
	ap.wi_interval = letoh16(pob->pob_beacon_period);
	ap.wi_capinfo = letoh16(pob->pob_capinfo);
	ap.wi_ssid_len = min(sizeof(ap.wi_ssid), pob->pob_ssid.pos_length);
	memcpy(ap.wi_ssid, pob->pob_ssid.pos_ssid, ap.wi_ssid_len);
	n = 0;
	for (i = 0; i < 16; i++) {
		if (letoh16(pob->pob_rates) & (1 << i)) {
			if (i > rs->rs_nrates)
				break;
			ap.wi_srates[n++] = ap.wi_rate = rs->rs_rates[i];
			if (n >= sizeof(ap.wi_srates) / sizeof(ap.wi_srates[0]))
				break;
		}
	}
	memcpy(scanres, &ap, WI_PRISM2_RES_SIZE);
}

int
pgt_node_set_authorization(struct pgt_softc *sc,
    struct pgt_ieee80211_node *pin, enum pin_dot1x_authorization newstate)
{
	int error;

	if (pin->pin_dot1x_auth == newstate)
		return (0);
	IEEE80211_DPRINTF(("%s: %02x:%02x:%02x:%02x:%02x:%02x "
	    "changing authorization to %d\n", __func__,
	    pin->pin_node.ni_macaddr[0], pin->pin_node.ni_macaddr[1],
	    pin->pin_node.ni_macaddr[2], pin->pin_node.ni_macaddr[3],
	    pin->pin_node.ni_macaddr[4], pin->pin_node.ni_macaddr[5],
	    newstate));
	error = pgt_oid_set(sc,
	    newstate == PIN_DOT1X_AUTHORIZED ?
	    PGT_OID_EAPAUTHSTA : PGT_OID_EAPUNAUTHSTA,
	    pin->pin_node.ni_macaddr, sizeof(pin->pin_node.ni_macaddr));
	if (error == 0)
		pin->pin_dot1x_auth = pin->pin_dot1x_auth_desired = newstate;
	return (error);
}

#if 0
int
pgt_do_mlme_sta(struct pgt_softc *sc, struct ieee80211req_mlme *imlme)
{
	struct pgt_obj_mlme pffmlme;
	struct ieee80211com *ic;
	int error = 0;

	ic = &sc->sc_ic;
	switch (imlme->im_op) {
	case IEEE80211_MLME_ASSOC:
		IEEE80211_ADDR_COPY(pffmlme.pom_address, imlme->im_macaddr);
		pffmlme.pom_id = htole16(0);
		pffmlme.pom_state = htole16(PGT_MLME_STATE_ASSOC);
		pffmlme.pom_code = htole16(imlme->im_reason);
		error = pgt_oid_set(sc, PGT_OID_ASSOCIATE,
		    &pffmlme, sizeof(pffmlme));
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
#endif

#if 0
int
pgt_do_mlme_hostap(struct pgt_softc *sc, struct ieee80211req_mlme *imlme)
{
	struct pgt_ieee80211_node *pin;
	struct ieee80211com *ic;
	int error = 0;

	ic = &sc->sc_ic;
	switch (imlme->im_op) {
	/* Would IEEE80211_MLME_ASSOC/PGT_MLME_STATE_ASSOC be used for WDS? */
	case IEEE80211_MLME_AUTHORIZE:
		pin = (struct pgt_ieee80211_node *)ieee80211_find_node(ic,
		    imlme->im_macaddr);
		if (pin == NULL) {
			error = ENOENT;
			break;
		}
		error = pgt_node_set_authorization(sc, pin,
		    PIN_DOT1X_AUTHORIZED);
		ieee80211_release_node(ic, (struct ieee80211_node *)pin);
		break;
	case IEEE80211_MLME_UNAUTHORIZE:
		pin = (struct pgt_ieee80211_node *)ieee80211_find_node(ic,
		    imlme->im_macaddr);
		if (pin == NULL) {
			error = ENOENT;
			break;
		}
		error = pgt_node_set_authorization(sc, pin,
		    PIN_DOT1X_UNAUTHORIZED);
		ieee80211_release_node(ic, (struct ieee80211_node *)pin);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
#endif

#if 0
int
pgt_do_mlme_adhoc(struct pgt_softc *sc, struct ieee80211req_mlme *imlme)
{
	struct pgt_ieee80211_node *pin;
	struct ieee80211com *ic;
	int error = 0;

	ic = &sc->sc_ic;
	switch (imlme->im_op) {
	case IEEE80211_MLME_AUTHORIZE:
		pin = (struct pgt_ieee80211_node *)ieee80211_find_txnode(ic,
		    imlme->im_macaddr);
		if (pin == NULL) {
			error = ENOMEM;
			break;
		} else if ((struct ieee80211_node *)pin == ic->ic_bss) {
			error = EINVAL;
			break;
		}
		error = pgt_node_set_authorization(sc, pin,
		    PIN_DOT1X_AUTHORIZED);
		ieee80211_release_node(ic, (struct ieee80211_node *)pin);
		break;
	case IEEE80211_MLME_UNAUTHORIZE:
		pin = (struct pgt_ieee80211_node *)ieee80211_find_txnode(ic,
		    imlme->im_macaddr);
		if (pin == NULL) {
			error = ENOMEM;
			break;
		} else if ((struct ieee80211_node *)pin == ic->ic_bss) {
			error = EINVAL;
			break;
		}
		error = pgt_node_set_authorization(sc, pin,
		    PIN_DOT1X_UNAUTHORIZED);
		ieee80211_release_node(ic, (struct ieee80211_node *)pin);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
#endif

#if 0
int
pgt_80211_set(struct pgt_softc *sc, struct ieee80211req *ireq)
{
	//struct ieee80211req_mlme mlme;
	struct ieee80211com *ic;
	int error;

	ic = &sc->sc_ic;
	switch (ireq->i_type) {
	/*
	 * These are 802.11 requests we want to let fall through to
	 * net80211 but do not need a reset afterward.
	 */
	case IEEE80211_POWERSAVE_OFF:
	case IEEE80211_POWERSAVE_ON:
		error = ieee80211_ioctl(&ic->ic_if, SIOCS80211,
		    (caddr_t)ireq);
		if (error == ENETRESET)
			error = 0;
		break;
	/*
	 * These are 802.11 requests we want to let fall through to
	 * net80211 but then use their results without doing a full
	 * reset afterward.
	 */
	case IEEE80211_IOC_WEPKEY:
	case IEEE80211_IOC_WEPTXKEY:
		error = ieee80211_ioctl(&ic->ic_if, SIOCS80211, (caddr_t)ireq);
		if (error == ENETRESET) {
			pgt_update_hw_from_sw(sc,
			    ic->ic_state != IEEE80211_S_INIT,
			    ic->ic_opmode != IEEE80211_M_MONITOR);
			error = 0;
		}
		break;
	case IEEE80211_IOC_WEP:
		switch (ireq->i_val) {
		case IEEE80211_WEP_OFF:
		case IEEE80211_WEP_ON:
		case IEEE80211_WEP_MIXED:
			error = 0;
			break;
		default:
			error = EINVAL;
		}
		if (error)
			break;
		if (sc->sc_80211_ioc_wep != ireq->i_val) {
			sc->sc_80211_ioc_wep = ireq->i_val;
			pgt_update_hw_from_sw(sc, 0,
			    ic->ic_opmode != IEEE80211_M_MONITOR);
			error = 0;
		} else
			error = 0;
		break;
	case IEEE80211_IOC_AUTHMODE:
		switch (ireq->i_val) {
		case IEEE80211_AUTH_NONE:
		case IEEE80211_AUTH_OPEN:
		case IEEE80211_AUTH_SHARED:
			error = 0;
			break;
		default:
			error = EINVAL;
		}
		if (error)
			break;
		if (sc->sc_80211_ioc_auth != ireq->i_val) {
			sc->sc_80211_ioc_auth = ireq->i_val;
			pgt_update_hw_from_sw(sc, 0, 0);
			error = 0;
		} else
			error = 0;
		break;
	case IEEE80211_IOC_MLME:
		if (ireq->i_len != sizeof(mlme)) {
			error = EINVAL;
			break;
		}
		error = copyin(ireq->i_data, &mlme, sizeof(mlme));
		if (error)
			break;
		pgt_enter_critical(sc);
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			//error = pgt_do_mlme_sta(sc, &mlme);
			break;
		case IEEE80211_M_HOSTAP:
			//error = pgt_do_mlme_hostap(sc, &mlme);
			break;
		case IEEE80211_M_IBSS:
			//error = pgt_do_mlme_adhoc(sc, &mlme);
			break;
		default:
			error = EINVAL;
			break;
		}
		pgt_exit_critical(sc);
		if (error == 0)
			error = copyout(&mlme, ireq->i_data, sizeof(mlme));
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}
#endif

int
pgt_wavelan_get(struct pgt_softc *sc, struct wi_req *wreq)
{
	struct ieee80211com *ic;
	struct pgt_obj_bsslist *pob;
	struct wi_scan_p2_hdr *p2hdr;
	struct wi_scan_res *scan;
	uint32_t noise;
	unsigned int maxscan, i;
	int error;

	ic = &sc->sc_ic;
	switch (wreq->wi_type) {
	case WI_RID_COMMS_QUALITY:
		wreq->wi_val[0] = 0;	/* don't know correction factor */
		wreq->wi_val[1] = htole16(ic->ic_node_getrssi(ic, ic->ic_bss));
		wreq->wi_val[2] = htole16(sc->sc_noise);
		wreq->wi_len = 4;
		error = 0;
		break;
	case WI_RID_SCAN_RES:
		maxscan = PGT_OBJ_BSSLIST_NBSS;
		pob = malloc(sizeof(*pob) +
		    sizeof(struct pgt_obj_bss) * maxscan, M_DEVBUF, M_WAITOK);
		pgt_enter_critical(sc);
		error = pgt_oid_get(sc, PGT_OID_NOISE_FLOOR, &noise,
		    sizeof(noise));
		if (error == 0) {
			noise = letoh32(noise);
			error = pgt_oid_get(sc, PGT_OID_BSS_LIST, pob,
			    sizeof(*pob) +
			    sizeof(struct pgt_obj_bss) * maxscan);
		}
		if (error == 0) {
			maxscan = min(PGT_OBJ_BSSLIST_NBSS,
			    letoh32(pob->pob_count));
			maxscan = min(maxscan,
			    (sizeof(wreq->wi_val) - sizeof(*p2hdr)) /
			    WI_PRISM2_RES_SIZE);
			p2hdr = (struct wi_scan_p2_hdr *)&wreq->wi_val;
			p2hdr->wi_rsvd = 0;
			p2hdr->wi_reason = 1;	/* what should it be? */
			for (i = 0; i < maxscan; i++) {
				scan = (struct wi_scan_res *)
				    ((char *)&wreq->wi_val + sizeof(*p2hdr) +
				    i * WI_PRISM2_RES_SIZE);
				pgt_obj_bss2scanres(sc, &pob->pob_bsslist[i],
				    scan, noise);
			}
			wreq->wi_len = (maxscan * WI_PRISM2_RES_SIZE) / 2 +
			    sizeof(*p2hdr) / 2;
		}
		pgt_exit_critical(sc);
		free(pob, M_DEVBUF);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

int
pgt_wavelan_set(struct pgt_softc *sc, struct wi_req *wreq)
{
	int error;

	/*
	 * If we wanted to, we could support the "partial reset" interface
	 * here, but the Wavelan interface should really not need to be used.
	 */
	switch (wreq->wi_type) {
	case WI_RID_SCAN_REQ:
	case WI_RID_SCAN_APS:
		/* We're always scanning. */
		error = 0;
		break;
	case WI_RID_CHANNEL_LIST:
		/* The user can just use net80211's interface. */
		error = EPERM;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

void
node_mark_active_ap(void *arg, struct ieee80211_node *ni)
{
	/*
	 * HostAP mode lets all nodes stick around unless
	 * the firmware AP kicks them off.
	 */
	ni->ni_inact = 0;
}

void
node_mark_active_adhoc(void *arg, struct ieee80211_node *ni)
{
	struct pgt_ieee80211_node *pin;

	/*
	 * As there is no association in ad-hoc, we let links just
	 * time out naturally as long they are not holding any private
	 * configuration, such as 802.1x authorization.
	 */
	pin = (struct pgt_ieee80211_node *)ni;
	if (pin->pin_dot1x_auth == PIN_DOT1X_AUTHORIZED)
		pin->pin_node.ni_inact = 0;
}

void
pgt_periodic(struct ifnet *ifp)
{
	struct pgt_softc *sc;

	sc = ifp->if_softc;
	/*
	 * Check for timed out transmissions (and make sure to set
	 * this watchdog to fire again if there is still data in the
	 * output device queue).
	 */
	if (sc->sc_dirtyq_count[PGT_QUEUE_DATA_LOW_TX] != 0) {
		struct bintime txtime;
		int count;

		ifp->if_timer = 1;
		//getbinuptime(&txtime);
		bintime_sub(&txtime, &sc->sc_data_tx_started);
		if (txtime.sec >= 1) {
			count = pgt_drain_tx_queue(sc, PGT_QUEUE_DATA_LOW_TX);
			if (sc->sc_flags & SC_DEBUG_UNEXPECTED)
				printf("%s: timed out %d data transmissions\n",
				    sc->sc_dev.dv_xname, count);
		}
	}
	if (sc->sc_flags & (SC_DYING | SC_NEEDS_RESET))
		return;
	/*
	 * If we're goign to kick the device out of power-save mode
	 * just to update the BSSID and such, we should not do it
	 * very often; need to determine in what way to do that.
	 */
	if (ifp->if_flags & IFF_RUNNING &&
	    sc->sc_ic.ic_state != IEEE80211_S_INIT &&
	    sc->sc_ic.ic_opmode != IEEE80211_M_MONITOR)
#ifdef DEVICE_POLLING
		if (!(ifp->if_flags & IFF_POLLING))
#endif
			pgt_async_update(sc);
	/*
	 * As a firmware-based HostAP, we should not time out
	 * nodes inside the driver additionally to the timeout
	 * that exists in the firmware.  The only things we
	 * should have to deal with timing out when doing HostAP
	 * are the privacy-related.
	 */
	switch (sc->sc_ic.ic_opmode) {
	case IEEE80211_M_HOSTAP:
		ieee80211_iterate_nodes(&sc->sc_ic,
		    node_mark_active_ap, NULL);
		break;
	case IEEE80211_M_IBSS:
		ieee80211_iterate_nodes(&sc->sc_ic,
		    node_mark_active_adhoc, NULL);
		break;
	default:
		break;
	}
	ieee80211_watchdog(ifp);
	ifp->if_timer = 1;
}

int
pgt_init(struct ifnet *ifp)
{
	struct pgt_softc *sc = ifp->if_softc;
	struct ieee80211com *ic;

	ic = &sc->sc_ic;
	if (!(sc->sc_flags & (SC_DYING | SC_UNINITIALIZED)))
		pgt_update_hw_from_sw(sc,
		    ic->ic_state != IEEE80211_S_INIT,
		    ic->ic_opmode != IEEE80211_M_MONITOR);

	return (0);
}

/*
 * After most every configuration change, everything needs to be fully
 * reinitialized.  For some operations (currently, WEP settings
 * in ad-hoc+802.1x mode), the change is "soft" and doesn't remove
 * "associations," and allows EAP authorization to occur again.
 * If keepassoc is specified, the reset operation should try to go
 * back to the BSS had before.
 */
void
pgt_update_hw_from_sw(struct pgt_softc *sc, int keepassoc, int keepnodes)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct arpcom *ac = &ic->ic_ac;
	struct ifnet *ifp = &ac->ac_if;
	struct pgt_obj_key keyobj;
	struct pgt_obj_ssid essid;
	uint8_t availrates[IEEE80211_RATE_MAXSIZE + 1];
	uint32_t mode, bsstype, config, profile, channel, slot, preamble,
	    wep, exunencrypted, wepkey, dot1x, auth, mlme;
	unsigned int i;
	int success, shouldbeup;

	config = PGT_CONFIG_MANUAL_RUN | PGT_CONFIG_RX_ANNEX;
	/*
	 * Promiscuous mode is currently a no-op since packets transmitted,
	 * while in promiscuous mode, don't ever seem to go anywhere.
	 */
	shouldbeup = ifp->if_flags & IFF_RUNNING && ifp->if_flags & IFF_UP;
	if (shouldbeup) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (ifp->if_flags & IFF_PROMISC)
				mode = PGT_MODE_CLIENT;	/* what to do? */
			else
				mode = PGT_MODE_CLIENT;
			bsstype = PGT_BSS_TYPE_STA;
			dot1x = PGT_DOT1X_AUTH_ENABLED;
			break;
		case IEEE80211_M_IBSS:
			if (ifp->if_flags & IFF_PROMISC)
				mode = PGT_MODE_CLIENT;	/* what to do? */
			else
				mode = PGT_MODE_CLIENT;
			bsstype = PGT_BSS_TYPE_IBSS;
			dot1x = PGT_DOT1X_AUTH_ENABLED;
			break;
		case IEEE80211_M_HOSTAP:
			mode = PGT_MODE_AP;
			bsstype = PGT_BSS_TYPE_STA;
			/*
			 * For IEEE 802.1x, we need to authenticate and
			 * authorize hosts from here on or they remain
			 * associated but without the ability to send or
			 * receive normal traffic to us (courtesy the
			 * firmware AP implementation).
			 */
			dot1x = PGT_DOT1X_AUTH_ENABLED;
			/*
			 * WDS mode needs several things to work:
			 * discovery of exactly how creating the WDS
			 * links is meant to function, an interface
			 * for this, and ability to encode or decode
			 * the WDS frames.
			 */
			if (sc->sc_wds)
				config |= PGT_CONFIG_WDS;
			break;
		case IEEE80211_M_MONITOR:
			mode = PGT_MODE_PROMISCUOUS;
			bsstype = PGT_BSS_TYPE_ANY;
			dot1x = PGT_DOT1X_AUTH_NONE;
			break;
		default:
			goto badopmode;
		}
	} else {
badopmode:
		mode = PGT_MODE_CLIENT;
		bsstype = PGT_BSS_TYPE_NONE;
	}
	switch (ic->ic_curmode) {
	case IEEE80211_MODE_11A:
		profile = PGT_PROFILE_A_ONLY;
		preamble = PGT_OID_PREAMBLE_MODE_DYNAMIC;
		break;
	case IEEE80211_MODE_11B:
		profile = PGT_PROFILE_B_ONLY;
		preamble = PGT_OID_PREAMBLE_MODE_LONG;
		break;
	case IEEE80211_MODE_11G:
		profile = PGT_PROFILE_G_ONLY;
		preamble = PGT_OID_PREAMBLE_MODE_SHORT;
		break;
	case IEEE80211_MODE_FH:
	case IEEE80211_MODE_TURBO:
		/* not handled */
	case IEEE80211_MODE_AUTO:
		profile = PGT_PROFILE_MIXED_G_WIFI;
		preamble = PGT_OID_PREAMBLE_MODE_DYNAMIC;
		break;
	default:
		panic("unknown mode %d\n", ic->ic_curmode);
	}
	switch (sc->sc_80211_ioc_auth) {
	case IEEE80211_AUTH_NONE:
		auth = PGT_AUTH_MODE_NONE;
		break;
	case IEEE80211_AUTH_OPEN:
		auth = PGT_AUTH_MODE_OPEN;
		break;
	default:
		auth = PGT_AUTH_MODE_SHARED;
		break;
	}
	switch (sc->sc_80211_ioc_wep) {
	case IEEE80211_WEP_OFF:
		wep = 0;
		exunencrypted = 0;
		break;
	case IEEE80211_WEP_MIXED:
		wep = 1;
		exunencrypted = 0;
		break;
	case IEEE80211_WEP_ON:
	default:
		wep = 1;
		exunencrypted = 1;
		break;
	}
	mlme = htole32(PGT_MLME_AUTO_LEVEL_AUTO);
	wep = htole32(wep);
	exunencrypted = htole32(exunencrypted);
	profile = htole32(profile);
	preamble = htole32(preamble);
	bsstype = htole32(bsstype);
	config = htole32(config);
	mode = htole32(mode);
	if (!wep || !sc->sc_dot1x)
		dot1x = PGT_DOT1X_AUTH_NONE;
	dot1x = htole32(dot1x);
	auth = htole32(auth);
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		slot = htole32(PGT_OID_SLOT_MODE_SHORT);
	else
		slot = htole32(PGT_OID_SLOT_MODE_DYNAMIC);
	if (ic->ic_des_chan == IEEE80211_CHAN_ANYC) {
		if (keepassoc)
			channel = htole32(ieee80211_chan2ieee(ic,
			    ic->ic_bss->ni_chan));
		else
			channel = 0;
	} else {
		channel = htole32(ieee80211_chan2ieee(ic, ic->ic_des_chan));
	}
	for (i = 0; i < ic->ic_sup_rates[ic->ic_curmode].rs_nrates; i++)
		availrates[i] = ic->ic_sup_rates[ic->ic_curmode].rs_rates[i];
	availrates[i++] = 0;
	essid.pos_length = min(ic->ic_des_esslen, sizeof(essid.pos_ssid));
	memcpy(&essid.pos_ssid, ic->ic_des_essid, essid.pos_length);
	pgt_enter_critical(sc);
	for (success = 0; success == 0; success = 1) {
#define	SETOID(oid, var, size) {					\
	if (pgt_oid_set(sc, oid, var, size) != 0)			\
		break;							\
}
		SETOID(PGT_OID_PROFILE, &profile, sizeof(profile));
		SETOID(PGT_OID_CONFIG, &config, sizeof(config));
		SETOID(PGT_OID_MLME_AUTO_LEVEL, &mlme, sizeof(mlme));
		if (!IEEE80211_ADDR_EQ(ic->ic_myaddr, ac->ac_enaddr)) {
			SETOID(PGT_OID_MAC_ADDRESS, ac->ac_enaddr,
			    sizeof(ac->ac_enaddr));
			IEEE80211_ADDR_COPY(ic->ic_myaddr, ac->ac_enaddr);
		}
		SETOID(PGT_OID_MODE, &mode, sizeof(mode));
		SETOID(PGT_OID_BSS_TYPE, &bsstype, sizeof(bsstype));
		if (channel != 0)
			SETOID(PGT_OID_CHANNEL, &channel, sizeof(channel));
		if (ic->ic_flags & IEEE80211_F_DESBSSID) {
			SETOID(PGT_OID_BSSID, ic->ic_des_bssid,
			    sizeof(ic->ic_des_bssid));
		} else if (keepassoc) {
			SETOID(PGT_OID_BSSID, ic->ic_bss->ni_bssid,
			    sizeof(ic->ic_bss->ni_bssid));
		}
		SETOID(PGT_OID_SSID, &essid, sizeof(essid));
		if (ic->ic_des_esslen > 0)
			SETOID(PGT_OID_SSID_OVERRIDE, &essid, sizeof(essid));
		SETOID(PGT_OID_RATES, &availrates, i);
		SETOID(PGT_OID_EXTENDED_RATES, &availrates, i);
		SETOID(PGT_OID_PREAMBLE_MODE, &preamble, sizeof(preamble));
		SETOID(PGT_OID_SLOT_MODE, &slot, sizeof(slot));
		SETOID(PGT_OID_AUTH_MODE, &auth, sizeof(auth));
		SETOID(PGT_OID_EXCLUDE_UNENCRYPTED, &exunencrypted,
		    sizeof(exunencrypted));
		SETOID(PGT_OID_DOT1X, &dot1x, sizeof(dot1x));
		SETOID(PGT_OID_PRIVACY_INVOKED, &wep, sizeof(wep));
		if (letoh32(wep) != 0) {
			keyobj.pok_type = PGT_OBJ_KEY_TYPE_WEP;
			keyobj.pok_length = min(sizeof(keyobj.pok_key),
			    IEEE80211_KEYBUF_SIZE);
			keyobj.pok_length = min(keyobj.pok_length,
			    ic->ic_nw_keys[0].wk_len);
			bcopy(ic->ic_nw_keys[0].wk_key, keyobj.pok_key,
			    keyobj.pok_length);
			SETOID(PGT_OID_DEFAULT_KEY0, &keyobj, sizeof(keyobj));
			keyobj.pok_length = min(sizeof(keyobj.pok_key),
			    IEEE80211_KEYBUF_SIZE);
			keyobj.pok_length = min(keyobj.pok_length,
			    ic->ic_nw_keys[1].wk_len);
			bcopy(ic->ic_nw_keys[1].wk_key, keyobj.pok_key,
			    keyobj.pok_length);
			SETOID(PGT_OID_DEFAULT_KEY1, &keyobj, sizeof(keyobj));
			keyobj.pok_length = min(sizeof(keyobj.pok_key),
			    IEEE80211_KEYBUF_SIZE);
			keyobj.pok_length = min(keyobj.pok_length,
			    ic->ic_nw_keys[2].wk_len);
			bcopy(ic->ic_nw_keys[2].wk_key, keyobj.pok_key,
			    keyobj.pok_length);
			SETOID(PGT_OID_DEFAULT_KEY2, &keyobj, sizeof(keyobj));
			keyobj.pok_length = min(sizeof(keyobj.pok_key),
			    IEEE80211_KEYBUF_SIZE);
			keyobj.pok_length = min(keyobj.pok_length,
			    ic->ic_nw_keys[3].wk_len);
			bcopy(ic->ic_nw_keys[3].wk_key, keyobj.pok_key,
			    keyobj.pok_length);
			SETOID(PGT_OID_DEFAULT_KEY3, &keyobj, sizeof(keyobj));
			wepkey = htole32(ic->ic_wep_txkey);
			SETOID(PGT_OID_DEFAULT_KEYNUM, &wepkey, sizeof(wepkey));
		}
		/* set mode again to commit */
		SETOID(PGT_OID_MODE, &mode, sizeof(mode));
#undef SETOID
	}
	pgt_exit_critical(sc);
	if (success) {
		if (shouldbeup && keepnodes)
			sc->sc_flags |= SC_NOFREE_ALLNODES;
		if (shouldbeup)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		else
			ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	} else {
		printf("%s: problem setting modes\n", sc->sc_dev.dv_xname);
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	}
}

/*
 * After doing a soft-reinitialization, we will restore settings from
 * our pgt_ieee80211_nodes.  As we also lock the node list with our
 * softc mutex, unless we were to drop that the node list will remain
 * valid (see pgt_periodic()).
 */
void
pgt_update_hw_from_nodes(struct pgt_softc *sc)
{
	struct pgt_ieee80211_node *pin;
	//struct ieee80211_node *ni;
	struct pgt_ieee80211_node **addresses;
	size_t i, n;

	n = 0;
	/*
	TAILQ_FOREACH(ni, &sc->sc_ic.ic_node, ni_list) {
		pin = (struct pgt_ieee80211_node *)ni;
		if (pin->pin_dot1x_auth != pin->pin_dot1x_auth_desired)
			n++;
	}
	*/
	if (n == 0)
		return;
	addresses = malloc(sizeof(*addresses) * n, M_DEVBUF, M_NOWAIT);
	if (addresses == NULL)
		return;
	n = 0;
	/*
	TAILQ_FOREACH(ni, &sc->sc_ic.ic_node, ni_list) {
		pin = (struct pgt_ieee80211_node *)ni;
		if (pin->pin_dot1x_auth != pin->pin_dot1x_auth_desired) {
			addresses[n++] = pin;
			ieee80211_ref_node(&pin->pin_node);
		}
	}
	*/
	pgt_enter_critical(sc);
	for (i = 0; i < n; i++) {
		pin = addresses[i];
		if (pgt_oid_set(sc,
		    pin->pin_dot1x_auth_desired == PIN_DOT1X_AUTHORIZED ?
		    PGT_OID_EAPAUTHSTA : PGT_OID_EAPUNAUTHSTA,
		    pin->pin_node.ni_macaddr, sizeof(pin->pin_node.ni_macaddr))
		    == 0) {
			pin->pin_dot1x_auth = pin->pin_dot1x_auth_desired;
			IEEE80211_DPRINTF(("%s: %02x:%02x:%02x:%02x:%02x:%02x "
			    "reauthorized to %d\n", __func__,
			    pin->pin_node.ni_macaddr[0],
			    pin->pin_node.ni_macaddr[1],
			    pin->pin_node.ni_macaddr[2],
			    pin->pin_node.ni_macaddr[3],
			    pin->pin_node.ni_macaddr[4],
			    pin->pin_node.ni_macaddr[5],
			    pin->pin_dot1x_auth));
		}
		ieee80211_release_node(&sc->sc_ic, &pin->pin_node);
	}
	pgt_exit_critical(sc);
	free(addresses, M_DEVBUF);
}

void
pgt_hostap_handle_mlme(struct pgt_softc *sc, uint32_t oid,
    struct pgt_obj_mlme *mlme)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct pgt_ieee80211_node *pin;
	struct ieee80211_node *ni;

	ni = ieee80211_find_node(ic, mlme->pom_address);
	pin = (struct pgt_ieee80211_node *)ni;
	switch (oid) {
	case PGT_OID_DISASSOCIATE:
		if (ni != NULL)
			ieee80211_release_node(&sc->sc_ic, ni);
		break;
	case PGT_OID_ASSOCIATE:
		if (ni == NULL) {
			ni = ieee80211_dup_bss(ic, mlme->pom_address);
			if (ni == NULL)
				break;
			ic->ic_newassoc(ic, ni, 1);
			pin = (struct pgt_ieee80211_node *)ni;
		}
		ni->ni_associd = letoh16(mlme->pom_id);
		pin->pin_mlme_state = letoh16(mlme->pom_state);
		break;
	default:
		if (pin != NULL)
			pin->pin_mlme_state = letoh16(mlme->pom_state);
		break;
	}
}

/*
 * Either in response to an event or after a certain amount of time,
 * synchronize our idea of the network we're part of from the hardware.
 */
void
pgt_update_sw_from_hw(struct pgt_softc *sc, struct pgt_async_trap *pa,
	    struct mbuf *args)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct pgt_obj_ssid ssid;
	struct pgt_obj_bss bss;
	uint32_t channel, noise, ls;
	int error;

	if (pa != NULL) {
#if 0
		struct pgt_obj_mlmeex *mlmeex;
#endif
		struct pgt_obj_mlme *mlme;
		uint32_t oid;

		oid = *mtod(args, uint32_t *);
		m_adj(args, sizeof(uint32_t));
		if (sc->sc_debug & SC_DEBUG_TRAP)
			printf("%s: trap: oid 0x%x len %u\n",
			    sc->sc_dev.dv_xname, oid, args->m_len);
		switch (oid) {
		case PGT_OID_LINK_STATE:
			if (args->m_len < sizeof(uint32_t))
				break;
			ls = letoh32(*mtod(args, uint32_t *));
			if (sc->sc_debug & (SC_DEBUG_TRAP | SC_DEBUG_LINK))
				printf("%s: link: %u\n",
				    sc->sc_dev.dv_xname, ls);
			if (ls)
				ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
			else
				ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
			goto gotlinkstate;
		case PGT_OID_DEAUTHENTICATE:
		case PGT_OID_AUTHENTICATE:
		case PGT_OID_DISASSOCIATE:
		case PGT_OID_ASSOCIATE:
			if (args->m_len < sizeof(struct pgt_obj_mlme))
				break;
			mlme = mtod(args, struct pgt_obj_mlme *);
			if (sc->sc_debug & SC_DEBUG_TRAP)
				printf("%s: mlme: address "
				    "%02x:%02x:%02x:%02x:%02x:%02x "
				    "id 0x%02x state 0x%02x code 0x%02x\n",
				    sc->sc_dev.dv_xname,
				    mlme->pom_address[0], mlme->pom_address[1],
				    mlme->pom_address[2], mlme->pom_address[3],
				    mlme->pom_address[4], mlme->pom_address[5],
				    letoh16(mlme->pom_id),
				    letoh16(mlme->pom_state),
				    letoh16(mlme->pom_code));
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				pgt_hostap_handle_mlme(sc, oid, mlme);
			break;
		}
		return;
	}
	if (ic->ic_state == IEEE80211_S_SCAN) {
		pgt_enter_critical(sc);
		error = pgt_oid_get(sc, PGT_OID_LINK_STATE, &ls, sizeof(ls));
		pgt_exit_critical(sc);
		if (error)
			return;
		if (letoh32(ls) != 0)
			ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	}
gotlinkstate:
	pgt_enter_critical(sc);
	if (pgt_oid_get(sc, PGT_OID_NOISE_FLOOR, &noise, sizeof(noise)) != 0)
		goto out;
	sc->sc_noise = letoh32(noise);
	if (ic->ic_state == IEEE80211_S_RUN) {
		if (pgt_oid_get(sc, PGT_OID_CHANNEL, &channel,
		    sizeof(channel)) != 0)
			goto out;
		channel = min(letoh32(channel), IEEE80211_CHAN_MAX);
		ic->ic_bss->ni_chan = &ic->ic_channels[channel];
		if (pgt_oid_get(sc, PGT_OID_BSSID, ic->ic_bss->ni_bssid,
		    sizeof(ic->ic_bss->ni_bssid)) != 0)
			goto out;
		IEEE80211_ADDR_COPY(&bss.pob_address, ic->ic_bss->ni_bssid);
		error = pgt_oid_retrieve(sc, PGT_OID_BSS_FIND, &bss,
		    sizeof(bss));
		if (error == 0)
			ic->ic_bss->ni_rssi = bss.pob_rssi;
		else if (error != EPERM)
			goto out;
		error = pgt_oid_get(sc, PGT_OID_SSID, &ssid, sizeof(ssid));
		if (error)
			goto out;
		ic->ic_bss->ni_esslen = min(ssid.pos_length,
		    sizeof(ic->ic_bss->ni_essid));
		memcpy(ic->ic_bss->ni_essid, ssid.pos_ssid,
		    ssid.pos_length);
	}
out:
	pgt_exit_critical(sc);
}

int
pgt_media_change(struct ifnet *ifp)
{
	struct pgt_softc *sc = ifp->if_softc;
	int error;
	
	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		pgt_update_hw_from_sw(sc, 0, 0);
		error = 0;
	}
	return (error);
}

void
pgt_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct pgt_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t ls;
	int alreadylocked;
	
	imr->ifm_active = IFM_IEEE80211;
	if (!(ifp->if_flags & IFF_UP))
		return;
	alreadylocked = 0;
	if (!alreadylocked)
		alreadylocked = 0;
	imr->ifm_status = IFM_AVALID;
	pgt_enter_critical(sc);
	if (pgt_oid_get(sc, PGT_OID_LINK_STATE, &ls, sizeof(ls)) != 0) {
		imr->ifm_active |= IFM_NONE;
		imr->ifm_status = 0;
		goto out;
	}
	ls = letoh32(ls);
	if (sc->sc_debug & SC_DEBUG_LINK)
		printf("%s: link: %u\n", sc->sc_dev.dv_xname, ls);
	if (ls == 0) {
		imr->ifm_active |= IFM_NONE;
		imr->ifm_status = 0;
		goto out;
	}
	if (ic->ic_state != IEEE80211_S_INIT)
		imr->ifm_status |= IFM_ACTIVE;
	/* XXX query the PHY "mode"? */
	imr->ifm_active |= ieee80211_rate2media(ic, ls, IEEE80211_MODE_AUTO);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_AHDEMO:
		imr->ifm_active |= IFM_IEEE80211_ADHOC | IFM_FLAG0;
		break;
	case IEEE80211_M_HOSTAP:
		imr->ifm_active |= IFM_IEEE80211_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;
	}
out:
	pgt_exit_critical(sc);
	if (!alreadylocked)
		alreadylocked = 0;
}

/*
 * Synchronization here is due to the softc lock being held when called.
 */
int
pgt_new_state(struct ieee80211com *ic, enum ieee80211_state nstate,
    int mgtdata)
{
	struct pgt_softc *sc;
	enum ieee80211_state ostate;

	sc = (struct pgt_softc *)ic->ic_if.if_softc;
	ostate = ic->ic_state;
	IEEE80211_DPRINTF(("%s: %s -> %s\n", __func__,
	    ieee80211_state_name[ostate], ieee80211_state_name[nstate]));
	switch (nstate) {
	case IEEE80211_S_INIT:
		if (sc->sc_dirtyq_count[PGT_QUEUE_DATA_LOW_TX] == 0)
			ic->ic_if.if_timer = 0;
		ic->ic_mgt_timer = 0;
		ic->ic_flags &= ~IEEE80211_F_SIBSS;
		//IF_DRAIN(&ic->ic_mgtq);
		if (ic->ic_wep_ctx != NULL) {
			free(ic->ic_wep_ctx, M_DEVBUF);  
			ic->ic_wep_ctx = NULL;
		}
		ieee80211_free_allnodes(ic);
		ic->ic_state = nstate;
		break;
	case IEEE80211_S_SCAN:
		ic->ic_if.if_timer = 1;
		ic->ic_mgt_timer = 0;
		if (sc->sc_flags & SC_NOFREE_ALLNODES) {
			//struct ieee80211_node *ni;
			//struct pgt_ieee80211_node *pin;

			/* Locked already by pff mutex. */
			/*
			TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
				pin = (struct pgt_ieee80211_node *)ni;
				pin->pin_dot1x_auth = PIN_DOT1X_UNAUTHORIZED;
			}
			*/
			sc->sc_flags &= ~SC_NOFREE_ALLNODES;
		} else {
			ieee80211_free_allnodes(ic);
		}
		ic->ic_state = nstate;
		/* Just use any old channel; we override it anyway. */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			ieee80211_create_ibss(ic, ic->ic_ibss_chan);
		break;
	case IEEE80211_S_RUN:
		ic->ic_if.if_timer = 1;
		ic->ic_mgt_timer = 0;
		ic->ic_state = nstate;
		pgt_update_hw_from_nodes(sc);
		//if (!IFQ_DRV_IS_EMPTY(&ic->ic_if.if_snd))
		//	pgt_start_body(sc, ic, &ic->ic_if);
		break;
	default:
		break;
	}
	return (0);
}

int
pgt_drain_tx_queue(struct pgt_softc *sc, enum pgt_queue pq)
{
	int wokeup = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_PREWRITE);
	sc->sc_cb->pcb_device_curfrag[pq] =
	    sc->sc_cb->pcb_driver_curfrag[pq];
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cbdmam, 0,
	    sc->sc_cbdmam->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_PREREAD);
	while (!TAILQ_EMPTY(&sc->sc_dirtyq[pq])) {
		struct pgt_desc *pd;

		pd = TAILQ_FIRST(&sc->sc_dirtyq[pq]);
		TAILQ_REMOVE(&sc->sc_dirtyq[pq], pd, pd_link);
		sc->sc_dirtyq_count[pq]--;
		TAILQ_INSERT_TAIL(&sc->sc_freeq[pq], pd, pd_link);
		sc->sc_freeq_count[pq]++;
		pgt_unload_tx_desc_frag(sc, pd);
		if (sc->sc_debug & SC_DEBUG_QUEUES)
			printf("%s: queue: tx %u <- [%u] (drained)\n",
			    sc->sc_dev.dv_xname, pd->pd_fragnum, pq);
		wokeup++;
		if (pgt_queue_is_data(pq)) {
			sc->sc_ic.ic_if.if_oerrors++;
			//pgt_try_exit_data_critical(sc);
		}
	}
	return (wokeup);
}

int
pgt_dma_alloc(struct pgt_softc *sc)
{
	size_t size;
	int i, error, nsegs;

	for (i = 0; i < PGT_QUEUE_COUNT; i++)
		TAILQ_INIT(&sc->sc_freeq[i]);

	/*
	 * control block
	 */
	size = sizeof(struct pgt_control_block);

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_ALLOCNOW, &sc->sc_cbdmam);
	if (error != 0) {
		printf("%s: can not create DMA tag for control block\n",
		    sc->sc_dev);
		goto out;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE,
	    0, &sc->sc_cbdmas, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: can not allocate DMA memory for control block\n",
		    sc->sc_dev);
		goto out;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->sc_cbdmas, nsegs,
	    size, (caddr_t *)&sc->sc_cb, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: can not map DMA memory for control block\n",
		    sc->sc_dev);
		goto out;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_cbdmam,
	    sc->sc_cb, size, NULL, BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: can not load DMA map for control block\n",
		    sc->sc_dev);
		goto out;
	}

	/*
	 * powersave
	 */
	size = PGT_FRAG_SIZE * PGT_PSM_BUFFER_FRAME_COUNT;

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_ALLOCNOW, &sc->sc_psmdmam);
	if (error != 0) {
		printf("%s: can not create DMA tag for powersave\n",
		    sc->sc_dev);
		goto out;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE,
	   0, &sc->sc_psmdmas, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: can not allocate DMA memory for powersave\n",
		    sc->sc_dev);
		goto out;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->sc_psmdmas, nsegs,
	    size, (caddr_t *)&sc->sc_psmbuf, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: can not map DMA memory for powersave\n",
		    sc->sc_dev);
		goto out;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_psmdmam,
	    sc->sc_psmbuf, size, NULL, BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: can not load DMA map for powersave\n",
		    sc->sc_dev);
		goto out;
	}

	/*
	 * fragments
	 */
	error = pgt_dma_alloc_queue(sc, PGT_QUEUE_DATA_LOW_RX);
	if (error != 0)
		goto out;

	error = pgt_dma_alloc_queue(sc, PGT_QUEUE_DATA_LOW_TX);
	if (error != 0)
		goto out;

	error = pgt_dma_alloc_queue(sc, PGT_QUEUE_DATA_HIGH_RX);
	if (error != 0)
		goto out;

	error = pgt_dma_alloc_queue(sc, PGT_QUEUE_DATA_HIGH_TX);
	if (error != 0)
		goto out;

	error = pgt_dma_alloc_queue(sc, PGT_QUEUE_MGMT_RX);
	if (error != 0)
		goto out;

	error = pgt_dma_alloc_queue(sc, PGT_QUEUE_MGMT_TX);
	if (error != 0)
		goto out;

out:
	if (error) {
		printf("%s: error in DMA allocation\n", sc->sc_dev);
		pgt_dma_free(sc);
	}

	return (error);
}

int
pgt_dma_alloc_queue(struct pgt_softc *sc, enum pgt_queue pq)
{
	struct pgt_desc *pd;
	struct pgt_frag *pcbqueue;
	size_t i, qsize;
	int error, nsegs;

	switch (pq) {
		case PGT_QUEUE_DATA_LOW_RX:
			pcbqueue = sc->sc_cb->pcb_data_low_rx;
			qsize = PGT_QUEUE_DATA_RX_SIZE;
			break;
		case PGT_QUEUE_DATA_LOW_TX:
			pcbqueue = sc->sc_cb->pcb_data_low_tx;
			qsize = PGT_QUEUE_DATA_TX_SIZE;
			break;
		case PGT_QUEUE_DATA_HIGH_RX:
			pcbqueue = sc->sc_cb->pcb_data_high_rx;
			qsize = PGT_QUEUE_DATA_RX_SIZE;
			break;
		case PGT_QUEUE_DATA_HIGH_TX:
			pcbqueue = sc->sc_cb->pcb_data_high_tx;
			qsize = PGT_QUEUE_DATA_TX_SIZE;
			break;
		case PGT_QUEUE_MGMT_RX:
			pcbqueue = sc->sc_cb->pcb_mgmt_rx;
			qsize = PGT_QUEUE_MGMT_SIZE;
			break;
		case PGT_QUEUE_MGMT_TX:
			pcbqueue = sc->sc_cb->pcb_mgmt_tx;
			qsize = PGT_QUEUE_MGMT_SIZE;
			break;
	}

	for (i = 0; i < qsize; i++) {
		pd = malloc(sizeof(*pd), M_DEVBUF, M_WAITOK);

		error = bus_dmamap_create(sc->sc_dmat, PGT_FRAG_SIZE, 1,
		    PGT_FRAG_SIZE, 0, BUS_DMA_ALLOCNOW, &pd->pd_dmam);
		if (error != 0) {
			printf("%s: can not create DMA tag for fragment\n",
			    sc->sc_dev);
			break;
		}

		error = bus_dmamem_alloc(sc->sc_dmat, PGT_FRAG_SIZE, PAGE_SIZE,
		    0, &pd->pd_dmas, 1, &nsegs, BUS_DMA_WAITOK);
		if (error != 0) {
			printf("%s: error alloc frag %u on queue %u\n",
			    sc->sc_dev, i, pq, error);
			free(pd, M_DEVBUF);
			break;
		}

		error = bus_dmamem_map(sc->sc_dmat, &pd->pd_dmas, nsegs,
		    PGT_FRAG_SIZE, (caddr_t *)&pd->pd_mem, BUS_DMA_WAITOK);
		if (error != 0) {
			printf("%s: error map frag %u on queue %u\n",
			    sc->sc_dev, i, pq);
			free(pd, M_DEVBUF);
			break;
		}

		if (pgt_queue_is_rx(pq)) {
			error = bus_dmamap_load(sc->sc_dmat, pd->pd_dmam,
			    pd->pd_mem, PGT_FRAG_SIZE, NULL, BUS_DMA_WAITOK);
			if (error != 0) {
				printf("%s: error load frag %u on queue %u\n",
				    sc->sc_dev, i, pq);
				bus_dmamem_free(sc->sc_dmat, &pd->pd_dmas,
				    nsegs);
				free(pd, M_DEVBUF);
				break;
			}
		}
		TAILQ_INSERT_TAIL(&sc->sc_freeq[pq], pd, pd_link);
	}

	return (error);
}

void
pgt_dma_free(struct pgt_softc *sc)
{
	/*
	 * fragments
	 */
	if (sc->sc_dmat != NULL) {
		pgt_dma_free_queue(sc, PGT_QUEUE_DATA_LOW_RX);
		pgt_dma_free_queue(sc, PGT_QUEUE_DATA_LOW_TX);
		pgt_dma_free_queue(sc, PGT_QUEUE_DATA_HIGH_RX);
		pgt_dma_free_queue(sc, PGT_QUEUE_DATA_HIGH_TX);
		pgt_dma_free_queue(sc, PGT_QUEUE_MGMT_RX);
		pgt_dma_free_queue(sc, PGT_QUEUE_MGMT_TX);
	}

	/*
	 * powersave
	 */
	if (sc->sc_psmbuf != NULL) {
		bus_dmamap_unload(sc->sc_dmat, sc->sc_psmdmam);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_psmdmas, 1);
		sc->sc_psmbuf = NULL;
		sc->sc_psmdmam = NULL;
	}

	/*
	 * control block
	 */
	if (sc->sc_cb != NULL) {
		bus_dmamap_unload(sc->sc_dmat, sc->sc_cbdmam);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_cbdmas, 1);
		sc->sc_cb = NULL;
		sc->sc_cbdmam = NULL;
	}
}

void
pgt_dma_free_queue(struct pgt_softc *sc, enum pgt_queue pq)
{
	struct pgt_desc	*pd;

	while (!TAILQ_EMPTY(&sc->sc_freeq[pq])) {
		pd = TAILQ_FIRST(&sc->sc_freeq[pq]);
		TAILQ_REMOVE(&sc->sc_freeq[pq], pd, pd_link);
		if (pd->pd_dmam != NULL) {
			bus_dmamap_unload(sc->sc_dmat, pd->pd_dmam);
			pd->pd_dmam = NULL;
		}
		bus_dmamem_free(sc->sc_dmat, &pd->pd_dmas, 1);
		free(pd, M_DEVBUF);
	}
}
