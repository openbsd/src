/* 	$OpenBSD: compat_dir.c,v 1.11 2014/12/16 21:25:28 tedu Exp $	*/

/*
 * Copyright (c) 2000 Constantine Sapuntzakis
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/dirent.h>

#include <compat/common/compat_dir.h>

int
readdir_with_callback(struct file *fp, off_t *off, u_long nbytes,
    int (*appendfunc)(void *, struct dirent *), void *arg)
{
	struct dirent *bdp;
	caddr_t inp, buf;
	int buflen;
	struct uio auio;
	struct iovec aiov;
	int eofflag = 0;
	int error, len, reclen;
	off_t newoff = *off;
	struct vnode *vp;
	struct vattr va;
		
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);

	vp = (struct vnode *)fp->f_data;

	if (vp->v_type != VDIR)
		return (EINVAL);

	if ((error = VOP_GETATTR(vp, &va, fp->f_cred, curproc)) != 0)
		return (error);

	buflen = min(MAXBSIZE, nbytes);
	buflen = max(buflen, va.va_blocksize);
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curproc);
	if (error) {
		free(buf, M_TEMP, 0);
		return (error);
	}

again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = curproc;
	auio.uio_resid = buflen;
	auio.uio_offset = newoff;

	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag);
	*off = auio.uio_offset;
	if (error)
		goto out;

	if ((len = buflen - auio.uio_resid) <= 0)
		goto eof;	

	inp = buf;

	for (; len > 0; len -= reclen, inp += reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;

		if (len < reclen)
			break;

		if (reclen & 3) {
			error = EFAULT;
			goto out;
		}

		/* Skip holes */
		if (bdp->d_fileno != 0) {
			if ((error = (*appendfunc) (arg, bdp)) != 0) {
				if (error == ENOMEM)
					error = 0;
				break;
			}
		}
	}

	if (len <= 0 && !eofflag)
		goto again;

eof:
out:
	VOP_UNLOCK(vp, 0, curproc);
	free(buf, M_TEMP, 0);
	return (error);
}
