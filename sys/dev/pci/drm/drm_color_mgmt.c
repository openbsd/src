/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_color_mgmt.h>

#include "drm_crtc_internal.h"

/**
 * DOC: overview
 *
 * Color management or color space adjustments is supported through a set of 5
 * properties on the &drm_crtc object. They are set up by calling
 * drm_crtc_enable_color_mgmt().
 *
 * "DEGAMMA_LUT”:
 *	Blob property to set the degamma lookup table (LUT) mapping pixel data
 *	from the framebuffer before it is given to the transformation matrix.
 *	The data is interpreted as an array of &struct drm_color_lut elements.
 *	Hardware might choose not to use the full precision of the LUT elements
 *	nor use all the elements of the LUT (for example the hardware might
 *	choose to interpolate between LUT[0] and LUT[4]).
 *
 *	Setting this to NULL (blob property value set to 0) means a
 *	linear/pass-thru gamma table should be used. This is generally the
 *	driver boot-up state too. Drivers can access this blob through
 *	&drm_crtc_state.degamma_lut.
 *
 * “DEGAMMA_LUT_SIZE”:
 *	Unsinged range property to give the size of the lookup table to be set
 *	on the DEGAMMA_LUT property (the size depends on the underlying
 *	hardware). If drivers support multiple LUT sizes then they should
 *	publish the largest size, and sub-sample smaller sized LUTs (e.g. for
 *	split-gamma modes) appropriately.
 *
 * “CTM”:
 *	Blob property to set the current transformation matrix (CTM) apply to
 *	pixel data after the lookup through the degamma LUT and before the
 *	lookup through the gamma LUT. The data is interpreted as a struct
 *	&drm_color_ctm.
 *
 *	Setting this to NULL (blob property value set to 0) means a
 *	unit/pass-thru matrix should be used. This is generally the driver
 *	boot-up state too. Drivers can access the blob for the color conversion
 *	matrix through &drm_crtc_state.ctm.
 *
 * “GAMMA_LUT”:
 *	Blob property to set the gamma lookup table (LUT) mapping pixel data
 *	after the transformation matrix to data sent to the connector. The
 *	data is interpreted as an array of &struct drm_color_lut elements.
 *	Hardware might choose not to use the full precision of the LUT elements
 *	nor use all the elements of the LUT (for example the hardware might
 *	choose to interpolate between LUT[0] and LUT[4]).
 *
 *	Setting this to NULL (blob property value set to 0) means a
 *	linear/pass-thru gamma table should be used. This is generally the
 *	driver boot-up state too. Drivers can access this blob through
 *	&drm_crtc_state.gamma_lut.
 *
 * “GAMMA_LUT_SIZE”:
 *	Unsigned range property to give the size of the lookup table to be set
 *	on the GAMMA_LUT property (the size depends on the underlying hardware).
 *	If drivers support multiple LUT sizes then they should publish the
 *	largest size, and sub-sample smaller sized LUTs (e.g. for split-gamma
 *	modes) appropriately.
 *
 * There is also support for a legacy gamma table, which is set up by calling
 * drm_mode_crtc_set_gamma_size(). Drivers which support both should use
 * drm_atomic_helper_legacy_gamma_set() to alias the legacy gamma ramp with the
 * "GAMMA_LUT" property above.
 *
 * Support for different non RGB color encodings is controlled through
 * &drm_plane specific COLOR_ENCODING and COLOR_RANGE properties. They
 * are set up by calling drm_plane_create_color_properties().
 *
 * "COLOR_ENCODING"
 * 	Optional plane enum property to support different non RGB
 * 	color encodings. The driver can provide a subset of standard
 * 	enum values supported by the DRM plane.
 *
 * "COLOR_RANGE"
 * 	Optional plane enum property to support different non RGB
 * 	color parameter ranges. The driver can provide a subset of
 * 	standard enum values supported by the DRM plane.
 */

/**
 * drm_color_lut_extract - clamp and round LUT entries
 * @user_input: input value
 * @bit_precision: number of bits the hw LUT supports
 *
 * Extract a degamma/gamma LUT value provided by user (in the form of
 * &drm_color_lut entries) and round it to the precision supported by the
 * hardware.
 */
uint32_t drm_color_lut_extract(uint32_t user_input, uint32_t bit_precision)
{
	uint32_t val = user_input;
	uint32_t max = 0xffff >> (16 - bit_precision);

	/* Round only if we're not using full precision. */
	if (bit_precision < 16) {
		val += 1UL << (16 - bit_precision - 1);
		val >>= 16 - bit_precision;
	}

	return clamp_val(val, 0, max);
}
EXPORT_SYMBOL(drm_color_lut_extract);

/**
 * drm_crtc_enable_color_mgmt - enable color management properties
 * @crtc: DRM CRTC
 * @degamma_lut_size: the size of the degamma lut (before CSC)
 * @has_ctm: whether to attach ctm_property for CSC matrix
 * @gamma_lut_size: the size of the gamma lut (after CSC)
 *
 * This function lets the driver enable the color correction
 * properties on a CRTC. This includes 3 degamma, csc and gamma
 * properties that userspace can set and 2 size properties to inform
 * the userspace of the lut sizes. Each of the properties are
 * optional. The gamma and degamma properties are only attached if
 * their size is not 0 and ctm_property is only attached if has_ctm is
 * true.
 *
 * Drivers should use drm_atomic_helper_legacy_gamma_set() to implement the
 * legacy &drm_crtc_funcs.gamma_set callback.
 */
void drm_crtc_enable_color_mgmt(struct drm_crtc *crtc,
				uint degamma_lut_size,
				bool has_ctm,
				uint gamma_lut_size)
{
	struct drm_device *dev = crtc->dev;
	struct drm_mode_config *config = &dev->mode_config;

	if (degamma_lut_size) {
		drm_object_attach_property(&crtc->base,
					   config->degamma_lut_property, 0);
		drm_object_attach_property(&crtc->base,
					   config->degamma_lut_size_property,
					   degamma_lut_size);
	}

	if (has_ctm)
		drm_object_attach_property(&crtc->base,
					   config->ctm_property, 0);

	if (gamma_lut_size) {
		drm_object_attach_property(&crtc->base,
					   config->gamma_lut_property, 0);
		drm_object_attach_property(&crtc->base,
					   config->gamma_lut_size_property,
					   gamma_lut_size);
	}
}
EXPORT_SYMBOL(drm_crtc_enable_color_mgmt);

/**
 * drm_mode_crtc_set_gamma_size - set the gamma table size
 * @crtc: CRTC to set the gamma table size for
 * @gamma_size: size of the gamma table
 *
 * Drivers which support gamma tables should set this to the supported gamma
 * table size when initializing the CRTC. Currently the drm core only supports a
 * fixed gamma table size.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_crtc_set_gamma_size(struct drm_crtc *crtc,
				 int gamma_size)
{
	uint16_t *r_base, *g_base, *b_base;
	int i;

	crtc->gamma_size = gamma_size;

	crtc->gamma_store = kcalloc(gamma_size, sizeof(uint16_t) * 3,
				    GFP_KERNEL);
	if (!crtc->gamma_store) {
		crtc->gamma_size = 0;
		return -ENOMEM;
	}

	r_base = crtc->gamma_store;
	g_base = r_base + gamma_size;
	b_base = g_base + gamma_size;
	for (i = 0; i < gamma_size; i++) {
		r_base[i] = i << 8;
		g_base[i] = i << 8;
		b_base[i] = i << 8;
	}


	return 0;
}
EXPORT_SYMBOL(drm_mode_crtc_set_gamma_size);

/**
 * drm_mode_gamma_set_ioctl - set the gamma table
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * Set the gamma table of a CRTC to the one passed in by the user. Userspace can
 * inquire the required gamma table size through drm_mode_gamma_get_ioctl.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_gamma_set_ioctl(struct drm_device *dev,
			     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc_lut *crtc_lut = data;
	struct drm_crtc *crtc;
	void *r_base, *g_base, *b_base;
	int size;
	struct drm_modeset_acquire_ctx ctx;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	crtc = drm_crtc_find(dev, file_priv, crtc_lut->crtc_id);
	if (!crtc)
		return -ENOENT;

	if (crtc->funcs->gamma_set == NULL)
		return -ENOSYS;

	/* memcpy into gamma store */
	if (crtc_lut->gamma_size != crtc->gamma_size)
		return -EINVAL;

	drm_modeset_acquire_init(&ctx, 0);
retry:
	ret = drm_modeset_lock_all_ctx(dev, &ctx);
	if (ret)
		goto out;

	size = crtc_lut->gamma_size * (sizeof(uint16_t));
	r_base = crtc->gamma_store;
	if (copy_from_user(r_base, (void __user *)(unsigned long)crtc_lut->red, size)) {
		ret = -EFAULT;
		goto out;
	}

	g_base = r_base + size;
	if (copy_from_user(g_base, (void __user *)(unsigned long)crtc_lut->green, size)) {
		ret = -EFAULT;
		goto out;
	}

	b_base = g_base + size;
	if (copy_from_user(b_base, (void __user *)(unsigned long)crtc_lut->blue, size)) {
		ret = -EFAULT;
		goto out;
	}

	ret = crtc->funcs->gamma_set(crtc, r_base, g_base, b_base,
				     crtc->gamma_size, &ctx);

out:
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	}
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;

}

/**
 * drm_mode_gamma_get_ioctl - get the gamma table
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * Copy the current gamma table into the storage provided. This also provides
 * the gamma table size the driver expects, which can be used to size the
 * allocated storage.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_gamma_get_ioctl(struct drm_device *dev,
			     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc_lut *crtc_lut = data;
	struct drm_crtc *crtc;
	void *r_base, *g_base, *b_base;
	int size;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	crtc = drm_crtc_find(dev, file_priv, crtc_lut->crtc_id);
	if (!crtc)
		return -ENOENT;

	/* memcpy into gamma store */
	if (crtc_lut->gamma_size != crtc->gamma_size)
		return -EINVAL;

	drm_modeset_lock(&crtc->mutex, NULL);
	size = crtc_lut->gamma_size * (sizeof(uint16_t));
	r_base = crtc->gamma_store;
	if (copy_to_user((void __user *)(unsigned long)crtc_lut->red, r_base, size)) {
		ret = -EFAULT;
		goto out;
	}

	g_base = r_base + size;
	if (copy_to_user((void __user *)(unsigned long)crtc_lut->green, g_base, size)) {
		ret = -EFAULT;
		goto out;
	}

	b_base = g_base + size;
	if (copy_to_user((void __user *)(unsigned long)crtc_lut->blue, b_base, size)) {
		ret = -EFAULT;
		goto out;
	}
out:
	drm_modeset_unlock(&crtc->mutex);
	return ret;
}

static const char * const color_encoding_name[] = {
	[DRM_COLOR_YCBCR_BT601] = "ITU-R BT.601 YCbCr",
	[DRM_COLOR_YCBCR_BT709] = "ITU-R BT.709 YCbCr",
	[DRM_COLOR_YCBCR_BT2020] = "ITU-R BT.2020 YCbCr",
};

static const char * const color_range_name[] = {
	[DRM_COLOR_YCBCR_FULL_RANGE] = "YCbCr full range",
	[DRM_COLOR_YCBCR_LIMITED_RANGE] = "YCbCr limited range",
};

/**
 * drm_get_color_encoding_name - return a string for color encoding
 * @encoding: color encoding to compute name of
 *
 * In contrast to the other drm_get_*_name functions this one here returns a
 * const pointer and hence is threadsafe.
 */
const char *drm_get_color_encoding_name(enum drm_color_encoding encoding)
{
	if (WARN_ON(encoding >= ARRAY_SIZE(color_encoding_name)))
		return "unknown";

	return color_encoding_name[encoding];
}

/**
 * drm_get_color_range_name - return a string for color range
 * @range: color range to compute name of
 *
 * In contrast to the other drm_get_*_name functions this one here returns a
 * const pointer and hence is threadsafe.
 */
const char *drm_get_color_range_name(enum drm_color_range range)
{
	if (WARN_ON(range >= ARRAY_SIZE(color_range_name)))
		return "unknown";

	return color_range_name[range];
}

/**
 * drm_plane_create_color_properties - color encoding related plane properties
 * @plane: plane object
 * @supported_encodings: bitfield indicating supported color encodings
 * @supported_ranges: bitfileld indicating supported color ranges
 * @default_encoding: default color encoding
 * @default_range: default color range
 *
 * Create and attach plane specific COLOR_ENCODING and COLOR_RANGE
 * properties to @plane. The supported encodings and ranges should
 * be provided in supported_encodings and supported_ranges bitmasks.
 * Each bit set in the bitmask indicates that its number as enum
 * value is supported.
 */
int drm_plane_create_color_properties(struct drm_plane *plane,
				      u32 supported_encodings,
				      u32 supported_ranges,
				      enum drm_color_encoding default_encoding,
				      enum drm_color_range default_range)
{
	struct drm_device *dev = plane->dev;
	struct drm_property *prop;
	struct drm_prop_enum_list enum_list[max_t(int, DRM_COLOR_ENCODING_MAX,
						       DRM_COLOR_RANGE_MAX)];
	int i, len;

	if (WARN_ON(supported_encodings == 0 ||
		    (supported_encodings & -BIT(DRM_COLOR_ENCODING_MAX)) != 0 ||
		    (supported_encodings & BIT(default_encoding)) == 0))
		return -EINVAL;

	if (WARN_ON(supported_ranges == 0 ||
		    (supported_ranges & -BIT(DRM_COLOR_RANGE_MAX)) != 0 ||
		    (supported_ranges & BIT(default_range)) == 0))
		return -EINVAL;

	len = 0;
	for (i = 0; i < DRM_COLOR_ENCODING_MAX; i++) {
		if ((supported_encodings & BIT(i)) == 0)
			continue;

		enum_list[len].type = i;
		enum_list[len].name = color_encoding_name[i];
		len++;
	}

	prop = drm_property_create_enum(dev, 0, "COLOR_ENCODING",
					enum_list, len);
	if (!prop)
		return -ENOMEM;
	plane->color_encoding_property = prop;
	drm_object_attach_property(&plane->base, prop, default_encoding);
	if (plane->state)
		plane->state->color_encoding = default_encoding;

	len = 0;
	for (i = 0; i < DRM_COLOR_RANGE_MAX; i++) {
		if ((supported_ranges & BIT(i)) == 0)
			continue;

		enum_list[len].type = i;
		enum_list[len].name = color_range_name[i];
		len++;
	}

	prop = drm_property_create_enum(dev, 0,	"COLOR_RANGE",
					enum_list, len);
	if (!prop)
		return -ENOMEM;
	plane->color_range_property = prop;
	drm_object_attach_property(&plane->base, prop, default_range);
	if (plane->state)
		plane->state->color_range = default_range;

	return 0;
}
EXPORT_SYMBOL(drm_plane_create_color_properties);
