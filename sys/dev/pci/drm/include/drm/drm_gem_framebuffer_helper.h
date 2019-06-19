/* Public domain. */

#ifndef DRM_GEM_FRAMEBUFFER_HELPER_H
#define DRM_GEM_FRAMEBUFFER_HELPER_H

void	drm_gem_fb_destroy(struct drm_framebuffer *);
int	drm_gem_fb_create_handle(struct drm_framebuffer *, struct drm_file *,
	    unsigned int *);

#endif
