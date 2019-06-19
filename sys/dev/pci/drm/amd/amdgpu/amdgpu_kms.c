/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
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
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#include <drm/drmP.h>
#include "amdgpu.h"
#include <drm/amdgpu_drm.h>
#include "amdgpu_sched.h"
#include "amdgpu_uvd.h"
#include "amdgpu_vce.h"
#include "atom.h"

#include <linux/vga_switcheroo.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include "amdgpu_amdkfd.h"

#include "vga.h"

#if NVGA > 0
#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>

extern int vga_console_attached;
#endif

#ifdef __amd64__
#include "efifb.h"
#include <machine/biosvar.h>
#endif

#if NEFIFB > 0
#include <machine/efifbvar.h>
#endif

int     amdgpu_probe(struct device *, void *, void *);
void    amdgpu_attach(struct device *, struct device *, void *);
int     amdgpu_detach(struct device *, int);
int     amdgpu_activate(struct device *, int);
void    amdgpu_attachhook(struct device *);
int     amdgpu_forcedetach(struct amdgpu_device *);

bool	amdgpu_msi_ok(struct amdgpu_device *);

extern const struct drm_pcidev amdgpu_pciidlist[];
extern struct drm_driver amdgpu_kms_driver;
extern int amdgpu_exp_hw_support;

/*
 * set if the mountroot hook has a fatal error
 * such as not being able to find the firmware
 */
int amdgpu_fatal_error;

struct cfattach amdgpu_ca = {
        sizeof (struct amdgpu_device), amdgpu_probe, amdgpu_attach,
        amdgpu_detach, amdgpu_activate
};

struct cfdriver amdgpu_cd = {
        NULL, "amdgpu", DV_DULL
};

#ifdef __linux__
/**
 * amdgpu_driver_unload_kms - Main unload function for KMS.
 *
 * @dev: drm dev pointer
 *
 * This is the main unload function for KMS (all asics).
 * Returns 0 on success.
 */
void amdgpu_driver_unload_kms(struct drm_device *dev)
{
	struct amdgpu_device *adev = dev->dev_private;

	if (adev == NULL)
		return;

	if (adev->rmmio == NULL)
		goto done_free;

	if (amdgpu_sriov_vf(adev))
		amdgpu_virt_request_full_gpu(adev, false);

	if (amdgpu_device_is_px(dev)) {
		pm_runtime_get_sync(dev->dev);
		pm_runtime_forbid(dev->dev);
	}

	amdgpu_acpi_fini(adev);

	amdgpu_device_fini(adev);

done_free:
	kfree(adev);
	dev->dev_private = NULL;
}

/**
 * amdgpu_driver_load_kms - Main load function for KMS.
 *
 * @dev: drm dev pointer
 * @flags: device flags
 *
 * This is the main load function for KMS (all asics).
 * Returns 0 on success, error on failure.
 */
int amdgpu_driver_load_kms(struct drm_device *dev, unsigned long flags)
{
	struct amdgpu_device *adev;
	int r, acpi_status;

#ifdef CONFIG_DRM_AMDGPU_SI
	if (!amdgpu_si_support) {
		switch (flags & AMD_ASIC_MASK) {
		case CHIP_TAHITI:
		case CHIP_PITCAIRN:
		case CHIP_VERDE:
		case CHIP_OLAND:
		case CHIP_HAINAN:
			dev_info(dev->dev,
				 "SI support provided by radeon.\n");
			dev_info(dev->dev,
				 "Use radeon.si_support=0 amdgpu.si_support=1 to override.\n"
				);
			return -ENODEV;
		}
	}
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
	if (!amdgpu_cik_support) {
		switch (flags & AMD_ASIC_MASK) {
		case CHIP_KAVERI:
		case CHIP_BONAIRE:
		case CHIP_HAWAII:
		case CHIP_KABINI:
		case CHIP_MULLINS:
			dev_info(dev->dev,
				 "CIK support provided by radeon.\n");
			dev_info(dev->dev,
				 "Use radeon.cik_support=0 amdgpu.cik_support=1 to override.\n"
				);
			return -ENODEV;
		}
	}
#endif

	adev = kzalloc(sizeof(struct amdgpu_device), GFP_KERNEL);
	if (adev == NULL) {
		return -ENOMEM;
	}
	dev->dev_private = (void *)adev;

	if ((amdgpu_runtime_pm != 0) &&
	    amdgpu_has_atpx() &&
	    (amdgpu_is_atpx_hybrid() ||
	     amdgpu_has_atpx_dgpu_power_cntl()) &&
	    ((flags & AMD_IS_APU) == 0) &&
	    !pci_is_thunderbolt_attached(dev->pdev))
		flags |= AMD_IS_PX;

	/* amdgpu_device_init should report only fatal error
	 * like memory allocation failure or iomapping failure,
	 * or memory manager initialization failure, it must
	 * properly initialize the GPU MC controller and permit
	 * VRAM allocation
	 */
	r = amdgpu_device_init(adev, dev, dev->pdev, flags);
	if (r) {
		dev_err(&dev->pdev->dev, "Fatal error during GPU init\n");
		goto out;
	}

	/* Call ACPI methods: require modeset init
	 * but failure is not fatal
	 */
	if (!r) {
		acpi_status = amdgpu_acpi_init(adev);
		if (acpi_status)
		dev_dbg(&dev->pdev->dev,
				"Error during ACPI methods call\n");
	}

	if (amdgpu_device_is_px(dev)) {
		dev_pm_set_driver_flags(dev->dev, DPM_FLAG_NEVER_SKIP);
		pm_runtime_use_autosuspend(dev->dev);
		pm_runtime_set_autosuspend_delay(dev->dev, 5000);
		pm_runtime_set_active(dev->dev);
		pm_runtime_allow(dev->dev);
		pm_runtime_mark_last_busy(dev->dev);
		pm_runtime_put_autosuspend(dev->dev);
	}

out:
	if (r) {
		/* balance pm_runtime_get_sync in amdgpu_driver_unload_kms */
		if (adev->rmmio && amdgpu_device_is_px(dev))
			pm_runtime_put_noidle(dev->dev);
		amdgpu_driver_unload_kms(dev);
	}

	return r;
}
#endif /* __linux__ */

static int amdgpu_firmware_info(struct drm_amdgpu_info_firmware *fw_info,
				struct drm_amdgpu_query_fw *query_fw,
				struct amdgpu_device *adev)
{
	switch (query_fw->fw_type) {
	case AMDGPU_INFO_FW_VCE:
		fw_info->ver = adev->vce.fw_version;
		fw_info->feature = adev->vce.fb_version;
		break;
	case AMDGPU_INFO_FW_UVD:
		fw_info->ver = adev->uvd.fw_version;
		fw_info->feature = 0;
		break;
	case AMDGPU_INFO_FW_VCN:
		fw_info->ver = adev->vcn.fw_version;
		fw_info->feature = 0;
		break;
	case AMDGPU_INFO_FW_GMC:
		fw_info->ver = adev->gmc.fw_version;
		fw_info->feature = 0;
		break;
	case AMDGPU_INFO_FW_GFX_ME:
		fw_info->ver = adev->gfx.me_fw_version;
		fw_info->feature = adev->gfx.me_feature_version;
		break;
	case AMDGPU_INFO_FW_GFX_PFP:
		fw_info->ver = adev->gfx.pfp_fw_version;
		fw_info->feature = adev->gfx.pfp_feature_version;
		break;
	case AMDGPU_INFO_FW_GFX_CE:
		fw_info->ver = adev->gfx.ce_fw_version;
		fw_info->feature = adev->gfx.ce_feature_version;
		break;
	case AMDGPU_INFO_FW_GFX_RLC:
		fw_info->ver = adev->gfx.rlc_fw_version;
		fw_info->feature = adev->gfx.rlc_feature_version;
		break;
	case AMDGPU_INFO_FW_GFX_RLC_RESTORE_LIST_CNTL:
		fw_info->ver = adev->gfx.rlc_srlc_fw_version;
		fw_info->feature = adev->gfx.rlc_srlc_feature_version;
		break;
	case AMDGPU_INFO_FW_GFX_RLC_RESTORE_LIST_GPM_MEM:
		fw_info->ver = adev->gfx.rlc_srlg_fw_version;
		fw_info->feature = adev->gfx.rlc_srlg_feature_version;
		break;
	case AMDGPU_INFO_FW_GFX_RLC_RESTORE_LIST_SRM_MEM:
		fw_info->ver = adev->gfx.rlc_srls_fw_version;
		fw_info->feature = adev->gfx.rlc_srls_feature_version;
		break;
	case AMDGPU_INFO_FW_GFX_MEC:
		if (query_fw->index == 0) {
			fw_info->ver = adev->gfx.mec_fw_version;
			fw_info->feature = adev->gfx.mec_feature_version;
		} else if (query_fw->index == 1) {
			fw_info->ver = adev->gfx.mec2_fw_version;
			fw_info->feature = adev->gfx.mec2_feature_version;
		} else
			return -EINVAL;
		break;
	case AMDGPU_INFO_FW_SMC:
		fw_info->ver = adev->pm.fw_version;
		fw_info->feature = 0;
		break;
	case AMDGPU_INFO_FW_SDMA:
		if (query_fw->index >= adev->sdma.num_instances)
			return -EINVAL;
		fw_info->ver = adev->sdma.instance[query_fw->index].fw_version;
		fw_info->feature = adev->sdma.instance[query_fw->index].feature_version;
		break;
	case AMDGPU_INFO_FW_SOS:
		fw_info->ver = adev->psp.sos_fw_version;
		fw_info->feature = adev->psp.sos_feature_version;
		break;
	case AMDGPU_INFO_FW_ASD:
		fw_info->ver = adev->psp.asd_fw_version;
		fw_info->feature = adev->psp.asd_feature_version;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * Userspace get information ioctl
 */
/**
 * amdgpu_info_ioctl - answer a device specific request.
 *
 * @adev: amdgpu device pointer
 * @data: request object
 * @filp: drm filp
 *
 * This function is used to pass device specific parameters to the userspace
 * drivers.  Examples include: pci device id, pipeline parms, tiling params,
 * etc. (all asics).
 * Returns 0 on success, -EINVAL on failure.
 */
static int amdgpu_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct amdgpu_device *adev = dev->dev_private;
	struct drm_amdgpu_info *info = data;
	struct amdgpu_mode_info *minfo = &adev->mode_info;
	void __user *out = (void __user *)(uintptr_t)info->return_pointer;
	uint32_t size = info->return_size;
	struct drm_crtc *crtc;
	uint32_t ui32 = 0;
	uint64_t ui64 = 0;
	int i, j, found;
	int ui32_size = sizeof(ui32);

	if (!info->return_size || !info->return_pointer)
		return -EINVAL;

	switch (info->query) {
	case AMDGPU_INFO_ACCEL_WORKING:
		ui32 = adev->accel_working;
		return copy_to_user(out, &ui32, min(size, 4u)) ? -EFAULT : 0;
	case AMDGPU_INFO_CRTC_FROM_ID:
		for (i = 0, found = 0; i < adev->mode_info.num_crtc; i++) {
			crtc = (struct drm_crtc *)minfo->crtcs[i];
			if (crtc && crtc->base.id == info->mode_crtc.id) {
				struct amdgpu_crtc *amdgpu_crtc = to_amdgpu_crtc(crtc);
				ui32 = amdgpu_crtc->crtc_id;
				found = 1;
				break;
			}
		}
		if (!found) {
			DRM_DEBUG_KMS("unknown crtc id %d\n", info->mode_crtc.id);
			return -EINVAL;
		}
		return copy_to_user(out, &ui32, min(size, 4u)) ? -EFAULT : 0;
	case AMDGPU_INFO_HW_IP_INFO: {
		struct drm_amdgpu_info_hw_ip ip = {};
		enum amd_ip_block_type type;
		uint32_t ring_mask = 0;
		uint32_t ib_start_alignment = 0;
		uint32_t ib_size_alignment = 0;

		if (info->query_hw_ip.ip_instance >= AMDGPU_HW_IP_INSTANCE_MAX_COUNT)
			return -EINVAL;

		switch (info->query_hw_ip.type) {
		case AMDGPU_HW_IP_GFX:
			type = AMD_IP_BLOCK_TYPE_GFX;
			for (i = 0; i < adev->gfx.num_gfx_rings; i++)
				ring_mask |= adev->gfx.gfx_ring[i].ready << i;
			ib_start_alignment = 32;
			ib_size_alignment = 32;
			break;
		case AMDGPU_HW_IP_COMPUTE:
			type = AMD_IP_BLOCK_TYPE_GFX;
			for (i = 0; i < adev->gfx.num_compute_rings; i++)
				ring_mask |= adev->gfx.compute_ring[i].ready << i;
			ib_start_alignment = 32;
			ib_size_alignment = 32;
			break;
		case AMDGPU_HW_IP_DMA:
			type = AMD_IP_BLOCK_TYPE_SDMA;
			for (i = 0; i < adev->sdma.num_instances; i++)
				ring_mask |= adev->sdma.instance[i].ring.ready << i;
			ib_start_alignment = 256;
			ib_size_alignment = 4;
			break;
		case AMDGPU_HW_IP_UVD:
			type = AMD_IP_BLOCK_TYPE_UVD;
			for (i = 0; i < adev->uvd.num_uvd_inst; i++) {
				if (adev->uvd.harvest_config & (1 << i))
					continue;
				ring_mask |= adev->uvd.inst[i].ring.ready;
			}
			ib_start_alignment = 64;
			ib_size_alignment = 64;
			break;
		case AMDGPU_HW_IP_VCE:
			type = AMD_IP_BLOCK_TYPE_VCE;
			for (i = 0; i < adev->vce.num_rings; i++)
				ring_mask |= adev->vce.ring[i].ready << i;
			ib_start_alignment = 4;
			ib_size_alignment = 1;
			break;
		case AMDGPU_HW_IP_UVD_ENC:
			type = AMD_IP_BLOCK_TYPE_UVD;
			for (i = 0; i < adev->uvd.num_uvd_inst; i++) {
				if (adev->uvd.harvest_config & (1 << i))
					continue;
				for (j = 0; j < adev->uvd.num_enc_rings; j++)
					ring_mask |= adev->uvd.inst[i].ring_enc[j].ready << j;
			}
			ib_start_alignment = 64;
			ib_size_alignment = 64;
			break;
		case AMDGPU_HW_IP_VCN_DEC:
			type = AMD_IP_BLOCK_TYPE_VCN;
			ring_mask = adev->vcn.ring_dec.ready;
			ib_start_alignment = 16;
			ib_size_alignment = 16;
			break;
		case AMDGPU_HW_IP_VCN_ENC:
			type = AMD_IP_BLOCK_TYPE_VCN;
			for (i = 0; i < adev->vcn.num_enc_rings; i++)
				ring_mask |= adev->vcn.ring_enc[i].ready << i;
			ib_start_alignment = 64;
			ib_size_alignment = 1;
			break;
		case AMDGPU_HW_IP_VCN_JPEG:
			type = AMD_IP_BLOCK_TYPE_VCN;
			ring_mask = adev->vcn.ring_jpeg.ready;
			ib_start_alignment = 16;
			ib_size_alignment = 16;
			break;
		default:
			return -EINVAL;
		}

		for (i = 0; i < adev->num_ip_blocks; i++) {
			if (adev->ip_blocks[i].version->type == type &&
			    adev->ip_blocks[i].status.valid) {
				ip.hw_ip_version_major = adev->ip_blocks[i].version->major;
				ip.hw_ip_version_minor = adev->ip_blocks[i].version->minor;
				ip.capabilities_flags = 0;
				ip.available_rings = ring_mask;
				ip.ib_start_alignment = ib_start_alignment;
				ip.ib_size_alignment = ib_size_alignment;
				break;
			}
		}
		return copy_to_user(out, &ip,
				    min((size_t)size, sizeof(ip))) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_HW_IP_COUNT: {
		enum amd_ip_block_type type;
		uint32_t count = 0;

		switch (info->query_hw_ip.type) {
		case AMDGPU_HW_IP_GFX:
			type = AMD_IP_BLOCK_TYPE_GFX;
			break;
		case AMDGPU_HW_IP_COMPUTE:
			type = AMD_IP_BLOCK_TYPE_GFX;
			break;
		case AMDGPU_HW_IP_DMA:
			type = AMD_IP_BLOCK_TYPE_SDMA;
			break;
		case AMDGPU_HW_IP_UVD:
			type = AMD_IP_BLOCK_TYPE_UVD;
			break;
		case AMDGPU_HW_IP_VCE:
			type = AMD_IP_BLOCK_TYPE_VCE;
			break;
		case AMDGPU_HW_IP_UVD_ENC:
			type = AMD_IP_BLOCK_TYPE_UVD;
			break;
		case AMDGPU_HW_IP_VCN_DEC:
		case AMDGPU_HW_IP_VCN_ENC:
		case AMDGPU_HW_IP_VCN_JPEG:
			type = AMD_IP_BLOCK_TYPE_VCN;
			break;
		default:
			return -EINVAL;
		}

		for (i = 0; i < adev->num_ip_blocks; i++)
			if (adev->ip_blocks[i].version->type == type &&
			    adev->ip_blocks[i].status.valid &&
			    count < AMDGPU_HW_IP_INSTANCE_MAX_COUNT)
				count++;

		return copy_to_user(out, &count, min(size, 4u)) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_TIMESTAMP:
		ui64 = amdgpu_gfx_get_gpu_clock_counter(adev);
		return copy_to_user(out, &ui64, min(size, 8u)) ? -EFAULT : 0;
	case AMDGPU_INFO_FW_VERSION: {
		struct drm_amdgpu_info_firmware fw_info;
		int ret;

		/* We only support one instance of each IP block right now. */
		if (info->query_fw.ip_instance != 0)
			return -EINVAL;

		ret = amdgpu_firmware_info(&fw_info, &info->query_fw, adev);
		if (ret)
			return ret;

		return copy_to_user(out, &fw_info,
				    min((size_t)size, sizeof(fw_info))) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_NUM_BYTES_MOVED:
		ui64 = atomic64_read(&adev->num_bytes_moved);
		return copy_to_user(out, &ui64, min(size, 8u)) ? -EFAULT : 0;
	case AMDGPU_INFO_NUM_EVICTIONS:
		ui64 = atomic64_read(&adev->num_evictions);
		return copy_to_user(out, &ui64, min(size, 8u)) ? -EFAULT : 0;
	case AMDGPU_INFO_NUM_VRAM_CPU_PAGE_FAULTS:
		ui64 = atomic64_read(&adev->num_vram_cpu_page_faults);
		return copy_to_user(out, &ui64, min(size, 8u)) ? -EFAULT : 0;
	case AMDGPU_INFO_VRAM_USAGE:
		ui64 = amdgpu_vram_mgr_usage(&adev->mman.bdev.man[TTM_PL_VRAM]);
		return copy_to_user(out, &ui64, min(size, 8u)) ? -EFAULT : 0;
	case AMDGPU_INFO_VIS_VRAM_USAGE:
		ui64 = amdgpu_vram_mgr_vis_usage(&adev->mman.bdev.man[TTM_PL_VRAM]);
		return copy_to_user(out, &ui64, min(size, 8u)) ? -EFAULT : 0;
	case AMDGPU_INFO_GTT_USAGE:
		ui64 = amdgpu_gtt_mgr_usage(&adev->mman.bdev.man[TTM_PL_TT]);
		return copy_to_user(out, &ui64, min(size, 8u)) ? -EFAULT : 0;
	case AMDGPU_INFO_GDS_CONFIG: {
		struct drm_amdgpu_info_gds gds_info;

		memset(&gds_info, 0, sizeof(gds_info));
		gds_info.gds_gfx_partition_size = adev->gds.mem.gfx_partition_size >> AMDGPU_GDS_SHIFT;
		gds_info.compute_partition_size = adev->gds.mem.cs_partition_size >> AMDGPU_GDS_SHIFT;
		gds_info.gds_total_size = adev->gds.mem.total_size >> AMDGPU_GDS_SHIFT;
		gds_info.gws_per_gfx_partition = adev->gds.gws.gfx_partition_size >> AMDGPU_GWS_SHIFT;
		gds_info.gws_per_compute_partition = adev->gds.gws.cs_partition_size >> AMDGPU_GWS_SHIFT;
		gds_info.oa_per_gfx_partition = adev->gds.oa.gfx_partition_size >> AMDGPU_OA_SHIFT;
		gds_info.oa_per_compute_partition = adev->gds.oa.cs_partition_size >> AMDGPU_OA_SHIFT;
		return copy_to_user(out, &gds_info,
				    min((size_t)size, sizeof(gds_info))) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_VRAM_GTT: {
		struct drm_amdgpu_info_vram_gtt vram_gtt;

		vram_gtt.vram_size = adev->gmc.real_vram_size -
			atomic64_read(&adev->vram_pin_size);
		vram_gtt.vram_cpu_accessible_size = adev->gmc.visible_vram_size -
			atomic64_read(&adev->visible_pin_size);
		vram_gtt.gtt_size = adev->mman.bdev.man[TTM_PL_TT].size;
		vram_gtt.gtt_size *= PAGE_SIZE;
		vram_gtt.gtt_size -= atomic64_read(&adev->gart_pin_size);
		return copy_to_user(out, &vram_gtt,
				    min((size_t)size, sizeof(vram_gtt))) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_MEMORY: {
		struct drm_amdgpu_memory_info mem;

		memset(&mem, 0, sizeof(mem));
		mem.vram.total_heap_size = adev->gmc.real_vram_size;
		mem.vram.usable_heap_size = adev->gmc.real_vram_size -
			atomic64_read(&adev->vram_pin_size);
		mem.vram.heap_usage =
			amdgpu_vram_mgr_usage(&adev->mman.bdev.man[TTM_PL_VRAM]);
		mem.vram.max_allocation = mem.vram.usable_heap_size * 3 / 4;

		mem.cpu_accessible_vram.total_heap_size =
			adev->gmc.visible_vram_size;
		mem.cpu_accessible_vram.usable_heap_size = adev->gmc.visible_vram_size -
			atomic64_read(&adev->visible_pin_size);
		mem.cpu_accessible_vram.heap_usage =
			amdgpu_vram_mgr_vis_usage(&adev->mman.bdev.man[TTM_PL_VRAM]);
		mem.cpu_accessible_vram.max_allocation =
			mem.cpu_accessible_vram.usable_heap_size * 3 / 4;

		mem.gtt.total_heap_size = adev->mman.bdev.man[TTM_PL_TT].size;
		mem.gtt.total_heap_size *= PAGE_SIZE;
		mem.gtt.usable_heap_size = mem.gtt.total_heap_size -
			atomic64_read(&adev->gart_pin_size);
		mem.gtt.heap_usage =
			amdgpu_gtt_mgr_usage(&adev->mman.bdev.man[TTM_PL_TT]);
		mem.gtt.max_allocation = mem.gtt.usable_heap_size * 3 / 4;

		return copy_to_user(out, &mem,
				    min((size_t)size, sizeof(mem)))
				    ? -EFAULT : 0;
	}
	case AMDGPU_INFO_READ_MMR_REG: {
		unsigned n, alloc_size;
		uint32_t *regs;
		unsigned se_num = (info->read_mmr_reg.instance >>
				   AMDGPU_INFO_MMR_SE_INDEX_SHIFT) &
				  AMDGPU_INFO_MMR_SE_INDEX_MASK;
		unsigned sh_num = (info->read_mmr_reg.instance >>
				   AMDGPU_INFO_MMR_SH_INDEX_SHIFT) &
				  AMDGPU_INFO_MMR_SH_INDEX_MASK;

		/* set full masks if the userspace set all bits
		 * in the bitfields */
		if (se_num == AMDGPU_INFO_MMR_SE_INDEX_MASK)
			se_num = 0xffffffff;
		if (sh_num == AMDGPU_INFO_MMR_SH_INDEX_MASK)
			sh_num = 0xffffffff;

		regs = kmalloc_array(info->read_mmr_reg.count, sizeof(*regs), GFP_KERNEL);
		if (!regs)
			return -ENOMEM;
		alloc_size = info->read_mmr_reg.count * sizeof(*regs);

		for (i = 0; i < info->read_mmr_reg.count; i++)
			if (amdgpu_asic_read_register(adev, se_num, sh_num,
						      info->read_mmr_reg.dword_offset + i,
						      &regs[i])) {
				DRM_DEBUG_KMS("unallowed offset %#x\n",
					      info->read_mmr_reg.dword_offset + i);
				kfree(regs);
				return -EFAULT;
			}
		n = copy_to_user(out, regs, min(size, alloc_size));
		kfree(regs);
		return n ? -EFAULT : 0;
	}
	case AMDGPU_INFO_DEV_INFO: {
		struct drm_amdgpu_info_device dev_info = {};
		uint64_t vm_size;

		dev_info.device_id = dev->pdev->device;
		dev_info.chip_rev = adev->rev_id;
		dev_info.external_rev = adev->external_rev_id;
		dev_info.pci_rev = dev->pdev->revision;
		dev_info.family = adev->family;
		dev_info.num_shader_engines = adev->gfx.config.max_shader_engines;
		dev_info.num_shader_arrays_per_engine = adev->gfx.config.max_sh_per_se;
		/* return all clocks in KHz */
		dev_info.gpu_counter_freq = amdgpu_asic_get_xclk(adev) * 10;
		if (adev->pm.dpm_enabled) {
			dev_info.max_engine_clock = amdgpu_dpm_get_sclk(adev, false) * 10;
			dev_info.max_memory_clock = amdgpu_dpm_get_mclk(adev, false) * 10;
		} else {
			dev_info.max_engine_clock = adev->clock.default_sclk * 10;
			dev_info.max_memory_clock = adev->clock.default_mclk * 10;
		}
		dev_info.enabled_rb_pipes_mask = adev->gfx.config.backend_enable_mask;
		dev_info.num_rb_pipes = adev->gfx.config.max_backends_per_se *
			adev->gfx.config.max_shader_engines;
		dev_info.num_hw_gfx_contexts = adev->gfx.config.max_hw_contexts;
		dev_info._pad = 0;
		dev_info.ids_flags = 0;
		if (adev->flags & AMD_IS_APU)
			dev_info.ids_flags |= AMDGPU_IDS_FLAGS_FUSION;
		if (amdgpu_sriov_vf(adev))
			dev_info.ids_flags |= AMDGPU_IDS_FLAGS_PREEMPTION;

		vm_size = adev->vm_manager.max_pfn * AMDGPU_GPU_PAGE_SIZE;
		vm_size -= AMDGPU_VA_RESERVED_SIZE;

		/* Older VCE FW versions are buggy and can handle only 40bits */
		if (adev->vce.fw_version < AMDGPU_VCE_FW_53_45)
			vm_size = min(vm_size, 1ULL << 40);

		dev_info.virtual_address_offset = AMDGPU_VA_RESERVED_SIZE;
		dev_info.virtual_address_max =
			min(vm_size, AMDGPU_VA_HOLE_START);

		if (vm_size > AMDGPU_VA_HOLE_START) {
			dev_info.high_va_offset = AMDGPU_VA_HOLE_END;
			dev_info.high_va_max = AMDGPU_VA_HOLE_END | vm_size;
		}
		dev_info.virtual_address_alignment = max((int)PAGE_SIZE, AMDGPU_GPU_PAGE_SIZE);
		dev_info.pte_fragment_size = (1 << adev->vm_manager.fragment_size) * AMDGPU_GPU_PAGE_SIZE;
		dev_info.gart_page_size = AMDGPU_GPU_PAGE_SIZE;
		dev_info.cu_active_number = adev->gfx.cu_info.number;
		dev_info.cu_ao_mask = adev->gfx.cu_info.ao_cu_mask;
		dev_info.ce_ram_size = adev->gfx.ce_ram_size;
		memcpy(&dev_info.cu_ao_bitmap[0], &adev->gfx.cu_info.ao_cu_bitmap[0],
		       sizeof(adev->gfx.cu_info.ao_cu_bitmap));
		memcpy(&dev_info.cu_bitmap[0], &adev->gfx.cu_info.bitmap[0],
		       sizeof(adev->gfx.cu_info.bitmap));
		dev_info.vram_type = adev->gmc.vram_type;
		dev_info.vram_bit_width = adev->gmc.vram_width;
		dev_info.vce_harvest_config = adev->vce.harvest_config;
		dev_info.gc_double_offchip_lds_buf =
			adev->gfx.config.double_offchip_lds_buf;

		if (amdgpu_ngg) {
			dev_info.prim_buf_gpu_addr = adev->gfx.ngg.buf[NGG_PRIM].gpu_addr;
			dev_info.prim_buf_size = adev->gfx.ngg.buf[NGG_PRIM].size;
			dev_info.pos_buf_gpu_addr = adev->gfx.ngg.buf[NGG_POS].gpu_addr;
			dev_info.pos_buf_size = adev->gfx.ngg.buf[NGG_POS].size;
			dev_info.cntl_sb_buf_gpu_addr = adev->gfx.ngg.buf[NGG_CNTL].gpu_addr;
			dev_info.cntl_sb_buf_size = adev->gfx.ngg.buf[NGG_CNTL].size;
			dev_info.param_buf_gpu_addr = adev->gfx.ngg.buf[NGG_PARAM].gpu_addr;
			dev_info.param_buf_size = adev->gfx.ngg.buf[NGG_PARAM].size;
		}
		dev_info.wave_front_size = adev->gfx.cu_info.wave_front_size;
		dev_info.num_shader_visible_vgprs = adev->gfx.config.max_gprs;
		dev_info.num_cu_per_sh = adev->gfx.config.max_cu_per_sh;
		dev_info.num_tcc_blocks = adev->gfx.config.max_texture_channel_caches;
		dev_info.gs_vgt_table_depth = adev->gfx.config.gs_vgt_table_depth;
		dev_info.gs_prim_buffer_depth = adev->gfx.config.gs_prim_buffer_depth;
		dev_info.max_gs_waves_per_vgt = adev->gfx.config.max_gs_threads;

		return copy_to_user(out, &dev_info,
				    min((size_t)size, sizeof(dev_info))) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_VCE_CLOCK_TABLE: {
		unsigned i;
		struct drm_amdgpu_info_vce_clock_table vce_clk_table = {};
		struct amd_vce_state *vce_state;

		for (i = 0; i < AMDGPU_VCE_CLOCK_TABLE_ENTRIES; i++) {
			vce_state = amdgpu_dpm_get_vce_clock_state(adev, i);
			if (vce_state) {
				vce_clk_table.entries[i].sclk = vce_state->sclk;
				vce_clk_table.entries[i].mclk = vce_state->mclk;
				vce_clk_table.entries[i].eclk = vce_state->evclk;
				vce_clk_table.num_valid_entries++;
			}
		}

		return copy_to_user(out, &vce_clk_table,
				    min((size_t)size, sizeof(vce_clk_table))) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_VBIOS: {
		uint32_t bios_size = adev->bios_size;

		switch (info->vbios_info.type) {
		case AMDGPU_INFO_VBIOS_SIZE:
			return copy_to_user(out, &bios_size,
					min((size_t)size, sizeof(bios_size)))
					? -EFAULT : 0;
		case AMDGPU_INFO_VBIOS_IMAGE: {
			uint8_t *bios;
			uint32_t bios_offset = info->vbios_info.offset;

			if (bios_offset >= bios_size)
				return -EINVAL;

			bios = adev->bios + bios_offset;
			return copy_to_user(out, bios,
					    min((size_t)size, (size_t)(bios_size - bios_offset)))
					? -EFAULT : 0;
		}
		default:
			DRM_DEBUG_KMS("Invalid request %d\n",
					info->vbios_info.type);
			return -EINVAL;
		}
	}
	case AMDGPU_INFO_NUM_HANDLES: {
		struct drm_amdgpu_info_num_handles handle;

		switch (info->query_hw_ip.type) {
		case AMDGPU_HW_IP_UVD:
			/* Starting Polaris, we support unlimited UVD handles */
			if (adev->asic_type < CHIP_POLARIS10) {
				handle.uvd_max_handles = adev->uvd.max_handles;
				handle.uvd_used_handles = amdgpu_uvd_used_handles(adev);

				return copy_to_user(out, &handle,
					min((size_t)size, sizeof(handle))) ? -EFAULT : 0;
			} else {
				return -ENODATA;
			}

			break;
		default:
			return -EINVAL;
		}
	}
	case AMDGPU_INFO_SENSOR: {
		if (!adev->pm.dpm_enabled)
			return -ENOENT;

		switch (info->sensor_info.type) {
		case AMDGPU_INFO_SENSOR_GFX_SCLK:
			/* get sclk in Mhz */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_GFX_SCLK,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			ui32 /= 100;
			break;
		case AMDGPU_INFO_SENSOR_GFX_MCLK:
			/* get mclk in Mhz */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_GFX_MCLK,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			ui32 /= 100;
			break;
		case AMDGPU_INFO_SENSOR_GPU_TEMP:
			/* get temperature in millidegrees C */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_GPU_TEMP,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			break;
		case AMDGPU_INFO_SENSOR_GPU_LOAD:
			/* get GPU load */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_GPU_LOAD,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			break;
		case AMDGPU_INFO_SENSOR_GPU_AVG_POWER:
			/* get average GPU power */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_GPU_POWER,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			ui32 >>= 8;
			break;
		case AMDGPU_INFO_SENSOR_VDDNB:
			/* get VDDNB in millivolts */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_VDDNB,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			break;
		case AMDGPU_INFO_SENSOR_VDDGFX:
			/* get VDDGFX in millivolts */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_VDDGFX,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			break;
		case AMDGPU_INFO_SENSOR_STABLE_PSTATE_GFX_SCLK:
			/* get stable pstate sclk in Mhz */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_STABLE_PSTATE_SCLK,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			ui32 /= 100;
			break;
		case AMDGPU_INFO_SENSOR_STABLE_PSTATE_GFX_MCLK:
			/* get stable pstate mclk in Mhz */
			if (amdgpu_dpm_read_sensor(adev,
						   AMDGPU_PP_SENSOR_STABLE_PSTATE_MCLK,
						   (void *)&ui32, &ui32_size)) {
				return -EINVAL;
			}
			ui32 /= 100;
			break;
		default:
			DRM_DEBUG_KMS("Invalid request %d\n",
				      info->sensor_info.type);
			return -EINVAL;
		}
		return copy_to_user(out, &ui32, min(size, 4u)) ? -EFAULT : 0;
	}
	case AMDGPU_INFO_VRAM_LOST_COUNTER:
		ui32 = atomic_read(&adev->vram_lost_counter);
		return copy_to_user(out, &ui32, min(size, 4u)) ? -EFAULT : 0;
	default:
		DRM_DEBUG_KMS("Invalid request %d\n", info->query);
		return -EINVAL;
	}
	return 0;
}


/*
 * Outdated mess for old drm with Xorg being in charge (void function now).
 */
/**
 * amdgpu_driver_lastclose_kms - drm callback for last close
 *
 * @dev: drm dev pointer
 *
 * Switch vga_switcheroo state after last close (all asics).
 */
void amdgpu_driver_lastclose_kms(struct drm_device *dev)
{
	drm_fb_helper_lastclose(dev);
	vga_switcheroo_process_delayed_switch();
}

/**
 * amdgpu_driver_open_kms - drm callback for open
 *
 * @dev: drm dev pointer
 * @file_priv: drm file
 *
 * On device open, init vm on cayman+ (all asics).
 * Returns 0 on success, error on failure.
 */
int amdgpu_driver_open_kms(struct drm_device *dev, struct drm_file *file_priv)
{
	struct amdgpu_device *adev = dev->dev_private;
	struct amdgpu_fpriv *fpriv;
	int r, pasid;

	/* Ensure IB tests are run on ring */
	flush_delayed_work(&adev->late_init_work);

	file_priv->driver_priv = NULL;

	r = pm_runtime_get_sync(dev->dev);
	if (r < 0)
		return r;

	fpriv = kzalloc(sizeof(*fpriv), GFP_KERNEL);
	if (unlikely(!fpriv)) {
		r = -ENOMEM;
		goto out_suspend;
	}

	pasid = amdgpu_pasid_alloc(16);
	if (pasid < 0) {
		dev_warn(adev->dev, "No more PASIDs available!");
		pasid = 0;
	}
	r = amdgpu_vm_init(adev, &fpriv->vm, AMDGPU_VM_CONTEXT_GFX, pasid);
	if (r)
		goto error_pasid;

	fpriv->prt_va = amdgpu_vm_bo_add(adev, &fpriv->vm, NULL);
	if (!fpriv->prt_va) {
		r = -ENOMEM;
		goto error_vm;
	}

	if (amdgpu_sriov_vf(adev)) {
		r = amdgpu_map_static_csa(adev, &fpriv->vm, &fpriv->csa_va);
		if (r)
			goto error_vm;
	}

	rw_init(&fpriv->bo_list_lock, "agbo");
	idr_init(&fpriv->bo_list_handles);

	amdgpu_ctx_mgr_init(&fpriv->ctx_mgr);

	file_priv->driver_priv = fpriv;
	goto out_suspend;

error_vm:
	amdgpu_vm_fini(adev, &fpriv->vm);

error_pasid:
	if (pasid)
		amdgpu_pasid_free(pasid);

	kfree(fpriv);

out_suspend:
	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);

	return r;
}

/**
 * amdgpu_driver_postclose_kms - drm callback for post close
 *
 * @dev: drm dev pointer
 * @file_priv: drm file
 *
 * On device post close, tear down vm on cayman+ (all asics).
 */
void amdgpu_driver_postclose_kms(struct drm_device *dev,
				 struct drm_file *file_priv)
{
	struct amdgpu_device *adev = dev->dev_private;
	struct amdgpu_fpriv *fpriv = file_priv->driver_priv;
	struct amdgpu_bo_list *list;
	struct amdgpu_bo *pd;
	unsigned int pasid;
	int handle;

	if (!fpriv)
		return;

	pm_runtime_get_sync(dev->dev);

	if (adev->asic_type != CHIP_RAVEN) {
		amdgpu_uvd_free_handles(adev, file_priv);
		amdgpu_vce_free_handles(adev, file_priv);
	}

	amdgpu_vm_bo_rmv(adev, fpriv->prt_va);

	if (amdgpu_sriov_vf(adev)) {
		/* TODO: how to handle reserve failure */
		BUG_ON(amdgpu_bo_reserve(adev->virt.csa_obj, true));
		amdgpu_vm_bo_rmv(adev, fpriv->csa_va);
		fpriv->csa_va = NULL;
		amdgpu_bo_unreserve(adev->virt.csa_obj);
	}

	pasid = fpriv->vm.pasid;
	pd = amdgpu_bo_ref(fpriv->vm.root.base.bo);

	amdgpu_vm_fini(adev, &fpriv->vm);
	amdgpu_ctx_mgr_fini(&fpriv->ctx_mgr);

	if (pasid)
		amdgpu_pasid_free_delayed(pd->tbo.resv, pasid);
	amdgpu_bo_unref(&pd);

	idr_for_each_entry(&fpriv->bo_list_handles, list, handle)
		amdgpu_bo_list_put(list);

	idr_destroy(&fpriv->bo_list_handles);
	mutex_destroy(&fpriv->bo_list_lock);

	kfree(fpriv);
	file_priv->driver_priv = NULL;

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);
}

/*
 * VBlank related functions.
 */
/**
 * amdgpu_get_vblank_counter_kms - get frame count
 *
 * @dev: drm dev pointer
 * @pipe: crtc to get the frame count from
 *
 * Gets the frame count on the requested crtc (all asics).
 * Returns frame count on success, -EINVAL on failure.
 */
u32 amdgpu_get_vblank_counter_kms(struct drm_device *dev, unsigned int pipe)
{
	struct amdgpu_device *adev = dev->dev_private;
	int vpos, hpos, stat;
	u32 count;

	if (pipe >= adev->mode_info.num_crtc) {
		DRM_ERROR("Invalid crtc %u\n", pipe);
		return -EINVAL;
	}

	/* The hw increments its frame counter at start of vsync, not at start
	 * of vblank, as is required by DRM core vblank counter handling.
	 * Cook the hw count here to make it appear to the caller as if it
	 * incremented at start of vblank. We measure distance to start of
	 * vblank in vpos. vpos therefore will be >= 0 between start of vblank
	 * and start of vsync, so vpos >= 0 means to bump the hw frame counter
	 * result by 1 to give the proper appearance to caller.
	 */
	if (adev->mode_info.crtcs[pipe]) {
		/* Repeat readout if needed to provide stable result if
		 * we cross start of vsync during the queries.
		 */
		do {
			count = amdgpu_display_vblank_get_counter(adev, pipe);
			/* Ask amdgpu_display_get_crtc_scanoutpos to return
			 * vpos as distance to start of vblank, instead of
			 * regular vertical scanout pos.
			 */
			stat = amdgpu_display_get_crtc_scanoutpos(
				dev, pipe, GET_DISTANCE_TO_VBLANKSTART,
				&vpos, &hpos, NULL, NULL,
				&adev->mode_info.crtcs[pipe]->base.hwmode);
		} while (count != amdgpu_display_vblank_get_counter(adev, pipe));

		if (((stat & (DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_ACCURATE)) !=
		    (DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_ACCURATE))) {
			DRM_DEBUG_VBL("Query failed! stat %d\n", stat);
		} else {
			DRM_DEBUG_VBL("crtc %d: dist from vblank start %d\n",
				      pipe, vpos);

			/* Bump counter if we are at >= leading edge of vblank,
			 * but before vsync where vpos would turn negative and
			 * the hw counter really increments.
			 */
			if (vpos >= 0)
				count++;
		}
	} else {
		/* Fallback to use value as is. */
		count = amdgpu_display_vblank_get_counter(adev, pipe);
		DRM_DEBUG_VBL("NULL mode info! Returned count may be wrong.\n");
	}

	return count;
}

/**
 * amdgpu_enable_vblank_kms - enable vblank interrupt
 *
 * @dev: drm dev pointer
 * @pipe: crtc to enable vblank interrupt for
 *
 * Enable the interrupt on the requested crtc (all asics).
 * Returns 0 on success, -EINVAL on failure.
 */
int amdgpu_enable_vblank_kms(struct drm_device *dev, unsigned int pipe)
{
	struct amdgpu_device *adev = dev->dev_private;
	int idx = amdgpu_display_crtc_idx_to_irq_type(adev, pipe);

	return amdgpu_irq_get(adev, &adev->crtc_irq, idx);
}

/**
 * amdgpu_disable_vblank_kms - disable vblank interrupt
 *
 * @dev: drm dev pointer
 * @pipe: crtc to disable vblank interrupt for
 *
 * Disable the interrupt on the requested crtc (all asics).
 */
void amdgpu_disable_vblank_kms(struct drm_device *dev, unsigned int pipe)
{
	struct amdgpu_device *adev = dev->dev_private;
	int idx = amdgpu_display_crtc_idx_to_irq_type(adev, pipe);

	amdgpu_irq_put(adev, &adev->crtc_irq, idx);
}

const struct drm_ioctl_desc amdgpu_ioctls_kms[] = {
	DRM_IOCTL_DEF_DRV(AMDGPU_GEM_CREATE, amdgpu_gem_create_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_CTX, amdgpu_ctx_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_VM, amdgpu_vm_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_SCHED, amdgpu_sched_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF_DRV(AMDGPU_BO_LIST, amdgpu_bo_list_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_FENCE_TO_HANDLE, amdgpu_cs_fence_to_handle_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	/* KMS */
	DRM_IOCTL_DEF_DRV(AMDGPU_GEM_MMAP, amdgpu_gem_mmap_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_GEM_WAIT_IDLE, amdgpu_gem_wait_idle_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_CS, amdgpu_cs_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_INFO, amdgpu_info_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_WAIT_CS, amdgpu_cs_wait_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_WAIT_FENCES, amdgpu_cs_wait_fences_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_GEM_METADATA, amdgpu_gem_metadata_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_GEM_VA, amdgpu_gem_va_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_GEM_OP, amdgpu_gem_op_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(AMDGPU_GEM_USERPTR, amdgpu_gem_userptr_ioctl, DRM_AUTH|DRM_RENDER_ALLOW)
};
const int amdgpu_max_kms_ioctl = ARRAY_SIZE(amdgpu_ioctls_kms);

/*
 * Debugfs info
 */
#if defined(CONFIG_DEBUG_FS)

static int amdgpu_debugfs_firmware_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = dev->dev_private;
	struct drm_amdgpu_info_firmware fw_info;
	struct drm_amdgpu_query_fw query_fw;
	struct atom_context *ctx = adev->mode_info.atom_context;
	int ret, i;

	/* VCE */
	query_fw.fw_type = AMDGPU_INFO_FW_VCE;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "VCE feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* UVD */
	query_fw.fw_type = AMDGPU_INFO_FW_UVD;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "UVD feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* GMC */
	query_fw.fw_type = AMDGPU_INFO_FW_GMC;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "MC feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* ME */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_ME;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "ME feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* PFP */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_PFP;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "PFP feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* CE */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_CE;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "CE feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* RLC */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_RLC;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "RLC feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* RLC SAVE RESTORE LIST CNTL */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_RLC_RESTORE_LIST_CNTL;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "RLC SRLC feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* RLC SAVE RESTORE LIST GPM MEM */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_RLC_RESTORE_LIST_GPM_MEM;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "RLC SRLG feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* RLC SAVE RESTORE LIST SRM MEM */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_RLC_RESTORE_LIST_SRM_MEM;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "RLC SRLS feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* MEC */
	query_fw.fw_type = AMDGPU_INFO_FW_GFX_MEC;
	query_fw.index = 0;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "MEC feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* MEC2 */
	if (adev->asic_type == CHIP_KAVERI ||
	    (adev->asic_type > CHIP_TOPAZ && adev->asic_type != CHIP_STONEY)) {
		query_fw.index = 1;
		ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
		if (ret)
			return ret;
		seq_printf(m, "MEC2 feature version: %u, firmware version: 0x%08x\n",
			   fw_info.feature, fw_info.ver);
	}

	/* PSP SOS */
	query_fw.fw_type = AMDGPU_INFO_FW_SOS;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "SOS feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);


	/* PSP ASD */
	query_fw.fw_type = AMDGPU_INFO_FW_ASD;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "ASD feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* SMC */
	query_fw.fw_type = AMDGPU_INFO_FW_SMC;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "SMC feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);

	/* SDMA */
	query_fw.fw_type = AMDGPU_INFO_FW_SDMA;
	for (i = 0; i < adev->sdma.num_instances; i++) {
		query_fw.index = i;
		ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
		if (ret)
			return ret;
		seq_printf(m, "SDMA%d feature version: %u, firmware version: 0x%08x\n",
			   i, fw_info.feature, fw_info.ver);
	}

	/* VCN */
	query_fw.fw_type = AMDGPU_INFO_FW_VCN;
	ret = amdgpu_firmware_info(&fw_info, &query_fw, adev);
	if (ret)
		return ret;
	seq_printf(m, "VCN feature version: %u, firmware version: 0x%08x\n",
		   fw_info.feature, fw_info.ver);


	seq_printf(m, "VBIOS version: %s\n", ctx->vbios_version);

	return 0;
}

static const struct drm_info_list amdgpu_firmware_info_list[] = {
	{"amdgpu_firmware_info", amdgpu_debugfs_firmware_info, 0, NULL},
};
#endif

int amdgpu_debugfs_firmware_init(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	return amdgpu_debugfs_add_files(adev, amdgpu_firmware_info_list,
					ARRAY_SIZE(amdgpu_firmware_info_list));
#else
	return 0;
#endif
}

int
amdgpu_probe(struct device *parent, void *match, void *aux)
{
	if (amdgpu_fatal_error)
		return 0;
	if (drm_pciprobe(aux, amdgpu_pciidlist))
		return 20;
	return 0;
}

/*
 * some functions are only called once on init regardless of how many times
 * amdgpu attaches in linux this is handled via module_init()/module_exit()
 */
int amdgpu_refcnt;

int __init drm_sched_fence_slab_init(void);
void __exit drm_sched_fence_slab_fini(void);

void
amdgpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct amdgpu_device	*adev = (struct amdgpu_device *)self;
	struct drm_device	*dev;
	struct pci_attach_args	*pa = aux;
	const struct drm_pcidev	*id_entry;
	pcireg_t		 type;
	int			 i;
	uint8_t			 rmmio_bar;
	paddr_t			 fb_aper;
	pcireg_t		 addr, mask;
	int			 s;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), amdgpu_pciidlist);
	adev->flags = id_entry->driver_data;
	adev->family = adev->flags & AMD_ASIC_MASK;
	adev->pc = pa->pa_pc;
	adev->pa_tag = pa->pa_tag;
	adev->iot = pa->pa_iot;
	adev->memt = pa->pa_memt;
	adev->dmat = pa->pa_dmat;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA &&
	    (pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    == (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE)) {
		adev->primary = 1;
#if NVGA > 0
		adev->console = vga_is_console(pa->pa_iot, -1);
		vga_console_attached = 1;
#endif
	}
#if NEFIFB > 0
	if (efifb_is_primary(pa)) {
		adev->primary = 1;
		adev->console = efifb_is_console(pa);
		efifb_detach();
	}
#endif

#define AMDGPU_PCI_MEM		0x10

	type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, AMDGPU_PCI_MEM);
	if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
	    pci_mapreg_info(pa->pa_pc, pa->pa_tag, AMDGPU_PCI_MEM,
	    type, &adev->fb_aper_offset, &adev->fb_aper_size, NULL)) {
		printf(": can't get frambuffer info\n");
		return;
	}

	if (adev->fb_aper_offset == 0) {
		bus_size_t start, end, pci_mem_end;
		bus_addr_t base;

		start = max(PCI_MEM_START, pa->pa_memex->ex_start);
		if (PCI_MAPREG_MEM_TYPE(type) == PCI_MAPREG_MEM_TYPE_64BIT)
			pci_mem_end = PCI_MEM64_END;
		else
			pci_mem_end = PCI_MEM_END;
		end = min(pci_mem_end, pa->pa_memex->ex_end);
		if (pa->pa_memex == NULL ||
		    extent_alloc_subregion(pa->pa_memex, start, end,
		    adev->fb_aper_size, adev->fb_aper_size, 0, 0, 0, &base)) {
			printf(": can't reserve framebuffer space\n");
			return;
		}
		pci_conf_write(pa->pa_pc, pa->pa_tag, AMDGPU_PCI_MEM, base);
		if (PCI_MAPREG_MEM_TYPE(type) == PCI_MAPREG_MEM_TYPE_64BIT)
			pci_conf_write(pa->pa_pc, pa->pa_tag,
			    AMDGPU_PCI_MEM + 4, (uint64_t)base >> 32);
		adev->fb_aper_offset = base;
	}

	for (i = PCI_MAPREG_START; i < PCI_MAPREG_END ;) {
		type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, i);
		if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_IO) {
			if (type & PCI_MAPREG_MEM_TYPE_64BIT)
				i += 8;
			else
				i += 4;
			continue;
		}
		if (pci_mapreg_map(pa, i, type, 0,
		    &adev->rio_mem_bst, &adev->rio_mem_bsh, NULL,
		    &adev->rio_mem_size, 0)) {
			printf(": can't map rio space\n");
			return;
		}
		break;
	}

	if (adev->family >= CHIP_BONAIRE) {
		type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, 0x18);
		if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
		    pci_mapreg_map(pa, 0x18, type, 0,
		    &adev->doorbell.bst, &adev->doorbell.bsh,
		    &adev->doorbell.base, &adev->doorbell.size, 0)) {
			printf(": can't map doorbell space\n");
			return;
		}
	}

	if (adev->family >= CHIP_BONAIRE)
		rmmio_bar = 0x24;
	else
		rmmio_bar = 0x18;

	type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, rmmio_bar);
	if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
	    pci_mapreg_map(pa, rmmio_bar, type, 0,
	    &adev->rmmio_bst, &adev->rmmio_bsh, &adev->rmmio_base,
	    &adev->rmmio_size, 0)) {
		printf(": can't map rmmio space\n");
		return;
	}

	/*
	 * Make sure we have a base address for the ROM such that we
	 * can map it later.
	 */
	s = splhigh();
	addr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, ~PCI_ROM_ENABLE);
	mask = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, addr);
	splx(s);

	if (addr == 0 && PCI_ROM_SIZE(mask) != 0 && pa->pa_memex) {
		bus_size_t size, start, end;
		bus_addr_t base;

		size = PCI_ROM_SIZE(mask);
		start = max(PCI_MEM_START, pa->pa_memex->ex_start);
		end = min(PCI_MEM_END, pa->pa_memex->ex_end);
		if (extent_alloc_subregion(pa->pa_memex, start, end, size,
		    size, 0, 0, 0, &base) == 0)
			pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, base);
	}

	printf("\n");

	/* from amdgpu_init() */
	if (amdgpu_refcnt == 0) {
		drm_sched_fence_slab_init();

		if (amdgpu_sync_init()) {
			printf(": amdgpu_sync_init failed\n");
			return;
		}

		if (amdgpu_fence_slab_init()) {
			amdgpu_sync_fini();
			printf(": amdgpu_fence_slab_init failed\n");
			return;
		}

		amdgpu_kms_driver.num_ioctls = amdgpu_max_kms_ioctl;
		amdgpu_register_atpx_handler();
	}
	amdgpu_refcnt++;

	/* from amdgpu_pci_probe() */
{
	int ret;
	bool supports_atomic = false;

	if (!amdgpu_virtual_display &&
	     amdgpu_device_asic_has_dc_support(adev->family))
		supports_atomic = true;

	if ((adev->flags & AMD_EXP_HW_SUPPORT) && !amdgpu_exp_hw_support) {
		DRM_INFO("This hardware requires experimental hardware support.\n");
	}

	/*
	 * Initialize amdkfd before starting radeon.
	 */
	amdgpu_amdkfd_init();

	/* warn the user if they mix atomic and non-atomic capable GPUs */
	if ((amdgpu_kms_driver.driver_features & DRIVER_ATOMIC) && !supports_atomic)
		DRM_ERROR("Mixing atomic and non-atomic capable GPUs!\n");
	/* support atomic early so the atomic debugfs stuff gets created */
	if (supports_atomic)
		amdgpu_kms_driver.driver_features |= DRIVER_ATOMIC;
}

	dev = drm_attach_pci(&amdgpu_kms_driver, pa, 0, adev->primary,
	    self, NULL);
	adev->ddev = dev;
	adev->pdev = dev->pdev;

	if (!amdgpu_msi_ok(adev))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	adev->irq.msi_enabled = false;
	if (pci_intr_map_msi(pa, &adev->intrh) == 0)
		adev->irq.msi_enabled = true;
	else if (pci_intr_map(pa, &adev->intrh) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}
	printf("%s: %s\n", adev->self.dv_xname,
	    pci_intr_string(pa->pa_pc, adev->intrh));

	adev->irqh = pci_intr_establish(pa->pa_pc, adev->intrh, IPL_TTY,
	    amdgpu_irq_handler, adev->ddev, adev->self.dv_xname);
	if (adev->irqh == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    adev->self.dv_xname);
		return;
	}
	adev->pdev->irq = -1;

	fb_aper = bus_space_mmap(adev->memt, adev->fb_aper_offset, 0, 0, 0);
	if (fb_aper != -1)
		rasops_claim_framebuffer(fb_aper, adev->fb_aper_size, self);


	adev->shutdown = true;
	config_mountroot(self, amdgpu_attachhook);
}

int
amdgpu_forcedetach(struct amdgpu_device *adev)
{
	struct pci_softc	*sc = (struct pci_softc *)adev->self.dv_parent;
	pcitag_t		 tag = adev->pa_tag;

#if NVGA > 0
	if (adev->primary)
		vga_console_attached = 0;
#endif

	/* reprobe pci device for non efi systems */
#if NEFIFB > 0
	if (bios_efiinfo == NULL && !efifb_cb_found()) {
#endif
		config_detach(&adev->self, 0);
		return pci_probe_device(sc, tag, NULL, NULL);
#if NEFIFB > 0
	} else if (adev->primary) {
		efifb_reattach();
	}
#endif

	return 0;
}

void amdgpu_burner(void *, u_int, u_int);
int amdgpu_wsioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t amdgpu_wsmmap(void *, off_t, int);
int amdgpu_alloc_screen(void *, const struct wsscreen_descr *,
    void **, int *, int *, long *);
void amdgpu_free_screen(void *, void *);
int amdgpu_show_screen(void *, void *, int,
    void (*)(void *, int, int), void *);
void amdgpu_doswitch(void *);
void amdgpu_enter_ddb(void *, void *);

struct wsscreen_descr amdgpu_stdscreen = {
	"std",
	0, 0,
	0,
	0, 0,
	WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *amdgpu_scrlist[] = {
	&amdgpu_stdscreen,
};

struct wsscreen_list amdgpu_screenlist = {
	nitems(amdgpu_scrlist), amdgpu_scrlist
};

struct wsdisplay_accessops amdgpu_accessops = {
	.ioctl = amdgpu_wsioctl,
	.mmap = amdgpu_wsmmap,
	.alloc_screen = amdgpu_alloc_screen,
	.free_screen = amdgpu_free_screen,
	.show_screen = amdgpu_show_screen,
	.enter_ddb = amdgpu_enter_ddb,
	.getchar = rasops_getchar,
	.load_font = rasops_load_font,
	.list_font = rasops_list_font,
	.scrollback = rasops_scrollback,
	.burn_screen = amdgpu_burner
};

int
amdgpu_wsioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct rasops_info *ri = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = WSDISPLAY_TYPE_RADEONDRM;
		return 0;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;
		return 0;
	default:
		return -1;
	}
}

paddr_t
amdgpu_wsmmap(void *v, off_t off, int prot)
{
	return (-1);
}

int
amdgpu_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	return rasops_alloc_screen(v, cookiep, curxp, curyp, attrp);
}

void
amdgpu_free_screen(void *v, void *cookie)
{
	return rasops_free_screen(v, cookie);
}

int
amdgpu_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct rasops_info *ri = v;
	struct amdgpu_device *adev = ri->ri_hw;

	if (cookie == ri->ri_active)
		return (0);

	adev->switchcb = cb;
	adev->switchcbarg = cbarg;
	adev->switchcookie = cookie;
	if (cb) {
		task_add(systq, &adev->switchtask);
		return (EAGAIN);
	}

	amdgpu_doswitch(v);

	return (0);
}

void
amdgpu_doswitch(void *v)
{
	struct rasops_info *ri = v;
	struct amdgpu_device *adev = ri->ri_hw;
	struct amdgpu_crtc *amdgpu_crtc;
	int i, crtc;

	rasops_show_screen(ri, adev->switchcookie, 0, NULL, NULL);
	drm_fb_helper_restore_fbdev_mode_unlocked((void *)adev->mode_info.rfbdev);

	if (adev->switchcb)
		(adev->switchcb)(adev->switchcbarg, 0, 0);
}

void
amdgpu_enter_ddb(void *v, void *cookie)
{
	struct rasops_info *ri = v;
	struct amdgpu_device *adev = ri->ri_hw;
	struct drm_fb_helper *fb_helper = (void *)adev->mode_info.rfbdev;

	if (cookie == ri->ri_active)
		return;

	rasops_show_screen(ri, cookie, 0, NULL, NULL);
	drm_fb_helper_debug_enter(fb_helper->fbdev);
}


void
amdgpu_attachhook(struct device *self)
{
	struct amdgpu_device	*adev = (struct amdgpu_device *)self;
	struct drm_device	*dev = adev->ddev;
	int r, acpi_status;

	if ((amdgpu_runtime_pm != 0) &&
	    amdgpu_has_atpx() &&
	    (amdgpu_is_atpx_hybrid() ||
	     amdgpu_has_atpx_dgpu_power_cntl()) &&
	    ((adev->flags & AMD_IS_APU) == 0) &&
	    !pci_is_thunderbolt_attached(dev->pdev))
		adev->flags |= AMD_IS_PX;

	/* amdgpu_device_init should report only fatal error
	 * like memory allocation failure or iomapping failure,
	 * or memory manager initialization failure, it must
	 * properly initialize the GPU MC controller and permit
	 * VRAM allocation
	 */
	r = amdgpu_device_init(adev, dev, dev->pdev, adev->flags);
	if (r) {
		dev_err(&dev->pdev->dev, "Fatal error during GPU init\n");
		goto out;
	}

	/* Call ACPI methods: require modeset init
	 * but failure is not fatal
	 */
	if (!r) {
		acpi_status = amdgpu_acpi_init(adev);
		if (acpi_status)
		dev_dbg(&dev->pdev->dev,
				"Error during ACPI methods call\n");
	}

	if (amdgpu_device_is_px(dev)) {
		pm_runtime_use_autosuspend(dev->dev);
		pm_runtime_set_autosuspend_delay(dev->dev, 5000);
		pm_runtime_set_active(dev->dev);
		pm_runtime_allow(dev->dev);
		pm_runtime_mark_last_busy(dev->dev);
		pm_runtime_put_autosuspend(dev->dev);
	}

{
	struct drm_fb_helper *fb_helper = (void *)adev->mode_info.rfbdev;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &adev->ro;

	task_set(&adev->switchtask, amdgpu_doswitch, ri);

	if (ri->ri_bits == NULL)
		return;

	drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper);

	ri->ri_flg = RI_CENTER | RI_VCONS | RI_WRONLY;
	rasops_init(ri, 160, 160);

	ri->ri_hw = adev;

	amdgpu_stdscreen.capabilities = ri->ri_caps;
	amdgpu_stdscreen.nrows = ri->ri_rows;
	amdgpu_stdscreen.ncols = ri->ri_cols;
	amdgpu_stdscreen.textops = &ri->ri_ops;
	amdgpu_stdscreen.fontwidth = ri->ri_font->fontwidth;
	amdgpu_stdscreen.fontheight = ri->ri_font->fontheight;

	aa.console = adev->console;
	aa.primary = adev->primary;
	aa.scrdata = &amdgpu_screenlist;
	aa.accessops = &amdgpu_accessops;
	aa.accesscookie = ri;
	aa.defaultscreens = 0;

	if (adev->console) {
		long defattr;

		ri->ri_ops.alloc_attr(ri->ri_active, 0, 0, 0, &defattr);
		wsdisplay_cnattach(&amdgpu_stdscreen, ri->ri_active,
		    ri->ri_ccol, ri->ri_crow, defattr);
	}

	/*
	 * Now that we've taken over the console, disable decoding of
	 * VGA legacy addresses, and opt out of arbitration.
	 */
	amdgpu_asic_set_vga_state(adev, false);
	pci_disable_legacy_vga(&adev->self);

	printf("%s: %dx%d, %dbpp\n", adev->self.dv_xname,
	    ri->ri_width, ri->ri_height, ri->ri_depth);

	config_found_sm(&adev->self, &aa, wsemuldisplaydevprint,
	    wsemuldisplaydevsubmatch);

	/*
	 * in linux via amdgpu_pci_probe -> drm_dev_register
	 */
	drm_dev_register(dev, adev->flags);
}

out:
	if (r) {
		/* balance pm_runtime_get_sync in amdgpu_driver_unload_kms */
		if (amdgpu_device_is_px(dev))
			pm_runtime_put_noidle(dev->dev);
		amdgpu_fatal_error = 1;
		amdgpu_forcedetach(adev);
	}
}

/* from amdgpu_exit amdgpu_driver_unload_kms */
int
amdgpu_detach(struct device *self, int flags)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)self;
	struct drm_device *dev = adev->ddev;

	if (adev == NULL)
		return 0;

	amdgpu_refcnt--;

	if (amdgpu_refcnt == 0)
		amdgpu_amdkfd_fini();

	pci_intr_disestablish(adev->pc, adev->irqh);

	if (amdgpu_sriov_vf(adev))
		amdgpu_virt_request_full_gpu(adev, false);

	if (amdgpu_device_is_px(dev)) {
		pm_runtime_get_sync(dev->dev);
		pm_runtime_forbid(dev->dev);
	}

	amdgpu_acpi_fini(adev);

	amdgpu_device_fini(adev);

	if (amdgpu_refcnt == 0) {
		amdgpu_unregister_atpx_handler();
		amdgpu_sync_fini();
		amdgpu_fence_slab_fini();

		drm_sched_fence_slab_fini();
	}
	
	if (adev->ddev != NULL) {
		config_detach(adev->ddev->dev, flags);
		adev->ddev = NULL;
	}

	return 0;
}

int     
amdgpu_activate(struct device *self, int act)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)self;
	int rv = 0;

	if (adev->ddev == NULL)
		return (0);

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_activate_children(self, act);
		amdgpu_device_suspend(adev->ddev, true, true);
		break;
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		break;
	case DVACT_WAKEUP:
		amdgpu_device_resume(adev->ddev, true, true);
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}
