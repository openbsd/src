/*	$OpenBSD: z8530sc.c,v 1.6 2004/08/03 12:10:47 todd Exp $	*/
/*	$NetBSD: z8530sc.c,v 1.1 1996/05/18 18:54:28 briggs Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

/* #include <dev/ic/z8530reg.h> */
#include "z8530reg.h"
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
	zs_write_reg(cs, 5, cs->cs_creg[5]);
	splx(s);

	return 0;
}


/*
 * Compute the current baud rate given a ZSCC channel.
 */
int
zs_getspeed(cs)
	struct zs_chanstate *cs;
{
	int tconst;

	tconst = zs_read_reg(cs, 12);
	tconst |= zs_read_reg(cs, 13) << 8;
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
 * Figure out if a chip is an NMOS 8530, a CMOS 8530,
 * or an 85230. We use a form of the test in the Zilog SCC
 * users manual.
 */
int
zs_checkchip(cs)
	struct zs_chanstate *cs;
{
	char	r1, r2, r3;
	int	chip;

	/* we assume we can write to the chip */

	r1=cs->cs_creg[15]; /* see if bit 0 sticks */
	zs_write_reg(cs, 15, (r1 | ZSWR15_ENABLE_ENHANCED));
	if ((zs_read_reg(cs, 15) & ZSWR15_ENABLE_ENHANCED) != 0) {
		/* we have either an 8580 or 85230. NB Zilog says we should only
		 * have an 85230 at this point, but the 8580 seems to pass this
		 * test too. To test, we try to write to WR7', and see if we
		 * loose sight of RR14. */
		r2=cs->cs_creg[14];
		r3=(r2 != 0x47) ? 0x47 : 0x40;
		/* unique bit pattern to turn on reading of WR7' at RR14 */
		zs_write_reg(cs, 7, ~r2);
		if (zs_read_reg(cs, ZSRR_ENHANCED) != r2) {
			chip = ZS_CHIP_ESCC;
			zs_write_reg(cs, 7, cs->cs_creg[ZS_ENHANCED_REG]);
		} else {
			chip = ZS_CHIP_8580;
			zs_write_reg(cs, 7, cs->cs_creg[7]);
		}
		zs_write_reg(cs, 15, r1);
	} else { /* now we have to tell an NMOS from a CMOS; does WR15 D2 work? */
		zs_write_reg(cs, 15, (r1 | ZSWR15_SDLC_FIFO));
		r2=cs->cs_creg[2];
		zs_write_reg(cs, 2, (r2 | 0x80));
		chip = (zs_read_reg(cs, 6) & 0x80) ? ZS_CHIP_NMOS : ZS_CHIP_CMOS;
		zs_write_reg(cs, 2, r2);
	}
	zs_write_reg(cs, 15, r1);
	return chip;
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

	/* Copy "pending" regs to "current" */
	bcopy((caddr_t)cs->cs_preg, (caddr_t)cs->cs_creg, 16);
	reg = cs->cs_creg;	/* current regs */

	zs_write_csr(cs, ZSM_RESET_ERR);	/* XXX: reset error condition */

#if 1
	/*
	 * XXX: Is this really a good idea?
	 * XXX: Should go elsewhere! -gwr
	 */
	zs_iflush(cs);	/* XXX */
#endif

	/* disable interrupts */
	zs_write_reg(cs, 1, reg[1] &
		~(ZSWR1_RIE_SPECIAL_ONLY | ZSWR1_TIE | ZSWR1_SIE));

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

	if ((cs->cs_cclk_flag & ZSC_EXTERN) ||
	    (cs->cs_pclk_flag & ZSC_EXTERN))
		zsmd_setclock(cs);
	/* the md layer wants to do something; let it. */

	/* clock mode control */
	zs_write_reg(cs, 11, reg[11]);

	/* baud rate (lo/hi) */
	zs_write_reg(cs, 12, reg[12]);
	zs_write_reg(cs, 13, reg[13]);

	/* Misc. control bits */
	zs_write_reg(cs, 14, reg[14]);

	/* which lines cause status interrupts */
	zs_write_reg(cs, 15, reg[15]);

	/* Zilog docs recommend resetting external status twice at this
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

	cs->cs_cclk_flag = cs->cs_pclk_flag;
	cs->cs_csource = cs->cs_psource;
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
	register int rval;
	register u_char rr3, rr3a;
#ifdef DIAGNOSTIC
	register int loopcount;
	loopcount = ZS_INTERRUPT_CNT;
#endif

	cs_a = &zsc->zsc_cs[0];
	cs_b = &zsc->zsc_cs[1];
	rval = 0;
	rr3a = 0;

	/* Note: only channel A has an RR3 */
	rr3 = zs_read_reg(cs_a, 3);

	while ((rr3 = zs_read_reg(cs_a, ZSRR_IPEND))
#ifdef DIAGNOSTIC
		 && --loopcount
#endif
		) {

		/* Handle receive interrupts first. */
		if (rr3 & ZSRR3_IP_A_RX)
			(*cs_a->cs_ops->zsop_rxint)(cs_a);
		if (rr3 & ZSRR3_IP_B_RX)
			(*cs_b->cs_ops->zsop_rxint)(cs_b);
	
		/* Handle status interrupts (i.e. flow control). */
		if (rr3 & ZSRR3_IP_A_STAT)
			(*cs_a->cs_ops->zsop_stint)(cs_a);
		if (rr3 & ZSRR3_IP_B_STAT)
			(*cs_b->cs_ops->zsop_stint)(cs_b);
	
		/* Handle transmit done interrupts. */
		if (rr3 & ZSRR3_IP_A_TX)
			(*cs_a->cs_ops->zsop_txint)(cs_a);
		if (rr3 & ZSRR3_IP_B_TX)
			(*cs_b->cs_ops->zsop_txint)(cs_b);
	
		rr3a |= rr3;
	}
#ifdef DIAGNOSTIC
	if (loopcount == 0) {
		if (rr3 & (ZSRR3_IP_A_RX | ZSRR3_IP_A_TX | ZSRR3_IP_A_STAT))
			cs_a->cs_flags |= ZS_FLAGS_INTERRUPT_OVERRUN;
		if (rr3 & (ZSRR3_IP_B_RX | ZSRR3_IP_B_TX | ZSRR3_IP_B_STAT))
			cs_b->cs_flags |= ZS_FLAGS_INTERRUPT_OVERRUN;
	}
#endif

	/* Clear interrupt. */
	if (rr3a & (ZSRR3_IP_A_RX | ZSRR3_IP_A_TX | ZSRR3_IP_A_STAT)) {
		zs_write_csr(cs_a, ZSWR0_CLR_INTR);
		rval |= 1;
	}
	if (rr3a & (ZSRR3_IP_B_RX | ZSRR3_IP_B_TX | ZSRR3_IP_B_STAT)) {
		zs_write_csr(cs_b, ZSWR0_CLR_INTR);
		rval |= 2;
	}

	if ((cs_a->cs_softreq) || (cs_b->cs_softreq)) {
		/* This is a machine-dependent function (or macro). */
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
	register int rval, unit;

	rval = 0;
	for (unit = 0; unit < 2; unit++) {
		cs = &zsc->zsc_cs[unit];

		/*
		 * The softint flag can be safely cleared once
		 * we have decided to call the softint routine.
		 * (No need to do splzs() first.)
		 */
		if (cs->cs_softreq) {
			cs->cs_softreq = 0;
			(*cs->cs_ops->zsop_softint)(cs);
			rval = 1;
		}
	}
	return (rval);
}

static void	zsnull_intr(struct zs_chanstate *);
static void	zsnull_softint(struct zs_chanstate *);

static void
zsnull_intr(cs)
	struct zs_chanstate *cs;
{
	zs_write_reg(cs,  1, 0);
	zs_write_reg(cs, 15, 0);
}

static void
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
