/*	$OpenBSD: linux_socket.c,v 1.34 2005/11/21 18:16:37 millert Exp $	*/
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
#include <sys/selinfo.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
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

static int linux_to_bsd_domain (int);
static int bsd_to_linux_domain(int);

int linux_socket(struct proc *, void *, register_t *);
int linux_bind(struct proc *, void *, register_t *);
int linux_connect(struct proc *, void *, register_t *);
int linux_listen(struct proc *, void *, register_t *);
int linux_accept(struct proc *, void *, register_t *);
int linux_getsockname(struct proc *, void *, register_t *);
int linux_getpeername(struct proc *, void *, register_t *);
int linux_socketpair(struct proc *, void *, register_t *);
int linux_send(struct proc *, void *, register_t *);
int linux_recv(struct proc *, void *, register_t *);
int linux_sendto(struct proc *, void *, register_t *);
int linux_recvfrom(struct proc *, void *, register_t *);
int linux_shutdown(struct proc *, void *, register_t *);
int linux_to_bsd_sopt_level(int);
int linux_to_bsd_so_sockopt(int);
int linux_to_bsd_ip_sockopt(int);
int linux_to_bsd_tcp_sockopt(int);
int linux_to_bsd_udp_sockopt(int);
int linux_setsockopt(struct proc *, void *, register_t *);
int linux_getsockopt(struct proc *, void *, register_t *);
int linux_recvmsg(struct proc *, void *, register_t *);
int linux_sendmsg(struct proc *, void *, register_t *);

int linux_check_hdrincl(struct proc *, int, register_t *, caddr_t *);
int linux_sendto_hdrincl(struct proc *, struct sys_sendto_args *,
    register_t *, caddr_t *);

int linux_sa_get(struct proc *, caddr_t *, struct sockaddr **,
    const struct osockaddr *, int *);
int linux_sa_put(struct osockaddr *);

static const int linux_to_bsd_domain_[LINUX_AF_MAX] = {
	AF_UNSPEC,
	AF_UNIX,
	AF_INET,
	-1,		/* LINUX_AF_AX25 */
	AF_IPX,
	AF_APPLETALK,
	-1,		/* LINUX_AF_NETROM */
	-1,		/* LINUX_AF_BRIDGE */
	-1,		/* LINUX_AF_ATMPVC */
	-1,		/* LINUX_AF_X25 */
	AF_INET6,
	-1,		/* LINUX_AF_ROSE */
	AF_DECnet,
	-1,		/* LINUX_AF_NETBEUI */
	-1,		/* LINUX_AF_SECURITY */
	-1,		/* pseudo_AF_KEY */
	AF_ROUTE,	/* LINUX_AF_NETLINK */
	-1,		/* LINUX_AF_PACKET */
	-1,		/* LINUX_AF_ASH */
	-1,		/* LINUX_AF_ECONET */
	-1,		/* LINUX_AF_ATMSVC */
	AF_SNA,
	/* rest up to LINUX_AF_MAX-1 is not allocated */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static const int bsd_to_linux_domain_[AF_MAX] = {
	LINUX_AF_UNSPEC,
	LINUX_AF_UNIX,
	LINUX_AF_INET,
	-1,		/* AF_IMPLINK */
	-1,		/* AF_PUP */
	-1,		/* AF_CHAOS */
	-1,		/* AF_NS */
	-1,		/* AF_ISO */
	-1,		/* AF_ECMA */
	-1,		/* AF_DATAKIT */
	-1,		/* AF_CCITT */
	-1,		/* LINUX_AF_SNA */
	-1,		/* LINUX_AF_DECnet */
	-1,		/* AF_DLI */
	-1,		/* AF_LAT */
	-1,		/* AF_HYLINK */
	LINUX_AF_APPLETALK,
	-1,		/* LINUX_AF_NETLINK */
	-1,		/* AF_LINK */
	-1,		/* AF_XTP */
	-1,		/* AF_COIP */
	-1,		/* AF_CNT */
	-1,		/* pseudo_AF_RTIP */
	LINUX_AF_IPX,
	LINUX_AF_INET6,
	-1,		/* pseudo_AF_PIP */
	-1,		/* AF_ISDN */
	-1,		/* AF_NATM */
	-1,		/* AF_ARP */
	-1,		/* LINUX_pseudo_AF_KEY */
	-1,		/* pseudo_AF_HDRCMPLT */
};

/*
 * Convert between Linux and BSD socket domain values
 */
static int
linux_to_bsd_domain(ldom)
	int ldom;
{
	if (ldom < 0 || ldom >= LINUX_AF_MAX)
		return (-1);

	return linux_to_bsd_domain_[ldom];
}

/*
 * Convert between BSD and Linux socket domain values
 */
static int
bsd_to_linux_domain(bdom)
	int bdom;
{
	if (bdom < 0 || bdom >= AF_MAX)
		return (-1);

	return bsd_to_linux_domain_[bdom];
}

int
linux_socket(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_socket_args /* {
		syscallarg(int)	domain;
		syscallarg(int)	type;
		syscallarg(int) protocol;
	} */ *uap = v;
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
linux_bind(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_bind_args /* {
		syscallarg(int)	s;
		syscallarg(struct sockaddr *) name;
		syscallarg(int)	namelen;
	} */ *uap = v;
	struct linux_bind_args lba;
	struct sys_bind_args bba;
	int error;
	int namlen;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lba, sizeof lba)))
		return error;

	SCARG(&bba, s) = lba.s;
	namlen = lba.namelen;
	if (lba.name) {
		struct sockaddr *sa;
		caddr_t sg = stackgap_init(p->p_emul);

		error = linux_sa_get(p, &sg, &sa, lba.name, &namlen);
		if (error)
			return (error);
		SCARG(&bba, name) = sa;
	} else
		SCARG(&bba, name) = NULL;
	SCARG(&bba, namelen) = namlen;

	return sys_bind(p, &bba, retval);
}

int
linux_connect(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_connect_args /* {
		syscallarg(int)	s;
		syscallarg(struct osockaddr *) name;
		syscallarg(int)	namelen;
	} */ *uap = v;
	struct linux_connect_args lca;
	struct sys_connect_args bca;
	struct sockaddr *sa;
	caddr_t sg = stackgap_init(p->p_emul);
	int namlen;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lca, sizeof lca)))
		return error;

	namlen = lca.namelen;
	error = linux_sa_get(p, &sg, &sa, lca.name, &namlen);
	if (error)
		return (error);

	SCARG(&bca, s) = lca.s;
	SCARG(&bca, name) = sa;
	SCARG(&bca, namelen) = (unsigned int)namlen;

	error = sys_connect(p, &bca, retval);

	if (error == EISCONN) {
		struct sys_getsockopt_args bga;
#if 0
		struct sys_fcntl_args fca;
#endif
		void *status, *statusl;
		int stat, statl = sizeof stat;

#if 0
		SCARG(&fca, fd) = lca.s;
		SCARG(&fca, cmd) = F_GETFL;
		SCARG(&fca, arg) = 0;
		if (sys_fcntl(p, &fca, retval) == -1 ||
		    (*retval & O_NONBLOCK) == 0)
			return error;
#endif

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
linux_listen(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_listen_args /* {
		syscallarg(int) s;
		syscallarg(int) backlog;
	} */ *uap = v;
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
linux_accept(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_accept_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) addr;
		syscallarg(int *) namelen;
	} */ *uap = v;
	struct linux_accept_args laa;
	struct compat_43_sys_accept_args baa;
	struct sys_fcntl_args fca;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &laa, sizeof laa)))
		return error;

	SCARG(&baa, s) = laa.s;
	SCARG(&baa, name) = (caddr_t) laa.addr;
	SCARG(&baa, anamelen) = laa.namelen;

	error = compat_43_sys_accept(p, &baa, retval);
	if (error)
		return (error);

	/*
	 * linux appears not to copy flags from the parent socket to the
	 * accepted one, so we must clear the flags in the new descriptor.
	 * Ignore any errors, because we already have an open fd.
	 */
	SCARG(&fca, fd) = *retval;
	SCARG(&fca, cmd) = F_SETFL;
	SCARG(&fca, arg) = 0;
	(void)sys_fcntl(p, &fca, retval);
	*retval = SCARG(&fca, fd);
	return (0);
}

int
linux_getsockname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_getsockname_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) addr;
		syscallarg(int *) namelen;
	} */ *uap = v;
	struct linux_getsockname_args lga;
	struct sys_getsockname_args bga;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lga, sizeof lga)))
		return error;

	SCARG(&bga, fdes) = lga.s;
	SCARG(&bga, asa) = (struct sockaddr *) lga.addr;
	SCARG(&bga, alen) = lga.namelen;

	error = sys_getsockname(p, &bga, retval);
	if (error)
		return (error);

	if ((error = linux_sa_put((struct osockaddr *)lga.addr)))
		return (error);

	return (0);
}

int
linux_getpeername(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_getpeername_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) addr;
		syscallarg(int *) namelen;
	} */ *uap = v;
	struct linux_getpeername_args lga;
	struct sys_getpeername_args bga;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lga, sizeof lga)))
		return error;

	SCARG(&bga, fdes) = lga.s;
	SCARG(&bga, asa) = (struct sockaddr *) lga.addr;
	SCARG(&bga, alen) = lga.namelen;

	error = sys_getpeername(p, &bga, retval);
	if (error)
		return (error);

	if ((error = linux_sa_put((struct osockaddr *)lga.addr)))
		return (error);

	return (0);
}

int
linux_socketpair(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_socketpair_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
		syscallarg(int *) rsv;
	} */ *uap = v;
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
linux_send(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_send_args /* {
		syscallarg(int) s;
		syscallarg(void *) msg;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap = v;
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
linux_recv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_recv_args /* {
		syscallarg(int) s;
		syscallarg(void *) msg;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap = v;
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
linux_check_hdrincl(p, fd, retval, sgp)
	struct proc *p;
	int fd;
	register_t *retval;
	caddr_t *sgp;
{
	struct sys_getsockopt_args /* {
		int s;
		int level;
		int name;
		caddr_t val;
		int *avalsize;
	} */ gsa;
	int error;
	caddr_t val;
	int *valsize;
	int size_val = sizeof val;
	int optval;

	val = stackgap_alloc(sgp, sizeof(optval));
	valsize = stackgap_alloc(sgp, sizeof(size_val));

	if ((error = copyout(&size_val, valsize, sizeof(size_val))))
		return (error);
	SCARG(&gsa, s) = fd;
	SCARG(&gsa, level) = IPPROTO_IP;
	SCARG(&gsa, name) = IP_HDRINCL;
	SCARG(&gsa, val) = val;
	SCARG(&gsa, avalsize) = valsize;

	if ((error = sys_getsockopt(p, &gsa, retval)))
		return (error);
	if ((error = copyin(val, &optval, sizeof(optval))))
		return (error);
	return (optval == 0);
}

/*
 * linux_ip_copysize defines how many bytes we should copy
 * from the beginning of the IP packet before we customize it for BSD.
 * It should include all the fields we modify (ip_len and ip_off)
 * and be as small as possible to minimize copying overhead.
 */
#define linux_ip_copysize      8

int
linux_sendto_hdrincl(p, bsa, retval, sgp)
	struct proc *p;
	struct sys_sendto_args *bsa;
	register_t *retval;
	caddr_t *sgp;
{
	struct sys_sendmsg_args ssa;
	struct ip *packet, rpacket;
	struct msghdr *msg, rmsg;
	struct iovec *iov, riov[2];
	int error;

	/* Check the packet isn't too small before we mess with it */
	if (SCARG(bsa, len) < linux_ip_copysize)
		return EINVAL;

	/*
	 * Tweaking the user buffer in place would be bad manners.
	 * We create a corrected IP header with just the needed length,
	 * then use an iovec to glue it to the rest of the user packet
	 * when calling sendmsg().
	 */
	packet = (struct ip *)stackgap_alloc(sgp, linux_ip_copysize);
	msg = (struct msghdr *)stackgap_alloc(sgp, sizeof(*msg));
	iov = (struct iovec *)stackgap_alloc(sgp, sizeof(*iov)*2);

	/* Make a copy of the beginning of the packet to be sent */
	if ((error = copyin(SCARG(bsa, buf), (caddr_t)&rpacket,
	    linux_ip_copysize)))
		return error;

	/* Convert fields from Linux to BSD raw IP socket format */
	rpacket.ip_len = SCARG(bsa, len);
	error = copyout(&rpacket, packet, linux_ip_copysize);
	if (error)
		return (error);

	riov[0].iov_base = (char *)packet;
	riov[0].iov_len = linux_ip_copysize;
	riov[1].iov_base = (caddr_t)SCARG(bsa, buf) + linux_ip_copysize;
	riov[1].iov_len = SCARG(bsa, len) - linux_ip_copysize;

	error = copyout(&riov[0], iov, sizeof(riov));
	if (error)
		return (error);

	/* Prepare the msghdr and iovec structures describing the new packet */
	rmsg.msg_name = (void *)SCARG(bsa, to);
	rmsg.msg_namelen = SCARG(bsa, tolen);
	rmsg.msg_iov = iov;
	rmsg.msg_iovlen = 2;
	rmsg.msg_control = NULL;
	rmsg.msg_controllen = 0;
	rmsg.msg_flags = 0;

	error = copyout(&riov[0], iov, sizeof(riov));
	if (error)
		return (error);

	SCARG(&ssa, s) = SCARG(bsa, s);
	SCARG(&ssa, msg) = msg;
	SCARG(&ssa, flags) = SCARG(bsa, flags);
	return sys_sendmsg(p, &ssa, retval);
}

int
linux_sendto(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sendto_args /* {
		syscallarg(int) s;
		syscallarg(void *) msg;
		syscallarg(int) len;
		syscallarg(int) flags;
		syscallarg(osockaddr *) to;
		syscallarg(int) tolen;
	} */ *uap = v;
	struct linux_sendto_args lsa;
	struct sys_sendto_args bsa;
	int error;
	int tolen;
	caddr_t sg = stackgap_init(p->p_emul);

	if ((error = copyin((caddr_t) uap, (caddr_t) &lsa, sizeof lsa)))
		return error;

	SCARG(&bsa, s) = lsa.s;
	SCARG(&bsa, buf) = lsa.msg;
	SCARG(&bsa, len) = lsa.len;
	SCARG(&bsa, flags) = lsa.flags;
	tolen = lsa.tolen;
	if (lsa.to) {
		struct sockaddr *sa;

		if ((error = linux_sa_get(p, &sg, &sa, lsa.to, &tolen)))
			return (error);
		SCARG(&bsa, to) = sa;
	} else
		SCARG(&bsa, to) = NULL;
	SCARG(&bsa, tolen) = tolen;

	if (linux_check_hdrincl(p, lsa.s, retval, &sg) == 0)
		return linux_sendto_hdrincl(p, &bsa, retval, &sg);
	return sys_sendto(p, &bsa, retval);
}

int
linux_recvfrom(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(void *) buf;
		syscallarg(int) len;
		syscallarg(int) flags;
		syscallarg(struct osockaddr *) from;
		syscallarg(int *) fromlen;
	} */ *uap = v;
	struct linux_recvfrom_args lra;
	struct sys_recvfrom_args bra;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lra, sizeof lra)))
		return error;

	SCARG(&bra, s) = lra.s;
	SCARG(&bra, buf) = lra.buf;
	SCARG(&bra, len) = lra.len;
	SCARG(&bra, flags) = lra.flags;
	SCARG(&bra, from) = (struct sockaddr *) lra.from;
	SCARG(&bra, fromlenaddr) = lra.fromlen;

	if ((error = sys_recvfrom(p, &bra, retval)))
		return (error);

	if (lra.from && (error = linux_sa_put(lra.from)))
		return (error);

	return (0);
}

int
linux_shutdown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_shutdown_args /* {
		syscallarg(int) s;
		syscallarg(int) how;
	} */ *uap = v;
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
 * Convert socket option level from Linux to OpenBSD value. Only SOL_SOCKET
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
 * Convert Linux socket level socket option numbers to OpenBSD values.
 */
int
linux_to_bsd_so_sockopt(lopt)
	int lopt;
{

	switch (lopt) {
	case LINUX_SO_DEBUG:
		return SO_DEBUG;
	case LINUX_SO_REUSEADDR:
		/*
		 * Linux does not implement SO_REUSEPORT, but allows reuse
		 * of a host:port pair through SO_REUSEADDR even if the
		 * address is not a multicast-address.  Effectively, this
		 * means that we should use SO_REUSEPORT to allow Linux
		 * applications to not exit with EADDRINUSE.
		 */
		return SO_REUSEPORT;
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
 * Convert Linux IP level socket option number to OpenBSD values.
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
 * Convert Linux TCP level socket option number to OpenBSD values.
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
 * Convert Linux UDP level socket option number to OpenBSD values.
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
linux_setsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_setsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) optname;
		syscallarg(void *) optval;
		syscallarg(int) optlen;
	} */ *uap = v;
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
linux_getsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_getsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) optname;
		syscallarg(void *) optval;
		syscallarg(int) *optlen;
	} */ *uap = v;
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

int
linux_recvmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_recvmsg_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct linux_recvmsg_args lla;
	struct sys_recvmsg_args bla;
	struct msghdr msg;
	int error;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lla, sizeof lla)))
		return error;

	SCARG(&bla, s) = lla.s;
	SCARG(&bla, msg) = (struct msghdr *)lla.msg;
	SCARG(&bla, flags) = lla.flags;

	error = sys_recvmsg(p, &bla, retval);
	if (error)
		return (error);

	error = copyin(lla.msg, (caddr_t)&msg, sizeof(msg));

	if (!error && msg.msg_name && msg.msg_namelen > 2)
		error = linux_sa_put(msg.msg_name);

	return (error);
}

int
linux_sendmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sendmsg_args /* {
		syscallarg(int) s;
		syscallarg(struct msghdr *) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct linux_sendmsg_args lla;
	struct sys_sendmsg_args bla;
	struct msghdr msg, *nmsg = NULL;
	int error;
	caddr_t control;
	int level;

	if ((error = copyin((caddr_t) uap, (caddr_t) &lla, sizeof lla)))
		return error;

	if ((error = copyin(lla.msg, (caddr_t) &msg, sizeof(msg))))
		return (error);

	if (msg.msg_name) {
		struct sockaddr *sa;
		caddr_t sg = stackgap_init(p->p_emul);

		nmsg = (struct msghdr *)stackgap_alloc(&sg,
		    sizeof(struct msghdr));
		if (!nmsg)
			return (ENOMEM);

		error = linux_sa_get(p, &sg, &sa,
		    (struct osockaddr *)msg.msg_name, &msg.msg_namelen);
		if (error)
			return (error);

		msg.msg_name = (struct sockaddr *)sa;
		if ((error = copyout(&msg, nmsg, sizeof(struct msghdr))))
			return (error);
		lla.msg = nmsg;
	}

	SCARG(&bla, s) = lla.s;
	SCARG(&bla, msg) = lla.msg;
	SCARG(&bla, flags) = lla.flags;

	error = copyin(lla.msg->msg_control, &control, sizeof(caddr_t));
	if (error)
		return error;
	if (control == NULL)
		goto done;
	error = copyin(&((struct cmsghdr *)control)->cmsg_level,
	    &level, sizeof(int));
	if (error)
		return error;
	if (level == 1) {
		/*
		 * Linux thinks that SOL_SOCKET is 1; we know that it's really
		 * 0xffff, of course.
		 */
		level = SOL_SOCKET;
		/*
		 * XXX should use stack gap!
		 * We don't because the control header is variable length
		 * up to 2048 bytes, and there's only 512 bytes of gap.
		 */
		error = copyout(&level, &((struct cmsghdr *)control)->
		    cmsg_level, sizeof(int));
		if (error)
			return error;
	}
done:
	error = sys_sendmsg(p, &bla, retval);
	/* replace level, just in case somebody cares. */
	if (level == SOL_SOCKET) {
		level = 1;
		/* don't worry about the error */
		copyout(&level, &((struct cmsghdr *)control)->cmsg_level,
		    sizeof(int));
	}
	return (error);
}

/*
 * Copy the osockaddr structure pointed to by osa to kernel, adjust
 * family and convert to sockaddr, allocate stackgap and put the
 * the converted structure there, address on stackgap returned in sap.
 */
int
linux_sa_get(p, sgp, sap, osa, osalen)
	struct proc *p;
	caddr_t *sgp;
	struct sockaddr **sap;
	const struct osockaddr *osa;
	int *osalen;
{
	int error=0, bdom;
	struct sockaddr *sa, *usa;
	struct osockaddr *kosa;
	int alloclen;
#ifdef INET6
	int oldv6size;
	struct sockaddr_in6 *sin6;
#endif

	if (*osalen < 2 || *osalen > UCHAR_MAX || !osa) {
		return (EINVAL);
	}

	alloclen = *osalen;
#ifdef INET6
	oldv6size = 0;
	/*
	 * Check for old (pre-RFC2553) sockaddr_in6. We may accept it
	 * if it's a v4-mapped address, so reserve the proper space
	 * for it.
	 */
	if (alloclen == sizeof (struct sockaddr_in6) - sizeof (u_int32_t)) {
		alloclen = sizeof (struct sockaddr_in6);
		oldv6size = 1;
	}
#endif

	kosa = (struct osockaddr *) malloc(alloclen, M_TEMP, M_WAITOK);

	if ((error = copyin(osa, (caddr_t) kosa, *osalen))) {
		goto out;
	}

	bdom = linux_to_bsd_domain(kosa->sa_family);
	if (bdom == -1) {
		error = EINVAL;
		goto out;
	}

#ifdef INET6
	/*
	 * Older Linux IPv6 code uses obsolete RFC2133 struct sockaddr_in6,
	 * which lacks the scope id compared with RFC2553 one. If we detect
	 * the situation, reject the address.
	 *
	 * Still accept addresses for which the scope id is not used.
	 */
	if (oldv6size && bdom == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)kosa;
		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr) ||
		    (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_V4COMPAT(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))) {
			sin6->sin6_scope_id = 0;
		} else {
			error = EINVAL;
			goto out;
		}
	} else
#endif 
	if (bdom == AF_INET) {
		alloclen = sizeof(struct sockaddr_in);
	}

	sa = (struct sockaddr *) kosa;
	sa->sa_family = bdom;
	sa->sa_len = alloclen;

	usa = (struct sockaddr *) stackgap_alloc(sgp, alloclen);
	if (!usa) {
		error = ENOMEM;
		goto out;
	}

	if ((error = copyout(sa, usa, alloclen))) {
		goto out;
	}

	*sap = usa;

    out:
	*osalen = alloclen;
	free(kosa, M_TEMP);
	return (error);
}

int
linux_sa_put(osa)
	struct osockaddr *osa;
{
	struct sockaddr sa;
	struct osockaddr *kosa;
	int error, bdom, len;

	/*
	 * Only read/write the sockaddr family and length part, the rest is
	 * not changed.
	 */
	len = sizeof(sa.sa_len) + sizeof(sa.sa_family);

	error = copyin((caddr_t) osa, (caddr_t) &sa, len);
	if (error)
		return (error);

	bdom = bsd_to_linux_domain(sa.sa_family);
	if (bdom == -1)
		return (EINVAL);

	/* Note: we convert from sockaddr to osockaddr here, too */
	kosa = (struct osockaddr *) &sa;
	kosa->sa_family = bdom;
	error = copyout(kosa, osa, len);
	if (error)
		return (error);

	return (0);
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
	case LINUX_SYS_sendmsg:
		return linux_sendmsg(p, SCARG(uap, args), retval);
	case LINUX_SYS_recvmsg:
		return linux_recvmsg(p, SCARG(uap, args), retval);
	default:
		return ENOSYS;
	}
}

int
linux_ioctl_socket(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	u_long com;
	struct sys_ioctl_args ia;
	struct file *fp;
	struct filedesc *fdp;
	struct vnode *vp;
	int (*ioctlf)(struct file *, u_long, caddr_t, struct proc *);
	struct ioctl_pt pt;
	int error = 0, isdev = 0, dosys = 1;

	fdp = p->p_fd;
	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);
	FREF(fp);

	if (fp->f_type == DTYPE_VNODE) {
		vp = (struct vnode *)fp->f_data;
		isdev = vp->v_type == VCHR;
	}

	/*
	 * Don't try to interpret socket ioctl calls that are done
	 * on a device filedescriptor, just pass them through, to
	 * emulate Linux behaviour. Use PTIOCLINUX so that the
	 * device will only handle these if it's prepared to do
	 * so, to avoid unexpected things from happening.
	 */
	if (isdev) {
		dosys = 0;
		ioctlf = fp->f_ops->fo_ioctl;
		pt.com = SCARG(uap, com);
		pt.data = SCARG(uap, data);
		error = ioctlf(fp, PTIOCLINUX, (caddr_t)&pt, p);
		/*
		 * XXX hack: if the function returns EJUSTRETURN,       
		 * it has stuffed a sysctl return value in pt.data.
		 */
		if (error == EJUSTRETURN) {
			retval[0] = (register_t)pt.data;
			error = 0;
		}
		goto out;
	}

	com = SCARG(uap, com);
	retval[0] = 0;

	switch (com) {
	case LINUX_FIOSETOWN:
		SCARG(&ia, com) = FIOSETOWN;
		break;
	case LINUX_SIOCSPGRP:
		SCARG(&ia, com) = SIOCSPGRP;
		break;
	case LINUX_FIOGETOWN:
		SCARG(&ia, com) = FIOGETOWN;
		break;
	case LINUX_SIOCGPGRP:
		SCARG(&ia, com) = SIOCGPGRP;
		break;
	case LINUX_SIOCATMARK:
		SCARG(&ia, com) = SIOCATMARK;
		break;
#if 0
	case LINUX_SIOCGSTAMP:
		SCARG(&ia, com) = SIOCGSTAMP;
		break;
#endif
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
		struct linux_ifreq *ifr = (struct linux_ifreq *)SCARG(uap, data);
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
					error = copyout(LLADDR(sdl),
					    (caddr_t)&ifr->ifr_hwaddr.sa_data,
					    LINUX_IFHWADDRLEN);
					dosys = 0;
					goto out;
				}
			}
		}
		error = ENOENT;
		break;
	    }
	default:
		error = EINVAL;
	}

out:
	if (error == 0 && dosys) {
		SCARG(&ia, fd) = SCARG(uap, fd);
		SCARG(&ia, data) = SCARG(uap, data);
		error = sys_ioctl(p, &ia, retval);
	}

	FRELE(fp);
	return (error);
}
