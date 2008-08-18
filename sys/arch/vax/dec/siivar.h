/*	$OpenBSD: siivar.h,v 1.1 2008/08/18 23:19:22 miod Exp $	*/
/*	$NetBSD: siivar.h,v 1.6 2000/06/02 20:16:51 mhitch Exp $	*/

#ifndef _SIIVAR_H
#define _SIIVAR_H

/*
 * This structure contains information that a SCSI interface controller
 * needs to execute a SCSI command.
 */
typedef struct ScsiCmd {
	int	unit;		/* unit number passed to device done routine */
	int	flags;		/* control flags for this command (see below) */
	int	buflen;		/* length of the data buffer in bytes */
	char	*buf;		/* pointer to data buffer for this command */
	int	cmdlen;		/* length of data in cmdbuf */
	u_char	*cmd;		/* buffer for the SCSI command */
	int	error;		/* compatibility hack for new scsi */
	int	lun;		/* LUN for MI SCSI */
} ScsiCmd;

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

#define SII_NCMD	8
struct sii_softc {
	struct device sc_dev;		/* us as a device */
	struct scsi_link sc_link;		/* scsi link struct */
	ScsiCmd sc_cmd_fake[SII_NCMD];		/* XXX - hack!!! */
	struct scsi_xfer *sc_xs[SII_NCMD];	/* XXX - hack!!! */
	SIIRegs	*sc_regs;		/* HW address of SII controller chip */
	int	sc_flags;
	int	sc_target;		/* target SCSI ID if connected */
	int	sc_hostid;
	ScsiCmd	*sc_cmd[SII_NCMD];	/* active command indexed by ID */
	void	(*sii_copytobuf)(void *, u_char *, u_int, int);
 	void	(*sii_copyfrombuf)(void *, u_int, u_char *, int);

	State	sc_st[SII_NCMD];	/* state info for each active command */

	u_char	sc_buf[258];	/* used for extended messages */
};

/* Machine-independent back-end attach entry point */
void	sii_attach(struct sii_softc *sc);
int	sii_intr(void *sc);

#endif	/* _SIIVAR_H */
