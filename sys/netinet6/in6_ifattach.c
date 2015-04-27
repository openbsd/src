/*	$OpenBSD: in6_ifattach.c,v 1.87 2015/04/27 14:51:44 mpi Exp $	*/
/*	$KAME: in6_ifattach.c,v 1.124 2001/07/18 08:32:51 jinmei Exp $	*/

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
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <crypto/sha2.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>
#ifdef MROUTING
#include <netinet6/ip6_mroute.h>
#endif

int get_last_resort_ifid(struct ifnet *, struct in6_addr *);
int get_hw_ifid(struct ifnet *, struct in6_addr *);
int get_ifid(struct ifnet *, struct in6_addr *);
int in6_ifattach_loopback(struct ifnet *);

#define EUI64_GBIT	0x01
#define EUI64_UBIT	0x02
#define EUI64_TO_IFID(in6)	do {(in6)->s6_addr[8] ^= EUI64_UBIT; } while (0)
#define EUI64_GROUP(in6)	((in6)->s6_addr[8] & EUI64_GBIT)
#define EUI64_INDIVIDUAL(in6)	(!EUI64_GROUP(in6))
#define EUI64_LOCAL(in6)	((in6)->s6_addr[8] & EUI64_UBIT)
#define EUI64_UNIVERSAL(in6)	(!EUI64_LOCAL(in6))

#define IFID_LOCAL(in6)		(!EUI64_LOCAL(in6))
#define IFID_UNIVERSAL(in6)	(!EUI64_UNIVERSAL(in6))

/*
 * Generate a last-resort interface identifier, when the machine has no
 * IEEE802/EUI64 address sources.
 * The goal here is to get an interface identifier that is
 * (1) random enough and (2) does not change across reboot.
 * We currently use SHA512(hostname) for it.
 *
 * in6 - upper 64bits are preserved
 */
int
get_last_resort_ifid(struct ifnet *ifp, struct in6_addr *in6)
{
	SHA2_CTX ctx;
	u_int8_t digest[SHA512_DIGEST_LENGTH];

#if 0
	/* we need at least several letters as seed for ifid */
	if (hostnamelen < 3)
		return -1;
#endif

	/* generate 8 bytes of pseudo-random value. */
	SHA512Init(&ctx);
	SHA512Update(&ctx, hostname, hostnamelen);
	SHA512Final(digest, &ctx);

	/* assumes sizeof(digest) > sizeof(ifid) */
	bcopy(digest, &in6->s6_addr[8], 8);

	/* make sure to set "u" bit to local, and "g" bit to individual. */
	in6->s6_addr[8] &= ~EUI64_GBIT;	/* g bit to "individual" */
	in6->s6_addr[8] |= EUI64_UBIT;	/* u bit to "local" */

	/* convert EUI64 into IPv6 interface identifier */
	EUI64_TO_IFID(in6);

	return 0;
}

/*
 * Generate a random interface identifier.
 *
 * in6 - upper 64bits are preserved
 */
void
in6_get_rand_ifid(struct ifnet *ifp, struct in6_addr *in6)
{
	arc4random_buf(&in6->s6_addr32[2], 8);

	/* make sure to set "u" bit to local, and "g" bit to individual. */
	in6->s6_addr[8] &= ~EUI64_GBIT;	/* g bit to "individual" */
	in6->s6_addr[8] |= EUI64_UBIT;	/* u bit to "local" */

	/* convert EUI64 into IPv6 interface identifier */
	EUI64_TO_IFID(in6);
}

/*
 * Get interface identifier for the specified interface.
 *
 * in6 - upper 64bits are preserved
 */
int
get_hw_ifid(struct ifnet *ifp, struct in6_addr *in6)
{
	struct sockaddr_dl *sdl;
	char *addr;
	size_t addrlen;
	static u_int8_t allzero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	static u_int8_t allone[8] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	sdl = (struct sockaddr_dl *)ifp->if_sadl;
	if (sdl == NULL || sdl->sdl_alen == 0)
		return -1;

	addr = LLADDR(sdl);
	addrlen = sdl->sdl_alen;

	switch (ifp->if_type) {
	case IFT_IEEE1394:
	case IFT_IEEE80211:
		/* IEEE1394 uses 16byte length address starting with EUI64 */
		if (addrlen > 8)
			addrlen = 8;
		break;
	default:
		break;
	}

	/* get EUI64 */
	switch (ifp->if_type) {
	/* IEEE802/EUI64 cases - what others? */
	case IFT_ETHER:
	case IFT_CARP:
	case IFT_IEEE1394:
	case IFT_IEEE80211:
		/* look at IEEE802/EUI64 only */
		if (addrlen != 8 && addrlen != 6)
			return -1;

		/*
		 * check for invalid MAC address - on bsdi, we see it a lot
		 * since wildboar configures all-zero MAC on pccard before
		 * card insertion.
		 */
		if (bcmp(addr, allzero, addrlen) == 0)
			return -1;
		if (bcmp(addr, allone, addrlen) == 0)
			return -1;

		/* make EUI64 address */
		if (addrlen == 8)
			bcopy(addr, &in6->s6_addr[8], 8);
		else if (addrlen == 6) {
			in6->s6_addr[8] = addr[0];
			in6->s6_addr[9] = addr[1];
			in6->s6_addr[10] = addr[2];
			in6->s6_addr[11] = 0xff;
			in6->s6_addr[12] = 0xfe;
			in6->s6_addr[13] = addr[3];
			in6->s6_addr[14] = addr[4];
			in6->s6_addr[15] = addr[5];
		}
		break;

	case IFT_GIF:
		/*
		 * RFC2893 says: "SHOULD use IPv4 address as ifid source".
		 * however, IPv4 address is not very suitable as unique
		 * identifier source (can be renumbered).
		 * we don't do this.
		 */
		return -1;

	default:
		return -1;
	}

	/* sanity check: g bit must not indicate "group" */
	if (EUI64_GROUP(in6))
		return -1;

	/* convert EUI64 into IPv6 interface identifier */
	EUI64_TO_IFID(in6);

	/*
	 * sanity check: ifid must not be all zero, avoid conflict with
	 * subnet router anycast
	 */
	if ((in6->s6_addr[8] & ~(EUI64_GBIT | EUI64_UBIT)) == 0x00 &&
	    bcmp(&in6->s6_addr[9], allzero, 7) == 0) {
		return -1;
	}

	return 0;
}

/*
 * Get interface identifier for the specified interface.  If it is not
 * available on ifp0, borrow interface identifier from other information
 * sources.
 */
int
get_ifid(struct ifnet *ifp0, struct in6_addr *in6)
{
	struct ifnet *ifp;

	/* first, try to get it from the interface itself */
	if (get_hw_ifid(ifp0, in6) == 0) {
		nd6log((LOG_DEBUG, "%s: got interface identifier from itself\n",
		    ifp0->if_xname));
		goto success;
	}

	/* next, try to get it from some other hardware interface */
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp == ifp0)
			continue;
		if (get_hw_ifid(ifp, in6) != 0)
			continue;

		/*
		 * to borrow ifid from other interface, ifid needs to be
		 * globally unique
		 */
		if (IFID_UNIVERSAL(in6)) {
			nd6log((LOG_DEBUG,
			    "%s: borrow interface identifier from %s\n",
			    ifp0->if_xname, ifp->if_xname));
			goto success;
		}
	}

	/* last resort: get from random number source */
	if (get_last_resort_ifid(ifp, in6) == 0) {
		nd6log((LOG_DEBUG,
		    "%s: interface identifier generated by random number\n",
		    ifp0->if_xname));
		goto success;
	}

	printf("%s: failed to get interface identifier\n", ifp0->if_xname);
	return -1;

success:
	nd6log((LOG_INFO, "%s: ifid: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
	    ifp0->if_xname, in6->s6_addr[8], in6->s6_addr[9], in6->s6_addr[10],
	    in6->s6_addr[11], in6->s6_addr[12], in6->s6_addr[13],
	    in6->s6_addr[14], in6->s6_addr[15]));
	return 0;
}

/*
 * ifid - used as EUI64 if not NULL, overrides other EUI64 sources
 */

int
in6_ifattach_linklocal(struct ifnet *ifp, struct in6_addr *ifid)
{
	struct in6_ifaddr *ia6;
	struct in6_aliasreq ifra;
	struct nd_prefix pr0;
	int i, s, error;

	/*
	 * configure link-local address.
	 */
	bzero(&ifra, sizeof(ifra));

	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, ifp->if_xname, sizeof(ifra.ifra_name));

	ifra.ifra_addr.sin6_family = AF_INET6;
	ifra.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_addr.sin6_addr.s6_addr16[0] = htons(0xfe80);
	ifra.ifra_addr.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	ifra.ifra_addr.sin6_addr.s6_addr32[1] = 0;
	if ((ifp->if_flags & IFF_LOOPBACK) != 0) {
		ifra.ifra_addr.sin6_addr.s6_addr32[2] = 0;
		ifra.ifra_addr.sin6_addr.s6_addr32[3] = htonl(1);
	} else if (ifid) {
		ifra.ifra_addr.sin6_addr = *ifid;
		ifra.ifra_addr.sin6_addr.s6_addr16[0] = htons(0xfe80);
		ifra.ifra_addr.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
		ifra.ifra_addr.sin6_addr.s6_addr32[1] = 0;
		ifra.ifra_addr.sin6_addr.s6_addr[8] &= ~EUI64_GBIT;
		ifra.ifra_addr.sin6_addr.s6_addr[8] |= EUI64_UBIT;
	} else {
		if (get_ifid(ifp, &ifra.ifra_addr.sin6_addr) != 0) {
			nd6log((LOG_ERR,
			    "%s: no ifid available\n", ifp->if_xname));
			return (-1);
		}
	}

	ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_prefixmask.sin6_family = AF_INET6;
	ifra.ifra_prefixmask.sin6_addr = in6mask64;
	/* link-local addresses should NEVER expire. */
	ifra.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	ifra.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

	/*
	 * Do not let in6_update_ifa() do DAD, since we need a random delay
	 * before sending an NS at the first time the interface becomes up.
	 */
	ifra.ifra_flags |= IN6_IFF_NODAD;

	/*
	 * Now call in6_update_ifa() to do a bunch of procedures to configure
	 * a link-local address. In the case of CARP, we may be called after
	 * one has already been configured, so check if it's already there
	 * with in6ifa_ifpforlinklocal() and clobber it if it exists.
	 */
	s = splsoftnet();
	error = in6_update_ifa(ifp, &ifra, in6ifa_ifpforlinklocal(ifp, 0));
	splx(s);

	if (error != 0) {
		/*
		 * XXX: When the interface does not support IPv6, this call
		 * would fail in the SIOCSIFADDR ioctl.  I believe the
		 * notification is rather confusing in this case, so just
		 * suppress it.  (jinmei@kame.net 20010130)
		 */
		if (error != EAFNOSUPPORT)
			nd6log((LOG_NOTICE, "in6_ifattach_linklocal: failed to "
			    "configure a link-local address on %s "
			    "(errno=%d)\n",
			    ifp->if_xname, error));
		return (-1);
	}

	/*
	 * Adjust ia6_flags so that in6_ifattach() will perform DAD.
	 * XXX: Some P2P interfaces seem not to send packets just after
	 * becoming up, so we skip p2p interfaces for safety.
	 */
	ia6 = in6ifa_ifpforlinklocal(ifp, 0); /* ia6 must not be NULL */
#ifdef DIAGNOSTIC
	if (!ia6) {
		panic("ia6 == NULL in in6_ifattach_linklocal");
		/* NOTREACHED */
	}
#endif
	if (in6if_do_dad(ifp) && ((ifp->if_flags & IFF_POINTOPOINT) ||
	    (ifp->if_type == IFT_CARP)) == 0) {
		ia6->ia6_flags &= ~IN6_IFF_NODAD;
		ia6->ia6_flags |= IN6_IFF_TENTATIVE;
	}

	/*
	 * Make the link-local prefix (fe80::/64%link) as on-link.
	 * Since we'd like to manage prefixes separately from addresses,
	 * we make an ND6 prefix structure for the link-local prefix,
	 * and add it to the prefix list as a never-expire prefix.
	 * XXX: this change might affect some existing code base...
	 */
	bzero(&pr0, sizeof(pr0));
	pr0.ndpr_ifp = ifp;
	/* this should be 64 at this moment. */
	pr0.ndpr_plen = in6_mask2len(&ifra.ifra_prefixmask.sin6_addr, NULL);
	pr0.ndpr_mask = ifra.ifra_prefixmask.sin6_addr;
	pr0.ndpr_prefix = ifra.ifra_addr;
	/* apply the mask for safety. (nd6_prelist_add will apply it again) */
	for (i = 0; i < 4; i++) {
		pr0.ndpr_prefix.sin6_addr.s6_addr32[i] &=
		    in6mask64.s6_addr32[i];
	}
	/*
	 * Initialize parameters.  The link-local prefix must always be
	 * on-link, and its lifetimes never expire.
	 */
	pr0.ndpr_raf_onlink = 1;
	pr0.ndpr_raf_auto = 1;	/* probably meaningless */
	pr0.ndpr_vltime = ND6_INFINITE_LIFETIME;
	pr0.ndpr_pltime = ND6_INFINITE_LIFETIME;
	/*
	 * Since there is no other link-local addresses, nd6_prefix_lookup()
	 * probably returns NULL.  However, we cannot always expect the result.
	 * For example, if we first remove the (only) existing link-local
	 * address, and then reconfigure another one, the prefix is still
	 * valid with referring to the old link-local address.
	 */
	if (nd6_prefix_lookup(&pr0) == NULL) {
		if ((error = nd6_prelist_add(&pr0, NULL, NULL)) != 0)
			return (error);
	}

	return 0;
}

int
in6_ifattach_loopback(struct ifnet *ifp)
{
	struct in6_aliasreq ifra;

	KASSERT(ifp->if_flags & IFF_LOOPBACK);

	bzero(&ifra, sizeof(ifra));

	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, ifp->if_xname, sizeof(ifra.ifra_name));

	ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_prefixmask.sin6_family = AF_INET6;
	ifra.ifra_prefixmask.sin6_addr = in6mask128;

	/*
	 * Always initialize ia_dstaddr (= broadcast address) to loopback
	 * address.  Follows IPv4 practice - see in_ifinit().
	 */
	ifra.ifra_dstaddr.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_dstaddr.sin6_family = AF_INET6;
	ifra.ifra_dstaddr.sin6_addr = in6addr_loopback;

	ifra.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_addr.sin6_family = AF_INET6;
	ifra.ifra_addr.sin6_addr = in6addr_loopback;

	/* the loopback  address should NEVER expire. */
	ifra.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	ifra.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

	/* we don't need to perform DAD on loopback interfaces. */
	ifra.ifra_flags |= IN6_IFF_NODAD;

	/*
	 * We are sure that this is a newly assigned address, so we can set
	 * NULL to the 3rd arg.
	 */
	return (in6_update_ifa(ifp, &ifra, NULL));
}

/*
 * compute NI group address, based on the current hostname setting.
 * see draft-ietf-ipngwg-icmp-name-lookup-* (04 and later).
 *
 * when ifp == NULL, the caller is responsible for filling scopeid.
 */
int
in6_nigroup(struct ifnet *ifp, const char *name, int namelen, 
    struct sockaddr_in6 *sa6)
{
	const char *p;
	u_int8_t *q;
	SHA2_CTX ctx;
	u_int8_t digest[SHA512_DIGEST_LENGTH];
	u_int8_t l;
	u_int8_t n[64];	/* a single label must not exceed 63 chars */

	if (!namelen || !name)
		return -1;

	p = name;
	while (p && *p && *p != '.' && p - name < namelen)
		p++;
	if (p - name > sizeof(n) - 1)
		return -1;	/* label too long */
	l = p - name;
	strncpy((char *)n, name, l);
	n[(int)l] = '\0';
	for (q = n; *q; q++) {
		if ('A' <= *q && *q <= 'Z')
			*q = *q - 'A' + 'a';
	}

	/* generate 8 bytes of pseudo-random value. */
	SHA512Init(&ctx);
	SHA512Update(&ctx, &l, sizeof(l));
	SHA512Update(&ctx, n, l);
	SHA512Final(digest, &ctx);

	bzero(sa6, sizeof(*sa6));
	sa6->sin6_family = AF_INET6;
	sa6->sin6_len = sizeof(*sa6);
	sa6->sin6_addr.s6_addr16[0] = htons(0xff02);
	sa6->sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	sa6->sin6_addr.s6_addr8[11] = 2;
	bcopy(digest, &sa6->sin6_addr.s6_addr32[3],
	    sizeof(sa6->sin6_addr.s6_addr32[3]));

	return 0;
}

/*
 * XXX multiple loopback interface needs more care.  for instance,
 * nodelocal address needs to be configured onto only one of them.
 * XXX multiple link-local address case
 */
int
in6_ifattach(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	int dad_delay = 0;		/* delay ticks before DAD output */

	/* some of the interfaces are inherently not IPv6 capable */
	switch (ifp->if_type) {
	case IFT_BRIDGE:
	case IFT_ENC:
	case IFT_PFLOG:
	case IFT_PFSYNC:
		return (0);
	}

	/*
	 * if link mtu is too small, don't try to configure IPv6.
	 * remember there could be some link-layer that has special
	 * fragmentation logic.
	 */
	if (ifp->if_mtu < IPV6_MMTU)
		return (EINVAL);

	if ((ifp->if_flags & IFF_MULTICAST) == 0)
		return (EINVAL);

	/* Assign a link-local address, if there's none. */
	if (in6ifa_ifpforlinklocal(ifp, 0) == NULL) {
		if (in6_ifattach_linklocal(ifp, NULL) != 0) {
			/* failed to assign linklocal address. bark? */
		}
	}

	/* Assign loopback address, if there's none. */
	if (ifp->if_flags & IFF_LOOPBACK) {
		struct in6_addr in6 = in6addr_loopback;
		if (in6ifa_ifpwithaddr(ifp, &in6) != NULL)
			return (0);

		return (in6_ifattach_loopback(ifp));
	}

	/* Perform DAD. */
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (ifatoia6(ifa)->ia6_flags & IN6_IFF_TENTATIVE)
			nd6_dad_start(ifa, &dad_delay);
	}

	if (ifp->if_xflags & IFXF_AUTOCONF6)
		nd6_rs_output_set_timo(ND6_RS_OUTPUT_QUICK_INTERVAL);

	return (0);
}

/*
 * NOTE: in6_ifdetach() does not support loopback if at this moment.
 */
void
in6_ifdetach(struct ifnet *ifp)
{
	struct ifaddr *ifa, *next;
	struct rtentry *rt;
	struct sockaddr_in6 sin6;

#ifdef MROUTING
	/* remove ip6_mrouter stuff */
	ip6_mrouter_detach(ifp);
#endif

	/* nuke any of IPv6 addresses we have */
	TAILQ_FOREACH_SAFE(ifa, &ifp->if_addrlist, ifa_list, next) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		in6_purgeaddr(ifa);
		dohooks(ifp->if_addrhooks, 0);
	}

	/*
	 * Remove neighbor management table.  Must be called after
	 * purging addresses.
	 */
	nd6_purge(ifp);

	/* remove route to interface local allnodes multicast (ff01::1) */
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_intfacelocal_allnodes;
	sin6.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	rt = rtalloc(sin6tosa(&sin6), 0, ifp->if_rdomain);
	if (rt && rt->rt_ifp == ifp) {
		rtdeletemsg(rt, ifp->if_rdomain);
		rtfree(rt);
	}

	/* remove route to link-local allnodes multicast (ff02::1) */
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_linklocal_allnodes;
	sin6.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	rt = rtalloc(sin6tosa(&sin6), 0, ifp->if_rdomain);
	if (rt && rt->rt_ifp == ifp) {
		rtdeletemsg(rt, ifp->if_rdomain);
		rtfree(rt);
	}

	if (ifp->if_xflags & IFXF_AUTOCONF6) {
		nd6_rs_timeout_count--;
		if (nd6_rs_timeout_count == 0)
			timeout_del(&nd6_rs_output_timer);
		if (RS_LHCOOKIE(ifp) != NULL)
			hook_disestablish(ifp->if_linkstatehooks,
			    RS_LHCOOKIE(ifp));
		ifp->if_xflags &= ~IFXF_AUTOCONF6;
	}
}
