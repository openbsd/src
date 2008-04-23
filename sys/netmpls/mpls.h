/*	$OpenBSD: mpls.h,v 1.1 2008/04/23 11:00:35 norby Exp $	*/

/*
 * Copyright (C) 1999, 2000 and 2001 AYAME Project, WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULARPURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, ORCONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#ifndef _NETMPLS_MPLS_H_
#define _NETMPLS_MPLS_H_

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>

/*
 * Structure of a SHIM header.
 */
struct shim_hdr {
	u_int32_t shim_label;	/* 20 bit label, 4 bit exp & BoS, 8 bit TTL */
};

/*
 * By byte-swapping the constants, we avoid ever having to byte-swap IP
 * addresses inside the kernel.  Unfortunately, user-level programs rely
 * on these macros not doing byte-swapping.
 */

#ifdef _KERNEL
#define __MADDR(x)     ((u_int32_t)htonl((u_int32_t)(x)))
#else
#define __MADDR(x)     ((u_int32_t)(x))
#endif

#define MPLS_LABEL_MASK		__MADDR(0xfffff000U)
#define MPLS_LABEL_OFFSET	12
#define MPLS_EXP_MASK		__MADDR(0x00000e00U)
#define MPLS_EXP_OFFSET		9
#define MPLS_BOS_MASK		__MADDR(0x00000100U)
#define MPLS_BOS_OFFSET		8
#define MPLS_TTL_MASK		__MADDR(0x000000ffU)

#define MPLS_BOS_ISSET(l)	(((l) & MPLS_BOS_MASK) == MPLS_BOS_MASK)

/* Reserved lavel values (RFC3032) */
#define MPLS_LABEL_IPV4NULL	0               /* IPv4 Explicit NULL Label */
#define MPLS_LABEL_RTALERT	1               /* Router Alert Label       */
#define MPLS_LABEL_IPV6NULL	2               /* IPv6 Explicit NULL Label */
#define MPLS_LABEL_IMPLNULL	3               /* Implicit NULL Label      */
/*      MPLS_LABEL_RESERVED	4-15 */		/* Values 4-15 are reserved */
#define MPLS_LABEL_RESERVED_MAX 15

/*
 * Socket address
 */

struct sockaddr_mpls {
	u_int8_t	smpls_len;		/* length */
	u_int8_t	smpls_family;		/* AF_MPLS */
	u_int8_t	smpls_operation;
	u_int8_t	smpls_out_exp;		/* outgoing exp value */
	u_int32_t	smpls_out_label;	/* outgoing MPLS label */
	u_int16_t	smpls_out_ifindex;
	u_int16_t	smpls_in_ifindex;
	u_int32_t	smpls_in_label;		/* MPLS label 20 bits*/
#if MPLS_MCAST
	u_int8_t smpls_mcexp;
	u_int8_t smpls_pad2[2];
	u_int32_t smpls_mclabel;
#endif
};

#define MPLS_OP_POP		1
#define MPLS_OP_PUSH		2
#define MPLS_OP_SWAP		3

#define MPLS_INKERNEL_LOOP_MAX	16

#define satosmpls(sa)		((struct sockaddr_mpls *)(sa))
#define smplstosa(smpls)	((struct sockaddr *)(smpls))
#define satosdl(sa)		((struct sockaddr_dl *)(sa))
#define sdltosa(sdl)		((struct sockaddr *)(sdl))

/*
 * Names for MPLS sysctl objects
 */
#define MPLSCTL_ENABLE			1
#define	MPLSCTL_DEFTTL			2
#define MPLSCTL_IFQUEUE			3
#define	MPLSCTL_MAXINKLOOP		4
#define MPLSCTL_MAXID			5

#define MPLSCTL_NAMES { \
	{ 0, 0 }, \
	{ "enable", CTLTYPE_INT }, \
	{ "ttl", CTLTYPE_INT }, \
	{ "ifq", CTLTYPE_NODE },\
	{ "maxloop_inkernel", CTLTYPE_INT }, \
}

#define MPLSCTL_VARS { \
	0, \
	&mpls_enable, \
	&mpls_defttl, \
	0, \
	&mpls_inkloop, \
}

#endif
/*
 * Copyright (C) 1999, 2000 and 2001 AYAME Project, WIDE Project.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULARPURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, ORCONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

/*
 *
 *	$Id: mpls.h,v 1.1 2008/04/23 11:00:35 norby Exp $
 */

#ifndef _NETMPLS_MPLS_H_
#define _NETMPLS_MPLS_H_

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>

/*
 * Structure of a SHIM header.
 */
struct shim_hdr {
	u_int32_t shim_label;	/* 20bit label, 4bit exp & BOS, 8bit ttl */
};

/*
 * By byte-swapping the constants, we avoid ever having to byte-swap IP
 * addresses inside the kernel.  Unfortunately, user-level programs rely
 * on these macros not doing byte-swapping.
 */
#define SHIM_LABEL_MASK		0xfffff000U
#define SHIM_LABEL_OFFSET	12
#define SHIM_EXP_MASK		0x00000e00U
#define SHIM_EXP_OFFSET		9
#define SHIM_BOS_MASK		0x00000100U
#define SHIM_BOS_OFFSET		8
#define SHIM_TTL_MASK		0x000000ffU

#define MPLS_SHIM_LABEL_GET(l)	(((l) & SHIM_LABEL_MASK) >> SHIM_LABEL_OFFSET)
#define MPLS_SHIM_EXP_GET(l)	(((l) & SHIM_EXP_MASK) >> SHIM_EXP_OFFSET)
#define MPLS_SHIM_BOS_ISSET(l)	(((l) & SHIM_BOS_MASK) == SHIM_BOS_MASK)
#define MPLS_SHIM_TTL_GET(l)	((l) & SHIM_TTL_MASK)
#define MPLS_SHIM_TTL_SET(l, t)	(((l) & ~SHIM_TTL_MASK) | ((t) & SHIM_TTL_MASK))

/* Reserved lavel values (rfc3032) */
#define MPLS_LABEL_IPV4NULL	0               /* IPv4 Explicit NULL Label */
#define MPLS_LABEL_RTALERT	1               /* Router Alert Label       */
#define MPLS_LABEL_IPV6NULL	2               /* IPv6 Explicit NULL Label */
#define MPLS_LABEL_IMPLNULL	3               /* Implicit NULL Label      */
/*      MPLS_LABEL_RESERVED	4-15 */		/* Values 4-15 are reserved */
#define MPLS_LABEL_RESERVED_MAX 15

/*
 * Socket address
 */

struct sockaddr_mpls {
	u_int8_t	smpls_len;		/* length */
	u_int8_t	smpls_family;		/* AF_MPLS */
	u_int8_t	smpls_operation;
	u_int8_t	smpls_out_exp;		/* outgoing exp value */
	u_int32_t	smpls_out_label;	/* outgoing MPLS label */
	u_int16_t	smpls_out_ifindex;
	u_int16_t	smpls_in_ifindex;
	u_int32_t	smpls_in_label;		/* MPLS label 20 bits*/
#if MPLS_MCAST
	u_int8_t smpls_mcexp;
	u_int8_t smpls_pad2[2];
	u_int32_t smpls_mclabel;
#endif
};

#define MPLS_EXP_MASK	0x07		/* mpls exp value   ( 3bits) */

#define MPLS_OP_POP		1
#define MPLS_OP_PUSH		2
#define MPLS_OP_SWAP		3

#define MPLS_INKERNEL_LOOP_MAX	16

#define satosmpls(sa)		((struct sockaddr_mpls *)(sa))
#define smplstosa(smpls)	((struct sockaddr *)(smpls))
#define satosdl(sa)		((struct sockaddr_dl *)(sa))
#define sdltosa(sdl)		((struct sockaddr *)(sdl))

struct mpls_ifaddr {
	struct  ifaddr ia_ifa;		/* protocol-independent info */
#define ia_ifp		ia_ifa.ifa_ifp
#define ia_flags	ia_ifa.ifa_flags
	TAILQ_ENTRY(mpls_ifaddr) ia_list;	/* list of MPLS addresses */
	struct sockaddr_mpls ia_addr;	/* interface address */
	struct sockaddr ia_dstaddr;	/* peer dst address */
};

struct  mpls_aliasreq {
	char ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct sockaddr_mpls ifra_addr;
	struct sockaddr ifra_dstaddr;
	struct sockaddr_mpls ifra_mask;		/* not used */
};


/*
 * Names for IP sysctl objects
 */
#define	MPLSCTL_DEFTTL			1
#define	MPLSCTL_MAXINKLOOP		2
#define	MPLSCTL_PUSHEXPNULL_IP		3
#define	MPLSCTL_PUSHEXPNULL_IP6		4
#define	MPLSCTL_MAPTTL_IP		5
#define	MPLSCTL_MAPTTL_IP6		6
#define MPLSCTL_MAXID			7

#define MPLSCTL_NAMES { \
	{ 0, 0 }, \
	{ "ttl", CTLTYPE_INT }, \
	{ "maxloop_inkernel", CTLTYPE_INT }, \
	{ "pushexpnull_ip", CTLTYPE_INT }, \
	{ "pushexpnull_ip6", CTLTYPE_INT }, \
	{ "mapttl_ip", CTLTYPE_INT }, \
	{ "mapttl_ip6", CTLTYPE_INT }, \
}

#ifdef _KERNEL
TAILQ_HEAD(mpls_ifaddrhead, mpls_ifaddr);	/* the actual queue head */
extern struct mpls_ifaddrhead mpls_ifaddr;

extern void mpls_init(void);
extern int mpls_control(struct socket *, u_long, caddr_t, struct ifnet *);
extern void mpls_purgeif(struct ifnet *);
#endif /* _KERNEL */
#endif /* _NETMPLS_MPLS_H_ */
