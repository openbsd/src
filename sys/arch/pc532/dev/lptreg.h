/*	$NetBSD: lptreg.h,v 1.3 1995/05/16 07:30:35 phil Exp $	*/

/*
 * Copyright (c) 1994 Matthias Pfaller.
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
 *	This product includes software developed by Matthias Pfaller.
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
 *
 *	lptreg.h: definitions for the lpt driver.
 *
 */

struct i8255 {
	unsigned char port_a;		/* Port A data  (r/w) */
	unsigned char port_b;		/* Port B data  (r/w) */
	unsigned char port_c;		/* Port C data  (r/w) */
	unsigned char port_control;	/* Port control (-/w) */
};

/* port_a */
#define LPA_ALF		0x01
#define LPA_SELECT	0x02
#define LPA_NPRIME	0x04
#define	LPA_ACKENABLE	0x08		/* Enable Ack interrupts */
#define LPA_ACTIVE	0x10		/* Device active led */

/* port_c */
#define LPC_IRQ		0x01
#define LPC_NSTROBE	0x02
#define LPC_NBUSY	0x08		/* Negative logic! */
#define LPC_NERROR	0x10
#define LPC_ONLINE	0x20
#define LPC_NOPAPER	0x40
#define LPC_NACK	0x80

/* port_control */
#define LPT_PROBE_MODE	0x8c
#define LPT_MODE	0x8d		/* Port A: Output, Mode 0 */
					/* Port B: Output, Mode 1 */
					/* Port C: Input */
#define LPT_IRQENABLE	0x05		/* Enable LPT interrupts */
#define LPT_IRQDISABLE	0x04		/* Disable LPT interrupts */

#define LPT_PROBE_MASK	0x08
#define LPT_PROBE_SET	0x07
#define LPT_PROBE_CLR	0x06

					/* Default mapped address */
#define LPT_ADR(unit) (((volatile struct i8255 *)0xFFC80030) + unit)
					/* Default interrupts */
#define LPT_IRQ(unit) (unit?IR_TTY3 - 1:IR_TTY3)
#define LPT_MAX	2			/* Maximum number of lpt interfaces*/
