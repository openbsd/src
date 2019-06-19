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

#include "dm_services.h"
#include "dc.h"
#include "core_types.h"
#include "resource.h"
#include "ipp.h"
#include "timing_generator.h"

#define DC_LOGGER dc->ctx->logger

/*******************************************************************************
 * Private functions
 ******************************************************************************/
void update_stream_signal(struct dc_stream_state *stream)
{

	struct dc_sink *dc_sink = stream->sink;

	if (dc_sink->sink_signal == SIGNAL_TYPE_NONE)
		stream->signal = stream->sink->link->connector_signal;
	else
		stream->signal = dc_sink->sink_signal;

	if (dc_is_dvi_signal(stream->signal)) {
		if (stream->ctx->dc->caps.dual_link_dvi &&
		    stream->timing.pix_clk_khz > TMDS_MAX_PIXEL_CLOCK &&
		    stream->sink->sink_signal != SIGNAL_TYPE_DVI_SINGLE_LINK)
			stream->signal = SIGNAL_TYPE_DVI_DUAL_LINK;
		else
			stream->signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
	}
}

static void construct(struct dc_stream_state *stream,
	struct dc_sink *dc_sink_data)
{
	uint32_t i = 0;

	stream->sink = dc_sink_data;
	stream->ctx = stream->sink->ctx;

	dc_sink_retain(dc_sink_data);

	/* Copy audio modes */
	/* TODO - Remove this translation */
	for (i = 0; i < (dc_sink_data->edid_caps.audio_mode_count); i++)
	{
		stream->audio_info.modes[i].channel_count = dc_sink_data->edid_caps.audio_modes[i].channel_count;
		stream->audio_info.modes[i].format_code = dc_sink_data->edid_caps.audio_modes[i].format_code;
		stream->audio_info.modes[i].sample_rates.all = dc_sink_data->edid_caps.audio_modes[i].sample_rate;
		stream->audio_info.modes[i].sample_size = dc_sink_data->edid_caps.audio_modes[i].sample_size;
	}
	stream->audio_info.mode_count = dc_sink_data->edid_caps.audio_mode_count;
	stream->audio_info.audio_latency = dc_sink_data->edid_caps.audio_latency;
	stream->audio_info.video_latency = dc_sink_data->edid_caps.video_latency;
	memmove(
		stream->audio_info.display_name,
		dc_sink_data->edid_caps.display_name,
		AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS);
	stream->audio_info.manufacture_id = dc_sink_data->edid_caps.manufacturer_id;
	stream->audio_info.product_id = dc_sink_data->edid_caps.product_id;
	stream->audio_info.flags.all = dc_sink_data->edid_caps.speaker_flags;

	if (dc_sink_data->dc_container_id != NULL) {
		struct dc_container_id *dc_container_id = dc_sink_data->dc_container_id;

		stream->audio_info.port_id[0] = dc_container_id->portId[0];
		stream->audio_info.port_id[1] = dc_container_id->portId[1];
	} else {
		/* TODO - WindowDM has implemented,
		other DMs need Unhardcode port_id */
		stream->audio_info.port_id[0] = 0x5558859e;
		stream->audio_info.port_id[1] = 0xd989449;
	}

	/* EDID CAP translation for HDMI 2.0 */
	stream->timing.flags.LTE_340MCSC_SCRAMBLE = dc_sink_data->edid_caps.lte_340mcsc_scramble;

	stream->status.link = stream->sink->link;

	update_stream_signal(stream);

	stream->out_transfer_func = dc_create_transfer_func();
	stream->out_transfer_func->type = TF_TYPE_BYPASS;
}

static void destruct(struct dc_stream_state *stream)
{
	dc_sink_release(stream->sink);
	if (stream->out_transfer_func != NULL) {
		dc_transfer_func_release(stream->out_transfer_func);
		stream->out_transfer_func = NULL;
	}
}

void dc_stream_retain(struct dc_stream_state *stream)
{
	kref_get(&stream->refcount);
}

static void dc_stream_free(struct kref *kref)
{
	struct dc_stream_state *stream = container_of(kref, struct dc_stream_state, refcount);

	destruct(stream);
	kfree(stream);
}

void dc_stream_release(struct dc_stream_state *stream)
{
	if (stream != NULL) {
		kref_put(&stream->refcount, dc_stream_free);
	}
}

struct dc_stream_state *dc_create_stream_for_sink(
		struct dc_sink *sink)
{
	struct dc_stream_state *stream;

	if (sink == NULL)
		return NULL;

	stream = kzalloc(sizeof(struct dc_stream_state), GFP_KERNEL);
	if (stream == NULL)
		return NULL;

	construct(stream, sink);

	kref_init(&stream->refcount);

	return stream;
}

struct dc_stream_status *dc_stream_get_status(
	struct dc_stream_state *stream)
{
	uint8_t i;
	struct dc  *dc = stream->ctx->dc;

	for (i = 0; i < dc->current_state->stream_count; i++) {
		if (stream == dc->current_state->streams[i])
			return &dc->current_state->stream_status[i];
	}

	return NULL;
}

/**
 * Update the cursor attributes and set cursor surface address
 */
bool dc_stream_set_cursor_attributes(
	struct dc_stream_state *stream,
	const struct dc_cursor_attributes *attributes)
{
	int i;
	struct dc  *core_dc;
	struct resource_context *res_ctx;
	struct pipe_ctx *pipe_to_program = NULL;

	if (NULL == stream) {
		dm_error("DC: dc_stream is NULL!\n");
		return false;
	}
	if (NULL == attributes) {
		dm_error("DC: attributes is NULL!\n");
		return false;
	}

	if (attributes->address.quad_part == 0) {
		dm_output_to_console("DC: Cursor address is 0!\n");
		return false;
	}

	core_dc = stream->ctx->dc;
	res_ctx = &core_dc->current_state->res_ctx;
	stream->cursor_attributes = *attributes;

	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx = &res_ctx->pipe_ctx[i];

		if (pipe_ctx->stream != stream)
			continue;
		if (pipe_ctx->top_pipe && pipe_ctx->plane_state != pipe_ctx->top_pipe->plane_state)
			continue;

		if (!pipe_to_program) {
			pipe_to_program = pipe_ctx;
			core_dc->hwss.pipe_control_lock(core_dc, pipe_to_program, true);
		}

		core_dc->hwss.set_cursor_attribute(pipe_ctx);
		if (core_dc->hwss.set_cursor_sdr_white_level)
			core_dc->hwss.set_cursor_sdr_white_level(pipe_ctx);
	}

	if (pipe_to_program)
		core_dc->hwss.pipe_control_lock(core_dc, pipe_to_program, false);

	return true;
}

bool dc_stream_set_cursor_position(
	struct dc_stream_state *stream,
	const struct dc_cursor_position *position)
{
	int i;
	struct dc  *core_dc;
	struct resource_context *res_ctx;
	struct pipe_ctx *pipe_to_program = NULL;

	if (NULL == stream) {
		dm_error("DC: dc_stream is NULL!\n");
		return false;
	}

	if (NULL == position) {
		dm_error("DC: cursor position is NULL!\n");
		return false;
	}

	core_dc = stream->ctx->dc;
	res_ctx = &core_dc->current_state->res_ctx;
	stream->cursor_position = *position;

	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx = &res_ctx->pipe_ctx[i];

		if (pipe_ctx->stream != stream ||
				(!pipe_ctx->plane_res.mi  && !pipe_ctx->plane_res.hubp) ||
				!pipe_ctx->plane_state ||
				(!pipe_ctx->plane_res.xfm && !pipe_ctx->plane_res.dpp) ||
				!pipe_ctx->plane_res.ipp)
			continue;

		if (!pipe_to_program) {
			pipe_to_program = pipe_ctx;
			core_dc->hwss.pipe_control_lock(core_dc, pipe_to_program, true);
		}

		core_dc->hwss.set_cursor_position(pipe_ctx);
	}

	if (pipe_to_program)
		core_dc->hwss.pipe_control_lock(core_dc, pipe_to_program, false);

	return true;
}

uint32_t dc_stream_get_vblank_counter(const struct dc_stream_state *stream)
{
	uint8_t i;
	struct dc  *core_dc = stream->ctx->dc;
	struct resource_context *res_ctx =
		&core_dc->current_state->res_ctx;

	for (i = 0; i < MAX_PIPES; i++) {
		struct timing_generator *tg = res_ctx->pipe_ctx[i].stream_res.tg;

		if (res_ctx->pipe_ctx[i].stream != stream)
			continue;

		return tg->funcs->get_frame_count(tg);
	}

	return 0;
}

bool dc_stream_get_scanoutpos(const struct dc_stream_state *stream,
				  uint32_t *v_blank_start,
				  uint32_t *v_blank_end,
				  uint32_t *h_position,
				  uint32_t *v_position)
{
	uint8_t i;
	bool ret = false;
	struct dc  *core_dc = stream->ctx->dc;
	struct resource_context *res_ctx =
		&core_dc->current_state->res_ctx;

	for (i = 0; i < MAX_PIPES; i++) {
		struct timing_generator *tg = res_ctx->pipe_ctx[i].stream_res.tg;

		if (res_ctx->pipe_ctx[i].stream != stream)
			continue;

		tg->funcs->get_scanoutpos(tg,
					  v_blank_start,
					  v_blank_end,
					  h_position,
					  v_position);

		ret = true;
		break;
	}

	return ret;
}

void dc_stream_log(const struct dc *dc, const struct dc_stream_state *stream)
{
	DC_LOG_DC(
			"core_stream 0x%p: src: %d, %d, %d, %d; dst: %d, %d, %d, %d, colorSpace:%d\n",
			stream,
			stream->src.x,
			stream->src.y,
			stream->src.width,
			stream->src.height,
			stream->dst.x,
			stream->dst.y,
			stream->dst.width,
			stream->dst.height,
			stream->output_color_space);
	DC_LOG_DC(
			"\tpix_clk_khz: %d, h_total: %d, v_total: %d, pixelencoder:%d, displaycolorDepth:%d\n",
			stream->timing.pix_clk_khz,
			stream->timing.h_total,
			stream->timing.v_total,
			stream->timing.pixel_encoding,
			stream->timing.display_color_depth);
	DC_LOG_DC(
			"\tsink name: %s, serial: %d\n",
			stream->sink->edid_caps.display_name,
			stream->sink->edid_caps.serial_number);
	DC_LOG_DC(
			"\tlink: %d\n",
			stream->sink->link->link_index);
}
