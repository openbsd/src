/*	$OpenBSD: igc_mac.h,v 1.1 2021/10/31 14:52:57 patrick Exp $	*/
/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef _IGC_MAC_H_
#define _IGC_MAC_H_

void	igc_init_mac_ops_generic(struct igc_hw *);
int	igc_null_ops_generic(struct igc_hw *);
int	igc_null_link_info(struct igc_hw *, uint16_t *, uint16_t *);
bool	igc_null_mng_mode(struct igc_hw *);
void	igc_null_update_mc(struct igc_hw *, uint8_t *h, uint32_t);
void	igc_null_write_vfta(struct igc_hw *, uint32_t a, uint32_t);
int	igc_check_for_copper_link_generic(struct igc_hw *);
int	igc_config_fc_after_link_up_generic(struct igc_hw *);
int	igc_disable_pcie_master_generic(struct igc_hw *);
int	igc_force_mac_fc_generic(struct igc_hw *);
int	igc_get_auto_rd_done_generic(struct igc_hw *);
int	igc_get_bus_info_pcie_generic(struct igc_hw *);
void	igc_set_lan_id_single_port(struct igc_hw *);
int	igc_get_speed_and_duplex_copper_generic(struct igc_hw *, uint16_t *,
 	    uint16_t *);
void	igc_update_mc_addr_list_generic(struct igc_hw *, uint8_t *, uint32_t);
int	igc_rar_set_generic(struct igc_hw *, uint8_t *, uint32_t);
int	igc_set_fc_watermarks_generic(struct igc_hw *);
int	igc_setup_link_generic(struct igc_hw *);
int	igc_validate_mdi_setting_crossover_generic(struct igc_hw *);

int	igc_hash_mc_addr_generic(struct igc_hw *, uint8_t *);

void	igc_clear_hw_cntrs_base_generic(struct igc_hw *);
void	igc_clear_vfta_generic(struct igc_hw *);
void	igc_init_rx_addrs_generic(struct igc_hw *, uint16_t);
void	igc_pcix_mmrbc_workaround_generic(struct igc_hw *);
void	igc_put_hw_semaphore_generic(struct igc_hw *);
int	igc_check_alt_mac_addr_generic(struct igc_hw *);
void	igc_set_pcie_no_snoop_generic(struct igc_hw *, uint32_t);
void	igc_write_vfta_generic(struct igc_hw *, uint32_t, uint32_t);
void	igc_config_collision_dist_generic(struct igc_hw *);

#endif	/* _IGC_MAC_H_ */
