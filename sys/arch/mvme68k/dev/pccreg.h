/*	$OpenBSD: pccreg.h,v 1.7 2004/07/30 22:29:45 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 */

/*
 * MVME147 PCC chip
 */
struct pccreg {
	volatile u_long		pcc_dmataddr;	/* dma table address */
	volatile u_long		pcc_dmadaddr;	/* dma data address */
	volatile u_long		pcc_dmabcnt;	/* dma byte count */
	volatile u_long		pcc_dmahold;	/* dma data hold register */
	volatile u_short	pcc_t1pload;	/* timer1 preload */
	volatile u_short	pcc_t1count;	/* timer1 count */
	volatile u_short	pcc_t2pload;	/* timer2 preload */
	volatile u_short	pcc_t2count;	/* timer2 count */
	volatile u_char		pcc_t1irq;	/* timer1 interrupt ctrl */
	volatile u_char		pcc_t1ctl;	/* timer1 ctrl reg */
	volatile u_char		pcc_t2irq;	/* timer2 interrupt ctrl */
	volatile u_char		pcc_t2ctl;	/* timer2 ctrl reg */
	volatile u_char		pcc_acfirq;	/* acfail intr reg */
	volatile u_char		pcc_dogirq;	/* watchdog intr reg */
	volatile u_char		pcc_lpirq;	/* printer intr reg */
	volatile u_char		pcc_lpctl;	/* printer ctrl */
	volatile u_char		pcc_dmairq;	/* dma interrupt control */
	volatile u_char		pcc_dmacsr;	/* dma csr */
	volatile u_char		pcc_busirq;	/* bus error interrupt */
	volatile u_char		pcc_dmasr;	/* dma status register */
	volatile u_char		pcc_abortirq;	/* abort interrupt control reg */
	volatile u_char		pcc_tafcr;	/* table address function code reg */
	volatile u_char		pcc_zsirq;	/* serial interrupt reg */
	volatile u_char		pcc_genctl;	/* general control register */
	volatile u_char		pcc_leirq;	/* ethernet interrupt */
	volatile u_char		pcc_gensr;	/* general status */
	volatile u_char		pcc_sbicirq;	/* sbic interrupt reg */
	volatile u_char		pcc_slavebase;	/* slave base addr reg */
	volatile u_char		pcc_sw1inq;	/* software interrupt #1 cr */
	volatile u_char		pcc_vecbase;	/* interrupt base vector register */
	volatile u_char		pcc_sw2irq;	/* software interrupt #2 cr */
	volatile u_char		pcc_chiprev;	/* revision level */
};
#define PCCSPACE_PCCCHIP_OFF	0x1000

/*
 * points to system's PCC. This is not active until the pcc0 device
 * has been attached.
 */
extern struct pccreg *sys_pcc;

/*
 * We lock off our interrupt vector at 0x40.
 */
#define PCC_VECBASE	0x40
#define PCC_NVEC	12

/*
 * Vectors we use
 */
#define PCCV_ACFAIL	0
#define PCCV_BERR	1
#define PCCV_ABORT	2
#define PCCV_ZS		3
#define PCCV_LE		4
#define PCCV_SBIC	5
#define PCCV_DMA	6
#define PCCV_PRINTER	7
#define PCCV_TIMER1	8
#define PCCV_TIMER2	9
#define PCCV_SOFT1	10
#define PCCV_SOFT2	11

#define PCC_DMABCNT_MAKEFC(fcn)	((fcn) << 24)
#define PCC_DMABCNT_FCMASK	0x07000000
#define PCC_DMABCNT_L		0x80000000
#define PCC_DMABCNT_CNTMASK	0x00ffffff

#define PCC_DMACSR_DONE		0x80
#define PCC_DMACSR_ERR8BIT	0x40
#define PCC_DMACSR_TNOT32	0x20
#define PCC_DMACSR_DMAERRDATA	0x10
#define PCC_DMACSR_DMAERRTABLE	0x08
#define PCC_DMACSR_TOSCSI	0x04
#define PCC_DMACSR_USETABLE	0x02
#define PCC_DMACSR_DEN		0x01

#define PCC_SBIC_RESETIRQ	0x40
#define PCC_SBIC_RESETABS	0x20

/*
 * Fairly standard irq register bits.
 */
#define PCC_IRQ_IPL	0x07
#define PCC_IRQ_IEN	0x08
#define PCC_IRQ_INT	0x80

#define PCC_LPIRQ_ACK	0x20

/*
 * clock/timer
 */
#define PCC_TIMERACK	0x80	/* ack intr */
#define PCC_TIMERCLEAR	0x00	/* reset and clear timer */
#define PCC_TIMERSTART	0x03	/* start timer */

#define	pcc_timer_hz2lim(hz)	(65536 - (160000/(hz)))
#define	pcc_timer_us2lim(us)	(65536 - (160000/(1000000/(us))))

/*
 * serial control
 */
#define PCC_ZS_PCCVEC	0x10	/* let PCC supply vector */

/*
 * abort switch
 */
#define PCC_ABORT_IEN	0x08	/* enable interrupt */
#define PCC_ABORT_ABS	0x40	/* current state of switch */
#define PCC_ABORT_ACK	0x80	/* intr active; or write to ack */

/*
 * for the console we need zs phys addr
 */
#define ZS0_PHYS_147	(INTIOBASE_147 + 0x3000)
#define ZS1_PHYS_147	(INTIOBASE_147 + 0x3800)

/* XXX */
int	pccintr_establish(int, struct intrhand *, const char *);
int	pccspeed(struct pccreg *);

#define PCC_GENCTL_IEN	0x10
