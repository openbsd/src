/*	$OpenBSD: tty_tty.c,v 1.12 2011/10/06 09:14:35 mikeb Exp $	*/
/*	$NetBSD: tty_tty.c,v 1.13 1996/03/30 22:24:46 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tty_tty.c	8.2 (Berkeley) 9/23/93
 */

/*
 * Indirect driver for controlling tty.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/conf.h>


#define cttyvp(p) \
	((p)->p_p->ps_flags & PS_CONTROLT ? \
	    (p)->p_p->ps_session->s_ttyvp : NULL)

/*ARGSUSED*/
int
cttyopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct vnode *ttyvp = cttyvp(p);
	int error;

	if (ttyvp == NULL)
		return (ENXIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, p);
#ifdef PARANOID
	/*
	 * Since group is tty and mode is 620 on most terminal lines
	 * and since sessions protect terminals from processes outside
	 * your session, this check is probably no longer necessary.
	 * Since it inhibits setuid root programs that later switch 
	 * to another user from accessing /dev/tty, we have decided
	 * to delete this test. (mckusick 5/93)
	 */
	error = VOP_ACCESS(ttyvp,
	  (flag&FREAD ? VREAD : 0) | (flag&FWRITE ? VWRITE : 0), p->p_ucred, p);
	if (!error)
#endif /* PARANOID */
		error = VOP_OPEN(ttyvp, flag, NOCRED, p);
	VOP_UNLOCK(ttyvp, 0, p);
	return (error);
}

/*ARGSUSED*/
int
cttyread(dev_t dev, struct uio *uio, int flag)
{
	struct proc *p = uio->uio_procp;
	struct vnode *ttyvp = cttyvp(uio->uio_procp);
	int error;

	if (ttyvp == NULL)
		return (EIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_READ(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp, 0, p);
	return (error);
}

/*ARGSUSED*/
int
cttywrite(dev_t dev, struct uio *uio, int flag)
{
	struct proc *p = uio->uio_procp;
	struct vnode *ttyvp = cttyvp(uio->uio_procp);
	int error;

	if (ttyvp == NULL)
		return (EIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_WRITE(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp, 0, p);
	return (error);
}

/*ARGSUSED*/
int
cttyioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct vnode *ttyvp = cttyvp(p);

	if (ttyvp == NULL)
		return (EIO);
	if (cmd == TIOCSCTTY)		/* XXX */
		return (EINVAL);
	if (cmd == TIOCNOTTY) {
		if (!SESS_LEADER(p->p_p)) {
			atomic_clearbits_int(&p->p_p->ps_flags, PS_CONTROLT);
			return (0);
		} else
			return (EINVAL);
	}
	return (VOP_IOCTL(ttyvp, cmd, addr, flag, NOCRED, p));
}

/*ARGSUSED*/
int
cttypoll(dev_t dev, int events, struct proc *p)
{
	struct vnode *ttyvp = cttyvp(p);

	if (ttyvp == NULL)	/* try operation to get EOF/failure */
		return (seltrue(dev, events, p));
	return (VOP_POLL(ttyvp, events, p));
}

/*ARGSUSED*/
int
cttykqfilter(dev_t dev, struct knote *kn)
{
	struct vnode *ttyvp = cttyvp(curproc);

	if (ttyvp == NULL)
		return (ENXIO);
	return (VOP_KQFILTER(ttyvp, kn));
}
