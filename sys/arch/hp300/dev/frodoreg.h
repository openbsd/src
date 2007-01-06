/*	$OpenBSD: frodoreg.h,v 1.2 2007/01/06 20:17:43 miod Exp $	*/
/*	$NetBSD: frodoreg.h,v 1.1 1997/05/12 08:03:49 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Michael Smith.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Base address of the Frodo part */
#define	FRODO_BASE		(INTIOBASE + 0x1c000)

/*
 * Where we find the 8250-like APCI ports, and how far apart they are.
 */
#define	FRODO_APCIBASE		0x0
#define	FRODO_APCISPACE		0x20
#define	FRODO_APCI_OFFSET(x)	(FRODO_APCIBASE + ((x) * FRODO_APCISPACE))

/*
 * Other items in the Frodo part
 */

/* An mc146818-like calendar, but no battery... lame */
#define	FRODO_CALENDAR		0x80

#define	FRODO_TIMER		0xa0	/* 8254-like timer */
#define	FRODO_T1_CTR		0xa0	/* counter 1 */
#define	FRODO_T2_CTR		0xa4	/* counter 2 */
#define	FRODO_T3_CTR		0xa8	/* counter 3 */
#define	FRODO_T_CTRL		0xac	/* control register */
#define	FRODO_T_PSCALE		0xb0	/* prescaler */
#define	FRODO_T_PCOUNT		0xb4	/* precounter ? */
#define	FRODO_T_OVCOUNT		0xb8	/* overflow counter (0, 1, 2) */

#define	FRODO_PIO		0xc0	/* programmable i/o registers start
					   here */
#define	FRODO_IISR		0xc0	/* ISA Interrupt Status Register
					   (also PIR) */
#define	FRODO_IISR_SERVICE	(1<<0)	/* service switch "on" if 0 */
#define	FRODO_IISR_ILOW		(1<<1)	/* IRQ 3,4,5 or 6 on ISA if 0 */
#define	FRODO_IISR_IMID		(1<<2)	/* IRQ 7,9,10 or 11 on ISA if 0 */
#define	FRODO_IISR_IHI		(1<<3)	/* IRQ 12,13,14 or 15 on ISA if 0 */
		/* bits 4 and 5 are DN2500 SCSI interrupts */
		/* bit 6 is unused */
#define	FRODO_IISR_IOCHK	(1<<7)	/* ISA board asserted IOCHK if low */

#define	FRODO_PIO_IPR		0xc4	/* input polarity register
					   (ints 7->0) */

#define	FRODO_PIO_IELR		0xc8	/* input edge/level register */
#define	FRODO_PIO_ISA_CONTROL	0xcc	/* ISA interrupts masking */

/* This is probably not used on the 4xx */
#define	FRODO_DIAGCTL		0xd0	/* Diagnostic Control Register */

#define	FRODO_PIC_MU		0xe0	/* upper Interrupt Mask register */
#define	FRODO_PIC_ML		0xe4	/* lower Interrupt Mask register */
#define	FRODO_PIC_PU		0xe8	/* upper Interrupt Pending register */
#define	FRODO_PIC_PL		0xec	/* lower Interrupt Pending register */
#define	FRODO_PIC_IVR		0xf8	/* Interrupt Vector register */
#define	FRODO_PIC_ACK		0xf8	/* Interrupt Acknowledge */

/* Shorthand for register access. */
#define	FRODO_READ(sc, reg)		((sc)->sc_regs[(reg)])
#define	FRODO_WRITE(sc, reg, val)	(sc)->sc_regs[(reg)] = (val)

/* manipulate interrupt registers */
#define	FRODO_GETMASK(sc)						\
	((FRODO_READ((sc), FRODO_PIC_MU) << 8) | 			\
	    FRODO_READ((sc), FRODO_PIC_ML))
#define	FRODO_SETMASK(sc, val) do {					\
	FRODO_WRITE((sc), FRODO_PIC_MU, ((val) >> 8) & 0xff);		\
	FRODO_WRITE((sc), FRODO_PIC_ML, (val) & 0xff); } while (0)

#define	FRODO_GETPEND(sc)						\
	((FRODO_READ((sc), FRODO_PIC_PU) << 8) |			\
	    FRODO_READ((sc), FRODO_PIC_PL))

/*
 * Interrupt lines.  Use FRODO_INTR_BIT() below to get a bit
 * suitable for one of the interrupt mask registers.  Yes, line
 * 0 is unused.
 */
#define	FRODO_INTR_ILOW		1
#define	FRODO_INTR_IMID		2
#define	FRODO_INTR_IHI		3
#define	FRODO_INTR_SCSIDMA	4	/* DN2500 only */
#define	FRODO_INTR_SCSI		5	/* DN2500 only */
#define	FRODO_INTR_HORIZ	6
#define	FRODO_INTR_IOCHK	7
#define	FRODO_INTR_CALENDAR	8
#define	FRODO_INTR_TIMER0	9
#define	FRODO_INTR_TIMER1	10
#define	FRODO_INTR_TIMER2	11
#define	FRODO_INTR_APCI0	12
#define	FRODO_INTR_APCI1	13
#define	FRODO_INTR_APCI2	14
#define	FRODO_INTR_APCI3	15

#define	FRODO_NINTR		16

#define	FRODO_INTR_ISA(l)	((l) != 0 && (l) <= FRODO_INTR_IHI)
#define	FRODO_INTR_BIT(line)	(1 << (line))
