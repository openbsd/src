/*	$OpenBSD: z8530sc.c,v 1.1 2002/08/12 10:44:04 miod Exp $ */
/*	$NetBSD: z8530sc.c,v 1.4 1996/05/17 19:30:34 gwr Exp $	*/

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

#include <sparc/dev/z8530reg.h>
#include <machine/z8530var.h>

void
zs_break(cs, set)
	struct zs_chanstate *cs;
	int set;
{

	if (set) {
		cs->cs_preg[5] |= ZSWR5_BREAK;
		cs->cs_creg[5] |= ZSWR5_BREAK;
	} else {
		cs->cs_preg[5] &= ~ZSWR5_BREAK;
		cs->cs_creg[5] &= ~ZSWR5_BREAK;
	}
	zs_write_reg(cs, 5, cs->cs_creg[5]);
}


/*
 * drain on-chip fifo
 */
void
zs_iflush(cs)
	struct zs_chanstate *cs;
{
	u_char c, rr0, rr1;
	int i;

	/*
	 * Count how many times we loop. Some systems, such as some
	 * Apple PowerBooks, claim to have SCC's which they really don't.
	 */
	for (i = 0; i < 32; i++) {
		/* Is there input available? */
		rr0 = zs_read_csr(cs);
		if ((rr0 & ZSRR0_RX_READY) == 0)
			break;

		/*
		 * First read the status, because reading the data
		 * destroys the status of this char.
		 */
		rr1 = zs_read_reg(cs, 1);
		c = zs_read_data(cs);

		if (rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) {
			/* Clear the receive error. */
			zs_write_csr(cs, ZSWR0_RESET_ERRORS);
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

	zs_write_csr(cs, ZSM_RESET_ERR);	/* XXX: reset error condition */

#if 1
	/*
	 * XXX: Is this really a good idea?
	 * XXX: Should go elsewhere! -gwr
	 */
	zs_iflush(cs);	/* XXX */
#endif

	if (memcmp((caddr_t)cs->cs_preg, (caddr_t)cs->cs_creg, 16) == 0)
		return;		/* only change if values are different */

	/* Copy "pending" regs to "current" */
	memcpy((caddr_t)cs->cs_creg, (caddr_t)cs->cs_preg, 16);
	reg = cs->cs_creg;	/* current regs */

	/* disable interrupts */
	zs_write_reg(cs, 1, reg[1] & ~ZSWR1_IMASK);

	/* baud clock divisor, stop bits, parity */
	zs_write_reg(cs, 4, reg[4]);

	/* misc. TX/RX control bits */
	zs_write_reg(cs, 10, reg[10]);

	/* char size, enable (RX/TX) */
	zs_write_reg(cs, 3, reg[3] & ~ZSWR3_RX_ENABLE);
	zs_write_reg(cs, 5, reg[5] & ~ZSWR5_TX_ENABLE);

	/* synchronous mode stuff */
	zs_write_reg(cs, 6, reg[6]);
	zs_write_reg(cs, 7, reg[7]);

#if 0
	/*
	 * Registers 2 and 9 are special because they are
	 * actually common to both channels, but must be
	 * programmed through channel A.  The "zsc" attach
	 * function takes care of setting these registers
	 * and they should not be touched thereafter.
	 */
	/* interrupt vector */
	zs_write_reg(cs, 2, reg[2]);
	/* master interrupt control */
	zs_write_reg(cs, 9, reg[9]);
#endif

	/* Shut down the BRG */
	zs_write_reg(cs, 14, reg[14] & ~ZSWR14_BAUD_ENA);

#ifdef	ZS_MD_SETCLK
	/* Let the MD code setup any external clock. */
	ZS_MD_SETCLK(cs);
#endif	/* ZS_MD_SETCLK */

	/* clock mode control */
	zs_write_reg(cs, 11, reg[11]);

	/* baud rate (lo/hi) */
	zs_write_reg(cs, 12, reg[12]);
	zs_write_reg(cs, 13, reg[13]);

	/* Misc. control bits */
	zs_write_reg(cs, 14, reg[14]);

	/* which lines cause status interrupts */
	zs_write_reg(cs, 15, reg[15]);

	/*
	 * Zilog docs recommend resetting external status twice at this
	 * point. Mainly as the status bits are latched, and the first
	 * interrupt clear might unlatch them to new values, generating
	 * a second interrupt request.
	 */
	zs_write_csr(cs, ZSM_RESET_STINT);
	zs_write_csr(cs, ZSM_RESET_STINT);

	/* char size, enable (RX/TX)*/
	zs_write_reg(cs, 3, reg[3]);
	zs_write_reg(cs, 5, reg[5]);

	/* interrupt enables: TX, TX, STATUS */
	zs_write_reg(cs, 1, reg[1]);
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
	struct zsc_softc *zsc = arg;
	struct zs_chanstate *cs;
	u_char rr3;

	/* First look at channel A. */
	cs = &zsc->zsc_cs[0];
	/* Note: only channel A has an RR3 */
	rr3 = zs_read_reg(cs, 3);

	/*
	 * Clear interrupt first to avoid a race condition.
	 * If a new interrupt condition happens while we are
	 * servicing this one, we will get another interrupt
	 * shortly.  We can NOT just sit here in a loop, or
	 * we will cause horrible latency for other devices
	 * on this interrupt level (i.e. sun3x floppy disk).
	 */
	if (rr3 & (ZSRR3_IP_A_RX | ZSRR3_IP_A_TX | ZSRR3_IP_A_STAT)) {
		zs_write_csr(cs, ZSWR0_CLR_INTR);
		if (rr3 & ZSRR3_IP_A_RX)
			(*cs->cs_ops->zsop_rxint)(cs);
		if (rr3 & ZSRR3_IP_A_STAT)
			(*cs->cs_ops->zsop_stint)(cs, 0);
		if (rr3 & ZSRR3_IP_A_TX)
			(*cs->cs_ops->zsop_txint)(cs);
	}

	/* Now look at channel B. */
	cs = &zsc->zsc_cs[1];
	if (rr3 & (ZSRR3_IP_B_RX | ZSRR3_IP_B_TX | ZSRR3_IP_B_STAT)) {
		zs_write_csr(cs, ZSWR0_CLR_INTR);
		if (rr3 & ZSRR3_IP_B_RX)
			(*cs->cs_ops->zsop_rxint)(cs);
		if (rr3 & ZSRR3_IP_B_STAT)
			(*cs->cs_ops->zsop_stint)(cs, 0);
		if (rr3 & ZSRR3_IP_B_TX)
			(*cs->cs_ops->zsop_txint)(cs);
	}

	/* Note: caller will check cs_x->cs_softreq and DTRT. */
	return (rr3);
}


/*
 * ZS software interrupt.  Scan all channels for deferred interrupts.
 */
int
zsc_intr_soft(arg)
	void *arg;
{
	struct zsc_softc *zsc = arg;
	struct zs_chanstate *cs;
	int rval, chan;

	rval = 0;
	for (chan = 0; chan < 2; chan++) {
		cs = &zsc->zsc_cs[chan];

		/*
		 * The softint flag can be safely cleared once
		 * we have decided to call the softint routine.
		 * (No need to do splzs() first.)
		 */
		if (cs->cs_softreq) {
			cs->cs_softreq = 0;
			(*cs->cs_ops->zsop_softint)(cs);
			rval++;
		}
	}
	return (rval);
}

/*
 * Provide a null zs "ops" vector.
 */

void zsnull_rxint(struct zs_chanstate *);
void zsnull_stint(struct zs_chanstate *, int);
void zsnull_txint(struct zs_chanstate *);
void zsnull_softint(struct zs_chanstate *);

void
zsnull_rxint(cs)
	struct zs_chanstate *cs;
{
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

void
zsnull_stint(cs, force)
	struct zs_chanstate *cs;
	int force;
{
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

void
zsnull_txint(cs)
	struct zs_chanstate *cs;
{
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

void
zsnull_softint(cs)
	struct zs_chanstate *cs;
{
	zs_write_reg(cs,  1, 0);
	zs_write_reg(cs, 15, 0);
}

struct zsops zsops_null = {
	zsnull_rxint,	/* receive char available */
	zsnull_stint,	/* external/status */
	zsnull_txint,	/* xmit buffer empty */
	zsnull_softint,	/* process software interrupt */
};
