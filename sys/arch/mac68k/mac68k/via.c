/*	$OpenBSD: via.c,v 1.19 2004/11/26 21:21:28 miod Exp $	*/
/*	$NetBSD: via.c,v 1.62 1997/09/10 04:38:48 scottr Exp $	*/

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
#include <sys/evcount.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/viareg.h>

int	mrg_adbintr(void *);
int	mrg_pmintr(void *);
void	profclock(void *);
int	rbv_nubus_intr(void *);
static int rbv_slot_ignore(void *);
int	rtclock_intr(void *);
void	via1_intr(struct frame *);
int	via2_nubus_intr(void *);

int	VIA2 = 1;		/* default for II, IIx, IIcx, SE/30. */

struct intrhand via1intrs[7];
struct intrhand via2intrs[7];

void	oss_intr(struct frame *);
void	rbv_intr(struct frame *);
void	via2_intr(struct frame *);

void	(*real_via2_intr)(struct frame *);

/*
 * Nubus slot interrupt routines and parameters for slots 9-15.  Note
 * that for simplicity of code, "v2IRQ0" for internal video is treated
 * as a slot 15 interrupt; this slot is quite fictitious in real-world
 * Macs.  See also GMFH, pp. 165-167, and "Monster, Loch Ness."
 */
struct intrhand slotintrs[7];

void
via_init()
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

	/* register default VIA1 interrupts */
	via1_register_irq(2, mrg_adbintr, NULL, "adb");
	via1_register_irq(4, mrg_pmintr, NULL, "pm");
	via1_register_irq(VIA1_T1, rtclock_intr, NULL, "clock");

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

		/* register default VIA2 interrupts */
		via2_register_irq(1, via2_nubus_intr, NULL, NULL);
		/* 4 snd_intr, 5 via2t2_intr */

		/*
		 * Turn off SE/30 video interrupts.
		 */
		if (mac68k_machine.machineid == MACH_MACSE30) {
			via_reg(VIA1, vBufB) |= (0x40);
			via_reg(VIA1, vDirB) |= (0x40);
		}

		/*
		 * Set vPCR for SCSI interrupts.
		 */
		via2_reg(vPCR)   = 0x66;
		switch(mac68k_machine.machineid) {
		case MACH_MACPB140:
		case MACH_MACPB145:
		case MACH_MACPB150:
		case MACH_MACPB160:
		case MACH_MACPB165:
		case MACH_MACPB165C:
		case MACH_MACPB170:
		case MACH_MACPB180:
		case MACH_MACPB180C:
			break;
		default:
			via2_reg(vBufB) |= 0x02;	/* Unlock NuBus */
			via2_reg(vDirB) |= 0x02;
			break;
		}

		real_via2_intr = via2_intr;
	} else if (current_mac_model->class == MACH_CLASSIIfx) { /* OSS */
		real_via2_intr = oss_intr;
	} else {	/* RBV */
		if (current_mac_model->class == MACH_CLASSIIci) {
			/*
			 * Disable cache card. (p. 174--GtMFH)
			 */
			via2_reg(rBufB) |= DB2O_CEnable;
		}
		real_via2_intr = rbv_intr;
		via2_register_irq(1, rbv_nubus_intr, NULL, NULL);
		/* XXX necessary? */
		add_nubus_intr(0, rbv_slot_ignore, NULL, "dummy");
	}
}

/*
 * Set the state of the modem serial port's clock source.
 */
void
via_set_modem(int onoff)
{
	via_reg(VIA1, vDirA) |= DA1O_vSync;
	if (onoff)
		via_reg(VIA1, vBufA) |= DA1O_vSync;
	else
		via_reg(VIA1, vBufA) &= ~DA1O_vSync;
}

void
via1_intr(struct frame *fp)
{
	struct intrhand *ih;
	u_int8_t intbits, bitnum;
	u_int mask;

	intbits = via_reg(VIA1, vIFR) & via_reg(VIA1, vIER);

	if (intbits == 0)
		return;

	/*
	 * Unflag interrupts here.  If we do it after each interrupt,
	 * the MRG ADB hangs up.
	 */
	via_reg(VIA1, vIFR) = intbits;

	intbits &= 0x7f;
	mask = 1;
	for (bitnum = 0, ih = via1intrs; ; bitnum++, ih++) {
		if ((intbits & mask) != 0 && ih->ih_fn != NULL) {
			if ((*ih->ih_fn)(ih->ih_arg) != 0)
				ih->ih_count.ec_count++;
		} else {
#if 0
			printf("spurious VIA1 interrupt, source %d\n", bitnum);
#endif
		}
		mask <<= 1;
		if (intbits < mask)
			break;
	}
}

void
via2_intr(struct frame *fp)
{
	struct intrhand *ih;
	u_int8_t intbits, bitnum;
	u_int mask;

	intbits = via2_reg(vIFR) & via2_reg(vIER);

	if (intbits == 0)
		return;

	via2_reg(vIFR) = intbits;

	intbits &= 0x7f;
	mask = 1;
	for (bitnum = 0, ih = via2intrs; ; bitnum++, ih++) {
		if ((intbits & mask) != 0 && ih->ih_fn != NULL) {
			if ((*ih->ih_fn)(ih->ih_arg) != 0)
				ih->ih_count.ec_count++;
		} else {
#if 0
			printf("spurious VIA2 interrupt, source %d\n", bitnum);
#endif
		}
		mask <<= 1;
		if (intbits < mask)
			break;
	}
}

void
oss_intr(struct frame *fp)
{
	struct intrhand *ih;
	u_int8_t intbits, bitnum;
	u_int mask;

	intbits = via2_reg(vIFR + rIFR);

	if (intbits == 0)
		return;

	intbits &= 0x7f;
	mask =1 ;
	for (bitnum = 0, ih = slotintrs; ; bitnum++, ih++) {
		if (intbits & mask) {
			if (ih->ih_fn != NULL) {
				if ((*ih->ih_fn)(ih->ih_arg) != 0)
					ih->ih_count.ec_count++;
			} else {
				printf("spurious nubus interrupt, slot %d\n",
				    bitnum);
			}
			via2_reg(rIFR) = mask;
		}
		mask <<= 1;
		if (intbits < mask)
			break;
	}
}

void
rbv_intr(struct frame *fp)
{
	struct intrhand *ih;
	u_int8_t intbits, bitnum;
	u_int mask;

	intbits = (via2_reg(vIFR + rIFR) & via2_reg(vIER + rIER));

	if (intbits == 0)
		return;

	via2_reg(rIFR) = intbits;

	intbits &= 0x7f;
	mask = 1;
	for (bitnum = 0, ih = via2intrs; ; bitnum++, ih++) {
		if ((intbits & mask) != 0 && ih->ih_fn != NULL) {
			if ((*ih->ih_fn)(ih->ih_arg) != 0)
				ih->ih_count.ec_count++;
		} else {
#if 0
			printf("spurious VIA2 interrupt, source %d\n", bitnum);
#endif
		}
		mask <<= 1;
		if (intbits < mask)
			break;
	}
}

static int nubus_intr_mask = 0;

void
add_nubus_intr(int slot, int (*func)(void *), void *client_data,
    const char *name)
{
	struct intrhand *ih;
	int s;

	/*
	 * Map Nubus slot 0 to "slot" 15; see note on Nubus slot
	 * interrupt tables.
	 */
	if (slot == 0)
		slot = 15;
	slot -= 9;
#ifdef DIAGNOSTIC
	if (slot < 0 || slot > 7)
		panic("add_nubus_intr: wrong slot %d", slot + 9);
#endif

	s = splhigh();

	ih = &slotintrs[slot];

#ifdef DIAGNOSTIC
	if (ih->ih_fn != NULL)
		panic("add_nubus_intr: attempt to share slot %d", slot + 9);
#endif

	ih->ih_fn = func;
	ih->ih_arg = client_data;
	ih->ih_ipl = slot + 9;
	evcount_attach(&ih->ih_count, name, (void *)&ih->ih_ipl, &evcount_intr);

	nubus_intr_mask |= (1 << slot);

	splx(s);
}

void
enable_nubus_intr()
{
	if ((nubus_intr_mask & 0x3f) == 0)
		return;

	if (VIA2 == VIA2OFF)
		via2_reg(vIER) = 0x80 | V2IF_SLOTINT;
	else
		via2_reg(rIER) = 0x80 | V2IF_SLOTINT;
}

/*ARGSUSED*/
int
via2_nubus_intr(void *bitarg)
{
	struct intrhand *ih;
	u_int8_t i, intbits, mask;
	int rv = 0;

	via2_reg(vIFR) = 0x80 | V2IF_SLOTINT;
	while ((intbits = (~via2_reg(vBufA)) & nubus_intr_mask)) {
		for (i = 6, ih = &slotintrs[i], mask = 1 << i; mask != 0;
		    i--, ih--, mask >>= 1) {
			if (intbits & mask) {
				if (ih->ih_fn != NULL) {
					if ((*ih->ih_fn)(ih->ih_arg) != 0) {
						ih->ih_count.ec_count++;
						rv = 1;
					}
				} else {
#if 0
					printf("spurious nubus interrupt, slot %d\n",
					    i);
#endif
				}
			}
		}
		via2_reg(vIFR) = V2IF_SLOTINT;
	}
	return (rv);
}

/*ARGSUSED*/
int
rbv_nubus_intr(void *bitarg)
{
	struct intrhand *ih;
	u_int8_t i, intbits, mask;
	int rv = 0;

	via2_reg(rIFR) = 0x80 | V2IF_SLOTINT;
	while ((intbits = (~via2_reg(rBufA)) & via2_reg(rSlotInt))) {
		for (i = 6, ih = &slotintrs[i], mask = 1 << i; mask != 0;
		    i--, ih--, mask >>= 1) {
			if (intbits & mask) {
				if (ih->ih_fn != NULL) {
					if ((*ih->ih_fn)(ih->ih_arg) != 0) {
						ih->ih_count.ec_count++;
						rv = 1;
					}
				} else {
#if 0
					printf("spurious nubus interrupt, slot %d\n",
					    i);
#endif
				}
			}
		}
		via2_reg(rIFR) = 0x80 | V2IF_SLOTINT;
	}
	return (rv);
}

static int
rbv_slot_ignore(void *client_data)
{
	int slot = 0 + 9;
	int mask = (1 << (slot - 9));

	if (VIA2 == VIA2OFF) {
		via2_reg(vDirA) |= mask;
		via2_reg(vBufA) = mask;
		via2_reg(vDirA) &= ~mask;
	} else
		via2_reg(rBufA) = mask;

	return (1);
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
via1_register_irq(int irq, int (*irq_func)(void *), void *client_data,
    const char *name)
{
	struct intrhand *ih;

#ifdef DIAGNOSTIC
	if (irq < 0 || irq > 7)
		panic("via1_register_irq: bad irq %d", irq);
#endif

	ih = &via1intrs[irq];

	/*
	 * VIA1_T1 is special, since we need to temporary replace
	 * the callback during bootstrap, to compute the delay
	 * values.
	 * To avoid a loop in evcount lists, only invoke
	 * evcount_attach() if name is non-NULL, and have the two
	 * replacements calls in clock.c pass a NULL pointer.
	 */
#ifdef DIAGNOSTIC
	if (ih->ih_fn != NULL && irq != VIA1_T1)
		panic("via1_register_irq: attempt to share irq %d", irq);
#endif

	ih->ih_fn = irq_func;
	ih->ih_arg = client_data;
	ih->ih_ipl = irq;
	if (name != NULL || irq != VIA1_T1)
		evcount_attach(&ih->ih_count, name, (void *)&ih->ih_ipl,
		    &evcount_intr);
}

void
via2_register_irq(int irq, int (*irq_func)(void *), void *client_data,
    const char *name)
{
	struct intrhand *ih;

#ifdef DIAGNOSTIC
	if (irq < 0 || irq > 7)
		panic("via2_register_irq: bad irq %d", irq);
#endif

	ih = &via2intrs[irq];

#ifdef DIAGNOSTIC
	if (ih->ih_fn != NULL)
		panic("via2_register_irq: attempt to share irq %d", irq);
#endif

	ih->ih_fn = irq_func;
	ih->ih_arg = client_data;
	ih->ih_ipl = irq;
	if (name != NULL)
		evcount_attach(&ih->ih_count, name, (void *)&ih->ih_ipl,
		    &evcount_intr);
}
