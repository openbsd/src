/*	$OpenBSD: rx_pkt.c,v 1.1.1.1 1998/09/14 21:53:16 art Exp $	*/
#include "rx_locl.h"

RCSID("$KTH: rx_pkt.c,v 1.9 1998/03/14 13:40:22 assar Exp $");

struct rx_packet *rx_mallocedP = 0;
struct rx_cbuf *rx_mallocedC = 0;

char cml_version_number[4711];	       /* XXX - What is this? */
extern int (*rx_almostSent) ();

/*
 * some rules about packets:
 * 1.  When a packet is allocated, the final iov_buf contains room for
 * a security trailer, but iov_len masks that fact.  If the security
 * package wants to add the trailer, it may do so, and then extend
 * iov_len appropriately.  For this reason, packet's niovecs and
 * iov_len fields should be accurate before calling PreparePacket.
 */

/* 
 * Preconditions:
 *        all packet buffers (iov_base) are integral multiples of
 *	  the word size.
 *        offset is an integral multiple of the word size.
 */

long 
rx_SlowGetLong(struct rx_packet *packet, int offset)
{
    int i, l;

    for (l = 0, i = 1; i < packet->niovecs; i++) {
	if (l + packet->wirevec[i].iov_len > offset) {
	    return *((u_int32_t *)
		     ((char *)packet->wirevec[i].iov_base + (offset - l)));
	}
	l += packet->wirevec[i].iov_len;
    }

    return 0;
}

/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the word
 *	  size.
 *        offset is an integral multiple of the word size.
 */
long 
rx_SlowPutLong(struct rx_packet *packet, int offset, long data)
{
    int i, l;

    for (l = 0, i = 1; i < packet->niovecs; i++) {
	if (l + packet->wirevec[i].iov_len > offset) {
	    *((u_int32_t *) ((char *)packet->wirevec[i].iov_base + (offset - l))) = data;
	    return 0;
	}
	l += packet->wirevec[i].iov_len;
    }

    return 0;
}

/*
 * Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the
 *        word size.
 *        offset is an integral multiple of the word size.
 * Packet Invariants:
 *         all buffers are contiguously arrayed in the iovec from 0..niovecs-1
 */
size_t
rx_SlowReadPacket(struct rx_packet *packet, int offset, int resid, void *out)
{
    int i;
    unsigned char *p = out;
    size_t bytes;
    
    for(i = 1; (i < packet->niovecs) && (offset + (ssize_t)resid > 0); i++) {
	if(offset < packet->wirevec[i].iov_len) {
	    /* at this point the intersection of this iovec and 
	       [offset, offset+resid) is non-empty, so we can copy 
	       min(base + len, base + offset + resid) - 
	       max(base, base + offset) bytes
	       */
	    bytes = min(packet->wirevec[i].iov_len, offset + resid) - 
		    max(offset, 0);
	    memcpy(p,
		   (char *)packet->wirevec[i].iov_base + max(offset, 0),
		   bytes);
	    p += bytes;
	}
	offset -= packet->wirevec[i].iov_len;
    }
    return p - (unsigned char *)out;
}


/*
 * Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the
 *        word size.
 *        offset is an integral multiple of the word size.
 */
size_t
rx_SlowWritePacket(struct rx_packet *packet, int offset, int resid, void *in)
{
    int i;
    unsigned char *p = in;
    size_t bytes;
    
    for(i = 1; i < RX_MAXWVECS && offset + resid > 0; i++) {
	if(i >= packet->niovecs)
	    if(rxi_AllocDataBuf(packet, resid))
		break;
	if(offset < packet->wirevec[i].iov_len) {
	    /* at this point the intersection of this iovec and 
	       [offset, offset+resid) is non-empty, so we can copy 
	       min(base + len, base + offset + resid) - 
	       max(base, base + offset) bytes
	       */
	    bytes = min(packet->wirevec[i].iov_len, offset + resid) - 
		    max(offset, 0);
	    memcpy((char *)(packet->wirevec[i].iov_base) + max(offset, 0),
		   p, bytes);
	    p += bytes;
	}
	offset -= packet->wirevec[i].iov_len;
    }
    return p - (unsigned char *)in;
}


static void 
freeCBuf(struct rx_cbuf *c)
{
    SPLVAR;

    dpf(("Free cbuf %x\n", c));
    NETPRI;

    MObtainWriteLock(&rx_freePktQ_lock);

    queue_Append(&rx_freeCbufQueue, c);
    rx_nFreeCbufs++;

    MReleaseWriteLock(&rx_freePktQ_lock);
    USERPRI;

    return;
}

static struct rx_cbuf *
allocCBuf(void)
{
    struct rx_cbuf *c;

    SPLVAR;

    NETPRI;
    MObtainWriteLock(&rx_freePktQ_lock);

    if (queue_IsEmpty(&rx_freeCbufQueue)) {
	c = NULL;
	rxi_NeedMoreCbufs = TRUE;
	goto done;
    }
    rx_nFreeCbufs--;
    c = queue_First(&rx_freeCbufQueue, rx_cbuf);

    dpf(("Alloc cb %x\n", c));

    queue_Remove(c);

done:
    MReleaseWriteLock(&rx_freePktQ_lock);

    USERPRI;
    return c;
}

/* Allocate more CBufs iff we need them */
/*
 * In kernel, can't page in memory with interrupts disabled, so we
 * don't use the event mechanism.
 */
void 
rx_CheckCbufs(unsigned long when)
/* time when I should be called next */
{
    struct clock now;

    clock_GetTime(&now);

    if (rxi_NeedMoreCbufs) {
	rxi_MoreCbufs(rx_Window);
    }
#ifndef KERNEL
    now.sec += RX_CBUF_TIME;
    rxevent_Post(&now, rx_CheckCbufs, (void *)now.sec, NULL);
#endif
}

/*
 * this one is kind of awful.
 * In rxkad, the packet has been all shortened, and everything, ready for
 * sending.  All of a sudden, we discover we need some of that space back.
 * This isn't terribly general, because it knows that the packets are only
 * rounded up to the EBS (userdata + security header).
 */
int 
rxi_RoundUpPacket(struct rx_packet *p, unsigned int nb)
{
    int i;

    i = p->niovecs - 1;
    if (p->wirevec[i].iov_base == (caddr_t) p->localdata) {
	if (p->wirevec[i].iov_len <= RX_FIRSTBUFFERSIZE - nb) {
	    p->wirevec[i].iov_len += nb;
	    return 0;
	}
    } else {
	if (p->wirevec[i].iov_len <= RX_CBUFFERSIZE - nb) {
	    p->wirevec[i].iov_len += nb;
	    return 0;
	}
    }

    return 0;
}

/* get sufficient space to store nb bytes of data (or more), and hook
 * it into the supplied packet.  Return nbytes<=0 if successful, otherwise
 * returns the number of bytes >0 which it failed to come up with.
 * Don't need to worry about locking on packet, since only
 * one thread can manipulate one at a time. Locking on cbufs is handled
 * by allocCBuf */
/* MTUXXX don't need to go throught the for loop if we can trust niovecs */
int 
rxi_AllocDataBuf(struct rx_packet *p, int nb)
{
    int i;

    for (i = 0; nb > 0 && i < RX_MAXWVECS; i++) {
	if (p->wirevec[i].iov_base)
	    continue;

	switch (i) {
	case 1:
	    p->wirevec[i].iov_len = RX_FIRSTBUFFERSIZE;
	    p->wirevec[i].iov_base = (caddr_t) p->localdata;
	    nb -= RX_FIRSTBUFFERSIZE;
	    p->length += RX_FIRSTBUFFERSIZE;
	    break;
	default:
	    {
		struct rx_cbuf *cb;

		if ((cb = allocCBuf()) != NULL) {
		    p->wirevec[i].iov_base = (caddr_t) cb->data;
		    p->wirevec[i].iov_len = RX_CBUFFERSIZE;
		    nb -= RX_CBUFFERSIZE;
		    p->length += RX_CBUFFERSIZE;
		    p->niovecs++;
		} else
		    i = RX_MAXWVECS;
	    }
	    break;
	}
    }

    return nb;
}

int 
rxi_FreeDataBufs(struct rx_packet *p, int first)
{
    int i;

    if (first != 1)		       /* MTUXXX */
	osi_Panic("FreeDataBufs 1: first must be 1");

    for (i = first; i < RX_MAXWVECS; i++) {
	if (p->wirevec[i].iov_base) {
	    if (p->wirevec[i].iov_base != (caddr_t) p->localdata) {
		freeCBuf((struct rx_cbuf *)((char *)p->wirevec[i].iov_base - 
					    sizeof(struct rx_queue)));
	    }
	    p->wirevec[i].iov_base = NULL;
	} else if (i == 1)	       /* MTUXXX */
	    osi_Panic("FreeDataBufs 4: vec 1 must not be NULL");

	p->wirevec[i].iov_len = 0;
    }
    p->length = 0;

    return 0;
}

/*
 * add n more fragment buffers (continuation buffers)
 * Must be called at user priority or will crash RS/6000s 
 */
void 
rxi_MoreCbufs(int n)
{
    struct rx_cbuf *c, *e;
    int getme;

    SPLVAR;

    if (!n)
	return;

    getme = n * sizeof(struct rx_cbuf);
    c = rx_mallocedC = (struct rx_cbuf *) osi_Alloc(getme);
    if (!c)
	return;

    memset(c, 0, getme);

    PIN(c, getme);		       /* XXXXX */
    NETPRI;
    MObtainWriteLock(&rx_freePktQ_lock);

    for (e = c + n; c < e; c++) {
	queue_Append(&rx_freeCbufQueue, c);
    }
    rxi_NeedMoreCbufs = FALSE;
    rx_nFreeCbufs += n;
    rx_nCbufs += n;

    MReleaseWriteLock(&rx_freePktQ_lock);
    USERPRI;

    return;
}

/* Add more packet buffers */
void
rxi_MorePackets(int apackets)
{
    struct rx_packet *p, *e;
    int getme;

    SPLVAR;

    getme = apackets * sizeof(struct rx_packet);
    p = rx_mallocedP = (struct rx_packet *) osi_Alloc(getme);

    PIN(p, getme);		       /* XXXXX */
    memset((char *) p, 0, getme);
    NETPRI;
    MObtainWriteLock(&rx_freePktQ_lock);

    for (e = p + apackets; p < e; p++) {
	p->wirevec[0].iov_base = (char *) (p->wirehead);
	p->wirevec[0].iov_len = RX_HEADER_SIZE;
	p->wirevec[1].iov_base = (char *) (p->localdata);
	p->wirevec[1].iov_len = RX_FIRSTBUFFERSIZE;
	p->niovecs = 2;

	queue_Append(&rx_freePacketQueue, p);
    }
    rx_nFreePackets += apackets;

    MReleaseWriteLock(&rx_freePktQ_lock);
    USERPRI;

    /*
     * allocate enough cbufs that 1/4 of the packets will be able to hold
     * maximal amounts of data
     */
/* MTUXXX enable this -- currently disabled for testing
  rxi_MoreCbufs((apackets/4)*(rx_maxReceiveSize - RX_FIRSTBUFFERSIZE)/RX_CBUFFERSIZE);
*/

}

void 
rxi_FreeAllPackets(void)
{
    /* must be called at proper interrupt level, etcetera */
    /* MTUXXX need to free all Cbufs */
    osi_Free(rx_mallocedP, (rx_Window + 2) * sizeof(struct rx_packet));
    UNPIN(rx_mallocedP, (rx_Window + 2) * sizeof(struct rx_packet));

    return;
}


/*
 * In the packet freeing routine below, the assumption is that
 * we want all of the packets to be used equally frequently, so that we
 * don't get packet buffers paging out.  It would be just as valid to
 * assume that we DO want them to page out if not many are being used.
 * In any event, we assume the former, and append the packets to the end
 * of the free list. 
 */
/*
 * This explanation is bogus.  The free list doesn't remain in any kind of
 * useful order for long: the packets in use get pretty much randomly scattered
 * across all the pages.  In order to permit unused {packets,bufs} to page
 * out, they must be stored so that packets which are adjacent in memory are
 * adjacent in the free list.  An array springs rapidly to mind.
 */

/* 
 * Free the packet p.  P is assumed not to be on any queue, i.e.
 * remove it yourself first if you call this routine.
 */
void
rxi_FreePacket(struct rx_packet *p)
{
    SPLVAR;
    dpf(("Free %x\n", p));

    rxi_FreeDataBufs(p, 1);	       /* this gets the locks below, so must
				        * call it first */

    NETPRI;

    MObtainWriteLock(&rx_freePktQ_lock);

    rx_nFreePackets++;
    queue_Append(&rx_freePacketQueue, p);
    /* Wakeup anyone waiting for packets */
    rxi_PacketsUnWait();

    MReleaseWriteLock(&rx_freePktQ_lock);
    USERPRI;
}



/*
 * rxi_AllocPacket sets up p->length so it reflects the number of
 * bytes in the packet at this point, **not including** the header.
 * The header is absolutely necessary, besides, this is the way the
 * length field is usually used
 */
struct rx_packet *
rxi_AllocPacket(int class)
{
    struct rx_packet *p;

    if (rxi_OverQuota(class)) {
	rx_stats.noPackets[class]++;
	return NULL;
    }
    rx_stats.packetRequests++;

    MObtainWriteLock(&rx_freePktQ_lock);
    rx_nFreePackets--;
    if (queue_IsEmpty(&rx_freePacketQueue))
	osi_Panic("rxi_AllocPacket error");

    p = queue_First(&rx_freePacketQueue, rx_packet);

    dpf(("Alloc %x, class %d\n",
	 queue_First(&rx_freePacketQueue, rx_packet), class));

    queue_Remove(p);
    MReleaseWriteLock(&rx_freePktQ_lock);

    /*
     * have to do this here because rx_FlushWrite fiddles with the iovs in
     * order to truncate outbound packets.  In the near future, may need to
     * allocate bufs from a static pool here, and/or in AllocSendPacket
     */
    p->wirevec[0].iov_base = (char *) (p->wirehead);
    p->wirevec[0].iov_len = RX_HEADER_SIZE;
    p->wirevec[1].iov_base = (char *) (p->localdata);
    p->wirevec[1].iov_len = RX_FIRSTBUFFERSIZE;
    p->niovecs = 2;
    p->length = RX_FIRSTBUFFERSIZE;
    return p;
}


/*
 * This guy comes up with as many buffers as it {takes,can get} given
 * the MTU for this call. It also sets the packet length before
 * returning.  caution: this is often called at NETPRI
 */
struct rx_packet *
rxi_AllocSendPacket(struct rx_call *call, int want)
{
    struct rx_packet *p = (struct rx_packet *) 0;
    int mud;

    SPLVAR;
    mud = call->conn->maxPacketSize - RX_HEADER_SIZE;

    while (!(call->error)) {
	/* if an error occurred, or we get the packet we want, we're done */
	if ((p = rxi_AllocPacket(RX_PACKET_CLASS_SEND)) != NULL) {

	    want += rx_GetSecurityHeaderSize(rx_ConnectionOf(call)) +
		rx_GetSecurityMaxTrailerSize(rx_ConnectionOf(call));
	    want = MIN(want, mud);

	    if (want > p->length)
		(void) rxi_AllocDataBuf(p, (want - p->length));

	    if (p->length > mud)
		p->length = mud;

	    p->length -= rx_GetSecurityHeaderSize(rx_ConnectionOf(call)) +
		rx_GetSecurityMaxTrailerSize(rx_ConnectionOf(call));

	    if (p->length <= 0) {
		rxi_FreePacket(p);
		p = NULL;
	    }
	    break;
	}

	/*
	 * no error occurred, and we didn't get a packet, so we sleep. At
	 * this point, we assume that packets will be returned sooner or
	 * later, as packets are acknowledged, and so we just wait.
	 */
	NETPRI;
	MUTEX_ENTER(&rx_waitingForPackets_lock);
	rx_waitingForPackets = 1;
	call->flags |= RX_CALL_WAIT_PACKETS;

#ifdef	RX_ENABLE_LOCKS
	cv_wait(&rx_waitingForPackets_cv, &rx_waitingForPackets_lock);
#else
	osi_rxSleep(&rx_waitingForPackets);
#endif
	call->flags &= ~RX_CALL_WAIT_PACKETS;
	MUTEX_EXIT(&rx_waitingForPackets_lock);
	USERPRI;
    }

    return p;
}


#ifndef KERNEL
/* count the number of used FDs */
static int 
CountFDs(int amax)
{
    struct stat tstat;
    int i, code;
    int count;

    count = 0;
    for (i = 0; i < amax; i++) {
	code = fstat(i, &tstat);
	if (code == 0)
	    count++;
    }
    return count;
}

/*
 * This function reads a single packet from the interface into the
 * supplied packet buffer (*p).  Return 0 if the packet is bogus.  The
 * (host,port) of the sender are stored in the supplied variables, and
 * the data length of the packet is stored in the packet structure.
 * The header is decoded.
 */
int 
rxi_ReadPacket(int socket, struct rx_packet *p,
	       u_long *host, u_short *port)
{
    struct sockaddr_in from;
    int nbytes;
    long rlen;
    long tlen;
    long _tlen;
    struct msghdr msg;
    u_int32_t dummy;			       /* was using rlen but had aliasing
				        * problems */

    rx_computelen(p, tlen);
    rx_SetDataSize(p, tlen);	       /* this is the size of the user data
				        * area */

    tlen += RX_HEADER_SIZE;	       /* now this is the size of the entire
				        * packet */
    rlen = rx_maxReceiveSize;	       /* this is what I am advertising.
				        * Only check it once in order to
				        * avoid races.  */
    _tlen = rlen - tlen;
    if (_tlen > 0) {
	_tlen = rxi_AllocDataBuf(p, _tlen);
	if (_tlen >0) {
	    _tlen = rlen - _tlen;
	}
	else _tlen = rlen;
    }
    else _tlen = rlen;
    tlen=(tlen>_tlen)?tlen:_tlen;

    /*
     * set up this one iovec for padding, it's just to make sure that the
     * read doesn't return more data than we expect, and is done to get
     * around our problems caused by the lack of a length field in the rx
     * header.
     */
    p->wirevec[p->niovecs].iov_base = (caddr_t) & dummy;
    p->wirevec[p->niovecs++].iov_len = 4;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (char *) &from;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = p->wirevec;
    msg.msg_iovlen = p->niovecs;
#if 0
    msg.msg_accrights = NULL;
    msg.msg_accrightslen = 0;
#endif
    nbytes = recvmsg(socket, &msg, 0);

    /* restore the vec to its correct state */
    p->wirevec[--p->niovecs].iov_base = NULL;
    p->wirevec[p->niovecs].iov_len = 0;

    p->length = (nbytes - RX_HEADER_SIZE);
    if ((nbytes > tlen) || (nbytes < (int)RX_HEADER_SIZE)) { /* Bogus packet */
	if (nbytes > 0)
	    rxi_MoreCbufs(rx_Window);
	if (nbytes > tlen)
	    rxi_AllocDataBuf(p, nbytes - tlen);
	else if (nbytes < 0 && errno == EWOULDBLOCK)
	    rx_stats.noPacketOnRead++;
	else {
	    rx_stats.bogusPacketOnRead++;
	    rx_stats.bogusHost = from.sin_addr.s_addr;
	    dpf(("B: bogus packet from [%x,%d] nb=%d", from.sin_addr.s_addr,
		 from.sin_port, nbytes));
	}
	return 0;
    } else {
	/* Extract packet header. */
	rxi_DecodePacketHeader(p);

	*host = from.sin_addr.s_addr;
	*port = from.sin_port; 
	if (p->header.type > 0 && p->header.type < RX_N_PACKET_TYPES)
	    rx_stats.packetsRead[p->header.type - 1]++;

	return 1;
    }
}

/* Send a udp datagram */
int 
osi_NetSend(osi_socket socket, char *addr, struct iovec *dvec,
	    int nvecs, int length)
{
    struct msghdr msg;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = dvec;
    msg.msg_iovlen = nvecs;

    msg.msg_name = addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);
#if 0
    msg.msg_accrights = NULL;
    msg.msg_accrightslen = 0;
#endif

    while (sendmsg(socket, &msg, 0) == -1) {
	int err;
	fd_set sfds;

	rx_stats.sendSelects++;
	if (errno != EWOULDBLOCK && errno != ENOBUFS) {
	    (osi_Msg "rx failed to send packet: ");
	    perror("rx_send");	       /* translates the message to English */
	    return 3;
	}

	FD_ZERO(&sfds);
	FD_SET(socket, &sfds);
	while ((err = select(socket + 1, 0, &sfds, 0, 0)) != 1) {
	    if (err >= 0 || errno != EINTR)
		osi_Panic("osi_NetSend: select error %d.%d", err, errno);
	}
    }

    return 0;
}

#else				       /* KERNEL */
/*
 * osi_NetSend is defined in afs/afs_osinet.c
 * message receipt is done in rxk_input or rx_put.
 */

#ifdef AFS_SUN5_ENV
/*
 * Copy an mblock to the contiguous area pointed to by cp.
 * MTUXXX Supposed to skip <off> bytes and copy <len> bytes,
 * but it doesn't really.
 * Returns the number of bytes not transferred.
 * The message is NOT changed.
 */
static int 
cpytoc(mblk_t *mp, int off, int len, char *cp)
{
    int n;

    for (; mp && len > 0; mp = mp->b_cont) {
	if (mp->b_datap->db_type != M_DATA) {
	    return -1;
	}
	n = MIN(len, (mp->b_wptr - mp->b_rptr));
	memcpy(cp, mp->b_rptr, n);
	cp += n;
	len -= n;
	mp->b_rptr += n;
    }
    return (len);
}

/*
 * MTUXXX Supposed to skip <off> bytes and copy <len> bytes,
 * but it doesn't really.
 * This sucks, anyway, do it like m_cpy.... below
 */
static int 
cpytoiovec(mblk_t *mp, int off, int len, 
	   struct iovec *iovs, int niovs)
{
    int m, n, o, t, i;

    for (i = -1, t = 0; i < niovs && mp && len > 0; mp = mp->b_cont) {
	if (mp->b_datap->db_type != M_DATA) {
	    return -1;
	}
	n = MIN(len, (mp->b_wptr - mp->b_rptr));
	len -= n;
	while (n) {
	    if (!t) {
		o = 0;
		i++;
		t = iovs[i].iov_len;
	    }
	    m = MIN(n, t);
	    memcpy(iovs[i].iov_base + o, mp->b_rptr, m);
	    mp->b_rptr += m;
	    o += m;
	    t -= m;
	    n -= m;
	}
    }
    return (len);
}

#define m_cpytoc(a, b, c, d)  cpytoc(a, b, c, d)
#define m_cpytoiovec(a, b, c, d, e) cpytoiovec(a, b, c, d, e)
#else

static int 
m_cpytoiovec(struct mbuf *m, int off, int len, struct iovec iovs[], int niovs)
{
    caddr_t p1, p2;
    unsigned int l1, l2, i, t;

    if (m == NULL || off < 0 || len < 0 || iovs == NULL)
	panic("m_cpytoiovec");	       /* MTUXXX probably don't need this
				        * check */

    while (off && m)
	if (m->m_len <= off) {
	    off -= m->m_len;
	    m = m->m_next;
	    continue;
	} else
	    break;

    if (m == NULL)
	return len;

    p1 = mtod(m, caddr_t) + off;
    l1 = m->m_len - off;
    i = 0;
    p2 = iovs[0].iov_base;
    l2 = iovs[0].iov_len;

    while (len) {
	t = MIN(l1, MIN(l2, (unsigned int) len));
	memcpy(p2, p1, t);
	p1 += t;
	p2 += t;
	l1 -= t;
	l2 -= t;
	len -= t;
	if (!l1) {
	    m = m->m_next;
	    if (!m)
		break;
	    p1 = mtod(m, caddr_t);
	    l1 = m->m_len;
	}
	if (!l2) {
	    if (++i >= niovs)
		break;
	    p2 = iovs[i].iov_base;
	    l2 = iovs[i].iov_len;
	}
    }

    return len;
}

#endif				       /* AFS_SUN5_ENV */

int 
rx_mb_to_packet(char *amb, void (*free)(), int hdr_len, int data_len,
		struct rx_packet *phandle)
{
    int code;

    code = m_cpytoiovec(amb, hdr_len, data_len, phandle->wirevec,
			phandle->niovecs);
    (*free) (amb);

    return code;
}

#define CountFDs(amax) amax

#endif				       /* KERNEL */


/* send a response to a debug packet */

struct rx_packet *
rxi_ReceiveDebugPacket(struct rx_packet *ap, osi_socket asocket,
		       long ahost, short aport)
{
    struct rx_debugIn tin;
    long tl;

    rx_packetread(ap, 0, sizeof(struct rx_debugIn), (char *) &tin);

    /*
     * all done with packet, now set length to the truth, so we can reuse
     * this packet
     */
    rx_computelen(ap, ap->length);

    tin.type = ntohl(tin.type);
    tin.index = ntohl(tin.index);
    switch (tin.type) {
    case RX_DEBUGI_GETSTATS:{
	    struct rx_debugStats tstat;

	    /* get basic stats */
	    memset((char *) &tstat, 0, sizeof(tstat));	/* make sure spares are
							 * zero */
	    tstat.version = RX_DEBUGI_VERSION;
#ifndef	RX_ENABLE_LOCKS
	    tstat.waitingForPackets = rx_waitingForPackets;
#endif
	    tstat.nFreePackets = htonl(rx_nFreePackets);
	    tstat.callsExecuted = htonl(rxi_nCalls);
	    tstat.packetReclaims = htonl(0);
	    tstat.usedFDs = CountFDs(64);
	    tstat.nWaiting = htonl(rx_nWaiting);


	    tl = sizeof(struct rx_debugStats) - ap->length;
	    if (tl > 0)
		tl = rxi_AllocDataBuf(ap, tl);

	    if (tl <= 0) {
		rx_packetwrite(ap, 0, sizeof(struct rx_debugStats), (char *) &tstat);
		ap->length = sizeof(struct rx_debugStats);
		rxi_SendDebugPacket(ap, asocket, ahost, aport);
		rx_computelen(ap, ap->length);
	    }
	    break;
	}

    case RX_DEBUGI_GETALLCONN:
    case RX_DEBUGI_GETCONN:{
	    int i, j;
	    struct rx_connection *tc;
	    struct rx_call *tcall;
	    struct rx_debugConn tconn;
	    int all = (tin.type == RX_DEBUGI_GETALLCONN);


	    tl = sizeof(struct rx_debugConn) - ap->length;
	    if (tl > 0)
		tl = rxi_AllocDataBuf(ap, tl);
	    if (tl > 0)
		return ap;

	    memset((char *) &tconn, 0, sizeof(tconn));	/* make sure spares are
							 * zero */
	    /* get N'th (maybe) "interesting" connection info */
	    for (i = 0; i < rx_hashTableSize; i++) {
		for (tc = rx_connHashTable[i]; tc; tc = tc->next) {
		    if ((all || rxi_IsConnInteresting(tc)) && tin.index-- <= 0) {
			tconn.host = tc->peer->host;
			tconn.port = tc->peer->port;
			tconn.cid = htonl(tc->cid);
			tconn.epoch = htonl(tc->epoch);
			tconn.serial = htonl(tc->serial);
			for (j = 0; j < RX_MAXCALLS; j++) {
			    tconn.callNumber[j] = htonl(tc->callNumber[j]);
			    if ((tcall = tc->call[j]) != NULL) {
				tconn.callState[j] = tcall->state;
				tconn.callMode[j] = tcall->mode;
				tconn.callFlags[j] = tcall->flags;
				if (queue_IsNotEmpty(&tcall->rq))
				    tconn.callOther[j] |= RX_OTHER_IN;
				if (queue_IsNotEmpty(&tcall->tq))
				    tconn.callOther[j] |= RX_OTHER_OUT;
			    } else
				tconn.callState[j] = RX_STATE_NOTINIT;
			}

			tconn.maxPacketSize = htonl(tc->maxPacketSize);
			tconn.error = htonl(tc->error);
			tconn.flags = tc->flags;
			tconn.type = tc->type;
			tconn.securityIndex = tc->securityIndex;
			if (tc->securityObject) {
			    RXS_GetStats(tc->securityObject, tc,
					 &tconn.secStats);
#define DOHTONL(a) (tconn.secStats.a = htonl(tconn.secStats.a))
#define DOHTONS(a) (tconn.secStats.a = htons(tconn.secStats.a))
			    DOHTONL(flags);
			    DOHTONL(expires);
			    DOHTONL(packetsReceived);
			    DOHTONL(packetsSent);
			    DOHTONL(bytesReceived);
			    DOHTONL(bytesSent);
			    for (i = 0;
				 i < sizeof(tconn.secStats.spares) / sizeof(short);
				 i++)
				DOHTONS(spares[i]);
			    for (i = 0;
				 i < sizeof(tconn.secStats.sparel) / 4;
				 i++)
				DOHTONL(sparel[i]);
			}
			rx_packetwrite(ap, 0, sizeof(struct rx_debugConn), (char *) &tconn);
			tl = ap->length;
			ap->length = sizeof(struct rx_debugConn);
			rxi_SendDebugPacket(ap, asocket, ahost, aport);
			ap->length = tl;
			return ap;
		    }
		}
	    }
	    /* if we make it here, there are no interesting packets */
	    tconn.cid = htonl(0xffffffff);	/* means end */
	    rx_packetwrite(ap, 0, sizeof(struct rx_debugConn), &tconn);
	    tl = ap->length;
	    ap->length = sizeof(struct rx_debugConn);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport);
	    ap->length = tl;
	    break;
	}

    case RX_DEBUGI_RXSTATS:{
	    int i;
	    long *s;

	    tl = sizeof(rx_stats) - ap->length;
	    if (tl > 0)
		tl = rxi_AllocDataBuf(ap, tl);
	    if (tl > 0)
		return ap;

	    /* Since its all longs convert to network order with a loop. */
	    s = (long *) &rx_stats;
	    for (i = 0; i < sizeof(rx_stats) / 4; i++, s++)
		rx_PutLong(ap, i * 4, htonl(*s));

	    tl = ap->length;
	    ap->length = sizeof(rx_stats);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport);
	    ap->length = tl;
	    break;
	}

    default:
	/* error response packet */
	tin.type = htonl(RX_DEBUGI_BADTYPE);
	tin.index = tin.type;
	rx_packetwrite(ap, 0, sizeof(struct rx_debugIn), &tin);
	tl = ap->length;
	ap->length = sizeof(struct rx_debugIn);
	rxi_SendDebugPacket(ap, asocket, ahost, aport);
	ap->length = tl;
	break;
    }
    return ap;
}

struct rx_packet *
rxi_ReceiveVersionPacket(struct rx_packet *ap, osi_socket asocket,
			 long ahost, short aport)
{
    long tl;

    rx_packetwrite(ap, 0, 65, cml_version_number + 4);
    tl = ap->length;
    ap->length = 65;
    rxi_SendDebugPacket(ap, asocket, ahost, aport);
    ap->length = tl;
    return ap;
}


/* send a debug packet back to the sender */
void
rxi_SendDebugPacket(struct rx_packet *apacket, osi_socket asocket,
		    long ahost, short aport)
{
    struct sockaddr_in taddr;

    taddr.sin_family = AF_INET;
    taddr.sin_port = aport;
    taddr.sin_addr.s_addr = ahost;

    GLOBAL_UNLOCK();
    /* debug packets are not reliably delivered, hence the cast below. */
    /* MTUXXX need to adjust lengths as in sendSpecial */
    (void) osi_NetSend(asocket, (char *)&taddr, apacket->wirevec,
		       apacket->niovecs, apacket->length + RX_HEADER_SIZE);
    GLOBAL_LOCK();
}

/*
 * Send the packet to appropriate destination for the specified
 * connection.  The header is first encoded and placed in the packet.
 */
void
rxi_SendPacket(struct rx_connection *conn, 
	       struct rx_packet *p)
{
    struct sockaddr_in addr;
    struct rx_peer *peer = conn->peer;
    osi_socket socket;

#ifdef RXDEBUG
    char deliveryType = 'S';

#endif

    memset(&addr, 0, sizeof(addr));

    /* The address we're sending the packet to */
    addr.sin_family = AF_INET;
    addr.sin_port = peer->port;
    addr.sin_addr.s_addr = peer->host;

    /*
     * This stuff should be revamped, I think, so that most, if not all, of
     * the header stuff is always added here.  We could probably do away with
     * the encode/decode routines. XXXXX
     */

    /*
     * Stamp each packet with a unique serial number.  The serial number is
     * maintained on a connection basis because some types of security may be
     * based on the serial number of the packet, and security is handled on a
     * per authenticated-connection basis.
     */

    /*
     * Pre-increment, to guarantee no zero serial number; a zero serial
     * number means the packet was never sent.
     */
    p->header.serial = ++conn->serial;

    /*
     * This is so we can adjust retransmit time-outs better in the face of
     * rapidly changing round-trip times.  RTO estimation is not a la Karn.
     */
    if (p->firstSerial == 0) {
	p->firstSerial = p->header.serial;
    }
#ifdef RXDEBUG

    /*
     * If an output tracer function is defined, call it with the packet and
     * network address.  Note this function may modify its arguments.
     */
    if (rx_almostSent) {
	int drop = (*rx_almostSent) (p, &addr);

	/* drop packet if return value is non-zero? */
	if (drop)
	    deliveryType = 'D';	       /* Drop the packet */
    }
#endif

    /* Get network byte order header */
    rxi_EncodePacketHeader(p);	       /* XXX in the event of rexmit, etc,
				        * don't need to touch ALL the fields */

    /*
     * Send the packet out on the same socket that related packets are being
     * received on
     */
    socket = (conn->type == RX_CLIENT_CONNECTION
	      ? rx_socket : conn->service->socket);

#ifdef RXDEBUG
    /* Possibly drop this packet,  for testing purposes */
    if ((deliveryType == 'D') ||
	((rx_intentionallyDroppedPacketsPer100 > 0) &&
	 (random() % 100 < rx_intentionallyDroppedPacketsPer100))) {
	deliveryType = 'D';	       /* Drop the packet */
    } else {
	deliveryType = 'S';	       /* Send the packet */
#endif				       /* RXDEBUG */

	/*
	 * Loop until the packet is sent.  We'd prefer just to use a blocking
	 * socket, but unfortunately the interface doesn't allow us to have
	 * the socket block in send mode, and not block in receive mode
	 */
	GLOBAL_UNLOCK();
	if (osi_NetSend(socket, (char *)&addr, p->wirevec, 
			p->niovecs, p->length + RX_HEADER_SIZE)) {
	    /* send failed, so let's hurry up the resend, eh? */
	    rx_stats.netSendFailures++;
	    clock_Zero(&p->retryTime);
	    p->header.serial = 0;      /* Another way of saying never
				        * transmitted... */
	}
	GLOBAL_LOCK();
#ifdef RXDEBUG
    }
    dpf(("%c %d %s: %x.%u.%u.%u.%u.%u.%u flags %d, packet %x resend %d.%0.3d",
	 deliveryType, p->header.serial, rx_packetTypes[p->header.type - 1],
	 peer->host, peer->port, p->header.serial, p->header.epoch,
	 p->header.cid, p->header.callNumber, p->header.seq, p->header.flags,
	 p, p->retryTime.sec, p->retryTime.usec / 1000));
#endif
    rx_stats.packetsSent[p->header.type - 1]++;
}


/*
 * Send a "special" packet to the peer connection.  If call is
 * specified, then the packet is directed to a specific call channel
 * associated with the connection, otherwise it is directed to the
 * connection only. Uses optionalPacket if it is supplied, rather than
 * allocating a new packet buffer.  Nbytes is the length of the data
 * portion of the packet.  If data is non-null, nbytes of data are
 * copied into the packet.  Type is the type of the packet, as defined
 * in rx.h.  Bug: there's a lot of duplication between this and other
 * routines.  This needs to be cleaned up.
 */
struct rx_packet *
rxi_SendSpecial(struct rx_call *call, 
		struct rx_connection *conn, 
		struct rx_packet *optionalPacket, int type, char *data,
		int nbytes)
{

    /*
     * Some of the following stuff should be common code for all packet sends
     * (it's repeated elsewhere)
     */
    struct rx_packet *p;
    int i = 0;
    int savelen = 0;
    int saven = 0;
    int channel, callNumber;

    if (call) {
	channel = call->channel;
	callNumber = *call->callNumber;
    } else {
	channel = 0;
	callNumber = 0;
    }
    p = optionalPacket;
    if (!p) {
	p = rxi_AllocPacket(RX_PACKET_CLASS_SPECIAL);
	if (!p)
	    osi_Panic("rxi_SendSpecial failure");
    }
    if (nbytes != -1)
	p->length = nbytes;
    else
	nbytes = p->length;
    p->header.serviceId = conn->serviceId;
    p->header.securityIndex = conn->securityIndex;
    p->header.cid = (conn->cid | channel);
    p->header.callNumber = callNumber;
    p->header.seq = 0;
    p->header.epoch = conn->epoch;
    p->header.type = type;
    p->header.flags = 0;
    if (conn->type == RX_CLIENT_CONNECTION)
	p->header.flags |= RX_CLIENT_INITIATED;
    if (data)
	rx_packetwrite(p, 0, nbytes, data);

    for (i = 1; i < p->niovecs; i++) {
	if (nbytes <= p->wirevec[i].iov_len) {
	    savelen = p->wirevec[i].iov_len;
	    saven = p->niovecs;
	    p->wirevec[i].iov_len = nbytes;
	    p->niovecs = i + 1;	       /* so condition fails because i ==
				        * niovecs */
	} else
	    nbytes -= p->wirevec[i].iov_len;
    }

    if (call)
	rxi_Send(call, p);
    else
	rxi_SendPacket(conn, p);
    if (saven) {		       /* means we truncated the packet
				        * above.  We probably don't  */
	/* really need to do this, but it seems safer this way, given that  */
	/* sneaky optionalPacket... */
	p->wirevec[i - 1].iov_len = savelen;
	p->niovecs = saven;
    }
    if (!optionalPacket)
	rxi_FreePacket(p);
    return optionalPacket;
}


/* Encode the packet's header (from the struct header in the packet to
 * the net byte order representation in the wire representation of the
 * packet, which is what is actually sent out on the wire) */
void 
rxi_EncodePacketHeader(struct rx_packet *p)
{
    u_int32_t *buf = (u_int32_t *) (p->wirevec[0].iov_base); /* MTUXXX */

    memset((char *) buf, 0, RX_HEADER_SIZE);
    *buf++ = htonl(p->header.epoch);
    *buf++ = htonl(p->header.cid);
    *buf++ = htonl(p->header.callNumber);
    *buf++ = htonl(p->header.seq);
    *buf++ = htonl(p->header.serial);
    *buf++ = htonl((((unsigned long) p->header.type) << 24)
		   | (((unsigned long) p->header.flags) << 16)
		   | (p->header.userStatus << 8) | p->header.securityIndex);
    /* Note: top 16 bits of this next word were reserved */
    *buf++ = htonl((p->header.spare << 16) | (p->header.serviceId & 0xffff));
}

/* Decode the packet's header (from net byte order to a struct header) */
void 
rxi_DecodePacketHeader(struct rx_packet *p)
{
    u_int32_t *buf = (u_int32_t *) (p->wirevec[0].iov_base); /* MTUXXX */
    u_int32_t temp;

    p->header.epoch = ntohl(*buf++);
    p->header.cid = ntohl(*buf++);
    p->header.callNumber = ntohl(*buf++);
    p->header.seq = ntohl(*buf++);
    p->header.serial = ntohl(*buf++);
    temp = ntohl(*buf++);
    /* C will truncate byte fields to bytes for me */
    p->header.type = temp >> 24;
    p->header.flags = temp >> 16;
    p->header.userStatus = temp >> 8;
    p->header.securityIndex = temp >> 0;
    temp = ntohl(*buf++);
    p->header.serviceId = (temp & 0xffff);
    p->header.spare = temp >> 16;
    /* Note: top 16 bits of this last word are the security checksum */
}

void 
rxi_PrepareSendPacket(struct rx_call *call,
		      struct rx_packet *p, int last)
{
    struct rx_connection *conn = call->conn;
    int len, i;

    p->acked = 0;
    p->header.cid = (conn->cid | call->channel);
    p->header.serviceId = conn->serviceId;
    p->header.securityIndex = conn->securityIndex;
    p->header.callNumber = *call->callNumber;
    p->header.seq = call->tnext++;
    p->header.epoch = conn->epoch;
    p->header.type = RX_PACKET_TYPE_DATA;
    p->header.flags = 0;
    p->header.spare = 0;
    if (conn->type == RX_CLIENT_CONNECTION)
	p->header.flags |= RX_CLIENT_INITIATED;

    if (last)
	p->header.flags |= RX_LAST_PACKET;

    clock_Zero(&p->retryTime);	       /* Never yet transmitted */
    p->header.serial = 0;	       /* Another way of saying never
				        * transmitted... */
    p->backoff = 0;

    /*
     * Now that we're sure this is the last data on the call, make sure that
     * the "length" and the sum of the iov_lens matches.
     */
    len = p->length + call->conn->securityHeaderSize;

    for (i = 1; i < p->niovecs && len > 0; i++) {
	len -= p->wirevec[i].iov_len;
    }
    if (len > 0) {
	osi_Panic("PrepareSendPacket 1\n");	/* MTUXXX */
    } else {
	p->niovecs = i;
	p->wirevec[i - 1].iov_len += len;
    }
    RXS_PreparePacket(conn->securityObject, call, p);
}
