/*	$OpenBSD: pcctworeg.h,v 1.6 2004/07/30 22:29:45 miod Exp $ */

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
 * MVME16x PCC2 chip: sort of a confused mish-mash of the MC in the 162
 * and the PCC in the 147
 */
struct pcctworeg {
	volatile u_char		pcc2_chipid;
	volatile u_char		pcc2_chiprev;
	volatile u_char		pcc2_genctl;
	volatile u_char		pcc2_vecbase;	/* irq vector base */
	volatile u_long		pcc2_t1cmp;	/* timer1 compare */
	volatile u_long		pcc2_t1count;	/* timer1 count */
	volatile u_long		pcc2_t2cmp;	/* timer2 compare */
	volatile u_long		pcc2_t2count;	/* timer2 count */
	volatile u_char		pcc2_pscalecnt;	/* timer prescaler counter */
	volatile u_char		pcc2_pscaleadj;	/* timer prescaler adjust */
	volatile u_char		pcc2_t2ctl;	/* timer2 ctrl reg */
	volatile u_char		pcc2_t1ctl;	/* timer1 ctrl reg */
	volatile u_char		pcc2_gpioirq;	/* gpio irq */
	volatile u_char		pcc2_gpio;	/* gpio i/o */
	volatile u_char		pcc2_t2irq;
	volatile u_char		pcc2_t1irq;
	volatile u_char		pcc2_sccerr;
	volatile u_char		pcc2_sccirq;
	volatile u_char		pcc2_scctx;
	volatile u_char		pcc2_sccrx;
	volatile u_char		:8;
	volatile u_char		:8;
	volatile u_char		:8;
	volatile u_char		pcc2_sccmoiack;
	volatile u_char		:8;
	volatile u_char		pcc2_scctxiack;
	volatile u_char		:8;
	volatile u_char		pcc2_sccrxiack;
	volatile u_char		pcc2_ieerr;
	volatile u_char		:8;
	volatile u_char		pcc2_ieirq;
	volatile u_char		pcc2_iefailirq;
	volatile u_char		pcc2_ncrerr;
	volatile u_char		:8;
	volatile u_char		:8;
	volatile u_char		pcc2_ncrirq;
	volatile u_char		pcc2_prtairq;
	volatile u_char		pcc2_prtfirq;
	volatile u_char		pcc2_prtsirq;
	volatile u_char		pcc2_prtpirq;
	volatile u_char		pcc2_prtbirq;
	volatile u_char		:8;
	volatile u_char		pcc2_prtstat;
	volatile u_char		pcc2_prtctl;
	volatile u_short	pcc2_speed;	/* DO NOT USE */
	volatile u_short	pcc2_prtdat;
	volatile u_short	:16;
	volatile u_char		pcc2_ipl;
	volatile u_char		pcc2_mask;
};
#define PCC2_PCC2CHIP_OFF	0x42000
#define PCC2_CHIPID		0x20

/*
 * points to system's PCCTWO. This is not active until the pcctwo0
 * device has been attached.
 */
extern struct pcctworeg *sys_pcc2;

/*
 * We lock off our interrupt vector at 0x50.
 */
#define PCC2_VECBASE		0x50
#define PCC2_NVEC		16

/*
 * Vectors we use
 */
#define PCC2V_NCR		0x05
#define PCC2V_IEFAIL		0x06
#define PCC2V_IE		0x07
#define PCC2V_TIMER2		0x08
#define PCC2V_TIMER1		0x09
#define PCC2V_GPIO		0x0a
#define PCC2V_SCC_RXE		0x0c
#define PCC2V_SCC_M		0x0d
#define PCC2V_SCC_TX		0x0e
#define PCC2V_SCC_RX		0x0f

#define PCC2_TCTL_CEN		0x01
#define PCC2_TCTL_COC		0x02
#define PCC2_TCTL_COVF		0x04
#define PCC2_TCTL_OVF		0xf0

#define PCC2_GPIO_PLTY		0x80
#define PCC2_GPIO_EL		0x40

#define PCC2_GPIOCR_OE		0x2
#define PCC2_GPIOCR_O		0x1

#define PCC2_SCC_AVEC		0x08
#define PCC2_SCCRX_INHIBIT	(0 << 6)
#define PCC2_SCCRX_SNOOP	(1 << 6)
#define PCC2_SCCRX_INVAL	(2 << 6)
#define PCC2_SCCRX_RESV		(3 << 6)

#define pcc2_timer_us2lim(us)	(us)		/* timer increments in "us" */

#define PCC2_IRQ_IPL		0x07
#define PCC2_IRQ_ICLR		0x08
#define PCC2_IRQ_IEN		0x10
#define PCC2_IRQ_INT		0x20

#define PCC2_IEERR_SCLR		0x01

#define PCC2_GENCTL_FAST	0x01
#define PCC2_GENCTL_IEN		0x02
#define PCC2_GENCTL_C040	0x03

#define PCC2_SC_INHIBIT		(0 << 6)
#define PCC2_SC_SNOOP		(1 << 6)
#define PCC2_SC_INVAL		(2 << 6)
#define PCC2_SC_RESV		(3 << 6)

int  pcctwointr_establish(int, struct intrhand *, const char *);
