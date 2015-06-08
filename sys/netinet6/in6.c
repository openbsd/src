/*	$OpenBSD: in6.c,v 1.159 2015/06/08 22:19:27 krw Exp $	*/
/*	$KAME: in6.c,v 1.372 2004/06/14 08:14:21 itojun Exp $	*/

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
 *	@(#)in.c	8.2 (Berkeley) 11/15/93
 */

#include "bridge.h"
#include "carp.h"

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/mld6_var.h>
#ifdef MROUTING
#include <netinet6/ip6_mroute.h>
#endif
#include <netinet6/in6_ifattach.h>
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

/* backward compatibility for a while... */
#define COMPAT_IN6IFIOCTL

/*
 * Definitions of some constant IP6 addresses.
 */
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
const struct in6_addr in6addr_intfacelocal_allnodes =
	IN6ADDR_INTFACELOCAL_ALLNODES_INIT;
const struct in6_addr in6addr_linklocal_allnodes =
	IN6ADDR_LINKLOCAL_ALLNODES_INIT;
const struct in6_addr in6addr_linklocal_allrouters =
	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT;

const struct in6_addr in6mask0 = IN6MASK0;
const struct in6_addr in6mask32 = IN6MASK32;
const struct in6_addr in6mask64 = IN6MASK64;
const struct in6_addr in6mask96 = IN6MASK96;
const struct in6_addr in6mask128 = IN6MASK128;

int in6_lifaddr_ioctl(struct socket *, u_long, caddr_t, struct ifnet *);
int in6_ifinit(struct ifnet *, struct in6_ifaddr *, int);
void in6_unlink_ifa(struct in6_ifaddr *, struct ifnet *);

const struct sockaddr_in6 sa6_any = {
	sizeof(sa6_any), AF_INET6, 0, 0, IN6ADDR_ANY_INIT, 0
};

int
in6_mask2len(struct in6_addr *mask, u_char *lim0)
{
	int x = 0, y;
	u_char *lim = lim0, *p;

	/* ignore the scope_id part */
	if (lim0 == NULL || lim0 - (u_char *)mask > sizeof(*mask))
		lim = (u_char *)mask + sizeof(*mask);
	for (p = (u_char *)mask; p < lim; x++, p++) {
		if (*p != 0xff)
			break;
	}
	y = 0;
	if (p < lim) {
		for (y = 0; y < 8; y++) {
			if ((*p & (0x80 >> y)) == 0)
				break;
		}
	}

	/*
	 * when the limit pointer is given, do a stricter check on the
	 * remaining bits.
	 */
	if (p < lim) {
		if (y != 0 && (*p & (0x00ff >> y)) != 0)
			return (-1);
		for (p = p + 1; p < lim; p++)
			if (*p != 0)
				return (-1);
	}

	return x * 8 + y;
}

int
in6_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp)
{
	struct	in6_ifreq *ifr = (struct in6_ifreq *)data;
	struct	in6_ifaddr *ia6 = NULL;
	struct	in6_aliasreq *ifra = (struct in6_aliasreq *)data;
	struct sockaddr_in6 *sa6;
	int s, privileged;

	privileged = 0;
	if ((so->so_state & SS_PRIV) != 0)
		privileged++;

#ifdef MROUTING
	switch (cmd) {
	case SIOCGETSGCNT_IN6:
	case SIOCGETMIFCNT_IN6:
		return (mrt6_ioctl(cmd, data));
	}
#endif

	if (ifp == NULL)
		return (EOPNOTSUPP);

	switch (cmd) {
	case SIOCSNDFLUSH_IN6:
	case SIOCSPFXFLUSH_IN6:
	case SIOCSRTRFLUSH_IN6:
	case SIOCSIFINFO_FLAGS:
		if (!privileged)
			return (EPERM);
		/* FALLTHROUGH */
	case SIOCGIFINFO_IN6:
	case SIOCGNBRINFO_IN6:
		return (nd6_ioctl(cmd, data, ifp));
	}

	switch (cmd) {
	case SIOCSIFPREFIX_IN6:
	case SIOCDIFPREFIX_IN6:
	case SIOCAIFPREFIX_IN6:
	case SIOCCIFPREFIX_IN6:
	case SIOCSGIFPREFIX_IN6:
	case SIOCGIFPREFIX_IN6:
		log(LOG_NOTICE,
		    "prefix ioctls are now invalidated. "
		    "please use ifconfig.\n");
		return (EOPNOTSUPP);
	}

	switch (cmd) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		if (!privileged)
			return (EPERM);
		/* FALLTHROUGH */
	case SIOCGLIFADDR:
		return in6_lifaddr_ioctl(so, cmd, data, ifp);
	}

	/*
	 * Find address for this interface, if it exists.
	 *
	 * In netinet code, we have checked ifra_addr in SIOCSIF*ADDR operation
	 * only, and used the first interface address as the target of other
	 * operations (without checking ifra_addr).  This was because netinet
	 * code/API assumed at most 1 interface address per interface.
	 * Since IPv6 allows a node to assign multiple addresses
	 * on a single interface, we almost always look and check the
	 * presence of ifra_addr, and reject invalid ones here.
	 * It also decreases duplicated code among SIOC*_IN6 operations.
	 */
	switch (cmd) {
	case SIOCAIFADDR_IN6:
	case SIOCSIFPHYADDR_IN6:
		sa6 = &ifra->ifra_addr;
		break;
	case SIOCSIFADDR_IN6:
	case SIOCGIFADDR_IN6:
	case SIOCSIFDSTADDR_IN6:
	case SIOCSIFNETMASK_IN6:
	case SIOCGIFDSTADDR_IN6:
	case SIOCGIFNETMASK_IN6:
	case SIOCDIFADDR_IN6:
	case SIOCGIFPSRCADDR_IN6:
	case SIOCGIFPDSTADDR_IN6:
	case SIOCGIFAFLAG_IN6:
	case SIOCSNDFLUSH_IN6:
	case SIOCSPFXFLUSH_IN6:
	case SIOCSRTRFLUSH_IN6:
	case SIOCGIFALIFETIME_IN6:
	case SIOCSIFALIFETIME_IN6:
	case SIOCGIFSTAT_IN6:
	case SIOCGIFSTAT_ICMP6:
		sa6 = &ifr->ifr_addr;
		break;
	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFBRDADDR:
	case SIOCSIFNETMASK:
		/*
		 * Do not pass those ioctl to driver handler since they are not
		 * properly setup. Instead just error out.
		 */
		return (EOPNOTSUPP);
	default:
		sa6 = NULL;
		break;
	}
	if (sa6 && sa6->sin6_family == AF_INET6) {
		if (IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr)) {
			if (sa6->sin6_addr.s6_addr16[1] == 0) {
				/* link ID is not embedded by the user */
				sa6->sin6_addr.s6_addr16[1] =
				    htons(ifp->if_index);
			} else if (sa6->sin6_addr.s6_addr16[1] !=
			    htons(ifp->if_index)) {
				return (EINVAL);	/* link ID contradicts */
			}
			if (sa6->sin6_scope_id) {
				if (sa6->sin6_scope_id !=
				    (u_int32_t)ifp->if_index)
					return (EINVAL);
				sa6->sin6_scope_id = 0; /* XXX: good way? */
			}
		}
		ia6 = in6ifa_ifpwithaddr(ifp, &sa6->sin6_addr);
	} else
		ia6 = NULL;

	switch (cmd) {
	case SIOCSIFADDR_IN6:
	case SIOCSIFDSTADDR_IN6:
	case SIOCSIFNETMASK_IN6:
		/*
		 * Since IPv6 allows a node to assign multiple addresses
		 * on a single interface, SIOCSIFxxx ioctls are deprecated.
		 */
		return (EINVAL);

	case SIOCDIFADDR_IN6:
		/*
		 * for IPv4, we look for existing in_ifaddr here to allow
		 * "ifconfig if0 delete" to remove the first IPv4 address on
		 * the interface.  For IPv6, as the spec allows multiple
		 * interface address from the day one, we consider "remove the
		 * first one" semantics to be not preferable.
		 */
		if (ia6 == NULL)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCAIFADDR_IN6:
		/*
		 * We always require users to specify a valid IPv6 address for
		 * the corresponding operation.
		 */
		if (ifra->ifra_addr.sin6_family != AF_INET6 ||
		    ifra->ifra_addr.sin6_len != sizeof(struct sockaddr_in6))
			return (EAFNOSUPPORT);
		if (!privileged)
			return (EPERM);

		break;

	case SIOCGIFADDR_IN6:
		/* This interface is basically deprecated. use SIOCGIFCONF. */
		/* FALLTHROUGH */
	case SIOCGIFAFLAG_IN6:
	case SIOCGIFNETMASK_IN6:
	case SIOCGIFDSTADDR_IN6:
	case SIOCGIFALIFETIME_IN6:
		/* must think again about its semantics */
		if (ia6 == NULL)
			return (EADDRNOTAVAIL);
		break;
	case SIOCSIFALIFETIME_IN6:
	    {
		struct in6_addrlifetime *lt;

		if (!privileged)
			return (EPERM);
		if (ia6 == NULL)
			return (EADDRNOTAVAIL);
		/* sanity for overflow - beware unsigned */
		lt = &ifr->ifr_ifru.ifru_lifetime;
		if (lt->ia6t_vltime != ND6_INFINITE_LIFETIME
		 && lt->ia6t_vltime + time_second < time_second) {
			return EINVAL;
		}
		if (lt->ia6t_pltime != ND6_INFINITE_LIFETIME
		 && lt->ia6t_pltime + time_second < time_second) {
			return EINVAL;
		}
		break;
	    }
	}

	switch (cmd) {

	case SIOCGIFADDR_IN6:
		ifr->ifr_addr = ia6->ia_addr;
		break;

	case SIOCGIFDSTADDR_IN6:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		/*
		 * XXX: should we check if ifa_dstaddr is NULL and return
		 * an error?
		 */
		ifr->ifr_dstaddr = ia6->ia_dstaddr;
		break;

	case SIOCGIFNETMASK_IN6:
		ifr->ifr_addr = ia6->ia_prefixmask;
		break;

	case SIOCGIFAFLAG_IN6:
		ifr->ifr_ifru.ifru_flags6 = ia6->ia6_flags;
		break;

	case SIOCGIFSTAT_IN6:
		if (ifp == NULL)
			return EINVAL;
		bzero(&ifr->ifr_ifru.ifru_stat,
		    sizeof(ifr->ifr_ifru.ifru_stat));
		ifr->ifr_ifru.ifru_stat =
		    *((struct in6_ifextra *)ifp->if_afdata[AF_INET6])->in6_ifstat;
		break;

	case SIOCGIFSTAT_ICMP6:
		if (ifp == NULL)
			return EINVAL;
		bzero(&ifr->ifr_ifru.ifru_icmp6stat,
		    sizeof(ifr->ifr_ifru.ifru_icmp6stat));
		ifr->ifr_ifru.ifru_icmp6stat =
		    *((struct in6_ifextra *)ifp->if_afdata[AF_INET6])->icmp6_ifstat;
		break;

	case SIOCGIFALIFETIME_IN6:
		ifr->ifr_ifru.ifru_lifetime = ia6->ia6_lifetime;
		if (ia6->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
			time_t maxexpire;
			struct in6_addrlifetime *retlt =
			    &ifr->ifr_ifru.ifru_lifetime;

			/*
			 * XXX: adjust expiration time assuming time_t is
			 * signed.
			 */
			maxexpire =
			    (time_t)~(1ULL << ((sizeof(maxexpire) * 8) - 1));
			if (ia6->ia6_lifetime.ia6t_vltime <
			    maxexpire - ia6->ia6_updatetime) {
				retlt->ia6t_expire = ia6->ia6_updatetime +
				    ia6->ia6_lifetime.ia6t_vltime;
			} else
				retlt->ia6t_expire = maxexpire;
		}
		if (ia6->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
			time_t maxexpire;
			struct in6_addrlifetime *retlt =
			    &ifr->ifr_ifru.ifru_lifetime;

			/*
			 * XXX: adjust expiration time assuming time_t is
			 * signed.
			 */
			maxexpire =
			    (time_t)~(1ULL << ((sizeof(maxexpire) * 8) - 1));
			if (ia6->ia6_lifetime.ia6t_pltime <
			    maxexpire - ia6->ia6_updatetime) {
				retlt->ia6t_preferred = ia6->ia6_updatetime +
				    ia6->ia6_lifetime.ia6t_pltime;
			} else
				retlt->ia6t_preferred = maxexpire;
		}
		break;

	case SIOCSIFALIFETIME_IN6:
		ia6->ia6_lifetime = ifr->ifr_ifru.ifru_lifetime;
		/* for sanity */
		if (ia6->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
			ia6->ia6_lifetime.ia6t_expire =
				time_second + ia6->ia6_lifetime.ia6t_vltime;
		} else
			ia6->ia6_lifetime.ia6t_expire = 0;
		if (ia6->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
			ia6->ia6_lifetime.ia6t_preferred =
				time_second + ia6->ia6_lifetime.ia6t_pltime;
		} else
			ia6->ia6_lifetime.ia6t_preferred = 0;
		break;

	case SIOCAIFADDR_IN6:
	{
		int i, error = 0;
		struct nd_prefix pr0, *pr;

		/* reject read-only flags */
		if ((ifra->ifra_flags & IN6_IFF_DUPLICATED) != 0 ||
		    (ifra->ifra_flags & IN6_IFF_DETACHED) != 0 ||
		    (ifra->ifra_flags & IN6_IFF_NODAD) != 0 ||
		    (ifra->ifra_flags & IN6_IFF_AUTOCONF) != 0) {
			return (EINVAL);
		}
		/*
		 * first, make or update the interface address structure,
		 * and link it to the list. try to enable inet6 if there
		 * is no link-local yet.
		 */
		s = splsoftnet();
		in6_ifattach(ifp);
		error = in6_update_ifa(ifp, ifra, ia6);
		splx(s);
		if (error != 0)
			return (error);
		if ((ia6 = in6ifa_ifpwithaddr(ifp, &ifra->ifra_addr.sin6_addr))
		    == NULL) {
			/*
			 * this can happen when the user specify the 0 valid
			 * lifetime.
			 */
			break;
		}

		/*
		 * then, make the prefix on-link on the interface.
		 * XXX: we'd rather create the prefix before the address, but
		 * we need at least one address to install the corresponding
		 * interface route, so we configure the address first.
		 */

		/*
		 * convert mask to prefix length (prefixmask has already
		 * been validated in in6_update_ifa().
		 */
		bzero(&pr0, sizeof(pr0));
		pr0.ndpr_ifp = ifp;
		pr0.ndpr_plen = in6_mask2len(&ifra->ifra_prefixmask.sin6_addr,
		    NULL);
		if (pr0.ndpr_plen == 128) {
			dohooks(ifp->if_addrhooks, 0);
			break;	/* we don't need to install a host route. */
		}
		pr0.ndpr_prefix = ifra->ifra_addr;
		pr0.ndpr_mask = ifra->ifra_prefixmask.sin6_addr;
		/* apply the mask for safety. */
		for (i = 0; i < 4; i++) {
			pr0.ndpr_prefix.sin6_addr.s6_addr32[i] &=
			    ifra->ifra_prefixmask.sin6_addr.s6_addr32[i];
		}
		/*
		 * XXX: since we don't have an API to set prefix (not address)
		 * lifetimes, we just use the same lifetimes as addresses.
		 * The (temporarily) installed lifetimes can be overridden by
		 * later advertised RAs (when accept_rtadv is non 0), which is
		 * an intended behavior.
		 */
		pr0.ndpr_raf_onlink = 1; /* should be configurable? */
		pr0.ndpr_raf_auto =
		    ((ifra->ifra_flags & IN6_IFF_AUTOCONF) != 0);
		pr0.ndpr_vltime = ifra->ifra_lifetime.ia6t_vltime;
		pr0.ndpr_pltime = ifra->ifra_lifetime.ia6t_pltime;

		/* add the prefix if not yet. */
		if ((pr = nd6_prefix_lookup(&pr0)) == NULL) {
			/*
			 * nd6_prelist_add will install the corresponding
			 * interface route.
			 */
			if ((error = nd6_prelist_add(&pr0, NULL, &pr)) != 0)
				return (error);
			if (pr == NULL) {
				log(LOG_ERR, "nd6_prelist_add succeeded but "
				    "no prefix\n");
				return (EINVAL); /* XXX panic here? */
			}
		}

		/* relate the address to the prefix */
		if (ia6->ia6_ndpr == NULL) {
			ia6->ia6_ndpr = pr;
			pr->ndpr_refcnt++;
		}

		s = splsoftnet();
		/*
		 * this might affect the status of autoconfigured addresses,
		 * that is, this address might make other addresses detached.
		 */
		pfxlist_onlink_check();

		dohooks(ifp->if_addrhooks, 0);
		splx(s);
		break;
	}

	case SIOCDIFADDR_IN6:
		s = splsoftnet();
		in6_purgeaddr(&ia6->ia_ifa);
		dohooks(ifp->if_addrhooks, 0);
		splx(s);
		break;

	default:
		if (ifp == NULL || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}

	return (0);
}

/*
 * Update parameters of an IPv6 interface address.
 * If necessary, a new entry is created and linked into address chains.
 * This function is separated from in6_control().
 */
int
in6_update_ifa(struct ifnet *ifp, struct in6_aliasreq *ifra,
    struct in6_ifaddr *ia6)
{
	int error = 0, hostIsNew = 0, plen = -1;
	struct sockaddr_in6 dst6;
	struct in6_addrlifetime *lt;
	struct in6_multi_mship *imm;
	struct rtentry *rt;
	char addr[INET6_ADDRSTRLEN];

	splsoftassert(IPL_SOFTNET);

	/* Validate parameters */
	if (ifp == NULL || ifra == NULL) /* this maybe redundant */
		return (EINVAL);

	/*
	 * The destination address for a p2p link must have a family
	 * of AF_UNSPEC or AF_INET6.
	 */
	if ((ifp->if_flags & IFF_POINTOPOINT) != 0 &&
	    ifra->ifra_dstaddr.sin6_family != AF_INET6 &&
	    ifra->ifra_dstaddr.sin6_family != AF_UNSPEC)
		return (EAFNOSUPPORT);

	/*
	 * validate ifra_prefixmask.  don't check sin6_family, netmask
	 * does not carry fields other than sin6_len.
	 */
	if (ifra->ifra_prefixmask.sin6_len > sizeof(struct sockaddr_in6))
		return (EINVAL);
	/*
	 * Because the IPv6 address architecture is classless, we require
	 * users to specify a (non 0) prefix length (mask) for a new address.
	 * We also require the prefix (when specified) mask is valid, and thus
	 * reject a non-consecutive mask.
	 */
	if (ia6 == NULL && ifra->ifra_prefixmask.sin6_len == 0)
		return (EINVAL);
	if (ifra->ifra_prefixmask.sin6_len != 0) {
		plen = in6_mask2len(&ifra->ifra_prefixmask.sin6_addr,
		    (u_char *)&ifra->ifra_prefixmask +
		    ifra->ifra_prefixmask.sin6_len);
		if (plen <= 0)
			return (EINVAL);
	} else {
		/*
		 * In this case, ia6 must not be NULL.  We just use its prefix
		 * length.
		 */
		plen = in6_mask2len(&ia6->ia_prefixmask.sin6_addr, NULL);
	}
	/*
	 * If the destination address on a p2p interface is specified,
	 * and the address is a scoped one, validate/set the scope
	 * zone identifier.
	 */
	dst6 = ifra->ifra_dstaddr;
	if ((ifp->if_flags & (IFF_POINTOPOINT|IFF_LOOPBACK)) != 0 &&
	    (dst6.sin6_family == AF_INET6)) {
		/* link-local index check: should be a separate function? */
		if (IN6_IS_ADDR_LINKLOCAL(&dst6.sin6_addr)) {
			if (dst6.sin6_addr.s6_addr16[1] == 0) {
				/*
				 * interface ID is not embedded by
				 * the user
				 */
				dst6.sin6_addr.s6_addr16[1] =
				    htons(ifp->if_index);
			} else if (dst6.sin6_addr.s6_addr16[1] !=
			    htons(ifp->if_index)) {
				return (EINVAL);	/* ifid contradicts */
			}
		}
	}
	/*
	 * The destination address can be specified only for a p2p or a
	 * loopback interface.  If specified, the corresponding prefix length
	 * must be 128.
	 */
	if (ifra->ifra_dstaddr.sin6_family == AF_INET6) {
		if ((ifp->if_flags & (IFF_POINTOPOINT|IFF_LOOPBACK)) == 0) {
			/* XXX: noisy message */
			nd6log((LOG_INFO, "in6_update_ifa: a destination can "
			    "be specified for a p2p or a loopback IF only\n"));
			return (EINVAL);
		}
		if (plen != 128) {
			nd6log((LOG_INFO, "in6_update_ifa: prefixlen should "
			    "be 128 when dstaddr is specified\n"));
			return (EINVAL);
		}
	}
	/* lifetime consistency check */
	lt = &ifra->ifra_lifetime;
	if (lt->ia6t_pltime > lt->ia6t_vltime)
		return (EINVAL);
	if (lt->ia6t_vltime == 0) {
		/*
		 * the following log might be noisy, but this is a typical
		 * configuration mistake or a tool's bug.
		 */
		nd6log((LOG_INFO,
		    "in6_update_ifa: valid lifetime is 0 for %s\n",
		    inet_ntop(AF_INET6, &ifra->ifra_addr.sin6_addr,
		        addr, sizeof(addr))));

		if (ia6 == NULL)
			return (0); /* there's nothing to do */
	}

	/*
	 * If this is a new address, allocate a new ifaddr and link it
	 * into chains.
	 */
	if (ia6 == NULL) {
		hostIsNew = 1;
		ia6 = malloc(sizeof(*ia6), M_IFADDR, M_WAITOK | M_ZERO);
		LIST_INIT(&ia6->ia6_memberships);
		/* Initialize the address and masks, and put time stamp */
		ia6->ia_ifa.ifa_addr = sin6tosa(&ia6->ia_addr);
		ia6->ia_addr.sin6_family = AF_INET6;
		ia6->ia_addr.sin6_len = sizeof(ia6->ia_addr);
		ia6->ia6_createtime = ia6->ia6_updatetime = time_second;
		if ((ifp->if_flags & (IFF_POINTOPOINT | IFF_LOOPBACK)) != 0) {
			/*
			 * XXX: some functions expect that ifa_dstaddr is not
			 * NULL for p2p interfaces.
			 */
			ia6->ia_ifa.ifa_dstaddr = sin6tosa(&ia6->ia_dstaddr);
		} else {
			ia6->ia_ifa.ifa_dstaddr = NULL;
		}
		ia6->ia_ifa.ifa_netmask = sin6tosa(&ia6->ia_prefixmask);

		ia6->ia_ifp = ifp;
		TAILQ_INSERT_TAIL(&in6_ifaddr, ia6, ia_list);
		ia6->ia_addr = ifra->ifra_addr;
		ifa_add(ifp, &ia6->ia_ifa);
	}

	/* set prefix mask */
	if (ifra->ifra_prefixmask.sin6_len) {
		/*
		 * We prohibit changing the prefix length of an existing
		 * address, because
		 * + such an operation should be rare in IPv6, and
		 * + the operation would confuse prefix management.
		 */
		if (ia6->ia_prefixmask.sin6_len &&
		    in6_mask2len(&ia6->ia_prefixmask.sin6_addr, NULL) != plen) {
			nd6log((LOG_INFO, "in6_update_ifa: the prefix length of an"
			    " existing (%s) address should not be changed\n",
			    inet_ntop(AF_INET6, &ia6->ia_addr.sin6_addr,
				addr, sizeof(addr))));
			error = EINVAL;
			goto unlink;
		}
		ia6->ia_prefixmask = ifra->ifra_prefixmask;
	}

	/*
	 * If a new destination address is specified, scrub the old one and
	 * install the new destination.  Note that the interface must be
	 * p2p or loopback (see the check above.)
	 */
	if ((ifp->if_flags & IFF_POINTOPOINT) && dst6.sin6_family == AF_INET6 &&
	    !IN6_ARE_ADDR_EQUAL(&dst6.sin6_addr, &ia6->ia_dstaddr.sin6_addr)) {
		struct ifaddr *ifa = &ia6->ia_ifa;

		if ((ia6->ia_flags & IFA_ROUTE) != 0 &&
		    rt_ifa_del(ifa, RTF_HOST, ifa->ifa_dstaddr) != 0) {
			nd6log((LOG_ERR, "in6_update_ifa: failed to remove "
			    "a route to the old destination: %s\n",
			    inet_ntop(AF_INET6, &ia6->ia_addr.sin6_addr,
				addr, sizeof(addr))));
			/* proceed anyway... */
		} else
			ia6->ia_flags &= ~IFA_ROUTE;
		ia6->ia_dstaddr = dst6;
	}

	/*
	 * Set lifetimes.  We do not refer to ia6t_expire and ia6t_preferred
	 * to see if the address is deprecated or invalidated, but initialize
	 * these members for applications.
	 */
	ia6->ia6_lifetime = ifra->ifra_lifetime;
	if (ia6->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
		ia6->ia6_lifetime.ia6t_expire =
		    time_second + ia6->ia6_lifetime.ia6t_vltime;
	} else
		ia6->ia6_lifetime.ia6t_expire = 0;
	if (ia6->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
		ia6->ia6_lifetime.ia6t_preferred =
		    time_second + ia6->ia6_lifetime.ia6t_pltime;
	} else
		ia6->ia6_lifetime.ia6t_preferred = 0;

	/* reset the interface and routing table appropriately. */
	if ((error = in6_ifinit(ifp, ia6, hostIsNew)) != 0)
		goto unlink;

	/*
	 * configure address flags.
	 */
	ia6->ia6_flags = ifra->ifra_flags;
	/*
	 * backward compatibility - if IN6_IFF_DEPRECATED is set from the
	 * userland, make it deprecated.
	 */
	if ((ifra->ifra_flags & IN6_IFF_DEPRECATED) != 0) {
		ia6->ia6_lifetime.ia6t_pltime = 0;
		ia6->ia6_lifetime.ia6t_preferred = time_second;
	}
	/*
	 * Make the address tentative before joining multicast addresses,
	 * so that corresponding MLD responses would not have a tentative
	 * source address.
	 */
	ia6->ia6_flags &= ~IN6_IFF_DUPLICATED;	/* safety */
	if (hostIsNew && in6if_do_dad(ifp) &&
	    (ifra->ifra_flags & IN6_IFF_NODAD) == 0)
		ia6->ia6_flags |= IN6_IFF_TENTATIVE;

	/*
	 * We are done if we have simply modified an existing address.
	 */
	if (!hostIsNew)
		return (error);

	/*
	 * Beyond this point, we should call in6_purgeaddr upon an error,
	 * not just go to unlink.
	 */

	/* join necessary multiast groups */
	if ((ifp->if_flags & IFF_MULTICAST) != 0) {
		struct sockaddr_in6 mltaddr, mltmask;

		/* join solicited multicast addr for new host id */
		struct sockaddr_in6 llsol;

		bzero(&llsol, sizeof(llsol));
		llsol.sin6_family = AF_INET6;
		llsol.sin6_len = sizeof(llsol);
		llsol.sin6_addr.s6_addr16[0] = htons(0xff02);
		llsol.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
		llsol.sin6_addr.s6_addr32[1] = 0;
		llsol.sin6_addr.s6_addr32[2] = htonl(1);
		llsol.sin6_addr.s6_addr32[3] =
		    ifra->ifra_addr.sin6_addr.s6_addr32[3];
		llsol.sin6_addr.s6_addr8[12] = 0xff;
		imm = in6_joingroup(ifp, &llsol.sin6_addr, &error);
		if (!imm) {
			nd6log((LOG_ERR, "in6_update_ifa: "
			    "addmulti failed for %s on %s (errno=%d)\n",
			    inet_ntop(AF_INET6, &llsol.sin6_addr,
				addr, sizeof(addr)),
			    ifp->if_xname, error));
			goto cleanup;
		}
		LIST_INSERT_HEAD(&ia6->ia6_memberships, imm, i6mm_chain);

		bzero(&mltmask, sizeof(mltmask));
		mltmask.sin6_len = sizeof(struct sockaddr_in6);
		mltmask.sin6_family = AF_INET6;
		mltmask.sin6_addr = in6mask32;

		/*
		 * join link-local all-nodes address
		 */
		bzero(&mltaddr, sizeof(mltaddr));
		mltaddr.sin6_len = sizeof(struct sockaddr_in6);
		mltaddr.sin6_family = AF_INET6;
		mltaddr.sin6_addr = in6addr_linklocal_allnodes;
		mltaddr.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
		mltaddr.sin6_scope_id = 0;

		/*
		 * XXX: do we really need this automatic routes?
		 * We should probably reconsider this stuff.  Most applications
		 * actually do not need the routes, since they usually specify
		 * the outgoing interface.
		 */
		rt = rtalloc(sin6tosa(&mltaddr), 0, ifp->if_rdomain);
		if (rt) {
			/*
			 * 32bit came from "mltmask"
			 */
			if (memcmp(&mltaddr.sin6_addr,
			    &satosin6(rt_key(rt))->sin6_addr,
			    32 / 8)) {
				rtfree(rt);
				rt = NULL;
			}
		}
		if (!rt) {
			struct rt_addrinfo info;

			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = sin6tosa(&mltaddr);
			info.rti_info[RTAX_GATEWAY] = sin6tosa(&ia6->ia_addr);
			info.rti_info[RTAX_NETMASK] = sin6tosa(&mltmask);
			info.rti_info[RTAX_IFA] = sin6tosa(&ia6->ia_addr);
			/* XXX: we need RTF_CLONING to fake nd6_rtrequest */
			info.rti_flags = RTF_UP | RTF_CLONING;
			error = rtrequest1(RTM_ADD, &info, RTP_CONNECTED, NULL,
			    ifp->if_rdomain);
			if (error)
				goto cleanup;
		} else {
			rtfree(rt);
		}
		imm = in6_joingroup(ifp, &mltaddr.sin6_addr, &error);
		if (!imm) {
			nd6log((LOG_WARNING,
			    "in6_update_ifa: addmulti failed for "
			    "%s on %s (errno=%d)\n",
			    inet_ntop(AF_INET6, &mltaddr.sin6_addr,
				addr, sizeof(addr)),
			    ifp->if_xname, error));
			goto cleanup;
		}
		LIST_INSERT_HEAD(&ia6->ia6_memberships, imm, i6mm_chain);

		/*
		 * join node information group address
		 */
		if (in6_nigroup(ifp, hostname, hostnamelen, &mltaddr) == 0) {
			imm = in6_joingroup(ifp, &mltaddr.sin6_addr, &error);
			if (!imm) {
				nd6log((LOG_WARNING, "in6_update_ifa: "
				    "addmulti failed for %s on %s (errno=%d)\n",
				    inet_ntop(AF_INET6, &mltaddr.sin6_addr,
					addr, sizeof(addr)),
				    ifp->if_xname, error));
				/* XXX not very fatal, go on... */
			} else {
				LIST_INSERT_HEAD(&ia6->ia6_memberships,
				    imm, i6mm_chain);
			}
		}

		/*
		 * join interface-local all-nodes address.
		 * (ff01::1%ifN, and ff01::%ifN/32)
		 */
		bzero(&mltaddr.sin6_addr, sizeof(mltaddr.sin6_addr));
		mltaddr.sin6_len = sizeof(struct sockaddr_in6);
		mltaddr.sin6_family = AF_INET6;
		mltaddr.sin6_addr = in6addr_intfacelocal_allnodes;
		mltaddr.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
		mltaddr.sin6_scope_id = 0;

		/* XXX: again, do we really need the route? */
		rt = rtalloc(sin6tosa(&mltaddr), 0, ifp->if_rdomain);
		if (rt) {
			/* 32bit came from "mltmask" */
			if (memcmp(&mltaddr.sin6_addr,
			    &satosin6(rt_key(rt))->sin6_addr,
			    32 / 8)) {
				rtfree(rt);
				rt = NULL;
			}
		}
		if (!rt) {
			struct rt_addrinfo info;

			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = sin6tosa(&mltaddr);
			info.rti_info[RTAX_GATEWAY] = sin6tosa(&ia6->ia_addr);
			info.rti_info[RTAX_NETMASK] = sin6tosa(&mltmask);
			info.rti_info[RTAX_IFA] = sin6tosa(&ia6->ia_addr);
			info.rti_flags = RTF_UP | RTF_CLONING;
			error = rtrequest1(RTM_ADD, &info, RTP_CONNECTED,
			    NULL, ifp->if_rdomain);
			if (error)
				goto cleanup;
		} else {
			rtfree(rt);
		}
		imm = in6_joingroup(ifp, &mltaddr.sin6_addr, &error);
		if (!imm) {
			nd6log((LOG_WARNING, "in6_update_ifa: "
			    "addmulti failed for %s on %s (errno=%d)\n",
			    inet_ntop(AF_INET6, &mltaddr.sin6_addr,
				addr, sizeof(addr)),
			    ifp->if_xname, error));
			goto cleanup;
		}
		LIST_INSERT_HEAD(&ia6->ia6_memberships, imm, i6mm_chain);
	}

	/*
	 * Perform DAD, if needed.
	 * XXX It may be of use, if we can administratively
	 * disable DAD.
	 */
	if (hostIsNew && in6if_do_dad(ifp) &&
	    (ifra->ifra_flags & IN6_IFF_NODAD) == 0)
	{
		nd6_dad_start(&ia6->ia_ifa, NULL);
	}

	return (error);

  unlink:
	/*
	 * XXX: if a change of an existing address failed, keep the entry
	 * anyway.
	 */
	if (hostIsNew)
		in6_unlink_ifa(ia6, ifp);
	return (error);

  cleanup:
	in6_purgeaddr(&ia6->ia_ifa);
	return error;
}

void
in6_purgeaddr(struct ifaddr *ifa)
{
	struct ifnet *ifp = ifa->ifa_ifp;
	struct in6_ifaddr *ia6 = ifatoia6(ifa);
	struct in6_multi_mship *imm;

	/* stop DAD processing */
	nd6_dad_stop(ifa);

	/*
	 * delete route to the destination of the address being purged.
	 * The interface must be p2p or loopback in this case.
	 */
	if ((ifp->if_flags & IFF_POINTOPOINT) && (ia6->ia_flags & IFA_ROUTE) &&
	    ia6->ia_dstaddr.sin6_len != 0) {
		int e;

		if ((e = rt_ifa_del(ifa, RTF_HOST, ifa->ifa_dstaddr)) != 0) {
			char addr[INET6_ADDRSTRLEN];
			log(LOG_ERR, "in6_purgeaddr: failed to remove "
			    "a route to the p2p destination: %s on %s, "
			    "errno=%d\n",
			    inet_ntop(AF_INET6, &ia6->ia_addr.sin6_addr,
				addr, sizeof(addr)),
			    ifp->if_xname, e);
			/* proceed anyway... */
		} else
			ia6->ia_flags &= ~IFA_ROUTE;
	}

	/* Remove ownaddr's loopback rtentry, if it exists. */
	rt_ifa_dellocal(&(ia6->ia_ifa));

	/*
	 * leave from multicast groups we have joined for the interface
	 */
	while (!LIST_EMPTY(&ia6->ia6_memberships)) {
		imm = LIST_FIRST(&ia6->ia6_memberships);
		LIST_REMOVE(imm, i6mm_chain);
		in6_leavegroup(imm);
	}

	in6_unlink_ifa(ia6, ifp);
}

void
in6_unlink_ifa(struct in6_ifaddr *ia6, struct ifnet *ifp)
{
	splsoftassert(IPL_SOFTNET);

	ifa_del(ifp, &ia6->ia_ifa);

	TAILQ_REMOVE(&in6_ifaddr, ia6, ia_list);

	/* Release the reference to the base prefix. */
	if (ia6->ia6_ndpr == NULL) {
		char addr[INET6_ADDRSTRLEN];

		if (!IN6_IS_ADDR_LINKLOCAL(IA6_IN6(ia6)) &&
		    !IN6_IS_ADDR_LOOPBACK(IA6_IN6(ia6)) &&
		    !IN6_ARE_ADDR_EQUAL(IA6_MASKIN6(ia6), &in6mask128))
			log(LOG_NOTICE, "in6_unlink_ifa: interface address "
			    "%s has no prefix\n",
			    inet_ntop(AF_INET6, IA6_IN6(ia6), addr,
				sizeof(addr)));
	} else {
		ia6->ia6_flags &= ~IN6_IFF_AUTOCONF;
		if (--ia6->ia6_ndpr->ndpr_refcnt == 0)
			prelist_remove(ia6->ia6_ndpr);
		ia6->ia6_ndpr = NULL;
	}

	/*
	 * release another refcnt for the link from in6_ifaddr.
	 * Note that we should decrement the refcnt at least once for all *BSD.
	 */
	ifafree(&ia6->ia_ifa);
}

/*
 * SIOC[GAD]LIFADDR.
 *	SIOCGLIFADDR: get first address. (?)
 *	SIOCGLIFADDR with IFLR_PREFIX:
 *		get first address that matches the specified prefix.
 *	SIOCALIFADDR: add the specified address.
 *	SIOCALIFADDR with IFLR_PREFIX:
 *		add the specified prefix, filling hostid part from
 *		the first link-local address.  prefixlen must be <= 64.
 *	SIOCDLIFADDR: delete the specified address.
 *	SIOCDLIFADDR with IFLR_PREFIX:
 *		delete the first address that matches the specified prefix.
 * return values:
 *	EINVAL on invalid parameters
 *	EADDRNOTAVAIL on prefix match failed/specified address not found
 *	other values may be returned from in6_ioctl()
 *
 * NOTE: SIOCALIFADDR(with IFLR_PREFIX set) allows prefixlen less than 64.
 * this is to accommodate address naming scheme other than RFC2374,
 * in the future.
 * RFC2373 defines interface id to be 64bit, but it allows non-RFC2374
 * address encoding scheme. (see figure on page 8)
 */
int
in6_lifaddr_ioctl(struct socket *so, u_long cmd, caddr_t data,
    struct ifnet *ifp)
{
	struct if_laddrreq *iflr = (struct if_laddrreq *)data;
	struct ifaddr *ifa;
	struct sockaddr *sa;

	/* sanity checks */
	if (!data || !ifp) {
		panic("invalid argument to in6_lifaddr_ioctl");
		/* NOTREACHED */
	}

	switch (cmd) {
	case SIOCGLIFADDR:
		/* address must be specified on GET with IFLR_PREFIX */
		if ((iflr->flags & IFLR_PREFIX) == 0)
			break;
		/* FALLTHROUGH */
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* address must be specified on ADD and DELETE */
		sa = (struct sockaddr *)&iflr->addr;
		if (sa->sa_family != AF_INET6)
			return EINVAL;
		if (sa->sa_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		/* XXX need improvement */
		sa = (struct sockaddr *)&iflr->dstaddr;
		if (sa->sa_family && sa->sa_family != AF_INET6)
			return EINVAL;
		if (sa->sa_len && sa->sa_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		break;
	default: /* shouldn't happen */
#if 0
		panic("invalid cmd to in6_lifaddr_ioctl");
		/* NOTREACHED */
#else
		return EOPNOTSUPP;
#endif
	}
	if (sizeof(struct in6_addr) * 8 < iflr->prefixlen)
		return EINVAL;

	switch (cmd) {
	case SIOCALIFADDR:
	    {
		struct in6_aliasreq ifra;
		struct in6_addr *hostid = NULL;
		int prefixlen;

		if ((iflr->flags & IFLR_PREFIX) != 0) {
			struct sockaddr_in6 *sin6;

			/*
			 * hostid is to fill in the hostid part of the
			 * address.  hostid points to the first link-local
			 * address attached to the interface.
			 */
			ifa = &in6ifa_ifpforlinklocal(ifp, 0)->ia_ifa;
			if (!ifa)
				return EADDRNOTAVAIL;
			hostid = IFA_IN6(ifa);

			/* prefixlen must be <= 64. */
			if (64 < iflr->prefixlen)
				return EINVAL;
			prefixlen = iflr->prefixlen;

			/* hostid part must be zero. */
			sin6 = (struct sockaddr_in6 *)&iflr->addr;
			if (sin6->sin6_addr.s6_addr32[2] != 0
			 || sin6->sin6_addr.s6_addr32[3] != 0) {
				return EINVAL;
			}
		} else
			prefixlen = iflr->prefixlen;

		/* copy args to in6_aliasreq, perform ioctl(SIOCAIFADDR_IN6). */
		bzero(&ifra, sizeof(ifra));
		bcopy(iflr->iflr_name, ifra.ifra_name, sizeof(ifra.ifra_name));

		bcopy(&iflr->addr, &ifra.ifra_addr,
		    ((struct sockaddr *)&iflr->addr)->sa_len);
		if (hostid) {
			/* fill in hostid part */
			ifra.ifra_addr.sin6_addr.s6_addr32[2] =
			    hostid->s6_addr32[2];
			ifra.ifra_addr.sin6_addr.s6_addr32[3] =
			    hostid->s6_addr32[3];
		}

		if (((struct sockaddr *)&iflr->dstaddr)->sa_family) {	/*XXX*/
			bcopy(&iflr->dstaddr, &ifra.ifra_dstaddr,
			    ((struct sockaddr *)&iflr->dstaddr)->sa_len);
			if (hostid) {
				ifra.ifra_dstaddr.sin6_addr.s6_addr32[2] =
				    hostid->s6_addr32[2];
				ifra.ifra_dstaddr.sin6_addr.s6_addr32[3] =
				    hostid->s6_addr32[3];
			}
		}

		ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
		in6_prefixlen2mask(&ifra.ifra_prefixmask.sin6_addr, prefixlen);

		ifra.ifra_flags = iflr->flags & ~IFLR_PREFIX;
		return in6_control(so, SIOCAIFADDR_IN6, (caddr_t)&ifra, ifp);
	    }
	case SIOCGLIFADDR:
	case SIOCDLIFADDR:
	    {
		struct in6_ifaddr *ia6;
		struct in6_addr mask, candidate, match;
		struct sockaddr_in6 *sin6;
		int cmp;

		bzero(&mask, sizeof(mask));
		if (iflr->flags & IFLR_PREFIX) {
			/* lookup a prefix rather than address. */
			in6_prefixlen2mask(&mask, iflr->prefixlen);

			sin6 = (struct sockaddr_in6 *)&iflr->addr;
			bcopy(&sin6->sin6_addr, &match, sizeof(match));
			match.s6_addr32[0] &= mask.s6_addr32[0];
			match.s6_addr32[1] &= mask.s6_addr32[1];
			match.s6_addr32[2] &= mask.s6_addr32[2];
			match.s6_addr32[3] &= mask.s6_addr32[3];

			/* if you set extra bits, that's wrong */
			if (bcmp(&match, &sin6->sin6_addr, sizeof(match)))
				return EINVAL;

			cmp = 1;
		} else {
			if (cmd == SIOCGLIFADDR) {
				/* on getting an address, take the 1st match */
				cmp = 0;	/* XXX */
			} else {
				/* on deleting an address, do exact match */
				in6_prefixlen2mask(&mask, 128);
				sin6 = (struct sockaddr_in6 *)&iflr->addr;
				bcopy(&sin6->sin6_addr, &match, sizeof(match));

				cmp = 1;
			}
		}

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			if (!cmp)
				break;

			bcopy(IFA_IN6(ifa), &candidate, sizeof(candidate));
			candidate.s6_addr32[0] &= mask.s6_addr32[0];
			candidate.s6_addr32[1] &= mask.s6_addr32[1];
			candidate.s6_addr32[2] &= mask.s6_addr32[2];
			candidate.s6_addr32[3] &= mask.s6_addr32[3];
			if (IN6_ARE_ADDR_EQUAL(&candidate, &match))
				break;
		}
		if (!ifa)
			return EADDRNOTAVAIL;
		ia6 = ifatoia6(ifa);

		if (cmd == SIOCGLIFADDR) {
			/* fill in the if_laddrreq structure */
			bcopy(&ia6->ia_addr, &iflr->addr, ia6->ia_addr.sin6_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia6->ia_dstaddr, &iflr->dstaddr,
				    ia6->ia_dstaddr.sin6_len);
			} else
				bzero(&iflr->dstaddr, sizeof(iflr->dstaddr));

			iflr->prefixlen =
			    in6_mask2len(&ia6->ia_prefixmask.sin6_addr, NULL);

			iflr->flags = ia6->ia6_flags;	/*XXX*/

			return 0;
		} else {
			struct in6_aliasreq ifra;

			/* fill in6_aliasreq and do ioctl(SIOCDIFADDR_IN6) */
			bzero(&ifra, sizeof(ifra));
			bcopy(iflr->iflr_name, ifra.ifra_name,
			    sizeof(ifra.ifra_name));

			bcopy(&ia6->ia_addr, &ifra.ifra_addr,
			    ia6->ia_addr.sin6_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia6->ia_dstaddr, &ifra.ifra_dstaddr,
				    ia6->ia_dstaddr.sin6_len);
			} else {
				bzero(&ifra.ifra_dstaddr,
				    sizeof(ifra.ifra_dstaddr));
			}
			bcopy(&ia6->ia_prefixmask, &ifra.ifra_dstaddr,
			    ia6->ia_prefixmask.sin6_len);

			ifra.ifra_flags = ia6->ia6_flags;
			return in6_control(so, SIOCDIFADDR_IN6, (caddr_t)&ifra,
			    ifp);
		}
	    }
	}

	return EOPNOTSUPP;	/* just for safety */
}

/*
 * Initialize an interface's intetnet6 address
 * and routing table entry.
 */
int
in6_ifinit(struct ifnet *ifp, struct in6_ifaddr *ia6, int newhost)
{
	int	error = 0, plen, ifacount = 0;
	struct ifaddr *ifa;

	splsoftassert(IPL_SOFTNET);

	/*
	 * Give the interface a chance to initialize
	 * if this is its first address (or it is a CARP interface)
	 * and to validate the address if necessary.
	 */
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ifacount++;
	}

	if ((ifacount <= 1 || ifp->if_type == IFT_CARP ||
	    (ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT))) &&
	    ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia6))) {
		return (error);
	}

	ia6->ia_ifa.ifa_metric = ifp->if_metric;

	/* we could do in(6)_socktrim here, but just omit it at this moment. */

	/*
	 * Special case:
	 * If the destination address is specified for a point-to-point
	 * interface, install a route to the destination as an interface
	 * direct route.
	 */
	plen = in6_mask2len(&ia6->ia_prefixmask.sin6_addr, NULL); /* XXX */
	if ((ifp->if_flags & IFF_POINTOPOINT) && plen == 128 &&
	    ia6->ia_dstaddr.sin6_family == AF_INET6) {
		ifa = &ia6->ia_ifa;
		error = rt_ifa_add(ifa, RTF_UP | RTF_HOST, ifa->ifa_dstaddr);
		if (error != 0)
			return (error);
		ia6->ia_flags |= IFA_ROUTE;
	}

	/* Add ownaddr as loopback rtentry, if necessary (ex. on p2p link). */
	if (newhost) {
		/* set the rtrequest function to create llinfo */
		if ((ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) == 0)
			ia6->ia_ifa.ifa_rtrequest = nd6_rtrequest;

		rt_ifa_addlocal(&(ia6->ia_ifa));
	}

	return (error);
}

/*
 * Add an address to the list of IP6 multicast addresses for a
 * given interface.
 */
struct in6_multi *
in6_addmulti(struct in6_addr *maddr6, struct ifnet *ifp, int *errorp)
{
	struct	in6_ifreq ifr;
	struct	in6_multi *in6m;
	int	s;

	*errorp = 0;
	/*
	 * See if address already in list.
	 */
	IN6_LOOKUP_MULTI(*maddr6, ifp, in6m);
	if (in6m != NULL) {
		/*
		 * Found it; just increment the refrence count.
		 */
		in6m->in6m_refcnt++;
	} else {
		if (ifp->if_ioctl == NULL) {
			*errorp = ENXIO; /* XXX: appropriate? */
			return (NULL);
		}

		/*
		 * New address; allocate a new multicast record
		 * and link it into the interface's multicast list.
		 */
		in6m = malloc(sizeof(*in6m), M_IPMADDR, M_NOWAIT);
		if (in6m == NULL) {
			*errorp = ENOBUFS;
			return (NULL);
		}

		in6m->in6m_sin.sin6_len = sizeof(struct sockaddr_in6);
		in6m->in6m_sin.sin6_family = AF_INET6;
		in6m->in6m_sin.sin6_addr = *maddr6;
		in6m->in6m_refcnt = 1;
		in6m->in6m_ifidx = ifp->if_index;
		in6m->in6m_ifma.ifma_addr = sin6tosa(&in6m->in6m_sin);

		/*
		 * Ask the network driver to update its multicast reception
		 * filter appropriately for the new address.
		 */
		memcpy(&ifr.ifr_addr, &in6m->in6m_sin, sizeof(in6m->in6m_sin));
		*errorp = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
		if (*errorp) {
			free(in6m, M_IPMADDR, 0);
			return (NULL);
		}

		s = splsoftnet();
		TAILQ_INSERT_HEAD(&ifp->if_maddrlist, &in6m->in6m_ifma,
		    ifma_list);
		splx(s);

		/*
		 * Let MLD6 know that we have joined a new IP6 multicast
		 * group.
		 */
		mld6_start_listening(in6m);
	}

	return (in6m);
}

/*
 * Delete a multicast address record.
 */
void
in6_delmulti(struct in6_multi *in6m)
{
	struct	in6_ifreq ifr;
	struct	ifnet *ifp;
	int	s;

	if (--in6m->in6m_refcnt == 0) {
		/*
		 * No remaining claims to this record; let MLD6 know
		 * that we are leaving the multicast group.
		 */
		mld6_stop_listening(in6m);
		ifp = if_get(in6m->in6m_ifidx);

		/*
		 * Notify the network driver to update its multicast
		 * reception filter.
		 */
		if (ifp != NULL) {
			bzero(&ifr.ifr_addr, sizeof(struct sockaddr_in6));
			ifr.ifr_addr.sin6_len = sizeof(struct sockaddr_in6);
			ifr.ifr_addr.sin6_family = AF_INET6;
			ifr.ifr_addr.sin6_addr = in6m->in6m_addr;
			(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);

			s = splsoftnet();
			TAILQ_REMOVE(&ifp->if_maddrlist, &in6m->in6m_ifma,
			    ifma_list);
			splx(s);
		}

		free(in6m, M_IPMADDR, 0);
	}
}

struct in6_multi_mship *
in6_joingroup(struct ifnet *ifp, struct in6_addr *addr, int *errorp)
{
	struct in6_multi_mship *imm;

	imm = malloc(sizeof(*imm), M_IPMADDR, M_NOWAIT);
	if (!imm) {
		*errorp = ENOBUFS;
		return NULL;
	}
	imm->i6mm_maddr = in6_addmulti(addr, ifp, errorp);
	if (!imm->i6mm_maddr) {
		/* *errorp is alrady set */
		free(imm, M_IPMADDR, 0);
		return NULL;
	}
	return imm;
}

int
in6_leavegroup(struct in6_multi_mship *imm)
{

	if (imm->i6mm_maddr)
		in6_delmulti(imm->i6mm_maddr);
	free(imm,  M_IPMADDR, 0);
	return 0;
}

/*
 * Find an IPv6 interface link-local address specific to an interface.
 */
struct in6_ifaddr *
in6ifa_ifpforlinklocal(struct ifnet *ifp, int ignoreflags)
{
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (IN6_IS_ADDR_LINKLOCAL(IFA_IN6(ifa))) {
			if ((ifatoia6(ifa)->ia6_flags & ignoreflags) != 0)
				continue;
			break;
		}
	}

	return (ifatoia6(ifa));
}


/*
 * find the internet address corresponding to a given interface and address.
 */
struct in6_ifaddr *
in6ifa_ifpwithaddr(struct ifnet *ifp, struct in6_addr *addr)
{
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (IN6_ARE_ADDR_EQUAL(addr, IFA_IN6(ifa)))
			break;
	}

	return (ifatoia6(ifa));
}

/*
 * Check whether an interface has a prefix by looking up the cloning route.
 */
int
in6_ifpprefix(const struct ifnet *ifp, const struct in6_addr *addr)
{
	struct sockaddr_in6 dst;
	struct rtentry *rt;
	u_int tableid = ifp->if_rdomain;

	bzero(&dst, sizeof(dst));
	dst.sin6_len = sizeof(struct sockaddr_in6);
	dst.sin6_family = AF_INET6;
	dst.sin6_addr = *addr;
	rt = rtalloc(sin6tosa(&dst), 0, tableid);

	if (rt == NULL)
		return (0);
	if ((rt->rt_flags & (RTF_CLONING | RTF_CLONED)) == 0 ||
	    (rt->rt_ifp != ifp &&
#if NBRIDGE > 0
	    !SAME_BRIDGE(rt->rt_ifp->if_bridgeport, ifp->if_bridgeport) &&
#endif
#if NCARP > 0
	    (ifp->if_type != IFT_CARP || rt->rt_ifp != ifp->if_carpdev) &&
	    (rt->rt_ifp->if_type != IFT_CARP || rt->rt_ifp->if_carpdev != ifp)&&
	    (ifp->if_type != IFT_CARP || rt->rt_ifp->if_type != IFT_CARP ||
	    rt->rt_ifp->if_carpdev != ifp->if_carpdev) &&
#endif
	    1)) {
		rtfree(rt);
		return (0);
	}

	rtfree(rt);
	return (1);
}

/*
 * Get a scope of the address. Node-local, link-local, site-local or global.
 */
int
in6_addrscope(struct in6_addr *addr)
{
	int scope;

	if (addr->s6_addr8[0] == 0xfe) {
		scope = addr->s6_addr8[1] & 0xc0;

		switch (scope) {
		case 0x80:
			return __IPV6_ADDR_SCOPE_LINKLOCAL;
			break;
		case 0xc0:
			return __IPV6_ADDR_SCOPE_SITELOCAL;
			break;
		default:
			return __IPV6_ADDR_SCOPE_GLOBAL; /* just in case */
			break;
		}
	}


	if (addr->s6_addr8[0] == 0xff) {
		scope = addr->s6_addr8[1] & 0x0f;

		/*
		 * due to other scope such as reserved,
		 * return scope doesn't work.
		 */
		switch (scope) {
		case __IPV6_ADDR_SCOPE_INTFACELOCAL:
			return __IPV6_ADDR_SCOPE_INTFACELOCAL;
			break;
		case __IPV6_ADDR_SCOPE_LINKLOCAL:
			return __IPV6_ADDR_SCOPE_LINKLOCAL;
			break;
		case __IPV6_ADDR_SCOPE_SITELOCAL:
			return __IPV6_ADDR_SCOPE_SITELOCAL;
			break;
		default:
			return __IPV6_ADDR_SCOPE_GLOBAL;
			break;
		}
	}

	if (bcmp(&in6addr_loopback, addr, sizeof(*addr) - 1) == 0) {
		if (addr->s6_addr8[15] == 1) /* loopback */
			return __IPV6_ADDR_SCOPE_INTFACELOCAL;
		if (addr->s6_addr8[15] == 0) /* unspecified */
			return __IPV6_ADDR_SCOPE_LINKLOCAL;
	}

	return __IPV6_ADDR_SCOPE_GLOBAL;
}

/*
 * ifp - must not be NULL
 * addr - must not be NULL
 */

int
in6_addr2scopeid(struct ifnet *ifp, struct in6_addr *addr)
{
	int scope = in6_addrscope(addr);

	switch(scope) {
	case __IPV6_ADDR_SCOPE_INTFACELOCAL:
	case __IPV6_ADDR_SCOPE_LINKLOCAL:
		/* XXX: we do not distinguish between a link and an I/F. */
		return (ifp->if_index);

	case __IPV6_ADDR_SCOPE_SITELOCAL:
		return (0);	/* XXX: invalid. */

	default:
		return (0);	/* XXX: treat as global. */
	}
}

/*
 * return length of part which dst and src are equal
 * hard coding...
 */
int
in6_matchlen(struct in6_addr *src, struct in6_addr *dst)
{
	int match = 0;
	u_char *s = (u_char *)src, *d = (u_char *)dst;
	u_char *lim = s + 16, r;

	while (s < lim)
		if ((r = (*d++ ^ *s++)) != 0) {
			while (r < 128) {
				match++;
				r <<= 1;
			}
			break;
		} else
			match += 8;
	return match;
}

int
in6_are_prefix_equal(struct in6_addr *p1, struct in6_addr *p2, int len)
{
	int bytelen, bitlen;

	/* sanity check */
	if (0 > len || len > 128) {
		log(LOG_ERR, "in6_are_prefix_equal: invalid prefix length(%d)\n",
		    len);
		return (0);
	}

	bytelen = len / 8;
	bitlen = len % 8;

	if (bcmp(&p1->s6_addr, &p2->s6_addr, bytelen))
		return (0);
	/* len == 128 is ok because bitlen == 0 then */
	if (bitlen != 0 &&
	    p1->s6_addr[bytelen] >> (8 - bitlen) !=
	    p2->s6_addr[bytelen] >> (8 - bitlen))
		return (0);

	return (1);
}

void
in6_prefixlen2mask(struct in6_addr *maskp, int len)
{
	u_char maskarray[8] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
	int bytelen, bitlen, i;

	/* sanity check */
	if (0 > len || len > 128) {
		log(LOG_ERR, "in6_prefixlen2mask: invalid prefix length(%d)\n",
		    len);
		return;
	}

	bzero(maskp, sizeof(*maskp));
	bytelen = len / 8;
	bitlen = len % 8;
	for (i = 0; i < bytelen; i++)
		maskp->s6_addr[i] = 0xff;
	/* len == 128 is ok because bitlen == 0 then */
	if (bitlen)
		maskp->s6_addr[bytelen] = maskarray[bitlen - 1];
}

/*
 * return the best address out of the same scope
 */
struct in6_ifaddr *
in6_ifawithscope(struct ifnet *oifp, struct in6_addr *dst, u_int rdomain)
{
	int dst_scope =	in6_addrscope(dst), src_scope, best_scope = 0;
	int blen = -1;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	struct in6_ifaddr *ia6_best = NULL;
#if NCARP > 0
	struct sockaddr_dl *proxydl = NULL;
#endif

	if (oifp == NULL) {
		printf("in6_ifawithscope: output interface is not specified\n");
		return (NULL);
	}

	/*
	 * We search for all addresses on all interfaces from the beginning.
	 * Comparing an interface with the outgoing interface will be done
	 * only at the final stage of tiebreaking.
	 */
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rdomain)
			continue;
#if NCARP > 0
		/*
		 * Never use a carp address of an interface which is not
		 * the master.
		 */
		if (ifp->if_type == IFT_CARP &&
		    !carp_iamatch6(ifp, NULL, &proxydl))
			continue;
#endif

		/*
		 * We can never take an address that breaks the scope zone
		 * of the destination.
		 */
		if (in6_addr2scopeid(ifp, dst) != in6_addr2scopeid(oifp, dst))
			continue;

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			int tlen = -1, dscopecmp, bscopecmp, matchcmp;

			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;

			src_scope = in6_addrscope(IFA_IN6(ifa));

#ifdef ADDRSELECT_DEBUG		/* should be removed after stabilization */
		{
			char adst[INET6_ADDRSTRLEN], asrc[INET6_ADDRSTRLEN];
			char bestaddr[INET6_ADDRSTRLEN];


			dscopecmp = IN6_ARE_SCOPE_CMP(src_scope, dst_scope);
			printf("in6_ifawithscope: dst=%s bestaddr=%s, "
			       "newaddr=%s, scope=%x, dcmp=%d, bcmp=%d, "
			       "matchlen=%d, flgs=%x\n",
			       inet_ntop(AF_INET6, dst, adst, sizeof(adst)),
			       (ia6_best == NULL) ? "none" :
			       inet_ntop(AF_INET6, &ia6_best->ia_addr.sin6_addr,
			           bestaddr, sizeof(bestaddr)),
			       inet_ntop(AF_INET6, IFA_IN6(ifa),
			           asrc, sizeof(asrc)),
			       src_scope, dscopecmp,
			       ia6_best ? IN6_ARE_SCOPE_CMP(src_scope, best_scope) : -1,
			       in6_matchlen(IFA_IN6(ifa), dst),
			       ifatoia6(ifa)->ia6_flags);
		}
#endif

			/*
			 * Don't use an address before completing DAD
			 * nor a duplicated address.
			 */
			if (ifatoia6(ifa)->ia6_flags & IN6_IFF_NOTREADY)
				continue;

			/* XXX: is there any case to allow anycasts? */
			if (ifatoia6(ifa)->ia6_flags & IN6_IFF_ANYCAST)
				continue;

			if (ifatoia6(ifa)->ia6_flags & IN6_IFF_DETACHED)
				continue;

			/*
			 * If this is the first address we find,
			 * keep it anyway.
			 */
			if (ia6_best == NULL)
				goto replace;

			/*
			 * ia6_best is never NULL beyond this line except
			 * within the block labeled "replace".
			 */

			/*
			 * If ia6_best has a smaller scope than dst and
			 * the current address has a larger one than
			 * (or equal to) dst, always replace ia6_best.
			 * Also, if the current address has a smaller scope
			 * than dst, ignore it unless ia6_best also has a
			 * smaller scope.
			 */
			if (IN6_ARE_SCOPE_CMP(best_scope, dst_scope) < 0 &&
			    IN6_ARE_SCOPE_CMP(src_scope, dst_scope) >= 0)
				goto replace;
			if (IN6_ARE_SCOPE_CMP(src_scope, dst_scope) < 0 &&
			    IN6_ARE_SCOPE_CMP(best_scope, dst_scope) >= 0)
				continue;

			/*
			 * A deprecated address SHOULD NOT be used in new
			 * communications if an alternate (non-deprecated)
			 * address is available and has sufficient scope.
			 * RFC 2462, Section 5.5.4.
			 */
			if (ifatoia6(ifa)->ia6_flags & IN6_IFF_DEPRECATED) {
				/*
				 * Ignore any deprecated addresses if
				 * specified by configuration.
				 */
				if (!ip6_use_deprecated)
					continue;

				/*
				 * If we have already found a non-deprecated
				 * candidate, just ignore deprecated addresses.
				 */
				if ((ia6_best->ia6_flags & IN6_IFF_DEPRECATED)
				    == 0)
					continue;
			}

			/*
			 * A non-deprecated address is always preferred
			 * to a deprecated one regardless of scopes and
			 * address matching.
			 */
			if ((ia6_best->ia6_flags & IN6_IFF_DEPRECATED) &&
			    (ifatoia6(ifa)->ia6_flags &
			     IN6_IFF_DEPRECATED) == 0)
				goto replace;

			/*
			 * At this point, we have two cases:
			 * 1. we are looking at a non-deprecated address,
			 *    and ia6_best is also non-deprecated.
			 * 2. we are looking at a deprecated address,
			 *    and ia6_best is also deprecated.
			 * Also, we do not have to consider a case where
			 * the scope of if_best is larger(smaller) than dst and
			 * the scope of the current address is smaller(larger)
			 * than dst. Such a case has already been covered.
			 * Tiebreaking is done according to the following
			 * items:
			 * - the scope comparison between the address and
			 *   dst (dscopecmp)
			 * - the scope comparison between the address and
			 *   ia6_best (bscopecmp)
			 * - if the address match dst longer than ia6_best
			 *   (matchcmp)
			 * - if the address is on the outgoing I/F (outI/F)
			 *
			 * Roughly speaking, the selection policy is
			 * - the most important item is scope. The same scope
			 *   is best. Then search for a larger scope.
			 *   Smaller scopes are the last resort.
			 * - A deprecated address is chosen only when we have
			 *   no address that has an enough scope, but is
			 *   prefered to any addresses of smaller scopes.
			 * - Longest address match against dst is considered
			 *   only for addresses that has the same scope of dst.
			 * - If there is no other reasons to choose one,
			 *   addresses on the outgoing I/F are preferred.
			 *
			 * The precise decision table is as follows:
			 * dscopecmp bscopecmp matchcmp outI/F | replace?
			 *    !equal     equal      N/A    Yes |      Yes (1)
			 *    !equal     equal      N/A     No |       No (2)
			 *    larger    larger      N/A    N/A |       No (3)
			 *    larger   smaller      N/A    N/A |      Yes (4)
			 *   smaller    larger      N/A    N/A |      Yes (5)
			 *   smaller   smaller      N/A    N/A |       No (6)
			 *     equal   smaller      N/A    N/A |      Yes (7)
			 *     equal    larger       (already done)
			 *     equal     equal   larger    N/A |      Yes (8)
			 *     equal     equal  smaller    N/A |       No (9)
			 *     equal     equal    equal    Yes |      Yes (a)
			 *     equal     equal    equal     No |       No (b)
			 */
			dscopecmp = IN6_ARE_SCOPE_CMP(src_scope, dst_scope);
			bscopecmp = IN6_ARE_SCOPE_CMP(src_scope, best_scope);

			if (dscopecmp && bscopecmp == 0) {
				if (oifp == ifp) /* (1) */
					goto replace;
				continue; /* (2) */
			}
			if (dscopecmp > 0) {
				if (bscopecmp > 0) /* (3) */
					continue;
				goto replace; /* (4) */
			}
			if (dscopecmp < 0) {
				if (bscopecmp > 0) /* (5) */
					goto replace;
				continue; /* (6) */
			}

			/* now dscopecmp must be 0 */
			if (bscopecmp < 0)
				goto replace; /* (7) */

			/*
			 * At last both dscopecmp and bscopecmp must be 0.
			 * We need address matching against dst for
			 * tiebreaking.
			 * Privacy addresses are preferred over public
			 * addresses (RFC3484 requires a config knob for
			 * this which we don't provide).
			 */
			if (oifp == ifp) {
				/* Do not replace temporary autoconf addresses
				 * with non-temporary addresses. */
				if ((ia6_best->ia6_flags & IN6_IFF_PRIVACY) &&
			            !(ifatoia6(ifa)->ia6_flags &
				    IN6_IFF_PRIVACY))
					continue;

				/* Replace non-temporary autoconf addresses
				 * with temporary addresses. */
				if (!(ia6_best->ia6_flags & IN6_IFF_PRIVACY) &&
			            (ifatoia6(ifa)->ia6_flags &
				    IN6_IFF_PRIVACY))
					goto replace;
			}
			tlen = in6_matchlen(IFA_IN6(ifa), dst);
			matchcmp = tlen - blen;
			if (matchcmp > 0) { /* (8) */
#if NCARP > 0
				/* 
				 * Don't let carp interfaces win a tie against
				 * the output interface based on matchlen.
				 * We should only use a carp address if no
				 * other interface has a usable address.
				 * Otherwise, when communicating from a carp
				 * master to a carp slave, the slave won't
				 * respond since the carp address is also
				 * configured as a local address on the slave.
				 * Note that carp interfaces in backup state
				 * were already skipped above.
				 */
				if (ifp->if_type == IFT_CARP &&
				    oifp->if_type != IFT_CARP)
					continue;
#endif
				goto replace;
			}
			if (matchcmp < 0) /* (9) */
				continue;
			if (oifp == ifp) /* (a) */
				goto replace;
			continue; /* (b) */

		  replace:
			ia6_best = ifatoia6(ifa);
			blen = tlen >= 0 ? tlen :
				in6_matchlen(IFA_IN6(ifa), dst);
			best_scope = in6_addrscope(&ia6_best->ia_addr.sin6_addr);
		}
	}

	/* count statistics for future improvements */
	if (ia6_best == NULL)
		ip6stat.ip6s_sources_none++;
	else {
		if (oifp == ia6_best->ia_ifp)
			ip6stat.ip6s_sources_sameif[best_scope]++;
		else
			ip6stat.ip6s_sources_otherif[best_scope]++;

		if (best_scope == dst_scope)
			ip6stat.ip6s_sources_samescope[best_scope]++;
		else
			ip6stat.ip6s_sources_otherscope[best_scope]++;

		if ((ia6_best->ia6_flags & IN6_IFF_DEPRECATED) != 0)
			ip6stat.ip6s_sources_deprecated[best_scope]++;
	}

	return (ia6_best);
}

int
in6if_do_dad(struct ifnet *ifp)
{
	if ((ifp->if_flags & IFF_LOOPBACK) != 0)
		return (0);

	switch (ifp->if_type) {
#if NCARP > 0
	case IFT_CARP:
		/*
		 * XXX: DAD does not work currently on carp(4)
		 * so disable it for now.
		 */
		return (0);
#endif
	default:
		/*
		 * Our DAD routine requires the interface up and running.
		 * However, some interfaces can be up before the RUNNING
		 * status.  Additionaly, users may try to assign addresses
		 * before the interface becomes up (or running).
		 * We simply skip DAD in such a case as a work around.
		 * XXX: we should rather mark "tentative" on such addresses,
		 * and do DAD after the interface becomes ready.
		 */
		if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			return (0);

		return (1);
	}
}

void *
in6_domifattach(struct ifnet *ifp)
{
	struct in6_ifextra *ext;

	ext = malloc(sizeof(*ext), M_IFADDR, M_WAITOK | M_ZERO);

	ext->in6_ifstat = malloc(sizeof(*ext->in6_ifstat), M_IFADDR,
	    M_WAITOK | M_ZERO);

	ext->icmp6_ifstat = malloc(sizeof(*ext->icmp6_ifstat), M_IFADDR,
	    M_WAITOK | M_ZERO);

	ext->nd_ifinfo = nd6_ifattach(ifp);
	ext->nprefixes = 0;
	ext->ndefrouters = 0;
	return ext;
}

void
in6_domifdetach(struct ifnet *ifp, void *aux)
{
	struct in6_ifextra *ext = (struct in6_ifextra *)aux;

	nd6_ifdetach(ext->nd_ifinfo);
	free(ext->in6_ifstat, M_IFADDR, 0);
	free(ext->icmp6_ifstat, M_IFADDR, 0);
	free(ext, M_IFADDR, 0);
}
