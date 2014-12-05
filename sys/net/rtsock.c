/*	$OpenBSD: rtsock.c,v 1.153 2014/12/05 15:50:04 mpi Exp $	*/
/*	$NetBSD: rtsock.c,v 1.18 1996/03/29 00:32:10 cgd Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988, 1991, 1993
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
 *	@(#)rtsock.c	8.6 (Berkeley) 2/11/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/raw_cb.h>

#ifdef MPLS
#include <netmpls/mpls.h>
#endif

#include <sys/stdarg.h>
#include <sys/kernel.h>
#include <sys/timeout.h>

struct sockaddr		route_dst = { 2, PF_ROUTE, };
struct sockaddr		route_src = { 2, PF_ROUTE, };
struct sockproto	route_proto = { PF_ROUTE, };

struct walkarg {
	int	w_op, w_arg, w_given, w_needed, w_tmemsize;
	caddr_t	w_where, w_tmem;
};

int	route_ctloutput(int, struct socket *, int, int, struct mbuf **);
void	route_input(struct mbuf *m0, ...);

struct mbuf	*rt_msg1(int, struct rt_addrinfo *);
int		 rt_msg2(int, int, struct rt_addrinfo *, caddr_t,
		     struct walkarg *);
void		 rt_xaddrs(caddr_t, caddr_t, struct rt_addrinfo *);

struct routecb {
	struct rawcb	rcb;
	struct timeout	timeout;
	unsigned int	msgfilter;
	unsigned int	flags;
	u_int		rtableid;
};
#define	sotoroutecb(so)	((struct routecb *)(so)->so_pcb)

struct route_cb {
	int		ip_count;
	int		ip6_count;
	int		mpls_count;
	int		any_count;
};

struct route_cb route_cb;

/*
 * These flags and timeout are used for indicating to userland (via a 
 * RTM_DESYNC msg) when the route socket has overflowed and messages 
 * have been lost.
 */
#define ROUTECB_FLAG_DESYNC	0x1	/* Route socket out of memory */
#define ROUTECB_FLAG_FLUSH	0x2	/* Wait until socket is empty before 
					   queueing more packets */

#define ROUTE_DESYNC_RESEND_TIMEOUT	(hz / 5)	/* In hz */

void	rt_senddesync(void *);

int
route_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	struct rawcb	*rp;
	struct routecb	*rop;
	int		 s, af;
	int		 error = 0;

	s = splsoftnet();
	rp = sotorawcb(so);

	switch (req) {
	case PRU_ATTACH:
		/*
		 * use the rawcb but allocate a routecb, this
		 * code does not care about the additional fields
		 * and works directly on the raw socket.
		 */
		rop = malloc(sizeof(struct routecb), M_PCB, M_WAITOK|M_ZERO);
		rp = &rop->rcb;
		so->so_pcb = rp;
		/* Init the timeout structure */
		timeout_set(&((struct routecb *)rp)->timeout, rt_senddesync, rp);
		/*
		 * Don't call raw_usrreq() in the attach case, because
		 * we want to allow non-privileged processes to listen
		 * on and send "safe" commands to the routing socket.
		 */
		if (curproc == 0)
			error = EACCES;
		else
			error = raw_attach(so, (int)(long)nam);
		if (error) {
			free(rp, M_PCB, 0);
			splx(s);
			return (error);
		}
		rop->rtableid = curproc->p_p->ps_rtableid;
		af = rp->rcb_proto.sp_protocol;
		if (af == AF_INET)
			route_cb.ip_count++;
		else if (af == AF_INET6)
			route_cb.ip6_count++;
#ifdef MPLS
		else if (af == AF_MPLS)
			route_cb.mpls_count++;
#endif
		rp->rcb_faddr = &route_src;
		route_cb.any_count++;
		soisconnected(so);
		so->so_options |= SO_USELOOPBACK;
		break;

	case PRU_RCVD:
		rop = (struct routecb *)rp;

		/*
		 * If we are in a FLUSH state, check if the buffer is 
		 * empty so that we can clear the flag.
		 */
		if (((rop->flags & ROUTECB_FLAG_FLUSH) != 0) &&
		    ((sbspace(&rp->rcb_socket->so_rcv) == 
		    rp->rcb_socket->so_rcv.sb_hiwat)))
			rop->flags &= ~ROUTECB_FLAG_FLUSH;
		break;

	case PRU_DETACH:
		if (rp) {
			timeout_del(&((struct routecb *)rp)->timeout);
			af = rp->rcb_proto.sp_protocol;
			if (af == AF_INET)
				route_cb.ip_count--;
			else if (af == AF_INET6)
				route_cb.ip6_count--;
#ifdef MPLS
			else if (af == AF_MPLS)
				route_cb.mpls_count--;
#endif
			route_cb.any_count--;
		}
		/* FALLTHROUGH */
	default:
		error = raw_usrreq(so, req, m, nam, control, p);
	}

	splx(s);
	return (error);
}

int
route_ctloutput(int op, struct socket *so, int level, int optname,
    struct mbuf **mp)
{
	struct routecb *rop = sotoroutecb(so);
	struct mbuf *m = *mp;
	int error = 0;
	unsigned int tid;

	if (level != AF_ROUTE) {
		error = EINVAL;
		if (op == PRCO_SETOPT && *mp)
			m_free(*mp);
		return (error);
	}

	switch (op) {
	case PRCO_SETOPT:
		switch (optname) {
		case ROUTE_MSGFILTER:
			if (m == NULL || m->m_len != sizeof(unsigned int))
				error = EINVAL;
			else
				rop->msgfilter = *mtod(m, unsigned int *);
			break;
		case ROUTE_TABLEFILTER:
			if (m == NULL || m->m_len != sizeof(unsigned int)) {
				error = EINVAL;
				break;
			}
			tid = *mtod(m, unsigned int *);
			if (tid != RTABLE_ANY && !rtable_exists(tid))
				error = ENOENT;
			else
				rop->rtableid = tid;
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			m_free(m);
		break;
	case PRCO_GETOPT:
		switch (optname) {
		case ROUTE_MSGFILTER:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);   
			m->m_len = sizeof(unsigned int);
			*mtod(m, unsigned int *) = rop->msgfilter;
			break;
		case ROUTE_TABLEFILTER:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);   
			m->m_len = sizeof(unsigned int);
			*mtod(m, unsigned int *) = rop->rtableid;
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
	}
	return (error);
}

void
rt_senddesync(void *data)
{
	struct rawcb	*rp;
	struct routecb	*rop;
	struct mbuf	*desync_mbuf;

	rp = (struct rawcb *)data;
	rop = (struct routecb *)rp;

	/* If we are in a DESYNC state, try to send a RTM_DESYNC packet */
	if ((rop->flags & ROUTECB_FLAG_DESYNC) != 0) {
		/*
		 * If we fail to alloc memory or if sbappendaddr() 
		 * fails, re-add timeout and try again.
		 */
		desync_mbuf = rt_msg1(RTM_DESYNC, NULL);
		if ((desync_mbuf != NULL) && 
		    (sbappendaddr(&rp->rcb_socket->so_rcv, &route_src, 
		    desync_mbuf, (struct mbuf *)0) != 0)) {
			rop->flags &= ~ROUTECB_FLAG_DESYNC;
			sorwakeup(rp->rcb_socket);
		} else {
			if (desync_mbuf)
				m_freem(desync_mbuf);
			/* Re-add timeout to try sending msg again */
			timeout_add(&rop->timeout, ROUTE_DESYNC_RESEND_TIMEOUT);
		}
	}
}

void
route_input(struct mbuf *m0, ...)
{
	struct rawcb *rp;
	struct routecb *rop;
	struct rt_msghdr *rtm;
	struct mbuf *m = m0;
	int sockets = 0;
	struct socket *last = NULL;
	va_list ap;
	struct sockproto *proto;
	struct sockaddr *sosrc, *sodst;
	
	va_start(ap, m0);
	proto = va_arg(ap, struct sockproto *);
	sosrc = va_arg(ap, struct sockaddr *);
	sodst = va_arg(ap, struct sockaddr *);
	va_end(ap);

	/* ensure that we can access the rtm_type via mtod() */
	if (m->m_len < offsetof(struct rt_msghdr, rtm_type) + 1) {
		m_freem(m);
		return;
	}

	LIST_FOREACH(rp, &rawcb, rcb_list) {
		if (rp->rcb_socket->so_state & SS_CANTRCVMORE)
			continue;
		if (rp->rcb_proto.sp_family != proto->sp_family)
			continue;
		if (rp->rcb_proto.sp_protocol && proto->sp_protocol &&
		    rp->rcb_proto.sp_protocol != proto->sp_protocol)
			continue;
		/*
		 * We assume the lower level routines have
		 * placed the address in a canonical format
		 * suitable for a structure comparison.
		 *
		 * Note that if the lengths are not the same
		 * the comparison will fail at the first byte.
		 */
#define	equal(a1, a2) \
  (bcmp((caddr_t)(a1), (caddr_t)(a2), a1->sa_len) == 0)
		if (rp->rcb_laddr && !equal(rp->rcb_laddr, sodst))
			continue;
		if (rp->rcb_faddr && !equal(rp->rcb_faddr, sosrc))
			continue;

		/* filter messages that the process does not want */
		rop = (struct routecb *)rp;
		rtm = mtod(m, struct rt_msghdr *);
		/* but RTM_DESYNC can't be filtered */
		if (rtm->rtm_type != RTM_DESYNC && rop->msgfilter != 0 &&
		    !(rop->msgfilter & (1 << rtm->rtm_type)))
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFANNOUNCE:
		case RTM_DESYNC:
			/* no tableid */
			break;
		case RTM_RESOLVE:
		case RTM_NEWADDR:
		case RTM_DELADDR:
		case RTM_IFINFO:
			/* check against rdomain id */
			if (rop->rtableid != RTABLE_ANY &&
			    rtable_l2(rop->rtableid) != rtm->rtm_tableid)
				continue;
			break;
		default:
			/* check against rtable id */
			if (rop->rtableid != RTABLE_ANY &&
			    rop->rtableid != rtm->rtm_tableid)
				continue;
			break;
		}

		/*
		 * Check to see if the flush flag is set. If so, don't queue 
		 * any more messages until the flag is cleared.
		 */
		if ((rop->flags & ROUTECB_FLAG_FLUSH) != 0)
			continue;

		if (last) {
			struct mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (sbspace(&last->so_rcv) < (2 * MSIZE) ||
				    sbappendaddr(&last->so_rcv, sosrc,
				    n, (struct mbuf *)0) == 0) {
					/*
					 * Flag socket as desync'ed and 
					 * flush required
					 */
					sotoroutecb(last)->flags |= 
					    ROUTECB_FLAG_DESYNC | 
					    ROUTECB_FLAG_FLUSH;
					rt_senddesync((void *) sotorawcb(last));
					m_freem(n);
				} else {
					sorwakeup(last);
					sockets++;
				}
			}
		}
		last = rp->rcb_socket;
	}
	if (last) {
		if (sbspace(&last->so_rcv) < (2 * MSIZE) ||
		    sbappendaddr(&last->so_rcv, sosrc,
		    m, (struct mbuf *)0) == 0) {
			/* Flag socket as desync'ed and flush required */
			sotoroutecb(last)->flags |= 
			    ROUTECB_FLAG_DESYNC | ROUTECB_FLAG_FLUSH;
			rt_senddesync((void *) sotorawcb(last));
			m_freem(m);
		} else {
			sorwakeup(last);
			sockets++;
		}
	} else
		m_freem(m);
}

int
route_output(struct mbuf *m, ...)
{
	struct rt_msghdr	*rtm = NULL;
	struct radix_node	*rn = NULL;
	struct rtentry		*rt = NULL;
	struct rtentry		*saved_nrt = NULL;
	struct radix_node_head	*rnh;
	struct rt_addrinfo	 info;
	int			 len, newgate, error = 0;
	struct ifnet		*ifp = NULL;
	struct ifaddr		*ifa = NULL;
	struct socket		*so;
	struct rawcb		*rp = NULL;
	struct sockaddr_rtlabel	 sa_rl;
#ifdef MPLS
	struct sockaddr_mpls	 sa_mpls, *psa_mpls;
#endif
	va_list			 ap;
	u_int			 tableid;
	u_int8_t		 prio;
	u_char			 vers;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	va_end(ap);

	info.rti_info[RTAX_DST] = NULL;	/* for error handling (goto flush) */
	if (m == 0 || ((m->m_len < sizeof(int32_t)) &&
	    (m = m_pullup(m, sizeof(int32_t))) == 0))
		return (ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	len = m->m_pkthdr.len;
	if (len < offsetof(struct rt_msghdr, rtm_type) + 1 ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen) {
		error = EINVAL;
		goto fail;
	}
	vers = mtod(m, struct rt_msghdr *)->rtm_version;
	switch (vers) {
	case RTM_VERSION:
		if (len < sizeof(struct rt_msghdr)) {
			error = EINVAL;
			goto fail;
		}
		if (len > RTM_MAXSIZE) {
			error = EMSGSIZE;
			goto fail;
		}
		rtm = malloc(len, M_RTABLE, M_NOWAIT);
		if (rtm == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		m_copydata(m, 0, len, (caddr_t)rtm);
		break;
	default:
		error = EPROTONOSUPPORT;
		goto fail;
	}
	rtm->rtm_pid = curproc->p_p->ps_pid;
	if (rtm->rtm_hdrlen == 0)	/* old client */
		rtm->rtm_hdrlen = sizeof(struct rt_msghdr);
	if (len < rtm->rtm_hdrlen) {
		error = EINVAL;
		goto fail;
	}

	/* Verify that the caller is sending an appropriate message early */
	switch (rtm->rtm_type) {
	case RTM_ADD:
	case RTM_DELETE:
	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		break;
	default:
		error = EOPNOTSUPP;
		goto fail;
	}

	/*
	 * Verify that the caller has the appropriate privilege; RTM_GET
	 * is the only operation the non-superuser is allowed.
	 */
	if (rtm->rtm_type != RTM_GET && suser(curproc, 0) != 0) {
		error = EACCES;
		goto fail;
	}

	tableid = rtm->rtm_tableid;
	if (!rtable_exists(tableid)) {
		if (rtm->rtm_type == RTM_ADD) {
			if ((error = rtable_add(tableid)) != 0)
				goto flush;
		} else {
			error = EINVAL;
			goto flush;
		}
	}


	/* Do not let userland play with kernel-only flags. */
	if ((rtm->rtm_flags & (RTF_LOCAL|RTF_BROADCAST)) != 0) {
		error = EINVAL;
		goto fail;
	}

	/* make sure that kernel-only bits are not set */
	rtm->rtm_priority &= RTP_MASK;
	rtm->rtm_flags &= ~(RTF_DONE|RTF_CLONED);
	rtm->rtm_fmask &= RTF_FMASK;

	if (rtm->rtm_priority != 0) {
		if (rtm->rtm_priority > RTP_MAX ||
		    rtm->rtm_priority == RTP_LOCAL) {
			error = EINVAL;
			goto fail;
		}
		prio = rtm->rtm_priority;
	} else if (rtm->rtm_type != RTM_ADD)
		prio = RTP_ANY;
	else if (rtm->rtm_flags & RTF_STATIC)
		prio = 0;
	else
		prio = RTP_DEFAULT;

	bzero(&info, sizeof(info));
	info.rti_addrs = rtm->rtm_addrs;
	rt_xaddrs(rtm->rtm_hdrlen + (caddr_t)rtm, len + (caddr_t)rtm, &info);
	info.rti_flags = rtm->rtm_flags;
	if (info.rti_info[RTAX_DST] == NULL ||
	    info.rti_info[RTAX_DST]->sa_family >= AF_MAX ||
	    (info.rti_info[RTAX_GATEWAY] != NULL &&
	    info.rti_info[RTAX_GATEWAY]->sa_family >= AF_MAX) ||
	    info.rti_info[RTAX_GENMASK] != NULL) {
		error = EINVAL;
		goto flush;
	}
#ifdef MPLS
	info.rti_mpls = rtm->rtm_mpls;
#endif

	switch (rtm->rtm_type) {
	case RTM_ADD:
		if (info.rti_info[RTAX_GATEWAY] == NULL) {
			error = EINVAL;
			goto flush;
		}
		error = rtrequest1(rtm->rtm_type, &info, prio, &saved_nrt,
		    tableid);
		if (error == 0 && saved_nrt) {
			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
			    &saved_nrt->rt_rmx);
			saved_nrt->rt_refcnt--;
			/* write back the priority the kernel used */
			rtm->rtm_priority = saved_nrt->rt_priority & RTP_MASK;
			rtm->rtm_index = saved_nrt->rt_ifp->if_index;
			rtm->rtm_flags = saved_nrt->rt_flags;
		}
		break;
	case RTM_DELETE:
		error = rtrequest1(rtm->rtm_type, &info, prio, &saved_nrt,
		    tableid);
		if (error == 0) {
			(rt = saved_nrt)->rt_refcnt++;
			goto report;
		}
		break;
	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		rnh = rtable_get(tableid, info.rti_info[RTAX_DST]->sa_family);
		if (rnh == NULL) {
			error = EAFNOSUPPORT;
			goto flush;
		}
		rt = rt_lookup(info.rti_info[RTAX_DST],
		    info.rti_info[RTAX_NETMASK], tableid);
		rn = (struct radix_node *)rt;
		if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0) {
			error = ESRCH;
			rt = NULL;
			goto flush;
		}
#ifndef SMALL_KERNEL
		if (rn_mpath_capable(rnh)) {
			/* first find the right priority */
			rt = rt_mpath_matchgate(rt, NULL, prio);
			if (!rt) {
				error = ESRCH;
				goto flush;
			}
			/*
			 * For RTM_CHANGE/LOCK, if we got multipath routes,
			 * a matching RTAX_GATEWAY is required.
			 * OR
			 * If a gateway is specified then RTM_GET and
			 * RTM_LOCK must match the gateway no matter
			 * what even in the non multipath case.
			 */
			if ((rt->rt_flags & RTF_MPATH) ||
			    (info.rti_info[RTAX_GATEWAY] && rtm->rtm_type !=
			    RTM_CHANGE)) {
				rt = rt_mpath_matchgate(rt,
				    info.rti_info[RTAX_GATEWAY], prio);
				if (!rt) {
					error = ESRCH;
					goto flush;
				}
				/*
				 * only RTM_GET may use an empty gateway
				 * on multipath routes
				 */
				if (!info.rti_info[RTAX_GATEWAY] &&
				    rtm->rtm_type != RTM_GET) {
					rt = NULL;
					error = ESRCH;
					goto flush;
				}
			}
			rn = (struct radix_node *)rt;
		}
#endif
		rt->rt_refcnt++;

		/*
		 * RTM_CHANGE/LOCK need a perfect match, rn_lookup()
		 * returns a perfect match in case a netmask is specified.
		 * For host routes only a longest prefix match is returned
		 * so it is necessary to compare the existence of the netmaks.
		 * If both have a netmask rn_lookup() did a perfect match and
		 * if none of them have a netmask both are host routes which is
		 * also a perfect match.
		 */
		if (rtm->rtm_type != RTM_GET &&
		    !rt_mask(rt) != !info.rti_info[RTAX_NETMASK]) {
			error = ESRCH;
			goto flush;
		}

		switch (rtm->rtm_type) {
		case RTM_GET:
report:
			info.rti_info[RTAX_DST] = rt_key(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_LABEL] =
			    rtlabel_id2sa(rt->rt_labelid, &sa_rl);
#ifdef MPLS
			if (rt->rt_flags & RTF_MPLS) {
				bzero(&sa_mpls, sizeof(sa_mpls));
				sa_mpls.smpls_family = AF_MPLS;
				sa_mpls.smpls_len = sizeof(sa_mpls);
				sa_mpls.smpls_label = ((struct rt_mpls *)
				    rt->rt_llinfo)->mpls_label;
				info.rti_info[RTAX_SRC] =
				    (struct sockaddr *)&sa_mpls;
				info.rti_mpls = ((struct rt_mpls *)
				    rt->rt_llinfo)->mpls_operation;
				rtm->rtm_mpls = info.rti_mpls;
			}
#endif
			info.rti_info[RTAX_IFP] = NULL;
			info.rti_info[RTAX_IFA] = NULL;
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA) &&
			    (ifp = rt->rt_ifp) != NULL) {
				info.rti_info[RTAX_IFP] =
					(struct sockaddr *)ifp->if_sadl;
				info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
				if (ifp->if_flags & IFF_POINTOPOINT)
					info.rti_info[RTAX_BRD] =
					    rt->rt_ifa->ifa_dstaddr;
				else
					info.rti_info[RTAX_BRD] = NULL;
				rtm->rtm_index = ifp->if_index;
			}
			len = rt_msg2(rtm->rtm_type, RTM_VERSION, &info, NULL,
			    NULL);
			if (len > rtm->rtm_msglen) {
				struct rt_msghdr	*new_rtm;
				new_rtm = malloc(len, M_RTABLE, M_NOWAIT);
				if (new_rtm == NULL) {
					error = ENOBUFS;
					goto flush;
				}
				memcpy(new_rtm, rtm, rtm->rtm_msglen);
				free(rtm, M_RTABLE, 0);
				rtm = new_rtm;
			}
			rt_msg2(rtm->rtm_type, RTM_VERSION, &info, (caddr_t)rtm,
			    NULL);
			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_use = 0;
			rtm->rtm_priority = rt->rt_priority & RTP_MASK;
			rt_getmetrics(&rt->rt_rmx, &rtm->rtm_rmx);
			rtm->rtm_addrs = info.rti_addrs;
			break;

		case RTM_CHANGE:
			/*
			 * new gateway could require new ifaddr, ifp;
			 * flags may also be different; ifp may be specified
			 * by ll sockaddr when protocol address is ambiguous
			 */
			if ((error = rt_getifa(&info, tableid)) != 0)
				goto flush;
			newgate = 0;
			if (info.rti_info[RTAX_GATEWAY] != NULL)
				if (rt->rt_gateway == NULL ||
				    bcmp(rt->rt_gateway,
				    info.rti_info[RTAX_GATEWAY],
				    info.rti_info[RTAX_GATEWAY]->sa_len)) {
					newgate = 1;
				}
			if (info.rti_info[RTAX_GATEWAY] != NULL &&
			    rt_setgate(rt, rt_key(rt),
			    info.rti_info[RTAX_GATEWAY], tableid)) {
				error = EDQUOT;
				goto flush;
			}
			ifa = info.rti_ifa;
			if (ifa) {
				if (rt->rt_ifa != ifa) {
					if (rt->rt_ifa->ifa_rtrequest)
						rt->rt_ifa->ifa_rtrequest(
						    RTM_DELETE, rt);
					ifafree(rt->rt_ifa);
					rt->rt_ifa = ifa;
					ifa->ifa_refcnt++;
					rt->rt_ifp = ifa->ifa_ifp;
#ifndef SMALL_KERNEL
					/* recheck link state after ifp change*/
					rt_if_linkstate_change(
					    (struct radix_node *)rt, rt->rt_ifp,
					    tableid);
#endif
				}
			}
#ifdef MPLS
			if ((rtm->rtm_flags & RTF_MPLS) &&
			    info.rti_info[RTAX_SRC] != NULL) {
				struct rt_mpls *rt_mpls;

				psa_mpls = (struct sockaddr_mpls *)
				    info.rti_info[RTAX_SRC];

				if (rt->rt_llinfo == NULL) {
					rt->rt_llinfo =
					    malloc(sizeof(struct rt_mpls),
					    M_TEMP, M_NOWAIT|M_ZERO);
				}
				if (rt->rt_llinfo == NULL) {
					error = ENOMEM;
					goto flush;
				}

				rt_mpls = (struct rt_mpls *)rt->rt_llinfo;

				if (psa_mpls != NULL) {
					rt_mpls->mpls_label =
					    psa_mpls->smpls_label;
				}

				rt_mpls->mpls_operation = info.rti_mpls;

				/* XXX: set experimental bits */

				rt->rt_flags |= RTF_MPLS;
			} else if (newgate || ((rtm->rtm_fmask & RTF_MPLS) &&
			    !(rtm->rtm_flags & RTF_MPLS))) {
				/* if gateway changed remove MPLS information */
				if (rt->rt_llinfo != NULL &&
				    rt->rt_flags & RTF_MPLS) {
					free(rt->rt_llinfo, M_TEMP, 0);
					rt->rt_llinfo = NULL;
					rt->rt_flags &= ~RTF_MPLS;
				}
			}
#endif
			/* Hack to allow some flags to be toggled */
			if (rtm->rtm_fmask)
				rt->rt_flags =
				    (rt->rt_flags & ~rtm->rtm_fmask) |
				    (rtm->rtm_flags & rtm->rtm_fmask);

			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
			    &rt->rt_rmx);
			rtm->rtm_index = rt->rt_ifp->if_index;
			rtm->rtm_priority = rt->rt_priority & RTP_MASK;
			rtm->rtm_flags = rt->rt_flags;
			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
				rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt);
			if (info.rti_info[RTAX_LABEL] != NULL) {
				char *rtlabel = ((struct sockaddr_rtlabel *)
				    info.rti_info[RTAX_LABEL])->sr_label;
				rtlabel_unref(rt->rt_labelid);
				rt->rt_labelid =
				    rtlabel_name2id(rtlabel);
			}
			if_group_routechange(info.rti_info[RTAX_DST],
			    info.rti_info[RTAX_NETMASK]);
			/* FALLTHROUGH */
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks &= ~(rtm->rtm_inits);
			rt->rt_rmx.rmx_locks |=
			    (rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
			rtm->rtm_priority = rt->rt_priority & RTP_MASK;
			break;
		}
		break;
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else { 
			rtm->rtm_flags |= RTF_DONE;
		}
	}
	if (info.rti_info[RTAX_DST])
		route_proto.sp_protocol = info.rti_info[RTAX_DST]->sa_family;
	if (rt)
		rtfree(rt);

	/*
	 * Check to see if we don't want our own messages.
	 */
	if (!(so->so_options & SO_USELOOPBACK)) {
		if (route_cb.any_count <= 1) {
fail:
			free(rtm, M_RTABLE, 0);
			m_freem(m);
			return (error);
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}
	if (rp)
		rp->rcb_proto.sp_family = 0; /* Avoid us */
	if (rtm) {
		if (m_copyback(m, 0, rtm->rtm_msglen, rtm, M_NOWAIT)) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > rtm->rtm_msglen)
			m_adj(m, rtm->rtm_msglen - m->m_pkthdr.len);
		free(rtm, M_RTABLE, 0);
	}
	if (m)
		route_input(m, &route_proto, &route_src, &route_dst);
	if (rp)
		rp->rcb_proto.sp_family = PF_ROUTE;

	return (error);
}

void
rt_setmetrics(u_long which, struct rt_metrics *in, struct rt_kmetrics *out)
{
	if (which & RTV_MTU)
		out->rmx_mtu = in->rmx_mtu;
	if (which & RTV_EXPIRE)
		out->rmx_expire = in->rmx_expire;
	/* RTV_PRIORITY handled before */
}

void
rt_getmetrics(struct rt_kmetrics *in, struct rt_metrics *out)
{
	bzero(out, sizeof(*out));
	out->rmx_locks = in->rmx_locks;
	out->rmx_mtu = in->rmx_mtu;
	out->rmx_expire = in->rmx_expire;
	out->rmx_pksent = in->rmx_pksent;
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
	struct sockaddr	*sa;
	int		 i;

	bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}

struct mbuf *
rt_msg1(int type, struct rt_addrinfo *rtinfo)
{
	struct rt_msghdr	*rtm;
	struct mbuf		*m;
	int			 i;
	struct sockaddr		*sa;
	int			 len, dlen, hlen;

	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;
	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;
	case RTM_IFANNOUNCE:
		len = sizeof(struct if_announcemsghdr);
		break;
	default:
		len = sizeof(struct rt_msghdr);
		break;
	}
	if (len > MCLBYTES)
		panic("rt_msg1");
	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m && len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			m = NULL;
		}
	}
	if (m == 0)
		return (m);
	m->m_pkthdr.len = m->m_len = hlen = len;
	m->m_pkthdr.rcvif = NULL;
	rtm = mtod(m, struct rt_msghdr *);
	bzero(rtm, len);
	for (i = 0; i < RTAX_MAX; i++) {
		if (rtinfo == NULL || (sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		if (m_copyback(m, len, dlen, sa, M_NOWAIT)) {
			m_freem(m);
			return (NULL);
		}
		len += dlen;
	}
	rtm->rtm_msglen = len;
	rtm->rtm_hdrlen = hlen;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	return (m);
}

int
rt_msg2(int type, int vers, struct rt_addrinfo *rtinfo, caddr_t cp,
    struct walkarg *w)
{
	int		i;
	int		len, dlen, hlen, second_time = 0;
	caddr_t		cp0;

	rtinfo->rti_addrs = 0;
again:
	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;
	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;
	default:
		len = sizeof(struct rt_msghdr);
		break;
	}
	hlen = len;
	if ((cp0 = cp) != NULL)
		cp += len;
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		if (cp) {
			bcopy(sa, cp, (size_t)dlen);
			cp += dlen;
		}
		len += dlen;
	}
	/* align message length to the next natural boundary */
	len = ALIGN(len);
	if (cp == 0 && w != NULL && !second_time) {
		struct walkarg *rw = w;

		rw->w_needed += len;
		if (rw->w_needed <= 0 && rw->w_where) {
			if (rw->w_tmemsize < len) {
				free(rw->w_tmem, M_RTABLE, 0);
				rw->w_tmem = malloc(len, M_RTABLE, M_NOWAIT);
				if (rw->w_tmem)
					rw->w_tmemsize = len;
			}
			if (rw->w_tmem) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			} else
				rw->w_where = 0;
		}
	}
	if (cp && w)		/* clear the message header */
		bzero(cp0, hlen);

	if (cp) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)cp0;

		rtm->rtm_version = RTM_VERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
		rtm->rtm_hdrlen = hlen;
	}
	return (len);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occurred, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags,
    struct ifnet *ifp, int error, u_int tableid)
{
	struct rt_msghdr	*rtm;
	struct mbuf		*m;
	struct sockaddr		*sa = rtinfo->rti_info[RTAX_DST];

	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(type, rtinfo);
	if (m == 0)
		return;
	rtm = mtod(m, struct rt_msghdr *);
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_errno = error;
	rtm->rtm_tableid = tableid;
	rtm->rtm_addrs = rtinfo->rti_addrs;
	if (ifp != NULL)
		rtm->rtm_index = ifp->if_index;
	if (sa == NULL)
		route_proto.sp_protocol = 0;
	else
		route_proto.sp_protocol = sa->sa_family;
	route_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
rt_ifmsg(struct ifnet *ifp)
{
	struct if_msghdr	*ifm;
	struct mbuf		*m;

	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(RTM_IFINFO, NULL);
	if (m == 0)
		return;
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_tableid = ifp->if_rdomain;
	ifm->ifm_flags = ifp->if_flags;
	ifm->ifm_xflags = ifp->if_xflags;
	ifm->ifm_data = ifp->if_data;
	ifm->ifm_addrs = 0;
	route_proto.sp_protocol = 0;
	route_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 */
void
rt_newaddrmsg(int cmd, struct ifaddr *ifa, int error, struct rtentry *rt)
{
	struct rt_addrinfo	 info;
	struct sockaddr		*sa = NULL;
	int			 pass;
	struct mbuf		*m = NULL;
	struct ifnet		*ifp = ifa->ifa_ifp;

	if (route_cb.any_count == 0)
		return;
	for (pass = 1; pass < 3; pass++) {
		bzero(&info, sizeof(info));
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			struct ifa_msghdr	*ifam;
			int			 ncmd;

			if (cmd == RTM_ADD)
				ncmd = RTM_NEWADDR;
			else
				ncmd = RTM_DELADDR;

			info.rti_info[RTAX_IFA] = sa = ifa->ifa_addr;
			info.rti_info[RTAX_IFP] =
			    (struct sockaddr *)ifp->if_sadl;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			if ((m = rt_msg1(ncmd, &info)) == NULL)
				continue;
			ifam = mtod(m, struct ifa_msghdr *);
			ifam->ifam_index = ifp->if_index;
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_flags = ifa->ifa_flags;
			ifam->ifam_addrs = info.rti_addrs;
			ifam->ifam_tableid = ifp->if_rdomain;
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			struct rt_msghdr *rtm;
			struct sockaddr_rtlabel sa_rl;
			
			if (rt == 0)
				continue;
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_DST] = sa = rt_key(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			info.rti_info[RTAX_LABEL] =
			    rtlabel_id2sa(rt->rt_labelid, &sa_rl);
			if ((m = rt_msg1(cmd, &info)) == NULL)
				continue;
			rtm = mtod(m, struct rt_msghdr *);
			rtm->rtm_index = ifp->if_index;
			rtm->rtm_flags |= rt->rt_flags;
			rtm->rtm_priority = rt->rt_priority & RTP_MASK;
			rtm->rtm_errno = error;
			rtm->rtm_addrs = info.rti_addrs;
			rtm->rtm_tableid = ifp->if_rdomain;
		}
		if (sa == NULL)
			route_proto.sp_protocol = 0;
		else
			route_proto.sp_protocol = sa->sa_family;
		route_input(m, &route_proto, &route_src, &route_dst);
	}
}

/*
 * This is called to generate routing socket messages indicating
 * network interface arrival and departure.
 */
void
rt_ifannouncemsg(struct ifnet *ifp, int what)
{
	struct if_announcemsghdr	*ifan;
	struct mbuf			*m;

	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(RTM_IFANNOUNCE, NULL);
	if (m == 0)
		return;
	ifan = mtod(m, struct if_announcemsghdr *);
	ifan->ifan_index = ifp->if_index;
	strlcpy(ifan->ifan_name, ifp->if_xname, sizeof(ifan->ifan_name));
	ifan->ifan_what = what;
	route_proto.sp_protocol = 0;
	route_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This is used in dumping the kernel table via sysctl().
 */
int
sysctl_dumpentry(struct radix_node *rn, void *v, u_int id)
{
	struct walkarg		*w = v;
	struct rtentry		*rt = (struct rtentry *)rn;
	int			 error = 0, size;
	struct rt_addrinfo	 info;
#ifdef MPLS
	struct sockaddr_mpls	 sa_mpls;
#endif
	struct sockaddr_rtlabel	 sa_rl;

	if (w->w_op == NET_RT_FLAGS && !(rt->rt_flags & w->w_arg))
		return 0;
	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	if (rt->rt_ifp) {
		info.rti_info[RTAX_IFP] =
		    (struct sockaddr *)rt->rt_ifp->if_sadl;
		info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
		if (rt->rt_ifp->if_flags & IFF_POINTOPOINT)
			info.rti_info[RTAX_BRD] = rt->rt_ifa->ifa_dstaddr;
	}
	info.rti_info[RTAX_LABEL] = rtlabel_id2sa(rt->rt_labelid, &sa_rl);
#ifdef MPLS
	if (rt->rt_flags & RTF_MPLS) {
		bzero(&sa_mpls, sizeof(sa_mpls));
		sa_mpls.smpls_family = AF_MPLS;
		sa_mpls.smpls_len = sizeof(sa_mpls);
		sa_mpls.smpls_label = ((struct rt_mpls *)
		    rt->rt_llinfo)->mpls_label;
		info.rti_info[RTAX_SRC] = (struct sockaddr *)&sa_mpls;
		info.rti_mpls = ((struct rt_mpls *)
		    rt->rt_llinfo)->mpls_operation;
	}
#endif

	size = rt_msg2(RTM_GET, RTM_VERSION, &info, NULL, w);
	if (w->w_where && w->w_tmem && w->w_needed <= 0) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)w->w_tmem;

		rtm->rtm_flags = rt->rt_flags;
		rtm->rtm_priority = rt->rt_priority & RTP_MASK;
		rt_getmetrics(&rt->rt_rmx, &rtm->rtm_rmx);
		rtm->rtm_rmx.rmx_refcnt = rt->rt_refcnt;
		rtm->rtm_index = rt->rt_ifp->if_index;
		rtm->rtm_addrs = info.rti_addrs;
		rtm->rtm_tableid = id;
#ifdef MPLS
		rtm->rtm_mpls = info.rti_mpls;
#endif
		if ((error = copyout(rtm, w->w_where, size)) != 0)
			w->w_where = NULL;
		else
			w->w_where += size;
	}
	return (error);
}

int
sysctl_iflist(int af, struct walkarg *w)
{
	struct ifnet		*ifp;
	struct ifaddr		*ifa;
	struct rt_addrinfo	 info;
	int			 len, error = 0;

	bzero(&info, sizeof(info));
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		/* Copy the link-layer address first */
		info.rti_info[RTAX_IFP] = (struct sockaddr *)ifp->if_sadl;
		len = rt_msg2(RTM_IFINFO, RTM_VERSION, &info, 0, w);
		if (w->w_where && w->w_tmem && w->w_needed <= 0) {
			struct if_msghdr *ifm;

			ifm = (struct if_msghdr *)w->w_tmem;
			ifm->ifm_index = ifp->if_index;
			ifm->ifm_tableid = ifp->if_rdomain;
			ifm->ifm_flags = ifp->if_flags;
			ifm->ifm_data = ifp->if_data;
			ifm->ifm_addrs = info.rti_addrs;
			error = copyout(ifm, w->w_where, len);
			if (error)
				return (error);
			w->w_where += len;
		}
		info.rti_info[RTAX_IFP] = NULL;
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			KASSERT(ifa->ifa_addr->sa_family != AF_LINK);
			if (af && af != ifa->ifa_addr->sa_family)
				continue;
			info.rti_info[RTAX_IFA] = ifa->ifa_addr;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			len = rt_msg2(RTM_NEWADDR, RTM_VERSION, &info, 0, w);
			if (w->w_where && w->w_tmem && w->w_needed <= 0) {
				struct ifa_msghdr *ifam;

				ifam = (struct ifa_msghdr *)w->w_tmem;
				ifam->ifam_index = ifa->ifa_ifp->if_index;
				ifam->ifam_flags = ifa->ifa_flags;
				ifam->ifam_metric = ifa->ifa_metric;
				ifam->ifam_addrs = info.rti_addrs;
				error = copyout(w->w_tmem, w->w_where, len);
				if (error)
					return (error);
				w->w_where += len;
			}
		}
		info.rti_info[RTAX_IFA] = info.rti_info[RTAX_NETMASK] =
		    info.rti_info[RTAX_BRD] = NULL;
	}
	return (0);
}

int
sysctl_rtable(int *name, u_int namelen, void *where, size_t *given, void *new,
    size_t newlen)
{
	struct radix_node_head	*rnh;
	int			 i, s, error = EINVAL;
	u_char  		 af;
	struct walkarg		 w;
	struct rt_tableinfo	 tableinfo;
	u_int			 tableid = 0;

	if (new)
		return (EPERM);
	if (namelen < 3 || namelen > 4)
		return (EINVAL);
	af = name[0];
	bzero(&w, sizeof(w));
	w.w_where = where;
	w.w_given = *given;
	w.w_needed = 0 - w.w_given;
	w.w_op = name[1];
	w.w_arg = name[2];

	if (namelen == 4) {
		tableid = name[3];
		if (!rtable_exists(tableid))
			return (ENOENT);
	} else
		tableid = curproc->p_p->ps_rtableid;

	s = splsoftnet();
	switch (w.w_op) {

	case NET_RT_DUMP:
	case NET_RT_FLAGS:
		for (i = 1; i <= AF_MAX; i++)
			if ((rnh = rtable_get(tableid, i)) != NULL &&
			    (af == 0 || af == i) &&
			    (error = (*rnh->rnh_walktree)(rnh,
			    sysctl_dumpentry, &w)))
				break;
		break;

	case NET_RT_IFLIST:
		error = sysctl_iflist(af, &w);
		break;

	case NET_RT_STATS:
		error = sysctl_rdstruct(where, given, new,
		    &rtstat, sizeof(rtstat));
		splx(s);
		return (error);
	case NET_RT_TABLE:
		tableid = w.w_arg;
		if (!rtable_exists(tableid)) {
			splx(s);
			return (ENOENT);
		}
		tableinfo.rti_tableid = tableid;
		tableinfo.rti_domainid = rtable_l2(tableid);
		error = sysctl_rdstruct(where, given, new,
		    &tableinfo, sizeof(tableinfo));
		splx(s);
		return (error);
	}
	splx(s);
	free(w.w_tmem, M_RTABLE, 0);
	w.w_needed += w.w_given;
	if (where) {
		*given = w.w_where - (caddr_t)where;
		if (*given < w.w_needed)
			return (ENOMEM);
	} else
		*given = (11 * w.w_needed) / 10;

	return (error);
}

/*
 * Definitions of protocols supported in the ROUTE domain.
 */

extern	struct domain routedomain;		/* or at least forward */

struct protosw routesw[] = {
{ SOCK_RAW,	&routedomain,	0,		PR_ATOMIC|PR_ADDR|PR_WANTRCVD,
  route_input,	route_output,	raw_ctlinput,	route_ctloutput,
  route_usrreq,
  raw_init,	0,		0,		0,
  sysctl_rtable,
}
};

struct domain routedomain =
    { PF_ROUTE, "route", route_init, 0, 0,
      routesw, &routesw[nitems(routesw)] };
