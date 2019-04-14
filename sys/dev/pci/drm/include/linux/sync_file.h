/* Public domain. */

#ifndef _LINUX_SYNC_FILE_H
#define _LINUX_SYNC_FILE_H

#include <linux/dma-fence.h>
#include <linux/ktime.h>

struct sync_file {
};

static inline struct dma_fence *
sync_file_get_fence(int fd)
{
	printf("%s: stub\n", __func__);
	return NULL;
}

static inline struct sync_file *
sync_file_create(struct dma_fence *fence)
{
	printf("%s: stub\n", __func__);
	return NULL;
}

#endif
