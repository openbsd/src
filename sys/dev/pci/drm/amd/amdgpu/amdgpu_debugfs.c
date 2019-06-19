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
 */

#include <linux/kthread.h>
#include <drm/drmP.h>
#include <linux/debugfs.h>
#include "amdgpu.h"

/**
 * amdgpu_debugfs_add_files - Add simple debugfs entries
 *
 * @adev:  Device to attach debugfs entries to
 * @files:  Array of function callbacks that respond to reads
 * @nfiles: Number of callbacks to register
 *
 */
int amdgpu_debugfs_add_files(struct amdgpu_device *adev,
			     const struct drm_info_list *files,
			     unsigned nfiles)
{
	unsigned i;

	for (i = 0; i < adev->debugfs_count; i++) {
		if (adev->debugfs[i].files == files) {
			/* Already registered */
			return 0;
		}
	}

	i = adev->debugfs_count + 1;
	if (i > AMDGPU_DEBUGFS_MAX_COMPONENTS) {
		DRM_ERROR("Reached maximum number of debugfs components.\n");
		DRM_ERROR("Report so we increase "
			  "AMDGPU_DEBUGFS_MAX_COMPONENTS.\n");
		return -EINVAL;
	}
	adev->debugfs[adev->debugfs_count].files = files;
	adev->debugfs[adev->debugfs_count].num_files = nfiles;
	adev->debugfs_count = i;
#if defined(CONFIG_DEBUG_FS)
	drm_debugfs_create_files(files, nfiles,
				 adev->ddev->primary->debugfs_root,
				 adev->ddev->primary);
#endif
	return 0;
}

#if defined(CONFIG_DEBUG_FS)

/**
 * amdgpu_debugfs_process_reg_op - Handle MMIO register reads/writes
 *
 * @read: True if reading
 * @f: open file handle
 * @buf: User buffer to write/read to
 * @size: Number of bytes to write/read
 * @pos:  Offset to seek to
 *
 * This debugfs entry has special meaning on the offset being sought.
 * Various bits have different meanings:
 *
 * Bit 62:  Indicates a GRBM bank switch is needed
 * Bit 61:  Indicates a SRBM bank switch is needed (implies bit 62 is
 * 			zero)
 * Bits 24..33: The SE or ME selector if needed
 * Bits 34..43: The SH (or SA) or PIPE selector if needed
 * Bits 44..53: The INSTANCE (or CU/WGP) or QUEUE selector if needed
 *
 * Bit 23:  Indicates that the PM power gating lock should be held
 * 			This is necessary to read registers that might be
 * 			unreliable during a power gating transistion.
 *
 * The lower bits are the BYTE offset of the register to read.  This
 * allows reading multiple registers in a single call and having
 * the returned size reflect that.
 */
static int  amdgpu_debugfs_process_reg_op(bool read, struct file *f,
		char __user *buf, size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;
	bool pm_pg_lock, use_bank, use_ring;
	unsigned instance_bank, sh_bank, se_bank, me, pipe, queue;

	pm_pg_lock = use_bank = use_ring = false;
	instance_bank = sh_bank = se_bank = me = pipe = queue = 0;

	if (size & 0x3 || *pos & 0x3 ||
			((*pos & (1ULL << 62)) && (*pos & (1ULL << 61))))
		return -EINVAL;

	/* are we reading registers for which a PG lock is necessary? */
	pm_pg_lock = (*pos >> 23) & 1;

	if (*pos & (1ULL << 62)) {
		se_bank = (*pos & GENMASK_ULL(33, 24)) >> 24;
		sh_bank = (*pos & GENMASK_ULL(43, 34)) >> 34;
		instance_bank = (*pos & GENMASK_ULL(53, 44)) >> 44;

		if (se_bank == 0x3FF)
			se_bank = 0xFFFFFFFF;
		if (sh_bank == 0x3FF)
			sh_bank = 0xFFFFFFFF;
		if (instance_bank == 0x3FF)
			instance_bank = 0xFFFFFFFF;
		use_bank = 1;
	} else if (*pos & (1ULL << 61)) {

		me = (*pos & GENMASK_ULL(33, 24)) >> 24;
		pipe = (*pos & GENMASK_ULL(43, 34)) >> 34;
		queue = (*pos & GENMASK_ULL(53, 44)) >> 44;

		use_ring = 1;
	} else {
		use_bank = use_ring = 0;
	}

	*pos &= (1UL << 22) - 1;

	if (use_bank) {
		if ((sh_bank != 0xFFFFFFFF && sh_bank >= adev->gfx.config.max_sh_per_se) ||
		    (se_bank != 0xFFFFFFFF && se_bank >= adev->gfx.config.max_shader_engines))
			return -EINVAL;
		mutex_lock(&adev->grbm_idx_mutex);
		amdgpu_gfx_select_se_sh(adev, se_bank,
					sh_bank, instance_bank);
	} else if (use_ring) {
		mutex_lock(&adev->srbm_mutex);
		amdgpu_gfx_select_me_pipe_q(adev, me, pipe, queue);
	}

	if (pm_pg_lock)
		mutex_lock(&adev->pm.mutex);

	while (size) {
		uint32_t value;

		if (*pos > adev->rmmio_size)
			goto end;

		if (read) {
			value = RREG32(*pos >> 2);
			r = put_user(value, (uint32_t *)buf);
		} else {
			r = get_user(value, (uint32_t *)buf);
			if (!r)
				WREG32(*pos >> 2, value);
		}
		if (r) {
			result = r;
			goto end;
		}

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

end:
	if (use_bank) {
		amdgpu_gfx_select_se_sh(adev, 0xffffffff, 0xffffffff, 0xffffffff);
		mutex_unlock(&adev->grbm_idx_mutex);
	} else if (use_ring) {
		amdgpu_gfx_select_me_pipe_q(adev, 0, 0, 0);
		mutex_unlock(&adev->srbm_mutex);
	}

	if (pm_pg_lock)
		mutex_unlock(&adev->pm.mutex);

	return result;
}

/**
 * amdgpu_debugfs_regs_read - Callback for reading MMIO registers
 */
static ssize_t amdgpu_debugfs_regs_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	return amdgpu_debugfs_process_reg_op(true, f, buf, size, pos);
}

/**
 * amdgpu_debugfs_regs_write - Callback for writing MMIO registers
 */
static ssize_t amdgpu_debugfs_regs_write(struct file *f, const char __user *buf,
					 size_t size, loff_t *pos)
{
	return amdgpu_debugfs_process_reg_op(false, f, (char __user *)buf, size, pos);
}


/**
 * amdgpu_debugfs_regs_pcie_read - Read from a PCIE register
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to read.  This
 * allows reading multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_pcie_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	while (size) {
		uint32_t value;

		value = RREG32_PCIE(*pos >> 2);
		r = put_user(value, (uint32_t *)buf);
		if (r)
			return r;

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	return result;
}

/**
 * amdgpu_debugfs_regs_pcie_write - Write to a PCIE register
 *
 * @f: open file handle
 * @buf: User buffer to write data from
 * @size: Number of bytes to write
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to write.  This
 * allows writing multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_pcie_write(struct file *f, const char __user *buf,
					 size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	while (size) {
		uint32_t value;

		r = get_user(value, (uint32_t *)buf);
		if (r)
			return r;

		WREG32_PCIE(*pos >> 2, value);

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	return result;
}

/**
 * amdgpu_debugfs_regs_didt_read - Read from a DIDT register
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to read.  This
 * allows reading multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_didt_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	while (size) {
		uint32_t value;

		value = RREG32_DIDT(*pos >> 2);
		r = put_user(value, (uint32_t *)buf);
		if (r)
			return r;

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	return result;
}

/**
 * amdgpu_debugfs_regs_didt_write - Write to a DIDT register
 *
 * @f: open file handle
 * @buf: User buffer to write data from
 * @size: Number of bytes to write
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to write.  This
 * allows writing multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_didt_write(struct file *f, const char __user *buf,
					 size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	while (size) {
		uint32_t value;

		r = get_user(value, (uint32_t *)buf);
		if (r)
			return r;

		WREG32_DIDT(*pos >> 2, value);

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	return result;
}

/**
 * amdgpu_debugfs_regs_smc_read - Read from a SMC register
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to read.  This
 * allows reading multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_smc_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	while (size) {
		uint32_t value;

		value = RREG32_SMC(*pos);
		r = put_user(value, (uint32_t *)buf);
		if (r)
			return r;

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	return result;
}

/**
 * amdgpu_debugfs_regs_smc_write - Write to a SMC register
 *
 * @f: open file handle
 * @buf: User buffer to write data from
 * @size: Number of bytes to write
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to write.  This
 * allows writing multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_smc_write(struct file *f, const char __user *buf,
					 size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	while (size) {
		uint32_t value;

		r = get_user(value, (uint32_t *)buf);
		if (r)
			return r;

		WREG32_SMC(*pos, value);

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	return result;
}

/**
 * amdgpu_debugfs_gca_config_read - Read from gfx config data
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * This file is used to access configuration data in a somewhat
 * stable fashion.  The format is a series of DWORDs with the first
 * indicating which revision it is.  New content is appended to the
 * end so that older software can still read the data.
 */

static ssize_t amdgpu_debugfs_gca_config_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;
	uint32_t *config, no_regs = 0;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	config = kmalloc_array(256, sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	/* version, increment each time something is added */
	config[no_regs++] = 3;
	config[no_regs++] = adev->gfx.config.max_shader_engines;
	config[no_regs++] = adev->gfx.config.max_tile_pipes;
	config[no_regs++] = adev->gfx.config.max_cu_per_sh;
	config[no_regs++] = adev->gfx.config.max_sh_per_se;
	config[no_regs++] = adev->gfx.config.max_backends_per_se;
	config[no_regs++] = adev->gfx.config.max_texture_channel_caches;
	config[no_regs++] = adev->gfx.config.max_gprs;
	config[no_regs++] = adev->gfx.config.max_gs_threads;
	config[no_regs++] = adev->gfx.config.max_hw_contexts;
	config[no_regs++] = adev->gfx.config.sc_prim_fifo_size_frontend;
	config[no_regs++] = adev->gfx.config.sc_prim_fifo_size_backend;
	config[no_regs++] = adev->gfx.config.sc_hiz_tile_fifo_size;
	config[no_regs++] = adev->gfx.config.sc_earlyz_tile_fifo_size;
	config[no_regs++] = adev->gfx.config.num_tile_pipes;
	config[no_regs++] = adev->gfx.config.backend_enable_mask;
	config[no_regs++] = adev->gfx.config.mem_max_burst_length_bytes;
	config[no_regs++] = adev->gfx.config.mem_row_size_in_kb;
	config[no_regs++] = adev->gfx.config.shader_engine_tile_size;
	config[no_regs++] = adev->gfx.config.num_gpus;
	config[no_regs++] = adev->gfx.config.multi_gpu_tile_size;
	config[no_regs++] = adev->gfx.config.mc_arb_ramcfg;
	config[no_regs++] = adev->gfx.config.gb_addr_config;
	config[no_regs++] = adev->gfx.config.num_rbs;

	/* rev==1 */
	config[no_regs++] = adev->rev_id;
	config[no_regs++] = adev->pg_flags;
	config[no_regs++] = adev->cg_flags;

	/* rev==2 */
	config[no_regs++] = adev->family;
	config[no_regs++] = adev->external_rev_id;

	/* rev==3 */
	config[no_regs++] = adev->pdev->device;
	config[no_regs++] = adev->pdev->revision;
	config[no_regs++] = adev->pdev->subsystem_device;
	config[no_regs++] = adev->pdev->subsystem_vendor;

	while (size && (*pos < no_regs * 4)) {
		uint32_t value;

		value = config[*pos >> 2];
		r = put_user(value, (uint32_t *)buf);
		if (r) {
			kfree(config);
			return r;
		}

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	kfree(config);
	return result;
}

/**
 * amdgpu_debugfs_sensor_read - Read from the powerplay sensors
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The offset is treated as the BYTE address of one of the sensors
 * enumerated in amd/include/kgd_pp_interface.h under the
 * 'amd_pp_sensors' enumeration.  For instance to read the UVD VCLK
 * you would use the offset 3 * 4 = 12.
 */
static ssize_t amdgpu_debugfs_sensor_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	int idx, x, outsize, r, valuesize;
	uint32_t values[16];

	if (size & 3 || *pos & 0x3)
		return -EINVAL;

	if (!adev->pm.dpm_enabled)
		return -EINVAL;

	/* convert offset to sensor number */
	idx = *pos >> 2;

	valuesize = sizeof(values);
	if (adev->powerplay.pp_funcs && adev->powerplay.pp_funcs->read_sensor)
		r = amdgpu_dpm_read_sensor(adev, idx, &values[0], &valuesize);
	else
		return -EINVAL;

	if (size > valuesize)
		return -EINVAL;

	outsize = 0;
	x = 0;
	if (!r) {
		while (size) {
			r = put_user(values[x++], (int32_t *)buf);
			buf += 4;
			size -= 4;
			outsize += 4;
		}
	}

	return !r ? outsize : r;
}

/** amdgpu_debugfs_wave_read - Read WAVE STATUS data
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The offset being sought changes which wave that the status data
 * will be returned for.  The bits are used as follows:
 *
 * Bits 0..6: 	Byte offset into data
 * Bits 7..14:	SE selector
 * Bits 15..22:	SH/SA selector
 * Bits 23..30: CU/{WGP+SIMD} selector
 * Bits 31..36: WAVE ID selector
 * Bits 37..44: SIMD ID selector
 *
 * The returned data begins with one DWORD of version information
 * Followed by WAVE STATUS registers relevant to the GFX IP version
 * being used.  See gfx_v8_0_read_wave_data() for an example output.
 */
static ssize_t amdgpu_debugfs_wave_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = f->f_inode->i_private;
	int r, x;
	ssize_t result=0;
	uint32_t offset, se, sh, cu, wave, simd, data[32];

	if (size & 3 || *pos & 3)
		return -EINVAL;

	/* decode offset */
	offset = (*pos & GENMASK_ULL(6, 0));
	se = (*pos & GENMASK_ULL(14, 7)) >> 7;
	sh = (*pos & GENMASK_ULL(22, 15)) >> 15;
	cu = (*pos & GENMASK_ULL(30, 23)) >> 23;
	wave = (*pos & GENMASK_ULL(36, 31)) >> 31;
	simd = (*pos & GENMASK_ULL(44, 37)) >> 37;

	/* switch to the specific se/sh/cu */
	mutex_lock(&adev->grbm_idx_mutex);
	amdgpu_gfx_select_se_sh(adev, se, sh, cu);

	x = 0;
	if (adev->gfx.funcs->read_wave_data)
		adev->gfx.funcs->read_wave_data(adev, simd, wave, data, &x);

	amdgpu_gfx_select_se_sh(adev, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	mutex_unlock(&adev->grbm_idx_mutex);

	if (!x)
		return -EINVAL;

	while (size && (offset < x * 4)) {
		uint32_t value;

		value = data[offset >> 2];
		r = put_user(value, (uint32_t *)buf);
		if (r)
			return r;

		result += 4;
		buf += 4;
		offset += 4;
		size -= 4;
	}

	return result;
}

/** amdgpu_debugfs_gpr_read - Read wave gprs
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The offset being sought changes which wave that the status data
 * will be returned for.  The bits are used as follows:
 *
 * Bits 0..11:	Byte offset into data
 * Bits 12..19:	SE selector
 * Bits 20..27:	SH/SA selector
 * Bits 28..35: CU/{WGP+SIMD} selector
 * Bits 36..43: WAVE ID selector
 * Bits 37..44: SIMD ID selector
 * Bits 52..59: Thread selector
 * Bits 60..61: Bank selector (VGPR=0,SGPR=1)
 *
 * The return data comes from the SGPR or VGPR register bank for
 * the selected operational unit.
 */
static ssize_t amdgpu_debugfs_gpr_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = f->f_inode->i_private;
	int r;
	ssize_t result = 0;
	uint32_t offset, se, sh, cu, wave, simd, thread, bank, *data;

	if (size & 3 || *pos & 3)
		return -EINVAL;

	/* decode offset */
	offset = *pos & GENMASK_ULL(11, 0);
	se = (*pos & GENMASK_ULL(19, 12)) >> 12;
	sh = (*pos & GENMASK_ULL(27, 20)) >> 20;
	cu = (*pos & GENMASK_ULL(35, 28)) >> 28;
	wave = (*pos & GENMASK_ULL(43, 36)) >> 36;
	simd = (*pos & GENMASK_ULL(51, 44)) >> 44;
	thread = (*pos & GENMASK_ULL(59, 52)) >> 52;
	bank = (*pos & GENMASK_ULL(61, 60)) >> 60;

	data = kmalloc_array(1024, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* switch to the specific se/sh/cu */
	mutex_lock(&adev->grbm_idx_mutex);
	amdgpu_gfx_select_se_sh(adev, se, sh, cu);

	if (bank == 0) {
		if (adev->gfx.funcs->read_wave_vgprs)
			adev->gfx.funcs->read_wave_vgprs(adev, simd, wave, thread, offset, size>>2, data);
	} else {
		if (adev->gfx.funcs->read_wave_sgprs)
			adev->gfx.funcs->read_wave_sgprs(adev, simd, wave, offset, size>>2, data);
	}

	amdgpu_gfx_select_se_sh(adev, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	mutex_unlock(&adev->grbm_idx_mutex);

	while (size) {
		uint32_t value;

		value = data[offset++];
		r = put_user(value, (uint32_t *)buf);
		if (r) {
			result = r;
			goto err;
		}

		result += 4;
		buf += 4;
		size -= 4;
	}

err:
	kfree(data);
	return result;
}

static const struct file_operations amdgpu_debugfs_regs_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_regs_read,
	.write = amdgpu_debugfs_regs_write,
	.llseek = default_llseek
};
static const struct file_operations amdgpu_debugfs_regs_didt_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_regs_didt_read,
	.write = amdgpu_debugfs_regs_didt_write,
	.llseek = default_llseek
};
static const struct file_operations amdgpu_debugfs_regs_pcie_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_regs_pcie_read,
	.write = amdgpu_debugfs_regs_pcie_write,
	.llseek = default_llseek
};
static const struct file_operations amdgpu_debugfs_regs_smc_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_regs_smc_read,
	.write = amdgpu_debugfs_regs_smc_write,
	.llseek = default_llseek
};

static const struct file_operations amdgpu_debugfs_gca_config_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_gca_config_read,
	.llseek = default_llseek
};

static const struct file_operations amdgpu_debugfs_sensors_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_sensor_read,
	.llseek = default_llseek
};

static const struct file_operations amdgpu_debugfs_wave_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_wave_read,
	.llseek = default_llseek
};
static const struct file_operations amdgpu_debugfs_gpr_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_gpr_read,
	.llseek = default_llseek
};

static const struct file_operations *debugfs_regs[] = {
	&amdgpu_debugfs_regs_fops,
	&amdgpu_debugfs_regs_didt_fops,
	&amdgpu_debugfs_regs_pcie_fops,
	&amdgpu_debugfs_regs_smc_fops,
	&amdgpu_debugfs_gca_config_fops,
	&amdgpu_debugfs_sensors_fops,
	&amdgpu_debugfs_wave_fops,
	&amdgpu_debugfs_gpr_fops,
};

static const char *debugfs_regs_names[] = {
	"amdgpu_regs",
	"amdgpu_regs_didt",
	"amdgpu_regs_pcie",
	"amdgpu_regs_smc",
	"amdgpu_gca_config",
	"amdgpu_sensors",
	"amdgpu_wave",
	"amdgpu_gpr",
};

/**
 * amdgpu_debugfs_regs_init -	Initialize debugfs entries that provide
 * 								register access.
 *
 * @adev: The device to attach the debugfs entries to
 */
int amdgpu_debugfs_regs_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev->ddev->primary;
	struct dentry *ent, *root = minor->debugfs_root;
	unsigned i, j;

	for (i = 0; i < ARRAY_SIZE(debugfs_regs); i++) {
		ent = debugfs_create_file(debugfs_regs_names[i],
					  S_IFREG | S_IRUGO, root,
					  adev, debugfs_regs[i]);
		if (IS_ERR(ent)) {
			for (j = 0; j < i; j++) {
				debugfs_remove(adev->debugfs_regs[i]);
				adev->debugfs_regs[i] = NULL;
			}
			return PTR_ERR(ent);
		}

		if (!i)
			i_size_write(ent->d_inode, adev->rmmio_size);
		adev->debugfs_regs[i] = ent;
	}

	return 0;
}

void amdgpu_debugfs_regs_cleanup(struct amdgpu_device *adev)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(debugfs_regs); i++) {
		if (adev->debugfs_regs[i]) {
			debugfs_remove(adev->debugfs_regs[i]);
			adev->debugfs_regs[i] = NULL;
		}
	}
}

static int amdgpu_debugfs_test_ib(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = dev->dev_private;
	int r = 0, i;

	/* hold on the scheduler */
	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;
		kthread_park(ring->sched.thread);
	}

	seq_printf(m, "run ib test:\n");
	r = amdgpu_ib_ring_tests(adev);
	if (r)
		seq_printf(m, "ib ring tests failed (%d).\n", r);
	else
		seq_printf(m, "ib ring tests passed.\n");

	/* go on the scheduler */
	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;
		kthread_unpark(ring->sched.thread);
	}

	return 0;
}

static int amdgpu_debugfs_get_vbios_dump(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = dev->dev_private;

	seq_write(m, adev->bios, adev->bios_size);
	return 0;
}

static int amdgpu_debugfs_evict_vram(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = dev->dev_private;

	seq_printf(m, "(%d)\n", amdgpu_bo_evict_vram(adev));
	return 0;
}

static int amdgpu_debugfs_evict_gtt(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = dev->dev_private;

	seq_printf(m, "(%d)\n", ttm_bo_evict_mm(&adev->mman.bdev, TTM_PL_TT));
	return 0;
}

static const struct drm_info_list amdgpu_debugfs_list[] = {
	{"amdgpu_vbios", amdgpu_debugfs_get_vbios_dump},
	{"amdgpu_test_ib", &amdgpu_debugfs_test_ib},
	{"amdgpu_evict_vram", &amdgpu_debugfs_evict_vram},
	{"amdgpu_evict_gtt", &amdgpu_debugfs_evict_gtt},
};

int amdgpu_debugfs_init(struct amdgpu_device *adev)
{
	return amdgpu_debugfs_add_files(adev, amdgpu_debugfs_list,
					ARRAY_SIZE(amdgpu_debugfs_list));
}

#else
int amdgpu_debugfs_init(struct amdgpu_device *adev)
{
	return 0;
}
int amdgpu_debugfs_regs_init(struct amdgpu_device *adev)
{
	return 0;
}
void amdgpu_debugfs_regs_cleanup(struct amdgpu_device *adev) { }
#endif
