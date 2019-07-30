/*	$OpenBSD: evergreen_blit_kms.c,v 1.3 2015/04/18 14:47:35 jsg Exp $	*/
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
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     Alex Deucher <alexander.deucher@amd.com>
 */

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/radeon_drm.h>
#include "radeon.h"

#include "evergreend.h"
#include "evergreen_blit_shaders.h"
#include "cayman_blit_shaders.h"
#include "radeon_blit_common.h"

/* emits 17 */
static void
set_render_target(struct radeon_device *rdev, int format,
		  int w, int h, u64 gpu_addr)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	u32 cb_color_info;
	int pitch, slice;

	h = roundup2(h, 8);
	if (h < 8)
		h = 8;

	cb_color_info = CB_FORMAT(format) |
		CB_SOURCE_FORMAT(CB_SF_EXPORT_NORM) |
		CB_ARRAY_MODE(ARRAY_1D_TILED_THIN1);
	pitch = (w / 8) - 1;
	slice = ((w * h) / 64) - 1;

	radeon_ring_write(ring, PACKET3(PACKET3_SET_CONTEXT_REG, 15));
	radeon_ring_write(ring, (CB_COLOR0_BASE - PACKET3_SET_CONTEXT_REG_START) >> 2);
	radeon_ring_write(ring, gpu_addr >> 8);
	radeon_ring_write(ring, pitch);
	radeon_ring_write(ring, slice);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, cb_color_info);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, (w - 1) | ((h - 1) << 16));
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
}

/* emits 5dw */
static void
cp_set_surface_sync(struct radeon_device *rdev,
		    u32 sync_type, u32 size,
		    u64 mc_addr)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	u32 cp_coher_size;

	if (size == 0xffffffff)
		cp_coher_size = 0xffffffff;
	else
		cp_coher_size = ((size + 255) >> 8);

	if (rdev->family >= CHIP_CAYMAN) {
		/* CP_COHER_CNTL2 has to be set manually when submitting a surface_sync
		 * to the RB directly. For IBs, the CP programs this as part of the
		 * surface_sync packet.
		 */
		radeon_ring_write(ring, PACKET3(PACKET3_SET_CONFIG_REG, 1));
		radeon_ring_write(ring, (0x85e8 - PACKET3_SET_CONFIG_REG_START) >> 2);
		radeon_ring_write(ring, 0); /* CP_COHER_CNTL2 */
	}
	radeon_ring_write(ring, PACKET3(PACKET3_SURFACE_SYNC, 3));
	radeon_ring_write(ring, sync_type);
	radeon_ring_write(ring, cp_coher_size);
	radeon_ring_write(ring, mc_addr >> 8);
	radeon_ring_write(ring, 10); /* poll interval */
}

/* emits 11dw + 1 surface sync = 16dw */
static void
set_shaders(struct radeon_device *rdev)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	u64 gpu_addr;

	/* VS */
	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.vs_offset;
	radeon_ring_write(ring, PACKET3(PACKET3_SET_CONTEXT_REG, 3));
	radeon_ring_write(ring, (SQ_PGM_START_VS - PACKET3_SET_CONTEXT_REG_START) >> 2);
	radeon_ring_write(ring, gpu_addr >> 8);
	radeon_ring_write(ring, 2);
	radeon_ring_write(ring, 0);

	/* PS */
	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.ps_offset;
	radeon_ring_write(ring, PACKET3(PACKET3_SET_CONTEXT_REG, 4));
	radeon_ring_write(ring, (SQ_PGM_START_PS - PACKET3_SET_CONTEXT_REG_START) >> 2);
	radeon_ring_write(ring, gpu_addr >> 8);
	radeon_ring_write(ring, 1);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 2);

	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.vs_offset;
	cp_set_surface_sync(rdev, PACKET3_SH_ACTION_ENA, 512, gpu_addr);
}

/* emits 10 + 1 sync (5) = 15 */
static void
set_vtx_resource(struct radeon_device *rdev, u64 gpu_addr)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	u32 sq_vtx_constant_word2, sq_vtx_constant_word3;

	/* high addr, stride */
	sq_vtx_constant_word2 = SQ_VTXC_BASE_ADDR_HI(upper_32_bits(gpu_addr) & 0xff) |
		SQ_VTXC_STRIDE(16);
#ifdef __BIG_ENDIAN
	sq_vtx_constant_word2 |= SQ_VTXC_ENDIAN_SWAP(SQ_ENDIAN_8IN32);
#endif
	/* xyzw swizzles */
	sq_vtx_constant_word3 = SQ_VTCX_SEL_X(SQ_SEL_X) |
		SQ_VTCX_SEL_Y(SQ_SEL_Y) |
		SQ_VTCX_SEL_Z(SQ_SEL_Z) |
		SQ_VTCX_SEL_W(SQ_SEL_W);

	radeon_ring_write(ring, PACKET3(PACKET3_SET_RESOURCE, 8));
	radeon_ring_write(ring, 0x580);
	radeon_ring_write(ring, gpu_addr & 0xffffffff);
	radeon_ring_write(ring, 48 - 1); /* size */
	radeon_ring_write(ring, sq_vtx_constant_word2);
	radeon_ring_write(ring, sq_vtx_constant_word3);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, S__SQ_CONSTANT_TYPE(SQ_TEX_VTX_VALID_BUFFER));

	if ((rdev->family == CHIP_CEDAR) ||
	    (rdev->family == CHIP_PALM) ||
	    (rdev->family == CHIP_SUMO) ||
	    (rdev->family == CHIP_SUMO2) ||
	    (rdev->family == CHIP_CAICOS))
		cp_set_surface_sync(rdev,
				    PACKET3_TC_ACTION_ENA, 48, gpu_addr);
	else
		cp_set_surface_sync(rdev,
				    PACKET3_VC_ACTION_ENA, 48, gpu_addr);

}

/* emits 10 */
static void
set_tex_resource(struct radeon_device *rdev,
		 int format, int w, int h, int pitch,
		 u64 gpu_addr, u32 size)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	u32 sq_tex_resource_word0, sq_tex_resource_word1;
	u32 sq_tex_resource_word4, sq_tex_resource_word7;

	if (h < 1)
		h = 1;

	sq_tex_resource_word0 = TEX_DIM(SQ_TEX_DIM_2D);
	sq_tex_resource_word0 |= ((((pitch >> 3) - 1) << 6) |
				  ((w - 1) << 18));
	sq_tex_resource_word1 = ((h - 1) << 0) |
				TEX_ARRAY_MODE(ARRAY_1D_TILED_THIN1);
	/* xyzw swizzles */
	sq_tex_resource_word4 = TEX_DST_SEL_X(SQ_SEL_X) |
				TEX_DST_SEL_Y(SQ_SEL_Y) |
				TEX_DST_SEL_Z(SQ_SEL_Z) |
				TEX_DST_SEL_W(SQ_SEL_W);

	sq_tex_resource_word7 = format |
		S__SQ_CONSTANT_TYPE(SQ_TEX_VTX_VALID_TEXTURE);

	cp_set_surface_sync(rdev,
			    PACKET3_TC_ACTION_ENA, size, gpu_addr);

	radeon_ring_write(ring, PACKET3(PACKET3_SET_RESOURCE, 8));
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, sq_tex_resource_word0);
	radeon_ring_write(ring, sq_tex_resource_word1);
	radeon_ring_write(ring, gpu_addr >> 8);
	radeon_ring_write(ring, gpu_addr >> 8);
	radeon_ring_write(ring, sq_tex_resource_word4);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, 0);
	radeon_ring_write(ring, sq_tex_resource_word7);
}

/* emits 12 */
static void
set_scissors(struct radeon_device *rdev, int x1, int y1,
	     int x2, int y2)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	/* workaround some hw bugs */
	if (x2 == 0)
		x1 = 1;
	if (y2 == 0)
		y1 = 1;
	if (rdev->family >= CHIP_CAYMAN) {
		if ((x2 == 1) && (y2 == 1))
			x2 = 2;
	}

	radeon_ring_write(ring, PACKET3(PACKET3_SET_CONTEXT_REG, 2));
	radeon_ring_write(ring, (PA_SC_SCREEN_SCISSOR_TL - PACKET3_SET_CONTEXT_REG_START) >> 2);
	radeon_ring_write(ring, (x1 << 0) | (y1 << 16));
	radeon_ring_write(ring, (x2 << 0) | (y2 << 16));

	radeon_ring_write(ring, PACKET3(PACKET3_SET_CONTEXT_REG, 2));
	radeon_ring_write(ring, (PA_SC_GENERIC_SCISSOR_TL - PACKET3_SET_CONTEXT_REG_START) >> 2);
	radeon_ring_write(ring, (x1 << 0) | (y1 << 16) | (1 << 31));
	radeon_ring_write(ring, (x2 << 0) | (y2 << 16));

	radeon_ring_write(ring, PACKET3(PACKET3_SET_CONTEXT_REG, 2));
	radeon_ring_write(ring, (PA_SC_WINDOW_SCISSOR_TL - PACKET3_SET_CONTEXT_REG_START) >> 2);
	radeon_ring_write(ring, (x1 << 0) | (y1 << 16) | (1 << 31));
	radeon_ring_write(ring, (x2 << 0) | (y2 << 16));
}

/* emits 10 */
static void
draw_auto(struct radeon_device *rdev)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	radeon_ring_write(ring, PACKET3(PACKET3_SET_CONFIG_REG, 1));
	radeon_ring_write(ring, (VGT_PRIMITIVE_TYPE - PACKET3_SET_CONFIG_REG_START) >> 2);
	radeon_ring_write(ring, DI_PT_RECTLIST);

	radeon_ring_write(ring, PACKET3(PACKET3_INDEX_TYPE, 0));
	radeon_ring_write(ring,
#ifdef __BIG_ENDIAN
			  (2 << 2) |
#endif
			  DI_INDEX_SIZE_16_BIT);

	radeon_ring_write(ring, PACKET3(PACKET3_NUM_INSTANCES, 0));
	radeon_ring_write(ring, 1);

	radeon_ring_write(ring, PACKET3(PACKET3_DRAW_INDEX_AUTO, 1));
	radeon_ring_write(ring, 3);
	radeon_ring_write(ring, DI_SRC_SEL_AUTO_INDEX);

}

/* emits 39 */
static void
set_default_state(struct radeon_device *rdev)
{
	struct radeon_ring *ring = &rdev->ring[RADEON_RING_TYPE_GFX_INDEX];
	u32 sq_config, sq_gpr_resource_mgmt_1, sq_gpr_resource_mgmt_2, sq_gpr_resource_mgmt_3;
	u32 sq_thread_resource_mgmt, sq_thread_resource_mgmt_2;
	u32 sq_stack_resource_mgmt_1, sq_stack_resource_mgmt_2, sq_stack_resource_mgmt_3;
	int num_ps_gprs, num_vs_gprs, num_temp_gprs;
	int num_gs_gprs, num_es_gprs, num_hs_gprs, num_ls_gprs;
	int num_ps_threads, num_vs_threads, num_gs_threads, num_es_threads;
	int num_hs_threads, num_ls_threads;
	int num_ps_stack_entries, num_vs_stack_entries, num_gs_stack_entries, num_es_stack_entries;
	int num_hs_stack_entries, num_ls_stack_entries;
	u64 gpu_addr;
	int dwords;

	/* set clear context state */
	radeon_ring_write(ring, PACKET3(PACKET3_CLEAR_STATE, 0));
	radeon_ring_write(ring, 0);

	if (rdev->family < CHIP_CAYMAN) {
		switch (rdev->family) {
		case CHIP_CEDAR:
		default:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 96;
			num_vs_threads = 16;
			num_gs_threads = 16;
			num_es_threads = 16;
			num_hs_threads = 16;
			num_ls_threads = 16;
			num_ps_stack_entries = 42;
			num_vs_stack_entries = 42;
			num_gs_stack_entries = 42;
			num_es_stack_entries = 42;
			num_hs_stack_entries = 42;
			num_ls_stack_entries = 42;
			break;
		case CHIP_REDWOOD:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 128;
			num_vs_threads = 20;
			num_gs_threads = 20;
			num_es_threads = 20;
			num_hs_threads = 20;
			num_ls_threads = 20;
			num_ps_stack_entries = 42;
			num_vs_stack_entries = 42;
			num_gs_stack_entries = 42;
			num_es_stack_entries = 42;
			num_hs_stack_entries = 42;
			num_ls_stack_entries = 42;
			break;
		case CHIP_JUNIPER:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 128;
			num_vs_threads = 20;
			num_gs_threads = 20;
			num_es_threads = 20;
			num_hs_threads = 20;
			num_ls_threads = 20;
			num_ps_stack_entries = 85;
			num_vs_stack_entries = 85;
			num_gs_stack_entries = 85;
			num_es_stack_entries = 85;
			num_hs_stack_entries = 85;
			num_ls_stack_entries = 85;
			break;
		case CHIP_CYPRESS:
		case CHIP_HEMLOCK:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 128;
			num_vs_threads = 20;
			num_gs_threads = 20;
			num_es_threads = 20;
			num_hs_threads = 20;
			num_ls_threads = 20;
			num_ps_stack_entries = 85;
			num_vs_stack_entries = 85;
			num_gs_stack_entries = 85;
			num_es_stack_entries = 85;
			num_hs_stack_entries = 85;
			num_ls_stack_entries = 85;
			break;
		case CHIP_PALM:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 96;
			num_vs_threads = 16;
			num_gs_threads = 16;
			num_es_threads = 16;
			num_hs_threads = 16;
			num_ls_threads = 16;
			num_ps_stack_entries = 42;
			num_vs_stack_entries = 42;
			num_gs_stack_entries = 42;
			num_es_stack_entries = 42;
			num_hs_stack_entries = 42;
			num_ls_stack_entries = 42;
			break;
		case CHIP_SUMO:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 96;
			num_vs_threads = 25;
			num_gs_threads = 25;
			num_es_threads = 25;
			num_hs_threads = 25;
			num_ls_threads = 25;
			num_ps_stack_entries = 42;
			num_vs_stack_entries = 42;
			num_gs_stack_entries = 42;
			num_es_stack_entries = 42;
			num_hs_stack_entries = 42;
			num_ls_stack_entries = 42;
			break;
		case CHIP_SUMO2:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 96;
			num_vs_threads = 25;
			num_gs_threads = 25;
			num_es_threads = 25;
			num_hs_threads = 25;
			num_ls_threads = 25;
			num_ps_stack_entries = 85;
			num_vs_stack_entries = 85;
			num_gs_stack_entries = 85;
			num_es_stack_entries = 85;
			num_hs_stack_entries = 85;
			num_ls_stack_entries = 85;
			break;
		case CHIP_BARTS:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 128;
			num_vs_threads = 20;
			num_gs_threads = 20;
			num_es_threads = 20;
			num_hs_threads = 20;
			num_ls_threads = 20;
			num_ps_stack_entries = 85;
			num_vs_stack_entries = 85;
			num_gs_stack_entries = 85;
			num_es_stack_entries = 85;
			num_hs_stack_entries = 85;
			num_ls_stack_entries = 85;
			break;
		case CHIP_TURKS:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 128;
			num_vs_threads = 20;
			num_gs_threads = 20;
			num_es_threads = 20;
			num_hs_threads = 20;
			num_ls_threads = 20;
			num_ps_stack_entries = 42;
			num_vs_stack_entries = 42;
			num_gs_stack_entries = 42;
			num_es_stack_entries = 42;
			num_hs_stack_entries = 42;
			num_ls_stack_entries = 42;
			break;
		case CHIP_CAICOS:
			num_ps_gprs = 93;
			num_vs_gprs = 46;
			num_temp_gprs = 4;
			num_gs_gprs = 31;
			num_es_gprs = 31;
			num_hs_gprs = 23;
			num_ls_gprs = 23;
			num_ps_threads = 128;
			num_vs_threads = 10;
			num_gs_threads = 10;
			num_es_threads = 10;
			num_hs_threads = 10;
			num_ls_threads = 10;
			num_ps_stack_entries = 42;
			num_vs_stack_entries = 42;
			num_gs_stack_entries = 42;
			num_es_stack_entries = 42;
			num_hs_stack_entries = 42;
			num_ls_stack_entries = 42;
			break;
		}

		if ((rdev->family == CHIP_CEDAR) ||
		    (rdev->family == CHIP_PALM) ||
		    (rdev->family == CHIP_SUMO) ||
		    (rdev->family == CHIP_SUMO2) ||
		    (rdev->family == CHIP_CAICOS))
			sq_config = 0;
		else
			sq_config = VC_ENABLE;

		sq_config |= (EXPORT_SRC_C |
			      CS_PRIO(0) |
			      LS_PRIO(0) |
			      HS_PRIO(0) |
			      PS_PRIO(0) |
			      VS_PRIO(1) |
			      GS_PRIO(2) |
			      ES_PRIO(3));

		sq_gpr_resource_mgmt_1 = (NUM_PS_GPRS(num_ps_gprs) |
					  NUM_VS_GPRS(num_vs_gprs) |
					  NUM_CLAUSE_TEMP_GPRS(num_temp_gprs));
		sq_gpr_resource_mgmt_2 = (NUM_GS_GPRS(num_gs_gprs) |
					  NUM_ES_GPRS(num_es_gprs));
		sq_gpr_resource_mgmt_3 = (NUM_HS_GPRS(num_hs_gprs) |
					  NUM_LS_GPRS(num_ls_gprs));
		sq_thread_resource_mgmt = (NUM_PS_THREADS(num_ps_threads) |
					   NUM_VS_THREADS(num_vs_threads) |
					   NUM_GS_THREADS(num_gs_threads) |
					   NUM_ES_THREADS(num_es_threads));
		sq_thread_resource_mgmt_2 = (NUM_HS_THREADS(num_hs_threads) |
					     NUM_LS_THREADS(num_ls_threads));
		sq_stack_resource_mgmt_1 = (NUM_PS_STACK_ENTRIES(num_ps_stack_entries) |
					    NUM_VS_STACK_ENTRIES(num_vs_stack_entries));
		sq_stack_resource_mgmt_2 = (NUM_GS_STACK_ENTRIES(num_gs_stack_entries) |
					    NUM_ES_STACK_ENTRIES(num_es_stack_entries));
		sq_stack_resource_mgmt_3 = (NUM_HS_STACK_ENTRIES(num_hs_stack_entries) |
					    NUM_LS_STACK_ENTRIES(num_ls_stack_entries));

		/* disable dyn gprs */
		radeon_ring_write(ring, PACKET3(PACKET3_SET_CONFIG_REG, 1));
		radeon_ring_write(ring, (SQ_DYN_GPR_CNTL_PS_FLUSH_REQ - PACKET3_SET_CONFIG_REG_START) >> 2);
		radeon_ring_write(ring, 0);

		/* setup LDS */
		radeon_ring_write(ring, PACKET3(PACKET3_SET_CONFIG_REG, 1));
		radeon_ring_write(ring, (SQ_LDS_RESOURCE_MGMT - PACKET3_SET_CONFIG_REG_START) >> 2);
		radeon_ring_write(ring, 0x10001000);

		/* SQ config */
		radeon_ring_write(ring, PACKET3(PACKET3_SET_CONFIG_REG, 11));
		radeon_ring_write(ring, (SQ_CONFIG - PACKET3_SET_CONFIG_REG_START) >> 2);
		radeon_ring_write(ring, sq_config);
		radeon_ring_write(ring, sq_gpr_resource_mgmt_1);
		radeon_ring_write(ring, sq_gpr_resource_mgmt_2);
		radeon_ring_write(ring, sq_gpr_resource_mgmt_3);
		radeon_ring_write(ring, 0);
		radeon_ring_write(ring, 0);
		radeon_ring_write(ring, sq_thread_resource_mgmt);
		radeon_ring_write(ring, sq_thread_resource_mgmt_2);
		radeon_ring_write(ring, sq_stack_resource_mgmt_1);
		radeon_ring_write(ring, sq_stack_resource_mgmt_2);
		radeon_ring_write(ring, sq_stack_resource_mgmt_3);
	}

	/* CONTEXT_CONTROL */
	radeon_ring_write(ring, 0xc0012800);
	radeon_ring_write(ring, 0x80000000);
	radeon_ring_write(ring, 0x80000000);

	/* SQ_VTX_BASE_VTX_LOC */
	radeon_ring_write(ring, 0xc0026f00);
	radeon_ring_write(ring, 0x00000000);
	radeon_ring_write(ring, 0x00000000);
	radeon_ring_write(ring, 0x00000000);

	/* SET_SAMPLER */
	radeon_ring_write(ring, 0xc0036e00);
	radeon_ring_write(ring, 0x00000000);
	radeon_ring_write(ring, 0x00000012);
	radeon_ring_write(ring, 0x00000000);
	radeon_ring_write(ring, 0x00000000);

	/* set to DX10/11 mode */
	radeon_ring_write(ring, PACKET3(PACKET3_MODE_CONTROL, 0));
	radeon_ring_write(ring, 1);

	/* emit an IB pointing at default state */
	dwords = roundup2(rdev->r600_blit.state_len, 0x10);
	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.state_offset;
	radeon_ring_write(ring, PACKET3(PACKET3_INDIRECT_BUFFER, 2));
	radeon_ring_write(ring, gpu_addr & 0xFFFFFFFC);
	radeon_ring_write(ring, upper_32_bits(gpu_addr) & 0xFF);
	radeon_ring_write(ring, dwords);

}

int evergreen_blit_init(struct radeon_device *rdev)
{
	u32 obj_size;
	int i, r, dwords;
	void *ptr;
	u32 packet2s[16];
	int num_packet2s = 0;

	rdev->r600_blit.primitives.set_render_target = set_render_target;
	rdev->r600_blit.primitives.cp_set_surface_sync = cp_set_surface_sync;
	rdev->r600_blit.primitives.set_shaders = set_shaders;
	rdev->r600_blit.primitives.set_vtx_resource = set_vtx_resource;
	rdev->r600_blit.primitives.set_tex_resource = set_tex_resource;
	rdev->r600_blit.primitives.set_scissors = set_scissors;
	rdev->r600_blit.primitives.draw_auto = draw_auto;
	rdev->r600_blit.primitives.set_default_state = set_default_state;

	rdev->r600_blit.ring_size_common = 8; /* sync semaphore */
	rdev->r600_blit.ring_size_common += 55; /* shaders + def state */
	rdev->r600_blit.ring_size_common += 16; /* fence emit for VB IB */
	rdev->r600_blit.ring_size_common += 5; /* done copy */
	rdev->r600_blit.ring_size_common += 16; /* fence emit for done copy */

	rdev->r600_blit.ring_size_per_loop = 74;
	if (rdev->family >= CHIP_CAYMAN)
		rdev->r600_blit.ring_size_per_loop += 9; /* additional DWs for surface sync */

	rdev->r600_blit.max_dim = 16384;

	rdev->r600_blit.state_offset = 0;

	if (rdev->family < CHIP_CAYMAN)
		rdev->r600_blit.state_len = evergreen_default_size;
	else
		rdev->r600_blit.state_len = cayman_default_size;

	dwords = rdev->r600_blit.state_len;
	while (dwords & 0xf) {
		packet2s[num_packet2s++] = cpu_to_le32(PACKET2(0));
		dwords++;
	}

	obj_size = dwords * 4;
	obj_size = roundup2(obj_size, 256);

	rdev->r600_blit.vs_offset = obj_size;
	if (rdev->family < CHIP_CAYMAN)
		obj_size += evergreen_vs_size * 4;
	else
		obj_size += cayman_vs_size * 4;
	obj_size = roundup2(obj_size, 256);

	rdev->r600_blit.ps_offset = obj_size;
	if (rdev->family < CHIP_CAYMAN)
		obj_size += evergreen_ps_size * 4;
	else
		obj_size += cayman_ps_size * 4;
	obj_size = roundup2(obj_size, 256);

	/* pin copy shader into vram if not already initialized */
	if (!rdev->r600_blit.shader_obj) {
		r = radeon_bo_create(rdev, obj_size, PAGE_SIZE, true,
				     RADEON_GEM_DOMAIN_VRAM,
				     NULL, &rdev->r600_blit.shader_obj);
		if (r) {
			DRM_ERROR("evergreen failed to allocate shader\n");
			return r;
		}

		r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
		if (unlikely(r != 0))
			return r;
		r = radeon_bo_pin(rdev->r600_blit.shader_obj, RADEON_GEM_DOMAIN_VRAM,
				  &rdev->r600_blit.shader_gpu_addr);
		radeon_bo_unreserve(rdev->r600_blit.shader_obj);
		if (r) {
			dev_err(rdev->dev, "(%d) pin blit object failed\n", r);
			return r;
		}
	}

	DRM_DEBUG("evergreen blit allocated bo %08x vs %08x ps %08x\n",
		  obj_size,
		  rdev->r600_blit.vs_offset, rdev->r600_blit.ps_offset);

	r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
	if (unlikely(r != 0))
		return r;
	r = radeon_bo_kmap(rdev->r600_blit.shader_obj, &ptr);
	if (r) {
		DRM_ERROR("failed to map blit object %d\n", r);
		return r;
	}

	if (rdev->family < CHIP_CAYMAN) {
		memcpy_toio(ptr + rdev->r600_blit.state_offset,
			    evergreen_default_state, rdev->r600_blit.state_len * 4);

		if (num_packet2s)
			memcpy_toio(ptr + rdev->r600_blit.state_offset + (rdev->r600_blit.state_len * 4),
				    packet2s, num_packet2s * 4);
		for (i = 0; i < evergreen_vs_size; i++)
			*(u32 *)((unsigned long)ptr + rdev->r600_blit.vs_offset + i * 4) = cpu_to_le32(evergreen_vs[i]);
		for (i = 0; i < evergreen_ps_size; i++)
			*(u32 *)((unsigned long)ptr + rdev->r600_blit.ps_offset + i * 4) = cpu_to_le32(evergreen_ps[i]);
	} else {
		memcpy_toio(ptr + rdev->r600_blit.state_offset,
			    cayman_default_state, rdev->r600_blit.state_len * 4);

		if (num_packet2s)
			memcpy_toio(ptr + rdev->r600_blit.state_offset + (rdev->r600_blit.state_len * 4),
				    packet2s, num_packet2s * 4);
		for (i = 0; i < cayman_vs_size; i++)
			*(u32 *)((unsigned long)ptr + rdev->r600_blit.vs_offset + i * 4) = cpu_to_le32(cayman_vs[i]);
		for (i = 0; i < cayman_ps_size; i++)
			*(u32 *)((unsigned long)ptr + rdev->r600_blit.ps_offset + i * 4) = cpu_to_le32(cayman_ps[i]);
	}
	radeon_bo_kunmap(rdev->r600_blit.shader_obj);
	radeon_bo_unreserve(rdev->r600_blit.shader_obj);

	radeon_ttm_set_active_vram_size(rdev, rdev->mc.real_vram_size);
	return 0;
}
