/*	$NetBSD: z8530sc.c,v 1.1 1996/01/24 01:07:23 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)zs.c	8.1 (Berkeley) 7/19/93
 */

/*
 * Zilog Z8530 Dual UART driver (common part)
 *
 * This file contains the machine-independent parts of the
 * driver common to tty and keyboard/mouse sub-drivers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

int
zs_break(cs, set)
	struct zs_chanstate *cs;
	int set;
{
	int s;

	s = splzs();
	if (set) {
		cs->cs_preg[5] |= ZSWR5_BREAK;
		cs->cs_creg[5] |= ZSWR5_BREAK;
	} else {
		cs->cs_preg[5] &= ~ZSWR5_BREAK;
		cs->cs_creg[5] &= ~ZSWR5_BREAK;
	}
	ZS_WRITE(cs, 5, cs->cs_creg[5]);
	splx(s);
}


/*
 * Compute the current baud rate given a ZSCC channel.
 */
int
zs_getspeed(cs)
	struct zs_chanstate *cs;
{
	int tconst;

	tconst = ZS_READ(cs, 12);
	tconst |= ZS_READ(cs, 13) << 8;
	return (TCONST_TO_BPS(cs->cs_pclk_div16, tconst));
}

/*
 * drain on-chip fifo
 */
void
zs_iflush(cs)
	struct zs_chanstate *cs;
{
	u_char c, rr0, rr1;

	for (;;) {
		/* Is there input available? */
		rr0 = *(cs->cs_reg_csr);
		ZS_DELAY();
		if ((rr0 & ZSRR0_RX_READY) == 0)
			break;

		/* Read the data. */
		c = *(cs->cs_reg_data);
		ZS_DELAY();

		/* Need to read status register too? */
		rr1 = ZS_READ(cs, 1);
		if (rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) {
			/* Clear the receive error. */
			*(cs->cs_reg_csr) = ZSWR0_RESET_ERRORS;
			ZS_DELAY();
		}
	}
}
	

/*
 * Write the given register set to the given zs channel in the proper order.
 * The channel must not be transmitting at the time.  The receiver will
 * be disabled for the time it takes to write all the registers.
 * Call this with interrupts disabled.
 */
void
zs_loadchannelregs(cs)
	struct zs_chanstate *cs;
{
	u_char *reg;
	int i;

	/* Copy "pending" regs to "current" */
	bcopy((caddr_t)cs->cs_preg, (caddr_t)cs->cs_creg, 16);
	reg = cs->cs_creg;	/* current regs */

	*(cs->cs_reg_csr) = ZSM_RESET_ERR;	/* XXX: reset error condition */
	ZS_DELAY();

#if 1
	/*
	 * XXX: Is this really a good idea?
	 * XXX: Should go elsewhere! -gwr
	 */
	zs_iflush(cs);	/* XXX */
#endif

	/* baud clock divisor, stop bits, parity */
	ZS_WRITE(cs, 4, reg[4]);

	/* misc. TX/RX control bits */
	ZS_WRITE(cs, 10, reg[10]);

	/* char size, enable (RX/TX) */
	ZS_WRITE(cs, 3, reg[3] & ~ZSWR3_RX_ENABLE);
	ZS_WRITE(cs, 5, reg[5] & ~ZSWR5_TX_ENABLE);

	/* interrupt enables: TX, TX, STATUS */
	ZS_WRITE(cs, 1, reg[1]);

#if 0
	/*
	 * Registers 2 and 9 are special because they are
	 * actually common to both channels, but must be
	 * programmed through channel A.  The "zsc" attach
	 * function takes care of setting these registers
	 * and they should not be touched thereafter.
	 */
	/* interrupt vector */
	ZS_WRITE(cs, 2, reg[2]);
	/* master interrupt control */
	ZS_WRITE(cs, 9, reg[9]);
#endif

	/* clock mode control */
	ZS_WRITE(cs, 11, reg[11]);

	/* baud rate (lo/hi) */
	ZS_WRITE(cs, 12, reg[12]);
	ZS_WRITE(cs, 13, reg[13]);

	/* Misc. control bits */
	ZS_WRITE(cs, 14, reg[14]);

	/* which lines cause status interrupts */
	ZS_WRITE(cs, 15, reg[15]);

	/* char size, enable (RX/TX)*/
	ZS_WRITE(cs, 3, reg[3]);
	ZS_WRITE(cs, 5, reg[5]);
}


/*
 * ZS hardware interrupt.  Scan all ZS channels.  NB: we know here that
 * channels are kept in (A,B) pairs.
 *
 * Do just a little, then get out; set a software interrupt if more
 * work is needed.
 *
 * We deliberately ignore the vectoring Zilog gives us, and match up
 * only the number of `reset interrupt under service' operations, not
 * the order.
 */
int
zsc_intr_hard(arg)
	void *arg;
{
	register struct zsc_softc *zsc = arg;
	register struct zs_chanstate *cs_a;
	register struct zs_chanstate *cs_b;
	register int rval, soft;
	register u_char rr3;

	cs_a = &zsc->zsc_cs[0];
	cs_b = &zsc->zsc_cs[1];
	rval = 0;
	soft = 0;

	/* Note: only channel A has an RR3 */
	rr3 = ZS_READ(cs_a, 3);

	/* Handle receive interrupts first. */
	if (rr3 & ZSRR3_IP_A_RX)
		(*cs_a->cs_ops->zsop_rxint)(cs_a);
	if (rr3 & ZSRR3_IP_B_RX)
		(*cs_b->cs_ops->zsop_rxint)(cs_b);

	/* Handle transmit done interrupts. */
	if (rr3 & ZSRR3_IP_A_TX)
		(*cs_a->cs_ops->zsop_txint)(cs_a);
	if (rr3 & ZSRR3_IP_B_TX)
		(*cs_b->cs_ops->zsop_txint)(cs_b);

	/* Handle status interrupts. */
	if (rr3 & ZSRR3_IP_A_STAT)
		(*cs_a->cs_ops->zsop_stint)(cs_a);
	if (rr3 & ZSRR3_IP_B_STAT)
		(*cs_b->cs_ops->zsop_stint)(cs_b);

	/* Clear interrupt. */
	if (rr3 & (ZSRR3_IP_A_RX | ZSRR3_IP_A_TX | ZSRR3_IP_A_STAT)) {
		*(cs_a->cs_reg_csr) = ZSWR0_CLR_INTR;
		ZS_DELAY();
		rval |= 1;
	}
	if (rr3 & (ZSRR3_IP_B_RX | ZSRR3_IP_B_TX | ZSRR3_IP_B_STAT)) {
		*(cs_b->cs_reg_csr) = ZSWR0_CLR_INTR;
		ZS_DELAY();
		rval |= 2;
	}

	if ((cs_a->cs_softreq) || (cs_b->cs_softreq))
	{
		/* This is a machine-dependent function. */
		zsc_req_softint(zsc);
	}

	return (rval);
}


/*
 * ZS software interrupt.  Scan all channels for deferred interrupts.
 */
int
zsc_intr_soft(arg)
	void *arg;
{
	register struct zsc_softc *zsc = arg;
	register struct zs_chanstate *cs;
	register int req, rval, s, unit;

	rval = 0;
	for (unit = 0; unit < 2; unit++) {
		cs = &zsc->zsc_cs[unit];

		s = splzs();
		req = cs->cs_softreq;
		cs->cs_softreq = 0;
		splx(s);

		if (req) {
			(*cs->cs_ops->zsop_softint)(cs);
			rval = 1;
		}
	}
	return (rval);
}


static int
zsnull_intr(cs)
	struct zs_chanstate *cs;
{
	ZS_WRITE(cs,  1, 0);
	ZS_WRITE(cs, 15, 0);
}

static int
zsnull_softint(cs)
	struct zs_chanstate *cs;
{
}

struct zsops zsops_null = {
	zsnull_intr,	/* receive char available */
	zsnull_intr,	/* external/status */
	zsnull_intr,	/* xmit buffer empty */
	zsnull_softint,	/* process software interrupt */
};
