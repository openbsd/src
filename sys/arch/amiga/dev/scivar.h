/*	$OpenBSD: scivar.h,v 1.2 1996/05/02 06:44:29 niklas Exp $	*/
/*	$NetBSD: scivar.h,v 1.10 1996/04/28 06:41:01 mhitch Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)scivar.h	7.1 (Berkeley) 5/8/90
 */
#ifndef _SCIVAR_H_
#define _SCIVAR_H_

struct	sci_pending {
	TAILQ_ENTRY(sci_pending) link;
	struct scsi_xfer *xs;
};

struct sci_softc;

struct	sci_softc {
	struct	device sc_dev;
	struct	isr sc_isr;
	struct	scsi_link sc_link;	/* proto for sub devices */
	TAILQ_HEAD(,sci_pending) sc_xslist;
	struct	sci_pending sc_xsstore[8][8];
	struct	scsi_xfer *sc_xs;	/* transfer from high level code */

	volatile u_char	*sci_data;	/* r: Current data */
	volatile u_char	*sci_odata;	/* w: Out data */
	volatile u_char	*sci_icmd;	/* rw: Initiator command */
	volatile u_char	*sci_mode;	/* rw: Mode */
	volatile u_char	*sci_tcmd;	/* rw: Target command */
	volatile u_char	*sci_bus_csr;	/* r: Bus Status */
	volatile u_char	*sci_sel_enb;	/* w: Select enable */
	volatile u_char	*sci_csr;	/* r: Status */
	volatile u_char	*sci_dma_send;	/* w: Start dma send data */
	volatile u_char	*sci_idata;	/* r: Input data */
	volatile u_char	*sci_trecv;	/* w: Start dma receive, target */
	volatile u_char	*sci_iack;	/* r: Interrupt Acknowledge */
	volatile u_char	*sci_irecv;	/* w: Start dma receive, initiator */

	/* psuedo DMA transfer */
	int	(*dma_xfer_in) __P((struct sci_softc *, int, u_char *, int));
	/* psuedo DMA transfer */
	int	(*dma_xfer_out) __P((struct sci_softc *, int, u_char *, int));
	u_char	sc_flags;
	u_char	sc_lun;
	/* one for each target */
	struct syncpar {
	  u_char state;
	  u_char period, offset;
	} sc_sync[8];
	u_char	sc_slave;
	u_char	sc_scsi_addr;
	u_char	sc_stat[2];
	u_char	sc_msg[8];
};

/* sc_flags */
#define	SCI_IO		0x80	/* DMA I/O in progress */
#define	SCI_ALIVE	0x01	/* controller initialized */
#define SCI_SELECTED	0x04	/* bus is in selected state. Needed for
				   correct abort procedure. */

/* sync states */
#define SYNC_START	0	/* no sync handshake started */
#define SYNC_SENT	1	/* we sent sync request, no answer yet */
#define SYNC_DONE	2	/* target accepted our (or inferior) settings,
				   or it rejected the request and we stay async */

#define	PHASE		0x07		/* mask for psns/pctl phase */
#define	DATA_OUT_PHASE	0x00
#define	DATA_IN_PHASE	0x01
#define	CMD_PHASE	0x02
#define	STATUS_PHASE	0x03
#define	BUS_FREE_PHASE	0x04
#define	ARB_SEL_PHASE	0x05	/* Fuji chip combines arbitration with sel. */
#define	MESG_OUT_PHASE	0x06
#define	MESG_IN_PHASE	0x07

#define	MSG_CMD_COMPLETE	0x00
#define MSG_EXT_MESSAGE		0x01
#define	MSG_SAVE_DATA_PTR	0x02
#define	MSG_RESTORE_PTR		0x03
#define	MSG_DISCONNECT		0x04
#define	MSG_INIT_DETECT_ERROR	0x05
#define	MSG_ABORT		0x06
#define	MSG_REJECT		0x07
#define	MSG_NOOP		0x08
#define	MSG_PARITY_ERROR	0x09
#define	MSG_BUS_DEVICE_RESET	0x0C
#define	MSG_IDENTIFY		0x80
#define	MSG_IDENTIFY_DR		0xc0	/* (disconnect/reconnect allowed) */
#define	MSG_SYNC_REQ 		0x01


#define	STS_CHECKCOND	0x02	/* Check Condition (ie., read sense) */
#define	STS_CONDMET	0x04	/* Condition Met (ie., search worked) */
#define	STS_BUSY	0x08
#define	STS_INTERMED	0x10	/* Intermediate status sent */
#define	STS_EXT		0x80	/* Extended status valid */

/*
 * XXXX
 */
struct scsi_fmt_cdb {
	int len;		/* cdb length (in bytes) */
	u_char cdb[28];		/* cdb to use on next read/write */
};

struct buf;
struct scsi_xfer;

void sci_minphys __P((struct buf *));
int sci_scsicmd __P((struct scsi_xfer *));
void scireset __P((struct sci_softc *));

#endif /* _SCIVAR_H_ */
