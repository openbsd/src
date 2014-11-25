/*	$OpenBSD: radix_mpath.h,v 1.14 2014/11/25 14:50:46 mpi Exp $	*/
/*	$KAME: radix_mpath.h,v 1.9 2004/03/30 11:21:49 keiichi Exp $	*/

/*
 * Copyright (C) 2001 WIDE Project.
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
 * THE AUTHORS DO NOT GUARANTEE THAT THIS SOFTWARE DOES NOT INFRINGE
 * ANY OTHERS' INTELLECTUAL PROPERTIES. IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY INFRINGEMENT OF ANY OTHERS' INTELLECTUAL
 * PROPERTIES.
 */

#ifndef _NET_RADIX_MPATH_H_
#define	_NET_RADIX_MPATH_H_

#ifdef _KERNEL
/*
 * Radix tree API with multipath support
 */
struct rtentry;
struct sockaddr;
int	rn_mpath_capable(struct radix_node_head *);
#define RMP_MODE_ACTIVE	0
#define RMP_MODE_ALL	1
#define RMP_MODE_BYPRIO	2
struct radix_node *rn_mpath_next(struct radix_node *, int);
struct radix_node *rn_mpath_prio(struct radix_node *, u_int8_t);
void	rn_mpath_reprio(struct radix_node *, int);
void	rn_mpath_adj_mpflag(struct radix_node *, u_int8_t);
int	rn_mpath_active_count(struct radix_node *);
struct rtentry *rt_mpath_matchgate(struct rtentry *, struct sockaddr *,
	    u_int8_t);
int	rt_mpath_conflict(struct radix_node_head *, struct rtentry *,
	    struct sockaddr *, int);
struct rtentry *rtalloc_mpath(struct sockaddr *, u_int32_t *, u_int);
int	rn_mpath_inithead(void **, int);
#endif /* _KERNEL */

#endif /* _NET_RADIX_MPATH_H_ */
