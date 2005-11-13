/*	$OpenBSD: scsi_base.c,v 1.95 2005/11/13 02:39:45 krw Exp $	*/
/*	$NetBSD: scsi_base.c,v 1.43 1997/04/02 02:29:36 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1997 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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
 * Detailed SCSI error printing Copyright 1997 by Matthew Jacob.
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
#include <sys/pool.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

static __inline struct scsi_xfer *scsi_make_xs(struct scsi_link *,
    struct scsi_generic *, int cmdlen, u_char *data_addr,
    int datalen, int retries, int timeout, struct buf *, int flags);
static __inline void asc2ascii(u_int8_t, u_int8_t ascq, char *result,
    size_t len);
int	sc_err1(struct scsi_xfer *);
int	scsi_interpret_sense(struct scsi_xfer *);
char   *scsi_decode_sense(struct scsi_sense_data *, int);

/* Values for flag parameter to scsi_decode_sense. */
#define	DECODE_SENSE_KEY	1
#define	DECODE_ASC_ASCQ		2
#define DECODE_SKSV		3
 
struct pool scsi_xfer_pool;

/*
 * Called when a scsibus is attached to initialize global data.
 */
void
scsi_init()
{
	static int scsi_init_done;

	if (scsi_init_done)
		return;
	scsi_init_done = 1;

	/* Initialize the scsi_xfer pool. */
	pool_init(&scsi_xfer_pool, sizeof(struct scsi_xfer), 0,
	    0, 0, "scxspl", NULL);
}

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
	while (sc_link->openings == 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("sleeping\n"));
		if ((flags & SCSI_NOSLEEP) != 0) {
			splx(s);
			return (NULL);
		}
		sc_link->flags |= SDEV_WAITING;
		if (tsleep(sc_link, PRIBIO, "getxs", 0)) {
			/* Bail out on getting a signal. */
			sc_link->flags &= ~SDEV_WAITING;
			return (NULL);
		}	
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("calling pool_get\n"));
	xs = pool_get(&scsi_xfer_pool,
	    ((flags & SCSI_NOSLEEP) != 0 ? PR_NOWAIT : PR_WAITOK));
	if (xs != NULL) {
		bzero(xs, sizeof *xs);
		sc_link->openings--;
		xs->flags = flags;
	} else {
		sc_print_addr(sc_link);
		printf("cannot allocate scsi xs\n");
	}
	splx(s);

	SC_DEBUG(sc_link, SDEV_DB3, ("returning\n"));

	return (xs);
}

/*
 * Given a scsi_xfer struct, and a device (referenced through sc_link)
 * return the struct to the free pool and credit the device with it
 * If another process is waiting for an xs, do a wakeup, let it proceed
 */
void 
scsi_free_xs(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;

	splassert(IPL_BIO);

	SC_DEBUG(sc_link, SDEV_DB3, ("scsi_free_xs\n"));

	pool_put(&scsi_xfer_pool, xs);

	/* if was 0 and someone waits, wake them up */
	sc_link->openings++;
	if ((sc_link->flags & SDEV_WAITING) != 0) {
		sc_link->flags &= ~SDEV_WAITING;
		wakeup(sc_link);
	} else {
		if (sc_link->device->start) {
			SC_DEBUG(sc_link, SDEV_DB2,
			    ("calling private start()\n"));
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

	/*
	 * Set the LUN in the CDB.  This may only be needed if we have an
	 * older device.  However, we also set it for more modern SCSI
	 * devices "just in case".  The old code assumed everything newer
	 * than SCSI-2 would not need it, but why risk it?  This was the
	 * old conditional:
	 *
	 * if ((sc_link->scsi_version & SID_ANSII) <= 2)
	 */
	xs->cmd->bytes[0] &= ~SCSI_CMD_LUN_MASK;
	xs->cmd->bytes[0] |=
	    ((sc_link->lun << SCSI_CMD_LUN_SHIFT) & SCSI_CMD_LUN_MASK);

	return xs;
}

/*
 * Find out from the device what its capacity is.
 */
u_long
scsi_size(sc_link, flags, blksize)
	struct scsi_link *sc_link;
	int flags;
	u_int32_t *blksize;
{
	struct scsi_read_capacity scsi_cmd;
	struct scsi_read_cap_data rdcap;
	u_long max_addr;
	int error;

	if (blksize)
		*blksize = 0;

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
	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)&rdcap, sizeof(rdcap), 2, 20000, NULL,
	    flags | SCSI_DATA_IN);
	if (error) {
		SC_DEBUG(sc_link, SDEV_DB1, ("READ CAPACITY error (%#x)\n",
		    error));
		return (0);
	}	

	max_addr = _4btol(rdcap.addr);
	if (blksize)
		*blksize = _4btol(rdcap.length);

	if (max_addr == 0xffffffffUL) {
		/*
		 * The device is reporting it has more than 2^32-1 sectors. The
		 * 16-byte READ CAPACITY command must be issued to get full
		 * capacity.
		 */
		sc_print_addr(sc_link);
		printf("only the first 4,294,967,295 sectors will be used.\n");
		return (0xffffffffUL);
	}
	
	return (max_addr + 1);
}

/*
 * Get scsi driver to send a "are you ready?" command
 */
int 
scsi_test_unit_ready(sc_link, retries, flags)
	struct scsi_link *sc_link;
	int retries;
	int flags;
{
	struct scsi_test_unit_ready scsi_cmd;

	if (sc_link->quirks & ADEV_NOTUR)
		return 0;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = TEST_UNIT_READY;

	return scsi_scsi_cmd(sc_link, (struct scsi_generic *) &scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, retries, 10000, NULL, flags);
}

/*
 * Do a scsi operation asking a device what it is.
 * Use the scsi_cmd routine in the switch table.
 */
int 
scsi_inquire(sc_link, inqbuf, flags)
	struct scsi_link *sc_link;
	struct scsi_inquiry_data *inqbuf;
	int flags;
{
	struct scsi_inquiry scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof scsi_cmd);
	scsi_cmd.opcode = INQUIRY;

	bzero(inqbuf, sizeof *inqbuf);

	memset(&inqbuf->vendor, ' ', sizeof inqbuf->vendor);
	memset(&inqbuf->product, ' ', sizeof inqbuf->product);
	memset(&inqbuf->revision, ' ', sizeof inqbuf->revision);
	memset(&inqbuf->extra, ' ', sizeof inqbuf->extra);

	/*
	 * Ask for only the basic 36 bytes of SCSI2 inquiry information. This
	 * avoids problems with devices that choke trying to supply more.
	 */
	scsi_cmd.length = SID_INQUIRY_HDR + SID_SCSI2_ALEN;
	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)inqbuf, scsi_cmd.length, 2, 10000, NULL,
	    SCSI_DATA_IN | flags);

	return (error);
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

	if (sc_link->quirks & ADEV_NODOORLOCK)
		return 0;

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

int
scsi_mode_sense(sc_link, byte2, page, data, len, flags, timeout)
	struct scsi_link *sc_link;
	int byte2, page, flags, timeout;
	size_t len;
	struct scsi_mode_header *data;
{
	struct scsi_mode_sense scsi_cmd;
	int error;

	/*
	 * Make sure the sense buffer is clean before we do the mode sense, so
	 * that checks for bogus values of 0 will work in case the mode sense
	 * fails.
	 */
	bzero(data, len);

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SENSE;
	scsi_cmd.byte2 = byte2;
	scsi_cmd.page = page;

	if (len > 0xff) 
		len = 0xff;
	scsi_cmd.length = len;

	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, len, 4, timeout, NULL,
	    flags | SCSI_DATA_IN);

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_mode_sense: page %#x, error = %d\n",
	    page, error));

	return (error);
}

int
scsi_mode_sense_big(sc_link, byte2, page, data, len, flags, timeout)
	struct scsi_link *sc_link;
	int byte2, page, flags, timeout;
	size_t len;
	struct scsi_mode_header_big *data;
{
	struct scsi_mode_sense_big scsi_cmd;
	int error;

	/*
	 * Make sure the sense buffer is clean before we do the mode sense, so
	 * that checks for bogus values of 0 will work in case the mode sense
	 * fails.
	 */
	bzero(data, len);

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SENSE_BIG;
	scsi_cmd.byte2 = byte2;
	scsi_cmd.page = page;

	if (len > 0xffff)
		len = 0xffff;
	_lto2b(len, scsi_cmd.length);

	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, len, 4, timeout, NULL,
	    flags | SCSI_DATA_IN);

	SC_DEBUG(sc_link, SDEV_DB2,
	    ("scsi_mode_sense_big: page %#x, error = %d\n", page, error));

	return (error);
}

void *
scsi_mode_sense_page(hdr, page_len)
	struct scsi_mode_header *hdr;
	const int page_len;
{
	int total_length, header_length;

	total_length = hdr->data_length + sizeof(hdr->data_length);
	header_length = sizeof(*hdr) + hdr->blk_desc_len;

	if ((total_length - header_length) < page_len)
		return (NULL);
		
	return ((u_char *)hdr + header_length);	
}

void *
scsi_mode_sense_big_page(hdr, page_len)
	struct scsi_mode_header_big *hdr;
	const int page_len;
{
	int total_length, header_length;

	total_length = _2btol(hdr->data_length) + sizeof(hdr->data_length);
	header_length = sizeof(*hdr) + _2btol(hdr->blk_desc_len);

	if ((total_length - header_length) < page_len)
		return (NULL);

	return ((u_char *)hdr + header_length);
}

int
scsi_do_mode_sense(sc_link, page, buf, page_data, density, block_count,
    block_size, page_len, flags, big)
	struct scsi_link *sc_link;
	union scsi_mode_sense_buf *buf;
	int page, page_len, flags, *big;
	u_int32_t *density, *block_size;
	u_int64_t *block_count;
	void **page_data;
{
	struct scsi_direct_blk_desc *direct;
	struct scsi_blk_desc *general;
	int error, blk_desc_len, offset;
	
	*page_data = NULL;

	if (density)
		*density = 0;
	if (block_count)
		*block_count = 0;
	if (block_size)
		*block_size = 0;
	if (big)
		*big = 0;

	if ((sc_link->flags & SDEV_ATAPI) == 0) {
		/*
		 * Try 6 byte mode sense request first. Some devices don't
		 * distinguish between 6 and 10 byte MODE SENSE commands,
		 * returning 6 byte data for 10 byte requests. Don't bother
		 * with SMS_DBD. Check returned data length to ensure that
		 * at least a header (3 additional bytes) is returned.
		 */
		error = scsi_mode_sense(sc_link, 0, page, &buf->hdr,
		    sizeof(*buf), flags, 20000);
		if (error == 0 && buf->hdr.data_length > 2) {
			*page_data = scsi_mode_sense_page(&buf->hdr, page_len);
			offset = sizeof(struct scsi_mode_header);
			blk_desc_len = buf->hdr.blk_desc_len;
			goto blk_desc;
		}	
	}	

	/*
	 * Try 10 byte mode sense request. Don't bother with SMS_DBD or
	 * SMS_LLBAA. Bail out if the returned information is less than
	 * a big header in size (6 additional bytes).
	 */
	error = scsi_mode_sense_big(sc_link, 0, page, &buf->hdr_big,
	    sizeof(*buf), flags, 20000);
	if (error != 0)
		return (error);
	if (_2btol(buf->hdr_big.data_length) < 6)
		return (EIO);

	if (big)
		*big = 1;
	offset = sizeof(struct scsi_mode_header_big);
	*page_data = scsi_mode_sense_big_page(&buf->hdr_big, page_len);
	blk_desc_len = _2btol(buf->hdr_big.blk_desc_len);

blk_desc:
	/* Both scsi_blk_desc and scsi_direct_blk_desc are 8 bytes. */
	if (blk_desc_len == 0 || (blk_desc_len % 8 != 0))
		return (0);

	switch (sc_link->inqdata.device & SID_TYPE) {
	case T_SEQUENTIAL:
		/*
		 * XXX What other device types return general block descriptors?
		 */
		general = (struct scsi_blk_desc *)&buf->buf[offset];	
		if (density)
			*density = general->density;
		if (block_size)
			*block_size = _3btol(general->blklen);
		if (block_count)
			*block_count = (u_int64_t)_3btol(general->nblocks);
		break;

	default:
		direct = (struct scsi_direct_blk_desc *)&buf->buf[offset];
		if (density)
			*density = direct->density;
		if (block_size)
			*block_size = _3btol(direct->blklen);
		if (block_count)
			*block_count = (u_int64_t)_4btol(direct->nblocks);
		break;
	}

	return (0);
}

int
scsi_mode_select(sc_link, byte2, data, flags, timeout)
	struct scsi_link *sc_link;
	int byte2, flags, timeout;
	struct scsi_mode_header *data;
{
	struct scsi_mode_select scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SELECT;
	scsi_cmd.byte2 = byte2;
	scsi_cmd.length = data->data_length + 1; /* 1 == sizeof(data_length) */

	/* Length is reserved when doing mode select so zero it. */
	data->data_length = 0;

	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, scsi_cmd.length, 4, timeout, NULL,
	    flags | SCSI_DATA_OUT);

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_mode_select: error = %d\n", error));

	return (error);
}

int
scsi_mode_select_big(sc_link, byte2, data, flags, timeout)
	struct scsi_link *sc_link;
	int byte2, flags, timeout;
	struct scsi_mode_header_big *data;
{
	struct scsi_mode_select_big scsi_cmd;
	u_int32_t len;
	int error;

	len = _2btol(data->data_length) + 2; /* 2 == sizeof data->data_length */

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SELECT_BIG;
	scsi_cmd.byte2 = byte2;
	_lto2b(len, scsi_cmd.length);

	/* Length is reserved when doing mode select so zero it. */
	_lto2b(0, data->data_length);

	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, len, 4, timeout, NULL,
	    flags | SCSI_DATA_OUT);

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_mode_select_big: error = %d\n",
	    error));

	return (error);
}

/*
 * This routine is called by the scsi interrupt when the transfer is complete.
 */
void 
scsi_done(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct buf *bp;
	int error;

	splassert(IPL_BIO);

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
		SC_DEBUG(sc_link, SDEV_DB3, ("returned from user done()\n"));

		scsi_free_xs(xs); /* restarts queue too */
		SC_DEBUG(sc_link, SDEV_DB3, ("returning to adapter\n"));
		return;
	}

	if (!((xs->flags & (SCSI_NOSLEEP | SCSI_POLL)) == SCSI_NOSLEEP)) {
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
	error = sc_err1(xs);
	if (error == ERESTART) {
		switch ((*(sc_link->adapter->scsi_cmd)) (xs)) {
		case SUCCESSFULLY_QUEUED:
			return;

		case TRY_AGAIN_LATER:
			xs->error = XS_BUSY;
			/* FALLTHROUGH */
		case COMPLETE:
			goto retry;
		}
	}

	bp = xs->bp;
	if (bp) {
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
		} else {
			bp->b_error = 0;
			bp->b_resid = xs->resid;
		}
	}
	if (sc_link->device->done) {
		/*
		 * Tell the device the operation is actually complete.
		 * No more will happen with this xfer.  This for
		 * notification of the upper-level driver only; they
		 * won't be returning any meaningful information to us.
		 */
		(*sc_link->device->done)(xs);
	}
	scsi_free_xs(xs);
	if (bp)
		biodone(bp);
}

int
scsi_execute_xs(xs)
	struct scsi_xfer *xs;
{
	int error, flags, rslt, s;

	xs->flags &= ~ITSDONE;
	xs->error = XS_NOERROR;
	xs->resid = xs->datalen;
	xs->status = 0;

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
	 * If we have a SCSI_NOSLEEP (typically because we have a buf)
	 * we just return.  All the error processing and the buffer
	 * code both expect us to return straight to them, so as soon
	 * as the command is queued, return.
	 */

	/* 
	 * We save the flags here because the xs structure may already
	 * be freed by scsi_done by the time adapter->scsi_cmd returns.
	 *
	 * scsi_done is responsible for freeing the xs if either
	 * (flags & (SCSI_NOSLEEP | SCSI_POLL)) == SCSI_NOSLEEP
	 * -or-
	 * (flags & SCSI_USER) != 0
	 *
	 * Note: SCSI_USER must always be called with SCSI_NOSLEEP
	 * and never with SCSI_POLL, so the second expression should be
	 * is equivalent to the first.
	 */

	flags = xs->flags;
#ifdef DIAGNOSTIC
	if ((flags & (SCSI_USER | SCSI_NOSLEEP)) == SCSI_USER)
		panic("scsi_execute_xs: USER without NOSLEEP");
	if ((flags & (SCSI_USER | SCSI_POLL)) == (SCSI_USER | SCSI_POLL))
		panic("scsi_execute_xs: USER with POLL");
#endif
retry:
	rslt = (*(xs->sc_link->adapter->scsi_cmd))(xs);
	switch (rslt) {
	case SUCCESSFULLY_QUEUED:
		if ((flags & (SCSI_NOSLEEP | SCSI_POLL)) == SCSI_NOSLEEP)
			return EJUSTRETURN;
#ifdef DIAGNOSTIC
		if (flags & SCSI_NOSLEEP)
			panic("scsi_execute_xs: NOSLEEP and POLL");
#endif
		s = splbio();
		/* Since the xs is active we can't bail out on a signal. */
		while ((xs->flags & ITSDONE) == 0)
			tsleep(xs, PRIBIO + 1, "scsicmd", 0);
		splx(s);
		/* FALLTHROUGH */
	case COMPLETE:		/* Polling command completed ok */
		if ((flags & (SCSI_NOSLEEP | SCSI_POLL)) == SCSI_NOSLEEP)
			return EJUSTRETURN;
		if (xs->bp)
			return EJUSTRETURN;
	doit:
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("back in cmd()\n"));
		if ((error = sc_err1(xs)) != ERESTART)
			return error;
		goto retry;

	case TRY_AGAIN_LATER:	/* adapter resource shortage */
		xs->error = XS_BUSY;
		goto doit;

	default:
		panic("scsi_execute_xs: invalid return code (%#x)", rslt);
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
	int s;

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

	s = splbio();
	/*
	 * we have finished with the xfer stuct, free it and
	 * check if anyone else needs to be started up.
	 */
	scsi_free_xs(xs);
	splx(s);
	return error;
}

int 
sc_err1(xs)
	struct scsi_xfer *xs;
{
	int error;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("sc_err1,err = 0x%x\n", xs->error));

	/*
	 * If it has a buf, we might be working with
	 * a request from the buffer cache or some other
	 * piece of code that requires us to process
	 * errors at interrupt time. We have probably
	 * been called by scsi_done()
	 */
	switch (xs->error) {
	case XS_NOERROR:	/* nearly always hit this one */
		error = 0;
		break;

	case XS_SENSE:
	case XS_SHORTSENSE:
		if ((error = scsi_interpret_sense(xs)) == ERESTART)
			goto retry;
		SC_DEBUG(xs->sc_link, SDEV_DB3,
		    ("scsi_interpret_sense returned %d\n", error));
		break;

	case XS_BUSY:
		if (xs->retries) {
			if ((error = scsi_delay(xs, 1)) == EIO) ;
				goto lose;
		}
		/* FALLTHROUGH */
	case XS_TIMEOUT:
	retry:
		if (xs->retries--) {
			xs->error = XS_NOERROR;
			xs->flags &= ~ITSDONE;
			return ERESTART;
		}
		/* FALLTHROUGH */
	case XS_DRIVER_STUFFUP:
	lose:
		error = EIO;
		break;

	case XS_SELTIMEOUT:
		/* XXX Disable device? */
		error = EIO;
		break;

	case XS_RESET:
		if (xs->retries) {
			SC_DEBUG(xs->sc_link, SDEV_DB3,
			    ("restarting command destroyed by reset\n"));
			goto retry;
		}
		error = EIO;
		break;

	default:
		sc_print_addr(xs->sc_link);
		printf("unknown error category (0x%x) from scsi driver\n",
		    xs->error);
		error = EIO;
		break;
	}

	return error;
}

int
scsi_delay(xs, seconds)
	struct scsi_xfer *xs;
	int seconds;
{
	switch (xs->flags & (SCSI_POLL | SCSI_NOSLEEP)) {
	case SCSI_POLL:
		delay(1000000 * seconds);
		return (ERESTART);
	case SCSI_NOSLEEP:	
		/* Retry the command immediately since we can't delay. */
		return (ERESTART);
	case (SCSI_POLL | SCSI_NOSLEEP):
		/* Invalid combination! */
		return (EIO);
	}
	
	while (seconds-- > 0)	
		if (tsleep(&lbolt, PRIBIO, "scbusy", 0))
			/* Signal == abort xs. */
			return (EIO);

	return (ERESTART);		
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
	struct scsi_sense_data *sense = &xs->sense;
	struct scsi_link *sc_link = xs->sc_link;
	u_int8_t serr, skey;
	int error;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("code:%#x valid:%d key:%#x ili:%d eom:%d fmark:%d extra:%d\n",
	    sense->error_code & SSD_ERRCODE,
	    sense->error_code & SSD_ERRCODE_VALID ? 1 : 0,
	    sense->flags & SSD_KEY,
	    sense->flags & SSD_ILI ? 1 : 0,
	    sense->flags & SSD_EOM ? 1 : 0,
	    sense->flags & SSD_FILEMARK ? 1 : 0,
	    sense->extra_len));
#ifdef	SCSIDEBUG
	if ((sc_link->flags & SDEV_DB1) != 0)
		show_mem((u_char *)&xs->sense, sizeof xs->sense);
#endif	/* SCSIDEBUG */

	/*
	 * If the device has it's own error handler, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal error processing.
	 */
	if (sc_link->device->err_handler) {
		SC_DEBUG(sc_link, SDEV_DB2, ("calling private err_handler()\n"));
		error = (*sc_link->device->err_handler) (xs);
		if (error != EJUSTRETURN)
			return error;		/* error >= 0  better ? */
	}

	/* Default sense interpretation. */
	serr = sense->error_code & SSD_ERRCODE;
	if (serr != 0x70 && serr != 0x71)
		skey = 0xff;	/* Invalid value, since key is 4 bit value. */
	else
		skey = sense->flags & SSD_KEY;

	/*
	 * Interpret the key/asc/ascq information where appropriate.
	 */
	error = 0;
	switch (skey) {
	case SKEY_NO_SENSE:
	case SKEY_RECOVERED_ERROR:
		if (xs->resid == xs->datalen)
			xs->resid = 0;	/* not short read */
		break;
	case SKEY_BLANK_CHECK:
	case SKEY_EQUAL:
		break;
	case SKEY_NOT_READY:
		if ((xs->flags & SCSI_IGNORE_NOT_READY) != 0)
			return (0);
		error = EIO;
		if (xs->retries) {
			switch (sense->add_sense_code) {
			case 0x04:	/* LUN not ready */
				switch (sense->add_sense_code_qual) {
				case 0x01: /* Becoming Ready */
				case 0x04: /* Format In Progress */
				case 0x05: /* Rebuild In Progress */
				case 0x06: /* Recalculation In Progress */
				case 0x07: /* Operation In Progress */
				case 0x08: /* Long Write In Progress */
				case 0x09: /* Self-Test In Progress */
					SC_DEBUG(sc_link, SDEV_DB1,
		    			    ("not ready: busy (%#x)\n",
					    sense->add_sense_code_qual));
					return (scsi_delay(xs, 1));
				}	
				break;
			case 0x3a:	/* Medium not present */
				sc_link->flags &= ~SDEV_MEDIA_LOADED;
				error = ENODEV;
				break;
			}
		}
		break;
	case SKEY_ILLEGAL_REQUEST:
		if ((xs->flags & SCSI_IGNORE_ILLEGAL_REQUEST) != 0)
			return 0;
		error = EINVAL;
		break;
	case SKEY_UNIT_ATTENTION:
		if (sense->add_sense_code == 0x29 /* device or bus reset */)
			return (scsi_delay(xs, 1));
		if ((sc_link->flags & SDEV_REMOVABLE) != 0)
			sc_link->flags &= ~SDEV_MEDIA_LOADED;
		if ((xs->flags & SCSI_IGNORE_MEDIA_CHANGE) != 0 ||
		    /* XXX Should reupload any transient state. */
		    (sc_link->flags & SDEV_REMOVABLE) == 0) {
			return (scsi_delay(xs, 1));
		}
		error = EIO;
		break;
	case SKEY_WRITE_PROTECT:
		error = EROFS;
		break;
	case SKEY_ABORTED_COMMAND:
		error = ERESTART;
		break;
	case SKEY_VOLUME_OVERFLOW:
		error = ENOSPC;
		break;
	default:
		error = EIO;
		break;
	}

	if (skey && (xs->flags & SCSI_SILENT) == 0)
		scsi_print_sense(xs);

	return error;
}

/*
 * Utility routines often used in SCSI stuff
 */


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

static const char *sense_keys[16] = {
	"No Additional Sense",
	"Soft Error",
	"Not Ready",
	"Media Error",
	"Hardware Error",
	"Illegal Request",
	"Unit Attention",
	"Write Protected",
	"Blank Check",
	"Vendor Unique",
	"Copy Aborted",
	"Aborted Command",
	"Equal Error",
	"Volume Overflow",
	"Miscompare Error",
	"Reserved"
};

#ifdef SCSITERSE
static __inline void
asc2ascii(asc, ascq, result, len)
	u_int8_t asc, ascq;
	char *result;
	size_t len;
{
	snprintf(result, len, "ASC 0x%02x ASCQ 0x%02x", asc, ascq);
}
#else
static const struct {
	u_int8_t asc, ascq;
	char *description;
} adesc[] = {
	{ 0x00, 0x00, "No Additional Sense Information" },
	{ 0x00, 0x01, "Filemark Detected" },
	{ 0x00, 0x02, "End-Of-Partition/Medium Detected" },
	{ 0x00, 0x03, "Setmark Detected" },
	{ 0x00, 0x04, "Beginning-Of-Partition/Medium Detected" },
	{ 0x00, 0x05, "End-Of-Data Detected" },
	{ 0x00, 0x06, "I/O Process Terminated" },
	{ 0x00, 0x11, "Audio Play Operation In Progress" },
	{ 0x00, 0x12, "Audio Play Operation Paused" },
	{ 0x00, 0x13, "Audio Play Operation Successfully Completed" },
	{ 0x00, 0x14, "Audio Play Operation Stopped Due to Error" },
	{ 0x00, 0x15, "No Current Audio Status To Return" },
	{ 0x00, 0x16, "Operation In Progress" },
	{ 0x00, 0x17, "Cleaning Requested" },
	{ 0x00, 0x18, "Erase Operation In Progress" },
	{ 0x00, 0x19, "Locate Operation In Progress" },
	{ 0x00, 0x1A, "Rewind Operation In Progress" },
	{ 0x00, 0x1B, "Set Capacity Operation In Progress" },
	{ 0x00, 0x1C, "Verify Operation In Progress" },
	{ 0x01, 0x00, "No Index/Sector Signal" },
	{ 0x02, 0x00, "No Seek Complete" },
	{ 0x03, 0x00, "Peripheral Device Write Fault" },
	{ 0x03, 0x01, "No Write Current" },
	{ 0x03, 0x02, "Excessive Write Errors" },
	{ 0x04, 0x00, "Logical Unit Not Ready, Cause Not Reportable" },
	{ 0x04, 0x01, "Logical Unit Is in Process Of Becoming Ready" },
	{ 0x04, 0x02, "Logical Unit Not Ready, Initialization Command Required" },
	{ 0x04, 0x03, "Logical Unit Not Ready, Manual Intervention Required" },
	{ 0x04, 0x04, "Logical Unit Not Ready, Format In Progress" },
	{ 0x04, 0x05, "Logical Unit Not Ready, Rebuild In Progress" },
	{ 0x04, 0x06, "Logical Unit Not Ready, Recalculation In Progress" },
	{ 0x04, 0x07, "Logical Unit Not Ready, Operation In Progress" },
	{ 0x04, 0x08, "Logical Unit Not Ready, Long Write In Progress" },
	{ 0x04, 0x09, "Logical Unit Not Ready, Self-Test In Progress" },
	{ 0x04, 0x0A, "Logical Unit Not Accessible, Asymmetric Access State Transition" },
	{ 0x04, 0x0B, "Logical Unit Not Accessible, Target Port In Standby State" },
	{ 0x04, 0x0C, "Logical Unit Not Accessible, Target Port In Unavailable State" },
	{ 0x04, 0x10, "Logical Unit Not Ready, Auxiliary Memory Not Accessible" },
	{ 0x04, 0x11, "Logical Unit Not Ready, Notify (Enable Spinup) Required" },
	{ 0x05, 0x00, "Logical Unit Does Not Respond To Selection" },
	{ 0x06, 0x00, "No Reference Position Found" },
	{ 0x07, 0x00, "Multiple Peripheral Devices Selected" },
	{ 0x08, 0x00, "Logical Unit Communication Failure" },
	{ 0x08, 0x01, "Logical Unit Communication Timeout" },
	{ 0x08, 0x02, "Logical Unit Communication Parity Error" },
	{ 0x08, 0x03, "Logical Unit Communication CRC Error (ULTRA-DMA/32)" },
	{ 0x08, 0x04, "Unreachable Copy Target" },
	{ 0x09, 0x00, "Track Following Error" },
	{ 0x09, 0x01, "Tracking Servo Failure" },
	{ 0x09, 0x02, "Focus Servo Failure" },
	{ 0x09, 0x03, "Spindle Servo Failure" },
	{ 0x09, 0x04, "Head Select Fault" },
	{ 0x0A, 0x00, "Error Log Overflow" },
	{ 0x0B, 0x00, "Warning" },
	{ 0x0B, 0x01, "Warning - Specified Temperature Exceeded" },
	{ 0x0B, 0x02, "Warning - Enclosure Degraded" },
	{ 0x0C, 0x00, "Write Error" },
	{ 0x0C, 0x01, "Write Error Recovered with Auto Reallocation" },
	{ 0x0C, 0x02, "Write Error - Auto Reallocate Failed" },
	{ 0x0C, 0x03, "Write Error - Recommend Reassignment" },
	{ 0x0C, 0x04, "Compression Check Miscompare Error" },
	{ 0x0C, 0x05, "Data Expansion Occurred During Compression" },
	{ 0x0C, 0x06, "Block Not Compressible" },
	{ 0x0C, 0x07, "Write Error - Recovery Needed" },
	{ 0x0C, 0x08, "Write Error - Recovery Failed" },
	{ 0x0C, 0x09, "Write Error - Loss Of Streaming" },
	{ 0x0C, 0x0A, "Write Error - Padding Blocks Added" },
	{ 0x0C, 0x0B, "Auxiliary Memory Write Error" },
	{ 0x0C, 0x0C, "Write Error - Unexpected Unsolicited Data" },
	{ 0x0C, 0x0D, "Write Error - Not Enough Unsolicited Data" },
	{ 0x0D, 0x00, "Error Detected By Third Party Temporary Initiator" },
	{ 0x0D, 0x01, "Third Party Device Failure" },
	{ 0x0D, 0x02, "Copy Target Device Not Reachable" },
	{ 0x0D, 0x03, "Incorrect Copy Target Device Type" },
	{ 0x0D, 0x04, "Copy Target Device Data Underrun" },
	{ 0x0D, 0x05, "Copy Target Device Data Overrun" },
	{ 0x0E, 0x00, "Invalid Information Unit" },
	{ 0x0E, 0x01, "Information Unit Too Short" },
	{ 0x0E, 0x02, "Information Unit Too Long" },
	{ 0x10, 0x00, "ID CRC Or ECC Error" },
	{ 0x11, 0x00, "Unrecovered Read Error" },
	{ 0x11, 0x01, "Read Retries Exhausted" },
	{ 0x11, 0x02, "Error Too Long To Correct" },
	{ 0x11, 0x03, "Multiple Read Errors" },
	{ 0x11, 0x04, "Unrecovered Read Error - Auto Reallocate Failed" },
	{ 0x11, 0x05, "L-EC Uncorrectable Error" },
	{ 0x11, 0x06, "CIRC Unrecovered Error" },
	{ 0x11, 0x07, "Data Resynchronization Error" },
	{ 0x11, 0x08, "Incomplete Block Read" },
	{ 0x11, 0x09, "No Gap Found" },
	{ 0x11, 0x0A, "Miscorrected Error" },
	{ 0x11, 0x0B, "Uncorrected Read Error - Recommend Reassignment" },
	{ 0x11, 0x0C, "Uncorrected Read Error - Recommend Rewrite The Data" },
	{ 0x11, 0x0D, "De-Compression CRC Error" },
	{ 0x11, 0x0E, "Cannot Decompress Using Declared Algorithm" },
	{ 0x11, 0x0F, "Error Reading UPC/EAN Number" },
	{ 0x11, 0x10, "Error Reading ISRC Number" },
	{ 0x11, 0x11, "Read Error - Loss Of Streaming" },
	{ 0x11, 0x12, "Auxiliary Memory Read Error" },
	{ 0x11, 0x13, "Read Error - Failed Retransmission Request" },
	{ 0x12, 0x00, "Address Mark Not Found for ID Field" },
	{ 0x13, 0x00, "Address Mark Not Found for Data Field" },
	{ 0x14, 0x00, "Recorded Entity Not Found" },
	{ 0x14, 0x01, "Record Not Found" },
	{ 0x14, 0x02, "Filemark or Setmark Not Found" },
	{ 0x14, 0x03, "End-Of-Data Not Found" },
	{ 0x14, 0x04, "Block Sequence Error" },
	{ 0x14, 0x05, "Record Not Found - Recommend Reassignment" },
	{ 0x14, 0x06, "Record Not Found - Data Auto-Reallocated" },
	{ 0x14, 0x07, "Locate Operation Failure" },
	{ 0x15, 0x00, "Random Positioning Error" },
	{ 0x15, 0x01, "Mechanical Positioning Error" },
	{ 0x15, 0x02, "Positioning Error Detected By Read of Medium" },
	{ 0x16, 0x00, "Data Synchronization Mark Error" },
	{ 0x16, 0x01, "Data Sync Error - Data Rewritten" },
	{ 0x16, 0x02, "Data Sync Error - Recommend Rewrite" },
	{ 0x16, 0x03, "Data Sync Error - Data Auto-Reallocated" },
	{ 0x16, 0x04, "Data Sync Error - Recommend Reassignment" },
	{ 0x17, 0x00, "Recovered Data With No Error Correction Applied" },
	{ 0x17, 0x01, "Recovered Data With Retries" },
	{ 0x17, 0x02, "Recovered Data With Positive Head Offset" },
	{ 0x17, 0x03, "Recovered Data With Negative Head Offset" },
	{ 0x17, 0x04, "Recovered Data With Retries and/or CIRC Applied" },
	{ 0x17, 0x05, "Recovered Data Using Previous Sector ID" },
	{ 0x17, 0x06, "Recovered Data Without ECC - Data Auto-Reallocated" },
	{ 0x17, 0x07, "Recovered Data Without ECC - Recommend Reassignment" },
	{ 0x17, 0x08, "Recovered Data Without ECC - Recommend Rewrite" },
	{ 0x17, 0x09, "Recovered Data Without ECC - Data Rewritten" },
	{ 0x18, 0x00, "Recovered Data With Error Correction Applied" },
	{ 0x18, 0x01, "Recovered Data With Error Correction & Retries Applied" },
	{ 0x18, 0x02, "Recovered Data - Data Auto-Reallocated" },
	{ 0x18, 0x03, "Recovered Data With CIRC" },
	{ 0x18, 0x04, "Recovered Data With L-EC" },
	{ 0x18, 0x05, "Recovered Data - Recommend Reassignment" },
	{ 0x18, 0x06, "Recovered Data - Recommend Rewrite" },
	{ 0x18, 0x07, "Recovered Data With ECC - Data Rewritten" },
	{ 0x18, 0x08, "Recovered Data With Linking" },
	{ 0x19, 0x00, "Defect List Error" },
	{ 0x19, 0x01, "Defect List Not Available" },
	{ 0x19, 0x02, "Defect List Error in Primary List" },
	{ 0x19, 0x03, "Defect List Error in Grown List" },
	{ 0x1A, 0x00, "Parameter List Length Error" },
	{ 0x1B, 0x00, "Synchronous Data Transfer Error" },
	{ 0x1C, 0x00, "Defect List Not Found" },
	{ 0x1C, 0x01, "Primary Defect List Not Found" },
	{ 0x1C, 0x02, "Grown Defect List Not Found" },
	{ 0x1D, 0x00, "Miscompare During Verify Operation" },
	{ 0x1E, 0x00, "Recovered ID with ECC" },
	{ 0x1F, 0x00, "Partial Defect List Transfer" },
	{ 0x20, 0x00, "Invalid Command Operation Code" },
	{ 0x20, 0x01, "Access Denied - Initiator Pending-Enrolled" },
	{ 0x20, 0x02, "Access Denied - No Access rights" },
	{ 0x20, 0x03, "Access Denied - Invalid Mgmt ID Key" },
	{ 0x20, 0x04, "Illegal Command While In Write Capable State" },
	{ 0x20, 0x05, "Obsolete" },
	{ 0x20, 0x06, "Illegal Command While In Explicit Address Mode" },
	{ 0x20, 0x07, "Illegal Command While In Implicit Address Mode" },
	{ 0x20, 0x08, "Access Denied - Enrollment Conflict" },
	{ 0x20, 0x09, "Access Denied - Invalid LU Identifier" },
	{ 0x20, 0x0A, "Access Denied - Invalid Proxy Token" },
	{ 0x20, 0x0B, "Access Denied - ACL LUN Conflict" },
	{ 0x21, 0x00, "Logical Block Address Out of Range" },
	{ 0x21, 0x01, "Invalid Element Address" },
	{ 0x21, 0x02, "Invalid Address For Write" },
	{ 0x22, 0x00, "Illegal Function (Should 20 00, 24 00, or 26 00)" },
	{ 0x24, 0x00, "Illegal Field in CDB" },
	{ 0x24, 0x01, "CDB Decryption Error" },
	{ 0x24, 0x02, "Obsolete" },
	{ 0x24, 0x03, "Obsolete" },
	{ 0x24, 0x04, "Security Audit Value Frozen" },
	{ 0x24, 0x05, "Security Working Key Frozen" },
	{ 0x24, 0x06, "Nonce Not Unique" },
	{ 0x24, 0x07, "Nonce Timestamp Out Of Range" },
	{ 0x25, 0x00, "Logical Unit Not Supported" },
	{ 0x26, 0x00, "Invalid Field In Parameter List" },
	{ 0x26, 0x01, "Parameter Not Supported" },
	{ 0x26, 0x02, "Parameter Value Invalid" },
	{ 0x26, 0x03, "Threshold Parameters Not Supported" },
	{ 0x26, 0x04, "Invalid Release Of Persistent Reservation" },
	{ 0x26, 0x05, "Data Decryption Error" },
	{ 0x26, 0x06, "Too Many Target Descriptors" },
	{ 0x26, 0x07, "Unsupported Target Descriptor Type Code" },
	{ 0x26, 0x08, "Too Many Segment Descriptors" },
	{ 0x26, 0x09, "Unsupported Segment Descriptor Type Code" },
	{ 0x26, 0x0A, "Unexpected Inexact Segment" },
	{ 0x26, 0x0B, "Inline Data Length Exceeded" },
	{ 0x26, 0x0C, "Invalid Operation For Copy Source Or Destination" },
	{ 0x26, 0x0D, "Copy Segment Granularity Violation" },
	{ 0x26, 0x0E, "Invalid Parameter While Port Is Enabled" },
	{ 0x27, 0x00, "Write Protected" },
	{ 0x27, 0x01, "Hardware Write Protected" },
	{ 0x27, 0x02, "Logical Unit Software Write Protected" },
	{ 0x27, 0x03, "Associated Write Protect" },
	{ 0x27, 0x04, "Persistent Write Protect" },
	{ 0x27, 0x05, "Permanent Write Protect" },
	{ 0x27, 0x06, "Conditional Write Protect" },
	{ 0x28, 0x00, "Not Ready To Ready Transition (Medium May Have Changed)" },
	{ 0x28, 0x01, "Import Or Export Element Accessed" },
	{ 0x29, 0x00, "Power On, Reset, or Bus Device Reset Occurred" },
	{ 0x29, 0x01, "Power On Occurred" },
	{ 0x29, 0x02, "SCSI Bus Reset Occurred" },
	{ 0x29, 0x03, "Bus Device Reset Function Occurred" },
	{ 0x29, 0x04, "Device Internal Reset" },
	{ 0x29, 0x05, "Transceiver Mode Changed to Single Ended" },
	{ 0x29, 0x06, "Transceiver Mode Changed to LVD" },
	{ 0x29, 0x07, "I_T Nexus Loss Occurred" },
	{ 0x2A, 0x00, "Parameters Changed" },
	{ 0x2A, 0x01, "Mode Parameters Changed" },
	{ 0x2A, 0x02, "Log Parameters Changed" },
	{ 0x2A, 0x03, "Reservations Preempted" },
	{ 0x2A, 0x04, "Reservations Released" },
	{ 0x2A, 0x05, "Registrations Preempted" },
	{ 0x2A, 0x06, "Asymmetric Access State Changed" },
	{ 0x2A, 0x07, "Implicit Asymmetric Access State Transition Failed" },
	{ 0x2B, 0x00, "Copy Cannot Execute Since Host Cannot Disconnect" },
	{ 0x2C, 0x00, "Command Sequence Error" },
	{ 0x2C, 0x01, "Too Many Windows Specified" },
	{ 0x2C, 0x02, "Invalid Combination of Windows Specified" },
	{ 0x2C, 0x03, "Current Program Area Is Not Empty" },
	{ 0x2C, 0x04, "Current Program Area Is Empty" },
	{ 0x2C, 0x05, "Illegal Power Condition Request" },
	{ 0x2C, 0x06, "Persistent Prevent Conflict" },
	{ 0x2C, 0x07, "Previous Busy Status" },
	{ 0x2C, 0x08, "Previous Task Set Full Status" },
	{ 0x2C, 0x09, "Previous Reservation Conflict Status" },
	{ 0x2C, 0x0A, "Partition Or Collection Contains User Objects" },
	{ 0x2D, 0x00, "Overwrite Error On Update In Place" },
	{ 0x2E, 0x00, "Insufficient Time For Operation" },
	{ 0x2F, 0x00, "Commands Cleared By Another Initiator" },
	{ 0x30, 0x00, "Incompatible Medium Installed" },
	{ 0x30, 0x01, "Cannot Read Medium - Unknown Format" },
	{ 0x30, 0x02, "Cannot Read Medium - Incompatible Format" },
	{ 0x30, 0x03, "Cleaning Cartridge Installed" },
	{ 0x30, 0x04, "Cannot Write Medium - Unknown Format" },
	{ 0x30, 0x05, "Cannot Write Medium - Incompatible Format" },
	{ 0x30, 0x06, "Cannot Format Medium - Incompatible Medium" },
	{ 0x30, 0x07, "Cleaning Failure" },
	{ 0x30, 0x08, "Cannot Write - Application Code Mismatch" },
	{ 0x30, 0x09, "Current Session Not Fixated For Append" },
	{ 0x30, 0x0A, "Cleaning Request Rejected" },
	{ 0x30, 0x10, "Medium Not Formatted" },
	{ 0x31, 0x00, "Medium Format Corrupted" },
	{ 0x31, 0x01, "Format Command Failed" },
	{ 0x32, 0x00, "No Defect Spare Location Available" },
	{ 0x32, 0x01, "Defect List Update Failure" },
	{ 0x33, 0x00, "Tape Length Error" },
	{ 0x34, 0x00, "Enclosure Failure" },
	{ 0x35, 0x00, "Enclosure Services Failure" },
	{ 0x35, 0x01, "Unsupported Enclosure Function" },
	{ 0x35, 0x02, "Enclosure Services Unavailable" },
	{ 0x35, 0x03, "Enclosure Services Transfer Failure" },
	{ 0x35, 0x04, "Enclosure Services Transfer Refused" },
	{ 0x36, 0x00, "Ribbon, Ink, or Toner Failure" },
	{ 0x37, 0x00, "Rounded Parameter" },
	{ 0x38, 0x00, "Event Status Notification" },
	{ 0x38, 0x02, "ESN - Power Management Class Event" },
	{ 0x38, 0x04, "ESN - Media Class Event" },
	{ 0x38, 0x06, "ESN - Device Busy Class Event" },
	{ 0x39, 0x00, "Saving Parameters Not Supported" },
	{ 0x3A, 0x00, "Medium Not Present" },
	{ 0x3A, 0x01, "Medium Not Present - Tray Closed" },
	{ 0x3A, 0x02, "Medium Not Present - Tray Open" },
	{ 0x3A, 0x03, "Medium Not Present - Loadable" },
	{ 0x3A, 0x04, "Medium Not Present - Medium Auxiliary Memory Accessible" },
	{ 0x3B, 0x00, "Sequential Positioning Error" },
	{ 0x3B, 0x01, "Tape Position Error At Beginning-of-Medium" },
	{ 0x3B, 0x02, "Tape Position Error At End-of-Medium" },
	{ 0x3B, 0x03, "Tape or Electronic Vertical Forms Unit Not Ready" },
	{ 0x3B, 0x04, "Slew Failure" },
	{ 0x3B, 0x05, "Paper Jam" },
	{ 0x3B, 0x06, "Failed To Sense Top-Of-Form" },
	{ 0x3B, 0x07, "Failed To Sense Bottom-Of-Form" },
	{ 0x3B, 0x08, "Reposition Error" },
	{ 0x3B, 0x09, "Read Past End Of Medium" },
	{ 0x3B, 0x0A, "Read Past Beginning Of Medium" },
	{ 0x3B, 0x0B, "Position Past End Of Medium" },
	{ 0x3B, 0x0C, "Position Past Beginning Of Medium" },
	{ 0x3B, 0x0D, "Medium Destination Element Full" },
	{ 0x3B, 0x0E, "Medium Source Element Empty" },
	{ 0x3B, 0x0F, "End Of Medium Reached" },
	{ 0x3B, 0x11, "Medium Magazine Not Accessible" },
	{ 0x3B, 0x12, "Medium Magazine Removed" },
	{ 0x3B, 0x13, "Medium Magazine Inserted" },
	{ 0x3B, 0x14, "Medium Magazine Locked" },
	{ 0x3B, 0x15, "Medium Magazine Unlocked" },
	{ 0x3B, 0x16, "Mechanical Positioning Or Changer Error" },
	{ 0x3D, 0x00, "Invalid Bits In IDENTIFY Message" },
	{ 0x3E, 0x00, "Logical Unit Has Not Self-Configured Yet" },
	{ 0x3E, 0x01, "Logical Unit Failure" },
	{ 0x3E, 0x02, "Timeout On Logical Unit" },
	{ 0x3E, 0x03, "Logical Unit Failed Self-Test" },
	{ 0x3E, 0x04, "Logical Unit Unable To Update Self-Test Log" },
	{ 0x3F, 0x00, "Target Operating Conditions Have Changed" },
	{ 0x3F, 0x01, "Microcode Has Changed" },
	{ 0x3F, 0x02, "Changed Operating Definition" },
	{ 0x3F, 0x03, "INQUIRY Data Has Changed" },
	{ 0x3F, 0x04, "component Device Attached" },
	{ 0x3F, 0x05, "Device Identifier Changed" },
	{ 0x3F, 0x06, "Redundancy Group Created Or Modified" },
	{ 0x3F, 0x07, "Redundancy Group Deleted" },
	{ 0x3F, 0x08, "Spare Created Or Modified" },
	{ 0x3F, 0x09, "Spare Deleted" },
	{ 0x3F, 0x0A, "Volume Set Created Or Modified" },
	{ 0x3F, 0x0B, "Volume Set Deleted" },
	{ 0x3F, 0x0C, "Volume Set Deassigned" },
	{ 0x3F, 0x0D, "Volume Set Reassigned" },
	{ 0x3F, 0x0E, "Reported LUNs Data Has Changed" },
	{ 0x3F, 0x0F, "Echo Buffer Overwritten" },
	{ 0x3F, 0x10, "Medium Loadable" },
	{ 0x3F, 0x11, "Medium Auxiliary Memory Accessible" },
	{ 0x40, 0x00, "RAM FAILURE (Should Use 40 NN)" },
	/* 
	 * ASC 0x40 also has an ASCQ range from 0x80 to 0xFF.
	 * 0x40 0xNN DIAGNOSTIC FAILURE ON COMPONENT NN
	 */
	{ 0x41, 0x00, "Data Path FAILURE (Should Use 40 NN)" },
	{ 0x42, 0x00, "Power-On or Self-Test FAILURE (Should Use 40 NN)" },
	{ 0x43, 0x00, "Message Error" },
	{ 0x44, 0x00, "Internal Target Failure" },
	{ 0x45, 0x00, "Select Or Reselect Failure" },
	{ 0x46, 0x00, "Unsuccessful Soft Reset" },
	{ 0x47, 0x00, "SCSI Parity Error" },
	{ 0x47, 0x01, "Data Phase CRC Error Detected" },
	{ 0x47, 0x02, "SCSI Parity Error Detected During ST Data Phase" },
	{ 0x47, 0x03, "Information Unit iuCRC Error Detected" },
	{ 0x47, 0x04, "Asynchronous Information Protection Error Detected" },
	{ 0x47, 0x05, "Protocol Service CRC Error" },
	{ 0x47, 0x7F, "Some Commands Cleared By iSCSI Protocol Event" },
	{ 0x48, 0x00, "Initiator Detected Error Message Received" },
	{ 0x49, 0x00, "Invalid Message Error" },
	{ 0x4A, 0x00, "Command Phase Error" },
	{ 0x4B, 0x00, "Data Phase Error" },
	{ 0x4B, 0x01, "Invalid Target Port Transfer Tag Received" },
	{ 0x4B, 0x02, "Too Much Write Data" },
	{ 0x4B, 0x03, "ACK/NAK Timeout" },
	{ 0x4B, 0x04, "NAK Received" },
	{ 0x4B, 0x05, "Data Offset Error" },
	{ 0x4B, 0x06, "Initiator Response Timeout" },
	{ 0x4C, 0x00, "Logical Unit Failed Self-Configuration" },
	/* 
	 * ASC 0x4D has an ASCQ range from 0x00 to 0xFF.
	 * 0x4D 0xNN TAGGED OVERLAPPED COMMANDS (NN = TASK TAG) 
	 */
	{ 0x4E, 0x00, "Overlapped Commands Attempted" },
	{ 0x50, 0x00, "Write Append Error" },
	{ 0x50, 0x01, "Write Append Position Error" },
	{ 0x50, 0x02, "Position Error Related To Timing" },
	{ 0x51, 0x00, "Erase Failure" },
	{ 0x51, 0x01, "Erase Failure - Incomplete Erase Operation Detected" },
	{ 0x52, 0x00, "Cartridge Fault" },
	{ 0x53, 0x00, "Media Load or Eject Failed" },
	{ 0x53, 0x01, "Unload Tape Failure" },
	{ 0x53, 0x02, "Medium Removal Prevented" },
	{ 0x54, 0x00, "SCSI To Host System Interface Failure" },
	{ 0x55, 0x00, "System Resource Failure" },
	{ 0x55, 0x01, "System Buffer Full" },
	{ 0x55, 0x02, "Insufficient Reservation Resources" },
	{ 0x55, 0x03, "Insufficient Resources" },
	{ 0x55, 0x04, "Insufficient Registration Resources" },
	{ 0x55, 0x05, "Insufficient Access Control Resources" },
	{ 0x55, 0x06, "Auxiliary Memory Out Of Space" },
	{ 0x57, 0x00, "Unable To Recover Table-Of-Contents" },
	{ 0x58, 0x00, "Generation Does Not Exist" },
	{ 0x59, 0x00, "Updated Block Read" },
	{ 0x5A, 0x00, "Operator Request or State Change Input" },
	{ 0x5A, 0x01, "Operator Medium Removal Requested" },
	{ 0x5A, 0x02, "Operator Selected Write Protect" },
	{ 0x5A, 0x03, "Operator Selected Write Permit" },
	{ 0x5B, 0x00, "Log Exception" },
	{ 0x5B, 0x01, "Threshold Condition Met" },
	{ 0x5B, 0x02, "Log Counter At Maximum" },
	{ 0x5B, 0x03, "Log List Codes Exhausted" },
	{ 0x5C, 0x00, "RPL Status Change" },
	{ 0x5C, 0x01, "Spindles Synchronized" },
	{ 0x5C, 0x02, "Spindles Not Synchronized" },
	{ 0x5D, 0x00, "Failure Prediction Threshold Exceeded" },
	{ 0x5D, 0x01, "Media Failure Prediction Threshold Exceeded" },
	{ 0x5D, 0x02, "Logical Unit Failure Prediction Threshold Exceeded" },
	{ 0x5D, 0x03, "Spare Area Exhaustion Prediction Threshold Exceeded" },
	{ 0x5D, 0x10, "Hardware Impending Failure General Hard Drive Failure" },
	{ 0x5D, 0x11, "Hardware Impending Failure Drive Error Rate Too High" },
	{ 0x5D, 0x12, "Hardware Impending Failure Data Error Rate Too High" },
	{ 0x5D, 0x13, "Hardware Impending Failure Seek Error Rate Too High" },
	{ 0x5D, 0x14, "Hardware Impending Failure Too Many Block Reassigns" },
	{ 0x5D, 0x15, "Hardware Impending Failure Access Times Too High" },
	{ 0x5D, 0x16, "Hardware Impending Failure Start Unit Times Too High" },
	{ 0x5D, 0x17, "Hardware Impending Failure Channel Parametrics" },
	{ 0x5D, 0x18, "Hardware Impending Failure Controller Detected" },
	{ 0x5D, 0x19, "Hardware Impending Failure Throughput Performance" },
	{ 0x5D, 0x1A, "Hardware Impending Failure Seek Time Performance" },
	{ 0x5D, 0x1B, "Hardware Impending Failure Spin-Up Retry Count" },
	{ 0x5D, 0x1C, "Hardware Impending Failure Drive Calibration Retry Count" },
	{ 0x5D, 0x20, "Controller Impending Failure General Hard Drive Failure" },
	{ 0x5D, 0x21, "Controller Impending Failure Drive Error Rate Too High" },
	{ 0x5D, 0x22, "Controller Impending Failure Data Error Rate Too High" },
	{ 0x5D, 0x23, "Controller Impending Failure Seek Error Rate Too High" },
	{ 0x5D, 0x24, "Controller Impending Failure Too Many Block Reassigns" },
	{ 0x5D, 0x25, "Controller Impending Failure Access Times Too High" },
	{ 0x5D, 0x26, "Controller Impending Failure Start Unit Times Too High" },
	{ 0x5D, 0x27, "Controller Impending Failure Channel Parametrics" },
	{ 0x5D, 0x28, "Controller Impending Failure Controller Detected" },
	{ 0x5D, 0x29, "Controller Impending Failure Throughput Performance" },
	{ 0x5D, 0x2A, "Controller Impending Failure Seek Time Performance" },
	{ 0x5D, 0x2B, "Controller Impending Failure Spin-Up Retry Count" },
	{ 0x5D, 0x2C, "Controller Impending Failure Drive Calibration Retry Count" },
	{ 0x5D, 0x30, "Data Channel Impending Failure General Hard Drive Failure" },
	{ 0x5D, 0x31, "Data Channel Impending Failure Drive Error Rate Too High" },
	{ 0x5D, 0x32, "Data Channel Impending Failure Data Error Rate Too High" },
	{ 0x5D, 0x33, "Data Channel Impending Failure Seek Error Rate Too High" },
	{ 0x5D, 0x34, "Data Channel Impending Failure Too Many Block Reassigns" },
	{ 0x5D, 0x35, "Data Channel Impending Failure Access Times Too High" },
	{ 0x5D, 0x36, "Data Channel Impending Failure Start Unit Times Too High" },
	{ 0x5D, 0x37, "Data Channel Impending Failure Channel Parametrics" },
	{ 0x5D, 0x38, "Data Channel Impending Failure Controller Detected" },
	{ 0x5D, 0x39, "Data Channel Impending Failure Throughput Performance" },
	{ 0x5D, 0x3A, "Data Channel Impending Failure Seek Time Performance" },
	{ 0x5D, 0x3B, "Data Channel Impending Failure Spin-Up Retry Count" },
	{ 0x5D, 0x3C, "Data Channel Impending Failure Drive Calibration Retry Count" },
	{ 0x5D, 0x40, "Servo Impending Failure General Hard Drive Failure" },
	{ 0x5D, 0x41, "Servo Impending Failure Drive Error Rate Too High" },
	{ 0x5D, 0x42, "Servo Impending Failure Data Error Rate Too High" },
	{ 0x5D, 0x43, "Servo Impending Failure Seek Error Rate Too High" },
	{ 0x5D, 0x44, "Servo Impending Failure Too Many Block Reassigns" },
	{ 0x5D, 0x45, "Servo Impending Failure Access Times Too High" },
	{ 0x5D, 0x46, "Servo Impending Failure Start Unit Times Too High" },
	{ 0x5D, 0x47, "Servo Impending Failure Channel Parametrics" },
	{ 0x5D, 0x48, "Servo Impending Failure Controller Detected" },
	{ 0x5D, 0x49, "Servo Impending Failure Throughput Performance" },
	{ 0x5D, 0x4A, "Servo Impending Failure Seek Time Performance" },
	{ 0x5D, 0x4B, "Servo Impending Failure Spin-Up Retry Count" },
	{ 0x5D, 0x4C, "Servo Impending Failure Drive Calibration Retry Count" },
	{ 0x5D, 0x50, "Spindle Impending Failure General Hard Drive Failure" },
	{ 0x5D, 0x51, "Spindle Impending Failure Drive Error Rate Too High" },
	{ 0x5D, 0x52, "Spindle Impending Failure Data Error Rate Too High" },
	{ 0x5D, 0x53, "Spindle Impending Failure Seek Error Rate Too High" },
	{ 0x5D, 0x54, "Spindle Impending Failure Too Many Block Reassigns" },
	{ 0x5D, 0x55, "Spindle Impending Failure Access Times Too High" },
	{ 0x5D, 0x56, "Spindle Impending Failure Start Unit Times Too High" },
	{ 0x5D, 0x57, "Spindle Impending Failure Channel Parametrics" },
	{ 0x5D, 0x58, "Spindle Impending Failure Controller Detected" },
	{ 0x5D, 0x59, "Spindle Impending Failure Throughput Performance" },
	{ 0x5D, 0x5A, "Spindle Impending Failure Seek Time Performance" },
	{ 0x5D, 0x5B, "Spindle Impending Failure Spin-Up Retry Count" },
	{ 0x5D, 0x5C, "Spindle Impending Failure Drive Calibration Retry Count" },
	{ 0x5D, 0x60, "Firmware Impending Failure General Hard Drive Failure" },
	{ 0x5D, 0x61, "Firmware Impending Failure Drive Error Rate Too High" },
	{ 0x5D, 0x62, "Firmware Impending Failure Data Error Rate Too High" },
	{ 0x5D, 0x63, "Firmware Impending Failure Seek Error Rate Too High" },
	{ 0x5D, 0x64, "Firmware Impending Failure Too Many Block Reassigns" },
	{ 0x5D, 0x65, "Firmware Impending Failure Access Times Too High" },
	{ 0x5D, 0x66, "Firmware Impending Failure Start Unit Times Too High" },
	{ 0x5D, 0x67, "Firmware Impending Failure Channel Parametrics" },
	{ 0x5D, 0x68, "Firmware Impending Failure Controller Detected" },
	{ 0x5D, 0x69, "Firmware Impending Failure Throughput Performance" },
	{ 0x5D, 0x6A, "Firmware Impending Failure Seek Time Performance" },
	{ 0x5D, 0x6B, "Firmware Impending Failure Spin-Up Retry Count" },
	{ 0x5D, 0x6C, "Firmware Impending Failure Drive Calibration Retry Count" },
	{ 0x5D, 0xFF, "Failure Prediction Threshold Exceeded (false)" },
	{ 0x5E, 0x00, "Low Power Condition On" },
	{ 0x5E, 0x01, "Idle Condition Activated By Timer" },
	{ 0x5E, 0x02, "Standby Condition Activated By Timer" },
	{ 0x5E, 0x03, "Idle Condition Activated By Command" },
	{ 0x5E, 0x04, "Standby Condition Activated By Command" },
	{ 0x5E, 0x41, "Power State Change To Active" },
	{ 0x5E, 0x42, "Power State Change To Idle" },
	{ 0x5E, 0x43, "Power State Change To Standby" },
	{ 0x5E, 0x45, "Power State Change To Sleep" },
	{ 0x5E, 0x47, "Power State Change To Device Control" },
	{ 0x60, 0x00, "Lamp Failure" },
	{ 0x61, 0x00, "Video Acquisition Error" },
	{ 0x61, 0x01, "Unable To Acquire Video" },
	{ 0x61, 0x02, "Out Of Focus" },
	{ 0x62, 0x00, "Scan Head Positioning Error" },
	{ 0x63, 0x00, "End Of User Area Encountered On This Track" },
	{ 0x63, 0x01, "Packet Does Not Fit In Available Space" },
	{ 0x64, 0x00, "Illegal Mode For This Track" },
	{ 0x64, 0x01, "Invalid Packet Size" },
	{ 0x65, 0x00, "Voltage Fault" },
	{ 0x66, 0x00, "Automatic Document Feeder Cover Up" },
	{ 0x66, 0x01, "Automatic Document Feeder Lift Up" },
	{ 0x66, 0x02, "Document Jam In Automatic Document Feeder" },
	{ 0x66, 0x03, "Document Miss Feed Automatic In Document Feeder" },
	{ 0x67, 0x00, "Configuration Failure" },
	{ 0x67, 0x01, "Configuration Of Incapable Logical Units Failed" },
	{ 0x67, 0x02, "Add Logical Unit Failed" },
	{ 0x67, 0x03, "Modification Of Logical Unit Failed" },
	{ 0x67, 0x04, "Exchange Of Logical Unit Failed" },
	{ 0x67, 0x05, "Remove Of Logical Unit Failed" },
	{ 0x67, 0x06, "Attachment Of Logical Unit Failed" },
	{ 0x67, 0x07, "Creation Of Logical Unit Failed" },
	{ 0x67, 0x08, "Assign Failure Occurred" },
	{ 0x67, 0x09, "Multiply Assigned Logical Unit" },
	{ 0x67, 0x0A, "Set Target Port Groups Command Failed" },
	{ 0x68, 0x00, "Logical Unit Not Configured" },
	{ 0x69, 0x00, "Data Loss On Logical Unit" },
	{ 0x69, 0x01, "Multiple Logical Unit Failures" },
	{ 0x69, 0x02, "Parity/Data Mismatch" },
	{ 0x6A, 0x00, "Informational, Refer To Log" },
	{ 0x6B, 0x00, "State Change Has Occurred" },
	{ 0x6B, 0x01, "Redundancy Level Got Better" },
	{ 0x6B, 0x02, "Redundancy Level Got Worse" },
	{ 0x6C, 0x00, "Rebuild Failure Occurred" },
	{ 0x6D, 0x00, "Recalculate Failure Occurred" },
	{ 0x6E, 0x00, "Command To Logical Unit Failed" },
	{ 0x6F, 0x00, "Copy Protection Key Exchange Failure - Authentication Failure" },
	{ 0x6F, 0x01, "Copy Protection Key Exchange Failure - Key Not Present" },
	{ 0x6F, 0x02, "Copy Protection Key Exchange Failure - Key Not Established" },
	{ 0x6F, 0x03, "Read Of Scrambled Sector Without Authentication" },
	{ 0x6F, 0x04, "Media Region Code Is Mismatched To Logical Unit Region" },
	{ 0x6F, 0x05, "Drive Region Must Be Permanent/Region Reset Count Error" },
	/* 
	 * ASC 0x70 has an ASCQ range from 0x00 to 0xFF.
	 * 0x70 0xNN DECOMPRESSION EXCEPTION SHORT ALGORITHM ID Of NN
	 */
	{ 0x71, 0x00, "Decompression Exception Long Algorithm ID" },
	{ 0x72, 0x00, "Session Fixation Error" },
	{ 0x72, 0x01, "Session Fixation Error Writing Lead-In" },
	{ 0x72, 0x02, "Session Fixation Error Writing Lead-Out" },
	{ 0x72, 0x03, "Session Fixation Error - Incomplete Track In Session" },
	{ 0x72, 0x04, "Empty Or Partially Written Reserved Track" },
	{ 0x72, 0x05, "No More Track Reservations Allowed" },
	{ 0x73, 0x00, "CD Control Error" },
	{ 0x73, 0x01, "Power Calibration Area Almost Full" },
	{ 0x73, 0x02, "Power Calibration Area Is Full" },
	{ 0x73, 0x03, "Power Calibration Area Error" },
	{ 0x73, 0x04, "Program Memory Area Update Failure" },
	{ 0x73, 0x05, "Program Memory Area Is Full" },
	{ 0x73, 0x06, "RMA/PMA Is Almost Full" },
	{ 0x00, 0x00, NULL }
};

static __inline void
asc2ascii(asc, ascq, result, len)
	u_int8_t asc, ascq;
	char *result;
	size_t len;
{
	int i;
	
	/* Check for a dynamically built description. */
	switch (asc) {
	case 0x40:
		if (ascq >= 0x80) {
			snprintf(result, len,
		            "Diagnostic Failure on Component 0x%02x", ascq);
			return;
		}
		break;
	case 0x4d:
		snprintf(result, len,
	 	    "Tagged Overlapped Commands (0x%02x = TASK TAG)", ascq); 
		return;
	case 0x70:
		snprintf(result, len,
		    "Decompression Exception Short Algorithm ID OF 0x%02x",
		    ascq);
		return;
	default:
		break;
	}

	/* Check for a fixed description. */
	for (i = 0; adesc[i].description != NULL; i++)
		if (adesc[i].asc == asc && adesc[i].ascq == ascq) {
			strlcpy(result, adesc[i].description, len);
			return;
		}

	/* Just print out the ASC and ASCQ values as a description. */
	snprintf(result, len, "ASC 0x%02x ASCQ 0x%02x", asc, ascq);
}
#endif /* SCSITERSE */

void
scsi_print_sense(xs)
	struct scsi_xfer *xs;
{
	struct scsi_sense_data *sense = &xs->sense;
	u_int8_t serr = sense->error_code & SSD_ERRCODE;
	int32_t info;
	char *sbs;

	sc_print_addr(xs->sc_link);

	/* XXX For error 0x71, current opcode is not the relevant one. */
	printf("%sCheck Condition (error %#x) on opcode 0x%x\n",
	    (serr == 0x71) ? "DEFERRED " : "", serr, xs->cmd->opcode);

	if (serr != 0x70 && serr != 0x71) {	
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
			struct scsi_sense_data_unextended *usense =
			    (struct scsi_sense_data_unextended *)sense;
			printf("   AT BLOCK #: %d (decimal)",
			    _3btol(usense->block));
		}
		return;
	}

	printf("    SENSE KEY: %s\n", scsi_decode_sense(sense,
	    DECODE_SENSE_KEY));

	if (sense->flags & (SSD_FILEMARK | SSD_EOM | SSD_ILI)) {
		char pad = ' ';

		printf("             ");
		if (sense->flags & SSD_FILEMARK) {
			printf("%c Filemark Detected", pad);
			pad = ',';
		}
		if (sense->flags & SSD_EOM) {
			printf("%c EOM Detected", pad);
			pad = ',';
		}
		if (sense->flags & SSD_ILI)
			printf("%c Incorrect Length Indicator Set", pad);
		printf("\n");
	}

	/*
	 * It is inconvenient to use device type to figure out how to
	 * format the info fields. So print them as 32 bit integers.
	 */
	info = _4btol(&sense->info[0]);
	if (info)
		printf("         INFO: 0x%x (VALID flag %s)\n", info,
		    sense->error_code & SSD_ERRCODE_VALID ? "on" : "off");

	if (sense->extra_len < 4)
		return;

	info = _4btol(&sense->cmd_spec_info[0]);
	if (info)
		printf(" COMMAND INFO: 0x%x\n", info);
	sbs = scsi_decode_sense(sense, DECODE_ASC_ASCQ);
	if (strlen(sbs) > 0)
		printf("     ASC/ASCQ: %s\n", sbs);
	if (sense->fru != 0)
		printf("     FRU CODE: 0x%x\n", sense->fru);
	sbs = scsi_decode_sense(sense, DECODE_SKSV);
	if (strlen(sbs) > 0)
		printf("         SKSV: %s\n", sbs);
}

char *
scsi_decode_sense(sense, flag)
	struct scsi_sense_data *sense;
	int flag;
{
	static char rqsbuf[132];
	u_int16_t count;
	u_int8_t skey, spec_1;
	int len;

	bzero(rqsbuf, sizeof rqsbuf);

	skey = sense->flags & SSD_KEY;
	spec_1 = sense->sense_key_spec_1;
	count = _2btol(&sense->sense_key_spec_2);

	switch (flag) {
	case DECODE_SENSE_KEY:
		strlcpy(rqsbuf, sense_keys[skey], sizeof rqsbuf);
		break;
	case DECODE_ASC_ASCQ:
		asc2ascii(sense->add_sense_code, sense->add_sense_code_qual,
		    rqsbuf, sizeof rqsbuf);
		break;
	case DECODE_SKSV:
		if (sense->extra_len < 9 || ((spec_1 & SSD_SCS_VALID) == 0))
			break;
		switch (skey) {
		case SKEY_ILLEGAL_REQUEST:
			len = snprintf(rqsbuf, sizeof rqsbuf,
			    "Error in %s, Offset %d",
			    (spec_1 & SSD_SCS_CDB_ERROR) ? "CDB" : "Parameters",
			    count);
			if ((len != -1 && len < sizeof rqsbuf) &&
			    (spec_1 & SSD_SCS_VALID_BIT_INDEX))
				snprintf(rqsbuf+len, sizeof rqsbuf - len,
				    ", bit %d", spec_1 & SSD_SCS_BIT_INDEX);
			break;
		case SKEY_RECOVERED_ERROR:
		case SKEY_MEDIUM_ERROR:
		case SKEY_HARDWARE_ERROR:
			snprintf(rqsbuf, sizeof rqsbuf,
			    "Actual Retry Count: %d", count);
			break;
		case SKEY_NOT_READY:
			snprintf(rqsbuf, sizeof rqsbuf,
			    "Progress Indicator: %d", count);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return (rqsbuf);
}

#ifdef	SCSIDEBUG
/*
 * Given a scsi_xfer, dump the request, in all it's glory
 */
void
show_scsi_xs(xs)
	struct scsi_xfer *xs;
{
	printf("xs(%p): ", xs);
	printf("flg(0x%x)", xs->flags);
	printf("sc_link(%p)", xs->sc_link);
	printf("retr(0x%x)", xs->retries);
	printf("timo(0x%x)", xs->timeout);
	printf("cmd(%p)", xs->cmd);
	printf("len(0x%x)", xs->cmdlen);
	printf("data(%p)", xs->data);
	printf("len(0x%x)", xs->datalen);
	printf("res(0x%x)", xs->resid);
	printf("err(0x%x)", xs->error);
	printf("bp(%p)", xs->bp);
	show_scsi_cmd(xs);
}

void
show_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	u_char *b = (u_char *) xs->cmd;
	int	i = 0;

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
#endif /* SCSIDEBUG */
