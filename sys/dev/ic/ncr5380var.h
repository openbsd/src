/*	$NetBSD: ncr5380var.h,v 1.4 1996/01/01 22:24:38 thorpej Exp $	*/

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

/* These are NOT artibtrary, but map to bits in sci_tcmd */
#define PHASE_DATA_OUT	0x0
#define PHASE_DATA_IN	0x1
#define PHASE_COMMAND	0x2
#define PHASE_STATUS	0x3
#define PHASE_UNSPEC1	0x4
#define PHASE_UNSPEC2	0x5
#define PHASE_MSG_OUT	0x6
#define PHASE_MSG_IN	0x7

/*
 * This illegal phase is used to prevent the 5380 from having
 * a phase-match condition when we don't want one, such as
 * when setting up the DMA engine or whatever...
 */
#define PHASE_INVALID	PHASE_UNSPEC1


/* Per-request state.  This is required in order to support reselection. */
struct sci_req {
	struct		scsi_xfer *sr_xs;	/* Pointer to xfer struct, NULL=unused */
	int		sr_target, sr_lun;	/* For fast access */
	void		*sr_dma_hand;		/* Current DMA hnadle */
	u_char		*sr_dataptr;		/* Saved data pointer */
	int		sr_datalen;
	int		sr_flags;		/* Internal error code */
#define	SR_IMMED			1	/* Immediate command */
#define	SR_SENSE			2	/* We are getting sense */
#define	SR_OVERDUE			4	/* Timeout while not current */
#define	SR_ERROR			8	/* Error occurred */
	int		sr_status;		/* Status code from last cmd */
};
#define	SCI_OPENINGS	16		/* How many commands we can enqueue. */


struct ncr5380_softc {
	struct device	sc_dev;
	struct		scsi_link sc_link;

	/* Pointers to 5380 registers.  See ncr5380reg.h */
	volatile u_char *sci_r0;
	volatile u_char *sci_r1;
	volatile u_char *sci_r2;
	volatile u_char *sci_r3;
	volatile u_char *sci_r4;
	volatile u_char *sci_r5;
	volatile u_char *sci_r6;
	volatile u_char *sci_r7;

	/* Functions set from MD code */
	int		(*sc_pio_out) __P((struct ncr5380_softc *,
					   int, int, u_char *));
	int		(*sc_pio_in) __P((struct ncr5380_softc *,
					  int, int, u_char *));
	void		(*sc_dma_alloc) __P((struct ncr5380_softc *));
	void		(*sc_dma_free) __P((struct ncr5380_softc *));

	void		(*sc_dma_setup) __P((struct ncr5380_softc *));
	void		(*sc_dma_start) __P((struct ncr5380_softc *));
	void		(*sc_dma_poll) __P((struct ncr5380_softc *));
	void		(*sc_dma_eop) __P((struct ncr5380_softc *));
	void		(*sc_dma_stop) __P((struct ncr5380_softc *));

	void		(*sc_intr_on) __P((struct ncr5380_softc *));
	void		(*sc_intr_off) __P((struct ncr5380_softc *));

	int		sc_flags;	/* Misc. flags and capabilities */
#define	NCR5380_PERMIT_RESELECT		1  /* Allow disconnect/reselect */
#define	NCR5380_FORCE_POLLING		2  /* Do not use interrupts. */

	int 	sc_min_dma_len;	/* Smaller than this is done with PIO */

	/* Begin MI shared data */

	int		sc_state;
#define	NCR_IDLE		   0	/* Ready for new work. */
#define NCR_WORKING 	0x01	/* Some command is in progress. */
#define	NCR_ABORTING	0x02	/* Bailing out */
#define NCR_DOINGDMA	0x04	/* The FIFO data path is active! */
#define NCR_DROP_MSGIN	0x10	/* Discard all msgs (parity err detected) */

	/* The request that has the bus now. */
	struct		sci_req *sc_current;

	/* Active data pointer for current SCSI command. */
	u_char		*sc_dataptr;
	int		sc_datalen;

	/* Begin MI private data */

	/* The number of operations in progress on the bus */
	volatile int	sc_ncmds;

	/* Ring buffer of pending/active requests */
	struct		sci_req sc_ring[SCI_OPENINGS];
	int		sc_rr;		/* Round-robin scan pointer */

	/* Active requests, by target/LUN */
	struct		sci_req *sc_matrix[8][8];

	/* Message stuff */
	int	sc_prevphase;

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
int 	ncr5380_intr __P((struct ncr5380_softc *));
int 	ncr5380_scsi_cmd __P((struct scsi_xfer *));
int 	ncr5380_pio_in __P((struct ncr5380_softc *, int, int, u_char *));
int 	ncr5380_pio_out __P((struct ncr5380_softc *, int, int, u_char *));

#ifdef	DEBUG
struct ncr5380_softc *ncr5380_debug_sc;
void ncr5380_trace __P((char *msg, long val));
#define	NCR_TRACE(msg, val) ncr5380_trace(msg, val)
#else
#define	NCR_TRACE(msg, val)	/* nada */
#endif
