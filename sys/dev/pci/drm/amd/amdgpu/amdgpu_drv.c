/*
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <drm/drmP.h>
#include <drm/amdgpu_drm.h>
#include <drm/drm_gem.h>
#include "amdgpu_drv.h"

#include <drm/drm_pciids.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/vga_switcheroo.h>
#include <drm/drm_crtc_helper.h>

#include "amdgpu.h"
#include "amdgpu_irq.h"

#include "amdgpu_amdkfd.h"

/*
 * KMS wrapper.
 * - 3.0.0 - initial driver
 * - 3.1.0 - allow reading more status registers (GRBM, SRBM, SDMA, CP)
 * - 3.2.0 - GFX8: Uses EOP_TC_WB_ACTION_EN, so UMDs don't have to do the same
 *           at the end of IBs.
 * - 3.3.0 - Add VM support for UVD on supported hardware.
 * - 3.4.0 - Add AMDGPU_INFO_NUM_EVICTIONS.
 * - 3.5.0 - Add support for new UVD_NO_OP register.
 * - 3.6.0 - kmd involves use CONTEXT_CONTROL in ring buffer.
 * - 3.7.0 - Add support for VCE clock list packet
 * - 3.8.0 - Add support raster config init in the kernel
 * - 3.9.0 - Add support for memory query info about VRAM and GTT.
 * - 3.10.0 - Add support for new fences ioctl, new gem ioctl flags
 * - 3.11.0 - Add support for sensor query info (clocks, temp, etc).
 * - 3.12.0 - Add query for double offchip LDS buffers
 * - 3.13.0 - Add PRT support
 * - 3.14.0 - Fix race in amdgpu_ctx_get_fence() and note new functionality
 * - 3.15.0 - Export more gpu info for gfx9
 * - 3.16.0 - Add reserved vmid support
 * - 3.17.0 - Add AMDGPU_NUM_VRAM_CPU_PAGE_FAULTS.
 * - 3.18.0 - Export gpu always on cu bitmap
 * - 3.19.0 - Add support for UVD MJPEG decode
 * - 3.20.0 - Add support for local BOs
 * - 3.21.0 - Add DRM_AMDGPU_FENCE_TO_HANDLE ioctl
 * - 3.22.0 - Add DRM_AMDGPU_SCHED ioctl
 * - 3.23.0 - Add query for VRAM lost counter
 * - 3.24.0 - Add high priority compute support for gfx9
 * - 3.25.0 - Add support for sensor query info (stable pstate sclk/mclk).
 * - 3.26.0 - GFX9: Process AMDGPU_IB_FLAG_TC_WB_NOT_INVALIDATE.
 * - 3.27.0 - Add new chunk to to AMDGPU_CS to enable BO_LIST creation.
 */
#define KMS_DRIVER_MAJOR	3
#define KMS_DRIVER_MINOR	27
#define KMS_DRIVER_PATCHLEVEL	0

int amdgpu_vram_limit = 0;
int amdgpu_vis_vram_limit = 0;
int amdgpu_gart_size = -1; /* auto */
int amdgpu_gtt_size = -1; /* auto */
int amdgpu_moverate = -1; /* auto */
int amdgpu_benchmarking = 0;
int amdgpu_testing = 0;
int amdgpu_audio = -1;
int amdgpu_disp_priority = 0;
int amdgpu_hw_i2c = 0;
int amdgpu_pcie_gen2 = -1;
int amdgpu_msi = -1;
int amdgpu_lockup_timeout = 10000;
int amdgpu_dpm = -1;
int amdgpu_fw_load_type = -1;
int amdgpu_aspm = -1;
int amdgpu_runtime_pm = -1;
uint amdgpu_ip_block_mask = 0xffffffff;
int amdgpu_bapm = -1;
int amdgpu_deep_color = 0;
int amdgpu_vm_size = -1;
int amdgpu_vm_fragment_size = -1;
int amdgpu_vm_block_size = -1;
int amdgpu_vm_fault_stop = 0;
int amdgpu_vm_debug = 0;
int amdgpu_vram_page_split = 512;
int amdgpu_vm_update_mode = -1;
int amdgpu_exp_hw_support = 0;
int amdgpu_dc = -1;
int amdgpu_sched_jobs = 32;
int amdgpu_sched_hw_submission = 2;
uint amdgpu_pcie_gen_cap = 0;
uint amdgpu_pcie_lane_cap = 0;
uint amdgpu_cg_mask = 0xffffffff;
uint amdgpu_pg_mask = 0xffffffff;
uint amdgpu_sdma_phase_quantum = 32;
char *amdgpu_disable_cu = NULL;
char *amdgpu_virtual_display = NULL;
/* OverDrive(bit 14),gfxoff(bit 15),stutter mode(bit 17) disabled by default*/
uint amdgpu_pp_feature_mask = 0xfffd3fff;
int amdgpu_ngg = 0;
int amdgpu_prim_buf_per_se = 0;
int amdgpu_pos_buf_per_se = 0;
int amdgpu_cntl_sb_buf_per_se = 0;
int amdgpu_param_buf_per_se = 0;
int amdgpu_job_hang_limit = 0;
int amdgpu_lbpw = -1;
int amdgpu_compute_multipipe = -1;
int amdgpu_gpu_recovery = -1; /* auto */
int amdgpu_emu_mode = 0;
uint amdgpu_smu_memory_pool_size = 0;

/**
 * DOC: vramlimit (int)
 * Restrict the total amount of VRAM in MiB for testing.  The default is 0 (Use full VRAM).
 */
MODULE_PARM_DESC(vramlimit, "Restrict VRAM for testing, in megabytes");
module_param_named(vramlimit, amdgpu_vram_limit, int, 0600);

/**
 * DOC: vis_vramlimit (int)
 * Restrict the amount of CPU visible VRAM in MiB for testing.  The default is 0 (Use full CPU visible VRAM).
 */
MODULE_PARM_DESC(vis_vramlimit, "Restrict visible VRAM for testing, in megabytes");
module_param_named(vis_vramlimit, amdgpu_vis_vram_limit, int, 0444);

/**
 * DOC: gartsize (uint)
 * Restrict the size of GART in Mib (32, 64, etc.) for testing. The default is -1 (The size depends on asic).
 */
MODULE_PARM_DESC(gartsize, "Size of GART to setup in megabytes (32, 64, etc., -1=auto)");
module_param_named(gartsize, amdgpu_gart_size, uint, 0600);

/**
 * DOC: gttsize (int)
 * Restrict the size of GTT domain in MiB for testing. The default is -1 (It's VRAM size if 3GB < VRAM < 3/4 RAM,
 * otherwise 3/4 RAM size).
 */
MODULE_PARM_DESC(gttsize, "Size of the GTT domain in megabytes (-1 = auto)");
module_param_named(gttsize, amdgpu_gtt_size, int, 0600);

/**
 * DOC: moverate (int)
 * Set maximum buffer migration rate in MB/s. The default is -1 (8 MB/s).
 */
MODULE_PARM_DESC(moverate, "Maximum buffer migration rate in MB/s. (32, 64, etc., -1=auto, 0=1=disabled)");
module_param_named(moverate, amdgpu_moverate, int, 0600);

/**
 * DOC: benchmark (int)
 * Run benchmarks. The default is 0 (Skip benchmarks).
 */
MODULE_PARM_DESC(benchmark, "Run benchmark");
module_param_named(benchmark, amdgpu_benchmarking, int, 0444);

/**
 * DOC: test (int)
 * Test BO GTT->VRAM and VRAM->GTT GPU copies. The default is 0 (Skip test, only set 1 to run test).
 */
MODULE_PARM_DESC(test, "Run tests");
module_param_named(test, amdgpu_testing, int, 0444);

/**
 * DOC: audio (int)
 * Set HDMI/DPAudio. Only affects non-DC display handling. The default is -1 (Enabled), set 0 to disabled it.
 */
MODULE_PARM_DESC(audio, "Audio enable (-1 = auto, 0 = disable, 1 = enable)");
module_param_named(audio, amdgpu_audio, int, 0444);

/**
 * DOC: disp_priority (int)
 * Set display Priority (1 = normal, 2 = high). Only affects non-DC display handling. The default is 0 (auto).
 */
MODULE_PARM_DESC(disp_priority, "Display Priority (0 = auto, 1 = normal, 2 = high)");
module_param_named(disp_priority, amdgpu_disp_priority, int, 0444);

/**
 * DOC: hw_i2c (int)
 * To enable hw i2c engine. Only affects non-DC display handling. The default is 0 (Disabled).
 */
MODULE_PARM_DESC(hw_i2c, "hw i2c engine enable (0 = disable)");
module_param_named(hw_i2c, amdgpu_hw_i2c, int, 0444);

/**
 * DOC: pcie_gen2 (int)
 * To disable PCIE Gen2/3 mode (0 = disable, 1 = enable). The default is -1 (auto, enabled).
 */
MODULE_PARM_DESC(pcie_gen2, "PCIE Gen2 mode (-1 = auto, 0 = disable, 1 = enable)");
module_param_named(pcie_gen2, amdgpu_pcie_gen2, int, 0444);

/**
 * DOC: msi (int)
 * To disable Message Signaled Interrupts (MSI) functionality (1 = enable, 0 = disable). The default is -1 (auto, enabled).
 */
MODULE_PARM_DESC(msi, "MSI support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(msi, amdgpu_msi, int, 0444);

/**
 * DOC: lockup_timeout (int)
 * Set GPU scheduler timeout value in ms. Value 0 is invalidated, will be adjusted to 10000.
 * Negative values mean 'infinite timeout' (MAX_JIFFY_OFFSET). The default is 10000.
 */
MODULE_PARM_DESC(lockup_timeout, "GPU lockup timeout in ms > 0 (default 10000)");
module_param_named(lockup_timeout, amdgpu_lockup_timeout, int, 0444);

/**
 * DOC: dpm (int)
 * Override for dynamic power management setting (1 = enable, 0 = disable). The default is -1 (auto).
 */
MODULE_PARM_DESC(dpm, "DPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(dpm, amdgpu_dpm, int, 0444);

/**
 * DOC: fw_load_type (int)
 * Set different firmware loading type for debugging (0 = direct, 1 = SMU, 2 = PSP). The default is -1 (auto).
 */
MODULE_PARM_DESC(fw_load_type, "firmware loading type (0 = direct, 1 = SMU, 2 = PSP, -1 = auto)");
module_param_named(fw_load_type, amdgpu_fw_load_type, int, 0444);

/**
 * DOC: aspm (int)
 * To disable ASPM (1 = enable, 0 = disable). The default is -1 (auto, enabled).
 */
MODULE_PARM_DESC(aspm, "ASPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(aspm, amdgpu_aspm, int, 0444);

/**
 * DOC: runpm (int)
 * Override for runtime power management control for dGPUs in PX/HG laptops. The amdgpu driver can dynamically power down
 * the dGPU on PX/HG laptops when it is idle. The default is -1 (auto enable). Setting the value to 0 disables this functionality.
 */
MODULE_PARM_DESC(runpm, "PX runtime pm (1 = force enable, 0 = disable, -1 = PX only default)");
module_param_named(runpm, amdgpu_runtime_pm, int, 0444);

/**
 * DOC: ip_block_mask (uint)
 * Override what IP blocks are enabled on the GPU. Each GPU is a collection of IP blocks (gfx, display, video, etc.).
 * Use this parameter to disable specific blocks. Note that the IP blocks do not have a fixed index. Some asics may not have
 * some IPs or may include multiple instances of an IP so the ordering various from asic to asic. See the driver output in
 * the kernel log for the list of IPs on the asic. The default is 0xffffffff (enable all blocks on a device).
 */
MODULE_PARM_DESC(ip_block_mask, "IP Block Mask (all blocks enabled (default))");
module_param_named(ip_block_mask, amdgpu_ip_block_mask, uint, 0444);

/**
 * DOC: bapm (int)
 * Bidirectional Application Power Management (BAPM) used to dynamically share TDP between CPU and GPU. Set value 0 to disable it.
 * The default -1 (auto, enabled)
 */
MODULE_PARM_DESC(bapm, "BAPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(bapm, amdgpu_bapm, int, 0444);

/**
 * DOC: deep_color (int)
 * Set 1 to enable Deep Color support. Only affects non-DC display handling. The default is 0 (disabled).
 */
MODULE_PARM_DESC(deep_color, "Deep Color support (1 = enable, 0 = disable (default))");
module_param_named(deep_color, amdgpu_deep_color, int, 0444);

/**
 * DOC: vm_size (int)
 * Override the size of the GPU's per client virtual address space in GiB.  The default is -1 (automatic for each asic).
 */
MODULE_PARM_DESC(vm_size, "VM address space size in gigabytes (default 64GB)");
module_param_named(vm_size, amdgpu_vm_size, int, 0444);

/**
 * DOC: vm_fragment_size (int)
 * Override VM fragment size in bits (4, 5, etc. 4 = 64K, 9 = 2M). The default is -1 (automatic for each asic).
 */
MODULE_PARM_DESC(vm_fragment_size, "VM fragment size in bits (4, 5, etc. 4 = 64K (default), Max 9 = 2M)");
module_param_named(vm_fragment_size, amdgpu_vm_fragment_size, int, 0444);

/**
 * DOC: vm_block_size (int)
 * Override VM page table size in bits (default depending on vm_size and hw setup). The default is -1 (automatic for each asic).
 */
MODULE_PARM_DESC(vm_block_size, "VM page table size in bits (default depending on vm_size)");
module_param_named(vm_block_size, amdgpu_vm_block_size, int, 0444);

/**
 * DOC: vm_fault_stop (int)
 * Stop on VM fault for debugging (0 = never, 1 = print first, 2 = always). The default is 0 (No stop).
 */
MODULE_PARM_DESC(vm_fault_stop, "Stop on VM fault (0 = never (default), 1 = print first, 2 = always)");
module_param_named(vm_fault_stop, amdgpu_vm_fault_stop, int, 0444);

/**
 * DOC: vm_debug (int)
 * Debug VM handling (0 = disabled, 1 = enabled). The default is 0 (Disabled).
 */
MODULE_PARM_DESC(vm_debug, "Debug VM handling (0 = disabled (default), 1 = enabled)");
module_param_named(vm_debug, amdgpu_vm_debug, int, 0644);

/**
 * DOC: vm_update_mode (int)
 * Override VM update mode. VM updated by using CPU (0 = never, 1 = Graphics only, 2 = Compute only, 3 = Both). The default
 * is -1 (Only in large BAR(LB) systems Compute VM tables will be updated by CPU, otherwise 0, never).
 */
MODULE_PARM_DESC(vm_update_mode, "VM update using CPU (0 = never (default except for large BAR(LB)), 1 = Graphics only, 2 = Compute only (default for LB), 3 = Both");
module_param_named(vm_update_mode, amdgpu_vm_update_mode, int, 0444);

/**
 * DOC: vram_page_split (int)
 * Override the number of pages after we split VRAM allocations (default 512, -1 = disable). The default is 512.
 */
MODULE_PARM_DESC(vram_page_split, "Number of pages after we split VRAM allocations (default 512, -1 = disable)");
module_param_named(vram_page_split, amdgpu_vram_page_split, int, 0444);

/**
 * DOC: exp_hw_support (int)
 * Enable experimental hw support (1 = enable). The default is 0 (disabled).
 */
MODULE_PARM_DESC(exp_hw_support, "experimental hw support (1 = enable, 0 = disable (default))");
module_param_named(exp_hw_support, amdgpu_exp_hw_support, int, 0444);

/**
 * DOC: dc (int)
 * Disable/Enable Display Core driver for debugging (1 = enable, 0 = disable). The default is -1 (automatic for each asic).
 */
MODULE_PARM_DESC(dc, "Display Core driver (1 = enable, 0 = disable, -1 = auto (default))");
module_param_named(dc, amdgpu_dc, int, 0444);

/**
 * DOC: sched_jobs (int)
 * Override the max number of jobs supported in the sw queue. The default is 32.
 */
MODULE_PARM_DESC(sched_jobs, "the max number of jobs supported in the sw queue (default 32)");
module_param_named(sched_jobs, amdgpu_sched_jobs, int, 0444);

/**
 * DOC: sched_hw_submission (int)
 * Override the max number of HW submissions. The default is 2.
 */
MODULE_PARM_DESC(sched_hw_submission, "the max number of HW submissions (default 2)");
module_param_named(sched_hw_submission, amdgpu_sched_hw_submission, int, 0444);

/**
 * DOC: ppfeaturemask (uint)
 * Override power features enabled. See enum PP_FEATURE_MASK in drivers/gpu/drm/amd/include/amd_shared.h.
 * The default is the current set of stable power features.
 */
MODULE_PARM_DESC(ppfeaturemask, "all power features enabled (default))");
module_param_named(ppfeaturemask, amdgpu_pp_feature_mask, uint, 0444);

/**
 * DOC: pcie_gen_cap (uint)
 * Override PCIE gen speed capabilities. See the CAIL flags in drivers/gpu/drm/amd/include/amd_pcie.h.
 * The default is 0 (automatic for each asic).
 */
MODULE_PARM_DESC(pcie_gen_cap, "PCIE Gen Caps (0: autodetect (default))");
module_param_named(pcie_gen_cap, amdgpu_pcie_gen_cap, uint, 0444);

/**
 * DOC: pcie_lane_cap (uint)
 * Override PCIE lanes capabilities. See the CAIL flags in drivers/gpu/drm/amd/include/amd_pcie.h.
 * The default is 0 (automatic for each asic).
 */
MODULE_PARM_DESC(pcie_lane_cap, "PCIE Lane Caps (0: autodetect (default))");
module_param_named(pcie_lane_cap, amdgpu_pcie_lane_cap, uint, 0444);

/**
 * DOC: cg_mask (uint)
 * Override Clockgating features enabled on GPU (0 = disable clock gating). See the AMD_CG_SUPPORT flags in
 * drivers/gpu/drm/amd/include/amd_shared.h. The default is 0xffffffff (all enabled).
 */
MODULE_PARM_DESC(cg_mask, "Clockgating flags mask (0 = disable clock gating)");
module_param_named(cg_mask, amdgpu_cg_mask, uint, 0444);

/**
 * DOC: pg_mask (uint)
 * Override Powergating features enabled on GPU (0 = disable power gating). See the AMD_PG_SUPPORT flags in
 * drivers/gpu/drm/amd/include/amd_shared.h. The default is 0xffffffff (all enabled).
 */
MODULE_PARM_DESC(pg_mask, "Powergating flags mask (0 = disable power gating)");
module_param_named(pg_mask, amdgpu_pg_mask, uint, 0444);

/**
 * DOC: sdma_phase_quantum (uint)
 * Override SDMA context switch phase quantum (x 1K GPU clock cycles, 0 = no change). The default is 32.
 */
MODULE_PARM_DESC(sdma_phase_quantum, "SDMA context switch phase quantum (x 1K GPU clock cycles, 0 = no change (default 32))");
module_param_named(sdma_phase_quantum, amdgpu_sdma_phase_quantum, uint, 0444);

/**
 * DOC: disable_cu (charp)
 * Set to disable CUs (It's set like se.sh.cu,...). The default is NULL.
 */
MODULE_PARM_DESC(disable_cu, "Disable CUs (se.sh.cu,...)");
module_param_named(disable_cu, amdgpu_disable_cu, charp, 0444);

/**
 * DOC: virtual_display (charp)
 * Set to enable virtual display feature. This feature provides a virtual display hardware on headless boards
 * or in virtualized environments. It will be set like xxxx:xx:xx.x,x;xxxx:xx:xx.x,x. It's the pci address of
 * the device, plus the number of crtcs to expose. E.g., 0000:26:00.0,4 would enable 4 virtual crtcs on the pci
 * device at 26:00.0. The default is NULL.
 */
MODULE_PARM_DESC(virtual_display,
		 "Enable virtual display feature (the virtual_display will be set like xxxx:xx:xx.x,x;xxxx:xx:xx.x,x)");
module_param_named(virtual_display, amdgpu_virtual_display, charp, 0444);

/**
 * DOC: ngg (int)
 * Set to enable Next Generation Graphics (1 = enable). The default is 0 (disabled).
 */
MODULE_PARM_DESC(ngg, "Next Generation Graphics (1 = enable, 0 = disable(default depending on gfx))");
module_param_named(ngg, amdgpu_ngg, int, 0444);

/**
 * DOC: prim_buf_per_se (int)
 * Override the size of Primitive Buffer per Shader Engine in Byte. The default is 0 (depending on gfx).
 */
MODULE_PARM_DESC(prim_buf_per_se, "the size of Primitive Buffer per Shader Engine (default depending on gfx)");
module_param_named(prim_buf_per_se, amdgpu_prim_buf_per_se, int, 0444);

/**
 * DOC: pos_buf_per_se (int)
 * Override the size of Position Buffer per Shader Engine in Byte. The default is 0 (depending on gfx).
 */
MODULE_PARM_DESC(pos_buf_per_se, "the size of Position Buffer per Shader Engine (default depending on gfx)");
module_param_named(pos_buf_per_se, amdgpu_pos_buf_per_se, int, 0444);

/**
 * DOC: cntl_sb_buf_per_se (int)
 * Override the size of Control Sideband per Shader Engine in Byte. The default is 0 (depending on gfx).
 */
MODULE_PARM_DESC(cntl_sb_buf_per_se, "the size of Control Sideband per Shader Engine (default depending on gfx)");
module_param_named(cntl_sb_buf_per_se, amdgpu_cntl_sb_buf_per_se, int, 0444);

/**
 * DOC: param_buf_per_se (int)
 * Override the size of Off-Chip Pramater Cache per Shader Engine in Byte. The default is 0 (depending on gfx).
 */
MODULE_PARM_DESC(param_buf_per_se, "the size of Off-Chip Pramater Cache per Shader Engine (default depending on gfx)");
module_param_named(param_buf_per_se, amdgpu_param_buf_per_se, int, 0444);

/**
 * DOC: job_hang_limit (int)
 * Set how much time allow a job hang and not drop it. The default is 0.
 */
MODULE_PARM_DESC(job_hang_limit, "how much time allow a job hang and not drop it (default 0)");
module_param_named(job_hang_limit, amdgpu_job_hang_limit, int ,0444);

/**
 * DOC: lbpw (int)
 * Override Load Balancing Per Watt (LBPW) support (1 = enable, 0 = disable). The default is -1 (auto, enabled).
 */
MODULE_PARM_DESC(lbpw, "Load Balancing Per Watt (LBPW) support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(lbpw, amdgpu_lbpw, int, 0444);

MODULE_PARM_DESC(compute_multipipe, "Force compute queues to be spread across pipes (1 = enable, 0 = disable, -1 = auto)");
module_param_named(compute_multipipe, amdgpu_compute_multipipe, int, 0444);

/**
 * DOC: gpu_recovery (int)
 * Set to enable GPU recovery mechanism (1 = enable, 0 = disable). The default is -1 (auto, disabled except SRIOV).
 */
MODULE_PARM_DESC(gpu_recovery, "Enable GPU recovery mechanism, (1 = enable, 0 = disable, -1 = auto)");
module_param_named(gpu_recovery, amdgpu_gpu_recovery, int, 0444);

/**
 * DOC: emu_mode (int)
 * Set value 1 to enable emulation mode. This is only needed when running on an emulator. The default is 0 (disabled).
 */
MODULE_PARM_DESC(emu_mode, "Emulation mode, (1 = enable, 0 = disable)");
module_param_named(emu_mode, amdgpu_emu_mode, int, 0444);

/**
 * DOC: si_support (int)
 * Set SI support driver. This parameter works after set config CONFIG_DRM_AMDGPU_SI. For SI asic, when radeon driver is enabled,
 * set value 0 to use radeon driver, while set value 1 to use amdgpu driver. The default is using radeon driver when it available,
 * otherwise using amdgpu driver.
 */
#ifdef CONFIG_DRM_AMDGPU_SI

#if defined(CONFIG_DRM_RADEON) || defined(CONFIG_DRM_RADEON_MODULE)
int amdgpu_si_support = 0;
MODULE_PARM_DESC(si_support, "SI support (1 = enabled, 0 = disabled (default))");
#else
int amdgpu_si_support = 1;
MODULE_PARM_DESC(si_support, "SI support (1 = enabled (default), 0 = disabled)");
#endif

module_param_named(si_support, amdgpu_si_support, int, 0444);
#endif

/**
 * DOC: cik_support (int)
 * Set CIK support driver. This parameter works after set config CONFIG_DRM_AMDGPU_CIK. For CIK asic, when radeon driver is enabled,
 * set value 0 to use radeon driver, while set value 1 to use amdgpu driver. The default is using radeon driver when it available,
 * otherwise using amdgpu driver.
 */
#ifdef CONFIG_DRM_AMDGPU_CIK

#if defined(CONFIG_DRM_RADEON) || defined(CONFIG_DRM_RADEON_MODULE)
int amdgpu_cik_support = 0;
MODULE_PARM_DESC(cik_support, "CIK support (1 = enabled, 0 = disabled (default))");
#else
int amdgpu_cik_support = 1;
MODULE_PARM_DESC(cik_support, "CIK support (1 = enabled (default), 0 = disabled)");
#endif

module_param_named(cik_support, amdgpu_cik_support, int, 0444);
#endif

/**
 * DOC: smu_memory_pool_size (uint)
 * It is used to reserve gtt for smu debug usage, setting value 0 to disable it. The actual size is value * 256MiB.
 * E.g. 0x1 = 256Mbyte, 0x2 = 512Mbyte, 0x4 = 1 Gbyte, 0x8 = 2GByte. The default is 0 (disabled).
 */
MODULE_PARM_DESC(smu_memory_pool_size,
	"reserve gtt for smu debug usage, 0 = disable,"
		"0x1 = 256Mbyte, 0x2 = 512Mbyte, 0x4 = 1 Gbyte, 0x8 = 2GByte");
module_param_named(smu_memory_pool_size, amdgpu_smu_memory_pool_size, uint, 0444);

const struct drm_pcidev amdgpu_pciidlist[] = {
#ifdef  CONFIG_DRM_AMDGPU_SI
	{0x1002, 0x6780, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6784, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6788, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x678A, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6790, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6791, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6792, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6798, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6799, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x679A, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x679B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x679E, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x679F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TAHITI},
	{0x1002, 0x6800, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN|AMD_IS_MOBILITY},
	{0x1002, 0x6801, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN|AMD_IS_MOBILITY},
	{0x1002, 0x6802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN|AMD_IS_MOBILITY},
	{0x1002, 0x6806, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6808, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6809, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6810, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6811, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6816, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6817, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6818, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6819, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_PITCAIRN},
	{0x1002, 0x6600, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6601, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6602, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6603, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6604, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6605, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6606, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6607, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6608, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND},
	{0x1002, 0x6610, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND},
	{0x1002, 0x6611, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND},
	{0x1002, 0x6613, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND},
	{0x1002, 0x6617, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6620, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6621, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6623, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND|AMD_IS_MOBILITY},
	{0x1002, 0x6631, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_OLAND},
	{0x1002, 0x6820, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6821, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6822, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6823, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6824, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6825, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6826, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6827, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6828, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x6829, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x682A, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x682B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x682C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x682D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x682F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6830, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6831, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE|AMD_IS_MOBILITY},
	{0x1002, 0x6835, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x6837, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x6838, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x6839, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x683B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x683D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x683F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VERDE},
	{0x1002, 0x6660, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAINAN|AMD_IS_MOBILITY},
	{0x1002, 0x6663, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAINAN|AMD_IS_MOBILITY},
	{0x1002, 0x6664, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAINAN|AMD_IS_MOBILITY},
	{0x1002, 0x6665, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAINAN|AMD_IS_MOBILITY},
	{0x1002, 0x6667, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAINAN|AMD_IS_MOBILITY},
	{0x1002, 0x666F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAINAN|AMD_IS_MOBILITY},
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
	/* Kaveri */
	{0x1002, 0x1304, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x1305, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1306, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x1307, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1309, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x130A, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x130B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x130C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x130D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x130E, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x130F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1310, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1311, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1312, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1313, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1315, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1316, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x1317, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x1318, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x131B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x131C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	{0x1002, 0x131D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KAVERI|AMD_IS_APU},
	/* Bonaire */
	{0x1002, 0x6640, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE|AMD_IS_MOBILITY},
	{0x1002, 0x6641, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE|AMD_IS_MOBILITY},
	{0x1002, 0x6646, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE|AMD_IS_MOBILITY},
	{0x1002, 0x6647, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE|AMD_IS_MOBILITY},
	{0x1002, 0x6649, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE},
	{0x1002, 0x6650, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE},
	{0x1002, 0x6651, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE},
	{0x1002, 0x6658, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE},
	{0x1002, 0x665c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE},
	{0x1002, 0x665d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE},
	{0x1002, 0x665f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_BONAIRE},
	/* Hawaii */
	{0x1002, 0x67A0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67A1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67A2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67A8, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67A9, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67AA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67B0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67B1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67B8, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67B9, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67BA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	{0x1002, 0x67BE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_HAWAII},
	/* Kabini */
	{0x1002, 0x9830, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9831, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x9832, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9833, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x9834, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9835, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x9836, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9837, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x9838, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9839, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x983a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x983b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x983c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x983d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x983e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	{0x1002, 0x983f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_KABINI|AMD_IS_APU},
	/* mullins */
	{0x1002, 0x9850, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9851, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9852, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9853, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9854, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9855, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9856, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9857, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9858, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x9859, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x985A, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x985B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x985C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x985D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x985E, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
	{0x1002, 0x985F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MULLINS|AMD_IS_MOBILITY|AMD_IS_APU},
#endif
	/* topaz */
	{0x1002, 0x6900, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TOPAZ},
	{0x1002, 0x6901, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TOPAZ},
	{0x1002, 0x6902, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TOPAZ},
	{0x1002, 0x6903, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TOPAZ},
	{0x1002, 0x6907, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TOPAZ},
	/* tonga */
	{0x1002, 0x6920, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x6921, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x6928, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x6929, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x692B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x692F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x6930, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x6938, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	{0x1002, 0x6939, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TONGA},
	/* fiji */
	{0x1002, 0x7300, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_FIJI},
	{0x1002, 0x730F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_FIJI},
	/* carrizo */
	{0x1002, 0x9870, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_CARRIZO|AMD_IS_APU},
	{0x1002, 0x9874, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_CARRIZO|AMD_IS_APU},
	{0x1002, 0x9875, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_CARRIZO|AMD_IS_APU},
	{0x1002, 0x9876, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_CARRIZO|AMD_IS_APU},
	{0x1002, 0x9877, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_CARRIZO|AMD_IS_APU},
	/* stoney */
	{0x1002, 0x98E4, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_STONEY|AMD_IS_APU},
	/* Polaris11 */
	{0x1002, 0x67E0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67E3, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67E8, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67EB, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67EF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67FF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67E1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67E7, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	{0x1002, 0x67E9, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS11},
	/* Polaris10 */
	{0x1002, 0x67C0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67C1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67C2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67C4, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67C7, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67D0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67DF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67C8, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67C9, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67CA, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67CC, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x67CF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	{0x1002, 0x6FDF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10},
	/* Polaris12 */
	{0x1002, 0x6980, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	{0x1002, 0x6981, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	{0x1002, 0x6985, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	{0x1002, 0x6986, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	{0x1002, 0x6987, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	{0x1002, 0x6995, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	{0x1002, 0x6997, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	{0x1002, 0x699F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS12},
	/* VEGAM */
	{0x1002, 0x694C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGAM},
	{0x1002, 0x694E, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGAM},
	{0x1002, 0x694F, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGAM},
	/* Vega 10 */
	{0x1002, 0x6860, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x6861, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x6862, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x6863, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x6864, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x6867, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x6868, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x6869, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x686a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x686b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x686c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x686d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x686e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x686f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	{0x1002, 0x687f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA10},
	/* Vega 12 */
	{0x1002, 0x69A0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA12},
	{0x1002, 0x69A1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA12},
	{0x1002, 0x69A2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA12},
	{0x1002, 0x69A3, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA12},
	{0x1002, 0x69AF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA12},
	/* Vega 20 */
	{0x1002, 0x66A0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA20|AMD_EXP_HW_SUPPORT},
	{0x1002, 0x66A1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA20|AMD_EXP_HW_SUPPORT},
	{0x1002, 0x66A2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA20|AMD_EXP_HW_SUPPORT},
	{0x1002, 0x66A3, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA20|AMD_EXP_HW_SUPPORT},
	{0x1002, 0x66A7, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA20|AMD_EXP_HW_SUPPORT},
	{0x1002, 0x66AF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_VEGA20|AMD_EXP_HW_SUPPORT},
	/* Raven */
	{0x1002, 0x15dd, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_RAVEN|AMD_IS_APU},

	{0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, pciidlist);

struct drm_driver amdgpu_kms_driver;

#ifdef notyet
static int amdgpu_kick_out_firmware_fb(struct pci_dev *pdev)
{
	struct apertures_struct *ap;
	bool primary = false;

	ap = alloc_apertures(1);
	if (!ap)
		return -ENOMEM;

	ap->ranges[0].base = pci_resource_start(pdev, 0);
	ap->ranges[0].size = pci_resource_len(pdev, 0);

#ifdef CONFIG_X86
	primary = pdev->resource[PCI_ROM_RESOURCE].flags & IORESOURCE_ROM_SHADOW;
#endif
	drm_fb_helper_remove_conflicting_framebuffers(ap, "amdgpudrmfb", primary);
	kfree(ap);

	return 0;
}
#endif


#ifdef notyet
static int amdgpu_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *ent)
{
	struct drm_device *dev;
	unsigned long flags = ent->driver_data;
	int ret, retry = 0;
	bool supports_atomic = false;

	if (!amdgpu_virtual_display &&
	    amdgpu_device_asic_has_dc_support(flags & AMD_ASIC_MASK))
		supports_atomic = true;

	if ((flags & AMD_EXP_HW_SUPPORT) && !amdgpu_exp_hw_support) {
		DRM_INFO("This hardware requires experimental hardware support.\n"
			 "See modparam exp_hw_support\n");
		return -ENODEV;
	}

	/*
	 * Initialize amdkfd before starting radeon. If it was not loaded yet,
	 * defer radeon probing
	 */
	ret = amdgpu_amdkfd_init();
	if (ret == -EPROBE_DEFER)
		return ret;

	/* Get rid of things like offb */
	ret = amdgpu_kick_out_firmware_fb(pdev);
	if (ret)
		return ret;

	/* warn the user if they mix atomic and non-atomic capable GPUs */
	if ((kms_driver.driver_features & DRIVER_ATOMIC) && !supports_atomic)
		DRM_ERROR("Mixing atomic and non-atomic capable GPUs!\n");
	/* support atomic early so the atomic debugfs stuff gets created */
	if (supports_atomic)
		kms_driver.driver_features |= DRIVER_ATOMIC;

	dev = drm_dev_alloc(&kms_driver, &pdev->dev);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ret = pci_enable_device(pdev);
	if (ret)
		goto err_free;

	dev->pdev = pdev;

	pci_set_drvdata(pdev, dev);

retry_init:
	ret = drm_dev_register(dev, ent->driver_data);
	if (ret == -EAGAIN && ++retry <= 3) {
		DRM_INFO("retry init %d\n", retry);
		/* Don't request EX mode too frequently which is attacking */
		drm_msleep(5000);
		goto retry_init;
	} else if (ret)
		goto err_pci;

	return 0;

err_pci:
	pci_disable_device(pdev);
err_free:
	drm_dev_put(dev);
	return ret;
}

static void
amdgpu_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_dev_unregister(dev);
	drm_dev_put(dev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

static void
amdgpu_pci_shutdown(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct amdgpu_device *adev = dev->dev_private;

	/* if we are running in a VM, make sure the device
	 * torn down properly on reboot/shutdown.
	 * unfortunately we can't detect certain
	 * hypervisors so just do this all the time.
	 */
	amdgpu_device_ip_suspend(adev);
}

static int amdgpu_pmops_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	return amdgpu_device_suspend(drm_dev, true, true);
}

static int amdgpu_pmops_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	/* GPU comes up enabled by the bios on resume */
	if (amdgpu_device_is_px(drm_dev)) {
		pm_runtime_disable(dev);
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
	}

	return amdgpu_device_resume(drm_dev, true, true);
}

static int amdgpu_pmops_freeze(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	return amdgpu_device_suspend(drm_dev, false, true);
}

static int amdgpu_pmops_thaw(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	return amdgpu_device_resume(drm_dev, false, true);
}

static int amdgpu_pmops_poweroff(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	return amdgpu_device_suspend(drm_dev, true, true);
}

static int amdgpu_pmops_restore(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	return amdgpu_device_resume(drm_dev, false, true);
}

static int amdgpu_pmops_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int ret;

	if (!amdgpu_device_is_px(drm_dev)) {
		pm_runtime_forbid(dev);
		return -EBUSY;
	}

	drm_dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;
	drm_kms_helper_poll_disable(drm_dev);

	ret = amdgpu_device_suspend(drm_dev, false, false);
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_ignore_hotplug(pdev);
	if (amdgpu_is_atpx_hybrid())
		pci_set_power_state(pdev, PCI_D3cold);
	else if (!amdgpu_has_atpx_dgpu_power_cntl())
		pci_set_power_state(pdev, PCI_D3hot);
	drm_dev->switch_power_state = DRM_SWITCH_POWER_DYNAMIC_OFF;

	return 0;
}

static int amdgpu_pmops_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int ret;

	if (!amdgpu_device_is_px(drm_dev))
		return -EINVAL;

	drm_dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;

	if (amdgpu_is_atpx_hybrid() ||
	    !amdgpu_has_atpx_dgpu_power_cntl())
		pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	ret = pci_enable_device(pdev);
	if (ret)
		return ret;
	pci_set_master(pdev);

	ret = amdgpu_device_resume(drm_dev, false, false);
	drm_kms_helper_poll_enable(drm_dev);
	drm_dev->switch_power_state = DRM_SWITCH_POWER_ON;
	return 0;
}

static int amdgpu_pmops_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct drm_crtc *crtc;

	if (!amdgpu_device_is_px(drm_dev)) {
		pm_runtime_forbid(dev);
		return -EBUSY;
	}

	list_for_each_entry(crtc, &drm_dev->mode_config.crtc_list, head) {
		if (crtc->enabled) {
			DRM_DEBUG_DRIVER("failing to power off - crtc active\n");
			return -EBUSY;
		}
	}

	pm_runtime_mark_last_busy(dev);
	pm_runtime_autosuspend(dev);
	/* we don't want the main rpm_idle to call suspend - we want to autosuspend */
	return 1;
}
#endif /* notyet */

#ifdef __linux__
long amdgpu_drm_ioctl(struct file *filp,
		      unsigned int cmd, unsigned long arg)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev;
	long ret;
	dev = file_priv->minor->dev;
	ret = pm_runtime_get_sync(dev->dev);
	if (ret < 0)
		return ret;

	ret = drm_ioctl(filp, cmd, arg);

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);
	return ret;
}

static const struct dev_pm_ops amdgpu_pm_ops = {
	.suspend = amdgpu_pmops_suspend,
	.resume = amdgpu_pmops_resume,
	.freeze = amdgpu_pmops_freeze,
	.thaw = amdgpu_pmops_thaw,
	.poweroff = amdgpu_pmops_poweroff,
	.restore = amdgpu_pmops_restore,
	.runtime_suspend = amdgpu_pmops_runtime_suspend,
	.runtime_resume = amdgpu_pmops_runtime_resume,
	.runtime_idle = amdgpu_pmops_runtime_idle,
};

static int amdgpu_flush(struct file *f, fl_owner_t id)
{
	struct drm_file *file_priv = f->private_data;
	struct amdgpu_fpriv *fpriv = file_priv->driver_priv;

	amdgpu_ctx_mgr_entity_flush(&fpriv->ctx_mgr);

	return 0;
}

static const struct file_operations amdgpu_driver_kms_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.flush = amdgpu_flush,
	.release = drm_release,
	.unlocked_ioctl = amdgpu_drm_ioctl,
	.mmap = amdgpu_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amdgpu_kms_compat_ioctl,
#endif
};
#endif /* __linux__ */

static bool
amdgpu_get_crtc_scanout_position(struct drm_device *dev, unsigned int pipe,
				 bool in_vblank_irq, int *vpos, int *hpos,
				 ktime_t *stime, ktime_t *etime,
				 const struct drm_display_mode *mode)
{
	return amdgpu_display_get_crtc_scanoutpos(dev, pipe, 0, vpos, hpos,
						  stime, etime, mode);
}

struct drm_driver amdgpu_kms_driver = {
	.driver_features =
	    DRIVER_USE_AGP |
	    DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED | DRIVER_GEM |
	    DRIVER_PRIME | DRIVER_RENDER | DRIVER_MODESET | DRIVER_SYNCOBJ,
#ifdef notyet
	.load = amdgpu_driver_load_kms,
#endif
	.open = amdgpu_driver_open_kms,
	.mmap = amdgpu_mmap,
	.postclose = amdgpu_driver_postclose_kms,
	.lastclose = amdgpu_driver_lastclose_kms,
#ifdef notyet
	.unload = amdgpu_driver_unload_kms,
#endif
	.get_vblank_counter = amdgpu_get_vblank_counter_kms,
	.enable_vblank = amdgpu_enable_vblank_kms,
	.disable_vblank = amdgpu_disable_vblank_kms,
	.get_vblank_timestamp = drm_calc_vbltimestamp_from_scanoutpos,
	.get_scanout_position = amdgpu_get_crtc_scanout_position,
#ifdef notyet
	.irq_handler = amdgpu_irq_handler,
#endif
	.ioctls = amdgpu_ioctls_kms,
	.gem_free_object_unlocked = amdgpu_gem_object_free,
	.gem_open_object = amdgpu_gem_object_open,
	.gem_close_object = amdgpu_gem_object_close,
	.gem_size = sizeof(struct amdgpu_bo),
	.dumb_create = amdgpu_mode_dumb_create,
	.dumb_map_offset = amdgpu_mode_dumb_mmap,
#ifdef __linux__
	.fops = &amdgpu_driver_kms_fops,
#endif

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = amdgpu_gem_prime_export,
	.gem_prime_import = amdgpu_gem_prime_import,
	.gem_prime_res_obj = amdgpu_gem_prime_res_obj,
#ifdef notyet
	.gem_prime_get_sg_table = amdgpu_gem_prime_get_sg_table,
	.gem_prime_import_sg_table = amdgpu_gem_prime_import_sg_table,
	.gem_prime_vmap = amdgpu_gem_prime_vmap,
	.gem_prime_vunmap = amdgpu_gem_prime_vunmap,
	.gem_prime_mmap = amdgpu_gem_prime_mmap,
#endif

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = KMS_DRIVER_MAJOR,
	.minor = KMS_DRIVER_MINOR,
	.patchlevel = KMS_DRIVER_PATCHLEVEL,
};

static struct drm_driver *driver;
#ifdef __linux__
static struct pci_driver *pdriver;

static struct pci_driver amdgpu_kms_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = amdgpu_pci_probe,
	.remove = amdgpu_pci_remove,
	.shutdown = amdgpu_pci_shutdown,
	.driver.pm = &amdgpu_pm_ops,
};

static int __init amdgpu_init(void)
{
	int r;

	if (vgacon_text_force()) {
		DRM_ERROR("VGACON disables amdgpu kernel modesetting.\n");
		return -EINVAL;
	}

	r = amdgpu_sync_init();
	if (r)
		goto error_sync;

	r = amdgpu_fence_slab_init();
	if (r)
		goto error_fence;

	DRM_INFO("amdgpu kernel modesetting enabled.\n");
	driver = &kms_driver;
	pdriver = &amdgpu_kms_pci_driver;
	driver->num_ioctls = amdgpu_max_kms_ioctl;
	amdgpu_register_atpx_handler();
	/* let modprobe override vga console setting */
	return pci_register_driver(pdriver);

error_fence:
	amdgpu_sync_fini();

error_sync:
	return r;
}

static void __exit amdgpu_exit(void)
{
	amdgpu_amdkfd_fini();
	pci_unregister_driver(pdriver);
	amdgpu_unregister_atpx_handler();
	amdgpu_sync_fini();
	amdgpu_fence_slab_fini();
}

module_init(amdgpu_init);
module_exit(amdgpu_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
#endif /* __linux__ */
