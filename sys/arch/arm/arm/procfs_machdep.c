/*	$OpenBSD: procfs_machdep.c,v 1.1 2004/02/01 05:09:48 drahn Exp $	*/
/*	$NetBSD: procfs_machdep.c,v 1.2 2003/07/15 00:24:39 lukem Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>


#if 0
/*
 * Linux-style /proc/cpuinfo.
 * Only used when procfs is mounted with -o linux.
 */
int
procfs_getcpuinfstr(char *buf, int *len)
{
	*len = 0;

	return 0;
}
#endif
