/*
 * Copyright 2016 Advanced Micro Devices, Inc.
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
 * Authors: AMD
 *
 */

#ifndef __DC_HUBBUB_DCN10_H__
#define __DC_HUBBUB_DCN10_H__

#include "core_types.h"
#include "dchubbub.h"

#define HUBHUB_REG_LIST_DCN()\
	SR(DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_A),\
	SR(DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_A),\
	SR(DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_A),\
	SR(DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_B),\
	SR(DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_B),\
	SR(DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_B),\
	SR(DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_C),\
	SR(DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_C),\
	SR(DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_C),\
	SR(DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_D),\
	SR(DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_D),\
	SR(DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_D),\
	SR(DCHUBBUB_ARB_WATERMARK_CHANGE_CNTL),\
	SR(DCHUBBUB_ARB_DRAM_STATE_CNTL),\
	SR(DCHUBBUB_ARB_SAT_LEVEL),\
	SR(DCHUBBUB_ARB_DF_REQ_OUTSTAND),\
	SR(DCHUBBUB_GLOBAL_TIMER_CNTL), \
	SR(DCHUBBUB_TEST_DEBUG_INDEX), \
	SR(DCHUBBUB_TEST_DEBUG_DATA),\
	SR(DCHUBBUB_SOFT_RESET)

#define HUBBUB_SR_WATERMARK_REG_LIST()\
	SR(DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_A),\
	SR(DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_A),\
	SR(DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_B),\
	SR(DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_B),\
	SR(DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_C),\
	SR(DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_C),\
	SR(DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_D),\
	SR(DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_D)

#define HUBBUB_REG_LIST_DCN10(id)\
	HUBHUB_REG_LIST_DCN(), \
	HUBBUB_SR_WATERMARK_REG_LIST(), \
	SR(DCHUBBUB_SDPIF_FB_TOP),\
	SR(DCHUBBUB_SDPIF_FB_BASE),\
	SR(DCHUBBUB_SDPIF_FB_OFFSET),\
	SR(DCHUBBUB_SDPIF_AGP_BASE),\
	SR(DCHUBBUB_SDPIF_AGP_BOT),\
	SR(DCHUBBUB_SDPIF_AGP_TOP)

struct dcn_hubbub_registers {
	uint32_t DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_A;
	uint32_t DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_A;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_A;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_A;
	uint32_t DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_A;
	uint32_t DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_B;
	uint32_t DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_B;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_B;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_B;
	uint32_t DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_B;
	uint32_t DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_C;
	uint32_t DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_C;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_C;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_C;
	uint32_t DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_C;
	uint32_t DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_D;
	uint32_t DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_D;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_D;
	uint32_t DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_D;
	uint32_t DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_D;
	uint32_t DCHUBBUB_ARB_WATERMARK_CHANGE_CNTL;
	uint32_t DCHUBBUB_ARB_SAT_LEVEL;
	uint32_t DCHUBBUB_ARB_DF_REQ_OUTSTAND;
	uint32_t DCHUBBUB_GLOBAL_TIMER_CNTL;
	uint32_t DCHUBBUB_ARB_DRAM_STATE_CNTL;
	uint32_t DCHUBBUB_TEST_DEBUG_INDEX;
	uint32_t DCHUBBUB_TEST_DEBUG_DATA;
	uint32_t DCHUBBUB_SDPIF_FB_TOP;
	uint32_t DCHUBBUB_SDPIF_FB_BASE;
	uint32_t DCHUBBUB_SDPIF_FB_OFFSET;
	uint32_t DCHUBBUB_SDPIF_AGP_BASE;
	uint32_t DCHUBBUB_SDPIF_AGP_BOT;
	uint32_t DCHUBBUB_SDPIF_AGP_TOP;
	uint32_t DCHUBBUB_CRC_CTRL;
	uint32_t DCHUBBUB_SOFT_RESET;
};

/* set field name */
#define HUBBUB_SF(reg_name, field_name, post_fix)\
	.field_name = reg_name ## __ ## field_name ## post_fix


#define HUBBUB_MASK_SH_LIST_DCN(mask_sh)\
		HUBBUB_SF(DCHUBBUB_GLOBAL_TIMER_CNTL, DCHUBBUB_GLOBAL_TIMER_ENABLE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_SOFT_RESET, DCHUBBUB_GLOBAL_SOFT_RESET, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_WATERMARK_CHANGE_CNTL, DCHUBBUB_ARB_WATERMARK_CHANGE_REQUEST, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_WATERMARK_CHANGE_CNTL, DCHUBBUB_ARB_WATERMARK_CHANGE_DONE_INTERRUPT_DISABLE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_DRAM_STATE_CNTL, DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_VALUE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_DRAM_STATE_CNTL, DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_ENABLE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_DRAM_STATE_CNTL, DCHUBBUB_ARB_ALLOW_PSTATE_CHANGE_FORCE_VALUE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_DRAM_STATE_CNTL, DCHUBBUB_ARB_ALLOW_PSTATE_CHANGE_FORCE_ENABLE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_SAT_LEVEL, DCHUBBUB_ARB_SAT_LEVEL, mask_sh), \
		HUBBUB_SF(DCHUBBUB_ARB_DF_REQ_OUTSTAND, DCHUBBUB_ARB_MIN_REQ_OUTSTAND, mask_sh)

#define HUBBUB_MASK_SH_LIST_DCN10(mask_sh)\
		HUBBUB_MASK_SH_LIST_DCN(mask_sh), \
		HUBBUB_SF(DCHUBBUB_SDPIF_FB_TOP, SDPIF_FB_TOP, mask_sh), \
		HUBBUB_SF(DCHUBBUB_SDPIF_FB_BASE, SDPIF_FB_BASE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_SDPIF_FB_OFFSET, SDPIF_FB_OFFSET, mask_sh), \
		HUBBUB_SF(DCHUBBUB_SDPIF_AGP_BASE, SDPIF_AGP_BASE, mask_sh), \
		HUBBUB_SF(DCHUBBUB_SDPIF_AGP_BOT, SDPIF_AGP_BOT, mask_sh), \
		HUBBUB_SF(DCHUBBUB_SDPIF_AGP_TOP, SDPIF_AGP_TOP, mask_sh)

#define DCN_HUBBUB_REG_FIELD_LIST(type) \
		type DCHUBBUB_GLOBAL_TIMER_ENABLE; \
		type DCHUBBUB_ARB_WATERMARK_CHANGE_REQUEST;\
		type DCHUBBUB_ARB_WATERMARK_CHANGE_DONE_INTERRUPT_DISABLE;\
		type DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_VALUE;\
		type DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_ENABLE;\
		type DCHUBBUB_ARB_ALLOW_PSTATE_CHANGE_FORCE_VALUE;\
		type DCHUBBUB_ARB_ALLOW_PSTATE_CHANGE_FORCE_ENABLE;\
		type DCHUBBUB_ARB_SAT_LEVEL;\
		type DCHUBBUB_ARB_MIN_REQ_OUTSTAND;\
		type DCHUBBUB_GLOBAL_TIMER_REFDIV;\
		type DCHUBBUB_GLOBAL_SOFT_RESET; \
		type SDPIF_FB_TOP;\
		type SDPIF_FB_BASE;\
		type SDPIF_FB_OFFSET;\
		type SDPIF_AGP_BASE;\
		type SDPIF_AGP_BOT;\
		type SDPIF_AGP_TOP


struct dcn_hubbub_shift {
	DCN_HUBBUB_REG_FIELD_LIST(uint8_t);
};

struct dcn_hubbub_mask {
	DCN_HUBBUB_REG_FIELD_LIST(uint32_t);
};

struct dc;

struct dcn_hubbub_wm_set {
	uint32_t wm_set;
	uint32_t data_urgent;
	uint32_t pte_meta_urgent;
	uint32_t sr_enter;
	uint32_t sr_exit;
	uint32_t dram_clk_chanage;
};

struct dcn_hubbub_wm {
	struct dcn_hubbub_wm_set sets[4];
};

struct hubbub {
	const struct hubbub_funcs *funcs;
	struct dc_context *ctx;
	const struct dcn_hubbub_registers *regs;
	const struct dcn_hubbub_shift *shifts;
	const struct dcn_hubbub_mask *masks;
	unsigned int debug_test_index_pstate;
	struct dcn_watermark_set watermarks;
};

void hubbub1_update_dchub(
	struct hubbub *hubbub,
	struct dchub_init_data *dh_data);

bool hubbub1_verify_allow_pstate_change_high(
	struct hubbub *hubbub);

void hubbub1_wm_change_req_wa(struct hubbub *hubbub);

void hubbub1_program_watermarks(
		struct hubbub *hubbub,
		struct dcn_watermark_set *watermarks,
		unsigned int refclk_mhz,
		bool safe_to_lower);

void hubbub1_toggle_watermark_change_req(
		struct hubbub *hubbub);

void hubbub1_wm_read_state(struct hubbub *hubbub,
		struct dcn_hubbub_wm *wm);

void hubbub1_soft_reset(struct hubbub *hubbub, bool reset);
void hubbub1_construct(struct hubbub *hubbub,
	struct dc_context *ctx,
	const struct dcn_hubbub_registers *hubbub_regs,
	const struct dcn_hubbub_shift *hubbub_shift,
	const struct dcn_hubbub_mask *hubbub_mask);

#endif
