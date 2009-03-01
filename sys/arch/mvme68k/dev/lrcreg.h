/*	$OpenBSD: lrcreg.h,v 1.1 2009/03/01 21:40:49 miod Exp $	*/

/*
 * Copyright (c) 2006, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * LRC Local Resource Controller, found on MVME165
 */

/*
 * Register layout.
 * Note that this chip is 24-bit internally, so the 8 high-order bits of
 * each register are undefined and likely set to ones.
 */
struct lrcreg {
	/* timer constants */
	volatile u_int32_t	lrc_t0base;
	volatile u_int32_t	lrc_t1base;
	volatile u_int32_t	lrc_t2base;
	/* timer counts */
	volatile u_int32_t	lrc_t0cnt;
	volatile u_int32_t	lrc_t1cnt;
	volatile u_int32_t	lrc_t2cnt;
	/* timer control words */
	volatile u_int32_t	lrc_tcr0;
	volatile u_int32_t	lrc_tcr1;
	volatile u_int32_t	lrc_tcr2;
	/* general control */
	volatile u_int32_t	lrc_gcr;
	/* bus control */
	volatile u_int32_t	lrc_bcr;
	/* interrupt control */
	volatile u_int32_t	lrc_icr0;
	volatile u_int32_t	lrc_icr1;
	/* status register */
	volatile u_int32_t	lrc_stat;
	/* RAM address */
	volatile u_int32_t	lrc_ramaddr;
	/* bus error status */
	volatile u_int32_t	lrc_berrstat;
};

/* TCR0 bits */
#define	TCR_TEN0	0x80	/* timer0 enable */
#define	TCR_TLD0	0x40	/* timer0 reload */
#define	TCR_TCYC0	0x20	/* timer0 cycle */
#define	TCR_T0IE	0x10	/* time0 interrupt enable */
#define	TCR_TOS_MASK	0x0c	/* timer output select */
#define	TCR_TOS_TMR0	0x00
#define	TCR_TOS_TMR1	0x04
#define	TCR_TOS_TMR2	0x08
#define	TCR_TOEN	0x02	/* timer output enable */
#define	TCR_TOMD	0x01	/* timer output mode: pulse / square wave */

/* TCR1 bits */
#define	TCR_TEN1	0x80	/* timer1 enable */
#define	TCR_TLD1	0x40	/* timer1 reload */
#define	TCR_TCYC1	0x20	/* timer1 cycle */
#define	TCR_T1IE	0x10	/* timer1 interrupt enable */
#define	TCR_TIS_MASK	0x0c	/* timer input select */
#define	TCR_TIS_TMR0	0x00
#define	TCR_TIS_TMR1	0x04
#define	TCR_TIS_TMR2	0x08
#define	TCR_TIS_NONE	0x0c
#define	TCR_TIEN	0x02	/* timer input enable */
#define	TCR_TIMD	0x01	/* timer input mode */

/* TCR2 bits */
#define	TCR_TEN2	0x80	/* timer2 enable */
#define	TCR_TLD2	0x40	/* timer2 reload */
#define	TCR_TCYC2	0x20	/* timer2 cycle */
#define	TCR_T2IE	0x10	/* timer2 interrupt enable */
#define	TCR_TWD_MASK	0x0c	/* timer watchdog select */
#define	TCR_TWD_TMR0	0x00
#define	TCR_TWD_TMR1	0x04
#define	TCR_TWD_TMR2	0x08
#define	TCR_TWD_NONE	0x0c
#define	TCR_TWDEN	0x02	/* timer watchdog enable */
#define	TCR_TTST	0x01	/* timer test (do not use) */

/* GCR bits */
#define	GCR_SCFREQ	0x10	/* dart serial clock frequency */
#define	GCR_FAST	0x08	/* EPROM fast acknowledge */
#define	GCR_GPCTL1	0x04	/* write wrong parity */
#define	GCR_GPCTL0	0x02	/* parity error enable */
#define	GCR_RSTO	0x01	/* reset output */

/* BCR bits */
#define	BCR_VA24	0x10	/* force VME A24 addressing for low 16MB */
#define	BCR_VSBEN	0x08	/* VSB enable */
#define	BCR_VSBMD	0x04	/* restricted VSB ranges */
#define	BCR_ROEN	0x02	/* VSB read only enable */
#define	BCR_ROMD	0x01	/* VSB read only mode uses VSBRO */

/* ICR0 bits */
#define	ICR0_GIE	0x80	/* global interrupt enable */
#define	ICR0_IRQ3G2IE	0x40	/* DART tick timer, level 3 */
#define	ICR0_IRQ4G2IE	0x20	/* VSBIRQ, level 4 */
#define	ICR0_IRQ5G2IE	0x10	/* DART DUART irq */
#define	ICR0_IRQ6G2IE	0x08	/* DART tick timer, level 6 */
#define	ICR0_IRQ6G4IE	0x04	/* VSBIRQ, level 6 */
#define	ICR0_IRQ7G2IE	0x02	/* unused */
#define	ICR0_IRQ7G4IE	0x01	/* PARERR */

/* ICR1 bits */
#define	ICR1_VEC_MASK	0xf0	/* interrupt vector significant nibble */
#define	ICR1_IRQ7G6IE	0x02	/* ACFAIL */
#define	ICR1_IRQ7G5IE	0x01	/* ABORT */

/* STAT bits */
#define	STAT_WTS	0x020000	/* Watchdog timeout */
#define	STAT_IRQ7G6S	0x010000	/* ACFAIL */
#define	STAT_IRQ7G5S	0x008000	/* ABORT */
#define	STAT_GPSTATL	0x004000	/* SYSFAIL (latched) */
#define	STAT_GPSTATS	0x002000	/* SYSFAIL */
#define	STAT_PORS	0x001000	/* Power On Reset */
#define	STAT_IRQ3G2	0x000800	/* DART tick timer */
#define	STAT_IRQ4G2	0x000400	/* VSBIRQ */
#define	STAT_IRQ5G2	0x000200	/* DART irq */
#define	STAT_IRQ6G2	0x000100	/* DART tick timer */
#define	STAT_IRQ7G2	0x000080	/* unused */
#define	STAT_TMR2	0x000040	/* timer2 timeout */
#define	STAT_TMR1	0x000020	/* timer1 timeout */
#define	STAT_TMR0	0x000010	/* timer0 timeout */
#define	STAT_IRQ6G4	0x000008	/* VSBIRQ */
#define	STAT_IRQ7G4	0x000004	/* PARERR */
#define	STAT_IRQ7G5	0x000002	/* ABORT */
#define	STAT_IRQ7G6	0x000001	/* ACFAIL */

/*
 * Fixed interrupt vectors
 */

#define	LRC_VECBASE	0x40
#define	LRC_NVEC	0x10

#define	LRCVEC_TIMER0	0x00
#define	LRCVEC_TIMER1	0x01
#define	LRCVEC_TIMER2	0x02
#define	LRCVEC_IRQ7G2	0x04	/* not used */
#define	LRCVEC_IRQ6G2	0x05
#define	LRCVEC_IRQ3G2	0x07
#define	LRCVEC_PARERR	0x08
#define	LRCVEC_VSBIRQ	0x09
#define	LRCVEC_ABORT	0x0a
#define	LRCVEC_ACFAIL	0x0b

#define	LRCVEC_DART	0x0c	/* as set up by the BUG; not a fixed value */

int	lrcintr_establish(u_int, struct intrhand *, const char *);
int	lrcspeed(struct lrcreg *);

extern struct lrcreg *sys_lrc;
