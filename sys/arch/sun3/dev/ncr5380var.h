/*	$NetBSD: ncr5380var.h,v 1.1 1995/10/29 21:19:10 gwr Exp $	*/

/*
 * Copyright (c) 1995 David Jones, Gordon W. Ross
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *      David Jones and Gordon Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file defines the interface between the machine-dependent
 * module and the machine-indepenedent ncr5380sbc.c module.
 */

#define SCI_CLR_INTR(sc)	(*(sc)->sci_iack)
#define	SCI_BUSY(sc)		(*sc->sci_bus_csr & SCI_BUS_BSY)

#define PHASE_DATA_OUT	0x0
#define PHASE_DATA_IN	0x1
#define PHASE_CMD	0x2
#define PHASE_STATUS	0x3
#define PHASE_UNSPEC1	0x4
#define PHASE_UNSPEC2	0x5
#define PHASE_MSG_OUT	0x6
#define PHASE_MSG_IN	0x7

#define PHASE_INVALID	-1

#define SCSI_PHASE(x)	((x)&0x7)

/* Per-request state.  This is required in order to support reselection. */
struct sci_req {
	struct		scsi_xfer *sr_xs;	/* Pointer to xfer struct, NULL=unused */
	int		sr_target, sr_lun;	/* For fast access */
	void		*sr_dma_hand;		/* Current DMA hnadle */
	u_char		*sr_data;		/* Saved data pointer */
	int		sr_datalen;
	int		sr_flags;		/* Internal error code */
#define	SR_IMMED			1	/* Immediate command */
#define	SR_SENSE			2	/* We are getting sense */
#define	SR_ERROR			4	/* Error occurred */
	int		sr_status;		/* Status code from last cmd */
};
#define	SCI_OPENINGS	4		/* Up to 4 commands at once */


struct ncr5380_softc {
	struct device	sc_dev;
	struct		scsi_link sc_link;

	/* Pointers to 5380 registers.  MD code must set these up. */
	volatile u_char *sci_data;
	volatile u_char *sci_icmd;
	volatile u_char *sci_mode;
	volatile u_char *sci_tcmd;
	volatile u_char *sci_bus_csr;
	volatile u_char *sci_csr;
	volatile u_char *sci_idata;
	volatile u_char *sci_iack;

	/* Functions set from MD code */
	int		(*sc_pio_out) __P((struct ncr5380_softc *,
					   int, int, u_char *));
	int		(*sc_pio_in) __P((struct ncr5380_softc *,
					  int, int, u_char *));
	void		(*sc_dma_alloc) __P((struct ncr5380_softc *));
	void		(*sc_dma_free) __P((struct ncr5380_softc *));
	void		(*sc_dma_start) __P((struct ncr5380_softc *));
	void		(*sc_dma_poll) __P((struct ncr5380_softc *));
	void		(*sc_dma_eop) __P((struct ncr5380_softc *));
	void		(*sc_dma_stop) __P((struct ncr5380_softc *));

	int		sc_flags;	/* Misc. flags and capabilities */
#define	NCR5380_PERMIT_RESELECT		1  /* Allow disconnect/reselect */

	int 	sc_min_dma_len;	/* Smaller than this is done with PIO */

	/* Begin MI shared data */

	/* Active data pointer for current SCSI process */
	u_char		*sc_dataptr;
	int		sc_datalen;

	void		*sc_dma_hand;	/* DMA handle */
	u_int		sc_dma_flags;
#define	DMA5380_INPROGRESS		1	/* MD: DMA is curently in progress */
#define	DMA5380_WRITE			2	/* MI: DMA is to output to SCSI */
#define	DMA5380_POLL			4	/* MI: Poll for DMA completion */
#define	DMA5380_ERROR			8	/* MD: DMA operation failed */
#define	DMA5380_PHYS			16	/* MI: Buffer has B_PHYS set */

	/* Begin MI private data */

	/* The request that has the bus now. */
	struct		sci_req *sc_current;

	/* The number of operations in progress on the bus */
	volatile int	sc_ncmds;

	/* Ring buffer of pending/active requests */
	struct		sci_req sc_ring[SCI_OPENINGS];
	int		sc_rr;		/* Round-robin scan pointer */

	/* Active requests, by target/LUN */
	struct		sci_req *sc_matrix[8][8];

	/* Message stuff */
	int	sc_prevphase;
	int	sc_msg_flags;
#define NCR_DROP_MSGIN	1
#define NCR_ABORTING	2
#define NCR_NEED_RESET	4
	u_int	sc_msgpriq;	/* Messages we want to send */
	u_int	sc_msgoutq;	/* Messages sent during last MESSAGE OUT */
	u_int	sc_msgout;	/* Message last transmitted */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_ABORT		0x04
#define SEND_REJECT		0x08
#define SEND_INIT_DET_ERR	0x10
#define SEND_IDENTIFY  		0x20
#define SEND_SDTR		0x40
#define	SEND_WDTR		0x80
#define NCR_MAX_MSG_LEN 8
	u_char  sc_omess[NCR_MAX_MSG_LEN];
	u_char	*sc_omp;		/* Outgoing message pointer */
	u_char	sc_imess[NCR_MAX_MSG_LEN];
	u_char	*sc_imp;		/* Incoming message pointer */

};

void	ncr5380_init __P((struct ncr5380_softc *));
void	ncr5380_reset_scsibus __P((struct ncr5380_softc *));
int 	ncr5380_sbc_intr __P((struct ncr5380_softc *));
int 	ncr5380_scsi_cmd __P((struct scsi_xfer *));
int 	ncr5380_pio_in __P((struct ncr5380_softc *, int, int, u_char *));
int 	ncr5380_pio_out __P((struct ncr5380_softc *, int, int, u_char *));

