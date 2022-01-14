/* Public domain. */

#ifndef _LINUX_DMA_BUF_MAP_H
#define _LINUX_DMA_BUF_MAP_H

#include <linux/io.h>
#include <linux/string.h>

struct dma_buf_map {
	union {
		void *vaddr_iomem;
		void *vaddr;
	};
	bool is_iomem;
	bus_space_handle_t bsh;
	bus_size_t size;
};

static inline void
dma_buf_map_incr(struct dma_buf_map *dbm, size_t n)
{
	if (dbm->is_iomem)
		dbm->vaddr_iomem += n;
	else
		dbm->vaddr += n;
}

static inline void
dma_buf_map_memcpy_to(struct dma_buf_map *dbm, const void *src, size_t len)
{
	if (dbm->is_iomem)
		memcpy_toio(dbm->vaddr_iomem, src, len);
	else
		memcpy(dbm->vaddr, src, len);
}

static inline bool
dma_buf_map_is_null(const struct dma_buf_map *dbm)
{
	if (dbm->is_iomem)
		return (dbm->vaddr_iomem == NULL);
	else
		return (dbm->vaddr == NULL);
}

static inline bool
dma_buf_map_is_set(const struct dma_buf_map *dbm)
{
	if (dbm->is_iomem)
		return (dbm->vaddr_iomem != NULL);
	else
		return (dbm->vaddr != NULL);
}

static inline void
dma_buf_map_clear(struct dma_buf_map *dbm)
{
	if (dbm->is_iomem) {
		dbm->vaddr_iomem = NULL;
		dbm->is_iomem = false;
	} else {
		dbm->vaddr = NULL;
	}
}

static inline void
dma_buf_map_set_vaddr_iomem(struct dma_buf_map *dbm, void *addr)
{
	dbm->vaddr_iomem = addr;
	dbm->is_iomem = true;
}

static inline void
dma_buf_map_set_vaddr(struct dma_buf_map *dbm, void *addr)
{
	dbm->vaddr = addr;
	dbm->is_iomem = false;
}

#endif
