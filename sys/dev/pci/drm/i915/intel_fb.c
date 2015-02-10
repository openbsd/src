/*	$OpenBSD: intel_fb.c,v 1.11 2015/02/10 01:39:32 jsg Exp $	*/
/*
 * Copyright © 2007 David Airlie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     David Airlie
 */

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm.h>
#include <dev/pci/drm/drm_crtc.h>
#include <dev/pci/drm/drm_fb_helper.h>
#include <dev/pci/drm/i915_drm.h>
#include "i915_drv.h"
#include "intel_drv.h"

static int intelfb_create(struct intel_fbdev *ifbdev,
    struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = ifbdev->helper.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
#if 0
	struct fb_info *info;
#endif
	struct drm_framebuffer *fb;
	struct drm_mode_fb_cmd2 mode_cmd = {};
	struct drm_i915_gem_object *obj;
	int size, ret;

	/* we don't do packed 24bpp */
	if (sizes->surface_bpp == 24)
		sizes->surface_bpp = 32;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;

	mode_cmd.pitches[0] = roundup2(mode_cmd.width * ((sizes->surface_bpp + 7) /
							 8), 64);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;
	size = roundup2(size, PAGE_SIZE);
	obj = i915_gem_alloc_object(dev, size);
	if (!obj) {
		DRM_ERROR("failed to allocate framebuffer\n");
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&dev->struct_mutex);

	/* Flush everything out, we'll be doing GTT only from now on */
	ret = intel_pin_and_fence_fb_obj(dev, obj, false);
	if (ret) {
		DRM_ERROR("failed to pin fb: %d\n", ret);
		goto out_unref;
	}

#if 0
	info = framebuffer_alloc(0, device);
	if (!info) {
		ret = -ENOMEM;
		goto out_unpin;
	}

	info->par = ifbdev;
#endif

	ret = intel_framebuffer_init(dev, &ifbdev->ifb, &mode_cmd, obj);
	if (ret)
		goto out_unpin;

	fb = &ifbdev->ifb.base;

	ifbdev->helper.fb = fb;
#if 0
	ifbdev->helper.fbdev = info;

	strlcpy(info->fix.id, "inteldrmfb", sizeof(info->fix.id));

	info->flags = FBINFO_DEFAULT | FBINFO_CAN_FORCE_OUTPUT;
	info->fbops = &intelfb_ops;

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out_unpin;
	}
	/* setup aperture base/size for vesafb takeover */
	info->apertures = alloc_apertures(1);
	if (!info->apertures) {
		ret = -ENOMEM;
		goto out_unpin;
	}
	info->apertures->ranges[0].base = dev->mode_config.fb_base;
	info->apertures->ranges[0].size =
		dev_priv->mm.gtt->gtt_mappable_entries << PAGE_SHIFT;

	info->fix.smem_start = dev->mode_config.fb_base + obj->gtt_offset;
	info->fix.smem_len = size;

	info->screen_base = ioremap_wc(dev->agp->base + obj->gtt_offset, size);
	if (!info->screen_base) {
		ret = -ENOSPC;
		goto out_unpin;
	}
	info->screen_size = size;

//	memset(info->screen_base, 0, size);

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, &ifbdev->helper, sizes->fb_width, sizes->fb_height);

	/* Use default scratch pixmap (info->pixmap.flags = FB_PIXMAP_SYSTEM) */
#else
{
	struct rasops_info *ri = &dev_priv->ro;
	bus_space_handle_t bsh;
	int err;

	err = agp_map_subregion(dev_priv->agph, obj->gtt_offset, size, &bsh);
	if (err) {
		ret = -err;
		goto out_unpin;
	}

	ri->ri_bits = bus_space_vaddr(dev->bst, bsh);
	ri->ri_depth = fb->bits_per_pixel;
	ri->ri_stride = fb->pitches[0];
	ri->ri_width = sizes->fb_width;
	ri->ri_height = sizes->fb_height;

	switch (fb->pixel_format) {
	case DRM_FORMAT_XRGB8888:
		ri->ri_rnum = 8;
		ri->ri_rpos = 16;
		ri->ri_gnum = 8;
		ri->ri_gpos = 8;
		ri->ri_bnum = 8;
		ri->ri_bpos = 0;
		break;
	case DRM_FORMAT_RGB565:
		ri->ri_rnum = 5;
		ri->ri_rpos = 11;
		ri->ri_gnum = 6;
		ri->ri_gpos = 5;
		ri->ri_bnum = 5;
		ri->ri_bpos = 0;
		break;
	}
}
#endif

	DRM_DEBUG_KMS("allocated %dx%d fb: 0x%08x, bo %p\n",
		      fb->width, fb->height,
		      obj->gtt_offset, obj);

	mutex_unlock(&dev->struct_mutex);
#if 1
	DRM_DEBUG_KMS("skipping call to vga_switcheroo_client_fb_set\n");
#else
	vga_switcheroo_client_fb_set(dev->pdev, info);
#endif
	return 0;

out_unpin:
	i915_gem_object_unpin(obj);
out_unref:
	drm_gem_object_unreference(&obj->base);
	mutex_unlock(&dev->struct_mutex);
out:
	return ret;
}

static int intel_fb_find_or_create_single(struct drm_fb_helper *helper,
					  struct drm_fb_helper_surface_size *sizes)
{
	struct intel_fbdev *ifbdev = (struct intel_fbdev *)helper;
	int new_fb = 0;
	int ret;

	if (!helper->fb) {
		ret = intelfb_create(ifbdev, sizes);
		if (ret)
			return ret;
		new_fb = 1;
	}
	return new_fb;
}

static struct drm_fb_helper_funcs intel_fb_helper_funcs = {
	.gamma_set = intel_crtc_fb_gamma_set,
	.gamma_get = intel_crtc_fb_gamma_get,
	.fb_probe = intel_fb_find_or_create_single,
};

static void intel_fbdev_destroy(struct drm_device *dev,
				struct intel_fbdev *ifbdev)
{
#if 0
	struct fb_info *info;
#endif
	struct intel_framebuffer *ifb = &ifbdev->ifb;

#if 0
	if (ifbdev->helper.fbdev) {
		info = ifbdev->helper.fbdev;
		unregister_framebuffer(info);
		iounmap(info->screen_base);
		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}
#endif

	drm_fb_helper_fini(&ifbdev->helper);

	drm_framebuffer_cleanup(&ifb->base);
	if (ifb->obj) {
		drm_gem_object_unreference_unlocked(&ifb->obj->base);
		ifb->obj = NULL;
	}
}

int intel_fbdev_init(struct drm_device *dev)
{
	struct intel_fbdev *ifbdev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	ifbdev = kzalloc(sizeof(struct intel_fbdev), GFP_KERNEL);
	if (!ifbdev)
		return -ENOMEM;

	dev_priv->fbdev = ifbdev;
	ifbdev->helper.funcs = &intel_fb_helper_funcs;

	ret = drm_fb_helper_init(dev, &ifbdev->helper,
				 INTEL_INFO(dev)->num_pipes,
				 INTELFB_CONN_LIMIT);
	if (ret) {
		kfree(ifbdev);
		return ret;
	}

	drm_fb_helper_single_add_all_connectors(&ifbdev->helper);
	drm_fb_helper_initial_config(&ifbdev->helper, 32);
	return 0;
}

void intel_fbdev_fini(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	if (!dev_priv->fbdev)
		return;

	intel_fbdev_destroy(dev, dev_priv->fbdev);
	kfree(dev_priv->fbdev);
	dev_priv->fbdev = NULL;
}

void intel_fb_output_poll_changed(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_fb_helper_hotplug_event(&dev_priv->fbdev->helper);
}

void intel_fb_restore_mode(struct drm_device *dev)
{
	int ret;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_plane *plane;

	rw_enter_write(&dev->mode_config.rwl);

	ret = drm_fb_helper_restore_fbdev_mode(&dev_priv->fbdev->helper);
	if (ret)
		DRM_DEBUG("failed to restore crtc mode\n");

	/* Be sure to shut off any planes that may be active */
	list_for_each_entry(plane, &config->plane_list, head)
		plane->funcs->disable_plane(plane);

	rw_exit_write(&dev->mode_config.rwl);
}
