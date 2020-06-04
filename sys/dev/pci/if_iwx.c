/*	$OpenBSD: if_iwx.c,v 1.19 2020/05/26 12:07:36 stsp Exp $	*/

/*
 * Copyright (c) 2014, 2016 genua gmbh <info@genua.de>
 *   Author: Stefan Sperling <stsp@openbsd.org>
 * Copyright (c) 2014 Fixup Software Ltd.
 * Copyright (c) 2017, 2019, 2020 Stefan Sperling <stsp@openbsd.org>
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

/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 ******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************
 */

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/endian.h>

#include <sys/refcnt.h>
#include <sys/task.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_mira.h>
#include <net80211/ieee80211_radiotap.h>

#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

#define IC2IFP(_ic_) (&(_ic_)->ic_if)

#define le16_to_cpup(_a_) (le16toh(*(const uint16_t *)(_a_)))
#define le32_to_cpup(_a_) (le32toh(*(const uint32_t *)(_a_)))

#ifdef IWX_DEBUG
#define DPRINTF(x)	do { if (iwx_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (iwx_debug >= (n)) printf x; } while (0)
int iwx_debug = 1;
#else
#define DPRINTF(x)	do { ; } while (0)
#define DPRINTFN(n, x)	do { ; } while (0)
#endif

#include <dev/pci/if_iwxreg.h>
#include <dev/pci/if_iwxvar.h>

const uint8_t iwx_nvm_channels_8000[] = {
	/* 2.4 GHz */
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	/* 5 GHz */
	36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92,
	96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
	149, 153, 157, 161, 165, 169, 173, 177, 181
};

#define IWX_NUM_2GHZ_CHANNELS	14

const struct iwx_rate {
	uint16_t rate;
	uint8_t plcp;
	uint8_t ht_plcp;
} iwx_rates[] = {
		/* Legacy */		/* HT */
	{   2,	IWX_RATE_1M_PLCP,	IWX_RATE_HT_SISO_MCS_INV_PLCP  },
	{   4,	IWX_RATE_2M_PLCP,	IWX_RATE_HT_SISO_MCS_INV_PLCP },
	{  11,	IWX_RATE_5M_PLCP,	IWX_RATE_HT_SISO_MCS_INV_PLCP  },
	{  22,	IWX_RATE_11M_PLCP,	IWX_RATE_HT_SISO_MCS_INV_PLCP },
	{  12,	IWX_RATE_6M_PLCP,	IWX_RATE_HT_SISO_MCS_0_PLCP },
	{  18,	IWX_RATE_9M_PLCP,	IWX_RATE_HT_SISO_MCS_INV_PLCP  },
	{  24,	IWX_RATE_12M_PLCP,	IWX_RATE_HT_SISO_MCS_1_PLCP },
	{  26,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_8_PLCP },
	{  36,	IWX_RATE_18M_PLCP,	IWX_RATE_HT_SISO_MCS_2_PLCP },
	{  48,	IWX_RATE_24M_PLCP,	IWX_RATE_HT_SISO_MCS_3_PLCP },
	{  52,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_9_PLCP },
	{  72,	IWX_RATE_36M_PLCP,	IWX_RATE_HT_SISO_MCS_4_PLCP },
	{  78,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_10_PLCP },
	{  96,	IWX_RATE_48M_PLCP,	IWX_RATE_HT_SISO_MCS_5_PLCP },
	{ 104,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_11_PLCP },
	{ 108,	IWX_RATE_54M_PLCP,	IWX_RATE_HT_SISO_MCS_6_PLCP },
	{ 128,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_SISO_MCS_7_PLCP },
	{ 156,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_12_PLCP },
	{ 208,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_13_PLCP },
	{ 234,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_14_PLCP },
	{ 260,	IWX_RATE_INVM_PLCP,	IWX_RATE_HT_MIMO2_MCS_15_PLCP },
};
#define IWX_RIDX_CCK	0
#define IWX_RIDX_OFDM	4
#define IWX_RIDX_MAX	(nitems(iwx_rates)-1)
#define IWX_RIDX_IS_CCK(_i_) ((_i_) < IWX_RIDX_OFDM)
#define IWX_RIDX_IS_OFDM(_i_) ((_i_) >= IWX_RIDX_OFDM)
#define IWX_RVAL_IS_OFDM(_i_) ((_i_) >= 12 && (_i_) != 22)

/* Convert an MCS index into an iwx_rates[] index. */
const int iwx_mcs2ridx[] = {
	IWX_RATE_MCS_0_INDEX,
	IWX_RATE_MCS_1_INDEX,
	IWX_RATE_MCS_2_INDEX,
	IWX_RATE_MCS_3_INDEX,
	IWX_RATE_MCS_4_INDEX,
	IWX_RATE_MCS_5_INDEX,
	IWX_RATE_MCS_6_INDEX,
	IWX_RATE_MCS_7_INDEX,
	IWX_RATE_MCS_8_INDEX,
	IWX_RATE_MCS_9_INDEX,
	IWX_RATE_MCS_10_INDEX,
	IWX_RATE_MCS_11_INDEX,
	IWX_RATE_MCS_12_INDEX,
	IWX_RATE_MCS_13_INDEX,
	IWX_RATE_MCS_14_INDEX,
	IWX_RATE_MCS_15_INDEX,
};

struct iwx_nvm_section {
	uint16_t length;
	uint8_t *data;
};

uint8_t	iwx_lookup_cmd_ver(struct iwx_softc *, uint8_t, uint8_t);
int	iwx_is_mimo_ht_plcp(uint8_t);
int	iwx_is_mimo_mcs(int);
int	iwx_store_cscheme(struct iwx_softc *, uint8_t *, size_t);
int	iwx_alloc_fw_monitor_block(struct iwx_softc *, uint8_t, uint8_t);
int	iwx_alloc_fw_monitor(struct iwx_softc *, uint8_t);
int	iwx_apply_debug_destination(struct iwx_softc *);
int	iwx_ctxt_info_init(struct iwx_softc *, const struct iwx_fw_sects *);
void	iwx_ctxt_info_free(struct iwx_softc *);
void	iwx_ctxt_info_free_paging(struct iwx_softc *);
int	iwx_init_fw_sec(struct iwx_softc *, const struct iwx_fw_sects *,
	    struct iwx_context_info_dram *);
int	iwx_firmware_store_section(struct iwx_softc *, enum iwx_ucode_type,
	    uint8_t *, size_t);
int	iwx_set_default_calib(struct iwx_softc *, const void *);
void	iwx_fw_info_free(struct iwx_fw_info *);
int	iwx_read_firmware(struct iwx_softc *);
uint32_t iwx_read_prph(struct iwx_softc *, uint32_t);
void	iwx_write_prph(struct iwx_softc *, uint32_t, uint32_t);
int	iwx_read_mem(struct iwx_softc *, uint32_t, void *, int);
int	iwx_write_mem(struct iwx_softc *, uint32_t, const void *, int);
int	iwx_write_mem32(struct iwx_softc *, uint32_t, uint32_t);
int	iwx_poll_bit(struct iwx_softc *, int, uint32_t, uint32_t, int);
int	iwx_nic_lock(struct iwx_softc *);
void	iwx_nic_assert_locked(struct iwx_softc *);
void	iwx_nic_unlock(struct iwx_softc *);
void	iwx_set_bits_mask_prph(struct iwx_softc *, uint32_t, uint32_t,
	    uint32_t);
void	iwx_set_bits_prph(struct iwx_softc *, uint32_t, uint32_t);
void	iwx_clear_bits_prph(struct iwx_softc *, uint32_t, uint32_t);
int	iwx_dma_contig_alloc(bus_dma_tag_t, struct iwx_dma_info *, bus_size_t,
	    bus_size_t);
void	iwx_dma_contig_free(struct iwx_dma_info *);
int	iwx_alloc_rx_ring(struct iwx_softc *, struct iwx_rx_ring *);
void	iwx_disable_rx_dma(struct iwx_softc *);
void	iwx_reset_rx_ring(struct iwx_softc *, struct iwx_rx_ring *);
void	iwx_free_rx_ring(struct iwx_softc *, struct iwx_rx_ring *);
int	iwx_alloc_tx_ring(struct iwx_softc *, struct iwx_tx_ring *, int);
void	iwx_reset_tx_ring(struct iwx_softc *, struct iwx_tx_ring *);
void	iwx_free_tx_ring(struct iwx_softc *, struct iwx_tx_ring *);
void	iwx_enable_rfkill_int(struct iwx_softc *);
int	iwx_check_rfkill(struct iwx_softc *);
void	iwx_enable_interrupts(struct iwx_softc *);
void	iwx_enable_fwload_interrupt(struct iwx_softc *);
void	iwx_restore_interrupts(struct iwx_softc *);
void	iwx_disable_interrupts(struct iwx_softc *);
void	iwx_ict_reset(struct iwx_softc *);
int	iwx_set_hw_ready(struct iwx_softc *);
int	iwx_prepare_card_hw(struct iwx_softc *);
void	iwx_apm_config(struct iwx_softc *);
int	iwx_apm_init(struct iwx_softc *);
void	iwx_apm_stop(struct iwx_softc *);
int	iwx_allow_mcast(struct iwx_softc *);
void	iwx_init_msix_hw(struct iwx_softc *);
void	iwx_conf_msix_hw(struct iwx_softc *, int);
int	iwx_start_hw(struct iwx_softc *);
void	iwx_stop_device(struct iwx_softc *);
void	iwx_nic_config(struct iwx_softc *);
int	iwx_nic_rx_init(struct iwx_softc *);
int	iwx_nic_init(struct iwx_softc *);
int	iwx_enable_txq(struct iwx_softc *, int, int, int, int);
void	iwx_post_alive(struct iwx_softc *);
void	iwx_protect_session(struct iwx_softc *, struct iwx_node *, uint32_t,
	    uint32_t);
void	iwx_unprotect_session(struct iwx_softc *, struct iwx_node *);
int	iwx_nvm_read_chunk(struct iwx_softc *, uint16_t, uint16_t, uint16_t,
	    uint8_t *, uint16_t *);
int	iwx_nvm_read_section(struct iwx_softc *, uint16_t, uint8_t *,
	    uint16_t *, size_t);
void	iwx_init_channel_map(struct iwx_softc *, const uint16_t * const,
	    const uint8_t *nvm_channels, int nchan);
void	iwx_setup_ht_rates(struct iwx_softc *);
int	iwx_mimo_enabled(struct iwx_softc *);
void	iwx_htprot_task(void *);
void	iwx_update_htprot(struct ieee80211com *, struct ieee80211_node *);
int	iwx_ampdu_rx_start(struct ieee80211com *, struct ieee80211_node *,
	    uint8_t);
void	iwx_ampdu_rx_stop(struct ieee80211com *, struct ieee80211_node *,
	    uint8_t);
void	iwx_sta_rx_agg(struct iwx_softc *, struct ieee80211_node *, uint8_t,
	    uint16_t, uint16_t, int);
#ifdef notyet
int	iwx_ampdu_tx_start(struct ieee80211com *, struct ieee80211_node *,
	    uint8_t);
void	iwx_ampdu_tx_stop(struct ieee80211com *, struct ieee80211_node *,
	    uint8_t);
#endif
void	iwx_ba_task(void *);

int	iwx_parse_nvm_data(struct iwx_softc *, const uint16_t *,
	    const uint16_t *, const uint16_t *,
	    const uint16_t *, const uint16_t *,
	    const uint16_t *, int);
void	iwx_set_hw_address_8000(struct iwx_softc *, struct iwx_nvm_data *,
	    const uint16_t *, const uint16_t *);
int	iwx_parse_nvm_sections(struct iwx_softc *, struct iwx_nvm_section *);
int	iwx_nvm_init(struct iwx_softc *);
int	iwx_load_firmware(struct iwx_softc *);
int	iwx_start_fw(struct iwx_softc *);
int	iwx_send_tx_ant_cfg(struct iwx_softc *, uint8_t);
int	iwx_send_phy_cfg_cmd(struct iwx_softc *);
int	iwx_load_ucode_wait_alive(struct iwx_softc *);
int	iwx_send_dqa_cmd(struct iwx_softc *);
int	iwx_run_init_mvm_ucode(struct iwx_softc *, int);
int	iwx_config_ltr(struct iwx_softc *);
void	iwx_update_rx_desc(struct iwx_softc *, struct iwx_rx_ring *, int);
int	iwx_rx_addbuf(struct iwx_softc *, int, int);
int	iwx_rxmq_get_signal_strength(struct iwx_softc *, struct iwx_rx_mpdu_desc *);
void	iwx_rx_rx_phy_cmd(struct iwx_softc *, struct iwx_rx_packet *,
	    struct iwx_rx_data *);
int	iwx_get_noise(const struct iwx_statistics_rx_non_phy *);
void	iwx_rx_frame(struct iwx_softc *, struct mbuf *, int, int, int, uint32_t,
	    struct ieee80211_rxinfo *, struct mbuf_list *);
void	iwx_enable_ht_cck_fallback(struct iwx_softc *, struct iwx_node *);
void	iwx_rx_tx_cmd_single(struct iwx_softc *, struct iwx_rx_packet *,
	    struct iwx_node *, int, int);
void	iwx_rx_tx_cmd(struct iwx_softc *, struct iwx_rx_packet *,
	    struct iwx_rx_data *);
void	iwx_rx_bmiss(struct iwx_softc *, struct iwx_rx_packet *,
	    struct iwx_rx_data *);
int	iwx_binding_cmd(struct iwx_softc *, struct iwx_node *, uint32_t);
int	iwx_phy_ctxt_cmd_uhb(struct iwx_softc *, struct iwx_phy_ctxt *, uint8_t,
	    uint8_t, uint32_t, uint32_t);
int	iwx_phy_ctxt_cmd(struct iwx_softc *, struct iwx_phy_ctxt *, uint8_t,
	    uint8_t, uint32_t, uint32_t);
int	iwx_send_cmd(struct iwx_softc *, struct iwx_host_cmd *);
int	iwx_send_cmd_pdu(struct iwx_softc *, uint32_t, uint32_t, uint16_t,
	    const void *);
int	iwx_send_cmd_status(struct iwx_softc *, struct iwx_host_cmd *,
	    uint32_t *);
int	iwx_send_cmd_pdu_status(struct iwx_softc *, uint32_t, uint16_t,
	    const void *, uint32_t *);
void	iwx_free_resp(struct iwx_softc *, struct iwx_host_cmd *);
void	iwx_cmd_done(struct iwx_softc *, int, int, int);
const struct iwx_rate *iwx_tx_fill_cmd(struct iwx_softc *, struct iwx_node *,
	    struct ieee80211_frame *, struct iwx_tx_cmd_gen2 *);
void	iwx_tx_update_byte_tbl(struct iwx_tx_ring *, uint16_t, uint16_t);
int	iwx_tx(struct iwx_softc *, struct mbuf *, struct ieee80211_node *, int);
int	iwx_flush_tx_path(struct iwx_softc *);
int	iwx_beacon_filter_send_cmd(struct iwx_softc *,
	    struct iwx_beacon_filter_cmd *);
int	iwx_update_beacon_abort(struct iwx_softc *, struct iwx_node *, int);
void	iwx_power_build_cmd(struct iwx_softc *, struct iwx_node *,
	    struct iwx_mac_power_cmd *);
int	iwx_power_mac_update_mode(struct iwx_softc *, struct iwx_node *);
int	iwx_power_update_device(struct iwx_softc *);
int	iwx_enable_beacon_filter(struct iwx_softc *, struct iwx_node *);
int	iwx_disable_beacon_filter(struct iwx_softc *);
int	iwx_add_sta_cmd(struct iwx_softc *, struct iwx_node *, int);
int	iwx_add_aux_sta(struct iwx_softc *);
int	iwx_rm_sta_cmd(struct iwx_softc *, struct iwx_node *);
int	iwx_fill_probe_req(struct iwx_softc *, struct iwx_scan_probe_req *);
int	iwx_config_umac_scan(struct iwx_softc *);
int	iwx_umac_scan(struct iwx_softc *, int);
uint8_t	iwx_ridx2rate(struct ieee80211_rateset *, int);
int	iwx_rval2ridx(int);
void	iwx_ack_rates(struct iwx_softc *, struct iwx_node *, int *, int *);
void	iwx_mac_ctxt_cmd_common(struct iwx_softc *, struct iwx_node *,
	    struct iwx_mac_ctx_cmd *, uint32_t);
void	iwx_mac_ctxt_cmd_fill_sta(struct iwx_softc *, struct iwx_node *,
	    struct iwx_mac_data_sta *, int);
int	iwx_mac_ctxt_cmd(struct iwx_softc *, struct iwx_node *, uint32_t, int);
int	iwx_clear_statistics(struct iwx_softc *);
int	iwx_update_quotas(struct iwx_softc *, struct iwx_node *, int);
void	iwx_add_task(struct iwx_softc *, struct taskq *, struct task *);
void	iwx_del_task(struct iwx_softc *, struct taskq *, struct task *);
int	iwx_scan(struct iwx_softc *);
int	iwx_bgscan(struct ieee80211com *);
int	iwx_umac_scan_abort(struct iwx_softc *);
int	iwx_scan_abort(struct iwx_softc *);
int	iwx_enable_data_tx_queues(struct iwx_softc *);
int	iwx_auth(struct iwx_softc *);
int	iwx_deauth(struct iwx_softc *);
int	iwx_assoc(struct iwx_softc *);
int	iwx_disassoc(struct iwx_softc *);
int	iwx_run(struct iwx_softc *);
int	iwx_run_stop(struct iwx_softc *);
struct ieee80211_node *iwx_node_alloc(struct ieee80211com *);
void	iwx_calib_timeout(void *);
int	iwx_media_change(struct ifnet *);
void	iwx_newstate_task(void *);
int	iwx_newstate(struct ieee80211com *, enum ieee80211_state, int);
void	iwx_endscan(struct iwx_softc *);
void	iwx_fill_sf_command(struct iwx_softc *, struct iwx_sf_cfg_cmd *,
	    struct ieee80211_node *);
int	iwx_sf_config(struct iwx_softc *, int);
int	iwx_send_bt_init_conf(struct iwx_softc *);
int	iwx_send_soc_conf(struct iwx_softc *);
int	iwx_send_update_mcc_cmd(struct iwx_softc *, const char *);
int	iwx_init_hw(struct iwx_softc *);
int	iwx_init(struct ifnet *);
void	iwx_start(struct ifnet *);
void	iwx_stop(struct ifnet *);
void	iwx_watchdog(struct ifnet *);
int	iwx_ioctl(struct ifnet *, u_long, caddr_t);
const char *iwx_desc_lookup(uint32_t);
void	iwx_nic_error(struct iwx_softc *);
void	iwx_nic_umac_error(struct iwx_softc *);
int	iwx_rx_pkt_valid(struct iwx_rx_packet *);
void	iwx_rx_pkt(struct iwx_softc *, struct iwx_rx_data *,
	    struct mbuf_list *);
void	iwx_notif_intr(struct iwx_softc *);
int	iwx_intr(void *);
int	iwx_intr_msix(void *);
int	iwx_match(struct device *, void *, void *);
int	iwx_preinit(struct iwx_softc *);
void	iwx_attach_hook(struct device *);
void	iwx_attach(struct device *, struct device *, void *);
void	iwx_init_task(void *);
int	iwx_activate(struct device *, int);
int	iwx_resume(struct iwx_softc *);

#if NBPFILTER > 0
void	iwx_radiotap_attach(struct iwx_softc *);
#endif

uint8_t
iwx_lookup_cmd_ver(struct iwx_softc *sc, uint8_t grp, uint8_t cmd)
{
	const struct iwx_fw_cmd_version *entry;
	int i;

	for (i = 0; i < sc->n_cmd_versions; i++) {
		entry = &sc->cmd_versions[i];
		if (entry->group == grp && entry->cmd == cmd)
			return entry->cmd_ver;
	}

	return IWX_FW_CMD_VER_UNKNOWN;
}

int
iwx_is_mimo_ht_plcp(uint8_t ht_plcp)
{
	return (ht_plcp != IWX_RATE_HT_SISO_MCS_INV_PLCP &&
	    (ht_plcp & IWX_RATE_HT_MCS_NSS_MSK));
}

int
iwx_is_mimo_mcs(int mcs)
{
	int ridx = iwx_mcs2ridx[mcs];
	return iwx_is_mimo_ht_plcp(iwx_rates[ridx].ht_plcp);
	
}

int
iwx_store_cscheme(struct iwx_softc *sc, uint8_t *data, size_t dlen)
{
	struct iwx_fw_cscheme_list *l = (void *)data;

	if (dlen < sizeof(*l) ||
	    dlen < sizeof(l->size) + l->size * sizeof(*l->cs))
		return EINVAL;

	/* we don't actually store anything for now, always use s/w crypto */

	return 0;
}

int
iwx_ctxt_info_alloc_dma(struct iwx_softc *sc,
    const struct iwx_fw_onesect *sec, struct iwx_dma_info *dram)
{
	int err = iwx_dma_contig_alloc(sc->sc_dmat, dram, sec->fws_len, 0);
	if (err) {
		printf("%s: could not allocate context info DMA memory\n",
		    DEVNAME(sc));
		return err;
	}

	memcpy(dram->vaddr, sec->fws_data, sec->fws_len);

	return 0;
}

void iwx_ctxt_info_free_paging(struct iwx_softc *sc)
{
	struct iwx_self_init_dram *dram = &sc->init_dram;
	int i;

	if (!dram->paging)
		return;

	/* free paging*/
	for (i = 0; i < dram->paging_cnt; i++)
		iwx_dma_contig_free(dram->paging);

	free(dram->paging, M_DEVBUF, dram->paging_cnt * sizeof(*dram->paging));
	dram->paging_cnt = 0;
	dram->paging = NULL;
}

int
iwx_get_num_sections(const struct iwx_fw_sects *fws, int start)
{
	int i = 0;

	while (start < fws->fw_count &&
	       fws->fw_sect[start].fws_devoff != IWX_CPU1_CPU2_SEPARATOR_SECTION &&
	       fws->fw_sect[start].fws_devoff != IWX_PAGING_SEPARATOR_SECTION) {
		start++;
		i++;
	}

	return i;
}

int
iwx_init_fw_sec(struct iwx_softc *sc, const struct iwx_fw_sects *fws,
    struct iwx_context_info_dram *ctxt_dram)
{
	struct iwx_self_init_dram *dram = &sc->init_dram;
	int i, ret, lmac_cnt, umac_cnt, paging_cnt;

	KASSERT(dram->paging == NULL);

	lmac_cnt = iwx_get_num_sections(fws, 0);
	/* add 1 due to separator */
	umac_cnt = iwx_get_num_sections(fws, lmac_cnt + 1);
	/* add 2 due to separators */
	paging_cnt = iwx_get_num_sections(fws, lmac_cnt + umac_cnt + 2);

	dram->fw = mallocarray(umac_cnt + lmac_cnt, sizeof(*dram->fw),
	    M_DEVBUF,  M_ZERO | M_NOWAIT);
	if (!dram->fw)
		return ENOMEM;
	dram->paging = mallocarray(paging_cnt, sizeof(*dram->paging),
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (!dram->paging)
		return ENOMEM;

	/* initialize lmac sections */
	for (i = 0; i < lmac_cnt; i++) {
		ret = iwx_ctxt_info_alloc_dma(sc, &fws->fw_sect[i],
						   &dram->fw[dram->fw_cnt]);
		if (ret)
			return ret;
		ctxt_dram->lmac_img[i] =
			htole64(dram->fw[dram->fw_cnt].paddr);
		DPRINTF(("%s: firmware LMAC section %d at 0x%llx size %lld\n", __func__, i,
		    (unsigned long long)dram->fw[dram->fw_cnt].paddr,
		    (unsigned long long)dram->fw[dram->fw_cnt].size));
		dram->fw_cnt++;
	}

	/* initialize umac sections */
	for (i = 0; i < umac_cnt; i++) {
		/* access FW with +1 to make up for lmac separator */
		ret = iwx_ctxt_info_alloc_dma(sc,
		    &fws->fw_sect[dram->fw_cnt + 1], &dram->fw[dram->fw_cnt]);
		if (ret)
			return ret;
		ctxt_dram->umac_img[i] =
			htole64(dram->fw[dram->fw_cnt].paddr);
		DPRINTF(("%s: firmware UMAC section %d at 0x%llx size %lld\n", __func__, i,
			(unsigned long long)dram->fw[dram->fw_cnt].paddr,
			(unsigned long long)dram->fw[dram->fw_cnt].size));
		dram->fw_cnt++;
	}

	/*
	 * Initialize paging.
	 * Paging memory isn't stored in dram->fw as the umac and lmac - it is
	 * stored separately.
	 * This is since the timing of its release is different -
	 * while fw memory can be released on alive, the paging memory can be
	 * freed only when the device goes down.
	 * Given that, the logic here in accessing the fw image is a bit
	 * different - fw_cnt isn't changing so loop counter is added to it.
	 */
	for (i = 0; i < paging_cnt; i++) {
		/* access FW with +2 to make up for lmac & umac separators */
		int fw_idx = dram->fw_cnt + i + 2;

		ret = iwx_ctxt_info_alloc_dma(sc,
		    &fws->fw_sect[fw_idx], &dram->paging[i]);
		if (ret)
			return ret;

		ctxt_dram->virtual_img[i] = htole64(dram->paging[i].paddr);
		DPRINTF(("%s: firmware paging section %d at 0x%llx size %lld\n", __func__, i,
		    (unsigned long long)dram->paging[i].paddr,
		    (unsigned long long)dram->paging[i].size));
		dram->paging_cnt++;
	}

	return 0;
}

int
iwx_alloc_fw_monitor_block(struct iwx_softc *sc, uint8_t max_power,
    uint8_t min_power)
{
	struct iwx_dma_info *fw_mon = &sc->fw_mon;
	uint32_t size = 0;
	uint8_t power;
	int err;

	if (fw_mon->size)
		return 0;

	for (power = max_power; power >= min_power; power--) {
		size = (1 << power);

		err = iwx_dma_contig_alloc(sc->sc_dmat, fw_mon, size, 0);
		if (err)
			continue;

		DPRINTF(("%s: allocated 0x%08x bytes for firmware monitor.\n",
			 DEVNAME(sc), size));
		break;
	}

	if (err) {
		fw_mon->size = 0;
		return err;
	}

	if (power != max_power)
		DPRINTF(("%s: Sorry - debug buffer is only %luK while you requested %luK\n",
			DEVNAME(sc), (unsigned long)(1 << (power - 10)),
			(unsigned long)(1 << (max_power - 10))));

	return 0;
}

int
iwx_alloc_fw_monitor(struct iwx_softc *sc, uint8_t max_power)
{
	if (!max_power) {
		/* default max_power is maximum */
		max_power = 26;
	} else {
		max_power += 11;
	}

	if (max_power > 26) {
		 DPRINTF(("%s: External buffer size for monitor is too big %d, "
		     "check the FW TLV\n", DEVNAME(sc), max_power));
		return 0;
	}

	if (sc->fw_mon.size)
		return 0;

	return iwx_alloc_fw_monitor_block(sc, max_power, 11);
}

int
iwx_apply_debug_destination(struct iwx_softc *sc)
{
	struct iwx_fw_dbg_dest_tlv_v1 *dest_v1;
	int i, err;
	uint8_t mon_mode, size_power, base_shift, end_shift;
	uint32_t base_reg, end_reg;

	dest_v1 = sc->sc_fw.dbg_dest_tlv_v1;
	mon_mode = dest_v1->monitor_mode;
	size_power = dest_v1->size_power;
	base_reg = le32toh(dest_v1->base_reg);
	end_reg = le32toh(dest_v1->end_reg);
	base_shift = dest_v1->base_shift;
	end_shift = dest_v1->end_shift;

	DPRINTF(("%s: applying debug destination %d\n", DEVNAME(sc), mon_mode));

	if (mon_mode == EXTERNAL_MODE) {
		err = iwx_alloc_fw_monitor(sc, size_power);
		if (err)
			return err;
	}

	if (!iwx_nic_lock(sc))
		return EBUSY;

	for (i = 0; i < sc->sc_fw.n_dest_reg; i++) {
		uint32_t addr, val;
		uint8_t op;

		addr = le32toh(dest_v1->reg_ops[i].addr);
		val = le32toh(dest_v1->reg_ops[i].val);
		op = dest_v1->reg_ops[i].op;

		DPRINTF(("%s: op=%u addr=%u val=%u\n", __func__, op, addr, val));
		switch (op) {
		case CSR_ASSIGN:
			IWX_WRITE(sc, addr, val);
			break;
		case CSR_SETBIT:
			IWX_SETBITS(sc, addr, (1 << val));
			break;
		case CSR_CLEARBIT:
			IWX_CLRBITS(sc, addr, (1 << val));
			break;
		case PRPH_ASSIGN:
			iwx_write_prph(sc, addr, val);
			break;
		case PRPH_SETBIT:
			iwx_set_bits_prph(sc, addr, (1 << val));
			break;
		case PRPH_CLEARBIT:
			iwx_clear_bits_prph(sc, addr, (1 << val));
			break;
		case PRPH_BLOCKBIT:
			if (iwx_read_prph(sc, addr) & (1 << val))
				goto monitor;
			break;
		default:
			DPRINTF(("%s: FW debug - unknown OP %d\n",
			    DEVNAME(sc), op));
			break;
		}
	}

monitor:
	if (mon_mode == EXTERNAL_MODE && sc->fw_mon.size) {
		iwx_write_prph(sc, le32toh(base_reg),
		    sc->fw_mon.paddr >> base_shift);
		iwx_write_prph(sc, end_reg,
		    (sc->fw_mon.paddr + sc->fw_mon.size - 256)
		    >> end_shift);
	}

	iwx_nic_unlock(sc);
	return 0;
}

int
iwx_ctxt_info_init(struct iwx_softc *sc, const struct iwx_fw_sects *fws)
{
	struct iwx_context_info *ctxt_info;
	struct iwx_context_info_rbd_cfg *rx_cfg;
	uint32_t control_flags = 0, rb_size;
	int err;

	err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->ctxt_info_dma,
	    sizeof(*ctxt_info), 0);
	if (err) {
		printf("%s: could not allocate context info DMA memory\n",
		    DEVNAME(sc));
		return err;
	}
	ctxt_info = sc->ctxt_info_dma.vaddr;

	ctxt_info->version.version = 0;
	ctxt_info->version.mac_id =
		htole16((uint16_t)IWX_READ(sc, IWX_CSR_HW_REV));
	/* size is in DWs */
	ctxt_info->version.size = htole16(sizeof(*ctxt_info) / 4);

	if (sc->sc_device_family >= IWX_DEVICE_FAMILY_22560)
		rb_size = IWX_CTXT_INFO_RB_SIZE_2K;
	else
		rb_size = IWX_CTXT_INFO_RB_SIZE_4K;

	KASSERT(IWX_RX_QUEUE_CB_SIZE(IWX_MQ_RX_TABLE_SIZE) < 0xF);
	control_flags = IWX_CTXT_INFO_TFD_FORMAT_LONG |
			(IWX_RX_QUEUE_CB_SIZE(IWX_MQ_RX_TABLE_SIZE) <<
			 IWX_CTXT_INFO_RB_CB_SIZE_POS) |
			(rb_size << IWX_CTXT_INFO_RB_SIZE_POS);
	ctxt_info->control.control_flags = htole32(control_flags);

	/* initialize RX default queue */
	rx_cfg = &ctxt_info->rbd_cfg;
	rx_cfg->free_rbd_addr = htole64(sc->rxq.free_desc_dma.paddr);
	rx_cfg->used_rbd_addr = htole64(sc->rxq.used_desc_dma.paddr);
	rx_cfg->status_wr_ptr = htole64(sc->rxq.stat_dma.paddr);

	/* initialize TX command queue */
	ctxt_info->hcmd_cfg.cmd_queue_addr =
		htole64(sc->txq[IWX_DQA_CMD_QUEUE].desc_dma.paddr);
	ctxt_info->hcmd_cfg.cmd_queue_size =
		IWX_TFD_QUEUE_CB_SIZE(IWX_CMD_QUEUE_SIZE);

	/* allocate ucode sections in dram and set addresses */
	err = iwx_init_fw_sec(sc, fws, &ctxt_info->dram);
	if (err) {
		iwx_ctxt_info_free(sc);
		return err;
	}

	/* Configure debug, if exists */
	if (sc->sc_fw.dbg_dest_tlv_v1) {
		err = iwx_apply_debug_destination(sc);
		if (err)
			return err;
	}

	/* kick FW self load */
	IWX_WRITE_8(sc, IWX_CSR_CTXT_INFO_BA, sc->ctxt_info_dma.paddr);
	if (!iwx_nic_lock(sc))
		return EBUSY;
	iwx_write_prph(sc, IWX_UREG_CPU_INIT_RUN, 1);
	iwx_nic_unlock(sc);

	/* Context info will be released upon alive or failure to get one */

	return 0;
}

void
iwx_ctxt_info_free_fw_img(struct iwx_softc *sc)
{
	struct iwx_self_init_dram *dram = &sc->init_dram;
	int i;

	if (!dram->fw) {
		KASSERT(dram->fw_cnt == 0);
		return;
	}

	for (i = 0; i < dram->fw_cnt; i++)
		iwx_dma_contig_free(&dram->fw[i]);

	free(dram->fw, M_DEVBUF, dram->fw_cnt * sizeof(dram->fw[0]));
	dram->fw_cnt = 0;
	dram->fw = NULL;
}

void
iwx_ctxt_info_free(struct iwx_softc *sc)
{
	iwx_dma_contig_free(&sc->ctxt_info_dma);
	iwx_ctxt_info_free_fw_img(sc);
}

int
iwx_firmware_store_section(struct iwx_softc *sc, enum iwx_ucode_type type,
    uint8_t *data, size_t dlen)
{
	struct iwx_fw_sects *fws;
	struct iwx_fw_onesect *fwone;

	if (type >= IWX_UCODE_TYPE_MAX)
		return EINVAL;
	if (dlen < sizeof(uint32_t))
		return EINVAL;

	fws = &sc->sc_fw.fw_sects[type];
	DPRINTF(("%s: ucode type %d section %d\n", DEVNAME(sc), type, fws->fw_count));
	if (fws->fw_count >= IWX_UCODE_SECT_MAX)
		return EINVAL;

	fwone = &fws->fw_sect[fws->fw_count];

	/* first 32bit are device load offset */
	memcpy(&fwone->fws_devoff, data, sizeof(uint32_t));

	/* rest is data */
	fwone->fws_data = data + sizeof(uint32_t);
	fwone->fws_len = dlen - sizeof(uint32_t);

	fws->fw_count++;
	fws->fw_totlen += fwone->fws_len;

	return 0;
}

#define IWX_DEFAULT_SCAN_CHANNELS	40
/* Newer firmware might support more channels. Raise this value if needed. */
#define IWX_MAX_SCAN_CHANNELS		52 /* as of 8265-34 firmware image */

struct iwx_tlv_calib_data {
	uint32_t ucode_type;
	struct iwx_tlv_calib_ctrl calib;
} __packed;

int
iwx_set_default_calib(struct iwx_softc *sc, const void *data)
{
	const struct iwx_tlv_calib_data *def_calib = data;
	uint32_t ucode_type = le32toh(def_calib->ucode_type);

	if (ucode_type >= IWX_UCODE_TYPE_MAX)
		return EINVAL;

	sc->sc_default_calib[ucode_type].flow_trigger =
	    def_calib->calib.flow_trigger;
	sc->sc_default_calib[ucode_type].event_trigger =
	    def_calib->calib.event_trigger;

	return 0;
}

void
iwx_fw_info_free(struct iwx_fw_info *fw)
{
	free(fw->fw_rawdata, M_DEVBUF, fw->fw_rawsize);
	fw->fw_rawdata = NULL;
	fw->fw_rawsize = 0;
	/* don't touch fw->fw_status */
	memset(fw->fw_sects, 0, sizeof(fw->fw_sects));
}

#define IWX_FW_ADDR_CACHE_CONTROL 0xC0000000

int
iwx_read_firmware(struct iwx_softc *sc)
{
	struct iwx_fw_info *fw = &sc->sc_fw;
	struct iwx_tlv_ucode_header *uhdr;
	struct iwx_ucode_tlv tlv;
	uint32_t tlv_type;
	uint8_t *data;
	int err;
	size_t len;

	if (fw->fw_status == IWX_FW_STATUS_DONE)
		return 0;

	while (fw->fw_status == IWX_FW_STATUS_INPROGRESS)
		tsleep_nsec(&sc->sc_fw, 0, "iwxfwp", INFSLP);
	fw->fw_status = IWX_FW_STATUS_INPROGRESS;

	if (fw->fw_rawdata != NULL)
		iwx_fw_info_free(fw);

	err = loadfirmware(sc->sc_fwname,
	    (u_char **)&fw->fw_rawdata, &fw->fw_rawsize);
	if (err) {
		printf("%s: could not read firmware %s (error %d)\n",
		    DEVNAME(sc), sc->sc_fwname, err);
		goto out;
	}

	sc->sc_capaflags = 0;
	sc->sc_capa_n_scan_channels = IWX_DEFAULT_SCAN_CHANNELS;
	memset(sc->sc_enabled_capa, 0, sizeof(sc->sc_enabled_capa));

	uhdr = (void *)fw->fw_rawdata;
	if (*(uint32_t *)fw->fw_rawdata != 0
	    || le32toh(uhdr->magic) != IWX_TLV_UCODE_MAGIC) {
		printf("%s: invalid firmware %s\n",
		    DEVNAME(sc), sc->sc_fwname);
		err = EINVAL;
		goto out;
	}

	snprintf(sc->sc_fwver, sizeof(sc->sc_fwver), "%d.%d (API ver %d)",
	    IWX_UCODE_MAJOR(le32toh(uhdr->ver)),
	    IWX_UCODE_MINOR(le32toh(uhdr->ver)),
	    IWX_UCODE_API(le32toh(uhdr->ver)));
	data = uhdr->data;
	len = fw->fw_rawsize - sizeof(*uhdr);

	while (len >= sizeof(tlv)) {
		size_t tlv_len;
		void *tlv_data;

		memcpy(&tlv, data, sizeof(tlv));
		tlv_len = le32toh(tlv.length);
		tlv_type = le32toh(tlv.type);

		len -= sizeof(tlv);
		data += sizeof(tlv);
		tlv_data = data;

		if (len < tlv_len) {
			printf("%s: firmware too short: %zu bytes\n",
			    DEVNAME(sc), len);
			err = EINVAL;
			goto parse_out;
		}

		switch (tlv_type) {
		case IWX_UCODE_TLV_PROBE_MAX_LEN:
			if (tlv_len < sizeof(uint32_t)) {
				err = EINVAL;
				goto parse_out;
			}
			sc->sc_capa_max_probe_len
			    = le32toh(*(uint32_t *)tlv_data);
			if (sc->sc_capa_max_probe_len >
			    IWX_SCAN_OFFLOAD_PROBE_REQ_SIZE) {
				err = EINVAL;
				goto parse_out;
			}
			break;
		case IWX_UCODE_TLV_PAN:
			if (tlv_len) {
				err = EINVAL;
				goto parse_out;
			}
			sc->sc_capaflags |= IWX_UCODE_TLV_FLAGS_PAN;
			break;
		case IWX_UCODE_TLV_FLAGS:
			if (tlv_len < sizeof(uint32_t)) {
				err = EINVAL;
				goto parse_out;
			}
			/*
			 * Apparently there can be many flags, but Linux driver
			 * parses only the first one, and so do we.
			 *
			 * XXX: why does this override IWX_UCODE_TLV_PAN?
			 * Intentional or a bug?  Observations from
			 * current firmware file:
			 *  1) TLV_PAN is parsed first
			 *  2) TLV_FLAGS contains TLV_FLAGS_PAN
			 * ==> this resets TLV_PAN to itself... hnnnk
			 */
			sc->sc_capaflags = le32toh(*(uint32_t *)tlv_data);
			break;
		case IWX_UCODE_TLV_CSCHEME:
			err = iwx_store_cscheme(sc, tlv_data, tlv_len);
			if (err)
				goto parse_out;
			break;
		case IWX_UCODE_TLV_NUM_OF_CPU: {
			uint32_t num_cpu;
			if (tlv_len != sizeof(uint32_t)) {
				err = EINVAL;
				goto parse_out;
			}
			num_cpu = le32toh(*(uint32_t *)tlv_data);
			if (num_cpu < 1 || num_cpu > 2) {
				err = EINVAL;
				goto parse_out;
			}
			break;
		}
		case IWX_UCODE_TLV_SEC_RT:
			err = iwx_firmware_store_section(sc,
			    IWX_UCODE_TYPE_REGULAR, tlv_data, tlv_len);
			if (err)
				goto parse_out;
			break;
		case IWX_UCODE_TLV_SEC_INIT:
			err = iwx_firmware_store_section(sc,
			    IWX_UCODE_TYPE_INIT, tlv_data, tlv_len);
			if (err)
				goto parse_out;
			break;
		case IWX_UCODE_TLV_SEC_WOWLAN:
			err = iwx_firmware_store_section(sc,
			    IWX_UCODE_TYPE_WOW, tlv_data, tlv_len);
			if (err)
				goto parse_out;
			break;
		case IWX_UCODE_TLV_DEF_CALIB:
			if (tlv_len != sizeof(struct iwx_tlv_calib_data)) {
				err = EINVAL;
				goto parse_out;
			}
			err = iwx_set_default_calib(sc, tlv_data);
			if (err)
				goto parse_out;
			break;
		case IWX_UCODE_TLV_PHY_SKU:
			if (tlv_len != sizeof(uint32_t)) {
				err = EINVAL;
				goto parse_out;
			}
			sc->sc_fw_phy_config = le32toh(*(uint32_t *)tlv_data);
			break;

		case IWX_UCODE_TLV_API_CHANGES_SET: {
			struct iwx_ucode_api *api;
			int idx, i;
			if (tlv_len != sizeof(*api)) {
				err = EINVAL;
				goto parse_out;
			}
			api = (struct iwx_ucode_api *)tlv_data;
			idx = le32toh(api->api_index);
			if (idx >= howmany(IWX_NUM_UCODE_TLV_API, 32)) {
				err = EINVAL;
				goto parse_out;
			}
			for (i = 0; i < 32; i++) {
				if ((le32toh(api->api_flags) & (1 << i)) == 0)
					continue;
				setbit(sc->sc_ucode_api, i + (32 * idx));
			}
			break;
		}

		case IWX_UCODE_TLV_ENABLED_CAPABILITIES: {
			struct iwx_ucode_capa *capa;
			int idx, i;
			if (tlv_len != sizeof(*capa)) {
				err = EINVAL;
				goto parse_out;
			}
			capa = (struct iwx_ucode_capa *)tlv_data;
			idx = le32toh(capa->api_index);
			if (idx >= howmany(IWX_NUM_UCODE_TLV_CAPA, 32)) {
				goto parse_out;
			}
			for (i = 0; i < 32; i++) {
				if ((le32toh(capa->api_capa) & (1 << i)) == 0)
					continue;
				setbit(sc->sc_enabled_capa, i + (32 * idx));
			}
			break;
		}

		case IWX_UCODE_TLV_SDIO_ADMA_ADDR:
		case IWX_UCODE_TLV_FW_GSCAN_CAPA:
			/* ignore, not used by current driver */
			break;

		case IWX_UCODE_TLV_SEC_RT_USNIFFER:
			err = iwx_firmware_store_section(sc,
			    IWX_UCODE_TYPE_REGULAR_USNIFFER, tlv_data,
			    tlv_len);
			if (err)
				goto parse_out;
			break;

		case IWX_UCODE_TLV_PAGING:
			if (tlv_len != sizeof(uint32_t)) {
				err = EINVAL;
				goto parse_out;
			}
			break;

		case IWX_UCODE_TLV_N_SCAN_CHANNELS:
			if (tlv_len != sizeof(uint32_t)) {
				err = EINVAL;
				goto parse_out;
			}
			sc->sc_capa_n_scan_channels =
			  le32toh(*(uint32_t *)tlv_data);
			if (sc->sc_capa_n_scan_channels > IWX_MAX_SCAN_CHANNELS) {
				err = ERANGE;
				goto parse_out;
			}
			break;

		case IWX_UCODE_TLV_FW_VERSION:
			if (tlv_len != sizeof(uint32_t) * 3) {
				err = EINVAL;
				goto parse_out;
			}
			snprintf(sc->sc_fwver, sizeof(sc->sc_fwver),
			    "%u.%u.%u",
			    le32toh(((uint32_t *)tlv_data)[0]),
			    le32toh(((uint32_t *)tlv_data)[1]),
			    le32toh(((uint32_t *)tlv_data)[2]));
			break;

		case IWX_UCODE_TLV_FW_DBG_DEST: {
			struct iwx_fw_dbg_dest_tlv_v1 *dest_v1 = NULL;

			fw->dbg_dest_ver = (uint8_t *)tlv_data;
			if (*fw->dbg_dest_ver != 0) {
				err = EINVAL;
				goto parse_out;
			}

			if (fw->dbg_dest_tlv_init)
				break;
			fw->dbg_dest_tlv_init = true;

			dest_v1 = (void *)tlv_data;
			fw->dbg_dest_tlv_v1 = dest_v1;
			fw->n_dest_reg = tlv_len -
			    offsetof(struct iwx_fw_dbg_dest_tlv_v1, reg_ops);
			fw->n_dest_reg /= sizeof(dest_v1->reg_ops[0]);
			DPRINTF(("%s: found debug dest; n_dest_reg=%d\n", __func__, fw->n_dest_reg));
			break;
		}

		case IWX_UCODE_TLV_FW_DBG_CONF: {
			struct iwx_fw_dbg_conf_tlv *conf = (void *)tlv_data;

			if (!fw->dbg_dest_tlv_init ||
			    conf->id >= nitems(fw->dbg_conf_tlv) ||
			    fw->dbg_conf_tlv[conf->id] != NULL)
				break;

			DPRINTF(("Found debug configuration: %d\n", conf->id));
			fw->dbg_conf_tlv[conf->id] = conf;
			fw->dbg_conf_tlv_len[conf->id] = tlv_len;
			break;
		}

		case IWX_UCODE_TLV_UMAC_DEBUG_ADDRS: {
			struct iwx_umac_debug_addrs *dbg_ptrs =
				(void *)tlv_data;

			if (tlv_len != sizeof(*dbg_ptrs)) {
				err = EINVAL;
				goto parse_out;
			}
			if (sc->sc_device_family < IWX_DEVICE_FAMILY_22000)
				break;
			sc->sc_uc.uc_umac_error_event_table =
				le32toh(dbg_ptrs->error_info_addr) &
				~IWX_FW_ADDR_CACHE_CONTROL;
			sc->sc_uc.error_event_table_tlv_status |=
				IWX_ERROR_EVENT_TABLE_UMAC;
			break;
		}

		case IWX_UCODE_TLV_LMAC_DEBUG_ADDRS: {
			struct iwx_lmac_debug_addrs *dbg_ptrs =
				(void *)tlv_data;

			if (tlv_len != sizeof(*dbg_ptrs)) {
				err = EINVAL;
				goto parse_out;
			}
			if (sc->sc_device_family < IWX_DEVICE_FAMILY_22000)
				break;
			sc->sc_uc.uc_lmac_error_event_table[0] =
				le32toh(dbg_ptrs->error_event_table_ptr) &
				~IWX_FW_ADDR_CACHE_CONTROL;
			sc->sc_uc.error_event_table_tlv_status |=
				IWX_ERROR_EVENT_TABLE_LMAC1;
			break;
		}

		case IWX_UCODE_TLV_FW_MEM_SEG:
			break;

		case IWX_UCODE_TLV_CMD_VERSIONS:
			if (tlv_len % sizeof(struct iwx_fw_cmd_version)) {
				tlv_len /= sizeof(struct iwx_fw_cmd_version);
				tlv_len *= sizeof(struct iwx_fw_cmd_version);
			}
			if (sc->n_cmd_versions != 0) {
				err = EINVAL;
				goto parse_out;
			}
			if (tlv_len > sizeof(sc->cmd_versions)) {
				err = EINVAL;
				goto parse_out;
			}
			memcpy(&sc->cmd_versions[0], tlv_data, tlv_len);
			sc->n_cmd_versions = tlv_len / sizeof(struct iwx_fw_cmd_version);
			break;

		case IWX_UCODE_TLV_FW_RECOVERY_INFO:
			break;

		case IWX_UCODE_TLV_FW_FSEQ_VERSION:
			break;

		/* undocumented TLVs found in iwx-cc-a0-46 image */
		case 58:
		case 0x1000003:
		case 0x1000004:
			break;

		/* undocumented TLVs found in iwx-cc-a0-48 image */
		case 0x1000000:
		case 0x1000002:
			break;

		default:
			err = EINVAL;
			goto parse_out;
		}

		len -= roundup(tlv_len, 4);
		data += roundup(tlv_len, 4);
	}

	KASSERT(err == 0);

 parse_out:
	if (err) {
		printf("%s: firmware parse error %d, "
		    "section type %d\n", DEVNAME(sc), err, tlv_type);
	}

 out:
	if (err) {
		fw->fw_status = IWX_FW_STATUS_NONE;
		if (fw->fw_rawdata != NULL)
			iwx_fw_info_free(fw);
	} else
		fw->fw_status = IWX_FW_STATUS_DONE;
	wakeup(&sc->sc_fw);

	return err;
}

uint32_t
iwx_read_prph(struct iwx_softc *sc, uint32_t addr)
{
	iwx_nic_assert_locked(sc);
	IWX_WRITE(sc,
	    IWX_HBUS_TARG_PRPH_RADDR, ((addr & 0x000fffff) | (3 << 24)));
	IWX_BARRIER_READ_WRITE(sc);
	return IWX_READ(sc, IWX_HBUS_TARG_PRPH_RDAT);
}

void
iwx_write_prph(struct iwx_softc *sc, uint32_t addr, uint32_t val)
{
	iwx_nic_assert_locked(sc);
	IWX_WRITE(sc,
	    IWX_HBUS_TARG_PRPH_WADDR, ((addr & 0x000fffff) | (3 << 24)));
	IWX_BARRIER_WRITE(sc);
	IWX_WRITE(sc, IWX_HBUS_TARG_PRPH_WDAT, val);
}

void
iwx_write_prph64(struct iwx_softc *sc, uint64_t addr, uint64_t val)
{
	iwx_write_prph(sc, (uint32_t)addr, val & 0xffffffff);
	iwx_write_prph(sc, (uint32_t)addr + 4, val >> 32);
}

int
iwx_read_mem(struct iwx_softc *sc, uint32_t addr, void *buf, int dwords)
{
	int offs, err = 0;
	uint32_t *vals = buf;

	if (iwx_nic_lock(sc)) {
		IWX_WRITE(sc, IWX_HBUS_TARG_MEM_RADDR, addr);
		for (offs = 0; offs < dwords; offs++)
			vals[offs] = le32toh(IWX_READ(sc, IWX_HBUS_TARG_MEM_RDAT));
		iwx_nic_unlock(sc);
	} else {
		err = EBUSY;
	}
	return err;
}

int
iwx_write_mem(struct iwx_softc *sc, uint32_t addr, const void *buf, int dwords)
{
	int offs;	
	const uint32_t *vals = buf;

	if (iwx_nic_lock(sc)) {
		IWX_WRITE(sc, IWX_HBUS_TARG_MEM_WADDR, addr);
		/* WADDR auto-increments */
		for (offs = 0; offs < dwords; offs++) {
			uint32_t val = vals ? vals[offs] : 0;
			IWX_WRITE(sc, IWX_HBUS_TARG_MEM_WDAT, val);
		}
		iwx_nic_unlock(sc);
	} else {
		return EBUSY;
	}
	return 0;
}

int
iwx_write_mem32(struct iwx_softc *sc, uint32_t addr, uint32_t val)
{
	return iwx_write_mem(sc, addr, &val, 1);
}

int
iwx_poll_bit(struct iwx_softc *sc, int reg, uint32_t bits, uint32_t mask,
    int timo)
{
	for (;;) {
		if ((IWX_READ(sc, reg) & mask) == (bits & mask)) {
			return 1;
		}
		if (timo < 10) {
			return 0;
		}
		timo -= 10;
		DELAY(10);
	}
}

int
iwx_nic_lock(struct iwx_softc *sc)
{
	if (sc->sc_nic_locks > 0) {
		iwx_nic_assert_locked(sc);
		sc->sc_nic_locks++;
		return 1; /* already locked */
	}

	IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
	    IWX_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	DELAY(2);

	if (iwx_poll_bit(sc, IWX_CSR_GP_CNTRL,
	    IWX_CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN,
	    IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY
	     | IWX_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP, 150000)) {
		sc->sc_nic_locks++;
		return 1;
	}

	printf("%s: acquiring device failed\n", DEVNAME(sc));
	return 0;
}

void
iwx_nic_assert_locked(struct iwx_softc *sc)
{
	uint32_t reg = IWX_READ(sc, IWX_CSR_GP_CNTRL);
	if ((reg & IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY) == 0)
		panic("%s: mac clock not ready", DEVNAME(sc));
	if (reg & IWX_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP)
		panic("%s: mac gone to sleep", DEVNAME(sc));
	if (sc->sc_nic_locks <= 0)
		panic("%s: nic locks counter %d", DEVNAME(sc), sc->sc_nic_locks);
}

void
iwx_nic_unlock(struct iwx_softc *sc)
{
	if (sc->sc_nic_locks > 0) {
		if (--sc->sc_nic_locks == 0)
			IWX_CLRBITS(sc, IWX_CSR_GP_CNTRL,
			    IWX_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	} else
		printf("%s: NIC already unlocked\n", DEVNAME(sc));
}

void
iwx_set_bits_mask_prph(struct iwx_softc *sc, uint32_t reg, uint32_t bits,
    uint32_t mask)
{
	uint32_t val;

	/* XXX: no error path? */
	if (iwx_nic_lock(sc)) {
		val = iwx_read_prph(sc, reg) & mask;
		val |= bits;
		iwx_write_prph(sc, reg, val);
		iwx_nic_unlock(sc);
	}
}

void
iwx_set_bits_prph(struct iwx_softc *sc, uint32_t reg, uint32_t bits)
{
	iwx_set_bits_mask_prph(sc, reg, bits, ~0);
}

void
iwx_clear_bits_prph(struct iwx_softc *sc, uint32_t reg, uint32_t bits)
{
	iwx_set_bits_mask_prph(sc, reg, 0, ~bits);
}

int
iwx_dma_contig_alloc(bus_dma_tag_t tag, struct iwx_dma_info *dma,
    bus_size_t size, bus_size_t alignment)
{
	int nsegs, err;
	caddr_t va;

	dma->tag = tag;
	dma->size = size;

	err = bus_dmamap_create(tag, size, 1, size, 0, BUS_DMA_NOWAIT,
	    &dma->map);
	if (err)
		goto fail;

	err = bus_dmamem_alloc(tag, size, alignment, 0, &dma->seg, 1, &nsegs,
	    BUS_DMA_NOWAIT);
	if (err)
		goto fail;

	err = bus_dmamem_map(tag, &dma->seg, 1, size, &va,
	    BUS_DMA_NOWAIT);
	if (err)
		goto fail;
	dma->vaddr = va;

	err = bus_dmamap_load(tag, dma->map, dma->vaddr, size, NULL,
	    BUS_DMA_NOWAIT);
	if (err)
		goto fail;

	memset(dma->vaddr, 0, size);
	bus_dmamap_sync(tag, dma->map, 0, size, BUS_DMASYNC_PREWRITE);
	dma->paddr = dma->map->dm_segs[0].ds_addr;

	return 0;

fail:	iwx_dma_contig_free(dma);
	return err;
}

void
iwx_dma_contig_free(struct iwx_dma_info *dma)
{
	if (dma->map != NULL) {
		if (dma->vaddr != NULL) {
			bus_dmamap_sync(dma->tag, dma->map, 0, dma->size,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(dma->tag, dma->map);
			bus_dmamem_unmap(dma->tag, dma->vaddr, dma->size);
			bus_dmamem_free(dma->tag, &dma->seg, 1);
			dma->vaddr = NULL;
		}
		bus_dmamap_destroy(dma->tag, dma->map);
		dma->map = NULL;
	}
}

int
iwx_alloc_rx_ring(struct iwx_softc *sc, struct iwx_rx_ring *ring)
{
	bus_size_t size;
	int i, err;

	ring->cur = 0;

	/* Allocate RX descriptors (256-byte aligned). */
	size = IWX_RX_MQ_RING_COUNT * sizeof(uint64_t);
	err = iwx_dma_contig_alloc(sc->sc_dmat, &ring->free_desc_dma, size, 256);
	if (err) {
		printf("%s: could not allocate RX ring DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}
	ring->desc = ring->free_desc_dma.vaddr;

	/* Allocate RX status area (16-byte aligned). */
	err = iwx_dma_contig_alloc(sc->sc_dmat, &ring->stat_dma,
	    sizeof(*ring->stat), 16);
	if (err) {
		printf("%s: could not allocate RX status DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}
	ring->stat = ring->stat_dma.vaddr;

	size = IWX_RX_MQ_RING_COUNT * sizeof(uint32_t);
	err = iwx_dma_contig_alloc(sc->sc_dmat, &ring->used_desc_dma,
	    size, 256);
	if (err) {
		printf("%s: could not allocate RX ring DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}

	for (i = 0; i < IWX_RX_MQ_RING_COUNT; i++) {
		struct iwx_rx_data *data = &ring->data[i];

		memset(data, 0, sizeof(*data));
		err = bus_dmamap_create(sc->sc_dmat, IWX_RBUF_SIZE, 1,
		    IWX_RBUF_SIZE, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &data->map);
		if (err) {
			printf("%s: could not create RX buf DMA map\n",
			    DEVNAME(sc));
			goto fail;
		}

		err = iwx_rx_addbuf(sc, IWX_RBUF_SIZE, i);
		if (err)
			goto fail;
	}
	return 0;

fail:	iwx_free_rx_ring(sc, ring);
	return err;
}

void
iwx_disable_rx_dma(struct iwx_softc *sc)
{
	int ntries;

	if (iwx_nic_lock(sc)) {
		iwx_write_prph(sc, IWX_RFH_RXF_DMA_CFG, 0);
		for (ntries = 0; ntries < 1000; ntries++) {
			if (iwx_read_prph(sc, IWX_RFH_GEN_STATUS) &
			    IWX_RXF_DMA_IDLE)
				break;
			DELAY(10);
		}
		iwx_nic_unlock(sc);
	}
}

void
iwx_reset_rx_ring(struct iwx_softc *sc, struct iwx_rx_ring *ring)
{
	ring->cur = 0;
	bus_dmamap_sync(sc->sc_dmat, ring->stat_dma.map, 0,
	    ring->stat_dma.size, BUS_DMASYNC_PREWRITE);
	memset(ring->stat, 0, sizeof(*ring->stat));
	bus_dmamap_sync(sc->sc_dmat, ring->stat_dma.map, 0,
	    ring->stat_dma.size, BUS_DMASYNC_POSTWRITE);

}

void
iwx_free_rx_ring(struct iwx_softc *sc, struct iwx_rx_ring *ring)
{
	int i;

	iwx_dma_contig_free(&ring->free_desc_dma);
	iwx_dma_contig_free(&ring->stat_dma);
	iwx_dma_contig_free(&ring->used_desc_dma);

	for (i = 0; i < IWX_RX_MQ_RING_COUNT; i++) {
		struct iwx_rx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
		if (data->map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

int
iwx_alloc_tx_ring(struct iwx_softc *sc, struct iwx_tx_ring *ring, int qid)
{
	bus_addr_t paddr;
	bus_size_t size;
	int i, err, qlen;

	ring->qid = qid;
	ring->queued = 0;
	ring->cur = 0;
	ring->tail = 0;

	if (qid == IWX_DQA_CMD_QUEUE)
		qlen = IWX_CMD_QUEUE_SIZE;
	else
		qlen = IWX_TX_RING_COUNT;

	/* Allocate TX descriptors (256-byte aligned). */
	size = qlen * sizeof (struct iwx_tfh_tfd);
	err = iwx_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma, size, 256);
	if (err) {
		printf("%s: could not allocate TX ring DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}
	ring->desc = ring->desc_dma.vaddr;

	/*
	 * There is no need to allocate DMA buffers for unused rings.
	 * The hardware supports up to 31 Tx rings which is more
	 * than we currently need.
	 *
	 * In DQA mode we use 1 command queue + 4 DQA mgmt/data queues.
	 * The command is queue 0 (sc->txq[0]), and 4 mgmt/data frame queues
	 * are sc->tqx[IWX_DQA_MIN_MGMT_QUEUE + ac], i.e. sc->txq[5:8],
	 * in order to provide one queue per EDCA category.
	 *
	 * Tx aggregation will require additional queues (one queue per TID
	 * for which aggregation is enabled) but we do not implement this yet.
	 */
	if (qid > IWX_DQA_MAX_MGMT_QUEUE)
		return 0;

	err = iwx_dma_contig_alloc(sc->sc_dmat, &ring->bc_tbl,
	    sizeof(struct iwx_agn_scd_bc_tbl), 0);
	if (err) {
		printf("%s: could not allocate byte count table DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}

	size = qlen * sizeof(struct iwx_device_cmd);
	err = iwx_dma_contig_alloc(sc->sc_dmat, &ring->cmd_dma, size,
	    IWX_FIRST_TB_SIZE_ALIGN);
	if (err) {
		printf("%s: could not allocate cmd DMA memory\n", DEVNAME(sc));
		goto fail;
	}
	ring->cmd = ring->cmd_dma.vaddr;

	paddr = ring->cmd_dma.paddr;
	for (i = 0; i < qlen; i++) {
		struct iwx_tx_data *data = &ring->data[i];
		size_t mapsize;

		data->cmd_paddr = paddr;
		paddr += sizeof(struct iwx_device_cmd);

		/* FW commands may require more mapped space than packets. */
		if (qid == IWX_DQA_CMD_QUEUE)
			mapsize = (sizeof(struct iwx_cmd_header) +
			    IWX_MAX_CMD_PAYLOAD_SIZE);
		else
			mapsize = MCLBYTES;
		err = bus_dmamap_create(sc->sc_dmat, mapsize,
		    IWX_TFH_NUM_TBS - 2, mapsize, 0, BUS_DMA_NOWAIT,
		    &data->map);
		if (err) {
			printf("%s: could not create TX buf DMA map\n",
			    DEVNAME(sc));
			goto fail;
		}
	}
	KASSERT(paddr == ring->cmd_dma.paddr + size);
	return 0;

fail:	iwx_free_tx_ring(sc, ring);
	return err;
}

void
iwx_reset_tx_ring(struct iwx_softc *sc, struct iwx_tx_ring *ring)
{
	int i, qlen;

	if (ring->qid == IWX_DQA_CMD_QUEUE)
		qlen = IWX_CMD_QUEUE_SIZE;
	else
		qlen = IWX_TX_RING_COUNT;

	for (i = 0; i < qlen; i++) {
		struct iwx_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}
	/* Clear TX descriptors. */
	memset(ring->desc, 0, ring->desc_dma.size);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0,
	    ring->desc_dma.size, BUS_DMASYNC_PREWRITE);
	sc->qfullmsk &= ~(1 << ring->qid);
	ring->queued = 0;
	ring->cur = 0;
	ring->tail = 0;
}

void
iwx_free_tx_ring(struct iwx_softc *sc, struct iwx_tx_ring *ring)
{
	int i, qlen;

	iwx_dma_contig_free(&ring->desc_dma);
	iwx_dma_contig_free(&ring->cmd_dma);
	iwx_dma_contig_free(&ring->bc_tbl);

	if (ring->qid == IWX_DQA_CMD_QUEUE)
		qlen = IWX_CMD_QUEUE_SIZE;
	else
		qlen = IWX_TX_RING_COUNT;

	for (i = 0; i < qlen; i++) {
		struct iwx_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
		if (data->map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

void
iwx_enable_rfkill_int(struct iwx_softc *sc)
{
	if (!sc->sc_msix) {
		sc->sc_intmask = IWX_CSR_INT_BIT_RF_KILL;
		IWX_WRITE(sc, IWX_CSR_INT_MASK, sc->sc_intmask);
	} else {
		IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
		    sc->sc_fh_init_mask);
		IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_MASK_AD,
		    ~IWX_MSIX_HW_INT_CAUSES_REG_RF_KILL);
		sc->sc_hw_mask = IWX_MSIX_HW_INT_CAUSES_REG_RF_KILL;
	}

	IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
	    IWX_CSR_GP_CNTRL_REG_FLAG_RFKILL_WAKE_L1A_EN);
}

int
iwx_check_rfkill(struct iwx_softc *sc)
{
	uint32_t v;
	int s;
	int rv;

	s = splnet();

	/*
	 * "documentation" is not really helpful here:
	 *  27:	HW_RF_KILL_SW
	 *	Indicates state of (platform's) hardware RF-Kill switch
	 *
	 * But apparently when it's off, it's on ...
	 */
	v = IWX_READ(sc, IWX_CSR_GP_CNTRL);
	rv = (v & IWX_CSR_GP_CNTRL_REG_FLAG_HW_RF_KILL_SW) == 0;
	if (rv) {
		sc->sc_flags |= IWX_FLAG_RFKILL;
	} else {
		sc->sc_flags &= ~IWX_FLAG_RFKILL;
	}

	splx(s);
	return rv;
}

void
iwx_enable_interrupts(struct iwx_softc *sc)
{
	if (!sc->sc_msix) {
		sc->sc_intmask = IWX_CSR_INI_SET_MASK;
		IWX_WRITE(sc, IWX_CSR_INT_MASK, sc->sc_intmask);
	} else {
		/*
		 * fh/hw_mask keeps all the unmasked causes.
		 * Unlike msi, in msix cause is enabled when it is unset.
		 */
		sc->sc_hw_mask = sc->sc_hw_init_mask;
		sc->sc_fh_mask = sc->sc_fh_init_mask;
		IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
		    ~sc->sc_fh_mask);
		IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_MASK_AD,
		    ~sc->sc_hw_mask);
	}
}

void
iwx_enable_fwload_interrupt(struct iwx_softc *sc)
{
	if (!sc->sc_msix) {
		sc->sc_intmask = IWX_CSR_INT_BIT_ALIVE | IWX_CSR_INT_BIT_FH_RX;
		IWX_WRITE(sc, IWX_CSR_INT_MASK, sc->sc_intmask);
	} else {
		IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_MASK_AD,
		    ~IWX_MSIX_HW_INT_CAUSES_REG_ALIVE);
		sc->sc_hw_mask = IWX_MSIX_HW_INT_CAUSES_REG_ALIVE;
		/*
		 * Leave all the FH causes enabled to get the ALIVE
		 * notification.
		 */
		IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
		    ~sc->sc_fh_init_mask);
		sc->sc_fh_mask = sc->sc_fh_init_mask;
	}
}

void
iwx_restore_interrupts(struct iwx_softc *sc)
{
	IWX_WRITE(sc, IWX_CSR_INT_MASK, sc->sc_intmask);
}

void
iwx_disable_interrupts(struct iwx_softc *sc)
{
	int s = splnet();

	if (!sc->sc_msix) {
		IWX_WRITE(sc, IWX_CSR_INT_MASK, 0);

		/* acknowledge all interrupts */
		IWX_WRITE(sc, IWX_CSR_INT, ~0);
		IWX_WRITE(sc, IWX_CSR_FH_INT_STATUS, ~0);
	} else {
		IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
		    sc->sc_fh_init_mask);
		IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_MASK_AD,
		    sc->sc_hw_init_mask);
	}

	splx(s);
}

void
iwx_ict_reset(struct iwx_softc *sc)
{
	iwx_disable_interrupts(sc);

	memset(sc->ict_dma.vaddr, 0, IWX_ICT_SIZE);
	sc->ict_cur = 0;

	/* Set physical address of ICT (4KB aligned). */
	IWX_WRITE(sc, IWX_CSR_DRAM_INT_TBL_REG,
	    IWX_CSR_DRAM_INT_TBL_ENABLE
	    | IWX_CSR_DRAM_INIT_TBL_WRAP_CHECK
	    | IWX_CSR_DRAM_INIT_TBL_WRITE_POINTER
	    | sc->ict_dma.paddr >> IWX_ICT_PADDR_SHIFT);

	/* Switch to ICT interrupt mode in driver. */
	sc->sc_flags |= IWX_FLAG_USE_ICT;

	IWX_WRITE(sc, IWX_CSR_INT, ~0);
	iwx_enable_interrupts(sc);
}

#define IWX_HW_READY_TIMEOUT 50
int
iwx_set_hw_ready(struct iwx_softc *sc)
{
	int ready;

	IWX_SETBITS(sc, IWX_CSR_HW_IF_CONFIG_REG,
	    IWX_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY);

	ready = iwx_poll_bit(sc, IWX_CSR_HW_IF_CONFIG_REG,
	    IWX_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY,
	    IWX_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY,
	    IWX_HW_READY_TIMEOUT);
	if (ready)
		IWX_SETBITS(sc, IWX_CSR_MBOX_SET_REG,
		    IWX_CSR_MBOX_SET_REG_OS_ALIVE);

	return ready;
}
#undef IWX_HW_READY_TIMEOUT

int
iwx_prepare_card_hw(struct iwx_softc *sc)
{
	int t = 0;

	if (iwx_set_hw_ready(sc))
		return 0;

	IWX_SETBITS(sc, IWX_CSR_DBG_LINK_PWR_MGMT_REG,
	    IWX_CSR_RESET_LINK_PWR_MGMT_DISABLED);
	DELAY(1000);
 

	/* If HW is not ready, prepare the conditions to check again */
	IWX_SETBITS(sc, IWX_CSR_HW_IF_CONFIG_REG,
	    IWX_CSR_HW_IF_CONFIG_REG_PREPARE);

	do {
		if (iwx_set_hw_ready(sc))
			return 0;
		DELAY(200);
		t += 200;
	} while (t < 150000);

	return ETIMEDOUT;
}

void
iwx_apm_config(struct iwx_softc *sc)
{
	pcireg_t lctl, cap;

	/*
	 * HW bug W/A for instability in PCIe bus L0S->L1 transition.
	 * Check if BIOS (or OS) enabled L1-ASPM on this device.
	 * If so (likely), disable L0S, so device moves directly L0->L1;
	 *    costs negligible amount of power savings.
	 * If not (unlikely), enable L0S, so there is at least some
	 *    power savings, even without L1.
	 */
	lctl = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
	    sc->sc_cap_off + PCI_PCIE_LCSR);
	if (lctl & PCI_PCIE_LCSR_ASPM_L1) {
		IWX_SETBITS(sc, IWX_CSR_GIO_REG,
		    IWX_CSR_GIO_REG_VAL_L0S_ENABLED);
	} else {
		IWX_CLRBITS(sc, IWX_CSR_GIO_REG,
		    IWX_CSR_GIO_REG_VAL_L0S_ENABLED);
	}

	cap = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
	    sc->sc_cap_off + PCI_PCIE_DCSR2);
	sc->sc_ltr_enabled = (cap & PCI_PCIE_DCSR2_LTREN) ? 1 : 0;
	DPRINTF(("%s: L1 %sabled - LTR %sabled\n",
	    DEVNAME(sc),
	    (lctl & PCI_PCIE_LCSR_ASPM_L1) ? "En" : "Dis",
	    sc->sc_ltr_enabled ? "En" : "Dis"));
}

/*
 * Start up NIC's basic functionality after it has been reset
 * e.g. after platform boot or shutdown.
 * NOTE:  This does not load uCode nor start the embedded processor
 */
int
iwx_apm_init(struct iwx_softc *sc)
{
	int err = 0;

	/*
	 * Disable L0s without affecting L1;
	 *  don't wait for ICH L0s (ICH bug W/A)
	 */
	IWX_SETBITS(sc, IWX_CSR_GIO_CHICKEN_BITS,
	    IWX_CSR_GIO_CHICKEN_BITS_REG_BIT_L1A_NO_L0S_RX);

	/* Set FH wait threshold to maximum (HW error during stress W/A) */
	IWX_SETBITS(sc, IWX_CSR_DBG_HPET_MEM_REG, IWX_CSR_DBG_HPET_MEM_REG_VAL);

	/*
	 * Enable HAP INTA (interrupt from management bus) to
	 * wake device's PCI Express link L1a -> L0s
	 */
	IWX_SETBITS(sc, IWX_CSR_HW_IF_CONFIG_REG,
	    IWX_CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A);

	iwx_apm_config(sc);

	/*
	 * Set "initialization complete" bit to move adapter from
	 * D0U* --> D0A* (powered-up active) state.
	 */
	IWX_SETBITS(sc, IWX_CSR_GP_CNTRL, IWX_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);

	/*
	 * Wait for clock stabilization; once stabilized, access to
	 * device-internal resources is supported, e.g. iwx_write_prph()
	 * and accesses to uCode SRAM.
	 */
	if (!iwx_poll_bit(sc, IWX_CSR_GP_CNTRL,
	    IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
	    IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY, 25000)) {
		printf("%s: timeout waiting for clock stabilization\n",
		    DEVNAME(sc));
		err = ETIMEDOUT;
		goto out;
	}
 out:
	if (err)
		printf("%s: apm init error %d\n", DEVNAME(sc), err);
	return err;
}

void
iwx_apm_stop(struct iwx_softc *sc)
{
	IWX_SETBITS(sc, IWX_CSR_DBG_LINK_PWR_MGMT_REG,
	    IWX_CSR_RESET_LINK_PWR_MGMT_DISABLED);
	IWX_SETBITS(sc, IWX_CSR_HW_IF_CONFIG_REG,
	    IWX_CSR_HW_IF_CONFIG_REG_PREPARE |
	    IWX_CSR_HW_IF_CONFIG_REG_ENABLE_PME);
	DELAY(1000);
	IWX_CLRBITS(sc, IWX_CSR_DBG_LINK_PWR_MGMT_REG,
	    IWX_CSR_RESET_LINK_PWR_MGMT_DISABLED);
	DELAY(5000);

	/* stop device's busmaster DMA activity */
	IWX_SETBITS(sc, IWX_CSR_RESET, IWX_CSR_RESET_REG_FLAG_STOP_MASTER);

	if (!iwx_poll_bit(sc, IWX_CSR_RESET,
	    IWX_CSR_RESET_REG_FLAG_MASTER_DISABLED,
	    IWX_CSR_RESET_REG_FLAG_MASTER_DISABLED, 100))
		printf("%s: timeout waiting for master\n", DEVNAME(sc));

	/*
	 * Clear "initialization complete" bit to move adapter from
	 * D0A* (powered-up Active) --> D0U* (Uninitialized) state.
	 */
	IWX_CLRBITS(sc, IWX_CSR_GP_CNTRL,
	    IWX_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);
}

void
iwx_init_msix_hw(struct iwx_softc *sc)
{
	iwx_conf_msix_hw(sc, 0);

	if (!sc->sc_msix)
		return;

	sc->sc_fh_init_mask = ~IWX_READ(sc, IWX_CSR_MSIX_FH_INT_MASK_AD);
	sc->sc_fh_mask = sc->sc_fh_init_mask;
	sc->sc_hw_init_mask = ~IWX_READ(sc, IWX_CSR_MSIX_HW_INT_MASK_AD);
	sc->sc_hw_mask = sc->sc_hw_init_mask;
}

void
iwx_conf_msix_hw(struct iwx_softc *sc, int stopped)
{
	int vector = 0;

	if (!sc->sc_msix) {
		/* Newer chips default to MSIX. */
		if (!stopped && iwx_nic_lock(sc)) {
			iwx_write_prph(sc, IWX_UREG_CHICK,
			    IWX_UREG_CHICK_MSI_ENABLE);
			iwx_nic_unlock(sc);
		}
		return;
	}

	if (!stopped && iwx_nic_lock(sc)) {
		iwx_write_prph(sc, IWX_UREG_CHICK, IWX_UREG_CHICK_MSIX_ENABLE);
		iwx_nic_unlock(sc);
	}

	/* Disable all interrupts */
	IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_MASK_AD, ~0);
	IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_MASK_AD, ~0);

	/* Map fallback-queue (command/mgmt) to a single vector */
	IWX_WRITE_1(sc, IWX_CSR_MSIX_RX_IVAR(0),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	/* Map RSS queue (data) to the same vector */
	IWX_WRITE_1(sc, IWX_CSR_MSIX_RX_IVAR(1),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);

	/* Enable the RX queues cause interrupts */
	IWX_CLRBITS(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
	    IWX_MSIX_FH_INT_CAUSES_Q0 | IWX_MSIX_FH_INT_CAUSES_Q1);

	/* Map non-RX causes to the same vector */
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_D2S_CH0_NUM),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_D2S_CH1_NUM),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_S2D),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_FH_ERR),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_ALIVE),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_WAKEUP),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_IML),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_CT_KILL),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_RF_KILL),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_PERIODIC),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_SW_ERR),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_SCD),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_FH_TX),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_HW_ERR),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
	IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_HAP),
	    vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);

	/* Enable non-RX causes interrupts */
	IWX_CLRBITS(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
	    IWX_MSIX_FH_INT_CAUSES_D2S_CH0_NUM |
	    IWX_MSIX_FH_INT_CAUSES_D2S_CH1_NUM |
	    IWX_MSIX_FH_INT_CAUSES_S2D |
	    IWX_MSIX_FH_INT_CAUSES_FH_ERR);
	IWX_CLRBITS(sc, IWX_CSR_MSIX_HW_INT_MASK_AD,
	    IWX_MSIX_HW_INT_CAUSES_REG_ALIVE |
	    IWX_MSIX_HW_INT_CAUSES_REG_WAKEUP |
	    IWX_MSIX_HW_INT_CAUSES_REG_IML |
	    IWX_MSIX_HW_INT_CAUSES_REG_CT_KILL |
	    IWX_MSIX_HW_INT_CAUSES_REG_RF_KILL |
	    IWX_MSIX_HW_INT_CAUSES_REG_PERIODIC |
	    IWX_MSIX_HW_INT_CAUSES_REG_SW_ERR |
	    IWX_MSIX_HW_INT_CAUSES_REG_SCD |
	    IWX_MSIX_HW_INT_CAUSES_REG_FH_TX |
	    IWX_MSIX_HW_INT_CAUSES_REG_HW_ERR |
	    IWX_MSIX_HW_INT_CAUSES_REG_HAP);
}

int
iwx_start_hw(struct iwx_softc *sc)
{
	int err;

	err = iwx_prepare_card_hw(sc);
	if (err)
		return err;

	/* Reset the entire device */
	IWX_SETBITS(sc, IWX_CSR_RESET, IWX_CSR_RESET_REG_FLAG_SW_RESET);
	DELAY(5000);

	err = iwx_apm_init(sc);
	if (err)
		return err;

	iwx_init_msix_hw(sc);

	iwx_enable_rfkill_int(sc);
	iwx_check_rfkill(sc);

	return 0;
}


void
iwx_stop_device(struct iwx_softc *sc)
{
	int qid;

	iwx_disable_interrupts(sc);
	sc->sc_flags &= ~IWX_FLAG_USE_ICT;

	iwx_disable_rx_dma(sc);
	iwx_reset_rx_ring(sc, &sc->rxq);
	for (qid = 0; qid < nitems(sc->txq); qid++)
		iwx_reset_tx_ring(sc, &sc->txq[qid]);

	/* Make sure (redundant) we've released our request to stay awake */
	IWX_CLRBITS(sc, IWX_CSR_GP_CNTRL,
	    IWX_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	if (sc->sc_nic_locks > 0)
		printf("%s: %d active NIC locks forcefully cleared\n",
		    DEVNAME(sc), sc->sc_nic_locks);
	sc->sc_nic_locks = 0;

	/* Stop the device, and put it in low power state */
	iwx_apm_stop(sc);

	/* Reset the on-board processor. */
	IWX_SETBITS(sc, IWX_CSR_RESET, IWX_CSR_RESET_REG_FLAG_SW_RESET);
	DELAY(5000);

	/*
	 * Upon stop, the IVAR table gets erased, so msi-x won't
	 * work. This causes a bug in RF-KILL flows, since the interrupt
	 * that enables radio won't fire on the correct irq, and the
	 * driver won't be able to handle the interrupt.
	 * Configure the IVAR table again after reset.
	 */
	iwx_conf_msix_hw(sc, 1);

	/* 
	 * Upon stop, the APM issues an interrupt if HW RF kill is set.
	 * Clear the interrupt again.
	 */
	iwx_disable_interrupts(sc);

	/* Even though we stop the HW we still want the RF kill interrupt. */
	iwx_enable_rfkill_int(sc);
	iwx_check_rfkill(sc);

	iwx_prepare_card_hw(sc);

	iwx_ctxt_info_free_paging(sc);
}

void
iwx_nic_config(struct iwx_softc *sc)
{
	uint8_t radio_cfg_type, radio_cfg_step, radio_cfg_dash;
	uint32_t mask, val, reg_val = 0;

	radio_cfg_type = (sc->sc_fw_phy_config & IWX_FW_PHY_CFG_RADIO_TYPE) >>
	    IWX_FW_PHY_CFG_RADIO_TYPE_POS;
	radio_cfg_step = (sc->sc_fw_phy_config & IWX_FW_PHY_CFG_RADIO_STEP) >>
	    IWX_FW_PHY_CFG_RADIO_STEP_POS;
	radio_cfg_dash = (sc->sc_fw_phy_config & IWX_FW_PHY_CFG_RADIO_DASH) >>
	    IWX_FW_PHY_CFG_RADIO_DASH_POS;

	reg_val |= IWX_CSR_HW_REV_STEP(sc->sc_hw_rev) <<
	    IWX_CSR_HW_IF_CONFIG_REG_POS_MAC_STEP;
	reg_val |= IWX_CSR_HW_REV_DASH(sc->sc_hw_rev) <<
	    IWX_CSR_HW_IF_CONFIG_REG_POS_MAC_DASH;

	/* radio configuration */
	reg_val |= radio_cfg_type << IWX_CSR_HW_IF_CONFIG_REG_POS_PHY_TYPE;
	reg_val |= radio_cfg_step << IWX_CSR_HW_IF_CONFIG_REG_POS_PHY_STEP;
	reg_val |= radio_cfg_dash << IWX_CSR_HW_IF_CONFIG_REG_POS_PHY_DASH;

	mask = IWX_CSR_HW_IF_CONFIG_REG_MSK_MAC_DASH |
	    IWX_CSR_HW_IF_CONFIG_REG_MSK_MAC_STEP |
	    IWX_CSR_HW_IF_CONFIG_REG_MSK_PHY_STEP |
	    IWX_CSR_HW_IF_CONFIG_REG_MSK_PHY_DASH |
	    IWX_CSR_HW_IF_CONFIG_REG_MSK_PHY_TYPE |
	    IWX_CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI |
	    IWX_CSR_HW_IF_CONFIG_REG_BIT_MAC_SI;

	val = IWX_READ(sc, IWX_CSR_HW_IF_CONFIG_REG);
	val &= ~mask;
	val |= reg_val;
	IWX_WRITE(sc, IWX_CSR_HW_IF_CONFIG_REG, val);
}

int
iwx_nic_rx_init(struct iwx_softc *sc)
{
	IWX_WRITE_1(sc, IWX_CSR_INT_COALESCING, IWX_HOST_INT_TIMEOUT_DEF);

	/*
	 * We don't configure the RFH; the firmware will do that.
	 * Rx descriptors are set when firmware sends an ALIVE interrupt.
	 */
	return 0;
}

int
iwx_nic_init(struct iwx_softc *sc)
{
	int err;

	iwx_apm_init(sc);
	iwx_nic_config(sc);

	err = iwx_nic_rx_init(sc);
	if (err)
		return err;

	IWX_SETBITS(sc, IWX_CSR_MAC_SHADOW_REG_CTRL, 0x800fffff);

	return 0;
}

/* Map ieee80211_edca_ac categories to firmware Tx FIFO. */
const uint8_t iwx_ac_to_tx_fifo[] = {
	IWX_TX_FIFO_BE,
	IWX_TX_FIFO_BK,
	IWX_TX_FIFO_VI,
	IWX_TX_FIFO_VO,
};

int
iwx_enable_txq(struct iwx_softc *sc, int sta_id, int qid, int tid,
    int num_slots)
{
	struct iwx_tx_queue_cfg_cmd cmd;
	struct iwx_rx_packet *pkt;
	struct iwx_tx_queue_cfg_rsp *resp;
	struct iwx_host_cmd hcmd = {
		.id = IWX_SCD_QUEUE_CFG,
		.flags = IWX_CMD_WANT_RESP,
		.resp_pkt_len = sizeof(*pkt) + sizeof(*resp),
	};
	struct iwx_tx_ring *ring = &sc->txq[qid];
	int err, fwqid;
	uint32_t wr_idx;
	size_t resp_len;

	iwx_reset_tx_ring(sc, ring);

	memset(&cmd, 0, sizeof(cmd));
	cmd.sta_id = sta_id;
	cmd.tid = tid;
	cmd.flags = htole16(IWX_TX_QUEUE_CFG_ENABLE_QUEUE);
	cmd.cb_size = htole32(IWX_TFD_QUEUE_CB_SIZE(num_slots));
	cmd.byte_cnt_addr = htole64(ring->bc_tbl.paddr);
	cmd.tfdq_addr = htole64(ring->desc_dma.paddr);

	hcmd.data[0] = &cmd;
	hcmd.len[0] = sizeof(cmd);

	err = iwx_send_cmd(sc, &hcmd);
	if (err)
		return err;

	pkt = hcmd.resp_pkt;
	if (!pkt || (pkt->hdr.flags & IWX_CMD_FAILED_MSK)) {
		DPRINTF(("SCD_QUEUE_CFG command failed\n"));
		err = EIO;
		goto out;
	}

	resp_len = iwx_rx_packet_payload_len(pkt);
	if (resp_len != sizeof(*resp)) {
		DPRINTF(("SCD_QUEUE_CFG returned %zu bytes, expected %zu bytes\n", resp_len, sizeof(*resp)));
		err = EIO;
		goto out;
	}

	resp = (void *)pkt->data;
	fwqid = le16toh(resp->queue_number);
	wr_idx = le16toh(resp->write_pointer);

	/* Unlike iwlwifi, we do not support dynamic queue ID assignment. */
	if (fwqid != qid) {
		DPRINTF(("requested qid %d but %d was assigned\n", qid, fwqid));
		err = EIO;
		goto out;
	}

	if (wr_idx != ring->cur) {
		DPRINTF(("fw write index is %d but ring is %d\n", wr_idx, ring->cur));
		err = EIO;
		goto out;
	}
out:
	iwx_free_resp(sc, &hcmd);
	return err;
}

void
iwx_post_alive(struct iwx_softc *sc)
{
	iwx_ict_reset(sc);
	iwx_ctxt_info_free(sc);
}

/*
 * For the high priority TE use a time event type that has similar priority to
 * the FW's action scan priority.
 */
#define IWX_ROC_TE_TYPE_NORMAL IWX_TE_P2P_DEVICE_DISCOVERABLE
#define IWX_ROC_TE_TYPE_MGMT_TX IWX_TE_P2P_CLIENT_ASSOC

int
iwx_send_time_event_cmd(struct iwx_softc *sc,
    const struct iwx_time_event_cmd *cmd)
{
	struct iwx_rx_packet *pkt;
	struct iwx_time_event_resp *resp;
	struct iwx_host_cmd hcmd = {
		.id = IWX_TIME_EVENT_CMD,
		.flags = IWX_CMD_WANT_RESP,
		.resp_pkt_len = sizeof(*pkt) + sizeof(*resp),
	};
	uint32_t resp_len;
	int err;

	hcmd.data[0] = cmd;
	hcmd.len[0] = sizeof(*cmd);
	err = iwx_send_cmd(sc, &hcmd);
	if (err)
		return err;

	pkt = hcmd.resp_pkt;
	if (!pkt || (pkt->hdr.flags & IWX_CMD_FAILED_MSK)) {
		err = EIO;
		goto out;
	}

	resp_len = iwx_rx_packet_payload_len(pkt);
	if (resp_len != sizeof(*resp)) {
		err = EIO;
		goto out;
	}

	resp = (void *)pkt->data;
	if (le32toh(resp->status) == 0)
		sc->sc_time_event_uid = le32toh(resp->unique_id);
	else
		err = EIO;
out:
	iwx_free_resp(sc, &hcmd);
	return err;
}

void
iwx_protect_session(struct iwx_softc *sc, struct iwx_node *in,
    uint32_t duration, uint32_t max_delay)
{
	struct iwx_time_event_cmd time_cmd;

	/* Do nothing if a time event is already scheduled. */
	if (sc->sc_flags & IWX_FLAG_TE_ACTIVE)
		return;

	memset(&time_cmd, 0, sizeof(time_cmd));

	time_cmd.action = htole32(IWX_FW_CTXT_ACTION_ADD);
	time_cmd.id_and_color =
	    htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	time_cmd.id = htole32(IWX_TE_BSS_STA_AGGRESSIVE_ASSOC);

	time_cmd.apply_time = htole32(0);

	time_cmd.max_frags = IWX_TE_V2_FRAG_NONE;
	time_cmd.max_delay = htole32(max_delay);
	/* TODO: why do we need to interval = bi if it is not periodic? */
	time_cmd.interval = htole32(1);
	time_cmd.duration = htole32(duration);
	time_cmd.repeat = 1;
	time_cmd.policy
	    = htole16(IWX_TE_V2_NOTIF_HOST_EVENT_START |
	        IWX_TE_V2_NOTIF_HOST_EVENT_END |
		IWX_T2_V2_START_IMMEDIATELY);

	if (iwx_send_time_event_cmd(sc, &time_cmd) == 0)
		sc->sc_flags |= IWX_FLAG_TE_ACTIVE;

	DELAY(100);
}

void
iwx_unprotect_session(struct iwx_softc *sc, struct iwx_node *in)
{
	struct iwx_time_event_cmd time_cmd;

	/* Do nothing if the time event has already ended. */
	if ((sc->sc_flags & IWX_FLAG_TE_ACTIVE) == 0)
		return;

	memset(&time_cmd, 0, sizeof(time_cmd));

	time_cmd.action = htole32(IWX_FW_CTXT_ACTION_REMOVE);
	time_cmd.id_and_color =
	    htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	time_cmd.id = htole32(sc->sc_time_event_uid);

	if (iwx_send_time_event_cmd(sc, &time_cmd) == 0)
		sc->sc_flags &= ~IWX_FLAG_TE_ACTIVE;

	DELAY(100);
}

/*
 * NVM read access and content parsing.  We do not support
 * external NVM or writing NVM.
 */

/* list of NVM sections we are allowed/need to read */
const int iwx_nvm_to_read[] = {
	IWX_NVM_SECTION_TYPE_SW,
	IWX_NVM_SECTION_TYPE_REGULATORY,
	IWX_NVM_SECTION_TYPE_CALIBRATION,
	IWX_NVM_SECTION_TYPE_PRODUCTION,
	IWX_NVM_SECTION_TYPE_REGULATORY_SDP,
	IWX_NVM_SECTION_TYPE_HW_8000,
	IWX_NVM_SECTION_TYPE_MAC_OVERRIDE,
	IWX_NVM_SECTION_TYPE_PHY_SKU,
};

#define IWX_NVM_DEFAULT_CHUNK_SIZE	(2*1024)

#define IWX_NVM_WRITE_OPCODE 1
#define IWX_NVM_READ_OPCODE 0

int
iwx_nvm_read_chunk(struct iwx_softc *sc, uint16_t section, uint16_t offset,
    uint16_t length, uint8_t *data, uint16_t *len)
{
	offset = 0;
	struct iwx_nvm_access_cmd nvm_access_cmd = {
		.offset = htole16(offset),
		.length = htole16(length),
		.type = htole16(section),
		.op_code = IWX_NVM_READ_OPCODE,
	};
	struct iwx_nvm_access_resp *nvm_resp;
	struct iwx_rx_packet *pkt;
	struct iwx_host_cmd cmd = {
		.id = IWX_NVM_ACCESS_CMD,
		.flags = (IWX_CMD_WANT_RESP | IWX_CMD_SEND_IN_RFKILL),
		.resp_pkt_len = IWX_CMD_RESP_MAX,
		.data = { &nvm_access_cmd, },
	};
	int err, offset_read;
	size_t bytes_read;
	uint8_t *resp_data;

	cmd.len[0] = sizeof(struct iwx_nvm_access_cmd);

	err = iwx_send_cmd(sc, &cmd);
	if (err)
		return err;

	pkt = cmd.resp_pkt;
	if (pkt->hdr.flags & IWX_CMD_FAILED_MSK) {
		err = EIO;
		goto exit;
	}

	/* Extract NVM response */
	nvm_resp = (void *)pkt->data;
	if (nvm_resp == NULL)
		return EIO;

	err = le16toh(nvm_resp->status);
	bytes_read = le16toh(nvm_resp->length);
	offset_read = le16toh(nvm_resp->offset);
	resp_data = nvm_resp->data;
	if (err) {
		err = EINVAL;
		goto exit;
	}

	if (offset_read != offset) {
		err = EINVAL;
		goto exit;
	}

	if (bytes_read > length) {
		err = EINVAL;
		goto exit;
	}

	memcpy(data + offset, resp_data, bytes_read);
	*len = bytes_read;

 exit:
	iwx_free_resp(sc, &cmd);
	return err;
}

/*
 * Reads an NVM section completely.
 * NICs prior to 7000 family doesn't have a real NVM, but just read
 * section 0 which is the EEPROM. Because the EEPROM reading is unlimited
 * by uCode, we need to manually check in this case that we don't
 * overflow and try to read more than the EEPROM size.
 */
int
iwx_nvm_read_section(struct iwx_softc *sc, uint16_t section, uint8_t *data,
    uint16_t *len, size_t max_len)
{
	uint16_t chunklen, seglen;
	int err = 0;

	chunklen = seglen = IWX_NVM_DEFAULT_CHUNK_SIZE;
	*len = 0;

	/* Read NVM chunks until exhausted (reading less than requested) */
	while (seglen == chunklen && *len < max_len) {
		err = iwx_nvm_read_chunk(sc,
		    section, *len, chunklen, data, &seglen);
		if (err)
			return err;

		*len += seglen;
	}

	return err;
}

uint8_t
iwx_fw_valid_tx_ant(struct iwx_softc *sc)
{
	uint8_t tx_ant;

	tx_ant = ((sc->sc_fw_phy_config & IWX_FW_PHY_CFG_TX_CHAIN)
	    >> IWX_FW_PHY_CFG_TX_CHAIN_POS);

	if (sc->sc_nvm.valid_tx_ant)
		tx_ant &= sc->sc_nvm.valid_tx_ant;

	return tx_ant;
}

uint8_t
iwx_fw_valid_rx_ant(struct iwx_softc *sc)
{
	uint8_t rx_ant;

	rx_ant = ((sc->sc_fw_phy_config & IWX_FW_PHY_CFG_RX_CHAIN)
	    >> IWX_FW_PHY_CFG_RX_CHAIN_POS);

	if (sc->sc_nvm.valid_rx_ant)
		rx_ant &= sc->sc_nvm.valid_rx_ant;

	return rx_ant;
}

void
iwx_init_channel_map(struct iwx_softc *sc, const uint16_t * const nvm_ch_flags,
    const uint8_t *nvm_channels, int nchan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_nvm_data *data = &sc->sc_nvm;
	int ch_idx;
	struct ieee80211_channel *channel;
	uint16_t ch_flags;
	int is_5ghz;
	int flags, hw_value;

	for (ch_idx = 0; ch_idx < nchan; ch_idx++) {
		ch_flags = le16_to_cpup(nvm_ch_flags + ch_idx);

		if (ch_idx >= IWX_NUM_2GHZ_CHANNELS &&
		    !data->sku_cap_band_52GHz_enable)
			ch_flags &= ~IWX_NVM_CHANNEL_VALID;

		if (!(ch_flags & IWX_NVM_CHANNEL_VALID))
			continue;

		hw_value = nvm_channels[ch_idx];
		channel = &ic->ic_channels[hw_value];

		is_5ghz = ch_idx >= IWX_NUM_2GHZ_CHANNELS;
		if (!is_5ghz) {
			flags = IEEE80211_CHAN_2GHZ;
			channel->ic_flags
			    = IEEE80211_CHAN_CCK
			    | IEEE80211_CHAN_OFDM
			    | IEEE80211_CHAN_DYN
			    | IEEE80211_CHAN_2GHZ;
		} else {
			flags = IEEE80211_CHAN_5GHZ;
			channel->ic_flags =
			    IEEE80211_CHAN_A;
		}
		channel->ic_freq = ieee80211_ieee2mhz(hw_value, flags);

		if (!(ch_flags & IWX_NVM_CHANNEL_ACTIVE))
			channel->ic_flags |= IEEE80211_CHAN_PASSIVE;

		if (data->sku_cap_11n_enable)
			channel->ic_flags |= IEEE80211_CHAN_HT;
	}
}

int
iwx_mimo_enabled(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	return !sc->sc_nvm.sku_cap_mimo_disable &&
	    (ic->ic_userflags & IEEE80211_F_NOMIMO) == 0;
}

void
iwx_setup_ht_rates(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t rx_ant;

	/* TX is supported with the same MCS as RX. */
	ic->ic_tx_mcs_set = IEEE80211_TX_MCS_SET_DEFINED;

	memset(ic->ic_sup_mcs, 0, sizeof(ic->ic_sup_mcs));
	ic->ic_sup_mcs[0] = 0xff;		/* MCS 0-7 */

	if (!iwx_mimo_enabled(sc))
		return;

	rx_ant = iwx_fw_valid_rx_ant(sc);
	if ((rx_ant & IWX_ANT_AB) == IWX_ANT_AB ||
	    (rx_ant & IWX_ANT_BC) == IWX_ANT_BC)
		ic->ic_sup_mcs[1] = 0xff;	/* MCS 8-15 */
}

#define IWX_MAX_RX_BA_SESSIONS 16

void
iwx_sta_rx_agg(struct iwx_softc *sc, struct ieee80211_node *ni, uint8_t tid,
    uint16_t ssn, uint16_t winsize, int start)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_add_sta_cmd cmd;
	struct iwx_node *in = (void *)ni;
	int err, s;
	uint32_t status;

	if (start && sc->sc_rx_ba_sessions >= IWX_MAX_RX_BA_SESSIONS) {
		ieee80211_addba_req_refuse(ic, ni, tid);
		return;
	}

	memset(&cmd, 0, sizeof(cmd));

	cmd.sta_id = IWX_STATION_ID;
	cmd.mac_id_n_color
	    = htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	cmd.add_modify = IWX_STA_MODE_MODIFY;

	if (start) {
		cmd.add_immediate_ba_tid = (uint8_t)tid;
		cmd.add_immediate_ba_ssn = htole16(ssn);
		cmd.rx_ba_window = htole16(winsize);
	} else {
		cmd.remove_immediate_ba_tid = (uint8_t)tid;
	}
	cmd.modify_mask = start ? IWX_STA_MODIFY_ADD_BA_TID :
	    IWX_STA_MODIFY_REMOVE_BA_TID;

	status = IWX_ADD_STA_SUCCESS;
	err = iwx_send_cmd_pdu_status(sc, IWX_ADD_STA, sizeof(cmd), &cmd,
	    &status);

	s = splnet();
	if (!err && (status & IWX_ADD_STA_STATUS_MASK) == IWX_ADD_STA_SUCCESS) {
		if (start) {
			sc->sc_rx_ba_sessions++;
			ieee80211_addba_req_accept(ic, ni, tid);
		} else if (sc->sc_rx_ba_sessions > 0)
			sc->sc_rx_ba_sessions--;
	} else if (start)
		ieee80211_addba_req_refuse(ic, ni, tid);

	splx(s);
}

void
iwx_htprot_task(void *arg)
{
	struct iwx_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	int err, s = splnet();

	if (sc->sc_flags & IWX_FLAG_SHUTDOWN) {
		refcnt_rele_wake(&sc->task_refs);
		splx(s);
		return;
	}

	/* This call updates HT protection based on in->in_ni.ni_htop1. */
	err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 1);
	if (err)
		printf("%s: could not change HT protection: error %d\n",
		    DEVNAME(sc), err);

	refcnt_rele_wake(&sc->task_refs);
	splx(s);
}

/*
 * This function is called by upper layer when HT protection settings in
 * beacons have changed.
 */
void
iwx_update_htprot(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct iwx_softc *sc = ic->ic_softc;

	/* assumes that ni == ic->ic_bss */
	iwx_add_task(sc, systq, &sc->htprot_task);
}

void
iwx_ba_task(void *arg)
{
	struct iwx_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	int s = splnet();

	if (sc->sc_flags & IWX_FLAG_SHUTDOWN) {
		refcnt_rele_wake(&sc->task_refs);
		splx(s);
		return;
	}
	
	if (sc->ba_start)
		iwx_sta_rx_agg(sc, ni, sc->ba_tid, sc->ba_ssn,
		    sc->ba_winsize, 1);
	else
		iwx_sta_rx_agg(sc, ni, sc->ba_tid, 0, 0, 0);

	refcnt_rele_wake(&sc->task_refs);
	splx(s);
}

/*
 * This function is called by upper layer when an ADDBA request is received
 * from another STA and before the ADDBA response is sent.
 */
int
iwx_ampdu_rx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
	struct iwx_softc *sc = IC2IFP(ic)->if_softc;

	if (sc->sc_rx_ba_sessions >= IWX_MAX_RX_BA_SESSIONS)
		return ENOSPC;

	sc->ba_start = 1;
	sc->ba_tid = tid;
	sc->ba_ssn = htole16(ba->ba_winstart);
	sc->ba_winsize = htole16(ba->ba_winsize);
	iwx_add_task(sc, systq, &sc->ba_task);

	return EBUSY;
}

/*
 * This function is called by upper layer on teardown of an HT-immediate
 * Block Ack agreement (eg. upon receipt of a DELBA frame).
 */
void
iwx_ampdu_rx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct iwx_softc *sc = IC2IFP(ic)->if_softc;

	sc->ba_start = 0;
	sc->ba_tid = tid;
	iwx_add_task(sc, systq, &sc->ba_task);
}

void
iwx_set_hw_address_8000(struct iwx_softc *sc, struct iwx_nvm_data *data,
    const uint16_t *mac_override, const uint16_t *nvm_hw)
{
	const uint8_t *hw_addr;

	if (mac_override) {
		static const uint8_t reserved_mac[] = {
			0x02, 0xcc, 0xaa, 0xff, 0xee, 0x00
		};

		hw_addr = (const uint8_t *)(mac_override +
				 IWX_MAC_ADDRESS_OVERRIDE_8000);

		/*
		 * Store the MAC address from MAO section.
		 * No byte swapping is required in MAO section
		 */
		memcpy(data->hw_addr, hw_addr, ETHER_ADDR_LEN);

		/*
		 * Force the use of the OTP MAC address in case of reserved MAC
		 * address in the NVM, or if address is given but invalid.
		 */
		if (memcmp(reserved_mac, hw_addr, ETHER_ADDR_LEN) != 0 &&
		    (memcmp(etherbroadcastaddr, data->hw_addr,
		    sizeof(etherbroadcastaddr)) != 0) &&
		    (memcmp(etheranyaddr, data->hw_addr,
		    sizeof(etheranyaddr)) != 0) &&
		    !ETHER_IS_MULTICAST(data->hw_addr))
			return;
	}

	if (nvm_hw) {
		/* Read the mac address from WFMP registers. */
		uint32_t mac_addr0, mac_addr1;

		if (!iwx_nic_lock(sc))
			goto out;
		mac_addr0 = htole32(iwx_read_prph(sc, IWX_WFMP_MAC_ADDR_0));
		mac_addr1 = htole32(iwx_read_prph(sc, IWX_WFMP_MAC_ADDR_1));
		iwx_nic_unlock(sc);

		hw_addr = (const uint8_t *)&mac_addr0;
		data->hw_addr[0] = hw_addr[3];
		data->hw_addr[1] = hw_addr[2];
		data->hw_addr[2] = hw_addr[1];
		data->hw_addr[3] = hw_addr[0];

		hw_addr = (const uint8_t *)&mac_addr1;
		data->hw_addr[4] = hw_addr[1];
		data->hw_addr[5] = hw_addr[0];

		return;
	}
out:
	printf("%s: mac address not found\n", DEVNAME(sc));
	memset(data->hw_addr, 0, sizeof(data->hw_addr));
}

int
iwx_parse_nvm_data(struct iwx_softc *sc, const uint16_t *nvm_hw,
    const uint16_t *nvm_sw, const uint16_t *nvm_calib,
    const uint16_t *mac_override, const uint16_t *phy_sku,
    const uint16_t *regulatory, int n_regulatory)
{
	struct iwx_nvm_data *data = &sc->sc_nvm;
	uint32_t sku, radio_cfg;
	uint16_t lar_config, lar_offset;

	data->nvm_version = le16_to_cpup(nvm_sw + IWX_NVM_VERSION);

	radio_cfg = le32_to_cpup((uint32_t *)(phy_sku + IWX_RADIO_CFG_8000));
	data->radio_cfg_type = IWX_NVM_RF_CFG_TYPE_MSK_8000(radio_cfg);
	data->radio_cfg_step = IWX_NVM_RF_CFG_STEP_MSK_8000(radio_cfg);
	data->radio_cfg_dash = IWX_NVM_RF_CFG_DASH_MSK_8000(radio_cfg);
	data->radio_cfg_pnum = IWX_NVM_RF_CFG_PNUM_MSK_8000(radio_cfg);
	data->valid_tx_ant = IWX_NVM_RF_CFG_TX_ANT_MSK_8000(radio_cfg);
	data->valid_rx_ant = IWX_NVM_RF_CFG_RX_ANT_MSK_8000(radio_cfg);

	sku = le32_to_cpup((uint32_t *)(phy_sku + IWX_SKU_8000));
	data->sku_cap_band_24GHz_enable = sku & IWX_NVM_SKU_CAP_BAND_24GHZ;
	data->sku_cap_band_52GHz_enable = sku & IWX_NVM_SKU_CAP_BAND_52GHZ;
	data->sku_cap_11n_enable = sku & IWX_NVM_SKU_CAP_11N_ENABLE;
	data->sku_cap_mimo_disable = sku & IWX_NVM_SKU_CAP_MIMO_DISABLE;

	lar_offset = data->nvm_version < 0xE39 ?
			       IWX_NVM_LAR_OFFSET_8000_OLD :
			       IWX_NVM_LAR_OFFSET_8000;

	lar_config = le16_to_cpup(regulatory + lar_offset);
	data->n_hw_addrs = le16_to_cpup(nvm_sw + IWX_N_HW_ADDRS_8000);
	iwx_set_hw_address_8000(sc, data, mac_override, nvm_hw);

	iwx_init_channel_map(sc, &regulatory[IWX_NVM_CHANNELS_8000],
	    iwx_nvm_channels_8000,
	    MIN(n_regulatory, nitems(iwx_nvm_channels_8000)));

	data->calib_version = 255;   /* TODO:
					this value will prevent some checks from
					failing, we need to check if this
					field is still needed, and if it does,
					where is it in the NVM */

	return 0;
}

int
iwx_parse_nvm_sections(struct iwx_softc *sc, struct iwx_nvm_section *sections)
{
	const uint16_t *hw, *sw, *calib, *mac_override = NULL, *phy_sku = NULL;
	const uint16_t *regulatory = NULL;
	int n_regulatory = 0;

	/* Checking for required sections */

	/* SW and REGULATORY sections are mandatory */
	if (!sections[IWX_NVM_SECTION_TYPE_SW].data ||
	    !sections[IWX_NVM_SECTION_TYPE_REGULATORY].data) {
		return ENOENT;
	}
	/* MAC_OVERRIDE or at least HW section must exist */
	if (!sections[IWX_NVM_SECTION_TYPE_HW_8000].data &&
	    !sections[IWX_NVM_SECTION_TYPE_MAC_OVERRIDE].data) {
		return ENOENT;
	}

	/* PHY_SKU section is mandatory in B0 */
	if (!sections[IWX_NVM_SECTION_TYPE_PHY_SKU].data) {
		return ENOENT;
	}

	regulatory = (const uint16_t *)
	    sections[IWX_NVM_SECTION_TYPE_REGULATORY].data;
	n_regulatory = sections[IWX_NVM_SECTION_TYPE_REGULATORY].length;
	hw = (const uint16_t *)
	    sections[IWX_NVM_SECTION_TYPE_HW_8000].data;
	mac_override =
		(const uint16_t *)
		sections[IWX_NVM_SECTION_TYPE_MAC_OVERRIDE].data;
	phy_sku = (const uint16_t *)
	    sections[IWX_NVM_SECTION_TYPE_PHY_SKU].data;

	sw = (const uint16_t *)sections[IWX_NVM_SECTION_TYPE_SW].data;
	calib = (const uint16_t *)
	    sections[IWX_NVM_SECTION_TYPE_CALIBRATION].data;

	/* XXX should pass in the length of every section */
	return iwx_parse_nvm_data(sc, hw, sw, calib, mac_override,
	    phy_sku, regulatory, n_regulatory);
}

int
iwx_nvm_init(struct iwx_softc *sc)
{
	struct iwx_nvm_section nvm_sections[IWX_NVM_NUM_OF_SECTIONS];
	int i, section, err;
	uint16_t len;
	uint8_t *buf;
	const size_t bufsz = sc->sc_nvm_max_section_size;

	memset(nvm_sections, 0, sizeof(nvm_sections));

	buf = malloc(bufsz, M_DEVBUF, M_WAIT);
	if (buf == NULL)
		return ENOMEM;

	for (i = 0; i < nitems(iwx_nvm_to_read); i++) {
		section = iwx_nvm_to_read[i];
		KASSERT(section <= nitems(nvm_sections));

		err = iwx_nvm_read_section(sc, section, buf, &len, bufsz);
		if (err) {
			err = 0;
			continue;
		}
		nvm_sections[section].data = malloc(len, M_DEVBUF, M_WAIT);
		if (nvm_sections[section].data == NULL) {
			err = ENOMEM;
			break;
		}
		memcpy(nvm_sections[section].data, buf, len);
		nvm_sections[section].length = len;
	}
	free(buf, M_DEVBUF, bufsz);
	if (err == 0)
		err = iwx_parse_nvm_sections(sc, nvm_sections);

	for (i = 0; i < IWX_NVM_NUM_OF_SECTIONS; i++) {
		if (nvm_sections[i].data != NULL)
			free(nvm_sections[i].data, M_DEVBUF,
			    nvm_sections[i].length);
	}

	return err;
}

int
iwx_load_firmware(struct iwx_softc *sc)
{
	struct iwx_fw_sects *fws;
	int err, w;

	sc->sc_uc.uc_intr = 0;

	fws = &sc->sc_fw.fw_sects[IWX_UCODE_TYPE_REGULAR];
	err = iwx_ctxt_info_init(sc, fws);
	if (err) {
		printf("%s: could not init context info\n", DEVNAME(sc));
		return err;
	}

	/* wait for the firmware to load */
	for (w = 0; !sc->sc_uc.uc_intr && w < 10; w++) {
		err = tsleep_nsec(&sc->sc_uc, 0, "iwxuc", MSEC_TO_NSEC(100));
	}
	if (err || !sc->sc_uc.uc_ok)
		printf("%s: could not load firmware\n", DEVNAME(sc));
	if (!sc->sc_uc.uc_ok)
		return EINVAL;

	return err;
}

int
iwx_start_fw(struct iwx_softc *sc)
{
	int err;

	IWX_WRITE(sc, IWX_CSR_INT, ~0);

	err = iwx_nic_init(sc);
	if (err) {
		printf("%s: unable to init nic\n", DEVNAME(sc));
		return err;
	}

	/* make sure rfkill handshake bits are cleared */
	IWX_WRITE(sc, IWX_CSR_UCODE_DRV_GP1_CLR, IWX_CSR_UCODE_SW_BIT_RFKILL);
	IWX_WRITE(sc, IWX_CSR_UCODE_DRV_GP1_CLR,
	    IWX_CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);

	/* clear (again), then enable firwmare load interrupt */
	IWX_WRITE(sc, IWX_CSR_INT, ~0);
	iwx_enable_fwload_interrupt(sc);

	return iwx_load_firmware(sc);
}

int
iwx_send_tx_ant_cfg(struct iwx_softc *sc, uint8_t valid_tx_ant)
{
	struct iwx_tx_ant_cfg_cmd tx_ant_cmd = {
		.valid = htole32(valid_tx_ant),
	};

	return iwx_send_cmd_pdu(sc, IWX_TX_ANT_CONFIGURATION_CMD,
	    0, sizeof(tx_ant_cmd), &tx_ant_cmd);
}

int
iwx_send_phy_cfg_cmd(struct iwx_softc *sc)
{
	struct iwx_phy_cfg_cmd phy_cfg_cmd;

	phy_cfg_cmd.phy_cfg = htole32(sc->sc_fw_phy_config);
	phy_cfg_cmd.calib_control.event_trigger =
	    sc->sc_default_calib[IWX_UCODE_TYPE_REGULAR].event_trigger;
	phy_cfg_cmd.calib_control.flow_trigger =
	    sc->sc_default_calib[IWX_UCODE_TYPE_REGULAR].flow_trigger;

	return iwx_send_cmd_pdu(sc, IWX_PHY_CONFIGURATION_CMD, 0,
	    sizeof(phy_cfg_cmd), &phy_cfg_cmd);
}

int
iwx_send_dqa_cmd(struct iwx_softc *sc)
{
	struct iwx_dqa_enable_cmd dqa_cmd = {
		.cmd_queue = htole32(IWX_DQA_CMD_QUEUE),
	};
	uint32_t cmd_id;

	cmd_id = iwx_cmd_id(IWX_DQA_ENABLE_CMD, IWX_DATA_PATH_GROUP, 0);
	return iwx_send_cmd_pdu(sc, cmd_id, 0, sizeof(dqa_cmd), &dqa_cmd);
}

int
iwx_load_ucode_wait_alive(struct iwx_softc *sc)
{
	int err;

	err = iwx_read_firmware(sc);
	if (err)
		return err;

	err = iwx_start_fw(sc);
	if (err)
		return err;

	iwx_post_alive(sc);

	return 0;
}

int
iwx_run_init_mvm_ucode(struct iwx_softc *sc, int readnvm)
{
	const int wait_flags = IWX_INIT_COMPLETE;
	struct iwx_nvm_access_complete_cmd nvm_complete = {};
	struct iwx_init_extended_cfg_cmd init_cfg = {
		.init_flags = htole32(IWX_INIT_NVM),
	};
	int err;

	if ((sc->sc_flags & IWX_FLAG_RFKILL) && !readnvm) {
		printf("%s: radio is disabled by hardware switch\n",
		    DEVNAME(sc));
		return EPERM;
	}

	sc->sc_init_complete = 0;
	err = iwx_load_ucode_wait_alive(sc);
	if (err) {
		printf("%s: failed to load init firmware\n", DEVNAME(sc));
		return err;
	}

	/*
	 * Send init config command to mark that we are sending NVM
	 * access commands
	 */
	err = iwx_send_cmd_pdu(sc, IWX_WIDE_ID(IWX_SYSTEM_GROUP,
	    IWX_INIT_EXTENDED_CFG_CMD), 0, sizeof(init_cfg), &init_cfg);
	if (err)
		return err;

	if (readnvm) {
		err = iwx_nvm_init(sc);
		if (err) {
			printf("%s: failed to read nvm\n", DEVNAME(sc));
			return err;
		}
	}

	err = iwx_send_cmd_pdu(sc, IWX_WIDE_ID(IWX_REGULATORY_AND_NVM_GROUP,
	    IWX_NVM_ACCESS_COMPLETE), 0, sizeof(nvm_complete), &nvm_complete);
	if (err)
		return err;

	/* Wait for the init complete notification from the firmware. */
	while ((sc->sc_init_complete & wait_flags) != wait_flags) {
		err = tsleep_nsec(&sc->sc_init_complete, 0, "iwxinit",
		    SEC_TO_NSEC(2));
		if (err)
			return err;
	}

	if (readnvm && IEEE80211_ADDR_EQ(etheranyaddr, sc->sc_ic.ic_myaddr))
		IEEE80211_ADDR_COPY(sc->sc_ic.ic_myaddr,
		    sc->sc_nvm.hw_addr);
	return 0;
}

int
iwx_config_ltr(struct iwx_softc *sc)
{
	struct iwx_ltr_config_cmd cmd = {
		.flags = htole32(IWX_LTR_CFG_FLAG_FEATURE_ENABLE),
	};

	if (!sc->sc_ltr_enabled)
		return 0;

	return iwx_send_cmd_pdu(sc, IWX_LTR_CONFIG, 0, sizeof(cmd), &cmd);
}

void
iwx_update_rx_desc(struct iwx_softc *sc, struct iwx_rx_ring *ring, int idx)
{
	struct iwx_rx_data *data = &ring->data[idx];

	((uint64_t *)ring->desc)[idx] =
	    htole64(data->map->dm_segs[0].ds_addr | (idx & 0x0fff));
	bus_dmamap_sync(sc->sc_dmat, ring->free_desc_dma.map,
	    idx * sizeof(uint64_t), sizeof(uint64_t),
	    BUS_DMASYNC_PREWRITE);
}

int
iwx_rx_addbuf(struct iwx_softc *sc, int size, int idx)
{
	struct iwx_rx_ring *ring = &sc->rxq;
	struct iwx_rx_data *data = &ring->data[idx];
	struct mbuf *m;
	int err;
	int fatal = 0;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	if (size <= MCLBYTES) {
		MCLGET(m, M_DONTWAIT);
	} else {
		MCLGETI(m, M_DONTWAIT, NULL, IWX_RBUF_SIZE);
	}
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return ENOBUFS;
	}

	if (data->m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, data->map);
		fatal = 1;
	}

	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
	err = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (err) {
		/* XXX */
		if (fatal)
			panic("%s: could not load RX mbuf", DEVNAME(sc));
		m_freem(m);
		return err;
	}
	data->m = m;
	bus_dmamap_sync(sc->sc_dmat, data->map, 0, size, BUS_DMASYNC_PREREAD);

	/* Update RX descriptor. */
	iwx_update_rx_desc(sc, ring, idx);

	return 0;
}

int
iwx_rxmq_get_signal_strength(struct iwx_softc *sc,
    struct iwx_rx_mpdu_desc *desc)
{
	int energy_a, energy_b;

	energy_a = desc->v1.energy_a;
	energy_b = desc->v1.energy_b;
	energy_a = energy_a ? -energy_a : -256;
	energy_b = energy_b ? -energy_b : -256;
	return MAX(energy_a, energy_b);
}

void
iwx_rx_rx_phy_cmd(struct iwx_softc *sc, struct iwx_rx_packet *pkt,
    struct iwx_rx_data *data)
{
	struct iwx_rx_phy_info *phy_info = (void *)pkt->data;

	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof(*pkt),
	    sizeof(*phy_info), BUS_DMASYNC_POSTREAD);

	memcpy(&sc->sc_last_phy_info, phy_info, sizeof(sc->sc_last_phy_info));
}

/*
 * Retrieve the average noise (in dBm) among receivers.
 */
int
iwx_get_noise(const struct iwx_statistics_rx_non_phy *stats)
{
	int i, total, nbant, noise;

	total = nbant = noise = 0;
	for (i = 0; i < 3; i++) {
		noise = letoh32(stats->beacon_silence_rssi[i]) & 0xff;
		if (noise) {
			total += noise;
			nbant++;
		}
	}

	/* There should be at least one antenna but check anyway. */
	return (nbant == 0) ? -127 : (total / nbant) - 107;
}

void
iwx_rx_frame(struct iwx_softc *sc, struct mbuf *m, int chanidx,
     int is_shortpre, int rate_n_flags, uint32_t device_timestamp,
     struct ieee80211_rxinfo *rxi, struct mbuf_list *ml)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct ieee80211_channel *bss_chan;
	uint8_t saved_bssid[IEEE80211_ADDR_LEN] = { 0 };

	if (chanidx < 0 || chanidx >= nitems(ic->ic_channels))	
		chanidx = ieee80211_chan2ieee(ic, ic->ic_ibss_chan);

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);
	if (ni == ic->ic_bss) {
		/* 
		 * We may switch ic_bss's channel during scans.
		 * Record the current channel so we can restore it later.
		 */
		bss_chan = ni->ni_chan;
		IEEE80211_ADDR_COPY(&saved_bssid, ni->ni_macaddr);
	}
	ni->ni_chan = &ic->ic_channels[chanidx];

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct iwx_rx_radiotap_header *tap = &sc->sc_rxtap;
		uint16_t chan_flags;

		tap->wr_flags = 0;
		if (is_shortpre)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[chanidx].ic_freq);
		chan_flags = ic->ic_channels[chanidx].ic_flags;
		if (ic->ic_curmode != IEEE80211_MODE_11N)
			chan_flags &= ~IEEE80211_CHAN_HT;
		tap->wr_chan_flags = htole16(chan_flags);
		tap->wr_dbm_antsignal = (int8_t)rxi->rxi_rssi;
		tap->wr_dbm_antnoise = (int8_t)sc->sc_noise;
		tap->wr_tsft = device_timestamp;
		if (rate_n_flags & IWX_RATE_MCS_HT_MSK) {
			uint8_t mcs = (rate_n_flags &
			    (IWX_RATE_HT_MCS_RATE_CODE_MSK |
			    IWX_RATE_HT_MCS_NSS_MSK));
			tap->wr_rate = (0x80 | mcs);
		} else {
			uint8_t rate = (rate_n_flags &
			    IWX_RATE_LEGACY_RATE_MSK);
			switch (rate) {
			/* CCK rates. */
			case  10: tap->wr_rate =   2; break;
			case  20: tap->wr_rate =   4; break;
			case  55: tap->wr_rate =  11; break;
			case 110: tap->wr_rate =  22; break;
			/* OFDM rates. */
			case 0xd: tap->wr_rate =  12; break;
			case 0xf: tap->wr_rate =  18; break;
			case 0x5: tap->wr_rate =  24; break;
			case 0x7: tap->wr_rate =  36; break;
			case 0x9: tap->wr_rate =  48; break;
			case 0xb: tap->wr_rate =  72; break;
			case 0x1: tap->wr_rate =  96; break;
			case 0x3: tap->wr_rate = 108; break;
			/* Unknown rate: should not happen. */
			default:  tap->wr_rate =   0;
			}
		}

		bpf_mtap_hdr(sc->sc_drvbpf, tap, sc->sc_rxtap_len,
		    m, BPF_DIRECTION_IN);
	}
#endif
	ieee80211_inputm(IC2IFP(ic), m, ni, rxi, ml);
	/*
	 * ieee80211_inputm() might have changed our BSS.
	 * Restore ic_bss's channel if we are still in the same BSS.
	 */
	if (ni == ic->ic_bss && IEEE80211_ADDR_EQ(saved_bssid, ni->ni_macaddr))
		ni->ni_chan = bss_chan;
	ieee80211_release_node(ic, ni);
}

void
iwx_rx_mpdu_mq(struct iwx_softc *sc, struct mbuf *m, void *pktdata,
    size_t maxlen, struct mbuf_list *ml)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_rxinfo rxi;
	struct iwx_rx_mpdu_desc *desc;
	uint32_t len, hdrlen, rate_n_flags, device_timestamp;
	int rssi;
	uint8_t chanidx;
	uint16_t phy_info;

	desc = (struct iwx_rx_mpdu_desc *)pktdata;

	if (!(desc->status & htole16(IWX_RX_MPDU_RES_STATUS_CRC_OK)) ||
	    !(desc->status & htole16(IWX_RX_MPDU_RES_STATUS_OVERRUN_OK))) {
		m_freem(m);
		return; /* drop */
	}

	len = le16toh(desc->mpdu_len);
	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		/* Allow control frames in monitor mode. */
		if (len < sizeof(struct ieee80211_frame_cts)) {
			ic->ic_stats.is_rx_tooshort++;
			IC2IFP(ic)->if_ierrors++;
			m_freem(m);
			return;
		}
	} else if (len < sizeof(struct ieee80211_frame)) {
		ic->ic_stats.is_rx_tooshort++;
		IC2IFP(ic)->if_ierrors++;
		m_freem(m);
		return;
	}
	if (len > maxlen - sizeof(*desc)) {
		IC2IFP(ic)->if_ierrors++;
		m_freem(m);
		return;
	}

	m->m_data = pktdata + sizeof(*desc);
	m->m_pkthdr.len = m->m_len = len;

	/* Account for padding following the frame header. */
	if (desc->mac_flags2 & IWX_RX_MPDU_MFLG2_PAD) {
		struct ieee80211_frame *wh = mtod(m, struct ieee80211_frame *);
		int type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
		if (type == IEEE80211_FC0_TYPE_CTL) {
			switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
			case IEEE80211_FC0_SUBTYPE_CTS:
				hdrlen = sizeof(struct ieee80211_frame_cts);
				break;
			case IEEE80211_FC0_SUBTYPE_ACK:
				hdrlen = sizeof(struct ieee80211_frame_ack);
				break;
			default:
				hdrlen = sizeof(struct ieee80211_frame_min);
				break;
			}
		} else
			hdrlen = ieee80211_get_hdrlen(wh);
		memmove(m->m_data + 2, m->m_data, hdrlen);
		m_adj(m, 2);
	}

	phy_info = le16toh(desc->phy_info);
	rate_n_flags = le32toh(desc->v1.rate_n_flags);
	chanidx = desc->v1.channel;
	device_timestamp = desc->v1.gp2_on_air_rise;

	rssi = iwx_rxmq_get_signal_strength(sc, desc);
	rssi = (0 - IWX_MIN_DBM) + rssi;	/* normalize */
	rssi = MIN(rssi, ic->ic_max_rssi);	/* clip to max. 100% */

	memset(&rxi, 0, sizeof(rxi));
	rxi.rxi_rssi = rssi;
	rxi.rxi_tstamp = le64toh(desc->v1.tsf_on_air_rise);

	iwx_rx_frame(sc, m, chanidx,
	    (phy_info & IWX_RX_MPDU_PHY_SHORT_PREAMBLE),
	    rate_n_flags, device_timestamp, &rxi, ml);
}

void
iwx_enable_ht_cck_fallback(struct iwx_softc *sc, struct iwx_node *in)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	uint8_t rval = (rs->rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL);
	uint8_t min_rval = ieee80211_min_basic_rate(ic);
	int i;

	/* Are CCK frames forbidden in our BSS? */
	if (IWX_RVAL_IS_OFDM(min_rval))
		return;

	in->ht_force_cck = 1;

	ieee80211_mira_cancel_timeouts(&in->in_mn);
	ieee80211_mira_node_init(&in->in_mn);
	ieee80211_amrr_node_init(&sc->sc_amrr, &in->in_amn);

	/* Choose initial CCK Tx rate. */
	ni->ni_txrate = 0;
	for (i = 0; i < rs->rs_nrates; i++) {
		rval = (rs->rs_rates[i] & IEEE80211_RATE_VAL);
		if (rval == min_rval) {
			ni->ni_txrate = i;
			break;
		}
	}
}

void
iwx_rx_tx_cmd_single(struct iwx_softc *sc, struct iwx_rx_packet *pkt,
    struct iwx_node *in, int txmcs, int txrate)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	struct ifnet *ifp = IC2IFP(ic);
	struct iwx_tx_resp *tx_resp = (void *)pkt->data;
	int status = le16toh(tx_resp->status.status) & IWX_TX_STATUS_MSK;
	int txfail;
	
	KASSERT(tx_resp->frame_count == 1);

	txfail = (status != IWX_TX_STATUS_SUCCESS &&
	    status != IWX_TX_STATUS_DIRECT_DONE);

	/*
	 * Update rate control statistics.
	 * Only report frames which were actually queued with the currently
	 * selected Tx rate. Because Tx queues are relatively long we may
	 * encounter previously selected rates here during Tx bursts.
	 * Providing feedback based on such frames can lead to suboptimal
	 * Tx rate control decisions.
	 */
	if ((ni->ni_flags & IEEE80211_NODE_HT) == 0 || in->ht_force_cck) {
		if (txrate == ni->ni_txrate) {
			in->in_amn.amn_txcnt++;
			if (txfail)
				in->in_amn.amn_retrycnt++;
			if (tx_resp->failure_frame > 0)
				in->in_amn.amn_retrycnt++;
		}
	} else if (ic->ic_fixed_mcs == -1 && txmcs == ni->ni_txmcs) {
		in->in_mn.frames += tx_resp->frame_count;
		in->in_mn.ampdu_size = le16toh(tx_resp->byte_cnt);
		in->in_mn.agglen = tx_resp->frame_count;
		if (tx_resp->failure_frame > 0)
			in->in_mn.retries += tx_resp->failure_frame;
		if (txfail)
			in->in_mn.txfail += tx_resp->frame_count;
		if (ic->ic_state == IEEE80211_S_RUN && !in->ht_force_cck) {
			int otxmcs = ni->ni_txmcs;

			ieee80211_mira_choose(&in->in_mn, ic, &in->in_ni);

			/* Fall back to CCK rates if MCS 0 is failing. */
			if (txfail && IEEE80211_IS_CHAN_2GHZ(ni->ni_chan) &&
			    otxmcs == 0 && ni->ni_txmcs == 0)
				iwx_enable_ht_cck_fallback(sc, in);
		}
	}

	if (txfail)
		ifp->if_oerrors++;
}

void
iwx_txd_done(struct iwx_softc *sc, struct iwx_tx_data *txd)
{
	struct ieee80211com *ic = &sc->sc_ic;

	bus_dmamap_sync(sc->sc_dmat, txd->map, 0, txd->map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, txd->map);
	m_freem(txd->m);
	txd->m = NULL;

	KASSERT(txd->in);
	ieee80211_release_node(ic, &txd->in->in_ni);
	txd->in = NULL;
}

void
iwx_rx_tx_cmd(struct iwx_softc *sc, struct iwx_rx_packet *pkt,
    struct iwx_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	struct iwx_cmd_header *cmd_hdr = &pkt->hdr;
	int idx = cmd_hdr->idx;
	int qid = cmd_hdr->qid;
	struct iwx_tx_ring *ring = &sc->txq[qid];
	struct iwx_tx_data *txd;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, IWX_RBUF_SIZE,
	    BUS_DMASYNC_POSTREAD);

	sc->sc_tx_timer = 0;

	txd = &ring->data[idx];
	if (txd->m == NULL)
		return;

	iwx_rx_tx_cmd_single(sc, pkt, txd->in, txd->txmcs, txd->txrate);
	iwx_txd_done(sc, txd);

	/*
	 * XXX Sometimes we miss Tx completion interrupts.
	 * We cannot check Tx success/failure for affected frames; just free
	 * the associated mbuf and release the associated node reference.
	 */
	while (ring->tail != idx) {
		txd = &ring->data[ring->tail];
		if (txd->m != NULL) {
			DPRINTF(("%s: missed Tx completion: tail=%d idx=%d\n",
			    __func__, ring->tail, idx));
			iwx_txd_done(sc, txd);
			ring->queued--;
		}
		ring->tail = (ring->tail + 1) % IWX_TX_RING_COUNT;
	}

	if (--ring->queued < IWX_TX_RING_LOMARK) {
		sc->qfullmsk &= ~(1 << ring->qid);
		if (sc->qfullmsk == 0 && ifq_is_oactive(&ifp->if_snd)) {
			ifq_clr_oactive(&ifp->if_snd);
			/*
			 * Well, we're in interrupt context, but then again
			 * I guess net80211 does all sorts of stunts in
			 * interrupt context, so maybe this is no biggie.
			 */
			(*ifp->if_start)(ifp);
		}
	}
}

void
iwx_rx_bmiss(struct iwx_softc *sc, struct iwx_rx_packet *pkt,
    struct iwx_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_missed_beacons_notif *mbn = (void *)pkt->data;
	uint32_t missed;

	if ((ic->ic_opmode != IEEE80211_M_STA) ||
	    (ic->ic_state != IEEE80211_S_RUN))
		return;

	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof(*pkt),
	    sizeof(*mbn), BUS_DMASYNC_POSTREAD);

	missed = le32toh(mbn->consec_missed_beacons_since_last_rx);
	if (missed > ic->ic_bmissthres && ic->ic_mgt_timer == 0) {
		if (ic->ic_if.if_flags & IFF_DEBUG)
			printf("%s: receiving no beacons from %s; checking if "
			    "this AP is still responding to probe requests\n",
			    DEVNAME(sc), ether_sprintf(ic->ic_bss->ni_macaddr));
		/*
		 * Rather than go directly to scan state, try to send a
		 * directed probe request first. If that fails then the
		 * state machine will drop us into scanning after timing
		 * out waiting for a probe response.
		 */
		IEEE80211_SEND_MGMT(ic, ic->ic_bss,
		    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
	}

}

int
iwx_binding_cmd(struct iwx_softc *sc, struct iwx_node *in, uint32_t action)
{
	struct iwx_binding_cmd cmd;
	struct iwx_phy_ctxt *phyctxt = in->in_phyctxt;
	uint32_t mac_id = IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color);
	int i, err, active = (sc->sc_flags & IWX_FLAG_BINDING_ACTIVE);
	uint32_t status;

	if (action == IWX_FW_CTXT_ACTION_ADD && active)
		panic("binding already added");
	if (action == IWX_FW_CTXT_ACTION_REMOVE && !active)
		panic("binding already removed");

	if (phyctxt == NULL) /* XXX race with iwx_stop() */
		return EINVAL;

	memset(&cmd, 0, sizeof(cmd));

	cmd.id_and_color
	    = htole32(IWX_FW_CMD_ID_AND_COLOR(phyctxt->id, phyctxt->color));
	cmd.action = htole32(action);
	cmd.phy = htole32(IWX_FW_CMD_ID_AND_COLOR(phyctxt->id, phyctxt->color));

	cmd.macs[0] = htole32(mac_id);
	for (i = 1; i < IWX_MAX_MACS_IN_BINDING; i++)
		cmd.macs[i] = htole32(IWX_FW_CTXT_INVALID);

	if (IEEE80211_IS_CHAN_2GHZ(phyctxt->channel) ||
	    !isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_CDB_SUPPORT))
		cmd.lmac_id = htole32(IWX_LMAC_24G_INDEX);
	else
		cmd.lmac_id = htole32(IWX_LMAC_5G_INDEX);

	status = 0;
	err = iwx_send_cmd_pdu_status(sc, IWX_BINDING_CONTEXT_CMD, sizeof(cmd),
	    &cmd, &status);
	if (err == 0 && status != 0)
		err = EIO;

	return err;
}

int
iwx_phy_ctxt_cmd_uhb(struct iwx_softc *sc, struct iwx_phy_ctxt *ctxt,
    uint8_t chains_static, uint8_t chains_dynamic, uint32_t action,
    uint32_t apply_time)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_phy_context_cmd_uhb cmd;
	uint8_t active_cnt, idle_cnt;
	struct ieee80211_channel *chan = ctxt->channel;

	memset(&cmd, 0, sizeof(cmd));
	cmd.id_and_color = htole32(IWX_FW_CMD_ID_AND_COLOR(ctxt->id,
	    ctxt->color));
	cmd.action = htole32(action);
	cmd.apply_time = htole32(apply_time);

	cmd.ci.band = IEEE80211_IS_CHAN_2GHZ(chan) ?
	    IWX_PHY_BAND_24 : IWX_PHY_BAND_5;
	cmd.ci.channel = htole32(ieee80211_chan2ieee(ic, chan));
	cmd.ci.width = IWX_PHY_VHT_CHANNEL_MODE20;
	cmd.ci.ctrl_pos = IWX_PHY_VHT_CTRL_POS_1_BELOW;

	idle_cnt = chains_static;
	active_cnt = chains_dynamic;
	cmd.rxchain_info = htole32(iwx_fw_valid_rx_ant(sc) <<
					IWX_PHY_RX_CHAIN_VALID_POS);
	cmd.rxchain_info |= htole32(idle_cnt << IWX_PHY_RX_CHAIN_CNT_POS);
	cmd.rxchain_info |= htole32(active_cnt <<
	    IWX_PHY_RX_CHAIN_MIMO_CNT_POS);
	cmd.txchain_info = htole32(iwx_fw_valid_tx_ant(sc));

	return iwx_send_cmd_pdu(sc, IWX_PHY_CONTEXT_CMD, 0, sizeof(cmd), &cmd);
}

int
iwx_phy_ctxt_cmd(struct iwx_softc *sc, struct iwx_phy_ctxt *ctxt,
    uint8_t chains_static, uint8_t chains_dynamic, uint32_t action,
    uint32_t apply_time)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_phy_context_cmd cmd;
	uint8_t active_cnt, idle_cnt;
	struct ieee80211_channel *chan = ctxt->channel;

	/*
	 * Intel increased the size of the fw_channel_info struct and neglected
	 * to bump the phy_context_cmd struct, which contains an fw_channel_info
	 * member in the middle.
	 * To keep things simple we use a separate function to handle the larger
	 * variant of the phy context command.
	 */
	if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_ULTRA_HB_CHANNELS))
		return iwx_phy_ctxt_cmd_uhb(sc, ctxt, chains_static,
		    chains_dynamic, action, apply_time);

	memset(&cmd, 0, sizeof(cmd));
	cmd.id_and_color = htole32(IWX_FW_CMD_ID_AND_COLOR(ctxt->id,
	    ctxt->color));
	cmd.action = htole32(action);
	cmd.apply_time = htole32(apply_time);

	cmd.ci.band = IEEE80211_IS_CHAN_2GHZ(chan) ?
	    IWX_PHY_BAND_24 : IWX_PHY_BAND_5;
	cmd.ci.channel = ieee80211_chan2ieee(ic, chan);
	cmd.ci.width = IWX_PHY_VHT_CHANNEL_MODE20;
	cmd.ci.ctrl_pos = IWX_PHY_VHT_CTRL_POS_1_BELOW;

	idle_cnt = chains_static;
	active_cnt = chains_dynamic;
	cmd.rxchain_info = htole32(iwx_fw_valid_rx_ant(sc) <<
					IWX_PHY_RX_CHAIN_VALID_POS);
	cmd.rxchain_info |= htole32(idle_cnt << IWX_PHY_RX_CHAIN_CNT_POS);
	cmd.rxchain_info |= htole32(active_cnt <<
	    IWX_PHY_RX_CHAIN_MIMO_CNT_POS);
	cmd.txchain_info = htole32(iwx_fw_valid_tx_ant(sc));

	return iwx_send_cmd_pdu(sc, IWX_PHY_CONTEXT_CMD, 0, sizeof(cmd), &cmd);
}

int
iwx_send_cmd(struct iwx_softc *sc, struct iwx_host_cmd *hcmd)
{
	struct iwx_tx_ring *ring = &sc->txq[IWX_DQA_CMD_QUEUE];
	struct iwx_tfh_tfd *desc;
	struct iwx_tx_data *txdata;
	struct iwx_device_cmd *cmd;
	struct mbuf *m;
	bus_addr_t paddr;
	uint64_t addr;
	int err = 0, i, paylen, off, s;
	int idx, code, async, group_id;
	size_t hdrlen, datasz;
	uint8_t *data;
	int generation = sc->sc_generation;

	code = hcmd->id;
	async = hcmd->flags & IWX_CMD_ASYNC;
	idx = ring->cur;

	for (i = 0, paylen = 0; i < nitems(hcmd->len); i++) {
		paylen += hcmd->len[i];
	}

	/* If this command waits for a response, allocate response buffer. */
	hcmd->resp_pkt = NULL;
	if (hcmd->flags & IWX_CMD_WANT_RESP) {
		uint8_t *resp_buf;
		KASSERT(!async);
		KASSERT(hcmd->resp_pkt_len >= sizeof(struct iwx_rx_packet));
		KASSERT(hcmd->resp_pkt_len <= IWX_CMD_RESP_MAX);
		if (sc->sc_cmd_resp_pkt[idx] != NULL)
			return ENOSPC;
		resp_buf = malloc(hcmd->resp_pkt_len, M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (resp_buf == NULL)
			return ENOMEM;
		sc->sc_cmd_resp_pkt[idx] = resp_buf;
		sc->sc_cmd_resp_len[idx] = hcmd->resp_pkt_len;
	} else {
		sc->sc_cmd_resp_pkt[idx] = NULL;
	}

	s = splnet();

	desc = &ring->desc[idx];
	txdata = &ring->data[idx];

	group_id = iwx_cmd_groupid(code);
	if (group_id != 0) {
		hdrlen = sizeof(cmd->hdr_wide);
		datasz = sizeof(cmd->data_wide);
	} else {
		hdrlen = sizeof(cmd->hdr);
		datasz = sizeof(cmd->data);
	}

	if (paylen > datasz) {
		/* Command is too large to fit in pre-allocated space. */
		size_t totlen = hdrlen + paylen;
		if (paylen > IWX_MAX_CMD_PAYLOAD_SIZE) {
			printf("%s: firmware command too long (%zd bytes)\n",
			    DEVNAME(sc), totlen);
			err = EINVAL;
			goto out;
		}
		m = MCLGETI(NULL, M_DONTWAIT, NULL, totlen);
		if (m == NULL) {
			printf("%s: could not get fw cmd mbuf (%zd bytes)\n",
			    DEVNAME(sc), totlen);
			err = ENOMEM;
			goto out;
		}
		cmd = mtod(m, struct iwx_device_cmd *);
		err = bus_dmamap_load(sc->sc_dmat, txdata->map, cmd,
		    totlen, NULL, BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (err) {
			printf("%s: could not load fw cmd mbuf (%zd bytes)\n",
			    DEVNAME(sc), totlen);
			m_freem(m);
			goto out;
		}
		txdata->m = m; /* mbuf will be freed in iwx_cmd_done() */
		paddr = txdata->map->dm_segs[0].ds_addr;
	} else {
		cmd = &ring->cmd[idx];
		paddr = txdata->cmd_paddr;
	}

	if (group_id != 0) {
		cmd->hdr_wide.opcode = iwx_cmd_opcode(code);
		cmd->hdr_wide.group_id = group_id;
		cmd->hdr_wide.qid = ring->qid;
		cmd->hdr_wide.idx = idx;
		cmd->hdr_wide.length = htole16(paylen);
		cmd->hdr_wide.version = iwx_cmd_version(code);
		data = cmd->data_wide;
	} else {
		cmd->hdr.code = code;
		cmd->hdr.flags = 0;
		cmd->hdr.qid = ring->qid;
		cmd->hdr.idx = idx;
		data = cmd->data;
	}

	for (i = 0, off = 0; i < nitems(hcmd->data); i++) {
		if (hcmd->len[i] == 0)
			continue;
		memcpy(data + off, hcmd->data[i], hcmd->len[i]);
		off += hcmd->len[i];
	}
	KASSERT(off == paylen);

	desc->tbs[0].tb_len = htole16(hdrlen + paylen);
	addr = htole64((uint64_t)paddr);
	memcpy(&desc->tbs[0].addr, &addr, sizeof(addr));
	desc->num_tbs = 1;

	if (paylen > datasz) {
		bus_dmamap_sync(sc->sc_dmat, txdata->map, 0,
		    hdrlen + paylen, BUS_DMASYNC_PREWRITE);
	} else {
		bus_dmamap_sync(sc->sc_dmat, ring->cmd_dma.map,
		    (char *)(void *)cmd - (char *)(void *)ring->cmd_dma.vaddr,
		    hdrlen + paylen, BUS_DMASYNC_PREWRITE);
	}
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map,
	    (char *)(void *)desc - (char *)(void *)ring->desc_dma.vaddr,
	    sizeof (*desc), BUS_DMASYNC_PREWRITE);
	/* Kick command ring. */
	DPRINTF(("%s: sending command 0x%x\n", __func__, code));
	ring->queued++;
	ring->cur = (ring->cur + 1) % IWX_CMD_QUEUE_SIZE;
	IWX_WRITE(sc, IWX_HBUS_TARG_WRPTR, ring->qid << 16 | ring->cur);

	if (!async) {
		err = tsleep_nsec(desc, PCATCH, "iwxcmd", SEC_TO_NSEC(1));
		if (err == 0) {
			/* if hardware is no longer up, return error */
			if (generation != sc->sc_generation) {
				err = ENXIO;
				goto out;
			}

			/* Response buffer will be freed in iwx_free_resp(). */
			hcmd->resp_pkt = (void *)sc->sc_cmd_resp_pkt[idx];
			sc->sc_cmd_resp_pkt[idx] = NULL;
		} else if (generation == sc->sc_generation) {
			free(sc->sc_cmd_resp_pkt[idx], M_DEVBUF,
			    sc->sc_cmd_resp_len[idx]);
			sc->sc_cmd_resp_pkt[idx] = NULL;	
		}
	}
 out:
	splx(s);

	return err;
}

int
iwx_send_cmd_pdu(struct iwx_softc *sc, uint32_t id, uint32_t flags,
    uint16_t len, const void *data)
{
	struct iwx_host_cmd cmd = {
		.id = id,
		.len = { len, },
		.data = { data, },
		.flags = flags,
	};

	return iwx_send_cmd(sc, &cmd);
}

int
iwx_send_cmd_status(struct iwx_softc *sc, struct iwx_host_cmd *cmd,
    uint32_t *status)
{
	struct iwx_rx_packet *pkt;
	struct iwx_cmd_response *resp;
	int err, resp_len;

	KASSERT((cmd->flags & IWX_CMD_WANT_RESP) == 0);
	cmd->flags |= IWX_CMD_WANT_RESP;
	cmd->resp_pkt_len = sizeof(*pkt) + sizeof(*resp);

	err = iwx_send_cmd(sc, cmd);
	if (err)
		return err;

	pkt = cmd->resp_pkt;
	if (pkt == NULL || (pkt->hdr.flags & IWX_CMD_FAILED_MSK))
		return EIO;

	resp_len = iwx_rx_packet_payload_len(pkt);
	if (resp_len != sizeof(*resp)) {
		iwx_free_resp(sc, cmd);
		return EIO;
	}

	resp = (void *)pkt->data;
	*status = le32toh(resp->status);
	iwx_free_resp(sc, cmd);
	return err;
}

int
iwx_send_cmd_pdu_status(struct iwx_softc *sc, uint32_t id, uint16_t len,
    const void *data, uint32_t *status)
{
	struct iwx_host_cmd cmd = {
		.id = id,
		.len = { len, },
		.data = { data, },
	};

	return iwx_send_cmd_status(sc, &cmd, status);
}

void
iwx_free_resp(struct iwx_softc *sc, struct iwx_host_cmd *hcmd)
{
	KASSERT((hcmd->flags & (IWX_CMD_WANT_RESP)) == IWX_CMD_WANT_RESP);
	free(hcmd->resp_pkt, M_DEVBUF, hcmd->resp_pkt_len);
	hcmd->resp_pkt = NULL;
}

void
iwx_cmd_done(struct iwx_softc *sc, int qid, int idx, int code)
{
	struct iwx_tx_ring *ring = &sc->txq[IWX_DQA_CMD_QUEUE];
	struct iwx_tx_data *data;

	if (qid != IWX_DQA_CMD_QUEUE) {
		return;	/* Not a command ack. */
	}

	data = &ring->data[idx];

	if (data->m != NULL) {
		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}
	wakeup(&ring->desc[idx]);

	DPRINTF(("%s: command 0x%x done\n", __func__, code));
	if (ring->queued == 0) {
		if (code != IWX_NVM_ACCESS_CMD) 
			DPRINTF(("%s: unexpected firmware response to command 0x%x\n",
				DEVNAME(sc), code));
	} else if (ring->queued > 0)
		ring->queued--;
}

/*
 * Fill in various bit for management frames, and leave them
 * unfilled for data frames (firmware takes care of that).
 * Return the selected TX rate.
 */
const struct iwx_rate *
iwx_tx_fill_cmd(struct iwx_softc *sc, struct iwx_node *in,
    struct ieee80211_frame *wh, struct iwx_tx_cmd_gen2 *tx)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	const struct iwx_rate *rinfo;
	int type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	int min_ridx = iwx_rval2ridx(ieee80211_min_basic_rate(ic));
	int ridx, rate_flags;
	uint32_t flags = 0;

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA) {
		/* for non-data, use the lowest supported rate */
		ridx = min_ridx;
	} else if (ic->ic_fixed_mcs != -1) {
		ridx = sc->sc_fixed_ridx;
	} else if (ic->ic_fixed_rate != -1) {
		ridx = sc->sc_fixed_ridx;
	} else if ((ni->ni_flags & IEEE80211_NODE_HT) && !in->ht_force_cck) {
		ridx = iwx_mcs2ridx[ni->ni_txmcs];
	} else {
		uint8_t rval;
		rval = (rs->rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL);
		ridx = iwx_rval2ridx(rval);
		if (ridx < min_ridx)
			ridx = min_ridx;
	}

	flags = (IWX_TX_FLAGS_CMD_RATE | IWX_TX_FLAGS_ENCRYPT_DIS);
	if ((ic->ic_flags & IEEE80211_F_RSNON) &&
	    ni->ni_rsn_supp_state == RSNA_SUPP_PTKNEGOTIATING)
		flags |= IWX_TX_FLAGS_HIGH_PRI;
	tx->flags = htole32(flags);

	rinfo = &iwx_rates[ridx];
	if (iwx_is_mimo_ht_plcp(rinfo->ht_plcp))
		rate_flags = IWX_RATE_MCS_ANT_AB_MSK;
	else
		rate_flags = IWX_RATE_MCS_ANT_A_MSK;
	if (IWX_RIDX_IS_CCK(ridx))
		rate_flags |= IWX_RATE_MCS_CCK_MSK;
	if ((ni->ni_flags & IEEE80211_NODE_HT) &&
	    rinfo->ht_plcp != IWX_RATE_HT_SISO_MCS_INV_PLCP) {
		rate_flags |= IWX_RATE_MCS_HT_MSK; 
		if (ieee80211_node_supports_ht_sgi20(ni))
			rate_flags |= IWX_RATE_MCS_SGI_MSK;
		tx->rate_n_flags = htole32(rate_flags | rinfo->ht_plcp);
	} else
		tx->rate_n_flags = htole32(rate_flags | rinfo->plcp);

	return rinfo;
}

#if 0
/*
 * necessary only for block ack mode
 */
void
iwx_tx_update_byte_tbl(struct iwx_tx_ring *txq, uint16_t byte_cnt,
    uint16_t num_tbs)
{
	uint8_t filled_tfd_size, num_fetch_chunks;
	uint16_t len = byte_cnt;
	uint16_t bc_ent;
	struct iwx_agn_scd_bc_tbl *scd_bc_tbl = txq->bc_tbl.vaddr;

	filled_tfd_size = offsetof(struct iwx_tfh_tfd, tbs) +
			  num_tbs * sizeof(struct iwx_tfh_tb);
	/*
	 * filled_tfd_size contains the number of filled bytes in the TFD.
	 * Dividing it by 64 will give the number of chunks to fetch
	 * to SRAM- 0 for one chunk, 1 for 2 and so on.
	 * If, for example, TFD contains only 3 TBs then 32 bytes
	 * of the TFD are used, and only one chunk of 64 bytes should
	 * be fetched
	 */
	num_fetch_chunks = howmany(filled_tfd_size, 64) - 1;

	/* Before AX210, the HW expects DW */
	len = howmany(len, 4);
	bc_ent = htole16(len | (num_fetch_chunks << 12));
	scd_bc_tbl->tfd_offset[txq->cur] = bc_ent;
}
#endif

int
iwx_tx(struct iwx_softc *sc, struct mbuf *m, struct ieee80211_node *ni, int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ni;
	struct iwx_tx_ring *ring;
	struct iwx_tx_data *data;
	struct iwx_tfh_tfd *desc;
	struct iwx_device_cmd *cmd;
	struct iwx_tx_cmd_gen2 *tx;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	const struct iwx_rate *rinfo;
	uint64_t paddr;
	u_int hdrlen;
	bus_dma_segment_t *seg;
	uint16_t num_tbs;
	uint8_t type;
	int i, totlen, err, pad;

	wh = mtod(m, struct ieee80211_frame *);
	hdrlen = ieee80211_get_hdrlen(wh);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	/*
	 * Map EDCA categories to Tx data queues.
	 *
	 * We use static data queue assignments even in DQA mode. We do not
	 * need to share Tx queues between stations because we only implement
	 * client mode; the firmware's station table contains only one entry
	 * which represents our access point.
	 *
	 * Tx aggregation will require additional queues (one queue per TID
	 * for which aggregation is enabled) but we do not implement this yet.
	 */
	ring = &sc->txq[IWX_DQA_MIN_MGMT_QUEUE + ac];
	desc = &ring->desc[ring->cur];
	memset(desc, 0, sizeof(*desc));
	data = &ring->data[ring->cur];

	cmd = &ring->cmd[ring->cur];
	cmd->hdr.code = IWX_TX_CMD;
	cmd->hdr.flags = 0;
	cmd->hdr.qid = ring->qid;
	cmd->hdr.idx = ring->cur;

	tx = (void *)cmd->data;
	memset(tx, 0, sizeof(*tx));

	rinfo = iwx_tx_fill_cmd(sc, in, wh, tx);

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct iwx_tx_radiotap_header *tap = &sc->sc_txtap;
		uint16_t chan_flags;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		chan_flags = ni->ni_chan->ic_flags;
		if (ic->ic_curmode != IEEE80211_MODE_11N)
			chan_flags &= ~IEEE80211_CHAN_HT;
		tap->wt_chan_flags = htole16(chan_flags);
		if ((ni->ni_flags & IEEE80211_NODE_HT) &&
		    !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
		    type == IEEE80211_FC0_TYPE_DATA &&
		    rinfo->ht_plcp != IWX_RATE_HT_SISO_MCS_INV_PLCP) {
			tap->wt_rate = (0x80 | rinfo->ht_plcp);
		} else
			tap->wt_rate = rinfo->rate;
		tap->wt_hwqueue = ac;
		if ((ic->ic_flags & IEEE80211_F_WEPON) &&
		    (wh->i_fc[1] & IEEE80211_FC1_PROTECTED))
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap_hdr(sc->sc_drvbpf, tap, sc->sc_txtap_len,
		    m, BPF_DIRECTION_OUT);
	}
#endif

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
                k = ieee80211_get_txkey(ic, wh, ni);
		if ((m = ieee80211_encrypt(ic, m, k)) == NULL)
			return ENOBUFS;
		/* 802.11 header may have moved. */
		wh = mtod(m, struct ieee80211_frame *);
	}
	totlen = m->m_pkthdr.len;

	if (hdrlen & 3) {
		/* First segment length must be a multiple of 4. */
		pad = 4 - (hdrlen & 3);
		tx->offload_assist |= htole16(IWX_TX_CMD_OFFLD_PAD);
	} else
		pad = 0;

	tx->len = htole16(totlen);

	/* Copy 802.11 header in TX command. */
	memcpy(((uint8_t *)tx) + sizeof(*tx), wh, hdrlen);

	/* Trim 802.11 header. */
	m_adj(m, hdrlen);

	err = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (err && err != EFBIG) {
		printf("%s: can't map mbuf (error %d)\n", DEVNAME(sc), err);
		m_freem(m);
		return err;
	}
	if (err) {
		/* Too many DMA segments, linearize mbuf. */
		if (m_defrag(m, M_DONTWAIT)) {
			m_freem(m);
			return ENOBUFS;
		}
		err = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
		    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (err) {
			printf("%s: can't map mbuf (error %d)\n", DEVNAME(sc),
			    err);
			m_freem(m);
			return err;
		}
	}
	data->m = m;
	data->in = in;
	data->txmcs = ni->ni_txmcs;
	data->txrate = ni->ni_txrate;

	/* Fill TX descriptor. */
	num_tbs = 2 + data->map->dm_nsegs;
	desc->num_tbs = htole16(num_tbs);

	desc->tbs[0].tb_len = htole16(IWX_FIRST_TB_SIZE);
	paddr = htole64(data->cmd_paddr);
	memcpy(&desc->tbs[0].addr, &paddr, sizeof(paddr));
	if (data->cmd_paddr >> 32 != (data->cmd_paddr + le32toh(desc->tbs[0].tb_len)) >> 32)
		DPRINTF(("%s: TB0 crosses 32bit boundary\n", __func__));
	desc->tbs[1].tb_len = htole16(sizeof(struct iwx_cmd_header) +
	    sizeof(*tx) + hdrlen + pad - IWX_FIRST_TB_SIZE);
	paddr = htole64(data->cmd_paddr + IWX_FIRST_TB_SIZE);
	memcpy(&desc->tbs[1].addr, &paddr, sizeof(paddr));

	if (data->cmd_paddr >> 32 != (data->cmd_paddr + le32toh(desc->tbs[1].tb_len)) >> 32)
		DPRINTF(("%s: TB1 crosses 32bit boundary\n", __func__));

	/* Other DMA segments are for data payload. */
	seg = data->map->dm_segs;
	for (i = 0; i < data->map->dm_nsegs; i++, seg++) {
		desc->tbs[i + 2].tb_len = htole16(seg->ds_len);
		paddr = htole64(seg->ds_addr);
		memcpy(&desc->tbs[i + 2].addr, &paddr, sizeof(paddr));
		if (data->cmd_paddr >> 32 != (data->cmd_paddr + le32toh(desc->tbs[i + 2].tb_len)) >> 32)
			DPRINTF(("%s: TB%d crosses 32bit boundary\n", __func__, i + 2));
	}

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, ring->cmd_dma.map,
	    (char *)(void *)cmd - (char *)(void *)ring->cmd_dma.vaddr,
	    sizeof (*cmd), BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map,
	    (char *)(void *)desc - (char *)(void *)ring->desc_dma.vaddr,
	    sizeof (*desc), BUS_DMASYNC_PREWRITE);

#if 0
	iwx_tx_update_byte_tbl(ring, totlen, num_tbs);
#endif

	/* Kick TX ring. */
	ring->cur = (ring->cur + 1) % IWX_TX_RING_COUNT;
	IWX_WRITE(sc, IWX_HBUS_TARG_WRPTR, ring->qid << 16 | ring->cur);

	/* Mark TX ring as full if we reach a certain threshold. */
	if (++ring->queued > IWX_TX_RING_HIMARK) {
		sc->qfullmsk |= 1 << ring->qid;
	}

	return 0;
}

int
iwx_flush_tx_path(struct iwx_softc *sc)
{
	struct iwx_tx_path_flush_cmd flush_cmd = {
		.sta_id = htole32(IWX_STATION_ID),
		.tid_mask = htole16(0xffff),
	};
	int err;

	err = iwx_send_cmd_pdu(sc, IWX_TXPATH_FLUSH, 0,
	    sizeof(flush_cmd), &flush_cmd);
	if (err)
                printf("%s: Flushing tx queue failed: %d\n", DEVNAME(sc), err);
	return err;
}

#define IWX_POWER_KEEP_ALIVE_PERIOD_SEC    25

int
iwx_beacon_filter_send_cmd(struct iwx_softc *sc,
    struct iwx_beacon_filter_cmd *cmd)
{
	size_t len;

	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_BEACON_FILTER_V4))
		len = sizeof(struct iwx_beacon_filter_cmd);
	else
		len = offsetof(struct iwx_beacon_filter_cmd,
		    bf_threshold_absolute_low);

	return iwx_send_cmd_pdu(sc, IWX_REPLY_BEACON_FILTERING_CMD,
	    0, len, cmd);
}

int
iwx_update_beacon_abort(struct iwx_softc *sc, struct iwx_node *in, int enable)
{
	struct iwx_beacon_filter_cmd cmd = {
		IWX_BF_CMD_CONFIG_DEFAULTS,
		.bf_enable_beacon_filter = htole32(1),
		.ba_enable_beacon_abort = htole32(enable),
	};

	if (!sc->sc_bf.bf_enabled)
		return 0;

	sc->sc_bf.ba_enabled = enable;
	return iwx_beacon_filter_send_cmd(sc, &cmd);
}

void
iwx_power_build_cmd(struct iwx_softc *sc, struct iwx_node *in,
    struct iwx_mac_power_cmd *cmd)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	int dtim_period, dtim_msec, keep_alive;

	cmd->id_and_color = htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id,
	    in->in_color));
	if (ni->ni_dtimperiod)
		dtim_period = ni->ni_dtimperiod;
	else
		dtim_period = 1;

	/*
	 * Regardless of power management state the driver must set
	 * keep alive period. FW will use it for sending keep alive NDPs
	 * immediately after association. Check that keep alive period
	 * is at least 3 * DTIM.
	 */
	dtim_msec = dtim_period * ni->ni_intval;
	keep_alive = MAX(3 * dtim_msec, 1000 * IWX_POWER_KEEP_ALIVE_PERIOD_SEC);
	keep_alive = roundup(keep_alive, 1000) / 1000;
	cmd->keep_alive_seconds = htole16(keep_alive);

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		cmd->flags = htole16(IWX_POWER_FLAGS_POWER_SAVE_ENA_MSK);
}

int
iwx_power_mac_update_mode(struct iwx_softc *sc, struct iwx_node *in)
{
	int err;
	int ba_enable;
	struct iwx_mac_power_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	iwx_power_build_cmd(sc, in, &cmd);

	err = iwx_send_cmd_pdu(sc, IWX_MAC_PM_POWER_TABLE, 0,
	    sizeof(cmd), &cmd);
	if (err != 0)
		return err;

	ba_enable = !!(cmd.flags &
	    htole16(IWX_POWER_FLAGS_POWER_MANAGEMENT_ENA_MSK));
	return iwx_update_beacon_abort(sc, in, ba_enable);
}

int
iwx_power_update_device(struct iwx_softc *sc)
{
	struct iwx_device_power_cmd cmd = { };
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		cmd.flags = htole16(IWX_DEVICE_POWER_FLAGS_POWER_SAVE_ENA_MSK);

	return iwx_send_cmd_pdu(sc,
	    IWX_POWER_TABLE_CMD, 0, sizeof(cmd), &cmd);
}

int
iwx_enable_beacon_filter(struct iwx_softc *sc, struct iwx_node *in)
{
	struct iwx_beacon_filter_cmd cmd = {
		IWX_BF_CMD_CONFIG_DEFAULTS,
		.bf_enable_beacon_filter = htole32(1),
		.ba_enable_beacon_abort = htole32(sc->sc_bf.ba_enabled),
	};
	int err;

	err = iwx_beacon_filter_send_cmd(sc, &cmd);
	if (err == 0)
		sc->sc_bf.bf_enabled = 1;

	return err;
}

int
iwx_disable_beacon_filter(struct iwx_softc *sc)
{
	struct iwx_beacon_filter_cmd cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));

	err = iwx_beacon_filter_send_cmd(sc, &cmd);
	if (err == 0)
		sc->sc_bf.bf_enabled = 0;

	return err;
}

int
iwx_add_sta_cmd(struct iwx_softc *sc, struct iwx_node *in, int update)
{
	struct iwx_add_sta_cmd add_sta_cmd;
	int err;
	uint32_t status;
	struct ieee80211com *ic = &sc->sc_ic;

	if (!update && (sc->sc_flags & IWX_FLAG_STA_ACTIVE))
		panic("STA already added");

	memset(&add_sta_cmd, 0, sizeof(add_sta_cmd));

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		add_sta_cmd.sta_id = IWX_MONITOR_STA_ID;
		add_sta_cmd.station_type = IWX_STA_GENERAL_PURPOSE;
	} else {
		add_sta_cmd.sta_id = IWX_STATION_ID;
		add_sta_cmd.station_type = IWX_STA_LINK;
	}
	add_sta_cmd.mac_id_n_color
	    = htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	if (!update) {
		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			IEEE80211_ADDR_COPY(&add_sta_cmd.addr,
			    etheranyaddr);
		else
			IEEE80211_ADDR_COPY(&add_sta_cmd.addr,
			    in->in_ni.ni_bssid);
	}
	add_sta_cmd.add_modify = update ? 1 : 0;
	add_sta_cmd.station_flags_msk
	    |= htole32(IWX_STA_FLG_FAT_EN_MSK | IWX_STA_FLG_MIMO_EN_MSK);
	add_sta_cmd.tid_disable_tx = htole16(0xffff);
	if (update)
		add_sta_cmd.modify_mask |= (IWX_STA_MODIFY_TID_DISABLE_TX);

	if (in->in_ni.ni_flags & IEEE80211_NODE_HT) {
		add_sta_cmd.station_flags_msk
		    |= htole32(IWX_STA_FLG_MAX_AGG_SIZE_MSK |
		    IWX_STA_FLG_AGG_MPDU_DENS_MSK);

		add_sta_cmd.station_flags
		    |= htole32(IWX_STA_FLG_MAX_AGG_SIZE_64K);
		switch (ic->ic_ampdu_params & IEEE80211_AMPDU_PARAM_SS) {
		case IEEE80211_AMPDU_PARAM_SS_2:
			add_sta_cmd.station_flags
			    |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_2US);
			break;
		case IEEE80211_AMPDU_PARAM_SS_4:
			add_sta_cmd.station_flags
			    |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_4US);
			break;
		case IEEE80211_AMPDU_PARAM_SS_8:
			add_sta_cmd.station_flags
			    |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_8US);
			break;
		case IEEE80211_AMPDU_PARAM_SS_16:
			add_sta_cmd.station_flags
			    |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_16US);
			break;
		default:
			break;
		}
	}

	status = IWX_ADD_STA_SUCCESS;
	err = iwx_send_cmd_pdu_status(sc, IWX_ADD_STA, sizeof(add_sta_cmd),
	    &add_sta_cmd, &status);
	if (!err && (status & IWX_ADD_STA_STATUS_MASK) != IWX_ADD_STA_SUCCESS)
		err = EIO;

	return err;
}

int
iwx_add_aux_sta(struct iwx_softc *sc)
{
	struct iwx_add_sta_cmd cmd;
	int err, qid = IWX_DQA_AUX_QUEUE;
	uint32_t status;

	memset(&cmd, 0, sizeof(cmd));
	cmd.sta_id = IWX_AUX_STA_ID;
	cmd.station_type = IWX_STA_AUX_ACTIVITY;
	cmd.mac_id_n_color =
	    htole32(IWX_FW_CMD_ID_AND_COLOR(IWX_MAC_INDEX_AUX, 0));
	cmd.tid_disable_tx = htole16(0xffff);

	status = IWX_ADD_STA_SUCCESS;
	err = iwx_send_cmd_pdu_status(sc, IWX_ADD_STA, sizeof(cmd), &cmd,
	    &status);
	if (!err && (status & IWX_ADD_STA_STATUS_MASK) != IWX_ADD_STA_SUCCESS)
		return EIO;

	return iwx_enable_txq(sc, IWX_AUX_STA_ID, qid, IWX_MGMT_TID,
	    IWX_TX_RING_COUNT);
}

int
iwx_rm_sta_cmd(struct iwx_softc *sc, struct iwx_node *in)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_rm_sta_cmd rm_sta_cmd;
	int err;

	if ((sc->sc_flags & IWX_FLAG_STA_ACTIVE) == 0)
		panic("sta already removed");

	memset(&rm_sta_cmd, 0, sizeof(rm_sta_cmd));
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		rm_sta_cmd.sta_id = IWX_MONITOR_STA_ID;
	else
		rm_sta_cmd.sta_id = IWX_STATION_ID;

	err = iwx_send_cmd_pdu(sc, IWX_REMOVE_STA, 0, sizeof(rm_sta_cmd),
	    &rm_sta_cmd);

	return err;
}

uint8_t
iwx_umac_scan_fill_channels(struct iwx_softc *sc,
    struct iwx_scan_channel_cfg_umac *chan, int n_ssids, int bgscan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c;
	uint8_t nchan;

	for (nchan = 0, c = &ic->ic_channels[1];
	    c <= &ic->ic_channels[IEEE80211_CHAN_MAX] &&
	    nchan < sc->sc_capa_n_scan_channels;
	    c++) {
		if (c->ic_flags == 0)
			continue;

		chan->channel_num = ieee80211_mhz2ieee(c->ic_freq, 0);
		chan->iter_count = 1;
		chan->iter_interval = htole16(0);
		if (n_ssids != 0 && !bgscan)
			chan->flags = htole32(1 << 0); /* select SSID 0 */
		chan++;
		nchan++;
	}

	return nchan;
}

int
iwx_fill_probe_req_v1(struct iwx_softc *sc, struct iwx_scan_probe_req_v1 *preq1)
{
	struct iwx_scan_probe_req preq2;
	int err, i;

	err = iwx_fill_probe_req(sc, &preq2);
	if (err)
		return err;

	preq1->mac_header = preq2.mac_header;
	for (i = 0; i < nitems(preq1->band_data); i++)
		preq1->band_data[i] = preq2.band_data[i];
	preq1->common_data = preq2.common_data;
	memcpy(preq1->buf, preq2.buf, sizeof(preq1->buf));
	return 0;
}

int
iwx_fill_probe_req(struct iwx_softc *sc, struct iwx_scan_probe_req *preq)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	struct ieee80211_frame *wh = (struct ieee80211_frame *)preq->buf;
	struct ieee80211_rateset *rs;
	size_t remain = sizeof(preq->buf);
	uint8_t *frm, *pos;

	memset(preq, 0, sizeof(*preq));

	if (remain < sizeof(*wh) + 2 + ic->ic_des_esslen)
		return ENOBUFS;

	/*
	 * Build a probe request frame.  Most of the following code is a
	 * copy & paste of what is done in net80211.
	 */
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	IEEE80211_ADDR_COPY(wh->i_addr1, etherbroadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, etherbroadcastaddr);
	*(uint16_t *)&wh->i_dur[0] = 0;	/* filled by HW */
	*(uint16_t *)&wh->i_seq[0] = 0;	/* filled by HW */

	frm = (uint8_t *)(wh + 1);
	frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);

	/* Tell the firmware where the MAC header is. */
	preq->mac_header.offset = 0;
	preq->mac_header.len = htole16(frm - (uint8_t *)wh);
	remain -= frm - (uint8_t *)wh;

	/* Fill in 2GHz IEs and tell firmware where they are. */
	rs = &ic->ic_sup_rates[IEEE80211_MODE_11G];
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		if (remain < 4 + rs->rs_nrates)
			return ENOBUFS;
	} else if (remain < 2 + rs->rs_nrates)
		return ENOBUFS;
	preq->band_data[0].offset = htole16(frm - (uint8_t *)wh);
	pos = frm;
	frm = ieee80211_add_rates(frm, rs);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);
	preq->band_data[0].len = htole16(frm - pos);
	remain -= frm - pos;

	if (isset(sc->sc_enabled_capa, 
	    IWX_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT)) {
		if (remain < 3)
			return ENOBUFS;
		*frm++ = IEEE80211_ELEMID_DSPARMS;
		*frm++ = 1;
		*frm++ = 0;
		remain -= 3;
	}

	if (sc->sc_nvm.sku_cap_band_52GHz_enable) {
		/* Fill in 5GHz IEs. */
		rs = &ic->ic_sup_rates[IEEE80211_MODE_11A];
		if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
			if (remain < 4 + rs->rs_nrates)
				return ENOBUFS;
		} else if (remain < 2 + rs->rs_nrates)
			return ENOBUFS;
		preq->band_data[1].offset = htole16(frm - (uint8_t *)wh);
		pos = frm;
		frm = ieee80211_add_rates(frm, rs);
		if (rs->rs_nrates > IEEE80211_RATE_SIZE)
			frm = ieee80211_add_xrates(frm, rs);
		preq->band_data[1].len = htole16(frm - pos);
		remain -= frm - pos;
	}

	/* Send 11n IEs on both 2GHz and 5GHz bands. */
	preq->common_data.offset = htole16(frm - (uint8_t *)wh);
	pos = frm;
	if (ic->ic_flags & IEEE80211_F_HTON) {
		if (remain < 28)
			return ENOBUFS;
		frm = ieee80211_add_htcaps(frm, ic);
		/* XXX add WME info? */
	}
	preq->common_data.len = htole16(frm - pos);

	return 0;
}

int
iwx_config_umac_scan(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_scan_config *scan_config;
	int err, nchan;
	size_t cmd_size;
	struct ieee80211_channel *c;
	struct iwx_host_cmd hcmd = {
		.id = iwx_cmd_id(IWX_SCAN_CFG_CMD, IWX_LONG_GROUP, 0),
		.flags = 0,
	};
	static const uint32_t rates = (IWX_SCAN_CONFIG_RATE_1M |
	    IWX_SCAN_CONFIG_RATE_2M | IWX_SCAN_CONFIG_RATE_5M |
	    IWX_SCAN_CONFIG_RATE_11M | IWX_SCAN_CONFIG_RATE_6M |
	    IWX_SCAN_CONFIG_RATE_9M | IWX_SCAN_CONFIG_RATE_12M |
	    IWX_SCAN_CONFIG_RATE_18M | IWX_SCAN_CONFIG_RATE_24M |
	    IWX_SCAN_CONFIG_RATE_36M | IWX_SCAN_CONFIG_RATE_48M |
	    IWX_SCAN_CONFIG_RATE_54M);

	cmd_size = sizeof(*scan_config) + sc->sc_capa_n_scan_channels;

	scan_config = malloc(cmd_size, M_DEVBUF, M_WAIT | M_CANFAIL | M_ZERO);
	if (scan_config == NULL)
		return ENOMEM;

	scan_config->tx_chains = htole32(iwx_fw_valid_tx_ant(sc));
	scan_config->rx_chains = htole32(iwx_fw_valid_rx_ant(sc));
	scan_config->legacy_rates = htole32(rates |
	    IWX_SCAN_CONFIG_SUPPORTED_RATE(rates));

	/* These timings correspond to iwlwifi's UNASSOC scan. */
	scan_config->dwell.active = 10;
	scan_config->dwell.passive = 110;
	scan_config->dwell.fragmented = 44;
	scan_config->dwell.extended = 90;
	scan_config->out_of_channel_time[IWX_SCAN_LB_LMAC_IDX] = htole32(0);
	scan_config->out_of_channel_time[IWX_SCAN_HB_LMAC_IDX] = htole32(0);
	scan_config->suspend_time[IWX_SCAN_LB_LMAC_IDX] = htole32(0);
	scan_config->suspend_time[IWX_SCAN_HB_LMAC_IDX] = htole32(0);

	IEEE80211_ADDR_COPY(scan_config->mac_addr, sc->sc_ic.ic_myaddr);

	scan_config->bcast_sta_id = IWX_AUX_STA_ID;
	scan_config->channel_flags = 0;

	for (c = &ic->ic_channels[1], nchan = 0;
	    c <= &ic->ic_channels[IEEE80211_CHAN_MAX] &&
	    nchan < sc->sc_capa_n_scan_channels; c++) {
		if (c->ic_flags == 0)
			continue;
		scan_config->channel_array[nchan++] =
		    ieee80211_mhz2ieee(c->ic_freq, 0);
	}

	scan_config->flags = htole32(IWX_SCAN_CONFIG_FLAG_ACTIVATE |
	    IWX_SCAN_CONFIG_FLAG_ALLOW_CHUB_REQS |
	    IWX_SCAN_CONFIG_FLAG_SET_TX_CHAINS |
	    IWX_SCAN_CONFIG_FLAG_SET_RX_CHAINS |
	    IWX_SCAN_CONFIG_FLAG_SET_AUX_STA_ID |
	    IWX_SCAN_CONFIG_FLAG_SET_ALL_TIMES |
	    IWX_SCAN_CONFIG_FLAG_SET_LEGACY_RATES |
	    IWX_SCAN_CONFIG_FLAG_SET_MAC_ADDR |
	    IWX_SCAN_CONFIG_FLAG_SET_CHANNEL_FLAGS|
	    IWX_SCAN_CONFIG_N_CHANNELS(nchan) |
	    IWX_SCAN_CONFIG_FLAG_CLEAR_FRAGMENTED);

	hcmd.data[0] = scan_config;
	hcmd.len[0] = cmd_size;

	err = iwx_send_cmd(sc, &hcmd);
	free(scan_config, M_DEVBUF, cmd_size);
	return err;
}

int
iwx_umac_scan_size(struct iwx_softc *sc)
{
	int base_size = IWX_SCAN_REQ_UMAC_SIZE_V1;
	int tail_size;

	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2))
		base_size = IWX_SCAN_REQ_UMAC_SIZE_V8;
	else if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL))
		base_size = IWX_SCAN_REQ_UMAC_SIZE_V7;
#ifdef notyet
	else if (sc->sc_device_family >= IWX_DEVICE_FAMILY_22000)
		base_size = IWX_SCAN_REQ_UMAC_SIZE_V6;
#endif
	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_SCAN_EXT_CHAN_VER))
		tail_size = sizeof(struct iwx_scan_req_umac_tail_v2);
	else
		tail_size = sizeof(struct iwx_scan_req_umac_tail_v1);

	return base_size + sizeof(struct iwx_scan_channel_cfg_umac) *
	    sc->sc_capa_n_scan_channels + tail_size;
}

struct iwx_scan_umac_chan_param *
iwx_get_scan_req_umac_chan_param(struct iwx_softc *sc,
    struct iwx_scan_req_umac *req)
{
	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2))
		return &req->v8.channel;

	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL))
		return &req->v7.channel;
#ifdef notyet
	if (sc->sc_device_family >= IWX_DEVICE_FAMILY_22000)
		return &req->v6.channel;
#endif
	return &req->v1.channel;
}

void *
iwx_get_scan_req_umac_data(struct iwx_softc *sc, struct iwx_scan_req_umac *req)
{
	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2))
		return (void *)&req->v8.data;

	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL))
		return (void *)&req->v7.data;
#ifdef notyet
	if (sc->sc_device_family >= IWX_DEVICE_FAMILY_22000)
		return (void *)&req->v6.data;
#endif
	return (void *)&req->v1.data;

}

/* adaptive dwell max budget time [TU] for full scan */
#define IWX_SCAN_ADWELL_MAX_BUDGET_FULL_SCAN 300
/* adaptive dwell max budget time [TU] for directed scan */
#define IWX_SCAN_ADWELL_MAX_BUDGET_DIRECTED_SCAN 100
/* adaptive dwell default high band APs number */
#define IWX_SCAN_ADWELL_DEFAULT_HB_N_APS 8
/* adaptive dwell default low band APs number */
#define IWX_SCAN_ADWELL_DEFAULT_LB_N_APS 2
/* adaptive dwell default APs number in social channels (1, 6, 11) */
#define IWX_SCAN_ADWELL_DEFAULT_N_APS_SOCIAL 10

int
iwx_umac_scan(struct iwx_softc *sc, int bgscan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_host_cmd hcmd = {
		.id = iwx_cmd_id(IWX_SCAN_REQ_UMAC, IWX_LONG_GROUP, 0),
		.len = { 0, },
		.data = { NULL, },
		.flags = 0,
	};
	struct iwx_scan_req_umac *req;
	void *cmd_data, *tail_data;
	struct iwx_scan_req_umac_tail_v2 *tail;
	struct iwx_scan_req_umac_tail_v1 *tailv1;
	struct iwx_scan_umac_chan_param *chanparam;
	size_t req_len;
	int err, async = bgscan;

	req_len = iwx_umac_scan_size(sc);
	if ((req_len < IWX_SCAN_REQ_UMAC_SIZE_V1 +
	    sizeof(struct iwx_scan_req_umac_tail_v1)) ||
	    req_len > IWX_MAX_CMD_PAYLOAD_SIZE)
		return ERANGE;
	req = malloc(req_len, M_DEVBUF,
	    (async ? M_NOWAIT : M_WAIT) | M_CANFAIL | M_ZERO);
	if (req == NULL)
		return ENOMEM;

	hcmd.len[0] = (uint16_t)req_len;
	hcmd.data[0] = (void *)req;
	hcmd.flags |= async ? IWX_CMD_ASYNC : 0;

	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL)) {
		req->v7.adwell_default_n_aps_social =
			IWX_SCAN_ADWELL_DEFAULT_N_APS_SOCIAL;
		req->v7.adwell_default_n_aps =
			IWX_SCAN_ADWELL_DEFAULT_LB_N_APS;

		if (ic->ic_des_esslen != 0)
			req->v7.adwell_max_budget =
			    htole16(IWX_SCAN_ADWELL_MAX_BUDGET_DIRECTED_SCAN);
		else
			req->v7.adwell_max_budget =
			    htole16(IWX_SCAN_ADWELL_MAX_BUDGET_FULL_SCAN);

		req->v7.scan_priority = htole32(IWX_SCAN_PRIORITY_HIGH);
		req->v7.max_out_time[IWX_SCAN_LB_LMAC_IDX] = 0;
		req->v7.suspend_time[IWX_SCAN_LB_LMAC_IDX] = 0;

		if (isset(sc->sc_ucode_api,
		    IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2)) {
			req->v8.active_dwell[IWX_SCAN_LB_LMAC_IDX] = 10;
			req->v8.passive_dwell[IWX_SCAN_LB_LMAC_IDX] = 110;
		} else {
			req->v7.active_dwell = 10;
			req->v7.passive_dwell = 110;
			req->v7.fragmented_dwell = 44;
		}
	} else {
		/* These timings correspond to iwlwifi's UNASSOC scan. */
		req->v1.active_dwell = 10;
		req->v1.passive_dwell = 110;
		req->v1.fragmented_dwell = 44;
		req->v1.extended_dwell = 90;
	}

	if (bgscan) {
		const uint32_t timeout = htole32(120);
		if (isset(sc->sc_ucode_api,
		    IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2)) {
			req->v8.max_out_time[IWX_SCAN_LB_LMAC_IDX] = timeout;
			req->v8.suspend_time[IWX_SCAN_LB_LMAC_IDX] = timeout;
		} else if (isset(sc->sc_ucode_api,
		    IWX_UCODE_TLV_API_ADAPTIVE_DWELL)) {
			req->v7.max_out_time[IWX_SCAN_LB_LMAC_IDX] = timeout;
			req->v7.suspend_time[IWX_SCAN_LB_LMAC_IDX] = timeout;
		} else {
			req->v1.max_out_time = timeout;
			req->v1.suspend_time = timeout;
		}
	}

	req->v1.scan_priority = htole32(IWX_SCAN_PRIORITY_HIGH);
	req->ooc_priority = htole32(IWX_SCAN_PRIORITY_HIGH);

	cmd_data = iwx_get_scan_req_umac_data(sc, req);
	chanparam = iwx_get_scan_req_umac_chan_param(sc, req);
	chanparam->count = iwx_umac_scan_fill_channels(sc,
	    (struct iwx_scan_channel_cfg_umac *)cmd_data,
	    ic->ic_des_esslen != 0, bgscan);
	chanparam->flags = 0;

	tail_data = cmd_data + sizeof(struct iwx_scan_channel_cfg_umac) *
	    sc->sc_capa_n_scan_channels;
	tail = tail_data;
	/* tail v1 layout differs in preq and direct_scan member fields. */
	tailv1 = tail_data;

	req->general_flags = htole32(IWX_UMAC_SCAN_GEN_FLAGS_PASS_ALL |
	    IWX_UMAC_SCAN_GEN_FLAGS_ITER_COMPLETE);
	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2)) {
		req->v8.general_flags2 =
			IWX_UMAC_SCAN_GEN_FLAGS2_ALLOW_CHNL_REORDER;
	}

#if 0 /* XXX Active scan causes firmware errors after association. */
	/* Check if we're doing an active directed scan. */
	if (ic->ic_des_esslen != 0) {
		if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_SCAN_EXT_CHAN_VER)) {
			tail->direct_scan[0].id = IEEE80211_ELEMID_SSID;
			tail->direct_scan[0].len = ic->ic_des_esslen;
			memcpy(tail->direct_scan[0].ssid, ic->ic_des_essid,
			    ic->ic_des_esslen);
		} else {
			tailv1->direct_scan[0].id = IEEE80211_ELEMID_SSID;
			tailv1->direct_scan[0].len = ic->ic_des_esslen;
			memcpy(tailv1->direct_scan[0].ssid, ic->ic_des_essid,
			    ic->ic_des_esslen);
		}
		req->general_flags |=
		    htole32(IWX_UMAC_SCAN_GEN_FLAGS_PRE_CONNECT);
	} else
#endif
		req->general_flags |= htole32(IWX_UMAC_SCAN_GEN_FLAGS_PASSIVE);

	if (isset(sc->sc_enabled_capa, 
	    IWX_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT))
		req->general_flags |=
		    htole32(IWX_UMAC_SCAN_GEN_FLAGS_RRM_ENABLED);

	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL)) {
		req->general_flags |=
		    htole32(IWX_UMAC_SCAN_GEN_FLAGS_ADAPTIVE_DWELL);
	} else {
		req->general_flags |=
		    htole32(IWX_UMAC_SCAN_GEN_FLAGS_EXTENDED_DWELL);
	}

	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_SCAN_EXT_CHAN_VER))
		err = iwx_fill_probe_req(sc, &tail->preq);
	else
		err = iwx_fill_probe_req_v1(sc, &tailv1->preq);
	if (err) {
		free(req, M_DEVBUF, req_len);
		return err;
	}

	/* Specify the scan plan: We'll do one iteration. */
	tail->schedule[0].interval = 0;
	tail->schedule[0].iter_count = 1;

	err = iwx_send_cmd(sc, &hcmd);
	free(req, M_DEVBUF, req_len);
	return err;
}

uint8_t
iwx_ridx2rate(struct ieee80211_rateset *rs, int ridx)
{
	int i;
	uint8_t rval;

	for (i = 0; i < rs->rs_nrates; i++) {
		rval = (rs->rs_rates[i] & IEEE80211_RATE_VAL);
		if (rval == iwx_rates[ridx].rate)
			return rs->rs_rates[i];
	}

	return 0;
}

int
iwx_rval2ridx(int rval)
{
	int ridx;

	for (ridx = 0; ridx < nitems(iwx_rates); ridx++) {
		if (iwx_rates[ridx].plcp == IWX_RATE_INVM_PLCP)
			continue;
		if (rval == iwx_rates[ridx].rate)
			break;
	}

       return ridx;
}

void
iwx_ack_rates(struct iwx_softc *sc, struct iwx_node *in, int *cck_rates,
    int *ofdm_rates)
{
	struct ieee80211_node *ni = &in->in_ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	int lowest_present_ofdm = -1;
	int lowest_present_cck = -1;
	uint8_t cck = 0;
	uint8_t ofdm = 0;
	int i;

	if (ni->ni_chan == IEEE80211_CHAN_ANYC ||
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		for (i = IWX_FIRST_CCK_RATE; i < IWX_FIRST_OFDM_RATE; i++) {
			if ((iwx_ridx2rate(rs, i) & IEEE80211_RATE_BASIC) == 0)
				continue;
			cck |= (1 << i);
			if (lowest_present_cck == -1 || lowest_present_cck > i)
				lowest_present_cck = i;
		}
	}
	for (i = IWX_FIRST_OFDM_RATE; i <= IWX_LAST_NON_HT_RATE; i++) {
		if ((iwx_ridx2rate(rs, i) & IEEE80211_RATE_BASIC) == 0)
			continue;	
		ofdm |= (1 << (i - IWX_FIRST_OFDM_RATE));
		if (lowest_present_ofdm == -1 || lowest_present_ofdm > i)
			lowest_present_ofdm = i;
	}

	/*
	 * Now we've got the basic rates as bitmaps in the ofdm and cck
	 * variables. This isn't sufficient though, as there might not
	 * be all the right rates in the bitmap. E.g. if the only basic
	 * rates are 5.5 Mbps and 11 Mbps, we still need to add 1 Mbps
	 * and 6 Mbps because the 802.11-2007 standard says in 9.6:
	 *
	 *    [...] a STA responding to a received frame shall transmit
	 *    its Control Response frame [...] at the highest rate in the
	 *    BSSBasicRateSet parameter that is less than or equal to the
	 *    rate of the immediately previous frame in the frame exchange
	 *    sequence ([...]) and that is of the same modulation class
	 *    ([...]) as the received frame. If no rate contained in the
	 *    BSSBasicRateSet parameter meets these conditions, then the
	 *    control frame sent in response to a received frame shall be
	 *    transmitted at the highest mandatory rate of the PHY that is
	 *    less than or equal to the rate of the received frame, and
	 *    that is of the same modulation class as the received frame.
	 *
	 * As a consequence, we need to add all mandatory rates that are
	 * lower than all of the basic rates to these bitmaps.
	 */

	if (IWX_RATE_24M_INDEX < lowest_present_ofdm)
		ofdm |= IWX_RATE_BIT_MSK(24) >> IWX_FIRST_OFDM_RATE;
	if (IWX_RATE_12M_INDEX < lowest_present_ofdm)
		ofdm |= IWX_RATE_BIT_MSK(12) >> IWX_FIRST_OFDM_RATE;
	/* 6M already there or needed so always add */
	ofdm |= IWX_RATE_BIT_MSK(6) >> IWX_FIRST_OFDM_RATE;

	/*
	 * CCK is a bit more complex with DSSS vs. HR/DSSS vs. ERP.
	 * Note, however:
	 *  - if no CCK rates are basic, it must be ERP since there must
	 *    be some basic rates at all, so they're OFDM => ERP PHY
	 *    (or we're in 5 GHz, and the cck bitmap will never be used)
	 *  - if 11M is a basic rate, it must be ERP as well, so add 5.5M
	 *  - if 5.5M is basic, 1M and 2M are mandatory
	 *  - if 2M is basic, 1M is mandatory
	 *  - if 1M is basic, that's the only valid ACK rate.
	 * As a consequence, it's not as complicated as it sounds, just add
	 * any lower rates to the ACK rate bitmap.
	 */
	if (IWX_RATE_11M_INDEX < lowest_present_cck)
		cck |= IWX_RATE_BIT_MSK(11) >> IWX_FIRST_CCK_RATE;
	if (IWX_RATE_5M_INDEX < lowest_present_cck)
		cck |= IWX_RATE_BIT_MSK(5) >> IWX_FIRST_CCK_RATE;
	if (IWX_RATE_2M_INDEX < lowest_present_cck)
		cck |= IWX_RATE_BIT_MSK(2) >> IWX_FIRST_CCK_RATE;
	/* 1M already there or needed so always add */
	cck |= IWX_RATE_BIT_MSK(1) >> IWX_FIRST_CCK_RATE;

	*cck_rates = cck;
	*ofdm_rates = ofdm;
}

void
iwx_mac_ctxt_cmd_common(struct iwx_softc *sc, struct iwx_node *in,
    struct iwx_mac_ctx_cmd *cmd, uint32_t action)
{
#define IWX_EXP2(x)	((1 << (x)) - 1)	/* CWmin = 2^ECWmin - 1 */
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	int cck_ack_rates, ofdm_ack_rates;
	int i;

	cmd->id_and_color = htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id,
	    in->in_color));
	cmd->action = htole32(action);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		cmd->mac_type = htole32(IWX_FW_MAC_TYPE_LISTENER);
	else if (ic->ic_opmode == IEEE80211_M_STA)
		cmd->mac_type = htole32(IWX_FW_MAC_TYPE_BSS_STA);
	else
		panic("unsupported operating mode %d\n", ic->ic_opmode);
	cmd->tsf_id = htole32(IWX_TSF_ID_A);

	IEEE80211_ADDR_COPY(cmd->node_addr, ic->ic_myaddr);
	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		IEEE80211_ADDR_COPY(cmd->bssid_addr, etherbroadcastaddr);
		return;
	}

	IEEE80211_ADDR_COPY(cmd->bssid_addr, ni->ni_bssid);
	iwx_ack_rates(sc, in, &cck_ack_rates, &ofdm_ack_rates);
	cmd->cck_rates = htole32(cck_ack_rates);
	cmd->ofdm_rates = htole32(ofdm_ack_rates);

	cmd->cck_short_preamble
	    = htole32((ic->ic_flags & IEEE80211_F_SHPREAMBLE)
	      ? IWX_MAC_FLG_SHORT_PREAMBLE : 0);
	cmd->short_slot
	    = htole32((ic->ic_flags & IEEE80211_F_SHSLOT)
	      ? IWX_MAC_FLG_SHORT_SLOT : 0);

	for (i = 0; i < EDCA_NUM_AC; i++) {
		struct ieee80211_edca_ac_params *ac = &ic->ic_edca_ac[i];
		int txf = iwx_ac_to_tx_fifo[i];

		cmd->ac[txf].cw_min = htole16(IWX_EXP2(ac->ac_ecwmin));
		cmd->ac[txf].cw_max = htole16(IWX_EXP2(ac->ac_ecwmax));
		cmd->ac[txf].aifsn = ac->ac_aifsn;
		cmd->ac[txf].fifos_mask = (1 << txf);
		cmd->ac[txf].edca_txop = htole16(ac->ac_txoplimit * 32);
	}
	if (ni->ni_flags & IEEE80211_NODE_QOS)
		cmd->qos_flags |= htole32(IWX_MAC_QOS_FLG_UPDATE_EDCA);

	if (ni->ni_flags & IEEE80211_NODE_HT) {
		enum ieee80211_htprot htprot =
		    (ni->ni_htop1 & IEEE80211_HTOP1_PROT_MASK);
		switch (htprot) {
		case IEEE80211_HTPROT_NONE:
			break;
		case IEEE80211_HTPROT_NONMEMBER:
		case IEEE80211_HTPROT_NONHT_MIXED:
			cmd->protection_flags |=
			    htole32(IWX_MAC_PROT_FLG_HT_PROT);
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				cmd->protection_flags |=
				    htole32(IWX_MAC_PROT_FLG_SELF_CTS_EN);
			break;
		case IEEE80211_HTPROT_20MHZ:
			if (ic->ic_htcaps & IEEE80211_HTCAP_CBW20_40) {
				/* XXX ... and if our channel is 40 MHz ... */
				cmd->protection_flags |=
				    htole32(IWX_MAC_PROT_FLG_HT_PROT |
				    IWX_MAC_PROT_FLG_FAT_PROT);
				if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
					cmd->protection_flags |= htole32(
					    IWX_MAC_PROT_FLG_SELF_CTS_EN);
			}
			break;
		default:
			break;
		}

		cmd->qos_flags |= htole32(IWX_MAC_QOS_FLG_TGN);
	}
	if (ic->ic_flags & IEEE80211_F_USEPROT)
		cmd->protection_flags |= htole32(IWX_MAC_PROT_FLG_TGG_PROTECT);

	cmd->filter_flags = htole32(IWX_MAC_FILTER_ACCEPT_GRP);
#undef IWX_EXP2
}

void
iwx_mac_ctxt_cmd_fill_sta(struct iwx_softc *sc, struct iwx_node *in,
    struct iwx_mac_data_sta *sta, int assoc)
{
	struct ieee80211_node *ni = &in->in_ni;
	uint32_t dtim_off;
	uint64_t tsf;

	dtim_off = ni->ni_dtimcount * ni->ni_intval * IEEE80211_DUR_TU;
	memcpy(&tsf, ni->ni_tstamp, sizeof(tsf));
	tsf = letoh64(tsf);

	sta->is_assoc = htole32(assoc);
	sta->dtim_time = htole32(ni->ni_rstamp + dtim_off);
	sta->dtim_tsf = htole64(tsf + dtim_off);
	sta->bi = htole32(ni->ni_intval);
	sta->bi_reciprocal = htole32(iwx_reciprocal(ni->ni_intval));
	sta->dtim_interval = htole32(ni->ni_intval * ni->ni_dtimperiod);
	sta->dtim_reciprocal = htole32(iwx_reciprocal(sta->dtim_interval));
	sta->listen_interval = htole32(10);
	sta->assoc_id = htole32(ni->ni_associd);
	sta->assoc_beacon_arrive_time = htole32(ni->ni_rstamp);
}

int
iwx_mac_ctxt_cmd(struct iwx_softc *sc, struct iwx_node *in, uint32_t action,
    int assoc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	struct iwx_mac_ctx_cmd cmd;
	int active = (sc->sc_flags & IWX_FLAG_MAC_ACTIVE);

	if (action == IWX_FW_CTXT_ACTION_ADD && active)
		panic("MAC already added");
	if (action == IWX_FW_CTXT_ACTION_REMOVE && !active)
		panic("MAC already removed");

	memset(&cmd, 0, sizeof(cmd));

	iwx_mac_ctxt_cmd_common(sc, in, &cmd, action);

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		cmd.filter_flags |= htole32(IWX_MAC_FILTER_IN_PROMISC |
		    IWX_MAC_FILTER_IN_CONTROL_AND_MGMT |
		    IWX_MAC_FILTER_ACCEPT_GRP |
		    IWX_MAC_FILTER_IN_BEACON |
		    IWX_MAC_FILTER_IN_PROBE_REQUEST |
		    IWX_MAC_FILTER_IN_CRC32);
	} else if (!assoc || !ni->ni_associd || !ni->ni_dtimperiod)
		/* 
		 * Allow beacons to pass through as long as we are not
		 * associated or we do not have dtim period information.
		 */
		cmd.filter_flags |= htole32(IWX_MAC_FILTER_IN_BEACON);
	else
		iwx_mac_ctxt_cmd_fill_sta(sc, in, &cmd.sta, assoc);

	return iwx_send_cmd_pdu(sc, IWX_MAC_CONTEXT_CMD, 0, sizeof(cmd), &cmd);
}

int
iwx_clear_statistics(struct iwx_softc *sc)
{
	struct iwx_statistics_cmd scmd = {
		.flags = htole32(IWX_STATISTICS_FLG_CLEAR)
	};
	struct iwx_host_cmd cmd = {
		.id = IWX_STATISTICS_CMD,
		.len[0] = sizeof(scmd),
		.data[0] = &scmd,
		.flags = IWX_CMD_WANT_RESP,
		.resp_pkt_len = sizeof(struct iwx_notif_statistics),
	};
	int err;

	err = iwx_send_cmd(sc, &cmd);
	if (err)
		return err;

	iwx_free_resp(sc, &cmd);
	return 0;
}

int
iwx_update_quotas(struct iwx_softc *sc, struct iwx_node *in, int running)
{
	struct iwx_time_quota_cmd cmd;
	int i, idx, num_active_macs, quota, quota_rem;
	int colors[IWX_MAX_BINDINGS] = { -1, -1, -1, -1, };
	int n_ifs[IWX_MAX_BINDINGS] = {0, };
	uint16_t id;

	memset(&cmd, 0, sizeof(cmd));

	/* currently, PHY ID == binding ID */
	if (in && in->in_phyctxt) {
		id = in->in_phyctxt->id;
		KASSERT(id < IWX_MAX_BINDINGS);
		colors[id] = in->in_phyctxt->color;
		if (running)
			n_ifs[id] = 1;
	}

	/*
	 * The FW's scheduling session consists of
	 * IWX_MAX_QUOTA fragments. Divide these fragments
	 * equally between all the bindings that require quota
	 */
	num_active_macs = 0;
	for (i = 0; i < IWX_MAX_BINDINGS; i++) {
		cmd.quotas[i].id_and_color = htole32(IWX_FW_CTXT_INVALID);
		num_active_macs += n_ifs[i];
	}

	quota = 0;
	quota_rem = 0;
	if (num_active_macs) {
		quota = IWX_MAX_QUOTA / num_active_macs;
		quota_rem = IWX_MAX_QUOTA % num_active_macs;
	}

	for (idx = 0, i = 0; i < IWX_MAX_BINDINGS; i++) {
		if (colors[i] < 0)
			continue;

		cmd.quotas[idx].id_and_color =
			htole32(IWX_FW_CMD_ID_AND_COLOR(i, colors[i]));

		if (n_ifs[i] <= 0) {
			cmd.quotas[idx].quota = htole32(0);
			cmd.quotas[idx].max_duration = htole32(0);
		} else {
			cmd.quotas[idx].quota = htole32(quota * n_ifs[i]);
			cmd.quotas[idx].max_duration = htole32(0);
		}
		idx++;
	}

	/* Give the remainder of the session to the first binding */
	cmd.quotas[0].quota = htole32(le32toh(cmd.quotas[0].quota) + quota_rem);

	return iwx_send_cmd_pdu(sc, IWX_TIME_QUOTA_CMD, 0,
	    sizeof(cmd), &cmd);
}

void
iwx_add_task(struct iwx_softc *sc, struct taskq *taskq, struct task *task)
{
	int s = splnet();

	if (sc->sc_flags & IWX_FLAG_SHUTDOWN) {
		splx(s);
		return;
	}

	refcnt_take(&sc->task_refs);
	if (!task_add(taskq, task))
		refcnt_rele_wake(&sc->task_refs);
	splx(s);
}

void
iwx_del_task(struct iwx_softc *sc, struct taskq *taskq, struct task *task)
{
	if (task_del(taskq, task))
		refcnt_rele(&sc->task_refs);
}

int
iwx_scan(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	int err;

	if (sc->sc_flags & IWX_FLAG_BGSCAN) {
		err = iwx_scan_abort(sc);
		if (err) {
			printf("%s: could not abort background scan\n",
			    DEVNAME(sc));
			return err;
		}
	}

	err = iwx_umac_scan(sc, 0);
	if (err) {
		printf("%s: could not initiate scan\n", DEVNAME(sc));
		return err;
	}

	/*
	 * The current mode might have been fixed during association.
	 * Ensure all channels get scanned.
	 */
	if (IFM_MODE(ic->ic_media.ifm_cur->ifm_media) == IFM_AUTO)
		ieee80211_setmode(ic, IEEE80211_MODE_AUTO);

	sc->sc_flags |= IWX_FLAG_SCANNING;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: %s -> %s\n", ifp->if_xname,
		    ieee80211_state_name[ic->ic_state],
		    ieee80211_state_name[IEEE80211_S_SCAN]);
	if ((sc->sc_flags & IWX_FLAG_BGSCAN) == 0) {
		ieee80211_set_link_state(ic, LINK_STATE_DOWN);
		ieee80211_node_cleanup(ic, ic->ic_bss);
	}
	ic->ic_state = IEEE80211_S_SCAN;
	wakeup(&ic->ic_state); /* wake iwx_init() */

	return 0;
}

int
iwx_bgscan(struct ieee80211com *ic) 
{
	struct iwx_softc *sc = IC2IFP(ic)->if_softc;
	int err;

	if (sc->sc_flags & IWX_FLAG_SCANNING)
		return 0;

	err = iwx_umac_scan(sc, 1);
	if (err) {
		printf("%s: could not initiate scan\n", DEVNAME(sc));
		return err;
	}

	sc->sc_flags |= IWX_FLAG_BGSCAN;
	return 0;
}

int
iwx_umac_scan_abort(struct iwx_softc *sc)
{
	struct iwx_umac_scan_abort cmd = { 0 };

	return iwx_send_cmd_pdu(sc,
	    IWX_WIDE_ID(IWX_LONG_GROUP, IWX_SCAN_ABORT_UMAC),
	    0, sizeof(cmd), &cmd);
}

int
iwx_scan_abort(struct iwx_softc *sc)
{
	int err;

	err = iwx_umac_scan_abort(sc);
	if (err == 0)
		sc->sc_flags &= ~(IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN);
	return err;
}

int
iwx_enable_data_tx_queues(struct iwx_softc *sc)
{
	int err, ac;

	for (ac = 0; ac < EDCA_NUM_AC; ac++) {
		int qid = ac + IWX_DQA_AUX_QUEUE + 1;
		/*
		 * Regular data frames use the "MGMT" TID and queue.
		 * Other TIDs and queues are reserved for frame aggregation.
		 */
		err = iwx_enable_txq(sc, IWX_STATION_ID, qid, IWX_MGMT_TID,
		    IWX_TX_RING_COUNT);
		if (err) {
			printf("%s: could not enable Tx queue %d (error %d)\n",
			    DEVNAME(sc), ac, err);
			return err;
		}
	}

	return 0;
}

int
iwx_auth(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	uint32_t duration;
	int generation = sc->sc_generation, err;

	splassert(IPL_NET);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		sc->sc_phyctxt[0].channel = ic->ic_ibss_chan;
	else
		sc->sc_phyctxt[0].channel = in->in_ni.ni_chan;
	err = iwx_phy_ctxt_cmd(sc, &sc->sc_phyctxt[0], 1, 1,
	    IWX_FW_CTXT_ACTION_MODIFY, 0);
	if (err) {
		printf("%s: could not update PHY context (error %d)\n",
		    DEVNAME(sc), err);
		return err;
	}
	in->in_phyctxt = &sc->sc_phyctxt[0];

	err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_ADD, 0);
	if (err) {
		printf("%s: could not add MAC context (error %d)\n",
		    DEVNAME(sc), err);
		return err;
 	}
	sc->sc_flags |= IWX_FLAG_MAC_ACTIVE;

	err = iwx_binding_cmd(sc, in, IWX_FW_CTXT_ACTION_ADD);
	if (err) {
		printf("%s: could not add binding (error %d)\n",
		    DEVNAME(sc), err);
		goto rm_mac_ctxt;
	}
	sc->sc_flags |= IWX_FLAG_BINDING_ACTIVE;

	err = iwx_add_sta_cmd(sc, in, 0);
	if (err) {
		printf("%s: could not add sta (error %d)\n",
		    DEVNAME(sc), err);
		goto rm_binding;
	}
	sc->sc_flags |= IWX_FLAG_STA_ACTIVE;

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		err = iwx_enable_txq(sc, IWX_MONITOR_STA_ID,
		    IWX_DQA_INJECT_MONITOR_QUEUE, IWX_MGMT_TID,
		    IWX_TX_RING_COUNT);
		if (err)
			goto rm_sta;
		return 0;
	}

	err = iwx_enable_data_tx_queues(sc);
	if (err)
		goto rm_sta;

	err = iwx_clear_statistics(sc);
	if (err)
		goto rm_sta;

	/*
	 * Prevent the FW from wandering off channel during association
	 * by "protecting" the session with a time event.
	 */
	if (in->in_ni.ni_intval)
		duration = in->in_ni.ni_intval * 2;
	else
		duration = IEEE80211_DUR_TU; 
	iwx_protect_session(sc, in, duration, in->in_ni.ni_intval / 2);

	return 0;

rm_sta:
	if (generation == sc->sc_generation) {
		iwx_rm_sta_cmd(sc, in);
		sc->sc_flags &= ~IWX_FLAG_STA_ACTIVE;
	}
rm_binding:
	if (generation == sc->sc_generation) {
		iwx_binding_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE);
		sc->sc_flags &= ~IWX_FLAG_BINDING_ACTIVE;
	}
rm_mac_ctxt:
	if (generation == sc->sc_generation) {
		iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0);
		sc->sc_flags &= ~IWX_FLAG_MAC_ACTIVE;
	}
	return err;
}

int
iwx_deauth(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	int err;

	splassert(IPL_NET);

	iwx_unprotect_session(sc, in);

	if (sc->sc_flags & IWX_FLAG_STA_ACTIVE) {
		err = iwx_flush_tx_path(sc);
		if (err) {
			printf("%s: could not flush Tx path (error %d)\n",
			    DEVNAME(sc), err);
			return err;
		}
		err = iwx_rm_sta_cmd(sc, in);
		if (err) {
			printf("%s: could not remove STA (error %d)\n",
			    DEVNAME(sc), err);
			return err;
		}
		sc->sc_flags &= ~IWX_FLAG_STA_ACTIVE;
	}

	if (sc->sc_flags & IWX_FLAG_BINDING_ACTIVE) {
		err = iwx_binding_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE);
		if (err) {
			printf("%s: could not remove binding (error %d)\n",
			    DEVNAME(sc), err);
			return err;
		}
		sc->sc_flags &= ~IWX_FLAG_BINDING_ACTIVE;
	}

	if (sc->sc_flags & IWX_FLAG_MAC_ACTIVE) {
		err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0);
		if (err) {
			printf("%s: could not remove MAC context (error %d)\n",
			    DEVNAME(sc), err);
			return err;
		}
		sc->sc_flags &= ~IWX_FLAG_MAC_ACTIVE;
	}

	return 0;
}

int
iwx_assoc(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	int update_sta = (sc->sc_flags & IWX_FLAG_STA_ACTIVE);
	int err;

	splassert(IPL_NET);

	err = iwx_add_sta_cmd(sc, in, update_sta);
	if (err) {
		printf("%s: could not %s STA (error %d)\n",
		    DEVNAME(sc), update_sta ? "update" : "add", err);
		return err;
	}

	if (!update_sta)
		err = iwx_enable_data_tx_queues(sc);

	return err;
}

int
iwx_disassoc(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	int err;

	splassert(IPL_NET);

	if (sc->sc_flags & IWX_FLAG_STA_ACTIVE) {
		err = iwx_rm_sta_cmd(sc, in);
		if (err) {
			printf("%s: could not remove STA (error %d)\n",
			    DEVNAME(sc), err);
			return err;
		}
		sc->sc_flags &= ~IWX_FLAG_STA_ACTIVE;
	}

	return 0;
}

int
iwx_run(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	int err;

	splassert(IPL_NET);

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		/* Add a MAC context and a sniffing STA. */
		err = iwx_auth(sc);
		if (err)
			return err;
	}

	/* Configure Rx chains for MIMO. */
	if ((ic->ic_opmode == IEEE80211_M_MONITOR ||
	    (in->in_ni.ni_flags & IEEE80211_NODE_HT)) &&
	    iwx_mimo_enabled(sc)) {
		err = iwx_phy_ctxt_cmd(sc, &sc->sc_phyctxt[0],
		    2, 2, IWX_FW_CTXT_ACTION_MODIFY, 0);
		if (err) {
			printf("%s: failed to update PHY\n",
			    DEVNAME(sc));
			return err;
		}
	}

	/* We have now been assigned an associd by the AP. */
	err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 1);
	if (err) {
		printf("%s: failed to update MAC\n", DEVNAME(sc));
		return err;
	}

	err = iwx_sf_config(sc, IWX_SF_FULL_ON);
	if (err) {
		printf("%s: could not set sf full on (error %d)\n",
		    DEVNAME(sc), err);
		return err;
	}

	err = iwx_allow_mcast(sc);
	if (err) {
		printf("%s: could not allow mcast (error %d)\n",
		    DEVNAME(sc), err);
		return err;
	}

	err = iwx_power_update_device(sc);
	if (err) {
		printf("%s: could not send power command (error %d)\n",
		    DEVNAME(sc), err);
		return err;
	}
#ifdef notyet
	/* 
	 * Disabled for now. Default beacon filter settings
	 * prevent net80211 from getting ERP and HT protection
	 * updates from beacons.
	 */
	err = iwx_enable_beacon_filter(sc, in);
	if (err) {
		printf("%s: could not enable beacon filter\n",
		    DEVNAME(sc));
		return err;
	}
#endif
	err = iwx_power_mac_update_mode(sc, in);
	if (err) {
		printf("%s: could not update MAC power (error %d)\n",
		    DEVNAME(sc), err);
		return err;
	}

	if (!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_DYNAMIC_QUOTA)) {
		err = iwx_update_quotas(sc, in, 1);
		if (err) {
			printf("%s: could not update quotas (error %d)\n",
			    DEVNAME(sc), err);
			return err;
		}
	}

	ieee80211_amrr_node_init(&sc->sc_amrr, &in->in_amn);
	ieee80211_mira_node_init(&in->in_mn);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		return 0;

	/* Start at lowest available bit-rate, AMRR will raise. */
	in->in_ni.ni_txrate = 0;
	in->in_ni.ni_txmcs = 0;

	if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_TLC_OFFLOAD))
		DPRINTF(("%s: TODO: Enable firmware rate scaling?\n",
		    DEVNAME(sc)));

	timeout_add_msec(&sc->sc_calib_to, 500);
	return 0;
}

int
iwx_run_stop(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	int err;

	splassert(IPL_NET);

	err = iwx_sf_config(sc, IWX_SF_INIT_OFF);
	if (err)
		return err;

	err = iwx_disable_beacon_filter(sc);
	if (err) {
		printf("%s: could not disable beacon filter (error %d)\n",
		    DEVNAME(sc), err);
		return err;
	}	

	if (!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_DYNAMIC_QUOTA)) {
		err = iwx_update_quotas(sc, in, 0);
		if (err) {
			printf("%s: could not update quotas (error %d)\n",
			    DEVNAME(sc), err);
			return err;
		}
	}

	err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 0);
	if (err) {
		printf("%s: failed to update MAC\n", DEVNAME(sc));
		return err;
	}

	/* Reset Tx chains in case MIMO was enabled. */
	if ((in->in_ni.ni_flags & IEEE80211_NODE_HT) &&
	    iwx_mimo_enabled(sc)) {
		err = iwx_phy_ctxt_cmd(sc, &sc->sc_phyctxt[0], 1, 1,
		    IWX_FW_CTXT_ACTION_MODIFY, 0);
		if (err) {
			printf("%s: failed to update PHY\n", DEVNAME(sc));
			return err;
		}
	}

	return 0;
}

struct ieee80211_node *
iwx_node_alloc(struct ieee80211com *ic)
{
	return malloc(sizeof (struct iwx_node), M_DEVBUF, M_NOWAIT | M_ZERO);
}

void
iwx_calib_timeout(void *arg)
{
	struct iwx_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	struct ieee80211_node *ni = &in->in_ni;
	int s;

	s = splnet();
	if ((ic->ic_fixed_rate == -1 || ic->ic_fixed_mcs == -1) &&
	    ((ni->ni_flags & IEEE80211_NODE_HT) == 0 || in->ht_force_cck) &&
	    ic->ic_opmode == IEEE80211_M_STA && ic->ic_bss) {
		ieee80211_amrr_choose(&sc->sc_amrr, &in->in_ni, &in->in_amn);
		if (in->ht_force_cck) {
			struct ieee80211_rateset *rs = &ni->ni_rates;
			uint8_t rv;
			rv = (rs->rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL);
			if (IWX_RVAL_IS_OFDM(rv))
				in->ht_force_cck = 0;
		}
	}

	splx(s);

	timeout_add_msec(&sc->sc_calib_to, 500);
}

int
iwx_media_change(struct ifnet *ifp)
{
	struct iwx_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t rate, ridx;
	int err;

	err = ieee80211_media_change(ifp);
	if (err != ENETRESET)
		return err;

	if (ic->ic_fixed_mcs != -1)
		sc->sc_fixed_ridx = iwx_mcs2ridx[ic->ic_fixed_mcs];
	else if (ic->ic_fixed_rate != -1) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		/* Map 802.11 rate to HW rate index. */
		for (ridx = 0; ridx <= IWX_RIDX_MAX; ridx++)
			if (iwx_rates[ridx].rate == rate)
				break;
		sc->sc_fixed_ridx = ridx;
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		iwx_stop(ifp);
		err = iwx_init(ifp);
	}
	return err;
}

void
iwx_newstate_task(void *psc)
{
	struct iwx_softc *sc = (struct iwx_softc *)psc;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state nstate = sc->ns_nstate;
	enum ieee80211_state ostate = ic->ic_state;
	int arg = sc->ns_arg;
	int err = 0, s = splnet();

	if (sc->sc_flags & IWX_FLAG_SHUTDOWN) {
		/* iwx_stop() is waiting for us. */
		refcnt_rele_wake(&sc->task_refs);
		splx(s);
		return;
	}

	if (ostate == IEEE80211_S_SCAN) {
		if (nstate == ostate) {
			if (sc->sc_flags & IWX_FLAG_SCANNING) {
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
			err = iwx_run_stop(sc);
			if (err)
				goto out;
			/* FALLTHROUGH */
		case IEEE80211_S_ASSOC:
			if (nstate <= IEEE80211_S_ASSOC) {
				err = iwx_disassoc(sc);
				if (err)
					goto out;
			}
			/* FALLTHROUGH */
		case IEEE80211_S_AUTH:
			if (nstate <= IEEE80211_S_AUTH) {
				err = iwx_deauth(sc);
				if (err)
					goto out;
			}
			/* FALLTHROUGH */
		case IEEE80211_S_SCAN:
		case IEEE80211_S_INIT:
			break;
		}

		/* Die now if iwx_stop() was called while we were sleeping. */
		if (sc->sc_flags & IWX_FLAG_SHUTDOWN) {
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
		err = iwx_scan(sc);
		if (err)
			break;
		refcnt_rele_wake(&sc->task_refs);
		splx(s);
		return;

	case IEEE80211_S_AUTH:
		err = iwx_auth(sc);
		break;

	case IEEE80211_S_ASSOC:
		err = iwx_assoc(sc);
		break;

	case IEEE80211_S_RUN:
		err = iwx_run(sc);
		break;
	}

out:
	if ((sc->sc_flags & IWX_FLAG_SHUTDOWN) == 0) {
		if (err)
			task_add(systq, &sc->init_task);
		else
			sc->sc_newstate(ic, nstate, arg);
	}
	refcnt_rele_wake(&sc->task_refs);
	splx(s);
}

int
iwx_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = IC2IFP(ic);
	struct iwx_softc *sc = ifp->if_softc;
	struct iwx_node *in = (void *)ic->ic_bss;

	if (ic->ic_state == IEEE80211_S_RUN) {
		timeout_del(&sc->sc_calib_to);
		ieee80211_mira_cancel_timeouts(&in->in_mn);
		iwx_del_task(sc, systq, &sc->ba_task);
		iwx_del_task(sc, systq, &sc->htprot_task);
	}

	sc->ns_nstate = nstate;
	sc->ns_arg = arg;

	iwx_add_task(sc, sc->sc_nswq, &sc->newstate_task);

	return 0;
}

void
iwx_endscan(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if ((sc->sc_flags & (IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN)) == 0)
		return;

	sc->sc_flags &= ~(IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN);
	ieee80211_end_scan(&ic->ic_if);
}

/*
 * Aging and idle timeouts for the different possible scenarios
 * in default configuration
 */
static const uint32_t
iwx_sf_full_timeout_def[IWX_SF_NUM_SCENARIO][IWX_SF_NUM_TIMEOUT_TYPES] = {
	{
		htole32(IWX_SF_SINGLE_UNICAST_AGING_TIMER_DEF),
		htole32(IWX_SF_SINGLE_UNICAST_IDLE_TIMER_DEF)
	},
	{
		htole32(IWX_SF_AGG_UNICAST_AGING_TIMER_DEF),
		htole32(IWX_SF_AGG_UNICAST_IDLE_TIMER_DEF)
	},
	{
		htole32(IWX_SF_MCAST_AGING_TIMER_DEF),
		htole32(IWX_SF_MCAST_IDLE_TIMER_DEF)
	},
	{
		htole32(IWX_SF_BA_AGING_TIMER_DEF),
		htole32(IWX_SF_BA_IDLE_TIMER_DEF)
	},
	{
		htole32(IWX_SF_TX_RE_AGING_TIMER_DEF),
		htole32(IWX_SF_TX_RE_IDLE_TIMER_DEF)
	},
};

/*
 * Aging and idle timeouts for the different possible scenarios
 * in single BSS MAC configuration.
 */
static const uint32_t
iwx_sf_full_timeout[IWX_SF_NUM_SCENARIO][IWX_SF_NUM_TIMEOUT_TYPES] = {
	{
		htole32(IWX_SF_SINGLE_UNICAST_AGING_TIMER),
		htole32(IWX_SF_SINGLE_UNICAST_IDLE_TIMER)
	},
	{
		htole32(IWX_SF_AGG_UNICAST_AGING_TIMER),
		htole32(IWX_SF_AGG_UNICAST_IDLE_TIMER)
	},
	{
		htole32(IWX_SF_MCAST_AGING_TIMER),
		htole32(IWX_SF_MCAST_IDLE_TIMER)
	},
	{
		htole32(IWX_SF_BA_AGING_TIMER),
		htole32(IWX_SF_BA_IDLE_TIMER)
	},
	{
		htole32(IWX_SF_TX_RE_AGING_TIMER),
		htole32(IWX_SF_TX_RE_IDLE_TIMER)
	},
};

void
iwx_fill_sf_command(struct iwx_softc *sc, struct iwx_sf_cfg_cmd *sf_cmd,
    struct ieee80211_node *ni)
{
	int i, j, watermark;

	sf_cmd->watermark[IWX_SF_LONG_DELAY_ON] = htole32(IWX_SF_W_MARK_SCAN);

	/*
	 * If we are in association flow - check antenna configuration
	 * capabilities of the AP station, and choose the watermark accordingly.
	 */
	if (ni) {
		if (ni->ni_flags & IEEE80211_NODE_HT) {
			if (ni->ni_rxmcs[1] != 0)
				watermark = IWX_SF_W_MARK_MIMO2;
			else
				watermark = IWX_SF_W_MARK_SISO;
		} else {
			watermark = IWX_SF_W_MARK_LEGACY;
		}
	/* default watermark value for unassociated mode. */
	} else {
		watermark = IWX_SF_W_MARK_MIMO2;
	}
	sf_cmd->watermark[IWX_SF_FULL_ON] = htole32(watermark);

	for (i = 0; i < IWX_SF_NUM_SCENARIO; i++) {
		for (j = 0; j < IWX_SF_NUM_TIMEOUT_TYPES; j++) {
			sf_cmd->long_delay_timeouts[i][j] =
					htole32(IWX_SF_LONG_DELAY_AGING_TIMER);
		}
	}

	if (ni) {
		memcpy(sf_cmd->full_on_timeouts, iwx_sf_full_timeout,
		       sizeof(iwx_sf_full_timeout));
	} else {
		memcpy(sf_cmd->full_on_timeouts, iwx_sf_full_timeout_def,
		       sizeof(iwx_sf_full_timeout_def));
	}

}

int
iwx_sf_config(struct iwx_softc *sc, int new_state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_sf_cfg_cmd sf_cmd = {
		.state = htole32(new_state),
	};
	int err = 0;

	switch (new_state) {
	case IWX_SF_UNINIT:
	case IWX_SF_INIT_OFF:
		iwx_fill_sf_command(sc, &sf_cmd, NULL);
		break;
	case IWX_SF_FULL_ON:
		iwx_fill_sf_command(sc, &sf_cmd, ic->ic_bss);
		break;
	default:
		return EINVAL;
	}

	err = iwx_send_cmd_pdu(sc, IWX_REPLY_SF_CFG_CMD, IWX_CMD_ASYNC,
				   sizeof(sf_cmd), &sf_cmd);
	return err;
}

int
iwx_send_bt_init_conf(struct iwx_softc *sc)
{
	struct iwx_bt_coex_cmd bt_cmd;

	bt_cmd.mode = htole32(IWX_BT_COEX_WIFI);
	bt_cmd.enabled_modules = 0;

	return iwx_send_cmd_pdu(sc, IWX_BT_CONFIG, 0, sizeof(bt_cmd),
	    &bt_cmd);
}

int
iwx_send_soc_conf(struct iwx_softc *sc)
{
	struct iwx_soc_configuration_cmd cmd;
	int err;
	uint32_t cmd_id, flags = 0;

	memset(&cmd, 0, sizeof(cmd));

	/*
	 * In VER_1 of this command, the discrete value is considered
	 * an integer; In VER_2, it's a bitmask.  Since we have only 2
	 * values in VER_1, this is backwards-compatible with VER_2,
	 * as long as we don't set any other flag bits.
	 */
	if (!sc->sc_integrated) { /* VER_1 */
		flags = IWX_SOC_CONFIG_CMD_FLAGS_DISCRETE;
	} else { /* VER_2 */
		uint8_t scan_cmd_ver;
		if (sc->sc_ltr_delay != IWX_SOC_FLAGS_LTR_APPLY_DELAY_NONE)
			flags |= (sc->sc_ltr_delay &
			    IWX_SOC_FLAGS_LTR_APPLY_DELAY_MASK);
		scan_cmd_ver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP,
		    IWX_SCAN_REQ_UMAC);
		if (scan_cmd_ver >= 2 && sc->sc_low_latency_xtal)
			flags |= IWX_SOC_CONFIG_CMD_FLAGS_LOW_LATENCY;
	}
	cmd.flags = htole32(flags);

	cmd.latency = htole32(sc->sc_xtal_latency);

	cmd_id = iwx_cmd_id(IWX_SOC_CONFIGURATION_CMD, IWX_SYSTEM_GROUP, 0);
	err = iwx_send_cmd_pdu(sc, cmd_id, 0, sizeof(cmd), &cmd);
	if (err)
		printf("%s: failed to set soc latency: %d\n", DEVNAME(sc), err);
	return err;
}

int
iwx_send_update_mcc_cmd(struct iwx_softc *sc, const char *alpha2)
{
	struct iwx_mcc_update_cmd mcc_cmd;
	struct iwx_host_cmd hcmd = {
		.id = IWX_MCC_UPDATE_CMD,
		.flags = IWX_CMD_WANT_RESP,
		.data = { &mcc_cmd },
	};
	int err;

	memset(&mcc_cmd, 0, sizeof(mcc_cmd));
	mcc_cmd.mcc = htole16(alpha2[0] << 8 | alpha2[1]);
	if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_WIFI_MCC_UPDATE) ||
	    isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_LAR_MULTI_MCC))
		mcc_cmd.source_id = IWX_MCC_SOURCE_GET_CURRENT;
	else
		mcc_cmd.source_id = IWX_MCC_SOURCE_OLD_FW;

	hcmd.len[0] = sizeof(struct iwx_mcc_update_cmd);
	hcmd.resp_pkt_len = sizeof(struct iwx_rx_packet) +
	    sizeof(struct iwx_mcc_update_resp);

	err = iwx_send_cmd(sc, &hcmd);
	if (err)
		return err;

	iwx_free_resp(sc, &hcmd);

	return 0;
}

int
iwx_init_hw(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int err, i;

	err = iwx_preinit(sc);
	if (err)
		return err;

	err = iwx_start_hw(sc);
	if (err) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return err;
	}

	err = iwx_run_init_mvm_ucode(sc, 0);
	if (err)
		return err;

	/* Should stop and start HW since INIT image just loaded. */
	iwx_stop_device(sc);
	err = iwx_start_hw(sc);
	if (err) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return err;
	}

	err = iwx_load_ucode_wait_alive(sc);
	if (err) {
		printf("%s: could not load firmware\n", DEVNAME(sc));
		goto err;
	}

	if (!iwx_nic_lock(sc))
		return EBUSY;

	err = iwx_send_tx_ant_cfg(sc, iwx_fw_valid_tx_ant(sc));
	if (err) {
		printf("%s: could not init tx ant config (error %d)\n",
		    DEVNAME(sc), err);
		goto err;
	}

	if (sc->sc_tx_with_siso_diversity) {
		err = iwx_send_phy_cfg_cmd(sc);
		if (err) {
			printf("%s: could not send phy config (error %d)\n",
			    DEVNAME(sc), err);
			goto err;
		}
	}

	err = iwx_send_bt_init_conf(sc);
	if (err) {
		printf("%s: could not init bt coex (error %d)\n",
		    DEVNAME(sc), err);
		return err;
	}

	err = iwx_send_soc_conf(sc);
	if (err)
		return err;

	err = iwx_send_dqa_cmd(sc);
	if (err)
		return err;

	/* Add auxiliary station for scanning */
	err = iwx_add_aux_sta(sc);
	if (err) {
		printf("%s: could not add aux station (error %d)\n",
		    DEVNAME(sc), err);
		goto err;
	}

	for (i = 0; i < 1; i++) {
		/*
		 * The channel used here isn't relevant as it's
		 * going to be overwritten in the other flows.
		 * For now use the first channel we have.
		 */
		sc->sc_phyctxt[i].channel = &ic->ic_channels[1];
		err = iwx_phy_ctxt_cmd(sc, &sc->sc_phyctxt[i], 1, 1,
		    IWX_FW_CTXT_ACTION_ADD, 0);
		if (err) {
			printf("%s: could not add phy context %d (error %d)\n",
			    DEVNAME(sc), i, err);
			goto err;
		}
	}

	err = iwx_config_ltr(sc);
	if (err) {
		printf("%s: PCIe LTR configuration failed (error %d)\n",
		    DEVNAME(sc), err);
	}

	err = iwx_power_update_device(sc);
	if (err) {
		printf("%s: could not send power command (error %d)\n",
		    DEVNAME(sc), err);
		goto err;
	}

	if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_LAR_SUPPORT)) {
		err = iwx_send_update_mcc_cmd(sc, "ZZ");
		if (err) {
			printf("%s: could not init LAR (error %d)\n",
			    DEVNAME(sc), err);
			goto err;
		}
	}

	err = iwx_config_umac_scan(sc);
	if (err) {
		printf("%s: could not configure scan (error %d)\n",
		    DEVNAME(sc), err);
		goto err;
	}

	err = iwx_disable_beacon_filter(sc);
	if (err) {
		printf("%s: could not disable beacon filter (error %d)\n",
		    DEVNAME(sc), err);
		goto err;
	}

err:
	iwx_nic_unlock(sc);
	return err;
}

/* Allow multicast from our BSSID. */
int
iwx_allow_mcast(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwx_mcast_filter_cmd *cmd;
	size_t size;
	int err;

	size = roundup(sizeof(*cmd), 4);
	cmd = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cmd == NULL)
		return ENOMEM;
	cmd->filter_own = 1;
	cmd->port_id = 0;
	cmd->count = 0;
	cmd->pass_all = 1;
	IEEE80211_ADDR_COPY(cmd->bssid, ni->ni_bssid);

	err = iwx_send_cmd_pdu(sc, IWX_MCAST_FILTER_CMD,
	    0, size, cmd);
	free(cmd, M_DEVBUF, size);
	return err;
}

int
iwx_init(struct ifnet *ifp)
{
	struct iwx_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int err, generation;

	rw_assert_wrlock(&sc->ioctl_rwl);

	generation = ++sc->sc_generation;

	KASSERT(sc->task_refs.refs == 0);
	refcnt_init(&sc->task_refs);

	err = iwx_init_hw(sc);
	if (err) {
		if (generation == sc->sc_generation)
			iwx_stop(ifp);
		return err;
	}

	if (sc->sc_nvm.sku_cap_11n_enable)
		iwx_setup_ht_rates(sc);

	ifq_clr_oactive(&ifp->if_snd);
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
		return 0;
	}

	ieee80211_begin_scan(ifp);

	/* 
	 * ieee80211_begin_scan() ends up scheduling iwx_newstate_task().
	 * Wait until the transition to SCAN state has completed.
	 */
	do {
		err = tsleep_nsec(&ic->ic_state, PCATCH, "iwxinit",
		    SEC_TO_NSEC(1));
		if (generation != sc->sc_generation)
			return ENXIO;
		if (err)
			return err;
	} while (ic->ic_state != IEEE80211_S_SCAN);

	return 0;
}

void
iwx_start(struct ifnet *ifp)
{
	struct iwx_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ether_header *eh;
	struct mbuf *m;
	int ac = EDCA_AC_BE; /* XXX */

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd))
		return;

	for (;;) {
		/* why isn't this done per-queue? */
		if (sc->qfullmsk != 0) {
			ifq_set_oactive(&ifp->if_snd);
			break;
		}

		/* need to send management frames even if we're not RUNning */
		m = mq_dequeue(&ic->ic_mgtq);
		if (m) {
			ni = m->m_pkthdr.ph_cookie;
			goto sendit;
		}

		if (ic->ic_state != IEEE80211_S_RUN ||
		    (ic->ic_xflags & IEEE80211_F_TX_MGMT_ONLY))
			break;

		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (!m)
			break;
		if (m->m_len < sizeof (*eh) &&
		    (m = m_pullup(m, sizeof (*eh))) == NULL) {
			ifp->if_oerrors++;
			continue;
		}
#if NBPFILTER > 0
		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		if ((m = ieee80211_encap(ifp, m, &ni)) == NULL) {
			ifp->if_oerrors++;
			continue;
		}

 sendit:
#if NBPFILTER > 0
		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_OUT);
#endif
		if (iwx_tx(sc, m, ni, ac) != 0) {
			ieee80211_release_node(ic, ni);
			ifp->if_oerrors++;
			continue;
		}

		if (ifp->if_flags & IFF_UP) {
			sc->sc_tx_timer = 15;
			ifp->if_timer = 1;
		}
	}

	return;
}

void
iwx_stop(struct ifnet *ifp)
{
	struct iwx_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwx_node *in = (void *)ic->ic_bss;
	int i, s = splnet();

	rw_assert_wrlock(&sc->ioctl_rwl);

	sc->sc_flags |= IWX_FLAG_SHUTDOWN; /* Disallow new tasks. */

	/* Cancel scheduled tasks and let any stale tasks finish up. */
	task_del(systq, &sc->init_task);
	iwx_del_task(sc, sc->sc_nswq, &sc->newstate_task);
	iwx_del_task(sc, systq, &sc->ba_task);
	iwx_del_task(sc, systq, &sc->htprot_task);
	KASSERT(sc->task_refs.refs >= 1);
	refcnt_finalize(&sc->task_refs, "iwxstop");

	iwx_stop_device(sc);

	/* Reset soft state. */

	sc->sc_generation++;
	for (i = 0; i < nitems(sc->sc_cmd_resp_pkt); i++) {
		free(sc->sc_cmd_resp_pkt[i], M_DEVBUF, sc->sc_cmd_resp_len[i]);
		sc->sc_cmd_resp_pkt[i] = NULL;
		sc->sc_cmd_resp_len[i] = 0;
	}
	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	in->in_phyctxt = NULL;
	if (ic->ic_state == IEEE80211_S_RUN)
		ieee80211_mira_cancel_timeouts(&in->in_mn); /* XXX refcount? */

	sc->sc_flags &= ~(IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN);
	sc->sc_flags &= ~IWX_FLAG_MAC_ACTIVE;
	sc->sc_flags &= ~IWX_FLAG_BINDING_ACTIVE;
	sc->sc_flags &= ~IWX_FLAG_STA_ACTIVE;
	sc->sc_flags &= ~IWX_FLAG_TE_ACTIVE;
	sc->sc_flags &= ~IWX_FLAG_HW_ERR;
	sc->sc_flags &= ~IWX_FLAG_SHUTDOWN;

	sc->sc_newstate(ic, IEEE80211_S_INIT, -1);

	timeout_del(&sc->sc_calib_to); /* XXX refcount? */
	ifp->if_timer = sc->sc_tx_timer = 0;

	splx(s);
}

void
iwx_watchdog(struct ifnet *ifp)
{
	struct iwx_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;
	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", DEVNAME(sc));
#ifdef IWX_DEBUG
			iwx_nic_error(sc);
#endif
			if ((sc->sc_flags & IWX_FLAG_SHUTDOWN) == 0)
				task_add(systq, &sc->init_task);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
iwx_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iwx_softc *sc = ifp->if_softc;
	int s, err = 0, generation = sc->sc_generation;

	/*
	 * Prevent processes from entering this function while another
	 * process is tsleep'ing in it.
	 */
	err = rw_enter(&sc->ioctl_rwl, RW_WRITE | RW_INTR);
	if (err == 0 && generation != sc->sc_generation) {
		rw_exit(&sc->ioctl_rwl);
		return ENXIO;
	}
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
				err = iwx_init(ifp);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iwx_stop(ifp);
		}
		break;

	default:
		err = ieee80211_ioctl(ifp, cmd, data);
	}

	if (err == ENETRESET) {
		err = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			iwx_stop(ifp);
			err = iwx_init(ifp);
		}
	}

	splx(s);
	rw_exit(&sc->ioctl_rwl);

	return err;
}

#if 1 /* usually #ifdef IWX_DEBUG but always enabled for now */
/*
 * Note: This structure is read from the device with IO accesses,
 * and the reading already does the endian conversion. As it is
 * read with uint32_t-sized accesses, any members with a different size
 * need to be ordered correctly though!
 */
struct iwx_error_event_table {
	uint32_t valid;		/* (nonzero) valid, (0) log is empty */
	uint32_t error_id;		/* type of error */
	uint32_t trm_hw_status0;	/* TRM HW status */
	uint32_t trm_hw_status1;	/* TRM HW status */
	uint32_t blink2;		/* branch link */
	uint32_t ilink1;		/* interrupt link */
	uint32_t ilink2;		/* interrupt link */
	uint32_t data1;		/* error-specific data */
	uint32_t data2;		/* error-specific data */
	uint32_t data3;		/* error-specific data */
	uint32_t bcon_time;		/* beacon timer */
	uint32_t tsf_low;		/* network timestamp function timer */
	uint32_t tsf_hi;		/* network timestamp function timer */
	uint32_t gp1;		/* GP1 timer register */
	uint32_t gp2;		/* GP2 timer register */
	uint32_t fw_rev_type;	/* firmware revision type */
	uint32_t major;		/* uCode version major */
	uint32_t minor;		/* uCode version minor */
	uint32_t hw_ver;		/* HW Silicon version */
	uint32_t brd_ver;		/* HW board version */
	uint32_t log_pc;		/* log program counter */
	uint32_t frame_ptr;		/* frame pointer */
	uint32_t stack_ptr;		/* stack pointer */
	uint32_t hcmd;		/* last host command header */
	uint32_t isr0;		/* isr status register LMPM_NIC_ISR0:
				 * rxtx_flag */
	uint32_t isr1;		/* isr status register LMPM_NIC_ISR1:
				 * host_flag */
	uint32_t isr2;		/* isr status register LMPM_NIC_ISR2:
				 * enc_flag */
	uint32_t isr3;		/* isr status register LMPM_NIC_ISR3:
				 * time_flag */
	uint32_t isr4;		/* isr status register LMPM_NIC_ISR4:
				 * wico interrupt */
	uint32_t last_cmd_id;	/* last HCMD id handled by the firmware */
	uint32_t wait_event;		/* wait event() caller address */
	uint32_t l2p_control;	/* L2pControlField */
	uint32_t l2p_duration;	/* L2pDurationField */
	uint32_t l2p_mhvalid;	/* L2pMhValidBits */
	uint32_t l2p_addr_match;	/* L2pAddrMatchStat */
	uint32_t lmpm_pmg_sel;	/* indicate which clocks are turned on
				 * (LMPM_PMG_SEL) */
	uint32_t u_timestamp;	/* indicate when the date and time of the
				 * compilation */
	uint32_t flow_handler;	/* FH read/write pointers, RX credit */
} __packed /* LOG_ERROR_TABLE_API_S_VER_3 */;

/*
 * UMAC error struct - relevant starting from family 8000 chip.
 * Note: This structure is read from the device with IO accesses,
 * and the reading already does the endian conversion. As it is
 * read with u32-sized accesses, any members with a different size
 * need to be ordered correctly though!
 */
struct iwx_umac_error_event_table {
	uint32_t valid;		/* (nonzero) valid, (0) log is empty */
	uint32_t error_id;	/* type of error */
	uint32_t blink1;	/* branch link */
	uint32_t blink2;	/* branch link */
	uint32_t ilink1;	/* interrupt link */
	uint32_t ilink2;	/* interrupt link */
	uint32_t data1;		/* error-specific data */
	uint32_t data2;		/* error-specific data */
	uint32_t data3;		/* error-specific data */
	uint32_t umac_major;
	uint32_t umac_minor;
	uint32_t frame_pointer;	/* core register 27*/
	uint32_t stack_pointer;	/* core register 28 */
	uint32_t cmd_header;	/* latest host cmd sent to UMAC */
	uint32_t nic_isr_pref;	/* ISR status register */
} __packed;

#define ERROR_START_OFFSET  (1 * sizeof(uint32_t))
#define ERROR_ELEM_SIZE     (7 * sizeof(uint32_t))

void
iwx_nic_umac_error(struct iwx_softc *sc)
{
	struct iwx_umac_error_event_table table;
	uint32_t base;

	base = sc->sc_uc.uc_umac_error_event_table;

	if (base < 0x800000) {
		printf("%s: Invalid error log pointer 0x%08x\n",
		    DEVNAME(sc), base);
		return;
	}

	if (iwx_read_mem(sc, base, &table, sizeof(table)/sizeof(uint32_t))) {
		printf("%s: reading errlog failed\n", DEVNAME(sc));
		return;
	}

	if (ERROR_START_OFFSET <= table.valid * ERROR_ELEM_SIZE) {
		printf("%s: Start UMAC Error Log Dump:\n", DEVNAME(sc));
		printf("%s: Status: 0x%x, count: %d\n", DEVNAME(sc),
			sc->sc_flags, table.valid);
	}

	printf("%s: 0x%08X | %s\n", DEVNAME(sc), table.error_id,
		iwx_desc_lookup(table.error_id));
	printf("%s: 0x%08X | umac branchlink1\n", DEVNAME(sc), table.blink1);
	printf("%s: 0x%08X | umac branchlink2\n", DEVNAME(sc), table.blink2);
	printf("%s: 0x%08X | umac interruptlink1\n", DEVNAME(sc), table.ilink1);
	printf("%s: 0x%08X | umac interruptlink2\n", DEVNAME(sc), table.ilink2);
	printf("%s: 0x%08X | umac data1\n", DEVNAME(sc), table.data1);
	printf("%s: 0x%08X | umac data2\n", DEVNAME(sc), table.data2);
	printf("%s: 0x%08X | umac data3\n", DEVNAME(sc), table.data3);
	printf("%s: 0x%08X | umac major\n", DEVNAME(sc), table.umac_major);
	printf("%s: 0x%08X | umac minor\n", DEVNAME(sc), table.umac_minor);
	printf("%s: 0x%08X | frame pointer\n", DEVNAME(sc),
	    table.frame_pointer);
	printf("%s: 0x%08X | stack pointer\n", DEVNAME(sc),
	    table.stack_pointer);
	printf("%s: 0x%08X | last host cmd\n", DEVNAME(sc), table.cmd_header);
	printf("%s: 0x%08X | isr status reg\n", DEVNAME(sc),
	    table.nic_isr_pref);
}

#define IWX_FW_SYSASSERT_CPU_MASK 0xf0000000
static struct {
	const char *name;
	uint8_t num;
} advanced_lookup[] = {
	{ "NMI_INTERRUPT_WDG", 0x34 },
	{ "SYSASSERT", 0x35 },
	{ "UCODE_VERSION_MISMATCH", 0x37 },
	{ "BAD_COMMAND", 0x38 },
	{ "BAD_COMMAND", 0x39 },
	{ "NMI_INTERRUPT_DATA_ACTION_PT", 0x3C },
	{ "FATAL_ERROR", 0x3D },
	{ "NMI_TRM_HW_ERR", 0x46 },
	{ "NMI_INTERRUPT_TRM", 0x4C },
	{ "NMI_INTERRUPT_BREAK_POINT", 0x54 },
	{ "NMI_INTERRUPT_WDG_RXF_FULL", 0x5C },
	{ "NMI_INTERRUPT_WDG_NO_RBD_RXF_FULL", 0x64 },
	{ "NMI_INTERRUPT_HOST", 0x66 },
	{ "NMI_INTERRUPT_LMAC_FATAL", 0x70 },
	{ "NMI_INTERRUPT_UMAC_FATAL", 0x71 },
	{ "NMI_INTERRUPT_OTHER_LMAC_FATAL", 0x73 },
	{ "NMI_INTERRUPT_ACTION_PT", 0x7C },
	{ "NMI_INTERRUPT_UNKNOWN", 0x84 },
	{ "NMI_INTERRUPT_INST_ACTION_PT", 0x86 },
	{ "ADVANCED_SYSASSERT", 0 },
};

const char *
iwx_desc_lookup(uint32_t num)
{
	int i;

	for (i = 0; i < nitems(advanced_lookup) - 1; i++)
		if (advanced_lookup[i].num ==
		    (num & ~IWX_FW_SYSASSERT_CPU_MASK))
			return advanced_lookup[i].name;

	/* No entry matches 'num', so it is the last: ADVANCED_SYSASSERT */
	return advanced_lookup[i].name;
}

/*
 * Support for dumping the error log seemed like a good idea ...
 * but it's mostly hex junk and the only sensible thing is the
 * hw/ucode revision (which we know anyway).  Since it's here,
 * I'll just leave it in, just in case e.g. the Intel guys want to
 * help us decipher some "ADVANCED_SYSASSERT" later.
 */
void
iwx_nic_error(struct iwx_softc *sc)
{
	struct iwx_error_event_table table;
	uint32_t base;

	printf("%s: dumping device error log\n", DEVNAME(sc));
	base = sc->sc_uc.uc_lmac_error_event_table[0];
	if (base < 0x800000) {
		printf("%s: Invalid error log pointer 0x%08x\n",
		    DEVNAME(sc), base);
		return;
	}

	if (iwx_read_mem(sc, base, &table, sizeof(table)/sizeof(uint32_t))) {
		printf("%s: reading errlog failed\n", DEVNAME(sc));
		return;
	}

	if (!table.valid) {
		printf("%s: errlog not found, skipping\n", DEVNAME(sc));
		return;
	}

	if (ERROR_START_OFFSET <= table.valid * ERROR_ELEM_SIZE) {
		printf("%s: Start Error Log Dump:\n", DEVNAME(sc));
		printf("%s: Status: 0x%x, count: %d\n", DEVNAME(sc),
		    sc->sc_flags, table.valid);
	}

	printf("%s: 0x%08X | %-28s\n", DEVNAME(sc), table.error_id,
	    iwx_desc_lookup(table.error_id));
	printf("%s: %08X | trm_hw_status0\n", DEVNAME(sc),
	    table.trm_hw_status0);
	printf("%s: %08X | trm_hw_status1\n", DEVNAME(sc),
	    table.trm_hw_status1);
	printf("%s: %08X | branchlink2\n", DEVNAME(sc), table.blink2);
	printf("%s: %08X | interruptlink1\n", DEVNAME(sc), table.ilink1);
	printf("%s: %08X | interruptlink2\n", DEVNAME(sc), table.ilink2);
	printf("%s: %08X | data1\n", DEVNAME(sc), table.data1);
	printf("%s: %08X | data2\n", DEVNAME(sc), table.data2);
	printf("%s: %08X | data3\n", DEVNAME(sc), table.data3);
	printf("%s: %08X | beacon time\n", DEVNAME(sc), table.bcon_time);
	printf("%s: %08X | tsf low\n", DEVNAME(sc), table.tsf_low);
	printf("%s: %08X | tsf hi\n", DEVNAME(sc), table.tsf_hi);
	printf("%s: %08X | time gp1\n", DEVNAME(sc), table.gp1);
	printf("%s: %08X | time gp2\n", DEVNAME(sc), table.gp2);
	printf("%s: %08X | uCode revision type\n", DEVNAME(sc),
	    table.fw_rev_type);
	printf("%s: %08X | uCode version major\n", DEVNAME(sc),
	    table.major);
	printf("%s: %08X | uCode version minor\n", DEVNAME(sc),
	    table.minor);
	printf("%s: %08X | hw version\n", DEVNAME(sc), table.hw_ver);
	printf("%s: %08X | board version\n", DEVNAME(sc), table.brd_ver);
	printf("%s: %08X | hcmd\n", DEVNAME(sc), table.hcmd);
	printf("%s: %08X | isr0\n", DEVNAME(sc), table.isr0);
	printf("%s: %08X | isr1\n", DEVNAME(sc), table.isr1);
	printf("%s: %08X | isr2\n", DEVNAME(sc), table.isr2);
	printf("%s: %08X | isr3\n", DEVNAME(sc), table.isr3);
	printf("%s: %08X | isr4\n", DEVNAME(sc), table.isr4);
	printf("%s: %08X | last cmd Id\n", DEVNAME(sc), table.last_cmd_id);
	printf("%s: %08X | wait_event\n", DEVNAME(sc), table.wait_event);
	printf("%s: %08X | l2p_control\n", DEVNAME(sc), table.l2p_control);
	printf("%s: %08X | l2p_duration\n", DEVNAME(sc), table.l2p_duration);
	printf("%s: %08X | l2p_mhvalid\n", DEVNAME(sc), table.l2p_mhvalid);
	printf("%s: %08X | l2p_addr_match\n", DEVNAME(sc), table.l2p_addr_match);
	printf("%s: %08X | lmpm_pmg_sel\n", DEVNAME(sc), table.lmpm_pmg_sel);
	printf("%s: %08X | timestamp\n", DEVNAME(sc), table.u_timestamp);
	printf("%s: %08X | flow_handler\n", DEVNAME(sc), table.flow_handler);

	if (sc->sc_uc.uc_umac_error_event_table)
		iwx_nic_umac_error(sc);
}
#endif

#define SYNC_RESP_STRUCT(_var_, _pkt_)					\
do {									\
	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof(*(_pkt_)),	\
	    sizeof(*(_var_)), BUS_DMASYNC_POSTREAD);			\
	_var_ = (void *)((_pkt_)+1);					\
} while (/*CONSTCOND*/0)

#define SYNC_RESP_PTR(_ptr_, _len_, _pkt_)				\
do {									\
	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof(*(_pkt_)),	\
	    sizeof(len), BUS_DMASYNC_POSTREAD);				\
	_ptr_ = (void *)((_pkt_)+1);					\
} while (/*CONSTCOND*/0)

int
iwx_rx_pkt_valid(struct iwx_rx_packet *pkt)
{
	int qid, idx, code;

	qid = pkt->hdr.qid & ~0x80;
	idx = pkt->hdr.idx;
	code = IWX_WIDE_ID(pkt->hdr.flags, pkt->hdr.code);

	return (!(qid == 0 && idx == 0 && code == 0) &&
	    pkt->len_n_flags != htole32(IWX_FH_RSCSR_FRAME_INVALID));
}

void
iwx_rx_pkt(struct iwx_softc *sc, struct iwx_rx_data *data, struct mbuf_list *ml)
{
	struct ifnet *ifp = IC2IFP(&sc->sc_ic);
	struct iwx_rx_packet *pkt, *nextpkt;
	uint32_t offset = 0, nextoff = 0, nmpdu = 0, len;
	struct mbuf *m0, *m;
	const size_t minsz = sizeof(pkt->len_n_flags) + sizeof(pkt->hdr);
	size_t remain = IWX_RBUF_SIZE;
	int qid, idx, code, handled = 1;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, IWX_RBUF_SIZE,
	    BUS_DMASYNC_POSTREAD);

	m0 = data->m;
	while (m0 && offset + minsz < IWX_RBUF_SIZE) {
		pkt = (struct iwx_rx_packet *)(m0->m_data + offset);
		qid = pkt->hdr.qid;
		idx = pkt->hdr.idx;

		code = IWX_WIDE_ID(pkt->hdr.flags, pkt->hdr.code);

		if (!iwx_rx_pkt_valid(pkt))
			break;

		len = sizeof(pkt->len_n_flags) + iwx_rx_packet_len(pkt);
		if (len < sizeof(pkt->hdr) ||
		    len > (IWX_RBUF_SIZE - offset - minsz))
			break;

		if (code == IWX_REPLY_RX_MPDU_CMD && ++nmpdu == 1) {
			/* Take mbuf m0 off the RX ring. */
			if (iwx_rx_addbuf(sc, IWX_RBUF_SIZE, sc->rxq.cur)) {
				ifp->if_ierrors++;
				break;
			}
			KASSERT(data->m != m0);
		}

		switch (code) {
		case IWX_REPLY_RX_PHY_CMD:
			iwx_rx_rx_phy_cmd(sc, pkt, data);
			break;

		case IWX_REPLY_RX_MPDU_CMD: {
			size_t maxlen = remain - minsz;
			nextoff = offset +
			    roundup(len, IWX_FH_RSCSR_FRAME_ALIGN);
			nextpkt = (struct iwx_rx_packet *)
			    (m0->m_data + nextoff);
			if (nextoff + minsz >= IWX_RBUF_SIZE ||
			    !iwx_rx_pkt_valid(nextpkt)) {
				/* No need to copy last frame in buffer. */
				if (offset > 0)
					m_adj(m0, offset);
				iwx_rx_mpdu_mq(sc, m0, pkt->data, maxlen, ml);
				m0 = NULL; /* stack owns m0 now; abort loop */
			} else {
				/*
				 * Create an mbuf which points to the current
				 * packet. Always copy from offset zero to
				 * preserve m_pkthdr.
				 */
				m = m_copym(m0, 0, M_COPYALL, M_DONTWAIT);
				if (m == NULL) {
					ifp->if_ierrors++;
					m_freem(m0);
					m0 = NULL;
					break;
				}
				m_adj(m, offset);
				iwx_rx_mpdu_mq(sc, m, pkt->data, maxlen, ml);
			}

			if (offset + minsz < remain)
				remain -= offset;
			else
				remain = minsz;
 			break;
		}

		case IWX_TX_CMD:
			iwx_rx_tx_cmd(sc, pkt, data);
			break;

		case IWX_MISSED_BEACONS_NOTIFICATION:
			iwx_rx_bmiss(sc, pkt, data);
			break;

		case IWX_MFUART_LOAD_NOTIFICATION:
			break;

		case IWX_ALIVE: {
			struct iwx_alive_resp_v4 *resp4;

			DPRINTF(("%s: firmware alive\n", __func__));
			if (iwx_rx_packet_payload_len(pkt) == sizeof(*resp4)) {
				SYNC_RESP_STRUCT(resp4, pkt);
				sc->sc_uc.uc_lmac_error_event_table[0] = le32toh(
				    resp4->lmac_data[0].dbg_ptrs.error_event_table_ptr);
				sc->sc_uc.uc_lmac_error_event_table[1] = le32toh(
				    resp4->lmac_data[1].dbg_ptrs.error_event_table_ptr);
				sc->sc_uc.uc_log_event_table = le32toh(
				    resp4->lmac_data[0].dbg_ptrs.log_event_table_ptr);
				sc->sched_base = le32toh(
				    resp4->lmac_data[0].dbg_ptrs.scd_base_ptr);
				sc->sc_uc.uc_umac_error_event_table = le32toh(
				    resp4->umac_data.dbg_ptrs.error_info_addr);
				if (resp4->status == IWX_ALIVE_STATUS_OK)
					sc->sc_uc.uc_ok = 1;
				else
					sc->sc_uc.uc_ok = 0;
			}

			sc->sc_uc.uc_intr = 1;
			wakeup(&sc->sc_uc);
			break;
		}

		case IWX_STATISTICS_NOTIFICATION: {
			struct iwx_notif_statistics *stats;
			SYNC_RESP_STRUCT(stats, pkt);
			memcpy(&sc->sc_stats, stats, sizeof(sc->sc_stats));
			sc->sc_noise = iwx_get_noise(&stats->rx.general);
			break;
		}

		case IWX_DTS_MEASUREMENT_NOTIFICATION:
		case IWX_WIDE_ID(IWX_PHY_OPS_GROUP,
				 IWX_DTS_MEASUREMENT_NOTIF_WIDE):
			break;

		case IWX_PHY_CONFIGURATION_CMD:
		case IWX_TX_ANT_CONFIGURATION_CMD:
		case IWX_ADD_STA:
		case IWX_MAC_CONTEXT_CMD:
		case IWX_REPLY_SF_CFG_CMD:
		case IWX_POWER_TABLE_CMD:
		case IWX_LTR_CONFIG:
		case IWX_PHY_CONTEXT_CMD:
		case IWX_BINDING_CONTEXT_CMD:
		case IWX_WIDE_ID(IWX_LONG_GROUP, IWX_SCAN_CFG_CMD):
		case IWX_WIDE_ID(IWX_LONG_GROUP, IWX_SCAN_REQ_UMAC):
		case IWX_WIDE_ID(IWX_LONG_GROUP, IWX_SCAN_ABORT_UMAC):
		case IWX_REPLY_BEACON_FILTERING_CMD:
		case IWX_MAC_PM_POWER_TABLE:
		case IWX_TIME_QUOTA_CMD:
		case IWX_REMOVE_STA:
		case IWX_TXPATH_FLUSH:
		case IWX_BT_CONFIG:
		case IWX_NVM_ACCESS_CMD:
		case IWX_MCC_UPDATE_CMD:
		case IWX_TIME_EVENT_CMD:
		case IWX_STATISTICS_CMD:
		case IWX_SCD_QUEUE_CFG: {
			size_t pkt_len;

			if (sc->sc_cmd_resp_pkt[idx] == NULL)
				break;

			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    sizeof(*pkt), BUS_DMASYNC_POSTREAD);

			pkt_len = sizeof(pkt->len_n_flags) +
			    iwx_rx_packet_len(pkt);

			if ((pkt->hdr.flags & IWX_CMD_FAILED_MSK) ||
			    pkt_len < sizeof(*pkt) ||
			    pkt_len > sc->sc_cmd_resp_len[idx]) {
				free(sc->sc_cmd_resp_pkt[idx], M_DEVBUF,
				    sc->sc_cmd_resp_len[idx]);
				sc->sc_cmd_resp_pkt[idx] = NULL;
				break;
			}

			bus_dmamap_sync(sc->sc_dmat, data->map, sizeof(*pkt),
			    pkt_len - sizeof(*pkt), BUS_DMASYNC_POSTREAD);
			memcpy(sc->sc_cmd_resp_pkt[idx], pkt, pkt_len);
			break;
		}

		case IWX_INIT_COMPLETE_NOTIF:
			sc->sc_init_complete |= IWX_INIT_COMPLETE;
			wakeup(&sc->sc_init_complete);
			break;

		case IWX_SCAN_COMPLETE_UMAC: {
			struct iwx_umac_scan_complete *notif;
			SYNC_RESP_STRUCT(notif, pkt);
			iwx_endscan(sc);
			break;
		}

		case IWX_SCAN_ITERATION_COMPLETE_UMAC: {
			struct iwx_umac_scan_iter_complete_notif *notif;
			SYNC_RESP_STRUCT(notif, pkt);
			iwx_endscan(sc);
			break;
		}

		case IWX_REPLY_ERROR: {
			struct iwx_error_resp *resp;
			SYNC_RESP_STRUCT(resp, pkt);
			printf("%s: firmware error 0x%x, cmd 0x%x\n",
				DEVNAME(sc), le32toh(resp->error_type),
				resp->cmd_id);
			break;
		}

		case IWX_TIME_EVENT_NOTIFICATION: {
			struct iwx_time_event_notif *notif;
			uint32_t action;
			SYNC_RESP_STRUCT(notif, pkt);

			if (sc->sc_time_event_uid != le32toh(notif->unique_id))
				break;
			action = le32toh(notif->action);
			if (action & IWX_TE_V2_NOTIF_HOST_EVENT_END)
				sc->sc_flags &= ~IWX_FLAG_TE_ACTIVE;
			break;
		}

		case IWX_WIDE_ID(IWX_SYSTEM_GROUP,
		    IWX_FSEQ_VER_MISMATCH_NOTIFICATION):
		    break;

		/*
		 * Firmware versions 21 and 22 generate some DEBUG_LOG_MSG
		 * messages. Just ignore them for now.
		 */
		case IWX_DEBUG_LOG_MSG:
			break;

		case IWX_MCAST_FILTER_CMD:
			break;

		case IWX_WIDE_ID(IWX_DATA_PATH_GROUP, IWX_DQA_ENABLE_CMD):
			break;

		case IWX_WIDE_ID(IWX_SYSTEM_GROUP, IWX_SOC_CONFIGURATION_CMD):
			break;

		case IWX_WIDE_ID(IWX_SYSTEM_GROUP, IWX_INIT_EXTENDED_CFG_CMD):
			break;

		case IWX_WIDE_ID(IWX_REGULATORY_AND_NVM_GROUP,
		    IWX_NVM_ACCESS_COMPLETE):
			break;

		case IWX_WIDE_ID(IWX_DATA_PATH_GROUP, IWX_RX_NO_DATA_NOTIF):
			break; /* happens in monitor mode; ignore for now */

		default:
			handled = 0;
			printf("%s: unhandled firmware response 0x%x/0x%x "
			    "rx ring %d[%d]\n",
			    DEVNAME(sc), code, pkt->len_n_flags,
			    (qid & ~0x80), idx);
			break;
		}

		/*
		 * uCode sets bit 0x80 when it originates the notification,
		 * i.e. when the notification is not a direct response to a
		 * command sent by the driver. 
		 * For example, uCode issues IWX_REPLY_RX when it sends a
		 * received frame to the driver.
		 */
		if (handled && !(qid & (1 << 7))) {
			iwx_cmd_done(sc, qid, idx, code);
		}

		offset += roundup(len, IWX_FH_RSCSR_FRAME_ALIGN);
	}

	if (m0 && m0 != data->m)
		m_freem(m0);
}

void
iwx_notif_intr(struct iwx_softc *sc)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	uint16_t hw;

	bus_dmamap_sync(sc->sc_dmat, sc->rxq.stat_dma.map,
	    0, sc->rxq.stat_dma.size, BUS_DMASYNC_POSTREAD);

	hw = le16toh(sc->rxq.stat->closed_rb_num) & 0xfff;
	hw &= (IWX_RX_MQ_RING_COUNT - 1);
	while (sc->rxq.cur != hw) {
		struct iwx_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		iwx_rx_pkt(sc, data, &ml);
		sc->rxq.cur = (sc->rxq.cur + 1) % IWX_RX_MQ_RING_COUNT;
	}
	if_input(&sc->sc_ic.ic_if, &ml);

	/*
	 * Tell the firmware what we have processed.
	 * Seems like the hardware gets upset unless we align the write by 8??
	 */
	hw = (hw == 0) ? IWX_RX_MQ_RING_COUNT - 1 : hw - 1;
	IWX_WRITE(sc, IWX_RFH_Q0_FRBDCB_WIDX_TRG, hw & ~7);
}

int
iwx_intr(void *arg)
{
	struct iwx_softc *sc = arg;
	int handled = 0;
	int r1, r2, rv = 0;

	IWX_WRITE(sc, IWX_CSR_INT_MASK, 0);

	if (sc->sc_flags & IWX_FLAG_USE_ICT) {
		uint32_t *ict = sc->ict_dma.vaddr;
		int tmp;

		tmp = htole32(ict[sc->ict_cur]);
		if (!tmp)
			goto out_ena;

		/*
		 * ok, there was something.  keep plowing until we have all.
		 */
		r1 = r2 = 0;
		while (tmp) {
			r1 |= tmp;
			ict[sc->ict_cur] = 0;
			sc->ict_cur = (sc->ict_cur+1) % IWX_ICT_COUNT;
			tmp = htole32(ict[sc->ict_cur]);
		}

		/* this is where the fun begins.  don't ask */
		if (r1 == 0xffffffff)
			r1 = 0;

		/* i am not expected to understand this */
		if (r1 & 0xc0000)
			r1 |= 0x8000;
		r1 = (0xff & r1) | ((0xff00 & r1) << 16);
	} else {
		r1 = IWX_READ(sc, IWX_CSR_INT);
		if (r1 == 0xffffffff || (r1 & 0xfffffff0) == 0xa5a5a5a0)
			goto out;
		r2 = IWX_READ(sc, IWX_CSR_FH_INT_STATUS);
	}
	if (r1 == 0 && r2 == 0) {
		goto out_ena;
	}

	IWX_WRITE(sc, IWX_CSR_INT, r1 | ~sc->sc_intmask);

	if (r1 & IWX_CSR_INT_BIT_ALIVE) {
		int i;

		/* Firmware has now configured the RFH. */
		for (i = 0; i < IWX_RX_MQ_RING_COUNT; i++)
			iwx_update_rx_desc(sc, &sc->rxq, i);
		IWX_WRITE(sc, IWX_RFH_Q0_FRBDCB_WIDX_TRG, 8);
	}

	handled |= (r1 & (IWX_CSR_INT_BIT_ALIVE /*| IWX_CSR_INT_BIT_SCD*/));

	if (r1 & IWX_CSR_INT_BIT_RF_KILL) {
		handled |= IWX_CSR_INT_BIT_RF_KILL;
		iwx_check_rfkill(sc);
		task_add(systq, &sc->init_task);
		rv = 1;
		goto out_ena;
	}

	if (r1 & IWX_CSR_INT_BIT_SW_ERR) {
#if 1 /* usually #ifdef IWX_DEBUG but always enabled for now */
		int i;

		iwx_nic_error(sc);

		/* Dump driver status (TX and RX rings) while we're here. */
		printf("driver status:\n");
		for (i = 0; i < IWX_MAX_QUEUES; i++) {
			struct iwx_tx_ring *ring = &sc->txq[i];
			printf("  tx ring %2d: qid=%-2d cur=%-3d "
			    "queued=%-3d\n",
			    i, ring->qid, ring->cur, ring->queued);
		}
		printf("  rx ring: cur=%d\n", sc->rxq.cur);
		printf("  802.11 state %s\n",
		    ieee80211_state_name[sc->sc_ic.ic_state]);
#endif

		printf("%s: fatal firmware error\n", DEVNAME(sc));
		if ((sc->sc_flags & IWX_FLAG_SHUTDOWN) == 0)
			task_add(systq, &sc->init_task);
		rv = 1;
		goto out;

	}

	if (r1 & IWX_CSR_INT_BIT_HW_ERR) {
		handled |= IWX_CSR_INT_BIT_HW_ERR;
		printf("%s: hardware error, stopping device \n", DEVNAME(sc));
		if ((sc->sc_flags & IWX_FLAG_SHUTDOWN) == 0) {
			sc->sc_flags |= IWX_FLAG_HW_ERR;
			task_add(systq, &sc->init_task);
		}
		rv = 1;
		goto out;
	}

	/* firmware chunk loaded */
	if (r1 & IWX_CSR_INT_BIT_FH_TX) {
		IWX_WRITE(sc, IWX_CSR_FH_INT_STATUS, IWX_CSR_FH_INT_TX_MASK);
		handled |= IWX_CSR_INT_BIT_FH_TX;

		sc->sc_fw_chunk_done = 1;
		wakeup(&sc->sc_fw);
	}

	if (r1 & (IWX_CSR_INT_BIT_FH_RX | IWX_CSR_INT_BIT_SW_RX |
	    IWX_CSR_INT_BIT_RX_PERIODIC)) {
		if (r1 & (IWX_CSR_INT_BIT_FH_RX | IWX_CSR_INT_BIT_SW_RX)) {
			handled |= (IWX_CSR_INT_BIT_FH_RX | IWX_CSR_INT_BIT_SW_RX);
			IWX_WRITE(sc, IWX_CSR_FH_INT_STATUS, IWX_CSR_FH_INT_RX_MASK);
		}
		if (r1 & IWX_CSR_INT_BIT_RX_PERIODIC) {
			handled |= IWX_CSR_INT_BIT_RX_PERIODIC;
			IWX_WRITE(sc, IWX_CSR_INT, IWX_CSR_INT_BIT_RX_PERIODIC);
		}

		/* Disable periodic interrupt; we use it as just a one-shot. */
		IWX_WRITE_1(sc, IWX_CSR_INT_PERIODIC_REG, IWX_CSR_INT_PERIODIC_DIS);

		/*
		 * Enable periodic interrupt in 8 msec only if we received
		 * real RX interrupt (instead of just periodic int), to catch
		 * any dangling Rx interrupt.  If it was just the periodic
		 * interrupt, there was no dangling Rx activity, and no need
		 * to extend the periodic interrupt; one-shot is enough.
		 */
		if (r1 & (IWX_CSR_INT_BIT_FH_RX | IWX_CSR_INT_BIT_SW_RX))
			IWX_WRITE_1(sc, IWX_CSR_INT_PERIODIC_REG,
			    IWX_CSR_INT_PERIODIC_ENA);

		iwx_notif_intr(sc);
	}

	rv = 1;

 out_ena:
	iwx_restore_interrupts(sc);
 out:
	return rv;
}

int
iwx_intr_msix(void *arg)
{
	struct iwx_softc *sc = arg;
	uint32_t inta_fh, inta_hw;
	int vector = 0;

	inta_fh = IWX_READ(sc, IWX_CSR_MSIX_FH_INT_CAUSES_AD);
	inta_hw = IWX_READ(sc, IWX_CSR_MSIX_HW_INT_CAUSES_AD);
	IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_CAUSES_AD, inta_fh);
	IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_CAUSES_AD, inta_hw);
	inta_fh &= sc->sc_fh_mask;
	inta_hw &= sc->sc_hw_mask;

	if (inta_fh & IWX_MSIX_FH_INT_CAUSES_Q0 ||
	    inta_fh & IWX_MSIX_FH_INT_CAUSES_Q1) {
		iwx_notif_intr(sc);
	}

	/* firmware chunk loaded */
	if (inta_fh & IWX_MSIX_FH_INT_CAUSES_D2S_CH0_NUM) {
		sc->sc_fw_chunk_done = 1;
		wakeup(&sc->sc_fw);
	}

	if ((inta_fh & IWX_MSIX_FH_INT_CAUSES_FH_ERR) ||
	    (inta_hw & IWX_MSIX_HW_INT_CAUSES_REG_SW_ERR) ||
	    (inta_hw & IWX_MSIX_HW_INT_CAUSES_REG_SW_ERR_V2)) {
#if 1 /* usually #ifdef IWX_DEBUG but always enabled for now */
		int i;

		iwx_nic_error(sc);

		/* Dump driver status (TX and RX rings) while we're here. */
		printf("driver status:\n");
		for (i = 0; i < IWX_MAX_QUEUES; i++) {
			struct iwx_tx_ring *ring = &sc->txq[i];
			printf("  tx ring %2d: qid=%-2d cur=%-3d "
			    "queued=%-3d\n",
			    i, ring->qid, ring->cur, ring->queued);
		}
		printf("  rx ring: cur=%d\n", sc->rxq.cur);
		printf("  802.11 state %s\n",
		    ieee80211_state_name[sc->sc_ic.ic_state]);
#endif

		printf("%s: fatal firmware error\n", DEVNAME(sc));
		if ((sc->sc_flags & IWX_FLAG_SHUTDOWN) == 0)
			task_add(systq, &sc->init_task);
		return 1;
	}

	if (inta_hw & IWX_MSIX_HW_INT_CAUSES_REG_RF_KILL) {
		iwx_check_rfkill(sc);
		task_add(systq, &sc->init_task);
	}

	if (inta_hw & IWX_MSIX_HW_INT_CAUSES_REG_HW_ERR) {
		printf("%s: hardware error, stopping device \n", DEVNAME(sc));
		if ((sc->sc_flags & IWX_FLAG_SHUTDOWN) == 0) {
			sc->sc_flags |= IWX_FLAG_HW_ERR;
			task_add(systq, &sc->init_task);
		}
		return 1;
	}

	if (inta_hw & IWX_MSIX_HW_INT_CAUSES_REG_ALIVE) {
		int i;

		/* Firmware has now configured the RFH. */
		for (i = 0; i < IWX_RX_MQ_RING_COUNT; i++)
			iwx_update_rx_desc(sc, &sc->rxq, i);
		IWX_WRITE(sc, IWX_RFH_Q0_FRBDCB_WIDX_TRG, 8);
	}

	/*
	 * Before sending the interrupt the HW disables it to prevent
	 * a nested interrupt. This is done by writing 1 to the corresponding
	 * bit in the mask register. After handling the interrupt, it should be
	 * re-enabled by clearing this bit. This register is defined as
	 * write 1 clear (W1C) register, meaning that it's being clear
	 * by writing 1 to the bit.
	 */
	IWX_WRITE(sc, IWX_CSR_MSIX_AUTOMASK_ST_AD, 1 << vector);
	return 1;
}

typedef void *iwx_match_t;

static const struct pci_matchid iwx_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_22500_1 },
};

int
iwx_match(struct device *parent, iwx_match_t match __unused, void *aux)
{
	return pci_matchbyid((struct pci_attach_args *)aux, iwx_devices,
	    nitems(iwx_devices));
}

int
iwx_preinit(struct iwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	int err;
	static int attached;

	err = iwx_prepare_card_hw(sc);
	if (err) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return err;
	}

	if (attached) {
		/* Update MAC in case the upper layers changed it. */
		IEEE80211_ADDR_COPY(sc->sc_ic.ic_myaddr,
		    ((struct arpcom *)ifp)->ac_enaddr);
		return 0;
	}

	err = iwx_start_hw(sc);
	if (err) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return err;
	}

	err = iwx_run_init_mvm_ucode(sc, 1);
	iwx_stop_device(sc);
	if (err)
		return err;

	/* Print version info and MAC address on first successful fw load. */
	attached = 1;
	printf("%s: hw rev 0x%x, fw ver %s, address %s\n",
	    DEVNAME(sc), sc->sc_hw_rev & IWX_CSR_HW_REV_TYPE_MSK,
	    sc->sc_fwver, ether_sprintf(sc->sc_nvm.hw_addr));

	if (sc->sc_nvm.sku_cap_11n_enable)
		iwx_setup_ht_rates(sc);

	/* not all hardware can do 5GHz band */
	if (!sc->sc_nvm.sku_cap_band_52GHz_enable)
		memset(&ic->ic_sup_rates[IEEE80211_MODE_11A], 0,
		    sizeof(ic->ic_sup_rates[IEEE80211_MODE_11A]));

	/* Configure channel information obtained from firmware. */
	ieee80211_channel_init(ifp);

	/* Configure MAC address. */
	err = if_setlladdr(ifp, ic->ic_myaddr);
	if (err)
		printf("%s: could not set MAC address (error %d)\n",
		    DEVNAME(sc), err);

	ieee80211_media_init(ifp, iwx_media_change, ieee80211_media_status);

	return 0;
}

void
iwx_attach_hook(struct device *self)
{
	struct iwx_softc *sc = (void *)self;

	KASSERT(!cold);

	iwx_preinit(sc);
}

void
iwx_attach(struct device *parent, struct device *self, void *aux)
{
	struct iwx_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	pcireg_t reg, memtype;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	const char *intrstr;
	int err;
	int txq_i, i;

	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	rw_init(&sc->ioctl_rwl, "iwxioctl");

	err = pci_get_capability(sc->sc_pct, sc->sc_pcitag,
	    PCI_CAP_PCIEXPRESS, &sc->sc_cap_off, NULL);
	if (err == 0) {
		printf("%s: PCIe capability structure not found!\n",
		    DEVNAME(sc));
		return;
	}

	/* Clear device-specific "PCI retry timeout" register (41h). */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, reg & ~0xff00);

	/* Enable bus-mastering and hardware bug workaround. */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	/* if !MSI */
	if (reg & PCI_COMMAND_INTERRUPT_DISABLE) {
		reg &= ~PCI_COMMAND_INTERRUPT_DISABLE;
	}
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG, reg);

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	err = pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &sc->sc_sz, 0);
	if (err) {
		printf("%s: can't map mem space\n", DEVNAME(sc));
		return;
	}

	if (pci_intr_map_msix(pa, 0, &ih) == 0) {
		sc->sc_msix = 1;
	} else if (pci_intr_map_msi(pa, &ih) && pci_intr_map(pa, &ih)) {
		printf("%s: can't map interrupt\n", DEVNAME(sc));
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih);
	if (sc->sc_msix)
		sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET,
		    iwx_intr_msix, sc, DEVNAME(sc));
	else
		sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET,
		    iwx_intr, sc, DEVNAME(sc));

	if (sc->sc_ih == NULL) {
		printf("\n");
		printf("%s: can't establish interrupt", DEVNAME(sc));
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(", %s\n", intrstr);

	iwx_disable_interrupts(sc);

	sc->sc_hw_rev = IWX_READ(sc, IWX_CSR_HW_REV);
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_WL_22500_1:
		sc->sc_fwname = "iwx-cc-a0-46";
		sc->sc_device_family = IWX_DEVICE_FAMILY_22000;
		sc->sc_fwdmasegsz = IWX_FWDMASEGSZ_8000;
		sc->sc_nvm_max_section_size = 32768;
		sc->sc_integrated = 1;
		sc->sc_ltr_delay = IWX_SOC_FLAGS_LTR_APPLY_DELAY_NONE;
		sc->sc_low_latency_xtal = 0;
		sc->sc_xtal_latency = 0;
		sc->sc_tx_with_siso_diversity = 0;
		break;
	default:
		printf("%s: unknown adapter type\n", DEVNAME(sc));
		return;
	}

	/*
	 * In the 8000 HW family the format of the 4 bytes of CSR_HW_REV have
	 * changed, and now the revision step also includes bit 0-1 (no more
	 * "dash" value). To keep hw_rev backwards compatible - we'll store it
	 * in the old format.
	 */
	sc->sc_hw_rev = (sc->sc_hw_rev & 0xfff0) |
			(IWX_CSR_HW_REV_STEP(sc->sc_hw_rev << 2) << 2);
	
	if (iwx_prepare_card_hw(sc) != 0) {
		printf("%s: could not initialize hardware\n",
		    DEVNAME(sc));
		return;
	}

	/*
	 * In order to recognize C step the driver should read the
	 * chip version id located at the AUX bus MISC address.
	 */
	IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
		    IWX_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);
	DELAY(2);

	err = iwx_poll_bit(sc, IWX_CSR_GP_CNTRL,
			   IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
			   IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
			   25000);
	if (!err) {
		printf("%s: Failed to wake up the nic\n", DEVNAME(sc));
		return;
	}

	if (iwx_nic_lock(sc)) {
		uint32_t hw_step = iwx_read_prph(sc, IWX_WFPM_CTRL_REG);
		hw_step |= IWX_ENABLE_WFPM;
		iwx_write_prph(sc, IWX_WFPM_CTRL_REG, hw_step);
		hw_step = iwx_read_prph(sc, IWX_AUX_MISC_REG);
		hw_step = (hw_step >> IWX_HW_STEP_LOCATION_BITS) & 0xF;
		if (hw_step == 0x3)
			sc->sc_hw_rev = (sc->sc_hw_rev & 0xFFFFFFF3) |
					(IWX_SILICON_C_STEP << 2);
		iwx_nic_unlock(sc);
	} else {
		printf("%s: Failed to lock the nic\n", DEVNAME(sc));
		return;
	}

	/* 
	 * Allocate DMA memory for firmware transfers.
	 * Must be aligned on a 16-byte boundary.
	 */
	err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->fw_dma,
	    sc->sc_fwdmasegsz, 16);
	if (err) {
		printf("%s: could not allocate memory for firmware\n",
		    DEVNAME(sc));
		return;
	}

	/* Allocate interrupt cause table (ICT).*/
	err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->ict_dma,
	    IWX_ICT_SIZE, 1<<IWX_ICT_PADDR_SHIFT);
	if (err) {
		printf("%s: could not allocate ICT table\n", DEVNAME(sc));
		goto fail1;
	}

	/* TX scheduler rings must be aligned on a 1KB boundary. */
	err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->sched_dma,
	    nitems(sc->txq) * sizeof(struct iwx_agn_scd_bc_tbl), 1024);
	if (err) {
		printf("%s: could not allocate TX scheduler rings\n",
		    DEVNAME(sc));
		goto fail3;
	}

	for (txq_i = 0; txq_i < nitems(sc->txq); txq_i++) {
		err = iwx_alloc_tx_ring(sc, &sc->txq[txq_i], txq_i);
		if (err) {
			printf("%s: could not allocate TX ring %d\n",
			    DEVNAME(sc), txq_i);
			goto fail4;
		}
	}

	err = iwx_alloc_rx_ring(sc, &sc->rxq);
	if (err) {
		printf("%s: could not allocate RX ring\n", DEVNAME(sc));
		goto fail4;
	}

	sc->sc_nswq = taskq_create("iwxns", 1, IPL_NET, 0);
	if (sc->sc_nswq == NULL)
		goto fail4;

	/* Clear pending interrupts. */
	IWX_WRITE(sc, IWX_CSR_INT, 0xffffffff);

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_WEP |		/* WEP */
	    IEEE80211_C_RSN |		/* WPA/RSN */
	    IEEE80211_C_SCANALL |	/* device scans all channels at once */
	    IEEE80211_C_SCANALLBAND |	/* device scans all bands at once */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE;	/* short preamble supported */

	ic->ic_htcaps = IEEE80211_HTCAP_SGI20;
	ic->ic_htcaps |=
	    (IEEE80211_HTCAP_SMPS_DIS << IEEE80211_HTCAP_SMPS_SHIFT);
	ic->ic_htxcaps = 0;
	ic->ic_txbfcaps = 0;
	ic->ic_aselcaps = 0;
	ic->ic_ampdu_params = (IEEE80211_AMPDU_PARAM_SS_4 | 0x3 /* 64k */);

	ic->ic_sup_rates[IEEE80211_MODE_11A] = ieee80211_std_rateset_11a;
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	for (i = 0; i < nitems(sc->sc_phyctxt); i++) {
		sc->sc_phyctxt[i].id = i;
	}

	sc->sc_amrr.amrr_min_success_threshold =  1;
	sc->sc_amrr.amrr_max_success_threshold = 15;

	/* IBSS channel undefined for now. */
	ic->ic_ibss_chan = &ic->ic_channels[1];

	ic->ic_max_rssi = IWX_MAX_DBM - IWX_MIN_DBM;

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = iwx_ioctl;
	ifp->if_start = iwx_start;
	ifp->if_watchdog = iwx_watchdog;
	memcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ieee80211_media_init(ifp, iwx_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	iwx_radiotap_attach(sc);
#endif
	timeout_set(&sc->sc_calib_to, iwx_calib_timeout, sc);
	task_set(&sc->init_task, iwx_init_task, sc);
	task_set(&sc->newstate_task, iwx_newstate_task, sc);
	task_set(&sc->ba_task, iwx_ba_task, sc);
	task_set(&sc->htprot_task, iwx_htprot_task, sc);

	ic->ic_node_alloc = iwx_node_alloc;
#ifdef notyet
	/* TODO: background scans trigger firmware errors */
	ic->ic_bgscan_start = iwx_bgscan;
#endif

	/* Override 802.11 state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwx_newstate;
	ic->ic_update_htprot = iwx_update_htprot;
	ic->ic_ampdu_rx_start = iwx_ampdu_rx_start;
	ic->ic_ampdu_rx_stop = iwx_ampdu_rx_stop;
#ifdef notyet
	ic->ic_ampdu_tx_start = iwx_ampdu_tx_start;
	ic->ic_ampdu_tx_stop = iwx_ampdu_tx_stop;
#endif
	/*
	 * We cannot read the MAC address without loading the
	 * firmware from disk. Postpone until mountroot is done.
	 */
	config_mountroot(self, iwx_attach_hook);

	return;

fail4:	while (--txq_i >= 0)
		iwx_free_tx_ring(sc, &sc->txq[txq_i]);
	iwx_free_rx_ring(sc, &sc->rxq);
	iwx_dma_contig_free(&sc->sched_dma);
fail3:	if (sc->ict_dma.vaddr != NULL)
		iwx_dma_contig_free(&sc->ict_dma);
	
fail1:	iwx_dma_contig_free(&sc->fw_dma);
	return;
}

#if NBPFILTER > 0
void
iwx_radiotap_attach(struct iwx_softc *sc)
{
	bpfattach(&sc->sc_drvbpf, &sc->sc_ic.ic_if, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWX_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWX_TX_RADIOTAP_PRESENT);
}
#endif

void
iwx_init_task(void *arg1)
{
	struct iwx_softc *sc = arg1;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s = splnet();
	int generation = sc->sc_generation;
	int fatal = (sc->sc_flags & (IWX_FLAG_HW_ERR | IWX_FLAG_RFKILL));

	rw_enter_write(&sc->ioctl_rwl);
	if (generation != sc->sc_generation) {
		rw_exit(&sc->ioctl_rwl);
		splx(s);
		return;
	}

	if (ifp->if_flags & IFF_RUNNING)
		iwx_stop(ifp);
	else
		sc->sc_flags &= ~IWX_FLAG_HW_ERR;

	if (!fatal && (ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP)
		iwx_init(ifp);

	rw_exit(&sc->ioctl_rwl);
	splx(s);
}

int
iwx_resume(struct iwx_softc *sc)
{
	pcireg_t reg;

	/* Clear device-specific "PCI retry timeout" register (41h). */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, reg & ~0xff00);

	/* reconfigure the MSI-X mapping to get the correct IRQ for rfkill */
	iwx_conf_msix_hw(sc, 0);

	iwx_enable_rfkill_int(sc);
	iwx_check_rfkill(sc);

	return iwx_prepare_card_hw(sc);
}

int
iwx_activate(struct device *self, int act)
{
	struct iwx_softc *sc = (struct iwx_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int err = 0;

	switch (act) {
	case DVACT_QUIESCE:
		if (ifp->if_flags & IFF_RUNNING) {
			rw_enter_write(&sc->ioctl_rwl);
			iwx_stop(ifp);
			rw_exit(&sc->ioctl_rwl);
		}
		break;
	case DVACT_RESUME:
		err = iwx_resume(sc);
		if (err)
			printf("%s: could not initialize hardware\n",
			    DEVNAME(sc));
		break;
	case DVACT_WAKEUP:
		/* Hardware should be up at this point. */
		if (iwx_set_hw_ready(sc))
			task_add(systq, &sc->init_task);
		break;
	}

	return 0;
}

struct cfdriver iwx_cd = {
	NULL, "iwx", DV_IFNET
};

struct cfattach iwx_ca = {
	sizeof(struct iwx_softc), iwx_match, iwx_attach,
	NULL, iwx_activate
};
