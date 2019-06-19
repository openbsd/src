/*
 * Copyright 2015 Advanced Micro Devices, Inc.
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
 */

#include "dm_services.h"

#include "dc.h"

#include "core_status.h"
#include "core_types.h"
#include "hw_sequencer.h"
#include "dce/dce_hwseq.h"

#include "resource.h"

#include "clock_source.h"
#include "dc_bios_types.h"

#include "bios_parser_interface.h"
#include "include/irq_service_interface.h"
#include "transform.h"
#include "dmcu.h"
#include "dpp.h"
#include "timing_generator.h"
#include "abm.h"
#include "virtual/virtual_link_encoder.h"

#include "link_hwss.h"
#include "link_encoder.h"

#include "dc_link_ddc.h"
#include "dm_helpers.h"
#include "mem_input.h"
#include "hubp.h"

#include "dc_link_dp.h"
#define DC_LOGGER \
	dc->ctx->logger


/*******************************************************************************
 * Private functions
 ******************************************************************************/

static inline void elevate_update_type(enum surface_update_type *original, enum surface_update_type new)
{
	if (new > *original)
		*original = new;
}

static void destroy_links(struct dc *dc)
{
	uint32_t i;

	for (i = 0; i < dc->link_count; i++) {
		if (NULL != dc->links[i])
			link_destroy(&dc->links[i]);
	}
}

static bool create_links(
		struct dc *dc,
		uint32_t num_virtual_links)
{
	int i;
	int connectors_num;
	struct dc_bios *bios = dc->ctx->dc_bios;

	dc->link_count = 0;

	connectors_num = bios->funcs->get_connectors_number(bios);

	if (connectors_num > ENUM_ID_COUNT) {
		dm_error(
			"DC: Number of connectors %d exceeds maximum of %d!\n",
			connectors_num,
			ENUM_ID_COUNT);
		return false;
	}

	if (connectors_num == 0 && num_virtual_links == 0) {
		dm_error("DC: Number of connectors is zero!\n");
	}

	dm_output_to_console(
		"DC: %s: connectors_num: physical:%d, virtual:%d\n",
		__func__,
		connectors_num,
		num_virtual_links);

	for (i = 0; i < connectors_num; i++) {
		struct link_init_data link_init_params = {0};
		struct dc_link *link;

		link_init_params.ctx = dc->ctx;
		/* next BIOS object table connector */
		link_init_params.connector_index = i;
		link_init_params.link_index = dc->link_count;
		link_init_params.dc = dc;
		link = link_create(&link_init_params);

		if (link) {
			dc->links[dc->link_count] = link;
			link->dc = dc;
			++dc->link_count;
		}
	}

	for (i = 0; i < num_virtual_links; i++) {
		struct dc_link *link = kzalloc(sizeof(*link), GFP_KERNEL);
		struct encoder_init_data enc_init = {0};

		if (link == NULL) {
			BREAK_TO_DEBUGGER();
			goto failed_alloc;
		}

		link->link_index = dc->link_count;
		dc->links[dc->link_count] = link;
		dc->link_count++;

		link->ctx = dc->ctx;
		link->dc = dc;
		link->connector_signal = SIGNAL_TYPE_VIRTUAL;
		link->link_id.type = OBJECT_TYPE_CONNECTOR;
		link->link_id.id = CONNECTOR_ID_VIRTUAL;
		link->link_id.enum_id = ENUM_ID_1;
		link->link_enc = kzalloc(sizeof(*link->link_enc), GFP_KERNEL);

		if (!link->link_enc) {
			BREAK_TO_DEBUGGER();
			goto failed_alloc;
		}

		link->link_status.dpcd_caps = &link->dpcd_caps;

		enc_init.ctx = dc->ctx;
		enc_init.channel = CHANNEL_ID_UNKNOWN;
		enc_init.hpd_source = HPD_SOURCEID_UNKNOWN;
		enc_init.transmitter = TRANSMITTER_UNKNOWN;
		enc_init.connector = link->link_id;
		enc_init.encoder.type = OBJECT_TYPE_ENCODER;
		enc_init.encoder.id = ENCODER_ID_INTERNAL_VIRTUAL;
		enc_init.encoder.enum_id = ENUM_ID_1;
		virtual_link_encoder_construct(link->link_enc, &enc_init);
	}

	return true;

failed_alloc:
	return false;
}

/**
 *****************************************************************************
 *  Function: dc_stream_adjust_vmin_vmax
 *
 *  @brief
 *     Looks up the pipe context of dc_stream_state and updates the
 *     vertical_total_min and vertical_total_max of the DRR, Dynamic Refresh
 *     Rate, which is a power-saving feature that targets reducing panel
 *     refresh rate while the screen is static
 *
 *  @param [in] dc: dc reference
 *  @param [in] stream: Initial dc stream state
 *  @param [in] adjust: Updated parameters for vertical_total_min and
 *  vertical_total_max
 *****************************************************************************
 */
bool dc_stream_adjust_vmin_vmax(struct dc *dc,
		struct dc_stream_state **streams, int num_streams,
		int vmin, int vmax)
{
	/* TODO: Support multiple streams */
	struct dc_stream_state *stream = streams[0];
	int i = 0;
	bool ret = false;

	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe = &dc->current_state->res_ctx.pipe_ctx[i];

		if (pipe->stream == stream && pipe->stream_res.stream_enc) {
			dc->hwss.set_drr(&pipe, 1, vmin, vmax);

			/* build and update the info frame */
			resource_build_info_frame(pipe);
			dc->hwss.update_info_frame(pipe);

			ret = true;
		}
	}
	return ret;
}

bool dc_stream_get_crtc_position(struct dc *dc,
		struct dc_stream_state **streams, int num_streams,
		unsigned int *v_pos, unsigned int *nom_v_pos)
{
	/* TODO: Support multiple streams */
	struct dc_stream_state *stream = streams[0];
	int i = 0;
	bool ret = false;
	struct crtc_position position;

	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe =
				&dc->current_state->res_ctx.pipe_ctx[i];

		if (pipe->stream == stream && pipe->stream_res.stream_enc) {
			dc->hwss.get_position(&pipe, 1, &position);

			*v_pos = position.vertical_count;
			*nom_v_pos = position.nominal_vcount;
			ret = true;
		}
	}
	return ret;
}

/**
 * dc_stream_configure_crc: Configure CRC capture for the given stream.
 * @dc: DC Object
 * @stream: The stream to configure CRC on.
 * @enable: Enable CRC if true, disable otherwise.
 * @continuous: Capture CRC on every frame if true. Otherwise, only capture
 *              once.
 *
 * By default, only CRC0 is configured, and the entire frame is used to
 * calculate the crc.
 */
bool dc_stream_configure_crc(struct dc *dc, struct dc_stream_state *stream,
			     bool enable, bool continuous)
{
	int i;
	struct pipe_ctx *pipe;
	struct crc_params param;
	struct timing_generator *tg;

	for (i = 0; i < MAX_PIPES; i++) {
		pipe = &dc->current_state->res_ctx.pipe_ctx[i];
		if (pipe->stream == stream)
			break;
	}
	/* Stream not found */
	if (i == MAX_PIPES)
		return false;

	/* Always capture the full frame */
	param.windowa_x_start = 0;
	param.windowa_y_start = 0;
	param.windowa_x_end = pipe->stream->timing.h_addressable;
	param.windowa_y_end = pipe->stream->timing.v_addressable;
	param.windowb_x_start = 0;
	param.windowb_y_start = 0;
	param.windowb_x_end = pipe->stream->timing.h_addressable;
	param.windowb_y_end = pipe->stream->timing.v_addressable;

	/* Default to the union of both windows */
	param.selection = UNION_WINDOW_A_B;
	param.continuous_mode = continuous;
	param.enable = enable;

	tg = pipe->stream_res.tg;

	/* Only call if supported */
	if (tg->funcs->configure_crc)
		return tg->funcs->configure_crc(tg, &param);
	DC_LOG_WARNING("CRC capture not supported.");
	return false;
}

/**
 * dc_stream_get_crc: Get CRC values for the given stream.
 * @dc: DC object
 * @stream: The DC stream state of the stream to get CRCs from.
 * @r_cr, g_y, b_cb: CRC values for the three channels are stored here.
 *
 * dc_stream_configure_crc needs to be called beforehand to enable CRCs.
 * Return false if stream is not found, or if CRCs are not enabled.
 */
bool dc_stream_get_crc(struct dc *dc, struct dc_stream_state *stream,
		       uint32_t *r_cr, uint32_t *g_y, uint32_t *b_cb)
{
	int i;
	struct pipe_ctx *pipe;
	struct timing_generator *tg;

	for (i = 0; i < MAX_PIPES; i++) {
		pipe = &dc->current_state->res_ctx.pipe_ctx[i];
		if (pipe->stream == stream)
			break;
	}
	/* Stream not found */
	if (i == MAX_PIPES)
		return false;

	tg = pipe->stream_res.tg;

	if (tg->funcs->get_crc)
		return tg->funcs->get_crc(tg, r_cr, g_y, b_cb);
	DC_LOG_WARNING("CRC capture not supported.");
	return false;
}

void dc_stream_set_dither_option(struct dc_stream_state *stream,
		enum dc_dither_option option)
{
	struct bit_depth_reduction_params params;
	struct dc_link *link = stream->status.link;
	struct pipe_ctx *pipes = NULL;
	int i;

	for (i = 0; i < MAX_PIPES; i++) {
		if (link->dc->current_state->res_ctx.pipe_ctx[i].stream ==
				stream) {
			pipes = &link->dc->current_state->res_ctx.pipe_ctx[i];
			break;
		}
	}

	if (!pipes)
		return;
	if (option > DITHER_OPTION_MAX)
		return;

	stream->dither_option = option;

	memset(&params, 0, sizeof(params));
	resource_build_bit_depth_reduction_params(stream, &params);
	stream->bit_depth_params = params;

	if (pipes->plane_res.xfm &&
	    pipes->plane_res.xfm->funcs->transform_set_pixel_storage_depth) {
		pipes->plane_res.xfm->funcs->transform_set_pixel_storage_depth(
			pipes->plane_res.xfm,
			pipes->plane_res.scl_data.lb_params.depth,
			&stream->bit_depth_params);
	}

	pipes->stream_res.opp->funcs->
		opp_program_bit_depth_reduction(pipes->stream_res.opp, &params);
}

void dc_stream_set_static_screen_events(struct dc *dc,
		struct dc_stream_state **streams,
		int num_streams,
		const struct dc_static_screen_events *events)
{
	int i = 0;
	int j = 0;
	struct pipe_ctx *pipes_affected[MAX_PIPES];
	int num_pipes_affected = 0;

	for (i = 0; i < num_streams; i++) {
		struct dc_stream_state *stream = streams[i];

		for (j = 0; j < MAX_PIPES; j++) {
			if (dc->current_state->res_ctx.pipe_ctx[j].stream
					== stream) {
				pipes_affected[num_pipes_affected++] =
						&dc->current_state->res_ctx.pipe_ctx[j];
			}
		}
	}

	dc->hwss.set_static_screen_control(pipes_affected, num_pipes_affected, events);
}

void dc_link_set_drive_settings(struct dc *dc,
				struct link_training_settings *lt_settings,
				const struct dc_link *link)
{

	int i;

	for (i = 0; i < dc->link_count; i++) {
		if (dc->links[i] == link)
			break;
	}

	if (i >= dc->link_count)
		ASSERT_CRITICAL(false);

	dc_link_dp_set_drive_settings(dc->links[i], lt_settings);
}

void dc_link_perform_link_training(struct dc *dc,
				   struct dc_link_settings *link_setting,
				   bool skip_video_pattern)
{
	int i;

	for (i = 0; i < dc->link_count; i++)
		dc_link_dp_perform_link_training(
			dc->links[i],
			link_setting,
			skip_video_pattern);
}

void dc_link_set_preferred_link_settings(struct dc *dc,
					 struct dc_link_settings *link_setting,
					 struct dc_link *link)
{
	struct dc_link_settings store_settings = *link_setting;
	struct dc_stream_state *link_stream =
		link->dc->current_state->res_ctx.pipe_ctx[0].stream;

	link->preferred_link_setting = store_settings;
	if (link_stream)
		decide_link_settings(link_stream, &store_settings);

	if ((store_settings.lane_count != LANE_COUNT_UNKNOWN) &&
		(store_settings.link_rate != LINK_RATE_UNKNOWN))
		dp_retrain_link_dp_test(link, &store_settings, false);
}

void dc_link_enable_hpd(const struct dc_link *link)
{
	dc_link_dp_enable_hpd(link);
}

void dc_link_disable_hpd(const struct dc_link *link)
{
	dc_link_dp_disable_hpd(link);
}


void dc_link_set_test_pattern(struct dc_link *link,
			      enum dp_test_pattern test_pattern,
			      const struct link_training_settings *p_link_settings,
			      const unsigned char *p_custom_pattern,
			      unsigned int cust_pattern_size)
{
	if (link != NULL)
		dc_link_dp_set_test_pattern(
			link,
			test_pattern,
			p_link_settings,
			p_custom_pattern,
			cust_pattern_size);
}

static void destruct(struct dc *dc)
{
	dc_release_state(dc->current_state);
	dc->current_state = NULL;

	destroy_links(dc);

	dc_destroy_resource_pool(dc);

	if (dc->ctx->gpio_service)
		dal_gpio_service_destroy(&dc->ctx->gpio_service);

	if (dc->ctx->i2caux)
		dal_i2caux_destroy(&dc->ctx->i2caux);

	if (dc->ctx->created_bios)
		dal_bios_parser_destroy(&dc->ctx->dc_bios);

	kfree(dc->ctx);
	dc->ctx = NULL;

	kfree(dc->bw_vbios);
	dc->bw_vbios = NULL;

	kfree(dc->bw_dceip);
	dc->bw_dceip = NULL;

#ifdef CONFIG_DRM_AMD_DC_DCN1_0
	kfree(dc->dcn_soc);
	dc->dcn_soc = NULL;

	kfree(dc->dcn_ip);
	dc->dcn_ip = NULL;

#endif
}

static bool construct(struct dc *dc,
		const struct dc_init_data *init_params)
{
	struct dc_context *dc_ctx;
	struct bw_calcs_dceip *dc_dceip;
	struct bw_calcs_vbios *dc_vbios;
#ifdef CONFIG_DRM_AMD_DC_DCN1_0
	struct dcn_soc_bounding_box *dcn_soc;
	struct dcn_ip_params *dcn_ip;
#endif

	enum dce_version dc_version = DCE_VERSION_UNKNOWN;

	dc_dceip = kzalloc(sizeof(*dc_dceip), GFP_KERNEL);
	if (!dc_dceip) {
		dm_error("%s: failed to create dceip\n", __func__);
		goto fail;
	}

	dc->bw_dceip = dc_dceip;

	dc_vbios = kzalloc(sizeof(*dc_vbios), GFP_KERNEL);
	if (!dc_vbios) {
		dm_error("%s: failed to create vbios\n", __func__);
		goto fail;
	}

	dc->bw_vbios = dc_vbios;
#ifdef CONFIG_DRM_AMD_DC_DCN1_0
	dcn_soc = kzalloc(sizeof(*dcn_soc), GFP_KERNEL);
	if (!dcn_soc) {
		dm_error("%s: failed to create dcn_soc\n", __func__);
		goto fail;
	}

	dc->dcn_soc = dcn_soc;

	dcn_ip = kzalloc(sizeof(*dcn_ip), GFP_KERNEL);
	if (!dcn_ip) {
		dm_error("%s: failed to create dcn_ip\n", __func__);
		goto fail;
	}

	dc->dcn_ip = dcn_ip;
#endif

	dc_ctx = kzalloc(sizeof(*dc_ctx), GFP_KERNEL);
	if (!dc_ctx) {
		dm_error("%s: failed to create ctx\n", __func__);
		goto fail;
	}

	dc_ctx->cgs_device = init_params->cgs_device;
	dc_ctx->driver_context = init_params->driver;
	dc_ctx->dc = dc;
	dc_ctx->asic_id = init_params->asic_id;
	dc_ctx->dc_sink_id_count = 0;
	dc->ctx = dc_ctx;

	dc->current_state = dc_create_state();

	if (!dc->current_state) {
		dm_error("%s: failed to create validate ctx\n", __func__);
		goto fail;
	}

	/* Create logger */

	dc_ctx->dce_environment = init_params->dce_environment;

	dc_version = resource_parse_asic_id(init_params->asic_id);
	dc_ctx->dce_version = dc_version;

	/* Resource should construct all asic specific resources.
	 * This should be the only place where we need to parse the asic id
	 */
	if (init_params->vbios_override)
		dc_ctx->dc_bios = init_params->vbios_override;
	else {
		/* Create BIOS parser */
		struct bp_init_data bp_init_data;

		bp_init_data.ctx = dc_ctx;
		bp_init_data.bios = init_params->asic_id.atombios_base_address;

		dc_ctx->dc_bios = dal_bios_parser_create(
				&bp_init_data, dc_version);

		if (!dc_ctx->dc_bios) {
			ASSERT_CRITICAL(false);
			goto fail;
		}

		dc_ctx->created_bios = true;
		}

	/* Create I2C AUX */
	dc_ctx->i2caux = dal_i2caux_create(dc_ctx);

	if (!dc_ctx->i2caux) {
		ASSERT_CRITICAL(false);
		goto fail;
	}

	/* Create GPIO service */
	dc_ctx->gpio_service = dal_gpio_service_create(
			dc_version,
			dc_ctx->dce_environment,
			dc_ctx);

	if (!dc_ctx->gpio_service) {
		ASSERT_CRITICAL(false);
		goto fail;
	}

	dc->res_pool = dc_create_resource_pool(
			dc,
			init_params->num_virtual_links,
			dc_version,
			init_params->asic_id);
	if (!dc->res_pool)
		goto fail;

	dc_resource_state_construct(dc, dc->current_state);

	if (!create_links(dc, init_params->num_virtual_links))
		goto fail;

	return true;

fail:

	destruct(dc);
	return false;
}

static void disable_dangling_plane(struct dc *dc, struct dc_state *context)
{
	int i, j;
	struct dc_state *dangling_context = dc_create_state();
	struct dc_state *current_ctx;

	if (dangling_context == NULL)
		return;

	dc_resource_state_copy_construct(dc->current_state, dangling_context);

	for (i = 0; i < dc->res_pool->pipe_count; i++) {
		struct dc_stream_state *old_stream =
				dc->current_state->res_ctx.pipe_ctx[i].stream;
		bool should_disable = true;

		for (j = 0; j < context->stream_count; j++) {
			if (old_stream == context->streams[j]) {
				should_disable = false;
				break;
			}
		}
		if (should_disable && old_stream) {
			dc_rem_all_planes_for_stream(dc, old_stream, dangling_context);
			dc->hwss.apply_ctx_for_surface(dc, old_stream, 0, dangling_context);
		}
	}

	current_ctx = dc->current_state;
	dc->current_state = dangling_context;
	dc_release_state(current_ctx);
}

/*******************************************************************************
 * Public functions
 ******************************************************************************/

struct dc *dc_create(const struct dc_init_data *init_params)
{
	struct dc *dc = kzalloc(sizeof(*dc), GFP_KERNEL);
	unsigned int full_pipe_count;

	if (NULL == dc)
		goto alloc_fail;

	if (false == construct(dc, init_params))
		goto construct_fail;

	/*TODO: separate HW and SW initialization*/
	dc->hwss.init_hw(dc);

	full_pipe_count = dc->res_pool->pipe_count;
	if (dc->res_pool->underlay_pipe_index != NO_UNDERLAY_PIPE)
		full_pipe_count--;
	dc->caps.max_streams = min(
			full_pipe_count,
			dc->res_pool->stream_enc_count);

	dc->caps.max_links = dc->link_count;
	dc->caps.max_audios = dc->res_pool->audio_count;
	dc->caps.linear_pitch_alignment = 64;

	/* Populate versioning information */
	dc->versions.dc_ver = DC_VER;

	if (dc->res_pool->dmcu != NULL)
		dc->versions.dmcu_version = dc->res_pool->dmcu->dmcu_version;

	dc->config = init_params->flags;

	DC_LOG_DC("Display Core initialized\n");


	/* TODO: missing feature to be enabled */
	dc->debug.disable_dfs_bypass = true;

	return dc;

construct_fail:
	kfree(dc);

alloc_fail:
	return NULL;
}

void dc_destroy(struct dc **dc)
{
	destruct(*dc);
	kfree(*dc);
	*dc = NULL;
}

static void enable_timing_multisync(
		struct dc *dc,
		struct dc_state *ctx)
{
	int i = 0, multisync_count = 0;
	int pipe_count = dc->res_pool->pipe_count;
	struct pipe_ctx *multisync_pipes[MAX_PIPES] = { NULL };

	for (i = 0; i < pipe_count; i++) {
		if (!ctx->res_ctx.pipe_ctx[i].stream ||
				!ctx->res_ctx.pipe_ctx[i].stream->triggered_crtc_reset.enabled)
			continue;
		if (ctx->res_ctx.pipe_ctx[i].stream == ctx->res_ctx.pipe_ctx[i].stream->triggered_crtc_reset.event_source)
			continue;
		multisync_pipes[multisync_count] = &ctx->res_ctx.pipe_ctx[i];
		multisync_count++;
	}

	if (multisync_count > 0) {
		dc->hwss.enable_per_frame_crtc_position_reset(
			dc, multisync_count, multisync_pipes);
	}
}

static void program_timing_sync(
		struct dc *dc,
		struct dc_state *ctx)
{
	int i, j;
	int group_index = 0;
	int pipe_count = dc->res_pool->pipe_count;
	struct pipe_ctx *unsynced_pipes[MAX_PIPES] = { NULL };

	for (i = 0; i < pipe_count; i++) {
		if (!ctx->res_ctx.pipe_ctx[i].stream || ctx->res_ctx.pipe_ctx[i].top_pipe)
			continue;

		unsynced_pipes[i] = &ctx->res_ctx.pipe_ctx[i];
	}

	for (i = 0; i < pipe_count; i++) {
		int group_size = 1;
		struct pipe_ctx *pipe_set[MAX_PIPES];

		if (!unsynced_pipes[i])
			continue;

		pipe_set[0] = unsynced_pipes[i];
		unsynced_pipes[i] = NULL;

		/* Add tg to the set, search rest of the tg's for ones with
		 * same timing, add all tgs with same timing to the group
		 */
		for (j = i + 1; j < pipe_count; j++) {
			if (!unsynced_pipes[j])
				continue;

			if (resource_are_streams_timing_synchronizable(
					unsynced_pipes[j]->stream,
					pipe_set[0]->stream)) {
				pipe_set[group_size] = unsynced_pipes[j];
				unsynced_pipes[j] = NULL;
				group_size++;
			}
		}

		/* set first unblanked pipe as master */
		for (j = 0; j < group_size; j++) {
			struct pipe_ctx *temp;

			if (pipe_set[j]->stream_res.tg->funcs->is_blanked && !pipe_set[j]->stream_res.tg->funcs->is_blanked(pipe_set[j]->stream_res.tg)) {
				if (j == 0)
					break;

				temp = pipe_set[0];
				pipe_set[0] = pipe_set[j];
				pipe_set[j] = temp;
				break;
			}
		}

		/* remove any other unblanked pipes as they have already been synced */
		for (j = j + 1; j < group_size; j++) {
			if (pipe_set[j]->stream_res.tg->funcs->is_blanked && !pipe_set[j]->stream_res.tg->funcs->is_blanked(pipe_set[j]->stream_res.tg)) {
				group_size--;
				pipe_set[j] = pipe_set[group_size];
				j--;
			}
		}

		if (group_size > 1) {
			dc->hwss.enable_timing_synchronization(
				dc, group_index, group_size, pipe_set);
			group_index++;
		}
	}
}

static bool context_changed(
		struct dc *dc,
		struct dc_state *context)
{
	uint8_t i;

	if (context->stream_count != dc->current_state->stream_count)
		return true;

	for (i = 0; i < dc->current_state->stream_count; i++) {
		if (dc->current_state->streams[i] != context->streams[i])
			return true;
	}

	return false;
}

bool dc_enable_stereo(
	struct dc *dc,
	struct dc_state *context,
	struct dc_stream_state *streams[],
	uint8_t stream_count)
{
	bool ret = true;
	int i, j;
	struct pipe_ctx *pipe;

	for (i = 0; i < MAX_PIPES; i++) {
		if (context != NULL)
			pipe = &context->res_ctx.pipe_ctx[i];
		else
			pipe = &dc->current_state->res_ctx.pipe_ctx[i];
		for (j = 0 ; pipe && j < stream_count; j++)  {
			if (streams[j] && streams[j] == pipe->stream &&
				dc->hwss.setup_stereo)
				dc->hwss.setup_stereo(pipe, dc);
		}
	}

	return ret;
}

/*
 * Applies given context to HW and copy it into current context.
 * It's up to the user to release the src context afterwards.
 */
static enum dc_status dc_commit_state_no_check(struct dc *dc, struct dc_state *context)
{
	struct dc_bios *dcb = dc->ctx->dc_bios;
	enum dc_status result = DC_ERROR_UNEXPECTED;
	struct pipe_ctx *pipe;
	int i, k, l;
	struct dc_stream_state *dc_streams[MAX_STREAMS] = {0};

	disable_dangling_plane(dc, context);

	for (i = 0; i < context->stream_count; i++)
		dc_streams[i] =  context->streams[i];

	if (!dcb->funcs->is_accelerated_mode(dcb))
		dc->hwss.enable_accelerated_mode(dc, context);

	dc->hwss.set_bandwidth(dc, context, false);

	/* re-program planes for existing stream, in case we need to
	 * free up plane resource for later use
	 */
	for (i = 0; i < context->stream_count; i++) {
		if (context->streams[i]->mode_changed)
			continue;

		dc->hwss.apply_ctx_for_surface(
			dc, context->streams[i],
			context->stream_status[i].plane_count,
			context); /* use new pipe config in new context */
	}

	/* Program hardware */
	dc->hwss.ready_shared_resources(dc, context);

	for (i = 0; i < dc->res_pool->pipe_count; i++) {
		pipe = &context->res_ctx.pipe_ctx[i];
		dc->hwss.wait_for_mpcc_disconnect(dc, dc->res_pool, pipe);
	}

	result = dc->hwss.apply_ctx_to_hw(dc, context);

	if (result != DC_OK)
		return result;

	if (context->stream_count > 1) {
		enable_timing_multisync(dc, context);
		program_timing_sync(dc, context);
	}

	/* Program all planes within new context*/
	for (i = 0; i < context->stream_count; i++) {
		const struct dc_sink *sink = context->streams[i]->sink;

		if (!context->streams[i]->mode_changed)
			continue;

		dc->hwss.apply_ctx_for_surface(
				dc, context->streams[i],
				context->stream_status[i].plane_count,
				context);

		/*
		 * enable stereo
		 * TODO rework dc_enable_stereo call to work with validation sets?
		 */
		for (k = 0; k < MAX_PIPES; k++) {
			pipe = &context->res_ctx.pipe_ctx[k];

			for (l = 0 ; pipe && l < context->stream_count; l++)  {
				if (context->streams[l] &&
					context->streams[l] == pipe->stream &&
					dc->hwss.setup_stereo)
					dc->hwss.setup_stereo(pipe, dc);
			}
		}

		CONN_MSG_MODE(sink->link, "{%dx%d, %dx%d@%dKhz}",
				context->streams[i]->timing.h_addressable,
				context->streams[i]->timing.v_addressable,
				context->streams[i]->timing.h_total,
				context->streams[i]->timing.v_total,
				context->streams[i]->timing.pix_clk_khz);
	}

	dc_enable_stereo(dc, context, dc_streams, context->stream_count);

	/* pplib is notified if disp_num changed */
	dc->hwss.set_bandwidth(dc, context, true);

	for (i = 0; i < context->stream_count; i++)
		context->streams[i]->mode_changed = false;

	dc_release_state(dc->current_state);

	dc->current_state = context;

	dc_retain_state(dc->current_state);

	dc->hwss.optimize_shared_resources(dc);

	return result;
}

bool dc_commit_state(struct dc *dc, struct dc_state *context)
{
	enum dc_status result = DC_ERROR_UNEXPECTED;
	int i;

	if (false == context_changed(dc, context))
		return DC_OK;

	DC_LOG_DC("%s: %d streams\n",
				__func__, context->stream_count);

	for (i = 0; i < context->stream_count; i++) {
		struct dc_stream_state *stream = context->streams[i];

		dc_stream_log(dc, stream);
	}

	result = dc_commit_state_no_check(dc, context);

	return (result == DC_OK);
}

bool dc_post_update_surfaces_to_stream(struct dc *dc)
{
	int i;
	struct dc_state *context = dc->current_state;

	post_surface_trace(dc);

	for (i = 0; i < dc->res_pool->pipe_count; i++)
		if (context->res_ctx.pipe_ctx[i].stream == NULL ||
		    context->res_ctx.pipe_ctx[i].plane_state == NULL) {
			context->res_ctx.pipe_ctx[i].pipe_idx = i;
			dc->hwss.disable_plane(dc, &context->res_ctx.pipe_ctx[i]);
		}

	dc->optimized_required = false;

	dc->hwss.set_bandwidth(dc, context, true);
	return true;
}

struct dc_state *dc_create_state(void)
{
	struct dc_state *context = kzalloc(sizeof(struct dc_state),
					   GFP_KERNEL);

	if (!context)
		return NULL;

	kref_init(&context->refcount);
	return context;
}

void dc_retain_state(struct dc_state *context)
{
	kref_get(&context->refcount);
}

static void dc_state_free(struct kref *kref)
{
	struct dc_state *context = container_of(kref, struct dc_state, refcount);
	dc_resource_state_destruct(context);
	kfree(context);
}

void dc_release_state(struct dc_state *context)
{
	kref_put(&context->refcount, dc_state_free);
}

static bool is_surface_in_context(
		const struct dc_state *context,
		const struct dc_plane_state *plane_state)
{
	int j;

	for (j = 0; j < MAX_PIPES; j++) {
		const struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[j];

		if (plane_state == pipe_ctx->plane_state) {
			return true;
		}
	}

	return false;
}

static unsigned int pixel_format_to_bpp(enum surface_pixel_format format)
{
	switch (format) {
	case SURFACE_PIXEL_FORMAT_VIDEO_420_YCbCr:
	case SURFACE_PIXEL_FORMAT_VIDEO_420_YCrCb:
		return 12;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB1555:
	case SURFACE_PIXEL_FORMAT_GRPH_RGB565:
	case SURFACE_PIXEL_FORMAT_VIDEO_420_10bpc_YCbCr:
	case SURFACE_PIXEL_FORMAT_VIDEO_420_10bpc_YCrCb:
		return 16;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB8888:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR8888:
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB2101010:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010:
		return 32;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616:
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616F:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616F:
		return 64;
	default:
		ASSERT_CRITICAL(false);
		return -1;
	}
}

static enum surface_update_type get_plane_info_update_type(const struct dc_surface_update *u)
{
	union surface_update_flags *update_flags = &u->surface->update_flags;

	if (!u->plane_info)
		return UPDATE_TYPE_FAST;

	if (u->plane_info->color_space != u->surface->color_space)
		update_flags->bits.color_space_change = 1;

	if (u->plane_info->horizontal_mirror != u->surface->horizontal_mirror)
		update_flags->bits.horizontal_mirror_change = 1;

	if (u->plane_info->rotation != u->surface->rotation)
		update_flags->bits.rotation_change = 1;

	if (u->plane_info->format != u->surface->format)
		update_flags->bits.pixel_format_change = 1;

	if (u->plane_info->stereo_format != u->surface->stereo_format)
		update_flags->bits.stereo_format_change = 1;

	if (u->plane_info->per_pixel_alpha != u->surface->per_pixel_alpha)
		update_flags->bits.per_pixel_alpha_change = 1;

	if (u->plane_info->dcc.enable != u->surface->dcc.enable
			|| u->plane_info->dcc.grph.independent_64b_blks != u->surface->dcc.grph.independent_64b_blks
			|| u->plane_info->dcc.grph.meta_pitch != u->surface->dcc.grph.meta_pitch)
		update_flags->bits.dcc_change = 1;

	if (pixel_format_to_bpp(u->plane_info->format) !=
			pixel_format_to_bpp(u->surface->format))
		/* different bytes per element will require full bandwidth
		 * and DML calculation
		 */
		update_flags->bits.bpp_change = 1;

	if (memcmp(&u->plane_info->tiling_info, &u->surface->tiling_info,
			sizeof(union dc_tiling_info)) != 0) {
		update_flags->bits.swizzle_change = 1;
		/* todo: below are HW dependent, we should add a hook to
		 * DCE/N resource and validated there.
		 */
		if (u->plane_info->tiling_info.gfx9.swizzle != DC_SW_LINEAR)
			/* swizzled mode requires RQ to be setup properly,
			 * thus need to run DML to calculate RQ settings
			 */
			update_flags->bits.bandwidth_change = 1;
	}

	if (update_flags->bits.rotation_change
			|| update_flags->bits.stereo_format_change
			|| update_flags->bits.pixel_format_change
			|| update_flags->bits.bpp_change
			|| update_flags->bits.bandwidth_change
			|| update_flags->bits.output_tf_change)
		return UPDATE_TYPE_FULL;

	return UPDATE_TYPE_MED;
}

static enum surface_update_type get_scaling_info_update_type(
		const struct dc_surface_update *u)
{
	union surface_update_flags *update_flags = &u->surface->update_flags;

	if (!u->scaling_info)
		return UPDATE_TYPE_FAST;

	if (u->scaling_info->clip_rect.width != u->surface->clip_rect.width
			|| u->scaling_info->clip_rect.height != u->surface->clip_rect.height
			|| u->scaling_info->dst_rect.width != u->surface->dst_rect.width
			|| u->scaling_info->dst_rect.height != u->surface->dst_rect.height) {
		update_flags->bits.scaling_change = 1;

		if ((u->scaling_info->dst_rect.width < u->surface->dst_rect.width
			|| u->scaling_info->dst_rect.height < u->surface->dst_rect.height)
				&& (u->scaling_info->dst_rect.width < u->surface->src_rect.width
					|| u->scaling_info->dst_rect.height < u->surface->src_rect.height))
			/* Making dst rect smaller requires a bandwidth change */
			update_flags->bits.bandwidth_change = 1;
	}

	if (u->scaling_info->src_rect.width != u->surface->src_rect.width
		|| u->scaling_info->src_rect.height != u->surface->src_rect.height) {

		update_flags->bits.scaling_change = 1;
		if (u->scaling_info->src_rect.width > u->surface->src_rect.width
				&& u->scaling_info->src_rect.height > u->surface->src_rect.height)
			/* Making src rect bigger requires a bandwidth change */
			update_flags->bits.clock_change = 1;
	}

	if (u->scaling_info->src_rect.x != u->surface->src_rect.x
			|| u->scaling_info->src_rect.y != u->surface->src_rect.y
			|| u->scaling_info->clip_rect.x != u->surface->clip_rect.x
			|| u->scaling_info->clip_rect.y != u->surface->clip_rect.y
			|| u->scaling_info->dst_rect.x != u->surface->dst_rect.x
			|| u->scaling_info->dst_rect.y != u->surface->dst_rect.y)
		update_flags->bits.position_change = 1;

	if (update_flags->bits.clock_change
			|| update_flags->bits.bandwidth_change)
		return UPDATE_TYPE_FULL;

	if (update_flags->bits.scaling_change
			|| update_flags->bits.position_change)
		return UPDATE_TYPE_MED;

	return UPDATE_TYPE_FAST;
}

static enum surface_update_type det_surface_update(const struct dc *dc,
		const struct dc_surface_update *u)
{
	const struct dc_state *context = dc->current_state;
	enum surface_update_type type;
	enum surface_update_type overall_type = UPDATE_TYPE_FAST;
	union surface_update_flags *update_flags = &u->surface->update_flags;

	update_flags->raw = 0; // Reset all flags

	if (!is_surface_in_context(context, u->surface)) {
		update_flags->bits.new_plane = 1;
		return UPDATE_TYPE_FULL;
	}

	if (u->surface->force_full_update) {
		update_flags->bits.full_update = 1;
		return UPDATE_TYPE_FULL;
	}

	type = get_plane_info_update_type(u);
	elevate_update_type(&overall_type, type);

	type = get_scaling_info_update_type(u);
	elevate_update_type(&overall_type, type);

	if (u->in_transfer_func)
		update_flags->bits.in_transfer_func_change = 1;

	if (u->input_csc_color_matrix)
		update_flags->bits.input_csc_change = 1;

	if (u->coeff_reduction_factor)
		update_flags->bits.coeff_reduction_change = 1;

	if (u->gamma) {
		enum surface_pixel_format format = SURFACE_PIXEL_FORMAT_GRPH_BEGIN;

		if (u->plane_info)
			format = u->plane_info->format;
		else if (u->surface)
			format = u->surface->format;

		if (dce_use_lut(format))
			update_flags->bits.gamma_change = 1;
	}

	if (update_flags->bits.in_transfer_func_change) {
		type = UPDATE_TYPE_MED;
		elevate_update_type(&overall_type, type);
	}

	if (update_flags->bits.input_csc_change
			|| update_flags->bits.coeff_reduction_change
			|| update_flags->bits.gamma_change) {
		type = UPDATE_TYPE_FULL;
		elevate_update_type(&overall_type, type);
	}

	return overall_type;
}

static enum surface_update_type check_update_surfaces_for_stream(
		struct dc *dc,
		struct dc_surface_update *updates,
		int surface_count,
		struct dc_stream_update *stream_update,
		const struct dc_stream_status *stream_status)
{
	int i;
	enum surface_update_type overall_type = UPDATE_TYPE_FAST;

	if (stream_status == NULL || stream_status->plane_count != surface_count)
		return UPDATE_TYPE_FULL;

	if (stream_update)
		return UPDATE_TYPE_FULL;

	for (i = 0 ; i < surface_count; i++) {
		enum surface_update_type type =
				det_surface_update(dc, &updates[i]);

		if (type == UPDATE_TYPE_FULL)
			return type;

		elevate_update_type(&overall_type, type);
	}

	return overall_type;
}

enum surface_update_type dc_check_update_surfaces_for_stream(
		struct dc *dc,
		struct dc_surface_update *updates,
		int surface_count,
		struct dc_stream_update *stream_update,
		const struct dc_stream_status *stream_status)
{
	int i;
	enum surface_update_type type;

	for (i = 0; i < surface_count; i++)
		updates[i].surface->update_flags.raw = 0;

	type = check_update_surfaces_for_stream(dc, updates, surface_count, stream_update, stream_status);
	if (type == UPDATE_TYPE_FULL)
		for (i = 0; i < surface_count; i++)
			updates[i].surface->update_flags.raw = 0xFFFFFFFF;

	return type;
}

static struct dc_stream_status *stream_get_status(
	struct dc_state *ctx,
	struct dc_stream_state *stream)
{
	uint8_t i;

	for (i = 0; i < ctx->stream_count; i++) {
		if (stream == ctx->streams[i]) {
			return &ctx->stream_status[i];
		}
	}

	return NULL;
}

static const enum surface_update_type update_surface_trace_level = UPDATE_TYPE_FULL;


static void commit_planes_for_stream(struct dc *dc,
		struct dc_surface_update *srf_updates,
		int surface_count,
		struct dc_stream_state *stream,
		struct dc_stream_update *stream_update,
		enum surface_update_type update_type,
		struct dc_state *context)
{
	int i, j;
	struct pipe_ctx *top_pipe_to_program = NULL;

	if (update_type == UPDATE_TYPE_FULL) {
		dc->hwss.set_bandwidth(dc, context, false);
		context_clock_trace(dc, context);
	}

	if (surface_count == 0) {
		/*
		 * In case of turning off screen, no need to program front end a second time.
		 * just return after program front end.
		 */
		dc->hwss.apply_ctx_for_surface(dc, stream, surface_count, context);
		return;
	}

	/* Full fe update*/
	for (j = 0; j < dc->res_pool->pipe_count; j++) {
		struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[j];

		if (!pipe_ctx->top_pipe &&
			pipe_ctx->stream &&
			pipe_ctx->stream == stream) {
			struct dc_stream_status *stream_status = NULL;

			top_pipe_to_program = pipe_ctx;

			if (update_type == UPDATE_TYPE_FAST || !pipe_ctx->plane_state)
				continue;

			stream_status =
					stream_get_status(context, pipe_ctx->stream);

			dc->hwss.apply_ctx_for_surface(
					dc, pipe_ctx->stream, stream_status->plane_count, context);

			if (stream_update && stream_update->abm_level && pipe_ctx->stream_res.abm) {
				if (pipe_ctx->stream_res.tg->funcs->is_blanked) {
					// if otg funcs defined check if blanked before programming
					if (!pipe_ctx->stream_res.tg->funcs->is_blanked(pipe_ctx->stream_res.tg))
						pipe_ctx->stream_res.abm->funcs->set_abm_level(
								pipe_ctx->stream_res.abm, stream->abm_level);
				} else
					pipe_ctx->stream_res.abm->funcs->set_abm_level(
							pipe_ctx->stream_res.abm, stream->abm_level);
			}

			if (stream_update && stream_update->periodic_fn_vsync_delta &&
					pipe_ctx->stream_res.tg->funcs->program_vline_interrupt)
				pipe_ctx->stream_res.tg->funcs->program_vline_interrupt(
						pipe_ctx->stream_res.tg, &pipe_ctx->stream->timing,
						pipe_ctx->stream->periodic_fn_vsync_delta);
		}
	}

	if (update_type == UPDATE_TYPE_FULL)
		context_timing_trace(dc, &context->res_ctx);

	/* Lock the top pipe while updating plane addrs, since freesync requires
	 *  plane addr update event triggers to be synchronized.
	 *  top_pipe_to_program is expected to never be NULL
	 */
	if (update_type == UPDATE_TYPE_FAST) {
		dc->hwss.pipe_control_lock(dc, top_pipe_to_program, true);

		/* Perform requested Updates */
		for (i = 0; i < surface_count; i++) {
			struct dc_plane_state *plane_state = srf_updates[i].surface;

			for (j = 0; j < dc->res_pool->pipe_count; j++) {
				struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[j];

				if (pipe_ctx->stream != stream)
					continue;

				if (pipe_ctx->plane_state != plane_state)
					continue;

				if (srf_updates[i].flip_addr)
					dc->hwss.update_plane_addr(dc, pipe_ctx);
			}
		}

		dc->hwss.pipe_control_lock(dc, top_pipe_to_program, false);
	}

	if (stream && stream_update && update_type > UPDATE_TYPE_FAST)
		for (j = 0; j < dc->res_pool->pipe_count; j++) {
			struct pipe_ctx *pipe_ctx =
					&context->res_ctx.pipe_ctx[j];

			if (pipe_ctx->stream != stream)
				continue;

			if (stream_update->hdr_static_metadata) {
				resource_build_info_frame(pipe_ctx);
				dc->hwss.update_info_frame(pipe_ctx);
			}
		}
}

void dc_commit_updates_for_stream(struct dc *dc,
		struct dc_surface_update *srf_updates,
		int surface_count,
		struct dc_stream_state *stream,
		struct dc_stream_update *stream_update,
		struct dc_plane_state **plane_states,
		struct dc_state *state)
{
	const struct dc_stream_status *stream_status;
	enum surface_update_type update_type;
	struct dc_state *context;
	struct dc_context *dc_ctx = dc->ctx;
	int i, j;

	stream_status = dc_stream_get_status(stream);
	context = dc->current_state;

	update_type = dc_check_update_surfaces_for_stream(
				dc, srf_updates, surface_count, stream_update, stream_status);

	if (update_type >= update_surface_trace_level)
		update_surface_trace(dc, srf_updates, surface_count);


	if (update_type >= UPDATE_TYPE_FULL) {

		/* initialize scratch memory for building context */
		context = dc_create_state();
		if (context == NULL) {
			DC_ERROR("Failed to allocate new validate context!\n");
			return;
		}

		dc_resource_state_copy_construct(state, context);

		for (i = 0; i < dc->res_pool->pipe_count; i++) {
			struct pipe_ctx *new_pipe = &context->res_ctx.pipe_ctx[i];
			struct pipe_ctx *old_pipe = &dc->current_state->res_ctx.pipe_ctx[i];

			if (new_pipe->plane_state && new_pipe->plane_state != old_pipe->plane_state)
				new_pipe->plane_state->force_full_update = true;
		}
	}


	for (i = 0; i < surface_count; i++) {
		struct dc_plane_state *surface = srf_updates[i].surface;

		/* TODO: On flip we don't build the state, so it still has the
		 * old address. Which is why we are updating the address here
		 */
		if (srf_updates[i].flip_addr) {
			surface->address = srf_updates[i].flip_addr->address;
			surface->flip_immediate = srf_updates[i].flip_addr->flip_immediate;

		}

		if (update_type >= UPDATE_TYPE_MED) {
			for (j = 0; j < dc->res_pool->pipe_count; j++) {
				struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[j];

				if (pipe_ctx->plane_state != surface)
					continue;

				resource_build_scaling_params(pipe_ctx);
			}
		}
	}

	commit_planes_for_stream(
				dc,
				srf_updates,
				surface_count,
				stream,
				stream_update,
				update_type,
				context);
	/*update current_State*/
	if (dc->current_state != context) {

		struct dc_state *old = dc->current_state;

		dc->current_state = context;
		dc_release_state(old);

		for (i = 0; i < dc->res_pool->pipe_count; i++) {
			struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[i];

			if (pipe_ctx->plane_state && pipe_ctx->stream == stream)
				pipe_ctx->plane_state->force_full_update = false;
		}
	}
	/*let's use current_state to update watermark etc*/
	if (update_type >= UPDATE_TYPE_FULL)
		dc_post_update_surfaces_to_stream(dc);

	return;

}

uint8_t dc_get_current_stream_count(struct dc *dc)
{
	return dc->current_state->stream_count;
}

struct dc_stream_state *dc_get_stream_at_index(struct dc *dc, uint8_t i)
{
	if (i < dc->current_state->stream_count)
		return dc->current_state->streams[i];
	return NULL;
}

enum dc_irq_source dc_interrupt_to_irq_source(
		struct dc *dc,
		uint32_t src_id,
		uint32_t ext_id)
{
	return dal_irq_service_to_irq_source(dc->res_pool->irqs, src_id, ext_id);
}

bool dc_interrupt_set(struct dc *dc, enum dc_irq_source src, bool enable)
{

	if (dc == NULL)
		return false;

	return dal_irq_service_set(dc->res_pool->irqs, src, enable);
}

void dc_interrupt_ack(struct dc *dc, enum dc_irq_source src)
{
	dal_irq_service_ack(dc->res_pool->irqs, src);
}

void dc_set_power_state(
	struct dc *dc,
	enum dc_acpi_cm_power_state power_state)
{
	struct kref refcount;

	switch (power_state) {
	case DC_ACPI_CM_POWER_STATE_D0:
		dc_resource_state_construct(dc, dc->current_state);

		dc->hwss.init_hw(dc);
		break;
	default:

		dc->hwss.power_down(dc);

		/* Zero out the current context so that on resume we start with
		 * clean state, and dc hw programming optimizations will not
		 * cause any trouble.
		 */

		/* Preserve refcount */
		refcount = dc->current_state->refcount;
		dc_resource_state_destruct(dc->current_state);
		memset(dc->current_state, 0,
				sizeof(*dc->current_state));

		dc->current_state->refcount = refcount;

		break;
	}

}

void dc_resume(struct dc *dc)
{

	uint32_t i;

	for (i = 0; i < dc->link_count; i++)
		core_link_resume(dc->links[i]);
}

bool dc_submit_i2c(
		struct dc *dc,
		uint32_t link_index,
		struct i2c_command *cmd)
{

	struct dc_link *link = dc->links[link_index];
	struct ddc_service *ddc = link->ddc;

	return dal_i2caux_submit_i2c_command(
		ddc->ctx->i2caux,
		ddc->ddc_pin,
		cmd);
}

static bool link_add_remote_sink_helper(struct dc_link *dc_link, struct dc_sink *sink)
{
	if (dc_link->sink_count >= MAX_SINKS_PER_LINK) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	dc_sink_retain(sink);

	dc_link->remote_sinks[dc_link->sink_count] = sink;
	dc_link->sink_count++;

	return true;
}

struct dc_sink *dc_link_add_remote_sink(
		struct dc_link *link,
		const uint8_t *edid,
		int len,
		struct dc_sink_init_data *init_data)
{
	struct dc_sink *dc_sink;
	enum dc_edid_status edid_status;

	if (len > DC_MAX_EDID_BUFFER_SIZE) {
		dm_error("Max EDID buffer size breached!\n");
		return NULL;
	}

	if (!init_data) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	if (!init_data->link) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	dc_sink = dc_sink_create(init_data);

	if (!dc_sink)
		return NULL;

	memmove(dc_sink->dc_edid.raw_edid, edid, len);
	dc_sink->dc_edid.length = len;

	if (!link_add_remote_sink_helper(
			link,
			dc_sink))
		goto fail_add_sink;

	edid_status = dm_helpers_parse_edid_caps(
			link->ctx,
			&dc_sink->dc_edid,
			&dc_sink->edid_caps);

	/*
	 * Treat device as no EDID device if EDID
	 * parsing fails
	 */
	if (edid_status != EDID_OK) {
		dc_sink->dc_edid.length = 0;
		dm_error("Bad EDID, status%d!\n", edid_status);
	}

	return dc_sink;

fail_add_sink:
	dc_sink_release(dc_sink);
	return NULL;
}

void dc_link_remove_remote_sink(struct dc_link *link, struct dc_sink *sink)
{
	int i;

	if (!link->sink_count) {
		BREAK_TO_DEBUGGER();
		return;
	}

	for (i = 0; i < link->sink_count; i++) {
		if (link->remote_sinks[i] == sink) {
			dc_sink_release(sink);
			link->remote_sinks[i] = NULL;

			/* shrink array to remove empty place */
			while (i < link->sink_count - 1) {
				link->remote_sinks[i] = link->remote_sinks[i+1];
				i++;
			}
			link->remote_sinks[i] = NULL;
			link->sink_count--;
			return;
		}
	}
}
