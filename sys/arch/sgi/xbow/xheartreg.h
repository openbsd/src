/*	$OpenBSD: xheartreg.h,v 1.3 2009/10/22 22:08:54 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * IP30 HEART registers
 */

/* physical address in PIU mode */
#define	HEART_PIU_BASE	0x000000000ff00000UL

#define	HEART_MODE		0x0000
#define	HEART_MEMORY_STATUS	0x0020	/* 8 32 bit registers */
#define	HEART_MEMORY_VALID		0x80000000
#define	HEART_MEMORY_SIZE_MASK		0x003f0000
#define	HEART_MEMORY_SIZE_SHIFT		16
#define	HEART_MEMORY_ADDR_MASK		0x000001ff
#define	HEART_MEMORY_ADDR_SHIFT		0
#define	HEART_MEMORY_UNIT_SHIFT		25	/* 32MB */

#define	HEART_MICROLAN		0x00b8

/*
 * Interrupt handling registers.
 * The Heart supports four different interrupt targets, although only
 * the two cpus are used in practice.
 */

#define	HEART_IMR(s)		(0x00010000 + (s) * 8)
#define	HEART_ISR_SET		0x00010020
#define	HEART_ISR_CLR		0x00010028
#define	HEART_ISR		0x00010030

/*
 * ISR bit assignments.
 */

/** Level 4 interrupt: hardware error */
#define	HEART_ISR_LVL4_MASK		0xfff8000000000000UL
#define	HEART_ISR_LVL4_MAX		63
/* Heart (widget 8) error */
#define	HEART_ISR_WID08_ERROR		63
/* CPU bus error */
#define	HEART_ISR_CPU_BUSERR(c)		(59 + (c))
/* Crossbow (widget 0) error */
#define	HEART_ISR_WID00_ERROR		58
/* Widget error */
#define	HEART_ISR_WID0F_ERROR		57
#define	HEART_ISR_WID0E_ERROR		56
#define	HEART_ISR_WID0D_ERROR		55
#define	HEART_ISR_WID0C_ERROR		54
#define	HEART_ISR_WID0B_ERROR		53
#define	HEART_ISR_WID0A_ERROR		52
#define	HEART_ISR_WID09_ERROR		51

#define	HEART_ISR_WID_ERROR(w)	\
	((w) == 0 ? HEART_ISR_WID00_ERROR : \
	 (w) == 8 ? HEART_ISR_WID08_ERROR : HEART_ISR_WID09_ERROR + (w) - 9)

/** Level 3 interrupt: heart counter/timer */
#define	HEART_ISR_LVL3_MASK		0x0004000000000000UL
#define	HEART_ISR_LVL3_MAX		50
/* Crossbow clock */
#define	HEART_ISR_HEARTCLOCK		50

/** Level 2 interrupt */
#define	HEART_ISR_LVL2_MASK		0x0003ffff00000000UL
#define	HEART_ISR_LVL2_MAX		49
/* IPI */
#define	HEART_ISR_IPI(c)		(46 + (c))
/* Debugger interrupts */
#define	HEART_ISR_DBG(c)		(42 + (c))
/* Power switch */
#define	HEART_ISR_POWER			41
/* 40-32 freely available */

/** Level 1 interrupt */
#define	HEART_ISR_LVL1_MASK		0x00000000ffff0000UL
#define	HEART_ISR_LVL1_MAX		31
/* 31-16 freely available */

/** Level 0 interrupt */
#define	HEART_ISR_LVL0_MASK		0x000000000000ffffUL
#define	HEART_ISR_LVL0_MAX		15
/* 15-3 freely available */

#define	HEART_INTR_WIDGET_MAX		15
#define	HEART_INTR_WIDGET_MIN		3

#define	HEART_NINTS			64
