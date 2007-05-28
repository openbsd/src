/*	$OpenBSD: uipc_mbuf.c,v 1.82 2007/05/28 17:16:39 henning Exp $	*/
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
#define MBTYPES
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pool.h>

#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

struct	mbstat mbstat;		/* mbuf stats */
struct	pool mbpool;		/* mbuf pool */
struct	pool mclpool;		/* mbuf cluster pool */

int max_linkhdr;		/* largest link-level header */
int max_protohdr;		/* largest protocol header */
int max_hdr;			/* largest link+protocol header */
int max_datalen;		/* MHLEN - max_hdr */

struct mbuf *m_copym0(struct mbuf *, int, int, int, int);
void	nmbclust_update(void);


const char *mclpool_warnmsg =
    "WARNING: mclpool limit reached; increase kern.maxclusters";

/*
 * Initialize the mbuf allocator.
 */
void
mbinit(void)
{
	pool_init(&mbpool, MSIZE, 0, 0, 0, "mbpl", NULL);
	pool_init(&mclpool, MCLBYTES, 0, 0, 0, "mclpl", NULL);

	nmbclust_update();

	/*
	 * Set a low water mark for both mbufs and clusters.  This should
	 * help ensure that they can be allocated in a memory starvation
	 * situation.  This is important for e.g. diskless systems which
	 * must allocate mbufs in order for the pagedaemon to clean pages.
	 */
	pool_setlowat(&mbpool, mblowat);
	pool_setlowat(&mclpool, mcllowat);
}

void
nmbclust_update(void)
{
	/*
	 * Set the hard limit on the mclpool to the number of
	 * mbuf clusters the kernel is to support.  Log the limit
	 * reached message max once a minute.
	 */
	(void)pool_sethardlimit(&mclpool, nmbclust, mclpool_warnmsg, 60);
}

void
m_reclaim(void *arg, int flags)
{
	struct domain *dp;
	struct protosw *pr;
	int s = splvm();

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain)
				(*pr->pr_drain)();
	splx(s);
	mbstat.m_drain++;
}

/*
 * Space allocation routines.
 */
struct mbuf *
m_get(int nowait, int type)
{
	struct mbuf *m;
	int s;

	s = splvm();
	m = pool_get(&mbpool, nowait == M_WAIT ? PR_WAITOK|PR_LIMITFAIL : 0);
	if (m) {
		m->m_type = type;
		mbstat.m_mtypes[type]++;
		m->m_next = (struct mbuf *)NULL;
		m->m_nextpkt = (struct mbuf *)NULL;
		m->m_data = m->m_dat;
		m->m_flags = 0;
	}
	splx(s);
	return (m);
}

struct mbuf *
m_gethdr(int nowait, int type)
{
	struct mbuf *m;
	int s;

	s = splvm();
	m = pool_get(&mbpool, nowait == M_WAIT ? PR_WAITOK|PR_LIMITFAIL : 0);
	if (m) {
		m->m_type = type;
		mbstat.m_mtypes[type]++;
		m->m_next = (struct mbuf *)NULL;
		m->m_nextpkt = (struct mbuf *)NULL;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
		SLIST_INIT(&m->m_pkthdr.tags);
		m->m_pkthdr.csum_flags = 0;
		m->m_pkthdr.pf.hdr = NULL;
		m->m_pkthdr.pf.rtableid = 0;
		m->m_pkthdr.pf.qid = 0;
		m->m_pkthdr.pf.tag = 0;
		m->m_pkthdr.pf.flags = 0;
		m->m_pkthdr.pf.routed = 0;
	}
	splx(s);
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

void
m_clget(struct mbuf *m, int how)
{
	int s;

	s = splvm();
	m->m_ext.ext_buf =
	    pool_get(&mclpool, how == M_WAIT ? (PR_WAITOK|PR_LIMITFAIL) : 0);
	splx(s);
	if (m->m_ext.ext_buf != NULL) {
		m->m_data = m->m_ext.ext_buf;
		m->m_flags |= M_EXT|M_CLUSTER;
		m->m_ext.ext_size = MCLBYTES;
		m->m_ext.ext_free = NULL;
		m->m_ext.ext_arg = NULL;
		MCLINITREFERENCE(m);
	}
}

struct mbuf *
m_free(struct mbuf *m)
{
	struct mbuf *n;

	MFREE(m, n);
	return (n);
}

void
m_freem(struct mbuf *m)
{
	struct mbuf *n;

	if (m == NULL)
		return;
	do {
		MFREE(m, n);
	} while ((m = n) != NULL);
}

/*
 * Mbuffer utility routines.
 */

/*
 * Lesser-used path for M_PREPEND:
 * allocate new mbuf to prepend to chain,
 * copy junk along.
 */
struct mbuf *
m_prepend(struct mbuf *m, int len, int how)
{
	struct mbuf *mn;

	if (len > MHLEN)
		panic("mbuf prepend length too big");

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
	return (m);
}

/*
 * Make a copy of an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes.  If len is M_COPYALL, copy to end of mbuf.
 * The wait parameter is a choice of M_WAIT/M_DONTWAIT from caller.
 */
int MCFail;

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
m_copym0(struct mbuf *m, int off, int len, int wait, int deep)
{
	struct mbuf *n, **np;
	struct mbuf *top;
	int copyhdr = 0;

	if (off < 0 || len < 0)
		panic("m_copym0: off %d, len %d", off, len);
	if (off == 0 && m->m_flags & M_PKTHDR)
		copyhdr = 1;
	while (off > 0) {
		if (m == NULL)
			panic("m_copym0: null mbuf");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
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
			M_DUP_PKTHDR(n, m);
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
	if (top == NULL)
		MCFail++;
	return (top);
nospace:
	m_freem(top);
	MCFail++;
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
	while (off > 0) {
		if (m == NULL)
			panic("m_copydata: null mbuf in skip");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
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
void
m_copyback(struct mbuf *m0, int off, int len, const void *_cp)
{
	int mlen;
	struct mbuf *m = m0, *n;
	int totlen = 0;
	caddr_t cp = (caddr_t)_cp;

	if (m0 == NULL)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == NULL) {
			n = m_getclr(M_DONTWAIT, m->m_type);
			if (n == NULL)
				goto out;
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		mlen = min (m->m_len - off, len);
		bcopy(cp, off + mtod(m, caddr_t), (unsigned)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == NULL) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == NULL)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
out:	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
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
int MPFail;

struct mbuf *
m_pullup(struct mbuf *n, int len)
{
	struct mbuf *m;
	int count;
	int space;

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
	MPFail++;
	return (NULL);
}

/*
 * m_pullup2() works like m_pullup, save that len can be <= MCLBYTES.
 * m_pullup2() only works on values of len such that MHLEN < len <= MCLBYTES,
 * it calls m_pullup() for values <= MHLEN.  It also only coagulates the
 * reqested number of bytes.  (For those of us who expect unwieldly option
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
		if ((m->m_flags & M_EXT) == 0)
			goto bad;
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
	MPFail++;
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
		}
		else {
	    		loc -= m->m_len;

	    		if (m->m_next == NULL) {
				if (loc == 0) {
 					/* Point at the end of valid data */
		    			*off = m->m_len;
		    			return (m);
				}
				else
		  			return (NULL);
	    		} else
	      			m = m->m_next;
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
		M_DUP_PKTHDR(n, m0);
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
	struct mbuf *m;
	struct mbuf *top = NULL, **mp = &top;
	int len;
	char *cp;
	char *epkt;

	cp = buf;
	epkt = cp + totlen;
	if (off) {
		/*
		 * If 'off' is non-zero, packet is trailer-encapsulated,
		 * so we have to skip the type and length fields.
		 */
		cp += off + 2 * sizeof(u_int16_t);
		totlen -= 2 * sizeof(u_int16_t);
	}
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	while (totlen > 0) {
		if (top != NULL) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return (NULL);
			}
			m->m_len = MLEN;
		}
		len = min(totlen, epkt - cp);
		if (len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = len = min(len, MCLBYTES);
			else
				len = m->m_len;
		} else {
			/*
			 * Place initial small packet/header at end of mbuf.
			 */
			if (len < m->m_len) {
				if (top == NULL &&
				    len + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = len;
			} else
				len = m->m_len;
		}
		if (copy)
			copy(cp, mtod(m, caddr_t), (size_t)len);
		else
			bcopy(cp, mtod(m, caddr_t), (size_t)len);
		cp += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
		if (cp == epkt)
			cp = buf;
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
