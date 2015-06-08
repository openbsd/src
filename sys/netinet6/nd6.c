/*	$OpenBSD: nd6.c,v 1.138 2015/06/08 22:19:28 krw Exp $	*/
/*	$KAME: nd6.c,v 1.280 2002/06/08 19:52:07 itojun Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/task.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_ipsp.h>

#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/icmp6.h>

#define ND6_SLOWTIMER_INTERVAL (60 * 60) /* 1 hour */
#define ND6_RECALC_REACHTM_INTERVAL (60 * 120) /* 2 hours */

#define SDL(s) ((struct sockaddr_dl *)s)

/* timer values */
int	nd6_prune	= 1;	/* walk list every 1 seconds */
int	nd6_delay	= 5;	/* delay first probe time 5 second */
int	nd6_umaxtries	= 3;	/* maximum unicast query */
int	nd6_mmaxtries	= 3;	/* maximum multicast query */
int	nd6_gctimer	= (60 * 60 * 24); /* 1 day: garbage collection timer */

/* preventing too many loops in ND option parsing */
int nd6_maxndopt = 10;	/* max # of ND options allowed */

int nd6_maxnudhint = 0;	/* max # of subsequent upper layer hints */

#ifdef ND6_DEBUG
int nd6_debug = 1;
#else
int nd6_debug = 0;
#endif

static int nd6_inuse, nd6_allocated;

struct llinfo_nd6 llinfo_nd6 = {&llinfo_nd6, &llinfo_nd6};
struct nd_drhead nd_defrouter;
struct nd_prhead nd_prefix = { 0 };

int nd6_recalc_reachtm_interval = ND6_RECALC_REACHTM_INTERVAL;

void nd6_setmtu0(struct ifnet *, struct nd_ifinfo *);
void nd6_slowtimo(void *);
struct llinfo_nd6 *nd6_free(struct rtentry *, int);
void nd6_llinfo_timer(void *);

struct timeout nd6_slowtimo_ch;
struct timeout nd6_timer_ch;
struct task nd6_timer_task;
void nd6_timer_work(void *);

struct timeout nd6_rs_output_timer;
int nd6_rs_output_timeout = ND6_RS_OUTPUT_INTERVAL;
int nd6_rs_timeout_count = 0;
void nd6_rs_output_timo(void *);

int fill_drlist(void *, size_t *, size_t);
int fill_prlist(void *, size_t *, size_t);

#define LN_DEQUEUE(ln) do { \
	(ln)->ln_next->ln_prev = (ln)->ln_prev; \
	(ln)->ln_prev->ln_next = (ln)->ln_next; \
	} while (0)
#define LN_INSERTHEAD(ln) do { \
	(ln)->ln_next = llinfo_nd6.ln_next; \
	llinfo_nd6.ln_next = (ln); \
	(ln)->ln_prev = &llinfo_nd6; \
	(ln)->ln_next->ln_prev = (ln); \
	} while (0)

void
nd6_init(void)
{
	static int nd6_init_done = 0;

	if (nd6_init_done) {
		log(LOG_NOTICE, "nd6_init called more than once(ignored)\n");
		return;
	}

	/* initialization of the default router list */
	TAILQ_INIT(&nd_defrouter);

	task_set(&nd6_timer_task, nd6_timer_work, NULL);

	nd6_init_done = 1;

	/* start timer */
	timeout_set(&nd6_slowtimo_ch, nd6_slowtimo, NULL);
	timeout_add_sec(&nd6_slowtimo_ch, ND6_SLOWTIMER_INTERVAL);

	timeout_set(&nd6_rs_output_timer, nd6_rs_output_timo, NULL);
}

struct nd_ifinfo *
nd6_ifattach(struct ifnet *ifp)
{
	struct nd_ifinfo *nd;

	nd = malloc(sizeof(*nd), M_IP6NDP, M_WAITOK | M_ZERO);

	nd->initialized = 1;

	nd->chlim = IPV6_DEFHLIM;
	nd->basereachable = REACHABLE_TIME;
	nd->reachable = ND_COMPUTE_RTIME(nd->basereachable);
	nd->retrans = RETRANS_TIMER;
	/* per-interface IFXF_AUTOCONF6 needs to be set too to accept RAs */
	nd->flags = (ND6_IFF_PERFORMNUD | ND6_IFF_ACCEPT_RTADV);

	/* XXX: we cannot call nd6_setmtu since ifp is not fully initialized */
	nd6_setmtu0(ifp, nd);

	return nd;
}

void
nd6_ifdetach(struct nd_ifinfo *nd)
{

	free(nd, M_IP6NDP, 0);
}

void
nd6_setmtu(struct ifnet *ifp)
{
	nd6_setmtu0(ifp, ND_IFINFO(ifp));
}

void
nd6_setmtu0(struct ifnet *ifp, struct nd_ifinfo *ndi)
{
	u_int32_t omaxmtu;

	omaxmtu = ndi->maxmtu;
	ndi->maxmtu = ifp->if_mtu;

	/*
	 * Decreasing the interface MTU under IPV6 minimum MTU may cause
	 * undesirable situation.  We thus notify the operator of the change
	 * explicitly.  The check for omaxmtu is necessary to restrict the
	 * log to the case of changing the MTU, not initializing it.
	 */
	if (omaxmtu >= IPV6_MMTU && ndi->maxmtu < IPV6_MMTU) {
		log(LOG_NOTICE, "nd6_setmtu0: "
		    "new link MTU on %s (%lu) is too small for IPv6\n",
		    ifp->if_xname, (unsigned long)ndi->maxmtu);
	}
}

void
nd6_option_init(void *opt, int icmp6len, union nd_opts *ndopts)
{
	bzero(ndopts, sizeof(*ndopts));
	ndopts->nd_opts_search = (struct nd_opt_hdr *)opt;
	ndopts->nd_opts_last
		= (struct nd_opt_hdr *)(((u_char *)opt) + icmp6len);

	if (icmp6len == 0) {
		ndopts->nd_opts_done = 1;
		ndopts->nd_opts_search = NULL;
	}
}

/*
 * Take one ND option.
 */
struct nd_opt_hdr *
nd6_option(union nd_opts *ndopts)
{
	struct nd_opt_hdr *nd_opt;
	int olen;

	if (!ndopts)
		panic("ndopts == NULL in nd6_option");
	if (!ndopts->nd_opts_last)
		panic("uninitialized ndopts in nd6_option");
	if (!ndopts->nd_opts_search)
		return NULL;
	if (ndopts->nd_opts_done)
		return NULL;

	nd_opt = ndopts->nd_opts_search;

	/* make sure nd_opt_len is inside the buffer */
	if ((caddr_t)&nd_opt->nd_opt_len >= (caddr_t)ndopts->nd_opts_last) {
		bzero(ndopts, sizeof(*ndopts));
		return NULL;
	}

	olen = nd_opt->nd_opt_len << 3;
	if (olen == 0) {
		/*
		 * Message validation requires that all included
		 * options have a length that is greater than zero.
		 */
		bzero(ndopts, sizeof(*ndopts));
		return NULL;
	}

	ndopts->nd_opts_search = (struct nd_opt_hdr *)((caddr_t)nd_opt + olen);
	if (ndopts->nd_opts_search > ndopts->nd_opts_last) {
		/* option overruns the end of buffer, invalid */
		bzero(ndopts, sizeof(*ndopts));
		return NULL;
	} else if (ndopts->nd_opts_search == ndopts->nd_opts_last) {
		/* reached the end of options chain */
		ndopts->nd_opts_done = 1;
		ndopts->nd_opts_search = NULL;
	}
	return nd_opt;
}

/*
 * Parse multiple ND options.
 * This function is much easier to use, for ND routines that do not need
 * multiple options of the same type.
 */
int
nd6_options(union nd_opts *ndopts)
{
	struct nd_opt_hdr *nd_opt;
	int i = 0;

	if (!ndopts)
		panic("ndopts == NULL in nd6_options");
	if (!ndopts->nd_opts_last)
		panic("uninitialized ndopts in nd6_options");
	if (!ndopts->nd_opts_search)
		return 0;

	while (1) {
		nd_opt = nd6_option(ndopts);
		if (!nd_opt && !ndopts->nd_opts_last) {
			/*
			 * Message validation requires that all included
			 * options have a length that is greater than zero.
			 */
			icmp6stat.icp6s_nd_badopt++;
			bzero(ndopts, sizeof(*ndopts));
			return -1;
		}

		if (!nd_opt)
			goto skip1;

		switch (nd_opt->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_MTU:
		case ND_OPT_REDIRECTED_HEADER:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type]) {
				nd6log((LOG_INFO,
				    "duplicated ND6 option found (type=%d)\n",
				    nd_opt->nd_opt_type));
				/* XXX bark? */
			} else {
				ndopts->nd_opt_array[nd_opt->nd_opt_type]
					= nd_opt;
			}
			break;
		case ND_OPT_PREFIX_INFORMATION:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type] == 0) {
				ndopts->nd_opt_array[nd_opt->nd_opt_type]
					= nd_opt;
			}
			ndopts->nd_opts_pi_end =
				(struct nd_opt_prefix_info *)nd_opt;
			break;
		default:
			/*
			 * Unknown options must be silently ignored,
			 * to accommodate future extension to the protocol.
			 */
			nd6log((LOG_DEBUG,
			    "nd6_options: unsupported option %d - "
			    "option ignored\n", nd_opt->nd_opt_type));
		}

skip1:
		i++;
		if (i > nd6_maxndopt) {
			icmp6stat.icp6s_nd_toomanyopt++;
			nd6log((LOG_INFO, "too many loop in nd opt\n"));
			break;
		}

		if (ndopts->nd_opts_done)
			break;
	}

	return 0;
}

/*
 * ND6 timer routine to handle ND6 entries
 */
void
nd6_llinfo_settimer(struct llinfo_nd6 *ln, long tick)
{
	int s;

	s = splsoftnet();

	if (tick < 0) {
		ln->ln_expire = 0;
		ln->ln_ntick = 0;
		timeout_del(&ln->ln_timer_ch);
	} else {
		ln->ln_expire = time_second + tick / hz;
		if (tick > INT_MAX) {
			ln->ln_ntick = tick - INT_MAX;
			timeout_add(&ln->ln_timer_ch, INT_MAX);
		} else {
			ln->ln_ntick = 0;
			timeout_add(&ln->ln_timer_ch, tick);
		}
	}

	splx(s);
}

void
nd6_llinfo_timer(void *arg)
{
	int s;
	struct llinfo_nd6 *ln;
	struct rtentry *rt;
	struct sockaddr_in6 *dst;
	struct ifnet *ifp;
	struct nd_ifinfo *ndi = NULL;

	s = splsoftnet();

	ln = (struct llinfo_nd6 *)arg;

	if (ln->ln_ntick > 0) {
		if (ln->ln_ntick > INT_MAX) {
			ln->ln_ntick -= INT_MAX;
			nd6_llinfo_settimer(ln, INT_MAX);
		} else {
			ln->ln_ntick = 0;
			nd6_llinfo_settimer(ln, ln->ln_ntick);
		}
		splx(s);
		return;
	}

	if ((rt = ln->ln_rt) == NULL)
		panic("ln->ln_rt == NULL");
	if ((ifp = rt->rt_ifp) == NULL)
		panic("ln->ln_rt->rt_ifp == NULL");
	ndi = ND_IFINFO(ifp);
	dst = satosin6(rt_key(rt));

	/* sanity check */
	if (rt->rt_llinfo && (struct llinfo_nd6 *)rt->rt_llinfo != ln)
		panic("rt_llinfo(%p) is not equal to ln(%p)",
		      rt->rt_llinfo, ln);
	if (!dst)
		panic("dst=0 in nd6_timer(ln=%p)", ln);

	switch (ln->ln_state) {
	case ND6_LLINFO_INCOMPLETE:
		if (ln->ln_asked < nd6_mmaxtries) {
			ln->ln_asked++;
			nd6_llinfo_settimer(ln, (long)ndi->retrans * hz / 1000);
			nd6_ns_output(ifp, NULL, &dst->sin6_addr, ln, 0);
		} else {
			struct mbuf *m = ln->ln_hold;
			if (m) {
				ln->ln_hold = NULL;
				/*
				 * Fake rcvif to make the ICMP error
				 * more helpful in diagnosing for the
				 * receiver.
				 * XXX: should we consider
				 * older rcvif?
				 */
				m->m_pkthdr.rcvif = rt->rt_ifp;

				icmp6_error(m, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADDR, 0);
				if (ln->ln_hold == m) {
					/* m is back in ln_hold. Discard. */
					m_freem(ln->ln_hold);
					ln->ln_hold = NULL;
				}
			}
			(void)nd6_free(rt, 0);
			ln = NULL;
		}
		break;
	case ND6_LLINFO_REACHABLE:
		if (!ND6_LLINFO_PERMANENT(ln)) {
			ln->ln_state = ND6_LLINFO_STALE;
			nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
		}
		break;

	case ND6_LLINFO_STALE:
	case ND6_LLINFO_PURGE:
		/* Garbage Collection(RFC 2461 5.3) */
		if (!ND6_LLINFO_PERMANENT(ln)) {
			(void)nd6_free(rt, 1);
			ln = NULL;
		}
		break;

	case ND6_LLINFO_DELAY:
		if (ndi && (ndi->flags & ND6_IFF_PERFORMNUD) != 0) {
			/* We need NUD */
			ln->ln_asked = 1;
			ln->ln_state = ND6_LLINFO_PROBE;
			nd6_llinfo_settimer(ln, (long)ndi->retrans * hz / 1000);
			nd6_ns_output(ifp, &dst->sin6_addr,
			    &dst->sin6_addr, ln, 0);
		} else {
			ln->ln_state = ND6_LLINFO_STALE; /* XXX */
			nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
		}
		break;
	case ND6_LLINFO_PROBE:
		if (ln->ln_asked < nd6_umaxtries) {
			ln->ln_asked++;
			nd6_llinfo_settimer(ln, (long)ndi->retrans * hz / 1000);
			nd6_ns_output(ifp, &dst->sin6_addr,
			    &dst->sin6_addr, ln, 0);
		} else {
			(void)nd6_free(rt, 0);
			ln = NULL;
		}
		break;
	}

	splx(s);
}

/*
 * ND6 timer routine to expire default route list and prefix list
 */
void
nd6_timer_work(void *null)
{
	int s;
	struct nd_defrouter *dr, *ndr;
	struct nd_prefix *pr, *npr;
	struct in6_ifaddr *ia6, *nia6;

	s = splsoftnet();
	timeout_set(&nd6_timer_ch, nd6_timer, NULL);
	timeout_add_sec(&nd6_timer_ch, nd6_prune);

	/* expire default router list */
	TAILQ_FOREACH_SAFE(dr, &nd_defrouter, dr_entry, ndr)
		if (dr->expire && dr->expire < time_second)
			defrtrlist_del(dr);

	/*
	 * expire interface addresses.
	 * in the past the loop was inside prefix expiry processing.
	 * However, from a stricter spec-conformance standpoint, we should
	 * rather separate address lifetimes and prefix lifetimes.
	 */
	TAILQ_FOREACH_SAFE(ia6, &in6_ifaddr, ia_list, nia6) {
		/* check address lifetime */
		if (IFA6_IS_INVALID(ia6)) {
			in6_purgeaddr(&ia6->ia_ifa);
		} else if (IFA6_IS_DEPRECATED(ia6)) {
			ia6->ia6_flags |= IN6_IFF_DEPRECATED;
		} else {
			/*
			 * A new RA might have made a deprecated address
			 * preferred.
			 */
			ia6->ia6_flags &= ~IN6_IFF_DEPRECATED;
		}
	}

	/* expire prefix list */
	LIST_FOREACH_SAFE(pr, &nd_prefix, ndpr_entry, npr) {
		/*
		 * check prefix lifetime.
		 * since pltime is just for autoconf, pltime processing for
		 * prefix is not necessary.
		 */
		if (pr->ndpr_vltime != ND6_INFINITE_LIFETIME &&
		    time_second - pr->ndpr_lastupdate > pr->ndpr_vltime) {
			/*
			 * address expiration and prefix expiration are
			 * separate.  NEVER perform in6_purgeaddr here.
			 */

			prelist_remove(pr);
		}
	}
	splx(s);
}

void
nd6_timer(void *ignored_arg)
{
	task_add(systq, &nd6_timer_task);
}

/*
 * Nuke neighbor cache/prefix/default router management table, right before
 * ifp goes away.
 */
void
nd6_purge(struct ifnet *ifp)
{
	struct llinfo_nd6 *ln, *nln;
	struct nd_defrouter *dr, *ndr;
	struct nd_prefix *pr, *npr;

	/*
	 * Nuke default router list entries toward ifp.
	 * We defer removal of default router list entries that is installed
	 * in the routing table, in order to keep additional side effects as
	 * small as possible.
	 */
	TAILQ_FOREACH_SAFE(dr, &nd_defrouter, dr_entry, ndr) {
		if (dr->installed)
			continue;

		if (dr->ifp == ifp)
			defrtrlist_del(dr);
	}
	TAILQ_FOREACH_SAFE(dr, &nd_defrouter, dr_entry, ndr) {
		if (!dr->installed)
			continue;

		if (dr->ifp == ifp)
			defrtrlist_del(dr);
	}

	/* Nuke prefix list entries toward ifp */
	LIST_FOREACH_SAFE(pr, &nd_prefix, ndpr_entry, npr) {
		if (pr->ndpr_ifp == ifp)
			prelist_remove(pr);
	}

	if (ifp->if_xflags & IFXF_AUTOCONF6) {
		/* refresh default router list */
		defrouter_select();
	}

	/*
	 * Nuke neighbor cache entries for the ifp.
	 * Note that rt->rt_ifp may not be the same as ifp,
	 * due to KAME goto ours hack.  See RTM_RESOLVE case in
	 * nd6_rtrequest(), and ip6_input().
	 */
	ln = llinfo_nd6.ln_next;
	while (ln && ln != &llinfo_nd6) {
		struct rtentry *rt;
		struct sockaddr_dl *sdl;

		nln = ln->ln_next;
		rt = ln->ln_rt;
		if (rt && rt->rt_gateway &&
		    rt->rt_gateway->sa_family == AF_LINK) {
			sdl = (struct sockaddr_dl *)rt->rt_gateway;
			if (sdl->sdl_index == ifp->if_index)
				nln = nd6_free(rt, 0);
		}
		ln = nln;
	}
}

struct rtentry *
nd6_lookup(struct in6_addr *addr6, int create, struct ifnet *ifp,
    u_int rtableid)
{
	struct rtentry *rt;
	struct sockaddr_in6 sin6;
	int flags;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = *addr6;
	flags = (create) ? (RT_REPORT|RT_RESOLVE) : 0;

	rt = rtalloc(sin6tosa(&sin6), flags, rtableid);
	if (rt && (rt->rt_flags & RTF_LLINFO) == 0) {
		/*
		 * This is the case for the default route.
		 * If we want to create a neighbor cache for the address, we
		 * should free the route for the destination and allocate an
		 * interface route.
		 */
		if (create) {
			rtfree(rt);
			rt = NULL;
		}
	}
	if (!rt) {
		if (create && ifp) {
			struct rt_addrinfo info;
			int e;

			/*
			 * If no route is available and create is set,
			 * we allocate a host route for the destination
			 * and treat it like an interface route.
			 * This hack is necessary for a neighbor which can't
			 * be covered by our own prefix.
			 */
			struct ifaddr *ifa =
			    ifaof_ifpforaddr(sin6tosa(&sin6), ifp);
			if (ifa == NULL)
				return (NULL);

			/*
			 * Create a new route.  RTF_LLINFO is necessary
			 * to create a Neighbor Cache entry for the
			 * destination in nd6_rtrequest which will be
			 * called in rtrequest1 via ifa->ifa_rtrequest.
			 */
			bzero(&info, sizeof(info));
			info.rti_flags = RTF_UP | RTF_HOST | RTF_LLINFO;
			info.rti_info[RTAX_DST] = sin6tosa(&sin6);
			info.rti_info[RTAX_GATEWAY] =
			    (struct sockaddr *)ifp->if_sadl;
			if ((e = rtrequest1(RTM_ADD, &info, RTP_CONNECTED,
			    &rt, rtableid)) != 0) {
#if 0
				char ip[INET6_ADDRSTRLEN];
				log(LOG_ERR,
				    "nd6_lookup: failed to add route for a "
				    "neighbor(%s), errno=%d\n",
				    inet_ntop(AF_INET6, addr6, ip, sizeof(ip)),
				    e);
#endif
				return (NULL);
			}
			if (rt == NULL)
				return (NULL);
			if (rt->rt_llinfo) {
				struct llinfo_nd6 *ln =
				    (struct llinfo_nd6 *)rt->rt_llinfo;
				ln->ln_state = ND6_LLINFO_NOSTATE;
			}
		} else
			return (NULL);
	}
	rt->rt_refcnt--;
	/*
	 * Validation for the entry.
	 * Note that the check for rt_llinfo is necessary because a cloned
	 * route from a parent route that has the L flag (e.g. the default
	 * route to a p2p interface) may have the flag, too, while the
	 * destination is not actually a neighbor.
	 * XXX: we can't use rt->rt_ifp to check for the interface, since
	 *      it might be the loopback interface if the entry is for our
	 *      own address on a non-loopback interface. Instead, we should
	 *      use rt->rt_ifa->ifa_ifp, which would specify the REAL
	 *	interface.
	 */
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK || rt->rt_llinfo == NULL ||
	    (ifp && rt->rt_ifa->ifa_ifp != ifp)) {
		if (create) {
			char addr[INET6_ADDRSTRLEN];
			nd6log((LOG_DEBUG,
			    "nd6_lookup: failed to lookup %s (if = %s)\n",
			    inet_ntop(AF_INET6, addr6, addr, sizeof(addr)),
			    ifp ? ifp->if_xname : "unspec"));
		}
		return (NULL);
	}
	return (rt);
}

/*
 * Detect if a given IPv6 address identifies a neighbor on a given link.
 * XXX: should take care of the destination of a p2p link?
 */
int
nd6_is_addr_neighbor(struct sockaddr_in6 *addr, struct ifnet *ifp)
{
	struct nd_prefix *pr;
	struct rtentry *rt;

	/*
	 * A link-local address is always a neighbor.
	 * XXX: we should use the sin6_scope_id field rather than the embedded
	 * interface index.
	 * XXX: a link does not necessarily specify a single interface.
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr) &&
	    ntohs(*(u_int16_t *)&addr->sin6_addr.s6_addr[2]) == ifp->if_index)
		return (1);

	/*
	 * If the address matches one of our on-link prefixes, it should be a
	 * neighbor.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		if (pr->ndpr_ifp != ifp)
			continue;

		if (!(pr->ndpr_stateflags & NDPRF_ONLINK))
			continue;

		if (IN6_ARE_MASKED_ADDR_EQUAL(&pr->ndpr_prefix.sin6_addr,
		    &addr->sin6_addr, &pr->ndpr_mask))
			return (1);
	}

	/*
	 * Even if the address matches none of our addresses, it might be
	 * in the neighbor cache.
	 */
	if ((rt = nd6_lookup(&addr->sin6_addr, 0, ifp,
	    ifp->if_rdomain)) != NULL)
		return (1);

	return (0);
}

/*
 * Free an nd6 llinfo entry.
 * Since the function would cause significant changes in the kernel, DO NOT
 * make it global, unless you have a strong reason for the change, and are sure
 * that the change is safe.
 */
struct llinfo_nd6 *
nd6_free(struct rtentry *rt, int gc)
{
	struct rt_addrinfo info;
	struct llinfo_nd6 *ln = (struct llinfo_nd6 *)rt->rt_llinfo, *next;
	struct in6_addr in6 = satosin6(rt_key(rt))->sin6_addr;
	struct nd_defrouter *dr;
	int s;

	/*
	 * we used to have pfctlinput(PRC_HOSTDEAD) here.
	 * even though it is not harmful, it was not really necessary.
	 */

	s = splsoftnet();
	if (!ip6_forwarding) {
		dr = defrouter_lookup(&satosin6(rt_key(rt))->sin6_addr,
		    rt->rt_ifp);

		if (dr != NULL && dr->expire &&
		    ln->ln_state == ND6_LLINFO_STALE && gc) {
			/*
			 * If the reason for the deletion is just garbage
			 * collection, and the neighbor is an active default
			 * router, do not delete it.  Instead, reset the GC
			 * timer using the router's lifetime.
			 * Simply deleting the entry would affect default
			 * router selection, which is not necessarily a good
			 * thing, especially when we're using router preference
			 * values.
			 * XXX: the check for ln_state would be redundant,
			 *      but we intentionally keep it just in case.
			 */
			if (dr->expire > time_second * hz) {
				nd6_llinfo_settimer(ln,
				    dr->expire - time_second * hz);
			} else
				nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
			splx(s);
			return (ln->ln_next);
		}

		if (ln->ln_router || dr) {
			/*
			 * rt6_flush must be called whether or not the neighbor
			 * is in the Default Router List.
			 * See a corresponding comment in nd6_na_input().
			 */
			rt6_flush(&in6, rt->rt_ifp);
		}

		if (dr) {
			/*
			 * Unreachability of a router might affect the default
			 * router selection and on-link detection of advertised
			 * prefixes.
			 */

			/*
			 * Temporarily fake the state to choose a new default
			 * router and to perform on-link determination of
			 * prefixes correctly.
			 * Below the state will be set correctly,
			 * or the entry itself will be deleted.
			 */
			ln->ln_state = ND6_LLINFO_INCOMPLETE;

			/*
			 * Since defrouter_select() does not affect the
			 * on-link determination and MIP6 needs the check
			 * before the default router selection, we perform
			 * the check now.
			 */
			pfxlist_onlink_check();

			/*
			 * refresh default router list
			 */
			defrouter_select();
		}
	}

	/*
	 * Before deleting the entry, remember the next entry as the
	 * return value.  We need this because pfxlist_onlink_check() above
	 * might have freed other entries (particularly the old next entry) as
	 * a side effect (XXX).
	 */
	next = ln->ln_next;

	/*
	 * Detach the route from the routing tree and the list of neighbor
	 * caches, and disable the route entry not to be used in already
	 * cached routes.
	 */
	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	rtrequest1(RTM_DELETE, &info, rt->rt_priority, NULL,
	    rt->rt_ifp->if_rdomain);
	splx(s);

	return (next);
}

/*
 * Upper-layer reachability hint for Neighbor Unreachability Detection.
 *
 * XXX cost-effective methods?
 */
void
nd6_nud_hint(struct rtentry *rt, struct in6_addr *dst6, int force,
    u_int rtableid)
{
	struct llinfo_nd6 *ln;

	/*
	 * If the caller specified "rt", use that.  Otherwise, resolve the
	 * routing table by supplied "dst6".
	 */
	if (!rt) {
		if (!dst6)
			return;
		if (!(rt = nd6_lookup(dst6, 0, NULL, rtableid)))
			return;
	}

	if ((rt->rt_flags & RTF_GATEWAY) != 0 ||
	    (rt->rt_flags & RTF_LLINFO) == 0 ||
	    !rt->rt_llinfo || !rt->rt_gateway ||
	    rt->rt_gateway->sa_family != AF_LINK) {
		/* This is not a host route. */
		return;
	}

	ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	if (ln->ln_state < ND6_LLINFO_REACHABLE)
		return;

	/*
	 * if we get upper-layer reachability confirmation many times,
	 * it is possible we have false information.
	 */
	if (!force) {
		ln->ln_byhint++;
		if (ln->ln_byhint > nd6_maxnudhint)
			return;
	}

	ln->ln_state = ND6_LLINFO_REACHABLE;
	if (!ND6_LLINFO_PERMANENT(ln)) {
		nd6_llinfo_settimer(ln,
		    (long)ND_IFINFO(rt->rt_ifp)->reachable * hz);
	}
}

void
nd6_rtrequest(int req, struct rtentry *rt)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_nd6 *ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	struct ifnet *ifp = rt->rt_ifp;
	struct ifaddr *ifa;
	struct nd_defrouter *dr;

	if (req == RTM_DELETE && (rt->rt_flags & RTF_GATEWAY) &&
	    (IN6_ARE_ADDR_EQUAL(&(satosin6(rt_key(rt)))->sin6_addr,
	    &in6addr_any) && rt_mask(rt) && (rt_mask(rt)->sa_len == 0 ||
	    IN6_ARE_ADDR_EQUAL(&(satosin6(rt_mask(rt)))->sin6_addr,
	    &in6addr_any)))) {
		dr = defrouter_lookup(&satosin6(gate)->sin6_addr, ifp);
		if (dr)
			dr->installed = 0;
	}

	if ((rt->rt_flags & RTF_GATEWAY) != 0)
		return;

	if (nd6_need_cache(ifp) == 0 && (rt->rt_flags & RTF_HOST) == 0) {
		/*
		 * This is probably an interface direct route for a link
		 * which does not need neighbor caches (e.g. fe80::%lo0/64).
		 * We do not need special treatment below for such a route.
		 * Moreover, the RTF_LLINFO flag which would be set below
		 * would annoy the ndp(8) command.
		 */
		return;
	}

	if (req == RTM_RESOLVE && nd6_need_cache(ifp) == 0) {
		/*
		 * For routing daemons like ospf6d we allow neighbor discovery
		 * based on the cloning route only.  This allows us to sent
		 * packets directly into a network without having an address
		 * with matching prefix on the interface.  If the cloning
		 * route is used for an stf interface, we would mistakenly
		 * make a neighbor cache for the host route, and would see
		 * strange neighbor solicitation for the corresponding
		 * destination.  In order to avoid confusion, we check if the
		 * interface is suitable for neighbor discovery, and stop the
		 * process if not.  Additionally, we remove the LLINFO flag
		 * so that ndp(8) will not try to get the neighbor information
		 * of the destination.
		 */
		rt->rt_flags &= ~RTF_LLINFO;
		return;
	}

	switch (req) {
	case RTM_ADD:
		/*
		 * There is no backward compatibility :)
		 *
		 * if ((rt->rt_flags & RTF_HOST) == 0 &&
		 *     SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
		 *	   rt->rt_flags |= RTF_CLONING;
		 */
		if ((rt->rt_flags & RTF_CLONING) ||
		    ((rt->rt_flags & (RTF_LLINFO | RTF_LOCAL)) && !ln)) {
			if (ln)
				nd6_llinfo_settimer(ln, 0);
			if ((rt->rt_flags & RTF_CLONING) != 0)
				break;
		}
		/*
		 * In IPv4 code, we try to announce new RTF_ANNOUNCE entry here.
		 * We don't do that here since llinfo is not ready yet.
		 *
		 * There are also couple of other things to be discussed:
		 * - unsolicited NA code needs improvement beforehand
		 * - RFC2461 says we MAY send multicast unsolicited NA
		 *   (7.2.6 paragraph 4), however, it also says that we
		 *   SHOULD provide a mechanism to prevent multicast NA storm.
		 *   we don't have anything like it right now.
		 *   note that the mechanism needs a mutual agreement
		 *   between proxies, which means that we need to implement
		 *   a new protocol, or a new kludge.
		 * - from RFC2461 6.2.4, host MUST NOT send an unsolicited NA.
		 *   we need to check ip6forwarding before sending it.
		 *   (or should we allow proxy ND configuration only for
		 *   routers?  there's no mention about proxy ND from hosts)
		 */
#if 0
		/* XXX it does not work */
		if (rt->rt_flags & RTF_ANNOUNCE)
			nd6_na_output(ifp,
			      &satosin6(rt_key(rt))->sin6_addr,
			      &satosin6(rt_key(rt))->sin6_addr,
			      ip6_forwarding ? ND_NA_FLAG_ROUTER : 0,
			      1, NULL);
#endif
		/* FALLTHROUGH */
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sizeof(struct sockaddr_dl)) {
			log(LOG_DEBUG, "%s: bad gateway value: %s\n",
			    __func__, ifp->if_xname);
			break;
		}
		SDL(gate)->sdl_type = ifp->if_type;
		SDL(gate)->sdl_index = ifp->if_index;
		if (ln != NULL)
			break;	/* This happens on a route change */
		/*
		 * Case 2: This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		ln = malloc(sizeof(*ln), M_RTABLE, M_NOWAIT | M_ZERO);
		rt->rt_llinfo = (caddr_t)ln;
		if (!ln) {
			log(LOG_DEBUG, "%s: malloc failed\n", __func__);
			break;
		}
		nd6_inuse++;
		nd6_allocated++;
		ln->ln_rt = rt;
		timeout_set(&ln->ln_timer_ch, nd6_llinfo_timer, ln);
		/* this is required for "ndp" command. - shin */
		if (req == RTM_ADD) {
		        /*
			 * gate should have some valid AF_LINK entry,
			 * and ln->ln_expire should have some lifetime
			 * which is specified by ndp command.
			 */
			ln->ln_state = ND6_LLINFO_REACHABLE;
			ln->ln_byhint = 0;
		} else {
		        /*
			 * When req == RTM_RESOLVE, rt is created and
			 * initialized in rtrequest(), so rt_expire is 0.
			 */
			ln->ln_state = ND6_LLINFO_NOSTATE;
			nd6_llinfo_settimer(ln, 0);
		}
		rt->rt_flags |= RTF_LLINFO;
		ln->ln_next = llinfo_nd6.ln_next;
		llinfo_nd6.ln_next = ln;
		ln->ln_prev = &llinfo_nd6;
		ln->ln_next->ln_prev = ln;

		/*
		 * If we have too many cache entries, initiate immediate
		 * purging for some "less recently used" entries.  Note that
		 * we cannot directly call nd6_free() here because it would
		 * cause re-entering rtable related routines triggering an LOR
		 * problem for FreeBSD.
		 */
		if (ip6_neighborgcthresh >= 0 &&
		    nd6_inuse >= ip6_neighborgcthresh) {
			int i;

			for (i = 0; i < 10 && llinfo_nd6.ln_prev != ln; i++) {
				struct llinfo_nd6 *ln_end = llinfo_nd6.ln_prev;

				/* Move this entry to the head */
				LN_DEQUEUE(ln_end);
				LN_INSERTHEAD(ln_end);

				if (ND6_LLINFO_PERMANENT(ln_end))
					continue;

				if (ln_end->ln_state > ND6_LLINFO_INCOMPLETE)
					ln_end->ln_state = ND6_LLINFO_STALE;
				else
					ln_end->ln_state = ND6_LLINFO_PURGE;
				nd6_llinfo_settimer(ln_end, 0);
			}
		}

		/*
		 * check if rt_key(rt) is one of my address assigned
		 * to the interface.
		 */
		ifa = &in6ifa_ifpwithaddr(ifp,
		    &satosin6(rt_key(rt))->sin6_addr)->ia_ifa;
		if (ifa) {
			nd6_llinfo_settimer(ln, -1);
			ln->ln_state = ND6_LLINFO_REACHABLE;
			ln->ln_byhint = 0;

			/*
			 * XXX Since lo0 is in the default rdomain we
			 * should not (ab)use it for any route related
			 * to an interface of a different rdomain.
			 */
			rt->rt_ifp = lo0ifp;

			/*
			 * Make sure rt_ifa be equal to the ifaddr
			 * corresponding to the address.
			 * We need this because when we refer
			 * rt_ifa->ia6_flags in ip6_input, we assume
			 * that the rt_ifa points to the address instead
			 * of the loopback address.
			 */
			if (ifa != rt->rt_ifa) {
				ifafree(rt->rt_ifa);
				ifa->ifa_refcnt++;
				rt->rt_ifa = ifa;
			}
		} else if (rt->rt_flags & RTF_ANNOUNCE) {
			nd6_llinfo_settimer(ln, -1);
			ln->ln_state = ND6_LLINFO_REACHABLE;
			ln->ln_byhint = 0;

			/* join solicited node multicast for proxy ND */
			if (ifp->if_flags & IFF_MULTICAST) {
				struct in6_addr llsol;
				int error;

				llsol = satosin6(rt_key(rt))->sin6_addr;
				llsol.s6_addr16[0] = htons(0xff02);
				llsol.s6_addr16[1] = htons(ifp->if_index);
				llsol.s6_addr32[1] = 0;
				llsol.s6_addr32[2] = htonl(1);
				llsol.s6_addr8[12] = 0xff;

				if (in6_addmulti(&llsol, ifp, &error)) {
					char addr[INET6_ADDRSTRLEN];
					nd6log((LOG_ERR, "%s: failed to join "
					    "%s (errno=%d)\n", ifp->if_xname,
					    inet_ntop(AF_INET6, &llsol,
						addr, sizeof(addr)),
					    error));
				}
			}
		}
		break;

	case RTM_DELETE:
		if (!ln)
			break;
		/* leave from solicited node multicast for proxy ND */
		if ((rt->rt_flags & RTF_ANNOUNCE) != 0 &&
		    (ifp->if_flags & IFF_MULTICAST) != 0) {
			struct in6_addr llsol;
			struct in6_multi *in6m;

			llsol = satosin6(rt_key(rt))->sin6_addr;
			llsol.s6_addr16[0] = htons(0xff02);
			llsol.s6_addr16[1] = htons(ifp->if_index);
			llsol.s6_addr32[1] = 0;
			llsol.s6_addr32[2] = htonl(1);
			llsol.s6_addr8[12] = 0xff;

			IN6_LOOKUP_MULTI(llsol, ifp, in6m);
			if (in6m)
				in6_delmulti(in6m);
		}
		nd6_inuse--;
		ln->ln_next->ln_prev = ln->ln_prev;
		ln->ln_prev->ln_next = ln->ln_next;
		ln->ln_prev = NULL;
		nd6_llinfo_settimer(ln, -1);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (ln->ln_hold)
			m_freem(ln->ln_hold);
		free(ln, M_RTABLE, 0);
	}
}

int
nd6_ioctl(u_long cmd, caddr_t data, struct ifnet *ifp)
{
	struct in6_ndireq *ndi = (struct in6_ndireq *)data;
	struct in6_nbrinfo *nbi = (struct in6_nbrinfo *)data;
	struct rtentry *rt;
	int error = 0;
	int s;

	switch (cmd) {
	case SIOCGIFINFO_IN6:
		ndi->ndi = *ND_IFINFO(ifp);
		break;
	case SIOCSIFINFO_FLAGS:
		ND_IFINFO(ifp)->flags = ndi->ndi.flags;
		break;
	case SIOCSNDFLUSH_IN6:	/* XXX: the ioctl name is confusing... */
		/* sync kernel routing table with the default router list */
		defrouter_reset();
		defrouter_select();
		break;
	case SIOCSPFXFLUSH_IN6:
	{
		/* flush all the prefix advertised by routers */
		struct nd_prefix *pr, *npr;

		s = splsoftnet();
		/* First purge the addresses referenced by a prefix. */
		LIST_FOREACH_SAFE(pr, &nd_prefix, ndpr_entry, npr) {
			struct in6_ifaddr *ia6, *ia6_next;

			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue; /* XXX */

			/* do we really have to remove addresses as well? */
			TAILQ_FOREACH_SAFE(ia6, &in6_ifaddr, ia_list, ia6_next) {
				if ((ia6->ia6_flags & IN6_IFF_AUTOCONF) == 0)
					continue;

				if (ia6->ia6_ndpr == pr)
					in6_purgeaddr(&ia6->ia_ifa);
			}
		}
		/*
		 * Purging the addresses might remove the prefix as well.
		 * So run the loop again to access only prefixes that have
		 * not been freed already.
		 */
		LIST_FOREACH_SAFE(pr, &nd_prefix, ndpr_entry, npr) {
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue; /* XXX */

			prelist_remove(pr);
		}
		splx(s);
		break;
	}
	case SIOCSRTRFLUSH_IN6:
	{
		/* flush all the default routers */
		struct nd_defrouter *dr, *ndr;

		s = splsoftnet();
		defrouter_reset();
		TAILQ_FOREACH_SAFE(dr, &nd_defrouter, dr_entry, ndr)
			defrtrlist_del(dr);
		defrouter_select();
		splx(s);
		break;
	}
	case SIOCGNBRINFO_IN6:
	{
		struct llinfo_nd6 *ln;
		struct in6_addr nb_addr = nbi->addr; /* make local for safety */

		/*
		 * XXX: KAME specific hack for scoped addresses
		 *      XXXX: for other scopes than link-local?
		 */
		if (IN6_IS_ADDR_LINKLOCAL(&nbi->addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&nbi->addr)) {
			u_int16_t *idp = (u_int16_t *)&nb_addr.s6_addr[2];

			if (*idp == 0)
				*idp = htons(ifp->if_index);
		}

		s = splsoftnet();
		if ((rt = nd6_lookup(&nb_addr, 0, ifp, ifp->if_rdomain)) == NULL ||
		    (ln = (struct llinfo_nd6 *)rt->rt_llinfo) == NULL) {
			error = EINVAL;
			splx(s);
			break;
		}
		nbi->state = ln->ln_state;
		nbi->asked = ln->ln_asked;
		nbi->isrouter = ln->ln_router;
		nbi->expire = ln->ln_expire;
		splx(s);

		break;
	}
	}
	return (error);
}

/*
 * Create neighbor cache entry and cache link-layer address,
 * on reception of inbound ND6 packets.  (RS/RA/NS/redirect)
 *
 * type - ICMP6 type
 * code - type dependent information
 */
struct rtentry *
nd6_cache_lladdr(struct ifnet *ifp, struct in6_addr *from, char *lladdr,
    int lladdrlen, int type, int code)
{
	struct rtentry *rt = NULL;
	struct llinfo_nd6 *ln = NULL;
	int is_newentry;
	struct sockaddr_dl *sdl = NULL;
	int do_update;
	int olladdr;
	int llchange;
	int newstate = 0;

	if (!ifp)
		panic("ifp == NULL in nd6_cache_lladdr");
	if (!from)
		panic("from == NULL in nd6_cache_lladdr");

	/* nothing must be updated for unspecified address */
	if (IN6_IS_ADDR_UNSPECIFIED(from))
		return NULL;

	/*
	 * Validation about ifp->if_addrlen and lladdrlen must be done in
	 * the caller.
	 *
	 * XXX If the link does not have link-layer address, what should
	 * we do? (ifp->if_addrlen == 0)
	 * Spec says nothing in sections for RA, RS and NA.  There's small
	 * description on it in NS section (RFC 2461 7.2.3).
	 */

	rt = nd6_lookup(from, 0, ifp, ifp->if_rdomain);
	if (!rt) {
#if 0
		/* nothing must be done if there's no lladdr */
		if (!lladdr || !lladdrlen)
			return NULL;
#endif

		rt = nd6_lookup(from, 1, ifp, ifp->if_rdomain);
		is_newentry = 1;
	} else {
		/* do nothing if static ndp is set */
		if (rt->rt_flags & RTF_STATIC)
			return NULL;
		is_newentry = 0;
	}

	if (!rt)
		return NULL;
	if ((rt->rt_flags & (RTF_GATEWAY | RTF_LLINFO)) != RTF_LLINFO) {
fail:
		(void)nd6_free(rt, 0);
		return NULL;
	}
	ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	if (!ln)
		goto fail;
	if (!rt->rt_gateway)
		goto fail;
	if (rt->rt_gateway->sa_family != AF_LINK)
		goto fail;
	sdl = SDL(rt->rt_gateway);

	olladdr = (sdl->sdl_alen) ? 1 : 0;
	if (olladdr && lladdr) {
		if (bcmp(lladdr, LLADDR(sdl), ifp->if_addrlen))
			llchange = 1;
		else
			llchange = 0;
	} else
		llchange = 0;

	/*
	 * newentry olladdr  lladdr  llchange	(*=record)
	 *	0	n	n	--	(1)
	 *	0	y	n	--	(2)
	 *	0	n	y	--	(3) * STALE
	 *	0	y	y	n	(4) *
	 *	0	y	y	y	(5) * STALE
	 *	1	--	n	--	(6)   NOSTATE(= PASSIVE)
	 *	1	--	y	--	(7) * STALE
	 */

	if (llchange) {
		char addr[INET6_ADDRSTRLEN];
		log(LOG_INFO, "ndp info overwritten for %s by %s on %s\n",
		    inet_ntop(AF_INET6, from, addr, sizeof(addr)),
		    ether_sprintf(lladdr), ifp->if_xname);
	}
	if (lladdr) {		/* (3-5) and (7) */
		/*
		 * Record source link-layer address
		 * XXX is it dependent to ifp->if_type?
		 */
		sdl->sdl_alen = ifp->if_addrlen;
		bcopy(lladdr, LLADDR(sdl), ifp->if_addrlen);
	}

	if (!is_newentry) {
		if ((!olladdr && lladdr) ||		/* (3) */
		    (olladdr && lladdr && llchange)) {	/* (5) */
			do_update = 1;
			newstate = ND6_LLINFO_STALE;
		} else					/* (1-2,4) */
			do_update = 0;
	} else {
		do_update = 1;
		if (!lladdr)				/* (6) */
			newstate = ND6_LLINFO_NOSTATE;
		else					/* (7) */
			newstate = ND6_LLINFO_STALE;
	}

	if (do_update) {
		/*
		 * Update the state of the neighbor cache.
		 */
		ln->ln_state = newstate;

		if (ln->ln_state == ND6_LLINFO_STALE) {
			/*
			 * XXX: since nd6_output() below will cause
			 * state transition to DELAY and reset the timer,
			 * we must set the timer now, although it is actually
			 * meaningless.
			 */
			nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);

			if (ln->ln_hold) {
				struct mbuf *n = ln->ln_hold;
				ln->ln_hold = NULL;
				/*
				 * we assume ifp is not a p2p here, so just
				 * set the 2nd argument as the 1st one.
				 */
				nd6_output(ifp, n, satosin6(rt_key(rt)), rt);
				if (ln->ln_hold == n) {
					/* n is back in ln_hold. Discard. */
					m_freem(ln->ln_hold);
					ln->ln_hold = NULL;
				}
			}
		} else if (ln->ln_state == ND6_LLINFO_INCOMPLETE) {
			/* probe right away */
			nd6_llinfo_settimer((void *)ln, 0);
		}
	}

	/*
	 * ICMP6 type dependent behavior.
	 *
	 * NS: clear IsRouter if new entry
	 * RS: clear IsRouter
	 * RA: set IsRouter if there's lladdr
	 * redir: clear IsRouter if new entry
	 *
	 * RA case, (1):
	 * The spec says that we must set IsRouter in the following cases:
	 * - If lladdr exist, set IsRouter.  This means (1-5).
	 * - If it is old entry (!newentry), set IsRouter.  This means (7).
	 * So, based on the spec, in (1-5) and (7) cases we must set IsRouter.
	 * A question arises for (1) case.  (1) case has no lladdr in the
	 * neighbor cache, this is similar to (6).
	 * This case is rare but we figured that we MUST NOT set IsRouter.
	 *
	 * newentry olladdr  lladdr  llchange	    NS  RS  RA	redir
	 *							D R
	 *	0	n	n	--	(1)	c   ?     s
	 *	0	y	n	--	(2)	c   s     s
	 *	0	n	y	--	(3)	c   s     s
	 *	0	y	y	n	(4)	c   s     s
	 *	0	y	y	y	(5)	c   s     s
	 *	1	--	n	--	(6) c	c	c s
	 *	1	--	y	--	(7) c	c   s	c s
	 *
	 *					(c=clear s=set)
	 */
	switch (type & 0xff) {
	case ND_NEIGHBOR_SOLICIT:
		/*
		 * New entry must have is_router flag cleared.
		 */
		if (is_newentry)	/* (6-7) */
			ln->ln_router = 0;
		break;
	case ND_REDIRECT:
		/*
		 * If the icmp is a redirect to a better router, always set the
		 * is_router flag.  Otherwise, if the entry is newly created,
		 * clear the flag.  [RFC 2461, sec 8.3]
		 */
		if (code == ND_REDIRECT_ROUTER)
			ln->ln_router = 1;
		else if (is_newentry) /* (6-7) */
			ln->ln_router = 0;
		break;
	case ND_ROUTER_SOLICIT:
		/*
		 * is_router flag must always be cleared.
		 */
		ln->ln_router = 0;
		break;
	case ND_ROUTER_ADVERT:
		/*
		 * Mark an entry with lladdr as a router.
		 */
		if ((!is_newentry && (olladdr || lladdr)) ||	/* (2-5) */
		    (is_newentry && lladdr)) {			/* (7) */
			ln->ln_router = 1;
		}
		break;
	}

	/*
	 * When the link-layer address of a router changes, select the
	 * best router again.  In particular, when the neighbor entry is newly
	 * created, it might affect the selection policy.
	 * Question: can we restrict the first condition to the "is_newentry"
	 * case?
	 * XXX: when we hear an RA from a new router with the link-layer
	 * address option, defrouter_select() is called twice, since
	 * defrtrlist_update called the function as well.  However, I believe
	 * we can compromise the overhead, since it only happens the first
	 * time.
	 */
	if (do_update && ln->ln_router && (ifp->if_xflags & IFXF_AUTOCONF6))
		defrouter_select();

	return rt;
}

void
nd6_slowtimo(void *ignored_arg)
{
	int s = splsoftnet();
	struct nd_ifinfo *nd6if;
	struct ifnet *ifp;

	timeout_set(&nd6_slowtimo_ch, nd6_slowtimo, NULL);
	timeout_add_sec(&nd6_slowtimo_ch, ND6_SLOWTIMER_INTERVAL);
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		nd6if = ND_IFINFO(ifp);
		if (nd6if->basereachable && /* already initialized */
		    (nd6if->recalctm -= ND6_SLOWTIMER_INTERVAL) <= 0) {
			/*
			 * Since reachable time rarely changes by router
			 * advertisements, we SHOULD insure that a new random
			 * value gets recomputed at least once every few hours.
			 * (RFC 2461, 6.3.4)
			 */
			nd6if->recalctm = nd6_recalc_reachtm_interval;
			nd6if->reachable = ND_COMPUTE_RTIME(nd6if->basereachable);
		}
	}
	splx(s);
}

void
nd6_rs_output_set_timo(int timeout)
{
	nd6_rs_output_timeout = timeout;
	timeout_add_sec(&nd6_rs_output_timer, nd6_rs_output_timeout);
}

void
nd6_rs_output_timo(void *ignored_arg)
{
	struct ifnet *ifp;
	struct in6_ifaddr *ia6;

	if (nd6_rs_timeout_count == 0)
		return;

	if (nd6_rs_output_timeout < ND6_RS_OUTPUT_INTERVAL)
		/* exponential backoff if running quick timeouts */
		nd6_rs_output_timeout *= 2;
	if (nd6_rs_output_timeout > ND6_RS_OUTPUT_INTERVAL)
		nd6_rs_output_timeout = ND6_RS_OUTPUT_INTERVAL;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ISSET(ifp->if_flags, IFF_RUNNING) &&
		    ISSET(ifp->if_xflags, IFXF_AUTOCONF6)) {
			ia6 = in6ifa_ifpforlinklocal(ifp, IN6_IFF_TENTATIVE);
			if (ia6 != NULL)
				nd6_rs_output(ifp, ia6);
		}
	}
	timeout_add_sec(&nd6_rs_output_timer, nd6_rs_output_timeout);
}

#define senderr(e) { error = (e); goto bad;}
int
nd6_output(struct ifnet *ifp, struct mbuf *m0, struct sockaddr_in6 *dst,
    struct rtentry *rt0)
{
	struct mbuf *m = m0;
	struct rtentry *rt = rt0;
	struct llinfo_nd6 *ln = NULL;
	int error = 0;

	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr))
		goto sendpkt;

	if (nd6_need_cache(ifp) == 0)
		goto sendpkt;

	/*
	 * next hop determination.
	 */
	if (rt0 != NULL) {
		error = rt_checkgate(ifp, rt0, sin6tosa(dst),
		    m->m_pkthdr.ph_rtableid, &rt);
		if (error) {
			m_freem(m);
			return (error);
		}

		/*
		 * We skip link-layer address resolution and NUD
		 * if the gateway is not a neighbor from ND point
		 * of view, regardless of the value of nd_ifinfo.flags.
		 * The second condition is a bit tricky; we skip
		 * if the gateway is our own address, which is
		 * sometimes used to install a route to a p2p link.
		 */
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    ((nd6_is_addr_neighbor(satosin6(rt_key(rt)), ifp) == 0) ||
		    in6ifa_ifpwithaddr(ifp, &satosin6(rt_key(rt))->sin6_addr)))
			goto sendpkt;
	}

	/*
	 * Address resolution or Neighbor Unreachability Detection
	 * for the next hop.
	 * At this point, the destination of the packet must be a unicast
	 * or an anycast address(i.e. not a multicast).
	 */

	/* Look up the neighbor cache for the nexthop */
	if (rt && (rt->rt_flags & RTF_LLINFO) != 0)
		ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	else {
		/*
		 * Since nd6_is_addr_neighbor() internally calls nd6_lookup(),
		 * the condition below is not very efficient.  But we believe
		 * it is tolerable, because this should be a rare case.
		 */
		if (nd6_is_addr_neighbor(dst, ifp) &&
		    (rt = nd6_lookup(&dst->sin6_addr, 1, ifp,
		     ifp->if_rdomain)) != NULL)
			ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	}
	if (!ln || !rt) {
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0 &&
		    !(ND_IFINFO(ifp)->flags & ND6_IFF_PERFORMNUD)) {
			char addr[INET6_ADDRSTRLEN];

			log(LOG_DEBUG,
			    "nd6_output: can't allocate llinfo for %s "
			    "(ln=%p, rt=%p)\n",
			    inet_ntop(AF_INET6, &dst->sin6_addr,
				addr, sizeof(addr)),
			    ln, rt);
			senderr(EIO);	/* XXX: good error? */
		}

		goto sendpkt;	/* send anyway */
	}

	/*
	 * Move this entry to the head of the queue so that it is less likely
	 * for this entry to be a target of forced garbage collection (see
	 * nd6_rtrequest()).
	 */
	LN_DEQUEUE(ln);
	LN_INSERTHEAD(ln);

	/* We don't have to do link-layer address resolution on a p2p link. */
	if ((ifp->if_flags & IFF_POINTOPOINT) != 0 &&
	    ln->ln_state < ND6_LLINFO_REACHABLE) {
		ln->ln_state = ND6_LLINFO_STALE;
		nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
	}

	/*
	 * The first time we send a packet to a neighbor whose entry is
	 * STALE, we have to change the state to DELAY and a sets a timer to
	 * expire in DELAY_FIRST_PROBE_TIME seconds to ensure do
	 * neighbor unreachability detection on expiration.
	 * (RFC 2461 7.3.3)
	 */
	if (ln->ln_state == ND6_LLINFO_STALE) {
		ln->ln_asked = 0;
		ln->ln_state = ND6_LLINFO_DELAY;
		nd6_llinfo_settimer(ln, nd6_delay * hz);
	}

	/*
	 * If the neighbor cache entry has a state other than INCOMPLETE
	 * (i.e. its link-layer address is already resolved), just
	 * send the packet.
	 */
	if (ln->ln_state > ND6_LLINFO_INCOMPLETE)
		goto sendpkt;

	/*
	 * There is a neighbor cache entry, but no ethernet address
	 * response yet.  Replace the held mbuf (if any) with this
	 * latest one.
	 */
	if (ln->ln_state == ND6_LLINFO_NOSTATE)
		ln->ln_state = ND6_LLINFO_INCOMPLETE;
	if (ln->ln_hold)
		m_freem(ln->ln_hold);
	ln->ln_hold = m;
	/*
	 * If there has been no NS for the neighbor after entering the
	 * INCOMPLETE state, send the first solicitation.
	 */
	if (!ND6_LLINFO_PERMANENT(ln) && ln->ln_asked == 0) {
		ln->ln_asked++;
		nd6_llinfo_settimer(ln,
		    (long)ND_IFINFO(ifp)->retrans * hz / 1000);
		nd6_ns_output(ifp, NULL, &dst->sin6_addr, ln, 0);
	}
	return (0);

  sendpkt:
	return ((*ifp->if_output)(ifp, m, sin6tosa(dst), rt));

  bad:
	if (m)
		m_freem(m);
	return (error);
}
#undef senderr

int
nd6_need_cache(struct ifnet *ifp)
{
	/*
	 * RFC2893 says:
	 * - unidirectional tunnels needs no ND
	 */
	switch (ifp->if_type) {
	case IFT_ETHER:
	case IFT_IEEE1394:
	case IFT_PROPVIRTUAL:
	case IFT_IEEE80211:
	case IFT_CARP:
	case IFT_GIF:		/* XXX need more cases? */
		return (1);
	default:
		return (0);
	}
}

int
nd6_storelladdr(struct ifnet *ifp, struct rtentry *rt0, struct mbuf *m,
    struct sockaddr *dst, u_char *desten)
{
	struct sockaddr_dl *sdl;
	struct rtentry *rt;
	int error;

	if (m->m_flags & M_MCAST) {
		switch (ifp->if_type) {
		case IFT_ETHER:
			ETHER_MAP_IPV6_MULTICAST(&satosin6(dst)->sin6_addr,
						 desten);
			return (0);
			break;
		default:
			m_freem(m);
			return (EINVAL);
		}
	}

	if (rt0 == NULL) {
		/* this could happen, if we could not allocate memory */
		m_freem(m);
		return (ENOMEM);
	}

	error = rt_checkgate(ifp, rt0, dst, m->m_pkthdr.ph_rtableid, &rt);
	if (error) {
		m_freem(m);
		return (error);
	}

	if (rt->rt_gateway->sa_family != AF_LINK) {
		printf("nd6_storelladdr: something odd happens\n");
		m_freem(m);
		return (EINVAL);
	}
	sdl = SDL(rt->rt_gateway);
	if (sdl->sdl_alen != ETHER_ADDR_LEN) {
		char addr[INET6_ADDRSTRLEN];
		log(LOG_DEBUG, "%s: %s: incorrect nd6 information\n", __func__,
		    inet_ntop(AF_INET6, &satosin6(dst)->sin6_addr,
			addr, sizeof(addr)));
		m_freem(m);
		return (EINVAL);
	}

	bcopy(LLADDR(sdl), desten, sdl->sdl_alen);
	return (0);
}

/*
 * oldp - syscall arg, need copyout
 * newp - syscall arg, need copyin
 */

int
nd6_sysctl(int name, void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
	void *p;
	size_t ol;
	int error;

	error = 0;

	if (newp)
		return EPERM;
	if (oldp && !oldlenp)
		return EINVAL;
	ol = oldlenp ? *oldlenp : 0;

	if (oldp) {
		p = malloc(*oldlenp, M_TEMP, M_WAITOK | M_CANFAIL);
		if (!p)
			return ENOMEM;
	} else
		p = NULL;
	switch (name) {
	case ICMPV6CTL_ND6_DRLIST:
		error = fill_drlist(p, oldlenp, ol);
		if (!error && p && oldp)
			error = copyout(p, oldp, *oldlenp);
		break;

	case ICMPV6CTL_ND6_PRLIST:
		error = fill_prlist(p, oldlenp, ol);
		if (!error && p && oldp)
			error = copyout(p, oldp, *oldlenp);
		break;

	default:
		error = ENOPROTOOPT;
		break;
	}
	if (p)
		free(p, M_TEMP, 0);

	return (error);
}

int
fill_drlist(void *oldp, size_t *oldlenp, size_t ol)
{
	int error = 0, s;
	struct in6_defrouter *d = NULL, *de = NULL;
	struct nd_defrouter *dr;
	size_t l;

	s = splsoftnet();

	if (oldp) {
		d = (struct in6_defrouter *)oldp;
		de = (struct in6_defrouter *)((caddr_t)oldp + *oldlenp);
	}
	l = 0;

	TAILQ_FOREACH(dr, &nd_defrouter, dr_entry) {
		if (oldp && d + 1 <= de) {
			bzero(d, sizeof(*d));
			d->rtaddr.sin6_family = AF_INET6;
			d->rtaddr.sin6_len = sizeof(struct sockaddr_in6);
			d->rtaddr.sin6_addr = dr->rtaddr;
			in6_recoverscope(&d->rtaddr, &d->rtaddr.sin6_addr,
			    dr->ifp);
			d->flags = dr->flags;
			d->rtlifetime = dr->rtlifetime;
			d->expire = dr->expire;
			d->if_index = dr->ifp->if_index;
		}

		l += sizeof(*d);
		if (d)
			d++;
	}

	if (oldp) {
		*oldlenp = l;	/* (caddr_t)d - (caddr_t)oldp */
		if (l > ol)
			error = ENOMEM;
	} else
		*oldlenp = l;

	splx(s);

	return (error);
}

int
fill_prlist(void *oldp, size_t *oldlenp, size_t ol)
{
	int error = 0, s;
	struct nd_prefix *pr;
	struct in6_prefix *p = NULL;
	struct in6_prefix *pe = NULL;
	size_t l;

	s = splsoftnet();

	if (oldp) {
		p = (struct in6_prefix *)oldp;
		pe = (struct in6_prefix *)((caddr_t)oldp + *oldlenp);
	}
	l = 0;

	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		u_short advrtrs;
		size_t advance;
		struct sockaddr_in6 *sin6;
		struct sockaddr_in6 *s6;
		struct nd_pfxrouter *pfr;
		char addr[INET6_ADDRSTRLEN];

		if (oldp && p + 1 <= pe)
		{
			bzero(p, sizeof(*p));
			sin6 = (struct sockaddr_in6 *)(p + 1);

			p->prefix = pr->ndpr_prefix;
			if (in6_recoverscope(&p->prefix,
			    &p->prefix.sin6_addr, pr->ndpr_ifp) != 0)
				log(LOG_ERR,
				    "scope error in prefix list (%s)\n",
				    inet_ntop(AF_INET6, &p->prefix.sin6_addr,
					addr, sizeof(addr)));
			p->raflags = pr->ndpr_raf;
			p->prefixlen = pr->ndpr_plen;
			p->vltime = pr->ndpr_vltime;
			p->pltime = pr->ndpr_pltime;
			p->if_index = pr->ndpr_ifp->if_index;
			if (pr->ndpr_vltime == ND6_INFINITE_LIFETIME)
				p->expire = 0;
			else {
				time_t maxexpire;

				/* XXX: we assume time_t is signed. */
				maxexpire = (time_t)~(1ULL <<
				    ((sizeof(maxexpire) * 8) - 1));
				if (pr->ndpr_vltime <
				    maxexpire - pr->ndpr_lastupdate) {
					p->expire = pr->ndpr_lastupdate +
						pr->ndpr_vltime;
				} else
					p->expire = maxexpire;
			}
			p->refcnt = pr->ndpr_refcnt;
			p->flags = pr->ndpr_stateflags;
			p->origin = PR_ORIG_RA;
			advrtrs = 0;
			LIST_FOREACH(pfr, &pr->ndpr_advrtrs, pfr_entry) {
				if ((void *)&sin6[advrtrs + 1] > (void *)pe) {
					advrtrs++;
					continue;
				}
				s6 = &sin6[advrtrs];
				s6->sin6_family = AF_INET6;
				s6->sin6_len = sizeof(struct sockaddr_in6);
				s6->sin6_addr = pfr->router->rtaddr;
				in6_recoverscope(s6, &pfr->router->rtaddr,
				    pfr->router->ifp);
				advrtrs++;
			}
			p->advrtrs = advrtrs;
		}
		else {
			advrtrs = 0;
			LIST_FOREACH(pfr, &pr->ndpr_advrtrs, pfr_entry)
				advrtrs++;
		}

		advance = sizeof(*p) + sizeof(*sin6) * advrtrs;
		l += advance;
		if (p)
			p = (struct in6_prefix *)((caddr_t)p + advance);
	}

	if (oldp) {
		*oldlenp = l;	/* (caddr_t)d - (caddr_t)oldp */
		if (l > ol)
			error = ENOMEM;
	} else
		*oldlenp = l;

	splx(s);

	return (error);
}
