/* $OpenBSD: if_pflog.h,v 1.7 2002/10/29 19:51:04 mickey Exp $ */
/*
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
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

#ifndef _NET_IF_PFLOG_H_
#define _NET_IF_PFLOG_H_

struct pflog_softc {
	struct ifnet	sc_if;  /* the interface */
};

struct pfloghdr {
	u_int32_t af;
	char ifname[IFNAMSIZ];
	short rnr;
	u_short reason;
	u_short action;
	u_short dir;
};

#define PFLOG_HDRLEN	sizeof(struct pfloghdr)

#ifdef _KERNEL

#if NPFLOG > 0
#define	PFLOG_PACKET(i,x,a,b,c,d,e) \
	do { \
		if (b == AF_INET) { \
			HTONS(((struct ip *)x)->ip_len); \
			HTONS(((struct ip *)x)->ip_off); \
			pflog_packet(i,a,b,c,d,e); \
			NTOHS(((struct ip *)x)->ip_len); \
			NTOHS(((struct ip *)x)->ip_off); \
		} else { \
			pflog_packet(i,a,b,c,d,e); \
		} \
	} while (0)
#else
#define	PFLOG_PACKET(i,x,a,b,c,d,e)	((void)0)
#endif /* NPFLOG > 0 */
#endif /* _KERNEL */
#endif /* _NET_IF_PFLOG_H_ */
