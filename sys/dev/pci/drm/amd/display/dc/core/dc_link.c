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
#include "atom.h"
#include "dm_helpers.h"
#include "dc.h"
#include "grph_object_id.h"
#include "gpio_service_interface.h"
#include "core_status.h"
#include "dc_link_dp.h"
#include "dc_link_ddc.h"
#include "link_hwss.h"
#include "opp.h"

#include "link_encoder.h"
#include "hw_sequencer.h"
#include "resource.h"
#include "abm.h"
#include "fixed31_32.h"
#include "dpcd_defs.h"
#include "dmcu.h"

#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_enum.h"
#include "dce/dce_11_0_sh_mask.h"

#define DC_LOGGER_INIT(logger)


#define LINK_INFO(...) \
	DC_LOG_HW_HOTPLUG(  \
		__VA_ARGS__)

/*******************************************************************************
 * Private structures
 ******************************************************************************/

enum {
	LINK_RATE_REF_FREQ_IN_MHZ = 27,
	PEAK_FACTOR_X1000 = 1006,
	/*
	* Some receivers fail to train on first try and are good
	* on subsequent tries. 2 retries should be plenty. If we
	* don't have a successful training then we don't expect to
	* ever get one.
	*/
	LINK_TRAINING_MAX_VERIFY_RETRY = 2
};

/*******************************************************************************
 * Private functions
 ******************************************************************************/
static void destruct(struct dc_link *link)
{
	int i;

	if (link->ddc)
		dal_ddc_service_destroy(&link->ddc);

	if(link->link_enc)
		link->link_enc->funcs->destroy(&link->link_enc);

	if (link->local_sink)
		dc_sink_release(link->local_sink);

	for (i = 0; i < link->sink_count; ++i)
		dc_sink_release(link->remote_sinks[i]);
}

struct gpio *get_hpd_gpio(struct dc_bios *dcb,
		struct graphics_object_id link_id,
		struct gpio_service *gpio_service)
{
	enum bp_result bp_result;
	struct graphics_object_hpd_info hpd_info;
	struct gpio_pin_info pin_info;

	if (dcb->funcs->get_hpd_info(dcb, link_id, &hpd_info) != BP_RESULT_OK)
		return NULL;

	bp_result = dcb->funcs->get_gpio_pin_info(dcb,
		hpd_info.hpd_int_gpio_uid, &pin_info);

	if (bp_result != BP_RESULT_OK) {
		ASSERT(bp_result == BP_RESULT_NORECORD);
		return NULL;
	}

	return dal_gpio_service_create_irq(
		gpio_service,
		pin_info.offset,
		pin_info.mask);
}

/*
 *  Function: program_hpd_filter
 *
 *  @brief
 *     Programs HPD filter on associated HPD line
 *
 *  @param [in] delay_on_connect_in_ms: Connect filter timeout
 *  @param [in] delay_on_disconnect_in_ms: Disconnect filter timeout
 *
 *  @return
 *     true on success, false otherwise
 */
static bool program_hpd_filter(
	const struct dc_link *link)
{
	bool result = false;

	struct gpio *hpd;

	int delay_on_connect_in_ms = 0;
	int delay_on_disconnect_in_ms = 0;

	if (link->is_hpd_filter_disabled)
		return false;
	/* Verify feature is supported */
	switch (link->connector_signal) {
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
	case SIGNAL_TYPE_HDMI_TYPE_A:
		/* Program hpd filter */
		delay_on_connect_in_ms = 500;
		delay_on_disconnect_in_ms = 100;
		break;
	case SIGNAL_TYPE_DISPLAY_PORT:
	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		/* Program hpd filter to allow DP signal to settle */
		/* 500:	not able to detect MST <-> SST switch as HPD is low for
		 * 	only 100ms on DELL U2413
		 * 0:	some passive dongle still show aux mode instead of i2c
		 * 20-50:not enough to hide bouncing HPD with passive dongle.
		 * 	also see intermittent i2c read issues.
		 */
		delay_on_connect_in_ms = 80;
		delay_on_disconnect_in_ms = 0;
		break;
	case SIGNAL_TYPE_LVDS:
	case SIGNAL_TYPE_EDP:
	default:
		/* Don't program hpd filter */
		return false;
	}

	/* Obtain HPD handle */
	hpd = get_hpd_gpio(link->ctx->dc_bios, link->link_id, link->ctx->gpio_service);

	if (!hpd)
		return result;

	/* Setup HPD filtering */
	if (dal_gpio_open(hpd, GPIO_MODE_INTERRUPT) == GPIO_RESULT_OK) {
		struct gpio_hpd_config config;

		config.delay_on_connect = delay_on_connect_in_ms;
		config.delay_on_disconnect = delay_on_disconnect_in_ms;

		dal_irq_setup_hpd_filter(hpd, &config);

		dal_gpio_close(hpd);

		result = true;
	} else {
		ASSERT_CRITICAL(false);
	}

	/* Release HPD handle */
	dal_gpio_destroy_irq(&hpd);

	return result;
}

bool dc_link_detect_sink(struct dc_link *link, enum dc_connection_type *type)
{
	uint32_t is_hpd_high = 0;
	struct gpio *hpd_pin;

	/* todo: may need to lock gpio access */
	hpd_pin = get_hpd_gpio(link->ctx->dc_bios, link->link_id, link->ctx->gpio_service);
	if (hpd_pin == NULL)
		goto hpd_gpio_failure;

	dal_gpio_open(hpd_pin, GPIO_MODE_INTERRUPT);
	dal_gpio_get_value(hpd_pin, &is_hpd_high);
	dal_gpio_close(hpd_pin);
	dal_gpio_destroy_irq(&hpd_pin);

	if (is_hpd_high) {
		*type = dc_connection_single;
		/* TODO: need to do the actual detection */
	} else {
		*type = dc_connection_none;
	}

	return true;

hpd_gpio_failure:
	return false;
}

static enum ddc_transaction_type get_ddc_transaction_type(
		enum amd_signal_type sink_signal)
{
	enum ddc_transaction_type transaction_type = DDC_TRANSACTION_TYPE_NONE;

	switch (sink_signal) {
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
	case SIGNAL_TYPE_HDMI_TYPE_A:
	case SIGNAL_TYPE_LVDS:
	case SIGNAL_TYPE_RGB:
		transaction_type = DDC_TRANSACTION_TYPE_I2C;
		break;

	case SIGNAL_TYPE_DISPLAY_PORT:
	case SIGNAL_TYPE_EDP:
		transaction_type = DDC_TRANSACTION_TYPE_I2C_OVER_AUX;
		break;

	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		/* MST does not use I2COverAux, but there is the
		 * SPECIAL use case for "immediate dwnstrm device
		 * access" (EPR#370830). */
		transaction_type = DDC_TRANSACTION_TYPE_I2C_OVER_AUX;
		break;

	default:
		break;
	}

	return transaction_type;
}

static enum amd_signal_type get_basic_signal_type(
	struct graphics_object_id encoder,
	struct graphics_object_id downstream)
{
	if (downstream.type == OBJECT_TYPE_CONNECTOR) {
		switch (downstream.id) {
		case CONNECTOR_ID_SINGLE_LINK_DVII:
			switch (encoder.id) {
			case ENCODER_ID_INTERNAL_DAC1:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC1:
			case ENCODER_ID_INTERNAL_DAC2:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC2:
				return SIGNAL_TYPE_RGB;
			default:
				return SIGNAL_TYPE_DVI_SINGLE_LINK;
			}
		break;
		case CONNECTOR_ID_DUAL_LINK_DVII:
		{
			switch (encoder.id) {
			case ENCODER_ID_INTERNAL_DAC1:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC1:
			case ENCODER_ID_INTERNAL_DAC2:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC2:
				return SIGNAL_TYPE_RGB;
			default:
				return SIGNAL_TYPE_DVI_DUAL_LINK;
			}
		}
		break;
		case CONNECTOR_ID_SINGLE_LINK_DVID:
			return SIGNAL_TYPE_DVI_SINGLE_LINK;
		case CONNECTOR_ID_DUAL_LINK_DVID:
			return SIGNAL_TYPE_DVI_DUAL_LINK;
		case CONNECTOR_ID_VGA:
			return SIGNAL_TYPE_RGB;
		case CONNECTOR_ID_HDMI_TYPE_A:
			return SIGNAL_TYPE_HDMI_TYPE_A;
		case CONNECTOR_ID_LVDS:
			return SIGNAL_TYPE_LVDS;
		case CONNECTOR_ID_DISPLAY_PORT:
			return SIGNAL_TYPE_DISPLAY_PORT;
		case CONNECTOR_ID_EDP:
			return SIGNAL_TYPE_EDP;
		default:
			return SIGNAL_TYPE_NONE;
		}
	} else if (downstream.type == OBJECT_TYPE_ENCODER) {
		switch (downstream.id) {
		case ENCODER_ID_EXTERNAL_NUTMEG:
		case ENCODER_ID_EXTERNAL_TRAVIS:
			return SIGNAL_TYPE_DISPLAY_PORT;
		default:
			return SIGNAL_TYPE_NONE;
		}
	}

	return SIGNAL_TYPE_NONE;
}

/*
 * @brief
 * Check whether there is a dongle on DP connector
 */
bool dc_link_is_dp_sink_present(struct dc_link *link)
{
	enum gpio_result gpio_result;
	uint32_t clock_pin = 0;
	uint8_t retry = 0;
	struct ddc *ddc;

	enum connector_id connector_id =
		dal_graphics_object_id_get_connector_id(link->link_id);

	bool present =
		((connector_id == CONNECTOR_ID_DISPLAY_PORT) ||
		(connector_id == CONNECTOR_ID_EDP));

	ddc = dal_ddc_service_get_ddc_pin(link->ddc);

	if (!ddc) {
		BREAK_TO_DEBUGGER();
		return present;
	}

	/* Open GPIO and set it to I2C mode */
	/* Note: this GpioMode_Input will be converted
	 * to GpioConfigType_I2cAuxDualMode in GPIO component,
	 * which indicates we need additional delay */

	if (GPIO_RESULT_OK != dal_ddc_open(
		ddc, GPIO_MODE_INPUT, GPIO_DDC_CONFIG_TYPE_MODE_I2C)) {
		dal_gpio_destroy_ddc(&ddc);

		return present;
	}

	/*
	 * Read GPIO: DP sink is present if both clock and data pins are zero
	 *
	 * [W/A] plug-unplug DP cable, sometimes customer board has
	 * one short pulse on clk_pin(1V, < 1ms). DP will be config to HDMI/DVI
	 * then monitor can't br light up. Add retry 3 times
	 * But in real passive dongle, it need additional 3ms to detect
	 */
	do {
		gpio_result = dal_gpio_get_value(ddc->pin_clock, &clock_pin);
		ASSERT(gpio_result == GPIO_RESULT_OK);
		if (clock_pin)
			udelay(1000);
		else
			break;
	} while (retry++ < 3);

	present = (gpio_result == GPIO_RESULT_OK) && !clock_pin;

	dal_ddc_close(ddc);

	return present;
}

/*
 * @brief
 * Detect output sink type
 */
static enum amd_signal_type link_detect_sink(
	struct dc_link *link,
	enum dc_detect_reason reason)
{
	enum amd_signal_type result = get_basic_signal_type(
		link->link_enc->id, link->link_id);

	/* Internal digital encoder will detect only dongles
	 * that require digital signal */

	/* Detection mechanism is different
	 * for different native connectors.
	 * LVDS connector supports only LVDS signal;
	 * PCIE is a bus slot, the actual connector needs to be detected first;
	 * eDP connector supports only eDP signal;
	 * HDMI should check straps for audio */

	/* PCIE detects the actual connector on add-on board */

	if (link->link_id.id == CONNECTOR_ID_PCIE) {
		/* ZAZTODO implement PCIE add-on card detection */
	}

	switch (link->link_id.id) {
	case CONNECTOR_ID_HDMI_TYPE_A: {
		/* check audio support:
		 * if native HDMI is not supported, switch to DVI */
		struct audio_support *aud_support = &link->dc->res_pool->audio_support;

		if (!aud_support->hdmi_audio_native)
			if (link->link_id.id == CONNECTOR_ID_HDMI_TYPE_A)
				result = SIGNAL_TYPE_DVI_SINGLE_LINK;
	}
	break;
	case CONNECTOR_ID_DISPLAY_PORT: {
		/* DP HPD short pulse. Passive DP dongle will not
		 * have short pulse
		 */
		if (reason != DETECT_REASON_HPDRX) {
			/* Check whether DP signal detected: if not -
			 * we assume signal is DVI; it could be corrected
			 * to HDMI after dongle detection
			 */
			if (!dm_helpers_is_dp_sink_present(link))
				result = SIGNAL_TYPE_DVI_SINGLE_LINK;
		}
	}
	break;
	default:
	break;
	}

	return result;
}

static enum amd_signal_type decide_signal_from_strap_and_dongle_type(
		enum display_dongle_type dongle_type,
		struct audio_support *audio_support)
{
	enum amd_signal_type signal = SIGNAL_TYPE_NONE;

	switch (dongle_type) {
	case DISPLAY_DONGLE_DP_HDMI_DONGLE:
		if (audio_support->hdmi_audio_on_dongle)
			signal =  SIGNAL_TYPE_HDMI_TYPE_A;
		else
			signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
		break;
	case DISPLAY_DONGLE_DP_DVI_DONGLE:
		signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
		break;
	case DISPLAY_DONGLE_DP_HDMI_MISMATCHED_DONGLE:
		if (audio_support->hdmi_audio_native)
			signal =  SIGNAL_TYPE_HDMI_TYPE_A;
		else
			signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
		break;
	default:
		signal = SIGNAL_TYPE_NONE;
		break;
	}

	return signal;
}

static enum amd_signal_type dp_passive_dongle_detection(
		struct ddc_service *ddc,
		struct display_sink_capability *sink_cap,
		struct audio_support *audio_support)
{
	dal_ddc_service_i2c_query_dp_dual_mode_adaptor(
						ddc, sink_cap);
	return decide_signal_from_strap_and_dongle_type(
			sink_cap->dongle_type,
			audio_support);
}

static void link_disconnect_sink(struct dc_link *link)
{
	if (link->local_sink) {
		dc_sink_release(link->local_sink);
		link->local_sink = NULL;
	}

	link->dpcd_sink_count = 0;
}

static void link_disconnect_remap(struct dc_sink *prev_sink, struct dc_link *link)
{
	dc_sink_release(link->local_sink);
	link->local_sink = prev_sink;
}


static bool detect_dp(
	struct dc_link *link,
	struct display_sink_capability *sink_caps,
	bool *converter_disable_audio,
	struct audio_support *audio_support,
	enum dc_detect_reason reason)
{
	bool boot = false;
	sink_caps->signal = link_detect_sink(link, reason);
	sink_caps->transaction_type =
		get_ddc_transaction_type(sink_caps->signal);

	if (sink_caps->transaction_type == DDC_TRANSACTION_TYPE_I2C_OVER_AUX) {
		sink_caps->signal = SIGNAL_TYPE_DISPLAY_PORT;
		if (!detect_dp_sink_caps(link))
			return false;

		if (is_mst_supported(link)) {
			sink_caps->signal = SIGNAL_TYPE_DISPLAY_PORT_MST;
			link->type = dc_connection_mst_branch;

			dal_ddc_service_set_transaction_type(
							link->ddc,
							sink_caps->transaction_type);

			/*
			 * This call will initiate MST topology discovery. Which
			 * will detect MST ports and add new DRM connector DRM
			 * framework. Then read EDID via remote i2c over aux. In
			 * the end, will notify DRM detect result and save EDID
			 * into DRM framework.
			 *
			 * .detect is called by .fill_modes.
			 * .fill_modes is called by user mode ioctl
			 * DRM_IOCTL_MODE_GETCONNECTOR.
			 *
			 * .get_modes is called by .fill_modes.
			 *
			 * call .get_modes, AMDGPU DM implementation will create
			 * new dc_sink and add to dc_link. For long HPD plug
			 * in/out, MST has its own handle.
			 *
			 * Therefore, just after dc_create, link->sink is not
			 * created for MST until user mode app calls
			 * DRM_IOCTL_MODE_GETCONNECTOR.
			 *
			 * Need check ->sink usages in case ->sink = NULL
			 * TODO: s3 resume check
			 */
			if (reason == DETECT_REASON_BOOT)
				boot = true;

			dm_helpers_dp_update_branch_info(
				link->ctx,
				link);

			if (!dm_helpers_dp_mst_start_top_mgr(
				link->ctx,
				link, boot)) {
				/* MST not supported */
				link->type = dc_connection_single;
				sink_caps->signal = SIGNAL_TYPE_DISPLAY_PORT;
			}
		}

		if (link->type != dc_connection_mst_branch &&
			is_dp_active_dongle(link)) {
			/* DP active dongles */
			link->type = dc_connection_active_dongle;
			if (!link->dpcd_caps.sink_count.bits.SINK_COUNT) {
				/*
				 * active dongle unplug processing for short irq
				 */
				link_disconnect_sink(link);
				return true;
			}

			if (link->dpcd_caps.dongle_type != DISPLAY_DONGLE_DP_HDMI_CONVERTER)
				*converter_disable_audio = true;
		}
	} else {
		/* DP passive dongles */
		sink_caps->signal = dp_passive_dongle_detection(link->ddc,
				sink_caps,
				audio_support);
	}

	return true;
}

static bool is_same_edid(struct dc_edid *old_edid, struct dc_edid *new_edid)
{
	if (old_edid->length != new_edid->length)
		return false;

	if (new_edid->length == 0)
		return false;

	return (memcmp(old_edid->raw_edid, new_edid->raw_edid, new_edid->length) == 0);
}

bool dc_link_detect(struct dc_link *link, enum dc_detect_reason reason)
{
	struct dc_sink_init_data sink_init_data = { 0 };
	struct display_sink_capability sink_caps = { 0 };
	uint8_t i;
	bool converter_disable_audio = false;
	struct audio_support *aud_support = &link->dc->res_pool->audio_support;
	bool same_edid = false;
	enum dc_edid_status edid_status;
	struct dc_context *dc_ctx = link->ctx;
	struct dc_sink *sink = NULL;
	struct dc_sink *prev_sink = NULL;
	struct dpcd_caps prev_dpcd_caps;
	bool same_dpcd = true;
	enum dc_connection_type new_connection_type = dc_connection_none;
	DC_LOGGER_INIT(link->ctx->logger);
	if (link->connector_signal == SIGNAL_TYPE_VIRTUAL)
		return false;

	if (false == dc_link_detect_sink(link, &new_connection_type)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (link->connector_signal == SIGNAL_TYPE_EDP &&
			link->local_sink)
		return true;

	prev_sink = link->local_sink;
	if (prev_sink != NULL) {
		dc_sink_retain(prev_sink);
		memcpy(&prev_dpcd_caps, &link->dpcd_caps, sizeof(struct dpcd_caps));
	}
	link_disconnect_sink(link);

	if (new_connection_type != dc_connection_none) {
		link->type = new_connection_type;

		/* From Disconnected-to-Connected. */
		switch (link->connector_signal) {
		case SIGNAL_TYPE_HDMI_TYPE_A: {
			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C;
			if (aud_support->hdmi_audio_native)
				sink_caps.signal = SIGNAL_TYPE_HDMI_TYPE_A;
			else
				sink_caps.signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
			break;
		}

		case SIGNAL_TYPE_DVI_SINGLE_LINK: {
			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C;
			sink_caps.signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
			break;
		}

		case SIGNAL_TYPE_DVI_DUAL_LINK: {
			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C;
			sink_caps.signal = SIGNAL_TYPE_DVI_DUAL_LINK;
			break;
		}

		case SIGNAL_TYPE_EDP: {
			detect_edp_sink_caps(link);
			sink_caps.transaction_type =
				DDC_TRANSACTION_TYPE_I2C_OVER_AUX;
			sink_caps.signal = SIGNAL_TYPE_EDP;
			break;
		}

		case SIGNAL_TYPE_DISPLAY_PORT: {
			if (!detect_dp(
				link,
				&sink_caps,
				&converter_disable_audio,
				aud_support, reason)) {
				if (prev_sink != NULL)
					dc_sink_release(prev_sink);
				return false;
			}

			// Check if dpcp block is the same
			if (prev_sink != NULL) {
				if (memcmp(&link->dpcd_caps, &prev_dpcd_caps, sizeof(struct dpcd_caps)))
					same_dpcd = false;
			}
			/* Active dongle downstream unplug */
			if (link->type == dc_connection_active_dongle
					&& link->dpcd_caps.sink_count.
					bits.SINK_COUNT == 0) {
				if (prev_sink != NULL)
					dc_sink_release(prev_sink);
				return true;
			}

			if (link->type == dc_connection_mst_branch) {
				LINK_INFO("link=%d, mst branch is now Connected\n",
					link->link_index);
				/* Need to setup mst link_cap struct here
				 * otherwise dc_link_detect() will leave mst link_cap
				 * empty which leads to allocate_mst_payload() has "0"
				 * pbn_per_slot value leading to exception on dc_fixpt_div()
				 */
				link->verified_link_cap = link->reported_link_cap;
				if (prev_sink != NULL)
					dc_sink_release(prev_sink);
				return false;
			}

			break;
		}

		default:
			DC_ERROR("Invalid connector type! signal:%d\n",
				link->connector_signal);
			if (prev_sink != NULL)
				dc_sink_release(prev_sink);
			return false;
		} /* switch() */

		if (link->dpcd_caps.sink_count.bits.SINK_COUNT)
			link->dpcd_sink_count = link->dpcd_caps.sink_count.
					bits.SINK_COUNT;
		else
			link->dpcd_sink_count = 1;

		dal_ddc_service_set_transaction_type(
						link->ddc,
						sink_caps.transaction_type);

		link->aux_mode = dal_ddc_service_is_in_aux_transaction_mode(
				link->ddc);

		sink_init_data.link = link;
		sink_init_data.sink_signal = sink_caps.signal;

		sink = dc_sink_create(&sink_init_data);
		if (!sink) {
			DC_ERROR("Failed to create sink!\n");
			if (prev_sink != NULL)
				dc_sink_release(prev_sink);
			return false;
		}

		sink->dongle_max_pix_clk = sink_caps.max_hdmi_pixel_clock;
		sink->converter_disable_audio = converter_disable_audio;

		link->local_sink = sink;

		edid_status = dm_helpers_read_local_edid(
				link->ctx,
				link,
				sink);

		switch (edid_status) {
		case EDID_BAD_CHECKSUM:
			DC_LOG_ERROR("EDID checksum invalid.\n");
			break;
		case EDID_NO_RESPONSE:
			DC_LOG_ERROR("No EDID read.\n");

			/*
			 * Abort detection for non-DP connectors if we have
			 * no EDID
			 *
			 * DP needs to report as connected if HDP is high
			 * even if we have no EDID in order to go to
			 * fail-safe mode
			 */
			if (dc_is_hdmi_signal(link->connector_signal) ||
			    dc_is_dvi_signal(link->connector_signal)) {
				if (prev_sink != NULL)
					dc_sink_release(prev_sink);

				return false;
			}
		default:
			break;
		}

		// Check if edid is the same
		if ((prev_sink != NULL) && ((edid_status == EDID_THE_SAME) || (edid_status == EDID_OK)))
			same_edid = is_same_edid(&prev_sink->dc_edid, &sink->dc_edid);

		if (link->connector_signal == SIGNAL_TYPE_DISPLAY_PORT &&
			sink_caps.transaction_type == DDC_TRANSACTION_TYPE_I2C_OVER_AUX &&
			reason != DETECT_REASON_HPDRX) {
			/*
			 * TODO debug why Dell 2413 doesn't like
			 *  two link trainings
			 */

			/* deal with non-mst cases */
			for (i = 0; i < LINK_TRAINING_MAX_VERIFY_RETRY; i++) {
				int fail_count = 0;

				dp_verify_link_cap(link,
						  &link->reported_link_cap,
						  &fail_count);

				if (fail_count == 0)
					break;
			}

		} else {
			// If edid is the same, then discard new sink and revert back to original sink
			if (same_edid) {
				link_disconnect_remap(prev_sink, link);
				sink = prev_sink;
				prev_sink = NULL;

			}
		}

		/* HDMI-DVI Dongle */
		if (sink->sink_signal == SIGNAL_TYPE_HDMI_TYPE_A &&
				!sink->edid_caps.edid_hdmi)
			sink->sink_signal = SIGNAL_TYPE_DVI_SINGLE_LINK;

		/* Connectivity log: detection */
		for (i = 0; i < sink->dc_edid.length / EDID_BLOCK_SIZE; i++) {
			CONN_DATA_DETECT(link,
					&sink->dc_edid.raw_edid[i * EDID_BLOCK_SIZE],
					EDID_BLOCK_SIZE,
					"%s: [Block %d] ", sink->edid_caps.display_name, i);
		}

		DC_LOG_DETECTION_EDID_PARSER("%s: "
			"manufacturer_id = %X, "
			"product_id = %X, "
			"serial_number = %X, "
			"manufacture_week = %d, "
			"manufacture_year = %d, "
			"display_name = %s, "
			"speaker_flag = %d, "
			"audio_mode_count = %d\n",
			__func__,
			sink->edid_caps.manufacturer_id,
			sink->edid_caps.product_id,
			sink->edid_caps.serial_number,
			sink->edid_caps.manufacture_week,
			sink->edid_caps.manufacture_year,
			sink->edid_caps.display_name,
			sink->edid_caps.speaker_flags,
			sink->edid_caps.audio_mode_count);

		for (i = 0; i < sink->edid_caps.audio_mode_count; i++) {
			DC_LOG_DETECTION_EDID_PARSER("%s: mode number = %d, "
				"format_code = %d, "
				"channel_count = %d, "
				"sample_rate = %d, "
				"sample_size = %d\n",
				__func__,
				i,
				sink->edid_caps.audio_modes[i].format_code,
				sink->edid_caps.audio_modes[i].channel_count,
				sink->edid_caps.audio_modes[i].sample_rate,
				sink->edid_caps.audio_modes[i].sample_size);
		}

	} else {
		/* From Connected-to-Disconnected. */
		if (link->type == dc_connection_mst_branch) {
			LINK_INFO("link=%d, mst branch is now Disconnected\n",
				link->link_index);

			dm_helpers_dp_mst_stop_top_mgr(link->ctx, link);

			link->mst_stream_alloc_table.stream_count = 0;
			memset(link->mst_stream_alloc_table.stream_allocations, 0, sizeof(link->mst_stream_alloc_table.stream_allocations));
		}

		link->type = dc_connection_none;
		sink_caps.signal = SIGNAL_TYPE_NONE;
	}

	LINK_INFO("link=%d, dc_sink_in=%p is now %s prev_sink=%p dpcd same=%d edid same=%d\n",
		link->link_index, sink,
		(sink_caps.signal == SIGNAL_TYPE_NONE ?
			"Disconnected":"Connected"), prev_sink,
			same_dpcd, same_edid);

	if (prev_sink != NULL)
		dc_sink_release(prev_sink);

	return true;
}

static enum hpd_source_id get_hpd_line(
		struct dc_link *link)
{
	struct gpio *hpd;
	enum hpd_source_id hpd_id = HPD_SOURCEID_UNKNOWN;

	hpd = get_hpd_gpio(link->ctx->dc_bios, link->link_id, link->ctx->gpio_service);

	if (hpd) {
		switch (dal_irq_get_source(hpd)) {
		case DC_IRQ_SOURCE_HPD1:
			hpd_id = HPD_SOURCEID1;
		break;
		case DC_IRQ_SOURCE_HPD2:
			hpd_id = HPD_SOURCEID2;
		break;
		case DC_IRQ_SOURCE_HPD3:
			hpd_id = HPD_SOURCEID3;
		break;
		case DC_IRQ_SOURCE_HPD4:
			hpd_id = HPD_SOURCEID4;
		break;
		case DC_IRQ_SOURCE_HPD5:
			hpd_id = HPD_SOURCEID5;
		break;
		case DC_IRQ_SOURCE_HPD6:
			hpd_id = HPD_SOURCEID6;
		break;
		default:
			BREAK_TO_DEBUGGER();
		break;
		}

		dal_gpio_destroy_irq(&hpd);
	}

	return hpd_id;
}

static enum channel_id get_ddc_line(struct dc_link *link)
{
	struct ddc *ddc;
	enum channel_id channel = CHANNEL_ID_UNKNOWN;

	ddc = dal_ddc_service_get_ddc_pin(link->ddc);

	if (ddc) {
		switch (dal_ddc_get_line(ddc)) {
		case GPIO_DDC_LINE_DDC1:
			channel = CHANNEL_ID_DDC1;
			break;
		case GPIO_DDC_LINE_DDC2:
			channel = CHANNEL_ID_DDC2;
			break;
		case GPIO_DDC_LINE_DDC3:
			channel = CHANNEL_ID_DDC3;
			break;
		case GPIO_DDC_LINE_DDC4:
			channel = CHANNEL_ID_DDC4;
			break;
		case GPIO_DDC_LINE_DDC5:
			channel = CHANNEL_ID_DDC5;
			break;
		case GPIO_DDC_LINE_DDC6:
			channel = CHANNEL_ID_DDC6;
			break;
		case GPIO_DDC_LINE_DDC_VGA:
			channel = CHANNEL_ID_DDC_VGA;
			break;
		case GPIO_DDC_LINE_I2C_PAD:
			channel = CHANNEL_ID_I2C_PAD;
			break;
		default:
			BREAK_TO_DEBUGGER();
			break;
		}
	}

	return channel;
}

static enum transmitter translate_encoder_to_transmitter(
	struct graphics_object_id encoder)
{
	switch (encoder.id) {
	case ENCODER_ID_INTERNAL_UNIPHY:
		switch (encoder.enum_id) {
		case ENUM_ID_1:
			return TRANSMITTER_UNIPHY_A;
		case ENUM_ID_2:
			return TRANSMITTER_UNIPHY_B;
		default:
			return TRANSMITTER_UNKNOWN;
		}
	break;
	case ENCODER_ID_INTERNAL_UNIPHY1:
		switch (encoder.enum_id) {
		case ENUM_ID_1:
			return TRANSMITTER_UNIPHY_C;
		case ENUM_ID_2:
			return TRANSMITTER_UNIPHY_D;
		default:
			return TRANSMITTER_UNKNOWN;
		}
	break;
	case ENCODER_ID_INTERNAL_UNIPHY2:
		switch (encoder.enum_id) {
		case ENUM_ID_1:
			return TRANSMITTER_UNIPHY_E;
		case ENUM_ID_2:
			return TRANSMITTER_UNIPHY_F;
		default:
			return TRANSMITTER_UNKNOWN;
		}
	break;
	case ENCODER_ID_INTERNAL_UNIPHY3:
		switch (encoder.enum_id) {
		case ENUM_ID_1:
			return TRANSMITTER_UNIPHY_G;
		default:
			return TRANSMITTER_UNKNOWN;
		}
	break;
	case ENCODER_ID_EXTERNAL_NUTMEG:
		switch (encoder.enum_id) {
		case ENUM_ID_1:
			return TRANSMITTER_NUTMEG_CRT;
		default:
			return TRANSMITTER_UNKNOWN;
		}
	break;
	case ENCODER_ID_EXTERNAL_TRAVIS:
		switch (encoder.enum_id) {
		case ENUM_ID_1:
			return TRANSMITTER_TRAVIS_CRT;
		case ENUM_ID_2:
			return TRANSMITTER_TRAVIS_LCD;
		default:
			return TRANSMITTER_UNKNOWN;
		}
	break;
	default:
		return TRANSMITTER_UNKNOWN;
	}
}

static bool construct(
	struct dc_link *link,
	const struct link_init_data *init_params)
{
	uint8_t i;
	struct gpio *hpd_gpio = NULL;
	struct ddc_service_init_data ddc_service_init_data = { { 0 } };
	struct dc_context *dc_ctx = init_params->ctx;
	struct encoder_init_data enc_init_data = { 0 };
	struct integrated_info info = {{{ 0 }}};
	struct dc_bios *bios = init_params->dc->ctx->dc_bios;
	const struct dc_vbios_funcs *bp_funcs = bios->funcs;
	DC_LOGGER_INIT(dc_ctx->logger);

	link->irq_source_hpd = DC_IRQ_SOURCE_INVALID;
	link->irq_source_hpd_rx = DC_IRQ_SOURCE_INVALID;

	link->link_status.dpcd_caps = &link->dpcd_caps;

	link->dc = init_params->dc;
	link->ctx = dc_ctx;
	link->link_index = init_params->link_index;

	link->link_id = bios->funcs->get_connector_id(bios, init_params->connector_index);

	if (link->link_id.type != OBJECT_TYPE_CONNECTOR) {
		dm_error("%s: Invalid Connector ObjectID from Adapter Service for connector index:%d! type %d expected %d\n",
			 __func__, init_params->connector_index,
			 link->link_id.type, OBJECT_TYPE_CONNECTOR);
		goto create_fail;
	}

	if (link->dc->res_pool->funcs->link_init)
		link->dc->res_pool->funcs->link_init(link);

	hpd_gpio = get_hpd_gpio(link->ctx->dc_bios, link->link_id, link->ctx->gpio_service);

	if (hpd_gpio != NULL)
		link->irq_source_hpd = dal_irq_get_source(hpd_gpio);

	switch (link->link_id.id) {
	case CONNECTOR_ID_HDMI_TYPE_A:
		link->connector_signal = SIGNAL_TYPE_HDMI_TYPE_A;

		break;
	case CONNECTOR_ID_SINGLE_LINK_DVID:
	case CONNECTOR_ID_SINGLE_LINK_DVII:
		link->connector_signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
		break;
	case CONNECTOR_ID_DUAL_LINK_DVID:
	case CONNECTOR_ID_DUAL_LINK_DVII:
		link->connector_signal = SIGNAL_TYPE_DVI_DUAL_LINK;
		break;
	case CONNECTOR_ID_DISPLAY_PORT:
		link->connector_signal =	SIGNAL_TYPE_DISPLAY_PORT;

		if (hpd_gpio != NULL)
			link->irq_source_hpd_rx =
					dal_irq_get_rx_source(hpd_gpio);

		break;
	case CONNECTOR_ID_EDP:
		link->connector_signal = SIGNAL_TYPE_EDP;

		if (hpd_gpio != NULL) {
			link->irq_source_hpd = DC_IRQ_SOURCE_INVALID;
			link->irq_source_hpd_rx =
					dal_irq_get_rx_source(hpd_gpio);
		}
		break;
	default:
		DC_LOG_WARNING("Unsupported Connector type:%d!\n", link->link_id.id);
		goto create_fail;
	}

	if (hpd_gpio != NULL) {
		dal_gpio_destroy_irq(&hpd_gpio);
		hpd_gpio = NULL;
	}

	/* TODO: #DAL3 Implement id to str function.*/
	LINK_INFO("Connector[%d] description:"
			"signal %d\n",
			init_params->connector_index,
			link->connector_signal);

	ddc_service_init_data.ctx = link->ctx;
	ddc_service_init_data.id = link->link_id;
	ddc_service_init_data.link = link;
	link->ddc = dal_ddc_service_create(&ddc_service_init_data);

	if (link->ddc == NULL) {
		DC_ERROR("Failed to create ddc_service!\n");
		goto ddc_create_fail;
	}

	link->ddc_hw_inst =
		dal_ddc_get_line(
			dal_ddc_service_get_ddc_pin(link->ddc));

	enc_init_data.ctx = dc_ctx;
	bp_funcs->get_src_obj(dc_ctx->dc_bios, link->link_id, 0, &enc_init_data.encoder);
	enc_init_data.connector = link->link_id;
	enc_init_data.channel = get_ddc_line(link);
	enc_init_data.hpd_source = get_hpd_line(link);

	link->hpd_src = enc_init_data.hpd_source;

	enc_init_data.transmitter =
			translate_encoder_to_transmitter(enc_init_data.encoder);
	link->link_enc = link->dc->res_pool->funcs->link_enc_create(
								&enc_init_data);

	if( link->link_enc == NULL) {
		DC_ERROR("Failed to create link encoder!\n");
		goto link_enc_create_fail;
	}

	link->link_enc_hw_inst = link->link_enc->transmitter;

	for (i = 0; i < 4; i++) {
		if (BP_RESULT_OK !=
				bp_funcs->get_device_tag(dc_ctx->dc_bios, link->link_id, i, &link->device_tag)) {
			DC_ERROR("Failed to find device tag!\n");
			goto device_tag_fail;
		}

		/* Look for device tag that matches connector signal,
		 * CRT for rgb, LCD for other supported signal tyes
		 */
		if (!bp_funcs->is_device_id_supported(dc_ctx->dc_bios, link->device_tag.dev_id))
			continue;
		if (link->device_tag.dev_id.device_type == DEVICE_TYPE_CRT
			&& link->connector_signal != SIGNAL_TYPE_RGB)
			continue;
		if (link->device_tag.dev_id.device_type == DEVICE_TYPE_LCD
			&& link->connector_signal == SIGNAL_TYPE_RGB)
			continue;
		break;
	}

	if (bios->integrated_info)
		info = *bios->integrated_info;

	/* Look for channel mapping corresponding to connector and device tag */
	for (i = 0; i < MAX_NUMBER_OF_EXT_DISPLAY_PATH; i++) {
		struct external_display_path *path =
			&info.ext_disp_conn_info.path[i];
		if (path->device_connector_id.enum_id == link->link_id.enum_id
			&& path->device_connector_id.id == link->link_id.id
			&& path->device_connector_id.type == link->link_id.type) {

			if (link->device_tag.acpi_device != 0
				&& path->device_acpi_enum == link->device_tag.acpi_device) {
				link->ddi_channel_mapping = path->channel_mapping;
				link->chip_caps = path->caps;
			} else if (path->device_tag ==
					link->device_tag.dev_id.raw_device_tag) {
				link->ddi_channel_mapping = path->channel_mapping;
				link->chip_caps = path->caps;
			}
			break;
		}
	}

	/*
	 * TODO check if GPIO programmed correctly
	 *
	 * If GPIO isn't programmed correctly HPD might not rise or drain
	 * fast enough, leading to bounces.
	 */
	program_hpd_filter(link);

	return true;
device_tag_fail:
	link->link_enc->funcs->destroy(&link->link_enc);
link_enc_create_fail:
	dal_ddc_service_destroy(&link->ddc);
ddc_create_fail:
create_fail:

	if (hpd_gpio != NULL) {
		dal_gpio_destroy_irq(&hpd_gpio);
	}

	return false;
}

/*******************************************************************************
 * Public functions
 ******************************************************************************/
struct dc_link *link_create(const struct link_init_data *init_params)
{
	struct dc_link *link =
			kzalloc(sizeof(*link), GFP_KERNEL);

	if (NULL == link)
		goto alloc_fail;

	if (false == construct(link, init_params))
		goto construct_fail;

	return link;

construct_fail:
	kfree(link);

alloc_fail:
	return NULL;
}

void link_destroy(struct dc_link **link)
{
	destruct(*link);
	kfree(*link);
	*link = NULL;
}

static void dpcd_configure_panel_mode(
	struct dc_link *link,
	enum dp_panel_mode panel_mode)
{
	union dpcd_edp_config edp_config_set;
	bool panel_mode_edp = false;
	DC_LOGGER_INIT(link->ctx->logger);

	memset(&edp_config_set, '\0', sizeof(union dpcd_edp_config));

	if (DP_PANEL_MODE_DEFAULT != panel_mode) {

		switch (panel_mode) {
		case DP_PANEL_MODE_EDP:
		case DP_PANEL_MODE_SPECIAL:
			panel_mode_edp = true;
			break;

		default:
			break;
		}

		/*set edp panel mode in receiver*/
		core_link_read_dpcd(
			link,
			DP_EDP_CONFIGURATION_SET,
			&edp_config_set.raw,
			sizeof(edp_config_set.raw));

		if (edp_config_set.bits.PANEL_MODE_EDP
			!= panel_mode_edp) {
			enum ddc_result result = DDC_RESULT_UNKNOWN;

			edp_config_set.bits.PANEL_MODE_EDP =
			panel_mode_edp;
			result = core_link_write_dpcd(
				link,
				DP_EDP_CONFIGURATION_SET,
				&edp_config_set.raw,
				sizeof(edp_config_set.raw));

			ASSERT(result == DDC_RESULT_SUCESSFULL);
		}
	}
	DC_LOG_DETECTION_DP_CAPS("Link: %d eDP panel mode supported: %d "
			"eDP panel mode enabled: %d \n",
			link->link_index,
			link->dpcd_caps.panel_mode_edp,
			panel_mode_edp);
}

static void enable_stream_features(struct pipe_ctx *pipe_ctx)
{
	struct dc_stream_state *stream = pipe_ctx->stream;
	struct dc_link *link = stream->sink->link;
	union down_spread_ctrl old_downspread;
	union down_spread_ctrl new_downspread;

	core_link_read_dpcd(link, DP_DOWNSPREAD_CTRL,
			&old_downspread.raw, sizeof(old_downspread));

	new_downspread.raw = old_downspread.raw;

	new_downspread.bits.IGNORE_MSA_TIMING_PARAM =
			(stream->ignore_msa_timing_param) ? 1 : 0;

	if (new_downspread.raw != old_downspread.raw) {
		core_link_write_dpcd(link, DP_DOWNSPREAD_CTRL,
			&new_downspread.raw, sizeof(new_downspread));
	}
}

static enum dc_status enable_link_dp(
		struct dc_state *state,
		struct pipe_ctx *pipe_ctx)
{
	struct dc_stream_state *stream = pipe_ctx->stream;
	enum dc_status status;
	bool skip_video_pattern;
	struct dc_link *link = stream->sink->link;
	struct dc_link_settings link_settings = {0};
	enum dp_panel_mode panel_mode;
	enum dc_link_rate max_link_rate = LINK_RATE_HIGH2;

	/* get link settings for video mode timing */
	decide_link_settings(stream, &link_settings);

	/* raise clock state for HBR3 if required. Confirmed with HW DCE/DPCS
	 * logic for HBR3 still needs Nominal (0.8V) on VDDC rail
	 */
	if (link->link_enc->features.flags.bits.IS_HBR3_CAPABLE)
		max_link_rate = LINK_RATE_HIGH3;

	if (link_settings.link_rate == max_link_rate) {
		struct dc_clocks clocks = state->bw.dcn.clk;

		/* dce/dcn compat, do not update dispclk */
		clocks.dispclk_khz = 0;
		/* 27mhz = 27000000hz= 27000khz */
		clocks.phyclk_khz = link_settings.link_rate * 27000;

		state->dis_clk->funcs->update_clocks(
				state->dis_clk, &clocks, false);
	}

	dp_enable_link_phy(
		link,
		pipe_ctx->stream->signal,
		pipe_ctx->clock_source->id,
		&link_settings);

	if (stream->sink->edid_caps.panel_patch.dppowerup_delay > 0) {
		int delay_dp_power_up_in_ms = stream->sink->edid_caps.panel_patch.dppowerup_delay;

		drm_msleep(delay_dp_power_up_in_ms);
	}

	panel_mode = dp_get_panel_mode(link);
	dpcd_configure_panel_mode(link, panel_mode);

	skip_video_pattern = true;

	if (link_settings.link_rate == LINK_RATE_LOW)
			skip_video_pattern = false;

	if (perform_link_training_with_retries(
			link,
			&link_settings,
			skip_video_pattern,
			LINK_TRAINING_ATTEMPTS)) {
		link->cur_link_settings = link_settings;
		status = DC_OK;
	}
	else
		status = DC_FAIL_DP_LINK_TRAINING;

	enable_stream_features(pipe_ctx);

	return status;
}

static enum dc_status enable_link_edp(
		struct dc_state *state,
		struct pipe_ctx *pipe_ctx)
{
	enum dc_status status;
	struct dc_stream_state *stream = pipe_ctx->stream;
	struct dc_link *link = stream->sink->link;
	/*in case it is not on*/
	link->dc->hwss.edp_power_control(link, true);
	link->dc->hwss.edp_wait_for_hpd_ready(link, true);

	status = enable_link_dp(state, pipe_ctx);


	return status;
}

static enum dc_status enable_link_dp_mst(
		struct dc_state *state,
		struct pipe_ctx *pipe_ctx)
{
	struct dc_link *link = pipe_ctx->stream->sink->link;

	/* sink signal type after MST branch is MST. Multiple MST sinks
	 * share one link. Link DP PHY is enable or training only once.
	 */
	if (link->cur_link_settings.lane_count != LANE_COUNT_UNKNOWN)
		return DC_OK;

	/* clear payload table */
	dm_helpers_dp_mst_clear_payload_allocation_table(link->ctx, link);

	/* set the sink to MST mode before enabling the link */
	dp_enable_mst_on_sink(link, true);

	return enable_link_dp(state, pipe_ctx);
}

static bool get_ext_hdmi_settings(struct pipe_ctx *pipe_ctx,
		enum engine_id eng_id,
		struct ext_hdmi_settings *settings)
{
	bool result = false;
	int i = 0;
	struct integrated_info *integrated_info =
			pipe_ctx->stream->ctx->dc_bios->integrated_info;

	if (integrated_info == NULL)
		return false;

	/*
	 * Get retimer settings from sbios for passing SI eye test for DCE11
	 * The setting values are varied based on board revision and port id
	 * Therefore the setting values of each ports is passed by sbios.
	 */

	// Check if current bios contains ext Hdmi settings
	if (integrated_info->gpu_cap_info & 0x20) {
		switch (eng_id) {
		case ENGINE_ID_DIGA:
			settings->slv_addr = integrated_info->dp0_ext_hdmi_slv_addr;
			settings->reg_num = integrated_info->dp0_ext_hdmi_6g_reg_num;
			settings->reg_num_6g = integrated_info->dp0_ext_hdmi_6g_reg_num;
			memmove(settings->reg_settings,
					integrated_info->dp0_ext_hdmi_reg_settings,
					sizeof(integrated_info->dp0_ext_hdmi_reg_settings));
			memmove(settings->reg_settings_6g,
					integrated_info->dp0_ext_hdmi_6g_reg_settings,
					sizeof(integrated_info->dp0_ext_hdmi_6g_reg_settings));
			result = true;
			break;
		case ENGINE_ID_DIGB:
			settings->slv_addr = integrated_info->dp1_ext_hdmi_slv_addr;
			settings->reg_num = integrated_info->dp1_ext_hdmi_6g_reg_num;
			settings->reg_num_6g = integrated_info->dp1_ext_hdmi_6g_reg_num;
			memmove(settings->reg_settings,
					integrated_info->dp1_ext_hdmi_reg_settings,
					sizeof(integrated_info->dp1_ext_hdmi_reg_settings));
			memmove(settings->reg_settings_6g,
					integrated_info->dp1_ext_hdmi_6g_reg_settings,
					sizeof(integrated_info->dp1_ext_hdmi_6g_reg_settings));
			result = true;
			break;
		case ENGINE_ID_DIGC:
			settings->slv_addr = integrated_info->dp2_ext_hdmi_slv_addr;
			settings->reg_num = integrated_info->dp2_ext_hdmi_6g_reg_num;
			settings->reg_num_6g = integrated_info->dp2_ext_hdmi_6g_reg_num;
			memmove(settings->reg_settings,
					integrated_info->dp2_ext_hdmi_reg_settings,
					sizeof(integrated_info->dp2_ext_hdmi_reg_settings));
			memmove(settings->reg_settings_6g,
					integrated_info->dp2_ext_hdmi_6g_reg_settings,
					sizeof(integrated_info->dp2_ext_hdmi_6g_reg_settings));
			result = true;
			break;
		case ENGINE_ID_DIGD:
			settings->slv_addr = integrated_info->dp3_ext_hdmi_slv_addr;
			settings->reg_num = integrated_info->dp3_ext_hdmi_6g_reg_num;
			settings->reg_num_6g = integrated_info->dp3_ext_hdmi_6g_reg_num;
			memmove(settings->reg_settings,
					integrated_info->dp3_ext_hdmi_reg_settings,
					sizeof(integrated_info->dp3_ext_hdmi_reg_settings));
			memmove(settings->reg_settings_6g,
					integrated_info->dp3_ext_hdmi_6g_reg_settings,
					sizeof(integrated_info->dp3_ext_hdmi_6g_reg_settings));
			result = true;
			break;
		default:
			break;
		}

		if (result == true) {
			// Validate settings from bios integrated info table
			if (settings->slv_addr == 0)
				return false;
			if (settings->reg_num > 9)
				return false;
			if (settings->reg_num_6g > 3)
				return false;

			for (i = 0; i < settings->reg_num; i++) {
				if (settings->reg_settings[i].i2c_reg_index > 0x20)
					return false;
			}

			for (i = 0; i < settings->reg_num_6g; i++) {
				if (settings->reg_settings_6g[i].i2c_reg_index > 0x20)
					return false;
			}
		}
	}

	return result;
}

static bool i2c_write(struct pipe_ctx *pipe_ctx,
		uint8_t address, uint8_t *buffer, uint32_t length)
{
	struct i2c_command cmd = {0};
	struct i2c_payload payload = {0};

	memset(&payload, 0, sizeof(payload));
	memset(&cmd, 0, sizeof(cmd));

	cmd.number_of_payloads = 1;
	cmd.engine = I2C_COMMAND_ENGINE_DEFAULT;
	cmd.speed = pipe_ctx->stream->ctx->dc->caps.i2c_speed_in_khz;

	payload.address = address;
	payload.data = buffer;
	payload.length = length;
	payload.write = true;
	cmd.payloads = &payload;

	if (dc_submit_i2c(pipe_ctx->stream->ctx->dc,
			pipe_ctx->stream->sink->link->link_index, &cmd))
		return true;

	return false;
}

static void write_i2c_retimer_setting(
		struct pipe_ctx *pipe_ctx,
		bool is_vga_mode,
		bool is_over_340mhz,
		struct ext_hdmi_settings *settings)
{
	uint8_t slave_address = (settings->slv_addr >> 1);
	uint8_t buffer[2];
	const uint8_t apply_rx_tx_change = 0x4;
	uint8_t offset = 0xA;
	uint8_t value = 0;
	int i = 0;
	bool i2c_success = false;

	memset(&buffer, 0, sizeof(buffer));

	/* Start Ext-Hdmi programming*/

	for (i = 0; i < settings->reg_num; i++) {
		/* Apply 3G settings */
		if (settings->reg_settings[i].i2c_reg_index <= 0x20) {

			buffer[0] = settings->reg_settings[i].i2c_reg_index;
			buffer[1] = settings->reg_settings[i].i2c_reg_val;
			i2c_success = i2c_write(pipe_ctx, slave_address,
						buffer, sizeof(buffer));

			if (!i2c_success)
				/* Write failure */
				ASSERT(i2c_success);

			/* Based on DP159 specs, APPLY_RX_TX_CHANGE bit in 0x0A
			 * needs to be set to 1 on every 0xA-0xC write.
			 */
			if (settings->reg_settings[i].i2c_reg_index == 0xA ||
				settings->reg_settings[i].i2c_reg_index == 0xB ||
				settings->reg_settings[i].i2c_reg_index == 0xC) {

				/* Query current value from offset 0xA */
				if (settings->reg_settings[i].i2c_reg_index == 0xA)
					value = settings->reg_settings[i].i2c_reg_val;
				else {
					i2c_success =
						dal_ddc_service_query_ddc_data(
						pipe_ctx->stream->sink->link->ddc,
						slave_address, &offset, 1, &value, 1);
					if (!i2c_success)
						/* Write failure */
						ASSERT(i2c_success);
				}

				buffer[0] = offset;
				/* Set APPLY_RX_TX_CHANGE bit to 1 */
				buffer[1] = value | apply_rx_tx_change;
				i2c_success = i2c_write(pipe_ctx, slave_address,
						buffer, sizeof(buffer));
				if (!i2c_success)
					/* Write failure */
					ASSERT(i2c_success);
			}
		}
	}

	/* Apply 3G settings */
	if (is_over_340mhz) {
		for (i = 0; i < settings->reg_num_6g; i++) {
			/* Apply 3G settings */
			if (settings->reg_settings[i].i2c_reg_index <= 0x20) {

				buffer[0] = settings->reg_settings_6g[i].i2c_reg_index;
				buffer[1] = settings->reg_settings_6g[i].i2c_reg_val;
				i2c_success = i2c_write(pipe_ctx, slave_address,
							buffer, sizeof(buffer));

				if (!i2c_success)
					/* Write failure */
					ASSERT(i2c_success);

				/* Based on DP159 specs, APPLY_RX_TX_CHANGE bit in 0x0A
				 * needs to be set to 1 on every 0xA-0xC write.
				 */
				if (settings->reg_settings_6g[i].i2c_reg_index == 0xA ||
					settings->reg_settings_6g[i].i2c_reg_index == 0xB ||
					settings->reg_settings_6g[i].i2c_reg_index == 0xC) {

					/* Query current value from offset 0xA */
					if (settings->reg_settings_6g[i].i2c_reg_index == 0xA)
						value = settings->reg_settings_6g[i].i2c_reg_val;
					else {
						i2c_success =
								dal_ddc_service_query_ddc_data(
								pipe_ctx->stream->sink->link->ddc,
								slave_address, &offset, 1, &value, 1);
						if (!i2c_success)
							/* Write failure */
							ASSERT(i2c_success);
					}

					buffer[0] = offset;
					/* Set APPLY_RX_TX_CHANGE bit to 1 */
					buffer[1] = value | apply_rx_tx_change;
					i2c_success = i2c_write(pipe_ctx, slave_address,
							buffer, sizeof(buffer));
					if (!i2c_success)
						/* Write failure */
						ASSERT(i2c_success);
				}
			}
		}
	}

	if (is_vga_mode) {
		/* Program additional settings if using 640x480 resolution */

		/* Write offset 0xFF to 0x01 */
		buffer[0] = 0xff;
		buffer[1] = 0x01;
		i2c_success = i2c_write(pipe_ctx, slave_address,
				buffer, sizeof(buffer));
		if (!i2c_success)
			/* Write failure */
			ASSERT(i2c_success);

		/* Write offset 0x00 to 0x23 */
		buffer[0] = 0x00;
		buffer[1] = 0x23;
		i2c_success = i2c_write(pipe_ctx, slave_address,
				buffer, sizeof(buffer));
		if (!i2c_success)
			/* Write failure */
			ASSERT(i2c_success);

		/* Write offset 0xff to 0x00 */
		buffer[0] = 0xff;
		buffer[1] = 0x00;
		i2c_success = i2c_write(pipe_ctx, slave_address,
				buffer, sizeof(buffer));
		if (!i2c_success)
			/* Write failure */
			ASSERT(i2c_success);

	}
}

static void write_i2c_default_retimer_setting(
		struct pipe_ctx *pipe_ctx,
		bool is_vga_mode,
		bool is_over_340mhz)
{
	uint8_t slave_address = (0xBA >> 1);
	uint8_t buffer[2];
	bool i2c_success = false;

	memset(&buffer, 0, sizeof(buffer));

	/* Program Slave Address for tuning single integrity */
	/* Write offset 0x0A to 0x13 */
	buffer[0] = 0x0A;
	buffer[1] = 0x13;
	i2c_success = i2c_write(pipe_ctx, slave_address,
			buffer, sizeof(buffer));
	if (!i2c_success)
		/* Write failure */
		ASSERT(i2c_success);

	/* Write offset 0x0A to 0x17 */
	buffer[0] = 0x0A;
	buffer[1] = 0x17;
	i2c_success = i2c_write(pipe_ctx, slave_address,
			buffer, sizeof(buffer));
	if (!i2c_success)
		/* Write failure */
		ASSERT(i2c_success);

	/* Write offset 0x0B to 0xDA or 0xD8 */
	buffer[0] = 0x0B;
	buffer[1] = is_over_340mhz ? 0xDA : 0xD8;
	i2c_success = i2c_write(pipe_ctx, slave_address,
			buffer, sizeof(buffer));
	if (!i2c_success)
		/* Write failure */
		ASSERT(i2c_success);

	/* Write offset 0x0A to 0x17 */
	buffer[0] = 0x0A;
	buffer[1] = 0x17;
	i2c_success = i2c_write(pipe_ctx, slave_address,
			buffer, sizeof(buffer));
	if (!i2c_success)
		/* Write failure */
		ASSERT(i2c_success);

	/* Write offset 0x0C to 0x1D or 0x91 */
	buffer[0] = 0x0C;
	buffer[1] = is_over_340mhz ? 0x1D : 0x91;
	i2c_success = i2c_write(pipe_ctx, slave_address,
			buffer, sizeof(buffer));
	if (!i2c_success)
		/* Write failure */
		ASSERT(i2c_success);

	/* Write offset 0x0A to 0x17 */
	buffer[0] = 0x0A;
	buffer[1] = 0x17;
	i2c_success = i2c_write(pipe_ctx, slave_address,
			buffer, sizeof(buffer));
	if (!i2c_success)
		/* Write failure */
		ASSERT(i2c_success);


	if (is_vga_mode) {
		/* Program additional settings if using 640x480 resolution */

		/* Write offset 0xFF to 0x01 */
		buffer[0] = 0xff;
		buffer[1] = 0x01;
		i2c_success = i2c_write(pipe_ctx, slave_address,
				buffer, sizeof(buffer));
		if (!i2c_success)
			/* Write failure */
			ASSERT(i2c_success);

		/* Write offset 0x00 to 0x23 */
		buffer[0] = 0x00;
		buffer[1] = 0x23;
		i2c_success = i2c_write(pipe_ctx, slave_address,
				buffer, sizeof(buffer));
		if (!i2c_success)
			/* Write failure */
			ASSERT(i2c_success);

		/* Write offset 0xff to 0x00 */
		buffer[0] = 0xff;
		buffer[1] = 0x00;
		i2c_success = i2c_write(pipe_ctx, slave_address,
				buffer, sizeof(buffer));
		if (!i2c_success)
			/* Write failure */
			ASSERT(i2c_success);
	}
}

static void write_i2c_redriver_setting(
		struct pipe_ctx *pipe_ctx,
		bool is_over_340mhz)
{
	uint8_t slave_address = (0xF0 >> 1);
	uint8_t buffer[16];
	bool i2c_success = false;

	memset(&buffer, 0, sizeof(buffer));

	// Program Slave Address for tuning single integrity
	buffer[3] = 0x4E;
	buffer[4] = 0x4E;
	buffer[5] = 0x4E;
	buffer[6] = is_over_340mhz ? 0x4E : 0x4A;

	i2c_success = i2c_write(pipe_ctx, slave_address,
					buffer, sizeof(buffer));

	if (!i2c_success)
		/* Write failure */
		ASSERT(i2c_success);
}

static void enable_link_hdmi(struct pipe_ctx *pipe_ctx)
{
	struct dc_stream_state *stream = pipe_ctx->stream;
	struct dc_link *link = stream->sink->link;
	enum dc_color_depth display_color_depth;
	enum engine_id eng_id;
	struct ext_hdmi_settings settings = {0};
	bool is_over_340mhz = false;
	bool is_vga_mode = (stream->timing.h_addressable == 640)
			&& (stream->timing.v_addressable == 480);

	if (stream->phy_pix_clk == 0)
		stream->phy_pix_clk = stream->timing.pix_clk_khz;
	if (stream->phy_pix_clk > 340000)
		is_over_340mhz = true;

	if (dc_is_hdmi_signal(pipe_ctx->stream->signal)) {
		unsigned short masked_chip_caps = pipe_ctx->stream->sink->link->chip_caps &
				EXT_DISPLAY_PATH_CAPS__EXT_CHIP_MASK;
		if (masked_chip_caps == EXT_DISPLAY_PATH_CAPS__HDMI20_TISN65DP159RSBT) {
			/* DP159, Retimer settings */
			eng_id = pipe_ctx->stream_res.stream_enc->id;

			if (get_ext_hdmi_settings(pipe_ctx, eng_id, &settings)) {
				write_i2c_retimer_setting(pipe_ctx,
						is_vga_mode, is_over_340mhz, &settings);
			} else {
				write_i2c_default_retimer_setting(pipe_ctx,
						is_vga_mode, is_over_340mhz);
			}
		} else if (masked_chip_caps == EXT_DISPLAY_PATH_CAPS__HDMI20_PI3EQX1204) {
			/* PI3EQX1204, Redriver settings */
			write_i2c_redriver_setting(pipe_ctx, is_over_340mhz);
		}
	}

	if (dc_is_hdmi_signal(pipe_ctx->stream->signal))
		dal_ddc_service_write_scdc_data(
			stream->sink->link->ddc,
			stream->phy_pix_clk,
			stream->timing.flags.LTE_340MCSC_SCRAMBLE);

	memset(&stream->sink->link->cur_link_settings, 0,
			sizeof(struct dc_link_settings));

	display_color_depth = stream->timing.display_color_depth;
	if (stream->timing.pixel_encoding == PIXEL_ENCODING_YCBCR422)
		display_color_depth = COLOR_DEPTH_888;

	link->link_enc->funcs->enable_tmds_output(
			link->link_enc,
			pipe_ctx->clock_source->id,
			display_color_depth,
			pipe_ctx->stream->signal,
			stream->phy_pix_clk);

	if (pipe_ctx->stream->signal == SIGNAL_TYPE_HDMI_TYPE_A)
		dal_ddc_service_read_scdc_data(link->ddc);
}

/****************************enable_link***********************************/
static enum dc_status enable_link(
		struct dc_state *state,
		struct pipe_ctx *pipe_ctx)
{
	enum dc_status status = DC_ERROR_UNEXPECTED;
	switch (pipe_ctx->stream->signal) {
	case SIGNAL_TYPE_DISPLAY_PORT:
		status = enable_link_dp(state, pipe_ctx);
		break;
	case SIGNAL_TYPE_EDP:
		status = enable_link_edp(state, pipe_ctx);
		break;
	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		status = enable_link_dp_mst(state, pipe_ctx);
		drm_msleep(200);
		break;
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
	case SIGNAL_TYPE_HDMI_TYPE_A:
		enable_link_hdmi(pipe_ctx);
		status = DC_OK;
		break;
	case SIGNAL_TYPE_VIRTUAL:
		status = DC_OK;
		break;
	default:
		break;
	}

	return status;
}

static void disable_link(struct dc_link *link, enum amd_signal_type signal)
{
	/*
	 * TODO: implement call for dp_set_hw_test_pattern
	 * it is needed for compliance testing
	 */

	/* here we need to specify that encoder output settings
	 * need to be calculated as for the set mode,
	 * it will lead to querying dynamic link capabilities
	 * which should be done before enable output */

	if (dc_is_dp_signal(signal)) {
		/* SST DP, eDP */
		if (dc_is_dp_sst_signal(signal))
			dp_disable_link_phy(link, signal);
		else
			dp_disable_link_phy_mst(link, signal);
	} else
		link->link_enc->funcs->disable_output(link->link_enc, signal);
}

static bool dp_active_dongle_validate_timing(
		const struct dc_crtc_timing *timing,
		const struct dpcd_caps *dpcd_caps)
{
	unsigned int required_pix_clk = timing->pix_clk_khz;
	const struct dc_dongle_caps *dongle_caps = &dpcd_caps->dongle_caps;

	switch (dpcd_caps->dongle_type) {
	case DISPLAY_DONGLE_DP_VGA_CONVERTER:
	case DISPLAY_DONGLE_DP_DVI_CONVERTER:
	case DISPLAY_DONGLE_DP_DVI_DONGLE:
		if (timing->pixel_encoding == PIXEL_ENCODING_RGB)
			return true;
		else
			return false;
	default:
		break;
	}

	if (dongle_caps->dongle_type != DISPLAY_DONGLE_DP_HDMI_CONVERTER ||
		dongle_caps->extendedCapValid == false)
		return true;

	/* Check Pixel Encoding */
	switch (timing->pixel_encoding) {
	case PIXEL_ENCODING_RGB:
	case PIXEL_ENCODING_YCBCR444:
		break;
	case PIXEL_ENCODING_YCBCR422:
		if (!dongle_caps->is_dp_hdmi_ycbcr422_pass_through)
			return false;
		break;
	case PIXEL_ENCODING_YCBCR420:
		if (!dongle_caps->is_dp_hdmi_ycbcr420_pass_through)
			return false;
		break;
	default:
		/* Invalid Pixel Encoding*/
		return false;
	}


	/* Check Color Depth and Pixel Clock */
	if (timing->pixel_encoding == PIXEL_ENCODING_YCBCR420)
		required_pix_clk /= 2;
	else if (timing->pixel_encoding == PIXEL_ENCODING_YCBCR422)
		required_pix_clk = required_pix_clk * 2 / 3;

	switch (timing->display_color_depth) {
	case COLOR_DEPTH_666:
	case COLOR_DEPTH_888:
		/*888 and 666 should always be supported*/
		break;
	case COLOR_DEPTH_101010:
		if (dongle_caps->dp_hdmi_max_bpc < 10)
			return false;
		required_pix_clk = required_pix_clk * 10 / 8;
		break;
	case COLOR_DEPTH_121212:
		if (dongle_caps->dp_hdmi_max_bpc < 12)
			return false;
		required_pix_clk = required_pix_clk * 12 / 8;
		break;

	case COLOR_DEPTH_141414:
	case COLOR_DEPTH_161616:
	default:
		/* These color depths are currently not supported */
		return false;
	}

	if (required_pix_clk > dongle_caps->dp_hdmi_max_pixel_clk)
		return false;

	return true;
}

enum dc_status dc_link_validate_mode_timing(
		const struct dc_stream_state *stream,
		struct dc_link *link,
		const struct dc_crtc_timing *timing)
{
	uint32_t max_pix_clk = stream->sink->dongle_max_pix_clk;
	struct dpcd_caps *dpcd_caps = &link->dpcd_caps;

	/* A hack to avoid failing any modes for EDID override feature on
	 * topology change such as lower quality cable for DP or different dongle
	 */
	if (link->remote_sinks[0])
		return DC_OK;

	/* Passive Dongle */
	if (0 != max_pix_clk && timing->pix_clk_khz > max_pix_clk)
		return DC_EXCEED_DONGLE_CAP;

	/* Active Dongle*/
	if (!dp_active_dongle_validate_timing(timing, dpcd_caps))
		return DC_EXCEED_DONGLE_CAP;

	switch (stream->signal) {
	case SIGNAL_TYPE_EDP:
	case SIGNAL_TYPE_DISPLAY_PORT:
		if (!dp_validate_mode_timing(
				link,
				timing))
			return DC_NO_DP_LINK_BANDWIDTH;
		break;

	default:
		break;
	}

	return DC_OK;
}

int dc_link_get_backlight_level(const struct dc_link *link)
{
	struct abm *abm = link->ctx->dc->res_pool->abm;

	if (abm == NULL || abm->funcs->get_current_backlight_8_bit == NULL)
		return DC_ERROR_UNEXPECTED;

	return (int) abm->funcs->get_current_backlight_8_bit(abm);
}

bool dc_link_set_backlight_level(const struct dc_link *link, uint32_t level,
		uint32_t frame_ramp, const struct dc_stream_state *stream)
{
	struct dc  *core_dc = link->ctx->dc;
	struct abm *abm = core_dc->res_pool->abm;
	struct dmcu *dmcu = core_dc->res_pool->dmcu;
	unsigned int controller_id = 0;
	bool use_smooth_brightness = true;
	int i;
	DC_LOGGER_INIT(link->ctx->logger);

	if ((dmcu == NULL) ||
		(abm == NULL) ||
		(abm->funcs->set_backlight_level == NULL))
		return false;

	if (stream) {
		if (stream->bl_pwm_level == EDP_BACKLIGHT_RAMP_DISABLE_LEVEL)
			frame_ramp = 0;

		((struct dc_stream_state *)stream)->bl_pwm_level = level;
	}

	use_smooth_brightness = dmcu->funcs->is_dmcu_initialized(dmcu);

	DC_LOG_BACKLIGHT("New Backlight level: %d (0x%X)\n", level, level);

	if (dc_is_embedded_signal(link->connector_signal)) {
		if (stream != NULL) {
			for (i = 0; i < MAX_PIPES; i++) {
				if (core_dc->current_state->res_ctx.
						pipe_ctx[i].stream
						== stream)
					/* DMCU -1 for all controller id values,
					 * therefore +1 here
					 */
					controller_id =
						core_dc->current_state->
						res_ctx.pipe_ctx[i].stream_res.tg->inst +
						1;
			}
		}
		abm->funcs->set_backlight_level(
				abm,
				level,
				frame_ramp,
				controller_id,
				use_smooth_brightness);
	}

	return true;
}

bool dc_link_set_abm_disable(const struct dc_link *link)
{
	struct dc  *core_dc = link->ctx->dc;
	struct abm *abm = core_dc->res_pool->abm;

	if ((abm == NULL) || (abm->funcs->set_backlight_level == NULL))
		return false;

	abm->funcs->set_abm_immediate_disable(abm);

	return true;
}

bool dc_link_set_psr_enable(const struct dc_link *link, bool enable, bool wait)
{
	struct dc  *core_dc = link->ctx->dc;
	struct dmcu *dmcu = core_dc->res_pool->dmcu;

	if (dmcu != NULL && link->psr_enabled)
		dmcu->funcs->set_psr_enable(dmcu, enable, wait);

	return true;
}

const struct dc_link_status *dc_link_get_status(const struct dc_link *link)
{
	return &link->link_status;
}

void core_link_resume(struct dc_link *link)
{
	if (link->connector_signal != SIGNAL_TYPE_VIRTUAL)
		program_hpd_filter(link);
}

static struct fixed31_32 get_pbn_per_slot(struct dc_stream_state *stream)
{
	struct dc_link_settings *link_settings =
			&stream->sink->link->cur_link_settings;
	uint32_t link_rate_in_mbps =
			link_settings->link_rate * LINK_RATE_REF_FREQ_IN_MHZ;
	struct fixed31_32 mbps = dc_fixpt_from_int(
			link_rate_in_mbps * link_settings->lane_count);

	return dc_fixpt_div_int(mbps, 54);
}

static int get_color_depth(enum dc_color_depth color_depth)
{
	switch (color_depth) {
	case COLOR_DEPTH_666: return 6;
	case COLOR_DEPTH_888: return 8;
	case COLOR_DEPTH_101010: return 10;
	case COLOR_DEPTH_121212: return 12;
	case COLOR_DEPTH_141414: return 14;
	case COLOR_DEPTH_161616: return 16;
	default: return 0;
	}
}

static struct fixed31_32 get_pbn_from_timing(struct pipe_ctx *pipe_ctx)
{
	uint32_t bpc;
	uint64_t kbps;
	struct fixed31_32 peak_kbps;
	uint32_t numerator;
	uint32_t denominator;

	bpc = get_color_depth(pipe_ctx->stream_res.pix_clk_params.color_depth);
	kbps = pipe_ctx->stream_res.pix_clk_params.requested_pix_clk * bpc * 3;

	/*
	 * margin 5300ppm + 300ppm ~ 0.6% as per spec, factor is 1.006
	 * The unit of 54/64Mbytes/sec is an arbitrary unit chosen based on
	 * common multiplier to render an integer PBN for all link rate/lane
	 * counts combinations
	 * calculate
	 * peak_kbps *= (1006/1000)
	 * peak_kbps *= (64/54)
	 * peak_kbps *= 8    convert to bytes
	 */

	numerator = 64 * PEAK_FACTOR_X1000;
	denominator = 54 * 8 * 1000 * 1000;
	kbps *= numerator;
	peak_kbps = dc_fixpt_from_fraction(kbps, denominator);

	return peak_kbps;
}

static void update_mst_stream_alloc_table(
	struct dc_link *link,
	struct stream_encoder *stream_enc,
	const struct dp_mst_stream_allocation_table *proposed_table)
{
	struct link_mst_stream_allocation work_table[MAX_CONTROLLER_NUM] = {
			{ 0 } };
	struct link_mst_stream_allocation *dc_alloc;

	int i;
	int j;

	/* if DRM proposed_table has more than one new payload */
	ASSERT(proposed_table->stream_count -
			link->mst_stream_alloc_table.stream_count < 2);

	/* copy proposed_table to link, add stream encoder */
	for (i = 0; i < proposed_table->stream_count; i++) {

		for (j = 0; j < link->mst_stream_alloc_table.stream_count; j++) {
			dc_alloc =
			&link->mst_stream_alloc_table.stream_allocations[j];

			if (dc_alloc->vcp_id ==
				proposed_table->stream_allocations[i].vcp_id) {

				work_table[i] = *dc_alloc;
				break; /* exit j loop */
			}
		}

		/* new vcp_id */
		if (j == link->mst_stream_alloc_table.stream_count) {
			work_table[i].vcp_id =
				proposed_table->stream_allocations[i].vcp_id;
			work_table[i].slot_count =
				proposed_table->stream_allocations[i].slot_count;
			work_table[i].stream_enc = stream_enc;
		}
	}

	/* update link->mst_stream_alloc_table with work_table */
	link->mst_stream_alloc_table.stream_count =
			proposed_table->stream_count;
	for (i = 0; i < MAX_CONTROLLER_NUM; i++)
		link->mst_stream_alloc_table.stream_allocations[i] =
				work_table[i];
}

/* convert link_mst_stream_alloc_table to dm dp_mst_stream_alloc_table
 * because stream_encoder is not exposed to dm
 */
static enum dc_status allocate_mst_payload(struct pipe_ctx *pipe_ctx)
{
	struct dc_stream_state *stream = pipe_ctx->stream;
	struct dc_link *link = stream->sink->link;
	struct link_encoder *link_encoder = link->link_enc;
	struct stream_encoder *stream_encoder = pipe_ctx->stream_res.stream_enc;
	struct dp_mst_stream_allocation_table proposed_table = {0};
	struct fixed31_32 avg_time_slots_per_mtp;
	struct fixed31_32 pbn;
	struct fixed31_32 pbn_per_slot;
	uint8_t i;
	DC_LOGGER_INIT(link->ctx->logger);

	/* enable_link_dp_mst already check link->enabled_stream_count
	 * and stream is in link->stream[]. This is called during set mode,
	 * stream_enc is available.
	 */

	/* get calculate VC payload for stream: stream_alloc */
	if (dm_helpers_dp_mst_write_payload_allocation_table(
		stream->ctx,
		stream,
		&proposed_table,
		true)) {
		update_mst_stream_alloc_table(
					link, pipe_ctx->stream_res.stream_enc, &proposed_table);
	}
	else
		DC_LOG_WARNING("Failed to update"
				"MST allocation table for"
				"pipe idx:%d\n",
				pipe_ctx->pipe_idx);

	DC_LOG_MST("%s  "
			"stream_count: %d: \n ",
			__func__,
			link->mst_stream_alloc_table.stream_count);

	for (i = 0; i < MAX_CONTROLLER_NUM; i++) {
		DC_LOG_MST("stream_enc[%d]: %p      "
		"stream[%d].vcp_id: %d      "
		"stream[%d].slot_count: %d\n",
		i,
		(void *) link->mst_stream_alloc_table.stream_allocations[i].stream_enc,
		i,
		link->mst_stream_alloc_table.stream_allocations[i].vcp_id,
		i,
		link->mst_stream_alloc_table.stream_allocations[i].slot_count);
	}

	ASSERT(proposed_table.stream_count > 0);

	/* program DP source TX for payload */
	link_encoder->funcs->update_mst_stream_allocation_table(
		link_encoder,
		&link->mst_stream_alloc_table);

	/* send down message */
	dm_helpers_dp_mst_poll_for_allocation_change_trigger(
			stream->ctx,
			stream);

	dm_helpers_dp_mst_send_payload_allocation(
			stream->ctx,
			stream,
			true);

	/* slot X.Y for only current stream */
	pbn_per_slot = get_pbn_per_slot(stream);
	pbn = get_pbn_from_timing(pipe_ctx);
	avg_time_slots_per_mtp = dc_fixpt_div(pbn, pbn_per_slot);

	stream_encoder->funcs->set_mst_bandwidth(
		stream_encoder,
		avg_time_slots_per_mtp);

	return DC_OK;

}

static enum dc_status deallocate_mst_payload(struct pipe_ctx *pipe_ctx)
{
	struct dc_stream_state *stream = pipe_ctx->stream;
	struct dc_link *link = stream->sink->link;
	struct link_encoder *link_encoder = link->link_enc;
	struct stream_encoder *stream_encoder = pipe_ctx->stream_res.stream_enc;
	struct dp_mst_stream_allocation_table proposed_table = {0};
	struct fixed31_32 avg_time_slots_per_mtp = dc_fixpt_from_int(0);
	uint8_t i;
	bool mst_mode = (link->type == dc_connection_mst_branch);
	DC_LOGGER_INIT(link->ctx->logger);

	/* deallocate_mst_payload is called before disable link. When mode or
	 * disable/enable monitor, new stream is created which is not in link
	 * stream[] yet. For this, payload is not allocated yet, so de-alloc
	 * should not done. For new mode set, map_resources will get engine
	 * for new stream, so stream_enc->id should be validated until here.
	 */

	/* slot X.Y */
	stream_encoder->funcs->set_mst_bandwidth(
		stream_encoder,
		avg_time_slots_per_mtp);

	/* TODO: which component is responsible for remove payload table? */
	if (mst_mode) {
		if (dm_helpers_dp_mst_write_payload_allocation_table(
				stream->ctx,
				stream,
				&proposed_table,
				false)) {

			update_mst_stream_alloc_table(
				link, pipe_ctx->stream_res.stream_enc, &proposed_table);
		}
		else {
				DC_LOG_WARNING("Failed to update"
						"MST allocation table for"
						"pipe idx:%d\n",
						pipe_ctx->pipe_idx);
		}
	}

	DC_LOG_MST("%s"
			"stream_count: %d: ",
			__func__,
			link->mst_stream_alloc_table.stream_count);

	for (i = 0; i < MAX_CONTROLLER_NUM; i++) {
		DC_LOG_MST("stream_enc[%d]: %p      "
		"stream[%d].vcp_id: %d      "
		"stream[%d].slot_count: %d\n",
		i,
		(void *) link->mst_stream_alloc_table.stream_allocations[i].stream_enc,
		i,
		link->mst_stream_alloc_table.stream_allocations[i].vcp_id,
		i,
		link->mst_stream_alloc_table.stream_allocations[i].slot_count);
	}

	link_encoder->funcs->update_mst_stream_allocation_table(
		link_encoder,
		&link->mst_stream_alloc_table);

	if (mst_mode) {
		dm_helpers_dp_mst_poll_for_allocation_change_trigger(
			stream->ctx,
			stream);

		dm_helpers_dp_mst_send_payload_allocation(
			stream->ctx,
			stream,
			false);
	}

	return DC_OK;
}

void core_link_enable_stream(
		struct dc_state *state,
		struct pipe_ctx *pipe_ctx)
{
	struct dc  *core_dc = pipe_ctx->stream->ctx->dc;
	enum dc_status status;
	DC_LOGGER_INIT(pipe_ctx->stream->ctx->logger);

	/* eDP lit up by bios already, no need to enable again. */
	if (pipe_ctx->stream->signal == SIGNAL_TYPE_EDP &&
		core_dc->apply_edp_fast_boot_optimization) {
		core_dc->apply_edp_fast_boot_optimization = false;
		pipe_ctx->stream->dpms_off = false;
		return;
	}

	if (pipe_ctx->stream->dpms_off)
		return;

	status = enable_link(state, pipe_ctx);

	if (status != DC_OK) {
			DC_LOG_WARNING("enabling link %u failed: %d\n",
			pipe_ctx->stream->sink->link->link_index,
			status);

			/* Abort stream enable *unless* the failure was due to
			 * DP link training - some DP monitors will recover and
			 * show the stream anyway. But MST displays can't proceed
			 * without link training.
			 */
			if (status != DC_FAIL_DP_LINK_TRAINING ||
					pipe_ctx->stream->signal == SIGNAL_TYPE_DISPLAY_PORT_MST) {
				BREAK_TO_DEBUGGER();
				return;
			}
	}

	core_dc->hwss.enable_audio_stream(pipe_ctx);

	/* turn off otg test pattern if enable */
	if (pipe_ctx->stream_res.tg->funcs->set_test_pattern)
		pipe_ctx->stream_res.tg->funcs->set_test_pattern(pipe_ctx->stream_res.tg,
				CONTROLLER_DP_TEST_PATTERN_VIDEOMODE,
				COLOR_DEPTH_UNDEFINED);

	core_dc->hwss.enable_stream(pipe_ctx);

	if (pipe_ctx->stream->signal == SIGNAL_TYPE_DISPLAY_PORT_MST)
		allocate_mst_payload(pipe_ctx);

	core_dc->hwss.unblank_stream(pipe_ctx,
		&pipe_ctx->stream->sink->link->cur_link_settings);
}

void core_link_disable_stream(struct pipe_ctx *pipe_ctx, int option)
{
	struct dc  *core_dc = pipe_ctx->stream->ctx->dc;

	core_dc->hwss.blank_stream(pipe_ctx);

	if (pipe_ctx->stream->signal == SIGNAL_TYPE_DISPLAY_PORT_MST)
		deallocate_mst_payload(pipe_ctx);

	core_dc->hwss.disable_stream(pipe_ctx, option);

	disable_link(pipe_ctx->stream->sink->link, pipe_ctx->stream->signal);
}

void core_link_set_avmute(struct pipe_ctx *pipe_ctx, bool enable)
{
	struct dc  *core_dc = pipe_ctx->stream->ctx->dc;

	if (pipe_ctx->stream->signal != SIGNAL_TYPE_HDMI_TYPE_A)
		return;

	core_dc->hwss.set_avmute(pipe_ctx, enable);
}

/**
 *****************************************************************************
 *  Function: dc_link_enable_hpd_filter
 *
 *  @brief
 *     If enable is true, programs HPD filter on associated HPD line using
 *     delay_on_disconnect/delay_on_connect values dependent on
 *     link->connector_signal
 *
 *     If enable is false, programs HPD filter on associated HPD line with no
 *     delays on connect or disconnect
 *
 *  @param [in] link: pointer to the dc link
 *  @param [in] enable: boolean specifying whether to enable hbd
 *****************************************************************************
 */
void dc_link_enable_hpd_filter(struct dc_link *link, bool enable)
{
	struct gpio *hpd;

	if (enable) {
		link->is_hpd_filter_disabled = false;
		program_hpd_filter(link);
	} else {
		link->is_hpd_filter_disabled = true;
		/* Obtain HPD handle */
		hpd = get_hpd_gpio(link->ctx->dc_bios, link->link_id, link->ctx->gpio_service);

		if (!hpd)
			return;

		/* Setup HPD filtering */
		if (dal_gpio_open(hpd, GPIO_MODE_INTERRUPT) == GPIO_RESULT_OK) {
			struct gpio_hpd_config config;

			config.delay_on_connect = 0;
			config.delay_on_disconnect = 0;

			dal_irq_setup_hpd_filter(hpd, &config);

			dal_gpio_close(hpd);
		} else {
			ASSERT_CRITICAL(false);
		}
		/* Release HPD handle */
		dal_gpio_destroy_irq(&hpd);
	}
}

