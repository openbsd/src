/*	$OpenBSD: bpf.c,v 1.19 2000/02/19 08:59:04 niklas Exp $	*/
/*	$NetBSD: bpf.c,v 1.33 1997/02/21 23:59:35 thorpej Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence 
 * Berkeley Laboratory.
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
 *	@(#)bpf.c	8.2 (Berkeley) 3/28/94
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/map.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include <sys/file.h>
#if defined(sparc) && BSD < 199103
#include <sys/stream.h>
#endif
#include <sys/tty.h>
#include <sys/uio.h>

#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/kernel.h>

#include <net/if.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <netinet/in.h>
#include <netinet/if_arc.h>
#include <netinet/if_ether.h>

/*
 * Older BSDs don't have kernel malloc.
 */
#if BSD < 199103
extern bcopy();
caddr_t bpf_alloc();
#include <net/bpf_compat.h>
#define BPF_BUFSIZE (MCLBYTES-8)
#define UIOMOVE(cp, len, code, uio) uiomove(cp, len, code, uio)
#else
#define BPF_BUFSIZE 8192	/* 4096 too small for FDDI frames */
#define UIOMOVE(cp, len, code, uio) uiomove(cp, len, uio)
#endif

#define PRINET  6			/* interruptible */

/*
 * The default read buffer size is patchable.
 */
int bpf_bufsize = BPF_BUFSIZE;

/*
 *  bpf_iflist is the list of interfaces; each corresponds to an ifnet
 *  bpf_dtab holds the descriptors, indexed by minor device #
 */
struct bpf_if	*bpf_iflist;
struct bpf_d	bpf_dtab[NBPFILTER];

int	bpf_allocbufs __P((struct bpf_d *));
void	bpf_freed __P((struct bpf_d *));
void	bpf_ifname __P((struct ifnet *, struct ifreq *));
void	bpf_mcopy __P((const void *, void *, size_t));
int	bpf_movein __P((struct uio *, int, struct mbuf **, struct sockaddr *));
void	bpf_attachd __P((struct bpf_d *, struct bpf_if *));
void	bpf_detachd __P((struct bpf_d *));
int	bpf_setif __P((struct bpf_d *, struct ifreq *));
#if BSD >= 199103
int	bpfselect __P((dev_t, int, struct proc *));
#endif
static __inline void bpf_wakeup __P((struct bpf_d *));
void	bpf_catchpacket __P((struct bpf_d *, u_char *, size_t, size_t,
	    void (*)(const void *, void *, size_t)));
void	bpf_reset_d __P((struct bpf_d *));

int
bpf_movein(uio, linktype, mp, sockp)
	register struct uio *uio;
	int linktype;
	register struct mbuf **mp;
	register struct sockaddr *sockp;
{
	struct mbuf *m;
	int error;
	int len;
	int hlen;

	/*
	 * Build a sockaddr based on the data link layer type.
	 * We do this at this level because the ethernet header
	 * is copied directly into the data field of the sockaddr.
	 * In the case of SLIP, there is no header and the packet
	 * is forwarded as is.
	 * Also, we are careful to leave room at the front of the mbuf
	 * for the link level header.
	 */
	switch (linktype) {

	case DLT_SLIP:
		sockp->sa_family = AF_INET;
		hlen = 0;
		break;

	case DLT_PPP:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	case DLT_EN10MB:
		sockp->sa_family = AF_UNSPEC;
		/* XXX Would MAXLINKHDR be better? */
		hlen = sizeof(struct ether_header);
		break;

	case DLT_ARCNET:
		sockp->sa_family = AF_UNSPEC;
		hlen = ARC_HDRLEN;
		break;

	case DLT_FDDI:
		sockp->sa_family = AF_UNSPEC;
		/* XXX 4(FORMAC)+6(dst)+6(src)+3(LLC)+5(SNAP) */
		hlen = 24;
		break;

	case DLT_RAW:
	case DLT_NULL:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	default:
		return (EIO);
	}

	len = uio->uio_resid;
	if ((unsigned)len > MCLBYTES)
		return (EIO);

	MGETHDR(m, M_WAIT, MT_DATA);
	if (m == 0)
		return (ENOBUFS);
	m->m_pkthdr.rcvif = 0;
	m->m_pkthdr.len = len - hlen;

	if (len > MHLEN) {
#if BSD >= 199103
		MCLGET(m, M_WAIT);
		if ((m->m_flags & M_EXT) == 0) {
#else
		MCLGET(m);
		if (m->m_len != MCLBYTES) {
#endif
			error = ENOBUFS;
			goto bad;
		}
	}
	m->m_len = len;
	*mp = m;
	/*
	 * Make room for link header.
	 */
	if (hlen != 0) {
		m->m_len -= hlen;
#if BSD >= 199103
		m->m_data += hlen; /* XXX */
#else
		m->m_off += hlen;
#endif
		error = UIOMOVE((caddr_t)sockp->sa_data, hlen, UIO_WRITE, uio);
		if (error)
			goto bad;
	}
	error = UIOMOVE(mtod(m, caddr_t), len - hlen, UIO_WRITE, uio);
	if (!error)
		return (0);
 bad:
	m_freem(m);
	return (error);
}

/*
 * Attach file to the bpf interface, i.e. make d listen on bp.
 * Must be called at splimp.
 */
void
bpf_attachd(d, bp)
	struct bpf_d *d;
	struct bpf_if *bp;
{
	/*
	 * Point d at bp, and add d to the interface's list of listeners.
	 * Finally, point the driver's bpf cookie at the interface so
	 * it will divert packets to bpf.
	 */
	d->bd_bif = bp;
	d->bd_next = bp->bif_dlist;
	bp->bif_dlist = d;

	*bp->bif_driverp = bp;
}

/*
 * Detach a file from its interface.
 */
void
bpf_detachd(d)
	struct bpf_d *d;
{
	struct bpf_d **p;
	struct bpf_if *bp;

	bp = d->bd_bif;
	/*
	 * Check if this descriptor had requested promiscuous mode.
	 * If so, turn it off.
	 */
	if (d->bd_promisc) {
		int error;

		d->bd_promisc = 0;
		error = ifpromisc(bp->bif_ifp, 0);
		if (error && error != EINVAL)
			/*
			 * Something is really wrong if we were able to put
			 * the driver into promiscuous mode, but can't
			 * take it out.
			 */
			panic("bpf: ifpromisc failed");
	}
	/* Remove d from the interface's descriptor list. */
	p = &bp->bif_dlist;
	while (*p != d) {
		p = &(*p)->bd_next;
		if (*p == 0)
			panic("bpf_detachd: descriptor not in list");
	}
	*p = (*p)->bd_next;
	if (bp->bif_dlist == 0)
		/*
		 * Let the driver know that there are no more listeners.
		 */
		*d->bd_bif->bif_driverp = 0;
	d->bd_bif = 0;
}


/*
 * Mark a descriptor free by making it point to itself.
 * This is probably cheaper than marking with a constant since
 * the address should be in a register anyway.
 */
#define D_ISFREE(d) ((d) == (d)->bd_next)
#define D_MARKFREE(d) ((d)->bd_next = (d))
#define D_MARKUSED(d) ((d)->bd_next = 0)

#if BSD >= 199207 || NetBSD0_9 >= 2
/*
 * bpfilterattach() is called at boot time in new systems.  We do
 * nothing here since old systems will not call this.
 */
/* ARGSUSED */
void
bpfilterattach(n)
	int n;
{
	int i;

	/*
	 * Mark all the descriptors free if this hasn't been done.
	 */
	if (!D_ISFREE(&bpf_dtab[0]))
		for (i = 0; i < NBPFILTER; ++i)
			D_MARKFREE(&bpf_dtab[i]);
}
#endif

/*
 * Open ethernet device.  Returns ENXIO for illegal minor device number,
 * EBUSY if file is open by another process.
 */
/* ARGSUSED */
int
bpfopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	register struct bpf_d *d;

	if (minor(dev) >= NBPFILTER)
		return (ENXIO);
	/*
	 * Each minor can be opened by only one process.  If the requested
	 * minor is in use, return EBUSY.
	 */
	d = &bpf_dtab[minor(dev)];
	if (!D_ISFREE(d))
		return (EBUSY);

	/* Mark "free" and do most initialization. */
	bzero((char *)d, sizeof(*d));
	d->bd_bufsize = bpf_bufsize;
	d->bd_sig = SIGIO;

	return (0);
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
/* ARGSUSED */
int
bpfclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	register struct bpf_d *d = &bpf_dtab[minor(dev)];
	register int s;

	s = splimp();
	if (d->bd_bif)
		bpf_detachd(d);
	splx(s);
	bpf_freed(d);

	return (0);
}

/*
 * Support for SunOS, which does not have tsleep.
 */
#if BSD < 199103
int
bpf_timeout(arg)
	caddr_t arg;
{
	struct bpf_d *d = (struct bpf_d *)arg;
	d->bd_timedout = 1;
	wakeup(arg);
}

#define BPF_SLEEP(chan, pri, s, t) bpf_sleep((struct bpf_d *)chan)

int
bpf_sleep(d)
	register struct bpf_d *d;
{
	register int rto = d->bd_rtout;
	register int st;

	if (rto != 0) {
		d->bd_timedout = 0;
		timeout(bpf_timeout, (caddr_t)d, rto);
	}
	st = sleep((caddr_t)d, PRINET|PCATCH);
	if (rto != 0) {
		if (d->bd_timedout == 0)
			untimeout(bpf_timeout, (caddr_t)d);
		else if (st == 0)
			return EWOULDBLOCK;
	}
	return (st != 0) ? EINTR : 0;
}
#else
#define BPF_SLEEP tsleep
#endif

/*
 * Rotate the packet buffers in descriptor d.  Move the store buffer
 * into the hold slot, and the free buffer into the store slot.
 * Zero the length of the new store buffer.
 */
#define ROTATE_BUFFERS(d) \
	(d)->bd_hbuf = (d)->bd_sbuf; \
	(d)->bd_hlen = (d)->bd_slen; \
	(d)->bd_sbuf = (d)->bd_fbuf; \
	(d)->bd_slen = 0; \
	(d)->bd_fbuf = 0;
/*
 *  bpfread - read next chunk of packets from buffers
 */
int
bpfread(dev, uio, ioflag)
	dev_t dev;
	register struct uio *uio;
	int ioflag;
{
	register struct bpf_d *d = &bpf_dtab[minor(dev)];
	int error;
	int s;

	if (d->bd_bif == 0)
		return (ENXIO);

	/*
	 * Restrict application to use a buffer the same size as
	 * as kernel buffers.
	 */
	if (uio->uio_resid != d->bd_bufsize)
		return (EINVAL);

	s = splimp();

	/*
	 * bd_rdStart is tagged when we start the read, iff there's a timeout.
	 * we can then figure out when we're done reading.
	 */
	if (d->bd_rtout != -1 && d->bd_rdStart == 0)
		d->bd_rdStart = ticks;
	else
		d->bd_rdStart = 0;

	/*
	 * If the hold buffer is empty, then do a timed sleep, which
	 * ends when the timeout expires or when enough packets
	 * have arrived to fill the store buffer.
	 */
	while (d->bd_hbuf == 0) {
		if (d->bd_immediate && d->bd_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 * Rotate the buffers and return what's here.
			 */
			ROTATE_BUFFERS(d);
			break;
		}
		if ((d->bd_rtout != -1) || (d->bd_rdStart + d->bd_rtout) < ticks) {
			error = BPF_SLEEP((caddr_t)d, PRINET|PCATCH, "bpf",
			    d->bd_rtout);
		} else {
			if (d->bd_rtout == -1) {
				/* User requested non-blocking I/O */
				error = EWOULDBLOCK;
			} else
				error = 0;
		}
		if (error == EINTR || error == ERESTART) {
			splx(s);
			return (error);
		}
		if (error == EWOULDBLOCK) {
			/*
			 * On a timeout, return what's in the buffer,
			 * which may be nothing.  If there is something
			 * in the store buffer, we can rotate the buffers.
			 */
			if (d->bd_hbuf)
				/*
				 * We filled up the buffer in between
				 * getting the timeout and arriving
				 * here, so we don't need to rotate.
				 */
				break;

			if (d->bd_slen == 0) {
				splx(s);
				return (0);
			}
			ROTATE_BUFFERS(d);
			break;
		}
	}
	/*
	 * At this point, we know we have something in the hold slot.
	 */
	splx(s);

	/*
	 * Move data from hold buffer into user space.
	 * We know the entire buffer is transferred since
	 * we checked above that the read buffer is bpf_bufsize bytes.
	 */
	error = UIOMOVE(d->bd_hbuf, d->bd_hlen, UIO_READ, uio);

	s = splimp();
	d->bd_fbuf = d->bd_hbuf;
	d->bd_hbuf = 0;
	d->bd_hlen = 0;
	splx(s);

	return (error);
}


/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
static __inline void
bpf_wakeup(d)
	register struct bpf_d *d;
{
	wakeup((caddr_t)d);
	if (d->bd_async && d->bd_sig)
		csignal(d->bd_pgid, d->bd_sig,
		    d->bd_siguid, d->bd_sigeuid);

#if BSD >= 199103
	selwakeup(&d->bd_sel);
	/* XXX */
	d->bd_sel.si_selpid = 0;
#else
	if (d->bd_selproc) {
		selwakeup(d->bd_selproc, (int)d->bd_selcoll);
		d->bd_selcoll = 0;
		d->bd_selproc = 0;
	}
#endif
}

int
bpfwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	register struct bpf_d *d = &bpf_dtab[minor(dev)];
	struct ifnet *ifp;
	struct mbuf *m;
	int error, s;
	struct sockaddr dst;

	if (d->bd_bif == 0)
		return (ENXIO);

	ifp = d->bd_bif->bif_ifp;

	if (uio->uio_resid == 0)
		return (0);

	error = bpf_movein(uio, (int)d->bd_bif->bif_dlt, &m, &dst);
	if (error)
		return (error);

	if (m->m_pkthdr.len > ifp->if_mtu) {
		m_freem(m);
		return (EMSGSIZE);
	}

	s = splsoftnet();
#if BSD >= 199103
	error = (*ifp->if_output)(ifp, m, &dst, (struct rtentry *)0);
#else
	error = (*ifp->if_output)(ifp, m, &dst);
#endif
	splx(s);
	/*
	 * The driver frees the mbuf.
	 */
	return (error);
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the
 * receive and drop counts.  Should be called at splimp.
 */
void
bpf_reset_d(d)
	struct bpf_d *d;
{
	if (d->bd_hbuf) {
		/* Free the hold buffer. */
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = 0;
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	d->bd_rcount = 0;
	d->bd_dcount = 0;
}

/*
 *  FIONREAD		Check for read packet available.
 *  BIOCGBLEN		Get buffer len [for read()].
 *  BIOCSETF		Set ethernet read filter.
 *  BIOCFLUSH		Flush read packet buffer.
 *  BIOCPROMISC		Put interface into promiscuous mode.
 *  BIOCGDLT		Get link layer type.
 *  BIOCGETIF		Get interface name.
 *  BIOCSETIF		Set interface.
 *  BIOCSRTIMEOUT	Set read timeout.
 *  BIOCGRTIMEOUT	Get read timeout.
 *  BIOCGSTATS		Get packet stats.
 *  BIOCIMMEDIATE	Set immediate mode.
 *  BIOCVERSION		Get filter language version.
 */
/* ARGSUSED */
int
bpfioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	register struct bpf_d *d = &bpf_dtab[minor(dev)];
	int s, error = 0;

	switch (cmd) {

	default:
		error = EINVAL;
		break;

	/*
	 * Check for read packet available.
	 */
	case FIONREAD:
		{
			int n;

			s = splimp();
			n = d->bd_slen;
			if (d->bd_hbuf)
				n += d->bd_hlen;
			splx(s);

			*(int *)addr = n;
			break;
		}

	/*
	 * Get buffer len [for read()].
	 */
	case BIOCGBLEN:
		*(u_int *)addr = d->bd_bufsize;
		break;

	/*
	 * Set buffer length.
	 */
	case BIOCSBLEN:
#if BSD < 199103
		error = EINVAL;
#else
		if (d->bd_bif != 0)
			error = EINVAL;
		else {
			register u_int size = *(u_int *)addr;

			if (size > BPF_MAXBUFSIZE)
				*(u_int *)addr = size = BPF_MAXBUFSIZE;
			else if (size < BPF_MINBUFSIZE)
				*(u_int *)addr = size = BPF_MINBUFSIZE;
			d->bd_bufsize = size;
		}
#endif
		break;

	/*
	 * Set link layer read filter.
	 */
	case BIOCSETF:
		error = bpf_setf(d, (struct bpf_program *)addr);
		break;

	/*
	 * Flush read packet buffer.
	 */
	case BIOCFLUSH:
		s = splimp();
		bpf_reset_d(d);
		splx(s);
		break;

	/*
	 * Put interface into promiscuous mode.
	 */
	case BIOCPROMISC:
		if (d->bd_bif == 0) {
			/*
			 * No interface attached yet.
			 */
			error = EINVAL;
			break;
		}
		s = splimp();
		if (d->bd_promisc == 0) {
			error = ifpromisc(d->bd_bif->bif_ifp, 1);
			if (error == 0)
				d->bd_promisc = 1;
		}
		splx(s);
		break;

	/*
	 * Get device parameters.
	 */
	case BIOCGDLT:
		if (d->bd_bif == 0)
			error = EINVAL;
		else
			*(u_int *)addr = d->bd_bif->bif_dlt;
		break;

	/*
	 * Set interface name.
	 */
	case BIOCGETIF:
		if (d->bd_bif == 0)
			error = EINVAL;
		else
			bpf_ifname(d->bd_bif->bif_ifp, (struct ifreq *)addr);
		break;

	/*
	 * Set interface.
	 */
	case BIOCSETIF:
		error = bpf_setif(d, (struct ifreq *)addr);
		break;

	/*
	 * Set read timeout.
	 */
	case BIOCSRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			/* Compute number of ticks. */
			d->bd_rtout = tv->tv_sec * hz + tv->tv_usec / tick;
			if (d->bd_rtout == 0 && tv->tv_usec != 0)
				d->bd_rtout = 1;
			break;
		}

	/*
	 * Get read timeout.
	 */
	case BIOCGRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			tv->tv_sec = d->bd_rtout / hz;
			tv->tv_usec = (d->bd_rtout % hz) * tick;
			break;
		}

	/*
	 * Get packet stats.
	 */
	case BIOCGSTATS:
		{
			struct bpf_stat *bs = (struct bpf_stat *)addr;

			bs->bs_recv = d->bd_rcount;
			bs->bs_drop = d->bd_dcount;
			break;
		}

	/*
	 * Set immediate mode.
	 */
	case BIOCIMMEDIATE:
		d->bd_immediate = *(u_int *)addr;
		break;

	case BIOCVERSION:
		{
			struct bpf_version *bv = (struct bpf_version *)addr;

			bv->bv_major = BPF_MAJOR_VERSION;
			bv->bv_minor = BPF_MINOR_VERSION;
			break;
		}


	case FIONBIO:		/* Non-blocking I/O */
		if (*(int *)addr)
			d->bd_rtout = -1;
		else
			d->bd_rtout = 0;
		break;

	case FIOASYNC:		/* Send signal on receive packets */
		d->bd_async = *(int *)addr;
		break;

	/*
	 * N.B.  ioctl (FIOSETOWN) and fcntl (F_SETOWN) both end up doing
	 * the equivalent of a TIOCSPGRP and hence end up here.  *However*
	 * TIOCSPGRP's arg is a process group if it's positive and a process
	 * id if it's negative.  This is exactly the opposite of what the
	 * other two functions want!  Therefore there is code in ioctl and
	 * fcntl to negate the arg before calling here.
	 */
	case TIOCSPGRP:		/* Process or group to send signals to */
		d->bd_pgid = *(int *)addr;
		d->bd_siguid = p->p_cred->p_ruid;
		d->bd_sigeuid = p->p_ucred->cr_uid;
		break;

	case TIOCGPGRP:
		*(int *)addr = d->bd_pgid;
		break;

	case BIOCSRSIG:		/* Set receive signal */
		{
		 	u_int sig;

			sig = *(u_int *)addr;

			if (sig >= NSIG)
				error = EINVAL;
			else
				d->bd_sig = sig;
			break;
		}
	case BIOCGRSIG:
		*(u_int *)addr = d->bd_sig;
		break;
	}
	return (error);
}

/*
 * Set d's packet filter program to fp.  If this file already has a filter,
 * free it and replace it.  Returns EINVAL for bogus requests.
 */
int
bpf_setf(d, fp)
	struct bpf_d *d;
	struct bpf_program *fp;
{
	struct bpf_insn *fcode, *old;
	u_int flen, size;
	int s;

	old = d->bd_filter;
	if (fp->bf_insns == 0) {
		if (fp->bf_len != 0)
			return (EINVAL);
		s = splimp();
		d->bd_filter = 0;
		bpf_reset_d(d);
		splx(s);
		if (old != 0)
			free((caddr_t)old, M_DEVBUF);
		return (0);
	}
	flen = fp->bf_len;
	if (flen > BPF_MAXINSNS)
		return (EINVAL);

	size = flen * sizeof(*fp->bf_insns);
	fcode = (struct bpf_insn *)malloc(size, M_DEVBUF, M_WAITOK);
	if (copyin((caddr_t)fp->bf_insns, (caddr_t)fcode, size) == 0 &&
	    bpf_validate(fcode, (int)flen)) {
		s = splimp();
		d->bd_filter = fcode;
		bpf_reset_d(d);
		splx(s);
		if (old != 0)
			free((caddr_t)old, M_DEVBUF);

		return (0);
	}
	free((caddr_t)fcode, M_DEVBUF);
	return (EINVAL);
}

/*
 * Detach a file from its current interface (if attached at all) and attach
 * to the interface indicated by the name stored in ifr.
 * Return an errno or 0.
 */
int
bpf_setif(d, ifr)
	struct bpf_d *d;
	struct ifreq *ifr;
{
	struct bpf_if *bp;
	char *cp;
	int unit_seen, i, s, error;

	/*
	 * Make sure the provided name has a unit number, and default
	 * it to '0' if not specified.
	 * XXX This is ugly ... do this differently?
	 */
	unit_seen = 0;
	cp = ifr->ifr_name;
	cp[sizeof(ifr->ifr_name) - 1] = '\0';	/* sanity */
	while (*cp++)
		if (*cp >= '0' && *cp <= '9')
			unit_seen = 1;
	if (!unit_seen) {
		/* Make sure to leave room for the '\0'. */
		for (i = 0; i < (IFNAMSIZ - 1); ++i) {
			if ((ifr->ifr_name[i] >= 'a' &&
			     ifr->ifr_name[i] <= 'z') ||
			    (ifr->ifr_name[i] >= 'A' &&
			     ifr->ifr_name[i] <= 'Z'))
				continue;
			ifr->ifr_name[i] = '0';
		}
	}

	/*
	 * Look through attached interfaces for the named one.
	 */
	for (bp = bpf_iflist; bp != 0; bp = bp->bif_next) {
		struct ifnet *ifp = bp->bif_ifp;

		if (ifp == 0 ||
		    strcmp(ifp->if_xname, ifr->ifr_name) != 0)
			continue;
		/*
		 * We found the requested interface.
		 * If it's not up, return an error.
		 * Allocate the packet buffers if we need to.
		 * If we're already attached to requested interface,
		 * just flush the buffer.
		 */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (ENETDOWN);

		if (d->bd_sbuf == 0) {
			error = bpf_allocbufs(d);
			if (error != 0)
				return (error);
		}
		s = splimp();
		if (bp != d->bd_bif) {
			if (d->bd_bif)
				/*
				 * Detach if attached to something else.
				 */
				bpf_detachd(d);

			bpf_attachd(d, bp);
		}
		bpf_reset_d(d);
		splx(s);
		return (0);
	}
	/* Not found. */
	return (ENXIO);
}

/*
 * Copy the interface name to the ifreq.
 */
void
bpf_ifname(ifp, ifr)
	struct ifnet *ifp;
	struct ifreq *ifr;
{
	bcopy(ifp->if_xname, ifr->ifr_name, IFNAMSIZ);
}

/*
 * The new select interface passes down the proc pointer; the old select
 * stubs had to grab it out of the user struct.  This glue allows either case.
 */
#if BSD >= 199103
#define bpf_select bpfselect
#else
int
bpfselect(dev, rw)
	register dev_t dev;
	int rw;
{
	/*
	 * if there isn't data waiting, and there's a timeout,
	 * mark the time we started waiting.
	 */
	if (b->db_rtout != -1 && (d->bd_rdStart == 0))
		d->bd_rdStart = ticks;
			    
	return (bpf_select(dev, rw, u.u_procp));
}
#endif

/*
 * Support for select() system call
 *
 * Return true iff the specific operation will not block indefinitely.
 * Otherwise, return false but make a note that a selwakeup() must be done.
 */
int
bpf_select(dev, rw, p)
	register dev_t dev;
	int rw;
	struct proc *p;
{
	register struct bpf_d *d;
	register int s;

	if (rw != FREAD)
		return (0);
	/*
	 * An imitation of the FIONREAD ioctl code.
	 */
	d = &bpf_dtab[minor(dev)];

	s = splimp();
	if (d->bd_hlen != 0 || (d->bd_immediate && d->bd_slen != 0)) {
		/*
		 * There is data waiting.
		 */
		splx(s);
		return (1);
	}

	/*
	 * if there isn't data waiting, and there's a timeout,
	 * mark the time we started waiting.
	 */
	if (d->bd_rtout != -1 && d->bd_rdStart == 0)
		d->bd_rdStart = ticks;
			    
#if BSD >= 199103
	selrecord(p, &d->bd_sel);
#else
	/*
	 * No data ready.  If there's already a select() waiting on this
	 * minor device then this is a collision.  This shouldn't happen
	 * because minors really should not be shared, but if a process
	 * forks while one of these is open, it is possible that both
	 * processes could select on the same descriptor.
	 */
	if (d->bd_selproc && d->bd_selproc->p_wchan == (caddr_t)&selwait)
		d->bd_selcoll = 1;
	else
		d->bd_selproc = p;
#endif
	splx(s);
	return (0);
}

/*
 * Incoming linkage from device drivers.  Process the packet pkt, of length
 * pktlen, which is stored in a contiguous buffer.  The packet is parsed
 * by each process' filter, and if accepted, stashed into the corresponding
 * buffer.
 */
void
bpf_tap(arg, pkt, pktlen)
	caddr_t arg;
	register u_char *pkt;
	register u_int pktlen;
{
	struct bpf_if *bp;
	register struct bpf_d *d;
	register size_t slen;
	/*
	 * Note that the ipl does not have to be raised at this point.
	 * The only problem that could arise here is that if two different
	 * interfaces shared any data.  This is not the case.
	 */
	bp = (struct bpf_if *)arg;
	for (d = bp->bif_dlist; d != 0; d = d->bd_next) {
		++d->bd_rcount;
		slen = bpf_filter(d->bd_filter, pkt, pktlen, pktlen);
		if (slen != 0)
			bpf_catchpacket(d, pkt, pktlen, slen, bcopy);
	}
}

/*
 * Copy data from an mbuf chain into a buffer.  This code is derived
 * from m_copydata in sys/uipc_mbuf.c.
 */
void
bpf_mcopy(src_arg, dst_arg, len)
	const void *src_arg;
	void *dst_arg;
	register size_t len;
{
	register const struct mbuf *m;
	register u_int count;
	u_char *dst;

	m = src_arg;
	dst = dst_arg;
	while (len > 0) {
		if (m == 0)
			panic("bpf_mcopy");
		count = min(m->m_len, len);
		bcopy(mtod(m, caddr_t), (caddr_t)dst, count);
		m = m->m_next;
		dst += count;
		len -= count;
	}
}

/*
 * Incoming linkage from device drivers, when packet is in an mbuf chain.
 */
void
bpf_mtap(arg, m)
	caddr_t arg;
	struct mbuf *m;
{
	struct bpf_if *bp = (struct bpf_if *)arg;
	struct bpf_d *d;
	size_t pktlen, slen;
	struct mbuf *m0;

	if (m == NULL)
		return;

	pktlen = 0;
	for (m0 = m; m0 != 0; m0 = m0->m_next)
		pktlen += m0->m_len;

	for (d = bp->bif_dlist; d != 0; d = d->bd_next) {
		++d->bd_rcount;
		slen = bpf_filter(d->bd_filter, (u_char *)m, pktlen, 0);
		if (slen != 0)
			bpf_catchpacket(d, (u_char *)m, pktlen, slen, bpf_mcopy);
	}
}

/*
 * Move the packet data from interface memory (pkt) into the
 * store buffer.  Return 1 if it's time to wakeup a listener (buffer full),
 * otherwise 0.  "copy" is the routine called to do the actual data
 * transfer.  bcopy is passed in to copy contiguous chunks, while
 * bpf_mcopy is passed in to copy mbuf chains.  In the latter case,
 * pkt is really an mbuf.
 */
void
bpf_catchpacket(d, pkt, pktlen, snaplen, cpfn)
	register struct bpf_d *d;
	register u_char *pkt;
	register size_t pktlen, snaplen;
	register void (*cpfn) __P((const void *, void *, size_t));
{
	register struct bpf_hdr *hp;
	register int totlen, curlen;
	register int hdrlen = d->bd_bif->bif_hdrlen;
	/*
	 * Figure out how many bytes to move.  If the packet is
	 * greater or equal to the snapshot length, transfer that
	 * much.  Otherwise, transfer the whole packet (unless
	 * we hit the buffer size limit).
	 */
	totlen = hdrlen + min(snaplen, pktlen);
	if (totlen > d->bd_bufsize)
		totlen = d->bd_bufsize;

	/*
	 * Round up the end of the previous packet to the next longword.
	 */
	curlen = BPF_WORDALIGN(d->bd_slen);
	if (curlen + totlen > d->bd_bufsize) {
		/*
		 * This packet will overflow the storage buffer.
		 * Rotate the buffers if we can, then wakeup any
		 * pending reads.
		 */
		if (d->bd_fbuf == 0) {
			/*
			 * We haven't completed the previous read yet,
			 * so drop the packet.
			 */
			++d->bd_dcount;
			return;
		}
		ROTATE_BUFFERS(d);
		bpf_wakeup(d);
		curlen = 0;
	}
	else if (d->bd_immediate) {
		/*
		 * Immediate mode is set.  A packet arrived so any
		 * reads should be woken up.
		 */
		bpf_wakeup(d);
	}

	/*
	 * Append the bpf header.
	 */
	hp = (struct bpf_hdr *)(d->bd_sbuf + curlen);
#if BSD >= 199103
	microtime(&hp->bh_tstamp);
#elif defined(sun)
	uniqtime(&hp->bh_tstamp);
#else
	hp->bh_tstamp = time;
#endif
	hp->bh_datalen = pktlen;
	hp->bh_hdrlen = hdrlen;
	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
	(*cpfn)(pkt, (u_char *)hp + hdrlen, (hp->bh_caplen = totlen - hdrlen));
	d->bd_slen = curlen + totlen;

	if (d->bd_rdStart && (d->bd_rtout + d->bd_rdStart < ticks)) {
		/*
		 * we could be selecting on the bpf, and we
		 * may have timeouts set.  We got here by getting
		 * a packet, so wake up the reader.
		 */
		if (d->bd_fbuf) {
			d->bd_rdStart = 0;
			ROTATE_BUFFERS(d);
			bpf_wakeup(d);
			curlen = 0;
		}
	}
}

/*
 * Initialize all nonzero fields of a descriptor.
 */
int
bpf_allocbufs(d)
	register struct bpf_d *d;
{
	d->bd_fbuf = (caddr_t)malloc(d->bd_bufsize, M_DEVBUF, M_WAITOK);
	if (d->bd_fbuf == 0)
		return (ENOBUFS);

	d->bd_sbuf = (caddr_t)malloc(d->bd_bufsize, M_DEVBUF, M_WAITOK);
	if (d->bd_sbuf == 0) {
		free(d->bd_fbuf, M_DEVBUF);
		return (ENOBUFS);
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	return (0);
}

/*
 * Free buffers currently in use by a descriptor.
 * Called on close.
 */
void
bpf_freed(d)
	register struct bpf_d *d;
{
	/*
	 * We don't need to lock out interrupts since this descriptor has
	 * been detached from its interface and it yet hasn't been marked
	 * free.
	 */
	if (d->bd_sbuf != 0) {
		free(d->bd_sbuf, M_DEVBUF);
		if (d->bd_hbuf != 0)
			free(d->bd_hbuf, M_DEVBUF);
		if (d->bd_fbuf != 0)
			free(d->bd_fbuf, M_DEVBUF);
	}
	if (d->bd_filter)
		free((caddr_t)d->bd_filter, M_DEVBUF);

	D_MARKFREE(d);
}

/*
 * Attach an interface to bpf.  driverp is a pointer to a (struct bpf_if *)
 * in the driver's softc; dlt is the link layer type; hdrlen is the fixed
 * size of the link header (variable length headers not yet supported).
 */
void
bpfattach(driverp, ifp, dlt, hdrlen)
	caddr_t *driverp;
	struct ifnet *ifp;
	u_int dlt, hdrlen;
{
	struct bpf_if *bp;
#if BSD < 199103
	static struct bpf_if bpf_ifs[NBPFILTER];
	static int bpfifno;

	bp = (bpfifno < NBPFILTER) ? &bpf_ifs[bpfifno++] : 0;
#else
	bp = (struct bpf_if *)malloc(sizeof(*bp), M_DEVBUF, M_DONTWAIT);
#endif
	if (bp == 0)
		panic("bpfattach");

	bp->bif_dlist = 0;
	bp->bif_driverp = (struct bpf_if **)driverp;
	bp->bif_ifp = ifp;
	bp->bif_dlt = dlt;

	bp->bif_next = bpf_iflist;
	bpf_iflist = bp;

	*bp->bif_driverp = 0;

	/*
	 * Compute the length of the bpf header.  This is not necessarily
	 * equal to SIZEOF_BPF_HDR because we want to insert spacing such
	 * that the network layer header begins on a longword boundary (for
	 * performance reasons and to alleviate alignment restrictions).
	 */
	bp->bif_hdrlen = BPF_WORDALIGN(hdrlen + SIZEOF_BPF_HDR) - hdrlen;

#if 0
	printf("bpf: %s attached\n", ifp->if_xname);
#endif
}

/* Detach an interface from its attached bpf device.  */
void
bpfdetach(ifp)
	struct ifnet *ifp;
{
	struct bpf_if *bp, *nbp, **pbp = &bpf_iflist;
	struct bpf_d *bd;
	int maj, mn;

	for (bp = bpf_iflist; bp; bp = nbp) {
		nbp= bp->bif_next;
		if (bp->bif_ifp == ifp) {
			*pbp = nbp;

			/* Locate the major number. */
			for (maj = 0; maj < nchrdev; maj++)
				if (cdevsw[maj].d_open == bpfopen)
					break;

			for (bd = bp->bif_dlist; bd; bd = bp->bif_dlist)
				/*
				 * Locate the minor number and nuke the vnode
				 * for any open instance.
				 */
				for (mn = 0; mn < NBPFILTER; mn++)
					if (&bpf_dtab[mn] == bd) {
						vdevgone(maj, mn, mn, VCHR);
						break;
					}

#if BSD < 199103
			if (bp == &bpf_ifs[bpfifno - 1])
				bpfifno--;
			else
				printf("bpfdetach: leaked one bpf\n");
#else
			free(bp, M_DEVBUF);
#endif
		}
		pbp = &bp->bif_next;
	}
	ifp->if_bpf = NULL;
}

#if BSD >= 199103
/* XXX This routine belongs in net/if.c. */
/*
 * Set/clear promiscuous mode on interface ifp based on the truth value
 * of pswitch.  The calls are reference counted so that only the first
 * "on" request actually has an effect, as does the final "off" request.
 * Results are undefined if the "off" and "on" requests are not matched.
 */
int
ifpromisc(ifp, pswitch)
	struct ifnet *ifp;
	int pswitch;
{
	struct ifreq ifr;

	if (pswitch) {
		/*
		 * If the device is not configured up, we cannot put it in
		 * promiscuous mode.
		 */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (ENETDOWN);
		if (ifp->if_pcount++ != 0)
			return (0);
		ifp->if_flags |= IFF_PROMISC;
	} else {
		if (--ifp->if_pcount > 0)
			return (0);
		ifp->if_flags &= ~IFF_PROMISC;
		/*
		 * If the device is not configured up, we should not need to
		 * turn off promiscuous mode (device should have turned it
		 * off when interface went down; and will look at IFF_PROMISC
		 * again next time interface comes up).
		 */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (0);
	}
	ifr.ifr_flags = ifp->if_flags;
	return ((*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr));
}
#endif

#if BSD < 199103
/*
 * Allocate some memory for bpf.  This is temporary SunOS support, and
 * is admittedly a hack.
 * If resources unavaiable, return 0.
 */
caddr_t
bpf_alloc(size, canwait)
	register int size;
	register int canwait;
{
	register struct mbuf *m;

	if ((unsigned)size > (MCLBYTES-8))
		return 0;

	MGET(m, canwait, MT_DATA);
	if (m == 0)
		return 0;
	if ((unsigned)size > (MLEN-8)) {
		MCLGET(m);
		if (m->m_len != MCLBYTES) {
			m_freem(m);
			return 0;
		}
	}
	*mtod(m, struct mbuf **) = m;
	return mtod(m, caddr_t) + 8;
}
#endif
