/*      $NetBSD: adw.h,v 1.5 2000/02/03 20:29:15 dante Exp $        */

/*
 * Generic driver definitions and exported functions for the Advanced
 * Systems Inc. SCSI controllers
 * 
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
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
typedef void (* ADW_ASYNC_CALLBACK) (ADW_SOFTC *, u_int8_t);


/*
 * Every adw_carrier structure _MUST_ always be aligned on a 16 bytes boundary
 */
struct adw_carrier {
/* ---------- the microcode wants the field below ---------- */
	u_int32_t	unused;	  /* Carrier Virtual Address -UNUSED- */
	u_int32_t	carr_pa;  /* Carrier Physical Address */
	u_int32_t	areq_vpa; /* ADW_SCSI_REQ_Q Physical Address */
	/*
	 * next_vpa [31:4]	Carrier Physical Next Pointer
	 *
	 * next_vpa [3:1]	Reserved Bits
	 * next_vpa [0]		Done Flag set in Response Queue.
	 */
	u_int32_t	next_vpa;
/* ----------                                     ---------- */
	struct adw_carrier	*nexthash;	/* Carrier Virtual Address */

	int			id;
	/*
	 * This DMA map maps the buffer involved in the carrier transfer.
	 */
//	bus_dmamap_t	dmamap_xfer;
};

typedef struct adw_carrier ADW_CARRIER;

#define ADW_CARRIER_SIZE	((((int)((sizeof(ADW_CARRIER)-1)/16))+1)*16)


/*
 * Mask used to eliminate low 4 bits of carrier 'next_vpa' field.
 */
#define ASC_NEXT_VPA_MASK       0xFFFFFFF0

#define ASC_RQ_DONE             0x00000001
#define ASC_CQ_STOPPER          0x00000000

#define ASC_GET_CARRP(carrp) ((carrp) & ASC_NEXT_VPA_MASK)


/*
 * per request scatter-gather element limit
 * We could have up to 256 SG lists.
 */
#define ADW_MAX_SG_LIST		255

/* 
 * Scatter-Gather Definitions per request.
 */

#define NO_OF_SG_PER_BLOCK	15

/* Number of SG blocks needed. */
#define ADW_NUM_SG_BLOCK \
	((ADW_MAX_SG_LIST + (NO_OF_SG_PER_BLOCK - 1))/NO_OF_SG_PER_BLOCK)


struct adw_ccb {
	ADW_SCSI_REQ_Q		scsiq;
	ADW_SG_BLOCK		sg_block[ADW_NUM_SG_BLOCK];

	ADW_CARRIER		*carr_list;	/* carriers involved */

	struct scsi_sense_data  scsi_sense;

	TAILQ_ENTRY(adw_ccb)	chain;
	struct adw_ccb		*nexthash;
	u_int32_t		hashkey;

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
#define CCB_ALLOC	0x01
#define CCB_ABORTING	0x02
#define CCB_ABORTED	0x04


#define ADW_MAX_CARRIER	20	/* Max. number of host commands (253) */
#define ADW_MAX_CCB	16	/* Max. number commands per device (63) */

struct adw_control {
	ADW_CCB		ccbs[ADW_MAX_CCB];	/* all our control blocks */
	ADW_CARRIER	*carriers;		/* all our carriers */
	bus_dmamap_t	dmamap_xfer;
};

/*
 * Offset of a carrier from the beginning of the carriers DMA mapping.
 */
#define	ADW_CARRIER_ADDR(sc, x)	((sc)->sc_dmamap_carrier->dm_segs[0].ds_addr + \
			(((u_long)x) - ((u_long)(sc)->sc_control->carriers)))
/*
 * Offset of a CCB from the beginning of the control DMA mapping.
 */
#define	ADW_CCB_OFF(c)	(offsetof(struct adw_control, ccbs[0]) +	\
		    (((u_long)(c)) - ((u_long)&sc->sc_control->ccbs[0])))

/******************************************************************************/

int adw_init __P((ADW_SOFTC *sc));
void adw_attach __P((ADW_SOFTC *sc));
int adw_intr __P((void *arg));
ADW_CCB *adw_ccb_phys_kv __P((ADW_SOFTC *, u_int32_t));
ADW_CARRIER *adw_carrier_phys_kv __P((ADW_SOFTC *, u_int32_t));

/******************************************************************************/

#endif /* _ADVANSYS_ADW_H_ */
