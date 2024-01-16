/* Public domain. */

#ifndef _DRM_GEM_FRAMEBUFFER_HELPER_H
#define _DRM_GEM_FRAMEBUFFER_HELPER_H

struct drm_framebuffer;
struct drm_file;

struct drm_framebuffer *drm_gem_fb_create(struct drm_device *,
    struct drm_file *, const struct drm_mode_fb_cmd2 *);
void drm_gem_fb_destroy(struct drm_framebuffer *);
int drm_gem_fb_create_handle(struct drm_framebuffer *, struct drm_file *,
    unsigned int *);
struct drm_gem_object *drm_gem_fb_get_obj(struct drm_framebuffer *,
    unsigned int);

#endif
