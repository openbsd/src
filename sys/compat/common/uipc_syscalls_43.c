/*	$OpenBSD: uipc_syscalls_43.c,v 1.5 1998/08/05 16:38:31 millert Exp $	*/
/*	$NetBSD: uipc_syscalls_43.c,v 1.5 1996/03/14 19:31:50 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)uipc_syscalls.c	8.4 (Berkeley) 2/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

int
compat_43_sys_accept(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_accept_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) name;
		syscallarg(int *) anamelen;
	} */ *uap = v;
	int error;

	if ((error = sys_accept(p, uap, retval)) != 0)
		return error;

	if (SCARG(uap, name)) {
		struct sockaddr sa;

		if ((error = copyin(SCARG(uap, name), &sa, sizeof(sa))) != 0)
			return error;

		((struct osockaddr*) &sa)->sa_family = sa.sa_family;

		if ((error = copyout(&sa, SCARG(uap, name), sizeof(sa))) != 0)
			return error;
	}
	return 0;
}


int
compat_43_sys_getpeername(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_getpeername_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap = v;
	struct sockaddr sa;

	int error;

	if ((error = sys_getpeername(p, uap, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(uap, asa), &sa, sizeof(sa))) != 0)
		return error;

	((struct osockaddr*) &sa)->sa_family = sa.sa_family;

	if ((error = copyout(&sa, SCARG(uap, asa), sizeof(sa))) != 0)
		return error;

	return 0;
}


int
compat_43_sys_getsockname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_getsockname_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap = v;
	struct sockaddr sa;
	int error;

	if ((error = sys_getsockname(p, uap, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(uap, asa), &sa, sizeof(sa))) != 0)
		return error;

	((struct osockaddr*) &sa)->sa_family = sa.sa_family;

	if ((error = copyout(&sa, SCARG(uap, asa), sizeof(sa))) != 0)
		return error;

	return 0;
}


int
compat_43_sys_recv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_recv_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = SCARG(uap, flags);
	return (recvit(p, SCARG(uap, s), &msg, (caddr_t)0, retval));
}


#ifdef MSG_COMPAT
int
compat_43_sys_recvfrom(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(caddr_t) from;
		syscallarg(int *) fromlenaddr;
	} */ *uap = v;

	SCARG(uap, flags) |= MSG_COMPAT;
	return (sys_recvfrom(p, uap, retval));
}
#endif


#ifdef MSG_COMPAT
/*
 * Old recvmsg.  This code takes advantage of the fact that the old msghdr
 * overlays the new one, missing only the flags, and with the (old) access
 * rights where the control fields are now.
 */
int
compat_43_sys_recvmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_recvmsg_args /* {
		syscallarg(int) s;
		syscallarg(struct omsghdr *) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin((caddr_t)SCARG(uap, msg), (caddr_t)&msg,
	    sizeof (struct omsghdr));
	if (error)
		return (error);
	if (msg.msg_iovlen <= 0 || msg.msg_iovlen > UIO_MAXIOV)
		return (EMSGSIZE);
	if (msg.msg_iovlen > UIO_SMALLIOV)
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * msg.msg_iovlen, M_IOV, M_WAITOK);
	else
		iov = aiov;
	msg.msg_flags = SCARG(uap, flags) | MSG_COMPAT;
	error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)));
	if (error)
		goto done;
	msg.msg_iov = iov;
	error = recvit(p, SCARG(uap, s), &msg,
	    (caddr_t)&SCARG(uap, msg)->msg_namelen, retval);

	if (msg.msg_controllen && error == 0)
		error = copyout((caddr_t)&msg.msg_controllen,
		    (caddr_t)&SCARG(uap, msg)->msg_accrightslen, sizeof (int));
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif

int
compat_43_sys_send(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_send_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = 0;
	return (sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval));
}

#ifdef MSG_COMPAT
int
compat_43_sys_sendmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_sendmsg_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin(SCARG(uap, msg), (caddr_t)&msg,
	    sizeof (struct omsghdr));
	if (error)
		return (error);
	if (msg.msg_iovlen <= 0 || msg.msg_iovlen > UIO_MAXIOV)
		return (EMSGSIZE);
	if (msg.msg_iovlen > UIO_SMALLIOV)
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * msg.msg_iovlen, M_IOV, M_WAITOK);
	else
		iov = aiov;
	error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)));
	if (error)
		goto done;
	msg.msg_flags = MSG_COMPAT;
	msg.msg_iov = iov;
	error = sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif
