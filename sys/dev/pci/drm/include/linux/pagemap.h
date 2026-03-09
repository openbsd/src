/* Public domain. */

#ifndef _LINUX_PAGEMAP_H
#define _LINUX_PAGEMAP_H

#include <linux/uaccess.h>
#include <linux/highmem.h>

struct address_space;

static inline void
mapping_clear_unevictable(struct address_space *as)
{
}

#endif
