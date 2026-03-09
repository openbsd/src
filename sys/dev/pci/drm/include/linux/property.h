/* Public domain. */

#ifndef _LINUX_PROPERTY_H
#define _LINUX_PROPERTY_H

#include <linux/fwnode.h>

static inline void
fwnode_handle_put(struct fwnode_handle *h)
{
}

static inline const struct fwnode_handle *
dev_fwnode(struct device *d)
{
	return NULL;
}

#endif
