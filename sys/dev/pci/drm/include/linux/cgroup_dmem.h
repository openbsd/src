/* Public domain. */

#ifndef _LINUX_CGROUP_DMEM_H
#define _LINUX_CGROUP_DMEM_H

struct dmem_cgroup_pool_state;

static inline void *
dmem_cgroup_register_region(uint64_t size, const char *fmt, ...)
{
	return NULL;
}

static inline void
dmem_cgroup_unregister_region(void *r)
{
}

static inline bool
dmem_cgroup_state_evict_valuable(struct dmem_cgroup_pool_state *l,
    struct dmem_cgroup_pool_state *t, bool a, bool *b)
{
	return true;
}

static inline void
dmem_cgroup_pool_state_put(struct dmem_cgroup_pool_state *p)
{
}

static inline int
dmem_cgroup_try_charge(void *a, uint64_t size,
    struct dmem_cgroup_pool_state **r, struct dmem_cgroup_pool_state **p)
{
	if (r)
		*r = NULL;
	if (p)
		*p = NULL;
	return 0;
}

static inline void
dmem_cgroup_uncharge(struct dmem_cgroup_pool_state *p, uint64_t size)
{
}

#endif
