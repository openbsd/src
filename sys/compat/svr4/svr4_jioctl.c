/*	$OpenBSD: svr4_jioctl.c,v 1.2 2002/03/14 01:26:51 millert Exp $	 */

/*
 * Copyright (c) 1997 Niklas Hallqvist
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

/*
 * Deal with the "j" svr4 ioctls ("j" stands for "jerq", the first windowing
 * terminal).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ttycom.h>

#include <compat/svr4/svr4_ioctl.h>
#include <compat/svr4/svr4_jioctl.h>
#include <compat/svr4/svr4_util.h>

int
svr4_jerq_ioctl(fp, p, retval, fd, cmd, data)
	struct file *fp;
	struct proc *p;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t data;
{
	struct svr4_jwinsize jws;
	struct winsize ws;
	int error;
	int (*ctl)(struct file *, u_long,  caddr_t, struct proc *) =
	    fp->f_ops->fo_ioctl;

	switch (cmd) {
	case SVR4_JWINSIZE:
		error = (*ctl)(fp, TIOCGWINSZ, (caddr_t)&ws, p);
		if (error)
			return (error);
		jws.bytesx = ws.ws_col;
		jws.bytesy = ws.ws_row;
		jws.bitsx = ws.ws_xpixel;
		jws.bitsy = ws.ws_ypixel;
		return (copyout(&jws, data, sizeof (jws)));

	default:
		DPRINTF(("Unimplemented ioctl %lx\n", cmd));
		return (EINVAL);
	}
}
