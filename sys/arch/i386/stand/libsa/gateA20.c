/*	$OpenBSD: gateA20.c,v 1.7 2001/08/18 02:00:49 csapuntz Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <machine/pio.h>
#include <dev/ic/i8042reg.h>
#include <dev/isa/isareg.h>

#include "libsa.h"

#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   enable clock line */


#define A20_KBD		0
#define A20_0x92	1

int gateA20kbd __P((int));

/*
 * Check for an oddball IBM_L40 machine.
 */
int
getA20type()
{
	return(A20_KBD);
}


int
gateA20kbd(on)
	int on;
{
	u_int i = 1000000;

	while (i && (inb(IO_KBD + KBSTATP) & KBS_IBF)) i--;
	if (i == 0)
		return (1);

	while (inb(IO_KBD + KBSTATP) & KBS_DIB)
		(void)inb(IO_KBD + KBDATAP);
	
	outb(IO_KBD + KBCMDP, KC_CMD_WOUT);
	while (inb(IO_KBD + KBSTATP) & KBS_IBF);

	if (on)
		outb(IO_KBD + KBDATAP, KB_A20);
	else
		outb(IO_KBD + KBDATAP, 0xcd);
	while (inb(IO_KBD + KBSTATP) & KBS_IBF);

	while (inb(IO_KBD + KBSTATP) & KBS_DIB)
		(void)inb(IO_KBD + KBDATAP);

	return (0);
}

/*
 * Gate A20 for high memory
 */
void
gateA20(on)
	int on;
{
	int data;

	if (getA20type() == A20_KBD) {
		if (!gateA20kbd(on))
			return;
	}

	/* Try to use 0x92 to turn on A20 */
	if (on) {
		data = inb(0x92);
		outb(0x92, data | 0x2);
	} else {
		data = inb(0x92);
		outb(0x92, data & ~0x2);
	}
}

