/*	$OpenBSD: if_bridge.h,v 1.5 1999/03/19 02:46:54 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Bridge control request: add/delete member interfaces.
 */
struct ifbreq {
	char		ifbr_name[IFNAMSIZ];	/* bridge ifs name */
	char		ifbr_ifsname[IFNAMSIZ];	/* member ifs name */
	u_int32_t	ifbr_ifsflags;		/* memver ifs flags */
};

#define	IFBIF_LEARNING	0x1			/* ifs can learn */

/*
 * Interface list structure
 */
struct ifbifconf {
	char		ifbic_name[IFNAMSIZ];	/* bridge ifs name */
	u_int32_t	ifbic_len;		/* buffer size */
	union {
		caddr_t	ifbicu_buf;
		struct	ifbreq *ifbicu_req;
	} ifbic_ifbicu;
#define	ifbic_buf	ifbic_ifbicu.ifbicu_buf
#define	ifbic_req	ifbic_ifbicu.ifbicu_req
};

/*
 * Bridge address request
 */
struct ifbareq {
	char			ifba_name[IFNAMSIZ];	/* bridge name */
	char			ifba_ifsname[IFNAMSIZ];	/* destination ifs */
	u_int8_t		ifba_age;		/* address age */
	u_int8_t		ifba_flags;		/* address flags */
	struct ether_addr	ifba_dst;		/* destination addr */
};

#define	IFBAF_TYPEMASK		0x03		/* address type mask */
#define	IFBAF_DYNAMIC		0x00		/* dynamically learned */
#define	IFBAF_STATIC		0x01		/* static address */

struct ifbaconf {
	char			ifbac_name[IFNAMSIZ];	/* bridge ifs name */
	u_int32_t		ifbac_len;		/* buffer size */
	union {
		caddr_t	ifbacu_buf;			/* buffer */
		struct ifbareq *ifbacu_req;		/* request pointer */
	} ifbac_ifbacu;
#define	ifbac_buf	ifbac_ifbacu.ifbacu_buf
#define	ifbac_req	ifbac_ifbacu.ifbacu_req
};

/*
 * Bridge cache size get/set
 */
struct ifbcachereq {
	char			ifbc_name[IFNAMSIZ];	/* bridge ifs name */
	u_int32_t		ifbc_size;		/* cache size */
};

/*
 * Bridge cache timeout get/set
 */
struct ifbcachetoreq {
	char			ifbct_name[IFNAMSIZ];	/* bridge ifs name */
	u_int32_t		ifbct_time;		/* cache time (sec) */
};


#ifdef _KERNEL
struct mbuf *	bridge_input	__P((struct ifnet *, struct ether_header *,
    struct mbuf *));
int		bridge_output	__P((struct ifnet *, struct mbuf *,
    struct sockaddr *, struct rtentry *rt));
#endif /* _KERNEL */
