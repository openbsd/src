/*	$OpenBSD: igc_nvm.h,v 1.1 2021/10/31 14:52:57 patrick Exp $	*/
/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef _IGC_NVM_H_
#define _IGC_NVM_H_

void	igc_init_nvm_ops_generic(struct igc_hw *);
int	igc_null_read_nvm(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
void	igc_null_nvm_generic(struct igc_hw *);
int	igc_null_led_default(struct igc_hw *, uint16_t *);
int	igc_null_write_nvm(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
int	igc_poll_eerd_eewr_done(struct igc_hw *, int);
int	igc_read_mac_addr_generic(struct igc_hw *);
int	igc_read_pba_string_generic(struct igc_hw *, uint8_t *, uint32_t);
int	igc_read_nvm_eerd(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
int	igc_valid_led_default_generic(struct igc_hw *, uint16_t *);
int	igc_validate_nvm_checksum_generic(struct igc_hw *);
int	igc_write_nvm_spi(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
int	igc_update_nvm_checksum_generic(struct igc_hw *);
void	igc_release_nvm_generic(struct igc_hw *);
void	igc_reload_nvm_generic(struct igc_hw *);

#endif	/* _IGC_NVM_H_ */
