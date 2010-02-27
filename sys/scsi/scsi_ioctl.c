/*	$OpenBSD: scsi_ioctl.c,v 1.40 2010/02/27 00:03:53 krw Exp $	*/
/*	$NetBSD: scsi_ioctl.c,v 1.23 1996/10/12 23:23:17 christos Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Contributed by HD Associates (hd@world.std.com).
 * Copyright (c) 1992, 1993 HD Associates
 *
 * Berkeley style copyright.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/fcntl.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <sys/scsiio.h>

int			scsi_ioc_cmd(struct scsi_link *, scsireq_t *);

const unsigned char scsi_readsafe_cmd[256] = {
	[0x00] = 1,	/* TEST UNIT READY */
	[0x03] = 1,	/* REQUEST SENSE */
	[0x08] = 1,	/* READ(6) */
	[0x12] = 1,	/* INQUIRY */
	[0x1a] = 1,	/* MODE SENSE */
	[0x1b] = 1,	/* START STOP */
	[0x23] = 1,	/* READ FORMAT CAPACITIES */
	[0x25] = 1,	/* READ CDVD CAPACITY */
	[0x28] = 1,	/* READ(10) */
	[0x2b] = 1,	/* SEEK */
	[0x2f] = 1,	/* VERIFY(10) */
	[0x3c] = 1,	/* READ BUFFER */
	[0x3e] = 1,	/* READ LONG */
	[0x42] = 1,	/* READ SUBCHANNEL */
	[0x43] = 1,	/* READ TOC PMA ATIP */
	[0x44] = 1,	/* READ HEADER */
	[0x45] = 1,	/* PLAY AUDIO(10) */
	[0x46] = 1,	/* GET CONFIGURATION */
	[0x47] = 1,	/* PLAY AUDIO MSF */
	[0x48] = 1,	/* PLAY AUDIO TI */
	[0x4a] = 1,	/* GET EVENT STATUS NOTIFICATION */
	[0x4b] = 1,	/* PAUSE RESUME */
	[0x4e] = 1,	/* STOP PLAY SCAN */
	[0x51] = 1,	/* READ DISC INFO */
	[0x52] = 1,	/* READ TRACK RZONE INFO */
	[0x5a] = 1,	/* MODE SENSE(10) */
	[0x88] = 1,	/* READ(16) */
	[0x8f] = 1,	/* VERIFY(16) */
	[0xa4] = 1,	/* REPORT KEY */
	[0xa5] = 1,	/* PLAY AUDIO(12) */
	[0xa8] = 1,	/* READ(12) */
	[0xac] = 1,	/* GET PERFORMANCE */
	[0xad] = 1,	/* READ DVD STRUCTURE */
	[0xb9] = 1,	/* READ CD MSF */
	[0xba] = 1,	/* SCAN */
	[0xbc] = 1,	/* PLAY CD */
	[0xbd] = 1,	/* MECHANISM STATUS */
	[0xbe] = 1	/* READ CD */
};

int
scsi_ioc_cmd(struct scsi_link *link, scsireq_t *screq)
{
	struct scsi_xfer *xs;
	int err = 0;

	if (screq->cmdlen > sizeof(struct scsi_generic))
		return (EFAULT);

	xs = scsi_xs_get(link, 0);
	if (xs == NULL)
		return (ENOMEM);

	memcpy(xs->cmd, screq->cmd, screq->cmdlen);
	xs->cmdlen = screq->cmdlen;

	if (screq->datalen > 0) {
		xs->data = malloc(screq->datalen, M_TEMP, M_WAITOK);
		xs->datalen = screq->datalen;
	}

	if (screq->flags & SCCMD_READ)
		xs->flags |= SCSI_DATA_IN;
	if (screq->flags & SCCMD_WRITE) {
		if (screq->datalen > 0) {
			err = copyin(screq->databuf, xs->data, screq->datalen);
			if (err != 0)
				goto err;
		}

		xs->flags |= SCSI_DATA_OUT;
	}

	xs->flags |= SCSI_SILENT;	/* User is responsible for errors. */
	xs->timeout = screq->timeout;
	xs->retries = 0; /* user must do the retries *//* ignored */

	scsi_xs_sync(xs);

	screq->retsts = 0;
	screq->status = xs->status;
	switch (xs->error) {
	case XS_NOERROR:
		/* probably rubbish */
		screq->datalen_used = xs->datalen - xs->resid;
		screq->retsts = SCCMD_OK;
		break;
	case XS_SENSE:
		screq->senselen_used = min(sizeof(xs->sense),
		    sizeof(screq->sense));
		bcopy(&xs->sense, screq->sense, screq->senselen_used);
		screq->retsts = SCCMD_SENSE;
		break;
	case XS_SHORTSENSE:
		printf("XS_SHORTSENSE\n");
		screq->senselen_used = min(sizeof(xs->sense),
		    sizeof(screq->sense));
		bcopy(&xs->sense, screq->sense, screq->senselen_used);
		screq->retsts = SCCMD_UNKNOWN;
		break;
	case XS_DRIVER_STUFFUP:
		screq->retsts = SCCMD_UNKNOWN;
		break;
	case XS_TIMEOUT:
		screq->retsts = SCCMD_TIMEOUT;
		break;
	case XS_BUSY:
		screq->retsts = SCCMD_BUSY;
		break;
	default:
		screq->retsts = SCCMD_UNKNOWN;
		break;
	}

	if (screq->datalen > 0 && screq->flags & SCCMD_READ) {
		err = copyout(xs->data, screq->databuf, screq->datalen);
		if (err != 0)
			goto err;
	}

err:
	if (screq->datalen > 0)
		free(xs->data, M_TEMP);
	scsi_xs_put(xs);

	return (err);
}

/*
 * Something (e.g. another driver) has called us
 * with an sc_link for a target/lun/adapter, and a scsi
 * specific ioctl to perform, better try.
 * If user-level type command, we must still be running
 * in the context of the calling process
 */
int
scsi_do_ioctl(struct scsi_link *sc_link, dev_t dev, u_long cmd, caddr_t addr,
    int flag, struct proc *p)
{
	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_do_ioctl(0x%lx)\n", cmd));

	switch(cmd) {
	case SCIOCIDENTIFY: {
		struct scsi_addr *sca = (struct scsi_addr *)addr;

		if ((sc_link->flags & (SDEV_ATAPI | SDEV_UMASS)) == 0)
			/* A 'real' SCSI target. */
			sca->type = TYPE_SCSI;
		else	
			/* An 'emulated' SCSI target. */
			sca->type = TYPE_ATAPI;
		sca->scbus = sc_link->scsibus;
		sca->target = sc_link->target;
		sca->lun = sc_link->lun;
		return (0);
	}
	case SCIOCCOMMAND:
		if (scsi_readsafe_cmd[((scsireq_t *)addr)->cmd[0]])
			break;
		/* FALLTHROUGH */
	case SCIOCDEBUG:
		if ((flag & FWRITE) == 0)
			return (EPERM);
		break;
	default:
		if (sc_link->adapter->ioctl)
			return ((sc_link->adapter->ioctl)(sc_link, cmd, addr,
			    flag, p));
		else
			return (ENOTTY);
	}

	switch(cmd) {
	case SCIOCCOMMAND:
		return (scsi_ioc_cmd(sc_link, (scsireq_t *)addr));
	case SCIOCDEBUG: {
		int level = *((int *)addr);

		SC_DEBUG(sc_link, SDEV_DB3, ("debug set to %d\n", level));
		sc_link->flags &= ~SDEV_DBX; /* clear debug bits */
		if (level & 1)
			sc_link->flags |= SDEV_DB1;
		if (level & 2)
			sc_link->flags |= SDEV_DB2;
		if (level & 4)
			sc_link->flags |= SDEV_DB3;
		if (level & 8)
			sc_link->flags |= SDEV_DB4;
		return (0);
	}
	default:
#ifdef DIAGNOSTIC
		panic("scsi_do_ioctl: impossible cmd (%#lx)", cmd);
#endif
		return (0);
	}
}
