/*	$OpenBSD: bsdos_ioctl.c,v 1.1 1999/11/13 22:13:00 millert Exp $	*/

/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>

#include <sys/syscallargs.h>

#include <compat/bsdos/bsdos_syscallargs.h>
#include <compat/bsdos/bsdos_ioctl.h>

#include <compat/ossaudio/ossaudio.h>
#include <compat/ossaudio/ossaudiovar.h>

#include <compat/common/compat_util.h>

static void bsdos_to_oss	__P((struct bsdos_sys_ioctl_args *, struct oss_sys_ioctl_args *));

/*
 * BSD/OS and OSS have different values for IOC_*.  Also,
 * sizeof(bsdos_audio_buf_info) != sizeof(oss_audio_buf_info) which
 * is encoded in OSS_SNDCTL_DSP_GETOSPACE and OSS_SNDCTL_DSP_GETISPACE.
 */
static void
bsdos_to_oss(bap, oap)
	struct bsdos_sys_ioctl_args *bap;
	struct oss_sys_ioctl_args *oap;
{
	u_long bcom, ocom;

	bcom = SCARG(bap, com);
	ocom = bcom & ~BSDOS_IOC_DIRMASK;
	switch (bcom & BSDOS_IOC_DIRMASK) {
	case BSDOS_IOC_VOID:
		ocom |= OSS_IOC_VOID;
		break;
	case BSDOS_IOC_OUT:
		if (bcom == BSDOS_SNDCTL_DSP_GETOSPACE)
			ocom = OSS_SNDCTL_DSP_GETOSPACE;
		else if (bcom == BSDOS_SNDCTL_DSP_GETISPACE)
			ocom = OSS_SNDCTL_DSP_GETISPACE;
		else
			ocom |= OSS_IOC_OUT;
		break;
	case BSDOS_IOC_IN:
		ocom |= OSS_IOC_IN;
		break;
	case BSDOS_IOC_INOUT:
		ocom |= OSS_IOC_INOUT;
		break;
	}
	SCARG(oap, fd) = SCARG(bap, fd);
	SCARG(oap, com) = ocom;
	SCARG(oap, data) = SCARG(bap, data);
}

int
bsdos_sys_ioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct bsdos_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
        struct oss_sys_ioctl_args ap;

	/*
	 * XXX should support 'T' timer ioctl's
	 * XXX also /dev/sequencer and /dev/patmgr#
	 */
	switch (BSDOS_IOCGROUP(SCARG(uap, com))) {
	case 'M':
		bsdos_to_oss(uap, &ap);
		return (oss_ioctl_mixer(p, &ap, retval));
	case 'Q':
		bsdos_to_oss(uap, &ap);
		return (oss_ioctl_sequencer(p, &ap, retval));
	case 'P':
		bsdos_to_oss(uap, &ap);
		/*
		 * Special handling since the BSD/OS audio_buf_info
		 * struct lacks a fragstotal member.
		 */
		if (SCARG(uap, com) == BSDOS_SNDCTL_DSP_GETOSPACE ||
		    SCARG(uap, com) == BSDOS_SNDCTL_DSP_GETISPACE)
		{
			struct oss_audio_buf_info oss_buf, *oss_bufp;
			struct bsdos_audio_buf_info bsdos_buf;
			caddr_t sg = stackgap_init(p->p_emul);
			int error;

			oss_bufp = stackgap_alloc(&sg, sizeof(*oss_bufp));
			SCARG(&ap, data) = (void *) oss_bufp;
			error = oss_ioctl_audio(p, &ap, retval);
			if (error)
				return (error);
			error = copyin(oss_bufp, &oss_buf, sizeof(oss_buf));
			if (error)
				return (error);
			bsdos_buf.fragments = oss_buf.fragstotal;
			bsdos_buf.fragsize = oss_buf.fragsize;
			bsdos_buf.bytes = oss_buf.bytes;
			error = copyout(&bsdos_buf, SCARG(uap, data),
			    sizeof(bsdos_buf));
			if (error)
				return (error);
		} else
			return (oss_ioctl_audio(p, &ap, retval));
	}
	return (sys_ioctl(p, uap, retval));
}
