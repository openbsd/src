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

#ifndef __DAL_TIMING_GENERATOR_TYPES_H__
#define __DAL_TIMING_GENERATOR_TYPES_H__

#include "hw_shared.h"

struct dc_bios;

/* Contains CRTC vertical/horizontal pixel counters */
struct crtc_position {
	int32_t vertical_count;
	int32_t horizontal_count;
	int32_t nominal_vcount;
};

struct dcp_gsl_params {
	int gsl_group;
	int gsl_master;
};

struct gsl_params {
	int gsl0_en;
	int gsl1_en;
	int gsl2_en;
	int gsl_master_en;
	int gsl_master_mode;
	int master_update_lock_gsl_en;
	int gsl_window_start_x;
	int gsl_window_end_x;
	int gsl_window_start_y;
	int gsl_window_end_y;
};

/* define the structure of Dynamic Refresh Mode */
struct drr_params {
	uint32_t vertical_total_min;
	uint32_t vertical_total_max;
	bool immediate_flip;
};

#define LEFT_EYE_3D_PRIMARY_SURFACE 1
#define RIGHT_EYE_3D_PRIMARY_SURFACE 0

enum crtc_state {
	CRTC_STATE_VBLANK = 0,
	CRTC_STATE_VACTIVE
};

struct _dlg_otg_param {
	int vstartup_start;
	int vupdate_offset;
	int vupdate_width;
	int vready_offset;
	enum amd_signal_type signal;
};

struct vupdate_keepout_params {
	int start_offset;
	int end_offset;
	int enable;
};

struct crtc_stereo_flags {
	uint8_t PROGRAM_STEREO         : 1;
	uint8_t PROGRAM_POLARITY       : 1;
	uint8_t RIGHT_EYE_POLARITY     : 1;
	uint8_t FRAME_PACKED           : 1;
	uint8_t DISABLE_STEREO_DP_SYNC : 1;
};

enum crc_selection {
	/* Order must match values expected by hardware */
	UNION_WINDOW_A_B = 0,
	UNION_WINDOW_A_NOT_B,
	UNION_WINDOW_NOT_A_B,
	UNION_WINDOW_NOT_A_NOT_B,
	INTERSECT_WINDOW_A_B,
	INTERSECT_WINDOW_A_NOT_B,
	INTERSECT_WINDOW_NOT_A_B,
	INTERSECT_WINDOW_NOT_A_NOT_B,
};

struct crc_params {
	/* Regions used to calculate CRC*/
	uint16_t windowa_x_start;
	uint16_t windowa_x_end;
	uint16_t windowa_y_start;
	uint16_t windowa_y_end;

	uint16_t windowb_x_start;
	uint16_t windowb_x_end;
	uint16_t windowb_y_start;
	uint16_t windowb_y_end;

	enum crc_selection selection;

	bool continuous_mode;
	bool enable;
};

struct timing_generator {
	const struct timing_generator_funcs *funcs;
	struct dc_bios *bp;
	struct dc_context *ctx;
	struct _dlg_otg_param dlg_otg_param;
	int inst;
};

struct dc_crtc_timing;

struct drr_params;

struct timing_generator_funcs {
	bool (*validate_timing)(struct timing_generator *tg,
							const struct dc_crtc_timing *timing);
	void (*program_timing)(struct timing_generator *tg,
							const struct dc_crtc_timing *timing,
							bool use_vbios);
	void (*program_vline_interrupt)(struct timing_generator *optc,
			const struct dc_crtc_timing *dc_crtc_timing,
			unsigned long long vsync_delta);
	bool (*enable_crtc)(struct timing_generator *tg);
	bool (*disable_crtc)(struct timing_generator *tg);
	bool (*is_counter_moving)(struct timing_generator *tg);
	void (*get_position)(struct timing_generator *tg,
				struct crtc_position *position);

	uint32_t (*get_frame_count)(struct timing_generator *tg);
	void (*get_scanoutpos)(
		struct timing_generator *tg,
		uint32_t *v_blank_start,
		uint32_t *v_blank_end,
		uint32_t *h_position,
		uint32_t *v_position);
	bool (*get_otg_active_size)(struct timing_generator *optc,
			uint32_t *otg_active_width,
			uint32_t *otg_active_height);
	void (*set_early_control)(struct timing_generator *tg,
							   uint32_t early_cntl);
	void (*wait_for_state)(struct timing_generator *tg,
							enum crtc_state state);
	void (*set_blank)(struct timing_generator *tg,
					bool enable_blanking);
	bool (*is_blanked)(struct timing_generator *tg);
	void (*set_overscan_blank_color) (struct timing_generator *tg, const struct tg_color *color);
	void (*set_blank_color)(struct timing_generator *tg, const struct tg_color *color);
	void (*set_colors)(struct timing_generator *tg,
						const struct tg_color *blank_color,
						const struct tg_color *overscan_color);

	void (*disable_vga)(struct timing_generator *tg);
	bool (*did_triggered_reset_occur)(struct timing_generator *tg);
	void (*setup_global_swap_lock)(struct timing_generator *tg,
							const struct dcp_gsl_params *gsl_params);
	void (*unlock)(struct timing_generator *tg);
	void (*lock)(struct timing_generator *tg);
	void (*enable_reset_trigger)(struct timing_generator *tg,
				     int source_tg_inst);
	void (*enable_crtc_reset)(struct timing_generator *tg,
				  int source_tg_inst,
				  struct crtc_trigger_info *crtc_tp);
	void (*disable_reset_trigger)(struct timing_generator *tg);
	void (*tear_down_global_swap_lock)(struct timing_generator *tg);
	void (*enable_advanced_request)(struct timing_generator *tg,
					bool enable, const struct dc_crtc_timing *timing);
	void (*set_drr)(struct timing_generator *tg, const struct drr_params *params);
	void (*set_static_screen_control)(struct timing_generator *tg,
							uint32_t value);
	void (*set_test_pattern)(
		struct timing_generator *tg,
		enum controller_dp_test_pattern test_pattern,
		enum dc_color_depth color_depth);

	bool (*arm_vert_intr)(struct timing_generator *tg, uint8_t width);

	void (*program_global_sync)(struct timing_generator *tg);
	void (*enable_optc_clock)(struct timing_generator *tg, bool enable);
	void (*program_stereo)(struct timing_generator *tg,
		const struct dc_crtc_timing *timing, struct crtc_stereo_flags *flags);
	bool (*is_stereo_left_eye)(struct timing_generator *tg);

	void (*set_blank_data_double_buffer)(struct timing_generator *tg, bool enable);

	void (*tg_init)(struct timing_generator *tg);
	bool (*is_tg_enabled)(struct timing_generator *tg);
	bool (*is_optc_underflow_occurred)(struct timing_generator *tg);
	void (*clear_optc_underflow)(struct timing_generator *tg);

	/**
	 * Configure CRCs for the given timing generator. Return false if TG is
	 * not on.
	 */
	bool (*configure_crc)(struct timing_generator *tg,
			       const struct crc_params *params);

	/**
	 * Get CRCs for the given timing generator. Return false if CRCs are
	 * not enabled (via configure_crc).
	 */
	bool (*get_crc)(struct timing_generator *tg,
			uint32_t *r_cr, uint32_t *g_y, uint32_t *b_cb);

};

#endif
