/*	$OpenBSD: gateA20.c,v 1.4 1997/07/17 23:00:26 mickey Exp $	*/

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

#include <sys/types.h>
#include <machine/pio.h>
#include <lib/libsa/stand.h>
#include <dev/ic/i8042reg.h>

#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   enable clock line */

/*
 * Gate A20 for high memory
 */
void
gateA20(on)
	int on;
{
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	IBM_L40
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
#endif	IBM_L40
}
