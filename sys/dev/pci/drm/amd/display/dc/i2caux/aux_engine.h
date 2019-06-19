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

#ifndef __DAL_AUX_ENGINE_H__
#define __DAL_AUX_ENGINE_H__

#include "dc_ddc_types.h"

struct aux_engine;

struct aux_engine_funcs {
	void (*destroy)(
		struct aux_engine **ptr);
	bool (*acquire_engine)(
		struct aux_engine *engine);
	void (*configure)(
		struct aux_engine *engine,
		union aux_config cfg);
	void (*submit_channel_request)(
		struct aux_engine *engine,
		struct aux_request_transaction_data *request);
	void (*process_channel_reply)(
		struct aux_engine *engine,
		struct aux_reply_transaction_data *reply);
	int (*read_channel_reply)(
		struct aux_engine *engine,
		uint32_t size,
		uint8_t *buffer,
		uint8_t *reply_result,
		uint32_t *sw_status);
	enum aux_channel_operation_result (*get_channel_status)(
		struct aux_engine *engine,
		uint8_t *returned_bytes);
	bool (*is_engine_available) (
		struct aux_engine *engine);
};

struct aux_engine {
	struct engine base;
	const struct aux_engine_funcs *funcs;
	/* following values are expressed in milliseconds */
	uint32_t delay;
	uint32_t max_defer_write_retry;

	bool acquire_reset;
};

void dal_aux_engine_construct(
	struct aux_engine *engine,
	struct dc_context *ctx);

void dal_aux_engine_destruct(
	struct aux_engine *engine);
bool dal_aux_engine_submit_request(
	struct engine *ptr,
	struct i2caux_transaction_request *request,
	bool middle_of_transaction);
bool dal_aux_engine_acquire(
	struct engine *ptr,
	struct ddc *ddc);
enum i2caux_engine_type dal_aux_engine_get_engine_type(
	const struct engine *engine);

#endif
