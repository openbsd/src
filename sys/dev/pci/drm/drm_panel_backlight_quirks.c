/* Public domain. */

#include <linux/err.h>
#include <drm/drm_edid.h>

const struct drm_panel_backlight_quirk *
drm_get_panel_backlight_quirk(const struct drm_edid *edid)
{
	return ERR_PTR(-ENOSYS);
}
