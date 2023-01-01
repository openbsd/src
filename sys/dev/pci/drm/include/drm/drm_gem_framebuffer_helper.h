/* Public domain. */

#ifndef _DRM_GEM_FRAMEBUFFER_HELPER_H
#define _DRM_GEM_FRAMEBUFFER_HELPER_H

struct drm_framebuffer;
struct drm_file;

void drm_gem_fb_destroy(struct drm_framebuffer *);
int drm_gem_fb_create_handle(struct drm_framebuffer *, struct drm_file *,
    unsigned int *);

#endif
