/*
 * Copyright 2012-16 Advanced Micro Devices, Inc.
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

#include "dm_services.h"

#include "include/i2caux_interface.h"
#include "../i2caux.h"
#include "../engine.h"
#include "../i2c_engine.h"
#include "../i2c_sw_engine.h"
#include "../i2c_hw_engine.h"

#include "../dce110/i2c_hw_engine_dce110.h"
#include "../dce110/aux_engine_dce110.h"
#include "../dce110/i2caux_dce110.h"

#include "dce/dce_12_0_offset.h"
#include "dce/dce_12_0_sh_mask.h"
#include "soc15_hw_ip.h"
#include "vega10_ip_offset.h"

/* begin *********************
 * macros to expend register list macro defined in HW object header file */

#define BASE_INNER(seg) \
	DCE_BASE__INST0_SEG ## seg

/* compile time expand base address. */
#define BASE(seg) \
	BASE_INNER(seg)

#define SR(reg_name)\
		.reg_name = BASE(mm ## reg_name ## _BASE_IDX) +  \
					mm ## reg_name

#define SRI(reg_name, block, id)\
	.reg_name = BASE(mm ## block ## id ## _ ## reg_name ## _BASE_IDX) + \
					mm ## block ## id ## _ ## reg_name
/* macros to expend register list macro defined in HW object header file
 * end *********************/

#define aux_regs(id)\
[id] = {\
	AUX_COMMON_REG_LIST(id), \
	.AUX_RESET_MASK = DP_AUX0_AUX_CONTROL__AUX_RESET_MASK \
}

static const struct dce110_aux_registers dce120_aux_regs[] = {
		aux_regs(0),
		aux_regs(1),
		aux_regs(2),
		aux_regs(3),
		aux_regs(4),
		aux_regs(5),
};

#define hw_engine_regs(id)\
{\
		I2C_HW_ENGINE_COMMON_REG_LIST(id) \
}

static const struct dce110_i2c_hw_engine_registers dce120_hw_engine_regs[] = {
		hw_engine_regs(1),
		hw_engine_regs(2),
		hw_engine_regs(3),
		hw_engine_regs(4),
		hw_engine_regs(5),
		hw_engine_regs(6)
};

static const struct dce110_i2c_hw_engine_shift i2c_shift = {
		I2C_COMMON_MASK_SH_LIST_DCE110(__SHIFT)
};

static const struct dce110_i2c_hw_engine_mask i2c_mask = {
		I2C_COMMON_MASK_SH_LIST_DCE110(_MASK)
};

struct i2caux *dal_i2caux_dce120_create(
	struct dc_context *ctx)
{
	struct i2caux_dce110 *i2caux_dce110 =
		kzalloc(sizeof(struct i2caux_dce110), GFP_KERNEL);

	if (!i2caux_dce110) {
		ASSERT_CRITICAL(false);
		return NULL;
	}

	dal_i2caux_dce110_construct(i2caux_dce110,
				    ctx,
				    ARRAY_SIZE(dce120_aux_regs),
				    dce120_aux_regs,
				    dce120_hw_engine_regs,
				    &i2c_shift,
				    &i2c_mask);
	return &i2caux_dce110->base;
}
