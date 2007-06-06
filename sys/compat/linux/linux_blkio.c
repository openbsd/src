/*	$OpenBSD: linux_blkio.c,v 1.7 2007/06/06 17:15:13 deraadt Exp $	*/
/*	$NetBSD: linux_blkio.c,v 1.3 2001/01/18 17:48:04 tv Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/disklabel.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_blkio.h>

#include <compat/linux/linux_syscallargs.h>

int
linux_ioctl_blkio(struct proc *p, struct linux_sys_ioctl_args *uap,
		  register_t *retval)
{
	u_long com;
	long size;
	int error;
	struct filedesc *fdp;
	struct file *fp;
	int (*ioctlf)(struct file *, u_long, caddr_t, struct proc *);
	struct partinfo partp;
	struct disklabel label;

        fdp = p->p_fd;
	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);
	FREF(fp);
	error = 0;
	ioctlf = fp->f_ops->fo_ioctl;
	com = SCARG(uap, com);

	switch (com) {
	case LINUX_BLKGETSIZE:
		/*
		 * Try to get the partition size of this device. If that
		 * fails, it may be a disk without label; try to get
		 * the default label and compute the size from it.
		 */
		error = ioctlf(fp, DIOCGPART, (caddr_t)&partp, p);
		if (error != 0) {
			error = ioctlf(fp, DIOCGDINFO, (caddr_t)&label, p);
			if (error != 0)
				break;
			size = label.d_nsectors * label.d_ntracks *
			    label.d_ncylinders;
		} else
			/* XXX ignores > 32bit blocks */
			size = DL_GETPSIZE(partp.part);
		error = copyout(&size, SCARG(uap, data), sizeof size);
		break;
	case LINUX_BLKSECTGET:
		error = ioctlf(fp, DIOCGDINFO, (caddr_t)&label, p);
		if (error != 0)
			break;
		error = copyout(&label.d_secsize, SCARG(uap, data),
		    sizeof label.d_secsize);
		break;
	case LINUX_BLKROSET:
	case LINUX_BLKROGET:
	case LINUX_BLKRRPART:
	case LINUX_BLKFLSBUF:
	case LINUX_BLKRASET:
	case LINUX_BLKRAGET:
	case LINUX_BLKFRASET:
	case LINUX_BLKFRAGET:
	case LINUX_BLKSECTSET:
	case LINUX_BLKSSZGET:
	case LINUX_BLKPG:
	default:
		error = ENOTTY;
	}

	FRELE(fp);
	return error;
}
