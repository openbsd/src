/*	$OpenBSD: linux_mount.c,v 1.4 1996/10/16 12:25:26 deraadt Exp $	*/

/*
 * Copyright (c) 1996 Erik Theisen
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/filedesc.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>

/*
 * These are just dummy mount/umount functions
 * who's purpose is to satisfy brain dead code
 * that bindly calls mount().  They always 
 * return EPERM.
 *
 * You really shouldn't be running code via
 * emulation that mounts FSs.
 */
int
linux_sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_mount_args /* {
		syscallarg(char *) specialfile;
		syscallarg(char *) dir;
		syscallarg(char *) filesystemtype;
		syscallarg(long) rwflag;
		syscallarg(void *) data;
	} *uap = v */ ;
        return EPERM;
}

int
linux_sys_umount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_umount_args /* {
		syscallarg(char *) specialfile;
	} *uap = v */ ;
        return EPERM;
}
