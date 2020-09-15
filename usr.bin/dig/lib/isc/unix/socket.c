/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/tcp.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <isc/buffer.h>
#include <isc/bufferlist.h>

#include <isc/list.h>
#include <isc/log.h>
#include <isc/region.h>
#include <isc/socket.h>
#include <isc/task.h>
#include <isc/util.h>

#include "errno2result.h"

#include "socket_p.h"
#include "../task_p.h"

struct isc_socketwait {
	fd_set *readset;
	fd_set *writeset;
	int nfds;
	int maxfd;
};

/*
 * Set by the -T dscp option on the command line. If set to a value
 * other than -1, we check to make sure DSCP values match it, and
 * assert if not.
 */
int isc_dscp_check_value = -1;

/*%
 * Some systems define the socket length argument as an int, some as size_t,
 * some as socklen_t.  This is here so it can be easily changed if needed.
 */

/*%
 * Define what the possible "soft" errors can be.  These are non-fatal returns
 * of various network related functions, like recv() and so on.
 *
 * For some reason, BSDI (and perhaps others) will sometimes return <0
 * from recv() but will have errno==0.  This is broken, but we have to
 * work around it here.
 */
#define SOFT_ERROR(e)	((e) == EAGAIN || \
			 (e) == EWOULDBLOCK || \
			 (e) == EINTR || \
			 (e) == 0)

#define DLVL(x) ISC_LOGCATEGORY_GENERAL, ISC_LOGMODULE_SOCKET, ISC_LOG_DEBUG(x)

/*!<
 * DLVL(90)  --  Function entry/exit and other tracing.
 * DLVL(60)  --  Socket data send/receive
 * DLVL(50)  --  Event tracing, including receiving/sending completion events.
 * DLVL(20)  --  Socket creation/destruction.
 */
#define TRACE_LEVEL		90
#define IOEVENT_LEVEL		60
#define EVENT_LEVEL		50
#define CREATION_LEVEL		20

#define TRACE		DLVL(TRACE_LEVEL)
#define IOEVENT		DLVL(IOEVENT_LEVEL)
#define EVENT		DLVL(EVENT_LEVEL)
#define CREATION	DLVL(CREATION_LEVEL)

typedef isc_event_t intev_t;

/*!
 * IPv6 control information.  If the socket is an IPv6 socket we want
 * to collect the destination address and interface so the client can
 * set them on outgoing packets.
 */

/*%
 * NetBSD and FreeBSD can timestamp packets.  XXXMLG Should we have
 * a setsockopt() like interface to request timestamps, and if the OS
 * doesn't do it for us, call gettimeofday() on every UDP receive?
 */

/*%
 * Instead of calculating the cmsgbuf lengths every time we take
 * a rule of thumb approach - sizes are taken from x86_64 linux,
 * multiplied by 2, everything should fit. Those sizes are not
 * large enough to cause any concern.
 */
#define CMSG_SP_IN6PKT 40

#define CMSG_SP_TIMESTAMP 32

#define CMSG_SP_TCTOS 24

#define CMSG_SP_INT 24

#define RECVCMSGBUFLEN (2*(CMSG_SP_IN6PKT + CMSG_SP_TIMESTAMP + CMSG_SP_TCTOS)+1)
#define SENDCMSGBUFLEN (2*(CMSG_SP_IN6PKT + CMSG_SP_INT + CMSG_SP_TCTOS)+1)

/*%
 * The number of times a send operation is repeated if the result is EINTR.
 */
#define NRETRIES 10

struct isc_socket {
	/* Not locked. */
	isc_socketmgr_t	*manager;
	isc_sockettype_t	type;

	/* Locked by socket lock. */
	ISC_LINK(isc_socket_t)	link;
	unsigned int		references;
	int			fd;
	int			pf;

	ISC_LIST(isc_socketevent_t)		send_list;
	ISC_LIST(isc_socketevent_t)		recv_list;
	isc_socket_connev_t		       *connect_ev;

	/*
	 * Internal events.  Posted when a descriptor is readable or
	 * writable.  These are statically allocated and never freed.
	 * They will be set to non-purgable before use.
	 */
	intev_t			readable_ev;
	intev_t			writable_ev;

	struct sockaddr_storage		peer_address;       /* remote address */

	unsigned int		pending_recv : 1,
				pending_send : 1,
				connected : 1,
				connecting : 1,     /* connect pending */
				bound : 1,          /* bound to local addr */
				active : 1,         /* currently active */
				pktdscp : 1;	    /* per packet dscp */
	unsigned int		dscp;
};

struct isc_socketmgr {
	/* Not locked. */
	int			fd_bufsize;
	unsigned int		maxsocks;

	isc_socket_t	       **fds;
	int			*fdstate;

	/* Locked by manager lock. */
	ISC_LIST(isc_socket_t)	socklist;
	fd_set			*read_fds;
	fd_set			*read_fds_copy;
	fd_set			*write_fds;
	fd_set			*write_fds_copy;
	int			maxfd;
	unsigned int		refs;
};

static isc_socketmgr_t *socketmgr = NULL;

#define CLOSED			0	/* this one must be zero */
#define MANAGED			1
#define CLOSE_PENDING		2

/*
 * send() and recv() iovec counts
 */
#define MAXSCATTERGATHER_SEND	(ISC_SOCKET_MAXSCATTERGATHER)
#define MAXSCATTERGATHER_RECV	(ISC_SOCKET_MAXSCATTERGATHER)

static isc_result_t socket_create(isc_socketmgr_t *manager0, int pf,
				  isc_sockettype_t type,
				  isc_socket_t **socketp);
static void send_recvdone_event(isc_socket_t *, isc_socketevent_t **);
static void send_senddone_event(isc_socket_t *, isc_socketevent_t **);
static void free_socket(isc_socket_t **);
static isc_result_t allocate_socket(isc_socketmgr_t *, isc_sockettype_t,
				    isc_socket_t **);
static void destroy(isc_socket_t **);
static void internal_connect(isc_task_t *, isc_event_t *);
static void internal_recv(isc_task_t *, isc_event_t *);
static void internal_send(isc_task_t *, isc_event_t *);
static void process_cmsg(isc_socket_t *, struct msghdr *, isc_socketevent_t *);
static void build_msghdr_send(isc_socket_t *, char *, isc_socketevent_t *,
			      struct msghdr *, struct iovec *, size_t *);
static void build_msghdr_recv(isc_socket_t *, char *, isc_socketevent_t *,
			      struct msghdr *, struct iovec *, size_t *);

#define SELECT_POKE_SHUTDOWN		(-1)
#define SELECT_POKE_READ		(-3)
#define SELECT_POKE_WRITE		(-4)
#define SELECT_POKE_CONNECT		(-4) /*%< Same as _WRITE */
#define SELECT_POKE_CLOSE		(-5)

#define SOCK_DEAD(s)			((s)->references == 0)

/*%
 * Shortcut index arrays to get access to statistics counters.
 */
enum {
	STATID_OPEN = 0,
	STATID_OPENFAIL = 1,
	STATID_CLOSE = 2,
	STATID_BINDFAIL = 3,
	STATID_CONNECTFAIL = 4,
	STATID_CONNECT = 5,
	STATID_ACCEPTFAIL = 6,
	STATID_ACCEPT = 7,
	STATID_SENDFAIL = 8,
	STATID_RECVFAIL = 9,
	STATID_ACTIVE = 10
};

static void
socket_log(isc_socket_t *sock, struct sockaddr_storage *address,
	   isc_logcategory_t *category, isc_logmodule_t *module, int level,
	   const char *fmt, ...) __attribute__((__format__(__printf__, 6, 7)));
static void
socket_log(isc_socket_t *sock, struct sockaddr_storage *address,
	   isc_logcategory_t *category, isc_logmodule_t *module, int level,
	   const char *fmt, ...)
{
	char msgbuf[2048];
	char peerbuf[ISC_SOCKADDR_FORMATSIZE];
	va_list ap;

	if (! isc_log_wouldlog(isc_lctx, level))
		return;

	va_start(ap, fmt);
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	va_end(ap);

	if (address == NULL) {
		isc_log_write(isc_lctx, category, module, level,
			       "socket %p: %s", sock, msgbuf);
	} else {
		isc_sockaddr_format(address, peerbuf, sizeof(peerbuf));
		isc_log_write(isc_lctx, category, module, level,
			       "socket %p %s: %s", sock, peerbuf, msgbuf);
	}
}

static inline isc_result_t
watch_fd(isc_socketmgr_t *manager, int fd, int msg) {
	isc_result_t result = ISC_R_SUCCESS;

	if (msg == SELECT_POKE_READ)
		FD_SET(fd, manager->read_fds);
	if (msg == SELECT_POKE_WRITE)
		FD_SET(fd, manager->write_fds);

	return (result);
}

static inline isc_result_t
unwatch_fd(isc_socketmgr_t *manager, int fd, int msg) {
	isc_result_t result = ISC_R_SUCCESS;

	if (msg == SELECT_POKE_READ)
		FD_CLR(fd, manager->read_fds);
	else if (msg == SELECT_POKE_WRITE)
		FD_CLR(fd, manager->write_fds);

	return (result);
}

static void
wakeup_socket(isc_socketmgr_t *manager, int fd, int msg) {
	isc_result_t result;

	/*
	 * This is a wakeup on a socket.  If the socket is not in the
	 * process of being closed, start watching it for either reads
	 * or writes.
	 */

	INSIST(fd >= 0 && fd < (int)manager->maxsocks);

	if (msg == SELECT_POKE_CLOSE) {
		/* No one should be updating fdstate, so no need to lock it */
		INSIST(manager->fdstate[fd] == CLOSE_PENDING);
		manager->fdstate[fd] = CLOSED;
		(void)unwatch_fd(manager, fd, SELECT_POKE_READ);
		(void)unwatch_fd(manager, fd, SELECT_POKE_WRITE);
		(void)close(fd);
		return;
	}

	if (manager->fdstate[fd] == CLOSE_PENDING) {

		/*
		 * We accept (and ignore) any error from unwatch_fd() as we are
		 * closing the socket, hoping it doesn't leave dangling state in
		 * the kernel.
		 */
		(void)unwatch_fd(manager, fd, SELECT_POKE_READ);
		(void)unwatch_fd(manager, fd, SELECT_POKE_WRITE);
		return;
	}
	if (manager->fdstate[fd] != MANAGED) {
		return;
	}

	/*
	 * Set requested bit.
	 */
	result = watch_fd(manager, fd, msg);
	if (result != ISC_R_SUCCESS) {
		/*
		 * XXXJT: what should we do?  Ignoring the failure of watching
		 * a socket will make the application dysfunctional, but there
		 * seems to be no reasonable recovery process.
		 */
		isc_log_write(isc_lctx, ISC_LOGCATEGORY_GENERAL,
			      ISC_LOGMODULE_SOCKET, ISC_LOG_ERROR,
			      "failed to start watching FD (%d): %s",
			      fd, isc_result_totext(result));
	}
}

/*
 * Update the state of the socketmgr when something changes.
 */
static void
select_poke(isc_socketmgr_t *manager, int fd, int msg) {
	if (msg == SELECT_POKE_SHUTDOWN)
		return;
	else if (fd >= 0)
		wakeup_socket(manager, fd, msg);
	return;
}

/*
 * Make a fd non-blocking.
 */
static isc_result_t
make_nonblock(int fd) {
	int ret;
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, flags);

	if (ret == -1) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "fcntl(%d, F_SETFL, %d): %s", fd, flags,
				 strerror(errno));
		return (ISC_R_UNEXPECTED);
	}

	return (ISC_R_SUCCESS);
}

/*
 * Not all OSes support advanced CMSG macros: CMSG_LEN and CMSG_SPACE.
 * In order to ensure as much portability as possible, we provide wrapper
 * functions of these macros.
 * Note that cmsg_space() could run slow on OSes that do not have
 * CMSG_SPACE.
 */
static inline socklen_t
cmsg_len(socklen_t len) {
	return (CMSG_LEN(len));
}

static inline socklen_t
cmsg_space(socklen_t len) {
	return (CMSG_SPACE(len));
}

/*
 * Process control messages received on a socket.
 */
static void
process_cmsg(isc_socket_t *sock, struct msghdr *msg, isc_socketevent_t *dev) {
	struct cmsghdr *cmsgp;
	struct in6_pktinfo *pktinfop;
	void *timevalp;

	/*
	 * sock is used only when ISC_NET_BSD44MSGHDR and USE_CMSG are defined.
	 * msg and dev are used only when ISC_NET_BSD44MSGHDR is defined.
	 * They are all here, outside of the CPP tests, because it is
	 * more consistent with the usual ISC coding style.
	 */
	UNUSED(sock);
	UNUSED(msg);
	UNUSED(dev);

	if ((msg->msg_flags & MSG_TRUNC) == MSG_TRUNC)
		dev->attributes |= ISC_SOCKEVENTATTR_TRUNC;

	if ((msg->msg_flags & MSG_CTRUNC) == MSG_CTRUNC)
		dev->attributes |= ISC_SOCKEVENTATTR_CTRUNC;

	if (msg->msg_controllen == 0U || msg->msg_control == NULL)
		return;

	timevalp = NULL;
	pktinfop = NULL;

	cmsgp = CMSG_FIRSTHDR(msg);
	while (cmsgp != NULL) {
		socket_log(sock, NULL, TRACE,
			   "processing cmsg %p", cmsgp);

		if (cmsgp->cmsg_level == IPPROTO_IPV6
		    && cmsgp->cmsg_type == IPV6_PKTINFO) {

			pktinfop = (struct in6_pktinfo *)CMSG_DATA(cmsgp);
			memmove(&dev->pktinfo, pktinfop,
				sizeof(struct in6_pktinfo));
			dev->attributes |= ISC_SOCKEVENTATTR_PKTINFO;
			socket_log(sock, NULL, TRACE,
				   "interface received on ifindex %u",
				   dev->pktinfo.ipi6_ifindex);
			if (IN6_IS_ADDR_MULTICAST(&pktinfop->ipi6_addr))
				dev->attributes |= ISC_SOCKEVENTATTR_MULTICAST;
			goto next;
		}

		if (cmsgp->cmsg_level == SOL_SOCKET
		    && cmsgp->cmsg_type == SCM_TIMESTAMP) {
			struct timeval tv;
			timevalp = CMSG_DATA(cmsgp);
			memmove(&tv, timevalp, sizeof(tv));
			TIMEVAL_TO_TIMESPEC(&tv, &dev->timestamp);
			dev->attributes |= ISC_SOCKEVENTATTR_TIMESTAMP;
			goto next;
		}

		if (cmsgp->cmsg_level == IPPROTO_IPV6
		    && cmsgp->cmsg_type == IPV6_TCLASS) {
			dev->dscp = *(int *)CMSG_DATA(cmsgp);
			dev->dscp >>= 2;
			dev->attributes |= ISC_SOCKEVENTATTR_DSCP;
			goto next;
		}

		if (cmsgp->cmsg_level == IPPROTO_IP
		    && (cmsgp->cmsg_type == IP_TOS)) {
			dev->dscp = (int) *(unsigned char *)CMSG_DATA(cmsgp);
			dev->dscp >>= 2;
			dev->attributes |= ISC_SOCKEVENTATTR_DSCP;
			goto next;
		}
	next:
		cmsgp = CMSG_NXTHDR(msg, cmsgp);
	}

}

/*
 * Construct an iov array and attach it to the msghdr passed in.  This is
 * the SEND constructor, which will use the used region of the buffer
 * (if using a buffer list) or will use the internal region (if a single
 * buffer I/O is requested).
 *
 * Nothing can be NULL, and the done event must list at least one buffer
 * on the buffer linked list for this function to be meaningful.
 *
 * If write_countp != NULL, *write_countp will hold the number of bytes
 * this transaction can send.
 */
static void
build_msghdr_send(isc_socket_t *sock, char* cmsgbuf, isc_socketevent_t *dev,
		  struct msghdr *msg, struct iovec *iov, size_t *write_countp)
{
	unsigned int iovcount;
	isc_buffer_t *buffer;
	isc_region_t used;
	size_t write_count;
	size_t skip_count;
	struct cmsghdr *cmsgp;

	memset(msg, 0, sizeof(*msg));

	if (!sock->connected) {
		msg->msg_name = (void *)&dev->address;
		msg->msg_namelen = dev->address.ss_len;
	} else {
		msg->msg_name = NULL;
		msg->msg_namelen = 0;
	}

	buffer = ISC_LIST_HEAD(dev->bufferlist);
	write_count = 0;
	iovcount = 0;

	/*
	 * Single buffer I/O?  Skip what we've done so far in this region.
	 */
	if (buffer == NULL) {
		write_count = dev->region.length - dev->n;
		iov[0].iov_base = (void *)(dev->region.base + dev->n);
		iov[0].iov_len = write_count;
		iovcount = 1;

		goto config;
	}

	/*
	 * Multibuffer I/O.
	 * Skip the data in the buffer list that we have already written.
	 */
	skip_count = dev->n;
	while (buffer != NULL) {
		if (skip_count < isc_buffer_usedlength(buffer))
			break;
		skip_count -= isc_buffer_usedlength(buffer);
		buffer = ISC_LIST_NEXT(buffer, link);
	}

	while (buffer != NULL) {
		INSIST(iovcount < MAXSCATTERGATHER_SEND);

		isc_buffer_usedregion(buffer, &used);

		if (used.length > 0) {
			iov[iovcount].iov_base = (void *)(used.base
							  + skip_count);
			iov[iovcount].iov_len = used.length - skip_count;
			write_count += (used.length - skip_count);
			skip_count = 0;
			iovcount++;
		}
		buffer = ISC_LIST_NEXT(buffer, link);
	}

	INSIST(skip_count == 0U);

 config:
	msg->msg_iov = iov;
	msg->msg_iovlen = iovcount;

	msg->msg_control = NULL;
	msg->msg_controllen = 0;
	msg->msg_flags = 0;

	if ((sock->type == isc_sockettype_udp) &&
	    ((dev->attributes & ISC_SOCKEVENTATTR_PKTINFO) != 0))
	{
		struct in6_pktinfo *pktinfop;

		socket_log(sock, NULL, TRACE,
			   "sendto pktinfo data, ifindex %u",
			   dev->pktinfo.ipi6_ifindex);

		msg->msg_control = (void *)cmsgbuf;
		msg->msg_controllen = cmsg_space(sizeof(struct in6_pktinfo));
		INSIST(msg->msg_controllen <= SENDCMSGBUFLEN);

		cmsgp = (struct cmsghdr *)cmsgbuf;
		cmsgp->cmsg_level = IPPROTO_IPV6;
		cmsgp->cmsg_type = IPV6_PKTINFO;
		cmsgp->cmsg_len = cmsg_len(sizeof(struct in6_pktinfo));
		pktinfop = (struct in6_pktinfo *)CMSG_DATA(cmsgp);
		memmove(pktinfop, &dev->pktinfo, sizeof(struct in6_pktinfo));
	}

	if ((sock->type == isc_sockettype_udp) &&
	    ((dev->attributes & ISC_SOCKEVENTATTR_USEMINMTU) != 0))
	{
		int use_min_mtu = 1;	/* -1, 0, 1 */

		cmsgp = (struct cmsghdr *)(cmsgbuf +
					   msg->msg_controllen);

		msg->msg_control = (void *)cmsgbuf;
		msg->msg_controllen += cmsg_space(sizeof(use_min_mtu));
		INSIST(msg->msg_controllen <= SENDCMSGBUFLEN);

		cmsgp->cmsg_level = IPPROTO_IPV6;
		cmsgp->cmsg_type = IPV6_USE_MIN_MTU;
		cmsgp->cmsg_len = cmsg_len(sizeof(use_min_mtu));
		memmove(CMSG_DATA(cmsgp), &use_min_mtu, sizeof(use_min_mtu));
	}

	if (isc_dscp_check_value > -1) {
		if (sock->type == isc_sockettype_udp)
			INSIST((int)dev->dscp == isc_dscp_check_value);
		else if (sock->type == isc_sockettype_tcp)
			INSIST((int)sock->dscp == isc_dscp_check_value);
	}

	if ((sock->type == isc_sockettype_udp) &&
	    ((dev->attributes & ISC_SOCKEVENTATTR_DSCP) != 0))
	{
		int dscp = (dev->dscp << 2) & 0xff;

		INSIST(dev->dscp < 0x40);

		if (sock->pf == AF_INET && sock->pktdscp) {
			cmsgp = (struct cmsghdr *)(cmsgbuf +
						   msg->msg_controllen);
			msg->msg_control = (void *)cmsgbuf;
			msg->msg_controllen += cmsg_space(sizeof(dscp));
			INSIST(msg->msg_controllen <= SENDCMSGBUFLEN);

			cmsgp->cmsg_level = IPPROTO_IP;
			cmsgp->cmsg_type = IP_TOS;
			cmsgp->cmsg_len = cmsg_len(sizeof(char));
			*(unsigned char*)CMSG_DATA(cmsgp) = dscp;
		} else if (sock->pf == AF_INET && sock->dscp != dev->dscp) {
			if (setsockopt(sock->fd, IPPROTO_IP, IP_TOS,
			       (void *)&dscp, sizeof(int)) < 0)
			{
				UNEXPECTED_ERROR(__FILE__, __LINE__,
						 "setsockopt(%d, IP_TOS, %.02x)"
						 " %s: %s",
						 sock->fd, dscp >> 2,
						 "failed", strerror(errno));
			} else
				sock->dscp = dscp;
		}

		if (sock->pf == AF_INET6 && sock->pktdscp) {
			cmsgp = (struct cmsghdr *)(cmsgbuf +
						   msg->msg_controllen);
			msg->msg_control = (void *)cmsgbuf;
			msg->msg_controllen += cmsg_space(sizeof(dscp));
			INSIST(msg->msg_controllen <= SENDCMSGBUFLEN);

			cmsgp->cmsg_level = IPPROTO_IPV6;
			cmsgp->cmsg_type = IPV6_TCLASS;
			cmsgp->cmsg_len = cmsg_len(sizeof(dscp));
			memmove(CMSG_DATA(cmsgp), &dscp, sizeof(dscp));
		} else if (sock->pf == AF_INET6 && sock->dscp != dev->dscp) {
			if (setsockopt(sock->fd, IPPROTO_IPV6, IPV6_TCLASS,
				       (void *)&dscp, sizeof(int)) < 0) {
				UNEXPECTED_ERROR(__FILE__, __LINE__,
						 "setsockopt(%d, IPV6_TCLASS, "
						 "%.02x) %s: %s",
						 sock->fd, dscp >> 2,
						 "failed", strerror(errno));
			} else
				sock->dscp = dscp;
		}

		if (msg->msg_controllen != 0 &&
		    msg->msg_controllen < SENDCMSGBUFLEN)
		{
			memset(cmsgbuf + msg->msg_controllen, 0,
			       SENDCMSGBUFLEN - msg->msg_controllen);
		}
	}

	if (write_countp != NULL)
		*write_countp = write_count;
}

/*
 * Construct an iov array and attach it to the msghdr passed in.  This is
 * the RECV constructor, which will use the available region of the buffer
 * (if using a buffer list) or will use the internal region (if a single
 * buffer I/O is requested).
 *
 * Nothing can be NULL, and the done event must list at least one buffer
 * on the buffer linked list for this function to be meaningful.
 *
 * If read_countp != NULL, *read_countp will hold the number of bytes
 * this transaction can receive.
 */
static void
build_msghdr_recv(isc_socket_t *sock, char *cmsgbuf, isc_socketevent_t *dev,
		  struct msghdr *msg, struct iovec *iov, size_t *read_countp)
{
	unsigned int iovcount;
	isc_buffer_t *buffer;
	isc_region_t available;
	size_t read_count;

	memset(msg, 0, sizeof(struct msghdr));

	if (sock->type == isc_sockettype_udp) {
		memset(&dev->address, 0, sizeof(dev->address));
		msg->msg_name = (void *)&dev->address;
		msg->msg_namelen = sizeof(dev->address);
	} else { /* TCP */
		msg->msg_name = NULL;
		msg->msg_namelen = 0;
		dev->address = sock->peer_address;
	}

	buffer = ISC_LIST_HEAD(dev->bufferlist);
	read_count = 0;

	/*
	 * Single buffer I/O?  Skip what we've done so far in this region.
	 */
	if (buffer == NULL) {
		read_count = dev->region.length - dev->n;
		iov[0].iov_base = (void *)(dev->region.base + dev->n);
		iov[0].iov_len = read_count;
		iovcount = 1;

		goto config;
	}

	/*
	 * Multibuffer I/O.
	 * Skip empty buffers.
	 */
	while (buffer != NULL) {
		if (isc_buffer_availablelength(buffer) != 0)
			break;
		buffer = ISC_LIST_NEXT(buffer, link);
	}

	iovcount = 0;
	while (buffer != NULL) {
		INSIST(iovcount < MAXSCATTERGATHER_RECV);

		isc_buffer_availableregion(buffer, &available);

		if (available.length > 0) {
			iov[iovcount].iov_base = (void *)(available.base);
			iov[iovcount].iov_len = available.length;
			read_count += available.length;
			iovcount++;
		}
		buffer = ISC_LIST_NEXT(buffer, link);
	}

 config:

	/*
	 * If needed, set up to receive that one extra byte.
	 */
	msg->msg_iov = iov;
	msg->msg_iovlen = iovcount;

	msg->msg_control = cmsgbuf;
	msg->msg_controllen = RECVCMSGBUFLEN;
	msg->msg_flags = 0;

	if (read_countp != NULL)
		*read_countp = read_count;
}

static void
set_dev_address(struct sockaddr_storage *address, isc_socket_t *sock,
		isc_socketevent_t *dev)
{
	if (sock->type == isc_sockettype_udp) {
		if (address != NULL)
			dev->address = *address;
		else
			dev->address = sock->peer_address;
	} else if (sock->type == isc_sockettype_tcp) {
		INSIST(address == NULL);
		dev->address = sock->peer_address;
	}
}

static void
destroy_socketevent(isc_event_t *event) {
	isc_socketevent_t *ev = (isc_socketevent_t *)event;

	INSIST(ISC_LIST_EMPTY(ev->bufferlist));

	(ev->destroy)(event);
}

static isc_socketevent_t *
allocate_socketevent(void *sender,
		     isc_eventtype_t eventtype, isc_taskaction_t action,
		     void *arg)
{
	isc_socketevent_t *ev;

	ev = (isc_socketevent_t *)isc_event_allocate(sender,
						     eventtype, action, arg,
						     sizeof(*ev));

	if (ev == NULL)
		return (NULL);

	ev->result = ISC_R_UNSET;
	ISC_LINK_INIT(ev, ev_link);
	ISC_LIST_INIT(ev->bufferlist);
	ev->region.base = NULL;
	ev->n = 0;
	ev->offset = 0;
	ev->attributes = 0;
	ev->destroy = ev->ev_destroy;
	ev->ev_destroy = destroy_socketevent;
	ev->dscp = 0;

	return (ev);
}

#define DOIO_SUCCESS		0	/* i/o ok, event sent */
#define DOIO_SOFT		1	/* i/o ok, soft error, no event sent */
#define DOIO_HARD		2	/* i/o error, event sent */
#define DOIO_EOF		3	/* EOF, no event sent */

static int
doio_recv(isc_socket_t *sock, isc_socketevent_t *dev) {
	int cc;
	struct iovec iov[MAXSCATTERGATHER_RECV];
	size_t read_count;
	size_t actual_count;
	struct msghdr msghdr;
	isc_buffer_t *buffer;
	int recv_errno;
	union {
		struct msghdr msghdr;
		char m[RECVCMSGBUFLEN];
	} cmsgbuf;
	
	memset(&cmsgbuf, 0, sizeof(cmsgbuf));

	build_msghdr_recv(sock, cmsgbuf.m, dev, &msghdr, iov, &read_count);

	cc = recvmsg(sock->fd, &msghdr, 0);
	recv_errno = errno;

	if (cc < 0) {
		if (SOFT_ERROR(recv_errno))
			return (DOIO_SOFT);

		if (isc_log_wouldlog(isc_lctx, IOEVENT_LEVEL)) {
			socket_log(sock, NULL, IOEVENT,
				  "doio_recv: recvmsg(%d) %d bytes, err %d/%s",
				   sock->fd, cc, recv_errno,
				   strerror(recv_errno));
		}

#define SOFT_OR_HARD(_system, _isc) \
	if (recv_errno == _system) { \
		if (sock->connected) { \
			dev->result = _isc; \
			return (DOIO_HARD); \
		} \
		return (DOIO_SOFT); \
	}
#define ALWAYS_HARD(_system, _isc) \
	if (recv_errno == _system) { \
		dev->result = _isc; \
		return (DOIO_HARD); \
	}

		SOFT_OR_HARD(ECONNREFUSED, ISC_R_CONNREFUSED);
		SOFT_OR_HARD(ENETUNREACH, ISC_R_NETUNREACH);
		SOFT_OR_HARD(EHOSTUNREACH, ISC_R_HOSTUNREACH);
		SOFT_OR_HARD(EHOSTDOWN, ISC_R_HOSTDOWN);
		/* HPUX 11.11 can return EADDRNOTAVAIL. */
		SOFT_OR_HARD(EADDRNOTAVAIL, ISC_R_ADDRNOTAVAIL);
		ALWAYS_HARD(ENOBUFS, ISC_R_NORESOURCES);
		/* Should never get this one but it was seen. */
		SOFT_OR_HARD(ENOPROTOOPT, ISC_R_HOSTUNREACH);
		/*
		 * HPUX returns EPROTO and EINVAL on receiving some ICMP/ICMPv6
		 * errors.
		 */
		SOFT_OR_HARD(EPROTO, ISC_R_HOSTUNREACH);
		SOFT_OR_HARD(EINVAL, ISC_R_HOSTUNREACH);

#undef SOFT_OR_HARD
#undef ALWAYS_HARD

		dev->result = isc__errno2result(recv_errno);
		return (DOIO_HARD);
	}

	/*
	 * On TCP and UNIX sockets, zero length reads indicate EOF,
	 * while on UDP sockets, zero length reads are perfectly valid,
	 * although strange.
	 */
	switch (sock->type) {
	case isc_sockettype_tcp:
		if (cc == 0)
			return (DOIO_EOF);
		break;
	case isc_sockettype_udp:
		break;
	default:
		INSIST(0);
	}

	if (sock->type == isc_sockettype_udp) {
		dev->address.ss_len = msghdr.msg_namelen;
		if (isc_sockaddr_getport(&dev->address) == 0) {
			if (isc_log_wouldlog(isc_lctx, IOEVENT_LEVEL)) {
				socket_log(sock, &dev->address, IOEVENT,
					   "dropping source port zero packet");
			}
			return (DOIO_SOFT);
		}
	}

	socket_log(sock, &dev->address, IOEVENT,
		   "packet received correctly");

	/*
	 * Overflow bit detection.  If we received MORE bytes than we should,
	 * this indicates an overflow situation.  Set the flag in the
	 * dev entry and adjust how much we read by one.
	 */
	/*
	 * If there are control messages attached, run through them and pull
	 * out the interesting bits.
	 */
	process_cmsg(sock, &msghdr, dev);

	/*
	 * update the buffers (if any) and the i/o count
	 */
	dev->n += cc;
	actual_count = cc;
	buffer = ISC_LIST_HEAD(dev->bufferlist);
	while (buffer != NULL && actual_count > 0U) {
		if (isc_buffer_availablelength(buffer) <= actual_count) {
			actual_count -= isc_buffer_availablelength(buffer);
			isc_buffer_add(buffer,
				       isc_buffer_availablelength(buffer));
		} else {
			isc_buffer_add(buffer, actual_count);
			actual_count = 0;
			POST(actual_count);
			break;
		}
		buffer = ISC_LIST_NEXT(buffer, link);
		if (buffer == NULL) {
			INSIST(actual_count == 0U);
		}
	}

	/*
	 * If we read less than we expected, update counters,
	 * and let the upper layer poke the descriptor.
	 */
	if (((size_t)cc != read_count) && (dev->n < dev->minimum))
		return (DOIO_SOFT);

	/*
	 * Full reads are posted, or partials if partials are ok.
	 */
	dev->result = ISC_R_SUCCESS;
	return (DOIO_SUCCESS);
}

/*
 * Returns:
 *	DOIO_SUCCESS	The operation succeeded.  dev->result contains
 *			ISC_R_SUCCESS.
 *
 *	DOIO_HARD	A hard or unexpected I/O error was encountered.
 *			dev->result contains the appropriate error.
 *
 *	DOIO_SOFT	A soft I/O error was encountered.  No senddone
 *			event was sent.  The operation should be retried.
 *
 *	No other return values are possible.
 */
static int
doio_send(isc_socket_t *sock, isc_socketevent_t *dev) {
	int cc;
	struct iovec iov[MAXSCATTERGATHER_SEND];
	size_t write_count;
	struct msghdr msghdr;
	char addrbuf[ISC_SOCKADDR_FORMATSIZE];
	int attempts = 0;
	int send_errno;
	union {
		struct msghdr msghdr;
		char m[SENDCMSGBUFLEN];
	} cmsgbuf;
	
	memset(&cmsgbuf, 0, sizeof(cmsgbuf));

	build_msghdr_send(sock, cmsgbuf.m, dev, &msghdr, iov, &write_count);

 resend:
	cc = sendmsg(sock->fd, &msghdr, 0);
	send_errno = errno;

	/*
	 * Check for error or block condition.
	 */
	if (cc < 0) {
		if (send_errno == EINTR && ++attempts < NRETRIES)
			goto resend;

		if (SOFT_ERROR(send_errno)) {
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				dev->result = ISC_R_WOULDBLOCK;
			return (DOIO_SOFT);
		}

#define SOFT_OR_HARD(_system, _isc) \
	if (send_errno == _system) { \
		if (sock->connected) { \
			dev->result = _isc; \
			return (DOIO_HARD); \
		} \
		return (DOIO_SOFT); \
	}
#define ALWAYS_HARD(_system, _isc) \
	if (send_errno == _system) { \
		dev->result = _isc; \
		return (DOIO_HARD); \
	}

		SOFT_OR_HARD(ECONNREFUSED, ISC_R_CONNREFUSED);
		ALWAYS_HARD(EACCES, ISC_R_NOPERM);
		ALWAYS_HARD(EAFNOSUPPORT, ISC_R_ADDRNOTAVAIL);
		ALWAYS_HARD(EADDRNOTAVAIL, ISC_R_ADDRNOTAVAIL);
		ALWAYS_HARD(EHOSTUNREACH, ISC_R_HOSTUNREACH);
		ALWAYS_HARD(EHOSTDOWN, ISC_R_HOSTUNREACH);
		ALWAYS_HARD(ENETUNREACH, ISC_R_NETUNREACH);
		ALWAYS_HARD(ENOBUFS, ISC_R_NORESOURCES);
		ALWAYS_HARD(EPERM, ISC_R_HOSTUNREACH);
		ALWAYS_HARD(EPIPE, ISC_R_NOTCONNECTED);
		ALWAYS_HARD(ECONNRESET, ISC_R_CONNECTIONRESET);

#undef SOFT_OR_HARD
#undef ALWAYS_HARD

		/*
		 * The other error types depend on whether or not the
		 * socket is UDP or TCP.  If it is UDP, some errors
		 * that we expect to be fatal under TCP are merely
		 * annoying, and are really soft errors.
		 *
		 * However, these soft errors are still returned as
		 * a status.
		 */
		isc_sockaddr_format(&dev->address, addrbuf, sizeof(addrbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__, "internal_send: %s: %s",
				 addrbuf, strerror(send_errno));
		dev->result = isc__errno2result(send_errno);
		return (DOIO_HARD);
	}

	if (cc == 0) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "doio_send: send() %s 0", "returned");
	}

	/*
	 * If we write less than we expected, update counters, poke.
	 */
	dev->n += cc;
	if ((size_t)cc != write_count)
		return (DOIO_SOFT);

	/*
	 * Exactly what we wanted to write.  We're done with this
	 * entry.  Post its completion event.
	 */
	dev->result = ISC_R_SUCCESS;
	return (DOIO_SUCCESS);
}

/*
 * Kill.
 *
 * Caller must ensure that the socket is not locked and no external
 * references exist.
 */
static void
socketclose(isc_socketmgr_t *manager, isc_socket_t *sock, int fd) {
	/*
	 * No one has this socket open, so the watcher doesn't have to be
	 * poked, and the socket doesn't have to be locked.
	 */
	manager->fds[fd] = NULL;
	manager->fdstate[fd] = CLOSE_PENDING;
	select_poke(manager, fd, SELECT_POKE_CLOSE);

	if (sock->active == 1) {
		sock->active = 0;
	}

	/*
	 * update manager->maxfd here (XXX: this should be implemented more
	 * efficiently)
	 */
	if (manager->maxfd == fd) {
		int i;

		manager->maxfd = 0;
		for (i = fd - 1; i >= 0; i--) {
			if (manager->fdstate[i] == MANAGED) {
				manager->maxfd = i;
				break;
			}
		}
	}

}

static void
destroy(isc_socket_t **sockp) {
	int fd;
	isc_socket_t *sock = *sockp;
	isc_socketmgr_t *manager = sock->manager;

	socket_log(sock, NULL, CREATION, "destroying");

	INSIST(ISC_LIST_EMPTY(sock->recv_list));
	INSIST(ISC_LIST_EMPTY(sock->send_list));
	INSIST(sock->connect_ev == NULL);
	INSIST(sock->fd >= -1 && sock->fd < (int)manager->maxsocks);

	if (sock->fd >= 0) {
		fd = sock->fd;
		sock->fd = -1;
		socketclose(manager, sock, fd);
	}

	ISC_LIST_UNLINK(manager->socklist, sock, link);

	/* can't unlock manager as its memory context is still used */
	free_socket(sockp);
}

static isc_result_t
allocate_socket(isc_socketmgr_t *manager, isc_sockettype_t type,
		isc_socket_t **socketp)
{
	isc_socket_t *sock;

	sock = malloc(sizeof(*sock));

	if (sock == NULL)
		return (ISC_R_NOMEMORY);

	sock->references = 0;

	sock->manager = manager;
	sock->type = type;
	sock->fd = -1;
	sock->dscp = 0;		/* TOS/TCLASS is zero until set. */
	sock->active = 0;

	ISC_LINK_INIT(sock, link);

	/*
	 * Set up list of readers and writers to be initially empty.
	 */
	ISC_LIST_INIT(sock->recv_list);
	ISC_LIST_INIT(sock->send_list);
	sock->connect_ev = NULL;
	sock->pending_recv = 0;
	sock->pending_send = 0;
	sock->connected = 0;
	sock->connecting = 0;
	sock->bound = 0;
	sock->pktdscp = 0;

	/*
	 * Initialize readable and writable events.
	 */
	ISC_EVENT_INIT(&sock->readable_ev, sizeof(intev_t),
		       ISC_EVENTATTR_NOPURGE, NULL, ISC_SOCKEVENT_INTR,
		       NULL, sock, sock, NULL);
	ISC_EVENT_INIT(&sock->writable_ev, sizeof(intev_t),
		       ISC_EVENTATTR_NOPURGE, NULL, ISC_SOCKEVENT_INTW,
		       NULL, sock, sock, NULL);

	*socketp = sock;

	return (ISC_R_SUCCESS);
}

/*
 * This event requires that the various lists be empty, that the reference
 * count be 1.  The other socket bits,
 * like the lock, must be initialized as well.  The fd associated must be
 * marked as closed, by setting it to -1 on close, or this routine will
 * also close the socket.
 */
static void
free_socket(isc_socket_t **socketp) {
	isc_socket_t *sock = *socketp;

	INSIST(sock->references == 0);
	INSIST(!sock->connecting);
	INSIST(!sock->pending_recv);
	INSIST(!sock->pending_send);
	INSIST(ISC_LIST_EMPTY(sock->recv_list));
	INSIST(ISC_LIST_EMPTY(sock->send_list));
	INSIST(!ISC_LINK_LINKED(sock, link));

	free(sock);

	*socketp = NULL;
}

static void
use_min_mtu(isc_socket_t *sock) {
	/* use minimum MTU */
	if (sock->pf == AF_INET6) {
		int on = 1;
		(void)setsockopt(sock->fd, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
				(void *)&on, sizeof(on));
	}
}

static void
set_tcp_maxseg(isc_socket_t *sock, int size) {
	if (sock->type == isc_sockettype_tcp)
		(void)setsockopt(sock->fd, IPPROTO_TCP, TCP_MAXSEG,
				(void *)&size, sizeof(size));
}

static isc_result_t
opensocket(isc_socket_t *sock)
{
	isc_result_t result;
	const char *err = "socket";
	int on = 1;

	switch (sock->type) {
	case isc_sockettype_udp:
		sock->fd = socket(sock->pf, SOCK_DGRAM, IPPROTO_UDP);
		break;
	case isc_sockettype_tcp:
		sock->fd = socket(sock->pf, SOCK_STREAM, IPPROTO_TCP);
		break;
	}

	if (sock->fd < 0) {
		switch (errno) {
		case EMFILE:
		case ENFILE:
			isc_log_write(isc_lctx, ISC_LOGCATEGORY_GENERAL,
				       ISC_LOGMODULE_SOCKET, ISC_LOG_ERROR,
				       "%s: %s", err, strerror(errno));
			/* fallthrough */
		case ENOBUFS:
			return (ISC_R_NORESOURCES);

		case EPROTONOSUPPORT:
		case EPFNOSUPPORT:
		case EAFNOSUPPORT:
		/*
		 * Linux 2.2 (and maybe others) return EINVAL instead of
		 * EAFNOSUPPORT.
		 */
		case EINVAL:
			return (ISC_R_FAMILYNOSUPPORT);

		default:
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "%s() %s: %s", err, "failed",
					 strerror(errno));
			return (ISC_R_UNEXPECTED);
		}
	}

	result = make_nonblock(sock->fd);
	if (result != ISC_R_SUCCESS) {
		(void)close(sock->fd);
		return (result);
	}

	/*
	 * Use minimum mtu if possible.
	 */
	if (sock->type == isc_sockettype_tcp && sock->pf == AF_INET6) {
		use_min_mtu(sock);
		set_tcp_maxseg(sock, 1280 - 20 - 40); /* 1280 - TCP - IPV6 */
	}

	if (sock->type == isc_sockettype_udp) {

		if (setsockopt(sock->fd, SOL_SOCKET, SO_TIMESTAMP,
			       (void *)&on, sizeof(on)) < 0
		    && errno != ENOPROTOOPT) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "setsockopt(%d, SO_TIMESTAMP) %s: %s",
					 sock->fd, "failed", strerror(errno));
			/* Press on... */
		}

		/* RFC 3542 */
		if ((sock->pf == AF_INET6)
		    && (setsockopt(sock->fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
				   (void *)&on, sizeof(on)) < 0)) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "setsockopt(%d, IPV6_RECVPKTINFO) "
					 "%s: %s", sock->fd, "failed",
					 strerror(errno));
		}
	}

	if (sock->active == 0) {
		sock->active = 1;
	}

	return (ISC_R_SUCCESS);
}

/*
 * Create a 'type' socket managed
 * by 'manager'.  Events will be posted to 'task' and when dispatched
 * 'action' will be called with 'arg' as the arg value.  The new
 * socket is returned in 'socketp'.
 */
static isc_result_t
socket_create(isc_socketmgr_t *manager0, int pf, isc_sockettype_t type,
	      isc_socket_t **socketp)
{
	isc_socket_t *sock = NULL;
	isc_socketmgr_t *manager = (isc_socketmgr_t *)manager0;
	isc_result_t result;

	REQUIRE(socketp != NULL && *socketp == NULL);

	result = allocate_socket(manager, type, &sock);
	if (result != ISC_R_SUCCESS)
		return (result);

	switch (sock->type) {
	case isc_sockettype_udp:
		sock->pktdscp = 1;
		break;
	case isc_sockettype_tcp:
		break;
	default:
		INSIST(0);
	}

	sock->pf = pf;

	result = opensocket(sock);
	if (result != ISC_R_SUCCESS) {
		free_socket(&sock);
		return (result);
	}

	sock->references = 1;
	*socketp = (isc_socket_t *)sock;

	/*
	 * Note we don't have to lock the socket like we normally would because
	 * there are no external references to it yet.
	 */

	manager->fds[sock->fd] = sock;
	manager->fdstate[sock->fd] = MANAGED;

	ISC_LIST_APPEND(manager->socklist, sock, link);
	if (manager->maxfd < sock->fd)
		manager->maxfd = sock->fd;

	socket_log(sock, NULL, CREATION, "created");

	return (ISC_R_SUCCESS);
}

/*%
 * Create a new 'type' socket managed by 'manager'.  Events
 * will be posted to 'task' and when dispatched 'action' will be
 * called with 'arg' as the arg value.  The new socket is returned
 * in 'socketp'.
 */
isc_result_t
isc_socket_create(isc_socketmgr_t *manager0, int pf, isc_sockettype_t type,
		   isc_socket_t **socketp)
{
	return (socket_create(manager0, pf, type, socketp));
}

/*
 * Attach to a socket.  Caller must explicitly detach when it is done.
 */
void
isc_socket_attach(isc_socket_t *sock0, isc_socket_t **socketp) {
	isc_socket_t *sock = (isc_socket_t *)sock0;

	REQUIRE(socketp != NULL && *socketp == NULL);

	sock->references++;

	*socketp = (isc_socket_t *)sock;
}

/*
 * Dereference a socket.  If this is the last reference to it, clean things
 * up by destroying the socket.
 */
void
isc_socket_detach(isc_socket_t **socketp) {
	isc_socket_t *sock;
	int kill_socket = 0;

	REQUIRE(socketp != NULL);
	sock = (isc_socket_t *)*socketp;

	REQUIRE(sock->references > 0);
	sock->references--;
	if (sock->references == 0)
		kill_socket = 1;

	if (kill_socket)
		destroy(&sock);

	*socketp = NULL;
}

/*
 * I/O is possible on a given socket.  Schedule an event to this task that
 * will call an internal function to do the I/O.  This will charge the
 * task with the I/O operation and let our select loop handler get back
 * to doing something real as fast as possible.
 *
 * The socket and manager must be locked before calling this function.
 */
static void
dispatch_recv(isc_socket_t *sock) {
	intev_t *iev;
	isc_socketevent_t *ev;
	isc_task_t *sender;

	INSIST(!sock->pending_recv);

	ev = ISC_LIST_HEAD(sock->recv_list);
	if (ev == NULL)
		return;
	socket_log(sock, NULL, EVENT,
		   "dispatch_recv:  event %p -> task %p",
		   ev, ev->ev_sender);
	sender = ev->ev_sender;

	sock->pending_recv = 1;
	iev = &sock->readable_ev;

	sock->references++;
	iev->ev_sender = sock;
	iev->ev_action = internal_recv;
	iev->ev_arg = sock;

	isc_task_send(sender, (isc_event_t **)&iev);
}

static void
dispatch_send(isc_socket_t *sock) {
	intev_t *iev;
	isc_socketevent_t *ev;
	isc_task_t *sender;

	INSIST(!sock->pending_send);

	ev = ISC_LIST_HEAD(sock->send_list);
	if (ev == NULL)
		return;
	socket_log(sock, NULL, EVENT,
		   "dispatch_send:  event %p -> task %p",
		   ev, ev->ev_sender);
	sender = ev->ev_sender;

	sock->pending_send = 1;
	iev = &sock->writable_ev;

	sock->references++;
	iev->ev_sender = sock;
	iev->ev_action = internal_send;
	iev->ev_arg = sock;

	isc_task_send(sender, (isc_event_t **)&iev);
}

static void
dispatch_connect(isc_socket_t *sock) {
	intev_t *iev;
	isc_socket_connev_t *ev;

	iev = &sock->writable_ev;

	ev = sock->connect_ev;
	INSIST(ev != NULL); /* XXX */

	INSIST(sock->connecting);

	sock->references++;  /* keep socket around for this internal event */
	iev->ev_sender = sock;
	iev->ev_action = internal_connect;
	iev->ev_arg = sock;

	isc_task_send(ev->ev_sender, (isc_event_t **)&iev);
}

/*
 * Dequeue an item off the given socket's read queue, set the result code
 * in the done event to the one provided, and send it to the task it was
 * destined for.
 *
 * If the event to be sent is on a list, remove it before sending.  If
 * asked to, send and detach from the socket as well.
 *
 * Caller must have the socket locked if the event is attached to the socket.
 */
static void
send_recvdone_event(isc_socket_t *sock, isc_socketevent_t **dev) {
	isc_task_t *task;

	task = (*dev)->ev_sender;

	(*dev)->ev_sender = sock;

	if (ISC_LINK_LINKED(*dev, ev_link))
		ISC_LIST_DEQUEUE(sock->recv_list, *dev, ev_link);

	if (((*dev)->attributes & ISC_SOCKEVENTATTR_ATTACHED)
	    == ISC_SOCKEVENTATTR_ATTACHED)
		isc_task_sendanddetach(&task, (isc_event_t **)dev);
	else
		isc_task_send(task, (isc_event_t **)dev);
}

/*
 * See comments for send_recvdone_event() above.
 *
 * Caller must have the socket locked if the event is attached to the socket.
 */
static void
send_senddone_event(isc_socket_t *sock, isc_socketevent_t **dev) {
	isc_task_t *task;

	INSIST(dev != NULL && *dev != NULL);

	task = (*dev)->ev_sender;
	(*dev)->ev_sender = sock;

	if (ISC_LINK_LINKED(*dev, ev_link))
		ISC_LIST_DEQUEUE(sock->send_list, *dev, ev_link);

	if (((*dev)->attributes & ISC_SOCKEVENTATTR_ATTACHED)
	    == ISC_SOCKEVENTATTR_ATTACHED)
		isc_task_sendanddetach(&task, (isc_event_t **)dev);
	else
		isc_task_send(task, (isc_event_t **)dev);
}

static void
internal_recv(isc_task_t *me, isc_event_t *ev) {
	isc_socketevent_t *dev;
	isc_socket_t *sock;

	INSIST(ev->ev_type == ISC_SOCKEVENT_INTR);

	sock = ev->ev_sender;

	socket_log(sock, NULL, IOEVENT,
		   "internal_recv: task %p got event %p", me, ev);

	INSIST(sock->pending_recv == 1);
	sock->pending_recv = 0;

	INSIST(sock->references > 0);
	sock->references--;  /* the internal event is done with this socket */
	if (sock->references == 0) {
		destroy(&sock);
		return;
	}

	/*
	 * Try to do as much I/O as possible on this socket.  There are no
	 * limits here, currently.
	 */
	dev = ISC_LIST_HEAD(sock->recv_list);
	while (dev != NULL) {
		switch (doio_recv(sock, dev)) {
		case DOIO_SOFT:
			goto poke;

		case DOIO_EOF:
			/*
			 * read of 0 means the remote end was closed.
			 * Run through the event queue and dispatch all
			 * the events with an EOF result code.
			 */
			do {
				dev->result = ISC_R_EOF;
				send_recvdone_event(sock, &dev);
				dev = ISC_LIST_HEAD(sock->recv_list);
			} while (dev != NULL);
			goto poke;

		case DOIO_SUCCESS:
		case DOIO_HARD:
			send_recvdone_event(sock, &dev);
			break;
		}

		dev = ISC_LIST_HEAD(sock->recv_list);
	}

 poke:
	if (!ISC_LIST_EMPTY(sock->recv_list))
		select_poke(sock->manager, sock->fd, SELECT_POKE_READ);
}

static void
internal_send(isc_task_t *me, isc_event_t *ev) {
	isc_socketevent_t *dev;
	isc_socket_t *sock;

	INSIST(ev->ev_type == ISC_SOCKEVENT_INTW);

	/*
	 * Find out what socket this is and lock it.
	 */
	sock = (isc_socket_t *)ev->ev_sender;
	socket_log(sock, NULL, IOEVENT,
		   "internal_send: task %p got event %p", me, ev);

	INSIST(sock->pending_send == 1);
	sock->pending_send = 0;

	INSIST(sock->references > 0);
	sock->references--;  /* the internal event is done with this socket */
	if (sock->references == 0) {
		destroy(&sock);
		return;
	}

	/*
	 * Try to do as much I/O as possible on this socket.  There are no
	 * limits here, currently.
	 */
	dev = ISC_LIST_HEAD(sock->send_list);
	while (dev != NULL) {
		switch (doio_send(sock, dev)) {
		case DOIO_SOFT:
			goto poke;

		case DOIO_HARD:
		case DOIO_SUCCESS:
			send_senddone_event(sock, &dev);
			break;
		}

		dev = ISC_LIST_HEAD(sock->send_list);
	}

 poke:
	if (!ISC_LIST_EMPTY(sock->send_list))
		select_poke(sock->manager, sock->fd, SELECT_POKE_WRITE);
}

/*
 * Process read/writes on each fd here.  Avoid locking
 * and unlocking twice if both reads and writes are possible.
 */
static void
process_fd(isc_socketmgr_t *manager, int fd, int readable,
	   int writeable)
{
	isc_socket_t *sock;
	int unwatch_read = 0, unwatch_write = 0;

	/*
	 * If the socket is going to be closed, don't do more I/O.
	 */
	if (manager->fdstate[fd] == CLOSE_PENDING) {
		(void)unwatch_fd(manager, fd, SELECT_POKE_READ);
		(void)unwatch_fd(manager, fd, SELECT_POKE_WRITE);
		return;
	}

	sock = manager->fds[fd];
	if (readable) {
		if (sock == NULL) {
			unwatch_read = 1;
			goto check_write;
		}
		if (!SOCK_DEAD(sock)) {
			dispatch_recv(sock);
		}
		unwatch_read = 1;
	}
check_write:
	if (writeable) {
		if (sock == NULL) {
			unwatch_write = 1;
			goto unlock_fd;
		}
		if (!SOCK_DEAD(sock)) {
			if (sock->connecting)
				dispatch_connect(sock);
			else
				dispatch_send(sock);
		}
		unwatch_write = 1;
	}

 unlock_fd:
	if (unwatch_read)
		(void)unwatch_fd(manager, fd, SELECT_POKE_READ);
	if (unwatch_write)
		(void)unwatch_fd(manager, fd, SELECT_POKE_WRITE);

}

static void
process_fds(isc_socketmgr_t *manager, int maxfd, fd_set *readfds,
	    fd_set *writefds)
{
	int i;

	REQUIRE(maxfd <= (int)manager->maxsocks);

	for (i = 0; i < maxfd; i++) {
		process_fd(manager, i, FD_ISSET(i, readfds),
			   FD_ISSET(i, writefds));
	}
}

/*
 * Create a new socket manager.
 */

static isc_result_t
setup_watcher(isc_socketmgr_t *manager) {
	isc_result_t result;

	UNUSED(result);

	manager->fd_bufsize = sizeof(fd_set);

	manager->read_fds = NULL;
	manager->read_fds_copy = NULL;
	manager->write_fds = NULL;
	manager->write_fds_copy = NULL;

	manager->read_fds = malloc(manager->fd_bufsize);
	if (manager->read_fds != NULL)
		manager->read_fds_copy = malloc(manager->fd_bufsize);
	if (manager->read_fds_copy != NULL)
		manager->write_fds = malloc(manager->fd_bufsize);
	if (manager->write_fds != NULL) {
		manager->write_fds_copy = malloc(manager->fd_bufsize);
	}
	if (manager->write_fds_copy == NULL) {
		if (manager->write_fds != NULL) {
			free(manager->write_fds);
		}
		if (manager->read_fds_copy != NULL) {
			free(manager->read_fds_copy);
		}
		if (manager->read_fds != NULL) {
			free(manager->read_fds);
		}
		return (ISC_R_NOMEMORY);
	}
	memset(manager->read_fds, 0, manager->fd_bufsize);
	memset(manager->write_fds, 0, manager->fd_bufsize);

	manager->maxfd = 0;

	return (ISC_R_SUCCESS);
}

static void
cleanup_watcher(isc_socketmgr_t *manager) {

	if (manager->read_fds != NULL)
		free(manager->read_fds);
	if (manager->read_fds_copy != NULL)
		free(manager->read_fds_copy);
	if (manager->write_fds != NULL)
		free(manager->write_fds);
	if (manager->write_fds_copy != NULL)
		free(manager->write_fds_copy);
}

static isc_result_t
isc_socketmgr_create2(isc_socketmgr_t **managerp,
		       unsigned int maxsocks)
{
	isc_socketmgr_t *manager;
	isc_result_t result;

	REQUIRE(managerp != NULL && *managerp == NULL);

	if (socketmgr != NULL) {
		/* Don't allow maxsocks to be updated */
		if (maxsocks > 0 && socketmgr->maxsocks != maxsocks)
			return (ISC_R_EXISTS);

		socketmgr->refs++;
		*managerp = (isc_socketmgr_t *)socketmgr;
		return (ISC_R_SUCCESS);
	}

	if (maxsocks == 0)
		maxsocks = FD_SETSIZE;

	manager = malloc(sizeof(*manager));
	if (manager == NULL)
		return (ISC_R_NOMEMORY);

	/* zero-clear so that necessary cleanup on failure will be easy */
	memset(manager, 0, sizeof(*manager));
	manager->maxsocks = maxsocks;
	manager->fds = reallocarray(NULL, manager->maxsocks, sizeof(isc_socket_t *));
	if (manager->fds == NULL) {
		result = ISC_R_NOMEMORY;
		goto free_manager;
	}
	manager->fdstate = reallocarray(NULL, manager->maxsocks, sizeof(int));
	if (manager->fdstate == NULL) {
		result = ISC_R_NOMEMORY;
		goto free_manager;
	}

	memset(manager->fds, 0, manager->maxsocks * sizeof(isc_socket_t *));
	ISC_LIST_INIT(manager->socklist);

	manager->refs = 1;

	/*
	 * Set up initial state for the select loop
	 */
	result = setup_watcher(manager);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	memset(manager->fdstate, 0, manager->maxsocks * sizeof(int));

	socketmgr = manager;
	*managerp = (isc_socketmgr_t *)manager;

	return (ISC_R_SUCCESS);

cleanup:

free_manager:
	if (manager->fdstate != NULL) {
		free(manager->fdstate);
	}
	if (manager->fds != NULL) {
		free(manager->fds);
	}
	free(manager);

	return (result);
}

isc_result_t
isc_socketmgr_create(isc_socketmgr_t **managerp) {
	return (isc_socketmgr_create2(managerp, 0));
}

void
isc_socketmgr_destroy(isc_socketmgr_t **managerp) {
	isc_socketmgr_t *manager;
	int i;

	/*
	 * Destroy a socket manager.
	 */

	REQUIRE(managerp != NULL);
	manager = (isc_socketmgr_t *)*managerp;

	manager->refs--;
	if (manager->refs > 0) {
		*managerp = NULL;
		return;
	}
	socketmgr = NULL;

	/*
	 * Wait for all sockets to be destroyed.
	 */
	while (!ISC_LIST_EMPTY(manager->socklist)) {
		isc_taskmgr_dispatch(NULL);
	}

	/*
	 * Here, poke our select/poll thread.  Do this by closing the write
	 * half of the pipe, which will send EOF to the read half.
	 * This is currently a no-op in the non-threaded case.
	 */
	select_poke(manager, 0, SELECT_POKE_SHUTDOWN);

	/*
	 * Clean up.
	 */
	cleanup_watcher(manager);

	for (i = 0; i < (int)manager->maxsocks; i++)
		if (manager->fdstate[i] == CLOSE_PENDING) /* no need to lock */
			(void)close(i);

	free(manager->fds);
	free(manager->fdstate);

	free(manager);

	*managerp = NULL;

	socketmgr = NULL;
}

static isc_result_t
socket_recv(isc_socket_t *sock, isc_socketevent_t *dev, isc_task_t *task,
	    unsigned int flags)
{
	int io_state;
	isc_task_t *ntask = NULL;
	isc_result_t result = ISC_R_SUCCESS;

	dev->ev_sender = task;

	if (sock->type == isc_sockettype_udp) {
		io_state = doio_recv(sock, dev);
	} else {
		if (ISC_LIST_EMPTY(sock->recv_list))
			io_state = doio_recv(sock, dev);
		else
			io_state = DOIO_SOFT;
	}

	switch (io_state) {
	case DOIO_SOFT:
		/*
		 * We couldn't read all or part of the request right now, so
		 * queue it.
		 *
		 * Attach to socket and to task
		 */
		isc_task_attach(task, &ntask);
		dev->attributes |= ISC_SOCKEVENTATTR_ATTACHED;

		/*
		 * Enqueue the request.  If the socket was previously not being
		 * watched, poke the watcher to start paying attention to it.
		 */
		if (ISC_LIST_EMPTY(sock->recv_list) && !sock->pending_recv)
			select_poke(sock->manager, sock->fd, SELECT_POKE_READ);
		ISC_LIST_ENQUEUE(sock->recv_list, dev, ev_link);

		socket_log(sock, NULL, EVENT,
			   "socket_recv: event %p -> task %p",
			   dev, ntask);

		if ((flags & ISC_SOCKFLAG_IMMEDIATE) != 0)
			result = ISC_R_INPROGRESS;
		break;

	case DOIO_EOF:
		dev->result = ISC_R_EOF;
		/* fallthrough */

	case DOIO_HARD:
	case DOIO_SUCCESS:
		if ((flags & ISC_SOCKFLAG_IMMEDIATE) == 0)
			send_recvdone_event(sock, &dev);
		break;
	}

	return (result);
}

isc_result_t
isc_socket_recvv(isc_socket_t *sock0, isc_bufferlist_t *buflist,
		  unsigned int minimum, isc_task_t *task,
		  isc_taskaction_t action, void *arg)
{
	isc_socket_t *sock = (isc_socket_t *)sock0;
	isc_socketevent_t *dev;
	unsigned int iocount;
	isc_buffer_t *buffer;

	REQUIRE(buflist != NULL);
	REQUIRE(!ISC_LIST_EMPTY(*buflist));
	REQUIRE(task != NULL);
	REQUIRE(action != NULL);

	iocount = isc_bufferlist_availablecount(buflist);
	REQUIRE(iocount > 0);

	INSIST(sock->bound);

	dev = allocate_socketevent(sock,
				   ISC_SOCKEVENT_RECVDONE, action, arg);
	if (dev == NULL)
		return (ISC_R_NOMEMORY);

	/*
	 * UDP sockets are always partial read
	 */
	if (sock->type == isc_sockettype_udp)
		dev->minimum = 1;
	else {
		if (minimum == 0)
			dev->minimum = iocount;
		else
			dev->minimum = minimum;
	}

	/*
	 * Move each buffer from the passed in list to our internal one.
	 */
	buffer = ISC_LIST_HEAD(*buflist);
	while (buffer != NULL) {
		ISC_LIST_DEQUEUE(*buflist, buffer, link);
		ISC_LIST_ENQUEUE(dev->bufferlist, buffer, link);
		buffer = ISC_LIST_HEAD(*buflist);
	}

	return (socket_recv(sock, dev, task, 0));
}

static isc_result_t
socket_send(isc_socket_t *sock, isc_socketevent_t *dev, isc_task_t *task,
	    struct sockaddr_storage *address, struct in6_pktinfo *pktinfo,
	    unsigned int flags)
{
	int io_state;
	isc_task_t *ntask = NULL;
	isc_result_t result = ISC_R_SUCCESS;

	dev->ev_sender = task;

	set_dev_address(address, sock, dev);
	if (pktinfo != NULL) {
		dev->attributes |= ISC_SOCKEVENTATTR_PKTINFO;
		dev->pktinfo = *pktinfo;

		if (!isc_sockaddr_issitelocal(&dev->address) &&
		    !isc_sockaddr_islinklocal(&dev->address)) {
			socket_log(sock, NULL, TRACE,
				   "pktinfo structure provided, ifindex %u "
				   "(set to 0)", pktinfo->ipi6_ifindex);

			/*
			 * Set the pktinfo index to 0 here, to let the
			 * kernel decide what interface it should send on.
			 */
			dev->pktinfo.ipi6_ifindex = 0;
		}
	}

	if (sock->type == isc_sockettype_udp)
		io_state = doio_send(sock, dev);
	else {
		if (ISC_LIST_EMPTY(sock->send_list))
			io_state = doio_send(sock, dev);
		else
			io_state = DOIO_SOFT;
	}

	switch (io_state) {
	case DOIO_SOFT:
		/*
		 * We couldn't send all or part of the request right now, so
		 * queue it unless ISC_SOCKFLAG_NORETRY is set.
		 */
		if ((flags & ISC_SOCKFLAG_NORETRY) == 0) {
			isc_task_attach(task, &ntask);
			dev->attributes |= ISC_SOCKEVENTATTR_ATTACHED;

			/*
			 * Enqueue the request.  If the socket was previously
			 * not being watched, poke the watcher to start
			 * paying attention to it.
			 */
			if (ISC_LIST_EMPTY(sock->send_list) &&
			    !sock->pending_send)
				select_poke(sock->manager, sock->fd,
					    SELECT_POKE_WRITE);
			ISC_LIST_ENQUEUE(sock->send_list, dev, ev_link);

			socket_log(sock, NULL, EVENT,
				   "socket_send: event %p -> task %p",
				   dev, ntask);

			if ((flags & ISC_SOCKFLAG_IMMEDIATE) != 0)
				result = ISC_R_INPROGRESS;
			break;
		}

		/* FALLTHROUGH */

	case DOIO_HARD:
	case DOIO_SUCCESS:
		if ((flags & ISC_SOCKFLAG_IMMEDIATE) == 0)
			send_senddone_event(sock, &dev);
		break;
	}

	return (result);
}

isc_result_t
isc_socket_sendv(isc_socket_t *sock, isc_bufferlist_t *buflist,
		  isc_task_t *task, isc_taskaction_t action, void *arg)
{
	return (isc_socket_sendtov2(sock, buflist, task, action, arg, NULL,
				     NULL, 0));
}

isc_result_t
isc_socket_sendtov2(isc_socket_t *sock0, isc_bufferlist_t *buflist,
		     isc_task_t *task, isc_taskaction_t action, void *arg,
		     struct sockaddr_storage *address, struct in6_pktinfo *pktinfo,
		     unsigned int flags)
{
	isc_socket_t *sock = (isc_socket_t *)sock0;
	isc_socketevent_t *dev;
	unsigned int iocount;
	isc_buffer_t *buffer;

	REQUIRE(buflist != NULL);
	REQUIRE(!ISC_LIST_EMPTY(*buflist));
	REQUIRE(task != NULL);
	REQUIRE(action != NULL);

	iocount = isc_bufferlist_usedcount(buflist);
	REQUIRE(iocount > 0);

	dev = allocate_socketevent(sock,
				   ISC_SOCKEVENT_SENDDONE, action, arg);
	if (dev == NULL)
		return (ISC_R_NOMEMORY);

	/*
	 * Move each buffer from the passed in list to our internal one.
	 */
	buffer = ISC_LIST_HEAD(*buflist);
	while (buffer != NULL) {
		ISC_LIST_DEQUEUE(*buflist, buffer, link);
		ISC_LIST_ENQUEUE(dev->bufferlist, buffer, link);
		buffer = ISC_LIST_HEAD(*buflist);
	}

	return (socket_send(sock, dev, task, address, pktinfo, flags));
}

isc_result_t
isc_socket_bind(isc_socket_t *sock0, struct sockaddr_storage *sockaddr,
		 unsigned int options) {
	isc_socket_t *sock = (isc_socket_t *)sock0;
	int on = 1;

	INSIST(!sock->bound);

	if (sock->pf != sockaddr->ss_family) {
		return (ISC_R_FAMILYMISMATCH);
	}

	/*
	 * Only set SO_REUSEADDR when we want a specific port.
	 */
	if ((options & ISC_SOCKET_REUSEADDRESS) != 0 &&
	    isc_sockaddr_getport(sockaddr) != (in_port_t)0 &&
	    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (void *)&on,
		       sizeof(on)) < 0) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "setsockopt(%d) %s", sock->fd, "failed");
		/* Press on... */
	}
	if (bind(sock->fd, (struct sockaddr *)sockaddr, sockaddr->ss_len) < 0) {
		switch (errno) {
		case EACCES:
			return (ISC_R_NOPERM);
		case EADDRNOTAVAIL:
			return (ISC_R_ADDRNOTAVAIL);
		case EADDRINUSE:
			return (ISC_R_ADDRINUSE);
		case EINVAL:
			return (ISC_R_BOUND);
		default:
			UNEXPECTED_ERROR(__FILE__, __LINE__, "bind: %s",
					 strerror(errno));
			return (ISC_R_UNEXPECTED);
		}
	}

	socket_log(sock, sockaddr, TRACE, "bound");
	sock->bound = 1;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_socket_connect(isc_socket_t *sock0, struct sockaddr_storage *addr,
		   isc_task_t *task, isc_taskaction_t action, void *arg)
{
	isc_socket_t *sock = (isc_socket_t *)sock0;
	isc_socket_connev_t *dev;
	isc_task_t *ntask = NULL;
	isc_socketmgr_t *manager;
	int cc;
	char addrbuf[ISC_SOCKADDR_FORMATSIZE];

	REQUIRE(addr != NULL);
	REQUIRE(task != NULL);
	REQUIRE(action != NULL);

	manager = sock->manager;
	REQUIRE(addr != NULL);

	if (isc_sockaddr_ismulticast(addr))
		return (ISC_R_MULTICAST);

	REQUIRE(!sock->connecting);

	dev = (isc_socket_connev_t *)isc_event_allocate(sock,
							ISC_SOCKEVENT_CONNECT,
							action,	arg,
							sizeof(*dev));
	if (dev == NULL) {
		return (ISC_R_NOMEMORY);
	}
	ISC_LINK_INIT(dev, ev_link);

	/*
	 * Try to do the connect right away, as there can be only one
	 * outstanding, and it might happen to complete.
	 */
	sock->peer_address = *addr;
	cc = connect(sock->fd, (struct sockaddr *)addr, addr->ss_len);
	if (cc < 0) {
		/*
		 * HP-UX "fails" to connect a UDP socket and sets errno to
		 * EINPROGRESS if it's non-blocking.  We'd rather regard this as
		 * a success and let the user detect it if it's really an error
		 * at the time of sending a packet on the socket.
		 */
		if (sock->type == isc_sockettype_udp && errno == EINPROGRESS) {
			cc = 0;
			goto success;
		}
		if (SOFT_ERROR(errno) || errno == EINPROGRESS)
			goto queue;

		switch (errno) {
#define ERROR_MATCH(a, b) case a: dev->result = b; goto err_exit;
			ERROR_MATCH(EACCES, ISC_R_NOPERM);
			ERROR_MATCH(EADDRNOTAVAIL, ISC_R_ADDRNOTAVAIL);
			ERROR_MATCH(EAFNOSUPPORT, ISC_R_ADDRNOTAVAIL);
			ERROR_MATCH(ECONNREFUSED, ISC_R_CONNREFUSED);
			ERROR_MATCH(EHOSTUNREACH, ISC_R_HOSTUNREACH);
			ERROR_MATCH(EHOSTDOWN, ISC_R_HOSTUNREACH);
			ERROR_MATCH(ENETUNREACH, ISC_R_NETUNREACH);
			ERROR_MATCH(ENOBUFS, ISC_R_NORESOURCES);
			ERROR_MATCH(EPERM, ISC_R_HOSTUNREACH);
			ERROR_MATCH(EPIPE, ISC_R_NOTCONNECTED);
			ERROR_MATCH(ECONNRESET, ISC_R_CONNECTIONRESET);
#undef ERROR_MATCH
		}

		sock->connected = 0;

		isc_sockaddr_format(addr, addrbuf, sizeof(addrbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__, "connect(%s) %d/%s",
				 addrbuf, errno, strerror(errno));

		isc_event_free(ISC_EVENT_PTR(&dev));
		return (ISC_R_UNEXPECTED);

	err_exit:
		sock->connected = 0;
		isc_task_send(task, ISC_EVENT_PTR(&dev));

		return (ISC_R_SUCCESS);
	}

	/*
	 * If connect completed, fire off the done event.
	 */
 success:
	if (cc == 0) {
		sock->connected = 1;
		sock->bound = 1;
		dev->result = ISC_R_SUCCESS;
		isc_task_send(task, ISC_EVENT_PTR(&dev));

		return (ISC_R_SUCCESS);
	}

 queue:

	/*
	 * Attach to task.
	 */
	isc_task_attach(task, &ntask);

	sock->connecting = 1;

	dev->ev_sender = ntask;

	/*
	 * Poke watcher here.  We still have the socket locked, so there
	 * is no race condition.  We will keep the lock for such a short
	 * bit of time waking it up now or later won't matter all that much.
	 */
	if (sock->connect_ev == NULL)
		select_poke(manager, sock->fd, SELECT_POKE_CONNECT);

	sock->connect_ev = dev;

	return (ISC_R_SUCCESS);
}

/*
 * Called when a socket with a pending connect() finishes.
 */
static void
internal_connect(isc_task_t *me, isc_event_t *ev) {
	isc_socket_t *sock;
	isc_socket_connev_t *dev;
	isc_task_t *task;
	int cc;
	socklen_t optlen;
	char peerbuf[ISC_SOCKADDR_FORMATSIZE];

	UNUSED(me);
	INSIST(ev->ev_type == ISC_SOCKEVENT_INTW);

	sock = ev->ev_sender;

	/*
	 * When the internal event was sent the reference count was bumped
	 * to keep the socket around for us.  Decrement the count here.
	 */
	INSIST(sock->references > 0);
	sock->references--;
	if (sock->references == 0) {
		destroy(&sock);
		return;
	}

	/*
	 * Has this event been canceled?
	 */
	dev = sock->connect_ev;
	if (dev == NULL) {
		INSIST(!sock->connecting);
		return;
	}

	INSIST(sock->connecting);
	sock->connecting = 0;

	/*
	 * Get any possible error status here.
	 */
	optlen = sizeof(cc);
	if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR,
		       (void *)&cc, (void *)&optlen) < 0)
		cc = errno;
	else
		errno = cc;

	if (errno != 0) {
		/*
		 * If the error is EAGAIN, just re-select on this
		 * fd and pretend nothing strange happened.
		 */
		if (SOFT_ERROR(errno) || errno == EINPROGRESS) {
			sock->connecting = 1;
			select_poke(sock->manager, sock->fd,
				    SELECT_POKE_CONNECT);
			return;
		}

		/*
		 * Translate other errors into ISC_R_* flavors.
		 */
		switch (errno) {
#define ERROR_MATCH(a, b) case a: dev->result = b; break;
			ERROR_MATCH(EACCES, ISC_R_NOPERM);
			ERROR_MATCH(EADDRNOTAVAIL, ISC_R_ADDRNOTAVAIL);
			ERROR_MATCH(EAFNOSUPPORT, ISC_R_ADDRNOTAVAIL);
			ERROR_MATCH(ECONNREFUSED, ISC_R_CONNREFUSED);
			ERROR_MATCH(EHOSTUNREACH, ISC_R_HOSTUNREACH);
			ERROR_MATCH(EHOSTDOWN, ISC_R_HOSTUNREACH);
			ERROR_MATCH(ENETUNREACH, ISC_R_NETUNREACH);
			ERROR_MATCH(ENOBUFS, ISC_R_NORESOURCES);
			ERROR_MATCH(EPERM, ISC_R_HOSTUNREACH);
			ERROR_MATCH(EPIPE, ISC_R_NOTCONNECTED);
			ERROR_MATCH(ETIMEDOUT, ISC_R_TIMEDOUT);
			ERROR_MATCH(ECONNRESET, ISC_R_CONNECTIONRESET);
#undef ERROR_MATCH
		default:
			dev->result = ISC_R_UNEXPECTED;
			isc_sockaddr_format(&sock->peer_address, peerbuf,
					    sizeof(peerbuf));
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "internal_connect: connect(%s) %s",
					 peerbuf, strerror(errno));
		}
	} else {
		dev->result = ISC_R_SUCCESS;
		sock->connected = 1;
		sock->bound = 1;
	}

	sock->connect_ev = NULL;

	task = dev->ev_sender;
	dev->ev_sender = sock;
	isc_task_sendanddetach(&task, ISC_EVENT_PTR(&dev));
}

/*
 * Run through the list of events on this socket, and cancel the ones
 * queued for task "task" of type "how".  "how" is a bitmask.
 */
void
isc_socket_cancel(isc_socket_t *sock0, isc_task_t *task, unsigned int how) {
	isc_socket_t *sock = (isc_socket_t *)sock0;

	/*
	 * Quick exit if there is nothing to do.  Don't even bother locking
	 * in this case.
	 */
	if (how == 0)
		return;

	/*
	 * All of these do the same thing, more or less.
	 * Each will:
	 *	o If the internal event is marked as "posted" try to
	 *	  remove it from the task's queue.  If this fails, mark it
	 *	  as canceled instead, and let the task clean it up later.
	 *	o For each I/O request for that task of that type, post
	 *	  its done event with status of "ISC_R_CANCELED".
	 *	o Reset any state needed.
	 */
	if (((how & ISC_SOCKCANCEL_RECV) == ISC_SOCKCANCEL_RECV)
	    && !ISC_LIST_EMPTY(sock->recv_list)) {
		isc_socketevent_t      *dev;
		isc_socketevent_t      *next;
		isc_task_t	       *current_task;

		dev = ISC_LIST_HEAD(sock->recv_list);

		while (dev != NULL) {
			current_task = dev->ev_sender;
			next = ISC_LIST_NEXT(dev, ev_link);

			if ((task == NULL) || (task == current_task)) {
				dev->result = ISC_R_CANCELED;
				send_recvdone_event(sock, &dev);
			}
			dev = next;
		}
	}

	if (((how & ISC_SOCKCANCEL_SEND) == ISC_SOCKCANCEL_SEND)
	    && !ISC_LIST_EMPTY(sock->send_list)) {
		isc_socketevent_t      *dev;
		isc_socketevent_t      *next;
		isc_task_t	       *current_task;

		dev = ISC_LIST_HEAD(sock->send_list);

		while (dev != NULL) {
			current_task = dev->ev_sender;
			next = ISC_LIST_NEXT(dev, ev_link);

			if ((task == NULL) || (task == current_task)) {
				dev->result = ISC_R_CANCELED;
				send_senddone_event(sock, &dev);
			}
			dev = next;
		}
	}

	/*
	 * Connecting is not a list.
	 */
	if (((how & ISC_SOCKCANCEL_CONNECT) == ISC_SOCKCANCEL_CONNECT)
	    && sock->connect_ev != NULL) {
		isc_socket_connev_t    *dev;
		isc_task_t	       *current_task;

		INSIST(sock->connecting);
		sock->connecting = 0;

		dev = sock->connect_ev;
		current_task = dev->ev_sender;

		if ((task == NULL) || (task == current_task)) {
			sock->connect_ev = NULL;

			dev->result = ISC_R_CANCELED;
			dev->ev_sender = sock;
			isc_task_sendanddetach(&current_task,
					       ISC_EVENT_PTR(&dev));
		}
	}

}

/*
 * In our assumed scenario, we can simply use a single static object.
 * XXX: this is not true if the application uses multiple threads with
 *      'multi-context' mode.  Fixing this is a future TODO item.
 */
static isc_socketwait_t swait_private;

int
isc_socketmgr_waitevents(isc_socketmgr_t *manager0, struct timeval *tvp,
			  isc_socketwait_t **swaitp)
{
	isc_socketmgr_t *manager = (isc_socketmgr_t *)manager0;
	int n;

	REQUIRE(swaitp != NULL && *swaitp == NULL);

	if (manager == NULL)
		manager = socketmgr;
	if (manager == NULL)
		return (0);

	memmove(manager->read_fds_copy, manager->read_fds, manager->fd_bufsize);
	memmove(manager->write_fds_copy, manager->write_fds,
		manager->fd_bufsize);

	swait_private.readset = manager->read_fds_copy;
	swait_private.writeset = manager->write_fds_copy;
	swait_private.maxfd = manager->maxfd + 1;

	n = select(swait_private.maxfd, swait_private.readset,
		   swait_private.writeset, NULL, tvp);

	*swaitp = &swait_private;
	return (n);
}

isc_result_t
isc_socketmgr_dispatch(isc_socketmgr_t *manager0, isc_socketwait_t *swait) {
	isc_socketmgr_t *manager = (isc_socketmgr_t *)manager0;

	REQUIRE(swait == &swait_private);

	if (manager == NULL)
		manager = socketmgr;
	if (manager == NULL)
		return (ISC_R_NOTFOUND);

	process_fds(manager, swait->maxfd, swait->readset, swait->writeset);
	return (ISC_R_SUCCESS);
}
