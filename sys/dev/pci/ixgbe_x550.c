/*	$OpenBSD: ixgbe_x550.c,v 1.2 2016/11/18 11:25:11 mikeb Exp $	*/

/******************************************************************************

  Copyright (c) 2001-2015, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD: head/sys/dev/ixgbe/ixgbe_x550.c 295093 2016-01-31 15:14:23Z smh $*/

#include <dev/pci/ixgbe.h>
#include <dev/pci/ixgbe_type.h>

extern int32_t ixgbe_init_eeprom_params_X540(struct ixgbe_hw *hw);
extern int32_t ixgbe_acquire_swfw_sync_X540(struct ixgbe_hw *hw, uint32_t mask);
extern void ixgbe_release_swfw_sync_X540(struct ixgbe_hw *hw, uint32_t mask);

int32_t ixgbe_get_bus_info_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_init_eeprom_params_X550(struct ixgbe_hw *hw);
int32_t ixgbe_update_eeprom_checksum_X550(struct ixgbe_hw *hw);
int32_t ixgbe_calc_eeprom_checksum_X550(struct ixgbe_hw *hw);
int32_t ixgbe_calc_checksum_X550(struct ixgbe_hw *hw, uint16_t *buffer,
				 uint32_t buffer_size);
int32_t ixgbe_validate_eeprom_checksum_X550(struct ixgbe_hw *hw,
					    uint16_t *checksum_val);
int32_t ixgbe_update_flash_X550(struct ixgbe_hw *hw);
int32_t ixgbe_write_ee_hostif_X550(struct ixgbe_hw *hw, uint16_t offset,
			       uint16_t data);
int32_t ixgbe_read_ee_hostif_buffer_X550(struct ixgbe_hw *hw,
					 uint16_t offset, uint16_t words,
					 uint16_t *data);
int32_t ixgbe_read_ee_hostif_X550(struct ixgbe_hw *hw, uint16_t offset,
				  uint16_t *data);
int32_t ixgbe_read_ee_hostif_data_X550(struct ixgbe_hw *hw, uint16_t offset,
				       uint16_t *data);
int32_t ixgbe_write_ee_hostif_data_X550(struct ixgbe_hw *hw, uint16_t offset,
					uint16_t data);
int32_t ixgbe_write_iosf_sb_reg_x550(struct ixgbe_hw *hw, uint32_t reg_addr,
				     uint32_t device_type, uint32_t data);
int32_t ixgbe_read_iosf_sb_reg_x550(struct ixgbe_hw *hw, uint32_t reg_addr,
				    uint32_t device_type, uint32_t *data);
enum ixgbe_media_type ixgbe_get_media_type_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_setup_sfp_modules_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_get_link_capabilities_X550em(struct ixgbe_hw *hw,
					   ixgbe_link_speed *speed,
					   bool *autoneg);
void ixgbe_init_mac_link_ops_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_reset_hw_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_init_phy_ops_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_setup_kr_x550em(struct ixgbe_hw *hw);
int32_t ixgbe_init_ext_t_x550em(struct ixgbe_hw *hw);
int32_t ixgbe_setup_internal_phy_t_x550em(struct ixgbe_hw *hw);
int32_t ixgbe_setup_phy_loopback_x550em(struct ixgbe_hw *hw);
uint32_t ixgbe_get_supported_physical_layer_X550em(struct ixgbe_hw *hw);
void ixgbe_disable_rx_x550(struct ixgbe_hw *hw);
int32_t ixgbe_get_lcd_t_x550em(struct ixgbe_hw *hw, ixgbe_link_speed *lcd_speed);
int32_t ixgbe_acquire_swfw_sync_X550em(struct ixgbe_hw *hw, uint32_t mask);
void ixgbe_release_swfw_sync_X550em(struct ixgbe_hw *hw, uint32_t mask);
int32_t ixgbe_setup_fc_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_setup_mac_link_sfp_x550em(struct ixgbe_hw *hw,
					ixgbe_link_speed speed,
					bool autoneg_wait_to_complete);
int32_t ixgbe_handle_lasi_ext_t_x550em(struct ixgbe_hw *hw);
int32_t ixgbe_setup_mac_link_t_X550em(struct ixgbe_hw *hw,
				      ixgbe_link_speed speed,
				      bool autoneg_wait_to_complete);
int32_t ixgbe_check_link_t_X550em(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
				  bool *link_up, bool link_up_wait_to_complete);
int32_t ixgbe_reset_phy_t_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_identify_sfp_module_X550em(struct ixgbe_hw *hw);
int32_t ixgbe_led_on_t_X550em(struct ixgbe_hw *hw, uint32_t led_idx);
int32_t ixgbe_led_off_t_X550em(struct ixgbe_hw *hw, uint32_t led_idx);

int32_t ixgbe_setup_ixfi_x550em(struct ixgbe_hw *hw, ixgbe_link_speed *speed);

/**
 *  ixgbe_init_ops_X550 - Inits func ptrs and MAC type
 *  @hw: pointer to hardware structure
 *
 *  Initialize the function pointers and assign the MAC type for X550.
 *  Does not touch the hardware.
 **/
int32_t ixgbe_init_ops_X550(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	int32_t ret_val;

	DEBUGFUNC("ixgbe_init_ops_X550");

	ret_val = ixgbe_init_ops_X540(hw);

	eeprom->ops.init_params = ixgbe_init_eeprom_params_X550;
	eeprom->ops.calc_checksum = ixgbe_calc_eeprom_checksum_X550;
	eeprom->ops.read = ixgbe_read_ee_hostif_X550;
	eeprom->ops.write = ixgbe_write_ee_hostif_X550;
	eeprom->ops.update_checksum = ixgbe_update_eeprom_checksum_X550;
	eeprom->ops.validate_checksum = ixgbe_validate_eeprom_checksum_X550;

	mac->ops.disable_rx = ixgbe_disable_rx_x550;
	if (hw->device_id == IXGBE_DEV_ID_X550EM_X_10G_T) {
		hw->mac.ops.led_on = ixgbe_led_on_t_X550em;
		hw->mac.ops.led_off = ixgbe_led_off_t_X550em;
	}
	return ret_val;
}

/**
 * ixgbe_read_cs4227 - Read CS4227 register
 * @hw: pointer to hardware structure
 * @reg: register number to write
 * @value: pointer to receive value read
 *
 * Returns status code
 **/
int32_t ixgbe_read_cs4227(struct ixgbe_hw *hw, uint16_t reg, uint16_t *value)
{
	return hw->phy.ops.read_i2c_combined_unlocked(hw, IXGBE_CS4227, reg,
						      value);
}

/**
 * ixgbe_write_cs4227 - Write CS4227 register
 * @hw: pointer to hardware structure
 * @reg: register number to write
 * @value: value to write to register
 *
 * Returns status code
 **/
int32_t ixgbe_write_cs4227(struct ixgbe_hw *hw, uint16_t reg, uint16_t value)
{
	return hw->phy.ops.write_i2c_combined_unlocked(hw, IXGBE_CS4227, reg,
						       value);
}

/**
 * ixgbe_read_pe - Read register from port expander
 * @hw: pointer to hardware structure
 * @reg: register number to read
 * @value: pointer to receive read value
 *
 * Returns status code
 **/
int32_t ixgbe_read_pe(struct ixgbe_hw *hw, uint8_t reg, uint8_t *value)
{
	int32_t status;

	status = hw->phy.ops.read_i2c_byte_unlocked(hw, reg, IXGBE_PE, value);
	if (status != IXGBE_SUCCESS)
		DEBUGOUT1("port expander access failed with %d\n", status);
	return status;
}

/**
 * ixgbe_write_pe - Write register to port expander
 * @hw: pointer to hardware structure
 * @reg: register number to write
 * @value: value to write
 *
 * Returns status code
 **/
int32_t ixgbe_write_pe(struct ixgbe_hw *hw, uint8_t reg, uint8_t value)
{
	int32_t status;

	status = hw->phy.ops.write_i2c_byte_unlocked(hw, reg, IXGBE_PE, value);
	if (status != IXGBE_SUCCESS)
		DEBUGOUT1("port expander access failed with %d\n", status);
	return status;
}

/**
 * ixgbe_reset_cs4227 - Reset CS4227 using port expander
 * @hw: pointer to hardware structure
 *
 * This function assumes that the caller has acquired the proper semaphore.
 * Returns error code
 **/
int32_t ixgbe_reset_cs4227(struct ixgbe_hw *hw)
{
	int32_t status;
	uint32_t retry;
	uint16_t value;
	uint8_t reg;

	/* Trigger hard reset. */
	status = ixgbe_read_pe(hw, IXGBE_PE_OUTPUT, &reg);
	if (status != IXGBE_SUCCESS)
		return status;
	reg |= IXGBE_PE_BIT1;
	status = ixgbe_write_pe(hw, IXGBE_PE_OUTPUT, reg);
	if (status != IXGBE_SUCCESS)
		return status;

	status = ixgbe_read_pe(hw, IXGBE_PE_CONFIG, &reg);
	if (status != IXGBE_SUCCESS)
		return status;
	reg &= ~IXGBE_PE_BIT1;
	status = ixgbe_write_pe(hw, IXGBE_PE_CONFIG, reg);
	if (status != IXGBE_SUCCESS)
		return status;

	status = ixgbe_read_pe(hw, IXGBE_PE_OUTPUT, &reg);
	if (status != IXGBE_SUCCESS)
		return status;
	reg &= ~IXGBE_PE_BIT1;
	status = ixgbe_write_pe(hw, IXGBE_PE_OUTPUT, reg);
	if (status != IXGBE_SUCCESS)
		return status;

	usec_delay(IXGBE_CS4227_RESET_HOLD);

	status = ixgbe_read_pe(hw, IXGBE_PE_OUTPUT, &reg);
	if (status != IXGBE_SUCCESS)
		return status;
	reg |= IXGBE_PE_BIT1;
	status = ixgbe_write_pe(hw, IXGBE_PE_OUTPUT, reg);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Wait for the reset to complete. */
	msec_delay(IXGBE_CS4227_RESET_DELAY);
	for (retry = 0; retry < IXGBE_CS4227_RETRIES; retry++) {
		status = ixgbe_read_cs4227(hw, IXGBE_CS4227_EFUSE_STATUS,
					   &value);
		if (status == IXGBE_SUCCESS &&
		    value == IXGBE_CS4227_EEPROM_LOAD_OK)
			break;
		msec_delay(IXGBE_CS4227_CHECK_DELAY);
	}
	if (retry == IXGBE_CS4227_RETRIES) {
		DEBUGOUT("CS4227 reset did not complete.\n");
		return IXGBE_ERR_PHY;
	}

	status = ixgbe_read_cs4227(hw, IXGBE_CS4227_EEPROM_STATUS, &value);
	if (status != IXGBE_SUCCESS ||
	    !(value & IXGBE_CS4227_EEPROM_LOAD_OK)) {
		DEBUGOUT("CS4227 EEPROM did not load successfully.\n");
		return IXGBE_ERR_PHY;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_check_cs4227 - Check CS4227 and reset as needed
 * @hw: pointer to hardware structure
 **/
void ixgbe_check_cs4227(struct ixgbe_hw *hw)
{
	int32_t status = IXGBE_SUCCESS;
	uint32_t swfw_mask = hw->phy.phy_semaphore_mask;
	uint16_t value = 0;
	uint8_t retry;

	for (retry = 0; retry < IXGBE_CS4227_RETRIES; retry++) {
		status = hw->mac.ops.acquire_swfw_sync(hw, swfw_mask);
		if (status != IXGBE_SUCCESS) {
			DEBUGOUT1("semaphore failed with %d\n", status);
			msec_delay(IXGBE_CS4227_CHECK_DELAY);
			continue;
		}

		/* Get status of reset flow. */
		status = ixgbe_read_cs4227(hw, IXGBE_CS4227_SCRATCH, &value);

		if (status == IXGBE_SUCCESS &&
		    value == IXGBE_CS4227_RESET_COMPLETE)
			goto out;

		if (status != IXGBE_SUCCESS ||
		    value != IXGBE_CS4227_RESET_PENDING)
			break;

		/* Reset is pending. Wait and check again. */
		hw->mac.ops.release_swfw_sync(hw, swfw_mask);
		msec_delay(IXGBE_CS4227_CHECK_DELAY);
	}

	/* If still pending, assume other instance failed. */
	if (retry == IXGBE_CS4227_RETRIES) {
		status = hw->mac.ops.acquire_swfw_sync(hw, swfw_mask);
		if (status != IXGBE_SUCCESS) {
			DEBUGOUT1("semaphore failed with %d\n", status);
			return;
		}
	}

	/* Reset the CS4227. */
	status = ixgbe_reset_cs4227(hw);
	if (status != IXGBE_SUCCESS) {
		DEBUGOUT1("CS4227 reset failed: %d\n", status);
		goto out;
	}

	/* Reset takes so long, temporarily release semaphore in case the
	 * other driver instance is waiting for the reset indication.
	 */
	ixgbe_write_cs4227(hw, IXGBE_CS4227_SCRATCH,
			   IXGBE_CS4227_RESET_PENDING);
	hw->mac.ops.release_swfw_sync(hw, swfw_mask);
	msec_delay(10);
	status = hw->mac.ops.acquire_swfw_sync(hw, swfw_mask);
	if (status != IXGBE_SUCCESS) {
		DEBUGOUT1("semaphore failed with %d\n", status);
		return;
	}

	/* Record completion for next time. */
	status = ixgbe_write_cs4227(hw, IXGBE_CS4227_SCRATCH,
		IXGBE_CS4227_RESET_COMPLETE);

out:
	hw->mac.ops.release_swfw_sync(hw, swfw_mask);
	msec_delay(hw->eeprom.semaphore_delay);
}

/**
 * ixgbe_setup_mux_ctl - Setup ESDP register for I2C mux control
 * @hw: pointer to hardware structure
 **/
void ixgbe_setup_mux_ctl(struct ixgbe_hw *hw)
{
	uint32_t esdp = IXGBE_READ_REG(hw, IXGBE_ESDP);

	if (hw->bus.lan_id) {
		esdp &= ~(IXGBE_ESDP_SDP1_NATIVE | IXGBE_ESDP_SDP1);
		esdp |= IXGBE_ESDP_SDP1_DIR;
	}
	esdp &= ~(IXGBE_ESDP_SDP0_NATIVE | IXGBE_ESDP_SDP0_DIR);
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp);
	IXGBE_WRITE_FLUSH(hw);
}

/**
 * ixgbe_identify_phy_x550em - Get PHY type based on device id
 * @hw: pointer to hardware structure
 *
 * Returns error code
 */
int32_t ixgbe_identify_phy_x550em(struct ixgbe_hw *hw)
{
	switch (hw->device_id) {
	case IXGBE_DEV_ID_X550EM_X_SFP:
		/* set up for CS4227 usage */
		hw->phy.phy_semaphore_mask = IXGBE_GSSR_SHARED_I2C_SM;
		ixgbe_setup_mux_ctl(hw);
		ixgbe_check_cs4227(hw);

		return ixgbe_identify_module_generic(hw);
		break;
	case IXGBE_DEV_ID_X550EM_X_KX4:
		hw->phy.type = ixgbe_phy_x550em_kx4;
		break;
	case IXGBE_DEV_ID_X550EM_X_KR:
		hw->phy.type = ixgbe_phy_x550em_kr;
		break;
	case IXGBE_DEV_ID_X550EM_X_1G_T:
	case IXGBE_DEV_ID_X550EM_X_10G_T:
		return ixgbe_identify_phy_generic(hw);
	default:
		break;
	}
	return IXGBE_SUCCESS;
}

int32_t ixgbe_read_phy_reg_x550em(struct ixgbe_hw *hw, uint32_t reg_addr,
				  uint32_t device_type, uint16_t *phy_data)
{
	return IXGBE_NOT_IMPLEMENTED;
}

int32_t ixgbe_write_phy_reg_x550em(struct ixgbe_hw *hw, uint32_t reg_addr,
				   uint32_t device_type, uint16_t phy_data)
{
	return IXGBE_NOT_IMPLEMENTED;
}

/**
*  ixgbe_init_ops_X550EM - Inits func ptrs and MAC type
*  @hw: pointer to hardware structure
*
*  Initialize the function pointers and for MAC type X550EM.
*  Does not touch the hardware.
**/
int32_t ixgbe_init_ops_X550EM(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	struct ixgbe_phy_info *phy = &hw->phy;
	int32_t ret_val;

	DEBUGFUNC("ixgbe_init_ops_X550EM");

	/* Similar to X550 so start there. */
	ret_val = ixgbe_init_ops_X550(hw);

	/* Since this function eventually calls
	 * ixgbe_init_ops_540 by design, we are setting
	 * the pointers to NULL explicitly here to overwrite
	 * the values being set in the x540 function.
	 */

	/* IPsec not supported in x550EM */
	mac->ops.disable_sec_rx_path = NULL;
	mac->ops.enable_sec_rx_path = NULL;

	/* AUTOC register is not present in x550EM. */
	mac->ops.prot_autoc_read = NULL;
	mac->ops.prot_autoc_write = NULL;

	/* X550EM bus type is internal*/
	hw->bus.type = ixgbe_bus_type_internal;
	mac->ops.get_bus_info = ixgbe_get_bus_info_X550em;

	mac->ops.get_media_type = ixgbe_get_media_type_X550em;
	mac->ops.setup_sfp = ixgbe_setup_sfp_modules_X550em;
	mac->ops.get_link_capabilities = ixgbe_get_link_capabilities_X550em;
	mac->ops.reset_hw = ixgbe_reset_hw_X550em;
	mac->ops.get_supported_physical_layer =
				    ixgbe_get_supported_physical_layer_X550em;

	if (mac->ops.get_media_type(hw) == ixgbe_media_type_copper)
		mac->ops.setup_fc = ixgbe_setup_fc_generic;
	else
		mac->ops.setup_fc = ixgbe_setup_fc_X550em;

	mac->ops.acquire_swfw_sync = ixgbe_acquire_swfw_sync_X550em;
	mac->ops.release_swfw_sync = ixgbe_release_swfw_sync_X550em;

	/* PHY */
	phy->ops.init = ixgbe_init_phy_ops_X550em;
	phy->ops.identify = ixgbe_identify_phy_x550em;
	if (mac->ops.get_media_type(hw) != ixgbe_media_type_copper)
		phy->ops.set_phy_power = NULL;


	/* EEPROM */
	eeprom->ops.init_params = ixgbe_init_eeprom_params_X540;
	eeprom->ops.read = ixgbe_read_ee_hostif_X550;
	eeprom->ops.write = ixgbe_write_ee_hostif_X550;
	eeprom->ops.update_checksum = ixgbe_update_eeprom_checksum_X550;
	eeprom->ops.validate_checksum = ixgbe_validate_eeprom_checksum_X550;
	eeprom->ops.calc_checksum = ixgbe_calc_eeprom_checksum_X550;

	return ret_val;
}

/**
 *  ixgbe_init_eeprom_params_X550 - Initialize EEPROM params
 *  @hw: pointer to hardware structure
 *
 *  Initializes the EEPROM parameters ixgbe_eeprom_info within the
 *  ixgbe_hw struct in order to set up EEPROM access.
 **/
int32_t ixgbe_init_eeprom_params_X550(struct ixgbe_hw *hw)
{
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	uint32_t eec;
	uint16_t eeprom_size;

	DEBUGFUNC("ixgbe_init_eeprom_params_X550");

	if (eeprom->type == ixgbe_eeprom_uninitialized) {
		eeprom->semaphore_delay = 10;
		eeprom->type = ixgbe_flash;

		eec = IXGBE_READ_REG(hw, IXGBE_EEC);
		eeprom_size = (uint16_t)((eec & IXGBE_EEC_SIZE) >>
				    IXGBE_EEC_SIZE_SHIFT);
		eeprom->word_size = 1 << (eeprom_size +
					  IXGBE_EEPROM_WORD_SIZE_SHIFT);

		DEBUGOUT2("Eeprom params: type = %d, size = %d\n",
			  eeprom->type, eeprom->word_size);
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_iosf_wait - Wait for IOSF command completion
 * @hw: pointer to hardware structure
 * @ctrl: pointer to location to receive final IOSF control value
 *
 * Returns failing status on timeout
 *
 * Note: ctrl can be NULL if the IOSF control register value is not needed
 **/
int32_t ixgbe_iosf_wait(struct ixgbe_hw *hw, uint32_t *ctrl)
{
	uint32_t i, command = 0;

	/* Check every 10 usec to see if the address cycle completed.
	 * The SB IOSF BUSY bit will clear when the operation is
	 * complete
	 */
	for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
		command = IXGBE_READ_REG(hw, IXGBE_SB_IOSF_INDIRECT_CTRL);
		if ((command & IXGBE_SB_IOSF_CTRL_BUSY) == 0)
			break;
		usec_delay(10);
	}
	if (ctrl)
		*ctrl = command;
	if (i == IXGBE_MDIO_COMMAND_TIMEOUT) {
		DEBUGOUT( "Wait timed out\n");
		return IXGBE_ERR_PHY;
	}

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_write_iosf_sb_reg_x550 - Writes a value to specified register of the IOSF
 *  device
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @device_type: 3 bit device type
 *  @data: Data to write to the register
 **/
int32_t ixgbe_write_iosf_sb_reg_x550(struct ixgbe_hw *hw, uint32_t reg_addr,
				     uint32_t device_type, uint32_t data)
{
	uint32_t gssr = IXGBE_GSSR_PHY1_SM | IXGBE_GSSR_PHY0_SM;
	uint32_t command, error;
	int32_t ret;

	ret = hw->mac.ops.acquire_swfw_sync(hw, gssr);
	if (ret != IXGBE_SUCCESS)
		return ret;

	ret = ixgbe_iosf_wait(hw, NULL);
	if (ret != IXGBE_SUCCESS)
		goto out;

	command = ((reg_addr << IXGBE_SB_IOSF_CTRL_ADDR_SHIFT) |
		   (device_type << IXGBE_SB_IOSF_CTRL_TARGET_SELECT_SHIFT));

	/* Write IOSF control register */
	IXGBE_WRITE_REG(hw, IXGBE_SB_IOSF_INDIRECT_CTRL, command);

	/* Write IOSF data register */
	IXGBE_WRITE_REG(hw, IXGBE_SB_IOSF_INDIRECT_DATA, data);

	ret = ixgbe_iosf_wait(hw, &command);

	if ((command & IXGBE_SB_IOSF_CTRL_RESP_STAT_MASK) != 0) {
		error = (command & IXGBE_SB_IOSF_CTRL_CMPL_ERR_MASK) >>
			 IXGBE_SB_IOSF_CTRL_CMPL_ERR_SHIFT;
		DEBUGOUT1("Failed to write, error %x\n", error);
		ret = IXGBE_ERR_PHY;
	}

out:
	hw->mac.ops.release_swfw_sync(hw, gssr);
	return ret;
}

/**
 *  ixgbe_read_iosf_sb_reg_x550 - Writes a value to specified register of the IOSF
 *  device
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @device_type: 3 bit device type
 *  @phy_data: Pointer to read data from the register
 **/
int32_t ixgbe_read_iosf_sb_reg_x550(struct ixgbe_hw *hw, uint32_t reg_addr,
				    uint32_t device_type, uint32_t *data)
{
	uint32_t gssr = IXGBE_GSSR_PHY1_SM | IXGBE_GSSR_PHY0_SM;
	uint32_t command, error;
	int32_t ret;

	ret = hw->mac.ops.acquire_swfw_sync(hw, gssr);
	if (ret != IXGBE_SUCCESS)
		return ret;

	ret = ixgbe_iosf_wait(hw, NULL);
	if (ret != IXGBE_SUCCESS)
		goto out;

	command = ((reg_addr << IXGBE_SB_IOSF_CTRL_ADDR_SHIFT) |
		   (device_type << IXGBE_SB_IOSF_CTRL_TARGET_SELECT_SHIFT));

	/* Write IOSF control register */
	IXGBE_WRITE_REG(hw, IXGBE_SB_IOSF_INDIRECT_CTRL, command);

	ret = ixgbe_iosf_wait(hw, &command);

	if ((command & IXGBE_SB_IOSF_CTRL_RESP_STAT_MASK) != 0) {
		error = (command & IXGBE_SB_IOSF_CTRL_CMPL_ERR_MASK) >>
			 IXGBE_SB_IOSF_CTRL_CMPL_ERR_SHIFT;
		DEBUGOUT1("Failed to read, error %x\n", error);
		ret = IXGBE_ERR_PHY;
	}

	if (ret == IXGBE_SUCCESS)
		*data = IXGBE_READ_REG(hw, IXGBE_SB_IOSF_INDIRECT_DATA);

out:
	hw->mac.ops.release_swfw_sync(hw, gssr);
	return ret;
}

/**
 *  ixgbe_get_media_type_X550em - Get media type
 *  @hw: pointer to hardware structure
 *
 *  Returns the media type (fiber, copper, backplane)
 */
enum ixgbe_media_type ixgbe_get_media_type_X550em(struct ixgbe_hw *hw)
{
	enum ixgbe_media_type media_type;

	DEBUGFUNC("ixgbe_get_media_type_X550em");

	/* Detect if there is a copper PHY attached. */
	switch (hw->device_id) {
	case IXGBE_DEV_ID_X550EM_X_KR:
	case IXGBE_DEV_ID_X550EM_X_KX4:
		media_type = ixgbe_media_type_backplane;
		break;
	case IXGBE_DEV_ID_X550EM_X_SFP:
		media_type = ixgbe_media_type_fiber;
		break;
	case IXGBE_DEV_ID_X550EM_X_1G_T:
	case IXGBE_DEV_ID_X550EM_X_10G_T:
		media_type = ixgbe_media_type_copper;
		break;
	default:
		media_type = ixgbe_media_type_unknown;
		break;
	}
	return media_type;
}

/**
 *  ixgbe_supported_sfp_modules_X550em - Check if SFP module type is supported
 *  @hw: pointer to hardware structure
 *  @linear: TRUE if SFP module is linear
 */
int32_t ixgbe_supported_sfp_modules_X550em(struct ixgbe_hw *hw, bool *linear)
{
	DEBUGFUNC("ixgbe_supported_sfp_modules_X550em");

	switch (hw->phy.sfp_type) {
	case ixgbe_sfp_type_not_present:
		return IXGBE_ERR_SFP_NOT_PRESENT;
	case ixgbe_sfp_type_da_cu_core0:
	case ixgbe_sfp_type_da_cu_core1:
		*linear = TRUE;
		break;
	case ixgbe_sfp_type_srlr_core0:
	case ixgbe_sfp_type_srlr_core1:
	case ixgbe_sfp_type_da_act_lmt_core0:
	case ixgbe_sfp_type_da_act_lmt_core1:
	case ixgbe_sfp_type_1g_sx_core0:
	case ixgbe_sfp_type_1g_sx_core1:
	case ixgbe_sfp_type_1g_lx_core0:
	case ixgbe_sfp_type_1g_lx_core1:
		*linear = FALSE;
		break;
	case ixgbe_sfp_type_unknown:
	case ixgbe_sfp_type_1g_cu_core0:
	case ixgbe_sfp_type_1g_cu_core1:
	default:
		return IXGBE_ERR_SFP_NOT_SUPPORTED;
	}

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_identify_sfp_module_X550em - Identifies SFP modules
 *  @hw: pointer to hardware structure
 *
 *  Searches for and identifies the SFP module and assigns appropriate PHY type.
 **/
int32_t ixgbe_identify_sfp_module_X550em(struct ixgbe_hw *hw)
{
	int32_t status;
	bool linear;

	DEBUGFUNC("ixgbe_identify_sfp_module_X550em");

	status = ixgbe_identify_module_generic(hw);

	if (status != IXGBE_SUCCESS)
		return status;

	/* Check if SFP module is supported */
	status = ixgbe_supported_sfp_modules_X550em(hw, &linear);

	return status;
}

/**
 *  ixgbe_setup_sfp_modules_X550em - Setup MAC link ops
 *  @hw: pointer to hardware structure
 */
int32_t ixgbe_setup_sfp_modules_X550em(struct ixgbe_hw *hw)
{
	int32_t status;
	bool linear;

	DEBUGFUNC("ixgbe_setup_sfp_modules_X550em");

	/* Check if SFP module is supported */
	status = ixgbe_supported_sfp_modules_X550em(hw, &linear);

	if (status != IXGBE_SUCCESS)
		return status;

	ixgbe_init_mac_link_ops_X550em(hw);
	hw->phy.ops.reset = NULL;

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_init_mac_link_ops_X550em - init mac link function pointers
 *  @hw: pointer to hardware structure
 */
void ixgbe_init_mac_link_ops_X550em(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;

	DEBUGFUNC("ixgbe_init_mac_link_ops_X550em");

	 switch (hw->mac.ops.get_media_type(hw)) {
	 case ixgbe_media_type_fiber:
		/* CS4227 does not support autoneg, so disable the laser control
		 * functions for SFP+ fiber
		 */
		mac->ops.disable_tx_laser = NULL;
		mac->ops.enable_tx_laser = NULL;
		mac->ops.flap_tx_laser = NULL;
		mac->ops.setup_link = ixgbe_setup_mac_link_multispeed_fiber;
		mac->ops.setup_mac_link = ixgbe_setup_mac_link_sfp_x550em;
		mac->ops.set_rate_select_speed =
					ixgbe_set_soft_rate_select_speed;
		break;
	case ixgbe_media_type_copper:
		mac->ops.setup_link = ixgbe_setup_mac_link_t_X550em;
		mac->ops.check_link = ixgbe_check_link_t_X550em;
		break;
	default:
		break;
	 }
}

/**
 *  ixgbe_get_link_capabilities_x550em - Determines link capabilities
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @autoneg: TRUE when autoneg or autotry is enabled
 */
int32_t ixgbe_get_link_capabilities_X550em(struct ixgbe_hw *hw,
					   ixgbe_link_speed *speed,
					   bool *autoneg)
{
	DEBUGFUNC("ixgbe_get_link_capabilities_X550em");

	/* SFP */
	if (hw->phy.media_type == ixgbe_media_type_fiber) {

		/* CS4227 SFP must not enable auto-negotiation */
		*autoneg = FALSE;

		/* Check if 1G SFP module. */
		if (hw->phy.sfp_type == ixgbe_sfp_type_1g_sx_core0 ||
		    hw->phy.sfp_type == ixgbe_sfp_type_1g_sx_core1
		    || hw->phy.sfp_type == ixgbe_sfp_type_1g_lx_core0 ||
		    hw->phy.sfp_type == ixgbe_sfp_type_1g_lx_core1) {
			*speed = IXGBE_LINK_SPEED_1GB_FULL;
			return IXGBE_SUCCESS;
		}

		/* Link capabilities are based on SFP */
		if (hw->phy.multispeed_fiber)
			*speed = IXGBE_LINK_SPEED_10GB_FULL |
				 IXGBE_LINK_SPEED_1GB_FULL;
		else
			*speed = IXGBE_LINK_SPEED_10GB_FULL;
	} else {
		*speed = IXGBE_LINK_SPEED_10GB_FULL |
			 IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = TRUE;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_get_lasi_ext_t_x550em - Determime external Base T PHY interrupt cause
 * @hw: pointer to hardware structure
 * @lsc: pointer to boolean flag which indicates whether external Base T
 *       PHY interrupt is lsc
 *
 * Determime if external Base T PHY interrupt cause is high temperature
 * failure alarm or link status change.
 *
 * Return IXGBE_ERR_OVERTEMP if interrupt is high temperature
 * failure alarm, else return PHY access status.
 */
int32_t ixgbe_get_lasi_ext_t_x550em(struct ixgbe_hw *hw, bool *lsc)
{
	uint32_t status;
	uint16_t reg;

	*lsc = FALSE;

	/* Vendor alarm triggered */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_CHIP_STD_INT_FLAG,
				      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				      &reg);

	if (status != IXGBE_SUCCESS ||
	    !(reg & IXGBE_MDIO_GLOBAL_VEN_ALM_INT_EN))
		return status;

	/* Vendor Auto-Neg alarm triggered or Global alarm 1 triggered */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_INT_CHIP_VEN_FLAG,
				      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				      &reg);

	if (status != IXGBE_SUCCESS ||
	    !(reg & (IXGBE_MDIO_GLOBAL_AN_VEN_ALM_INT_EN |
	    IXGBE_MDIO_GLOBAL_ALARM_1_INT)))
		return status;

	/* Global alarm triggered */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_ALARM_1,
				      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				      &reg);

	if (status != IXGBE_SUCCESS)
		return status;

	/* If high temperature failure, then return over temp error and exit */
	if (reg & IXGBE_MDIO_GLOBAL_ALM_1_HI_TMP_FAIL) {
		/* power down the PHY in case the PHY FW didn't already */
		ixgbe_set_copper_phy_power(hw, FALSE);
		return IXGBE_ERR_OVERTEMP;
	} else if (reg & IXGBE_MDIO_GLOBAL_ALM_1_DEV_FAULT) {
		/*  device fault alarm triggered */
		status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_FAULT_MSG,
					  IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
					  &reg);

		if (status != IXGBE_SUCCESS)
			return status;

		/* if device fault was due to high temp alarm handle and exit */
		if (reg == IXGBE_MDIO_GLOBAL_FAULT_MSG_HI_TMP) {
			/* power down the PHY in case the PHY FW didn't */
			ixgbe_set_copper_phy_power(hw, FALSE);
			return IXGBE_ERR_OVERTEMP;
		}
	}

	/* Vendor alarm 2 triggered */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_CHIP_STD_INT_FLAG,
				      IXGBE_MDIO_AUTO_NEG_DEV_TYPE, &reg);

	if (status != IXGBE_SUCCESS ||
	    !(reg & IXGBE_MDIO_GLOBAL_STD_ALM2_INT))
		return status;

	/* link connect/disconnect event occurred */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_AUTO_NEG_VENDOR_TX_ALARM2,
				      IXGBE_MDIO_AUTO_NEG_DEV_TYPE, &reg);

	if (status != IXGBE_SUCCESS)
		return status;

	/* Indicate LSC */
	if (reg & IXGBE_MDIO_AUTO_NEG_VEN_LSC)
		*lsc = TRUE;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_enable_lasi_ext_t_x550em - Enable external Base T PHY interrupts
 * @hw: pointer to hardware structure
 *
 * Enable link status change and temperature failure alarm for the external
 * Base T PHY
 *
 * Returns PHY access status
 */
int32_t ixgbe_enable_lasi_ext_t_x550em(struct ixgbe_hw *hw)
{
	uint32_t status;
	uint16_t reg;
	bool lsc;

	/* Clear interrupt flags */
	status = ixgbe_get_lasi_ext_t_x550em(hw, &lsc);

	/* Enable link status change alarm */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_PMA_TX_VEN_LASI_INT_MASK,
				      IXGBE_MDIO_AUTO_NEG_DEV_TYPE, &reg);

	if (status != IXGBE_SUCCESS)
		return status;

	reg |= IXGBE_MDIO_PMA_TX_VEN_LASI_INT_EN;

	status = hw->phy.ops.write_reg(hw, IXGBE_MDIO_PMA_TX_VEN_LASI_INT_MASK,
				       IXGBE_MDIO_AUTO_NEG_DEV_TYPE, reg);

	if (status != IXGBE_SUCCESS)
		return status;

	/* Enables high temperature failure alarm */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_INT_MASK,
				      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				      &reg);

	if (status != IXGBE_SUCCESS)
		return status;

	reg |= IXGBE_MDIO_GLOBAL_INT_HI_TEMP_EN;

	status = hw->phy.ops.write_reg(hw, IXGBE_MDIO_GLOBAL_INT_MASK,
				       IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				       reg);

	if (status != IXGBE_SUCCESS)
		return status;

	/* Enable vendor Auto-Neg alarm and Global Interrupt Mask 1 alarm */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_INT_CHIP_VEN_MASK,
				      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				      &reg);

	if (status != IXGBE_SUCCESS)
		return status;

	reg |= (IXGBE_MDIO_GLOBAL_AN_VEN_ALM_INT_EN |
		IXGBE_MDIO_GLOBAL_ALARM_1_INT);

	status = hw->phy.ops.write_reg(hw, IXGBE_MDIO_GLOBAL_INT_CHIP_VEN_MASK,
				       IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				       reg);

	if (status != IXGBE_SUCCESS)
		return status;

	/* Enable chip-wide vendor alarm */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_GLOBAL_INT_CHIP_STD_MASK,
				      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				      &reg);

	if (status != IXGBE_SUCCESS)
		return status;

	reg |= IXGBE_MDIO_GLOBAL_VEN_ALM_INT_EN;

	status = hw->phy.ops.write_reg(hw, IXGBE_MDIO_GLOBAL_INT_CHIP_STD_MASK,
				       IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
				       reg);

	return status;
}

/**
 *  ixgbe_setup_kr_speed_x550em - Configure the KR PHY for link speed.
 *  @hw: pointer to hardware structure
 *  @speed: link speed
 *
 *  Configures the integrated KR PHY.
 **/
int32_t ixgbe_setup_kr_speed_x550em(struct ixgbe_hw *hw,
				    ixgbe_link_speed speed)
{
	int32_t status;
	uint32_t reg_val;

	status = ixgbe_read_iosf_sb_reg_x550(hw,
		IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status)
		return status;

	reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_AN_ENABLE;
	reg_val &= ~(IXGBE_KRM_LINK_CTRL_1_TETH_AN_CAP_KR |
		     IXGBE_KRM_LINK_CTRL_1_TETH_AN_CAP_KX);

	/* Advertise 10G support. */
	if (speed & IXGBE_LINK_SPEED_10GB_FULL)
		reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_AN_CAP_KR;

	/* Advertise 1G support. */
	if (speed & IXGBE_LINK_SPEED_1GB_FULL)
		reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_AN_CAP_KX;

	/* Restart auto-negotiation. */
	reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_AN_RESTART;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
		IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);

	return status;
}

/**
 *  ixgbe_init_phy_ops_X550em - PHY/SFP specific init
 *  @hw: pointer to hardware structure
 *
 *  Initialize any function pointers that were not able to be
 *  set during init_shared_code because the PHY/SFP type was
 *  not known.  Perform the SFP init if necessary.
 */
int32_t ixgbe_init_phy_ops_X550em(struct ixgbe_hw *hw)
{
	struct ixgbe_phy_info *phy = &hw->phy;
	ixgbe_link_speed speed;
	int32_t ret_val;

	DEBUGFUNC("ixgbe_init_phy_ops_X550em");

	hw->mac.ops.set_lan_id(hw);

	if (hw->mac.ops.get_media_type(hw) == ixgbe_media_type_fiber) {
		phy->phy_semaphore_mask = IXGBE_GSSR_SHARED_I2C_SM;
		ixgbe_setup_mux_ctl(hw);

		/* Save NW management interface connected on board. This is used
		 * to determine internal PHY mode.
		 */
		phy->nw_mng_if_sel = IXGBE_READ_REG(hw, IXGBE_NW_MNG_IF_SEL);
		if (phy->nw_mng_if_sel & IXGBE_NW_MNG_IF_SEL_INT_PHY_MODE) {
			speed = IXGBE_LINK_SPEED_10GB_FULL |
				IXGBE_LINK_SPEED_1GB_FULL;
		}
		phy->ops.identify_sfp = ixgbe_identify_sfp_module_X550em;
	}

	/* Identify the PHY or SFP module */
	ret_val = phy->ops.identify(hw);
	if (ret_val == IXGBE_ERR_SFP_NOT_SUPPORTED)
		return ret_val;

	/* Setup function pointers based on detected hardware */
	ixgbe_init_mac_link_ops_X550em(hw);
	if (phy->sfp_type != ixgbe_sfp_type_unknown)
		phy->ops.reset = NULL;

	/* Set functions pointers based on phy type */
	switch (hw->phy.type) {
	case ixgbe_phy_x550em_kx4:
		phy->ops.setup_link = NULL;
		phy->ops.read_reg = ixgbe_read_phy_reg_x550em;
		phy->ops.write_reg = ixgbe_write_phy_reg_x550em;
		break;
	case ixgbe_phy_x550em_kr:
		phy->ops.setup_link = ixgbe_setup_kr_x550em;
		phy->ops.read_reg = ixgbe_read_phy_reg_x550em;
		phy->ops.write_reg = ixgbe_write_phy_reg_x550em;
		break;
	case ixgbe_phy_x550em_ext_t:
		/* Save NW management interface connected on board. This is used
		 * to determine internal PHY mode
		 */
		phy->nw_mng_if_sel = IXGBE_READ_REG(hw, IXGBE_NW_MNG_IF_SEL);

		/* If internal link mode is XFI, then setup iXFI internal link,
		 * else setup KR now.
		 */
		if (!(phy->nw_mng_if_sel & IXGBE_NW_MNG_IF_SEL_INT_PHY_MODE)) {
			phy->ops.setup_internal_link =
					      ixgbe_setup_internal_phy_t_x550em;
		} else {
			speed = IXGBE_LINK_SPEED_10GB_FULL |
				IXGBE_LINK_SPEED_1GB_FULL;
			ret_val = ixgbe_setup_kr_speed_x550em(hw, speed);
		}

		phy->ops.handle_lasi = ixgbe_handle_lasi_ext_t_x550em;
		phy->ops.reset = ixgbe_reset_phy_t_X550em;
		break;
	default:
		break;
	}
	return ret_val;
}

/**
 *  ixgbe_reset_hw_X550em - Perform hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks
 *  and clears all interrupts, perform a PHY reset, and perform a link (MAC)
 *  reset.
 */
int32_t ixgbe_reset_hw_X550em(struct ixgbe_hw *hw)
{
	ixgbe_link_speed link_speed;
	int32_t status;
	uint32_t ctrl = 0;
	uint32_t i;
	uint32_t hlreg0;
	bool link_up = FALSE;

	DEBUGFUNC("ixgbe_reset_hw_X550em");

	/* Call adapter stop to disable Tx/Rx and clear interrupts */
	status = hw->mac.ops.stop_adapter(hw);
	if (status != IXGBE_SUCCESS)
		return status;

	/* flush pending Tx transactions */
	ixgbe_clear_tx_pending(hw);

	if (hw->device_id == IXGBE_DEV_ID_X550EM_X_10G_T) {
		/* Config MDIO clock speed before the first MDIO PHY access */
		hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
		hlreg0 &= ~IXGBE_HLREG0_MDCSPD;
		IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);
	}

	/* PHY ops must be identified and initialized prior to reset */
	status = hw->phy.ops.init(hw);

	if (status == IXGBE_ERR_SFP_NOT_SUPPORTED)
		return status;

	/* start the external PHY */
	if (hw->phy.type == ixgbe_phy_x550em_ext_t) {
		status = ixgbe_init_ext_t_x550em(hw);
		if (status)
			return status;
	}

	/* Setup SFP module if there is one present. */
	if (hw->phy.sfp_setup_needed) {
		status = hw->mac.ops.setup_sfp(hw);
		hw->phy.sfp_setup_needed = FALSE;
	}

	if (status == IXGBE_ERR_SFP_NOT_SUPPORTED)
		return status;

	/* Reset PHY */
	if (!hw->phy.reset_disable && hw->phy.ops.reset)
		hw->phy.ops.reset(hw);

mac_reset_top:
	/* Issue global reset to the MAC.  Needs to be SW reset if link is up.
	 * If link reset is used when link is up, it might reset the PHY when
	 * mng is using it.  If link is down or the flag to force full link
	 * reset is set, then perform link reset.
	 */
	ctrl = IXGBE_CTRL_LNK_RST;
	if (!hw->force_full_reset) {
		hw->mac.ops.check_link(hw, &link_speed, &link_up, FALSE);
		if (link_up)
			ctrl = IXGBE_CTRL_RST;
	}

	ctrl |= IXGBE_READ_REG(hw, IXGBE_CTRL);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, ctrl);
	IXGBE_WRITE_FLUSH(hw);

	/* Poll for reset bit to self-clear meaning reset is complete */
	for (i = 0; i < 10; i++) {
		usec_delay(1);
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		if (!(ctrl & IXGBE_CTRL_RST_MASK))
			break;
	}

	if (ctrl & IXGBE_CTRL_RST_MASK) {
		status = IXGBE_ERR_RESET_FAILED;
		DEBUGOUT("Reset polling failed to complete.\n");
	}

	msec_delay(50);

	/* Double resets are required for recovery from certain error
	 * conditions.  Between resets, it is necessary to stall to
	 * allow time for any pending HW events to complete.
	 */
	if (hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		goto mac_reset_top;
	}

	/* Store the permanent mac address */
	hw->mac.ops.get_mac_addr(hw, hw->mac.perm_addr);

	/* Store MAC address from RAR0, clear receive address registers, and
	 * clear the multicast table.  Also reset num_rar_entries to 128,
	 * since we modify this value when programming the SAN MAC address.
	 */
	hw->mac.num_rar_entries = 128;
	hw->mac.ops.init_rx_addrs(hw);

	if (hw->device_id == IXGBE_DEV_ID_X550EM_X_SFP)
		ixgbe_setup_mux_ctl(hw);

	return status;
}

/**
 * ixgbe_init_ext_t_x550em - Start (unstall) the external Base T PHY.
 * @hw: pointer to hardware structure
 */
int32_t ixgbe_init_ext_t_x550em(struct ixgbe_hw *hw)
{
	uint32_t status;
	uint16_t reg;

	status = hw->phy.ops.read_reg(hw,
				      IXGBE_MDIO_TX_VENDOR_ALARMS_3,
				      IXGBE_MDIO_PMA_PMD_DEV_TYPE,
				      &reg);

	if (status != IXGBE_SUCCESS)
		return status;

	/* If PHY FW reset completed bit is set then this is the first
	 * SW instance after a power on so the PHY FW must be un-stalled.
	 */
	if (reg & IXGBE_MDIO_TX_VENDOR_ALARMS_3_RST_MASK) {
		status = hw->phy.ops.read_reg(hw,
					IXGBE_MDIO_GLOBAL_RES_PR_10,
					IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
					&reg);

		if (status != IXGBE_SUCCESS)
			return status;

		reg &= ~IXGBE_MDIO_POWER_UP_STALL;

		status = hw->phy.ops.write_reg(hw,
					IXGBE_MDIO_GLOBAL_RES_PR_10,
					IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE,
					reg);

		if (status != IXGBE_SUCCESS)
			return status;
	}

	return status;
}

/**
 *  ixgbe_setup_kr_x550em - Configure the KR PHY.
 *  @hw: pointer to hardware structure
 *
 *  Configures the integrated KR PHY.
 **/
int32_t ixgbe_setup_kr_x550em(struct ixgbe_hw *hw)
{
	return ixgbe_setup_kr_speed_x550em(hw, hw->phy.autoneg_advertised);
}

/**
 *  ixgbe_setup_mac_link_sfp_x550em - Setup internal/external the PHY for SFP
 *  @hw: pointer to hardware structure
 *
 *  Configure the external PHY and the integrated KR PHY for SFP support.
 **/
int32_t ixgbe_setup_mac_link_sfp_x550em(struct ixgbe_hw *hw,
					ixgbe_link_speed speed,
					bool autoneg_wait_to_complete)
{
	int32_t ret_val;
	uint16_t reg_slice, reg_val;
	bool setup_linear = FALSE;

	/* Check if SFP module is supported and linear */
	ret_val = ixgbe_supported_sfp_modules_X550em(hw, &setup_linear);

	/* If no SFP module present, then return success. Return success since
	 * there is no reason to configure CS4227 and SFP not present error is
	 * not excepted in the setup MAC link flow.
	 */
	if (ret_val == IXGBE_ERR_SFP_NOT_PRESENT)
		return IXGBE_SUCCESS;

	if (ret_val != IXGBE_SUCCESS)
		return ret_val;

	if (!(hw->phy.nw_mng_if_sel & IXGBE_NW_MNG_IF_SEL_INT_PHY_MODE)) {
		/* Configure CS4227 LINE side to 10G SR. */
		reg_slice = IXGBE_CS4227_LINE_SPARE22_MSB +
			    (hw->bus.lan_id << 12);
		reg_val = IXGBE_CS4227_SPEED_10G;
		ret_val = hw->phy.ops.write_i2c_combined(hw, IXGBE_CS4227,
							 reg_slice, reg_val);

		reg_slice = IXGBE_CS4227_LINE_SPARE24_LSB +
			    (hw->bus.lan_id << 12);
		reg_val = (IXGBE_CS4227_EDC_MODE_SR << 1) | 0x1;
		ret_val = hw->phy.ops.write_i2c_combined(hw, IXGBE_CS4227,
							 reg_slice, reg_val);

		/* Configure CS4227 for HOST connection rate then type. */
		reg_slice = IXGBE_CS4227_HOST_SPARE22_MSB +
			    (hw->bus.lan_id << 12);
		reg_val = (speed & IXGBE_LINK_SPEED_10GB_FULL) ?
		IXGBE_CS4227_SPEED_10G : IXGBE_CS4227_SPEED_1G;
		ret_val = hw->phy.ops.write_i2c_combined(hw, IXGBE_CS4227,
							 reg_slice, reg_val);

		reg_slice = IXGBE_CS4227_HOST_SPARE24_LSB +
			    (hw->bus.lan_id << 12);
		if (setup_linear)
			reg_val = (IXGBE_CS4227_EDC_MODE_CX1 << 1) | 0x1;
		else
			reg_val = (IXGBE_CS4227_EDC_MODE_SR << 1) | 0x1;
		ret_val = hw->phy.ops.write_i2c_combined(hw, IXGBE_CS4227,
							 reg_slice, reg_val);

		/* Setup XFI internal link. */
		ret_val = ixgbe_setup_ixfi_x550em(hw, &speed);
	} else {
		/* Configure internal PHY for KR/KX. */
		ixgbe_setup_kr_speed_x550em(hw, speed);

		/* Configure CS4227 LINE side to proper mode. */
		reg_slice = IXGBE_CS4227_LINE_SPARE24_LSB +
			    (hw->bus.lan_id << 12);
		if (setup_linear)
			reg_val = (IXGBE_CS4227_EDC_MODE_CX1 << 1) | 0x1;
		else
			reg_val = (IXGBE_CS4227_EDC_MODE_SR << 1) | 0x1;
		ret_val = hw->phy.ops.write_i2c_combined(hw, IXGBE_CS4227,
							 reg_slice, reg_val);
	}
	return ret_val;
}

/**
 *  ixgbe_setup_ixfi_x550em - Configure the KR PHY for iXFI mode.
 *  @hw: pointer to hardware structure
 *  @speed: the link speed to force
 *
 *  Configures the integrated KR PHY to use iXFI mode. Used to connect an
 *  internal and external PHY at a specific speed, without autonegotiation.
 **/
int32_t ixgbe_setup_ixfi_x550em(struct ixgbe_hw *hw, ixgbe_link_speed *speed)
{
	int32_t status;
	uint32_t reg_val;

	/* Disable AN and force speed to 10G Serial. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
					IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
					IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	reg_val &= ~IXGBE_KRM_LINK_CTRL_1_TETH_AN_ENABLE;
	reg_val &= ~IXGBE_KRM_LINK_CTRL_1_TETH_FORCE_SPEED_MASK;

	/* Select forced link speed for internal PHY. */
	switch (*speed) {
	case IXGBE_LINK_SPEED_10GB_FULL:
		reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_FORCE_SPEED_10G;
		break;
	case IXGBE_LINK_SPEED_1GB_FULL:
		reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_FORCE_SPEED_1G;
		break;
	default:
		/* Other link speeds are not supported by internal KR PHY. */
		return IXGBE_ERR_LINK_SETUP;
	}

	status = ixgbe_write_iosf_sb_reg_x550(hw,
					IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
					IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Disable training protocol FSM. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
				IXGBE_KRM_RX_TRN_LINKUP_CTRL(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val |= IXGBE_KRM_RX_TRN_LINKUP_CTRL_CONV_WO_PROTOCOL;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
				IXGBE_KRM_RX_TRN_LINKUP_CTRL(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Disable Flex from training TXFFE. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
				IXGBE_KRM_DSP_TXFFE_STATE_4(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val &= ~IXGBE_KRM_DSP_TXFFE_STATE_C0_EN;
	reg_val &= ~IXGBE_KRM_DSP_TXFFE_STATE_CP1_CN1_EN;
	reg_val &= ~IXGBE_KRM_DSP_TXFFE_STATE_CO_ADAPT_EN;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
				IXGBE_KRM_DSP_TXFFE_STATE_4(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	status = ixgbe_read_iosf_sb_reg_x550(hw,
				IXGBE_KRM_DSP_TXFFE_STATE_5(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val &= ~IXGBE_KRM_DSP_TXFFE_STATE_C0_EN;
	reg_val &= ~IXGBE_KRM_DSP_TXFFE_STATE_CP1_CN1_EN;
	reg_val &= ~IXGBE_KRM_DSP_TXFFE_STATE_CO_ADAPT_EN;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
				IXGBE_KRM_DSP_TXFFE_STATE_5(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Enable override for coefficients. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
				IXGBE_KRM_TX_COEFF_CTRL_1(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val |= IXGBE_KRM_TX_COEFF_CTRL_1_OVRRD_EN;
	reg_val |= IXGBE_KRM_TX_COEFF_CTRL_1_CZERO_EN;
	reg_val |= IXGBE_KRM_TX_COEFF_CTRL_1_CPLUS1_OVRRD_EN;
	reg_val |= IXGBE_KRM_TX_COEFF_CTRL_1_CMINUS1_OVRRD_EN;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
				IXGBE_KRM_TX_COEFF_CTRL_1(hw->bus.lan_id),
				IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Toggle port SW reset by AN reset. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
					IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
					IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_AN_RESTART;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
					IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
					IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);

	return status;
}

/**
 * ixgbe_ext_phy_t_x550em_get_link - Get ext phy link status
 * @hw: address of hardware structure
 * @link_up: address of boolean to indicate link status
 *
 * Returns error code if unable to get link status.
 */
int32_t ixgbe_ext_phy_t_x550em_get_link(struct ixgbe_hw *hw, bool *link_up)
{
	uint32_t ret;
	uint16_t autoneg_status;

	*link_up = FALSE;

	/* read this twice back to back to indicate current status */
	ret = hw->phy.ops.read_reg(hw, IXGBE_MDIO_AUTO_NEG_STATUS,
				   IXGBE_MDIO_AUTO_NEG_DEV_TYPE,
				   &autoneg_status);
	if (ret != IXGBE_SUCCESS)
		return ret;

	ret = hw->phy.ops.read_reg(hw, IXGBE_MDIO_AUTO_NEG_STATUS,
				   IXGBE_MDIO_AUTO_NEG_DEV_TYPE,
				   &autoneg_status);
	if (ret != IXGBE_SUCCESS)
		return ret;

	*link_up = !!(autoneg_status & IXGBE_MDIO_AUTO_NEG_LINK_STATUS);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_setup_internal_phy_t_x550em - Configure KR PHY to X557 link
 * @hw: point to hardware structure
 *
 * Configures the link between the integrated KR PHY and the external X557 PHY
 * The driver will call this function when it gets a link status change
 * interrupt from the X557 PHY. This function configures the link speed
 * between the PHYs to match the link speed of the BASE-T link.
 *
 * A return of a non-zero value indicates an error, and the base driver should
 * not report link up.
 */
int32_t ixgbe_setup_internal_phy_t_x550em(struct ixgbe_hw *hw)
{
	ixgbe_link_speed force_speed;
	bool link_up;
	uint32_t status;
	uint16_t speed;

	if (hw->mac.ops.get_media_type(hw) != ixgbe_media_type_copper)
		return IXGBE_ERR_CONFIG;

	/* If link is not up, then there is no setup necessary so return  */
	status = ixgbe_ext_phy_t_x550em_get_link(hw, &link_up);
	if (status != IXGBE_SUCCESS)
		return status;

	if (!link_up)
		return IXGBE_SUCCESS;

	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_AUTO_NEG_VENDOR_STAT,
				      IXGBE_MDIO_AUTO_NEG_DEV_TYPE,
				      &speed);
	if (status != IXGBE_SUCCESS)
		return status;

	/* If link is not still up, then no setup is necessary so return */
	status = ixgbe_ext_phy_t_x550em_get_link(hw, &link_up);
	if (status != IXGBE_SUCCESS)
		return status;
	if (!link_up)
		return IXGBE_SUCCESS;

	/* clear everything but the speed and duplex bits */
	speed &= IXGBE_MDIO_AUTO_NEG_VENDOR_STATUS_MASK;

	switch (speed) {
	case IXGBE_MDIO_AUTO_NEG_VENDOR_STATUS_10GB_FULL:
		force_speed = IXGBE_LINK_SPEED_10GB_FULL;
		break;
	case IXGBE_MDIO_AUTO_NEG_VENDOR_STATUS_1GB_FULL:
		force_speed = IXGBE_LINK_SPEED_1GB_FULL;
		break;
	default:
		/* Internal PHY does not support anything else */
		return IXGBE_ERR_INVALID_LINK_SETTINGS;
	}

	return ixgbe_setup_ixfi_x550em(hw, &force_speed);
}

/**
 *  ixgbe_setup_phy_loopback_x550em - Configure the KR PHY for loopback.
 *  @hw: pointer to hardware structure
 *
 *  Configures the integrated KR PHY to use internal loopback mode.
 **/
int32_t ixgbe_setup_phy_loopback_x550em(struct ixgbe_hw *hw)
{
	int32_t status;
	uint32_t reg_val;

	/* Disable AN and force speed to 10G Serial. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
		IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val &= ~IXGBE_KRM_LINK_CTRL_1_TETH_AN_ENABLE;
	reg_val &= ~IXGBE_KRM_LINK_CTRL_1_TETH_FORCE_SPEED_MASK;
	reg_val |= IXGBE_KRM_LINK_CTRL_1_TETH_FORCE_SPEED_10G;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
		IXGBE_KRM_LINK_CTRL_1(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Set near-end loopback clocks. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
		IXGBE_KRM_PORT_CAR_GEN_CTRL(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val |= IXGBE_KRM_PORT_CAR_GEN_CTRL_NELB_32B;
	reg_val |= IXGBE_KRM_PORT_CAR_GEN_CTRL_NELB_KRPCS;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
		IXGBE_KRM_PORT_CAR_GEN_CTRL(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Set loopback enable. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
		IXGBE_KRM_PMD_DFX_BURNIN(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val |= IXGBE_KRM_PMD_DFX_BURNIN_TX_RX_KR_LB_MASK;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
		IXGBE_KRM_PMD_DFX_BURNIN(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);
	if (status != IXGBE_SUCCESS)
		return status;

	/* Training bypass. */
	status = ixgbe_read_iosf_sb_reg_x550(hw,
		IXGBE_KRM_RX_TRN_LINKUP_CTRL(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
	if (status != IXGBE_SUCCESS)
		return status;
	reg_val |= IXGBE_KRM_RX_TRN_LINKUP_CTRL_PROTOCOL_BYPASS;
	status = ixgbe_write_iosf_sb_reg_x550(hw,
		IXGBE_KRM_RX_TRN_LINKUP_CTRL(hw->bus.lan_id),
		IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);

	return status;
}

/**
 *  ixgbe_read_ee_hostif_X550 - Read EEPROM word using a host interface command
 *  assuming that the semaphore is already obtained.
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM using the hostif.
 **/
int32_t ixgbe_read_ee_hostif_data_X550(struct ixgbe_hw *hw, uint16_t offset,
				       uint16_t *data)
{
	int32_t status;
	struct ixgbe_hic_read_shadow_ram buffer;

	DEBUGFUNC("ixgbe_read_ee_hostif_data_X550");
	buffer.hdr.req.cmd = FW_READ_SHADOW_RAM_CMD;
	buffer.hdr.req.buf_lenh = 0;
	buffer.hdr.req.buf_lenl = FW_READ_SHADOW_RAM_LEN;
	buffer.hdr.req.checksum = FW_DEFAULT_CHECKSUM;

	/* convert offset from words to bytes */
	buffer.address = htobe32(offset * 2);
	/* one word */
	buffer.length = htobe16(sizeof(uint16_t));

	status = ixgbe_host_interface_command(hw, (uint32_t *)&buffer,
					      sizeof(buffer),
					      IXGBE_HI_COMMAND_TIMEOUT, FALSE);

	if (status)
		return status;

	*data = (uint16_t)IXGBE_READ_REG_ARRAY(hw, IXGBE_FLEX_MNG,
					  FW_NVM_DATA_OFFSET);

	return 0;
}

/**
 *  ixgbe_read_ee_hostif_X550 - Read EEPROM word using a host interface command
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM using the hostif.
 **/
int32_t ixgbe_read_ee_hostif_X550(struct ixgbe_hw *hw, uint16_t offset,
				  uint16_t *data)
{
	int32_t status = IXGBE_SUCCESS;

	DEBUGFUNC("ixgbe_read_ee_hostif_X550");

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) ==
	    IXGBE_SUCCESS) {
		status = ixgbe_read_ee_hostif_data_X550(hw, offset, data);
		hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	} else {
		status = IXGBE_ERR_SWFW_SYNC;
	}

	return status;
}

/**
 *  ixgbe_read_ee_hostif_buffer_X550- Read EEPROM word(s) using hostif
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @words: number of words
 *  @data: word(s) read from the EEPROM
 *
 *  Reads a 16 bit word(s) from the EEPROM using the hostif.
 **/
int32_t ixgbe_read_ee_hostif_buffer_X550(struct ixgbe_hw *hw,
					 uint16_t offset, uint16_t words,
					 uint16_t *data)
{
	struct ixgbe_hic_read_shadow_ram buffer;
	uint32_t current_word = 0;
	uint16_t words_to_read;
	int32_t status;
	uint32_t i;

	DEBUGFUNC("ixgbe_read_ee_hostif_buffer_X550");

	/* Take semaphore for the entire operation. */
	status = hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	if (status) {
		DEBUGOUT("EEPROM read buffer - semaphore failed\n");
		return status;
	}
	while (words) {
		if (words > FW_MAX_READ_BUFFER_SIZE / 2)
			words_to_read = FW_MAX_READ_BUFFER_SIZE / 2;
		else
			words_to_read = words;

		buffer.hdr.req.cmd = FW_READ_SHADOW_RAM_CMD;
		buffer.hdr.req.buf_lenh = 0;
		buffer.hdr.req.buf_lenl = FW_READ_SHADOW_RAM_LEN;
		buffer.hdr.req.checksum = FW_DEFAULT_CHECKSUM;

		/* convert offset from words to bytes */
		buffer.address = htobe32((offset + current_word) * 2);
		buffer.length = htobe16(words_to_read * 2);

		status = ixgbe_host_interface_command(hw, (uint32_t *)&buffer,
						      sizeof(buffer),
						      IXGBE_HI_COMMAND_TIMEOUT,
						      FALSE);

		if (status) {
			DEBUGOUT("Host interface command failed\n");
			goto out;
		}

		for (i = 0; i < words_to_read; i++) {
			uint32_t reg = IXGBE_FLEX_MNG + (FW_NVM_DATA_OFFSET << 2) +
				  2 * i;
			uint32_t value = IXGBE_READ_REG(hw, reg);

			data[current_word] = (uint16_t)(value & 0xffff);
			current_word++;
			i++;
			if (i < words_to_read) {
				value >>= 16;
				data[current_word] = (uint16_t)(value & 0xffff);
				current_word++;
			}
		}
		words -= words_to_read;
	}

out:
	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	return status;
}

/**
 *  ixgbe_write_ee_hostif_X550 - Write EEPROM word using hostif
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to write
 *  @data: word write to the EEPROM
 *
 *  Write a 16 bit word to the EEPROM using the hostif.
 **/
int32_t ixgbe_write_ee_hostif_data_X550(struct ixgbe_hw *hw, uint16_t offset,
					uint16_t data)
{
	int32_t status;
	struct ixgbe_hic_write_shadow_ram buffer;

	DEBUGFUNC("ixgbe_write_ee_hostif_data_X550");

	buffer.hdr.req.cmd = FW_WRITE_SHADOW_RAM_CMD;
	buffer.hdr.req.buf_lenh = 0;
	buffer.hdr.req.buf_lenl = FW_WRITE_SHADOW_RAM_LEN;
	buffer.hdr.req.checksum = FW_DEFAULT_CHECKSUM;

	 /* one word */
	buffer.length = htobe16(sizeof(uint16_t));
	buffer.data = data;
	buffer.address = htobe32(offset * 2);

	status = ixgbe_host_interface_command(hw, (uint32_t *)&buffer,
					      sizeof(buffer),
					      IXGBE_HI_COMMAND_TIMEOUT, FALSE);

	return status;
}

/**
 *  ixgbe_write_ee_hostif_X550 - Write EEPROM word using hostif
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to write
 *  @data: word write to the EEPROM
 *
 *  Write a 16 bit word to the EEPROM using the hostif.
 **/
int32_t ixgbe_write_ee_hostif_X550(struct ixgbe_hw *hw, uint16_t offset,
				   uint16_t data)
{
	int32_t status = IXGBE_SUCCESS;

	DEBUGFUNC("ixgbe_write_ee_hostif_X550");

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) ==
	    IXGBE_SUCCESS) {
		status = ixgbe_write_ee_hostif_data_X550(hw, offset, data);
		hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	} else {
		DEBUGOUT("write ee hostif failed to get semaphore\n");
		status = IXGBE_ERR_SWFW_SYNC;
	}

	return status;
}

/**
 * ixgbe_checksum_ptr_x550 - Checksum one pointer region
 * @hw: pointer to hardware structure
 * @ptr: pointer offset in eeprom
 * @size: size of section pointed by ptr, if 0 first word will be used as size
 * @csum: address of checksum to update
 *
 * Returns error status for any failure
 */
int32_t ixgbe_checksum_ptr_x550(struct ixgbe_hw *hw, uint16_t ptr,
				uint16_t size, uint16_t *csum, uint16_t *buffer,
				uint32_t buffer_size)
{
	uint16_t buf[256];
	int32_t status;
	uint16_t length, bufsz, i, start;
	uint16_t *local_buffer;

	bufsz = sizeof(buf) / sizeof(buf[0]);

	/* Read a chunk at the pointer location */
	if (!buffer) {
		status = ixgbe_read_ee_hostif_buffer_X550(hw, ptr, bufsz, buf);
		if (status) {
			DEBUGOUT("Failed to read EEPROM image\n");
			return status;
		}
		local_buffer = buf;
	} else {
		if (buffer_size < ptr)
			return  IXGBE_ERR_PARAM;
		local_buffer = &buffer[ptr];
	}

	if (size) {
		start = 0;
		length = size;
	} else {
		start = 1;
		length = local_buffer[0];

		/* Skip pointer section if length is invalid. */
		if (length == 0xFFFF || length == 0 ||
		    (ptr + length) >= hw->eeprom.word_size)
			return IXGBE_SUCCESS;
	}

	if (buffer && ((uint32_t)start + (uint32_t)length > buffer_size))
		return IXGBE_ERR_PARAM;

	for (i = start; length; i++, length--) {
		if (i == bufsz && !buffer) {
			ptr += bufsz;
			i = 0;
			if (length < bufsz)
				bufsz = length;

			/* Read a chunk at the pointer location */
			status = ixgbe_read_ee_hostif_buffer_X550(hw, ptr,
								  bufsz, buf);
			if (status) {
				DEBUGOUT("Failed to read EEPROM image\n");
				return status;
			}
		}
		*csum += local_buffer[i];
	}
	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_calc_checksum_X550 - Calculates and returns the checksum
 *  @hw: pointer to hardware structure
 *  @buffer: pointer to buffer containing calculated checksum
 *  @buffer_size: size of buffer
 *
 *  Returns a negative error code on error, or the 16-bit checksum
 **/
int32_t ixgbe_calc_checksum_X550(struct ixgbe_hw *hw, uint16_t *buffer,
				 uint32_t buffer_size)
{
	uint16_t eeprom_ptrs[IXGBE_EEPROM_LAST_WORD + 1];
	uint16_t *local_buffer;
	int32_t status;
	uint16_t checksum = 0;
	uint16_t pointer, i, size;

	DEBUGFUNC("ixgbe_calc_eeprom_checksum_X550");

	hw->eeprom.ops.init_params(hw);

	if (!buffer) {
		/* Read pointer area */
		status = ixgbe_read_ee_hostif_buffer_X550(hw, 0,
						     IXGBE_EEPROM_LAST_WORD + 1,
						     eeprom_ptrs);
		if (status) {
			DEBUGOUT("Failed to read EEPROM image\n");
			return status;
		}
		local_buffer = eeprom_ptrs;
	} else {
		if (buffer_size < IXGBE_EEPROM_LAST_WORD)
			return IXGBE_ERR_PARAM;
		local_buffer = buffer;
	}

	/*
	 * For X550 hardware include 0x0-0x41 in the checksum, skip the
	 * checksum word itself
	 */
	for (i = 0; i <= IXGBE_EEPROM_LAST_WORD; i++)
		if (i != IXGBE_EEPROM_CHECKSUM)
			checksum += local_buffer[i];

	/*
	 * Include all data from pointers 0x3, 0x6-0xE.  This excludes the
	 * FW, PHY module, and PCIe Expansion/Option ROM pointers.
	 */
	for (i = IXGBE_PCIE_ANALOG_PTR_X550; i < IXGBE_FW_PTR; i++) {
		if (i == IXGBE_PHY_PTR || i == IXGBE_OPTION_ROM_PTR)
			continue;

		pointer = local_buffer[i];

		/* Skip pointer section if the pointer is invalid. */
		if (pointer == 0xFFFF || pointer == 0 ||
		    pointer >= hw->eeprom.word_size)
			continue;

		switch (i) {
		case IXGBE_PCIE_GENERAL_PTR:
			size = IXGBE_IXGBE_PCIE_GENERAL_SIZE;
			break;
		case IXGBE_PCIE_CONFIG0_PTR:
		case IXGBE_PCIE_CONFIG1_PTR:
			size = IXGBE_PCIE_CONFIG_SIZE;
			break;
		default:
			size = 0;
			break;
		}

		status = ixgbe_checksum_ptr_x550(hw, pointer, size, &checksum,
						buffer, buffer_size);
		if (status)
			return status;
	}

	checksum = (uint16_t)IXGBE_EEPROM_SUM - checksum;

	return (int32_t)checksum;
}

/**
 *  ixgbe_calc_eeprom_checksum_X550 - Calculates and returns the checksum
 *  @hw: pointer to hardware structure
 *
 *  Returns a negative error code on error, or the 16-bit checksum
 **/
int32_t ixgbe_calc_eeprom_checksum_X550(struct ixgbe_hw *hw)
{
	return ixgbe_calc_checksum_X550(hw, NULL, 0);
}

/**
 *  ixgbe_validate_eeprom_checksum_X550 - Validate EEPROM checksum
 *  @hw: pointer to hardware structure
 *  @checksum_val: calculated checksum
 *
 *  Performs checksum calculation and validates the EEPROM checksum.  If the
 *  caller does not need checksum_val, the value can be NULL.
 **/
int32_t ixgbe_validate_eeprom_checksum_X550(struct ixgbe_hw *hw, uint16_t *checksum_val)
{
	int32_t status;
	uint16_t checksum;
	uint16_t read_checksum = 0;

	DEBUGFUNC("ixgbe_validate_eeprom_checksum_X550");

	/* Read the first word from the EEPROM. If this times out or fails, do
	 * not continue or we could be in for a very long wait while every
	 * EEPROM read fails
	 */
	status = hw->eeprom.ops.read(hw, 0, &checksum);
	if (status) {
		DEBUGOUT("EEPROM read failed\n");
		return status;
	}

	status = hw->eeprom.ops.calc_checksum(hw);
	if (status < 0)
		return status;

	checksum = (uint16_t)(status & 0xffff);

	status = ixgbe_read_ee_hostif_X550(hw, IXGBE_EEPROM_CHECKSUM,
					   &read_checksum);
	if (status)
		return status;

	/* Verify read checksum from EEPROM is the same as
	 * calculated checksum
	 */
	if (read_checksum != checksum) {
		status = IXGBE_ERR_EEPROM_CHECKSUM;
		DEBUGOUT("Invalid EEPROM checksum\n");
	}

	/* If the user cares, return the calculated checksum */
	if (checksum_val)
		*checksum_val = checksum;

	return status;
}

/**
 * ixgbe_update_eeprom_checksum_X550 - Updates the EEPROM checksum and flash
 * @hw: pointer to hardware structure
 *
 * After writing EEPROM to shadow RAM using EEWR register, software calculates
 * checksum and updates the EEPROM and instructs the hardware to update
 * the flash.
 **/
int32_t ixgbe_update_eeprom_checksum_X550(struct ixgbe_hw *hw)
{
	int32_t status;
	uint16_t checksum = 0;

	DEBUGFUNC("ixgbe_update_eeprom_checksum_X550");

	/* Read the first word from the EEPROM. If this times out or fails, do
	 * not continue or we could be in for a very long wait while every
	 * EEPROM read fails
	 */
	status = ixgbe_read_ee_hostif_X550(hw, 0, &checksum);
	if (status) {
		DEBUGOUT("EEPROM read failed\n");
		return status;
	}

	status = ixgbe_calc_eeprom_checksum_X550(hw);
	if (status < 0)
		return status;

	checksum = (uint16_t)(status & 0xffff);

	status = ixgbe_write_ee_hostif_X550(hw, IXGBE_EEPROM_CHECKSUM,
					    checksum);
	if (status)
		return status;

	status = ixgbe_update_flash_X550(hw);

	return status;
}

/**
 *  ixgbe_update_flash_X550 - Instruct HW to copy EEPROM to Flash device
 *  @hw: pointer to hardware structure
 *
 *  Issue a shadow RAM dump to FW to copy EEPROM from shadow RAM to the flash.
 **/
int32_t ixgbe_update_flash_X550(struct ixgbe_hw *hw)
{
	int32_t status = IXGBE_SUCCESS;
	union ixgbe_hic_hdr2 buffer;

	DEBUGFUNC("ixgbe_update_flash_X550");

	buffer.req.cmd = FW_SHADOW_RAM_DUMP_CMD;
	buffer.req.buf_lenh = 0;
	buffer.req.buf_lenl = FW_SHADOW_RAM_DUMP_LEN;
	buffer.req.checksum = FW_DEFAULT_CHECKSUM;

	status = ixgbe_host_interface_command(hw, (uint32_t *)&buffer,
					      sizeof(buffer),
					      IXGBE_HI_COMMAND_TIMEOUT, FALSE);

	return status;
}

/**
 *  ixgbe_get_supported_physical_layer_X550em - Returns physical layer type
 *  @hw: pointer to hardware structure
 *
 *  Determines physical layer capabilities of the current configuration.
 **/
uint32_t ixgbe_get_supported_physical_layer_X550em(struct ixgbe_hw *hw)
{
	uint32_t physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
	uint16_t ext_ability = 0;

	DEBUGFUNC("ixgbe_get_supported_physical_layer_X550em");

	hw->phy.ops.identify(hw);

	switch (hw->phy.type) {
	case ixgbe_phy_x550em_kr:
		physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_KR |
				 IXGBE_PHYSICAL_LAYER_1000BASE_KX;
		break;
	case ixgbe_phy_x550em_kx4:
		physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_KX4 |
				 IXGBE_PHYSICAL_LAYER_1000BASE_KX;
		break;
	case ixgbe_phy_x550em_ext_t:
		hw->phy.ops.read_reg(hw, IXGBE_MDIO_PHY_EXT_ABILITY,
				     IXGBE_MDIO_PMA_PMD_DEV_TYPE,
				     &ext_ability);
		if (ext_ability & IXGBE_MDIO_PHY_10GBASET_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_T;
		if (ext_ability & IXGBE_MDIO_PHY_1000BASET_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_T;
		break;
	default:
		break;
	}

	if (hw->mac.ops.get_media_type(hw) == ixgbe_media_type_fiber)
		physical_layer = ixgbe_get_supported_phy_sfp_layer_generic(hw);

	return physical_layer;
}

/**
 * ixgbe_get_bus_info_x550em - Set PCI bus info
 * @hw: pointer to hardware structure
 *
 * Sets bus link width and speed to unknown because X550em is
 * not a PCI device.
 **/
int32_t ixgbe_get_bus_info_X550em(struct ixgbe_hw *hw)
{

	DEBUGFUNC("ixgbe_get_bus_info_x550em");

	hw->bus.width = ixgbe_bus_width_unknown;
	hw->bus.speed = ixgbe_bus_speed_unknown;

	hw->mac.ops.set_lan_id(hw);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_disable_rx_x550 - Disable RX unit
 *
 * Enables the Rx DMA unit for x550
 **/
void ixgbe_disable_rx_x550(struct ixgbe_hw *hw)
{
	uint32_t rxctrl;
	int32_t status;
	struct ixgbe_hic_disable_rxen fw_cmd;

	DEBUGFUNC("ixgbe_disable_rx_dma_x550");

	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (rxctrl & IXGBE_RXCTRL_RXEN) {
		fw_cmd.hdr.cmd = FW_DISABLE_RXEN_CMD;
		fw_cmd.hdr.buf_len = FW_DISABLE_RXEN_LEN;
		fw_cmd.hdr.checksum = FW_DEFAULT_CHECKSUM;
		fw_cmd.port_number = (uint8_t)hw->bus.lan_id;

		status = ixgbe_host_interface_command(hw, (uint32_t *)&fw_cmd,
					sizeof(struct ixgbe_hic_disable_rxen),
					IXGBE_HI_COMMAND_TIMEOUT, TRUE);

		/* If we fail - disable RX using register write */
		if (status) {
			rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
			if (rxctrl & IXGBE_RXCTRL_RXEN) {
				rxctrl &= ~IXGBE_RXCTRL_RXEN;
				IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl);
			}
		}
	}
}

/**
 * ixgbe_get_lcd_x550em - Determine lowest common denominator
 *  @hw: pointer to hardware structure
 *  @lcd_speed: pointer to lowest common link speed
 *
 * Determine lowest common link speed with link partner.
 **/
int32_t ixgbe_get_lcd_t_x550em(struct ixgbe_hw *hw, ixgbe_link_speed *lcd_speed)
{
	uint16_t an_lp_status;
	int32_t status;
	uint16_t word = hw->eeprom.ctrl_word_3;

	*lcd_speed = IXGBE_LINK_SPEED_UNKNOWN;

	status = hw->phy.ops.read_reg(hw, IXGBE_AUTO_NEG_LP_STATUS,
				      IXGBE_MDIO_AUTO_NEG_DEV_TYPE,
				      &an_lp_status);

	if (status != IXGBE_SUCCESS)
		return status;

	/* If link partner advertised 1G, return 1G */
	if (an_lp_status & IXGBE_AUTO_NEG_LP_1000BASE_CAP) {
		*lcd_speed = IXGBE_LINK_SPEED_1GB_FULL;
		return status;
	}

	/* If 10G disabled for LPLU via NVM D10GMP, then return no valid LCD */
	if ((hw->bus.lan_id && (word & NVM_INIT_CTRL_3_D10GMP_PORT1)) ||
	    (word & NVM_INIT_CTRL_3_D10GMP_PORT0))
		return status;

	/* Link partner not capable of lower speeds, return 10G */
	*lcd_speed = IXGBE_LINK_SPEED_10GB_FULL;
	return status;
}

/**
 *  ixgbe_setup_fc_X550em - Set up flow control
 *  @hw: pointer to hardware structure
 *
 *  Called at init time to set up flow control.
 **/
int32_t ixgbe_setup_fc_X550em(struct ixgbe_hw *hw)
{
	int32_t ret_val = IXGBE_SUCCESS;
	uint32_t pause, asm_dir, reg_val;

	DEBUGFUNC("ixgbe_setup_fc_X550em");

	/* Validate the requested mode */
	if (hw->fc.strict_ieee && hw->fc.requested_mode == ixgbe_fc_rx_pause) {
		DEBUGOUT("ixgbe_fc_rx_pause not valid in strict IEEE mode\n");
		ret_val = IXGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	/* 10gig parts do not have a word in the EEPROM to determine the
	 * default flow control setting, so we explicitly set it to full.
	 */
	if (hw->fc.requested_mode == ixgbe_fc_default)
		hw->fc.requested_mode = ixgbe_fc_full;

	/* Determine PAUSE and ASM_DIR bits. */
	switch (hw->fc.requested_mode) {
	case ixgbe_fc_none:
		pause = 0;
		asm_dir = 0;
		break;
	case ixgbe_fc_tx_pause:
		pause = 0;
		asm_dir = 1;
		break;
	case ixgbe_fc_rx_pause:
		/* Rx Flow control is enabled and Tx Flow control is
		 * disabled by software override. Since there really
		 * isn't a way to advertise that we are capable of RX
		 * Pause ONLY, we will advertise that we support both
		 * symmetric and asymmetric Rx PAUSE, as such we fall
		 * through to the fc_full statement.  Later, we will
		 * disable the adapter's ability to send PAUSE frames.
		 */
	case ixgbe_fc_full:
		pause = 1;
		asm_dir = 1;
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		ret_val = IXGBE_ERR_CONFIG;
		goto out;
	}

	if (hw->device_id == IXGBE_DEV_ID_X550EM_X_KR) {
		ret_val = ixgbe_read_iosf_sb_reg_x550(hw,
			IXGBE_KRM_AN_CNTL_1(hw->bus.lan_id),
			IXGBE_SB_IOSF_TARGET_KR_PHY, &reg_val);
		if (ret_val != IXGBE_SUCCESS)
			goto out;
		reg_val &= ~(IXGBE_KRM_AN_CNTL_1_SYM_PAUSE |
			IXGBE_KRM_AN_CNTL_1_ASM_PAUSE);
		if (pause)
			reg_val |= IXGBE_KRM_AN_CNTL_1_SYM_PAUSE;
		if (asm_dir)
			reg_val |= IXGBE_KRM_AN_CNTL_1_ASM_PAUSE;
		ret_val = ixgbe_write_iosf_sb_reg_x550(hw,
			IXGBE_KRM_AN_CNTL_1(hw->bus.lan_id),
			IXGBE_SB_IOSF_TARGET_KR_PHY, reg_val);

		/* This device does not fully support AN. */
		hw->fc.disable_fc_autoneg = TRUE;
	}

out:
	return ret_val;
}

/**
 * ixgbe_set_mux - Set mux for port 1 access with CS4227
 * @hw: pointer to hardware structure
 * @state: set mux if 1, clear if 0
 */
void ixgbe_set_mux(struct ixgbe_hw *hw, uint8_t state)
{
	uint32_t esdp;

	if (!hw->bus.lan_id)
		return;
	esdp = IXGBE_READ_REG(hw, IXGBE_ESDP);
	if (state)
		esdp |= IXGBE_ESDP_SDP1;
	else
		esdp &= ~IXGBE_ESDP_SDP1;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp);
	IXGBE_WRITE_FLUSH(hw);
}

/**
 *  ixgbe_acquire_swfw_sync_X550em - Acquire SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to acquire
 *
 *  Acquires the SWFW semaphore and sets the I2C MUX
 **/
int32_t ixgbe_acquire_swfw_sync_X550em(struct ixgbe_hw *hw, uint32_t mask)
{
	int32_t status;

	DEBUGFUNC("ixgbe_acquire_swfw_sync_X550em");

	status = ixgbe_acquire_swfw_sync_X540(hw, mask);
	if (status)
		return status;

	if (mask & IXGBE_GSSR_I2C_MASK)
		ixgbe_set_mux(hw, 1);

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_release_swfw_sync_X550em - Release SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to release
 *
 *  Releases the SWFW semaphore and sets the I2C MUX
 **/
void ixgbe_release_swfw_sync_X550em(struct ixgbe_hw *hw, uint32_t mask)
{
	DEBUGFUNC("ixgbe_release_swfw_sync_X550em");

	if (mask & IXGBE_GSSR_I2C_MASK)
		ixgbe_set_mux(hw, 0);

	ixgbe_release_swfw_sync_X540(hw, mask);
}

/**
 * ixgbe_handle_lasi_ext_t_x550em - Handle external Base T PHY interrupt
 * @hw: pointer to hardware structure
 *
 * Handle external Base T PHY interrupt. If high temperature
 * failure alarm then return error, else if link status change
 * then setup internal/external PHY link
 *
 * Return IXGBE_ERR_OVERTEMP if interrupt is high temperature
 * failure alarm, else return PHY access status.
 */
int32_t ixgbe_handle_lasi_ext_t_x550em(struct ixgbe_hw *hw)
{
	bool lsc;
	uint32_t status;

	status = ixgbe_get_lasi_ext_t_x550em(hw, &lsc);

	if (status != IXGBE_SUCCESS)
		return status;

	if (lsc && hw->phy.ops.setup_internal_link)
		return hw->phy.ops.setup_internal_link(hw);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_setup_mac_link_t_X550em - Sets the auto advertised link speed
 * @hw: pointer to hardware structure
 * @speed: new link speed
 * @autoneg_wait_to_complete: TRUE when waiting for completion is needed
 *
 * Setup internal/external PHY link speed based on link speed, then set
 * external PHY auto advertised link speed.
 *
 * Returns error status for any failure
 **/
int32_t ixgbe_setup_mac_link_t_X550em(struct ixgbe_hw *hw,
				      ixgbe_link_speed speed,
				      bool autoneg_wait_to_complete)
{
	int32_t status;
	ixgbe_link_speed force_speed;

	DEBUGFUNC("ixgbe_setup_mac_link_t_X550em");

	/* Setup internal/external PHY link speed to iXFI (10G), unless
	 * only 1G is auto advertised then setup KX link.
	 */
	if (speed & IXGBE_LINK_SPEED_10GB_FULL)
		force_speed = IXGBE_LINK_SPEED_10GB_FULL;
	else
		force_speed = IXGBE_LINK_SPEED_1GB_FULL;

	/* If internal link mode is XFI, then setup XFI internal link. */
	if (!(hw->phy.nw_mng_if_sel & IXGBE_NW_MNG_IF_SEL_INT_PHY_MODE)) {
		status = ixgbe_setup_ixfi_x550em(hw, &force_speed);

		if (status != IXGBE_SUCCESS)
			return status;
	}

	return hw->phy.ops.setup_link_speed(hw, speed, autoneg_wait_to_complete);
}

/**
 * ixgbe_check_link_t_X550em - Determine link and speed status
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @link_up: TRUE when link is up
 * @link_up_wait_to_complete: bool used to wait for link up or not
 *
 * Check that both the MAC and X557 external PHY have link.
 **/
int32_t ixgbe_check_link_t_X550em(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
				  bool *link_up, bool link_up_wait_to_complete)
{
	uint32_t status;
	uint16_t autoneg_status;

	if (hw->mac.ops.get_media_type(hw) != ixgbe_media_type_copper)
		return IXGBE_ERR_CONFIG;

	status = ixgbe_check_mac_link_generic(hw, speed, link_up,
					      link_up_wait_to_complete);

	/* If check link fails or MAC link is not up, then return */
	if (status != IXGBE_SUCCESS || !(*link_up))
		return status;

	/* MAC link is up, so check external PHY link.
	 * Read this twice back to back to indicate current status.
	 */
	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_AUTO_NEG_STATUS,
				      IXGBE_MDIO_AUTO_NEG_DEV_TYPE,
				      &autoneg_status);

	if (status != IXGBE_SUCCESS)
		return status;

	status = hw->phy.ops.read_reg(hw, IXGBE_MDIO_AUTO_NEG_STATUS,
				      IXGBE_MDIO_AUTO_NEG_DEV_TYPE,
				      &autoneg_status);

	if (status != IXGBE_SUCCESS)
		return status;

	/* If external PHY link is not up, then indicate link not up */
	if (!(autoneg_status & IXGBE_MDIO_AUTO_NEG_LINK_STATUS))
		*link_up = FALSE;

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_reset_phy_t_X550em - Performs X557 PHY reset and enables LASI
 *  @hw: pointer to hardware structure
 **/
int32_t ixgbe_reset_phy_t_X550em(struct ixgbe_hw *hw)
{
	int32_t status;

	status = ixgbe_reset_phy_generic(hw);

	if (status != IXGBE_SUCCESS)
		return status;

	/* Configure Link Status Alarm and Temperature Threshold interrupts */
	return ixgbe_enable_lasi_ext_t_x550em(hw);
}

/**
 *  ixgbe_led_on_t_X550em - Turns on the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @led_idx: led number to turn on
 **/
int32_t ixgbe_led_on_t_X550em(struct ixgbe_hw *hw, uint32_t led_idx)
{
	uint16_t phy_data;

	DEBUGFUNC("ixgbe_led_on_t_X550em");

	if (led_idx >= IXGBE_X557_MAX_LED_INDEX)
		return IXGBE_ERR_PARAM;

	/* To turn on the LED, set mode to ON. */
	hw->phy.ops.read_reg(hw, IXGBE_X557_LED_PROVISIONING + led_idx,
			     IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE, &phy_data);
	phy_data |= IXGBE_X557_LED_MANUAL_SET_MASK;
	hw->phy.ops.write_reg(hw, IXGBE_X557_LED_PROVISIONING + led_idx,
			      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE, phy_data);

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_led_off_t_X550em - Turns off the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @led_idx: led number to turn off
 **/
int32_t ixgbe_led_off_t_X550em(struct ixgbe_hw *hw, uint32_t led_idx)
{
	uint16_t phy_data;

	DEBUGFUNC("ixgbe_led_off_t_X550em");

	if (led_idx >= IXGBE_X557_MAX_LED_INDEX)
		return IXGBE_ERR_PARAM;

	/* To turn on the LED, set mode to ON. */
	hw->phy.ops.read_reg(hw, IXGBE_X557_LED_PROVISIONING + led_idx,
			     IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE, &phy_data);
	phy_data &= ~IXGBE_X557_LED_MANUAL_SET_MASK;
	hw->phy.ops.write_reg(hw, IXGBE_X557_LED_PROVISIONING + led_idx,
			      IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE, phy_data);

	return IXGBE_SUCCESS;
}
