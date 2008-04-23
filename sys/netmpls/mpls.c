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
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

/*
 *
 *	$Id: mpls.c,v 1.1 2008/04/23 11:00:35 norby Exp $
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netmpls/mpls.h>
#include <netmpls/mpls_var.h>

extern void mpls_purgeaddr(struct ifaddr *, struct ifnet *);
extern int mpls_ifinit(struct ifnet *, struct mpls_ifaddr *,
			    struct sockaddr_mpls *, int);
extern void mpls_ifscrub(struct mpls_ifaddr *);

struct mpls_ifaddrhead mpls_ifaddr;
struct sockaddr_mpls mpls_scope_sockmask;

void
mpls_init()
{
	mplsintrq.ifq_maxlen = 50;
}

void
mpls_purgeaddr(struct ifaddr *ifa, struct ifnet *ifp)
{
}

void
mpls_purgeif(struct ifnet *ifp)
{
}

/*
 * Delete any existing route for an interface.
 */
void
mpls_ifscrub(struct mpls_ifaddr *ia)
{
}

/*
 * Initialize an interface's MPLS address
 * and routing table entry.
 */
int
mpls_ifinit(struct ifnet *ifp, struct mpls_ifaddr *ia,
    struct sockaddr_mpls *smpls, int scrub)
{
#ifdef MPLS_DEBUG
	panic("mpls_ifinit\n");
#endif
	return (EAFNOSUPPORT);
}

/*
 * Generic MPLS control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
mpls_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp)
{
	return (EOPNOTSUPP);
}
