/*	$OpenBSD: via.c,v 1.7 1996/06/08 16:21:17 briggs Exp $	*/
/*	$NetBSD: via.c,v 1.47 1996/06/07 13:04:46 briggs Exp $	*/

/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *	This code handles both the VIA and RBV functionality.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/viareg.h>

#include "ncrscsi.h"
#include "ncr96scsi.h"

static void	via1_noint __P((void *));
static void	via2_noint __P((void *));
static void	slot_ignore __P((void *, int));
static void	slot_noint __P((void *, int));
void	mrg_adbintr __P((void *));
void	mrg_pmintr __P((void *));
void	rtclock_intr __P((void *));
void	profclock __P((void *));
void	via1_intr __P((struct frame *));
void	via2_nubus_intr __P((void *));
void	rbv_nubus_intr __P((void *));
int	VIA2 = 1;		/* default for II, IIx, IIcx, SE/30. */

void (*via1itab[7]) __P((void *))={
	via1_noint,
	via1_noint,
	mrg_adbintr,
	via1_noint,
	mrg_pmintr,
	via1_noint,
	rtclock_intr,
};	/* VIA1 interrupt handler table */

void (*via2itab[7]) __P((void *))={
	via2_noint,
	via2_nubus_intr,
	via2_noint,
	via2_noint,
	via2_noint,	/* snd_intr */
	via2_noint,	/* via2t2_intr */
	via2_noint,
};	/* VIA2 interrupt handler table */

void *via2iarg[7] = {
	(void *) 0, (void *) 1, (void *) 2, (void *) 3,
	(void *) 4, (void *) 5, (void *) 6
};	/* Arg array for VIA2 interrupts. */

void		via2_intr __P((struct frame *));
void		rbv_intr __P((struct frame *));

void		(*real_via2_intr)(struct frame *);

/*
 * Nubus slot interrupt routines and parameters for slots 9-15.  Note
 * that for simplicity of code, "v2IRQ0" for internal video is treated
 * as a slot 15 interrupt; this slot is quite ficticious in real-world
 * Macs.  See also GMGH, pp. 165-167, and "Monster, Loch Ness."
 */
void (*slotitab[7]) __P((void *, int)) = {
	slot_noint,
	slot_noint,
	slot_noint,
	slot_noint,
	slot_noint,
	slot_noint,
	slot_noint	/* int_video_intr */
};

void	*slotptab[7] = {
	(void *) 0, (void *) 1, (void *) 2, (void *) 3,
	(void *) 4, (void *) 5, (void *) 6
};

void
VIA_initialize()
{
	/* Initialize VIA1 */
	/* set all timers to 0 */
	via_reg(VIA1, vT1L) = 0;
	via_reg(VIA1, vT1LH) = 0;
	via_reg(VIA1, vT1C) = 0;
	via_reg(VIA1, vT1CH) = 0;
	via_reg(VIA1, vT2C) = 0;
	via_reg(VIA1, vT2CH) = 0;

	/* turn off timer latch */
	via_reg(VIA1, vACR) &= 0x3f;

	if (VIA2 == VIA2OFF) {
		/* Initialize VIA2 */
		via2_reg(vT1L) = 0;
		via2_reg(vT1LH) = 0;
		via2_reg(vT1C) = 0;
		via2_reg(vT1CH) = 0;
		via2_reg(vT2C) = 0;
		via2_reg(vT2CH) = 0;

		/* turn off timer latch */
		via2_reg(vACR) &= 0x3f;

		/*
		 * Turn off SE/30 video interrupts.
		 */
		if (mac68k_machine.machineid == MACH_MACSE30) {
			via_reg(VIA1, vBufB) |= (0x40);
			via_reg(VIA1, vDirB) |= (0x40);
		}

		/*
		 * unlock nubus and set vPCR for SCSI interrupts.
		 */
		via2_reg(vPCR)   = 0x66;
		via2_reg(vBufB) |= 0x02;
		via2_reg(vDirB) |= 0x02;

		real_via2_intr = via2_intr;
		via2itab[1] = via2_nubus_intr;

	} else {	/* RBV */
		if (current_mac_model->class == MACH_CLASSIIci) {
			/*
			 * Disable cache card. (p. 174--GtMFH)
			 */
			via2_reg(rBufB) |= DB2O_CEnable;
		}
		real_via2_intr = rbv_intr;
		via2itab[1] = rbv_nubus_intr;
		add_nubus_intr(0, slot_ignore, NULL);
	}
}

/*
 * Set the state of the modem serial port's clock source.
 */
void
via_set_modem(onoff)
	int	onoff;
{
	via_reg(VIA1, vDirA) |= DA1O_vSync;
	if (onoff)
		via_reg(VIA1, vBufA) |= DA1O_vSync;
	else
		via_reg(VIA1, vBufA) &= ~DA1O_vSync;
}

void
via1_intr(fp)
	struct frame *fp;
{
	register unsigned char intbits;
	register unsigned char mask;
	register unsigned char bitnum;

	intbits = via_reg(VIA1, vIFR) & via_reg(VIA1, vIER);

	if (intbits == 0)
		return;

	/*
	 * Unflag interrupts here.  If we do it after each interrupt,
	 * the MRG ADB hangs up.
	 */
	via_reg(VIA1, vIFR) = intbits;

	mask = (unsigned char) 1;

	bitnum = 0;
	do {
		if (intbits & mask) {
			via1itab[bitnum]((void *)((int) bitnum));
			/* via_reg(VIA1, vIFR) = mask; */
		}
		mask <<= 1;
	} while (intbits >= mask && ++bitnum < 7);
}

void
via2_intr(fp)
	struct frame *fp;
{
	register unsigned char intbits;
	register unsigned char mask;
	register unsigned char bitnum;

	intbits = via2_reg(vIFR) & via2_reg(vIER);

	if (intbits == 0)
		return;

	via2_reg(vIFR) = intbits;

	mask = (unsigned char) 1;

	bitnum = 0;
	do {
		if (intbits & mask)
			via2itab[bitnum](via2iarg[bitnum]);
		mask <<= 1;
	} while (intbits >= mask && ++bitnum < 7);
}

void
rbv_intr(fp)
	struct frame *fp;
{
	register unsigned char intbits;
	register unsigned char mask;
	register unsigned char bitnum;

	intbits = (via2_reg(vIFR + rIFR) & via2_reg(vIER + rIER));

	if (intbits == 0)
		return;

	via2_reg(rIFR) = intbits;

	mask = (unsigned char) 1;

	bitnum = 0;
	do {
		if (intbits & mask)
			via2itab[bitnum](via2iarg[bitnum]);
		mask <<= 1;
	} while (intbits >= mask && ++bitnum < 7);
}

static void
via1_noint(bitnum)
	void *bitnum;
{
	printf("via1_noint(%d)\n", (int) bitnum);
}

static void
via2_noint(bitnum)
	void *bitnum;
{
	printf("via2_noint(%d)\n", (int) bitnum);
}

static int	nubus_intr_mask = 0;

int
add_nubus_intr(slot, func, client_data)
	int slot;
	void (*func) __P((void *, int));
	void *client_data;
{
	int	s = splhigh();

	/*
	 * Map Nubus slot 0 to "slot" 15; see note on Nubus slot
	 * interrupt tables.
	 */
	if (slot == 0)
		slot = 15;
	if (slot < 9 || slot > 15)
		return 0;

	slotitab[slot-9] = func;
	slotptab[slot-9] = client_data;

	nubus_intr_mask |= (1 << (slot-9));

	/*
	 * The following should be uncommented and the call in if_ae.c
	 * removed when we can reliably handle interrupts from the video
	 * cards.
	 */
/*	enable_nubus_intr();	*/

	splx(s);
	return 1;
}

void
enable_nubus_intr()
{
	if (VIA2 == VIA2OFF)
		via2_reg(vIER) = V2IF_SLOTINT | 0x80;
	else
		via2_reg(rIER) = V2IF_SLOTINT | 0x80;
}

/*ARGSUSED*/
void
via2_nubus_intr(bitarg)
	void *bitarg;
{
	register int	i, mask, ints;

try_again:
	via2_reg(vIFR) = 0x80 | V2IF_SLOTINT;
	if ((ints = ((~via2_reg(vBufA)) & nubus_intr_mask)) != 0) {
		mask = (1 << 6);
		i = 7;
		while (i--) {
			if (ints & mask)
				(*slotitab[i])(slotptab[i], i+9);
			mask >>= 1;
		}
	} else
		return;
	goto try_again;
}

/*ARGSUSED*/
void
rbv_nubus_intr(bitarg)
	void *bitarg;
{
	register int	i, mask, ints;

try_again:
	via2_reg(rIFR) = 0x80 | V2IF_SLOTINT;
	if ((ints = ((~via2_reg(rBufA)) & via2_reg(rSlotInt))) != 0) {
		mask = (1 << 6);
		i = 7;
		while (i--) {
			if (ints & mask)
				(*slotitab[i])(slotptab[i], i+9);
			mask >>= 1;
		}
	} else
		return;
	goto try_again;
}

static void
slot_ignore(client_data, slot)
	void *client_data;
	int slot;
{
	register int mask = (1 << (slot-9));

	if (VIA2 == VIA2OFF) {
		via2_reg(vDirA) |= mask;
		via2_reg(vBufA) = mask;
		via2_reg(vDirA) &= ~mask;
	} else
		via2_reg(rBufA) = mask;
}

static void
slot_noint(client_data, slot)
	void *client_data;
	int slot;
{
	printf("slot_noint() slot %x\n", slot);
}

void
via_shutdown()
{
	if (VIA2 == VIA2OFF) {
		via2_reg(vDirB) |= 0x04;  /* Set write for bit 2 */
		via2_reg(vBufB) &= ~0x04; /* Shut down */
	} else if (VIA2 == RBVOFF)
		via2_reg(rBufB) &= ~0x04;
}

int
rbv_vidstatus()
{
/*
	int montype;

	montype = via2_reg(rMonitor) & RBVMonitorMask;
	if(montype == RBVMonIDNone)
		montype = RBVMonIDOff;
*/
	return(0);
}

void
mac68k_register_scsi_drq(drq_func, client_data)
	void	(*drq_func)(void *);
	void	*client_data;
{
	if (drq_func) {
		via2itab[0] = drq_func;
		via2iarg[0] = client_data;
	} else {
 		via2itab[0] = via2_noint;
		via2iarg[0] = (void *) 0;
	}
}

void
mac68k_register_scsi_irq(irq_func, client_data)
	void	(*irq_func)(void *);
	void	*client_data;
{
	if (irq_func) {
 		via2itab[3] = irq_func;
		via2iarg[3] = client_data;
	} else {
 		via2itab[3] = via2_noint;
		via2iarg[3] = (void *) 3;
	}
}

void
mac68k_register_via1_t1_irq(irq_func)
	void	(*irq_func)(void *);
{
	if (irq_func)
 		via1itab[6] = irq_func;
	else
 		via1itab[6] = rtclock_intr;
}
