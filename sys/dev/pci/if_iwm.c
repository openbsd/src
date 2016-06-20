/*	$OpenBSD: if_iwm.c,v 1.89 2016/06/20 11:54:36 stsp Exp $	*/

/*
 * Copyright (c) 2014, 2016 genua gmbh <info@genua.de>
 *   Author: Stefan Sperling <stsp@openbsd.org>
 * Copyright (c) 2014 Fixup Software Ltd.
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
 ***********************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2013 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 Intel Deutschland GmbH
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 Intel Deutschland GmbH
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
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>

#include <sys/task.h>
#include <machine/bus.h>
#include <machine/endian.h>
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
#include <net80211/ieee80211_radiotap.h>

#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

#define IC2IFP(_ic_) (&(_ic_)->ic_if)

#define le16_to_cpup(_a_) (le16toh(*(const uint16_t *)(_a_)))
#define le32_to_cpup(_a_) (le32toh(*(const uint32_t *)(_a_)))

#ifdef IWM_DEBUG
#define DPRINTF(x)	do { if (iwm_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (iwm_debug >= (n)) printf x; } while (0)
int iwm_debug = 1;
#else
#define DPRINTF(x)	do { ; } while (0)
#define DPRINTFN(n, x)	do { ; } while (0)
#endif

#include <dev/pci/if_iwmreg.h>
#include <dev/pci/if_iwmvar.h>

const uint8_t iwm_nvm_channels[] = {
	/* 2.4 GHz */
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	/* 5 GHz */
	36, 40, 44 , 48, 52, 56, 60, 64,
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
	149, 153, 157, 161, 165
};

const uint8_t iwm_nvm_channels_8000[] = {
	/* 2.4 GHz */
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	/* 5 GHz */
	36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92,
	96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
	149, 153, 157, 161, 165, 169, 173, 177, 181
};

#define IWM_NUM_2GHZ_CHANNELS	14

const struct iwm_rate {
	uint8_t rate;
	uint8_t plcp;
	uint8_t ht_plcp;
} iwm_rates[] = {
		/* Legacy */		/* HT */
	{   2,	IWM_RATE_1M_PLCP,	IWM_RATE_HT_SISO_MCS_INV_PLCP  },
	{   4,	IWM_RATE_2M_PLCP,	IWM_RATE_HT_SISO_MCS_INV_PLCP },
	{  11,	IWM_RATE_5M_PLCP,	IWM_RATE_HT_SISO_MCS_INV_PLCP  },
	{  22,	IWM_RATE_11M_PLCP,	IWM_RATE_HT_SISO_MCS_INV_PLCP },
	{  12,	IWM_RATE_6M_PLCP,	IWM_RATE_HT_SISO_MCS_0_PLCP },
	{  18,	IWM_RATE_9M_PLCP,	IWM_RATE_HT_SISO_MCS_INV_PLCP  },
	{  24,	IWM_RATE_12M_PLCP,	IWM_RATE_HT_SISO_MCS_1_PLCP },
	{  36,	IWM_RATE_18M_PLCP,	IWM_RATE_HT_SISO_MCS_2_PLCP },
	{  48,	IWM_RATE_24M_PLCP,	IWM_RATE_HT_SISO_MCS_3_PLCP },
	{  72,	IWM_RATE_36M_PLCP,	IWM_RATE_HT_SISO_MCS_4_PLCP },
	{  96,	IWM_RATE_48M_PLCP,	IWM_RATE_HT_SISO_MCS_5_PLCP },
	{ 108,	IWM_RATE_54M_PLCP,	IWM_RATE_HT_SISO_MCS_6_PLCP },
	{ 128,	IWM_RATE_INVM_PLCP,	IWM_RATE_HT_SISO_MCS_7_PLCP },
};
#define IWM_RIDX_CCK	0
#define IWM_RIDX_OFDM	4
#define IWM_RIDX_MAX	(nitems(iwm_rates)-1)
#define IWM_RIDX_IS_CCK(_i_) ((_i_) < IWM_RIDX_OFDM)
#define IWM_RIDX_IS_OFDM(_i_) ((_i_) >= IWM_RIDX_OFDM)

/* Convert an MCS index into an iwm_rates[] index. */
const int iwm_mcs2ridx[] = {
	IWM_RATE_MCS_0_INDEX,
	IWM_RATE_MCS_1_INDEX,
	IWM_RATE_MCS_2_INDEX,
	IWM_RATE_MCS_3_INDEX,
	IWM_RATE_MCS_4_INDEX,
	IWM_RATE_MCS_5_INDEX,
	IWM_RATE_MCS_6_INDEX,
	IWM_RATE_MCS_7_INDEX,
};

struct iwm_nvm_section {
	uint16_t length;
	uint8_t *data;
};

int	iwm_store_cscheme(struct iwm_softc *, uint8_t *, size_t);
int	iwm_firmware_store_section(struct iwm_softc *, enum iwm_ucode_type,
					uint8_t *, size_t);
int	iwm_set_default_calib(struct iwm_softc *, const void *);
void	iwm_fw_info_free(struct iwm_fw_info *);
int	iwm_read_firmware(struct iwm_softc *, enum iwm_ucode_type);
uint32_t iwm_read_prph(struct iwm_softc *, uint32_t);
void	iwm_write_prph(struct iwm_softc *, uint32_t, uint32_t);
int	iwm_read_mem(struct iwm_softc *, uint32_t, void *, int);
int	iwm_write_mem(struct iwm_softc *, uint32_t, const void *, int);
int	iwm_write_mem32(struct iwm_softc *, uint32_t, uint32_t);
int	iwm_poll_bit(struct iwm_softc *, int, uint32_t, uint32_t, int);
int	iwm_nic_lock(struct iwm_softc *);
void	iwm_nic_unlock(struct iwm_softc *);
void	iwm_set_bits_mask_prph(struct iwm_softc *, uint32_t, uint32_t,
		    uint32_t);
void	iwm_set_bits_prph(struct iwm_softc *, uint32_t, uint32_t);
void	iwm_clear_bits_prph(struct iwm_softc *, uint32_t, uint32_t);
int	iwm_dma_contig_alloc(bus_dma_tag_t, struct iwm_dma_info *,
				bus_size_t, bus_size_t);
void	iwm_dma_contig_free(struct iwm_dma_info *);
int	iwm_alloc_fwmem(struct iwm_softc *);
void	iwm_free_fwmem(struct iwm_softc *);
int	iwm_alloc_sched(struct iwm_softc *);
void	iwm_free_sched(struct iwm_softc *);
int	iwm_alloc_kw(struct iwm_softc *);
void	iwm_free_kw(struct iwm_softc *);
int	iwm_alloc_ict(struct iwm_softc *);
void	iwm_free_ict(struct iwm_softc *);
int	iwm_alloc_rx_ring(struct iwm_softc *, struct iwm_rx_ring *);
void	iwm_disable_rx_dma(struct iwm_softc *);
void	iwm_reset_rx_ring(struct iwm_softc *, struct iwm_rx_ring *);
void	iwm_free_rx_ring(struct iwm_softc *, struct iwm_rx_ring *);
int	iwm_alloc_tx_ring(struct iwm_softc *, struct iwm_tx_ring *, int);
void	iwm_reset_tx_ring(struct iwm_softc *, struct iwm_tx_ring *);
void	iwm_free_tx_ring(struct iwm_softc *, struct iwm_tx_ring *);
void	iwm_enable_rfkill_int(struct iwm_softc *);
int	iwm_check_rfkill(struct iwm_softc *);
void	iwm_enable_interrupts(struct iwm_softc *);
void	iwm_restore_interrupts(struct iwm_softc *);
void	iwm_disable_interrupts(struct iwm_softc *);
void	iwm_ict_reset(struct iwm_softc *);
int	iwm_set_hw_ready(struct iwm_softc *);
int	iwm_prepare_card_hw(struct iwm_softc *);
void	iwm_apm_config(struct iwm_softc *);
int	iwm_apm_init(struct iwm_softc *);
void	iwm_apm_stop(struct iwm_softc *);
int	iwm_allow_mcast(struct iwm_softc *);
int	iwm_start_hw(struct iwm_softc *);
void	iwm_stop_device(struct iwm_softc *);
void	iwm_set_pwr(struct iwm_softc *);
void	iwm_mvm_nic_config(struct iwm_softc *);
int	iwm_nic_rx_init(struct iwm_softc *);
int	iwm_nic_tx_init(struct iwm_softc *);
int	iwm_nic_init(struct iwm_softc *);
int	iwm_enable_txq(struct iwm_softc *, int, int, int);
int	iwm_post_alive(struct iwm_softc *);
struct iwm_phy_db_entry *iwm_phy_db_get_section(struct iwm_softc *,
					enum iwm_phy_db_section_type, uint16_t);
int	iwm_phy_db_set_section(struct iwm_softc *,
				struct iwm_calib_res_notif_phy_db *);
int	iwm_is_valid_channel(uint16_t);
uint8_t	iwm_ch_id_to_ch_index(uint16_t);
uint16_t iwm_channel_id_to_papd(uint16_t);
uint16_t iwm_channel_id_to_txp(struct iwm_softc *, uint16_t);
int	iwm_phy_db_get_section_data(struct iwm_softc *, uint32_t, uint8_t **,
					uint16_t *, uint16_t);
int	iwm_send_phy_db_cmd(struct iwm_softc *, uint16_t, uint16_t, void *);
int	iwm_phy_db_send_all_channel_groups(struct iwm_softc *,
		enum iwm_phy_db_section_type, uint8_t);
int	iwm_send_phy_db_data(struct iwm_softc *);
void	iwm_mvm_te_v2_to_v1(const struct iwm_time_event_cmd_v2 *,
				struct iwm_time_event_cmd_v1 *);
int	iwm_mvm_send_time_event_cmd(struct iwm_softc *,
					const struct iwm_time_event_cmd_v2 *);
int	iwm_mvm_time_event_send_add(struct iwm_softc *, struct iwm_node *,
					void *, struct iwm_time_event_cmd_v2 *);
void	iwm_mvm_protect_session(struct iwm_softc *, struct iwm_node *,
				uint32_t, uint32_t);
int	iwm_nvm_read_chunk(struct iwm_softc *, uint16_t, uint16_t, uint16_t,
				uint8_t *, uint16_t *);
int	iwm_nvm_read_section(struct iwm_softc *, uint16_t, uint8_t *,
				uint16_t *, size_t);
void	iwm_init_channel_map(struct iwm_softc *, const uint16_t * const,
				const uint8_t *nvm_channels, size_t nchan);
void	iwm_setup_ht_rates(struct iwm_softc *);
void	iwm_htprot_task(void *);
void	iwm_update_htprot(struct ieee80211com *, struct ieee80211_node *);
int	iwm_ampdu_rx_start(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
void	iwm_ampdu_rx_stop(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
void	iwm_mvm_sta_rx_agg(struct iwm_softc *, struct ieee80211_node *,
				uint8_t, uint16_t, int);
#ifdef notyet
int	iwm_ampdu_tx_start(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
void	iwm_ampdu_tx_stop(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
#endif
void	iwm_ba_task(void *);

int	iwm_parse_nvm_data(struct iwm_softc *, const uint16_t *,
				const uint16_t *, const uint16_t *,
				const uint16_t *, const uint16_t *,
				const uint16_t *);
void	iwm_set_hw_address_8000(struct iwm_softc *, struct iwm_nvm_data *,
				const uint16_t *, const uint16_t *);
int	iwm_parse_nvm_sections(struct iwm_softc *, struct iwm_nvm_section *);
int	iwm_nvm_init(struct iwm_softc *);
int	iwm_firmware_load_sect(struct iwm_softc *, uint32_t, const uint8_t *,
				uint32_t);
int	iwm_firmware_load_chunk(struct iwm_softc *, uint32_t, const uint8_t *,
				uint32_t);
int	iwm_load_firmware_7000(struct iwm_softc *, enum iwm_ucode_type);
int	iwm_load_cpu_sections_8000(struct iwm_softc *, struct iwm_fw_sects *,
					int , int *);
int	iwm_load_firmware_8000(struct iwm_softc *, enum iwm_ucode_type);
int	iwm_load_firmware(struct iwm_softc *, enum iwm_ucode_type);
int	iwm_start_fw(struct iwm_softc *, enum iwm_ucode_type);
int	iwm_send_tx_ant_cfg(struct iwm_softc *, uint8_t);
int	iwm_send_phy_cfg_cmd(struct iwm_softc *);
int	iwm_mvm_load_ucode_wait_alive(struct iwm_softc *, enum iwm_ucode_type);
int	iwm_run_init_mvm_ucode(struct iwm_softc *, int);
int	iwm_rx_addbuf(struct iwm_softc *, int, int);
int	iwm_mvm_calc_rssi(struct iwm_softc *, struct iwm_rx_phy_info *);
int	iwm_mvm_get_signal_strength(struct iwm_softc *,
					struct iwm_rx_phy_info *);
void	iwm_mvm_rx_rx_phy_cmd(struct iwm_softc *, struct iwm_rx_packet *,
				struct iwm_rx_data *);
int	iwm_get_noise(const struct iwm_mvm_statistics_rx_non_phy *);
void	iwm_mvm_rx_rx_mpdu(struct iwm_softc *, struct iwm_rx_packet *,
				struct iwm_rx_data *);
void	iwm_mvm_rx_tx_cmd_single(struct iwm_softc *, struct iwm_rx_packet *,
				struct iwm_node *);
void	iwm_mvm_rx_tx_cmd(struct iwm_softc *, struct iwm_rx_packet *,
			struct iwm_rx_data *);
int	iwm_mvm_binding_cmd(struct iwm_softc *, struct iwm_node *, uint32_t);
int	iwm_mvm_binding_update(struct iwm_softc *, struct iwm_node *);
int	iwm_mvm_binding_add_vif(struct iwm_softc *, struct iwm_node *);
void	iwm_mvm_phy_ctxt_cmd_hdr(struct iwm_softc *, struct iwm_mvm_phy_ctxt *,
			struct iwm_phy_context_cmd *, uint32_t, uint32_t);
void	iwm_mvm_phy_ctxt_cmd_data(struct iwm_softc *,
		struct iwm_phy_context_cmd *, struct ieee80211_channel *,
		uint8_t, uint8_t);
int	iwm_mvm_phy_ctxt_apply(struct iwm_softc *, struct iwm_mvm_phy_ctxt *,
				uint8_t, uint8_t, uint32_t, uint32_t);
int	iwm_mvm_phy_ctxt_add(struct iwm_softc *, struct iwm_mvm_phy_ctxt *,
				struct ieee80211_channel *, uint8_t, uint8_t);
int	iwm_mvm_phy_ctxt_changed(struct iwm_softc *, struct iwm_mvm_phy_ctxt *,
				struct ieee80211_channel *, uint8_t, uint8_t);
int	iwm_send_cmd(struct iwm_softc *, struct iwm_host_cmd *);
int	iwm_mvm_send_cmd_pdu(struct iwm_softc *, uint8_t, uint32_t, uint16_t,
				const void *);
int	iwm_mvm_send_cmd_status(struct iwm_softc *, struct iwm_host_cmd *,
				uint32_t *);
int	iwm_mvm_send_cmd_pdu_status(struct iwm_softc *, uint8_t,
					uint16_t, const void *, uint32_t *);
void	iwm_free_resp(struct iwm_softc *, struct iwm_host_cmd *);
void	iwm_cmd_done(struct iwm_softc *, struct iwm_rx_packet *);
void	iwm_update_sched(struct iwm_softc *, int, int, uint8_t, uint16_t);
const struct iwm_rate *iwm_tx_fill_cmd(struct iwm_softc *, struct iwm_node *,
			struct ieee80211_frame *, struct iwm_tx_cmd *);
int	iwm_tx(struct iwm_softc *, struct mbuf *, struct ieee80211_node *, int);
void	iwm_mvm_led_enable(struct iwm_softc *);
void	iwm_mvm_led_disable(struct iwm_softc *);
int	iwm_mvm_led_is_enabled(struct iwm_softc *);
void	iwm_led_blink_timeout(void *);
void	iwm_led_blink_start(struct iwm_softc *);
void	iwm_led_blink_stop(struct iwm_softc *);
int	iwm_mvm_beacon_filter_send_cmd(struct iwm_softc *,
					struct iwm_beacon_filter_cmd *);
void	iwm_mvm_beacon_filter_set_cqm_params(struct iwm_softc *,
			struct iwm_node *, struct iwm_beacon_filter_cmd *);
int	iwm_mvm_update_beacon_abort(struct iwm_softc *, struct iwm_node *, int);
void	iwm_mvm_power_log(struct iwm_softc *, struct iwm_mac_power_cmd *);
void	iwm_mvm_power_build_cmd(struct iwm_softc *, struct iwm_node *,
				struct iwm_mac_power_cmd *);
int	iwm_mvm_power_mac_update_mode(struct iwm_softc *, struct iwm_node *);
int	iwm_mvm_power_update_device(struct iwm_softc *);
int	iwm_mvm_enable_beacon_filter(struct iwm_softc *, struct iwm_node *);
int	iwm_mvm_disable_beacon_filter(struct iwm_softc *);
int	iwm_mvm_send_add_sta_cmd_status(struct iwm_softc *,
					struct iwm_mvm_add_sta_cmd_v7 *, int *);
int	iwm_mvm_sta_send_to_fw(struct iwm_softc *, struct iwm_node *, int);
int	iwm_mvm_add_sta(struct iwm_softc *, struct iwm_node *);
int	iwm_mvm_update_sta(struct iwm_softc *, struct iwm_node *);
int	iwm_mvm_add_int_sta_common(struct iwm_softc *, struct iwm_int_sta *,
				const uint8_t *, uint16_t, uint16_t);
int	iwm_mvm_add_aux_sta(struct iwm_softc *);
uint16_t iwm_mvm_scan_rx_chain(struct iwm_softc *);
uint32_t iwm_mvm_scan_max_out_time(struct iwm_softc *, uint32_t, int);
uint32_t iwm_mvm_scan_suspend_time(struct iwm_softc *, int);
uint32_t iwm_mvm_scan_rate_n_flags(struct iwm_softc *, int, int);
uint16_t iwm_mvm_get_active_dwell(struct iwm_softc *, int, int);
uint16_t iwm_mvm_get_passive_dwell(struct iwm_softc *, int);
uint8_t	iwm_mvm_lmac_scan_fill_channels(struct iwm_softc *,
				struct iwm_scan_channel_cfg_lmac *, int);
int	iwm_mvm_fill_probe_req(struct iwm_softc *, struct iwm_scan_probe_req *);
int	iwm_mvm_lmac_scan(struct iwm_softc *);
int	iwm_mvm_config_umac_scan(struct iwm_softc *);
int	iwm_mvm_umac_scan(struct iwm_softc *);
void	iwm_mvm_ack_rates(struct iwm_softc *, struct iwm_node *, int *, int *);
void	iwm_mvm_mac_ctxt_cmd_common(struct iwm_softc *, struct iwm_node *,
					struct iwm_mac_ctx_cmd *, uint32_t);
int	iwm_mvm_mac_ctxt_send_cmd(struct iwm_softc *, struct iwm_mac_ctx_cmd *);
void	iwm_mvm_mac_ctxt_cmd_fill_sta(struct iwm_softc *, struct iwm_node *,
					struct iwm_mac_data_sta *, int);
int	iwm_mvm_mac_ctxt_cmd_station(struct iwm_softc *, struct iwm_node *,
					uint32_t);
int	iwm_mvm_mac_ctx_send(struct iwm_softc *, struct iwm_node *, uint32_t);
int	iwm_mvm_mac_ctxt_add(struct iwm_softc *, struct iwm_node *);
int	iwm_mvm_mac_ctxt_changed(struct iwm_softc *, struct iwm_node *);
int	iwm_mvm_update_quotas(struct iwm_softc *, struct iwm_node *);
int	iwm_auth(struct iwm_softc *);
int	iwm_assoc(struct iwm_softc *);
int	iwm_release(struct iwm_softc *, struct iwm_node *);
struct ieee80211_node *iwm_node_alloc(struct ieee80211com *);
void	iwm_calib_timeout(void *);
void	iwm_setrates(struct iwm_node *);
int	iwm_media_change(struct ifnet *);
void	iwm_newstate_task(void *);
int	iwm_newstate(struct ieee80211com *, enum ieee80211_state, int);
void	iwm_endscan_cb(void *);
void	iwm_mvm_fill_sf_command(struct iwm_softc *, struct iwm_sf_cfg_cmd *,
	    struct ieee80211_node *);
int	iwm_mvm_sf_config(struct iwm_softc *, enum iwm_sf_state);
int	iwm_send_bt_init_conf(struct iwm_softc *);
int	iwm_send_update_mcc_cmd(struct iwm_softc *, const char *);
void	iwm_mvm_tt_tx_backoff(struct iwm_softc *, uint32_t);
int	iwm_init_hw(struct iwm_softc *);
int	iwm_init(struct ifnet *);
void	iwm_start(struct ifnet *);
void	iwm_stop(struct ifnet *, int);
void	iwm_watchdog(struct ifnet *);
int	iwm_ioctl(struct ifnet *, u_long, caddr_t);
#ifdef IWM_DEBUG
const char *iwm_desc_lookup(uint32_t);
void	iwm_nic_error(struct iwm_softc *);
void	iwm_nic_umac_error(struct iwm_softc *);
#endif
void	iwm_notif_intr(struct iwm_softc *);
int	iwm_intr(void *);
int	iwm_match(struct device *, void *, void *);
int	iwm_preinit(struct iwm_softc *);
void	iwm_attach_hook(struct device *);
void	iwm_attach(struct device *, struct device *, void *);
void	iwm_init_task(void *);
int	iwm_activate(struct device *, int);
void	iwm_wakeup(struct iwm_softc *);

#if NBPFILTER > 0
void	iwm_radiotap_attach(struct iwm_softc *);
#endif

/*
 * Firmware parser.
 */

int
iwm_store_cscheme(struct iwm_softc *sc, uint8_t *data, size_t dlen)
{
	struct iwm_fw_cscheme_list *l = (void *)data;

	if (dlen < sizeof(*l) ||
	    dlen < sizeof(l->size) + l->size * sizeof(*l->cs))
		return EINVAL;

	/* we don't actually store anything for now, always use s/w crypto */

	return 0;
}

int
iwm_firmware_store_section(struct iwm_softc *sc,
	enum iwm_ucode_type type, uint8_t *data, size_t dlen)
{
	struct iwm_fw_sects *fws;
	struct iwm_fw_onesect *fwone;

	if (type >= IWM_UCODE_TYPE_MAX)
		return EINVAL;
	if (dlen < sizeof(uint32_t))
		return EINVAL;

	fws = &sc->sc_fw.fw_sects[type];
	if (fws->fw_count >= IWM_UCODE_SECT_MAX)
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

struct iwm_tlv_calib_data {
	uint32_t ucode_type;
	struct iwm_tlv_calib_ctrl calib;
} __packed;

int
iwm_set_default_calib(struct iwm_softc *sc, const void *data)
{
	const struct iwm_tlv_calib_data *def_calib = data;
	uint32_t ucode_type = le32toh(def_calib->ucode_type);

	if (ucode_type >= IWM_UCODE_TYPE_MAX) {
		DPRINTF(("%s: Wrong ucode_type %u for default "
		    "calibration.\n", DEVNAME(sc), ucode_type));
		return EINVAL;
	}

	sc->sc_default_calib[ucode_type].flow_trigger =
	    def_calib->calib.flow_trigger;
	sc->sc_default_calib[ucode_type].event_trigger =
	    def_calib->calib.event_trigger;

	return 0;
}

void
iwm_fw_info_free(struct iwm_fw_info *fw)
{
	free(fw->fw_rawdata, M_DEVBUF, fw->fw_rawsize);
	fw->fw_rawdata = NULL;
	fw->fw_rawsize = 0;
	/* don't touch fw->fw_status */
	memset(fw->fw_sects, 0, sizeof(fw->fw_sects));
}

int
iwm_read_firmware(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	struct iwm_fw_info *fw = &sc->sc_fw;
	struct iwm_tlv_ucode_header *uhdr;
	struct iwm_ucode_tlv tlv;
	enum iwm_ucode_tlv_type tlv_type;
	uint8_t *data;
	int error;
	size_t len;

	if (fw->fw_status == IWM_FW_STATUS_DONE &&
	    ucode_type != IWM_UCODE_TYPE_INIT)
		return 0;

	while (fw->fw_status == IWM_FW_STATUS_INPROGRESS)
		tsleep(&sc->sc_fw, 0, "iwmfwp", 0);
	fw->fw_status = IWM_FW_STATUS_INPROGRESS;

	if (fw->fw_rawdata != NULL)
		iwm_fw_info_free(fw);

	/*
	 * Load firmware into driver memory.
	 * fw_rawdata and fw_rawsize will be set.
	 */
	error = loadfirmware(sc->sc_fwname,
	    (u_char **)&fw->fw_rawdata, &fw->fw_rawsize);
	if (error != 0) {
		printf("%s: could not read firmware %s (error %d)\n",
		    DEVNAME(sc), sc->sc_fwname, error);
		goto out;
	}

	/* (Re-)Initialize default values. */
	sc->sc_capaflags = 0;
	sc->sc_capa_n_scan_channels = IWM_MAX_NUM_SCAN_CHANNELS;
	memset(sc->sc_enabled_capa, 0, sizeof(sc->sc_enabled_capa));
	memset(sc->sc_fw_mcc, 0, sizeof(sc->sc_fw_mcc));

	/*
	 * Parse firmware contents
	 */

	uhdr = (void *)fw->fw_rawdata;
	if (*(uint32_t *)fw->fw_rawdata != 0
	    || le32toh(uhdr->magic) != IWM_TLV_UCODE_MAGIC) {
		printf("%s: invalid firmware %s\n",
		    DEVNAME(sc), sc->sc_fwname);
		error = EINVAL;
		goto out;
	}

	snprintf(sc->sc_fwver, sizeof(sc->sc_fwver), "%d.%d (API ver %d)",
	    IWM_UCODE_MAJOR(le32toh(uhdr->ver)),
	    IWM_UCODE_MINOR(le32toh(uhdr->ver)),
	    IWM_UCODE_API(le32toh(uhdr->ver)));
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
			error = EINVAL;
			goto parse_out;
		}

		switch ((int)tlv_type) {
		case IWM_UCODE_TLV_PROBE_MAX_LEN:
			if (tlv_len < sizeof(uint32_t)) {
				error = EINVAL;
				goto parse_out;
			}
			sc->sc_capa_max_probe_len
			    = le32toh(*(uint32_t *)tlv_data);
			/* limit it to something sensible */
			if (sc->sc_capa_max_probe_len >
			    IWM_SCAN_OFFLOAD_PROBE_REQ_SIZE) {
				DPRINTF(("%s: IWM_UCODE_TLV_PROBE_MAX_LEN "
				    "ridiculous\n", DEVNAME(sc)));
				error = EINVAL;
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_PAN:
			if (tlv_len) {
				error = EINVAL;
				goto parse_out;
			}
			sc->sc_capaflags |= IWM_UCODE_TLV_FLAGS_PAN;
			break;
		case IWM_UCODE_TLV_FLAGS:
			if (tlv_len < sizeof(uint32_t)) {
				error = EINVAL;
				goto parse_out;
			}
			/*
			 * Apparently there can be many flags, but Linux driver
			 * parses only the first one, and so do we.
			 *
			 * XXX: why does this override IWM_UCODE_TLV_PAN?
			 * Intentional or a bug?  Observations from
			 * current firmware file:
			 *  1) TLV_PAN is parsed first
			 *  2) TLV_FLAGS contains TLV_FLAGS_PAN
			 * ==> this resets TLV_PAN to itself... hnnnk
			 */
			sc->sc_capaflags = le32toh(*(uint32_t *)tlv_data);
			break;
		case IWM_UCODE_TLV_CSCHEME:
			if ((error = iwm_store_cscheme(sc,
			    tlv_data, tlv_len)) != 0)
				goto parse_out;
			break;
		case IWM_UCODE_TLV_NUM_OF_CPU: {
			uint32_t num_cpu;
			if (tlv_len != sizeof(uint32_t)) {
				error = EINVAL;
				goto parse_out;
			}
			num_cpu = le32toh(*(uint32_t *)tlv_data);
			if (num_cpu < 1 || num_cpu > 2) {
				error = EINVAL;
				goto parse_out;
			}
			break;
		}
		case IWM_UCODE_TLV_SEC_RT:
			if ((error = iwm_firmware_store_section(sc,
			    IWM_UCODE_TYPE_REGULAR, tlv_data, tlv_len)) != 0)
				goto parse_out;
			break;
		case IWM_UCODE_TLV_SEC_INIT:
			if ((error = iwm_firmware_store_section(sc,
			    IWM_UCODE_TYPE_INIT, tlv_data, tlv_len)) != 0)
				goto parse_out;
			break;
		case IWM_UCODE_TLV_SEC_WOWLAN:
			if ((error = iwm_firmware_store_section(sc,
			    IWM_UCODE_TYPE_WOW, tlv_data, tlv_len)) != 0)
				goto parse_out;
			break;
		case IWM_UCODE_TLV_DEF_CALIB:
			if (tlv_len != sizeof(struct iwm_tlv_calib_data)) {
				error = EINVAL;
				goto parse_out;
			}
			if ((error = iwm_set_default_calib(sc, tlv_data)) != 0)
				goto parse_out;
			break;
		case IWM_UCODE_TLV_PHY_SKU:
			if (tlv_len != sizeof(uint32_t)) {
				error = EINVAL;
				goto parse_out;
			}
			sc->sc_fw_phy_config = le32toh(*(uint32_t *)tlv_data);
			break;

		case IWM_UCODE_TLV_API_CHANGES_SET: {
			struct iwm_ucode_api *api;
			if (tlv_len != sizeof(*api)) {
				error = EINVAL;
				goto parse_out;
			}
			api = (struct iwm_ucode_api *)tlv_data;
			/* Flags may exceed 32 bits in future firmware. */
			if (le32toh(api->api_index) > 0) {
				DPRINTF(("%s: unsupported API index %d\n",
				    DEVNAME(sc), le32toh(api->api_index)));
				goto parse_out;
			}
			sc->sc_ucode_api = le32toh(api->api_flags);
			break;
		}

		case IWM_UCODE_TLV_ENABLED_CAPABILITIES: {
			struct iwm_ucode_capa *capa;
			int idx, i;
			if (tlv_len != sizeof(*capa)) {
				error = EINVAL;
				goto parse_out;
			}
			capa = (struct iwm_ucode_capa *)tlv_data;
			idx = le32toh(capa->api_index);
			if (idx > howmany(IWM_NUM_UCODE_TLV_CAPA, 32)) {
				DPRINTF(("%s: unsupported API index %d\n",
				    DEVNAME(sc), idx));
				goto parse_out;
			}
			for (i = 0; i < 32; i++) {
				if ((le32toh(capa->api_capa) & (1 << i)) == 0)
					continue;
				setbit(sc->sc_enabled_capa, i + (32 * idx));
			}
			break;
		}

		case 48: /* undocumented TLV */
		case IWM_UCODE_TLV_SDIO_ADMA_ADDR:
		case IWM_UCODE_TLV_FW_GSCAN_CAPA:
			/* ignore, not used by current driver */
			break;

		case IWM_UCODE_TLV_SEC_RT_USNIFFER:
			if ((error = iwm_firmware_store_section(sc,
			    IWM_UCODE_TYPE_REGULAR_USNIFFER, tlv_data,
			    tlv_len)) != 0)
				goto parse_out;
			break;

		case IWM_UCODE_TLV_N_SCAN_CHANNELS:
			if (tlv_len != sizeof(uint32_t)) {
				error = EINVAL;
				goto parse_out;
			}
			sc->sc_capa_n_scan_channels =
			  le32toh(*(uint32_t *)tlv_data);
			break;

		case IWM_UCODE_TLV_FW_VERSION:
			if (tlv_len != sizeof(uint32_t) * 3) {
				error = EINVAL;
				goto parse_out;
			}
			snprintf(sc->sc_fwver, sizeof(sc->sc_fwver),
			    "%d.%d.%d",
			    le32toh(((uint32_t *)tlv_data)[0]),
			    le32toh(((uint32_t *)tlv_data)[1]),
			    le32toh(((uint32_t *)tlv_data)[2]));
			break;

		default:
			DPRINTF(("%s: unknown firmware section %d, abort\n",
			    DEVNAME(sc), tlv_type));
			error = EINVAL;
			goto parse_out;
		}

		len -= roundup(tlv_len, 4);
		data += roundup(tlv_len, 4);
	}

	KASSERT(error == 0);

 parse_out:
	if (error) {
		printf("%s: firmware parse error %d, "
		    "section type %d\n", DEVNAME(sc), error, tlv_type);
	}

	if (!(sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_PM_CMD_SUPPORT)) {
		printf("%s: device uses unsupported power ops\n", DEVNAME(sc));
		error = ENOTSUP;
	}

 out:
	if (error) {
		fw->fw_status = IWM_FW_STATUS_NONE;
		if (fw->fw_rawdata != NULL)
			iwm_fw_info_free(fw);
	} else
		fw->fw_status = IWM_FW_STATUS_DONE;
	wakeup(&sc->sc_fw);

	return error;
}

/*
 * basic device access
 */

uint32_t
iwm_read_prph(struct iwm_softc *sc, uint32_t addr)
{
	IWM_WRITE(sc,
	    IWM_HBUS_TARG_PRPH_RADDR, ((addr & 0x000fffff) | (3 << 24)));
	IWM_BARRIER_READ_WRITE(sc);
	return IWM_READ(sc, IWM_HBUS_TARG_PRPH_RDAT);
}

void
iwm_write_prph(struct iwm_softc *sc, uint32_t addr, uint32_t val)
{
	IWM_WRITE(sc,
	    IWM_HBUS_TARG_PRPH_WADDR, ((addr & 0x000fffff) | (3 << 24)));
	IWM_BARRIER_WRITE(sc);
	IWM_WRITE(sc, IWM_HBUS_TARG_PRPH_WDAT, val);
}

int
iwm_read_mem(struct iwm_softc *sc, uint32_t addr, void *buf, int dwords)
{
	int offs, ret = 0;
	uint32_t *vals = buf;

	if (iwm_nic_lock(sc)) {
		IWM_WRITE(sc, IWM_HBUS_TARG_MEM_RADDR, addr);
		for (offs = 0; offs < dwords; offs++)
			vals[offs] = IWM_READ(sc, IWM_HBUS_TARG_MEM_RDAT);
		iwm_nic_unlock(sc);
	} else {
		ret = EBUSY;
	}
	return ret;
}

int
iwm_write_mem(struct iwm_softc *sc, uint32_t addr, const void *buf, int dwords)
{
	int offs;	
	const uint32_t *vals = buf;

	if (iwm_nic_lock(sc)) {
		IWM_WRITE(sc, IWM_HBUS_TARG_MEM_WADDR, addr);
		/* WADDR auto-increments */
		for (offs = 0; offs < dwords; offs++) {
			uint32_t val = vals ? vals[offs] : 0;
			IWM_WRITE(sc, IWM_HBUS_TARG_MEM_WDAT, val);
		}
		iwm_nic_unlock(sc);
	} else {
		DPRINTF(("%s: write_mem failed\n", DEVNAME(sc)));
		return EBUSY;
	}
	return 0;
}

int
iwm_write_mem32(struct iwm_softc *sc, uint32_t addr, uint32_t val)
{
	return iwm_write_mem(sc, addr, &val, 1);
}

int
iwm_poll_bit(struct iwm_softc *sc, int reg,
	uint32_t bits, uint32_t mask, int timo)
{
	for (;;) {
		if ((IWM_READ(sc, reg) & mask) == (bits & mask)) {
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
iwm_nic_lock(struct iwm_softc *sc)
{
	int rv = 0;

	IWM_SETBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000)
		DELAY(2);

	if (iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY
	     | IWM_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP, 15000)) {
	    	rv = 1;
	} else {
		/* jolt */
		DPRINTF(("%s: resetting device via NMI\n", __func__));
		IWM_WRITE(sc, IWM_CSR_RESET, IWM_CSR_RESET_REG_FLAG_FORCE_NMI);
	}

	return rv;
}

void
iwm_nic_unlock(struct iwm_softc *sc)
{
	IWM_CLRBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
}

void
iwm_set_bits_mask_prph(struct iwm_softc *sc,
	uint32_t reg, uint32_t bits, uint32_t mask)
{
	uint32_t val;

	/* XXX: no error path? */
	if (iwm_nic_lock(sc)) {
		val = iwm_read_prph(sc, reg) & mask;
		val |= bits;
		iwm_write_prph(sc, reg, val);
		iwm_nic_unlock(sc);
	}
}

void
iwm_set_bits_prph(struct iwm_softc *sc, uint32_t reg, uint32_t bits)
{
	iwm_set_bits_mask_prph(sc, reg, bits, ~0);
}

void
iwm_clear_bits_prph(struct iwm_softc *sc, uint32_t reg, uint32_t bits)
{
	iwm_set_bits_mask_prph(sc, reg, 0, ~bits);
}

/*
 * DMA resource routines
 */

int
iwm_dma_contig_alloc(bus_dma_tag_t tag, struct iwm_dma_info *dma,
    bus_size_t size, bus_size_t alignment)
{
	int nsegs, error;
	caddr_t va;

	dma->tag = tag;
	dma->size = size;

	error = bus_dmamap_create(tag, size, 1, size, 0, BUS_DMA_NOWAIT,
	    &dma->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(tag, size, alignment, 0, &dma->seg, 1, &nsegs,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(tag, &dma->seg, 1, size, &va,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;
	dma->vaddr = va;

	error = bus_dmamap_load(tag, dma->map, dma->vaddr, size, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;

	memset(dma->vaddr, 0, size);
	bus_dmamap_sync(tag, dma->map, 0, size, BUS_DMASYNC_PREWRITE);
	dma->paddr = dma->map->dm_segs[0].ds_addr;

	return 0;

fail:	iwm_dma_contig_free(dma);
	return error;
}

void
iwm_dma_contig_free(struct iwm_dma_info *dma)
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

/* fwmem is used to load firmware onto the card */
int
iwm_alloc_fwmem(struct iwm_softc *sc)
{
	/* Must be aligned on a 16-byte boundary. */
	return iwm_dma_contig_alloc(sc->sc_dmat, &sc->fw_dma,
	    sc->sc_fwdmasegsz, 16);
}

void
iwm_free_fwmem(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->fw_dma);
}

/* tx scheduler rings.  not used? */
int
iwm_alloc_sched(struct iwm_softc *sc)
{
	int rv;

	/* TX scheduler rings must be aligned on a 1KB boundary. */
	rv = iwm_dma_contig_alloc(sc->sc_dmat, &sc->sched_dma,
	    nitems(sc->txq) * sizeof(struct iwm_agn_scd_bc_tbl), 1024);
	return rv;
}

void
iwm_free_sched(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->sched_dma);
}

/* keep-warm page is used internally by the card.  see iwl-fh.h for more info */
int
iwm_alloc_kw(struct iwm_softc *sc)
{
	return iwm_dma_contig_alloc(sc->sc_dmat, &sc->kw_dma, 4096, 4096);
}

void
iwm_free_kw(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->kw_dma);
}

/* interrupt cause table */
int
iwm_alloc_ict(struct iwm_softc *sc)
{
	return iwm_dma_contig_alloc(sc->sc_dmat, &sc->ict_dma,
	    IWM_ICT_SIZE, 1<<IWM_ICT_PADDR_SHIFT);
}

void
iwm_free_ict(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->ict_dma);
}

int
iwm_alloc_rx_ring(struct iwm_softc *sc, struct iwm_rx_ring *ring)
{
	bus_size_t size;
	int i, error;

	ring->cur = 0;

	/* Allocate RX descriptors (256-byte aligned). */
	size = IWM_RX_RING_COUNT * sizeof(uint32_t);
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma, size, 256);
	if (error != 0) {
		printf("%s: could not allocate RX ring DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}
	ring->desc = ring->desc_dma.vaddr;

	/* Allocate RX status area (16-byte aligned). */
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->stat_dma,
	    sizeof(*ring->stat), 16);
	if (error != 0) {
		printf("%s: could not allocate RX status DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}
	ring->stat = ring->stat_dma.vaddr;

	/*
	 * Allocate and map RX buffers.
	 */
	for (i = 0; i < IWM_RX_RING_COUNT; i++) {
		struct iwm_rx_data *data = &ring->data[i];

		memset(data, 0, sizeof(*data));
		error = bus_dmamap_create(sc->sc_dmat, IWM_RBUF_SIZE, 1,
		    IWM_RBUF_SIZE, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &data->map);
		if (error != 0) {
			printf("%s: could not create RX buf DMA map\n",
			    DEVNAME(sc));
			goto fail;
		}

		if ((error = iwm_rx_addbuf(sc, IWM_RBUF_SIZE, i)) != 0) {
			goto fail;
		}
	}
	return 0;

fail:	iwm_free_rx_ring(sc, ring);
	return error;
}

void
iwm_disable_rx_dma(struct iwm_softc *sc)
{
	int ntries;

	if (iwm_nic_lock(sc)) {
		IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_CONFIG_REG, 0);
		for (ntries = 0; ntries < 1000; ntries++) {
			if (IWM_READ(sc, IWM_FH_MEM_RSSR_RX_STATUS_REG) &
			    IWM_FH_RSSR_CHNL0_RX_STATUS_CHNL_IDLE)
				break;
			DELAY(10);
		}
		iwm_nic_unlock(sc);
	}
}

void
iwm_reset_rx_ring(struct iwm_softc *sc, struct iwm_rx_ring *ring)
{
	ring->cur = 0;
	bus_dmamap_sync(sc->sc_dmat, ring->stat_dma.map, 0,
	    ring->stat_dma.size, BUS_DMASYNC_PREWRITE);
	memset(ring->stat, 0, sizeof(*ring->stat));
	bus_dmamap_sync(sc->sc_dmat, ring->stat_dma.map, 0,
	    ring->stat_dma.size, BUS_DMASYNC_POSTWRITE);

}

void
iwm_free_rx_ring(struct iwm_softc *sc, struct iwm_rx_ring *ring)
{
	int i;

	iwm_dma_contig_free(&ring->desc_dma);
	iwm_dma_contig_free(&ring->stat_dma);

	for (i = 0; i < IWM_RX_RING_COUNT; i++) {
		struct iwm_rx_data *data = &ring->data[i];

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
iwm_alloc_tx_ring(struct iwm_softc *sc, struct iwm_tx_ring *ring, int qid)
{
	bus_addr_t paddr;
	bus_size_t size;
	int i, error;

	ring->qid = qid;
	ring->queued = 0;
	ring->cur = 0;

	/* Allocate TX descriptors (256-byte aligned). */
	size = IWM_TX_RING_COUNT * sizeof (struct iwm_tfd);
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma, size, 256);
	if (error != 0) {
		printf("%s: could not allocate TX ring DMA memory\n",
		    DEVNAME(sc));
		goto fail;
	}
	ring->desc = ring->desc_dma.vaddr;

	/*
	 * We only use rings 0 through 9 (4 EDCA + cmd) so there is no need
	 * to allocate commands space for other rings.
	 */
	if (qid > IWM_MVM_CMD_QUEUE)
		return 0;

	size = IWM_TX_RING_COUNT * sizeof(struct iwm_device_cmd);
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->cmd_dma, size, 4);
	if (error != 0) {
		printf("%s: could not allocate TX cmd DMA memory\n", DEVNAME(sc));
		goto fail;
	}
	ring->cmd = ring->cmd_dma.vaddr;

	paddr = ring->cmd_dma.paddr;
	for (i = 0; i < IWM_TX_RING_COUNT; i++) {
		struct iwm_tx_data *data = &ring->data[i];
		size_t mapsize;

		data->cmd_paddr = paddr;
		data->scratch_paddr = paddr + sizeof(struct iwm_cmd_header)
		    + offsetof(struct iwm_tx_cmd, scratch);
		paddr += sizeof(struct iwm_device_cmd);

		/* FW commands may require more mapped space than packets. */
		if (qid == IWM_MVM_CMD_QUEUE)
			mapsize = (sizeof(struct iwm_cmd_header) +
			    IWM_MAX_CMD_PAYLOAD_SIZE);
		else
			mapsize = MCLBYTES;
		error = bus_dmamap_create(sc->sc_dmat, mapsize,
		    IWM_NUM_OF_TBS - 2, mapsize, 0, BUS_DMA_NOWAIT,
		    &data->map);
		if (error != 0) {
			printf("%s: could not create TX buf DMA map\n", DEVNAME(sc));
			goto fail;
		}
	}
	KASSERT(paddr == ring->cmd_dma.paddr + size);
	return 0;

fail:	iwm_free_tx_ring(sc, ring);
	return error;
}

void
iwm_reset_tx_ring(struct iwm_softc *sc, struct iwm_tx_ring *ring)
{
	int i;

	for (i = 0; i < IWM_TX_RING_COUNT; i++) {
		struct iwm_tx_data *data = &ring->data[i];

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
}

void
iwm_free_tx_ring(struct iwm_softc *sc, struct iwm_tx_ring *ring)
{
	int i;

	iwm_dma_contig_free(&ring->desc_dma);
	iwm_dma_contig_free(&ring->cmd_dma);

	for (i = 0; i < IWM_TX_RING_COUNT; i++) {
		struct iwm_tx_data *data = &ring->data[i];

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

/*
 * High-level hardware frobbing routines
 */

void
iwm_enable_rfkill_int(struct iwm_softc *sc)
{
	sc->sc_intmask = IWM_CSR_INT_BIT_RF_KILL;
	IWM_WRITE(sc, IWM_CSR_INT_MASK, sc->sc_intmask);
}

int
iwm_check_rfkill(struct iwm_softc *sc)
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
	v = IWM_READ(sc, IWM_CSR_GP_CNTRL);
	rv = (v & IWM_CSR_GP_CNTRL_REG_FLAG_HW_RF_KILL_SW) == 0;
	if (rv) {
		sc->sc_flags |= IWM_FLAG_RFKILL;
	} else {
		sc->sc_flags &= ~IWM_FLAG_RFKILL;
	}

	splx(s);
	return rv;
}

void
iwm_enable_interrupts(struct iwm_softc *sc)
{
	sc->sc_intmask = IWM_CSR_INI_SET_MASK;
	IWM_WRITE(sc, IWM_CSR_INT_MASK, sc->sc_intmask);
}

void
iwm_restore_interrupts(struct iwm_softc *sc)
{
	IWM_WRITE(sc, IWM_CSR_INT_MASK, sc->sc_intmask);
}

void
iwm_disable_interrupts(struct iwm_softc *sc)
{
	int s = splnet();

	/* disable interrupts */
	IWM_WRITE(sc, IWM_CSR_INT_MASK, 0);

	/* acknowledge all interrupts */
	IWM_WRITE(sc, IWM_CSR_INT, ~0);
	IWM_WRITE(sc, IWM_CSR_FH_INT_STATUS, ~0);

	splx(s);
}

void
iwm_ict_reset(struct iwm_softc *sc)
{
	iwm_disable_interrupts(sc);

	/* Reset ICT table. */
	memset(sc->ict_dma.vaddr, 0, IWM_ICT_SIZE);
	sc->ict_cur = 0;

	/* Set physical address of ICT table (4KB aligned). */
	IWM_WRITE(sc, IWM_CSR_DRAM_INT_TBL_REG,
	    IWM_CSR_DRAM_INT_TBL_ENABLE
	    | IWM_CSR_DRAM_INIT_TBL_WRAP_CHECK
	    | IWM_CSR_DRAM_INIT_TBL_WRITE_POINTER
	    | sc->ict_dma.paddr >> IWM_ICT_PADDR_SHIFT);

	/* Switch to ICT interrupt mode in driver. */
	sc->sc_flags |= IWM_FLAG_USE_ICT;

	/* Re-enable interrupts. */
	IWM_WRITE(sc, IWM_CSR_INT, ~0);
	iwm_enable_interrupts(sc);
}

#define IWM_HW_READY_TIMEOUT 50
int
iwm_set_hw_ready(struct iwm_softc *sc)
{
	int ready;

	IWM_SETBITS(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY);

	ready = iwm_poll_bit(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY,
	    IWM_HW_READY_TIMEOUT);
	if (ready)
		IWM_SETBITS(sc, IWM_CSR_MBOX_SET_REG,
		    IWM_CSR_MBOX_SET_REG_OS_ALIVE);

	return ready;
}
#undef IWM_HW_READY_TIMEOUT

int
iwm_prepare_card_hw(struct iwm_softc *sc)
{
	int t = 0;

	if (iwm_set_hw_ready(sc))
		return 0;

	DELAY(100);

	/* If HW is not ready, prepare the conditions to check again */
	IWM_SETBITS(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_PREPARE);

	do {
		if (iwm_set_hw_ready(sc))
			return 0;
		DELAY(200);
		t += 200;
	} while (t < 150000);

	return ETIMEDOUT;
}

void
iwm_apm_config(struct iwm_softc *sc)
{
	pcireg_t reg;

	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
	    sc->sc_cap_off + PCI_PCIE_LCSR);
	if (reg & PCI_PCIE_LCSR_ASPM_L1) {
		/* Um the Linux driver prints "Disabling L0S for this one ... */
		IWM_SETBITS(sc, IWM_CSR_GIO_REG,
		    IWM_CSR_GIO_REG_VAL_L0S_ENABLED);
	} else {
		/* ... and "Enabling" here */
		IWM_CLRBITS(sc, IWM_CSR_GIO_REG,
		    IWM_CSR_GIO_REG_VAL_L0S_ENABLED);
	}
}

/*
 * Start up NIC's basic functionality after it has been reset
 * (e.g. after platform boot, or shutdown via iwm_pcie_apm_stop())
 * NOTE:  This does not load uCode nor start the embedded processor
 */
int
iwm_apm_init(struct iwm_softc *sc)
{
	int error = 0;

	DPRINTF(("iwm apm start\n"));

	/* Disable L0S exit timer (platform NMI Work/Around) */
	if (sc->sc_device_family != IWM_DEVICE_FAMILY_8000)
		IWM_SETBITS(sc, IWM_CSR_GIO_CHICKEN_BITS,
		    IWM_CSR_GIO_CHICKEN_BITS_REG_BIT_DIS_L0S_EXIT_TIMER);

	/*
	 * Disable L0s without affecting L1;
	 *  don't wait for ICH L0s (ICH bug W/A)
	 */
	IWM_SETBITS(sc, IWM_CSR_GIO_CHICKEN_BITS,
	    IWM_CSR_GIO_CHICKEN_BITS_REG_BIT_L1A_NO_L0S_RX);

	/* Set FH wait threshold to maximum (HW error during stress W/A) */
	IWM_SETBITS(sc, IWM_CSR_DBG_HPET_MEM_REG, IWM_CSR_DBG_HPET_MEM_REG_VAL);

	/*
	 * Enable HAP INTA (interrupt from management bus) to
	 * wake device's PCI Express link L1a -> L0s
	 */
	IWM_SETBITS(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A);

	iwm_apm_config(sc);

#if 0 /* not for 7k/8k */
	/* Configure analog phase-lock-loop before activating to D0A */
	if (trans->cfg->base_params->pll_cfg_val)
		IWM_SETBITS(trans, IWM_CSR_ANA_PLL_CFG,
		    trans->cfg->base_params->pll_cfg_val);
#endif

	/*
	 * Set "initialization complete" bit to move adapter from
	 * D0U* --> D0A* (powered-up active) state.
	 */
	IWM_SETBITS(sc, IWM_CSR_GP_CNTRL, IWM_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);

	/*
	 * Wait for clock stabilization; once stabilized, access to
	 * device-internal resources is supported, e.g. iwm_write_prph()
	 * and accesses to uCode SRAM.
	 */
	if (!iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY, 25000)) {
		printf("%s: timeout waiting for clock stabilization\n",
		    DEVNAME(sc));
		error = ETIMEDOUT;
		goto out;
	}

	if (sc->host_interrupt_operation_mode) {
		/*
		 * This is a bit of an abuse - This is needed for 7260 / 3160
		 * only check host_interrupt_operation_mode even if this is
		 * not related to host_interrupt_operation_mode.
		 *
		 * Enable the oscillator to count wake up time for L1 exit. This
		 * consumes slightly more power (100uA) - but allows to be sure
		 * that we wake up from L1 on time.
		 *
		 * This looks weird: read twice the same register, discard the
		 * value, set a bit, and yet again, read that same register
		 * just to discard the value. But that's the way the hardware
		 * seems to like it.
		 */
		iwm_read_prph(sc, IWM_OSC_CLK);
		iwm_read_prph(sc, IWM_OSC_CLK);
		iwm_set_bits_prph(sc, IWM_OSC_CLK, IWM_OSC_CLK_FORCE_CONTROL);
		iwm_read_prph(sc, IWM_OSC_CLK);
		iwm_read_prph(sc, IWM_OSC_CLK);
	}

	/*
	 * Enable DMA clock and wait for it to stabilize.
	 *
	 * Write to "CLK_EN_REG"; "1" bits enable clocks, while "0" bits
	 * do not disable clocks.  This preserves any hardware bits already
	 * set by default in "CLK_CTRL_REG" after reset.
	 */
	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000) {
		iwm_write_prph(sc, IWM_APMG_CLK_EN_REG,
		    IWM_APMG_CLK_VAL_DMA_CLK_RQT);
		DELAY(20);

		/* Disable L1-Active */
		iwm_set_bits_prph(sc, IWM_APMG_PCIDEV_STT_REG,
		    IWM_APMG_PCIDEV_STT_VAL_L1_ACT_DIS);

		/* Clear the interrupt in APMG if the NIC is in RFKILL */
		iwm_write_prph(sc, IWM_APMG_RTC_INT_STT_REG,
		    IWM_APMG_RTC_INT_STT_RFKILL);
	}
 out:
	if (error)
		printf("%s: apm init error %d\n", DEVNAME(sc), error);
	return error;
}

void
iwm_apm_stop(struct iwm_softc *sc)
{
	/* stop device's busmaster DMA activity */
	IWM_SETBITS(sc, IWM_CSR_RESET, IWM_CSR_RESET_REG_FLAG_STOP_MASTER);

	if (!iwm_poll_bit(sc, IWM_CSR_RESET,
	    IWM_CSR_RESET_REG_FLAG_MASTER_DISABLED,
	    IWM_CSR_RESET_REG_FLAG_MASTER_DISABLED, 100))
		printf("%s: timeout waiting for master\n", DEVNAME(sc));
        DPRINTF(("iwm apm stop\n"));
}

int
iwm_start_hw(struct iwm_softc *sc)
{
	int error;

	if ((error = iwm_prepare_card_hw(sc)) != 0)
		return error;

	/* Reset the entire device */
	IWM_WRITE(sc, IWM_CSR_RESET, IWM_CSR_RESET_REG_FLAG_SW_RESET);
	DELAY(10);

	if ((error = iwm_apm_init(sc)) != 0)
		return error;

	iwm_enable_rfkill_int(sc);
	iwm_check_rfkill(sc);

	return 0;
}


void
iwm_stop_device(struct iwm_softc *sc)
{
	int chnl, ntries;
	int qid;

	/* tell the device to stop sending interrupts */
	iwm_disable_interrupts(sc);

	/* device going down, Stop using ICT table */
	sc->sc_flags &= ~IWM_FLAG_USE_ICT;

	/* stop tx and rx.  tx and rx bits, as usual, are from if_iwn */

	iwm_write_prph(sc, IWM_SCD_TXFACT, 0);

	/* Stop all DMA channels. */
	if (iwm_nic_lock(sc)) {
		for (chnl = 0; chnl < IWM_FH_TCSR_CHNL_NUM; chnl++) {
			IWM_WRITE(sc,
			    IWM_FH_TCSR_CHNL_TX_CONFIG_REG(chnl), 0);
			for (ntries = 0; ntries < 200; ntries++) {
				uint32_t r;

				r = IWM_READ(sc, IWM_FH_TSSR_TX_STATUS_REG);
				if (r & IWM_FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(
				    chnl))
					break;
				DELAY(20);
			}
		}
		iwm_nic_unlock(sc);
	}
	iwm_disable_rx_dma(sc);

	/* Stop RX ring. */
	iwm_reset_rx_ring(sc, &sc->rxq);

	/* Reset all TX rings. */
	for (qid = 0; qid < nitems(sc->txq); qid++)
		iwm_reset_tx_ring(sc, &sc->txq[qid]);

	/*
	 * Power-down device's busmaster DMA clocks
	 */
	iwm_write_prph(sc, IWM_APMG_CLK_DIS_REG, IWM_APMG_CLK_VAL_DMA_CLK_RQT);
	DELAY(5);

	/* Make sure (redundant) we've released our request to stay awake */
	IWM_CLRBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	/* Stop the device, and put it in low power state */
	iwm_apm_stop(sc);

	/* Upon stop, the APM issues an interrupt if HW RF kill is set.
	 * Clean again the interrupt here
	 */
	iwm_disable_interrupts(sc);
	/* stop and reset the on-board processor */
	IWM_WRITE(sc, IWM_CSR_RESET, IWM_CSR_RESET_REG_FLAG_SW_RESET);

	/*
	 * Even if we stop the HW, we still want the RF kill
	 * interrupt
	 */
	iwm_enable_rfkill_int(sc);
	iwm_check_rfkill(sc);
}

void
iwm_set_pwr(struct iwm_softc *sc)
{
	iwm_set_bits_mask_prph(sc, IWM_APMG_PS_CTRL_REG,
	    IWM_APMG_PS_CTRL_VAL_PWR_SRC_VMAIN, ~IWM_APMG_PS_CTRL_MSK_PWR_SRC);
}

void
iwm_mvm_nic_config(struct iwm_softc *sc)
{
	uint8_t radio_cfg_type, radio_cfg_step, radio_cfg_dash;
	uint32_t reg_val = 0;

	radio_cfg_type = (sc->sc_fw_phy_config & IWM_FW_PHY_CFG_RADIO_TYPE) >>
	    IWM_FW_PHY_CFG_RADIO_TYPE_POS;
	radio_cfg_step = (sc->sc_fw_phy_config & IWM_FW_PHY_CFG_RADIO_STEP) >>
	    IWM_FW_PHY_CFG_RADIO_STEP_POS;
	radio_cfg_dash = (sc->sc_fw_phy_config & IWM_FW_PHY_CFG_RADIO_DASH) >>
	    IWM_FW_PHY_CFG_RADIO_DASH_POS;

	/* SKU control */
	reg_val |= IWM_CSR_HW_REV_STEP(sc->sc_hw_rev) <<
	    IWM_CSR_HW_IF_CONFIG_REG_POS_MAC_STEP;
	reg_val |= IWM_CSR_HW_REV_DASH(sc->sc_hw_rev) <<
	    IWM_CSR_HW_IF_CONFIG_REG_POS_MAC_DASH;

	/* radio configuration */
	reg_val |= radio_cfg_type << IWM_CSR_HW_IF_CONFIG_REG_POS_PHY_TYPE;
	reg_val |= radio_cfg_step << IWM_CSR_HW_IF_CONFIG_REG_POS_PHY_STEP;
	reg_val |= radio_cfg_dash << IWM_CSR_HW_IF_CONFIG_REG_POS_PHY_DASH;

	IWM_WRITE(sc, IWM_CSR_HW_IF_CONFIG_REG, reg_val);

	DPRINTF(("Radio type=0x%x-0x%x-0x%x\n", radio_cfg_type,
	    radio_cfg_step, radio_cfg_dash));

	/*
	 * W/A : NIC is stuck in a reset state after Early PCIe power off
	 * (PCIe power is lost before PERST# is asserted), causing ME FW
	 * to lose ownership and not being able to obtain it back.
	 */
	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000)
		iwm_set_bits_mask_prph(sc, IWM_APMG_PS_CTRL_REG,
		    IWM_APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS,
		    ~IWM_APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS);
}

int
iwm_nic_rx_init(struct iwm_softc *sc)
{
	if (!iwm_nic_lock(sc))
		return EBUSY;

	/*
	 * Initialize RX ring.  This is from the iwn driver.
	 */
	memset(sc->rxq.stat, 0, sizeof(*sc->rxq.stat));

	/* stop DMA */
	iwm_disable_rx_dma(sc);
	IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_RBDCB_WPTR, 0);
	IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_FLUSH_RB_REQ, 0);
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_RDPTR, 0);
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_RBDCB_WPTR_REG, 0);

	/* Set physical address of RX ring (256-byte aligned). */
	IWM_WRITE(sc,
	    IWM_FH_RSCSR_CHNL0_RBDCB_BASE_REG, sc->rxq.desc_dma.paddr >> 8);

	/* Set physical address of RX status (16-byte aligned). */
	IWM_WRITE(sc,
	    IWM_FH_RSCSR_CHNL0_STTS_WPTR_REG, sc->rxq.stat_dma.paddr >> 4);

	/* Enable RX. */
	IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_CONFIG_REG,
	    IWM_FH_RCSR_RX_CONFIG_CHNL_EN_ENABLE_VAL		|
	    IWM_FH_RCSR_CHNL0_RX_IGNORE_RXF_EMPTY		|  /* HW bug */
	    IWM_FH_RCSR_CHNL0_RX_CONFIG_IRQ_DEST_INT_HOST_VAL	|
	    IWM_FH_RCSR_CHNL0_RX_CONFIG_SINGLE_FRAME_MSK	|
	    (IWM_RX_RB_TIMEOUT << IWM_FH_RCSR_RX_CONFIG_REG_IRQ_RBTH_POS) |
	    IWM_FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_4K		|
	    IWM_RX_QUEUE_SIZE_LOG << IWM_FH_RCSR_RX_CONFIG_RBDCB_SIZE_POS);

	IWM_WRITE_1(sc, IWM_CSR_INT_COALESCING, IWM_HOST_INT_TIMEOUT_DEF);

	/* W/A for interrupt coalescing bug in 7260 and 3160 */
	if (sc->host_interrupt_operation_mode)
		IWM_SETBITS(sc, IWM_CSR_INT_COALESCING, IWM_HOST_INT_OPER_MODE);

	/*
	 * Thus sayeth el jefe (iwlwifi) via a comment:
	 *
	 * This value should initially be 0 (before preparing any
 	 * RBs), should be 8 after preparing the first 8 RBs (for example)
	 */
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_WPTR, 8);

	iwm_nic_unlock(sc);

	return 0;
}

int
iwm_nic_tx_init(struct iwm_softc *sc)
{
	int qid;

	if (!iwm_nic_lock(sc))
		return EBUSY;

	/* Deactivate TX scheduler. */
	iwm_write_prph(sc, IWM_SCD_TXFACT, 0);

	/* Set physical address of "keep warm" page (16-byte aligned). */
	IWM_WRITE(sc, IWM_FH_KW_MEM_ADDR_REG, sc->kw_dma.paddr >> 4);

	/* Initialize TX rings. */
	for (qid = 0; qid < nitems(sc->txq); qid++) {
		struct iwm_tx_ring *txq = &sc->txq[qid];

		/* Set physical address of TX ring (256-byte aligned). */
		IWM_WRITE(sc, IWM_FH_MEM_CBBC_QUEUE(qid),
		    txq->desc_dma.paddr >> 8);
		DPRINTF(("loading ring %d descriptors (%p) at %lx\n",
		    qid, txq->desc, txq->desc_dma.paddr >> 8));
	}

	iwm_write_prph(sc, IWM_SCD_GP_CTRL, IWM_SCD_GP_CTRL_AUTO_ACTIVE_MODE);

	iwm_nic_unlock(sc);

	return 0;
}

int
iwm_nic_init(struct iwm_softc *sc)
{
	int error;

	iwm_apm_init(sc);
	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000)
		iwm_set_pwr(sc);

	iwm_mvm_nic_config(sc);

	if ((error = iwm_nic_rx_init(sc)) != 0)
		return error;

	/*
	 * Ditto for TX, from iwn
	 */
	if ((error = iwm_nic_tx_init(sc)) != 0)
		return error;

	DPRINTF(("shadow registers enabled\n"));
	IWM_SETBITS(sc, IWM_CSR_MAC_SHADOW_REG_CTRL, 0x800fffff);

	return 0;
}

const uint8_t iwm_mvm_ac_to_tx_fifo[] = {
	IWM_MVM_TX_FIFO_VO,
	IWM_MVM_TX_FIFO_VI,
	IWM_MVM_TX_FIFO_BE,
	IWM_MVM_TX_FIFO_BK,
};

int
iwm_enable_txq(struct iwm_softc *sc, int sta_id, int qid, int fifo)
{
	if (!iwm_nic_lock(sc)) {
		DPRINTF(("%s: cannot enable txq %d\n", DEVNAME(sc), qid));
		return EBUSY;
	}

	IWM_WRITE(sc, IWM_HBUS_TARG_WRPTR, qid << 8 | 0);

	if (qid == IWM_MVM_CMD_QUEUE) {
		/* unactivate before configuration */
		iwm_write_prph(sc, IWM_SCD_QUEUE_STATUS_BITS(qid),
		    (0 << IWM_SCD_QUEUE_STTS_REG_POS_ACTIVE)
		    | (1 << IWM_SCD_QUEUE_STTS_REG_POS_SCD_ACT_EN));

		iwm_clear_bits_prph(sc, IWM_SCD_AGGR_SEL, (1 << qid));

		iwm_write_prph(sc, IWM_SCD_QUEUE_RDPTR(qid), 0);

		iwm_write_mem32(sc, sc->sched_base + IWM_SCD_CONTEXT_QUEUE_OFFSET(qid), 0);
		/* Set scheduler window size and frame limit. */
		iwm_write_mem32(sc,
		    sc->sched_base + IWM_SCD_CONTEXT_QUEUE_OFFSET(qid) +
		    sizeof(uint32_t),
		    ((IWM_FRAME_LIMIT << IWM_SCD_QUEUE_CTX_REG2_WIN_SIZE_POS) &
		    IWM_SCD_QUEUE_CTX_REG2_WIN_SIZE_MSK) |
		    ((IWM_FRAME_LIMIT << IWM_SCD_QUEUE_CTX_REG2_FRAME_LIMIT_POS) &
		    IWM_SCD_QUEUE_CTX_REG2_FRAME_LIMIT_MSK));

		iwm_write_prph(sc, IWM_SCD_QUEUE_STATUS_BITS(qid),
		    (1 << IWM_SCD_QUEUE_STTS_REG_POS_ACTIVE) |
		    (fifo << IWM_SCD_QUEUE_STTS_REG_POS_TXF) |
		    (1 << IWM_SCD_QUEUE_STTS_REG_POS_WSL) |
		    IWM_SCD_QUEUE_STTS_REG_MSK);
	} else {
		struct iwm_scd_txq_cfg_cmd cmd;
		int error;

		iwm_nic_unlock(sc);

		memset(&cmd, 0, sizeof(cmd));
		cmd.scd_queue = qid;
		cmd.enable = 1;
		cmd.sta_id = sta_id;
		cmd.tx_fifo = fifo;
		cmd.aggregate = 0;
		cmd.window = IWM_FRAME_LIMIT;

		error = iwm_mvm_send_cmd_pdu(sc, IWM_SCD_QUEUE_CFG, IWM_CMD_SYNC,
		    sizeof(cmd), &cmd);
		if (error) {
			DPRINTF(("%s: cannot enable txq %d\n", DEVNAME(sc), qid));
			return error;
		}

		if (!iwm_nic_lock(sc))
			return EBUSY;
	}

	iwm_write_prph(sc, IWM_SCD_EN_CTRL,
	    iwm_read_prph(sc, IWM_SCD_EN_CTRL) | qid);

	iwm_nic_unlock(sc);

	DPRINTF(("enabled txq %d FIFO %d\n", qid, fifo));

	return 0;
}

int
iwm_post_alive(struct iwm_softc *sc)
{
	int nwords;
	int error, chnl;
	uint32_t base;

	if (!iwm_nic_lock(sc))
		return EBUSY;

	base = iwm_read_prph(sc, IWM_SCD_SRAM_BASE_ADDR);
	if (sc->sched_base != base)
		DPRINTF(("%s: sched addr mismatch: alive: 0x%x prph: 0x%x\n",
		    DEVNAME(sc), sc->sched_base, base));

	iwm_ict_reset(sc);

	/* Clear TX scheduler state in SRAM. */
	nwords = (IWM_SCD_TRANS_TBL_MEM_UPPER_BOUND -
	    IWM_SCD_CONTEXT_MEM_LOWER_BOUND)
	    / sizeof(uint32_t);
	error = iwm_write_mem(sc,
	    sc->sched_base + IWM_SCD_CONTEXT_MEM_LOWER_BOUND,
	    NULL, nwords);
	if (error)
		goto out;

	/* Set physical address of TX scheduler rings (1KB aligned). */
	iwm_write_prph(sc, IWM_SCD_DRAM_BASE_ADDR, sc->sched_dma.paddr >> 10);

	iwm_write_prph(sc, IWM_SCD_CHAINEXT_EN, 0);

	iwm_nic_unlock(sc);

	/* enable command channel */
	error = iwm_enable_txq(sc, 0 /* unused */, IWM_MVM_CMD_QUEUE, 7);
	if (error)
		return error;

	if (!iwm_nic_lock(sc))
		return EBUSY;

	iwm_write_prph(sc, IWM_SCD_TXFACT, 0xff);

	/* Enable DMA channels. */
	for (chnl = 0; chnl < IWM_FH_TCSR_CHNL_NUM; chnl++) {
		IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_CONFIG_REG(chnl),
		    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE |
		    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_ENABLE);
	}

	IWM_SETBITS(sc, IWM_FH_TX_CHICKEN_BITS_REG,
	    IWM_FH_TX_CHICKEN_BITS_SCD_AUTO_RETRY_EN);

	/* Enable L1-Active */
	if (sc->sc_device_family != IWM_DEVICE_FAMILY_8000)
		iwm_clear_bits_prph(sc, IWM_APMG_PCIDEV_STT_REG,
		    IWM_APMG_PCIDEV_STT_VAL_L1_ACT_DIS);

 out:
 	iwm_nic_unlock(sc);
	return error;
}

/*
 * get phy db section: returns a pointer to a phy db section specified by
 * type and channel group id.
 */
struct iwm_phy_db_entry *
iwm_phy_db_get_section(struct iwm_softc *sc,
	enum iwm_phy_db_section_type type, uint16_t chg_id)
{
	struct iwm_phy_db *phy_db = &sc->sc_phy_db;

	if (type >= IWM_PHY_DB_MAX)
		return NULL;

	switch (type) {
	case IWM_PHY_DB_CFG:
		return &phy_db->cfg;
	case IWM_PHY_DB_CALIB_NCH:
		return &phy_db->calib_nch;
	case IWM_PHY_DB_CALIB_CHG_PAPD:
		if (chg_id >= IWM_NUM_PAPD_CH_GROUPS)
			return NULL;
		return &phy_db->calib_ch_group_papd[chg_id];
	case IWM_PHY_DB_CALIB_CHG_TXP:
		if (chg_id >= IWM_NUM_TXP_CH_GROUPS)
			return NULL;
		return &phy_db->calib_ch_group_txp[chg_id];
	default:
		return NULL;
	}
	return NULL;
}

int
iwm_phy_db_set_section(struct iwm_softc *sc,
	struct iwm_calib_res_notif_phy_db *phy_db_notif)
{
	enum iwm_phy_db_section_type type = le16toh(phy_db_notif->type);
	uint16_t size  = le16toh(phy_db_notif->length);
	struct iwm_phy_db_entry *entry;
	uint16_t chg_id = 0;

	if (type == IWM_PHY_DB_CALIB_CHG_PAPD ||
	    type == IWM_PHY_DB_CALIB_CHG_TXP)
		chg_id = le16toh(*(uint16_t *)phy_db_notif->data);

	entry = iwm_phy_db_get_section(sc, type, chg_id);
	if (!entry)
		return EINVAL;

	if (entry->data)
		free(entry->data, M_DEVBUF, entry->size);
	entry->data = malloc(size, M_DEVBUF, M_NOWAIT);
	if (!entry->data) {
		entry->size = 0;
		return ENOMEM;
	}
	memcpy(entry->data, phy_db_notif->data, size);
	entry->size = size;

	DPRINTFN(10, ("%s(%d): [PHYDB]SET: Type %d , Size: %d, data: %p\n",
	    __func__, __LINE__, type, size, entry->data));

	return 0;
}

int
iwm_is_valid_channel(uint16_t ch_id)
{
	if (ch_id <= 14 ||
	    (36 <= ch_id && ch_id <= 64 && ch_id % 4 == 0) ||
	    (100 <= ch_id && ch_id <= 140 && ch_id % 4 == 0) ||
	    (145 <= ch_id && ch_id <= 165 && ch_id % 4 == 1))
		return 1;
	return 0;
}

uint8_t
iwm_ch_id_to_ch_index(uint16_t ch_id)
{
	if (!iwm_is_valid_channel(ch_id))
		return 0xff;

	if (ch_id <= 14)
		return ch_id - 1;
	if (ch_id <= 64)
		return (ch_id + 20) / 4;
	if (ch_id <= 140)
		return (ch_id - 12) / 4;
	return (ch_id - 13) / 4;
}


uint16_t
iwm_channel_id_to_papd(uint16_t ch_id)
{
	if (!iwm_is_valid_channel(ch_id))
		return 0xff;

	if (1 <= ch_id && ch_id <= 14)
		return 0;
	if (36 <= ch_id && ch_id <= 64)
		return 1;
	if (100 <= ch_id && ch_id <= 140)
		return 2;
	return 3;
}

uint16_t
iwm_channel_id_to_txp(struct iwm_softc *sc, uint16_t ch_id)
{
	struct iwm_phy_db *phy_db = &sc->sc_phy_db;
	struct iwm_phy_db_chg_txp *txp_chg;
	int i;
	uint8_t ch_index = iwm_ch_id_to_ch_index(ch_id);

	if (ch_index == 0xff)
		return 0xff;

	for (i = 0; i < IWM_NUM_TXP_CH_GROUPS; i++) {
		txp_chg = (void *)phy_db->calib_ch_group_txp[i].data;
		if (!txp_chg)
			return 0xff;
		/*
		 * Looking for the first channel group that its max channel is
		 * higher then wanted channel.
		 */
		if (le16toh(txp_chg->max_channel_idx) >= ch_index)
			return i;
	}
	return 0xff;
}

int
iwm_phy_db_get_section_data(struct iwm_softc *sc,
	uint32_t type, uint8_t **data, uint16_t *size, uint16_t ch_id)
{
	struct iwm_phy_db_entry *entry;
	uint16_t ch_group_id = 0;

	/* find wanted channel group */
	if (type == IWM_PHY_DB_CALIB_CHG_PAPD)
		ch_group_id = iwm_channel_id_to_papd(ch_id);
	else if (type == IWM_PHY_DB_CALIB_CHG_TXP)
		ch_group_id = iwm_channel_id_to_txp(sc, ch_id);

	entry = iwm_phy_db_get_section(sc, type, ch_group_id);
	if (!entry)
		return EINVAL;

	*data = entry->data;
	*size = entry->size;

	DPRINTFN(10, ("%s(%d): [PHYDB] GET: Type %d , Size: %d\n",
		       __func__, __LINE__, type, *size));

	return 0;
}

int
iwm_send_phy_db_cmd(struct iwm_softc *sc, uint16_t type,
	uint16_t length, void *data)
{
	struct iwm_phy_db_cmd phy_db_cmd;
	struct iwm_host_cmd cmd = {
		.id = IWM_PHY_DB_CMD,
		.flags = IWM_CMD_ASYNC,
	};

	DPRINTFN(10, ("Sending PHY-DB hcmd of type %d, of length %d\n", type, length));

	/* Set phy db cmd variables */
	phy_db_cmd.type = le16toh(type);
	phy_db_cmd.length = le16toh(length);

	/* Set hcmd variables */
	cmd.data[0] = &phy_db_cmd;
	cmd.len[0] = sizeof(struct iwm_phy_db_cmd);
	cmd.data[1] = data;
	cmd.len[1] = length;

	return iwm_send_cmd(sc, &cmd);
}

int
iwm_phy_db_send_all_channel_groups(struct iwm_softc *sc,
	enum iwm_phy_db_section_type type, uint8_t max_ch_groups)
{
	uint16_t i;
	int err;
	struct iwm_phy_db_entry *entry;

	/* Send all the channel-specific groups to operational fw */
	for (i = 0; i < max_ch_groups; i++) {
		entry = iwm_phy_db_get_section(sc, type, i);
		if (!entry)
			return EINVAL;

		if (!entry->size)
			continue;

		/* Send the requested PHY DB section */
		err = iwm_send_phy_db_cmd(sc, type, entry->size, entry->data);
		if (err) {
			DPRINTF(("%s: Can't SEND phy_db section %d (%d), "
			    "err %d\n", DEVNAME(sc), type, i, err));
			return err;
		}

		DELAY(1000);
		DPRINTFN(10, ("Sent PHY_DB HCMD, type = %d num = %d\n", type, i));
	}

	return 0;
}

int
iwm_send_phy_db_data(struct iwm_softc *sc)
{
	uint8_t *data = NULL;
	uint16_t size = 0;
	int err;

	DPRINTF(("Sending phy db data and configuration to runtime image\n"));

	/* Send PHY DB CFG section */
	err = iwm_phy_db_get_section_data(sc, IWM_PHY_DB_CFG, &data, &size, 0);
	if (err) {
		DPRINTF(("%s: Cannot get Phy DB cfg section, %d\n",
		    DEVNAME(sc), err));
		return err;
	}

	err = iwm_send_phy_db_cmd(sc, IWM_PHY_DB_CFG, size, data);
	if (err) {
		DPRINTF(("%s: Cannot send HCMD of Phy DB cfg section, %d\n",
		    DEVNAME(sc), err));
		return err;
	}

	err = iwm_phy_db_get_section_data(sc, IWM_PHY_DB_CALIB_NCH,
	    &data, &size, 0);
	if (err) {
		DPRINTF(("%s: Cannot get Phy DB non specific channel section, "
		    "%d\n", DEVNAME(sc), err));
		return err;
	}

	err = iwm_send_phy_db_cmd(sc, IWM_PHY_DB_CALIB_NCH, size, data);
	if (err) {
		DPRINTF(("%s: Cannot send HCMD of Phy DB non specific channel "
		    "sect, %d\n", DEVNAME(sc), err));
		return err;
	}

	/* Send all the TXP channel specific data */
	err = iwm_phy_db_send_all_channel_groups(sc,
	    IWM_PHY_DB_CALIB_CHG_PAPD, IWM_NUM_PAPD_CH_GROUPS);
	if (err) {
		DPRINTF(("%s: Cannot send channel specific PAPD groups, %d\n",
		    DEVNAME(sc), err));
		return err;
	}

	/* Send all the TXP channel specific data */
	err = iwm_phy_db_send_all_channel_groups(sc,
	    IWM_PHY_DB_CALIB_CHG_TXP, IWM_NUM_TXP_CH_GROUPS);
	if (err) {
		DPRINTF(("%s: Cannot send channel specific TX power groups, "
		    "%d\n", DEVNAME(sc), err));
		return err;
	}

	DPRINTF(("Finished sending phy db non channel data\n"));
	return 0;
}

/*
 * For the high priority TE use a time event type that has similar priority to
 * the FW's action scan priority.
 */
#define IWM_MVM_ROC_TE_TYPE_NORMAL IWM_TE_P2P_DEVICE_DISCOVERABLE
#define IWM_MVM_ROC_TE_TYPE_MGMT_TX IWM_TE_P2P_CLIENT_ASSOC

/* used to convert from time event API v2 to v1 */
#define IWM_TE_V2_DEP_POLICY_MSK (IWM_TE_V2_DEP_OTHER | IWM_TE_V2_DEP_TSF |\
			     IWM_TE_V2_EVENT_SOCIOPATHIC)
static inline uint16_t
iwm_te_v2_get_notify(uint16_t policy)
{
	return le16toh(policy) & IWM_TE_V2_NOTIF_MSK;
}

static inline uint16_t
iwm_te_v2_get_dep_policy(uint16_t policy)
{
	return (le16toh(policy) & IWM_TE_V2_DEP_POLICY_MSK) >>
		IWM_TE_V2_PLACEMENT_POS;
}

static inline uint16_t
iwm_te_v2_get_absence(uint16_t policy)
{
	return (le16toh(policy) & IWM_TE_V2_ABSENCE) >> IWM_TE_V2_ABSENCE_POS;
}

void
iwm_mvm_te_v2_to_v1(const struct iwm_time_event_cmd_v2 *cmd_v2,
	struct iwm_time_event_cmd_v1 *cmd_v1)
{
	cmd_v1->id_and_color = cmd_v2->id_and_color;
	cmd_v1->action = cmd_v2->action;
	cmd_v1->id = cmd_v2->id;
	cmd_v1->apply_time = cmd_v2->apply_time;
	cmd_v1->max_delay = cmd_v2->max_delay;
	cmd_v1->depends_on = cmd_v2->depends_on;
	cmd_v1->interval = cmd_v2->interval;
	cmd_v1->duration = cmd_v2->duration;
	if (cmd_v2->repeat == IWM_TE_V2_REPEAT_ENDLESS)
		cmd_v1->repeat = htole32(IWM_TE_V1_REPEAT_ENDLESS);
	else
		cmd_v1->repeat = htole32(cmd_v2->repeat);
	cmd_v1->max_frags = htole32(cmd_v2->max_frags);
	cmd_v1->interval_reciprocal = 0; /* unused */

	cmd_v1->dep_policy = htole32(iwm_te_v2_get_dep_policy(cmd_v2->policy));
	cmd_v1->is_present = htole32(!iwm_te_v2_get_absence(cmd_v2->policy));
	cmd_v1->notify = htole32(iwm_te_v2_get_notify(cmd_v2->policy));
}

int
iwm_mvm_send_time_event_cmd(struct iwm_softc *sc,
	const struct iwm_time_event_cmd_v2 *cmd)
{
	struct iwm_time_event_cmd_v1 cmd_v1;

	if (sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_TIME_EVENT_API_V2)
		return iwm_mvm_send_cmd_pdu(sc, IWM_TIME_EVENT_CMD,
		    IWM_CMD_SYNC, sizeof(*cmd), cmd);

	iwm_mvm_te_v2_to_v1(cmd, &cmd_v1);
	return iwm_mvm_send_cmd_pdu(sc, IWM_TIME_EVENT_CMD, IWM_CMD_SYNC,
	    sizeof(cmd_v1), &cmd_v1);
}

int
iwm_mvm_time_event_send_add(struct iwm_softc *sc, struct iwm_node *in,
	void *te_data, struct iwm_time_event_cmd_v2 *te_cmd)
{
	int ret;

	DPRINTF(("Add new TE, duration %d TU\n", le32toh(te_cmd->duration)));

	ret = iwm_mvm_send_time_event_cmd(sc, te_cmd);
	if (ret) {
		DPRINTF(("%s: Couldn't send IWM_TIME_EVENT_CMD: %d\n",
		    DEVNAME(sc), ret));
	}

	return ret;
}

void
iwm_mvm_protect_session(struct iwm_softc *sc, struct iwm_node *in,
	uint32_t duration, uint32_t max_delay)
{
	struct iwm_time_event_cmd_v2 time_cmd;

	memset(&time_cmd, 0, sizeof(time_cmd));

	time_cmd.action = htole32(IWM_FW_CTXT_ACTION_ADD);
	time_cmd.id_and_color =
	    htole32(IWM_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	time_cmd.id = htole32(IWM_TE_BSS_STA_AGGRESSIVE_ASSOC);

	time_cmd.apply_time = htole32(0);

	time_cmd.max_frags = IWM_TE_V2_FRAG_NONE;
	time_cmd.max_delay = htole32(max_delay);
	/* TODO: why do we need to interval = bi if it is not periodic? */
	time_cmd.interval = htole32(1);
	time_cmd.duration = htole32(duration);
	time_cmd.repeat = 1;
	time_cmd.policy
	    = htole32(IWM_TE_V2_NOTIF_HOST_EVENT_START |
	        IWM_TE_V2_NOTIF_HOST_EVENT_END |
		IWM_T2_V2_START_IMMEDIATELY);

	iwm_mvm_time_event_send_add(sc, in, /*te_data*/NULL, &time_cmd);
}

/*
 * NVM read access and content parsing.  We do not support
 * external NVM or writing NVM.
 */

/* list of NVM sections we are allowed/need to read */
const int nvm_to_read[] = {
	IWM_NVM_SECTION_TYPE_HW,
	IWM_NVM_SECTION_TYPE_SW,
	IWM_NVM_SECTION_TYPE_REGULATORY,
	IWM_NVM_SECTION_TYPE_CALIBRATION,
	IWM_NVM_SECTION_TYPE_PRODUCTION,
	IWM_NVM_SECTION_TYPE_HW_8000,
	IWM_NVM_SECTION_TYPE_MAC_OVERRIDE,
	IWM_NVM_SECTION_TYPE_PHY_SKU,
};

/* Default NVM size to read */
#define IWM_NVM_DEFAULT_CHUNK_SIZE	(2*1024)
#define IWM_MAX_NVM_SECTION_SIZE	8192

#define IWM_NVM_WRITE_OPCODE 1
#define IWM_NVM_READ_OPCODE 0

/* load nvm chunk response */
#define IWM_READ_NVM_CHUNK_SUCCEED		0
#define IWM_READ_NVM_CHUNK_INVALID_ADDRESS	1

int
iwm_nvm_read_chunk(struct iwm_softc *sc, uint16_t section,
	uint16_t offset, uint16_t length, uint8_t *data, uint16_t *len)
{
	offset = 0;
	struct iwm_nvm_access_cmd nvm_access_cmd = {
		.offset = htole16(offset),
		.length = htole16(length),
		.type = htole16(section),
		.op_code = IWM_NVM_READ_OPCODE,
	};
	struct iwm_nvm_access_resp *nvm_resp;
	struct iwm_rx_packet *pkt;
	struct iwm_host_cmd cmd = {
		.id = IWM_NVM_ACCESS_CMD,
		.flags = IWM_CMD_SYNC | IWM_CMD_WANT_SKB |
		    IWM_CMD_SEND_IN_RFKILL,
		.data = { &nvm_access_cmd, },
	};
	int ret, offset_read;
	size_t bytes_read;
	uint8_t *resp_data;

	cmd.len[0] = sizeof(struct iwm_nvm_access_cmd);

	ret = iwm_send_cmd(sc, &cmd);
	if (ret) {
		DPRINTF(("%s: Could not send NVM_ACCESS command (error=%d)\n",
		    DEVNAME(sc), ret));
		return ret;
	}

	pkt = cmd.resp_pkt;
	if (pkt->hdr.flags & IWM_CMD_FAILED_MSK) {
		DPRINTF(("%s: Bad return from IWM_NVM_ACCES_COMMAND (0x%08X)\n",
		    DEVNAME(sc), pkt->hdr.flags));
		ret = EIO;
		goto exit;
	}

	/* Extract NVM response */
	nvm_resp = (void *)pkt->data;

	ret = le16toh(nvm_resp->status);
	bytes_read = le16toh(nvm_resp->length);
	offset_read = le16toh(nvm_resp->offset);
	resp_data = nvm_resp->data;
	if (ret) {
		DPRINTF(("%s: NVM access command failed with status %d\n",
		    DEVNAME(sc), ret));
		ret = EINVAL;
		goto exit;
	}

	if (offset_read != offset) {
		DPRINTF(("%s: NVM ACCESS response with invalid offset %d\n",
		    DEVNAME(sc), offset_read));
		ret = EINVAL;
		goto exit;
	}

	if (bytes_read > length) {
		DPRINTF(("%s: NVM ACCESS response with too much data "
		"(%d bytes requested, %zd bytes received)\n",
		DEVNAME(sc), length, bytes_read));
		ret = EINVAL;
		goto exit;
	}

	memcpy(data + offset, resp_data, bytes_read);
	*len = bytes_read;

 exit:
	iwm_free_resp(sc, &cmd);
	return ret;
}

/*
 * Reads an NVM section completely.
 * NICs prior to 7000 family doesn't have a real NVM, but just read
 * section 0 which is the EEPROM. Because the EEPROM reading is unlimited
 * by uCode, we need to manually check in this case that we don't
 * overflow and try to read more than the EEPROM size.
 * For 7000 family NICs, we supply the maximal size we can read, and
 * the uCode fills the response with as much data as we can,
 * without overflowing, so no check is needed.
 */
int
iwm_nvm_read_section(struct iwm_softc *sc,
	uint16_t section, uint8_t *data, uint16_t *len, size_t max_len)
{
	uint16_t chunklen, seglen;
	int error = 0;

	DPRINTFN(2, ("%s: reading NVM section %d\n", DEVNAME(sc), section));

	chunklen = seglen = IWM_NVM_DEFAULT_CHUNK_SIZE;
	*len = 0;

	/* Read NVM chunks until exhausted (reading less than requested) */
	while (seglen == chunklen && *len < max_len) {
		error = iwm_nvm_read_chunk(sc,
		    section, *len, chunklen, data, &seglen);
		if (error) {
			DPRINTF(("%s: Cannot read from NVM section "
			    "%d at offset %d\n", DEVNAME(sc), section, *len));
			return error;
		}
		*len += seglen;
	}

	DPRINTFN(2, ("%s: NVM section %d read completed (%d bytes, error=%d)\n",
	    DEVNAME(sc), section, *len, error));
	return error;
}

uint8_t
iwm_fw_valid_tx_ant(struct iwm_softc *sc)
{
	uint8_t tx_ant;

	tx_ant = ((sc->sc_fw_phy_config & IWM_FW_PHY_CFG_TX_CHAIN)
	    >> IWM_FW_PHY_CFG_TX_CHAIN_POS);

	if (sc->sc_nvm.valid_tx_ant)
		tx_ant &= sc->sc_nvm.valid_tx_ant;

	return tx_ant;
}

uint8_t
iwm_fw_valid_rx_ant(struct iwm_softc *sc)
{
	uint8_t rx_ant;

	rx_ant = ((sc->sc_fw_phy_config & IWM_FW_PHY_CFG_RX_CHAIN)
	    >> IWM_FW_PHY_CFG_RX_CHAIN_POS);

	if (sc->sc_nvm.valid_rx_ant)
		rx_ant &= sc->sc_nvm.valid_rx_ant;

	return rx_ant;
}

/* NVM offsets (in words) definitions */
enum iwm_nvm_offsets {
	/* NVM HW-Section offset (in words) definitions */
	IWM_HW_ADDR = 0x15,

/* NVM SW-Section offset (in words) definitions */
	IWM_NVM_SW_SECTION = 0x1C0,
	IWM_NVM_VERSION = 0,
	IWM_RADIO_CFG = 1,
	IWM_SKU = 2,
	IWM_N_HW_ADDRS = 3,
	IWM_NVM_CHANNELS = 0x1E0 - IWM_NVM_SW_SECTION,

/* NVM calibration section offset (in words) definitions */
	IWM_NVM_CALIB_SECTION = 0x2B8,
	IWM_XTAL_CALIB = 0x316 - IWM_NVM_CALIB_SECTION
};

enum iwm_8000_nvm_offsets {
	/* NVM HW-Section offset (in words) definitions */
	IWM_HW_ADDR0_WFPM_8000 = 0x12,
	IWM_HW_ADDR1_WFPM_8000 = 0x16,
	IWM_HW_ADDR0_PCIE_8000 = 0x8A,
	IWM_HW_ADDR1_PCIE_8000 = 0x8E,
	IWM_MAC_ADDRESS_OVERRIDE_8000 = 1,

	/* NVM SW-Section offset (in words) definitions */
	IWM_NVM_SW_SECTION_8000 = 0x1C0,
	IWM_NVM_VERSION_8000 = 0,
	IWM_RADIO_CFG_8000 = 0,
	IWM_SKU_8000 = 2,
	IWM_N_HW_ADDRS_8000 = 3,

	/* NVM REGULATORY -Section offset (in words) definitions */
	IWM_NVM_CHANNELS_8000 = 0,
	IWM_NVM_LAR_OFFSET_8000_OLD = 0x4C7,
	IWM_NVM_LAR_OFFSET_8000 = 0x507,
	IWM_NVM_LAR_ENABLED_8000 = 0x7,

	/* NVM calibration section offset (in words) definitions */
	IWM_NVM_CALIB_SECTION_8000 = 0x2B8,
	IWM_XTAL_CALIB_8000 = 0x316 - IWM_NVM_CALIB_SECTION_8000
};

/* SKU Capabilities (actual values from NVM definition) */
enum nvm_sku_bits {
	IWM_NVM_SKU_CAP_BAND_24GHZ	= (1 << 0),
	IWM_NVM_SKU_CAP_BAND_52GHZ	= (1 << 1),
	IWM_NVM_SKU_CAP_11N_ENABLE	= (1 << 2),
	IWM_NVM_SKU_CAP_11AC_ENABLE	= (1 << 3),
	IWM_NVM_SKU_CAP_MIMO_DISABLE	= (1 << 5),
};

/* radio config bits (actual values from NVM definition) */
#define IWM_NVM_RF_CFG_DASH_MSK(x)   (x & 0x3)         /* bits 0-1   */
#define IWM_NVM_RF_CFG_STEP_MSK(x)   ((x >> 2)  & 0x3) /* bits 2-3   */
#define IWM_NVM_RF_CFG_TYPE_MSK(x)   ((x >> 4)  & 0x3) /* bits 4-5   */
#define IWM_NVM_RF_CFG_PNUM_MSK(x)   ((x >> 6)  & 0x3) /* bits 6-7   */
#define IWM_NVM_RF_CFG_TX_ANT_MSK(x) ((x >> 8)  & 0xF) /* bits 8-11  */
#define IWM_NVM_RF_CFG_RX_ANT_MSK(x) ((x >> 12) & 0xF) /* bits 12-15 */

#define IWM_NVM_RF_CFG_PNUM_MSK_8000(x)		(x & 0xF)
#define IWM_NVM_RF_CFG_DASH_MSK_8000(x)		((x >> 4) & 0xF)
#define IWM_NVM_RF_CFG_STEP_MSK_8000(x)		((x >> 8) & 0xF)
#define IWM_NVM_RF_CFG_TYPE_MSK_8000(x)		((x >> 12) & 0xFFF)
#define IWM_NVM_RF_CFG_TX_ANT_MSK_8000(x)	((x >> 24) & 0xF)
#define IWM_NVM_RF_CFG_RX_ANT_MSK_8000(x)	((x >> 28) & 0xF)

#define DEFAULT_MAX_TX_POWER 16

/**
 * enum iwm_nvm_channel_flags - channel flags in NVM
 * @IWM_NVM_CHANNEL_VALID: channel is usable for this SKU/geo
 * @IWM_NVM_CHANNEL_IBSS: usable as an IBSS channel
 * @IWM_NVM_CHANNEL_ACTIVE: active scanning allowed
 * @IWM_NVM_CHANNEL_RADAR: radar detection required
 * @IWM_NVM_CHANNEL_DFS: dynamic freq selection candidate
 * @IWM_NVM_CHANNEL_WIDE: 20 MHz channel okay (?)
 * @IWM_NVM_CHANNEL_40MHZ: 40 MHz channel okay (?)
 * @IWM_NVM_CHANNEL_80MHZ: 80 MHz channel okay (?)
 * @IWM_NVM_CHANNEL_160MHZ: 160 MHz channel okay (?)
 */
enum iwm_nvm_channel_flags {
	IWM_NVM_CHANNEL_VALID = (1 << 0),
	IWM_NVM_CHANNEL_IBSS = (1 << 1),
	IWM_NVM_CHANNEL_ACTIVE = (1 << 3),
	IWM_NVM_CHANNEL_RADAR = (1 << 4),
	IWM_NVM_CHANNEL_DFS = (1 << 7),
	IWM_NVM_CHANNEL_WIDE = (1 << 8),
	IWM_NVM_CHANNEL_40MHZ = (1 << 9),
	IWM_NVM_CHANNEL_80MHZ = (1 << 10),
	IWM_NVM_CHANNEL_160MHZ = (1 << 11),
};

void
iwm_init_channel_map(struct iwm_softc *sc, const uint16_t * const nvm_ch_flags,
			const uint8_t *nvm_channels, size_t nchan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_nvm_data *data = &sc->sc_nvm;
	int ch_idx;
	struct ieee80211_channel *channel;
	uint16_t ch_flags;
	int is_5ghz;
	int flags, hw_value;

	for (ch_idx = 0; ch_idx < nchan; ch_idx++) {
		ch_flags = le16_to_cpup(nvm_ch_flags + ch_idx);

		if (ch_idx >= IWM_NUM_2GHZ_CHANNELS &&
		    !data->sku_cap_band_52GHz_enable)
			ch_flags &= ~IWM_NVM_CHANNEL_VALID;

		if (!(ch_flags & IWM_NVM_CHANNEL_VALID)) {
			DPRINTF(("Ch. %d Flags %x [%sGHz] - No traffic\n",
			    nvm_channels[ch_idx],
			    ch_flags,
			    (ch_idx >= IWM_NUM_2GHZ_CHANNELS) ?
			    "5.2" : "2.4"));
			continue;
		}

		hw_value = nvm_channels[ch_idx];
		channel = &ic->ic_channels[hw_value];

		is_5ghz = ch_idx >= IWM_NUM_2GHZ_CHANNELS;
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

		if (!(ch_flags & IWM_NVM_CHANNEL_ACTIVE))
			channel->ic_flags |= IEEE80211_CHAN_PASSIVE;

		if (data->sku_cap_11n_enable)
			channel->ic_flags |= IEEE80211_CHAN_HT;
	}
}

void
iwm_setup_ht_rates(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* TX is supported with the same MCS as RX. */
	ic->ic_tx_mcs_set = IEEE80211_TX_MCS_SET_DEFINED;

	ic->ic_sup_mcs[0] = 0xff;		/* MCS 0-7 */

#ifdef notyet
	if (sc->sc_nvm.sku_cap_mimo_disable)
		return;

	if (iwm_fw_valid_rx_ant(sc) > 1)
		ic->ic_sup_mcs[1] = 0xff;	/* MCS 8-15 */
	if (iwm_fw_valid_rx_ant(sc) > 2)
		ic->ic_sup_mcs[2] = 0xff;	/* MCS 16-23 */
#endif
}

#define IWM_MAX_RX_BA_SESSIONS 16

void
iwm_mvm_sta_rx_agg(struct iwm_softc *sc, struct ieee80211_node *ni,
    uint8_t tid, uint16_t ssn, int start)
{
	struct iwm_mvm_add_sta_cmd_v7 cmd;
	struct iwm_node *in = (void *)ni;
	int ret, s;
	uint32_t status;

	if (start && sc->sc_rx_ba_sessions >= IWM_MAX_RX_BA_SESSIONS)
		return;

	memset(&cmd, 0, sizeof(cmd));

	cmd.sta_id = IWM_STATION_ID;
	cmd.mac_id_n_color
	    = htole32(IWM_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	cmd.add_modify = IWM_STA_MODE_MODIFY;

	if (start) {
		cmd.add_immediate_ba_tid = (uint8_t)tid;
		cmd.add_immediate_ba_ssn = ssn;
	} else {
		cmd.remove_immediate_ba_tid = (uint8_t)tid;
	}
	cmd.modify_mask = start ? IWM_STA_MODIFY_ADD_BA_TID :
	    IWM_STA_MODIFY_REMOVE_BA_TID;

	status = IWM_ADD_STA_SUCCESS;
	ret = iwm_mvm_send_add_sta_cmd_status(sc, &cmd, &status);
	if (ret)
		return;

	switch (status) {
	case IWM_ADD_STA_SUCCESS:
		DPRINTF(("RX BA Session %sed in fw\n",
		    start ? "start" : "stopp"));
		s = splnet();
		if (start)
			sc->sc_rx_ba_sessions++;
		else if (sc->sc_rx_ba_sessions > 0)
			/* check that restart flow didn't zero counter */
			sc->sc_rx_ba_sessions--;
		splx(s);
		break;
	case IWM_ADD_STA_IMMEDIATE_BA_FAILURE:
		ret = EIO;
		DPRINTF(("RX BA Session refused by fw\n"));
		break;
	default:
		ret = EIO;
		DPRINTF(("RX BA Session failed %sing, status 0x%x\n",
		    start ? "start" : "stopp", status));
		break;
	}
}

void
iwm_htprot_task(void *arg)
{
	struct iwm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_node *in = (void *)ic->ic_bss;
	int error;

	/* This call updates HT protection based on in->in_ni.ni_htop1. */
	error = iwm_mvm_mac_ctxt_changed(sc, in);
	if (error != 0)
		printf("%s: could not change HT protection: error %d\n",
		    DEVNAME(sc), error);
}

/*
 * This function is called by upper layer when HT protection settings in
 * beacons have changed.
 */
void
iwm_update_htprot(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct iwm_softc *sc = ic->ic_softc;

	/* assumes that ni == ic->ic_bss */
	task_add(systq, &sc->htprot_task);
}

void
iwm_ba_task(void *arg)
{
	struct iwm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	
	if (sc->ba_start)
		iwm_mvm_sta_rx_agg(sc, ni, sc->ba_tid, sc->ba_ssn, 1);
	else
		iwm_mvm_sta_rx_agg(sc, ni, sc->ba_tid, 0, 0);
}

/*
 * This function is called by upper layer when an ADDBA request is received
 * from another STA and before the ADDBA response is sent.
 */
int
iwm_ampdu_rx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
	struct iwm_softc *sc = IC2IFP(ic)->if_softc;

	if (sc->sc_rx_ba_sessions >= IWM_MAX_RX_BA_SESSIONS)
		return ENOSPC;

	sc->ba_start = 1;
	sc->ba_tid = tid;
	sc->ba_ssn = htole16(ba->ba_winstart);
	task_add(systq, &sc->ba_task);

	return 0; /* XXX firmware may still fail to add BA agreement... */
}

/*
 * This function is called by upper layer on teardown of an HT-immediate
 * Block Ack agreement (eg. upon receipt of a DELBA frame).
 */
void
iwm_ampdu_rx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct iwm_softc *sc = IC2IFP(ic)->if_softc;

	sc->ba_start = 0;
	sc->ba_tid = tid;
	task_add(systq, &sc->ba_task);
}

void
iwm_set_hw_address_8000(struct iwm_softc *sc, struct iwm_nvm_data *data,
	const uint16_t *mac_override, const uint16_t *nvm_hw)
{
	const uint8_t *hw_addr;

	if (mac_override) {
		static const uint8_t reserved_mac[] = {
			0x02, 0xcc, 0xaa, 0xff, 0xee, 0x00
		};

		hw_addr = (const uint8_t *)(mac_override +
				 IWM_MAC_ADDRESS_OVERRIDE_8000);

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

		DPRINTF(("%s: mac address from nvm override section invalid\n",
		    DEVNAME(sc)));
	}

	if (nvm_hw) {
		/* read the mac address from WFMP registers */
		uint32_t mac_addr0 =
		    htole32(iwm_read_prph(sc, IWM_WFMP_MAC_ADDR_0));
		uint32_t mac_addr1 =
		    htole32(iwm_read_prph(sc, IWM_WFMP_MAC_ADDR_1));

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

	printf("%s: mac address not found\n", DEVNAME(sc));
	memset(data->hw_addr, 0, sizeof(data->hw_addr));
}

int
iwm_parse_nvm_data(struct iwm_softc *sc,
	const uint16_t *nvm_hw, const uint16_t *nvm_sw,
	const uint16_t *nvm_calib, const uint16_t *mac_override,
	const uint16_t *phy_sku, const uint16_t *regulatory)
{
	struct iwm_nvm_data *data = &sc->sc_nvm;
	uint8_t hw_addr[ETHER_ADDR_LEN];
	uint32_t sku;

	data->nvm_version = le16_to_cpup(nvm_sw + IWM_NVM_VERSION);

	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000) {
		uint16_t radio_cfg = le16_to_cpup(nvm_sw + IWM_RADIO_CFG);
		data->radio_cfg_type = IWM_NVM_RF_CFG_TYPE_MSK(radio_cfg);
		data->radio_cfg_step = IWM_NVM_RF_CFG_STEP_MSK(radio_cfg);
		data->radio_cfg_dash = IWM_NVM_RF_CFG_DASH_MSK(radio_cfg);
		data->radio_cfg_pnum = IWM_NVM_RF_CFG_PNUM_MSK(radio_cfg);

		sku = le16_to_cpup(nvm_sw + IWM_SKU);
	} else {
		uint32_t radio_cfg =
		    le32_to_cpup((uint32_t *)(phy_sku + IWM_RADIO_CFG_8000));
		data->radio_cfg_type = IWM_NVM_RF_CFG_TYPE_MSK_8000(radio_cfg);
		data->radio_cfg_step = IWM_NVM_RF_CFG_STEP_MSK_8000(radio_cfg);
		data->radio_cfg_dash = IWM_NVM_RF_CFG_DASH_MSK_8000(radio_cfg);
		data->radio_cfg_pnum = IWM_NVM_RF_CFG_PNUM_MSK_8000(radio_cfg);
		data->valid_tx_ant = IWM_NVM_RF_CFG_TX_ANT_MSK_8000(radio_cfg);
		data->valid_rx_ant = IWM_NVM_RF_CFG_RX_ANT_MSK_8000(radio_cfg);

		sku = le32_to_cpup((uint32_t *)(phy_sku + IWM_SKU_8000));
	}

	data->sku_cap_band_24GHz_enable = sku & IWM_NVM_SKU_CAP_BAND_24GHZ;
	data->sku_cap_band_52GHz_enable = sku & IWM_NVM_SKU_CAP_BAND_52GHZ;
	data->sku_cap_11n_enable = sku & IWM_NVM_SKU_CAP_11N_ENABLE;
	data->sku_cap_mimo_disable = sku & IWM_NVM_SKU_CAP_MIMO_DISABLE;

	data->n_hw_addrs = le16_to_cpup(nvm_sw + IWM_N_HW_ADDRS);

	/* The byte order is little endian 16 bit, meaning 214365 */
	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000) {
		memcpy(hw_addr, nvm_hw + IWM_HW_ADDR, ETHER_ADDR_LEN);
		data->hw_addr[0] = hw_addr[1];
		data->hw_addr[1] = hw_addr[0];
		data->hw_addr[2] = hw_addr[3];
		data->hw_addr[3] = hw_addr[2];
		data->hw_addr[4] = hw_addr[5];
		data->hw_addr[5] = hw_addr[4];
	} else
		iwm_set_hw_address_8000(sc, data, mac_override, nvm_hw);

	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000)
		iwm_init_channel_map(sc, &nvm_sw[IWM_NVM_CHANNELS],
		    iwm_nvm_channels, nitems(iwm_nvm_channels));
	else
		iwm_init_channel_map(sc, &regulatory[IWM_NVM_CHANNELS_8000],
		    iwm_nvm_channels_8000, nitems(iwm_nvm_channels_8000));

	data->calib_version = 255;   /* TODO:
					this value will prevent some checks from
					failing, we need to check if this
					field is still needed, and if it does,
					where is it in the NVM */

	return 0;
}

int
iwm_parse_nvm_sections(struct iwm_softc *sc, struct iwm_nvm_section *sections)
{
	const uint16_t *hw, *sw, *calib, *mac_override = NULL, *phy_sku = NULL;
	const uint16_t *regulatory = NULL;

	/* Checking for required sections */
	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000) {
		if (!sections[IWM_NVM_SECTION_TYPE_SW].data ||
		    !sections[IWM_NVM_SECTION_TYPE_HW].data) {
			DPRINTF(("%s: Can't parse empty OTP/NVM sections\n",
			    DEVNAME(sc)));
			return ENOENT;
		}

		hw = (const uint16_t *) sections[IWM_NVM_SECTION_TYPE_HW].data;
	} else if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000) {
		/* SW and REGULATORY sections are mandatory */
		if (!sections[IWM_NVM_SECTION_TYPE_SW].data ||
		    !sections[IWM_NVM_SECTION_TYPE_REGULATORY].data) {
			DPRINTF(("%s: Can't parse empty OTP/NVM sections\n",
			    DEVNAME(sc)));
			return ENOENT;
		}
		/* MAC_OVERRIDE or at least HW section must exist */
		if (!sections[IWM_NVM_SECTION_TYPE_HW_8000].data &&
		    !sections[IWM_NVM_SECTION_TYPE_MAC_OVERRIDE].data) {
			DPRINTF(("%s: Can't parse mac_address, empty "
			    "sections\n", DEVNAME(sc)));
			return ENOENT;
		}

		/* PHY_SKU section is mandatory in B0 */
		if (!sections[IWM_NVM_SECTION_TYPE_PHY_SKU].data) {
			DPRINTF(("%s: Can't parse phy_sku in B0, empty "
			    "sections\n", DEVNAME(sc)));
			return ENOENT;
		}

		regulatory = (const uint16_t *)
		    sections[IWM_NVM_SECTION_TYPE_REGULATORY].data;
		hw = (const uint16_t *)
		    sections[IWM_NVM_SECTION_TYPE_HW_8000].data;
		mac_override =
			(const uint16_t *)
			sections[IWM_NVM_SECTION_TYPE_MAC_OVERRIDE].data;
		phy_sku = (const uint16_t *)
		    sections[IWM_NVM_SECTION_TYPE_PHY_SKU].data;
	} else {
		panic("unknown device family %d\n", sc->sc_device_family);
	}

	sw = (const uint16_t *)sections[IWM_NVM_SECTION_TYPE_SW].data;
	calib = (const uint16_t *)
	    sections[IWM_NVM_SECTION_TYPE_CALIBRATION].data;

	return iwm_parse_nvm_data(sc, hw, sw, calib, mac_override,
	    phy_sku, regulatory);
}

int
iwm_nvm_init(struct iwm_softc *sc)
{
	struct iwm_nvm_section nvm_sections[IWM_NVM_NUM_OF_SECTIONS];
	int i, section, error;
	uint16_t len;
	uint8_t *buf;
	const size_t bufsz = IWM_MAX_NVM_SECTION_SIZE;

	memset(nvm_sections, 0, sizeof(nvm_sections));

	buf = malloc(bufsz, M_DEVBUF, M_WAIT);
	if (buf == NULL)
		return ENOMEM;

	for (i = 0; i < nitems(nvm_to_read); i++) {
		section = nvm_to_read[i];
		KASSERT(section <= nitems(nvm_sections));

		error = iwm_nvm_read_section(sc, section, buf, &len, bufsz);
		if (error) {
			error = 0;
			continue;
		}
		nvm_sections[section].data = malloc(len, M_DEVBUF, M_WAIT);
		if (nvm_sections[section].data == NULL) {
			error = ENOMEM;
			break;
		}
		memcpy(nvm_sections[section].data, buf, len);
		nvm_sections[section].length = len;
	}
	free(buf, M_DEVBUF, bufsz);
	if (error == 0)
		error = iwm_parse_nvm_sections(sc, nvm_sections);

	for (i = 0; i < IWM_NVM_NUM_OF_SECTIONS; i++) {
		if (nvm_sections[i].data != NULL)
			free(nvm_sections[i].data, M_DEVBUF,
			    nvm_sections[i].length);
	}

	return error;
}

/*
 * Firmware loading gunk.  This is kind of a weird hybrid between the
 * iwn driver and the Linux iwlwifi driver.
 */

int
iwm_firmware_load_sect(struct iwm_softc *sc, uint32_t dst_addr,
	const uint8_t *section, uint32_t byte_cnt)
{
	int error = EINVAL;
	uint32_t chunk_sz, offset;

	chunk_sz = MIN(IWM_FH_MEM_TB_MAX_LENGTH, byte_cnt);

	for (offset = 0; offset < byte_cnt; offset += chunk_sz) {
		uint32_t addr, len;
		const uint8_t *data;

		addr = dst_addr + offset;
		len = MIN(chunk_sz, byte_cnt - offset);
		data = section + offset;

		error = iwm_firmware_load_chunk(sc, addr, data, len);
		if (error)
			break;
	}

	return error;
}

int
iwm_firmware_load_chunk(struct iwm_softc *sc, uint32_t dst_addr,
	const uint8_t *chunk, uint32_t byte_cnt)
{
	struct iwm_dma_info *dma = &sc->fw_dma;
	int error;

	/* Copy firmware chunk into pre-allocated DMA-safe memory. */
	memcpy(dma->vaddr, chunk, byte_cnt);
	bus_dmamap_sync(sc->sc_dmat,
	    dma->map, 0, byte_cnt, BUS_DMASYNC_PREWRITE);

	if (dst_addr >= IWM_FW_MEM_EXTENDED_START &&
	    dst_addr <= IWM_FW_MEM_EXTENDED_END)
		iwm_set_bits_prph(sc, IWM_LMPM_CHICK,
		    IWM_LMPM_CHICK_EXTENDED_ADDR_SPACE);

	sc->sc_fw_chunk_done = 0;

	if (!iwm_nic_lock(sc))
		return EBUSY;

	IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_CONFIG_REG(IWM_FH_SRVC_CHNL),
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_PAUSE);
	IWM_WRITE(sc, IWM_FH_SRVC_CHNL_SRAM_ADDR_REG(IWM_FH_SRVC_CHNL),
	    dst_addr);
	IWM_WRITE(sc, IWM_FH_TFDIB_CTRL0_REG(IWM_FH_SRVC_CHNL),
	    dma->paddr & IWM_FH_MEM_TFDIB_DRAM_ADDR_LSB_MSK);
	IWM_WRITE(sc, IWM_FH_TFDIB_CTRL1_REG(IWM_FH_SRVC_CHNL),
	    (iwm_get_dma_hi_addr(dma->paddr)
	      << IWM_FH_MEM_TFDIB_REG1_ADDR_BITSHIFT) | byte_cnt);
	IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_BUF_STS_REG(IWM_FH_SRVC_CHNL),
	    1 << IWM_FH_TCSR_CHNL_TX_BUF_STS_REG_POS_TB_NUM |
	    1 << IWM_FH_TCSR_CHNL_TX_BUF_STS_REG_POS_TB_IDX |
	    IWM_FH_TCSR_CHNL_TX_BUF_STS_REG_VAL_TFDB_VALID);
	IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_CONFIG_REG(IWM_FH_SRVC_CHNL),
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE    |
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_DISABLE |
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_ENDTFD);

	iwm_nic_unlock(sc);

	/* wait 1s for this segment to load */
	while (!sc->sc_fw_chunk_done)
		if ((error = tsleep(&sc->sc_fw, 0, "iwmfw", hz)) != 0)
			break;

	if (!sc->sc_fw_chunk_done)
		printf("%s: fw chunk addr 0x%x len %d failed to load\n",
		    DEVNAME(sc), dst_addr, byte_cnt);

	if (dst_addr >= IWM_FW_MEM_EXTENDED_START &&
	    dst_addr <= IWM_FW_MEM_EXTENDED_END && iwm_nic_lock(sc)) {
		iwm_clear_bits_prph(sc, IWM_LMPM_CHICK,
		    IWM_LMPM_CHICK_EXTENDED_ADDR_SPACE);
		iwm_nic_unlock(sc);
	}

	return error;
}

int
iwm_load_firmware_7000(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	struct iwm_fw_sects *fws;
	int error, i;
	void *data;
	uint32_t dlen;
	uint32_t offset;

	sc->sc_uc.uc_intr = 0;

	fws = &sc->sc_fw.fw_sects[ucode_type];
	for (i = 0; i < fws->fw_count; i++) {
		data = fws->fw_sect[i].fws_data;
		dlen = fws->fw_sect[i].fws_len;
		offset = fws->fw_sect[i].fws_devoff;
		DPRINTFN(4, ("LOAD FIRMWARE type %d offset %u len %d\n",
		    ucode_type, offset, dlen));
		if (dlen > sc->sc_fwdmasegsz) {
			DPRINTF(("chunk %d too large (%d bytes)\n", i, dlen));
			error = EFBIG;
		} else
			error = iwm_firmware_load_sect(sc, offset, data, dlen);
		if (error) {
			printf("%s: could not load firmware chunk %u of %u\n",
			    DEVNAME(sc), i, fws->fw_count);
			return error;
		}
	}

	IWM_WRITE(sc, IWM_CSR_RESET, 0);

	return 0;
}

int
iwm_load_cpu_sections_8000(struct iwm_softc *sc, struct iwm_fw_sects *fws,
    int cpu, int *first_ucode_section)
{
	int shift_param;
	int i, error = 0, sec_num = 0x1;
	uint32_t val, last_read_idx = 0;
	void *data;
	uint32_t dlen;
	uint32_t offset;

	if (cpu == 1) {
		shift_param = 0;
		*first_ucode_section = 0;
	} else {
		shift_param = 16;
		(*first_ucode_section)++;
	}

	for (i = *first_ucode_section; i < IWM_UCODE_SECT_MAX; i++) {
		last_read_idx = i;
		data = fws->fw_sect[i].fws_data;
		dlen = fws->fw_sect[i].fws_len;
		offset = fws->fw_sect[i].fws_devoff;

		/*
		 * CPU1_CPU2_SEPARATOR_SECTION delimiter - separate between
		 * CPU1 to CPU2.
		 * PAGING_SEPARATOR_SECTION delimiter - separate between
		 * CPU2 non paged to CPU2 paging sec.
		 */
		if (!data || offset == IWM_CPU1_CPU2_SEPARATOR_SECTION ||
		    offset == IWM_PAGING_SEPARATOR_SECTION)
			break;

		DPRINTFN(4, ("LOAD FIRMWARE chunk %d offset 0x%x len %d "
		    "for cpu %d\n", i, offset, dlen, cpu));

		if (dlen > sc->sc_fwdmasegsz) {
			DPRINTF(("chunk %d too large (%d bytes)\n", i, dlen));
			error = EFBIG;
		} else
			error = iwm_firmware_load_sect(sc, offset, data, dlen);
		if (error) {
			printf("%s: could not load firmware chunk %d "
			    "(error %d)\n", DEVNAME(sc), i, error);
			return error;
		}

		/* Notify the ucode of the loaded section number and status */
		if (iwm_nic_lock(sc)) {
			val = IWM_READ(sc, IWM_FH_UCODE_LOAD_STATUS);
			val = val | (sec_num << shift_param);
			IWM_WRITE(sc, IWM_FH_UCODE_LOAD_STATUS, val);
			sec_num = (sec_num << 1) | 0x1;
			iwm_nic_unlock(sc);

			/*
			 * The firmware won't load correctly without this delay.
			 */
			DELAY(8000);
		}
	}

	*first_ucode_section = last_read_idx;

	if (iwm_nic_lock(sc)) {
		if (cpu == 1)
			IWM_WRITE(sc, IWM_FH_UCODE_LOAD_STATUS, 0xFFFF);
		else
			IWM_WRITE(sc, IWM_FH_UCODE_LOAD_STATUS, 0xFFFFFFFF);
		iwm_nic_unlock(sc);
	}

	return 0;
}

int
iwm_load_firmware_8000(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	struct iwm_fw_sects *fws;
	int error = 0;
	int first_ucode_section;

	DPRINTF(("%s: loading ucode type %d\n", DEVNAME(sc), ucode_type));

	fws = &sc->sc_fw.fw_sects[ucode_type];

	/* configure the ucode to be ready to get the secured image */
	/* release CPU reset */
	iwm_write_prph(sc, IWM_RELEASE_CPU_RESET, IWM_RELEASE_CPU_RESET_BIT);

	/* load to FW the binary Secured sections of CPU1 */
	error = iwm_load_cpu_sections_8000(sc, fws, 1, &first_ucode_section);
	if (error)
		return error;

	/* load to FW the binary sections of CPU2 */
	return iwm_load_cpu_sections_8000(sc, fws, 2, &first_ucode_section);
}

int
iwm_load_firmware(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	int error, w;

	if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000)
		error = iwm_load_firmware_8000(sc, ucode_type);
	else
		error = iwm_load_firmware_7000(sc, ucode_type);

	if (error)
		return error;

	/* wait for the firmware to load */
	for (w = 0; !sc->sc_uc.uc_intr && w < 10; w++) {
		error = tsleep(&sc->sc_uc, 0, "iwmuc", hz/10);
	}
	if (error || !sc->sc_uc.uc_ok) {
		printf("%s: could not load firmware\n", DEVNAME(sc));
		if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000) {
			DPRINTF(("%s: cpu1 status: 0x%x\n", DEVNAME(sc),
			    iwm_read_prph(sc, IWM_SB_CPU_1_STATUS)));
			DPRINTF(("%s: cpu2 status: 0x%x\n", DEVNAME(sc),
			    iwm_read_prph(sc, IWM_SB_CPU_2_STATUS)));
		}
	}

	/*
	 * Give the firmware some time to initialize.
	 * Accessing it too early causes errors.
	 */
	tsleep(&w, PCATCH, "iwmfwinit", hz);

	return error;
}

int
iwm_start_fw(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	int error;

	IWM_WRITE(sc, IWM_CSR_INT, ~0);

	if ((error = iwm_nic_init(sc)) != 0) {
		printf("%s: unable to init nic\n", DEVNAME(sc));
		return error;
	}

	/* make sure rfkill handshake bits are cleared */
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR, IWM_CSR_UCODE_SW_BIT_RFKILL);
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR,
	    IWM_CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);

	/* clear (again), then enable host interrupts */
	IWM_WRITE(sc, IWM_CSR_INT, ~0);
	iwm_enable_interrupts(sc);

	/* really make sure rfkill handshake bits are cleared */
	/* maybe we should write a few times more?  just to make sure */
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR, IWM_CSR_UCODE_SW_BIT_RFKILL);
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR, IWM_CSR_UCODE_SW_BIT_RFKILL);

	/* Load the given image to the HW */
	return iwm_load_firmware(sc, ucode_type);
}

int
iwm_send_tx_ant_cfg(struct iwm_softc *sc, uint8_t valid_tx_ant)
{
	struct iwm_tx_ant_cfg_cmd tx_ant_cmd = {
		.valid = htole32(valid_tx_ant),
	};

	return iwm_mvm_send_cmd_pdu(sc, IWM_TX_ANT_CONFIGURATION_CMD,
	    IWM_CMD_SYNC, sizeof(tx_ant_cmd), &tx_ant_cmd);
}

int
iwm_send_phy_cfg_cmd(struct iwm_softc *sc)
{
	struct iwm_phy_cfg_cmd phy_cfg_cmd;
	enum iwm_ucode_type ucode_type = sc->sc_uc_current;

	/* Set parameters */
	phy_cfg_cmd.phy_cfg = htole32(sc->sc_fw_phy_config);
	phy_cfg_cmd.calib_control.event_trigger =
	    sc->sc_default_calib[ucode_type].event_trigger;
	phy_cfg_cmd.calib_control.flow_trigger =
	    sc->sc_default_calib[ucode_type].flow_trigger;

	DPRINTFN(10, ("Sending Phy CFG command: 0x%x\n", phy_cfg_cmd.phy_cfg));
	return iwm_mvm_send_cmd_pdu(sc, IWM_PHY_CONFIGURATION_CMD, IWM_CMD_SYNC,
	    sizeof(phy_cfg_cmd), &phy_cfg_cmd);
}

int
iwm_mvm_load_ucode_wait_alive(struct iwm_softc *sc,
	enum iwm_ucode_type ucode_type)
{
	enum iwm_ucode_type old_type = sc->sc_uc_current;
	int error;

	if ((error = iwm_read_firmware(sc, ucode_type)) != 0)
		return error;

	sc->sc_uc_current = ucode_type;
	error = iwm_start_fw(sc, ucode_type);
	if (error) {
		sc->sc_uc_current = old_type;
		return error;
	}

	return iwm_post_alive(sc);
}

/*
 * mvm misc bits
 */

int
iwm_run_init_mvm_ucode(struct iwm_softc *sc, int justnvm)
{
	int error;

	/* do not operate with rfkill switch turned on */
	if ((sc->sc_flags & IWM_FLAG_RFKILL) && !justnvm) {
		printf("%s: radio is disabled by hardware switch\n",
		    DEVNAME(sc));
		return EPERM;
	}

	sc->sc_init_complete = 0;
	if ((error = iwm_mvm_load_ucode_wait_alive(sc,
	    IWM_UCODE_TYPE_INIT)) != 0) {
		printf("%s: failed to load init firmware\n", DEVNAME(sc));
		return error;
	}

	if (justnvm) {
		if ((error = iwm_nvm_init(sc)) != 0) {
			printf("%s: failed to read nvm\n", DEVNAME(sc));
			return error;
		}

		if (IEEE80211_ADDR_EQ(etheranyaddr, sc->sc_ic.ic_myaddr))
			IEEE80211_ADDR_COPY(sc->sc_ic.ic_myaddr,
			    sc->sc_nvm.hw_addr);

		return 0;
	}

	if ((error = iwm_send_bt_init_conf(sc)) != 0) {
		DPRINTF(("%s: failed to send bt coex configuration: %d\n",
		    DEVNAME(sc), error));
		return error;
	}

	/* Init Smart FIFO. */
	error = iwm_mvm_sf_config(sc, IWM_SF_INIT_OFF);
	if (error != 0)
		return error;

	/* Send TX valid antennas before triggering calibrations */
	if ((error = iwm_send_tx_ant_cfg(sc, iwm_fw_valid_tx_ant(sc))) != 0) {
		DPRINTF(("%s: failed to send antennas before calibration: %d\n",
		    DEVNAME(sc), error));
		return error;
	}

	/*
	 * Send phy configurations command to init uCode
	 * to start the 16.0 uCode init image internal calibrations.
	 */
	if ((error = iwm_send_phy_cfg_cmd(sc)) != 0 ) {
		DPRINTF(("%s: failed to run internal calibration: %d\n",
		    DEVNAME(sc), error));
		return error;
	}

	/*
	 * Nothing to do but wait for the init complete notification
	 * from the firmware
	 */
	while (!sc->sc_init_complete)
		if ((error = tsleep(&sc->sc_init_complete,
		    0, "iwminit", 2*hz)) != 0)
			break;

	DPRINTF(("%s: init %scomplete\n", DEVNAME(sc),
	    sc->sc_init_complete ? "" : "not "));

	return error;
}

/*
 * receive side
 */

/* (re)stock rx ring, called at init-time and at runtime */
int
iwm_rx_addbuf(struct iwm_softc *sc, int size, int idx)
{
	struct iwm_rx_ring *ring = &sc->rxq;
	struct iwm_rx_data *data = &ring->data[idx];
	struct mbuf *m;
	int error;
	int fatal = 0;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	if (size <= MCLBYTES) {
		MCLGET(m, M_DONTWAIT);
	} else {
		MCLGETI(m, M_DONTWAIT, NULL, IWM_RBUF_SIZE);
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
	if ((error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT)) != 0) {
		/* XXX */
		if (fatal)
			panic("iwm: could not load RX mbuf");
		m_freem(m);
		return error;
	}
	data->m = m;
	bus_dmamap_sync(sc->sc_dmat, data->map, 0, size, BUS_DMASYNC_PREREAD);

	/* Update RX descriptor. */
	ring->desc[idx] = htole32(data->map->dm_segs[0].ds_addr >> 8);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map,
	    idx * sizeof(uint32_t), sizeof(uint32_t), BUS_DMASYNC_PREWRITE);

	return 0;
}

#define IWM_RSSI_OFFSET 50
int
iwm_mvm_calc_rssi(struct iwm_softc *sc, struct iwm_rx_phy_info *phy_info)
{
	int rssi_a, rssi_b, rssi_a_dbm, rssi_b_dbm, max_rssi_dbm;
	uint32_t agc_a, agc_b;
	uint32_t val;

	val = le32toh(phy_info->non_cfg_phy[IWM_RX_INFO_AGC_IDX]);
	agc_a = (val & IWM_OFDM_AGC_A_MSK) >> IWM_OFDM_AGC_A_POS;
	agc_b = (val & IWM_OFDM_AGC_B_MSK) >> IWM_OFDM_AGC_B_POS;

	val = le32toh(phy_info->non_cfg_phy[IWM_RX_INFO_RSSI_AB_IDX]);
	rssi_a = (val & IWM_OFDM_RSSI_INBAND_A_MSK) >> IWM_OFDM_RSSI_A_POS;
	rssi_b = (val & IWM_OFDM_RSSI_INBAND_B_MSK) >> IWM_OFDM_RSSI_B_POS;

	/*
	 * dBm = rssi dB - agc dB - constant.
	 * Higher AGC (higher radio gain) means lower signal.
	 */
	rssi_a_dbm = rssi_a - IWM_RSSI_OFFSET - agc_a;
	rssi_b_dbm = rssi_b - IWM_RSSI_OFFSET - agc_b;
	max_rssi_dbm = MAX(rssi_a_dbm, rssi_b_dbm);

	DPRINTF(("Rssi In A %d B %d Max %d AGCA %d AGCB %d\n",
	    rssi_a_dbm, rssi_b_dbm, max_rssi_dbm, agc_a, agc_b));

	return max_rssi_dbm;
}

/*
 * iwm_mvm_get_signal_strength - use new rx PHY INFO API
 * values are reported by the fw as positive values - need to negate
 * to obtain their dBM.  Account for missing antennas by replacing 0
 * values by -256dBm: practically 0 power and a non-feasible 8 bit value.
 */
int
iwm_mvm_get_signal_strength(struct iwm_softc *sc, struct iwm_rx_phy_info *phy_info)
{
	int energy_a, energy_b, energy_c, max_energy;
	uint32_t val;

	val = le32toh(phy_info->non_cfg_phy[IWM_RX_INFO_ENERGY_ANT_ABC_IDX]);
	energy_a = (val & IWM_RX_INFO_ENERGY_ANT_A_MSK) >>
	    IWM_RX_INFO_ENERGY_ANT_A_POS;
	energy_a = energy_a ? -energy_a : -256;
	energy_b = (val & IWM_RX_INFO_ENERGY_ANT_B_MSK) >>
	    IWM_RX_INFO_ENERGY_ANT_B_POS;
	energy_b = energy_b ? -energy_b : -256;
	energy_c = (val & IWM_RX_INFO_ENERGY_ANT_C_MSK) >>
	    IWM_RX_INFO_ENERGY_ANT_C_POS;
	energy_c = energy_c ? -energy_c : -256;
	max_energy = MAX(energy_a, energy_b);
	max_energy = MAX(max_energy, energy_c);

	DPRINTFN(12, ("energy In A %d B %d C %d , and max %d\n",
	    energy_a, energy_b, energy_c, max_energy));

	return max_energy;
}

void
iwm_mvm_rx_rx_phy_cmd(struct iwm_softc *sc,
	struct iwm_rx_packet *pkt, struct iwm_rx_data *data)
{
	struct iwm_rx_phy_info *phy_info = (void *)pkt->data;

	DPRINTFN(20, ("received PHY stats\n"));
	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof(*pkt),
	    sizeof(*phy_info), BUS_DMASYNC_POSTREAD);

	memcpy(&sc->sc_last_phy_info, phy_info, sizeof(sc->sc_last_phy_info));
}

/*
 * Retrieve the average noise (in dBm) among receivers.
 */
int
iwm_get_noise(const struct iwm_mvm_statistics_rx_non_phy *stats)
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

/*
 * iwm_mvm_rx_rx_mpdu - IWM_REPLY_RX_MPDU_CMD handler
 *
 * Handles the actual data of the Rx packet from the fw
 */
void
iwm_mvm_rx_rx_mpdu(struct iwm_softc *sc,
	struct iwm_rx_packet *pkt, struct iwm_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct ieee80211_channel *c = NULL;
	struct ieee80211_rxinfo rxi;
	struct mbuf *m;
	struct iwm_rx_phy_info *phy_info;
	struct iwm_rx_mpdu_res_start *rx_res;
	int device_timestamp;
	uint32_t len;
	uint32_t rx_pkt_status;
	int rssi;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, IWM_RBUF_SIZE,
	    BUS_DMASYNC_POSTREAD);

	phy_info = &sc->sc_last_phy_info;
	rx_res = (struct iwm_rx_mpdu_res_start *)pkt->data;
	wh = (struct ieee80211_frame *)(pkt->data + sizeof(*rx_res));
	len = le16toh(rx_res->byte_count);
	rx_pkt_status = le32toh(*(uint32_t *)(pkt->data + sizeof(*rx_res) + len));

	m = data->m;
	m->m_data = pkt->data + sizeof(*rx_res);
	m->m_pkthdr.len = m->m_len = len;

	if (__predict_false(phy_info->cfg_phy_cnt > 20)) {
		DPRINTF(("dsp size out of range [0,20]: %d\n",
		    phy_info->cfg_phy_cnt));
		return;
	}

	if (!(rx_pkt_status & IWM_RX_MPDU_RES_STATUS_CRC_OK) ||
	    !(rx_pkt_status & IWM_RX_MPDU_RES_STATUS_OVERRUN_OK)) {
		DPRINTF(("Bad CRC or FIFO: 0x%08X.\n", rx_pkt_status));
		return; /* drop */
	}

	device_timestamp = le32toh(phy_info->system_timestamp);

	if (sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_RX_ENERGY_API) {
		rssi = iwm_mvm_get_signal_strength(sc, phy_info);
	} else {
		rssi = iwm_mvm_calc_rssi(sc, phy_info);
	}
	rssi = (0 - IWM_MIN_DBM) + rssi;	/* normalize */
	rssi = MIN(rssi, ic->ic_max_rssi);	/* clip to max. 100% */

	/* replenish ring for the buffer we're going to feed to the sharks */
	if (iwm_rx_addbuf(sc, IWM_RBUF_SIZE, sc->rxq.cur) != 0)
		return;

	if (le32toh(phy_info->channel) < nitems(ic->ic_channels))
		c = &ic->ic_channels[le32toh(phy_info->channel)];

	memset(&rxi, 0, sizeof(rxi));
	rxi.rxi_rssi = rssi;
	rxi.rxi_tstamp = device_timestamp;
	ni = ieee80211_find_rxnode(ic, wh);
	if (c)
		ni->ni_chan = c;

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct iwm_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		if (phy_info->phy_flags & htole16(IWM_PHY_INFO_FLAG_SHPREAMBLE))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[phy_info->channel].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[phy_info->channel].ic_flags);
		tap->wr_dbm_antsignal = (int8_t)rssi;
		tap->wr_dbm_antnoise = (int8_t)sc->sc_noise;
		tap->wr_tsft = phy_info->system_timestamp;
		if (phy_info->phy_flags &
		    htole16(IWM_RX_RES_PHY_FLAGS_OFDM_HT)) {
#ifdef notyet
			uint8_t mcs = (phy_info->rate_n_flags &
			    htole32(IWM_RATE_HT_MCS_RATE_CODE_MSK));
#endif
			/* XXX need a way to pass current MCS in 11n mode */
			tap->wr_rate = 0;
		} else {
			uint8_t rate = (phy_info->rate_n_flags &
			    htole32(IWM_RATE_LEGACY_RATE_MSK));
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

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
	}
#endif
	ieee80211_input(IC2IFP(ic), m, ni, &rxi);
	ieee80211_release_node(ic, ni);
}

void
iwm_mvm_rx_tx_cmd_single(struct iwm_softc *sc, struct iwm_rx_packet *pkt,
	struct iwm_node *in)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	struct iwm_mvm_tx_resp *tx_resp = (void *)pkt->data;
	int status = le16toh(tx_resp->status.status) & IWM_TX_STATUS_MSK;
	int failack = tx_resp->failure_frame;

	KASSERT(tx_resp->frame_count == 1);

	/* Update rate control statistics. */
	in->in_amn.amn_txcnt++;
	if (failack > 0) {
		in->in_amn.amn_retrycnt++;
	}

	if (status != IWM_TX_STATUS_SUCCESS &&
	    status != IWM_TX_STATUS_DIRECT_DONE)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;
}

void
iwm_mvm_rx_tx_cmd(struct iwm_softc *sc,
	struct iwm_rx_packet *pkt, struct iwm_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	struct iwm_cmd_header *cmd_hdr = &pkt->hdr;
	int idx = cmd_hdr->idx;
	int qid = cmd_hdr->qid;
	struct iwm_tx_ring *ring = &sc->txq[qid];
	struct iwm_tx_data *txd = &ring->data[idx];
	struct iwm_node *in = txd->in;

	if (txd->done) {
		DPRINTF(("%s: got tx interrupt that's already been handled!\n",
		    DEVNAME(sc)));
		return;
	}

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, IWM_RBUF_SIZE,
	    BUS_DMASYNC_POSTREAD);

	sc->sc_tx_timer = 0;

	iwm_mvm_rx_tx_cmd_single(sc, pkt, in);

	/* Unmap and free mbuf. */
	bus_dmamap_sync(sc->sc_dmat, txd->map, 0, txd->map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, txd->map);
	m_freem(txd->m);

	DPRINTFN(8, ("free txd %p, in %p\n", txd, txd->in));
	KASSERT(txd->done == 0);
	txd->done = 1;
	KASSERT(txd->in);

	txd->m = NULL;
	txd->in = NULL;
	ieee80211_release_node(ic, &in->in_ni);

	if (--ring->queued < IWM_TX_RING_LOMARK) {
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

int
iwm_mvm_binding_cmd(struct iwm_softc *sc, struct iwm_node *in, uint32_t action)
{
	struct iwm_binding_cmd cmd;
	struct iwm_mvm_phy_ctxt *phyctxt = in->in_phyctxt;
	int i, ret;
	uint32_t status;

	memset(&cmd, 0, sizeof(cmd));

	cmd.id_and_color
	    = htole32(IWM_FW_CMD_ID_AND_COLOR(phyctxt->id, phyctxt->color));
	cmd.action = htole32(action);
	cmd.phy = htole32(IWM_FW_CMD_ID_AND_COLOR(phyctxt->id, phyctxt->color));

	cmd.macs[0] = htole32(IWM_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	for (i = 1; i < IWM_MAX_MACS_IN_BINDING; i++)
		cmd.macs[i] = htole32(IWM_FW_CTXT_INVALID);

	status = 0;
	ret = iwm_mvm_send_cmd_pdu_status(sc, IWM_BINDING_CONTEXT_CMD,
	    sizeof(cmd), &cmd, &status);
	if (ret) {
		DPRINTF(("%s: Failed to send binding (action:%d): %d\n",
		    DEVNAME(sc), action, ret));
		return ret;
	}

	if (status) {
		DPRINTF(("%s: Binding command failed: %u\n", DEVNAME(sc),
		    status));
		ret = EIO;
	}

	return ret;
}

int
iwm_mvm_binding_update(struct iwm_softc *sc, struct iwm_node *in)
{
	return iwm_mvm_binding_cmd(sc, in, IWM_FW_CTXT_ACTION_MODIFY);
}

int
iwm_mvm_binding_add_vif(struct iwm_softc *sc, struct iwm_node *in)
{
	return iwm_mvm_binding_cmd(sc, in, IWM_FW_CTXT_ACTION_ADD);
}

/*
 * Construct the generic fields of the PHY context command
 */
void
iwm_mvm_phy_ctxt_cmd_hdr(struct iwm_softc *sc, struct iwm_mvm_phy_ctxt *ctxt,
	struct iwm_phy_context_cmd *cmd, uint32_t action, uint32_t apply_time)
{
	memset(cmd, 0, sizeof(struct iwm_phy_context_cmd));

	cmd->id_and_color = htole32(IWM_FW_CMD_ID_AND_COLOR(ctxt->id,
	    ctxt->color));
	cmd->action = htole32(action);
	cmd->apply_time = htole32(apply_time);
}

/*
 * Add the phy configuration to the PHY context command
 */
void
iwm_mvm_phy_ctxt_cmd_data(struct iwm_softc *sc,
	struct iwm_phy_context_cmd *cmd, struct ieee80211_channel *chan,
	uint8_t chains_static, uint8_t chains_dynamic)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t active_cnt, idle_cnt;

	cmd->ci.band = IEEE80211_IS_CHAN_2GHZ(chan) ?
	    IWM_PHY_BAND_24 : IWM_PHY_BAND_5;

	cmd->ci.channel = ieee80211_chan2ieee(ic, chan);
	cmd->ci.width = IWM_PHY_VHT_CHANNEL_MODE20;
	cmd->ci.ctrl_pos = IWM_PHY_VHT_CTRL_POS_1_BELOW;

	/* Set rx the chains */
	idle_cnt = chains_static;
	active_cnt = chains_dynamic;

	cmd->rxchain_info = htole32(iwm_fw_valid_rx_ant(sc) <<
					IWM_PHY_RX_CHAIN_VALID_POS);
	cmd->rxchain_info |= htole32(idle_cnt << IWM_PHY_RX_CHAIN_CNT_POS);
	cmd->rxchain_info |= htole32(active_cnt <<
	    IWM_PHY_RX_CHAIN_MIMO_CNT_POS);

	cmd->txchain_info = htole32(iwm_fw_valid_tx_ant(sc));
}

/*
 * Send a command
 * only if something in the configuration changed: in case that this is the
 * first time that the phy configuration is applied or in case that the phy
 * configuration changed from the previous apply.
 */
int
iwm_mvm_phy_ctxt_apply(struct iwm_softc *sc,
	struct iwm_mvm_phy_ctxt *ctxt,
	uint8_t chains_static, uint8_t chains_dynamic,
	uint32_t action, uint32_t apply_time)
{
	struct iwm_phy_context_cmd cmd;
	int ret;

	/* Set the command header fields */
	iwm_mvm_phy_ctxt_cmd_hdr(sc, ctxt, &cmd, action, apply_time);

	/* Set the command data */
	iwm_mvm_phy_ctxt_cmd_data(sc, &cmd, ctxt->channel,
	    chains_static, chains_dynamic);

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_PHY_CONTEXT_CMD, IWM_CMD_SYNC,
	    sizeof(struct iwm_phy_context_cmd), &cmd);
	if (ret) {
		DPRINTF(("PHY ctxt cmd error. ret=%d\n", ret));
	}
	return ret;
}

/*
 * Send a command to add a PHY context based on the current HW configuration.
 */
int
iwm_mvm_phy_ctxt_add(struct iwm_softc *sc, struct iwm_mvm_phy_ctxt *ctxt,
	struct ieee80211_channel *chan,
	uint8_t chains_static, uint8_t chains_dynamic)
{
	ctxt->channel = chan;
	return iwm_mvm_phy_ctxt_apply(sc, ctxt,
	    chains_static, chains_dynamic, IWM_FW_CTXT_ACTION_ADD, 0);
}

/*
 * Send a command to modify the PHY context based on the current HW
 * configuration. Note that the function does not check that the configuration
 * changed.
 */
int
iwm_mvm_phy_ctxt_changed(struct iwm_softc *sc,
	struct iwm_mvm_phy_ctxt *ctxt, struct ieee80211_channel *chan,
	uint8_t chains_static, uint8_t chains_dynamic)
{
	ctxt->channel = chan;
	return iwm_mvm_phy_ctxt_apply(sc, ctxt,
	    chains_static, chains_dynamic, IWM_FW_CTXT_ACTION_MODIFY, 0);
}

/*
 * transmit side
 */

/*
 * Send a command to the firmware.  We try to implement the Linux
 * driver interface for the routine.
 * mostly from if_iwn (iwn_cmd()).
 *
 * For now, we always copy the first part and map the second one (if it exists).
 */
int
iwm_send_cmd(struct iwm_softc *sc, struct iwm_host_cmd *hcmd)
{
	struct iwm_tx_ring *ring = &sc->txq[IWM_MVM_CMD_QUEUE];
	struct iwm_tfd *desc;
	struct iwm_tx_data *txdata;
	struct iwm_device_cmd *cmd;
	struct mbuf *m;
	bus_addr_t paddr;
	uint32_t addr_lo;
	int error = 0, i, paylen, off, s;
	int code;
	int async, wantresp;
	int group_id;
	size_t hdrlen, datasz;
	uint8_t *data;

	code = hcmd->id;
	async = hcmd->flags & IWM_CMD_ASYNC;
	wantresp = hcmd->flags & IWM_CMD_WANT_SKB;

	for (i = 0, paylen = 0; i < nitems(hcmd->len); i++) {
		paylen += hcmd->len[i];
	}

	/* if the command wants an answer, busy sc_cmd_resp */
	if (wantresp) {
		KASSERT(!async);
		while (sc->sc_wantresp != IWM_CMD_RESP_IDLE)
			tsleep(&sc->sc_wantresp, 0, "iwmcmdsl", 0);
		sc->sc_wantresp = ring->qid << 16 | ring->cur;
		DPRINTFN(12, ("wantresp is %x\n", sc->sc_wantresp));
	}

	/*
	 * Is the hardware still available?  (after e.g. above wait).
	 */
	s = splnet();
	if (sc->sc_flags & IWM_FLAG_STOPPED) {
		error = ENXIO;
		goto out;
	}

	desc = &ring->desc[ring->cur];
	txdata = &ring->data[ring->cur];

	group_id = iwm_cmd_groupid(code);
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
		if (paylen > IWM_MAX_CMD_PAYLOAD_SIZE) {
			printf("%s: firmware command too long (%zd bytes)\n",
			    DEVNAME(sc), totlen);
			error = EINVAL;
			goto out;
		}
		m = MCLGETI(NULL, M_DONTWAIT, NULL, totlen);
		if (m == NULL) {
			printf("%s: could not get fw cmd mbuf (%zd bytes)\n",
			    DEVNAME(sc), totlen);
			error = ENOMEM;
			goto out;
		}
		cmd = mtod(m, struct iwm_device_cmd *);
		error = bus_dmamap_load(sc->sc_dmat, txdata->map, cmd,
		    totlen, NULL, BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error != 0) {
			printf("%s: could not load fw cmd mbuf (%zd bytes)\n",
			    DEVNAME(sc), totlen);
			m_freem(m);
			goto out;
		}
		txdata->m = m; /* mbuf will be freed in iwm_cmd_done() */
		paddr = txdata->map->dm_segs[0].ds_addr;
	} else {
		cmd = &ring->cmd[ring->cur];
		paddr = txdata->cmd_paddr;
	}

	if (group_id != 0) {
		cmd->hdr_wide.opcode = iwm_cmd_opcode(code);
		cmd->hdr_wide.group_id = group_id;
		cmd->hdr_wide.qid = ring->qid;
		cmd->hdr_wide.idx = ring->cur;
		cmd->hdr_wide.length = htole16(paylen);
		cmd->hdr_wide.version = iwm_cmd_version(code);
		data = cmd->data_wide;
	} else {
		cmd->hdr.code = code;
		cmd->hdr.flags = 0;
		cmd->hdr.qid = ring->qid;
		cmd->hdr.idx = ring->cur;
		data = cmd->data;
	}

	for (i = 0, off = 0; i < nitems(hcmd->data); i++) {
		if (hcmd->len[i] == 0)
			continue;
		memcpy(data + off, hcmd->data[i], hcmd->len[i]);
		off += hcmd->len[i];
	}
	KASSERT(off == paylen);

	/* lo field is not aligned */
	addr_lo = htole32((uint32_t)paddr);
	memcpy(&desc->tbs[0].lo, &addr_lo, sizeof(uint32_t));
	desc->tbs[0].hi_n_len  = htole16(iwm_get_dma_hi_addr(paddr)
	    | ((hdrlen + paylen) << 4));
	desc->num_tbs = 1;

	DPRINTFN(8, ("iwm_send_cmd 0x%x size=%lu %s\n",
	    code, hcmd->len[0] + hcmd->len[1] + hdrlen,
	    async ? " (async)" : ""));

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

	IWM_SETBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	if (!iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN,
	    (IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY |
	     IWM_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP), 15000)) {
		printf("%s: acquiring device failed\n", DEVNAME(sc));
		error = EBUSY;
		goto out;
	}

#if 0
	iwm_update_sched(sc, ring->qid, ring->cur, 0, 0);
#endif
	DPRINTF(("sending command 0x%x qid %d, idx %d\n",
	    code, ring->qid, ring->cur));

	/* Kick command ring. */
	ring->cur = (ring->cur + 1) % IWM_TX_RING_COUNT;
	IWM_WRITE(sc, IWM_HBUS_TARG_WRPTR, ring->qid << 8 | ring->cur);

	if (!async) {
		/* m..m-mmyy-mmyyyy-mym-ym m-my generation */
		int generation = sc->sc_generation;
		error = tsleep(desc, PCATCH, "iwmcmd", hz);
		if (error == 0) {
			/* if hardware is no longer up, return error */
			if (generation != sc->sc_generation) {
				error = ENXIO;
			} else {
				hcmd->resp_pkt = (void *)sc->sc_cmd_resp;
			}
		}
	}
 out:
	if (wantresp && error != 0) {
		iwm_free_resp(sc, hcmd);
	}
	splx(s);

	return error;
}

int
iwm_mvm_send_cmd_pdu(struct iwm_softc *sc, uint8_t id,
	uint32_t flags, uint16_t len, const void *data)
{
	struct iwm_host_cmd cmd = {
		.id = id,
		.len = { len, },
		.data = { data, },
		.flags = flags,
	};

	return iwm_send_cmd(sc, &cmd);
}

int
iwm_mvm_send_cmd_status(struct iwm_softc *sc,
	struct iwm_host_cmd *cmd, uint32_t *status)
{
	struct iwm_rx_packet *pkt;
	struct iwm_cmd_response *resp;
	int error, resp_len;

	KASSERT((cmd->flags & IWM_CMD_WANT_SKB) == 0);
	cmd->flags |= IWM_CMD_SYNC | IWM_CMD_WANT_SKB;

	if ((error = iwm_send_cmd(sc, cmd)) != 0)
		return error;
	pkt = cmd->resp_pkt;

	/* Can happen if RFKILL is asserted */
	if (!pkt) {
		error = 0;
		goto out_free_resp;
	}

	if (pkt->hdr.flags & IWM_CMD_FAILED_MSK) {
		error = EIO;
		goto out_free_resp;
	}

	resp_len = iwm_rx_packet_payload_len(pkt);
	if (resp_len != sizeof(*resp)) {
		error = EIO;
		goto out_free_resp;
	}

	resp = (void *)pkt->data;
	*status = le32toh(resp->status);
 out_free_resp:
	iwm_free_resp(sc, cmd);
	return error;
}

int
iwm_mvm_send_cmd_pdu_status(struct iwm_softc *sc, uint8_t id,
	uint16_t len, const void *data, uint32_t *status)
{
	struct iwm_host_cmd cmd = {
		.id = id,
		.len = { len, },
		.data = { data, },
	};

	return iwm_mvm_send_cmd_status(sc, &cmd, status);
}

void
iwm_free_resp(struct iwm_softc *sc, struct iwm_host_cmd *hcmd)
{
	KASSERT(sc->sc_wantresp != IWM_CMD_RESP_IDLE);
	KASSERT((hcmd->flags & (IWM_CMD_WANT_SKB|IWM_CMD_SYNC))
	    == (IWM_CMD_WANT_SKB|IWM_CMD_SYNC));
	sc->sc_wantresp = IWM_CMD_RESP_IDLE;
	wakeup(&sc->sc_wantresp);
}

/*
 * Process a "command done" firmware notification.  This is where we wakeup
 * processes waiting for a synchronous command completion.
 * from if_iwn
 */
void
iwm_cmd_done(struct iwm_softc *sc, struct iwm_rx_packet *pkt)
{
	struct iwm_tx_ring *ring = &sc->txq[IWM_MVM_CMD_QUEUE];
	struct iwm_tx_data *data;

	if (pkt->hdr.qid != IWM_MVM_CMD_QUEUE) {
		return;	/* Not a command ack. */
	}

	data = &ring->data[pkt->hdr.idx];

	/* If the command was mapped in an mbuf, free it. */
	if (data->m != NULL) {
		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}
	wakeup(&ring->desc[pkt->hdr.idx]);
}

#if 0
/*
 * necessary only for block ack mode
 */
void
iwm_update_sched(struct iwm_softc *sc, int qid, int idx, uint8_t sta_id,
	uint16_t len)
{
	struct iwm_agn_scd_bc_tbl *scd_bc_tbl;
	uint16_t w_val;

	scd_bc_tbl = sc->sched_dma.vaddr;

	len += 8; /* magic numbers came naturally from paris */
	if (sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_DW_BC_TABLE)
		len = roundup(len, 4) / 4;

	w_val = htole16(sta_id << 12 | len);

	/* Update TX scheduler. */
	scd_bc_tbl[qid].tfd_offset[idx] = w_val;
	bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
	    (char *)(void *)w - (char *)(void *)sc->sched_dma.vaddr,
	    sizeof(uint16_t), BUS_DMASYNC_PREWRITE);

	/* I really wonder what this is ?!? */
	if (idx < IWM_TFD_QUEUE_SIZE_BC_DUP) {
		scd_bc_tbl[qid].tfd_offset[IWM_TFD_QUEUE_SIZE_MAX + idx] = w_val;
		bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
		    (char *)(void *)(w + IWM_TFD_QUEUE_SIZE_MAX) -
		    (char *)(void *)sc->sched_dma.vaddr,
		    sizeof (uint16_t), BUS_DMASYNC_PREWRITE);
	}
}
#endif

/*
 * Fill in various bit for management frames, and leave them
 * unfilled for data frames (firmware takes care of that).
 * Return the selected TX rate.
 */
const struct iwm_rate *
iwm_tx_fill_cmd(struct iwm_softc *sc, struct iwm_node *in,
	struct ieee80211_frame *wh, struct iwm_tx_cmd *tx)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	const struct iwm_rate *rinfo;
	int type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	int ridx, rate_flags, i;
	int nrates = ni->ni_rates.rs_nrates;

	tx->rts_retry_limit = IWM_RTS_DFAULT_RETRY_LIMIT;
	tx->data_retry_limit = IWM_DEFAULT_TX_RETRY;

	if (type != IEEE80211_FC0_TYPE_DATA) {
		/* for non-data, use the lowest supported rate */
		ridx = (IEEE80211_IS_CHAN_5GHZ(ni->ni_chan)) ?
		    IWM_RIDX_OFDM : IWM_RIDX_CCK;
	} else if (ic->ic_fixed_mcs != -1) {
		ridx = sc->sc_fixed_ridx;
	} else if (ic->ic_fixed_rate != -1) {
		ridx = sc->sc_fixed_ridx;
	} else {
		/* for data frames, use RS table */
		tx->initial_rate_index = 0;
		tx->tx_flags |= htole32(IWM_TX_CMD_FLG_STA_RATE);
		DPRINTFN(12, ("start with txrate %d\n", tx->initial_rate_index));
		if (ni->ni_flags & IEEE80211_NODE_HT) {
			ridx = iwm_mcs2ridx[ni->ni_txmcs];
			return &iwm_rates[ridx];
		}
		ridx = 0;
		for (i = 0; i < nrates; i++) {
			if (iwm_rates[i].rate == (ni->ni_txrate &
			    IEEE80211_RATE_VAL)) {
				ridx = i;
				break;
			}
		}
		return &iwm_rates[ridx];
	}

	rinfo = &iwm_rates[ridx];
	rate_flags = 1 << IWM_RATE_MCS_ANT_POS;
	if (IWM_RIDX_IS_CCK(ridx))
		rate_flags |= IWM_RATE_MCS_CCK_MSK;
	if ((ni->ni_flags & IEEE80211_NODE_HT) &&
	    rinfo->ht_plcp != IWM_RATE_HT_SISO_MCS_INV_PLCP) {
		rate_flags |= IWM_RATE_MCS_HT_MSK; 
		tx->rate_n_flags = htole32(rate_flags | rinfo->ht_plcp);
	} else
		tx->rate_n_flags = htole32(rate_flags | rinfo->plcp);

	return rinfo;
}

#define TB0_SIZE 16
int
iwm_tx(struct iwm_softc *sc, struct mbuf *m, struct ieee80211_node *ni, int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_node *in = (void *)ni;
	struct iwm_tx_ring *ring;
	struct iwm_tx_data *data;
	struct iwm_tfd *desc;
	struct iwm_device_cmd *cmd;
	struct iwm_tx_cmd *tx;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	const struct iwm_rate *rinfo;
	uint32_t flags;
	u_int hdrlen;
	bus_dma_segment_t *seg;
	uint8_t tid, type;
	int i, totlen, error, pad;
	int hdrlen2;

	wh = mtod(m, struct ieee80211_frame *);
	hdrlen = ieee80211_get_hdrlen(wh);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	hdrlen2 = (ieee80211_has_qos(wh)) ?
	    sizeof (struct ieee80211_qosframe) :
	    sizeof (struct ieee80211_frame);

	if (hdrlen != hdrlen2)
		DPRINTF(("%s: hdrlen error (%d != %d)\n",
		    DEVNAME(sc), hdrlen, hdrlen2));

	tid = 0;

	ring = &sc->txq[ac];
	desc = &ring->desc[ring->cur];
	memset(desc, 0, sizeof(*desc));
	data = &ring->data[ring->cur];

	/* Fill out iwm_tx_cmd to send to the firmware */
	cmd = &ring->cmd[ring->cur];
	cmd->hdr.code = IWM_TX_CMD;
	cmd->hdr.flags = 0;
	cmd->hdr.qid = ring->qid;
	cmd->hdr.idx = ring->cur;

	tx = (void *)cmd->data;
	memset(tx, 0, sizeof(*tx));

	rinfo = iwm_tx_fill_cmd(sc, in, wh, tx);

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct iwm_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ni->ni_chan->ic_flags);
		if (rinfo->plcp == IWM_RATE_INVM_PLCP) {
			/* XXX need a way to pass current MCS in 11n mode */
			tap->wt_rate = 0;
		} else
			tap->wt_rate = rinfo->rate;
		tap->wt_hwqueue = ac;
		if ((ic->ic_flags & IEEE80211_F_WEPON) &&
		    (wh->i_fc[1] & IEEE80211_FC1_PROTECTED))
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	/* Encrypt the frame if need be. */
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		/* Retrieve key for TX && do software encryption. */
                k = ieee80211_get_txkey(ic, wh, ni);
		if ((m = ieee80211_encrypt(ic, m, k)) == NULL)
			return ENOBUFS;
		/* 802.11 header may have moved. */
		wh = mtod(m, struct ieee80211_frame *);
	}
	totlen = m->m_pkthdr.len;

	flags = 0;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= IWM_TX_CMD_FLG_ACK;
	}

	if (type != IEEE80211_FC0_TYPE_DATA
	    && (totlen + IEEE80211_CRC_LEN > ic->ic_rtsthreshold)
	    && !IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= IWM_TX_CMD_FLG_PROT_REQUIRE;
	}

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA)
		tx->sta_id = sc->sc_aux_sta.sta_id;
	else
		tx->sta_id = IWM_STATION_ID;

	if (type == IEEE80211_FC0_TYPE_MGT) {
		uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			tx->pm_frame_timeout = htole16(3);
		else
			tx->pm_frame_timeout = htole16(2);
	} else {
		tx->pm_frame_timeout = htole16(0);
	}

	if (hdrlen & 3) {
		/* First segment length must be a multiple of 4. */
		flags |= IWM_TX_CMD_FLG_MH_PAD;
		pad = 4 - (hdrlen & 3);
	} else
		pad = 0;

	tx->driver_txop = 0;
	tx->next_frame_len = 0;

	tx->len = htole16(totlen);
	tx->tid_tspec = tid;
	tx->life_time = htole32(IWM_TX_CMD_LIFE_TIME_INFINITE);

	/* Set physical address of "scratch area". */
	tx->dram_lsb_ptr = htole32(data->scratch_paddr);
	tx->dram_msb_ptr = iwm_get_dma_hi_addr(data->scratch_paddr);

	/* Copy 802.11 header in TX command. */
	memcpy(((uint8_t *)tx) + sizeof(*tx), wh, hdrlen);

	flags |= IWM_TX_CMD_FLG_BT_DIS | IWM_TX_CMD_FLG_SEQ_CTL;

	tx->sec_ctl = 0;
	tx->tx_flags |= htole32(flags);

	/* Trim 802.11 header. */
	m_adj(m, hdrlen);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (error && error != EFBIG) {
		printf("%s: can't map mbuf (error %d)\n", DEVNAME(sc), error);
		m_freem(m);
		return error;
	}
	if (error) {
		/* Too many DMA segments, linearize mbuf. */
		if (m_defrag(m, M_DONTWAIT)) {
			m_freem(m);
			return ENOBUFS;
		}
		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
		    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error) {
			printf("%s: can't map mbuf (error %d)\n", DEVNAME(sc),
			    error);
			m_freem(m);
			return error;
		}
	}
	data->m = m;
	data->in = in;
	data->done = 0;

	DPRINTFN(8, ("sending txd %p, in %p\n", data, data->in));
	KASSERT(data->in != NULL);

	DPRINTFN(8, ("sending data: qid=%d idx=%d len=%d nsegs=%d\n",
	    ring->qid, ring->cur, totlen, data->map->dm_nsegs));

	/* Fill TX descriptor. */
	desc->num_tbs = 2 + data->map->dm_nsegs;

	desc->tbs[0].lo = htole32(data->cmd_paddr);
	desc->tbs[0].hi_n_len = htole16(iwm_get_dma_hi_addr(data->cmd_paddr)) |
	    (TB0_SIZE << 4);
	desc->tbs[1].lo = htole32(data->cmd_paddr + TB0_SIZE);
	desc->tbs[1].hi_n_len = htole16(iwm_get_dma_hi_addr(data->cmd_paddr)) |
	    ((sizeof(struct iwm_cmd_header) + sizeof(*tx)
	      + hdrlen + pad - TB0_SIZE) << 4);

	/* Other DMA segments are for data payload. */
	seg = data->map->dm_segs;
	for (i = 0; i < data->map->dm_nsegs; i++, seg++) {
		desc->tbs[i+2].lo = htole32(seg->ds_addr);
		desc->tbs[i+2].hi_n_len = \
		    htole16(iwm_get_dma_hi_addr(seg->ds_addr))
		    | ((seg->ds_len) << 4);
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
	iwm_update_sched(sc, ring->qid, ring->cur, tx->sta_id, le16toh(tx->len));
#endif

	/* Kick TX ring. */
	ring->cur = (ring->cur + 1) % IWM_TX_RING_COUNT;
	IWM_WRITE(sc, IWM_HBUS_TARG_WRPTR, ring->qid << 8 | ring->cur);

	/* Mark TX ring as full if we reach a certain threshold. */
	if (++ring->queued > IWM_TX_RING_HIMARK) {
		sc->qfullmsk |= 1 << ring->qid;
	}

	return 0;
}

#if 0
/* not necessary? */
int
iwm_mvm_flush_tx_path(struct iwm_softc *sc, int tfd_msk, int sync)
{
	struct iwm_tx_path_flush_cmd flush_cmd = {
		.queues_ctl = htole32(tfd_msk),
		.flush_ctl = htole16(IWM_DUMP_TX_FIFO_FLUSH),
	};
	int ret;

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_TXPATH_FLUSH,
	    sync ? IWM_CMD_SYNC : IWM_CMD_ASYNC,
	    sizeof(flush_cmd), &flush_cmd);
	if (ret)
                printf("%s: Flushing tx queue failed: %d\n", DEVNAME(sc), ret);
	return ret;
}
#endif

/* Set led register on */
void
iwm_mvm_led_enable(struct iwm_softc *sc)
{
	IWM_WRITE(sc, IWM_CSR_LED_REG, IWM_CSR_LED_REG_TURN_ON);
}

/* Set led register off */
void
iwm_mvm_led_disable(struct iwm_softc *sc)
{
	IWM_WRITE(sc, IWM_CSR_LED_REG, IWM_CSR_LED_REG_TURN_OFF);
}

int
iwm_mvm_led_is_enabled(struct iwm_softc *sc)
{
	return (IWM_READ(sc, IWM_CSR_LED_REG) == IWM_CSR_LED_REG_TURN_ON);
}

void
iwm_led_blink_timeout(void *arg)
{
	struct iwm_softc *sc = arg;

	if (iwm_mvm_led_is_enabled(sc))
		iwm_mvm_led_disable(sc);
	else
		iwm_mvm_led_enable(sc);

	timeout_add_msec(&sc->sc_led_blink_to, 200);
}

void
iwm_led_blink_start(struct iwm_softc *sc)
{
	timeout_add(&sc->sc_led_blink_to, 0);
}

void
iwm_led_blink_stop(struct iwm_softc *sc)
{
	timeout_del(&sc->sc_led_blink_to);
	iwm_mvm_led_disable(sc);
}

#define IWM_POWER_KEEP_ALIVE_PERIOD_SEC    25

int
iwm_mvm_beacon_filter_send_cmd(struct iwm_softc *sc,
	struct iwm_beacon_filter_cmd *cmd)
{
	int ret;

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_REPLY_BEACON_FILTERING_CMD,
	    IWM_CMD_SYNC, sizeof(struct iwm_beacon_filter_cmd), cmd);

	if (!ret) {
		DPRINTF(("ba_enable_beacon_abort is: %d\n",
		    le32toh(cmd->ba_enable_beacon_abort)));
		DPRINTF(("ba_escape_timer is: %d\n",
		    le32toh(cmd->ba_escape_timer)));
		DPRINTF(("bf_debug_flag is: %d\n",
		    le32toh(cmd->bf_debug_flag)));
		DPRINTF(("bf_enable_beacon_filter is: %d\n",
		    le32toh(cmd->bf_enable_beacon_filter)));
		DPRINTF(("bf_energy_delta is: %d\n",
		    le32toh(cmd->bf_energy_delta)));
		DPRINTF(("bf_escape_timer is: %d\n",
		    le32toh(cmd->bf_escape_timer)));
		DPRINTF(("bf_roaming_energy_delta is: %d\n",
		    le32toh(cmd->bf_roaming_energy_delta)));
		DPRINTF(("bf_roaming_state is: %d\n",
		    le32toh(cmd->bf_roaming_state)));
		DPRINTF(("bf_temp_threshold is: %d\n",
		    le32toh(cmd->bf_temp_threshold)));
		DPRINTF(("bf_temp_fast_filter is: %d\n",
		    le32toh(cmd->bf_temp_fast_filter)));
		DPRINTF(("bf_temp_slow_filter is: %d\n",
		    le32toh(cmd->bf_temp_slow_filter)));
	}
	return ret;
}

void
iwm_mvm_beacon_filter_set_cqm_params(struct iwm_softc *sc,
	struct iwm_node *in, struct iwm_beacon_filter_cmd *cmd)
{
	cmd->ba_enable_beacon_abort = htole32(sc->sc_bf.ba_enabled);
}

int
iwm_mvm_update_beacon_abort(struct iwm_softc *sc, struct iwm_node *in,
	int enable)
{
	struct iwm_beacon_filter_cmd cmd = {
		IWM_BF_CMD_CONFIG_DEFAULTS,
		.bf_enable_beacon_filter = htole32(1),
		.ba_enable_beacon_abort = htole32(enable),
	};

	if (!sc->sc_bf.bf_enabled)
		return 0;

	sc->sc_bf.ba_enabled = enable;
	iwm_mvm_beacon_filter_set_cqm_params(sc, in, &cmd);
	return iwm_mvm_beacon_filter_send_cmd(sc, &cmd);
}

void
iwm_mvm_power_log(struct iwm_softc *sc, struct iwm_mac_power_cmd *cmd)
{
	DPRINTF(("Sending power table command on mac id 0x%X for "
	    "power level %d, flags = 0x%X\n",
	    cmd->id_and_color, IWM_POWER_SCHEME_CAM, le16toh(cmd->flags)));
	DPRINTF(("Keep alive = %u sec\n", le16toh(cmd->keep_alive_seconds)));

	if (!(cmd->flags & htole16(IWM_POWER_FLAGS_POWER_MANAGEMENT_ENA_MSK))) {
		DPRINTF(("Disable power management\n"));
		return;
	}
}

void
iwm_mvm_power_build_cmd(struct iwm_softc *sc, struct iwm_node *in,
	struct iwm_mac_power_cmd *cmd)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	int dtimper, dtimper_msec;
	int keep_alive;

	cmd->id_and_color = htole32(IWM_FW_CMD_ID_AND_COLOR(in->in_id,
	    in->in_color));
	dtimper = ic->ic_dtim_period ?: 1;

	/*
	 * Regardless of power management state the driver must set
	 * keep alive period. FW will use it for sending keep alive NDPs
	 * immediately after association. Check that keep alive period
	 * is at least 3 * DTIM
	 */
	dtimper_msec = dtimper * ni->ni_intval;
	keep_alive
	    = MAX(3 * dtimper_msec, 1000 * IWM_POWER_KEEP_ALIVE_PERIOD_SEC);
	keep_alive = roundup(keep_alive, 1000) / 1000;
	cmd->keep_alive_seconds = htole16(keep_alive);
}

int
iwm_mvm_power_mac_update_mode(struct iwm_softc *sc, struct iwm_node *in)
{
	int ret;
	int ba_enable;
	struct iwm_mac_power_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	iwm_mvm_power_build_cmd(sc, in, &cmd);
	iwm_mvm_power_log(sc, &cmd);

	if ((ret = iwm_mvm_send_cmd_pdu(sc, IWM_MAC_PM_POWER_TABLE,
	    IWM_CMD_SYNC, sizeof(cmd), &cmd)) != 0)
		return ret;

	ba_enable = !!(cmd.flags &
	    htole16(IWM_POWER_FLAGS_POWER_MANAGEMENT_ENA_MSK));
	return iwm_mvm_update_beacon_abort(sc, in, ba_enable);
}

int
iwm_mvm_power_update_device(struct iwm_softc *sc)
{
	struct iwm_device_power_cmd cmd = {
		.flags = htole16(IWM_DEVICE_POWER_FLAGS_POWER_SAVE_ENA_MSK),
	};

	if (!(sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_DEVICE_PS_CMD))
		return 0;

	cmd.flags |= htole16(IWM_DEVICE_POWER_FLAGS_CAM_MSK);
	DPRINTF(("Sending device power command with flags = 0x%X\n", cmd.flags));

	return iwm_mvm_send_cmd_pdu(sc,
	    IWM_POWER_TABLE_CMD, IWM_CMD_SYNC, sizeof(cmd), &cmd);
}

int
iwm_mvm_enable_beacon_filter(struct iwm_softc *sc, struct iwm_node *in)
{
	struct iwm_beacon_filter_cmd cmd = {
		IWM_BF_CMD_CONFIG_DEFAULTS,
		.bf_enable_beacon_filter = htole32(1),
	};
	int ret;

	iwm_mvm_beacon_filter_set_cqm_params(sc, in, &cmd);
	ret = iwm_mvm_beacon_filter_send_cmd(sc, &cmd);

	if (ret == 0)
		sc->sc_bf.bf_enabled = 1;

	return ret;
}

int
iwm_mvm_disable_beacon_filter(struct iwm_softc *sc)
{
	struct iwm_beacon_filter_cmd cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	if ((sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_BF_UPDATED) == 0)
		return 0;

	ret = iwm_mvm_beacon_filter_send_cmd(sc, &cmd);
	if (ret == 0)
		sc->sc_bf.bf_enabled = 0;

	return ret;
}

int
iwm_mvm_send_add_sta_cmd_status(struct iwm_softc *sc,
	struct iwm_mvm_add_sta_cmd_v7 *cmd, int *status)
{
	return iwm_mvm_send_cmd_pdu_status(sc, IWM_ADD_STA, sizeof(*cmd),
	    cmd, status);
}

/* send station add/update command to firmware */
int
iwm_mvm_sta_send_to_fw(struct iwm_softc *sc, struct iwm_node *in, int update)
{
	struct iwm_mvm_add_sta_cmd_v7 add_sta_cmd;
	int ret;
	uint32_t status;
	struct ieee80211com *ic = &sc->sc_ic;

	memset(&add_sta_cmd, 0, sizeof(add_sta_cmd));

	add_sta_cmd.sta_id = IWM_STATION_ID;
	add_sta_cmd.mac_id_n_color
	    = htole32(IWM_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
	if (!update) {
		int ac;
		for (ac = 0; ac < EDCA_NUM_AC; ac++) {
			add_sta_cmd.tfd_queue_msk |=
			    htole32(1 << iwm_mvm_ac_to_tx_fifo[ac]);
		}
		IEEE80211_ADDR_COPY(&add_sta_cmd.addr, in->in_ni.ni_bssid);
	}
	add_sta_cmd.add_modify = update ? 1 : 0;
	add_sta_cmd.station_flags_msk
	    |= htole32(IWM_STA_FLG_FAT_EN_MSK | IWM_STA_FLG_MIMO_EN_MSK);
	add_sta_cmd.tid_disable_tx = htole16(0xffff);
	if (update)
		add_sta_cmd.modify_mask |= (IWM_STA_MODIFY_TID_DISABLE_TX);

	if (in->in_ni.ni_flags & IEEE80211_NODE_HT) {
		add_sta_cmd.station_flags_msk
		    |= htole32(IWM_STA_FLG_MAX_AGG_SIZE_MSK |
		    IWM_STA_FLG_AGG_MPDU_DENS_MSK);

		add_sta_cmd.station_flags
		    |= htole32(IWM_STA_FLG_MAX_AGG_SIZE_64K);
		switch (ic->ic_ampdu_params & IEEE80211_AMPDU_PARAM_SS) {
		case IEEE80211_AMPDU_PARAM_SS_2:
			add_sta_cmd.station_flags
			    |= htole32(IWM_STA_FLG_AGG_MPDU_DENS_2US);
			break;
		case IEEE80211_AMPDU_PARAM_SS_4:
			add_sta_cmd.station_flags
			    |= htole32(IWM_STA_FLG_AGG_MPDU_DENS_4US);
			break;
		case IEEE80211_AMPDU_PARAM_SS_8:
			add_sta_cmd.station_flags
			    |= htole32(IWM_STA_FLG_AGG_MPDU_DENS_8US);
			break;
		case IEEE80211_AMPDU_PARAM_SS_16:
			add_sta_cmd.station_flags
			    |= htole32(IWM_STA_FLG_AGG_MPDU_DENS_16US);
			break;
		default:
			break;
		}
	}

	status = IWM_ADD_STA_SUCCESS;
	ret = iwm_mvm_send_add_sta_cmd_status(sc, &add_sta_cmd, &status);
	if (ret)
		return ret;

	switch (status) {
	case IWM_ADD_STA_SUCCESS:
		break;
	default:
		ret = EIO;
		DPRINTF(("IWM_ADD_STA failed\n"));
		break;
	}

	return ret;
}

int
iwm_mvm_add_sta(struct iwm_softc *sc, struct iwm_node *in)
{
	return iwm_mvm_sta_send_to_fw(sc, in, 0);
}

int
iwm_mvm_update_sta(struct iwm_softc *sc, struct iwm_node *in)
{
	return iwm_mvm_sta_send_to_fw(sc, in, 1);
}

int
iwm_mvm_add_int_sta_common(struct iwm_softc *sc, struct iwm_int_sta *sta,
	const uint8_t *addr, uint16_t mac_id, uint16_t color)
{
	struct iwm_mvm_add_sta_cmd_v7 cmd;
	int ret;
	uint32_t status;

	memset(&cmd, 0, sizeof(cmd));
	cmd.sta_id = sta->sta_id;
	cmd.mac_id_n_color = htole32(IWM_FW_CMD_ID_AND_COLOR(mac_id, color));

	cmd.tfd_queue_msk = htole32(sta->tfd_queue_msk);
	cmd.tid_disable_tx = htole16(0xffff);

	if (addr)
		memcpy(cmd.addr, addr, ETHER_ADDR_LEN);

	ret = iwm_mvm_send_add_sta_cmd_status(sc, &cmd, &status);
	if (ret)
		return ret;

	switch (status) {
	case IWM_ADD_STA_SUCCESS:
		DPRINTF(("Internal station added.\n"));
		return 0;
	default:
		DPRINTF(("%s: Add internal station failed, status=0x%x\n",
		    DEVNAME(sc), status));
		ret = EIO;
		break;
	}
	return ret;
}

int
iwm_mvm_add_aux_sta(struct iwm_softc *sc)
{
	int ret;

	sc->sc_aux_sta.sta_id = IWM_AUX_STA_ID;
	sc->sc_aux_sta.tfd_queue_msk = (1 << IWM_MVM_AUX_QUEUE);

	ret = iwm_enable_txq(sc, 0, IWM_MVM_AUX_QUEUE, IWM_MVM_TX_FIFO_MCAST);
	if (ret)
		return ret;

	ret = iwm_mvm_add_int_sta_common(sc,
	    &sc->sc_aux_sta, NULL, IWM_MAC_INDEX_AUX, 0);

	if (ret)
		memset(&sc->sc_aux_sta, 0, sizeof(sc->sc_aux_sta));
	return ret;
}

#define IWM_PLCP_QUIET_THRESH 1
#define IWM_ACTIVE_QUIET_TIME 10
#define LONG_OUT_TIME_PERIOD 600
#define SHORT_OUT_TIME_PERIOD 200
#define SUSPEND_TIME_PERIOD 100

uint16_t
iwm_mvm_scan_rx_chain(struct iwm_softc *sc)
{
	uint16_t rx_chain;
	uint8_t rx_ant;

	rx_ant = iwm_fw_valid_rx_ant(sc);
	rx_chain = rx_ant << IWM_PHY_RX_CHAIN_VALID_POS;
	rx_chain |= rx_ant << IWM_PHY_RX_CHAIN_FORCE_MIMO_SEL_POS;
	rx_chain |= rx_ant << IWM_PHY_RX_CHAIN_FORCE_SEL_POS;
	rx_chain |= 0x1 << IWM_PHY_RX_CHAIN_DRIVER_FORCE_POS;
	return htole16(rx_chain);
}

#define ieee80211_tu_to_usec(a) (1024*(a))

uint32_t
iwm_mvm_scan_max_out_time(struct iwm_softc *sc, uint32_t flags, int is_assoc)
{
	if (!is_assoc)
		return 0;
	if (flags & 0x1)
		return htole32(ieee80211_tu_to_usec(SHORT_OUT_TIME_PERIOD));
	return htole32(ieee80211_tu_to_usec(LONG_OUT_TIME_PERIOD));
}

uint32_t
iwm_mvm_scan_suspend_time(struct iwm_softc *sc, int is_assoc)
{
	if (!is_assoc)
		return 0;
	return htole32(ieee80211_tu_to_usec(SUSPEND_TIME_PERIOD));
}

uint32_t
iwm_mvm_scan_rate_n_flags(struct iwm_softc *sc, int flags, int no_cck)
{
	uint32_t tx_ant;
	int i, ind;

	for (i = 0, ind = sc->sc_scan_last_antenna;
	    i < IWM_RATE_MCS_ANT_NUM; i++) {
		ind = (ind + 1) % IWM_RATE_MCS_ANT_NUM;
		if (iwm_fw_valid_tx_ant(sc) & (1 << ind)) {
			sc->sc_scan_last_antenna = ind;
			break;
		}
	}
	tx_ant = (1 << sc->sc_scan_last_antenna) << IWM_RATE_MCS_ANT_POS;

	if ((flags & IEEE80211_CHAN_2GHZ) && !no_cck)
		return htole32(IWM_RATE_1M_PLCP | IWM_RATE_MCS_CCK_MSK |
				   tx_ant);
	else
		return htole32(IWM_RATE_6M_PLCP | tx_ant);
}

/*
 * If req->n_ssids > 0, it means we should do an active scan.
 * In case of active scan w/o directed scan, we receive a zero-length SSID
 * just to notify that this scan is active and not passive.
 * In order to notify the FW of the number of SSIDs we wish to scan (including
 * the zero-length one), we need to set the corresponding bits in chan->type,
 * one for each SSID, and set the active bit (first). If the first SSID is
 * already included in the probe template, so we need to set only
 * req->n_ssids - 1 bits in addition to the first bit.
 */
uint16_t
iwm_mvm_get_active_dwell(struct iwm_softc *sc, int flags, int n_ssids)
{
	if (flags & IEEE80211_CHAN_2GHZ)
		return 30  + 3 * (n_ssids + 1);
	return 20  + 2 * (n_ssids + 1);
}

uint16_t
iwm_mvm_get_passive_dwell(struct iwm_softc *sc, int flags)
{
	return (flags & IEEE80211_CHAN_2GHZ) ? 100 + 20 : 100 + 10;
}

uint8_t
iwm_mvm_lmac_scan_fill_channels(struct iwm_softc *sc,
	struct iwm_scan_channel_cfg_lmac *chan, int n_ssids)
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

		chan->channel_num = htole16(ieee80211_mhz2ieee(c->ic_freq, 0));
		chan->iter_count = htole16(1);
		chan->iter_interval = 0;
		chan->flags = htole32(IWM_UNIFIED_SCAN_CHANNEL_PARTIAL);
#if 0 /* makes scanning while associated less useful */
		if (n_ssids != 0)
			chan->flags |= htole32(1 << 1); /* select SSID 0 */
#endif
		chan++;
		nchan++;
	}

	return nchan;
}

uint8_t
iwm_mvm_umac_scan_fill_channels(struct iwm_softc *sc,
	struct iwm_scan_channel_cfg_umac *chan, int n_ssids)
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
#if 0 /* makes scanning while associated less useful */
		if (n_ssids != 0)
			chan->flags = htole32(1 << 0); /* select SSID 0 */
#endif
		chan++;
		nchan++;
	}

	return nchan;
}

int
iwm_mvm_fill_probe_req(struct iwm_softc *sc, struct iwm_scan_probe_req *preq)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	struct ieee80211_frame *wh = (struct ieee80211_frame *)preq->buf;
	struct ieee80211_rateset *rs;
	size_t remain = sizeof(preq->buf);
	uint8_t *frm, *pos;

	memset(preq, 0, sizeof(*preq));

	/* Ensure enough space for header and SSID IE. */
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
	    IWM_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT)) {
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
iwm_mvm_lmac_scan(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_host_cmd hcmd = {
		.id = IWM_SCAN_OFFLOAD_REQUEST_CMD,
		.len = { 0, },
		.data = { NULL, },
		.flags = IWM_CMD_SYNC,
	};
	struct iwm_scan_req_lmac *req;
	size_t req_len;
	int ret;

	req_len = sizeof(struct iwm_scan_req_lmac) +
	    (sizeof(struct iwm_scan_channel_cfg_lmac) *
	    sc->sc_capa_n_scan_channels) + sizeof(struct iwm_scan_probe_req);
	if (req_len > IWM_MAX_CMD_PAYLOAD_SIZE)
		return ENOMEM;
	req = malloc(req_len, M_DEVBUF, M_WAIT | M_CANFAIL | M_ZERO);
	if (req == NULL)
		return ENOMEM;

	hcmd.len[0] = (uint16_t)req_len;
	hcmd.data[0] = (void *)req;

	DPRINTF(("Handling ieee80211 scan request\n"));

	/* These timings correspond to iwlwifi's UNASSOC scan. */
	req->active_dwell = 10;
	req->passive_dwell = 110;
	req->fragmented_dwell = 44;
	req->extended_dwell = 90;
	req->max_out_time = 0;
	req->suspend_time = 0;

	req->scan_prio = htole32(IWM_SCAN_PRIORITY_HIGH);
	req->rx_chain_select = iwm_mvm_scan_rx_chain(sc);
	req->iter_num = htole32(1);
	req->delay = 0;

	req->scan_flags = htole32(IWM_MVM_LMAC_SCAN_FLAG_PASS_ALL |
	    IWM_MVM_LMAC_SCAN_FLAG_ITER_COMPLETE |
	    IWM_MVM_LMAC_SCAN_FLAG_EXTENDED_DWELL);
	if (ic->ic_des_esslen == 0)
		req->scan_flags |= htole32(IWM_MVM_LMAC_SCAN_FLAG_PASSIVE);
	else
		req->scan_flags |=
		    htole32(IWM_MVM_LMAC_SCAN_FLAG_PRE_CONNECTION);
	if (isset(sc->sc_enabled_capa, 
	    IWM_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT))
		req->scan_flags |= htole32(IWM_MVM_LMAC_SCAN_FLAGS_RRM_ENABLED);

	req->flags = htole32(IWM_PHY_BAND_24);
	if (sc->sc_nvm.sku_cap_band_52GHz_enable)
		req->flags |= htole32(IWM_PHY_BAND_5);
	req->filter_flags =
	    htole32(IWM_MAC_FILTER_ACCEPT_GRP | IWM_MAC_FILTER_IN_BEACON);

	/* Tx flags 2 GHz. */
	req->tx_cmd[0].tx_flags = htole32(IWM_TX_CMD_FLG_SEQ_CTL |
	    IWM_TX_CMD_FLG_BT_DIS);
	req->tx_cmd[0].rate_n_flags =
	    iwm_mvm_scan_rate_n_flags(sc, IEEE80211_CHAN_2GHZ, 1/*XXX*/);
	req->tx_cmd[0].sta_id = sc->sc_aux_sta.sta_id;

	/* Tx flags 5 GHz. */
	req->tx_cmd[1].tx_flags = htole32(IWM_TX_CMD_FLG_SEQ_CTL |
	    IWM_TX_CMD_FLG_BT_DIS);
	req->tx_cmd[1].rate_n_flags =
	    iwm_mvm_scan_rate_n_flags(sc, IEEE80211_CHAN_5GHZ, 1/*XXX*/);
	req->tx_cmd[1].sta_id = sc->sc_aux_sta.sta_id;

	/* Check if we're doing an active directed scan. */
	if (ic->ic_des_esslen != 0) {
		req->direct_scan[0].id = IEEE80211_ELEMID_SSID;
		req->direct_scan[0].len = ic->ic_des_esslen;
		memcpy(req->direct_scan[0].ssid, ic->ic_des_essid,
		    ic->ic_des_esslen);
	}

	req->n_channels = iwm_mvm_lmac_scan_fill_channels(sc,
	    (struct iwm_scan_channel_cfg_lmac *)req->data,
	    ic->ic_des_esslen != 0);

	ret = iwm_mvm_fill_probe_req(sc,
			    (struct iwm_scan_probe_req *)(req->data +
			    (sizeof(struct iwm_scan_channel_cfg_lmac) *
			    sc->sc_capa_n_scan_channels)));
	if (ret) {
		free(req, M_DEVBUF, req_len);
		return ret;
	}

	/* Specify the scan plan: We'll do one iteration. */
	req->schedule[0].iterations = 1;
	req->schedule[0].full_scan_mul = 1;

	/* Disable EBS. */
	req->channel_opt[0].non_ebs_ratio = 1;
	req->channel_opt[1].non_ebs_ratio = 1;

	ret = iwm_send_cmd(sc, &hcmd);
	if (!ret)
		DPRINTF(("Scan request was sent successfully\n"));
	free(req, M_DEVBUF, req_len);
	return ret;
}

int
iwm_mvm_config_umac_scan(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_scan_config *scan_config;
	int ret, nchan;
	size_t cmd_size;
	struct ieee80211_channel *c;
	struct iwm_host_cmd hcmd = {
		.id = iwm_cmd_id(IWM_SCAN_CFG_CMD, IWM_ALWAYS_LONG_GROUP, 0),
		.flags = IWM_CMD_SYNC,
	};
	static const uint32_t rates = (IWM_SCAN_CONFIG_RATE_1M |
	    IWM_SCAN_CONFIG_RATE_2M | IWM_SCAN_CONFIG_RATE_5M |
	    IWM_SCAN_CONFIG_RATE_11M | IWM_SCAN_CONFIG_RATE_6M |
	    IWM_SCAN_CONFIG_RATE_9M | IWM_SCAN_CONFIG_RATE_12M |
	    IWM_SCAN_CONFIG_RATE_18M | IWM_SCAN_CONFIG_RATE_24M |
	    IWM_SCAN_CONFIG_RATE_36M | IWM_SCAN_CONFIG_RATE_48M |
	    IWM_SCAN_CONFIG_RATE_54M);

	cmd_size = sizeof(*scan_config) + sc->sc_capa_n_scan_channels;

	scan_config = malloc(cmd_size, M_DEVBUF, M_WAIT | M_CANFAIL | M_ZERO);
	if (scan_config == NULL)
		return ENOMEM;

	scan_config->tx_chains = htole32(iwm_fw_valid_tx_ant(sc));
	scan_config->rx_chains = htole32(iwm_fw_valid_rx_ant(sc));
	scan_config->legacy_rates = htole32(rates |
	    IWM_SCAN_CONFIG_SUPPORTED_RATE(rates));

	/* These timings correspond to iwlwifi's UNASSOC scan. */
	scan_config->dwell_active = 10;
	scan_config->dwell_passive = 110;
	scan_config->dwell_fragmented = 44;
	scan_config->dwell_extended = 90;
	scan_config->out_of_channel_time = htole32(0);
	scan_config->suspend_time = htole32(0);

	IEEE80211_ADDR_COPY(scan_config->mac_addr, sc->sc_ic.ic_myaddr);

	scan_config->bcast_sta_id = sc->sc_aux_sta.sta_id;
	scan_config->channel_flags = IWM_CHANNEL_FLAG_EBS |
	    IWM_CHANNEL_FLAG_ACCURATE_EBS | IWM_CHANNEL_FLAG_EBS_ADD |
	    IWM_CHANNEL_FLAG_PRE_SCAN_PASSIVE2ACTIVE;

	for (c = &ic->ic_channels[1], nchan = 0;
	    c <= &ic->ic_channels[IEEE80211_CHAN_MAX] &&
	    nchan < sc->sc_capa_n_scan_channels; c++) {
		if (c->ic_flags == 0)
			continue;
		scan_config->channel_array[nchan++] =
		    ieee80211_mhz2ieee(c->ic_freq, 0);
	}

	scan_config->flags = htole32(IWM_SCAN_CONFIG_FLAG_ACTIVATE |
	    IWM_SCAN_CONFIG_FLAG_ALLOW_CHUB_REQS |
	    IWM_SCAN_CONFIG_FLAG_SET_TX_CHAINS |
	    IWM_SCAN_CONFIG_FLAG_SET_RX_CHAINS |
	    IWM_SCAN_CONFIG_FLAG_SET_AUX_STA_ID |
	    IWM_SCAN_CONFIG_FLAG_SET_ALL_TIMES |
	    IWM_SCAN_CONFIG_FLAG_SET_LEGACY_RATES |
	    IWM_SCAN_CONFIG_FLAG_SET_MAC_ADDR |
	    IWM_SCAN_CONFIG_FLAG_SET_CHANNEL_FLAGS|
	    IWM_SCAN_CONFIG_N_CHANNELS(nchan) |
	    IWM_SCAN_CONFIG_FLAG_CLEAR_FRAGMENTED);

	hcmd.data[0] = scan_config;
	hcmd.len[0] = cmd_size;

	DPRINTF(("Sending UMAC scan config\n"));

	ret = iwm_send_cmd(sc, &hcmd);
	if (!ret)
		DPRINTF(("UMAC scan config was sent successfully\n"));

	free(scan_config, M_DEVBUF, cmd_size);
	return ret;
}

int
iwm_mvm_umac_scan(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_host_cmd hcmd = {
		.id = iwm_cmd_id(IWM_SCAN_REQ_UMAC, IWM_ALWAYS_LONG_GROUP, 0),
		.len = { 0, },
		.data = { NULL, },
		.flags = IWM_CMD_SYNC,
	};
	struct iwm_scan_req_umac *req;
	struct iwm_scan_req_umac_tail *tail;
	size_t req_len;
	int ret;

	req_len = sizeof(struct iwm_scan_req_umac) +
	    (sizeof(struct iwm_scan_channel_cfg_umac) *
	    sc->sc_capa_n_scan_channels) +
	    sizeof(struct iwm_scan_req_umac_tail);
	if (req_len > IWM_MAX_CMD_PAYLOAD_SIZE)
		return ENOMEM;
	req = malloc(req_len, M_DEVBUF, M_WAIT | M_CANFAIL | M_ZERO);
	if (req == NULL)
		return ENOMEM;

	hcmd.len[0] = (uint16_t)req_len;
	hcmd.data[0] = (void *)req;

	DPRINTF(("Handling ieee80211 scan request\n"));

	/* These timings correspond to iwlwifi's UNASSOC scan. */
	req->active_dwell = 10;
	req->passive_dwell = 110;
	req->fragmented_dwell = 44;
	req->extended_dwell = 90;
	req->max_out_time = 0;
	req->suspend_time = 0;

	req->scan_priority = htole32(IWM_SCAN_PRIORITY_HIGH);
	req->ooc_priority = htole32(IWM_SCAN_PRIORITY_HIGH);

	req->n_channels = iwm_mvm_umac_scan_fill_channels(sc,
	    (struct iwm_scan_channel_cfg_umac *)req->data,
	    ic->ic_des_esslen != 0);

	req->general_flags = htole32(IWM_UMAC_SCAN_GEN_FLAGS_PASS_ALL |
	    IWM_UMAC_SCAN_GEN_FLAGS_ITER_COMPLETE |
	    IWM_UMAC_SCAN_GEN_FLAGS_EXTENDED_DWELL);

	tail = (void *)&req->data +
		sizeof(struct iwm_scan_channel_cfg_umac) *
			sc->sc_capa_n_scan_channels;

	/* Check if we're doing an active directed scan. */
	if (ic->ic_des_esslen != 0) {
		tail->direct_scan[0].id = IEEE80211_ELEMID_SSID;
		tail->direct_scan[0].len = ic->ic_des_esslen;
		memcpy(tail->direct_scan[0].ssid, ic->ic_des_essid,
		    ic->ic_des_esslen);
		req->general_flags |=
		    htole32(IWM_UMAC_SCAN_GEN_FLAGS_PRE_CONNECT);
	} else
		req->general_flags |= htole32(IWM_UMAC_SCAN_GEN_FLAGS_PASSIVE);

	if (isset(sc->sc_enabled_capa, 
	    IWM_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT))
		req->general_flags |=
		    htole32(IWM_UMAC_SCAN_GEN_FLAGS_RRM_ENABLED);

	ret = iwm_mvm_fill_probe_req(sc, &tail->preq);
	if (ret) {
		free(req, M_DEVBUF, req_len);
		return ret;
	}

	/* Specify the scan plan: We'll do one iteration. */
	tail->schedule[0].interval = 0;
	tail->schedule[0].iter_count = 1;

	ret = iwm_send_cmd(sc, &hcmd);
	if (!ret)
		DPRINTF(("Scan request was sent successfully\n"));
	free(req, M_DEVBUF, req_len);
	return ret;
}

void
iwm_mvm_ack_rates(struct iwm_softc *sc, struct iwm_node *in,
	int *cck_rates, int *ofdm_rates)
{
	struct ieee80211_node *ni = &in->in_ni;
	int lowest_present_ofdm = 100;
	int lowest_present_cck = 100;
	uint8_t cck = 0;
	uint8_t ofdm = 0;
	int i;

	if (ni->ni_chan == IEEE80211_CHAN_ANYC ||
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		for (i = 0; i <= IWM_LAST_CCK_RATE; i++) {
			cck |= (1 << i);
			if (lowest_present_cck > i)
				lowest_present_cck = i;
		}
	}
	for (i = IWM_FIRST_OFDM_RATE; i <= IWM_LAST_NON_HT_RATE; i++) {
		int adj = i - IWM_FIRST_OFDM_RATE;
		ofdm |= (1 << adj);
		if (lowest_present_ofdm > i)
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

	if (IWM_RATE_24M_INDEX < lowest_present_ofdm)
		ofdm |= IWM_RATE_BIT_MSK(24) >> IWM_FIRST_OFDM_RATE;
	if (IWM_RATE_12M_INDEX < lowest_present_ofdm)
		ofdm |= IWM_RATE_BIT_MSK(12) >> IWM_FIRST_OFDM_RATE;
	/* 6M already there or needed so always add */
	ofdm |= IWM_RATE_BIT_MSK(6) >> IWM_FIRST_OFDM_RATE;

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
	if (IWM_RATE_11M_INDEX < lowest_present_cck)
		cck |= IWM_RATE_BIT_MSK(11) >> IWM_FIRST_CCK_RATE;
	if (IWM_RATE_5M_INDEX < lowest_present_cck)
		cck |= IWM_RATE_BIT_MSK(5) >> IWM_FIRST_CCK_RATE;
	if (IWM_RATE_2M_INDEX < lowest_present_cck)
		cck |= IWM_RATE_BIT_MSK(2) >> IWM_FIRST_CCK_RATE;
	/* 1M already there or needed so always add */
	cck |= IWM_RATE_BIT_MSK(1) >> IWM_FIRST_CCK_RATE;

	*cck_rates = cck;
	*ofdm_rates = ofdm;
}

void
iwm_mvm_mac_ctxt_cmd_common(struct iwm_softc *sc, struct iwm_node *in,
	struct iwm_mac_ctx_cmd *cmd, uint32_t action)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	int cck_ack_rates, ofdm_ack_rates;
	int i;

	cmd->id_and_color = htole32(IWM_FW_CMD_ID_AND_COLOR(in->in_id,
	    in->in_color));
	cmd->action = htole32(action);

	cmd->mac_type = htole32(IWM_FW_MAC_TYPE_BSS_STA);
	cmd->tsf_id = htole32(in->in_tsfid);

	IEEE80211_ADDR_COPY(cmd->node_addr, ic->ic_myaddr);
	if (in->in_assoc) {
		IEEE80211_ADDR_COPY(cmd->bssid_addr, ni->ni_bssid);
	} else {
		IEEE80211_ADDR_COPY(cmd->bssid_addr, etherbroadcastaddr);
	}
	iwm_mvm_ack_rates(sc, in, &cck_ack_rates, &ofdm_ack_rates);
	cmd->cck_rates = htole32(cck_ack_rates);
	cmd->ofdm_rates = htole32(ofdm_ack_rates);

	cmd->cck_short_preamble
	    = htole32((ic->ic_flags & IEEE80211_F_SHPREAMBLE)
	      ? IWM_MAC_FLG_SHORT_PREAMBLE : 0);
	cmd->short_slot
	    = htole32((ic->ic_flags & IEEE80211_F_SHSLOT)
	      ? IWM_MAC_FLG_SHORT_SLOT : 0);

	for (i = 0; i < IWM_AC_NUM+1; i++) {
		int txf = i;

		cmd->ac[txf].cw_min = htole16(0x0f);
		cmd->ac[txf].cw_max = htole16(0x3f);
		cmd->ac[txf].aifsn = 1;
		cmd->ac[txf].fifos_mask = (1 << txf);
		cmd->ac[txf].edca_txop = 0;
	}

	if (ni->ni_flags & IEEE80211_NODE_HT) {
		enum ieee80211_htprot htprot =
		    (ni->ni_htop1 & IEEE80211_HTOP1_PROT_MASK);
		switch (htprot) {
		case IEEE80211_HTPROT_NONE:
			break;
		case IEEE80211_HTPROT_NONMEMBER:
		case IEEE80211_HTPROT_NONHT_MIXED:
			cmd->protection_flags |=
			    htole32(IWM_MAC_PROT_FLG_HT_PROT);
		case IEEE80211_HTPROT_20MHZ:
			cmd->protection_flags |=
			    htole32(IWM_MAC_PROT_FLG_HT_PROT |
			    IWM_MAC_PROT_FLG_FAT_PROT);
			break;
		default:
			DPRINTF(("Unknown protection mode %d\n", htprot));
			break;
		}

		cmd->qos_flags |= htole32(IWM_MAC_QOS_FLG_TGN);
	}
	if (ic->ic_flags & IEEE80211_F_USEPROT)
		cmd->protection_flags |= htole32(IWM_MAC_PROT_FLG_TGG_PROTECT);

	cmd->filter_flags = htole32(IWM_MAC_FILTER_ACCEPT_GRP);
}

int
iwm_mvm_mac_ctxt_send_cmd(struct iwm_softc *sc, struct iwm_mac_ctx_cmd *cmd)
{
	int ret = iwm_mvm_send_cmd_pdu(sc, IWM_MAC_CONTEXT_CMD, IWM_CMD_SYNC,
				       sizeof(*cmd), cmd);
	if (ret)
		DPRINTF(("%s: Failed to send MAC context (action:%d): %d\n",
		    DEVNAME(sc), le32toh(cmd->action), ret));
	return ret;
}

/*
 * Fill the specific data for mac context of type station or p2p client
 */
void
iwm_mvm_mac_ctxt_cmd_fill_sta(struct iwm_softc *sc, struct iwm_node *in,
	struct iwm_mac_data_sta *ctxt_sta, int force_assoc_off)
{
	struct ieee80211_node *ni = &in->in_ni;
	unsigned dtim_period, dtim_count;
	struct ieee80211com *ic = &sc->sc_ic;

	/* will this work? */
	dtim_period = ic->ic_dtim_period;
	dtim_count = ic->ic_dtim_count;
	DPRINTF(("dtim %d %d\n", dtim_period, dtim_count));

	/* We need the dtim_period to set the MAC as associated */
	if (in->in_assoc && dtim_period && !force_assoc_off) {
		uint64_t tsf;
		uint32_t dtim_offs;

		/*
		 * The DTIM count counts down, so when it is N that means N
		 * more beacon intervals happen until the DTIM TBTT. Therefore
		 * add this to the current time. If that ends up being in the
		 * future, the firmware will handle it.
		 *
		 * Also note that the system_timestamp (which we get here as
		 * "sync_device_ts") and TSF timestamp aren't at exactly the
		 * same offset in the frame -- the TSF is at the first symbol
		 * of the TSF, the system timestamp is at signal acquisition
		 * time. This means there's an offset between them of at most
		 * a few hundred microseconds (24 * 8 bits + PLCP time gives
		 * 384us in the longest case), this is currently not relevant
		 * as the firmware wakes up around 2ms before the TBTT.
		 */
		dtim_offs = dtim_count * ni->ni_intval;
		/* convert TU to usecs */
		dtim_offs *= 1024;

		/* XXX: byte order? */
		memcpy(&tsf, ni->ni_tstamp, sizeof(tsf));

		ctxt_sta->dtim_tsf = htole64(tsf + dtim_offs);
		ctxt_sta->dtim_time = htole64(ni->ni_rstamp + dtim_offs);

		DPRINTF(("DTIM TBTT is 0x%llx/0x%x, offset %d\n",
		    (long long)le64toh(ctxt_sta->dtim_tsf),
		    le32toh(ctxt_sta->dtim_time), dtim_offs));

		ctxt_sta->is_assoc = htole32(1);
	} else {
		ctxt_sta->is_assoc = htole32(0);
	}

	ctxt_sta->bi = htole32(ni->ni_intval);
	ctxt_sta->bi_reciprocal = htole32(iwm_mvm_reciprocal(ni->ni_intval));
	ctxt_sta->dtim_interval = htole32(ni->ni_intval * dtim_period);
	ctxt_sta->dtim_reciprocal =
	    htole32(iwm_mvm_reciprocal(ni->ni_intval * dtim_period));

	/* 10 = CONN_MAX_LISTEN_INTERVAL */
	ctxt_sta->listen_interval = htole32(10);
	ctxt_sta->assoc_id = htole32(ni->ni_associd);
}

int
iwm_mvm_mac_ctxt_cmd_station(struct iwm_softc *sc, struct iwm_node *in,
	uint32_t action)
{
	struct iwm_mac_ctx_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	/* Fill the common data for all mac context types */
	iwm_mvm_mac_ctxt_cmd_common(sc, in, &cmd, action);

	/* Allow beacons to pass through as long as we are not associated,or we
	 * do not have dtim period information */
	if (!in->in_assoc || !sc->sc_ic.ic_dtim_period)
		cmd.filter_flags |= htole32(IWM_MAC_FILTER_IN_BEACON);
	else
		cmd.filter_flags &= ~htole32(IWM_MAC_FILTER_IN_BEACON);

	/* Fill the data specific for station mode */
	iwm_mvm_mac_ctxt_cmd_fill_sta(sc, in,
	    &cmd.sta, action == IWM_FW_CTXT_ACTION_ADD);

	return iwm_mvm_mac_ctxt_send_cmd(sc, &cmd);
}

int
iwm_mvm_mac_ctx_send(struct iwm_softc *sc, struct iwm_node *in, uint32_t action)
{
	return iwm_mvm_mac_ctxt_cmd_station(sc, in, action);
}

int
iwm_mvm_mac_ctxt_add(struct iwm_softc *sc, struct iwm_node *in)
{
	int ret;

	ret = iwm_mvm_mac_ctx_send(sc, in, IWM_FW_CTXT_ACTION_ADD);
	if (ret)
		return ret;

	return 0;
}

int
iwm_mvm_mac_ctxt_changed(struct iwm_softc *sc, struct iwm_node *in)
{
	return iwm_mvm_mac_ctx_send(sc, in, IWM_FW_CTXT_ACTION_MODIFY);
}

#if 0
int
iwm_mvm_mac_ctxt_remove(struct iwm_softc *sc, struct iwm_node *in)
{
	struct iwm_mac_ctx_cmd cmd;
	int ret;

	if (!in->in_uploaded) {
		print("%s: attempt to remove !uploaded node %p", DEVNAME(sc), in);
		return EIO;
	}

	memset(&cmd, 0, sizeof(cmd));

	cmd.id_and_color = htole32(IWM_FW_CMD_ID_AND_COLOR(in->in_id,
	    in->in_color));
	cmd.action = htole32(IWM_FW_CTXT_ACTION_REMOVE);

	ret = iwm_mvm_send_cmd_pdu(sc,
	    IWM_MAC_CONTEXT_CMD, IWM_CMD_SYNC, sizeof(cmd), &cmd);
	if (ret) {
		printf("%s: Failed to remove MAC context: %d\n", DEVNAME(sc), ret);
		return ret;
	}
	in->in_uploaded = 0;

	return 0;
}
#endif

int
iwm_mvm_update_quotas(struct iwm_softc *sc, struct iwm_node *in)
{
	struct iwm_time_quota_cmd cmd;
	int i, idx, ret, num_active_macs, quota, quota_rem;
	int colors[IWM_MAX_BINDINGS] = { -1, -1, -1, -1, };
	int n_ifs[IWM_MAX_BINDINGS] = {0, };
	uint16_t id;

	memset(&cmd, 0, sizeof(cmd));

	/* currently, PHY ID == binding ID */
	if (in) {
		id = in->in_phyctxt->id;
		KASSERT(id < IWM_MAX_BINDINGS);
		colors[id] = in->in_phyctxt->color;

		if (1)
			n_ifs[id] = 1;
	}

	/*
	 * The FW's scheduling session consists of
	 * IWM_MVM_MAX_QUOTA fragments. Divide these fragments
	 * equally between all the bindings that require quota
	 */
	num_active_macs = 0;
	for (i = 0; i < IWM_MAX_BINDINGS; i++) {
		cmd.quotas[i].id_and_color = htole32(IWM_FW_CTXT_INVALID);
		num_active_macs += n_ifs[i];
	}

	quota = 0;
	quota_rem = 0;
	if (num_active_macs) {
		quota = IWM_MVM_MAX_QUOTA / num_active_macs;
		quota_rem = IWM_MVM_MAX_QUOTA % num_active_macs;
	}

	for (idx = 0, i = 0; i < IWM_MAX_BINDINGS; i++) {
		if (colors[i] < 0)
			continue;

		cmd.quotas[idx].id_and_color =
			htole32(IWM_FW_CMD_ID_AND_COLOR(i, colors[i]));

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

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_TIME_QUOTA_CMD, IWM_CMD_SYNC,
	    sizeof(cmd), &cmd);
	if (ret)
		DPRINTF(("%s: Failed to send quota: %d\n", DEVNAME(sc), ret));
	return ret;
}

/*
 * aieee80211 routines
 */

/*
 * Change to AUTH state in 80211 state machine.  Roughly matches what
 * Linux does in bss_info_changed().
 */
int
iwm_auth(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_node *in = (void *)ic->ic_bss;
	uint32_t duration;
	int error;

	in->in_assoc = 0;

	error = iwm_mvm_sf_config(sc, IWM_SF_FULL_ON);
	if (error != 0)
		return error;

	error = iwm_allow_mcast(sc);
	if (error)
		return error;

	if ((error = iwm_mvm_phy_ctxt_changed(sc, &sc->sc_phyctxt[0],
	    in->in_ni.ni_chan, 1, 1)) != 0) {
		DPRINTF(("%s: failed add phy ctxt\n", DEVNAME(sc)));
		return error;
	}
	in->in_phyctxt = &sc->sc_phyctxt[0];

	if ((error = iwm_mvm_binding_add_vif(sc, in)) != 0) {
		DPRINTF(("%s: binding cmd\n", DEVNAME(sc)));
		return error;
	}

	if ((error = iwm_mvm_add_sta(sc, in)) != 0) {
		DPRINTF(("%s: failed to add STA\n", DEVNAME(sc)));
		return error;
	}

	if ((error = iwm_mvm_mac_ctxt_changed(sc, in)) != 0) {
		printf("%s: failed to update MAC\n", DEVNAME(sc));
		return error;
	}

	/*
	 * Prevent the FW from wandering off channel during association
	 * by "protecting" the session with a time event.
	 */
	/* XXX duration is in units of TU, not MS */
	duration = IWM_MVM_TE_SESSION_PROTECTION_MAX_TIME_MS;
	iwm_mvm_protect_session(sc, in, duration, 500 /* XXX magic number */);
	DELAY(100);

	return 0;
}

int
iwm_assoc(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_node *in = (void *)ic->ic_bss;
	int error;

	if ((error = iwm_mvm_update_sta(sc, in)) != 0) {
		DPRINTF(("%s: failed to update STA\n", DEVNAME(sc)));
		return error;
	}

	in->in_assoc = 1;

	if ((error = iwm_mvm_mac_ctxt_changed(sc, in)) != 0) {
		printf("%s: failed to update MAC\n", DEVNAME(sc));
		return error;
	}

	return 0;
}

int
iwm_release(struct iwm_softc *sc, struct iwm_node *in)
{
	/*
	 * Ok, so *technically* the proper set of calls for going
	 * from RUN back to SCAN is:
	 *
	 * iwm_mvm_power_mac_disable(sc, in);
	 * iwm_mvm_mac_ctxt_changed(sc, in);
	 * iwm_mvm_rm_sta(sc, in);
	 * iwm_mvm_update_quotas(sc, NULL);
	 * iwm_mvm_mac_ctxt_changed(sc, in);
	 * iwm_mvm_binding_remove_vif(sc, in);
	 * iwm_mvm_mac_ctxt_remove(sc, in);
	 *
	 * However, that freezes the device not matter which permutations
	 * and modifications are attempted.  Obviously, this driver is missing
	 * something since it works in the Linux driver, but figuring out what
	 * is missing is a little more complicated.  Now, since we're going
	 * back to nothing anyway, we'll just do a complete device reset.
	 * Up your's, device!
	 */
	/* iwm_mvm_flush_tx_path(sc, 0xf, 1); */
	iwm_stop_device(sc);
	iwm_init_hw(sc);
	if (in)
		in->in_assoc = 0;
	return 0;

#if 0
	int error;

	iwm_mvm_power_mac_disable(sc, in);

	if ((error = iwm_mvm_mac_ctxt_changed(sc, in)) != 0) {
		printf("%s: mac ctxt change fail 1 %d\n", DEVNAME(sc), error);
		return error;
	}

	if ((error = iwm_mvm_rm_sta(sc, in)) != 0) {
		printf("%s: sta remove fail %d\n", DEVNAME(sc), error);
		return error;
	}
	error = iwm_mvm_rm_sta(sc, in);
	in->in_assoc = 0;
	iwm_mvm_update_quotas(sc, NULL);
	if ((error = iwm_mvm_mac_ctxt_changed(sc, in)) != 0) {
		printf("%s: mac ctxt change fail 2 %d\n", DEVNAME(sc), error);
		return error;
	}
	iwm_mvm_binding_remove_vif(sc, in);

	iwm_mvm_mac_ctxt_remove(sc, in);

	return error;
#endif
}

struct ieee80211_node *
iwm_node_alloc(struct ieee80211com *ic)
{
	return malloc(sizeof (struct iwm_node), M_DEVBUF, M_NOWAIT | M_ZERO);
}

void
iwm_calib_timeout(void *arg)
{
	struct iwm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_node *in = (void *)ic->ic_bss;
	struct ieee80211_node *ni = &in->in_ni;
	int s, otxrate;

	s = splnet();
	if ((ic->ic_fixed_rate == -1 || ic->ic_fixed_mcs == -1) &&
	    ic->ic_opmode == IEEE80211_M_STA && ic->ic_bss) {
		if (ni->ni_flags & IEEE80211_NODE_HT)
			otxrate = ni->ni_txmcs;
		else
			otxrate = ni->ni_txrate;
		ieee80211_amrr_choose(&sc->sc_amrr, &in->in_ni, &in->in_amn);

		/* 
		 * If AMRR has chosen a new TX rate we must update
		 * the firwmare's LQ rate table from process context.
		 */
		if ((ni->ni_flags & IEEE80211_NODE_HT) &&
		    otxrate != ni->ni_txmcs)
			task_add(systq, &sc->setrates_task);
		else if (otxrate != ni->ni_txrate)
			task_add(systq, &sc->setrates_task);
	}
	splx(s);

	timeout_add_msec(&sc->sc_calib_to, 500);
}

void
iwm_setrates_task(void *arg)
{
	struct iwm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_node *in = (struct iwm_node *)ic->ic_bss;

	/* Update rates table based on new TX rate determined by AMRR. */
	iwm_setrates(in);
}

void
iwm_setrates(struct iwm_node *in)
{
	struct ieee80211_node *ni = &in->in_ni;
	struct ieee80211com *ic = ni->ni_ic;
	struct iwm_softc *sc = IC2IFP(ic)->if_softc;
	struct iwm_lq_cmd *lq = &in->in_lq;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	int i, ridx, j, tab = 0;
	struct iwm_host_cmd cmd = {
		.id = IWM_LQ_CMD,
		.len = { sizeof(in->in_lq), },
		.flags = IWM_CMD_SYNC,
	};

	memset(lq, 0, sizeof(*lq));
	lq->sta_id = IWM_STATION_ID;

	/* For HT, always enable RTS/CTS to avoid excessive retries. */
	if (ni->ni_flags & IEEE80211_NODE_HT)
		lq->flags |= IWM_LQ_FLAG_USE_RTS_MSK;

	/*
	 * Fill the LQ rate selection table with legacy and/or HT rates
	 * in descending order, i.e. with the node's current TX rate first.
	 * In cases where throughput of an HT rate corresponds to a legacy
	 * rate it makes no sense to add both. We rely on the fact that
	 * iwm_rates is laid out such that equivalent HT/legacy rates share
	 * the same IWM_RATE_*_INDEX value. Also, rates not applicable to
	 * legacy/HT are assumed to be marked with an 'invalid' PLCP value.
	 */
	j = 0;
	for (ridx = IWM_RIDX_MAX; ridx >= 0; ridx--) {
		if (j >= nitems(lq->rs_table))
			break;
		tab = 0;
		if ((ni->ni_flags & IEEE80211_NODE_HT) &&
		    iwm_rates[ridx].ht_plcp != IWM_RATE_HT_SISO_MCS_INV_PLCP) {
			for (i = ni->ni_txmcs; i >= 0; i--) {
				if (isclr(ni->ni_rxmcs, i))
					continue;
				if (ridx == iwm_mcs2ridx[i]) {
					tab = iwm_rates[ridx].ht_plcp;
					tab |= IWM_RATE_MCS_HT_MSK;
					break;
				}
			}
		}
		if (tab == 0 && iwm_rates[ridx].plcp != IWM_RATE_INVM_PLCP) {
			for (i = ni->ni_txrate; i >= 0; i--) {
				if (iwm_rates[ridx].rate == (rs->rs_rates[i] &
				    IEEE80211_RATE_VAL)) {
					tab = iwm_rates[ridx].plcp;
					break;
				}
			}
		}

		if (tab == 0)
			continue;

		tab |= 1 << IWM_RATE_MCS_ANT_POS;
		if (IWM_RIDX_IS_CCK(ridx))
			tab |= IWM_RATE_MCS_CCK_MSK;
		DPRINTFN(2, ("station rate %d %x\n", j, tab));
		lq->rs_table[j++] = htole32(tab);
	}

	/* Fill the rest with the lowest possible rate */
	i = j > 0 ? j - 1 : 0;
	while (j < nitems(lq->rs_table))
		lq->rs_table[j++] = lq->rs_table[i];

	lq->single_stream_ant_msk = IWM_ANT_A;
	lq->dual_stream_ant_msk = IWM_ANT_AB;

	lq->agg_time_limit = htole16(4000);	/* 4ms */
	lq->agg_disable_start_th = 3;
	lq->agg_frame_cnt_limit = 0x3f;

	cmd.data[0] = &in->in_lq;
	if (iwm_send_cmd(sc, &cmd) != 0) {
		DPRINTF(("%s: IWM_LQ_CMD failed\n", DEVNAME(sc)));
	}
}

int
iwm_media_change(struct ifnet *ifp)
{
	struct iwm_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t rate, ridx;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if (ic->ic_fixed_mcs != -1)
		sc->sc_fixed_ridx = iwm_mcs2ridx[ic->ic_fixed_mcs];
	else if (ic->ic_fixed_rate != -1) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		/* Map 802.11 rate to HW rate index. */
		for (ridx = 0; ridx <= IWM_RIDX_MAX; ridx++)
			if (iwm_rates[ridx].rate == rate)
				break;
		sc->sc_fixed_ridx = ridx;
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		iwm_stop(ifp, 0);
		error = iwm_init(ifp);
	}
	return error;
}

void
iwm_newstate_task(void *psc)
{
	struct iwm_softc *sc = (struct iwm_softc *)psc;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state nstate = sc->ns_nstate;
	enum ieee80211_state ostate = ic->ic_state;
	struct iwm_node *in;
	int arg = sc->ns_arg;
	int error;

	DPRINTF(("switching state %s->%s\n",
	    ieee80211_state_name[ostate],
	    ieee80211_state_name[nstate]));

	if (ostate == IEEE80211_S_SCAN && nstate != ostate)
		iwm_led_blink_stop(sc);

	/* disable beacon filtering if we're hopping out of RUN */
	if (ostate == IEEE80211_S_RUN && nstate != ostate)
		iwm_mvm_disable_beacon_filter(sc);

	/* Reset the device if moving out of AUTH, ASSOC, or RUN. */
	if (ostate > IEEE80211_S_SCAN && nstate < ostate) {
		if (((in = (void *)ic->ic_bss) != NULL))
			in->in_assoc = 0;
		iwm_release(sc, NULL);

		/*
		 * It's impossible to directly go RUN->SCAN. If we iwm_release()
		 * above then the card will be completely reinitialized,
		 * so the driver must do everything necessary to bring the card
		 * from INIT to SCAN.
		 *
		 * Additionally, upon receiving deauth frame from AP,
		 * OpenBSD 802.11 stack puts the driver in IEEE80211_S_AUTH
		 * state. This will also fail with this driver, so bring the FSM
		 * from IEEE80211_S_RUN to IEEE80211_S_SCAN in this case as well.
		 */
		if (nstate == IEEE80211_S_SCAN ||
		    nstate == IEEE80211_S_AUTH ||
		    nstate == IEEE80211_S_ASSOC) {
			DPRINTF(("Force transition to INIT; MGT=%d\n", arg));
			/* Always pass arg as -1 since we can't Tx right now. */
			sc->sc_newstate(ic, IEEE80211_S_INIT, -1);
			DPRINTF(("Going INIT->SCAN\n"));
			nstate = IEEE80211_S_SCAN;
		}
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;

	case IEEE80211_S_SCAN:
		if (ic->ic_state == nstate &&
		    (sc->sc_flags & IWM_FLAG_SCANNING))
			return;
		if (isset(sc->sc_enabled_capa, IWM_UCODE_TLV_CAPA_UMAC_SCAN))
			error = iwm_mvm_umac_scan(sc);
		else
			error = iwm_mvm_lmac_scan(sc);
		if (error != 0) {
			printf("%s: could not initiate scan\n", DEVNAME(sc));
			return;
		}
		sc->sc_flags |= IWM_FLAG_SCANNING;
		ic->ic_state = nstate;
		iwm_led_blink_start(sc);
		return;

	case IEEE80211_S_AUTH:
		if ((error = iwm_auth(sc)) != 0) {
			DPRINTF(("%s: could not move to auth state: %d\n",
			    DEVNAME(sc), error));
			return;
		}

		break;

	case IEEE80211_S_ASSOC:
		if ((error = iwm_assoc(sc)) != 0) {
			DPRINTF(("%s: failed to associate: %d\n", DEVNAME(sc),
			    error));
			return;
		}
		break;

	case IEEE80211_S_RUN: {
		in = (struct iwm_node *)ic->ic_bss;
		iwm_mvm_power_mac_update_mode(sc, in);
		iwm_mvm_enable_beacon_filter(sc, in);
		iwm_mvm_update_quotas(sc, in);

		/* init amrr */
		ieee80211_amrr_node_init(&sc->sc_amrr, &in->in_amn);

		/* Start at lowest available bit-rate, AMRR will raise. */
		in->in_ni.ni_txrate = 0;
		in->in_ni.ni_txmcs = 0;
		iwm_setrates(in);

		timeout_add_msec(&sc->sc_calib_to, 500);
		iwm_mvm_led_enable(sc);
		break; }

	default:
		break;
	}

	sc->sc_newstate(ic, nstate, arg);
}

int
iwm_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = IC2IFP(ic);
	struct iwm_softc *sc = ifp->if_softc;

	timeout_del(&sc->sc_calib_to);

	sc->ns_nstate = nstate;
	sc->ns_arg = arg;

	task_add(sc->sc_nswq, &sc->newstate_task);

	return 0;
}

void
iwm_endscan_cb(void *arg)
{
	struct iwm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(("scan ended\n"));

	sc->sc_flags &= ~IWM_FLAG_SCANNING;
	ieee80211_end_scan(&ic->ic_if);
}

/*
 * Aging and idle timeouts for the different possible scenarios
 * in default configuration
 */
static const uint32_t
iwm_sf_full_timeout_def[IWM_SF_NUM_SCENARIO][IWM_SF_NUM_TIMEOUT_TYPES] = {
	{
		htole32(IWM_SF_SINGLE_UNICAST_AGING_TIMER_DEF),
		htole32(IWM_SF_SINGLE_UNICAST_IDLE_TIMER_DEF)
	},
	{
		htole32(IWM_SF_AGG_UNICAST_AGING_TIMER_DEF),
		htole32(IWM_SF_AGG_UNICAST_IDLE_TIMER_DEF)
	},
	{
		htole32(IWM_SF_MCAST_AGING_TIMER_DEF),
		htole32(IWM_SF_MCAST_IDLE_TIMER_DEF)
	},
	{
		htole32(IWM_SF_BA_AGING_TIMER_DEF),
		htole32(IWM_SF_BA_IDLE_TIMER_DEF)
	},
	{
		htole32(IWM_SF_TX_RE_AGING_TIMER_DEF),
		htole32(IWM_SF_TX_RE_IDLE_TIMER_DEF)
	},
};

/*
 * Aging and idle timeouts for the different possible scenarios
 * in single BSS MAC configuration.
 */
static const uint32_t
iwm_sf_full_timeout[IWM_SF_NUM_SCENARIO][IWM_SF_NUM_TIMEOUT_TYPES] = {
	{
		htole32(IWM_SF_SINGLE_UNICAST_AGING_TIMER),
		htole32(IWM_SF_SINGLE_UNICAST_IDLE_TIMER)
	},
	{
		htole32(IWM_SF_AGG_UNICAST_AGING_TIMER),
		htole32(IWM_SF_AGG_UNICAST_IDLE_TIMER)
	},
	{
		htole32(IWM_SF_MCAST_AGING_TIMER),
		htole32(IWM_SF_MCAST_IDLE_TIMER)
	},
	{
		htole32(IWM_SF_BA_AGING_TIMER),
		htole32(IWM_SF_BA_IDLE_TIMER)
	},
	{
		htole32(IWM_SF_TX_RE_AGING_TIMER),
		htole32(IWM_SF_TX_RE_IDLE_TIMER)
	},
};

void
iwm_mvm_fill_sf_command(struct iwm_softc *sc, struct iwm_sf_cfg_cmd *sf_cmd,
    struct ieee80211_node *ni)
{
	int i, j, watermark;

	sf_cmd->watermark[IWM_SF_LONG_DELAY_ON] = htole32(IWM_SF_W_MARK_SCAN);

	/*
	 * If we are in association flow - check antenna configuration
	 * capabilities of the AP station, and choose the watermark accordingly.
	 */
	if (ni) {
		if (ni->ni_flags & IEEE80211_NODE_HT) {
#ifdef notyet
			if (ni->ni_rxmcs[2] != 0)
				watermark = IWM_SF_W_MARK_MIMO3;
			else if (ni->ni_rxmcs[1] != 0)
				watermark = IWM_SF_W_MARK_MIMO2;
			else
#endif
				watermark = IWM_SF_W_MARK_SISO;
		} else {
			watermark = IWM_SF_W_MARK_LEGACY;
		}
	/* default watermark value for unassociated mode. */
	} else {
		watermark = IWM_SF_W_MARK_MIMO2;
	}
	sf_cmd->watermark[IWM_SF_FULL_ON] = htole32(watermark);

	for (i = 0; i < IWM_SF_NUM_SCENARIO; i++) {
		for (j = 0; j < IWM_SF_NUM_TIMEOUT_TYPES; j++) {
			sf_cmd->long_delay_timeouts[i][j] =
					htole32(IWM_SF_LONG_DELAY_AGING_TIMER);
		}
	}

	if (ni) {
		memcpy(sf_cmd->full_on_timeouts, iwm_sf_full_timeout,
		       sizeof(iwm_sf_full_timeout));
	} else {
		memcpy(sf_cmd->full_on_timeouts, iwm_sf_full_timeout_def,
		       sizeof(iwm_sf_full_timeout_def));
	}

}

int
iwm_mvm_sf_config(struct iwm_softc *sc, enum iwm_sf_state new_state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_sf_cfg_cmd sf_cmd = {
		.state = htole32(IWM_SF_FULL_ON),
	};
	int ret = 0;

	if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000)
		sf_cmd.state |= htole32(IWM_SF_CFG_DUMMY_NOTIF_OFF);

	switch (new_state) {
	case IWM_SF_UNINIT:
	case IWM_SF_INIT_OFF:
		iwm_mvm_fill_sf_command(sc, &sf_cmd, NULL);
		break;
	case IWM_SF_FULL_ON:
		iwm_mvm_fill_sf_command(sc, &sf_cmd, ic->ic_bss);
		break;
	default:
		DPRINTF(("Invalid state: %d. not sending Smart Fifo cmd\n",
			  new_state));
		return EINVAL;
	}

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_REPLY_SF_CFG_CMD, IWM_CMD_ASYNC,
				   sizeof(sf_cmd), &sf_cmd);
	return ret;
}

int
iwm_send_bt_init_conf(struct iwm_softc *sc)
{
	struct iwm_bt_coex_cmd bt_cmd;

	bt_cmd.mode = htole32(IWM_BT_COEX_WIFI);
	bt_cmd.enabled_modules = htole32(IWM_BT_COEX_HIGH_BAND_RET);

	return iwm_mvm_send_cmd_pdu(sc, IWM_BT_CONFIG, 0, sizeof(bt_cmd),
	    &bt_cmd);
}

int
iwm_send_update_mcc_cmd(struct iwm_softc *sc, const char *alpha2)
{
	struct iwm_mcc_update_cmd mcc_cmd;
	struct iwm_host_cmd hcmd = {
		.id = IWM_MCC_UPDATE_CMD,
		.flags = (IWM_CMD_SYNC | IWM_CMD_WANT_SKB),
		.data = { &mcc_cmd },
	};
	int ret;
#ifdef IWM_DEBUG
	struct iwm_rx_packet *pkt;
	struct iwm_mcc_update_resp_v1 *mcc_resp_v1 = NULL;
	struct iwm_mcc_update_resp *mcc_resp;
	int n_channels;
	uint16_t mcc;
#endif
	int resp_v2 = isset(sc->sc_enabled_capa,
	    IWM_UCODE_TLV_CAPA_LAR_SUPPORT_V2);

	memset(&mcc_cmd, 0, sizeof(mcc_cmd));
	mcc_cmd.mcc = htole16(alpha2[0] << 8 | alpha2[1]);
	if ((sc->sc_ucode_api & IWM_UCODE_TLV_API_WIFI_MCC_UPDATE) ||
	    isset(sc->sc_enabled_capa, IWM_UCODE_TLV_CAPA_LAR_MULTI_MCC))
		mcc_cmd.source_id = IWM_MCC_SOURCE_GET_CURRENT;
	else
		mcc_cmd.source_id = IWM_MCC_SOURCE_OLD_FW;

	if (resp_v2)
		hcmd.len[0] = sizeof(struct iwm_mcc_update_cmd);
	else
		hcmd.len[0] = sizeof(struct iwm_mcc_update_cmd_v1);

	DPRINTF(("send MCC update to FW with '%c%c' src = %d\n",
	    alpha2[0], alpha2[1], mcc_cmd.source_id));

	ret = iwm_send_cmd(sc, &hcmd);
	if (ret)
		return ret;

#ifdef IWM_DEBUG
	pkt = hcmd.resp_pkt;

	/* Extract MCC response */
	if (resp_v2) {
		mcc_resp = (void *)pkt->data;
		mcc = mcc_resp->mcc;
		n_channels =  le32toh(mcc_resp->n_channels);
	} else {
		mcc_resp_v1 = (void *)pkt->data;
		mcc = mcc_resp_v1->mcc;
		n_channels =  le32toh(mcc_resp_v1->n_channels);
	}

	/* W/A for a FW/NVM issue - returns 0x00 for the world domain */
	if (mcc == 0)
		mcc = 0x3030;  /* "00" - world */

	DPRINTF(("%s: regulatory domain '%c%c' (%d channels available)\n",
	    DEVNAME(sc), mcc >> 8, mcc & 0xff, n_channels));
#endif
	iwm_free_resp(sc, &hcmd);

	return 0;
}

void
iwm_mvm_tt_tx_backoff(struct iwm_softc *sc, uint32_t backoff)
{
	struct iwm_host_cmd cmd = {
		.id = IWM_REPLY_THERMAL_MNG_BACKOFF,
		.len = { sizeof(uint32_t), },
		.data = { &backoff, },
	};

	if (iwm_send_cmd(sc, &cmd) != 0)
		DPRINTF(("%s: failed to change thermal tx backoff\n",
		    DEVNAME(sc)));
}

int
iwm_init_hw(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwm_node *in = (struct iwm_node *)ic->ic_bss;
	int error, i, ac;

	if ((error = iwm_preinit(sc)) != 0)
		return error;

	if ((error = iwm_start_hw(sc)) != 0) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return error;
	}

	if ((error = iwm_run_init_mvm_ucode(sc, 0)) != 0)
		return error;

	/*
	 * should stop and start HW since that INIT
	 * image just loaded
	 */
	iwm_stop_device(sc);
	if ((error = iwm_start_hw(sc)) != 0) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return error;
	}

	/* Restart, this time with the regular firmware */
	error = iwm_mvm_load_ucode_wait_alive(sc, IWM_UCODE_TYPE_REGULAR);
	if (error) {
		printf("%s: could not load firmware\n", DEVNAME(sc));
		goto error;
	}

	if ((error = iwm_send_bt_init_conf(sc)) != 0) {
		printf("%s: could not init bt coex (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
	}

	if ((error = iwm_send_tx_ant_cfg(sc, iwm_fw_valid_tx_ant(sc))) != 0) {
		printf("%s: could not init tx ant config (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
	}

	/* Send phy db control command and then phy db calibration*/
	if ((error = iwm_send_phy_db_data(sc)) != 0) {
		printf("%s: could not init phy db (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
	}

	if ((error = iwm_send_phy_cfg_cmd(sc)) != 0) {
		printf("%s: could not send phy config (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
	}

	/* Add auxiliary station for scanning */
	if ((error = iwm_mvm_add_aux_sta(sc)) != 0) {
		printf("%s: could not add aux station (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
	}

	for (i = 0; i < IWM_NUM_PHY_CTX; i++) {
		/*
		 * The channel used here isn't relevant as it's
		 * going to be overwritten in the other flows.
		 * For now use the first channel we have.
		 */
		if ((error = iwm_mvm_phy_ctxt_add(sc,
		    &sc->sc_phyctxt[i], &ic->ic_channels[1], 1, 1)) != 0) {
			printf("%s: could not add phy context %d (error %d)\n",
			    DEVNAME(sc), i, error);
			goto error;
		}
	}

	/* Initialize tx backoffs to the minimum. */
	if (sc->sc_device_family == IWM_DEVICE_FAMILY_7000)
		iwm_mvm_tt_tx_backoff(sc, 0);

	error = iwm_mvm_power_update_device(sc);
	if (error) {
		printf("%s: could send power update command (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
	}

	if (isset(sc->sc_enabled_capa, IWM_UCODE_TLV_CAPA_LAR_SUPPORT)) {
		if ((error = iwm_send_update_mcc_cmd(sc, "ZZ")) != 0) {
			printf("%s: could not send mcc command (error %d)\n",
			    DEVNAME(sc), error);
			goto error;
		}
	}

	if (isset(sc->sc_enabled_capa, IWM_UCODE_TLV_CAPA_UMAC_SCAN)) {
		if ((error = iwm_mvm_config_umac_scan(sc)) != 0) {
			printf("%s: could not configure scan (error %d)\n",
			    DEVNAME(sc), error);
			goto error;
		}
	}

	/* Enable Tx queues. */
	for (ac = 0; ac < EDCA_NUM_AC; ac++) {
		error = iwm_enable_txq(sc, IWM_STATION_ID, ac,
		    iwm_mvm_ac_to_tx_fifo[ac]);
		if (error) {
			printf("%s: could not enable Tx queue %d (error %d)\n",
			    DEVNAME(sc), ac, error);
			goto error;
		}
	}

	/* Add the MAC context. */
	if ((error = iwm_mvm_mac_ctxt_add(sc, in)) != 0) {
		printf("%s: could not add MAC context (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
 	}

	if ((error = iwm_mvm_disable_beacon_filter(sc)) != 0) {
		printf("%s: could not disable beacon filter (error %d)\n",
		    DEVNAME(sc), error);
		goto error;
	}

	return 0;

 error:
	iwm_stop_device(sc);
	return error;
}

/* Allow multicast from our BSSID. */
int
iwm_allow_mcast(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwm_mcast_filter_cmd *cmd;
	size_t size;
	int error;

	size = roundup(sizeof(*cmd), 4);
	cmd = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cmd == NULL)
		return ENOMEM;
	cmd->filter_own = 1;
	cmd->port_id = 0;
	cmd->count = 0;
	cmd->pass_all = 1;
	IEEE80211_ADDR_COPY(cmd->bssid, ni->ni_bssid);

	error = iwm_mvm_send_cmd_pdu(sc, IWM_MCAST_FILTER_CMD,
	    IWM_CMD_SYNC, size, cmd);
	free(cmd, M_DEVBUF, size);
	return error;
}

/*
 * ifnet interfaces
 */

int
iwm_init(struct ifnet *ifp)
{
	struct iwm_softc *sc = ifp->if_softc;
	int error;

	if (sc->sc_flags & IWM_FLAG_HW_INITED) {
		return 0;
	}
	sc->sc_generation++;
	sc->sc_flags &= ~IWM_FLAG_STOPPED;

	if ((error = iwm_init_hw(sc)) != 0) {
		iwm_stop(ifp, 1);
		return error;
	}

	/*
 	 * Ok, firmware loaded and we are jogging
	 */

	ifq_clr_oactive(&ifp->if_snd);
	ifp->if_flags |= IFF_RUNNING;

	ieee80211_begin_scan(ifp);
	sc->sc_flags |= IWM_FLAG_HW_INITED;

	return 0;
}

/*
 * Dequeue packets from sendq and call send.
 * mostly from iwn
 */
void
iwm_start(struct ifnet *ifp)
{
	struct iwm_softc *sc = ifp->if_softc;
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
		if (ic->ic_state != IEEE80211_S_RUN) {
			break;
		}

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
		if (iwm_tx(sc, m, ni, ac) != 0) {
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
iwm_stop(struct ifnet *ifp, int disable)
{
	struct iwm_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	sc->sc_flags &= ~IWM_FLAG_HW_INITED;
	sc->sc_flags |= IWM_FLAG_STOPPED;
	sc->sc_generation++;
	ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	task_del(systq, &sc->init_task);
	task_del(sc->sc_nswq, &sc->newstate_task);
	task_del(sc->sc_eswq, &sc->sc_eswk);
	task_del(systq, &sc->setrates_task);
	task_del(systq, &sc->ba_task);
	task_del(systq, &sc->htprot_task);

	sc->sc_newstate(ic, IEEE80211_S_INIT, -1);

	timeout_del(&sc->sc_calib_to);
	iwm_led_blink_stop(sc);
	ifp->if_timer = sc->sc_tx_timer = 0;
	iwm_stop_device(sc);
}

void
iwm_watchdog(struct ifnet *ifp)
{
	struct iwm_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;
	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", DEVNAME(sc));
#ifdef IWM_DEBUG
			iwm_nic_error(sc);
#endif
			task_add(systq, &sc->init_task);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
iwm_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iwm_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();

	/*
	 * Prevent processes from entering this function while another
	 * process is tsleep'ing in it.
	 */
	while ((sc->sc_flags & IWM_FLAG_BUSY) && error == 0)
		error = tsleep(&sc->sc_flags, PCATCH, "iwmioc", 0);
	if (error != 0) {
		splx(s);
		return error;
	}
	sc->sc_flags |= IWM_FLAG_BUSY;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING)) {
				if ((error = iwm_init(ifp)) != 0)
					ifp->if_flags &= ~IFF_UP;
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iwm_stop(ifp, 1);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);
		if (error == ENETRESET)
			error = 0;
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		error = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			iwm_stop(ifp, 0);
			error = iwm_init(ifp);
		}
	}

	sc->sc_flags &= ~IWM_FLAG_BUSY;
	wakeup(&sc->sc_flags);
	splx(s);
	return error;
}

/*
 * The interrupt side of things
 */

#ifdef IWM_DEBUG
/*
 * Note: This structure is read from the device with IO accesses,
 * and the reading already does the endian conversion. As it is
 * read with uint32_t-sized accesses, any members with a different size
 * need to be ordered correctly though!
 */
struct iwm_error_event_table {
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
struct iwm_umac_error_event_table {
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
iwm_nic_umac_error(struct iwm_softc *sc)
{
	struct iwm_umac_error_event_table table;
	uint32_t base;

	base = sc->sc_uc.uc_umac_error_event_table;

	if (base < 0x800000) {
		printf("%s: Invalid error log pointer 0x%08x\n",
		    DEVNAME(sc), base);
		return;
	}

	if (iwm_read_mem(sc, base, &table, sizeof(table)/sizeof(uint32_t))) {
		printf("%s: reading errlog failed\n", DEVNAME(sc));
		return;
	}

	if (ERROR_START_OFFSET <= table.valid * ERROR_ELEM_SIZE) {
		printf("%s: Start UMAC Error Log Dump:\n", DEVNAME(sc));
		printf("%s: Status: 0x%x, count: %d\n", DEVNAME(sc),
			sc->sc_flags, table.valid);
	}

	printf("%s: 0x%08X | %s\n", DEVNAME(sc), table.error_id,
		iwm_desc_lookup(table.error_id));
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

struct {
	const char *name;
	uint8_t num;
} advanced_lookup[] = {
	{ "NMI_INTERRUPT_WDG", 0x34 },
	{ "SYSASSERT", 0x35 },
	{ "UCODE_VERSION_MISMATCH", 0x37 },
	{ "BAD_COMMAND", 0x38 },
	{ "NMI_INTERRUPT_DATA_ACTION_PT", 0x3C },
	{ "FATAL_ERROR", 0x3D },
	{ "NMI_TRM_HW_ERR", 0x46 },
	{ "NMI_INTERRUPT_TRM", 0x4C },
	{ "NMI_INTERRUPT_BREAK_POINT", 0x54 },
	{ "NMI_INTERRUPT_WDG_RXF_FULL", 0x5C },
	{ "NMI_INTERRUPT_WDG_NO_RBD_RXF_FULL", 0x64 },
	{ "NMI_INTERRUPT_HOST", 0x66 },
	{ "NMI_INTERRUPT_ACTION_PT", 0x7C },
	{ "NMI_INTERRUPT_UNKNOWN", 0x84 },
	{ "NMI_INTERRUPT_INST_ACTION_PT", 0x86 },
	{ "ADVANCED_SYSASSERT", 0 },
};

const char *
iwm_desc_lookup(uint32_t num)
{
	int i;

	for (i = 0; i < nitems(advanced_lookup) - 1; i++)
		if (advanced_lookup[i].num == num)
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
iwm_nic_error(struct iwm_softc *sc)
{
	struct iwm_error_event_table table;
	uint32_t base;

	printf("%s: dumping device error log\n", DEVNAME(sc));
	base = sc->sc_uc.uc_error_event_table;
	if (base < 0x800000) {
		printf("%s: Invalid error log pointer 0x%08x\n",
		    DEVNAME(sc), base);
		return;
	}

	if (iwm_read_mem(sc, base, &table, sizeof(table)/sizeof(uint32_t))) {
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
	    iwm_desc_lookup(table.error_id));
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
		iwm_nic_umac_error(sc);
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

#define ADVANCE_RXQ(sc) (sc->rxq.cur = (sc->rxq.cur + 1) % IWM_RX_RING_COUNT);

/*
 * Process an IWM_CSR_INT_BIT_FH_RX or IWM_CSR_INT_BIT_SW_RX interrupt.
 * Basic structure from if_iwn
 */
void
iwm_notif_intr(struct iwm_softc *sc)
{
	uint16_t hw;

	bus_dmamap_sync(sc->sc_dmat, sc->rxq.stat_dma.map,
	    0, sc->rxq.stat_dma.size, BUS_DMASYNC_POSTREAD);

	hw = le16toh(sc->rxq.stat->closed_rb_num) & 0xfff;
	while (sc->rxq.cur != hw) {
		struct iwm_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		struct iwm_rx_packet *pkt;
		struct iwm_cmd_response *cresp;
		int qid, idx, code;

		bus_dmamap_sync(sc->sc_dmat, data->map, 0, sizeof(*pkt),
		    BUS_DMASYNC_POSTREAD);
		pkt = mtod(data->m, struct iwm_rx_packet *);

		qid = pkt->hdr.qid & ~0x80;
		idx = pkt->hdr.idx;

		code = IWM_WIDE_ID(pkt->hdr.flags, pkt->hdr.code);
		DPRINTFN(12, ("rx packet qid=%d idx=%d type=%x %d %d\n",
		    pkt->hdr.qid & ~0x80, pkt->hdr.idx, code, sc->rxq.cur, hw));

		/*
		 * randomly get these from the firmware, no idea why.
		 * they at least seem harmless, so just ignore them for now
		 */
		if (__predict_false((pkt->hdr.code == 0 && qid == 0 && idx == 0)
		    || pkt->len_n_flags == htole32(0x55550000))) {
			ADVANCE_RXQ(sc);
			continue;
		}

		switch (code) {
		case IWM_REPLY_RX_PHY_CMD:
			iwm_mvm_rx_rx_phy_cmd(sc, pkt, data);
			break;

		case IWM_REPLY_RX_MPDU_CMD:
			iwm_mvm_rx_rx_mpdu(sc, pkt, data);
			break;

		case IWM_TX_CMD:
			iwm_mvm_rx_tx_cmd(sc, pkt, data);
			break;

		case IWM_MISSED_BEACONS_NOTIFICATION:
			/* OpenBSD does not provide ieee80211_beacon_miss() */
			break;

		case IWM_MFUART_LOAD_NOTIFICATION:
			break;

		case IWM_MVM_ALIVE: {
			struct iwm_mvm_alive_resp_v1 *resp1;
			struct iwm_mvm_alive_resp_v2 *resp2;
			struct iwm_mvm_alive_resp_v3 *resp3;

			if (iwm_rx_packet_payload_len(pkt) == sizeof(*resp1)) {
				SYNC_RESP_STRUCT(resp1, pkt);
				sc->sc_uc.uc_error_event_table
				    = le32toh(resp1->error_event_table_ptr);
				sc->sc_uc.uc_log_event_table
				    = le32toh(resp1->log_event_table_ptr);
				sc->sched_base = le32toh(resp1->scd_base_ptr);
				if (resp1->status == IWM_ALIVE_STATUS_OK)
					sc->sc_uc.uc_ok = 1;
				else
					sc->sc_uc.uc_ok = 0;
			}

			if (iwm_rx_packet_payload_len(pkt) == sizeof(*resp2)) {
				SYNC_RESP_STRUCT(resp2, pkt);
				sc->sc_uc.uc_error_event_table
				    = le32toh(resp2->error_event_table_ptr);
				sc->sc_uc.uc_log_event_table
				    = le32toh(resp2->log_event_table_ptr);
				sc->sched_base = le32toh(resp2->scd_base_ptr);
				sc->sc_uc.uc_umac_error_event_table
				    = le32toh(resp2->error_info_addr);
				if (resp2->status == IWM_ALIVE_STATUS_OK)
					sc->sc_uc.uc_ok = 1;
				else
					sc->sc_uc.uc_ok = 0;
			}

			if (iwm_rx_packet_payload_len(pkt) == sizeof(*resp3)) {
				SYNC_RESP_STRUCT(resp3, pkt);
				sc->sc_uc.uc_error_event_table
				    = le32toh(resp3->error_event_table_ptr);
				sc->sc_uc.uc_log_event_table
				    = le32toh(resp3->log_event_table_ptr);
				sc->sched_base = le32toh(resp3->scd_base_ptr);
				sc->sc_uc.uc_umac_error_event_table
				    = le32toh(resp3->error_info_addr);
				if (resp3->status == IWM_ALIVE_STATUS_OK)
					sc->sc_uc.uc_ok = 1;
				else
					sc->sc_uc.uc_ok = 0;
			}

			sc->sc_uc.uc_intr = 1;
			wakeup(&sc->sc_uc);
			break; }

		case IWM_CALIB_RES_NOTIF_PHY_DB: {
			struct iwm_calib_res_notif_phy_db *phy_db_notif;
			SYNC_RESP_STRUCT(phy_db_notif, pkt);

			iwm_phy_db_set_section(sc, phy_db_notif);

			break; }

		case IWM_STATISTICS_NOTIFICATION: {
			struct iwm_notif_statistics *stats;
			SYNC_RESP_STRUCT(stats, pkt);
			memcpy(&sc->sc_stats, stats, sizeof(sc->sc_stats));
			sc->sc_noise = iwm_get_noise(&stats->rx.general);
			break; }

		case IWM_NVM_ACCESS_CMD:
		case IWM_MCC_UPDATE_CMD:
			if (sc->sc_wantresp == ((qid << 16) | idx)) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    sizeof(sc->sc_cmd_resp),
				    BUS_DMASYNC_POSTREAD);
				memcpy(sc->sc_cmd_resp,
				    pkt, sizeof(sc->sc_cmd_resp));
			}
			break;

		case IWM_MCC_CHUB_UPDATE_CMD: {
			struct iwm_mcc_chub_notif *notif;
			SYNC_RESP_STRUCT(notif, pkt);

			sc->sc_fw_mcc[0] = (notif->mcc & 0xff00) >> 8;
			sc->sc_fw_mcc[1] = notif->mcc & 0xff;
			sc->sc_fw_mcc[2] = '\0';
			DPRINTF(("%s: fw source %d sent CC '%s'\n",
			    DEVNAME(sc), notif->source_id, sc->sc_fw_mcc));
		}

		case IWM_DTS_MEASUREMENT_NOTIFICATION:
			break;

		case IWM_PHY_CONFIGURATION_CMD:
		case IWM_TX_ANT_CONFIGURATION_CMD:
		case IWM_ADD_STA:
		case IWM_MAC_CONTEXT_CMD:
		case IWM_REPLY_SF_CFG_CMD:
		case IWM_POWER_TABLE_CMD:
		case IWM_PHY_CONTEXT_CMD:
		case IWM_BINDING_CONTEXT_CMD:
		case IWM_TIME_EVENT_CMD:
		case IWM_SCAN_REQUEST_CMD:
		case IWM_WIDE_ID(IWM_ALWAYS_LONG_GROUP, IWM_SCAN_CFG_CMD):
		case IWM_WIDE_ID(IWM_ALWAYS_LONG_GROUP, IWM_SCAN_REQ_UMAC):
		case IWM_SCAN_OFFLOAD_REQUEST_CMD:
		case IWM_REPLY_BEACON_FILTERING_CMD:
		case IWM_MAC_PM_POWER_TABLE:
		case IWM_TIME_QUOTA_CMD:
		case IWM_REMOVE_STA:
		case IWM_TXPATH_FLUSH:
		case IWM_LQ_CMD:
		case IWM_BT_CONFIG:
		case IWM_REPLY_THERMAL_MNG_BACKOFF:
			SYNC_RESP_STRUCT(cresp, pkt);
			if (sc->sc_wantresp == ((qid << 16) | idx)) {
				memcpy(sc->sc_cmd_resp,
				    pkt, sizeof(*pkt)+sizeof(*cresp));
			}
			break;

		/* ignore */
		case 0x6c: /* IWM_PHY_DB_CMD */
			break;

		case IWM_INIT_COMPLETE_NOTIF:
			sc->sc_init_complete = 1;
			wakeup(&sc->sc_init_complete);
			break;

		case IWM_SCAN_OFFLOAD_COMPLETE: {
			struct iwm_periodic_scan_complete *notif;
			SYNC_RESP_STRUCT(notif, pkt);

			break; }

		case IWM_SCAN_ITERATION_COMPLETE: {
			struct iwm_lmac_scan_complete_notif *notif;
			SYNC_RESP_STRUCT(notif, pkt);

			task_add(sc->sc_eswq, &sc->sc_eswk);
			break; }

		case IWM_SCAN_COMPLETE_UMAC: {
			struct iwm_umac_scan_complete *notif;
			SYNC_RESP_STRUCT(notif, pkt);

			DPRINTF(("UMAC scan complete, status=0x%x\n",
			    notif->status));
			task_add(sc->sc_eswq, &sc->sc_eswk);
			break;
		}

		case IWM_SCAN_ITERATION_COMPLETE_UMAC: {
			struct iwm_umac_scan_iter_complete_notif *notif;
			SYNC_RESP_STRUCT(notif, pkt);

			DPRINTF(("UMAC scan iteration complete, status=0x%x, "
			"%d channels scanned\n",
			    notif->status, notif->scanned_channels));
			task_add(sc->sc_eswq, &sc->sc_eswk);
			break;
		}

		case IWM_REPLY_ERROR: {
			struct iwm_error_resp *resp;
			SYNC_RESP_STRUCT(resp, pkt);

			printf("%s: firmware error 0x%x, cmd 0x%x\n",
				DEVNAME(sc), le32toh(resp->error_type),
				resp->cmd_id);
			break; }

		case IWM_TIME_EVENT_NOTIFICATION: {
			struct iwm_time_event_notif *notif;
			SYNC_RESP_STRUCT(notif, pkt);

			DPRINTF(("%s: TE notif status = 0x%x action = 0x%x\n",
			    DEVNAME(sc), notif->status, notif->action));
			break; }

		case IWM_MCAST_FILTER_CMD:
			break;

		case IWM_SCD_QUEUE_CFG: {
			struct iwm_scd_txq_cfg_rsp *rsp;
			SYNC_RESP_STRUCT(rsp, pkt);
			
			DPRINTF(("%s: queue cfg token=0x%x sta_id=%d "
			    "tid=%d scd_queue=%d\n",
			    DEVNAME(sc), rsp->token, rsp->sta_id, rsp->tid,
			    rsp->scd_queue));
			break;
		}

		default:
			printf("%s: unhandled firmware response 0x%x/0x%x "
			    "rx ring %d[%d]\n",
			    DEVNAME(sc), pkt->hdr.code, pkt->len_n_flags, qid,
			    idx);
			break;
		}

		/*
		 * Why test bit 0x80?  The Linux driver:
		 *
		 * There is one exception:  uCode sets bit 15 when it
		 * originates the response/notification, i.e. when the
		 * response/notification is not a direct response to a
		 * command sent by the driver.  For example, uCode issues
		 * IWM_REPLY_RX when it sends a received frame to the driver;
		 * it is not a direct response to any driver command.
		 *
		 * Ok, so since when is 7 == 15?  Well, the Linux driver
		 * uses a slightly different format for pkt->hdr, and "qid"
		 * is actually the upper byte of a two-byte field.
		 */
		if (!(pkt->hdr.qid & (1 << 7))) {
			iwm_cmd_done(sc, pkt);
		}

		ADVANCE_RXQ(sc);
	}

	IWM_CLRBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	/*
	 * Tell the firmware what we have processed.
	 * Seems like the hardware gets upset unless we align
	 * the write by 8??
	 */
	hw = (hw == 0) ? IWM_RX_RING_COUNT - 1 : hw - 1;
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_WPTR, hw & ~7);
}

int
iwm_intr(void *arg)
{
	struct iwm_softc *sc = arg;
	struct ifnet *ifp = IC2IFP(&sc->sc_ic);
	int handled = 0;
	int r1, r2, rv = 0;
	int isperiodic = 0;

	IWM_WRITE(sc, IWM_CSR_INT_MASK, 0);

	if (sc->sc_flags & IWM_FLAG_USE_ICT) {
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
			sc->ict_cur = (sc->ict_cur+1) % IWM_ICT_COUNT;
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
		r1 = IWM_READ(sc, IWM_CSR_INT);
		/* "hardware gone" (where, fishing?) */
		if (r1 == 0xffffffff || (r1 & 0xfffffff0) == 0xa5a5a5a0)
			goto out;
		r2 = IWM_READ(sc, IWM_CSR_FH_INT_STATUS);
	}
	if (r1 == 0 && r2 == 0) {
		goto out_ena;
	}

	IWM_WRITE(sc, IWM_CSR_INT, r1 | ~sc->sc_intmask);

	/* ignored */
	handled |= (r1 & (IWM_CSR_INT_BIT_ALIVE /*| IWM_CSR_INT_BIT_SCD*/));

	if (r1 & IWM_CSR_INT_BIT_SW_ERR) {
#ifdef IWM_DEBUG
		int i;

		iwm_nic_error(sc);

		/* Dump driver status (TX and RX rings) while we're here. */
		DPRINTF(("driver status:\n"));
		for (i = 0; i < IWM_MVM_MAX_QUEUES; i++) {
			struct iwm_tx_ring *ring = &sc->txq[i];
			DPRINTF(("  tx ring %2d: qid=%-2d cur=%-3d "
			    "queued=%-3d\n",
			    i, ring->qid, ring->cur, ring->queued));
		}
		DPRINTF(("  rx ring: cur=%d\n", sc->rxq.cur));
		DPRINTF(("  802.11 state %s\n",
		    ieee80211_state_name[sc->sc_ic.ic_state]));
#endif

		printf("%s: fatal firmware error\n", DEVNAME(sc));
		ifp->if_flags &= ~IFF_UP;
		iwm_stop(ifp, 1);
		rv = 1;
		goto out;

	}

	if (r1 & IWM_CSR_INT_BIT_HW_ERR) {
		handled |= IWM_CSR_INT_BIT_HW_ERR;
		printf("%s: hardware error, stopping device \n", DEVNAME(sc));
		ifp->if_flags &= ~IFF_UP;
		iwm_stop(ifp, 1);
		rv = 1;
		goto out;
	}

	/* firmware chunk loaded */
	if (r1 & IWM_CSR_INT_BIT_FH_TX) {
		IWM_WRITE(sc, IWM_CSR_FH_INT_STATUS, IWM_CSR_FH_INT_TX_MASK);
		handled |= IWM_CSR_INT_BIT_FH_TX;

		sc->sc_fw_chunk_done = 1;
		wakeup(&sc->sc_fw);
	}

	if (r1 & IWM_CSR_INT_BIT_RF_KILL) {
		handled |= IWM_CSR_INT_BIT_RF_KILL;
		if (iwm_check_rfkill(sc) && (ifp->if_flags & IFF_UP)) {
			DPRINTF(("%s: rfkill switch, disabling interface\n",
			    DEVNAME(sc)));
			ifp->if_flags &= ~IFF_UP;
			iwm_stop(ifp, 1);
		}
	}

	/*
	 * The Linux driver uses periodic interrupts to avoid races.
	 * We cargo-cult like it's going out of fashion.
	 */
	if (r1 & IWM_CSR_INT_BIT_RX_PERIODIC) {
		handled |= IWM_CSR_INT_BIT_RX_PERIODIC;
		IWM_WRITE(sc, IWM_CSR_INT, IWM_CSR_INT_BIT_RX_PERIODIC);
		if ((r1 & (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX)) == 0)
			IWM_WRITE_1(sc,
			    IWM_CSR_INT_PERIODIC_REG, IWM_CSR_INT_PERIODIC_DIS);
		isperiodic = 1;
	}

	if ((r1 & (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX)) || isperiodic) {
		handled |= (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX);
		IWM_WRITE(sc, IWM_CSR_FH_INT_STATUS, IWM_CSR_FH_INT_RX_MASK);

		iwm_notif_intr(sc);

		/* enable periodic interrupt, see above */
		if (r1 & (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX) && !isperiodic)
			IWM_WRITE_1(sc, IWM_CSR_INT_PERIODIC_REG,
			    IWM_CSR_INT_PERIODIC_ENA);
	}

	if (__predict_false(r1 & ~handled))
		DPRINTF(("%s: unhandled interrupts: %x\n", DEVNAME(sc), r1));
	rv = 1;

 out_ena:
	iwm_restore_interrupts(sc);
 out:
	return rv;
}

/*
 * Autoconf glue-sniffing
 */

typedef void *iwm_match_t;

static const struct pci_matchid iwm_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_3160_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_3160_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_3165_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_3165_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_7260_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_7260_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_7265_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_7265_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_8260_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WL_8260_2 },
};

int
iwm_match(struct device *parent, iwm_match_t match __unused, void *aux)
{
	return pci_matchbyid((struct pci_attach_args *)aux, iwm_devices,
	    nitems(iwm_devices));
}

int
iwm_preinit(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	int error;
	static int attached;

	if ((error = iwm_prepare_card_hw(sc)) != 0) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return error;
	}

	if (attached) {
		/* Update MAC in case the upper layers changed it. */
		IEEE80211_ADDR_COPY(sc->sc_ic.ic_myaddr,
		    ((struct arpcom *)ifp)->ac_enaddr);
		return 0;
	}

	if ((error = iwm_start_hw(sc)) != 0) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return error;
	}

	error = iwm_run_init_mvm_ucode(sc, 1);
	iwm_stop_device(sc);
	if (error)
		return error;

	/* Print version info and MAC address on first successful fw load. */
	attached = 1;
	printf("%s: hw rev 0x%x, fw ver %s, address %s\n",
	    DEVNAME(sc), sc->sc_hw_rev & IWM_CSR_HW_REV_TYPE_MSK,
	    sc->sc_fwver, ether_sprintf(sc->sc_nvm.hw_addr));

	if (sc->sc_nvm.sku_cap_11n_enable)
		iwm_setup_ht_rates(sc);

	/* not all hardware can do 5GHz band */
	if (!sc->sc_nvm.sku_cap_band_52GHz_enable)
		memset(&ic->ic_sup_rates[IEEE80211_MODE_11A], 0,
		    sizeof(ic->ic_sup_rates[IEEE80211_MODE_11A]));

	/* Configure channel information obtained from firmware. */
	ieee80211_channel_init(ifp);

	/* Configure MAC address. */
	error = if_setlladdr(ifp, ic->ic_myaddr);
	if (error)
		printf("%s: could not set MAC address (error %d)\n",
		    DEVNAME(sc), error);

	ieee80211_media_init(ifp, iwm_media_change, ieee80211_media_status);

	return 0;
}

void
iwm_attach_hook(struct device *self)
{
	struct iwm_softc *sc = (void *)self;

	KASSERT(!cold);

	iwm_preinit(sc);
}

void
iwm_attach(struct device *parent, struct device *self, void *aux)
{
	struct iwm_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	pcireg_t reg, memtype;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	const char *intrstr;
	int error;
	int txq_i, i;

	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	task_set(&sc->sc_eswk, iwm_endscan_cb, sc);

	/*
	 * Get the offset of the PCI Express Capability Structure in PCI
	 * Configuration Space.
	 */
	error = pci_get_capability(sc->sc_pct, sc->sc_pcitag,
	    PCI_CAP_PCIEXPRESS, &sc->sc_cap_off, NULL);
	if (error == 0) {
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
	error = pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &sc->sc_sz, 0);
	if (error != 0) {
		printf("%s: can't map mem space\n", DEVNAME(sc));
		return;
	}

	/* Install interrupt handler. */
	if (pci_intr_map_msi(pa, &ih) && pci_intr_map(pa, &ih)) {
		printf("%s: can't map interrupt\n", DEVNAME(sc));
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih);
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, iwm_intr, sc,
	    DEVNAME(sc));

	if (sc->sc_ih == NULL) {
		printf("\n");
		printf("%s: can't establish interrupt", DEVNAME(sc));
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(", %s\n", intrstr);

	sc->sc_wantresp = IWM_CMD_RESP_IDLE;

	sc->sc_hw_rev = IWM_READ(sc, IWM_CSR_HW_REV);
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_WL_3160_1:
	case PCI_PRODUCT_INTEL_WL_3160_2:
		sc->sc_fwname = "iwm-3160-16";
		sc->host_interrupt_operation_mode = 1;
		sc->sc_device_family = IWM_DEVICE_FAMILY_7000;
		sc->sc_fwdmasegsz = IWM_FWDMASEGSZ;
		break;
	case PCI_PRODUCT_INTEL_WL_3165_1:
	case PCI_PRODUCT_INTEL_WL_3165_2:
		sc->sc_fwname = "iwm-7265-16";
		sc->host_interrupt_operation_mode = 0;
		sc->sc_device_family = IWM_DEVICE_FAMILY_7000;
		sc->sc_fwdmasegsz = IWM_FWDMASEGSZ;
		break;
	case PCI_PRODUCT_INTEL_WL_7260_1:
	case PCI_PRODUCT_INTEL_WL_7260_2:
		sc->sc_fwname = "iwm-7260-16";
		sc->host_interrupt_operation_mode = 1;
		sc->sc_device_family = IWM_DEVICE_FAMILY_7000;
		sc->sc_fwdmasegsz = IWM_FWDMASEGSZ;
		break;
	case PCI_PRODUCT_INTEL_WL_7265_1:
	case PCI_PRODUCT_INTEL_WL_7265_2:
		sc->sc_fwname = "iwm-7265-16";
		sc->host_interrupt_operation_mode = 0;
		sc->sc_device_family = IWM_DEVICE_FAMILY_7000;
		sc->sc_fwdmasegsz = IWM_FWDMASEGSZ;
		break;
	case PCI_PRODUCT_INTEL_WL_8260_1:
	case PCI_PRODUCT_INTEL_WL_8260_2:
		sc->sc_fwname = "iwm-8000C-16";
		sc->host_interrupt_operation_mode = 0;
		sc->sc_device_family = IWM_DEVICE_FAMILY_8000;
		sc->sc_fwdmasegsz = IWM_FWDMASEGSZ_8000;
		break;
	default:
		printf("%s: unknown adapter type\n", DEVNAME(sc));
		return;
	}

	/*
	 * We now start fiddling with the hardware
	 */

	/*
	 * In the 8000 HW family the format of the 4 bytes of CSR_HW_REV have
	 * changed, and now the revision step also includes bit 0-1 (no more
	 * "dash" value). To keep hw_rev backwards compatible - we'll store it
	 * in the old format.
	 */
	if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000)
		sc->sc_hw_rev = (sc->sc_hw_rev & 0xfff0) |
				(IWM_CSR_HW_REV_STEP(sc->sc_hw_rev << 2) << 2);
		
	if (iwm_prepare_card_hw(sc) != 0) {
		printf("%s: could not initialize hardware\n", DEVNAME(sc));
		return;
	}

	if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000) {
		int ret;
		uint32_t hw_step;

		/*
		 * In order to recognize C step the driver should read the
		 * chip version id located at the AUX bus MISC address.
		 */
		IWM_SETBITS(sc, IWM_CSR_GP_CNTRL,
			    IWM_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);
		DELAY(2);

		ret = iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
				   IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
				   IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
				   25000);
		if (ret < 0) {
			printf("%s: Failed to wake up the nic\n", DEVNAME(sc));
			return;
		}

		if (iwm_nic_lock(sc)) {
			hw_step = iwm_read_prph(sc, IWM_WFPM_CTRL_REG);
			hw_step |= IWM_ENABLE_WFPM;
			iwm_write_prph(sc, IWM_WFPM_CTRL_REG, hw_step);
			hw_step = iwm_read_prph(sc, IWM_AUX_MISC_REG);
			hw_step = (hw_step >> IWM_HW_STEP_LOCATION_BITS) & 0xF;
			if (hw_step == 0x3)
				sc->sc_hw_rev = (sc->sc_hw_rev & 0xFFFFFFF3) |
						(IWM_SILICON_C_STEP << 2);
			iwm_nic_unlock(sc);
		} else {
			printf("%s: Failed to lock the nic\n", DEVNAME(sc));
			return;
		}
	}

	/* Allocate DMA memory for firmware transfers. */
	if ((error = iwm_alloc_fwmem(sc)) != 0) {
		printf("%s: could not allocate memory for firmware\n",
		    DEVNAME(sc));
		return;
	}

	/* Allocate "Keep Warm" page. */
	if ((error = iwm_alloc_kw(sc)) != 0) {
		printf("%s: could not allocate keep warm page\n", DEVNAME(sc));
		goto fail1;
	}

	/* We use ICT interrupts */
	if ((error = iwm_alloc_ict(sc)) != 0) {
		printf("%s: could not allocate ICT table\n", DEVNAME(sc));
		goto fail2;
	}

	/* Allocate TX scheduler "rings". */
	if ((error = iwm_alloc_sched(sc)) != 0) {
		printf("%s: could not allocate TX scheduler rings\n",
		    DEVNAME(sc));
		goto fail3;
	}

	/* Allocate TX rings */
	for (txq_i = 0; txq_i < nitems(sc->txq); txq_i++) {
		if ((error = iwm_alloc_tx_ring(sc,
		    &sc->txq[txq_i], txq_i)) != 0) {
			printf("%s: could not allocate TX ring %d\n",
			    DEVNAME(sc), txq_i);
			goto fail4;
		}
	}

	/* Allocate RX ring. */
	if ((error = iwm_alloc_rx_ring(sc, &sc->rxq)) != 0) {
		printf("%s: could not allocate RX ring\n", DEVNAME(sc));
		goto fail4;
	}

	sc->sc_eswq = taskq_create("iwmes", 1, IPL_NET, 0);
	if (sc->sc_eswq == NULL)
		goto fail4;
	sc->sc_nswq = taskq_create("iwmns", 1, IPL_NET, 0);
	if (sc->sc_nswq == NULL)
		goto fail4;

	/* Clear pending interrupts. */
	IWM_WRITE(sc, IWM_CSR_INT, 0xffffffff);

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_WEP |		/* WEP */
	    IEEE80211_C_RSN |		/* WPA/RSN */
	    IEEE80211_C_SCANALL |	/* device scans all channels at once */
	    IEEE80211_C_SCANALLBAND |	/* device scans all bands at once */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE;	/* short preamble supported */

	/* No optional HT features supported for now, */
	ic->ic_htcaps = 0;
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

	/* Max RSSI */
	ic->ic_max_rssi = IWM_MAX_DBM - IWM_MIN_DBM;

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = iwm_ioctl;
	ifp->if_start = iwm_start;
	ifp->if_watchdog = iwm_watchdog;
	memcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ieee80211_media_init(ifp, iwm_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	iwm_radiotap_attach(sc);
#endif
	timeout_set(&sc->sc_calib_to, iwm_calib_timeout, sc);
	timeout_set(&sc->sc_led_blink_to, iwm_led_blink_timeout, sc);
	task_set(&sc->init_task, iwm_init_task, sc);
	task_set(&sc->newstate_task, iwm_newstate_task, sc);
	task_set(&sc->setrates_task, iwm_setrates_task, sc);
	task_set(&sc->ba_task, iwm_ba_task, sc);
	task_set(&sc->htprot_task, iwm_htprot_task, sc);

	ic->ic_node_alloc = iwm_node_alloc;

	/* Override 802.11 state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwm_newstate;
	ic->ic_update_htprot = iwm_update_htprot;
	ic->ic_ampdu_rx_start = iwm_ampdu_rx_start;
	ic->ic_ampdu_rx_stop = iwm_ampdu_rx_stop;
#ifdef notyet
	ic->ic_ampdu_tx_start = iwm_ampdu_tx_start;
	ic->ic_ampdu_tx_stop = iwm_ampdu_tx_stop;
#endif
	/*
	 * We cannot read the MAC address without loading the
	 * firmware from disk. Postpone until mountroot is done.
	 */
	config_mountroot(self, iwm_attach_hook);

	return;

	/* Free allocated memory if something failed during attachment. */
fail4:	while (--txq_i >= 0)
		iwm_free_tx_ring(sc, &sc->txq[txq_i]);
	iwm_free_rx_ring(sc, &sc->rxq);
	iwm_free_sched(sc);
fail3:	if (sc->ict_dma.vaddr != NULL)
		iwm_free_ict(sc);
fail2:	iwm_free_kw(sc);
fail1:	iwm_free_fwmem(sc);
	return;
}

#if NBPFILTER > 0
/*
 * Attach the interface to 802.11 radiotap.
 */
void
iwm_radiotap_attach(struct iwm_softc *sc)
{
	bpfattach(&sc->sc_drvbpf, &sc->sc_ic.ic_if, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWM_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWM_TX_RADIOTAP_PRESENT);
}
#endif

void
iwm_init_task(void *arg1)
{
	struct iwm_softc *sc = arg1;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splnet();
	while (sc->sc_flags & IWM_FLAG_BUSY)
		tsleep(&sc->sc_flags, 0, "iwmpwr", 0);
	sc->sc_flags |= IWM_FLAG_BUSY;

	iwm_stop(ifp, 0);
	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP)
		iwm_init(ifp);

	sc->sc_flags &= ~IWM_FLAG_BUSY;
	wakeup(&sc->sc_flags);
	splx(s);
}

void
iwm_wakeup(struct iwm_softc *sc)
{
	pcireg_t reg;

	/* Clear device-specific "PCI retry timeout" register (41h). */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, reg & ~0xff00);

	task_add(systq, &sc->init_task);
}

int
iwm_activate(struct device *self, int act)
{
	struct iwm_softc *sc = (struct iwm_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	switch (act) {
	case DVACT_SUSPEND:
		if (ifp->if_flags & IFF_RUNNING)
			iwm_stop(ifp, 0);
		break;
	case DVACT_WAKEUP:
		iwm_wakeup(sc);
		break;
	}

	return 0;
}

struct cfdriver iwm_cd = {
	NULL, "iwm", DV_IFNET
};

struct cfattach iwm_ca = {
	sizeof(struct iwm_softc), iwm_match, iwm_attach,
	NULL, iwm_activate
};
