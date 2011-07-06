/*	$OpenBSD: if_ppp.c,v 1.61 2011/07/06 02:42:28 henning Exp $	*/
/*	$NetBSD: if_ppp.c,v 1.39 1997/05/17 21:11:59 christos Exp $	*/

/*
 * if_ppp.c - Point-to-Point Protocol (PPP) Asynchronous driver.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Based on:
 *	@(#)if_sl.c	7.6.1.2 (Berkeley) 2/15/89
 *
 * Copyright (c) 1987, 1989, 1992, 1993
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
 * Serial Line interface
 *
 * Rick Adams
 * Center for Seismic Studies
 * 1300 N 17th Street, Suite 1450
 * Arlington, Virginia 22209
 * (703)276-7900
 * rick@seismo.ARPA
 * seismo!rick
 *
 * Pounded on heavily by Chris Torek (chris@mimsy.umd.edu, umcp-cs!chris).
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 *
 * Converted to 4.3BSD+ 386BSD by Brad Parker (brad@cayman.com)
 * Added VJ tcp header compression; more unified ioctls
 *
 * Extensively modified by Paul Mackerras (paulus@cs.anu.edu.au).
 * Cleaned up a lot of the mbuf-related code to fix bugs that
 * caused system crashes and packet corruption.  Changed pppstart
 * so that it doesn't just give up with a collision if the whole
 * packet doesn't fit in the output ring buffer.
 *
 * Added priority queueing for interactive IP packets, following
 * the model of if_sl.c, plus hooks for bpf.
 * Paul Mackerras (paulus@cs.anu.edu.au).
 */

/* from if_sl.c,v 1.11 84/10/04 12:54:47 rick Exp */
/* from NetBSD: if_ppp.c,v 1.15.2.2 1994/07/28 05:17:58 cgd Exp */

#include "ppp.h"
#if NPPP > 0

#define VJC
#define PPP_COMPRESS

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#else
#ifdef _KERNEL
#ifdef VJC
#error ppp device with VJC assumes INET
#endif
#endif
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef VJC
#include <net/slcompress.h>
#endif

#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#include <net/if_pppvar.h>
#include <machine/cpu.h>

#ifdef PPP_COMPRESS
#define PACKETPTR	struct mbuf *
#include <net/ppp-comp.h>
#endif

static int	pppsioctl(struct ifnet *, u_long, caddr_t);
static void	ppp_requeue(struct ppp_softc *);
static void	ppp_ccp(struct ppp_softc *, struct mbuf *m, int rcvd);
static void	ppp_ccp_closed(struct ppp_softc *);
static void	ppp_inproc(struct ppp_softc *, struct mbuf *);
static void	pppdumpm(struct mbuf *m0);
#ifdef ALTQ
static void	ppp_ifstart(struct ifnet *ifp);
#endif
int		ppp_clone_create(struct if_clone *, int);
int		ppp_clone_destroy(struct ifnet *);

/*
 * Some useful mbuf macros not in mbuf.h.
 */
#define M_IS_CLUSTER(m)	((m)->m_flags & M_EXT)

#define M_DATASTART(m)	\
	(M_IS_CLUSTER(m) ? (m)->m_ext.ext_buf : \
	    (m)->m_flags & M_PKTHDR ? (m)->m_pktdat : (m)->m_dat)

#define M_DATASIZE(m)	\
	(M_IS_CLUSTER(m) ? (m)->m_ext.ext_size : \
	    (m)->m_flags & M_PKTHDR ? MHLEN: MLEN)

/*
 * We steal two bits in the mbuf m_flags, to mark high-priority packets
 * for output, and received packets following lost/corrupted packets.
 */
#define M_HIGHPRI	0x2000	/* output packet for sc_fastq */
#define M_ERRMARK	0x4000	/* steal a bit in mbuf m_flags */


#ifdef PPP_COMPRESS
/*
 * List of compressors we know about.
 * We leave some space so maybe we can modload compressors.
 */

extern struct compressor ppp_bsd_compress;
extern struct compressor ppp_deflate, ppp_deflate_draft;

struct compressor *ppp_compressors[8] = {
#if DO_BSD_COMPRESS && defined(PPP_BSDCOMP)
    &ppp_bsd_compress,
#endif
#if DO_DEFLATE && defined(PPP_DEFLATE)
    &ppp_deflate,
    &ppp_deflate_draft,
#endif
    NULL
};
#endif /* PPP_COMPRESS */

LIST_HEAD(, ppp_softc) ppp_softc_list;
struct if_clone ppp_cloner =
    IF_CLONE_INITIALIZER("ppp", ppp_clone_create, ppp_clone_destroy);

/*
 * Called from boot code to establish ppp interfaces.
 */
void
pppattach()
{
    LIST_INIT(&ppp_softc_list);
    if_clone_attach(&ppp_cloner);
}

int
ppp_clone_create(ifc, unit)
    struct if_clone *ifc;
    int unit;
{
    struct ppp_softc *sc;
    int s;

    sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO);
    if (!sc)
	return (ENOMEM);

    sc->sc_unit = unit;
    snprintf(sc->sc_if.if_xname, sizeof sc->sc_if.if_xname, "%s%d",
	ifc->ifc_name, unit);
    sc->sc_if.if_softc = sc;
    sc->sc_if.if_mtu = PPP_MTU;
    sc->sc_if.if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
    sc->sc_if.if_type = IFT_PPP;
    sc->sc_if.if_hdrlen = PPP_HDRLEN;
    sc->sc_if.if_ioctl = pppsioctl;
    sc->sc_if.if_output = pppoutput;
#ifdef ALTQ
    sc->sc_if.if_start = ppp_ifstart;
#endif
    IFQ_SET_MAXLEN(&sc->sc_if.if_snd, ifqmaxlen);
    IFQ_SET_MAXLEN(&sc->sc_inq, ifqmaxlen);
    IFQ_SET_MAXLEN(&sc->sc_fastq, ifqmaxlen);
    IFQ_SET_MAXLEN(&sc->sc_rawq, ifqmaxlen);
    IFQ_SET_READY(&sc->sc_if.if_snd);
    if_attach(&sc->sc_if);
    if_alloc_sadl(&sc->sc_if);
#if NBPFILTER > 0
    bpfattach(&sc->sc_bpf, &sc->sc_if, DLT_PPP, PPP_HDRLEN);
#endif
    s = splnet();
    LIST_INSERT_HEAD(&ppp_softc_list, sc, sc_list);
    splx(s);

    return (0);
}

int
ppp_clone_destroy(ifp)
    struct ifnet *ifp;
{
    struct ppp_softc *sc = ifp->if_softc;
    int s;

    if (sc->sc_devp != NULL)
	return (EBUSY);

    s = splnet();
    LIST_REMOVE(sc, sc_list);
    splx(s);

    if_detach(ifp);

    free(sc, M_DEVBUF);
    return (0);
}

/*
 * Allocate a ppp interface unit and initialize it.
 */
struct ppp_softc *
pppalloc(pid)
    pid_t pid;
{
    int i;
    struct ppp_softc *sc;

    LIST_FOREACH(sc, &ppp_softc_list, sc_list)
	if (sc->sc_xfer == pid) {
	    sc->sc_xfer = 0;
	    return sc;
	}
    LIST_FOREACH(sc, &ppp_softc_list, sc_list)
	if (sc->sc_devp == NULL)
	    break;
    if (sc == NULL)
	return NULL;

    sc->sc_flags = 0;
    sc->sc_mru = PPP_MRU;
    sc->sc_relinq = NULL;
    bzero((char *)&sc->sc_stats, sizeof(sc->sc_stats));
#ifdef VJC
    sc->sc_comp = malloc(sizeof(struct slcompress), M_DEVBUF, M_NOWAIT);
    if (sc->sc_comp)
	sl_compress_init(sc->sc_comp);
#endif
#ifdef PPP_COMPRESS
    sc->sc_xc_state = NULL;
    sc->sc_rc_state = NULL;
#endif /* PPP_COMPRESS */
    for (i = 0; i < NUM_NP; ++i)
	sc->sc_npmode[i] = NPMODE_ERROR;
    sc->sc_npqueue = NULL;
    sc->sc_npqtail = &sc->sc_npqueue;
    sc->sc_last_sent = sc->sc_last_recv = time_second;

    return sc;
}

/*
 * Deallocate a ppp unit.  Must be called at splsoftnet or higher.
 */
void
pppdealloc(sc)
    struct ppp_softc *sc;
{
    struct mbuf *m;

    splsoftassert(IPL_SOFTNET);

    if_down(&sc->sc_if);
    sc->sc_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
    sc->sc_devp = NULL;
    sc->sc_xfer = 0;
    for (;;) {
	IF_DEQUEUE(&sc->sc_rawq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }
    for (;;) {
	IF_DEQUEUE(&sc->sc_inq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }
    for (;;) {
	IF_DEQUEUE(&sc->sc_fastq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }
    while ((m = sc->sc_npqueue) != NULL) {
	sc->sc_npqueue = m->m_nextpkt;
	m_freem(m);
    }
    if (sc->sc_togo != NULL) {
	m_freem(sc->sc_togo);
	sc->sc_togo = NULL;
    }
#ifdef PPP_COMPRESS
    ppp_ccp_closed(sc);
    sc->sc_xc_state = NULL;
    sc->sc_rc_state = NULL;
#endif /* PPP_COMPRESS */
#if NBPFILTER > 0
    if (sc->sc_pass_filt.bf_insns != 0) {
	free(sc->sc_pass_filt.bf_insns, M_DEVBUF);
	sc->sc_pass_filt.bf_insns = 0;
	sc->sc_pass_filt.bf_len = 0;
    }
    if (sc->sc_active_filt.bf_insns != 0) {
	free(sc->sc_active_filt.bf_insns, M_DEVBUF);
	sc->sc_active_filt.bf_insns = 0;
	sc->sc_active_filt.bf_len = 0;
    }
#endif
#ifdef VJC
    if (sc->sc_comp != 0) {
	free(sc->sc_comp, M_DEVBUF);
	sc->sc_comp = 0;
    }
#endif
}

/*
 * Ioctl routine for generic ppp devices.
 */
int
pppioctl(sc, cmd, data, flag, p)
    struct ppp_softc *sc;
    u_long cmd;
    caddr_t data;
    int flag;
    struct proc *p;
{
    int s, error, flags, mru, npx;
    u_int nb;
    struct ppp_option_data *odp;
    struct compressor **cp;
    struct npioctl *npi;
    time_t t;
#if NBPFILTER > 0
    struct bpf_program *bp, *nbp;
    struct bpf_insn *newcode, *oldcode;
    int newcodelen;
#endif
#ifdef	PPP_COMPRESS
    u_char ccp_option[CCP_MAX_OPTION_LENGTH];
#endif

    switch (cmd) {
    case FIONREAD:
	*(int *)data = sc->sc_inq.ifq_len;
	break;

    case PPPIOCGUNIT:
	*(int *)data = sc->sc_unit;	/* XXX */
	break;

    case PPPIOCGFLAGS:
	*(u_int *)data = sc->sc_flags;
	break;

    case PPPIOCSFLAGS:
	if ((error = suser(p, 0)) != 0)
	    return (error);
	flags = *(int *)data & SC_MASK;
	s = splsoftnet();
#ifdef PPP_COMPRESS
	if (sc->sc_flags & SC_CCP_OPEN && !(flags & SC_CCP_OPEN))
	    ppp_ccp_closed(sc);
#endif
	splnet();
	sc->sc_flags = (sc->sc_flags & ~SC_MASK) | flags;
	splx(s);
	break;

    case PPPIOCSMRU:
	if ((error = suser(p, 0)) != 0)
	    return (error);
	mru = *(int *)data;
	if (mru >= PPP_MRU && mru <= PPP_MAXMRU)
	    sc->sc_mru = mru;
	break;

    case PPPIOCGMRU:
	*(int *)data = sc->sc_mru;
	break;

#ifdef VJC
    case PPPIOCSMAXCID:
	if ((error = suser(p, 0)) != 0)
	    return (error);
	if (sc->sc_comp) {
	    s = splsoftnet();
	    sl_compress_setup(sc->sc_comp, *(int *)data);
	    splx(s);
	}
	break;
#endif

    case PPPIOCXFERUNIT:
	if ((error = suser(p, 0)) != 0)
	    return (error);
	sc->sc_xfer = p->p_pid;
	break;

#ifdef PPP_COMPRESS
    case PPPIOCSCOMPRESS:
	if ((error = suser(p, 0)) != 0)
	    return (error);
	odp = (struct ppp_option_data *) data;
	nb = odp->length;
	if (nb > sizeof(ccp_option))
	    nb = sizeof(ccp_option);
	if ((error = copyin(odp->ptr, ccp_option, nb)) != 0)
	    return (error);
	if (ccp_option[1] < 2)	/* preliminary check on the length byte */
	    return (EINVAL);
	for (cp = ppp_compressors; *cp != NULL; ++cp)
	    if ((*cp)->compress_proto == ccp_option[0]) {
		/*
		 * Found a handler for the protocol - try to allocate
		 * a compressor or decompressor.
		 */
		error = 0;
		if (odp->transmit) {
		    s = splsoftnet();
		    if (sc->sc_xc_state != NULL)
			(*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
		    sc->sc_xcomp = *cp;
		    sc->sc_xc_state = (*cp)->comp_alloc(ccp_option, nb);
		    if (sc->sc_xc_state == NULL) {
			if (sc->sc_flags & SC_DEBUG)
			    printf("%s: comp_alloc failed\n",
				sc->sc_if.if_xname);
			error = ENOBUFS;
		    }
		    splnet();
		    sc->sc_flags &= ~SC_COMP_RUN;
		    splx(s);
		} else {
		    s = splsoftnet();
		    if (sc->sc_rc_state != NULL)
			(*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
		    sc->sc_rcomp = *cp;
		    sc->sc_rc_state = (*cp)->decomp_alloc(ccp_option, nb);
		    if (sc->sc_rc_state == NULL) {
			if (sc->sc_flags & SC_DEBUG)
			    printf("%s: decomp_alloc failed\n",
				sc->sc_if.if_xname);
			error = ENOBUFS;
		    }
		    splnet();
		    sc->sc_flags &= ~SC_DECOMP_RUN;
		    splx(s);
		}
		return (error);
	    }
	if (sc->sc_flags & SC_DEBUG)
	    printf("%s: no compressor for [%x %x %x], %x\n",
		sc->sc_if.if_xname, ccp_option[0], ccp_option[1],
		ccp_option[2], nb);
	return (EINVAL);	/* no handler found */
#endif /* PPP_COMPRESS */

    case PPPIOCGNPMODE:
    case PPPIOCSNPMODE:
	npi = (struct npioctl *) data;
	switch (npi->protocol) {
	case PPP_IP:
	    npx = NP_IP;
	    break;
	default:
	    return EINVAL;
	}
	if (cmd == PPPIOCGNPMODE) {
	    npi->mode = sc->sc_npmode[npx];
	} else {
	    if ((error = suser(p, 0)) != 0)
		return (error);
	    if (npi->mode != sc->sc_npmode[npx]) {
		s = splsoftnet();
		sc->sc_npmode[npx] = npi->mode;
		if (npi->mode != NPMODE_QUEUE) {
		    ppp_requeue(sc);
		    (*sc->sc_start)(sc);
		}
		splx(s);
	    }
	}
	break;

    case PPPIOCGIDLE:
	s = splsoftnet();
	t = time_second;
	((struct ppp_idle *)data)->xmit_idle = t - sc->sc_last_sent;
	((struct ppp_idle *)data)->recv_idle = t - sc->sc_last_recv;
	splx(s);
	break;

#if NBPFILTER > 0
    case PPPIOCSPASS:
    case PPPIOCSACTIVE:
	nbp = (struct bpf_program *) data;
	if ((unsigned) nbp->bf_len > BPF_MAXINSNS)
	    return EINVAL;
	newcodelen = nbp->bf_len * sizeof(struct bpf_insn);
	if (newcodelen != 0) {
	    newcode = malloc(newcodelen, M_DEVBUF, M_WAITOK);
	    if ((error = copyin((caddr_t)nbp->bf_insns, (caddr_t)newcode,
			       newcodelen)) != 0) {
		free(newcode, M_DEVBUF);
		return error;
	    }
	    if (!bpf_validate(newcode, nbp->bf_len)) {
		free(newcode, M_DEVBUF);
		return EINVAL;
	    }
	} else
	    newcode = 0;
	bp = (cmd == PPPIOCSPASS)? &sc->sc_pass_filt: &sc->sc_active_filt;
	oldcode = bp->bf_insns;
	s = splnet();
	bp->bf_len = nbp->bf_len;
	bp->bf_insns = newcode;
	splx(s);
	if (oldcode != 0)
	    free(oldcode, M_DEVBUF);
	break;
#endif

    default:
	return (-1);
    }
    return (0);
}

/*
 * Process an ioctl request to the ppp network interface.
 */
static int
pppsioctl(ifp, cmd, data)
    struct ifnet *ifp;
    u_long cmd;
    caddr_t data;
{
    struct ppp_softc *sc = ifp->if_softc;
    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *)data;
    struct ppp_stats *psp;
#ifdef	PPP_COMPRESS
    struct ppp_comp_stats *pcp;
#endif
    int s = splnet(), error = 0;

    switch (cmd) {
    case SIOCSIFFLAGS:
	if ((ifp->if_flags & IFF_RUNNING) == 0)
	    ifp->if_flags &= ~IFF_UP;
	break;

    case SIOCSIFADDR:
	if (ifa->ifa_addr->sa_family != AF_INET)
	    error = EAFNOSUPPORT;
	break;

    case SIOCSIFDSTADDR:
	if (ifa->ifa_addr->sa_family != AF_INET)
	    error = EAFNOSUPPORT;
	break;

    case SIOCSIFMTU:
	sc->sc_if.if_mtu = ifr->ifr_mtu;
	break;

    case SIOCADDMULTI:
    case SIOCDELMULTI:
	if (ifr == 0) {
	    error = EAFNOSUPPORT;
	    break;
	}
	switch(ifr->ifr_addr.sa_family) {
#ifdef INET
	case AF_INET:
	    break;
#endif
	default:
	    error = EAFNOSUPPORT;
	    break;
	}
	break;

    case SIOCGPPPSTATS:
	psp = &((struct ifpppstatsreq *) data)->stats;
	bzero(psp, sizeof(*psp));
	psp->p = sc->sc_stats;
#if defined(VJC) && !defined(SL_NO_STATS)
	if (sc->sc_comp) {
	    psp->vj.vjs_packets = sc->sc_comp->sls_packets;
	    psp->vj.vjs_compressed = sc->sc_comp->sls_compressed;
	    psp->vj.vjs_searches = sc->sc_comp->sls_searches;
	    psp->vj.vjs_misses = sc->sc_comp->sls_misses;
	    psp->vj.vjs_uncompressedin = sc->sc_comp->sls_uncompressedin;
	    psp->vj.vjs_compressedin = sc->sc_comp->sls_compressedin;
	    psp->vj.vjs_errorin = sc->sc_comp->sls_errorin;
	    psp->vj.vjs_tossed = sc->sc_comp->sls_tossed;
	}
#endif /* VJC */
	break;

#ifdef PPP_COMPRESS
    case SIOCGPPPCSTATS:
	pcp = &((struct ifpppcstatsreq *) data)->stats;
	bzero(pcp, sizeof(*pcp));
	if (sc->sc_xc_state != NULL)
	    (*sc->sc_xcomp->comp_stat)(sc->sc_xc_state, &pcp->c);
	if (sc->sc_rc_state != NULL)
	    (*sc->sc_rcomp->decomp_stat)(sc->sc_rc_state, &pcp->d);
	break;
#endif /* PPP_COMPRESS */

    default:
	error = ENOTTY;
    }
    splx(s);
    return (error);
}

/*
 * Queue a packet.  Start transmission if not active.
 * Packet is placed in Information field of PPP frame.
 */
int
pppoutput(ifp, m0, dst, rtp)
    struct ifnet *ifp;
    struct mbuf *m0;
    struct sockaddr *dst;
    struct rtentry *rtp;
{
    struct ppp_softc *sc = ifp->if_softc;
    int protocol, address, control;
    u_char *cp;
    int s, error;
    struct ip *ip;
    struct ifqueue *ifq;
    enum NPmode mode;
    int len;

    if (sc->sc_devp == NULL || (ifp->if_flags & IFF_RUNNING) == 0
	|| ((ifp->if_flags & IFF_UP) == 0 && dst->sa_family != AF_UNSPEC)) {
	error = ENETDOWN;	/* sort of */
	goto bad;
    }

#ifdef DIAGNOSTIC
    if (ifp->if_rdomain != rtable_l2(m0->m_pkthdr.rdomain)) {
	printf("%s: trying to send packet on wrong domain. "
	    "if %d vs. mbuf %d, AF %d\n", ifp->if_xname, ifp->if_rdomain,
	    rtable_l2(m0->m_pkthdr.rdomain), dst->sa_family);
    }
#endif

    /*
     * Compute PPP header.
     */
    m0->m_flags &= ~M_HIGHPRI;
    switch (dst->sa_family) {
#ifdef INET
    case AF_INET:
	address = PPP_ALLSTATIONS;
	control = PPP_UI;
	protocol = PPP_IP;
	mode = sc->sc_npmode[NP_IP];

	/*
	 * If this packet has the "low delay" bit set in the IP header,
	 * put it on the fastq instead.
	 */
	ip = mtod(m0, struct ip *);
	if (ip->ip_tos & IPTOS_LOWDELAY)
	    m0->m_flags |= M_HIGHPRI;
	break;
#endif
    case AF_UNSPEC:
	address = PPP_ADDRESS(dst->sa_data);
	control = PPP_CONTROL(dst->sa_data);
	protocol = PPP_PROTOCOL(dst->sa_data);
	mode = NPMODE_PASS;
	break;
    default:
	printf("%s: af%d not supported\n", ifp->if_xname, dst->sa_family);
	error = EAFNOSUPPORT;
	goto bad;
    }

    /*
     * Drop this packet, or return an error, if necessary.
     */
    if (mode == NPMODE_ERROR) {
	error = ENETDOWN;
	goto bad;
    }
    if (mode == NPMODE_DROP) {
	error = 0;
	goto bad;
    }

    /*
     * Add PPP header.  If no space in first mbuf, allocate another.
     * (This assumes M_LEADINGSPACE is always 0 for a cluster mbuf.)
     */
    M_PREPEND(m0, PPP_HDRLEN, M_DONTWAIT);
    if (m0 == 0) {
	error = ENOBUFS;
	goto bad;
    }

    cp = mtod(m0, u_char *);
    *cp++ = address;
    *cp++ = control;
    *cp++ = protocol >> 8;
    *cp++ = protocol & 0xff;

    if ((m0->m_flags & M_PKTHDR) == 0)
	    panic("mbuf packet without packet header!");
    len = m0->m_pkthdr.len;

    if (sc->sc_flags & SC_LOG_OUTPKT) {
	printf("%s output: ", ifp->if_xname);
	pppdumpm(m0);
    }

    if ((protocol & 0x8000) == 0) {
#if NBPFILTER > 0
	/*
	 * Apply the pass and active filters to the packet,
	 * but only if it is a data packet.
	 */
	*mtod(m0, u_char *) = 1;	/* indicates outbound */
	if (sc->sc_pass_filt.bf_insns != 0
	    && bpf_filter(sc->sc_pass_filt.bf_insns, (u_char *) m0,
			  len, 0) == 0) {
	    error = 0;		/* drop this packet */
	    goto bad;
	}

	/*
	 * Update the time we sent the most recent packet.
	 */
	if (sc->sc_active_filt.bf_insns == 0
	    || bpf_filter(sc->sc_active_filt.bf_insns, (u_char *) m0, len, 0))
	    sc->sc_last_sent = time_second;

	*mtod(m0, u_char *) = address;
#else
	/*
	 * Update the time we sent the most recent packet.
	 */
	sc->sc_last_sent = time_second;
#endif
    }

#if NBPFILTER > 0
    /*
     * See if bpf wants to look at the packet.
     */
    if (sc->sc_bpf)
	bpf_mtap(sc->sc_bpf, m0, BPF_DIRECTION_OUT);
#endif

    /*
     * Put the packet on the appropriate queue.
     */
    s = splsoftnet();
    if (mode == NPMODE_QUEUE) {
	/* XXX we should limit the number of packets on this queue */
	*sc->sc_npqtail = m0;
	m0->m_nextpkt = NULL;
	sc->sc_npqtail = &m0->m_nextpkt;
    } else {
	if ((m0->m_flags & M_HIGHPRI)
#ifdef ALTQ
	    && ALTQ_IS_ENABLED(&sc->sc_if.if_snd) == 0
#endif
	    ) {
	    ifq = &sc->sc_fastq;
	    if (IF_QFULL(ifq) && dst->sa_family != AF_UNSPEC) {
		IF_DROP(ifq);
		m_freem(m0);
		error = ENOBUFS;
	    }
	    else {
		IF_ENQUEUE(ifq, m0);
		error = 0;
	    }
	} else
	    IFQ_ENQUEUE(&sc->sc_if.if_snd, m0, NULL, error);
	if (error) {
	    splx(s);
	    sc->sc_if.if_oerrors++;
	    sc->sc_stats.ppp_oerrors++;
	    return (error);
	}
	(*sc->sc_start)(sc);
    }
    ifp->if_opackets++;
    ifp->if_obytes += len;

    splx(s);
    return (0);

bad:
    m_freem(m0);
    return (error);
}

/*
 * After a change in the NPmode for some NP, move packets from the
 * npqueue to the send queue or the fast queue as appropriate.
 * Should be called at splsoftnet.
 */
static void
ppp_requeue(sc)
    struct ppp_softc *sc;
{
    struct mbuf *m, **mpp;
    struct ifqueue *ifq;
    enum NPmode mode;
    int error;

    splsoftassert(IPL_SOFTNET);

    for (mpp = &sc->sc_npqueue; (m = *mpp) != NULL; ) {
	switch (PPP_PROTOCOL(mtod(m, u_char *))) {
	case PPP_IP:
	    mode = sc->sc_npmode[NP_IP];
	    break;
	default:
	    mode = NPMODE_PASS;
	}

	switch (mode) {
	case NPMODE_PASS:
	    /*
	     * This packet can now go on one of the queues to be sent.
	     */
	    *mpp = m->m_nextpkt;
	    m->m_nextpkt = NULL;
	    if ((m->m_flags & M_HIGHPRI)
#ifdef ALTQ
		&& ALTQ_IS_ENABLED(&sc->sc_if.if_snd) == 0
#endif
		) {
		ifq = &sc->sc_fastq;
		if (IF_QFULL(ifq)) {
		    IF_DROP(ifq);
		    m_freem(m);
		    error = ENOBUFS;
		}
		else {
		    IF_ENQUEUE(ifq, m);
		    error = 0;
		}
	    } else
		IFQ_ENQUEUE(&sc->sc_if.if_snd, m, NULL, error);
	    if (error) {
		sc->sc_if.if_oerrors++;
		sc->sc_stats.ppp_oerrors++;
	    }
	    break;

	case NPMODE_DROP:
	case NPMODE_ERROR:
	    *mpp = m->m_nextpkt;
	    m_freem(m);
	    break;

	case NPMODE_QUEUE:
	    mpp = &m->m_nextpkt;
	    break;
	}
    }
    sc->sc_npqtail = mpp;
}

/*
 * Transmitter has finished outputting some stuff;
 * remember to call sc->sc_start later at splsoftnet.
 */
void
ppp_restart(sc)
    struct ppp_softc *sc;
{
    int s = splnet();

    sc->sc_flags &= ~SC_TBUSY;
    schednetisr(NETISR_PPP);
    splx(s);
}

/*
 * Get a packet to send.  This procedure is intended to be called at
 * splsoftnet, since it may involve time-consuming operations such as
 * applying VJ compression, packet compression, address/control and/or
 * protocol field compression to the packet.
 */
struct mbuf *
ppp_dequeue(sc)
    struct ppp_softc *sc;
{
    struct mbuf *m, *mp;
    u_char *cp;
    int address, control, protocol;

    /*
     * Grab a packet to send: first try the fast queue, then the
     * normal queue.
     */
    IF_DEQUEUE(&sc->sc_fastq, m);
    if (m == NULL)
	IFQ_DEQUEUE(&sc->sc_if.if_snd, m);
    if (m == NULL)
      return NULL;

    ++sc->sc_stats.ppp_opackets;

    /*
     * Extract the ppp header of the new packet.
     * The ppp header will be in one mbuf.
     */
    cp = mtod(m, u_char *);
    address = PPP_ADDRESS(cp);
    control = PPP_CONTROL(cp);
    protocol = PPP_PROTOCOL(cp);

    switch (protocol) {
    case PPP_IP:
#ifdef VJC
	/*
	 * If the packet is a TCP/IP packet, see if we can compress it.
	 */
	if ((sc->sc_flags & SC_COMP_TCP) && sc->sc_comp != NULL) {
	    struct ip *ip;
	    int type;

	    mp = m;
	    ip = (struct ip *) (cp + PPP_HDRLEN);
	    if (mp->m_len <= PPP_HDRLEN) {
		mp = mp->m_next;
		if (mp == NULL)
		    break;
		ip = mtod(mp, struct ip *);
	    }
	    /* this code assumes the IP/TCP header is in one non-shared mbuf */
	    if (ip->ip_p == IPPROTO_TCP) {
		type = sl_compress_tcp(mp, ip, sc->sc_comp,
				       !(sc->sc_flags & SC_NO_TCP_CCID));
		switch (type) {
		case TYPE_UNCOMPRESSED_TCP:
		    protocol = PPP_VJC_UNCOMP;
		    break;
		case TYPE_COMPRESSED_TCP:
		    protocol = PPP_VJC_COMP;
		    cp = mtod(m, u_char *);
		    cp[0] = address;	/* header has moved */
		    cp[1] = control;
		    cp[2] = 0;
		    break;
		}
		cp[3] = protocol;	/* update protocol in PPP header */
	    }
	}
#endif	/* VJC */
	break;

#ifdef PPP_COMPRESS
    case PPP_CCP:
	ppp_ccp(sc, m, 0);
	break;
#endif	/* PPP_COMPRESS */
    }

#ifdef PPP_COMPRESS
    if (protocol != PPP_LCP && protocol != PPP_CCP
	&& sc->sc_xc_state && (sc->sc_flags & SC_COMP_RUN)) {
	struct mbuf *mcomp = NULL;
	int slen;

	slen = 0;
	for (mp = m; mp != NULL; mp = mp->m_next)
	    slen += mp->m_len;
	(*sc->sc_xcomp->compress)
	    (sc->sc_xc_state, &mcomp, m, slen,
	     (sc->sc_flags & SC_CCP_UP ? sc->sc_if.if_mtu + PPP_HDRLEN : 0));
	if (mcomp != NULL) {
	    if (sc->sc_flags & SC_CCP_UP) {
		/* Send the compressed packet instead of the original. */
		m_freem(m);
		m = mcomp;
		cp = mtod(m, u_char *);
		protocol = cp[3];
	    } else {
		/* Can't transmit compressed packets until CCP is up. */
		m_freem(mcomp);
	    }
	}
    }
#endif	/* PPP_COMPRESS */

    /*
     * Compress the address/control and protocol, if possible.
     */
    if (sc->sc_flags & SC_COMP_AC && address == PPP_ALLSTATIONS &&
	control == PPP_UI && protocol != PPP_ALLSTATIONS &&
	protocol != PPP_LCP) {
	/* can compress address/control */
	m->m_data += 2;
	m->m_len -= 2;
    }
    if (sc->sc_flags & SC_COMP_PROT && protocol < 0xFF) {
	/* can compress protocol */
	if (mtod(m, u_char *) == cp) {
	    cp[2] = cp[1];	/* move address/control up */
	    cp[1] = cp[0];
	}
	++m->m_data;
	--m->m_len;
    }

    return m;
}

/*
 * Software interrupt routine, called at splsoftnet.
 */
void
pppintr()
{
    struct ppp_softc *sc;
    int s, s2;
    struct mbuf *m;

    splsoftassert(IPL_SOFTNET);

    s = splsoftnet();	/* XXX - what's the point of this? see comment above */
    LIST_FOREACH(sc, &ppp_softc_list, sc_list) {
	if (!(sc->sc_flags & SC_TBUSY)
	    && (!IFQ_IS_EMPTY(&sc->sc_if.if_snd) ||
	    !IFQ_IS_EMPTY(&sc->sc_fastq))) {
	    s2 = splnet();
	    sc->sc_flags |= SC_TBUSY;
	    splx(s2);
	    (*sc->sc_start)(sc);
	}
	while (!IFQ_IS_EMPTY(&sc->sc_rawq)) {
	    s2 = splnet();
	    IF_DEQUEUE(&sc->sc_rawq, m);
	    splx(s2);
	    if (m == NULL)
		break;
	    ppp_inproc(sc, m);
	}
    }
    splx(s);
}

#ifdef PPP_COMPRESS
/*
 * Handle a CCP packet.  `rcvd' is 1 if the packet was received,
 * 0 if it is about to be transmitted.
 */
static void
ppp_ccp(sc, m, rcvd)
    struct ppp_softc *sc;
    struct mbuf *m;
    int rcvd;
{
    u_char *dp, *ep;
    struct mbuf *mp;
    int slen, s;

    /*
     * Get a pointer to the data after the PPP header.
     */
    if (m->m_len <= PPP_HDRLEN) {
	mp = m->m_next;
	if (mp == NULL)
	    return;
	dp = (mp != NULL)? mtod(mp, u_char *): NULL;
    } else {
	mp = m;
	dp = mtod(mp, u_char *) + PPP_HDRLEN;
    }

    ep = mtod(mp, u_char *) + mp->m_len;
    if (dp + CCP_HDRLEN > ep)
	return;
    slen = CCP_LENGTH(dp);
    if (dp + slen > ep) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("if_ppp/ccp: not enough data in mbuf (%p+%x > %p+%x)\n",
		dp, slen, mtod(mp, u_char *), mp->m_len);
	return;
    }

    switch (CCP_CODE(dp)) {
    case CCP_CONFREQ:
    case CCP_TERMREQ:
    case CCP_TERMACK:
	/* CCP must be going down - disable compression */
	if (sc->sc_flags & SC_CCP_UP) {
	    s = splnet();
	    sc->sc_flags &= ~(SC_CCP_UP | SC_COMP_RUN | SC_DECOMP_RUN);
	    splx(s);
	}
	break;

    case CCP_CONFACK:
	if (sc->sc_flags & SC_CCP_OPEN && !(sc->sc_flags & SC_CCP_UP)
	    && slen >= CCP_HDRLEN + CCP_OPT_MINLEN
	    && slen >= CCP_OPT_LENGTH(dp + CCP_HDRLEN) + CCP_HDRLEN) {
	    if (!rcvd) {
		/* we're agreeing to send compressed packets. */
		if (sc->sc_xc_state != NULL
		    && (*sc->sc_xcomp->comp_init)
			(sc->sc_xc_state, dp + CCP_HDRLEN, slen - CCP_HDRLEN,
			 sc->sc_unit, 0, sc->sc_flags & SC_DEBUG)) {
		    s = splnet();
		    sc->sc_flags |= SC_COMP_RUN;
		    splx(s);
		}
	    } else {
		/* peer is agreeing to send compressed packets. */
		if (sc->sc_rc_state != NULL
		    && (*sc->sc_rcomp->decomp_init)
			(sc->sc_rc_state, dp + CCP_HDRLEN, slen - CCP_HDRLEN,
			 sc->sc_unit, 0, sc->sc_mru,
			 sc->sc_flags & SC_DEBUG)) {
		    s = splnet();
		    sc->sc_flags |= SC_DECOMP_RUN;
		    sc->sc_flags &= ~(SC_DC_ERROR | SC_DC_FERROR);
		    splx(s);
		}
	    }
	}
	break;

    case CCP_RESETACK:
	if (sc->sc_flags & SC_CCP_UP) {
	    if (!rcvd) {
		if (sc->sc_xc_state && (sc->sc_flags & SC_COMP_RUN))
		    (*sc->sc_xcomp->comp_reset)(sc->sc_xc_state);
	    } else {
		if (sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)) {
		    (*sc->sc_rcomp->decomp_reset)(sc->sc_rc_state);
		    s = splnet();
		    sc->sc_flags &= ~SC_DC_ERROR;
		    splx(s);
		}
	    }
	}
	break;
    }
}

/*
 * CCP is down; free (de)compressor state if necessary.
 */
static void
ppp_ccp_closed(sc)
    struct ppp_softc *sc;
{
    if (sc->sc_xc_state) {
	(*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
	sc->sc_xc_state = NULL;
    }
    if (sc->sc_rc_state) {
	(*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
	sc->sc_rc_state = NULL;
    }
}
#endif /* PPP_COMPRESS */

/*
 * PPP packet input routine.
 * The caller has checked and removed the FCS and has inserted
 * the address/control bytes and the protocol high byte if they
 * were omitted.
 */
void
ppppktin(sc, m, lost)
    struct ppp_softc *sc;
    struct mbuf *m;
    int lost;
{
    int s = splnet();

    if (lost)
	m->m_flags |= M_ERRMARK;
    IF_ENQUEUE(&sc->sc_rawq, m);
    schednetisr(NETISR_PPP);
    splx(s);
}

/*
 * Process a received PPP packet, doing decompression as necessary.
 * Should be called at splsoftnet.
 */
#define COMPTYPE(proto)	((proto) == PPP_VJC_COMP? TYPE_COMPRESSED_TCP: \
			 TYPE_UNCOMPRESSED_TCP)

static void
ppp_inproc(sc, m)
    struct ppp_softc *sc;
    struct mbuf *m;
{
    struct ifnet *ifp = &sc->sc_if;
    struct ifqueue *inq;
    int s, ilen, xlen, proto, rv;
    u_char *cp, adrs, ctrl;
    struct mbuf *mp, *dmp = NULL;
    u_char *iphdr;
    u_int hlen;

    sc->sc_stats.ppp_ipackets++;

    if (sc->sc_flags & SC_LOG_INPKT) {
	ilen = 0;
	for (mp = m; mp != NULL; mp = mp->m_next)
	    ilen += mp->m_len;
	printf("%s: got %d bytes\n", ifp->if_xname, ilen);
	pppdumpm(m);
    }

    cp = mtod(m, u_char *);
    adrs = PPP_ADDRESS(cp);
    ctrl = PPP_CONTROL(cp);
    proto = PPP_PROTOCOL(cp);

    if (m->m_flags & M_ERRMARK) {
	m->m_flags &= ~M_ERRMARK;
	s = splnet();
	sc->sc_flags |= SC_VJ_RESET;
	splx(s);
    }

#ifdef PPP_COMPRESS
    /*
     * Decompress this packet if necessary, update the receiver's
     * dictionary, or take appropriate action on a CCP packet.
     */
    if (proto == PPP_COMP && sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)
	&& !(sc->sc_flags & SC_DC_ERROR) && !(sc->sc_flags & SC_DC_FERROR)) {
	/* decompress this packet */
	rv = (*sc->sc_rcomp->decompress)(sc->sc_rc_state, m, &dmp);
	if (rv == DECOMP_OK) {
	    m_freem(m);
	    if (dmp == NULL) {
		/* no error, but no decompressed packet produced */
		return;
	    }
	    m = dmp;
	    cp = mtod(m, u_char *);
	    proto = PPP_PROTOCOL(cp);

	} else {
	    /*
	     * An error has occurred in decompression.
	     * Pass the compressed packet up to pppd, which may take
	     * CCP down or issue a Reset-Req.
	     */
	    if (sc->sc_flags & SC_DEBUG)
		printf("%s: decompress failed %d\n", ifp->if_xname, rv);
	    s = splnet();
	    sc->sc_flags |= SC_VJ_RESET;
	    if (rv == DECOMP_ERROR)
		sc->sc_flags |= SC_DC_ERROR;
	    else
		sc->sc_flags |= SC_DC_FERROR;
	    splx(s);
	}

    } else {
	if (sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)) {
	    (*sc->sc_rcomp->incomp)(sc->sc_rc_state, m);
	}
	if (proto == PPP_CCP) {
	    ppp_ccp(sc, m, 1);
	}
    }
#endif

    ilen = 0;
    for (mp = m; mp != NULL; mp = mp->m_next)
	ilen += mp->m_len;

#ifdef VJC
    if (sc->sc_flags & SC_VJ_RESET) {
	/*
	 * If we've missed a packet, we must toss subsequent compressed
	 * packets which don't have an explicit connection ID.
	 */
	if (sc->sc_comp)
	    sl_uncompress_tcp(NULL, 0, TYPE_ERROR, sc->sc_comp);
	s = splnet();
	sc->sc_flags &= ~SC_VJ_RESET;
	splx(s);
    }

    /*
     * See if we have a VJ-compressed packet to uncompress.
     */
    if (proto == PPP_VJC_COMP) {
	if ((sc->sc_flags & SC_REJ_COMP_TCP) || sc->sc_comp == 0)
	    goto bad;

	xlen = sl_uncompress_tcp_core(cp + PPP_HDRLEN, m->m_len - PPP_HDRLEN,
				      ilen - PPP_HDRLEN, TYPE_COMPRESSED_TCP,
				      sc->sc_comp, &iphdr, &hlen);

	if (xlen <= 0) {
	    if (sc->sc_flags & SC_DEBUG)
		printf("%s: VJ uncompress failed on type comp\n",
		    ifp->if_xname);
	    goto bad;
	}

	/* Copy the PPP and IP headers into a new mbuf. */
	MGETHDR(mp, M_DONTWAIT, MT_DATA);
	if (mp == NULL)
	    goto bad;
	mp->m_len = 0;
	mp->m_next = NULL;
	if (hlen + PPP_HDRLEN > MHLEN) {
	    MCLGET(mp, M_DONTWAIT);
	    if (M_TRAILINGSPACE(mp) < hlen + PPP_HDRLEN) {
		m_freem(mp);
		goto bad;	/* lose if big headers and no clusters */
	    }
	}
	if (m->m_flags & M_PKTHDR)
		M_MOVE_HDR(mp, m);
	cp = mtod(mp, u_char *);
	cp[0] = adrs;
	cp[1] = ctrl;
	cp[2] = 0;
	cp[3] = PPP_IP;
	proto = PPP_IP;
	bcopy(iphdr, cp + PPP_HDRLEN, hlen);
	mp->m_len = hlen + PPP_HDRLEN;

	/*
	 * Trim the PPP and VJ headers off the old mbuf
	 * and stick the new and old mbufs together.
	 */
	m->m_data += PPP_HDRLEN + xlen;
	m->m_len -= PPP_HDRLEN + xlen;
	if (m->m_len <= M_TRAILINGSPACE(mp)) {
	    bcopy(mtod(m, u_char *), mtod(mp, u_char *) + mp->m_len, m->m_len);
	    mp->m_len += m->m_len;
	    MFREE(m, mp->m_next);
	} else
	    mp->m_next = m;
	m = mp;
	ilen += hlen - xlen;

    } else if (proto == PPP_VJC_UNCOMP) {
	if ((sc->sc_flags & SC_REJ_COMP_TCP) || sc->sc_comp == 0)
	    goto bad;

	xlen = sl_uncompress_tcp_core(cp + PPP_HDRLEN, m->m_len - PPP_HDRLEN,
				      ilen - PPP_HDRLEN, TYPE_UNCOMPRESSED_TCP,
				      sc->sc_comp, &iphdr, &hlen);

	if (xlen < 0) {
	    if (sc->sc_flags & SC_DEBUG)
		printf("%s: VJ uncompress failed on type uncomp\n",
		    ifp->if_xname);
	    goto bad;
	}

	proto = PPP_IP;
	cp[3] = PPP_IP;
    }
#endif /* VJC */

    /*
     * If the packet will fit in a header mbuf, don't waste a
     * whole cluster on it.
     */
    if (ilen <= MHLEN && M_IS_CLUSTER(m)) {
	MGETHDR(mp, M_DONTWAIT, MT_DATA);
	if (mp != NULL) {
	    m_copydata(m, 0, ilen, mtod(mp, caddr_t));
	    m_freem(m);
	    m = mp;
	    m->m_len = ilen;
	}
    }
    m->m_pkthdr.len = ilen;
    m->m_pkthdr.rcvif = ifp;

    /* mark incoming routing domain */
    m->m_pkthdr.rdomain = ifp->if_rdomain;

    if ((proto & 0x8000) == 0) {
#if NBPFILTER > 0
	/*
	 * See whether we want to pass this packet, and
	 * if it counts as link activity.
	 */
	adrs = *mtod(m, u_char *);	/* save address field */
	*mtod(m, u_char *) = 0;		/* indicate inbound */
	if (sc->sc_pass_filt.bf_insns != 0
	    && bpf_filter(sc->sc_pass_filt.bf_insns, (u_char *) m,
			  ilen, 0) == 0) {
	    /* drop this packet */
	    m_freem(m);
	    return;
	}
	if (sc->sc_active_filt.bf_insns == 0
	    || bpf_filter(sc->sc_active_filt.bf_insns, (u_char *) m, ilen, 0))
	    sc->sc_last_recv = time_second;

	*mtod(m, u_char *) = adrs;
#else
	/*
	 * Record the time that we received this packet.
	 */
	sc->sc_last_recv = time_second;
#endif
    }

#if NBPFILTER > 0
    /* See if bpf wants to look at the packet. */
    if (sc->sc_bpf)
	bpf_mtap(sc->sc_bpf, m, BPF_DIRECTION_IN);
#endif

    rv = 0;
    switch (proto) {
#ifdef INET
    case PPP_IP:
	/*
	 * IP packet - take off the ppp header and pass it up to IP.
	 */
	if ((ifp->if_flags & IFF_UP) == 0
	    || sc->sc_npmode[NP_IP] != NPMODE_PASS) {
	    /* interface is down - drop the packet. */
	    m_freem(m);
	    return;
	}
	m->m_pkthdr.len -= PPP_HDRLEN;
	m->m_data += PPP_HDRLEN;
	m->m_len -= PPP_HDRLEN;
	schednetisr(NETISR_IP);
	inq = &ipintrq;
	break;
#endif

    default:
	/*
	 * Some other protocol - place on input queue for read().
	 */
	inq = &sc->sc_inq;
	rv = 1;
	break;
    }

    /*
     * Put the packet on the appropriate input queue.
     */
    s = splnet();
    if (IF_QFULL(inq)) {
	IF_DROP(inq);
	splx(s);
	if (sc->sc_flags & SC_DEBUG)
	    printf("%s: input queue full\n", ifp->if_xname);
	ifp->if_iqdrops++;
	if (!inq->ifq_congestion)
		if_congestion(inq);
	goto bad;
    }
    IF_ENQUEUE(inq, m);
    splx(s);
    ifp->if_ipackets++;
    ifp->if_ibytes += ilen;

    if (rv)
	(*sc->sc_ctlp)(sc);

    return;

 bad:
    m_freem(m);
    sc->sc_if.if_ierrors++;
    sc->sc_stats.ppp_ierrors++;
}

#define MAX_DUMP_BYTES	128

static void
pppdumpm(m0)
    struct mbuf *m0;
{
    char buf[3*MAX_DUMP_BYTES+4];
    char *bp = buf;
    struct mbuf *m;
    static char digits[] = "0123456789abcdef";

    for (m = m0; m; m = m->m_next) {
	int l = m->m_len;
	u_char *rptr = mtod(m, u_char *);

	while (l--) {
	    if (bp > buf + sizeof(buf) - 4)
		goto done;
	    *bp++ = digits[*rptr >> 4]; /* convert byte to ascii hex */
	    *bp++ = digits[*rptr++ & 0xf];
	}

	if (m->m_next) {
	    if (bp > buf + sizeof(buf) - 3)
		goto done;
	    *bp++ = '|';
	} else
	    *bp++ = ' ';
    }
done:
    if (m)
	*bp++ = '>';
    *bp = 0;
    printf("%s\n", buf);
}

#ifdef ALTQ
/*
 * a wrapper to transmit a packet from if_start since ALTQ uses
 * if_start to send a packet.
 */
static void
ppp_ifstart(ifp)
	struct ifnet *ifp;
{
	struct ppp_softc *sc;

	sc = ifp->if_softc;
	(*sc->sc_start)(sc);
}
#endif

#endif	/* NPPP > 0 */
