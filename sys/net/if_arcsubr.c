/*	$OpenBSD: if_arcsubr.c,v 1.12 2002/09/11 05:38:47 itojun Exp $	*/
/*	$NetBSD: if_arcsubr.c,v 1.8 1996/05/07 02:40:29 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Ignatios Souvatzis
 * Copyright (c) 1982, 1989, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * from: NetBSD: if_ethersubr.c,v 1.9 1994/06/29 06:36:11 cgd Exp
 *       @(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif
#include <netinet/if_arc.h>

#ifndef	ARC_PHDSMTU
#define	ARC_PHDSMTU	1500
#endif

static struct mbuf *arc_defrag(struct ifnet *, struct mbuf *);

/*
 * RC1201 requires us to have this configurable. We have it only per 
 * machine at the moment... there is no generic "set mtu" ioctl, AFAICS.
 * Anyway, it is possible to binpatch this or set it per kernel config
 * option.
 */
#if ARC_PHDSMTU > 60480
ERROR: The arc_phdsmtu is ARC_PHDSMTU, but must not exceed 60480.
#endif
u_int16_t arc_phdsmtu = ARC_PHDSMTU;
u_int8_t  arcbroadcastaddr = 0;

#define senderr(e) { error = (e); goto bad;}
#define SIN(s) ((struct sockaddr_in *)s)

/*
 * ARCnet output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arccom structure.
 */
int
arc_output(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	struct mbuf		*m, *m1, *mcopy;
	struct rtentry		*rt;
	struct arccom		*ac;
	register struct arc_header *ah;
	int			s, error, newencoding, len;
	u_int8_t		atype, adst;
	int			tfrags, sflag, fsflag, rsflag;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) 
		return (ENETDOWN); /* m, m1 aren't initialized yet */

	error = newencoding = 0;
	ac = (struct arccom *)ifp;
	m = m0;
	mcopy = m1 = NULL;

	if ((rt = rt0)) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = rtalloc1(dst, 1)))
				rt->rt_refcnt--;
			else 
				senderr(EHOSTUNREACH);
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_REJECT)
			if (rt->rt_rmx.rmx_expire == 0 ||
			    time.tv_sec < rt->rt_rmx.rmx_expire)
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}

	/*
	 * if the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:

		/*
		 * For now, use the simple IP addr -> ARCnet addr mapping
		 */
		if (m->m_flags & M_BCAST) 
			adst = arcbroadcastaddr; /* ARCnet broadcast address */
		else
			adst = ntohl(SIN(dst)->sin_addr.s_addr) & 0xFF;

		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		if (ifp->if_flags & IFF_LINK0) {
			atype = ARCTYPE_IP;
			newencoding = 1;
		} else {
			atype = ARCTYPE_IP_OLD;
			newencoding = 0;
		}
		break;
#endif

	case AF_UNSPEC:
		ah = (struct arc_header *)dst->sa_data;
 		adst = ah->arc_dhost;
		atype = ah->arc_type;
		break;

	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	if (mcopy)
		(void) looutput(ifp, mcopy, dst, rt);

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 * 
	 * For ARCnet, this is just symbolic. The header changes
	 * form and position on its way into the hardware and out of
	 * the wire.  At this point, it contains source, destination and
	 * packet type.
	 */
	if (newencoding) {
		++ac->ac_seqid; /* make the seqid unique */

		tfrags = (m->m_pkthdr.len + 503) / 504;
		fsflag = 2 * tfrags - 3;
		sflag = 0;
		rsflag = fsflag;

		while (sflag < fsflag) {
			/* we CAN'T have short packets here */
			m1 = m_split(m, 504, M_DONTWAIT);
			if (m1 == 0)
				senderr(ENOBUFS);

			M_PREPEND(m, ARC_HDRNEWLEN, M_DONTWAIT);
			if (m == 0)
				senderr(ENOBUFS);
			ah = mtod(m, struct arc_header *);
			ah->arc_type = atype;
			ah->arc_dhost = adst;
			ah->arc_shost = ac->ac_anaddr;
			ah->arc_flag = rsflag;
			ah->arc_seqid = ac->ac_seqid;

			len = m->m_pkthdr.len;
			s = splimp();
			/*
			 * Queue message on interface, and start output if 
			 * interface not yet active.
			 */
			IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
			if (error) {
				/* mbuf is already freed */
				splx(s);
				return (error);
			}
			ifp->if_obytes += len;
			if ((ifp->if_flags & IFF_OACTIVE) == 0)
				(*ifp->if_start)(ifp);
			splx(s);
	
			m = m1;
			sflag += 2;
			rsflag = sflag;
		}
		m1 = NULL;

		M_PREPEND(m, ARC_HDRNEWLEN, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		ah = mtod(m, struct arc_header *);
		ah->arc_type = atype;
		ah->arc_flag = sflag;
		ah->arc_seqid = ac->ac_seqid;

		/* here we can have small, especially forbidden packets */

		if ((m->m_pkthdr.len >= ARC_MIN_FORBID_LEN + 2) &&
		    (m->m_pkthdr.len <= ARC_MAX_FORBID_LEN + 2)) {
			M_PREPEND(m, 4, M_DONTWAIT);
			if (m == 0)
				senderr(ENOBUFS);
			m = m_pullup(m, ARC_HDRNEWLEN);
			if (m == NULL)
				senderr(ENOBUFS);
			ah = mtod(m, struct arc_header *);
			ah->arc_type = atype;
			ah->arc_flag = 0xFF;
			ah->arc_seqid = 0xFFFF;
		}

		ah->arc_dhost = adst;
		ah->arc_shost = ac->ac_anaddr;
	} else {
		M_PREPEND(m, ARC_HDRLEN, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		ah = mtod(m, struct arc_header *);
		ah->arc_type = atype;
		ah->arc_dhost = adst;
		ah->arc_shost = ac->ac_anaddr;
	}

	len = m->m_pkthdr.len;
	s = splimp();
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
	if (error) {
		/* mbuf is already freed */
		splx(s);
		return (error);
	}
	ifp->if_obytes += len;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);

	return (error);

bad:
	if (m1)
		m_freem(m1);
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Defragmenter. Returns mbuf if last packet found, else 
 * NULL. frees imcoming mbuf as necessary.
 */

__inline struct mbuf *
arc_defrag(ifp, m)
	struct ifnet *ifp;
	struct mbuf *m;
{
	struct arc_header *ah, *ah1;
	struct arccom *ac;
	struct ac_frag *af;
	struct mbuf *m1;
	char *s;
	int newflen;
	u_char src,dst,typ;
	
	ac = (struct arccom *)ifp;

	m = m_pullup(m, ARC_HDRNEWLEN);
	if (m == NULL) {
		++ifp->if_ierrors;
		return NULL;
	}

	ah = mtod(m, struct arc_header *);
	typ = ah->arc_type;

	if (!arc_isphds(typ))
		return m;

	src = ah->arc_shost;
	dst = ah->arc_dhost;

	if (ah->arc_flag == 0xff) {
		m_adj(m, 4);

		m = m_pullup(m, ARC_HDRNEWLEN);
		if (m == NULL) {
			++ifp->if_ierrors;
			return NULL;
		}

		ah = mtod(m, struct arc_header *);
		ah->arc_shost = src;
		ah->arc_dhost = dst;
		ah->arc_type = typ;
	}

	af = &ac->ac_fragtab[src];
	m1 = af->af_packet;
	s = "debug code error";

	if (ah->arc_flag & 1) {
		/* 
		 * first fragment. We always initialize, which is
		 * about the right thing to do, as we only want to
		 * accept one fragmented packet per src at a time.
		 */
		if (m1 != NULL)
			m_freem(m1);

		af->af_packet = m;
		m1 = m;
		af->af_maxflag = ah->arc_flag;
		af->af_lastseen = 0;
		af->af_seqid = ah->arc_seqid;

		return NULL;
		/* notreached */
	} else {
		/* check for unfragmented packet */
		if (ah->arc_flag == 0)
			return m;

		/* do we have a first packet from that src? */
		if (m1 == NULL) {
			s = "no first frag";
			goto outofseq;
		}

		ah1 = mtod(m1, struct arc_header *);

		if (ah->arc_seqid != ah1->arc_seqid) {
			s = "seqid differs";
			goto outofseq;
		}

		if (ah->arc_type != ah1->arc_type) {
			s = "type differs";
			goto outofseq;
		}

		if (ah->arc_dhost != ah1->arc_dhost) {
			s = "dest host differs";
			goto outofseq;
		}

		/* typ, seqid and dst are ok here. */

		if (ah->arc_flag == af->af_lastseen) {
			m_freem(m);
			return NULL;
		}

		if (ah->arc_flag == af->af_lastseen + 2) {
			/* ok, this is next fragment */
			af->af_lastseen = ah->arc_flag;
			m_adj(m,ARC_HDRNEWLEN);

			/* 
			 * m_cat might free the first mbuf (with pkthdr)
			 * in 2nd chain; therefore:
			 */

			newflen = m->m_pkthdr.len;	

			m_cat(m1,m);

			m1->m_pkthdr.len += newflen;

			/* is it the last one? */
			if (af->af_lastseen > af->af_maxflag) {
				af->af_packet = NULL;
				return (m1);
			} else
				return NULL;
		}
		s = "other reason";
		/* if all else fails, it is out of sequence, too */
	}
outofseq:
	if (m1) {
		m_freem(m1);
		af->af_packet = NULL;
	}

	if (m) 
		m_freem(m);

	log(LOG_INFO,"%s: got out of seq. packet: %s\n",
	    ifp->if_xname, s);

	return NULL;
}

/*
 * return 1 if Packet Header Definition Standard, else 0.
 * For now: old IP, old ARP aren't obviously. Lacking correct information,
 * we guess that besides new IP and new ARP also IPX and APPLETALK are PHDS.
 * (Apple and Novell corporations were involved, among others, in PHDS work).
 * Easiest is to assume that everybody else uses that, too.
 */
int
arc_isphds(type)
	u_int8_t type;
{
	return ((type != ARCTYPE_IP_OLD && 
		 type != ARCTYPE_ARP_OLD));
}

/*
 * Process a received Arcnet packet;
 * the packet is in the mbuf chain m with
 * the ARCnet header.
 */
void
arc_input(ifp, m)
	struct ifnet *ifp;
	struct mbuf *m;
{
	register struct arc_header *ah;
	register struct ifqueue *inq;
	u_int8_t atype;
	int s;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	/* possibly defragment: */
	m = arc_defrag(ifp, m);
	if (m == NULL) 
		return;

	ah = mtod(m, struct arc_header *);

	ifp->if_ibytes += m->m_pkthdr.len;

	if (arcbroadcastaddr == ah->arc_dhost) {
		m->m_flags |= M_BCAST;
		ifp->if_imcasts++;
	}

	atype = ah->arc_type;
	switch (atype) {
#ifdef INET
	case ARCTYPE_IP:
		m_adj(m, ARC_HDRNEWLEN);
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ARCTYPE_IP_OLD:
		m_adj(m, ARC_HDRLEN);
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;
#endif
	default:
		m_freem(m);
		return;
	}

	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}

/*
 * Convert Arcnet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
char *
arc_sprintf(ap)
	register u_int8_t *ap;
{
	static char arcbuf[3];
	register char *cp = arcbuf;

	*cp++ = digits[*ap >> 4];
	*cp++ = digits[*ap++ & 0xf];
	*cp   = 0;
	return (arcbuf);
}

/*
 * Perform common duties while attaching to interface list
 */
void
arc_ifattach(ifp)
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;
	register struct sockaddr_dl *sdl;
	register struct arccom *ac;

	ifp->if_type = IFT_ARCNET;
	ifp->if_addrlen = 1;
	ifp->if_hdrlen = ARC_HDRLEN;
	if (ifp->if_flags & IFF_LINK0 && arc_phdsmtu > 60480)
		log(LOG_ERR,
		    "%s: arc_phdsmtu is %d, but must not exceed 60480",
		    ifp->if_xname, arc_phdsmtu);

	ifp->if_mtu = (ifp->if_flags & IFF_LINK0 ? arc_phdsmtu : ARCMTU);
	ac = (struct arccom *)ifp;
	ac->ac_seqid = (time.tv_sec) & 0xFFFF; /* try to make seqid unique */
	if (ac->ac_anaddr == 0) {
		/* XXX this message isn't entirely clear, to me -- cgd */
		log(LOG_ERR,"%s: link address 0 reserved for broadcasts.  Please change it and ifconfig %s down up\n",
		   ifp->if_xname, ifp->if_xname); 
	}
	if_alloc_sadl(ifp);
	bcopy((caddr_t)&((struct arccom *)ifp)->ac_anaddr,
	      LLADDR(ifp->if_sadl), ifp->if_addrlen);
}
