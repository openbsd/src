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

/**
 * This file defines helper functions provided by the Display Manager to
 * Display Core.
 */
#ifndef __DM_HELPERS__
#define __DM_HELPERS__

#include "dc_types.h"
#include "dc.h"

struct dp_mst_stream_allocation_table;

enum dc_edid_status dm_helpers_parse_edid_caps(
	struct dc_context *ctx,
	const struct dc_edid *edid,
	struct dc_edid_caps *edid_caps);


/*
 * Update DP branch info
 */
void dm_helpers_dp_update_branch_info(
		struct dc_context *ctx,
		const struct dc_link *link);

/*
 * Writes payload allocation table in immediate downstream device.
 */
bool dm_helpers_dp_mst_write_payload_allocation_table(
		struct dc_context *ctx,
		const struct dc_stream_state *stream,
		struct dp_mst_stream_allocation_table *proposed_table,
		bool enable);

/*
 * Clear payload allocation table before enable MST DP link.
 */
void dm_helpers_dp_mst_clear_payload_allocation_table(
	struct dc_context *ctx,
	const struct dc_link *link);

/*
 * Polls for ACT (allocation change trigger) handled and
 */
bool dm_helpers_dp_mst_poll_for_allocation_change_trigger(
		struct dc_context *ctx,
		const struct dc_stream_state *stream);
/*
 * Sends ALLOCATE_PAYLOAD message.
 */
bool dm_helpers_dp_mst_send_payload_allocation(
		struct dc_context *ctx,
		const struct dc_stream_state *stream,
		bool enable);

bool dm_helpers_dp_mst_start_top_mgr(
		struct dc_context *ctx,
		const struct dc_link *link,
		bool boot);

void dm_helpers_dp_mst_stop_top_mgr(
		struct dc_context *ctx,
		const struct dc_link *link);
/**
 * OS specific aux read callback.
 */
bool dm_helpers_dp_read_dpcd(
		struct dc_context *ctx,
		const struct dc_link *link,
		uint32_t address,
		uint8_t *data,
		uint32_t size);

/**
 * OS specific aux write callback.
 */
bool dm_helpers_dp_write_dpcd(
		struct dc_context *ctx,
		const struct dc_link *link,
		uint32_t address,
		const uint8_t *data,
		uint32_t size);

bool dm_helpers_submit_i2c(
		struct dc_context *ctx,
		const struct dc_link *link,
		struct i2c_command *cmd);

bool dm_helpers_is_dp_sink_present(
		struct dc_link *link);

enum dc_edid_status dm_helpers_read_local_edid(
		struct dc_context *ctx,
		struct dc_link *link,
		struct dc_sink *sink);

void dm_set_dcn_clocks(
		struct dc_context *ctx,
		struct dc_clocks *clks);

#endif /* __DM_HELPERS__ */
