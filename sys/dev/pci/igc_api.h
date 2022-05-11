/*	$OpenBSD: igc_api.h,v 1.2 2022/05/11 06:14:15 kevlo Exp $	*/
/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef _IGC_API_H_
#define _IGC_API_H_

#include <dev/pci/if_igc.h>
#include <dev/pci/igc_hw.h>

extern void	igc_init_function_pointers_i225(struct igc_hw *);

int		igc_set_mac_type(struct igc_hw *);
int		igc_setup_init_funcs(struct igc_hw *, bool);
int		igc_init_mac_params(struct igc_hw *);
int		igc_init_nvm_params(struct igc_hw *);
int		igc_init_phy_params(struct igc_hw *);
int		igc_get_bus_info(struct igc_hw *);
void		igc_clear_vfta(struct igc_hw *);
void		igc_write_vfta(struct igc_hw *, uint32_t, uint32_t);
int		igc_force_mac_fc(struct igc_hw *);
int		igc_check_for_link(struct igc_hw *);
int		igc_reset_hw(struct igc_hw *);
int		igc_init_hw(struct igc_hw *);
int		igc_setup_link(struct igc_hw *);
int		igc_get_speed_and_duplex(struct igc_hw *, uint16_t *,
		    uint16_t *);
int		igc_disable_pcie_master(struct igc_hw *);
void		igc_config_collision_dist(struct igc_hw *);
int		igc_rar_set(struct igc_hw *, uint8_t *, uint32_t);
uint32_t	igc_hash_mc_addr(struct igc_hw *, uint8_t *);
void		igc_update_mc_addr_list(struct igc_hw *, uint8_t *, uint32_t);
int		igc_check_reset_block(struct igc_hw *);
int		igc_get_cable_length(struct igc_hw *);
int		igc_validate_mdi_setting(struct igc_hw *);
int		igc_get_phy_info(struct igc_hw *);
int		igc_phy_hw_reset(struct igc_hw *);
void		igc_power_up_phy(struct igc_hw *);
void		igc_power_down_phy(struct igc_hw *);
int		igc_read_mac_addr(struct igc_hw *);
int		igc_read_pba_string(struct igc_hw *, uint8_t *, uint32_t);
void		igc_reload_nvm(struct igc_hw *);
int		igc_validate_nvm_checksum(struct igc_hw *);
int		igc_read_nvm(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
int		igc_write_nvm(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
int		igc_set_d3_lplu_state(struct igc_hw *, bool);
int		igc_set_d0_lplu_state(struct igc_hw *, bool);

#endif /* _IGC_API_H_ */
