/*	$OpenBSD: rx_rdwr.c,v 1.1.1.1 1998/09/14 21:53:17 art Exp $	*/
#include "rx_locl.h"

RCSID("$KTH: rx_rdwr.c,v 1.4 1998/02/22 19:53:57 joda Exp $");

#ifdef	AFS_SGIMP_ENV
int 
rx_Read(struct rx_call *call, char *buf, int nbytes)
{
    int rv;

    SPLVAR;
    GLOCKSTATE ms;

    NETPRI;
    AFS_GRELEASE(&ms);
    rv = rx_ReadProc(call, buf, nbytes);
    AFS_GACQUIRE(&ms);
    USERPRI;
    return rv;
}

int 
rx_Write(struct rx_call *call, char *buf, int nbytes)
{
    int rv;

    SPLVAR;
    GLOCKSTATE ms;

    NETPRI;
    AFS_GRELEASE(&ms);
    rv = rx_WriteProc(call, buf, nbytes);
    AFS_GACQUIRE(&ms);
    USERPRI;
    return rv;
}

#endif

int 
rx_ReadProc(struct rx_call *call, char *buf, int nbytes)
{
    struct rx_packet *rp;
    int requestCount;

    SPLVAR;
/* XXXX took out clock_NewTime from here.  Was it needed? */
    requestCount = nbytes;

    NETPRI;
    GLOBAL_LOCK();
    MUTEX_ENTER(&call->lock);

    while (nbytes) {
	if (call->nLeft == 0) {
	    /* Get next packet */
	    for (;;) {
		if (call->error || (call->mode != RX_MODE_RECEIVING)) {
		    if (call->error) {
			MUTEX_EXIT(&call->lock);
			GLOBAL_UNLOCK();
			USERPRI;
			return 0;
		    }
		    if (call->mode == RX_MODE_SENDING) {
			rx_FlushWrite(call);
			continue;
		    }
		}
		if (queue_IsNotEmpty(&call->rq)) {
		    /* Check that next packet available is next in sequence */
		    rp = queue_First(&call->rq, rx_packet);
		    if (rp->header.seq == call->rnext) {
			long error;
			struct rx_connection *conn = call->conn;

			queue_Remove(rp);

			/*
			 * RXS_CheckPacket called to undo RXS_PreparePacket's
			 * work.  It may reduce the length of the packet by
			 * up to conn->maxTrailerSize, to reflect the length
			 * of the data + the header.
			 */
			if ((error = RXS_CheckPacket(conn->securityObject,
						    call, rp)) != 0) {

			    /*
			     * Used to merely shut down the call, but now we
			     * shut down the whole connection since this may
			     * indicate an attempt to hijack it
			     */
			    rxi_ConnectionError(conn, error);
			    rp = rxi_SendConnectionAbort(conn, rp);
			    rxi_FreePacket(rp);

			    MUTEX_EXIT(&call->lock);
			    GLOBAL_UNLOCK();
			    USERPRI;
			    return 0;
			}
			call->rnext++;
			call->currentPacket = rp;
			call->curvec = 1;	/* 0th vec is always header */

			/*
			 * begin at the beginning [ more or less ], continue
			 * on until the end, then stop.
			 */
			call->curpos = call->conn->securityHeaderSize;

			/*
			 * Notice that this code works correctly if the data
			 * size is 0 (which it may be--no reply arguments
			 * from server, for example).  This relies heavily on
			 * the fact that the code below immediately frees the
			 * packet (no yields, etc.).  If it didn't, this
			 * would be a problem because a value of zero for
			 * call->nLeft normally means that there is no read
			 * packet
			 */
			call->nLeft = rp->length;
			if (rp->header.flags & RX_LAST_PACKET)
			    call->flags |= RX_CALL_RECEIVE_DONE;

			/*
			 * now, if we haven't send a hard ack for window/2
			 * packets we spontaneously generate one, to take
			 * care of the case where (most likely in the kernel)
			 * we receive a window-full of packets, and ack all
			 * of them before any are read by the user, thus not
			 * hard-acking any of them.  The sender retransmits
			 * in this case only under a timer, which is a real
			 * loser
			 */

#ifndef ADAPT_WINDOW
			if (call->rnext > (call->lastAcked + (rx_Window >> 1)))
#else				       /* ADAPT_WINDOW */
			if (call->rnext > (call->lastAcked +
					(call->conn->peer->maxWindow >> 1)))
#endif				       /* ADAPT_WINDOW */
			{
			    rxi_SendAck(call, 0, 0, 0, 0, RX_ACK_DELAY);
			}
			break;
		    }
		}
/*
MTUXXX  doesn't there need to be an "else" here ???
*/
		/* Are there ever going to be any more packets? */
		if (call->flags & RX_CALL_RECEIVE_DONE) {
		    MUTEX_EXIT(&call->lock);
		    GLOBAL_UNLOCK();
		    USERPRI;
		    return requestCount - nbytes;
		}
		/* Wait for in-sequence packet */
		call->flags |= RX_CALL_READER_WAIT;
		clock_NewTime();
		call->startWait = clock_Sec();

		MUTEX_EXIT(&call->lock);
		MUTEX_ENTER(&call->lockq);
#ifdef	RX_ENABLE_LOCKS
		while (call->flags & RX_CALL_READER_WAIT)
		    cv_wait(&call->cv_rq, &call->lockq);
#else
		osi_rxSleep(&call->rq);
#endif
		MUTEX_EXIT(&call->lockq);
		MUTEX_ENTER(&call->lock);

		call->startWait = 0;
	    }
	} else {		       /* assert(call->currentPacket); */
                                       /*
				        * MTUXXX  this should be replaced by
				        * some error-recovery code before
					* shipping
					*/
            /*
	     * It's possible for call->nLeft to be smaller than
	     * any particular iov_len. Usually, recvmsg doesn't change the
	     * iov_len, since it reflects the size of the buffer.  We have to
	     * keep track of the number of bytes read in the length field of
	     * the packet struct.  On the final portion of a received packet,
	     * it's almost certain that call->nLeft will be smaller than the
	     * final buffer.
	     */
	    unsigned int t, l1;
	    caddr_t p1;

	    while (nbytes && call->currentPacket) {
		p1 = (char*)call->currentPacket->wirevec[call->curvec].iov_base +
		    call->curpos;
		l1 = call->currentPacket->wirevec[call->curvec].iov_len -
		    call->curpos;

		t = MIN(l1, nbytes);
		t = MIN(t, call->nLeft);
		memcpy(buf, p1, t);
		p1 += t;
		buf += t;
		l1 -= t;
		nbytes -= t;
		call->curpos += t;
		call->nLeft -= t;

		if (call->nLeft == 0) {
		    /* out of packet.  Get another one. */
		    rxi_FreePacket(call->currentPacket);
		    call->currentPacket = NULL;
		} else if (l1 == 0) {
		    /* need to get another struct iov */
		    if (++call->curvec > call->currentPacket->niovecs) {
			/*
			 * current packet is exhausted, get ready for another
			 */

			/*
			 * don't worry about curvec and stuff, they get set
			 * somewhere else
			 */
			rxi_FreePacket(call->currentPacket);
			call->currentPacket = NULL;
			call->nLeft = 0;
		    } else
			call->curpos = 0;
		}
	    }
	    if (nbytes == 0) {
		/* user buffer is full, return */
		MUTEX_EXIT(&call->lock);
		GLOBAL_UNLOCK();
		USERPRI;
		return requestCount;
	    }
	}

    }				       /* while (nbytes) ... */

    MUTEX_EXIT(&call->lock);
    GLOBAL_UNLOCK();
    USERPRI;
    return requestCount;
}

int 
rx_WriteProc(struct rx_call *call, char *buf, int nbytes)
{
    struct rx_connection *conn = call->conn;
    int requestCount = nbytes;

    SPLVAR;

    GLOBAL_LOCK();
    MUTEX_ENTER(&call->lock);
    NETPRI;
    if (call->mode != RX_MODE_SENDING) {
	if ((conn->type == RX_SERVER_CONNECTION)
	    && (call->mode == RX_MODE_RECEIVING)) {
	    call->mode = RX_MODE_SENDING;
	    if (call->currentPacket) {
		rxi_FreePacket(call->currentPacket);
		call->currentPacket = NULL;
		call->nLeft = 0;
		call->nFree = 0;
	    }
	} else {
	    MUTEX_EXIT(&call->lock);
	    GLOBAL_UNLOCK();
	    USERPRI;
	    return 0;
	}
    }

    /*
     * Loop condition is checked at end, so that a write of 0 bytes will
     * force a packet to be created--specially for the case where there are 0
     * bytes on the stream, but we must send a packet anyway.
     */
    do {
	if (call->nFree == 0) {
	    struct rx_packet *cp = call->currentPacket;

	    if (!call->error && call->currentPacket) {
		clock_NewTime();       /* Bogus:  need new time package */

		/*
		 * The 0, below, specifies that it is not the last packet:
		 * there will be others. PrepareSendPacket may alter the
		 * packet length by up to conn->securityMaxTrailerSize
		 */
		rxi_PrepareSendPacket(call, cp, 0);
		queue_Append(&call->tq, cp);
		rxi_Start(0, call);
	    }
	    /* Wait for transmit window to open up */
	    while (!call->error && 
		   call->tnext + 1 > call->tfirst + call->twind) {
		clock_NewTime();
		call->startWait = clock_Sec();

		MUTEX_EXIT(&call->lock);
		MUTEX_ENTER(&call->lockw);

#ifdef	RX_ENABLE_LOCKS
		cv_wait(&call->cv_twind, &call->lockw);
#else
		call->flags |= RX_CALL_WAIT_WINDOW_ALLOC;
		osi_rxSleep(&call->twind);
#endif
		MUTEX_EXIT(&call->lockw);
		MUTEX_ENTER(&call->lock);

		call->startWait = 0;
	    }
	    if ((call->currentPacket = rxi_AllocSendPacket(call, 
							   nbytes)) != 0) {
		call->nFree = call->currentPacket->length;
		call->curvec = 1;      /* 0th vec is always header */

		/*
		 * begin at the beginning [ more or less ], continue on until
		 * the end, then stop.
		 */
		call->curpos = call->conn->securityHeaderSize;
	    }
	    if (call->error) {
		if (call->currentPacket) {
		    rxi_FreePacket(call->currentPacket);
		    call->currentPacket = NULL;
		}
		MUTEX_EXIT(&call->lock);
		GLOBAL_UNLOCK();
		USERPRI;
		return 0;
	    }
	}

	/*
	 * If the remaining bytes fit in the buffer, then store them and
	 * return.  Don't ship a buffer that's full immediately to the
	 * peer--we don't know if it's the last buffer yet
	 */

	if (!(call->currentPacket)) {
	    call->nFree = 0;
	} 
	{
	    struct rx_packet *cp = call->currentPacket;
	    unsigned int t, l1;
	    caddr_t p1;

	    while (nbytes && call->nFree) {
		p1 = (char *)cp->wirevec[call->curvec].iov_base + call->curpos;
		l1 = cp->wirevec[call->curvec].iov_len - call->curpos;

		t = MIN(call->nFree, MIN(l1, nbytes));
		memcpy(p1, buf, t);
		p1 += t;
		buf += t;
		l1 -= t;
		nbytes -= t;
		call->curpos += t;
		call->nFree -= t;

		if (!l1) {
		    call->curpos = 0;
		    /* need to get another struct iov */
		    if (++call->curvec >= cp->niovecs) {
			/* current packet is full, extend or send it */
			call->nFree = 0;
		    }
		}
		if (!call->nFree) {
		    int len, mud;

		    len = cp->length;
		    mud = rx_MaxUserDataSize(conn);
		    if (mud > len) {
			int want;

			want = MIN(nbytes, mud - len);
			rxi_AllocDataBuf(cp, want);
			if (cp->length > mud)
			    cp->length = mud;
			call->nFree += (cp->length - len);
		    }
		}
	    }			       /*
					* while bytes to send and room to
				        * send them 
					*/
	    /* might be out of space now */
	    if (!nbytes) {
		MUTEX_EXIT(&call->lock);
		GLOBAL_UNLOCK();
		USERPRI;
		return requestCount;
	    } else;		       /*
					* more data to send, so get another
				        * packet and keep going 
					*/
	}
    } while (nbytes);

    MUTEX_EXIT(&call->lock);
    GLOBAL_UNLOCK();
    USERPRI;
    return requestCount - nbytes;
}

/*
 * Flush any buffered data to the stream, switch to read mode
 * (clients) or to EOF mode (servers)
 */
void 
rx_FlushWrite(struct rx_call *call)
{
    SPLVAR;
    NETPRI;
    if (call->mode == RX_MODE_SENDING) {
	struct rx_packet *cp;

	call->mode = (call->conn->type == RX_CLIENT_CONNECTION ?
		      RX_MODE_RECEIVING : RX_MODE_EOF);

	if (call->currentPacket) {

	    /* cp->length is only supposed to be the user's data */

	    cp = call->currentPacket;

	    /*
	     * cp->length was already set to (then-current) MaxUserDataSize
	     * or less.
	     */
	    cp->length -= call->nFree;
	    call->currentPacket = (struct rx_packet *) 0;
	    call->nFree = 0;


	} else {
	    cp = rxi_AllocSendPacket(call, 0);
	    if (!cp) {
		/* Mode can no longer be MODE_SENDING */
		USERPRI;
		return;
	    }
	    cp->length = 0;
	    cp->niovecs = 1;	       /* just the header */
	    call->nFree = 0;
	}

	/* The 1 specifies that this is the last packet */
	rxi_PrepareSendPacket(call, cp, 1);
	queue_Append(&call->tq, cp);
	rxi_Start(0, call);
    }
    USERPRI;
}
