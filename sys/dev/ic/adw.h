/*	$OpenBSD: adw.h,v 1.1 1998/11/17 06:14:58 downsj Exp $	*/
/*      $NetBSD: adw.h,v 1.1 1998/09/26 16:10:41 dante Exp $        */

/*
 * Generic driver definitions and exported functions for the Advanced
 * Systems Inc. SCSI controllers
 * 
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ADVANSYS_WIDE_H_
#define _ADVANSYS_WIDE_H_

/******************************************************************************/

typedef int (* ADW_ISR_CALLBACK) (ADW_SOFTC *, ADW_SCSI_REQ_Q *);
typedef int (* ADW_SBRESET_CALLBACK) (ADW_SOFTC *);

/* per request scatter-gather element limit  */
#define ADW_MAX_SG_LIST		64

/* 
 * Scatter-Gather Definitions per request.
 */

#define NO_OF_SG_PER_BLOCK	15

/* Number of SG blocks needed. */
#define ADW_NUM_SG_BLOCK \
	((ADW_MAX_SG_LIST + (NO_OF_SG_PER_BLOCK - 1))/NO_OF_SG_PER_BLOCK)


struct adw_ccb
{
	ADW_SG_BLOCK		sg_block[ADW_NUM_SG_BLOCK];
	ADW_SCSI_REQ_Q		scsiq;

	struct scsi_sense_data scsi_sense;

	TAILQ_ENTRY(adw_ccb)	chain;
	struct scsi_xfer	*xs;	/* the scsi_xfer for this cmd */
	int			flags;	/* see below */

	int			timeout;
	/*
	 * This DMA map maps the buffer involved in the transfer.
	 */
	bus_dmamap_t		dmamap_xfer;
};

typedef struct adw_ccb ADW_CCB;

/* flags for ADW_CCB */
#define CCB_ALLOC       0x01
#define CCB_ABORT       0x02
#define	CCB_WATCHDOG	0x10


#define ADW_MAX_CCB	16

struct adw_control
{
	ADW_CCB	ccbs[ADW_MAX_CCB];	/* all our control blocks */
};

/*
 * Offset of a CCB from the beginning of the control DMA mapping.
 */
#define	ADW_CCB_OFF(c)	(offsetof(struct adw_control, ccbs[0]) +	\
		    (((u_long)(c)) - ((u_long)&sc->sc_control->ccbs[0])))

/******************************************************************************/

int adw_init __P((ADW_SOFTC *sc));
void adw_attach __P((ADW_SOFTC *sc));
int adw_intr __P((void *arg));

/******************************************************************************/

#endif /* _ADVANSYS_ADW_H_ */
