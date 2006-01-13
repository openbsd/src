/*	$OpenBSD: via.c,v 1.26 2006/01/13 21:02:04 miod Exp $	*/
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
 *	This code handles both the VIA, RBV and OSS functionality.
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

int	adb_intr(void *);
int	pm_intr(void *);
int	rtclock_intr(void *);
void	profclock(void *);

int	via1_intr(void *);
int	via2_intr(void *);
int	rbv_intr(void *);
int	oss_intr(void *);
int	via2_nubus_intr(void *);
int	rbv_nubus_intr(void *);

static	int slot_ignore(void *);

int	VIA2 = VIA2OFF;		/* default for II, IIx, IIcx, SE/30. */

struct intrhand via1intrs[7];
via2hand_t via2intrs[7];

/*
 * Nubus slot interrupt routines and parameters for slots 9-15.  Note
 * that for simplicity of code, "v2IRQ0" for internal video is treated
 * as a slot 15 interrupt; this slot is quite fictitious in real-world
 * Macs.  See also GMFH, pp. 165-167, and "Monster, Loch Ness."
 */
struct intrhand slotintrs[7];

static struct via2hand nubus_intr;

void
via_init()
{
	unsigned int i;

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

	intr_establish(via1_intr, NULL, mac68k_machine.via1_ipl, "via1");

	/* register default VIA1 interrupts */
	via1_register_irq(2, adb_intr, NULL, "adb");
	via1_register_irq(4, pm_intr, NULL, "pm");
	via1_register_irq(VIA1_T1, rtclock_intr, NULL, "clock");

	for (i = 0; i < 7; i++)
		SLIST_INIT(&via2intrs[i]);

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
		nubus_intr.vh_ipl = 1;
		nubus_intr.vh_fn = via2_nubus_intr;
		via2_register_irq(&nubus_intr, NULL);
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

		intr_establish(via2_intr, NULL, mac68k_machine.via2_ipl,
		    "via2");
	} else if (current_mac_model->class == MACH_CLASSIIfx) { /* OSS */
		volatile u_char *ossintr;
		ossintr = (volatile u_char *)IOBase + 0x1a006;
		*ossintr = 0;
		intr_establish(oss_intr, NULL, mac68k_machine.via2_ipl,
		    "via2");
	} else {	/* RBV */
		if (current_mac_model->class == MACH_CLASSIIci) {
			/*
			 * Disable cache card. (p. 174--GtMFH)
			 */
			via2_reg(rBufB) |= DB2O_CEnable;
		}
		intr_establish(rbv_intr, NULL, mac68k_machine.via2_ipl,
		    "via2");

		nubus_intr.vh_ipl = 1;
		nubus_intr.vh_fn = rbv_nubus_intr;
		via2_register_irq(&nubus_intr, NULL);
		/* XXX necessary? */
		add_nubus_intr(0, slot_ignore, (void *)0, "dummy");
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

int
via1_intr(void *arg)
{
	struct intrhand *ih;
	u_int8_t intbits, bitnum;
	u_int mask;

	intbits = via_reg(VIA1, vIFR);		/* get interrupts pending */
	intbits &= via_reg(VIA1, vIER);		/* only care about enabled */

	if (intbits == 0)
		return (0);

	/*
	 * Unflag interrupts here.  If we do it after each interrupt,
	 * the MRG ADB hangs up.
	 */
	via_reg(VIA1, vIFR) = intbits;

	intbits &= 0x7f;
	mask = 1;
	for (bitnum = 0, ih = via1intrs; ; bitnum++, ih++) {
		if ((intbits & mask) != 0 && ih->ih_fn != NULL)
			if ((*ih->ih_fn)(ih->ih_arg) != 0)
				ih->ih_count.ec_count++;
		mask <<= 1;
		if (intbits < mask)
			break;
	}

	return (1);
}

int
via2_intr(void *arg)
{
	struct via2hand *v2h;
	via2hand_t *anchor;
	u_int8_t intbits, bitnum;
	u_int mask;
	int handled, rc;

	intbits = via2_reg(vIFR);		/* get interrupts pending */
	intbits &= via2_reg(vIER);		/* only care about enabled */

	if (intbits == 0)
		return (0);

	via2_reg(vIFR) = intbits;

	intbits &= 0x7f;
	mask = 1;
	for (bitnum = 0, anchor = via2intrs; ; bitnum++, anchor++) {
		if ((intbits & mask) != 0) {
			handled = 0;
			SLIST_FOREACH(v2h, anchor, v2h_link) {
				struct intrhand *ih = &v2h->v2h_ih;
				rc = (*ih->ih_fn)(ih->ih_arg);
				if (rc != 0) {
					ih->ih_count.ec_count++;
					handled |= rc;
				}
			}
		}
		mask <<= 1;
		if (intbits < mask)
			break;
	}

	return (1);
}

int
rbv_intr(void *arg)
{
	struct via2hand *v2h;
	via2hand_t *anchor;
	u_int8_t intbits, bitnum;
	u_int mask;
	int handled, rc;

	intbits = via2_reg(vIFR + rIFR);
	intbits &= via2_reg(vIER + rIER);

	if (intbits == 0)
		return (0);

	via2_reg(rIFR) = intbits;

	intbits &= 0x7f;
	mask = 1;
	for (bitnum = 0, anchor = via2intrs; ; bitnum++, anchor++) {
		if ((intbits & mask) != 0) {
			handled = 0;
			SLIST_FOREACH(v2h, anchor, v2h_link) {
				struct intrhand *ih = &v2h->v2h_ih;
				rc = (*ih->ih_fn)(ih->ih_arg);
				if (rc != 0) {
					ih->ih_count.ec_count++;
					handled |= rc;
				}
			}
		}
		mask <<= 1;
		if (intbits < mask)
			break;
	}

	return (1);
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

int
oss_intr(void *arg)
{
	struct intrhand *ih;
	u_int8_t intbits, bitnum;
	u_int mask;

	intbits = via2_reg(vIFR + rIFR);

	if (intbits == 0)
		return (0);

	intbits &= 0x7f;
	mask = 1;
	for (bitnum = 0, ih = slotintrs; ; bitnum++, ih++) {
		if (intbits & mask) {
			if (ih->ih_fn != NULL) {
				if ((*ih->ih_fn)(ih->ih_arg) != 0)
					ih->ih_count.ec_count++;
			}
			via2_reg(rIFR) = mask;
		}
		mask <<= 1;
		if (intbits < mask)
			break;
	}

	return (1);
}

/*ARGSUSED*/
int
via2_nubus_intr(void *bitarg)
{
	struct intrhand *ih;
	u_int8_t i, intbits, mask;
	int rv = 0;

	via2_reg(vIFR) = V2IF_SLOTINT;
	while ((intbits = (~via2_reg(vBufA)) & nubus_intr_mask)) {
		for (i = 6, ih = &slotintrs[i], mask = 1 << i; mask != 0;
		    i--, ih--, mask >>= 1) {
			if (intbits & mask) {
				if (ih->ih_fn != NULL) {
					if ((*ih->ih_fn)(ih->ih_arg) != 0) {
						ih->ih_count.ec_count++;
						rv = 1;
					}
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
				}
			}
		}
		via2_reg(rIFR) = 0x80 | V2IF_SLOTINT;
	}
	return (rv);
}

static int
slot_ignore(void *client_data)
{
	int mask = (1 << (int)client_data);

	if (VIA2 == VIA2OFF) {
		via2_reg(vDirA) |= mask;
		via2_reg(vBufA) = mask;
		via2_reg(vDirA) &= ~mask;
	} else
		via2_reg(rBufA) = mask;

	return (1);
}

void
via_powerdown()
{
	if (VIA2 == VIA2OFF) {
		via2_reg(vDirB) |= 0x04;  /* Set write for bit 2 */
		via2_reg(vBufB) &= ~0x04; /* Shut down */
	} else if (VIA2 == RBVOFF) {
		via2_reg(rBufB) &= ~0x04;
	} else if (VIA2 == OSSOFF) {
		/*
		 * Thanks to Brad Boyer <flar@cegt201.bradley.edu> for the
		 * Linux/mac68k code that I derived this from.
		 */
		via2_reg(OSS_oRCR) |= OSS_POWEROFF;
	}
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
	 * VIA1 interrupts are special, since we start with temporary handlers,
	 * and later switch to better routines whenever possible.
	 * To avoid a loop in evcount lists, only invoke evcount_attach() if
	 * name is non-NULL, and have the replacements calls in adb_direct.c,
	 * clock.c and pm_direct.c pass a NULL pointer.
	 */
#ifdef DIAGNOSTIC
	if (ih->ih_fn != NULL && name != NULL)
		panic("via1_register_irq: improper invocation");
#endif

	ih->ih_fn = irq_func;
	ih->ih_arg = client_data;
	ih->ih_ipl = irq;
	if (name != NULL)
		evcount_attach(&ih->ih_count, name, (void *)&ih->ih_ipl,
		    &evcount_intr);
}

int
via2_register_irq(struct via2hand *vh, const char *name)
{
	int irq = vh->vh_ipl;

#ifdef DIAGNOSTIC
	if (irq < 0 || irq > 7)
		panic("via2_register_irq: bad irq %d", irq);
#endif

	if (name != NULL)
		evcount_attach(&vh->vh_count, name, (void *)&vh->vh_ipl,
		    &evcount_intr);
	SLIST_INSERT_HEAD(&via2intrs[irq], vh, v2h_link);
	return (0);
}
