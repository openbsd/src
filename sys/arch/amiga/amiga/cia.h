/*	$NetBSD: cia.h,v 1.8 1995/03/28 18:14:28 jtc Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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

/*
 * This is a rewrite (retype) of the Amiga's CIA chip register map, based
 * on the Hardware Reference Manual.  It is NOT based on the Amiga's
 *  hardware/cia.h.  
 */

#ifndef _AMIGA_CIA_
#define _AMIGA_CIA_

struct CIA {
	volatile unsigned char pra;          char pad0[0xff];
	volatile unsigned char prb;          char pad1[0xff];
	volatile unsigned char ddra;         char pad2[0xff];
	volatile unsigned char ddrb;         char pad3[0xff];
	volatile unsigned char talo;         char pad4[0xff];
	volatile unsigned char tahi;         char pad5[0xff];
	volatile unsigned char tblo;         char pad6[0xff];
	volatile unsigned char tbhi;         char pad7[0xff];
	volatile unsigned char todlo;        char pad8[0xff];
	volatile unsigned char todmid;       char pad9[0xff];
	volatile unsigned char todhi;        char pada[0x1ff];
	volatile unsigned char sdr;          char padc[0xff];
	volatile unsigned char icr;          char padd[0xff];
	volatile unsigned char cra;          char pade[0xff];
	volatile unsigned char crb;          char padf[0xff];
};

#ifdef _KERNEL
#ifndef LOCORE
vm_offset_t CIAAbase, CIABbase, CIAADDR;
#define CIABASE		(0x00BFC000)
#define CIATOP		(0x00C00000)
#define NCIAPG		btoc(CIATOP - CIABASE)
#endif

#define ciaa (*((volatile struct CIA *)CIAAbase))
#define ciab (*((volatile struct CIA *)CIABbase))
#endif

/*
 * bits in CIA-B
 */
#define CIAB_PRA_BUSY	(1<<0)
#define CIAB_PRA_POUT	(1<<1)
#define CIAB_PRA_SEL	(1<<2)
#define CIAB_PRA_DSR	(1<<3)
#define CIAB_PRA_CTS	(1<<4)
#define CIAB_PRA_CD	(1<<5)
#define CIAB_PRA_RTS	(1<<6)
#define CIAB_PRA_DTR	(1<<7)

#define CIAB_PRB_STEP	(1<<0)
#define CIAB_PRB_DIR	(1<<1)
#define CIAB_PRB_SIDE	(1<<2)
#define CIAB_PRB_SEL0	(1<<3)
#define CIAB_PRB_SEL1	(1<<4)
#define CIAB_PRB_SEL2	(1<<5)
#define CIAB_PRB_SEL3	(1<<6)
#define CIAB_PRB_MTR	(1<<7)

/*
 * bits in CIA-A
 */
#define CIAA_PRA_OVL	(1<<0)
#define CIAA_PRA_LED	(1<<1)
#define CIAA_PRA_CHNG	(1<<2)
#define CIAA_PRA_WPRO	(1<<3)
#define CIAA_PRA_TK0	(1<<4)
#define CIAA_PRA_RDY	(1<<5)
#define CIAA_PRA_FIR0	(1<<6)
#define CIAA_PRA_FIR1	(1<<7)

/*
 * ciaa-prb is centronics interface
 */


/*
 * interrupt bits
 */
#define CIA_ICR_TA	(1<<0)
#define CIA_ICR_TB	(1<<1)
#define CIA_ICR_ALARM	(1<<2)
#define CIA_ICR_SP	(1<<3)
#define CIA_ICR_FLG	(1<<4)
#define CIA_ICR_IR_SC	(1<<7)


/*
 * since many CIA signals are low-active, these defines should make the
 * code more readable
 */
#define SETDCD(c) (c &= ~CIAB_PRA_CD)
#define CLRDCD(c) (c |= CIAB_PRA_CD)
#define ISDCD(c)  (!(c & CIAB_PRA_CD))

#define SETCTS(c) (c &= ~CIAB_PRA_CTS)
#define CLRCTS(c) (c |= CIAB_PRA_CTS)
#define ISCTS(c)  (!(c & CIAB_PRA_CTS))

#define SETRTS(c) (c &= ~CIAB_PRA_RTS)
#define CLRRTS(c) (c |= CIAB_PRA_RTS)
#define ISRTS(c)  (!(c & CIAB_PRA_RTS))

#define SETDTR(c) (c &= ~CIAB_PRA_DTR)
#define CLRDTR(c) (c |= CIAB_PRA_DTR)
#define ISDTR(c)  (!(c & CIAB_PRA_DTR))

#define SETDSR(c) (c &= ~CIAB_PRA_DSR)
#define CLRDSR(c) (c |= CIAB_PRA_DSR)
#define ISDSR(c)  (!(c & CIAB_PRA_DSR))

void dispatch_cia_ints __P((int, int));
void ciaa_intr __P((void));
void ciab_intr __P((void));

#endif _AMIGA_CIA_
