
/*	$OpenBSD: linux_cdrom.c,v 1.8 2002/03/14 01:26:50 millert Exp $	*/
/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/cdio.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_cdrom.h>

void bsd_addr_to_linux_addr(union msf_lba *bsd,
    union linux_cdrom_addr *linux, int format);

void 
bsd_addr_to_linux_addr(bsd, linux, format)
        union msf_lba *bsd;
        union linux_cdrom_addr *linux;
        int format;
{
        if (format == CD_MSF_FORMAT) {
 	        linux->msf.minute = bsd->msf.minute;
		linux->msf.second = bsd->msf.second;
		linux->msf.frame = bsd->msf.frame;
	} else 
	        linux->lba = bsd->lba;
}

int
linux_ioctl_cdrom(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	struct file *fp;
	struct filedesc *fdp;
	caddr_t sg;
	u_long com, arg;
	struct sys_ioctl_args ia;
	int error;

	union {
	        struct cd_toc_entry te;
	        struct cd_sub_channel_info scinfo;
	} data;
	union {
	        struct ioc_toc_header th;
	        struct ioc_read_toc_entry tes;
	        struct ioc_play_track ti;
		struct ioc_play_msf msf;
		struct ioc_play_blocks blk;
	        struct ioc_read_subchannel sc;
		struct ioc_vol vol;
	} tmpb;
	union {
	        struct linux_cdrom_tochdr th;
	        struct linux_cdrom_tocentry te;
	        struct linux_cdrom_ti ti;
		struct linux_cdrom_msf msf;
		struct linux_cdrom_blk blk;
	        struct linux_cdrom_subchnl sc;
		struct linux_cdrom_volctrl vol;
	} tmpl;


	fdp = p->p_fd;
	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);
	FREF(fp);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0) {
		error = EBADF;
		goto out;
	}

	com = SCARG(uap, com);
	retval[0] = 0;
                
	switch (com) {
	case LINUX_CDROMREADTOCHDR:
	        error = (*fp->f_ops->fo_ioctl)(fp, CDIOREADTOCHEADER,
		    (caddr_t)&tmpb.th, p);
	        if (error)
			goto out;
		tmpl.th.cdth_trk0 = tmpb.th.starting_track;
		tmpl.th.cdth_trk1 = tmpb.th.ending_track;
		error = copyout(&tmpl, SCARG(uap, data), sizeof tmpl.th);
		goto out;
	case LINUX_CDROMREADTOCENTRY:
		error = copyin(SCARG(uap, data), &tmpl.te, sizeof tmpl.te);
		if (error)
		        goto out;

		sg = stackgap_init(p->p_emul);
		
		bzero(&tmpb.tes, sizeof tmpb.tes);
		tmpb.tes.starting_track = tmpl.te.cdte_track;
		tmpb.tes.address_format = (tmpl.te.cdte_format == LINUX_CDROM_MSF)
		    ? CD_MSF_FORMAT : CD_LBA_FORMAT;
		tmpb.tes.data_len = sizeof(struct cd_toc_entry);
		tmpb.tes.data = stackgap_alloc(&sg, tmpb.tes.data_len);

	        error = (*fp->f_ops->fo_ioctl)(fp, CDIOREADTOCENTRYS,
		    (caddr_t)&tmpb.tes, p);
	        if (error) 
			goto out;
		if ((error = copyin(tmpb.tes.data, &data.te, sizeof data.te)))
			goto out;
		
		tmpl.te.cdte_ctrl = data.te.control;
		tmpl.te.cdte_adr = data.te.addr_type;
		tmpl.te.cdte_track = data.te.track;
		tmpl.te.cdte_datamode = CD_TRACK_INFO;
		bsd_addr_to_linux_addr(&data.te.addr, &tmpl.te.cdte_addr, 
		    tmpb.tes.address_format);
		error = copyout(&tmpl, SCARG(uap, data), sizeof tmpl.te);
		goto out;
	case LINUX_CDROMSUBCHNL:
		error = copyin(SCARG(uap, data), &tmpl.sc, sizeof tmpl.sc);
		if (error)
			goto out;

		sg = stackgap_init(p->p_emul);
		
		bzero(&tmpb.sc, sizeof tmpb.sc);
		tmpb.sc.data_format = CD_CURRENT_POSITION;
		tmpb.sc.address_format = (tmpl.sc.cdsc_format == LINUX_CDROM_MSF)
		    ? CD_MSF_FORMAT : CD_LBA_FORMAT;
		tmpb.sc.data_len = sizeof(struct cd_sub_channel_info);
		tmpb.sc.data = stackgap_alloc(&sg, tmpb.sc.data_len);

	        error = (*fp->f_ops->fo_ioctl)(fp, CDIOCREADSUBCHANNEL,
		    (caddr_t)&tmpb.sc, p);
	        if (error)
			goto out;
		if ((error = copyin(tmpb.sc.data, &data.scinfo, sizeof data.scinfo)))
			goto out;
		
		tmpl.sc.cdsc_audiostatus = data.scinfo.header.audio_status;
		tmpl.sc.cdsc_adr = data.scinfo.what.position.addr_type;
		tmpl.sc.cdsc_ctrl = data.scinfo.what.position.control;
		tmpl.sc.cdsc_trk = data.scinfo.what.position.track_number;
		tmpl.sc.cdsc_ind = data.scinfo.what.position.index_number;
		bsd_addr_to_linux_addr(&data.scinfo.what.position.absaddr, 
		    &tmpl.sc.cdsc_absaddr, 
		    tmpb.sc.address_format);
		bsd_addr_to_linux_addr(&data.scinfo.what.position.reladdr, 
		    &tmpl.sc.cdsc_reladdr, 
		    tmpb.sc.address_format);

		error = copyout(&tmpl, SCARG(uap, data), sizeof tmpl.sc);
		goto out;
	case LINUX_CDROMPLAYTRKIND:
		error = copyin(SCARG(uap, data), &tmpl.ti, sizeof tmpl.ti);
		if (error)
			goto out;

		tmpb.ti.start_track = tmpl.ti.cdti_trk0;
		tmpb.ti.start_index = tmpl.ti.cdti_ind0;
		tmpb.ti.end_track = tmpl.ti.cdti_trk1;
		tmpb.ti.end_index = tmpl.ti.cdti_ind1;
	        error = (*fp->f_ops->fo_ioctl)(fp, CDIOCPLAYTRACKS,
		    (caddr_t)&tmpb.ti, p);
		goto out;
	case LINUX_CDROMPLAYMSF:
		error = copyin(SCARG(uap, data), &tmpl.msf, sizeof tmpl.msf);
		if (error)
			goto out;

		tmpb.msf.start_m = tmpl.msf.cdmsf_min0;
		tmpb.msf.start_s = tmpl.msf.cdmsf_sec0;
		tmpb.msf.start_f = tmpl.msf.cdmsf_frame0;
		tmpb.msf.end_m = tmpl.msf.cdmsf_min1;
		tmpb.msf.end_s = tmpl.msf.cdmsf_sec1;
		tmpb.msf.end_f = tmpl.msf.cdmsf_frame1;

		error = (*fp->f_ops->fo_ioctl)(fp, CDIOCPLAYMSF,
		    (caddr_t)&tmpb.msf, p);
		goto out;
	case LINUX_CDROMPLAYBLK:
		error = copyin(SCARG(uap, data), &tmpl.blk, sizeof tmpl.blk);
		if (error)
			goto out;

		tmpb.blk.blk = tmpl.blk.from;
		tmpb.blk.len = tmpl.blk.len;

		error = (*fp->f_ops->fo_ioctl)(fp, CDIOCPLAYBLOCKS,
		    (caddr_t)&tmpb.blk, p);
		goto out;
	case LINUX_CDROMVOLCTRL:
		error = copyin(SCARG(uap, data), &tmpl.vol, sizeof tmpl.vol);
		if (error)
			goto out;

		tmpb.vol.vol[0] = tmpl.vol.channel0;
		tmpb.vol.vol[1] = tmpl.vol.channel1;
		tmpb.vol.vol[2] = tmpl.vol.channel2;
		tmpb.vol.vol[3] = tmpl.vol.channel3;

		error = (*fp->f_ops->fo_ioctl)(fp, CDIOCSETVOL,
		    (caddr_t)&tmpb.vol, p);
		goto out;
	case LINUX_CDROMVOLREAD:
		error = (*fp->f_ops->fo_ioctl)(fp, CDIOCGETVOL,
		    (caddr_t)&tmpb.vol, p);
		if (error)
			goto out;

		tmpl.vol.channel0 = tmpb.vol.vol[0];
		tmpl.vol.channel1 = tmpb.vol.vol[1];
		tmpl.vol.channel2 = tmpb.vol.vol[2];
		tmpl.vol.channel3 = tmpb.vol.vol[3];

		error = copyout(&tmpl.vol, SCARG(uap, data), sizeof tmpl.vol);
		goto out;
	case LINUX_CDROMPAUSE:
		SCARG(&ia, com) = CDIOCPAUSE;
		break;
	case LINUX_CDROMRESUME:
		SCARG(&ia, com) = CDIOCRESUME;
		break;
	case LINUX_CDROMSTOP:
		SCARG(&ia, com) = CDIOCSTOP;
		break;
	case LINUX_CDROMSTART:
		SCARG(&ia, com) = CDIOCSTART;
		break;
	case LINUX_CDROMEJECT_SW:
		error = copyin(SCARG(uap, data), &arg, sizeof arg);
		if (error)
			goto out;
		SCARG(&ia, com) = arg ? CDIOCALLOW : CDIOCPREVENT;
		break;
	case LINUX_CDROMEJECT:
		SCARG(&ia, com) = CDIOCEJECT;
		break;
	case LINUX_CDROMRESET:
		SCARG(&ia, com) = CDIOCRESET;
		break;
	default:
	        printf("linux_ioctl_cdrom: invalid ioctl %08lx\n", com);
		error = EINVAL;
		goto out;
	}

	SCARG(&ia, fd) = SCARG(uap, fd);
	SCARG(&ia, data) = SCARG(uap, data);
	error = sys_ioctl(p, &ia, retval);

out:
	FRELE(fp);
	return (error);
}
