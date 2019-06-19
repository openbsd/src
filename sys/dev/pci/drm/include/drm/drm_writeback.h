/* Public domain. */

#ifndef DRM_WRITEBACK_H
#define DRM_WRITEBACK_H

#include <drm/drm_connector.h>

struct drm_writeback_connector {
	struct drm_connector base;
};

struct drm_writeback_job {
	struct dma_fence *out_fence;
	struct drm_framebuffer *fb;
};

static inline struct drm_writeback_connector *
drm_connector_to_writeback(struct drm_connector *connector)
{
	return container_of(connector, struct drm_writeback_connector, base);
}

static inline struct dma_fence *
drm_writeback_get_out_fence(struct drm_writeback_connector *connector)
{
	return NULL;
}

#endif
