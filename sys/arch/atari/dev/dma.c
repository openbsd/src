/*	$NetBSD: dma.c,v 1.4.2.1 1995/11/06 21:51:12 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
 * All rights reserved.
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
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains special code dealing with the DMA interface
 * on the Atari ST.
 *
 * The DMA circuitry requires some special treatment for the peripheral
 * devices which make use of the ST's DMA feature (the hard disk and the
 * floppy drive).
 * All devices using DMA need mutually exclusive access and can follow some
 * standard pattern which will be provided in this file.
 *
 * The file contains the following entry points:
 *
 *	st_dmagrab:	ensure exclusive access to the DMA circuitry
 *	st_dmafree:	free exclusive access to the DMA circuitry
 *	st_dmawanted:	somebody is queued waiting for DMA-access
 *	dmaint:		DMA interrupt routine, switches to the current driver
 *	st_dmaaddr_set:	specify 24 bit RAM address
 *	st_dmaaddr_get:	get address of last DMA-op
 *	st_dmacomm:	program DMA, flush FIFO first
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <machine/cpu.h>
#include <machine/iomap.h>
#include <machine/dma.h>

#define	NDMA_DEV	10	/* Max 2 floppy's, 8 hard-disks		*/
typedef struct dma_entry {
	TAILQ_ENTRY(dma_entry)	entries;	/* List pointers	   */
	void			(*call_func)();	/* Call when lock granted  */
	void			(*int_func)();	/* Call on DMA interrupt   */
	void			*softc;		/* Arg. to int_func	   */
	int			*lock_stat;	/* status of DMA lock	   */
} DMA_ENTRY;

/*
 * Preallocated entries. An allocator seem an overkill here.
 */
static	DMA_ENTRY dmatable[NDMA_DEV];	/* preallocated entries		*/
static	int	  sched_soft = 0;	/* callback scheduled		*/

/*
 * Heads of free and active lists:
 */
static  TAILQ_HEAD(freehead, dma_entry)	dma_free;
static  TAILQ_HEAD(acthead, dma_entry)	dma_active;

static	int	must_init = 1;		/* Must initialize		*/

static	void	cdmasoft __P((void));
static	void	init_queues __P((void));

static void init_queues()
{
	int	i;

	TAILQ_INIT(&dma_free);
	TAILQ_INIT(&dma_active);

	for(i = 0; i < NDMA_DEV; i++)
		TAILQ_INSERT_HEAD(&dma_free, &dmatable[i], entries);
}

int st_dmagrab(int_func, call_func, softc, lock_stat, rcaller)
void 	(*int_func)();
void 	(*call_func)();
void	*softc;
int	*lock_stat;
int	rcaller;
{
	int		sps;
	DMA_ENTRY	*req;

	if(must_init) {
		init_queues();
		must_init = 0;
	}
	*lock_stat = DMA_LOCK_REQ;

	sps = splhigh();

	/*
	 * Create a request...
	 */
	if(dma_free.tqh_first == NULL)
		panic("st_dmagrab: Too many outstanding requests\n");
	req = dma_free.tqh_first;
	TAILQ_REMOVE(&dma_free, dma_free.tqh_first, entries);
	req->call_func = call_func;
	req->int_func  = int_func;
	req->softc     = softc;
	req->lock_stat = lock_stat;
	TAILQ_INSERT_TAIL(&dma_active, req, entries);

	if(dma_active.tqh_first != req) {
		splx(sps);
		return(0);
	}
	splx(sps);

	/*
	 * We're at the head of the queue, ergo: we got the lock.
	 */
	*lock_stat = DMA_LOCK_GRANT;

	if(rcaller) {
		/*
		 * Just return to caller immediately without going
		 * through 'call_func' first.
		 */
		return(1);
	}

	(*call_func)(softc);	/* Call followup function		*/
	return(0);
}

void
st_dmafree(softc, lock_stat)
void	*softc;
int	*lock_stat;
{
	int		sps;
	DMA_ENTRY	*req;
	
	sps = splhigh();

	/*
	 * Some validity checks first.
	 */
	if((req = dma_active.tqh_first) == NULL)
		panic("st_dmafree: empty active queue\n");
	if(req->softc != softc)
		printf("Caller of st_dmafree is not lock-owner!\n");

	/*
	 * Clear lock status, move request from active to free queue.
	 */
	*lock_stat = 0;
	TAILQ_REMOVE(&dma_active, req, entries);
	TAILQ_INSERT_HEAD(&dma_free, req, entries);

	if((req = dma_active.tqh_first) != NULL) {
		/*
		 * Call next request through softint handler. This avoids
		 * spl-conflicts.
		 */
		*req->lock_stat = DMA_LOCK_GRANT;
		add_sicallback(req->call_func, req->softc, 0);
	}
	splx(sps);
	return;
}

int
st_dmawanted()
{
	return(dma_active.tqh_first->entries.tqe_next != NULL);
}

cdmaint(sr)
long	sr;	/* sr at time of interrupt */
{
	if(dma_active.tqh_first != NULL) {
		if(!BASEPRI(sr)) {
			if(!sched_soft++)
				add_sicallback(cdmasoft, 0, 0);
		}
		else {
			spl1();
			cdmasoft();
		}
	}
	else printf("DMA interrupt discarded\n");
}

static void cdmasoft()
{
	int	s;
	void 	(*int_func)();
	void	*softc;

	/*
	 * Prevent a race condition here. DMA might be freed while
	 * the callback was pending!
	 */
	s = splhigh();
	sched_soft = 0;
	if(dma_active.tqh_first != NULL) {
		int_func = dma_active.tqh_first->int_func;
		softc    = dma_active.tqh_first->softc;
	}
	else int_func = NULL;
	splx(s);

	if(int_func != NULL)
		(*int_func)(softc);
}

/*
 * Setup address for DMA-transfer.
 * Note: The order _is_ important!
 */
void
st_dmaaddr_set(address)
caddr_t	address;
{
	register u_long ad = (u_long)address;

	DMA->dma_addr[AD_LOW ] = (ad     ) & 0xff;
	DMA->dma_addr[AD_MID ] = (ad >> 8) & 0xff;
	DMA->dma_addr[AD_HIGH] = (ad >>16) & 0xff;
}

/*
 * Get address from DMA unit.
 */
u_long
st_dmaaddr_get()
{
	register u_long ad = 0;

	ad  = (DMA->dma_addr[AD_LOW ] & 0xff);
	ad |= (DMA->dma_addr[AD_MID ] & 0xff) << 8;
	ad |= (DMA->dma_addr[AD_HIGH] & 0xff) <<16;
	return(ad);
}

/*
 * Program the DMA-controller to transfer 'nblk' blocks of 512 bytes.
 * The DMA_WRBIT trick flushes the FIFO before doing DMA.
 */
void
st_dmacomm(mode, nblk)
int	mode, nblk;
{
	DMA->dma_mode = mode;
	DMA->dma_mode = mode ^ DMA_WRBIT;
	DMA->dma_mode = mode;
	DMA->dma_data = nblk;
	delay(2);	/* Needed for Falcon */
	DMA->dma_mode = DMA_SCREG | (mode & DMA_WRBIT);
}
