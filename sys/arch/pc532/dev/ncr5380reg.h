/*	$NetBSD: ncr5380reg.h,v 1.4 1995/11/30 00:58:54 jtc Exp $	*/

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

#ifndef _NCR5380REG_H
#define _NCR5380REG_H
/*
 * NCR5380 common interface definitions.
 */

/*
 * Register numbers: (first argument to GET/SET_5380_REG )
 */
#define	NCR5380_DATA	0		/* Data register		*/
#define	NCR5380_ICOM	1		/* Initiator command register	*/
#define	NCR5380_MODE	2		/* Mode register		*/
#define	NCR5380_TCOM	3		/* Target command register	*/
#define	NCR5380_IDSTAT	4		/* Bus status register		*/
#define	NCR5380_DMSTAT	5		/* DMA status register		*/
#define	NCR5380_TRCV	6		/* Target receive register	*/
#define	NCR5380_IRCV	7		/* Initiator receive register	*/

/*
 * Definitions for Initiator command register.
 */
#define	SC_A_RST	0x80	/* RW - Assert RST			*/
#define	SC_TEST		0x40	/* W  - Test mode			*/
#define	SC_AIP		0x40	/* R  - Arbitration in progress		*/
#define	SC_LA		0x20	/* R  - Lost arbitration		*/
#define	SC_A_ACK	0x10	/* RW - Assert ACK			*/
#define	SC_A_BSY	0x08	/* RW - Assert BSY			*/
#define	SC_A_SEL	0x04	/* RW - Assert SEL			*/
#define	SC_A_ATN	0x02	/* RW - Assert ATN			*/
#define	SC_ADTB		0x01	/* RW - Assert Data To Bus		*/

/*
 * Definitions for mode register
 */
#define	SC_B_DMA	0x80	/* RW - Block mode DMA (not on TT!)	*/
#define	SC_T_MODE	0x40	/* RW - Target mode			*/
#define	SC_E_PAR	0x20	/* RW - Enable parity check		*/
#define	SC_E_PARI	0x10	/* RW - Enable parity interrupt		*/
#define	SC_E_EOPI	0x08	/* RW - Enable End Of Process Interrupt	*/
#define	SC_MON_BSY	0x04	/* RW - Monitor BSY			*/
#define	SC_M_DMA	0x02	/* RW - Set DMA mode			*/
#define	SC_ARBIT	0x01	/* RW - Arbitrate			*/

/*
 * Definitions for tcom register
 */
#define	SC_LBS		0x80	/* RW - Last Byte Send (not on TT!)	*/
#define	SC_A_REQ	0x08	/* RW - Assert REQ			*/
#define	SC_A_MSG	0x04	/* RW - Assert MSG			*/
#define	SC_A_CD		0x02	/* RW - Assert C/D			*/
#define	SC_A_IO		0x01	/* RW - Assert I/O			*/

/*
 * Definitions for idstat register
 */
#define	SC_S_RST	0x80	/* R  - RST is set			*/
#define	SC_S_BSY	0x40	/* R  - BSY is set			*/
#define	SC_S_REQ	0x20	/* R  - REQ is set			*/
#define	SC_S_MSG	0x10	/* R  - MSG is set			*/
#define	SC_S_CD		0x08	/* R  - C/D is set			*/
#define	SC_S_IO		0x04	/* R  - I/O is set			*/
#define	SC_S_SEL	0x02	/* R  - SEL is set			*/
#define	SC_S_PAR	0x01	/* R  - Parity bit			*/

/*
 * Definitions for dmastat register
 */
#define	SC_END_DMA	0x80	/* R  - End of DMA			*/
#define	SC_DMA_REQ	0x40	/* R  - DMA request			*/
#define	SC_PAR_ERR	0x20	/* R  - Parity error			*/
#define	SC_IRQ_SET	0x10	/* R  - IRQ is active			*/
#define	SC_PHS_MTCH	0x08	/* R  - Phase Match			*/
#define	SC_BSY_ERR	0x04	/* R  - Busy error			*/
#define	SC_ATN_STAT	0x02	/* R  - State of ATN line		*/
#define	SC_ACK_STAT	0x01	/* R  - State of ACK line		*/
#define	SC_S_SEND	0x00	/* W  - Start DMA output		*/

#define	SC_CLINT	{ 		/* Clear interrupts	*/	\
			int i = GET_5380_REG(NCR5380_IRCV);		\
			}


/*
 * Definition of SCSI-bus phases. The values are determined by signals
 * on the SCSI-bus. DO NOT CHANGE!
 * The values must be used to index the pointers in SCSI-PARMS.
 */
#define	NR_PHASE	8
#define	PH_DATAOUT	0
#define	PH_DATAIN	1
#define	PH_CMD		2
#define	PH_STATUS	3
#define	PH_RES1		4
#define	PH_RES2		5
#define	PH_MSGOUT	6
#define	PH_MSGIN	7

#define	PH_OUT(phase)	(!(phase & 1))	/* TRUE if output phase		*/
#define	PH_IN(phase)	(phase & 1)	/* TRUE if input phase		*/

/*
 * Id of Host-adapter
 */
#define SC_HOST_ID	0x80

/*
 * Base setting for 5380 mode register
 */
#define	IMODE_BASE	SC_E_PAR

/*
 * SCSI completion status codes, should move to sys/scsi/????
 */
#define SCSMASK		0x1e	/* status code mask			*/
#define SCSGOOD		0x00	/* good status				*/
#define SCSCHKC		0x02	/* check condition			*/
#define SCSBUSY		0x08	/* busy status				*/
#define SCSCMET		0x04	/* condition met / good			*/

/*
 * Return values of check_intr()
 */
#define	INTR_SPURIOUS	0
#define	INTR_RESEL	2
#define	INTR_DMA	3

struct	ncr_softc {
	struct	device		sc_dev;
	struct	scsi_link	sc_link;

	/*
	 * Some (pre-SCSI2) devices don't support select with ATN.
	 * If the device responds to select with ATN by going into
	 * command phase (ignoring ATN), then we flag it in the
	 * following bitmask.
	 * We also keep track of which devices have been selected
	 * before.  This allows us to not even try raising ATN if
	 * the target doesn't respond to it the first time.
	 */
	u_int8_t	sc_noselatn;
	u_int8_t	sc_selected;
};

/*
 * Max. number of dma-chains per request
 */
#define	MAXDMAIO	(MAXPHYS/NBPG + 1)

/*
 * Some requests are not contiguous in physical memory. We need to break them
 * up into contiguous parts for DMA.
 */
struct dma_chain {
	u_int	dm_count;
	u_long	dm_addr;
};

/*
 * Define our issue, free and disconnect queue's.
 */
typedef struct	req_q {
    struct req_q	*next;	    /* next in free, issue or discon queue  */
    struct req_q	*link;	    /* next linked command to execute       */
    struct scsi_xfer	*xs;	    /* request from high-level driver       */
    u_short		dr_flag;    /* driver state			    */
    u_char		phase;	    /* current SCSI phase		    */
    u_char		msgout;	    /* message to send when requested       */
    u_char		targ_id;    /* target for command		    */
    u_char		targ_lun;   /* lun for command			    */
    u_char		status;	    /* returned status byte		    */
    u_char		message;    /* returned message byte		    */
    u_char		*bounceb;   /* allocated bounce buffer		    */
    u_char		*bouncerp;  /* bounce read-pointer		    */
    struct dma_chain	dm_chain[MAXDMAIO];
    struct dma_chain	*dm_cur;    /* current dma-request		    */
    struct dma_chain	*dm_last;   /* last dma-request			    */
    long		xdata_len;  /* length of transfer		    */
    u_char		*xdata_ptr; /* virtual address of transfer	    */
    struct scsi_generic	xcmd;	    /* command to execute		    */
} SC_REQ;

/*
 * Values for dr_flag:
 */
#define	DRIVER_IN_DMA	0x01	/* Non-polled DMA activated		*/
#define	DRIVER_AUTOSEN	0x02	/* Doing automatic sense		*/
#define	DRIVER_NOINT	0x04	/* We are booting: no interrupts	*/
#define	DRIVER_DMAOK	0x08	/* DMA can be used on this request	*/
#define	DRIVER_BOUNCING	0x10	/* Using the bounce buffer		*/

/* XXX: Should go to ncr5380var.h */
static SC_REQ	*issue_q   = NULL;	/* Commands waiting to be issued*/
static SC_REQ	*discon_q  = NULL;	/* Commands disconnected	*/
static SC_REQ	*connected = NULL;	/* Command currently connected	*/

/*
 * Function decls:
 */
static int  transfer_pio __P((u_char *, u_char *, u_long *, int));
static int  wait_req_true __P((void));
static int  wait_req_false __P((void));
static int  scsi_select __P((SC_REQ *, int));
static int  handle_message __P((SC_REQ *, u_int));
static void ack_message __P((void));
static void nack_message __P((SC_REQ *, u_char));
static int  information_transfer __P((void));
static void reselect __P((struct ncr_softc *));
static int  dma_ready __P((void));
static void transfer_dma __P((SC_REQ *, u_int, int));
static int  check_autosense __P((SC_REQ *, int));
static int  reach_msg_out __P((struct ncr_softc *, u_long));
static int  check_intr __P((struct ncr_softc *));
static void scsi_reset __P((struct ncr_softc *));
static int  scsi_dmaok __P((SC_REQ *));
static void run_main __P((struct ncr_softc *));
static void scsi_main __P((struct ncr_softc *));
static void ncr_ctrl_intr __P((struct ncr_softc *));
static void ncr_dma_intr __P((struct ncr_softc *));
static void ncr_tprint __P((SC_REQ *, char *, ...));
static void ncr_aprint __P((struct ncr_softc *, char *, ...));

static void show_request __P((SC_REQ *, char *));
static void show_phase __P((SC_REQ *, int));
static void show_signals __P((u_char, u_char));

#endif /* _NCR5380REG_H */
