/*	$OpenBSD: qwx.c,v 1.2 2024/01/02 17:39:08 stsp Exp $	*/

/*
 * Copyright 2023 Stefan Sperling <stsp@openbsd.org>
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

/*
 * Copyright (c) 2018-2019 The Linux Foundation.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  * Neither the name of [Owner Organization] nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for Qualcomm Technologies 802.11ax chipset.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <sys/refcnt.h>
#include <sys/task.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

/* XXX linux porting goo */
#ifdef __LP64__
#define BITS_PER_LONG		64
#else
#define BITS_PER_LONG		32
#endif
#define GENMASK(h, l) (((~0UL) >> (BITS_PER_LONG - (h) - 1)) & ((~0UL) << (l)))
#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_GET(_m, _v) ((typeof(_m))(((_v) & (_m)) >> __bf_shf(_m)))
#define FIELD_PREP(_m, _v) (((typeof(_m))(_v) << __bf_shf(_m)) & (_m))
#define BIT(x)               (1UL << (x))
#define test_bit(i, a)  ((a) & (1 << (i)))
#define clear_bit(i, a) ((a)) &= ~(1 << (i))
#define set_bit(i, a)   ((a)) |= (1 << (i))
#define container_of(ptr, type, member) ({			\
	const __typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

/* #define QWX_DEBUG */

#include <dev/ic/qwxreg.h>
#include <dev/ic/qwxvar.h>

#ifdef QWX_DEBUG
uint32_t	qwx_debug = 0
		    | QWX_D_MISC
/*		    | QWX_D_MHI */
/*		    | QWX_D_QMI */
/*		    | QWX_D_WMI */
/*		    | QWX_D_HTC */
/*		    | QWX_D_HTT */
/*		    | QWX_D_MAC */
/*		    | QWX_D_MGMT */
		;
#endif

int qwx_ce_init_pipes(struct qwx_softc *);
int qwx_hal_srng_src_num_free(struct qwx_softc *, struct hal_srng *, int);
int qwx_ce_per_engine_service(struct qwx_softc *, uint16_t);
int qwx_hal_srng_setup(struct qwx_softc *, enum hal_ring_type, int, int,
    struct hal_srng_params *);
int qwx_ce_send(struct qwx_softc *, struct mbuf *, uint8_t, uint16_t);
int qwx_htc_connect_service(struct qwx_htc *, struct qwx_htc_svc_conn_req *,
    struct qwx_htc_svc_conn_resp *);
void qwx_hal_srng_shadow_update_hp_tp(struct qwx_softc *, struct hal_srng *);
int qwx_dp_service_srng(struct qwx_softc *);
void qwx_wmi_free_dbring_caps(struct qwx_softc *);
int qwx_core_init(struct qwx_softc *);
int qwx_qmi_event_server_arrive(struct qwx_softc *);
int qwx_mac_register(struct qwx_softc *);
int qwx_mac_start(struct qwx_softc *);
void qwx_mac_scan_finish(struct qwx_softc *);

int qwx_scan(struct qwx_softc *);
void qwx_scan_abort(struct qwx_softc *);
int qwx_assoc(struct qwx_softc *);
int qwx_disassoc(struct qwx_softc *);
int qwx_auth(struct qwx_softc *);
int qwx_deauth(struct qwx_softc *);
int qwx_run(struct qwx_softc *);
int qwx_run_stop(struct qwx_softc *);

struct ieee80211_node *
qwx_node_alloc(struct ieee80211com *ic)
{
	return malloc(sizeof(struct qwx_node), M_DEVBUF, M_NOWAIT | M_ZERO);
}

int
qwx_init(struct ifnet *ifp)
{
	int error;
	struct qwx_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	sc->fw_mode = ATH11K_FIRMWARE_MODE_NORMAL;
	sc->crypto_mode = ATH11K_CRYPT_MODE_SW;
	sc->frame_mode = ATH11K_HW_TXRX_NATIVE_WIFI;
	ic->ic_state = IEEE80211_S_INIT;
	sc->ns_nstate = IEEE80211_S_INIT;
	sc->scan.state = ATH11K_SCAN_IDLE;
	sc->vdev_id_11d_scan = QWX_11D_INVALID_VDEV_ID;

	error = qwx_core_init(sc);
	if (error) {
		printf(": failed to init core: %d\n", error);
		return error;
	}

	memset(&sc->qrtr_server, 0, sizeof(sc->qrtr_server));
	sc->qrtr_server.node = QRTR_NODE_BCAST;

	/* wait for QRTR init to be done */
	while (sc->qrtr_server.node == QRTR_NODE_BCAST) {
		error = tsleep_nsec(&sc->qrtr_server, 0, "qwxqrtr",
		    SEC_TO_NSEC(5));
		if (error) {
			printf("%s: qrtr init timeout\n", sc->sc_dev.dv_xname);
			return error;
		}
	}

	error = qwx_qmi_event_server_arrive(sc);
	if (error)
		return error;

	/* Configure channel information obtained from firmware. */
	ieee80211_channel_init(ifp);
	ieee80211_media_init(ifp, qwx_media_change, ieee80211_media_status);

	if (sc->attached) {
		/*
		 * We are either going up for the first time or qwx_stop() ran
		 * before us and has waited for any stale tasks to finish up.
		 */
		KASSERT(sc->task_refs.r_refs == 0);
		refcnt_init(&sc->task_refs);

		ifq_clr_oactive(&ifp->if_snd);
		ifp->if_flags |= IFF_RUNNING;

		/* Update MAC in case the upper layers changed it. */
		IEEE80211_ADDR_COPY(ic->ic_myaddr,
		    ((struct arpcom *)ifp)->ac_enaddr);

		error = qwx_mac_start(sc);
		if (error)
			return error;

		ieee80211_begin_scan(ifp);
	} else {
		sc->attached = 1;

		/* Configure MAC address at boot-time. */
		error = if_setlladdr(ifp, ic->ic_myaddr);
		if (error)
			printf("%s: could not set MAC address %s: %d\n",
			    sc->sc_dev.dv_xname, ether_sprintf(ic->ic_myaddr),
			    error);
	}

	return 0;
}

void
qwx_add_task(struct qwx_softc *sc, struct taskq *taskq, struct task *task)
{
	int s = splnet();

	if (test_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags)) {
		splx(s);
		return;
	}

	refcnt_take(&sc->task_refs);
	if (!task_add(taskq, task))
		refcnt_rele_wake(&sc->task_refs);
	splx(s);
}

void
qwx_del_task(struct qwx_softc *sc, struct taskq *taskq, struct task *task)
{
	if (task_del(taskq, task))
		refcnt_rele(&sc->task_refs);
}

void
qwx_stop(struct ifnet *ifp)
{
	struct qwx_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s = splnet();

	rw_assert_wrlock(&sc->ioctl_rwl);

	/* Disallow new tasks. */
	set_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags);

	/* Cancel scheduled tasks and let any stale tasks finish up. */
	task_del(systq, &sc->init_task);
	qwx_del_task(sc, sc->sc_nswq, &sc->newstate_task);
	refcnt_finalize(&sc->task_refs, "qwxstop");

	clear_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags);

	ifp->if_timer = sc->sc_tx_timer = 0;

	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	sc->sc_newstate(ic, IEEE80211_S_INIT, -1);
	sc->ns_nstate = IEEE80211_S_INIT;
	sc->scan.state = ATH11K_SCAN_IDLE;
	sc->vdev_id_11d_scan = QWX_11D_INVALID_VDEV_ID;

	/* power off hardware */
	qwx_core_deinit(sc);

	splx(s);
}

int
qwx_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct qwx_softc *sc = ifp->if_softc;
	int s, err = 0;

	/*
	 * Prevent processes from entering this function while another
	 * process is tsleep'ing in it.
	 */
	err = rw_enter(&sc->ioctl_rwl, RW_WRITE | RW_INTR);
	if (err)
		return err;
	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING)) {
				/* Force reload of firmware image from disk. */
				sc->have_firmware = 0;
				err = qwx_init(ifp);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				qwx_stop(ifp);
		}
		break;

	default:
		err = ieee80211_ioctl(ifp, cmd, data);
	}

	if (err == ENETRESET) {
		err = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			qwx_stop(ifp);
			err = qwx_init(ifp);
		}
	}

	splx(s);
	rw_exit(&sc->ioctl_rwl);

	return err;
}

void
qwx_start(struct ifnet *ifp)
{
	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd))
		return;

	printf("%s: not implemented\n", __func__);
}

void
qwx_watchdog(struct ifnet *ifp)
{
	struct qwx_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			if (!test_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags))
				task_add(systq, &sc->init_task);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
qwx_media_change(struct ifnet *ifp)
{
	int err;

	err = ieee80211_media_change(ifp);
	if (err != ENETRESET)
		return err;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		qwx_stop(ifp);
		err = qwx_init(ifp);
	}

	return err;
}

int
qwx_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct qwx_softc *sc = ifp->if_softc;

	/*
	 * Prevent attempts to transition towards the same state, unless
	 * we are scanning in which case a SCAN -> SCAN transition
	 * triggers another scan iteration. And AUTH -> AUTH is needed
	 * to support band-steering.
	 */
	if (sc->ns_nstate == nstate && nstate != IEEE80211_S_SCAN &&
	    nstate != IEEE80211_S_AUTH)
		return 0;
#if 0
	if (ic->ic_state == IEEE80211_S_RUN) {
		qwx_del_task(sc, systq, &sc->ba_task);
		qwx_del_task(sc, systq, &sc->setkey_task);
		memset(sc->setkey_arg, 0, sizeof(sc->setkey_arg));
		sc->setkey_cur = sc->setkey_tail = sc->setkey_nkeys = 0;
		qwx_del_task(sc, systq, &sc->bgscan_done_task);
	}
#endif
	sc->ns_nstate = nstate;
	sc->ns_arg = arg;

	qwx_add_task(sc, sc->sc_nswq, &sc->newstate_task);

	return 0;
}

void
qwx_newstate_task(void *arg)
{
	struct qwx_softc *sc = (struct qwx_softc *)arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	enum ieee80211_state nstate = sc->ns_nstate;
	enum ieee80211_state ostate = ic->ic_state;
	int err = 0, s = splnet();

	if (test_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags)) {
		/* qwx_stop() is waiting for us. */
		refcnt_rele_wake(&sc->task_refs);
		splx(s);
		return;
	}

	if (ostate == IEEE80211_S_SCAN) {
		if (nstate == ostate) {
			if (sc->scan.state != ATH11K_SCAN_IDLE) {
				refcnt_rele_wake(&sc->task_refs);
				splx(s);
				return;
			}
			/* Firmware is no longer scanning. Do another scan. */
			goto next_scan;
		}
	}

	if (nstate <= ostate) {
		switch (ostate) {
		case IEEE80211_S_RUN:
			err = qwx_run_stop(sc);
			if (err)
				goto out;
			/* FALLTHROUGH */
		case IEEE80211_S_ASSOC:
			if (nstate <= IEEE80211_S_ASSOC) {
				err = qwx_disassoc(sc);
				if (err)
					goto out;
			}
			/* FALLTHROUGH */
		case IEEE80211_S_AUTH:
			if (nstate <= IEEE80211_S_AUTH) {
				err = qwx_deauth(sc);
				if (err)
					goto out;
			}
			/* FALLTHROUGH */
		case IEEE80211_S_SCAN:
		case IEEE80211_S_INIT:
			break;
		}

		/* Die now if qwx_stop() was called while we were sleeping. */
		if (test_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags)) {
			refcnt_rele_wake(&sc->task_refs);
			splx(s);
			return;
		}
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;

	case IEEE80211_S_SCAN:
next_scan:
		err = qwx_scan(sc);
		if (err)
			break;
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: %s -> %s\n", ifp->if_xname,
			    ieee80211_state_name[ic->ic_state],
			    ieee80211_state_name[IEEE80211_S_SCAN]);
		ic->ic_state = IEEE80211_S_SCAN;
		refcnt_rele_wake(&sc->task_refs);
		splx(s);
		return;

	case IEEE80211_S_AUTH:
		err = qwx_auth(sc);
		break;

	case IEEE80211_S_ASSOC:
		err = qwx_assoc(sc);
		break;

	case IEEE80211_S_RUN:
		err = qwx_run(sc);
		break;
	}
out:
	if (!test_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags)) {
		if (err)
			task_add(systq, &sc->init_task);
		else
			sc->sc_newstate(ic, nstate, sc->ns_arg);
	}
	refcnt_rele_wake(&sc->task_refs);
	splx(s);
}

struct cfdriver qwx_cd = {
	NULL, "qwx", DV_IFNET
};

void qwx_init_wmi_config_qca6390(struct qwx_softc *sc,
    struct target_resource_config *config)
{
	config->num_vdevs = 4;
	config->num_peers = 16;
	config->num_tids = 32;

	config->num_offload_peers = 3;
	config->num_offload_reorder_buffs = 3;
	config->num_peer_keys = TARGET_NUM_PEER_KEYS;
	config->ast_skid_limit = TARGET_AST_SKID_LIMIT;
	config->tx_chain_mask = (1 << sc->target_caps.num_rf_chains) - 1;
	config->rx_chain_mask = (1 << sc->target_caps.num_rf_chains) - 1;
	config->rx_timeout_pri[0] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[1] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[2] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[3] = TARGET_RX_TIMEOUT_HI_PRI;
	config->rx_decap_mode = TARGET_DECAP_MODE_NATIVE_WIFI;
	config->scan_max_pending_req = TARGET_SCAN_MAX_PENDING_REQS;
	config->bmiss_offload_max_vdev = TARGET_BMISS_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_vdev = TARGET_ROAM_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_ap_profiles = TARGET_ROAM_OFFLOAD_MAX_AP_PROFILES;
	config->num_mcast_groups = 0;
	config->num_mcast_table_elems = 0;
	config->mcast2ucast_mode = 0;
	config->tx_dbg_log_size = TARGET_TX_DBG_LOG_SIZE;
	config->num_wds_entries = 0;
	config->dma_burst_size = 0;
	config->rx_skip_defrag_timeout_dup_detection_check = 0;
	config->vow_config = TARGET_VOW_CONFIG;
	config->gtk_offload_max_vdev = 2;
	config->num_msdu_desc = 0x400;
	config->beacon_tx_offload_max_vdev = 2;
	config->rx_batchmode = TARGET_RX_BATCHMODE;

	config->peer_map_unmap_v2_support = 0;
	config->use_pdev_id = 1;
	config->max_frag_entries = 0xa;
	config->num_tdls_vdevs = 0x1;
	config->num_tdls_conn_table_entries = 8;
	config->beacon_tx_offload_max_vdev = 0x2;
	config->num_multicast_filter_entries = 0x20;
	config->num_wow_filters = 0x16;
	config->num_keep_alive_pattern = 0;
	config->flag1 |= WMI_RSRC_CFG_FLAG1_BSS_CHANNEL_INFO_64;
}

void qwx_init_wmi_config_ipq8074(struct qwx_softc *sc,
    struct target_resource_config *config)
{
	config->num_vdevs = sc->num_radios * TARGET_NUM_VDEVS(sc);

	if (sc->num_radios == 2) {
		config->num_peers = TARGET_NUM_PEERS(sc, DBS);
		config->num_tids = TARGET_NUM_TIDS(sc, DBS);
	} else if (sc->num_radios == 3) {
		config->num_peers = TARGET_NUM_PEERS(sc, DBS_SBS);
		config->num_tids = TARGET_NUM_TIDS(sc, DBS_SBS);
	} else {
		/* Control should not reach here */
		config->num_peers = TARGET_NUM_PEERS(sc, SINGLE);
		config->num_tids = TARGET_NUM_TIDS(sc, SINGLE);
	}
	config->num_offload_peers = TARGET_NUM_OFFLD_PEERS;
	config->num_offload_reorder_buffs = TARGET_NUM_OFFLD_REORDER_BUFFS;
	config->num_peer_keys = TARGET_NUM_PEER_KEYS;
	config->ast_skid_limit = TARGET_AST_SKID_LIMIT;
	config->tx_chain_mask = (1 << sc->target_caps.num_rf_chains) - 1;
	config->rx_chain_mask = (1 << sc->target_caps.num_rf_chains) - 1;
	config->rx_timeout_pri[0] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[1] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[2] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[3] = TARGET_RX_TIMEOUT_HI_PRI;

	if (test_bit(ATH11K_FLAG_RAW_MODE, sc->sc_flags))
		config->rx_decap_mode = TARGET_DECAP_MODE_RAW;
	else
		config->rx_decap_mode = TARGET_DECAP_MODE_NATIVE_WIFI;

	config->scan_max_pending_req = TARGET_SCAN_MAX_PENDING_REQS;
	config->bmiss_offload_max_vdev = TARGET_BMISS_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_vdev = TARGET_ROAM_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_ap_profiles = TARGET_ROAM_OFFLOAD_MAX_AP_PROFILES;
	config->num_mcast_groups = TARGET_NUM_MCAST_GROUPS;
	config->num_mcast_table_elems = TARGET_NUM_MCAST_TABLE_ELEMS;
	config->mcast2ucast_mode = TARGET_MCAST2UCAST_MODE;
	config->tx_dbg_log_size = TARGET_TX_DBG_LOG_SIZE;
	config->num_wds_entries = TARGET_NUM_WDS_ENTRIES;
	config->dma_burst_size = TARGET_DMA_BURST_SIZE;
	config->rx_skip_defrag_timeout_dup_detection_check =
		TARGET_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK;
	config->vow_config = TARGET_VOW_CONFIG;
	config->gtk_offload_max_vdev = TARGET_GTK_OFFLOAD_MAX_VDEV;
	config->num_msdu_desc = TARGET_NUM_MSDU_DESC;
	config->beacon_tx_offload_max_vdev = sc->num_radios * TARGET_MAX_BCN_OFFLD;
	config->rx_batchmode = TARGET_RX_BATCHMODE;
	config->peer_map_unmap_v2_support = 1;
	config->twt_ap_pdev_count = sc->num_radios;
	config->twt_ap_sta_count = 1000;
	config->flag1 |= WMI_RSRC_CFG_FLAG1_BSS_CHANNEL_INFO_64;
	config->flag1 |= WMI_RSRC_CFG_FLAG1_ACK_RSSI;
	config->ema_max_vap_cnt = sc->num_radios;
	config->ema_max_profile_period = TARGET_EMA_MAX_PROFILE_PERIOD;
	config->beacon_tx_offload_max_vdev += config->ema_max_vap_cnt;
}

const struct ath11k_hw_ops ipq8074_ops = {
#if notyet
	.get_hw_mac_from_pdev_id = ath11k_hw_ipq8074_mac_from_pdev_id,
#endif
	.wmi_init_config = qwx_init_wmi_config_ipq8074,
#if notyet
	.mac_id_to_pdev_id = ath11k_hw_mac_id_to_pdev_id_ipq8074,
	.mac_id_to_srng_id = ath11k_hw_mac_id_to_srng_id_ipq8074,
	.tx_mesh_enable = ath11k_hw_ipq8074_tx_mesh_enable,
	.rx_desc_get_first_msdu = ath11k_hw_ipq8074_rx_desc_get_first_msdu,
	.rx_desc_get_last_msdu = ath11k_hw_ipq8074_rx_desc_get_last_msdu,
	.rx_desc_get_l3_pad_bytes = ath11k_hw_ipq8074_rx_desc_get_l3_pad_bytes,
	.rx_desc_get_hdr_status = ath11k_hw_ipq8074_rx_desc_get_hdr_status,
	.rx_desc_encrypt_valid = ath11k_hw_ipq8074_rx_desc_encrypt_valid,
	.rx_desc_get_encrypt_type = ath11k_hw_ipq8074_rx_desc_get_encrypt_type,
	.rx_desc_get_decap_type = ath11k_hw_ipq8074_rx_desc_get_decap_type,
	.rx_desc_get_mesh_ctl = ath11k_hw_ipq8074_rx_desc_get_mesh_ctl,
	.rx_desc_get_ldpc_support = ath11k_hw_ipq8074_rx_desc_get_ldpc_support,
	.rx_desc_get_mpdu_seq_ctl_vld = ath11k_hw_ipq8074_rx_desc_get_mpdu_seq_ctl_vld,
	.rx_desc_get_mpdu_fc_valid = ath11k_hw_ipq8074_rx_desc_get_mpdu_fc_valid,
	.rx_desc_get_mpdu_start_seq_no = ath11k_hw_ipq8074_rx_desc_get_mpdu_start_seq_no,
	.rx_desc_get_msdu_len = ath11k_hw_ipq8074_rx_desc_get_msdu_len,
	.rx_desc_get_msdu_sgi = ath11k_hw_ipq8074_rx_desc_get_msdu_sgi,
	.rx_desc_get_msdu_rate_mcs = ath11k_hw_ipq8074_rx_desc_get_msdu_rate_mcs,
	.rx_desc_get_msdu_rx_bw = ath11k_hw_ipq8074_rx_desc_get_msdu_rx_bw,
	.rx_desc_get_msdu_freq = ath11k_hw_ipq8074_rx_desc_get_msdu_freq,
	.rx_desc_get_msdu_pkt_type = ath11k_hw_ipq8074_rx_desc_get_msdu_pkt_type,
	.rx_desc_get_msdu_nss = ath11k_hw_ipq8074_rx_desc_get_msdu_nss,
	.rx_desc_get_mpdu_tid = ath11k_hw_ipq8074_rx_desc_get_mpdu_tid,
	.rx_desc_get_mpdu_peer_id = ath11k_hw_ipq8074_rx_desc_get_mpdu_peer_id,
	.rx_desc_copy_attn_end_tlv = ath11k_hw_ipq8074_rx_desc_copy_attn_end,
	.rx_desc_get_mpdu_start_tag = ath11k_hw_ipq8074_rx_desc_get_mpdu_start_tag,
	.rx_desc_get_mpdu_ppdu_id = ath11k_hw_ipq8074_rx_desc_get_mpdu_ppdu_id,
	.rx_desc_set_msdu_len = ath11k_hw_ipq8074_rx_desc_set_msdu_len,
	.rx_desc_get_attention = ath11k_hw_ipq8074_rx_desc_get_attention,
	.rx_desc_get_msdu_payload = ath11k_hw_ipq8074_rx_desc_get_msdu_payload,
	.reo_setup = ath11k_hw_ipq8074_reo_setup,
	.mpdu_info_get_peerid = ath11k_hw_ipq8074_mpdu_info_get_peerid,
	.rx_desc_mac_addr2_valid = ath11k_hw_ipq8074_rx_desc_mac_addr2_valid,
	.rx_desc_mpdu_start_addr2 = ath11k_hw_ipq8074_rx_desc_mpdu_start_addr2,
	.get_ring_selector = ath11k_hw_ipq8074_get_tcl_ring_selector,
#endif
};

const struct ath11k_hw_ops ipq6018_ops = {
#if notyet
	.get_hw_mac_from_pdev_id = ath11k_hw_ipq6018_mac_from_pdev_id,
#endif
	.wmi_init_config = qwx_init_wmi_config_ipq8074,
#if notyet
	.mac_id_to_pdev_id = ath11k_hw_mac_id_to_pdev_id_ipq8074,
	.mac_id_to_srng_id = ath11k_hw_mac_id_to_srng_id_ipq8074,
	.tx_mesh_enable = ath11k_hw_ipq8074_tx_mesh_enable,
	.rx_desc_get_first_msdu = ath11k_hw_ipq8074_rx_desc_get_first_msdu,
	.rx_desc_get_last_msdu = ath11k_hw_ipq8074_rx_desc_get_last_msdu,
	.rx_desc_get_l3_pad_bytes = ath11k_hw_ipq8074_rx_desc_get_l3_pad_bytes,
	.rx_desc_get_hdr_status = ath11k_hw_ipq8074_rx_desc_get_hdr_status,
	.rx_desc_encrypt_valid = ath11k_hw_ipq8074_rx_desc_encrypt_valid,
	.rx_desc_get_encrypt_type = ath11k_hw_ipq8074_rx_desc_get_encrypt_type,
	.rx_desc_get_decap_type = ath11k_hw_ipq8074_rx_desc_get_decap_type,
	.rx_desc_get_mesh_ctl = ath11k_hw_ipq8074_rx_desc_get_mesh_ctl,
	.rx_desc_get_ldpc_support = ath11k_hw_ipq8074_rx_desc_get_ldpc_support,
	.rx_desc_get_mpdu_seq_ctl_vld = ath11k_hw_ipq8074_rx_desc_get_mpdu_seq_ctl_vld,
	.rx_desc_get_mpdu_fc_valid = ath11k_hw_ipq8074_rx_desc_get_mpdu_fc_valid,
	.rx_desc_get_mpdu_start_seq_no = ath11k_hw_ipq8074_rx_desc_get_mpdu_start_seq_no,
	.rx_desc_get_msdu_len = ath11k_hw_ipq8074_rx_desc_get_msdu_len,
	.rx_desc_get_msdu_sgi = ath11k_hw_ipq8074_rx_desc_get_msdu_sgi,
	.rx_desc_get_msdu_rate_mcs = ath11k_hw_ipq8074_rx_desc_get_msdu_rate_mcs,
	.rx_desc_get_msdu_rx_bw = ath11k_hw_ipq8074_rx_desc_get_msdu_rx_bw,
	.rx_desc_get_msdu_freq = ath11k_hw_ipq8074_rx_desc_get_msdu_freq,
	.rx_desc_get_msdu_pkt_type = ath11k_hw_ipq8074_rx_desc_get_msdu_pkt_type,
	.rx_desc_get_msdu_nss = ath11k_hw_ipq8074_rx_desc_get_msdu_nss,
	.rx_desc_get_mpdu_tid = ath11k_hw_ipq8074_rx_desc_get_mpdu_tid,
	.rx_desc_get_mpdu_peer_id = ath11k_hw_ipq8074_rx_desc_get_mpdu_peer_id,
	.rx_desc_copy_attn_end_tlv = ath11k_hw_ipq8074_rx_desc_copy_attn_end,
	.rx_desc_get_mpdu_start_tag = ath11k_hw_ipq8074_rx_desc_get_mpdu_start_tag,
	.rx_desc_get_mpdu_ppdu_id = ath11k_hw_ipq8074_rx_desc_get_mpdu_ppdu_id,
	.rx_desc_set_msdu_len = ath11k_hw_ipq8074_rx_desc_set_msdu_len,
	.rx_desc_get_attention = ath11k_hw_ipq8074_rx_desc_get_attention,
	.rx_desc_get_msdu_payload = ath11k_hw_ipq8074_rx_desc_get_msdu_payload,
	.reo_setup = ath11k_hw_ipq8074_reo_setup,
	.mpdu_info_get_peerid = ath11k_hw_ipq8074_mpdu_info_get_peerid,
	.rx_desc_mac_addr2_valid = ath11k_hw_ipq8074_rx_desc_mac_addr2_valid,
	.rx_desc_mpdu_start_addr2 = ath11k_hw_ipq8074_rx_desc_mpdu_start_addr2,
	.get_ring_selector = ath11k_hw_ipq8074_get_tcl_ring_selector,
#endif
};

const struct ath11k_hw_ops qca6390_ops = {
#if notyet
	.get_hw_mac_from_pdev_id = ath11k_hw_ipq8074_mac_from_pdev_id,
#endif
	.wmi_init_config = qwx_init_wmi_config_qca6390,
#if notyet
	.mac_id_to_pdev_id = ath11k_hw_mac_id_to_pdev_id_qca6390,
	.mac_id_to_srng_id = ath11k_hw_mac_id_to_srng_id_qca6390,
	.tx_mesh_enable = ath11k_hw_ipq8074_tx_mesh_enable,
	.rx_desc_get_first_msdu = ath11k_hw_ipq8074_rx_desc_get_first_msdu,
	.rx_desc_get_last_msdu = ath11k_hw_ipq8074_rx_desc_get_last_msdu,
	.rx_desc_get_l3_pad_bytes = ath11k_hw_ipq8074_rx_desc_get_l3_pad_bytes,
	.rx_desc_get_hdr_status = ath11k_hw_ipq8074_rx_desc_get_hdr_status,
	.rx_desc_encrypt_valid = ath11k_hw_ipq8074_rx_desc_encrypt_valid,
	.rx_desc_get_encrypt_type = ath11k_hw_ipq8074_rx_desc_get_encrypt_type,
	.rx_desc_get_decap_type = ath11k_hw_ipq8074_rx_desc_get_decap_type,
	.rx_desc_get_mesh_ctl = ath11k_hw_ipq8074_rx_desc_get_mesh_ctl,
	.rx_desc_get_ldpc_support = ath11k_hw_ipq8074_rx_desc_get_ldpc_support,
	.rx_desc_get_mpdu_seq_ctl_vld = ath11k_hw_ipq8074_rx_desc_get_mpdu_seq_ctl_vld,
	.rx_desc_get_mpdu_fc_valid = ath11k_hw_ipq8074_rx_desc_get_mpdu_fc_valid,
	.rx_desc_get_mpdu_start_seq_no = ath11k_hw_ipq8074_rx_desc_get_mpdu_start_seq_no,
	.rx_desc_get_msdu_len = ath11k_hw_ipq8074_rx_desc_get_msdu_len,
	.rx_desc_get_msdu_sgi = ath11k_hw_ipq8074_rx_desc_get_msdu_sgi,
	.rx_desc_get_msdu_rate_mcs = ath11k_hw_ipq8074_rx_desc_get_msdu_rate_mcs,
	.rx_desc_get_msdu_rx_bw = ath11k_hw_ipq8074_rx_desc_get_msdu_rx_bw,
	.rx_desc_get_msdu_freq = ath11k_hw_ipq8074_rx_desc_get_msdu_freq,
	.rx_desc_get_msdu_pkt_type = ath11k_hw_ipq8074_rx_desc_get_msdu_pkt_type,
	.rx_desc_get_msdu_nss = ath11k_hw_ipq8074_rx_desc_get_msdu_nss,
	.rx_desc_get_mpdu_tid = ath11k_hw_ipq8074_rx_desc_get_mpdu_tid,
	.rx_desc_get_mpdu_peer_id = ath11k_hw_ipq8074_rx_desc_get_mpdu_peer_id,
	.rx_desc_copy_attn_end_tlv = ath11k_hw_ipq8074_rx_desc_copy_attn_end,
	.rx_desc_get_mpdu_start_tag = ath11k_hw_ipq8074_rx_desc_get_mpdu_start_tag,
	.rx_desc_get_mpdu_ppdu_id = ath11k_hw_ipq8074_rx_desc_get_mpdu_ppdu_id,
	.rx_desc_set_msdu_len = ath11k_hw_ipq8074_rx_desc_set_msdu_len,
	.rx_desc_get_attention = ath11k_hw_ipq8074_rx_desc_get_attention,
	.rx_desc_get_msdu_payload = ath11k_hw_ipq8074_rx_desc_get_msdu_payload,
	.reo_setup = ath11k_hw_ipq8074_reo_setup,
	.mpdu_info_get_peerid = ath11k_hw_ipq8074_mpdu_info_get_peerid,
	.rx_desc_mac_addr2_valid = ath11k_hw_ipq8074_rx_desc_mac_addr2_valid,
	.rx_desc_mpdu_start_addr2 = ath11k_hw_ipq8074_rx_desc_mpdu_start_addr2,
	.get_ring_selector = ath11k_hw_ipq8074_get_tcl_ring_selector,
#endif
};

const struct ath11k_hw_ops qcn9074_ops = {
#if notyet
	.get_hw_mac_from_pdev_id = ath11k_hw_ipq6018_mac_from_pdev_id,
#endif
	.wmi_init_config = qwx_init_wmi_config_ipq8074,
#ifdef notyet
	.mac_id_to_pdev_id = ath11k_hw_mac_id_to_pdev_id_ipq8074,
	.mac_id_to_srng_id = ath11k_hw_mac_id_to_srng_id_ipq8074,
	.tx_mesh_enable = ath11k_hw_qcn9074_tx_mesh_enable,
	.rx_desc_get_first_msdu = ath11k_hw_qcn9074_rx_desc_get_first_msdu,
	.rx_desc_get_last_msdu = ath11k_hw_qcn9074_rx_desc_get_last_msdu,
	.rx_desc_get_l3_pad_bytes = ath11k_hw_qcn9074_rx_desc_get_l3_pad_bytes,
	.rx_desc_get_hdr_status = ath11k_hw_qcn9074_rx_desc_get_hdr_status,
	.rx_desc_encrypt_valid = ath11k_hw_qcn9074_rx_desc_encrypt_valid,
	.rx_desc_get_encrypt_type = ath11k_hw_qcn9074_rx_desc_get_encrypt_type,
	.rx_desc_get_decap_type = ath11k_hw_qcn9074_rx_desc_get_decap_type,
	.rx_desc_get_mesh_ctl = ath11k_hw_qcn9074_rx_desc_get_mesh_ctl,
	.rx_desc_get_ldpc_support = ath11k_hw_qcn9074_rx_desc_get_ldpc_support,
	.rx_desc_get_mpdu_seq_ctl_vld = ath11k_hw_qcn9074_rx_desc_get_mpdu_seq_ctl_vld,
	.rx_desc_get_mpdu_fc_valid = ath11k_hw_qcn9074_rx_desc_get_mpdu_fc_valid,
	.rx_desc_get_mpdu_start_seq_no = ath11k_hw_qcn9074_rx_desc_get_mpdu_start_seq_no,
	.rx_desc_get_msdu_len = ath11k_hw_qcn9074_rx_desc_get_msdu_len,
	.rx_desc_get_msdu_sgi = ath11k_hw_qcn9074_rx_desc_get_msdu_sgi,
	.rx_desc_get_msdu_rate_mcs = ath11k_hw_qcn9074_rx_desc_get_msdu_rate_mcs,
	.rx_desc_get_msdu_rx_bw = ath11k_hw_qcn9074_rx_desc_get_msdu_rx_bw,
	.rx_desc_get_msdu_freq = ath11k_hw_qcn9074_rx_desc_get_msdu_freq,
	.rx_desc_get_msdu_pkt_type = ath11k_hw_qcn9074_rx_desc_get_msdu_pkt_type,
	.rx_desc_get_msdu_nss = ath11k_hw_qcn9074_rx_desc_get_msdu_nss,
	.rx_desc_get_mpdu_tid = ath11k_hw_qcn9074_rx_desc_get_mpdu_tid,
	.rx_desc_get_mpdu_peer_id = ath11k_hw_qcn9074_rx_desc_get_mpdu_peer_id,
	.rx_desc_copy_attn_end_tlv = ath11k_hw_qcn9074_rx_desc_copy_attn_end,
	.rx_desc_get_mpdu_start_tag = ath11k_hw_qcn9074_rx_desc_get_mpdu_start_tag,
	.rx_desc_get_mpdu_ppdu_id = ath11k_hw_qcn9074_rx_desc_get_mpdu_ppdu_id,
	.rx_desc_set_msdu_len = ath11k_hw_qcn9074_rx_desc_set_msdu_len,
	.rx_desc_get_attention = ath11k_hw_qcn9074_rx_desc_get_attention,
	.rx_desc_get_msdu_payload = ath11k_hw_qcn9074_rx_desc_get_msdu_payload,
	.reo_setup = ath11k_hw_ipq8074_reo_setup,
	.mpdu_info_get_peerid = ath11k_hw_ipq8074_mpdu_info_get_peerid,
	.rx_desc_mac_addr2_valid = ath11k_hw_ipq9074_rx_desc_mac_addr2_valid,
	.rx_desc_mpdu_start_addr2 = ath11k_hw_ipq9074_rx_desc_mpdu_start_addr2,
	.get_ring_selector = ath11k_hw_ipq8074_get_tcl_ring_selector,
#endif
};

const struct ath11k_hw_ops wcn6855_ops = {
#if notyet
	.get_hw_mac_from_pdev_id = ath11k_hw_ipq8074_mac_from_pdev_id,
#endif
	.wmi_init_config = qwx_init_wmi_config_qca6390,
#ifdef notyet
	.mac_id_to_pdev_id = ath11k_hw_mac_id_to_pdev_id_qca6390,
	.mac_id_to_srng_id = ath11k_hw_mac_id_to_srng_id_qca6390,
	.tx_mesh_enable = ath11k_hw_wcn6855_tx_mesh_enable,
	.rx_desc_get_first_msdu = ath11k_hw_wcn6855_rx_desc_get_first_msdu,
	.rx_desc_get_last_msdu = ath11k_hw_wcn6855_rx_desc_get_last_msdu,
	.rx_desc_get_l3_pad_bytes = ath11k_hw_wcn6855_rx_desc_get_l3_pad_bytes,
	.rx_desc_get_hdr_status = ath11k_hw_wcn6855_rx_desc_get_hdr_status,
	.rx_desc_encrypt_valid = ath11k_hw_wcn6855_rx_desc_encrypt_valid,
	.rx_desc_get_encrypt_type = ath11k_hw_wcn6855_rx_desc_get_encrypt_type,
	.rx_desc_get_decap_type = ath11k_hw_wcn6855_rx_desc_get_decap_type,
	.rx_desc_get_mesh_ctl = ath11k_hw_wcn6855_rx_desc_get_mesh_ctl,
	.rx_desc_get_ldpc_support = ath11k_hw_wcn6855_rx_desc_get_ldpc_support,
	.rx_desc_get_mpdu_seq_ctl_vld = ath11k_hw_wcn6855_rx_desc_get_mpdu_seq_ctl_vld,
	.rx_desc_get_mpdu_fc_valid = ath11k_hw_wcn6855_rx_desc_get_mpdu_fc_valid,
	.rx_desc_get_mpdu_start_seq_no = ath11k_hw_wcn6855_rx_desc_get_mpdu_start_seq_no,
	.rx_desc_get_msdu_len = ath11k_hw_wcn6855_rx_desc_get_msdu_len,
	.rx_desc_get_msdu_sgi = ath11k_hw_wcn6855_rx_desc_get_msdu_sgi,
	.rx_desc_get_msdu_rate_mcs = ath11k_hw_wcn6855_rx_desc_get_msdu_rate_mcs,
	.rx_desc_get_msdu_rx_bw = ath11k_hw_wcn6855_rx_desc_get_msdu_rx_bw,
	.rx_desc_get_msdu_freq = ath11k_hw_wcn6855_rx_desc_get_msdu_freq,
	.rx_desc_get_msdu_pkt_type = ath11k_hw_wcn6855_rx_desc_get_msdu_pkt_type,
	.rx_desc_get_msdu_nss = ath11k_hw_wcn6855_rx_desc_get_msdu_nss,
	.rx_desc_get_mpdu_tid = ath11k_hw_wcn6855_rx_desc_get_mpdu_tid,
	.rx_desc_get_mpdu_peer_id = ath11k_hw_wcn6855_rx_desc_get_mpdu_peer_id,
	.rx_desc_copy_attn_end_tlv = ath11k_hw_wcn6855_rx_desc_copy_attn_end,
	.rx_desc_get_mpdu_start_tag = ath11k_hw_wcn6855_rx_desc_get_mpdu_start_tag,
	.rx_desc_get_mpdu_ppdu_id = ath11k_hw_wcn6855_rx_desc_get_mpdu_ppdu_id,
	.rx_desc_set_msdu_len = ath11k_hw_wcn6855_rx_desc_set_msdu_len,
	.rx_desc_get_attention = ath11k_hw_wcn6855_rx_desc_get_attention,
	.rx_desc_get_msdu_payload = ath11k_hw_wcn6855_rx_desc_get_msdu_payload,
	.reo_setup = ath11k_hw_wcn6855_reo_setup,
	.mpdu_info_get_peerid = ath11k_hw_wcn6855_mpdu_info_get_peerid,
	.rx_desc_mac_addr2_valid = ath11k_hw_wcn6855_rx_desc_mac_addr2_valid,
	.rx_desc_mpdu_start_addr2 = ath11k_hw_wcn6855_rx_desc_mpdu_start_addr2,
	.get_ring_selector = ath11k_hw_ipq8074_get_tcl_ring_selector,
#endif
};

const struct ath11k_hw_ops wcn6750_ops = {
#if notyet
	.get_hw_mac_from_pdev_id = ath11k_hw_ipq8074_mac_from_pdev_id,
#endif
	.wmi_init_config = qwx_init_wmi_config_qca6390,
#if notyet
	.mac_id_to_pdev_id = ath11k_hw_mac_id_to_pdev_id_qca6390,
	.mac_id_to_srng_id = ath11k_hw_mac_id_to_srng_id_qca6390,
	.tx_mesh_enable = ath11k_hw_qcn9074_tx_mesh_enable,
	.rx_desc_get_first_msdu = ath11k_hw_qcn9074_rx_desc_get_first_msdu,
	.rx_desc_get_last_msdu = ath11k_hw_qcn9074_rx_desc_get_last_msdu,
	.rx_desc_get_l3_pad_bytes = ath11k_hw_qcn9074_rx_desc_get_l3_pad_bytes,
	.rx_desc_get_hdr_status = ath11k_hw_qcn9074_rx_desc_get_hdr_status,
	.rx_desc_encrypt_valid = ath11k_hw_qcn9074_rx_desc_encrypt_valid,
	.rx_desc_get_encrypt_type = ath11k_hw_qcn9074_rx_desc_get_encrypt_type,
	.rx_desc_get_decap_type = ath11k_hw_qcn9074_rx_desc_get_decap_type,
	.rx_desc_get_mesh_ctl = ath11k_hw_qcn9074_rx_desc_get_mesh_ctl,
	.rx_desc_get_ldpc_support = ath11k_hw_qcn9074_rx_desc_get_ldpc_support,
	.rx_desc_get_mpdu_seq_ctl_vld = ath11k_hw_qcn9074_rx_desc_get_mpdu_seq_ctl_vld,
	.rx_desc_get_mpdu_fc_valid = ath11k_hw_qcn9074_rx_desc_get_mpdu_fc_valid,
	.rx_desc_get_mpdu_start_seq_no = ath11k_hw_qcn9074_rx_desc_get_mpdu_start_seq_no,
	.rx_desc_get_msdu_len = ath11k_hw_qcn9074_rx_desc_get_msdu_len,
	.rx_desc_get_msdu_sgi = ath11k_hw_qcn9074_rx_desc_get_msdu_sgi,
	.rx_desc_get_msdu_rate_mcs = ath11k_hw_qcn9074_rx_desc_get_msdu_rate_mcs,
	.rx_desc_get_msdu_rx_bw = ath11k_hw_qcn9074_rx_desc_get_msdu_rx_bw,
	.rx_desc_get_msdu_freq = ath11k_hw_qcn9074_rx_desc_get_msdu_freq,
	.rx_desc_get_msdu_pkt_type = ath11k_hw_qcn9074_rx_desc_get_msdu_pkt_type,
	.rx_desc_get_msdu_nss = ath11k_hw_qcn9074_rx_desc_get_msdu_nss,
	.rx_desc_get_mpdu_tid = ath11k_hw_qcn9074_rx_desc_get_mpdu_tid,
	.rx_desc_get_mpdu_peer_id = ath11k_hw_qcn9074_rx_desc_get_mpdu_peer_id,
	.rx_desc_copy_attn_end_tlv = ath11k_hw_qcn9074_rx_desc_copy_attn_end,
	.rx_desc_get_mpdu_start_tag = ath11k_hw_qcn9074_rx_desc_get_mpdu_start_tag,
	.rx_desc_get_mpdu_ppdu_id = ath11k_hw_qcn9074_rx_desc_get_mpdu_ppdu_id,
	.rx_desc_set_msdu_len = ath11k_hw_qcn9074_rx_desc_set_msdu_len,
	.rx_desc_get_attention = ath11k_hw_qcn9074_rx_desc_get_attention,
	.rx_desc_get_msdu_payload = ath11k_hw_qcn9074_rx_desc_get_msdu_payload,
	.reo_setup = ath11k_hw_wcn6855_reo_setup,
	.mpdu_info_get_peerid = ath11k_hw_ipq8074_mpdu_info_get_peerid,
	.rx_desc_mac_addr2_valid = ath11k_hw_ipq9074_rx_desc_mac_addr2_valid,
	.rx_desc_mpdu_start_addr2 = ath11k_hw_ipq9074_rx_desc_mpdu_start_addr2,
	.get_ring_selector = ath11k_hw_wcn6750_get_tcl_ring_selector,
#endif
};

#define ATH11K_TX_RING_MASK_0 BIT(0)
#define ATH11K_TX_RING_MASK_1 BIT(1)
#define ATH11K_TX_RING_MASK_2 BIT(2)
#define ATH11K_TX_RING_MASK_3 BIT(3)
#define ATH11K_TX_RING_MASK_4 BIT(4)

#define ATH11K_RX_RING_MASK_0 0x1
#define ATH11K_RX_RING_MASK_1 0x2
#define ATH11K_RX_RING_MASK_2 0x4
#define ATH11K_RX_RING_MASK_3 0x8

#define ATH11K_RX_ERR_RING_MASK_0 0x1

#define ATH11K_RX_WBM_REL_RING_MASK_0 0x1

#define ATH11K_REO_STATUS_RING_MASK_0 0x1

#define ATH11K_RXDMA2HOST_RING_MASK_0 0x1
#define ATH11K_RXDMA2HOST_RING_MASK_1 0x2
#define ATH11K_RXDMA2HOST_RING_MASK_2 0x4

#define ATH11K_HOST2RXDMA_RING_MASK_0 0x1
#define ATH11K_HOST2RXDMA_RING_MASK_1 0x2
#define ATH11K_HOST2RXDMA_RING_MASK_2 0x4

#define ATH11K_RX_MON_STATUS_RING_MASK_0 0x1
#define ATH11K_RX_MON_STATUS_RING_MASK_1 0x2
#define ATH11K_RX_MON_STATUS_RING_MASK_2 0x4

const struct ath11k_hw_ring_mask ath11k_hw_ring_mask_ipq8074 = {
	.tx  = {
		ATH11K_TX_RING_MASK_0,
		ATH11K_TX_RING_MASK_1,
		ATH11K_TX_RING_MASK_2,
	},
	.rx_mon_status = {
		0, 0, 0, 0,
		ATH11K_RX_MON_STATUS_RING_MASK_0,
		ATH11K_RX_MON_STATUS_RING_MASK_1,
		ATH11K_RX_MON_STATUS_RING_MASK_2,
	},
	.rx = {
		0, 0, 0, 0, 0, 0, 0,
		ATH11K_RX_RING_MASK_0,
		ATH11K_RX_RING_MASK_1,
		ATH11K_RX_RING_MASK_2,
		ATH11K_RX_RING_MASK_3,
	},
	.rx_err = {
		ATH11K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		ATH11K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		ATH11K_REO_STATUS_RING_MASK_0,
	},
	.rxdma2host = {
		ATH11K_RXDMA2HOST_RING_MASK_0,
		ATH11K_RXDMA2HOST_RING_MASK_1,
		ATH11K_RXDMA2HOST_RING_MASK_2,
	},
	.host2rxdma = {
		ATH11K_HOST2RXDMA_RING_MASK_0,
		ATH11K_HOST2RXDMA_RING_MASK_1,
		ATH11K_HOST2RXDMA_RING_MASK_2,
	},
};

const struct ath11k_hw_ring_mask ath11k_hw_ring_mask_qca6390 = {
	.tx  = {
		ATH11K_TX_RING_MASK_0,
	},
	.rx_mon_status = {
		0, 0, 0, 0,
		ATH11K_RX_MON_STATUS_RING_MASK_0,
		ATH11K_RX_MON_STATUS_RING_MASK_1,
		ATH11K_RX_MON_STATUS_RING_MASK_2,
	},
	.rx = {
		0, 0, 0, 0, 0, 0, 0,
		ATH11K_RX_RING_MASK_0,
		ATH11K_RX_RING_MASK_1,
		ATH11K_RX_RING_MASK_2,
		ATH11K_RX_RING_MASK_3,
	},
	.rx_err = {
		ATH11K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		ATH11K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		ATH11K_REO_STATUS_RING_MASK_0,
	},
	.rxdma2host = {
		ATH11K_RXDMA2HOST_RING_MASK_0,
		ATH11K_RXDMA2HOST_RING_MASK_1,
		ATH11K_RXDMA2HOST_RING_MASK_2,
	},
	.host2rxdma = {
	},
};

const struct ath11k_hw_ring_mask ath11k_hw_ring_mask_qcn9074 = {
	.tx  = {
		ATH11K_TX_RING_MASK_0,
		ATH11K_TX_RING_MASK_1,
		ATH11K_TX_RING_MASK_2,
	},
	.rx_mon_status = {
		0, 0, 0,
		ATH11K_RX_MON_STATUS_RING_MASK_0,
		ATH11K_RX_MON_STATUS_RING_MASK_1,
		ATH11K_RX_MON_STATUS_RING_MASK_2,
	},
	.rx = {
		0, 0, 0, 0,
		ATH11K_RX_RING_MASK_0,
		ATH11K_RX_RING_MASK_1,
		ATH11K_RX_RING_MASK_2,
		ATH11K_RX_RING_MASK_3,
	},
	.rx_err = {
		0, 0, 0,
		ATH11K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		0, 0, 0,
		ATH11K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		0, 0, 0,
		ATH11K_REO_STATUS_RING_MASK_0,
	},
	.rxdma2host = {
		0, 0, 0,
		ATH11K_RXDMA2HOST_RING_MASK_0,
	},
	.host2rxdma = {
		0, 0, 0,
		ATH11K_HOST2RXDMA_RING_MASK_0,
	},
};

const struct ath11k_hw_ring_mask ath11k_hw_ring_mask_wcn6750 = {
	.tx  = {
		ATH11K_TX_RING_MASK_0,
		0,
		ATH11K_TX_RING_MASK_2,
		0,
		ATH11K_TX_RING_MASK_4,
	},
	.rx_mon_status = {
		0, 0, 0, 0, 0, 0,
		ATH11K_RX_MON_STATUS_RING_MASK_0,
	},
	.rx = {
		0, 0, 0, 0, 0, 0, 0,
		ATH11K_RX_RING_MASK_0,
		ATH11K_RX_RING_MASK_1,
		ATH11K_RX_RING_MASK_2,
		ATH11K_RX_RING_MASK_3,
	},
	.rx_err = {
		0, ATH11K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		0, ATH11K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		0, ATH11K_REO_STATUS_RING_MASK_0,
	},
	.rxdma2host = {
		ATH11K_RXDMA2HOST_RING_MASK_0,
		ATH11K_RXDMA2HOST_RING_MASK_1,
		ATH11K_RXDMA2HOST_RING_MASK_2,
	},
	.host2rxdma = {
	},
};

/* Target firmware's Copy Engine configuration. */
const struct ce_pipe_config ath11k_target_ce_config_wlan_ipq8074[] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.pipenum = htole32(0),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE1: target->host HTT + HTC control */
	{
		.pipenum = htole32(1),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE2: target->host WMI */
	{
		.pipenum = htole32(2),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE3: host->target WMI */
	{
		.pipenum = htole32(3),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE4: host->target HTT */
	{
		.pipenum = htole32(4),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(256),
		.nbytes_max = htole32(256),
		.flags = htole32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = htole32(0),
	},

	/* CE5: target->host Pktlog */
	{
		.pipenum = htole32(5),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(0),
		.reserved = htole32(0),
	},

	/* CE6: Reserved for target autonomous hif_memcpy */
	{
		.pipenum = htole32(6),
		.pipedir = htole32(PIPEDIR_INOUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(65535),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE7 used only by Host */
	{
		.pipenum = htole32(7),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE8 target->host used only by IPA */
	{
		.pipenum = htole32(8),
		.pipedir = htole32(PIPEDIR_INOUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(65535),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE9 host->target HTT */
	{
		.pipenum = htole32(9),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE10 target->host HTT */
	{
		.pipenum = htole32(10),
		.pipedir = htole32(PIPEDIR_INOUT_H2H),
		.nentries = htole32(0),
		.nbytes_max = htole32(0),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE11 Not used */
};

/* Map from service/endpoint to Copy Engine.
 * This table is derived from the CE_PCI TABLE, above.
 * It is passed to the Target at startup for use by firmware.
 */
const struct service_to_pipe ath11k_target_service_to_ce_map_wlan_ipq8074[] = {
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(7),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC2),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(9),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC2),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(0),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(1),
	},
	{ /* not used */
		.service_id = htole32(ATH11K_HTC_SVC_ID_TEST_RAW_STREAMS),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(0),
	},
	{ /* not used */
		.service_id = htole32(ATH11K_HTC_SVC_ID_TEST_RAW_STREAMS),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(1),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(4),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(1),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_PKT_LOG),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(5),
	},

	/* (Additions here) */

	{ /* terminator entry */ }
};

const struct service_to_pipe ath11k_target_service_to_ce_map_wlan_ipq6018[] = {
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(3),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(7),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(2),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(0),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(1),
	},
	{ /* not used */
		.service_id = htole32(ATH11K_HTC_SVC_ID_TEST_RAW_STREAMS),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(0),
	},
	{ /* not used */
		.service_id = htole32(ATH11K_HTC_SVC_ID_TEST_RAW_STREAMS),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(1),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		.pipedir = htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		.pipenum = htole32(4),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(1),
	},
	{
		.service_id = htole32(ATH11K_HTC_SVC_ID_PKT_LOG),
		.pipedir = htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		.pipenum = htole32(5),
	},

	/* (Additions here) */

	{ /* terminator entry */ }
};

/* Target firmware's Copy Engine configuration. */
const struct ce_pipe_config ath11k_target_ce_config_wlan_qca6390[] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.pipenum = htole32(0),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE1: target->host HTT + HTC control */
	{
		.pipenum = htole32(1),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE2: target->host WMI */
	{
		.pipenum = htole32(2),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE3: host->target WMI */
	{
		.pipenum = htole32(3),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE4: host->target HTT */
	{
		.pipenum = htole32(4),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(256),
		.nbytes_max = htole32(256),
		.flags = htole32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = htole32(0),
	},

	/* CE5: target->host Pktlog */
	{
		.pipenum = htole32(5),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE6: Reserved for target autonomous hif_memcpy */
	{
		.pipenum = htole32(6),
		.pipedir = htole32(PIPEDIR_INOUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(16384),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE7 used only by Host */
	{
		.pipenum = htole32(7),
		.pipedir = htole32(PIPEDIR_INOUT_H2H),
		.nentries = htole32(0),
		.nbytes_max = htole32(0),
		.flags = htole32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = htole32(0),
	},

	/* CE8 target->host used only by IPA */
	{
		.pipenum = htole32(8),
		.pipedir = htole32(PIPEDIR_INOUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(16384),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},
	/* CE 9, 10, 11 are used by MHI driver */
};

/* Map from service/endpoint to Copy Engine.
 * This table is derived from the CE_PCI TABLE, above.
 * It is passed to the Target at startup for use by firmware.
 */
const struct service_to_pipe ath11k_target_service_to_ce_map_wlan_qca6390[] = {
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(0),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(4),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(1),
	},

	/* (Additions here) */

	{ /* must be last */
		htole32(0),
		htole32(0),
		htole32(0),
	},
};

/* Target firmware's Copy Engine configuration. */
const struct ce_pipe_config ath11k_target_ce_config_wlan_qcn9074[] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.pipenum = htole32(0),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE1: target->host HTT + HTC control */
	{
		.pipenum = htole32(1),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE2: target->host WMI */
	{
		.pipenum = htole32(2),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE3: host->target WMI */
	{
		.pipenum = htole32(3),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE4: host->target HTT */
	{
		.pipenum = htole32(4),
		.pipedir = htole32(PIPEDIR_OUT),
		.nentries = htole32(256),
		.nbytes_max = htole32(256),
		.flags = htole32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = htole32(0),
	},

	/* CE5: target->host Pktlog */
	{
		.pipenum = htole32(5),
		.pipedir = htole32(PIPEDIR_IN),
		.nentries = htole32(32),
		.nbytes_max = htole32(2048),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE6: Reserved for target autonomous hif_memcpy */
	{
		.pipenum = htole32(6),
		.pipedir = htole32(PIPEDIR_INOUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(16384),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},

	/* CE7 used only by Host */
	{
		.pipenum = htole32(7),
		.pipedir = htole32(PIPEDIR_INOUT_H2H),
		.nentries = htole32(0),
		.nbytes_max = htole32(0),
		.flags = htole32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = htole32(0),
	},

	/* CE8 target->host used only by IPA */
	{
		.pipenum = htole32(8),
		.pipedir = htole32(PIPEDIR_INOUT),
		.nentries = htole32(32),
		.nbytes_max = htole32(16384),
		.flags = htole32(CE_ATTR_FLAGS),
		.reserved = htole32(0),
	},
	/* CE 9, 10, 11 are used by MHI driver */
};

/* Map from service/endpoint to Copy Engine.
 * This table is derived from the CE_PCI TABLE, above.
 * It is passed to the Target at startup for use by firmware.
 */
const struct service_to_pipe ath11k_target_service_to_ce_map_wlan_qcn9074[] = {
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(3),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(2),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(0),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(1),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_TEST_RAW_STREAMS),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(0),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_TEST_RAW_STREAMS),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(1),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		htole32(PIPEDIR_OUT),	/* out = UL = host -> target */
		htole32(4),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(1),
	},
	{
		htole32(ATH11K_HTC_SVC_ID_PKT_LOG),
		htole32(PIPEDIR_IN),	/* in = DL = target -> host */
		htole32(5),
	},

	/* (Additions here) */

	{ /* must be last */
		htole32(0),
		htole32(0),
		htole32(0),
	},
};

#define QWX_CE_COUNT_IPQ8074	21

const struct ce_attr qwx_host_ce_config_ipq8074[QWX_CE_COUNT_IPQ8074] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 16,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = qwx_htc_tx_completion_handler,
	},

	/* CE1: target->host HTT + HTC control */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_htc_rx_completion_handler,
	},

	/* CE2: target->host WMI */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_htc_rx_completion_handler,
	},

	/* CE3: host->target WMI (mac0) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = qwx_htc_tx_completion_handler,
	},

	/* CE4: host->target HTT */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 2048,
		.src_sz_max = 256,
		.dest_nentries = 0,
	},

	/* CE5: target->host pktlog */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_dp_htt_htc_t2h_msg_handler,
	},

	/* CE6: target autonomous hif_memcpy */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE7: host->target WMI (mac1) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = qwx_htc_tx_completion_handler,
	},

	/* CE8: target autonomous hif_memcpy */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE9: host->target WMI (mac2) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = qwx_htc_tx_completion_handler,
	},

	/* CE10: target->host HTT */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_htc_rx_completion_handler,
	},

	/* CE11: Not used */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},
};

#define QWX_CE_COUNT_QCA6390	9

const struct ce_attr qwx_host_ce_config_qca6390[QWX_CE_COUNT_QCA6390] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 16,
		.src_sz_max = 2048,
		.dest_nentries = 0,
	},

	/* CE1: target->host HTT + HTC control */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_htc_rx_completion_handler,
	},

	/* CE2: target->host WMI */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_htc_rx_completion_handler,
	},

	/* CE3: host->target WMI (mac0) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = qwx_htc_tx_completion_handler,
	},

	/* CE4: host->target HTT */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 2048,
		.src_sz_max = 256,
		.dest_nentries = 0,
	},

	/* CE5: target->host pktlog */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_dp_htt_htc_t2h_msg_handler,
	},

	/* CE6: target autonomous hif_memcpy */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE7: host->target WMI (mac1) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = qwx_htc_tx_completion_handler,
	},

	/* CE8: target autonomous hif_memcpy */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

};

#define QWX_CE_COUNT_QCN9074	6

const struct ce_attr qwx_host_ce_config_qcn9074[QWX_CE_COUNT_QCN9074] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 16,
		.src_sz_max = 2048,
		.dest_nentries = 0,
	},

	/* CE1: target->host HTT + HTC control */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_htc_rx_completion_handler,
	},

	/* CE2: target->host WMI */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 32,
		.recv_cb = qwx_htc_rx_completion_handler,
	},

	/* CE3: host->target WMI (mac0) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = qwx_htc_tx_completion_handler,
	},

	/* CE4: host->target HTT */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 2048,
		.src_sz_max = 256,
		.dest_nentries = 0,
	},

	/* CE5: target->host pktlog */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = qwx_dp_htt_htc_t2h_msg_handler,
	},
};

static const struct ath11k_hw_tcl2wbm_rbm_map ath11k_hw_tcl2wbm_rbm_map_ipq8074[] = {
	{
		.tcl_ring_num = 0,
		.wbm_ring_num = 0,
		.rbm_id = HAL_RX_BUF_RBM_SW0_BM,
	},
	{
		.tcl_ring_num = 1,
		.wbm_ring_num = 1,
		.rbm_id = HAL_RX_BUF_RBM_SW1_BM,
	},
	{
		.tcl_ring_num = 2,
		.wbm_ring_num = 2,
		.rbm_id = HAL_RX_BUF_RBM_SW2_BM,
	},
};

static const struct ath11k_hw_tcl2wbm_rbm_map ath11k_hw_tcl2wbm_rbm_map_wcn6750[] = {
	{
		.tcl_ring_num = 0,
		.wbm_ring_num = 0,
		.rbm_id = HAL_RX_BUF_RBM_SW0_BM,
	},
	{
		.tcl_ring_num = 1,
		.wbm_ring_num = 4,
		.rbm_id = HAL_RX_BUF_RBM_SW4_BM,
	},
	{
		.tcl_ring_num = 2,
		.wbm_ring_num = 2,
		.rbm_id = HAL_RX_BUF_RBM_SW2_BM,
	},
};


static const struct ath11k_hw_hal_params ath11k_hw_hal_params_ipq8074 = {
	.rx_buf_rbm = HAL_RX_BUF_RBM_SW3_BM,
	.tcl2wbm_rbm_map = ath11k_hw_tcl2wbm_rbm_map_ipq8074,
};

static const struct ath11k_hw_hal_params ath11k_hw_hal_params_qca6390 = {
	.rx_buf_rbm = HAL_RX_BUF_RBM_SW1_BM,
	.tcl2wbm_rbm_map = ath11k_hw_tcl2wbm_rbm_map_ipq8074,
};

static const struct ath11k_hw_hal_params ath11k_hw_hal_params_wcn6750 = {
	.rx_buf_rbm = HAL_RX_BUF_RBM_SW1_BM,
	.tcl2wbm_rbm_map = ath11k_hw_tcl2wbm_rbm_map_wcn6750,
};

static const struct ath11k_hw_params ath11k_hw_params[] = {
	{
		.hw_rev = ATH11K_HW_IPQ8074,
		.name = "ipq8074 hw2.0",
		.fw = {
			.dir = "IPQ8074/hw2.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
		},
		.max_radios = 3,
		.bdf_addr = 0x4B0C0000,
		.hw_ops = &ipq8074_ops,
		.ring_mask = &ath11k_hw_ring_mask_ipq8074,
		.internal_sleep_clock = false,
		.regs = &ipq8074_regs,
		.qmi_service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ8074,
		.host_ce_config = qwx_host_ce_config_ipq8074,
		.ce_count = QWX_CE_COUNT_IPQ8074,
		.target_ce_config = ath11k_target_ce_config_wlan_ipq8074,
		.target_ce_count = 11,
		.svc_to_ce_map = ath11k_target_service_to_ce_map_wlan_ipq8074,
		.svc_to_ce_map_len = 21,
		.single_pdev_only = false,
		.rxdma1_enable = true,
		.num_rxmda_per_pdev = 1,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,
		.htt_peer_map_v2 = true,
#if notyet
		.spectral = {
			.fft_sz = 2,
			/* HW bug, expected BIN size is 2 bytes but HW report as 4 bytes.
			 * so added pad size as 2 bytes to compensate the BIN size
			 */
			.fft_pad_sz = 2,
			.summary_pad_sz = 0,
			.fft_hdr_len = 16,
			.max_fft_bins = 512,
			.fragment_160mhz = true,
		},

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
		.supports_monitor = true,
		.full_monitor_mode = false,
#endif
		.supports_shadow_regs = false,
		.idle_ps = false,
		.supports_sta_ps = false,
		.cold_boot_calib = true,
		.cbcal_restart_fw = true,
		.fw_mem_mode = 0,
		.num_vdevs = 16 + 1,
		.num_peers = 512,
		.supports_suspend = false,
		.hal_desc_sz = sizeof(struct hal_rx_desc_ipq8074),
		.supports_regdb = false,
		.fix_l1ss = true,
		.credit_flow = false,
		.max_tx_ring = DP_TCL_NUM_RING_MAX,
		.hal_params = &ath11k_hw_hal_params_ipq8074,
#if notyet
		.supports_dynamic_smps_6ghz = false,
		.alloc_cacheable_memory = true,
		.supports_rssi_stats = false,
#endif
		.fw_wmi_diag_event = false,
		.current_cc_support = false,
		.dbr_debug_support = true,
		.global_reset = false,
#ifdef notyet
		.bios_sar_capa = NULL,
#endif
		.m3_fw_support = false,
		.fixed_bdf_addr = true,
		.fixed_mem_region = true,
		.static_window_map = false,
#if notyet
		.hybrid_bus_type = false,
		.fixed_fw_mem = false,
		.support_off_channel_tx = false,
		.supports_multi_bssid = false,

		.sram_dump = {},

		.tcl_ring_retry = true,
		.tx_ring_size = DP_TCL_DATA_RING_SIZE,
		.smp2p_wow_exit = false,
#endif
	},
	{
		.hw_rev = ATH11K_HW_IPQ6018_HW10,
		.name = "ipq6018 hw1.0",
		.fw = {
			.dir = "IPQ6018/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
		},
		.max_radios = 2,
		.bdf_addr = 0x4ABC0000,
		.hw_ops = &ipq6018_ops,
		.ring_mask = &ath11k_hw_ring_mask_ipq8074,
		.internal_sleep_clock = false,
		.regs = &ipq8074_regs,
		.qmi_service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ8074,
		.host_ce_config = qwx_host_ce_config_ipq8074,
		.ce_count = QWX_CE_COUNT_IPQ8074,
		.target_ce_config = ath11k_target_ce_config_wlan_ipq8074,
		.target_ce_count = 11,
		.svc_to_ce_map = ath11k_target_service_to_ce_map_wlan_ipq6018,
		.svc_to_ce_map_len = 19,
		.single_pdev_only = false,
		.rxdma1_enable = true,
		.num_rxmda_per_pdev = 1,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,
		.htt_peer_map_v2 = true,
#if notyet
		.spectral = {
			.fft_sz = 4,
			.fft_pad_sz = 0,
			.summary_pad_sz = 0,
			.fft_hdr_len = 16,
			.max_fft_bins = 512,
			.fragment_160mhz = true,
		},

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
		.supports_monitor = true,
		.full_monitor_mode = false,
#endif
		.supports_shadow_regs = false,
		.idle_ps = false,
		.supports_sta_ps = false,
		.cold_boot_calib = true,
		.cbcal_restart_fw = true,
		.fw_mem_mode = 0,
		.num_vdevs = 16 + 1,
		.num_peers = 512,
		.supports_suspend = false,
		.hal_desc_sz = sizeof(struct hal_rx_desc_ipq8074),
		.supports_regdb = false,
		.fix_l1ss = true,
		.credit_flow = false,
		.max_tx_ring = DP_TCL_NUM_RING_MAX,
		.hal_params = &ath11k_hw_hal_params_ipq8074,
#if notyet
		.supports_dynamic_smps_6ghz = false,
		.alloc_cacheable_memory = true,
		.supports_rssi_stats = false,
#endif
		.fw_wmi_diag_event = false,
		.current_cc_support = false,
		.dbr_debug_support = true,
		.global_reset = false,
#ifdef notyet
		.bios_sar_capa = NULL,
#endif
		.m3_fw_support = false,
		.fixed_bdf_addr = true,
		.fixed_mem_region = true,
		.static_window_map = false,
		.hybrid_bus_type = false,
		.fixed_fw_mem = false,
#if notyet
		.support_off_channel_tx = false,
		.supports_multi_bssid = false,

		.sram_dump = {},

		.tcl_ring_retry = true,
		.tx_ring_size = DP_TCL_DATA_RING_SIZE,
		.smp2p_wow_exit = false,
#endif
	},
	{
		.name = "qca6390 hw2.0",
		.hw_rev = ATH11K_HW_QCA6390_HW20,
		.fw = {
			.dir = "QCA6390/hw2.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
		},
		.max_radios = 3,
		.bdf_addr = 0x4B0C0000,
		.hw_ops = &qca6390_ops,
		.ring_mask = &ath11k_hw_ring_mask_qca6390,
		.internal_sleep_clock = true,
		.regs = &qca6390_regs,
		.qmi_service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCA6390,
		.host_ce_config = qwx_host_ce_config_qca6390,
		.ce_count = QWX_CE_COUNT_QCA6390,
		.target_ce_config = ath11k_target_ce_config_wlan_qca6390,
		.target_ce_count = 9,
		.svc_to_ce_map = ath11k_target_service_to_ce_map_wlan_qca6390,
		.svc_to_ce_map_len = 14,
		.single_pdev_only = true,
		.rxdma1_enable = false,
		.num_rxmda_per_pdev = 2,
		.rx_mac_buf_ring = true,
		.vdev_start_delay = true,
		.htt_peer_map_v2 = false,
#if notyet
		.spectral = {
			.fft_sz = 0,
			.fft_pad_sz = 0,
			.summary_pad_sz = 0,
			.fft_hdr_len = 0,
			.max_fft_bins = 0,
			.fragment_160mhz = false,
		},

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP),
		.supports_monitor = false,
		.full_monitor_mode = false,
#endif
		.supports_shadow_regs = true,
		.idle_ps = true,
		.supports_sta_ps = true,
		.cold_boot_calib = false,
		.cbcal_restart_fw = false,
		.fw_mem_mode = 0,
		.num_vdevs = 16 + 1,
		.num_peers = 512,
		.supports_suspend = true,
		.hal_desc_sz = sizeof(struct hal_rx_desc_ipq8074),
		.supports_regdb = false,
		.fix_l1ss = true,
		.credit_flow = true,
		.max_tx_ring = DP_TCL_NUM_RING_MAX_QCA6390,
		.hal_params = &ath11k_hw_hal_params_qca6390,
#if notyet
		.supports_dynamic_smps_6ghz = false,
		.alloc_cacheable_memory = false,
		.supports_rssi_stats = true,
#endif
		.fw_wmi_diag_event = true,
		.current_cc_support = true,
		.dbr_debug_support = false,
		.global_reset = true,
#ifdef notyet
		.bios_sar_capa = NULL,
#endif
		.m3_fw_support = true,
		.fixed_bdf_addr = false,
		.fixed_mem_region = false,
		.static_window_map = false,
		.hybrid_bus_type = false,
		.fixed_fw_mem = false,
#if notyet
		.support_off_channel_tx = true,
		.supports_multi_bssid = true,

		.sram_dump = {
			.start = 0x01400000,
			.end = 0x0171ffff,
		},

		.tcl_ring_retry = true,
		.tx_ring_size = DP_TCL_DATA_RING_SIZE,
		.smp2p_wow_exit = false,
#endif
	},
	{
		.name = "qcn9074 hw1.0",
		.hw_rev = ATH11K_HW_QCN9074_HW10,
		.fw = {
			.dir = "QCN9074/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
		},
		.max_radios = 1,
#if notyet
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9074,
#endif
		.hw_ops = &qcn9074_ops,
		.ring_mask = &ath11k_hw_ring_mask_qcn9074,
		.internal_sleep_clock = false,
		.regs = &qcn9074_regs,
		.host_ce_config = qwx_host_ce_config_qcn9074,
		.ce_count = QWX_CE_COUNT_QCN9074,
		.target_ce_config = ath11k_target_ce_config_wlan_qcn9074,
		.target_ce_count = 9,
		.svc_to_ce_map = ath11k_target_service_to_ce_map_wlan_qcn9074,
		.svc_to_ce_map_len = 18,
		.rxdma1_enable = true,
		.num_rxmda_per_pdev = 1,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,
		.htt_peer_map_v2 = true,
#if notyet
		.spectral = {
			.fft_sz = 2,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 1024,
			.fragment_160mhz = false,
		},

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
		.supports_monitor = true,
		.full_monitor_mode = true,
#endif
		.supports_shadow_regs = false,
		.idle_ps = false,
		.supports_sta_ps = false,
		.cold_boot_calib = false,
		.cbcal_restart_fw = false,
		.fw_mem_mode = 2,
		.num_vdevs = 8,
		.num_peers = 128,
		.supports_suspend = false,
		.hal_desc_sz = sizeof(struct hal_rx_desc_qcn9074),
		.supports_regdb = false,
		.fix_l1ss = true,
		.credit_flow = false,
		.max_tx_ring = DP_TCL_NUM_RING_MAX,
		.hal_params = &ath11k_hw_hal_params_ipq8074,
#if notyet
		.supports_dynamic_smps_6ghz = true,
		.alloc_cacheable_memory = true,
		.supports_rssi_stats = false,
#endif
		.fw_wmi_diag_event = false,
		.current_cc_support = false,
		.dbr_debug_support = true,
		.global_reset = false,
#ifdef notyet
		.bios_sar_capa = NULL,
#endif
		.m3_fw_support = true,
		.fixed_bdf_addr = false,
		.fixed_mem_region = false,
		.static_window_map = true,
		.hybrid_bus_type = false,
		.fixed_fw_mem = false,
#if notyet
		.support_off_channel_tx = false,
		.supports_multi_bssid = false,

		.sram_dump = {},

		.tcl_ring_retry = true,
		.tx_ring_size = DP_TCL_DATA_RING_SIZE,
		.smp2p_wow_exit = false,
#endif
	},
	{
		.name = "wcn6855 hw2.0",
		.hw_rev = ATH11K_HW_WCN6855_HW20,
		.fw = {
			.dir = "WCN6855/hw2.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
		},
		.max_radios = 3,
		.bdf_addr = 0x4B0C0000,
		.hw_ops = &wcn6855_ops,
		.ring_mask = &ath11k_hw_ring_mask_qca6390,
		.internal_sleep_clock = true,
		.regs = &wcn6855_regs,
		.qmi_service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCA6390,
		.host_ce_config = qwx_host_ce_config_qca6390,
		.ce_count = QWX_CE_COUNT_QCA6390,
		.target_ce_config = ath11k_target_ce_config_wlan_qca6390,
		.target_ce_count = 9,
		.svc_to_ce_map = ath11k_target_service_to_ce_map_wlan_qca6390,
		.svc_to_ce_map_len = 14,
		.single_pdev_only = true,
		.rxdma1_enable = false,
		.num_rxmda_per_pdev = 2,
		.rx_mac_buf_ring = true,
		.vdev_start_delay = true,
		.htt_peer_map_v2 = false,
#if notyet
		.spectral = {
			.fft_sz = 0,
			.fft_pad_sz = 0,
			.summary_pad_sz = 0,
			.fft_hdr_len = 0,
			.max_fft_bins = 0,
			.fragment_160mhz = false,
		},

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP),
		.supports_monitor = false,
		.full_monitor_mode = false,
#endif
		.supports_shadow_regs = true,
		.idle_ps = true,
		.supports_sta_ps = true,
		.cold_boot_calib = false,
		.cbcal_restart_fw = false,
		.fw_mem_mode = 0,
		.num_vdevs = 16 + 1,
		.num_peers = 512,
		.supports_suspend = true,
		.hal_desc_sz = sizeof(struct hal_rx_desc_wcn6855),
		.supports_regdb = true,
		.fix_l1ss = false,
		.credit_flow = true,
		.max_tx_ring = DP_TCL_NUM_RING_MAX_QCA6390,
		.hal_params = &ath11k_hw_hal_params_qca6390,
#if notyet
		.supports_dynamic_smps_6ghz = false,
		.alloc_cacheable_memory = false,
		.supports_rssi_stats = true,
#endif
		.fw_wmi_diag_event = true,
		.current_cc_support = true,
		.dbr_debug_support = false,
		.global_reset = true,
#ifdef notyet
		.bios_sar_capa = &ath11k_hw_sar_capa_wcn6855,
#endif
		.m3_fw_support = true,
		.fixed_bdf_addr = false,
		.fixed_mem_region = false,
		.static_window_map = false,
		.hybrid_bus_type = false,
		.fixed_fw_mem = false,
#if notyet
		.support_off_channel_tx = true,
		.supports_multi_bssid = true,

		.sram_dump = {
			.start = 0x01400000,
			.end = 0x0177ffff,
		},

		.tcl_ring_retry = true,
		.tx_ring_size = DP_TCL_DATA_RING_SIZE,
		.smp2p_wow_exit = false,
#endif
	},
	{
		.name = "wcn6855 hw2.1",
		.hw_rev = ATH11K_HW_WCN6855_HW21,
		.fw = {
			.dir = "WCN6855/hw2.1",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
		},
		.max_radios = 3,
		.bdf_addr = 0x4B0C0000,
		.hw_ops = &wcn6855_ops,
		.ring_mask = &ath11k_hw_ring_mask_qca6390,
		.internal_sleep_clock = true,
		.regs = &wcn6855_regs,
		.qmi_service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCA6390,
		.host_ce_config = qwx_host_ce_config_qca6390,
		.ce_count = QWX_CE_COUNT_QCA6390,
		.target_ce_config = ath11k_target_ce_config_wlan_qca6390,
		.target_ce_count = 9,
		.svc_to_ce_map = ath11k_target_service_to_ce_map_wlan_qca6390,
		.svc_to_ce_map_len = 14,
		.single_pdev_only = true,
		.rxdma1_enable = false,
		.num_rxmda_per_pdev = 2,
		.rx_mac_buf_ring = true,
		.vdev_start_delay = true,
		.htt_peer_map_v2 = false,
#if notyet
		.spectral = {
			.fft_sz = 0,
			.fft_pad_sz = 0,
			.summary_pad_sz = 0,
			.fft_hdr_len = 0,
			.max_fft_bins = 0,
			.fragment_160mhz = false,
		},

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP),
		.supports_monitor = false,
#endif
		.supports_shadow_regs = true,
		.idle_ps = true,
		.supports_sta_ps = true,
		.cold_boot_calib = false,
		.cbcal_restart_fw = false,
		.fw_mem_mode = 0,
		.num_vdevs = 16 + 1,
		.num_peers = 512,
		.supports_suspend = true,
		.hal_desc_sz = sizeof(struct hal_rx_desc_wcn6855),
		.supports_regdb = true,
		.fix_l1ss = false,
		.credit_flow = true,
		.max_tx_ring = DP_TCL_NUM_RING_MAX_QCA6390,
		.hal_params = &ath11k_hw_hal_params_qca6390,
#if notyet
		.supports_dynamic_smps_6ghz = false,
		.alloc_cacheable_memory = false,
		.supports_rssi_stats = true,
#endif
		.fw_wmi_diag_event = true,
		.current_cc_support = true,
		.dbr_debug_support = false,
		.global_reset = true,
#ifdef notyet
		.bios_sar_capa = &ath11k_hw_sar_capa_wcn6855,
#endif
		.m3_fw_support = true,
		.fixed_bdf_addr = false,
		.fixed_mem_region = false,
		.static_window_map = false,
		.hybrid_bus_type = false,
		.fixed_fw_mem = false,
#if notyet
		.support_off_channel_tx = true,
		.supports_multi_bssid = true,

		.sram_dump = {
			.start = 0x01400000,
			.end = 0x0177ffff,
		},

		.tcl_ring_retry = true,
		.tx_ring_size = DP_TCL_DATA_RING_SIZE,
		.smp2p_wow_exit = false,
#endif
	},
	{
		.name = "wcn6750 hw1.0",
		.hw_rev = ATH11K_HW_WCN6750_HW10,
		.fw = {
			.dir = "WCN6750/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
		},
		.max_radios = 1,
		.bdf_addr = 0x4B0C0000,
		.hw_ops = &wcn6750_ops,
		.ring_mask = &ath11k_hw_ring_mask_wcn6750,
		.internal_sleep_clock = false,
		.regs = &wcn6750_regs,
		.qmi_service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_WCN6750,
		.host_ce_config = qwx_host_ce_config_qca6390,
		.ce_count = QWX_CE_COUNT_QCA6390,
		.target_ce_config = ath11k_target_ce_config_wlan_qca6390,
		.target_ce_count = 9,
		.svc_to_ce_map = ath11k_target_service_to_ce_map_wlan_qca6390,
		.svc_to_ce_map_len = 14,
		.single_pdev_only = true,
		.rxdma1_enable = false,
		.num_rxmda_per_pdev = 1,
		.rx_mac_buf_ring = true,
		.vdev_start_delay = true,
		.htt_peer_map_v2 = false,
#if notyet
		.spectral = {
			.fft_sz = 0,
			.fft_pad_sz = 0,
			.summary_pad_sz = 0,
			.fft_hdr_len = 0,
			.max_fft_bins = 0,
			.fragment_160mhz = false,
		},

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP),
		.supports_monitor = false,
#endif
		.supports_shadow_regs = true,
		.idle_ps = true,
		.supports_sta_ps = true,
		.cold_boot_calib = true,
		.cbcal_restart_fw = false,
		.fw_mem_mode = 0,
		.num_vdevs = 16 + 1,
		.num_peers = 512,
		.supports_suspend = false,
		.hal_desc_sz = sizeof(struct hal_rx_desc_qcn9074),
		.supports_regdb = true,
		.fix_l1ss = false,
		.credit_flow = true,
		.max_tx_ring = DP_TCL_NUM_RING_MAX,
		.hal_params = &ath11k_hw_hal_params_wcn6750,
#if notyet
		.supports_dynamic_smps_6ghz = false,
		.alloc_cacheable_memory = false,
		.supports_rssi_stats = true,
#endif
		.fw_wmi_diag_event = false,
		.current_cc_support = true,
		.dbr_debug_support = false,
		.global_reset = false,
#ifdef notyet
		.bios_sar_capa = NULL,
#endif
		.m3_fw_support = false,
		.fixed_bdf_addr = false,
		.fixed_mem_region = false,
		.static_window_map = true,
		.hybrid_bus_type = true,
		.fixed_fw_mem = true,
#if notyet
		.support_off_channel_tx = true,
		.supports_multi_bssid = true,

		.sram_dump = {},

		.tcl_ring_retry = false,
		.tx_ring_size = DP_TCL_DATA_RING_SIZE_WCN6750,
		.smp2p_wow_exit = true,
#endif
	},
};

const struct ath11k_hw_regs ipq8074_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_base_lsb = 0x00000510,
	.hal_tcl1_ring_base_msb = 0x00000514,
	.hal_tcl1_ring_id = 0x00000518,
	.hal_tcl1_ring_misc = 0x00000520,
	.hal_tcl1_ring_tp_addr_lsb = 0x0000052c,
	.hal_tcl1_ring_tp_addr_msb = 0x00000530,
	.hal_tcl1_ring_consumer_int_setup_ix0 = 0x00000540,
	.hal_tcl1_ring_consumer_int_setup_ix1 = 0x00000544,
	.hal_tcl1_ring_msi1_base_lsb = 0x00000558,
	.hal_tcl1_ring_msi1_base_msb = 0x0000055c,
	.hal_tcl1_ring_msi1_data = 0x00000560,
	.hal_tcl2_ring_base_lsb = 0x00000568,
	.hal_tcl_ring_base_lsb = 0x00000618,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = 0x00000720,

	/* REO2SW(x) R0 ring configuration address */
	.hal_reo1_ring_base_lsb = 0x0000029c,
	.hal_reo1_ring_base_msb = 0x000002a0,
	.hal_reo1_ring_id = 0x000002a4,
	.hal_reo1_ring_misc = 0x000002ac,
	.hal_reo1_ring_hp_addr_lsb = 0x000002b0,
	.hal_reo1_ring_hp_addr_msb = 0x000002b4,
	.hal_reo1_ring_producer_int_setup = 0x000002c0,
	.hal_reo1_ring_msi1_base_lsb = 0x000002e4,
	.hal_reo1_ring_msi1_base_msb = 0x000002e8,
	.hal_reo1_ring_msi1_data = 0x000002ec,
	.hal_reo2_ring_base_lsb = 0x000002f4,
	.hal_reo1_aging_thresh_ix_0 = 0x00000564,
	.hal_reo1_aging_thresh_ix_1 = 0x00000568,
	.hal_reo1_aging_thresh_ix_2 = 0x0000056c,
	.hal_reo1_aging_thresh_ix_3 = 0x00000570,

	/* REO2SW(x) R2 ring pointers (head/tail) address */
	.hal_reo1_ring_hp = 0x00003038,
	.hal_reo1_ring_tp = 0x0000303c,
	.hal_reo2_ring_hp = 0x00003040,

	/* REO2TCL R0 ring configuration address */
	.hal_reo_tcl_ring_base_lsb = 0x000003fc,
	.hal_reo_tcl_ring_hp = 0x00003058,

	/* REO CMD ring address */
	.hal_reo_cmd_ring_base_lsb = 0x00000194,
	.hal_reo_cmd_ring_hp = 0x00003020,

	/* REO status address */
	.hal_reo_status_ring_base_lsb = 0x00000504,
	.hal_reo_status_hp = 0x00003070,

	/* SW2REO ring address */
	.hal_sw2reo_ring_base_lsb = 0x000001ec,
	.hal_sw2reo_ring_hp = 0x00003028,

	/* WCSS relative address */
	.hal_seq_wcss_umac_ce0_src_reg = 0x00a00000,
	.hal_seq_wcss_umac_ce0_dst_reg = 0x00a01000,
	.hal_seq_wcss_umac_ce1_src_reg = 0x00a02000,
	.hal_seq_wcss_umac_ce1_dst_reg = 0x00a03000,

	/* WBM Idle address */
	.hal_wbm_idle_link_ring_base_lsb = 0x00000860,
	.hal_wbm_idle_link_ring_misc = 0x00000870,

	/* SW2WBM release address */
	.hal_wbm_release_ring_base_lsb = 0x000001d8,

	/* WBM2SW release address */
	.hal_wbm0_release_ring_base_lsb = 0x00000910,
	.hal_wbm1_release_ring_base_lsb = 0x00000968,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x0,
	.pcie_pcs_osc_dtct_config_base = 0x0,

	/* Shadow register area */
	.hal_shadow_base_addr = 0x0,

	/* REO misc control register, not used in IPQ8074 */
	.hal_reo1_misc_ctl = 0x0,
};

const struct ath11k_hw_regs qca6390_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_base_lsb = 0x00000684,
	.hal_tcl1_ring_base_msb = 0x00000688,
	.hal_tcl1_ring_id = 0x0000068c,
	.hal_tcl1_ring_misc = 0x00000694,
	.hal_tcl1_ring_tp_addr_lsb = 0x000006a0,
	.hal_tcl1_ring_tp_addr_msb = 0x000006a4,
	.hal_tcl1_ring_consumer_int_setup_ix0 = 0x000006b4,
	.hal_tcl1_ring_consumer_int_setup_ix1 = 0x000006b8,
	.hal_tcl1_ring_msi1_base_lsb = 0x000006cc,
	.hal_tcl1_ring_msi1_base_msb = 0x000006d0,
	.hal_tcl1_ring_msi1_data = 0x000006d4,
	.hal_tcl2_ring_base_lsb = 0x000006dc,
	.hal_tcl_ring_base_lsb = 0x0000078c,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = 0x00000894,

	/* REO2SW(x) R0 ring configuration address */
	.hal_reo1_ring_base_lsb = 0x00000244,
	.hal_reo1_ring_base_msb = 0x00000248,
	.hal_reo1_ring_id = 0x0000024c,
	.hal_reo1_ring_misc = 0x00000254,
	.hal_reo1_ring_hp_addr_lsb = 0x00000258,
	.hal_reo1_ring_hp_addr_msb = 0x0000025c,
	.hal_reo1_ring_producer_int_setup = 0x00000268,
	.hal_reo1_ring_msi1_base_lsb = 0x0000028c,
	.hal_reo1_ring_msi1_base_msb = 0x00000290,
	.hal_reo1_ring_msi1_data = 0x00000294,
	.hal_reo2_ring_base_lsb = 0x0000029c,
	.hal_reo1_aging_thresh_ix_0 = 0x0000050c,
	.hal_reo1_aging_thresh_ix_1 = 0x00000510,
	.hal_reo1_aging_thresh_ix_2 = 0x00000514,
	.hal_reo1_aging_thresh_ix_3 = 0x00000518,

	/* REO2SW(x) R2 ring pointers (head/tail) address */
	.hal_reo1_ring_hp = 0x00003030,
	.hal_reo1_ring_tp = 0x00003034,
	.hal_reo2_ring_hp = 0x00003038,

	/* REO2TCL R0 ring configuration address */
	.hal_reo_tcl_ring_base_lsb = 0x000003a4,
	.hal_reo_tcl_ring_hp = 0x00003050,

	/* REO CMD ring address */
	.hal_reo_cmd_ring_base_lsb = 0x00000194,
	.hal_reo_cmd_ring_hp = 0x00003020,

	/* REO status address */
	.hal_reo_status_ring_base_lsb = 0x000004ac,
	.hal_reo_status_hp = 0x00003068,

	/* SW2REO ring address */
	.hal_sw2reo_ring_base_lsb = 0x000001ec,
	.hal_sw2reo_ring_hp = 0x00003028,

	/* WCSS relative address */
	.hal_seq_wcss_umac_ce0_src_reg = 0x00a00000,
	.hal_seq_wcss_umac_ce0_dst_reg = 0x00a01000,
	.hal_seq_wcss_umac_ce1_src_reg = 0x00a02000,
	.hal_seq_wcss_umac_ce1_dst_reg = 0x00a03000,

	/* WBM Idle address */
	.hal_wbm_idle_link_ring_base_lsb = 0x00000860,
	.hal_wbm_idle_link_ring_misc = 0x00000870,

	/* SW2WBM release address */
	.hal_wbm_release_ring_base_lsb = 0x000001d8,

	/* WBM2SW release address */
	.hal_wbm0_release_ring_base_lsb = 0x00000910,
	.hal_wbm1_release_ring_base_lsb = 0x00000968,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x01e0c0ac,
	.pcie_pcs_osc_dtct_config_base = 0x01e0c628,

	/* Shadow register area */
	.hal_shadow_base_addr = 0x000008fc,

	/* REO misc control register, not used in QCA6390 */
	.hal_reo1_misc_ctl = 0x0,
};

const struct ath11k_hw_regs qcn9074_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_base_lsb = 0x000004f0,
	.hal_tcl1_ring_base_msb = 0x000004f4,
	.hal_tcl1_ring_id = 0x000004f8,
	.hal_tcl1_ring_misc = 0x00000500,
	.hal_tcl1_ring_tp_addr_lsb = 0x0000050c,
	.hal_tcl1_ring_tp_addr_msb = 0x00000510,
	.hal_tcl1_ring_consumer_int_setup_ix0 = 0x00000520,
	.hal_tcl1_ring_consumer_int_setup_ix1 = 0x00000524,
	.hal_tcl1_ring_msi1_base_lsb = 0x00000538,
	.hal_tcl1_ring_msi1_base_msb = 0x0000053c,
	.hal_tcl1_ring_msi1_data = 0x00000540,
	.hal_tcl2_ring_base_lsb = 0x00000548,
	.hal_tcl_ring_base_lsb = 0x000005f8,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = 0x00000700,

	/* REO2SW(x) R0 ring configuration address */
	.hal_reo1_ring_base_lsb = 0x0000029c,
	.hal_reo1_ring_base_msb = 0x000002a0,
	.hal_reo1_ring_id = 0x000002a4,
	.hal_reo1_ring_misc = 0x000002ac,
	.hal_reo1_ring_hp_addr_lsb = 0x000002b0,
	.hal_reo1_ring_hp_addr_msb = 0x000002b4,
	.hal_reo1_ring_producer_int_setup = 0x000002c0,
	.hal_reo1_ring_msi1_base_lsb = 0x000002e4,
	.hal_reo1_ring_msi1_base_msb = 0x000002e8,
	.hal_reo1_ring_msi1_data = 0x000002ec,
	.hal_reo2_ring_base_lsb = 0x000002f4,
	.hal_reo1_aging_thresh_ix_0 = 0x00000564,
	.hal_reo1_aging_thresh_ix_1 = 0x00000568,
	.hal_reo1_aging_thresh_ix_2 = 0x0000056c,
	.hal_reo1_aging_thresh_ix_3 = 0x00000570,

	/* REO2SW(x) R2 ring pointers (head/tail) address */
	.hal_reo1_ring_hp = 0x00003038,
	.hal_reo1_ring_tp = 0x0000303c,
	.hal_reo2_ring_hp = 0x00003040,

	/* REO2TCL R0 ring configuration address */
	.hal_reo_tcl_ring_base_lsb = 0x000003fc,
	.hal_reo_tcl_ring_hp = 0x00003058,

	/* REO CMD ring address */
	.hal_reo_cmd_ring_base_lsb = 0x00000194,
	.hal_reo_cmd_ring_hp = 0x00003020,

	/* REO status address */
	.hal_reo_status_ring_base_lsb = 0x00000504,
	.hal_reo_status_hp = 0x00003070,

	/* SW2REO ring address */
	.hal_sw2reo_ring_base_lsb = 0x000001ec,
	.hal_sw2reo_ring_hp = 0x00003028,

	/* WCSS relative address */
	.hal_seq_wcss_umac_ce0_src_reg = 0x01b80000,
	.hal_seq_wcss_umac_ce0_dst_reg = 0x01b81000,
	.hal_seq_wcss_umac_ce1_src_reg = 0x01b82000,
	.hal_seq_wcss_umac_ce1_dst_reg = 0x01b83000,

	/* WBM Idle address */
	.hal_wbm_idle_link_ring_base_lsb = 0x00000874,
	.hal_wbm_idle_link_ring_misc = 0x00000884,

	/* SW2WBM release address */
	.hal_wbm_release_ring_base_lsb = 0x000001ec,

	/* WBM2SW release address */
	.hal_wbm0_release_ring_base_lsb = 0x00000924,
	.hal_wbm1_release_ring_base_lsb = 0x0000097c,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x01e0e0a8,
	.pcie_pcs_osc_dtct_config_base = 0x01e0f45c,

	/* Shadow register area */
	.hal_shadow_base_addr = 0x0,

	/* REO misc control register, not used in QCN9074 */
	.hal_reo1_misc_ctl = 0x0,
};

const struct ath11k_hw_regs wcn6855_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_base_lsb = 0x00000690,
	.hal_tcl1_ring_base_msb = 0x00000694,
	.hal_tcl1_ring_id = 0x00000698,
	.hal_tcl1_ring_misc = 0x000006a0,
	.hal_tcl1_ring_tp_addr_lsb = 0x000006ac,
	.hal_tcl1_ring_tp_addr_msb = 0x000006b0,
	.hal_tcl1_ring_consumer_int_setup_ix0 = 0x000006c0,
	.hal_tcl1_ring_consumer_int_setup_ix1 = 0x000006c4,
	.hal_tcl1_ring_msi1_base_lsb = 0x000006d8,
	.hal_tcl1_ring_msi1_base_msb = 0x000006dc,
	.hal_tcl1_ring_msi1_data = 0x000006e0,
	.hal_tcl2_ring_base_lsb = 0x000006e8,
	.hal_tcl_ring_base_lsb = 0x00000798,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = 0x000008a0,

	/* REO2SW(x) R0 ring configuration address */
	.hal_reo1_ring_base_lsb = 0x00000244,
	.hal_reo1_ring_base_msb = 0x00000248,
	.hal_reo1_ring_id = 0x0000024c,
	.hal_reo1_ring_misc = 0x00000254,
	.hal_reo1_ring_hp_addr_lsb = 0x00000258,
	.hal_reo1_ring_hp_addr_msb = 0x0000025c,
	.hal_reo1_ring_producer_int_setup = 0x00000268,
	.hal_reo1_ring_msi1_base_lsb = 0x0000028c,
	.hal_reo1_ring_msi1_base_msb = 0x00000290,
	.hal_reo1_ring_msi1_data = 0x00000294,
	.hal_reo2_ring_base_lsb = 0x0000029c,
	.hal_reo1_aging_thresh_ix_0 = 0x000005bc,
	.hal_reo1_aging_thresh_ix_1 = 0x000005c0,
	.hal_reo1_aging_thresh_ix_2 = 0x000005c4,
	.hal_reo1_aging_thresh_ix_3 = 0x000005c8,

	/* REO2SW(x) R2 ring pointers (head/tail) address */
	.hal_reo1_ring_hp = 0x00003030,
	.hal_reo1_ring_tp = 0x00003034,
	.hal_reo2_ring_hp = 0x00003038,

	/* REO2TCL R0 ring configuration address */
	.hal_reo_tcl_ring_base_lsb = 0x00000454,
	.hal_reo_tcl_ring_hp = 0x00003060,

	/* REO CMD ring address */
	.hal_reo_cmd_ring_base_lsb = 0x00000194,
	.hal_reo_cmd_ring_hp = 0x00003020,

	/* REO status address */
	.hal_reo_status_ring_base_lsb = 0x0000055c,
	.hal_reo_status_hp = 0x00003078,

	/* SW2REO ring address */
	.hal_sw2reo_ring_base_lsb = 0x000001ec,
	.hal_sw2reo_ring_hp = 0x00003028,

	/* WCSS relative address */
	.hal_seq_wcss_umac_ce0_src_reg = 0x1b80000,
	.hal_seq_wcss_umac_ce0_dst_reg = 0x1b81000,
	.hal_seq_wcss_umac_ce1_src_reg = 0x1b82000,
	.hal_seq_wcss_umac_ce1_dst_reg = 0x1b83000,

	/* WBM Idle address */
	.hal_wbm_idle_link_ring_base_lsb = 0x00000870,
	.hal_wbm_idle_link_ring_misc = 0x00000880,

	/* SW2WBM release address */
	.hal_wbm_release_ring_base_lsb = 0x000001e8,

	/* WBM2SW release address */
	.hal_wbm0_release_ring_base_lsb = 0x00000920,
	.hal_wbm1_release_ring_base_lsb = 0x00000978,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x01e0c0ac,
	.pcie_pcs_osc_dtct_config_base = 0x01e0c628,

	/* Shadow register area */
	.hal_shadow_base_addr = 0x000008fc,

	/* REO misc control register, used for fragment
	 * destination ring config in WCN6855.
	 */
	.hal_reo1_misc_ctl = 0x00000630,
};

const struct ath11k_hw_regs wcn6750_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_base_lsb = 0x00000694,
	.hal_tcl1_ring_base_msb = 0x00000698,
	.hal_tcl1_ring_id = 0x0000069c,
	.hal_tcl1_ring_misc = 0x000006a4,
	.hal_tcl1_ring_tp_addr_lsb = 0x000006b0,
	.hal_tcl1_ring_tp_addr_msb = 0x000006b4,
	.hal_tcl1_ring_consumer_int_setup_ix0 = 0x000006c4,
	.hal_tcl1_ring_consumer_int_setup_ix1 = 0x000006c8,
	.hal_tcl1_ring_msi1_base_lsb = 0x000006dc,
	.hal_tcl1_ring_msi1_base_msb = 0x000006e0,
	.hal_tcl1_ring_msi1_data = 0x000006e4,
	.hal_tcl2_ring_base_lsb = 0x000006ec,
	.hal_tcl_ring_base_lsb = 0x0000079c,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = 0x000008a4,

	/* REO2SW(x) R0 ring configuration address */
	.hal_reo1_ring_base_lsb = 0x000001ec,
	.hal_reo1_ring_base_msb = 0x000001f0,
	.hal_reo1_ring_id = 0x000001f4,
	.hal_reo1_ring_misc = 0x000001fc,
	.hal_reo1_ring_hp_addr_lsb = 0x00000200,
	.hal_reo1_ring_hp_addr_msb = 0x00000204,
	.hal_reo1_ring_producer_int_setup = 0x00000210,
	.hal_reo1_ring_msi1_base_lsb = 0x00000234,
	.hal_reo1_ring_msi1_base_msb = 0x00000238,
	.hal_reo1_ring_msi1_data = 0x0000023c,
	.hal_reo2_ring_base_lsb = 0x00000244,
	.hal_reo1_aging_thresh_ix_0 = 0x00000564,
	.hal_reo1_aging_thresh_ix_1 = 0x00000568,
	.hal_reo1_aging_thresh_ix_2 = 0x0000056c,
	.hal_reo1_aging_thresh_ix_3 = 0x00000570,

	/* REO2SW(x) R2 ring pointers (head/tail) address */
	.hal_reo1_ring_hp = 0x00003028,
	.hal_reo1_ring_tp = 0x0000302c,
	.hal_reo2_ring_hp = 0x00003030,

	/* REO2TCL R0 ring configuration address */
	.hal_reo_tcl_ring_base_lsb = 0x000003fc,
	.hal_reo_tcl_ring_hp = 0x00003058,

	/* REO CMD ring address */
	.hal_reo_cmd_ring_base_lsb = 0x000000e4,
	.hal_reo_cmd_ring_hp = 0x00003010,

	/* REO status address */
	.hal_reo_status_ring_base_lsb = 0x00000504,
	.hal_reo_status_hp = 0x00003070,

	/* SW2REO ring address */
	.hal_sw2reo_ring_base_lsb = 0x0000013c,
	.hal_sw2reo_ring_hp = 0x00003018,

	/* WCSS relative address */
	.hal_seq_wcss_umac_ce0_src_reg = 0x01b80000,
	.hal_seq_wcss_umac_ce0_dst_reg = 0x01b81000,
	.hal_seq_wcss_umac_ce1_src_reg = 0x01b82000,
	.hal_seq_wcss_umac_ce1_dst_reg = 0x01b83000,

	/* WBM Idle address */
	.hal_wbm_idle_link_ring_base_lsb = 0x00000874,
	.hal_wbm_idle_link_ring_misc = 0x00000884,

	/* SW2WBM release address */
	.hal_wbm_release_ring_base_lsb = 0x000001ec,

	/* WBM2SW release address */
	.hal_wbm0_release_ring_base_lsb = 0x00000924,
	.hal_wbm1_release_ring_base_lsb = 0x0000097c,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x0,
	.pcie_pcs_osc_dtct_config_base = 0x0,

	/* Shadow register area */
	.hal_shadow_base_addr = 0x00000504,

	/* REO misc control register, used for fragment
	 * destination ring config in WCN6750.
	 */
	.hal_reo1_misc_ctl = 0x000005d8,
};

#define QWX_SLEEP_CLOCK_SELECT_INTERNAL_BIT	0x02
#define QWX_HOST_CSTATE_BIT			0x04
#define QWX_PLATFORM_CAP_PCIE_GLOBAL_RESET	0x08
#define QWX_PLATFORM_CAP_PCIE_PME_D3COLD	0x10

const struct qmi_elem_info qmi_response_type_v01_ei[] = {
	{
		.data_type	= QMI_SIGNED_2_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct qmi_response_type_v01, result),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_SIGNED_2_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct qmi_response_type_v01, error),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.elem_len	= 0,
		.elem_size	= 0,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= 0,
		.ei_array	= NULL,
	},
};

const struct qmi_elem_info qmi_wlanfw_ind_register_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_ready_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_ready_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   msa_ready_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   msa_ready_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   pin_connect_result_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   pin_connect_result_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   client_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   client_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_mem_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   request_mem_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_mem_ready_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_mem_ready_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_init_done_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   fw_init_done_enable),
	},

	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   rejuvenate_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   rejuvenate_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   xo_cal_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   xo_cal_enable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   cal_done_enable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_req_msg_v01,
					   cal_done_enable),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_ind_register_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_resp_msg_v01,
					   fw_status_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_ind_register_resp_msg_v01,
					   fw_status),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_host_cap_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   num_clients_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   num_clients),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   wake_msi_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   wake_msi),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   gpios_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   gpios_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_WLFW_MAX_NUM_GPIO_V01,
		.elem_size	= sizeof(uint32_t),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   gpios),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   nm_modem_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   nm_modem),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_cache_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   bdf_cache_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_cache_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   m3_cache_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_filesys_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_filesys_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_cache_support_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_cache_support),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_done_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   cal_done),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_bucket_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_bucket),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_cfg_mode_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_req_msg_v01,
					   mem_cfg_mode),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_host_cap_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_host_cap_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_mem_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, offset),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, size),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, secure_flag),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_mem_seg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01,
				  size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, type),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, mem_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_MEM_CFG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, mem_cfg),
		.ei_array	= qmi_wlanfw_mem_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_request_mem_ind_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_request_mem_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_request_mem_ind_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_mem_seg_resp_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, type),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, restore),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_respond_mem_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_req_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_resp_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_req_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_respond_mem_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_cap_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_rf_chip_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_rf_chip_info_s_v01,
					   chip_id),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_rf_chip_info_s_v01,
					   chip_family),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_rf_board_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_rf_board_info_s_v01,
					   board_id),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_soc_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_soc_info_s_v01, soc_id),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_fw_version_info_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_fw_version_info_s_v01,
					   fw_version),
	},
	{
		.data_type	= QMI_STRING,
		.elem_len	= ATH11K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01 + 1,
		.elem_size	= sizeof(char),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_fw_version_info_s_v01,
					   fw_build_timestamp),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_cap_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   chip_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_rf_chip_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   chip_info),
		.ei_array	= qmi_wlanfw_rf_chip_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   board_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_rf_board_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   board_info),
		.ei_array	= qmi_wlanfw_rf_board_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   soc_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_soc_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   soc_info),
		.ei_array	= qmi_wlanfw_soc_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_version_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_wlanfw_fw_version_info_s_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_version_info),
		.ei_array	= qmi_wlanfw_fw_version_info_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_build_id_valid),
	},
	{
		.data_type	= QMI_STRING,
		.elem_len	= ATH11K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01 + 1,
		.elem_size	= sizeof(char),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   fw_build_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   num_macs_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   num_macs),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   voltage_mv_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   voltage_mv),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   time_freq_hz_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   time_freq_hz),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   otp_version_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   otp_version),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   eeprom_read_timeout_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct qmi_wlanfw_cap_resp_msg_v01,
					   eeprom_read_timeout),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_bdf_download_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   valid),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   file_id_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_cal_temp_id_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   file_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= QMI_WLANFW_MAX_DATA_SIZE_V01,
		.elem_size	= sizeof(uint8_t),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   data),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   end),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   bdf_type_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_req_msg_v01,
					   bdf_type),
	},

	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_bdf_download_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_bdf_download_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_m3_info_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_m3_info_req_msg_v01, addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_m3_info_req_msg_v01, size),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_m3_info_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_m3_info_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_wlan_ini_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_ini_req_msg_v01,
					   enablefwlog_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_ini_req_msg_v01,
					   enablefwlog),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_wlan_ini_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_wlan_ini_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_ce_tgt_pipe_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_pipedir_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   nentries),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   nbytes_max),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01,
					   flags),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_ce_svc_pipe_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01,
					   service_id),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_pipedir_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_shadow_reg_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_shadow_reg_cfg_s_v01, id),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_shadow_reg_cfg_s_v01,
					   offset),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_shadow_reg_v2_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_shadow_reg_v2_cfg_s_v01,
					   addr),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_wlan_mode_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   mode),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   hw_debug_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_req_msg_v01,
					   hw_debug),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_wlan_mode_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_wlan_mode_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_wlan_cfg_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   host_version_valid),
	},
	{
		.data_type	= QMI_STRING,
		.elem_len	= QMI_WLANFW_MAX_STR_LEN_V01 + 1,
		.elem_size	= sizeof(char),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   host_version),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_CE_V01,
		.elem_size	= sizeof(
				struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   tgt_cfg),
		.ei_array	= qmi_wlanfw_ce_tgt_pipe_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   svc_cfg_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   svc_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_SVC_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   svc_cfg),
		.ei_array	= qmi_wlanfw_ce_svc_pipe_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_SHADOW_REG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_shadow_reg_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg),
		.ei_array	= qmi_wlanfw_shadow_reg_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v2_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v2_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_SHADOW_REG_V2_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_shadow_reg_v2_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v2),
		.ei_array	= qmi_wlanfw_shadow_reg_v2_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_wlan_cfg_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_wlan_cfg_resp_msg_v01, resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

int
qwx_intr(struct qwx_softc *sc)
{
	int ret = 0, i;

	if (test_bit(ATH11K_FLAG_CE_IRQ_ENABLED, sc->sc_flags)) {
		for (i = 0; i < sc->hw_params.ce_count; i++) {
			if (qwx_ce_per_engine_service(sc, i))
				ret = 1;
		}
	}

	if (test_bit(ATH11K_FLAG_EXT_IRQ_ENABLED, sc->sc_flags)) {
		if (qwx_dp_service_srng(sc))
			ret = 1;
	}

	return ret;
}

const char *qmi_data_type_name[QMI_NUM_DATA_TYPES] = {
	"EOTI",
	"OPT_FLAG",
	"DATA_LEN",
	"UNSIGNED_1_BYTE",
	"UNSIGNED_2_BYTE",
	"UNSIGNED_4_BYTE",
	"UNSIGNED_8_BYTE",
	"SIGNED_2_BYTE_ENUM",
	"SIGNED_4_BYTE_ENUM",
	"STRUCT",
	"STRING"
};

const struct qmi_elem_info *
qwx_qmi_decode_get_elem(const struct qmi_elem_info *ei, uint8_t elem_type)
{
	while (ei->data_type != QMI_EOTI && ei->tlv_type != elem_type)
		ei++;

	DNPRINTF(QWX_D_QMI, "%s: found elem 0x%x data type 0x%x\n", __func__,
	    ei->tlv_type, ei->data_type);
	return ei;
}

size_t
qwx_qmi_decode_min_elem_size(const struct qmi_elem_info *ei, int nested)
{
	size_t min_size = 0;

	switch (ei->data_type) {
	case QMI_EOTI:
	case QMI_OPT_FLAG:
		break;
	case QMI_DATA_LEN:
		if (ei->elem_len == 1)
			min_size += sizeof(uint8_t);
		else
			min_size += sizeof(uint16_t);
		break;
	case QMI_UNSIGNED_1_BYTE:
	case QMI_UNSIGNED_2_BYTE:
	case QMI_UNSIGNED_4_BYTE:
	case QMI_UNSIGNED_8_BYTE:
	case QMI_SIGNED_2_BYTE_ENUM:
	case QMI_SIGNED_4_BYTE_ENUM:
		min_size += ei->elem_len * ei->elem_size;
		break;
	case QMI_STRUCT:
		if (nested > 2) {
			printf("%s: QMI struct element 0x%x with "
			    "data type %s (0x%x) is nested too "
			    "deeply\n", __func__,
			    ei->tlv_type,
			    qmi_data_type_name[ei->data_type],
			    ei->data_type);
		}
		ei = ei->ei_array;
		while (ei->data_type != QMI_EOTI) {
			min_size += qwx_qmi_decode_min_elem_size(ei,
			    nested + 1);
			ei++;
		}
		break;
	case QMI_STRING:
		min_size += 1;
		/* Strings nested in structs use an in-band length field. */
		if (nested) {
			if (ei->elem_len <= 0xff)
				min_size += sizeof(uint8_t);
			else
				min_size += sizeof(uint16_t);
		}
		break;
	default:
		printf("%s: unhandled data type 0x%x\n", __func__,
		    ei->data_type);
		break;
	}

	return min_size;
}

int
qwx_qmi_decode_tlv_hdr(struct qwx_softc *sc,
    const struct qmi_elem_info **next_ei, uint16_t *actual_size,
    size_t output_len, const struct qmi_elem_info *ei0,
    uint8_t *input, size_t input_len)
{
	uint8_t *p = input;
	size_t remain = input_len;
	uint8_t elem_type;
	uint16_t elem_size = 0;
	const struct qmi_elem_info *ei;

	*next_ei = NULL;
	*actual_size = 0;

	if (remain < 3) {
		printf("%s: QMI message TLV header too short\n",
		   sc->sc_dev.dv_xname);
		return -1;
	}
	elem_type = *p;
	p++;
	remain--;

	/*
	 * By relying on TLV type information we can skip over EIs which
	 * describe optional elements that have not been encoded.
	 * Such elements will be left at their default value (zero) in
	 * the decoded output struct.
	 * XXX We currently allow elements to appear in any order and
	 * we do not detect duplicates.
	 */
	ei = qwx_qmi_decode_get_elem(ei0, elem_type);

	DNPRINTF(QWX_D_QMI,
	    "%s: decoding element 0x%x with data type %s (0x%x)\n",
	    __func__, elem_type, qmi_data_type_name[ei->data_type],
	    ei->data_type);

	if (remain < 2) {
		printf("%s: QMI message too short\n", sc->sc_dev.dv_xname);
		return -1;
	}

	if (ei->data_type == QMI_DATA_LEN && ei->elem_len == 1) {
		elem_size = p[0];
		p++;
		remain--;
	} else {
		elem_size = (p[0] | (p[1] << 8));
		p += 2;
		remain -= 2;
	}

	*next_ei = ei;
	*actual_size = elem_size;

	if (ei->data_type == QMI_EOTI) {
		DNPRINTF(QWX_D_QMI,
		    "%s: unrecognized QMI element type 0x%x size %u\n",
		    sc->sc_dev.dv_xname, elem_type, elem_size);
		return 0;
	}

	/*
	 * Is this an optional element which has been encoded?
	 * If so, use info about this optional element for verification.
	 */
	if (ei->data_type == QMI_OPT_FLAG)
		ei++;

	DNPRINTF(QWX_D_QMI, "%s: ei->size %u, actual size %u\n", __func__,
	    ei->elem_size, *actual_size);

	switch (ei->data_type) {
	case QMI_UNSIGNED_1_BYTE:
	case QMI_UNSIGNED_2_BYTE:
	case QMI_UNSIGNED_4_BYTE:
	case QMI_UNSIGNED_8_BYTE:
	case QMI_SIGNED_2_BYTE_ENUM:
	case QMI_SIGNED_4_BYTE_ENUM:
		if (elem_size != ei->elem_size) {
			printf("%s: QMI message element 0x%x "
			    "data type %s (0x%x) with bad size: %u\n",
			    sc->sc_dev.dv_xname, elem_type,
			    qmi_data_type_name[ei->data_type],
			    ei->data_type, elem_size);
			return -1;
		}
		break;
	case QMI_DATA_LEN:
		break;
	case QMI_STRING:
	case QMI_STRUCT:
		if (elem_size < qwx_qmi_decode_min_elem_size(ei, 0)) {
			printf("%s: QMI message element 0x%x "
			    "data type %s (0x%x) with bad size: %u\n",
			    sc->sc_dev.dv_xname, elem_type,
			    qmi_data_type_name[ei->data_type],
			    ei->data_type, elem_size);
			return -1;
		}
		break;
	default:
		printf("%s: unexpected QMI message element "
		    "data type 0x%x\n", sc->sc_dev.dv_xname,
		    ei->data_type);
		return -1;
	}

	if (remain < elem_size) {
		printf("%s: QMI message too short\n", sc->sc_dev.dv_xname);
		return -1;
	}

	if (ei->offset + ei->elem_size > output_len) {
		printf("%s: QMI message element type 0x%x too large: %u\n",
		    sc->sc_dev.dv_xname, elem_type, ei->elem_size);
		return -1;
	}

	return 0;
}

int
qwx_qmi_decode_byte(void *output, const struct qmi_elem_info *ei, void *input)
{
	if (ei->elem_size != sizeof(uint8_t)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	DNPRINTF(QWX_D_QMI, "%s: element 0x%x data type 0x%x size %u\n",
	    __func__, ei->tlv_type, ei->data_type, ei->elem_size);
	memcpy(output, input, ei->elem_size);
	return 0;
}

int
qwx_qmi_decode_word(void *output, const struct qmi_elem_info *ei, void *input)
{
	if (ei->elem_size != sizeof(uint16_t)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	DNPRINTF(QWX_D_QMI, "%s: element 0x%x data type 0x%x size %u\n",
	    __func__, ei->tlv_type, ei->data_type, ei->elem_size);
	memcpy(output, input, ei->elem_size);
	return 0;
}

int
qwx_qmi_decode_dword(void *output, const struct qmi_elem_info *ei, void *input)
{
	if (ei->elem_size != sizeof(uint32_t)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	DNPRINTF(QWX_D_QMI, "%s: element 0x%x data type 0x%x size %u\n",
	    __func__, ei->tlv_type, ei->data_type, ei->elem_size);
	memcpy(output, input, ei->elem_size);
	return 0;
}

int
qwx_qmi_decode_qword(void *output, const struct qmi_elem_info *ei, void *input)
{
	if (ei->elem_size != sizeof(uint64_t)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	DNPRINTF(QWX_D_QMI, "%s: element 0x%x data type 0x%x size %u\n",
	    __func__, ei->tlv_type, ei->data_type, ei->elem_size);
	memcpy(output, input, ei->elem_size);
	return 0;
}

int
qwx_qmi_decode_datalen(struct qwx_softc *sc, size_t *used, uint32_t *datalen,
    void *output, size_t output_len, const struct qmi_elem_info *ei,
    uint8_t *input, uint16_t input_len)
{
	uint8_t *p = input;
	size_t remain = input_len;

	*datalen = 0;

	DNPRINTF(QWX_D_QMI, "%s: input: ", __func__);
	for (int i = 0; i < input_len; i++) {
		DNPRINTF(QWX_D_QMI, " %02x", input[i]);
	}
	DNPRINTF(QWX_D_QMI, "\n");

	if (remain < ei->elem_size) {
		printf("%s: QMI message too short: remain=%zu elem_size=%u\n", __func__, remain, ei->elem_size);
		return -1;
	}

	switch (ei->elem_size) {
	case sizeof(uint8_t):
		*datalen = p[0];
		break;
	case sizeof(uint16_t):
		*datalen = p[0] | (p[1] << 8);
		break;
	default:
		printf("%s: bad datalen element size %u\n",
		    sc->sc_dev.dv_xname, ei->elem_size);
		return -1;
		
	}
	*used = ei->elem_size;

	if (ei->offset + sizeof(*datalen) > output_len) {
		printf("%s: QMI message element type 0x%x too large\n",
		    sc->sc_dev.dv_xname, ei->tlv_type);
		return -1;
	}
	memcpy(output + ei->offset, datalen, sizeof(*datalen));
	return 0;
}

int
qwx_qmi_decode_string(struct qwx_softc *sc, size_t *used_total,
    void *output, size_t output_len, const struct qmi_elem_info *ei,
    uint8_t *input, uint16_t input_len, uint16_t elem_size, int nested)
{
	uint8_t *p = input;
	uint16_t len;
	size_t remain = input_len;

	*used_total = 0;

	DNPRINTF(QWX_D_QMI, "%s: input: ", __func__);
	for (int i = 0; i < input_len; i++) {
		DNPRINTF(QWX_D_QMI, " %02x", input[i]);
	}
	DNPRINTF(QWX_D_QMI, "\n");

	if (nested) {
		/* Strings nested in structs use an in-band length field. */
		if (ei->elem_len <= 0xff) {
			if (remain == 0) {
				printf("%s: QMI string length header exceeds "
				    "input buffer size\n", __func__);
				return -1;
			}
			len = p[0];
			p++;
			(*used_total)++;
			remain--;
		} else {
			if (remain < 2) {
				printf("%s: QMI string length header exceeds "
				    "input buffer size\n", __func__);
				return -1;
			}
			len = p[0] | (p[1] << 8);
			p += 2;
			*used_total += 2;
			remain -= 2;
		}
	} else
		len = elem_size;

	if (len > ei->elem_len) {
		printf("%s: QMI string element of length %u exceeds "
		    "maximum length %u\n", __func__, len, ei->elem_len);
		return -1;
	}
	if (len > remain) {
		printf("%s: QMI string element of length %u exceeds "
		    "input buffer size %zu\n", __func__, len, remain);
		return -1;
	}
	if (len > output_len) {
		printf("%s: QMI string element of length %u exceeds "
		    "output buffer size %zu\n", __func__, len, output_len);
		return -1;
	}

	memcpy(output, p, len);

	p = output;
	p[len] = '\0';
	DNPRINTF(QWX_D_QMI, "%s: string (len %u): %s\n", __func__, len, p);

	*used_total += len;
	return 0;
}

int
qwx_qmi_decode_struct(struct qwx_softc *sc, size_t *used_total,
    void *output, size_t output_len,
    const struct qmi_elem_info *struct_ei,
    uint8_t *input, uint16_t input_len,
    int nested)
{
	const struct qmi_elem_info *ei = struct_ei->ei_array;
	uint32_t min_size;
	uint8_t *p = input;
	size_t remain = input_len;
	size_t used = 0;

	*used_total = 0;

	DNPRINTF(QWX_D_QMI, "%s: input: ", __func__);
	for (int i = 0; i < input_len; i++) {
		DNPRINTF(QWX_D_QMI, " %02x", input[i]);
	}
	DNPRINTF(QWX_D_QMI, "\n");

	min_size = qwx_qmi_decode_min_elem_size(struct_ei, 0);
	DNPRINTF(QWX_D_QMI, "%s: minimum struct size: %u\n", __func__, min_size);
	while (*used_total < min_size && ei->data_type != QMI_EOTI) {
		if (remain == 0) {
			printf("%s: QMI message too short\n", __func__);
			return -1;
		}

		if (ei->data_type == QMI_DATA_LEN) {
			uint32_t datalen;

			used = 0;
			if (qwx_qmi_decode_datalen(sc, &used, &datalen,
			    output, output_len, ei, p, remain))
				return -1;
			DNPRINTF(QWX_D_QMI, "%s: datalen %u used %zu bytes\n",
			    __func__, datalen, used);
			p += used;
			remain -= used;
			*used_total += used;
			if (remain < datalen) {
				printf("%s: QMI message too short\n", __func__);
				return -1;
			}
			ei++;
			DNPRINTF(QWX_D_QMI, "%s: datalen is for data_type=0x%x "
			    "tlv_type=0x%x elem_size=%u(0x%x) remain=%zu\n",
			    __func__, ei->data_type, ei->tlv_type,
			    ei->elem_size, ei->elem_size, remain);
			if (datalen == 0) {
				ei++;
				DNPRINTF(QWX_D_QMI,
				    "%s: skipped to data_type=0x%x "
				    "tlv_type=0x%x elem_size=%u(0x%x) "
				    "remain=%zu\n", __func__,
				    ei->data_type, ei->tlv_type,
				    ei->elem_size, ei->elem_size, remain);
				continue;
			}
		} else {
			if (remain < ei->elem_size) {
				printf("%s: QMI message too short\n",
				    __func__);
				return -1;
			}
		}

		if (ei->offset + ei->elem_size > output_len) {
			printf("%s: QMI message struct member element "
			    "type 0x%x too large: %u\n", sc->sc_dev.dv_xname,
			    ei->tlv_type, ei->elem_size);
			return -1;
		}

		DNPRINTF(QWX_D_QMI,
		    "%s: decoding struct member element 0x%x with "
		    "data type %s (0x%x) size=%u(0x%x) remain=%zu\n", __func__,
		    ei->tlv_type, qmi_data_type_name[ei->data_type],
		    ei->data_type, ei->elem_size, ei->elem_size, remain);
		switch (ei->data_type) {
		case QMI_UNSIGNED_1_BYTE:
			if (qwx_qmi_decode_byte(output + ei->offset, ei, p))
				return -1;
			remain -= ei->elem_size;
			p += ei->elem_size;
			*used_total += ei->elem_size;
			break;
		case QMI_UNSIGNED_2_BYTE:
		case QMI_SIGNED_2_BYTE_ENUM:
			if (qwx_qmi_decode_word(output + ei->offset, ei, p))
				return -1;
			remain -= ei->elem_size;
			p += ei->elem_size;
			*used_total += ei->elem_size;
			break;
		case QMI_UNSIGNED_4_BYTE:
		case QMI_SIGNED_4_BYTE_ENUM:
			if (qwx_qmi_decode_dword(output + ei->offset, ei, p))
				return -1;
			remain -= ei->elem_size;
			p += ei->elem_size;
			*used_total += ei->elem_size;
			break;
		case QMI_UNSIGNED_8_BYTE:
			if (qwx_qmi_decode_qword(output + ei->offset, ei, p))
				return -1;
			remain -= ei->elem_size;
			p += ei->elem_size;
			*used_total += ei->elem_size;
			break;
		case QMI_STRUCT:
			if (nested > 2) {
				printf("%s: QMI struct element data type 0x%x "
				    "is nested too deeply\n",
				    sc->sc_dev.dv_xname, ei->data_type);
				return -1;
			}
			used = 0;
			if (qwx_qmi_decode_struct(sc, &used,
			    output + ei->offset, output_len - ei->offset,
			    ei, p, remain, nested + 1))
				return -1;
			remain -= used;
			p += used;
			*used_total += used;
			break;
		case QMI_STRING:
			used = 0;
			if (qwx_qmi_decode_string(sc, &used,
			    output + ei->offset, output_len - ei->offset,
			    ei, p, remain, 0, 1))
				return -1;
			remain -= used;
			p += used;
			*used_total += used;
			break;
		default:
			printf("%s: unhandled QMI struct element "
			    "data type 0x%x\n", sc->sc_dev.dv_xname,
			    ei->data_type);
			return -1;
		}

		ei++;
		DNPRINTF(QWX_D_QMI, "%s: next ei 0x%x ei->data_type=0x%x\n",
		    __func__, ei->tlv_type, ei->data_type);
	}

	DNPRINTF(QWX_D_QMI, "%s: used_total=%zu ei->data_type=0x%x\n",
	    __func__, *used_total, ei->data_type);

	return 0;
}

int
qwx_qmi_decode_msg(struct qwx_softc *sc, void *output, size_t output_len,
    const struct qmi_elem_info *ei0, uint8_t *input, uint16_t input_len)
{
	uint8_t *p = input;
	size_t remain = input_len, used;
	const struct qmi_elem_info *ei = ei0;

	memset(output, 0, output_len);

	DNPRINTF(QWX_D_QMI, "%s: input: ", __func__);
	for (int i = 0; i < input_len; i++) {
		DNPRINTF(QWX_D_QMI, " %02x", input[i]);
	}
	DNPRINTF(QWX_D_QMI, "\n");

	while (remain > 0 && ei->data_type != QMI_EOTI) {
		uint32_t nelem = 1, i;
		uint16_t datalen;

		if (qwx_qmi_decode_tlv_hdr(sc, &ei, &datalen, output_len,
		    ei0, p, remain))
			return -1;

		/* Skip unrecognized elements. */
		if (ei->data_type == QMI_EOTI) {
			p += 3 + datalen;
			remain -= 3 + datalen;
			ei = ei0;
			continue;
		}

		/* Set 'valid' flag for optional fields in output struct. */
		if (ei->data_type == QMI_OPT_FLAG) {
			uint8_t *pvalid;

			if (ei->offset + ei->elem_size > output_len) {
				printf("%s: QMI message element type 0x%x "
				    "too large: %u\n", sc->sc_dev.dv_xname,
				    ei->tlv_type, ei->elem_size);
			}

			pvalid = (uint8_t *)output + ei->offset;
			*pvalid = 1;

			ei++;
		}

		p += 3;
		remain -= 3;

		if (ei->data_type == QMI_DATA_LEN) {
			const struct qmi_elem_info *datalen_ei = ei;
			uint8_t elem_type = ei->tlv_type;

			/*
			 * Size info in TLV header indicates the
			 * total length of element data that follows.
			 */
			if (remain < datalen) {
				printf("%s:%d QMI message too short\n",
				    __func__, __LINE__);
				return -1;
			}

			ei++;
			DNPRINTF(QWX_D_QMI,
			    "%s: next ei data_type=0x%x tlv_type=0x%x "
			    "dst elem_size=%u(0x%x) src total size=%u "
			    "remain=%zu\n", __func__, ei->data_type,
			    ei->tlv_type, ei->elem_size, ei->elem_size,
			    datalen, remain);

			/* Related EIs must have the same type. */
			if (ei->tlv_type != elem_type) {
				printf("%s: unexepected element type 0x%x; "
				    "expected 0x%x\n", __func__,
				    ei->tlv_type, elem_type);
				return -1;
			}

			if (datalen == 0) {
				if (ei->data_type != QMI_EOTI)
					ei++;
				continue;
			}

			/*
			 * For variable length arrays a one- or two-byte
			 * value follows the header, indicating the number
			 * of elements in the array.
			 */
			if (ei->array_type == VAR_LEN_ARRAY) {
				DNPRINTF(QWX_D_QMI,
				    "%s: variable length array\n", __func__);
				used = 0;
				if (qwx_qmi_decode_datalen(sc, &used, &nelem,
				    output, output_len, datalen_ei, p, remain))
					return -1;
				p += used;
				remain -= used;
				/*
				 * Previous datalen value included the total
				 * amount of bytes following the DATALEN TLV
				 * header.
				 */
				datalen -= used;

				if (nelem == 0) {
					if (ei->data_type != QMI_EOTI)
						ei++;
					continue;
				}

				DNPRINTF(QWX_D_QMI,
				    "%s: datalen %u used %zu bytes\n",
				    __func__, nelem, used);

				DNPRINTF(QWX_D_QMI,
				    "%s: decoding %u array elements with "
				    "src size %u dest size %u\n", __func__,
				    nelem, datalen / nelem, ei->elem_size);
			}
		}

		if (remain < datalen) {
			printf("%s:%d QMI message too short: remain=%zu, "
			    "datalen=%u\n", __func__, __LINE__, remain,
			    datalen);
			return -1;
		}
		if (output_len < nelem * ei->elem_size) {
			printf("%s: QMI output buffer too short: remain=%zu "
			    "nelem=%u ei->elem_size=%u\n", __func__, remain,
			    nelem, ei->elem_size);
			return -1;
		}

		for (i = 0; i < nelem && remain > 0; i++) {
			size_t outoff;

			outoff = ei->offset + (ei->elem_size * i);
			switch (ei->data_type) {
			case QMI_STRUCT:
				used = 0;
				if (qwx_qmi_decode_struct(sc, &used,
				    output + outoff, output_len - outoff,
				    ei, p, remain, 0))
					return -1;
				remain -= used;
				p += used;
				if (used != datalen) {
					DNPRINTF(QWX_D_QMI,
					    "%s struct used only %zu bytes "
					    "of %u input bytes\n", __func__,
					    used, datalen);
				} else {
					DNPRINTF(QWX_D_QMI,
					    "%s: struct used %zu bytes "
					    "of input\n", __func__, used);
				}
				break;
			case QMI_STRING:
				used = 0;
				if (qwx_qmi_decode_string(sc, &used,
				    output + outoff, output_len - outoff,
				    ei, p, remain, datalen, 0))
					return -1;
				remain -= used;
				p += used;
				if (used != datalen) {
					DNPRINTF(QWX_D_QMI,
					    "%s: string used only %zu bytes "
					    "of %u input bytes\n", __func__,
					    used, datalen);
				} else {
					DNPRINTF(QWX_D_QMI,
					    "%s: string used %zu bytes "
					    "of input\n", __func__, used);
				}
				break;
			case QMI_UNSIGNED_1_BYTE:
				if (remain < ei->elem_size) {
					printf("%s: QMI message too "
					    "short\n", __func__);
					return -1;
				}
				if (qwx_qmi_decode_byte(output + outoff,
				    ei, p))
					return -1;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			case QMI_UNSIGNED_2_BYTE:
			case QMI_SIGNED_2_BYTE_ENUM:
				if (remain < ei->elem_size) {
					printf("%s: QMI message too "
					    "short\n", __func__);
					return -1;
				}
				if (qwx_qmi_decode_word(output + outoff,
				    ei, p))
					return -1;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			case QMI_UNSIGNED_4_BYTE:
			case QMI_SIGNED_4_BYTE_ENUM:
				if (remain < ei->elem_size) {
					printf("%s: QMI message too "
					    "short\n", __func__);
					return -1;
				}
				if (qwx_qmi_decode_dword(output + outoff,
				    ei, p))
					return -1;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			case QMI_UNSIGNED_8_BYTE:
				if (remain < ei->elem_size) {
					printf("%s: QMI message too "
					    "short 4\n", __func__);
					return -1;
				}
				if (qwx_qmi_decode_qword(output + outoff,
				    ei, p))
					return -1;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			default:
				printf("%s: unhandled QMI message element "
				    "data type 0x%x\n",
				    sc->sc_dev.dv_xname, ei->data_type);
				return -1;
			}
		}

		ei++;
		DNPRINTF(QWX_D_QMI,
		    "%s: next ei 0x%x ei->data_type=0x%x remain=%zu\n",
		    __func__, ei->tlv_type, ei->data_type, remain);

		DNPRINTF(QWX_D_QMI, "%s: remaining input: ", __func__);
		for (int i = 0; i < remain; i++)
			DNPRINTF(QWX_D_QMI, " %02x", p[i]);
		DNPRINTF(QWX_D_QMI, "\n");
	}

	return 0;
}

void
qwx_qmi_recv_wlanfw_ind_register_req_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_ind_register_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_ind_register_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));
	DNPRINTF(QWX_D_QMI, "%s: resp.fw_status=0x%llx\n",
	   __func__, le64toh(resp.fw_status));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_host_cap_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_host_cap_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_host_cap_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_respond_mem_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_respond_mem_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_respond_mem_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_cap_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_cap_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	memset(&resp, 0, sizeof(resp));

	ei = qmi_wlanfw_cap_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	if (resp.chip_info_valid) {
		sc->qmi_target.chip_id = resp.chip_info.chip_id;
		sc->qmi_target.chip_family = resp.chip_info.chip_family;
	}

	if (resp.board_info_valid)
		sc->qmi_target.board_id = resp.board_info.board_id;
	else
		sc->qmi_target.board_id = 0xFF;

	if (resp.soc_info_valid)
		sc->qmi_target.soc_id = resp.soc_info.soc_id;

	if (resp.fw_version_info_valid) {
		sc->qmi_target.fw_version = resp.fw_version_info.fw_version;
		strlcpy(sc->qmi_target.fw_build_timestamp,
			resp.fw_version_info.fw_build_timestamp,
			sizeof(sc->qmi_target.fw_build_timestamp));
	}

	if (resp.fw_build_id_valid)
		strlcpy(sc->qmi_target.fw_build_id, resp.fw_build_id,
			sizeof(sc->qmi_target.fw_build_id));

	if (resp.eeprom_read_timeout_valid) {
		sc->qmi_target.eeprom_caldata = resp.eeprom_read_timeout;
		DNPRINTF(QWX_D_QMI,
		    "%s: qmi cal data supported from eeprom\n", __func__);
	}

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_bdf_download_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_bdf_download_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	memset(&resp, 0, sizeof(resp));

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_bdf_download_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_m3_info_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_m3_info_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	memset(&resp, 0, sizeof(resp));

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_m3_info_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_wlan_ini_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_wlan_ini_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	memset(&resp, 0, sizeof(resp));

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_wlan_ini_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_wlan_cfg_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_wlan_cfg_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	memset(&resp, 0, sizeof(resp));

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_wlan_cfg_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_wlanfw_wlan_mode_resp_v1(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_wlan_mode_resp_msg_v01 resp;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	memset(&resp, 0, sizeof(resp));

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	ei = qmi_wlanfw_wlan_mode_resp_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, &resp, sizeof(resp), ei, msg, msg_len))
		return;

	DNPRINTF(QWX_D_QMI, "%s: resp.resp.result=0x%x\n",
	    __func__, le16toh(resp.resp.result));
	DNPRINTF(QWX_D_QMI, "%s: resp.resp.error=0x%x\n",
	    __func__, le16toh(resp.resp.error));

	sc->qmi_resp.result = le16toh(resp.resp.result);
	sc->qmi_resp.error = le16toh(resp.resp.error);
	wakeup(&sc->qmi_resp);
}

void
qwx_qmi_recv_response(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_id, uint16_t msg_len)
{
	switch (msg_id) {
	case QMI_WLANFW_IND_REGISTER_REQ_V01:
		qwx_qmi_recv_wlanfw_ind_register_req_v1(sc, m, txn_id, msg_len);
		break;
	case QMI_WLFW_HOST_CAP_RESP_V01:
		qwx_qmi_recv_wlanfw_host_cap_resp_v1(sc, m, txn_id, msg_len);
		break;
	case QMI_WLFW_RESPOND_MEM_RESP_V01:
		qwx_qmi_recv_wlanfw_respond_mem_resp_v1(sc, m, txn_id, msg_len);
		break;
	case QMI_WLANFW_CAP_RESP_V01:
		qwx_qmi_recv_wlanfw_cap_resp_v1(sc, m, txn_id, msg_len);
		break;
	case QMI_WLANFW_BDF_DOWNLOAD_RESP_V01:
		qwx_qmi_recv_wlanfw_bdf_download_resp_v1(sc, m, txn_id,
		    msg_len);
		break;
	case QMI_WLANFW_M3_INFO_RESP_V01:
		qwx_qmi_recv_wlanfw_m3_info_resp_v1(sc, m, txn_id, msg_len);
		break;
	case QMI_WLANFW_WLAN_INI_RESP_V01:
		qwx_qmi_recv_wlanfw_wlan_ini_resp_v1(sc, m, txn_id, msg_len);
		break;
	case QMI_WLANFW_WLAN_CFG_RESP_V01:
		qwx_qmi_recv_wlanfw_wlan_cfg_resp_v1(sc, m, txn_id, msg_len);
		break;
	case QMI_WLANFW_WLAN_MODE_RESP_V01:
		qwx_qmi_recv_wlanfw_wlan_mode_resp_v1(sc, m, txn_id, msg_len);
		break;
	default:
		printf("%s: unhandled QMI response 0x%x\n",
		    sc->sc_dev.dv_xname, msg_id);
		break;
	}
}

void
qwx_qmi_recv_wlanfw_request_mem_indication(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_len)
{
	struct qmi_wlanfw_request_mem_ind_msg_v01 *ind = NULL;
	const struct qmi_elem_info *ei;
	uint8_t *msg = mtod(m, uint8_t *);

	DNPRINTF(QWX_D_QMI, "%s\n", __func__);

	/* This structure is too large for the stack. */
	ind = malloc(sizeof(*ind), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ind == NULL)
		return;

	ei = qmi_wlanfw_request_mem_ind_msg_v01_ei;
	if (qwx_qmi_decode_msg(sc, ind, sizeof(*ind), ei, msg, msg_len)) {
		free(ind, M_DEVBUF, sizeof(*ind));
		return;
	}

	/* Handled by qwx_qmi_mem_seg_send() in process context */
	sc->sc_req_mem_ind = ind;
	wakeup(&sc->sc_req_mem_ind);
}

void
qwx_qmi_recv_indication(struct qwx_softc *sc, struct mbuf *m,
    uint16_t txn_id, uint16_t msg_id, uint16_t msg_len)
{
	switch (msg_id) {
	case QMI_WLFW_REQUEST_MEM_IND_V01:
		qwx_qmi_recv_wlanfw_request_mem_indication(sc, m,
		    txn_id, msg_len);
		break;
	case QMI_WLFW_FW_MEM_READY_IND_V01:
		sc->fwmem_ready = 1;
		wakeup(&sc->fwmem_ready);
		break;
	case QMI_WLFW_FW_INIT_DONE_IND_V01:
		sc->fw_init_done = 1;
		wakeup(&sc->fw_init_done);
		break;
	default:
		printf("%s: unhandled QMI indication 0x%x\n",
		    sc->sc_dev.dv_xname, msg_id);
		break;
	}
}

void
qwx_qrtr_recv_data(struct qwx_softc *sc, struct mbuf *m, size_t size)
{
	struct qmi_header hdr;
	uint16_t txn_id, msg_id, msg_len;

	if (size < sizeof(hdr)) {
		printf("%s: QMI message too short: %zu bytes\n",
		    sc->sc_dev.dv_xname, size);
		return;
	}

	memcpy(&hdr, mtod(m, void *), sizeof(hdr));

	DNPRINTF(QWX_D_QMI,
	    "%s: QMI message type=0x%x txn=0x%x id=0x%x len=%u\n",
	    __func__, hdr.type, le16toh(hdr.txn_id),
	    le16toh(hdr.msg_id), le16toh(hdr.msg_len));

	txn_id = le16toh(hdr.txn_id);
	msg_id = le16toh(hdr.msg_id);
	msg_len = le16toh(hdr.msg_len);
	if (sizeof(hdr) + msg_len != size) {
		printf("%s: bad length in QMI message header: %u\n",
		    sc->sc_dev.dv_xname, msg_len);
		return;
	}

	switch (hdr.type) {
	case QMI_RESPONSE:
		m_adj(m, sizeof(hdr));
		qwx_qmi_recv_response(sc, m, txn_id, msg_id, msg_len);
		break;
	case QMI_INDICATION:
		m_adj(m, sizeof(hdr));
		qwx_qmi_recv_indication(sc, m, txn_id, msg_id, msg_len);
		break;
	default:
		printf("%s: unhandled QMI message type %u\n",
		    sc->sc_dev.dv_xname, hdr.type);
		break;
	}
}

int
qwx_qrtr_say_hello(struct qwx_softc *sc)
{
	struct qrtr_hdr_v1 hdr;
	struct qrtr_ctrl_pkt pkt;
	struct mbuf *m;
	size_t totlen, padlen;
	int err;

	totlen = sizeof(hdr) + sizeof(pkt);
	padlen = roundup(totlen, 4);

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		err = ENOBUFS;
		goto done;
	}

	if (padlen <= MCLBYTES)
		MCLGET(m, M_DONTWAIT);
	else
		MCLGETL(m, M_DONTWAIT, padlen);
	if ((m->m_flags & M_EXT) == 0) {
		err = ENOBUFS;
		goto done;
	}

	m->m_len = m->m_pkthdr.len = padlen;

	memset(&hdr, 0, sizeof(hdr));
	hdr.version = htole32(QRTR_PROTO_VER_1);
	hdr.type = htole32(QRTR_TYPE_HELLO);
	hdr.src_node_id = htole32(0x01); /* TODO make human-readable */
	hdr.src_port_id = htole32(0xfffffffeU); /* TODO make human-readable */
	hdr.dst_node_id = htole32(0x07); /* TODO make human-readable */
	hdr.dst_port_id = htole32(0xfffffffeU); /* TODO make human-readable */
	hdr.size = htole32(sizeof(pkt));

	err = m_copyback(m, 0, sizeof(hdr), &hdr, M_NOWAIT);
	if (err)
		goto done;

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = htole32(QRTR_TYPE_HELLO);

	err = m_copyback(m, sizeof(hdr), sizeof(pkt), &pkt, M_NOWAIT);
	if (err)
		goto done;

	/* Zero-pad the mbuf */
	if (padlen != totlen) {
		uint32_t pad = 0;
		err = m_copyback(m, totlen, padlen - totlen, &pad, M_NOWAIT);
		if (err)
			goto done;
	}

	err = sc->ops.submit_xfer(sc, m);
done:
	if (err)
		m_freem(m);
	return err;
}

int
qwx_qrtr_resume_tx(struct qwx_softc *sc)
{
	struct qrtr_hdr_v1 hdr;
	struct qrtr_ctrl_pkt pkt;
	struct mbuf *m;
	size_t totlen, padlen;
	int err;

	totlen = sizeof(hdr) + sizeof(pkt);
	padlen = roundup(totlen, 4);

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		err = ENOBUFS;
		goto done;
	}

	if (padlen <= MCLBYTES)
		MCLGET(m, M_DONTWAIT);
	else
		MCLGETL(m, M_DONTWAIT, padlen);
	if ((m->m_flags & M_EXT) == 0) {
		err = ENOBUFS;
		goto done;
	}

	m->m_len = m->m_pkthdr.len = padlen;

	memset(&hdr, 0, sizeof(hdr));
	hdr.version = htole32(QRTR_PROTO_VER_1);
	hdr.type = htole32(QRTR_TYPE_RESUME_TX);
	hdr.src_node_id = htole32(0x01); /* TODO make human-readable */
	hdr.src_port_id = htole32(0x4000); /* TODO make human-readable */
	hdr.dst_node_id = htole32(0x07); /* TODO make human-readable */
	hdr.dst_port_id = htole32(0x01); /* TODO make human-readable */
	hdr.size = htole32(sizeof(pkt));

	err = m_copyback(m, 0, sizeof(hdr), &hdr, M_NOWAIT);
	if (err)
		goto done;

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = htole32(QRTR_TYPE_RESUME_TX);
	pkt.client.node = htole32(0x01);
	pkt.client.port = htole32(0x4000);

	err = m_copyback(m, sizeof(hdr), sizeof(pkt), &pkt, M_NOWAIT);
	if (err)
		goto done;

	/* Zero-pad the mbuf */
	if (padlen != totlen) {
		uint32_t pad = 0;
		err = m_copyback(m, totlen, padlen - totlen, &pad, M_NOWAIT);
		if (err)
			goto done;
	}

	err = sc->ops.submit_xfer(sc, m);
done:
	if (err)
		m_freem(m);
	return err;
}

void
qwx_qrtr_recv_msg(struct qwx_softc *sc, struct mbuf *m)
{
	struct qrtr_hdr_v1 *v1 = mtod(m, struct qrtr_hdr_v1 *);
	struct qrtr_hdr_v2 *v2 = mtod(m, struct qrtr_hdr_v2 *);
	struct qrtr_ctrl_pkt *pkt;
	uint32_t type, size, hdrsize;
	uint8_t ver, confirm_rx;

	ver = *mtod(m, uint8_t *);
	switch (ver) {
	case QRTR_PROTO_VER_1:
		DNPRINTF(QWX_D_QMI,
		    "%s: type %u size %u confirm_rx %u\n", __func__,
		    letoh32(v1->type), letoh32(v1->size),
		    letoh32(v1->confirm_rx));
		type = letoh32(v1->type);
		size = letoh32(v1->size);
		confirm_rx = !!letoh32(v1->confirm_rx);
		hdrsize = sizeof(*v1);
		break;
	case QRTR_PROTO_VER_2:
		DNPRINTF(QWX_D_QMI,
		    "%s: type %u size %u confirm_rx %u\n", __func__,
		    v2->type, letoh32(v2->size),
		    !!(v2->flags & QRTR_FLAGS_CONFIRM_RX));
		type = v2->type;
		size = letoh32(v2->size);
		confirm_rx = !!(v2->flags & QRTR_FLAGS_CONFIRM_RX);
		hdrsize = sizeof(*v2);
		break;
	default:
		printf("%s: unsupported qrtr version %u\n",
		    sc->sc_dev.dv_xname, ver);
		return;
	}

	if (size > m->m_pkthdr.len) {
		printf("%s: bad size in qrtr message header: %u\n",
		    sc->sc_dev.dv_xname, size);
		return;
	}

	switch (type) {
	case QRTR_TYPE_DATA:
		m_adj(m, hdrsize);
		qwx_qrtr_recv_data(sc, m, size);
		break;
	case QRTR_TYPE_HELLO:
		qwx_qrtr_say_hello(sc);
		break;
	case QRTR_TYPE_NEW_SERVER:
		m_adj(m, hdrsize);
		pkt = mtod(m, struct qrtr_ctrl_pkt *);
		sc->qrtr_server.service = le32toh(pkt->server.service);
		sc->qrtr_server.instance = le32toh(pkt->server.instance);
		sc->qrtr_server.node = le32toh(pkt->server.node);
		sc->qrtr_server.port = le32toh(pkt->server.port);
		DNPRINTF(QWX_D_QMI,
		    "%s: new server: service=0x%x instance=0x%x node=0x%x "
		    "port=0x%x\n", __func__, sc->qrtr_server.service,
		    sc->qrtr_server.instance,
		    sc->qrtr_server.node, sc->qrtr_server.port);
		wakeup(&sc->qrtr_server);
		break;
	default:
		printf("%s: unhandled qrtr type %u\n",
		    sc->sc_dev.dv_xname, type);
		return;
	}

	if (confirm_rx)
		qwx_qrtr_resume_tx(sc);
}

// Not needed because we don't implenent QMI as a network service.
#define qwx_qmi_init_service(sc)	(0)
#define qwx_qmi_deinit_service(sc)	(0)

int
qwx_qmi_encode_datalen(uint8_t *p, uint32_t *datalen,
    const struct qmi_elem_info *ei, void *input)
{
	memcpy(datalen, input + ei->offset, sizeof(uint32_t));

	if (ei->elem_size == sizeof(uint8_t)) {
		p[0] = (*datalen & 0xff);
	} else if (ei->elem_size == sizeof(uint16_t)) {
		p[0] = (*datalen & 0xff);
		p[1] = (*datalen >> 8) & 0xff;
	} else {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	return 0;
}

int
qwx_qmi_encode_byte(uint8_t *p, const struct qmi_elem_info *ei, void *input,
    int i)
{
	if (ei->elem_size != sizeof(uint8_t)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	if (p == NULL)
		return 0;

	memcpy(p, input + ei->offset + (i * ei->elem_size), ei->elem_size);
	return 0;
}

int
qwx_qmi_encode_word(uint8_t *p, const struct qmi_elem_info *ei, void *input,
    int i)
{
	uint16_t val;

	if (ei->elem_size != sizeof(val)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	if (p == NULL)
		return 0;

	memcpy(&val, input + ei->offset + (i * ei->elem_size), ei->elem_size);
	val = htole16(val);
	memcpy(p, &val, sizeof(val));
	return 0;
}

int
qwx_qmi_encode_dword(uint8_t *p, const struct qmi_elem_info *ei, void *input,
    int i)
{
	uint32_t val;

	if (ei->elem_size != sizeof(val)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	if (p == NULL)
		return 0;

	memcpy(&val, input + ei->offset + (i * ei->elem_size), ei->elem_size);
	val = htole32(val);
	memcpy(p, &val, sizeof(val));
	return 0;
}

int
qwx_qmi_encode_qword(uint8_t *p, const struct qmi_elem_info *ei, void *input,
    int i)
{
	uint64_t val;

	if (ei->elem_size != sizeof(val)) {
		printf("%s: bad element size\n", __func__);
		return -1;
	}

	if (p == NULL)
		return 0;

	memcpy(&val, input + ei->offset + (i * ei->elem_size), ei->elem_size);
	val = htole64(val);
	memcpy(p, &val, sizeof(val));
	return 0;
}

int
qwx_qmi_encode_struct(uint8_t *p, size_t *encoded_len,
    const struct qmi_elem_info *struct_ei, void *input, size_t input_len)
{
	const struct qmi_elem_info *ei = struct_ei->ei_array;
	size_t remain = input_len;

	*encoded_len = 0;

	while (ei->data_type != QMI_EOTI) {
		if (ei->data_type == QMI_OPT_FLAG) {
			uint8_t do_encode, tlv_type;

			memcpy(&do_encode, input + ei->offset, sizeof(uint8_t));
			ei++; /* Advance to element we might have to encode. */
			if (ei->data_type == QMI_OPT_FLAG ||
			    ei->data_type == QMI_EOTI) {
				printf("%s: bad optional flag element\n",
				    __func__);
				return -1;
			}
			if (!do_encode) {
				/* The element will not be encoded. Skip it. */
				tlv_type = ei->tlv_type;
				while (ei->data_type != QMI_EOTI &&
				    ei->tlv_type == tlv_type)
					ei++;
				continue;
			}
		}

		if (ei->elem_size > remain) {
			printf("%s: QMI message buffer too short\n", __func__);
			return -1;
		}

		switch (ei->data_type) {
		case QMI_UNSIGNED_1_BYTE:
			if (qwx_qmi_encode_byte(p, ei, input, 0))
				return -1;
			break;
		case QMI_UNSIGNED_2_BYTE:
			if (qwx_qmi_encode_word(p, ei, input, 0))
				return -1;
			break;
		case QMI_UNSIGNED_4_BYTE:
		case QMI_SIGNED_4_BYTE_ENUM:
			if (qwx_qmi_encode_dword(p, ei, input, 0))
				return -1;
			break;
		case QMI_UNSIGNED_8_BYTE:
			if (qwx_qmi_encode_qword(p, ei, input, 0))
				return -1;
			break;
		default:
			printf("%s: unhandled QMI struct element type %d\n",
			    __func__, ei->data_type);
			return -1;
		}

		remain -= ei->elem_size;
		if (p != NULL)
			p += ei->elem_size;
		*encoded_len += ei->elem_size;
		ei++;
	}

	return 0;
}

int
qwx_qmi_encode_string(uint8_t *p, size_t *encoded_len,
    const struct qmi_elem_info *string_ei, void *input, size_t input_len)
{
	*encoded_len = strnlen(input, input_len);
	if (*encoded_len > string_ei->elem_len) {
		printf("%s: QMI message buffer too short\n", __func__);
		return -1;
	}

	if (p)
		memcpy(p, input, *encoded_len);

	return 0;
}

int
qwx_qmi_encode_msg(uint8_t **encoded_msg, size_t *encoded_len, int type,
    uint16_t *txn_id, uint16_t msg_id, size_t msg_len,
    const struct qmi_elem_info *ei, void *input, size_t input_len)
{
	const struct qmi_elem_info *ei0 = ei;
	struct qmi_header hdr;
	size_t remain;
	uint8_t *p, *op;

	*encoded_msg = NULL;
	*encoded_len = 0;

	/* First pass: Determine length of encoded message. */
	while (ei->data_type != QMI_EOTI) {
		int nelem = 1, i;

		if (ei->offset + ei->elem_size > input_len) {
			printf("%s: bad input buffer offset at element 0x%x "
			    "data type 0x%x\n",
			    __func__, ei->tlv_type, ei->data_type);
			goto err;
		}

		/*
		 * OPT_FLAG determines whether the next element
		 * should be considered for encoding.
		 */
		if (ei->data_type == QMI_OPT_FLAG) {
			uint8_t do_encode, tlv_type;

			memcpy(&do_encode, input + ei->offset, sizeof(uint8_t));
			ei++; /* Advance to element we might have to encode. */
			if (ei->data_type == QMI_OPT_FLAG ||
			    ei->data_type == QMI_EOTI) {
				printf("%s: bad optional element\n", __func__);
				goto err;
			}
			if (!do_encode) {
				/* The element will not be encoded. Skip it. */
				tlv_type = ei->tlv_type;
				while (ei->data_type != QMI_EOTI &&
				    ei->tlv_type == tlv_type)
					ei++;
				continue;
			}
		}

		*encoded_len += 3; /* type, length */
		if (ei->data_type == QMI_DATA_LEN) {
			uint32_t datalen = 0;
			uint8_t dummy[2];

			if (qwx_qmi_encode_datalen(dummy, &datalen, ei, input))
				goto err;
			*encoded_len += ei->elem_size;
			ei++;
			if (ei->array_type != VAR_LEN_ARRAY) {
				printf("%s: data len not for a var array\n",
				    __func__);
				goto err;
			}
			nelem = datalen;
			if (ei->data_type == QMI_STRUCT) {
				for (i = 0; i < nelem; i++) {
					size_t encoded_struct_len = 0;
					size_t inoff = ei->offset + (i * ei->elem_size);

					if (qwx_qmi_encode_struct(NULL,
					    &encoded_struct_len, ei,
					    input + inoff, input_len - inoff))
						goto err;

					*encoded_len += encoded_struct_len;
				}
			} else
				*encoded_len += nelem * ei->elem_size;
			ei++;
		} else if (ei->data_type == QMI_STRING) {
			size_t encoded_string_len = 0;
			size_t inoff = ei->offset;

			if (qwx_qmi_encode_string(NULL,
			    &encoded_string_len, ei,
			    input + inoff, input_len - inoff))
				goto err;
			*encoded_len += encoded_string_len;
			ei++;
		} else {
			*encoded_len += ei->elem_size;
			ei++;
		}
	}

	*encoded_len += sizeof(hdr);
	*encoded_msg = malloc(*encoded_len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (*encoded_msg == NULL)
		return ENOMEM;

	hdr.type = type;
	hdr.txn_id = htole16(*txn_id);
	hdr.msg_id = htole16(msg_id);
	hdr.msg_len = htole16(*encoded_len - sizeof(hdr));
	memcpy(*encoded_msg, &hdr, sizeof(hdr));

	/* Second pass: Encode the message. */
	ei = ei0;
	p = *encoded_msg + sizeof(hdr);
	remain = *encoded_len - sizeof(hdr);
	while (ei->data_type != QMI_EOTI) {
		uint32_t datalen = 0;
		int nelem = 1, i;

		if (ei->data_type == QMI_OPT_FLAG) {
			uint8_t do_encode, tlv_type;

			memcpy(&do_encode, input + ei->offset, sizeof(uint8_t));
			ei++; /* Advance to element we might have to encode. */
			if (ei->data_type == QMI_OPT_FLAG ||
			    ei->data_type == QMI_EOTI) {
				printf("%s: bad optional flag element\n",
				    __func__);
				goto err;
			}
			if (!do_encode) {
				/* The element will not be encoded. Skip it. */
				tlv_type = ei->tlv_type;
				while (ei->data_type != QMI_EOTI &&
				    ei->tlv_type == tlv_type)
					ei++;
				continue;
			}
		}

		if (ei->elem_size + 3 > remain) {
			printf("%s: QMI message buffer too short\n", __func__);
			goto err;
		}

		/* 3 bytes of type-length-value header, remember for later */
		op = p;
		p += 3;

		if (ei->data_type == QMI_DATA_LEN) {
			if (qwx_qmi_encode_datalen(p, &datalen, ei, input))
				goto err;
			p += ei->elem_size;
			ei++;
			if (ei->array_type == VAR_LEN_ARRAY)
				nelem = datalen;
		}

		for (i = 0; i < nelem; i++) {
			size_t encoded_struct_len = 0;
			size_t encoded_string_len = 0;
			size_t inoff = ei->offset + (i * ei->elem_size);

			switch (ei->data_type) {
			case QMI_UNSIGNED_1_BYTE:
				if (qwx_qmi_encode_byte(p, ei, input, i))
					goto err;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			case QMI_UNSIGNED_2_BYTE:
			case QMI_SIGNED_2_BYTE_ENUM:
				if (qwx_qmi_encode_word(p, ei, input, i))
					goto err;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			case QMI_UNSIGNED_4_BYTE:
			case QMI_SIGNED_4_BYTE_ENUM:
				if (qwx_qmi_encode_dword(p, ei, input, i))
					goto err;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			case QMI_UNSIGNED_8_BYTE:
				if (qwx_qmi_encode_qword(p, ei, input, i))
					goto err;
				remain -= ei->elem_size;
				p += ei->elem_size;
				break;
			case QMI_STRUCT:
				if (qwx_qmi_encode_struct(p,
				    &encoded_struct_len, ei,
				    input + inoff, input_len - inoff))
					goto err;
				remain -= encoded_struct_len;
				p += encoded_struct_len;
				break;
			case QMI_STRING:
				if (qwx_qmi_encode_string(p,
				    &encoded_string_len, ei,
				    input + inoff, input_len - inoff))
					goto err;
				remain -= encoded_string_len;
				p += encoded_string_len;
				break;
			default:
				printf("%s: unhandled QMI message element type %d\n",
				    __func__, ei->data_type);
				goto err;
			}
		}

		op[0] = ei->tlv_type;
		op[1] = (p - (op + 3)) & 0xff;
		op[2] = ((p - (op + 3)) >> 8) & 0xff;

		ei++;
	}

	if (0) {
		int i;
		DNPRINTF(QWX_D_QMI,
		   "%s: message type 0x%x txnid 0x%x msgid 0x%x "
		    "msglen %zu encoded:", __func__,
		    type, *txn_id, msg_id, *encoded_len - sizeof(hdr));
		for (i = 0; i < *encoded_len; i++) {
			DNPRINTF(QWX_D_QMI, "%s %.2x", i % 16 == 0 ? "\n" : "",
			    (*encoded_msg)[i]);
		}
		if (i % 16)
			DNPRINTF(QWX_D_QMI, "\n");
	}

	(*txn_id)++; /* wrap-around is fine */
	return 0;
err:
	free(*encoded_msg, M_DEVBUF, *encoded_len);
	*encoded_msg = NULL;
	*encoded_len = 0;
	return -1;
}

int
qwx_qmi_send_request(struct qwx_softc *sc, uint16_t msg_id, size_t msg_len,
    const struct qmi_elem_info *ei, void *req, size_t req_len)
{
	struct qrtr_hdr_v1 hdr;
	struct mbuf *m;
	uint8_t *encoded_msg;
	size_t encoded_len;
	size_t totlen, padlen;
	int err;

	if (qwx_qmi_encode_msg(&encoded_msg, &encoded_len, QMI_REQUEST,
	    &sc->qmi_txn_id, msg_id, msg_len, ei, req, req_len))
		return -1;

	totlen = sizeof(hdr) + encoded_len;
	padlen = roundup(totlen, 4);

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		err = ENOBUFS;
		goto done;
	}

	if (padlen <= MCLBYTES)
		MCLGET(m, M_DONTWAIT);
	else
		MCLGETL(m, M_DONTWAIT, padlen);
	if ((m->m_flags & M_EXT) == 0) {
		err = ENOBUFS;
		goto done;
	}

	m->m_len = m->m_pkthdr.len = padlen;

	memset(&hdr, 0, sizeof(hdr));
	hdr.version = htole32(QRTR_PROTO_VER_1);
	hdr.type = htole32(QRTR_TYPE_DATA);
	hdr.src_node_id = htole32(0x01); /* TODO make human-readable */
	hdr.src_port_id = htole32(0x4000); /* TODO make human-readable */
	hdr.dst_node_id = htole32(0x07); /* TODO make human-readable */
	hdr.dst_port_id = htole32(0x01); /* TODO make human-readable */
	hdr.size = htole32(encoded_len); 

	err = m_copyback(m, 0, sizeof(hdr), &hdr, M_NOWAIT);
	if (err)
		goto done;

	err = m_copyback(m, sizeof(hdr), encoded_len, encoded_msg, M_NOWAIT);
	if (err)
		goto done;

	/* Zero-pad the mbuf */
	if (padlen != totlen) {
		uint32_t pad = 0;
		err = m_copyback(m, totlen, padlen - totlen, &pad, M_NOWAIT);
		if (err)
			goto done;
	}

	err = sc->ops.submit_xfer(sc, m);
done:
	if (err)
		m_freem(m);
	free(encoded_msg, M_DEVBUF, encoded_len);
	return err;
}

int
qwx_qmi_fw_ind_register_send(struct qwx_softc *sc)
{
	struct qmi_wlanfw_ind_register_req_msg_v01 req;
	int ret;

	memset(&req, 0, sizeof(req));

	req.client_id_valid = 1;
	req.client_id = QMI_WLANFW_CLIENT_ID;
	req.fw_ready_enable_valid = 1;
	req.fw_ready_enable = 1;
	req.cal_done_enable_valid = 1;
	req.cal_done_enable = 1;
	req.fw_init_done_enable_valid = 1;
	req.fw_init_done_enable = 1;

	req.pin_connect_result_enable_valid = 0;
	req.pin_connect_result_enable = 0;

	/*
	 * WCN6750 doesn't request for DDR memory via QMI,
	 * instead it uses a fixed 12MB reserved memory region in DDR.
	 */
	if (!sc->hw_params.fixed_fw_mem) {
		req.request_mem_enable_valid = 1;
		req.request_mem_enable = 1;
		req.fw_mem_ready_enable_valid = 1;
		req.fw_mem_ready_enable = 1;
	}

	DNPRINTF(QWX_D_QMI, "%s: qmi indication register request\n", __func__);

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_IND_REGISTER_REQ_V01,
			       QMI_WLANFW_IND_REGISTER_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_ind_register_req_msg_v01_ei,
			       &req, sizeof(req));
	if (ret) {
		printf("%s: failed to send indication register request: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return -1;
	}

	sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxfwind",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: fw indication register request timeout\n",
			    sc->sc_dev.dv_xname);
			return ret;
		}
	}

	return 0;
}

int
qwx_qmi_host_cap_send(struct qwx_softc *sc)
{
	struct qmi_wlanfw_host_cap_req_msg_v01 req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.num_clients_valid = 1;
	req.num_clients = 1;
	req.mem_cfg_mode = sc->hw_params.fw_mem_mode;
	req.mem_cfg_mode_valid = 1;
	req.bdf_support_valid = 1;
	req.bdf_support = 1;

	if (sc->hw_params.m3_fw_support) {
		req.m3_support_valid = 1;
		req.m3_support = 1;
		req.m3_cache_support_valid = 1;
		req.m3_cache_support = 1;
	} else {
		req.m3_support_valid = 0;
		req.m3_support = 0;
		req.m3_cache_support_valid = 0;
		req.m3_cache_support = 0;
	}

	req.cal_done_valid = 1;
	req.cal_done = sc->qmi_cal_done;

	if (sc->hw_params.internal_sleep_clock) {
		req.nm_modem_valid = 1;

		/* Notify firmware that this is non-qualcomm platform. */
		req.nm_modem |= QWX_HOST_CSTATE_BIT;

		/* Notify firmware about the sleep clock selection,
		 * nm_modem_bit[1] is used for this purpose. Host driver on
		 * non-qualcomm platforms should select internal sleep
		 * clock.
		 */
		req.nm_modem |= QWX_SLEEP_CLOCK_SELECT_INTERNAL_BIT;
	}

	if (sc->hw_params.global_reset)
		req.nm_modem |= QWX_PLATFORM_CAP_PCIE_GLOBAL_RESET;

	req.nm_modem |= QWX_PLATFORM_CAP_PCIE_PME_D3COLD;

	DNPRINTF(QWX_D_QMI, "%s: qmi host cap request\n", __func__);

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_HOST_CAP_REQ_V01,
			       QMI_WLANFW_HOST_CAP_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_host_cap_req_msg_v01_ei,
			       &req, sizeof(req));
	if (ret) {
		printf("%s: failed to send host cap request: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return -1;
	}

	sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxfwhcap",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: fw host cap request timeout\n",
			    sc->sc_dev.dv_xname);
			return ret;
		}
	}

	return 0;
}

int
qwx_qmi_mem_seg_send(struct qwx_softc *sc)
{
	struct qmi_wlanfw_respond_mem_req_msg_v01 *req;
	struct qmi_wlanfw_request_mem_ind_msg_v01 *ind;
	uint32_t mem_seg_len;
	const uint32_t mem_seg_len_max = 64; /* bump if needed by future fw */
	uint16_t expected_result;
	size_t total_size;
	int i, ret;

	sc->fwmem_ready = 0;

	while (sc->sc_req_mem_ind == NULL) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxfwmem",
		    SEC_TO_NSEC(10));
		if (ret) {
			printf("%s: fw memory request timeout\n",
			    sc->sc_dev.dv_xname);
			return -1;
		}
	}

	ind = sc->sc_req_mem_ind;
	mem_seg_len = le32toh(ind->mem_seg_len);
	if (mem_seg_len > mem_seg_len_max) {
		printf("%s: firmware requested too many memory segments: %u\n",
		    sc->sc_dev.dv_xname, mem_seg_len);
		return -1;
	}

	total_size = 0;
	for (i = 0; i < mem_seg_len; i++) {
		if (ind->mem_seg[i].size == 0) {
			printf("%s: firmware requested zero-sized "
			    "memory segment %u\n", sc->sc_dev.dv_xname, i);
			return -1;
		}
		total_size += le32toh(ind->mem_seg[i].size);
	}

	req = malloc(sizeof(*req), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (req == NULL) {
		printf("%s: failed to allocate respond memory request\n",
		    sc->sc_dev.dv_xname);
		free(sc->sc_req_mem_ind, M_DEVBUF, sizeof(*sc->sc_req_mem_ind));
		sc->sc_req_mem_ind = NULL;
		return -1;
	}

	if (total_size == 0) {
		/* Should not happen. Send back an empty allocation. */
		printf("%s: firmware has requested no memory\n",
		    sc->sc_dev.dv_xname);
		mem_seg_len = 0;
	} else if (sc->fwmem == NULL || QWX_DMA_LEN(sc->fwmem) < total_size) {
		if (sc->fwmem != NULL) 
			qwx_dmamem_free(sc->sc_dmat, sc->fwmem);
		sc->fwmem = qwx_dmamem_alloc(sc->sc_dmat, total_size, 65536);
		if (sc->fwmem == NULL) {
			printf("%s: failed to allocate %zu bytes of DMA "
			    "memory for firmware\n", sc->sc_dev.dv_xname,
			    total_size);
			/* Send back an empty allocation. */
			mem_seg_len = 0;
		} else
			DPRINTF("%s: allocated %zu bytes of DMA memory for "
			    "firmware\n", sc->sc_dev.dv_xname, total_size);
	}

	/* Chunk DMA memory block into segments as requested by firmware. */
	req->mem_seg_len = htole32(mem_seg_len);
	if (sc->fwmem) {
		uint64_t paddr = QWX_DMA_DVA(sc->fwmem);

		for (i = 0; i < mem_seg_len; i++) {
			DPRINTF("%s: mem seg[%d] addr=%llx size=%u type=%u\n",
			    __func__, i, paddr, le32toh(ind->mem_seg[i].size),
			    le32toh(ind->mem_seg[i].type));
			req->mem_seg[i].addr = htole64(paddr);
			paddr += le32toh(ind->mem_seg[i].size);

			/* Values in 'ind' are in little-endian format. */
			req->mem_seg[i].size = ind->mem_seg[i].size;
			req->mem_seg[i].type = ind->mem_seg[i].type;
		}
	}

	free(ind, M_DEVBUF, sizeof(*ind));
	sc->sc_req_mem_ind = NULL;

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_RESPOND_MEM_REQ_V01,
			       QMI_WLANFW_RESPOND_MEM_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_respond_mem_req_msg_v01_ei,
			       req, sizeof(*req));
	free(req, M_DEVBUF, sizeof(*req));
	if (ret) {
		printf("%s: failed to send respond memory request: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return -1;
	}

	if (mem_seg_len == 0) {
		expected_result = QMI_RESULT_FAILURE_V01;
		sc->qmi_resp.result = QMI_RESULT_SUCCESS_V01;
	} else {
		expected_result = QMI_RESULT_SUCCESS_V01;
		sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	}
	while (sc->qmi_resp.result != expected_result) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxfwrespmem",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: fw respond memory request timeout\n",
			    sc->sc_dev.dv_xname);
			return -1;
		}
	}

	if (mem_seg_len == 0)
		return EBUSY;

	if (!sc->hw_params.fixed_fw_mem) {
		while (!sc->fwmem_ready) {
			ret = tsleep_nsec(&sc->fwmem_ready, 0, "qwxfwrdy",
			    SEC_TO_NSEC(10));
			if (ret) {
				printf("%s: fw memory ready timeout\n",
				    sc->sc_dev.dv_xname);
				return -1;
			}
		}
	}

	return 0;
}

int
qwx_core_check_smbios(struct qwx_softc *sc)
{
	return 0; /* TODO */
}

int
qwx_core_check_dt(struct qwx_softc *sc)
{
	return 0; /* TODO */
}

int
qwx_qmi_request_target_cap(struct qwx_softc *sc)
{
	struct qmi_wlanfw_cap_req_msg_v01 req;
	int ret = 0;
	int r;
	char *fw_build_id;
	int fw_build_id_mask_len;

	memset(&req, 0, sizeof(req));

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_CAP_REQ_V01,
	    QMI_WLANFW_CAP_REQ_MSG_V01_MAX_LEN,
	    qmi_wlanfw_cap_req_msg_v01_ei, &req, sizeof(req));
	if (ret) {
		printf("%s: failed to send qmi cap request: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto out;
	}

	sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxfwcap",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: qmi cap request failed\n",
			    sc->sc_dev.dv_xname);
			return ret;
		}
	}

	fw_build_id = sc->qmi_target.fw_build_id;
	fw_build_id_mask_len = strlen(QWX_FW_BUILD_ID_MASK);
	if (!strncmp(fw_build_id, QWX_FW_BUILD_ID_MASK, fw_build_id_mask_len))
		fw_build_id = fw_build_id + fw_build_id_mask_len;

	DPRINTF("%s: chip_id 0x%x chip_family 0x%x board_id 0x%x soc_id 0x%x\n",
	    sc->sc_dev.dv_xname,
	    sc->qmi_target.chip_id, sc->qmi_target.chip_family,
	    sc->qmi_target.board_id, sc->qmi_target.soc_id);

	DPRINTF("%s: fw_version 0x%x fw_build_timestamp %s fw_build_id %s\n",
	    sc->sc_dev.dv_xname, sc->qmi_target.fw_version,
	    sc->qmi_target.fw_build_timestamp, fw_build_id);

	r = qwx_core_check_smbios(sc);
	if (r)
		DPRINTF("%s: SMBIOS bdf variant name not set\n", __func__);

	r = qwx_core_check_dt(sc);
	if (r)
		DPRINTF("%s: DT bdf variant name not set\n", __func__);

out:
	return ret;
}

int
qwx_qmi_request_device_info(struct qwx_softc *sc)
{
	/* device info message req is only sent for hybrid bus devices */
	if (!sc->hw_params.hybrid_bus_type)
		return 0;

	/* TODO */
	return -1;
}

int
_qwx_core_create_board_name(struct qwx_softc *sc, char *name,
    size_t name_len, int with_variant, int bus_type_mode)
{
	/* strlen(',variant=') + strlen(ab->qmi.target.bdf_ext) */
	char variant[9 + ATH11K_QMI_BDF_EXT_STR_LENGTH] = { 0 };

	if (with_variant && sc->qmi_target.bdf_ext[0] != '\0')
		snprintf(variant, sizeof(variant), ",variant=%s",
		    sc->qmi_target.bdf_ext);

	switch (sc->id.bdf_search) {
	case ATH11K_BDF_SEARCH_BUS_AND_BOARD:
		if (bus_type_mode)
			snprintf(name, name_len, "bus=%s", sc->sc_bus_str);
		else
			snprintf(name, name_len,
			    "bus=%s,vendor=%04x,device=%04x,"
			    "subsystem-vendor=%04x,subsystem-device=%04x,"
			    "qmi-chip-id=%d,qmi-board-id=%d%s",
			    sc->sc_bus_str, sc->id.vendor, sc->id.device,
			    sc->id.subsystem_vendor, sc->id.subsystem_device,
			    sc->qmi_target.chip_id, sc->qmi_target.board_id,
			    variant);
		break;
	default:
		snprintf(name, name_len,
		    "bus=%s,qmi-chip-id=%d,qmi-board-id=%d%s",
		    sc->sc_bus_str, sc->qmi_target.chip_id,
		    sc->qmi_target.board_id, variant);
		break;
	}

	DPRINTF("%s: using board name '%s'\n", __func__, name);

	return 0;
}

int
qwx_core_create_board_name(struct qwx_softc *sc, char *name, size_t name_len)
{
	return _qwx_core_create_board_name(sc, name, name_len, 1, 0);
}

int
qwx_core_create_fallback_board_name(struct qwx_softc *sc, char *name,
    size_t name_len)
{
	return _qwx_core_create_board_name(sc, name, name_len, 0, 0);
}

int
qwx_core_create_bus_type_board_name(struct qwx_softc *sc, char *name,
    size_t name_len)
{
	return _qwx_core_create_board_name(sc, name, name_len, 0, 1);
}

struct ath11k_fw_ie {
	uint32_t id;
	uint32_t len;
	uint8_t data[];
};

enum ath11k_bd_ie_board_type {
	ATH11K_BD_IE_BOARD_NAME = 0,
	ATH11K_BD_IE_BOARD_DATA = 1,
};

enum ath11k_bd_ie_regdb_type {
	ATH11K_BD_IE_REGDB_NAME = 0,
	ATH11K_BD_IE_REGDB_DATA = 1,
};

enum ath11k_bd_ie_type {
	/* contains sub IEs of enum ath11k_bd_ie_board_type */
	ATH11K_BD_IE_BOARD = 0,
	/* contains sub IEs of enum ath11k_bd_ie_regdb_type */
	ATH11K_BD_IE_REGDB = 1,
};

static inline const char *
qwx_bd_ie_type_str(enum ath11k_bd_ie_type type)
{
	switch (type) {
	case ATH11K_BD_IE_BOARD:
		return "board data";
	case ATH11K_BD_IE_REGDB:
		return "regdb data";
	}

	return "unknown";
}

int
qwx_core_parse_bd_ie_board(struct qwx_softc *sc,
    const u_char **boardfw, size_t *boardfw_len,
    const void *buf, size_t buf_len,
    const char *boardname, int ie_id, int name_id, int data_id)
{
	const struct ath11k_fw_ie *hdr;
	int name_match_found = 0;
	int ret, board_ie_id;
	size_t board_ie_len;
	const void *board_ie_data;

	*boardfw = NULL;
	*boardfw_len = 0;

	/* go through ATH11K_BD_IE_BOARD_/ATH11K_BD_IE_REGDB_ elements */
	while (buf_len > sizeof(struct ath11k_fw_ie)) {
		hdr = buf;
		board_ie_id = le32toh(hdr->id);
		board_ie_len = le32toh(hdr->len);
		board_ie_data = hdr->data;

		buf_len -= sizeof(*hdr);
		buf += sizeof(*hdr);

		if (buf_len < roundup(board_ie_len, 4)) {
			printf("%s: invalid %s length: %zu < %zu\n",
			    sc->sc_dev.dv_xname, qwx_bd_ie_type_str(ie_id),
			    buf_len, roundup(board_ie_len, 4));
			return EINVAL;
		}

		if (board_ie_id == name_id) {
			if (board_ie_len != strlen(boardname))
				goto next;

			ret = memcmp(board_ie_data, boardname, board_ie_len);
			if (ret)
				goto next;

			name_match_found = 1;
			   DPRINTF("%s: found match %s for name '%s'", __func__,
			       qwx_bd_ie_type_str(ie_id), boardname);
		} else if (board_ie_id == data_id) {
			if (!name_match_found)
				/* no match found */
				goto next;

			DPRINTF("%s: found %s for '%s'", __func__,
			    qwx_bd_ie_type_str(ie_id), boardname);

			*boardfw = board_ie_data;
			*boardfw_len = board_ie_len;
			return 0;
		} else {
			printf("%s: unknown %s id found: %d\n", __func__,
			    qwx_bd_ie_type_str(ie_id), board_ie_id);
		}
next:
		/* jump over the padding */
		board_ie_len = roundup(board_ie_len, 4);

		buf_len -= board_ie_len;
		buf += board_ie_len;
	}

	/* no match found */
	return ENOENT;
}

int
qwx_core_fetch_board_data_api_n(struct qwx_softc *sc,
    const u_char **boardfw, size_t *boardfw_len,
    u_char *fwdata, size_t fwdata_len,
    const char *boardname, int ie_id_match, int name_id, int data_id)
{
	size_t len, magic_len;
	const uint8_t *data;
	char *filename;
	size_t ie_len;
	struct ath11k_fw_ie *hdr;
	int ret, ie_id;

	filename = ATH11K_BOARD_API2_FILE;

	*boardfw = NULL;
	*boardfw_len = 0;

	data = fwdata;
	len = fwdata_len;

	/* magic has extra null byte padded */
	magic_len = strlen(ATH11K_BOARD_MAGIC) + 1;
	if (len < magic_len) {
		printf("%s: failed to find magic value in %s, "
		    "file too short: %zu\n",
		    sc->sc_dev.dv_xname, filename, len);
		return EINVAL;
	}

	if (memcmp(data, ATH11K_BOARD_MAGIC, magic_len)) {
		DPRINTF("%s: found invalid board magic\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}

	/* magic is padded to 4 bytes */
	magic_len = roundup(magic_len, 4);
	if (len < magic_len) {
		printf("%s: %s too small to contain board data, len: %zu\n",
		    sc->sc_dev.dv_xname, filename, len);
		return EINVAL;
	}

	data += magic_len;
	len -= magic_len;

	while (len > sizeof(struct ath11k_fw_ie)) {
		hdr = (struct ath11k_fw_ie *)data;
		ie_id = le32toh(hdr->id);
		ie_len = le32toh(hdr->len);

		len -= sizeof(*hdr);
		data = hdr->data;

		if (len < roundup(ie_len, 4)) {
			printf("%s: invalid length for board ie_id %d "
			    "ie_len %zu len %zu\n",
			    sc->sc_dev.dv_xname, ie_id, ie_len, len);
			return EINVAL;
		}

		if (ie_id == ie_id_match) {
			ret = qwx_core_parse_bd_ie_board(sc,
			    boardfw, boardfw_len, data, ie_len,
			    boardname, ie_id_match, name_id, data_id);
			if (ret == ENOENT)
				/* no match found, continue */
				goto next;
			else if (ret)
				/* there was an error, bail out */
				return ret;
			/* either found or error, so stop searching */
			goto out;
		}
next:
		/* jump over the padding */
		ie_len = roundup(ie_len, 4);

		len -= ie_len;
		data += ie_len;
	}

out:
	if (!*boardfw || !*boardfw_len) {
		printf("%s: failed to fetch %s for %s from %s\n",
		    __func__, qwx_bd_ie_type_str(ie_id_match),
		    boardname, filename);
		return ENOENT;
	}

	return 0;
}

int
qwx_core_fetch_bdf(struct qwx_softc *sc, u_char **data, size_t *len,
    const u_char **boardfw, size_t *boardfw_len, const char *filename)
{
	char path[PATH_MAX];
	char boardname[200];
	int ret;

	ret = snprintf(path, sizeof(path), "%s/%s/%s",
	    ATH11K_FW_DIR, sc->hw_params.fw.dir, filename);
	if (ret < 0 || ret >= sizeof(path))
		return ENOSPC;

	ret = qwx_core_create_board_name(sc, boardname, sizeof(boardname));
	if (ret) {
		DPRINTF("%s: failed to create board name: %d",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = loadfirmware(path, data, len);
	if (ret) {
		printf("%s: could not read %s (error %d)\n",
		    sc->sc_dev.dv_xname, path, ret);
		return ret;
	}

	ret = qwx_core_fetch_board_data_api_n(sc, boardfw, boardfw_len,
	    *data, *len, boardname, ATH11K_BD_IE_BOARD,
	    ATH11K_BD_IE_BOARD_NAME, ATH11K_BD_IE_BOARD_DATA);
	if (ret) {
		DPRINTF("%s: failed to fetch board data for %s from %s\n",
		    sc->sc_dev.dv_xname, boardname, path);
		return ret;
	}

	return 0;
}

int
qwx_qmi_load_file_target_mem(struct qwx_softc *sc, const u_char *data,
    size_t len, int type)
{
	struct qmi_wlanfw_bdf_download_req_msg_v01 *req;
	const uint8_t *p = data;
#ifdef notyet
	void *bdf_addr = NULL;
#endif
	int ret;
	uint32_t remaining = len;

	req = malloc(sizeof(*req), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!req) {
		printf("%s: failed to allocate bfd download request\n",
		    sc->sc_dev.dv_xname);
		return ENOMEM;
	}

	if (sc->hw_params.fixed_bdf_addr) {
#ifdef notyet
		bdf_addr = ioremap(ab->hw_params.bdf_addr, ab->hw_params.fw.board_size);
		if (!bdf_addr) {
			ath11k_warn(ab, "qmi ioremap error for bdf_addr\n");
			ret = -EIO;
			goto err_free_req;
		}
#else
		printf("%s: fixed bdf address not yet supported\n",
		    sc->sc_dev.dv_xname);
		ret = EIO;
		goto err_free_req;
#endif
	}

	while (remaining) {
		req->valid = 1;
		req->file_id_valid = 1;
		req->file_id = sc->qmi_target.board_id;
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->bdf_type = type;
		req->bdf_type_valid = 1;
		req->end_valid = 1;
		req->end = 0;

		if (remaining > QMI_WLANFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLANFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		if (sc->hw_params.fixed_bdf_addr ||
		    type == ATH11K_QMI_FILE_TYPE_EEPROM) {
			req->data_valid = 0;
			req->end = 1;
			req->data_len = ATH11K_QMI_MAX_BDF_FILE_NAME_SIZE;
		} else {
			memcpy(req->data, p, req->data_len);
		}
#ifdef notyet
		if (ab->hw_params.fixed_bdf_addr) {
			if (type == ATH11K_QMI_FILE_TYPE_CALDATA)
				bdf_addr += ab->hw_params.fw.cal_offset;

			memcpy_toio(bdf_addr, p, len);
		}
#endif
		DPRINTF("%s: bdf download req fixed addr type %d\n",
		    __func__, type);

		ret = qwx_qmi_send_request(sc,
		    QMI_WLANFW_BDF_DOWNLOAD_REQ_V01,
		    QMI_WLANFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_LEN,
		    qmi_wlanfw_bdf_download_req_msg_v01_ei,
		    req, sizeof(*req));
		if (ret) {
			printf("%s: failed to send bdf download request\n",
			    sc->sc_dev.dv_xname);
			goto err_iounmap;
		}

		sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
		while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
			ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxbdf",
			    SEC_TO_NSEC(1));
			if (ret) {
				printf("%s: bdf download request timeout\n",
				    sc->sc_dev.dv_xname);
				goto err_iounmap;
			}
		}

		if (sc->hw_params.fixed_bdf_addr ||
		    type == ATH11K_QMI_FILE_TYPE_EEPROM) {
			remaining = 0;
		} else {
			remaining -= req->data_len;
			p += req->data_len;
			req->seg_id++;
			DPRINTF("%s: bdf download request remaining %i\n",
			    __func__, remaining);
		}
	}

err_iounmap:
#ifdef notyet
	if (ab->hw_params.fixed_bdf_addr)
		iounmap(bdf_addr);
#endif
err_free_req:
	free(req, M_DEVBUF, sizeof(*req));

	return ret;
}

#define QWX_ELFMAG	"\177ELF"
#define QWX_SELFMAG	4

int
qwx_qmi_load_bdf_qmi(struct qwx_softc *sc, int regdb)
{
	u_char *data;
	const u_char *boardfw;
	size_t len, boardfw_len;
	uint32_t fw_size;
	int ret = 0, bdf_type;
#ifdef notyet
	const uint8_t *tmp;
	uint32_t file_type;
#endif

	ret = qwx_core_fetch_bdf(sc, &data, &len, &boardfw, &boardfw_len,
	    regdb ? ATH11K_REGDB_FILE : ATH11K_BOARD_API2_FILE);
	if (ret)
		goto out;

	if (regdb)
		bdf_type = ATH11K_QMI_BDF_TYPE_REGDB;
	else if (len >= QWX_SELFMAG &&
	    memcmp(boardfw, QWX_ELFMAG, QWX_SELFMAG) == 0)
		bdf_type = ATH11K_QMI_BDF_TYPE_ELF;
	else
		bdf_type = ATH11K_QMI_BDF_TYPE_BIN;

	DPRINTF("%s: bdf_type %d\n", __func__, bdf_type);

	fw_size = MIN(sc->hw_params.fw.board_size, len);

	ret = qwx_qmi_load_file_target_mem(sc, boardfw, boardfw_len, bdf_type);
	if (ret < 0) {
		printf("%s: failed to load bdf file\n", __func__);
		goto out;
	}

	/* QCA6390/WCN6855 does not support cal data, skip it */
	if (bdf_type == ATH11K_QMI_BDF_TYPE_ELF || bdf_type == ATH11K_QMI_BDF_TYPE_REGDB)
		goto out;
#ifdef notyet
	if (ab->qmi.target.eeprom_caldata) {
		file_type = ATH11K_QMI_FILE_TYPE_EEPROM;
		tmp = filename;
		fw_size = ATH11K_QMI_MAX_BDF_FILE_NAME_SIZE;
	} else {
		file_type = ATH11K_QMI_FILE_TYPE_CALDATA;

		/* cal-<bus>-<id>.bin */
		snprintf(filename, sizeof(filename), "cal-%s-%s.bin",
			 ath11k_bus_str(ab->hif.bus), dev_name(dev));
		fw_entry = ath11k_core_firmware_request(ab, filename);
		if (!IS_ERR(fw_entry))
			goto success;

		fw_entry = ath11k_core_firmware_request(ab, ATH11K_DEFAULT_CAL_FILE);
		if (IS_ERR(fw_entry)) {
			/* Caldata may not be present during first time calibration in
			 * factory hence allow to boot without loading caldata in ftm mode
			 */
			if (ath11k_ftm_mode) {
				ath11k_info(ab,
					    "Booting without cal data file in factory test mode\n");
				return 0;
			}
			ret = PTR_ERR(fw_entry);
			ath11k_warn(ab,
				    "qmi failed to load CAL data file:%s\n",
				    filename);
			goto out;
		}
success:
		fw_size = min_t(u32, ab->hw_params.fw.board_size, fw_entry->size);
		tmp = fw_entry->data;
	}

	ret = ath11k_qmi_load_file_target_mem(ab, tmp, fw_size, file_type);
	if (ret < 0) {
		ath11k_warn(ab, "qmi failed to load caldata\n");
		goto out_qmi_cal;
	}

	ath11k_dbg(ab, ATH11K_DBG_QMI, "caldata type: %u\n", file_type);

out_qmi_cal:
	if (!ab->qmi.target.eeprom_caldata)
		release_firmware(fw_entry);
#endif
out:
	free(data, M_DEVBUF, len);
	if (ret == 0)
		DPRINTF("%s: BDF download sequence completed\n", __func__);

	return ret;
}

int
qwx_qmi_event_load_bdf(struct qwx_softc *sc)
{
	int ret;

	ret = qwx_qmi_request_target_cap(sc);
	if (ret < 0) {
		printf("%s: failed to request qmi target capabilities: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_qmi_request_device_info(sc);
	if (ret < 0) {
		printf("%s: failed to request qmi device info: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	if (sc->hw_params.supports_regdb)
		qwx_qmi_load_bdf_qmi(sc, 1);

	ret = qwx_qmi_load_bdf_qmi(sc, 0);
	if (ret < 0) {
		printf("%s: failed to load board data file: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	return 0;
}

int
qwx_qmi_m3_load(struct qwx_softc *sc)
{
	u_char *data;
	size_t len;
	char path[PATH_MAX];
	int ret;

	ret = snprintf(path, sizeof(path), "%s/%s/%s",
	    ATH11K_FW_DIR, sc->hw_params.fw.dir, ATH11K_M3_FILE);
	if (ret < 0 || ret >= sizeof(path))
		return ENOSPC;

	ret = loadfirmware(path, &data, &len);
	if (ret) {
		printf("%s: could not read %s (error %d)\n",
		    sc->sc_dev.dv_xname, path, ret);
		return ret;
	}

	if (sc->m3_mem == NULL || QWX_DMA_LEN(sc->m3_mem) < len) {
		if (sc->m3_mem)
			qwx_dmamem_free(sc->sc_dmat, sc->m3_mem);
		sc->m3_mem = qwx_dmamem_alloc(sc->sc_dmat, len, 65536);
		if (sc->m3_mem == NULL) {
			printf("%s: failed to allocate %zu bytes of DMA "
			    "memory for M3 firmware\n", sc->sc_dev.dv_xname,
			    len);
			return ENOMEM;
		}
	}

	memcpy(QWX_DMA_KVA(sc->m3_mem), data, len);
	free(data, M_DEVBUF, len);
	return 0;
}

int
qwx_qmi_wlanfw_m3_info_send(struct qwx_softc *sc)
{
	struct qmi_wlanfw_m3_info_req_msg_v01 req;
	int ret = 0;
	uint64_t paddr;
	uint32_t size;

	memset(&req, 0, sizeof(req));

	if (sc->hw_params.m3_fw_support) {
		ret = qwx_qmi_m3_load(sc);
		if (ret) {
			printf("%s: failed to load m3 firmware: %d",
			    sc->sc_dev.dv_xname, ret);
			return ret;
		}

		paddr = QWX_DMA_DVA(sc->m3_mem);
		size = QWX_DMA_LEN(sc->m3_mem);
		req.addr = htole64(paddr);
		req.size = htole32(size);
	} else {
		req.addr = 0;
		req.size = 0;
	}

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_M3_INFO_REQ_V01,
	    QMI_WLANFW_M3_INFO_REQ_MSG_V01_MAX_MSG_LEN,
	    qmi_wlanfw_m3_info_req_msg_v01_ei, &req, sizeof(req));
	if (ret) {
		printf("%s: failed to send m3 information request: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxfwm3",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: m3 information request timeout\n",
			    sc->sc_dev.dv_xname);
			return ret;
		}
	}

	return 0;
}

void
qwx_hal_dump_srng_stats(struct qwx_softc *sc)
{
	DPRINTF("%s not implemented\n", __func__);
}

uint16_t
qwx_hal_srng_get_entrysize(struct qwx_softc *sc, uint32_t ring_type)
{
	struct hal_srng_config *srng_config;

	KASSERT(ring_type < HAL_MAX_RING_TYPES);

	srng_config = &sc->hal.srng_config[ring_type];
	return (srng_config->entry_size << 2);
}

uint32_t
qwx_hal_srng_get_max_entries(struct qwx_softc *sc, uint32_t ring_type)
{
	struct hal_srng_config *srng_config;

	KASSERT(ring_type < HAL_MAX_RING_TYPES);

	srng_config = &sc->hal.srng_config[ring_type];
	return (srng_config->max_size / srng_config->entry_size);
}

uint32_t *
qwx_hal_srng_dst_get_next_entry(struct qwx_softc *sc, struct hal_srng *srng)
{
	uint32_t *desc;
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	desc = srng->ring_base_vaddr + srng->u.dst_ring.tp;

	srng->u.dst_ring.tp += srng->entry_size;

	/* wrap around to start of ring*/
	if (srng->u.dst_ring.tp == srng->ring_size)
		srng->u.dst_ring.tp = 0;
#ifdef notyet
	/* Try to prefetch the next descriptor in the ring */
	if (srng->flags & HAL_SRNG_FLAGS_CACHED)
		ath11k_hal_srng_prefetch_desc(ab, srng);
#endif
	return desc;
}

int
qwx_hal_srng_dst_num_free(struct qwx_softc *sc, struct hal_srng *srng,
    int sync_hw_ptr)
{
	uint32_t tp, hp;
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	tp = srng->u.dst_ring.tp;

	if (sync_hw_ptr) {
		hp = *srng->u.dst_ring.hp_addr;
		srng->u.dst_ring.cached_hp = hp;
	} else {
		hp = srng->u.dst_ring.cached_hp;
	}

	if (hp >= tp)
		return (hp - tp) / srng->entry_size;
	else
		return (srng->ring_size - tp + hp) / srng->entry_size;
}

uint32_t *
qwx_hal_srng_src_get_next_reaped(struct qwx_softc *sc, struct hal_srng *srng)
{
	uint32_t *desc;
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	if (srng->u.src_ring.hp == srng->u.src_ring.reap_hp)
		return NULL;

	desc = srng->ring_base_vaddr + srng->u.src_ring.hp;
	srng->u.src_ring.hp = (srng->u.src_ring.hp + srng->entry_size) %
			      srng->ring_size;

	return desc;
}

void
qwx_get_msi_address(struct qwx_softc *sc, uint32_t *addr_lo,
    uint32_t *addr_hi)
{
	*addr_lo = sc->msi_addr_lo;
	*addr_hi = sc->msi_addr_hi;
}

int
qwx_dp_srng_find_ring_in_mask(int ring_num, const uint8_t *grp_mask)
{
	int ext_group_num;
	uint8_t mask = 1 << ring_num;

	for (ext_group_num = 0; ext_group_num < ATH11K_EXT_IRQ_GRP_NUM_MAX;
	     ext_group_num++) {
		if (mask & grp_mask[ext_group_num])
			return ext_group_num;
	}

	return -1;
}

int
qwx_dp_srng_calculate_msi_group(struct qwx_softc *sc, enum hal_ring_type type,
    int ring_num)
{
	const uint8_t *grp_mask;

	switch (type) {
	case HAL_WBM2SW_RELEASE:
		if (ring_num == DP_RX_RELEASE_RING_NUM) {
			grp_mask = &sc->hw_params.ring_mask->rx_wbm_rel[0];
			ring_num = 0;
		} else {
			grp_mask = &sc->hw_params.ring_mask->tx[0];
		}
		break;
	case HAL_REO_EXCEPTION:
		grp_mask = &sc->hw_params.ring_mask->rx_err[0];
		break;
	case HAL_REO_DST:
		grp_mask = &sc->hw_params.ring_mask->rx[0];
		break;
	case HAL_REO_STATUS:
		grp_mask = &sc->hw_params.ring_mask->reo_status[0];
		break;
	case HAL_RXDMA_MONITOR_STATUS:
	case HAL_RXDMA_MONITOR_DST:
		grp_mask = &sc->hw_params.ring_mask->rx_mon_status[0];
		break;
	case HAL_RXDMA_DST:
		grp_mask = &sc->hw_params.ring_mask->rxdma2host[0];
		break;
	case HAL_RXDMA_BUF:
		grp_mask = &sc->hw_params.ring_mask->host2rxdma[0];
		break;
	case HAL_RXDMA_MONITOR_BUF:
	case HAL_TCL_DATA:
	case HAL_TCL_CMD:
	case HAL_REO_CMD:
	case HAL_SW2WBM_RELEASE:
	case HAL_WBM_IDLE_LINK:
	case HAL_TCL_STATUS:
	case HAL_REO_REINJECT:
	case HAL_CE_SRC:
	case HAL_CE_DST:
	case HAL_CE_DST_STATUS:
	default:
		return -1;
	}

	return qwx_dp_srng_find_ring_in_mask(ring_num, grp_mask);
}

void
qwx_dp_srng_msi_setup(struct qwx_softc *sc, struct hal_srng_params *ring_params,
    enum hal_ring_type type, int ring_num)
{
	int msi_group_number;
	uint32_t msi_data_start = 0;
	uint32_t msi_data_count = 1;
	uint32_t msi_irq_start = 0;
	uint32_t addr_lo;
	uint32_t addr_hi;
	int ret;

	ret = sc->ops.get_user_msi_vector(sc, "DP",
	    &msi_data_count, &msi_data_start, &msi_irq_start);
	if (ret)
		return;

	msi_group_number = qwx_dp_srng_calculate_msi_group(sc, type,
	    ring_num);
	if (msi_group_number < 0) {
		DPRINTF("%s ring not part of an ext_group; ring_type %d,"
		    "ring_num %d\n", __func__, type, ring_num);
		ring_params->msi_addr = 0;
		ring_params->msi_data = 0;
		return;
	}

	qwx_get_msi_address(sc, &addr_lo, &addr_hi);

	ring_params->msi_addr = addr_lo;
	ring_params->msi_addr |= (((uint64_t)addr_hi) << 32);
	ring_params->msi_data = (msi_group_number % msi_data_count) +
	    msi_data_start;
	ring_params->flags |= HAL_SRNG_FLAGS_MSI_INTR;
}

int
qwx_dp_srng_setup(struct qwx_softc *sc, struct dp_srng *ring,
    enum hal_ring_type type, int ring_num, int mac_id, int num_entries)
{
	struct hal_srng_params params = { 0 };
	uint16_t entry_sz = qwx_hal_srng_get_entrysize(sc, type);
	uint32_t max_entries = qwx_hal_srng_get_max_entries(sc, type);
	int ret;
	int cached = 0;

	if (num_entries > max_entries)
		num_entries = max_entries;

	ring->size = (num_entries * entry_sz) + HAL_RING_BASE_ALIGN - 1;

#ifdef notyet
	if (sc->hw_params.alloc_cacheable_memory) {
		/* Allocate the reo dst and tx completion rings from cacheable memory */
		switch (type) {
		case HAL_REO_DST:
		case HAL_WBM2SW_RELEASE:
			cached = true;
			break;
		default:
			cached = false;
		}

		if (cached) {
			ring->vaddr_unaligned = kzalloc(ring->size, GFP_KERNEL);
			ring->paddr_unaligned = virt_to_phys(ring->vaddr_unaligned);
		}
		if (!ring->vaddr_unaligned)
			return -ENOMEM;
	}
#endif
	if (!cached) {
		ring->mem = qwx_dmamem_alloc(sc->sc_dmat, ring->size,
		    PAGE_SIZE);
		if (ring->mem == NULL) {
			printf("%s: could not allocate DP SRNG DMA memory\n",
			    sc->sc_dev.dv_xname);
			return ENOMEM;

		}
	}

	ring->vaddr = QWX_DMA_KVA(ring->mem);
	ring->paddr = QWX_DMA_DVA(ring->mem);

	params.ring_base_vaddr = ring->vaddr;
	params.ring_base_paddr = ring->paddr;
	params.num_entries = num_entries;
	qwx_dp_srng_msi_setup(sc, &params, type, ring_num + mac_id);

	switch (type) {
	case HAL_REO_DST:
		params.intr_batch_cntr_thres_entries =
		    HAL_SRNG_INT_BATCH_THRESHOLD_RX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_RXDMA_BUF:
	case HAL_RXDMA_MONITOR_BUF:
	case HAL_RXDMA_MONITOR_STATUS:
		params.low_threshold = num_entries >> 3;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 0;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_WBM2SW_RELEASE:
		if (ring_num < 3) {
			params.intr_batch_cntr_thres_entries =
			    HAL_SRNG_INT_BATCH_THRESHOLD_TX;
			params.intr_timer_thres_us =
			    HAL_SRNG_INT_TIMER_THRESHOLD_TX;
			break;
		}
		/* follow through when ring_num >= 3 */
		/* FALLTHROUGH */
	case HAL_REO_EXCEPTION:
	case HAL_REO_REINJECT:
	case HAL_REO_CMD:
	case HAL_REO_STATUS:
	case HAL_TCL_DATA:
	case HAL_TCL_CMD:
	case HAL_TCL_STATUS:
	case HAL_WBM_IDLE_LINK:
	case HAL_SW2WBM_RELEASE:
	case HAL_RXDMA_DST:
	case HAL_RXDMA_MONITOR_DST:
	case HAL_RXDMA_MONITOR_DESC:
		params.intr_batch_cntr_thres_entries =
		    HAL_SRNG_INT_BATCH_THRESHOLD_OTHER;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_OTHER;
		break;
	case HAL_RXDMA_DIR_BUF:
		break;
	default:
		printf("%s: Not a valid ring type in dp :%d\n",
		    sc->sc_dev.dv_xname, type);
		return EINVAL;
	}

	if (cached) {
		params.flags |= HAL_SRNG_FLAGS_CACHED;
		ring->cached = 1;
	}

	ret = qwx_hal_srng_setup(sc, type, ring_num, mac_id, &params);
	if (ret < 0) {
		printf("%s: failed to setup srng: %d ring_id %d\n",
		    sc->sc_dev.dv_xname, ret, ring_num);
		return ret;
	}

	ring->ring_id = ret;
	return 0;
}

void
qwx_hal_srng_access_begin(struct qwx_softc *sc, struct hal_srng *srng)
{
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		srng->u.src_ring.cached_tp =
			*(volatile uint32_t *)srng->u.src_ring.tp_addr;
	} else {
		srng->u.dst_ring.cached_hp = *srng->u.dst_ring.hp_addr;
	}
}

void
qwx_hal_srng_access_end(struct qwx_softc *sc, struct hal_srng *srng)
{
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	/* TODO: See if we need a write memory barrier here */
	if (srng->flags & HAL_SRNG_FLAGS_LMAC_RING) {
		/* For LMAC rings, ring pointer updates are done through FW and
		 * hence written to a shared memory location that is read by FW
		 */
		if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
			srng->u.src_ring.last_tp =
			    *(volatile uint32_t *)srng->u.src_ring.tp_addr;
			*srng->u.src_ring.hp_addr = srng->u.src_ring.hp;
		} else {
			srng->u.dst_ring.last_hp = *srng->u.dst_ring.hp_addr;
			*srng->u.dst_ring.tp_addr = srng->u.dst_ring.tp;
		}
	} else {
		if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
			srng->u.src_ring.last_tp =
			    *(volatile uint32_t *)srng->u.src_ring.tp_addr;
			sc->ops.write32(sc,
			    (unsigned long)srng->u.src_ring.hp_addr -
			    (unsigned long)sc->mem, srng->u.src_ring.hp);
		} else {
			srng->u.dst_ring.last_hp = *srng->u.dst_ring.hp_addr;
			sc->ops.write32(sc,
			    (unsigned long)srng->u.dst_ring.tp_addr -
			    (unsigned long)sc->mem, srng->u.dst_ring.tp);
		}
	}
#ifdef notyet
	srng->timestamp = jiffies;
#endif
}

int
qwx_wbm_idle_ring_setup(struct qwx_softc *sc, uint32_t *n_link_desc)
{
	struct qwx_dp *dp = &sc->dp;
	uint32_t n_mpdu_link_desc, n_mpdu_queue_desc;
	uint32_t n_tx_msdu_link_desc, n_rx_msdu_link_desc;
	int ret = 0;

	n_mpdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_MPDUS_PER_TID_MAX) /
			   HAL_NUM_MPDUS_PER_LINK_DESC;

	n_mpdu_queue_desc = n_mpdu_link_desc /
			    HAL_NUM_MPDU_LINKS_PER_QUEUE_DESC;

	n_tx_msdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_FLOWS_PER_TID *
			       DP_AVG_MSDUS_PER_FLOW) /
			      HAL_NUM_TX_MSDUS_PER_LINK_DESC;

	n_rx_msdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_MPDUS_PER_TID_MAX *
			       DP_AVG_MSDUS_PER_MPDU) /
			      HAL_NUM_RX_MSDUS_PER_LINK_DESC;

	*n_link_desc = n_mpdu_link_desc + n_mpdu_queue_desc +
		      n_tx_msdu_link_desc + n_rx_msdu_link_desc;

	if (*n_link_desc & (*n_link_desc - 1))
		*n_link_desc = 1 << fls(*n_link_desc);

	ret = qwx_dp_srng_setup(sc, &dp->wbm_idle_ring,
	    HAL_WBM_IDLE_LINK, 0, 0, *n_link_desc);
	if (ret) {
		printf("%s: failed to setup wbm_idle_ring: %d\n",
		    sc->sc_dev.dv_xname, ret);
	}

	return ret;
}

void
qwx_dp_link_desc_bank_free(struct qwx_softc *sc,
    struct dp_link_desc_bank *link_desc_banks)
{
	int i;

	for (i = 0; i < DP_LINK_DESC_BANKS_MAX; i++) {
		if (link_desc_banks[i].mem) {
			qwx_dmamem_free(sc->sc_dmat, link_desc_banks[i].mem);
			link_desc_banks[i].mem = NULL;
		}
	}
}

int
qwx_dp_link_desc_bank_alloc(struct qwx_softc *sc,
    struct dp_link_desc_bank *desc_bank, int n_link_desc_bank,
    int last_bank_sz)
{
	struct qwx_dp *dp = &sc->dp;
	int i;
	int ret = 0;
	int desc_sz = DP_LINK_DESC_ALLOC_SIZE_THRESH;

	for (i = 0; i < n_link_desc_bank; i++) {
		if (i == (n_link_desc_bank - 1) && last_bank_sz)
			desc_sz = last_bank_sz;

		desc_bank[i].mem = qwx_dmamem_alloc(sc->sc_dmat, desc_sz,
		    PAGE_SIZE);
		if (!desc_bank[i].mem) {
			ret = ENOMEM;
			goto err;
		}

		desc_bank[i].vaddr = QWX_DMA_KVA(desc_bank[i].mem);
		desc_bank[i].paddr = QWX_DMA_DVA(desc_bank[i].mem);
		desc_bank[i].size = desc_sz;
	}

	return 0;

err:
	qwx_dp_link_desc_bank_free(sc, dp->link_desc_banks);

	return ret;
}

void
qwx_hal_setup_link_idle_list(struct qwx_softc *sc,
    struct hal_wbm_idle_scatter_list *sbuf,
    uint32_t nsbufs, uint32_t tot_link_desc, uint32_t end_offset)
{
	struct ath11k_buffer_addr *link_addr;
	int i;
	uint32_t reg_scatter_buf_sz = HAL_WBM_IDLE_SCATTER_BUF_SIZE / 64;

	link_addr = (void *)sbuf[0].vaddr + HAL_WBM_IDLE_SCATTER_BUF_SIZE;

	for (i = 1; i < nsbufs; i++) {
		link_addr->info0 = sbuf[i].paddr & HAL_ADDR_LSB_REG_MASK;
		link_addr->info1 = FIELD_PREP(
		    HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32,
		    (uint64_t)sbuf[i].paddr >> HAL_ADDR_MSB_REG_SHIFT) |
		    FIELD_PREP(HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_MATCH_TAG,
		    BASE_ADDR_MATCH_TAG_VAL);

		link_addr = (void *)sbuf[i].vaddr +
		    HAL_WBM_IDLE_SCATTER_BUF_SIZE;
	}

	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_R0_IDLE_LIST_CONTROL_ADDR,
	    FIELD_PREP(HAL_WBM_SCATTER_BUFFER_SIZE, reg_scatter_buf_sz) |
	    FIELD_PREP(HAL_WBM_LINK_DESC_IDLE_LIST_MODE, 0x1));
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_R0_IDLE_LIST_SIZE_ADDR,
	    FIELD_PREP(HAL_WBM_SCATTER_RING_SIZE_OF_IDLE_LINK_DESC_LIST,
	    reg_scatter_buf_sz * nsbufs));
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SCATTERED_RING_BASE_LSB,
	    FIELD_PREP(BUFFER_ADDR_INFO0_ADDR,
	    sbuf[0].paddr & HAL_ADDR_LSB_REG_MASK));
	sc->ops.write32(sc, HAL_SEQ_WCSS_UMAC_WBM_REG +
	    HAL_WBM_SCATTERED_RING_BASE_MSB,
	    FIELD_PREP(HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32,
	    (uint64_t)sbuf[0].paddr >> HAL_ADDR_MSB_REG_SHIFT) |
	    FIELD_PREP(HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_MATCH_TAG,
	    BASE_ADDR_MATCH_TAG_VAL));

	/* Setup head and tail pointers for the idle list */
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG +
	    HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX0,
	    FIELD_PREP(BUFFER_ADDR_INFO0_ADDR, sbuf[nsbufs - 1].paddr));
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX1,
	    FIELD_PREP(HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32,
	    ((uint64_t)sbuf[nsbufs - 1].paddr >> HAL_ADDR_MSB_REG_SHIFT)) |
	    FIELD_PREP(HAL_WBM_SCATTERED_DESC_HEAD_P_OFFSET_IX1,
	    (end_offset >> 2)));
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG +
	    HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX0,
	    FIELD_PREP(BUFFER_ADDR_INFO0_ADDR, sbuf[0].paddr));

	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX0,
	    FIELD_PREP(BUFFER_ADDR_INFO0_ADDR, sbuf[0].paddr));
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX1,
	    FIELD_PREP(HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32,
	    ((uint64_t)sbuf[0].paddr >> HAL_ADDR_MSB_REG_SHIFT)) |
	    FIELD_PREP(HAL_WBM_SCATTERED_DESC_TAIL_P_OFFSET_IX1, 0));
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SCATTERED_DESC_PTR_HP_ADDR,
	    2 * tot_link_desc);

	/* Enable the SRNG */
	sc->ops.write32(sc,
	    HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_MISC_ADDR(sc),
	    0x40);
}

void
qwx_hal_set_link_desc_addr(struct hal_wbm_link_desc *desc, uint32_t cookie,
    bus_addr_t paddr)
{
	desc->buf_addr_info.info0 = FIELD_PREP(BUFFER_ADDR_INFO0_ADDR,
	    (paddr & HAL_ADDR_LSB_REG_MASK));
	desc->buf_addr_info.info1 = FIELD_PREP(BUFFER_ADDR_INFO1_ADDR,
	    ((uint64_t)paddr >> HAL_ADDR_MSB_REG_SHIFT)) |
	    FIELD_PREP(BUFFER_ADDR_INFO1_RET_BUF_MGR, 1) |
	    FIELD_PREP(BUFFER_ADDR_INFO1_SW_COOKIE, cookie);
}

void
qwx_dp_scatter_idle_link_desc_cleanup(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	struct hal_wbm_idle_scatter_list *slist = dp->scatter_list;
	int i;

	for (i = 0; i < DP_IDLE_SCATTER_BUFS_MAX; i++) {
		if (slist[i].mem == NULL)
			continue;

		qwx_dmamem_free(sc->sc_dmat, slist[i].mem);
		slist[i].mem = NULL;
		slist[i].vaddr = NULL;
		slist[i].paddr = 0L;
	}
}

int
qwx_dp_scatter_idle_link_desc_setup(struct qwx_softc *sc, int size,
    uint32_t n_link_desc_bank, uint32_t n_link_desc, uint32_t last_bank_sz)
{
	struct qwx_dp *dp = &sc->dp;
	struct dp_link_desc_bank *link_desc_banks = dp->link_desc_banks;
	struct hal_wbm_idle_scatter_list *slist = dp->scatter_list;
	uint32_t n_entries_per_buf;
	int num_scatter_buf, scatter_idx;
	struct hal_wbm_link_desc *scatter_buf;
	int n_entries;
	bus_addr_t paddr;
	int rem_entries;
	int i;
	int ret = 0;
	uint32_t end_offset;

	n_entries_per_buf = HAL_WBM_IDLE_SCATTER_BUF_SIZE /
	    qwx_hal_srng_get_entrysize(sc, HAL_WBM_IDLE_LINK);
	num_scatter_buf = howmany(size, HAL_WBM_IDLE_SCATTER_BUF_SIZE);

	if (num_scatter_buf > DP_IDLE_SCATTER_BUFS_MAX)
		return EINVAL;

	for (i = 0; i < num_scatter_buf; i++) {
		slist[i].mem = qwx_dmamem_alloc(sc->sc_dmat,
		    HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX, PAGE_SIZE);
		if (slist[i].mem == NULL) {
			ret = ENOMEM;
			goto err;
		}

		slist[i].vaddr = QWX_DMA_KVA(slist[i].mem);
		slist[i].paddr = QWX_DMA_DVA(slist[i].mem);
	}

	scatter_idx = 0;
	scatter_buf = slist[scatter_idx].vaddr;
	rem_entries = n_entries_per_buf;

	for (i = 0; i < n_link_desc_bank; i++) {
		n_entries = DP_LINK_DESC_ALLOC_SIZE_THRESH / HAL_LINK_DESC_SIZE;
		paddr = link_desc_banks[i].paddr;
		while (n_entries) {
			qwx_hal_set_link_desc_addr(scatter_buf, i, paddr);
			n_entries--;
			paddr += HAL_LINK_DESC_SIZE;
			if (rem_entries) {
				rem_entries--;
				scatter_buf++;
				continue;
			}

			rem_entries = n_entries_per_buf;
			scatter_idx++;
			scatter_buf = slist[scatter_idx].vaddr;
		}
	}

	end_offset = (scatter_buf - slist[scatter_idx].vaddr) *
	    sizeof(struct hal_wbm_link_desc);
	qwx_hal_setup_link_idle_list(sc, slist, num_scatter_buf,
	    n_link_desc, end_offset);

	return 0;

err:
	qwx_dp_scatter_idle_link_desc_cleanup(sc);

	return ret;
}

uint32_t *
qwx_hal_srng_src_get_next_entry(struct qwx_softc *sc, struct hal_srng *srng)
{
	uint32_t *desc;
	uint32_t next_hp;
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif

	/* TODO: Using % is expensive, but we have to do this since size of some
	 * SRNG rings is not power of 2 (due to descriptor sizes). Need to see
	 * if separate function is defined for rings having power of 2 ring size
	 * (TCL2SW, REO2SW, SW2RXDMA and CE rings) so that we can avoid the
	 * overhead of % by using mask (with &).
	 */
	next_hp = (srng->u.src_ring.hp + srng->entry_size) % srng->ring_size;

	if (next_hp == srng->u.src_ring.cached_tp)
		return NULL;

	desc = srng->ring_base_vaddr + srng->u.src_ring.hp;
	srng->u.src_ring.hp = next_hp;

	/* TODO: Reap functionality is not used by all rings. If particular
	 * ring does not use reap functionality, we need not update reap_hp
	 * with next_hp pointer. Need to make sure a separate function is used
	 * before doing any optimization by removing below code updating
	 * reap_hp.
	 */
	srng->u.src_ring.reap_hp = next_hp;

	return desc;
}

uint32_t *
qwx_hal_srng_src_reap_next(struct qwx_softc *sc, struct hal_srng *srng)
{
	uint32_t *desc;
	uint32_t next_reap_hp;
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	next_reap_hp = (srng->u.src_ring.reap_hp + srng->entry_size) %
	    srng->ring_size;

	if (next_reap_hp == srng->u.src_ring.cached_tp)
		return NULL;

	desc = srng->ring_base_vaddr + next_reap_hp;
	srng->u.src_ring.reap_hp = next_reap_hp;

	return desc;
}

int
qwx_dp_link_desc_setup(struct qwx_softc *sc,
    struct dp_link_desc_bank *link_desc_banks, uint32_t ring_type,
    struct hal_srng *srng, uint32_t n_link_desc)
{
	uint32_t tot_mem_sz;
	uint32_t n_link_desc_bank, last_bank_sz;
	uint32_t entry_sz, n_entries;
	uint64_t paddr;
	uint32_t *desc;
	int i, ret;

	tot_mem_sz = n_link_desc * HAL_LINK_DESC_SIZE;
	tot_mem_sz += HAL_LINK_DESC_ALIGN;

	if (tot_mem_sz <= DP_LINK_DESC_ALLOC_SIZE_THRESH) {
		n_link_desc_bank = 1;
		last_bank_sz = tot_mem_sz;
	} else {
		n_link_desc_bank = tot_mem_sz /
		    (DP_LINK_DESC_ALLOC_SIZE_THRESH - HAL_LINK_DESC_ALIGN);
		last_bank_sz = tot_mem_sz % (DP_LINK_DESC_ALLOC_SIZE_THRESH -
		    HAL_LINK_DESC_ALIGN);

		if (last_bank_sz)
			n_link_desc_bank += 1;
	}

	if (n_link_desc_bank > DP_LINK_DESC_BANKS_MAX)
		return EINVAL;

	ret = qwx_dp_link_desc_bank_alloc(sc, link_desc_banks,
	    n_link_desc_bank, last_bank_sz);
	if (ret)
		return ret;

	/* Setup link desc idle list for HW internal usage */
	entry_sz = qwx_hal_srng_get_entrysize(sc, ring_type);
	tot_mem_sz = entry_sz * n_link_desc;

	/* Setup scatter desc list when the total memory requirement is more */
	if (tot_mem_sz > DP_LINK_DESC_ALLOC_SIZE_THRESH &&
	    ring_type != HAL_RXDMA_MONITOR_DESC) {
		ret = qwx_dp_scatter_idle_link_desc_setup(sc, tot_mem_sz,
		    n_link_desc_bank, n_link_desc, last_bank_sz);
		if (ret) {
			printf("%s: failed to setup scatting idle list "
			    "descriptor :%d\n",
			    sc->sc_dev.dv_xname, ret);
			goto fail_desc_bank_free;
		}

		return 0;
	}
#if 0
	spin_lock_bh(&srng->lock);
#endif
	qwx_hal_srng_access_begin(sc, srng);

	for (i = 0; i < n_link_desc_bank; i++) {
		n_entries = (link_desc_banks[i].size) / HAL_LINK_DESC_SIZE;
		paddr = link_desc_banks[i].paddr;
		while (n_entries &&
		    (desc = qwx_hal_srng_src_get_next_entry(sc, srng))) {
			qwx_hal_set_link_desc_addr(
			    (struct hal_wbm_link_desc *) desc, i, paddr);
			n_entries--;
			paddr += HAL_LINK_DESC_SIZE;
		}
	}

	qwx_hal_srng_access_end(sc, srng);
#if 0
	spin_unlock_bh(&srng->lock);
#endif

	return 0;

fail_desc_bank_free:
	qwx_dp_link_desc_bank_free(sc, link_desc_banks);

	return ret;
}

void
qwx_dp_srng_cleanup(struct qwx_softc *sc, struct dp_srng *ring)
{
	if (ring->mem == NULL)
		return;

#if 0
	if (ring->cached)
		kfree(ring->vaddr_unaligned);
	else
#endif
		qwx_dmamem_free(sc->sc_dmat, ring->mem);

	ring->mem = NULL;
	ring->vaddr = NULL;
	ring->paddr = 0;
}

void
qwx_dp_shadow_stop_timer(struct qwx_softc *sc,
    struct qwx_hp_update_timer *update_timer)
{
	if (!sc->hw_params.supports_shadow_regs)
		return;

	timeout_del(&update_timer->timer);
}

void
qwx_dp_shadow_start_timer(struct qwx_softc *sc, struct hal_srng *srng,
    struct qwx_hp_update_timer *update_timer)
{
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	if (!sc->hw_params.supports_shadow_regs)
		return;

	update_timer->started = 1;
	update_timer->timer_tx_num = update_timer->tx_num;

	timeout_add_msec(&update_timer->timer, update_timer->interval);
}

void
qwx_dp_shadow_timer_handler(void *arg)
{
	struct qwx_hp_update_timer *update_timer = arg;
	struct qwx_softc *sc = update_timer->sc;
	struct hal_srng	*srng = &sc->hal.srng_list[update_timer->ring_id];
	int s;

#ifdef notyet
	spin_lock_bh(&srng->lock);
#endif
	s = splnet();

	/* 
	 * Update HP if there were no TX operations during the timeout interval,
	 * and stop the timer. Timer will be restarted if more TX happens.
	 */
	if (update_timer->timer_tx_num != update_timer->tx_num) {
		update_timer->timer_tx_num = update_timer->tx_num;
		timeout_add_msec(&update_timer->timer, update_timer->interval);
	} else {
		update_timer->started = 0;
		qwx_hal_srng_shadow_update_hp_tp(sc, srng);
	}
#ifdef notyet
	spin_unlock_bh(&srng->lock);
#endif
	splx(s);
}

void
qwx_dp_stop_shadow_timers(struct qwx_softc *sc)
{
	int i;

	for (i = 0; i < sc->hw_params.max_tx_ring; i++)
		qwx_dp_shadow_stop_timer(sc, &sc->dp.tx_ring_timer[i]);

	qwx_dp_shadow_stop_timer(sc, &sc->dp.reo_cmd_timer);
}

void
qwx_dp_srng_common_cleanup(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	int i;

	qwx_dp_stop_shadow_timers(sc);
	qwx_dp_srng_cleanup(sc, &dp->wbm_desc_rel_ring);
	qwx_dp_srng_cleanup(sc, &dp->tcl_cmd_ring);
	qwx_dp_srng_cleanup(sc, &dp->tcl_status_ring);
	for (i = 0; i < sc->hw_params.max_tx_ring; i++) {
		qwx_dp_srng_cleanup(sc, &dp->tx_ring[i].tcl_data_ring);
		qwx_dp_srng_cleanup(sc, &dp->tx_ring[i].tcl_comp_ring);
	}
	qwx_dp_srng_cleanup(sc, &dp->reo_reinject_ring);
	qwx_dp_srng_cleanup(sc, &dp->rx_rel_ring);
	qwx_dp_srng_cleanup(sc, &dp->reo_except_ring);
	qwx_dp_srng_cleanup(sc, &dp->reo_cmd_ring);
	qwx_dp_srng_cleanup(sc, &dp->reo_status_ring);
}

void
qwx_hal_srng_get_params(struct qwx_softc *sc, struct hal_srng *srng,
    struct hal_srng_params *params)
{
	params->ring_base_paddr = srng->ring_base_paddr;
	params->ring_base_vaddr = srng->ring_base_vaddr;
	params->num_entries = srng->num_entries;
	params->intr_timer_thres_us = srng->intr_timer_thres_us;
	params->intr_batch_cntr_thres_entries =
		srng->intr_batch_cntr_thres_entries;
	params->low_threshold = srng->u.src_ring.low_threshold;
	params->msi_addr = srng->msi_addr;
	params->msi_data = srng->msi_data;
	params->flags = srng->flags;
}

void
qwx_hal_tx_init_data_ring(struct qwx_softc *sc, struct hal_srng *srng)
{
	struct hal_srng_params params;
	struct hal_tlv_hdr *tlv;
	int i, entry_size;
	uint8_t *desc;

	memset(&params, 0, sizeof(params));

	entry_size = qwx_hal_srng_get_entrysize(sc, HAL_TCL_DATA);
	qwx_hal_srng_get_params(sc, srng, &params);
	desc = (uint8_t *)params.ring_base_vaddr;

	for (i = 0; i < params.num_entries; i++) {
		tlv = (struct hal_tlv_hdr *)desc;
		tlv->tl = FIELD_PREP(HAL_TLV_HDR_TAG, HAL_TCL_DATA_CMD) |
		    FIELD_PREP(HAL_TLV_HDR_LEN,
		    sizeof(struct hal_tcl_data_cmd));
		desc += entry_size;
	}
}

#define DSCP_TID_MAP_TBL_ENTRY_SIZE 64

/* dscp_tid_map - Default DSCP-TID mapping
 *
 * DSCP        TID
 * 000000      0
 * 001000      1
 * 010000      2
 * 011000      3
 * 100000      4
 * 101000      5
 * 110000      6
 * 111000      7
 */
static const uint8_t dscp_tid_map[DSCP_TID_MAP_TBL_ENTRY_SIZE] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7,
};

void
qwx_hal_tx_set_dscp_tid_map(struct qwx_softc *sc, int id)
{
	uint32_t ctrl_reg_val;
	uint32_t addr;
	uint8_t hw_map_val[HAL_DSCP_TID_TBL_SIZE];
	int i;
	uint32_t value;
	int cnt = 0;

	ctrl_reg_val = sc->ops.read32(sc, HAL_SEQ_WCSS_UMAC_TCL_REG +
	    HAL_TCL1_RING_CMN_CTRL_REG);

	/* Enable read/write access */
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	sc->ops.write32(sc, HAL_SEQ_WCSS_UMAC_TCL_REG +
	    HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	/* Configure each DSCP-TID mapping in three bits there by configure
	 * three bytes in an iteration.
	 */
	for (i = 0; i < DSCP_TID_MAP_TBL_ENTRY_SIZE; i += 8) {
		value = FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP0,
				   dscp_tid_map[i]) |
			FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP1,
				   dscp_tid_map[i + 1]) |
			FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP2,
				   dscp_tid_map[i + 2]) |
			FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP3,
				   dscp_tid_map[i + 3]) |
			FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP4,
				   dscp_tid_map[i + 4]) |
			FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP5,
				   dscp_tid_map[i + 5]) |
			FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP6,
				   dscp_tid_map[i + 6]) |
			FIELD_PREP(HAL_TCL1_RING_FIELD_DSCP_TID_MAP7,
				   dscp_tid_map[i + 7]);
		memcpy(&hw_map_val[cnt], (uint8_t *)&value, 3);
		cnt += 3;
	}

	for (i = 0; i < HAL_DSCP_TID_TBL_SIZE; i += 4) {
		sc->ops.write32(sc, addr, *(uint32_t *)&hw_map_val[i]);
		addr += 4;
	}

	/* Disable read/write access */
	ctrl_reg_val = sc->ops.read32(sc, HAL_SEQ_WCSS_UMAC_TCL_REG +
	    HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	sc->ops.write32(sc, HAL_SEQ_WCSS_UMAC_TCL_REG +
	    HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);
}

void
qwx_dp_shadow_init_timer(struct qwx_softc *sc,
    struct qwx_hp_update_timer *update_timer,
    uint32_t interval, uint32_t ring_id)
{
	if (!sc->hw_params.supports_shadow_regs)
		return;

	update_timer->tx_num = 0;
	update_timer->timer_tx_num = 0;
	update_timer->sc = sc;
	update_timer->ring_id = ring_id;
	update_timer->interval = interval;
	update_timer->init = 1;
	timeout_set(&update_timer->timer, qwx_dp_shadow_timer_handler,
	    update_timer);
}

void
qwx_hal_reo_init_cmd_ring(struct qwx_softc *sc, struct hal_srng *srng)
{
	struct hal_srng_params params;
	struct hal_tlv_hdr *tlv;
	struct hal_reo_get_queue_stats *desc;
	int i, cmd_num = 1;
	int entry_size;
	uint8_t *entry;

	memset(&params, 0, sizeof(params));

	entry_size = qwx_hal_srng_get_entrysize(sc, HAL_REO_CMD);
	qwx_hal_srng_get_params(sc, srng, &params);
	entry = (uint8_t *)params.ring_base_vaddr;

	for (i = 0; i < params.num_entries; i++) {
		tlv = (struct hal_tlv_hdr *)entry;
		desc = (struct hal_reo_get_queue_stats *)tlv->value;
		desc->cmd.info0 = FIELD_PREP(HAL_REO_CMD_HDR_INFO0_CMD_NUMBER,
		    cmd_num++);
		entry += entry_size;
	}
}

int
qwx_dp_srng_common_setup(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	struct hal_srng *srng;
	int i, ret;
	uint8_t tcl_num, wbm_num;

	ret = qwx_dp_srng_setup(sc, &dp->wbm_desc_rel_ring, HAL_SW2WBM_RELEASE,
	    0, 0, DP_WBM_RELEASE_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up wbm2sw_release ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	ret = qwx_dp_srng_setup(sc, &dp->tcl_cmd_ring, HAL_TCL_CMD,
	    0, 0, DP_TCL_CMD_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up tcl_cmd ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	ret = qwx_dp_srng_setup(sc, &dp->tcl_status_ring, HAL_TCL_STATUS,
	    0, 0, DP_TCL_STATUS_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up tcl_status ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	for (i = 0; i < sc->hw_params.max_tx_ring; i++) {
		const struct ath11k_hw_hal_params *hal_params;

		hal_params = sc->hw_params.hal_params;
		tcl_num = hal_params->tcl2wbm_rbm_map[i].tcl_ring_num;
		wbm_num = hal_params->tcl2wbm_rbm_map[i].wbm_ring_num;

		ret = qwx_dp_srng_setup(sc, &dp->tx_ring[i].tcl_data_ring,
		    HAL_TCL_DATA, tcl_num, 0, sc->hw_params.tx_ring_size);
		if (ret) {
			printf("%s: failed to set up tcl_data ring (%d) :%d\n",
			    sc->sc_dev.dv_xname, i, ret);
			goto err;
		}

		ret = qwx_dp_srng_setup(sc, &dp->tx_ring[i].tcl_comp_ring,
		    HAL_WBM2SW_RELEASE, wbm_num, 0, DP_TX_COMP_RING_SIZE);
		if (ret) {
			printf("%s: failed to set up tcl_comp ring (%d) :%d\n",
			    sc->sc_dev.dv_xname, i, ret);
			goto err;
		}

		srng = &sc->hal.srng_list[dp->tx_ring[i].tcl_data_ring.ring_id];
		qwx_hal_tx_init_data_ring(sc, srng);

		qwx_dp_shadow_init_timer(sc, &dp->tx_ring_timer[i],
		    ATH11K_SHADOW_DP_TIMER_INTERVAL,
		    dp->tx_ring[i].tcl_data_ring.ring_id);
	}

	ret = qwx_dp_srng_setup(sc, &dp->reo_reinject_ring, HAL_REO_REINJECT,
	    0, 0, DP_REO_REINJECT_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up reo_reinject ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	ret = qwx_dp_srng_setup(sc, &dp->rx_rel_ring, HAL_WBM2SW_RELEASE,
	    DP_RX_RELEASE_RING_NUM, 0, DP_RX_RELEASE_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up rx_rel ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	ret = qwx_dp_srng_setup(sc, &dp->reo_except_ring, HAL_REO_EXCEPTION,
	    0, 0, DP_REO_EXCEPTION_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up reo_exception ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	ret = qwx_dp_srng_setup(sc, &dp->reo_cmd_ring, HAL_REO_CMD, 0, 0,
	    DP_REO_CMD_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up reo_cmd ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	srng = &sc->hal.srng_list[dp->reo_cmd_ring.ring_id];
	qwx_hal_reo_init_cmd_ring(sc, srng);

	qwx_dp_shadow_init_timer(sc, &dp->reo_cmd_timer,
	     ATH11K_SHADOW_CTRL_TIMER_INTERVAL, dp->reo_cmd_ring.ring_id);

	ret = qwx_dp_srng_setup(sc, &dp->reo_status_ring, HAL_REO_STATUS,
	    0, 0, DP_REO_STATUS_RING_SIZE);
	if (ret) {
		printf("%s: failed to set up reo_status ring :%d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}
#ifdef notyet
	/* When hash based routing of rx packet is enabled, 32 entries to map
	 * the hash values to the ring will be configured.
	 */
	sc->hw_params.hw_ops->reo_setup(sc);
#endif
	return 0;

err:
	qwx_dp_srng_common_cleanup(sc);

	return ret;
}

void
qwx_dp_link_desc_cleanup(struct qwx_softc *sc,
    struct dp_link_desc_bank *desc_bank, uint32_t ring_type,
    struct dp_srng *ring)
{
	qwx_dp_link_desc_bank_free(sc, desc_bank);

	if (ring_type != HAL_RXDMA_MONITOR_DESC) {
		qwx_dp_srng_cleanup(sc, ring);
		qwx_dp_scatter_idle_link_desc_cleanup(sc);
	}
}


int
qwx_dp_alloc(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	struct hal_srng *srng = NULL;
	size_t size = 0;
	uint32_t n_link_desc = 0;
	int ret;
	int i;

	dp->sc = sc;

	TAILQ_INIT(&dp->reo_cmd_list);
	TAILQ_INIT(&dp->reo_cmd_cache_flush_list);
#if 0
	INIT_LIST_HEAD(&dp->dp_full_mon_mpdu_list);
	spin_lock_init(&dp->reo_cmd_lock);
#endif

	dp->reo_cmd_cache_flush_count = 0;

	ret = qwx_wbm_idle_ring_setup(sc, &n_link_desc);
	if (ret) {
		printf("%s: failed to setup wbm_idle_ring: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	srng = &sc->hal.srng_list[dp->wbm_idle_ring.ring_id];

	ret = qwx_dp_link_desc_setup(sc, dp->link_desc_banks,
	    HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret) {
		printf("%s: failed to setup link desc: %d\n",
		   sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_dp_srng_common_setup(sc);
	if (ret)
		goto fail_link_desc_cleanup;

	size = sizeof(struct hal_wbm_release_ring) * DP_TX_COMP_RING_SIZE;

	for (i = 0; i < sc->hw_params.max_tx_ring; i++) {
#if 0
		idr_init(&dp->tx_ring[i].txbuf_idr);
		spin_lock_init(&dp->tx_ring[i].tx_idr_lock);
#endif
		dp->tx_ring[i].tcl_data_ring_id = i;

		dp->tx_ring[i].tx_status_head = 0;
		dp->tx_ring[i].tx_status_tail = DP_TX_COMP_RING_SIZE - 1;
		dp->tx_ring[i].tx_status = malloc(size, M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (!dp->tx_ring[i].tx_status) {
			ret = ENOMEM;
			goto fail_cmn_srng_cleanup;
		}
	}

	for (i = 0; i < HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX; i++)
		qwx_hal_tx_set_dscp_tid_map(sc, i);

	/* Init any SOC level resource for DP */

	return 0;
fail_cmn_srng_cleanup:
	qwx_dp_srng_common_cleanup(sc);
fail_link_desc_cleanup:
	qwx_dp_link_desc_cleanup(sc, dp->link_desc_banks, HAL_WBM_IDLE_LINK,
	    &dp->wbm_idle_ring);

	return ret;
}

void
qwx_dp_free(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	int i;

	qwx_dp_link_desc_cleanup(sc, dp->link_desc_banks,
	    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);

	qwx_dp_srng_common_cleanup(sc);
#ifdef notyet
	ath11k_dp_reo_cmd_list_cleanup(ab);
#endif
	for (i = 0; i < sc->hw_params.max_tx_ring; i++) {
#if 0
		spin_lock_bh(&dp->tx_ring[i].tx_idr_lock);
		idr_for_each(&dp->tx_ring[i].txbuf_idr,
			     ath11k_dp_tx_pending_cleanup, ab);
		idr_destroy(&dp->tx_ring[i].txbuf_idr);
		spin_unlock_bh(&dp->tx_ring[i].tx_idr_lock);
#endif
		free(dp->tx_ring[i].tx_status, M_DEVBUF,
		    sizeof(struct hal_wbm_release_ring) * DP_TX_COMP_RING_SIZE);
		dp->tx_ring[i].tx_status = NULL;
	}

	/* Deinit any SOC level resource */
}

void
qwx_qmi_process_coldboot_calibration(struct qwx_softc *sc)
{
	printf("%s not implemented\n", __func__);
}

int
qwx_qmi_wlanfw_wlan_ini_send(struct qwx_softc *sc, int enable)
{
	int ret;
	struct qmi_wlanfw_wlan_ini_req_msg_v01 req = {};

	req.enablefwlog_valid = 1;
	req.enablefwlog = enable ? 1 : 0;

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_WLAN_INI_REQ_V01,
	    QMI_WLANFW_WLAN_INI_REQ_MSG_V01_MAX_LEN,
	    qmi_wlanfw_wlan_ini_req_msg_v01_ei, &req, sizeof(req));
	if (ret) {
		printf("%s: failed to send wlan ini request, err = %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxini",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: wlan ini request timeout\n",
			    sc->sc_dev.dv_xname);
			return ret;
		}
	}

	return 0;
}

int
qwx_qmi_wlanfw_wlan_cfg_send(struct qwx_softc *sc)
{
	struct qmi_wlanfw_wlan_cfg_req_msg_v01 *req;
	const struct ce_pipe_config *ce_cfg;
	const struct service_to_pipe *svc_cfg;
	int ret = 0, pipe_num;

	ce_cfg	= sc->hw_params.target_ce_config;
	svc_cfg	= sc->hw_params.svc_to_ce_map;

	req = malloc(sizeof(*req), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!req)
		return ENOMEM;

	req->host_version_valid = 1;
	strlcpy(req->host_version, ATH11K_HOST_VERSION_STRING,
	    sizeof(req->host_version));

	req->tgt_cfg_valid = 1;
	/* This is number of CE configs */
	req->tgt_cfg_len = sc->hw_params.target_ce_count;
	for (pipe_num = 0; pipe_num < req->tgt_cfg_len ; pipe_num++) {
		req->tgt_cfg[pipe_num].pipe_num = ce_cfg[pipe_num].pipenum;
		req->tgt_cfg[pipe_num].pipe_dir = ce_cfg[pipe_num].pipedir;
		req->tgt_cfg[pipe_num].nentries = ce_cfg[pipe_num].nentries;
		req->tgt_cfg[pipe_num].nbytes_max = ce_cfg[pipe_num].nbytes_max;
		req->tgt_cfg[pipe_num].flags = ce_cfg[pipe_num].flags;
	}

	req->svc_cfg_valid = 1;
	/* This is number of Service/CE configs */
	req->svc_cfg_len = sc->hw_params.svc_to_ce_map_len;
	for (pipe_num = 0; pipe_num < req->svc_cfg_len; pipe_num++) {
		req->svc_cfg[pipe_num].service_id = svc_cfg[pipe_num].service_id;
		req->svc_cfg[pipe_num].pipe_dir = svc_cfg[pipe_num].pipedir;
		req->svc_cfg[pipe_num].pipe_num = svc_cfg[pipe_num].pipenum;
	}
	req->shadow_reg_valid = 0;

	/* set shadow v2 configuration */
	if (sc->hw_params.supports_shadow_regs) {
		req->shadow_reg_v2_valid = 1;
		req->shadow_reg_v2_len = MIN(sc->qmi_ce_cfg.shadow_reg_v2_len,
		    QMI_WLANFW_MAX_NUM_SHADOW_REG_V2_V01);
		memcpy(&req->shadow_reg_v2, sc->qmi_ce_cfg.shadow_reg_v2,
		       sizeof(uint32_t) * req->shadow_reg_v2_len);
	} else {
		req->shadow_reg_v2_valid = 0;
	}

	DNPRINTF(QWX_D_QMI, "%s: wlan cfg req\n", __func__);

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_WLAN_CFG_REQ_V01,
	    QMI_WLANFW_WLAN_CFG_REQ_MSG_V01_MAX_LEN,
	    qmi_wlanfw_wlan_cfg_req_msg_v01_ei, req, sizeof(*req));
	if (ret) {
		printf("%s: failed to send wlan config request: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto out;
	}

	sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxwlancfg",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: wlan config request failed\n",
			    sc->sc_dev.dv_xname);
			goto out;
		}
	}
out:
	free(req, M_DEVBUF, sizeof(*req));
	return ret;
}

int
qwx_qmi_wlanfw_mode_send(struct qwx_softc *sc, enum ath11k_firmware_mode mode)
{
	int ret;
	struct qmi_wlanfw_wlan_mode_req_msg_v01 req = {};

	req.mode = mode;
	req.hw_debug_valid = 1;
	req.hw_debug = 0;

	ret = qwx_qmi_send_request(sc, QMI_WLANFW_WLAN_MODE_REQ_V01,
	    QMI_WLANFW_WLAN_MODE_REQ_MSG_V01_MAX_LEN,
	    qmi_wlanfw_wlan_mode_req_msg_v01_ei, &req, sizeof(req));
	if (ret) {
		printf("%s: failed to send wlan mode request, err = %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	sc->qmi_resp.result = QMI_RESULT_FAILURE_V01; 
	while (sc->qmi_resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tsleep_nsec(&sc->qmi_resp, 0, "qwxfwmode",
		    SEC_TO_NSEC(1));
		if (ret) {
			if (mode == ATH11K_FIRMWARE_MODE_OFF)
				return 0;
			printf("%s: wlan mode request timeout\n",
			    sc->sc_dev.dv_xname);
			return ret;
		}
	}

	return 0;
}

int
qwx_qmi_firmware_start(struct qwx_softc *sc, enum ath11k_firmware_mode mode)
{
	int ret;

	DPRINTF("%s: firmware start\n", sc->sc_dev.dv_xname);

	if (sc->hw_params.fw_wmi_diag_event) {
		ret = qwx_qmi_wlanfw_wlan_ini_send(sc, 1);
		if (ret < 0) {
			printf("%s: qmi failed to send wlan fw ini: %d\n",
			    sc->sc_dev.dv_xname, ret);
			return ret;
		}
	}

	ret = qwx_qmi_wlanfw_wlan_cfg_send(sc);
	if (ret) {
		printf("%s: qmi failed to send wlan cfg: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_qmi_wlanfw_mode_send(sc, mode);
	if (ret) {
		printf("%s: qmi failed to send wlan fw mode: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	return 0;
}

void
qwx_qmi_firmware_stop(struct qwx_softc *sc)
{
	int ret;

	ret = qwx_qmi_wlanfw_mode_send(sc, ATH11K_FIRMWARE_MODE_OFF);
	if (ret) {
		printf("%s: qmi failed to send wlan mode off: %d\n",
		    sc->sc_dev.dv_xname, ret);
	}
}

int
qwx_core_start_firmware(struct qwx_softc *sc, enum ath11k_firmware_mode mode)
{
	int ret;

	qwx_ce_get_shadow_config(sc, &sc->qmi_ce_cfg.shadow_reg_v2,
	    &sc->qmi_ce_cfg.shadow_reg_v2_len);

	ret = qwx_qmi_firmware_start(sc, mode);
	if (ret) {
		printf("%s: failed to send firmware start: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	return ret;
}

int
qwx_wmi_pdev_attach(struct qwx_softc *sc, uint8_t pdev_id)
{
	struct qwx_pdev_wmi *wmi_handle;

	if (pdev_id >= sc->hw_params.max_radios)
		return EINVAL;

	wmi_handle = &sc->wmi.wmi[pdev_id];
	wmi_handle->wmi = &sc->wmi;

	wmi_handle->tx_ce_desc = 1;

	return 0;
}

void
qwx_wmi_detach(struct qwx_softc *sc)
{
	qwx_wmi_free_dbring_caps(sc);
}

int
qwx_wmi_attach(struct qwx_softc *sc)
{
	int ret;

	ret = qwx_wmi_pdev_attach(sc, 0);
	if (ret)
		return ret;

	sc->wmi.sc = sc;
	sc->wmi.preferred_hw_mode = WMI_HOST_HW_MODE_MAX;
	sc->wmi.tx_credits = 1;

	/* It's overwritten when service_ext_ready is handled */
	if (sc->hw_params.single_pdev_only &&
	    sc->hw_params.num_rxmda_per_pdev > 1)
		sc->wmi.preferred_hw_mode = WMI_HOST_HW_MODE_SINGLE;

	return 0;
}

void
qwx_wmi_htc_tx_complete(struct qwx_softc *sc, struct mbuf *m)
{
	struct qwx_pdev_wmi *wmi = NULL;
	uint32_t i;
	uint8_t wmi_ep_count;
	uint8_t eid;

	eid = (uintptr_t)m->m_pkthdr.ph_cookie;
	m_freem(m);

	if (eid >= ATH11K_HTC_EP_COUNT)
		return;

	wmi_ep_count = sc->htc.wmi_ep_count;
	if (wmi_ep_count > sc->hw_params.max_radios)
		return;

	for (i = 0; i < sc->htc.wmi_ep_count; i++) {
		if (sc->wmi.wmi[i].eid == eid) {
			wmi = &sc->wmi.wmi[i];
			break;
		}
	}

	if (wmi)
		wakeup(&wmi->tx_ce_desc);
}

int
qwx_wmi_tlv_services_parser(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	const struct wmi_service_available_event *ev;
	uint32_t *wmi_ext2_service_bitmap;
	int i, j;

	switch (tag) {
	case WMI_TAG_SERVICE_AVAILABLE_EVENT:
		ev = (struct wmi_service_available_event *)ptr;
		for (i = 0, j = WMI_MAX_SERVICE;
		    i < WMI_SERVICE_SEGMENT_BM_SIZE32 &&
		    j < WMI_MAX_EXT_SERVICE;
		    i++) {
			do {
				if (ev->wmi_service_segment_bitmap[i] &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					setbit(sc->wmi.svc_map, j);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		DNPRINTF(QWX_D_WMI,
		    "%s: wmi_ext_service_bitmap 0:0x%04x, 1:0x%04x, "
		    "2:0x%04x, 3:0x%04x\n", __func__,
		    ev->wmi_service_segment_bitmap[0],
		    ev->wmi_service_segment_bitmap[1],
		    ev->wmi_service_segment_bitmap[2],
		    ev->wmi_service_segment_bitmap[3]);
		break;
	case WMI_TAG_ARRAY_UINT32:
		wmi_ext2_service_bitmap = (uint32_t *)ptr;
		for (i = 0, j = WMI_MAX_EXT_SERVICE;
		    i < WMI_SERVICE_SEGMENT_BM_SIZE32 &&
		    j < WMI_MAX_EXT2_SERVICE;
		    i++) {
			do {
				if (wmi_ext2_service_bitmap[i] &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					setbit(sc->wmi.svc_map, j);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		DNPRINTF(QWX_D_WMI,
		    "%s: wmi_ext2_service__bitmap  0:0x%04x, 1:0x%04x, "
		    "2:0x%04x, 3:0x%04x\n", __func__,
		    wmi_ext2_service_bitmap[0], wmi_ext2_service_bitmap[1],
		    wmi_ext2_service_bitmap[2], wmi_ext2_service_bitmap[3]);
		break;
	}

	return 0;
}

static const struct wmi_tlv_policy wmi_tlv_policies[] = {
	[WMI_TAG_ARRAY_BYTE]
		= { .min_len = 0 },
	[WMI_TAG_ARRAY_UINT32]
		= { .min_len = 0 },
	[WMI_TAG_SERVICE_READY_EVENT]
		= { .min_len = sizeof(struct wmi_service_ready_event) },
	[WMI_TAG_SERVICE_READY_EXT_EVENT]
		= { .min_len =  sizeof(struct wmi_service_ready_ext_event) },
	[WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS]
		= { .min_len = sizeof(struct wmi_soc_mac_phy_hw_mode_caps) },
	[WMI_TAG_SOC_HAL_REG_CAPABILITIES]
		= { .min_len = sizeof(struct wmi_soc_hal_reg_capabilities) },
	[WMI_TAG_VDEV_START_RESPONSE_EVENT]
		= { .min_len = sizeof(struct wmi_vdev_start_resp_event) },
	[WMI_TAG_PEER_DELETE_RESP_EVENT]
		= { .min_len = sizeof(struct wmi_peer_delete_resp_event) },
	[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT]
		= { .min_len = sizeof(struct wmi_bcn_tx_status_event) },
	[WMI_TAG_VDEV_STOPPED_EVENT]
		= { .min_len = sizeof(struct wmi_vdev_stopped_event) },
	[WMI_TAG_REG_CHAN_LIST_CC_EVENT]
		= { .min_len = sizeof(struct wmi_reg_chan_list_cc_event) },
	[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT]
		= { .min_len = sizeof(struct wmi_reg_chan_list_cc_ext_event) },
	[WMI_TAG_MGMT_RX_HDR]
		= { .min_len = sizeof(struct wmi_mgmt_rx_hdr) },
	[WMI_TAG_MGMT_TX_COMPL_EVENT]
		= { .min_len = sizeof(struct wmi_mgmt_tx_compl_event) },
	[WMI_TAG_SCAN_EVENT]
		= { .min_len = sizeof(struct wmi_scan_event) },
	[WMI_TAG_PEER_STA_KICKOUT_EVENT]
		= { .min_len = sizeof(struct wmi_peer_sta_kickout_event) },
	[WMI_TAG_ROAM_EVENT]
		= { .min_len = sizeof(struct wmi_roam_event) },
	[WMI_TAG_CHAN_INFO_EVENT]
		= { .min_len = sizeof(struct wmi_chan_info_event) },
	[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT]
		= { .min_len = sizeof(struct wmi_pdev_bss_chan_info_event) },
	[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT]
		= { .min_len = sizeof(struct wmi_vdev_install_key_compl_event) },
	[WMI_TAG_READY_EVENT] = {
		.min_len = sizeof(struct wmi_ready_event_min) },
	[WMI_TAG_SERVICE_AVAILABLE_EVENT]
		= {.min_len = sizeof(struct wmi_service_available_event) },
	[WMI_TAG_PEER_ASSOC_CONF_EVENT]
		= { .min_len = sizeof(struct wmi_peer_assoc_conf_event) },
	[WMI_TAG_STATS_EVENT]
		= { .min_len = sizeof(struct wmi_stats_event) },
	[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT]
		= { .min_len = sizeof(struct wmi_pdev_ctl_failsafe_chk_event) },
	[WMI_TAG_HOST_SWFDA_EVENT] = {
		.min_len = sizeof(struct wmi_fils_discovery_event) },
	[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_probe_resp_tx_status_event) },
	[WMI_TAG_VDEV_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_delete_resp_event) },
	[WMI_TAG_OBSS_COLOR_COLLISION_EVT] = {
		.min_len = sizeof(struct wmi_obss_color_collision_event) },
	[WMI_TAG_11D_NEW_COUNTRY_EVENT] = {
		.min_len = sizeof(struct wmi_11d_new_cc_ev) },
	[WMI_TAG_PER_CHAIN_RSSI_STATS] = {
		.min_len = sizeof(struct wmi_per_chain_rssi_stats) },
	[WMI_TAG_TWT_ADD_DIALOG_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_add_dialog_event) },
};

int
qwx_wmi_tlv_iter(struct qwx_softc *sc, const void *ptr, size_t len,
    int (*iter)(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data), void *data)
{
	const void *begin = ptr;
	const struct wmi_tlv *tlv;
	uint16_t tlv_tag, tlv_len;
	int ret;

	while (len > 0) {
		if (len < sizeof(*tlv)) {
			printf("%s: wmi tlv parse failure at byte %zd "
			    "(%zu bytes left, %zu expected)\n", __func__,
			    ptr - begin, len, sizeof(*tlv));
			return EINVAL;
		}

		tlv = ptr;
		tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
		tlv_len = FIELD_GET(WMI_TLV_LEN, tlv->header);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);

		if (tlv_len > len) {
			printf("%s: wmi tlv parse failure of tag %u "
			    "at byte %zd (%zu bytes left, %u expected)\n",
			    __func__, tlv_tag, ptr - begin, len, tlv_len);
			return EINVAL;
		}

		if (tlv_tag < nitems(wmi_tlv_policies) &&
		    wmi_tlv_policies[tlv_tag].min_len &&
		    wmi_tlv_policies[tlv_tag].min_len > tlv_len) {
			printf("%s: wmi tlv parse failure of tag %u "
			    "at byte %zd (%u bytes is less than "
			    "min length %zu)\n", __func__,
			    tlv_tag, ptr - begin, tlv_len,
			    wmi_tlv_policies[tlv_tag].min_len);
			return EINVAL;
		}

		ret = iter(sc, tlv_tag, tlv_len, ptr, data);
		if (ret)
			return ret;

		ptr += tlv_len;
		len -= tlv_len;
	}

	return 0;
}

int
qwx_pull_service_ready_tlv(struct qwx_softc *sc, const void *evt_buf,
    struct ath11k_targ_cap *cap)
{
	const struct wmi_service_ready_event *ev = evt_buf;

	if (!ev)
		return EINVAL;

	cap->phy_capability = ev->phy_capability;
	cap->max_frag_entry = ev->max_frag_entry;
	cap->num_rf_chains = ev->num_rf_chains;
	cap->ht_cap_info = ev->ht_cap_info;
	cap->vht_cap_info = ev->vht_cap_info;
	cap->vht_supp_mcs = ev->vht_supp_mcs;
	cap->hw_min_tx_power = ev->hw_min_tx_power;
	cap->hw_max_tx_power = ev->hw_max_tx_power;
	cap->sys_cap_info = ev->sys_cap_info;
	cap->min_pkt_size_enable = ev->min_pkt_size_enable;
	cap->max_bcn_ie_size = ev->max_bcn_ie_size;
	cap->max_num_scan_channels = ev->max_num_scan_channels;
	cap->max_supported_macs = ev->max_supported_macs;
	cap->wmi_fw_sub_feat_caps = ev->wmi_fw_sub_feat_caps;
	cap->txrx_chainmask = ev->txrx_chainmask;
	cap->default_dbs_hw_mode_index = ev->default_dbs_hw_mode_index;
	cap->num_msdu_desc = ev->num_msdu_desc;

	return 0;
}

/* Save the wmi_service_bitmap into a linear bitmap. The wmi_services in
 * wmi_service ready event are advertised in b0-b3 (LSB 4-bits) of each
 * 4-byte word.
 */
void
qwx_wmi_service_bitmap_copy(struct qwx_pdev_wmi *wmi,
    const uint32_t *wmi_svc_bm)
{
	int i, j = 0;

	for (i = 0; i < WMI_SERVICE_BM_SIZE && j < WMI_MAX_SERVICE; i++) {
		do {
			if (wmi_svc_bm[i] & BIT(j % WMI_SERVICE_BITS_IN_SIZE32))
				setbit(wmi->wmi->svc_map, j);
		} while (++j % WMI_SERVICE_BITS_IN_SIZE32);
	}
}

int
qwx_wmi_tlv_svc_rdy_parse(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	struct wmi_tlv_svc_ready_parse *svc_ready = data;
	struct qwx_pdev_wmi *wmi_handle = &sc->wmi.wmi[0];
	uint16_t expect_len;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EVENT:
		if (qwx_pull_service_ready_tlv(sc, ptr, &sc->target_caps))
			return EINVAL;
		break;

	case WMI_TAG_ARRAY_UINT32:
		if (!svc_ready->wmi_svc_bitmap_done) {
			expect_len = WMI_SERVICE_BM_SIZE * sizeof(uint32_t);
			if (len < expect_len) {
				printf("%s: invalid len %d for the tag 0x%x\n",
				    __func__, len, tag);
				return EINVAL;
			}

			qwx_wmi_service_bitmap_copy(wmi_handle, ptr);

			svc_ready->wmi_svc_bitmap_done = 1;
		}
		break;
	default:
		break;
	}

	return 0;
}

void
qwx_service_ready_event(struct qwx_softc *sc, struct mbuf *m)
{
	struct wmi_tlv_svc_ready_parse svc_ready = { };
	int ret;

	ret = qwx_wmi_tlv_iter(sc, mtod(m, void *), m->m_pkthdr.len,
	    qwx_wmi_tlv_svc_rdy_parse, &svc_ready);
	if (ret) {
		printf("%s: failed to parse tlv %d\n", __func__, ret);
		return;
	}

	DNPRINTF(QWX_D_WMI, "%s: event service ready\n", __func__);
}

int
qwx_pull_svc_ready_ext(struct qwx_pdev_wmi *wmi_handle, const void *ptr,
    struct ath11k_service_ext_param *param)
{
	const struct wmi_service_ready_ext_event *ev = ptr;

	if (!ev)
		return EINVAL;

	/* Move this to host based bitmap */
	param->default_conc_scan_config_bits = ev->default_conc_scan_config_bits;
	param->default_fw_config_bits =	ev->default_fw_config_bits;
	param->he_cap_info = ev->he_cap_info;
	param->mpdu_density = ev->mpdu_density;
	param->max_bssid_rx_filters = ev->max_bssid_rx_filters;
	memcpy(&param->ppet, &ev->ppet, sizeof(param->ppet));

	return 0;
}

int
qwx_pull_mac_phy_cap_svc_ready_ext(struct qwx_pdev_wmi *wmi_handle,
    struct wmi_soc_mac_phy_hw_mode_caps *hw_caps,
    struct wmi_hw_mode_capabilities *wmi_hw_mode_caps,
    struct wmi_soc_hal_reg_capabilities *hal_reg_caps,
    struct wmi_mac_phy_capabilities *wmi_mac_phy_caps,
    uint8_t hw_mode_id, uint8_t phy_id, struct qwx_pdev *pdev)
{
	struct wmi_mac_phy_capabilities *mac_phy_caps;
	struct qwx_softc *sc = wmi_handle->wmi->sc;
	struct ath11k_band_cap *cap_band;
	struct ath11k_pdev_cap *pdev_cap = &pdev->cap;
	uint32_t phy_map;
	uint32_t hw_idx, phy_idx = 0;

	if (!hw_caps || !wmi_hw_mode_caps || !hal_reg_caps)
		return EINVAL;

	for (hw_idx = 0; hw_idx < hw_caps->num_hw_modes; hw_idx++) {
		if (hw_mode_id == wmi_hw_mode_caps[hw_idx].hw_mode_id)
			break;

		phy_map = wmi_hw_mode_caps[hw_idx].phy_id_map;
		while (phy_map) {
			phy_map >>= 1;
			phy_idx++;
		}
	}

	if (hw_idx == hw_caps->num_hw_modes)
		return EINVAL;

	phy_idx += phy_id;
	if (phy_id >= hal_reg_caps->num_phy)
		return EINVAL;

	mac_phy_caps = wmi_mac_phy_caps + phy_idx;

	pdev->pdev_id = mac_phy_caps->pdev_id;
	pdev_cap->supported_bands |= mac_phy_caps->supported_bands;
	pdev_cap->ampdu_density = mac_phy_caps->ampdu_density;
	sc->target_pdev_ids[sc->target_pdev_count].supported_bands =
	    mac_phy_caps->supported_bands;
	sc->target_pdev_ids[sc->target_pdev_count].pdev_id = mac_phy_caps->pdev_id;
	sc->target_pdev_count++;

	if (!(mac_phy_caps->supported_bands & WMI_HOST_WLAN_2G_CAP) &&
	    !(mac_phy_caps->supported_bands & WMI_HOST_WLAN_5G_CAP))
		return EINVAL;

	/* Take non-zero tx/rx chainmask. If tx/rx chainmask differs from
	 * band to band for a single radio, need to see how this should be
	 * handled.
	 */
	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_2G_CAP) {
		pdev_cap->tx_chain_mask = mac_phy_caps->tx_chain_mask_2g;
		pdev_cap->rx_chain_mask = mac_phy_caps->rx_chain_mask_2g;
	}

	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_5G_CAP) {
		pdev_cap->vht_cap = mac_phy_caps->vht_cap_info_5g;
		pdev_cap->vht_mcs = mac_phy_caps->vht_supp_mcs_5g;
		pdev_cap->he_mcs = mac_phy_caps->he_supp_mcs_5g;
		pdev_cap->tx_chain_mask = mac_phy_caps->tx_chain_mask_5g;
		pdev_cap->rx_chain_mask = mac_phy_caps->rx_chain_mask_5g;
		pdev_cap->nss_ratio_enabled =
		    WMI_NSS_RATIO_ENABLE_DISABLE_GET(mac_phy_caps->nss_ratio);
		pdev_cap->nss_ratio_info =
		    WMI_NSS_RATIO_INFO_GET(mac_phy_caps->nss_ratio);
	}

	/* tx/rx chainmask reported from fw depends on the actual hw chains used,
	 * For example, for 4x4 capable macphys, first 4 chains can be used for first
	 * mac and the remaining 4 chains can be used for the second mac or vice-versa.
	 * In this case, tx/rx chainmask 0xf will be advertised for first mac and 0xf0
	 * will be advertised for second mac or vice-versa. Compute the shift value
	 * for tx/rx chainmask which will be used to advertise supported ht/vht rates to
	 * mac80211.
	 */
	pdev_cap->tx_chain_mask_shift = ffs(pdev_cap->tx_chain_mask);
	pdev_cap->rx_chain_mask_shift = ffs(pdev_cap->rx_chain_mask);

	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_2G_CAP) {
		cap_band = &pdev_cap->band[0];
		cap_band->phy_id = mac_phy_caps->phy_id;
		cap_band->max_bw_supported = mac_phy_caps->max_bw_supported_2g;
		cap_band->ht_cap_info = mac_phy_caps->ht_cap_info_2g;
		cap_band->he_cap_info[0] = mac_phy_caps->he_cap_info_2g;
		cap_band->he_cap_info[1] = mac_phy_caps->he_cap_info_2g_ext;
		cap_band->he_mcs = mac_phy_caps->he_supp_mcs_2g;
		memcpy(cap_band->he_cap_phy_info,
		    &mac_phy_caps->he_cap_phy_info_2g,
		    sizeof(uint32_t) * PSOC_HOST_MAX_PHY_SIZE);
		memcpy(&cap_band->he_ppet, &mac_phy_caps->he_ppet2g,
		    sizeof(struct ath11k_ppe_threshold));
	}

	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_5G_CAP) {
		cap_band = &pdev_cap->band[1];
		cap_band->phy_id = mac_phy_caps->phy_id;
		cap_band->max_bw_supported = mac_phy_caps->max_bw_supported_5g;
		cap_band->ht_cap_info = mac_phy_caps->ht_cap_info_5g;
		cap_band->he_cap_info[0] = mac_phy_caps->he_cap_info_5g;
		cap_band->he_cap_info[1] = mac_phy_caps->he_cap_info_5g_ext;
		cap_band->he_mcs = mac_phy_caps->he_supp_mcs_5g;
		memcpy(cap_band->he_cap_phy_info, &mac_phy_caps->he_cap_phy_info_5g,
		    sizeof(uint32_t) * PSOC_HOST_MAX_PHY_SIZE);
		memcpy(&cap_band->he_ppet, &mac_phy_caps->he_ppet5g,
		    sizeof(struct ath11k_ppe_threshold));
#if 0
		cap_band = &pdev_cap->band[NL80211_BAND_6GHZ];
		cap_band->max_bw_supported = mac_phy_caps->max_bw_supported_5g;
		cap_band->ht_cap_info = mac_phy_caps->ht_cap_info_5g;
		cap_band->he_cap_info[0] = mac_phy_caps->he_cap_info_5g;
		cap_band->he_cap_info[1] = mac_phy_caps->he_cap_info_5g_ext;
		cap_band->he_mcs = mac_phy_caps->he_supp_mcs_5g;
		memcpy(cap_band->he_cap_phy_info, &mac_phy_caps->he_cap_phy_info_5g,
		       sizeof(u32) * PSOC_HOST_MAX_PHY_SIZE);
		memcpy(&cap_band->he_ppet, &mac_phy_caps->he_ppet5g,
		       sizeof(struct ath11k_ppe_threshold));
#endif
	}

	return 0;
}

int
qwx_wmi_tlv_ext_soc_hal_reg_caps_parse(struct qwx_softc *sc, uint16_t len,
    const void *ptr, void *data)
{
	struct qwx_pdev_wmi *wmi_handle = &sc->wmi.wmi[0];
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	uint8_t hw_mode_id = svc_rdy_ext->pref_hw_mode_caps.hw_mode_id;
	uint32_t phy_id_map;
	int pdev_index = 0;
	int ret;

	svc_rdy_ext->soc_hal_reg_caps = (struct wmi_soc_hal_reg_capabilities *)ptr;
	svc_rdy_ext->param.num_phy = svc_rdy_ext->soc_hal_reg_caps->num_phy;

	sc->num_radios = 0;
	sc->target_pdev_count = 0;
	phy_id_map = svc_rdy_ext->pref_hw_mode_caps.phy_id_map;

	while (phy_id_map && sc->num_radios < MAX_RADIOS) {
		ret = qwx_pull_mac_phy_cap_svc_ready_ext(wmi_handle,
		    svc_rdy_ext->hw_caps,
		    svc_rdy_ext->hw_mode_caps,
		    svc_rdy_ext->soc_hal_reg_caps,
		    svc_rdy_ext->mac_phy_caps,
		    hw_mode_id, sc->num_radios, &sc->pdevs[pdev_index]);
		if (ret) {
			printf("%s: failed to extract mac caps, idx: %d\n",
			    __func__, sc->num_radios);
			return ret;
		}

		sc->num_radios++;

		/* For QCA6390, save mac_phy capability in the same pdev */
		if (sc->hw_params.single_pdev_only)
			pdev_index = 0;
		else
			pdev_index = sc->num_radios;

		/* TODO: mac_phy_cap prints */
		phy_id_map >>= 1;
	}

	/* For QCA6390, set num_radios to 1 because host manages
	 * both 2G and 5G radio in one pdev.
	 * Set pdev_id = 0 and 0 means soc level.
	 */
	if (sc->hw_params.single_pdev_only) {
		sc->num_radios = 1;
		sc->pdevs[0].pdev_id = 0;
	}

	return 0;
}

int
qwx_wmi_tlv_hw_mode_caps_parse(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_hw_mode_capabilities *hw_mode_cap;
	uint32_t phy_map = 0;

	if (tag != WMI_TAG_HW_MODE_CAPABILITIES)
		return EPROTO;

	if (svc_rdy_ext->n_hw_mode_caps >= svc_rdy_ext->param.num_hw_modes)
		return ENOBUFS;

	hw_mode_cap = container_of(ptr, struct wmi_hw_mode_capabilities,
	    hw_mode_id);
	svc_rdy_ext->n_hw_mode_caps++;

	phy_map = hw_mode_cap->phy_id_map;
	while (phy_map) {
		svc_rdy_ext->tot_phy_id++;
		phy_map = phy_map >> 1;
	}

	return 0;
}

#define PRIMAP(_hw_mode_) \
	[_hw_mode_] = _hw_mode_##_PRI

static const int qwx_hw_mode_pri_map[] = {
	PRIMAP(WMI_HOST_HW_MODE_SINGLE),
	PRIMAP(WMI_HOST_HW_MODE_DBS),
	PRIMAP(WMI_HOST_HW_MODE_SBS_PASSIVE),
	PRIMAP(WMI_HOST_HW_MODE_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_OR_SBS),
	/* keep last */
	PRIMAP(WMI_HOST_HW_MODE_MAX),
};

int
qwx_wmi_tlv_hw_mode_caps(struct qwx_softc *sc, uint16_t len,
    const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_hw_mode_capabilities *hw_mode_caps;
	enum wmi_host_hw_mode_config_type mode, pref;
	uint32_t i;
	int ret;

	svc_rdy_ext->n_hw_mode_caps = 0;
	svc_rdy_ext->hw_mode_caps = (struct wmi_hw_mode_capabilities *)ptr;

	ret = qwx_wmi_tlv_iter(sc, ptr, len,
	    qwx_wmi_tlv_hw_mode_caps_parse, svc_rdy_ext);
	if (ret) {
		printf("%s: failed to parse tlv %d\n", __func__, ret);
		return ret;
	}

	i = 0;
	while (i < svc_rdy_ext->n_hw_mode_caps) {
		hw_mode_caps = &svc_rdy_ext->hw_mode_caps[i];
		mode = hw_mode_caps->hw_mode_id;
		pref = sc->wmi.preferred_hw_mode;

		if (qwx_hw_mode_pri_map[mode] < qwx_hw_mode_pri_map[pref]) {
			svc_rdy_ext->pref_hw_mode_caps = *hw_mode_caps;
			sc->wmi.preferred_hw_mode = mode;
		}
		i++;
	}

	DNPRINTF(QWX_D_WMI, "%s: preferred_hw_mode: %d\n", __func__,
	    sc->wmi.preferred_hw_mode);
	if (sc->wmi.preferred_hw_mode >= WMI_HOST_HW_MODE_MAX)
		return EINVAL;

	return 0;
}

int
qwx_wmi_tlv_mac_phy_caps_parse(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES)
		return EPROTO;

	if (svc_rdy_ext->n_mac_phy_caps >= svc_rdy_ext->tot_phy_id)
		return ENOBUFS;

	len = MIN(len, sizeof(struct wmi_mac_phy_capabilities));
	if (!svc_rdy_ext->n_mac_phy_caps) {
		svc_rdy_ext->mac_phy_caps = mallocarray(
		    svc_rdy_ext->tot_phy_id,
		    sizeof(struct wmi_mac_phy_capabilities),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!svc_rdy_ext->mac_phy_caps)
			return ENOMEM;
		svc_rdy_ext->mac_phy_caps_size = len * svc_rdy_ext->tot_phy_id;
	}

	memcpy(svc_rdy_ext->mac_phy_caps + svc_rdy_ext->n_mac_phy_caps,
	    ptr, len);
	svc_rdy_ext->n_mac_phy_caps++;
	return 0;
}

int
qwx_wmi_tlv_ext_hal_reg_caps_parse(struct qwx_softc *sc,
    uint16_t tag, uint16_t len, const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_HAL_REG_CAPABILITIES_EXT)
		return EPROTO;

	if (svc_rdy_ext->n_ext_hal_reg_caps >= svc_rdy_ext->param.num_phy)
		return ENOBUFS;

	svc_rdy_ext->n_ext_hal_reg_caps++;
	return 0;
}

int
qwx_pull_reg_cap_svc_rdy_ext(struct qwx_pdev_wmi *wmi_handle,
    struct wmi_soc_hal_reg_capabilities *reg_caps,
    struct wmi_hal_reg_capabilities_ext *wmi_ext_reg_cap,
    uint8_t phy_idx, struct ath11k_hal_reg_capabilities_ext *param)
{
	struct wmi_hal_reg_capabilities_ext *ext_reg_cap;

	if (!reg_caps || !wmi_ext_reg_cap)
		return EINVAL;

	if (phy_idx >= reg_caps->num_phy)
		return EINVAL;

	ext_reg_cap = &wmi_ext_reg_cap[phy_idx];

	param->phy_id = ext_reg_cap->phy_id;
	param->eeprom_reg_domain = ext_reg_cap->eeprom_reg_domain;
	param->eeprom_reg_domain_ext = ext_reg_cap->eeprom_reg_domain_ext;
	param->regcap1 = ext_reg_cap->regcap1;
	param->regcap2 = ext_reg_cap->regcap2;
	/* check if param->wireless_mode is needed */
	param->low_2ghz_chan = ext_reg_cap->low_2ghz_chan;
	param->high_2ghz_chan = ext_reg_cap->high_2ghz_chan;
	param->low_5ghz_chan = ext_reg_cap->low_5ghz_chan;
	param->high_5ghz_chan = ext_reg_cap->high_5ghz_chan;

	return 0;
}

int
qwx_wmi_tlv_ext_hal_reg_caps(struct qwx_softc *sc, uint16_t len,
    const void *ptr, void *data)
{
	struct qwx_pdev_wmi *wmi_handle = &sc->wmi.wmi[0];
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath11k_hal_reg_capabilities_ext reg_cap;
	int ret;
	uint32_t i;

	svc_rdy_ext->n_ext_hal_reg_caps = 0;
	svc_rdy_ext->ext_hal_reg_caps =
	    (struct wmi_hal_reg_capabilities_ext *)ptr;
	ret = qwx_wmi_tlv_iter(sc, ptr, len,
	    qwx_wmi_tlv_ext_hal_reg_caps_parse, svc_rdy_ext);
	if (ret) {
		printf("%s: failed to parse tlv %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < svc_rdy_ext->param.num_phy; i++) {
		ret = qwx_pull_reg_cap_svc_rdy_ext(wmi_handle,
		    svc_rdy_ext->soc_hal_reg_caps,
		    svc_rdy_ext->ext_hal_reg_caps, i, &reg_cap);
		if (ret) {
			printf("%s: failed to extract reg cap %d\n",
			    __func__, i);
			return ret;
		}

		memcpy(&sc->hal_reg_cap[reg_cap.phy_id], &reg_cap,
		    sizeof(sc->hal_reg_cap[0]));
	}

	return 0;
}

int
qwx_wmi_tlv_dma_ring_caps_parse(struct qwx_softc *sc, uint16_t tag,
    uint16_t len, const void *ptr, void *data)
{
	struct wmi_tlv_dma_ring_caps_parse *parse = data;

	if (tag != WMI_TAG_DMA_RING_CAPABILITIES)
		return EPROTO;

	parse->n_dma_ring_caps++;
	return 0;
}

int
qwx_wmi_alloc_dbring_caps(struct qwx_softc *sc, uint32_t num_cap)
{
	void *ptr;

	ptr = mallocarray(num_cap, sizeof(struct qwx_dbring_cap),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!ptr)
		return ENOMEM;

	sc->db_caps = ptr;
	sc->num_db_cap = num_cap;

	return 0;
}

void
qwx_wmi_free_dbring_caps(struct qwx_softc *sc)
{
	free(sc->db_caps, M_DEVBUF,
	    sc->num_db_cap * sizeof(struct qwx_dbring_cap));
	sc->db_caps = NULL;
	sc->num_db_cap = 0;
}

int
qwx_wmi_tlv_dma_ring_caps(struct qwx_softc *sc, uint16_t len,
    const void *ptr, void *data)
{
	struct wmi_tlv_dma_ring_caps_parse *dma_caps_parse = data;
	struct wmi_dma_ring_capabilities *dma_caps;
	struct qwx_dbring_cap *dir_buff_caps;
	int ret;
	uint32_t i;

	dma_caps_parse->n_dma_ring_caps = 0;
	dma_caps = (struct wmi_dma_ring_capabilities *)ptr;
	ret = qwx_wmi_tlv_iter(sc, ptr, len,
	    qwx_wmi_tlv_dma_ring_caps_parse, dma_caps_parse);
	if (ret) {
		printf("%s: failed to parse dma ring caps tlv %d\n",
		    __func__, ret);
		return ret;
	}

	if (!dma_caps_parse->n_dma_ring_caps)
		return 0;

	if (sc->num_db_cap) {
		DNPRINTF(QWX_D_WMI,
		    "%s: Already processed, so ignoring dma ring caps\n",
		    __func__);
		return 0;
	}

	ret = qwx_wmi_alloc_dbring_caps(sc, dma_caps_parse->n_dma_ring_caps);
	if (ret)
		return ret;

	dir_buff_caps = sc->db_caps;
	for (i = 0; i < dma_caps_parse->n_dma_ring_caps; i++) {
		if (dma_caps[i].module_id >= WMI_DIRECT_BUF_MAX) {
			printf("%s: Invalid module id %d\n", __func__,
			    dma_caps[i].module_id);
			ret = EINVAL;
			goto free_dir_buff;
		}

		dir_buff_caps[i].id = dma_caps[i].module_id;
		dir_buff_caps[i].pdev_id = DP_HW2SW_MACID(dma_caps[i].pdev_id);
		dir_buff_caps[i].min_elem = dma_caps[i].min_elem;
		dir_buff_caps[i].min_buf_sz = dma_caps[i].min_buf_sz;
		dir_buff_caps[i].min_buf_align = dma_caps[i].min_buf_align;
	}

	return 0;

free_dir_buff:
	qwx_wmi_free_dbring_caps(sc);
	return ret;
}

int
qwx_wmi_tlv_svc_rdy_ext_parse(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	struct qwx_pdev_wmi *wmi_handle = &sc->wmi.wmi[0];
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	int ret;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT_EVENT:
		ret = qwx_pull_svc_ready_ext(wmi_handle, ptr,
		    &svc_rdy_ext->param);
		if (ret) {
			printf("%s: unable to extract ext params\n", __func__);
			return ret;
		}
		break;

	case WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS:
		svc_rdy_ext->hw_caps = (struct wmi_soc_mac_phy_hw_mode_caps *)ptr;
		svc_rdy_ext->param.num_hw_modes = svc_rdy_ext->hw_caps->num_hw_modes;
		break;

	case WMI_TAG_SOC_HAL_REG_CAPABILITIES:
		ret = qwx_wmi_tlv_ext_soc_hal_reg_caps_parse(sc, len, ptr,
		    svc_rdy_ext);
		if (ret)
			return ret;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!svc_rdy_ext->hw_mode_done) {
			ret = qwx_wmi_tlv_hw_mode_caps(sc, len, ptr,
			    svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->hw_mode_done = 1;
		} else if (!svc_rdy_ext->mac_phy_done) {
			svc_rdy_ext->n_mac_phy_caps = 0;
			ret = qwx_wmi_tlv_iter(sc, ptr, len,
			    qwx_wmi_tlv_mac_phy_caps_parse, svc_rdy_ext);
			if (ret) {
				printf("%s: failed to parse tlv %d\n",
				    __func__, ret);
				return ret;
			}

			svc_rdy_ext->mac_phy_done = 1;
		} else if (!svc_rdy_ext->ext_hal_reg_done) {
			ret = qwx_wmi_tlv_ext_hal_reg_caps(sc, len, ptr,
			    svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->ext_hal_reg_done = 1;
		} else if (!svc_rdy_ext->mac_phy_chainmask_combo_done) {
			svc_rdy_ext->mac_phy_chainmask_combo_done = 1;
		} else if (!svc_rdy_ext->mac_phy_chainmask_cap_done) {
			svc_rdy_ext->mac_phy_chainmask_cap_done = 1;
		} else if (!svc_rdy_ext->oem_dma_ring_cap_done) {
			svc_rdy_ext->oem_dma_ring_cap_done = 1;
		} else if (!svc_rdy_ext->dma_ring_cap_done) {
			ret = qwx_wmi_tlv_dma_ring_caps(sc, len, ptr,
			    &svc_rdy_ext->dma_caps_parse);
			if (ret)
				return ret;

			svc_rdy_ext->dma_ring_cap_done = 1;
		}
		break;

	default:
		break;
	}

	return 0;
}

void
qwx_service_ready_ext_event(struct qwx_softc *sc, struct mbuf *m)
{
	struct wmi_tlv_svc_rdy_ext_parse svc_rdy_ext = { };
	int ret;

	ret = qwx_wmi_tlv_iter(sc, mtod(m, void *), m->m_pkthdr.len,
	    qwx_wmi_tlv_svc_rdy_ext_parse, &svc_rdy_ext);
	if (ret) {
		printf("%s: failed to parse tlv %d\n", __func__, ret);
		qwx_wmi_free_dbring_caps(sc);
		return;
	}

	DNPRINTF(QWX_D_WMI, "%s: event service ready ext\n", __func__);

	if (!isset(sc->wmi.svc_map, WMI_TLV_SERVICE_EXT2_MSG))
		wakeup(&sc->wmi.service_ready);

	free(svc_rdy_ext.mac_phy_caps, M_DEVBUF,
	    svc_rdy_ext.mac_phy_caps_size);
}

int
qwx_wmi_tlv_svc_rdy_ext2_parse(struct qwx_softc *sc,
    uint16_t tag, uint16_t len, const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext2_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_ARRAY_STRUCT:
		if (!parse->dma_ring_cap_done) {
			ret = qwx_wmi_tlv_dma_ring_caps(sc, len, ptr,
			    &parse->dma_caps_parse);
			if (ret)
				return ret;

			parse->dma_ring_cap_done = 1;
		}
		break;
	default:
		break;
	}

	return 0;
}

void
qwx_service_ready_ext2_event(struct qwx_softc *sc, struct mbuf *m)
{
	struct wmi_tlv_svc_rdy_ext2_parse svc_rdy_ext2 = { };
	int ret;

	ret = qwx_wmi_tlv_iter(sc, mtod(m, void *), m->m_pkthdr.len,
	    qwx_wmi_tlv_svc_rdy_ext2_parse, &svc_rdy_ext2);
	if (ret) {
		printf("%s: failed to parse ext2 event tlv %d\n",
		    __func__, ret);
		qwx_wmi_free_dbring_caps(sc);
		return;
	}

	DNPRINTF(QWX_D_WMI, "%s: event service ready ext2\n", __func__);

	sc->wmi.service_ready = 1;
	wakeup(&sc->wmi.service_ready);
}

void
qwx_service_available_event(struct qwx_softc *sc, struct mbuf *m)
{
	int ret;

	ret = qwx_wmi_tlv_iter(sc, mtod(m, void *), m->m_pkthdr.len,
	    qwx_wmi_tlv_services_parser, NULL);
	if (ret)
		printf("%s: failed to parse services available tlv %d\n",
		    sc->sc_dev.dv_xname, ret);

	DNPRINTF(QWX_D_WMI, "%s: event service available\n", __func__);
}

int
qwx_wmi_tlv_rdy_parse(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	struct wmi_tlv_rdy_parse *rdy_parse = data;
	struct wmi_ready_event fixed_param;
	struct wmi_mac_addr *addr_list;
	struct qwx_pdev *pdev;
	uint32_t num_mac_addr;
	int i;

	switch (tag) {
	case WMI_TAG_READY_EVENT:
		memset(&fixed_param, 0, sizeof(fixed_param));
		memcpy(&fixed_param, (struct wmi_ready_event *)ptr,
		       MIN(sizeof(fixed_param), len));
		sc->wlan_init_status = fixed_param.ready_event_min.status;
		rdy_parse->num_extra_mac_addr =
			fixed_param.ready_event_min.num_extra_mac_addr;

		IEEE80211_ADDR_COPY(sc->mac_addr,
		    fixed_param.ready_event_min.mac_addr.addr);
		sc->pktlog_defs_checksum = fixed_param.pktlog_defs_checksum;
		sc->wmi_ready = 1;
		break;
	case WMI_TAG_ARRAY_FIXED_STRUCT:
		addr_list = (struct wmi_mac_addr *)ptr;
		num_mac_addr = rdy_parse->num_extra_mac_addr;

		if (!(sc->num_radios > 1 && num_mac_addr >= sc->num_radios))
			break;

		for (i = 0; i < sc->num_radios; i++) {
			pdev = &sc->pdevs[i];
			IEEE80211_ADDR_COPY(pdev->mac_addr, addr_list[i].addr);
		}
		sc->pdevs_macaddr_valid = 1;
		break;
	default:
		break;
	}

	return 0;
}

void
qwx_ready_event(struct qwx_softc *sc, struct mbuf *m)
{
	struct wmi_tlv_rdy_parse rdy_parse = { };
	int ret;

	ret = qwx_wmi_tlv_iter(sc, mtod(m, void *), m->m_pkthdr.len,
	    qwx_wmi_tlv_rdy_parse, &rdy_parse);
	if (ret) {
		printf("%s: failed to parse tlv %d\n", __func__, ret);
		return;
	}

	DNPRINTF(QWX_D_WMI, "%s: event ready", __func__);

	sc->wmi.unified_ready = 1;
	wakeup(&sc->wmi.unified_ready);
}

int
qwx_wmi_tlv_iter_parse(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	const void **tb = data;

	if (tag < WMI_TAG_MAX)
		tb[tag] = ptr;

	return 0;
}

int
qwx_wmi_tlv_parse(struct qwx_softc *sc, const void **tb,
    const void *ptr, size_t len)
{
	return qwx_wmi_tlv_iter(sc, ptr, len, qwx_wmi_tlv_iter_parse,
	    (void *)tb);
}


const void **
qwx_wmi_tlv_parse_alloc(struct qwx_softc *sc, const void *ptr, size_t len)
{
	const void **tb;
	int ret;

	tb = mallocarray(WMI_TAG_MAX, sizeof(*tb), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!tb)
		return NULL;

	ret = qwx_wmi_tlv_parse(sc, tb, ptr, len);
	if (ret) {
		free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
		return NULL;
	}

	return tb;
}

static void
qwx_print_reg_rule(struct qwx_softc *sc, const char *band,
    uint32_t num_reg_rules, struct cur_reg_rule *reg_rule_ptr)
{
	struct cur_reg_rule *reg_rule = reg_rule_ptr;
	uint32_t count;

	DNPRINTF(QWX_D_WMI, "%s: number of reg rules in %s band: %d\n",
	    __func__, band, num_reg_rules);

	for (count = 0; count < num_reg_rules; count++) {
		DNPRINTF(QWX_D_WMI,
		    "%s: reg rule %d: (%d - %d @ %d) (%d, %d) (FLAGS %d)\n",
		    __func__, count + 1, reg_rule->start_freq,
		    reg_rule->end_freq, reg_rule->max_bw, reg_rule->ant_gain,
		    reg_rule->reg_power, reg_rule->flags);
		reg_rule++;
	}
}

struct cur_reg_rule
*qwx_create_reg_rules_from_wmi(uint32_t num_reg_rules,
    struct wmi_regulatory_rule_struct *wmi_reg_rule)
{
	struct cur_reg_rule *reg_rule_ptr;
	uint32_t count;

	reg_rule_ptr = mallocarray(num_reg_rules, sizeof(*reg_rule_ptr),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!reg_rule_ptr)
		return NULL;

	for (count = 0; count < num_reg_rules; count++) {
		reg_rule_ptr[count].start_freq = FIELD_GET(REG_RULE_START_FREQ,
		    wmi_reg_rule[count].freq_info);
		reg_rule_ptr[count].end_freq = FIELD_GET(REG_RULE_END_FREQ,
		    wmi_reg_rule[count].freq_info);
		reg_rule_ptr[count].max_bw = FIELD_GET(REG_RULE_MAX_BW,
		    wmi_reg_rule[count].bw_pwr_info);
		reg_rule_ptr[count].reg_power = FIELD_GET(REG_RULE_REG_PWR,
		    wmi_reg_rule[count].bw_pwr_info);
		reg_rule_ptr[count].ant_gain = FIELD_GET(REG_RULE_ANT_GAIN,
		    wmi_reg_rule[count].bw_pwr_info);
		reg_rule_ptr[count].flags = FIELD_GET(REG_RULE_FLAGS,
		    wmi_reg_rule[count].flag_info);
	}

	return reg_rule_ptr;
}

int
qwx_pull_reg_chan_list_update_ev(struct qwx_softc *sc, struct mbuf *m,
    struct cur_regulatory_info *reg_info)
{
	const void **tb;
	const struct wmi_reg_chan_list_cc_event *chan_list_event_hdr;
	struct wmi_regulatory_rule_struct *wmi_reg_rule;
	uint32_t num_2ghz_reg_rules, num_5ghz_reg_rules;
	int ret;

	DNPRINTF(QWX_D_WMI, "%s: processing regulatory channel list\n",
	    __func__);

	tb = qwx_wmi_tlv_parse_alloc(sc, mtod(m, void *), m->m_pkthdr.len);
	if (tb == NULL) {
		ret = ENOMEM; /* XXX allocation failure or parsing failure? */
		printf("%s: failed to parse tlv: %d\n", __func__, ret);
		return ENOMEM;
	}

	chan_list_event_hdr = tb[WMI_TAG_REG_CHAN_LIST_CC_EVENT];
	if (!chan_list_event_hdr) {
		printf("%s: failed to fetch reg chan list update ev\n",
		    __func__);
		free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
		return EPROTO;
	}

	reg_info->num_2ghz_reg_rules = chan_list_event_hdr->num_2ghz_reg_rules;
	reg_info->num_5ghz_reg_rules = chan_list_event_hdr->num_5ghz_reg_rules;

	if (!(reg_info->num_2ghz_reg_rules + reg_info->num_5ghz_reg_rules)) {
		printf("%s: No regulatory rules available in the event info\n",
		    __func__);
		free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
		return EINVAL;
	}

	memcpy(reg_info->alpha2, &chan_list_event_hdr->alpha2, REG_ALPHA2_LEN);
	reg_info->dfs_region = chan_list_event_hdr->dfs_region;
	reg_info->phybitmap = chan_list_event_hdr->phybitmap;
	reg_info->num_phy = chan_list_event_hdr->num_phy;
	reg_info->phy_id = chan_list_event_hdr->phy_id;
	reg_info->ctry_code = chan_list_event_hdr->country_id;
	reg_info->reg_dmn_pair = chan_list_event_hdr->domain_code;

	DNPRINTF(QWX_D_WMI, "%s: CC status_code %s\n", __func__,
	    qwx_cc_status_to_str(reg_info->status_code));

	reg_info->status_code =
		qwx_wmi_cc_setting_code_to_reg(chan_list_event_hdr->status_code);

	reg_info->is_ext_reg_event = false;

	reg_info->min_bw_2ghz = chan_list_event_hdr->min_bw_2ghz;
	reg_info->max_bw_2ghz = chan_list_event_hdr->max_bw_2ghz;
	reg_info->min_bw_5ghz = chan_list_event_hdr->min_bw_5ghz;
	reg_info->max_bw_5ghz = chan_list_event_hdr->max_bw_5ghz;

	num_2ghz_reg_rules = reg_info->num_2ghz_reg_rules;
	num_5ghz_reg_rules = reg_info->num_5ghz_reg_rules;

	DNPRINTF(QWX_D_WMI,
	    "%s: cc %s dsf %d BW: min_2ghz %d max_2ghz %d min_5ghz %d "
	    "max_5ghz %d\n", __func__, reg_info->alpha2, reg_info->dfs_region,
	    reg_info->min_bw_2ghz, reg_info->max_bw_2ghz,
	    reg_info->min_bw_5ghz, reg_info->max_bw_5ghz);

	DNPRINTF(QWX_D_WMI,
	    "%s: num_2ghz_reg_rules %d num_5ghz_reg_rules %d\n", __func__,
	    num_2ghz_reg_rules, num_5ghz_reg_rules);

	wmi_reg_rule = (struct wmi_regulatory_rule_struct *)
	    ((uint8_t *)chan_list_event_hdr + sizeof(*chan_list_event_hdr)
	    + sizeof(struct wmi_tlv));

	if (num_2ghz_reg_rules) {
		reg_info->reg_rules_2ghz_ptr = qwx_create_reg_rules_from_wmi(
		    num_2ghz_reg_rules, wmi_reg_rule);
		if (!reg_info->reg_rules_2ghz_ptr) {
			free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
			printf("%s: Unable to allocate memory for "
			    "2 GHz rules\n", __func__);
			return ENOMEM;
		}

		qwx_print_reg_rule(sc, "2 GHz", num_2ghz_reg_rules,
		    reg_info->reg_rules_2ghz_ptr);
	}

	if (num_5ghz_reg_rules) {
		wmi_reg_rule += num_2ghz_reg_rules;
		reg_info->reg_rules_5ghz_ptr = qwx_create_reg_rules_from_wmi(
		    num_5ghz_reg_rules, wmi_reg_rule);
		if (!reg_info->reg_rules_5ghz_ptr) {
			free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
			printf("%s: Unable to allocate memory for "
			    "5 GHz rules\n", __func__);
			return ENOMEM;
		}

		qwx_print_reg_rule(sc, "5 GHz", num_5ghz_reg_rules,
		    reg_info->reg_rules_5ghz_ptr);
	}

	DNPRINTF(QWX_D_WMI, "%s: processed regulatory channel list\n",
	    __func__);

	free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
	return 0;
}

int
qwx_pull_reg_chan_list_ext_update_ev(struct qwx_softc *sc, struct mbuf *m,
    struct cur_regulatory_info *reg_info)
{
	printf("%s: not implemented\n", __func__);
	return ENOTSUP;
}

void
qwx_init_channels(struct qwx_softc *sc, struct cur_regulatory_info *reg_info)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *chan;
	struct cur_reg_rule *rule;
	int i, chnum;
	uint16_t freq;

	for (i = 0; i < reg_info->num_2ghz_reg_rules; i++) {
		rule = &reg_info->reg_rules_2ghz_ptr[i];
		if (rule->start_freq < 2402 ||
		    rule->start_freq > 2500 ||
		    rule->start_freq > rule->end_freq) {
			DPRINTF("%s: bad regulatory rule: start freq %u, "
			    "end freq %u\n", __func__, rule->start_freq,
			    rule->end_freq);
			continue;
		}

		freq = rule->start_freq + 10;
		chnum = ieee80211_mhz2ieee(freq, IEEE80211_CHAN_2GHZ);
		if (chnum < 1 || chnum > 14) {
			DPRINTF("%s: bad regulatory rule: freq %u, "
			    "channel %u\n", __func__, freq, chnum);
			continue;
		}
		while (freq <= rule->end_freq && chnum <= 14) {
			chan = &ic->ic_channels[chnum];
			if (rule->flags & REGULATORY_CHAN_DISABLED) {
				chan->ic_freq = 0;
				chan->ic_flags = 0;
			} else {
				chan->ic_freq = freq;
				chan->ic_flags = IEEE80211_CHAN_CCK |
				    IEEE80211_CHAN_OFDM |
				    IEEE80211_CHAN_DYN |
				    IEEE80211_CHAN_2GHZ;
			}
			chnum++;
			freq = ieee80211_ieee2mhz(chnum, IEEE80211_CHAN_2GHZ);
		}
	}

	for (i = 0; i < reg_info->num_5ghz_reg_rules; i++) {
		rule = &reg_info->reg_rules_5ghz_ptr[i];
		if (rule->start_freq < 5170 ||
		    rule->start_freq > 6000 ||
		    rule->start_freq > rule->end_freq) {
			DPRINTF("%s: bad regulatory rule: start freq %u, "
			    "end freq %u\n", __func__, rule->start_freq,
			    rule->end_freq);
			continue;
		}

		freq = rule->start_freq + 10;
		chnum = ieee80211_mhz2ieee(freq, IEEE80211_CHAN_5GHZ);
		if (chnum < 36 || chnum > IEEE80211_CHAN_MAX) {
			DPRINTF("%s: bad regulatory rule: freq %u, "
			    "channel %u\n", __func__, freq, chnum);
			continue;
		}
		while (freq <= rule->end_freq && freq <= 5885 &&
		    chnum <= IEEE80211_CHAN_MAX) {
			chan = &ic->ic_channels[chnum];
			if (rule->flags & (REGULATORY_CHAN_DISABLED |
			    REGULATORY_CHAN_NO_OFDM)) {
				chan->ic_freq = 0;
				chan->ic_flags = 0;
			} else {
				chan->ic_freq = freq;
				chan->ic_flags = IEEE80211_CHAN_A;
				if (rule->flags & (REGULATORY_CHAN_RADAR |
				    REGULATORY_CHAN_NO_IR |
				    REGULATORY_CHAN_INDOOR_ONLY)) {
					chan->ic_flags |=
					    IEEE80211_CHAN_PASSIVE;
				}
			}
			chnum += 4;
			freq = ieee80211_ieee2mhz(chnum, IEEE80211_CHAN_5GHZ);
		}
	}
}

int
qwx_reg_chan_list_event(struct qwx_softc *sc, struct mbuf *m,
    enum wmi_reg_chan_list_cmd_type id)
{
	struct cur_regulatory_info *reg_info = NULL;
	int ret = 0;
#if 0
	struct ieee80211_regdomain *regd = NULL;
	bool intersect = false;
	int pdev_idx, i, j;
	struct ath11k *ar;
#endif

	reg_info = malloc(sizeof(*reg_info), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!reg_info) {
		ret = ENOMEM;
		goto fallback;
	}

	if (id == WMI_REG_CHAN_LIST_CC_ID)
		ret = qwx_pull_reg_chan_list_update_ev(sc, m, reg_info);
	else
		ret = qwx_pull_reg_chan_list_ext_update_ev(sc, m, reg_info);

	if (ret) {
		printf("%s: failed to extract regulatory info from "
		    "received event\n", sc->sc_dev.dv_xname);
		goto fallback;
	}

	DNPRINTF(QWX_D_WMI, "%s: event reg chan list id %d\n", __func__, id);

	if (reg_info->status_code != REG_SET_CC_STATUS_PASS) {
		/* In case of failure to set the requested ctry,
		 * fw retains the current regd. We print a failure info
		 * and return from here.
		 */
		printf("%s: Failed to set the requested Country "
		    "regulatory setting\n", __func__);
		goto mem_free;
	}

	qwx_init_channels(sc, reg_info);
#if 0
	pdev_idx = reg_info->phy_id;

	/* Avoid default reg rule updates sent during FW recovery if
	 * it is already available
	 */
	spin_lock(&ab->base_lock);
	if (test_bit(ATH11K_FLAG_RECOVERY, &ab->dev_flags) &&
	    ab->default_regd[pdev_idx]) {
		spin_unlock(&ab->base_lock);
		goto mem_free;
	}
	spin_unlock(&ab->base_lock);

	if (pdev_idx >= ab->num_radios) {
		/* Process the event for phy0 only if single_pdev_only
		 * is true. If pdev_idx is valid but not 0, discard the
		 * event. Otherwise, it goes to fallback.
		 */
		if (ab->hw_params.single_pdev_only &&
		    pdev_idx < ab->hw_params.num_rxmda_per_pdev)
			goto mem_free;
		else
			goto fallback;
	}

	/* Avoid multiple overwrites to default regd, during core
	 * stop-start after mac registration.
	 */
	if (ab->default_regd[pdev_idx] && !ab->new_regd[pdev_idx] &&
	    !memcmp((char *)ab->default_regd[pdev_idx]->alpha2,
		    (char *)reg_info->alpha2, 2))
		goto mem_free;

	/* Intersect new rules with default regd if a new country setting was
	 * requested, i.e a default regd was already set during initialization
	 * and the regd coming from this event has a valid country info.
	 */
	if (ab->default_regd[pdev_idx] &&
	    !ath11k_reg_is_world_alpha((char *)
		ab->default_regd[pdev_idx]->alpha2) &&
	    !ath11k_reg_is_world_alpha((char *)reg_info->alpha2))
		intersect = true;

	regd = ath11k_reg_build_regd(ab, reg_info, intersect);
	if (!regd) {
		ath11k_warn(ab, "failed to build regd from reg_info\n");
		goto fallback;
	}

	spin_lock(&ab->base_lock);
	if (ab->default_regd[pdev_idx]) {
		/* The initial rules from FW after WMI Init is to build
		 * the default regd. From then on, any rules updated for
		 * the pdev could be due to user reg changes.
		 * Free previously built regd before assigning the newly
		 * generated regd to ar. NULL pointer handling will be
		 * taken care by kfree itself.
		 */
		ar = ab->pdevs[pdev_idx].ar;
		kfree(ab->new_regd[pdev_idx]);
		ab->new_regd[pdev_idx] = regd;
		queue_work(ab->workqueue, &ar->regd_update_work);
	} else {
		/* This regd would be applied during mac registration and is
		 * held constant throughout for regd intersection purpose
		 */
		ab->default_regd[pdev_idx] = regd;
	}
	ab->dfs_region = reg_info->dfs_region;
	spin_unlock(&ab->base_lock);
#endif
	goto mem_free;

fallback:
	/* Fallback to older reg (by sending previous country setting
	 * again if fw has succeeded and we failed to process here.
	 * The Regdomain should be uniform across driver and fw. Since the
	 * FW has processed the command and sent a success status, we expect
	 * this function to succeed as well. If it doesn't, CTRY needs to be
	 * reverted at the fw and the old SCAN_CHAN_LIST cmd needs to be sent.
	 */
	/* TODO: This is rare, but still should also be handled */
mem_free:
	if (reg_info) {
		free(reg_info->reg_rules_2ghz_ptr, M_DEVBUF,
		    reg_info->num_2ghz_reg_rules *
		    sizeof(*reg_info->reg_rules_2ghz_ptr));
		free(reg_info->reg_rules_5ghz_ptr, M_DEVBUF,
		    reg_info->num_5ghz_reg_rules *
		    sizeof(*reg_info->reg_rules_5ghz_ptr));
#if 0
		if (reg_info->is_ext_reg_event) {
			for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++)
				kfree(reg_info->reg_rules_6ghz_ap_ptr[i]);

			for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++)
				for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++)
					kfree(reg_info->reg_rules_6ghz_client_ptr[j][i]);
		}
#endif
		free(reg_info, M_DEVBUF, sizeof(*reg_info));
	}
	return ret;
}

const char *
qwx_wmi_event_scan_type_str(enum wmi_scan_event_type type,
    enum wmi_scan_completion_reason reason)
{
	switch (type) {
	case WMI_SCAN_EVENT_STARTED:
		return "started";
	case WMI_SCAN_EVENT_COMPLETED:
		switch (reason) {
		case WMI_SCAN_REASON_COMPLETED:
			return "completed";
		case WMI_SCAN_REASON_CANCELLED:
			return "completed [cancelled]";
		case WMI_SCAN_REASON_PREEMPTED:
			return "completed [preempted]";
		case WMI_SCAN_REASON_TIMEDOUT:
			return "completed [timedout]";
		case WMI_SCAN_REASON_INTERNAL_FAILURE:
			return "completed [internal err]";
		case WMI_SCAN_REASON_MAX:
			break;
		}
		return "completed [unknown]";
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		return "bss channel";
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		return "foreign channel";
	case WMI_SCAN_EVENT_DEQUEUED:
		return "dequeued";
	case WMI_SCAN_EVENT_PREEMPTED:
		return "preempted";
	case WMI_SCAN_EVENT_START_FAILED:
		return "start failed";
	case WMI_SCAN_EVENT_RESTARTED:
		return "restarted";
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
		return "foreign channel exit";
	default:
		return "unknown";
	}
}

const char *
qwx_scan_state_str(enum ath11k_scan_state state)
{
	switch (state) {
	case ATH11K_SCAN_IDLE:
		return "idle";
	case ATH11K_SCAN_STARTING:
		return "starting";
	case ATH11K_SCAN_RUNNING:
		return "running";
	case ATH11K_SCAN_ABORTING:
		return "aborting";
	}

	return "unknown";
}

int
qwx_pull_scan_ev(struct qwx_softc *sc, struct mbuf *m,
    struct wmi_scan_event *scan_evt_param)
{
	const void **tb;
	const struct wmi_scan_event *ev;

	tb = qwx_wmi_tlv_parse_alloc(sc, mtod(m, void *), m->m_pkthdr.len);
	if (tb == NULL) {
		DPRINTF("%s: failed to parse tlv\n", __func__);
		return EINVAL;
	}

	ev = tb[WMI_TAG_SCAN_EVENT];
	if (!ev) {
		DPRINTF("%s: failed to fetch scan ev\n", __func__);
		free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
		return EPROTO;
	}

	scan_evt_param->event_type = ev->event_type;
	scan_evt_param->reason = ev->reason;
	scan_evt_param->channel_freq = ev->channel_freq;
	scan_evt_param->scan_req_id = ev->scan_req_id;
	scan_evt_param->scan_id = ev->scan_id;
	scan_evt_param->vdev_id = ev->vdev_id;
	scan_evt_param->tsf_timestamp = ev->tsf_timestamp;

	free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
	return 0;
}

void
qwx_wmi_event_scan_started(struct qwx_softc *sc)
{
#ifdef notyet
	lockdep_assert_held(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		printf("%s: received scan started event in an invalid "
		"scan state: %s (%d)\n", sc->sc_dev.dv_xname,
		qwx_scan_state_str(sc->scan.state), sc->scan.state);
		break;
	case ATH11K_SCAN_STARTING:
		sc->scan.state = ATH11K_SCAN_RUNNING;
#if 0
		if (ar->scan.is_roc)
			ieee80211_ready_on_channel(ar->hw);
#endif
		wakeup(&sc->scan.state);
		break;
	}
}

void
qwx_wmi_event_scan_completed(struct qwx_softc *sc)
{
#ifdef notyet
	lockdep_assert_held(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		/* One suspected reason scan can be completed while starting is
		 * if firmware fails to deliver all scan events to the host,
		 * e.g. when transport pipe is full. This has been observed
		 * with spectral scan phyerr events starving wmi transport
		 * pipe. In such case the "scan completed" event should be (and
		 * is) ignored by the host as it may be just firmware's scan
		 * state machine recovering.
		 */
		printf("%s: received scan completed event in an invalid "
		    "scan state: %s (%d)\n", sc->sc_dev.dv_xname,
		    qwx_scan_state_str(sc->scan.state), sc->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		qwx_mac_scan_finish(sc);
		break;
	}
}

void
qwx_wmi_event_scan_bss_chan(struct qwx_softc *sc)
{
#ifdef notyet
	lockdep_assert_held(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		printf("%s: received scan bss chan event in an invalid "
		    "scan state: %s (%d)\n", sc->sc_dev.dv_xname,
		    qwx_scan_state_str(sc->scan.state), sc->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		sc->scan_channel = 0;
		break;
	}
}

void
qwx_wmi_event_scan_foreign_chan(struct qwx_softc *sc, uint32_t freq)
{
#ifdef notyet
	lockdep_assert_held(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		printf("%s: received scan foreign chan event in an invalid "
		    "scan state: %s (%d)\n", sc->sc_dev.dv_xname,
		    qwx_scan_state_str(sc->scan.state), sc->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		sc->scan_channel = ieee80211_mhz2ieee(freq, 0);
#if 0
		if (ar->scan.is_roc && ar->scan.roc_freq == freq)
			complete(&ar->scan.on_channel);
#endif
		break;
	}
}

void
qwx_wmi_event_scan_start_failed(struct qwx_softc *sc)
{
#ifdef notyet
	lockdep_assert_held(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		printf("%s: received scan start failed event in an invalid "
		    "scan state: %s (%d)\n", sc->sc_dev.dv_xname,
		    qwx_scan_state_str(sc->scan.state), sc->scan.state);
		break;
	case ATH11K_SCAN_STARTING:
		wakeup(&sc->scan.state);
		qwx_mac_scan_finish(sc);
		break;
	}
}


void
qwx_scan_event(struct qwx_softc *sc, struct mbuf *m)
{
	struct wmi_scan_event scan_ev = { 0 };
	struct qwx_vif *arvif;

	if (qwx_pull_scan_ev(sc, m, &scan_ev) != 0) {
		printf("%s: failed to extract scan event",
		    sc->sc_dev.dv_xname);
		return;
	}
#ifdef notyet
	rcu_read_lock();
#endif
	TAILQ_FOREACH(arvif, &sc->vif_list, entry) {
		if (arvif->vdev_id == scan_ev.vdev_id)
			break;
	}

	if (!arvif) {
		printf("%s: received scan event for unknown vdev\n",
		    sc->sc_dev.dv_xname);
#if 0
		rcu_read_unlock();
#endif
		return;
	}
#if 0
	spin_lock_bh(&ar->data_lock);
#endif
	DNPRINTF(QWX_D_WMI,
	    "%s: event scan %s type %d reason %d freq %d req_id %d scan_id %d "
	    "vdev_id %d state %s (%d)\n", __func__,
	    qwx_wmi_event_scan_type_str(scan_ev.event_type, scan_ev.reason),
	    scan_ev.event_type, scan_ev.reason, scan_ev.channel_freq,
	    scan_ev.scan_req_id, scan_ev.scan_id, scan_ev.vdev_id,
	    qwx_scan_state_str(sc->scan.state), sc->scan.state);

	switch (scan_ev.event_type) {
	case WMI_SCAN_EVENT_STARTED:
		qwx_wmi_event_scan_started(sc);
		break;
	case WMI_SCAN_EVENT_COMPLETED:
		qwx_wmi_event_scan_completed(sc);
		break;
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		qwx_wmi_event_scan_bss_chan(sc);
		break;
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		qwx_wmi_event_scan_foreign_chan(sc, scan_ev.channel_freq);
		break;
	case WMI_SCAN_EVENT_START_FAILED:
		printf("%s: received scan start failure event\n",
		    sc->sc_dev.dv_xname);
		qwx_wmi_event_scan_start_failed(sc);
		break;
	case WMI_SCAN_EVENT_DEQUEUED:
		qwx_mac_scan_finish(sc);
		break;
	case WMI_SCAN_EVENT_PREEMPTED:
	case WMI_SCAN_EVENT_RESTARTED:
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
	default:
		break;
	}
#if 0
	spin_unlock_bh(&ar->data_lock);

	rcu_read_unlock();
#endif
}

int
qwx_pull_chan_info_ev(struct qwx_softc *sc, uint8_t *evt_buf, uint32_t len,
    struct wmi_chan_info_event *ch_info_ev)
{
	const void **tb;
	const struct wmi_chan_info_event *ev;

	tb = qwx_wmi_tlv_parse_alloc(sc, evt_buf, len);
	if (tb == NULL) {
		printf("%s: failed to parse tlv\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}

	ev = tb[WMI_TAG_CHAN_INFO_EVENT];
	if (!ev) {
		printf("%s: failed to fetch chan info ev\n",
		    sc->sc_dev.dv_xname);
		free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
		return EPROTO;
	}

	ch_info_ev->err_code = ev->err_code;
	ch_info_ev->freq = ev->freq;
	ch_info_ev->cmd_flags = ev->cmd_flags;
	ch_info_ev->noise_floor = ev->noise_floor;
	ch_info_ev->rx_clear_count = ev->rx_clear_count;
	ch_info_ev->cycle_count = ev->cycle_count;
	ch_info_ev->chan_tx_pwr_range = ev->chan_tx_pwr_range;
	ch_info_ev->chan_tx_pwr_tp = ev->chan_tx_pwr_tp;
	ch_info_ev->rx_frame_count = ev->rx_frame_count;
	ch_info_ev->tx_frame_cnt = ev->tx_frame_cnt;
	ch_info_ev->mac_clk_mhz = ev->mac_clk_mhz;
	ch_info_ev->vdev_id = ev->vdev_id;

	free(tb, M_DEVBUF, WMI_TAG_MAX * sizeof(*tb));
	return 0;
}

void
qwx_chan_info_event(struct qwx_softc *sc, struct mbuf *m)
{
	struct qwx_vif *arvif;
	struct wmi_chan_info_event ch_info_ev = {0};
	struct qwx_survey_info *survey;
	int idx;
	/* HW channel counters frequency value in hertz */
	uint32_t cc_freq_hz = sc->cc_freq_hz;

	if (qwx_pull_chan_info_ev(sc, mtod(m, void *), m->m_pkthdr.len,
	    &ch_info_ev) != 0) {
		printf("%s: failed to extract chan info event\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	DNPRINTF(QWX_D_WMI, "%s: event chan info vdev_id %d err_code %d "
	    "freq %d cmd_flags %d noise_floor %d rx_clear_count %d "
	    "cycle_count %d mac_clk_mhz %d\n", __func__,
	    ch_info_ev.vdev_id, ch_info_ev.err_code, ch_info_ev.freq,
	    ch_info_ev.cmd_flags, ch_info_ev.noise_floor,
	    ch_info_ev.rx_clear_count, ch_info_ev.cycle_count,
	    ch_info_ev.mac_clk_mhz);

	if (ch_info_ev.cmd_flags == WMI_CHAN_INFO_END_RESP) {
		DNPRINTF(QWX_D_WMI, "chan info report completed\n");
		return;
	}
#ifdef notyet
	rcu_read_lock();
#endif
	TAILQ_FOREACH(arvif, &sc->vif_list, entry) {
		if (arvif->vdev_id == ch_info_ev.vdev_id)
			break;
	}
	if (!arvif) {
		printf("%s: invalid vdev id in chan info ev %d\n",
		   sc->sc_dev.dv_xname, ch_info_ev.vdev_id);
#ifdef notyet
		rcu_read_unlock();
#endif
		return;
	}
#ifdef notyet
	spin_lock_bh(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		printf("%s: received chan info event without a scan request, "
		    "ignoring\n", sc->sc_dev.dv_xname);
		goto exit;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		break;
	}

	idx = ieee80211_mhz2ieee(ch_info_ev.freq, 0);
	if (idx >= nitems(sc->survey)) {
		printf("%s: invalid frequency %d (idx %d out of bounds)\n",
		    sc->sc_dev.dv_xname, ch_info_ev.freq, idx);
		goto exit;
	}

	/* If FW provides MAC clock frequency in Mhz, overriding the initialized
	 * HW channel counters frequency value
	 */
	if (ch_info_ev.mac_clk_mhz)
		cc_freq_hz = (ch_info_ev.mac_clk_mhz * 1000);

	if (ch_info_ev.cmd_flags == WMI_CHAN_INFO_START_RESP) {
		survey = &sc->survey[idx];
		memset(survey, 0, sizeof(*survey));
		survey->noise = ch_info_ev.noise_floor;
		survey->time = ch_info_ev.cycle_count / cc_freq_hz;
		survey->time_busy = ch_info_ev.rx_clear_count / cc_freq_hz;
	}
exit:
#ifdef notyet
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();
#else
	return;
#endif
}

int
qwx_wmi_tlv_mgmt_rx_parse(struct qwx_softc *sc, uint16_t tag, uint16_t len,
    const void *ptr, void *data)
{
	struct wmi_tlv_mgmt_rx_parse *parse = data;

	switch (tag) {
	case WMI_TAG_MGMT_RX_HDR:
		parse->fixed = ptr;
		break;
	case WMI_TAG_ARRAY_BYTE:
		if (!parse->frame_buf_done) {
			parse->frame_buf = ptr;
			parse->frame_buf_done = 1;
		}
		break;
	}
	return 0;
}

int
qwx_pull_mgmt_rx_params_tlv(struct qwx_softc *sc, struct mbuf *m,
    struct mgmt_rx_event_params *hdr)
{
	struct wmi_tlv_mgmt_rx_parse parse = { 0 };
	const struct wmi_mgmt_rx_hdr *ev;
	const uint8_t *frame;
	int ret;
	size_t totlen, hdrlen;

	ret = qwx_wmi_tlv_iter(sc, mtod(m, void *), m->m_pkthdr.len,
	    qwx_wmi_tlv_mgmt_rx_parse, &parse);
	if (ret) {
		printf("%s: failed to parse mgmt rx tlv %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ev = parse.fixed;
	frame = parse.frame_buf;

	if (!ev || !frame) {
		printf("%s: failed to fetch mgmt rx hdr\n",
		    sc->sc_dev.dv_xname);
		return EPROTO;
	}

	hdr->pdev_id =  ev->pdev_id;
	hdr->chan_freq = le32toh(ev->chan_freq);
	hdr->channel = le32toh(ev->channel);
	hdr->snr = le32toh(ev->snr);
	hdr->rate = le32toh(ev->rate);
	hdr->phy_mode = le32toh(ev->phy_mode);
	hdr->buf_len = le32toh(ev->buf_len);
	hdr->status = le32toh(ev->status);
	hdr->flags = le32toh(ev->flags);
	hdr->rssi = le32toh(ev->rssi);
	hdr->tsf_delta = le32toh(ev->tsf_delta);
	memcpy(hdr->rssi_ctl, ev->rssi_ctl, sizeof(hdr->rssi_ctl));

	if (frame < mtod(m, uint8_t *) ||
	    frame >= mtod(m, uint8_t *) + m->m_pkthdr.len) {
		printf("%s: invalid mgmt rx frame pointer\n",
		    sc->sc_dev.dv_xname);
		return EPROTO;
	}
	hdrlen = frame - mtod(m, uint8_t *);

	if (hdrlen + hdr->buf_len < hdr->buf_len) {
		printf("%s: length overflow in mgmt rx hdr ev\n",
		    sc->sc_dev.dv_xname);
		return EPROTO;
	}
	totlen = hdrlen + hdr->buf_len;
	if (m->m_pkthdr.len < totlen) {
		printf("%s: invalid length in mgmt rx hdr ev\n",
		    sc->sc_dev.dv_xname);
		return EPROTO;
	}

	/* shift the mbuf to point at `frame` */
	m->m_len = m->m_pkthdr.len = totlen;
	m_adj(m, hdrlen);

#if 0 /* Not needed on OpenBSD? */
	ath11k_ce_byte_swap(skb->data, hdr->buf_len);
#endif
	return 0;
}

void
qwx_mgmt_rx_event(struct qwx_softc *sc, struct mbuf *m)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct mgmt_rx_event_params rx_ev = {0};
	struct ieee80211_rxinfo rxi;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;

	if (qwx_pull_mgmt_rx_params_tlv(sc, m, &rx_ev) != 0) {
		printf("%s: failed to extract mgmt rx event\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return;
	}

	memset(&rxi, 0, sizeof(rxi));

	DNPRINTF(QWX_D_MGMT, "%s: event mgmt rx status %08x\n", __func__,
	    rx_ev.status);
#ifdef notyet
	rcu_read_lock();
#endif
	if (rx_ev.pdev_id >= nitems(sc->pdevs)) {
		printf("%s: invalid pdev_id %d in mgmt_rx_event\n",
		    sc->sc_dev.dv_xname, rx_ev.pdev_id);
		m_freem(m);
		goto exit;
	}

	if ((test_bit(ATH11K_CAC_RUNNING, sc->sc_flags)) ||
	    (rx_ev.status & (WMI_RX_STATUS_ERR_DECRYPT |
	    WMI_RX_STATUS_ERR_KEY_CACHE_MISS | WMI_RX_STATUS_ERR_CRC))) {
		m_freem(m);
		goto exit;
	}

	if (rx_ev.status & WMI_RX_STATUS_ERR_MIC) {
		ic->ic_stats.is_ccmp_dec_errs++;
		m_freem(m);
		goto exit;
	}

	rxi.rxi_chan = rx_ev.channel;
	rxi.rxi_rssi = rx_ev.snr + ATH11K_DEFAULT_NOISE_FLOOR;
#if 0
	status->rate_idx = ath11k_mac_bitrate_to_idx(sband, rx_ev.rate / 100);
#endif

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);
#if 0
	/* In case of PMF, FW delivers decrypted frames with Protected Bit set.
	 * Don't clear that. Also, FW delivers broadcast management frames
	 * (ex: group privacy action frames in mesh) as encrypted payload.
	 */
	if (ieee80211_has_protected(hdr->frame_control) &&
	    !is_multicast_ether_addr(ieee80211_get_DA(hdr))) {
		status->flag |= RX_FLAG_DECRYPTED;

		if (!ieee80211_is_robust_mgmt_frame(skb)) {
			status->flag |= RX_FLAG_IV_STRIPPED |
					RX_FLAG_MMIC_STRIPPED;
			hdr->frame_control = __cpu_to_le16(fc &
					     ~IEEE80211_FCTL_PROTECTED);
		}
	}

	if (ieee80211_is_beacon(hdr->frame_control))
		ath11k_mac_handle_beacon(ar, skb);
#endif

	DNPRINTF(QWX_D_MGMT,
	    "%s: event mgmt rx skb %p len %d ftype %02x stype %02x\n",
	    __func__, m, m->m_pkthdr.len,
	    wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK,
	    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);

	DNPRINTF(QWX_D_MGMT, "%s: event mgmt rx freq %d chan %d snr %d\n",
	    __func__, rx_ev.chan_freq, rx_ev.channel, rx_ev.snr);

	ieee80211_input(ifp, m, ni, &rxi);
exit:
#ifdef notyet
	rcu_read_unlock();
#else
	return;
#endif
}

void
qwx_wmi_tlv_op_rx(struct qwx_softc *sc, struct mbuf *m)
{
	struct wmi_cmd_hdr *cmd_hdr;
	enum wmi_tlv_event_id id;

	cmd_hdr = mtod(m, struct wmi_cmd_hdr *);
	id = FIELD_GET(WMI_CMD_HDR_CMD_ID, (cmd_hdr->cmd_id));

	m_adj(m, sizeof(struct wmi_cmd_hdr));

	switch (id) {
		/* Process all the WMI events here */
	case WMI_SERVICE_READY_EVENTID:
		qwx_service_ready_event(sc, m);
		break;
	case WMI_SERVICE_READY_EXT_EVENTID:
		qwx_service_ready_ext_event(sc, m);
		break;
	case WMI_SERVICE_READY_EXT2_EVENTID:
		qwx_service_ready_ext2_event(sc, m);
		break;
	case WMI_REG_CHAN_LIST_CC_EVENTID:
		qwx_reg_chan_list_event(sc, m, WMI_REG_CHAN_LIST_CC_ID);
		break;
	case WMI_REG_CHAN_LIST_CC_EXT_EVENTID:
		qwx_reg_chan_list_event(sc, m, WMI_REG_CHAN_LIST_CC_EXT_ID);
		break;
	case WMI_READY_EVENTID:
		qwx_ready_event(sc, m);
		break;
#if 0
	case WMI_PEER_DELETE_RESP_EVENTID:
		ath11k_peer_delete_resp_event(ab, skb);
		break;
	case WMI_VDEV_START_RESP_EVENTID:
		ath11k_vdev_start_resp_event(ab, skb);
		break;
	case WMI_OFFLOAD_BCN_TX_STATUS_EVENTID:
		ath11k_bcn_tx_status_event(ab, skb);
		break;
	case WMI_VDEV_STOPPED_EVENTID:
		ath11k_vdev_stopped_event(ab, skb);
		break;
#endif
	case WMI_MGMT_RX_EVENTID:
		DPRINTF("%s: 0x%x: mgmt rx event\n", __func__, id);
#if 0
		qwx_mgmt_rx_event(sc, m);
		/* mgmt_rx_event() owns the skb now! */
#else
		m_freem(m);
#endif
		return;
#if 0
	case WMI_MGMT_TX_COMPLETION_EVENTID:
		ath11k_mgmt_tx_compl_event(ab, skb);
		break;
#endif
	case WMI_SCAN_EVENTID:
		DPRINTF("%s: 0x%x: scan event\n", __func__, id);
		qwx_scan_event(sc, m);
		break;
#if 0
	case WMI_PEER_STA_KICKOUT_EVENTID:
		ath11k_peer_sta_kickout_event(ab, skb);
		break;
	case WMI_ROAM_EVENTID:
		ath11k_roam_event(ab, skb);
		break;
#endif
	case WMI_CHAN_INFO_EVENTID:
		DPRINTF("%s: 0x%x: chan info event\n", __func__, id);
		qwx_chan_info_event(sc, m);
		break;
#if 0
	case WMI_PDEV_BSS_CHAN_INFO_EVENTID:
		ath11k_pdev_bss_chan_info_event(ab, skb);
		break;
	case WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID:
		ath11k_vdev_install_key_compl_event(ab, skb);
		break;
#endif
	case WMI_SERVICE_AVAILABLE_EVENTID:
		qwx_service_available_event(sc, m);
		break;
#if 0
	case WMI_PEER_ASSOC_CONF_EVENTID:
		ath11k_peer_assoc_conf_event(ab, skb);
		break;
	case WMI_UPDATE_STATS_EVENTID:
		ath11k_update_stats_event(ab, skb);
		break;
	case WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID:
		ath11k_pdev_ctl_failsafe_check_event(ab, skb);
		break;
	case WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID:
		ath11k_wmi_pdev_csa_switch_count_status_event(ab, skb);
		break;
	case WMI_PDEV_UTF_EVENTID:
		ath11k_tm_wmi_event(ab, id, skb);
		break;
	case WMI_PDEV_TEMPERATURE_EVENTID:
		ath11k_wmi_pdev_temperature_event(ab, skb);
		break;
	case WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID:
		ath11k_wmi_pdev_dma_ring_buf_release_event(ab, skb);
		break;
	case WMI_HOST_FILS_DISCOVERY_EVENTID:
		ath11k_fils_discovery_event(ab, skb);
		break;
	case WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID:
		ath11k_probe_resp_tx_status_event(ab, skb);
		break;
	case WMI_OBSS_COLOR_COLLISION_DETECTION_EVENTID:
		ath11k_wmi_obss_color_collision_event(ab, skb);
		break;
	case WMI_TWT_ADD_DIALOG_EVENTID:
		ath11k_wmi_twt_add_dialog_event(ab, skb);
		break;
	case WMI_PDEV_DFS_RADAR_DETECTION_EVENTID:
		ath11k_wmi_pdev_dfs_radar_detected_event(ab, skb);
		break;
	case WMI_VDEV_DELETE_RESP_EVENTID:
		ath11k_vdev_delete_resp_event(ab, skb);
		break;
	case WMI_WOW_WAKEUP_HOST_EVENTID:
		ath11k_wmi_event_wow_wakeup_host(ab, skb);
		break;
	case WMI_11D_NEW_COUNTRY_EVENTID:
		ath11k_reg_11d_new_cc_event(ab, skb);
		break;
#endif
	case WMI_DIAG_EVENTID:
		/* Ignore. These events trigger tracepoints in Linux. */
		break;
#if 0
	case WMI_PEER_STA_PS_STATECHG_EVENTID:
		ath11k_wmi_event_peer_sta_ps_state_chg(ab, skb);
		break;
	case WMI_GTK_OFFLOAD_STATUS_EVENTID:
		ath11k_wmi_gtk_offload_status_event(ab, skb);
		break;
#endif
	case WMI_UPDATE_FW_MEM_DUMP_EVENTID:
		DPRINTF("%s: 0x%x: update fw mem dump\n", __func__, id);
		break;
	case WMI_PDEV_SET_HW_MODE_RESP_EVENTID:
		DPRINTF("%s: 0x%x: set HW mode response event\n", __func__, id);
		break;
	case WMI_WLAN_FREQ_AVOID_EVENTID:
		DPRINTF("%s: 0x%x: wlan freq avoid event\n", __func__, id);
		break;
	default:
		printf("%s: unsupported event id 0x%x\n", __func__, id);
		break;
	}

	m_freem(m);
}

void
qwx_wmi_op_ep_tx_credits(struct qwx_softc *sc)
{
	struct qwx_htc *htc = &sc->htc;
	int i;

	/* try to send pending beacons first. they take priority */
	sc->wmi.tx_credits = 1;
	wakeup(&sc->wmi.tx_credits);

	if (!sc->hw_params.credit_flow)
		return;

	for (i = ATH11K_HTC_EP_0; i < ATH11K_HTC_EP_COUNT; i++) {
		struct qwx_htc_ep *ep = &htc->endpoint[i];
		if (ep->tx_credit_flow_enabled && ep->tx_credits > 0)
			wakeup(&ep->tx_credits);
	}
}

int
qwx_connect_pdev_htc_service(struct qwx_softc *sc, uint32_t pdev_idx)
{
	int status;
	uint32_t svc_id[] = { ATH11K_HTC_SVC_ID_WMI_CONTROL,
	    ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1,
	    ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC2 };
	struct qwx_htc_svc_conn_req conn_req;
	struct qwx_htc_svc_conn_resp conn_resp;

	memset(&conn_req, 0, sizeof(conn_req));
	memset(&conn_resp, 0, sizeof(conn_resp));

	/* these fields are the same for all service endpoints */
	conn_req.ep_ops.ep_tx_complete = qwx_wmi_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = qwx_wmi_tlv_op_rx;
	conn_req.ep_ops.ep_tx_credits = qwx_wmi_op_ep_tx_credits;

	/* connect to control service */
	conn_req.service_id = svc_id[pdev_idx];

	status = qwx_htc_connect_service(&sc->htc, &conn_req, &conn_resp);
	if (status) {
		printf("%s: failed to connect to WMI CONTROL service "
		    "status: %d\n", sc->sc_dev.dv_xname, status);
		return status;
	}

	sc->wmi.wmi_endpoint_id[pdev_idx] = conn_resp.eid;
	sc->wmi.wmi[pdev_idx].eid = conn_resp.eid;
	sc->wmi.max_msg_len[pdev_idx] = conn_resp.max_msg_len;
	sc->wmi.wmi[pdev_idx].tx_ce_desc = 0;

	return 0;
}

int
qwx_wmi_connect(struct qwx_softc *sc)
{
	uint32_t i;
	uint8_t wmi_ep_count;

	wmi_ep_count = sc->htc.wmi_ep_count;
	if (wmi_ep_count > sc->hw_params.max_radios)
		return -1;

	for (i = 0; i < wmi_ep_count; i++)
		qwx_connect_pdev_htc_service(sc, i);

	return 0;
}

void
qwx_htc_reset_endpoint_states(struct qwx_htc *htc)
{
	struct qwx_htc_ep *ep;
	int i;

	for (i = ATH11K_HTC_EP_0; i < ATH11K_HTC_EP_COUNT; i++) {
		ep = &htc->endpoint[i];
		ep->service_id = ATH11K_HTC_SVC_ID_UNUSED;
		ep->max_ep_message_len = 0;
		ep->max_tx_queue_depth = 0;
		ep->eid = i;
		ep->htc = htc;
		ep->tx_credit_flow_enabled = 1;
	}
}

void
qwx_htc_control_tx_complete(struct qwx_softc *sc, struct mbuf *m)
{
	printf("%s: not implemented\n", __func__);

	m_freem(m);
}

void
qwx_htc_control_rx_complete(struct qwx_softc *sc, struct mbuf *m)
{
	printf("%s: not implemented\n", __func__);

	m_freem(m);
}

uint8_t
qwx_htc_get_credit_allocation(struct qwx_htc *htc, uint16_t service_id)
{
	uint8_t i, allocation = 0;

	for (i = 0; i < ATH11K_HTC_MAX_SERVICE_ALLOC_ENTRIES; i++) {
		if (htc->service_alloc_table[i].service_id == service_id) {
			allocation =
			    htc->service_alloc_table[i].credit_allocation;
		}
	}

	return allocation;
}

const char *
qwx_htc_service_name(enum ath11k_htc_svc_id id)
{
	switch (id) {
	case ATH11K_HTC_SVC_ID_RESERVED:
		return "Reserved";
	case ATH11K_HTC_SVC_ID_RSVD_CTRL:
		return "Control";
	case ATH11K_HTC_SVC_ID_WMI_CONTROL:
		return "WMI";
	case ATH11K_HTC_SVC_ID_WMI_DATA_BE:
		return "DATA BE";
	case ATH11K_HTC_SVC_ID_WMI_DATA_BK:
		return "DATA BK";
	case ATH11K_HTC_SVC_ID_WMI_DATA_VI:
		return "DATA VI";
	case ATH11K_HTC_SVC_ID_WMI_DATA_VO:
		return "DATA VO";
	case ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1:
		return "WMI MAC1";
	case ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC2:
		return "WMI MAC2";
	case ATH11K_HTC_SVC_ID_NMI_CONTROL:
		return "NMI Control";
	case ATH11K_HTC_SVC_ID_NMI_DATA:
		return "NMI Data";
	case ATH11K_HTC_SVC_ID_HTT_DATA_MSG:
		return "HTT Data";
	case ATH11K_HTC_SVC_ID_TEST_RAW_STREAMS:
		return "RAW";
	case ATH11K_HTC_SVC_ID_IPA_TX:
		return "IPA TX";
	case ATH11K_HTC_SVC_ID_PKT_LOG:
		return "PKT LOG";
	}

	return "Unknown";
}

struct mbuf *
qwx_htc_alloc_mbuf(size_t payload_size)
{
	struct mbuf *m;
	size_t size = sizeof(struct ath11k_htc_hdr) + payload_size;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;

	if (size <= MCLBYTES)
		MCLGET(m, M_DONTWAIT);
	else
		MCLGETL(m, M_DONTWAIT, size);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return NULL;
	}

	m->m_len = m->m_pkthdr.len = size;
	memset(mtod(m, void *), 0, size);

	return m;
}

struct mbuf *
qwx_htc_build_tx_ctrl_mbuf(void)
{
	size_t size;

	size = ATH11K_HTC_CONTROL_BUFFER_SIZE - sizeof(struct ath11k_htc_hdr);

	return qwx_htc_alloc_mbuf(size);
}

void
qwx_htc_prepare_tx_mbuf(struct qwx_htc_ep *ep, struct mbuf *m)
{
	struct ath11k_htc_hdr *hdr;

	hdr = mtod(m, struct ath11k_htc_hdr *);

	memset(hdr, 0, sizeof(*hdr));
	hdr->htc_info = FIELD_PREP(HTC_HDR_ENDPOINTID, ep->eid) |
	    FIELD_PREP(HTC_HDR_PAYLOADLEN, (m->m_pkthdr.len - sizeof(*hdr)));

	if (ep->tx_credit_flow_enabled)
		hdr->htc_info |= FIELD_PREP(HTC_HDR_FLAGS,
		    ATH11K_HTC_FLAG_NEED_CREDIT_UPDATE);
#ifdef notyet
	spin_lock_bh(&ep->htc->tx_lock);
#endif
	hdr->ctrl_info = FIELD_PREP(HTC_HDR_CONTROLBYTES1, ep->seq_no++);
#ifdef notyet
	spin_unlock_bh(&ep->htc->tx_lock);
#endif
}

int
qwx_htc_send(struct qwx_htc *htc, enum ath11k_htc_ep_id eid, struct mbuf *m)
{
	struct qwx_htc_ep *ep = &htc->endpoint[eid];
	struct qwx_softc *sc = htc->sc;
	struct qwx_ce_pipe *pipe = &sc->ce.ce_pipe[ep->ul_pipe_id];
	void *ctx;
	struct qwx_tx_data *tx_data;
	int credits = 0;
	int ret;
	int credit_flow_enabled = (sc->hw_params.credit_flow &&
	    ep->tx_credit_flow_enabled);

	if (eid >= ATH11K_HTC_EP_COUNT) {
		printf("%s: Invalid endpoint id: %d\n", __func__, eid);
		return ENOENT;
	}

	if (credit_flow_enabled) {
		credits = howmany(m->m_pkthdr.len, htc->target_credit_size);
#ifdef notyet
		spin_lock_bh(&htc->tx_lock);
#endif
		if (ep->tx_credits < credits) {
			DNPRINTF(QWX_D_HTC,
			    "%s: ep %d insufficient credits required %d "
			    "total %d\n", __func__, eid, credits,
			    ep->tx_credits);
#ifdef notyet
			spin_unlock_bh(&htc->tx_lock);
#endif
			return EAGAIN;
		}
		ep->tx_credits -= credits;
		DNPRINTF(QWX_D_HTC, "%s: ep %d credits consumed %d total %d\n",
		    __func__, eid, credits, ep->tx_credits);
#ifdef notyet
		spin_unlock_bh(&htc->tx_lock);
#endif
	}

	qwx_htc_prepare_tx_mbuf(ep, m);

	ctx = pipe->src_ring->per_transfer_context[pipe->src_ring->write_index];
	tx_data = (struct qwx_tx_data *)ctx;

	tx_data->eid = eid;
	ret = bus_dmamap_load_mbuf(sc->sc_dmat, tx_data->map,
	    m, BUS_DMA_WRITE | BUS_DMA_NOWAIT);
	if (ret) {
		printf("%s: can't map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, ret);
		if (ret != ENOBUFS)
			m_freem(m);
		goto err_credits;
	}

	DNPRINTF(QWX_D_HTC, "%s: tx mbuf %p eid %d paddr %lx\n",
	    __func__, m, tx_data->eid, tx_data->map->dm_segs[0].ds_addr);
#ifdef QWX_DEBUG
	{
		int i;
		uint8_t *p = mtod(m, uint8_t *);
		DNPRINTF(QWX_D_HTC, "%s message buffer:", __func__);
		for (i = 0; i < m->m_pkthdr.len; i++) {
			DNPRINTF(QWX_D_HTC, "%s %.2x",
			    i % 16 == 0 ? "\n" : "", p[i]);
		}
		if (i % 16)
			DNPRINTF(QWX_D_HTC, "\n");
	}
#endif
	ret = qwx_ce_send(htc->sc, m, ep->ul_pipe_id, ep->eid);
	if (ret)
		goto err_unmap;

	return 0;

err_unmap:
	bus_dmamap_unload(sc->sc_dmat, tx_data->map);
err_credits:
	if (credit_flow_enabled) {
#ifdef notyet
		spin_lock_bh(&htc->tx_lock);
#endif
		ep->tx_credits += credits;
		DNPRINTF(QWX_D_HTC, "%s: ep %d credits reverted %d total %d\n",
		    __func__, eid, credits, ep->tx_credits);
#ifdef notyet
		spin_unlock_bh(&htc->tx_lock);
#endif

		if (ep->ep_ops.ep_tx_credits)
			ep->ep_ops.ep_tx_credits(htc->sc);
	}
	return ret;
}

int
qwx_htc_connect_service(struct qwx_htc *htc,
    struct qwx_htc_svc_conn_req *conn_req,
    struct qwx_htc_svc_conn_resp *conn_resp)
{
	struct qwx_softc *sc = htc->sc;
	struct ath11k_htc_conn_svc *req_msg;
	struct ath11k_htc_conn_svc_resp resp_msg_dummy;
	struct ath11k_htc_conn_svc_resp *resp_msg = &resp_msg_dummy;
	enum ath11k_htc_ep_id assigned_eid = ATH11K_HTC_EP_COUNT;
	struct qwx_htc_ep *ep;
	struct mbuf *m;
	unsigned int max_msg_size = 0;
	int length, status = 0;
	int disable_credit_flow_ctrl = 0;
	uint16_t flags = 0;
	uint16_t message_id, service_id;
	uint8_t tx_alloc = 0;

	/* special case for HTC pseudo control service */
	if (conn_req->service_id == ATH11K_HTC_SVC_ID_RSVD_CTRL) {
		disable_credit_flow_ctrl = 1;
		assigned_eid = ATH11K_HTC_EP_0;
		max_msg_size = ATH11K_HTC_MAX_CTRL_MSG_LEN;
		memset(&resp_msg_dummy, 0, sizeof(resp_msg_dummy));
		goto setup;
	}

	tx_alloc = qwx_htc_get_credit_allocation(htc, conn_req->service_id);
	if (!tx_alloc)
		DNPRINTF(QWX_D_HTC,
		    "%s: htc service %s does not allocate target credits\n",
		    sc->sc_dev.dv_xname,
		    qwx_htc_service_name(conn_req->service_id));

	m = qwx_htc_build_tx_ctrl_mbuf();
	if (!m) {
		printf("%s: Failed to allocate HTC packet\n",
		    sc->sc_dev.dv_xname);
		return ENOMEM;
	}

	length = sizeof(*req_msg);
	m->m_len = m->m_pkthdr.len = sizeof(struct ath11k_htc_hdr) + length;

	req_msg = (struct ath11k_htc_conn_svc *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr));
	memset(req_msg, 0, length);
	req_msg->msg_svc_id = FIELD_PREP(HTC_MSG_MESSAGEID,
	    ATH11K_HTC_MSG_CONNECT_SERVICE_ID);

	flags |= FIELD_PREP(ATH11K_HTC_CONN_FLAGS_RECV_ALLOC, tx_alloc);

	/* Only enable credit flow control for WMI ctrl service */
	if (!(conn_req->service_id == ATH11K_HTC_SVC_ID_WMI_CONTROL ||
	      conn_req->service_id == ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1 ||
	      conn_req->service_id == ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC2)) {
		flags |= ATH11K_HTC_CONN_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
		disable_credit_flow_ctrl = 1;
	}

	if (!sc->hw_params.credit_flow) {
		flags |= ATH11K_HTC_CONN_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
		disable_credit_flow_ctrl = 1;
	}

	req_msg->flags_len = FIELD_PREP(HTC_SVC_MSG_CONNECTIONFLAGS, flags);
	req_msg->msg_svc_id |= FIELD_PREP(HTC_SVC_MSG_SERVICE_ID,
	    conn_req->service_id);

	sc->ctl_resp = 0;

	status = qwx_htc_send(htc, ATH11K_HTC_EP_0, m);
	if (status) {
		if (status != ENOBUFS)
			m_freem(m);
		return status;
	}

	while (!sc->ctl_resp) {
		int ret = tsleep_nsec(&sc->ctl_resp, 0, "qwxhtcinit",
		    SEC_TO_NSEC(1));
		if (ret) {
			printf("%s: Service connect timeout\n",
			    sc->sc_dev.dv_xname);
			return ret;
		}
	}

	/* we controlled the buffer creation, it's aligned */
	resp_msg = (struct ath11k_htc_conn_svc_resp *)htc->control_resp_buffer;
	message_id = FIELD_GET(HTC_MSG_MESSAGEID, resp_msg->msg_svc_id);
	service_id = FIELD_GET(HTC_SVC_RESP_MSG_SERVICEID,
			       resp_msg->msg_svc_id);
	if ((message_id != ATH11K_HTC_MSG_CONNECT_SERVICE_RESP_ID) ||
	    (htc->control_resp_len < sizeof(*resp_msg))) {
		printf("%s: Invalid resp message ID 0x%x", __func__,
		    message_id);
		return EPROTO;
	}

	DNPRINTF(QWX_D_HTC, "%s: service %s connect response status 0x%lx "
	    "assigned ep 0x%lx\n", __func__, qwx_htc_service_name(service_id),
	    FIELD_GET(HTC_SVC_RESP_MSG_STATUS, resp_msg->flags_len),
	    FIELD_GET(HTC_SVC_RESP_MSG_ENDPOINTID, resp_msg->flags_len));

	conn_resp->connect_resp_code = FIELD_GET(HTC_SVC_RESP_MSG_STATUS,
	    resp_msg->flags_len);

	/* check response status */
	if (conn_resp->connect_resp_code !=
	    ATH11K_HTC_CONN_SVC_STATUS_SUCCESS) {
		printf("%s: HTC Service %s connect request failed: 0x%x)\n",
		    __func__, qwx_htc_service_name(service_id),
		    conn_resp->connect_resp_code);
		return EPROTO;
	}

	assigned_eid = (enum ath11k_htc_ep_id)FIELD_GET(
	    HTC_SVC_RESP_MSG_ENDPOINTID, resp_msg->flags_len);

	max_msg_size = FIELD_GET(HTC_SVC_RESP_MSG_MAXMSGSIZE,
	    resp_msg->flags_len);
setup:
	if (assigned_eid >= ATH11K_HTC_EP_COUNT)
		return EPROTO;

	if (max_msg_size == 0)
		return EPROTO;

	ep = &htc->endpoint[assigned_eid];
	ep->eid = assigned_eid;

	if (ep->service_id != ATH11K_HTC_SVC_ID_UNUSED)
		return EPROTO;

	/* return assigned endpoint to caller */
	conn_resp->eid = assigned_eid;
	conn_resp->max_msg_len = FIELD_GET(HTC_SVC_RESP_MSG_MAXMSGSIZE,
	    resp_msg->flags_len);

	/* setup the endpoint */
	ep->service_id = conn_req->service_id;
	ep->max_tx_queue_depth = conn_req->max_send_queue_depth;
	ep->max_ep_message_len = FIELD_GET(HTC_SVC_RESP_MSG_MAXMSGSIZE,
	    resp_msg->flags_len);
	ep->tx_credits = tx_alloc;

	/* copy all the callbacks */
	ep->ep_ops = conn_req->ep_ops;

	status = sc->ops.map_service_to_pipe(htc->sc, ep->service_id,
	    &ep->ul_pipe_id, &ep->dl_pipe_id);
	if (status)
		return status;

	DNPRINTF(QWX_D_HTC,
	    "%s: htc service '%s' ul pipe %d dl pipe %d eid %d ready\n",
	    __func__, qwx_htc_service_name(ep->service_id), ep->ul_pipe_id,
	    ep->dl_pipe_id, ep->eid);

	if (disable_credit_flow_ctrl && ep->tx_credit_flow_enabled) {
		ep->tx_credit_flow_enabled = 0;
		DNPRINTF(QWX_D_HTC,
		    "%s: htc service '%s' eid %d tx flow control disabled\n",
		    __func__, qwx_htc_service_name(ep->service_id),
		    assigned_eid);
	}

	return status;
}

int
qwx_htc_start(struct qwx_htc *htc)
{
	struct mbuf *m;
	int status = 0;
	struct qwx_softc *sc = htc->sc;
	struct ath11k_htc_setup_complete_extended *msg;

	m = qwx_htc_build_tx_ctrl_mbuf();
	if (!m)
		return ENOMEM;

	m->m_len = m->m_pkthdr.len = sizeof(struct ath11k_htc_hdr) +
	    sizeof(*msg);

	msg = (struct ath11k_htc_setup_complete_extended *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr));
	msg->msg_id = FIELD_PREP(HTC_MSG_MESSAGEID,
	    ATH11K_HTC_MSG_SETUP_COMPLETE_EX_ID);

	if (sc->hw_params.credit_flow)
		DNPRINTF(QWX_D_HTC, "%s: using tx credit flow control\n",
		    __func__);
	else
		msg->flags |= ATH11K_GLOBAL_DISABLE_CREDIT_FLOW;

	status = qwx_htc_send(htc, ATH11K_HTC_EP_0, m);
	if (status) {
		m_freem(m);
		return status;
	}

	return 0;
}

int
qwx_htc_init(struct qwx_softc *sc)
{
	struct qwx_htc *htc = &sc->htc;
	struct qwx_htc_svc_conn_req conn_req;
	struct qwx_htc_svc_conn_resp conn_resp;
	int ret;
#ifdef notyet
	spin_lock_init(&htc->tx_lock);
#endif
	qwx_htc_reset_endpoint_states(htc);

	htc->sc = sc;

	switch (sc->wmi.preferred_hw_mode) {
	case WMI_HOST_HW_MODE_SINGLE:
		htc->wmi_ep_count = 1;
		break;
	case WMI_HOST_HW_MODE_DBS:
	case WMI_HOST_HW_MODE_DBS_OR_SBS:
		htc->wmi_ep_count = 2;
		break;
	case WMI_HOST_HW_MODE_DBS_SBS:
		htc->wmi_ep_count = 3;
		break;
	default:
		htc->wmi_ep_count = sc->hw_params.max_radios;
		break;
	}

	/* setup our pseudo HTC control endpoint connection */
	memset(&conn_req, 0, sizeof(conn_req));
	memset(&conn_resp, 0, sizeof(conn_resp));
	conn_req.ep_ops.ep_tx_complete = qwx_htc_control_tx_complete;
	conn_req.ep_ops.ep_rx_complete = qwx_htc_control_rx_complete;
	conn_req.max_send_queue_depth = ATH11K_NUM_CONTROL_TX_BUFFERS;
	conn_req.service_id = ATH11K_HTC_SVC_ID_RSVD_CTRL;

	/* connect fake service */
	ret = qwx_htc_connect_service(htc, &conn_req, &conn_resp);
	if (ret) {
		printf("%s: could not connect to htc service (%d)\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	return 0;
}

int
qwx_htc_setup_target_buffer_assignments(struct qwx_htc *htc)
{
	struct qwx_htc_svc_tx_credits *serv_entry;
	uint32_t svc_id[] = {
		ATH11K_HTC_SVC_ID_WMI_CONTROL,
		ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1,
		ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC2,
	};
	int i, credits;

	credits =  htc->total_transmit_credits;
	serv_entry = htc->service_alloc_table;

	if ((htc->wmi_ep_count == 0) ||
	    (htc->wmi_ep_count > nitems(svc_id)))
		return EINVAL;

	/* Divide credits among number of endpoints for WMI */
	credits = credits / htc->wmi_ep_count;
	for (i = 0; i < htc->wmi_ep_count; i++) {
		serv_entry[i].service_id = svc_id[i];
		serv_entry[i].credit_allocation = credits;
	}

	return 0;
}

int
qwx_htc_wait_target(struct qwx_softc *sc)
{
	struct qwx_htc *htc = &sc->htc;
	int polling = 0, ret;
	uint16_t i;
	struct ath11k_htc_ready *ready;
	uint16_t message_id;
	uint16_t credit_count;
	uint16_t credit_size;

	sc->ctl_resp = 0;
	while (!sc->ctl_resp) {
		ret = tsleep_nsec(&sc->ctl_resp, 0, "qwxhtcinit",
		    SEC_TO_NSEC(1));
		if (ret) {
			if (ret != EWOULDBLOCK)
				return ret;

			if (polling) {
				printf("%s: failed to receive control response "
				    "completion\n", sc->sc_dev.dv_xname);
				return ret;
			}

			printf("%s: failed to receive control response "
			    "completion, polling...\n", sc->sc_dev.dv_xname);
			polling = 1;

			for (i = 0; i < sc->hw_params.ce_count; i++)
				qwx_ce_per_engine_service(sc, i);
		}
	}

	if (htc->control_resp_len < sizeof(*ready)) {
		printf("%s: Invalid HTC ready msg len:%d\n", __func__,
		    htc->control_resp_len);
		return EINVAL;
	}

	ready = (struct ath11k_htc_ready *)htc->control_resp_buffer;
	message_id = FIELD_GET(HTC_MSG_MESSAGEID, ready->id_credit_count);
	credit_count = FIELD_GET(HTC_READY_MSG_CREDITCOUNT,
	    ready->id_credit_count);
	credit_size = FIELD_GET(HTC_READY_MSG_CREDITSIZE, ready->size_ep);

	if (message_id != ATH11K_HTC_MSG_READY_ID) {
		printf("%s: Invalid HTC ready msg: 0x%x\n", __func__,
		    message_id);
		return EINVAL;
	}

	htc->total_transmit_credits = credit_count;
	htc->target_credit_size = credit_size;

	DNPRINTF(QWX_D_HTC, "%s: target ready total_transmit_credits %d "
	    "target_credit_size %d\n", __func__,
	    htc->total_transmit_credits, htc->target_credit_size);

	if ((htc->total_transmit_credits == 0) ||
	    (htc->target_credit_size == 0)) {
		printf("%s: Invalid credit size received\n", __func__);
		return EINVAL;
	}

	/* For QCA6390, wmi endpoint uses 1 credit to avoid
	 * back-to-back write.
	 */
	if (sc->hw_params.supports_shadow_regs)
		htc->total_transmit_credits = 1;

	qwx_htc_setup_target_buffer_assignments(htc);

	return 0;
}

void
qwx_dp_htt_htc_tx_complete(struct qwx_softc *sc, struct mbuf *m)
{
	/* Just free the mbuf, no further action required. */
	m_freem(m);
}

void
qwx_dp_htt_htc_t2h_msg_handler(struct qwx_softc *sc, struct mbuf *m)
{
	struct qwx_dp *dp = &sc->dp;
	struct htt_resp_msg *resp = mtod(m, struct htt_resp_msg *);
	enum htt_t2h_msg_type type = FIELD_GET(HTT_T2H_MSG_TYPE,
	    *(uint32_t *)resp);
#if 0
	uint16_t peer_id;
	uint8_t vdev_id;
	uint8_t mac_addr[IEEE80211_ADDR_LEN];
	uint16_t peer_mac_h16;
	uint16_t ast_hash;
	uint16_t hw_peer_id;
#endif
	DPRINTF("%s: dp_htt rx msg type: 0x%0x\n", __func__, type);

	switch (type) {
	case HTT_T2H_MSG_TYPE_VERSION_CONF:
		dp->htt_tgt_ver_major = FIELD_GET(HTT_T2H_VERSION_CONF_MAJOR,
		    resp->version_msg.version);
		dp->htt_tgt_ver_minor = FIELD_GET(HTT_T2H_VERSION_CONF_MINOR,
		    resp->version_msg.version);
		dp->htt_tgt_version_received = 1;
		wakeup(&dp->htt_tgt_version_received);
		break;
#if 0
	case HTT_T2H_MSG_TYPE_PEER_MAP:
		vdev_id = FIELD_GET(HTT_T2H_PEER_MAP_INFO_VDEV_ID,
				    resp->peer_map_ev.info);
		peer_id = FIELD_GET(HTT_T2H_PEER_MAP_INFO_PEER_ID,
				    resp->peer_map_ev.info);
		peer_mac_h16 = FIELD_GET(HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16,
					 resp->peer_map_ev.info1);
		ath11k_dp_get_mac_addr(resp->peer_map_ev.mac_addr_l32,
				       peer_mac_h16, mac_addr);
		ath11k_peer_map_event(ab, vdev_id, peer_id, mac_addr, 0, 0);
		break;
	case HTT_T2H_MSG_TYPE_PEER_MAP2:
		vdev_id = FIELD_GET(HTT_T2H_PEER_MAP_INFO_VDEV_ID,
				    resp->peer_map_ev.info);
		peer_id = FIELD_GET(HTT_T2H_PEER_MAP_INFO_PEER_ID,
				    resp->peer_map_ev.info);
		peer_mac_h16 = FIELD_GET(HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16,
					 resp->peer_map_ev.info1);
		ath11k_dp_get_mac_addr(resp->peer_map_ev.mac_addr_l32,
				       peer_mac_h16, mac_addr);
		ast_hash = FIELD_GET(HTT_T2H_PEER_MAP_INFO2_AST_HASH_VAL,
				     resp->peer_map_ev.info2);
		hw_peer_id = FIELD_GET(HTT_T2H_PEER_MAP_INFO1_HW_PEER_ID,
				       resp->peer_map_ev.info1);
		ath11k_peer_map_event(ab, vdev_id, peer_id, mac_addr, ast_hash,
				      hw_peer_id);
		break;
	case HTT_T2H_MSG_TYPE_PEER_UNMAP:
	case HTT_T2H_MSG_TYPE_PEER_UNMAP2:
		peer_id = FIELD_GET(HTT_T2H_PEER_UNMAP_INFO_PEER_ID,
				    resp->peer_unmap_ev.info);
		ath11k_peer_unmap_event(ab, peer_id);
		break;
	case HTT_T2H_MSG_TYPE_PPDU_STATS_IND:
		ath11k_htt_pull_ppdu_stats(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_EXT_STATS_CONF:
		ath11k_debugfs_htt_ext_stats_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_PKTLOG:
		ath11k_htt_pktlog(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_BKPRESSURE_EVENT_IND:
		ath11k_htt_backpressure_event_handler(ab, skb);
		break;
#endif
	default:
		printf("%s: htt event %d not handled\n", __func__, type);
		break;
	}

	m_freem(m);
}

int
qwx_dp_htt_connect(struct qwx_dp *dp)
{
	struct qwx_htc_svc_conn_req conn_req;
	struct qwx_htc_svc_conn_resp conn_resp;
	int status;

	memset(&conn_req, 0, sizeof(conn_req));
	memset(&conn_resp, 0, sizeof(conn_resp));

	conn_req.ep_ops.ep_tx_complete = qwx_dp_htt_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = qwx_dp_htt_htc_t2h_msg_handler;

	/* connect to control service */
	conn_req.service_id = ATH11K_HTC_SVC_ID_HTT_DATA_MSG;

	status = qwx_htc_connect_service(&dp->sc->htc, &conn_req, &conn_resp);

	if (status)
		return status;

	dp->eid = conn_resp.eid;

	return 0;
}

void
qwx_dp_pdev_reo_cleanup(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	int i;

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		qwx_dp_srng_cleanup(sc, &dp->reo_dst_ring[i]);
}

int
qwx_dp_pdev_reo_setup(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	int ret;
	int i;

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		ret = qwx_dp_srng_setup(sc, &dp->reo_dst_ring[i],
		    HAL_REO_DST, i, 0, DP_REO_DST_RING_SIZE);
		if (ret) {
			printf("%s: failed to setup reo_dst_ring\n", __func__);
			qwx_dp_pdev_reo_cleanup(sc);
			return ret;
		}
	}

	return 0;
}

void
qwx_dp_rx_pdev_srng_free(struct qwx_softc *sc, int mac_id)
{
	struct qwx_pdev_dp *dp = &sc->pdev_dp;
	int i;

	qwx_dp_srng_cleanup(sc, &dp->rx_refill_buf_ring.refill_buf_ring);

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		if (sc->hw_params.rx_mac_buf_ring)
			qwx_dp_srng_cleanup(sc, &dp->rx_mac_buf_ring[i]);

		qwx_dp_srng_cleanup(sc, &dp->rxdma_err_dst_ring[i]);
		qwx_dp_srng_cleanup(sc,
		    &dp->rx_mon_status_refill_ring[i].refill_buf_ring);
	}

	qwx_dp_srng_cleanup(sc, &dp->rxdma_mon_buf_ring.refill_buf_ring);
}

int
qwx_dp_rx_pdev_srng_alloc(struct qwx_softc *sc)
{
	struct qwx_pdev_dp *dp = &sc->pdev_dp;
	struct dp_srng *srng = NULL;
	int i;
	int ret;

	ret = qwx_dp_srng_setup(sc, &dp->rx_refill_buf_ring.refill_buf_ring,
	    HAL_RXDMA_BUF, 0, dp->mac_id, DP_RXDMA_BUF_RING_SIZE);
	if (ret) {
		printf("%s: failed to setup rx_refill_buf_ring\n",
		    sc->sc_dev.dv_xname);
		return ret;
	}

	if (sc->hw_params.rx_mac_buf_ring) {
		for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
			ret = qwx_dp_srng_setup(sc, &dp->rx_mac_buf_ring[i],
			    HAL_RXDMA_BUF, 1, dp->mac_id + i, 1024);
			if (ret) {
				printf("%s: failed to setup "
				    "rx_mac_buf_ring %d\n",
				    sc->sc_dev.dv_xname, i);
				return ret;
			}
		}
	}

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		ret = qwx_dp_srng_setup(sc, &dp->rxdma_err_dst_ring[i],
		    HAL_RXDMA_DST, 0, dp->mac_id + i,
		    DP_RXDMA_ERR_DST_RING_SIZE);
		if (ret) {
			printf("%s: failed to setup rxdma_err_dst_ring %d\n",
			   sc->sc_dev.dv_xname, i);
			return ret;
		}
	}

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		srng = &dp->rx_mon_status_refill_ring[i].refill_buf_ring;
		ret = qwx_dp_srng_setup(sc, srng, HAL_RXDMA_MONITOR_STATUS, 0,
		    dp->mac_id + i, DP_RXDMA_MON_STATUS_RING_SIZE);
		if (ret) {
			printf("%s: failed to setup "
			    "rx_mon_status_refill_ring %d\n",
			    sc->sc_dev.dv_xname, i);
			return ret;
		}
	}

	/* if rxdma1_enable is false, then it doesn't need
	 * to setup rxdam_mon_buf_ring, rxdma_mon_dst_ring
	 * and rxdma_mon_desc_ring.
	 * init reap timer for QCA6390.
	 */
	if (!sc->hw_params.rxdma1_enable) {
#if 0
		//init mon status buffer reap timer
		timer_setup(&ar->ab->mon_reap_timer,
			    ath11k_dp_service_mon_ring, 0);
#endif
		return 0;
	}
#if 0
	ret = ath11k_dp_srng_setup(ar->ab,
				   &dp->rxdma_mon_buf_ring.refill_buf_ring,
				   HAL_RXDMA_MONITOR_BUF, 0, dp->mac_id,
				   DP_RXDMA_MONITOR_BUF_RING_SIZE);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to setup HAL_RXDMA_MONITOR_BUF\n");
		return ret;
	}

	ret = ath11k_dp_srng_setup(ar->ab, &dp->rxdma_mon_dst_ring,
				   HAL_RXDMA_MONITOR_DST, 0, dp->mac_id,
				   DP_RXDMA_MONITOR_DST_RING_SIZE);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to setup HAL_RXDMA_MONITOR_DST\n");
		return ret;
	}

	ret = ath11k_dp_srng_setup(ar->ab, &dp->rxdma_mon_desc_ring,
				   HAL_RXDMA_MONITOR_DESC, 0, dp->mac_id,
				   DP_RXDMA_MONITOR_DESC_RING_SIZE);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to setup HAL_RXDMA_MONITOR_DESC\n");
		return ret;
	}
#endif
	return 0;
}

void
qwx_dp_rxdma_buf_ring_free(struct qwx_softc *sc, struct dp_rxdma_ring *rx_ring)
{
	int i;

	for (i = 0; i < rx_ring->bufs_max; i++) {
		struct qwx_rx_data *rx_data = &rx_ring->rx_data[i];

		if (rx_data->map == NULL)
			continue;

		if (rx_data->m) {
			bus_dmamap_unload(sc->sc_dmat, rx_data->map);
			m_free(rx_data->m);
			rx_data->m = NULL;
		}

		bus_dmamap_destroy(sc->sc_dmat, rx_data->map);
		rx_data->map = NULL;
	}

	free(rx_ring->rx_data, M_DEVBUF,
	    sizeof(rx_ring->rx_data[0]) * rx_ring->bufs_max);
	rx_ring->rx_data = NULL;
	rx_ring->bufs_max = 0;
	rx_ring->cur = 0;
}

void
qwx_dp_rxdma_pdev_buf_free(struct qwx_softc *sc, int mac_id)
{
	struct qwx_pdev_dp *dp = &sc->pdev_dp;
	struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
	int i;

	qwx_dp_rxdma_buf_ring_free(sc, rx_ring);

	rx_ring = &dp->rxdma_mon_buf_ring;
	qwx_dp_rxdma_buf_ring_free(sc, rx_ring);

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		rx_ring = &dp->rx_mon_status_refill_ring[i];
		qwx_dp_rxdma_buf_ring_free(sc, rx_ring);
	}
}

void
qwx_hal_rx_buf_addr_info_set(void *desc, uint64_t paddr, uint32_t cookie,
    uint8_t manager)
{
	struct ath11k_buffer_addr *binfo = (struct ath11k_buffer_addr *)desc;
	uint32_t paddr_lo, paddr_hi;

	paddr_lo = paddr & 0xffffffff;
	paddr_hi = paddr >> 32;
	binfo->info0 = FIELD_PREP(BUFFER_ADDR_INFO0_ADDR, paddr_lo);
	binfo->info1 = FIELD_PREP(BUFFER_ADDR_INFO1_ADDR, paddr_hi) |
	    FIELD_PREP(BUFFER_ADDR_INFO1_SW_COOKIE, cookie) |
	    FIELD_PREP(BUFFER_ADDR_INFO1_RET_BUF_MGR, manager);
}

/* Returns number of Rx buffers replenished */
int
qwx_dp_rxbufs_replenish(struct qwx_softc *sc, int mac_id,
    struct dp_rxdma_ring *rx_ring, int req_entries,
    enum hal_rx_buf_return_buf_manager mgr)
{
	struct hal_srng *srng;
	uint32_t *desc;
	struct mbuf *m;
	int num_free;
	int num_remain;
	int ret;
	uint32_t cookie;
	uint64_t paddr;
	struct qwx_rx_data *rx_data;

	req_entries = MIN(req_entries, rx_ring->bufs_max);

	srng = &sc->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
#ifdef notyet
	spin_lock_bh(&srng->lock);
#endif
	qwx_hal_srng_access_begin(sc, srng);

	num_free = qwx_hal_srng_src_num_free(sc, srng, 1);
	if (!req_entries && (num_free > (rx_ring->bufs_max * 3) / 4))
		req_entries = num_free;

	req_entries = MIN(num_free, req_entries);
	num_remain = req_entries;

	while (num_remain > 0) {
		const size_t size = DP_RX_BUFFER_SIZE;

		m = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			goto fail_free_mbuf;

		if (size <= MCLBYTES)
			MCLGET(m, M_DONTWAIT);
		else
			MCLGETL(m, M_DONTWAIT, size);
		if ((m->m_flags & M_EXT) == 0)
			goto fail_free_mbuf;

		m->m_len = m->m_pkthdr.len = size;
		rx_data = &rx_ring->rx_data[rx_ring->cur];
		if (rx_data->m != NULL)
			goto fail_free_mbuf;

		if (rx_data->map == NULL) {
			ret = bus_dmamap_create(sc->sc_dmat, size, 1,
			    size, 0, BUS_DMA_NOWAIT, &rx_data->map);
			if (ret)
				goto fail_free_mbuf;
		}
		
		ret = bus_dmamap_load_mbuf(sc->sc_dmat, rx_data->map, m,
		    BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (ret) {
			printf("%s: can't map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, ret);
			goto fail_free_mbuf;
		}

		desc = qwx_hal_srng_src_get_next_entry(sc, srng);
		if (!desc)
			goto fail_dma_unmap;

		rx_data->m = m;

		cookie = FIELD_PREP(DP_RXDMA_BUF_COOKIE_PDEV_ID, mac_id) |
		    FIELD_PREP(DP_RXDMA_BUF_COOKIE_BUF_ID, rx_ring->cur);

		rx_ring->cur = (rx_ring->cur + 1) % rx_ring->bufs_max;
		num_remain--;

		paddr = rx_data->map->dm_segs[0].ds_addr;
		qwx_hal_rx_buf_addr_info_set(desc, paddr, cookie, mgr);
	}

	qwx_hal_srng_access_end(sc, srng);
#ifdef notyet
	spin_unlock_bh(&srng->lock);
#endif
	return 0;

fail_dma_unmap:
	bus_dmamap_unload(sc->sc_dmat, rx_data->map);
fail_free_mbuf:
	m_free(m);

	qwx_hal_srng_access_end(sc, srng);
#ifdef notyet
	spin_unlock_bh(&srng->lock);
#endif
	return ENOBUFS;
}

int
qwx_dp_rxdma_ring_buf_setup(struct qwx_softc *sc,
    struct dp_rxdma_ring *rx_ring, uint32_t ringtype)
{
	struct qwx_pdev_dp *dp = &sc->pdev_dp;
	int num_entries;

	num_entries = rx_ring->refill_buf_ring.size /
	    qwx_hal_srng_get_entrysize(sc, ringtype);

	KASSERT(rx_ring->rx_data == NULL);
	rx_ring->rx_data = mallocarray(num_entries, sizeof(rx_ring->rx_data[0]),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (rx_ring->rx_data == NULL)
		return ENOMEM;

	rx_ring->bufs_max = num_entries;
	rx_ring->cur = 0;

	return qwx_dp_rxbufs_replenish(sc, dp->mac_id, rx_ring, num_entries,
	    sc->hw_params.hal_params->rx_buf_rbm);
}

int
qwx_dp_rxdma_pdev_buf_setup(struct qwx_softc *sc)
{
	struct qwx_pdev_dp *dp = &sc->pdev_dp;
	struct dp_rxdma_ring *rx_ring;
	int ret, i;

	rx_ring = &dp->rx_refill_buf_ring;
	ret = qwx_dp_rxdma_ring_buf_setup(sc, rx_ring, HAL_RXDMA_BUF);
	if (ret)
		return ret;

	if (sc->hw_params.rxdma1_enable) {
		rx_ring = &dp->rxdma_mon_buf_ring;
		ret = qwx_dp_rxdma_ring_buf_setup(sc, rx_ring,
		    HAL_RXDMA_MONITOR_BUF);
		if (ret)
			return ret;
	}

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		rx_ring = &dp->rx_mon_status_refill_ring[i];
		ret = qwx_dp_rxdma_ring_buf_setup(sc, rx_ring,
		    HAL_RXDMA_MONITOR_STATUS);
		if (ret)
			return ret;
	}

	return 0;
}

void
qwx_dp_rx_pdev_free(struct qwx_softc *sc, int mac_id)
{
	qwx_dp_rx_pdev_srng_free(sc, mac_id);
	qwx_dp_rxdma_pdev_buf_free(sc, mac_id);
}

bus_addr_t
qwx_hal_srng_get_hp_addr(struct qwx_softc *sc, struct hal_srng *srng)
{
	if (!(srng->flags & HAL_SRNG_FLAGS_LMAC_RING))
		return 0;

	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		return sc->hal.wrp.paddr +
		    ((unsigned long)srng->u.src_ring.hp_addr -
		    (unsigned long)sc->hal.wrp.vaddr);
	} else {
		return sc->hal.rdp.paddr +
		    ((unsigned long)srng->u.dst_ring.hp_addr -
		    (unsigned long)sc->hal.rdp.vaddr);
	}
}

bus_addr_t
qwx_hal_srng_get_tp_addr(struct qwx_softc *sc, struct hal_srng *srng)
{
	if (!(srng->flags & HAL_SRNG_FLAGS_LMAC_RING))
		return 0;

	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		return sc->hal.rdp.paddr +
		    ((unsigned long)srng->u.src_ring.tp_addr -
		    (unsigned long)sc->hal.rdp.vaddr);
	} else {
		return sc->hal.wrp.paddr +
		    ((unsigned long)srng->u.dst_ring.tp_addr -
		    (unsigned long)sc->hal.wrp.vaddr);
	}
}

int
qwx_dp_tx_get_ring_id_type(struct qwx_softc *sc, int mac_id, uint32_t ring_id,
    enum hal_ring_type ring_type, enum htt_srng_ring_type *htt_ring_type,
    enum htt_srng_ring_id *htt_ring_id)
{
	int lmac_ring_id_offset = 0;

	switch (ring_type) {
	case HAL_RXDMA_BUF:
		lmac_ring_id_offset = mac_id * HAL_SRNG_RINGS_PER_LMAC;

		/* for QCA6390, host fills rx buffer to fw and fw fills to
		 * rxbuf ring for each rxdma
		 */
		if (!sc->hw_params.rx_mac_buf_ring) {
			if (!(ring_id == (HAL_SRNG_RING_ID_WMAC1_SW2RXDMA0_BUF +
			    lmac_ring_id_offset) ||
			    ring_id == (HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_BUF +
			    lmac_ring_id_offset)))
				return EINVAL;
			*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
			*htt_ring_type = HTT_SW_TO_HW_RING;
		} else {
			if (ring_id == HAL_SRNG_RING_ID_WMAC1_SW2RXDMA0_BUF) {
				*htt_ring_id = HTT_HOST1_TO_FW_RXBUF_RING;
				*htt_ring_type = HTT_SW_TO_SW_RING;
			} else {
				*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
				*htt_ring_type = HTT_SW_TO_HW_RING;
			}
		}
		break;
	case HAL_RXDMA_DST:
		*htt_ring_id = HTT_RXDMA_NON_MONITOR_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_BUF:
		*htt_ring_id = HTT_RXDMA_MONITOR_BUF_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		*htt_ring_id = HTT_RXDMA_MONITOR_STATUS_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_DST:
		*htt_ring_id = HTT_RXDMA_MONITOR_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_DESC:
		*htt_ring_id = HTT_RXDMA_MONITOR_DESC_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	default:
		printf("%s: Unsupported ring type in DP :%d\n",
		    sc->sc_dev.dv_xname, ring_type);
		return EINVAL;
	}

	return 0;
}

int
qwx_dp_tx_htt_srng_setup(struct qwx_softc *sc, uint32_t ring_id, int mac_id,
    enum hal_ring_type ring_type)
{
	struct htt_srng_setup_cmd *cmd;
	struct hal_srng *srng = &sc->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct mbuf *m;
	uint32_t ring_entry_sz;
	uint64_t hp_addr, tp_addr;
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	m = qwx_htc_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	memset(&params, 0, sizeof(params));
	qwx_hal_srng_get_params(sc, srng, &params);

	hp_addr = qwx_hal_srng_get_hp_addr(sc, srng);
	tp_addr = qwx_hal_srng_get_tp_addr(sc, srng);

	ret = qwx_dp_tx_get_ring_id_type(sc, mac_id, ring_id,
	    ring_type, &htt_ring_type, &htt_ring_id);
	if (ret)
		goto err_free;

	cmd = (struct htt_srng_setup_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr));
	cmd->info0 = FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_MSG_TYPE,
	    HTT_H2T_MSG_TYPE_SRING_SETUP);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID,
		    DP_SW2HW_MACID(mac_id));
	else
		cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID,
		    mac_id);
	cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_RING_TYPE,
	    htt_ring_type);
	cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_RING_ID, htt_ring_id);

	cmd->ring_base_addr_lo = params.ring_base_paddr & HAL_ADDR_LSB_REG_MASK;

	cmd->ring_base_addr_hi = (uint64_t)params.ring_base_paddr >>
	    HAL_ADDR_MSB_REG_SHIFT;

	ring_entry_sz = qwx_hal_srng_get_entrysize(sc, ring_type);

	ring_entry_sz >>= 2;
	cmd->info1 = FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO1_RING_ENTRY_SIZE,
	    ring_entry_sz);
	cmd->info1 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO1_RING_SIZE,
	    params.num_entries * ring_entry_sz);
	cmd->info1 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_MSI_SWAP,
	    !!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP));
	cmd->info1 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_TLV_SWAP,
	    !!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP));
	cmd->info1 |= FIELD_PREP(
	    HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_HOST_FW_SWAP,
	    !!(params.flags & HAL_SRNG_FLAGS_RING_PTR_SWAP));
	if (htt_ring_type == HTT_SW_TO_HW_RING)
		cmd->info1 |= HTT_SRNG_SETUP_CMD_INFO1_RING_LOOP_CNT_DIS;

	cmd->ring_head_off32_remote_addr_lo = hp_addr & HAL_ADDR_LSB_REG_MASK;
	cmd->ring_head_off32_remote_addr_hi = hp_addr >> HAL_ADDR_MSB_REG_SHIFT;

	cmd->ring_tail_off32_remote_addr_lo = tp_addr & HAL_ADDR_LSB_REG_MASK;
	cmd->ring_tail_off32_remote_addr_hi = tp_addr >> HAL_ADDR_MSB_REG_SHIFT;

	cmd->ring_msi_addr_lo = params.msi_addr & 0xffffffff;
	cmd->ring_msi_addr_hi = 0;
	cmd->msi_data = params.msi_data;

	cmd->intr_info = FIELD_PREP(
	    HTT_SRNG_SETUP_CMD_INTR_INFO_BATCH_COUNTER_THRESH,
	    params.intr_batch_cntr_thres_entries * ring_entry_sz);
	cmd->intr_info |= FIELD_PREP(
	    HTT_SRNG_SETUP_CMD_INTR_INFO_INTR_TIMER_THRESH,
	    params.intr_timer_thres_us >> 3);

	cmd->info2 = 0;
	if (params.flags & HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN) {
		cmd->info2 = FIELD_PREP(
		    HTT_SRNG_SETUP_CMD_INFO2_INTR_LOW_THRESH,
		    params.low_threshold);
	}

	DNPRINTF(QWX_D_HTT, "%s: htt srng setup msi_addr_lo 0x%x "
	    "msi_addr_hi 0x%x msi_data 0x%x ring_id %d ring_type %d "
	    "intr_info 0x%x flags 0x%x\n", __func__, cmd->ring_msi_addr_lo,
	    cmd->ring_msi_addr_hi, cmd->msi_data, ring_id, ring_type,
	    cmd->intr_info, cmd->info2);

	ret = qwx_htc_send(&sc->htc, sc->dp.eid, m);
	if (ret)
		goto err_free;

	return 0;

err_free:
	m_freem(m);

	return ret;
}

int
qwx_dp_tx_htt_h2t_ppdu_stats_req(struct qwx_softc *sc, uint32_t mask,
    uint8_t pdev_id)
{
	struct qwx_dp *dp = &sc->dp;
	struct mbuf *m;
	struct htt_ppdu_stats_cfg_cmd *cmd;
	int len = sizeof(*cmd);
	uint8_t pdev_mask;
	int ret;
	int i;

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		m = qwx_htc_alloc_mbuf(len);
		if (!m)
			return ENOMEM;

		cmd = (struct htt_ppdu_stats_cfg_cmd *)(mtod(m, uint8_t *) +
		    sizeof(struct ath11k_htc_hdr));
		cmd->msg = FIELD_PREP(HTT_PPDU_STATS_CFG_MSG_TYPE,
				      HTT_H2T_MSG_TYPE_PPDU_STATS_CFG);

		pdev_mask = 1 << (pdev_id + i);
		cmd->msg |= FIELD_PREP(HTT_PPDU_STATS_CFG_PDEV_ID, pdev_mask);
		cmd->msg |= FIELD_PREP(HTT_PPDU_STATS_CFG_TLV_TYPE_BITMASK,
		    mask);

		ret = qwx_htc_send(&sc->htc, dp->eid, m);
		if (ret) {
			m_freem(m);
			return ret;
		}
	}

	return 0;
}

int
qwx_dp_tx_htt_rx_filter_setup(struct qwx_softc *sc, uint32_t ring_id,
    int mac_id, enum hal_ring_type ring_type, size_t rx_buf_size,
    struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd;
	struct hal_srng *srng = &sc->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct mbuf *m;
	int len = sizeof(*cmd);
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	m = qwx_htc_alloc_mbuf(len);
	if (!m)
		return ENOMEM;

	memset(&params, 0, sizeof(params));
	qwx_hal_srng_get_params(sc, srng, &params);

	ret = qwx_dp_tx_get_ring_id_type(sc, mac_id, ring_id,
	    ring_type, &htt_ring_type, &htt_ring_id);
	if (ret)
		goto err_free;

	cmd = (struct htt_rx_ring_selection_cfg_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr));
	cmd->info0 = FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE,
	    HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING) {
		cmd->info0 |=
		    FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID,
		    DP_SW2HW_MACID(mac_id));
	} else {
		cmd->info0 |=
		    FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID,
		    mac_id);
	}
	cmd->info0 |= FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_RING_ID,
	    htt_ring_id);
	cmd->info0 |= FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_SS,
	    !!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP));
	cmd->info0 |= FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PS,
	    !!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP));

	cmd->info1 = FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO1_BUF_SIZE,
	    rx_buf_size);
	cmd->pkt_type_en_flags0 = tlv_filter->pkt_filter_flags0;
	cmd->pkt_type_en_flags1 = tlv_filter->pkt_filter_flags1;
	cmd->pkt_type_en_flags2 = tlv_filter->pkt_filter_flags2;
	cmd->pkt_type_en_flags3 = tlv_filter->pkt_filter_flags3;
	cmd->rx_filter_tlv = tlv_filter->rx_filter;

	ret = qwx_htc_send(&sc->htc, sc->dp.eid, m);
	if (ret)
		goto err_free;

	return 0;

err_free:
	m_freem(m);

	return ret;
}

int
qwx_dp_rx_pdev_alloc(struct qwx_softc *sc, int mac_id)
{
	struct qwx_pdev_dp *dp = &sc->pdev_dp;
	uint32_t ring_id;
	int i;
	int ret;

	ret = qwx_dp_rx_pdev_srng_alloc(sc);
	if (ret) {
		printf("%s: failed to setup rx srngs: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_dp_rxdma_pdev_buf_setup(sc);
	if (ret) {
		printf("%s: failed to setup rxdma ring: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;
	ret = qwx_dp_tx_htt_srng_setup(sc, ring_id, mac_id, HAL_RXDMA_BUF);
	if (ret) {
		printf("%s: failed to configure rx_refill_buf_ring: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	if (sc->hw_params.rx_mac_buf_ring) {
		for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
			ring_id = dp->rx_mac_buf_ring[i].ring_id;
			ret = qwx_dp_tx_htt_srng_setup(sc, ring_id,
			    mac_id + i, HAL_RXDMA_BUF);
			if (ret) {
				printf("%s: failed to configure "
				    "rx_mac_buf_ring%d: %d\n",
				    sc->sc_dev.dv_xname, i, ret);
				return ret;
			}
		}
	}

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		ring_id = dp->rxdma_err_dst_ring[i].ring_id;
		ret = qwx_dp_tx_htt_srng_setup(sc, ring_id, mac_id + i,
		    HAL_RXDMA_DST);
		if (ret) {
			printf("%s: failed to configure "
			    "rxdma_err_dest_ring%d %d\n",
			    sc->sc_dev.dv_xname, i, ret);
			return ret;
		}
	}

	if (!sc->hw_params.rxdma1_enable)
		goto config_refill_ring;
#if 0
	ring_id = dp->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
	ret = ath11k_dp_tx_htt_srng_setup(ab, ring_id,
					  mac_id, HAL_RXDMA_MONITOR_BUF);
	if (ret) {
		ath11k_warn(ab, "failed to configure rxdma_mon_buf_ring %d\n",
			    ret);
		return ret;
	}
	ret = ath11k_dp_tx_htt_srng_setup(ab,
					  dp->rxdma_mon_dst_ring.ring_id,
					  mac_id, HAL_RXDMA_MONITOR_DST);
	if (ret) {
		ath11k_warn(ab, "failed to configure rxdma_mon_dst_ring %d\n",
			    ret);
		return ret;
	}
	ret = ath11k_dp_tx_htt_srng_setup(ab,
					  dp->rxdma_mon_desc_ring.ring_id,
					  mac_id, HAL_RXDMA_MONITOR_DESC);
	if (ret) {
		ath11k_warn(ab, "failed to configure rxdma_mon_dst_ring %d\n",
			    ret);
		return ret;
	}
#endif
config_refill_ring:
	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		ret = qwx_dp_tx_htt_srng_setup(sc,
		    dp->rx_mon_status_refill_ring[i].refill_buf_ring.ring_id,
		    mac_id + i, HAL_RXDMA_MONITOR_STATUS);
		if (ret) {
			printf("%s: failed to configure "
			    "mon_status_refill_ring%d %d\n",
			    sc->sc_dev.dv_xname, i, ret);
			return ret;
		}
	}

	return 0;
}

void
qwx_dp_pdev_free(struct qwx_softc *sc)
{
	int i;

	for (i = 0; i < sc->num_radios; i++)
		qwx_dp_rx_pdev_free(sc, i);
}

int
qwx_dp_pdev_alloc(struct qwx_softc *sc)
{
	int ret;
	int i;

	for (i = 0; i < sc->num_radios; i++) {
		ret = qwx_dp_rx_pdev_alloc(sc, i);
		if (ret) {
			printf("%s: failed to allocate pdev rx "
			    "for pdev_id %d\n", sc->sc_dev.dv_xname, i);
			goto err;
		}
	}

	return 0;

err:
	qwx_dp_pdev_free(sc);

	return ret;
}

int
qwx_dp_tx_htt_h2t_ver_req_msg(struct qwx_softc *sc)
{
	struct qwx_dp *dp = &sc->dp;
	struct mbuf *m;
	struct htt_ver_req_cmd *cmd;
	int len = sizeof(*cmd);
	int ret;

	dp->htt_tgt_version_received = 0;

	m = qwx_htc_alloc_mbuf(len);
	if (!m)
		return ENOMEM;

	cmd = (struct htt_ver_req_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr));
	cmd->ver_reg_info = FIELD_PREP(HTT_VER_REQ_INFO_MSG_ID,
	    HTT_H2T_MSG_TYPE_VERSION_REQ);

	ret = qwx_htc_send(&sc->htc, dp->eid, m);
	if (ret) {
		m_freem(m);
		return ret;
	}

	while (!dp->htt_tgt_version_received) {
		ret = tsleep_nsec(&dp->htt_tgt_version_received, 0,
		    "qwxtgtver", SEC_TO_NSEC(3));
		if (ret)
			return ETIMEDOUT;
	}

	if (dp->htt_tgt_ver_major != HTT_TARGET_VERSION_MAJOR) {
		printf("%s: unsupported htt major version %d "
		    "supported version is %d\n", __func__,
		    dp->htt_tgt_ver_major, HTT_TARGET_VERSION_MAJOR);
		return ENOTSUP;
	}

	return 0;
}

void
qwx_dp_update_vdev_search(struct qwx_softc *sc, struct qwx_vif *arvif)
{
	 /* When v2_map_support is true:for STA mode, enable address
	  * search index, tcl uses ast_hash value in the descriptor.
	  * When v2_map_support is false: for STA mode, don't enable
	  * address search index.
	  */
	switch (arvif->vdev_type) {
	case WMI_VDEV_TYPE_STA:
		if (sc->hw_params.htt_peer_map_v2) {
			arvif->hal_addr_search_flags = HAL_TX_ADDRX_EN;
			arvif->search_type = HAL_TX_ADDR_SEARCH_INDEX;
		} else {
			arvif->hal_addr_search_flags = HAL_TX_ADDRY_EN;
			arvif->search_type = HAL_TX_ADDR_SEARCH_DEFAULT;
		}
		break;
	case WMI_VDEV_TYPE_AP:
	case WMI_VDEV_TYPE_IBSS:
		arvif->hal_addr_search_flags = HAL_TX_ADDRX_EN;
		arvif->search_type = HAL_TX_ADDR_SEARCH_DEFAULT;
		break;
	case WMI_VDEV_TYPE_MONITOR:
	default:
		return;
	}
}

void
qwx_dp_vdev_tx_attach(struct qwx_softc *sc, struct qwx_pdev *pdev,
    struct qwx_vif *arvif)
{
	arvif->tcl_metadata |= FIELD_PREP(HTT_TCL_META_DATA_TYPE, 1) |
	    FIELD_PREP(HTT_TCL_META_DATA_VDEV_ID, arvif->vdev_id) |
	    FIELD_PREP(HTT_TCL_META_DATA_PDEV_ID, pdev->pdev_id);

	/* set HTT extension valid bit to 0 by default */
	arvif->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT;

	qwx_dp_update_vdev_search(sc, arvif);
}

int
qwx_dp_tx_completion_handler(struct qwx_softc *sc, int ring_id)
{
	return 0;
}

int
qwx_dp_process_rx_err(struct qwx_softc *sc)
{
	return 0;
}

int
qwx_dp_rx_process_wbm_err(struct qwx_softc *sc)
{
	return 0;
}

void
qwx_dp_rx_process_received_packets(struct qwx_softc *sc,
    struct mbuf_list *msdu_list, int mac_id)
{
	printf("%s: not implemented", __func__);
	ml_purge(msdu_list);
}

int
qwx_dp_process_rx(struct qwx_softc *sc, int ring_id)
{
	struct qwx_dp *dp = &sc->dp;
	struct qwx_pdev_dp *pdev_dp = &sc->pdev_dp;
	struct dp_rxdma_ring *rx_ring;
	int num_buffs_reaped[MAX_RADIOS] = {0};
	struct mbuf_list msdu_list[MAX_RADIOS];
	struct mbuf *m;
	struct qwx_rx_data *rx_data;
	int total_msdu_reaped = 0;
	struct hal_srng *srng;
	int done = 0;
	int idx, mac_id;
	struct hal_reo_dest_ring *desc;
	enum hal_reo_dest_ring_push_reason push_reason;
	uint32_t cookie;
	int i;

	for (i = 0; i < MAX_RADIOS; i++)
		ml_init(&msdu_list[i]);

	srng = &sc->hal.srng_list[dp->reo_dst_ring[ring_id].ring_id];
#ifdef notyet
	spin_lock_bh(&srng->lock);
#endif
try_again:
	qwx_hal_srng_access_begin(sc, srng);

	while ((desc = (struct hal_reo_dest_ring *)
	    qwx_hal_srng_dst_get_next_entry(sc, srng))) {
		cookie = FIELD_GET(BUFFER_ADDR_INFO1_SW_COOKIE,
		    desc->buf_addr_info.info1);
		idx = FIELD_GET(DP_RXDMA_BUF_COOKIE_BUF_ID, cookie);
		mac_id = FIELD_GET(DP_RXDMA_BUF_COOKIE_PDEV_ID, cookie);

		rx_ring = &pdev_dp->rx_refill_buf_ring;
		if (idx >= rx_ring->bufs_max)
			continue;

		rx_data = &rx_ring->rx_data[idx];
		if (rx_data->m == NULL)
			continue;

		bus_dmamap_unload(sc->sc_dmat, rx_data->map);
		m = rx_data->m;
		rx_data->m = NULL;

		num_buffs_reaped[mac_id]++;

		push_reason = FIELD_GET(HAL_REO_DEST_RING_INFO0_PUSH_REASON,
		    desc->info0);
		if (push_reason !=
		    HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION) {
			m_freem(m);
#if 0
			sc->soc_stats.hal_reo_error[
			    dp->reo_dst_ring[ring_id].ring_id]++;
#endif
			continue;
		}

		rx_data->is_first_msdu = !!(desc->rx_msdu_info.info0 &
		    RX_MSDU_DESC_INFO0_FIRST_MSDU_IN_MPDU);
		rx_data->is_last_msdu = !!(desc->rx_msdu_info.info0 &
		    RX_MSDU_DESC_INFO0_LAST_MSDU_IN_MPDU);
		rx_data->is_continuation = !!(desc->rx_msdu_info.info0 &
		    RX_MSDU_DESC_INFO0_MSDU_CONTINUATION);
		rx_data->peer_id = FIELD_GET(RX_MPDU_DESC_META_DATA_PEER_ID,
		    desc->rx_mpdu_info.meta_data);
		rx_data->seq_no = FIELD_GET(RX_MPDU_DESC_INFO0_SEQ_NUM,
		    desc->rx_mpdu_info.info0);
		rx_data->tid = FIELD_GET(HAL_REO_DEST_RING_INFO0_RX_QUEUE_NUM,
		    desc->info0);

		rx_data->mac_id = mac_id;
		ml_enqueue(&msdu_list[mac_id], m);

		if (rx_data->is_continuation) {
			done = 0;
		} else {
			total_msdu_reaped++;
			done = 1;
		}
	}

	/* Hw might have updated the head pointer after we cached it.
	 * In this case, even though there are entries in the ring we'll
	 * get rx_desc NULL. Give the read another try with updated cached
	 * head pointer so that we can reap complete MPDU in the current
	 * rx processing.
	 */
	if (!done && qwx_hal_srng_dst_num_free(sc, srng, 1)) {
		qwx_hal_srng_access_end(sc, srng);
		goto try_again;
	}

	qwx_hal_srng_access_end(sc, srng);
#ifdef notyet
	spin_unlock_bh(&srng->lock);
#endif
	if (!total_msdu_reaped)
		goto exit;

	for (i = 0; i < sc->num_radios; i++) {
		if (!num_buffs_reaped[i])
			continue;

		qwx_dp_rx_process_received_packets(sc, &msdu_list[i], i);

		rx_ring = &sc->pdev_dp.rx_refill_buf_ring;

		qwx_dp_rxbufs_replenish(sc, i, rx_ring, num_buffs_reaped[i],
		    sc->hw_params.hal_params->rx_buf_rbm);
	}
exit:
	return total_msdu_reaped;
}

int
qwx_dp_rx_process_mon_rings(struct qwx_softc *sc)
{
	return 0;
}

int
qwx_dp_process_rxdma_err(struct qwx_softc *sc)
{
	return 0;
}

int
qwx_dp_process_reo_status(struct qwx_softc *sc)
{
	return 0;
}

int
qwx_dp_service_srng(struct qwx_softc *sc)
{
	struct qwx_pdev_dp *dp = &sc->pdev_dp;
	int i, ret = 0;

	for (i = 0; i < sc->hw_params.max_tx_ring; i++) {
		if (qwx_dp_tx_completion_handler(sc, i))
			ret = 1;
	}

	if (qwx_dp_process_rx_err(sc))
		ret = 1;

	if (qwx_dp_rx_process_wbm_err(sc))
		ret = 1;

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		if (qwx_dp_process_rx(sc, i))
			ret = 1;
	}

	if (qwx_dp_rx_process_mon_rings(sc))
		ret = 1;

	if (qwx_dp_process_reo_status(sc))
		ret = 1;

	if (qwx_dp_process_rxdma_err(sc))
		ret = 1;

	qwx_dp_rxbufs_replenish(sc, dp->mac_id, &dp->rx_refill_buf_ring, 0,
	    sc->hw_params.hal_params->rx_buf_rbm);

	return ret;
}

int
qwx_wmi_wait_for_service_ready(struct qwx_softc *sc)
{
	int ret;

	while (!sc->wmi.service_ready) {
		ret = tsleep_nsec(&sc->wmi.service_ready, 0, "qwxwmirdy",
		    SEC_TO_NSEC(5));
		if (ret)
			return -1;
	}

	return 0;
}

void
qwx_fill_band_to_mac_param(struct qwx_softc *sc,
    struct wmi_host_pdev_band_to_mac *band_to_mac)
{
	uint8_t i;
	struct ath11k_hal_reg_capabilities_ext *hal_reg_cap;
	struct qwx_pdev *pdev;

	for (i = 0; i < sc->num_radios; i++) {
		pdev = &sc->pdevs[i];
		hal_reg_cap = &sc->hal_reg_cap[i];
		band_to_mac[i].pdev_id = pdev->pdev_id;

		switch (pdev->cap.supported_bands) {
		case WMI_HOST_WLAN_2G_5G_CAP:
			band_to_mac[i].start_freq = hal_reg_cap->low_2ghz_chan;
			band_to_mac[i].end_freq = hal_reg_cap->high_5ghz_chan;
			break;
		case WMI_HOST_WLAN_2G_CAP:
			band_to_mac[i].start_freq = hal_reg_cap->low_2ghz_chan;
			band_to_mac[i].end_freq = hal_reg_cap->high_2ghz_chan;
			break;
		case WMI_HOST_WLAN_5G_CAP:
			band_to_mac[i].start_freq = hal_reg_cap->low_5ghz_chan;
			band_to_mac[i].end_freq = hal_reg_cap->high_5ghz_chan;
			break;
		default:
			break;
		}
	}
}

struct mbuf *
qwx_wmi_alloc_mbuf(size_t len)
{
	struct mbuf *m;
	uint32_t round_len = roundup(len, 4);

	m = qwx_htc_alloc_mbuf(sizeof(struct wmi_cmd_hdr) + round_len);
	if (!m)
		return NULL;

	return m;
}

int
qwx_wmi_cmd_send_nowait(struct qwx_pdev_wmi *wmi, struct mbuf *m,
    uint32_t cmd_id)
{
	struct qwx_softc *sc = wmi->wmi->sc;
	struct wmi_cmd_hdr *cmd_hdr;
	uint32_t cmd = 0;

	cmd |= FIELD_PREP(WMI_CMD_HDR_CMD_ID, cmd_id);

	cmd_hdr = (struct wmi_cmd_hdr *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr));
	cmd_hdr->cmd_id = htole32(cmd);

	DNPRINTF(QWX_D_WMI, "%s: sending WMI command 0x%u\n", __func__, cmd);
	return qwx_htc_send(&sc->htc, wmi->eid, m);
}

int
qwx_wmi_cmd_send(struct qwx_pdev_wmi *wmi, struct mbuf *m, uint32_t cmd_id)
{
	struct qwx_wmi_base *wmi_sc = wmi->wmi;
	int ret = EOPNOTSUPP;
	struct qwx_softc *sc = wmi_sc->sc;
#ifdef notyet
	might_sleep();
#endif
	if (sc->hw_params.credit_flow) {
		struct qwx_htc *htc = &sc->htc;
		struct qwx_htc_ep *ep = &htc->endpoint[wmi->eid];

		while (!ep->tx_credits) {
			ret = tsleep_nsec(&ep->tx_credits, 0, "qwxtxcrd",
			    SEC_TO_NSEC(3));
			if (ret) {
				printf("%s: tx credits timeout\n",
				    sc->sc_dev.dv_xname);
				if (test_bit(ATH11K_FLAG_CRASH_FLUSH,
				    sc->sc_flags))
					return ESHUTDOWN;
				else
					return EAGAIN;
			}
		}
	} else {
		while (!wmi->tx_ce_desc) {
			ret = tsleep_nsec(&wmi->tx_ce_desc, 0, "qwxtxce",
			    SEC_TO_NSEC(3));
			if (ret) {
				printf("%s: tx ce desc timeout\n",
				    sc->sc_dev.dv_xname);
				if (test_bit(ATH11K_FLAG_CRASH_FLUSH,
				    sc->sc_flags))
					return ESHUTDOWN;
				else
					return EAGAIN;
			}
		}
	}

	ret = qwx_wmi_cmd_send_nowait(wmi, m, cmd_id);

	if (ret == EAGAIN)
		printf("%s: wmi command %d timeout\n",
		    sc->sc_dev.dv_xname, cmd_id);

	if (ret == ENOBUFS)
		printf("%s: ce desc not available for wmi command %d\n",
		    sc->sc_dev.dv_xname, cmd_id);

	return ret;
}

int
qwx_wmi_pdev_set_param(struct qwx_softc *sc, uint32_t param_id,
    uint32_t param_value, uint8_t pdev_id)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct wmi_pdev_set_param_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_pdev_set_param_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_PARAM_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;
	cmd->param_id = param_id;
	cmd->param_value = param_value;

	ret = qwx_wmi_cmd_send(wmi, m, WMI_PDEV_SET_PARAM_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_PDEV_SET_PARAM cmd\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd pdev set param %d pdev id %d value %d\n",
	    __func__, param_id, pdev_id, param_value);

	return 0;
}

int
qwx_wmi_pdev_lro_cfg(struct qwx_softc *sc, uint8_t pdev_id)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct ath11k_wmi_pdev_lro_config_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct ath11k_wmi_pdev_lro_config_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_LRO_INFO_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	arc4random_buf(cmd->th_4, sizeof(uint32_t) * ATH11K_IPV4_TH_SEED_SIZE);
	arc4random_buf(cmd->th_6, sizeof(uint32_t) * ATH11K_IPV6_TH_SEED_SIZE);

	cmd->pdev_id = pdev_id;

	ret = qwx_wmi_cmd_send(wmi, m, WMI_LRO_CONFIG_CMDID);
	if (ret) {
		printf("%s: failed to send lro cfg req wmi cmd\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd lro config pdev_id 0x%x\n",
	    __func__, pdev_id);

	return 0;
}

int
qwx_wmi_pdev_set_ps_mode(struct qwx_softc *sc, int vdev_id, uint8_t pdev_id,
    enum wmi_sta_ps_mode psmode)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct wmi_pdev_set_ps_mode_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_pdev_set_ps_mode_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
	    WMI_TAG_STA_POWERSAVE_MODE_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->sta_ps_mode = psmode;

	ret = qwx_wmi_cmd_send(wmi, m, WMI_STA_POWERSAVE_MODE_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_PDEV_SET_PARAM cmd\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd sta powersave mode psmode %d vdev id %d\n",
	    __func__, psmode, vdev_id);

	return 0;
}

int
qwx_wmi_scan_prob_req_oui(struct qwx_softc *sc, const uint8_t *mac_addr,
    uint8_t pdev_id)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct mbuf *m;
	struct wmi_scan_prob_req_oui_cmd *cmd;
	uint32_t prob_req_oui;
	int len, ret;

	prob_req_oui = (((uint32_t)mac_addr[0]) << 16) |
		       (((uint32_t)mac_addr[1]) << 8) | mac_addr[2];

	len = sizeof(*cmd);
	m = qwx_wmi_alloc_mbuf(len);
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_scan_prob_req_oui_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
	    WMI_TAG_SCAN_PROB_REQ_OUI_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->prob_req_oui = prob_req_oui;

	DNPRINTF(QWX_D_WMI, "%s: scan prob req oui %d\n", __func__,
	    prob_req_oui);

	ret = qwx_wmi_cmd_send(wmi, m, WMI_SCAN_PROB_REQ_OUI_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_SCAN_PROB_REQ_OUI cmd\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	return 0;
}

int
qwx_wmi_send_dfs_phyerr_offload_enable_cmd(struct qwx_softc *sc, uint32_t pdev_id)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct wmi_dfs_phyerr_offload_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_dfs_phyerr_offload_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
	    WMI_TAG_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = pdev_id;

	ret = qwx_wmi_cmd_send(wmi, m,
	    WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE "
		    "cmd\n", sc->sc_dev.dv_xname);
		m_free(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd pdev dfs phyerr offload enable "
	    "pdev id %d\n", __func__, pdev_id);

	return 0;
}

int
qwx_wmi_send_scan_chan_list_cmd(struct qwx_softc *sc, uint8_t pdev_id,
    struct scan_chan_list_params *chan_list)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct wmi_scan_chan_list_cmd *cmd;
	struct mbuf *m;
	struct wmi_channel *chan_info;
	struct channel_param *tchan_info;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	uint16_t num_send_chans, num_sends = 0, max_chan_limit = 0;
	uint32_t *reg1, *reg2;

	tchan_info = chan_list->ch_param;
	while (chan_list->nallchans) {
		len = sizeof(*cmd) + TLV_HDR_SIZE;
		max_chan_limit = (wmi->wmi->max_msg_len[pdev_id] - len) /
		    sizeof(*chan_info);

		if (chan_list->nallchans > max_chan_limit)
			num_send_chans = max_chan_limit;
		else
			num_send_chans = chan_list->nallchans;

		chan_list->nallchans -= num_send_chans;
		len += sizeof(*chan_info) * num_send_chans;

		m = qwx_wmi_alloc_mbuf(len);
		if (!m)
			return ENOMEM;

		cmd = (struct wmi_scan_chan_list_cmd *)(mtod(m, uint8_t *) +
		    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
		cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
		    WMI_TAG_SCAN_CHAN_LIST_CMD) |
		    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
		cmd->pdev_id = chan_list->pdev_id;
		cmd->num_scan_chans = num_send_chans;
		if (num_sends)
			cmd->flags |= WMI_APPEND_TO_EXISTING_CHAN_LIST_FLAG;

		DNPRINTF(QWX_D_WMI, "%s: no.of chan = %d len = %d "
		    "pdev_id = %d num_sends = %d\n", __func__, num_send_chans,
		    len, cmd->pdev_id, num_sends);

		ptr = (void *)(mtod(m, uint8_t *) +
		    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr) +
		    sizeof(*cmd));

		len = sizeof(*chan_info) * num_send_chans;
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		    FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < num_send_chans; ++i) {
			chan_info = ptr;
			memset(chan_info, 0, sizeof(*chan_info));
			len = sizeof(*chan_info);
			chan_info->tlv_header = FIELD_PREP(WMI_TLV_TAG,
			    WMI_TAG_CHANNEL) |
			    FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

			reg1 = &chan_info->reg_info_1;
			reg2 = &chan_info->reg_info_2;
			chan_info->mhz = tchan_info->mhz;
			chan_info->band_center_freq1 = tchan_info->cfreq1;
			chan_info->band_center_freq2 = tchan_info->cfreq2;

			if (tchan_info->is_chan_passive)
				chan_info->info |= WMI_CHAN_INFO_PASSIVE;
			if (tchan_info->allow_he)
				chan_info->info |= WMI_CHAN_INFO_ALLOW_HE;
			else if (tchan_info->allow_vht)
				chan_info->info |= WMI_CHAN_INFO_ALLOW_VHT;
			else if (tchan_info->allow_ht)
				chan_info->info |= WMI_CHAN_INFO_ALLOW_HT;
			if (tchan_info->half_rate)
				chan_info->info |= WMI_CHAN_INFO_HALF_RATE;
			if (tchan_info->quarter_rate)
				chan_info->info |= WMI_CHAN_INFO_QUARTER_RATE;
			if (tchan_info->psc_channel)
				chan_info->info |= WMI_CHAN_INFO_PSC;
			if (tchan_info->dfs_set)
				chan_info->info |= WMI_CHAN_INFO_DFS;

			chan_info->info |= FIELD_PREP(WMI_CHAN_INFO_MODE,
			    tchan_info->phy_mode);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_MIN_PWR,
			    tchan_info->minpower);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_MAX_PWR,
			    tchan_info->maxpower);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_MAX_REG_PWR,
			    tchan_info->maxregpower);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_REG_CLS,
			    tchan_info->reg_class_id);
			*reg2 |= FIELD_PREP(WMI_CHAN_REG_INFO2_ANT_MAX,
			    tchan_info->antennamax);
			*reg2 |= FIELD_PREP(WMI_CHAN_REG_INFO2_MAX_TX_PWR,
			    tchan_info->maxregpower);

			DNPRINTF(QWX_D_WMI, "%s: chan scan list "
			    "chan[%d] = %u, chan_info->info %8x\n",
			    __func__, i, chan_info->mhz, chan_info->info);

			ptr += sizeof(*chan_info);

			tchan_info++;
		}

		ret = qwx_wmi_cmd_send(wmi, m, WMI_SCAN_CHAN_LIST_CMDID);
		if (ret) {
			printf("%s: failed to send WMI_SCAN_CHAN_LIST cmd\n",
			    sc->sc_dev.dv_xname);
			m_freem(m);
			return ret;
		}

		DNPRINTF(QWX_D_WMI, "%s: cmd scan chan list channels %d\n",
		    __func__, num_send_chans);

		num_sends++;
	}

	return 0;
}

int
qwx_wmi_send_11d_scan_start_cmd(struct qwx_softc *sc,
    struct wmi_11d_scan_start_params *param, uint8_t pdev_id)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct wmi_11d_scan_start_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_11d_scan_start_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_11D_SCAN_START_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->vdev_id;
	cmd->scan_period_msec = param->scan_period_msec;
	cmd->start_interval_msec = param->start_interval_msec;

	ret = qwx_wmi_cmd_send(wmi, m, WMI_11D_SCAN_START_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_11D_SCAN_START_CMDID: %d\n",
		    sc->sc_dev.dv_xname, ret);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd 11d scan start vdev id %d period %d "
	    "ms internal %d ms\n", __func__, cmd->vdev_id,
	    cmd->scan_period_msec, cmd->start_interval_msec);

	return 0;
}

static inline void
qwx_wmi_copy_scan_event_cntrl_flags(struct wmi_start_scan_cmd *cmd,
    struct scan_req_params *param)
{
	/* Scan events subscription */
	if (param->scan_ev_started)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_STARTED;
	if (param->scan_ev_completed)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_COMPLETED;
	if (param->scan_ev_bss_chan)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_BSS_CHANNEL;
	if (param->scan_ev_foreign_chan)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_FOREIGN_CHAN;
	if (param->scan_ev_dequeued)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_DEQUEUED;
	if (param->scan_ev_preempted)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_PREEMPTED;
	if (param->scan_ev_start_failed)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_START_FAILED;
	if (param->scan_ev_restarted)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_RESTARTED;
	if (param->scan_ev_foreign_chn_exit)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT;
	if (param->scan_ev_suspended)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_SUSPENDED;
	if (param->scan_ev_resumed)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_RESUMED;

	/** Set scan control flags */
	cmd->scan_ctrl_flags = 0;
	if (param->scan_f_passive)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_PASSIVE;
	if (param->scan_f_strict_passive_pch)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_STRICT_PASSIVE_ON_PCHN;
	if (param->scan_f_promisc_mode)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FILTER_PROMISCUOS;
	if (param->scan_f_capture_phy_err)
		cmd->scan_ctrl_flags |=  WMI_SCAN_CAPTURE_PHY_ERROR;
	if (param->scan_f_half_rate)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_HALF_RATE_SUPPORT;
	if (param->scan_f_quarter_rate)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_QUARTER_RATE_SUPPORT;
	if (param->scan_f_cck_rates)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_CCK_RATES;
	if (param->scan_f_ofdm_rates)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_OFDM_RATES;
	if (param->scan_f_chan_stat_evnt)
		cmd->scan_ctrl_flags |=  WMI_SCAN_CHAN_STAT_EVENT;
	if (param->scan_f_filter_prb_req)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FILTER_PROBE_REQ;
	if (param->scan_f_bcast_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_BCAST_PROBE_REQ;
	if (param->scan_f_offchan_mgmt_tx)
		cmd->scan_ctrl_flags |=  WMI_SCAN_OFFCHAN_MGMT_TX;
	if (param->scan_f_offchan_data_tx)
		cmd->scan_ctrl_flags |=  WMI_SCAN_OFFCHAN_DATA_TX;
	if (param->scan_f_force_active_dfs_chn)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_FORCE_ACTIVE_ON_DFS;
	if (param->scan_f_add_tpc_ie_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_TPC_IE_IN_PROBE_REQ;
	if (param->scan_f_add_ds_ie_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_DS_IE_IN_PROBE_REQ;
	if (param->scan_f_add_spoofed_mac_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_SPOOF_MAC_IN_PROBE_REQ;
	if (param->scan_f_add_rand_seq_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_RANDOM_SEQ_NO_IN_PROBE_REQ;
	if (param->scan_f_en_ie_whitelist_in_probe)
		cmd->scan_ctrl_flags |=
			 WMI_SCAN_ENABLE_IE_WHTELIST_IN_PROBE_REQ;

	/* for adaptive scan mode using 3 bits (21 - 23 bits) */
	WMI_SCAN_SET_DWELL_MODE(cmd->scan_ctrl_flags,
	    param->adaptive_dwell_time_mode);

	cmd->scan_ctrl_flags_ext = param->scan_ctrl_flags_ext;
}

int
qwx_wmi_send_scan_start_cmd(struct qwx_softc *sc,
    struct scan_req_params *params)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[params->pdev_id];
	struct wmi_start_scan_cmd *cmd;
	struct wmi_ssid *ssid = NULL;
	struct wmi_mac_addr *bssid;
	struct mbuf *m;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	uint32_t *tmp_ptr;
	uint16_t extraie_len_with_pad = 0;
	struct hint_short_ssid *s_ssid = NULL;
	struct hint_bssid *hint_bssid = NULL;

	len = sizeof(*cmd);

	len += TLV_HDR_SIZE;
	if (params->num_chan)
		len += params->num_chan * sizeof(uint32_t);

	len += TLV_HDR_SIZE;
	if (params->num_ssids)
		len += params->num_ssids * sizeof(*ssid);

	len += TLV_HDR_SIZE;
	if (params->num_bssid)
		len += sizeof(*bssid) * params->num_bssid;

	len += TLV_HDR_SIZE;
	if (params->extraie.len && params->extraie.len <= 0xFFFF) {
		extraie_len_with_pad = roundup(params->extraie.len,
		    sizeof(uint32_t));
	}
	len += extraie_len_with_pad;

	if (params->num_hint_bssid) {
		len += TLV_HDR_SIZE +
		    params->num_hint_bssid * sizeof(struct hint_bssid);
	}

	if (params->num_hint_s_ssid) {
		len += TLV_HDR_SIZE +
		    params->num_hint_s_ssid * sizeof(struct hint_short_ssid);
	}

	m = qwx_wmi_alloc_mbuf(len);
	if (!m)
		return ENOMEM;

	ptr = (void *)(mtod(m, uint8_t *) + sizeof(struct ath11k_htc_hdr) +
	    sizeof(struct wmi_cmd_hdr));

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_START_SCAN_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->scan_id = params->scan_id;
	cmd->scan_req_id = params->scan_req_id;
	cmd->vdev_id = params->vdev_id;
	cmd->scan_priority = params->scan_priority;
	cmd->notify_scan_events = params->notify_scan_events;

	qwx_wmi_copy_scan_event_cntrl_flags(cmd, params);

	cmd->dwell_time_active = params->dwell_time_active;
	cmd->dwell_time_active_2g = params->dwell_time_active_2g;
	cmd->dwell_time_passive = params->dwell_time_passive;
	cmd->dwell_time_active_6g = params->dwell_time_active_6g;
	cmd->dwell_time_passive_6g = params->dwell_time_passive_6g;
	cmd->min_rest_time = params->min_rest_time;
	cmd->max_rest_time = params->max_rest_time;
	cmd->repeat_probe_time = params->repeat_probe_time;
	cmd->probe_spacing_time = params->probe_spacing_time;
	cmd->idle_time = params->idle_time;
	cmd->max_scan_time = params->max_scan_time;
	cmd->probe_delay = params->probe_delay;
	cmd->burst_duration = params->burst_duration;
	cmd->num_chan = params->num_chan;
	cmd->num_bssid = params->num_bssid;
	cmd->num_ssids = params->num_ssids;
	cmd->ie_len = params->extraie.len;
	cmd->n_probes = params->n_probes;
	IEEE80211_ADDR_COPY(cmd->mac_addr.addr, params->mac_addr.addr);
	IEEE80211_ADDR_COPY(cmd->mac_mask.addr, params->mac_mask.addr);

	ptr += sizeof(*cmd);

	len = params->num_chan * sizeof(uint32_t);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
	    FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;
	tmp_ptr = (uint32_t *)ptr;

	for (i = 0; i < params->num_chan; ++i)
		tmp_ptr[i] = params->chan_list[i];

	ptr += len;

	len = params->num_ssids * sizeof(*ssid);
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_FIXED_STRUCT) |
	    FIELD_PREP(WMI_TLV_LEN, len);

	ptr += TLV_HDR_SIZE;

	if (params->num_ssids) {
		ssid = ptr;
		for (i = 0; i < params->num_ssids; ++i) {
			ssid->ssid_len = params->ssid[i].length;
			memcpy(ssid->ssid, params->ssid[i].ssid,
			       params->ssid[i].length);
			ssid++;
		}
	}

	ptr += (params->num_ssids * sizeof(*ssid));
	len = params->num_bssid * sizeof(*bssid);
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_FIXED_STRUCT) |
	    FIELD_PREP(WMI_TLV_LEN, len);

	ptr += TLV_HDR_SIZE;
	bssid = ptr;

	if (params->num_bssid) {
		for (i = 0; i < params->num_bssid; ++i) {
			IEEE80211_ADDR_COPY(bssid->addr,
			    params->bssid_list[i].addr);
			bssid++;
		}
	}

	ptr += params->num_bssid * sizeof(*bssid);

	len = extraie_len_with_pad;
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
	    FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	if (extraie_len_with_pad)
		memcpy(ptr, params->extraie.ptr, params->extraie.len);

	ptr += extraie_len_with_pad;

	if (params->num_hint_s_ssid) {
		len = params->num_hint_s_ssid * sizeof(struct hint_short_ssid);
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG,
		    WMI_TAG_ARRAY_FIXED_STRUCT) |
		    FIELD_PREP(WMI_TLV_LEN, len);
		ptr += TLV_HDR_SIZE;
		s_ssid = ptr;
		for (i = 0; i < params->num_hint_s_ssid; ++i) {
			s_ssid->freq_flags = params->hint_s_ssid[i].freq_flags;
			s_ssid->short_ssid = params->hint_s_ssid[i].short_ssid;
			s_ssid++;
		}
		ptr += len;
	}

	if (params->num_hint_bssid) {
		len = params->num_hint_bssid * sizeof(struct hint_bssid);
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG,
		    WMI_TAG_ARRAY_FIXED_STRUCT) |
		    FIELD_PREP(WMI_TLV_LEN, len);
		ptr += TLV_HDR_SIZE;
		hint_bssid = ptr;
		for (i = 0; i < params->num_hint_bssid; ++i) {
			hint_bssid->freq_flags =
				params->hint_bssid[i].freq_flags;
			IEEE80211_ADDR_COPY(
			    &params->hint_bssid[i].bssid.addr[0],
			    &hint_bssid->bssid.addr[0]);
			hint_bssid++;
		}
	}

	ret = qwx_wmi_cmd_send(wmi, m, WMI_START_SCAN_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_START_SCAN_CMDID\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd start scan", __func__);

	return 0;
}

int
qwx_wmi_send_scan_stop_cmd(struct qwx_softc *sc,
    struct scan_cancel_param *param)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[param->pdev_id];
	struct wmi_stop_scan_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_stop_scan_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_STOP_SCAN_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->vdev_id;
	cmd->requestor = param->requester;
	cmd->scan_id = param->scan_id;
	cmd->pdev_id = param->pdev_id;
	/* stop the scan with the corresponding scan_id */
	if (param->req_type == WLAN_SCAN_CANCEL_PDEV_ALL) {
		/* Cancelling all scans */
		cmd->req_type =  WMI_SCAN_STOP_ALL;
	} else if (param->req_type == WLAN_SCAN_CANCEL_VDEV_ALL) {
		/* Cancelling VAP scans */
		cmd->req_type =  WMI_SCN_STOP_VAP_ALL;
	} else if (param->req_type == WLAN_SCAN_CANCEL_SINGLE) {
		/* Cancelling specific scan */
		cmd->req_type =  WMI_SCAN_STOP_ONE;
	} else {
		printf("%s: invalid scan cancel param %d\n",
		    sc->sc_dev.dv_xname, param->req_type);
		m_freem(m);
		return EINVAL;
	}

	ret = qwx_wmi_cmd_send(wmi, m, WMI_STOP_SCAN_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_STOP_SCAN_CMDID\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd stop scan\n", __func__);
	return ret;
}

void
qwx_wmi_copy_resource_config(struct wmi_resource_config *wmi_cfg,
    struct target_resource_config *tg_cfg)
{
	wmi_cfg->num_vdevs = tg_cfg->num_vdevs;
	wmi_cfg->num_peers = tg_cfg->num_peers;
	wmi_cfg->num_offload_peers = tg_cfg->num_offload_peers;
	wmi_cfg->num_offload_reorder_buffs = tg_cfg->num_offload_reorder_buffs;
	wmi_cfg->num_peer_keys = tg_cfg->num_peer_keys;
	wmi_cfg->num_tids = tg_cfg->num_tids;
	wmi_cfg->ast_skid_limit = tg_cfg->ast_skid_limit;
	wmi_cfg->tx_chain_mask = tg_cfg->tx_chain_mask;
	wmi_cfg->rx_chain_mask = tg_cfg->rx_chain_mask;
	wmi_cfg->rx_timeout_pri[0] = tg_cfg->rx_timeout_pri[0];
	wmi_cfg->rx_timeout_pri[1] = tg_cfg->rx_timeout_pri[1];
	wmi_cfg->rx_timeout_pri[2] = tg_cfg->rx_timeout_pri[2];
	wmi_cfg->rx_timeout_pri[3] = tg_cfg->rx_timeout_pri[3];
	wmi_cfg->rx_decap_mode = tg_cfg->rx_decap_mode;
	wmi_cfg->scan_max_pending_req = tg_cfg->scan_max_pending_req;
	wmi_cfg->bmiss_offload_max_vdev = tg_cfg->bmiss_offload_max_vdev;
	wmi_cfg->roam_offload_max_vdev = tg_cfg->roam_offload_max_vdev;
	wmi_cfg->roam_offload_max_ap_profiles =
	    tg_cfg->roam_offload_max_ap_profiles;
	wmi_cfg->num_mcast_groups = tg_cfg->num_mcast_groups;
	wmi_cfg->num_mcast_table_elems = tg_cfg->num_mcast_table_elems;
	wmi_cfg->mcast2ucast_mode = tg_cfg->mcast2ucast_mode;
	wmi_cfg->tx_dbg_log_size = tg_cfg->tx_dbg_log_size;
	wmi_cfg->num_wds_entries = tg_cfg->num_wds_entries;
	wmi_cfg->dma_burst_size = tg_cfg->dma_burst_size;
	wmi_cfg->mac_aggr_delim = tg_cfg->mac_aggr_delim;
	wmi_cfg->rx_skip_defrag_timeout_dup_detection_check =
	    tg_cfg->rx_skip_defrag_timeout_dup_detection_check;
	wmi_cfg->vow_config = tg_cfg->vow_config;
	wmi_cfg->gtk_offload_max_vdev = tg_cfg->gtk_offload_max_vdev;
	wmi_cfg->num_msdu_desc = tg_cfg->num_msdu_desc;
	wmi_cfg->max_frag_entries = tg_cfg->max_frag_entries;
	wmi_cfg->num_tdls_vdevs = tg_cfg->num_tdls_vdevs;
	wmi_cfg->num_tdls_conn_table_entries =
	    tg_cfg->num_tdls_conn_table_entries;
	wmi_cfg->beacon_tx_offload_max_vdev =
	    tg_cfg->beacon_tx_offload_max_vdev;
	wmi_cfg->num_multicast_filter_entries =
	    tg_cfg->num_multicast_filter_entries;
	wmi_cfg->num_wow_filters = tg_cfg->num_wow_filters;
	wmi_cfg->num_keep_alive_pattern = tg_cfg->num_keep_alive_pattern;
	wmi_cfg->keep_alive_pattern_size = tg_cfg->keep_alive_pattern_size;
	wmi_cfg->max_tdls_concurrent_sleep_sta =
	    tg_cfg->max_tdls_concurrent_sleep_sta;
	wmi_cfg->max_tdls_concurrent_buffer_sta =
	    tg_cfg->max_tdls_concurrent_buffer_sta;
	wmi_cfg->wmi_send_separate = tg_cfg->wmi_send_separate;
	wmi_cfg->num_ocb_vdevs = tg_cfg->num_ocb_vdevs;
	wmi_cfg->num_ocb_channels = tg_cfg->num_ocb_channels;
	wmi_cfg->num_ocb_schedules = tg_cfg->num_ocb_schedules;
	wmi_cfg->bpf_instruction_size = tg_cfg->bpf_instruction_size;
	wmi_cfg->max_bssid_rx_filters = tg_cfg->max_bssid_rx_filters;
	wmi_cfg->use_pdev_id = tg_cfg->use_pdev_id;
	wmi_cfg->flag1 = tg_cfg->flag1;
	wmi_cfg->peer_map_unmap_v2_support = tg_cfg->peer_map_unmap_v2_support;
	wmi_cfg->sched_params = tg_cfg->sched_params;
	wmi_cfg->twt_ap_pdev_count = tg_cfg->twt_ap_pdev_count;
	wmi_cfg->twt_ap_sta_count = tg_cfg->twt_ap_sta_count;
#ifdef notyet /* 6 GHz support */
	wmi_cfg->host_service_flags &=
	    ~(1 << WMI_CFG_HOST_SERVICE_FLAG_REG_CC_EXT);
	wmi_cfg->host_service_flags |= (tg_cfg->is_reg_cc_ext_event_supported <<
	    WMI_CFG_HOST_SERVICE_FLAG_REG_CC_EXT);
	wmi_cfg->flags2 = WMI_RSRC_CFG_FLAG2_CALC_NEXT_DTIM_COUNT_SET;
	wmi_cfg->ema_max_vap_cnt = tg_cfg->ema_max_vap_cnt;
	wmi_cfg->ema_max_profile_period = tg_cfg->ema_max_profile_period;
#endif
}

int
qwx_init_cmd_send(struct qwx_pdev_wmi *wmi, struct wmi_init_cmd_param *param)
{
	struct mbuf *m;
	struct wmi_init_cmd *cmd;
	struct wmi_resource_config *cfg;
	struct wmi_pdev_set_hw_mode_cmd_param *hw_mode;
	struct wmi_pdev_band_to_mac *band_to_mac;
	struct wlan_host_mem_chunk *host_mem_chunks;
	struct wmi_tlv *tlv;
	size_t ret, len;
	void *ptr;
	uint32_t hw_mode_len = 0;
	uint16_t idx;

	if (param->hw_mode_id != WMI_HOST_HW_MODE_MAX)
		hw_mode_len = sizeof(*hw_mode) + TLV_HDR_SIZE +
		    (param->num_band_to_mac * sizeof(*band_to_mac));

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*cfg) + hw_mode_len +
	    (param->num_mem_chunks ?
	    (sizeof(*host_mem_chunks) * WMI_MAX_MEM_REQS) : 0);

	m = qwx_wmi_alloc_mbuf(len);
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_init_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_INIT_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ptr = mtod(m, uint8_t *) + sizeof(struct ath11k_htc_hdr) +
	   sizeof(struct wmi_cmd_hdr) + sizeof(*cmd);
	cfg = ptr;

	qwx_wmi_copy_resource_config(cfg, param->res_cfg);

	cfg->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_RESOURCE_CONFIG) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cfg) - TLV_HDR_SIZE);

	ptr += sizeof(*cfg);
	host_mem_chunks = ptr + TLV_HDR_SIZE;
	len = sizeof(struct wlan_host_mem_chunk);

	for (idx = 0; idx < param->num_mem_chunks; ++idx) {
		host_mem_chunks[idx].tlv_header =
		    FIELD_PREP(WMI_TLV_TAG, WMI_TAG_WLAN_HOST_MEMORY_CHUNK) |
		    FIELD_PREP(WMI_TLV_LEN, len);

		host_mem_chunks[idx].ptr = param->mem_chunks[idx].paddr;
		host_mem_chunks[idx].size = param->mem_chunks[idx].len;
		host_mem_chunks[idx].req_id = param->mem_chunks[idx].req_id;

		DNPRINTF(QWX_D_WMI,
		    "%s: host mem chunk req_id %d paddr 0x%llx len %d\n",
		    __func__, param->mem_chunks[idx].req_id,
		    (uint64_t)param->mem_chunks[idx].paddr,
		    param->mem_chunks[idx].len);
	}
	cmd->num_host_mem_chunks = param->num_mem_chunks;
	len = sizeof(struct wlan_host_mem_chunk) * param->num_mem_chunks;

	/* num_mem_chunks is zero */
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
	    FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE + len;

	if (param->hw_mode_id != WMI_HOST_HW_MODE_MAX) {
		hw_mode = (struct wmi_pdev_set_hw_mode_cmd_param *)ptr;
		hw_mode->tlv_header = FIELD_PREP(WMI_TLV_TAG,
		    WMI_TAG_PDEV_SET_HW_MODE_CMD) |
		    FIELD_PREP(WMI_TLV_LEN, sizeof(*hw_mode) - TLV_HDR_SIZE);

		hw_mode->hw_mode_index = param->hw_mode_id;
		hw_mode->num_band_to_mac = param->num_band_to_mac;

		ptr += sizeof(*hw_mode);

		len = param->num_band_to_mac * sizeof(*band_to_mac);
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		    FIELD_PREP(WMI_TLV_LEN, len);

		ptr += TLV_HDR_SIZE;
		len = sizeof(*band_to_mac);

		for (idx = 0; idx < param->num_band_to_mac; idx++) {
			band_to_mac = (void *)ptr;

			band_to_mac->tlv_header = FIELD_PREP(WMI_TLV_TAG,
			    WMI_TAG_PDEV_BAND_TO_MAC) |
			    FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
			band_to_mac->pdev_id = param->band_to_mac[idx].pdev_id;
			band_to_mac->start_freq =
			    param->band_to_mac[idx].start_freq;
			band_to_mac->end_freq =
			    param->band_to_mac[idx].end_freq;
			ptr += sizeof(*band_to_mac);
		}
	}

	ret = qwx_wmi_cmd_send(wmi, m, WMI_INIT_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_INIT_CMDID\n", __func__);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd wmi init\n", __func__);

	return 0;
}

int
qwx_wmi_cmd_init(struct qwx_softc *sc)
{
	struct qwx_wmi_base *wmi_sc = &sc->wmi;
	struct wmi_init_cmd_param init_param;
	struct target_resource_config  config;

	memset(&init_param, 0, sizeof(init_param));
	memset(&config, 0, sizeof(config));

	sc->hw_params.hw_ops->wmi_init_config(sc, &config);

	if (isset(sc->wmi.svc_map, WMI_TLV_SERVICE_REG_CC_EXT_EVENT_SUPPORT))
		config.is_reg_cc_ext_event_supported = 1;

	memcpy(&wmi_sc->wlan_resource_config, &config, sizeof(config));

	init_param.res_cfg = &wmi_sc->wlan_resource_config;
	init_param.num_mem_chunks = wmi_sc->num_mem_chunks;
	init_param.hw_mode_id = wmi_sc->preferred_hw_mode;
	init_param.mem_chunks = wmi_sc->mem_chunks;

	if (sc->hw_params.single_pdev_only)
		init_param.hw_mode_id = WMI_HOST_HW_MODE_MAX;

	init_param.num_band_to_mac = sc->num_radios;
	qwx_fill_band_to_mac_param(sc, init_param.band_to_mac);

	return qwx_init_cmd_send(&wmi_sc->wmi[0], &init_param);
}

int
qwx_wmi_wait_for_unified_ready(struct qwx_softc *sc)
{
	int ret;

	while (!sc->wmi.unified_ready) {
		ret = tsleep_nsec(&sc->wmi.unified_ready, 0, "qwxunfrdy",
		    SEC_TO_NSEC(5));
		if (ret)
			return -1;
	}

	return 0;
}

int
qwx_wmi_set_hw_mode(struct qwx_softc *sc,
    enum wmi_host_hw_mode_config_type mode)
{
	struct wmi_pdev_set_hw_mode_cmd_param *cmd;
	struct mbuf *m;
	struct qwx_wmi_base *wmi = &sc->wmi;
	int len;
	int ret;

	len = sizeof(*cmd);

	m = qwx_wmi_alloc_mbuf(len);
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_pdev_set_hw_mode_cmd_param *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_HW_MODE_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = WMI_PDEV_ID_SOC;
	cmd->hw_mode_index = mode;

	ret = qwx_wmi_cmd_send(&wmi->wmi[0], m, WMI_PDEV_SET_HW_MODE_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_PDEV_SET_HW_MODE_CMDID\n",
		    __func__);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd pdev set hw mode %d\n", __func__,
	    cmd->hw_mode_index);

	return 0;
}

int
qwx_wmi_set_sta_ps_param(struct qwx_softc *sc, uint32_t vdev_id,
     uint8_t pdev_id, uint32_t param, uint32_t param_value)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct wmi_sta_powersave_param_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_sta_powersave_param_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
	    WMI_TAG_STA_POWERSAVE_PARAM_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->param = param;
	cmd->value = param_value;

	ret = qwx_wmi_cmd_send(wmi, m, WMI_STA_POWERSAVE_PARAM_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_STA_POWERSAVE_PARAM_CMDID",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd set powersave param vdev_id %d param %d "
	    "value %d\n", __func__, vdev_id, param, param_value);

	return 0;
}

int
qwx_wmi_vdev_create(struct qwx_softc *sc, uint8_t *macaddr,
    struct vdev_create_params *param)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[param->pdev_id];
	struct wmi_vdev_create_cmd *cmd;
	struct mbuf *m;
	struct wmi_vdev_txrx_streams *txrx_streams;
	struct wmi_tlv *tlv;
	int ret, len;
	void *ptr;

	/* It can be optimized my sending tx/rx chain configuration
	 * only for supported bands instead of always sending it for
	 * both the bands.
	 */
	len = sizeof(*cmd) + TLV_HDR_SIZE +
		(WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams));

	m = qwx_wmi_alloc_mbuf(len);
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_vdev_create_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_CREATE_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->if_id;
	cmd->vdev_type = param->type;
	cmd->vdev_subtype = param->subtype;
	cmd->num_cfg_txrx_streams = WMI_NUM_SUPPORTED_BAND_MAX;
	cmd->pdev_id = param->pdev_id;
	cmd->mbssid_flags = param->mbssid_flags;
	cmd->mbssid_tx_vdev_id = param->mbssid_tx_vdev_id;

	IEEE80211_ADDR_COPY(cmd->vdev_macaddr.addr, macaddr);

	ptr = (void *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr) +
	    sizeof(*cmd));
	len = WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
	    FIELD_PREP(WMI_TLV_LEN, len);

	ptr += TLV_HDR_SIZE;
	txrx_streams = ptr;
	len = sizeof(*txrx_streams);
	txrx_streams->tlv_header =
	    FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_TXRX_STREAMS) |
	    FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	txrx_streams->band = WMI_TPC_CHAINMASK_CONFIG_BAND_2G;
	txrx_streams->supported_tx_streams = param->chains[0].tx;
	txrx_streams->supported_rx_streams = param->chains[0].rx;

	txrx_streams++;
	txrx_streams->tlv_header =
	    FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_TXRX_STREAMS) |
	    FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	txrx_streams->band = WMI_TPC_CHAINMASK_CONFIG_BAND_5G;
	txrx_streams->supported_tx_streams = param->chains[1].tx;
	txrx_streams->supported_rx_streams = param->chains[1].rx;

	ret = qwx_wmi_cmd_send(wmi, m, WMI_VDEV_CREATE_CMDID);
	if (ret) {
		printf("%s: failed to submit WMI_VDEV_CREATE_CMDID\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd vdev create id %d type %d subtype %d "
	    "macaddr %s pdevid %d\n", __func__, param->if_id, param->type,
	    param->subtype, ether_sprintf(macaddr), param->pdev_id);

	return ret;
}

int
qwx_wmi_vdev_set_param_cmd(struct qwx_softc *sc, uint32_t vdev_id,
    uint8_t pdev_id, uint32_t param_id, uint32_t param_value)
{
	struct qwx_pdev_wmi *wmi = &sc->wmi.wmi[pdev_id];
	struct wmi_vdev_set_param_cmd *cmd;
	struct mbuf *m;
	int ret;

	m = qwx_wmi_alloc_mbuf(sizeof(*cmd));
	if (!m)
		return ENOMEM;

	cmd = (struct wmi_vdev_set_param_cmd *)(mtod(m, uint8_t *) +
	    sizeof(struct ath11k_htc_hdr) + sizeof(struct wmi_cmd_hdr));
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_SET_PARAM_CMD) |
	    FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->param_id = param_id;
	cmd->param_value = param_value;

	ret = qwx_wmi_cmd_send(wmi, m, WMI_VDEV_SET_PARAM_CMDID);
	if (ret) {
		printf("%s: failed to send WMI_VDEV_SET_PARAM_CMDID\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return ret;
	}

	DNPRINTF(QWX_D_WMI, "%s: cmd vdev set param vdev 0x%x param %d "
	    "value %d\n", __func__, vdev_id, param_id, param_value);

	return 0;
}

int
qwx_core_start(struct qwx_softc *sc)
{
	int ret;

	ret = qwx_wmi_attach(sc);
	if (ret) {
		printf("%s: failed to attach wmi: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_htc_init(sc);
	if (ret) {
		printf("%s: failed to init htc: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_wmi_detach;
	}

	ret = sc->ops.start(sc);
	if (ret) {
		printf("%s: failed to start host interface: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_wmi_detach;
	}

	ret = qwx_htc_wait_target(sc);
	if (ret) {
		printf("%s: failed to connect to HTC: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_hif_stop;
	}

	ret = qwx_dp_htt_connect(&sc->dp);
	if (ret) {
		printf("%s: failed to connect to HTT: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_hif_stop;
	}

	ret = qwx_wmi_connect(sc);
	if (ret) {
		printf("%s: failed to connect wmi: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_hif_stop;
	}

	sc->wmi.service_ready = 0;

	ret = qwx_htc_start(&sc->htc);
	if (ret) {
		printf("%s: failed to start HTC: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_hif_stop;
	}

	ret = qwx_wmi_wait_for_service_ready(sc);
	if (ret) {
		printf("%s: failed to receive wmi service ready event: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_hif_stop;
	}
#if 0
	ret = ath11k_mac_allocate(ab);
	if (ret) {
		ath11k_err(ab, "failed to create new hw device with mac80211 :%d\n",
			   ret);
		goto err_hif_stop;
	}
	ath11k_dp_pdev_pre_alloc(sc);
#endif
	ret = qwx_dp_pdev_reo_setup(sc);
	if (ret) {
		printf("%s: failed to initialize reo destination rings: %d\n",
		    __func__, ret);
		goto err_mac_destroy;
	}

	ret = qwx_wmi_cmd_init(sc);
	if (ret) {
		printf("%s: failed to send wmi init cmd: %d\n", __func__, ret);
		goto err_reo_cleanup;
	}

	ret = qwx_wmi_wait_for_unified_ready(sc);
	if (ret) {
		printf("%s: failed to receive wmi unified ready event: %d\n",
		    __func__, ret);
		goto err_reo_cleanup;
	}

	/* put hardware to DBS mode */
	if (sc->hw_params.single_pdev_only &&
	    sc->hw_params.num_rxmda_per_pdev > 1) {
		ret = qwx_wmi_set_hw_mode(sc, WMI_HOST_HW_MODE_DBS);
		if (ret) {
			printf("%s: failed to send dbs mode: %d\n",
			    __func__, ret);
			goto err_hif_stop;
		}
	}

	ret = qwx_dp_tx_htt_h2t_ver_req_msg(sc);
	if (ret) {
		if (ret != ENOTSUP) {
			printf("%s: failed to send htt version "
			    "request message: %d\n", __func__, ret);
		}
		goto err_reo_cleanup;
	}

	return 0;
err_reo_cleanup:
	qwx_dp_pdev_reo_cleanup(sc);
err_mac_destroy:
#if 0
	ath11k_mac_destroy(ab);
#endif
err_hif_stop:
	sc->ops.stop(sc);
err_wmi_detach:
	qwx_wmi_detach(sc);
	return ret;
}

void
qwx_core_stop(struct qwx_softc *sc)
{
	if (!test_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags))
		qwx_qmi_firmware_stop(sc);
	
	sc->ops.stop(sc);
	qwx_wmi_detach(sc);
	qwx_dp_pdev_reo_cleanup(sc);
}

void
qwx_core_pdev_destroy(struct qwx_softc *sc)
{
	qwx_dp_pdev_free(sc);
}

int
qwx_core_pdev_create(struct qwx_softc *sc)
{
	int ret;

	ret = qwx_dp_pdev_alloc(sc);
	if (ret) {
		printf("%s: failed to attach DP pdev: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_mac_register(sc);
	if (ret) {
		printf("%s: failed register the radio with mac80211: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_dp_pdev_free;
	}
#if 0

	ret = ath11k_thermal_register(ab);
	if (ret) {
		ath11k_err(ab, "could not register thermal device: %d\n",
			   ret);
		goto err_mac_unregister;
	}

	ret = ath11k_spectral_init(ab);
	if (ret) {
		ath11k_err(ab, "failed to init spectral %d\n", ret);
		goto err_thermal_unregister;
	}

	return 0;

err_thermal_unregister:
	ath11k_thermal_unregister(ab);
err_mac_unregister:
	ath11k_mac_unregister(ab);
#endif
err_dp_pdev_free:
	qwx_dp_pdev_free(sc);
#if 0
err_pdev_debug:
	ath11k_debugfs_pdev_destroy(ab);
#endif
	return ret;
}

void
qwx_core_deinit(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;
	int s = splnet();

#ifdef notyet
	mutex_lock(&ab->core_lock);
#endif
	sc->ops.irq_disable(sc);

	qwx_core_stop(sc);
	qwx_core_pdev_destroy(sc);
#ifdef notyet
	mutex_unlock(&ab->core_lock);
#endif
	sc->ops.power_down(sc);
#if 0
	ath11k_mac_destroy(ab);
	ath11k_debugfs_soc_destroy(ab);
#endif
	qwx_dp_free(sc);
#if 0
	ath11k_reg_free(ab);
#endif
	qwx_qmi_deinit_service(sc);

	hal->num_shadow_reg_configured = 0;

	splx(s);
}

int
qwx_core_qmi_firmware_ready(struct qwx_softc *sc)
{
	int ret;

	ret = qwx_core_start_firmware(sc, sc->fw_mode);
	if (ret) {
		printf("%s: failed to start firmware: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_ce_init_pipes(sc);
	if (ret) {
		printf("%s: failed to initialize CE: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_firmware_stop;
	}

	ret = qwx_dp_alloc(sc);
	if (ret) {
		printf("%s: failed to init DP: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_firmware_stop;
	}

	switch (sc->crypto_mode) {
	case ATH11K_CRYPT_MODE_SW:
		set_bit(ATH11K_FLAG_HW_CRYPTO_DISABLED, sc->sc_flags);
		set_bit(ATH11K_FLAG_RAW_MODE, sc->sc_flags);
		break;
	case ATH11K_CRYPT_MODE_HW:
		clear_bit(ATH11K_FLAG_HW_CRYPTO_DISABLED, sc->sc_flags);
		clear_bit(ATH11K_FLAG_RAW_MODE, sc->sc_flags);
		break;
	default:
		printf("%s: invalid crypto_mode: %d\n",
		    sc->sc_dev.dv_xname, sc->crypto_mode);
		return EINVAL;
	}

	if (sc->frame_mode == ATH11K_HW_TXRX_RAW)
		set_bit(ATH11K_FLAG_RAW_MODE, sc->sc_flags);
#if 0
	mutex_lock(&ab->core_lock);
#endif
	ret = qwx_core_start(sc);
	if (ret) {
		printf("%s: failed to start core: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_dp_free;
	}

	if (!sc->attached) {
		printf("%s: %s fw 0x%x address %s\n", sc->sc_dev.dv_xname,
		    sc->hw_params.name, sc->qmi_target.fw_version,
		    ether_sprintf(sc->mac_addr));
	}

	ret = qwx_core_pdev_create(sc);
	if (ret) {
		printf("%s: failed to create pdev core: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto err_core_stop;
	}

#if 0 /* TODO: Is this in the right spot for OpenBSD? */
	sc->ops.irq_enable(sc);
#endif

#if 0
	mutex_unlock(&ab->core_lock);
#endif

	return 0;
err_core_stop:
	qwx_core_stop(sc);
#if 0
	ath11k_mac_destroy(ab);
#endif
err_dp_free:
	qwx_dp_free(sc);
#if 0
	mutex_unlock(&ab->core_lock);
#endif
err_firmware_stop:
	qwx_qmi_firmware_stop(sc);

	return ret;
}

void
qwx_qmi_fw_init_done(struct qwx_softc *sc)
{
	int ret = 0;

	clear_bit(ATH11K_FLAG_QMI_FAIL, sc->sc_flags);

	if (sc->qmi_cal_done == 0 && sc->hw_params.cold_boot_calib) {
		qwx_qmi_process_coldboot_calibration(sc);
	} else {
		clear_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags);
		clear_bit(ATH11K_FLAG_RECOVERY, sc->sc_flags);
		ret = qwx_core_qmi_firmware_ready(sc);
		if (ret) {
			set_bit(ATH11K_FLAG_QMI_FAIL, sc->sc_flags);
			return;
		}
	}
}

int
qwx_qmi_event_server_arrive(struct qwx_softc *sc)
{
	int ret;

	sc->fw_init_done = 0;

	ret = qwx_qmi_fw_ind_register_send(sc);
	if (ret < 0) {
		printf("%s: failed to send qmi firmware indication: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_qmi_host_cap_send(sc);
	if (ret < 0) {
		printf("%s: failed to send qmi host cap: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_qmi_mem_seg_send(sc);
	if (ret == EBUSY)
		ret = qwx_qmi_mem_seg_send(sc);
	if (ret) {
		printf("%s: failed to send qmi memory segments: %d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_qmi_event_load_bdf(sc);
	if (ret < 0) {
		printf("%s: qmi failed to download BDF:%d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	ret = qwx_qmi_wlanfw_m3_info_send(sc);
	if (ret) {
		printf("%s: qmi m3 info send failed:%d\n",
		    sc->sc_dev.dv_xname, ret);
		return ret;
	}

	while (!sc->fw_init_done) {
		ret = tsleep_nsec(&sc->fw_init_done, 0, "qwxfwinit",
		    SEC_TO_NSEC(10));
		if (ret) {
			printf("%s: fw init timeout\n", sc->sc_dev.dv_xname);
			return -1;
		}
	}

	qwx_qmi_fw_init_done(sc);
	return 0;
}

int
qwx_core_init(struct qwx_softc *sc)
{
	int error;

	error = qwx_qmi_init_service(sc);
	if (error) {
		printf("failed to initialize qmi :%d\n", error);
		return error;
	}

	error = sc->ops.power_up(sc);
	if (error) {
		printf("failed to power up :%d\n", error);
		qwx_qmi_deinit_service(sc);
	}

	return error;
}

int
qwx_init_hw_params(struct qwx_softc *sc)
{
	const struct ath11k_hw_params *hw_params = NULL;
	int i;

	for (i = 0; i < nitems(ath11k_hw_params); i++) {
		hw_params = &ath11k_hw_params[i];

		if (hw_params->hw_rev == sc->sc_hw_rev)
			break;
	}

	if (i == nitems(ath11k_hw_params)) {
		printf("%s: Unsupported hardware version: 0x%x\n",
		    sc->sc_dev.dv_xname, sc->sc_hw_rev);
		return EINVAL;
	}

	sc->hw_params = *hw_params;

	DPRINTF("%s: %s\n", sc->sc_dev.dv_xname, sc->hw_params.name);

	return 0;
}

static const struct hal_srng_config hw_srng_config_templ[QWX_NUM_SRNG_CFG] = {
	/* TODO: max_rings can populated by querying HW capabilities */
	{ /* REO_DST */
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW1,
		.max_rings = 4,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE,
	},

	{ /* REO_EXCEPTION */
		/* Designating REO2TCL ring as exception ring. This ring is
		 * similar to other REO2SW rings though it is named as REO2TCL.
		 * Any of theREO2SW rings can be used as exception ring.
		 */
		.start_ring_id = HAL_SRNG_RING_ID_REO2TCL,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2TCL_RING_BASE_MSB_RING_SIZE,
	},
	{ /* REO_REINJECT */
		.start_ring_id = HAL_SRNG_RING_ID_SW2REO,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_SW2REO_RING_BASE_MSB_RING_SIZE,
	},
	{ /* REO_CMD */
		.start_ring_id = HAL_SRNG_RING_ID_REO_CMD,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			sizeof(struct hal_reo_get_queue_stats)) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_CMD_RING_BASE_MSB_RING_SIZE,
	},
	{ /* REO_STATUS */
		.start_ring_id = HAL_SRNG_RING_ID_REO_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			sizeof(struct hal_reo_get_queue_stats_status)) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_STATUS_RING_BASE_MSB_RING_SIZE,
	},
	{ /* TCL_DATA */
		.start_ring_id = HAL_SRNG_RING_ID_SW2TCL1,
		.max_rings = 3,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			     sizeof(struct hal_tcl_data_cmd)) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2TCL1_RING_BASE_MSB_RING_SIZE,
	},
	{ /* TCL_CMD */
		.start_ring_id = HAL_SRNG_RING_ID_SW2TCL_CMD,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			     sizeof(struct hal_tcl_gse_cmd)) >> 2,
		.lmac_ring =  false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2TCL1_CMD_RING_BASE_MSB_RING_SIZE,
	},
	{ /* TCL_STATUS */
		.start_ring_id = HAL_SRNG_RING_ID_TCL_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			     sizeof(struct hal_tcl_status_ring)) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TCL_STATUS_RING_BASE_MSB_RING_SIZE,
	},
	{ /* CE_SRC */
		.start_ring_id = HAL_SRNG_RING_ID_CE0_SRC,
		.max_rings = 12,
		.entry_size = sizeof(struct hal_ce_srng_src_desc) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_SRC_RING_BASE_MSB_RING_SIZE,
	},
	{ /* CE_DST */
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST,
		.max_rings = 12,
		.entry_size = sizeof(struct hal_ce_srng_dest_desc) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_DST_RING_BASE_MSB_RING_SIZE,
	},
	{ /* CE_DST_STATUS */
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST_STATUS,
		.max_rings = 12,
		.entry_size = sizeof(struct hal_ce_srng_dst_status_desc) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_CE_DST_STATUS_RING_BASE_MSB_RING_SIZE,
	},
	{ /* WBM_IDLE_LINK */
		.start_ring_id = HAL_SRNG_RING_ID_WBM_IDLE_LINK,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_link_desc) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_IDLE_LINK_RING_BASE_MSB_RING_SIZE,
	},
	{ /* SW2WBM_RELEASE */
		.start_ring_id = HAL_SRNG_RING_ID_WBM_SW_RELEASE,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2WBM_RELEASE_RING_BASE_MSB_RING_SIZE,
	},
	{ /* WBM2SW_RELEASE */
		.start_ring_id = HAL_SRNG_RING_ID_WBM2SW0_RELEASE,
		.max_rings = 5,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.lmac_ring = false,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_WBM2SW_RELEASE_RING_BASE_MSB_RING_SIZE,
	},
	{ /* RXDMA_BUF */
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXDMA0_BUF,
		.max_rings = 2,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.lmac_ring = true,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
	{ /* RXDMA_DST */
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_RXDMA2SW0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.lmac_ring = true,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
	{ /* RXDMA_MONITOR_BUF */
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXDMA2_BUF,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.lmac_ring = true,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
	{ /* RXDMA_MONITOR_STATUS */
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_STATBUF,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.lmac_ring = true,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
	{ /* RXDMA_MONITOR_DST */
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_RXDMA2SW1,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.lmac_ring = true,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
	{ /* RXDMA_MONITOR_DESC */
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_DESC,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.lmac_ring = true,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
	{ /* RXDMA DIR BUF */
		.start_ring_id = HAL_SRNG_RING_ID_RXDMA_DIR_BUF,
		.max_rings = 1,
		.entry_size = 8 >> 2, /* TODO: Define the struct */
		.lmac_ring = true,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
};

int
qwx_hal_srng_create_config(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;
	struct hal_srng_config *s;

	memcpy(hal->srng_config, hw_srng_config_templ,
	    sizeof(hal->srng_config));

	s = &hal->srng_config[HAL_REO_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_HP(sc);
	s->reg_size[0] = HAL_REO2_RING_BASE_LSB(sc) - HAL_REO1_RING_BASE_LSB(sc);
	s->reg_size[1] = HAL_REO2_RING_HP(sc) - HAL_REO1_RING_HP(sc);

	s = &hal->srng_config[HAL_REO_EXCEPTION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_TCL_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_TCL_RING_HP(sc);

	s = &hal->srng_config[HAL_REO_REINJECT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_HP(sc);

	s = &hal->srng_config[HAL_REO_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_HP(sc);

	s = &hal->srng_config[HAL_REO_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_STATUS_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_STATUS_HP(sc);

	s = &hal->srng_config[HAL_TCL_DATA];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_HP;
	s->reg_size[0] = HAL_TCL2_RING_BASE_LSB(sc) - HAL_TCL1_RING_BASE_LSB(sc);
	s->reg_size[1] = HAL_TCL2_RING_HP - HAL_TCL1_RING_HP;

	s = &hal->srng_config[HAL_TCL_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_RING_HP;

	s = &hal->srng_config[HAL_TCL_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_HP;

	s = &hal->srng_config[HAL_CE_SRC];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(sc) + HAL_CE_DST_RING_BASE_LSB +
		ATH11K_CE_OFFSET(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(sc) + HAL_CE_DST_RING_HP +
		ATH11K_CE_OFFSET(sc);
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG(sc) -
		HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(sc);
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG(sc) -
		HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(sc);

	s = &hal->srng_config[HAL_CE_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc) + HAL_CE_DST_RING_BASE_LSB +
		ATH11K_CE_OFFSET(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc) + HAL_CE_DST_RING_HP +
		ATH11K_CE_OFFSET(sc);
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(sc) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc);
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(sc) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc);

	s = &hal->srng_config[HAL_CE_DST_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc) +
		HAL_CE_DST_STATUS_RING_BASE_LSB + ATH11K_CE_OFFSET(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc) + HAL_CE_DST_STATUS_RING_HP +
		ATH11K_CE_OFFSET(sc);
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(sc) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc);
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(sc) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(sc);

	s = &hal->srng_config[HAL_WBM_IDLE_LINK];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_HP;

	s = &hal->srng_config[HAL_SW2WBM_RELEASE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_RELEASE_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_RELEASE_RING_HP;

	s = &hal->srng_config[HAL_WBM2SW_RELEASE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM0_RELEASE_RING_BASE_LSB(sc);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM0_RELEASE_RING_HP;
	s->reg_size[0] = HAL_WBM1_RELEASE_RING_BASE_LSB(sc) -
		HAL_WBM0_RELEASE_RING_BASE_LSB(sc);
	s->reg_size[1] = HAL_WBM1_RELEASE_RING_HP - HAL_WBM0_RELEASE_RING_HP;

	return 0;
}

int
qwx_hal_srng_get_ring_id(struct qwx_softc *sc,
    enum hal_ring_type type, int ring_num, int mac_id)
{
	struct hal_srng_config *srng_config = &sc->hal.srng_config[type];
	int ring_id;

	if (ring_num >= srng_config->max_rings) {
		printf("%s: invalid ring number :%d\n", __func__, ring_num);
		return -1;
	}

	ring_id = srng_config->start_ring_id + ring_num;
	if (srng_config->lmac_ring)
		ring_id += mac_id * HAL_SRNG_RINGS_PER_LMAC;

	if (ring_id >= HAL_SRNG_RING_ID_MAX) {
		printf("%s: invalid ring ID :%d\n", __func__, ring_id);
		return -1;
	}

	return ring_id;
}

void
qwx_hal_srng_update_hp_tp_addr(struct qwx_softc *sc, int shadow_cfg_idx,
    enum hal_ring_type ring_type, int ring_num)
{
	struct hal_srng *srng;
	struct ath11k_hal *hal = &sc->hal;
	int ring_id;
	struct hal_srng_config *srng_config = &hal->srng_config[ring_type];

	ring_id = qwx_hal_srng_get_ring_id(sc, ring_type, ring_num, 0);
	if (ring_id < 0)
		return;

	srng = &hal->srng_list[ring_id];

	if (srng_config->ring_dir == HAL_SRNG_DIR_DST)
		srng->u.dst_ring.tp_addr = (uint32_t *)(
		    HAL_SHADOW_REG(sc, shadow_cfg_idx) +
		    (unsigned long)sc->mem);
	else
		srng->u.src_ring.hp_addr = (uint32_t *)(
		    HAL_SHADOW_REG(sc, shadow_cfg_idx) +
		    (unsigned long)sc->mem);
}

void
qwx_hal_srng_shadow_update_hp_tp(struct qwx_softc *sc, struct hal_srng *srng)
{
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	/* Update the shadow HP if the ring isn't empty. */
	if (srng->ring_dir == HAL_SRNG_DIR_SRC &&
	    *srng->u.src_ring.tp_addr != srng->u.src_ring.hp)
		qwx_hal_srng_access_end(sc, srng);
}

int
qwx_hal_srng_update_shadow_config(struct qwx_softc *sc,
    enum hal_ring_type ring_type, int ring_num)
{
	struct ath11k_hal *hal = &sc->hal;
	struct hal_srng_config *srng_config = &hal->srng_config[ring_type];
	int shadow_cfg_idx = hal->num_shadow_reg_configured;
	uint32_t target_reg;

	if (shadow_cfg_idx >= HAL_SHADOW_NUM_REGS)
		return EINVAL;

	hal->num_shadow_reg_configured++;

	target_reg = srng_config->reg_start[HAL_HP_OFFSET_IN_REG_START];
	target_reg += srng_config->reg_size[HAL_HP_OFFSET_IN_REG_START] *
		ring_num;

	/* For destination ring, shadow the TP */
	if (srng_config->ring_dir == HAL_SRNG_DIR_DST)
		target_reg += HAL_OFFSET_FROM_HP_TO_TP;

	hal->shadow_reg_addr[shadow_cfg_idx] = target_reg;

	/* update hp/tp addr to hal structure*/
	qwx_hal_srng_update_hp_tp_addr(sc, shadow_cfg_idx, ring_type, ring_num);

	DPRINTF("%s: target_reg %x, shadow reg 0x%x shadow_idx 0x%x, "
	    "ring_type %d, ring num %d\n", __func__, target_reg,
	     HAL_SHADOW_REG(sc, shadow_cfg_idx), shadow_cfg_idx,
	     ring_type, ring_num);

	return 0;
}

void
qwx_hal_srng_shadow_config(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;
	int ring_type, ring_num;
	struct hal_srng_config *cfg;

	/* update all the non-CE srngs. */
	for (ring_type = 0; ring_type < HAL_MAX_RING_TYPES; ring_type++) {
		cfg = &hal->srng_config[ring_type];

		if (ring_type == HAL_CE_SRC ||
		    ring_type == HAL_CE_DST ||
			ring_type == HAL_CE_DST_STATUS)
			continue;

		if (cfg->lmac_ring)
			continue;

		for (ring_num = 0; ring_num < cfg->max_rings; ring_num++) {
			qwx_hal_srng_update_shadow_config(sc, ring_type,
			    ring_num);
		}
	}
}

void
qwx_hal_srng_get_shadow_config(struct qwx_softc *sc, uint32_t **cfg,
    uint32_t *len)
{
	struct ath11k_hal *hal = &sc->hal;

	*len = hal->num_shadow_reg_configured;
	*cfg = hal->shadow_reg_addr;
}

int
qwx_hal_alloc_cont_rdp(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;
	size_t size = sizeof(uint32_t) * HAL_SRNG_RING_ID_MAX;

	if (hal->rdpmem == NULL) {
		hal->rdpmem = qwx_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE);
		if (hal->rdpmem == NULL) {
			printf("%s: could not allocate RDP DMA memory\n",
			    sc->sc_dev.dv_xname);
			return ENOMEM;

		}
	}

	hal->rdp.vaddr = QWX_DMA_KVA(hal->rdpmem);
	hal->rdp.paddr = QWX_DMA_DVA(hal->rdpmem);
	return 0;
}

void
qwx_hal_free_cont_rdp(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;

	if (hal->rdpmem == NULL)
		return;

	hal->rdp.vaddr = NULL;
	hal->rdp.paddr = 0L;
	qwx_dmamem_free(sc->sc_dmat, hal->rdpmem);
	hal->rdpmem = NULL;
}

int
qwx_hal_alloc_cont_wrp(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;
	size_t size = sizeof(uint32_t) * HAL_SRNG_NUM_LMAC_RINGS;

	if (hal->wrpmem == NULL) {
		hal->wrpmem = qwx_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE);
		if (hal->wrpmem == NULL) {
			printf("%s: could not allocate WDP DMA memory\n",
			    sc->sc_dev.dv_xname);
			return ENOMEM;

		}
	}

	hal->wrp.vaddr = QWX_DMA_KVA(hal->wrpmem);
	hal->wrp.paddr = QWX_DMA_DVA(hal->wrpmem);
	return 0;
}

void
qwx_hal_free_cont_wrp(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;

	if (hal->wrpmem == NULL)
		return;

	hal->wrp.vaddr = NULL;
	hal->wrp.paddr = 0L;
	qwx_dmamem_free(sc->sc_dmat, hal->wrpmem);
	hal->wrpmem = NULL;
}

int
qwx_hal_srng_init(struct qwx_softc *sc)
{
	struct ath11k_hal *hal = &sc->hal;
	int ret;

	memset(hal, 0, sizeof(*hal));

	ret = qwx_hal_srng_create_config(sc);
	if (ret)
		goto err_hal;

	ret = qwx_hal_alloc_cont_rdp(sc);
	if (ret)
		goto err_hal;

	ret = qwx_hal_alloc_cont_wrp(sc);
	if (ret)
		goto err_free_cont_rdp;

#ifdef notyet
	qwx_hal_register_srng_key(sc);
#endif

	return 0;
err_free_cont_rdp:
	qwx_hal_free_cont_rdp(sc);

err_hal:
	return ret;
}

void
qwx_hal_srng_dst_hw_init(struct qwx_softc *sc, struct hal_srng *srng)
{
	struct ath11k_hal *hal = &sc->hal;
	uint32_t val;
	uint64_t hp_addr;
	uint32_t reg_base;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];

	if (srng->flags & HAL_SRNG_FLAGS_MSI_INTR) {
		sc->ops.write32(sc,
		    reg_base + HAL_REO1_RING_MSI1_BASE_LSB_OFFSET(sc),
		    srng->msi_addr);

		val = FIELD_PREP(HAL_REO1_RING_MSI1_BASE_MSB_ADDR,
		    ((uint64_t)srng->msi_addr >> HAL_ADDR_MSB_REG_SHIFT)) |
		    HAL_REO1_RING_MSI1_BASE_MSB_MSI1_ENABLE;
		sc->ops.write32(sc,
		    reg_base + HAL_REO1_RING_MSI1_BASE_MSB_OFFSET(sc), val);

		sc->ops.write32(sc,
		    reg_base + HAL_REO1_RING_MSI1_DATA_OFFSET(sc),
		    srng->msi_data);
	}

	sc->ops.write32(sc, reg_base, srng->ring_base_paddr);

	val = FIELD_PREP(HAL_REO1_RING_BASE_MSB_RING_BASE_ADDR_MSB,
	    ((uint64_t)srng->ring_base_paddr >> HAL_ADDR_MSB_REG_SHIFT)) |
	    FIELD_PREP(HAL_REO1_RING_BASE_MSB_RING_SIZE,
	    (srng->entry_size * srng->num_entries));
	sc->ops.write32(sc,
	    reg_base + HAL_REO1_RING_BASE_MSB_OFFSET(sc), val);

	val = FIELD_PREP(HAL_REO1_RING_ID_RING_ID, srng->ring_id) |
	    FIELD_PREP(HAL_REO1_RING_ID_ENTRY_SIZE, srng->entry_size);
	sc->ops.write32(sc, reg_base + HAL_REO1_RING_ID_OFFSET(sc), val);

	/* interrupt setup */
	val = FIELD_PREP(HAL_REO1_RING_PRDR_INT_SETUP_INTR_TMR_THOLD,
	    (srng->intr_timer_thres_us >> 3));

	val |= FIELD_PREP(HAL_REO1_RING_PRDR_INT_SETUP_BATCH_COUNTER_THOLD,
	    (srng->intr_batch_cntr_thres_entries * srng->entry_size));

	sc->ops.write32(sc,
	    reg_base + HAL_REO1_RING_PRODUCER_INT_SETUP_OFFSET(sc), val);

	hp_addr = hal->rdp.paddr + ((unsigned long)srng->u.dst_ring.hp_addr -
	    (unsigned long)hal->rdp.vaddr);
	sc->ops.write32(sc, reg_base + HAL_REO1_RING_HP_ADDR_LSB_OFFSET(sc),
	    hp_addr & HAL_ADDR_LSB_REG_MASK);
	sc->ops.write32(sc, reg_base + HAL_REO1_RING_HP_ADDR_MSB_OFFSET(sc),
	    hp_addr >> HAL_ADDR_MSB_REG_SHIFT);

	/* Initialize head and tail pointers to indicate ring is empty */
	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R2];
	sc->ops.write32(sc, reg_base, 0);
	sc->ops.write32(sc, reg_base + HAL_REO1_RING_TP_OFFSET(sc), 0);
	*srng->u.dst_ring.hp_addr = 0;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];
	val = 0;
	if (srng->flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP)
		val |= HAL_REO1_RING_MISC_DATA_TLV_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_RING_PTR_SWAP)
		val |= HAL_REO1_RING_MISC_HOST_FW_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_MSI_SWAP)
		val |= HAL_REO1_RING_MISC_MSI_SWAP;
	val |= HAL_REO1_RING_MISC_SRNG_ENABLE;

	sc->ops.write32(sc, reg_base + HAL_REO1_RING_MISC_OFFSET(sc), val);
}

void
qwx_hal_srng_src_hw_init(struct qwx_softc *sc, struct hal_srng *srng)
{
	struct ath11k_hal *hal = &sc->hal;
	uint32_t val;
	uint64_t tp_addr;
	uint32_t reg_base;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];

	if (srng->flags & HAL_SRNG_FLAGS_MSI_INTR) {
		sc->ops.write32(sc,
		    reg_base + HAL_TCL1_RING_MSI1_BASE_LSB_OFFSET(sc),
		    srng->msi_addr);

		val = FIELD_PREP(HAL_TCL1_RING_MSI1_BASE_MSB_ADDR,
		    ((uint64_t)srng->msi_addr >> HAL_ADDR_MSB_REG_SHIFT)) |
		      HAL_TCL1_RING_MSI1_BASE_MSB_MSI1_ENABLE;
		sc->ops.write32(sc,
		    reg_base + HAL_TCL1_RING_MSI1_BASE_MSB_OFFSET(sc),
		    val);

		sc->ops.write32(sc,
		    reg_base + HAL_TCL1_RING_MSI1_DATA_OFFSET(sc),
		    srng->msi_data);
	}

	sc->ops.write32(sc, reg_base, srng->ring_base_paddr);

	val = FIELD_PREP(HAL_TCL1_RING_BASE_MSB_RING_BASE_ADDR_MSB,
	    ((uint64_t)srng->ring_base_paddr >> HAL_ADDR_MSB_REG_SHIFT)) |
	    FIELD_PREP(HAL_TCL1_RING_BASE_MSB_RING_SIZE,
	    (srng->entry_size * srng->num_entries));
	sc->ops.write32(sc, reg_base + HAL_TCL1_RING_BASE_MSB_OFFSET(sc), val);

	val = FIELD_PREP(HAL_REO1_RING_ID_ENTRY_SIZE, srng->entry_size);
	sc->ops.write32(sc, reg_base + HAL_TCL1_RING_ID_OFFSET(sc), val);

	if (srng->ring_id == HAL_SRNG_RING_ID_WBM_IDLE_LINK) {
		sc->ops.write32(sc, reg_base, (uint32_t)srng->ring_base_paddr);
		val = FIELD_PREP(HAL_TCL1_RING_BASE_MSB_RING_BASE_ADDR_MSB,
		    ((uint64_t)srng->ring_base_paddr >>
		    HAL_ADDR_MSB_REG_SHIFT)) |
		    FIELD_PREP(HAL_TCL1_RING_BASE_MSB_RING_SIZE,
		    (srng->entry_size * srng->num_entries));
		sc->ops.write32(sc,
		    reg_base + HAL_TCL1_RING_BASE_MSB_OFFSET(sc), val);
	}

	/* interrupt setup */
	/* NOTE: IPQ8074 v2 requires the interrupt timer threshold in the
	 * unit of 8 usecs instead of 1 usec (as required by v1).
	 */
	val = FIELD_PREP(HAL_TCL1_RING_CONSR_INT_SETUP_IX0_INTR_TMR_THOLD,
	    srng->intr_timer_thres_us);

	val |= FIELD_PREP(HAL_TCL1_RING_CONSR_INT_SETUP_IX0_BATCH_COUNTER_THOLD,
	    (srng->intr_batch_cntr_thres_entries * srng->entry_size));

	sc->ops.write32(sc,
	    reg_base + HAL_TCL1_RING_CONSR_INT_SETUP_IX0_OFFSET(sc), val);

	val = 0;
	if (srng->flags & HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN) {
		val |= FIELD_PREP(HAL_TCL1_RING_CONSR_INT_SETUP_IX1_LOW_THOLD,
		    srng->u.src_ring.low_threshold);
	}
	sc->ops.write32(sc,
	    reg_base + HAL_TCL1_RING_CONSR_INT_SETUP_IX1_OFFSET(sc), val);

	if (srng->ring_id != HAL_SRNG_RING_ID_WBM_IDLE_LINK) {
		tp_addr = hal->rdp.paddr +
		    ((unsigned long)srng->u.src_ring.tp_addr -
		    (unsigned long)hal->rdp.vaddr);
		sc->ops.write32(sc,
		    reg_base + HAL_TCL1_RING_TP_ADDR_LSB_OFFSET(sc),
		    tp_addr & HAL_ADDR_LSB_REG_MASK);
		sc->ops.write32(sc,
		    reg_base + HAL_TCL1_RING_TP_ADDR_MSB_OFFSET(sc),
		    tp_addr >> HAL_ADDR_MSB_REG_SHIFT);
	}

	/* Initialize head and tail pointers to indicate ring is empty */
	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R2];
	sc->ops.write32(sc, reg_base, 0);
	sc->ops.write32(sc, reg_base + HAL_TCL1_RING_TP_OFFSET, 0);
	*srng->u.src_ring.tp_addr = 0;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];
	val = 0;
	if (srng->flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP)
		val |= HAL_TCL1_RING_MISC_DATA_TLV_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_RING_PTR_SWAP)
		val |= HAL_TCL1_RING_MISC_HOST_FW_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_MSI_SWAP)
		val |= HAL_TCL1_RING_MISC_MSI_SWAP;

	/* Loop count is not used for SRC rings */
	val |= HAL_TCL1_RING_MISC_MSI_LOOPCNT_DISABLE;

	val |= HAL_TCL1_RING_MISC_SRNG_ENABLE;

	sc->ops.write32(sc, reg_base + HAL_TCL1_RING_MISC_OFFSET(sc), val);
}

void
qwx_hal_srng_hw_init(struct qwx_softc *sc, struct hal_srng *srng)
{
	if (srng->ring_dir == HAL_SRNG_DIR_SRC)
		qwx_hal_srng_src_hw_init(sc, srng);
	else
		qwx_hal_srng_dst_hw_init(sc, srng);
}

void
qwx_hal_ce_dst_setup(struct qwx_softc *sc, struct hal_srng *srng, int ring_num)
{
	struct hal_srng_config *srng_config = &sc->hal.srng_config[HAL_CE_DST];
	uint32_t addr;
	uint32_t val;

	addr = HAL_CE_DST_RING_CTRL +
	    srng_config->reg_start[HAL_SRNG_REG_GRP_R0] +
	    ring_num * srng_config->reg_size[HAL_SRNG_REG_GRP_R0];

	val = sc->ops.read32(sc, addr);
	val &= ~HAL_CE_DST_R0_DEST_CTRL_MAX_LEN;
	val |= FIELD_PREP(HAL_CE_DST_R0_DEST_CTRL_MAX_LEN,
	    srng->u.dst_ring.max_buffer_length);
	sc->ops.write32(sc, addr, val);
}

void
qwx_hal_ce_src_set_desc(void *buf, uint64_t paddr, uint32_t len, uint32_t id,
    uint8_t byte_swap_data)
{
	struct hal_ce_srng_src_desc *desc = (struct hal_ce_srng_src_desc *)buf;

	desc->buffer_addr_low = paddr & HAL_ADDR_LSB_REG_MASK;
	desc->buffer_addr_info = FIELD_PREP(HAL_CE_SRC_DESC_ADDR_INFO_ADDR_HI,
	    (paddr >> HAL_ADDR_MSB_REG_SHIFT)) |
	    FIELD_PREP(HAL_CE_SRC_DESC_ADDR_INFO_BYTE_SWAP,
	    byte_swap_data) |
	    FIELD_PREP(HAL_CE_SRC_DESC_ADDR_INFO_GATHER, 0) |
	    FIELD_PREP(HAL_CE_SRC_DESC_ADDR_INFO_LEN, len);
	desc->meta_info = FIELD_PREP(HAL_CE_SRC_DESC_META_INFO_DATA, id);
}

void
qwx_hal_ce_dst_set_desc(void *buf, uint64_t paddr)
{
	struct hal_ce_srng_dest_desc *desc =
	    (struct hal_ce_srng_dest_desc *)buf;

	desc->buffer_addr_low = htole32(paddr & HAL_ADDR_LSB_REG_MASK);
	desc->buffer_addr_info = htole32(FIELD_PREP(
	    HAL_CE_DEST_DESC_ADDR_INFO_ADDR_HI,
	    (paddr >> HAL_ADDR_MSB_REG_SHIFT)));
}

uint32_t
qwx_hal_ce_dst_status_get_length(void *buf)
{
	struct hal_ce_srng_dst_status_desc *desc =
		(struct hal_ce_srng_dst_status_desc *)buf;
	uint32_t len;

	len = FIELD_GET(HAL_CE_DST_STATUS_DESC_FLAGS_LEN, desc->flags);
	desc->flags &= ~HAL_CE_DST_STATUS_DESC_FLAGS_LEN;

	return len;
}


int
qwx_hal_srng_setup(struct qwx_softc *sc, enum hal_ring_type type,
    int ring_num, int mac_id, struct hal_srng_params *params)
{
	struct ath11k_hal *hal = &sc->hal;
	struct hal_srng_config *srng_config = &sc->hal.srng_config[type];
	struct hal_srng *srng;
	int ring_id;
	uint32_t lmac_idx;
	int i;
	uint32_t reg_base;

	ring_id = qwx_hal_srng_get_ring_id(sc, type, ring_num, mac_id);
	if (ring_id < 0)
		return ring_id;

	srng = &hal->srng_list[ring_id];

	srng->ring_id = ring_id;
	srng->ring_dir = srng_config->ring_dir;
	srng->ring_base_paddr = params->ring_base_paddr;
	srng->ring_base_vaddr = params->ring_base_vaddr;
	srng->entry_size = srng_config->entry_size;
	srng->num_entries = params->num_entries;
	srng->ring_size = srng->entry_size * srng->num_entries;
	srng->intr_batch_cntr_thres_entries =
	    params->intr_batch_cntr_thres_entries;
	srng->intr_timer_thres_us = params->intr_timer_thres_us;
	srng->flags = params->flags;
	srng->msi_addr = params->msi_addr;
	srng->msi_data = params->msi_data;
	srng->initialized = 1;
#if 0
	spin_lock_init(&srng->lock);
	lockdep_set_class(&srng->lock, hal->srng_key + ring_id);
#endif

	for (i = 0; i < HAL_SRNG_NUM_REG_GRP; i++) {
		srng->hwreg_base[i] = srng_config->reg_start[i] +
		    (ring_num * srng_config->reg_size[i]);
	}

	memset(srng->ring_base_vaddr, 0,
	    (srng->entry_size * srng->num_entries) << 2);

#if 0 /* Not needed on OpenBSD? We do swapping in sofware... */
	/* TODO: Add comments on these swap configurations */
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		srng->flags |= HAL_SRNG_FLAGS_MSI_SWAP | HAL_SRNG_FLAGS_DATA_TLV_SWAP |
			       HAL_SRNG_FLAGS_RING_PTR_SWAP;
#endif
	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R2];

	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		srng->u.src_ring.hp = 0;
		srng->u.src_ring.cached_tp = 0;
		srng->u.src_ring.reap_hp = srng->ring_size - srng->entry_size;
		srng->u.src_ring.tp_addr = (void *)(hal->rdp.vaddr + ring_id);
		srng->u.src_ring.low_threshold = params->low_threshold *
		    srng->entry_size;
		if (srng_config->lmac_ring) {
			lmac_idx = ring_id - HAL_SRNG_RING_ID_LMAC1_ID_START;
			srng->u.src_ring.hp_addr = (void *)(hal->wrp.vaddr +
			    lmac_idx);
			srng->flags |= HAL_SRNG_FLAGS_LMAC_RING;
		} else {
			if (!sc->hw_params.supports_shadow_regs)
				srng->u.src_ring.hp_addr =
				    (uint32_t *)((unsigned long)sc->mem +
				    reg_base);
			else
				DPRINTF("%s: type %d ring_num %d reg_base "
				    "0x%x shadow 0x%lx\n",
				    sc->sc_dev.dv_xname, type, ring_num, reg_base,
				   (unsigned long)srng->u.src_ring.hp_addr -
				   (unsigned long)sc->mem);
		}
	} else {
		/* During initialization loop count in all the descriptors
		 * will be set to zero, and HW will set it to 1 on completing
		 * descriptor update in first loop, and increments it by 1 on
		 * subsequent loops (loop count wraps around after reaching
		 * 0xffff). The 'loop_cnt' in SW ring state is the expected
		 * loop count in descriptors updated by HW (to be processed
		 * by SW).
		 */
		srng->u.dst_ring.loop_cnt = 1;
		srng->u.dst_ring.tp = 0;
		srng->u.dst_ring.cached_hp = 0;
		srng->u.dst_ring.hp_addr = (void *)(hal->rdp.vaddr + ring_id);
		if (srng_config->lmac_ring) {
			/* For LMAC rings, tail pointer updates will be done
			 * through FW by writing to a shared memory location
			 */
			lmac_idx = ring_id - HAL_SRNG_RING_ID_LMAC1_ID_START;
			srng->u.dst_ring.tp_addr = (void *)(hal->wrp.vaddr +
			    lmac_idx);
			srng->flags |= HAL_SRNG_FLAGS_LMAC_RING;
		} else {
			if (!sc->hw_params.supports_shadow_regs)
				srng->u.dst_ring.tp_addr =
				    (uint32_t *)((unsigned long)sc->mem +
				    reg_base + (HAL_REO1_RING_TP(sc) -
				    HAL_REO1_RING_HP(sc)));
			else
				DPRINTF("%s: type %d ring_num %d target_reg "
				    "0x%x shadow 0x%lx\n", sc->sc_dev.dv_xname,
				    type, ring_num,
				    reg_base + (HAL_REO1_RING_TP(sc) -
				    HAL_REO1_RING_HP(sc)),
				    (unsigned long)srng->u.dst_ring.tp_addr -
				    (unsigned long)sc->mem);
		}
	}

	if (srng_config->lmac_ring)
		return ring_id;

	qwx_hal_srng_hw_init(sc, srng);

	if (type == HAL_CE_DST) {
		srng->u.dst_ring.max_buffer_length = params->max_buffer_len;
		qwx_hal_ce_dst_setup(sc, srng, ring_num);
	}

	return ring_id;
}

size_t
qwx_hal_ce_get_desc_size(enum hal_ce_desc type)
{
	switch (type) {
	case HAL_CE_DESC_SRC:
		return sizeof(struct hal_ce_srng_src_desc);
	case HAL_CE_DESC_DST:
		return sizeof(struct hal_ce_srng_dest_desc);
	case HAL_CE_DESC_DST_STATUS:
		return sizeof(struct hal_ce_srng_dst_status_desc);
	}

	return 0;
}

void
qwx_htc_tx_completion_handler(struct qwx_softc *sc, struct mbuf *m)
{
	printf("%s: not implemented\n", __func__);
}

struct qwx_tx_data *
qwx_ce_completed_send_next(struct qwx_ce_pipe *pipe)
{
	struct qwx_softc *sc = pipe->sc;
	struct hal_srng *srng;
	unsigned int sw_index;
	unsigned int nentries_mask;
	void *ctx;
	struct qwx_tx_data *tx_data = NULL;
	uint32_t *desc;
#ifdef notyet
	spin_lock_bh(&ab->ce.ce_lock);
#endif
	sw_index = pipe->src_ring->sw_index;
	nentries_mask = pipe->src_ring->nentries_mask;

	srng = &sc->hal.srng_list[pipe->src_ring->hal_ring_id];
#ifdef notyet
	spin_lock_bh(&srng->lock);
#endif
	qwx_hal_srng_access_begin(sc, srng);

	desc = qwx_hal_srng_src_reap_next(sc, srng);
	if (!desc)
		goto err_unlock;

	ctx = pipe->src_ring->per_transfer_context[sw_index];
	tx_data = (struct qwx_tx_data *)ctx;

	sw_index = CE_RING_IDX_INCR(nentries_mask, sw_index);
	pipe->src_ring->sw_index = sw_index;

err_unlock:
#ifdef notyet
	spin_unlock_bh(&srng->lock);

	spin_unlock_bh(&ab->ce.ce_lock);
#endif
	return tx_data;
}

int
qwx_ce_tx_process_cb(struct qwx_ce_pipe *pipe)
{
	struct qwx_softc *sc = pipe->sc;
	struct qwx_tx_data *tx_data;
	struct mbuf *m;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	int ret = 0;

	while ((tx_data = qwx_ce_completed_send_next(pipe)) != NULL) {
		bus_dmamap_unload(sc->sc_dmat, tx_data->map);
		m = tx_data->m;
		tx_data->m = NULL;

		if ((!pipe->send_cb) || sc->hw_params.credit_flow) {
			m_freem(m);
			continue;
		}

		ml_enqueue(&ml, m);
		ret = 1;
	}

	while ((m = ml_dequeue(&ml))) {
		DNPRINTF(QWX_D_CE, "%s: tx ce pipe %d len %d\n", __func__,
		    pipe->pipe_num, m->m_len);
		pipe->send_cb(sc, m);
	}

	return ret;
}

void
qwx_ce_poll_send_completed(struct qwx_softc *sc, uint8_t pipe_id)
{
	struct qwx_ce_pipe *pipe = &sc->ce.ce_pipe[pipe_id];
	const struct ce_attr *attr =  &sc->hw_params.host_ce_config[pipe_id];

	if ((pipe->attr_flags & CE_ATTR_DIS_INTR) && attr->src_nentries)
		qwx_ce_tx_process_cb(pipe);
}

void
qwx_htc_process_credit_report(struct qwx_htc *htc,
    const struct ath11k_htc_credit_report *report, int len,
    enum ath11k_htc_ep_id eid)
{
	struct qwx_softc *sc = htc->sc;
	struct qwx_htc_ep *ep;
	int i, n_reports;

	if (len % sizeof(*report))
		printf("%s: Uneven credit report len %d", __func__, len);

	n_reports = len / sizeof(*report);
#ifdef notyet
	spin_lock_bh(&htc->tx_lock);
#endif
	for (i = 0; i < n_reports; i++, report++) {
		if (report->eid >= ATH11K_HTC_EP_COUNT)
			break;

		ep = &htc->endpoint[report->eid];
		ep->tx_credits += report->credits;

		DNPRINTF(QWX_D_HTC, "%s: ep %d credits got %d total %d\n",
		    __func__, report->eid, report->credits, ep->tx_credits);

		if (ep->ep_ops.ep_tx_credits) {
#ifdef notyet
			spin_unlock_bh(&htc->tx_lock);
#endif
			ep->ep_ops.ep_tx_credits(sc);
#ifdef notyet
			spin_lock_bh(&htc->tx_lock);
#endif
		}
	}
#ifdef notyet
	spin_unlock_bh(&htc->tx_lock);
#endif
}

int
qwx_htc_process_trailer(struct qwx_htc *htc, uint8_t *buffer, int length,
    enum ath11k_htc_ep_id src_eid)
{
	struct qwx_softc *sc = htc->sc;
	int status = 0;
	struct ath11k_htc_record *record;
	size_t len;

	while (length > 0) {
		record = (struct ath11k_htc_record *)buffer;

		if (length < sizeof(record->hdr)) {
			status = EINVAL;
			break;
		}

		if (record->hdr.len > length) {
			/* no room left in buffer for record */
			printf("%s: Invalid record length: %d\n",
			    __func__, record->hdr.len);
			status = EINVAL;
			break;
		}

		if (sc->hw_params.credit_flow) {
			switch (record->hdr.id) {
			case ATH11K_HTC_RECORD_CREDITS:
				len = sizeof(struct ath11k_htc_credit_report);
				if (record->hdr.len < len) {
					printf("%s: Credit report too long\n",
					    __func__);
					status = EINVAL;
					break;
				}
				qwx_htc_process_credit_report(htc,
				    record->credit_report,
				    record->hdr.len, src_eid);
				break;
			default:
				printf("%s: unhandled record: id:%d length:%d\n",
				    __func__, record->hdr.id, record->hdr.len);
				break;
			}
		}

		if (status)
			break;

		/* multiple records may be present in a trailer */
		buffer += sizeof(record->hdr) + record->hdr.len;
		length -= sizeof(record->hdr) + record->hdr.len;
	}

	return status;
}

void
qwx_htc_suspend_complete(struct qwx_softc *sc, int ack)
{
	printf("%s: not implemented\n", __func__);
}

void
qwx_htc_wakeup_from_suspend(struct qwx_softc *sc)
{
	/* TODO This is really all the Linux driver does here... silence it? */
	printf("%s: wakeup from suspend received\n", __func__);
}

void
qwx_htc_rx_completion_handler(struct qwx_softc *sc, struct mbuf *m)
{
	struct qwx_htc *htc = &sc->htc;
	struct ath11k_htc_hdr *hdr;
	struct qwx_htc_ep *ep;
	uint16_t payload_len;
	uint32_t message_id, trailer_len = 0;
	uint8_t eid;
	int trailer_present;

	m = m_pullup(m, sizeof(struct ath11k_htc_hdr));
	if (m == NULL) {
		printf("%s: m_pullup failed\n", __func__);
		m = NULL; /* already freed */
		goto out;
	}

	hdr = mtod(m, struct ath11k_htc_hdr *);

	eid = FIELD_GET(HTC_HDR_ENDPOINTID, hdr->htc_info);

	if (eid >= ATH11K_HTC_EP_COUNT) {
		printf("%s: HTC Rx: invalid eid %d\n", __func__, eid);
		printf("%s: HTC info: 0x%x\n", __func__, hdr->htc_info);
		printf("%s: CTRL info: 0x%x\n", __func__, hdr->ctrl_info);
		goto out;
	}

	ep = &htc->endpoint[eid];

	payload_len = FIELD_GET(HTC_HDR_PAYLOADLEN, hdr->htc_info);

	if (payload_len + sizeof(*hdr) > ATH11K_HTC_MAX_LEN) {
		printf("%s: HTC rx frame too long, len: %zu\n", __func__,
		    payload_len + sizeof(*hdr));
		goto out;
	}

	if (m->m_pkthdr.len < payload_len) {
		printf("%s: HTC Rx: insufficient length, got %d, "
		    "expected %d\n", __func__, m->m_pkthdr.len, payload_len);
		goto out;
	}

	/* get flags to check for trailer */
	trailer_present = (FIELD_GET(HTC_HDR_FLAGS, hdr->htc_info)) &
	    ATH11K_HTC_FLAG_TRAILER_PRESENT;

	DNPRINTF(QWX_D_HTC, "%s: rx ep %d mbuf %p trailer_present %d\n",
	    __func__, eid, m, trailer_present);

	if (trailer_present) {
		int status = 0;
		uint8_t *trailer;
		int trim;
		size_t min_len;

		trailer_len = FIELD_GET(HTC_HDR_CONTROLBYTES0, hdr->ctrl_info);
		min_len = sizeof(struct ath11k_htc_record_hdr);

		if ((trailer_len < min_len) ||
		    (trailer_len > payload_len)) {
			printf("%s: Invalid trailer length: %d\n", __func__,
			    trailer_len);
			goto out;
		}

		trailer = (uint8_t *)hdr;
		trailer += sizeof(*hdr);
		trailer += payload_len;
		trailer -= trailer_len;
		status = qwx_htc_process_trailer(htc, trailer,
		    trailer_len, eid);
		if (status)
			goto out;

		trim = trailer_len;
		m_adj(m, -trim);
	}

	if (trailer_len >= payload_len)
		/* zero length packet with trailer data, just drop these */
		goto out;

	m_adj(m, sizeof(*hdr));

	if (eid == ATH11K_HTC_EP_0) {
		struct ath11k_htc_msg *msg;

		msg = mtod(m, struct ath11k_htc_msg *);
		message_id = FIELD_GET(HTC_MSG_MESSAGEID, msg->msg_svc_id);

		DNPRINTF(QWX_D_HTC, "%s: rx ep %d mbuf %p message_id %d\n",
		    __func__, eid, m, message_id);

		switch (message_id) {
		case ATH11K_HTC_MSG_READY_ID:
		case ATH11K_HTC_MSG_CONNECT_SERVICE_RESP_ID:
			/* handle HTC control message */
			if (sc->ctl_resp) {
				/* this is a fatal error, target should not be
				 * sending unsolicited messages on the ep 0
				 */
				printf("%s: HTC rx ctrl still processing\n",
				    __func__);
				goto out;
			}

			htc->control_resp_len =
			    MIN(m->m_pkthdr.len, ATH11K_HTC_MAX_CTRL_MSG_LEN);

			m_copydata(m, 0, htc->control_resp_len,
			    htc->control_resp_buffer);

			sc->ctl_resp = 1;
			wakeup(&sc->ctl_resp);
			break;
		case ATH11K_HTC_MSG_SEND_SUSPEND_COMPLETE:
			qwx_htc_suspend_complete(sc, 1);
			break;
		case ATH11K_HTC_MSG_NACK_SUSPEND:
			qwx_htc_suspend_complete(sc, 0);
			break;
		case ATH11K_HTC_MSG_WAKEUP_FROM_SUSPEND_ID:
			qwx_htc_wakeup_from_suspend(sc);
			break;
		default:
			printf("%s: ignoring unsolicited htc ep0 event %ld\n",
			    __func__,
			    FIELD_GET(HTC_MSG_MESSAGEID, msg->msg_svc_id));
			break;
		}
		goto out;
	}

	DNPRINTF(QWX_D_HTC, "%s: rx ep %d mbuf %p\n", __func__, eid, m);

	ep->ep_ops.ep_rx_complete(sc, m);

	/* poll tx completion for interrupt disabled CE's */
	qwx_ce_poll_send_completed(sc, ep->ul_pipe_id);

	/* mbuf is now owned by the rx completion handler */
	m = NULL;
out:
	m_freem(m);
}

void
qwx_ce_free_ring(struct qwx_softc *sc, struct qwx_ce_ring *ring)
{
	bus_size_t dsize;
	size_t size;
	
	if (ring == NULL)
		return;

	if (ring->base_addr) {
		dsize = ring->nentries * ring->desc_sz;
		bus_dmamem_unmap(sc->sc_dmat, ring->base_addr, dsize);
	}
	if (ring->nsegs)
		bus_dmamem_free(sc->sc_dmat, &ring->dsegs, ring->nsegs);
	if (ring->dmap)
		bus_dmamap_destroy(sc->sc_dmat, ring->dmap);

	size = sizeof(*ring) + (ring->nentries *
	    sizeof(ring->per_transfer_context[0]));
	free(ring, M_DEVBUF, size);
}

static inline int
qwx_ce_need_shadow_fix(int ce_id)
{
	/* only ce4 needs shadow workaround */
	return (ce_id == 4);
}

void
qwx_ce_stop_shadow_timers(struct qwx_softc *sc)
{
	int i;

	if (!sc->hw_params.supports_shadow_regs)
		return;

	for (i = 0; i < sc->hw_params.ce_count; i++)
		if (qwx_ce_need_shadow_fix(i))
			qwx_dp_shadow_stop_timer(sc, &sc->ce.hp_timer[i]);
}

void
qwx_ce_free_pipes(struct qwx_softc *sc)
{
	struct qwx_ce_pipe *pipe;
	int i;

	for (i = 0; i < sc->hw_params.ce_count; i++) {
		pipe = &sc->ce.ce_pipe[i];
		if (qwx_ce_need_shadow_fix(i))
			qwx_dp_shadow_stop_timer(sc, &sc->ce.hp_timer[i]);
		if (pipe->src_ring) {
			qwx_ce_free_ring(sc, pipe->src_ring);
			pipe->src_ring = NULL;
		}

		if (pipe->dest_ring) {
			qwx_ce_free_ring(sc, pipe->dest_ring);
			pipe->dest_ring = NULL;
		}

		if (pipe->status_ring) {
			qwx_ce_free_ring(sc, pipe->status_ring);
			pipe->status_ring = NULL;
		}
	}
}

int
qwx_ce_alloc_src_ring_transfer_contexts(struct qwx_ce_pipe *pipe,
    const struct ce_attr *attr)
{
	struct qwx_softc *sc = pipe->sc;
	struct qwx_tx_data *txdata;
	size_t size;
	int ret, i;

	/* Allocate an array of qwx_tx_data structures. */
	txdata = mallocarray(pipe->src_ring->nentries, sizeof(*txdata),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (txdata == NULL)
		return ENOMEM;

	size = sizeof(*txdata) * pipe->src_ring->nentries;

	/* Create per-transfer DMA maps. */
	for (i = 0; i < pipe->src_ring->nentries; i++) {
		struct qwx_tx_data *ctx = &txdata[i];
		ret = bus_dmamap_create(sc->sc_dmat, attr->src_sz_max, 1,
		    attr->src_sz_max, 0, BUS_DMA_NOWAIT, &ctx->map);
		if (ret) {
			int j;
			for (j = 0; j < i; j++) {
				struct qwx_tx_data *ctx = &txdata[j];
				bus_dmamap_destroy(sc->sc_dmat, ctx->map);
			}
			free(txdata, M_DEVBUF, size);
			return ret;
		}
		pipe->src_ring->per_transfer_context[i] = ctx;
	}

	return 0;
}

int
qwx_ce_alloc_dest_ring_transfer_contexts(struct qwx_ce_pipe *pipe,
    const struct ce_attr *attr)
{
	struct qwx_softc *sc = pipe->sc;
	struct qwx_rx_data *rxdata;
	size_t size;
	int ret, i;

	/* Allocate an array of qwx_rx_data structures. */
	rxdata = mallocarray(pipe->dest_ring->nentries, sizeof(*rxdata),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (rxdata == NULL)
		return ENOMEM;

	size = sizeof(*rxdata) * pipe->dest_ring->nentries;

	/* Create per-transfer DMA maps. */
	for (i = 0; i < pipe->dest_ring->nentries; i++) {
		struct qwx_rx_data *ctx = &rxdata[i];
		ret = bus_dmamap_create(sc->sc_dmat, attr->src_sz_max, 1,
		    attr->src_sz_max, 0, BUS_DMA_NOWAIT, &ctx->map);
		if (ret) {
			int j;
			for (j = 0; j < i; j++) {
				struct qwx_rx_data *ctx = &rxdata[j];
				bus_dmamap_destroy(sc->sc_dmat, ctx->map);
			}
			free(rxdata, M_DEVBUF, size);
			return ret;
		}
		pipe->dest_ring->per_transfer_context[i] = ctx;
	}

	return 0;
}

struct qwx_ce_ring *
qwx_ce_alloc_ring(struct qwx_softc *sc, int nentries, size_t desc_sz)
{
	struct qwx_ce_ring *ce_ring;
	size_t size = sizeof(*ce_ring) +
	    (nentries * sizeof(ce_ring->per_transfer_context[0]));
	bus_size_t dsize;

	ce_ring = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ce_ring == NULL)
		return NULL;

	ce_ring->nentries = nentries;
	ce_ring->nentries_mask = nentries - 1;
	ce_ring->desc_sz = desc_sz;

	dsize = nentries * desc_sz;
	if (bus_dmamap_create(sc->sc_dmat, dsize, 1, dsize, 0, BUS_DMA_NOWAIT,
	    &ce_ring->dmap)) {
		free(ce_ring, M_DEVBUF, size);
		return NULL;
	}

	if (bus_dmamem_alloc(sc->sc_dmat, dsize, CE_DESC_RING_ALIGN, 0,
	    &ce_ring->dsegs, 1, &ce_ring->nsegs,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO)) {
		qwx_ce_free_ring(sc, ce_ring);
		return NULL;
	}

	if (bus_dmamem_map(sc->sc_dmat, &ce_ring->dsegs, 1, dsize,
	    &ce_ring->base_addr, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
		qwx_ce_free_ring(sc, ce_ring);
		return NULL;
	}

	if (bus_dmamap_load(sc->sc_dmat, ce_ring->dmap, ce_ring->base_addr,
	    dsize, NULL, BUS_DMA_NOWAIT)) {
		qwx_ce_free_ring(sc, ce_ring);
		return NULL;
	}

	return ce_ring;
}

int
qwx_ce_alloc_pipe(struct qwx_softc *sc, int ce_id)
{
	struct qwx_ce_pipe *pipe = &sc->ce.ce_pipe[ce_id];
	const struct ce_attr *attr = &sc->hw_params.host_ce_config[ce_id];
	struct qwx_ce_ring *ring;
	int nentries;
	size_t desc_sz;

	pipe->attr_flags = attr->flags;

	if (attr->src_nentries) {
		pipe->send_cb = attr->send_cb;
		nentries = qwx_roundup_pow_of_two(attr->src_nentries);
		desc_sz = qwx_hal_ce_get_desc_size(HAL_CE_DESC_SRC);
		ring = qwx_ce_alloc_ring(sc, nentries, desc_sz);
		if (ring == NULL)
			return ENOMEM;
		pipe->src_ring = ring;
		if (qwx_ce_alloc_src_ring_transfer_contexts(pipe, attr))
			return ENOMEM;
	}

	if (attr->dest_nentries) {
		pipe->recv_cb = attr->recv_cb;
		nentries = qwx_roundup_pow_of_two(attr->dest_nentries);
		desc_sz = qwx_hal_ce_get_desc_size(HAL_CE_DESC_DST);
		ring = qwx_ce_alloc_ring(sc, nentries, desc_sz);
		if (ring == NULL)
			return ENOMEM;
		pipe->dest_ring = ring;
		if (qwx_ce_alloc_dest_ring_transfer_contexts(pipe, attr))
			return ENOMEM;

		desc_sz = qwx_hal_ce_get_desc_size(HAL_CE_DESC_DST_STATUS);
		ring = qwx_ce_alloc_ring(sc, nentries, desc_sz);
		if (ring == NULL)
			return ENOMEM;
		pipe->status_ring = ring;
	}

	return 0;
}

void
qwx_ce_rx_pipe_cleanup(struct qwx_ce_pipe *pipe)
{
	struct qwx_softc *sc = pipe->sc;
	struct qwx_ce_ring *ring = pipe->dest_ring;
	void *ctx;
	struct qwx_rx_data *rx_data;
	int i;

	if (!(ring && pipe->buf_sz))
		return;

	for (i = 0; i < ring->nentries; i++) {
		ctx = ring->per_transfer_context[i];
		if (!ctx)
			continue;

		rx_data = (struct qwx_rx_data *)ctx;
		if (rx_data->m) {
			bus_dmamap_unload(sc->sc_dmat, rx_data->map);
			m_freem(rx_data->m);
			rx_data->m = NULL;
		}
	}
}

void
qwx_ce_shadow_config(struct qwx_softc *sc)
{
	int i;

	for (i = 0; i < sc->hw_params.ce_count; i++) {
		if (sc->hw_params.host_ce_config[i].src_nentries)
			qwx_hal_srng_update_shadow_config(sc, HAL_CE_SRC, i);

		if (sc->hw_params.host_ce_config[i].dest_nentries) {
			qwx_hal_srng_update_shadow_config(sc, HAL_CE_DST, i);

			qwx_hal_srng_update_shadow_config(sc,
			    HAL_CE_DST_STATUS, i);
		}
	}
}

void
qwx_ce_get_shadow_config(struct qwx_softc *sc, uint32_t **shadow_cfg,
    uint32_t *shadow_cfg_len)
{
	if (!sc->hw_params.supports_shadow_regs)
		return;

	qwx_hal_srng_get_shadow_config(sc, shadow_cfg, shadow_cfg_len);

	/* shadow is already configured */
	if (*shadow_cfg_len)
		return;

	/* shadow isn't configured yet, configure now.
	 * non-CE srngs are configured firstly, then
	 * all CE srngs.
	 */
	qwx_hal_srng_shadow_config(sc);
	qwx_ce_shadow_config(sc);

	/* get the shadow configuration */
	qwx_hal_srng_get_shadow_config(sc, shadow_cfg, shadow_cfg_len);
}

void
qwx_ce_cleanup_pipes(struct qwx_softc *sc)
{
	struct qwx_ce_pipe *pipe;
	int pipe_num;

	qwx_ce_stop_shadow_timers(sc);

	for (pipe_num = 0; pipe_num < sc->hw_params.ce_count; pipe_num++) {
		pipe = &sc->ce.ce_pipe[pipe_num];
		qwx_ce_rx_pipe_cleanup(pipe);

		/* Cleanup any src CE's which have interrupts disabled */
		qwx_ce_poll_send_completed(sc, pipe_num);
	}
}

int
qwx_ce_alloc_pipes(struct qwx_softc *sc)
{
	struct qwx_ce_pipe *pipe;
	int i;
	int ret;
	const struct ce_attr *attr;

	for (i = 0; i < sc->hw_params.ce_count; i++) {
		attr = &sc->hw_params.host_ce_config[i];
		pipe = &sc->ce.ce_pipe[i];
		pipe->pipe_num = i;
		pipe->sc = sc;
		pipe->buf_sz = attr->src_sz_max;

		ret = qwx_ce_alloc_pipe(sc, i);
		if (ret) {
			/* Free any partial successful allocation */
			qwx_ce_free_pipes(sc);
			return ret;
		}
	}

	return 0;
}

void
qwx_get_ce_msi_idx(struct qwx_softc *sc, uint32_t ce_id,
    uint32_t *msi_data_idx)
{
	*msi_data_idx = ce_id;
}

void
qwx_ce_srng_msi_ring_params_setup(struct qwx_softc *sc, uint32_t ce_id,
    struct hal_srng_params *ring_params)
{
	uint32_t msi_data_start = 0;
	uint32_t msi_data_count = 1, msi_data_idx;
	uint32_t msi_irq_start = 0;
	uint32_t addr_lo;
	uint32_t addr_hi;
	int ret;

	ret = sc->ops.get_user_msi_vector(sc, "CE",
	    &msi_data_count, &msi_data_start, &msi_irq_start);
	if (ret)
		return;

	qwx_get_msi_address(sc, &addr_lo, &addr_hi);
	qwx_get_ce_msi_idx(sc, ce_id, &msi_data_idx);

	ring_params->msi_addr = addr_lo;
	ring_params->msi_addr |= (((uint64_t)addr_hi) << 32);
	ring_params->msi_data = (msi_data_idx % msi_data_count) + msi_data_start;
	ring_params->flags |= HAL_SRNG_FLAGS_MSI_INTR;
}

int
qwx_ce_init_ring(struct qwx_softc *sc, struct qwx_ce_ring *ce_ring,
    int ce_id, enum hal_ring_type type)
{
	struct hal_srng_params params = { 0 };
	int ret;

	params.ring_base_paddr = ce_ring->dmap->dm_segs[0].ds_addr;
	params.ring_base_vaddr = (uint32_t *)ce_ring->base_addr;
	params.num_entries = ce_ring->nentries;

	if (!(CE_ATTR_DIS_INTR & sc->hw_params.host_ce_config[ce_id].flags))
		qwx_ce_srng_msi_ring_params_setup(sc, ce_id, &params);

	switch (type) {
	case HAL_CE_SRC:
		if (!(CE_ATTR_DIS_INTR &
		    sc->hw_params.host_ce_config[ce_id].flags))
			params.intr_batch_cntr_thres_entries = 1;
		break;
	case HAL_CE_DST:
		params.max_buffer_len =
		    sc->hw_params.host_ce_config[ce_id].src_sz_max;
		if (!(sc->hw_params.host_ce_config[ce_id].flags &
		    CE_ATTR_DIS_INTR)) {
			params.intr_timer_thres_us = 1024;
			params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
			params.low_threshold = ce_ring->nentries - 3;
		}
		break;
	case HAL_CE_DST_STATUS:
		if (!(sc->hw_params.host_ce_config[ce_id].flags &
		    CE_ATTR_DIS_INTR)) {
			params.intr_batch_cntr_thres_entries = 1;
			params.intr_timer_thres_us = 0x1000;
		}
		break;
	default:
		printf("%s: Invalid CE ring type %d\n",
		    sc->sc_dev.dv_xname, type);
		return EINVAL;
	}

	/* TODO: Init other params needed by HAL to init the ring */

	ret = qwx_hal_srng_setup(sc, type, ce_id, 0, &params);
	if (ret < 0) {
		printf("%s: failed to setup srng: ring_id %d ce_id %d\n",
		    sc->sc_dev.dv_xname, ret, ce_id);
		return ret;
	}

	ce_ring->hal_ring_id = ret;

	if (sc->hw_params.supports_shadow_regs &&
	    qwx_ce_need_shadow_fix(ce_id))
		qwx_dp_shadow_init_timer(sc, &sc->ce.hp_timer[ce_id],
		    ATH11K_SHADOW_CTRL_TIMER_INTERVAL, ce_ring->hal_ring_id);

	return 0;
}

int
qwx_ce_init_pipes(struct qwx_softc *sc)
{
	struct qwx_ce_pipe *pipe;
	int i;
	int ret;

	for (i = 0; i < sc->hw_params.ce_count; i++) {
		pipe = &sc->ce.ce_pipe[i];

		if (pipe->src_ring) {
			ret = qwx_ce_init_ring(sc, pipe->src_ring, i,
			    HAL_CE_SRC);
			if (ret) {
				printf("%s: failed to init src ring: %d\n",
				    sc->sc_dev.dv_xname, ret);
				/* Should we clear any partial init */
				return ret;
			}

			pipe->src_ring->write_index = 0;
			pipe->src_ring->sw_index = 0;
		}

		if (pipe->dest_ring) {
			ret = qwx_ce_init_ring(sc, pipe->dest_ring, i,
			    HAL_CE_DST);
			if (ret) {
				printf("%s: failed to init dest ring: %d\n",
				    sc->sc_dev.dv_xname, ret);
				/* Should we clear any partial init */
				return ret;
			}

			pipe->rx_buf_needed = pipe->dest_ring->nentries ?
			    pipe->dest_ring->nentries - 2 : 0;

			pipe->dest_ring->write_index = 0;
			pipe->dest_ring->sw_index = 0;
		}

		if (pipe->status_ring) {
			ret = qwx_ce_init_ring(sc, pipe->status_ring, i,
			    HAL_CE_DST_STATUS);
			if (ret) {
				printf("%s: failed to init status ring: %d\n",
				    sc->sc_dev.dv_xname, ret);
				/* Should we clear any partial init */
				return ret;
			}

			pipe->status_ring->write_index = 0;
			pipe->status_ring->sw_index = 0;
		}
	}

	return 0;
}

int
qwx_hal_srng_src_num_free(struct qwx_softc *sc, struct hal_srng *srng,
    int sync_hw_ptr)
{
	uint32_t tp, hp;
#ifdef notyet
	lockdep_assert_held(&srng->lock);
#endif
	hp = srng->u.src_ring.hp;

	if (sync_hw_ptr) {
		tp = *srng->u.src_ring.tp_addr;
		srng->u.src_ring.cached_tp = tp;
	} else {
		tp = srng->u.src_ring.cached_tp;
	}

	if (tp > hp)
		return ((tp - hp) / srng->entry_size) - 1;
	else
		return ((srng->ring_size - hp + tp) / srng->entry_size) - 1;
}

int
qwx_ce_rx_buf_enqueue_pipe(struct qwx_ce_pipe *pipe, bus_dmamap_t map)
{
	struct qwx_softc *sc = pipe->sc;
	struct qwx_ce_ring *ring = pipe->dest_ring;
	struct hal_srng *srng;
	unsigned int write_index;
	unsigned int nentries_mask = ring->nentries_mask;
	uint32_t *desc;
	uint64_t paddr;
	int ret;
#ifdef notyet
	lockdep_assert_held(&ab->ce.ce_lock);
#endif
	write_index = ring->write_index;

	srng = &sc->hal.srng_list[ring->hal_ring_id];
#ifdef notyet
	spin_lock_bh(&srng->lock);
#endif
	qwx_hal_srng_access_begin(sc, srng);
	bus_dmamap_sync(sc->sc_dmat, map, 0,
	    srng->entry_size * sizeof(uint32_t), BUS_DMASYNC_POSTREAD);

	if (qwx_hal_srng_src_num_free(sc, srng, 0) < 1) {
		ret = ENOSPC;
		goto exit;
	}

	desc = qwx_hal_srng_src_get_next_entry(sc, srng);
	if (!desc) {
		ret = ENOSPC;
		goto exit;
	}

	paddr = map->dm_segs[0].ds_addr;
	qwx_hal_ce_dst_set_desc(desc, paddr);

	write_index = CE_RING_IDX_INCR(nentries_mask, write_index);
	ring->write_index = write_index;

	pipe->rx_buf_needed--;

	ret = 0;
exit:
	qwx_hal_srng_access_end(sc, srng);
	bus_dmamap_sync(sc->sc_dmat, map, 0,
	    srng->entry_size * sizeof(uint32_t), BUS_DMASYNC_PREREAD);
#ifdef notyet
	spin_unlock_bh(&srng->lock);
#endif
	return ret;
}

int
qwx_ce_rx_post_pipe(struct qwx_ce_pipe *pipe)
{
	struct qwx_softc *sc = pipe->sc;
	int ret = 0;
	unsigned int idx;
	void *ctx;
	struct qwx_rx_data *rx_data;
	struct mbuf *m;

	if (!pipe->dest_ring)
		return 0;

#ifdef notyet
	spin_lock_bh(&ab->ce.ce_lock);
#endif
	while (pipe->rx_buf_needed) {
		m = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			ret = ENOBUFS;
			goto done;
		}

		if (pipe->buf_sz <= MCLBYTES)
			MCLGET(m, M_DONTWAIT);
		else
			MCLGETL(m, M_DONTWAIT, pipe->buf_sz);
		if ((m->m_flags & M_EXT) == 0) {
			ret = ENOBUFS;
			goto done;
		}

		idx = pipe->dest_ring->write_index;
		ctx = pipe->dest_ring->per_transfer_context[idx];
		rx_data = (struct qwx_rx_data *)ctx;

		m->m_len = m->m_pkthdr.len = pipe->buf_sz;
		ret = bus_dmamap_load_mbuf(sc->sc_dmat, rx_data->map,
		    m, BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (ret) {
			printf("%s: can't map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, ret);
			m_freem(m);
			goto done;
		}

		ret = qwx_ce_rx_buf_enqueue_pipe(pipe, rx_data->map);
		if (ret) {
			printf("%s: failed to enqueue rx buf: %d\n",
			    sc->sc_dev.dv_xname, ret);
			bus_dmamap_unload(sc->sc_dmat, rx_data->map);
			m_freem(m);
			break;
		} else
			rx_data->m = m;
	}

done:
#ifdef notyet
	spin_unlock_bh(&ab->ce.ce_lock);
#endif
	return ret;
}

void
qwx_ce_rx_post_buf(struct qwx_softc *sc)
{
	struct qwx_ce_pipe *pipe;
	int i;
	int ret;

	for (i = 0; i < sc->hw_params.ce_count; i++) {
		pipe = &sc->ce.ce_pipe[i];
		ret = qwx_ce_rx_post_pipe(pipe);
		if (ret) {
			if (ret == ENOBUFS)
				continue;

			printf("%s: failed to post rx buf to pipe: %d err: %d\n",
			    sc->sc_dev.dv_xname, i, ret);
#ifdef notyet
			mod_timer(&ab->rx_replenish_retry,
				  jiffies + ATH11K_CE_RX_POST_RETRY_JIFFIES);
#endif

			return;
		}
	}
}

int
qwx_ce_completed_recv_next(struct qwx_ce_pipe *pipe,
    void **per_transfer_contextp, int *nbytes)
{
	struct qwx_softc *sc = pipe->sc;
	struct hal_srng *srng;
	unsigned int sw_index;
	unsigned int nentries_mask;
	uint32_t *desc;
	int ret = 0;
#ifdef notyet
	spin_lock_bh(&ab->ce.ce_lock);
#endif
	sw_index = pipe->dest_ring->sw_index;
	nentries_mask = pipe->dest_ring->nentries_mask;

	srng = &sc->hal.srng_list[pipe->status_ring->hal_ring_id];
#ifdef notyet
	spin_lock_bh(&srng->lock);
#endif
	qwx_hal_srng_access_begin(sc, srng);

	desc = qwx_hal_srng_dst_get_next_entry(sc, srng);
	if (!desc) {
		ret = EIO;
		goto err;
	}

	*nbytes = qwx_hal_ce_dst_status_get_length(desc);
	if (*nbytes == 0) {
		ret = EIO;
		goto err;
	}

	if (per_transfer_contextp) {
		*per_transfer_contextp =
		    pipe->dest_ring->per_transfer_context[sw_index];
	}

	sw_index = CE_RING_IDX_INCR(nentries_mask, sw_index);
	pipe->dest_ring->sw_index = sw_index;

	pipe->rx_buf_needed++;
err:
	qwx_hal_srng_access_end(sc, srng);
#ifdef notyet
	spin_unlock_bh(&srng->lock);
	spin_unlock_bh(&ab->ce.ce_lock);
#endif
	return ret;
}

int
qwx_ce_recv_process_cb(struct qwx_ce_pipe *pipe)
{
	struct qwx_softc *sc = pipe->sc;
	struct mbuf *m;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	void *transfer_context;
	unsigned int nbytes, max_nbytes;
	int ret = 0, err;

	while (qwx_ce_completed_recv_next(pipe, &transfer_context,
	    &nbytes) == 0) {
		struct qwx_rx_data *rx_data = transfer_context;

		bus_dmamap_unload(sc->sc_dmat, rx_data->map);
		m = rx_data->m;
		rx_data->m = NULL;

		max_nbytes = m->m_pkthdr.len;
		if (max_nbytes < nbytes) {
			printf("%s: received more than expected (nbytes %d, "
			    "max %d)", __func__, nbytes, max_nbytes);
			m_freem(m);
			continue;
		}
		m->m_len = m->m_pkthdr.len = nbytes;
		ml_enqueue(&ml, m);
		ret = 1;
	}

	while ((m = ml_dequeue(&ml))) {
		DNPRINTF(QWX_D_CE, "%s: rx ce pipe %d len %d\n", __func__,
		    pipe->pipe_num, m->m_len);
		pipe->recv_cb(sc, m);
	}

	err = qwx_ce_rx_post_pipe(pipe);
	if (err && err != ENOBUFS) {
		printf("%s: failed to post rx buf to pipe: %d err: %d\n",
		    __func__, pipe->pipe_num, err);
#ifdef notyet
		mod_timer(&ab->rx_replenish_retry,
			  jiffies + ATH11K_CE_RX_POST_RETRY_JIFFIES);
#endif
	}

	return ret;
}

int
qwx_ce_per_engine_service(struct qwx_softc *sc, uint16_t ce_id)
{
	struct qwx_ce_pipe *pipe = &sc->ce.ce_pipe[ce_id];
	const struct ce_attr *attr = &sc->hw_params.host_ce_config[ce_id];
	int ret = 0;

	if (attr->src_nentries) {
		if (qwx_ce_tx_process_cb(pipe))
			ret = 1;
	}

	if (pipe->recv_cb) {
		if (qwx_ce_recv_process_cb(pipe))
			ret = 1;
	}

	return ret;
}

int
qwx_ce_send(struct qwx_softc *sc, struct mbuf *m, uint8_t pipe_id,
    uint16_t transfer_id)
{
	struct qwx_ce_pipe *pipe = &sc->ce.ce_pipe[pipe_id];
	struct hal_srng *srng;
	uint32_t *desc;
	unsigned int write_index, sw_index;
	unsigned int nentries_mask;
	int ret = 0;
	uint8_t byte_swap_data = 0;
	int num_used;
	uint64_t paddr;
	void *ctx;
	struct qwx_tx_data *tx_data;

	/* Check if some entries could be regained by handling tx completion if
	 * the CE has interrupts disabled and the used entries is more than the
	 * defined usage threshold.
	 */
	if (pipe->attr_flags & CE_ATTR_DIS_INTR) {
#ifdef notyet
		spin_lock_bh(&ab->ce.ce_lock);
#endif
		write_index = pipe->src_ring->write_index;

		sw_index = pipe->src_ring->sw_index;

		if (write_index >= sw_index)
			num_used = write_index - sw_index;
		else
			num_used = pipe->src_ring->nentries - sw_index +
			    write_index;
#ifdef notyet
		spin_unlock_bh(&ab->ce.ce_lock);
#endif
		if (num_used > ATH11K_CE_USAGE_THRESHOLD)
			qwx_ce_poll_send_completed(sc, pipe->pipe_num);
	}

	if (test_bit(ATH11K_FLAG_CRASH_FLUSH, sc->sc_flags))
		return ESHUTDOWN;
#ifdef notyet
	spin_lock_bh(&ab->ce.ce_lock);
#endif
	write_index = pipe->src_ring->write_index;
	nentries_mask = pipe->src_ring->nentries_mask;

	srng = &sc->hal.srng_list[pipe->src_ring->hal_ring_id];
#ifdef notyet
	spin_lock_bh(&srng->lock);
#endif
	qwx_hal_srng_access_begin(sc, srng);

	if (qwx_hal_srng_src_num_free(sc, srng, 0) < 1) {
		qwx_hal_srng_access_end(sc, srng);
		ret = ENOBUFS;
		goto err_unlock;
	}

	desc = qwx_hal_srng_src_get_next_reaped(sc, srng);
	if (!desc) {
		qwx_hal_srng_access_end(sc, srng);
		ret = ENOBUFS;
		goto err_unlock;
	}

	if (pipe->attr_flags & CE_ATTR_BYTE_SWAP_DATA)
		byte_swap_data = 1;

	ctx = pipe->src_ring->per_transfer_context[write_index];
	tx_data = (struct qwx_tx_data *)ctx;

	paddr = tx_data->map->dm_segs[0].ds_addr;
	qwx_hal_ce_src_set_desc(desc, paddr, m->m_pkthdr.len,
	    transfer_id, byte_swap_data);

	pipe->src_ring->write_index = CE_RING_IDX_INCR(nentries_mask,
	    write_index);

	qwx_hal_srng_access_end(sc, srng);

	if (qwx_ce_need_shadow_fix(pipe_id))
		qwx_dp_shadow_start_timer(sc, srng, &sc->ce.hp_timer[pipe_id]);

err_unlock:
#ifdef notyet
	spin_unlock_bh(&srng->lock);

	spin_unlock_bh(&ab->ce.ce_lock);
#endif
	return ret;
}

int
qwx_get_num_chains(uint32_t mask)
{
	int num_chains = 0;

	while (mask) {
		if (mask & 0x1)
			num_chains++;
		mask >>= 1;
	}

	return num_chains;
}

int
qwx_set_antenna(struct qwx_pdev *pdev, uint32_t tx_ant, uint32_t rx_ant)
{
	struct qwx_softc *sc = pdev->sc;
	int ret;
#ifdef notyet
	lockdep_assert_held(&ar->conf_mutex);
#endif
	sc->cfg_tx_chainmask = tx_ant;
	sc->cfg_rx_chainmask = rx_ant;
#if 0
	if (ar->state != ATH11K_STATE_ON &&
	    ar->state != ATH11K_STATE_RESTARTED)
		return 0;
#endif
	ret = qwx_wmi_pdev_set_param(sc, WMI_PDEV_PARAM_TX_CHAIN_MASK,
	    tx_ant, pdev->pdev_id);
	if (ret) {
		printf("%s: failed to set tx-chainmask: %d, req 0x%x\n",
		    sc->sc_dev.dv_xname, ret, tx_ant);
		return ret;
	}

	sc->num_tx_chains = qwx_get_num_chains(tx_ant);

	ret = qwx_wmi_pdev_set_param(sc, WMI_PDEV_PARAM_RX_CHAIN_MASK,
	    rx_ant, pdev->pdev_id);
	if (ret) {
		printf("%s: failed to set rx-chainmask: %d, req 0x%x\n",
		    sc->sc_dev.dv_xname, ret, rx_ant);
		return ret;
	}

	sc->num_rx_chains = qwx_get_num_chains(rx_ant);
#if 0
	/* Reload HT/VHT/HE capability */
	ath11k_mac_setup_ht_vht_cap(ar, &ar->pdev->cap, NULL);
	ath11k_mac_setup_he_cap(ar, &ar->pdev->cap);
#endif
	return 0;
}

int
qwx_reg_update_chan_list(struct qwx_softc *sc, uint8_t pdev_id)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct scan_chan_list_params *params;
	struct ieee80211_channel *channel, *lastc;
	struct channel_param *ch;
	int num_channels = 0;
	size_t params_size;
	int ret;
#if 0
	if (ar->state == ATH11K_STATE_RESTARTING)
		return 0;
#endif
	lastc = &ic->ic_channels[IEEE80211_CHAN_MAX];
	for (channel = &ic->ic_channels[1]; channel <= lastc; channel++) {
		if (channel->ic_flags == 0)
			continue;
		num_channels++;
	}

	if (!num_channels)
		return EINVAL;

	params_size = sizeof(*params) +
	    num_channels * sizeof(*params->ch_param);

	/*
	 * TODO: This is a temporary list for qwx_wmi_send_scan_chan_list_cmd
	 * to loop over. Could that function loop over ic_channels directly?
	 */
	params = malloc(params_size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!params)
		return ENOMEM;

	params->pdev_id = pdev_id;
	params->nallchans = num_channels;

	ch = params->ch_param;
	lastc = &ic->ic_channels[IEEE80211_CHAN_MAX];
	for (channel = &ic->ic_channels[1]; channel <= lastc; channel++) {
		if (channel->ic_flags == 0)
			continue;
#ifdef notyet
		/* TODO: Set to true/false based on some condition? */
		ch->allow_ht = true;
		ch->allow_vht = true;
		ch->allow_he = true;
#endif
		ch->dfs_set = !!(IEEE80211_IS_CHAN_5GHZ(channel) &&
		    (channel->ic_flags & IEEE80211_CHAN_PASSIVE));
		ch->is_chan_passive = !!(channel->ic_flags &
		    IEEE80211_CHAN_PASSIVE);
		ch->is_chan_passive |= ch->dfs_set;
		ch->mhz = ieee80211_ieee2mhz(ieee80211_chan2ieee(ic, channel),
		    channel->ic_flags);
		ch->cfreq1 = ch->mhz;
		ch->minpower = 0;
		ch->maxpower = 40; /* XXX from Linux debug trace */
		ch->maxregpower = ch->maxpower; 
		ch->antennamax = 0;

		/* TODO: Use appropriate phymodes */
		if (IEEE80211_IS_CHAN_2GHZ(channel))
			ch->phy_mode = MODE_11G;
		else
			ch->phy_mode = MODE_11A;
#ifdef notyet
		if (channel->band == NL80211_BAND_6GHZ &&
		    cfg80211_channel_is_psc(channel))
			ch->psc_channel = true;
#endif
		DNPRINTF(QWX_D_WMI, "%s: mac channel freq %d maxpower %d "
		    "regpower %d antenna %d mode %d\n", __func__,
		    ch->mhz, ch->maxpower, ch->maxregpower,
		    ch->antennamax, ch->phy_mode);

		ch++;
		/* TODO: use quarrter/half rate, cfreq12, dfs_cfreq2
		 * set_agile, reg_class_idx
		 */
	}

	ret = qwx_wmi_send_scan_chan_list_cmd(sc, pdev_id, params);
	free(params, M_DEVBUF, params_size);

	return ret;
}

static const struct htt_rx_ring_tlv_filter qwx_mac_mon_status_filter_default = {
	.rx_filter = HTT_RX_FILTER_TLV_FLAGS_MPDU_START |
	    HTT_RX_FILTER_TLV_FLAGS_PPDU_END |
	    HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE,
	.pkt_filter_flags0 = HTT_RX_FP_MGMT_FILTER_FLAGS0,
	.pkt_filter_flags1 = HTT_RX_FP_MGMT_FILTER_FLAGS1,
	.pkt_filter_flags2 = HTT_RX_FP_CTRL_FILTER_FLASG2,
	.pkt_filter_flags3 = HTT_RX_FP_DATA_FILTER_FLASG3 |
	    HTT_RX_FP_CTRL_FILTER_FLASG3
};

int
qwx_mac_register(struct qwx_softc *sc)
{
	/* Initialize channel counters frequency value in hertz */
	sc->cc_freq_hz = IPQ8074_CC_FREQ_HERTZ;

	sc->free_vdev_map = (1U << (sc->num_radios * TARGET_NUM_VDEVS(sc))) - 1;

	if (IEEE80211_ADDR_EQ(etheranyaddr, sc->sc_ic.ic_myaddr))
		IEEE80211_ADDR_COPY(sc->sc_ic.ic_myaddr, sc->mac_addr);

	return 0;
}

int
qwx_mac_config_mon_status_default(struct qwx_softc *sc, int enable)
{
	struct htt_rx_ring_tlv_filter tlv_filter = { 0 };
	int i, ret = 0;
	struct dp_rxdma_ring *ring;

	if (enable)
		tlv_filter = qwx_mac_mon_status_filter_default;

	for (i = 0; i < sc->hw_params.num_rxmda_per_pdev; i++) {
		ring = &sc->pdev_dp.rx_mon_status_refill_ring[i];
		ret = qwx_dp_tx_htt_rx_filter_setup(sc,
		    ring->refill_buf_ring.ring_id, sc->pdev_dp.mac_id + i,
		    HAL_RXDMA_MONITOR_STATUS, DP_RX_BUFFER_SIZE, &tlv_filter);
		if (ret)
			return ret;
	}
#if 0
	if (enable && !ar->ab->hw_params.rxdma1_enable)
		mod_timer(&ar->ab->mon_reap_timer, jiffies +
			  msecs_to_jiffies(ATH11K_MON_TIMER_INTERVAL));
#endif
	return ret;
}

int
qwx_mac_txpower_recalc(struct qwx_softc *sc, struct qwx_pdev *pdev)
{
	struct qwx_vif *arvif;
	int ret, txpower = -1;
	uint32_t param;
	uint32_t min_tx_power = sc->target_caps.hw_min_tx_power;
	uint32_t max_tx_power = sc->target_caps.hw_max_tx_power;
#ifdef notyet
	lockdep_assert_held(&ar->conf_mutex);
#endif
	TAILQ_FOREACH(arvif, &sc->vif_list, entry) {
		if (arvif->txpower <= 0)
			continue;

		if (txpower == -1)
			txpower = arvif->txpower;
		else
			txpower = MIN(txpower, arvif->txpower);
	}

	if (txpower == -1)
		return 0;

	/* txpwr is set as 2 units per dBm in FW*/
	txpower = MIN(MAX(min_tx_power, txpower), max_tx_power) * 2;
	DNPRINTF(QWX_D_MAC, "txpower to set in hw %d\n", txpower / 2);

	if (pdev->cap.supported_bands & WMI_HOST_WLAN_2G_CAP) {
		param = WMI_PDEV_PARAM_TXPOWER_LIMIT2G;
		ret = qwx_wmi_pdev_set_param(sc, param, txpower,
		    pdev->pdev_id);
		if (ret)
			goto fail;
	}

	if (pdev->cap.supported_bands & WMI_HOST_WLAN_5G_CAP) {
		param = WMI_PDEV_PARAM_TXPOWER_LIMIT5G;
		ret = qwx_wmi_pdev_set_param(sc, param, txpower,
		    pdev->pdev_id);
		if (ret)
			goto fail;
	}

	return 0;

fail:
	DNPRINTF(QWX_D_MAC, "%s: failed to recalc txpower limit %d "
	    "using pdev param %d: %d\n", sc->sc_dev.dv_xname, txpower / 2,
	    param, ret);

	return ret;
}

int
qwx_mac_op_start(struct qwx_pdev *pdev)
{
	struct qwx_softc *sc = pdev->sc;
	int ret;

	ret = qwx_wmi_pdev_set_param(sc, WMI_PDEV_PARAM_PMF_QOS, 1,
	    pdev->pdev_id);
	if (ret) {
		printf("%s: failed to enable PMF QOS for pdev %d: %d\n",
		    sc->sc_dev.dv_xname, pdev->pdev_id, ret);
		goto err;
	}

	ret = qwx_wmi_pdev_set_param(sc, WMI_PDEV_PARAM_DYNAMIC_BW, 1,
	    pdev->pdev_id);
	if (ret) {
		printf("%s: failed to enable dynamic bw for pdev %d: %d\n",
		    sc->sc_dev.dv_xname, pdev->pdev_id, ret);
		goto err;
	}

	if (isset(sc->wmi.svc_map, WMI_TLV_SERVICE_SPOOF_MAC_SUPPORT)) {
		ret = qwx_wmi_scan_prob_req_oui(sc, sc->mac_addr,
		    pdev->pdev_id);
		if (ret) {
			printf("%s: failed to set prob req oui for "
			    "pdev %d: %i\n", sc->sc_dev.dv_xname,
			    pdev->pdev_id, ret);
			goto err;
		}
	}

	ret = qwx_wmi_pdev_set_param(sc, WMI_PDEV_PARAM_ARP_AC_OVERRIDE, 0,
	    pdev->pdev_id);
	if (ret) {
		printf("%s: failed to set ac override for ARP for "
		    "pdev %d: %d\n", sc->sc_dev.dv_xname, pdev->pdev_id, ret);
		goto err;
	}

	ret = qwx_wmi_send_dfs_phyerr_offload_enable_cmd(sc, pdev->pdev_id);
	if (ret) {
		printf("%s: failed to offload radar detection for "
		    "pdev %d: %d\n", sc->sc_dev.dv_xname, pdev->pdev_id, ret);
		goto err;
	}

	ret = qwx_dp_tx_htt_h2t_ppdu_stats_req(sc, HTT_PPDU_STATS_TAG_DEFAULT,
	    pdev->pdev_id);
	if (ret) {
		printf("%s: failed to req ppdu stats for pdev %d: %d\n",
		    sc->sc_dev.dv_xname, pdev->pdev_id, ret);
		goto err;
	}

	ret = qwx_wmi_pdev_set_param(sc, WMI_PDEV_PARAM_MESH_MCAST_ENABLE, 1,
	    pdev->pdev_id);
	if (ret) {
		printf("%s: failed to enable MESH MCAST ENABLE for "
		    "pdev %d: %d\n", sc->sc_dev.dv_xname, pdev->pdev_id, ret);
		goto err;
	}

	qwx_set_antenna(pdev, pdev->cap.tx_chain_mask, pdev->cap.rx_chain_mask);

	/* TODO: Do we need to enable ANI? */

	ret = qwx_reg_update_chan_list(sc, pdev->pdev_id);
	if (ret) {
		printf("%s: failed to update channel list for pdev %d: %d\n",
		    sc->sc_dev.dv_xname, pdev->pdev_id, ret);
		goto err;
	}

	sc->num_started_vdevs = 0;
	sc->num_created_vdevs = 0;
	sc->num_peers = 0;
	sc->allocated_vdev_map = 0;

	/* Configure monitor status ring with default rx_filter to get rx status
	 * such as rssi, rx_duration.
	 */
	ret = qwx_mac_config_mon_status_default(sc, 1);
	if (ret) {
		printf("%s: failed to configure monitor status ring "
		    "with default rx_filter: (%d)\n",
		    sc->sc_dev.dv_xname, ret);
		goto err;
	}

	/* Configure the hash seed for hash based reo dest ring selection */
	qwx_wmi_pdev_lro_cfg(sc, pdev->pdev_id);

	/* allow device to enter IMPS */
	if (sc->hw_params.idle_ps) {
		ret = qwx_wmi_pdev_set_param(sc, WMI_PDEV_PARAM_IDLE_PS_CONFIG,
		    1, pdev->pdev_id);
		if (ret) {
			printf("%s: failed to enable idle ps: %d\n",
			    sc->sc_dev.dv_xname, ret);
			goto err;
		}
	}
#ifdef notyet
	mutex_unlock(&ar->conf_mutex);
#endif
#if 0
	rcu_assign_pointer(ab->pdevs_active[ar->pdev_idx],
			   &ab->pdevs[ar->pdev_idx]);
#endif

	return 0;
err:
#ifdef notyet
	ar->state = ATH11K_STATE_OFF;
	mutex_unlock(&ar->conf_mutex);
#endif
	return ret;
}

int
qwx_mac_setup_vdev_params_mbssid(struct qwx_vif *arvif,
    uint32_t *flags, uint32_t *tx_vdev_id)
{
	*tx_vdev_id = 0;
	*flags = WMI_HOST_VDEV_FLAGS_NON_MBSSID_AP;
	return 0;
}

int
qwx_mac_setup_vdev_create_params(struct qwx_vif *arvif, struct qwx_pdev *pdev,
    struct vdev_create_params *params)
{
	struct qwx_softc *sc = arvif->sc;
	int ret;

	params->if_id = arvif->vdev_id;
	params->type = arvif->vdev_type;
	params->subtype = arvif->vdev_subtype;
	params->pdev_id = pdev->pdev_id;
	params->mbssid_flags = 0;
	params->mbssid_tx_vdev_id = 0;

	if (!isset(sc->wmi.svc_map,
	    WMI_TLV_SERVICE_MBSS_PARAM_IN_VDEV_START_SUPPORT)) {
		ret = qwx_mac_setup_vdev_params_mbssid(arvif,
		    &params->mbssid_flags, &params->mbssid_tx_vdev_id);
		if (ret)
			return ret;
	}

	if (pdev->cap.supported_bands & WMI_HOST_WLAN_2G_CAP) {
		params->chains[0].tx = sc->num_tx_chains;
		params->chains[0].rx = sc->num_rx_chains;
	}
	if (pdev->cap.supported_bands & WMI_HOST_WLAN_5G_CAP) {
		params->chains[1].tx = sc->num_tx_chains;
		params->chains[1].rx = sc->num_rx_chains;
	}
#if 0
	if (pdev->cap.supported_bands & WMI_HOST_WLAN_5G_CAP &&
	    ar->supports_6ghz) {
		params->chains[NL80211_BAND_6GHZ].tx = ar->num_tx_chains;
		params->chains[NL80211_BAND_6GHZ].rx = ar->num_rx_chains;
	}
#endif
	return 0;
}

int
qwx_mac_op_update_vif_offload(struct qwx_softc *sc, struct qwx_pdev *pdev,
    struct qwx_vif *arvif)
{
	uint32_t param_id, param_value;
	int ret;

	param_id = WMI_VDEV_PARAM_TX_ENCAP_TYPE;
	if (test_bit(ATH11K_FLAG_RAW_MODE, sc->sc_flags))
		param_value = ATH11K_HW_TXRX_RAW;
	else
		param_value = ATH11K_HW_TXRX_NATIVE_WIFI;

	ret = qwx_wmi_vdev_set_param_cmd(sc, arvif->vdev_id, pdev->pdev_id,
	    param_id, param_value);
	if (ret) {
		printf("%s: failed to set vdev %d tx encap mode: %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
		return ret;
	}

	param_id = WMI_VDEV_PARAM_RX_DECAP_TYPE;
	if (test_bit(ATH11K_FLAG_RAW_MODE, sc->sc_flags))
		param_value = ATH11K_HW_TXRX_RAW;
	else
		param_value = ATH11K_HW_TXRX_NATIVE_WIFI;

	ret = qwx_wmi_vdev_set_param_cmd(sc, arvif->vdev_id, pdev->pdev_id,
	    param_id, param_value);
	if (ret) {
		printf("%s: failed to set vdev %d rx decap mode: %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
		return ret;
	}

	return 0;
}

void
qwx_mac_vdev_delete(struct qwx_softc *sc, struct qwx_vif *arvif)
{
	printf("%s: not implemented\n", __func__);
}

int
qwx_mac_op_add_interface(struct qwx_pdev *pdev)
{
	struct qwx_softc *sc = pdev->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct qwx_vif *arvif = NULL;
	struct vdev_create_params vdev_param = { 0 };
#if 0
	struct peer_create_params peer_param;
#endif
	uint32_t param_id, param_value;
	uint16_t nss;
#if 0
	int i;
	int fbret;
#endif
	int ret, bit;
#ifdef notyet
	mutex_lock(&ar->conf_mutex);
#endif
#if 0
	if (vif->type == NL80211_IFTYPE_AP &&
	    ar->num_peers > (ar->max_num_peers - 1)) {
		ath11k_warn(ab, "failed to create vdev due to insufficient peer entry resource in firmware\n");
		ret = -ENOBUFS;
		goto err;
	}
#endif
	if (sc->num_created_vdevs > (TARGET_NUM_VDEVS(sc) - 1)) {
		printf("%s: failed to create vdev %u, reached vdev limit %d\n",
		    sc->sc_dev.dv_xname, sc->num_created_vdevs,
		    TARGET_NUM_VDEVS(sc));
		ret = EBUSY;
		goto err;
	}

	arvif = malloc(sizeof(*arvif), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (arvif == NULL) {
		ret = ENOMEM;
		goto err;
	}

	arvif->sc = sc;
#if 0
	INIT_DELAYED_WORK(&arvif->connection_loss_work,
			  ath11k_mac_vif_sta_connection_loss_work);
	for (i = 0; i < ARRAY_SIZE(arvif->bitrate_mask.control); i++) {
		arvif->bitrate_mask.control[i].legacy = 0xffffffff;
		arvif->bitrate_mask.control[i].gi = 0;
		memset(arvif->bitrate_mask.control[i].ht_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].ht_mcs));
		memset(arvif->bitrate_mask.control[i].vht_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].vht_mcs));
		memset(arvif->bitrate_mask.control[i].he_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].he_mcs));
	}
#endif

	bit = ffs(sc->free_vdev_map);

	arvif->vdev_id = bit;
	arvif->vdev_subtype = WMI_VDEV_SUBTYPE_NONE;

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		arvif->vdev_type = WMI_VDEV_TYPE_STA;
		break;
#if 0
	case NL80211_IFTYPE_MESH_POINT:
		arvif->vdev_subtype = WMI_VDEV_SUBTYPE_MESH_11S;
		fallthrough;
	case NL80211_IFTYPE_AP:
		arvif->vdev_type = WMI_VDEV_TYPE_AP;
		break;
	case NL80211_IFTYPE_MONITOR:
		arvif->vdev_type = WMI_VDEV_TYPE_MONITOR;
		ar->monitor_vdev_id = bit;
		break;
#endif
	default:
		printf("%s: invalid operating mode %d\n",
		    sc->sc_dev.dv_xname, ic->ic_opmode);
		ret = EINVAL;
		goto err;
	}

	DNPRINTF(QWX_D_MAC,
	    "%s: add interface id %d type %d subtype %d map 0x%x\n",
	    __func__, arvif->vdev_id, arvif->vdev_type,
	    arvif->vdev_subtype, sc->free_vdev_map);

	ret = qwx_mac_setup_vdev_create_params(arvif, pdev, &vdev_param);
	if (ret) {
		printf("%s: failed to create vdev parameters %d: %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
		goto err;
	}

	ret = qwx_wmi_vdev_create(sc, sc->mac_addr, &vdev_param);
	if (ret) {
		printf("%s: failed to create WMI vdev %d %s: %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_id,
		    ether_sprintf(sc->mac_addr), ret);
		goto err;
	}

	sc->num_created_vdevs++;
	DNPRINTF(QWX_D_MAC, "%s: vdev %s created, vdev_id %d\n", __func__,
	    ether_sprintf(sc->mac_addr), arvif->vdev_id);
	sc->allocated_vdev_map |= 1U << arvif->vdev_id;
	sc->free_vdev_map &= ~(1U << arvif->vdev_id);
#ifdef notyet
	spin_lock_bh(&ar->data_lock);
#endif
	TAILQ_INSERT_TAIL(&sc->vif_list, arvif, entry);
#ifdef notyet
	spin_unlock_bh(&ar->data_lock);
#endif
	ret = qwx_mac_op_update_vif_offload(sc, pdev, arvif);
	if (ret)
		goto err_vdev_del;

	nss = qwx_get_num_chains(sc->cfg_tx_chainmask) ? : 1;
	ret = qwx_wmi_vdev_set_param_cmd(sc, arvif->vdev_id, pdev->pdev_id,
	    WMI_VDEV_PARAM_NSS, nss);
	if (ret) {
		printf("%s: failed to set vdev %d chainmask 0x%x, nss %d: %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_id, sc->cfg_tx_chainmask,
		    nss, ret);
		goto err_vdev_del;
	}

	switch (arvif->vdev_type) {
#if 0
	case WMI_VDEV_TYPE_AP:
		peer_param.vdev_id = arvif->vdev_id;
		peer_param.peer_addr = vif->addr;
		peer_param.peer_type = WMI_PEER_TYPE_DEFAULT;
		ret = ath11k_peer_create(ar, arvif, NULL, &peer_param);
		if (ret) {
			ath11k_warn(ab, "failed to vdev %d create peer for AP: %d\n",
				    arvif->vdev_id, ret);
			goto err_vdev_del;
		}

		ret = ath11k_mac_set_kickout(arvif);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set vdev %i kickout parameters: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}

		ath11k_mac_11d_scan_stop_all(ar->ab);
		break;
#endif
	case WMI_VDEV_TYPE_STA:
		param_id = WMI_STA_PS_PARAM_RX_WAKE_POLICY;
		param_value = WMI_STA_PS_RX_WAKE_POLICY_WAKE;
		ret = qwx_wmi_set_sta_ps_param(sc, arvif->vdev_id,
		    pdev->pdev_id, param_id, param_value);
		if (ret) {
			printf("%s: failed to set vdev %d RX wake policy: %d\n",
			    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
			goto err_peer_del;
		}

		param_id = WMI_STA_PS_PARAM_TX_WAKE_THRESHOLD;
		param_value = WMI_STA_PS_TX_WAKE_THRESHOLD_ALWAYS;
		ret = qwx_wmi_set_sta_ps_param(sc, arvif->vdev_id,
		    pdev->pdev_id, param_id, param_value);
		if (ret) {
			printf("%s: failed to set vdev %d "
			    "TX wake threshold: %d\n",
			    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
			goto err_peer_del;
		}

		param_id = WMI_STA_PS_PARAM_PSPOLL_COUNT;
		param_value = WMI_STA_PS_PSPOLL_COUNT_NO_MAX;
		ret = qwx_wmi_set_sta_ps_param(sc, arvif->vdev_id,
		    pdev->pdev_id, param_id, param_value);
		if (ret) {
			printf("%s: failed to set vdev %d pspoll count: %d\n",
			    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
			goto err_peer_del;
		}

		ret = qwx_wmi_pdev_set_ps_mode(sc, arvif->vdev_id,
		    pdev->pdev_id, WMI_STA_PS_MODE_DISABLED);
		if (ret) {
			printf("%s: failed to disable vdev %d ps mode: %d\n",
			    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
			goto err_peer_del;
		}

		if (isset(sc->wmi.svc_map, WMI_TLV_SERVICE_11D_OFFLOAD)) {
			sc->completed_11d_scan = 0;
			sc->state_11d = ATH11K_11D_PREPARING;
		}
		break;
#if 0
	case WMI_VDEV_TYPE_MONITOR:
		set_bit(ATH11K_FLAG_MONITOR_VDEV_CREATED, &ar->monitor_flags);
		break;
#endif
	default:
		printf("%s: invalid vdev type %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_type);
		ret = EINVAL;
		goto err;
	}

	arvif->txpower = 40;
	ret = qwx_mac_txpower_recalc(sc, pdev);
	if (ret)
		goto err_peer_del;

	param_id = WMI_VDEV_PARAM_RTS_THRESHOLD;
	param_value = ic->ic_rtsthreshold;
	ret = qwx_wmi_vdev_set_param_cmd(sc, arvif->vdev_id, pdev->pdev_id,
	    param_id, param_value);
	if (ret) {
		printf("%s: failed to set rts threshold for vdev %d: %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
		goto err_peer_del;
	}

	qwx_dp_vdev_tx_attach(sc, pdev, arvif);
#if 0
	if (vif->type != NL80211_IFTYPE_MONITOR &&
	    test_bit(ATH11K_FLAG_MONITOR_CONF_ENABLED, &ar->monitor_flags)) {
		ret = ath11k_mac_monitor_vdev_create(ar);
		if (ret)
			ath11k_warn(ar->ab, "failed to create monitor vdev during add interface: %d",
				    ret);
	}

	mutex_unlock(&ar->conf_mutex);
#endif
	return 0;

err_peer_del:
#if 0
	if (arvif->vdev_type == WMI_VDEV_TYPE_AP) {
		fbret = qwx_peer_delete(sc, arvif->vdev_id, vif->addr);
		if (fbret) {
			printf("%s: fallback fail to delete peer addr %pM "
			    "vdev_id %d ret %d\n", sc->sc_dev.dv_xname,
			    vif->addr, arvif->vdev_id, fbret);
			goto err;
		}
	}
#endif
err_vdev_del:
	qwx_mac_vdev_delete(sc, arvif);
#ifdef notyet
	spin_lock_bh(&ar->data_lock);
#endif
	TAILQ_REMOVE(&sc->vif_list, arvif, entry);
#ifdef notyet
	spin_unlock_bh(&ar->data_lock);
#endif

err:
#ifdef notyet
	mutex_unlock(&ar->conf_mutex);
#endif
	free(arvif, M_DEVBUF, sizeof(*arvif));
	return ret;
}

int
qwx_mac_start(struct qwx_softc *sc)
{
	struct qwx_pdev *pdev;
	int i, error;

	for (i = 0; i < sc->num_radios; i++) {
		pdev = &sc->pdevs[i];
		error = qwx_mac_op_start(pdev);
		if (error)
			return error;

		error = qwx_mac_op_add_interface(pdev);
		if (error)
			return error;
	}

	return 0;
}

void
qwx_init_task(void *arg)
{
	struct qwx_softc *sc = arg;

	printf("%s: %s not implemented\n", sc->sc_dev.dv_xname, __func__);
}

void
qwx_mac_11d_scan_start(struct qwx_softc *sc, struct qwx_vif *arvif)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wmi_11d_scan_start_params param;
	int ret;
#ifdef notyet
	mutex_lock(&ar->ab->vdev_id_11d_lock);
#endif
	DNPRINTF(QWX_D_MAC, "%s: vdev id for 11d scan %d\n", __func__,
	    sc->vdev_id_11d_scan);
#if 0
	if (ar->regdom_set_by_user)
		goto fin;
#endif
	if (sc->vdev_id_11d_scan != QWX_11D_INVALID_VDEV_ID)
		goto fin;

	if (!isset(sc->wmi.svc_map, WMI_TLV_SERVICE_11D_OFFLOAD))
		goto fin;

	if (ic->ic_opmode != IEEE80211_M_STA)
		goto fin;

	param.vdev_id = arvif->vdev_id;
	param.start_interval_msec = 0;
	param.scan_period_msec = QWX_SCAN_11D_INTERVAL;

	DNPRINTF(QWX_D_MAC, "%s: start 11d scan\n", __func__);

	ret = qwx_wmi_send_11d_scan_start_cmd(sc, &param,
	   0 /* TODO: derive pdev ID from arvif somehow? */);
	if (ret) {
		printf("%s: failed to start 11d scan; vdev: %d ret: %d\n",
		    sc->sc_dev.dv_xname, arvif->vdev_id, ret);
	} else {
		sc->vdev_id_11d_scan = arvif->vdev_id;
		if (sc->state_11d == ATH11K_11D_PREPARING)
			sc->state_11d = ATH11K_11D_RUNNING;
	}
fin:
	if (sc->state_11d == ATH11K_11D_PREPARING) {
		sc->state_11d = ATH11K_11D_IDLE;
		sc->completed_11d_scan = 0;
	}
#ifdef notyet
	mutex_unlock(&ar->ab->vdev_id_11d_lock);
#endif
}

void
qwx_mac_scan_finish(struct qwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	enum ath11k_scan_state ostate;

#ifdef notyet
	lockdep_assert_held(&ar->data_lock);
#endif
	ostate = sc->scan.state;
	switch (ostate) {
	case ATH11K_SCAN_IDLE:
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
#if 0
		if (ar->scan.is_roc && ar->scan.roc_notify)
			ieee80211_remain_on_channel_expired(ar->hw);
		fallthrough;
#endif
	case ATH11K_SCAN_STARTING:
		sc->scan.state = ATH11K_SCAN_IDLE;
		sc->scan_channel = 0;
		sc->scan.roc_freq = 0;

		timeout_del(&sc->scan.timeout);
		if (!sc->scan.is_roc)
			ieee80211_end_scan(ifp);
#if 0
		complete_all(&ar->scan.completed);
#endif
		break;
	}
}

void
qwx_wmi_start_scan_init(struct qwx_softc *sc, struct scan_req_params *arg)
{
	/* setup commonly used values */
	arg->scan_req_id = 1;
	if (sc->state_11d == ATH11K_11D_PREPARING)
		arg->scan_priority = WMI_SCAN_PRIORITY_MEDIUM;
	else
		arg->scan_priority = WMI_SCAN_PRIORITY_LOW;
	arg->dwell_time_active = 50;
	arg->dwell_time_active_2g = 0;
	arg->dwell_time_passive = 150;
	arg->dwell_time_active_6g = 40;
	arg->dwell_time_passive_6g = 30;
	arg->min_rest_time = 50;
	arg->max_rest_time = 500;
	arg->repeat_probe_time = 0;
	arg->probe_spacing_time = 0;
	arg->idle_time = 0;
	arg->max_scan_time = 20000;
	arg->probe_delay = 5;
	arg->notify_scan_events = WMI_SCAN_EVENT_STARTED |
	    WMI_SCAN_EVENT_COMPLETED | WMI_SCAN_EVENT_BSS_CHANNEL |
	    WMI_SCAN_EVENT_FOREIGN_CHAN | WMI_SCAN_EVENT_DEQUEUED;
	arg->scan_flags |= WMI_SCAN_CHAN_STAT_EVENT;

	if (isset(sc->wmi.svc_map,
	    WMI_TLV_SERVICE_PASSIVE_SCAN_START_TIME_ENHANCE))
		arg->scan_ctrl_flags_ext |=
		    WMI_SCAN_FLAG_EXT_PASSIVE_SCAN_START_TIME_ENHANCE;

	arg->num_bssid = 1;

	/* fill bssid_list[0] with 0xff, otherwise bssid and RA will be
	 * ZEROs in probe request
	 */
	IEEE80211_ADDR_COPY(arg->bssid_list[0].addr, etheranyaddr);
}

enum ath11k_spectral_mode
qwx_spectral_get_mode(struct qwx_softc *sc)
{
#if 0
	if (sc->spectral.enabled)
		return ar->spectral.mode;
	else
#endif
		return ATH11K_SPECTRAL_DISABLED;
}

void
qwx_spectral_reset_buffer(struct qwx_softc *sc)
{
	printf("%s: not implemented\n", __func__);
}

int
qwx_scan_stop(struct qwx_softc *sc)
{
	struct scan_cancel_param arg = {
		.req_type = WLAN_SCAN_CANCEL_SINGLE,
		.scan_id = ATH11K_SCAN_ID,
	};
	int ret;
#ifdef notyet
	lockdep_assert_held(&ar->conf_mutex);
#endif
	/* TODO: Fill other STOP Params */
	arg.pdev_id = 0; /* TODO: derive pdev ID somehow? */
	arg.vdev_id = sc->scan.vdev_id;

	ret = qwx_wmi_send_scan_stop_cmd(sc, &arg);
	if (ret) {
		printf("%s: failed to stop wmi scan: %d\n",
		    sc->sc_dev.dv_xname, ret);
		goto out;
	}

	while (sc->scan.state != ATH11K_SCAN_IDLE) {
		ret = tsleep_nsec(&sc->scan.state, 0, "qwxscstop",
		    SEC_TO_NSEC(3));
		if (ret) {
			printf("%s: scan stop timeout\n", sc->sc_dev.dv_xname);
			break;
		}
	}
out:
	/* Scan state should be updated upon scan completion but in case
	 * firmware fails to deliver the event (for whatever reason) it is
	 * desired to clean up scan state anyway. Firmware may have just
	 * dropped the scan completion event delivery due to transport pipe
	 * being overflown with data and/or it can recover on its own before
	 * next scan request is submitted.
	 */
#ifdef notyet
	spin_lock_bh(&ar->data_lock);
#endif
	if (sc->scan.state != ATH11K_SCAN_IDLE)
		qwx_mac_scan_finish(sc);
#ifdef notyet
	spin_unlock_bh(&ar->data_lock);
#endif
	return ret;
}

void
qwx_scan_timeout(void *arg)
{
	struct qwx_softc *sc = arg;
	int s = splnet();

#ifdef notyet
	mutex_lock(&ar->conf_mutex);
#endif
	printf("%s\n", __func__);
	qwx_scan_abort(sc);
#ifdef notyet
	mutex_unlock(&ar->conf_mutex);
#endif
	splx(s);
}

int
qwx_start_scan(struct qwx_softc *sc, struct scan_req_params *arg)
{
	int ret;
	unsigned long timeout = 1;
#ifdef notyet
	lockdep_assert_held(&ar->conf_mutex);
#endif
	if (qwx_spectral_get_mode(sc) == ATH11K_SPECTRAL_BACKGROUND)
		qwx_spectral_reset_buffer(sc);

	ret = qwx_wmi_send_scan_start_cmd(sc, arg);
	if (ret)
		return ret;

	if (isset(sc->wmi.svc_map, WMI_TLV_SERVICE_11D_OFFLOAD)) {
		timeout = 5;
#if 0
		if (ar->supports_6ghz)
			timeout += 5 * HZ;
#endif
	}

	while (sc->scan.state == ATH11K_SCAN_STARTING) {
		ret = tsleep_nsec(&sc->scan.state, 0, "qwxscan",
		    SEC_TO_NSEC(timeout));
		if (ret) {
			printf("%s: scan start timeout\n", sc->sc_dev.dv_xname);
			qwx_scan_stop(sc);
			break;
		}
	}

#ifdef notyet
	spin_lock_bh(&ar->data_lock);
	spin_unlock_bh(&ar->data_lock);
#endif
	return ret;
}

#define ATH11K_MAC_SCAN_CMD_EVT_OVERHEAD		200 /* in msecs */

int
qwx_scan(struct qwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct qwx_vif *arvif = TAILQ_FIRST(&sc->vif_list);
	struct scan_req_params *arg = NULL;
	struct ieee80211_channel *chan, *lastc;
	int ret = 0, num_channels, i;
	uint32_t scan_timeout;

	if (arvif == NULL) {
		printf("%s: no vdev found\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}

	/*
	 * TODO Will we need separate scan iterations on devices with
	 * multiple radios?
	 */
	if (sc->num_radios > 1)
		printf("%s: TODO: only scanning with first vdev\n", __func__);

	/* Firmwares advertising the support of triggering 11D algorithm
	 * on the scan results of a regular scan expects driver to send
	 * WMI_11D_SCAN_START_CMDID before sending WMI_START_SCAN_CMDID.
	 * With this feature, separate 11D scan can be avoided since
	 * regdomain can be determined with the scan results of the
	 * regular scan.
	 */
	if (sc->state_11d == ATH11K_11D_PREPARING &&
	    isset(sc->wmi.svc_map, WMI_TLV_SERVICE_SUPPORT_11D_FOR_HOST_SCAN))
		qwx_mac_11d_scan_start(sc, arvif);
#ifdef notyet
	mutex_lock(&ar->conf_mutex);

	spin_lock_bh(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
		sc->scan.started = 0;
		sc->scan.completed = 0;
		sc->scan.state = ATH11K_SCAN_STARTING;
		sc->scan.is_roc = 0;
		sc->scan.vdev_id = arvif->vdev_id;
		ret = 0;
		break;
	case ATH11K_SCAN_STARTING:
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		ret = EBUSY;
		break;
	}
#ifdef notyet
	spin_unlock_bh(&ar->data_lock);
#endif
	if (ret)
		goto exit;

	arg = malloc(sizeof(*arg), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (!arg) {
		ret = ENOMEM;
		goto exit;
	}

	qwx_wmi_start_scan_init(sc, arg);
	arg->vdev_id = arvif->vdev_id;
	arg->scan_id = ATH11K_SCAN_ID;

	if (ic->ic_des_esslen != 0) {
		arg->num_ssids = 1;
		arg->ssid[0].length  = ic->ic_des_esslen;
		memcpy(&arg->ssid[0].ssid, ic->ic_des_essid,
		    ic->ic_des_esslen);
	} else
		arg->scan_flags |= WMI_SCAN_FLAG_PASSIVE;

	lastc = &ic->ic_channels[IEEE80211_CHAN_MAX];
	num_channels = 0;
	for (chan = &ic->ic_channels[1]; chan <= lastc; chan++) {
		if (chan->ic_flags == 0)
			continue;
		num_channels++;
	}
	if (num_channels) {
		arg->num_chan = num_channels;
		arg->chan_list = mallocarray(arg->num_chan,
		    sizeof(*arg->chan_list), M_DEVBUF, M_NOWAIT | M_ZERO);

		if (!arg->chan_list) {
			ret = ENOMEM;
			goto exit;
		}

		i = 0;
		for (chan = &ic->ic_channels[1]; chan <= lastc; chan++) {
			if (chan->ic_flags == 0)
				continue;
			if (isset(sc->wmi.svc_map,
			    WMI_TLV_SERVICE_SCAN_CONFIG_PER_CHANNEL)) {
				arg->chan_list[i++] = chan->ic_freq &
				    WMI_SCAN_CONFIG_PER_CHANNEL_MASK;
#if 0
				/* If NL80211_SCAN_FLAG_COLOCATED_6GHZ is set in scan
				 * flags, then scan all PSC channels in 6 GHz band and
				 * those non-PSC channels where RNR IE is found during
				 * the legacy 2.4/5 GHz scan.
				 * If NL80211_SCAN_FLAG_COLOCATED_6GHZ is not set,
				 * then all channels in 6 GHz will be scanned.
				 */
				if (req->channels[i]->band == NL80211_BAND_6GHZ &&
				    req->flags & NL80211_SCAN_FLAG_COLOCATED_6GHZ &&
				    !cfg80211_channel_is_psc(req->channels[i]))
					arg->chan_list[i] |=
						WMI_SCAN_CH_FLAG_SCAN_ONLY_IF_RNR_FOUND;
#endif
			} else {
				arg->chan_list[i++] = chan->ic_freq;
			}
		}
	}
#if 0
	if (req->flags & NL80211_SCAN_FLAG_RANDOM_ADDR) {
		arg->scan_f_add_spoofed_mac_in_probe = 1;
		ether_addr_copy(arg->mac_addr.addr, req->mac_addr);
		ether_addr_copy(arg->mac_mask.addr, req->mac_addr_mask);
	}
#endif
	scan_timeout = 5000;

	/* Add a margin to account for event/command processing */
	scan_timeout += ATH11K_MAC_SCAN_CMD_EVT_OVERHEAD;

	ret = qwx_start_scan(sc, arg);
	if (ret) {
		printf("%s: failed to start hw scan: %d\n",
		    sc->sc_dev.dv_xname, ret);
#ifdef notyet
		spin_lock_bh(&ar->data_lock);
#endif
		sc->scan.state = ATH11K_SCAN_IDLE;
#ifdef notyet
		spin_unlock_bh(&ar->data_lock);
#endif
	}
#if 0
	timeout_add_msec(&sc->scan.timeout, scan_timeout);
#endif
exit:
	if (arg) {
		free(arg->chan_list, M_DEVBUF,
		    arg->num_chan * sizeof(*arg->chan_list));
#if 0
		kfree(arg->extraie.ptr);
#endif
		free(arg, M_DEVBUF, sizeof(*arg));
	}
#ifdef notyet
	mutex_unlock(&ar->conf_mutex);
#endif
	if (sc->state_11d == ATH11K_11D_PREPARING)
		qwx_mac_11d_scan_start(sc, arvif);

	return ret;
}

void
qwx_scan_abort(struct qwx_softc *sc)
{
	int ret;
#ifdef notyet
	lockdep_assert_held(&ar->conf_mutex);

	spin_lock_bh(&ar->data_lock);
#endif
	switch (sc->scan.state) {
	case ATH11K_SCAN_IDLE:
		/* This can happen if timeout worker kicked in and called
		 * abortion while scan completion was being processed.
		 */
		break;
	case ATH11K_SCAN_STARTING:
	case ATH11K_SCAN_ABORTING:
		printf("%s: refusing scan abortion due to invalid "
		    "scan state: %d\n", sc->sc_dev.dv_xname, sc->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
		sc->scan.state = ATH11K_SCAN_ABORTING;
#ifdef notyet
		spin_unlock_bh(&ar->data_lock);
#endif
		ret = qwx_scan_stop(sc);
		if (ret)
			printf("%s: failed to abort scan: %d\n",
			    sc->sc_dev.dv_xname, ret);
#ifdef notyet
		spin_lock_bh(&ar->data_lock);
#endif
		break;
	}
#ifdef notyet
	spin_unlock_bh(&ar->data_lock);
#endif
}

int
qwx_auth(struct qwx_softc *sc)
{
	printf("%s: not implemented\n", __func__);
	return ENOTSUP;
}

int
qwx_deauth(struct qwx_softc *sc)
{
	printf("%s: not implemented\n", __func__);
	return ENOTSUP;
}

int
qwx_assoc(struct qwx_softc *sc)
{
	printf("%s: not implemented\n", __func__);
	return ENOTSUP;
}

int
qwx_disassoc(struct qwx_softc *sc)
{
	printf("%s: not implemented\n", __func__);
	return ENOTSUP;
}

int
qwx_run(struct qwx_softc *sc)
{
	printf("%s: not implemented\n", __func__);
	return ENOTSUP;
}

int
qwx_run_stop(struct qwx_softc *sc)
{
	printf("%s: not implemented\n", __func__);
	return ENOTSUP;
}

int
qwx_attach(struct qwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int error, i;

	task_set(&sc->init_task, qwx_init_task, sc);
	task_set(&sc->newstate_task, qwx_newstate_task, sc);
	timeout_set_proc(&sc->scan.timeout, qwx_scan_timeout, sc);

	for (i = 0; i < nitems(sc->pdevs); i++)
		sc->pdevs[i].sc = sc;

	TAILQ_INIT(&sc->vif_list);

	error = qwx_init(ifp);
	if (error)
		return error;

	/* Turn device off until interface comes up. */
	qwx_core_deinit(sc);

	return 0;
}

void
qwx_detach(struct qwx_softc *sc)
{
	if (sc->fwmem) {
		qwx_dmamem_free(sc->sc_dmat, sc->fwmem);
		sc->fwmem = NULL;
	}

	if (sc->m3_mem) {
		qwx_dmamem_free(sc->sc_dmat, sc->m3_mem);
		sc->m3_mem = NULL;
	}
}

struct qwx_dmamem *
qwx_dmamem_alloc(bus_dma_tag_t dmat, bus_size_t size, bus_size_t align)
{
	struct qwx_dmamem *adm;
	int nsegs;

	adm = malloc(sizeof(*adm), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (adm == NULL)
		return NULL;
	adm->size = size;

	if (bus_dmamap_create(dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &adm->map) != 0)
		goto admfree;

	if (bus_dmamem_alloc_range(dmat, size, align, 0, &adm->seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO, 0, 0xffffffff) != 0)
		goto destroy;

	if (bus_dmamem_map(dmat, &adm->seg, nsegs, size,
	    &adm->kva, BUS_DMA_NOWAIT | BUS_DMA_COHERENT) != 0)
		goto free;

	if (bus_dmamap_load_raw(dmat, adm->map, &adm->seg, nsegs, size,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(adm->kva, size);

	return adm;

unmap:
	bus_dmamem_unmap(dmat, adm->kva, size);
free:
	bus_dmamem_free(dmat, &adm->seg, 1);
destroy:
	bus_dmamap_destroy(dmat, adm->map);
admfree:
	free(adm, M_DEVBUF, sizeof(*adm));

	return NULL;
}

void
qwx_dmamem_free(bus_dma_tag_t dmat, struct qwx_dmamem *adm)
{
	bus_dmamem_unmap(dmat, adm->kva, adm->size);
	bus_dmamem_free(dmat, &adm->seg, 1);
	bus_dmamap_destroy(dmat, adm->map);
	free(adm, M_DEVBUF, sizeof(*adm));
}
