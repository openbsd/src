/*	$NetBSD: svr4_filio.c,v 1.3 1995/10/07 06:27:40 mycroft Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <net/if.h>
#include <sys/malloc.h>

#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_stropts.h>
#include <compat/svr4/svr4_ioctl.h>
#include <compat/svr4/svr4_filio.h>


int
svr4_filioctl(fp, cmd, data, p, retval)
	struct file *fp;
	u_long cmd;
	caddr_t data;
	struct proc *p;
	register_t *retval;
{
	struct filedesc *fdp = p->p_fd;
	int error;
	int fd;
	int num;
	int (*ctl) __P((struct file *, u_long,  caddr_t, struct proc *)) =
			fp->f_ops->fo_ioctl;

	*retval = 0;

	switch (cmd) {
	case SVR4_FIOCLEX:
		fd = fp - fdp->fd_ofiles[0]; 
		fdp->fd_ofileflags[fd] |= UF_EXCLOSE;
		return 0;

	case SVR4_FIONCLEX:
		fd = fp - fdp->fd_ofiles[0]; 
		fdp->fd_ofileflags[fd] &= ~UF_EXCLOSE;
		return 0;

	case SVR4_FIOGETOWN:
	case SVR4_FIOSETOWN:
	case SVR4_FIOASYNC:
	case SVR4_FIONBIO:
	case SVR4_FIONREAD:
		if ((error = copyin(data, &num, sizeof(num))) != 0)
			return error;

		switch (cmd) {
		case SVR4_FIOGETOWN:	cmd = FIOGETOWN; break;
		case SVR4_FIOSETOWN:	cmd = FIOSETOWN; break;
		case SVR4_FIOASYNC:	cmd = FIOASYNC;  break;
		case SVR4_FIONBIO:	cmd = FIONBIO;   break;
		case SVR4_FIONREAD:	cmd = FIONREAD;  break;
		}

		error = (*ctl)(fp, cmd, (caddr_t) &num, p);

		if (error)
			return error;

		return copyout(&num, data, sizeof(num));

	default:
		DPRINTF(("Unknown svr4 filio %x\n", cmd));
		return 0;	/* ENOSYS really */
	}
}
