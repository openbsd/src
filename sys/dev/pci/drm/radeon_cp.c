/* radeon_cp.c -- CP support for Radeon -*- linux-c -*- */
/*
 * Copyright 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
 * Copyright 2007-2009 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Kevin E. Martin <martin@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *    Dave Airlie <airlied@redhat.com>
 *    Alex Deucher <alexander.deucher@amd.com>
 */

#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"
#include "r300_reg.h"

#include "radeon_microcode.h"
#include "r600_microcode.h"
#define RADEON_FIFO_DEBUG	0

int	radeon_do_cleanup_cp(struct drm_device *);
void	radeon_do_cp_start(drm_radeon_private_t *);
void	radeon_do_cp_reset(drm_radeon_private_t *);
void	radeon_do_cp_stop(drm_radeon_private_t *);
int	radeon_do_engine_reset(struct drm_device *);
void	radeon_cp_init_ring_buffer(struct drm_device *, drm_radeon_private_t *);
int	radeon_do_init_cp(struct drm_device *, drm_radeon_init_t *);
void	radeon_cp_load_microcode(drm_radeon_private_t *);
int	radeon_cp_get_buffers(struct drm_device *dev, struct drm_file *,
	    struct drm_dma *);

void	r600gart_add_entry(struct drm_ati_pcigart_info *, bus_size_t,
	    bus_addr_t);

u32	R500_READ_MCIND(drm_radeon_private_t *, int );
u32	RS480_READ_MCIND(drm_radeon_private_t *, int);
u32	RS690_READ_MCIND(drm_radeon_private_t *, int);
u32	RS600_READ_MCIND(drm_radeon_private_t *, int);
u32	IGP_READ_MCIND(drm_radeon_private_t *, int);
int	RADEON_READ_PLL(struct drm_device * , int);
u32	RADEON_READ_PCIE(drm_radeon_private_t *, int);

int	radeon_do_pixcache_flush(drm_radeon_private_t *);
int	radeon_do_wait_for_fifo(drm_radeon_private_t *, int);
int	radeon_do_wait_for_idle(drm_radeon_private_t *);
int	r600_do_wait_for_fifo(drm_radeon_private_t *, int);
int	r600_do_wait_for_idle(drm_radeon_private_t *);
void	radeon_init_pipes(drm_radeon_private_t *);
void	radeon_test_writeback(drm_radeon_private_t *);
void	radeon_set_igpgart(drm_radeon_private_t *, int);
void	rs600_set_igpgart(drm_radeon_private_t *, int);
void	radeon_set_pciegart(drm_radeon_private_t *, int);
void	radeon_set_pcigart(drm_radeon_private_t *, int);
int	radeondrm_setup_pcigart(struct drm_radeon_private *);
int	radeon_setup_pcigart_surface(drm_radeon_private_t *dev_priv);

# define ATI_PCIGART_PAGE_SIZE		4096	/**< PCI GART page size */
# define ATI_PCIGART_PAGE_MASK		(~(ATI_PCIGART_PAGE_SIZE-1))

#define R600_PTE_VALID     (1 << 0)
#define R600_PTE_SYSTEM    (1 << 1)
#define R600_PTE_SNOOPED   (1 << 2)
#define R600_PTE_READABLE  (1 << 5)
#define R600_PTE_WRITEABLE (1 << 6)

/* MAX values used for gfx init */
#define R6XX_MAX_SH_GPRS           256
#define R6XX_MAX_TEMP_GPRS         16
#define R6XX_MAX_SH_THREADS        256
#define R6XX_MAX_SH_STACK_ENTRIES  4096
#define R6XX_MAX_BACKENDS          8
#define R6XX_MAX_BACKENDS_MASK     0xff
#define R6XX_MAX_SIMDS             8
#define R6XX_MAX_SIMDS_MASK        0xff
#define R6XX_MAX_PIPES             8
#define R6XX_MAX_PIPES_MASK        0xff

#define R7XX_MAX_SH_GPRS           256
#define R7XX_MAX_TEMP_GPRS         16
#define R7XX_MAX_SH_THREADS        256
#define R7XX_MAX_SH_STACK_ENTRIES  4096
#define R7XX_MAX_BACKENDS          8
#define R7XX_MAX_BACKENDS_MASK     0xff
#define R7XX_MAX_SIMDS             16
#define R7XX_MAX_SIMDS_MASK        0xffff
#define R7XX_MAX_PIPES             8
#define R7XX_MAX_PIPES_MASK        0xff


u32
R500_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	u32 ret;
	RADEON_WRITE(R520_MC_IND_INDEX, 0x7f0000 | (addr & 0xff));
	ret = RADEON_READ(R520_MC_IND_DATA);
	RADEON_WRITE(R520_MC_IND_INDEX, 0);
	return ret;
}

u32
RS480_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	u32 ret;
	RADEON_WRITE(RS480_NB_MC_INDEX, addr & 0xff);
	ret = RADEON_READ(RS480_NB_MC_DATA);
	RADEON_WRITE(RS480_NB_MC_INDEX, 0xff);
	return ret;
}

u32
RS690_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	u32 ret;
	RADEON_WRITE(RS690_MC_INDEX, (addr & RS690_MC_INDEX_MASK));
	ret = RADEON_READ(RS690_MC_DATA);
	RADEON_WRITE(RS690_MC_INDEX, RS690_MC_INDEX_MASK);
	return ret;
}

u32
RS600_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	u32 ret;
	RADEON_WRITE(RS600_MC_INDEX, ((addr & RS600_MC_ADDR_MASK) |
				      RS600_MC_IND_CITF_ARB0));
	ret = RADEON_READ(RS600_MC_DATA);
	return ret;
}
u32
IGP_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740))
		return RS690_READ_MCIND(dev_priv, addr);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600)
		return RS600_READ_MCIND(dev_priv, addr);
	else
		return RS480_READ_MCIND(dev_priv, addr);
}

u32
radeon_read_fb_location(drm_radeon_private_t *dev_priv)
{

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
		return RADEON_READ(R700_MC_VM_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		return RADEON_READ(R600_MC_VM_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515)
		return R500_READ_MCIND(dev_priv, RV515_MC_FB_LOCATION);
	else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
		((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740))
		return RS690_READ_MCIND(dev_priv, RS690_MC_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600)
		return RS600_READ_MCIND(dev_priv, RS600_MC_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515)
		return R500_READ_MCIND(dev_priv, R520_MC_FB_LOCATION);
	else
		return RADEON_READ(RADEON_MC_FB_LOCATION);
}

void
radeon_write_fb_location(drm_radeon_private_t *dev_priv, u32 fb_loc)
{
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
		RADEON_WRITE(R700_MC_VM_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		RADEON_WRITE(R600_MC_VM_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515)
		R500_WRITE_MCIND(RV515_MC_FB_LOCATION, fb_loc);
	else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
		 ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740))
		RS690_WRITE_MCIND(RS690_MC_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600)
		RS600_WRITE_MCIND(RS600_MC_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515)
		R500_WRITE_MCIND(R520_MC_FB_LOCATION, fb_loc);
	else
		RADEON_WRITE(RADEON_MC_FB_LOCATION, fb_loc);
}

void
radeon_write_agp_location(drm_radeon_private_t *dev_priv, u32 agp_loc)
{
	/*R6xx/R7xx: AGP_TOP and BOT are actually 18 bits each */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770) {
		RADEON_WRITE(R700_MC_VM_AGP_BOT, agp_loc & 0xffff); /* FIX ME */
		RADEON_WRITE(R700_MC_VM_AGP_TOP, (agp_loc >> 16) & 0xffff);
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		RADEON_WRITE(R600_MC_VM_AGP_BOT, agp_loc & 0xffff); /* FIX ME */
		RADEON_WRITE(R600_MC_VM_AGP_TOP, (agp_loc >> 16) & 0xffff);
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515)
		R500_WRITE_MCIND(RV515_MC_AGP_LOCATION, agp_loc);
	else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
		 ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740))
		RS690_WRITE_MCIND(RS690_MC_AGP_LOCATION, agp_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600)
		RS600_WRITE_MCIND(RS600_MC_AGP_LOCATION, agp_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515)
		R500_WRITE_MCIND(R520_MC_AGP_LOCATION, agp_loc);
	else
		RADEON_WRITE(RADEON_MC_AGP_LOCATION, agp_loc);
}

void
radeon_write_agp_base(drm_radeon_private_t *dev_priv, u64 agp_base)
{
	u32 agp_base_hi = upper_32_bits(agp_base);
	u32 agp_base_lo = agp_base & 0xffffffff;
	u32 r6xx_agp_base = (agp_base >> 22) & 0x3ffff;

	/* R6xx/R7xx must be aligned to a 4MB boundry */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
		RADEON_WRITE(R700_MC_VM_AGP_BASE, r6xx_agp_base);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		RADEON_WRITE(R600_MC_VM_AGP_BASE, r6xx_agp_base);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515) {
		R500_WRITE_MCIND(RV515_MC_AGP_BASE, agp_base_lo);
		R500_WRITE_MCIND(RV515_MC_AGP_BASE_2, agp_base_hi);
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
		 ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740)) {
		RS690_WRITE_MCIND(RS690_MC_AGP_BASE, agp_base_lo);
		RS690_WRITE_MCIND(RS690_MC_AGP_BASE_2, agp_base_hi);
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600) {
		RS600_WRITE_MCIND(RS600_AGP_BASE, agp_base_lo);
		RS600_WRITE_MCIND(RS600_AGP_BASE_2, agp_base_hi);
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515) {
		R500_WRITE_MCIND(R520_MC_AGP_BASE, agp_base_lo);
		R500_WRITE_MCIND(R520_MC_AGP_BASE_2, agp_base_hi);
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS400) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS480)) {
		RADEON_WRITE(RADEON_AGP_BASE, agp_base_lo);
		RADEON_WRITE(RS480_AGP_BASE_2, agp_base_hi);
	} else {
		RADEON_WRITE(RADEON_AGP_BASE, agp_base_lo);
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R200)
			RADEON_WRITE(RADEON_AGP_BASE_2, agp_base_hi);
	}
}

void
radeon_enable_bm(struct drm_radeon_private *dev_priv)
{
	u32 tmp;
	/* Turn on bus mastering */
	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740)) {
		/* rs600/rs690/rs740 */
		tmp = RADEON_READ(RADEON_BUS_CNTL) & ~RS600_BUS_MASTER_DIS;
		RADEON_WRITE(RADEON_BUS_CNTL, tmp);
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV350) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R420) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS400) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS480)) {
		/* r1xx, r2xx, r300, r(v)350, r420/r481, rs400/rs480 */
		tmp = RADEON_READ(RADEON_BUS_CNTL) & ~RADEON_BUS_MASTER_DIS;
		RADEON_WRITE(RADEON_BUS_CNTL, tmp);
	} /* PCIE cards appears to not need this */
}

int
RADEON_READ_PLL(struct drm_device *dev, int addr)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	RADEON_WRITE8(RADEON_CLOCK_CNTL_INDEX, addr & 0x1f);
	return RADEON_READ(RADEON_CLOCK_CNTL_DATA);
}

u32
RADEON_READ_PCIE(drm_radeon_private_t *dev_priv, int addr)
{
	RADEON_WRITE8(RADEON_PCIE_INDEX, addr & 0xff);
	return RADEON_READ(RADEON_PCIE_DATA);
}

/*
 * Some R600/R700 setup functions.
 */
static u32 r600_get_tile_pipe_to_backend_map(u32 num_tile_pipes,
					     u32 num_backends,
					     u32 backend_disable_mask)
{
	u32 backend_map = 0;
	u32 enabled_backends_mask;
	u32 enabled_backends_count;
	u32 cur_pipe;
	u32 swizzle_pipe[R6XX_MAX_PIPES];
	u32 cur_backend;
	u32 i;

	if (num_tile_pipes > R6XX_MAX_PIPES)
		num_tile_pipes = R6XX_MAX_PIPES;
	if (num_tile_pipes < 1)
		num_tile_pipes = 1;
	if (num_backends > R6XX_MAX_BACKENDS)
		num_backends = R6XX_MAX_BACKENDS;
	if (num_backends < 1)
		num_backends = 1;

	enabled_backends_mask = 0;
	enabled_backends_count = 0;
	for (i = 0; i < R6XX_MAX_BACKENDS; ++i) {
		if (((backend_disable_mask >> i) & 1) == 0) {
			enabled_backends_mask |= (1 << i);
			++enabled_backends_count;
		}
		if (enabled_backends_count == num_backends)
			break;
	}

	if (enabled_backends_count == 0) {
		enabled_backends_mask = 1;
		enabled_backends_count = 1;
	}

	if (enabled_backends_count != num_backends)
		num_backends = enabled_backends_count;

	memset((uint8_t *)&swizzle_pipe[0], 0, sizeof(u32) * R6XX_MAX_PIPES);
	switch (num_tile_pipes) {
	case 1:
		swizzle_pipe[0] = 0;
		break;
	case 2:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 1;
		break;
	case 3:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 1;
		swizzle_pipe[2] = 2;
		break;
	case 4:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 1;
		swizzle_pipe[2] = 2;
		swizzle_pipe[3] = 3;
		break;
	case 5:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 1;
		swizzle_pipe[2] = 2;
		swizzle_pipe[3] = 3;
		swizzle_pipe[4] = 4;
		break;
	case 6:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 4;
		swizzle_pipe[3] = 5;
		swizzle_pipe[4] = 1;
		swizzle_pipe[5] = 3;
		break;
	case 7:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 4;
		swizzle_pipe[3] = 6;
		swizzle_pipe[4] = 1;
		swizzle_pipe[5] = 3;
		swizzle_pipe[6] = 5;
		break;
	case 8:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 4;
		swizzle_pipe[3] = 6;
		swizzle_pipe[4] = 1;
		swizzle_pipe[5] = 3;
		swizzle_pipe[6] = 5;
		swizzle_pipe[7] = 7;
		break;
	}

	cur_backend = 0;
	for (cur_pipe = 0; cur_pipe < num_tile_pipes; ++cur_pipe) {
		while (((1 << cur_backend) & enabled_backends_mask) == 0)
			cur_backend = (cur_backend + 1) % R6XX_MAX_BACKENDS;

		backend_map |= (u32)(((cur_backend & 3) << (swizzle_pipe[cur_pipe] * 2)));

		cur_backend = (cur_backend + 1) % R6XX_MAX_BACKENDS;
	}

	return backend_map;
}

static int r600_count_pipe_bits(uint32_t val)
{
	int i, ret = 0;
	for (i = 0; i < 32; i++) {
		ret += val & 1;
		val >>= 1;
	}
	return ret;
}

static void r600_gfx_init(struct drm_device *dev,
			  drm_radeon_private_t *dev_priv)
{
	int i, j, num_qd_pipes;
	u32 sx_debug_1;
	u32 tc_cntl;
	u32 arb_pop;
	u32 num_gs_verts_per_thread;
	u32 vgt_gs_per_es;
	u32 gs_prim_buffer_depth = 0;
	u32 sq_ms_fifo_sizes;
	u32 sq_config;
	u32 sq_gpr_resource_mgmt_1 = 0;
	u32 sq_gpr_resource_mgmt_2 = 0;
	u32 sq_thread_resource_mgmt = 0;
	u32 sq_stack_resource_mgmt_1 = 0;
	u32 sq_stack_resource_mgmt_2 = 0;
	u32 hdp_host_path_cntl;
	u32 backend_map;
	u32 gb_tiling_config = 0;
	u32 cc_rb_backend_disable = 0;
	u32 cc_gc_shader_pipe_config = 0;
	u32 ramcfg;

	/* setup chip specs */
	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_R600:
		dev_priv->r600_max_pipes = 4;
		dev_priv->r600_max_tile_pipes = 8;
		dev_priv->r600_max_simds = 4;
		dev_priv->r600_max_backends = 4;
		dev_priv->r600_max_gprs = 256;
		dev_priv->r600_max_threads = 192;
		dev_priv->r600_max_stack_entries = 256;
		dev_priv->r600_max_hw_contexts = 8;
		dev_priv->r600_max_gs_threads = 16;
		dev_priv->r600_sx_max_export_size = 128;
		dev_priv->r600_sx_max_export_pos_size = 16;
		dev_priv->r600_sx_max_export_smx_size = 128;
		dev_priv->r600_sq_num_cf_insts = 2;
		break;
	case CHIP_RV630:
	case CHIP_RV635:
		dev_priv->r600_max_pipes = 2;
		dev_priv->r600_max_tile_pipes = 2;
		dev_priv->r600_max_simds = 3;
		dev_priv->r600_max_backends = 1;
		dev_priv->r600_max_gprs = 128;
		dev_priv->r600_max_threads = 192;
		dev_priv->r600_max_stack_entries = 128;
		dev_priv->r600_max_hw_contexts = 8;
		dev_priv->r600_max_gs_threads = 4;
		dev_priv->r600_sx_max_export_size = 128;
		dev_priv->r600_sx_max_export_pos_size = 16;
		dev_priv->r600_sx_max_export_smx_size = 128;
		dev_priv->r600_sq_num_cf_insts = 2;
		break;
	case CHIP_RV610:
	case CHIP_RS780:
	case CHIP_RS880:
	case CHIP_RV620:
		dev_priv->r600_max_pipes = 1;
		dev_priv->r600_max_tile_pipes = 1;
		dev_priv->r600_max_simds = 2;
		dev_priv->r600_max_backends = 1;
		dev_priv->r600_max_gprs = 128;
		dev_priv->r600_max_threads = 192;
		dev_priv->r600_max_stack_entries = 128;
		dev_priv->r600_max_hw_contexts = 4;
		dev_priv->r600_max_gs_threads = 4;
		dev_priv->r600_sx_max_export_size = 128;
		dev_priv->r600_sx_max_export_pos_size = 16;
		dev_priv->r600_sx_max_export_smx_size = 128;
		dev_priv->r600_sq_num_cf_insts = 1;
		break;
	case CHIP_RV670:
		dev_priv->r600_max_pipes = 4;
		dev_priv->r600_max_tile_pipes = 4;
		dev_priv->r600_max_simds = 4;
		dev_priv->r600_max_backends = 4;
		dev_priv->r600_max_gprs = 192;
		dev_priv->r600_max_threads = 192;
		dev_priv->r600_max_stack_entries = 256;
		dev_priv->r600_max_hw_contexts = 8;
		dev_priv->r600_max_gs_threads = 16;
		dev_priv->r600_sx_max_export_size = 128;
		dev_priv->r600_sx_max_export_pos_size = 16;
		dev_priv->r600_sx_max_export_smx_size = 128;
		dev_priv->r600_sq_num_cf_insts = 2;
		break;
	default:
		break;
	}

	/* Initialize HDP */
	j = 0;
	for (i = 0; i < 32; i++) {
		RADEON_WRITE((0x2c14 + j), 0x00000000);
		RADEON_WRITE((0x2c18 + j), 0x00000000);
		RADEON_WRITE((0x2c1c + j), 0x00000000);
		RADEON_WRITE((0x2c20 + j), 0x00000000);
		RADEON_WRITE((0x2c24 + j), 0x00000000);
		j += 0x18;
	}

	RADEON_WRITE(R600_GRBM_CNTL, R600_GRBM_READ_TIMEOUT(0xff));

	/* setup tiling, simd, pipe config */
	ramcfg = RADEON_READ(R600_RAMCFG);

	switch (dev_priv->r600_max_tile_pipes) {
	case 1:
		gb_tiling_config |= R600_PIPE_TILING(0);
		break;
	case 2:
		gb_tiling_config |= R600_PIPE_TILING(1);
		break;
	case 4:
		gb_tiling_config |= R600_PIPE_TILING(2);
		break;
	case 8:
		gb_tiling_config |= R600_PIPE_TILING(3);
		break;
	default:
		break;
	}

	gb_tiling_config |= R600_BANK_TILING((ramcfg >> R600_NOOFBANK_SHIFT) & R600_NOOFBANK_MASK);

	gb_tiling_config |= R600_GROUP_SIZE(0);

	if (((ramcfg >> R600_NOOFROWS_SHIFT) & R600_NOOFROWS_MASK) > 3) {
		gb_tiling_config |= R600_ROW_TILING(3);
		gb_tiling_config |= R600_SAMPLE_SPLIT(3);
	} else {
		gb_tiling_config |=
			R600_ROW_TILING(((ramcfg >> R600_NOOFROWS_SHIFT) & R600_NOOFROWS_MASK));
		gb_tiling_config |=
			R600_SAMPLE_SPLIT(((ramcfg >> R600_NOOFROWS_SHIFT) & R600_NOOFROWS_MASK));
	}

	gb_tiling_config |= R600_BANK_SWAPS(1);

	backend_map = r600_get_tile_pipe_to_backend_map(dev_priv->r600_max_tile_pipes,
							dev_priv->r600_max_backends,
							(0xff << dev_priv->r600_max_backends) & 0xff);
	gb_tiling_config |= R600_BACKEND_MAP(backend_map);

	cc_gc_shader_pipe_config =
		R600_INACTIVE_QD_PIPES((R6XX_MAX_PIPES_MASK << dev_priv->r600_max_pipes) & R6XX_MAX_PIPES_MASK);
	cc_gc_shader_pipe_config |=
		R600_INACTIVE_SIMDS((R6XX_MAX_SIMDS_MASK << dev_priv->r600_max_simds) & R6XX_MAX_SIMDS_MASK);

	cc_rb_backend_disable =
		R600_BACKEND_DISABLE((R6XX_MAX_BACKENDS_MASK << dev_priv->r600_max_backends) & R6XX_MAX_BACKENDS_MASK);

	RADEON_WRITE(R600_GB_TILING_CONFIG,      gb_tiling_config);
	RADEON_WRITE(R600_DCP_TILING_CONFIG,    (gb_tiling_config & 0xffff));
	RADEON_WRITE(R600_HDP_TILING_CONFIG,    (gb_tiling_config & 0xffff));

	RADEON_WRITE(R600_CC_RB_BACKEND_DISABLE,      cc_rb_backend_disable);
	RADEON_WRITE(R600_CC_GC_SHADER_PIPE_CONFIG,   cc_gc_shader_pipe_config);
	RADEON_WRITE(R600_GC_USER_SHADER_PIPE_CONFIG, cc_gc_shader_pipe_config);

	num_qd_pipes =
		R6XX_MAX_BACKENDS - r600_count_pipe_bits(cc_gc_shader_pipe_config & R600_INACTIVE_QD_PIPES_MASK);
	RADEON_WRITE(R600_VGT_OUT_DEALLOC_CNTL, (num_qd_pipes * 4) & R600_DEALLOC_DIST_MASK);
	RADEON_WRITE(R600_VGT_VERTEX_REUSE_BLOCK_CNTL, ((num_qd_pipes * 4) - 2) & R600_VTX_REUSE_DEPTH_MASK);

	/* set HW defaults for 3D engine */
	RADEON_WRITE(R600_CP_QUEUE_THRESHOLDS, (R600_ROQ_IB1_START(0x16) |
						R600_ROQ_IB2_START(0x2b)));

	RADEON_WRITE(R600_CP_MEQ_THRESHOLDS, (R600_MEQ_END(0x40) |
					      R600_ROQ_END(0x40)));

	RADEON_WRITE(R600_TA_CNTL_AUX, (R600_DISABLE_CUBE_ANISO |
					R600_SYNC_GRADIENT |
					R600_SYNC_WALKER |
					R600_SYNC_ALIGNER));

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV670)
		RADEON_WRITE(R600_ARB_GDEC_RD_CNTL, 0x00000021);

	sx_debug_1 = RADEON_READ(R600_SX_DEBUG_1);
	sx_debug_1 |= R600_SMX_EVENT_RELEASE;
	if (((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_R600))
		sx_debug_1 |= R600_ENABLE_NEW_SMX_ADDRESS;
	RADEON_WRITE(R600_SX_DEBUG_1, sx_debug_1);

	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R600) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV630) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV610) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV620) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS780) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS880))
		RADEON_WRITE(R600_DB_DEBUG, R600_PREZ_MUST_WAIT_FOR_POSTZ_DONE);
	else
		RADEON_WRITE(R600_DB_DEBUG, 0);

	RADEON_WRITE(R600_DB_WATERMARKS, (R600_DEPTH_FREE(4) |
					  R600_DEPTH_FLUSH(16) |
					  R600_DEPTH_PENDING_FREE(4) |
					  R600_DEPTH_CACHELINE_FREE(16)));
	RADEON_WRITE(R600_PA_SC_MULTI_CHIP_CNTL, 0);
	RADEON_WRITE(R600_VGT_NUM_INSTANCES, 0);

	RADEON_WRITE(R600_SPI_CONFIG_CNTL, R600_GPR_WRITE_PRIORITY(0));
	RADEON_WRITE(R600_SPI_CONFIG_CNTL_1, R600_VTX_DONE_DELAY(0));

	sq_ms_fifo_sizes = RADEON_READ(R600_SQ_MS_FIFO_SIZES);
	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV610) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV620) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS780) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS880)) {
		sq_ms_fifo_sizes = (R600_CACHE_FIFO_SIZE(0xa) |
				    R600_FETCH_FIFO_HIWATER(0xa) |
				    R600_DONE_FIFO_HIWATER(0xe0) |
				    R600_ALU_UPDATE_FIFO_HIWATER(0x8));
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R600) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV630)) {
		sq_ms_fifo_sizes &= ~R600_DONE_FIFO_HIWATER(0xff);
		sq_ms_fifo_sizes |= R600_DONE_FIFO_HIWATER(0x4);
	}
	RADEON_WRITE(R600_SQ_MS_FIFO_SIZES, sq_ms_fifo_sizes);

	/* SQ_CONFIG, SQ_GPR_RESOURCE_MGMT, SQ_THREAD_RESOURCE_MGMT, SQ_STACK_RESOURCE_MGMT
	 * should be adjusted as needed by the 2D/3D drivers.  This just sets default values
	 */
	sq_config = RADEON_READ(R600_SQ_CONFIG);
	sq_config &= ~(R600_PS_PRIO(3) |
		       R600_VS_PRIO(3) |
		       R600_GS_PRIO(3) |
		       R600_ES_PRIO(3));
	sq_config |= (R600_DX9_CONSTS |
		      R600_VC_ENABLE |
		      R600_PS_PRIO(0) |
		      R600_VS_PRIO(1) |
		      R600_GS_PRIO(2) |
		      R600_ES_PRIO(3));

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R600) {
		sq_gpr_resource_mgmt_1 = (R600_NUM_PS_GPRS(124) |
					  R600_NUM_VS_GPRS(124) |
					  R600_NUM_CLAUSE_TEMP_GPRS(4));
		sq_gpr_resource_mgmt_2 = (R600_NUM_GS_GPRS(0) |
					  R600_NUM_ES_GPRS(0));
		sq_thread_resource_mgmt = (R600_NUM_PS_THREADS(136) |
					   R600_NUM_VS_THREADS(48) |
					   R600_NUM_GS_THREADS(4) |
					   R600_NUM_ES_THREADS(4));
		sq_stack_resource_mgmt_1 = (R600_NUM_PS_STACK_ENTRIES(128) |
					    R600_NUM_VS_STACK_ENTRIES(128));
		sq_stack_resource_mgmt_2 = (R600_NUM_GS_STACK_ENTRIES(0) |
					    R600_NUM_ES_STACK_ENTRIES(0));
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV610) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV620) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS780) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS880)) {
		/* no vertex cache */
		sq_config &= ~R600_VC_ENABLE;

		sq_gpr_resource_mgmt_1 = (R600_NUM_PS_GPRS(44) |
					  R600_NUM_VS_GPRS(44) |
					  R600_NUM_CLAUSE_TEMP_GPRS(2));
		sq_gpr_resource_mgmt_2 = (R600_NUM_GS_GPRS(17) |
					  R600_NUM_ES_GPRS(17));
		sq_thread_resource_mgmt = (R600_NUM_PS_THREADS(79) |
					   R600_NUM_VS_THREADS(78) |
					   R600_NUM_GS_THREADS(4) |
					   R600_NUM_ES_THREADS(31));
		sq_stack_resource_mgmt_1 = (R600_NUM_PS_STACK_ENTRIES(40) |
					    R600_NUM_VS_STACK_ENTRIES(40));
		sq_stack_resource_mgmt_2 = (R600_NUM_GS_STACK_ENTRIES(32) |
					    R600_NUM_ES_STACK_ENTRIES(16));
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV630) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV635)) {
		sq_gpr_resource_mgmt_1 = (R600_NUM_PS_GPRS(44) |
					  R600_NUM_VS_GPRS(44) |
					  R600_NUM_CLAUSE_TEMP_GPRS(2));
		sq_gpr_resource_mgmt_2 = (R600_NUM_GS_GPRS(18) |
					  R600_NUM_ES_GPRS(18));
		sq_thread_resource_mgmt = (R600_NUM_PS_THREADS(79) |
					   R600_NUM_VS_THREADS(78) |
					   R600_NUM_GS_THREADS(4) |
					   R600_NUM_ES_THREADS(31));
		sq_stack_resource_mgmt_1 = (R600_NUM_PS_STACK_ENTRIES(40) |
					    R600_NUM_VS_STACK_ENTRIES(40));
		sq_stack_resource_mgmt_2 = (R600_NUM_GS_STACK_ENTRIES(32) |
					    R600_NUM_ES_STACK_ENTRIES(16));
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV670) {
		sq_gpr_resource_mgmt_1 = (R600_NUM_PS_GPRS(44) |
					  R600_NUM_VS_GPRS(44) |
					  R600_NUM_CLAUSE_TEMP_GPRS(2));
		sq_gpr_resource_mgmt_2 = (R600_NUM_GS_GPRS(17) |
					  R600_NUM_ES_GPRS(17));
		sq_thread_resource_mgmt = (R600_NUM_PS_THREADS(79) |
					   R600_NUM_VS_THREADS(78) |
					   R600_NUM_GS_THREADS(4) |
					   R600_NUM_ES_THREADS(31));
		sq_stack_resource_mgmt_1 = (R600_NUM_PS_STACK_ENTRIES(64) |
					    R600_NUM_VS_STACK_ENTRIES(64));
		sq_stack_resource_mgmt_2 = (R600_NUM_GS_STACK_ENTRIES(64) |
					    R600_NUM_ES_STACK_ENTRIES(64));
	}

	RADEON_WRITE(R600_SQ_CONFIG, sq_config);
	RADEON_WRITE(R600_SQ_GPR_RESOURCE_MGMT_1,  sq_gpr_resource_mgmt_1);
	RADEON_WRITE(R600_SQ_GPR_RESOURCE_MGMT_2,  sq_gpr_resource_mgmt_2);
	RADEON_WRITE(R600_SQ_THREAD_RESOURCE_MGMT, sq_thread_resource_mgmt);
	RADEON_WRITE(R600_SQ_STACK_RESOURCE_MGMT_1, sq_stack_resource_mgmt_1);
	RADEON_WRITE(R600_SQ_STACK_RESOURCE_MGMT_2, sq_stack_resource_mgmt_2);

	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV610) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV620) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS780) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS880))
		RADEON_WRITE(R600_VGT_CACHE_INVALIDATION, R600_CACHE_INVALIDATION(R600_TC_ONLY));
	else
		RADEON_WRITE(R600_VGT_CACHE_INVALIDATION, R600_CACHE_INVALIDATION(R600_VC_AND_TC));

	RADEON_WRITE(R600_PA_SC_AA_SAMPLE_LOCS_2S, (R600_S0_X(0xc) |
						    R600_S0_Y(0x4) |
						    R600_S1_X(0x4) |
						    R600_S1_Y(0xc)));
	RADEON_WRITE(R600_PA_SC_AA_SAMPLE_LOCS_4S, (R600_S0_X(0xe) |
						    R600_S0_Y(0xe) |
						    R600_S1_X(0x2) |
						    R600_S1_Y(0x2) |
						    R600_S2_X(0xa) |
						    R600_S2_Y(0x6) |
						    R600_S3_X(0x6) |
						    R600_S3_Y(0xa)));
	RADEON_WRITE(R600_PA_SC_AA_SAMPLE_LOCS_8S_WD0, (R600_S0_X(0xe) |
							R600_S0_Y(0xb) |
							R600_S1_X(0x4) |
							R600_S1_Y(0xc) |
							R600_S2_X(0x1) |
							R600_S2_Y(0x6) |
							R600_S3_X(0xa) |
							R600_S3_Y(0xe)));
	RADEON_WRITE(R600_PA_SC_AA_SAMPLE_LOCS_8S_WD1, (R600_S4_X(0x6) |
							R600_S4_Y(0x1) |
							R600_S5_X(0x0) |
							R600_S5_Y(0x0) |
							R600_S6_X(0xb) |
							R600_S6_Y(0x4) |
							R600_S7_X(0x7) |
							R600_S7_Y(0x8)));


	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_R600:
	case CHIP_RV630:
	case CHIP_RV635:
		gs_prim_buffer_depth = 0;
		break;
	case CHIP_RV610:
	case CHIP_RS780:
	case CHIP_RS880:
	case CHIP_RV620:
		gs_prim_buffer_depth = 32;
		break;
	case CHIP_RV670:
		gs_prim_buffer_depth = 128;
		break;
	default:
		break;
	}

	num_gs_verts_per_thread = dev_priv->r600_max_pipes * 16;
	vgt_gs_per_es = gs_prim_buffer_depth + num_gs_verts_per_thread;
	/* Max value for this is 256 */
	if (vgt_gs_per_es > 256)
		vgt_gs_per_es = 256;

	RADEON_WRITE(R600_VGT_ES_PER_GS, 128);
	RADEON_WRITE(R600_VGT_GS_PER_ES, vgt_gs_per_es);
	RADEON_WRITE(R600_VGT_GS_PER_VS, 2);
	RADEON_WRITE(R600_VGT_GS_VERTEX_REUSE, 16);

	/* more default values. 2D/3D driver should adjust as needed */
	RADEON_WRITE(R600_PA_SC_LINE_STIPPLE_STATE, 0);
	RADEON_WRITE(R600_VGT_STRMOUT_EN, 0);
	RADEON_WRITE(R600_SX_MISC, 0);
	RADEON_WRITE(R600_PA_SC_MODE_CNTL, 0);
	RADEON_WRITE(R600_PA_SC_AA_CONFIG, 0);
	RADEON_WRITE(R600_PA_SC_LINE_STIPPLE, 0);
	RADEON_WRITE(R600_SPI_INPUT_Z, 0);
	RADEON_WRITE(R600_SPI_PS_IN_CONTROL_0, R600_NUM_INTERP(2));
	RADEON_WRITE(R600_CB_COLOR7_FRAG, 0);

	/* clear render buffer base addresses */
	RADEON_WRITE(R600_CB_COLOR0_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR1_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR2_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR3_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR4_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR5_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR6_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR7_BASE, 0);

	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_RV610:
	case CHIP_RS780:
	case CHIP_RS880:
	case CHIP_RV620:
		tc_cntl = R600_TC_L2_SIZE(8);
		break;
	case CHIP_RV630:
	case CHIP_RV635:
		tc_cntl = R600_TC_L2_SIZE(4);
		break;
	case CHIP_R600:
		tc_cntl = R600_TC_L2_SIZE(0) | R600_L2_DISABLE_LATE_HIT;
		break;
	default:
		tc_cntl = R600_TC_L2_SIZE(0);
		break;
	}

	RADEON_WRITE(R600_TC_CNTL, tc_cntl);

	hdp_host_path_cntl = RADEON_READ(R600_HDP_HOST_PATH_CNTL);
	RADEON_WRITE(R600_HDP_HOST_PATH_CNTL, hdp_host_path_cntl);

	arb_pop = RADEON_READ(R600_ARB_POP);
	arb_pop |= R600_ENABLE_TC128;
	RADEON_WRITE(R600_ARB_POP, arb_pop);

	RADEON_WRITE(R600_PA_SC_MULTI_CHIP_CNTL, 0);
	RADEON_WRITE(R600_PA_CL_ENHANCE, (R600_CLIP_VTX_REORDER_ENA |
					  R600_NUM_CLIP_SEQ(3)));
	RADEON_WRITE(R600_PA_SC_ENHANCE, R600_FORCE_EOV_MAX_CLK_CNT(4095));

}

static u32
r700_get_tile_pipe_to_backend_map(u32 num_tile_pipes, u32 num_backends,
    u32 backend_disable_mask)
{
	u32 backend_map = 0;
	u32 enabled_backends_mask;
	u32 enabled_backends_count;
	u32 cur_pipe;
	u32 swizzle_pipe[R7XX_MAX_PIPES];
	u32 cur_backend;
	u32 i;

	if (num_tile_pipes > R7XX_MAX_PIPES)
		num_tile_pipes = R7XX_MAX_PIPES;
	if (num_tile_pipes < 1)
		num_tile_pipes = 1;
	if (num_backends > R7XX_MAX_BACKENDS)
		num_backends = R7XX_MAX_BACKENDS;
	if (num_backends < 1)
		num_backends = 1;

	enabled_backends_mask = 0;
	enabled_backends_count = 0;
	for (i = 0; i < R7XX_MAX_BACKENDS; ++i) {
		if (((backend_disable_mask >> i) & 1) == 0) {
			enabled_backends_mask |= (1 << i);
			++enabled_backends_count;
		}
		if (enabled_backends_count == num_backends)
			break;
	}

	if (enabled_backends_count == 0) {
		enabled_backends_mask = 1;
		enabled_backends_count = 1;
	}

	if (enabled_backends_count != num_backends)
		num_backends = enabled_backends_count;

	memset((uint8_t *)&swizzle_pipe[0], 0, sizeof(u32) * R7XX_MAX_PIPES);
	switch (num_tile_pipes) {
	case 1:
		swizzle_pipe[0] = 0;
		break;
	case 2:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 1;
		break;
	case 3:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 1;
		break;
	case 4:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 3;
		swizzle_pipe[3] = 1;
		break;
	case 5:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 4;
		swizzle_pipe[3] = 1;
		swizzle_pipe[4] = 3;
		break;
	case 6:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 4;
		swizzle_pipe[3] = 5;
		swizzle_pipe[4] = 3;
		swizzle_pipe[5] = 1;
		break;
	case 7:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 4;
		swizzle_pipe[3] = 6;
		swizzle_pipe[4] = 3;
		swizzle_pipe[5] = 1;
		swizzle_pipe[6] = 5;
		break;
	case 8:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 2;
		swizzle_pipe[2] = 4;
		swizzle_pipe[3] = 6;
		swizzle_pipe[4] = 3;
		swizzle_pipe[5] = 1;
		swizzle_pipe[6] = 7;
		swizzle_pipe[7] = 5;
		break;
	}

	cur_backend = 0;
	for (cur_pipe = 0; cur_pipe < num_tile_pipes; ++cur_pipe) {
		while (((1 << cur_backend) & enabled_backends_mask) == 0)
			cur_backend = (cur_backend + 1) % R7XX_MAX_BACKENDS;

		backend_map |= (u32)(((cur_backend & 3) << (swizzle_pipe[cur_pipe] * 2)));

		cur_backend = (cur_backend + 1) % R7XX_MAX_BACKENDS;
	}

	return backend_map;
}

static void r700_gfx_init(struct drm_device *dev,
			  drm_radeon_private_t *dev_priv)
{
	int i, j, num_qd_pipes;
	u32 sx_debug_1;
	u32 smx_dc_ctl0;
	u32 num_gs_verts_per_thread;
	u32 vgt_gs_per_es;
	u32 gs_prim_buffer_depth = 0;
	u32 sq_ms_fifo_sizes;
	u32 sq_config;
	u32 sq_thread_resource_mgmt;
	u32 hdp_host_path_cntl;
	u32 sq_dyn_gpr_size_simd_ab_0;
	u32 backend_map;
	u32 gb_tiling_config = 0;
	u32 cc_rb_backend_disable = 0;
	u32 cc_gc_shader_pipe_config = 0;
	u32 mc_arb_ramcfg;
	u32 db_debug4;

	/* setup chip specs */
	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_RV770:
		dev_priv->r600_max_pipes = 4;
		dev_priv->r600_max_tile_pipes = 8;
		dev_priv->r600_max_simds = 10;
		dev_priv->r600_max_backends = 4;
		dev_priv->r600_max_gprs = 256;
		dev_priv->r600_max_threads = 248;
		dev_priv->r600_max_stack_entries = 512;
		dev_priv->r600_max_hw_contexts = 8;
		dev_priv->r600_max_gs_threads = 16 * 2;
		dev_priv->r600_sx_max_export_size = 128;
		dev_priv->r600_sx_max_export_pos_size = 16;
		dev_priv->r600_sx_max_export_smx_size = 112;
		dev_priv->r600_sq_num_cf_insts = 2;

		dev_priv->r700_sx_num_of_sets = 7;
		dev_priv->r700_sc_prim_fifo_size = 0xF9;
		dev_priv->r700_sc_hiz_tile_fifo_size = 0x30;
		dev_priv->r700_sc_earlyz_tile_fifo_fize = 0x130;
		break;
	case CHIP_RV730:
		dev_priv->r600_max_pipes = 2;
		dev_priv->r600_max_tile_pipes = 4;
		dev_priv->r600_max_simds = 8;
		dev_priv->r600_max_backends = 2;
		dev_priv->r600_max_gprs = 128;
		dev_priv->r600_max_threads = 248;
		dev_priv->r600_max_stack_entries = 256;
		dev_priv->r600_max_hw_contexts = 8;
		dev_priv->r600_max_gs_threads = 16 * 2;
		dev_priv->r600_sx_max_export_size = 256;
		dev_priv->r600_sx_max_export_pos_size = 32;
		dev_priv->r600_sx_max_export_smx_size = 224;
		dev_priv->r600_sq_num_cf_insts = 2;

		dev_priv->r700_sx_num_of_sets = 7;
		dev_priv->r700_sc_prim_fifo_size = 0xf9;
		dev_priv->r700_sc_hiz_tile_fifo_size = 0x30;
		dev_priv->r700_sc_earlyz_tile_fifo_fize = 0x130;
		if (dev_priv->r600_sx_max_export_pos_size > 16) {
			dev_priv->r600_sx_max_export_pos_size -= 16;
			dev_priv->r600_sx_max_export_smx_size += 16;
		}
		break;
	case CHIP_RV710:
		dev_priv->r600_max_pipes = 2;
		dev_priv->r600_max_tile_pipes = 2;
		dev_priv->r600_max_simds = 2;
		dev_priv->r600_max_backends = 1;
		dev_priv->r600_max_gprs = 256;
		dev_priv->r600_max_threads = 192;
		dev_priv->r600_max_stack_entries = 256;
		dev_priv->r600_max_hw_contexts = 4;
		dev_priv->r600_max_gs_threads = 8 * 2;
		dev_priv->r600_sx_max_export_size = 128;
		dev_priv->r600_sx_max_export_pos_size = 16;
		dev_priv->r600_sx_max_export_smx_size = 112;
		dev_priv->r600_sq_num_cf_insts = 1;

		dev_priv->r700_sx_num_of_sets = 7;
		dev_priv->r700_sc_prim_fifo_size = 0x40;
		dev_priv->r700_sc_hiz_tile_fifo_size = 0x30;
		dev_priv->r700_sc_earlyz_tile_fifo_fize = 0x130;
		break;
	case CHIP_RV740:
		dev_priv->r600_max_pipes = 4;
		dev_priv->r600_max_tile_pipes = 4;
		dev_priv->r600_max_simds = 8;
		dev_priv->r600_max_backends = 4;
		dev_priv->r600_max_gprs = 256;
		dev_priv->r600_max_threads = 248;
		dev_priv->r600_max_stack_entries = 512;
		dev_priv->r600_max_hw_contexts = 8;
		dev_priv->r600_max_gs_threads = 16 * 2;
		dev_priv->r600_sx_max_export_size = 256;
		dev_priv->r600_sx_max_export_pos_size = 32;
		dev_priv->r600_sx_max_export_smx_size = 224;
		dev_priv->r600_sq_num_cf_insts = 2;

		dev_priv->r700_sx_num_of_sets = 7;
		dev_priv->r700_sc_prim_fifo_size = 0x100;
		dev_priv->r700_sc_hiz_tile_fifo_size = 0x30;
		dev_priv->r700_sc_earlyz_tile_fifo_fize = 0x130;

		if (dev_priv->r600_sx_max_export_pos_size > 16) {
			dev_priv->r600_sx_max_export_pos_size -= 16;
			dev_priv->r600_sx_max_export_smx_size += 16;
		}
		break;
	default:
		break;
	}

	/* Initialize HDP */
	j = 0;
	for (i = 0; i < 32; i++) {
		RADEON_WRITE((0x2c14 + j), 0x00000000);
		RADEON_WRITE((0x2c18 + j), 0x00000000);
		RADEON_WRITE((0x2c1c + j), 0x00000000);
		RADEON_WRITE((0x2c20 + j), 0x00000000);
		RADEON_WRITE((0x2c24 + j), 0x00000000);
		j += 0x18;
	}

	RADEON_WRITE(R600_GRBM_CNTL, R600_GRBM_READ_TIMEOUT(0xff));

	/* setup tiling, simd, pipe config */
	mc_arb_ramcfg = RADEON_READ(R700_MC_ARB_RAMCFG);

	switch (dev_priv->r600_max_tile_pipes) {
	case 1:
		gb_tiling_config |= R600_PIPE_TILING(0);
		break;
	case 2:
		gb_tiling_config |= R600_PIPE_TILING(1);
		break;
	case 4:
		gb_tiling_config |= R600_PIPE_TILING(2);
		break;
	case 8:
		gb_tiling_config |= R600_PIPE_TILING(3);
		break;
	default:
		break;
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV770)
		gb_tiling_config |= R600_BANK_TILING(1);
	else
		gb_tiling_config |= R600_BANK_TILING((mc_arb_ramcfg >> R700_NOOFBANK_SHIFT) & R700_NOOFBANK_MASK);

	gb_tiling_config |= R600_GROUP_SIZE(0);

	if (((mc_arb_ramcfg >> R700_NOOFROWS_SHIFT) & R700_NOOFROWS_MASK) > 3) {
		gb_tiling_config |= R600_ROW_TILING(3);
		gb_tiling_config |= R600_SAMPLE_SPLIT(3);
	} else {
		gb_tiling_config |=
			R600_ROW_TILING(((mc_arb_ramcfg >> R700_NOOFROWS_SHIFT) & R700_NOOFROWS_MASK));
		gb_tiling_config |=
			R600_SAMPLE_SPLIT(((mc_arb_ramcfg >> R700_NOOFROWS_SHIFT) & R700_NOOFROWS_MASK));
	}

	gb_tiling_config |= R600_BANK_SWAPS(1);

	backend_map = r700_get_tile_pipe_to_backend_map(dev_priv->r600_max_tile_pipes,
							dev_priv->r600_max_backends,
							(0xff << dev_priv->r600_max_backends) & 0xff);
	gb_tiling_config |= R600_BACKEND_MAP(backend_map);

	cc_gc_shader_pipe_config =
		R600_INACTIVE_QD_PIPES((R7XX_MAX_PIPES_MASK << dev_priv->r600_max_pipes) & R7XX_MAX_PIPES_MASK);
	cc_gc_shader_pipe_config |=
		R600_INACTIVE_SIMDS((R7XX_MAX_SIMDS_MASK << dev_priv->r600_max_simds) & R7XX_MAX_SIMDS_MASK);

	cc_rb_backend_disable =
		R600_BACKEND_DISABLE((R7XX_MAX_BACKENDS_MASK << dev_priv->r600_max_backends) & R7XX_MAX_BACKENDS_MASK);

	RADEON_WRITE(R600_GB_TILING_CONFIG,      gb_tiling_config);
	RADEON_WRITE(R600_DCP_TILING_CONFIG,    (gb_tiling_config & 0xffff));
	RADEON_WRITE(R600_HDP_TILING_CONFIG,    (gb_tiling_config & 0xffff));

	RADEON_WRITE(R600_CC_RB_BACKEND_DISABLE,      cc_rb_backend_disable);
	RADEON_WRITE(R600_CC_GC_SHADER_PIPE_CONFIG,   cc_gc_shader_pipe_config);
	RADEON_WRITE(R600_GC_USER_SHADER_PIPE_CONFIG, cc_gc_shader_pipe_config);

	RADEON_WRITE(R700_CC_SYS_RB_BACKEND_DISABLE, cc_rb_backend_disable);
	RADEON_WRITE(R700_CGTS_SYS_TCC_DISABLE, 0);
	RADEON_WRITE(R700_CGTS_TCC_DISABLE, 0);
	RADEON_WRITE(R700_CGTS_USER_SYS_TCC_DISABLE, 0);
	RADEON_WRITE(R700_CGTS_USER_TCC_DISABLE, 0);

	num_qd_pipes =
		R7XX_MAX_BACKENDS - r600_count_pipe_bits(cc_gc_shader_pipe_config & R600_INACTIVE_QD_PIPES_MASK);
	RADEON_WRITE(R600_VGT_OUT_DEALLOC_CNTL, (num_qd_pipes * 4) & R600_DEALLOC_DIST_MASK);
	RADEON_WRITE(R600_VGT_VERTEX_REUSE_BLOCK_CNTL, ((num_qd_pipes * 4) - 2) & R600_VTX_REUSE_DEPTH_MASK);

	/* set HW defaults for 3D engine */
	RADEON_WRITE(R600_CP_QUEUE_THRESHOLDS, (R600_ROQ_IB1_START(0x16) |
						R600_ROQ_IB2_START(0x2b)));

	RADEON_WRITE(R600_CP_MEQ_THRESHOLDS, R700_STQ_SPLIT(0x30));

	RADEON_WRITE(R600_TA_CNTL_AUX, (R600_DISABLE_CUBE_ANISO |
					R600_SYNC_GRADIENT |
					R600_SYNC_WALKER |
					R600_SYNC_ALIGNER));

	sx_debug_1 = RADEON_READ(R700_SX_DEBUG_1);
	sx_debug_1 |= R700_ENABLE_NEW_SMX_ADDRESS;
	RADEON_WRITE(R700_SX_DEBUG_1, sx_debug_1);

	smx_dc_ctl0 = RADEON_READ(R600_SMX_DC_CTL0);
	smx_dc_ctl0 &= ~R700_CACHE_DEPTH(0x1ff);
	smx_dc_ctl0 |= R700_CACHE_DEPTH((dev_priv->r700_sx_num_of_sets * 64) - 1);
	RADEON_WRITE(R600_SMX_DC_CTL0, smx_dc_ctl0);

	RADEON_WRITE(R700_SMX_EVENT_CTL, (R700_ES_FLUSH_CTL(4) |
					  R700_GS_FLUSH_CTL(4) |
					  R700_ACK_FLUSH_CTL(3) |
					  R700_SYNC_FLUSH_CTL));

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV770)
		RADEON_WRITE(R700_DB_DEBUG3, R700_DB_CLK_OFF_DELAY(0x1f));
	else {
		db_debug4 = RADEON_READ(RV700_DB_DEBUG4);
		db_debug4 |= RV700_DISABLE_TILE_COVERED_FOR_PS_ITER;
		RADEON_WRITE(RV700_DB_DEBUG4, db_debug4);
	}

	RADEON_WRITE(R600_SX_EXPORT_BUFFER_SIZES, (R600_COLOR_BUFFER_SIZE((dev_priv->r600_sx_max_export_size / 4) - 1) |
						   R600_POSITION_BUFFER_SIZE((dev_priv->r600_sx_max_export_pos_size / 4) - 1) |
						   R600_SMX_BUFFER_SIZE((dev_priv->r600_sx_max_export_smx_size / 4) - 1)));

	RADEON_WRITE(R700_PA_SC_FIFO_SIZE_R7XX, (R700_SC_PRIM_FIFO_SIZE(dev_priv->r700_sc_prim_fifo_size) |
						 R700_SC_HIZ_TILE_FIFO_SIZE(dev_priv->r700_sc_hiz_tile_fifo_size) |
						 R700_SC_EARLYZ_TILE_FIFO_SIZE(dev_priv->r700_sc_earlyz_tile_fifo_fize)));

	RADEON_WRITE(R600_PA_SC_MULTI_CHIP_CNTL, 0);

	RADEON_WRITE(R600_VGT_NUM_INSTANCES, 1);

	RADEON_WRITE(R600_SPI_CONFIG_CNTL, R600_GPR_WRITE_PRIORITY(0));

	RADEON_WRITE(R600_SPI_CONFIG_CNTL_1, R600_VTX_DONE_DELAY(4));

	RADEON_WRITE(R600_CP_PERFMON_CNTL, 0);

	sq_ms_fifo_sizes = (R600_CACHE_FIFO_SIZE(16 * dev_priv->r600_sq_num_cf_insts) |
			    R600_DONE_FIFO_HIWATER(0xe0) |
			    R600_ALU_UPDATE_FIFO_HIWATER(0x8));
	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_RV770:
		sq_ms_fifo_sizes |= R600_FETCH_FIFO_HIWATER(0x1);
		break;
	case CHIP_RV730:
	case CHIP_RV710:
	case CHIP_RV740:
	default:
		sq_ms_fifo_sizes |= R600_FETCH_FIFO_HIWATER(0x4);
		break;
	}
	RADEON_WRITE(R600_SQ_MS_FIFO_SIZES, sq_ms_fifo_sizes);

	/* SQ_CONFIG, SQ_GPR_RESOURCE_MGMT, SQ_THREAD_RESOURCE_MGMT, SQ_STACK_RESOURCE_MGMT
	 * should be adjusted as needed by the 2D/3D drivers.  This just sets default values
	 */
	sq_config = RADEON_READ(R600_SQ_CONFIG);
	sq_config &= ~(R600_PS_PRIO(3) |
		       R600_VS_PRIO(3) |
		       R600_GS_PRIO(3) |
		       R600_ES_PRIO(3));
	sq_config |= (R600_DX9_CONSTS |
		      R600_VC_ENABLE |
		      R600_EXPORT_SRC_C |
		      R600_PS_PRIO(0) |
		      R600_VS_PRIO(1) |
		      R600_GS_PRIO(2) |
		      R600_ES_PRIO(3));
	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV710)
		/* no vertex cache */
		sq_config &= ~R600_VC_ENABLE;

	RADEON_WRITE(R600_SQ_CONFIG, sq_config);

	RADEON_WRITE(R600_SQ_GPR_RESOURCE_MGMT_1,  (R600_NUM_PS_GPRS((dev_priv->r600_max_gprs * 24)/64) |
						    R600_NUM_VS_GPRS((dev_priv->r600_max_gprs * 24)/64) |
						    R600_NUM_CLAUSE_TEMP_GPRS(((dev_priv->r600_max_gprs * 24)/64)/2)));

	RADEON_WRITE(R600_SQ_GPR_RESOURCE_MGMT_2,  (R600_NUM_GS_GPRS((dev_priv->r600_max_gprs * 7)/64) |
						    R600_NUM_ES_GPRS((dev_priv->r600_max_gprs * 7)/64)));

	sq_thread_resource_mgmt = (R600_NUM_PS_THREADS((dev_priv->r600_max_threads * 4)/8) |
				   R600_NUM_VS_THREADS((dev_priv->r600_max_threads * 2)/8) |
				   R600_NUM_ES_THREADS((dev_priv->r600_max_threads * 1)/8));
	if (((dev_priv->r600_max_threads * 1) / 8) > dev_priv->r600_max_gs_threads)
		sq_thread_resource_mgmt |= R600_NUM_GS_THREADS(dev_priv->r600_max_gs_threads);
	else
		sq_thread_resource_mgmt |= R600_NUM_GS_THREADS((dev_priv->r600_max_gs_threads * 1)/8);
	RADEON_WRITE(R600_SQ_THREAD_RESOURCE_MGMT, sq_thread_resource_mgmt);

	RADEON_WRITE(R600_SQ_STACK_RESOURCE_MGMT_1, (R600_NUM_PS_STACK_ENTRIES((dev_priv->r600_max_stack_entries * 1)/4) |
						     R600_NUM_VS_STACK_ENTRIES((dev_priv->r600_max_stack_entries * 1)/4)));

	RADEON_WRITE(R600_SQ_STACK_RESOURCE_MGMT_2, (R600_NUM_GS_STACK_ENTRIES((dev_priv->r600_max_stack_entries * 1)/4) |
						     R600_NUM_ES_STACK_ENTRIES((dev_priv->r600_max_stack_entries * 1)/4)));

	sq_dyn_gpr_size_simd_ab_0 = (R700_SIMDA_RING0((dev_priv->r600_max_gprs * 38)/64) |
				     R700_SIMDA_RING1((dev_priv->r600_max_gprs * 38)/64) |
				     R700_SIMDB_RING0((dev_priv->r600_max_gprs * 38)/64) |
				     R700_SIMDB_RING1((dev_priv->r600_max_gprs * 38)/64));

	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_0, sq_dyn_gpr_size_simd_ab_0);
	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_1, sq_dyn_gpr_size_simd_ab_0);
	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_2, sq_dyn_gpr_size_simd_ab_0);
	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_3, sq_dyn_gpr_size_simd_ab_0);
	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_4, sq_dyn_gpr_size_simd_ab_0);
	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_5, sq_dyn_gpr_size_simd_ab_0);
	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_6, sq_dyn_gpr_size_simd_ab_0);
	RADEON_WRITE(R700_SQ_DYN_GPR_SIZE_SIMD_AB_7, sq_dyn_gpr_size_simd_ab_0);

	RADEON_WRITE(R700_PA_SC_FORCE_EOV_MAX_CNTS, (R700_FORCE_EOV_MAX_CLK_CNT(4095) |
						     R700_FORCE_EOV_MAX_REZ_CNT(255)));

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV710)
		RADEON_WRITE(R600_VGT_CACHE_INVALIDATION, (R600_CACHE_INVALIDATION(R600_TC_ONLY) |
							   R700_AUTO_INVLD_EN(R700_ES_AND_GS_AUTO)));
	else
		RADEON_WRITE(R600_VGT_CACHE_INVALIDATION, (R600_CACHE_INVALIDATION(R600_VC_AND_TC) |
							   R700_AUTO_INVLD_EN(R700_ES_AND_GS_AUTO)));

	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_RV770:
	case CHIP_RV730:
	case CHIP_RV740:
		gs_prim_buffer_depth = 384;
		break;
	case CHIP_RV710:
		gs_prim_buffer_depth = 128;
		break;
	default:
		break;
	}

	num_gs_verts_per_thread = dev_priv->r600_max_pipes * 16;
	vgt_gs_per_es = gs_prim_buffer_depth + num_gs_verts_per_thread;
	/* Max value for this is 256 */
	if (vgt_gs_per_es > 256)
		vgt_gs_per_es = 256;

	RADEON_WRITE(R600_VGT_ES_PER_GS, 128);
	RADEON_WRITE(R600_VGT_GS_PER_ES, vgt_gs_per_es);
	RADEON_WRITE(R600_VGT_GS_PER_VS, 2);

	/* more default values. 2D/3D driver should adjust as needed */
	RADEON_WRITE(R600_VGT_GS_VERTEX_REUSE, 16);
	RADEON_WRITE(R600_PA_SC_LINE_STIPPLE_STATE, 0);
	RADEON_WRITE(R600_VGT_STRMOUT_EN, 0);
	RADEON_WRITE(R600_SX_MISC, 0);
	RADEON_WRITE(R600_PA_SC_MODE_CNTL, 0);
	RADEON_WRITE(R700_PA_SC_EDGERULE, 0xaaaaaaaa);
	RADEON_WRITE(R600_PA_SC_AA_CONFIG, 0);
	RADEON_WRITE(R600_PA_SC_CLIPRECT_RULE, 0xffff);
	RADEON_WRITE(R600_PA_SC_LINE_STIPPLE, 0);
	RADEON_WRITE(R600_SPI_INPUT_Z, 0);
	RADEON_WRITE(R600_SPI_PS_IN_CONTROL_0, R600_NUM_INTERP(2));
	RADEON_WRITE(R600_CB_COLOR7_FRAG, 0);

	/* clear render buffer base addresses */
	RADEON_WRITE(R600_CB_COLOR0_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR1_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR2_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR3_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR4_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR5_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR6_BASE, 0);
	RADEON_WRITE(R600_CB_COLOR7_BASE, 0);

	RADEON_WRITE(R700_TCP_CNTL, 0);

	hdp_host_path_cntl = RADEON_READ(R600_HDP_HOST_PATH_CNTL);
	RADEON_WRITE(R600_HDP_HOST_PATH_CNTL, hdp_host_path_cntl);

	RADEON_WRITE(R600_PA_SC_MULTI_CHIP_CNTL, 0);

	RADEON_WRITE(R600_PA_CL_ENHANCE, (R600_CLIP_VTX_REORDER_ENA |
					  R600_NUM_CLIP_SEQ(3)));

}

#if RADEON_FIFO_DEBUG
static void
radeon_status(drm_radeon_private_t *dev_priv)
{
	printf("%s:\n", __FUNCTION__);
	printf("RBBM_STATUS = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_RBBM_STATUS));
	printf("CP_RB_RTPR = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_CP_RB_RPTR));
	printf("CP_RB_WTPR = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_CP_RB_WPTR));
	printf("AIC_CNTL = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_CNTL));
	printf("AIC_STAT = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_STAT));
	printf("AIC_PT_BASE = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_PT_BASE));
	printf("TLB_ADDR = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_TLB_ADDR));
	printf("TLB_DATA = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_TLB_DATA));
}
#endif

/* ================================================================
 * Engine, FIFO control
 */

int
radeon_do_pixcache_flush(drm_radeon_private_t *dev_priv)
{
	u32 tmp;
	int i;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV280) {
		tmp = RADEON_READ(RADEON_RB3D_DSTCACHE_CTLSTAT);
		tmp |= RADEON_RB3D_DC_FLUSH_ALL;
		RADEON_WRITE(RADEON_RB3D_DSTCACHE_CTLSTAT, tmp);

		for (i = 0; i < dev_priv->usec_timeout; i++) {
			if (!(RADEON_READ(RADEON_RB3D_DSTCACHE_CTLSTAT)
			      & RADEON_RB3D_DC_BUSY)) {
				return 0;
			}
			DRM_UDELAY(1);
		}
	} else {
		/* don't flush or purge cache here or lockup */
		return 0;
	}

#if RADEON_FIFO_DEBUG
	DRM_ERROR("failed!\n");
	radeon_status(dev_priv);
#endif
	return EBUSY;
}

int
radeon_do_wait_for_fifo(drm_radeon_private_t *dev_priv, int entries)
{
	int i;

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		int slots = (RADEON_READ(RADEON_RBBM_STATUS)
			     & RADEON_RBBM_FIFOCNT_MASK);
		if (slots >= entries)
			return 0;
		DRM_UDELAY(1);
	}
	DRM_INFO("wait for fifo failed status : 0x%08X 0x%08X\n",
		 RADEON_READ(RADEON_RBBM_STATUS),
		 RADEON_READ(R300_VAP_CNTL_STATUS));

#if RADEON_FIFO_DEBUG
	DRM_ERROR("failed!\n");
	radeon_status(dev_priv);
#endif
	return EBUSY;
}

int
radeon_do_wait_for_idle(drm_radeon_private_t *dev_priv)
{
	int i, ret;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		return (r600_do_wait_for_idle(dev_priv));

	ret = radeon_do_wait_for_fifo(dev_priv, 64);
	if (ret)
		return ret;

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		if (!(RADEON_READ(RADEON_RBBM_STATUS)
		      & RADEON_RBBM_ACTIVE)) {
			radeon_do_pixcache_flush(dev_priv);
			return 0;
		}
		DRM_UDELAY(1);
	}
	DRM_INFO("wait idle failed status : 0x%08X 0x%08X\n",
		 RADEON_READ(RADEON_RBBM_STATUS),
		 RADEON_READ(R300_VAP_CNTL_STATUS));

#if RADEON_FIFO_DEBUG
	DRM_ERROR("failed!\n");
	radeon_status(dev_priv);
#endif
	return EBUSY;
}

int
r600_do_wait_for_fifo(drm_radeon_private_t *dev_priv, int entries)
{
	int i;

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		int slots;
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
			slots = (RADEON_READ(R600_GRBM_STATUS)
				 & R700_CMDFIFO_AVAIL_MASK);
		else
			slots = (RADEON_READ(R600_GRBM_STATUS)
				 & R600_CMDFIFO_AVAIL_MASK);
		if (slots >= entries)
			return 0;
		DRM_UDELAY(1);
	}
	DRM_INFO("wait for fifo failed status : 0x%08X 0x%08X\n",
		 RADEON_READ(R600_GRBM_STATUS),
		 RADEON_READ(R600_GRBM_STATUS2));

	return -EBUSY;
}

int
r600_do_wait_for_idle(drm_radeon_private_t *dev_priv)
{
	int i, ret;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
		ret = r600_do_wait_for_fifo(dev_priv, 8);
	else
		ret = r600_do_wait_for_fifo(dev_priv, 16);
	if (ret)
		return ret;
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		if (!(RADEON_READ(R600_GRBM_STATUS) & R600_GUI_ACTIVE))
			return 0;
		DRM_UDELAY(1);
	}
	DRM_INFO("wait idle failed status : 0x%08X 0x%08X\n",
		 RADEON_READ(R600_GRBM_STATUS),
		 RADEON_READ(R600_GRBM_STATUS2));

	return -EBUSY;
}

void
radeon_init_pipes(drm_radeon_private_t *dev_priv)
{
	uint32_t gb_tile_config, gb_pipe_sel = 0;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV530) {
		uint32_t z_pipe_sel = RADEON_READ(RV530_GB_PIPE_SELECT2);
		if ((z_pipe_sel & 3) == 3)
			dev_priv->num_z_pipes = 2;
		else
			dev_priv->num_z_pipes = 1;
	} else
		dev_priv->num_z_pipes = 1;

	/* RS4xx/RS6xx/R4xx/R5xx */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R420) {
		gb_pipe_sel = RADEON_READ(R400_GB_PIPE_SELECT);
		dev_priv->num_gb_pipes = ((gb_pipe_sel >> 12) & 0x3) + 1;
	} else {
		/* R3xx */
		if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R300) ||
		    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R350)) {
			dev_priv->num_gb_pipes = 2;
		} else {
			/* R3Vxx */
			dev_priv->num_gb_pipes = 1;
		}
	}
	DRM_DEBUG("Num pipes: %d\n", dev_priv->num_gb_pipes);

	gb_tile_config = (R300_ENABLE_TILING | R300_TILE_SIZE_16 /*| R300_SUBPIXEL_1_16*/);

	switch (dev_priv->num_gb_pipes) {
	case 2: gb_tile_config |= R300_PIPE_COUNT_R300; break;
	case 3: gb_tile_config |= R300_PIPE_COUNT_R420_3P; break;
	case 4: gb_tile_config |= R300_PIPE_COUNT_R420; break;
	default:
	case 1: gb_tile_config |= R300_PIPE_COUNT_RV350; break;
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV515) {
		RADEON_WRITE_PLL(R500_DYN_SCLK_PWMEM_PIPE, (1 | ((gb_pipe_sel >> 8) & 0xf) << 4));
		RADEON_WRITE(R300_SU_REG_DEST, ((1 << dev_priv->num_gb_pipes) - 1));
	}
	RADEON_WRITE(R300_GB_TILE_CONFIG, gb_tile_config);
	radeon_do_wait_for_idle(dev_priv);
	RADEON_WRITE(R300_DST_PIPE_CONFIG, RADEON_READ(R300_DST_PIPE_CONFIG) | R300_PIPE_AUTO_CONFIG);
	RADEON_WRITE(R300_RB2D_DSTCACHE_MODE, (RADEON_READ(R300_RB2D_DSTCACHE_MODE) |
					       R300_DC_AUTOFLUSH_ENABLE |
					       R300_DC_DC_DISABLE_IGNORE_PE));


}

/* ================================================================
 * CP control, initialization
 */
static void r600_vm_flush_gart_range(struct drm_radeon_private *dev_priv)
{
	u_int32_t	resp, countdown = 1000;

	RADEON_WRITE(R600_VM_CONTEXT0_INVALIDATION_LOW_ADDR, dev_priv->gart_vm_start >> 12);
	RADEON_WRITE(R600_VM_CONTEXT0_INVALIDATION_HIGH_ADDR, (dev_priv->gart_vm_start + dev_priv->gart_size - 1) >> 12);
	RADEON_WRITE(R600_VM_CONTEXT0_REQUEST_RESPONSE, 2);

	do {
		resp = RADEON_READ(R600_VM_CONTEXT0_REQUEST_RESPONSE);
		countdown--;
		DRM_UDELAY(1);
	} while (((resp & 0xf0) == 0) && countdown);
}

static void r600_vm_init(struct drm_radeon_private *dev_priv)
{
	/* initialise the VM to use the page table we constructed up there */
	u32 vm_c0, i;
	u32 mc_rd_a;
	u32 vm_l2_cntl, vm_l2_cntl3;
	/* okay set up the PCIE aperture type thingo */
	RADEON_WRITE(R600_MC_VM_SYSTEM_APERTURE_LOW_ADDR, dev_priv->gart_vm_start >> 12);
	RADEON_WRITE(R600_MC_VM_SYSTEM_APERTURE_HIGH_ADDR, (dev_priv->gart_vm_start + dev_priv->gart_size - 1) >> 12);
	RADEON_WRITE(R600_MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR, 0);

	/* setup MC RD a */
	mc_rd_a = R600_MCD_L1_TLB | R600_MCD_L1_FRAG_PROC | R600_MCD_SYSTEM_ACCESS_MODE_IN_SYS |
		R600_MCD_SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU | R600_MCD_EFFECTIVE_L1_TLB_SIZE(5) |
		R600_MCD_EFFECTIVE_L1_QUEUE_SIZE(5) | R600_MCD_WAIT_L2_QUERY;

	RADEON_WRITE(R600_MCD_RD_A_CNTL, mc_rd_a);
	RADEON_WRITE(R600_MCD_RD_B_CNTL, mc_rd_a);

	RADEON_WRITE(R600_MCD_WR_A_CNTL, mc_rd_a);
	RADEON_WRITE(R600_MCD_WR_B_CNTL, mc_rd_a);

	RADEON_WRITE(R600_MCD_RD_GFX_CNTL, mc_rd_a);
	RADEON_WRITE(R600_MCD_WR_GFX_CNTL, mc_rd_a);

	RADEON_WRITE(R600_MCD_RD_SYS_CNTL, mc_rd_a);
	RADEON_WRITE(R600_MCD_WR_SYS_CNTL, mc_rd_a);

	RADEON_WRITE(R600_MCD_RD_HDP_CNTL, mc_rd_a | R600_MCD_L1_STRICT_ORDERING);
	RADEON_WRITE(R600_MCD_WR_HDP_CNTL, mc_rd_a /*| R600_MCD_L1_STRICT_ORDERING*/);

	RADEON_WRITE(R600_MCD_RD_PDMA_CNTL, mc_rd_a);
	RADEON_WRITE(R600_MCD_WR_PDMA_CNTL, mc_rd_a);

	RADEON_WRITE(R600_MCD_RD_SEM_CNTL, mc_rd_a | R600_MCD_SEMAPHORE_MODE);
	RADEON_WRITE(R600_MCD_WR_SEM_CNTL, mc_rd_a);

	vm_l2_cntl = R600_VM_L2_CACHE_EN | R600_VM_L2_FRAG_PROC | R600_VM_ENABLE_PTE_CACHE_LRU_W;
	vm_l2_cntl |= R600_VM_L2_CNTL_QUEUE_SIZE(7);
	RADEON_WRITE(R600_VM_L2_CNTL, vm_l2_cntl);

	RADEON_WRITE(R600_VM_L2_CNTL2, 0);
	vm_l2_cntl3 = (R600_VM_L2_CNTL3_BANK_SELECT_0(0) |
		       R600_VM_L2_CNTL3_BANK_SELECT_1(1) |
		       R600_VM_L2_CNTL3_CACHE_UPDATE_MODE(2));
	RADEON_WRITE(R600_VM_L2_CNTL3, vm_l2_cntl3);

	vm_c0 = R600_VM_ENABLE_CONTEXT | R600_VM_PAGE_TABLE_DEPTH_FLAT;

	RADEON_WRITE(R600_VM_CONTEXT0_CNTL, vm_c0);

	vm_c0 &= ~R600_VM_ENABLE_CONTEXT;

	/* disable all other contexts */
	for (i = 1; i < 8; i++)
		RADEON_WRITE(R600_VM_CONTEXT0_CNTL + (i * 4), vm_c0);

	RADEON_WRITE(R600_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, dev_priv->gart_info.bus_addr >> 12);
	RADEON_WRITE(R600_VM_CONTEXT0_PAGE_TABLE_START_ADDR, dev_priv->gart_vm_start >> 12);
	RADEON_WRITE(R600_VM_CONTEXT0_PAGE_TABLE_END_ADDR, (dev_priv->gart_vm_start + dev_priv->gart_size - 1) >> 12);

	r600_vm_flush_gart_range(dev_priv);
}

/* load r600 microcode */
static void r600_cp_load_microcode(drm_radeon_private_t *dev_priv)
{
	const u32 (*cp)[3];
	const u32 *pfp;
	int i;

	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_R600:
		DRM_DEBUG("Loading R600 Microcode\n");
		cp  = R600_cp_microcode;
		pfp = R600_pfp_microcode;
		break;
	case CHIP_RV610:
		DRM_DEBUG("Loading RV610 Microcode\n");
		cp  = RV610_cp_microcode;
		pfp = RV610_pfp_microcode;
		break;
	case CHIP_RV630:
		DRM_DEBUG("Loading RV630 Microcode\n");
		cp  = RV630_cp_microcode;
		pfp = RV630_pfp_microcode;
		break;
	case CHIP_RV620:
		DRM_DEBUG("Loading RV620 Microcode\n");
		cp  = RV620_cp_microcode;
		pfp = RV620_pfp_microcode;
		break;
	case CHIP_RV635:
		DRM_DEBUG("Loading RV635 Microcode\n");
		cp  = RV635_cp_microcode;
		pfp = RV635_pfp_microcode;
		break;
	case CHIP_RV670:
		DRM_DEBUG("Loading RV670 Microcode\n");
		cp  = RV670_cp_microcode;
		pfp = RV670_pfp_microcode;
		break;
	case CHIP_RS780:
	case CHIP_RS880:
		DRM_DEBUG("Loading RS780/RS880 Microcode\n");
		cp  = RS780_cp_microcode;
		pfp = RS780_pfp_microcode;
		break;
	default:
		return;
	}

	radeon_do_cp_stop(dev_priv);

	RADEON_WRITE(R600_CP_RB_CNTL,
		     R600_RB_NO_UPDATE |
		     R600_RB_BLKSZ(15) |
		     R600_RB_BUFSZ(3));

	RADEON_WRITE(R600_GRBM_SOFT_RESET, R600_SOFT_RESET_CP);
	RADEON_READ(R600_GRBM_SOFT_RESET);
	DRM_UDELAY(15000);
	RADEON_WRITE(R600_GRBM_SOFT_RESET, 0);

	RADEON_WRITE(R600_CP_ME_RAM_WADDR, 0);

	for (i = 0; i < PM4_UCODE_SIZE; i++) {
		RADEON_WRITE(R600_CP_ME_RAM_DATA, cp[i][0]);
		RADEON_WRITE(R600_CP_ME_RAM_DATA, cp[i][1]);
		RADEON_WRITE(R600_CP_ME_RAM_DATA, cp[i][2]);
	}

	RADEON_WRITE(R600_CP_PFP_UCODE_ADDR, 0);
	for (i = 0; i < PFP_UCODE_SIZE; i++)
		RADEON_WRITE(R600_CP_PFP_UCODE_DATA, pfp[i]);

	RADEON_WRITE(R600_CP_PFP_UCODE_ADDR, 0);
	RADEON_WRITE(R600_CP_ME_RAM_WADDR, 0);
	RADEON_WRITE(R600_CP_ME_RAM_RADDR, 0);
}

static void r700_vm_init(struct drm_radeon_private  *dev_priv)
{
	/* initialise the VM to use the page table we constructed up there */
	u32 vm_c0, i;
	u32 mc_vm_md_l1;
	u32 vm_l2_cntl, vm_l2_cntl3;
	/* okay set up the PCIE aperture type thingo */
	RADEON_WRITE(R700_MC_VM_SYSTEM_APERTURE_LOW_ADDR, dev_priv->gart_vm_start >> 12);
	RADEON_WRITE(R700_MC_VM_SYSTEM_APERTURE_HIGH_ADDR, (dev_priv->gart_vm_start + dev_priv->gart_size - 1) >> 12);
	RADEON_WRITE(R700_MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR, 0);

	mc_vm_md_l1 = R700_ENABLE_L1_TLB |
	    R700_ENABLE_L1_FRAGMENT_PROCESSING |
	    R700_SYSTEM_ACCESS_MODE_IN_SYS |
	    R700_SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
	    R700_EFFECTIVE_L1_TLB_SIZE(5) |
	    R700_EFFECTIVE_L1_QUEUE_SIZE(5);

	RADEON_WRITE(R700_MC_VM_MD_L1_TLB0_CNTL, mc_vm_md_l1);
	RADEON_WRITE(R700_MC_VM_MD_L1_TLB1_CNTL, mc_vm_md_l1);
	RADEON_WRITE(R700_MC_VM_MD_L1_TLB2_CNTL, mc_vm_md_l1);
	RADEON_WRITE(R700_MC_VM_MB_L1_TLB0_CNTL, mc_vm_md_l1);
	RADEON_WRITE(R700_MC_VM_MB_L1_TLB1_CNTL, mc_vm_md_l1);
	RADEON_WRITE(R700_MC_VM_MB_L1_TLB2_CNTL, mc_vm_md_l1);
	RADEON_WRITE(R700_MC_VM_MB_L1_TLB3_CNTL, mc_vm_md_l1);

	vm_l2_cntl = R600_VM_L2_CACHE_EN | R600_VM_L2_FRAG_PROC | R600_VM_ENABLE_PTE_CACHE_LRU_W;
	vm_l2_cntl |= R700_VM_L2_CNTL_QUEUE_SIZE(7);
	RADEON_WRITE(R600_VM_L2_CNTL, vm_l2_cntl);

	RADEON_WRITE(R600_VM_L2_CNTL2, 0);
	vm_l2_cntl3 = R700_VM_L2_CNTL3_BANK_SELECT(0) | R700_VM_L2_CNTL3_CACHE_UPDATE_MODE(2);
	RADEON_WRITE(R600_VM_L2_CNTL3, vm_l2_cntl3);

	vm_c0 = R600_VM_ENABLE_CONTEXT | R600_VM_PAGE_TABLE_DEPTH_FLAT;

	RADEON_WRITE(R600_VM_CONTEXT0_CNTL, vm_c0);

	vm_c0 &= ~R600_VM_ENABLE_CONTEXT;

	/* disable all other contexts */
	for (i = 1; i < 8; i++)
		RADEON_WRITE(R600_VM_CONTEXT0_CNTL + (i * 4), vm_c0);

	RADEON_WRITE(R700_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, dev_priv->gart_info.bus_addr >> 12);
	RADEON_WRITE(R700_VM_CONTEXT0_PAGE_TABLE_START_ADDR, dev_priv->gart_vm_start >> 12);
	RADEON_WRITE(R700_VM_CONTEXT0_PAGE_TABLE_END_ADDR, (dev_priv->gart_vm_start + dev_priv->gart_size - 1) >> 12);

	r600_vm_flush_gart_range(dev_priv);
}

/* load r600 microcode */
static void r700_cp_load_microcode(drm_radeon_private_t *dev_priv)
{
	const u32 *pfp;
	const u32 *cp;
	int i;

	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_RV770:
		DRM_DEBUG("Loading RV770/RV790 Microcode\n");
		pfp = RV770_pfp_microcode;
		cp  = RV770_cp_microcode;
		break;
	case CHIP_RV730:
	case CHIP_RV740:
		DRM_DEBUG("Loading RV730/RV740 Microcode\n");
		pfp = RV730_pfp_microcode;
		cp  = RV730_cp_microcode;
		break;
	case CHIP_RV710:
		DRM_DEBUG("Loading RV710 Microcode\n");
		pfp = RV710_pfp_microcode;
		cp  = RV710_cp_microcode;
		break;
	default:
		return;
	}

	radeon_do_cp_stop(dev_priv);

	RADEON_WRITE(R600_CP_RB_CNTL,
		     R600_RB_NO_UPDATE |
		     (15 << 8) |
		     (3 << 0));

	RADEON_WRITE(R600_GRBM_SOFT_RESET, R600_SOFT_RESET_CP);
	RADEON_READ(R600_GRBM_SOFT_RESET);
	DRM_UDELAY(15000);
	RADEON_WRITE(R600_GRBM_SOFT_RESET, 0);

	RADEON_WRITE(R600_CP_PFP_UCODE_ADDR, 0);
	for (i = 0; i < R700_PFP_UCODE_SIZE; i++)
		RADEON_WRITE(R600_CP_PFP_UCODE_DATA, pfp[i]);
	RADEON_WRITE(R600_CP_PFP_UCODE_ADDR, 0);

	RADEON_WRITE(R600_CP_ME_RAM_WADDR, 0);
	for (i = 0; i < R700_PM4_UCODE_SIZE; i++)
		RADEON_WRITE(R600_CP_ME_RAM_DATA, cp[i]);
	RADEON_WRITE(R600_CP_ME_RAM_WADDR, 0);

	RADEON_WRITE(R600_CP_PFP_UCODE_ADDR, 0);
	RADEON_WRITE(R600_CP_ME_RAM_WADDR, 0);
	RADEON_WRITE(R600_CP_ME_RAM_RADDR, 0);
}

/* Load the microcode for the CP */
void
radeon_cp_load_microcode(drm_radeon_private_t *dev_priv)
{
	const u32	(*cp)[2];
	int		 i;

	DRM_DEBUG("\n");

	if (((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)) {
		r700_cp_load_microcode(dev_priv);
		return;
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		r600_cp_load_microcode(dev_priv);
		return;
	}

	switch (dev_priv->flags & RADEON_FAMILY_MASK) {
	case CHIP_R100:
	case CHIP_RV100:
	case CHIP_RV200:
	case CHIP_RS100:
	case CHIP_RS200:
		DRM_DEBUG("Loading R100 Microcode\n");
		cp = R100_cp_microcode;
		break;
	case CHIP_R200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
		DRM_DEBUG("Loading R200 Microcode\n");
		cp = R200_cp_microcode;
		break;
	case CHIP_R300:
	case CHIP_R350:
	case CHIP_RV350:
	case CHIP_RV380:
	case CHIP_RS400:
	case CHIP_RS480:
		DRM_DEBUG("Loading R300 Microcode\n");
		cp = R300_cp_microcode;
		break;
	case CHIP_R420:
	case CHIP_R423:
	case CHIP_RV410:
		DRM_DEBUG("Loading R400 Microcode\n");
		cp = R420_cp_microcode;
		break;
	case CHIP_RS690:
	case CHIP_RS740:
		DRM_DEBUG("Loading RS690/RS740 Microcode\n");
		cp = RS690_cp_microcode;
		break;
	case CHIP_RS600:
		DRM_DEBUG("Loading RS600 Microcode\n");
		cp = RS600_cp_microcode;
		break;
	case CHIP_RV515:
	case CHIP_R520:
	case CHIP_RV530:
	case CHIP_R580:
	case CHIP_RV560:
	case CHIP_RV570:
		DRM_DEBUG("Loading R500 Microcode\n");
		cp = R520_cp_microcode;
		break;
	default:
		return;
	}

	radeon_do_wait_for_idle(dev_priv);

	RADEON_WRITE(RADEON_CP_ME_RAM_ADDR, 0);

	for (i = 0; i != 256; i++) {
		RADEON_WRITE(RADEON_CP_ME_RAM_DATAH, cp[i][1]);
		RADEON_WRITE(RADEON_CP_ME_RAM_DATAL, cp[i][0]);
	}
}

/* Wait for the CP to go idle.
 */
int
radeon_do_cp_idle(drm_radeon_private_t *dev_priv)
{
	DRM_DEBUG("\n");

	if (dev_priv->cp_running == 0)
		return (0);

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		BEGIN_RING(5);
		OUT_RING(CP_PACKET3(R600_IT_EVENT_WRITE, 0));
		OUT_RING(R600_CACHE_FLUSH_AND_INV_EVENT);
		/* wait for 3D idle clean */
		OUT_RING(CP_PACKET3(R600_IT_SET_CONFIG_REG, 1));
		OUT_RING((R600_WAIT_UNTIL - R600_SET_CONFIG_REG_OFFSET) >> 2);
		OUT_RING(RADEON_WAIT_3D_IDLE | RADEON_WAIT_3D_IDLECLEAN);

		ADVANCE_RING();
		COMMIT_RING();

		return (r600_do_wait_for_idle(dev_priv));
	}

	BEGIN_RING(6);

	RADEON_PURGE_CACHE();
	RADEON_PURGE_ZCACHE();
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();
	COMMIT_RING();

	return (radeon_do_wait_for_idle(dev_priv));
}

/* Start the Command Processor.
 */
void
radeon_do_cp_start(drm_radeon_private_t *dev_priv)
{
	DRM_DEBUG("\n");

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		BEGIN_RING(7);
		OUT_RING(CP_PACKET3(R600_IT_ME_INITIALIZE, 5));
		OUT_RING(0x00000001);
		if (((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_RV770))
			OUT_RING(0x00000003);
		else
			OUT_RING(0x00000000);
		OUT_RING((dev_priv->r600_max_hw_contexts - 1));
		OUT_RING(R600_ME_INITIALIZE_DEVICE_ID(1));
		OUT_RING(0x00000000);
		OUT_RING(0x00000000);
		ADVANCE_RING();
		COMMIT_RING();

		/* set the mux and reset the halt bit */
		RADEON_WRITE(R600_CP_ME_CNTL, 0xff);
	} else {
		radeon_do_wait_for_idle(dev_priv);

		RADEON_WRITE(RADEON_CP_CSQ_CNTL, dev_priv->cp_mode);

		/* on r420, any DMA from CP to system memory while 2D is active
		 * can cause a hang.  workaround is to queue a CP RESYNC token
		 */
		if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R420) {
			BEGIN_RING(3);
			OUT_RING(CP_PACKET0(R300_CP_RESYNC_ADDR, 1));
			OUT_RING(5); /* scratch reg 5 */
			OUT_RING(0xdeadbeef);
			ADVANCE_RING();
			COMMIT_RING();
		}

		BEGIN_RING(8);
		/* isync can only be written through cp on r5xx write it here */
		OUT_RING(CP_PACKET0(RADEON_ISYNC_CNTL, 0));
		OUT_RING(RADEON_ISYNC_ANY2D_IDLE3D |
			 RADEON_ISYNC_ANY3D_IDLE2D |
			 RADEON_ISYNC_WAIT_IDLEGUI |
			 RADEON_ISYNC_CPSCRATCH_IDLEGUI);
		RADEON_PURGE_CACHE();
		RADEON_PURGE_ZCACHE();
		RADEON_WAIT_UNTIL_IDLE();
		ADVANCE_RING();
		COMMIT_RING();

		dev_priv->track_flush |= RADEON_FLUSH_EMITED | RADEON_PURGE_EMITED;
	}

	dev_priv->cp_running = 1;
}

/* Reset the Command Processor.  This will not flush any pending
 * commands, so you must wait for the CP command stream to complete
 * before calling this routine.
 */
void
radeon_do_cp_reset(drm_radeon_private_t *dev_priv)
{
	u32 cur_read_ptr;
	DRM_DEBUG("\n");

	
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		cur_read_ptr = RADEON_READ(R600_CP_RB_RPTR);
		RADEON_WRITE(R600_CP_RB_WPTR, cur_read_ptr);
	} else {
		cur_read_ptr = RADEON_READ(RADEON_CP_RB_RPTR);
		RADEON_WRITE(RADEON_CP_RB_WPTR, cur_read_ptr);
	}
	radeondrm_set_ring_head(dev_priv, cur_read_ptr);
	dev_priv->ring.tail = cur_read_ptr;
}

/* Stop the Command Processor.  This will not flush any pending
 * commands, so you must flush the command stream and wait for the CP
 * to go idle before calling this routine.
 */
void
radeon_do_cp_stop(drm_radeon_private_t *dev_priv)
{
	DRM_DEBUG("\n");

	/* finish the pending CP_RESYNC token */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R420) {
		BEGIN_RING(2);
		OUT_RING(CP_PACKET0(R300_RB3D_DSTCACHE_CTLSTAT, 0));
		OUT_RING(R300_RB3D_DC_FINISH);
		ADVANCE_RING();
		COMMIT_RING();
		radeon_do_wait_for_idle(dev_priv);
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		RADEON_WRITE(R600_CP_ME_CNTL, 0xff | R600_CP_ME_HALT);
	else 
		RADEON_WRITE(RADEON_CP_CSQ_CNTL, RADEON_CSQ_PRIDIS_INDDIS);

	dev_priv->cp_running = 0;
}

int r600_do_engine_reset(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	u32 cp_ptr, cp_me_cntl, cp_rb_cntl;

	DRM_DEBUG("Resetting GPU\n");

	cp_ptr = RADEON_READ(R600_CP_RB_WPTR);
	cp_me_cntl = RADEON_READ(R600_CP_ME_CNTL);
	RADEON_WRITE(R600_CP_ME_CNTL, R600_CP_ME_HALT);

	RADEON_WRITE(R600_GRBM_SOFT_RESET, 0x7fff);
	RADEON_READ(R600_GRBM_SOFT_RESET);
	DRM_UDELAY(50);
	RADEON_WRITE(R600_GRBM_SOFT_RESET, 0);
	RADEON_READ(R600_GRBM_SOFT_RESET);

	RADEON_WRITE(R600_CP_RB_WPTR_DELAY, 0);
	cp_rb_cntl = RADEON_READ(R600_CP_RB_CNTL);
	RADEON_WRITE(R600_CP_RB_CNTL, R600_RB_RPTR_WR_ENA);

	RADEON_WRITE(R600_CP_RB_RPTR_WR, cp_ptr);
	RADEON_WRITE(R600_CP_RB_WPTR, cp_ptr);
	RADEON_WRITE(R600_CP_RB_CNTL, cp_rb_cntl);
	RADEON_WRITE(R600_CP_ME_CNTL, cp_me_cntl);

	/* Reset the CP ring */
	radeon_do_cp_reset(dev_priv);

	/* The CP is no longer running after an engine reset */
	dev_priv->cp_running = 0;

	/* Reset any pending vertex, indirect buffers */
	radeon_freelist_reset(dev);

	return 0;

}

/* Reset the engine.  This will stop the CP if it is running.
 */
int
radeon_do_engine_reset(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	u32 clock_cntl_index = 0, mclk_cntl = 0, rbbm_soft_reset;
	DRM_DEBUG("\n");

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		return (r600_do_engine_reset(dev));

	radeon_do_pixcache_flush(dev_priv);

	if ((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV410) {
	        /* may need something similar for newer chips */
		clock_cntl_index = RADEON_READ(RADEON_CLOCK_CNTL_INDEX);
		mclk_cntl = RADEON_READ_PLL(dev, RADEON_MCLK_CNTL);

		RADEON_WRITE_PLL(RADEON_MCLK_CNTL, (mclk_cntl |
						    RADEON_FORCEON_MCLKA |
						    RADEON_FORCEON_MCLKB |
						    RADEON_FORCEON_YCLKA |
						    RADEON_FORCEON_YCLKB |
						    RADEON_FORCEON_MC |
						    RADEON_FORCEON_AIC));
	}

	rbbm_soft_reset = RADEON_READ(RADEON_RBBM_SOFT_RESET);

	RADEON_WRITE(RADEON_RBBM_SOFT_RESET, (rbbm_soft_reset |
					      RADEON_SOFT_RESET_CP |
					      RADEON_SOFT_RESET_HI |
					      RADEON_SOFT_RESET_SE |
					      RADEON_SOFT_RESET_RE |
					      RADEON_SOFT_RESET_PP |
					      RADEON_SOFT_RESET_E2 |
					      RADEON_SOFT_RESET_RB));
	RADEON_READ(RADEON_RBBM_SOFT_RESET);
	RADEON_WRITE(RADEON_RBBM_SOFT_RESET, (rbbm_soft_reset &
					      ~(RADEON_SOFT_RESET_CP |
						RADEON_SOFT_RESET_HI |
						RADEON_SOFT_RESET_SE |
						RADEON_SOFT_RESET_RE |
						RADEON_SOFT_RESET_PP |
						RADEON_SOFT_RESET_E2 |
						RADEON_SOFT_RESET_RB)));
	RADEON_READ(RADEON_RBBM_SOFT_RESET);

	if ((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV410) {
		RADEON_WRITE_PLL(RADEON_MCLK_CNTL, mclk_cntl);
		RADEON_WRITE(RADEON_CLOCK_CNTL_INDEX, clock_cntl_index);
		RADEON_WRITE(RADEON_RBBM_SOFT_RESET, rbbm_soft_reset);
	}

	/* setup the raster pipes */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R300)
	    radeon_init_pipes(dev_priv);

	/* Reset the CP ring */
	radeon_do_cp_reset(dev_priv);

	/* The CP is no longer running after an engine reset */
	dev_priv->cp_running = 0;

	/* Reset any pending vertex, indirect buffers */
	radeon_freelist_reset(dev);

	return 0;
}

static void r600_cp_init_ring_buffer(struct drm_device *dev,
    drm_radeon_private_t *dev_priv)
{
	u32 ring_start;
	u64 rptr_addr;

	if (((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770))
		r700_gfx_init(dev, dev_priv);
	else
		r600_gfx_init(dev, dev_priv);

	RADEON_WRITE(R600_GRBM_SOFT_RESET, R600_SOFT_RESET_CP);
	RADEON_READ(R600_GRBM_SOFT_RESET);
	DRM_UDELAY(15000);
	RADEON_WRITE(R600_GRBM_SOFT_RESET, 0);


	/* Set ring buffer size */
#ifdef __BIG_ENDIAN
	RADEON_WRITE(R600_CP_RB_CNTL,
		     RADEON_BUF_SWAP_32BIT |
		     RADEON_RB_NO_UPDATE |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#else
	RADEON_WRITE(R600_CP_RB_CNTL,
		     RADEON_RB_NO_UPDATE |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#endif

	RADEON_WRITE(R600_CP_SEM_WAIT_TIMER, 0x4);

	/* Set the write pointer delay */
	RADEON_WRITE(R600_CP_RB_WPTR_DELAY, 0);

#ifdef __BIG_ENDIAN
	RADEON_WRITE(R600_CP_RB_CNTL,
		     RADEON_BUF_SWAP_32BIT |
		     RADEON_RB_NO_UPDATE |
		     RADEON_RB_RPTR_WR_ENA |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#else
	RADEON_WRITE(R600_CP_RB_CNTL,
		     RADEON_RB_NO_UPDATE |
		     RADEON_RB_RPTR_WR_ENA |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#endif

	/* Initialize the ring buffer's read and write pointers */
	RADEON_WRITE(R600_CP_RB_RPTR_WR, 0);
	RADEON_WRITE(R600_CP_RB_WPTR, 0);
	radeondrm_set_ring_head(dev_priv, 0);
	dev_priv->ring.tail = 0;

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		rptr_addr = dev_priv->ring_rptr->offset - dev->agp->base +
		    dev_priv->gart_vm_start;
	} else
#endif
	{
		rptr_addr = dev_priv->ring_rptr->offset - dev->sg->handle +
		    dev_priv->gart_vm_start;
	}
	RADEON_WRITE(R600_CP_RB_RPTR_ADDR,
		     rptr_addr & 0xffffffff);
	RADEON_WRITE(R600_CP_RB_RPTR_ADDR_HI,
		     upper_32_bits(rptr_addr));

#ifdef __BIG_ENDIAN
	RADEON_WRITE(R600_CP_RB_CNTL,
		     RADEON_BUF_SWAP_32BIT |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#else
	RADEON_WRITE(R600_CP_RB_CNTL,
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#endif

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		/* XXX */
		radeon_write_agp_base(dev_priv, dev->agp->base);

		/* XXX */
		radeon_write_agp_location(dev_priv,
			     (((dev_priv->gart_vm_start - 1 +
				dev_priv->gart_size) & 0xffff0000) |
			      (dev_priv->gart_vm_start >> 16)));

		ring_start = (dev_priv->cp_ring->offset
			      - dev->agp->base
			      + dev_priv->gart_vm_start);
	} else
#endif
		ring_start = (dev_priv->cp_ring->offset - dev->sg->handle +
		    dev_priv->gart_vm_start);

	RADEON_WRITE(R600_CP_RB_BASE, ring_start >> 8);

	RADEON_WRITE(R600_CP_ME_CNTL, 0xff);

	RADEON_WRITE(R600_CP_DEBUG, (1 << 27) | (1 << 28));

	/* Initialize the scratch register pointer.  This will cause
	 * the scratch register values to be written out to memory
	 * whenever they are updated.
	 *
	 * We simply put this behind the ring read pointer, this works
	 * with PCI GART as well as (whatever kind of) AGP GART
	 */
	{
		u64 scratch_addr;

		scratch_addr = RADEON_READ(R600_CP_RB_RPTR_ADDR);
		scratch_addr |= ((u64)RADEON_READ(R600_CP_RB_RPTR_ADDR_HI)) << 32;
		scratch_addr += R600_SCRATCH_REG_OFFSET;
		scratch_addr >>= 8;
		scratch_addr &= 0xffffffff;

		RADEON_WRITE(R600_SCRATCH_ADDR, (uint32_t)scratch_addr);
	}

	RADEON_WRITE(R600_SCRATCH_UMSK, 0x7);

	/* Turn on bus mastering */
	radeon_enable_bm(dev_priv);

	radeondrm_write_rptr(dev_priv, R600_SCRATCHOFF(0), 0);
	RADEON_WRITE(R600_LAST_FRAME_REG, 0);

	radeondrm_write_rptr(dev_priv, R600_SCRATCHOFF(1), 0);
	RADEON_WRITE(R600_LAST_DISPATCH_REG, 0);

	radeondrm_write_rptr(dev_priv, R600_SCRATCHOFF(2), 0);
	RADEON_WRITE(R600_LAST_CLEAR_REG, 0);

	/* reset sarea copies of these */
	if (dev_priv->sarea_priv) {
		dev_priv->sarea_priv->last_frame = 0;
		dev_priv->sarea_priv->last_dispatch = 0;
		dev_priv->sarea_priv->last_clear = 0;
	}

	r600_do_wait_for_idle(dev_priv);
}

void
radeon_cp_init_ring_buffer(struct drm_device *dev,
    drm_radeon_private_t *dev_priv)
{
	u32 ring_start, cur_read_ptr;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		r600_cp_init_ring_buffer(dev, dev_priv);
		return;
	}
	/* Initialize the memory controller. With new memory map, the fb location
	 * is not changed, it should have been properly initialized already. Part
	 * of the problem is that the code below is bogus, assuming the GART is
	 * always appended to the fb which is not necessarily the case
	 */
	if (!dev_priv->new_memmap)
		radeon_write_fb_location(dev_priv,
			     ((dev_priv->gart_vm_start - 1) & 0xffff0000)
			     | (dev_priv->fb_location >> 16));

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		radeon_write_agp_base(dev_priv, dev->agp->base);

		radeon_write_agp_location(dev_priv,
			     (((dev_priv->gart_vm_start - 1 +
				dev_priv->gart_size) & 0xffff0000) |
			      (dev_priv->gart_vm_start >> 16)));

		ring_start = (dev_priv->cp_ring->offset
			      - dev->agp->base
			      + dev_priv->gart_vm_start);
	} else
#endif
		ring_start = (dev_priv->cp_ring->offset - dev->sg->handle +
		    dev_priv->gart_vm_start);

	RADEON_WRITE(RADEON_CP_RB_BASE, ring_start);

	/* Set the write pointer delay */
	RADEON_WRITE(RADEON_CP_RB_WPTR_DELAY, 0);

	/* Initialize the ring buffer's read and write pointers */
	cur_read_ptr = RADEON_READ(RADEON_CP_RB_RPTR);
	RADEON_WRITE(RADEON_CP_RB_WPTR, cur_read_ptr);
	radeondrm_set_ring_head(dev_priv, cur_read_ptr);
	dev_priv->ring.tail = cur_read_ptr;

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		RADEON_WRITE(RADEON_CP_RB_RPTR_ADDR,
		    dev_priv->ring_rptr->offset - dev->agp->base +
		    dev_priv->gart_vm_start);
	} else
#endif
	{
		RADEON_WRITE(RADEON_CP_RB_RPTR_ADDR,
		    dev_priv->ring_rptr->offset - dev->sg->handle +
		    dev_priv->gart_vm_start);
	}

	/* Set ring buffer size */
#ifdef __BIG_ENDIAN
	RADEON_WRITE(RADEON_CP_RB_CNTL,
		     RADEON_BUF_SWAP_32BIT |
		     (dev_priv->ring.fetch_size_l2ow << 18) |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#else
	RADEON_WRITE(RADEON_CP_RB_CNTL,
		     (dev_priv->ring.fetch_size_l2ow << 18) |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#endif

	/* Initialize the scratch register pointer.  This will cause
	 * the scratch register values to be written out to memory
	 * whenever they are updated.
	 *
	 * We simply put this behind the ring read pointer, this works
	 * with PCI GART as well as (whatever kind of) AGP GART
	 */
	RADEON_WRITE(RADEON_SCRATCH_ADDR, RADEON_READ(RADEON_CP_RB_RPTR_ADDR)
		     + RADEON_SCRATCH_REG_OFFSET);

	RADEON_WRITE(RADEON_SCRATCH_UMSK, 0x7);

	radeon_enable_bm(dev_priv);

	radeondrm_write_rptr(dev_priv, RADEON_SCRATCHOFF(0), 0);
	dev_priv->sarea_priv->last_frame = 0;
	RADEON_WRITE(RADEON_LAST_FRAME_REG, dev_priv->sarea_priv->last_frame);

	radeondrm_write_rptr(dev_priv, RADEON_SCRATCHOFF(1), 0);
	dev_priv->sarea_priv->last_dispatch = 0;
	RADEON_WRITE(RADEON_LAST_DISPATCH_REG,
	    dev_priv->sarea_priv->last_dispatch);

	radeondrm_write_rptr(dev_priv, RADEON_SCRATCHOFF(2), 0);
	dev_priv->sarea_priv->last_clear = 0;
	RADEON_WRITE(RADEON_LAST_CLEAR_REG, dev_priv->sarea_priv->last_clear);

	radeon_do_wait_for_idle(dev_priv);

	/* Sync everything up */
	RADEON_WRITE(RADEON_ISYNC_CNTL,
		     (RADEON_ISYNC_ANY2D_IDLE3D |
		      RADEON_ISYNC_ANY3D_IDLE2D |
		      RADEON_ISYNC_WAIT_IDLEGUI |
		      RADEON_ISYNC_CPSCRATCH_IDLEGUI));

}

void
radeon_test_writeback(drm_radeon_private_t *dev_priv)
{
	u32 tmp;

	/* Start with assuming that writeback doesn't work */
	dev_priv->writeback_works = 0;

	/* Writeback doesn't seem to work everywhere, test it here and possibly
	 * enable it if it appears to work
	 */

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		radeondrm_write_rptr(dev_priv, R600_SCRATCHOFF(1), 0);
		RADEON_WRITE(R600_SCRATCH_REG1, 0xdeadbeef);

		for (tmp = 0; tmp < dev_priv->usec_timeout; tmp++) {
			u32 val;

			val = radeondrm_read_rptr(dev_priv, R600_SCRATCHOFF(1));
			if (val == 0xdeadbeef)
				break;
			DRM_UDELAY(1);
		}
	} else {
		radeondrm_write_rptr(dev_priv, RADEON_SCRATCHOFF(1), 0);
		RADEON_WRITE(RADEON_SCRATCH_REG1, 0xdeadbeef);

		for (tmp = 0; tmp < dev_priv->usec_timeout; tmp++) {
			if (radeondrm_read_rptr(dev_priv,
			    RADEON_SCRATCHOFF(1)) == 0xdeadbeef)
			break;
			DRM_UDELAY(1);
		}
	}

	if (tmp < dev_priv->usec_timeout) {
		dev_priv->writeback_works = 1;
		DRM_DEBUG("writeback test succeeded in %d usecs\n", tmp);
	} else {
		dev_priv->writeback_works = 0;
		DRM_INFO("writeback test failed\n");
	}
	if (radeon_no_wb == 1) {
		dev_priv->writeback_works = 0;
		DRM_INFO("writeback forced off\n");
	}

	if (!dev_priv->writeback_works) {
		/* Disable writeback to avoid unnecessary bus master transfers */
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
			RADEON_WRITE(R600_CP_RB_CNTL,
			    RADEON_READ(R600_CP_RB_CNTL) | RADEON_RB_NO_UPDATE);
			RADEON_WRITE(R600_SCRATCH_UMSK, 0);
		} else {
			RADEON_WRITE(RADEON_CP_RB_CNTL,
			    RADEON_READ(RADEON_CP_RB_CNTL) |
			    RADEON_RB_NO_UPDATE);
			RADEON_WRITE(RADEON_SCRATCH_UMSK, 0);
		}
	}
}

void
r600_page_table_cleanup(struct drm_device *dev,
    struct drm_ati_pcigart_info *gart_info)
{
	if (gart_info->bus_addr) {
		gart_info->bus_addr = 0;
		if (gart_info->gart_table_location == DRM_ATI_GART_MAIN &&
		    gart_info->tbl.dma.mem != NULL) {
			drm_dmamem_free(dev->dmat, gart_info->tbl.dma.mem);
			gart_info->tbl.dma.mem = NULL;
		}
	}
}

void
r600gart_add_entry(struct drm_ati_pcigart_info *gart_info, bus_size_t offset,
    bus_addr_t entry_addr)
{
	u_int64_t	page_base = (u_int64_t)entry_addr &
	    		    ATI_PCIGART_PAGE_MASK;
	page_base |= R600_PTE_VALID | R600_PTE_SYSTEM | R600_PTE_SNOOPED;
	page_base |= R600_PTE_READABLE | R600_PTE_WRITEABLE;
	bus_space_write_4(gart_info->tbl.fb.bst, gart_info->tbl.fb.bsh,
	    offset * sizeof(u_int64_t), (u_int32_t)page_base);
	bus_space_write_4(gart_info->tbl.fb.bst, gart_info->tbl.fb.bsh,
	    offset * sizeof(u_int64_t) + 4, upper_32_bits(page_base));
}

/* R600 has page table setup */
int r600_page_table_init(struct drm_device *dev)
{
	drm_radeon_private_t		*dev_priv = dev->dev_private;
	struct drm_ati_pcigart_info	*gart_info = &dev_priv->gart_info;
	struct drm_sg_mem		*entry = dev->sg;
	int				 i, j, max_pages, pages, gart_idx;
	bus_addr_t			 entry_addr;

	/* okay page table is available - lets rock */

	max_pages = (gart_info->table_size / sizeof(u_int64_t));
	/* convert from ati pages */
	max_pages /= (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE);
	pages = (entry->mem->map->dm_nsegs <= max_pages) ?
	    entry->mem->map->dm_nsegs : max_pages;

	bus_space_set_region_1(gart_info->tbl.fb.bst, gart_info->tbl.fb.bsh,
	    0, 0, gart_info->table_size);

	KASSERT(PAGE_SIZE >= ATI_PCIGART_PAGE_SIZE);

	for (gart_idx = 0, i = 0; i < pages; i++) {
		entry_addr = dev->sg->mem->map->dm_segs[i].ds_addr;
		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE);
		    j++, entry_addr += ATI_PCIGART_PAGE_SIZE)
			r600gart_add_entry(gart_info, gart_idx++, entry_addr);
	}
	return 0;
}

/*
 * Set up the addresses for a pcigart table, then fill it.
 */
int
radeondrm_setup_pcigart(struct drm_radeon_private *dev_priv)
{
	struct drm_ati_pcigart_info	*agi = &dev_priv->gart_info;
	struct drm_device		*dev;
	bus_addr_t			 gartaddr;
	u_int32_t			 sctrl;
	int				 ret;

	dev = (struct drm_device *)dev_priv->drmdev;

	agi->table_mask = DMA_BIT_MASK(32);

	/* if we have an offset set from userspace */
	if (dev_priv->pcigart_offset_set) {
		gartaddr = dev_priv->fb_aper_offset + dev_priv->pcigart_offset;

		agi->tbl.fb.bst = dev_priv->bst;
		/* XXX write combining */
		if ((ret = bus_space_map(agi->tbl.fb.bst, gartaddr,
		    agi->table_size, 0, &agi->tbl.fb.bsh)) != 0)
			return (ret);

		/* this is a radeon virtual address */
		agi->bus_addr = dev_priv->fb_location +
		    dev_priv->pcigart_offset;
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
			agi->gart_reg_if = DRM_ATI_GART_R600;
		} else if (dev_priv->flags & RADEON_IS_PCIE) {
			agi->gart_reg_if = DRM_ATI_GART_PCIE;
		} else {
			agi->gart_reg_if = DRM_ATI_GART_PCI;
		}
		agi->gart_table_location = DRM_ATI_GART_FB;
	} else {
		if (dev_priv->flags & RADEON_IS_PCIE ||
		    ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)) {
			DRM_ERROR("Cannot use PCI Express without GART "
			    "in FB memory\n");
			return (EINVAL);
		}
		if (dev_priv->flags & RADEON_IS_IGPGART)
			agi->gart_reg_if = DRM_ATI_GART_IGP;
		else
			agi->gart_reg_if = DRM_ATI_GART_PCI;
			
		agi->gart_table_location = DRM_ATI_GART_MAIN;

		/* pcigart_init will allocate dma memory for us */
		agi->bus_addr = 0;
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_R600) {
		/* clear surfaces while we write the gart */
		sctrl = RADEON_READ(RADEON_SURFACE_CNTL);
		RADEON_WRITE(RADEON_SURFACE_CNTL, 0);
	}
	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600 ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600))
		ret = r600_page_table_init(dev);
	else
		ret = drm_ati_pcigart_init(dev, agi);
	if ((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_R600)
		RADEON_WRITE(RADEON_SURFACE_CNTL, sctrl);
	if (ret) {
		DRM_ERROR("failed to init PCI GART!\n");
		return (ENOMEM);
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_R600 &&
	    (ret = radeon_setup_pcigart_surface(dev_priv)) != 0) {
		DRM_ERROR("failed to setup GART surface!\n");
		if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600)
			r600_page_table_cleanup(dev, &dev_priv->gart_info);
		else
			drm_ati_pcigart_cleanup(dev, &dev_priv->gart_info);
		return (ret);
	}

	/* Turn on PCI GART */
	radeon_set_pcigart(dev_priv, 1);

	return (0);
}

/* Enable or disable IGP GART on the chip */
void
radeon_set_igpgart(drm_radeon_private_t *dev_priv, int on) {
	u32 temp;

	if (on) {
		DRM_DEBUG("programming igp gart %08X %08lX %08X\n",
			 dev_priv->gart_vm_start,
			 (long)dev_priv->gart_info.bus_addr,
			 dev_priv->gart_size);

		temp = IGP_READ_MCIND(dev_priv, RS480_MC_MISC_CNTL);
		if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
		    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740))
			IGP_WRITE_MCIND(RS480_MC_MISC_CNTL, (RS480_GART_INDEX_REG_EN |
							     RS690_BLOCK_GFX_D3_EN));
		else
			IGP_WRITE_MCIND(RS480_MC_MISC_CNTL, RS480_GART_INDEX_REG_EN);

		IGP_WRITE_MCIND(RS480_AGP_ADDRESS_SPACE_SIZE, (RS480_GART_EN |
							       RS480_VA_SIZE_32MB));

		temp = IGP_READ_MCIND(dev_priv, RS480_GART_FEATURE_ID);
		IGP_WRITE_MCIND(RS480_GART_FEATURE_ID, (RS480_HANG_EN |
							RS480_TLB_ENABLE |
							RS480_GTW_LAC_EN |
							RS480_1LEVEL_GART));

		temp = dev_priv->gart_info.bus_addr & 0xfffff000;
		temp |= (upper_32_bits(dev_priv->gart_info.bus_addr) & 0xff) << 4;
		IGP_WRITE_MCIND(RS480_GART_BASE, temp);

		temp = IGP_READ_MCIND(dev_priv, RS480_AGP_MODE_CNTL);
		IGP_WRITE_MCIND(RS480_AGP_MODE_CNTL, ((1 << RS480_REQ_TYPE_SNOOP_SHIFT) |
						      RS480_REQ_TYPE_SNOOP_DIS));

		radeon_write_agp_base(dev_priv, dev_priv->gart_vm_start);

		dev_priv->gart_size = 32*1024*1024;
		temp = (((dev_priv->gart_vm_start - 1 + dev_priv->gart_size) &
		    0xffff0000) | (dev_priv->gart_vm_start >> 16));

		radeon_write_agp_location(dev_priv, temp);

		temp = IGP_READ_MCIND(dev_priv, RS480_AGP_ADDRESS_SPACE_SIZE);
		IGP_WRITE_MCIND(RS480_AGP_ADDRESS_SPACE_SIZE, (RS480_GART_EN |
							       RS480_VA_SIZE_32MB));

		do {
			temp = IGP_READ_MCIND(dev_priv, RS480_GART_CACHE_CNTRL);
			if ((temp & RS480_GART_CACHE_INVALIDATE) == 0)
				break;
			DRM_UDELAY(1);
		} while (1);

		IGP_WRITE_MCIND(RS480_GART_CACHE_CNTRL,
				RS480_GART_CACHE_INVALIDATE);

		do {
			temp = IGP_READ_MCIND(dev_priv, RS480_GART_CACHE_CNTRL);
			if ((temp & RS480_GART_CACHE_INVALIDATE) == 0)
				break;
			DRM_UDELAY(1);
		} while (1);

		IGP_WRITE_MCIND(RS480_GART_CACHE_CNTRL, 0);
	} else {
		IGP_WRITE_MCIND(RS480_AGP_ADDRESS_SPACE_SIZE, 0);
	}
}

/* Enable or disable IGP GART on the chip */
void
rs600_set_igpgart(drm_radeon_private_t *dev_priv, int on)
{
	u32 temp;
	int i;

	if (on) {
		DRM_DEBUG("programming igp gart %08X %08lX %08X\n",
			 dev_priv->gart_vm_start,
			 (long)dev_priv->gart_info.bus_addr,
			 dev_priv->gart_size);

		IGP_WRITE_MCIND(RS600_MC_PT0_CNTL, (RS600_EFFECTIVE_L2_CACHE_SIZE(6) |
						    RS600_EFFECTIVE_L2_QUEUE_SIZE(6)));

		for (i = 0; i < 19; i++)
			IGP_WRITE_MCIND(RS600_MC_PT0_CLIENT0_CNTL + i,
					(RS600_ENABLE_TRANSLATION_MODE_OVERRIDE |
					 RS600_SYSTEM_ACCESS_MODE_IN_SYS |
					 RS600_SYSTEM_APERTURE_UNMAPPED_ACCESS_PASSTHROUGH |
					 RS600_EFFECTIVE_L1_CACHE_SIZE(3) |
					 RS600_ENABLE_FRAGMENT_PROCESSING |
					 RS600_EFFECTIVE_L1_QUEUE_SIZE(3)));

		IGP_WRITE_MCIND(RS600_MC_PT0_CONTEXT0_CNTL, (RS600_ENABLE_PAGE_TABLE |
							     RS600_PAGE_TABLE_TYPE_FLAT));

		/* disable all other contexts */
		for (i = 1; i < 8; i++)
			IGP_WRITE_MCIND(RS600_MC_PT0_CONTEXT0_CNTL + i, 0);

		/* setup the page table aperture */
		IGP_WRITE_MCIND(RS600_MC_PT0_CONTEXT0_FLAT_BASE_ADDR,
				dev_priv->gart_info.bus_addr);
		IGP_WRITE_MCIND(RS600_MC_PT0_CONTEXT0_FLAT_START_ADDR,
				dev_priv->gart_vm_start);
		IGP_WRITE_MCIND(RS600_MC_PT0_CONTEXT0_FLAT_END_ADDR,
				(dev_priv->gart_vm_start + dev_priv->gart_size - 1));
		IGP_WRITE_MCIND(RS600_MC_PT0_CONTEXT0_DEFAULT_READ_ADDR, 0);

		/* setup the system aperture */
		IGP_WRITE_MCIND(RS600_MC_PT0_SYSTEM_APERTURE_LOW_ADDR,
				dev_priv->gart_vm_start);
		IGP_WRITE_MCIND(RS600_MC_PT0_SYSTEM_APERTURE_HIGH_ADDR,
				(dev_priv->gart_vm_start + dev_priv->gart_size - 1));

		/* enable page tables */
		temp = IGP_READ_MCIND(dev_priv, RS600_MC_PT0_CNTL);
		IGP_WRITE_MCIND(RS600_MC_PT0_CNTL, (temp | RS600_ENABLE_PT));

		temp = IGP_READ_MCIND(dev_priv, RS600_MC_CNTL1);
		IGP_WRITE_MCIND(RS600_MC_CNTL1, (temp | RS600_ENABLE_PAGE_TABLES));

		/* invalidate the cache */
		temp = IGP_READ_MCIND(dev_priv, RS600_MC_PT0_CNTL);

		temp &= ~(RS600_INVALIDATE_ALL_L1_TLBS | RS600_INVALIDATE_L2_CACHE);
		IGP_WRITE_MCIND(RS600_MC_PT0_CNTL, temp);
		temp = IGP_READ_MCIND(dev_priv, RS600_MC_PT0_CNTL);

		temp |= RS600_INVALIDATE_ALL_L1_TLBS | RS600_INVALIDATE_L2_CACHE;
		IGP_WRITE_MCIND(RS600_MC_PT0_CNTL, temp);
		temp = IGP_READ_MCIND(dev_priv, RS600_MC_PT0_CNTL);

		temp &= ~(RS600_INVALIDATE_ALL_L1_TLBS | RS600_INVALIDATE_L2_CACHE);
		IGP_WRITE_MCIND(RS600_MC_PT0_CNTL, temp);
		temp = IGP_READ_MCIND(dev_priv, RS600_MC_PT0_CNTL);

	} else {
		IGP_WRITE_MCIND(RS600_MC_PT0_CNTL, 0);
		temp = IGP_READ_MCIND(dev_priv, RS600_MC_CNTL1);
		temp &= ~RS600_ENABLE_PAGE_TABLES;
		IGP_WRITE_MCIND(RS600_MC_CNTL1, temp);
	}
}

void
radeon_set_pciegart(drm_radeon_private_t *dev_priv, int on)
{
	u32 tmp = RADEON_READ_PCIE(dev_priv, RADEON_PCIE_TX_GART_CNTL);
	if (on) {

		DRM_DEBUG("programming pcie %08X %08lX %08X\n",
			  dev_priv->gart_vm_start,
			  (long)dev_priv->gart_info.bus_addr,
			  dev_priv->gart_size);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_DISCARD_RD_ADDR_LO,
				  dev_priv->gart_vm_start);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_BASE,
				  dev_priv->gart_info.bus_addr);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_START_LO,
				  dev_priv->gart_vm_start);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_END_LO,
				  dev_priv->gart_vm_start +
				  dev_priv->gart_size - 1);

		radeon_write_agp_location(dev_priv, 0xffffffc0); /* ?? */

		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_CNTL,
				  RADEON_PCIE_TX_GART_EN);
	} else {
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_CNTL,
				  tmp & ~RADEON_PCIE_TX_GART_EN);
	}
}

/* Enable or disable PCI GART on the chip */
void
radeon_set_pcigart(drm_radeon_private_t *dev_priv, int on)
{
	u32 tmp;

	
	if (((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)) {
		if (on)
			r700_vm_init(dev_priv);
		return;
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		if (on)
			r600_vm_init(dev_priv);
		return;
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740) ||
	    (dev_priv->flags & RADEON_IS_IGPGART)) {
		radeon_set_igpgart(dev_priv, on);
		return;
	} else

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS600) {
		rs600_set_igpgart(dev_priv, on);
		return;
	}

	if (dev_priv->flags & RADEON_IS_PCIE) {
		radeon_set_pciegart(dev_priv, on);
		return;
	}

	tmp = RADEON_READ(RADEON_AIC_CNTL);

	if (on) {
		RADEON_WRITE(RADEON_AIC_CNTL,
			     tmp | RADEON_PCIGART_TRANSLATE_EN);

		/* set PCI GART page-table base address
		 */
		RADEON_WRITE(RADEON_AIC_PT_BASE, dev_priv->gart_info.bus_addr);

		/* set address range for PCI address translate
		 */
		RADEON_WRITE(RADEON_AIC_LO_ADDR, dev_priv->gart_vm_start);
		RADEON_WRITE(RADEON_AIC_HI_ADDR, dev_priv->gart_vm_start
			     + dev_priv->gart_size - 1);

		/* Turn off AGP aperture -- is this required for PCI GART?
		 */
		radeon_write_agp_location(dev_priv, 0xffffffc0);
		RADEON_WRITE(RADEON_AGP_COMMAND, 0);	/* clear AGP_COMMAND */
	} else {
		RADEON_WRITE(RADEON_AIC_CNTL,
			     tmp & ~RADEON_PCIGART_TRANSLATE_EN);
	}
}

int
radeon_setup_pcigart_surface(drm_radeon_private_t *dev_priv)
{
	struct drm_ati_pcigart_info *gart_info = &dev_priv->gart_info;
	struct radeon_virt_surface *vp;
	int i;

	for (i = 0; i < RADEON_MAX_SURFACES * 2; i++) {
		if (!dev_priv->virt_surfaces[i].file_priv ||
		    dev_priv->virt_surfaces[i].file_priv == PCIGART_FILE_PRIV)
			break;
	}
	if (i >= 2 * RADEON_MAX_SURFACES)
		return ENOMEM;
	vp = &dev_priv->virt_surfaces[i];

	for (i = 0; i < RADEON_MAX_SURFACES; i++) {
		struct radeon_surface *sp = &dev_priv->surfaces[i];
		if (sp->refcount)
			continue;

		vp->surface_index = i;
		vp->lower = gart_info->bus_addr;
		vp->upper = vp->lower + gart_info->table_size;
		vp->flags = 0;
		vp->file_priv = PCIGART_FILE_PRIV;

		sp->refcount = 1;
		sp->lower = vp->lower;
		sp->upper = vp->upper;
		sp->flags = 0;

		RADEON_WRITE(RADEON_SURFACE0_INFO + 16 * i, sp->flags);
		RADEON_WRITE(RADEON_SURFACE0_LOWER_BOUND + 16 * i, sp->lower);
		RADEON_WRITE(RADEON_SURFACE0_UPPER_BOUND + 16 * i, sp->upper);
		return 0;
	}

	return ENOMEM;
}

int
radeon_do_init_cp(struct drm_device *dev, drm_radeon_init_t *init)
{
	drm_radeon_private_t	*dev_priv = dev->dev_private;
	u_int32_t		 ssz;
	int			 lshift, sshift;


	DRM_DEBUG("\n");

	/* if we require new memory map but we don't have it fail */
	if ((dev_priv->flags & RADEON_NEW_MEMMAP) && !dev_priv->new_memmap) {
		DRM_ERROR("Cannot initialise DRM on this card\nThis card requires a new X.org DDX for 3D\n");
		radeon_do_cleanup_cp(dev);
		return EINVAL;
	}

	if (init->is_pci && (dev_priv->flags & RADEON_IS_AGP)) {
		DRM_DEBUG("Forcing AGP card to PCI mode\n");
		dev_priv->flags &= ~RADEON_IS_AGP;
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
			/* The writeback test succeeds, but when writeback is
			 * enabled, the ring buffer read ptr update fails
			 * after first 128 bytes.
			 */
			radeon_no_wb = 1;
		}
	} else if (!(dev_priv->flags & (RADEON_IS_AGP | RADEON_IS_PCI | RADEON_IS_PCIE))
		 && !init->is_pci) {
		DRM_DEBUG("Restoring AGP flag\n");
		dev_priv->flags |= RADEON_IS_AGP;
	}

	if ((!(dev_priv->flags & RADEON_IS_AGP)) && !dev->sg) {
		DRM_ERROR("PCI GART memory not allocated!\n");
		radeon_do_cleanup_cp(dev);
		return EINVAL;
	}

	dev_priv->usec_timeout = init->usec_timeout;
	if (dev_priv->usec_timeout < 1 ||
	    dev_priv->usec_timeout > RADEON_MAX_USEC_TIMEOUT) {
		DRM_DEBUG("TIMEOUT problem!\n");
		radeon_do_cleanup_cp(dev);
		return EINVAL;
	}

	dev_priv->cp_mode = init->cp_mode;

	/* We don't support anything other than bus-mastering ring mode,
	 * but the ring can be in either AGP or PCI space for the ring
	 * read pointer.
	 */
	if ((init->cp_mode != RADEON_CSQ_PRIBM_INDDIS) &&
	    (init->cp_mode != RADEON_CSQ_PRIBM_INDBM)) {
		DRM_DEBUG("BAD cp_mode (%x)!\n", init->cp_mode);
		radeon_do_cleanup_cp(dev);
		return EINVAL;
	}

	switch (init->fb_bpp) {
	case 16:
		dev_priv->color_fmt = RADEON_COLOR_FORMAT_RGB565;
		break;
	case 32:
	default:
		dev_priv->color_fmt = RADEON_COLOR_FORMAT_ARGB8888;
		break;
	}
	dev_priv->front_offset = init->front_offset;
	dev_priv->front_pitch = init->front_pitch;
	dev_priv->back_offset = init->back_offset;
	dev_priv->back_pitch = init->back_pitch;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_R600) {
		switch (init->depth_bpp) {
		case 16:
			dev_priv->depth_fmt = RADEON_DEPTH_FORMAT_16BIT_INT_Z;
			break;
		case 32:
		default:
			dev_priv->depth_fmt = RADEON_DEPTH_FORMAT_24BIT_INT_Z;
			break;
		}
		dev_priv->depth_offset = init->depth_offset;
		dev_priv->depth_pitch = init->depth_pitch;

			/* Hardware state for depth clears.  Remove this
			 * if/when we no longer clear the depth buffer with
			 * a 3D rectangle. Hard-code all values to prevent
			 * unwanted 3D state from slipping through and
			 * screwing with the clear operation.
			 */
			dev_priv->depth_clear.rb3d_cntl =
			    (RADEON_PLANE_MASK_ENABLE |
			    (dev_priv->color_fmt << 10) |
			    (dev_priv->chip_family < CHIP_R200 ?
			    RADEON_ZBLOCK16 : 0));

			dev_priv->depth_clear.rb3d_zstencilcntl =
			    (dev_priv->depth_fmt | RADEON_Z_TEST_ALWAYS |
			    RADEON_STENCIL_TEST_ALWAYS |
			    RADEON_STENCIL_S_FAIL_REPLACE |
			    RADEON_STENCIL_ZPASS_REPLACE |
			    RADEON_STENCIL_ZFAIL_REPLACE |
			    RADEON_Z_WRITE_ENABLE);

			dev_priv->depth_clear.se_cntl = (RADEON_FFACE_CULL_CW |
			    RADEON_BFACE_SOLID | RADEON_FFACE_SOLID |
			    RADEON_FLAT_SHADE_VTX_LAST |
			    RADEON_DIFFUSE_SHADE_FLAT |
			    RADEON_ALPHA_SHADE_FLAT |
			    RADEON_SPECULAR_SHADE_FLAT |
			    RADEON_FOG_SHADE_FLAT |
			    RADEON_VTX_PIX_CENTER_OGL |
			    RADEON_ROUND_MODE_TRUNC |
			    RADEON_ROUND_PREC_8TH_PIX);
	}

	dev_priv->ring_offset = init->ring_offset;
	dev_priv->ring_rptr_offset = init->ring_rptr_offset;
	dev_priv->buffers_offset = init->buffers_offset;
	dev_priv->gart_textures_offset = init->gart_textures_offset;

	dev_priv->sarea = drm_getsarea(dev);
	if (!dev_priv->sarea) {
		DRM_ERROR("could not find sarea!\n");
		radeon_do_cleanup_cp(dev);
		/* XXX */
		return EINVAL;
	}

	dev_priv->cp_ring = drm_core_findmap(dev, init->ring_offset);
	if (!dev_priv->cp_ring) {
		DRM_ERROR("could not find cp ring region!\n");
		radeon_do_cleanup_cp(dev);
		return EINVAL;
	}
	dev_priv->ring_rptr = drm_core_findmap(dev, init->ring_rptr_offset);
	if (!dev_priv->ring_rptr) {
		DRM_ERROR("could not find ring read pointer!\n");
		radeon_do_cleanup_cp(dev);
		return EINVAL;
	}
	dev->agp_buffer_map = drm_core_findmap(dev, init->buffers_offset);
	if (!dev->agp_buffer_map) {
		DRM_ERROR("could not find dma buffer region!\n");
		radeon_do_cleanup_cp(dev);
		return EINVAL;
	}

	if (init->gart_textures_offset) {
		dev_priv->gart_textures =
		    drm_core_findmap(dev, init->gart_textures_offset);
		if (!dev_priv->gart_textures) {
			DRM_ERROR("could not find GART texture region!\n");
			radeon_do_cleanup_cp(dev);
			return EINVAL;
		}
	}

	dev_priv->sarea_priv =
	    (drm_radeon_sarea_t *)((u8 *)dev_priv->sarea->handle +
	    init->sarea_priv_offset);

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		/* XXX WC */
		drm_core_ioremap(dev_priv->cp_ring, dev);
		drm_core_ioremap(dev_priv->ring_rptr, dev);
		drm_core_ioremap(dev->agp_buffer_map, dev);
		if (!dev_priv->cp_ring->handle ||
		    !dev_priv->ring_rptr->handle ||
		    !dev->agp_buffer_map->handle) {
			DRM_ERROR("could not find ioremap agp regions!\n");
			radeon_do_cleanup_cp(dev);
			return EINVAL;
		}
	} else
#endif
	{
		dev_priv->cp_ring->handle = (void *)dev_priv->cp_ring->offset;
		dev_priv->ring_rptr->handle =
		    (void *)dev_priv->ring_rptr->offset;
		dev->agp_buffer_map->handle =
		    (void *)dev->agp_buffer_map->offset;

		DRM_DEBUG("dev_priv->cp_ring->handle %p\n",
			  dev_priv->cp_ring->handle);
		DRM_DEBUG("dev_priv->ring_rptr->handle %p\n",
			  dev_priv->ring_rptr->handle);
		DRM_DEBUG("dev->agp_buffer_map->handle %p\n",
			  dev->agp_buffer_map->handle);
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		lshift = 24;
		sshift = 8;
		ssz = 0x1000000;
	} else {
		lshift = 16;
		sshift = 0;
		ssz = 0x10000;
	}

	dev_priv->fb_location = (radeon_read_fb_location(dev_priv) & 0xffff) <<
	   lshift;
	dev_priv->fb_size =
	    (((radeon_read_fb_location(dev_priv) & 0xffff0000u) << sshift) +
	    ssz) - dev_priv->fb_location;

	dev_priv->front_pitch_offset = (((dev_priv->front_pitch / 64) << 22) |
					((dev_priv->front_offset
					  + dev_priv->fb_location) >> 10));

	dev_priv->back_pitch_offset = (((dev_priv->back_pitch / 64) << 22) |
				       ((dev_priv->back_offset
					 + dev_priv->fb_location) >> 10));

	dev_priv->depth_pitch_offset = (((dev_priv->depth_pitch / 64) << 22) |
					((dev_priv->depth_offset
					  + dev_priv->fb_location) >> 10));

	dev_priv->gart_size = init->gart_size;

	/* New let's set the memory map ... */
	if (dev_priv->new_memmap) {
		u32 base = 0;

		DRM_DEBUG("Setting GART location based on new memory map\n");

		/* If using AGP, try to locate the AGP aperture at the same
		 * location in the card and on the bus, though we have to
		 * align it down.
		 */
#if __OS_HAS_AGP
		if (dev_priv->flags & RADEON_IS_AGP) {
			base = dev->agp->base;
			/* Check if valid */
			if ((base + dev_priv->gart_size - 1) >= dev_priv->fb_location &&
			    base < (dev_priv->fb_location + dev_priv->fb_size - 1)) {
				DRM_INFO("Can't use AGP base @0x%08lx, won't fit\n",
					 dev->agp->base);
				base = 0;
			}
		}
#endif
		/* If not or if AGP is at 0 (Macs), try to put it elsewhere */
		if (base == 0) {
			base = dev_priv->fb_location + dev_priv->fb_size;
			if (base < dev_priv->fb_location ||
			    ((base + dev_priv->gart_size) & 0xfffffffful) < base)
				base = dev_priv->fb_location
					- dev_priv->gart_size;
		}
		dev_priv->gart_vm_start = base & 0xffc00000u;
		if (dev_priv->gart_vm_start != base)
			DRM_INFO("GART aligned down from 0x%08x to 0x%08x\n",
				 base, dev_priv->gart_vm_start);
	} else {
		DRM_DEBUG("Setting GART location based on old memory map\n");
		dev_priv->gart_vm_start = dev_priv->fb_location +
			RADEON_READ(RADEON_CONFIG_APER_SIZE);
	}

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP)
		dev_priv->gart_buffers_offset = (dev->agp_buffer_map->offset -
		    dev->agp->base + dev_priv->gart_vm_start);
	else
#endif
		dev_priv->gart_buffers_offset = (dev->agp_buffer_map->offset -
		    dev->sg->handle + dev_priv->gart_vm_start);

	DRM_DEBUG("fb 0x%08x size %d\n",
		  (unsigned int) dev_priv->fb_location,
		  (unsigned int) dev_priv->fb_size);
	DRM_DEBUG("dev_priv->gart_size %d\n", dev_priv->gart_size);
	DRM_DEBUG("dev_priv->gart_vm_start 0x%x\n", dev_priv->gart_vm_start);
	DRM_DEBUG("dev_priv->gart_buffers_offset 0x%lx\n",
		  dev_priv->gart_buffers_offset);

	dev_priv->ring.start = (u_int32_t *)dev_priv->cp_ring->handle;
	dev_priv->ring.size = init->ring_size / sizeof(u_int32_t);
	dev_priv->ring.end = ((u_int32_t *)dev_priv->cp_ring->handle +
	    dev_priv->ring.size);
	dev_priv->ring.tail_mask = dev_priv->ring.size - 1;

	/* Parameters for ringbuffer initialisation */
	dev_priv->ring.size_l2qw = drm_order(init->ring_size / 8);
	dev_priv->ring.rptr_update_l2qw = drm_order( /* init->rptr_update */
	    4096 / 8);
	dev_priv->ring.fetch_size_l2ow = drm_order( /* init->fetch_size */
	    32 / 16);

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		/* Turn off PCI GART */
		radeon_set_pcigart(dev_priv, 0);
	} else
#endif
	{
		if (radeondrm_setup_pcigart(dev_priv) != 0) {
			radeon_do_cleanup_cp(dev);
			return EINVAL;
		}
	}

	radeon_cp_load_microcode(dev_priv);
	radeon_cp_init_ring_buffer(dev, dev_priv);

	dev_priv->last_buf = 0;

	radeon_do_engine_reset(dev);
	radeon_test_writeback(dev_priv);

	return 0;
}

int
radeon_do_cleanup_cp(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		if (dev_priv->cp_ring != NULL)
			drm_core_ioremapfree(dev_priv->cp_ring);
		if (dev_priv->ring_rptr != NULL)
			drm_core_ioremapfree(dev_priv->ring_rptr);
		if (dev->agp_buffer_map != NULL)
			drm_core_ioremapfree(dev->agp_buffer_map);
	} else
#endif
	{

		if (dev_priv->gart_info.bus_addr) {
			/* Turn off PCI GART */
			radeon_set_pcigart(dev_priv, 0);
			if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600
			    || (dev_priv->flags & RADEON_FAMILY_MASK) ==
			    CHIP_RS600) {
				r600_page_table_cleanup(dev,
				    &dev_priv->gart_info);
			} else if (drm_ati_pcigart_cleanup(dev,
			    &dev_priv->gart_info)) {
				DRM_ERROR("failed to cleanup PCI GART!\n");
			}
		}

		if (dev_priv->gart_info.gart_table_location ==
		    DRM_ATI_GART_FB && dev_priv->gart_info.tbl.fb.bst != 0)
			bus_space_unmap(dev_priv->gart_info.tbl.fb.bst,
			    dev_priv->gart_info.tbl.fb.bsh,
			    dev_priv->gart_info.table_size);
		memset(&dev_priv->gart_info.tbl, 0,
		    sizeof(dev_priv->gart_info.tbl));
	}
	dev->agp_buffer_map = dev_priv->ring_rptr = dev_priv->cp_ring = NULL;

	return 0;
}

int
radeon_cp_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_init_t 		*init = data;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (init->func == RADEON_INIT_R300_CP)
		r300_init_reg_flags(dev);

	switch (init->func) {
	case RADEON_INIT_CP:
	case RADEON_INIT_R200_CP:
	case RADEON_INIT_R300_CP:
	case RADEON_INIT_R600_CP:
		return radeon_do_init_cp(dev, init);
	case RADEON_CLEANUP_CP:
		return radeon_do_cleanup_cp(dev);
	}

	return EINVAL;
}

int
radeon_cp_start(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (dev_priv->cp_running) {
		DRM_DEBUG("while CP running\n");
		return 0;
	}
	if (dev_priv->cp_mode == RADEON_CSQ_PRIDIS_INDDIS) {
		DRM_DEBUG("called with bogus CP mode (%d)\n",
			  dev_priv->cp_mode);
		return 0;
	}

	radeon_do_cp_start(dev_priv);

	return 0;
}

/* Stop the CP.  The engine must have been idled before calling this
 * routine.
 */
int
radeon_cp_stop(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_cp_stop_t *stop = data;
	int ret;
	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (!dev_priv->cp_running)
		return 0;

	/* If we fail to make the engine go idle, we return an error
	 * code so that the DRM ioctl wrapper can try again.
	 */
	if (stop->idle) {
		ret = radeon_do_cp_idle(dev_priv);
		if (ret)
			return ret;
	}

	/* Finally, we can turn off the CP.  If the engine isn't idle,
	 * we will get some dropped triangles as they won't be fully
	 * rendered before the CP is shut down.
	 *
	 * After turning off we reset the engine.
	 */
	radeon_do_cp_stop(dev_priv);
	radeon_do_engine_reset(dev);

	return 0;
}

void
radeon_driver_lastclose(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i, ret;

	radeon_surfaces_release(PCIGART_FILE_PRIV, dev->dev_private);

	if (dev_priv->cp_running) {
		/* Stop the cp */
		while ((ret = radeon_do_cp_idle(dev_priv)) != 0) {
			DRM_DEBUG("radeon_do_cp_idle %d\n", ret);
			tsleep(&ret, PZERO, "rdnrel", 1);
		}
		radeon_do_cp_stop(dev_priv);
		radeon_do_engine_reset(dev);
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_R600) {
		/* Disable *all* interrupts */
		RADEON_WRITE(RADEON_GEN_INT_CNTL, 0);

		/* remove all surfaces */
		for (i = 0; i < RADEON_MAX_SURFACES; i++) {
			RADEON_WRITE(RADEON_SURFACE0_INFO + 16 * i, 0);
			RADEON_WRITE(RADEON_SURFACE0_LOWER_BOUND + 16 * i, 0);
			RADEON_WRITE(RADEON_SURFACE0_UPPER_BOUND + 16 * i, 0);
			bzero(&dev_priv->surfaces[i],
			    sizeof(struct radeon_surface));
		}
	}

	/* Free memory heap structures */
	drm_mem_takedown(&dev_priv->gart_heap);
	drm_mem_takedown(&dev_priv->fb_heap);

	radeon_do_cleanup_cp(dev);
}

/* Just reset the CP ring.  Called as part of an X Server engine reset.
 */
int
radeon_cp_reset(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (!dev_priv) {
		DRM_DEBUG("called before init done\n");
		return EINVAL;
	}

	radeon_do_cp_reset(dev_priv);

	/* The CP is no longer running after an engine reset */
	dev_priv->cp_running = 0;

	return 0;
}

int
radeon_cp_idle(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	return radeon_do_cp_idle(dev_priv);
}

/*
 * This code will reinit the Radeon CP hardware after a resume from disc.
 * AFAIK, it would be very difficult to pickle the state at suspend time, so
 * here we make sure that all Radeon hardware initialisation is re-done without
 * affecting running applications.
 *
 * Charl P. Botha <http://cpbotha.net>
 */
int
radeon_cp_resume(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if (!dev_priv) {
		DRM_ERROR("Called with no initialization\n");
		return EINVAL;
	}

	DRM_DEBUG("Starting radeon_cp_resume()\n");

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		/* Turn off PCI GART */
		radeon_set_pcigart(dev_priv, 0);
	} else
#endif
	{
		/* Turn on PCI GART */
		radeon_set_pcigart(dev_priv, 1);
	}

	radeon_cp_load_microcode(dev_priv);
	radeon_cp_init_ring_buffer(dev, dev_priv);

	radeon_do_engine_reset(dev);
	if ((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_R600)
		radeon_irq_set_state(dev, RADEON_SW_INT_ENABLE, 1);

	DRM_DEBUG("radeon_cp_resume() complete\n");

	return 0;
}

/* ================================================================
 * Freelist management
 */

/* Original comment: FIXME: ROTATE_BUFS is a hack to cycle through
 *   bufs until freelist code is used.  Note this hides a problem with
 *   the scratch register * (used to keep track of last buffer
 *   completed) being written to before * the last buffer has actually
 *   completed rendering.
 *
 * KW:  It's also a good way to find free buffers quickly.
 *
 * KW: Ideally this loop wouldn't exist, and freelist_get wouldn't
 * sleep.  However, bugs in older versions of radeon_accel.c mean that
 * we essentially have to do this, else old clients will break.
 *
 * However, it does leave open a potential deadlock where all the
 * buffers are held by other clients, which can't release them because
 * they can't get the lock.
 */

struct drm_buf *
radeon_freelist_get(struct drm_device *dev)
{
	struct drm_device_dma *dma = dev->dma;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv;
	struct drm_buf *buf;
	int i, t;
	int start;

	if (++dev_priv->last_buf >= dma->buf_count)
		dev_priv->last_buf = 0;

	start = dev_priv->last_buf;

	for (t = 0; t < dev_priv->usec_timeout; t++) {
		u_int32_t done_age = radeondrm_get_scratch(dev_priv, 1);

		DRM_DEBUG("done_age = %d\n", done_age);
		for (i = 0; i < dma->buf_count; i++) {
			buf = dma->buflist[start];
			buf_priv = buf->dev_private;
			if (buf->file_priv == NULL || (buf->pending &&
			    buf_priv->age <= done_age)) {
				buf->pending = 0;
				return buf;
			}
			if (++start >=dma->buf_count)
				start = 0;
		}

		if (t) {
			DRM_UDELAY(1);
		}
	}

	return NULL;
}

void
radeon_freelist_reset(struct drm_device *dev)
{
	struct drm_device_dma *dma = dev->dma;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i;

	dev_priv->last_buf = 0;
	for (i = 0; i < dma->buf_count; i++) {
		struct drm_buf *buf = dma->buflist[i];
		drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
		buf_priv->age = 0;
	}
}

/* ================================================================
 * CP command submission
 */

int
radeon_wait_ring(drm_radeon_private_t *dev_priv, int n)
{
	drm_radeon_ring_buffer_t	*ring = &dev_priv->ring;
	u_int32_t			 last_head;
	int				 i;

	last_head = radeondrm_get_ring_head(dev_priv);

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		u_int32_t head = radeondrm_get_ring_head(dev_priv);

		ring->space = head - ring->tail;
		if (ring->space <= 0)
			ring->space += ring->size;
		if (ring->space > n)
			return 0;

		if (head != last_head)
			i = 0;
		last_head = head;

		DRM_UDELAY(1);
	}

	/* FIXME: This return value is ignored in the BEGIN_RING macro! */
#if RADEON_FIFO_DEBUG
	radeon_status(dev_priv);
	DRM_ERROR("failed!\n");
#endif
	return EBUSY;
}

int
radeon_cp_buffers(struct drm_device *dev, struct drm_dma * d,
    struct drm_file *file_priv)
{
	int i;
	struct drm_buf *buf;

	for (i = d->granted_count; i < d->request_count; i++) {
		buf = radeon_freelist_get(dev);
		if (!buf)
			return EBUSY;	/* NOTE: broken client */

		buf->file_priv = file_priv;

		if (DRM_COPY_TO_USER(&d->request_indices[i], &buf->idx,
				     sizeof(buf->idx)))
			return EFAULT;
		if (DRM_COPY_TO_USER(&d->request_sizes[i], &buf->total,
				     sizeof(buf->total)))
			return EFAULT;

		d->granted_count++;
	}
	return 0;
}

/* Create mappings for registers and framebuffer so userland doesn't necessarily
 * have to find them.
 */
int
radeon_driver_firstopen(struct drm_device *dev)
{
	drm_radeon_private_t	*dev_priv = dev->dev_private;
	struct drm_local_map	*map;
	int			 ret;

	dev_priv->gart_info.table_size = RADEON_PCIGART_TABLE_SIZE;

	ret = drm_addmap(dev, dev_priv->fb_aper_offset, dev_priv->fb_aper_size,
	    _DRM_FRAME_BUFFER, _DRM_WRITE_COMBINING, &map);
	if (ret != 0)
		return ret;

	return 0;
}
