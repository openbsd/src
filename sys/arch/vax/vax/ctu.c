/*	$OpenBSD: ctu.c,v 1.10 2007/06/06 17:15:13 deraadt Exp $ */
/*	$NetBSD: ctu.c,v 1.10 2000/03/23 06:46:44 thorpej Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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

/*
 * Device driver for 11/750 Console TU58.
 *
 * Error checking is almost nonexistent, the driver should be
 * fixed to at least calculate checksum on incoming packets.
 * Writing of tapes does not work, by some unknown reason so far.
 * It is almost useless to try to use this driver when running
 * multiuser, because the serial device don't have any buffers 
 * so we will lose interrupts.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/disklabel.h>	/* For disklabel prototype */

#include <machine/mtpr.h>
#include <machine/rsp.h>
#include <machine/scb.h>
#include <machine/trap.h>

enum tu_state {
	SC_UNUSED,
	SC_INIT,
	SC_READY,
	SC_SEND_CMD,
	SC_GET_RESP,
	SC_GET_WCONT,
	SC_GET_END,
	SC_RESTART,
};

struct tu_softc {
	enum	tu_state sc_state;
	int	sc_error;
	char	sc_rsp[15];	/* Should be struct rsb; but don't work */
	u_char	*sc_xfptr;	/* Current char to xfer */
	u_char	*sc_blk;	/* Base of current 128b block */
	int	sc_tpblk;	/* Start block number */
	int	sc_nbytes;	/* Number of bytes to xfer */
	int	sc_xbytes;	/* Number of xfer'd bytes */
	int	sc_bbytes;	/* Number of xfer'd bytes this block */
	int	sc_op;		/* Read/write */
	int	sc_xmtok;	/* set if OK to xmit */
	struct	buf_queue sc_q;	/* pending I/O requests */
} tu_sc;

struct	ivec_dsp tu_recv, tu_xmit;

void	ctutintr(void *);
void	cturintr(void *);
void	ctuattach(void);
void	ctustart(struct buf *);
void	ctuwatch(void *);
short	ctu_cksum(unsigned short *, int);

int	ctuopen(dev_t, int, int, struct proc *);
int	ctuclose(dev_t, int, int, struct proc *);
void	ctustrategy(struct buf *);
int	ctuioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	ctudump(dev_t, daddr64_t, caddr_t, size_t);

static struct callout ctu_watch_ch = CALLOUT_INITIALIZER;

void
ctuattach()
{
	BUFQ_INIT(&tu_sc.sc_q);

	tu_recv = idsptch;
	tu_recv.hoppaddr = cturintr;
	scb->scb_csrint = (void *)&tu_recv;

	tu_xmit = idsptch;
	tu_xmit.hoppaddr = ctutintr;
	scb->scb_cstint = (void *)&tu_xmit;
}

int
ctuopen(dev, oflags, devtype, p)
	dev_t dev;
	int oflags, devtype;
	struct proc *p;
{
	int unit, error;

	unit = minor(dev);

	if (unit)
		return ENXIO;

	if (tu_sc.sc_state != SC_UNUSED)
		return EBUSY;

	tu_sc.sc_error = 0;
	mtpr(0100, PR_CSRS);	/* Enable receive interrupt */
	callout_reset(&ctu_watch_ch, hz, ctuwatch, NULL);

	tu_sc.sc_state = SC_INIT;

	mtpr(RSP_TYP_INIT, PR_CSTD);

        if ((error = tsleep((caddr_t)&tu_sc, (PZERO + 10)|PCATCH,
	    "ctuopen", 0)))
                return (error);

	if (tu_sc.sc_error)
		return tu_sc.sc_error;

	tu_sc.sc_state = SC_READY;
	tu_sc.sc_xmtok = 1;

	mtpr(0100, PR_CSTS);
	return 0;

}

int
ctuclose(dev, oflags, devtype, p)
	dev_t dev;
	int oflags, devtype;
	struct proc *p;
{
	mtpr(0, PR_CSRS);
	mtpr(0, PR_CSTS);
	tu_sc.sc_state = SC_UNUSED;
	callout_stop(&ctu_watch_ch);
	return 0;
}

void
ctustrategy(bp)
	struct buf *bp;
{
	int	s;

#ifdef TUDEBUG
	printf("addr %x, block %x, nblock %x, read %x\n",
		bp->b_data, bp->b_blkno, bp->b_bcount,
		bp->b_flags & B_READ);
#endif

	if (bp->b_blkno >= 512) {
		s = splbio();
		biodone(bp);
		splx(s);
		return;
	}
	bp->b_rawblkno = bp->b_blkno;
	s = splbio();
	disksort_blkno(&tu_sc.sc_q, bp); /* Why not use disksort? */
	if (tu_sc.sc_state == SC_READY)
		ctustart(bp);
	splx(s);
}

void
ctustart(bp)
	struct	buf *bp;
{
	struct rsp *rsp = (struct rsp *)tu_sc.sc_rsp;


	tu_sc.sc_xfptr = tu_sc.sc_blk = bp->b_data;
	tu_sc.sc_tpblk = bp->b_blkno;
	tu_sc.sc_nbytes = bp->b_bcount;
	tu_sc.sc_xbytes = tu_sc.sc_bbytes = 0;
	tu_sc.sc_op = bp->b_flags & B_READ ? RSP_OP_READ : RSP_OP_WRITE;

	rsp->rsp_typ = RSP_TYP_COMMAND;
	rsp->rsp_sz = 012;
	rsp->rsp_op = tu_sc.sc_op;
	rsp->rsp_mod = 0;
	rsp->rsp_drv = 0;
	rsp->rsp_sw = rsp->rsp_xx1 = rsp->rsp_xx2 = 0;
	rsp->rsp_cnt = tu_sc.sc_nbytes;
	rsp->rsp_blk = tu_sc.sc_tpblk;
	rsp->rsp_sum = ctu_cksum((unsigned short *)rsp, 6);
	tu_sc.sc_state = SC_SEND_CMD;
	if (tu_sc.sc_xmtok) {
		tu_sc.sc_xmtok = 0;
		ctutintr(NULL);
	}
}

int
ctuioctl(dev, cmd, data, fflag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int fflag;
	struct proc *p;
{
	return 0;
}

/*
 * Not bloody likely... 
 */
int
ctudump(dev, blkno, va, size)
	dev_t dev;
	daddr64_t blkno;
	caddr_t va;
	size_t size;
{
	return 0;
}

void
cturintr(arg)
	void *arg;
{
	int	status = mfpr(PR_CSRD);
	struct	buf *bp;

	bp = BUFQ_FIRST(&tu_sc.sc_q);
	switch (tu_sc.sc_state) {

	case SC_UNUSED:
		printf("stray console storage interrupt, got %o\n", status);
		break;

	case SC_INIT:
		if (status != RSP_TYP_CONTINUE)
			tu_sc.sc_error = EIO;
		wakeup((void *)&tu_sc);
		break;
	case SC_GET_RESP:
		tu_sc.sc_tpblk++;
		if (tu_sc.sc_xbytes == tu_sc.sc_nbytes) {
			tu_sc.sc_bbytes++;
			if (tu_sc.sc_bbytes == 146) { /* We're finished! */
#ifdef TUDEBUG
				printf("Xfer ok\n");
#endif
				BUFQ_REMOVE(&tu_sc.sc_q, bp);
				biodone(bp);
				tu_sc.sc_xmtok = 1;
				tu_sc.sc_state = SC_READY;
				if (BUFQ_FIRST(&tu_sc.sc_q) != NULL)
					ctustart(BUFQ_FIRST(&tu_sc.sc_q));
			}
			break;
		}
		tu_sc.sc_bbytes++;
		if (tu_sc.sc_bbytes <  3) /* Data header */
			break;
		if (tu_sc.sc_bbytes == 132) { /* Finished */
			tu_sc.sc_bbytes = 0;
			break;
		}
		if (tu_sc.sc_bbytes == 131) /* First checksum */
			break;
		tu_sc.sc_xfptr[tu_sc.sc_xbytes++] = status;
		break;

	case SC_GET_WCONT:
		if (status != 020)
			printf("SC_GET_WCONT: status %o\n", status);
		else
			ctutintr(NULL);
		tu_sc.sc_xmtok = 0;
		break;

	case SC_RESTART:
		ctustart(BUFQ_FIRST(&tu_sc.sc_q));
		break;

	default:
		if (status == 4) { /* Protocol error, or something */
			tu_sc.sc_state = SC_RESTART;
			mtpr(RSP_TYP_INIT, PR_CSTD);
			return;
		}
		printf("Unknown receive ctuintr state %d, pack %o\n",
		    tu_sc.sc_state, status);
	}

}

void
ctutintr(arg)
	void *arg;
{
	int	c;

	if (tu_sc.sc_xmtok)
		return;

	switch (tu_sc.sc_state) {
	case SC_SEND_CMD:
		c = tu_sc.sc_rsp[tu_sc.sc_xbytes++] & 0xff;
		mtpr(c, PR_CSTD);
		if (tu_sc.sc_xbytes > 13) {
			tu_sc.sc_state = (tu_sc.sc_op == RSP_OP_READ ?
			    SC_GET_RESP : SC_GET_WCONT);
			tu_sc.sc_xbytes = 0;
			tu_sc.sc_xmtok++;
		}
		break;

	case SC_GET_WCONT:
		switch (tu_sc.sc_bbytes) {
		case 0:
			mtpr(1, PR_CSTD); /* This is a data packet */
			break;

		case 1:
			mtpr(128, PR_CSTD); /* # of bytes to send */
			break;

		case 130:
			mtpr(0, PR_CSTD); /* First checksum */
			break;

		case 131:
			mtpr(0, PR_CSTD); /* Second checksum */
			break;

		case 132: /* Nothing to send... */
			tu_sc.sc_bbytes = -1;
			if (tu_sc.sc_xbytes == tu_sc.sc_nbytes + 1)
				tu_sc.sc_op = SC_GET_END;
			break;
		default:
			c = tu_sc.sc_rsp[tu_sc.sc_xbytes++] & 0xff;
			mtpr(c, PR_CSTD);
			break;
		}
		tu_sc.sc_bbytes++;
		break;

	default:
		printf("Unknown xmit ctuintr state %d\n",tu_sc.sc_state);
	}
}

short
ctu_cksum(buf, words)
	unsigned short *buf;
	int words;
{
	int i, cksum;

	for (i = cksum = 0; i < words; i++)
		cksum += buf[i];

hej:	if (cksum > 65535) {
		cksum = (cksum & 65535) + (cksum >> 16);
		goto hej;
	}
	return cksum;
}

int	oldtp;

/*
 * Watch so that we don't get blocked unnecessary due to lost int's.
 */
void
ctuwatch(arg)
	void *arg;
{

	callout_reset(&ctu_watch_ch, hz, ctuwatch, NULL);

	if (tu_sc.sc_state == SC_GET_RESP && tu_sc.sc_tpblk != 0 &&
	    tu_sc.sc_tpblk == oldtp && (tu_sc.sc_tpblk % 128 != 0)) {
		printf("tu0: lost recv interrupt\n");
		ctustart(BUFQ_FIRST(&tu_sc.sc_q));
		return;
	}
	if (tu_sc.sc_state == SC_RESTART)
		mtpr(RSP_TYP_INIT, PR_CSTS);
	oldtp = tu_sc.sc_tpblk;
}
