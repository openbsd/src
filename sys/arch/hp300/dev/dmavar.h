/*	$OpenBSD: dmavar.h,v 1.7 2005/11/17 23:56:02 miod Exp $	*/
/*	$NetBSD: dmavar.h,v 1.9 1997/04/01 03:10:59 scottr Exp $	*/

/*
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1982, 1990, 1993
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
 *	@(#)dmavar.h	8.1 (Berkeley) 6/10/93
 */

#include <sys/queue.h>

/* dmago flags */
#define	DMAGO_BYTE	0x00	/* do byte (8 bit) transfers */
#define	DMAGO_WORD	0x01	/* do word (16 bit) transfers */
#define	DMAGO_LWORD	0x02	/* do longword (32 bit) transfers */
#define	DMAGO_PRI	0x04	/* do "priority" DMA */
#define	DMAGO_READ	0x08	/* transfer is a read */
#define	DMAGO_NOINT	0x80	/* don't interrupt on completion */

/* dma "controllers" (channels) */
#define	DMA0		0x1
#define	DMA1		0x2

/*
 * A DMA queue entry.  Initiator drivers each have one of these,
 * used to queue access to the DMA controller.
 */
struct dmaqueue {
	TAILQ_ENTRY(dmaqueue) dq_list;	/* entry on the queue */
	int	dq_chan;		/* OR of channels initiator can use */
	void	*dq_softc;		/* initiator's softc */

	/*
	 * These functions are called to start the initiator when
	 * it has been given the DMA controller, and to stop the
	 * initiator when the DMA controller has stopped.
	 */
	void	(*dq_start)(void *);
	void	(*dq_done)(void *);
};

#ifdef _KERNEL
void	dmainit(void);
void	dmago(int, char *, u_int, int);
void	dmastop(int);
void	dmafree(struct dmaqueue *);
int	dmareq(struct dmaqueue *);
void	dmacomputeipl(void);
#endif /* _KERNEL */
