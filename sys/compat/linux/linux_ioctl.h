/*	$OpenBSD: linux_ioctl.h,v 1.7 2002/03/14 01:26:50 millert Exp $	*/
/*	$NetBSD: linux_ioctl.h,v 1.4 1996/04/05 00:01:36 christos Exp $	*/

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

#define	_LINUX_IOC_NRBITS	8
#define	_LINUX_IOC_TYPEBITS	8
#define	_LINUX_IOC_SIZEBITS	14
#define	_LINUX_IOC_DIRBITS	2

#define	_LINUX_IOC_NRSHIFT	0

#define	_LINUX_IOC_NONE		0U
#define	_LINUX_IOC_WRITE	1U
#define	_LINUX_IOC_READ		2U

#define	_LINUX_IOC_NRMASK	((1 << _LINUX_IOC_NRBITS) - 1)
#define	_LINUX_IOC_TYPEMASK	((1 << _LINUX_IOC_TYPEBITS) - 1)
#define	_LINUX_IOC_SIZEMASK	((1 << _LINUX_IOC_SIZEBITS) - 1)
#define	_LINUX_IOC_DIRMASK	((1 << _LINUX_IOC_DIRBITS) - 1)

#define	_LINUX_IOC_TYPESHIFT	(_LINUX_IOC_NRSHIFT + _LINUX_IOC_NRBITS)
#define	_LINUX_IOC_SIZESHIFT	(_LINUX_IOC_TYPESHIFT + _LINUX_IOC_TYPEBITS)
#define	_LINUX_IOC_DIRSHIFT	(_LINUX_IOC_SIZESHIFT + _LINUX_IOC_SIZEBITS)

#define	_LINUX_IOC(dir,type,nr,size)		\
	(((nr)   << _LINUX_IOC_NRSHIFT) |	\
	 ((type) << _LINUX_IOC_TYPESHIFT) |	\
	 ((size) << _LINUX_IOC_SIZESHIFT) |	\
	 ((dir)  << _LINUX_IOC_DIRSHIFT))

#define _LINUX_IO(type,nr)		\
	_LINUX_IOC(_LINUX_IOC_NONE,(type),(nr),0)
#define _LINUX_IOR(type,nr,size)	\
	_LINUX_IOC(_LINUX_IOC_READ,(type),(nr),sizeof(size))
#define _LINUX_IOW(type,nr,size)	\
	_LINUX_IOC(_LINUX_IOC_WRITE,(type),(nr),sizeof(size))
#define _LINUX_IOWR(type,nr,size)	\
	_LINUX_IOC(_LINUX_IOC_READ|_LINUX_IOC_WRITE,(type),(nr),sizeof(size))

#define	_LINUX_IOC_DIR(nr)	\
	(((nr) >> _LINUX_IOC_DIRSHIFT) & _LINUX_IOC_DIRMASK)
#define	_LINUX_IOC_TYPE(nr)	\
	(((nr) >> _LINUX_IOC_TYPESHIFT) & _LINUX_IOC_TYPEMASK)
#define	_LINUX_IOC_NR(nr)	\
	(((nr) >> _LINUX_IOC_NRSHIFT) & _LINUX_IOC_NRMASK)
#define	_LINUX_IOC_SIZE(nr)	\
	(((nr) >> _LINUX_IOC_SIZESHIFT) & _LINUX_IOC_SIZEMASK)

#define	LINUX_IOCGROUP(x)	_LINUX_IOC_TYPE(x)

struct linux_sys_ioctl_args;
int linux_ioctl_audio(struct proc *, struct linux_sys_ioctl_args *,
    register_t *);
int linux_machdepioctl(struct proc *, void *, register_t *);
int linux_ioctl_termios(struct proc *, void *, register_t *);
int linux_ioctl_cdrom(struct proc *, void *, register_t *);
int linux_ioctl_socket(struct proc *, void *, register_t *);
int linux_ioctl_hdio(struct proc *, struct linux_sys_ioctl_args *,
    register_t *);
int linux_ioctl_fdio(struct proc *, struct linux_sys_ioctl_args *,
    register_t *);
int linux_ioctl_blkio(struct proc *, struct linux_sys_ioctl_args *,
    register_t *);
