/*	$OpenBSD: linux_hdio.c,v 1.7 2010/05/18 04:41:14 dlg Exp $	*/
/*	$NetBSD: linux_hdio.c,v 1.1 2000/12/10 14:12:17 fvdl Exp $	*/

/*
 * Copyright (c) 2000 Wasabi Systems, Inc.
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
#include <sys/dkio.h>

#include <dev/ata/atareg.h>
#include <dev/ic/wdcreg.h>
#include <sys/ataio.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_hdio.h>

#include <compat/linux/linux_syscallargs.h>

int
linux_ioctl_hdio(struct proc *p, struct linux_sys_ioctl_args *uap,
		 register_t *retval)
{
	u_long com;
	int error, error1;
	caddr_t sg;
	struct filedesc *fdp;
	struct file *fp;
	int (*ioctlf)(struct file *, u_long, caddr_t, struct proc *);
	struct ataparams *atap, ata;
	struct atareq req;
	struct disklabel label, *labp;
	struct partinfo partp;
	struct linux_hd_geometry hdg;
	struct linux_hd_big_geometry hdg_big;

	fdp = p->p_fd;
	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);

	FREF(fp);
	com = SCARG(uap, com);
	ioctlf = fp->f_ops->fo_ioctl;
	retval[0] = error = 0;

	com = SCARG(uap, com);

	switch (com) {
	case LINUX_HDIO_OBSOLETE_IDENTITY:
	case LINUX_HDIO_GET_IDENTITY:
		sg = stackgap_init(p->p_emul);
		atap = stackgap_alloc(&sg, DEV_BSIZE);
		if (atap == NULL) {
			error = ENOMEM;
			break;
		}

		req.flags = ATACMD_READ;
		req.command = WDCC_IDENTIFY;
		req.databuf = (caddr_t)atap;
		req.datalen = DEV_BSIZE;
		req.timeout = 1000;
		error = ioctlf(fp, ATAIOCCOMMAND, (caddr_t)&req, p);
		if (error != 0)
			break;
		if (req.retsts != ATACMD_OK) {
			error = EIO;
			break;
		}
		error = copyin(atap, &ata, sizeof ata);
		if (error != 0)
			break;
		/*
		 * 142 is the size of the old structure used by Linux,
		 * which doesn't seem to be defined anywhere anymore.
		 */
		error = copyout(&ata, SCARG(uap, data),
		    com == LINUX_HDIO_GET_IDENTITY ? sizeof ata : 142);
		break;
	case LINUX_HDIO_GETGEO:
		error = linux_machdepioctl(p, uap, retval);
		if (error == 0)
			break;
		error = ioctlf(fp, DIOCGDINFO, (caddr_t)&label, p);
		error1 = ioctlf(fp, DIOCGPART, (caddr_t)&partp, p);
		if (error != 0 && error1 != 0) {
			error = error1;
			break;
		}
		labp = error != 0 ? &label : partp.disklab;
		hdg.start = error1 != 0 ? DL_GETPOFFSET(partp.part) & 0x7fffffff: 0;
		hdg.heads = labp->d_ntracks;
		hdg.cylinders = labp->d_ncylinders;
		hdg.sectors = labp->d_nsectors;
		error = copyout(&hdg, SCARG(uap, data), sizeof hdg);
		break;
	case LINUX_HDIO_GETGEO_BIG:
		error = linux_machdepioctl(p, uap, retval);
		if (error == 0)
			break;
	case LINUX_HDIO_GETGEO_BIG_RAW:
		error = ioctlf(fp, DIOCGDINFO, (caddr_t)&label, p);
		error1 = ioctlf(fp, DIOCGPART, (caddr_t)&partp, p);
		if (error != 0 && error1 != 0) {
			error = error1;
			break;
		}
		labp = error != 0 ? &label : partp.disklab;
		hdg_big.start = error1 != 0 ? DL_GETPOFFSET(partp.part) & 0x7fffffff : 0;
		hdg_big.heads = labp->d_ntracks;
		hdg_big.cylinders = labp->d_ncylinders;
		hdg_big.sectors = labp->d_nsectors;
		error = copyout(&hdg_big, SCARG(uap, data), sizeof hdg_big);
		break;
	case LINUX_HDIO_GET_UNMASKINTR:
	case LINUX_HDIO_GET_MULTCOUNT:
	case LINUX_HDIO_GET_KEEPSETTINGS:
	case LINUX_HDIO_GET_32BIT:
	case LINUX_HDIO_GET_NOWERR:
	case LINUX_HDIO_GET_DMA:
	case LINUX_HDIO_GET_NICE:
	case LINUX_HDIO_DRIVE_RESET:
	case LINUX_HDIO_TRISTATE_HWIF:
	case LINUX_HDIO_DRIVE_TASK:
	case LINUX_HDIO_DRIVE_CMD:
	case LINUX_HDIO_SET_MULTCOUNT:
	case LINUX_HDIO_SET_UNMASKINTR:
	case LINUX_HDIO_SET_KEEPSETTINGS:
	case LINUX_HDIO_SET_32BIT:
	case LINUX_HDIO_SET_NOWERR:
	case LINUX_HDIO_SET_DMA:
	case LINUX_HDIO_SET_PIO_MODE:
	case LINUX_HDIO_SCAN_HWIF:
	case LINUX_HDIO_SET_NICE:
	case LINUX_HDIO_UNREGISTER_HWIF:
		error = EINVAL;
	}

	FRELE(fp);
	return error;
}
