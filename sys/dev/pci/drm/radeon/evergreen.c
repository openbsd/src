/*	$OpenBSD: evergreen.c,v 1.19 2015/04/18 14:47:35 jsg Exp $	*/
/*
 * Copyright 2010 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Alex Deucher
 */
#include <dev/pci/drm/drmP.h>
#include "radeon.h"
#include "radeon_asic.h"
#include <dev/pci/drm/radeon_drm.h>
#include "evergreend.h"
#include "atom.h"
#include "avivod.h"
#include "evergreen_reg.h"
#include "evergreen_blit_shaders.h"

#define EVERGREEN_PFP_UCODE_SIZE 1120
#define EVERGREEN_PM4_UCODE_SIZE 1376

static const u32 crtc_offsets[6] =
{
	EVERGREEN_CRTC0_REGISTER_OFFSET,
	EVERGREEN_CRTC1_REGISTER_OFFSET,
	EVERGREEN_CRTC2_REGISTER_OFFSET,
	EVERGREEN_CRTC3_REGISTER_OFFSET,
	EVERGREEN_CRTC4_REGISTER_OFFSET,
	EVERGREEN_CRTC5_REGISTER_OFFSET
};

static void evergreen_gpu_init(struct radeon_device *rdev);
void evergreen_fini(struct radeon_device *rdev);
void evergreen_pcie_gen2_enable(struct radeon_device *rdev);
extern void cayman_cp_int_cntl_setup(struct radeon_device *rdev,
				     int ring, u32 cp_int_cntl);

void evergreen_tiling_fields(unsigned tiling_flags, unsigned *bankw,
			     unsigned *bankh, unsigned *mtaspect,
			     unsigned *tile_split)
{
	*bankw = (tiling_flags >> RADEON_TILING_EG_BANKW_SHIFT) & RADEON_TILING_EG_BANKW_MASK;
	*bankh = (tiling_flags >> RADEON_TILING_EG_BANKH_SHIFT) & RADEON_TILING_EG_BANKH_MASK;
	*mtaspect = (tiling_flags >> RADEON_TILING_EG_MACRO_TILE_ASPECT_SHIFT) & RADEON_TILING_EG_MACRO_TILE_ASPECT_MASK;
	*tile_split = (tiling_flags >> RADEON_TILING_EG_TILE_SPLIT_SHIFT) & RADEON_TILING_EG_TILE_SPLIT_MASK;
	switch (*bankw) {
	default:
	case 1: *bankw = EVERGREEN_ADDR_SURF_BANK_WIDTH_1; break;
	case 2: *bankw = EVERGREEN_ADDR_SURF_BANK_WIDTH_2; break;
	case 4: *bankw = EVERGREEN_ADDR_SURF_BANK_WIDTH_4; break;
	case 8: *bankw = EVERGREEN_ADDR_SURF_BANK_WIDTH_8; break;
	}
	switch (*bankh) {
	default:
	case 1: *bankh = EVERGREEN_ADDR_SURF_BANK_HEIGHT_1; break;
	case 2: *bankh = EVERGREEN_ADDR_SURF_BANK_HEIGHT_2; break;
	case 4: *bankh = EVERGREEN_ADDR_SURF_BANK_HEIGHT_4; break;
	case 8: *bankh = EVERGREEN_ADDR_SURF_BANK_HEIGHT_8; break;
	}
	switch (*mtaspect) {
	default:
	case 1: *mtaspect = EVERGREEN_ADDR_SURF_MACRO_TILE_ASPECT_1; break;
	case 2: *mtaspect = EVERGREEN_ADDR_SURF_MACRO_TILE_ASPECT_2; break;
	case 4: *mtaspect = EVERGREEN_ADDR_SURF_MACRO_TILE_ASPECT_4; break;
	case 8: *mtaspect = EVERGREEN_ADDR_SURF_MACRO_TILE_ASPECT_8; break;
	}
}

void evergreen_fix_pci_max_read_req_size(struct radeon_device *rdev)
{
	pcireg_t ctl, v;
	int off;

	if (pci_get_capability(rdev->pc, rdev->pa_tag, PCI_CAP_PCIEXPRESS,
			       &off, &ctl) == 0)
		return;

	ctl = pci_conf_read(rdev->pc, rdev->pa_tag, off + PCI_PCIE_DCSR);

	v = (ctl & PCI_PCIE_DCSR_MPS) >> 12;

	/* if bios or OS sets MAX_READ_REQUEST_SIZE to an invalid value, fix it
	 * to avoid hangs or perfomance issues
	 */
	if ((v == 0) || (v == 6) || (v == 7)) {
		ctl &= ~PCI_PCIE_DCSR_MPS;
		ctl |= (2 << 12);
		pci_conf_write(rdev->pc, rdev->pa_tag, off + PCI_PCIE_DCSR, ctl);
	}
}

static bool dce4_is_in_vblank(struct radeon_device *rdev, int crtc)
{
	if (RREG32(EVERGREEN_CRTC_STATUS + crtc_offsets[crtc]) & EVERGREEN_CRTC_V_BLANK)
		return true;
	else
		return false;
}

static bool dce4_is_counter_moving(struct radeon_device *rdev, int crtc)
{
	u32 pos1, pos2;

	pos1 = RREG32(EVERGREEN_CRTC_STATUS_POSITION + crtc_offsets[crtc]);
	pos2 = RREG32(EVERGREEN_CRTC_STATUS_POSITION + crtc_offsets[crtc]);

	if (pos1 != pos2)
		return true;
	else
		return false;
}

/**
 * dce4_wait_for_vblank - vblank wait asic callback.
 *
 * @rdev: radeon_device pointer
 * @crtc: crtc to wait for vblank on
 *
 * Wait for vblank on the requested crtc (evergreen+).
 */
void dce4_wait_for_vblank(struct radeon_device *rdev, int crtc)
{
	unsigned i = 0;

	if (crtc >= rdev->num_crtc)
		return;

	if (!(RREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[crtc]) & EVERGREEN_CRTC_MASTER_EN))
		return;

	/* depending on when we hit vblank, we may be close to active; if so,
	 * wait for another frame.
	 */
	while (dce4_is_in_vblank(rdev, crtc)) {
		if (i++ % 100 == 0) {
			if (!dce4_is_counter_moving(rdev, crtc))
				break;
		}
	}

	while (!dce4_is_in_vblank(rdev, crtc)) {
		if (i++ % 100 == 0) {
			if (!dce4_is_counter_moving(rdev, crtc))
				break;
		}
	}
}

/**
 * radeon_irq_kms_pflip_irq_get - pre-pageflip callback.
 *
 * @rdev: radeon_device pointer
 * @crtc: crtc to prepare for pageflip on
 *
 * Pre-pageflip callback (evergreen+).
 * Enables the pageflip irq (vblank irq).
 */
void evergreen_pre_page_flip(struct radeon_device *rdev, int crtc)
{
	/* enable the pflip int */
	radeon_irq_kms_pflip_irq_get(rdev, crtc);
}

/**
 * evergreen_post_page_flip - pos-pageflip callback.
 *
 * @rdev: radeon_device pointer
 * @crtc: crtc to cleanup pageflip on
 *
 * Post-pageflip callback (evergreen+).
 * Disables the pageflip irq (vblank irq).
 */
void evergreen_post_page_flip(struct radeon_device *rdev, int crtc)
{
	/* disable the pflip int */
	radeon_irq_kms_pflip_irq_put(rdev, crtc);
}

/**
 * evergreen_page_flip - pageflip callback.
 *
 * @rdev: radeon_device pointer
 * @crtc_id: crtc to cleanup pageflip on
 * @crtc_base: new address of the crtc (GPU MC address)
 *
 * Does the actual pageflip (evergreen+).
 * During vblank we take the crtc lock and wait for the update_pending
 * bit to go high, when it does, we release the lock, and allow the
 * double buffered update to take place.
 * Returns the current update pending status.
 */
u32 evergreen_page_flip(struct radeon_device *rdev, int crtc_id, u64 crtc_base)
{
	struct radeon_crtc *radeon_crtc = rdev->mode_info.crtcs[crtc_id];
	u32 tmp = RREG32(EVERGREEN_GRPH_UPDATE + radeon_crtc->crtc_offset);
	int i;

	/* Lock the graphics update lock */
	tmp |= EVERGREEN_GRPH_UPDATE_LOCK;
	WREG32(EVERGREEN_GRPH_UPDATE + radeon_crtc->crtc_offset, tmp);

	/* update the scanout addresses */
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + radeon_crtc->crtc_offset,
	       upper_32_bits(crtc_base));
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + radeon_crtc->crtc_offset,
	       (u32)crtc_base);

	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + radeon_crtc->crtc_offset,
	       upper_32_bits(crtc_base));
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + radeon_crtc->crtc_offset,
	       (u32)crtc_base);

	/* Wait for update_pending to go high. */
	for (i = 0; i < rdev->usec_timeout; i++) {
		if (RREG32(EVERGREEN_GRPH_UPDATE + radeon_crtc->crtc_offset) & EVERGREEN_GRPH_SURFACE_UPDATE_PENDING)
			break;
		udelay(1);
	}
	DRM_DEBUG("Update pending now high. Unlocking vupdate_lock.\n");

	/* Unlock the lock, so double-buffering can take place inside vblank */
	tmp &= ~EVERGREEN_GRPH_UPDATE_LOCK;
	WREG32(EVERGREEN_GRPH_UPDATE + radeon_crtc->crtc_offset, tmp);

	/* Return current update_pending status: */
	return RREG32(EVERGREEN_GRPH_UPDATE + radeon_crtc->crtc_offset) & EVERGREEN_GRPH_SURFACE_UPDATE_PENDING;
}

/* get temperature in millidegrees */
int evergreen_get_temp(struct radeon_device *rdev)
{
	u32 temp, toffset;
	int actual_temp = 0;

	if (rdev->family == CHIP_JUNIPER) {
		toffset = (RREG32(CG_THERMAL_CTRL) & TOFFSET_MASK) >>
			TOFFSET_SHIFT;
		temp = (RREG32(CG_TS0_STATUS) & TS0_ADC_DOUT_MASK) >>
			TS0_ADC_DOUT_SHIFT;

		if (toffset & 0x100)
			actual_temp = temp / 2 - (0x200 - toffset);
		else
			actual_temp = temp / 2 + toffset;

		actual_temp = actual_temp * 1000;

	} else {
		temp = (RREG32(CG_MULT_THERMAL_STATUS) & ASIC_T_MASK) >>
			ASIC_T_SHIFT;

		if (temp & 0x400)
			actual_temp = -256;
		else if (temp & 0x200)
			actual_temp = 255;
		else if (temp & 0x100) {
			actual_temp = temp & 0x1ff;
			actual_temp |= ~0x1ff;
		} else
			actual_temp = temp & 0xff;

		actual_temp = (actual_temp * 1000) / 2;
	}

	return actual_temp;
}

int sumo_get_temp(struct radeon_device *rdev)
{
	u32 temp = RREG32(CG_THERMAL_STATUS) & 0xff;
	int actual_temp = temp - 49;

	return actual_temp * 1000;
}

/**
 * sumo_pm_init_profile - Initialize power profiles callback.
 *
 * @rdev: radeon_device pointer
 *
 * Initialize the power states used in profile mode
 * (sumo, trinity, SI).
 * Used for profile mode only.
 */
void sumo_pm_init_profile(struct radeon_device *rdev)
{
	int idx;

	/* default */
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_off_ps_idx = rdev->pm.default_power_state_index;
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_on_ps_idx = rdev->pm.default_power_state_index;
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_on_cm_idx = 0;

	/* low,mid sh/mh */
	if (rdev->flags & RADEON_IS_MOBILITY)
		idx = radeon_pm_get_type_index(rdev, POWER_STATE_TYPE_BATTERY, 0);
	else
		idx = radeon_pm_get_type_index(rdev, POWER_STATE_TYPE_PERFORMANCE, 0);

	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_on_cm_idx = 0;

	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_on_cm_idx = 0;

	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_on_cm_idx = 0;

	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_on_cm_idx = 0;

	/* high sh/mh */
	idx = radeon_pm_get_type_index(rdev, POWER_STATE_TYPE_PERFORMANCE, 0);
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_on_cm_idx =
		rdev->pm.power_state[idx].num_clock_modes - 1;

	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_on_cm_idx =
		rdev->pm.power_state[idx].num_clock_modes - 1;
}

/**
 * btc_pm_init_profile - Initialize power profiles callback.
 *
 * @rdev: radeon_device pointer
 *
 * Initialize the power states used in profile mode
 * (BTC, cayman).
 * Used for profile mode only.
 */
void btc_pm_init_profile(struct radeon_device *rdev)
{
	int idx;

	/* default */
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_off_ps_idx = rdev->pm.default_power_state_index;
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_on_ps_idx = rdev->pm.default_power_state_index;
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_DEFAULT_IDX].dpms_on_cm_idx = 2;
	/* starting with BTC, there is one state that is used for both
	 * MH and SH.  Difference is that we always use the high clock index for
	 * mclk.
	 */
	if (rdev->flags & RADEON_IS_MOBILITY)
		idx = radeon_pm_get_type_index(rdev, POWER_STATE_TYPE_BATTERY, 0);
	else
		idx = radeon_pm_get_type_index(rdev, POWER_STATE_TYPE_PERFORMANCE, 0);
	/* low sh */
	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_LOW_SH_IDX].dpms_on_cm_idx = 0;
	/* mid sh */
	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_MID_SH_IDX].dpms_on_cm_idx = 1;
	/* high sh */
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_HIGH_SH_IDX].dpms_on_cm_idx = 2;
	/* low mh */
	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_LOW_MH_IDX].dpms_on_cm_idx = 0;
	/* mid mh */
	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_MID_MH_IDX].dpms_on_cm_idx = 1;
	/* high mh */
	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_off_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_on_ps_idx = idx;
	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_off_cm_idx = 0;
	rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_on_cm_idx = 2;
}

/**
 * evergreen_pm_misc - set additional pm hw parameters callback.
 *
 * @rdev: radeon_device pointer
 *
 * Set non-clock parameters associated with a power state
 * (voltage, etc.) (evergreen+).
 */
void evergreen_pm_misc(struct radeon_device *rdev)
{
	int req_ps_idx = rdev->pm.requested_power_state_index;
	int req_cm_idx = rdev->pm.requested_clock_mode_index;
	struct radeon_power_state *ps = &rdev->pm.power_state[req_ps_idx];
	struct radeon_voltage *voltage = &ps->clock_info[req_cm_idx].voltage;

	if (voltage->type == VOLTAGE_SW) {
		/* 0xff01 is a flag rather then an actual voltage */
		if (voltage->voltage == 0xff01)
			return;
		if (voltage->voltage && (voltage->voltage != rdev->pm.current_vddc)) {
			radeon_atom_set_voltage(rdev, voltage->voltage, SET_VOLTAGE_TYPE_ASIC_VDDC);
			rdev->pm.current_vddc = voltage->voltage;
			DRM_DEBUG("Setting: vddc: %d\n", voltage->voltage);
		}

		/* starting with BTC, there is one state that is used for both
		 * MH and SH.  Difference is that we always use the high clock index for
		 * mclk and vddci.
		 */
		if ((rdev->pm.pm_method == PM_METHOD_PROFILE) &&
		    (rdev->family >= CHIP_BARTS) &&
		    rdev->pm.active_crtc_count &&
		    ((rdev->pm.profile_index == PM_PROFILE_MID_MH_IDX) ||
		     (rdev->pm.profile_index == PM_PROFILE_LOW_MH_IDX)))
			voltage = &rdev->pm.power_state[req_ps_idx].
				clock_info[rdev->pm.profiles[PM_PROFILE_HIGH_MH_IDX].dpms_on_cm_idx].voltage;

		/* 0xff01 is a flag rather then an actual voltage */
		if (voltage->vddci == 0xff01)
			return;
		if (voltage->vddci && (voltage->vddci != rdev->pm.current_vddci)) {
			radeon_atom_set_voltage(rdev, voltage->vddci, SET_VOLTAGE_TYPE_ASIC_VDDCI);
			rdev->pm.current_vddci = voltage->vddci;
			DRM_DEBUG("Setting: vddci: %d\n", voltage->vddci);
		}
	}
}

/**
 * evergreen_pm_prepare - pre-power state change callback.
 *
 * @rdev: radeon_device pointer
 *
 * Prepare for a power state change (evergreen+).
 */
void evergreen_pm_prepare(struct radeon_device *rdev)
{
	struct drm_device *ddev = rdev->ddev;
	struct drm_crtc *crtc;
	struct radeon_crtc *radeon_crtc;
	u32 tmp;

	/* disable any active CRTCs */
	list_for_each_entry(crtc, &ddev->mode_config.crtc_list, head) {
		radeon_crtc = to_radeon_crtc(crtc);
		if (radeon_crtc->enabled) {
			tmp = RREG32(EVERGREEN_CRTC_CONTROL + radeon_crtc->crtc_offset);
			tmp |= EVERGREEN_CRTC_DISP_READ_REQUEST_DISABLE;
			WREG32(EVERGREEN_CRTC_CONTROL + radeon_crtc->crtc_offset, tmp);
		}
	}
}

/**
 * evergreen_pm_finish - post-power state change callback.
 *
 * @rdev: radeon_device pointer
 *
 * Clean up after a power state change (evergreen+).
 */
void evergreen_pm_finish(struct radeon_device *rdev)
{
	struct drm_device *ddev = rdev->ddev;
	struct drm_crtc *crtc;
	struct radeon_crtc *radeon_crtc;
	u32 tmp;

	/* enable any active CRTCs */
	list_for_each_entry(crtc, &ddev->mode_config.crtc_list, head) {
		radeon_crtc = to_radeon_crtc(crtc);
		if (radeon_crtc->enabled) {
			tmp = RREG32(EVERGREEN_CRTC_CONTROL + radeon_crtc->crtc_offset);
			tmp &= ~EVERGREEN_CRTC_DISP_READ_REQUEST_DISABLE;
			WREG32(EVERGREEN_CRTC_CONTROL + radeon_crtc->crtc_offset, tmp);
		}
	}
}

/**
 * evergreen_hpd_sense - hpd sense callback.
 *
 * @rdev: radeon_device pointer
 * @hpd: hpd (hotplug detect) pin
 *
 * Checks if a digital monitor is connected (evergreen+).
 * Returns true if connected, false if not connected.
 */
bool evergreen_hpd_sense(struct radeon_device *rdev, enum radeon_hpd_id hpd)
{
	bool connected = false;

	switch (hpd) {
	case RADEON_HPD_1:
		if (RREG32(DC_HPD1_INT_STATUS) & DC_HPDx_SENSE)
			connected = true;
		break;
	case RADEON_HPD_2:
		if (RREG32(DC_HPD2_INT_STATUS) & DC_HPDx_SENSE)
			connected = true;
		break;
	case RADEON_HPD_3:
		if (RREG32(DC_HPD3_INT_STATUS) & DC_HPDx_SENSE)
			connected = true;
		break;
	case RADEON_HPD_4:
		if (RREG32(DC_HPD4_INT_STATUS) & DC_HPDx_SENSE)
			connected = true;
		break;
	case RADEON_HPD_5:
		if (RREG32(DC_HPD5_INT_STATUS) & DC_HPDx_SENSE)
			connected = true;
		break;
	case RADEON_HPD_6:
		if (RREG32(DC_HPD6_INT_STATUS) & DC_HPDx_SENSE)
			connected = true;
			break;
	default:
		break;
	}

	return connected;
}

/**
 * evergreen_hpd_set_polarity - hpd set polarity callback.
 *
 * @rdev: radeon_device pointer
 * @hpd: hpd (hotplug detect) pin
 *
 * Set the polarity of the hpd pin (evergreen+).
 */
void evergreen_hpd_set_polarity(struct radeon_device *rdev,
				enum radeon_hpd_id hpd)
{
	u32 tmp;
	bool connected = evergreen_hpd_sense(rdev, hpd);

	switch (hpd) {
	case RADEON_HPD_1:
		tmp = RREG32(DC_HPD1_INT_CONTROL);
		if (connected)
			tmp &= ~DC_HPDx_INT_POLARITY;
		else
			tmp |= DC_HPDx_INT_POLARITY;
		WREG32(DC_HPD1_INT_CONTROL, tmp);
		break;
	case RADEON_HPD_2:
		tmp = RREG32(DC_HPD2_INT_CONTROL);
		if (connected)
			tmp &= ~DC_HPDx_INT_POLARITY;
		else
			tmp |= DC_HPDx_INT_POLARITY;
		WREG32(DC_HPD2_INT_CONTROL, tmp);
		break;
	case RADEON_HPD_3:
		tmp = RREG32(DC_HPD3_INT_CONTROL);
		if (connected)
			tmp &= ~DC_HPDx_INT_POLARITY;
		else
			tmp |= DC_HPDx_INT_POLARITY;
		WREG32(DC_HPD3_INT_CONTROL, tmp);
		break;
	case RADEON_HPD_4:
		tmp = RREG32(DC_HPD4_INT_CONTROL);
		if (connected)
			tmp &= ~DC_HPDx_INT_POLARITY;
		else
			tmp |= DC_HPDx_INT_POLARITY;
		WREG32(DC_HPD4_INT_CONTROL, tmp);
		break;
	case RADEON_HPD_5:
		tmp = RREG32(DC_HPD5_INT_CONTROL);
		if (connected)
			tmp &= ~DC_HPDx_INT_POLARITY;
		else
			tmp |= DC_HPDx_INT_POLARITY;
		WREG32(DC_HPD5_INT_CONTROL, tmp);
			break;
	case RADEON_HPD_6:
		tmp = RREG32(DC_HPD6_INT_CONTROL);
		if (connected)
			tmp &= ~DC_HPDx_INT_POLARITY;
		else
			tmp |= DC_HPDx_INT_POLARITY;
		WREG32(DC_HPD6_INT_CONTROL, tmp);
		break;
	default:
		break;
	}
}

/**
 * evergreen_hpd_init - hpd setup callback.
 *
 * @rdev: radeon_device pointer
 *
 * Setup the hpd pins used by the card (evergreen+).
 * Enable the pin, set the polarity, and enable the hpd interrupts.
 */
void evergreen_hpd_init(struct radeon_device *rdev)
{
	struct drm_device *dev = rdev->ddev;
	struct drm_connector *connector;
	unsigned enabled = 0;
	u32 tmp = DC_HPDx_CONNECTION_TIMER(0x9c4) |
		DC_HPDx_RX_INT_TIMER(0xfa) | DC_HPDx_EN;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		struct radeon_connector *radeon_connector = to_radeon_connector(connector);

		if (connector->connector_type == DRM_MODE_CONNECTOR_eDP ||
		    connector->connector_type == DRM_MODE_CONNECTOR_LVDS) {
			/* don't try to enable hpd on eDP or LVDS avoid breaking the
			 * aux dp channel on imac and help (but not completely fix)
			 * https://bugzilla.redhat.com/show_bug.cgi?id=726143
			 * also avoid interrupt storms during dpms.
			 */
			continue;
		}
		switch (radeon_connector->hpd.hpd) {
		case RADEON_HPD_1:
			WREG32(DC_HPD1_CONTROL, tmp);
			break;
		case RADEON_HPD_2:
			WREG32(DC_HPD2_CONTROL, tmp);
			break;
		case RADEON_HPD_3:
			WREG32(DC_HPD3_CONTROL, tmp);
			break;
		case RADEON_HPD_4:
			WREG32(DC_HPD4_CONTROL, tmp);
			break;
		case RADEON_HPD_5:
			WREG32(DC_HPD5_CONTROL, tmp);
			break;
		case RADEON_HPD_6:
			WREG32(DC_HPD6_CONTROL, tmp);
			break;
		default:
			break;
		}
		radeon_hpd_set_polarity(rdev, radeon_connector->hpd.hpd);
		enabled |= 1 << radeon_connector->hpd.hpd;
	}
	radeon_irq_kms_enable_hpd(rdev, enabled);
}

/**
 * evergreen_hpd_fini - hpd tear down callback.
 *
 * @rdev: radeon_device pointer
 *
 * Tear down the hpd pins used by the card (evergreen+).
 * Disable the hpd interrupts.
 */
void evergreen_hpd_fini(struct radeon_device *rdev)
{
	struct drm_device *dev = rdev->ddev;
	struct drm_connector *connector;
	unsigned disabled = 0;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		struct radeon_connector *radeon_connector = to_radeon_connector(connector);
		switch (radeon_connector->hpd.hpd) {
		case RADEON_HPD_1:
			WREG32(DC_HPD1_CONTROL, 0);
			break;
		case RADEON_HPD_2:
			WREG32(DC_HPD2_CONTROL, 0);
			break;
		case RADEON_HPD_3:
			WREG32(DC_HPD3_CONTROL, 0);
			break;
		case RADEON_HPD_4:
			WREG32(DC_HPD4_CONTROL, 0);
			break;
		case RADEON_HPD_5:
			WREG32(DC_HPD5_CONTROL, 0);
			break;
		case RADEON_HPD_6:
			WREG32(DC_HPD6_CONTROL, 0);
			break;
		default:
			break;
		}
		disabled |= 1 << radeon_connector->hpd.hpd;
	}
	radeon_irq_kms_disable_hpd(rdev, disabled);
}

/* watermark setup */

static u32 evergreen_line_buffer_adjust(struct radeon_device *rdev,
					struct radeon_crtc *radeon_crtc,
					struct drm_display_mode *mode,
					struct drm_display_mode *other_mode)
{
	u32 tmp, buffer_alloc, i;
	u32 pipe_offset = radeon_crtc->crtc_id * 0x20;
	/*
	 * Line Buffer Setup
	 * There are 3 line buffers, each one shared by 2 display controllers.
	 * DC_LB_MEMORY_SPLIT controls how that line buffer is shared between
	 * the display controllers.  The paritioning is done via one of four
	 * preset allocations specified in bits 2:0:
	 * first display controller
	 *  0 - first half of lb (3840 * 2)
	 *  1 - first 3/4 of lb (5760 * 2)
	 *  2 - whole lb (7680 * 2), other crtc must be disabled
	 *  3 - first 1/4 of lb (1920 * 2)
	 * second display controller
	 *  4 - second half of lb (3840 * 2)
	 *  5 - second 3/4 of lb (5760 * 2)
	 *  6 - whole lb (7680 * 2), other crtc must be disabled
	 *  7 - last 1/4 of lb (1920 * 2)
	 */
	/* this can get tricky if we have two large displays on a paired group
	 * of crtcs.  Ideally for multiple large displays we'd assign them to
	 * non-linked crtcs for maximum line buffer allocation.
	 */
	if (radeon_crtc->base.enabled && mode) {
		if (other_mode) {
			tmp = 0; /* 1/2 */
			buffer_alloc = 1;
		} else {
			tmp = 2; /* whole */
			buffer_alloc = 2;
		}
	} else {
		tmp = 0;
		buffer_alloc = 0;
	}

	/* second controller of the pair uses second half of the lb */
	if (radeon_crtc->crtc_id % 2)
		tmp += 4;
	WREG32(DC_LB_MEMORY_SPLIT + radeon_crtc->crtc_offset, tmp);

	if (ASIC_IS_DCE41(rdev) || ASIC_IS_DCE5(rdev)) {
		WREG32(PIPE0_DMIF_BUFFER_CONTROL + pipe_offset,
		       DMIF_BUFFERS_ALLOCATED(buffer_alloc));
		for (i = 0; i < rdev->usec_timeout; i++) {
			if (RREG32(PIPE0_DMIF_BUFFER_CONTROL + pipe_offset) &
			    DMIF_BUFFERS_ALLOCATED_COMPLETED)
				break;
			udelay(1);
		}
	}

	if (radeon_crtc->base.enabled && mode) {
		switch (tmp) {
		case 0:
		case 4:
		default:
			if (ASIC_IS_DCE5(rdev))
				return 4096 * 2;
			else
				return 3840 * 2;
		case 1:
		case 5:
			if (ASIC_IS_DCE5(rdev))
				return 6144 * 2;
			else
				return 5760 * 2;
		case 2:
		case 6:
			if (ASIC_IS_DCE5(rdev))
				return 8192 * 2;
			else
				return 7680 * 2;
		case 3:
		case 7:
			if (ASIC_IS_DCE5(rdev))
				return 2048 * 2;
			else
				return 1920 * 2;
		}
	}

	/* controller not enabled, so no lb used */
	return 0;
}

u32 evergreen_get_number_of_dram_channels(struct radeon_device *rdev)
{
	u32 tmp = RREG32(MC_SHARED_CHMAP);

	switch ((tmp & NOOFCHAN_MASK) >> NOOFCHAN_SHIFT) {
	case 0:
	default:
		return 1;
	case 1:
		return 2;
	case 2:
		return 4;
	case 3:
		return 8;
	}
}

struct evergreen_wm_params {
	u32 dram_channels; /* number of dram channels */
	u32 yclk;          /* bandwidth per dram data pin in kHz */
	u32 sclk;          /* engine clock in kHz */
	u32 disp_clk;      /* display clock in kHz */
	u32 src_width;     /* viewport width */
	u32 active_time;   /* active display time in ns */
	u32 blank_time;    /* blank time in ns */
	bool interlaced;    /* mode is interlaced */
	fixed20_12 vsc;    /* vertical scale ratio */
	u32 num_heads;     /* number of active crtcs */
	u32 bytes_per_pixel; /* bytes per pixel display + overlay */
	u32 lb_size;       /* line buffer allocated to pipe */
	u32 vtaps;         /* vertical scaler taps */
};

static u32 evergreen_dram_bandwidth(struct evergreen_wm_params *wm)
{
	/* Calculate DRAM Bandwidth and the part allocated to display. */
	fixed20_12 dram_efficiency; /* 0.7 */
	fixed20_12 yclk, dram_channels, bandwidth;
	fixed20_12 a;

	a.full = dfixed_const(1000);
	yclk.full = dfixed_const(wm->yclk);
	yclk.full = dfixed_div(yclk, a);
	dram_channels.full = dfixed_const(wm->dram_channels * 4);
	a.full = dfixed_const(10);
	dram_efficiency.full = dfixed_const(7);
	dram_efficiency.full = dfixed_div(dram_efficiency, a);
	bandwidth.full = dfixed_mul(dram_channels, yclk);
	bandwidth.full = dfixed_mul(bandwidth, dram_efficiency);

	return dfixed_trunc(bandwidth);
}

static u32 evergreen_dram_bandwidth_for_display(struct evergreen_wm_params *wm)
{
	/* Calculate DRAM Bandwidth and the part allocated to display. */
	fixed20_12 disp_dram_allocation; /* 0.3 to 0.7 */
	fixed20_12 yclk, dram_channels, bandwidth;
	fixed20_12 a;

	a.full = dfixed_const(1000);
	yclk.full = dfixed_const(wm->yclk);
	yclk.full = dfixed_div(yclk, a);
	dram_channels.full = dfixed_const(wm->dram_channels * 4);
	a.full = dfixed_const(10);
	disp_dram_allocation.full = dfixed_const(3); /* XXX worse case value 0.3 */
	disp_dram_allocation.full = dfixed_div(disp_dram_allocation, a);
	bandwidth.full = dfixed_mul(dram_channels, yclk);
	bandwidth.full = dfixed_mul(bandwidth, disp_dram_allocation);

	return dfixed_trunc(bandwidth);
}

static u32 evergreen_data_return_bandwidth(struct evergreen_wm_params *wm)
{
	/* Calculate the display Data return Bandwidth */
	fixed20_12 return_efficiency; /* 0.8 */
	fixed20_12 sclk, bandwidth;
	fixed20_12 a;

	a.full = dfixed_const(1000);
	sclk.full = dfixed_const(wm->sclk);
	sclk.full = dfixed_div(sclk, a);
	a.full = dfixed_const(10);
	return_efficiency.full = dfixed_const(8);
	return_efficiency.full = dfixed_div(return_efficiency, a);
	a.full = dfixed_const(32);
	bandwidth.full = dfixed_mul(a, sclk);
	bandwidth.full = dfixed_mul(bandwidth, return_efficiency);

	return dfixed_trunc(bandwidth);
}

static u32 evergreen_dmif_request_bandwidth(struct evergreen_wm_params *wm)
{
	/* Calculate the DMIF Request Bandwidth */
	fixed20_12 disp_clk_request_efficiency; /* 0.8 */
	fixed20_12 disp_clk, bandwidth;
	fixed20_12 a;

	a.full = dfixed_const(1000);
	disp_clk.full = dfixed_const(wm->disp_clk);
	disp_clk.full = dfixed_div(disp_clk, a);
	a.full = dfixed_const(10);
	disp_clk_request_efficiency.full = dfixed_const(8);
	disp_clk_request_efficiency.full = dfixed_div(disp_clk_request_efficiency, a);
	a.full = dfixed_const(32);
	bandwidth.full = dfixed_mul(a, disp_clk);
	bandwidth.full = dfixed_mul(bandwidth, disp_clk_request_efficiency);

	return dfixed_trunc(bandwidth);
}

static u32 evergreen_available_bandwidth(struct evergreen_wm_params *wm)
{
	/* Calculate the Available bandwidth. Display can use this temporarily but not in average. */
	u32 dram_bandwidth = evergreen_dram_bandwidth(wm);
	u32 data_return_bandwidth = evergreen_data_return_bandwidth(wm);
	u32 dmif_req_bandwidth = evergreen_dmif_request_bandwidth(wm);

	return min(dram_bandwidth, min(data_return_bandwidth, dmif_req_bandwidth));
}

static u32 evergreen_average_bandwidth(struct evergreen_wm_params *wm)
{
	/* Calculate the display mode Average Bandwidth
	 * DisplayMode should contain the source and destination dimensions,
	 * timing, etc.
	 */
	fixed20_12 bpp;
	fixed20_12 line_time;
	fixed20_12 src_width;
	fixed20_12 bandwidth;
	fixed20_12 a;

	a.full = dfixed_const(1000);
	line_time.full = dfixed_const(wm->active_time + wm->blank_time);
	line_time.full = dfixed_div(line_time, a);
	bpp.full = dfixed_const(wm->bytes_per_pixel);
	src_width.full = dfixed_const(wm->src_width);
	bandwidth.full = dfixed_mul(src_width, bpp);
	bandwidth.full = dfixed_mul(bandwidth, wm->vsc);
	bandwidth.full = dfixed_div(bandwidth, line_time);

	return dfixed_trunc(bandwidth);
}

static u32 evergreen_latency_watermark(struct evergreen_wm_params *wm)
{
	/* First calcualte the latency in ns */
	u32 mc_latency = 2000; /* 2000 ns. */
	u32 available_bandwidth = evergreen_available_bandwidth(wm);
	u32 worst_chunk_return_time = (512 * 8 * 1000) / available_bandwidth;
	u32 cursor_line_pair_return_time = (128 * 4 * 1000) / available_bandwidth;
	u32 dc_latency = 40000000 / wm->disp_clk; /* dc pipe latency */
	u32 other_heads_data_return_time = ((wm->num_heads + 1) * worst_chunk_return_time) +
		(wm->num_heads * cursor_line_pair_return_time);
	u32 latency = mc_latency + other_heads_data_return_time + dc_latency;
	u32 max_src_lines_per_dst_line, lb_fill_bw, line_fill_time;
	fixed20_12 a, b, c;

	if (wm->num_heads == 0)
		return 0;

	a.full = dfixed_const(2);
	b.full = dfixed_const(1);
	if ((wm->vsc.full > a.full) ||
	    ((wm->vsc.full > b.full) && (wm->vtaps >= 3)) ||
	    (wm->vtaps >= 5) ||
	    ((wm->vsc.full >= a.full) && wm->interlaced))
		max_src_lines_per_dst_line = 4;
	else
		max_src_lines_per_dst_line = 2;

	a.full = dfixed_const(available_bandwidth);
	b.full = dfixed_const(wm->num_heads);
	a.full = dfixed_div(a, b);

	b.full = dfixed_const(1000);
	c.full = dfixed_const(wm->disp_clk);
	b.full = dfixed_div(c, b);
	c.full = dfixed_const(wm->bytes_per_pixel);
	b.full = dfixed_mul(b, c);

	lb_fill_bw = min(dfixed_trunc(a), dfixed_trunc(b));

	a.full = dfixed_const(max_src_lines_per_dst_line * wm->src_width * wm->bytes_per_pixel);
	b.full = dfixed_const(1000);
	c.full = dfixed_const(lb_fill_bw);
	b.full = dfixed_div(c, b);
	a.full = dfixed_div(a, b);
	line_fill_time = dfixed_trunc(a);

	if (line_fill_time < wm->active_time)
		return latency;
	else
		return latency + (line_fill_time - wm->active_time);

}

static bool evergreen_average_bandwidth_vs_dram_bandwidth_for_display(struct evergreen_wm_params *wm)
{
	if (evergreen_average_bandwidth(wm) <=
	    (evergreen_dram_bandwidth_for_display(wm) / wm->num_heads))
		return true;
	else
		return false;
};

static bool evergreen_average_bandwidth_vs_available_bandwidth(struct evergreen_wm_params *wm)
{
	if (evergreen_average_bandwidth(wm) <=
	    (evergreen_available_bandwidth(wm) / wm->num_heads))
		return true;
	else
		return false;
};

static bool evergreen_check_latency_hiding(struct evergreen_wm_params *wm)
{
	u32 lb_partitions = wm->lb_size / wm->src_width;
	u32 line_time = wm->active_time + wm->blank_time;
	u32 latency_tolerant_lines;
	u32 latency_hiding;
	fixed20_12 a;

	a.full = dfixed_const(1);
	if (wm->vsc.full > a.full)
		latency_tolerant_lines = 1;
	else {
		if (lb_partitions <= (wm->vtaps + 1))
			latency_tolerant_lines = 1;
		else
			latency_tolerant_lines = 2;
	}

	latency_hiding = (latency_tolerant_lines * line_time + wm->blank_time);

	if (evergreen_latency_watermark(wm) <= latency_hiding)
		return true;
	else
		return false;
}

static void evergreen_program_watermarks(struct radeon_device *rdev,
					 struct radeon_crtc *radeon_crtc,
					 u32 lb_size, u32 num_heads)
{
	struct drm_display_mode *mode = &radeon_crtc->base.mode;
	struct evergreen_wm_params wm;
	u32 pixel_period;
	u32 line_time = 0;
	u32 latency_watermark_a = 0, latency_watermark_b = 0;
	u32 priority_a_mark = 0, priority_b_mark = 0;
	u32 priority_a_cnt = PRIORITY_OFF;
	u32 priority_b_cnt = PRIORITY_OFF;
	u32 pipe_offset = radeon_crtc->crtc_id * 16;
	u32 tmp, arb_control3;
	fixed20_12 a, b, c;

	if (radeon_crtc->base.enabled && num_heads && mode) {
		pixel_period = 1000000 / (u32)mode->clock;
		line_time = min((u32)mode->crtc_htotal * pixel_period, (u32)65535);
		priority_a_cnt = 0;
		priority_b_cnt = 0;

		wm.yclk = rdev->pm.current_mclk * 10;
		wm.sclk = rdev->pm.current_sclk * 10;
		wm.disp_clk = mode->clock;
		wm.src_width = mode->crtc_hdisplay;
		wm.active_time = mode->crtc_hdisplay * pixel_period;
		wm.blank_time = line_time - wm.active_time;
		wm.interlaced = false;
		if (mode->flags & DRM_MODE_FLAG_INTERLACE)
			wm.interlaced = true;
		wm.vsc = radeon_crtc->vsc;
		wm.vtaps = 1;
		if (radeon_crtc->rmx_type != RMX_OFF)
			wm.vtaps = 2;
		wm.bytes_per_pixel = 4; /* XXX: get this from fb config */
		wm.lb_size = lb_size;
		wm.dram_channels = evergreen_get_number_of_dram_channels(rdev);
		wm.num_heads = num_heads;

		/* set for high clocks */
		latency_watermark_a = min(evergreen_latency_watermark(&wm), (u32)65535);
		/* set for low clocks */
		/* wm.yclk = low clk; wm.sclk = low clk */
		latency_watermark_b = min(evergreen_latency_watermark(&wm), (u32)65535);

		/* possibly force display priority to high */
		/* should really do this at mode validation time... */
		if (!evergreen_average_bandwidth_vs_dram_bandwidth_for_display(&wm) ||
		    !evergreen_average_bandwidth_vs_available_bandwidth(&wm) ||
		    !evergreen_check_latency_hiding(&wm) ||
		    (rdev->disp_priority == 2)) {
			DRM_DEBUG_KMS("force priority to high\n");
			priority_a_cnt |= PRIORITY_ALWAYS_ON;
			priority_b_cnt |= PRIORITY_ALWAYS_ON;
		}

		a.full = dfixed_const(1000);
		b.full = dfixed_const(mode->clock);
		b.full = dfixed_div(b, a);
		c.full = dfixed_const(latency_watermark_a);
		c.full = dfixed_mul(c, b);
		c.full = dfixed_mul(c, radeon_crtc->hsc);
		c.full = dfixed_div(c, a);
		a.full = dfixed_const(16);
		c.full = dfixed_div(c, a);
		priority_a_mark = dfixed_trunc(c);
		priority_a_cnt |= priority_a_mark & PRIORITY_MARK_MASK;

		a.full = dfixed_const(1000);
		b.full = dfixed_const(mode->clock);
		b.full = dfixed_div(b, a);
		c.full = dfixed_const(latency_watermark_b);
		c.full = dfixed_mul(c, b);
		c.full = dfixed_mul(c, radeon_crtc->hsc);
		c.full = dfixed_div(c, a);
		a.full = dfixed_const(16);
		c.full = dfixed_div(c, a);
		priority_b_mark = dfixed_trunc(c);
		priority_b_cnt |= priority_b_mark & PRIORITY_MARK_MASK;
	}

	/* select wm A */
	arb_control3 = RREG32(PIPE0_ARBITRATION_CONTROL3 + pipe_offset);
	tmp = arb_control3;
	tmp &= ~LATENCY_WATERMARK_MASK(3);
	tmp |= LATENCY_WATERMARK_MASK(1);
	WREG32(PIPE0_ARBITRATION_CONTROL3 + pipe_offset, tmp);
	WREG32(PIPE0_LATENCY_CONTROL + pipe_offset,
	       (LATENCY_LOW_WATERMARK(latency_watermark_a) |
		LATENCY_HIGH_WATERMARK(line_time)));
	/* select wm B */
	tmp = RREG32(PIPE0_ARBITRATION_CONTROL3 + pipe_offset);
	tmp &= ~LATENCY_WATERMARK_MASK(3);
	tmp |= LATENCY_WATERMARK_MASK(2);
	WREG32(PIPE0_ARBITRATION_CONTROL3 + pipe_offset, tmp);
	WREG32(PIPE0_LATENCY_CONTROL + pipe_offset,
	       (LATENCY_LOW_WATERMARK(latency_watermark_b) |
		LATENCY_HIGH_WATERMARK(line_time)));
	/* restore original selection */
	WREG32(PIPE0_ARBITRATION_CONTROL3 + pipe_offset, arb_control3);

	/* write the priority marks */
	WREG32(PRIORITY_A_CNT + radeon_crtc->crtc_offset, priority_a_cnt);
	WREG32(PRIORITY_B_CNT + radeon_crtc->crtc_offset, priority_b_cnt);

}

/**
 * evergreen_bandwidth_update - update display watermarks callback.
 *
 * @rdev: radeon_device pointer
 *
 * Update the display watermarks based on the requested mode(s)
 * (evergreen+).
 */
void evergreen_bandwidth_update(struct radeon_device *rdev)
{
	struct drm_display_mode *mode0 = NULL;
	struct drm_display_mode *mode1 = NULL;
	u32 num_heads = 0, lb_size;
	int i;

	radeon_update_display_priority(rdev);

	for (i = 0; i < rdev->num_crtc; i++) {
		if (rdev->mode_info.crtcs[i]->base.enabled)
			num_heads++;
	}
	for (i = 0; i < rdev->num_crtc; i += 2) {
		mode0 = &rdev->mode_info.crtcs[i]->base.mode;
		mode1 = &rdev->mode_info.crtcs[i+1]->base.mode;
		lb_size = evergreen_line_buffer_adjust(rdev, rdev->mode_info.crtcs[i], mode0, mode1);
		evergreen_program_watermarks(rdev, rdev->mode_info.crtcs[i], lb_size, num_heads);
		lb_size = evergreen_line_buffer_adjust(rdev, rdev->mode_info.crtcs[i+1], mode1, mode0);
		evergreen_program_watermarks(rdev, rdev->mode_info.crtcs[i+1], lb_size, num_heads);
	}
}

/**
 * evergreen_mc_wait_for_idle - wait for MC idle callback.
 *
 * @rdev: radeon_device pointer
 *
 * Wait for the MC (memory controller) to be idle.
 * (evergreen+).
 * Returns 0 if the MC is idle, -1 if not.
 */
int evergreen_mc_wait_for_idle(struct radeon_device *rdev)
{
	unsigned i;
	u32 tmp;

	for (i = 0; i < rdev->usec_timeout; i++) {
		/* read MC_STATUS */
		tmp = RREG32(SRBM_STATUS) & 0x1F00;
		if (!tmp)
			return 0;
		udelay(1);
	}
	return -1;
}

/*
 * GART
 */
void evergreen_pcie_gart_tlb_flush(struct radeon_device *rdev)
{
	unsigned i;
	u32 tmp;

	WREG32(HDP_MEM_COHERENCY_FLUSH_CNTL, 0x1);

	WREG32(VM_CONTEXT0_REQUEST_RESPONSE, REQUEST_TYPE(1));
	for (i = 0; i < rdev->usec_timeout; i++) {
		/* read MC_STATUS */
		tmp = RREG32(VM_CONTEXT0_REQUEST_RESPONSE);
		tmp = (tmp & RESPONSE_TYPE_MASK) >> RESPONSE_TYPE_SHIFT;
		if (tmp == 2) {
			printk(KERN_WARNING "[drm] r600 flush TLB failed\n");
			return;
		}
		if (tmp) {
			return;
		}
		udelay(1);
	}
}

static int evergreen_pcie_gart_enable(struct radeon_device *rdev)
{
	u32 tmp;
	int r;

	if (rdev->gart.robj == NULL) {
		dev_err(rdev->dev, "No VRAM object for PCIE GART.\n");
		return -EINVAL;
	}
	r = radeon_gart_table_vram_pin(rdev);
	if (r)
		return r;
	radeon_gart_restore(rdev);
	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_CACHE | ENABLE_L2_FRAGMENT_PROCESSING |
				ENABLE_L2_PTE_CACHE_LRU_UPDATE_BY_WRITE |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = ENABLE_L1_TLB | ENABLE_L1_FRAGMENT_PROCESSING |
		SYSTEM_ACCESS_MODE_NOT_IN_SYS |
		SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
		EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	if (rdev->flags & RADEON_IS_IGP) {
		WREG32(FUS_MC_VM_MD_L1_TLB0_CNTL, tmp);
		WREG32(FUS_MC_VM_MD_L1_TLB1_CNTL, tmp);
		WREG32(FUS_MC_VM_MD_L1_TLB2_CNTL, tmp);
	} else {
		WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
		WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
		WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
		if ((rdev->family == CHIP_JUNIPER) ||
		    (rdev->family == CHIP_CYPRESS) ||
		    (rdev->family == CHIP_HEMLOCK) ||
		    (rdev->family == CHIP_BARTS))
			WREG32(MC_VM_MD_L1_TLB3_CNTL, tmp);
	}
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	WREG32(VM_CONTEXT0_PAGE_TABLE_START_ADDR, rdev->mc.gtt_start >> 12);
	WREG32(VM_CONTEXT0_PAGE_TABLE_END_ADDR, rdev->mc.gtt_end >> 12);
	WREG32(VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, rdev->gart.table_addr >> 12);
	WREG32(VM_CONTEXT0_CNTL, ENABLE_CONTEXT | PAGE_TABLE_DEPTH(0) |
				RANGE_PROTECTION_FAULT_ENABLE_DEFAULT);
	WREG32(VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR,
			(u32)(rdev->dummy_page.addr >> 12));
	WREG32(VM_CONTEXT1_CNTL, 0);

	evergreen_pcie_gart_tlb_flush(rdev);
	DRM_INFO("PCIE GART of %uM enabled (table at 0x%016llX).\n",
		 (unsigned)(rdev->mc.gtt_size >> 20),
		 (unsigned long long)rdev->gart.table_addr);
	rdev->gart.ready = true;
	return 0;
}

static void evergreen_pcie_gart_disable(struct radeon_device *rdev)
{
	u32 tmp;

	/* Disable all tables */
	WREG32(VM_CONTEXT0_CNTL, 0);
	WREG32(VM_CONTEXT1_CNTL, 0);

	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_FRAGMENT_PROCESSING |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	radeon_gart_table_vram_unpin(rdev);
}

static void evergreen_pcie_gart_fini(struct radeon_device *rdev)
{
	evergreen_pcie_gart_disable(rdev);
	radeon_gart_table_vram_free(rdev);
	radeon_gart_fini(rdev);
}


static void evergreen_agp_enable(struct radeon_device *rdev)
{
	u32 tmp;

	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_CACHE | ENABLE_L2_FRAGMENT_PROCESSING |
				ENABLE_L2_PTE_CACHE_LRU_UPDATE_BY_WRITE |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = ENABLE_L1_TLB | ENABLE_L1_FRAGMENT_PROCESSING |
		SYSTEM_ACCESS_MODE_NOT_IN_SYS |
		SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
		EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	WREG32(VM_CONTEXT0_CNTL, 0);
	WREG32(VM_CONTEXT1_CNTL, 0);
}

void evergreen_mc_stop(struct radeon_device *rdev, struct evergreen_mc_save *save)
{
	u32 crtc_enabled, tmp, frame_count, blackout;
	int i, j;

	save->vga_render_control = RREG32(VGA_RENDER_CONTROL);
	save->vga_hdp_control = RREG32(VGA_HDP_CONTROL);

	/* disable VGA render */
	WREG32(VGA_RENDER_CONTROL, 0);
	/* blank the display controllers */
	for (i = 0; i < rdev->num_crtc; i++) {
		crtc_enabled = RREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[i]) & EVERGREEN_CRTC_MASTER_EN;
		if (crtc_enabled) {
			save->crtc_enabled[i] = true;
			if (ASIC_IS_DCE6(rdev)) {
				tmp = RREG32(EVERGREEN_CRTC_BLANK_CONTROL + crtc_offsets[i]);
				if (!(tmp & EVERGREEN_CRTC_BLANK_DATA_EN)) {
					radeon_wait_for_vblank(rdev, i);
					WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 1);
					tmp |= EVERGREEN_CRTC_BLANK_DATA_EN;
					WREG32(EVERGREEN_CRTC_BLANK_CONTROL + crtc_offsets[i], tmp);
				}
			} else {
				tmp = RREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[i]);
				if (!(tmp & EVERGREEN_CRTC_DISP_READ_REQUEST_DISABLE)) {
					radeon_wait_for_vblank(rdev, i);
					WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 1);
					tmp |= EVERGREEN_CRTC_DISP_READ_REQUEST_DISABLE;
					WREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[i], tmp);
					WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 0);
				}
			}
			/* wait for the next frame */
			frame_count = radeon_get_vblank_counter(rdev, i);
			for (j = 0; j < rdev->usec_timeout; j++) {
				if (radeon_get_vblank_counter(rdev, i) != frame_count)
					break;
				udelay(1);
			}

			/* XXX this is a hack to avoid strange behavior with EFI on certain systems */
			WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 1);
			tmp = RREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[i]);
			tmp &= ~EVERGREEN_CRTC_MASTER_EN;
			WREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[i], tmp);
			WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 0);
			save->crtc_enabled[i] = false;
			/* ***** */
		} else {
			save->crtc_enabled[i] = false;
		}
	}

	radeon_mc_wait_for_idle(rdev);

	blackout = RREG32(MC_SHARED_BLACKOUT_CNTL);
	if ((blackout & BLACKOUT_MODE_MASK) != 1) {
		/* Block CPU access */
		WREG32(BIF_FB_EN, 0);
		/* blackout the MC */
		blackout &= ~BLACKOUT_MODE_MASK;
		WREG32(MC_SHARED_BLACKOUT_CNTL, blackout | 1);
	}
	/* wait for the MC to settle */
	udelay(100);

	/* lock double buffered regs */
	for (i = 0; i < rdev->num_crtc; i++) {
		if (save->crtc_enabled[i]) {
			tmp = RREG32(EVERGREEN_GRPH_UPDATE + crtc_offsets[i]);
			if (!(tmp & EVERGREEN_GRPH_UPDATE_LOCK)) {
				tmp |= EVERGREEN_GRPH_UPDATE_LOCK;
				WREG32(EVERGREEN_GRPH_UPDATE + crtc_offsets[i], tmp);
			}
			tmp = RREG32(EVERGREEN_MASTER_UPDATE_LOCK + crtc_offsets[i]);
			if (!(tmp & 1)) {
				tmp |= 1;
				WREG32(EVERGREEN_MASTER_UPDATE_LOCK + crtc_offsets[i], tmp);
			}
		}
	}
}

void evergreen_mc_resume(struct radeon_device *rdev, struct evergreen_mc_save *save)
{
	u32 tmp, frame_count;
	int i, j;

	/* update crtc base addresses */
	for (i = 0; i < rdev->num_crtc; i++) {
		WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + crtc_offsets[i],
		       upper_32_bits(rdev->mc.vram_start));
		WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + crtc_offsets[i],
		       upper_32_bits(rdev->mc.vram_start));
		WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + crtc_offsets[i],
		       (u32)rdev->mc.vram_start);
		WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + crtc_offsets[i],
		       (u32)rdev->mc.vram_start);
	}
	WREG32(EVERGREEN_VGA_MEMORY_BASE_ADDRESS_HIGH, upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_VGA_MEMORY_BASE_ADDRESS, (u32)rdev->mc.vram_start);

	/* unlock regs and wait for update */
	for (i = 0; i < rdev->num_crtc; i++) {
		if (save->crtc_enabled[i]) {
			tmp = RREG32(EVERGREEN_MASTER_UPDATE_MODE + crtc_offsets[i]);
			if ((tmp & 0x3) != 0) {
				tmp &= ~0x3;
				WREG32(EVERGREEN_MASTER_UPDATE_MODE + crtc_offsets[i], tmp);
			}
			tmp = RREG32(EVERGREEN_GRPH_UPDATE + crtc_offsets[i]);
			if (tmp & EVERGREEN_GRPH_UPDATE_LOCK) {
				tmp &= ~EVERGREEN_GRPH_UPDATE_LOCK;
				WREG32(EVERGREEN_GRPH_UPDATE + crtc_offsets[i], tmp);
			}
			tmp = RREG32(EVERGREEN_MASTER_UPDATE_LOCK + crtc_offsets[i]);
			if (tmp & 1) {
				tmp &= ~1;
				WREG32(EVERGREEN_MASTER_UPDATE_LOCK + crtc_offsets[i], tmp);
			}
			for (j = 0; j < rdev->usec_timeout; j++) {
				tmp = RREG32(EVERGREEN_GRPH_UPDATE + crtc_offsets[i]);
				if ((tmp & EVERGREEN_GRPH_SURFACE_UPDATE_PENDING) == 0)
					break;
				udelay(1);
			}
		}
	}

	/* unblackout the MC */
	tmp = RREG32(MC_SHARED_BLACKOUT_CNTL);
	tmp &= ~BLACKOUT_MODE_MASK;
	WREG32(MC_SHARED_BLACKOUT_CNTL, tmp);
	/* allow CPU access */
	WREG32(BIF_FB_EN, FB_READ_EN | FB_WRITE_EN);

	for (i = 0; i < rdev->num_crtc; i++) {
		if (save->crtc_enabled[i]) {
			if (ASIC_IS_DCE6(rdev)) {
				tmp = RREG32(EVERGREEN_CRTC_BLANK_CONTROL + crtc_offsets[i]);
				tmp |= EVERGREEN_CRTC_BLANK_DATA_EN;
				WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 1);
				WREG32(EVERGREEN_CRTC_BLANK_CONTROL + crtc_offsets[i], tmp);
				WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 0);
			} else {
				tmp = RREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[i]);
				tmp &= ~EVERGREEN_CRTC_DISP_READ_REQUEST_DISABLE;
				WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 1);
				WREG32(EVERGREEN_CRTC_CONTROL + crtc_offsets[i], tmp);
				WREG32(EVERGREEN_CRTC_UPDATE_LOCK + crtc_offsets[i], 0);
			}
			/* wait for the next frame */
			frame_count = radeon_get_vblank_counter(rdev, i);
			for (j = 0; j < rdev->usec_timeout; j++) {
				if (radeon_get_vblank_counter(rdev, i) != frame_count)
					break;
				udelay(1);
			}
		}
	}
	/* Unlock vga access */
	WREG32(VGA_HDP_CONTROL, save->vga_hdp_control);
	mdelay(1);
	WREG32(VGA_RENDER_CONTROL, save->vga_render_control);
}

void evergreen_mc_program(struct radeon_device *rdev)
{
	struct evergreen_mc_save save;
	u32 tmp;
	int i, j;

	/* Initialize HDP */
	for (i = 0, j = 0; i < 32; i++, j += 0x18) {
		WREG32((0x2c14 + j), 0x00000000);
		WREG32((0x2c18 + j), 0x00000000);
		WREG32((0x2c1c + j), 0x00000000);
		WREG32((0x2c20 + j), 0x00000000);
		WREG32((0x2c24 + j), 0x00000000);
	}
	WREG32(HDP_REG_COHERENCY_FLUSH_CNTL, 0);

	evergreen_mc_stop(rdev, &save);
	if (evergreen_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}
	/* Lockout access through VGA aperture*/
	WREG32(VGA_HDP_CONTROL, VGA_MEMORY_DISABLE);
	/* Update configuration */
	if (rdev->flags & RADEON_IS_AGP) {
		if (rdev->mc.vram_start < rdev->mc.gtt_start) {
			/* VRAM before AGP */
			WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
				rdev->mc.vram_start >> 12);
			WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
				rdev->mc.gtt_end >> 12);
		} else {
			/* VRAM after AGP */
			WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
				rdev->mc.gtt_start >> 12);
			WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
				rdev->mc.vram_end >> 12);
		}
	} else {
		WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
			rdev->mc.vram_start >> 12);
		WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
			rdev->mc.vram_end >> 12);
	}
	WREG32(MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR, rdev->vram_scratch.gpu_addr >> 12);
	/* llano/ontario only */
	if ((rdev->family == CHIP_PALM) ||
	    (rdev->family == CHIP_SUMO) ||
	    (rdev->family == CHIP_SUMO2)) {
		tmp = RREG32(MC_FUS_VM_FB_OFFSET) & 0x000FFFFF;
		tmp |= ((rdev->mc.vram_end >> 20) & 0xF) << 24;
		tmp |= ((rdev->mc.vram_start >> 20) & 0xF) << 20;
		WREG32(MC_FUS_VM_FB_OFFSET, tmp);
	}
	tmp = ((rdev->mc.vram_end >> 24) & 0xFFFF) << 16;
	tmp |= ((rdev->mc.vram_start >> 24) & 0xFFFF);
	WREG32(MC_VM_FB_LOCATION, tmp);
	WREG32(HDP_NONSURFACE_BASE, (rdev->mc.vram_start >> 8));
	WREG32(HDP_NONSURFACE_INFO, (2 << 7) | (1 << 30));
	WREG32(HDP_NONSURFACE_SIZE, 0x3FFFFFFF);
	if (rdev->flags & RADEON_IS_AGP) {
		WREG32(MC_VM_AGP_TOP, rdev->mc.gtt_end >> 16);
		WREG32(MC_VM_AGP_BOT, rdev->mc.gtt_start >> 16);
		WREG32(MC_VM_AGP_BASE, rdev->mc.agp_base >> 22);
	} else {
		WREG32(MC_VM_AGP_BASE, 0);
		WREG32(MC_VM_AGP_TOP, 0x0FFFFFFF);
		WREG32(MC_VM_AGP_BOT, 0x0FFFFFFF);
	}
	if (evergreen_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}
	evergreen_mc_resume(rdev, &save);
	/* we need to own VRAM, so turn off the VGA renderer here
	 * to stop it overwriting our objects */
	rv515_vga_render_disable(rdev);
}

/*
 * CP.
 */
void evergreen_ring_ib_execute(struct radeon_device *rdev, struct radeon_ib *ib)
{
	struct radeon_ring *ring = &rdev->ring[ib->ring];
	u32 next_rptr;

	/* set to DX10/11 mode */
	radeon_ring_write(ring, PACKET3(PACKET3_MODE_CONTROL, 0));
	radeon_ring_write(ring, 1);

	if (ring->rptr_save_reg) {
		next_rptr = ring->wptr + 3 + 4;
		radeon_ring_write(ring, PACKET3(PACKET3_SET_CONFIG_REG, 1));
		radeon_ring_write(ring, ((ring->rptr_save_reg - 
					  PACKET3_SET_CONFIG_REG_START) >> 2));
		radeon_ring_write(ring, next_rptr);
	} else if (rdev->wb.enabled) {
		next_rptr = ring->wptr + 5 + 4;
		radeon_ring_write(ring, PACKET3(PACKET3_MEM_WRITE, 3));
		radeon_ring_write(ring, ring->next_rptr_gpu_addr & 0xfffffffc);
		radeon_ring_write(ring, (upper_32_bits(ring->next_rptr_gpu_addr) & 0xff) | (1 << 18));
		radeon_ring_write(ring, next_rptr);
		radeon_ring_write(ring, 0);
	}

	radeon_ring_write(ring, PACKET3(PACKET3_INDIRECT_BUFFER, 2));
	radeon_ring_write(ring,
#ifdef __BIG_ENDIAN
			  (2 << 0) |
#endif
			  (ib->gpu_addr & 0xFFFFFFFC));
	radeon_ring_write(ring, upper_32_bits(ib->gpu_addr) & 0xFF);
	radeon_ring_write(ring, ib->length_dw);
}


static int evergreen_cp_load_microcode(struct radeon_device *rdev)
{
	const __be32 *fw_data;
	int i;

	if (!rdev->me_fw || !rdev->pfp_fw)
		return -EINVAL;

	r700_cp_stop(rdev);
	WREG32(CP_RB_CNTL,
#ifdef __BIG_ENDIAN
	       BUF_SWAP_32BIT |
#endif
	       RB_NO_UPDATE | RB_BLKSZ(15) | RB_BUFSZ(3));

	fw_data = (const __be32 *)rdev->pfp_fw;
	WREG32(CP_PFP_UCODE_ADDR, 0);
	for (i = 0; i < EVERGREEN_PFP_UCODE_SIZE; i++)
		WREG32(CP_PFP_UCODE_DATA, be32_to_cpup(fw_data++));
	WREG32(CP_PFP_UCODE_ADDR, 0);

	fw_data = (const __be32 *)rdev->me_fw;
	WREG32(CP_ME_RAM_WADDR, 0);
	for (i = 0; i < EVERGREEN_PM4_UCODE_SIZE; i++)
		WREG32(CP_ME_RAM_DATA, be32_to_cpup(fw_data++));

	WREG32(CP_PFP_UCODE_ADDR, 0);
	WREG32(CP_ME_RAM_WADDR, 0);
	WREG32(CP_ME_RAM_RADDR, 0);
	return 0;
}

static int evergreen_cp_start(struct radeon_device *rdev)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	int r, i;
	uint32_t cp_me;

	r = radeon_ring_lock(rdev, ring, 7);
	if (r) {
		DRM_ERROR("radeon: cp failed to lock ring (%d).\n", r);
		return r;
	}
	radeon_ring_write(ring, PACKET3(PACKET3_ME_INITIALIZE, 5));
	radeon_ring_write(ring, 0x1);
	radeon_ring_write(ring, 0x0);
	radeon_ring_write(ring, rdev->config.evergreen.max_hw_contexts - 1);
	radeon_ring_write(ring, PACKET3_ME_INITIALIZE_DEVICE_ID(1));
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_unlock_commit(rdev, ring);

	cp_me = 0xff;
	WREG32(CP_ME_CNTL, cp_me);

	r = radeon_ring_lock(rdev, ring, evergreen_default_size + 19);
	if (r) {
		DRM_ERROR("radeon: cp failed to lock ring (%d).\n", r);
		return r;
	}

	/* setup clear context state */
	radeon_ring_write(ring, PACKET3(PACKET3_PREAMBLE_CNTL, 0));
	radeon_ring_write(ring, PACKET3_PREAMBLE_BEGIN_CLEAR_STATE);

	for (i = 0; i < evergreen_default_size; i++)
		radeon_ring_write(ring, evergreen_default_state[i]);

	radeon_ring_write(ring, PACKET3(PACKET3_PREAMBLE_CNTL, 0));
	radeon_ring_write(ring, PACKET3_PREAMBLE_END_CLEAR_STATE);

	/* set clear context state */
	radeon_ring_write(ring, PACKET3(PACKET3_CLEAR_STATE, 0));
	radeon_ring_write(ring, 0);

	/* SQ_VTX_BASE_VTX_LOC */
	radeon_ring_write(ring, 0xc0026f00);
	radeon_ring_write(ring, 0x00000000);
	radeon_ring_write(ring, 0x00000000);
	radeon_ring_write(ring, 0x00000000);

	/* Clear consts */
	radeon_ring_write(ring, 0xc0036f00);
	radeon_ring_write(ring, 0x00000bc4);
	radeon_ring_write(ring, 0xffffffff);
	radeon_ring_write(ring, 0xffffffff);
	radeon_ring_write(ring, 0xffffffff);

	radeon_ring_write(ring, 0xc0026900);
	radeon_ring_write(ring, 0x00000316);
	radeon_ring_write(ring, 0x0000000e); /* VGT_VERTEX_REUSE_BLOCK_CNTL */
	radeon_ring_write(ring, 0x00000010); /*  */

	radeon_ring_unlock_commit(rdev, ring);

	return 0;
}

static int evergreen_cp_resume(struct radeon_device *rdev)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	u32 tmp;
	u32 rb_bufsz;
	int r;

	/* Reset cp; if cp is reset, then PA, SH, VGT also need to be reset */
	WREG32(GRBM_SOFT_RESET, (SOFT_RESET_CP |
				 SOFT_RESET_PA |
				 SOFT_RESET_SH |
				 SOFT_RESET_VGT |
				 SOFT_RESET_SPI |
				 SOFT_RESET_SX));
	RREG32(GRBM_SOFT_RESET);
	mdelay(15);
	WREG32(GRBM_SOFT_RESET, 0);
	RREG32(GRBM_SOFT_RESET);

	/* Set ring buffer size */
	rb_bufsz = drm_order(ring->ring_size / 8);
	tmp = (drm_order(RADEON_GPU_PAGE_SIZE/8) << 8) | rb_bufsz;
#ifdef __BIG_ENDIAN
	tmp |= BUF_SWAP_32BIT;
#endif
	WREG32(CP_RB_CNTL, tmp);
	WREG32(CP_SEM_WAIT_TIMER, 0x0);
	WREG32(CP_SEM_INCOMPLETE_TIMER_CNTL, 0x0);

	/* Set the write pointer delay */
	WREG32(CP_RB_WPTR_DELAY, 0);

	/* Initialize the ring buffer's read and write pointers */
	WREG32(CP_RB_CNTL, tmp | RB_RPTR_WR_ENA);
	WREG32(CP_RB_RPTR_WR, 0);
	ring->wptr = 0;
	WREG32(CP_RB_WPTR, ring->wptr);

	/* set the wb address whether it's enabled or not */
	WREG32(CP_RB_RPTR_ADDR,
	       ((rdev->wb.gpu_addr + RADEON_WB_CP_RPTR_OFFSET) & 0xFFFFFFFC));
	WREG32(CP_RB_RPTR_ADDR_HI, upper_32_bits(rdev->wb.gpu_addr + RADEON_WB_CP_RPTR_OFFSET) & 0xFF);
	WREG32(SCRATCH_ADDR, ((rdev->wb.gpu_addr + RADEON_WB_SCRATCH_OFFSET) >> 8) & 0xFFFFFFFF);

	if (rdev->wb.enabled)
		WREG32(SCRATCH_UMSK, 0xff);
	else {
		tmp |= RB_NO_UPDATE;
		WREG32(SCRATCH_UMSK, 0);
	}

	mdelay(1);
	WREG32(CP_RB_CNTL, tmp);

	WREG32(CP_RB_BASE, ring->gpu_addr >> 8);
	WREG32(CP_DEBUG, (1 << 27) | (1 << 28));

	ring->rptr = RREG32(CP_RB_RPTR);

	evergreen_cp_start(rdev);
	ring->ready = true;
	r = radeon_ring_test(rdev, RADEON_RING_TYPE_GFX_INDEX, ring);
	if (r) {
		ring->ready = false;
		return r;
	}
	return 0;
}

/*
 * Core functions
 */
static void evergreen_gpu_init(struct radeon_device *rdev)
{
	u32 gb_addr_config;
	u32 mc_shared_chmap, mc_arb_ramcfg;
	u32 sx_debug_1;
	u32 smx_dc_ctl0;
	u32 sq_config;
	u32 sq_lds_resource_mgmt;
	u32 sq_gpr_resource_mgmt_1;
	u32 sq_gpr_resource_mgmt_2;
	u32 sq_gpr_resource_mgmt_3;
	u32 sq_thread_resource_mgmt;
	u32 sq_thread_resource_mgmt_2;
	u32 sq_stack_resource_mgmt_1;
	u32 sq_stack_resource_mgmt_2;
	u32 sq_stack_resource_mgmt_3;
	u32 vgt_cache_invalidation;
	u32 hdp_host_path_cntl, tmp;
	u32 disabled_rb_mask;
	int i, j, num_shader_engines, ps_thread_count;

	switch (rdev->family) {
	case CHIP_CYPRESS:
	case CHIP_HEMLOCK:
		rdev->config.evergreen.num_ses = 2;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 8;
		rdev->config.evergreen.max_simds = 10;
		rdev->config.evergreen.max_backends = 4 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 512;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = CYPRESS_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_JUNIPER:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 4;
		rdev->config.evergreen.max_simds = 10;
		rdev->config.evergreen.max_backends = 4 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 512;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = JUNIPER_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_REDWOOD:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 4;
		rdev->config.evergreen.max_simds = 5;
		rdev->config.evergreen.max_backends = 2 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = REDWOOD_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_CEDAR:
	default:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 2;
		rdev->config.evergreen.max_tile_pipes = 2;
		rdev->config.evergreen.max_simds = 2;
		rdev->config.evergreen.max_backends = 1 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 192;
		rdev->config.evergreen.max_gs_threads = 16;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 128;
		rdev->config.evergreen.sx_max_export_pos_size = 32;
		rdev->config.evergreen.sx_max_export_smx_size = 96;
		rdev->config.evergreen.max_hw_contexts = 4;
		rdev->config.evergreen.sq_num_cf_insts = 1;

		rdev->config.evergreen.sc_prim_fifo_size = 0x40;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = CEDAR_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_PALM:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 2;
		rdev->config.evergreen.max_tile_pipes = 2;
		rdev->config.evergreen.max_simds = 2;
		rdev->config.evergreen.max_backends = 1 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 192;
		rdev->config.evergreen.max_gs_threads = 16;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 128;
		rdev->config.evergreen.sx_max_export_pos_size = 32;
		rdev->config.evergreen.sx_max_export_smx_size = 96;
		rdev->config.evergreen.max_hw_contexts = 4;
		rdev->config.evergreen.sq_num_cf_insts = 1;

		rdev->config.evergreen.sc_prim_fifo_size = 0x40;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = CEDAR_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_SUMO:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 4;
		if (rdev->pdev->device == 0x9648)
			rdev->config.evergreen.max_simds = 3;
		else if ((rdev->pdev->device == 0x9647) ||
			 (rdev->pdev->device == 0x964a))
			rdev->config.evergreen.max_simds = 4;
		else
			rdev->config.evergreen.max_simds = 5;
		rdev->config.evergreen.max_backends = 2 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x40;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = SUMO_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_SUMO2:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 4;
		rdev->config.evergreen.max_simds = 2;
		rdev->config.evergreen.max_backends = 1 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 512;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 4;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x40;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = SUMO2_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_BARTS:
		rdev->config.evergreen.num_ses = 2;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 8;
		rdev->config.evergreen.max_simds = 7;
		rdev->config.evergreen.max_backends = 4 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 512;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = BARTS_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_TURKS:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 4;
		rdev->config.evergreen.max_simds = 6;
		rdev->config.evergreen.max_backends = 2 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = TURKS_GB_ADDR_CONFIG_GOLDEN;
		break;
	case CHIP_CAICOS:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 2;
		rdev->config.evergreen.max_tile_pipes = 2;
		rdev->config.evergreen.max_simds = 2;
		rdev->config.evergreen.max_backends = 1 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 192;
		rdev->config.evergreen.max_gs_threads = 16;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 128;
		rdev->config.evergreen.sx_max_export_pos_size = 32;
		rdev->config.evergreen.sx_max_export_smx_size = 96;
		rdev->config.evergreen.max_hw_contexts = 4;
		rdev->config.evergreen.sq_num_cf_insts = 1;

		rdev->config.evergreen.sc_prim_fifo_size = 0x40;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		gb_addr_config = CAICOS_GB_ADDR_CONFIG_GOLDEN;
		break;
	}

	/* Initialize HDP */
	for (i = 0, j = 0; i < 32; i++, j += 0x18) {
		WREG32((0x2c14 + j), 0x00000000);
		WREG32((0x2c18 + j), 0x00000000);
		WREG32((0x2c1c + j), 0x00000000);
		WREG32((0x2c20 + j), 0x00000000);
		WREG32((0x2c24 + j), 0x00000000);
	}

	WREG32(GRBM_CNTL, GRBM_READ_TIMEOUT(0xff));

	evergreen_fix_pci_max_read_req_size(rdev);

	mc_shared_chmap = RREG32(MC_SHARED_CHMAP);
	if ((rdev->family == CHIP_PALM) ||
	    (rdev->family == CHIP_SUMO) ||
	    (rdev->family == CHIP_SUMO2))
		mc_arb_ramcfg = RREG32(FUS_MC_ARB_RAMCFG);
	else
		mc_arb_ramcfg = RREG32(MC_ARB_RAMCFG);

	/* setup tiling info dword.  gb_addr_config is not adequate since it does
	 * not have bank info, so create a custom tiling dword.
	 * bits 3:0   num_pipes
	 * bits 7:4   num_banks
	 * bits 11:8  group_size
	 * bits 15:12 row_size
	 */
	rdev->config.evergreen.tile_config = 0;
	switch (rdev->config.evergreen.max_tile_pipes) {
	case 1:
	default:
		rdev->config.evergreen.tile_config |= (0 << 0);
		break;
	case 2:
		rdev->config.evergreen.tile_config |= (1 << 0);
		break;
	case 4:
		rdev->config.evergreen.tile_config |= (2 << 0);
		break;
	case 8:
		rdev->config.evergreen.tile_config |= (3 << 0);
		break;
	}
	/* num banks is 8 on all fusion asics. 0 = 4, 1 = 8, 2 = 16 */
	if (rdev->flags & RADEON_IS_IGP)
		rdev->config.evergreen.tile_config |= 1 << 4;
	else {
		switch ((mc_arb_ramcfg & NOOFBANK_MASK) >> NOOFBANK_SHIFT) {
		case 0: /* four banks */
			rdev->config.evergreen.tile_config |= 0 << 4;
			break;
		case 1: /* eight banks */
			rdev->config.evergreen.tile_config |= 1 << 4;
			break;
		case 2: /* sixteen banks */
		default:
			rdev->config.evergreen.tile_config |= 2 << 4;
			break;
		}
	}
	rdev->config.evergreen.tile_config |= 0 << 8;
	rdev->config.evergreen.tile_config |=
		((gb_addr_config & 0x30000000) >> 28) << 12;

	num_shader_engines = (gb_addr_config & NUM_SHADER_ENGINES(3) >> 12) + 1;

	if ((rdev->family >= CHIP_CEDAR) && (rdev->family <= CHIP_HEMLOCK)) {
		u32 efuse_straps_4;
		u32 efuse_straps_3;

		WREG32(RCU_IND_INDEX, 0x204);
		efuse_straps_4 = RREG32(RCU_IND_DATA);
		WREG32(RCU_IND_INDEX, 0x203);
		efuse_straps_3 = RREG32(RCU_IND_DATA);
		tmp = (((efuse_straps_4 & 0xf) << 4) |
		      ((efuse_straps_3 & 0xf0000000) >> 28));
	} else {
		tmp = 0;
		for (i = (rdev->config.evergreen.num_ses - 1); i >= 0; i--) {
			u32 rb_disable_bitmap;

			WREG32(GRBM_GFX_INDEX, INSTANCE_BROADCAST_WRITES | SE_INDEX(i));
			WREG32(RLC_GFX_INDEX, INSTANCE_BROADCAST_WRITES | SE_INDEX(i));
			rb_disable_bitmap = (RREG32(CC_RB_BACKEND_DISABLE) & 0x00ff0000) >> 16;
			tmp <<= 4;
			tmp |= rb_disable_bitmap;
		}
	}
	/* enabled rb are just the one not disabled :) */
	disabled_rb_mask = tmp;

	WREG32(GRBM_GFX_INDEX, INSTANCE_BROADCAST_WRITES | SE_BROADCAST_WRITES);
	WREG32(RLC_GFX_INDEX, INSTANCE_BROADCAST_WRITES | SE_BROADCAST_WRITES);

	WREG32(GB_ADDR_CONFIG, gb_addr_config);
	WREG32(DMIF_ADDR_CONFIG, gb_addr_config);
	WREG32(HDP_ADDR_CONFIG, gb_addr_config);
	WREG32(DMA_TILING_CONFIG, gb_addr_config);

	if ((rdev->config.evergreen.max_backends == 1) &&
	    (rdev->flags & RADEON_IS_IGP)) {
		if ((disabled_rb_mask & 3) == 1) {
			/* RB0 disabled, RB1 enabled */
			tmp = 0x11111111;
		} else {
			/* RB1 disabled, RB0 enabled */
			tmp = 0x00000000;
		}
	} else {
		tmp = gb_addr_config & NUM_PIPES_MASK;
		tmp = r6xx_remap_render_backend(rdev, tmp, rdev->config.evergreen.max_backends,
						EVERGREEN_MAX_BACKENDS, disabled_rb_mask);
	}
	WREG32(GB_BACKEND_MAP, tmp);

	WREG32(CGTS_SYS_TCC_DISABLE, 0);
	WREG32(CGTS_TCC_DISABLE, 0);
	WREG32(CGTS_USER_SYS_TCC_DISABLE, 0);
	WREG32(CGTS_USER_TCC_DISABLE, 0);

	/* set HW defaults for 3D engine */
	WREG32(CP_QUEUE_THRESHOLDS, (ROQ_IB1_START(0x16) |
				     ROQ_IB2_START(0x2b)));

	WREG32(CP_MEQ_THRESHOLDS, STQ_SPLIT(0x30));

	WREG32(TA_CNTL_AUX, (DISABLE_CUBE_ANISO |
			     SYNC_GRADIENT |
			     SYNC_WALKER |
			     SYNC_ALIGNER));

	sx_debug_1 = RREG32(SX_DEBUG_1);
	sx_debug_1 |= ENABLE_NEW_SMX_ADDRESS;
	WREG32(SX_DEBUG_1, sx_debug_1);


	smx_dc_ctl0 = RREG32(SMX_DC_CTL0);
	smx_dc_ctl0 &= ~NUMBER_OF_SETS(0x1ff);
	smx_dc_ctl0 |= NUMBER_OF_SETS(rdev->config.evergreen.sx_num_of_sets);
	WREG32(SMX_DC_CTL0, smx_dc_ctl0);

	if (rdev->family <= CHIP_SUMO2)
		WREG32(SMX_SAR_CTL0, 0x00010000);

	WREG32(SX_EXPORT_BUFFER_SIZES, (COLOR_BUFFER_SIZE((rdev->config.evergreen.sx_max_export_size / 4) - 1) |
					POSITION_BUFFER_SIZE((rdev->config.evergreen.sx_max_export_pos_size / 4) - 1) |
					SMX_BUFFER_SIZE((rdev->config.evergreen.sx_max_export_smx_size / 4) - 1)));

	WREG32(PA_SC_FIFO_SIZE, (SC_PRIM_FIFO_SIZE(rdev->config.evergreen.sc_prim_fifo_size) |
				 SC_HIZ_TILE_FIFO_SIZE(rdev->config.evergreen.sc_hiz_tile_fifo_size) |
				 SC_EARLYZ_TILE_FIFO_SIZE(rdev->config.evergreen.sc_earlyz_tile_fifo_size)));

	WREG32(VGT_NUM_INSTANCES, 1);
	WREG32(SPI_CONFIG_CNTL, 0);
	WREG32(SPI_CONFIG_CNTL_1, VTX_DONE_DELAY(4));
	WREG32(CP_PERFMON_CNTL, 0);

	WREG32(SQ_MS_FIFO_SIZES, (CACHE_FIFO_SIZE(16 * rdev->config.evergreen.sq_num_cf_insts) |
				  FETCH_FIFO_HIWATER(0x4) |
				  DONE_FIFO_HIWATER(0xe0) |
				  ALU_UPDATE_FIFO_HIWATER(0x8)));

	sq_config = RREG32(SQ_CONFIG);
	sq_config &= ~(PS_PRIO(3) |
		       VS_PRIO(3) |
		       GS_PRIO(3) |
		       ES_PRIO(3));
	sq_config |= (VC_ENABLE |
		      EXPORT_SRC_C |
		      PS_PRIO(0) |
		      VS_PRIO(1) |
		      GS_PRIO(2) |
		      ES_PRIO(3));

	switch (rdev->family) {
	case CHIP_CEDAR:
	case CHIP_PALM:
	case CHIP_SUMO:
	case CHIP_SUMO2:
	case CHIP_CAICOS:
		/* no vertex cache */
		sq_config &= ~VC_ENABLE;
		break;
	default:
		break;
	}

	sq_lds_resource_mgmt = RREG32(SQ_LDS_RESOURCE_MGMT);

	sq_gpr_resource_mgmt_1 = NUM_PS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2))* 12 / 32);
	sq_gpr_resource_mgmt_1 |= NUM_VS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 6 / 32);
	sq_gpr_resource_mgmt_1 |= NUM_CLAUSE_TEMP_GPRS(4);
	sq_gpr_resource_mgmt_2 = NUM_GS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 4 / 32);
	sq_gpr_resource_mgmt_2 |= NUM_ES_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 4 / 32);
	sq_gpr_resource_mgmt_3 = NUM_HS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 3 / 32);
	sq_gpr_resource_mgmt_3 |= NUM_LS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 3 / 32);

	switch (rdev->family) {
	case CHIP_CEDAR:
	case CHIP_PALM:
	case CHIP_SUMO:
	case CHIP_SUMO2:
		ps_thread_count = 96;
		break;
	default:
		ps_thread_count = 128;
		break;
	}

	sq_thread_resource_mgmt = NUM_PS_THREADS(ps_thread_count);
	sq_thread_resource_mgmt |= NUM_VS_THREADS((((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8);
	sq_thread_resource_mgmt |= NUM_GS_THREADS((((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8);
	sq_thread_resource_mgmt |= NUM_ES_THREADS((((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8);
	sq_thread_resource_mgmt_2 = NUM_HS_THREADS((((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8);
	sq_thread_resource_mgmt_2 |= NUM_LS_THREADS((((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8);

	sq_stack_resource_mgmt_1 = NUM_PS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_1 |= NUM_VS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_2 = NUM_GS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_2 |= NUM_ES_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_3 = NUM_HS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_3 |= NUM_LS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);

	WREG32(SQ_CONFIG, sq_config);
	WREG32(SQ_GPR_RESOURCE_MGMT_1, sq_gpr_resource_mgmt_1);
	WREG32(SQ_GPR_RESOURCE_MGMT_2, sq_gpr_resource_mgmt_2);
	WREG32(SQ_GPR_RESOURCE_MGMT_3, sq_gpr_resource_mgmt_3);
	WREG32(SQ_THREAD_RESOURCE_MGMT, sq_thread_resource_mgmt);
	WREG32(SQ_THREAD_RESOURCE_MGMT_2, sq_thread_resource_mgmt_2);
	WREG32(SQ_STACK_RESOURCE_MGMT_1, sq_stack_resource_mgmt_1);
	WREG32(SQ_STACK_RESOURCE_MGMT_2, sq_stack_resource_mgmt_2);
	WREG32(SQ_STACK_RESOURCE_MGMT_3, sq_stack_resource_mgmt_3);
	WREG32(SQ_DYN_GPR_CNTL_PS_FLUSH_REQ, 0);
	WREG32(SQ_LDS_RESOURCE_MGMT, sq_lds_resource_mgmt);

	WREG32(PA_SC_FORCE_EOV_MAX_CNTS, (FORCE_EOV_MAX_CLK_CNT(4095) |
					  FORCE_EOV_MAX_REZ_CNT(255)));

	switch (rdev->family) {
	case CHIP_CEDAR:
	case CHIP_PALM:
	case CHIP_SUMO:
	case CHIP_SUMO2:
	case CHIP_CAICOS:
		vgt_cache_invalidation = CACHE_INVALIDATION(TC_ONLY);
		break;
	default:
		vgt_cache_invalidation = CACHE_INVALIDATION(VC_AND_TC);
		break;
	}
	vgt_cache_invalidation |= AUTO_INVLD_EN(ES_AND_GS_AUTO);
	WREG32(VGT_CACHE_INVALIDATION, vgt_cache_invalidation);

	WREG32(VGT_GS_VERTEX_REUSE, 16);
	WREG32(PA_SU_LINE_STIPPLE_VALUE, 0);
	WREG32(PA_SC_LINE_STIPPLE_STATE, 0);

	WREG32(VGT_VERTEX_REUSE_BLOCK_CNTL, 14);
	WREG32(VGT_OUT_DEALLOC_CNTL, 16);

	WREG32(CB_PERF_CTR0_SEL_0, 0);
	WREG32(CB_PERF_CTR0_SEL_1, 0);
	WREG32(CB_PERF_CTR1_SEL_0, 0);
	WREG32(CB_PERF_CTR1_SEL_1, 0);
	WREG32(CB_PERF_CTR2_SEL_0, 0);
	WREG32(CB_PERF_CTR2_SEL_1, 0);
	WREG32(CB_PERF_CTR3_SEL_0, 0);
	WREG32(CB_PERF_CTR3_SEL_1, 0);

	/* clear render buffer base addresses */
	WREG32(CB_COLOR0_BASE, 0);
	WREG32(CB_COLOR1_BASE, 0);
	WREG32(CB_COLOR2_BASE, 0);
	WREG32(CB_COLOR3_BASE, 0);
	WREG32(CB_COLOR4_BASE, 0);
	WREG32(CB_COLOR5_BASE, 0);
	WREG32(CB_COLOR6_BASE, 0);
	WREG32(CB_COLOR7_BASE, 0);
	WREG32(CB_COLOR8_BASE, 0);
	WREG32(CB_COLOR9_BASE, 0);
	WREG32(CB_COLOR10_BASE, 0);
	WREG32(CB_COLOR11_BASE, 0);

	/* set the shader const cache sizes to 0 */
	for (i = SQ_ALU_CONST_BUFFER_SIZE_PS_0; i < 0x28200; i += 4)
		WREG32(i, 0);
	for (i = SQ_ALU_CONST_BUFFER_SIZE_HS_0; i < 0x29000; i += 4)
		WREG32(i, 0);

	tmp = RREG32(HDP_MISC_CNTL);
	tmp |= HDP_FLUSH_INVALIDATE_CACHE;
	WREG32(HDP_MISC_CNTL, tmp);

	hdp_host_path_cntl = RREG32(HDP_HOST_PATH_CNTL);
	WREG32(HDP_HOST_PATH_CNTL, hdp_host_path_cntl);

	WREG32(PA_CL_ENHANCE, CLIP_VTX_REORDER_ENA | NUM_CLIP_SEQ(3));

	udelay(50);

}

int evergreen_mc_init(struct radeon_device *rdev)
{
	u32 tmp;
	int chansize, numchan;

	/* Get VRAM informations */
	rdev->mc.vram_is_ddr = true;
	if ((rdev->family == CHIP_PALM) ||
	    (rdev->family == CHIP_SUMO) ||
	    (rdev->family == CHIP_SUMO2))
		tmp = RREG32(FUS_MC_ARB_RAMCFG);
	else
		tmp = RREG32(MC_ARB_RAMCFG);
	if (tmp & CHANSIZE_OVERRIDE) {
		chansize = 16;
	} else if (tmp & CHANSIZE_MASK) {
		chansize = 64;
	} else {
		chansize = 32;
	}
	tmp = RREG32(MC_SHARED_CHMAP);
	switch ((tmp & NOOFCHAN_MASK) >> NOOFCHAN_SHIFT) {
	case 0:
	default:
		numchan = 1;
		break;
	case 1:
		numchan = 2;
		break;
	case 2:
		numchan = 4;
		break;
	case 3:
		numchan = 8;
		break;
	}
	rdev->mc.vram_width = numchan * chansize;
	/* Could aper size report 0 ? */
	rdev->mc.aper_base = rdev->fb_aper_offset;
	rdev->mc.aper_size = rdev->fb_aper_size;
	/* Setup GPU memory space */
	if ((rdev->family == CHIP_PALM) ||
	    (rdev->family == CHIP_SUMO) ||
	    (rdev->family == CHIP_SUMO2)) {
		/* size in bytes on fusion */
		rdev->mc.mc_vram_size = RREG32(CONFIG_MEMSIZE);
		rdev->mc.real_vram_size = RREG32(CONFIG_MEMSIZE);
	} else {
		/* size in MB on evergreen/cayman/tn */
		rdev->mc.mc_vram_size = RREG32(CONFIG_MEMSIZE) * 1024ULL * 1024ULL;
		rdev->mc.real_vram_size = RREG32(CONFIG_MEMSIZE) * 1024ULL * 1024ULL;
	}
	rdev->mc.visible_vram_size = rdev->mc.aper_size;
	r700_vram_gtt_location(rdev, &rdev->mc);
	radeon_update_bandwidth_info(rdev);

	return 0;
}

bool evergreen_gpu_is_lockup(struct radeon_device *rdev, struct radeon_ring *ring)
{
	u32 srbm_status;
	u32 grbm_status;
	u32 grbm_status_se0, grbm_status_se1;

	srbm_status = RREG32(SRBM_STATUS);
	grbm_status = RREG32(GRBM_STATUS);
	grbm_status_se0 = RREG32(GRBM_STATUS_SE0);
	grbm_status_se1 = RREG32(GRBM_STATUS_SE1);
	if (!(grbm_status & GUI_ACTIVE)) {
		radeon_ring_lockup_update(ring);
		return false;
	}
	/* force CP activities */
	radeon_ring_force_activity(rdev, ring);
	return radeon_ring_test_lockup(rdev, ring);
}

static void evergreen_gpu_soft_reset_gfx(struct radeon_device *rdev)
{
	u32 grbm_reset = 0;

	if (!(RREG32(GRBM_STATUS) & GUI_ACTIVE))
		return;

	dev_info(rdev->dev, "  GRBM_STATUS               = 0x%08X\n",
		RREG32(GRBM_STATUS));
	dev_info(rdev->dev, "  GRBM_STATUS_SE0           = 0x%08X\n",
		RREG32(GRBM_STATUS_SE0));
	dev_info(rdev->dev, "  GRBM_STATUS_SE1           = 0x%08X\n",
		RREG32(GRBM_STATUS_SE1));
	dev_info(rdev->dev, "  SRBM_STATUS               = 0x%08X\n",
		RREG32(SRBM_STATUS));
	dev_info(rdev->dev, "  R_008674_CP_STALLED_STAT1 = 0x%08X\n",
		RREG32(CP_STALLED_STAT1));
	dev_info(rdev->dev, "  R_008678_CP_STALLED_STAT2 = 0x%08X\n",
		RREG32(CP_STALLED_STAT2));
	dev_info(rdev->dev, "  R_00867C_CP_BUSY_STAT     = 0x%08X\n",
		RREG32(CP_BUSY_STAT));
	dev_info(rdev->dev, "  R_008680_CP_STAT          = 0x%08X\n",
		RREG32(CP_STAT));

	/* Disable CP parsing/prefetching */
	WREG32(CP_ME_CNTL, CP_ME_HALT | CP_PFP_HALT);

	/* reset all the gfx blocks */
	grbm_reset = (SOFT_RESET_CP |
		      SOFT_RESET_CB |
		      SOFT_RESET_DB |
		      SOFT_RESET_PA |
		      SOFT_RESET_SC |
		      SOFT_RESET_SPI |
		      SOFT_RESET_SH |
		      SOFT_RESET_SX |
		      SOFT_RESET_TC |
		      SOFT_RESET_TA |
		      SOFT_RESET_VC |
		      SOFT_RESET_VGT);

	dev_info(rdev->dev, "  GRBM_SOFT_RESET=0x%08X\n", grbm_reset);
	WREG32(GRBM_SOFT_RESET, grbm_reset);
	(void)RREG32(GRBM_SOFT_RESET);
	udelay(50);
	WREG32(GRBM_SOFT_RESET, 0);
	(void)RREG32(GRBM_SOFT_RESET);

	dev_info(rdev->dev, "  GRBM_STATUS               = 0x%08X\n",
		RREG32(GRBM_STATUS));
	dev_info(rdev->dev, "  GRBM_STATUS_SE0           = 0x%08X\n",
		RREG32(GRBM_STATUS_SE0));
	dev_info(rdev->dev, "  GRBM_STATUS_SE1           = 0x%08X\n",
		RREG32(GRBM_STATUS_SE1));
	dev_info(rdev->dev, "  SRBM_STATUS               = 0x%08X\n",
		RREG32(SRBM_STATUS));
	dev_info(rdev->dev, "  R_008674_CP_STALLED_STAT1 = 0x%08X\n",
		RREG32(CP_STALLED_STAT1));
	dev_info(rdev->dev, "  R_008678_CP_STALLED_STAT2 = 0x%08X\n",
		RREG32(CP_STALLED_STAT2));
	dev_info(rdev->dev, "  R_00867C_CP_BUSY_STAT     = 0x%08X\n",
		RREG32(CP_BUSY_STAT));
	dev_info(rdev->dev, "  R_008680_CP_STAT          = 0x%08X\n",
		RREG32(CP_STAT));
}

static void evergreen_gpu_soft_reset_dma(struct radeon_device *rdev)
{
	u32 tmp;

	if (RREG32(DMA_STATUS_REG) & DMA_IDLE)
		return;

	dev_info(rdev->dev, "  R_00D034_DMA_STATUS_REG   = 0x%08X\n",
		RREG32(DMA_STATUS_REG));

	/* Disable DMA */
	tmp = RREG32(DMA_RB_CNTL);
	tmp &= ~DMA_RB_ENABLE;
	WREG32(DMA_RB_CNTL, tmp);

	/* Reset dma */
	WREG32(SRBM_SOFT_RESET, SOFT_RESET_DMA);
	RREG32(SRBM_SOFT_RESET);
	udelay(50);
	WREG32(SRBM_SOFT_RESET, 0);

	dev_info(rdev->dev, "  R_00D034_DMA_STATUS_REG   = 0x%08X\n",
		RREG32(DMA_STATUS_REG));
}

static int evergreen_gpu_soft_reset(struct radeon_device *rdev, u32 reset_mask)
{
	struct evergreen_mc_save save;

	if (!(RREG32(GRBM_STATUS) & GUI_ACTIVE))
		reset_mask &= ~(RADEON_RESET_GFX | RADEON_RESET_COMPUTE);

	if (RREG32(DMA_STATUS_REG) & DMA_IDLE)
		reset_mask &= ~RADEON_RESET_DMA;

	if (reset_mask == 0)
		return 0;

	dev_info(rdev->dev, "GPU softreset: 0x%08X\n", reset_mask);

	evergreen_mc_stop(rdev, &save);
	if (evergreen_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}

	if (reset_mask & (RADEON_RESET_GFX | RADEON_RESET_COMPUTE))
		evergreen_gpu_soft_reset_gfx(rdev);

	if (reset_mask & RADEON_RESET_DMA)
		evergreen_gpu_soft_reset_dma(rdev);

	/* Wait a little for things to settle down */
	udelay(50);

	evergreen_mc_resume(rdev, &save);
	return 0;
}

int evergreen_asic_reset(struct radeon_device *rdev)
{
	return evergreen_gpu_soft_reset(rdev, (RADEON_RESET_GFX |
					       RADEON_RESET_COMPUTE |
					       RADEON_RESET_DMA));
}

/* Interrupts */

u32 evergreen_get_vblank_counter(struct radeon_device *rdev, int crtc)
{
	if (crtc >= rdev->num_crtc)
		return 0;
	else
		return RREG32(CRTC_STATUS_FRAME_COUNT + crtc_offsets[crtc]);
}

void evergreen_disable_interrupt_state(struct radeon_device *rdev)
{
	u32 tmp;

	if (rdev->family >= CHIP_CAYMAN) {
		cayman_cp_int_cntl_setup(rdev, 0,
					 CNTX_BUSY_INT_ENABLE | CNTX_EMPTY_INT_ENABLE);
		cayman_cp_int_cntl_setup(rdev, 1, 0);
		cayman_cp_int_cntl_setup(rdev, 2, 0);
		tmp = RREG32(CAYMAN_DMA1_CNTL) & ~TRAP_ENABLE;
		WREG32(CAYMAN_DMA1_CNTL, tmp);
	} else
		WREG32(CP_INT_CNTL, CNTX_BUSY_INT_ENABLE | CNTX_EMPTY_INT_ENABLE);
	tmp = RREG32(DMA_CNTL) & ~TRAP_ENABLE;
	WREG32(DMA_CNTL, tmp);
	WREG32(GRBM_INT_CNTL, 0);
	WREG32(INT_MASK + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(INT_MASK + EVERGREEN_CRTC1_REGISTER_OFFSET, 0);
	if (rdev->num_crtc >= 4) {
		WREG32(INT_MASK + EVERGREEN_CRTC2_REGISTER_OFFSET, 0);
		WREG32(INT_MASK + EVERGREEN_CRTC3_REGISTER_OFFSET, 0);
	}
	if (rdev->num_crtc >= 6) {
		WREG32(INT_MASK + EVERGREEN_CRTC4_REGISTER_OFFSET, 0);
		WREG32(INT_MASK + EVERGREEN_CRTC5_REGISTER_OFFSET, 0);
	}

	WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET, 0);
	if (rdev->num_crtc >= 4) {
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET, 0);
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET, 0);
	}
	if (rdev->num_crtc >= 6) {
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET, 0);
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET, 0);
	}

	/* only one DAC on DCE6 */
	if (!ASIC_IS_DCE6(rdev))
		WREG32(DACA_AUTODETECT_INT_CONTROL, 0);
	WREG32(DACB_AUTODETECT_INT_CONTROL, 0);

	tmp = RREG32(DC_HPD1_INT_CONTROL) & DC_HPDx_INT_POLARITY;
	WREG32(DC_HPD1_INT_CONTROL, tmp);
	tmp = RREG32(DC_HPD2_INT_CONTROL) & DC_HPDx_INT_POLARITY;
	WREG32(DC_HPD2_INT_CONTROL, tmp);
	tmp = RREG32(DC_HPD3_INT_CONTROL) & DC_HPDx_INT_POLARITY;
	WREG32(DC_HPD3_INT_CONTROL, tmp);
	tmp = RREG32(DC_HPD4_INT_CONTROL) & DC_HPDx_INT_POLARITY;
	WREG32(DC_HPD4_INT_CONTROL, tmp);
	tmp = RREG32(DC_HPD5_INT_CONTROL) & DC_HPDx_INT_POLARITY;
	WREG32(DC_HPD5_INT_CONTROL, tmp);
	tmp = RREG32(DC_HPD6_INT_CONTROL) & DC_HPDx_INT_POLARITY;
	WREG32(DC_HPD6_INT_CONTROL, tmp);

}

int evergreen_irq_set(struct radeon_device *rdev)
{
	u32 cp_int_cntl = CNTX_BUSY_INT_ENABLE | CNTX_EMPTY_INT_ENABLE;
	u32 cp_int_cntl1 = 0, cp_int_cntl2 = 0;
	u32 crtc1 = 0, crtc2 = 0, crtc3 = 0, crtc4 = 0, crtc5 = 0, crtc6 = 0;
	u32 hpd1, hpd2, hpd3, hpd4, hpd5, hpd6;
	u32 grbm_int_cntl = 0;
	u32 grph1 = 0, grph2 = 0, grph3 = 0, grph4 = 0, grph5 = 0, grph6 = 0;
	u32 afmt1 = 0, afmt2 = 0, afmt3 = 0, afmt4 = 0, afmt5 = 0, afmt6 = 0;
	u32 dma_cntl, dma_cntl1 = 0;

	if (!rdev->irq.installed) {
		WARN(1, "Can't enable IRQ/MSI because no handler is installed\n");
		return -EINVAL;
	}
	/* don't enable anything if the ih is disabled */
	if (!rdev->ih.enabled) {
		r600_disable_interrupts(rdev);
		/* force the active interrupt state to all disabled */
		evergreen_disable_interrupt_state(rdev);
		return 0;
	}

	hpd1 = RREG32(DC_HPD1_INT_CONTROL) & ~DC_HPDx_INT_EN;
	hpd2 = RREG32(DC_HPD2_INT_CONTROL) & ~DC_HPDx_INT_EN;
	hpd3 = RREG32(DC_HPD3_INT_CONTROL) & ~DC_HPDx_INT_EN;
	hpd4 = RREG32(DC_HPD4_INT_CONTROL) & ~DC_HPDx_INT_EN;
	hpd5 = RREG32(DC_HPD5_INT_CONTROL) & ~DC_HPDx_INT_EN;
	hpd6 = RREG32(DC_HPD6_INT_CONTROL) & ~DC_HPDx_INT_EN;

	afmt1 = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET) & ~AFMT_AZ_FORMAT_WTRIG_MASK;
	afmt2 = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET) & ~AFMT_AZ_FORMAT_WTRIG_MASK;
	afmt3 = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET) & ~AFMT_AZ_FORMAT_WTRIG_MASK;
	afmt4 = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET) & ~AFMT_AZ_FORMAT_WTRIG_MASK;
	afmt5 = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET) & ~AFMT_AZ_FORMAT_WTRIG_MASK;
	afmt6 = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET) & ~AFMT_AZ_FORMAT_WTRIG_MASK;

	dma_cntl = RREG32(DMA_CNTL) & ~TRAP_ENABLE;

	if (rdev->family >= CHIP_CAYMAN) {
		/* enable CP interrupts on all rings */
		if (atomic_read(&rdev->irq.ring_int[RADEON_RING_TYPE_GFX_INDEX])) {
			DRM_DEBUG("evergreen_irq_set: sw int gfx\n");
			cp_int_cntl |= TIME_STAMP_INT_ENABLE;
		}
		if (atomic_read(&rdev->irq.ring_int[CAYMAN_RING_TYPE_CP1_INDEX])) {
			DRM_DEBUG("evergreen_irq_set: sw int cp1\n");
			cp_int_cntl1 |= TIME_STAMP_INT_ENABLE;
		}
		if (atomic_read(&rdev->irq.ring_int[CAYMAN_RING_TYPE_CP2_INDEX])) {
			DRM_DEBUG("evergreen_irq_set: sw int cp2\n");
			cp_int_cntl2 |= TIME_STAMP_INT_ENABLE;
		}
	} else {
		if (atomic_read(&rdev->irq.ring_int[RADEON_RING_TYPE_GFX_INDEX])) {
			DRM_DEBUG("evergreen_irq_set: sw int gfx\n");
			cp_int_cntl |= RB_INT_ENABLE;
			cp_int_cntl |= TIME_STAMP_INT_ENABLE;
		}
	}

	if (atomic_read(&rdev->irq.ring_int[R600_RING_TYPE_DMA_INDEX])) {
		DRM_DEBUG("r600_irq_set: sw int dma\n");
		dma_cntl |= TRAP_ENABLE;
	}

	if (rdev->family >= CHIP_CAYMAN) {
		dma_cntl1 = RREG32(CAYMAN_DMA1_CNTL) & ~TRAP_ENABLE;
		if (atomic_read(&rdev->irq.ring_int[CAYMAN_RING_TYPE_DMA1_INDEX])) {
			DRM_DEBUG("r600_irq_set: sw int dma1\n");
			dma_cntl1 |= TRAP_ENABLE;
		}
	}

	if (rdev->irq.crtc_vblank_int[0] ||
	    atomic_read(&rdev->irq.pflip[0])) {
		DRM_DEBUG("evergreen_irq_set: vblank 0\n");
		crtc1 |= VBLANK_INT_MASK;
	}
	if (rdev->irq.crtc_vblank_int[1] ||
	    atomic_read(&rdev->irq.pflip[1])) {
		DRM_DEBUG("evergreen_irq_set: vblank 1\n");
		crtc2 |= VBLANK_INT_MASK;
	}
	if (rdev->irq.crtc_vblank_int[2] ||
	    atomic_read(&rdev->irq.pflip[2])) {
		DRM_DEBUG("evergreen_irq_set: vblank 2\n");
		crtc3 |= VBLANK_INT_MASK;
	}
	if (rdev->irq.crtc_vblank_int[3] ||
	    atomic_read(&rdev->irq.pflip[3])) {
		DRM_DEBUG("evergreen_irq_set: vblank 3\n");
		crtc4 |= VBLANK_INT_MASK;
	}
	if (rdev->irq.crtc_vblank_int[4] ||
	    atomic_read(&rdev->irq.pflip[4])) {
		DRM_DEBUG("evergreen_irq_set: vblank 4\n");
		crtc5 |= VBLANK_INT_MASK;
	}
	if (rdev->irq.crtc_vblank_int[5] ||
	    atomic_read(&rdev->irq.pflip[5])) {
		DRM_DEBUG("evergreen_irq_set: vblank 5\n");
		crtc6 |= VBLANK_INT_MASK;
	}
	if (rdev->irq.hpd[0]) {
		DRM_DEBUG("evergreen_irq_set: hpd 1\n");
		hpd1 |= DC_HPDx_INT_EN;
	}
	if (rdev->irq.hpd[1]) {
		DRM_DEBUG("evergreen_irq_set: hpd 2\n");
		hpd2 |= DC_HPDx_INT_EN;
	}
	if (rdev->irq.hpd[2]) {
		DRM_DEBUG("evergreen_irq_set: hpd 3\n");
		hpd3 |= DC_HPDx_INT_EN;
	}
	if (rdev->irq.hpd[3]) {
		DRM_DEBUG("evergreen_irq_set: hpd 4\n");
		hpd4 |= DC_HPDx_INT_EN;
	}
	if (rdev->irq.hpd[4]) {
		DRM_DEBUG("evergreen_irq_set: hpd 5\n");
		hpd5 |= DC_HPDx_INT_EN;
	}
	if (rdev->irq.hpd[5]) {
		DRM_DEBUG("evergreen_irq_set: hpd 6\n");
		hpd6 |= DC_HPDx_INT_EN;
	}
	if (rdev->irq.afmt[0]) {
		DRM_DEBUG("evergreen_irq_set: hdmi 0\n");
		afmt1 |= AFMT_AZ_FORMAT_WTRIG_MASK;
	}
	if (rdev->irq.afmt[1]) {
		DRM_DEBUG("evergreen_irq_set: hdmi 1\n");
		afmt2 |= AFMT_AZ_FORMAT_WTRIG_MASK;
	}
	if (rdev->irq.afmt[2]) {
		DRM_DEBUG("evergreen_irq_set: hdmi 2\n");
		afmt3 |= AFMT_AZ_FORMAT_WTRIG_MASK;
	}
	if (rdev->irq.afmt[3]) {
		DRM_DEBUG("evergreen_irq_set: hdmi 3\n");
		afmt4 |= AFMT_AZ_FORMAT_WTRIG_MASK;
	}
	if (rdev->irq.afmt[4]) {
		DRM_DEBUG("evergreen_irq_set: hdmi 4\n");
		afmt5 |= AFMT_AZ_FORMAT_WTRIG_MASK;
	}
	if (rdev->irq.afmt[5]) {
		DRM_DEBUG("evergreen_irq_set: hdmi 5\n");
		afmt6 |= AFMT_AZ_FORMAT_WTRIG_MASK;
	}

	if (rdev->family >= CHIP_CAYMAN) {
		cayman_cp_int_cntl_setup(rdev, 0, cp_int_cntl);
		cayman_cp_int_cntl_setup(rdev, 1, cp_int_cntl1);
		cayman_cp_int_cntl_setup(rdev, 2, cp_int_cntl2);
	} else
		WREG32(CP_INT_CNTL, cp_int_cntl);

	WREG32(DMA_CNTL, dma_cntl);

	if (rdev->family >= CHIP_CAYMAN)
		WREG32(CAYMAN_DMA1_CNTL, dma_cntl1);

	WREG32(GRBM_INT_CNTL, grbm_int_cntl);

	WREG32(INT_MASK + EVERGREEN_CRTC0_REGISTER_OFFSET, crtc1);
	WREG32(INT_MASK + EVERGREEN_CRTC1_REGISTER_OFFSET, crtc2);
	if (rdev->num_crtc >= 4) {
		WREG32(INT_MASK + EVERGREEN_CRTC2_REGISTER_OFFSET, crtc3);
		WREG32(INT_MASK + EVERGREEN_CRTC3_REGISTER_OFFSET, crtc4);
	}
	if (rdev->num_crtc >= 6) {
		WREG32(INT_MASK + EVERGREEN_CRTC4_REGISTER_OFFSET, crtc5);
		WREG32(INT_MASK + EVERGREEN_CRTC5_REGISTER_OFFSET, crtc6);
	}

	WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET, grph1);
	WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET, grph2);
	if (rdev->num_crtc >= 4) {
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET, grph3);
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET, grph4);
	}
	if (rdev->num_crtc >= 6) {
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET, grph5);
		WREG32(GRPH_INT_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET, grph6);
	}

	WREG32(DC_HPD1_INT_CONTROL, hpd1);
	WREG32(DC_HPD2_INT_CONTROL, hpd2);
	WREG32(DC_HPD3_INT_CONTROL, hpd3);
	WREG32(DC_HPD4_INT_CONTROL, hpd4);
	WREG32(DC_HPD5_INT_CONTROL, hpd5);
	WREG32(DC_HPD6_INT_CONTROL, hpd6);

	WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET, afmt1);
	WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET, afmt2);
	WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET, afmt3);
	WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET, afmt4);
	WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET, afmt5);
	WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET, afmt6);

	return 0;
}

static void evergreen_irq_ack(struct radeon_device *rdev)
{
	u32 tmp;

	rdev->irq.stat_regs.evergreen.disp_int = RREG32(DISP_INTERRUPT_STATUS);
	rdev->irq.stat_regs.evergreen.disp_int_cont = RREG32(DISP_INTERRUPT_STATUS_CONTINUE);
	rdev->irq.stat_regs.evergreen.disp_int_cont2 = RREG32(DISP_INTERRUPT_STATUS_CONTINUE2);
	rdev->irq.stat_regs.evergreen.disp_int_cont3 = RREG32(DISP_INTERRUPT_STATUS_CONTINUE3);
	rdev->irq.stat_regs.evergreen.disp_int_cont4 = RREG32(DISP_INTERRUPT_STATUS_CONTINUE4);
	rdev->irq.stat_regs.evergreen.disp_int_cont5 = RREG32(DISP_INTERRUPT_STATUS_CONTINUE5);
	rdev->irq.stat_regs.evergreen.d1grph_int = RREG32(GRPH_INT_STATUS + EVERGREEN_CRTC0_REGISTER_OFFSET);
	rdev->irq.stat_regs.evergreen.d2grph_int = RREG32(GRPH_INT_STATUS + EVERGREEN_CRTC1_REGISTER_OFFSET);
	if (rdev->num_crtc >= 4) {
		rdev->irq.stat_regs.evergreen.d3grph_int = RREG32(GRPH_INT_STATUS + EVERGREEN_CRTC2_REGISTER_OFFSET);
		rdev->irq.stat_regs.evergreen.d4grph_int = RREG32(GRPH_INT_STATUS + EVERGREEN_CRTC3_REGISTER_OFFSET);
	}
	if (rdev->num_crtc >= 6) {
		rdev->irq.stat_regs.evergreen.d5grph_int = RREG32(GRPH_INT_STATUS + EVERGREEN_CRTC4_REGISTER_OFFSET);
		rdev->irq.stat_regs.evergreen.d6grph_int = RREG32(GRPH_INT_STATUS + EVERGREEN_CRTC5_REGISTER_OFFSET);
	}

	rdev->irq.stat_regs.evergreen.afmt_status1 = RREG32(AFMT_STATUS + EVERGREEN_CRTC0_REGISTER_OFFSET);
	rdev->irq.stat_regs.evergreen.afmt_status2 = RREG32(AFMT_STATUS + EVERGREEN_CRTC1_REGISTER_OFFSET);
	rdev->irq.stat_regs.evergreen.afmt_status3 = RREG32(AFMT_STATUS + EVERGREEN_CRTC2_REGISTER_OFFSET);
	rdev->irq.stat_regs.evergreen.afmt_status4 = RREG32(AFMT_STATUS + EVERGREEN_CRTC3_REGISTER_OFFSET);
	rdev->irq.stat_regs.evergreen.afmt_status5 = RREG32(AFMT_STATUS + EVERGREEN_CRTC4_REGISTER_OFFSET);
	rdev->irq.stat_regs.evergreen.afmt_status6 = RREG32(AFMT_STATUS + EVERGREEN_CRTC5_REGISTER_OFFSET);

	if (rdev->irq.stat_regs.evergreen.d1grph_int & GRPH_PFLIP_INT_OCCURRED)
		WREG32(GRPH_INT_STATUS + EVERGREEN_CRTC0_REGISTER_OFFSET, GRPH_PFLIP_INT_CLEAR);
	if (rdev->irq.stat_regs.evergreen.d2grph_int & GRPH_PFLIP_INT_OCCURRED)
		WREG32(GRPH_INT_STATUS + EVERGREEN_CRTC1_REGISTER_OFFSET, GRPH_PFLIP_INT_CLEAR);
	if (rdev->irq.stat_regs.evergreen.disp_int & LB_D1_VBLANK_INTERRUPT)
		WREG32(VBLANK_STATUS + EVERGREEN_CRTC0_REGISTER_OFFSET, VBLANK_ACK);
	if (rdev->irq.stat_regs.evergreen.disp_int & LB_D1_VLINE_INTERRUPT)
		WREG32(VLINE_STATUS + EVERGREEN_CRTC0_REGISTER_OFFSET, VLINE_ACK);
	if (rdev->irq.stat_regs.evergreen.disp_int_cont & LB_D2_VBLANK_INTERRUPT)
		WREG32(VBLANK_STATUS + EVERGREEN_CRTC1_REGISTER_OFFSET, VBLANK_ACK);
	if (rdev->irq.stat_regs.evergreen.disp_int_cont & LB_D2_VLINE_INTERRUPT)
		WREG32(VLINE_STATUS + EVERGREEN_CRTC1_REGISTER_OFFSET, VLINE_ACK);

	if (rdev->num_crtc >= 4) {
		if (rdev->irq.stat_regs.evergreen.d3grph_int & GRPH_PFLIP_INT_OCCURRED)
			WREG32(GRPH_INT_STATUS + EVERGREEN_CRTC2_REGISTER_OFFSET, GRPH_PFLIP_INT_CLEAR);
		if (rdev->irq.stat_regs.evergreen.d4grph_int & GRPH_PFLIP_INT_OCCURRED)
			WREG32(GRPH_INT_STATUS + EVERGREEN_CRTC3_REGISTER_OFFSET, GRPH_PFLIP_INT_CLEAR);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont2 & LB_D3_VBLANK_INTERRUPT)
			WREG32(VBLANK_STATUS + EVERGREEN_CRTC2_REGISTER_OFFSET, VBLANK_ACK);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont2 & LB_D3_VLINE_INTERRUPT)
			WREG32(VLINE_STATUS + EVERGREEN_CRTC2_REGISTER_OFFSET, VLINE_ACK);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont3 & LB_D4_VBLANK_INTERRUPT)
			WREG32(VBLANK_STATUS + EVERGREEN_CRTC3_REGISTER_OFFSET, VBLANK_ACK);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont3 & LB_D4_VLINE_INTERRUPT)
			WREG32(VLINE_STATUS + EVERGREEN_CRTC3_REGISTER_OFFSET, VLINE_ACK);
	}

	if (rdev->num_crtc >= 6) {
		if (rdev->irq.stat_regs.evergreen.d5grph_int & GRPH_PFLIP_INT_OCCURRED)
			WREG32(GRPH_INT_STATUS + EVERGREEN_CRTC4_REGISTER_OFFSET, GRPH_PFLIP_INT_CLEAR);
		if (rdev->irq.stat_regs.evergreen.d6grph_int & GRPH_PFLIP_INT_OCCURRED)
			WREG32(GRPH_INT_STATUS + EVERGREEN_CRTC5_REGISTER_OFFSET, GRPH_PFLIP_INT_CLEAR);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont4 & LB_D5_VBLANK_INTERRUPT)
			WREG32(VBLANK_STATUS + EVERGREEN_CRTC4_REGISTER_OFFSET, VBLANK_ACK);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont4 & LB_D5_VLINE_INTERRUPT)
			WREG32(VLINE_STATUS + EVERGREEN_CRTC4_REGISTER_OFFSET, VLINE_ACK);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont5 & LB_D6_VBLANK_INTERRUPT)
			WREG32(VBLANK_STATUS + EVERGREEN_CRTC5_REGISTER_OFFSET, VBLANK_ACK);
		if (rdev->irq.stat_regs.evergreen.disp_int_cont5 & LB_D6_VLINE_INTERRUPT)
			WREG32(VLINE_STATUS + EVERGREEN_CRTC5_REGISTER_OFFSET, VLINE_ACK);
	}

	if (rdev->irq.stat_regs.evergreen.disp_int & DC_HPD1_INTERRUPT) {
		tmp = RREG32(DC_HPD1_INT_CONTROL);
		tmp |= DC_HPDx_INT_ACK;
		WREG32(DC_HPD1_INT_CONTROL, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.disp_int_cont & DC_HPD2_INTERRUPT) {
		tmp = RREG32(DC_HPD2_INT_CONTROL);
		tmp |= DC_HPDx_INT_ACK;
		WREG32(DC_HPD2_INT_CONTROL, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.disp_int_cont2 & DC_HPD3_INTERRUPT) {
		tmp = RREG32(DC_HPD3_INT_CONTROL);
		tmp |= DC_HPDx_INT_ACK;
		WREG32(DC_HPD3_INT_CONTROL, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.disp_int_cont3 & DC_HPD4_INTERRUPT) {
		tmp = RREG32(DC_HPD4_INT_CONTROL);
		tmp |= DC_HPDx_INT_ACK;
		WREG32(DC_HPD4_INT_CONTROL, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.disp_int_cont4 & DC_HPD5_INTERRUPT) {
		tmp = RREG32(DC_HPD5_INT_CONTROL);
		tmp |= DC_HPDx_INT_ACK;
		WREG32(DC_HPD5_INT_CONTROL, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.disp_int_cont5 & DC_HPD6_INTERRUPT) {
		tmp = RREG32(DC_HPD5_INT_CONTROL);
		tmp |= DC_HPDx_INT_ACK;
		WREG32(DC_HPD6_INT_CONTROL, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.afmt_status1 & AFMT_AZ_FORMAT_WTRIG) {
		tmp = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET);
		tmp |= AFMT_AZ_FORMAT_WTRIG_ACK;
		WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.afmt_status2 & AFMT_AZ_FORMAT_WTRIG) {
		tmp = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET);
		tmp |= AFMT_AZ_FORMAT_WTRIG_ACK;
		WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.afmt_status3 & AFMT_AZ_FORMAT_WTRIG) {
		tmp = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET);
		tmp |= AFMT_AZ_FORMAT_WTRIG_ACK;
		WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.afmt_status4 & AFMT_AZ_FORMAT_WTRIG) {
		tmp = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET);
		tmp |= AFMT_AZ_FORMAT_WTRIG_ACK;
		WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.afmt_status5 & AFMT_AZ_FORMAT_WTRIG) {
		tmp = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET);
		tmp |= AFMT_AZ_FORMAT_WTRIG_ACK;
		WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET, tmp);
	}
	if (rdev->irq.stat_regs.evergreen.afmt_status6 & AFMT_AZ_FORMAT_WTRIG) {
		tmp = RREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET);
		tmp |= AFMT_AZ_FORMAT_WTRIG_ACK;
		WREG32(AFMT_AUDIO_PACKET_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET, tmp);
	}
}

static void evergreen_irq_disable(struct radeon_device *rdev)
{
	r600_disable_interrupts(rdev);
	/* Wait and acknowledge irq */
	mdelay(1);
	evergreen_irq_ack(rdev);
	evergreen_disable_interrupt_state(rdev);
}

void evergreen_irq_suspend(struct radeon_device *rdev)
{
	evergreen_irq_disable(rdev);
	r600_rlc_stop(rdev);
}

static u32 evergreen_get_ih_wptr(struct radeon_device *rdev)
{
	u32 wptr, tmp;

	if (rdev->wb.enabled)
		wptr = le32_to_cpu(rdev->wb.wb[R600_WB_IH_WPTR_OFFSET/4]);
	else
		wptr = RREG32(IH_RB_WPTR);

	if (wptr & RB_OVERFLOW) {
		/* When a ring buffer overflow happen start parsing interrupt
		 * from the last not overwritten vector (wptr + 16). Hopefully
		 * this should allow us to catchup.
		 */
		dev_warn(rdev->dev, "IH ring buffer overflow (0x%08X, %d, %d)\n",
			wptr, rdev->ih.rptr, (wptr + 16) + rdev->ih.ptr_mask);
		rdev->ih.rptr = (wptr + 16) & rdev->ih.ptr_mask;
		tmp = RREG32(IH_RB_CNTL);
		tmp |= IH_WPTR_OVERFLOW_CLEAR;
		WREG32(IH_RB_CNTL, tmp);
	}
	return (wptr & rdev->ih.ptr_mask);
}

int evergreen_irq_process(struct radeon_device *rdev)
{
	u32 wptr;
	u32 rptr;
	u32 src_id, src_data;
	u32 ring_index;
	bool queue_hotplug = false;
	bool queue_hdmi = false;

	if (!rdev->ih.enabled || rdev->shutdown)
		return IRQ_NONE;

	wptr = evergreen_get_ih_wptr(rdev);

	if (wptr == rdev->ih.rptr)
		return IRQ_NONE;
restart_ih:
	/* is somebody else already processing irqs? */
	if (atomic_xchg(&rdev->ih.lock, 1))
		return IRQ_NONE;

	rptr = rdev->ih.rptr;
	DRM_DEBUG("r600_irq_process start: rptr %d, wptr %d\n", rptr, wptr);

	/* Order reading of wptr vs. reading of IH ring data */
	rmb();

	/* display interrupts */
	evergreen_irq_ack(rdev);

	while (rptr != wptr) {
		/* wptr/rptr are in bytes! */
		ring_index = rptr / 4;
		src_id =  le32_to_cpu(rdev->ih.ring[ring_index]) & 0xff;
		src_data = le32_to_cpu(rdev->ih.ring[ring_index + 1]) & 0xfffffff;

		switch (src_id) {
		case 1: /* D1 vblank/vline */
			switch (src_data) {
			case 0: /* D1 vblank */
				if (rdev->irq.stat_regs.evergreen.disp_int & LB_D1_VBLANK_INTERRUPT) {
					if (rdev->irq.crtc_vblank_int[0]) {
						drm_handle_vblank(rdev->ddev, 0);
						rdev->pm.vblank_sync = true;
						wake_up(&rdev->irq.vblank_queue);
					}
					if (atomic_read(&rdev->irq.pflip[0]))
						radeon_crtc_handle_flip(rdev, 0);
					rdev->irq.stat_regs.evergreen.disp_int &= ~LB_D1_VBLANK_INTERRUPT;
					DRM_DEBUG("IH: D1 vblank\n");
				}
				break;
			case 1: /* D1 vline */
				if (rdev->irq.stat_regs.evergreen.disp_int & LB_D1_VLINE_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int &= ~LB_D1_VLINE_INTERRUPT;
					DRM_DEBUG("IH: D1 vline\n");
				}
				break;
			default:
				DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 2: /* D2 vblank/vline */
			switch (src_data) {
			case 0: /* D2 vblank */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont & LB_D2_VBLANK_INTERRUPT) {
					if (rdev->irq.crtc_vblank_int[1]) {
						drm_handle_vblank(rdev->ddev, 1);
						rdev->pm.vblank_sync = true;
						wake_up(&rdev->irq.vblank_queue);
					}
					if (atomic_read(&rdev->irq.pflip[1]))
						radeon_crtc_handle_flip(rdev, 1);
					rdev->irq.stat_regs.evergreen.disp_int_cont &= ~LB_D2_VBLANK_INTERRUPT;
					DRM_DEBUG("IH: D2 vblank\n");
				}
				break;
			case 1: /* D2 vline */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont & LB_D2_VLINE_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont &= ~LB_D2_VLINE_INTERRUPT;
					DRM_DEBUG("IH: D2 vline\n");
				}
				break;
			default:
				DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 3: /* D3 vblank/vline */
			switch (src_data) {
			case 0: /* D3 vblank */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont2 & LB_D3_VBLANK_INTERRUPT) {
					if (rdev->irq.crtc_vblank_int[2]) {
						drm_handle_vblank(rdev->ddev, 2);
						rdev->pm.vblank_sync = true;
						wake_up(&rdev->irq.vblank_queue);
					}
					if (atomic_read(&rdev->irq.pflip[2]))
						radeon_crtc_handle_flip(rdev, 2);
					rdev->irq.stat_regs.evergreen.disp_int_cont2 &= ~LB_D3_VBLANK_INTERRUPT;
					DRM_DEBUG("IH: D3 vblank\n");
				}
				break;
			case 1: /* D3 vline */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont2 & LB_D3_VLINE_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont2 &= ~LB_D3_VLINE_INTERRUPT;
					DRM_DEBUG("IH: D3 vline\n");
				}
				break;
			default:
				DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 4: /* D4 vblank/vline */
			switch (src_data) {
			case 0: /* D4 vblank */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont3 & LB_D4_VBLANK_INTERRUPT) {
					if (rdev->irq.crtc_vblank_int[3]) {
						drm_handle_vblank(rdev->ddev, 3);
						rdev->pm.vblank_sync = true;
						wake_up(&rdev->irq.vblank_queue);
					}
					if (atomic_read(&rdev->irq.pflip[3]))
						radeon_crtc_handle_flip(rdev, 3);
					rdev->irq.stat_regs.evergreen.disp_int_cont3 &= ~LB_D4_VBLANK_INTERRUPT;
					DRM_DEBUG("IH: D4 vblank\n");
				}
				break;
			case 1: /* D4 vline */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont3 & LB_D4_VLINE_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont3 &= ~LB_D4_VLINE_INTERRUPT;
					DRM_DEBUG("IH: D4 vline\n");
				}
				break;
			default:
				DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 5: /* D5 vblank/vline */
			switch (src_data) {
			case 0: /* D5 vblank */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont4 & LB_D5_VBLANK_INTERRUPT) {
					if (rdev->irq.crtc_vblank_int[4]) {
						drm_handle_vblank(rdev->ddev, 4);
						rdev->pm.vblank_sync = true;
						wake_up(&rdev->irq.vblank_queue);
					}
					if (atomic_read(&rdev->irq.pflip[4]))
						radeon_crtc_handle_flip(rdev, 4);
					rdev->irq.stat_regs.evergreen.disp_int_cont4 &= ~LB_D5_VBLANK_INTERRUPT;
					DRM_DEBUG("IH: D5 vblank\n");
				}
				break;
			case 1: /* D5 vline */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont4 & LB_D5_VLINE_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont4 &= ~LB_D5_VLINE_INTERRUPT;
					DRM_DEBUG("IH: D5 vline\n");
				}
				break;
			default:
				DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 6: /* D6 vblank/vline */
			switch (src_data) {
			case 0: /* D6 vblank */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont5 & LB_D6_VBLANK_INTERRUPT) {
					if (rdev->irq.crtc_vblank_int[5]) {
						drm_handle_vblank(rdev->ddev, 5);
						rdev->pm.vblank_sync = true;
						wake_up(&rdev->irq.vblank_queue);
					}
					if (atomic_read(&rdev->irq.pflip[5]))
						radeon_crtc_handle_flip(rdev, 5);
					rdev->irq.stat_regs.evergreen.disp_int_cont5 &= ~LB_D6_VBLANK_INTERRUPT;
					DRM_DEBUG("IH: D6 vblank\n");
				}
				break;
			case 1: /* D6 vline */
				if (rdev->irq.stat_regs.evergreen.disp_int_cont5 & LB_D6_VLINE_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont5 &= ~LB_D6_VLINE_INTERRUPT;
					DRM_DEBUG("IH: D6 vline\n");
				}
				break;
			default:
				DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 42: /* HPD hotplug */
			switch (src_data) {
			case 0:
				if (rdev->irq.stat_regs.evergreen.disp_int & DC_HPD1_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int &= ~DC_HPD1_INTERRUPT;
					queue_hotplug = true;
					DRM_DEBUG("IH: HPD1\n");
				}
				break;
			case 1:
				if (rdev->irq.stat_regs.evergreen.disp_int_cont & DC_HPD2_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont &= ~DC_HPD2_INTERRUPT;
					queue_hotplug = true;
					DRM_DEBUG("IH: HPD2\n");
				}
				break;
			case 2:
				if (rdev->irq.stat_regs.evergreen.disp_int_cont2 & DC_HPD3_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont2 &= ~DC_HPD3_INTERRUPT;
					queue_hotplug = true;
					DRM_DEBUG("IH: HPD3\n");
				}
				break;
			case 3:
				if (rdev->irq.stat_regs.evergreen.disp_int_cont3 & DC_HPD4_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont3 &= ~DC_HPD4_INTERRUPT;
					queue_hotplug = true;
					DRM_DEBUG("IH: HPD4\n");
				}
				break;
			case 4:
				if (rdev->irq.stat_regs.evergreen.disp_int_cont4 & DC_HPD5_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont4 &= ~DC_HPD5_INTERRUPT;
					queue_hotplug = true;
					DRM_DEBUG("IH: HPD5\n");
				}
				break;
			case 5:
				if (rdev->irq.stat_regs.evergreen.disp_int_cont5 & DC_HPD6_INTERRUPT) {
					rdev->irq.stat_regs.evergreen.disp_int_cont5 &= ~DC_HPD6_INTERRUPT;
					queue_hotplug = true;
					DRM_DEBUG("IH: HPD6\n");
				}
				break;
			default:
				DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 44: /* hdmi */
			switch (src_data) {
			case 0:
				if (rdev->irq.stat_regs.evergreen.afmt_status1 & AFMT_AZ_FORMAT_WTRIG) {
					rdev->irq.stat_regs.evergreen.afmt_status1 &= ~AFMT_AZ_FORMAT_WTRIG;
					queue_hdmi = true;
					DRM_DEBUG("IH: HDMI0\n");
				}
				break;
			case 1:
				if (rdev->irq.stat_regs.evergreen.afmt_status2 & AFMT_AZ_FORMAT_WTRIG) {
					rdev->irq.stat_regs.evergreen.afmt_status2 &= ~AFMT_AZ_FORMAT_WTRIG;
					queue_hdmi = true;
					DRM_DEBUG("IH: HDMI1\n");
				}
				break;
			case 2:
				if (rdev->irq.stat_regs.evergreen.afmt_status3 & AFMT_AZ_FORMAT_WTRIG) {
					rdev->irq.stat_regs.evergreen.afmt_status3 &= ~AFMT_AZ_FORMAT_WTRIG;
					queue_hdmi = true;
					DRM_DEBUG("IH: HDMI2\n");
				}
				break;
			case 3:
				if (rdev->irq.stat_regs.evergreen.afmt_status4 & AFMT_AZ_FORMAT_WTRIG) {
					rdev->irq.stat_regs.evergreen.afmt_status4 &= ~AFMT_AZ_FORMAT_WTRIG;
					queue_hdmi = true;
					DRM_DEBUG("IH: HDMI3\n");
				}
				break;
			case 4:
				if (rdev->irq.stat_regs.evergreen.afmt_status5 & AFMT_AZ_FORMAT_WTRIG) {
					rdev->irq.stat_regs.evergreen.afmt_status5 &= ~AFMT_AZ_FORMAT_WTRIG;
					queue_hdmi = true;
					DRM_DEBUG("IH: HDMI4\n");
				}
				break;
			case 5:
				if (rdev->irq.stat_regs.evergreen.afmt_status6 & AFMT_AZ_FORMAT_WTRIG) {
					rdev->irq.stat_regs.evergreen.afmt_status6 &= ~AFMT_AZ_FORMAT_WTRIG;
					queue_hdmi = true;
					DRM_DEBUG("IH: HDMI5\n");
				}
				break;
			default:
				DRM_ERROR("Unhandled interrupt: %d %d\n", src_id, src_data);
				break;
			}
			break;
		case 146:
		case 147:
			dev_err(rdev->dev, "GPU fault detected: %d 0x%08x\n", src_id, src_data);
			dev_err(rdev->dev, "  VM_CONTEXT1_PROTECTION_FAULT_ADDR   0x%08X\n",
				RREG32(VM_CONTEXT1_PROTECTION_FAULT_ADDR));
			dev_err(rdev->dev, "  VM_CONTEXT1_PROTECTION_FAULT_STATUS 0x%08X\n",
				RREG32(VM_CONTEXT1_PROTECTION_FAULT_STATUS));
			/* reset addr and status */
			WREG32_P(VM_CONTEXT1_CNTL2, 1, ~1);
			break;
		case 176: /* CP_INT in ring buffer */
		case 177: /* CP_INT in IB1 */
		case 178: /* CP_INT in IB2 */
			DRM_DEBUG("IH: CP int: 0x%08x\n", src_data);
			radeon_fence_process(rdev, RADEON_RING_TYPE_GFX_INDEX);
			break;
		case 181: /* CP EOP event */
			DRM_DEBUG("IH: CP EOP\n");
			if (rdev->family >= CHIP_CAYMAN) {
				switch (src_data) {
				case 0:
					radeon_fence_process(rdev, RADEON_RING_TYPE_GFX_INDEX);
					break;
				case 1:
					radeon_fence_process(rdev, CAYMAN_RING_TYPE_CP1_INDEX);
					break;
				case 2:
					radeon_fence_process(rdev, CAYMAN_RING_TYPE_CP2_INDEX);
					break;
				}
			} else
				radeon_fence_process(rdev, RADEON_RING_TYPE_GFX_INDEX);
			break;
		case 224: /* DMA trap event */
			DRM_DEBUG("IH: DMA trap\n");
			radeon_fence_process(rdev, R600_RING_TYPE_DMA_INDEX);
			break;
		case 233: /* GUI IDLE */
			DRM_DEBUG("IH: GUI idle\n");
			break;
		case 244: /* DMA trap event */
			if (rdev->family >= CHIP_CAYMAN) {
				DRM_DEBUG("IH: DMA1 trap\n");
				radeon_fence_process(rdev, CAYMAN_RING_TYPE_DMA1_INDEX);
			}
			break;
		default:
			DRM_DEBUG("Unhandled interrupt: %d %d\n", src_id, src_data);
			break;
		}

		/* wptr/rptr are in bytes! */
		rptr += 16;
		rptr &= rdev->ih.ptr_mask;
	}
	if (queue_hotplug)
		task_add(systq, &rdev->hotplug_task);
	if (queue_hdmi)
		task_add(systq, &rdev->audio_task);
	rdev->ih.rptr = rptr;
	WREG32(IH_RB_RPTR, rdev->ih.rptr);
	atomic_set(&rdev->ih.lock, 0);

	/* make sure wptr hasn't changed while processing */
	wptr = evergreen_get_ih_wptr(rdev);
	if (wptr != rptr)
		goto restart_ih;

	return IRQ_HANDLED;
}

/**
 * evergreen_dma_fence_ring_emit - emit a fence on the DMA ring
 *
 * @rdev: radeon_device pointer
 * @fence: radeon fence object
 *
 * Add a DMA fence packet to the ring to write
 * the fence seq number and DMA trap packet to generate
 * an interrupt if needed (evergreen-SI).
 */
void evergreen_dma_fence_ring_emit(struct radeon_device *rdev,
				   struct radeon_fence *fence)
{
	struct radeon_ring *ring = &rdev->ring[fence->ring];
	u64 addr = rdev->fence_drv[fence->ring].gpu_addr;
	/* write the fence */
	radeon_ring_write(ring, DMA_PACKET(DMA_PACKET_FENCE, 0, 0, 0));
	radeon_ring_write(ring, addr & 0xfffffffc);
	radeon_ring_write(ring, (upper_32_bits(addr) & 0xff));
	radeon_ring_write(ring, fence->seq);
	/* generate an interrupt */
	radeon_ring_write(ring, DMA_PACKET(DMA_PACKET_TRAP, 0, 0, 0));
	/* flush HDP */
	radeon_ring_write(ring, DMA_PACKET(DMA_PACKET_SRBM_WRITE, 0, 0, 0));
	radeon_ring_write(ring, (0xf << 16) | (HDP_MEM_COHERENCY_FLUSH_CNTL >> 2));
	radeon_ring_write(ring, 1);
}

/**
 * evergreen_dma_ring_ib_execute - schedule an IB on the DMA engine
 *
 * @rdev: radeon_device pointer
 * @ib: IB object to schedule
 *
 * Schedule an IB in the DMA ring (evergreen).
 */
void evergreen_dma_ring_ib_execute(struct radeon_device *rdev,
				   struct radeon_ib *ib)
{
	struct radeon_ring *ring = &rdev->ring[ib->ring];

	if (rdev->wb.enabled) {
		u32 next_rptr = ring->wptr + 4;
		while ((next_rptr & 7) != 5)
			next_rptr++;
		next_rptr += 3;
		radeon_ring_write(ring, DMA_PACKET(DMA_PACKET_WRITE, 0, 0, 1));
		radeon_ring_write(ring, ring->next_rptr_gpu_addr & 0xfffffffc);
		radeon_ring_write(ring, upper_32_bits(ring->next_rptr_gpu_addr) & 0xff);
		radeon_ring_write(ring, next_rptr);
	}

	/* The indirect buffer packet must end on an 8 DW boundary in the DMA ring.
	 * Pad as necessary with NOPs.
	 */
	while ((ring->wptr & 7) != 5)
		radeon_ring_write(ring, DMA_PACKET(DMA_PACKET_NOP, 0, 0, 0));
	radeon_ring_write(ring, DMA_PACKET(DMA_PACKET_INDIRECT_BUFFER, 0, 0, 0));
	radeon_ring_write(ring, (ib->gpu_addr & 0xFFFFFFE0));
	radeon_ring_write(ring, (ib->length_dw << 12) | (upper_32_bits(ib->gpu_addr) & 0xFF));

}

/**
 * evergreen_copy_dma - copy pages using the DMA engine
 *
 * @rdev: radeon_device pointer
 * @src_offset: src GPU address
 * @dst_offset: dst GPU address
 * @num_gpu_pages: number of GPU pages to xfer
 * @fence: radeon fence object
 *
 * Copy GPU paging using the DMA engine (evergreen-cayman).
 * Used by the radeon ttm implementation to move pages if
 * registered as the asic copy callback.
 */
int evergreen_copy_dma(struct radeon_device *rdev,
		       uint64_t src_offset, uint64_t dst_offset,
		       unsigned num_gpu_pages,
		       struct radeon_fence **fence)
{
	struct radeon_semaphore *sem = NULL;
	int ring_index = rdev->asic->copy.dma_ring_index;
	struct radeon_ring *ring = &rdev->ring[ring_index];
	u32 size_in_dw, cur_size_in_dw;
	int i, num_loops;
	int r = 0;

	r = radeon_semaphore_create(rdev, &sem);
	if (r) {
		DRM_ERROR("radeon: moving bo (%d).\n", r);
		return r;
	}

	size_in_dw = (num_gpu_pages << RADEON_GPU_PAGE_SHIFT) / 4;
	num_loops = DIV_ROUND_UP(size_in_dw, 0xfffff);
	r = radeon_ring_lock(rdev, ring, num_loops * 5 + 11);
	if (r) {
		DRM_ERROR("radeon: moving bo (%d).\n", r);
		radeon_semaphore_free(rdev, &sem, NULL);
		return r;
	}

	if (radeon_fence_need_sync(*fence, ring->idx)) {
		radeon_semaphore_sync_rings(rdev, sem, (*fence)->ring,
					    ring->idx);
		radeon_fence_note_sync(*fence, ring->idx);
	} else {
		radeon_semaphore_free(rdev, &sem, NULL);
	}

	for (i = 0; i < num_loops; i++) {
		cur_size_in_dw = size_in_dw;
		if (cur_size_in_dw > 0xFFFFF)
			cur_size_in_dw = 0xFFFFF;
		size_in_dw -= cur_size_in_dw;
		radeon_ring_write(ring, DMA_PACKET(DMA_PACKET_COPY, 0, 0, cur_size_in_dw));
		radeon_ring_write(ring, dst_offset & 0xfffffffc);
		radeon_ring_write(ring, src_offset & 0xfffffffc);
		radeon_ring_write(ring, upper_32_bits(dst_offset) & 0xff);
		radeon_ring_write(ring, upper_32_bits(src_offset) & 0xff);
		src_offset += cur_size_in_dw * 4;
		dst_offset += cur_size_in_dw * 4;
	}

	r = radeon_fence_emit(rdev, fence, ring->idx);
	if (r) {
		radeon_ring_unlock_undo(rdev, ring);
		return r;
	}

	radeon_ring_unlock_commit(rdev, ring);
	radeon_semaphore_free(rdev, &sem, *fence);

	return r;
}

static int evergreen_startup(struct radeon_device *rdev)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	int r;

	/* enable pcie gen2 link */
	evergreen_pcie_gen2_enable(rdev);

	evergreen_mc_program(rdev);

	if (ASIC_IS_DCE5(rdev)) {
		if (!rdev->me_fw || !rdev->pfp_fw || !rdev->rlc_fw || !rdev->mc_fw) {
			r = ni_init_microcode(rdev);
			if (r) {
				DRM_ERROR("Failed to load firmware!\n");
				return r;
			}
		}
		r = ni_mc_load_microcode(rdev);
		if (r) {
			DRM_ERROR("Failed to load MC firmware!\n");
			return r;
		}
	} else {
		if (!rdev->me_fw || !rdev->pfp_fw || !rdev->rlc_fw) {
			r = r600_init_microcode(rdev);
			if (r) {
				DRM_ERROR("Failed to load firmware!\n");
				return r;
			}
		}
	}

	r = r600_vram_scratch_init(rdev);
	if (r)
		return r;

	if (rdev->flags & RADEON_IS_AGP) {
		evergreen_agp_enable(rdev);
	} else {
		r = evergreen_pcie_gart_enable(rdev);
		if (r)
			return r;
	}
	evergreen_gpu_init(rdev);

	r = evergreen_blit_init(rdev);
	if (r) {
		r600_blit_fini(rdev);
		rdev->asic->copy.copy = NULL;
		dev_warn(rdev->dev, "failed blitter (%d) falling back to memcpy\n", r);
	}

	/* allocate wb buffer */
	r = radeon_wb_init(rdev);
	if (r)
		return r;

	r = radeon_fence_driver_start_ring(rdev, RADEON_RING_TYPE_GFX_INDEX);
	if (r) {
		dev_err(rdev->dev, "failed initializing CP fences (%d).\n", r);
		return r;
	}

	r = radeon_fence_driver_start_ring(rdev, R600_RING_TYPE_DMA_INDEX);
	if (r) {
		dev_err(rdev->dev, "failed initializing DMA fences (%d).\n", r);
		return r;
	}

	/* Enable IRQ */
	if (!rdev->irq.installed) {
		r = radeon_irq_kms_init(rdev);
		if (r)
			return r;
	}

	r = r600_irq_init(rdev);
	if (r) {
		DRM_ERROR("radeon: IH init failed (%d).\n", r);
		radeon_irq_kms_fini(rdev);
		return r;
	}
	evergreen_irq_set(rdev);

	r = radeon_ring_init(rdev, ring, ring->ring_size, RADEON_WB_CP_RPTR_OFFSET,
			     R600_CP_RB_RPTR, R600_CP_RB_WPTR,
			     0, 0xfffff, RADEON_CP_PACKET2);
	if (r)
		return r;

	ring = &rdev->ring[R600_RING_TYPE_DMA_INDEX];
	r = radeon_ring_init(rdev, ring, ring->ring_size, R600_WB_DMA_RPTR_OFFSET,
			     DMA_RB_RPTR, DMA_RB_WPTR,
			     2, 0x3fffc, DMA_PACKET(DMA_PACKET_NOP, 0, 0, 0));
	if (r)
		return r;

	r = evergreen_cp_load_microcode(rdev);
	if (r)
		return r;
	r = evergreen_cp_resume(rdev);
	if (r)
		return r;
	r = r600_dma_resume(rdev);
	if (r)
		return r;

	r = radeon_ib_pool_init(rdev);
	if (r) {
		dev_err(rdev->dev, "IB initialization failed (%d).\n", r);
		return r;
	}

	r = r600_audio_init(rdev);
	if (r) {
		DRM_ERROR("radeon: audio init failed\n");
		return r;
	}

	return 0;
}

int evergreen_resume(struct radeon_device *rdev)
{
	int r;

	/* reset the asic, the gfx blocks are often in a bad state
	 * after the driver is unloaded or after a resume
	 */
	if (radeon_asic_reset(rdev))
		dev_warn(rdev->dev, "GPU reset failed !\n");
	/* Do not reset GPU before posting, on rv770 hw unlike on r500 hw,
	 * posting will perform necessary task to bring back GPU into good
	 * shape.
	 */
	/* post card */
	atom_asic_init(rdev->mode_info.atom_context);

	rdev->accel_working = true;
	r = evergreen_startup(rdev);
	if (r) {
		DRM_ERROR("evergreen startup failed on resume\n");
		rdev->accel_working = false;
		return r;
	}

	return r;

}

int evergreen_suspend(struct radeon_device *rdev)
{
	r600_audio_fini(rdev);
	r700_cp_stop(rdev);
	r600_dma_stop(rdev);
	evergreen_irq_suspend(rdev);
	radeon_wb_disable(rdev);
	evergreen_pcie_gart_disable(rdev);

	return 0;
}

/* Plan is to move initialization in that function and use
 * helper function so that radeon_device_init pretty much
 * do nothing more than calling asic specific function. This
 * should also allow to remove a bunch of callback function
 * like vram_info.
 */
int evergreen_init(struct radeon_device *rdev)
{
	int r;

	/* Read BIOS */
	if (!radeon_get_bios(rdev)) {
		if (ASIC_IS_AVIVO(rdev))
			return -EINVAL;
	}
	/* Must be an ATOMBIOS */
	if (!rdev->is_atom_bios) {
		dev_err(rdev->dev, "Expecting atombios for evergreen GPU\n");
		return -EINVAL;
	}
	r = radeon_atombios_init(rdev);
	if (r)
		return r;
	/* reset the asic, the gfx blocks are often in a bad state
	 * after the driver is unloaded or after a resume
	 */
	if (radeon_asic_reset(rdev))
		dev_warn(rdev->dev, "GPU reset failed !\n");
	/* Post card if necessary */
	if (!radeon_card_posted(rdev)) {
		if (!rdev->bios) {
			dev_err(rdev->dev, "Card not posted and no BIOS - ignoring\n");
			return -EINVAL;
		}
		DRM_INFO("GPU not posted. posting now...\n");
		atom_asic_init(rdev->mode_info.atom_context);
	}
	/* Initialize scratch registers */
	r600_scratch_init(rdev);
	/* Initialize surface registers */
	radeon_surface_init(rdev);
	/* Initialize clocks */
	radeon_get_clock_info(rdev->ddev);
	/* Fence driver */
	r = radeon_fence_driver_init(rdev);
	if (r)
		return r;
	/* initialize AGP */
	if (rdev->flags & RADEON_IS_AGP) {
		r = radeon_agp_init(rdev);
		if (r)
			radeon_agp_disable(rdev);
	}
	/* initialize memory controller */
	r = evergreen_mc_init(rdev);
	if (r)
		return r;
	/* Memory manager */
	r = radeon_bo_init(rdev);
	if (r)
		return r;

	rdev->ring[RADEON_RING_TYPE_GFX_INDEX].ring_obj = NULL;
	r600_ring_init(rdev, &rdev->ring[RADEON_RING_TYPE_GFX_INDEX], 1024 * 1024);

	rdev->ring[R600_RING_TYPE_DMA_INDEX].ring_obj = NULL;
	r600_ring_init(rdev, &rdev->ring[R600_RING_TYPE_DMA_INDEX], 64 * 1024);

	rdev->ih.ring_obj = NULL;
	r600_ih_ring_init(rdev, 64 * 1024);

	r = r600_pcie_gart_init(rdev);
	if (r)
		return r;

	rdev->accel_working = true;
	r = evergreen_startup(rdev);
	if (r) {
		dev_err(rdev->dev, "disabling GPU acceleration\n");
		r700_cp_fini(rdev);
		r600_dma_fini(rdev);
		r600_irq_fini(rdev);
		radeon_wb_fini(rdev);
		radeon_ib_pool_fini(rdev);
		radeon_irq_kms_fini(rdev);
		evergreen_pcie_gart_fini(rdev);
		rdev->accel_working = false;
	}

	/* Don't start up if the MC ucode is missing on BTC parts.
	 * The default clocks and voltages before the MC ucode
	 * is loaded are not suffient for advanced operations.
	 */
	if (ASIC_IS_DCE5(rdev)) {
		if (!rdev->mc_fw && !(rdev->flags & RADEON_IS_IGP)) {
			DRM_ERROR("radeon: MC ucode required for NI+.\n");
			return -EINVAL;
		}
	}

	return 0;
}

void evergreen_fini(struct radeon_device *rdev)
{
	r600_audio_fini(rdev);
	r600_blit_fini(rdev);
	r700_cp_fini(rdev);
	r600_dma_fini(rdev);
	r600_irq_fini(rdev);
	radeon_wb_fini(rdev);
	radeon_ib_pool_fini(rdev);
	radeon_irq_kms_fini(rdev);
	evergreen_pcie_gart_fini(rdev);
	r600_vram_scratch_fini(rdev);
	radeon_gem_fini(rdev);
	radeon_fence_driver_fini(rdev);
	radeon_agp_fini(rdev);
	radeon_bo_fini(rdev);
	radeon_atombios_fini(rdev);
	kfree(rdev->bios);
	rdev->bios = NULL;
}

void evergreen_pcie_gen2_enable(struct radeon_device *rdev)
{
	u32 link_width_cntl, speed_cntl, mask;
	int ret;

	if (radeon_pcie_gen2 == 0)
		return;

	if (rdev->flags & RADEON_IS_IGP)
		return;

	if (!(rdev->flags & RADEON_IS_PCIE))
		return;

	/* x2 cards have a special sequence */
	if (ASIC_IS_X2(rdev))
		return;
	
	ret = drm_pcie_get_speed_cap_mask(rdev->ddev, &mask);
	if (ret != 0)
		return;

	if (!(mask & DRM_PCIE_SPEED_50))
		return;

	speed_cntl = RREG32_PCIE_P(PCIE_LC_SPEED_CNTL);
	if (speed_cntl & LC_CURRENT_DATA_RATE) {
		DRM_INFO("PCIE gen 2 link speeds already enabled\n");
		return;
	}

	DRM_INFO("enabling PCIE gen 2 link speeds, disable with radeon.pcie_gen2=0\n");

	if ((speed_cntl & LC_OTHER_SIDE_EVER_SENT_GEN2) ||
	    (speed_cntl & LC_OTHER_SIDE_SUPPORTS_GEN2)) {

		link_width_cntl = RREG32_PCIE_P(PCIE_LC_LINK_WIDTH_CNTL);
		link_width_cntl &= ~LC_UPCONFIGURE_DIS;
		WREG32_PCIE_P(PCIE_LC_LINK_WIDTH_CNTL, link_width_cntl);

		speed_cntl = RREG32_PCIE_P(PCIE_LC_SPEED_CNTL);
		speed_cntl &= ~LC_TARGET_LINK_SPEED_OVERRIDE_EN;
		WREG32_PCIE_P(PCIE_LC_SPEED_CNTL, speed_cntl);

		speed_cntl = RREG32_PCIE_P(PCIE_LC_SPEED_CNTL);
		speed_cntl |= LC_CLR_FAILED_SPD_CHANGE_CNT;
		WREG32_PCIE_P(PCIE_LC_SPEED_CNTL, speed_cntl);

		speed_cntl = RREG32_PCIE_P(PCIE_LC_SPEED_CNTL);
		speed_cntl &= ~LC_CLR_FAILED_SPD_CHANGE_CNT;
		WREG32_PCIE_P(PCIE_LC_SPEED_CNTL, speed_cntl);

		speed_cntl = RREG32_PCIE_P(PCIE_LC_SPEED_CNTL);
		speed_cntl |= LC_GEN2_EN_STRAP;
		WREG32_PCIE_P(PCIE_LC_SPEED_CNTL, speed_cntl);

	} else {
		link_width_cntl = RREG32_PCIE_P(PCIE_LC_LINK_WIDTH_CNTL);
		/* XXX: only disable it if gen1 bridge vendor == 0x111d or 0x1106 */
		if (1)
			link_width_cntl |= LC_UPCONFIGURE_DIS;
		else
			link_width_cntl &= ~LC_UPCONFIGURE_DIS;
		WREG32_PCIE_P(PCIE_LC_LINK_WIDTH_CNTL, link_width_cntl);
	}
}
