/* Public domain. */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>

bool
drm_bridge_mode_fixup(struct drm_bridge *bridge,
    const struct drm_display_mode *mode,
    struct drm_display_mode *adjusted_mode)
{
	return true;
}

enum drm_mode_status
drm_bridge_mode_valid(struct drm_bridge *bridge,
    const struct drm_display_mode *mode)
{
	return MODE_OK;
}

void
drm_bridge_mode_set(struct drm_bridge *bridge, struct drm_display_mode *mode,
    struct drm_display_mode *adjusted_mode)
{
}

void
drm_bridge_pre_enable(struct drm_bridge *bridge)
{
}

void
drm_bridge_enable(struct drm_bridge *bridge)
{
}


void
drm_bridge_disable(struct drm_bridge *bridge)
{
}

void
drm_bridge_post_disable(struct drm_bridge *bridge)
{
}

void
drm_bridge_detach(struct drm_bridge *bridge)
{
}
