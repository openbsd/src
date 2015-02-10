/*	$OpenBSD: bpf.c,v 1.118 2015/02/10 21:56:10 miod Exp $	*/
/*	$NetBSD: bpf.c,v 1.33 1997/02/21 23:59:35 thorpej Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2010, 2014 Henning Brauer <henning@openbsd.org>
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
 *	@(#)bpf.c	8.2 (Berkeley) 3/28/94
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/rwlock.h>

#include <net/if.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "vlan.h"
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif

#define BPF_BUFSIZE 32768

#define PRINET  26			/* interruptible */

/* from kern/kern_clock.c; incremented each clock tick. */
extern int ticks;

/*
 * The default read buffer size is patchable.
 */
int bpf_bufsize = BPF_BUFSIZE;
int bpf_maxbufsize = BPF_MAXBUFSIZE;

/*
 *  bpf_iflist is the list of interfaces; each corresponds to an ifnet
 *  bpf_d_list is the list of descriptors
 */
struct bpf_if	*bpf_iflist;
LIST_HEAD(, bpf_d) bpf_d_list;

void	bpf_allocbufs(struct bpf_d *);
void	bpf_freed(struct bpf_d *);
void	bpf_ifname(struct ifnet *, struct ifreq *);
void	_bpf_mtap(caddr_t, struct mbuf *, u_int,
	    void (*)(const void *, void *, size_t));
void	bpf_mcopy(const void *, void *, size_t);
int	bpf_movein(struct uio *, u_int, struct mbuf **,
	    struct sockaddr *, struct bpf_insn *);
void	bpf_attachd(struct bpf_d *, struct bpf_if *);
void	bpf_detachd(struct bpf_d *);
int	bpf_setif(struct bpf_d *, struct ifreq *);
int	bpfpoll(dev_t, int, struct proc *);
int	bpfkqfilter(dev_t, struct knote *);
void	bpf_wakeup(struct bpf_d *);
void	bpf_catchpacket(struct bpf_d *, u_char *, size_t, size_t,
	    void (*)(const void *, void *, size_t), struct timeval *);
void	bpf_reset_d(struct bpf_d *);
int	bpf_getdltlist(struct bpf_d *, struct bpf_dltlist *);
int	bpf_setdlt(struct bpf_d *, u_int);

void	filt_bpfrdetach(struct knote *);
int	filt_bpfread(struct knote *, long);

int	bpf_sysctl_locked(int *, u_int, void *, size_t *, void *, size_t);

struct bpf_d *bpfilter_lookup(int);
struct bpf_d *bpfilter_create(int);
void bpfilter_destroy(struct bpf_d *);

int
bpf_movein(struct uio *uio, u_int linktype, struct mbuf **mp,
    struct sockaddr *sockp, struct bpf_insn *filter)
{
	struct mbuf *m;
	struct m_tag *mtag;
	int error;
	u_int hlen;
	u_int len;
	u_int slen;

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
		hlen = ETHER_HDR_LEN;
		break;

	case DLT_IEEE802_11:
	case DLT_IEEE802_11_RADIO:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	case DLT_RAW:
	case DLT_NULL:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	case DLT_LOOP:
		sockp->sa_family = AF_UNSPEC;
		hlen = sizeof(u_int32_t);
		break;

	default:
		return (EIO);
	}

	if (uio->uio_resid > MAXMCLBYTES)
		return (EIO);
	len = uio->uio_resid;

	MGETHDR(m, M_WAIT, MT_DATA);
	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.len = len - hlen;

	if (len > MHLEN) {
		MCLGETI(m, M_WAIT, NULL, len);
		if ((m->m_flags & M_EXT) == 0) {
			error = ENOBUFS;
			goto bad;
		}
	}
	m->m_len = len;
	*mp = m;

	error = uiomovei(mtod(m, caddr_t), len, uio);
	if (error)
		goto bad;

	slen = bpf_filter(filter, mtod(m, u_char *), len, len);
	if (slen < len) {
		error = EPERM;
		goto bad;
	}

	if (m->m_len < hlen) {
		error = EPERM;
		goto bad;
	}
	/*
	 * Make room for link header, and copy it to sockaddr
	 */
	if (hlen != 0) {
		if (linktype == DLT_LOOP) {
			u_int32_t af;

			/* the link header indicates the address family */
			KASSERT(hlen == sizeof(u_int32_t));
			memcpy(&af, m->m_data, hlen);
			sockp->sa_family = ntohl(af);
		} else
			memcpy(sockp->sa_data, m->m_data, hlen);
		m->m_len -= hlen;
		m->m_data += hlen; /* XXX */
	}

	/*
	 * Prepend the data link type as a mbuf tag
	 */
	mtag = m_tag_get(PACKET_TAG_DLT, sizeof(u_int), M_WAIT);
	*(u_int *)(mtag + 1) = linktype;
	m_tag_prepend(m, mtag);

	return (0);
 bad:
	m_freem(m);
	return (error);
}

/*
 * Attach file to the bpf interface, i.e. make d listen on bp.
 * Must be called at splnet.
 */
void
bpf_attachd(struct bpf_d *d, struct bpf_if *bp)
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
bpf_detachd(struct bpf_d *d)
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
		if (error && !(error == EINVAL || error == ENODEV))
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
	d->bd_bif = NULL;
}

/*
 * Reference count access to descriptor buffers
 */
#define D_GET(d) ((d)->bd_ref++)
#define D_PUT(d) bpf_freed(d)

/* ARGSUSED */
void
bpfilterattach(int n)
{
	LIST_INIT(&bpf_d_list);
}

/*
 * Open ethernet device.  Returns ENXIO for illegal minor device number,
 * EBUSY if file is open by another process.
 */
/* ARGSUSED */
int
bpfopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct bpf_d *d;

	/* create on demand */
	if ((d = bpfilter_create(minor(dev))) == NULL)
		return (EBUSY);

	/* Mark "free" and do most initialization. */
	d->bd_bufsize = bpf_bufsize;
	d->bd_sig = SIGIO;

	D_GET(d);

	return (0);
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
/* ARGSUSED */
int
bpfclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct bpf_d *d;
	int s;

	d = bpfilter_lookup(minor(dev));
	s = splnet();
	if (d->bd_bif)
		bpf_detachd(d);
	bpf_wakeup(d);
	D_PUT(d);
	splx(s);

	return (0);
}

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
bpfread(dev_t dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d;
	int error;
	int s;

	d = bpfilter_lookup(minor(dev));
	if (d->bd_bif == NULL)
		return (ENXIO);

	/*
	 * Restrict application to use a buffer the same size as
	 * as kernel buffers.
	 */
	if (uio->uio_resid != d->bd_bufsize)
		return (EINVAL);

	s = splnet();

	D_GET(d);

	/*
	 * If there's a timeout, bd_rdStart is tagged when we start the read.
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
		if (d->bd_bif == NULL) {
			/* interface is gone */
			if (d->bd_slen == 0) {
				D_PUT(d);
				splx(s);
				return (EIO);
			}
			ROTATE_BUFFERS(d);
			break;
		}
		if (d->bd_immediate && d->bd_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 * Rotate the buffers and return what's here.
			 */
			ROTATE_BUFFERS(d);
			break;
		}
		if (d->bd_rtout == -1) {
			/* User requested non-blocking I/O */
			error = EWOULDBLOCK;
		} else {
			if ((d->bd_rdStart + d->bd_rtout) < ticks) {
				error = tsleep((caddr_t)d, PRINET|PCATCH, "bpf",
				    d->bd_rtout);
			} else
				error = EWOULDBLOCK;
		}
		if (error == EINTR || error == ERESTART) {
			D_PUT(d);
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
				D_PUT(d);
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
	error = uiomovei(d->bd_hbuf, d->bd_hlen, uio);

	s = splnet();
	d->bd_fbuf = d->bd_hbuf;
	d->bd_hbuf = NULL;
	d->bd_hlen = 0;

	D_PUT(d);
	splx(s);

	return (error);
}


/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
void
bpf_wakeup(struct bpf_d *d)
{
	wakeup((caddr_t)d);
	if (d->bd_async && d->bd_sig)
		csignal(d->bd_pgid, d->bd_sig,
		    d->bd_siguid, d->bd_sigeuid);

	selwakeup(&d->bd_sel);
	/* XXX */
	d->bd_sel.si_selpid = 0;
}

int
bpfwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d;
	struct ifnet *ifp;
	struct mbuf *m;
	int error, s;
	struct sockaddr_storage dst;

	d = bpfilter_lookup(minor(dev));
	if (d->bd_bif == NULL)
		return (ENXIO);

	ifp = d->bd_bif->bif_ifp;

	if ((ifp->if_flags & IFF_UP) == 0)
		return (ENETDOWN);

	if (uio->uio_resid == 0)
		return (0);

	error = bpf_movein(uio, d->bd_bif->bif_dlt, &m,
	    (struct sockaddr *)&dst, d->bd_wfilter);
	if (error)
		return (error);

	if (m->m_pkthdr.len > ifp->if_mtu) {
		m_freem(m);
		return (EMSGSIZE);
	}

	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

	if (d->bd_hdrcmplt && dst.ss_family == AF_UNSPEC)
		dst.ss_family = pseudo_AF_HDRCMPLT;

	s = splsoftnet();
	error = (*ifp->if_output)(ifp, m, (struct sockaddr *)&dst, NULL);
	splx(s);
	/*
	 * The driver frees the mbuf.
	 */
	return (error);
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the
 * receive and drop counts.  Should be called at splnet.
 */
void
bpf_reset_d(struct bpf_d *d)
{
	if (d->bd_hbuf) {
		/* Free the hold buffer. */
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = NULL;
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
 *  BIOCGDLTLIST	Get supported link layer types.
 *  BIOCGDLT		Get link layer type.
 *  BIOCSDLT		Set link layer type.
 *  BIOCGETIF		Get interface name.
 *  BIOCSETIF		Set interface.
 *  BIOCSRTIMEOUT	Set read timeout.
 *  BIOCGRTIMEOUT	Get read timeout.
 *  BIOCGSTATS		Get packet stats.
 *  BIOCIMMEDIATE	Set immediate mode.
 *  BIOCVERSION		Get filter language version.
 *  BIOCGHDRCMPLT	Get "header already complete" flag
 *  BIOCSHDRCMPLT	Set "header already complete" flag
 */
/* ARGSUSED */
int
bpfioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct bpf_d *d;
	int s, error = 0;

	d = bpfilter_lookup(minor(dev));
	if (d->bd_locked && suser(p, 0) != 0) {
		/* list of allowed ioctls when locked and not root */
		switch (cmd) {
		case BIOCGBLEN:
		case BIOCFLUSH:
		case BIOCGDLT:
		case BIOCGDLTLIST:
		case BIOCGETIF:
		case BIOCGRTIMEOUT:
		case BIOCGSTATS:
		case BIOCVERSION:
		case BIOCGRSIG:
		case BIOCGHDRCMPLT:
		case FIONREAD:
		case BIOCLOCK:
		case BIOCSRTIMEOUT:
		case BIOCIMMEDIATE:
		case TIOCGPGRP:
		case BIOCGDIRFILT:
			break;
		default:
			return (EPERM);
		}
	}

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

			s = splnet();
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
		if (d->bd_bif != NULL)
			error = EINVAL;
		else {
			u_int size = *(u_int *)addr;

			if (size > bpf_maxbufsize)
				*(u_int *)addr = size = bpf_maxbufsize;
			else if (size < BPF_MINBUFSIZE)
				*(u_int *)addr = size = BPF_MINBUFSIZE;
			d->bd_bufsize = size;
		}
		break;

	/*
	 * Set link layer read filter.
	 */
	case BIOCSETF:
		error = bpf_setf(d, (struct bpf_program *)addr, 0);
		break;

	/*
	 * Set link layer write filter.
	 */
	case BIOCSETWF:
		error = bpf_setf(d, (struct bpf_program *)addr, 1);
		break;

	/*
	 * Flush read packet buffer.
	 */
	case BIOCFLUSH:
		s = splnet();
		bpf_reset_d(d);
		splx(s);
		break;

	/*
	 * Put interface into promiscuous mode.
	 */
	case BIOCPROMISC:
		if (d->bd_bif == NULL) {
			/*
			 * No interface attached yet.
			 */
			error = EINVAL;
			break;
		}
		s = splnet();
		if (d->bd_promisc == 0) {
			error = ifpromisc(d->bd_bif->bif_ifp, 1);
			if (error == 0)
				d->bd_promisc = 1;
		}
		splx(s);
		break;

	/*
	 * Get a list of supported device parameters.
	 */
	case BIOCGDLTLIST:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_getdltlist(d, (struct bpf_dltlist *)addr);
		break;

	/*
	 * Get device parameters.
	 */
	case BIOCGDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			*(u_int *)addr = d->bd_bif->bif_dlt;
		break;

	/*
	 * Set device parameters.
	 */
	case BIOCSDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_setdlt(d, *(u_int *)addr);
		break;

	/*
	 * Set interface name.
	 */
	case BIOCGETIF:
		if (d->bd_bif == NULL)
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

	case BIOCGHDRCMPLT:	/* get "header already complete" flag */
		*(u_int *)addr = d->bd_hdrcmplt;
		break;

	case BIOCSHDRCMPLT:	/* set "header already complete" flag */
		d->bd_hdrcmplt = *(u_int *)addr ? 1 : 0;
		break;

	case BIOCLOCK:		/* set "locked" flag (no reset) */
		d->bd_locked = 1;
		break;

	case BIOCGFILDROP:	/* get "filter-drop" flag */
		*(u_int *)addr = d->bd_fildrop;
		break;

	case BIOCSFILDROP:	/* set "filter-drop" flag */
		d->bd_fildrop = *(u_int *)addr ? 1 : 0;
		break;

	case BIOCGDIRFILT:	/* get direction filter */
		*(u_int *)addr = d->bd_dirfilt;
		break;

	case BIOCSDIRFILT:	/* set direction filter */
		d->bd_dirfilt = (*(u_int *)addr) &
		    (BPF_DIRECTION_IN|BPF_DIRECTION_OUT);
		break;

	case BIOCGQUEUE:	/* get queue */
		*(u_int *)addr = d->bd_queue;
		break;

	case BIOCSQUEUE:	/* set queue */
		d->bd_queue = *(u_int *)addr;
		break;

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
		d->bd_siguid = p->p_ucred->cr_ruid;
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
bpf_setf(struct bpf_d *d, struct bpf_program *fp, int wf)
{
	struct bpf_insn *fcode, *old;
	u_int flen, size;
	int s;

	old = wf ? d->bd_wfilter : d->bd_rfilter;
	if (fp->bf_insns == 0) {
		if (fp->bf_len != 0)
			return (EINVAL);
		s = splnet();
		if (wf)
			d->bd_wfilter = NULL;
		else
			d->bd_rfilter = NULL;
		bpf_reset_d(d);
		splx(s);
		free(old, M_DEVBUF, 0);
		return (0);
	}
	flen = fp->bf_len;
	if (flen > BPF_MAXINSNS)
		return (EINVAL);

	fcode = mallocarray(flen, sizeof(*fp->bf_insns), M_DEVBUF, M_WAITOK);
	size = flen * sizeof(*fp->bf_insns);
	if (copyin((caddr_t)fp->bf_insns, (caddr_t)fcode, size) == 0 &&
	    bpf_validate(fcode, (int)flen)) {
		s = splnet();
		if (wf)
			d->bd_wfilter = fcode;
		else
			d->bd_rfilter = fcode;
		bpf_reset_d(d);
		splx(s);
		free(old, M_DEVBUF, 0);

		return (0);
	}
	free(fcode, M_DEVBUF, size);
	return (EINVAL);
}

/*
 * Detach a file from its current interface (if attached at all) and attach
 * to the interface indicated by the name stored in ifr.
 * Return an errno or 0.
 */
int
bpf_setif(struct bpf_d *d, struct ifreq *ifr)
{
	struct bpf_if *bp, *candidate = NULL;
	int s;

	/*
	 * Look through attached interfaces for the named one.
	 */
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		struct ifnet *ifp = bp->bif_ifp;

		if (ifp == NULL ||
		    strcmp(ifp->if_xname, ifr->ifr_name) != 0)
			continue;

		if (candidate == NULL || candidate->bif_dlt > bp->bif_dlt)
			candidate = bp;
	}

	if (candidate != NULL) {
		/*
		 * Allocate the packet buffers if we need to.
		 * If we're already attached to requested interface,
		 * just flush the buffer.
		 */
		if (d->bd_sbuf == NULL)
			bpf_allocbufs(d);
		s = splnet();
		if (candidate != d->bd_bif) {
			if (d->bd_bif)
				/*
				 * Detach if attached to something else.
				 */
				bpf_detachd(d);

			bpf_attachd(d, candidate);
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
bpf_ifname(struct ifnet *ifp, struct ifreq *ifr)
{
	bcopy(ifp->if_xname, ifr->ifr_name, IFNAMSIZ);
}

/*
 * Support for poll() system call
 */
int
bpfpoll(dev_t dev, int events, struct proc *p)
{
	struct bpf_d *d;
	int s, revents;

	/*
	 * An imitation of the FIONREAD ioctl code.
	 */
	d = bpfilter_lookup(minor(dev));

	/*
	 * XXX The USB stack manages it to trigger some race condition
	 * which causes bpfilter_lookup to return NULL when a USB device
	 * gets detached while it is up and has an open bpf handler (e.g.
	 * dhclient).  We still should recheck if we can fix the root
	 * cause of this issue.
	 */
	if (d == NULL)
		return (POLLERR);

	/* Always ready to write data */
	revents = events & (POLLOUT | POLLWRNORM);

	if (events & (POLLIN | POLLRDNORM)) {
		s = splnet();
		if (d->bd_hlen != 0 || (d->bd_immediate && d->bd_slen != 0))
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			/*
			 * if there's a timeout, mark the time we
			 * started waiting.
			 */
			if (d->bd_rtout != -1 && d->bd_rdStart == 0)
				d->bd_rdStart = ticks;
			selrecord(p, &d->bd_sel);
		}
		splx(s);
	}
	return (revents);
}

struct filterops bpfread_filtops =
	{ 1, NULL, filt_bpfrdetach, filt_bpfread };

int
bpfkqfilter(dev_t dev, struct knote *kn)
{
	struct bpf_d *d;
	struct klist *klist;
	int s;

	d = bpfilter_lookup(minor(dev));
	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &d->bd_sel.si_note;
		kn->kn_fop = &bpfread_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = d;

	s = splnet();
	D_GET(d);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	if (d->bd_rtout != -1 && d->bd_rdStart == 0)
		d->bd_rdStart = ticks;
	splx(s);

	return (0);
}

void
filt_bpfrdetach(struct knote *kn)
{
	struct bpf_d *d = kn->kn_hook;
	int s;

	s = splnet();
	SLIST_REMOVE(&d->bd_sel.si_note, kn, knote, kn_selnext);
	D_PUT(d);
	splx(s);
}

int
filt_bpfread(struct knote *kn, long hint)
{
	struct bpf_d *d = kn->kn_hook;

	kn->kn_data = d->bd_hlen;
	if (d->bd_immediate)
		kn->kn_data += d->bd_slen;
	return (kn->kn_data > 0);
}

/*
 * Incoming linkage from device drivers.  Process the packet pkt, of length
 * pktlen, which is stored in a contiguous buffer.  The packet is parsed
 * by each process' filter, and if accepted, stashed into the corresponding
 * buffer.
 */
int
bpf_tap(caddr_t arg, u_char *pkt, u_int pktlen, u_int direction)
{
	struct bpf_if *bp;
	struct bpf_d *d;
	size_t slen;
	struct timeval tv;
	int drop = 0, gottime = 0;

	/*
	 * Note that the ipl does not have to be raised at this point.
	 * The only problem that could arise here is that if two different
	 * interfaces shared any data.  This is not the case.
	 */
	bp = (struct bpf_if *)arg;
	for (d = bp->bif_dlist; d != NULL; d = d->bd_next) {
		++d->bd_rcount;
		if ((direction & d->bd_dirfilt) != 0)
			slen = 0;
		else
			slen = bpf_filter(d->bd_rfilter, pkt, pktlen, pktlen);
		if (slen != 0) {
			if (!gottime++)
				microtime(&tv);
			bpf_catchpacket(d, pkt, pktlen, slen, bcopy, &tv);
			if (d->bd_fildrop)
				drop++;
		}
	}

	return (drop);
}

/*
 * Copy data from an mbuf chain into a buffer.  This code is derived
 * from m_copydata in sys/uipc_mbuf.c.
 */
void
bpf_mcopy(const void *src_arg, void *dst_arg, size_t len)
{
	const struct mbuf *m;
	u_int count;
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
 * like bpf_mtap, but copy fn can be given. used by various bpf_mtap*
 */
void
_bpf_mtap(caddr_t arg, struct mbuf *m, u_int direction,
    void (*cpfn)(const void *, void *, size_t))
{
	struct bpf_if *bp = (struct bpf_if *)arg;
	struct bpf_d *d;
	size_t pktlen, slen;
	struct mbuf *m0;
	struct timeval tv;
	int gottime = 0;

	if (m == NULL)
		return;

	if (cpfn == NULL)
		cpfn = bpf_mcopy;

	pktlen = 0;
	for (m0 = m; m0 != NULL; m0 = m0->m_next)
		pktlen += m0->m_len;

	for (d = bp->bif_dlist; d != NULL; d = d->bd_next) {
		++d->bd_rcount;
		if ((direction & d->bd_dirfilt) != 0)
			slen = 0;
		else if (d->bd_queue && m->m_pkthdr.pf.qid != d->bd_queue)
			slen = 0;
		else
			slen = bpf_filter(d->bd_rfilter, (u_char *)m,
			    pktlen, 0);

		if (slen == 0)
			continue;

		if (!gottime++)
			microtime(&tv);
		bpf_catchpacket(d, (u_char *)m, pktlen, slen, cpfn, &tv);
		if (d->bd_fildrop)
			m->m_flags |= M_FILDROP;
	}
}

/*
 * Incoming linkage from device drivers, when packet is in an mbuf chain.
 */
void
bpf_mtap(caddr_t arg, struct mbuf *m, u_int direction)
{
	_bpf_mtap(arg, m, direction, NULL);
}

/*
 * Incoming linkage from device drivers, where we have a mbuf chain
 * but need to prepend some arbitrary header from a linear buffer.
 *
 * Con up a minimal dummy header to pacify bpf.  Allocate (only) a
 * struct m_hdr on the stack.  This is safe as bpf only reads from the
 * fields in this header that we initialize, and will not try to free
 * it or keep a pointer to it.
 */
void
bpf_mtap_hdr(caddr_t arg, caddr_t data, u_int dlen, struct mbuf *m,
    u_int direction, void (*cpfn)(const void *, void *, size_t))
{
	struct m_hdr	 mh;
	struct mbuf	*m0;

	if (dlen > 0) {
		mh.mh_flags = 0;
		mh.mh_next = m;
		mh.mh_len = dlen;
		mh.mh_data = data;
		m0 = (struct mbuf *)&mh;
	} else 
		m0 = m;

	_bpf_mtap(arg, m0, direction, cpfn);
	if (m0 != m)
		m->m_flags |= m0->m_flags & M_FILDROP;
}

/*
 * Incoming linkage from device drivers, where we have a mbuf chain
 * but need to prepend the address family.
 *
 * Con up a minimal dummy header to pacify bpf.  We allocate (only) a
 * struct m_hdr on the stack.  This is safe as bpf only reads from the
 * fields in this header that we initialize, and will not try to free
 * it or keep a pointer to it.
 */
void
bpf_mtap_af(caddr_t arg, u_int32_t af, struct mbuf *m, u_int direction)
{
	u_int32_t    afh;

	afh = htonl(af);
	bpf_mtap_hdr(arg, (caddr_t)&afh, sizeof(afh), m, direction, NULL);
}

/*
 * Incoming linkage from device drivers, where we have a mbuf chain
 * but need to prepend a VLAN encapsulation header.
 *
 * Con up a minimal dummy header to pacify bpf.  Allocate (only) a
 * struct m_hdr on the stack.  This is safe as bpf only reads from the
 * fields in this header that we initialize, and will not try to free
 * it or keep a pointer to it.
 */
void
bpf_mtap_ether(caddr_t arg, struct mbuf *m, u_int direction)
{
#if NVLAN > 0
	struct ether_vlan_header evh;

	if ((m->m_flags & M_VLANTAG) == 0)
#endif
	{
		bpf_mtap(arg, m, direction);
		return;
	}

#if NVLAN > 0
	bcopy(mtod(m, char *), &evh, ETHER_HDR_LEN);
	evh.evl_proto = evh.evl_encap_proto;
	evh.evl_encap_proto = htons(ETHERTYPE_VLAN);
	evh.evl_tag = htons(m->m_pkthdr.ether_vtag);
	m->m_len -= ETHER_HDR_LEN;
	m->m_data += ETHER_HDR_LEN;

	bpf_mtap_hdr(arg, (caddr_t)&evh, sizeof(evh), m, direction, NULL);

	m->m_len += ETHER_HDR_LEN;
	m->m_data -= ETHER_HDR_LEN;
#endif
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
bpf_catchpacket(struct bpf_d *d, u_char *pkt, size_t pktlen, size_t snaplen,
    void (*cpfn)(const void *, void *, size_t), struct timeval *tv)
{
	struct bpf_hdr *hp;
	int totlen, curlen;
	int hdrlen = d->bd_bif->bif_hdrlen;

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
		if (d->bd_fbuf == NULL) {
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

	/*
	 * Append the bpf header.
	 */
	hp = (struct bpf_hdr *)(d->bd_sbuf + curlen);
	hp->bh_tstamp.tv_sec = tv->tv_sec;
	hp->bh_tstamp.tv_usec = tv->tv_usec;
	hp->bh_datalen = pktlen;
	hp->bh_hdrlen = hdrlen;
	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
	(*cpfn)(pkt, (u_char *)hp + hdrlen, (hp->bh_caplen = totlen - hdrlen));
	d->bd_slen = curlen + totlen;

	if (d->bd_immediate) {
		/*
		 * Immediate mode is set.  A packet arrived so any
		 * reads should be woken up.
		 */
		bpf_wakeup(d);
	}

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
		}
	}
}

/*
 * Initialize all nonzero fields of a descriptor.
 */
void
bpf_allocbufs(struct bpf_d *d)
{
	d->bd_fbuf = malloc(d->bd_bufsize, M_DEVBUF, M_WAITOK);
	d->bd_sbuf = malloc(d->bd_bufsize, M_DEVBUF, M_WAITOK);
	d->bd_slen = 0;
	d->bd_hlen = 0;
}

/*
 * Free buffers currently in use by a descriptor
 * when the reference count drops to zero.
 */
void
bpf_freed(struct bpf_d *d)
{
	if (--d->bd_ref > 0)
		return;

	free(d->bd_sbuf, M_DEVBUF, 0);
	free(d->bd_hbuf, M_DEVBUF, 0);
	free(d->bd_fbuf, M_DEVBUF, 0);
	free(d->bd_rfilter, M_DEVBUF, 0);
	free(d->bd_wfilter, M_DEVBUF, 0);

	bpfilter_destroy(d);
}

/*
 * Attach an interface to bpf.  driverp is a pointer to a (struct bpf_if *)
 * in the driver's softc; dlt is the link layer type; hdrlen is the fixed
 * size of the link header (variable length headers not yet supported).
 */
void
bpfattach(caddr_t *driverp, struct ifnet *ifp, u_int dlt, u_int hdrlen)
{
	struct bpf_if *bp;

	if ((bp = malloc(sizeof(*bp), M_DEVBUF, M_NOWAIT)) == NULL)
		panic("bpfattach");
	bp->bif_dlist = 0;
	bp->bif_driverp = (struct bpf_if **)driverp;
	bp->bif_ifp = ifp;
	bp->bif_dlt = dlt;

	bp->bif_next = bpf_iflist;
	bpf_iflist = bp;

	*bp->bif_driverp = NULL;

	/*
	 * Compute the length of the bpf header.  This is not necessarily
	 * equal to SIZEOF_BPF_HDR because we want to insert spacing such
	 * that the network layer header begins on a longword boundary (for
	 * performance reasons and to alleviate alignment restrictions).
	 */
	bp->bif_hdrlen = BPF_WORDALIGN(hdrlen + SIZEOF_BPF_HDR) - hdrlen;
}

/* Detach an interface from its attached bpf device.  */
void
bpfdetach(struct ifnet *ifp)
{
	struct bpf_if *bp, *nbp, **pbp = &bpf_iflist;
	struct bpf_d *bd;
	int maj;

	for (bp = bpf_iflist; bp; bp = nbp) {
		nbp= bp->bif_next;
		if (bp->bif_ifp == ifp) {
			*pbp = nbp;

			/* Locate the major number. */
			for (maj = 0; maj < nchrdev; maj++)
				if (cdevsw[maj].d_open == bpfopen)
					break;

			for (bd = bp->bif_dlist; bd; bd = bp->bif_dlist) {
				struct bpf_d *d;

				/*
				 * Locate the minor number and nuke the vnode
				 * for any open instance.
				 */
				LIST_FOREACH(d, &bpf_d_list, bd_list)
					if (d == bd) {
						vdevgone(maj, d->bd_unit,
						    d->bd_unit, VCHR);
						break;
					}
			}

			free(bp, M_DEVBUF, 0);
		} else
			pbp = &bp->bif_next;
	}
	ifp->if_bpf = NULL;
}

int
bpf_sysctl_locked(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	int newval;
	int error;

	switch (name[0]) {
	case NET_BPF_BUFSIZE:
		newval = bpf_bufsize;
		error = sysctl_int(oldp, oldlenp, newp, newlen, &newval);
		if (error)
			return (error);
		if (newval < BPF_MINBUFSIZE || newval > bpf_maxbufsize)
			return (EINVAL);
		bpf_bufsize = newval;
		break;
	case NET_BPF_MAXBUFSIZE:
		newval = bpf_maxbufsize;
		error = sysctl_int(oldp, oldlenp, newp, newlen, &newval);
		if (error)
			return (error);
		if (newval < BPF_MINBUFSIZE)
			return (EINVAL);
		bpf_maxbufsize = newval;
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

int
bpf_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	static struct rwlock bpf_sysctl_lk = RWLOCK_INITIALIZER("bpfsz");
	int flags = RW_INTR;
	int error;

	if (namelen != 1)
		return (ENOTDIR);

	flags |= (newp == NULL) ? RW_READ : RW_WRITE;

	error = rw_enter(&bpf_sysctl_lk, flags);
	if (error != 0)
		return (error);

	error = bpf_sysctl_locked(name, namelen, oldp, oldlenp, newp, newlen);

	rw_exit(&bpf_sysctl_lk);

	return (error);
}

struct bpf_d *
bpfilter_lookup(int unit)
{
	struct bpf_d *bd;

	LIST_FOREACH(bd, &bpf_d_list, bd_list)
		if (bd->bd_unit == unit)
			return (bd);
	return (NULL);
}

struct bpf_d *
bpfilter_create(int unit)
{
	struct bpf_d *bd;

	if ((bd = bpfilter_lookup(unit)) != NULL)
		return (NULL);
	if ((bd = malloc(sizeof(*bd), M_DEVBUF, M_NOWAIT|M_ZERO)) != NULL) {
		bd->bd_unit = unit;
		LIST_INSERT_HEAD(&bpf_d_list, bd, bd_list);
	}
	return (bd);
}

void
bpfilter_destroy(struct bpf_d *bd)
{
	LIST_REMOVE(bd, bd_list);
	free(bd, M_DEVBUF, sizeof(*bd));
}

/*
 * Get a list of available data link type of the interface.
 */
int
bpf_getdltlist(struct bpf_d *d, struct bpf_dltlist *bfl)
{
	int n, error;
	struct ifnet *ifp;
	struct bpf_if *bp;

	ifp = d->bd_bif->bif_ifp;
	n = 0;
	error = 0;
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (bp->bif_ifp != ifp)
			continue;
		if (bfl->bfl_list != NULL) {
			if (n >= bfl->bfl_len)
				return (ENOMEM);
			error = copyout(&bp->bif_dlt,
			    bfl->bfl_list + n, sizeof(u_int));
			if (error)
				break;
		}
		n++;
	}

	bfl->bfl_len = n;
	return (error);
}

/*
 * Set the data link type of a BPF instance.
 */
int
bpf_setdlt(struct bpf_d *d, u_int dlt)
{
	int s;
	struct ifnet *ifp;
	struct bpf_if *bp;

	if (d->bd_bif->bif_dlt == dlt)
		return (0);
	ifp = d->bd_bif->bif_ifp;
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (bp->bif_ifp == ifp && bp->bif_dlt == dlt)
			break;
	}
	if (bp == NULL)
		return (EINVAL);
	s = splnet();
	bpf_detachd(d);
	bpf_attachd(d, bp);
	bpf_reset_d(d);
	splx(s);
	return (0);
}
