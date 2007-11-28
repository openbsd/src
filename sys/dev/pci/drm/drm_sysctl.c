/*-
 * Copyright 2003 Eric Anholt
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * ERIC ANHOLT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** @file drm_sysctl.c
 * Implementation of various sysctls for controlling DRM behavior and reporting
 * debug information.
 */

#include "drmP.h"
#include "drm.h"

#include <sys/sysctl.h>

static int	   drm_name_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_vm_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_clients_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_bufs_info DRM_SYSCTL_HANDLER_ARGS;

struct drm_sysctl_list {
	const char *name;
	int	   (*f) DRM_SYSCTL_HANDLER_ARGS;
} drm_sysctl_list[] = {
	{"name",    drm_name_info},
	{"vm",	    drm_vm_info},
	{"clients", drm_clients_info},
	{"bufs",    drm_bufs_info},
};
#define DRM_SYSCTL_ENTRIES (sizeof(drm_sysctl_list)/sizeof(drm_sysctl_list[0]))

struct drm_sysctl_info {
#ifdef __FreeBSD__
	struct sysctl_ctx_list ctx;
#elif defined __NetBSD__
	const struct sysctlnode *dri, *dri_card, *dri_debug;
	const struct sysctlnode *dri_rest[DRM_SYSCTL_ENTRIES];
#endif
	char		       name[2];
};

int drm_sysctl_init(drm_device_t *dev)
{
#ifndef __OpenBSD__
	struct drm_sysctl_info *info;
#ifdef __FreeBSD__
	struct sysctl_oid *oid;
	struct sysctl_oid *top, *drioid;
#endif
	int		  i;

	info = malloc(sizeof *info, M_DRM, M_WAITOK | M_ZERO);
	if ( !info )
		return 1;
	dev->sysctl = info;

#ifdef __FreeBSD__
	/* Add the sysctl node for DRI if it doesn't already exist */
	drioid = SYSCTL_ADD_NODE( &info->ctx, &sysctl__hw_children, OID_AUTO, "dri", CTLFLAG_RW, NULL, "DRI Graphics");
	if (!drioid)
		return 1;

	/* Find the next free slot under hw.dri */
	i = 0;
	SLIST_FOREACH(oid, SYSCTL_CHILDREN(drioid), oid_link) {
		if (i <= oid->oid_arg2)
			i = oid->oid_arg2 + 1;
	}
	if (i>9)
		return 1;
	
	/* Add the hw.dri.x for our device */
	info->name[0] = '0' + i;
	info->name[1] = 0;
	top = SYSCTL_ADD_NODE( &info->ctx, SYSCTL_CHILDREN(drioid), OID_AUTO, info->name, CTLFLAG_RW, NULL, NULL);
	if (!top)
		return 1;
	
	for (i = 0; i < DRM_SYSCTL_ENTRIES; i++) {
		oid = SYSCTL_ADD_OID(&info->ctx, 
			SYSCTL_CHILDREN(top), 
			OID_AUTO, 
			drm_sysctl_list[i].name, 
			CTLTYPE_INT | CTLFLAG_RD, 
			dev, 
			0, 
			drm_sysctl_list[i].f, 
			"A", 
			NULL);
		if (!oid)
			return 1;
	}
	SYSCTL_ADD_INT(&info->ctx, SYSCTL_CHILDREN(top), OID_AUTO, "debug",
	    CTLFLAG_RW, &drm_debug_flag, sizeof(drm_debug_flag),
	    "Enable debugging output");
#elif defined(__NetBSD__)
	sysctl_createv(NULL, 0, NULL, &info->dri,
			CTLFLAG_READWRITE, CTLTYPE_NODE,
			"dri", SYSCTL_DESCR("DRI Graphics"), NULL, 0, NULL, 0,
			CTL_HW, CTL_CREATE);
	snprintf(info->name, 7, "card%d", dev->unit);
	sysctl_createv(NULL, 0, NULL, &info->dri_card,
			CTLFLAG_READWRITE, CTLTYPE_NODE,
			info->name, NULL, NULL, 0, NULL, 0,
			CTL_HW, info->dri->sysctl_num, CTL_CREATE);
	for (i = 0; i < DRM_SYSCTL_ENTRIES; i++)
		sysctl_createv(NULL, 0, NULL, &(info->dri_rest[i]),
				CTLFLAG_READONLY, CTLTYPE_STRING,
				drm_sysctl_list[i].name, NULL,
				drm_sysctl_list[i].f, 0, dev,
				sizeof(drm_device_t*),
				CTL_HW,
				info->dri->sysctl_num,
				info->dri_card->sysctl_num, CTL_CREATE);
	sysctl_createv(NULL, 0, NULL, &info->dri_debug,
			CTLFLAG_READWRITE, CTLTYPE_INT,
			"debug", SYSCTL_DESCR("Enable debugging output"),
			NULL, 0,
			&drm_debug_flag, sizeof(drm_debug_flag),
			CTL_HW, info->dri->sysctl_num, CTL_CREATE);
#endif
#endif /* !__OpenBSD__ */

	return 0;
}

int drm_sysctl_cleanup(drm_device_t *dev)
{
	int error=0;
#ifdef __FreeBSD__
	error = sysctl_ctx_free( &dev->sysctl->ctx );
#elif defined(__NetBSD__)
	int i;

	sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num,
	                              dev->sysctl->dri_debug->sysctl_num,
	                              CTL_DESTROY);
	for (i = 0; i < DRM_SYSCTL_ENTRIES; i++)
		sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num,
	       	                              dev->sysctl->dri_card->sysctl_num,
	       	                           dev->sysctl->dri_rest[i]->sysctl_num,
		                              CTL_DESTROY);
	sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num,
	                              dev->sysctl->dri_card->sysctl_num,
	                              CTL_DESTROY);
	sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num, CTL_DESTROY);
#endif

	if(dev->sysctl)
		free(dev->sysctl, M_DRM);
	dev->sysctl = NULL;

	return error;
}

#ifdef __NetBSD__
#define SYSCTL_OUT(x, y, z) \
	(len+=z,(len<*oldlenp)?(strcat((char*)oldp, y),0):EOVERFLOW)
#endif

#define DRM_SYSCTL_PRINT(fmt, arg...)				\
do {								\
	snprintf(buf, sizeof(buf), fmt, ##arg);			\
	retcode = SYSCTL_OUT(req, buf, strlen(buf));		\
	if (retcode)						\
		goto done;					\
} while (0)

static int drm_name_info DRM_SYSCTL_HANDLER_ARGS
{
#ifndef __OpenBSD__
#ifdef __FreeBSD__
	drm_device_t *dev = arg1;
#elif defined(__NetBSD__)
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
#endif
	char buf[128];
	int retcode;
	int hasunique = 0;

#ifdef __NetBSD__
	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';
#endif

#ifdef __FreeBSD__
	DRM_SYSCTL_PRINT("%s 0x%x", dev->driver.name, dev2udev(dev->devnode));
#elif defined(__NetBSD__)
	DRM_SYSCTL_PRINT("%s", dev->driver.name);
#endif
	
	DRM_LOCK();
	if (dev->unique) {
		snprintf(buf, sizeof(buf), " %s", dev->unique);
		hasunique = 1;
	}
	DRM_UNLOCK();
	
	if (hasunique)
		SYSCTL_OUT(req, buf, strlen(buf));

	SYSCTL_OUT(req, "", 1);

done:
	return retcode;
#endif /* !__OpenBSD__ */
	return 0;
}

static int drm_vm_info DRM_SYSCTL_HANDLER_ARGS
{
#ifndef __OpenBSD__
#ifdef __FreeBSD__
	drm_device_t *dev = arg1;
#elif defined(__NetBSD__)
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
#endif
	drm_local_map_t *map, *tempmaps;
	const char   *types[] = { "FB", "REG", "SHM", "AGP", "SG" };
	const char *type, *yesno;
	int i, mapcount;
	char buf[128];
	int retcode;

#ifdef __NetBSD__
	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';
#endif

	/* We can't hold the lock while doing SYSCTL_OUTs, so allocate a
	 * temporary copy of all the map entries and then SYSCTL_OUT that.
	 */
	DRM_LOCK();

	mapcount = 0;
	TAILQ_FOREACH(map, &dev->maplist, link)
		mapcount++;

	tempmaps = malloc(sizeof(drm_local_map_t) * mapcount, M_DRM, M_NOWAIT);
	if (tempmaps == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}

	i = 0;
	TAILQ_FOREACH(map, &dev->maplist, link)
		tempmaps[i++] = *map;

	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\nslot	 offset	      size type flags	 "
			 "address mtrr\n");

	for (i = 0; i < mapcount; i++) {
		map = &tempmaps[i];

		if (map->type < 0 || map->type > 4)
			type = "??";
		else
			type = types[map->type];

		if (!map->mtrr)
			yesno = "no";
		else
			yesno = "yes";

		DRM_SYSCTL_PRINT(
		    "%4d 0x%08lx 0x%08lx %4.4s  0x%02x 0x%08lx %s\n", i,
		    map->offset, map->size, type, map->flags,
		    (unsigned long)map->handle, yesno);
	}
	SYSCTL_OUT(req, "", 1);

done:
	free(tempmaps, M_DRM);
	return retcode;
#endif
	return 0;
}

static int drm_bufs_info DRM_SYSCTL_HANDLER_ARGS
{
#ifndef __OpenBSD__
#ifdef __FreeBSD__
	drm_device_t	 *dev = arg1;
#elif defined(__NetBSD__)
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
#endif
	drm_device_dma_t *dma = dev->dma;
	drm_device_dma_t tempdma;
	int *templists;
	int i;
	char buf[128];
	int retcode;

#ifdef __NetBSD__
	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';
#endif

	/* We can't hold the locks around DRM_SYSCTL_PRINT, so make a temporary
	 * copy of the whole structure and the relevant data from buflist.
	 */
	DRM_LOCK();
	if (dma == NULL) {
		DRM_UNLOCK();
		return 0;
	}
	DRM_SPINLOCK(&dev->dma_lock);
	tempdma = *dma;
	templists = malloc(sizeof(int) * dma->buf_count, M_DRM, M_NOWAIT);
	for (i = 0; i < dma->buf_count; i++)
		templists[i] = dma->buflist[i]->list;
	dma = &tempdma;
	DRM_SPINUNLOCK(&dev->dma_lock);
	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\n o     size count  free	 segs pages    kB\n");
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
		if (dma->bufs[i].buf_count)
			DRM_SYSCTL_PRINT("%2d %8d %5d %5d %5d %5d %5d\n",
				       i,
				       dma->bufs[i].buf_size,
				       dma->bufs[i].buf_count,
				       atomic_read(&dma->bufs[i]
						   .freelist.count),
				       dma->bufs[i].seg_count,
				       dma->bufs[i].seg_count
				       *(1 << dma->bufs[i].page_order),
				       (dma->bufs[i].seg_count
					* (1 << dma->bufs[i].page_order))
				       * PAGE_SIZE / 1024);
	}
	DRM_SYSCTL_PRINT("\n");
	for (i = 0; i < dma->buf_count; i++) {
		if (i && !(i%32)) DRM_SYSCTL_PRINT("\n");
		DRM_SYSCTL_PRINT(" %d", templists[i]);
	}
	DRM_SYSCTL_PRINT("\n");

	SYSCTL_OUT(req, "", 1);
done:
	free(templists, M_DRM);
	return retcode;
#endif
return 0;
}

static int drm_clients_info DRM_SYSCTL_HANDLER_ARGS
{
#ifndef __OpenBSD__
#ifdef __FreeBSD__
	drm_device_t *dev = arg1;
#elif defined(__NetBSD__)
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
#endif
	drm_file_t *priv, *tempprivs;
	char buf[128];
	int retcode;
	int privcount, i;

#ifdef __NetBSD__
	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';
#endif

	DRM_LOCK();

	privcount = 0;
	TAILQ_FOREACH(priv, &dev->files, link)
		privcount++;

	tempprivs = malloc(sizeof(drm_file_t) * privcount, M_DRM, M_NOWAIT);
	if (tempprivs == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}
	i = 0;
	TAILQ_FOREACH(priv, &dev->files, link)
		tempprivs[i++] = *priv;

	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\na dev	pid    uid	magic	  ioctls\n");
	for (i = 0; i < privcount; i++) {
		priv = &tempprivs[i];
		DRM_SYSCTL_PRINT("%c %3d %5d %5d %10u %10lu\n",
			       priv->authenticated ? 'y' : 'n',
			       priv->minor,
			       priv->pid,
			       priv->uid,
			       priv->magic,
			       priv->ioctl_count);
	}

	SYSCTL_OUT(req, "", 1);
done:
	free(tempprivs, M_DRM);
	return retcode;
#endif
	return 0;
}
