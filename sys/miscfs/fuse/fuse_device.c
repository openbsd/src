/* $OpenBSD: fuse_device.c,v 1.51 2026/06/20 13:45:13 helg Exp $ */
/*
 * Copyright (c) 2012-2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/event.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/refcnt.h>
#include <sys/rwlock.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vnode.h>
#include <sys/fusebuf.h>

#include "fusefs_node.h"
#include "fusefs.h"

/*
 * Locks used to protect struct members and global data
 *	l	fd_lock
 */

SIMPLEQ_HEAD(fusebuf_head, fusebuf);

struct fuse_d {
	struct rwlock fd_lock;
	struct refcnt fd_refcnt;
	struct fusefs_mnt *fd_fmp;
	int fd_unit;

	/*fusebufs queues*/
	struct fusebuf_head fd_fbufs_in;	/* [l] */
	struct fusebuf_head fd_fbufs_wait;

	/* kq fields */
	struct klist fd_rklist;			/* [l] */
	LIST_ENTRY(fuse_d) fd_list;
};

int stat_fbufs_in = 0;
int stat_fbufs_wait = 0;
int stat_opened_fusedev = 0;

LIST_HEAD(, fuse_d) fuse_d_list;
struct fuse_d *fuse_lookup(int);

void	fuseattach(int);
int	fuseopen(dev_t, int, int, struct proc *);
int	fuseclose(dev_t, int, int, struct proc *);
int	fuseread(dev_t, struct uio *, int);
int	fusewrite(dev_t, struct uio *, int);
int	fusekqfilter(dev_t dev, struct knote *kn);
int	filt_fuse_read(struct knote *, long);
void	filt_fuse_rdetach(struct knote *);
int	filt_fuse_modify(struct kevent *, struct knote *);
int	filt_fuse_process(struct knote *, struct kevent *);

static const struct filterops fuse_rd_filtops = {
	.f_flags	= FILTEROP_ISFD | FILTEROP_MPSAFE,
	.f_attach	= NULL,
	.f_detach	= filt_fuse_rdetach,
	.f_event	= filt_fuse_read,
	.f_modify	= filt_fuse_modify,
	.f_process	= filt_fuse_process,
};

struct fuse_d *
fuse_lookup(int unit)
{
	struct fuse_d *fd;

	LIST_FOREACH(fd, &fuse_d_list, fd_list)
		if (fd->fd_unit == unit) {
			refcnt_take(&fd->fd_refcnt);
			return (fd);
		}
	return (NULL);
}

/*
 * Cleanup all msgs from sc_fbufs_in and sc_fbufs_wait.
 */
void
fuse_device_cleanup(dev_t dev)
{
	struct fuse_d *fd;
	struct fusebuf *f, *ftmp, *lprev;

	fd = fuse_lookup(minor(dev));
	if (fd == NULL)
		return;

	/* clear FIFO IN */
	lprev = NULL;
	rw_enter_write(&fd->fd_lock);
	SIMPLEQ_FOREACH_SAFE(f, &fd->fd_fbufs_in, fb_next, ftmp) {
		DPRINTF("cleanup unprocessed msg in sc_fbufs_in\n");
		if (lprev == NULL)
			SIMPLEQ_REMOVE_HEAD(&fd->fd_fbufs_in, fb_next);
		else
			SIMPLEQ_REMOVE_AFTER(&fd->fd_fbufs_in, lprev,
			    fb_next);

		stat_fbufs_in--;
		f->fb_err = ENXIO;
		/* Wakeup up VFS syscall waiting on this fbuf, it will fail */
		wakeup(f);
		lprev = f;
	}
	knote_locked(&fd->fd_rklist, 0);
	rw_exit_write(&fd->fd_lock);

	/* clear FIFO WAIT*/
	lprev = NULL;
	SIMPLEQ_FOREACH_SAFE(f, &fd->fd_fbufs_wait, fb_next, ftmp) {
		DPRINTF("umount unprocessed msg in sc_fbufs_wait\n");
		if (lprev == NULL)
			SIMPLEQ_REMOVE_HEAD(&fd->fd_fbufs_wait, fb_next);
		else
			SIMPLEQ_REMOVE_AFTER(&fd->fd_fbufs_wait, lprev,
			    fb_next);

		stat_fbufs_wait--;
		f->fb_err = ENXIO;
		/* Wakeup up VFS syscall waiting on this fbuf, it will fail */
		wakeup(f);
		lprev = f;
	}

	refcnt_rele_wake(&fd->fd_refcnt);
}

void
fuse_device_queue_fbuf(dev_t dev, struct fusebuf *fbuf)
{
	struct fuse_d *fd;

	fd = fuse_lookup(minor(dev));
	if (fd == NULL)
		return;

	rw_enter_write(&fd->fd_lock);
	SIMPLEQ_INSERT_TAIL(&fd->fd_fbufs_in, fbuf, fb_next);
	knote_locked(&fd->fd_rklist, 0);
	rw_exit_write(&fd->fd_lock);
	stat_fbufs_in++;

	/* Let file system daemons know there is a request ready to process */
	wakeup_one(&fd->fd_fbufs_in);

	refcnt_rele_wake(&fd->fd_refcnt);
}

void
fuse_device_set_fmp(struct fusefs_mnt *fmp, int set)
{
	struct fuse_d *fd;

	fd = fuse_lookup(minor(fmp->dev));
	if (fd == NULL)
		return;

	if (set)
		fd->fd_fmp = fmp;
	else {
		fd->fd_fmp = NULL;

		/* Let file system daemons know the device is dead */
		wakeup(&fd->fd_fbufs_in);
	}

	refcnt_rele_wake(&fd->fd_refcnt);
}

void
fuseattach(int num)
{
	LIST_INIT(&fuse_d_list);
}

int
fuseopen(dev_t dev, int flags, int fmt, struct proc * p)
{
	struct fuse_d *fd;
	int unit = minor(dev);

	if (flags & O_EXCL)
		return (EBUSY); /* No exclusive opens */

	if ((fd = fuse_lookup(unit)) != NULL) {
		refcnt_rele_wake(&fd->fd_refcnt);
		return (EBUSY);
	}

	fd = malloc(sizeof(*fd), M_DEVBUF, M_WAITOK | M_ZERO);
	fd->fd_unit = unit;
	SIMPLEQ_INIT(&fd->fd_fbufs_in);
	SIMPLEQ_INIT(&fd->fd_fbufs_wait);
	rw_init(&fd->fd_lock, "fusedlk");
	klist_init_rwlock(&fd->fd_rklist, &fd->fd_lock);
	refcnt_init(&fd->fd_refcnt);

	LIST_INSERT_HEAD(&fuse_d_list, fd, fd_list);

	stat_opened_fusedev++;
	return (0);
}

int
fuseclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct fuse_d *fd;

	fd = fuse_lookup(minor(dev));
	if (fd == NULL)
		return (EBADF);

	fuse_device_cleanup(dev);

	/*
	 * Let fusefs_unmount know the device is closed so it doesn't try and
	 * send FBT_DESTROY to a dead file system daemon.
	 */
	if (fd->fd_fmp) {
		fd->fd_fmp->sess_init = 0;
		fuse_device_set_fmp(fd->fd_fmp, 0);
	}

	LIST_REMOVE(fd, fd_list);

	refcnt_rele(&fd->fd_refcnt);
	refcnt_finalize(&fd->fd_refcnt, "fusedfd");
	free(fd, M_DEVBUF, sizeof(*fd));
	stat_opened_fusedev--;
	return (0);
}

int
fuseread(dev_t dev, struct uio *uio, int ioflag)
{
	struct fuse_d *fd;
	struct fusebuf *fbuf;
	int error = 0;

	fd = fuse_lookup(minor(dev));
	if (fd == NULL)
		return (ENODEV);

	if (fd->fd_fmp == NULL) {
		refcnt_rele(&fd->fd_refcnt);
		return (ENODEV);
	}

	rw_enter_write(&fd->fd_lock);

	/* Loop to avoid a race condition with multithreaded daemons. */
	fbuf = SIMPLEQ_FIRST(&fd->fd_fbufs_in);
	while (fbuf == NULL) {
		if (ioflag & IO_NDELAY) {
			error = EAGAIN;
			goto end;
		}

		error = rwsleep_nsec(&fd->fd_fbufs_in, &fd->fd_lock,
		    PWAIT | PCATCH, "fusedr", INFSLP);

		/* check for unmount during sleep */
		if (fd->fd_fmp == NULL) {
			error = ENODEV;
			goto end;
		}
		if (error == EINTR || error == ERESTART) {
			error = EINTR;
			goto end;
		}

		fbuf = SIMPLEQ_FIRST(&fd->fd_fbufs_in);
	}

	/* We get the whole fusebuf or nothing */
	if (uio->uio_resid < sizeof(fbuf->hdr) + fbuf->op_in_len +
	    fbuf->fb_len) {
		error = EINVAL;
		goto end;
	}

	error = uiomove(&fbuf->hdr, sizeof(fbuf->hdr), uio);
	if (error)
		goto end;
	error = uiomove(&fbuf->op, fbuf->op_in_len, uio);
	if (error)
		goto end;
	if (fbuf->fb_len > 0) {
		error = uiomove(fbuf->fb_dat, fbuf->fb_len, uio);
		if (error)
			goto end;
	}

	free(fbuf->fb_dat, M_FUSEFS, fbuf->fb_len);
	fbuf->fb_dat = NULL;

	/* Move the fbuf to the wait queue */
	SIMPLEQ_REMOVE_HEAD(&fd->fd_fbufs_in, fb_next);
	stat_fbufs_in--;

	/* FUSE_FORGET has no response */
	if (fbuf->fb_type == FUSE_FORGET) {
		fb_delete(fbuf);
		goto end;
 	}

	SIMPLEQ_INSERT_TAIL(&fd->fd_fbufs_wait, fbuf, fb_next);
	stat_fbufs_wait++;

end:
	rw_exit_write(&fd->fd_lock);
	refcnt_rele_wake(&fd->fd_refcnt);
	return (error);
}

int
fusewrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct fusebuf *lastfbuf;
	struct fuse_d *fd;
	struct fusebuf *fbuf;
	struct fuse_out_header hdr;
	int error = 0;

	fd = fuse_lookup(minor(dev));
	if (fd == NULL)
		return (ENODEV);

	/* Check for sanity - must receive at least the header */
	if (uio->uio_resid < sizeof(hdr)) {
		error = EINVAL;
		goto out;
	}

	/* Read the header */
	if ((error = uiomove(&hdr, sizeof(hdr), uio)) != 0)
		goto out;

	/*
	 * A unique value of zero means daemon is notifying us and hdr.error
	 * contains notification type. Currently unsupported.
	 */
	if (hdr.unique == 0) {
		error = 0;
		goto out;
	}

	/* looking for uuid in fd_fbufs_wait */
	SIMPLEQ_FOREACH(fbuf, &fd->fd_fbufs_wait, fb_next) {
		if (fbuf->fb_uuid == hdr.unique)
			break;

		lastfbuf = fbuf;
	}
	if (fbuf == NULL) {
		error = ENOENT;
		goto out;
	}

	/* Update fb_hdr */
	fbuf->fb_err = -hdr.error;

	/* Don't expect out struct or data if there was an error */
	if (fbuf->fb_err) {
		if (uio->uio_resid > 0) {
			error = EINVAL;
			fbuf->fb_err = EIO;
		}
 		goto end;
 	}

	/* get operation output */
	if (fbuf->op_out_len > 0) {
		if ((error = uiomove(&fbuf->op, fbuf->op_out_len, uio)) != 0) {
			fbuf->fb_err = error;
			goto end;
		}
	}

	/* Calculate the length of the data buffer to expect */
	if (fbuf->op_out_buf) {
		fbuf->fb_len = hdr.len - sizeof(hdr) - fbuf->op_out_len;
		if (fbuf->fb_len > fd->fd_fmp->max_read || fbuf->fb_len < 0) {
			DPRINTF("invalid fusebuf read size: %llu opcode=%d\n",
			    fbuf->fb_len, fbuf->fb_type);
			error = EINVAL;
			fbuf->fb_err = EIO;
			goto end;
		}
	} else
		fbuf->fb_len = 0;

	/* validate remaining data */
	if (uio->uio_resid != fbuf->fb_len) {
		error = EINVAL;
		fbuf->fb_err = EIO;
		goto end;
	}

	if (fbuf->fb_len > 0) {
		fbuf->fb_dat = malloc(fbuf->fb_len, M_FUSEFS, M_WAITOK);
		if ((error = uiomove(fbuf->fb_dat, fbuf->fb_len, uio)) != 0) {
			free(fbuf->fb_dat, M_FUSEFS, fbuf->fb_len);
			fbuf->fb_dat = NULL;
			fbuf->fb_err = error;
			goto end;
		}
	}

	if (fbuf->fb_type == FUSE_INIT && fbuf->fb_err == 0) {
		/*
		 * We don't support userspace with a smaller major version and
		 * it's up to userspace implementations to fall back to our
	 	 * version if they are capable of a later version.
	 	 */
		if (fbuf->op.out.init.major != FUSE_KERNEL_VERSION) {
			DPRINTF("unsupported major version: %d.%d\n",
 			    fbuf->op.out.init.major, fbuf->op.out.init.minor);
			error = EINVAL;
			goto end;
		}
		/*
		 * If the major versions match then both shall use the smallest
 		 * of the two minor versions for communication. 7.9 is the
		 * smallest version less than what we support where the ABI has
		 * not changed. Supporting an earlier version would require
		 * conditional handling of some FUSE input arguments. If the
		 * daemon supports a later version then it must fall back to
		 * ours.
		 */
		if (fbuf->op.out.init.minor < 9) {
			DPRINTF("unsupported minor version: %d.%d\n",
 			    fbuf->op.out.init.major, fbuf->op.out.init.minor);
			error = EINVAL;
			goto end;
		}
		/*
		 * max_write determines the size of buffer to send to the file
		 * system daemon when writing so ensure that it's sane.
		 */
		fd->fd_fmp->max_write = MIN(fbuf->op.out.init.max_write,
		    FUSEBUFMAXSIZE);
		if (fd->fd_fmp->max_write == 0)
			fd->fd_fmp->max_write = FUSEBUFMAXSIZE;
		fd->fd_fmp->sess_init = 1;
 	}

end:
	/* Remove the fbuf from the wait queue */
	if (fbuf == SIMPLEQ_FIRST(&fd->fd_fbufs_wait))
		SIMPLEQ_REMOVE_HEAD(&fd->fd_fbufs_wait, fb_next);
	else
		SIMPLEQ_REMOVE_AFTER(&fd->fd_fbufs_wait, lastfbuf,
		    fb_next);
	stat_fbufs_wait--;

	/*
	 * FBT_INIT doesn't expect a response. Otherwise let the VFS
	 * syscall that is waiting on this fbuf know the reponse is ready.
	 */
	if (fbuf->fb_type == FUSE_INIT)
		fb_delete(fbuf);
	else
		wakeup(fbuf);

out:
	refcnt_rele_wake(&fd->fd_refcnt);
	return (error);
}

int
fusekqfilter(dev_t dev, struct knote *kn)
{
	struct fuse_d *fd;
	struct klist *klist;
	int error = 0;

	fd = fuse_lookup(minor(dev));
	if (fd == NULL)
		return (EINVAL);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &fd->fd_rklist;
		kn->kn_fop = &fuse_rd_filtops;
		break;
	case EVFILT_WRITE:
		error = seltrue_kqfilter(dev, kn);
		goto end;
	default:
		error = EINVAL;
		goto end;
	}

	kn->kn_hook = fd;

	klist_insert(klist, kn);

end:
	refcnt_rele_wake(&fd->fd_refcnt);

	return (error);
}

void
filt_fuse_rdetach(struct knote *kn)
{
	struct fuse_d *fd = kn->kn_hook;
	struct klist *klist = &fd->fd_rklist;

	klist_remove(klist, kn);
}

int
filt_fuse_read(struct knote *kn, long hint)
{
	struct fuse_d *fd = kn->kn_hook;
	int event = 0;

	rw_assert_wrlock(&fd->fd_lock);

	if (!SIMPLEQ_EMPTY(&fd->fd_fbufs_in))
		event = 1;

	return (event);
}

int
filt_fuse_modify(struct kevent *kev, struct knote *kn)
{
	struct fuse_d *fd = kn->kn_hook;
	int active;

	rw_enter_write(&fd->fd_lock);
	active = knote_modify(kev, kn);
	rw_exit_write(&fd->fd_lock);

	return (active);
}

int
filt_fuse_process(struct knote *kn, struct kevent *kev)
{
	struct fuse_d *fd = kn->kn_hook;
	int active;

	rw_enter_write(&fd->fd_lock);
	active = knote_process(kn, kev);
	rw_exit_write(&fd->fd_lock);

	return (active);
}

