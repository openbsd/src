/*	$NetBSD: scsi_base.c,v 1.33 1996/02/14 21:47:14 christos Exp $	*/

/*
 * Copyright (c) 1994, 1995 Charles Hannum.  All rights reserved.
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
 * Originally written by Julian Elischer (julian@dialix.oz.au)
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/cpu.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

void scsi_error __P((struct scsi_xfer *, int));

LIST_HEAD(xs_free_list, scsi_xfer) xs_free_list;

static __inline struct scsi_xfer *scsi_make_xs __P((struct scsi_link *,
						    struct scsi_generic *,
						    int cmdlen,
						    u_char *data_addr,
						    int datalen,
						    int retries,
						    int timeout,
						    struct buf *,
						    int flags));

int sc_err1 __P((struct scsi_xfer *, int));
int scsi_interpret_sense __P((struct scsi_xfer *));

/*
 * Get a scsi transfer structure for the caller. Charge the structure
 * to the device that is referenced by the sc_link structure. If the 
 * sc_link structure has no 'credits' then the device already has the
 * maximum number or outstanding operations under way. In this stage,
 * wait on the structure so that when one is freed, we are awoken again
 * If the SCSI_NOSLEEP flag is set, then do not wait, but rather, return
 * a NULL pointer, signifying that no slots were available
 * Note in the link structure, that we are waiting on it.
 */

struct scsi_xfer *
scsi_get_xs(sc_link, flags)
	struct scsi_link *sc_link;	/* who to charge the xs to */
	int flags;			/* if this call can sleep */
{
	struct scsi_xfer *xs;
	int s;

	SC_DEBUG(sc_link, SDEV_DB3, ("scsi_get_xs\n"));
	s = splbio();
	while (sc_link->openings <= 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("sleeping\n"));
		if ((flags & SCSI_NOSLEEP) != 0) {
			splx(s);
			return 0;
		}
		sc_link->flags |= SDEV_WAITING;
		(void) tsleep(sc_link, PRIBIO, "getxs", 0);
	}
	sc_link->openings--;
	if ((xs = xs_free_list.lh_first) != NULL) {
		LIST_REMOVE(xs, free_list);
		splx(s);
	} else {
		splx(s);
		SC_DEBUG(sc_link, SDEV_DB3, ("making\n"));
		xs = malloc(sizeof(*xs), M_DEVBUF,
		    ((flags & SCSI_NOSLEEP) != 0 ? M_NOWAIT : M_WAITOK));
		if (!xs) {
			sc_print_addr(sc_link);
			printf("cannot allocate scsi xs\n");
			return 0;
		}
	}

	SC_DEBUG(sc_link, SDEV_DB3, ("returning\n"));
	xs->flags = INUSE | flags;
	return xs;
}

/*
 * Given a scsi_xfer struct, and a device (referenced through sc_link)
 * return the struct to the free pool and credit the device with it
 * If another process is waiting for an xs, do a wakeup, let it proceed
 */
void 
scsi_free_xs(xs, flags)
	struct scsi_xfer *xs;
	int flags;
{
	struct scsi_link *sc_link = xs->sc_link;

	xs->flags &= ~INUSE;
	LIST_INSERT_HEAD(&xs_free_list, xs, free_list);

	SC_DEBUG(sc_link, SDEV_DB3, ("scsi_free_xs\n"));
	/* if was 0 and someone waits, wake them up */
	sc_link->openings++;
	if ((sc_link->flags & SDEV_WAITING) != 0) {
		sc_link->flags &= ~SDEV_WAITING;
		wakeup(sc_link);
	} else {
		if (sc_link->device->start) {
			SC_DEBUG(sc_link, SDEV_DB2, ("calling private start()\n"));
			(*(sc_link->device->start)) (sc_link->device_softc);
		}
	}
}

/*
 * Make a scsi_xfer, and return a pointer to it.
 */
static __inline struct scsi_xfer *
scsi_make_xs(sc_link, scsi_cmd, cmdlen, data_addr, datalen,
	     retries, timeout, bp, flags)
	struct scsi_link *sc_link;
	struct scsi_generic *scsi_cmd;
	int cmdlen;
	u_char *data_addr;
	int datalen;
	int retries;
	int timeout;
	struct buf *bp;
	int flags;
{
	struct scsi_xfer *xs;

	if ((xs = scsi_get_xs(sc_link, flags)) == NULL)
		return NULL;

	/*
	 * Fill out the scsi_xfer structure.  We don't know whose context
	 * the cmd is in, so copy it.
	 */
	xs->sc_link = sc_link;
	bcopy(scsi_cmd, &xs->cmdstore, cmdlen);
	xs->cmd = &xs->cmdstore;
	xs->cmdlen = cmdlen;
	xs->data = data_addr;
	xs->datalen = datalen;
	xs->retries = retries;
	xs->timeout = timeout;
	xs->bp = bp;

	return xs;
}

/*
 * Find out from the device what its capacity is.
 */
u_long
scsi_size(sc_link, flags)
	struct scsi_link *sc_link;
	int flags;
{
	struct scsi_read_cap_data rdcap;
	struct scsi_read_capacity scsi_cmd;
	u_long size;

	/*
	 * make up a scsi command and ask the scsi driver to do
	 * it for you.
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = READ_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks
	 */
	if (scsi_scsi_cmd(sc_link, (struct scsi_generic *)&scsi_cmd,
			  sizeof(scsi_cmd), (u_char *)&rdcap, sizeof(rdcap),
			  2, 20000, NULL, flags | SCSI_DATA_IN) != 0) {
		sc_print_addr(sc_link);
		printf("could not get size\n");
		return 0;
	} else {
		size = rdcap.addr_0 + 1;
		size += rdcap.addr_1 << 8;
		size += rdcap.addr_2 << 16;
		size += rdcap.addr_3 << 24;
	}
	return size;
}

/*
 * Get scsi driver to send a "are you ready?" command
 */
int 
scsi_test_unit_ready(sc_link, flags)
	struct scsi_link *sc_link;
	int flags;
{
	struct scsi_test_unit_ready scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = TEST_UNIT_READY;

	return scsi_scsi_cmd(sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, 2, 10000, NULL, flags);
}

/*
 * Do a scsi operation, asking a device to run as SCSI-II if it can.
 */
int 
scsi_change_def(sc_link, flags)
	struct scsi_link *sc_link;
	int flags;
{
	struct scsi_changedef scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = CHANGE_DEFINITION;
	scsi_cmd.how = SC_SCSI_2;

	return scsi_scsi_cmd(sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, 2, 100000, NULL, flags);
}

/*
 * Do a scsi operation asking a device what it is
 * Use the scsi_cmd routine in the switch table.
 */
int 
scsi_inquire(sc_link, inqbuf, flags)
	struct scsi_link *sc_link;
	struct scsi_inquiry_data *inqbuf;
	int flags;
{
	struct scsi_inquiry scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = INQUIRY;
	scsi_cmd.length = sizeof(struct scsi_inquiry_data);

	return scsi_scsi_cmd(sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), (u_char *) inqbuf,
			     sizeof(struct scsi_inquiry_data), 2, 10000, NULL,
			     SCSI_DATA_IN | flags);
}

/*
 * Prevent or allow the user to remove the media
 */
int 
scsi_prevent(sc_link, type, flags)
	struct scsi_link *sc_link;
	int type, flags;
{
	struct scsi_prevent scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PREVENT_ALLOW;
	scsi_cmd.how = type;
	return scsi_scsi_cmd(sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, 2, 5000, NULL, flags);
}

/*
 * Get scsi driver to send a "start up" command
 */
int 
scsi_start(sc_link, type, flags)
	struct scsi_link *sc_link;
	int type, flags;
{
	struct scsi_start_stop scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = START_STOP;
	scsi_cmd.byte2 = 0x00;
	scsi_cmd.how = type;
	return scsi_scsi_cmd(sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, 2,
			     type == SSS_START ? 30000 : 10000, NULL, flags);
}

/*
 * This routine is called by the scsi interrupt when the transfer is complete.
 */
void 
scsi_done(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	int error;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_done\n"));
#ifdef	SCSIDEBUG
	if ((sc_link->flags & SDEV_DB1) != 0)
		show_scsi_cmd(xs);
#endif /* SCSIDEBUG */

	/*
 	 * If it's a user level request, bypass all usual completion processing,
 	 * let the user work it out.. We take reponsibility for freeing the
 	 * xs when the user returns. (and restarting the device's queue).
 	 */
	if ((xs->flags & SCSI_USER) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("calling user done()\n"));
		scsi_user_done(xs); /* to take a copy of the sense etc. */
		SC_DEBUG(sc_link, SDEV_DB3, ("returned from user done()\n "));

		scsi_free_xs(xs, SCSI_NOSLEEP); /* restarts queue too */
		SC_DEBUG(sc_link, SDEV_DB3, ("returning to adapter\n"));
		return;
	}

	/*
	 * If the device has it's own done routine, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal processing.
	 *
	 * Make sure the upper-level driver knows that this might not
	 * actually be the last time they hear from us.  We need to get
	 * status back.
	 */
	if (sc_link->device->done) {
		SC_DEBUG(sc_link, SDEV_DB2, ("calling private done()\n"));
		error = (*sc_link->device->done)(xs, 0);
		if (error == EJUSTRETURN)
			goto done;
		SC_DEBUG(sc_link, SDEV_DB3, ("continuing with generic done()\n"));
	}
	if (xs->bp == NULL) {
		/*
		 * if it's a normal upper level request, then ask
		 * the upper level code to handle error checking
		 * rather than doing it here at interrupt time
		 */
		wakeup(xs);
		return;
	}
	/*
	 * Go and handle errors now.
	 * If it returns ERESTART then we should RETRY
	 */
retry:
	if (sc_err1(xs, 1) == ERESTART) {
		switch ((*(sc_link->adapter->scsi_cmd)) (xs)) {
		case SUCCESSFULLY_QUEUED:
			return;

		case TRY_AGAIN_LATER:
			xs->error = XS_BUSY;
		case COMPLETE:
			goto retry;
		}
	}
done:
	if (sc_link->device->done) {
		/*
		 * Tell the device the operation is actually complete.
		 * No more will happen with this xfer.  This for
		 * notification of the upper-level driver only; they
		 * won't be returning any meaningful information to us.
		 */
		(void)(*sc_link->device->done)(xs, 1);
	}
	scsi_free_xs(xs, SCSI_NOSLEEP);
}

int
scsi_execute_xs(xs)
	struct scsi_xfer *xs;
{
	int error;
	int s;

	xs->flags &= ~ITSDONE;
	xs->error = XS_NOERROR;
	xs->resid = xs->datalen;

retry:
	/*
	 * Do the transfer. If we are polling we will return:
	 * COMPLETE,  Was poll, and scsi_done has been called
	 * TRY_AGAIN_LATER, Adapter short resources, try again
	 * 
	 * if under full steam (interrupts) it will return:
	 * SUCCESSFULLY_QUEUED, will do a wakeup when complete
	 * TRY_AGAIN_LATER, (as for polling)
	 * After the wakeup, we must still check if it succeeded
	 * 
	 * If we have a bp however, all the error proccessing
	 * and the buffer code both expect us to return straight
	 * to them, so as soon as the command is queued, return
	 */
	switch ((*(xs->sc_link->adapter->scsi_cmd)) (xs)) {
	case SUCCESSFULLY_QUEUED:
		if (xs->bp)
			return EJUSTRETURN;
		s = splbio();
		while ((xs->flags & ITSDONE) == 0)
			tsleep(xs, PRIBIO + 1, "scsi_scsi_cmd", 0);
		splx(s);
	case COMPLETE:		/* Polling command completed ok */
		if (xs->bp)
			return EJUSTRETURN;
	doit:
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("back in cmd()\n"));
		if ((error = sc_err1(xs, 0)) != ERESTART)
			return error;
		goto retry;

	case TRY_AGAIN_LATER:	/* adapter resource shortage */
		xs->error = XS_BUSY;
		goto doit;

	default:
		panic("scsi_execute_xs: invalid return code");
	}

#ifdef DIAGNOSTIC
	panic("scsi_execute_xs: impossible");
#endif
	return EINVAL;
}

/*
 * ask the scsi driver to perform a command for us.
 * tell it where to read/write the data, and how
 * long the data is supposed to be. If we have  a buf
 * to associate with the transfer, we need that too.
 */
int 
scsi_scsi_cmd(sc_link, scsi_cmd, cmdlen, data_addr, datalen,
    retries, timeout, bp, flags)
	struct scsi_link *sc_link;
	struct scsi_generic *scsi_cmd;
	int cmdlen;
	u_char *data_addr;
	int datalen;
	int retries;
	int timeout;
	struct buf *bp;
	int flags;
{
	struct scsi_xfer *xs;
	int error;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_cmd\n"));

#ifdef DIAGNOSTIC
	if (bp != 0 && (flags & SCSI_NOSLEEP) == 0)
		panic("scsi_scsi_cmd: buffer without nosleep");
#endif

	if ((xs = scsi_make_xs(sc_link, scsi_cmd, cmdlen, data_addr, datalen,
	    retries, timeout, bp, flags)) == NULL)
		return ENOMEM;

	if ((error = scsi_execute_xs(xs)) == EJUSTRETURN)
		return 0;

	/*
	 * we have finished with the xfer stuct, free it and
	 * check if anyone else needs to be started up.
	 */
	scsi_free_xs(xs, flags);
	return error;
}

int 
sc_err1(xs, async)
	struct scsi_xfer *xs;
	int async;
{
	int error;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("sc_err1,err = 0x%x \n", xs->error));

	/*
	 * If it has a buf, we might be working with
	 * a request from the buffer cache or some other
	 * piece of code that requires us to process
	 * errors at inetrrupt time. We have probably
	 * been called by scsi_done()
	 */
	switch (xs->error) {
	case XS_NOERROR:	/* nearly always hit this one */
		error = 0;
		break;

	case XS_SENSE:
		if ((error = scsi_interpret_sense(xs)) == ERESTART)
			goto retry;
		SC_DEBUG(xs->sc_link, SDEV_DB3,
		    ("scsi_interpret_sense returned %d\n", error));
		break;

	case XS_BUSY:
		if (xs->retries) {
			if ((xs->flags & SCSI_POLL) != 0)
				delay(1000000);
			else if ((xs->flags & SCSI_NOSLEEP) == 0)
				tsleep(&lbolt, PRIBIO, "scbusy", 0);
			else
#if 0
				timeout(scsi_requeue, xs, hz);
#else
				goto lose;
#endif
		}
	case XS_TIMEOUT:
	retry:
		if (xs->retries--) {
			xs->error = XS_NOERROR;
			xs->flags &= ~ITSDONE;
			return ERESTART;
		}
	case XS_DRIVER_STUFFUP:
	lose:
		error = EIO;
		break;

	case XS_SELTIMEOUT:
		/* XXX Disable device? */
		error = EIO;
		break;

	default:
		sc_print_addr(xs->sc_link);
		printf("unknown error category from scsi driver\n");
		error = EIO;
		break;
	}

	scsi_error(xs, error);
	return error;
}

void
scsi_error(xs, error)
	struct scsi_xfer *xs;
	int error;
{
	struct buf *bp = xs->bp;

	if (bp) {
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
		} else {
			bp->b_error = 0;
			bp->b_resid = xs->resid;
		}
		biodone(bp);
	}
}

/*
 * Look at the returned sense and act on the error, determining
 * the unix error number to pass back.  (0 = report no error)
 *
 * THIS IS THE DEFAULT ERROR HANDLER
 */
int 
scsi_interpret_sense(xs)
	struct scsi_xfer *xs;
{
	struct scsi_sense_data *sense;
	struct scsi_link *sc_link = xs->sc_link;
	u_int8_t key;
	u_int32_t info;
	int error;

	static char *error_mes[] = {
		"soft error (corrected)",
		"not ready", "medium error",
		"non-media hardware failure", "illegal request",
		"unit attention", "readonly device",
		"no data found", "vendor unique",
		"copy aborted", "command aborted",
		"search returned equal", "volume overflow",
		"verify miscompare", "unknown error key"
	};

	sense = &xs->sense;
#ifdef	SCSIDEBUG
	if ((sc_link->flags & SDEV_DB1) != 0) {
		int count;
		printf("code%x valid%x ",
		    sense->error_code & SSD_ERRCODE,
		    sense->error_code & SSD_ERRCODE_VALID ? 1 : 0);
		printf("seg%x key%x ili%x eom%x fmark%x\n",
		    sense->extended_segment,
		    sense->extended_flags & SSD_KEY,
		    sense->extended_flags & SSD_ILI ? 1 : 0,
		    sense->extended_flags & SSD_EOM ? 1 : 0,
		    sense->extended_flags & SSD_FILEMARK ? 1 : 0);
		printf("info: %x %x %x %x followed by %d extra bytes\n",
		    sense->extended_info[0],
		    sense->extended_info[1],
		    sense->extended_info[2],
		    sense->extended_info[3],
		    sense->extended_extra_len);
		printf("extra: ");
		for (count = 0; count < sense->extended_extra_len; count++)
			printf("%x ", sense->extended_extra_bytes[count]);
		printf("\n");
	}
#endif	/*SCSIDEBUG */
	/*
	 * If the device has it's own error handler, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal error processing.
	 */
	if (sc_link->device->err_handler) {
		SC_DEBUG(sc_link, SDEV_DB2, ("calling private err_handler()\n"));
		error = (*sc_link->device->err_handler) (xs);
		if (error != -1)
			return error;		/* error >= 0  better ? */
	}
	/* otherwise use the default */
	switch (sense->error_code & SSD_ERRCODE) {
		/*
		 * If it's code 70, use the extended stuff and interpret the key
		 */
	case 0x71:		/* delayed error */
		sc_print_addr(sc_link);
		key = sense->extended_flags & SSD_KEY;
		printf(" DELAYED ERROR, key = 0x%x\n", key);
	case 0x70:
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
			bcopy(sense->extended_info, &info, sizeof info);
			info = ntohl(info);
		} else
			info = 0;
		key = sense->extended_flags & SSD_KEY;

		switch (key) {
		case 0x0:	/* NO SENSE */
		case 0x1:	/* RECOVERED ERROR */
			if (xs->resid == xs->datalen)
				xs->resid = 0;	/* not short read */
		case 0xc:	/* EQUAL */
			error = 0;
			break;
		case 0x2:	/* NOT READY */
			if ((sc_link->flags & SDEV_REMOVABLE) != 0)
				sc_link->flags &= ~SDEV_MEDIA_LOADED;
			if ((xs->flags & SCSI_IGNORE_NOT_READY) != 0)
				return 0;
			if (xs->retries && !(sc_link->flags & SDEV_REMOVABLE)) {
				delay(1000000);
				return ERESTART;
			}
			if ((xs->flags & SCSI_SILENT) != 0)
				return EIO;
			error = EIO;
			break;
		case 0x5:	/* ILLEGAL REQUEST */
			if ((xs->flags & SCSI_IGNORE_ILLEGAL_REQUEST) != 0)
				return 0;
			error = EINVAL;
			break;
		case 0x6:	/* UNIT ATTENTION */
			if ((sc_link->flags & SDEV_REMOVABLE) != 0)
				sc_link->flags &= ~SDEV_MEDIA_LOADED;
			if ((xs->flags & SCSI_IGNORE_MEDIA_CHANGE) != 0 ||
			    /* XXX Should reupload any transient state. */
			    (sc_link->flags & SDEV_REMOVABLE) == 0)
				return ERESTART;
			if ((xs->flags & SCSI_SILENT) != 0)
				return EIO;
			error = EIO;
			break;
		case 0x7:	/* DATA PROTECT */
			error = EACCES;
			break;
		case 0x8:	/* BLANK CHECK */
			error = 0;
			break;
		case 0xb:	/* COMMAND ABORTED */
			error = ERESTART;
			break;
		case 0xd:	/* VOLUME OVERFLOW */
			error = ENOSPC;
			break;
		default:
			error = EIO;
			break;
		}

		if (key) {
			sc_print_addr(sc_link);
			printf("%s", error_mes[key - 1]);
			if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
				switch (key) {
				case 0x2:	/* NOT READY */
				case 0x5:	/* ILLEGAL REQUEST */
				case 0x6:	/* UNIT ATTENTION */
				case 0x7:	/* DATA PROTECT */
					break;
				case 0x8:	/* BLANK CHECK */
					printf(", requested size: %d (decimal)",
					    info);
					break;
				case 0xb:	/* COMMAND ABORTED */
					if (xs->retries)
						printf(", retrying");
					printf(", cmd 0x%x, info 0x%x",
						xs->cmd->opcode, info);
					break;
				default:
					printf(", info = %d (decimal)", info);
				}
			}
			if (sense->extended_extra_len != 0) {
				int n;
				printf(", data =");
				for (n = 0; n < sense->extended_extra_len; n++)
					printf(" %02x", sense->extended_extra_bytes[n]);
			}
			printf("\n");
		}
		return error;

	/*
	 * Not code 70, just report it
	 */
	default:
		sc_print_addr(sc_link);
		printf("error code %d",
		    sense->error_code & SSD_ERRCODE);
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
			printf(" at block no. %d (decimal)",
			    (sense->XXX_unextended_blockhi << 16) +
			    (sense->XXX_unextended_blockmed << 8) +
			    (sense->XXX_unextended_blocklow));
		}
		printf("\n");
		return EIO;
	}
}

/*
 * Utility routines often used in SCSI stuff
 */

/*
 * convert a physical address to 3 bytes, 
 * MSB at the lowest address,
 * LSB at the highest.
 */
void
lto3b(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	*bytes++ = (val >> 16) & 0xff;
	*bytes++ = (val >> 8) & 0xff;
	*bytes = val & 0xff;
}

/*
 * The reverse of lto3b
 */
u_int32_t
_3btol(bytes)
	u_int8_t *bytes;
{
	u_int32_t rc;

	rc = (*bytes++ << 16);
	rc += (*bytes++ << 8);
	rc += *bytes;
	return (rc);
}

/*
 * Print out the scsi_link structure's address info.
 */
void
sc_print_addr(sc_link)
	struct scsi_link *sc_link;
{

	printf("%s(%s:%d:%d): ",
	    sc_link->device_softc ?
	    ((struct device *)sc_link->device_softc)->dv_xname : "probe",
	    ((struct device *)sc_link->adapter_softc)->dv_xname,
	    sc_link->target, sc_link->lun);		
}

#ifdef	SCSIDEBUG
/*
 * Given a scsi_xfer, dump the request, in all it's glory
 */
void
show_scsi_xs(xs)
	struct scsi_xfer *xs;
{
	printf("xs(0x%x): ", xs);
	printf("flg(0x%x)", xs->flags);
	printf("sc_link(0x%x)", xs->sc_link);
	printf("retr(0x%x)", xs->retries);
	printf("timo(0x%x)", xs->timeout);
	printf("cmd(0x%x)", xs->cmd);
	printf("len(0x%x)", xs->cmdlen);
	printf("data(0x%x)", xs->data);
	printf("len(0x%x)", xs->datalen);
	printf("res(0x%x)", xs->resid);
	printf("err(0x%x)", xs->error);
	printf("bp(0x%x)", xs->bp);
	show_scsi_cmd(xs);
}

void
show_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	u_char *b = (u_char *) xs->cmd;
	int     i = 0;

	sc_print_addr(xs->sc_link);
	printf("command: ");

	if ((xs->flags & SCSI_RESET) == 0) {
		while (i < xs->cmdlen) {
			if (i)
				printf(",");
			printf("%x", b[i++]);
		}
		printf("-[%d bytes]\n", xs->datalen);
		if (xs->datalen)
			show_mem(xs->data, min(64, xs->datalen));
	} else
		printf("-RESET-\n");
}

void
show_mem(address, num)
	u_char *address;
	int num;
{
	int x;

	printf("------------------------------");
	for (x = 0; x < num; x++) {
		if ((x % 16) == 0)
			printf("\n%03d: ", x);
		printf("%02x ", *address++);
	}
	printf("\n------------------------------\n");
}
#endif /*SCSIDEBUG */
