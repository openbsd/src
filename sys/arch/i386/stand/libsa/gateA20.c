/*	$OpenBSD: gateA20.c,v 1.5 1998/02/24 22:06:51 weingart Exp $	*/

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

#include "libsa.h"

#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   enable clock line */


#define A20_KBD		0
#define A20_0x92	1

/*
 * Check for an oddball IBM_L40 machine.
 */
int
getA20type()
{
	return(A20_KBD);
}


/*
 * Gate A20 for high memory
 */
void
gateA20(on)
	int on;
{

	if (getA20type() == A20_0x92) {
		int data;

		/* Try to use 0x92 to turn on A20 */
		if (on) {
			data = inb(0x92);
			outb(0x92, data | 0x2);
		} else {
			data = inb(0x92);
			outb(0x92, data & ~0x2);
		}
	} else {

		/* XXX - These whiles might need to be changed to bounded for loops */
		while (inb(KBSTATP) & KBS_IBF);

		while (inb(KBSTATP) & KBS_DIB)
			(void)inb(KBDATAP);

		outb(KBCMDP, KC_CMD_WOUT);
		while (inb(KBSTATP) & KBS_IBF);

		if (on)
			outb(KBDATAP, KB_A20);
		else
			outb(KBDATAP, 0xcd);
		while (inb(KBSTATP) & KBS_IBF);

		while (inb(KBSTATP) & KBS_DIB)
			(void)inb(KBDATAP);
	}
}

