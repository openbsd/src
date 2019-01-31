/* Public domain. */

struct drm_device;

void
i915_check_vgpu(struct drm_device *dev)
{
}

int intel_vgt_balloon(struct drm_device *dev)
{
	return 0;
}

void intel_vgt_deballoon(void)
{
}
