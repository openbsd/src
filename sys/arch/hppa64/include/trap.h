/*	$OpenBSD: trap.h,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_MACHINE_TRAP_H_
#define	_MACHINE_TRAP_H_

/*
 * This is PA-RISC trap types per 1.1 specs, see .c files for references.
 */
#define	T_NONEXIST	0	/* invalid interrupt vector */
#define	T_HPMC		1	/* high priority machine check */
#define	T_POWERFAIL	2	/* power failure */
#define	T_RECOVERY	3	/* recovery counter */
#define	T_INTERRUPT	4	/* external interrupt */
#define	T_LPMC		5	/* low-priority machine check */
#define	T_ITLBMISS	6	/* instruction TLB miss fault */
#define	T_IPROT		7	/* instruction protection */
#define	T_ILLEGAL	8	/* Illegal instruction */
#define	T_IBREAK	9	/* break instruction */
#define	T_PRIV_OP	10	/* privileged operation */
#define	T_PRIV_REG	11	/* privileged register */
#define	T_OVERFLOW	12	/* overflow */
#define	T_CONDITION	13	/* conditional */
#define	T_EXCEPTION	14	/* assist exception */
#define	T_DTLBMISS	15	/* data TLB miss */
#define	T_ITLBMISSNA	16	/* ITLB non-access miss */
#define	T_DTLBMISSNA	17	/* DTLB non-access miss */
#define	T_DPROT		18	/* data protection/rights/alignment <7100 */
#define	T_DBREAK	19	/* data break */
#define	T_TLB_DIRTY	20	/* TLB dirty bit */
#define	T_PAGEREF	21	/* page reference */
#define	T_EMULATION	22	/* assist emulation */
#define	T_HIGHERPL	23	/* higher-privelege transfer */
#define	T_LOWERPL	24	/* lower-privilege transfer */
#define	T_TAKENBR	25	/* taken branch */
#define	T_DATACC	26	/* data access rights >=7100 */
#define	T_DATAPID	27	/* data protection ID >=7100 */
#define	T_DATALIGN	28	/* unaligned data ref */
#define	T_PERFMON	29	/* performance monitor interrupt */
#define	T_IDEBUG	30	/* debug SFU interrupt */
#define	T_DDEBUG	31	/* debug SFU interrupt */

/*
 * Reserved range for traps is 0-63, place user flag at 6th bit
 */
#define	T_USER_POS	57
#define	T_USER		(1 << (63 - T_USER_POS))

/*
 * Various trap frame flags.
 */
#define	TFF_LAST_POS	40
#define	TFF_SYS_POS	41
#define	TFF_INTR_POS	42

#define	TFF_LAST	(1 << (63 - TFF_LAST_POS))
#define	TFF_SYS		(1 << (63 - TFF_SYS_POS))
#define	TFF_INTR	(1 << (63 - TFF_INTR_POS))

/*
 * Define this for pretty printings of trapflags.
 */
#define	T_BITS	"\020\07user\036intr\037itlb\040last"

/*
 * These are break instruction entry points.
 */
/* im5 */
#define	HPPA_BREAK_KERNEL	0
/* im13 */
#define	HPPA_BREAK_KGDB		5
#define	HPPA_BREAK_GET_PSW	9
#define	HPPA_BREAK_SET_PSW	10

#endif	/* _MACHINE_TRAP_H_ */
