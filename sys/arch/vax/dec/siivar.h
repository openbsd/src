/*	$OpenBSD: siivar.h,v 1.3 2008/08/30 20:13:03 miod Exp $	*/
/*	$NetBSD: siivar.h,v 1.6 2000/06/02 20:16:51 mhitch Exp $	*/

#ifndef _SIIVAR_H
#define _SIIVAR_H

typedef struct scsi_state {
	int	statusByte;	/* status byte returned during STATUS_PHASE */
	int	dmaDataPhase;	/* which data phase to expect */
	int	dmaCurPhase;	/* SCSI phase if DMA is in progress */
	int	dmaPrevPhase;	/* SCSI phase of DMA suspended by disconnect */
	u_int	dmaAddr[2];	/* DMA buffer memory offsets */
	int	dmaBufIndex;	/* which of the above is currently in use */
	int	dmalen;		/* amount to transfer in this chunk */
	int	cmdlen;		/* total remaining amount of cmd to transfer */
	u_char	*cmd;		/* current pointer within scsicmd->cmd */
	int	buflen;		/* total remaining amount of data to transfer */
	char	*buf;		/* current pointer within scsicmd->buf */
	u_short	flags;		/* see below */
	u_int8_t nextCmd;	/* next command to send if PENDING_CMD set */
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
#define	PENDING_CMD	0x02	/* need to send a particular command */

#define SII_NCMD	8
struct sii_softc {
	struct device sc_dev;		/* us as a device */
	struct scsi_link sc_link;	/* scsi link struct */
	SIIRegs	*sc_regs;		/* HW address of SII controller chip */
	int	sc_flags;
	int	sc_target;		/* target SCSI ID if connected */
	int	sc_hostid;
	void	(*sii_copytobuf)(void *, u_char *, u_int, int);
 	void	(*sii_copyfrombuf)(void *, u_int, u_char *, int);

	struct scsi_xfer *sc_xs[SII_NCMD]; /* currently executing requests */
	State	sc_st[SII_NCMD];	/* state info for each active command */

	u_char	sc_buf[258];		/* used for extended messages */
};

/* Machine-independent back-end attach entry point */
void	sii_attach(struct sii_softc *sc);
int	sii_intr(void *sc);

#endif	/* _SIIVAR_H */
