/*	$OpenBSD: firmload.c,v 1.2 2004/11/17 15:14:57 deraadt Exp $	*/

/*
 * Copyright (c) 2004 Theo de Raadt <deraadt@openbsd.org>
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
#include <sys/syslimits.h>
#include <sys/time.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/device.h>

int
loadfirmware(const char *name, u_char **bufp, size_t *buflen)
{
	struct proc *p = curproc;
	struct nameidata nid;
	char path[MAXPATHLEN];
	struct iovec iov;
	struct uio uio;
	struct vattr va;
	int error;
	char *ptr;

	if (!rootvp || !vcount(rootvp))
		return (EIO);

	snprintf(path, sizeof path, "/etc/firmware/%s", name);

	NDINIT(&nid, LOOKUP, NOFOLLOW|LOCKLEAF, UIO_SYSSPACE, path, p);
	error = namei(&nid);
	if (error)
		return error;
	error = VOP_GETATTR(nid.ni_vp, &va, p->p_ucred, p);
	if (error)
		goto fail;
	if (va.va_size > FIRMWARE_MAX) {
		error = E2BIG;
		goto fail;
	}
	ptr = malloc(va.va_size, M_DEVBUF, M_NOWAIT);
	if (ptr == NULL) {
		error = ENOMEM;
		goto fail;
	}

	iov.iov_base = ptr, 
	iov.iov_len = va.va_size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = va.va_size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;

	error = VOP_READ(nid.ni_vp, &uio, 0, NOCRED);

	if (error == 0) {
		*bufp = ptr;
		*buflen = va.va_size;
	} else
		free(ptr, M_DEVBUF);

fail:
	vput(nid.ni_vp);
	return (error);
}
