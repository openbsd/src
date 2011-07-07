/*	$OpenBSD: diskmap.c,v 1.6 2011/07/07 23:45:00 matthew Exp $	*/

/*
 * Copyright (c) 2009, 2010 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Disk mapper.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>

int
diskmapopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	return 0;
}

int
diskmapclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	return 0;
}

int
diskmapioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct dk_diskmap *dm;
	struct nameidata ndp;
	struct filedesc *fdp;
	struct file *fp = NULL;
	struct vnode *vp = NULL, *ovp;
	char *devname;
	int fd, error = EINVAL;

	if (cmd != DIOCMAP)
		return EINVAL;

	/*
	 * Map a request for a disk to the correct device. We should be
	 * supplied with either a diskname or a disklabel UID.
	 */

	dm = (struct dk_diskmap *)addr;
	fd = dm->fd;
	devname = malloc(PATH_MAX, M_DEVBUF, M_WAITOK);
	if (copyinstr(dm->device, devname, PATH_MAX, NULL))
		goto invalid;
	if (disk_map(devname, devname, PATH_MAX, dm->flags) == 0)
		if (copyoutstr(devname, dm->device, PATH_MAX, NULL))
			goto invalid;

	/* Attempt to open actual device. */
	fdp = p->p_fd;
	fdplock(fdp);

	if ((error = getvnode(fdp, fd, &fp)) != 0)
		goto bad;

	ndp.ni_segflg = UIO_SYSSPACE;
	ndp.ni_dirfd = AT_FDCWD;
	ndp.ni_dirp = devname;
	ndp.ni_cnd.cn_proc = p;
	if ((error = vn_open(&ndp, fp->f_flag, 0)) != 0)
		goto bad;

	vp = ndp.ni_vp;

	/* Close the original vnode. */
	ovp = (struct vnode *)fp->f_data;
	if (fp->f_flag & FWRITE)
		ovp->v_writecount--;

	if (ovp->v_writecount == 0) {
		vn_lock(ovp, LK_EXCLUSIVE | LK_RETRY, p);
		VOP_CLOSE(ovp, fp->f_flag, p->p_ucred, p);
		vput(ovp);
	}

	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)vp;
	fp->f_offset = 0;
	fp->f_rxfer = 0;
	fp->f_wxfer = 0;
	fp->f_seek = 0;
	fp->f_rbytes = 0;
	fp->f_wbytes = 0;

	VOP_UNLOCK(vp, 0, p);

	FRELE(fp);
	fdpunlock(fdp);
	free(devname, M_DEVBUF);

	return 0;

bad:
	if (vp)
		vput(vp);
	if (fp)
		FRELE(fp);

	fdpunlock(fdp);

invalid:
	free(devname, M_DEVBUF);

	return (error);
}

int
diskmapread(dev_t dev, struct uio *uio, int flag)
{
	return ENXIO;
}

int
diskmapwrite(dev_t dev, struct uio *uio, int flag)
{
	return ENXIO;
}
