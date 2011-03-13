/*	$OpenBSD: ctu.c,v 1.5 2011/03/13 00:13:53 deraadt Exp $ */
/*	$NetBSD: ctu.c,v 1.3 2000/05/20 13:30:03 ragge Exp $ */
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
 * Standalone device driver for 11/750 Console TU58.
 * It can only handle reads, and doesn't calculate checksum.
 */

#include <sys/param.h>

#include <lib/libsa/stand.h>

#include <machine/mtpr.h>
#include <machine/rsp.h>

#include "vaxstand.h"

static short ctu_cksum(unsigned short *, int);

enum tu_state {
	SC_INIT,
	SC_READY,
	SC_SEND_CMD,
	SC_GET_RESP,
};

volatile struct tu_softc {
	enum	tu_state sc_state;
	char	sc_rsp[15];	/* Should be struct rsb; but don't work */
	u_char	*sc_xfptr;	/* Current char to xfer */
	int	sc_nbytes;	/* Number of bytes to xfer */
	int	sc_xbytes;	/* Number of xfer'd bytes */
	int	sc_bbytes;	/* Number of xfer'd bytes this block */
} tu_sc;

void	ctutintr(void);
void	cturintr(void);

int
ctuopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{

	tu_sc.sc_state = SC_INIT;

	mtpr(RSP_TYP_INIT, PR_CSTD);
	cturintr();
	tu_sc.sc_state = SC_READY;
	return 0;

}

int
ctustrategy(void *f, int func, daddr32_t dblk, size_t size, void *buf, size_t *rsize)
{
	struct rsp *rsp = (struct rsp *)tu_sc.sc_rsp;

	tu_sc.sc_xfptr = buf;
	tu_sc.sc_nbytes = size;
	tu_sc.sc_xbytes = tu_sc.sc_bbytes = 0;

	rsp->rsp_typ = RSP_TYP_COMMAND;
	rsp->rsp_sz = 012;
	rsp->rsp_op = RSP_OP_READ;
	rsp->rsp_mod = 0;
	rsp->rsp_drv = 0;
	rsp->rsp_sw = rsp->rsp_xx1 = rsp->rsp_xx2 = 0;
	rsp->rsp_cnt = tu_sc.sc_nbytes;
	rsp->rsp_blk = dblk;
	rsp->rsp_sum = ctu_cksum((u_short *)rsp, 6);
	tu_sc.sc_state = SC_SEND_CMD;
	while (tu_sc.sc_state != SC_GET_RESP)
		ctutintr();
	while (tu_sc.sc_state != SC_READY)
		cturintr();
	*rsize = size;
	return 0;
}

void
cturintr(void)
{
	int	status;

	while ((mfpr(PR_CSRS) & 0x80) == 0)
		;

	status = mfpr(PR_CSRD);

	switch (tu_sc.sc_state) {

	case SC_INIT:
		break;

	case SC_GET_RESP:
		if (tu_sc.sc_xbytes == tu_sc.sc_nbytes) {
			tu_sc.sc_bbytes++;
			if (tu_sc.sc_bbytes == 146)
				tu_sc.sc_state = SC_READY;
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
	case SC_READY:
	case SC_SEND_CMD:
		break;
	}

}

void
ctutintr(void)
{
	int	c;

	while ((mfpr(PR_CSTS) & 0x80) == 0)
		;

	c = tu_sc.sc_rsp[tu_sc.sc_xbytes++] & 0xff;
	mtpr(c, PR_CSTD);
	if (tu_sc.sc_xbytes > 13) {
		tu_sc.sc_state = SC_GET_RESP;
		tu_sc.sc_xbytes = 0;
	}
}

short
ctu_cksum(unsigned short *buf, int words)
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
