/*	$OpenBSD: xfs_extern.h,v 1.1 1998/08/30 16:47:21 art Exp $	*/
#ifndef _XFS_XFS_EXTERN_H
#define _XFS_XFS_EXTERN_H

#ifdef _KERNEL

int xfs_devopen __P((dev_t dev, int flags, int devtype, struct proc * p));
int xfs_devclose __P((dev_t dev, int flags, int devtype, struct proc * p));
int xfs_devread __P((dev_t dev, struct uio * uiop, int ioflag));
int xfs_devwrite __P((dev_t dev, struct uio *uiop, int ioflag));
int xfs_devioctl __P((dev_t dev, u_long cmd, caddr_t data, int flags,
		      struct proc * p));
int xfs_devselect __P((dev_t dev, int which, struct proc * p));

#endif /* _KERNEL */

#endif
