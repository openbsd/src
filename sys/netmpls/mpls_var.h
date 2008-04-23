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
 *
 *	$Id: mpls_var.h,v 1.1 2008/04/23 11:00:35 norby Exp $
 */

#ifdef _KERNEL
extern int mpls_defttl;
extern int mpls_inkloop;
extern int mpls_push_expnull_ip;
extern int mpls_push_expnull_ip6;
extern int mpls_mapttl_ip;
extern int mpls_mapttl_ip6;

extern int mpls_lse(struct ifnet *, struct mbuf *,
				struct sockaddr_mpls *, u_int8_t);
extern int mpls_canfwbympls(u_int32_t);
extern struct mbuf *mpls_shim_peep(struct mbuf *, struct sockaddr_mpls *,
					u_int32_t *, u_int8_t *, u_int8_t *);
extern struct mbuf *mpls_shim_pop(struct mbuf *, struct sockaddr_mpls *,
					u_int32_t *, u_int8_t *, u_int8_t *);
extern struct mbuf *mpls_shim_swap(struct mbuf *, struct sockaddr_mpls *,
					u_int32_t *);
extern struct mbuf *mpls_shim_push(struct mbuf *, struct sockaddr_mpls *,
					u_int32_t *, u_int8_t *, u_int8_t *);

extern int mpls_raw_usrreq(struct socket *, int, struct mbuf *,
			struct mbuf *, struct mbuf *);

extern int mpls_sysctl(int *, u_int, void *, size_t *, void *, size_t);



extern struct  ifqueue mplsintrq;	/* MPLS input queue */

void	mplsintr(void);
#endif /* _KERNEL */
