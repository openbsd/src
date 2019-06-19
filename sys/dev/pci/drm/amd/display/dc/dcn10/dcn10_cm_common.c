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
#include "dc.h"
#include "reg_helper.h"
#include "dcn10_dpp.h"

#include "dcn10_cm_common.h"
#include "custom_float.h"

#define REG(reg) reg

#define CTX \
	ctx

#undef FN
#define FN(reg_name, field_name) \
	reg->shifts.field_name, reg->masks.field_name

void cm_helper_program_color_matrices(
		struct dc_context *ctx,
		const uint16_t *regval,
		const struct color_matrices_reg *reg)
{
	uint32_t cur_csc_reg;
	unsigned int i = 0;

	for (cur_csc_reg = reg->csc_c11_c12;
			cur_csc_reg <= reg->csc_c33_c34;
			cur_csc_reg++) {

		const uint16_t *regval0 = &(regval[2 * i]);
		const uint16_t *regval1 = &(regval[(2 * i) + 1]);

		REG_SET_2(cur_csc_reg, 0,
				csc_c11, *regval0,
				csc_c12, *regval1);

		i++;
	}

}

void cm_helper_program_xfer_func(
		struct dc_context *ctx,
		const struct pwl_params *params,
		const struct xfer_func_reg *reg)
{
	uint32_t reg_region_cur;
	unsigned int i = 0;

	REG_SET_2(reg->start_cntl_b, 0,
			exp_region_start, params->arr_points[0].custom_float_x,
			exp_resion_start_segment, 0);
	REG_SET_2(reg->start_cntl_g, 0,
			exp_region_start, params->arr_points[0].custom_float_x,
			exp_resion_start_segment, 0);
	REG_SET_2(reg->start_cntl_r, 0,
			exp_region_start, params->arr_points[0].custom_float_x,
			exp_resion_start_segment, 0);

	REG_SET(reg->start_slope_cntl_b, 0,
			field_region_linear_slope, params->arr_points[0].custom_float_slope);
	REG_SET(reg->start_slope_cntl_g, 0,
			field_region_linear_slope, params->arr_points[0].custom_float_slope);
	REG_SET(reg->start_slope_cntl_r, 0,
			field_region_linear_slope, params->arr_points[0].custom_float_slope);

	REG_SET(reg->start_end_cntl1_b, 0,
			field_region_end, params->arr_points[1].custom_float_x);
	REG_SET_2(reg->start_end_cntl2_b, 0,
			field_region_end_slope, params->arr_points[1].custom_float_slope,
			field_region_end_base, params->arr_points[1].custom_float_y);

	REG_SET(reg->start_end_cntl1_g, 0,
			field_region_end, params->arr_points[1].custom_float_x);
	REG_SET_2(reg->start_end_cntl2_g, 0,
			field_region_end_slope, params->arr_points[1].custom_float_slope,
		field_region_end_base, params->arr_points[1].custom_float_y);

	REG_SET(reg->start_end_cntl1_r, 0,
			field_region_end, params->arr_points[1].custom_float_x);
	REG_SET_2(reg->start_end_cntl2_r, 0,
			field_region_end_slope, params->arr_points[1].custom_float_slope,
		field_region_end_base, params->arr_points[1].custom_float_y);

	for (reg_region_cur = reg->region_start;
			reg_region_cur <= reg->region_end;
			reg_region_cur++) {

		const struct gamma_curve *curve0 = &(params->arr_curve_points[2 * i]);
		const struct gamma_curve *curve1 = &(params->arr_curve_points[(2 * i) + 1]);

		REG_SET_4(reg_region_cur, 0,
				exp_region0_lut_offset, curve0->offset,
				exp_region0_num_segments, curve0->segments_num,
				exp_region1_lut_offset, curve1->offset,
				exp_region1_num_segments, curve1->segments_num);

		i++;
	}

}



bool cm_helper_convert_to_custom_float(
		struct pwl_result_data *rgb_resulted,
		struct curve_points *arr_points,
		uint32_t hw_points_num,
		bool fixpoint)
{
	struct custom_float_format fmt;

	struct pwl_result_data *rgb = rgb_resulted;

	uint32_t i = 0;

	fmt.exponenta_bits = 6;
	fmt.mantissa_bits = 12;
	fmt.sign = false;

	if (!convert_to_custom_float_format(arr_points[0].x, &fmt,
					    &arr_points[0].custom_float_x)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!convert_to_custom_float_format(arr_points[0].offset, &fmt,
					    &arr_points[0].custom_float_offset)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!convert_to_custom_float_format(arr_points[0].slope, &fmt,
					    &arr_points[0].custom_float_slope)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	fmt.mantissa_bits = 10;
	fmt.sign = false;

	if (!convert_to_custom_float_format(arr_points[1].x, &fmt,
					    &arr_points[1].custom_float_x)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (fixpoint == true)
		arr_points[1].custom_float_y = dc_fixpt_clamp_u0d14(arr_points[1].y);
	else if (!convert_to_custom_float_format(arr_points[1].y, &fmt,
		&arr_points[1].custom_float_y)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!convert_to_custom_float_format(arr_points[1].slope, &fmt,
					    &arr_points[1].custom_float_slope)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (hw_points_num == 0 || rgb_resulted == NULL || fixpoint == true)
		return true;

	fmt.mantissa_bits = 12;
	fmt.sign = true;

	while (i != hw_points_num) {
		if (!convert_to_custom_float_format(rgb->red, &fmt,
						    &rgb->red_reg)) {
			BREAK_TO_DEBUGGER();
			return false;
		}

		if (!convert_to_custom_float_format(rgb->green, &fmt,
						    &rgb->green_reg)) {
			BREAK_TO_DEBUGGER();
			return false;
		}

		if (!convert_to_custom_float_format(rgb->blue, &fmt,
						    &rgb->blue_reg)) {
			BREAK_TO_DEBUGGER();
			return false;
		}

		if (!convert_to_custom_float_format(rgb->delta_red, &fmt,
						    &rgb->delta_red_reg)) {
			BREAK_TO_DEBUGGER();
			return false;
		}

		if (!convert_to_custom_float_format(rgb->delta_green, &fmt,
						    &rgb->delta_green_reg)) {
			BREAK_TO_DEBUGGER();
			return false;
		}

		if (!convert_to_custom_float_format(rgb->delta_blue, &fmt,
						    &rgb->delta_blue_reg)) {
			BREAK_TO_DEBUGGER();
			return false;
		}

		++rgb;
		++i;
	}

	return true;
}

/* driver uses 32 regions or less, but DCN HW has 34, extra 2 are set to 0 */
#define MAX_REGIONS_NUMBER 34
#define MAX_LOW_POINT      25
#define NUMBER_REGIONS     32
#define NUMBER_SW_SEGMENTS 16

bool cm_helper_translate_curve_to_hw_format(
				const struct dc_transfer_func *output_tf,
				struct pwl_params *lut_params, bool fixpoint)
{
	struct curve_points *arr_points;
	struct pwl_result_data *rgb_resulted;
	struct pwl_result_data *rgb;
	struct pwl_result_data *rgb_plus_1;
	struct fixed31_32 y_r;
	struct fixed31_32 y_g;
	struct fixed31_32 y_b;
	struct fixed31_32 y1_min;
	struct fixed31_32 y3_max;

	int32_t region_start, region_end;
	int32_t i;
	uint32_t j, k, seg_distr[MAX_REGIONS_NUMBER], increment, start_index, hw_points;

	if (output_tf == NULL || lut_params == NULL || output_tf->type == TF_TYPE_BYPASS)
		return false;

	PERF_TRACE();

	arr_points = lut_params->arr_points;
	rgb_resulted = lut_params->rgb_resulted;
	hw_points = 0;

	memset(lut_params, 0, sizeof(struct pwl_params));
	memset(seg_distr, 0, sizeof(seg_distr));

	if (output_tf->tf == TRANSFER_FUNCTION_PQ) {
		/* 32 segments
		 * segments are from 2^-25 to 2^7
		 */
		for (i = 0; i < NUMBER_REGIONS ; i++)
			seg_distr[i] = 3;

		region_start = -MAX_LOW_POINT;
		region_end   = NUMBER_REGIONS - MAX_LOW_POINT;
	} else {
		/* 10 segments
		 * segment is from 2^-10 to 2^0
		 * There are less than 256 points, for optimization
		 */
		seg_distr[0] = 3;
		seg_distr[1] = 4;
		seg_distr[2] = 4;
		seg_distr[3] = 4;
		seg_distr[4] = 4;
		seg_distr[5] = 4;
		seg_distr[6] = 4;
		seg_distr[7] = 4;
		seg_distr[8] = 4;
		seg_distr[9] = 4;

		region_start = -10;
		region_end = 0;
	}

	for (i = region_end - region_start; i < MAX_REGIONS_NUMBER ; i++)
		seg_distr[i] = -1;

	for (k = 0; k < MAX_REGIONS_NUMBER; k++) {
		if (seg_distr[k] != -1)
			hw_points += (1 << seg_distr[k]);
	}

	j = 0;
	for (k = 0; k < (region_end - region_start); k++) {
		increment = NUMBER_SW_SEGMENTS / (1 << seg_distr[k]);
		start_index = (region_start + k + MAX_LOW_POINT) *
				NUMBER_SW_SEGMENTS;
		for (i = start_index; i < start_index + NUMBER_SW_SEGMENTS;
				i += increment) {
			if (j == hw_points - 1)
				break;
			rgb_resulted[j].red = output_tf->tf_pts.red[i];
			rgb_resulted[j].green = output_tf->tf_pts.green[i];
			rgb_resulted[j].blue = output_tf->tf_pts.blue[i];
			j++;
		}
	}

	/* last point */
	start_index = (region_end + MAX_LOW_POINT) * NUMBER_SW_SEGMENTS;
	rgb_resulted[hw_points - 1].red = output_tf->tf_pts.red[start_index];
	rgb_resulted[hw_points - 1].green = output_tf->tf_pts.green[start_index];
	rgb_resulted[hw_points - 1].blue = output_tf->tf_pts.blue[start_index];

	arr_points[0].x = dc_fixpt_pow(dc_fixpt_from_int(2),
					     dc_fixpt_from_int(region_start));
	arr_points[1].x = dc_fixpt_pow(dc_fixpt_from_int(2),
					     dc_fixpt_from_int(region_end));

	y_r = rgb_resulted[0].red;
	y_g = rgb_resulted[0].green;
	y_b = rgb_resulted[0].blue;

	y1_min = dc_fixpt_min(y_r, dc_fixpt_min(y_g, y_b));

	arr_points[0].y = y1_min;
	arr_points[0].slope = dc_fixpt_div(arr_points[0].y, arr_points[0].x);
	y_r = rgb_resulted[hw_points - 1].red;
	y_g = rgb_resulted[hw_points - 1].green;
	y_b = rgb_resulted[hw_points - 1].blue;

	/* see comment above, m_arrPoints[1].y should be the Y value for the
	 * region end (m_numOfHwPoints), not last HW point(m_numOfHwPoints - 1)
	 */
	y3_max = dc_fixpt_max(y_r, dc_fixpt_max(y_g, y_b));

	arr_points[1].y = y3_max;

	arr_points[1].slope = dc_fixpt_zero;

	if (output_tf->tf == TRANSFER_FUNCTION_PQ) {
		/* for PQ, we want to have a straight line from last HW X point,
		 * and the slope to be such that we hit 1.0 at 10000 nits.
		 */
		const struct fixed31_32 end_value =
				dc_fixpt_from_int(125);

		arr_points[1].slope = dc_fixpt_div(
			dc_fixpt_sub(dc_fixpt_one, arr_points[1].y),
			dc_fixpt_sub(end_value, arr_points[1].x));
	}

	lut_params->hw_points_num = hw_points;

	k = 0;
	for (i = 1; i < MAX_REGIONS_NUMBER; i++) {
		if (seg_distr[k] != -1) {
			lut_params->arr_curve_points[k].segments_num =
					seg_distr[k];
			lut_params->arr_curve_points[i].offset =
					lut_params->arr_curve_points[k].offset + (1 << seg_distr[k]);
		}
		k++;
	}

	if (seg_distr[k] != -1)
		lut_params->arr_curve_points[k].segments_num = seg_distr[k];

	rgb = rgb_resulted;
	rgb_plus_1 = rgb_resulted + 1;

	i = 1;
	while (i != hw_points + 1) {
		if (dc_fixpt_lt(rgb_plus_1->red, rgb->red))
			rgb_plus_1->red = rgb->red;
		if (dc_fixpt_lt(rgb_plus_1->green, rgb->green))
			rgb_plus_1->green = rgb->green;
		if (dc_fixpt_lt(rgb_plus_1->blue, rgb->blue))
			rgb_plus_1->blue = rgb->blue;

		rgb->delta_red   = dc_fixpt_sub(rgb_plus_1->red,   rgb->red);
		rgb->delta_green = dc_fixpt_sub(rgb_plus_1->green, rgb->green);
		rgb->delta_blue  = dc_fixpt_sub(rgb_plus_1->blue,  rgb->blue);

		if (fixpoint == true) {
			rgb->delta_red_reg   = dc_fixpt_clamp_u0d10(rgb->delta_red);
			rgb->delta_green_reg = dc_fixpt_clamp_u0d10(rgb->delta_green);
			rgb->delta_blue_reg  = dc_fixpt_clamp_u0d10(rgb->delta_blue);
			rgb->red_reg         = dc_fixpt_clamp_u0d14(rgb->red);
			rgb->green_reg       = dc_fixpt_clamp_u0d14(rgb->green);
			rgb->blue_reg        = dc_fixpt_clamp_u0d14(rgb->blue);
		}

		++rgb_plus_1;
		++rgb;
		++i;
	}
	cm_helper_convert_to_custom_float(rgb_resulted,
						lut_params->arr_points,
						hw_points, fixpoint);

	return true;
}

#define NUM_DEGAMMA_REGIONS    12


bool cm_helper_translate_curve_to_degamma_hw_format(
				const struct dc_transfer_func *output_tf,
				struct pwl_params *lut_params)
{
	struct curve_points *arr_points;
	struct pwl_result_data *rgb_resulted;
	struct pwl_result_data *rgb;
	struct pwl_result_data *rgb_plus_1;
	struct fixed31_32 y_r;
	struct fixed31_32 y_g;
	struct fixed31_32 y_b;
	struct fixed31_32 y1_min;
	struct fixed31_32 y3_max;

	int32_t region_start, region_end;
	int32_t i;
	uint32_t j, k, seg_distr[MAX_REGIONS_NUMBER], increment, start_index, hw_points;

	if (output_tf == NULL || lut_params == NULL || output_tf->type == TF_TYPE_BYPASS)
		return false;

	PERF_TRACE();

	arr_points = lut_params->arr_points;
	rgb_resulted = lut_params->rgb_resulted;
	hw_points = 0;

	memset(lut_params, 0, sizeof(struct pwl_params));
	memset(seg_distr, 0, sizeof(seg_distr));

	region_start = -NUM_DEGAMMA_REGIONS;
	region_end   = 0;


	for (i = region_end - region_start; i < MAX_REGIONS_NUMBER ; i++)
		seg_distr[i] = -1;
	/* 12 segments
	 * segments are from 2^-12 to 0
	 */
	for (i = 0; i < NUM_DEGAMMA_REGIONS ; i++)
		seg_distr[i] = 4;

	for (k = 0; k < MAX_REGIONS_NUMBER; k++) {
		if (seg_distr[k] != -1)
			hw_points += (1 << seg_distr[k]);
	}

	j = 0;
	for (k = 0; k < (region_end - region_start); k++) {
		increment = NUMBER_SW_SEGMENTS / (1 << seg_distr[k]);
		start_index = (region_start + k + MAX_LOW_POINT) *
				NUMBER_SW_SEGMENTS;
		for (i = start_index; i < start_index + NUMBER_SW_SEGMENTS;
				i += increment) {
			if (j == hw_points - 1)
				break;
			rgb_resulted[j].red = output_tf->tf_pts.red[i];
			rgb_resulted[j].green = output_tf->tf_pts.green[i];
			rgb_resulted[j].blue = output_tf->tf_pts.blue[i];
			j++;
		}
	}

	/* last point */
	start_index = (region_end + MAX_LOW_POINT) * NUMBER_SW_SEGMENTS;
	rgb_resulted[hw_points - 1].red = output_tf->tf_pts.red[start_index];
	rgb_resulted[hw_points - 1].green = output_tf->tf_pts.green[start_index];
	rgb_resulted[hw_points - 1].blue = output_tf->tf_pts.blue[start_index];

	arr_points[0].x = dc_fixpt_pow(dc_fixpt_from_int(2),
					     dc_fixpt_from_int(region_start));
	arr_points[1].x = dc_fixpt_pow(dc_fixpt_from_int(2),
					     dc_fixpt_from_int(region_end));

	y_r = rgb_resulted[0].red;
	y_g = rgb_resulted[0].green;
	y_b = rgb_resulted[0].blue;

	y1_min = dc_fixpt_min(y_r, dc_fixpt_min(y_g, y_b));

	arr_points[0].y = y1_min;
	arr_points[0].slope = dc_fixpt_div(arr_points[0].y, arr_points[0].x);
	y_r = rgb_resulted[hw_points - 1].red;
	y_g = rgb_resulted[hw_points - 1].green;
	y_b = rgb_resulted[hw_points - 1].blue;

	/* see comment above, m_arrPoints[1].y should be the Y value for the
	 * region end (m_numOfHwPoints), not last HW point(m_numOfHwPoints - 1)
	 */
	y3_max = dc_fixpt_max(y_r, dc_fixpt_max(y_g, y_b));

	arr_points[1].y = y3_max;

	arr_points[1].slope = dc_fixpt_zero;

	if (output_tf->tf == TRANSFER_FUNCTION_PQ) {
		/* for PQ, we want to have a straight line from last HW X point,
		 * and the slope to be such that we hit 1.0 at 10000 nits.
		 */
		const struct fixed31_32 end_value =
				dc_fixpt_from_int(125);

		arr_points[1].slope = dc_fixpt_div(
			dc_fixpt_sub(dc_fixpt_one, arr_points[1].y),
			dc_fixpt_sub(end_value, arr_points[1].x));
	}

	lut_params->hw_points_num = hw_points;

	k = 0;
	for (i = 1; i < MAX_REGIONS_NUMBER; i++) {
		if (seg_distr[k] != -1) {
			lut_params->arr_curve_points[k].segments_num =
					seg_distr[k];
			lut_params->arr_curve_points[i].offset =
					lut_params->arr_curve_points[k].offset + (1 << seg_distr[k]);
		}
		k++;
	}

	if (seg_distr[k] != -1)
		lut_params->arr_curve_points[k].segments_num = seg_distr[k];

	rgb = rgb_resulted;
	rgb_plus_1 = rgb_resulted + 1;

	i = 1;
	while (i != hw_points + 1) {
		if (dc_fixpt_lt(rgb_plus_1->red, rgb->red))
			rgb_plus_1->red = rgb->red;
		if (dc_fixpt_lt(rgb_plus_1->green, rgb->green))
			rgb_plus_1->green = rgb->green;
		if (dc_fixpt_lt(rgb_plus_1->blue, rgb->blue))
			rgb_plus_1->blue = rgb->blue;

		rgb->delta_red   = dc_fixpt_sub(rgb_plus_1->red,   rgb->red);
		rgb->delta_green = dc_fixpt_sub(rgb_plus_1->green, rgb->green);
		rgb->delta_blue  = dc_fixpt_sub(rgb_plus_1->blue,  rgb->blue);

		++rgb_plus_1;
		++rgb;
		++i;
	}
	cm_helper_convert_to_custom_float(rgb_resulted,
						lut_params->arr_points,
						hw_points, false);

	return true;
}
