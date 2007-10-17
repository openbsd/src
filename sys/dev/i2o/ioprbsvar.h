/*	$OpenBSD: ioprbsvar.h,v 1.3 2007/10/17 15:07:37 deraadt Exp $	*/

/*
 * Copyright (c) 2001 Niklas Hallqvist
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _I2O_IOPRBSVAR_H_
#define	_I2O_IOPRBSVAR_H_

/*
 * A command contol block, one for each corresponding command index of the
 * controller.
 */
struct ioprbs_ccb {
	TAILQ_ENTRY(ioprbs_ccb) ic_chain;
	struct scsi_xfer *ic_xs;
#if 0
	struct aac_fib *ac_fib;		/* FIB associated with this command */
	bus_addr_t ac_fibphys;		/* bus address of the FIB */
	bus_dmamap_t ac_dmamap_xfer;
	struct aac_sg_table *ac_sgtable;/* pointer to s/g table in command */
#endif
	int ic_timeout;
	u_int32_t ic_blockno;
	u_int32_t ic_blockcnt;
	u_int8_t ic_flags;
#define IOPRBS_ICF_WATCHDOG 	0x1
#define IOPRBS_ICF_COMPLETED 	0x2
};

/* XXX What is correct? */
#define IOPRBS_MAX_CCBS 256

struct ioprbs_softc {
	struct	device sc_dv;			/* Generic device data */
	struct	scsi_link sc_link;	/* Virtual SCSI bus for cache devs */
	struct	iop_initiator sc_ii;
	struct	iop_initiator sc_eventii;

	int	sc_flags;
	int	sc_secperunit;			/* # sectors in total */
	int	sc_secsize;			/* sector size in bytes */
	int	sc_maxxfer;			/* max xfer size in bytes */
	int	sc_maxqueuecnt;			/* maximum h/w queue depth */
	int	sc_queuecnt;			/* current h/w queue depth */

	struct ioprbs_ccb sc_ccbs[IOPRBS_MAX_CCBS];
	TAILQ_HEAD(, ioprbs_ccb) sc_free_ccb, sc_ccbq;
	/* commands on hold for controller resources */
	TAILQ_HEAD(, ioprbs_ccb) sc_ready;
	/* commands which have been returned by the controller */
	LIST_HEAD(, scsi_xfer) sc_queue;
	struct scsi_xfer *sc_queuelast;
};

#define	IOPRBS_CLAIMED		0x01
#define	IOPRBS_NEW_EVTMASK	0x02
#define IOPRBS_ENABLED		0x04

#define	IOPRBS_TIMEOUT		(30 * 1000)
#define IOPRBS_BLOCK_SIZE	512

/*
 * Wait this long for a lost interrupt to get detected.
 */
#define IOPRBS_WATCH_TIMEOUT	10000		/* 10000 * 1ms = 10s */

#endif	/* !_I2O_IOPRMSVAR_H_ */
