/*	$NetBSD: icu.h,v 1.5 1995/08/25 07:52:18 phil Exp $	*/

/* 
 * Copyright (c) 1993 Philip A. Nelson.
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
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	icu.h
 */

/* icu.h: defines for use with the ns32532 icu. */

#ifndef _MACHINE_ICU_H_
#define _MACHINE_ICU_H_

/* We don't use vector interrupts, but make it right anyway */
#define VEC_ICU		0x10

/* The address of the ICU! */
#define ICU_ADR		0xfffffe00

/* ICU clock speed. */
#define ICU_CLK_HZ	3686400/4	/* raw ICU clock speed */

/* ICU registers
 */
#define HVCT	0
#define SVCT	1
#define ELTG	2
#define TPL	4
#define IPND	6
#define ISRV	8
#define IMSK	10
#define CSRC	12
#define FPRT	14
#define MCTL	16
#define OCASN	17
#define CIPTR	18
#define PDAT	19
#define IPS	20
#define PDIR	21
#define CCTL	22
#define CICTL	23
#define LCSV	24
#define HCSV	26
#define LCCV	28
#define HCCV	30

/* Byte and Word access to ICU registers
 */
#define ICUB(n)	*((unsigned char  *)(ICU_ADR + n))
#define ICUW(n)	*((unsigned short *)(ICU_ADR + n))

#ifndef LOCORE
/* Interrupt trigger modes
 */
enum {HIGH_LEVEL, LOW_LEVEL, RISING_EDGE, FALLING_EDGE} int_modes;
#endif /* !LOCORE */

/* Hardware interrupt request lines.
 */
#define IR_CLK		2		/* highest priority */
#define IR_SCSI0	5		/* Adaptec 6250 */
#define IR_SCSI1	4		/* NCR DP8490 */
#define IR_TTY0		13
#define IR_TTY0RDY	12
#define IR_TTY1		11
#define IR_TTY1RDY	10
#define IR_TTY2		9
#define IR_TTY2RDY	8
#define IR_TTY3		7
#define IR_TTY3RDY	6

/*    edge polarity
 *	0	0	falling edge
 *	0	1	rising edge
 *	1	0	low level
 *	1	1	high level
 *
 */
#define IEDGE		0
#define IPOLARITY	0

#define ints_off	bicpsrw	PSR_I
#define ints_on		bispsrw	PSR_I

/* SCSI controllers */
#define AIC6250		0
#define DP8490		1
#define ICU_SCSI_BIT	0x80

#ifndef LOCORE
/*
 * Select a SCSI controller.
 */
static __inline int
scsi_select_ctlr(int ctlr)
{
	int old;

	old = (ICUB(PDAT) & ICU_SCSI_BIT) == 0;
	if (ctlr == DP8490)
		ICUB(PDAT) &= ~ICU_SCSI_BIT;	/* select = 0 for 8490 */
	else
		ICUB(PDAT) |= ICU_SCSI_BIT;	/* select = 1 for AIC6250 */
	return(old);
}
#endif /* !LOCORE */
#endif /* _MACHINE_ICU_H_ */
