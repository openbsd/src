/*
 * ismounted.c --- Check to see if the filesystem was mounted
 * 
 * Copyright (C) 1995 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>
#endif
#ifdef HAVE_LINUX_FD_H
#include <linux/fd.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#ifdef HAVE_GETMNTINFO
#include <paths.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif /* HAVE_GETMNTINFO */

#include <linux/ext2_fs.h>
#include "ext2fs.h"

#ifdef HAVE_MNTENT_H
/*
 * XXX we only check to see if the mount is readonly when it's the
 * root filesystem.
 */
static errcode_t check_mntent(const char *file, int *mount_flags)
{
	FILE * f;
	struct mntent * mnt;
	int	fd;

	*mount_flags = 0;
	if ((f = setmntent (MOUNTED, "r")) == NULL)
		return errno;
	while ((mnt = getmntent (f)) != NULL)
		if (strcmp(file, mnt->mnt_fsname) == 0)
			break;
	endmntent (f);
	if (mnt == 0)
		return 0;
	*mount_flags = EXT2_MF_MOUNTED;
	
	if (!strcmp(mnt->mnt_dir, "/")) {
		*mount_flags |= EXT2_MF_ISROOT;
		fd = open(MOUNTED, O_RDWR);
		if (fd < 0) {
			if (errno == EROFS)
				*mount_flags |= EXT2_MF_READONLY;
		} else
			close(fd);
	}
	return 0;
}
#endif

#ifdef HAVE_GETMNTINFO
static errcode_t check_getmntinfo(const char *file, int *mount_flags)
{
	struct statfs *mp;
        int    len, n;
        const  char   *s1;
	char	*s2;

        n = getmntinfo(&mp, MNT_NOWAIT);
        if (n == 0)
		return errno;

        len = sizeof(_PATH_DEV) - 1;
        s1 = file;
        if (strncmp(_PATH_DEV, s1, len) == 0)
                s1 += len;
 
	*mount_flags = 0;
        while (--n >= 0) {
                s2 = mp->f_mntfromname;
                if (strncmp(_PATH_DEV, s2, len) == 0) {
                        s2 += len - 1;
                        *s2 = 'r';
                }
                if (strcmp(s1, s2) == 0 || strcmp(s1, &s2[1]) == 0) {
			*mount_flags = EXT2_MF_MOUNTED;
			break;
		}
                ++mp;
	}
	return 0;
}
#endif /* HAVE_GETMNTINFO */

/*
 * Is_mounted is set to 1 if the device is mounted, 0 otherwise
 */
errcode_t ext2fs_check_if_mounted(const char *file, int *mount_flags)
{
#ifdef HAVE_MNTENT_H
	return check_mntent(file, mount_flags);
#else 
#ifdef HAVE_GETMNTINFO
	return check_getmntinfo(file, mount_flags);
#else
	*mount_flags = 0;
	return 0;
#endif /* HAVE_GETMNTINFO */
#endif /* HAVE_MNTENT_H */
}
