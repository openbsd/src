/*
 * Copyright 2012-15 Advanced Micro Devices, Inc.
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

#ifndef __DAL_DCHUBBUB_H__
#define __DAL_DCHUBBUB_H__


enum dcc_control {
	dcc_control__256_256_xxx,
	dcc_control__128_128_xxx,
	dcc_control__256_64_64,
};

enum segment_order {
	segment_order__na,
	segment_order__contiguous,
	segment_order__non_contiguous,
};


struct hubbub_funcs {
	void (*update_dchub)(
			struct hubbub *hubbub,
			struct dchub_init_data *dh_data);

	bool (*get_dcc_compression_cap)(struct hubbub *hubbub,
			const struct dc_dcc_surface_param *input,
			struct dc_surface_dcc_cap *output);

	bool (*dcc_support_swizzle)(
			enum swizzle_mode_values swizzle,
			unsigned int bytes_per_element,
			enum segment_order *segment_order_horz,
			enum segment_order *segment_order_vert);

	bool (*dcc_support_pixel_format)(
			enum surface_pixel_format format,
			unsigned int *bytes_per_element);
};


#endif
