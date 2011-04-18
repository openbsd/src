/*	$OpenBSD: uipc_mbuf.c,v 1.156 2011/04/18 19:23:46 art Exp $	*/
/*	$NetBSD: uipc_mbuf.c,v 1.15.4.1 1996/06/13 17:11:44 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
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
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 */

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 * 
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pool.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>

#include <machine/cpu.h>

#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#endif

struct	mbstat mbstat;		/* mbuf stats */
struct	pool mbpool;		/* mbuf pool */

/* mbuf cluster pools */
u_int	mclsizes[] = {
	MCLBYTES,	/* must be at slot 0 */
	4 * 1024,
	8 * 1024,
	9 * 1024,
	12 * 1024,
	16 * 1024,
	64 * 1024
};
static	char mclnames[MCLPOOLS][8];
struct	pool mclpools[MCLPOOLS];

int	m_clpool(u_int);

int max_linkhdr;		/* largest link-level header */
int max_protohdr;		/* largest protocol header */
int max_hdr;			/* largest link+protocol header */
int max_datalen;		/* MHLEN - max_hdr */

struct timeout m_cltick_tmo;
int	m_clticks;
void	m_cltick(void *);

void	m_extfree(struct mbuf *);
struct mbuf *m_copym0(struct mbuf *, int, int, int, int);
void	nmbclust_update(void);


const char *mclpool_warnmsg =
    "WARNING: mclpools limit reached; increase kern.maxclusters";

/*
 * Initialize the mbuf allocator.
 */
void
mbinit(void)
{
	int i;

	pool_init(&mbpool, MSIZE, 0, 0, 0, "mbpl", NULL);
	pool_set_constraints(&mbpool, &kp_dma);
	pool_setlowat(&mbpool, mblowat);

	for (i = 0; i < nitems(mclsizes); i++) {
		snprintf(mclnames[i], sizeof(mclnames[0]), "mcl%dk",
		    mclsizes[i] >> 10);
		pool_init(&mclpools[i], mclsizes[i], 0, 0, 0,
		    mclnames[i], NULL);
		pool_set_constraints(&mclpools[i], &kp_dma); 
		pool_setlowat(&mclpools[i], mcllowat);
	}

	nmbclust_update();

	timeout_set(&m_cltick_tmo, m_cltick, NULL);
	m_cltick(NULL);
}

void
nmbclust_update(void)
{
	int i;
	/*
	 * Set the hard limit on the mclpools to the number of
	 * mbuf clusters the kernel is to support.  Log the limit
	 * reached message max once a minute.
	 */
	for (i = 0; i < nitems(mclsizes); i++) {
		(void)pool_sethardlimit(&mclpools[i], nmbclust,
		    mclpool_warnmsg, 60);
		/*
		 * XXX this needs to be reconsidered.
		 * Setting the high water mark to nmbclust is too high
		 * but we need to have enough spare buffers around so that
		 * allocations in interrupt context don't fail or mclgeti()
		 * drivers may end up with empty rings.
		 */
		pool_sethiwat(&mclpools[i], nmbclust);
	}
	pool_sethiwat(&mbpool, nmbclust);
}

void
m_reclaim(void *arg, int flags)
{
	struct domain *dp;
	struct protosw *pr;
	int s = splnet();

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain)
				(*pr->pr_drain)();
	mbstat.m_drain++;
	splx(s);
}

/*
 * Space allocation routines.
 */
struct mbuf *
m_get(int nowait, int type)
{
	struct mbuf *m;
	int s;

	s = splnet();
	m = pool_get(&mbpool, nowait == M_WAIT ? PR_WAITOK : PR_NOWAIT);
	if (m)
		mbstat.m_mtypes[type]++;
	splx(s);
	if (m) {
		m->m_type = type;
		m->m_next = (struct mbuf *)NULL;
		m->m_nextpkt = (struct mbuf *)NULL;
		m->m_data = m->m_dat;
		m->m_flags = 0;
	}
	return (m);
}

/*
 * ATTN: When changing anything here check m_inithdr() and m_defrag() those
 * may need to change as well.
 */
struct mbuf *
m_gethdr(int nowait, int type)
{
	struct mbuf *m;
	int s;

	s = splnet();
	m = pool_get(&mbpool, nowait == M_WAIT ? PR_WAITOK : PR_NOWAIT);
	if (m)
		mbstat.m_mtypes[type]++;
	splx(s);
	if (m) {
		m->m_type = type;

		/* keep in sync with m_inithdr */
		m->m_next = (struct mbuf *)NULL;
		m->m_nextpkt = (struct mbuf *)NULL;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
		bzero(&m->m_pkthdr, sizeof(m->m_pkthdr));
	}
	return (m);
}

struct mbuf *
m_inithdr(struct mbuf *m)
{
	/* keep in sync with m_gethdr */
	m->m_next = (struct mbuf *)NULL;
	m->m_nextpkt = (struct mbuf *)NULL;
	m->m_data = m->m_pktdat;
	m->m_flags = M_PKTHDR;
	bzero(&m->m_pkthdr, sizeof(m->m_pkthdr));

	return (m);
}

struct mbuf *
m_getclr(int nowait, int type)
{
	struct mbuf *m;

	MGET(m, nowait, type);
	if (m == NULL)
		return (NULL);
	memset(mtod(m, caddr_t), 0, MLEN);
	return (m);
}

int
m_clpool(u_int pktlen)
{
	int pi;

	for (pi = 0; pi < MCLPOOLS; pi++) {
                if (pktlen <= mclsizes[pi])
			return (pi);
	}

	return (-1);
}

void
m_clinitifp(struct ifnet *ifp)
{
	struct mclpool *mclp = ifp->if_data.ifi_mclpool;
	int i;

	/* Initialize high water marks for use of cluster pools */
	for (i = 0; i < MCLPOOLS; i++) {
		mclp = &ifp->if_data.ifi_mclpool[i];

		if (mclp->mcl_lwm == 0)
			mclp->mcl_lwm = 2;
		if (mclp->mcl_hwm == 0)
			mclp->mcl_hwm = 32768;

		mclp->mcl_cwm = MAX(4, mclp->mcl_lwm);
	}
}

void
m_clsetwms(struct ifnet *ifp, u_int pktlen, u_int lwm, u_int hwm)
{
	int pi;

	pi = m_clpool(pktlen);
	if (pi == -1)
		return;

	ifp->if_data.ifi_mclpool[pi].mcl_lwm = lwm;
	ifp->if_data.ifi_mclpool[pi].mcl_hwm = hwm;
}

/*
 * Record when the last timeout has been run.  If the delta is
 * too high, m_cldrop() will notice and decrease the interface
 * high water marks.
 */
void
m_cltick(void *arg)
{
	extern int ticks;

	m_clticks = ticks;
	timeout_add(&m_cltick_tmo, 1);
}

int m_livelock;
u_int mcllivelocks;

int
m_cldrop(struct ifnet *ifp, int pi)
{
	static int liveticks;
	struct mclpool *mclp;
	extern int ticks;
	int i;

	if (ticks - m_clticks > 1) {
		struct ifnet *aifp;

		/*
		 * Timeout did not run, so we are in some kind of livelock.
		 * Decrease the cluster allocation high water marks on all
		 * interfaces and prevent them from growth for the very near
		 * future.
		 */
		m_livelock = 1;
		mcllivelocks++;
		m_clticks = liveticks = ticks;
		TAILQ_FOREACH(aifp, &ifnet, if_list) {
			mclp = aifp->if_data.ifi_mclpool;
			for (i = 0; i < MCLPOOLS; i++) {
				int diff = max(mclp[i].mcl_cwm / 8, 2);
				mclp[i].mcl_cwm = max(mclp[i].mcl_lwm,
				    mclp[i].mcl_cwm - diff);
			}
		}
	} else if (m_livelock && (ticks - liveticks) > 4)
		m_livelock = 0;	/* Let the high water marks grow again */

	mclp = &ifp->if_data.ifi_mclpool[pi];
	if (m_livelock == 0 && ISSET(ifp->if_flags, IFF_RUNNING) &&
	    mclp->mcl_alive <= 4 && mclp->mcl_cwm < mclp->mcl_hwm &&
	    mclp->mcl_grown < ticks) {
		/* About to run out, so increase the current watermark */
		mclp->mcl_cwm++;
		mclp->mcl_grown = ticks;
	} else if (mclp->mcl_alive >= mclp->mcl_cwm)
		return (1);		/* No more packets given */

	return (0);
}

void
m_clcount(struct ifnet *ifp, int pi)
{
	ifp->if_data.ifi_mclpool[pi].mcl_alive++;
}

void
m_cluncount(struct mbuf *m, int all)
{
	struct mbuf_ext *me;

	do {
		me = &m->m_ext;
		if (((m->m_flags & (M_EXT|M_CLUSTER)) != (M_EXT|M_CLUSTER)) ||
		    (me->ext_ifp == NULL))
			continue;

		me->ext_ifp->if_data.ifi_mclpool[me->ext_backend].mcl_alive--;
		me->ext_ifp = NULL;
	} while (all && (m = m->m_next));
}

struct mbuf *
m_clget(struct mbuf *m, int how, struct ifnet *ifp, u_int pktlen)
{
	struct mbuf *m0 = NULL;
	int pi;
	int s;

	pi = m_clpool(pktlen);
#ifdef DIAGNOSTIC
	if (pi == -1)
		panic("m_clget: request for %u byte cluster", pktlen);
#endif

	s = splnet();

	if (ifp != NULL && m_cldrop(ifp, pi)) {
		splx(s);
		return (NULL);
	}

	if (m == NULL) {
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL) {
			splx(s);
			return (NULL);
		}
		m = m0;
	}			
	m->m_ext.ext_buf = pool_get(&mclpools[pi],
	    how == M_WAIT ? PR_WAITOK : PR_NOWAIT);
	if (!m->m_ext.ext_buf) {
		if (m0)
			m_freem(m0);
		splx(s);
		return (NULL);
	}
	if (ifp != NULL)
		m_clcount(ifp, pi);
	splx(s);

	m->m_data = m->m_ext.ext_buf;
	m->m_flags |= M_EXT|M_CLUSTER;
	m->m_ext.ext_size = mclpools[pi].pr_size;
	m->m_ext.ext_free = NULL;
	m->m_ext.ext_arg = NULL;
	m->m_ext.ext_backend = pi;
	m->m_ext.ext_ifp = ifp;
	MCLINITREFERENCE(m);
	return (m);
}

struct mbuf *
m_free_unlocked(struct mbuf *m)
{
	struct mbuf *n;

	mbstat.m_mtypes[m->m_type]--;
	if (m->m_flags & M_PKTHDR)
		m_tag_delete_chain(m);
	if (m->m_flags & M_EXT)
		m_extfree(m);
	n = m->m_next;
	pool_put(&mbpool, m);

	return (n);
}

struct mbuf *
m_free(struct mbuf *m)
{
	struct mbuf *n;
	int s;

	s = splnet();
	n = m_free_unlocked(m);
	splx(s);

	return (n);
}

void
m_extfree(struct mbuf *m)
{
	if (MCLISREFERENCED(m)) {
		m->m_ext.ext_nextref->m_ext.ext_prevref =
		    m->m_ext.ext_prevref;
		m->m_ext.ext_prevref->m_ext.ext_nextref =
		    m->m_ext.ext_nextref;
	} else if (m->m_flags & M_CLUSTER) {
		m_cluncount(m, 0);
		pool_put(&mclpools[m->m_ext.ext_backend],
		    m->m_ext.ext_buf);
	} else if (m->m_ext.ext_free)
		(*(m->m_ext.ext_free))(m->m_ext.ext_buf,
		    m->m_ext.ext_size, m->m_ext.ext_arg);
	else
		panic("unknown type of extension buffer");
	m->m_ext.ext_size = 0;
	m->m_flags &= ~(M_EXT|M_CLUSTER);
}

void
m_freem(struct mbuf *m)
{
	struct mbuf *n;
	int s;

	if (m == NULL)
		return;
	s = splnet();
	do {
		n = m_free_unlocked(m);
	} while ((m = n) != NULL);
	splx(s);
}

/*
 * mbuf chain defragmenter. This function uses some evil tricks to defragment
 * an mbuf chain into a single buffer without changing the mbuf pointer.
 * This needs to know a lot of the mbuf internals to make this work.
 */
int
m_defrag(struct mbuf *m, int how)
{
	struct mbuf *m0;

	if (m->m_next == NULL)
		return 0;

#ifdef DIAGNOSTIC
	if (!(m->m_flags & M_PKTHDR))
		panic("m_defrag: no packet hdr or not a chain");
#endif

	if ((m0 = m_gethdr(how, m->m_type)) == NULL)
		return -1;
	if (m->m_pkthdr.len > MHLEN) {
		MCLGETI(m0, how, NULL, m->m_pkthdr.len);
		if (!(m0->m_flags & M_EXT)) {
			m_free(m0);
			return -1;
		}
	}
	m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
	m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;

	/* free chain behind and possible ext buf on the first mbuf */
	m_freem(m->m_next);
	m->m_next = NULL;

	if (m->m_flags & M_EXT) {
		int s = splnet();
		m_extfree(m);
		splx(s);
	}

	/*
	 * Bounce copy mbuf over to the original mbuf and set everything up.
	 * This needs to reset or clear all pointers that may go into the
	 * original mbuf chain.
	 */
	if (m0->m_flags & M_EXT) {
		bcopy(&m0->m_ext, &m->m_ext, sizeof(struct mbuf_ext));
		MCLINITREFERENCE(m);
		m->m_flags |= M_EXT|M_CLUSTER;
		m->m_data = m->m_ext.ext_buf;
	} else {
		m->m_data = m->m_pktdat;
		bcopy(m0->m_data, m->m_data, m0->m_len);
	}
	m->m_pkthdr.len = m->m_len = m0->m_len;
	m->m_pkthdr.pf.hdr = NULL;	/* altq will cope */

	m0->m_flags &= ~(M_EXT|M_CLUSTER);	/* cluster is gone */
	m_free(m0);

	return 0;
}

/*
 * Mbuffer utility routines.
 */

/*
 * Ensure len bytes of contiguous space at the beginning of the mbuf chain
 */
struct mbuf *
m_prepend(struct mbuf *m, int len, int how)
{
	struct mbuf *mn;

	if (len > MHLEN)
		panic("mbuf prepend length too big");

	if (M_LEADINGSPACE(m) >= len) {
		m->m_data -= len;
		m->m_len += len;
	} else {
		MGET(mn, how, m->m_type);
		if (mn == NULL) {
			m_freem(m);
			return (NULL);
		}
		if (m->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(mn, m);
		mn->m_next = m;
		m = mn;
		MH_ALIGN(m, len);
		m->m_len = len;
	}
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len += len;
	return (m);
}

/*
 * Make a copy of an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes.  If len is M_COPYALL, copy to end of mbuf.
 * The wait parameter is a choice of M_WAIT/M_DONTWAIT from caller.
 */
struct mbuf *
m_copym(struct mbuf *m, int off, int len, int wait)
{
	return m_copym0(m, off, len, wait, 0);	/* shallow copy on M_EXT */
}

/*
 * m_copym2() is like m_copym(), except it COPIES cluster mbufs, instead
 * of merely bumping the reference count.
 */
struct mbuf *
m_copym2(struct mbuf *m, int off, int len, int wait)
{
	return m_copym0(m, off, len, wait, 1);	/* deep copy */
}

struct mbuf *
m_copym0(struct mbuf *m0, int off, int len, int wait, int deep)
{
	struct mbuf *m, *n, **np;
	struct mbuf *top;
	int copyhdr = 0;

	if (off < 0 || len < 0)
		panic("m_copym0: off %d, len %d", off, len);
	if (off == 0 && m0->m_flags & M_PKTHDR)
		copyhdr = 1;
	if ((m = m_getptr(m0, off, &off)) == NULL)
		panic("m_copym0: short mbuf chain");
	np = &top;
	top = NULL;
	while (len > 0) {
		if (m == NULL) {
			if (len != M_COPYALL)
				panic("m_copym0: m == NULL and not COPYALL");
			break;
		}
		MGET(n, wait, m->m_type);
		*np = n;
		if (n == NULL)
			goto nospace;
		if (copyhdr) {
			if (m_dup_pkthdr(n, m0, wait))
				goto nospace;
			if (len != M_COPYALL)
				n->m_pkthdr.len = len;
			copyhdr = 0;
		}
		n->m_len = min(len, m->m_len - off);
		if (m->m_flags & M_EXT) {
			if (!deep) {
				n->m_data = m->m_data + off;
				n->m_ext = m->m_ext;
				MCLADDREFERENCE(m, n);
			} else {
				/*
				 * we are unsure about the way m was allocated.
				 * copy into multiple MCLBYTES cluster mbufs.
				 */
				MCLGET(n, wait);
				n->m_len = 0;
				n->m_len = M_TRAILINGSPACE(n);
				n->m_len = min(n->m_len, len);
				n->m_len = min(n->m_len, m->m_len - off);
				memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + off,
				    (unsigned)n->m_len);
			}
		} else
			memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + off,
			    (unsigned)n->m_len);
		if (len != M_COPYALL)
			len -= n->m_len;
		off += n->m_len;
#ifdef DIAGNOSTIC
		if (off > m->m_len)
			panic("m_copym0 overrun");
#endif
		if (off == m->m_len) {
			m = m->m_next;
			off = 0;
		}
		np = &n->m_next;
	}
	return (top);
nospace:
	m_freem(top);
	return (NULL);
}

/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void
m_copydata(struct mbuf *m, int off, int len, caddr_t cp)
{
	unsigned count;

	if (off < 0)
		panic("m_copydata: off %d < 0", off);
	if (len < 0)
		panic("m_copydata: len %d < 0", len);
	if ((m = m_getptr(m, off, &off)) == NULL)
		panic("m_copydata: short mbuf chain");
	while (len > 0) {
		if (m == NULL)
			panic("m_copydata: null mbuf");
		count = min(m->m_len - off, len);
		bcopy(mtod(m, caddr_t) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}

/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary. The mbuf needs to be properly initialized
 * including the setting of m_len.
 */
int
m_copyback(struct mbuf *m0, int off, int len, const void *_cp, int wait)
{
	int mlen, totlen = 0;
	struct mbuf *m = m0, *n;
	caddr_t cp = (caddr_t)_cp;
	int error = 0;

	if (m0 == NULL)
		return (0);
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == NULL) {
			if ((n = m_get(wait, m->m_type)) == NULL) {
				error = ENOBUFS;
				goto out;
			}

			if (off + len > MLEN) {
				MCLGETI(n, wait, NULL, off + len);
				if (!(n->m_flags & M_EXT)) {
					m_free(n);
					error = ENOBUFS;
					goto out;
				}
			}
			bzero(mtod(n, caddr_t), off);
			n->m_len = len + off;
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		/* extend last packet to be filled fully */
		if (m->m_next == NULL && (len > m->m_len - off))
			m->m_len += min(len - (m->m_len - off),
			    M_TRAILINGSPACE(m));
		mlen = min(m->m_len - off, len);
		bcopy(cp, mtod(m, caddr_t) + off, (size_t)mlen);
		cp += mlen;
		len -= mlen;
		totlen += mlen + off;
		if (len == 0)
			break;
		off = 0;

		if (m->m_next == NULL) {
			if ((n = m_get(wait, m->m_type)) == NULL) {
				error = ENOBUFS;
				goto out;
			}

			if (len > MLEN) {
				MCLGETI(n, wait, NULL, len);
				if (!(n->m_flags & M_EXT)) {
					m_free(n);
					error = ENOBUFS;
					goto out;
				}
			}
			n->m_len = len;
			m->m_next = n;
		}
		m = m->m_next;
	}
out:
	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;

	return (error);
}

/*
 * Concatenate mbuf chain n to m.
 * n might be copied into m (when n->m_len is small), therefore data portion of
 * n could be copied into an mbuf of different mbuf type.
 * Therefore both chains should be of the same type (e.g. MT_DATA).
 * Any m_pkthdr is not updated.
 */
void
m_cat(struct mbuf *m, struct mbuf *n)
{
	while (m->m_next)
		m = m->m_next;
	while (n) {
		if (M_READONLY(m) || n->m_len > M_TRAILINGSPACE(m)) {
			/* just join the two chains */
			m->m_next = n;
			return;
		}
		/* splat the data from one into the other */
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		    (u_int)n->m_len);
		m->m_len += n->m_len;
		n = m_free(n);
	}
}

void
m_adj(struct mbuf *mp, int req_len)
{
	int len = req_len;
	struct mbuf *m;
	int count;

	if ((m = mp) == NULL)
		return;
	if (len >= 0) {
		/*
		 * Trim from head.
		 */
		while (m != NULL && len > 0) {
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_data += len;
				len = 0;
			}
		}
		m = mp;
		if (mp->m_flags & M_PKTHDR)
			m->m_pkthdr.len -= (req_len - len);
	} else {
		/*
		 * Trim from tail.  Scan the mbuf chain,
		 * calculating its length and finding the last mbuf.
		 * If the adjustment only affects this mbuf, then just
		 * adjust and return.  Otherwise, rescan and truncate
		 * after the remaining size.
		 */
		len = -len;
		count = 0;
		for (;;) {
			count += m->m_len;
			if (m->m_next == NULL)
				break;
			m = m->m_next;
		}
		if (m->m_len >= len) {
			m->m_len -= len;
			if (mp->m_flags & M_PKTHDR)
				mp->m_pkthdr.len -= len;
			return;
		}
		count -= len;
		if (count < 0)
			count = 0;
		/*
		 * Correct length for chain is "count".
		 * Find the mbuf with last data, adjust its length,
		 * and toss data from remaining mbufs on chain.
		 */
		m = mp;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len = count;
		for (; m; m = m->m_next) {
			if (m->m_len >= count) {
				m->m_len = count;
				break;
			}
			count -= m->m_len;
		}
		while ((m = m->m_next) != NULL)
			m->m_len = 0;
	}
}

/*
 * Rearange an mbuf chain so that len bytes are contiguous
 * and in the data area of an mbuf (so that mtod and dtom
 * will work for a structure of size len).  Returns the resulting
 * mbuf chain on success, frees it and returns null on failure.
 * If there is room, it will add up to max_protohdr-len extra bytes to the
 * contiguous region in an attempt to avoid being called next time.
 */
struct mbuf *
m_pullup(struct mbuf *n, int len)
{
	struct mbuf *m;
	int count;
	int space;
	int s;

	/*
	 * If first mbuf has no cluster, and has room for len bytes
	 * without shifting current data, pullup into it,
	 * otherwise allocate a new mbuf to prepend to the chain.
	 */
	if ((n->m_flags & M_EXT) == 0 &&
	    n->m_data + len < &n->m_dat[MLEN] && n->m_next) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else {
		if (len > MHLEN)
			goto bad;
		MGET(m, M_DONTWAIT, n->m_type);
		if (m == NULL)
			goto bad;
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(m, n);
	}
	space = &m->m_dat[MLEN] - (m->m_data + m->m_len);
	s = splnet();
	do {
		count = min(min(max(len, max_protohdr), space), n->m_len);
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		    (unsigned)count);
		len -= count;
		m->m_len += count;
		n->m_len -= count;
		space -= count;
		if (n->m_len)
			n->m_data += count;
		else
			n = m_free_unlocked(n);
	} while (len > 0 && n);
	if (len > 0) {
		(void)m_free_unlocked(m);
		splx(s);
		goto bad;
	}
	splx(s);
	m->m_next = n;
	return (m);
bad:
	m_freem(n);
	return (NULL);
}

/*
 * m_pullup2() works like m_pullup, save that len can be <= MCLBYTES.
 * m_pullup2() only works on values of len such that MHLEN < len <= MCLBYTES,
 * it calls m_pullup() for values <= MHLEN.  It also only coagulates the
 * requested number of bytes.  (For those of us who expect unwieldy option
 * headers.
 *
 * KEBE SAYS:  Remember that dtom() calls with data in clusters does not work!
 */
struct mbuf *   
m_pullup2(struct mbuf *n, int len)       
{
	struct mbuf *m;
	int count;

	if (len <= MHLEN)
		return m_pullup(n, len);
	if ((n->m_flags & M_EXT) != 0 &&
	    n->m_data + len < &n->m_data[MCLBYTES] && n->m_next) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else {
		if (len > MCLBYTES)
			goto bad;
		MGET(m, M_DONTWAIT, n->m_type);
		if (m == NULL)
			goto bad;
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			goto bad;
		}
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR) {
			/* Too many adverse side effects. */
			/* M_MOVE_PKTHDR(m, n); */
			m->m_flags = (n->m_flags & M_COPYFLAGS) |
			    M_EXT | M_CLUSTER;
			M_MOVE_HDR(m, n);
			/* n->m_data is cool. */
		}
	}

	do {
		count = min(len, n->m_len);
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		    (unsigned)count);
		len -= count;
		m->m_len += count;
		n->m_len -= count;
		if (n->m_len)
			n->m_data += count;
		else
			n = m_free(n);
	} while (len > 0 && n);
	if (len > 0) {
		(void)m_free(m);
		goto bad;
	}
	m->m_next = n;

	return (m);
bad:
	m_freem(n);
	return (NULL);
}

/*
 * Return a pointer to mbuf/offset of location in mbuf chain.
 */
struct mbuf *
m_getptr(struct mbuf *m, int loc, int *off)
{
	while (loc >= 0) {
		/* Normal end of search */
		if (m->m_len > loc) {
	    		*off = loc;
	    		return (m);
		} else {
	    		loc -= m->m_len;

	    		if (m->m_next == NULL) {
				if (loc == 0) {
 					/* Point at the end of valid data */
		    			*off = m->m_len;
		    			return (m);
				} else {
		  			return (NULL);
				}
	    		} else {
	      			m = m->m_next;
			}
		}
    	}

	return (NULL);
}

/*
 * Inject a new mbuf chain of length siz in mbuf chain m0 at
 * position len0. Returns a pointer to the first injected mbuf, or
 * NULL on failure (m0 is left undisturbed). Note that if there is
 * enough space for an object of size siz in the appropriate position,
 * no memory will be allocated. Also, there will be no data movement in
 * the first len0 bytes (pointers to that will remain valid).
 *
 * XXX It is assumed that siz is less than the size of an mbuf at the moment.
 */
struct mbuf *
m_inject(struct mbuf *m0, int len0, int siz, int wait)
{
	struct mbuf *m, *n, *n2 = NULL, *n3;
	unsigned len = len0, remain;

	if ((siz >= MHLEN) || (len0 <= 0))
	        return (NULL);
	for (m = m0; m && len > m->m_len; m = m->m_next)
		len -= m->m_len;
	if (m == NULL)
		return (NULL);
	remain = m->m_len - len;
	if (remain == 0) {
	        if ((m->m_next) && (M_LEADINGSPACE(m->m_next) >= siz)) {
		        m->m_next->m_len += siz;
			if (m0->m_flags & M_PKTHDR)
				m0->m_pkthdr.len += siz;
			m->m_next->m_data -= siz;
			return m->m_next;
		}
	} else {
	        n2 = m_copym2(m, len, remain, wait);
		if (n2 == NULL)
		        return (NULL);
	}

	MGET(n, wait, MT_DATA);
	if (n == NULL) {
	        if (n2)
		        m_freem(n2);
		return (NULL);
	}

	n->m_len = siz;
	if (m0->m_flags & M_PKTHDR)
		m0->m_pkthdr.len += siz;
	m->m_len -= remain; /* Trim */
	if (n2)	{
	        for (n3 = n; n3->m_next != NULL; n3 = n3->m_next)
		        ;
		n3->m_next = n2;
	} else
	        n3 = n;
	for (; n3->m_next != NULL; n3 = n3->m_next)
	        ;
	n3->m_next = m->m_next;
	m->m_next = n;
	return n;
}

/*
 * Partition an mbuf chain in two pieces, returning the tail --
 * all but the first len0 bytes.  In case of failure, it returns NULL and
 * attempts to restore the chain to its original state.
 */
struct mbuf *
m_split(struct mbuf *m0, int len0, int wait)
{
	struct mbuf *m, *n;
	unsigned len = len0, remain, olen;

	for (m = m0; m && len > m->m_len; m = m->m_next)
		len -= m->m_len;
	if (m == NULL)
		return (NULL);
	remain = m->m_len - len;
	if (m0->m_flags & M_PKTHDR) {
		MGETHDR(n, wait, m0->m_type);
		if (n == NULL)
			return (NULL);
		if (m_dup_pkthdr(n, m0, wait)) {
			m_freem(n);
			return (NULL);
		}
		n->m_pkthdr.len -= len0;
		olen = m0->m_pkthdr.len;
		m0->m_pkthdr.len = len0;
		if (m->m_flags & M_EXT)
			goto extpacket;
		if (remain > MHLEN) {
			/* m can't be the lead packet */
			MH_ALIGN(n, 0);
			n->m_next = m_split(m, len, wait);
			if (n->m_next == NULL) {
				(void) m_free(n);
				m0->m_pkthdr.len = olen;
				return (NULL);
			} else
				return (n);
		} else
			MH_ALIGN(n, remain);
	} else if (remain == 0) {
		n = m->m_next;
		m->m_next = NULL;
		return (n);
	} else {
		MGET(n, wait, m->m_type);
		if (n == NULL)
			return (NULL);
		M_ALIGN(n, remain);
	}
extpacket:
	if (m->m_flags & M_EXT) {
		n->m_ext = m->m_ext;
		MCLADDREFERENCE(m, n);
		n->m_data = m->m_data + len;
	} else {
		bcopy(mtod(m, caddr_t) + len, mtod(n, caddr_t), remain);
	}
	n->m_len = remain;
	m->m_len = len;
	n->m_next = m->m_next;
	m->m_next = NULL;
	return (n);
}

/*
 * Routine to copy from device local memory into mbufs.
 */
struct mbuf *
m_devget(char *buf, int totlen, int off, struct ifnet *ifp,
    void (*copy)(const void *, void *, size_t))
{
	struct mbuf	*m;
	struct mbuf	*top, **mp;
	int		 len;

	top = NULL;
	mp = &top;

	if (off < 0 || off > MHLEN)
		return (NULL);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;

	len = MHLEN;

	while (totlen > 0) {
		if (top != NULL) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return (NULL);
			}
			len = MLEN;
		}

		if (totlen + off >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		} else {
			/* Place initial small packet/header at end of mbuf. */
			if (top == NULL && totlen + off + max_linkhdr <= len) {
				m->m_data += max_linkhdr;
				len -= max_linkhdr;
			}
		}

		if (off) {
			m->m_data += off;
			len -= off;
			off = 0;
		}

		m->m_len = len = min(totlen, len);

		if (copy)
			copy(buf, mtod(m, caddr_t), (size_t)len);
		else
			bcopy(buf, mtod(m, caddr_t), (size_t)len);

		buf += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
	}
	return (top);
}

void
m_zero(struct mbuf *m)
{
	while (m) {
#ifdef DIAGNOSTIC
		if (M_READONLY(m))
			panic("m_zero: M_READONLY");
#endif /* DIAGNOSTIC */
		if (m->m_flags & M_EXT)
			memset(m->m_ext.ext_buf, 0, m->m_ext.ext_size);
		else {
			if (m->m_flags & M_PKTHDR)
				memset(m->m_pktdat, 0, MHLEN);
			else
				memset(m->m_dat, 0, MLEN);
		}
		m = m->m_next;
	}
}

/*
 * Apply function f to the data in an mbuf chain starting "off" bytes from the
 * beginning, continuing for "len" bytes.
 */
int
m_apply(struct mbuf *m, int off, int len,
    int (*f)(caddr_t, caddr_t, unsigned int), caddr_t fstate)
{
	int rval;
	unsigned int count;

	if (len < 0)
		panic("m_apply: len %d < 0", len);
	if (off < 0)
		panic("m_apply: off %d < 0", off);
	while (off > 0) {
		if (m == NULL)
			panic("m_apply: null mbuf in skip");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		if (m == NULL)
			panic("m_apply: null mbuf");
		count = min(m->m_len - off, len);

		rval = f(fstate, mtod(m, caddr_t) + off, count);
		if (rval)
			return (rval);

		len -= count;
		off = 0;
		m = m->m_next;
	}

	return (0);
}

int
m_leadingspace(struct mbuf *m)
{
	if (M_READONLY(m))
		return 0;
	return (m->m_flags & M_EXT ? m->m_data - m->m_ext.ext_buf :
	    m->m_flags & M_PKTHDR ? m->m_data - m->m_pktdat :
	    m->m_data - m->m_dat);
}

int
m_trailingspace(struct mbuf *m)
{
	if (M_READONLY(m))
		return 0;
	return (m->m_flags & M_EXT ? m->m_ext.ext_buf +
	    m->m_ext.ext_size - (m->m_data + m->m_len) :
	    &m->m_dat[MLEN] - (m->m_data + m->m_len));
}


/*
 * Duplicate mbuf pkthdr from from to to.
 * from must have M_PKTHDR set, and to must be empty.
 */
int
m_dup_pkthdr(struct mbuf *to, struct mbuf *from, int wait)
{
	int error;

	KASSERT(from->m_flags & M_PKTHDR);

	to->m_flags = (to->m_flags & (M_EXT | M_CLUSTER));
	to->m_flags |= (from->m_flags & M_COPYFLAGS);
	to->m_pkthdr = from->m_pkthdr;

	SLIST_INIT(&to->m_pkthdr.tags);

	if ((error = m_tag_copy_chain(to, from, wait)) != 0)
		return (error);

	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;

	return (0);
}

#ifdef DDB
void
m_print(void *v, int (*pr)(const char *, ...))
{
	struct mbuf *m = v;

	(*pr)("mbuf %p\n", m);
	(*pr)("m_type: %hi\tm_flags: %b\n", m->m_type, m->m_flags,
	    "\20\1M_EXT\2M_PKTHDR\3M_EOR\4M_CLUSTER\5M_PROTO1\6M_VLANTAG"
	    "\7M_LOOP\10M_FILDROP\11M_BCAST\12M_MCAST\13M_CONF\14M_AUTH"
	    "\15M_TUNNEL\16M_AUTH_AH\17M_LINK0");
	(*pr)("m_next: %p\tm_nextpkt: %p\n", m->m_next, m->m_nextpkt);
	(*pr)("m_data: %p\tm_len: %u\n", m->m_data, m->m_len);
	(*pr)("m_dat: %p m_pktdat: %p\n", m->m_dat, m->m_pktdat);
	if (m->m_flags & M_PKTHDR) {
		(*pr)("m_pkthdr.len: %i\tm_ptkhdr.rcvif: %p\t"
		    "m_ptkhdr.rdomain: %u\n", m->m_pkthdr.len, 
		    m->m_pkthdr.rcvif, m->m_pkthdr.rdomain);
		(*pr)("m_ptkhdr.tags: %p\tm_pkthdr.tagsset: %hx\n",
		    SLIST_FIRST(&m->m_pkthdr.tags), m->m_pkthdr.tagsset);
		(*pr)("m_pkthdr.csum_flags: %hx\tm_pkthdr.ether_vtag: %hu\n",
		    m->m_pkthdr.csum_flags, m->m_pkthdr.ether_vtag);
		(*pr)("m_pkthdr.pf.flags: %b\n",
		    m->m_pkthdr.pf.flags, "\20\1GENERATED\2FRAGCACHE"
		    "\3TRANSLATE_LOCALHOST\4DIVERTED\5DIVERTED_PACKET"
		    "\6PF_TAG_REROUTE");
		(*pr)("m_pkthdr.pf.hdr: %p\tm_pkthdr.pf.statekey: %p\n",
		    m->m_pkthdr.pf.hdr, m->m_pkthdr.pf.statekey);
		(*pr)("m_pkthdr.pf.qid:\t%u m_pkthdr.pf.tag: %hu\n",
		    m->m_pkthdr.pf.qid, m->m_pkthdr.pf.tag);
		(*pr)("m_pkthdr.pf.routed: %hhx\n", m->m_pkthdr.pf.routed);
	}
	if (m->m_flags & M_EXT) {
		(*pr)("m_ext.ext_buf: %p\tm_ext.ext_size: %u\n",
		    m->m_ext.ext_buf, m->m_ext.ext_size);
		(*pr)("m_ext.ext_type: %x\tm_ext.ext_backend: %i\n",
		    m->m_ext.ext_type, m->m_ext.ext_backend);
		(*pr)("m_ext.ext_ifp: %p\n", m->m_ext.ext_ifp);
		(*pr)("m_ext.ext_free: %p\tm_ext.ext_arg: %p\n",
		    m->m_ext.ext_free, m->m_ext.ext_arg);
		(*pr)("m_ext.ext_nextref: %p\tm_ext.ext_prevref: %p\n",
		    m->m_ext.ext_nextref, m->m_ext.ext_prevref);
	}
}
#endif
