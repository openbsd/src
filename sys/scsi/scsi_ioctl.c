/*	$OpenBSD: scsi_ioctl.c,v 1.21 2005/10/10 20:06:11 krw Exp $	*/
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

struct scsi_ioctl {
	LIST_ENTRY(scsi_ioctl) si_list;
	struct buf si_bp;
	struct uio si_uio;
	struct iovec si_iov;
	scsireq_t si_screq;
	struct scsi_link *si_sc_link;
};

LIST_HEAD(, scsi_ioctl) si_head;

struct scsi_ioctl *si_get(void);
void si_free(struct scsi_ioctl *);
struct scsi_ioctl *si_find(struct buf *);
void scsistrategy(struct buf *);

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

struct scsi_ioctl *
si_get(void)
{
	struct scsi_ioctl *si;
	int s;

	si = malloc(sizeof(struct scsi_ioctl), M_TEMP, M_WAITOK);
	bzero(si, sizeof(struct scsi_ioctl));
	s = splbio();
	LIST_INSERT_HEAD(&si_head, si, si_list);
	splx(s);
	return (si);
}

void
si_free(struct scsi_ioctl *si)
{
	int s;

	s = splbio();
	LIST_REMOVE(si, si_list);
	splx(s);
	free(si, M_TEMP);
}

struct scsi_ioctl *
si_find(struct buf *bp)
{
	struct scsi_ioctl *si;
	int s;

	s = splbio();
	LIST_FOREACH(si, &si_head, si_list)
		if (bp == &si->si_bp)
			break;
	splx(s);
	return (si);
}

/*
 * We let the user interpret his own sense in the generic scsi world.
 * This routine is called at interrupt time if the SCSI_USER bit was set
 * in the flags passed to scsi_scsi_cmd(). No other completion processing
 * takes place, even if we are running over another device driver.
 * The lower level routines that call us here, will free the xs and restart
 * the device's queue if such exists.
 */
void
scsi_user_done(struct scsi_xfer *xs)
{
	struct buf *bp;
	struct scsi_ioctl *si;
	scsireq_t *screq;
	struct scsi_link *sc_link;

	splassert(IPL_BIO);

	bp = xs->bp;
	if (!bp) {	/* ALL user requests must have a buf */
		sc_print_addr(xs->sc_link);
		printf("User command with no buf\n");
		return;
	}
	si = si_find(bp);
	if (!si) {
		sc_print_addr(xs->sc_link);
		printf("User command with no ioctl\n");
		return;
	}
	screq = &si->si_screq;
	sc_link = si->si_sc_link;
	SC_DEBUG(xs->sc_link, SDEV_DB2, ("user-done\n"));

	screq->retsts = 0;
	screq->status = xs->status;
	switch (xs->error) {
	case XS_NOERROR:
		SC_DEBUG(sc_link, SDEV_DB3, ("no error\n"));
		screq->datalen_used = xs->datalen - xs->resid; /* probably rubbish */
		screq->retsts = SCCMD_OK;
		break;
	case XS_SENSE:
		SC_DEBUG(sc_link, SDEV_DB3, ("have sense\n"));
		screq->senselen_used = min(sizeof(xs->sense), SENSEBUFLEN);
		bcopy(&xs->sense, screq->sense, screq->senselen);
		screq->retsts = SCCMD_SENSE;
		break;
	case XS_SHORTSENSE:
		SC_DEBUG(sc_link, SDEV_DB3, ("have short sense\n"));
		screq->senselen_used = min(sizeof(xs->sense), SENSEBUFLEN);
		bcopy(&xs->sense, screq->sense, screq->senselen);
		screq->retsts = SCCMD_UNKNOWN;
		break;
	case XS_DRIVER_STUFFUP:
		sc_print_addr(sc_link);
		printf("host adapter code inconsistency\n");
		screq->retsts = SCCMD_UNKNOWN;
		break;
	case XS_TIMEOUT:
		SC_DEBUG(sc_link, SDEV_DB3, ("timeout\n"));
		screq->retsts = SCCMD_TIMEOUT;
		break;
	case XS_BUSY:
		SC_DEBUG(sc_link, SDEV_DB3, ("busy\n"));
		screq->retsts = SCCMD_BUSY;
		break;
	default:
		sc_print_addr(sc_link);
		printf("unknown error category (0x%x) from host adapter code\n",
		    xs->error);
		screq->retsts = SCCMD_UNKNOWN;
		break;
	}
	biodone(bp); 	/* we're waiting on it in scsi_strategy() */
}


/* Pseudo strategy function
 * Called by scsi_do_ioctl() via physio/physstrat if there is to
 * be data transferred, and directly if there is no data transfer.
 * 
 * Should I reorganize this so it returns to physio instead
 * of sleeping in scsiio_scsi_cmd?  Is there any advantage, other
 * than avoiding the probable duplicate wakeup in iodone? [PD]
 *
 * No, seems ok to me... [JRE]
 * (I don't see any duplicate wakeups)
 *
 * Can't be used with block devices or raw_read/raw_write directly
 * from the cdevsw/bdevsw tables because they couldn't have added
 * the screq structure. [JRE]
 */
void
scsistrategy(struct buf *bp)
{
	struct scsi_ioctl *si;
	scsireq_t *screq;
	struct scsi_link *sc_link;
	int error;
	int flags = 0;
	int s;

	si = si_find(bp);
	if (!si) {
		printf("user_strat: No ioctl\n");
		error = EINVAL;
		goto bad;
	}
	screq = &si->si_screq;
	sc_link = si->si_sc_link;
	SC_DEBUG(sc_link, SDEV_DB2, ("user_strategy\n"));

	/*
	 * We're in trouble if physio tried to break up the transfer.
	 */
	if (bp->b_bcount != screq->datalen) {
		sc_print_addr(sc_link);
		printf("physio split the request.. cannot proceed\n");
		error = EIO;
		goto bad;
	}

	if (screq->timeout == 0) {
		error = EINVAL;
		goto bad;
	}

	if (screq->cmdlen > sizeof(struct scsi_generic)) {
		sc_print_addr(sc_link);
		printf("cmdlen too big\n");
		error = EFAULT;
		goto bad;
	}

	if (screq->flags & SCCMD_READ)
		flags |= SCSI_DATA_IN;
	if (screq->flags & SCCMD_WRITE)
		flags |= SCSI_DATA_OUT;
	if (screq->flags & SCCMD_TARGET)
		flags |= SCSI_TARGET;
	if (screq->flags & SCCMD_ESCAPE)
		flags |= SCSI_ESCAPE;

	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *)screq->cmd,
	    screq->cmdlen, (u_char *)bp->b_data, screq->datalen,
	    0, /* user must do the retries *//* ignored */
	    screq->timeout, bp, flags | SCSI_USER | SCSI_NOSLEEP);

	/* because there is a bp, scsi_scsi_cmd will return immediatly */
	if (error)
		goto bad;

	SC_DEBUG(sc_link, SDEV_DB3, ("about to sleep\n"));
	s = splbio();
	while ((bp->b_flags & B_DONE) == 0)
		tsleep(bp, PRIBIO, "scistr", 0);
	splx(s);
	SC_DEBUG(sc_link, SDEV_DB3, ("back from sleep\n"));

	return;

bad:
	bp->b_flags |= B_ERROR;
	bp->b_error = error;
	s = splbio();
	biodone(bp);
	splx(s);
}

/*
 * Something (e.g. another driver) has called us
 * with an sc_link for a target/lun/adapter, and a scsi
 * specific ioctl to perform, better try.
 * If user-level type command, we must still be running
 * in the context of the calling process
 */
int
scsi_do_ioctl( struct scsi_link *sc_link, dev_t dev, u_long cmd, caddr_t addr,
    int flag, struct proc *p)
{
	int error;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_do_ioctl(0x%lx)\n", cmd));

	switch(cmd) {
	case OSCIOCIDENTIFY: {
		struct oscsi_addr *sca = (struct oscsi_addr *)addr;

		sca->scbus = sc_link->scsibus;
		sca->target = sc_link->target;
		sca->lun = sc_link->lun;
		return (0);
	}
	case SCIOCIDENTIFY: {
		struct scsi_addr *sca = (struct scsi_addr *)addr;

		sca->type = (sc_link->flags & SDEV_ATAPI) 
			? TYPE_ATAPI : TYPE_SCSI;
		sca->scbus = sc_link->scsibus;
		sca->target = sc_link->target;
		sca->lun = sc_link->lun;
		return (0);
	}
	case SCIOCRECONFIG:
	case SCIOCDECONFIG:
		return (EINVAL);
	case SCIOCCOMMAND:
		if (scsi_readsafe_cmd[((scsireq_t *)addr)->cmd[0]])
			break;
		/* FALLTHROUGH */	
	case SCIOCDEBUG:
	case SCIOCREPROBE:
	case OSCIOCREPROBE:
	case SCIOCRESET:
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
	case SCIOCCOMMAND: {
		scsireq_t *screq = (scsireq_t *)addr;
		struct scsi_ioctl *si;
		int len;

		si = si_get();
		si->si_screq = *screq;
		si->si_sc_link = sc_link;
		len = screq->datalen;
		if (len) {
			si->si_iov.iov_base = screq->databuf;
			si->si_iov.iov_len = len;
			si->si_uio.uio_iov = &si->si_iov;
			si->si_uio.uio_iovcnt = 1;
			si->si_uio.uio_resid = len;
			si->si_uio.uio_offset = 0;
			si->si_uio.uio_segflg = UIO_USERSPACE;
			si->si_uio.uio_rw = 
			    (screq->flags & SCCMD_READ) ? UIO_READ : UIO_WRITE;
			si->si_uio.uio_procp = p;
			error = physio(scsistrategy, &si->si_bp, dev,
			    (screq->flags & SCCMD_READ) ? B_READ : B_WRITE,
			    sc_link->adapter->scsi_minphys, &si->si_uio);
		} else {
			/* if no data, no need to translate it.. */
			si->si_bp.b_flags = 0;
			si->si_bp.b_data = 0;
			si->si_bp.b_bcount = 0;
			si->si_bp.b_dev = dev;
			si->si_bp.b_proc = p;
			scsistrategy(&si->si_bp);
			error = si->si_bp.b_error;
		}
		*screq = si->si_screq;
		si_free(si);
		return (error);
	}
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
	case OSCIOCREPROBE: {
		struct oscsi_addr *sca = (struct oscsi_addr *)addr;

		return (scsi_probe_busses(sca->scbus, sca->target, sca->lun));
	}
	case SCIOCREPROBE: {
		struct scsi_addr *sca = (struct scsi_addr *)addr;

		return (scsi_probe_busses(sca->scbus, sca->target, sca->lun));
	}
	case SCIOCRESET: {
		scsi_scsi_cmd(sc_link, 0, 0, 0, 0, GENRETRY, 2000, NULL,
		    SCSI_RESET);
		return (0);
	}
	default:
#ifdef DIAGNOSTIC
		panic("scsi_do_ioctl: impossible cmd (%#x)", cmd);
#endif
		return (0);
	}
}
