/*	$OpenBSD: if_pflog.c,v 1.70 2015/07/15 22:16:41 deraadt Exp $	*/
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and 
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece, 
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 * Copyright (c) 2001, Angelos D. Keromytis, Niels Provos.
 * Copyright (c) 2002 - 2010 Henning Brauer
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include "bpfilter.h"
#include "pflog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/stdint.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#endif /* INET6 */

#include <net/pfvar.h>
#include <net/if_pflog.h>

#define PFLOGMTU	(32768 + MHLEN + MLEN)

#ifdef PFLOGDEBUG
#define DPRINTF(x)    do { if (pflogdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

void	pflogattach(int);
int	pflogifs_resize(size_t);
int	pflogoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	    	       struct rtentry *);
int	pflogioctl(struct ifnet *, u_long, caddr_t);
void	pflogstart(struct ifnet *);
int	pflog_clone_create(struct if_clone *, int);
int	pflog_clone_destroy(struct ifnet *);
void	pflog_bpfcopy(const void *, void *, size_t);

LIST_HEAD(, pflog_softc)	pflogif_list;
struct if_clone	pflog_cloner =
    IF_CLONE_INITIALIZER("pflog", pflog_clone_create, pflog_clone_destroy);

int		  npflogifs = 0;
struct ifnet	**pflogifs = NULL;	/* for fast access */
struct mbuf	 *pflog_mhdr = NULL, *pflog_mptr = NULL;

void
pflogattach(int npflog)
{
	LIST_INIT(&pflogif_list);
	if (pflog_mhdr == NULL)
		if ((pflog_mhdr = m_get(M_DONTWAIT, MT_HEADER)) == NULL)
			panic("pflogattach: no mbuf");
	if (pflog_mptr == NULL)
		if ((pflog_mptr = m_get(M_DONTWAIT, MT_DATA)) == NULL)
			panic("pflogattach: no mbuf");
	if_clone_attach(&pflog_cloner);
}

int
pflogifs_resize(size_t n)
{
	struct ifnet	**p;
	int		  i;

	if (n > SIZE_MAX / sizeof(*p))
		return (EINVAL);
	if (n == 0)
		p = NULL;
	else
		if ((p = mallocarray(n, sizeof(*p), M_DEVBUF,
		    M_NOWAIT|M_ZERO)) == NULL)
			return (ENOMEM);
	for (i = 0; i < n; i++)
		if (i < npflogifs)
			p[i] = pflogifs[i];
		else
			p[i] = NULL;

	if (pflogifs)
		free(pflogifs, M_DEVBUF, 0);
	pflogifs = p;
	npflogifs = n;
	return (0);
}

int
pflog_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet *ifp;
	struct pflog_softc *pflogif;
	int s;

	if ((pflogif = malloc(sizeof(*pflogif),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	pflogif->sc_unit = unit;
	ifp = &pflogif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pflog%d", unit);
	ifp->if_softc = pflogif;
	ifp->if_mtu = PFLOGMTU;
	ifp->if_ioctl = pflogioctl;
	ifp->if_output = pflogoutput;
	ifp->if_start = pflogstart;
	ifp->if_type = IFT_PFLOG;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_hdrlen = PFLOG_HDRLEN;
	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&pflogif->sc_if.if_bpf, ifp, DLT_PFLOG, PFLOG_HDRLEN);
#endif

	s = splnet();
	LIST_INSERT_HEAD(&pflogif_list, pflogif, sc_list);
	if (unit + 1 > npflogifs && pflogifs_resize(unit + 1) != 0) {
		splx(s);
		return (ENOMEM);
	}
	pflogifs[unit] = ifp;
	splx(s);

	return (0);
}

int
pflog_clone_destroy(struct ifnet *ifp)
{
	struct pflog_softc	*pflogif = ifp->if_softc;
	int			 s, i;

	s = splnet();
	pflogifs[pflogif->sc_unit] = NULL;
	LIST_REMOVE(pflogif, sc_list);

	for (i = npflogifs; i > 0 && pflogifs[i - 1] == NULL; i--)
		; /* nothing */
	if (i < npflogifs)
		pflogifs_resize(i);	/* error harmless here */
	splx(s);

	if_detach(ifp);
	free(pflogif, M_DEVBUF, 0);
	return (0);
}

/*
 * Start output on the pflog interface.
 */
void
pflogstart(struct ifnet *ifp)
{
	struct mbuf *m;
	int s;

	for (;;) {
		s = splnet();
		IF_DROP(&ifp->if_snd);
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;
		m_freem(m);
	}
}

int
pflogoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
int
pflogioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

int
pflog_packet(struct pf_pdesc *pd, u_int8_t reason, struct pf_rule *rm,
    struct pf_rule *am, struct pf_ruleset *ruleset, struct pf_rule *trigger)
{
#if NBPFILTER > 0
	struct ifnet *ifn;
	struct pfloghdr hdr;

	if (rm == NULL || pd == NULL || pd->kif == NULL || pd->m == NULL)
		return (-1);
	if (trigger == NULL)
		trigger = rm;

	if (trigger->logif >= npflogifs || (ifn = pflogifs[trigger->logif]) ==
	    NULL || !ifn->if_bpf)
		return (0);

	bzero(&hdr, sizeof(hdr));
	hdr.length = PFLOG_REAL_HDRLEN;
	hdr.action = rm->action;
	hdr.reason = reason;
	memcpy(hdr.ifname, pd->kif->pfik_name, sizeof(hdr.ifname));

	if (am == NULL) {
		hdr.rulenr = htonl(rm->nr);
		hdr.subrulenr = -1;
	} else {
		hdr.rulenr = htonl(am->nr);
		hdr.subrulenr = htonl(rm->nr);
		if (ruleset != NULL && ruleset->anchor != NULL)
			strlcpy(hdr.ruleset, ruleset->anchor->name,
			    sizeof(hdr.ruleset));
	}
	if (trigger->log & PF_LOG_SOCKET_LOOKUP && !pd->lookup.done)
		pd->lookup.done = pf_socket_lookup(pd);
	if (pd->lookup.done > 0) {
		hdr.uid = pd->lookup.uid;
		hdr.pid = pd->lookup.pid;
	} else {
		hdr.uid = UID_MAX;
		hdr.pid = NO_PID;
	}
	hdr.rule_uid = rm->cuid;
	hdr.rule_pid = rm->cpid;
	hdr.dir = pd->dir;

	PF_ACPY(&hdr.saddr, &pd->nsaddr, pd->naf);
	PF_ACPY(&hdr.daddr, &pd->ndaddr, pd->naf);
	hdr.af = pd->af;
	hdr.naf = pd->naf;
	hdr.sport = pd->nsport;
	hdr.dport = pd->ndport;

	ifn->if_opackets++;
	ifn->if_obytes += pd->m->m_pkthdr.len;

	bpf_mtap_hdr(ifn->if_bpf, (caddr_t)&hdr, PFLOG_HDRLEN, pd->m,
	    BPF_DIRECTION_OUT, pflog_bpfcopy);
#endif

	return (0);
}

void
pflog_bpfcopy(const void *src_arg, void *dst_arg, size_t len)
{
	struct mbuf		*m, *mp, *mhdr, *mptr;
	struct pfloghdr		*pfloghdr;
	u_int			 count;
	u_char			*dst, *mdst;
	int			 afto, hlen, mlen, off;
	union pf_headers {
		struct tcphdr		tcp;
		struct udphdr		udp;
		struct icmp		icmp;
#ifdef INET6
		struct icmp6_hdr	icmp6;
		struct mld_hdr		mld;
		struct nd_neighbor_solicit nd_ns;
#endif /* INET6 */
	} pdhdrs;

	struct pf_pdesc		 pd;
	struct pf_addr		 osaddr, odaddr;
	u_int16_t		 osport = 0, odport = 0;
	u_int8_t		 proto = 0;

	m = (struct mbuf *)src_arg;
	dst = dst_arg;

	mhdr = pflog_mhdr;
	mptr = pflog_mptr;

	if (m == NULL)
		panic("pflog_bpfcopy got no mbuf");

	/* first mbuf holds struct pfloghdr */
	pfloghdr = mtod(m, struct pfloghdr *);
	afto = pfloghdr->af != pfloghdr->naf;
	count = min(m->m_len, len);
	bcopy(pfloghdr, dst, count);
	pfloghdr = (struct pfloghdr *)dst;
	dst += count;
	len -= count;
	m = m->m_next;

	if (len <= 0)
		return;

	/* second mbuf is pkthdr */
	if (m == NULL)
		panic("no second mbuf");

	/*
	 * temporary mbuf will hold an ip/ip6 header and 8 bytes
	 * of the protocol header
	 */
	m_inithdr(mhdr);
	mhdr->m_len = 0;	/* XXX not done in m_inithdr() */

#if INET && INET6
	/* offset for a new header */
	if (afto && pfloghdr->af == AF_INET)
		mhdr->m_data += sizeof(struct ip6_hdr) -
		    sizeof(struct ip);
#endif /* INET && INET6 */

	mdst = mtod(mhdr, char *);
	switch (pfloghdr->af) {
	case AF_INET: {
		struct ip	*h;

		if (m->m_pkthdr.len < sizeof(*h))
			goto copy;
		m_copydata(m, 0, sizeof(*h), mdst);
		h = (struct ip *)mdst;
		hlen = h->ip_hl << 2;
		if (hlen > sizeof(*h) && (m->m_pkthdr.len >= hlen))
			m_copydata(m, sizeof(*h), hlen - sizeof(*h),
			    mdst + sizeof(*h));
		break;
	    }
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr	*h;

		if (m->m_pkthdr.len < sizeof(*h))
			goto copy;
		hlen = sizeof(struct ip6_hdr);
		m_copydata(m, 0, hlen, mdst);
		h = (struct ip6_hdr *)mdst;
		proto = h->ip6_nxt;
		break;
	    }
#endif /* INET6 */
	default:
		/* shouldn't happen ever :-) */
		goto copy;
	}

	if (m->m_pkthdr.len < hlen + 8 && proto != IPPROTO_NONE)
		goto copy;
	else if (proto != IPPROTO_NONE) {
		/* copy 8 bytes of the protocol header */
		m_copydata(m, hlen, 8, mdst + hlen);
		hlen += 8;
	}

	mhdr->m_len += hlen;
	mhdr->m_pkthdr.len = mhdr->m_len;

	/* create a chain mhdr -> mptr, mptr->m_data = (m->m_data+hlen) */
	mp = m_getptr(m, hlen, &off);
	if (mp != NULL) {
		bcopy(mp, mptr, sizeof(*mptr));
		mptr->m_data += off;
		mptr->m_len -= off;
		mptr->m_flags &= ~M_PKTHDR;
		mhdr->m_next = mptr;
		mhdr->m_pkthdr.len += m->m_pkthdr.len - hlen;
	}

	/*
	 * Rewrite addresses if needed. Reason pointer must be NULL to avoid
	 * counting the packet here again.
	 */
	if (pf_setup_pdesc(&pd, &pdhdrs, pfloghdr->af, pfloghdr->dir, NULL,
	    mhdr, NULL) != PF_PASS)
		goto copy;
	pd.naf = pfloghdr->naf;

	PF_ACPY(&osaddr, pd.src, pd.af);
	PF_ACPY(&odaddr, pd.dst, pd.af);
	if (pd.sport)
		osport = *pd.sport;
	if (pd.dport)
		odport = *pd.dport;

	if (pd.virtual_proto != PF_VPROTO_FRAGMENT &&
	    (pfloghdr->rewritten = pf_translate(&pd, &pfloghdr->saddr,
	    pfloghdr->sport, &pfloghdr->daddr, pfloghdr->dport, 0,
	    pfloghdr->dir))) {
		m_copyback(pd.m, pd.off, min(pd.m->m_len - pd.off, pd.hdrlen),
		    pd.hdr.any, M_NOWAIT);
#if INET && INET6
		if (afto) {
			PF_ACPY(&pd.nsaddr, &pfloghdr->saddr, pd.naf);
			PF_ACPY(&pd.ndaddr, &pfloghdr->daddr, pd.naf);
		}
#endif /* INET && INET6 */
		PF_ACPY(&pfloghdr->saddr, &osaddr, pd.af);
		PF_ACPY(&pfloghdr->daddr, &odaddr, pd.af);
		pfloghdr->sport = osport;
		pfloghdr->dport = odport;
	}

	pd.tot_len = min(pd.tot_len, len);
	pd.tot_len -= pd.m->m_data - pd.m->m_pktdat;

#if INET && INET6
	if (afto && pfloghdr->rewritten)
		pf_translate_af(&pd);
#endif /* INET && INET6 */

	m = pd.m;
 copy:
	mlen = min(m->m_pkthdr.len, len);
	m_copydata(m, 0, mlen, dst);
	len -= mlen;
	if (len > 0)
		bzero(dst + mlen, len);
}
