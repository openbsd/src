/*	$NetBSD: siivar.h,v 1.1 1996/10/13 03:02:41 jonathan Exp $	*/

#ifndef _SIIVAR_H
#define _SIIVAR_H

typedef struct scsi_state {
	int	statusByte;	/* status byte returned during STATUS_PHASE */
	int	dmaDataPhase;	/* which data phase to expect */
	int	dmaCurPhase;	/* SCSI phase if DMA is in progress */
	int	dmaPrevPhase;	/* SCSI phase of DMA suspended by disconnect */
	u_short	*dmaAddr[2];	/* DMA buffer memory address */
	int	dmaBufIndex;	/* which of the above is currently in use */
	int	dmalen;		/* amount to transfer in this chunk */
	int	cmdlen;		/* total remaining amount of cmd to transfer */
	u_char	*cmd;		/* current pointer within scsicmd->cmd */
	int	buflen;		/* total remaining amount of data to transfer */
	char	*buf;		/* current pointer within scsicmd->buf */
	u_short	flags;		/* see below */
	u_short	prevComm;	/* command reg before disconnect */
	u_short	dmaCtrl;	/* DMA control register if disconnect */
	u_short	dmaAddrL;	/* DMA address register if disconnect */
	u_short	dmaAddrH;	/* DMA address register if disconnect */
	u_short	dmaCnt;		/* DMA count if disconnect */
	u_short	dmaByte;	/* DMA byte if disconnect on odd boundary */
	u_short	dmaReqAck;	/* DMA synchronous xfer offset or 0 if async */
} State;

/* state flags */
#define FIRST_DMA	0x01	/* true if no data DMA started yet */
#define PARITY_ERR	0x02	/* true if parity error seen */

#define SII_NCMD	7
struct siisoftc {
	struct device sc_dev;		/* us as a device */
	void *sc_buf;			/* DMA buffer (may be special mem) */
	SIIRegs	*sc_regs;		/* HW address of SII controller chip */
	int	sc_flags;
	int	sc_target;		/* target SCSI ID if connected */
	ScsiCmd	*sc_cmd[SII_NCMD];	/* active command indexed by ID */
	State	sc_st[SII_NCMD];	/* state info for each active command */
#ifdef NEW_SCSI
	struct scsi_link sc_link;		/* scsi lint struct */
#endif
};

int siiintr __P((void *sc));

/* Machine-indepedent back-end attach entry point */
void	siiattach __P((struct siisoftc *sc));

#endif	/* _SIIVAR_H */
