/*	$OpenBSD: linux_ioctl.c,v 1.10 2003/07/23 20:18:10 tedu Exp $	*/
/*	$NetBSD: linux_ioctl.c,v 1.14 1996/04/05 00:01:28 christos Exp $	*/

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
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <sys/socket.h>
#include <net/if.h>
#include <sys/sockio.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_ioctl.h>

#include <compat/ossaudio/ossaudio.h>
#define LINUX_TO_OSS(v) (v)	/* do nothing, same ioctl() encoding */

/*
 * Most ioctl command are just converted to their OpenBSD values,
 * and passed on. The ones that take structure pointers and (flag)
 * values need some massaging. This is done the usual way by
 * allocating stackgap memory, letting the actual ioctl call do its
 * work their and converting back the data afterwards.
 */
int
linux_sys_ioctl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;

	switch (LINUX_IOCGROUP(SCARG(uap, com))) {
	case 'M':
		return oss_ioctl_mixer(p, LINUX_TO_OSS(v), retval);
	case 'Q':
		return oss_ioctl_sequencer(p, LINUX_TO_OSS(v), retval);
	case 'P':
		return oss_ioctl_audio(p, LINUX_TO_OSS(v), retval);
	case 't':
	case 'f':
	case 'T':	/* XXX MIDI sequencer uses 'T' as well */
		return linux_ioctl_termios(p, uap, retval);
	case 'S':
		return linux_ioctl_cdrom(p, uap, retval);
	case 'r':	/* VFAT ioctls; not yet support */
		return (EINVAL);
	case 0x89:
		return linux_ioctl_socket(p, uap, retval);
	case 0x03:
		return linux_ioctl_hdio(p, uap, retval);
	case 0x02:
		return linux_ioctl_fdio(p, uap, retval);
	case 0x12:
		return linux_ioctl_blkio(p, uap, retval);
	default:
		return linux_machdepioctl(p, uap, retval);
	}
}
