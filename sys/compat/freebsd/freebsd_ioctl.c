/*	$OpenBSD: freebsd_ioctl.c,v 1.4 2001/02/03 02:45:31 mickey Exp $	*/
/*	$NetBSD: freebsd_ioctl.c,v 1.1 1995/10/10 01:19:31 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>

#include <sys/syscallargs.h>

#include <compat/freebsd/freebsd_signal.h>
#include <compat/freebsd/freebsd_syscallargs.h>
#include <compat/freebsd/freebsd_util.h>
#include <compat/freebsd/freebsd_ioctl.h>

#include <compat/ossaudio/ossaudio.h>
#include <compat/ossaudio/ossaudiovar.h>

/* The FreeBSD and OSS(Linux) encodings of ioctl R/W differ. */
static void freebsd_to_oss(struct freebsd_sys_ioctl_args *,
			   struct oss_sys_ioctl_args *);

static void
freebsd_to_oss(uap, rap)
struct freebsd_sys_ioctl_args *uap;
struct oss_sys_ioctl_args *rap;
{
	u_long ocmd, ncmd;

        ocmd = SCARG(uap, com);
        ncmd = ocmd &~ FREEBSD_IOC_DIRMASK;
	switch(ocmd & FREEBSD_IOC_DIRMASK) {
        case FREEBSD_IOC_VOID:  ncmd |= OSS_IOC_VOID;  break;
        case FREEBSD_IOC_OUT:   ncmd |= OSS_IOC_OUT;   break;
        case FREEBSD_IOC_IN:    ncmd |= OSS_IOC_IN;    break;
        case FREEBSD_IOC_INOUT: ncmd |= OSS_IOC_INOUT; break;
        }
        SCARG(rap, fd) = SCARG(uap, fd);
        SCARG(rap, com) = ncmd;
        SCARG(rap, data) = SCARG(uap, data);
}

int
freebsd_sys_ioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
        struct oss_sys_ioctl_args ap;

	/*
	 * XXX - <sys/cdio.h>'s incompatibility
	 *	_IO('c', 25..27, *):	incompatible
	 *	_IO('c', 28... , *):	not exist
	 */
	/* XXX - <sys/mtio.h> */
	/* XXX - <sys/scsiio.h> */
	/* XXX - should convert machine dependent ioctl()s */

	switch (FREEBSD_IOCGROUP(SCARG(uap, com))) {
	case 'M':
        	freebsd_to_oss(uap, &ap);
		return oss_ioctl_mixer(p, &ap, retval);
	case 'Q':
        	freebsd_to_oss(uap, &ap);
		return oss_ioctl_sequencer(p, &ap, retval);
	case 'P':
        	freebsd_to_oss(uap, &ap);
		return oss_ioctl_audio(p, &ap, retval);
	default:
		return sys_ioctl(p, uap, retval);
	}
}
