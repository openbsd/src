/* Public domain. */

#include <drm/drm_plane.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_gem.h>
#include <linux/dma-resv.h>

int
drm_gem_plane_helper_prepare_fb(struct drm_plane *dp,
    struct drm_plane_state *dps)
{
	if (dps->fb != NULL) {
		struct drm_gem_object *obj = dps->fb->obj[0];
		drm_atomic_set_fence_for_plane(dps,
		    dma_resv_get_excl_unlocked(obj->resv));
		
	}

	return 0;
}
