/*	$OpenBSD: linux_socket.c,v 1.14 1998/07/13 19:45:47 deraadt Exp $	*/
/*	$NetBSD: linux_socket.c,v 1.14 1996/04/05 00:01:50 christos Exp $	*/

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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_socket.h>
#include <compat/linux/linux_socketcall.h>
#include <compat/linux/linux_sockio.h>

/*
 * All the calls in this file are entered via one common system
 * call in Linux, represented here by linux_socketcall()
 * Arguments for the various calls are on the user stack. A pointer
 * to them is the only thing that is passed. It is up to the various
 * calls to copy them in themselves. To make it look better, they
 * are copied to structures.
 */

int linux_to_bsd_domain __P((int));
int linux_socket __P((struct proc *, struct linux_socket_args *, register_t *));
int linux_bind __P((struct proc *, struct linux_bind_args *, register_t *));
int linux_connect __P((struct proc *, struct linux_connect_args *,
    register_t *));
int linux_listen __P((struct proc *, struct linux_listen_args *, register_t *));
int linux_accept __P((struct proc *, struct linux_accept_args *, register_t *));
int linux_getsockname __P((struct proc *, struct linux_getsockname_args *,
    register_t *));
int linux_getpeername __P((struct proc *, struct linux_getpeername_args *,
    register_t *));
int linux_socketpair __P((struct proc *, struct linux_socketpair_args *,
    register_t *));
int linux_send __P((struct proc *, struct linux_send_args *, register_t *));
int linux_recv __P((struct proc *, struct linux_recv_args *, register_t *));
int linux_sendto __P((struct proc *, struct linux_sendto_args *, register_t *));
int linux_recvfrom __P((struct proc *, struct linux_recvfrom_args *,
    register_t *));
int linux_shutdown __P((struct proc *, struct linux_shutdown_args *,
    register_t *));
int linux_to_bsd_sopt_level __P((int));
int linux_to_bsd_so_sockopt __P((int));
int linux_to_bsd_ip_sockopt __P((int));
int linux_to_bsd_tcp_sockopt __P((int));
int linux_to_bsd_udp_sockopt __P((int));
int linux_setsockopt __P((struct proc *, struct linux_setsockopt_args *,
    register_t *));
int linux_getsockopt __P((struct proc *, struct linux_getsockopt_args *,
    register_t *));

/*
 * Convert between Linux and BSD socket domain values
 */
int
linux_to_bsd_domain(ldom)
	int ldom;
{

	switch (ldom) {
	case LINUX_AF_UNSPEC:
		return AF_UNSPEC;
	case LINUX_AF_UNIX:
		return AF_LOCAL;
	case LINUX_AF_INET:
		return AF_INET;
	case LINUX_AF_AX25:
		return AF_CCITT;
	case LINUX_AF_IPX:
		return AF_IPX;
	case LINUX_AF_APPLETALK:
		return AF_APPLETALK;
	default:
		return -1;
	}
}

int
linux_socket(p, uap, retval)
	struct proc *p;
	struct linux_socket_args /* {
		syscallarg(int)	domain;
		syscallarg(int)	type;
		syscallarg(int) protocol;
	} */ *uap;
	register_t *retval;
{
	struct linux_socket_args lsa;
	struct sys_socket_args bsa;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lsa, sizeof lsa)))
		return error;

	SCARG(&bsa, protocol) = lsa.protocol;
	SCARG(&bsa, type) = lsa.type;
	SCARG(&bsa, domain) = linux_to_bsd_domain(lsa.domain);
	if (SCARG(&bsa, domain) == -1)
		return EINVAL;
	return sys_socket(p, &bsa, retval);
}

int
linux_bind(p, uap, retval)
	struct proc *p;
	struct linux_bind_args /* {
		syscallarg(int)	s;
		syscallarg(struct sockaddr *) name;
		syscallarg(int)	namelen;
	} */ *uap;
	register_t *retval;
{
	struct linux_bind_args lba;
	struct sys_bind_args bba;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lba, sizeof lba)))
		return error;

	SCARG(&bba, s) = lba.s;
	SCARG(&bba, name) = (void *) lba.name;
	SCARG(&bba, namelen) = lba.namelen;

	return sys_bind(p, &bba, retval);
}

int
linux_connect(p, uap, retval)
	struct proc *p;
	struct linux_connect_args /* {
		syscallarg(int)	s;
		syscallarg(struct sockaddr *) name;
		syscallarg(int)	namelen;
	} */ *uap;
	register_t *retval;
{
	struct linux_connect_args lca;
	struct sys_connect_args bca;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lca, sizeof lca)))
		return error;

	SCARG(&bca, s) = lca.s;
	SCARG(&bca, name) = (void *) lca.name;
	SCARG(&bca, namelen) = lca.namelen;

	error = sys_connect(p, &bca, retval);

	if (error == EISCONN) {
		struct sys_getsockopt_args bga;
		struct sys_fcntl_args fca;
		void *status, *statusl;
		int stat, statl = sizeof stat;
		caddr_t sg;

		SCARG(&fca, fd) = lca.s;
		SCARG(&fca, cmd) = F_GETFL;
		SCARG(&fca, arg) = 0;
		if (sys_fcntl(p, &fca, retval) == -1 ||
		    (*retval & O_NONBLOCK) == 0)
			return error;

		sg = stackgap_init(p->p_emul);
		status = stackgap_alloc(&sg, sizeof stat);
		statusl = stackgap_alloc(&sg, sizeof statusl);

		if ((error = copyout(&statl, statusl, sizeof statl)))
			return error;

		SCARG(&bga, s) = lca.s;
		SCARG(&bga, level) = SOL_SOCKET;
		SCARG(&bga, name) = SO_ERROR;
		SCARG(&bga, val) = status;
		SCARG(&bga, avalsize) = statusl;
		
		error = sys_getsockopt(p, &bga, retval);
		if (error)
			return error;
		if ((error = copyin(status, &stat, sizeof stat)))
			return error;
		return stat;
	}
	return error;
}

int
linux_listen(p, uap, retval)
	struct proc *p;
	struct linux_listen_args /* {
		syscallarg(int) s;
		syscallarg(int) backlog;
	} */ *uap;
	register_t *retval;
{
	struct linux_listen_args lla;
	struct sys_listen_args bla;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lla, sizeof lla)))
		return error;

	SCARG(&bla, s) = lla.s;
	SCARG(&bla, backlog) = lla.backlog;

	return sys_listen(p, &bla, retval);
}

int
linux_accept(p, uap, retval)
	struct proc *p;
	struct linux_accept_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) addr;
		syscallarg(int *) namelen;
	} */ *uap;
	register_t *retval;
{
	struct linux_accept_args laa;
	struct compat_43_sys_accept_args baa;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &laa, sizeof laa)))
		return error;

	SCARG(&baa, s) = laa.s;
	SCARG(&baa, name) = (caddr_t) laa.addr;
	SCARG(&baa, anamelen) = laa.namelen;

	return compat_43_sys_accept(p, &baa, retval);
}

int
linux_getsockname(p, uap, retval)
	struct proc *p;
	struct linux_getsockname_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) addr;
		syscallarg(int *) namelen;
	} */ *uap;
	register_t *retval;
{
	struct linux_getsockname_args lga;
	struct compat_43_sys_getsockname_args bga;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lga, sizeof lga)))
		return error;

	SCARG(&bga, fdec) = lga.s;
	SCARG(&bga, asa) = (caddr_t) lga.addr;
	SCARG(&bga, alen) = lga.namelen;

	return compat_43_sys_getsockname(p, &bga, retval);
}

int
linux_getpeername(p, uap, retval)
	struct proc *p;
	struct linux_getpeername_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) addr;
		syscallarg(int *) namelen;
	} */ *uap;
	register_t *retval;
{
	struct linux_getpeername_args lga;
	struct compat_43_sys_getpeername_args bga;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lga, sizeof lga)))
		return error;

	SCARG(&bga, fdes) = lga.s;
	SCARG(&bga, asa) = (caddr_t) lga.addr;
	SCARG(&bga, alen) = lga.namelen;

	return compat_43_sys_getpeername(p, &bga, retval);
}

int
linux_socketpair(p, uap, retval)
	struct proc *p;
	struct linux_socketpair_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
		syscallarg(int *) rsv;
	} */ *uap;
	register_t *retval;
{
	struct linux_socketpair_args lsa;
	struct sys_socketpair_args bsa;
	int error;

	if ((error = copyin((caddr_t) uap, &lsa, sizeof lsa)))
		return error;

	SCARG(&bsa, domain) = linux_to_bsd_domain(lsa.domain);
	if (SCARG(&bsa, domain) == -1)
		return EINVAL;
	SCARG(&bsa, type) = lsa.type;
	SCARG(&bsa, protocol) = lsa.protocol;
	SCARG(&bsa, rsv) = lsa.rsv;

	return sys_socketpair(p, &bsa, retval);
}

int
linux_send(p, uap, retval)
	struct proc *p;
	struct linux_send_args /* {
		syscallarg(int) s;
		syscallarg(void *) msg;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct linux_send_args lsa;
	struct compat_43_sys_send_args bsa;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lsa, sizeof lsa)))
		return error;

	SCARG(&bsa, s) = lsa.s;
	SCARG(&bsa, buf) = lsa.msg;
	SCARG(&bsa, len) = lsa.len;
	SCARG(&bsa, flags) = lsa.flags;

	return compat_43_sys_send(p, &bsa, retval);
}

int
linux_recv(p, uap, retval)
	struct proc *p;
	struct linux_recv_args /* {
		syscallarg(int) s;
		syscallarg(void *) msg;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct linux_recv_args lra;
	struct compat_43_sys_recv_args bra;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lra, sizeof lra)))
		return error;

	SCARG(&bra, s) = lra.s;
	SCARG(&bra, buf) = lra.msg;
	SCARG(&bra, len) = lra.len;
	SCARG(&bra, flags) = lra.flags;

	return compat_43_sys_recv(p, &bra, retval);
}

int
linux_sendto(p, uap, retval)
	struct proc *p;
	struct linux_sendto_args /* {
		syscallarg(int) s;
		syscallarg(void *) msg;
		syscallarg(int) len;
		syscallarg(int) flags;
		syscallarg(sockaddr *) to;
		syscallarg(int) tolen;
	} */ *uap;
	register_t *retval;
{
	struct linux_sendto_args lsa;
	struct sys_sendto_args bsa;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lsa, sizeof lsa)))
		return error;

	SCARG(&bsa, s) = lsa.s;
	SCARG(&bsa, buf) = lsa.msg;
	SCARG(&bsa, len) = lsa.len;
	SCARG(&bsa, flags) = lsa.flags;
	SCARG(&bsa, to) = (void *) lsa.to;
	SCARG(&bsa, tolen) = lsa.tolen;

	return sys_sendto(p, &bsa, retval);
}

int
linux_recvfrom(p, uap, retval)
	struct proc *p;
	struct linux_recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(void *) buf;
		syscallarg(int) len;
		syscallarg(int) flags;
		syscallarg(struct sockaddr *) from;
		syscallarg(int *) fromlen;
	} */ *uap;
	register_t *retval;
{
	struct linux_recvfrom_args lra;
	struct compat_43_sys_recvfrom_args bra;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lra, sizeof lra)))
		return error;

	SCARG(&bra, s) = lra.s;
	SCARG(&bra, buf) = lra.buf;
	SCARG(&bra, len) = lra.len;
	SCARG(&bra, flags) = lra.flags;
	SCARG(&bra, from) = (caddr_t) lra.from;
	SCARG(&bra, fromlenaddr) = lra.fromlen;

	return compat_43_sys_recvfrom(p, &bra, retval);
}

int
linux_shutdown(p, uap, retval)
	struct proc *p;
	struct linux_shutdown_args /* {
		syscallarg(int) s;
		syscallarg(int) how;
	} */ *uap;
	register_t *retval;
{
	struct linux_shutdown_args lsa;
	struct sys_shutdown_args bsa;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lsa, sizeof lsa)))
		return error;

	SCARG(&bsa, s) = lsa.s;
	SCARG(&bsa, how) = lsa.how;

	return sys_shutdown(p, &bsa, retval);
}

/*
 * Convert socket option level from Linux to NetBSD value. Only SOL_SOCKET
 * is different, the rest matches IPPROTO_* on both systems.
 */
int
linux_to_bsd_sopt_level(llevel)
	int llevel;
{

	switch (llevel) {
	case LINUX_SOL_SOCKET:
		return SOL_SOCKET;
	case LINUX_SOL_IP:
		return IPPROTO_IP;
	case LINUX_SOL_TCP:
		return IPPROTO_TCP;
	case LINUX_SOL_UDP:
		return IPPROTO_UDP;
	default:
		return -1;
	}
}

/*
 * Convert Linux socket level socket option numbers to NetBSD values.
 */
int
linux_to_bsd_so_sockopt(lopt)
	int lopt;
{

	switch (lopt) {
	case LINUX_SO_DEBUG:
		return SO_DEBUG;
	case LINUX_SO_REUSEADDR:
		return SO_REUSEADDR;
	case LINUX_SO_TYPE:
		return SO_TYPE;
	case LINUX_SO_ERROR:
		return SO_ERROR;
	case LINUX_SO_DONTROUTE:
		return SO_DONTROUTE;
	case LINUX_SO_BROADCAST:
		return SO_BROADCAST;
	case LINUX_SO_SNDBUF:
		return SO_SNDBUF;
	case LINUX_SO_RCVBUF:
		return SO_RCVBUF;
	case LINUX_SO_KEEPALIVE:
		return SO_KEEPALIVE;
	case LINUX_SO_OOBINLINE:
		return SO_OOBINLINE;
	case LINUX_SO_LINGER:
		return SO_LINGER;
	case LINUX_SO_PRIORITY:
	case LINUX_SO_NO_CHECK:
	default:
		return -1;
	}
}

/*
 * Convert Linux IP level socket option number to NetBSD values.
 */
int
linux_to_bsd_ip_sockopt(lopt)
	int lopt;
{

	switch (lopt) {
	case LINUX_IP_TOS:
		return IP_TOS;
	case LINUX_IP_TTL:
		return IP_TTL;
	case LINUX_IP_MULTICAST_TTL:
		return IP_MULTICAST_TTL;
	case LINUX_IP_MULTICAST_LOOP:
		return IP_MULTICAST_LOOP;
	case LINUX_IP_MULTICAST_IF:
		return IP_MULTICAST_IF;
	case LINUX_IP_ADD_MEMBERSHIP:
		return IP_ADD_MEMBERSHIP;
	case LINUX_IP_DROP_MEMBERSHIP:
		return IP_DROP_MEMBERSHIP;
	case LINUX_IP_HDRINCL:
		return IP_HDRINCL;
	default:
		return -1;
	}
}

/*
 * Convert Linux TCP level socket option number to NetBSD values.
 */
int
linux_to_bsd_tcp_sockopt(lopt)
	int lopt;
{

	switch (lopt) {
	case LINUX_TCP_NODELAY:
		return TCP_NODELAY;
	case LINUX_TCP_MAXSEG:
		return TCP_MAXSEG;
	default:
		return -1;
	}
}

/*
 * Convert Linux UDP level socket option number to NetBSD values.
 */
int
linux_to_bsd_udp_sockopt(lopt)
	int lopt;
{

	switch (lopt) {
	default:
		return -1;
	}
}

/*
 * Another reasonably straightforward function: setsockopt(2).
 * The level and option numbers are converted; the values passed
 * are not (yet) converted, the ones currently implemented don't
 * need conversion, as they are the same on both systems.
 */
int
linux_setsockopt(p, uap, retval)
	struct proc *p;
	struct linux_setsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) optname;
		syscallarg(void *) optval;
		syscallarg(int) optlen;
	} */ *uap;
	register_t *retval;
{
	struct linux_setsockopt_args lsa;
	struct sys_setsockopt_args bsa;
	int error, name;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lsa, sizeof lsa)))
		return error;

	SCARG(&bsa, s) = lsa.s;
	SCARG(&bsa, level) = linux_to_bsd_sopt_level(lsa.level);
	SCARG(&bsa, val) = lsa.optval;
	SCARG(&bsa, valsize) = lsa.optlen;

	switch (SCARG(&bsa, level)) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(lsa.optname);
		break;
	case IPPROTO_IP:
		name = linux_to_bsd_ip_sockopt(lsa.optname);
		break;
	case IPPROTO_TCP:
		name = linux_to_bsd_tcp_sockopt(lsa.optname);
		break;
	case IPPROTO_UDP:
		name = linux_to_bsd_udp_sockopt(lsa.optname);
		break;
	default:
		return EINVAL;
	}

	if (name == -1)
		return EINVAL;
	SCARG(&bsa, name) = name;

	return sys_setsockopt(p, &bsa, retval);
}

/*
 * getsockopt(2) is very much the same as setsockopt(2) (see above)
 */
int
linux_getsockopt(p, uap, retval)
	struct proc *p;
	struct linux_getsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) optname;
		syscallarg(void *) optval;
		syscallarg(int) *optlen;
	} */ *uap;
	register_t *retval;
{
	struct linux_getsockopt_args lga;
	struct sys_getsockopt_args bga;
	int error, name;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lga, sizeof lga)))
		return error;

	SCARG(&bga, s) = lga.s;
	SCARG(&bga, level) = linux_to_bsd_sopt_level(lga.level);
	SCARG(&bga, val) = lga.optval;
	SCARG(&bga, avalsize) = lga.optlen;

	switch (SCARG(&bga, level)) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(lga.optname);
		break;
	case IPPROTO_IP:
		name = linux_to_bsd_ip_sockopt(lga.optname);
		break;
	case IPPROTO_TCP:
		name = linux_to_bsd_tcp_sockopt(lga.optname);
		break;
	case IPPROTO_UDP:
		name = linux_to_bsd_udp_sockopt(lga.optname);
		break;
	default:
		return EINVAL;
	}

	if (name == -1)
		return EINVAL;
	SCARG(&bga, name) = name;

	return sys_getsockopt(p, &bga, retval);
}

/*
 * Entry point to all Linux socket calls. Just check which call to
 * make and take appropriate action.
 */
int
linux_sys_socketcall(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_socketcall_args /* {
		syscallarg(int) what;
		syscallarg(void *) args;
	} */ *uap = v;

	switch (SCARG(uap, what)) {
	case LINUX_SYS_socket:
		return linux_socket(p, SCARG(uap, args), retval);
	case LINUX_SYS_bind:
		return linux_bind(p, SCARG(uap, args), retval);
	case LINUX_SYS_connect:
		return linux_connect(p, SCARG(uap, args), retval);
	case LINUX_SYS_listen:
		return linux_listen(p, SCARG(uap, args), retval);
	case LINUX_SYS_accept:
		return linux_accept(p, SCARG(uap, args), retval);
	case LINUX_SYS_getsockname:
		return linux_getsockname(p, SCARG(uap, args), retval);
	case LINUX_SYS_getpeername:
		return linux_getpeername(p, SCARG(uap, args), retval);
	case LINUX_SYS_socketpair:
		return linux_socketpair(p, SCARG(uap, args), retval);
	case LINUX_SYS_send:
		return linux_send(p, SCARG(uap, args), retval);
	case LINUX_SYS_recv:
		return linux_recv(p, SCARG(uap, args), retval);
	case LINUX_SYS_sendto:
		return linux_sendto(p, SCARG(uap, args), retval);
	case LINUX_SYS_recvfrom:
		return linux_recvfrom(p, SCARG(uap, args), retval);
	case LINUX_SYS_shutdown:
		return linux_shutdown(p, SCARG(uap, args), retval);
	case LINUX_SYS_setsockopt:
		return linux_setsockopt(p, SCARG(uap, args), retval);
	case LINUX_SYS_getsockopt:
		return linux_getsockopt(p, SCARG(uap, args), retval);
	default:
		return ENOSYS;
	}
}

int
linux_ioctl_socket(p, uap, retval)
	register struct proc *p;
	register struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	register_t *retval;
{
	u_long com;
	struct sys_ioctl_args ia;

	com = SCARG(uap, com);
	retval[0] = 0;

	switch (com) {
	case LINUX_SIOCGIFCONF:
		SCARG(&ia, com) = OSIOCGIFCONF;
		break;
	case LINUX_SIOCGIFFLAGS:
		SCARG(&ia, com) = SIOCGIFFLAGS;
		break;
	case LINUX_SIOCGIFADDR:
		SCARG(&ia, com) = OSIOCGIFADDR;
		break;
	case LINUX_SIOCGIFDSTADDR:
		SCARG(&ia, com) = OSIOCGIFDSTADDR;
		break;
	case LINUX_SIOCGIFBRDADDR:
		SCARG(&ia, com) = OSIOCGIFBRDADDR;
		break;
	case LINUX_SIOCGIFNETMASK:
		SCARG(&ia, com) = OSIOCGIFNETMASK;
		break;
	case LINUX_SIOCGIFMETRIC:
		SCARG(&ia, com) = SIOCGIFMETRIC;
		break;
	case LINUX_SIOCGIFMTU:
		SCARG(&ia, com) = SIOCGIFMTU;
		break;
	case LINUX_SIOCADDMULTI:
		SCARG(&ia, com) = SIOCADDMULTI;
		break;
	case LINUX_SIOCDELMULTI:
		SCARG(&ia, com) = SIOCDELMULTI;
		break;
	case LINUX_SIOCGIFHWADDR: {
		struct linux_ifreq *ifr = (struct linux_ifreq *)SCARG(&ia, data);
		struct sockaddr_dl *sdl;
		struct ifnet *ifp;
		struct ifaddr *ifa;

		/* 
		 * Note that we don't actually respect the name in the ifreq
		 * structure, as Linux interface names are all different.
		 */
		for (ifp = ifnet.tqh_first; ifp != 0;
		    ifp = ifp->if_list.tqe_next) {
			if (ifp->if_type != IFT_ETHER)
				continue;
			for (ifa = ifp->if_addrlist.tqh_first; ifa;
			    ifa = ifa->ifa_list.tqe_next) {
				if ((sdl = (struct sockaddr_dl *)ifa->ifa_addr) &&
				    (sdl->sdl_family == AF_LINK) &&
				    (sdl->sdl_type == IFT_ETHER)) {
					return copyout(LLADDR(sdl),
					    (caddr_t)&ifr->ifr_hwaddr.sa_data,
					    LINUX_IFHWADDRLEN);
				}
			}
		}
		return ENOENT;
	    }
	default:
		return EINVAL;
	}

	SCARG(&ia, fd) = SCARG(uap, fd);
	SCARG(&ia, data) = SCARG(uap, data);
	return sys_ioctl(p, &ia, retval);
}
