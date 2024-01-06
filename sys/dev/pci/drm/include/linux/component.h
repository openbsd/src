/* Public domain. */

#ifndef _LINUX_COMPONENT_H
#define _LINUX_COMPONENT_H

#include <sys/_null.h>

struct component_match;
struct device;

struct component_ops {
	int (*bind)(struct device *, struct device *, void *);
	void (*unbind)(struct device *, struct device *, void *);
};

struct component_master_ops {
	int (*bind)(struct device *);
	void (*unbind)(struct device *);
};

#define component_del(a, b)
#define component_add(a, b)	0

static inline int
component_bind_all(struct device *dev, void *data)
{
	return 0;
}

#define component_unbind_all(a, b)

int	component_compare_of(struct device *, void *);
int	component_master_add_with_match(struct device *,
	    const struct component_master_ops *, struct component_match *);

#endif
