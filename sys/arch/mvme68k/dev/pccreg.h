/* $Id: pccreg.h,v 1.1.1.1 1995/10/18 08:51:10 deraadt Exp $ */

/*
 *
 * Copyright (c) 1995 Charles D. Cranor
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
 *      This product includes software developed by Charles D. Cranor.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * peripheral channel controller (at pa fffe1000)
 */

struct pcc {
  volatile u_long dma_taddr;		/* dma table address */
  volatile u_long dma_daddr;		/* dma data address */
  volatile u_long dma_bcnt;		/* dma byte count */
  volatile u_long dma_hold;		/* dma data hold register */
  volatile u_short t1_pload;		/* timer1 preload */
  volatile u_short t1_count;		/* timer1 count */
  volatile u_short t2_pload;		/* timer2 preload */
  volatile u_short t2_count;		/* timer2 count */
  volatile u_char t1_int;		/* timer1 interrupt ctrl */
  volatile u_char t1_cr;		/* timer1 ctrl reg */
  volatile u_char t2_int;		/* timer2 interrupt ctrl */
  volatile u_char t2_cr;		/* timer2 ctrl reg */
  volatile u_char acf_int;		/* acfail intr reg */
  volatile u_char dog_int;		/* watchdog intr reg */
  volatile u_char pr_int;		/* printer intr reg */
  volatile u_char pr_cr;		/* printer ctrl */
  volatile u_char dma_int;		/* dma interrupt control */
  volatile u_char dma_csr;		/* dma csr */
  volatile u_char bus_int;		/* bus error interrupt */
  volatile u_char dma_sr;		/* dma status register */
  volatile u_char abrt_int;		/* abort interrupt control reg */
  volatile u_char ta_fcr;		/* table address function code reg */
  volatile u_char zs_int;		/* serial interrupt reg */
  volatile u_char gen_cr;		/* general control register */
  volatile u_char le_int;		/* ethernet interrupt */
  volatile u_char gen_sr;		/* general status */
  volatile u_char scsi_int;		/* scsi interrupt reg */
  volatile u_char slave_ba;		/* slave base addr reg */
  volatile u_char sw1_int;		/* software interrupt #1 cr */
  volatile u_char int_vectr;		/* interrupt base vector register */
  volatile u_char sw2_int;		/* software interrupt #2 cr */
  volatile u_char pcc_rev;		/* revision level */
};


/*
 * points to system's PCC
 */

extern struct pcc *sys_pcc;

/*
 * we lock off our interrupt vector at 0x40.  if this is changed 
 * we'll need to change vector.s
 */

#define PCC_VECBASE 0x40
#define PCC_NVEC 12

/*
 * vectors we use
 */

#define PCCV_ACFAIL	0
#define PCCV_BERR	1
#define PCCV_ABORT	2
#define PCCV_ZS		3
#define PCCV_LE		4
#define PCCV_SCSIP	5
#define PCCV_SCSID	6
#define PCCV_PRINTER	7
#define PCCV_TIMER1	8
#define PCCV_TIMER2	9
#define PCCV_SOFT1	10
#define PCCV_SOFT2	11

/*
 * enable interrupt
 */

#define PCC_IENABLE 0x08

/*
 * interrupt mask
 */

#define PCC_IMASK 0x7

/*
 * clock/timer
 */

#define PCC_TIMERACK 0x80	/* ack intr */
#define PCC_TIMER100HZ 63936	/* load value for 100Hz */
#define PCC_TIMERCLEAR 0x0	/* reset and clear timer */
#define PCC_TIMERSTART 0x3      /* start timer */

/*
 * serial control
 */

#define PCC_ZSEXTERN 0x10	/* let PCC supply vector */
