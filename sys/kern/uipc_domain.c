/*	$OpenBSD: uipc_domain.c,v 1.32 2011/07/09 00:47:18 henning Exp $	*/
/*	$NetBSD: uipc_domain.c,v 1.14 1996/02/09 19:00:44 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)uipc_domain.c	8.2 (Berkeley) 10/18/93
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>
#include <sys/timeout.h>

#include "bluetooth.h"
#include "bpfilter.h"
#include "pflow.h"

struct	domain *domains;

void		pffasttimo(void *);
void		pfslowtimo(void *);
struct domain *	pffinddomain(int);

#if defined (KEY) || defined (IPSEC) || defined (TCP_SIGNATURE)
int pfkey_init(void);
#endif /* KEY || IPSEC || TCP_SIGNATURE */

#define	ADDDOMAIN(x)	{ \
	extern struct domain __CONCAT(x,domain); \
	__CONCAT(x,domain.dom_next) = domains; \
	domains = &__CONCAT(x,domain); \
}

void
domaininit(void)
{
	struct domain *dp;
	struct protosw *pr;
	static struct timeout pffast_timeout;
	static struct timeout pfslow_timeout;

#undef unix
	/*
	 * KAME NOTE: ADDDOMAIN(route) is moved to the last part so that
	 * it will be initialized as the *first* element.  confusing!
	 */
#ifndef lint
	ADDDOMAIN(unix);
#ifdef INET
	ADDDOMAIN(inet);
#endif
#ifdef INET6
	ADDDOMAIN(inet6);
#endif /* INET6 */
#if defined (KEY) || defined (IPSEC) || defined (TCP_SIGNATURE)
	pfkey_init();
#endif /* KEY || IPSEC */
#ifdef MPLS
       ADDDOMAIN(mpls);
#endif
#ifdef NATM
	ADDDOMAIN(natm);
#endif
#ifdef IPSEC
#ifdef __KAME__
	ADDDOMAIN(key);
#endif
#endif
#if NBLUETOOTH > 0
	ADDDOMAIN(bt);
#endif
	ADDDOMAIN(route);
#endif

	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_init)
			(*dp->dom_init)();
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_init)
				(*pr->pr_init)();
	}

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
	timeout_set(&pffast_timeout, pffasttimo, &pffast_timeout);
	timeout_set(&pfslow_timeout, pfslowtimo, &pfslow_timeout);
	timeout_add(&pffast_timeout, 1);
	timeout_add(&pfslow_timeout, 1);
}

struct domain *
pffinddomain(int family)
{
	struct domain *dp;

	for (dp = domains; dp != NULL; dp = dp->dom_next)
		if (dp->dom_family == family)
			return (dp);
	return (NULL);
}

struct protosw *
pffindtype(int family, int type)
{
	struct domain *dp;
	struct protosw *pr;

	dp = pffinddomain(family);
	if (dp == NULL)
		return (NULL);

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return (pr);
	return (NULL);
}

struct protosw *
pffindproto(int family, int protocol, int type)
{
	struct domain *dp;
	struct protosw *pr;
	struct protosw *maybe = NULL;

	if (family == 0)
		return (NULL);

	dp = pffinddomain(family);
	if (dp == NULL)
		return (NULL);

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if ((pr->pr_protocol == protocol) && (pr->pr_type == type))
			return (pr);

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == NULL)
			maybe = pr;
	}
	return (maybe);
}

int
net_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	struct domain *dp;
	struct protosw *pr;
	int family, protocol;

	/*
	 * All sysctl names at this level are nonterminal.
	 * Usually: next two components are protocol family and protocol
	 *	number, then at least one addition component.
	 */
	if (namelen < 2)
		return (EISDIR);		/* overloaded */
	family = name[0];

	if (family == 0)
		return (0);
#if NBPFILTER > 0
	if (family == PF_BPF)
		return (bpf_sysctl(name + 1, namelen - 1, oldp, oldlenp,
		    newp, newlen));
#endif
#if NPFLOW > 0
	if (family == PF_PFLOW)
		return (pflow_sysctl(name + 1, namelen - 1, oldp, oldlenp,
		    newp, newlen));
#endif
#ifdef PIPEX
	if (family == PF_PIPEX)
		return (pipex_sysctl(name + 1, namelen - 1, oldp, oldlenp,
		    newp, newlen));
#endif
	dp = pffinddomain(family);
	if (dp == NULL)
		return (ENOPROTOOPT);
#ifdef MPLS
	/* XXX WARNING: big fat ugly hack */
	/* stupid net.mpls is special as it does not have a protocol */
	if (family == PF_MPLS)
		return (dp->dom_protosw[0].pr_sysctl(name + 1, namelen - 1,
		    oldp, oldlenp, newp, newlen));
#endif

	if (namelen < 3)
		return (EISDIR);		/* overloaded */
	protocol = name[1];
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_protocol == protocol && pr->pr_sysctl)
			return ((*pr->pr_sysctl)(name + 2, namelen - 2,
			    oldp, oldlenp, newp, newlen));
	return (ENOPROTOOPT);
}

void
pfctlinput(int cmd, struct sockaddr *sa)
{
	struct domain *dp;
	struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, 0, NULL);
}

void
pfslowtimo(void *arg)
{
	struct timeout *to = (struct timeout *)arg;
	struct domain *dp;
	struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();
	timeout_add_msec(to, 500);
}

void
pffasttimo(void *arg)
{
	struct timeout *to = (struct timeout *)arg;
	struct domain *dp;
	struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();
	timeout_add_msec(to, 200);
}
