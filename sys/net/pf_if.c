/*	$OpenBSD: pf_if.c,v 1.1 2003/12/12 20:05:45 cedric Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/pfvar.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#define DPFPRINTF(n, x)	if (pf_status.debug >= (n)) printf x

/*
 * Global variables
 */

void	pfi_dynaddr_update(void *);

int
pfi_dynaddr_setup(struct pf_addr_wrap *aw, sa_family_t af)
{
	if (aw->type != PF_ADDR_DYNIFTL)
		return (0);
	aw->p.dyn = pool_get(&pf_addr_pl, PR_NOWAIT);
	if (aw->p.dyn == NULL)
		return (1);
	bcopy(aw->v.ifname, aw->p.dyn->ifname, sizeof(aw->p.dyn->ifname));
	aw->p.dyn->ifp = ifunit(aw->p.dyn->ifname);
	if (aw->p.dyn->ifp == NULL) {
		pool_put(&pf_addr_pl, aw->p.dyn);
		aw->p.dyn = NULL;
		return (1);
	}
	aw->p.dyn->addr = &aw->v.a.addr;
	aw->p.dyn->af = af;
	aw->p.dyn->undefined = 1;
	aw->p.dyn->hook_cookie = hook_establish(
	    aw->p.dyn->ifp->if_addrhooks, 1,
	    pfi_dynaddr_update, aw->p.dyn);
	if (aw->p.dyn->hook_cookie == NULL) {
		pool_put(&pf_addr_pl, aw->p.dyn);
		aw->p.dyn = NULL;
		return (1);
	}
	pfi_dynaddr_update(aw->p.dyn);
	return (0);
}

void
pfi_dynaddr_update(void *p)
{
	struct pf_addr_dyn	*ad = (struct pf_addr_dyn *)p;
	struct ifaddr		*ia;
	int			 s, changed = 0;

	if (ad == NULL || ad->ifp == NULL)
		panic("pfi_dynaddr_update");
	s = splsoftnet();
	TAILQ_FOREACH(ia, &ad->ifp->if_addrlist, ifa_list)
		if (ia->ifa_addr != NULL &&
		    ia->ifa_addr->sa_family == ad->af) {
			if (ad->af == AF_INET) {
				struct in_addr *a, *b;

				a = &ad->addr->v4;
				b = &((struct sockaddr_in *)ia->ifa_addr)
				    ->sin_addr;
				if (ad->undefined ||
				    memcmp(a, b, sizeof(*a))) {
					bcopy(b, a, sizeof(*a));
					changed = 1;
				}
			} else if (ad->af == AF_INET6) {
				struct in6_addr *a, *b;

				a = &ad->addr->v6;
				b = &((struct sockaddr_in6 *)ia->ifa_addr)
				    ->sin6_addr;
				if (ad->undefined ||
				    memcmp(a, b, sizeof(*a))) {
					bcopy(b, a, sizeof(*a));
					changed = 1;
				}
			}
			if (changed)
				ad->undefined = 0;
			break;
		}
	if (ia == NULL)
		ad->undefined = 1;
	splx(s);
}

void
pfi_dynaddr_remove(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL)
		return;
	hook_disestablish(aw->p.dyn->ifp->if_addrhooks,
	    aw->p.dyn->hook_cookie);
	pool_put(&pf_addr_pl, aw->p.dyn);
	aw->p.dyn = NULL;
}

void
pfi_dynaddr_copyout(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL)
		return;
	bcopy(aw->p.dyn->ifname, aw->v.ifname, sizeof(aw->v.ifname));
	aw->p.dyn = (struct pf_addr_dyn *)1;
}
