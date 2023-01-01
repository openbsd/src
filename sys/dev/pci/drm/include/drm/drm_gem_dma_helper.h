/* Public Domain */

#include <drm/drm_gem.h>

void drm_gem_dma_free_object(struct drm_gem_object *);
int drm_gem_dma_dumb_create(struct drm_file *, struct drm_device *,
    struct drm_mode_create_dumb *);
int drm_gem_dma_dumb_map_offset(struct drm_file *, struct drm_device *,
    uint32_t, uint64_t *);
struct drm_gem_dma_object *drm_gem_dma_create(struct drm_device *,
    size_t);

int drm_gem_dma_fault(struct drm_gem_object *, struct uvm_faultinfo *,
    off_t, vaddr_t, vm_page_t *, int, int, vm_prot_t, int);

struct sg_table *drm_gem_dma_get_sg_table(struct drm_gem_object *);

int drm_gem_dma_vmap(struct drm_gem_object *, struct iosys_map *);

struct drm_gem_dma_object {
	struct drm_gem_object	base;
	bus_dma_tag_t		dmat;
	bus_dmamap_t		dmamap;
	bus_dma_segment_t	dmasegs[1];
	size_t			dmasize;
	caddr_t			vaddr;
	struct sg_table		*sgt;
};

#define to_drm_gem_dma_obj(gem_obj) container_of(gem_obj, struct drm_gem_dma_object, base)
