/*	$OpenBSD: if_bridge.h,v 1.1 1999/02/26 17:01:32 jason Exp $	*/

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
 * Bridge control request: (add/delete/iterate) member interfaces.
 */
struct ifbreq {
	char		ifbname[IFNAMSIZ];	/* bridge ifs name */
	char		ifsname[IFNAMSIZ];	/* member ifs name */
	u_int32_t	index;			/* iteration index */
};

/*
 * Bridge routing request: iterate known routes.
 */
struct ifbrtreq {
	char			ifbname[IFNAMSIZ];	/* bridge ifs name */
	u_int32_t		index;			/* iteration index */
	struct ether_addr	dst;			/* destination addr */
	char			ifsname[IFNAMSIZ];	/* destination ifs */
	u_int16_t		age;			/* route age */
};

#ifdef _KERNEL
struct mbuf *	bridge_input	__P((struct ifnet *, struct ether_header *,
    struct mbuf *));
int		bridge_output	__P((struct ifnet *, struct mbuf *,
    struct sockaddr *, struct rtentry *rt));
#endif /* _KERNEL */
