/*	$OpenBSD: if_bnxt.c,v 1.2 2018/07/11 06:39:57 jmatthew Exp $	*/
/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2018 Jonathan Matthew <jmatthew@openbsd.org>
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
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/stdint.h>
#include <sys/sockio.h>
#include <sys/atomic.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define __FBSDID(x)
#include <dev/pci/if_bnxtreg.h>

#include <net/if.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#define BNXT_HWRM_BAR		0x10
#define BNXT_DOORBELL_BAR	0x18

#define BNXT_RX_RING_ID		0
#define BNXT_AG_RING_ID		1
#define BNXT_TX_RING_ID		3

#define BNXT_MAX_QUEUE		8
#define BNXT_MAX_MTU		9000

#define BNXT_MAX_TX_SEGS	32	/* a bit much? */

#define BNXT_HWRM_SHORT_REQ_LEN	sizeof(struct hwrm_short_input)

#define BNXT_HWRM_LOCK_INIT(_sc, _name)	\
	mtx_init_flags(&sc->sc_lock, IPL_NET, _name, 0)
#define BNXT_HWRM_LOCK(_sc) 		mtx_enter(&_sc->sc_lock)
#define BNXT_HWRM_UNLOCK(_sc) 		mtx_leave(&_sc->sc_lock)
#define BNXT_HWRM_LOCK_DESTROY(_sc)	/* nothing */
#define BNXT_HWRM_LOCK_ASSERT(_sc)	MUTEX_ASSERT_LOCKED(&_sc->sc_lock)

#define BNXT_FLAG_VF            0x0001
#define BNXT_FLAG_NPAR          0x0002
#define BNXT_FLAG_WOL_CAP       0x0004
#define BNXT_FLAG_SHORT_CMD     0x0008

#define NEXT_CP_CONS_V(_ring, _cons, _v_bit)		\
do {	 						\
	if (++(_cons) == (_ring)->ring_size)		\
		((_cons) = 0, (_v_bit) = !_v_bit);	\
} while (0);

struct bnxt_cos_queue {
	uint8_t			id;
	uint8_t			profile;
};

struct bnxt_ring {
	uint64_t		paddr;
	uint64_t		doorbell;
	caddr_t			vaddr;
	uint32_t		ring_size;
	uint16_t		id;
	uint16_t		phys_id;
	struct bnxt_full_tpa_start *tpa_start;
};

struct bnxt_cp_ring {
	struct bnxt_ring	ring;
	void			*irq;
	struct bnxt_softc	*softc;
	uint32_t		cons;
	int			v_bit;
	struct ctx_hw_stats	*stats;
	uint32_t		stats_ctx_id;
	uint32_t		last_idx;
};

struct bnxt_grp_info {
	uint32_t		grp_id;
	uint16_t		stats_ctx;
	uint16_t		rx_ring_id;
	uint16_t		cp_ring_id;
	uint16_t		ag_ring_id;
};

struct bnxt_vnic_info {
	uint16_t		id;
	uint16_t		def_ring_grp;
	uint16_t		cos_rule;
	uint16_t		lb_rule;
	uint16_t		mru;

	uint32_t		rx_mask;
	/* multicast things */

	uint32_t		flags;
#define BNXT_VNIC_FLAG_DEFAULT		0x01
#define BNXT_VNIC_FLAG_BD_STALL		0x02
#define BNXT_VNIC_FLAG_VLAN_STRIP	0x04

	uint64_t		filter_id;
	uint32_t		flow_id;

	uint16_t		rss_id;
	/* rss things */

	/* vlan things */
};

struct bnxt_slot {
	bus_dmamap_t		bs_map;
	struct mbuf		*bs_m;
};

struct bnxt_dmamem {
	bus_dmamap_t		bdm_map;
	bus_dma_segment_t	bdm_seg;
	size_t			bdm_size;
	caddr_t			bdm_kva;
};
#define BNXT_DMA_MAP(_bdm)	((_bdm)->bdm_map)
#define BNXT_DMA_LEN(_bdm)	((_bdm)->bdm_size)
#define BNXT_DMA_DVA(_bdm)	((u_int64_t)(_bdm)->bdm_map->dm_segs[0].ds_addr)
#define BNXT_DMA_KVA(_bdm)	((void *)(_bdm)->bdm_kva)

struct bnxt_softc {
	struct device		sc_dev;
	struct arpcom		sc_ac;
	struct ifmedia		sc_media;

	struct mutex		sc_lock;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;
	bus_dma_tag_t		sc_dmat;

	bus_space_tag_t		sc_hwrm_t;
	bus_space_handle_t	sc_hwrm_h;
	bus_size_t		sc_hwrm_s;

	struct bnxt_dmamem	*sc_cmd_resp;
	uint16_t		sc_cmd_seq;
	uint16_t		sc_max_req_len;
	uint32_t		sc_cmd_timeo;
	uint32_t		sc_flags;

	bus_space_tag_t		sc_db_t;
	bus_space_handle_t	sc_db_h;
	bus_size_t		sc_db_s;

	void			*sc_ih;

	int			sc_max_tc;
	struct bnxt_cos_queue	sc_q_info[BNXT_MAX_QUEUE];

	struct bnxt_vnic_info	sc_vnic;
	struct bnxt_dmamem	*sc_stats_ctx_mem;

	struct bnxt_cp_ring	sc_cp_ring;
	struct bnxt_dmamem	*sc_cp_ring_mem;

	/* rx */
	struct bnxt_dmamem	*sc_rx_ring_mem;	/* rx and ag */
	struct bnxt_ring	sc_rx_ring;
	/* struct bnxt_ring	sc_rx_ag_ring; */
	struct bnxt_grp_info	sc_ring_group;
	struct if_rxring	sc_rxr;
	struct bnxt_slot	*sc_rx_slots;
	int			sc_rx_prod;
	int			sc_rx_cons;
	struct timeout		sc_rx_refill;

	/* tx */
	struct bnxt_dmamem	*sc_tx_ring_mem;
	struct bnxt_ring	sc_tx_ring;
	struct bnxt_slot	*sc_tx_slots;
	int			sc_tx_prod;
	int			sc_tx_cons;
	int			sc_tx_ring_prod;
	int			sc_tx_ring_cons;
};
#define DEVNAME(_sc)	((_sc)->sc_dev.dv_xname)

const struct pci_matchid bnxt_devices[] = {
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57301 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57302 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57304 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57311 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57312 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57314 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57402 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57404 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57406 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57407 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57412 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57414 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57416 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57416_SFP },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57417 },
	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_BCM57417_SFP }
};

int		bnxt_match(struct device *, void *, void *);
void		bnxt_attach(struct device *, struct device *, void *);

void		bnxt_up(struct bnxt_softc *);
void		bnxt_down(struct bnxt_softc *);
void		bnxt_iff(struct bnxt_softc *);
int		bnxt_ioctl(struct ifnet *, u_long, caddr_t);
int		bnxt_rxrinfo(struct bnxt_softc *, struct if_rxrinfo *);
void		bnxt_start(struct ifqueue *);
int		bnxt_intr(void *);
void		bnxt_watchdog(struct ifnet *);
void		bnxt_media_status(struct ifnet *, struct ifmediareq *);
int		bnxt_media_change(struct ifnet *);

void		bnxt_mark_cpr_invalid(struct bnxt_cp_ring *);
void		bnxt_write_cp_doorbell(struct bnxt_softc *, struct bnxt_ring *,
		    int);
void		bnxt_write_cp_doorbell_index(struct bnxt_softc *,
		    struct bnxt_ring *, uint32_t, int);
void		bnxt_write_rx_doorbell(struct bnxt_softc *, struct bnxt_ring *,
		    int);
void		bnxt_write_tx_doorbell(struct bnxt_softc *, struct bnxt_ring *,
		    int);

int		bnxt_rx_fill(struct bnxt_softc *);
u_int		bnxt_rx_fill_slots(struct bnxt_softc *, u_int);
void		bnxt_refill(void *);
void		bnxt_rx(struct bnxt_softc *, struct mbuf_list *, int *,
		    struct cmpl_base *, struct cmpl_base *);

int		bnxt_txeof(struct bnxt_softc *, struct cmpl_base *);


/* HWRM Function Prototypes */
int		bnxt_hwrm_ring_alloc(struct bnxt_softc *, uint8_t,
		    struct bnxt_ring *, uint16_t, uint32_t, int);
int		bnxt_hwrm_ring_free(struct bnxt_softc *, uint8_t,
		    struct bnxt_ring *);
int		bnxt_hwrm_ver_get(struct bnxt_softc *);
int		bnxt_hwrm_queue_qportcfg(struct bnxt_softc *);
int		bnxt_hwrm_func_drv_rgtr(struct bnxt_softc *);
int		bnxt_hwrm_func_qcaps(struct bnxt_softc *);
int		bnxt_hwrm_func_qcfg(struct bnxt_softc *);
int		bnxt_hwrm_func_reset(struct bnxt_softc *);
int		bnxt_hwrm_vnic_ctx_alloc(struct bnxt_softc *, uint16_t *);
int		bnxt_hwrm_vnic_ctx_free(struct bnxt_softc *, uint16_t *);
int		bnxt_hwrm_vnic_cfg(struct bnxt_softc *,
		    struct bnxt_vnic_info *);
int		bnxt_hwrm_stat_ctx_alloc(struct bnxt_softc *,
		    struct bnxt_cp_ring *, uint64_t);
int		bnxt_hwrm_stat_ctx_free(struct bnxt_softc *,
		    struct bnxt_cp_ring *);
int		bnxt_hwrm_ring_grp_alloc(struct bnxt_softc *,
		    struct bnxt_grp_info *);
int		bnxt_hwrm_ring_grp_free(struct bnxt_softc *,
		    struct bnxt_grp_info *);
int		bnxt_hwrm_vnic_alloc(struct bnxt_softc *,
		    struct bnxt_vnic_info *);
int		bnxt_hwrm_vnic_free(struct bnxt_softc *,
		    struct bnxt_vnic_info *);
int		bnxt_hwrm_cfa_l2_set_rx_mask(struct bnxt_softc *,
		    struct bnxt_vnic_info *vnic);
int		bnxt_hwrm_set_filter(struct bnxt_softc *,
		    struct bnxt_vnic_info *);
int		bnxt_hwrm_free_filter(struct bnxt_softc *,
		    struct bnxt_vnic_info *);
int		bnxt_cfg_async_cr(struct bnxt_softc *);
int		bnxt_hwrm_nvm_get_dev_info(struct bnxt_softc *, uint16_t *,
		    uint16_t *, uint32_t *, uint32_t *, uint32_t *, uint32_t *);
int		bnxt_hwrm_port_phy_qcfg(struct bnxt_softc *,
		    struct ifmediareq *);
int		bnxt_hwrm_func_rgtr_async_events(struct bnxt_softc *);

/* not used yet: */
#if 0
int bnxt_hwrm_func_drv_unrgtr(struct bnxt_softc *softc, bool shutdown);

int bnxt_hwrm_set_link_setting(struct bnxt_softc *softc, bool set_pause,
    bool set_eee, bool set_link); 
int bnxt_hwrm_set_pause(struct bnxt_softc *softc);

int bnxt_hwrm_port_qstats(struct bnxt_softc *softc);

int bnxt_hwrm_rss_cfg(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic,
    uint32_t hash_type);

int bnxt_hwrm_vnic_tpa_cfg(struct bnxt_softc *softc);
void bnxt_validate_hw_lro_settings(struct bnxt_softc *softc);
int bnxt_hwrm_nvm_find_dir_entry(struct bnxt_softc *softc, uint16_t type,
    uint16_t *ordinal, uint16_t ext, uint16_t *index, bool use_index,
    uint8_t search_opt, uint32_t *data_length, uint32_t *item_length,
    uint32_t *fw_ver);
int bnxt_hwrm_nvm_read(struct bnxt_softc *softc, uint16_t index,
    uint32_t offset, uint32_t length, struct iflib_dma_info *data);
int bnxt_hwrm_nvm_modify(struct bnxt_softc *softc, uint16_t index,
    uint32_t offset, void *data, bool cpyin, uint32_t length);
int bnxt_hwrm_fw_reset(struct bnxt_softc *softc, uint8_t processor,
    uint8_t *selfreset);
int bnxt_hwrm_fw_qstatus(struct bnxt_softc *softc, uint8_t type,
    uint8_t *selfreset);
int bnxt_hwrm_nvm_write(struct bnxt_softc *softc, void *data, bool cpyin,
    uint16_t type, uint16_t ordinal, uint16_t ext, uint16_t attr,
    uint16_t option, uint32_t data_length, bool keep, uint32_t *item_length,
    uint16_t *index);
int bnxt_hwrm_nvm_erase_dir_entry(struct bnxt_softc *softc, uint16_t index);
int bnxt_hwrm_nvm_get_dir_info(struct bnxt_softc *softc, uint32_t *entries,
    uint32_t *entry_length);
int bnxt_hwrm_nvm_get_dir_entries(struct bnxt_softc *softc,
    uint32_t *entries, uint32_t *entry_length, struct iflib_dma_info *dma_data);

int bnxt_hwrm_nvm_install_update(struct bnxt_softc *softc,
    uint32_t install_type, uint64_t *installed_items, uint8_t *result,
    uint8_t *problem_item, uint8_t *reset_required);
int bnxt_hwrm_nvm_verify_update(struct bnxt_softc *softc, uint16_t type,
    uint16_t ordinal, uint16_t ext);
int bnxt_hwrm_fw_get_time(struct bnxt_softc *softc, uint16_t *year,
    uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *minute,
    uint8_t *second, uint16_t *millisecond, uint16_t *zone);
int bnxt_hwrm_fw_set_time(struct bnxt_softc *softc, uint16_t year,
    uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
    uint16_t millisecond, uint16_t zone);

uint16_t bnxt_hwrm_get_wol_fltrs(struct bnxt_softc *softc, uint16_t handle);
int bnxt_hwrm_alloc_wol_fltr(struct bnxt_softc *softc);
int bnxt_hwrm_free_wol_fltr(struct bnxt_softc *softc);
int bnxt_hwrm_set_coal(struct bnxt_softc *softc);

#endif


struct cfattach bnxt_ca = {
	sizeof(struct bnxt_softc), bnxt_match, bnxt_attach
};

struct cfdriver bnxt_cd = {
	NULL, "bnxt", DV_IFNET
};

struct bnxt_dmamem *
bnxt_dmamem_alloc(struct bnxt_softc *sc, size_t size)
{
	struct bnxt_dmamem *m;
	int nsegs;

	m = malloc(sizeof(*m), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (m == NULL)
		return (NULL);

	m->bdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &m->bdm_map) != 0)
		goto bdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &m->bdm_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &m->bdm_seg, nsegs, size, &m->bdm_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, m->bdm_map, m->bdm_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	return (m);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, m->bdm_kva, m->bdm_size);
free:
	bus_dmamem_free(sc->sc_dmat, &m->bdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, m->bdm_map);
bdmfree:
	free(m, M_DEVBUF, sizeof *m);

	return (NULL);
}

void
bnxt_dmamem_free(struct bnxt_softc *sc, struct bnxt_dmamem *m)
{
	bus_dmamem_unmap(sc->sc_dmat, m->bdm_kva, m->bdm_size);
	bus_dmamem_free(sc->sc_dmat, &m->bdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, m->bdm_map);
	free(m, M_DEVBUF, sizeof *m);
}

int
bnxt_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, bnxt_devices, nitems(bnxt_devices)));
}

void
bnxt_attach(struct device *parent, struct device *self, void *aux)
{
	struct bnxt_softc *sc = (struct bnxt_softc *)self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr;
	u_int memtype;

	/* enable busmaster? */

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BNXT_HWRM_BAR);
	if (pci_mapreg_map(pa, BNXT_HWRM_BAR, memtype, 0, &sc->sc_hwrm_t,
	    &sc->sc_hwrm_h, NULL, &sc->sc_hwrm_s, 0)) {
		printf(": failed to map hwrm\n");
		return;
	}

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BNXT_DOORBELL_BAR);
	if (pci_mapreg_map(pa, BNXT_DOORBELL_BAR, memtype, 0, &sc->sc_db_t,
	    &sc->sc_db_h, NULL, &sc->sc_db_s, 0)) {
		printf(": failed to map doorbell\n");
		goto unmap_1;
	}

	BNXT_HWRM_LOCK_INIT(sc, DEVNAME(sc));
	sc->sc_cmd_resp = bnxt_dmamem_alloc(sc, PAGE_SIZE);
	if (sc->sc_cmd_resp == NULL) {
		printf(": failed to allocate command response buffer\n");
		goto unmap_2;
	}

	if (bnxt_hwrm_ver_get(sc) != 0) {
		printf(": failed to query version info\n");
		goto free_resp;
	}

	if (bnxt_hwrm_nvm_get_dev_info(sc, NULL, NULL, NULL, NULL, NULL, NULL)
	    != 0) {
		printf(": failed to get nvram info\n");
		goto free_resp;
	}

	if (bnxt_hwrm_func_drv_rgtr(sc) != 0) {
		printf(": failed to register driver with firmware\n");
		goto free_resp;
	}

	if (bnxt_hwrm_func_rgtr_async_events(sc) != 0) {
		printf(": failed to register async events\n");
		goto free_resp;
	}

	if (bnxt_hwrm_func_qcaps(sc) != 0) {
		printf(": failed to get queue capabilities\n");
		goto free_resp;
	}

	/*
	 * devices advertise msi support, but there's no way to tell a
	 * completion queue to use msi mode, only legacy or msi-x.
	 */
	if (/*pci_intr_map_msi(pa, &ih) != 0 && */ pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		goto free_resp;
	}
	intrstr = pci_intr_string(sc->sc_pc, ih);
	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_NET | IPL_MPSAFE,
	    bnxt_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto deintr;
	}
	printf("%s, address %s\n", intrstr, ether_sprintf(sc->sc_ac.ac_enaddr));

	if (bnxt_hwrm_func_qcfg(sc) != 0) {
		printf("%s: failed to query function config\n", DEVNAME(sc));
		goto deintr;
	}

	if (bnxt_hwrm_queue_qportcfg(sc) != 0) {
		printf("%s: failed to query port config\n", DEVNAME(sc));
		goto deintr;
	}

	if (bnxt_hwrm_func_reset(sc) != 0) {
		printf("%s: reset failed\n", DEVNAME(sc));
		goto deintr;
	}

	sc->sc_cp_ring.stats_ctx_id = HWRM_NA_SIGNATURE;
	sc->sc_cp_ring.ring.phys_id = HWRM_NA_SIGNATURE;
	sc->sc_cp_ring.softc = sc;
	sc->sc_cp_ring.ring.id = 0;
	sc->sc_cp_ring.ring.doorbell = sc->sc_cp_ring.ring.id * 0x80;
	sc->sc_cp_ring.ring.ring_size = PAGE_SIZE / sizeof(struct cmpl_base);
	sc->sc_cp_ring_mem = bnxt_dmamem_alloc(sc, PAGE_SIZE);
	if (sc->sc_cp_ring_mem == NULL) {
		printf("%s: failed to allocate completion queue memory\n",
		    DEVNAME(sc));
		goto deintr;
	}
	sc->sc_cp_ring.ring.vaddr = BNXT_DMA_KVA(sc->sc_cp_ring_mem);
	sc->sc_cp_ring.ring.paddr = BNXT_DMA_DVA(sc->sc_cp_ring_mem);
	sc->sc_cp_ring.cons = UINT32_MAX;
	sc->sc_cp_ring.v_bit = 1;
	bnxt_mark_cpr_invalid(&sc->sc_cp_ring);
	if (bnxt_hwrm_ring_alloc(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
	    &sc->sc_cp_ring.ring, (uint16_t)HWRM_NA_SIGNATURE,
	    HWRM_NA_SIGNATURE, 1) != 0) {
		printf("%s: failed to allocate completion queue\n",
		    DEVNAME(sc));
		goto free_cp_mem;
	}
	if (bnxt_cfg_async_cr(sc) != 0) {
		printf("%s: failed to set async completion ring\n",
		    DEVNAME(sc));
		goto free_cp_mem;
	}
	bnxt_write_cp_doorbell(sc, &sc->sc_cp_ring.ring, 1);

	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST | IFF_SIMPLEX;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = bnxt_ioctl;
	ifp->if_qstart = bnxt_start;
	ifp->if_watchdog = bnxt_watchdog;
	ifp->if_hardmtu = BNXT_MAX_MTU;
	ifp->if_capabilities = IFCAP_VLAN_MTU;	 /* ? */
	/* checksum flags, hwtagging? */
	IFQ_SET_MAXLEN(&ifp->if_snd, 1024);	/* ? */

	ifmedia_init(&sc->sc_media, IFM_IMASK, bnxt_media_change,
	    bnxt_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->sc_rx_refill, bnxt_refill, sc);

	bnxt_hwrm_port_phy_qcfg(sc, NULL);
	return;

free_cp_mem:
	bnxt_dmamem_free(sc, sc->sc_cp_ring_mem);
deintr:
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	sc->sc_ih = NULL;
free_resp:
	bnxt_dmamem_free(sc, sc->sc_cmd_resp);
unmap_2:
	bus_space_unmap(sc->sc_hwrm_t, sc->sc_hwrm_h, sc->sc_hwrm_s);
	sc->sc_hwrm_s = 0;
unmap_1:
	bus_space_unmap(sc->sc_db_t, sc->sc_db_h, sc->sc_db_s);
	sc->sc_db_s = 0;
}

void
bnxt_free_slots(struct bnxt_softc *sc, struct bnxt_slot *slots, int allocated,
    int total)
{
	struct bnxt_slot *bs;

	int i = allocated;
	while (i-- > 0) {
		bs = &slots[i];
		bus_dmamap_destroy(sc->sc_dmat, bs->bs_map);
	}
	free(slots, M_DEVBUF, total * sizeof(*bs));
}

void
bnxt_up(struct bnxt_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct bnxt_slot *bs;
	int i;

	sc->sc_stats_ctx_mem = bnxt_dmamem_alloc(sc,
	    sizeof(struct ctx_hw_stats));
	if (sc->sc_stats_ctx_mem == NULL) {
		printf("%s: failed to allocate stats contexts\n", DEVNAME(sc));
		return;
	}

	sc->sc_tx_ring_mem = bnxt_dmamem_alloc(sc, PAGE_SIZE);
	if (sc->sc_tx_ring_mem == NULL) {
		printf("%s: failed to allocate tx ring\n", DEVNAME(sc));
		goto free_stats;
	}

	sc->sc_rx_ring_mem = bnxt_dmamem_alloc(sc, PAGE_SIZE * 2);
	if (sc->sc_rx_ring_mem == NULL) {
		printf("%s: failed to allocate rx ring\n", DEVNAME(sc));
		goto free_tx;
	}

	if (bnxt_hwrm_stat_ctx_alloc(sc, &sc->sc_cp_ring,
	    BNXT_DMA_DVA(sc->sc_stats_ctx_mem)) != 0) {
		printf("%s: failed to set up stats context\n", DEVNAME(sc));
		goto free_rx;
	}

	sc->sc_tx_ring.phys_id = HWRM_NA_SIGNATURE;
	sc->sc_tx_ring.id = BNXT_TX_RING_ID;
	sc->sc_tx_ring.doorbell = sc->sc_tx_ring.id * 0x80;
	sc->sc_tx_ring.ring_size = PAGE_SIZE / sizeof(struct tx_bd_short);
	sc->sc_tx_ring.vaddr = BNXT_DMA_KVA(sc->sc_tx_ring_mem);
	sc->sc_tx_ring.paddr = BNXT_DMA_DVA(sc->sc_tx_ring_mem);
	if (bnxt_hwrm_ring_alloc(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
	    &sc->sc_tx_ring, sc->sc_cp_ring.ring.phys_id,
	    HWRM_NA_SIGNATURE, 1) != 0) {
		printf("%s: failed to set up tx ring\n",
		    DEVNAME(sc));
		goto dealloc_stats;
	}
	bnxt_write_tx_doorbell(sc, &sc->sc_tx_ring, 0);

	sc->sc_rx_ring.phys_id = HWRM_NA_SIGNATURE;
	sc->sc_rx_ring.id = BNXT_RX_RING_ID;
	sc->sc_rx_ring.doorbell = sc->sc_rx_ring.id * 0x80;
	sc->sc_rx_ring.ring_size = PAGE_SIZE / sizeof(struct rx_prod_pkt_bd);
	sc->sc_rx_ring.vaddr = BNXT_DMA_KVA(sc->sc_rx_ring_mem);
	sc->sc_rx_ring.paddr = BNXT_DMA_DVA(sc->sc_rx_ring_mem);
	if (bnxt_hwrm_ring_alloc(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_RX,
	    &sc->sc_rx_ring, sc->sc_cp_ring.ring.phys_id,
	    HWRM_NA_SIGNATURE, 1) != 0) {
		printf("%s: failed to set up rx ring\n",
		    DEVNAME(sc));
		goto dealloc_tx;
	}
	bnxt_write_rx_doorbell(sc, &sc->sc_rx_ring, 0);

#if 0
	sc->sc_rx_ag_ring.phys_id = HWRM_NA_SIGNATURE;
	sc->sc_rx_ag_ring.id = BNXT_AG_RING_ID;
	sc->sc_rx_ag_ring.doorbell = sc->sc_rx_ring.id * 0x80;
	sc->sc_rx_ag_ring.ring_size = PAGE_SIZE / sizeof(struct rx_prod_pkt_bd);
	sc->sc_rx_ag_ring.vaddr = BNXT_DMA_KVA(sc->sc_rx_ring_mem) + PAGE_SIZE;
	sc->sc_rx_ag_ring.paddr = BNXT_DMA_DVA(sc->sc_rx_ring_mem) + PAGE_SIZE;
	if (bnxt_hwrm_ring_alloc(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_RX,
	    &sc->sc_rx_ag_ring, sc->sc_cp_ring.ring.phys_id,
	    HWRM_NA_SIGNATURE, 1) != 0) {
		printf("%s: failed to set up rx ag ring\n",
		    DEVNAME(sc));
		goto dealloc_rx;
	}
#endif

	sc->sc_ring_group.grp_id = HWRM_NA_SIGNATURE;
	sc->sc_ring_group.stats_ctx = sc->sc_cp_ring.stats_ctx_id;
	sc->sc_ring_group.rx_ring_id = sc->sc_rx_ring.phys_id;
	sc->sc_ring_group.ag_ring_id = HWRM_NA_SIGNATURE;
	sc->sc_ring_group.cp_ring_id = sc->sc_cp_ring.ring.phys_id;
	if (bnxt_hwrm_ring_grp_alloc(sc, &sc->sc_ring_group) != 0) {
		printf("%s: failed to allocate ring group\n",
		    DEVNAME(sc));
		goto dealloc_rx;
	}

	sc->sc_vnic.rss_id = HWRM_NA_SIGNATURE;
	if (bnxt_hwrm_vnic_ctx_alloc(sc, &sc->sc_vnic.rss_id) != 0) {
		printf("%s: failed to allocate vnic rss context\n",
		    DEVNAME(sc));
		goto dealloc_ring_group;
	}

	sc->sc_vnic.id = HWRM_NA_SIGNATURE;
	sc->sc_vnic.def_ring_grp = sc->sc_ring_group.grp_id;
	sc->sc_vnic.mru = MCLBYTES;
	sc->sc_vnic.cos_rule = (uint16_t)HWRM_NA_SIGNATURE;
	sc->sc_vnic.lb_rule = (uint16_t)HWRM_NA_SIGNATURE;
	sc->sc_vnic.rx_mask = HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST;
	/* sc->sc_vnic.mc_list_count = 0; */
	sc->sc_vnic.flags = BNXT_VNIC_FLAG_DEFAULT;
	if (bnxt_hwrm_vnic_alloc(sc, &sc->sc_vnic) != 0) {
		printf("%s: failed to allocate vnic\n", DEVNAME(sc));
		goto dealloc_vnic_ctx;
	}

	if (bnxt_hwrm_vnic_cfg(sc, &sc->sc_vnic) != 0) {
		printf("%s: failed to configure vnic\n", DEVNAME(sc));
		goto dealloc_vnic;
	}

	sc->sc_vnic.filter_id = -1;
	if (bnxt_hwrm_set_filter(sc, &sc->sc_vnic) != 0) {
		printf("%s: failed to set vnic filter\n", DEVNAME(sc));
		goto dealloc_vnic;
	}

	/* don't configure rss or tpa yet */

	sc->sc_rx_slots = mallocarray(sizeof(*bs), sc->sc_rx_ring.ring_size,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc->sc_rx_slots == NULL) {
		printf("%s: failed to allocate rx slots\n", DEVNAME(sc));
		goto dealloc_filter;
	}

	for (i = 0; i < sc->sc_rx_ring.ring_size; i++) {
		bs = &sc->sc_rx_slots[i];
		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &bs->bs_map) != 0) {
			printf("%s: failed to allocate rx dma maps\n",
			    DEVNAME(sc));
			goto destroy_rx_slots;
		}
	}

	sc->sc_tx_slots = mallocarray(sizeof(*bs), sc->sc_tx_ring.ring_size,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc->sc_tx_slots == NULL) {
		printf("%s: failed to allocate tx slots\n", DEVNAME(sc));
		goto destroy_rx_slots;
	}

	for (i = 0; i < sc->sc_tx_ring.ring_size; i++) {
		bs = &sc->sc_tx_slots[i];
		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, BNXT_MAX_TX_SEGS,
		    MCLBYTES, 0, BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
		    &bs->bs_map) != 0) {
			printf("%s: failed to allocate tx dma maps\n",
			    DEVNAME(sc));
			goto destroy_tx_slots;
		}
	}

	bnxt_iff(sc);

	/*
	 * initially, the rx ring must be filled at least some distance beyond
	 * the current consumer index, as it looks like the firmware assumes the
	 * ring is full on creation, but doesn't prefetch the whole thing.
	 * once the whole ring has been used once, we should be able to back off
	 * to 2 or so slots, but we currently don't have a way of doing that.
	 */
	if_rxr_init(&sc->sc_rxr, 32, sc->sc_rx_ring.ring_size - 1);
	sc->sc_rx_prod = 0;
	sc->sc_rx_cons = 0;
	bnxt_rx_fill(sc);

	SET(ifp->if_flags, IFF_RUNNING);

	sc->sc_tx_cons = 0;
	sc->sc_tx_prod = 0;
	sc->sc_tx_ring_cons = 0;
	sc->sc_tx_ring_prod = 0;
	ifq_clr_oactive(&ifp->if_snd);
	ifq_restart(&ifp->if_snd);

	return;

destroy_tx_slots:
	bnxt_free_slots(sc, sc->sc_tx_slots, i, sc->sc_tx_ring.ring_size);
	sc->sc_tx_slots = NULL;

	i = sc->sc_rx_ring.ring_size;
destroy_rx_slots:
	bnxt_free_slots(sc, sc->sc_rx_slots, i, sc->sc_rx_ring.ring_size);
	sc->sc_rx_slots = NULL;
dealloc_filter:
	bnxt_hwrm_free_filter(sc, &sc->sc_vnic);
dealloc_vnic:
	bnxt_hwrm_vnic_free(sc, &sc->sc_vnic);
dealloc_vnic_ctx:
	bnxt_hwrm_vnic_ctx_free(sc, &sc->sc_vnic.rss_id);
dealloc_ring_group:
	bnxt_hwrm_ring_grp_free(sc, &sc->sc_ring_group);
/* dealloc_ag: */
dealloc_tx:
	bnxt_hwrm_ring_free(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
	    &sc->sc_tx_ring);
dealloc_rx:
	bnxt_hwrm_ring_free(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_RX,
	    &sc->sc_rx_ring);
dealloc_stats:
	bnxt_hwrm_stat_ctx_free(sc, &sc->sc_cp_ring);
free_rx:
	bnxt_dmamem_free(sc, sc->sc_rx_ring_mem);
	sc->sc_rx_ring_mem = NULL;
free_tx:
	bnxt_dmamem_free(sc, sc->sc_tx_ring_mem);
	sc->sc_tx_ring_mem = NULL;
free_stats:
	bnxt_dmamem_free(sc, sc->sc_stats_ctx_mem);
	sc->sc_stats_ctx_mem = NULL;
}

void
bnxt_down(struct bnxt_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	
	CLR(ifp->if_flags, IFF_RUNNING);

	ifq_clr_oactive(&ifp->if_snd);
	ifq_barrier(&ifp->if_snd);

	timeout_del(&sc->sc_rx_refill);

	/* empty rx ring first i guess */

	bnxt_free_slots(sc, sc->sc_tx_slots, sc->sc_tx_ring.ring_size,
	    sc->sc_tx_ring.ring_size);
	sc->sc_tx_slots = NULL;

	bnxt_free_slots(sc, sc->sc_rx_slots, sc->sc_rx_ring.ring_size,
	    sc->sc_rx_ring.ring_size);
	sc->sc_rx_slots = NULL;

	bnxt_hwrm_free_filter(sc, &sc->sc_vnic);
	bnxt_hwrm_vnic_free(sc, &sc->sc_vnic);
	bnxt_hwrm_vnic_ctx_free(sc, &sc->sc_vnic.rss_id);
	bnxt_hwrm_ring_grp_free(sc, &sc->sc_ring_group);
	bnxt_hwrm_stat_ctx_free(sc, &sc->sc_cp_ring);

	/* may need to wait for 500ms here before we can free the rings */

	bnxt_hwrm_ring_free(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
	    &sc->sc_tx_ring);
	bnxt_hwrm_ring_free(sc, HWRM_RING_ALLOC_INPUT_RING_TYPE_RX,
	    &sc->sc_rx_ring);

	bnxt_dmamem_free(sc, sc->sc_rx_ring_mem);
	sc->sc_rx_ring_mem = NULL;

	bnxt_dmamem_free(sc, sc->sc_tx_ring_mem);
	sc->sc_tx_ring_mem = NULL;

	bnxt_dmamem_free(sc, sc->sc_stats_ctx_mem);
	sc->sc_stats_ctx_mem = NULL;
}

void
bnxt_iff(struct bnxt_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (ifp->if_flags & IFF_ALLMULTI)
		sc->sc_vnic.rx_mask |=
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;
	else
		sc->sc_vnic.rx_mask &=
		    ~HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;

	if (ifp->if_flags & IFF_PROMISC)
		sc->sc_vnic.rx_mask |=
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ANYVLAN_NONVLAN;
	else
		sc->sc_vnic.rx_mask &=
		    ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ANYVLAN_NONVLAN);

	bnxt_hwrm_cfa_l2_set_rx_mask(sc, &sc->sc_vnic);
}

int
bnxt_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bnxt_softc 	*sc = (struct bnxt_softc *)ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	int			s, error = 0;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				bnxt_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				bnxt_down(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCGIFRXR:
		error = bnxt_rxrinfo(sc, (struct if_rxrinfo *)ifr->ifr_data);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			bnxt_iff(sc);
		error = 0;
	}

	splx(s);

	return (error);
}

int
bnxt_rxrinfo(struct bnxt_softc *sc, struct if_rxrinfo *ifri)
{
	struct if_rxring_info ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_size = MCLBYTES;
	ifr.ifr_info = sc->sc_rxr;

	return (if_rxr_info_ioctl(ifri, 1, &ifr));
}

int
bnxt_load_mbuf(struct bnxt_softc *sc, struct bnxt_slot *bs, struct mbuf *m)
{
	switch (bus_dmamap_load_mbuf(sc->sc_dmat, bs->bs_map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT)) {
	case 0:
		break;

	case EFBIG:
		if (m_defrag(m, M_DONTWAIT) == 0 &&
		    bus_dmamap_load_mbuf(sc->sc_dmat, bs->bs_map, m,
		    BUS_DMA_STREAMING | BUS_DMA_NOWAIT) == 0)
			break;

	default:
		return (1);
	}

	bs->bs_m = m;
	return (0);
}

void
bnxt_start(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	struct tx_bd_short *txring;
	struct bnxt_softc *sc = ifp->if_softc;
	struct bnxt_slot *bs;
	bus_dmamap_t map;
	struct mbuf *m;
	u_int idx, free, used;
	uint16_t txflags;
	int i;

	txring = (struct tx_bd_short *)BNXT_DMA_KVA(sc->sc_tx_ring_mem);

	idx = sc->sc_tx_ring_prod;
	free = sc->sc_tx_ring_cons;
	if (free <= idx)
		free += sc->sc_tx_ring.ring_size;
	free -= idx;

	used = 0;

	for (;;) {
		if (used + BNXT_MAX_TX_SEGS > free) {
			ifq_set_oactive(ifq);
			break;
		}

		m = ifq_dequeue(ifq);
		if (m == NULL)
			break;

		bs = &sc->sc_tx_slots[sc->sc_tx_prod];
		if (bnxt_load_mbuf(sc, bs, m) != 0) {
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		map = bs->bs_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);
		used += map->dm_nsegs;

		if (map->dm_mapsize < 512)
			txflags = TX_BD_SHORT_FLAGS_LHINT_LT512;
		else if (map->dm_mapsize < 1024)
			txflags = TX_BD_SHORT_FLAGS_LHINT_LT1K;
		else if (map->dm_mapsize < 2048)
			txflags = TX_BD_SHORT_FLAGS_LHINT_LT2K;
		else
			txflags = TX_BD_SHORT_FLAGS_LHINT_GTE2K;

		txflags |= TX_BD_SHORT_TYPE_TX_BD_SHORT |
		    (map->dm_nsegs << TX_BD_SHORT_FLAGS_BD_CNT_SFT);

		for (i = 0; i < map->dm_nsegs; i++) {
			txring[idx].flags_type = htole16(txflags);
			if (i == map->dm_nsegs - 1)
				txring[idx].flags_type |=
				    TX_BD_SHORT_FLAGS_PACKET_END;
			txflags = TX_BD_SHORT_TYPE_TX_BD_SHORT;

			txring[idx].len =
			    htole16(bs->bs_map->dm_segs[i].ds_len);
			txring[idx].opaque = sc->sc_tx_prod;
			txring[idx].addr =
			    htole64(bs->bs_map->dm_segs[i].ds_addr);

			idx++;
			if (idx == sc->sc_tx_ring.ring_size)
				idx = 0;
		}

		if (++sc->sc_tx_prod >= sc->sc_tx_ring.ring_size)
			sc->sc_tx_prod = 0;
	}

	bnxt_write_tx_doorbell(sc, &sc->sc_tx_ring, idx);
	sc->sc_tx_ring_prod = idx;
}

void
bnxt_handle_async_event(struct bnxt_softc *sc, struct cmpl_base *cmpl)
{
	struct hwrm_async_event_cmpl *ae = (struct hwrm_async_event_cmpl *)cmpl;
	uint16_t type = le16toh(ae->event_id);

	switch (type) {
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE:
		bnxt_hwrm_port_phy_qcfg(sc, NULL);
		break;

	default:
		printf("%s: unexpected async event %x\n", DEVNAME(sc), type);
		break;
	}
}

int
bnxt_intr(void *xsc)
{
	struct bnxt_softc *sc = (struct bnxt_softc *)xsc;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct bnxt_cp_ring *cpr = &sc->sc_cp_ring;
	struct cmpl_base *cmpl, *cmpl2;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	uint32_t cons, last_cons;
	int v_bit, last_v_bit;
	uint16_t type;
	int rxfree, txfree;

	cons = cpr->cons;
	v_bit = cpr->v_bit;

	bnxt_write_cp_doorbell(sc, &cpr->ring, 0);
	rxfree = 0;
	txfree = 0;
	for (;;) {
		last_cons = cons;
		last_v_bit = v_bit;
		NEXT_CP_CONS_V(&cpr->ring, cons, v_bit);
		cmpl = &((struct cmpl_base *)cpr->ring.vaddr)[cons];

		if ((!!(cmpl->info3_v & htole32(CMPL_BASE_V))) != (!!v_bit))
			break;

		type = le16toh(cmpl->type) & CMPL_BASE_TYPE_MASK;
		switch (type) {
		case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
			bnxt_handle_async_event(sc, cmpl);
			break;
		case CMPL_BASE_TYPE_RX_L2:
			/* rx takes two slots */
			last_cons = cons;
			last_v_bit = v_bit;
			NEXT_CP_CONS_V(&cpr->ring, cons, v_bit);
			cmpl2 = &((struct cmpl_base *)cpr->ring.vaddr)[cons];
			bnxt_rx(sc, &ml, &rxfree, cmpl, cmpl2);
			break;
		case CMPL_BASE_TYPE_TX_L2:
			txfree += bnxt_txeof(sc, cmpl);
			break;
		default:
			printf("%s: unexpected completion type %u\n",
			    DEVNAME(sc), type);
		}
	}

	cpr->cons = last_cons;
	cpr->v_bit = last_v_bit;

	/*
	 * comments in bnxtreg.h suggest we should be writing cpr->cons here,
	 * but writing cpr->cons + 1 makes it stop interrupting.
	 */
	bnxt_write_cp_doorbell_index(sc, &cpr->ring,
	    (cpr->cons+1) % cpr->ring.ring_size, 1);

	if (rxfree != 0) {
		sc->sc_rx_cons += rxfree;
		if (sc->sc_rx_cons >= sc->sc_rx_ring.ring_size)
			sc->sc_rx_cons -= sc->sc_rx_ring.ring_size;

		if_rxr_put(&sc->sc_rxr, rxfree);

		bnxt_rx_fill(sc);
		if (sc->sc_rx_cons == sc->sc_rx_prod)
			timeout_add(&sc->sc_rx_refill, 0);

		if_input(&sc->sc_ac.ac_if, &ml);
	}
	if (txfree != 0) {
		if (ifq_is_oactive(&ifp->if_snd))
			ifq_restart(&ifp->if_snd);
	}
	return (1);
}

void
bnxt_watchdog(struct ifnet *ifp)
{
}

void
bnxt_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct bnxt_softc *sc = (struct bnxt_softc *)ifp->if_softc;
	bnxt_hwrm_port_phy_qcfg(sc, ifmr);
}

int
bnxt_media_change(struct ifnet *ifp)
{
	return 0;
}

void
bnxt_mark_cpr_invalid(struct bnxt_cp_ring *cpr)
{
	struct cmpl_base *cmp = (void *)cpr->ring.vaddr;
	int i;

	for (i = 0; i < cpr->ring.ring_size; i++)
		cmp[i].info3_v = !cpr->v_bit;
}

void
bnxt_write_cp_doorbell(struct bnxt_softc *sc, struct bnxt_ring *ring,
    int enable)
{
	uint32_t val = CMPL_DOORBELL_KEY_CMPL;
	if (enable == 0)
		val |= CMPL_DOORBELL_MASK;

	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, ring->doorbell, 4,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, 0, sc->sc_db_s,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(sc->sc_db_t, sc->sc_db_h, ring->doorbell,
	    htole32(val));
}

void
bnxt_write_cp_doorbell_index(struct bnxt_softc *sc, struct bnxt_ring *ring,
    uint32_t index, int enable)
{
	uint32_t val = CMPL_DOORBELL_KEY_CMPL | CMPL_DOORBELL_IDX_VALID |
	    (index & CMPL_DOORBELL_IDX_MASK);
	if (enable == 0)
		val |= CMPL_DOORBELL_MASK;
	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, ring->doorbell, 4,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(sc->sc_db_t, sc->sc_db_h, ring->doorbell,
	    htole32(val));
	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, 0, sc->sc_db_s,
	    BUS_SPACE_BARRIER_WRITE);
}

void
bnxt_write_rx_doorbell(struct bnxt_softc *sc, struct bnxt_ring *ring, int index)
{
	uint32_t val = RX_DOORBELL_KEY_RX | index;
	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, ring->doorbell, 4,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(sc->sc_db_t, sc->sc_db_h, ring->doorbell,
	    htole32(val));

	/* second write isn't necessary on all hardware */
	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, ring->doorbell, 4,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(sc->sc_db_t, sc->sc_db_h, ring->doorbell,
	    htole32(val));
}

void
bnxt_write_tx_doorbell(struct bnxt_softc *sc, struct bnxt_ring *ring, int index)
{
	uint32_t val = TX_DOORBELL_KEY_TX | index;
	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, ring->doorbell, 4,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(sc->sc_db_t, sc->sc_db_h, ring->doorbell,
	    htole32(val));

	/* second write isn't necessary on all hardware */
	bus_space_barrier(sc->sc_db_t, sc->sc_db_h, ring->doorbell, 4,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_write_4(sc->sc_db_t, sc->sc_db_h, ring->doorbell,
	    htole32(val));
}

u_int
bnxt_rx_fill_slots(struct bnxt_softc *sc, u_int slots)
{
	struct rx_prod_pkt_bd *rxring;
	struct bnxt_slot *bs;
	struct mbuf *m;
	uint p, fills;
	uint16_t type;

	type = RX_PROD_PKT_BD_TYPE_RX_PROD_PKT;

	rxring = (struct rx_prod_pkt_bd *)BNXT_DMA_KVA(sc->sc_rx_ring_mem);
	p = sc->sc_rx_prod;
	for (fills = 0; fills < slots; fills++) {
		bs = &sc->sc_rx_slots[p];
		m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES);
		if (m == NULL)
			break;

		m->m_len = m->m_pkthdr.len = MCLBYTES;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, bs->bs_map, m,
		    BUS_DMA_NOWAIT) != 0) {
			m_freem(m);
			break;
		}
		bs->bs_m = m;

		rxring[p].flags_type = htole16(type);
		rxring[p].len = htole16(MCLBYTES);
		rxring[p].opaque = p;
		rxring[p].addr = htole64(bs->bs_map->dm_segs[0].ds_addr);

		if (++p >= sc->sc_rx_ring.ring_size)
			p = 0;
	}

	if (fills != 0)
		bnxt_write_rx_doorbell(sc, &sc->sc_rx_ring, p);
	sc->sc_rx_prod = p;

	return (slots - fills);
}

int
bnxt_rx_fill(struct bnxt_softc *sc)
{
	u_int slots;

	slots = if_rxr_get(&sc->sc_rxr, sc->sc_rx_ring.ring_size);
	if (slots == 0)
		return (1);

	slots = bnxt_rx_fill_slots(sc, slots);
	if_rxr_put(&sc->sc_rxr, slots);

	return (0);
}

void
bnxt_refill(void *xsc)
{
	struct bnxt_softc *sc = xsc;

	bnxt_rx_fill(sc);

	if (sc->sc_rx_cons == sc->sc_rx_prod)
		timeout_add(&sc->sc_rx_refill, 1);
}

void
bnxt_rx(struct bnxt_softc *sc, struct mbuf_list *ml, int *slots,
    struct cmpl_base *cmpl, struct cmpl_base *cmpl2)
{
	struct mbuf *m;
	struct bnxt_slot *bs;
	struct rx_pkt_cmpl *rx = (struct rx_pkt_cmpl *)cmpl;
	/* struct rx_pkt_cmpl_hi *rxhi = (struct rx_pkt_cmpl_hi *)cmpl2; */

	bs = &sc->sc_rx_slots[rx->opaque];

	bus_dmamap_sync(sc->sc_dmat, bs->bs_map, 0, bs->bs_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, bs->bs_map);

	m = bs->bs_m;
	bs->bs_m = NULL;
	m->m_pkthdr.len = m->m_len = letoh16(rx->len);
	ml_enqueue(ml, m);
	(*slots)++;
}

int
bnxt_txeof(struct bnxt_softc *sc, struct cmpl_base *cmpl)
{
	struct tx_cmpl *txcmpl = (struct tx_cmpl *)cmpl;
	struct bnxt_slot *bs;
	bus_dmamap_t map;
	u_int idx, freed;

	if (txcmpl->opaque != sc->sc_tx_cons)
		printf("%s: txeof for %d, expected %d?\n",
		    DEVNAME(sc), txcmpl->opaque, sc->sc_tx_cons);
	idx = sc->sc_tx_ring_cons;

	bs = &sc->sc_tx_slots[sc->sc_tx_cons];
	map = bs->bs_map;

	freed = map->dm_nsegs;
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, map);
	m_freem(bs->bs_m);
	bs->bs_m = NULL;

	idx += freed;
	if (idx >= sc->sc_tx_ring.ring_size)
		idx -= sc->sc_tx_ring.ring_size;
	sc->sc_tx_ring_cons = idx;

	if (++sc->sc_tx_cons >= sc->sc_tx_ring.ring_size)
		sc->sc_tx_cons = 0;

	return (freed);
}

/* bnxt_hwrm.c */

static int bnxt_hwrm_err_map(uint16_t err);
#if 0
static void	bnxt_hwrm_set_link_common(struct bnxt_softc *softc,
		    struct hwrm_port_phy_cfg_input *req);
static void	bnxt_hwrm_set_pause_common(struct bnxt_softc *softc,
		    struct hwrm_port_phy_cfg_input *req);
static void	bnxt_hwrm_set_eee(struct bnxt_softc *softc,
		    struct hwrm_port_phy_cfg_input *req);
#endif

static int	_hwrm_send_message(struct bnxt_softc *, void *, uint32_t);
static int	hwrm_send_message(struct bnxt_softc *, void *, uint32_t);
static void bnxt_hwrm_cmd_hdr_init(struct bnxt_softc *, void *, uint16_t);

/* NVRam stuff has a five minute timeout */
#define BNXT_NVM_TIMEO	(5 * 60 * 1000)

static int
bnxt_hwrm_err_map(uint16_t err)
{
	int rc;

	switch (err) {
	case HWRM_ERR_CODE_SUCCESS:
		return 0;
	case HWRM_ERR_CODE_INVALID_PARAMS:
	case HWRM_ERR_CODE_INVALID_FLAGS:
	case HWRM_ERR_CODE_INVALID_ENABLES:
		return EINVAL;
	case HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED:
		return EACCES;
	case HWRM_ERR_CODE_RESOURCE_ALLOC_ERROR:
		return ENOMEM;
	case HWRM_ERR_CODE_CMD_NOT_SUPPORTED:
		return ENOSYS;
	case HWRM_ERR_CODE_FAIL:
		return EIO;
	case HWRM_ERR_CODE_HWRM_ERROR:
	case HWRM_ERR_CODE_UNKNOWN_ERR:
	default:
		return EIO;
	}

	return rc;
}

static void
bnxt_hwrm_cmd_hdr_init(struct bnxt_softc *softc, void *request,
    uint16_t req_type)
{
	struct input *req = request;

	req->req_type = htole16(req_type);
	req->cmpl_ring = 0xffff;
	req->target_id = 0xffff;
	req->resp_addr = htole64(BNXT_DMA_DVA(softc->sc_cmd_resp));
}

static int
_hwrm_send_message(struct bnxt_softc *softc, void *msg, uint32_t msg_len)
{
	struct input *req = msg;
	struct hwrm_err_output *resp = BNXT_DMA_KVA(softc->sc_cmd_resp);
	uint32_t *data = msg;
	int i;
	uint16_t cp_ring_id;
	uint8_t *valid;
	uint16_t err;
	uint16_t max_req_len = HWRM_MAX_REQ_LEN;
	struct hwrm_short_input short_input = {0};

	/* TODO: DMASYNC in here. */
	req->seq_id = htole16(softc->sc_cmd_seq++);
	memset(resp, 0, PAGE_SIZE);
	cp_ring_id = le16toh(req->cmpl_ring);

	if (softc->sc_flags & BNXT_FLAG_SHORT_CMD) {
		void *short_cmd_req = BNXT_DMA_KVA(softc->sc_cmd_resp);

		memcpy(short_cmd_req, req, msg_len);
		memset((uint8_t *) short_cmd_req + msg_len, 0,
		    softc->sc_max_req_len - msg_len);

		short_input.req_type = req->req_type;
		short_input.signature =
		    htole16(HWRM_SHORT_INPUT_SIGNATURE_SHORT_CMD);
		short_input.size = htole16(msg_len);
		short_input.req_addr =
		    htole64(BNXT_DMA_DVA(softc->sc_cmd_resp));

		data = (uint32_t *)&short_input;
		msg_len = sizeof(short_input);

		/* Sync memory write before updating doorbell */
		membar_sync();

		max_req_len = BNXT_HWRM_SHORT_REQ_LEN;
	}

	/* Write request msg to hwrm channel */
	for (i = 0; i < msg_len; i += 4) {
		bus_space_write_4(softc->sc_hwrm_t,
				  softc->sc_hwrm_h,
				  i, *data);
		data++;
	}

	/* Clear to the end of the request buffer */
	for (i = msg_len; i < max_req_len; i += 4)
		bus_space_write_4(softc->sc_hwrm_t, softc->sc_hwrm_h,
		    i, 0);

	/* Ring channel doorbell */
	bus_space_write_4(softc->sc_hwrm_t, softc->sc_hwrm_h, 0x100,
	    htole32(1));

	/* Check if response len is updated */
	for (i = 0; i < softc->sc_cmd_timeo; i++) {
		if (resp->resp_len && resp->resp_len <= 4096)
			break;
		DELAY(1000);
	}
	if (i >= softc->sc_cmd_timeo) {
		printf("%s: timeout sending %s: (timeout: %u) seq: %d\n",
		    DEVNAME(softc), GET_HWRM_REQ_TYPE(req->req_type),
		    softc->sc_cmd_timeo,
		    le16toh(req->seq_id));
		return ETIMEDOUT;
	}
	/* Last byte of resp contains the valid key */
	valid = (uint8_t *)resp + resp->resp_len - 1;
	for (i = 0; i < softc->sc_cmd_timeo; i++) {
		if (*valid == HWRM_RESP_VALID_KEY)
			break;
		DELAY(1000);
	}
	if (i >= softc->sc_cmd_timeo) {
		printf("%s: timeout sending %s: "
		    "(timeout: %u) msg {0x%x 0x%x} len:%d v: %d\n",
		    DEVNAME(softc), GET_HWRM_REQ_TYPE(req->req_type),
		    softc->sc_cmd_timeo, le16toh(req->req_type),
		    le16toh(req->seq_id), msg_len,
		    *valid);
		return ETIMEDOUT;
	}

	err = le16toh(resp->error_code);
	if (err) {
		/* HWRM_ERR_CODE_FAIL is a "normal" error, don't log */
		if (err != HWRM_ERR_CODE_FAIL) {
			printf("%s: %s command returned %s error.\n",
			    DEVNAME(softc),
			    GET_HWRM_REQ_TYPE(req->req_type),
			    GET_HWRM_ERROR_CODE(err));
		}
		return bnxt_hwrm_err_map(err);
	}

	return 0;
}


static int
hwrm_send_message(struct bnxt_softc *softc, void *msg, uint32_t msg_len)
{
	int rc;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, msg, msg_len);
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}


int
bnxt_hwrm_queue_qportcfg(struct bnxt_softc *softc)
{
	struct hwrm_queue_qportcfg_input req = {0};
	struct hwrm_queue_qportcfg_output *resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);

	int	rc = 0;
	uint8_t	*qptr;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_QPORTCFG);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto qportcfg_exit;

	if (!resp->max_configurable_queues) {
		rc = -EINVAL;
		goto qportcfg_exit;
	}
	softc->sc_max_tc = resp->max_configurable_queues;
	if (softc->sc_max_tc > BNXT_MAX_QUEUE)
		softc->sc_max_tc = BNXT_MAX_QUEUE;

	qptr = &resp->queue_id0;
	for (int i = 0; i < softc->sc_max_tc; i++) {
		softc->sc_q_info[i].id = *qptr++;
		softc->sc_q_info[i].profile = *qptr++;
	}

qportcfg_exit:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_ver_get(struct bnxt_softc *softc)
{
	struct hwrm_ver_get_input	req = {0};
	struct hwrm_ver_get_output	*resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);
	int				rc;
#if 0
	const char nastr[] = "<not installed>";
	const char naver[] = "<N/A>";
#endif
	uint32_t dev_caps_cfg;

	softc->sc_max_req_len = HWRM_MAX_REQ_LEN;
	softc->sc_cmd_timeo = 1000;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VER_GET);

	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	printf(": fw ver %d.%d.%d, ", resp->hwrm_fw_maj, resp->hwrm_fw_min,
	    resp->hwrm_fw_bld);
#if 0
	snprintf(softc->ver_info->hwrm_if_ver, BNXT_VERSTR_SIZE, "%d.%d.%d",
	    resp->hwrm_intf_maj, resp->hwrm_intf_min, resp->hwrm_intf_upd);
	softc->ver_info->hwrm_if_major = resp->hwrm_intf_maj;
	softc->ver_info->hwrm_if_minor = resp->hwrm_intf_min;
	softc->ver_info->hwrm_if_update = resp->hwrm_intf_upd;
	snprintf(softc->ver_info->hwrm_fw_ver, BNXT_VERSTR_SIZE, "%d.%d.%d",
	    resp->hwrm_fw_maj, resp->hwrm_fw_min, resp->hwrm_fw_bld);
	strlcpy(softc->ver_info->driver_hwrm_if_ver, HWRM_VERSION_STR,
	    BNXT_VERSTR_SIZE);
	strlcpy(softc->ver_info->hwrm_fw_name, resp->hwrm_fw_name,
	    BNXT_NAME_SIZE);

	if (resp->mgmt_fw_maj == 0 && resp->mgmt_fw_min == 0 &&
	    resp->mgmt_fw_bld == 0) {
		strlcpy(softc->ver_info->mgmt_fw_ver, naver, BNXT_VERSTR_SIZE);
		strlcpy(softc->ver_info->mgmt_fw_name, nastr, BNXT_NAME_SIZE);
	}
	else {
		snprintf(softc->ver_info->mgmt_fw_ver, BNXT_VERSTR_SIZE,
		    "%d.%d.%d", resp->mgmt_fw_maj, resp->mgmt_fw_min,
		    resp->mgmt_fw_bld);
		strlcpy(softc->ver_info->mgmt_fw_name, resp->mgmt_fw_name,
		    BNXT_NAME_SIZE);
	}
	if (resp->netctrl_fw_maj == 0 && resp->netctrl_fw_min == 0 &&
	    resp->netctrl_fw_bld == 0) {
		strlcpy(softc->ver_info->netctrl_fw_ver, naver,
		    BNXT_VERSTR_SIZE);
		strlcpy(softc->ver_info->netctrl_fw_name, nastr,
		    BNXT_NAME_SIZE);
	}
	else {
		snprintf(softc->ver_info->netctrl_fw_ver, BNXT_VERSTR_SIZE,
		    "%d.%d.%d", resp->netctrl_fw_maj, resp->netctrl_fw_min,
		    resp->netctrl_fw_bld);
		strlcpy(softc->ver_info->netctrl_fw_name, resp->netctrl_fw_name,
		    BNXT_NAME_SIZE);
	}
	if (resp->roce_fw_maj == 0 && resp->roce_fw_min == 0 &&
	    resp->roce_fw_bld == 0) {
		strlcpy(softc->ver_info->roce_fw_ver, naver, BNXT_VERSTR_SIZE);
		strlcpy(softc->ver_info->roce_fw_name, nastr, BNXT_NAME_SIZE);
	}
	else {
		snprintf(softc->ver_info->roce_fw_ver, BNXT_VERSTR_SIZE,
		    "%d.%d.%d", resp->roce_fw_maj, resp->roce_fw_min,
		    resp->roce_fw_bld);
		strlcpy(softc->ver_info->roce_fw_name, resp->roce_fw_name,
		    BNXT_NAME_SIZE);
	}
	softc->ver_info->chip_num = le16toh(resp->chip_num);
	softc->ver_info->chip_rev = resp->chip_rev;
	softc->ver_info->chip_metal = resp->chip_metal;
	softc->ver_info->chip_bond_id = resp->chip_bond_id;
	softc->ver_info->chip_type = resp->chip_platform_type;
#endif

	if (resp->max_req_win_len)
		softc->sc_max_req_len = le16toh(resp->max_req_win_len);
	if (resp->def_req_timeout)
		softc->sc_cmd_timeo = le16toh(resp->def_req_timeout);

	dev_caps_cfg = le32toh(resp->dev_caps_cfg);
	if ((dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED) &&
	    (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_REQUIRED))
		softc->sc_flags |= BNXT_FLAG_SHORT_CMD;

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}


int
bnxt_hwrm_func_drv_rgtr(struct bnxt_softc *softc)
{
	struct hwrm_func_drv_rgtr_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_DRV_RGTR);

	req.enables = htole32(HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER |
	    HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_OS_TYPE);
	req.os_type = htole16(HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_FREEBSD);

	req.ver_maj = 6;
	req.ver_min = 4;
	req.ver_upd = 0;

	return hwrm_send_message(softc, &req, sizeof(req));
}

#if 0

int
bnxt_hwrm_func_drv_unrgtr(struct bnxt_softc *softc, bool shutdown)
{
	struct hwrm_func_drv_unrgtr_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_DRV_UNRGTR);
	if (shutdown == true)
		req.flags |=
		    HWRM_FUNC_DRV_UNRGTR_INPUT_FLAGS_PREPARE_FOR_SHUTDOWN;
	return hwrm_send_message(softc, &req, sizeof(req));
}

#endif

int
bnxt_hwrm_func_qcaps(struct bnxt_softc *softc)
{
	int rc = 0;
	struct hwrm_func_qcaps_input req = {0};
	struct hwrm_func_qcaps_output *resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);
	/* struct bnxt_func_info *func = &softc->func; */

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_QCAPS);
	req.fid = htole16(0xffff);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	if (resp->flags &
	    htole32(HWRM_FUNC_QCAPS_OUTPUT_FLAGS_WOL_MAGICPKT_SUPPORTED))
		softc->sc_flags |= BNXT_FLAG_WOL_CAP;

	memcpy(softc->sc_ac.ac_enaddr, resp->mac_address, 6);
	/*
	func->fw_fid = le16toh(resp->fid);
	memcpy(func->mac_addr, resp->mac_address, ETHER_ADDR_LEN);
	func->max_rsscos_ctxs = le16toh(resp->max_rsscos_ctx);
	func->max_cp_rings = le16toh(resp->max_cmpl_rings);
	func->max_tx_rings = le16toh(resp->max_tx_rings);
	func->max_rx_rings = le16toh(resp->max_rx_rings);
	func->max_hw_ring_grps = le32toh(resp->max_hw_ring_grps);
	if (!func->max_hw_ring_grps)
		func->max_hw_ring_grps = func->max_tx_rings;
	func->max_l2_ctxs = le16toh(resp->max_l2_ctxs);
	func->max_vnics = le16toh(resp->max_vnics);
	func->max_stat_ctxs = le16toh(resp->max_stat_ctx);
	if (BNXT_PF(softc)) {
		struct bnxt_pf_info *pf = &softc->pf;

		pf->port_id = le16toh(resp->port_id);
		pf->first_vf_id = le16toh(resp->first_vf_id);
		pf->max_vfs = le16toh(resp->max_vfs);
		pf->max_encap_records = le32toh(resp->max_encap_records);
		pf->max_decap_records = le32toh(resp->max_decap_records);
		pf->max_tx_em_flows = le32toh(resp->max_tx_em_flows);
		pf->max_tx_wm_flows = le32toh(resp->max_tx_wm_flows);
		pf->max_rx_em_flows = le32toh(resp->max_rx_em_flows);
		pf->max_rx_wm_flows = le32toh(resp->max_rx_wm_flows);
	}
	if (!_is_valid_ether_addr(func->mac_addr)) {
		device_printf(softc->dev, "Invalid ethernet address, generating random locally administered address\n");
		get_random_ether_addr(func->mac_addr);
	}
	*/

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}


int 
bnxt_hwrm_func_qcfg(struct bnxt_softc *softc)
{
        struct hwrm_func_qcfg_input req = {0};
        /* struct hwrm_func_qcfg_output *resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);
	struct bnxt_func_qcfg *fn_qcfg = &softc->fn_qcfg; */
        int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_QCFG);
        req.fid = htole16(0xffff);
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
        if (rc)
		goto fail;

	/*
	fn_qcfg->alloc_completion_rings = le16toh(resp->alloc_cmpl_rings);
	fn_qcfg->alloc_tx_rings = le16toh(resp->alloc_tx_rings);
	fn_qcfg->alloc_rx_rings = le16toh(resp->alloc_rx_rings);
	fn_qcfg->alloc_vnics = le16toh(resp->alloc_vnics);
	*/
fail:
	BNXT_HWRM_UNLOCK(softc);
        return rc;
}


int
bnxt_hwrm_func_reset(struct bnxt_softc *softc)
{
	struct hwrm_func_reset_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_RESET);
	req.enables = 0;

	return hwrm_send_message(softc, &req, sizeof(req));
}

#if 0
static void
bnxt_hwrm_set_link_common(struct bnxt_softc *softc,
    struct hwrm_port_phy_cfg_input *req)
{
	uint8_t autoneg = softc->link_info.autoneg;
	uint16_t fw_link_speed = softc->link_info.req_link_speed;

	if (autoneg & BNXT_AUTONEG_SPEED) {
		req->auto_mode |=
		    HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ALL_SPEEDS;

		req->enables |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE);
		req->flags |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESTART_AUTONEG);
	} else {
		req->force_link_speed = htole16(fw_link_speed);
		req->flags |= htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE);
	}

	/* tell chimp that the setting takes effect immediately */
	req->flags |= htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESET_PHY);
}


static void
bnxt_hwrm_set_pause_common(struct bnxt_softc *softc,
    struct hwrm_port_phy_cfg_input *req)
{
	struct bnxt_link_info *link_info = &softc->link_info;

	if (link_info->flow_ctrl.autoneg) {
		req->auto_pause =
		    HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_AUTONEG_PAUSE;
		if (link_info->flow_ctrl.rx)
			req->auto_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_RX;
		if (link_info->flow_ctrl.tx)
			req->auto_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_TX;
		req->enables |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAUSE);
	} else {
		if (link_info->flow_ctrl.rx)
			req->force_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_RX;
		if (link_info->flow_ctrl.tx)
			req->force_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_TX;
		req->enables |=
			htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAUSE);
	}
}


/* JFV this needs interface connection */
static void
bnxt_hwrm_set_eee(struct bnxt_softc *softc, struct hwrm_port_phy_cfg_input *req)
{
	/* struct ethtool_eee *eee = &softc->eee; */
	bool	eee_enabled = false;

	if (eee_enabled) {
# i f 0
		uint16_t eee_speeds;
		uint32_t flags = HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_ENABLE;

		if (eee->tx_lpi_enabled)
			flags |= HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_TX_LPI;

		req->flags |= htole32(flags);
		eee_speeds = bnxt_get_fw_auto_link_speeds(eee->advertised);
		req->eee_link_speed_mask = htole16(eee_speeds);
		req->tx_lpi_timer = htole32(eee->tx_lpi_timer);
# e n d i f
	} else {
		req->flags |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_DISABLE);
	}
}


int
bnxt_hwrm_set_link_setting(struct bnxt_softc *softc, bool set_pause,
    bool set_eee, bool set_link)
{
	struct hwrm_port_phy_cfg_input req = {0};
	int rc;

	if (softc->sc_flags & BNXT_FLAG_NPAR)
		return ENOTSUP;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_PHY_CFG);
	
	if (set_pause) {
		bnxt_hwrm_set_pause_common(softc, &req);

		if (softc->link_info.flow_ctrl.autoneg)
			set_link = true;
	}

	if (set_link)
		bnxt_hwrm_set_link_common(softc, &req);
	
	if (set_eee)
		bnxt_hwrm_set_eee(softc, &req);
	
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));

	if (!rc) {
		if (set_pause) {
			/* since changing of 'force pause' setting doesn't 
			 * trigger any link change event, the driver needs to
			 * update the current pause result upon successfully i
			 * return of the phy_cfg command */
			if (!softc->link_info.flow_ctrl.autoneg) 
				bnxt_report_link(softc);
		}
	}
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

#endif

int
bnxt_hwrm_vnic_cfg(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_cfg_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_CFG);

	if (vnic->flags & BNXT_VNIC_FLAG_DEFAULT)
		req.flags |= htole32(HWRM_VNIC_CFG_INPUT_FLAGS_DEFAULT);
	if (vnic->flags & BNXT_VNIC_FLAG_BD_STALL)
		req.flags |= htole32(HWRM_VNIC_CFG_INPUT_FLAGS_BD_STALL_MODE);
	if (vnic->flags & BNXT_VNIC_FLAG_VLAN_STRIP)
		req.flags |= htole32(HWRM_VNIC_CFG_INPUT_FLAGS_VLAN_STRIP_MODE);
	req.enables = htole32(HWRM_VNIC_CFG_INPUT_ENABLES_DFLT_RING_GRP |
	    HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE |
	    HWRM_VNIC_CFG_INPUT_ENABLES_MRU);
	req.vnic_id = htole16(vnic->id);
	req.dflt_ring_grp = htole16(vnic->def_ring_grp);
	req.rss_rule = htole16(vnic->rss_id);
	req.cos_rule = htole16(vnic->cos_rule);
	req.lb_rule = htole16(vnic->lb_rule);
	req.mru = htole16(vnic->mru);

	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_vnic_alloc(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_alloc_input req = {0};
	struct hwrm_vnic_alloc_output *resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);
	int rc;

	if (vnic->id != (uint16_t)HWRM_NA_SIGNATURE) {
		printf("%s: attempt to re-allocate vnic %04x\n",
		    DEVNAME(softc), vnic->id);
		return EINVAL;
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_ALLOC);

	if (vnic->flags & BNXT_VNIC_FLAG_DEFAULT)
		req.flags = htole32(HWRM_VNIC_ALLOC_INPUT_FLAGS_DEFAULT);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	vnic->id = le32toh(resp->vnic_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_vnic_free(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_free_input req = {0};
	int rc;

	if (vnic->id == (uint16_t)HWRM_NA_SIGNATURE) {
		printf("%s: attempt to deallocate vnic %04x\n",
		    DEVNAME(softc), vnic->id);
		return (EINVAL);
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_FREE);
	req.vnic_id = htole16(vnic->id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc == 0)
		vnic->id = (uint16_t)HWRM_NA_SIGNATURE;
	BNXT_HWRM_UNLOCK(softc);

	return (rc);
}

int
bnxt_hwrm_vnic_ctx_alloc(struct bnxt_softc *softc, uint16_t *ctx_id)
{
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_input req = {0};
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_output *resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);
	int rc;

	if (*ctx_id != (uint16_t)HWRM_NA_SIGNATURE) {
		printf("%s: attempt to re-allocate vnic ctx %04x\n",
		    DEVNAME(softc), *ctx_id);
		return EINVAL;
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_RSS_COS_LB_CTX_ALLOC);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	*ctx_id = letoh16(resp->rss_cos_lb_ctx_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_vnic_ctx_free(struct bnxt_softc *softc, uint16_t *ctx_id)
{
	struct hwrm_vnic_rss_cos_lb_ctx_free_input req = {0};
	int rc;

	if (*ctx_id == (uint16_t)HWRM_NA_SIGNATURE) {
		printf("%s: attempt to deallocate vnic ctx %04x\n",
		    DEVNAME(softc), *ctx_id);
		return (EINVAL);
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_RSS_COS_LB_CTX_FREE);
	req.rss_cos_lb_ctx_id = htole32(*ctx_id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc == 0)
		*ctx_id = (uint16_t)HWRM_NA_SIGNATURE;
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_ring_grp_alloc(struct bnxt_softc *softc, struct bnxt_grp_info *grp)
{
	struct hwrm_ring_grp_alloc_input req = {0};
	struct hwrm_ring_grp_alloc_output *resp;
	int rc = 0;

	if (grp->grp_id != HWRM_NA_SIGNATURE) {
		printf("%s: attempt to re-allocate ring group %04x\n",
		    DEVNAME(softc), grp->grp_id);
		return EINVAL;
	}

	resp = BNXT_DMA_KVA(softc->sc_cmd_resp);
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_GRP_ALLOC);
	req.cr = htole16(grp->cp_ring_id);
	req.rr = htole16(grp->rx_ring_id);
	req.ar = htole16(grp->ag_ring_id);
	req.sc = htole16(grp->stats_ctx);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	grp->grp_id = letoh32(resp->ring_group_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_ring_grp_free(struct bnxt_softc *softc, struct bnxt_grp_info *grp)
{
	struct hwrm_ring_grp_free_input req = {0};
	int rc = 0;

	if (grp->grp_id == HWRM_NA_SIGNATURE) {
		printf("%s: attempt to free ring group %04x\n",
		    DEVNAME(softc), grp->grp_id);
		return EINVAL;
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_GRP_FREE);
	req.ring_group_id = htole32(grp->grp_id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc == 0)
		grp->grp_id = HWRM_NA_SIGNATURE;

	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

/*
 * Ring allocation message to the firmware
 */
int
bnxt_hwrm_ring_alloc(struct bnxt_softc *softc, uint8_t type,
    struct bnxt_ring *ring, uint16_t cmpl_ring_id, uint32_t stat_ctx_id,
    int irq)
{
	struct hwrm_ring_alloc_input req = {0};
	struct hwrm_ring_alloc_output *resp;
	int rc;

	if (ring->phys_id != (uint16_t)HWRM_NA_SIGNATURE) {
		printf("%s: attempt to re-allocate ring %04x\n",
		    DEVNAME(softc), ring->phys_id);
		return EINVAL;
	}

	resp = BNXT_DMA_KVA(softc->sc_cmd_resp);
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_ALLOC);
	req.enables = htole32(0);
	req.fbo = htole32(0);

	if (stat_ctx_id != HWRM_NA_SIGNATURE) {
		req.enables |= htole32(
		    HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID);
		req.stat_ctx_id = htole32(stat_ctx_id);
	}
	req.ring_type = type;
	req.page_tbl_addr = htole64(ring->paddr);
	req.length = htole32(ring->ring_size);
	req.logical_id = htole16(ring->id);
	req.cmpl_ring_id = htole16(cmpl_ring_id);
	req.queue_id = htole16(softc->sc_q_info[0].id);
	req.int_mode = 0;
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	ring->phys_id = le16toh(resp->ring_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_ring_free(struct bnxt_softc *softc, uint8_t type, struct bnxt_ring *ring)
{
	struct hwrm_ring_free_input req = {0};
	int rc;

	if (ring->phys_id == (uint16_t)HWRM_NA_SIGNATURE) {
		printf("%s: attempt to deallocate ring %04x\n",
		    DEVNAME(softc), ring->phys_id);
		return (EINVAL);
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_FREE);
	req.ring_type = type;
	req.ring_id = htole16(ring->phys_id);
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	ring->phys_id = (uint16_t)HWRM_NA_SIGNATURE;
fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}


int
bnxt_hwrm_stat_ctx_alloc(struct bnxt_softc *softc, struct bnxt_cp_ring *cpr,
    uint64_t paddr)
{
	struct hwrm_stat_ctx_alloc_input req = {0};
	struct hwrm_stat_ctx_alloc_output *resp;
	int rc = 0;

	if (cpr->stats_ctx_id != HWRM_NA_SIGNATURE) {
		printf("%s: attempt to re-allocate stats ctx %08x\n",
		    DEVNAME(softc), cpr->stats_ctx_id);
		return EINVAL;
	}

	resp = BNXT_DMA_KVA(softc->sc_cmd_resp);
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_STAT_CTX_ALLOC);

	req.update_period_ms = htole32(1000);
	req.stats_dma_addr = htole64(paddr);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	cpr->stats_ctx_id = le32toh(resp->stat_ctx_id);

fail:
	BNXT_HWRM_UNLOCK(softc);

	return rc;
}

int
bnxt_hwrm_stat_ctx_free(struct bnxt_softc *softc, struct bnxt_cp_ring *cpr)
{
	struct hwrm_stat_ctx_free_input req = {0};
	int rc = 0;

	if (cpr->stats_ctx_id == HWRM_NA_SIGNATURE) {
		printf("%s: attempt to free stats ctx %08x\n",
		    DEVNAME(softc), cpr->stats_ctx_id);
		return EINVAL;
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_STAT_CTX_FREE);
	req.stat_ctx_id = htole32(cpr->stats_ctx_id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	BNXT_HWRM_UNLOCK(softc);

	if (rc == 0)
		cpr->stats_ctx_id = HWRM_NA_SIGNATURE;

	return (rc);
}

#if 0

int
bnxt_hwrm_port_qstats(struct bnxt_softc *softc)
{
	struct hwrm_port_qstats_input req = {0};
	int rc = 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_QSTATS);

	req.port_id = htole16(softc->pf.port_id);
	req.rx_stat_host_addr = htole64(softc->hw_rx_port_stats.idi_paddr);
	req.tx_stat_host_addr = htole64(softc->hw_tx_port_stats.idi_paddr);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	BNXT_HWRM_UNLOCK(softc);

	return rc;
}

#endif

int
bnxt_hwrm_cfa_l2_set_rx_mask(struct bnxt_softc *softc,
    struct bnxt_vnic_info *vnic)
{
	struct hwrm_cfa_l2_set_rx_mask_input req = {0};
	uint32_t mask = vnic->rx_mask;

#if 0
	struct bnxt_vlan_tag *tag;
	uint32_t *tags;
	uint32_t num_vlan_tags = 0;;
	uint32_t i;
	int rc;

	SLIST_FOREACH(tag, &vnic->vlan_tags, next)
		num_vlan_tags++;

	if (num_vlan_tags) {
		if (!(mask &
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ANYVLAN_NONVLAN)) {
			if (!vnic->vlan_only)
				mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLAN_NONVLAN;
			else
				mask |=
				    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLANONLY;
		}
		if (vnic->vlan_tag_list.idi_vaddr) {
			iflib_dma_free(&vnic->vlan_tag_list);
			vnic->vlan_tag_list.idi_vaddr = NULL;
		}
		rc = iflib_dma_alloc(softc->ctx, 4 * num_vlan_tags,
		    &vnic->vlan_tag_list, BUS_DMA_NOWAIT);
		if (rc)
			return rc;
		tags = (uint32_t *)vnic->vlan_tag_list.idi_vaddr;

		i = 0;
		SLIST_FOREACH(tag, &vnic->vlan_tags, next) {
			tags[i] = htole32((tag->tpid << 16) | tag->tag);
			i++;
		}
	}
#endif
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_CFA_L2_SET_RX_MASK);

	req.vnic_id = htole32(vnic->id);
	req.mask = htole32(mask);
#if 0
	req.mc_tbl_addr = htole64(vnic->mc_list.idi_paddr);
	req.num_mc_entries = htole32(vnic->mc_list_count);
	req.vlan_tag_tbl_addr = htole64(vnic->vlan_tag_list.idi_paddr);
	req.num_vlan_tags = htole32(num_vlan_tags);
#endif
	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_set_filter(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_cfa_l2_filter_alloc_input	req = {0};
	struct hwrm_cfa_l2_filter_alloc_output	*resp;
	uint32_t enables = 0;
	int rc = 0;

	if (vnic->filter_id != -1) {
		printf("%s: attempt to re-allocate l2 ctx filter\n",
		    DEVNAME(softc));
		return EINVAL;
	}

	resp = BNXT_DMA_KVA(softc->sc_cmd_resp);
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_CFA_L2_FILTER_ALLOC);

	req.flags = htole32(HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX);
	enables = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR
	    | HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK
	    | HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID;
	req.enables = htole32(enables);
	req.dst_id = htole16(vnic->id);
	memcpy(req.l2_addr, softc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);
	memset(&req.l2_addr_mask, 0xff, sizeof(req.l2_addr_mask));

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	vnic->filter_id = le64toh(resp->l2_filter_id);
	vnic->flow_id = le64toh(resp->flow_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_free_filter(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_cfa_l2_filter_free_input req = {0};
	int rc = 0;

	if (vnic->filter_id == -1) {
		printf("%s: attempt to deallocate filter %llx\n",
		     DEVNAME(softc), vnic->filter_id);
		return (EINVAL);
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_CFA_L2_FILTER_FREE);
	req.l2_filter_id = htole64(vnic->filter_id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc == 0)
		vnic->filter_id = -1;
	BNXT_HWRM_UNLOCK(softc);

	return (rc);
}


#if 0

int
bnxt_hwrm_rss_cfg(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic,
    uint32_t hash_type)
{
	struct hwrm_vnic_rss_cfg_input	req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_RSS_CFG);

	req.hash_type = htole32(hash_type);
	req.ring_grp_tbl_addr = htole64(vnic->rss_grp_tbl.idi_paddr);
	req.hash_key_tbl_addr = htole64(vnic->rss_hash_key_tbl.idi_paddr);
	req.rss_ctx_idx = htole16(vnic->rss_id);

	return hwrm_send_message(softc, &req, sizeof(req));
}

#endif

int
bnxt_cfg_async_cr(struct bnxt_softc *softc)
{
	int rc = 0;
	
	if (1 /* BNXT_PF(softc) */) {
		struct hwrm_func_cfg_input req = {0};

		bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_CFG);

		req.fid = htole16(0xffff);
		req.enables = htole32(HWRM_FUNC_CFG_INPUT_ENABLES_ASYNC_EVENT_CR);
		req.async_event_cr = htole16(softc->sc_cp_ring.ring.phys_id);

		rc = hwrm_send_message(softc, &req, sizeof(req));
	} else {
		struct hwrm_func_vf_cfg_input req = {0};

		bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_VF_CFG);

		req.enables = htole32(HWRM_FUNC_VF_CFG_INPUT_ENABLES_ASYNC_EVENT_CR);
		req.async_event_cr = htole16(softc->sc_cp_ring.ring.phys_id);

		rc = hwrm_send_message(softc, &req, sizeof(req));
	}
	return rc;
}

#if 0

void
bnxt_validate_hw_lro_settings(struct bnxt_softc *softc)
{
	softc->hw_lro.enable = min(softc->hw_lro.enable, 1);

        softc->hw_lro.is_mode_gro = min(softc->hw_lro.is_mode_gro, 1);

	softc->hw_lro.max_agg_segs = min(softc->hw_lro.max_agg_segs,
		HWRM_VNIC_TPA_CFG_INPUT_MAX_AGG_SEGS_MAX);

	softc->hw_lro.max_aggs = min(softc->hw_lro.max_aggs,
		HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_MAX);

	softc->hw_lro.min_agg_len = min(softc->hw_lro.min_agg_len, BNXT_MAX_MTU);
}

int
bnxt_hwrm_vnic_tpa_cfg(struct bnxt_softc *softc)
{
	struct hwrm_vnic_tpa_cfg_input req = {0};
	uint32_t flags;

	if (softc->vnic_info.id == (uint16_t) HWRM_NA_SIGNATURE) {
		return 0;
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_TPA_CFG);

	if (softc->hw_lro.enable) {
		flags = HWRM_VNIC_TPA_CFG_INPUT_FLAGS_TPA |
			HWRM_VNIC_TPA_CFG_INPUT_FLAGS_ENCAP_TPA |
			HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_ECN |
			HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_SAME_GRE_SEQ;
		
        	if (softc->hw_lro.is_mode_gro)
			flags |= HWRM_VNIC_TPA_CFG_INPUT_FLAGS_GRO;
		else
			flags |= HWRM_VNIC_TPA_CFG_INPUT_FLAGS_RSC_WND_UPDATE;
			
		req.flags = htole32(flags);

		req.enables = htole32(HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGG_SEGS |
				HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGGS |
				HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MIN_AGG_LEN);

		req.max_agg_segs = htole16(softc->hw_lro.max_agg_segs);
		req.max_aggs = htole16(softc->hw_lro.max_aggs);
		req.min_agg_len = htole32(softc->hw_lro.min_agg_len);
	}

	req.vnic_id = htole16(softc->vnic_info.id);

	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_nvm_find_dir_entry(struct bnxt_softc *softc, uint16_t type,
    uint16_t *ordinal, uint16_t ext, uint16_t *index, bool use_index,
    uint8_t search_opt, uint32_t *data_length, uint32_t *item_length,
    uint32_t *fw_ver)
{
	struct hwrm_nvm_find_dir_entry_input req = {0};
	struct hwrm_nvm_find_dir_entry_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int	rc = 0;
	uint32_t old_timeo;

	MPASS(ordinal);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_FIND_DIR_ENTRY);
	if (use_index) {
		req.enables = htole32(
		    HWRM_NVM_FIND_DIR_ENTRY_INPUT_ENABLES_DIR_IDX_VALID);
		req.dir_idx = htole16(*index);
	}
	req.dir_type = htole16(type);
	req.dir_ordinal = htole16(*ordinal);
	req.dir_ext = htole16(ext);
	req.opt_ordinal = search_opt;

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (item_length)
		*item_length = le32toh(resp->dir_item_length);
	if (data_length)
		*data_length = le32toh(resp->dir_data_length);
	if (fw_ver)
		*fw_ver = le32toh(resp->fw_ver);
	*ordinal = le16toh(resp->dir_ordinal);
	if (index)
		*index = le16toh(resp->dir_idx);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_nvm_read(struct bnxt_softc *softc, uint16_t index, uint32_t offset,
    uint32_t length, struct iflib_dma_info *data)
{
	struct hwrm_nvm_read_input req = {0};
	int rc;
	uint32_t old_timeo;

	if (length > data->idi_size) {
		rc = EINVAL;
		goto exit;
	}
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_READ);
	req.host_dest_addr = htole64(data->idi_paddr);
	req.dir_idx = htole16(index);
	req.offset = htole32(offset);
	req.len = htole32(length);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	if (rc)
		goto exit;
	bus_dmamap_sync(data->idi_tag, data->idi_map, BUS_DMASYNC_POSTREAD);

	goto exit;

exit:
	return rc;
}

int
bnxt_hwrm_nvm_modify(struct bnxt_softc *softc, uint16_t index, uint32_t offset,
    void *data, bool cpyin, uint32_t length)
{
	struct hwrm_nvm_modify_input req = {0};
	struct iflib_dma_info dma_data;
	int rc;
	uint32_t old_timeo;

	if (length == 0 || !data)
		return EINVAL;
	rc = iflib_dma_alloc(softc->ctx, length, &dma_data,
	    BUS_DMA_NOWAIT);
	if (rc)
		return ENOMEM;
	if (cpyin) {
		rc = copyin(data, dma_data.idi_vaddr, length);
		if (rc)
			goto exit;
	}
	else
		memcpy(dma_data.idi_vaddr, data, length);
	bus_dmamap_sync(dma_data.idi_tag, dma_data.idi_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_MODIFY);
	req.host_src_addr = htole64(dma_data.idi_paddr);
	req.dir_idx = htole16(index);
	req.offset = htole32(offset);
	req.len = htole32(length);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);

exit:
	iflib_dma_free(&dma_data);
	return rc;
}

int
bnxt_hwrm_fw_reset(struct bnxt_softc *softc, uint8_t processor,
    uint8_t *selfreset)
{
	struct hwrm_fw_reset_input req = {0};
	struct hwrm_fw_reset_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	MPASS(selfreset);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_RESET);
	req.embedded_proc_type = processor;
	req.selfrst_status = *selfreset;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;
	*selfreset = resp->selfrst_status;

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_fw_qstatus(struct bnxt_softc *softc, uint8_t type, uint8_t *selfreset)
{
	struct hwrm_fw_qstatus_input req = {0};
	struct hwrm_fw_qstatus_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	MPASS(selfreset);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_QSTATUS);
	req.embedded_proc_type = type;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;
	*selfreset = resp->selfrst_status;

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_write(struct bnxt_softc *softc, void *data, bool cpyin,
    uint16_t type, uint16_t ordinal, uint16_t ext, uint16_t attr,
    uint16_t option, uint32_t data_length, bool keep, uint32_t *item_length,
    uint16_t *index)
{
	struct hwrm_nvm_write_input req = {0};
	struct hwrm_nvm_write_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct iflib_dma_info dma_data;
	int rc;
	uint32_t old_timeo;

	if (data_length) {
		rc = iflib_dma_alloc(softc->ctx, data_length, &dma_data,
		    BUS_DMA_NOWAIT);
		if (rc)
			return ENOMEM;
		if (cpyin) {
			rc = copyin(data, dma_data.idi_vaddr, data_length);
			if (rc)
				goto early_exit;
		}
		else
			memcpy(dma_data.idi_vaddr, data, data_length);
		bus_dmamap_sync(dma_data.idi_tag, dma_data.idi_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	else
		dma_data.idi_paddr = 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_WRITE);

	req.host_src_addr = htole64(dma_data.idi_paddr);
	req.dir_type = htole16(type);
	req.dir_ordinal = htole16(ordinal);
	req.dir_ext = htole16(ext);
	req.dir_attr = htole16(attr);
	req.dir_data_length = htole32(data_length);
	req.option = htole16(option);
	if (keep) {
		req.flags =
		    htole16(HWRM_NVM_WRITE_INPUT_FLAGS_KEEP_ORIG_ACTIVE_IMG);
	}
	if (item_length)
		req.dir_item_length = htole32(*item_length);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;
	if (item_length)
		*item_length = le32toh(resp->dir_item_length);
	if (index)
		*index = le16toh(resp->dir_idx);

exit:
	BNXT_HWRM_UNLOCK(softc);
early_exit:
	if (data_length)
		iflib_dma_free(&dma_data);
	return rc;
}

int
bnxt_hwrm_nvm_erase_dir_entry(struct bnxt_softc *softc, uint16_t index)
{
	struct hwrm_nvm_erase_dir_entry_input req = {0};
	uint32_t old_timeo;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_ERASE_DIR_ENTRY);
	req.dir_idx = htole16(index);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_get_dir_info(struct bnxt_softc *softc, uint32_t *entries,
    uint32_t *entry_length)
{
	struct hwrm_nvm_get_dir_info_input req = {0};
	struct hwrm_nvm_get_dir_info_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;
	uint32_t old_timeo;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_GET_DIR_INFO);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (entries)
		*entries = le32toh(resp->entries);
	if (entry_length)
		*entry_length = le32toh(resp->entry_length);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_get_dir_entries(struct bnxt_softc *softc, uint32_t *entries,
    uint32_t *entry_length, struct iflib_dma_info *dma_data)
{
	struct hwrm_nvm_get_dir_entries_input req = {0};
	uint32_t ent;
	uint32_t ent_len;
	int rc;
	uint32_t old_timeo;

	if (!entries)
		entries = &ent;
	if (!entry_length)
		entry_length = &ent_len;

	rc = bnxt_hwrm_nvm_get_dir_info(softc, entries, entry_length);
	if (rc)
		goto exit;
	if (*entries * *entry_length > dma_data->idi_size) {
		rc = EINVAL;
		goto exit;
	}

	/*
	 * TODO: There's a race condition here that could blow up DMA memory...
	 *	 we need to allocate the max size, not the currently in use
	 *	 size.  The command should totally have a max size here.
	 */
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_GET_DIR_ENTRIES);
	req.host_dest_addr = htole64(dma_data->idi_paddr);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	if (rc)
		goto exit;
	bus_dmamap_sync(dma_data->idi_tag, dma_data->idi_map,
	    BUS_DMASYNC_POSTWRITE);

exit:
	return rc;
}

#endif

int
bnxt_hwrm_nvm_get_dev_info(struct bnxt_softc *softc, uint16_t *mfg_id,
    uint16_t *device_id, uint32_t *sector_size, uint32_t *nvram_size,
    uint32_t *reserved_size, uint32_t *available_size)
{
	struct hwrm_nvm_get_dev_info_input req = {0};
	struct hwrm_nvm_get_dev_info_output *resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);
	int rc;
	uint32_t old_timeo;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_GET_DEV_INFO);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->sc_cmd_timeo;
	softc->sc_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->sc_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (mfg_id)
		*mfg_id = le16toh(resp->manufacturer_id);
	if (device_id)
		*device_id = le16toh(resp->device_id);
	if (sector_size)
		*sector_size = le32toh(resp->sector_size);
	if (nvram_size)
		*nvram_size = le32toh(resp->nvram_size);
	if (reserved_size)
		*reserved_size = le32toh(resp->reserved_size);
	if (available_size)
		*available_size = le32toh(resp->available_size);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

#if 0

int
bnxt_hwrm_nvm_install_update(struct bnxt_softc *softc,
    uint32_t install_type, uint64_t *installed_items, uint8_t *result,
    uint8_t *problem_item, uint8_t *reset_required)
{
	struct hwrm_nvm_install_update_input req = {0};
	struct hwrm_nvm_install_update_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;
	uint32_t old_timeo;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_INSTALL_UPDATE);
	req.install_type = htole32(install_type);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (installed_items)
		*installed_items = le32toh(resp->installed_items);
	if (result)
		*result = resp->result;
	if (problem_item)
		*problem_item = resp->problem_item;
	if (reset_required)
		*reset_required = resp->reset_required;

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_verify_update(struct bnxt_softc *softc, uint16_t type,
    uint16_t ordinal, uint16_t ext)
{
	struct hwrm_nvm_verify_update_input req = {0};
	uint32_t old_timeo;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_VERIFY_UPDATE);

	req.dir_type = htole16(type);
	req.dir_ordinal = htole16(ordinal);
	req.dir_ext = htole16(ext);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_fw_get_time(struct bnxt_softc *softc, uint16_t *year, uint8_t *month,
    uint8_t *day, uint8_t *hour, uint8_t *minute, uint8_t *second,
    uint16_t *millisecond, uint16_t *zone)
{
	struct hwrm_fw_get_time_input req = {0};
	struct hwrm_fw_get_time_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_GET_TIME);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;

	if (year)
		*year = le16toh(resp->year);
	if (month)
		*month = resp->month;
	if (day)
		*day = resp->day;
	if (hour)
		*hour = resp->hour;
	if (minute)
		*minute = resp->minute;
	if (second)
		*second = resp->second;
	if (millisecond)
		*millisecond = le16toh(resp->millisecond);
	if (zone)
		*zone = le16toh(resp->zone);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_fw_set_time(struct bnxt_softc *softc, uint16_t year, uint8_t month,
    uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
    uint16_t millisecond, uint16_t zone)
{
	struct hwrm_fw_set_time_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_SET_TIME);

	req.year = htole16(year);
	req.month = month;
	req.day = day;
	req.hour = hour;
	req.minute = minute;
	req.second = second;
	req.millisecond = htole16(millisecond);
	req.zone = htole16(zone);
	return hwrm_send_message(softc, &req, sizeof(req));
}

#endif

int
bnxt_hwrm_port_phy_qcfg(struct bnxt_softc *softc, struct ifmediareq *ifmr)
{
	struct ifnet *ifp = &softc->sc_ac.ac_if;
	struct hwrm_port_phy_qcfg_input req = {0};
	struct hwrm_port_phy_qcfg_output *resp =
	    BNXT_DMA_KVA(softc->sc_cmd_resp);
	int rc = 0;
	int link_state = LINK_STATE_DOWN;

	BNXT_HWRM_LOCK(softc);
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_PHY_QCFG);

	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc) {
		printf("%s: failed to query port phy config\n", DEVNAME(softc));
		goto exit;
	}

	if (resp->link == HWRM_PORT_PHY_QCFG_OUTPUT_LINK_LINK) {
		if (resp->duplex_state == HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_STATE_HALF)
			link_state = LINK_STATE_HALF_DUPLEX;
		else
			link_state = LINK_STATE_FULL_DUPLEX;

		switch (resp->link_speed) {
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10MB:
			ifp->if_baudrate = IF_Mbps(10);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100MB:
			ifp->if_baudrate = IF_Mbps(100);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_1GB:
			ifp->if_baudrate = IF_Gbps(1);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2GB:
			ifp->if_baudrate = IF_Gbps(2);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2_5GB:
			ifp->if_baudrate = IF_Mbps(2500);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10GB:
			ifp->if_baudrate = IF_Gbps(10);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_20GB:
			ifp->if_baudrate = IF_Gbps(20);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_25GB:
			ifp->if_baudrate = IF_Gbps(25);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_40GB:
			ifp->if_baudrate = IF_Gbps(40);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_50GB:
			ifp->if_baudrate = IF_Gbps(50);
			break;
		case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100GB:
			ifp->if_baudrate = IF_Gbps(100);
			break;
		}
	}

	if (ifmr != NULL) {
		ifmr->ifm_status = IFM_AVALID;
		if (LINK_STATE_IS_UP(ifp->if_link_state)) {
			ifmr->ifm_status |= IFM_ACTIVE;
			ifmr->ifm_active = IFM_ETHER | IFM_AUTO;
			if (resp->pause & HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_TX)
				ifmr->ifm_active |= IFM_ETH_TXPAUSE;
			if (resp->pause & HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_RX)
				ifmr->ifm_active |= IFM_ETH_RXPAUSE;
			if (resp->duplex_state == HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_STATE_HALF)
				ifmr->ifm_active |= IFM_HDX;
			else
				ifmr->ifm_active |= IFM_FDX;
		}
	}

exit:
	BNXT_HWRM_UNLOCK(softc);

	if (rc == 0 && (link_state != ifp->if_link_state)) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}

	return rc;
}

#if 0

uint16_t
bnxt_hwrm_get_wol_fltrs(struct bnxt_softc *softc, uint16_t handle)
{
	struct hwrm_wol_filter_qcfg_input req = {0};
	struct hwrm_wol_filter_qcfg_output *resp =
			(void *)softc->hwrm_cmd_resp.idi_vaddr;
	uint16_t next_handle = 0;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_WOL_FILTER_QCFG);
	req.port_id = htole16(softc->pf.port_id);
	req.handle = htole16(handle);
	rc = hwrm_send_message(softc, &req, sizeof(req));
	if (!rc) {
		next_handle = le16toh(resp->next_handle);
		if (next_handle != 0) {
			if (resp->wol_type ==
				HWRM_WOL_FILTER_ALLOC_INPUT_WOL_TYPE_MAGICPKT) {
				softc->wol = 1;
				softc->wol_filter_id = resp->wol_filter_id;
			}
		}
	}
	return next_handle;
}

int
bnxt_hwrm_alloc_wol_fltr(struct bnxt_softc *softc)
{
	struct hwrm_wol_filter_alloc_input req = {0};
	struct hwrm_wol_filter_alloc_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_WOL_FILTER_ALLOC);
	req.port_id = htole16(softc->pf.port_id);
	req.wol_type = HWRM_WOL_FILTER_ALLOC_INPUT_WOL_TYPE_MAGICPKT;
	req.enables =
		htole32(HWRM_WOL_FILTER_ALLOC_INPUT_ENABLES_MAC_ADDRESS);
	memcpy(req.mac_address, softc->func.mac_addr, ETHER_ADDR_LEN);
	rc = hwrm_send_message(softc, &req, sizeof(req));
	if (!rc)
		softc->wol_filter_id = resp->wol_filter_id;

	return rc;
}

int
bnxt_hwrm_free_wol_fltr(struct bnxt_softc *softc)
{
	struct hwrm_wol_filter_free_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_WOL_FILTER_FREE);
	req.port_id = htole16(softc->pf.port_id);
	req.enables =
		htole32(HWRM_WOL_FILTER_FREE_INPUT_ENABLES_WOL_FILTER_ID);
	req.wol_filter_id = softc->wol_filter_id;
	return hwrm_send_message(softc, &req, sizeof(req));
}

static void bnxt_hwrm_set_coal_params(struct bnxt_softc *softc, uint32_t max_frames,
        uint32_t buf_tmrs, uint16_t flags,
        struct hwrm_ring_cmpl_ring_cfg_aggint_params_input *req)
{
        req->flags = htole16(flags);
        req->num_cmpl_dma_aggr = htole16((uint16_t)max_frames);
        req->num_cmpl_dma_aggr_during_int = htole16(max_frames >> 16);
        req->cmpl_aggr_dma_tmr = htole16((uint16_t)buf_tmrs);
        req->cmpl_aggr_dma_tmr_during_int = htole16(buf_tmrs >> 16);
        /* Minimum time between 2 interrupts set to buf_tmr x 2 */
        req->int_lat_tmr_min = htole16((uint16_t)buf_tmrs * 2);
        req->int_lat_tmr_max = htole16((uint16_t)buf_tmrs * 4);
        req->num_cmpl_aggr_int = htole16((uint16_t)max_frames * 4);
}


int bnxt_hwrm_set_coal(struct bnxt_softc *softc)
{
        int i, rc = 0;
        struct hwrm_ring_cmpl_ring_cfg_aggint_params_input req_rx = {0},
                                                           req_tx = {0}, *req;
        uint16_t max_buf, max_buf_irq;
        uint16_t buf_tmr, buf_tmr_irq;
        uint32_t flags;

        bnxt_hwrm_cmd_hdr_init(softc, &req_rx,
                               HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS);
        bnxt_hwrm_cmd_hdr_init(softc, &req_tx,
                               HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS);

        /* Each rx completion (2 records) should be DMAed immediately.
         * DMA 1/4 of the completion buffers at a time.
         */
        max_buf = min_t(uint16_t, softc->rx_coal_frames / 4, 2);
        /* max_buf must not be zero */
        max_buf = clamp_t(uint16_t, max_buf, 1, 63);
        max_buf_irq = clamp_t(uint16_t, softc->rx_coal_frames_irq, 1, 63);
        buf_tmr = BNXT_USEC_TO_COAL_TIMER(softc->rx_coal_usecs);
        /* buf timer set to 1/4 of interrupt timer */
        buf_tmr = max_t(uint16_t, buf_tmr / 4, 1);
        buf_tmr_irq = BNXT_USEC_TO_COAL_TIMER(softc->rx_coal_usecs_irq);
        buf_tmr_irq = max_t(uint16_t, buf_tmr_irq, 1);

        flags = HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_TIMER_RESET;

        /* RING_IDLE generates more IRQs for lower latency.  Enable it only
         * if coal_usecs is less than 25 us.
         */
        if (softc->rx_coal_usecs < 25)
                flags |= HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_RING_IDLE;

        bnxt_hwrm_set_coal_params(softc, max_buf_irq << 16 | max_buf,
                                  buf_tmr_irq << 16 | buf_tmr, flags, &req_rx);

        /* max_buf must not be zero */
        max_buf = clamp_t(uint16_t, softc->tx_coal_frames, 1, 63);
        max_buf_irq = clamp_t(uint16_t, softc->tx_coal_frames_irq, 1, 63);
        buf_tmr = BNXT_USEC_TO_COAL_TIMER(softc->tx_coal_usecs);
        /* buf timer set to 1/4 of interrupt timer */
        buf_tmr = max_t(uint16_t, buf_tmr / 4, 1);
        buf_tmr_irq = BNXT_USEC_TO_COAL_TIMER(softc->tx_coal_usecs_irq);
        buf_tmr_irq = max_t(uint16_t, buf_tmr_irq, 1);
        flags = HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_TIMER_RESET;
        bnxt_hwrm_set_coal_params(softc, max_buf_irq << 16 | max_buf,
                                  buf_tmr_irq << 16 | buf_tmr, flags, &req_tx);

        for (i = 0; i < softc->nrxqsets; i++) {

                
		req = &req_rx;
                /*
                 * TBD:
		 *      Check if Tx also needs to be done
                 *      So far, Tx processing has been done in softirq contest
                 *
		 * req = &req_tx;
		 */
		req->ring_id = htole16(softc->grp_info[i].cp_ring_id);

                rc = hwrm_send_message(softc, req, sizeof(*req));
                if (rc)
                        break;
        }
        return rc;
}

#endif

void
_bnxt_hwrm_set_async_event_bit(struct hwrm_func_drv_rgtr_input *req, int bit)
{
	req->async_event_fwd[bit/32] |= (1 << (bit % 32));
}

int bnxt_hwrm_func_rgtr_async_events(struct bnxt_softc *softc)
{
	struct hwrm_func_drv_rgtr_input req = {0};
	int events[] = {
		HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE,
		HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD,
		HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED,
		HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE,
		HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE
	};
	int i;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_DRV_RGTR);

	req.enables =
		htole32(HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD);

	for (i = 0; i < nitems(events); i++)
		_bnxt_hwrm_set_async_event_bit(&req, events[i]);

	return hwrm_send_message(softc, &req, sizeof(req));
}
