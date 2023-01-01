/* Public domain. */

#ifndef _LINUX_IOSYS_MAP_H
#define _LINUX_IOSYS_MAP_H

#include <linux/io.h>
#include <linux/string.h>

struct iosys_map {
	union {
		void *vaddr_iomem;
		void *vaddr;
	};
	bool is_iomem;
	bus_space_handle_t bsh;
	bus_size_t size;
};

static inline void
iosys_map_incr(struct iosys_map *ism, size_t n)
{
	if (ism->is_iomem)
		ism->vaddr_iomem += n;
	else
		ism->vaddr += n;
}

static inline void
iosys_map_memcpy_to(struct iosys_map *ism, size_t off, const void *src,
    size_t len)
{
	if (ism->is_iomem)
		memcpy_toio(ism->vaddr_iomem + off, src, len);
	else
		memcpy(ism->vaddr + off, src, len);
}

static inline bool
iosys_map_is_null(const struct iosys_map *ism)
{
	if (ism->is_iomem)
		return (ism->vaddr_iomem == NULL);
	else
		return (ism->vaddr == NULL);
}

static inline bool
iosys_map_is_set(const struct iosys_map *ism)
{
	if (ism->is_iomem)
		return (ism->vaddr_iomem != NULL);
	else
		return (ism->vaddr != NULL);
}

static inline void
iosys_map_clear(struct iosys_map *ism)
{
	if (ism->is_iomem) {
		ism->vaddr_iomem = NULL;
		ism->is_iomem = false;
	} else {
		ism->vaddr = NULL;
	}
}

static inline void
iosys_map_set_vaddr_iomem(struct iosys_map *ism, void *addr)
{
	ism->vaddr_iomem = addr;
	ism->is_iomem = true;
}

static inline void
iosys_map_set_vaddr(struct iosys_map *ism, void *addr)
{
	ism->vaddr = addr;
	ism->is_iomem = false;
}

#endif
