/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/
#ifndef _NETINET6_IN6_VAR_H
#define _NETINET6_IN6_VAR_H 1

/*
 * IPv6 interface request and alias request.  Use sockaddr_in6 because
 * it is larger than sockaddr.
 */

struct inet6_ifreq
{
#ifndef IFNAMSIZ
#define IFNAMSIZ        16
#endif /* IFNAMSIZ */
        char    ifr_name[IFNAMSIZ];             /* if name, e.g. "en0" */
        union {
                struct  sockaddr_in6 ifru_addr;
                struct  sockaddr_in6 ifru_dstaddr;
                struct  sockaddr_in6 ifru_broadaddr;
                short   ifru_flags;
                int     ifru_metric;
                caddr_t ifru_data;
	      } ifr_ifru;
#define ifr_addr        ifr_ifru.ifru_addr      /* address */
#define ifr_dstaddr     ifr_ifru.ifru_dstaddr   /* other end of p-to-p link */
#define ifr_broadaddr   ifr_ifru.ifru_broadaddr /* broadcast address */
#define ifr_flags       ifr_ifru.ifru_flags     /* flags */
#define ifr_metric      ifr_ifru.ifru_metric    /* metric */
#define ifr_data        ifr_ifru.ifru_data      /* for use by interface */
      };

/*
 * IPv6 interface "alias" request.  Used to add interface addresses.  This 
 * may be needed to be expanded to pass down/up permanancy information, and
 * possibly deprecation lifetime values.  (That is, if the kernel doesn't
 * compute that stuff itself.)
 */

struct inet6_aliasreq
{
  char ifra_name[IFNAMSIZ];
  struct sockaddr_in6 ifra_addr;
  struct sockaddr_in6 ifra_dstaddr;
#define	ifra_broadaddr ifra_dstaddr
  struct sockaddr_in6 ifra_mask;
};

/* ioctl()'s for stuff with inet6_{aliasreq,ifreq}  (gag!) */

#define SIOCDIFADDR_INET6 _IOW('i',25, struct inet6_ifreq)  /* delete IF addr */
#define SIOCAIFADDR_INET6 _IOW('i',26, struct inet6_aliasreq)/* add/chg IFalias */
#define SIOCGIFADDR_INET6 _IOWR('i',33, struct inet6_ifreq) /* get ifnet address */
#define SIOCGIFDSTADDR_INET6 _IOWR('i',34, struct inet6_ifreq) /* get dst address */
#define SIOCSIFDSTADDR_INET6 _IOW('i', 14, struct inet6_ifreq) /* set dst address */
#define SIOCGIFNETMASK_INET6 _IOWR('i',37, struct inet6_ifreq) /* get netmask */

#define SIOCVIFADDR_INET6 _IOW('i',69,struct inet6_ifreq) /* Verify IPv6 addr */

/*
 * INET6 interface address.  This might also serve as the prefix list,
 * with the help of the I6AF_PREFIX flag.
 */

struct in6_ifaddr
{
  struct ifaddr i6a_ifa; /* protocol-independent info (32 bytes) */
#define i6a_ifp    i6a_ifa.ifa_ifp
#ifdef KERNEL
#define i6a_flags  i6a_ifa.ifa_flags
#endif
  
  /* All sorts of INET6-specific junk, some of it, very similar to IP's
     in_ifaddr. */
  
  /* Put any subnetting, etc here. */
  
  struct in6_ifaddr *i6a_next;
  struct in6_ifnet *i6a_i6ifp;           /* Pointer to IPv6 interface info */
  struct sockaddr_in6 i6a_addr;          /* Address. */
  struct sockaddr_in6 i6a_dstaddr;       /* Dest. if PPP link. */
  struct sockaddr_in6 i6a_sockmask;      /* Netmask.  This is IPv6, so
					    there is no "subnet/net"
					    distinction. */
  
  /*
   * IPv6 addresses have lifetimes.  Put in expiration information in
   * here.  A DEPRECATED address is still valid for inbound, but not for
   * outbound.  An EXPIRED address is invalid for both inbound and outbound,
   * and should be put out of its misery (and our in6_ifaddr list) ASAP.
   */
  u_long i6a_preferred;                 /* Preferred lifetime. */
  u_long i6a_expire;                    /* Expiration time. */
  u_short i6a_preflen;                  /* Prefix length for link-locals
					   (in bits). */
  u_short i6a_addrflags;                /* Additional flags because
					   ifa_flags isn't big enough. */
};

#define I6A_SIN(i6a) (&(((struct in6_ifaddr *)(i6a))->i6a_addr))
#define IS_EXPIRED(i6a) ((i6a->i6a_expire != 0 && \
			  i6a->i6a_expire < time.tv_sec))
#define IS_DEPRECATED(i6a) ((i6a)->i6a_addrflags & I6AF_DEPRECATED)

#define I6AF_LINKLOC 0x1    /* Link-local address.  Saves the IS_IN6_LINKLOC
			       check. */
#define I6AF_PERMANENT 0x2  /* Permanent address */
#define I6AF_PREFIX 0x4     /* I am a, "prefix list entry," meaning that
			       the portion of the address inside the mask
			       is directly attached to the link. */
#define I6AF_NOTSURE 0x8    /* I'm not sure if I'm allowed to be used yet.
			       This is designed for use with addresses
			       that haven't been verified as unique on a
			       link yet. */
#define I6AF_DEPRECATED 0x10  /* The use of this address should be discouraged.
                                 The address should not be used as a source
                                 address for new communications.  The address
                                 is still valid for receiving packets.  */

/*
 * IPv6 multicast structures and macros.
 */

struct in6_multi
{
  struct in6_multi *in6m_next;          /* Ptr. to next one. */
  struct in6_addr in6m_addr;            /* Multicast address. */
  struct ifnet *in6m_ifp;               /* Pointer to interface. */
  struct in6_ifnet *in6m_i6ifp;         /* Back ptr. to IPv6 if info. */
  uint in6m_refcount;                  /* Number of membership claims by
					   sockets. */
  uint in6m_timer;                     /* IGMP membership report timer. */
};

#ifdef KERNEL
/* General case IN6 multicast lookup.  Can be optimized out in certain
   places (like netinet6/ipv6_input.c ?). */

#define IN6_LOOKUP_MULTI(addr,ifp,in6m) \
{\
  register struct in6_ifnet *i6ifp;\
\
  for (i6ifp=in6_ifnet; i6ifp != NULL && i6ifp->i6ifp_ifp != ifp;\
       i6ifp=i6ifp->i6ifp_next)\
   ;\
  if (i6ifp == NULL)\
    in6m=NULL;\
  else\
    for ((in6m) = i6ifp->i6ifp_multiaddrs;\
       (in6m) != NULL && !IN6_ARE_ADDR_EQUAL(&(in6m)->in6m_addr,(addr));\
       (in6m) = (in6m)->in6m_next) ;\
}


#define IN6_MCASTOPTS 0x2

#define ETHER_MAP_IN6_MULTICAST(in6addr,enaddr) { \
   (enaddr)[0] = 0x33; \
   (enaddr)[1] = 0x33; \
   (enaddr)[2] = in6addr.s6_addr[12]; \
   (enaddr)[3] = in6addr.s6_addr[13]; \
   (enaddr)[4] = in6addr.s6_addr[14]; \
   (enaddr)[5] = in6addr.s6_addr[15]; \
   }

struct in6_ifnet
{
  struct in6_ifnet *i6ifp_next;         /* Next in list. */
  struct ifnet *i6ifp_ifp;              /* Back pointer to actual interface. */
  struct in6_multi *i6ifp_multiaddrs;   /* Multicast addresses for this
					   interface. */
  uint i6ifp_numaddrs;                 /* Number of IPv6 addresses on this
					   interface. */

  /* Addrconf and ND variables will go here. */
};

#endif /* KERNEL */

#endif /* _NETINET6_IN6_VAR_H */
